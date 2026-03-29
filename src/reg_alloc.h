/**
 * reg_alloc.h — Chaitin's Graph-Coloring Register Allocation
 *
 * Pipeline position: after IR optimization, before RISC-V code generation.
 *
 * Algorithm outline (per function):
 *   1. Build per-instruction liveness from the CFG liveness sets.
 *   2. Build interference graph: two variables interfere iff they are
 *      simultaneously live at any program point.
 *   3. Simplify: iteratively remove nodes with degree < K onto a stack.
 *      If stuck, choose a spill candidate and push it (potential spill).
 *   4. Select: pop stack, assign the first color not used by colored neighbours.
 *      If none available → actual spill.
 *   5. Spill rewrite: insert IR_LOAD/IR_STORE around each use/def of a
 *      spilled variable, then restart from step 1. Usually converges in ≤2 rounds.
 *   6. Export interference graph to a Graphviz DOT file for visualisation.
 */

#ifndef REG_ALLOC_H
#define REG_ALLOC_H

#include "ir.h"
#include "ir_opt.h"

/* -----------------------------------------------------------------------
 * Physical register pool (RISC-V)
 * -----------------------------------------------------------------------
 * Caller-saved (scratch): t0-t6  → 7 registers
 * Callee-saved           : s1-s11 → 11 registers  (s0 = frame pointer, off-limits)
 * Total colors K = 15
 */
#define RA_NUM_REGS 15
/* First callee-saved register index in RA_REG_NAMES (s1 starts here) */
#define RA_FIRST_CALLEE_SAVED 4

extern const char *RA_REG_NAMES[RA_NUM_REGS]; /* defined in reg_alloc.c */

/* -----------------------------------------------------------------------
 * Interference graph node
 * ----------------------------------------------------------------------- */
typedef struct IGNode {
    char  *name;          /* IR variable / temp name            */
    int    degree;        /* current number of neighbours        */
    int   *neighbours;    /* array of neighbour node indices     */
    int    nb_cap;        /* allocated capacity of neighbours[]  */

    /* Coloring result */
    int    color;         /* index into RA_REG_NAMES, or -1 if spilled */
    int    spilled;       /* 1 if this variable must be memory-resident */
    int    spill_offset;  /* frame offset for spill slot (relative to s0) */

    /* Bookkeeping during simplify/select */
    int    removed;       /* 1 if already pushed onto the simplify stack  */
    int    interferes_with_caller_saved; /* 1 if live across a call */
} IGNode;

/* -----------------------------------------------------------------------
 * Interference graph
 * ----------------------------------------------------------------------- */
typedef struct {
    IGNode *nodes;        /* array of nodes, one per unique variable       */
    int     count;        /* number of nodes                               */
    int     cap;          /* allocated capacity                            */
    char   *func_name;    /* owning function (for DOT labels)              */
} InterferenceGraph;

/* -----------------------------------------------------------------------
 * Result of register allocation for one function
 *
 * Maps each IR variable name to either:
 *   - a physical register string (e.g. "t2", "s3"), or
 *   - NULL if the variable was spilled (spill_offsets holds its frame offset)
 * ----------------------------------------------------------------------- */
typedef struct {
    char  *func_name;

    /* Parallel arrays indexed 0..var_count-1 */
    char **var_names;     /* IR variable name                              */
    int   *reg_index;     /* index into RA_REG_NAMES, or -1 if spilled    */
    int   *spill_offset;  /* frame offset (s0-relative) for spilled vars   */
    int    var_count;

    /* Which callee-saved registers were actually used (must save/restore) */
    int    callee_used[RA_NUM_REGS]; /* 1 if register i was assigned       */
} RegAllocResult;

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

/**
 * Run Chaitin's register allocation for every function in the IR program.
 * Returns a NULL-terminated array of RegAllocResult*, one per function.
 * Callers must free with reg_alloc_free_all().
 */
RegAllocResult **reg_alloc_program(IRProgram *prog);

/**
 * Look up the physical register name for a variable in one function's result.
 * Returns the register string (e.g. "t0") or NULL if the variable is spilled
 * or not found.
 */
const char *reg_alloc_lookup(RegAllocResult *res, const char *var_name);

/**
 * Look up the spill offset for a variable.
 * Returns the frame offset (negative integer, relative to s0), or 0 if not spilled.
 */
int reg_alloc_spill_offset(RegAllocResult *res, const char *var_name);

/**
 * Check whether a variable was spilled (i.e., has no physical register).
 * Returns 1 if spilled, 0 if register-allocated or not found.
 */
int reg_alloc_is_spilled(RegAllocResult *res, const char *var_name);

/**
 * Export the interference graph to a Graphviz DOT file.
 * Nodes are labelled with variable names and colored by assigned register;
 * spilled nodes are shown in red.
 */
void reg_alloc_export_dot(InterferenceGraph *ig, RegAllocResult *res,
                           const char *path);

/**
 * Free a NULL-terminated array of RegAllocResult* returned by reg_alloc_program().
 */
void reg_alloc_free_all(RegAllocResult **results);

#endif /* REG_ALLOC_H */
