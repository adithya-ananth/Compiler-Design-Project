#include <stdio.h>
#include <stdlib.h>
#include "symbol_table.h"
#include "ast.h"
#include "semantic.h"

static Symbol *current_function = NULL;
int semantic_errors = 0;

void semantic_error(int line, const char *msg) {
    printf("Semantic Error (line %d): %s\n", line, msg);
    semantic_errors++;
}


void analyze_function(ASTNode *node) {

    // Create function symbol (global scope)
    Symbol *func = create_symbol(
        node->str_val,
        node->left->data_type,   // return type
        SYM_FUNCTION,
        node->line_number
    );

    if (!insert_symbol(func)) {
        semantic_error(node->line_number, "Function redeclared");
        return;
    }

    current_function = func;

    // -------------------------------------------------
    // First pass: count parameters
    // -------------------------------------------------
    int count = 0;
    ASTNode *param = node->params;

    while (param) {
        count++;
        param = param->next;
    }

    func->param_count = count;

    if (count > 0) {
        func->param_types = malloc(sizeof(DataType) * count);
    } else {
        func->param_types = NULL;
    }

    // -------------------------------------------------
    // Second pass: store parameter types
    // -------------------------------------------------
    param = node->params;
    int i = 0;

    while (param) {
        func->param_types[i++] = param->left->data_type;
        param = param->next;
    }

    // -------------------------------------------------
    // Enter function scope
    // -------------------------------------------------
    enter_scope();

    // -------------------------------------------------
    // Insert parameters into function scope
    // -------------------------------------------------
    param = node->params;

    while (param) {
        Symbol *sym = create_symbol(
            param->str_val,
            param->left->data_type,
            SYM_PARAMETER,
            param->line_number
        );

        if (!insert_symbol(sym))
            semantic_error(param->line_number, "Parameter redeclared");

        param = param->next;
    }

    // -------------------------------------------------
    // Analyze function body
    // -------------------------------------------------
    int body_returns = analyze_node(node->body);

     if (current_function->type != TYPE_VOID && !body_returns) {
        semantic_error(node->line_number,
                       "Non-void function must return a value");
    }

    exit_scope();

    current_function = NULL;
}


void analyze_declaration(ASTNode *node) {
printf("Declaring %s at scope level %d\n",
       node->str_val,
       current_scope->level);
    Symbol *sym = create_symbol(
        node->str_val,
        node->left->data_type,
        SYM_VARIABLE,
        node->line_number
    );

    if (!insert_symbol(sym))
        semantic_error(node->line_number, "Variable redeclared");

    // If initializer exists
    if (node->right) {
        analyze_node(node->right);

        if (node->left->data_type != node->right->data_type)
            semantic_error(node->line_number,
                           "Type mismatch in initialization");
    }
}

int analyze_block(ASTNode *node) {
    enter_scope();

    ASTNode *stmt = node->left;
    int returns = 0;

    while (stmt) {

        returns = analyze_node(stmt);

        if (returns)
            break;   // everything after unreachable

        stmt = stmt->next;
    }

    exit_scope();

    return returns;
}

void analyze_variable(ASTNode *node) {

    Symbol *sym = lookup(node->str_val);

    if (!sym) {
        semantic_error(node->line_number,
                       "Undeclared variable");
        node->data_type = TYPE_INT;
        return;
    }

    node->data_type = sym->type;
}

void analyze_assignment(ASTNode *node) {

    analyze_node(node->left);
    analyze_node(node->right);

    if (node->left->data_type == TYPE_VOID ||
        node->right->data_type == TYPE_VOID)
        return;  // don't cascade


    if (node->left->data_type != node->right->data_type)
        semantic_error(node->line_number,
                       "Assignment type mismatch");

    node->data_type = node->left->data_type;
}

void analyze_binary(ASTNode *node) {

    analyze_node(node->left);
    analyze_node(node->right);

    if (node->left->data_type == TYPE_VOID ||
        node->right->data_type == TYPE_VOID)
        return;  // don't cascade


    if (node->left->data_type != node->right->data_type)
        semantic_error(node->line_number,
                       "Binary operand type mismatch");

    node->data_type = node->left->data_type;
}


void analyze_unary(ASTNode *node) {
    analyze_node(node->left);
    node->data_type = node->left->data_type;
}


void analyze_function_call(ASTNode *node) {

    Symbol *sym = lookup(node->str_val);

    if (!sym || sym->kind != SYM_FUNCTION) {
        semantic_error(node->line_number,
                       "Undeclared function");
        return;
    }

    ASTNode *arg = node->left;
    int i = 0;

    while (arg) {
        analyze_node(arg);

        if (i >= sym->param_count)
            semantic_error(node->line_number,
                           "Too many arguments");

        else if (arg->data_type != sym->param_types[i])
            semantic_error(node->line_number,
                           "Argument type mismatch");

        arg = arg->next;
        i++;
    }

    if (i < sym->param_count)
        semantic_error(node->line_number,
                       "Too few arguments");

    node->data_type = sym->type;
}

int analyze_return(ASTNode *node) {

    if (!current_function){
        semantic_error(node->line_number,
                       "Return outside function");
        return 0;
   }

   if (node->left){ // return with some parameter
    analyze_node(node->left);

    if (node->left->data_type != current_function->type)
        semantic_error(node->line_number,
                       "Return type mismatch");

   } else { //for simply return with no return value
           if(current_function->type != TYPE_VOID)
                   semantic_error(node->line_number,
                                   "Return type mismatch");
   }
   return 1;
}


int analyze_if(ASTNode *node) {

    // Analyze condition
    analyze_node(node->cond);

    if (node->cond->data_type == TYPE_VOID){
         semantic_error(node->line_number,
                   "Invalid condition type");
    }
    // THEN block
    int then_returns = analyze_node(node->left);

    int else_returns = 0;

    // ELSE block
    if (node->right) {
            else_returns = analyze_node(node->right);
    }

    return (then_returns && else_returns);
}

int analyze_while(ASTNode *node) {

    analyze_node(node->cond);

    if (node->cond->data_type == TYPE_VOID)
         semantic_error(node->line_number,
                   "Invalid condition type");


    analyze_node(node->body);

    return 0;

}

int analyze_for(ASTNode *node) {

    if (node->init)
        analyze_node(node->init);

    if (node->cond){
        analyze_node(node->cond);
       if (node->cond->data_type == TYPE_VOID)
       {
               semantic_error(node->line_number, "Invalid condition type");
       }
    }

    if (node->incr)
        analyze_node(node->incr);

    analyze_node(node->body);

    return 0;
}


int analyze_node(ASTNode *node) {
    if (!node) return 0;

    switch(node->type) {

        case NODE_FUNC_DEF:
            analyze_function(node);
            return 0;

        case NODE_VAR_DECL:
            analyze_declaration(node);
            return 0;;

        case NODE_BLOCK:
            return analyze_block(node);

        case NODE_IF:
            return analyze_if(node);

        case NODE_WHILE:
            return analyze_while(node);

        case NODE_FOR:
            return analyze_for(node);

        case NODE_RETURN:
            return analyze_return(node);

        case NODE_ASSIGN:
            analyze_assignment(node);
            return 0;

        case NODE_BIN_OP:
            analyze_binary(node);
            return 0;

        case NODE_UN_OP:
            analyze_unary(node);
            return 0;

        case NODE_CONST_INT:
            node->data_type = TYPE_INT;
            return 0;

        case NODE_CONST_CHAR:
            node->data_type = TYPE_CHAR;
            return 0;

        case NODE_STR_LIT:
            node->data_type = TYPE_CHAR;
            return 0;

        case NODE_VAR:
            analyze_variable(node);
            return 0;

        case NODE_FUNC_CALL:
            analyze_function_call(node);
            return 0;

        case NODE_TYPE:
            // type nodes usually need no semantic work
            // break;
        case NODE_EMPTY:
            return 0;

        default:
            return 0;
    }
}

int analyze_list(ASTNode *node) {

    int returns = 0;

    while (node) {
        returns = analyze_node(node);
        if(returns)
        { break; }
        node = node->next;
    }
    return returns;
}

void semantic_analyze(ASTNode *node) {
    if (!node) return;
    analyze_list(node);
}
