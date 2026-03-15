#ifndef RISCV_GEN_H
#define RISCV_GEN_H

#include "ir.h"

/* Generate RISC-V assembly from the IR program and write it to a file */
void riscv_generate(IRProgram *prog, const char *filename);

#endif /* RISCV_GEN_H */