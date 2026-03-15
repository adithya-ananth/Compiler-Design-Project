#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "symbol_table.h"  // CRITICAL: Used to fetch array & struct sizes!
#include "riscv_gen.h"

// --- Dynamic Stack Memory Tracker ---
typedef struct {
    char name[64];
    int offset;
} VarOffset;

static VarOffset var_offsets[256];
static int var_count = 0;
static int current_sp_offset = -32;
static int param_idx = 0; // Tracks a0-a7 registers for function arguments

static void reset_offsets() {
    var_count = 0;
    current_sp_offset = -32; 
    param_idx = 0;
}

static int get_offset(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_offsets[i].name, name) == 0) return var_offsets[i].offset;
    }
    
    // Ask the symbol table how big this variable is to prevent memory overwrites!
    Symbol *sym = lookup((char *)name);
    int size = 4; // Default to 4 bytes (1 word)
    
    if (sym && sym->array_dim_count > 0) {
        size = 4;
        for(int i = 0; i < sym->array_dim_count; i++) {
            if (sym->array_sizes[i] > 0) size *= sym->array_sizes[i];
        }
    } else if (sym && sym->type == TYPE_STRUCT && sym->struct_size > 0) {
        size = sym->struct_size;
    }

    current_sp_offset -= size; 
    strcpy(var_offsets[var_count].name, name);
    var_offsets[var_count].offset = current_sp_offset;
    return var_offsets[var_count++].offset;
}

// Helper: Load a value into a register
static void load_operand(FILE *out, IROperand op, const char *reg) {
    if (op.is_const) {
        fprintf(out, "  li %s, %d\n", reg, op.const_val);
    } else if (op.name) {
        int off = get_offset(op.name);
        fprintf(out, "  lw %s, %d(s0)\n", reg, off);
    }
}

// Helper: Load an absolute memory address (for arrays/pointers)
static void load_address(FILE *out, IROperand op, const char *reg) {
    if (op.name) {
        int off = get_offset(op.name);
        fprintf(out, "  addi %s, s0, %d\n", reg, off);
    }
}

// --- Main Generator ---
void riscv_generate(IRProgram *prog, const char *filename) {
    FILE *out = fopen(filename, "w");
    if (!out) {
        perror("Failed to open output file");
        return;
    }

    fprintf(out, "  .text\n");
    fprintf(out, "  .globl main\n\n");

    IRFunc *func = prog->funcs;
    while (func) {
        reset_offsets(); 
        fprintf(out, "%s:\n", func->name);

        // --- PROLOGUE ---
        fprintf(out, "  # --- Prologue ---\n");
        fprintf(out, "  addi sp, sp, -2048\n"); // Massive stack space to fit 2D matrices
        fprintf(out, "  sw ra, 2044(sp)\n");    
        fprintf(out, "  sw s0, 2040(sp)\n");    
        fprintf(out, "  addi s0, sp, 2048\n\n");  

        // --- FUNCTION BODY ---
        IRInstr *instr = func->instrs;
        while (instr) {
            fprintf(out, "  # Line %d: ", instr->line);
            switch (instr->kind) {
                
                // --- BASIC OPERATIONS ---
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
                    load_operand(out, instr->left, "t0");
                    if (instr->binop == '-') fprintf(out, "  neg t1, t0\n");
                    else if (instr->binop == '!') fprintf(out, "  seqz t1, t0\n"); // Logical NOT
                    else if (instr->binop == '&') load_address(out, instr->left, "t1"); // Address-Of
                    else fprintf(out, "  mv t1, t0\n"); 
                    fprintf(out, "  sw t1, %d(s0)\n", get_offset(instr->result));
                    break;

                // --- CONTROL FLOW ---
                case IR_IF:
                    fprintf(out, "if (...) goto %s\n", instr->label);
                    load_operand(out, instr->left, "t0");
                    load_operand(out, instr->right, "t1");
                    
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
                    
                // --- ARRAYS AND POINTERS ---
                case IR_LOAD: // t0 = arr[index]
                    fprintf(out, "Load Array/Pointer\n");
                    load_address(out, instr->left, "t0");  // Base address
                    load_operand(out, instr->right, "t1"); // Scaled index offset
                    fprintf(out, "  add t2, t0, t1\n");    // Exact memory address
                    fprintf(out, "  lw t3, 0(t2)\n");      // Dereference
                    fprintf(out, "  sw t3, %d(s0)\n", get_offset(instr->result));
                    break;

                case IR_STORE: // arr[index] = val
                    fprintf(out, "Store Array/Pointer\n");
                    load_address(out, instr->left, "t0");  
                    load_operand(out, instr->right, "t1"); 
                    load_operand(out, instr->src, "t2");   // Value to write
                    fprintf(out, "  add t3, t0, t1\n");    
                    fprintf(out, "  sw t2, 0(t3)\n");      
                    break;

                // --- FUNCTIONS AND POLYMORPHISM ---
                case IR_PARAM: 
                    fprintf(out, "Param\n");
                    load_operand(out, instr->src, "t0");
                    fprintf(out, "  mv a%d, t0\n", param_idx++); // Load arg into a0, a1...
                    break;

                case IR_CALL: // Direct Call
                    fprintf(out, "Call %s\n", instr->label);
                    fprintf(out, "  call %s\n", instr->label);
                    if (instr->result && strlen(instr->result) > 0) {
                        fprintf(out, "  sw a0, %d(s0)\n", get_offset(instr->result)); // Save return val
                    }
                    param_idx = 0; // Reset arg counter
                    break;

                case IR_CALL_INDIRECT: // VIRTUAL FUNCTION DISPATCH!
                    fprintf(out, "Indirect Call (Polymorphism!)\n");
                    load_operand(out, instr->left, "t0"); // Load function pointer address
                    fprintf(out, "  jalr ra, t0, 0\n");   // Jump to dynamic address
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

        // --- DEFAULT EPILOGUE ---
        fprintf(out, "\n  # --- Default Epilogue ---\n");
        fprintf(out, "  lw ra, 2044(sp)\n");
        fprintf(out, "  lw s0, 2040(sp)\n");
        fprintf(out, "  addi sp, sp, 2048\n");
        fprintf(out, "  jr ra\n\n");

        func = func->next;
    }

    fclose(out);
    printf("RISC-V assembly successfully written to %s\n", filename);
}