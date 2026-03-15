#ifndef IR_OPT_H
#define IR_OPT_H

#include "ir.h"

/* Run all optimization passes on the IR program */
void ir_optimize(IRProgram *prog);

#endif /* IR_OPT_H */