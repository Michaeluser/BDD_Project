#include <stdint.h>
#ifndef BDD_H
#define BDD_H

// BDD node structure
typedef struct Node {
    char variable;         // Variable label (e.g., 'A', 'B', etc.)
    struct Node* low;      // Low/False branch (0)
    struct Node* high;     // High/True branch (1)D
    int ref_count;         // Signifies how many nodes point to the current
} Node;

// Hash table entry for unique table
typedef struct HT_Node {
    Node* node;
    struct HT_Node* next_node;
} HT_Node;

// BDD structure
typedef struct {
    Node* root;               // Root node of the BDD
    int variable_count;       // Number of variables
    char* variable_order;
    int node_count;           // Total number of nodes
    Node* terminal_false;     // Terminal node for FALSE
    Node* terminal_true;      // Terminal node for TRUE
    HT_Node** unique_table; // Hash table for unique nodes
} BDD;



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

#endif