#ifndef RISCV_GEN_H
#define RISCV_GEN_H

#include "ir.h"
#include "reg_alloc.h"

/*
 * Generate RISC-V assembly from the IR program.
 * ra_results: NULL-terminated array of RegAllocResult*, one per function
 *             (from reg_alloc_program).  Pass NULL to use the old stack-only mode.
 */
void riscv_generate(IRProgram *prog, RegAllocResult **ra_results, const char *filename);

#endif /* RISCV_GEN_H */