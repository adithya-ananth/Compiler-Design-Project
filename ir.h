#ifndef IR_H
#define IR_H

#include "symbol_table.h"

// IR instruction types
typedef enum {
    IR_ASSIGN,      
    IR_BINOP,       
    IR_UNOP,        
    IR_PARAM,       
    IR_CALL,        
    IR_RETURN,      
    IR_LABEL,       
    IR_GOTO,        
    IR_IF           
} IROpKind;

// Relational operators
typedef enum {
    IR_LT, IR_GT, IR_LE, IR_GE, IR_EQ, IR_NE
} IRRelop;

IRRelop ast_relop_to_ir(int ast_op);

// Var/temp or integer constant
typedef struct IROperand {
    char *name;     
    int const_val;  
    int is_const;   
} IROperand;

// Three-address instruction node
typedef struct IRInstr {
    IROpKind kind;
    int line;       

    char *result;   // For ASSIGN, BINOP, UNOP
    IROperand src;  // For ASSIGN

    IROperand left; // For BINOP
    IROperand right;
    int binop;      

    IROperand unop_src; // For UNOP
    int unop;       

    char *call_fn;  // For CALL
    int arg_count;

    char *label;    // For LABEL, GOTO, IF

    IROperand if_left; // For IF
    IROperand if_right;
    IRRelop relop;

    struct IRInstr *next;
} IRInstr;

// Function instruction list
typedef struct IRFunc {
    char *name;
    DataType ret_type;
    IRInstr *instrs;
    struct IRFunc *next;
} IRFunc;

// Full program IR
typedef struct {
    IRFunc *funcs;
    IRInstr *global_instrs;  
} IRProgram;

// Generators
char* ir_new_temp(void);
char* ir_new_label(void);
void ir_reset_temps(void);

// Instruction factories
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

// Helpers
IROperand ir_op_name(char *name);
IROperand ir_op_const(int val);

// List management
void ir_append(IRInstr **head, IRInstr *instr);
void ir_append_list(IRInstr **head, IRInstr *list);

// Program management
IRProgram* ir_program_create(void);
void ir_program_add_func(IRProgram *prog, IRFunc *f);
IRFunc* ir_func_create(char *name, DataType ret_type);

// Printing and Export
void ir_print_instr(IRInstr *instr);
void ir_print_func(IRFunc *f);
void ir_print_program(IRProgram *prog);
void ir_export_to_file(IRProgram *prog, const char *filename);

// Memory cleanup
void ir_free_operand(IROperand *op);
void ir_free_instr(IRInstr *instr);
void ir_free_func(IRFunc *f);
void ir_free_program(IRProgram *prog);

#endif