/**
 * ir_gen.h - AST to IR (three-address code) generation
 * Week 4: IR generation from semantically-analyzed AST
 */

#ifndef IR_GEN_H
#define IR_GEN_H

#include "ast.h"
#include "ir.h"

/* Generate IR from AST. Call after semantic analysis. */
IRProgram* ir_generate(ASTNode *ast_root);

#endif /* IR_GEN_H */
