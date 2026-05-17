#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "BDD.h"

#define taken(x)(x > 16384 ? 1 : 0)
#define negated(x)(x > 16384 ? 1 : 0)

#define MAX_NUMBER_SYMBOLS 256
#define MIN_TEST_QUANTITY 100

#define MAX_VARIABLE_NUMBER 15
#define MAX_TERM_NUMBER 20

#define MIN_VARIABLE_NUMBER 13
#define MIN_TERM_NUMBER 10

char* generate_variable_set(size_t* out_count){
    int variables[MAX_NUMBER_SYMBOLS] = {0};
    size_t actual_variable_number = MIN_VARIABLE_NUMBER + rand()%(MAX_VARIABLE_NUMBER - MIN_VARIABLE_NUMBER + 1);
    char* chosen_vars = malloc(actual_variable_number + 1);
    if(!chosen_vars){return NULL;}

    size_t counter = 0;
    for(size_t k = 65; k <= 90 && counter < actual_variable_number; ++k){
        if(!variables[k] && taken(rand())){
            variables[k] = 1;
            chosen_vars[counter++] =(char)k;
        }
        if(k == 90 && counter < actual_variable_number)k = 64;
    }
    chosen_vars[counter] = '\0';
    *out_count = counter;
    return chosen_vars;
}

char* generate_expression_with_vars(const char* chosen_vars, size_t var_count){
    size_t actual_term_number = MIN_TERM_NUMBER + rand()%(MAX_TERM_NUMBER - MIN_TERM_NUMBER + 1);
    char* expression = malloc(sizeof(char)*(2 * actual_term_number * var_count + actual_term_number + 1));
    if(!expression)return NULL;

    int term_vars[MAX_NUMBER_SYMBOLS];
    size_t char_position = 0;

    for(size_t w = 0; w < actual_term_number; w++){
        memset(term_vars, 0, sizeof(term_vars));
        size_t threshold = 1 + rand()% var_count;
        size_t threshold_counter = 0;

        for(size_t i = 0; i < var_count && threshold_counter < threshold; ++i){
            if(!term_vars[(int)chosen_vars[i]] && taken(rand())){
                term_vars[(int)chosen_vars[i]] = 1;
                threshold_counter++;
            }
        }

        threshold_counter = 0;
        for(size_t i = 0; i < var_count && threshold_counter < threshold; ++i){
            if(term_vars[(int)chosen_vars[i]]){
                if(negated(rand())){
                    expression[char_position++] = '!';
                }
                expression[char_position++] = chosen_vars[i];
                threshold_counter++;
            }
        }

        if(w != actual_term_number - 1){
            expression[char_position++] = '+';
        }
    }

    expression[char_position] = '\0';
    return expression;
}

int evaluate_expression(const char** terms, const char* vars, const char* assignment, size_t var_count, size_t terms_num){
    for(int i = 0; i < terms_num; i++){
        const char* term = terms[i];
        int satisfied = 1;

        for(int j = 0; term[j] != '\0';){
            int is_negated = 0;
            if(term[j] == '!'){
                is_negated = 1;
                j++;
            }

            char var = term[j++];
            int idx = -1;
            for(int k = 0; k < var_count; k++){
                if(vars[k] == var){
                    idx = k;
                    break;
                }
            }
            
            if(idx == -1){
                // Handle the case where the variable is not found
                fprintf(stderr, "Error: Variable '%c' not found in vars '%s'\n", var, vars);
                return -1; // Or some other error indication
            }

            if((is_negated && assignment[idx] != '0')||(!is_negated && assignment[idx] != '1')){
                satisfied = 0;
                break;
            }
        }

        if(satisfied){return 1;}
    }

    return 0;
}

int main(){
    srand(time(NULL));
    FILE* log = fopen("bdd_test_results.txt", "w");
    if(!log){return 1;}

    int num_tests = 100;

    int passed = 0;
    int failed = 0;

    for(int test_idx = 0; test_idx < num_tests; ++test_idx){
        size_t var_count = 0;
        char* vars = generate_variable_set(&var_count);
        if(!vars){
            fprintf(log, "Failed to generate variable set\n");
            continue;
        }


        char* expr = generate_expression_with_vars(vars, var_count);
        if(!expr){
            fprintf(log, "Failed to generate expression\n");
            free(vars);
            continue;
        }

        BDD* original = NULL;
        BDD* optimized = NULL;
        char** terms = NULL;
        int memory_error = 0;

        size_t term_count;
        do {
            original = create_BDD(expr, vars);
            optimized = BDD_create_with_best_order(expr);

            if(!original || !optimized){
                fprintf(log, "Failed to create BDD\n");
                memory_error = 1;
                break;
            }

            terms = dissect_expression(expr, &term_count);
            if(!terms){
                fprintf(log, "Failed to dissect expression\n");
                memory_error = 1;
                break;
            }

            size_t total_combinations =(1ULL << var_count);
            int success = 1;

            for(size_t k = 0; k < total_combinations; ++k){
                char* assignment = malloc(var_count + 1);
                if(!assignment){
                    fprintf(log, "Memory allocation failed for assignment\n");
                    success = 0;
                    break;
                }

                for(size_t j = 0; j < var_count; ++j){
                    assignment[j] =(k &(1ULL <<(var_count - j - 1)))? '1' : '0';
                }
                assignment[var_count] = '\0';

                int expected = evaluate_expression((const char**)terms, vars, assignment, var_count, term_count);
                char* remapped_assignments = remap_assignments(assignment, vars, optimized->variable_order);
                
                if(expected == -1){printf("Everything has broken\n"); success = 0; break;}

                if(!remapped_assignments){
                    fprintf(log, "Failed to remap assignments\n");
                    free(assignment);
                    success = 0;
                    break;
                }

                int result_original = BDD_use(original, assignment);
                int result_optimized = BDD_use(optimized, remapped_assignments);

                if(result_original != expected || result_optimized != expected){
                    success = 0;
                    free(assignment);
                    free(remapped_assignments);
                    break;
                }

                free(assignment);
                free(remapped_assignments);
            }

            if(success){passed++;}
            else{failed++;}
            printf("Attempt - %d\n", test_idx + 1);
            fprintf(log, "--- Test - Attempt %d ---\n", test_idx + 1);
            fprintf(log, "Best order(number of nodes): %d\n", optimized->node_count);
            fprintf(log, "Regular order(number of nodes): %d\n", original->node_count);
        } while(0);
        // Comprehensive cleanup
        if(vars){free(vars);}
        if(expr){free(expr);}
        if(original){free_bdd(original);}
        if(optimized){free_bdd(optimized);}
        
        // Free terms array
        if(terms){
            for(size_t i = 0; i < term_count; i++){
                free(terms[i]);
            }
            free(terms);
        }
        
        if(memory_error){
            break;
        }
    }
    
    fprintf(log, "----------------------------------\n");
    fprintf(log, "Number of tests passed: %d\n", passed);
    fprintf(log, "Number of tests failed: %d\n", failed);
    fprintf(log, "----------------------------------\n");

    if(log){fclose(log);}
    return 0;
}
