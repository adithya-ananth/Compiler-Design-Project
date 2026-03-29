#ifndef IR_SCHED_H
#define IR_SCHED_H

#include "ir.h"
#include "ir_opt.h"

/**
 * Reorders instructions within each basic block of the function
 * to minimize execution stalls based on a simplified latency model.
 * It builds a dependency DAG and performs list scheduling.
 */
void ir_schedule_function(IRFunc *f);

#endif /* IR_SCHED_H */
