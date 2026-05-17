#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>  
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include "BDD.h"

#pragma region Function_declarations
#define HASH_TABLE_SIZE 10007  // Prime number for better hash distribution
#define SYMBOL_PARRAY_LENGTH 256 // Size of symbol_parray

short symbol_parray[SYMBOL_PARRAY_LENGTH]; // Presence table for symbols


char* glean_vars(char* expression);
char* disintegrate_term(char* expression, short term_length, int position);
char* clean_spaces(char* expression);
char** dissect_expression(char* expression, size_t* terms_number);
Node* append_node(BDD* bdd, char variable, Node* low, Node* high);
uint64_t hash_key(char variable, Node* low, Node* high);
int evaluate_term(const char* term, const char* vars, const char* assignment, int var_count);
Node* build_bdd_recursive(BDD* bdd, char** terms, int term_count, const char* vars, int var_count, int var_idx);
BDD* create_BDD(char* Bfunction, char* vars);
BDD* BDD_create_with_best_order(char* Bfunction);
int BDD_use(BDD* bdd, const char* assignment);
char* remap_assignments(char* original_assignments, char* original_order, char* new_order);
void free_nodes(HT_Node** table, Node* terminal_true,  Node* terminal_false, int size);
void free_hash_table(HT_Node** table, int size);
void free_bdd(BDD* bdd);

#pragma endregion Function_declarations

#pragma region Utilitary_declarations

void free_nodes(HT_Node** table, Node* terminal_true,  Node* terminal_false, int size){
    HT_Node* entry;
    HT_Node* next;
    Node* node;
    for(size_t i = 0; i < size; i++){
        entry = table[i];
        while(entry){
            node = entry->node;
            next = entry->next_node;

            if(node && node != terminal_true && node != terminal_false){free(node);}
            entry = next;
        }
    }
}

void free_hash_table(HT_Node** table, int size){
    if(!table){return ; }// Add check for NULL table

    for(int i = 0; i < size; i++){
        HT_Node* entry = table[i];
        while(entry){
            HT_Node* next = entry->next_node;
            free(entry);
            entry = next;
        }
    }
    free(table);
}

void free_bdd(BDD* bdd){
    if(!bdd){return ;}

    if(bdd->unique_table){
        free_nodes(bdd->unique_table, bdd->terminal_true, bdd->terminal_false, HASH_TABLE_SIZE);
        free_hash_table(bdd->unique_table, HASH_TABLE_SIZE);
    }


    free(bdd->terminal_false);
    free(bdd->terminal_true);
    free(bdd->variable_order); // Free the variable order string
    free(bdd);
}

char* glean_vars(char* expression){
    if(!expression){return NULL;}

    memset(symbol_parray, 0, sizeof(short)* SYMBOL_PARRAY_LENGTH);

    size_t i, y = 0;

    // Max 52 unique alphabetic characters
    char* vars = malloc(sizeof(char)*(52 + 1));
    if(!vars){return NULL;}

    for(i = 0; expression[i] != '\0'; i++){
        char ch = expression[i];
        if(((ch >= 65 && ch <= 90)||(ch >= 97 && ch <= 122)) && symbol_parray[(int)ch] == 0){
            symbol_parray[(int)ch] = 1;
        }
    }

    for(i = 0; i < SYMBOL_PARRAY_LENGTH; i++){
        if(symbol_parray[i] == 1){
            vars[y++] =(char) i;
        }
    }

    vars[y] = '\0';
    return vars;
}

char* disintegrate_term(char* expression, short term_length, int position){
    char* term = malloc(sizeof(char)*(term_length + 1));
    for(int ci = 0; ci < term_length; ci++){
        term[ci] = expression[position + ci]; // copy term symbol by symbol
    }   
    term[term_length] = '\0';
    return term;
}

char* clean_spaces(char* expression){
    size_t expr_len = strlen(expression);
    char* output = malloc(sizeof(char)*(expr_len + 1));
    if(!output){return NULL;}
    
    size_t i;
    size_t j = 0;
    for(i = 0; i < expr_len; i++){ // Iterate thorugh expression copying only non-whitespace symbols
        if(!isspace(expression[i])){
            output[j++] = expression[i];
        }
    }
    output[j] = '\0';
    return output;
}


char** dissect_expression(char* expression, size_t* terms_number){
    char* clean_expression = clean_spaces(expression);
    size_t expression_length = strlen(clean_expression);
    size_t terms_quantity = 0; 

    for(int i = 0; i < expression_length; i++){
        if(clean_expression[i] == '+'){terms_quantity++;}// Count quantity of terms
    }
    terms_quantity += 1; // Include last term without '+' symbol after it


    char** terms_list =(char**)malloc(sizeof(char*)* terms_quantity);
    short term_length = 0;
    int position = 0;
    int term_number = 0;
    
    for(int k = 0; k < expression_length; k++){
        if(clean_expression[k] == '+'){
            char* freed_term = disintegrate_term(clean_expression, term_length, position); // Pass each disintegrated term into the terms_list table // disintegrate_term returns pointer to memory allocated in the heap
            terms_list[term_number] = malloc(sizeof(char)*(term_length + 1));
            strcpy(terms_list[term_number], freed_term);
            term_number++;
            position = k + 1;
            term_length = 0;
            free(freed_term);
        }
        else{
            term_length++; // If it's a term's symbol increase length of the term
        }
    }
    char* last_term = disintegrate_term(clean_expression, term_length, position);
    terms_list[term_number] = malloc(sizeof(char)*(term_length + 1));
    strcpy(terms_list[term_number], last_term);
    free(last_term);

    if(clean_expression){free(clean_expression);}
    (*terms_number)= terms_quantity; // Update terms_number

    return terms_list;
}


uint64_t hash_key(char variable, Node* low, Node* high){
    uint64_t low_ptr =(uint64_t)(uintptr_t)low;
    uint64_t high_ptr =(uint64_t)(uintptr_t)high;
    
    return((variable * 7919)^((low_ptr << 5)|(low_ptr >> 27))^((high_ptr << 7)|(high_ptr >> 25)))% HASH_TABLE_SIZE;
}

Node* append_node(BDD* bdd, char variable, Node* low, Node* high){
    // Apply reduction rule: if both branches are the same, return the branch
    if(low == high){
        return low;
    }
    
    uint64_t hash = hash_key(variable, low, high);
    int index = hash % HASH_TABLE_SIZE;
    
    // Look for an existing equivalent node
    HT_Node* entry = bdd->unique_table[index];
    while(entry){
        Node* node = entry->node;
        if(node->variable == variable && node->low == low && node->high == high){
            // Found equivalent node
            node->ref_count++;
            return node;
        }
        entry = entry->next_node;
    }
    
    // Create a new node
    Node* new_node =(Node*)malloc(sizeof(Node));
    
    new_node->variable = variable;
    new_node->low = low;
    new_node->high = high;
    new_node->ref_count = 1;

    if(low){low->ref_count++;}
    if(high){high->ref_count++;}


    // Add to hash table
    HT_Node* new_entry =(HT_Node*)malloc(sizeof(HT_Node));
    
    new_entry->node = new_node;
    new_entry->next_node = bdd->unique_table[index];
    bdd->unique_table[index] = new_entry;
    
    // Update node count
    bdd->node_count++;
    
    return new_node;
}

char* remap_assignments(char* original_assignments, char* original_order, char* new_order){
    if(!original_assignments || !original_order || !new_order){
        printf("Error: Invalid arguments to remap_assignments\n");
        return NULL;
    }
    
    size_t orig_len = strlen(original_order);
    size_t new_len = strlen(new_order);
    size_t assign_len = strlen(original_assignments);
    
    if(orig_len != assign_len){
        printf("Error: Length mismatch between original_order(%zu)and assignments(%zu)\n", orig_len, assign_len);
        return NULL;
    }
    
    // Create a lookup map
    char assignment_map[256];
    memset(assignment_map, '?', sizeof(assignment_map)); // Initialize with invalid value
    
    for(size_t i = 0; i < orig_len; i++){
        char var = original_order[i];
        char value = original_assignments[i];
        
        if(value != '0' && value != '1'){
            printf("Warning: Invalid assignment '%c' for variable %c\n", value, var);
            value = '0';
        }
        
        assignment_map[(int)var] = value;
    }
    
    char* new_assignments =(char*)malloc((new_len + 1)* sizeof(char));
    
    for(size_t i = 0; i < new_len; i++){
        char var = new_order[i];
        char value = assignment_map[(int)var];
        
        if(value == '?'){
            printf("Warning: No assignment found for variable %c in new order\n", var);
            value = '0';
        }
        
        new_assignments[i] = value;
    }
    
    new_assignments[new_len] = '\0';
    
    return new_assignments;
}

int evaluate_term(const char* term, const char* vars, const char* assignment, int var_count){
    if(!term || !vars || !assignment){
        printf("Error: Invalid arguments to evaluate_term\n");
        return 0;
    }
    
    // Create a lookup map for quick access to variable values
    char value_map[256];
    memset(value_map, '?', sizeof(value_map));
    
    for(int i = 0; i < var_count; i++){
        value_map[(unsigned char)vars[i]] = assignment[i];
    }
    
    // Process each character in the term
    int i = 0;
    int term_len = strlen(term);
    
    while(i < term_len){
        int negated = 0;
        char var;
        
        // Check for negation
        if(term[i] == '!'){
            negated = 1;
            i++;
            if(i >= term_len){
                printf("Error: Unexpected end of term after negation\n");
                return 0;
            }
        }
        
        var = term[i++];
        
        // Skip if not a variable
        if(!((var >= 65 && var <= 90)||(var >= 97 && var <= 122))){
            continue;
        }
        
        // Get the variable's value
        char var_value = value_map[(unsigned char)var];
        
        if(var_value == '?'){
            printf("Warning: No assignment found for variable %c\n", var);
            var_value = '0';
        }
        
        // Evaluate this literal
        int literal_value;
        if(var_value == '1'){
            literal_value = negated ? 0 : 1;
        }else{
            literal_value = negated ? 1 : 0;
        }
        
        if(literal_value == 0){
            return 0;
        }
    }
    
    // At this point, all literals are evaluated to 1
    return 1;
}

Node* build_bdd_recursive(BDD* bdd, char** terms, int term_count, const char* vars, int var_count, int var_idx){
    // Base case: if we've processed all variables
    if(var_idx >= var_count){
        return term_count > 0 ? bdd->terminal_true : bdd->terminal_false;
    }
    
    char var = vars[var_idx];
    
    char* low_assignment =(char*)malloc((var_count + 1)* sizeof(char));
    char* high_assignment =(char*)malloc((var_count + 1)* sizeof(char));
    
    // Initialize assignments with dont-cares
    for(int i = 0; i < var_count; i++){
        low_assignment[i] = 'X';  
        high_assignment[i] = 'X';
    }
    low_assignment[var_count] = '\0';
    high_assignment[var_count] = '\0';
    
    // Set current variable
    low_assignment[var_idx] = '0';
    high_assignment[var_idx] = '1';
    
    // Find terms that are satisfied by low and high branches
    char** low_terms =(char**)malloc(term_count * sizeof(char*));
    char** high_terms =(char**)malloc(term_count * sizeof(char*));
    int low_term_count = 0;
    int high_term_count = 0;
    
    for(int i = 0; i < term_count; i++){
        // Check if this term contains the current variable
        int contains_var = 0;
        int is_negated = 0;
        for(int j = 0; j < strlen(terms[i]); j++){
            if(terms[i][j] == var){
                contains_var = 1;
                // Check if it's negated
                if(j > 0 && terms[i][j-1] == '!'){
                    is_negated = 1;
                }
                break;
            }
        }
        
        if(!contains_var){
            low_terms[low_term_count++] = terms[i];
            high_terms[high_term_count++] = terms[i];
        }
        else if(!is_negated){
            high_terms[high_term_count++] = terms[i];
        }
        else{
            low_terms[low_term_count++] = terms[i];
        }
    }
    
    // Recursively build the low and high branches
    Node* low_node = build_bdd_recursive(bdd, low_terms, low_term_count, vars, var_count, var_idx + 1);
    Node* high_node = build_bdd_recursive(bdd, high_terms, high_term_count, vars, var_count, var_idx + 1);
    
    free(low_assignment);
    free(high_assignment);
    free(low_terms);
    free(high_terms);
    
    // Create and return the node, applying reduction rules
    return append_node(bdd, var, low_node, high_node);
}
#pragma endregion Utilitary_declarations

#pragma region BDD_use
int BDD_use(BDD* bdd, const char* assignment){
    if(!bdd || !bdd->root){
        printf("Error: Invalid BDD or null root node in BDD evaluation\n");
        return 0;
    }
    
    Node* current = bdd->root;
    
    // Create a lookup map for assignments
    char assignment_map[256];
    memset(assignment_map, 0, sizeof(assignment_map));
    
    for(size_t i = 0; i < strlen(bdd->variable_order); i++){
        if(i < strlen(assignment)){
            assignment_map[(unsigned char)bdd->variable_order[i]] = assignment[i];
        }else{
            printf("Warning: Missing assignment for variable %c\n", bdd->variable_order[i]);
            assignment_map[(unsigned char)bdd->variable_order[i]] = '0';
        }
    }
    
    while(current != bdd->terminal_true && current != bdd->terminal_false){
        char var_assignment = assignment_map[(unsigned char)current->variable];
        
        if(var_assignment != '0' && var_assignment != '1'){
            printf("Error: Invalid assignment '%c' for variable %c\n", var_assignment, current->variable);
            return 0;
        }
        
        if(var_assignment == '1'){
            current = current->high;
        }else{
            current = current->low;
        }
        
        if(!current){
            printf("Error: Null branch encountered\n");
            return 0;
        }
    }
    
    return(current == bdd->terminal_true)? 1 : 0;
}

#pragma endregion BDD_use

#pragma region create_BDD
BDD* create_BDD(char* Bfunction, char* vars){
    size_t terms_quantity = 0;
    char** terms_list = dissect_expression(Bfunction, &terms_quantity);
    size_t var_count = strlen(vars);

    BDD* bdd =(BDD*)malloc(sizeof(BDD));

    bdd->unique_table =(HT_Node**)calloc(HASH_TABLE_SIZE, sizeof(HT_Node*));
    
    bdd->terminal_false =(Node*)malloc(sizeof(Node));
    bdd->terminal_true =(Node*)malloc(sizeof(Node));
    
    bdd->terminal_false->variable = '0';
    bdd->terminal_false->low = NULL;
    bdd->terminal_false->high = NULL;
    bdd->terminal_false->ref_count = INT_MAX;
    
    bdd->terminal_true->variable = '1';
    bdd->terminal_true->low = NULL;
    bdd->terminal_true->high = NULL;
    bdd->terminal_true->ref_count = INT_MAX;
    
    bdd->variable_count = var_count;
    bdd->node_count = 2;  // Two terminal nodes
    bdd->root = NULL;
    bdd->variable_order = strdup(vars);



    bdd->root = build_bdd_recursive(bdd, terms_list, terms_quantity, vars, var_count, 0);

    
    for(size_t y = 0; y < terms_quantity; y++){
        free(terms_list[y]);
    }
    free(terms_list);

    return bdd;
}
#pragma endregion create_BDD

#pragma region create_BDD_WBO
BDD* BDD_create_with_best_order(char* Bfunction){
    char* vars_array = glean_vars(Bfunction);
    if(!vars_array){
        printf("Error: Failed to extract variables from function\n");
        return NULL;
    }

    size_t var_num = strlen(vars_array);

    BDD* best_bdd = create_BDD(Bfunction, vars_array);
    if(!best_bdd){
        printf("Error: Failed to create initial BDD\n");
        free(vars_array);
        return NULL;
    }

    char* best_order = strdup(vars_array);
    if(!best_order){
        printf("Error: Failed to duplicate variable order\n");
        free(vars_array);
        free_bdd(best_bdd);
        return NULL;
    }

    int best_node_count = best_bdd->node_count;

    // Try all i-j swaps
    for(size_t i = 0; i < var_num; i++){
        for(size_t j = i + 1; j < var_num; j++){
            char* test_order = strdup(best_order);
            if(!test_order){
                printf("Warning: Failed to duplicate test order at(%zu, %zu)\n", i, j);
                continue;
            }

            // Swap
            char temp = test_order[i];
            test_order[i] = test_order[j];
            test_order[j] = temp;


            BDD* test_bdd = create_BDD(Bfunction, test_order);
            if(!test_bdd){
                printf("Warning: create_BDD failed for order: %s\n", test_order);
                free(test_order);
                continue;
            }


            if(test_bdd->node_count < best_node_count){
                free_bdd(best_bdd);
                free(best_order);

                best_bdd = test_bdd;
                best_order = test_order;
                best_node_count = test_bdd->node_count;
            }else{
                free_bdd(test_bdd);
                free(test_order);
            }
        }
    }

    if(best_bdd->variable_order){
        free(best_bdd->variable_order);
    }

    best_bdd->variable_order = strdup(best_order);
    if(!best_bdd->variable_order){
        printf("Error: Failed to set variable order in best BDD\n");
        free(best_order);
        free(vars_array);
        free_bdd(best_bdd);
        return NULL;
    }

    free(best_order);
    free(vars_array);

    return best_bdd;
}


#pragma endregion create_BDD_WBO

#pragma region Main
int main(){
    char* boolean_function = "!A!B!C!DE!F + !A!B!C!D + !G!H!I!J + !A!F!K!L + ABCDE!G + !A!C!EG!I!K + !BDFHJ + B!D!F!HJ!L + ACFGKJKL + !A!C!D!EF!G!H!I!J!K!L";
    
    char* initial_order = "ABCDEFGHIJKL";
    
    char* custom_order = "ICGJEFHBADKL";
    
    printf("Creating BDDs for function: %s\n", boolean_function);
    
    BDD* regular_bdd = create_BDD(boolean_function, custom_order);
    
    BDD* best_bdd = BDD_create_with_best_order(boolean_function);
    
    printf("\nBDD Statistics:\n");
    printf("Number of variables: %d\n", regular_bdd->variable_count);
    printf("Regular BDD order: %s\n", regular_bdd->variable_order);
    printf("Best BDD order: %s\n", best_bdd->variable_order);
    printf("Number of nodes(regular_bdd): %d\n", regular_bdd->node_count);
    printf("Number of nodes(best_bdd): %d\n", best_bdd->node_count);
    
    double max_theoretical_nodes = pow(2, regular_bdd->variable_count);
    double reduction_regular =((max_theoretical_nodes - regular_bdd->node_count)/ max_theoretical_nodes)* 100.0;
    double reduction_best =((max_theoretical_nodes - best_bdd->node_count)/ max_theoretical_nodes)* 100.0;
    
    printf("Maximum theoretical nodes: %.0f\n", max_theoretical_nodes);
    printf("Reduction efficiency(regular): %.2f%%\n", reduction_regular);
    printf("Reduction efficiency(best): %.2f%%\n", reduction_best);
    
    char* test_cases[] ={
        "110111011000",
        "000011001101",
        "001101110100", 
        "010101011110",
        "111111011011",
        "010101010101",
        "101010101010",
        "111111111111",
        "111110000000",
        "000000000000"
    };
    int test_count = sizeof(test_cases)/ sizeof(test_cases[0]);
    
    printf("\nEvaluating test cases directly on terms:\n");
    int* reference_results =(int*)malloc(test_count * sizeof(int));
    
    size_t term_count;
    char** terms = dissect_expression(boolean_function, &term_count);
    
    for(int i = 0; i < test_count; i++){
        int result = 0;
        for(size_t t = 0; t < term_count; t++){
            if(evaluate_term(terms[t], initial_order, test_cases[i], strlen(initial_order))){
                result = 1;
                break;
            }
        }
        reference_results[i] = result;
        printf("Direct evaluation of test case %d(%s): %d\n", i, test_cases[i], result);
    }
    
    for(int i = 0; i < test_count; i++){
        char* regular_assignment = remap_assignments(test_cases[i], initial_order, regular_bdd->variable_order);
        char* best_assignment = remap_assignments(test_cases[i], initial_order, best_bdd->variable_order);
        
        // Evaluate the BDDs
        int regular_result = BDD_use(regular_bdd, regular_assignment);
        int best_result = BDD_use(best_bdd, best_assignment);
        
        if(regular_result != reference_results[i] || best_result != reference_results[i]){
            printf(" [ERROR: Results don't match]");
        }else if(regular_result != best_result){
            printf(" [ERROR: BDD results don't match]");
        }
        printf("Test case %d for %s:(r = %d)&&(b = %d)\n", i + 1, test_cases[i], regular_result, best_result);

        free(regular_assignment);
        free(best_assignment);
    }
    
    for(size_t t = 0; t < term_count; t++){
        free(terms[t]);
    }
    free(terms);
    free(reference_results);
    
    free_bdd(regular_bdd);
    free_bdd(best_bdd);
    
    return 0;
}
#pragma endregion Main
