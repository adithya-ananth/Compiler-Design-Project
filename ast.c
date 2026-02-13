#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

ASTNode* create_node(NodeType type) {
    ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
    node->type = type;
    node->left = NULL;
    node->right = NULL;
    node->cond = NULL;
    node->body = NULL;
    node->init = NULL;
    node->incr = NULL;
    node->next = NULL;
    node->str_val = NULL;
    return node;
}

ASTNode* create_int_node(int val) {
    ASTNode *node = create_node(NODE_CONST_INT);
    node->int_val = val;
    return node;
}

ASTNode* create_char_node(int val) {
    ASTNode *node = create_node(NODE_CONST_CHAR);
    node->int_val = val;
    return node;
}

ASTNode* create_str_node(char *val) {
    ASTNode *node = create_node(NODE_STR_LIT);
    node->str_val = strdup(val); // Make a copy
    return node;
}

ASTNode* create_var_node(char *name) {
    ASTNode *node = create_node(NODE_VAR);
    node->str_val = strdup(name);
    return node;
}

ASTNode* create_type_node(int type_token) {
    ASTNode *node = create_node(NODE_TYPE);
    node->int_val = type_token;
    return node;
}

ASTNode* create_binary_node(int op, ASTNode *left, ASTNode *right) {
    ASTNode *node = create_node(NODE_BIN_OP);
    node->int_val = op; // Store the operator token
    node->left = left;
    node->right = right;
    return node;
}

ASTNode* create_unary_node(int op, ASTNode *child) {
    ASTNode *node = create_node(NODE_UN_OP);
    node->int_val = op;
    node->left = child;
    return node;
}

ASTNode* create_if_node(ASTNode *cond, ASTNode *then_stmt, ASTNode *else_stmt) {
    ASTNode *node = create_node(NODE_IF);
    node->cond = cond;
    node->left = then_stmt; // Using left for 'then'
    node->right = else_stmt; // Using right for 'else'
    return node;
}

ASTNode* create_while_node(ASTNode *cond, ASTNode *body) {
    ASTNode *node = create_node(NODE_WHILE);
    node->cond = cond;
    node->body = body;
    return node;
}

ASTNode* create_for_node(ASTNode *init, ASTNode *cond, ASTNode *incr, ASTNode *body) {
    ASTNode *node = create_node(NODE_FOR);
    node->init = init;
    node->cond = cond;
    node->incr = incr;
    node->body = body;
    return node;
}

ASTNode* create_func_def(ASTNode *ret_type, char *name, ASTNode *params, ASTNode *body) {
    ASTNode *node = create_node(NODE_FUNC_DEF);
    node->left = ret_type;
    node->str_val = strdup(name);
    node->right = params; // Parameters list
    node->body = body;
    return node;
}

void append_node(ASTNode *head, ASTNode *new_node) {
    if (!head) return;
    ASTNode *temp = head;
    while (temp->next) {
        temp = temp->next;
    }
    temp->next = new_node;
}

// Helper to print indentation
void print_indent(int level) {
    for (int i = 0; i < level; i++) printf("  ");
}

void print_ast(ASTNode *node, int level) {
    if (!node) return;

    print_indent(level);

    switch (node->type) {
        case NODE_FUNC_DEF:
            printf("FunctionDef: %s\n", node->str_val);
            print_indent(level + 1); printf("Return Type:\n");
            print_ast(node->left, level + 2);
            print_indent(level + 1); printf("Params:\n");
            print_ast(node->right, level + 2);
            print_indent(level + 1); printf("Body:\n");
            print_ast(node->body, level + 2);
            break;
        case NODE_VAR_DECL:
            printf("VarDecl: %s\n", node->str_val);
            print_ast(node->left, level + 1); // Type
            if (node->right) {
                print_indent(level + 1); printf("Initializer:\n");
                print_ast(node->right, level + 2);
            }
            break;
        case NODE_BLOCK:
            printf("Block\n");
            print_ast(node->left, level + 1); // Statements
            break;
        case NODE_IF:
            printf("If\n");
            print_indent(level + 1); printf("Cond:\n");
            print_ast(node->cond, level + 2);
            print_indent(level + 1); printf("Then:\n");
            print_ast(node->left, level + 2);
            if (node->right) {
                print_indent(level + 1); printf("Else:\n");
                print_ast(node->right, level + 2);
            }
            break;
        case NODE_WHILE:
            printf("While\n");
            print_indent(level + 1); printf("Cond:\n");
            print_ast(node->cond, level + 2);
            print_indent(level + 1); printf("Body:\n");
            print_ast(node->body, level + 2);
            break;
        case NODE_FOR:
            printf("For\n");
            print_indent(level + 1); printf("Init:\n");
            print_ast(node->init, level + 2);
            print_indent(level + 1); printf("Cond:\n");
            print_ast(node->cond, level + 2);
            print_indent(level + 1); printf("Incr:\n");
            print_ast(node->incr, level + 2);
            print_indent(level + 1); printf("Body:\n");
            print_ast(node->body, level + 2);
            break;
        case NODE_RETURN:
            printf("Return\n");
            print_ast(node->left, level + 1);
            break;
        case NODE_ASSIGN:
            printf("Assign\n");
            print_ast(node->left, level + 1); // Target
            print_ast(node->right, level + 1); // Value
            break;
        case NODE_BIN_OP:
            printf("BinOp: %c\n", node->int_val);
            print_ast(node->left, level + 1);
            print_ast(node->right, level + 1);
            break;
        case NODE_UN_OP:
            printf("UnOp: %c\n", node->int_val);
            print_ast(node->left, level + 1);
            break;
        case NODE_CONST_INT:
            printf("Int: %d\n", node->int_val);
            break;
        case NODE_CONST_CHAR:
            printf("Char: '%c'\n", node->int_val);
            break;
        case NODE_VAR:
            printf("Var: %s\n", node->str_val);
            break;
        case NODE_TYPE:
            printf("Type (token %d)\n", node->int_val);
            break;
        default:
            printf("Unknown Node\n");
    }

    // Print next sibling in the list
    if (node->next) {
        print_ast(node->next, level);
    }
}