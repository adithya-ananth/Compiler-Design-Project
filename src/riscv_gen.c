#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "symbol_table.h"  
#include "riscv_gen.h"

// --- Dynamic Temp Tracker ---
typedef struct {
    char name[64];
    int offset;
} VarOffset;

static VarOffset var_offsets[256];
static int var_count = 0;
static int current_temp_offset = -1024; // Start temporary variables far below local variables
static int param_idx = 0;

static void reset_offsets() {
    var_count = 0;
    current_temp_offset = -1024; 
    param_idx = 0;
}

static int get_offset(const char *name) {
    Symbol *sym = lookup((char *)name);
    // If it's a real variable, TRUST the Semantic Analyzer's offset layout!
    if (sym && sym->kind != SYM_FUNCTION && sym->kind != SYM_STRUCT) {
        return sym->frame_offset; 
    }
    
    // Otherwise, it's a temporary intermediate variable (e.g., t0, t1), allocate dynamically
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_offsets[i].name, name) == 0) return var_offsets[i].offset;
    }

    current_temp_offset -= 4; 
    strcpy(var_offsets[var_count].name, name);
    var_offsets[var_count].offset = current_temp_offset;
    return var_offsets[var_count++].offset;
}

static void load_operand(FILE *out, IROperand op, const char *reg) {
    if (op.is_const) {
        fprintf(out, "  li %s, %d\n", reg, op.const_val);
    } else if (op.name) {
        if (strncmp(op.name, "vtable_", 7) == 0) {
            fprintf(out, "  la %s, %s\n", reg, op.name); // Load global vtable address
        } else {
            int off = get_offset(op.name);
            fprintf(out, "  lw %s, %d(s0)\n", reg, off);
        }
    }
}

static void load_address(FILE *out, IROperand op, const char *reg) {
    if (op.name) {
        if (strncmp(op.name, "vtable_", 7) == 0) {
            fprintf(out, "  la %s, %s\n", reg, op.name);
            return;
        }
        Symbol *sym = lookup((char *)op.name);
        int off = get_offset(op.name);
        
        // Pointers & Parameters HOLD addresses. Local Arrays/Structs ARE addresses.
        if (sym && (sym->pointer_level > 0 || sym->kind == SYM_PARAMETER)) {
            fprintf(out, "  lw %s, %d(s0)\n", reg, off);
        } else {
            fprintf(out, "  addi %s, s0, %d\n", reg, off);
        }
    }
}

void riscv_generate(IRProgram *prog, const char *filename) {
    FILE *out = fopen(filename, "w");
    if (!out) return;

    fprintf(out, "  .text\n");
    fprintf(out, "  .globl main\n\n");

    IRFunc *func = prog->funcs;
    while (func) {
        reset_offsets(); 
        fprintf(out, "%s:\n", func->name);

        // --- PROLOGUE ---
        fprintf(out, "  # --- Prologue ---\n");
        fprintf(out, "  addi sp, sp, -2048\n"); 
        fprintf(out, "  sw ra, 2044(sp)\n");    
        fprintf(out, "  sw s0, 2040(sp)\n");    
        fprintf(out, "  addi s0, sp, 2048\n\n");  

        IRInstr *instr = func->instrs;
        while (instr) {
            fprintf(out, "  # Line %d: ", instr->line);
            switch (instr->kind) {
                
                case IR_ASSIGN:
                    fprintf(out, "%s = ...\n", instr->result);
                    load_operand(out, instr->src, "t0");
                    fprintf(out, "  sw t0, %d(s0)\n", get_offset(instr->result));
                    break;

                case IR_BINOP:
                    fprintf(out, "%s = ... %c ...\n", instr->result, instr->binop);
                    load_operand(out, instr->left, "t0");
                    load_operand(out, instr->right, "t1");
                    
                    if (instr->binop == '+')      fprintf(out, "  add t2, t0, t1\n");
                    else if (instr->binop == '-') fprintf(out, "  sub t2, t0, t1\n");
                    else if (instr->binop == '*') fprintf(out, "  mul t2, t0, t1\n");
                    else if (instr->binop == '/') fprintf(out, "  div t2, t0, t1\n");
                    
                    fprintf(out, "  sw t2, %d(s0)\n", get_offset(instr->result));
                    break;

                case IR_UNOP:
                    fprintf(out, "%s = UnOp ...\n", instr->result);
                    if (instr->unop == '&') {
                        load_address(out, instr->unop_src, "t1"); 
                    } else {
                        load_operand(out, instr->unop_src, "t0");
                        if (instr->unop == '-') fprintf(out, "  neg t1, t0\n");
                        else if (instr->unop == '!') fprintf(out, "  seqz t1, t0\n"); 
                        else fprintf(out, "  mv t1, t0\n"); 
                    }
                    fprintf(out, "  sw t1, %d(s0)\n", get_offset(instr->result));
                    break;

                case IR_IF:
                    fprintf(out, "if (...) goto %s\n", instr->label);
                    load_operand(out, instr->if_left, "t0");
                    load_operand(out, instr->if_right, "t1");
                    
                    switch (instr->relop) {
                        case IR_EQ: fprintf(out, "  beq t0, t1, %s\n", instr->label); break;
                        case IR_NE: fprintf(out, "  bne t0, t1, %s\n", instr->label); break;
                        case IR_LT: fprintf(out, "  blt t0, t1, %s\n", instr->label); break;
                        case IR_GT: fprintf(out, "  bgt t0, t1, %s\n", instr->label); break;
                        case IR_LE: fprintf(out, "  ble t0, t1, %s\n", instr->label); break;
                        case IR_GE: fprintf(out, "  bge t0, t1, %s\n", instr->label); break;
                        default: break;
                    }
                    break;

                case IR_GOTO:
                    fprintf(out, "  j %s\n", instr->label);
                    break;

                case IR_LABEL:
                    fprintf(out, "%s:\n", instr->label);
                    break;
                    
                case IR_LOAD: 
                    fprintf(out, "Load Array/Pointer\n");
                    load_address(out, instr->base, "t0");  
                    load_operand(out, instr->index, "t1"); 
                    if (instr->scale > 1) {
                        fprintf(out, "  li t4, %d\n  mul t1, t1, t4\n", instr->scale);
                    }
                    fprintf(out, "  add t2, t0, t1\n");    
                    fprintf(out, "  lw t3, 0(t2)\n");      
                    fprintf(out, "  sw t3, %d(s0)\n", get_offset(instr->result));
                    break;

                case IR_STORE: 
                    fprintf(out, "Store Array/Pointer\n");
                    load_address(out, instr->base, "t0");  
                    load_operand(out, instr->index, "t1"); 
                    if (instr->scale > 1) {
                        fprintf(out, "  li t4, %d\n  mul t1, t1, t4\n", instr->scale);
                    }
                    load_operand(out, instr->store_val, "t2");   
                    fprintf(out, "  add t3, t0, t1\n");    
                    fprintf(out, "  sw t2, 0(t3)\n");      
                    break;

                case IR_PARAM: 
                    fprintf(out, "Param\n");
                    load_operand(out, instr->src, "t0");
                    fprintf(out, "  mv a%d, t0\n", param_idx++); 
                    break;

                case IR_CALL: 
                    fprintf(out, "Call %s\n", instr->call_fn);
                    fprintf(out, "  call %s\n", instr->call_fn);
                    if (instr->result && strlen(instr->result) > 0) {
                        fprintf(out, "  sw a0, %d(s0)\n", get_offset(instr->result)); 
                    }
                    param_idx = 0; 
                    break;

                case IR_CALL_INDIRECT: 
                    fprintf(out, "Indirect Call (Polymorphism!)\n");
                    load_operand(out, instr->base, "t0"); 
                    fprintf(out, "  jalr ra, t0, 0\n");   
                    if (instr->result && strlen(instr->result) > 0) {
                        fprintf(out, "  sw a0, %d(s0)\n", get_offset(instr->result));
                    }
                    param_idx = 0; 
                    break;

                case IR_RETURN:
                    fprintf(out, "return\n");
                    if (instr->src.name || instr->src.is_const) {
                        load_operand(out, instr->src, "a0"); 
                    }
                    fprintf(out, "  lw ra, 2044(sp)\n");
                    fprintf(out, "  lw s0, 2040(sp)\n");
                    fprintf(out, "  addi sp, sp, 2048\n");
                    fprintf(out, "  jr ra\n");
                    break;

                default:
                    fprintf(out, "  # Unimplemented IR instruction\n");
                    break;
            }
            instr = instr->next;
        }

        fprintf(out, "\n  # --- Default Epilogue ---\n");
        fprintf(out, "  lw ra, 2044(sp)\n");
        fprintf(out, "  lw s0, 2040(sp)\n");
        fprintf(out, "  addi sp, sp, 2048\n");
        fprintf(out, "  jr ra\n\n");

        func = func->next;
    }
    fclose(out);
}