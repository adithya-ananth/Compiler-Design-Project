/**
 * ir.h - Three-address code IR for C-subset compiler
 * Week 4: Intermediate Representation (machine-independent)
 *
 * Format: each instruction has at most 3 addresses (result, op1, op2).
 * Supports arithmetic, control flow, and function calls.
 */

#ifndef IR_H
#define IR_H

#include "symbol_table.h"

/* --- IR instruction kinds --- */
typedef enum {
    IR_ASSIGN,      /* x := y (copy) */
    IR_BINOP,       /* x := y op z */
    IR_UNOP,        /* x := op y */
    IR_PARAM,       /* param x */
    IR_CALL,        /* x := call fn, n  (or call fn, n for void) */
    IR_RETURN,      /* return x  (or return for void) */
    IR_LABEL,       /* L1: */
    IR_GOTO,        /* goto L1 */
    IR_IF           /* if x relop y goto L1 */
} IROpKind;

/* Relational operators for IR_IF */
typedef enum {
    IR_LT, IR_GT, IR_LE, IR_GE, IR_EQ, IR_NE
} IRRelop;

/* Map AST binop token to IRRelop (for IR_IF) */
IRRelop ast_relop_to_ir(int ast_op);

/* Operand: either a named value (var/temp) or integer constant */
typedef struct IROperand {
    char *name;     /* variable or temp name (e.g. "x", "t1") */
    int const_val;  /* if name is NULL, this holds integer literal */
    int is_const;   /* 1 if operand is constant */
} IROperand;

/* Single three-address instruction */
typedef struct IRInstr {
    IROpKind kind;
    int line;       /* source line for debugging */

    /* For IR_ASSIGN, IR_BINOP, IR_UNOP: result location */
    char *result;

    /* For IR_ASSIGN: source */
    IROperand src;

    /* For IR_BINOP: left, right, operator token */
    IROperand left;
    IROperand right;
    int binop;      /* '+', '-', '*', '/', '%', T_AND, T_OR, etc. */

    /* For IR_UNOP: operand and operator */
    IROperand unop_src;
    int unop;       /* '-', '!' */

    /* For IR_CALL: function name, arg count */
    char *call_fn;
    int arg_count;

    /* For IR_LABEL, IR_GOTO, IR_IF: label */
    char *label;

    /* For IR_IF: condition operands and relop */
    IROperand if_left;
    IROperand if_right;
    IRRelop relop;

    struct IRInstr *next;
} IRInstr;

/* Function-level IR: list of instructions with function name */
typedef struct IRFunc {
    char *name;
    DataType ret_type;
    IRInstr *instrs;
    struct IRFunc *next;
} IRFunc;

/* Program IR: list of functions (incl. global decls as init code) */
typedef struct {
    IRFunc *funcs;
    IRInstr *global_instrs;  /* global var initializers, if any */
} IRProgram;

/* --- Temp and label generation --- */
char* ir_new_temp(void);
char* ir_new_label(void);
void ir_reset_temps(void);

/* --- Instruction creation --- */
IRInstr* ir_make_assign(char *dst, IROperand src, int line);
IRInstr* ir_make_binop(char *dst, IROperand left, IROperand right, int op, int line);
IRInstr* ir_make_unop(char *dst, IROperand src, int op, int line);
IRInstr* ir_make_param(IROperand op, int line);
IRInstr* ir_make_call(char *dst, char *fn, int nargs, int line);
IRInstr* ir_make_call_void(char *fn, int nargs, int line);
IRInstr* ir_make_return_val(IROperand op, int line);
IRInstr* ir_make_return(int line);
IRInstr* ir_make_label(char *label, int line);
IRInstr* ir_make_goto(char *label, int line);
IRInstr* ir_make_if(IROperand left, IROperand right, IRRelop relop, char *label, int line);

/* --- Operand helpers --- */
IROperand ir_op_name(char *name);
IROperand ir_op_const(int val);

/* --- List management --- */
void ir_append(IRInstr **head, IRInstr *instr);
void ir_append_list(IRInstr **head, IRInstr *list);

/* --- Program --- */
IRProgram* ir_program_create(void);
void ir_program_add_func(IRProgram *prog, IRFunc *f);
IRFunc* ir_func_create(char *name, DataType ret_type);

/* --- Output --- */
void ir_print_instr(IRInstr *instr);
void ir_print_func(IRFunc *f);
void ir_print_program(IRProgram *prog);
void ir_export_to_file(IRProgram *prog, const char *filename);

/* --- Cleanup --- */
void ir_free_operand(IROperand *op);
void ir_free_instr(IRInstr *instr);
void ir_free_func(IRFunc *f);
void ir_free_program(IRProgram *prog);

#endif /* IR_H */
