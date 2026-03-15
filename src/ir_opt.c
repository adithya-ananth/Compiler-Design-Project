#include <stdio.h>
#include <stdlib.h>
#include "ir.h"
#include "ir_opt.h"

void ir_optimize_func(IRFunc *func) {
    IRInstr *instr = func->instrs;
    int folded_count = 0;

    while (instr) {
        if (instr->kind == IR_BINOP) {
            if (instr->left.is_const && instr->right.is_const) {
                int l = instr->left.const_val;
                int r = instr->right.const_val;
                int res = 0;
                int can_fold = 1;

                switch (instr->binop) {
                    case '+': res = l + r; break;
                    case '-': res = l - r; break;
                    case '*': res = l * r; break;
                    case '/': 
                        if (r != 0) res = l / r; 
                        else can_fold = 0; 
                        break;
                    default: 
                        can_fold = 0; 
                        break; 
                }

                if (can_fold) {
                    instr->kind = IR_ASSIGN;
                    instr->src.is_const = 1;
                    instr->src.const_val = res;
                    
                    if (instr->src.name) free(instr->src.name);
                    instr->src.name = NULL;
                    folded_count++;
                }
            }
        }
        instr = instr->next;
    }

    if (folded_count > 0) {
        printf("Optimized '%s': Folded %d constant expressions.\n", func->name, folded_count);
    }
}

void ir_optimize(IRProgram *prog) {
    printf("Running IR Optimizations...\n");
    IRFunc *func = prog->funcs;
    while (func) {
        ir_optimize_func(func);
        func = func->next;
    }
}