/**
 * ir_gen.c - AST to three-address IR generation
 * Walks the AST and emits linear three-address instructions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "ir.h"
#include "ir_gen.h"
#include "y.tab.h"

/* Generate code for condition; jump to true_label if true, else false_label */
static void gen_cond(ASTNode *node, IRInstr **list, char *true_label, char *false_label, int line);

/* Generate code for expression; returns operand holding the result */
static IROperand gen_expr(ASTNode *node, IRInstr **list);

/* Generate code for statement */
static void gen_stmt(ASTNode *node, IRInstr **list);

/* --- Condition generation (for if/while/for) --- */
static void gen_cond(ASTNode *node, IRInstr **list, char *true_label, char *false_label, int line) {
    if (!node) {
        ir_append(list, ir_make_goto(true_label, line));
        return;
    }
    line = node->line_number;

    switch (node->type) {
        case NODE_CONST_INT:
            if (node->int_val)
                ir_append(list, ir_make_goto(true_label, line));
            else
                ir_append(list, ir_make_goto(false_label, line));
            break;

        case NODE_CONST_CHAR:
            if (node->int_val)
                ir_append(list, ir_make_goto(true_label, line));
            else
                ir_append(list, ir_make_goto(false_label, line));
            break;

        case NODE_VAR: {
            IROperand op = ir_op_name(node->str_val);
            ir_append(list, ir_make_if(op, ir_op_const(0), IR_NE, true_label, line));
            ir_append(list, ir_make_goto(false_label, line));
            if (op.name) free(op.name);
            break;
        }

        case NODE_BIN_OP:
            if (node->int_val == T_AND) {
                char *mid = ir_new_label();
                gen_cond(node->left, list, mid, false_label, line);
                ir_append(list, ir_make_label(mid, line));
                gen_cond(node->right, list, true_label, false_label, line);
                free(mid);
            } else if (node->int_val == T_OR) {
                char *mid = ir_new_label();
                gen_cond(node->left, list, true_label, mid, line);
                ir_append(list, ir_make_label(mid, line));
                gen_cond(node->right, list, true_label, false_label, line);
                free(mid);
            } else if (node->int_val == '<' || node->int_val == '>' ||
                       node->int_val == T_LE || node->int_val == T_GE ||
                       node->int_val == T_EQ || node->int_val == T_NEQ) {
                IROperand l = gen_expr(node->left, list);
                IROperand r = gen_expr(node->right, list);
                ir_append(list, ir_make_if(l, r, ast_relop_to_ir(node->int_val), true_label, line));
                ir_append(list, ir_make_goto(false_label, line));
                if (l.name) free(l.name);
                if (r.name) free(r.name);
            } else {
                /* Arithmetic in condition: evaluate, then branch on non-zero */
                IROperand place = gen_expr(node, list);
                ir_append(list, ir_make_if(place, ir_op_const(0), IR_NE, true_label, line));
                ir_append(list, ir_make_goto(false_label, line));
                if (place.name) free(place.name);
            }
            break;

        case NODE_UN_OP:
            if (node->int_val == '!') {
                gen_cond(node->left, list, false_label, true_label, line);
            } else {
                IROperand place = gen_expr(node, list);
                ir_append(list, ir_make_if(place, ir_op_const(0), IR_NE, true_label, line));
                ir_append(list, ir_make_goto(false_label, line));
                if (place.name) free(place.name);
            }
            break;

        case NODE_FUNC_CALL: {
            IROperand place = gen_expr(node, list);
            ir_append(list, ir_make_if(place, ir_op_const(0), IR_NE, true_label, line));
            ir_append(list, ir_make_goto(false_label, line));
            if (place.name) free(place.name);
            break;
        }

        default:
            /* Other expr: evaluate and branch on non-zero */
            IROperand place = gen_expr(node, list);
            ir_append(list, ir_make_if(place, ir_op_const(0), IR_NE, true_label, line));
            ir_append(list, ir_make_goto(false_label, line));
            if (place.name) free(place.name);
            break;
    }
}

/* --- Expression generation --- */
static IROperand gen_expr(ASTNode *node, IRInstr **list) {
    if (!node) return ir_op_const(0);

    int line = node->line_number;

    switch (node->type) {
        case NODE_CONST_INT:
            return ir_op_const(node->int_val);

        case NODE_CONST_CHAR:
            return ir_op_const(node->int_val);

        case NODE_STR_LIT:
            /* String literals: treat as char* - for now use address 0 as placeholder */
            return ir_op_const(0);

        case NODE_VAR:
            return ir_op_name(node->str_val);

        case NODE_BIN_OP: {
            if (node->int_val == T_AND || node->int_val == T_OR) {
                /* Short-circuit: produce 0 or 1 */
                char *t = ir_new_temp();
                char *L_true = ir_new_label();
                char *L_false = ir_new_label();
                char *L_end = ir_new_label();
                ir_append(list, ir_make_assign(t, ir_op_const(0), line));
                gen_cond(node, list, L_true, L_false, line);
                ir_append(list, ir_make_label(L_true, line));
                ir_append(list, ir_make_assign(t, ir_op_const(1), line));
                ir_append(list, ir_make_goto(L_end, line));
                ir_append(list, ir_make_label(L_false, line));
                ir_append(list, ir_make_label(L_end, line));
                free(L_true);
                free(L_false);
                free(L_end);
                IROperand op = ir_op_name(t);
                free(t);
                return op;
            }
            if (node->int_val == '<' || node->int_val == '>' ||
                node->int_val == T_LE || node->int_val == T_GE ||
                node->int_val == T_EQ || node->int_val == T_NEQ) {
                /* Relational in value context: produce 0 or 1 */
                IROperand l = gen_expr(node->left, list);
                IROperand r = gen_expr(node->right, list);
                char *t = ir_new_temp();
                char *L_true = ir_new_label();
                char *L_end = ir_new_label();
                ir_append(list, ir_make_assign(t, ir_op_const(0), line));
                ir_append(list, ir_make_if(l, r, ast_relop_to_ir(node->int_val), L_true, line));
                ir_append(list, ir_make_goto(L_end, line));
                ir_append(list, ir_make_label(L_true, line));
                ir_append(list, ir_make_assign(t, ir_op_const(1), line));
                ir_append(list, ir_make_label(L_end, line));
                if (l.name) free(l.name);
                if (r.name) free(r.name);
                IROperand op = ir_op_name(t);
                free(t);
                return op;
            }
            /* Arithmetic */
            IROperand left = gen_expr(node->left, list);
            IROperand right = gen_expr(node->right, list);
            char *t = ir_new_temp();
            ir_append(list, ir_make_binop(t, left, right, node->int_val, line));
            if (left.name) free(left.name);
            if (right.name) free(right.name);
            IROperand res = ir_op_name(t);
            free(t);
            return res;
        }

        case NODE_UN_OP: {
            IROperand child = gen_expr(node->left, list);
            char *t = ir_new_temp();
            ir_append(list, ir_make_unop(t, child, node->int_val, line));
            if (child.name) free(child.name);
            IROperand res = ir_op_name(t);
            free(t);
            return res;
        }

        case NODE_ASSIGN: {
            IROperand val = gen_expr(node->right, list);
            char *target = node->left->str_val;
            ir_append(list, ir_make_assign(target, val, line));
            if (val.name) free(val.name);
            return ir_op_name(target);
        }

        case NODE_FUNC_CALL: {
            int nargs = 0;
            ASTNode *arg = node->left;
            while (arg) {
                IROperand a = gen_expr(arg, list);
                ir_append(list, ir_make_param(a, line));
                if (a.name) free(a.name);
                nargs++;
                arg = arg->next;
            }
            /* Return type from semantic analysis */
            int is_void = (node->data_type == TYPE_VOID);
            if (is_void) {
                ir_append(list, ir_make_call_void(node->str_val, nargs, line));
                return ir_op_const(0);
            }
            char *t = ir_new_temp();
            ir_append(list, ir_make_call(t, node->str_val, nargs, line));
            IROperand res = ir_op_name(t);
            free(t);
            return res;
        }

        default:
            return ir_op_const(0);
    }
}

/* --- Statement generation --- */
static void gen_stmt(ASTNode *node, IRInstr **list) {
    if (!node) return;

    int line = node->line_number;

    switch (node->type) {
        case NODE_EMPTY:
        case NODE_TYPE:
            break;

        case NODE_BLOCK:
            for (ASTNode *s = node->left; s; s = s->next)
                gen_stmt(s, list);
            break;

        case NODE_IF: {
            char *L_then = ir_new_label();
            char *L_else = ir_new_label();
            char *L_end = ir_new_label();
            gen_cond(node->cond, list, L_then, node->right ? L_else : L_end, line);
            ir_append(list, ir_make_label(L_then, line));
            gen_stmt(node->left, list);
            if (node->right) {
                ir_append(list, ir_make_goto(L_end, line));
                ir_append(list, ir_make_label(L_else, line));
                gen_stmt(node->right, list);
            }
            ir_append(list, ir_make_label(L_end, line));
            free(L_then);
            free(L_else);
            free(L_end);
            break;
        }

        case NODE_WHILE: {
            char *L_cond = ir_new_label();
            char *L_body = ir_new_label();
            char *L_end = ir_new_label();
            ir_append(list, ir_make_label(L_cond, line));
            gen_cond(node->cond, list, L_body, L_end, line);
            ir_append(list, ir_make_label(L_body, line));
            gen_stmt(node->body, list);
            ir_append(list, ir_make_goto(L_cond, line));
            ir_append(list, ir_make_label(L_end, line));
            free(L_cond);
            free(L_body);
            free(L_end);
            break;
        }

        case NODE_FOR: {
            char *L_cond = ir_new_label();
            char *L_body = ir_new_label();
            char *L_end = ir_new_label();
            if (node->init && node->init->type != NODE_EMPTY)
                gen_stmt(node->init, list);
            ir_append(list, ir_make_label(L_cond, line));
            if (node->cond && node->cond->type != NODE_EMPTY)
                gen_cond(node->cond, list, L_body, L_end, line);
            else
                ir_append(list, ir_make_goto(L_body, line));
            ir_append(list, ir_make_label(L_body, line));
            gen_stmt(node->body, list);
            if (node->incr)
                (void)gen_expr(node->incr, list);
            ir_append(list, ir_make_goto(L_cond, line));
            ir_append(list, ir_make_label(L_end, line));
            free(L_cond);
            free(L_body);
            free(L_end);
            break;
        }

        case NODE_RETURN:
            if (node->left) {
                IROperand val = gen_expr(node->left, list);
                ir_append(list, ir_make_return_val(val, line));
                if (val.name) free(val.name);
            } else
                ir_append(list, ir_make_return(line));
            break;

        case NODE_ASSIGN:
            (void)gen_expr(node, list);
            break;

        case NODE_VAR_DECL:
            if (node->right) {
                IROperand init = gen_expr(node->right, list);
                ir_append(list, ir_make_assign(node->str_val, init, line));
                if (init.name) free(init.name);
            }
            break;

        default:
            /* Expression statement (e.g. foo(); x+1;) */
            (void)gen_expr(node, list);
            break;
    }
}

/* --- Function and program generation --- */
static void gen_func(ASTNode *node, IRProgram *prog) {
    if (!node || node->type != NODE_FUNC_DEF) return;

    DataType ret_type = node->left ? node->left->data_type : TYPE_VOID;
    IRFunc *f = ir_func_create(node->str_val, ret_type);
    ir_reset_temps();

    gen_stmt(node->body, &f->instrs);

    ir_program_add_func(prog, f);
}

IRProgram* ir_generate(ASTNode *ast_root) {
    if (!ast_root) return NULL;

    IRProgram *prog = ir_program_create();

    for (ASTNode *n = ast_root; n; n = n->next) {
        if (n->type == NODE_FUNC_DEF)
            gen_func(n, prog);
        else if (n->type == NODE_VAR_DECL) {
            /* Global var init: emit to global_instrs if needed */
            if (n->right) {
                IRInstr *init = NULL;
                IROperand val = gen_expr(n->right, &init);
                ir_append(&init, ir_make_assign(n->str_val, val, n->line_number));
                if (val.name) free(val.name);
                ir_append_list(&prog->global_instrs, init);
            }
        }
    }

    return prog;
}
