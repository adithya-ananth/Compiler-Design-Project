#ifndef IR_OPT_H
#define IR_OPT_H

#include "ir.h"

/* Optimization level */
typedef enum {
    OPT_O0,
    OPT_O1,
    OPT_O2
} OptLevel;

/* Basic Block structure */
typedef struct BasicBlock {
    int id;
    IRInstr *instrs;    /* Pointer to first instruction in block */
    IRInstr *last;      /* Pointer to last instruction in block */
    
    /* Control Flow */
    struct BasicBlock **preds;
    int pred_count;
    struct BasicBlock **succs;
    int succ_count;

    /* Liveness analysis */
    char **live_in;
    int live_in_count;
    char **live_out;
    int live_out_count;

    /* Gen/Kill (Use/Def) for liveness */
    char **use;
    int use_count;
    char **def;
    int def_count;
    
    /* Dominators */
    int *doms; /* bitset of block IDs */

    struct BasicBlock *next; /* For linear list of blocks in function */
} BasicBlock;

/* Control Flow Graph for a function */
typedef struct CFG {
    char *func_name;
    BasicBlock *entry;
    BasicBlock *blocks;
    int block_count;
} CFG;

/* Main entry point for IR optimizations */
void optimize_program(IRProgram *prog);

/* CFG Lifecycle */
CFG* build_cfg(IRFunc *f);
void free_cfg(CFG *cfg);
IRInstr* flatten_cfg(CFG *cfg);

/* Liveness analysis (also used by register allocator) */
void compute_liveness(CFG *cfg);

/* Dead code / unreachable block elimination */
void eliminate_dead_code(CFG *cfg);
void eliminate_unreachable_blocks(CFG *cfg);

#endif /* IR_OPT_H */
