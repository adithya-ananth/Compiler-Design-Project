%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symbol_table.h"
#include "semantic.h"
#include "ir_gen.h"
#include "ir_opt.h"

void print_vtables() {
    Scope *global = current_scope;
    while (global && global->level > 0) global = global->parent;
    if (!global) return;
    for (int i = 0; i < TABLE_SIZE; i++) {
        Symbol *sym = global->table[i];
        while (sym) {
            if (sym->kind == SYM_STRUCT && sym->virtual_methods) {
                printf("vtable_%s:\n", sym->name);
                Symbol *m = sym->virtual_methods;
                int idx = 0;
                while (m) {
                    printf("  .word %s\n", m->name);
                    idx++;
                    m = m->next_member;
                }
            }
            sym = sym->next;
        }
    }
}

// Declarations from Flex
extern int yylex();
extern int line_num;
extern int col_num;
extern char *yytext;
extern FILE *yyin;

void yyerror(const char *s);

#define SET_LINE(n) (n)->line_number = line_num;


// Global Root
ASTNode *root = NULL;
%}

%union {
    int intval;
    char *str;
    struct ASTNode *node;
}

/* Tokens */
%token <intval> T_INT T_VOID T_CHAR T_STRUCT T_VIRTUAL T_CLASS T_PUBLIC T_PRIVATE T_COLON
%token <intval> T_IF T_ELSE T_WHILE T_FOR T_RETURN T_SWITCH T_CASE T_DEFAULT T_BREAK T_CONTINUE
%token <str>    T_IDENT T_STRING_LIT
%token <intval> T_NUMBER T_CHAR_LIT
%token <intval> T_ARROW T_TILDE

/* Operators */
%token T_EQ T_NEQ T_LE T_GE T_AND T_OR

/* Precedence (lowest to highest) */
%right '='
%left T_OR
%left T_AND
%left T_EQ T_NEQ
%left '<' '>' T_LE T_GE
%left '+' '-'
%left '*' '/' '%'
%right '!'
%nonassoc LOWER_THAN_ELSE
%nonassoc T_ELSE

/* Types for non-terminals */
%type <node> program external_declaration_list external_declaration
%type <node> function_definition parameter_list parameter_declaration
%type <node> declaration declarator_list init_declarator declarator pointer direct_declarator type_specifier
%type <node> statement compound_statement block_item_list block_item
%type <node> expression_statement selection_statement iteration_statement jump_statement
%type <node> switch_statement switch_clause_list switch_clause
%type <node> statement_list statement_list_opt
%type <node> expression assignment_expression logical_or_expression logical_and_expression
%type <node> equality_expression relational_expression additive_expression
%type <node> multiplicative_expression unary_expression postfix_expression primary_expression
%type <node> argument_expression_list
%type <node> struct_specifier struct_declaration_list struct_member class_specifier

%%

/* Grammar Rules */

program
    : external_declaration_list { root = $1; }
    ;

external_declaration_list
    : external_declaration { $$ = $1; }
    | external_declaration_list external_declaration {
        $$ = append_node($1, $2);
    }
    ;

external_declaration
    : function_definition { $$ = $1; }
    | declaration { $$ = $1; }
    | error ';' {
          yyerrok;
          $$ = NULL;
      }
    ;

/* Function Definition */
function_definition
    : type_specifier T_IDENT '(' parameter_list ')' compound_statement {
        $$ = create_func_def($1, $2, $4, $6);
        SET_LINE($$);
    }
    | type_specifier T_IDENT '(' ')' compound_statement {
        $$ = create_func_def($1, $2, NULL, $5);
        SET_LINE($$);
    }
    ;

parameter_list
    : parameter_declaration { $$ = $1; }
    | parameter_list ',' parameter_declaration {
        $$ = append_node($1, $3);
    }
    ;

parameter_declaration
    : type_specifier declarator {
        ASTNode *param = create_node(NODE_PARAM);
        SET_LINE(param);
        param->left = $1;
        param->str_val = $2->str_val;
        param->pointer_level = $2->pointer_level;
        param->array_dim_count = $2->array_dim_count;
        param->array_dim_exprs = $2->array_dim_exprs;
        // Note: $2 is freed implicitly or we can free it
        $$ = param;
    }
    ;

/* Declarations */
declaration
    : type_specifier declarator_list ';' {
        /* If this is a struct definition with declarators (e.g. "struct S { ... } a;")
         * we need to keep the struct definition node in the AST so semantic
         * analysis can register the struct type before processing the variables.
         */
        ASTNode *result = NULL;
        ASTNode *type_node = NULL;

        if ($1->type == NODE_STRUCT_DEF) {
            /* Keep the struct definition in the list */
            result = $1;
            /* Build a type node to attach to each declarator */
            type_node = create_type_node(T_STRUCT);
            type_node->str_val = strdup($1->str_val);
            type_node->left = $1; /* keep definition for semantic use */
        } else {
            /* Normal type specifier (int/char/void/struct ref) */
            type_node = $1;
        }

        /* Distribute type to all declarators */
        ASTNode *temp = $2;
        while(temp) {
            temp->left = type_node;
            if (type_node->type == NODE_TYPE && type_node->int_val == T_STRUCT) {
                /* Preserve struct tag name if set */
                if (!temp->left->str_val && type_node->str_val)
                    temp->left->str_val = strdup(type_node->str_val);
            }
            temp = temp->next;
        }

        if (result) {
            /* append declarators after struct definition */
            $$ = append_node(result, $2);
        } else {
            $$ = $2;
        }
    }
    | type_specifier declarator_list error {
        yyerrok;
        $$ = $2;
      }
    | struct_specifier ';' {
        /* Standalone struct definition (no variable declared) */
        $$ = $1;
    }
    | class_specifier ';' {
        /* Standalone class definition (no variable declared) */
        $$ = $1;
    }
    ;

declarator_list
    : init_declarator { $$ = $1; }
    | declarator_list ',' init_declarator {
        if ($3)
          $$ = append_node($1, $3);
        else
          $$ = $1;
    }
    ;

init_declarator
    : declarator { $$ = $1; }
    | declarator '=' expression {
        $1->right = $3;   /* attach initializer */
        $$ = $1;
    }
    ;

declarator
    : pointer direct_declarator {
        $$ = $2;
        $$->pointer_level = $1->int_val;
    }
    | direct_declarator {
        $$ = $1;
        $$->pointer_level = 0;
    }
    ;

pointer
    : '*' {
        $$ = create_node(NODE_TYPE);
        $$->int_val = 1;
    }
    | '*' pointer {
        $$ = $2;
        $$->int_val++;
    }
    ;

direct_declarator
    : T_IDENT {
        $$ = create_node(NODE_VAR_DECL);
        SET_LINE($$);
        $$->str_val = strdup($1);
        $$->array_dim_count = 0;
        $$->array_dim_exprs = NULL;
        $$->next = NULL;
    }
    | direct_declarator '[' expression ']' {
        $$ = $1;
        $$->array_dim_exprs = realloc($$->array_dim_exprs, sizeof(ASTNode*) * ($$->array_dim_count + 1));
        $$->array_dim_exprs[$$->array_dim_count] = $3;
        $$->array_dim_count++;
    }
    | direct_declarator '[' ']' {
        $$ = $1;
        $$->array_dim_exprs = realloc($$->array_dim_exprs, sizeof(ASTNode*) * ($$->array_dim_count + 1));
        $$->array_dim_exprs[$$->array_dim_count] = NULL; // VLA
        $$->array_dim_count++;
    }
    ;

type_specifier: T_INT { $$ = create_type_node(T_INT); }
               | T_CHAR { $$ = create_type_node(T_CHAR); }
               | T_VOID { $$ = create_type_node(T_VOID); }
               | struct_specifier { $$ = $1; }
               | class_specifier { $$ = $1; }
               | T_IDENT { 
                   $$ = create_node(NODE_TYPE); 
                   $$->str_val = strdup($1); 
                   $$->data_type = TYPE_STRUCT; 
                   SET_LINE($$);
               }
               ;

struct_specifier
    : T_STRUCT T_IDENT '{' struct_declaration_list '}' {
        ASTNode *node = create_node(NODE_STRUCT_DEF);
        SET_LINE(node);
        node->str_val = strdup($2);             /* struct tag */
        node->body = $4;                        /* member declarations */
        $$ = node;
    }
    | T_STRUCT T_IDENT {
        /* Struct type reference (no definition)
         * We create a NODE_TYPE with int_val = T_STRUCT and str_val = tag.
         */
        ASTNode *node = create_type_node(T_STRUCT);
        node->str_val = strdup($2);
        $$ = node;
    }
    ;

class_specifier
    : T_CLASS T_IDENT '{' struct_declaration_list '}' {
        ASTNode *node = create_node(NODE_STRUCT_DEF);
        SET_LINE(node);
        node->str_val = strdup($2);
        node->body = $4;
        node->is_class = 1;
        $$ = node;
    }
    | T_CLASS T_IDENT T_COLON T_IDENT '{' struct_declaration_list '}' {
        ASTNode *node = create_node(NODE_STRUCT_DEF);
        SET_LINE(node);
        node->str_val = strdup($2);
        node->base_class_name = strdup($4);
        node->body = $6;
        node->is_class = 1;
        $$ = node;
    }
    | T_CLASS T_IDENT {
        ASTNode *node = create_type_node(T_CLASS);
        node->str_val = strdup($2);
        $$ = node;
    }
    ;

struct_declaration_list
    : struct_member { $$ = $1; }
    | struct_declaration_list struct_member { $$ = append_node($1, $2); }
    ;

struct_member
    : declaration { $$ = $1; }
    | T_VIRTUAL function_definition {
        /* Mark the function as virtual */
        $2->is_virtual = 1;
        $$ = $2;
    }
    | function_definition { $$ = $1; }
    | T_IDENT '(' parameter_list ')' compound_statement {
        /* Constructor with params */
        $$ = create_func_def(create_type_node(T_VOID), $1, $3, $5);
        $$->is_constructor = 1;
        SET_LINE($$);
    }
    | T_IDENT '(' ')' compound_statement {
        /* Constructor without params */
        $$ = create_func_def(create_type_node(T_VOID), $1, NULL, $4);
        $$->is_constructor = 1;
        SET_LINE($$);
    }
    | T_TILDE T_IDENT '(' ')' compound_statement {
        /* Destructor */
        $$ = create_func_def(create_type_node(T_VOID), $2, NULL, $5);
        $$->is_destructor = 1;
        SET_LINE($$);
    }
    | T_PUBLIC T_COLON {
        ASTNode *node = create_node(NODE_ACCESS_SPEC);
        SET_LINE(node);
        node->access_modifier = 0; /* public */
        $$ = node;
    }
    | T_PRIVATE T_COLON {
        ASTNode *node = create_node(NODE_ACCESS_SPEC);
        SET_LINE(node);
        node->access_modifier = 1; /* private */
        $$ = node;
    }
    ;

/* Statements */
statement
    : compound_statement { $$ = $1; }
    | expression_statement { $$ = $1; }
    | selection_statement { $$ = $1; }
    | iteration_statement { $$ = $1; }
    | jump_statement { $$ = $1; }
    | switch_statement { $$ = $1; }
    ;

compound_statement
    : '{' '}' {
        $$ = create_node(NODE_BLOCK);
        SET_LINE($$);
    }
    | '{' block_item_list '}' {
        $$ = create_node(NODE_BLOCK);
        SET_LINE($$);
        $$->left = $2;
    }
    | '{' error '}' {
        yyerrok;
        $$ = create_node(NODE_BLOCK);
        SET_LINE($$);
      }
    ;

block_item_list
    : block_item { $$ = $1; }
    | block_item_list block_item {
        $$ = append_node($1, $2);
    }
    ;

block_item
    : declaration { $$ = $1; }
    | statement { $$ = $1; }
    ;

expression_statement
    : ';' { $$ = create_node(NODE_EMPTY);  SET_LINE($$); }
    | expression ';' { $$ = $1; }
    | error ';' {
          yyerrok;
          $$ = create_node(NODE_EMPTY);
          SET_LINE($$);
      }
    ;

selection_statement
    : T_IF '(' expression ')' statement %prec LOWER_THAN_ELSE {
        $$ = create_if_node($3, $5, NULL);
        SET_LINE($$);
    }
    | T_IF '(' expression ')' statement T_ELSE statement {
        $$ = create_if_node($3, $5, $7);
        SET_LINE($$);
    }
    ;

iteration_statement
    : T_WHILE '(' expression ')' statement {
        $$ = create_while_node($3, $5);
        SET_LINE($$);
    }
    | T_FOR '(' expression_statement expression_statement ')' statement {
        // For loop without increment
        // Note: expression_statement includes the node, we might want to extract children or just link them
        $$ = create_for_node($3, $4, NULL, $6);
        SET_LINE($$);
    }
    | T_FOR '(' expression_statement expression_statement expression ')' statement {
        $$ = create_for_node($3, $4, $5, $7);
        SET_LINE($$);
    }
    ;

jump_statement
    : T_RETURN ';' {
        $$ = create_node(NODE_RETURN);
        SET_LINE($$);
    }
    | T_RETURN expression ';' {
        $$ = create_node(NODE_RETURN);
        SET_LINE($$);
        $$->left = $2;
    }
    | T_BREAK ';' {
        $$ = create_break_node();
        SET_LINE($$);
    }
    | T_CONTINUE ';' {
        $$ = create_continue_node();
        SET_LINE($$);
    }
    ;

switch_statement
    : T_SWITCH '(' expression ')' '{' switch_clause_list '}' {
        $$ = create_switch_node($3, $6);
        SET_LINE($$);
    }
    ;

switch_clause_list
    : switch_clause { $$ = $1; }
    | switch_clause_list switch_clause {
        $$ = append_node($1, $2);
    }
    ;

switch_clause
    : T_CASE expression T_COLON statement_list_opt {
        $$ = create_case_node($2, $4);
        SET_LINE($$);
    }
    | T_DEFAULT T_COLON statement_list_opt {
        $$ = create_case_node(NULL, $3);
        SET_LINE($$);
    }
    ;

statement_list_opt
    : /* empty */ { $$ = NULL; }
    | statement_list { $$ = $1; }
    ;

statement_list
    : statement { $$ = $1; }
    | statement_list statement {
        $$ = append_node($1, $2);
    }
    ;

/* Expressions */
expression
    : assignment_expression { $$ = $1; }
    ;

assignment_expression
    : logical_or_expression { /* no assignment, just propagate the expression */
        $$ = $1;
    }
    /* allow any lvalue produced by logical_or_expression (identifier, index, etc.) */
    | logical_or_expression '=' assignment_expression {
        /* left side is already an ASTNode representing a variable or indexed value */
        $$ = create_node(NODE_ASSIGN);
        SET_LINE($$);
        $$->left = $1;
        $$->right = $3;
    }
    ;

logical_or_expression
    : logical_and_expression { $$ = $1; }
    | logical_or_expression T_OR logical_and_expression {
        $$ = create_binary_node(T_OR, $1, $3);
        SET_LINE($$);
    }
    ;

logical_and_expression
    : equality_expression { $$ = $1; }
    | logical_and_expression T_AND equality_expression {
        $$ = create_binary_node(T_AND, $1, $3);
        SET_LINE($$);
    }
    ;

equality_expression
    : relational_expression { $$ = $1; }
    | equality_expression T_EQ relational_expression {
        $$ = create_binary_node(T_EQ, $1, $3);
        SET_LINE($$);
    }
    | equality_expression T_NEQ relational_expression {
        $$ = create_binary_node(T_NEQ, $1, $3);
        SET_LINE($$);
    }
    ;

relational_expression
    : additive_expression { $$ = $1; }
    | relational_expression '<' additive_expression {
        $$ = create_binary_node('<', $1, $3);
        SET_LINE($$);
    }
    | relational_expression '>' additive_expression {
        $$ = create_binary_node('>', $1, $3);
        SET_LINE($$);
    }
    | relational_expression T_LE additive_expression {
        $$ = create_binary_node(T_LE, $1, $3);
        SET_LINE($$);
    }
    | relational_expression T_GE additive_expression {
        $$ = create_binary_node(T_GE, $1, $3);
        SET_LINE($$);
    }
    ;

additive_expression
    : multiplicative_expression { $$ = $1; }
    | additive_expression '+' multiplicative_expression {
        $$ = create_binary_node('+', $1, $3);
        SET_LINE($$);
    }
    | additive_expression '-' multiplicative_expression {
        $$ = create_binary_node('-', $1, $3);
        SET_LINE($$);
    }
    ;

multiplicative_expression
    : unary_expression { $$ = $1; }
    | multiplicative_expression '*' unary_expression {
        $$ = create_binary_node('*', $1, $3);
        SET_LINE($$);
    }
    | multiplicative_expression '/' unary_expression {
        $$ = create_binary_node('/', $1, $3);
        SET_LINE($$);
    }
    | multiplicative_expression '%' unary_expression {
        $$ = create_binary_node('%', $1, $3);
        SET_LINE($$);
    }
    ;

postfix_expression
    : primary_expression { $$ = $1; }
    | postfix_expression '[' expression ']' {
        $$ = create_index_node($1, $3);
        SET_LINE($$);
    }
    | postfix_expression '.' T_IDENT {
        ASTNode *node = create_node(NODE_MEMBER_ACCESS);
        SET_LINE(node);
        node->left = $1;
        node->str_val = strdup($3);
        node->int_val = 0; /* dot */
        $$ = node;
    }
    | postfix_expression T_ARROW T_IDENT {
        ASTNode *node = create_node(NODE_MEMBER_ACCESS);
        SET_LINE(node);
        node->left = $1;
        node->str_val = strdup($3);
        node->int_val = 1; /* arrow */
        $$ = node;
    }
    | postfix_expression '(' argument_expression_list ')' {
        ASTNode *func = create_node(NODE_FUNC_CALL);
        SET_LINE(func);
        func->left = $1; /* Callee */
        func->right = $3; /* Arguments */
        if ($1 && $1->type == NODE_VAR) {
            func->str_val = strdup($1->str_val);
        }
        $$ = func;
    }
    | postfix_expression '(' ')' {
        ASTNode *func = create_node(NODE_FUNC_CALL);
        SET_LINE(func);
        func->left = $1; /* Callee */
        func->right = NULL;
        if ($1 && $1->type == NODE_VAR) {
            func->str_val = strdup($1->str_val);
        }
        $$ = func;
    }
    ;

unary_expression
    : postfix_expression { $$ = $1; }
    | '-' unary_expression {
        $$ = create_unary_node('-', $2);
        SET_LINE($$);
    }
    | '!' unary_expression {
        $$ = create_unary_node('!', $2);
        SET_LINE($$);
    }
    | '&' unary_expression {
        $$ = create_unary_node('&', $2);
        SET_LINE($$);
    }
    | '*' unary_expression {
        $$ = create_unary_node('*', $2);
        SET_LINE($$);
    }
    ;

primary_expression
    : T_IDENT {
        $$ = create_var_node($1);
        SET_LINE($$);
    }
    | T_NUMBER {
        $$ = create_int_node($1);
        SET_LINE($$);
    }
    | T_CHAR_LIT {
        $$ = create_char_node($1);
        SET_LINE($$);
    }
    | T_STRING_LIT {
        $$ = create_str_node($1);
        SET_LINE($$);
    }
    | '(' expression ')' {
        $$ = $2;
    }
    ;

argument_expression_list
    : expression { $$ = $1; }
    | argument_expression_list ',' expression {
        $$ = append_node($1, $3);
    }
    ;

%%

int parse_errors = 0;

void yyerror(const char *s) {
    fprintf(stderr, "Parser Error: %s at line %d, column %d (token: %s)\n", s, line_num, col_num, yytext);
    parse_errors++;
}

int main(int argc, char **argv) {
    // Check for --debug flag
    int arg_idx = 1;
    if (argc > 1 && strcmp(argv[1], "--debug") == 0) {
        // yydebug = 1; // Needs #define YYDEBUG 1
        arg_idx++;
        printf("Debug mode enabled\n");
    }

    // Check for filename
    if (arg_idx < argc) {
        FILE *file = fopen(argv[arg_idx], "r");
        if (!file) {
            perror("Error opening file");
            return 1;
        }
        yyin = file;
    }

    printf("Parsing...\n");
    int parse_result = yyparse();

    if(parse_result == 0 && parse_errors == 0 && root != NULL){
        init_symbol_table();
        semantic_analyze(root);
        if (semantic_errors == 0)
        {
          print_symbol_table();
          printf("Semantic analysis successful.\n");
          /* Week 4: IR generation */
          IRProgram *ir = ir_generate(root);
          if (ir) {
            ir_print_program(ir);
            print_vtables();
            ir_export_to_file(ir, "ir.txt");
            
            /* Optimization pass */
            printf("Optimizing IR...\n");
            optimize_program(ir);
            printf("Optimization complete. Optimized IR printed below:\n");
            ir_print_program(ir);
            ir_export_to_file(ir, "ir_opt.txt");

            ir_free_program(ir);
          }
        }
        else{
         printf("Semantic analysis failed with %d errors.\n", semantic_errors);
        }
    }

    if (parse_result == 0) {
        printf("Parsing Done with %d errors\n", parse_errors);
        printf("AST Structure:\n");
        if (root) {
            print_ast(root, 0);
            export_ast_to_dot(root, "ast.dot");
        }
        return 0;
    } else {
        printf("Parsing Failed\n");
        return 1;
     }
}
