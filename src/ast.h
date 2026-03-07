#include "symbol_table.h"

#ifndef AST_H
#define AST_H

typedef enum {
    NODE_FUNC_DEF,
    NODE_VAR_DECL,
    NODE_PARAM,
    NODE_BLOCK,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_RETURN,
    NODE_ASSIGN,
    NODE_BIN_OP,
    NODE_UN_OP,
    NODE_CONST_INT,
    NODE_CONST_CHAR,
    NODE_STR_LIT,
    NODE_VAR,
    NODE_FUNC_CALL,
    NODE_TYPE,
    NODE_EMPTY,
    NODE_SWITCH,
    NODE_CASE,
    NODE_BREAK,
    /* Arrays */
    NODE_ARRAY_DECL,   /* array declaration */
    NODE_INDEX         /* a[i] indexing expression */
} NodeType;

typedef struct ASTNode {
    NodeType type;
    DataType data_type;   // semantic type (int, char, void)
    int line_number;      // source line number
    // For operators (+, -, *, etc.) and types (int, void)
    int int_val;

    // For identifiers and string literals
    char *str_val;

    // Children pointers
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *cond;  // Specific for control flow
    struct ASTNode *body;  // Specific for control flow
    struct ASTNode *init;  // For loops
    struct ASTNode *incr;  // For loops
    struct ASTNode *params; // for function parameters

    // For linked lists (e.g., list of statements, list of args)
    struct ASTNode *next;

    // New: pointer and array info
    int pointer_level;
    int array_dim_count;
    struct ASTNode **array_dim_exprs;
} ASTNode;


void export_ast_to_dot(ASTNode *root, const char *filename);



// Node Creation Functions
ASTNode* create_node(NodeType type);
ASTNode* create_int_node(int val);
ASTNode* create_char_node(int val);
ASTNode* create_str_node(char *val);
ASTNode* create_var_node(char *name);
ASTNode* create_type_node(int type_token); // For int/void/char
ASTNode* create_binary_node(int op, ASTNode *left, ASTNode *right);
ASTNode* create_unary_node(int op, ASTNode *child);
ASTNode* create_if_node(ASTNode *cond, ASTNode *then_stmt, ASTNode *else_stmt);
ASTNode* create_while_node(ASTNode *cond, ASTNode *body);
ASTNode* create_for_node(ASTNode *init, ASTNode *cond, ASTNode *incr, ASTNode *body);
ASTNode* create_func_def(ASTNode *ret_type, char *name, ASTNode *params, ASTNode *body);
ASTNode* create_array_decl_node(char *name, ASTNode *type_node, int dim_count, ASTNode **dim_exprs);
ASTNode* create_index_node(ASTNode *base, ASTNode *index);
ASTNode* create_switch_node(ASTNode *cond, ASTNode *cases);
ASTNode* create_case_node(ASTNode *expr, ASTNode *body);
ASTNode* create_break_node(void);

// List manipulation
void append_node(ASTNode *head, ASTNode *new_node);

// Visualization
void print_ast(ASTNode *node, int level);

#endif
