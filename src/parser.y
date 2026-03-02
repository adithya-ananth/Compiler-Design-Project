%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symbol_table.h"
#include "semantic.h"
#include "ir_gen.h"


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
%token <intval> T_INT T_VOID T_CHAR
%token <intval> T_IF T_ELSE T_WHILE T_FOR T_RETURN
%token <str>    T_IDENT T_STRING_LIT
%token <intval> T_NUMBER T_CHAR_LIT

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
%type <node> declaration declarator_list declarator type_specifier
%type <node> statement compound_statement block_item_list block_item
%type <node> expression_statement selection_statement iteration_statement jump_statement
%type <node> expression assignment_expression logical_or_expression logical_and_expression
%type <node> equality_expression relational_expression additive_expression
%type <node> multiplicative_expression unary_expression primary_expression
%type <node> argument_expression_list

%%

/* Grammar Rules */

program
    : external_declaration_list { root = $1; }
    ;

external_declaration_list
    : external_declaration { $$ = $1; }
    | external_declaration_list external_declaration {
        $$ = $1;
        append_node($$, $2);
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
        $$ = $1;
        append_node($$, $3);
    }
    ;

parameter_declaration
    : type_specifier T_IDENT {
        $$ = create_node(NODE_PARAM);
        SET_LINE($$);
        $$->left = $1;
        $$->str_val = strdup($2);
    }
    ;

/* Declarations */
declaration
    : type_specifier declarator_list ';' {
        // Distribute type to all declarators
        ASTNode *temp = $2;
        while(temp) {
            temp->left = $1; // Set type
            temp = temp->next;
        }
        $$ = $2;
    }
    | type_specifier declarator_list error {
        yyerrok;
        $$ = $2;
      }
    ;

declarator_list
    : declarator { $$ = $1; }
    | declarator_list ',' declarator {
        $$ = $1;
        append_node($$, $3);
    }
    ;

declarator
    : T_IDENT {
        $$ = create_node(NODE_VAR_DECL);
        SET_LINE($$);
        $$->str_val = strdup($1);
    }
    | T_IDENT '=' expression {
        $$ = create_node(NODE_VAR_DECL);
        SET_LINE($$);
        $$->str_val = strdup($1);
        $$->right = $3; // Initializer
    }
    ;

type_specifier
    : T_INT  { $$ = create_type_node(T_INT);  SET_LINE($$); }
    | T_VOID { $$ = create_type_node(T_VOID); SET_LINE($$); }
    | T_CHAR { $$ = create_type_node(T_CHAR); SET_LINE($$); }
    ;

/* Statements */
statement
    : compound_statement { $$ = $1; }
    | expression_statement { $$ = $1; }
    | selection_statement { $$ = $1; }
    | iteration_statement { $$ = $1; }
    | jump_statement { $$ = $1; }
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
        $$ = $1;
        append_node($$, $2);
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
    ;

/* Expressions */
expression
    : assignment_expression { $$ = $1; }
    ;

assignment_expression
    : logical_or_expression { $$ = $1; }
    | T_IDENT '=' assignment_expression {
        ASTNode *var = create_var_node($1);
        $$ = create_node(NODE_ASSIGN);
        SET_LINE($$);
        $$->left = var;
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
    }
    | multiplicative_expression '/' unary_expression {
        $$ = create_binary_node('/', $1, $3);
    }
    | multiplicative_expression '%' unary_expression {
        $$ = create_binary_node('%', $1, $3);
    }
    ;

unary_expression
    : primary_expression { $$ = $1; }
    | '-' unary_expression {
        $$ = create_unary_node('-', $2);
        SET_LINE($$);
    }
    | '!' unary_expression {
        $$ = create_unary_node('!', $2);
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
    | T_IDENT '(' argument_expression_list ')' {
        ASTNode *func = create_node(NODE_FUNC_CALL);
        SET_LINE(func);
        func->str_val = strdup($1);
        func->left = $3; // Arguments
        $$ = func;
    }
    | T_IDENT '(' ')' {
        ASTNode *func = create_node(NODE_FUNC_CALL);
        SET_LINE(func);
        func->str_val = strdup($1);
        $$ = func;
    }
    ;

argument_expression_list
    : expression { $$ = $1; }
    | argument_expression_list ',' expression {
        $$ = $1;
        append_node($$, $3);
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
            ir_export_to_file(ir, "ir.txt");
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
