#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "riscv_gen.h"

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
        fprintf(out, "%s:\n", func->name);

        // --- PROLOGUE ---
        fprintf(out, "  # --- Prologue ---\n");
        fprintf(out, "  addi sp, sp, -32\n"); // Allocate stack space
        fprintf(out, "  sw ra, 28(sp)\n");    // Save return address
        fprintf(out, "  sw s0, 24(sp)\n");    // Save frame pointer
        fprintf(out, "  addi s0, sp, 32\n");  // Set new frame pointer
        fprintf(out, "\n");

        // --- FUNCTION BODY ---
        IRInstr *instr = func->instrs;
        while (instr) {
            fprintf(out, "  # Line %d\n", instr->line);
            switch (instr->kind) {
                case IR_ASSIGN:
                    fprintf(out, "  # TODO: Implement ASSIGN (%s = ...)\n", instr->result);
                    break;
                case IR_BINOP:
                    fprintf(out, "  # TODO: Implement BINOP (%s = ... %c ...)\n", instr->result, instr->binop);
                    break;
                case IR_IF:
                    fprintf(out, "  # TODO: Implement IF branch to %s\n", instr->label);
                    break;
                case IR_GOTO:
                    fprintf(out, "  j %s\n", instr->label);
                    break;
                case IR_LABEL:
                    fprintf(out, "%s:\n", instr->label);
                    break;
                case IR_RETURN:
                    fprintf(out, "  # TODO: Load return value into a0\n");
                    // --- EPILOGUE (for early returns) ---
                    fprintf(out, "  # --- Epilogue ---\n");
                    fprintf(out, "  lw ra, 28(sp)\n");
                    fprintf(out, "  lw s0, 24(sp)\n");
                    fprintf(out, "  addi sp, sp, 32\n");
                    fprintf(out, "  jr ra\n");
                    break;
                default:
                    break;
            }
            instr = instr->next;
        }

        // --- DEFAULT EPILOGUE ---
        fprintf(out, "\n  # --- Default Epilogue ---\n");
        fprintf(out, "  lw ra, 28(sp)\n");
        fprintf(out, "  lw s0, 24(sp)\n");
        fprintf(out, "  addi sp, sp, 32\n");
        fprintf(out, "  jr ra\n\n");

        func = func->next;
    }

    fclose(out);
    printf("RISC-V assembly successfully written to %s\n", filename);
}