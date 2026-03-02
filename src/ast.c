#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "y.tab.h"



const char* node_type_to_string(NodeType type) {
    switch (type) {
        case NODE_FUNC_DEF: return "FUNC_DEF";
        case NODE_VAR_DECL: return "VAR_DECL";
        case NODE_PARAM: return "PARAM";
        case NODE_BLOCK: return "BLOCK";
        case NODE_IF: return "IF";
        case NODE_WHILE: return "WHILE";
        case NODE_FOR: return "FOR";
        case NODE_RETURN: return "RETURN";
        case NODE_ASSIGN: return "ASSIGN";
        case NODE_BIN_OP: return "BIN_OP";
        case NODE_UN_OP: return "UN_OP";
        case NODE_CONST_INT: return "INT";
        case NODE_CONST_CHAR: return "CHAR";
        case NODE_VAR: return "VAR";
        case NODE_FUNC_CALL: return "FUNC_CALL";
        case NODE_TYPE: return "TYPE";
        case NODE_STR_LIT: return "STRING";
        default: return "UNKNOWN";
    }
}

static int node_counter = 0;


const char* get_op_string(int op) {
    switch (op) {
        case '+': return "+";
        case '-': return "-";
        case '*': return "*";
        case '/': return "/";
        case '%': return "%";
        case '<': return "<";
        case '>': return ">";
        case '=': return "=";
        case '!': return "!";

        case T_EQ:  return "==";
        case T_NEQ: return "!=";
        case T_LE:  return "<=";
        case T_GE:  return ">=";
        case T_AND: return "&&";
        case T_OR:  return "||";

        default: return "UNKNOWN_OP";
    }
}



int generate_dot(ASTNode *node, FILE *out) {
    if (!node) return -1;

    int my_id = node_counter++;

    // Create label
    fprintf(out, "node%d [label=\"%s", my_id,
            node_type_to_string(node->type));

    if (node->type == NODE_BIN_OP ||
    node->type == NODE_UN_OP) {
    fprintf(out, "\\n%s", get_op_string(node->int_val));
    }

    if (node->data_type == TYPE_INT)
    fprintf(out, "\\n[int]");
    else if (node->data_type == TYPE_CHAR)
    fprintf(out, "\\n[char]");

    if (node->str_val)
        fprintf(out, "\\n%s", node->str_val);

    if (node->type == NODE_CONST_INT)
        fprintf(out, "\\n%d", node->int_val);

    fprintf(out, "\"];\n");

    // Recursively generate children
    #define CONNECT(child) \
        if (node->child) { \
            int child_id = generate_dot(node->child, out); \
            fprintf(out, "node%d -> node%d;\n", my_id, child_id); \
        }

    CONNECT(left);
    CONNECT(right);
    CONNECT(cond);
    CONNECT(body);
    CONNECT(params);
    CONNECT(init);
    CONNECT(incr);

    // Handle next (sibling)
    if (node->next) {
        int next_id = generate_dot(node->next, out);
        fprintf(out, "node%d -> node%d [style=dashed];\n", my_id, next_id);
    }

    return my_id;
}

void export_ast_to_dot(ASTNode *root, const char *filename) {
    FILE *out = fopen(filename, "w");
    if (!out) {
        perror("Cannot open dot file");
        return;
    }

    fprintf(out, "digraph AST {\n");
    fprintf(out, "node [shape=box];\n");

    node_counter = 0;
    generate_dot(root, out);

    fprintf(out, "}\n");
    fclose(out);
}



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
    node->params = NULL;
    node->str_val = NULL;

    node->data_type = TYPE_VOID;  //default
    node->line_number = 0; //default
    return node;
}

ASTNode* create_int_node(int val) {
    ASTNode *node = create_node(NODE_CONST_INT);
    node->int_val = val;
    node->data_type = TYPE_INT;
    return node;
}

ASTNode* create_char_node(int val) {
    ASTNode *node = create_node(NODE_CONST_CHAR);
    node->int_val = val;
    node->data_type = TYPE_CHAR;
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
    switch (type_token) {
        case T_INT:
            node->data_type = TYPE_INT;
            break;
        case T_CHAR:
            node->data_type = TYPE_CHAR;
            break;
        case T_VOID:
            node->data_type = TYPE_VOID;
            break;
        default:
            node->data_type = TYPE_VOID;
    }
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

    if (cond) cond->next = NULL;
    if (then_stmt) then_stmt->next = NULL;
    if (else_stmt) else_stmt->next = NULL;

    return node;
}

ASTNode* create_while_node(ASTNode *cond, ASTNode *body) {
    ASTNode *node = create_node(NODE_WHILE);
    node->cond = cond;
    node->body = body;

    if (cond) cond->next = NULL;
    if (body) body->next = NULL;

    return node;
}

ASTNode* create_for_node(ASTNode *init, ASTNode *cond, ASTNode *incr, ASTNode *body) {
    ASTNode *node = create_node(NODE_FOR);
    node->init = init;
    node->cond = cond;
    node->incr = incr;
    node->body = body;

    if (init) init->next = NULL;
    if (cond) cond->next = NULL;
    if (incr) incr->next = NULL;
    if (body) body->next = NULL;


    return node;
}

ASTNode* create_func_def(ASTNode *ret_type, char *name, ASTNode *params, ASTNode *body) {
    ASTNode *node = create_node(NODE_FUNC_DEF);
    node->left = ret_type;
    node->str_val = strdup(name);
    node->params = params; // Parameters list
    node->body = body;
    return node;
}

void append_node(ASTNode *head, ASTNode *new_node) {
    if (!head || !new_node) return;

    new_node->next = NULL;

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
            print_ast(node->params, level + 2);
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
        case NODE_PARAM:
           printf("Param: %s\n", node->str_val);
           print_indent(level + 1);
           printf("Type:\n");
           print_ast(node->left, level + 2);
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
            printf("BinOp: %s\n", get_op_string(node->int_val));
            print_ast(node->left, level + 1);
            print_ast(node->right, level + 1);
            break;
        case NODE_UN_OP:
            printf("UnOp: %s\n", get_op_string(node->int_val));
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
