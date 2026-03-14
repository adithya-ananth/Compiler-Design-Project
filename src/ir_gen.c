/**
 * ir_gen.c - AST to three-address IR generation
 * Walks the AST and emits linear three-address instructions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "ir.h"
#include "semantic.h"
#include "ir_gen.h"
#include "y.tab.h"

/* Generate code for condition; jump to true_label if true, else false_label */
static void gen_cond(ASTNode *node, IRInstr **list, char *true_label, char *false_label, int line);

/* Generate code for expression; returns operand holding the result */
static IROperand gen_expr(ASTNode *node, IRInstr **list);

/* Generate code for statement */
static void gen_stmt(ASTNode *node, IRInstr **list);

static void get_index_info(ASTNode *node, char **base_name, IROperand *index_op, IRInstr **list, int line);
static IROperand gen_index_expr(ASTNode *node, IRInstr **list, int line);

/* Break/continue target stacks for loops and switches */
#define MAX_BREAK_DEPTH 64
static char *break_label_stack[MAX_BREAK_DEPTH];
static int break_label_top = 0;
static char *continue_label_stack[MAX_BREAK_DEPTH];
static int continue_label_top = 0;

static void push_break_label(const char *label) {
    if (break_label_top >= MAX_BREAK_DEPTH) return;
    break_label_stack[break_label_top++] = strdup(label);
}

static void pop_break_label(void) {
    if (break_label_top <= 0) return;
    free(break_label_stack[--break_label_top]);
}

static const char *current_break_label(void) {
    if (break_label_top <= 0) return NULL;
    return break_label_stack[break_label_top - 1];
}

static void push_continue_label(const char *label) {
    if (continue_label_top >= MAX_BREAK_DEPTH) return;
    continue_label_stack[continue_label_top++] = strdup(label);
}

static void pop_continue_label(void) {
    if (continue_label_top <= 0) return;
    free(continue_label_stack[--continue_label_top]);
}

static const char *current_continue_label(void) {
    if (continue_label_top <= 0) return NULL;
    return continue_label_stack[continue_label_top - 1];
}

static void get_index_info(ASTNode *node, char **base_name, IROperand *index_op, IRInstr **list, int line) {
    ASTNode *indices[10];
    int num_indices = 0;
    ASTNode *base_node = node;
    while (base_node->type == NODE_INDEX) {
        indices[num_indices++] = base_node->right;
        base_node = base_node->left;
    }
    *base_name = base_node->str_val;
    // Reverse indices: innermost first
    for (int i = 0; i < num_indices / 2; i++) {
        ASTNode *temp = indices[i];
        indices[i] = indices[num_indices - 1 - i];
        indices[num_indices - 1 - i] = temp;
    }
    // Compute linear index
    Symbol *sym = lookup(*base_name);
    if (!sym || sym->array_dim_count == 0) {
        *index_op = ir_op_const(0);
        return;
    }
    IROperand linear = gen_expr(indices[num_indices-1], list);
    int stride = 1;
    for (int i = num_indices - 2; i >= 0; i--) {
        stride *= sym->array_sizes[i+1];
        IROperand idx = gen_expr(indices[i], list);
        char *temp1 = ir_new_temp();
        ir_append(list, ir_make_binop(temp1, idx, ir_op_const(stride), '*', line));
        char *temp2 = ir_new_temp();
        ir_append(list, ir_make_binop(temp2, linear, ir_op_name(temp1), '+', line));
        linear = ir_op_name(temp2);
        free(temp1);
        free(temp2);
    }
    *index_op = linear;
}

static IROperand gen_index_expr(ASTNode *node, IRInstr **list, int line) {
    char *base_name;
    IROperand index_op;
    get_index_info(node, &base_name, &index_op, list, line);
    Symbol *sym = lookup(base_name);
    int scale = (sym && sym->type == TYPE_INT) ? 4 : 1;
    IROperand base_op = ir_op_name(base_name);
    char *scaled_temp = ir_new_temp();
    IROperand scaled_op = ir_op_name(scaled_temp);
    ir_append(list, ir_make_binop(scaled_temp, index_op, ir_op_const(scale), '*', line));
    char *t = ir_new_temp();
    ir_append(list, ir_make_load(t, base_op, scaled_op, 1, line));
    if (base_op.name) free(base_op.name);
    if (index_op.name) free(index_op.name);
    free(scaled_temp);
    IROperand res = ir_op_name(t);
    free(t);
    return res;
}

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
        case NODE_INDEX: {
            return gen_index_expr(node, list, line);
        }

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
            if (node->int_val == '*') {
                // dereference: load from pointer
                IROperand base = gen_expr(node->left, list);
                char *t = ir_new_temp();
                int scale = get_type_size(node->type);
                ir_append(list, ir_make_load(t, base, ir_op_const(0), scale, line));
                if (base.name) free(base.name);
                IROperand res = ir_op_name(t);
                free(t);
                return res;
            } else if (node->int_val == '&') {
                // address-of
                IROperand child = gen_expr(node->left, list);
                char *t = ir_new_temp();
                ir_append(list, ir_make_unop(t, child, node->int_val, line));
                if (child.name) free(child.name);
                IROperand res = ir_op_name(t);
                free(t);
                return res;
            } else {
                // other unops like -
                IROperand child = gen_expr(node->left, list);
                char *t = ir_new_temp();
                ir_append(list, ir_make_unop(t, child, node->int_val, line));
                if (child.name) free(child.name);
                IROperand res = ir_op_name(t);
                free(t);
                return res;
            }
        }

        case NODE_ASSIGN: {
            IROperand val = gen_expr(node->right, list);
            if (node->left->type == NODE_VAR) {
                char *target = node->left->str_val;
                ir_append(list, ir_make_assign(target, val, line));
                if (val.name) free(val.name);
                return ir_op_name(target);
            } else if (node->left->type == NODE_INDEX) {
                /* Array element store */
                char *base_name;
                IROperand index_op;
                get_index_info(node->left, &base_name, &index_op, list, line);
                int scale = 4;
                if (node->left->data_type == TYPE_CHAR) {
                    scale = 1;
                }
                IROperand base_op = ir_op_name(base_name);
                char *scaled_temp = ir_new_temp();
                IROperand scaled_op = ir_op_name(scaled_temp);
                ir_append(list, ir_make_binop(scaled_temp, index_op, ir_op_const(scale), '*', line));
                ir_append(list, ir_make_store(base_op, scaled_op, 1, val, line));
                if (base_op.name) free(base_op.name);
                if (index_op.name) free(index_op.name);
                free(scaled_temp);
                if (val.name) free(val.name);
                /* Result of assignment expression is the stored value */
                return ir_op_const(0);
            } else if (node->left->type == NODE_UN_OP && node->left->int_val == '*') {
                /* Pointer dereference assignment: *p = val */
                IROperand base = gen_expr(node->left->left, list);
                int scale = get_type_size(node->left->type);
                ir_append(list, ir_make_store(base, ir_op_const(0), scale, val, line));
                if (base.name) free(base.name);
                if (val.name) free(val.name);
                return ir_op_const(0);
            } else {
                /* Fallback: treat as simple assignment to unknown target */
                if (val.name) free(val.name);
                return ir_op_const(0);
            }
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
            push_break_label(L_end);
            push_continue_label(L_cond);
            gen_stmt(node->body, list);
            pop_continue_label();
            pop_break_label();
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
            char *L_continue = ir_new_label();
            char *L_end = ir_new_label();
            if (node->init && node->init->type != NODE_EMPTY)
                gen_stmt(node->init, list);
            ir_append(list, ir_make_label(L_cond, line));
            if (node->cond && node->cond->type != NODE_EMPTY)
                gen_cond(node->cond, list, L_body, L_end, line);
            else
                ir_append(list, ir_make_goto(L_body, line));
            ir_append(list, ir_make_label(L_body, line));
            push_break_label(L_end);
            push_continue_label(L_continue);
            gen_stmt(node->body, list);
            pop_continue_label();
            pop_break_label();
            ir_append(list, ir_make_label(L_continue, line));
            if (node->incr)
                (void)gen_expr(node->incr, list);
            ir_append(list, ir_make_goto(L_cond, line));
            ir_append(list, ir_make_label(L_end, line));
            free(L_cond);
            free(L_body);
            free(L_continue);
            free(L_end);
            break;
        }

        case NODE_SWITCH: {
            if (!node->cond || !node->body) {
                if (node->cond) {
                    (void)gen_expr(node->cond, list);
                }
                break;
            }

            /* Collect cases and labels */
            int count = 0;
            for (ASTNode *c = node->body; c; c = c->next) {
                count++;
            }
            if (count == 0) {
                (void)gen_expr(node->cond, list);
                break;
            }

            ASTNode **cases = (ASTNode **)malloc(sizeof(ASTNode *) * count);
            char **labels = (char **)malloc(sizeof(char *) * count);
            int idx = 0;
            int default_index = -1;
            for (ASTNode *c = node->body; c; c = c->next) {
                cases[idx] = c;
                labels[idx] = ir_new_label();
                if (c->type == NODE_CASE && c->left == NULL && default_index == -1) {
                    default_index = idx;
                }
                idx++;
            }

            char *L_end = ir_new_label();

            /* Dispatch on discriminant */
            IROperand discr = gen_expr(node->cond, list);
            for (int i = 0; i < count; i++) {
                ASTNode *c = cases[i];
                if (c->type == NODE_CASE && c->left) {
                    int v = c->left->int_val;
                    IROperand cv = ir_op_const(v);
                    ir_append(list, ir_make_if(discr, cv, IR_EQ, labels[i], line));
                }
            }

            if (default_index >= 0) {
                ir_append(list, ir_make_goto(labels[default_index], line));
            } else {
                ir_append(list, ir_make_goto(L_end, line));
            }

            if (discr.name) free(discr.name);

            /* Emit case bodies with fallthrough */
            push_break_label(L_end);
            for (int i = 0; i < count; i++) {
                ASTNode *c = cases[i];
                ir_append(list, ir_make_label(labels[i], c->line_number));
                if (c->type == NODE_CASE && c->body) {
                    for (ASTNode *s = c->body; s; s = s->next) {
                        gen_stmt(s, list);
                    }
                }
            }
            pop_break_label();

            ir_append(list, ir_make_label(L_end, line));

            for (int i = 0; i < count; i++) {
                free(labels[i]);
            }
            free(labels);
            free(cases);
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

        case NODE_BREAK: {
            const char *lbl = current_break_label();
            if (lbl) {
                ir_append(list, ir_make_goto((char *)lbl, line));
            }
            break;
        }

        case NODE_CONTINUE: {
            const char *lbl = current_continue_label();
            if (lbl) {
                ir_append(list, ir_make_goto((char *)lbl, line));
            }
            break;
        }

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
