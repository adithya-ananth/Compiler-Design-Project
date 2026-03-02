/**
 * ir.c - Three-address code IR implementation
 * Temp/label generation, instruction creation, printing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "ast.h"
#include "y.tab.h"

static int temp_counter = 0;
static int label_counter = 0;

/* --- Temp and label generation --- */
char* ir_new_temp(void) {
    char *buf = malloc(16);
    snprintf(buf, 16, "t%d", temp_counter++);
    return buf;
}

char* ir_new_label(void) {
    char *buf = malloc(16);
    snprintf(buf, 16, "L%d", label_counter++);
    return buf;
}

void ir_reset_temps(void) {
    temp_counter = 0;
    label_counter = 0;
}

/* --- Operand helpers --- */
IROperand ir_op_name(char *name) {
    IROperand op = {0};
    op.name = name ? strdup(name) : NULL;
    op.is_const = 0;
    return op;
}

IROperand ir_op_const(int val) {
    IROperand op = {0};
    op.name = NULL;
    op.const_val = val;
    op.is_const = 1;
    return op;
}

/* Deep-copy operand for storage in instruction (avoids double-free) */
static IROperand op_dup(IROperand *op) {
    IROperand c = *op;
    if (op->name) c.name = strdup(op->name);
    return c;
}

/* Map AST binop token to IR relop for conditional jumps */
IRRelop ast_relop_to_ir(int ast_op) {
    switch (ast_op) {
        case '<':  return IR_LT;
        case '>':  return IR_GT;
        case T_LE: return IR_LE;
        case T_GE: return IR_GE;
        case T_EQ: return IR_EQ;
        case T_NEQ: return IR_NE;
        default:   return IR_EQ;
    }
}

/* --- Instruction creation --- */
IRInstr* ir_make_assign(char *dst, IROperand src, int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_ASSIGN;
    i->line = line;
    i->result = dst ? strdup(dst) : NULL;
    i->src = op_dup(&src);
    return i;
}

IRInstr* ir_make_binop(char *dst, IROperand left, IROperand right, int op, int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_BINOP;
    i->line = line;
    i->result = dst ? strdup(dst) : NULL;
    i->left = op_dup(&left);
    i->right = op_dup(&right);
    i->binop = op;
    return i;
}

IRInstr* ir_make_unop(char *dst, IROperand src, int op, int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_UNOP;
    i->line = line;
    i->result = dst ? strdup(dst) : NULL;
    i->unop_src = op_dup(&src);
    i->unop = op;
    return i;
}

IRInstr* ir_make_param(IROperand op, int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_PARAM;
    i->line = line;
    i->src = op_dup(&op);
    return i;
}

IRInstr* ir_make_call(char *dst, char *fn, int nargs, int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_CALL;
    i->line = line;
    i->result = dst ? strdup(dst) : NULL;
    i->call_fn = fn ? strdup(fn) : NULL;
    i->arg_count = nargs;
    return i;
}

IRInstr* ir_make_call_void(char *fn, int nargs, int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_CALL;
    i->line = line;
    i->result = NULL;
    i->call_fn = fn ? strdup(fn) : NULL;
    i->arg_count = nargs;
    return i;
}

IRInstr* ir_make_return_val(IROperand op, int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_RETURN;
    i->line = line;
    i->src = op_dup(&op);
    return i;
}

IRInstr* ir_make_return(int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_RETURN;
    i->line = line;
    i->src.is_const = 0;
    i->src.name = NULL;
    return i;
}

IRInstr* ir_make_label(char *label, int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_LABEL;
    i->line = line;
    i->label = label ? strdup(label) : NULL;
    return i;
}

IRInstr* ir_make_goto(char *label, int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_GOTO;
    i->line = line;
    i->label = label ? strdup(label) : NULL;
    return i;
}

IRInstr* ir_make_if(IROperand left, IROperand right, IRRelop relop, char *label, int line) {
    IRInstr *i = calloc(1, sizeof(IRInstr));
    i->kind = IR_IF;
    i->line = line;
    i->if_left = op_dup(&left);
    i->if_right = op_dup(&right);
    i->relop = relop;
    i->label = label ? strdup(label) : NULL;
    return i;
}

/* --- List management --- */
void ir_append(IRInstr **head, IRInstr *instr) {
    if (!instr) return;
    instr->next = NULL;
    if (!*head) {
        *head = instr;
        return;
    }
    IRInstr *p = *head;
    while (p->next) p = p->next;
    p->next = instr;
}

void ir_append_list(IRInstr **head, IRInstr *list) {
    if (!list) return;
    IRInstr *p = list;
    while (p->next) p = p->next;
    p->next = *head;
    *head = list;
}

/* --- Program --- */
IRProgram* ir_program_create(void) {
    return calloc(1, sizeof(IRProgram));
}

void ir_program_add_func(IRProgram *prog, IRFunc *f) {
    if (!prog || !f) return;
    f->next = prog->funcs;
    prog->funcs = f;
}

IRFunc* ir_func_create(char *name, DataType ret_type) {
    IRFunc *f = calloc(1, sizeof(IRFunc));
    f->name = name ? strdup(name) : NULL;
    f->ret_type = ret_type;
    f->instrs = NULL;
    return f;
}

/* --- Print operand --- */
static void print_operand(IROperand *op) {
    if (op->is_const)
        printf("%d", op->const_val);
    else if (op->name)
        printf("%s", op->name);
    else
        printf("?");
}

static const char* binop_str(int op) {
    switch (op) {
        case '+': return "+";
        case '-': return "-";
        case '*': return "*";
        case '/': return "/";
        case '%': return "%";
        case '<': return "<";
        case '>': return ">";
        case T_EQ: return "==";
        case T_NEQ: return "!=";
        case T_LE: return "<=";
        case T_GE: return ">=";
        case T_AND: return "&&";
        case T_OR: return "||";
        default: return "?";
    }
}

static const char* relop_str(IRRelop r) {
    switch (r) {
        case IR_LT: return "<";
        case IR_GT: return ">";
        case IR_LE: return "<=";
        case IR_GE: return ">=";
        case IR_EQ: return "==";
        case IR_NE: return "!=";
        default: return "?";
    }
}

/* --- Output --- */
void ir_print_instr(IRInstr *instr) {
    if (!instr) return;
    switch (instr->kind) {
        case IR_ASSIGN:
            printf("  %s := ", instr->result);
            print_operand(&instr->src);
            printf("\n");
            break;
        case IR_BINOP:
            printf("  %s := ", instr->result);
            print_operand(&instr->left);
            printf(" %s ", binop_str(instr->binop));
            print_operand(&instr->right);
            printf("\n");
            break;
        case IR_UNOP:
            printf("  %s := %c", instr->result, instr->unop);
            print_operand(&instr->unop_src);
            printf("\n");
            break;
        case IR_PARAM:
            printf("  param ");
            print_operand(&instr->src);
            printf("\n");
            break;
        case IR_CALL:
            if (instr->result)
                printf("  %s := call %s, %d\n", instr->result, instr->call_fn, instr->arg_count);
            else
                printf("  call %s, %d\n", instr->call_fn, instr->arg_count);
            break;
        case IR_RETURN:
            if (instr->src.name || instr->src.is_const) {
                printf("  return ");
                print_operand(&instr->src);
                printf("\n");
            } else
                printf("  return\n");
            break;
        case IR_LABEL:
            printf("%s:\n", instr->label);
            break;
        case IR_GOTO:
            printf("  goto %s\n", instr->label);
            break;
        case IR_IF:
            printf("  if ");
            print_operand(&instr->if_left);
            printf(" %s ", relop_str(instr->relop));
            print_operand(&instr->if_right);
            printf(" goto %s\n", instr->label);
            break;
    }
}

void ir_print_func(IRFunc *f) {
    if (!f) return;
    printf("function %s:\n", f->name);
    for (IRInstr *i = f->instrs; i; i = i->next)
        ir_print_instr(i);
    printf("\n");
}

void ir_print_program(IRProgram *prog) {
    if (!prog) return;
    printf("\n=========== IR (Three-Address Code) ===========\n");
    for (IRFunc *f = prog->funcs; f; f = f->next)
        ir_print_func(f);
    printf("===============================================\n");
}

void ir_export_to_file(IRProgram *prog, const char *filename) {
    if (!prog || !filename) return;
    FILE *f = fopen(filename, "w");
    if (!f) return;
    for (IRFunc *fn = prog->funcs; fn; fn = fn->next) {
        fprintf(f, "function %s:\n", fn->name);
        for (IRInstr *i = fn->instrs; i; i = i->next) {
            switch (i->kind) {
                case IR_ASSIGN:
                    fprintf(f, "  %s := ", i->result);
                    if (i->src.is_const) fprintf(f, "%d", i->src.const_val);
                    else fprintf(f, "%s", i->src.name);
                    fprintf(f, "\n");
                    break;
                case IR_BINOP:
                    fprintf(f, "  %s := ", i->result);
                    if (i->left.is_const) fprintf(f, "%d", i->left.const_val);
                    else fprintf(f, "%s", i->left.name);
                    fprintf(f, " %s ", binop_str(i->binop));
                    if (i->right.is_const) fprintf(f, "%d", i->right.const_val);
                    else fprintf(f, "%s", i->right.name);
                    fprintf(f, "\n");
                    break;
                case IR_UNOP:
                    fprintf(f, "  %s := %c", i->result, i->unop);
                    if (i->unop_src.is_const) fprintf(f, "%d", i->unop_src.const_val);
                    else fprintf(f, "%s", i->unop_src.name);
                    fprintf(f, "\n");
                    break;
                case IR_PARAM:
                    fprintf(f, "  param ");
                    if (i->src.is_const) fprintf(f, "%d", i->src.const_val);
                    else fprintf(f, "%s", i->src.name);
                    fprintf(f, "\n");
                    break;
                case IR_CALL:
                    if (i->result)
                        fprintf(f, "  %s := call %s, %d\n", i->result, i->call_fn, i->arg_count);
                    else
                        fprintf(f, "  call %s, %d\n", i->call_fn, i->arg_count);
                    break;
                case IR_RETURN:
                    if (i->src.name || i->src.is_const) {
                        fprintf(f, "  return ");
                        if (i->src.is_const) fprintf(f, "%d", i->src.const_val);
                        else fprintf(f, "%s", i->src.name);
                        fprintf(f, "\n");
                    } else
                        fprintf(f, "  return\n");
                    break;
                case IR_LABEL:
                    fprintf(f, "%s:\n", i->label);
                    break;
                case IR_GOTO:
                    fprintf(f, "  goto %s\n", i->label);
                    break;
                case IR_IF:
                    fprintf(f, "  if ");
                    if (i->if_left.is_const) fprintf(f, "%d", i->if_left.const_val);
                    else fprintf(f, "%s", i->if_left.name);
                    fprintf(f, " %s ", relop_str(i->relop));
                    if (i->if_right.is_const) fprintf(f, "%d", i->if_right.const_val);
                    else fprintf(f, "%s", i->if_right.name);
                    fprintf(f, " goto %s\n", i->label);
                    break;
            }
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

/* --- Cleanup --- */
void ir_free_operand(IROperand *op) {
    if (op->name) free(op->name);
}

void ir_free_instr(IRInstr *instr) {
    if (!instr) return;
    switch (instr->kind) {
        case IR_ASSIGN:
            if (instr->result) free(instr->result);
            if (instr->src.name) free(instr->src.name);
            break;
        case IR_BINOP:
            if (instr->result) free(instr->result);
            if (instr->left.name) free(instr->left.name);
            if (instr->right.name) free(instr->right.name);
            break;
        case IR_UNOP:
            if (instr->result) free(instr->result);
            if (instr->unop_src.name) free(instr->unop_src.name);
            break;
        case IR_PARAM:
            if (instr->src.name) free(instr->src.name);
            break;
        case IR_CALL:
            if (instr->result) free(instr->result);
            if (instr->call_fn) free(instr->call_fn);
            break;
        case IR_RETURN:
            if (instr->src.name) free(instr->src.name);
            break;
        case IR_LABEL:
        case IR_GOTO:
        case IR_IF:
            if (instr->label) free(instr->label);
            if (instr->kind == IR_IF) {
                if (instr->if_left.name) free(instr->if_left.name);
                if (instr->if_right.name) free(instr->if_right.name);
            }
            break;
    }
    ir_free_instr(instr->next);
    free(instr);
}

void ir_free_func(IRFunc *f) {
    if (!f) return;
    if (f->name) free(f->name);
    ir_free_instr(f->instrs);
    ir_free_func(f->next);
    free(f);
}

void ir_free_program(IRProgram *prog) {
    if (!prog) return;
    ir_free_func(prog->funcs);
    ir_free_instr(prog->global_instrs);
    free(prog);
}
