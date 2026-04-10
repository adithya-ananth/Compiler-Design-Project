/**
 * reg_alloc.c — Chaitin's Graph-Coloring Register Allocation
 *
 * Implements the full Chaitin-Briggs style register allocator:
 *   Phase 1: Compute per-instruction liveness (backward pass over each BB).
 *   Phase 2: Build interference graph.
 *   Phase 3: Simplify — push low-degree nodes; choose spill candidates when stuck.
 *   Phase 4: Select — pop stack, assign colors; mark actual spills.
 *   Phase 5: Spill rewrite — insert IR loads/stores for spilled variables.
 *            (Iterates: phase 1→5 until no new spills appear.)
 *   Phase 6: Export interference graph to Graphviz DOT file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reg_alloc.h"
#include "ir_opt.h"
#include "ir_sched.h"

/* -----------------------------------------------------------------------
 * Physical register table
 * Caller-saved first (t0-t6), then callee-saved (s1-s11).
 * s0 is the frame pointer — never allocated.
 * ----------------------------------------------------------------------- */
const char *RA_REG_NAMES[RA_NUM_REGS] = {
    "t3", "t4", "t5", "t6",                     /* caller-saved 0-3  */
    "s1", "s2", "s3", "s4", "s5",                /* callee-saved 4-8 */
    "s6", "s7", "s8", "s9", "s10", "s11"         /* callee-saved 9-14*/
};

/* First callee-saved index is defined in reg_alloc.h as RA_FIRST_CALLEE_SAVED */

/* -----------------------------------------------------------------------
 * Interference Graph helpers
 * ----------------------------------------------------------------------- */

/* Find or create a node for the given variable name. Returns node index. */
static int ig_get_or_add(InterferenceGraph *ig, const char *name) {
    /* Search existing nodes */
    for (int i = 0; i < ig->count; i++) {
        if (strcmp(ig->nodes[i].name, name) == 0) return i;
    }
    /* Grow array if needed */
    if (ig->count == ig->cap) {
        ig->cap = ig->cap ? ig->cap * 2 : 16;
        ig->nodes = realloc(ig->nodes, sizeof(IGNode) * ig->cap);
    }
    /* Initialise new node */
    IGNode *n = &ig->nodes[ig->count];
    memset(n, 0, sizeof(IGNode));
    n->name   = strdup(name);
    n->color  = -1;
    n->spilled = 0;
    n->interferes_with_caller_saved = 0;
    return ig->count++;
}

/* Add a directed interference edge (undirected: add in both directions). */
static void ig_add_edge(InterferenceGraph *ig, int a, int b) {
    if (a == b) return;
    /* Check if edge already exists */
    for (int i = 0; i < ig->nodes[a].degree; i++)
        if (ig->nodes[a].neighbours[i] == b) return;

    /* Add b to a's neighbour list */
    IGNode *na = &ig->nodes[a];
    if (na->degree == na->nb_cap) {
        na->nb_cap = na->nb_cap ? na->nb_cap * 2 : 4;
        na->neighbours = realloc(na->neighbours, sizeof(int) * na->nb_cap);
    }
    na->neighbours[na->degree++] = b;

    /* Add a to b's neighbour list */
    IGNode *nb = &ig->nodes[b];
    if (nb->degree == nb->nb_cap) {
        nb->nb_cap = nb->nb_cap ? nb->nb_cap * 2 : 4;
        nb->neighbours = realloc(nb->neighbours, sizeof(int) * nb->nb_cap);
    }
    nb->neighbours[nb->degree++] = a;
}

static void ig_free(InterferenceGraph *ig) {
    for (int i = 0; i < ig->count; i++) {
        free(ig->nodes[i].name);
        if (ig->nodes[i].neighbours) free(ig->nodes[i].neighbours);
    }
    free(ig->nodes);
    free(ig->func_name);
    free(ig);
}

/* -----------------------------------------------------------------------
 * Helpers: check whether a variable name is an IR temporary (t0, t1, ...)
 * or a named source variable.  Both kinds go through the allocator.
 * We skip global/struct/vtable names.
 * ----------------------------------------------------------------------- */
static int is_allocatable(const char *name, Scope *scope) {
    if (!name) return 0;
    /* Skip vtable references */
    if (strncmp(name, "vtable_", 7) == 0) return 0;
    /* Skip empty string */
    if (name[0] == '\0') return 0;

    Symbol *sym = NULL;
    if (scope) {
        sym = lookup_in_scope(scope, name);
    }
    if (!sym) {
        sym = lookup_all_scopes(name);
    }

    /* Skip variables whose address is taken (must stay on stack for correctness) */
    if (sym && sym->is_address_taken) return 0;

    return 1;
}

/* -----------------------------------------------------------------------
 * Phase 1+2: Build interference graph for one function.
 *
 * Strategy (standard textbook):
 *   For each instruction I with result r, and live set LIVE at that point:
 *     for each v in LIVE: add edge(r, v)
 *   We compute live sets by walking each basic block backward.
 * ----------------------------------------------------------------------- */
static InterferenceGraph *build_interference_graph(IRFunc *f, CFG *cfg) {
    InterferenceGraph *ig = calloc(1, sizeof(InterferenceGraph));
    ig->func_name = strdup(f->name);

    /* Ensure liveness info is up to date */
    compute_liveness(cfg);


    /* --- Parameters interfere with each other at function entry --- */
    Symbol *fsym = lookup(f->name);
    if (fsym && fsym->kind == SYM_FUNCTION) {
        for (int i = 0; i < fsym->param_count; i++) {
            Symbol *p_i = lookup_in_scope(fsym->scope, fsym->param_names[i]);
            const char *iname_i = p_i ? p_i->ir_name : fsym->param_names[i];
            for (int j = i + 1; j < fsym->param_count; j++) {
                Symbol *p_j = lookup_in_scope(fsym->scope, fsym->param_names[j]);
                const char *iname_j = p_j ? p_j->ir_name : fsym->param_names[j];
                int u = ig_get_or_add(ig, iname_i);
                int v = ig_get_or_add(ig, iname_j);
                ig_add_edge(ig, u, v);
            }
        }
    }

    /* Walk each basic block */
    BasicBlock *bb = cfg->blocks;
    while (bb) {
        /* Count instructions in this BB */
        int cnt = 0;
        IRInstr *cur = bb->instrs;
        while (cur) {
            cnt++;
            if (cur == bb->last) break;
            cur = cur->next;
        }
        if (cnt == 0) { bb = bb->next; continue; }

        /* Collect instructions into an array for backward traversal */
        IRInstr **arr = malloc(sizeof(IRInstr*) * cnt);
        cur = bb->instrs;
        for (int i = 0; i < cnt; i++) {
            arr[i] = cur;
            if (cur == bb->last) break;
            cur = cur->next;
        }

        /* Start with LIVE_OUT of this block */
        char **live = NULL;
        int live_count = 0;

        /* Copy live_out into our working live set */
        for (int i = 0; i < bb->live_out_count; i++) {
            if (!is_allocatable(bb->live_out[i], fsym ? fsym->scope : NULL)) continue;
            /* Ensure node exists */
            ig_get_or_add(ig, bb->live_out[i]);
            /* Add to live set */
            live = realloc(live, sizeof(char*) * (live_count + 1));
            live[live_count++] = bb->live_out[i];
        }

        /* Walk instructions backward */
        for (int i = cnt - 1; i >= 0; i--) {
            IRInstr *instr = arr[i];



            /* --- Add interference edges at the definition point --- */
            if (instr->result && is_allocatable(instr->result, fsym ? fsym->scope : NULL)) {
                int def_idx = ig_get_or_add(ig, instr->result);
                for (int j = 0; j < live_count; j++) {
                    if (strcmp(live[j], instr->result) == 0) continue;
                    int nb_idx = ig_get_or_add(ig, live[j]);
                    ig_add_edge(ig, def_idx, nb_idx);
                }
                /* Remove result from live set (it's defined here) */
                for (int j = 0; j < live_count; j++) {
                    if (strcmp(live[j], instr->result) == 0) {
                        live[j] = live[--live_count];
                        break;
                    }
                }
            }

            /* --- Add uses to live set --- */
            IROperand *ops[6] = {NULL};
            int nops = 0;
            switch (instr->kind) {
                case IR_ASSIGN:        ops[nops++] = &instr->src; break;
                case IR_BINOP:         ops[nops++] = &instr->left;
                                       ops[nops++] = &instr->right; break;
                case IR_UNOP:          ops[nops++] = &instr->unop_src; break;
                case IR_PARAM:         ops[nops++] = &instr->src; break;
                case IR_RETURN:        ops[nops++] = &instr->src; break;
                case IR_IF:            ops[nops++] = &instr->if_left;
                                       ops[nops++] = &instr->if_right; break;
                case IR_LOAD:          ops[nops++] = &instr->base;
                                       ops[nops++] = &instr->index; break;
                case IR_STORE:         ops[nops++] = &instr->base;
                                       ops[nops++] = &instr->index;
                                       ops[nops++] = &instr->store_val; break;
                case IR_CALL_INDIRECT: ops[nops++] = &instr->base; break;
                case IR_ALLOCA:        ops[nops++] = &instr->src; break;
                default: break;
            }
            for (int j = 0; j < nops; j++) {
                if (!ops[j] || ops[j]->is_const || !ops[j]->name) continue;
                if (!is_allocatable(ops[j]->name, fsym ? fsym->scope : NULL)) continue;
                ig_get_or_add(ig, ops[j]->name);
                /* Add to live set if not already present */
                int found = 0;
                for (int k = 0; k < live_count; k++)
                    if (strcmp(live[k], ops[j]->name) == 0) { found = 1; break; }
                if (!found) {
                    live = realloc(live, sizeof(char*) * (live_count + 1));
                    live[live_count++] = ops[j]->name;
                }
            }

            /* --- If this is a call, all variables currently in LIVE interfere with caller-saved registers --- */
            if (instr->kind == IR_CALL || instr->kind == IR_CALL_INDIRECT) {
                for (int j = 0; j < live_count; j++) {
                    int v_idx = ig_get_or_add(ig, live[j]);
                    ig->nodes[v_idx].interferes_with_caller_saved = 1;
                }
            }
        }

        free(arr);
        free(live); /* names are owned by BB live sets, just free the array */
        bb = bb->next;
    }

    return ig;
}

/* -----------------------------------------------------------------------
 * Phase 3: Simplify (Chaitin's stack-based node removal).
 *
 * Repeatedly find a node with degree < K that hasn't been removed yet.
 * If none found but uncolored nodes remain, pick the best spill candidate
 * (highest degree, not a function parameter if possible).
 *
 * Returns a stack (array) of node indices in push order.
 * stack_size is set to the number of entries.
 * ----------------------------------------------------------------------- */
static int *simplify(InterferenceGraph *ig, int *stack_size) {
    int n = ig->count;
    int *stack    = malloc(sizeof(int) * n);
    int  top      = 0;
    int  removed  = 0;

    /* Reset removed flags */
    for (int i = 0; i < n; i++) ig->nodes[i].removed = 0;

    /* Working degrees (we decrement as nodes are removed) */
    int *deg = malloc(sizeof(int) * n);
    for (int i = 0; i < n; i++) deg[i] = ig->nodes[i].degree;

    while (removed < n) {
        /* Find a node with deg < K */
        int found = -1;
        for (int i = 0; i < n; i++) {
            if (!ig->nodes[i].removed && deg[i] < RA_NUM_REGS) {
                found = i; break;
            }
        }

        if (found == -1) {
            /* All remaining nodes have degree >= K — must spill.
             * Heuristic: choose highest-degree node (most constrained). */
            int best = -1, best_deg = -1;
            for (int i = 0; i < n; i++) {
                if (!ig->nodes[i].removed && deg[i] > best_deg) {
                    best_deg = deg[i]; best = i;
                }
            }
            if (best == -1) break; /* shouldn't happen */
            found = best;
        }

        /* Push onto stack and mark removed */
        stack[top++] = found;
        ig->nodes[found].removed = 1;
        removed++;

        /* Decrement neighbours' degrees */
        for (int j = 0; j < ig->nodes[found].degree; j++) {
            int nb = ig->nodes[found].neighbours[j];
            if (!ig->nodes[nb].removed) deg[nb]--;
        }
    }

    free(deg);
    *stack_size = top;
    return stack;
}

/* -----------------------------------------------------------------------
 * Phase 4: Select — pop stack, assign colors.
 *
 * spill_base_offset: next available stack offset for spill slots (decrements).
 * Returns the number of actual spills introduced.
 * ----------------------------------------------------------------------- */
static int select_colors(InterferenceGraph *ig,
                         int *stack, int stack_size,
                         int *spill_offset_counter) {
    int spill_count = 0;

    /* Pop in reverse push order */
    for (int i = stack_size - 1; i >= 0; i--) {
        int idx = stack[i];
        ig->nodes[idx].removed = 0; /* restore for neighbour checks */

        /* Collect colors used by already-colored neighbours */
        int used[RA_NUM_REGS];
        memset(used, 0, sizeof(used));
        for (int j = 0; j < ig->nodes[idx].degree; j++) {
            int nb = ig->nodes[idx].neighbours[j];
            if (ig->nodes[nb].color >= 0)
                used[ig->nodes[nb].color] = 1;
        }

        /* If this variable is live across a call, it cannot use caller-saved registers */
        if (ig->nodes[idx].interferes_with_caller_saved) {
            for (int c = 0; c < RA_FIRST_CALLEE_SAVED; c++) {
                used[c] = 1;
            }
        }

        /* Assign the first free color */
        int assigned = -1;
        for (int c = 0; c < RA_NUM_REGS; c++) {
            if (!used[c]) { assigned = c; break; }
        }

        if (assigned >= 0) {
            ig->nodes[idx].color  = assigned;
            ig->nodes[idx].spilled = 0;
        } else {
            /* Actual spill */
            ig->nodes[idx].color   = -1;
            ig->nodes[idx].spilled  = 1;
            *spill_offset_counter  -= 8; /* Use 8-byte spill slots for 64-bit pointers/values */
            ig->nodes[idx].spill_offset = *spill_offset_counter;
            spill_count++;
        }
    }
    return spill_count;
}

/* -----------------------------------------------------------------------
 * Phase 5: Spill rewrite.
 *
 * For each spilled variable v with spill slot at offset O:
 *   - At each use of v: insert  t_new := load(s0, O)  before the instruction,
 *     replace use with t_new.
 *   - At each def of v: replace result with t_new,
 *     insert  store(s0, O) := t_new  after the instruction.
 *
 * We generate new temporaries using ir_new_temp(). Because we insert these
 * fresh temps, they will have no interferences (only short live ranges) and
 * almost certainly get registers in the next round.
 *
 * Returns 1 if any rewrite was performed (triggering another allocation round).
 * ----------------------------------------------------------------------- */

/* Helper: is this operand a reference to var `name`? */
static int op_is(IROperand *op, const char *name) {
    return op && !op->is_const && op->name && strcmp(op->name, name) == 0;
}

/* Replace all uses of `old` in an operand with `new_name`. */
static void op_rename(IROperand *op, const char *old, const char *new_name) {
    if (op && !op->is_const && op->name && strcmp(op->name, old) == 0) {
        free(op->name);
        op->name = strdup(new_name);
    }
}

static int rewrite_spills(IRFunc *f, InterferenceGraph *ig) {
    int rewrote = 0;

    /* Collect all spilled variable names and their offsets */
    int n_spills = 0;
    for (int i = 0; i < ig->count; i++)
        if (ig->nodes[i].spilled) n_spills++;
    if (n_spills == 0) return 0;

    char **spill_names   = malloc(sizeof(char*) * n_spills);
    int   *spill_offsets = malloc(sizeof(int)   * n_spills);
    int sp = 0;
    for (int i = 0; i < ig->count; i++) {
        if (ig->nodes[i].spilled) {
            spill_names[sp]   = ig->nodes[i].name;
            spill_offsets[sp] = ig->nodes[i].spill_offset;
            sp++;
        }
    }

    /* Walk the flat instruction list of the function */
    IRInstr *prev = NULL;
    IRInstr *instr = f->instrs;

    while (instr) {
        IRInstr *next = instr->next;

        for (int s = 0; s < n_spills; s++) {
            const char *sname  = spill_names[s];
            int         soff   = spill_offsets[s]; (void)soff; /* used via ig node */

            /* --- Handle uses of spilled variable --- */
            /* Gather all operand pointers that reference sname */
            IROperand *use_ops[6] = {NULL};
            int n_use = 0;

            switch (instr->kind) {
                case IR_ASSIGN:        use_ops[n_use++] = &instr->src; break;
                case IR_BINOP:         use_ops[n_use++] = &instr->left;
                                       use_ops[n_use++] = &instr->right; break;
                case IR_UNOP:          use_ops[n_use++] = &instr->unop_src; break;
                case IR_PARAM:         use_ops[n_use++] = &instr->src; break;
                case IR_RETURN:        use_ops[n_use++] = &instr->src; break;
                case IR_IF:            use_ops[n_use++] = &instr->if_left;
                                       use_ops[n_use++] = &instr->if_right; break;
                case IR_LOAD:          use_ops[n_use++] = &instr->base;
                                       use_ops[n_use++] = &instr->index; break;
                case IR_STORE:         use_ops[n_use++] = &instr->base;
                                       use_ops[n_use++] = &instr->index;
                                       use_ops[n_use++] = &instr->store_val; break;
                case IR_CALL_INDIRECT: use_ops[n_use++] = &instr->base; break;
                case IR_ALLOCA:        use_ops[n_use++] = &instr->src; break;
                default: break;
            }

            for (int u = 0; u < n_use; u++) {
                if (!op_is(use_ops[u], sname)) continue;

                /* Insert:  t_new := load(s0, soff)  before current instr */
                char *t_new = ir_new_temp();

                /* Build a fake load: base=s0, index=const(soff), scale=1
                 * We model this as IR_ASSIGN from a stack address.
                 * Since riscv_gen already does lw for named vars with offsets,
                 * we instead give it a direct memory model via a dedicated
                 * spill-load IR_ASSIGN: result = operand(sname) —
                 * but sname IS the spilled var, so we use the frame model.
                 * Simpler: create an IR_ASSIGN t_new := sname (still spilled)
                 * and let riscv_gen emit lw t_new, soff(s0).
                 * Then rename the use to t_new (t_new will be short-lived
                 * and get a real register next round).
                 */
                IRInstr *load_instr = ir_make_assign(t_new, ir_op_name(strdup(sname)), instr->line);

                /* Insert before instr */
                load_instr->next = instr;
                if (prev) prev->next = load_instr;
                else       f->instrs = load_instr;

                /* Rename the use */
                op_rename(use_ops[u], sname, t_new);
                free(t_new);

                prev = load_instr;
                rewrote = 1;
            }

            /* --- Handle def of spilled variable --- */
            if (instr->result && strcmp(instr->result, sname) == 0) {
                /* Replace result with a fresh temp, then store to spill slot */
                char *t_def = ir_new_temp();
                free(instr->result);
                instr->result = strdup(t_def);

                /* Insert:  store to soff := t_def  after current instr */
                IRInstr *store_instr = ir_make_assign(strdup(sname), ir_op_name(strdup(t_def)), instr->line);
                store_instr->next = next;
                instr->next = store_instr;
                next = store_instr; /* will be processed next iteration */

                free(t_def);
                rewrote = 1;
            }
        }

        prev = instr;
        instr = next;
    }

    free(spill_names);
    free(spill_offsets);
    return rewrote;
}

/* -----------------------------------------------------------------------
 * Build RegAllocResult from a colored interference graph.
 * ----------------------------------------------------------------------- */
static RegAllocResult *build_result(InterferenceGraph *ig, const char *func_name) {
    RegAllocResult *res = calloc(1, sizeof(RegAllocResult));
    res->func_name  = strdup(func_name);
    res->var_count  = ig->count;
    res->var_names  = malloc(sizeof(char*) * ig->count);
    res->reg_index  = malloc(sizeof(int)   * ig->count);
    res->spill_offset = malloc(sizeof(int) * ig->count);

    for (int i = 0; i < ig->count; i++) {
        res->var_names[i]    = strdup(ig->nodes[i].name);
        res->reg_index[i]    = ig->nodes[i].color;      /* -1 if spilled */
        res->spill_offset[i] = ig->nodes[i].spill_offset;

        /* Track callee-saved register usage (for prologue save/restore) */
        if (ig->nodes[i].color >= RA_FIRST_CALLEE_SAVED)
            res->callee_used[ig->nodes[i].color] = 1;
    }
    return res;
}

/* -----------------------------------------------------------------------
 * Main per-function allocation driver.
 * Iterates build→simplify→select→spill-rewrite until stable.
 * ----------------------------------------------------------------------- */
static RegAllocResult *allocate_function(IRFunc *f) {
    /* Spill slot counter: starts at -2048 (below the fixed frame area)
     * Each spill occupies 4 bytes. */
    int spill_offset_base = -512; /* Starting below the typical local area; riscv_gen will adjust frame size */

    InterferenceGraph *ig   = NULL;
    RegAllocResult    *res  = NULL;
    int                rounds = 0;

    for (;;) {
        rounds++;

        /* Phase 0: Instruction Scheduling (only on the first round before any spill rewrites) */
        if (rounds == 1) {
            ir_schedule_function(f);
        }

        /* Build CFG + liveness for (possibly rewritten) function */
        CFG *cfg = build_cfg(f);
        if (!cfg) {
            /* Empty function: return empty result */
            res = calloc(1, sizeof(RegAllocResult));
            res->func_name = strdup(f->name);
            return res;
        }

        /* Phase 1+2: interference graph */
        if (ig) ig_free(ig);
        ig = build_interference_graph(f, cfg);
        free_cfg(cfg);

        if (ig->count == 0) break; /* no variables to allocate */

        /* Phase 3: simplify */
        int stack_size;
        int *stack = simplify(ig, &stack_size);

        /* Phase 4: select */
        int n_spills = select_colors(ig, stack, stack_size, &spill_offset_base);
        free(stack);

        if (n_spills == 0) break; /* coloring succeeded — done */

        /* Phase 5: spill rewrite */
        int changed = rewrite_spills(f, ig);
        if (!changed) break; /* nothing to rewrite (shouldn't happen) */

        /* Safety valve: at most 10 rounds */
        if (rounds >= 10) break;
    }

    /* Build the result lookup table */
    res = build_result(ig, f->name);

    /* Phase 6: export DOT file (one per function, last round's graph) */
    char dot_path[128];
    snprintf(dot_path, sizeof(dot_path), "%s_interference.dot", f->name);
    reg_alloc_export_dot(ig, res, dot_path);

    ig_free(ig);
    return res;
}

/* -----------------------------------------------------------------------
 * Public API implementations
 * ----------------------------------------------------------------------- */

RegAllocResult **reg_alloc_program(IRProgram *prog) {
    if (!prog) return NULL;

    /* Count functions */
    int n = 0;
    IRFunc *f = prog->funcs;
    while (f) { n++; f = f->next; }

    /* Allocate NULL-terminated array */
    RegAllocResult **results = calloc(n + 1, sizeof(RegAllocResult*));

    f = prog->funcs;
    for (int i = 0; i < n; i++) {
        results[i] = allocate_function(f);
        f = f->next;
    }
    results[n] = NULL; /* sentinel */
    return results;
}

const char *reg_alloc_lookup(RegAllocResult *res, const char *var_name) {
    if (!res || !var_name) return NULL;
    for (int i = 0; i < res->var_count; i++) {
        if (strcmp(res->var_names[i], var_name) == 0) {
            if (res->reg_index[i] < 0) return NULL; /* spilled */
            return RA_REG_NAMES[res->reg_index[i]];
        }
    }
    return NULL;
}

int reg_alloc_spill_offset(RegAllocResult *res, const char *var_name) {
    if (!res || !var_name) return 0;
    for (int i = 0; i < res->var_count; i++) {
        if (strcmp(res->var_names[i], var_name) == 0)
            return res->spill_offset[i];
    }
    return 0;
}

int reg_alloc_is_spilled(RegAllocResult *res, const char *var_name) {
    if (!res || !var_name) return 0;
    for (int i = 0; i < res->var_count; i++) {
        if (strcmp(res->var_names[i], var_name) == 0)
            return (res->reg_index[i] < 0) ? 1 : 0;
    }
    return 0; /* not found → treat as not allocated, use stack slot */
}

void reg_alloc_free_all(RegAllocResult **results) {
    if (!results) return;
    for (int i = 0; results[i] != NULL; i++) {
        RegAllocResult *r = results[i];
        free(r->func_name);
        for (int j = 0; j < r->var_count; j++) free(r->var_names[j]);
        free(r->var_names);
        free(r->reg_index);
        free(r->spill_offset);
        free(r);
    }
    free(results);
}

/* -----------------------------------------------------------------------
 * Phase 6: Graphviz DOT export
 *
 * Nodes:
 *   - Colored by register (label = "varname\n(regname)")
 *   - Spilled nodes filled red, labeled "varname\n(spill @offset)"
 * Edges: interference edges (undirected — emit once per pair).
 * ----------------------------------------------------------------------- */

/* Pastel register colors for readability */
static const char *reg_colors[RA_NUM_REGS] = {
    "#FAD7A0", "#D2B4DE", "#F1948A", "#A3E4D7",
    "#85C1E9", "#82E0AA", "#F8C471", "#F0B27A", "#C39BD3",
    "#7FB3D3", "#76D7C4", "#F1948A", "#85929E", "#A2D9CE", "#FDEBD0"
};

void reg_alloc_export_dot(InterferenceGraph *ig, RegAllocResult *res,
                           const char *path) {
    if (!ig || !path) return;
    FILE *fp = fopen(path, "w");
    if (!fp) { fprintf(stderr, "[reg_alloc] Cannot write DOT file: %s\n", path); return; }

    fprintf(fp, "graph interference_%s {\n", ig->func_name ? ig->func_name : "fn");
    fprintf(fp, "  label=\"Interference Graph: %s\\nK=%d registers\";\n",
            ig->func_name ? ig->func_name : "fn", RA_NUM_REGS);
    fprintf(fp, "  labelloc=top;\n");
    fprintf(fp, "  node [style=filled, fontname=\"Helvetica\"];\n\n");

    /* Emit nodes */
    for (int i = 0; i < ig->count; i++) {
        IGNode *nd = &ig->nodes[i];
        const char *label_reg = "";
        const char *fill      = "#FFFFFF";

        if (nd->spilled) {
            char spill_label[32];
            snprintf(spill_label, sizeof(spill_label), "spill@%d", nd->spill_offset);
            fprintf(fp, "  n%d [label=\"%s\\n(%s)\", fillcolor=\"#E74C3C\", fontcolor=white];\n",
                    i, nd->name, spill_label);
        } else if (nd->color >= 0 && nd->color < RA_NUM_REGS) {
            label_reg = RA_REG_NAMES[nd->color];
            fill      = reg_colors[nd->color];
            fprintf(fp, "  n%d [label=\"%s\\n(%s)\", fillcolor=\"%s\"];\n",
                    i, nd->name, label_reg, fill);
        } else {
            fprintf(fp, "  n%d [label=\"%s\\n(?)\", fillcolor=\"#CCCCCC\"];\n",
                    i, nd->name);
        }
    }
    fprintf(fp, "\n");

    /* Emit edges (each undirected edge once: only when i < neighbour index) */
    for (int i = 0; i < ig->count; i++) {
        for (int j = 0; j < ig->nodes[i].degree; j++) {
            int nb = ig->nodes[i].neighbours[j];
            if (nb > i)
                fprintf(fp, "  n%d -- n%d;\n", i, nb);
        }
    }

    fprintf(fp, "}\n");
    fclose(fp);
    printf("[reg_alloc] Interference graph written to %s\n", path);
}
