#ifndef SEMANTIC_H
#define SEMANTIC_H
extern int semantic_errors;

void semantic_error(int line, const char *msg);

void semantic_analyze(ASTNode *node);

int analyze_node(ASTNode *node);

void analyze_function(ASTNode *node);

void analyze_declaration(ASTNode *node);

int analyze_block(ASTNode *node);

void analyze_variable(ASTNode *node);

void analyze_assignment(ASTNode *node);

void analyze_binary(ASTNode *node);

void analyze_unary(ASTNode *node);

void analyze_function_call(ASTNode *node);

int analyze_return(ASTNode *node);

int analyze_list(ASTNode *node);

int analyze_if(ASTNode *node);

int analyze_while(ASTNode *node);

int analyze_for(ASTNode* node);

#endif
