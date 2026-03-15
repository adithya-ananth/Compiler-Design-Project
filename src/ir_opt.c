#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir_opt.h"
#include "y.tab.h"

/* --- CFG Construction --- */

static BasicBlock* create_bb(int id) {
    BasicBlock *bb = calloc(1, sizeof(BasicBlock));
    bb->id = id;
    return bb;
}

static void add_succ(BasicBlock *src, BasicBlock *dest) {
    if (!src || !dest) return;
    for (int i = 0; i < src->succ_count; i++) {
        if (src->succs[i] == dest) return;
    }
    src->succs = realloc(src->succs, sizeof(BasicBlock*) * (src->succ_count + 1));
    src->succs[src->succ_count++] = dest;

    dest->preds = realloc(dest->preds, sizeof(BasicBlock*) * (dest->pred_count + 1));
    dest->preds[dest->pred_count++] = src;
}

static BasicBlock* find_bb_by_label(BasicBlock *list, const char *label) {
    while (list) {
        if (list->instrs && list->instrs->kind == IR_LABEL && strcmp(list->instrs->label, label) == 0) {
            return list;
        }
        list = list->next;
    }
    return NULL;
}

CFG* build_cfg(IRFunc *f) {
    if (!f || !f->instrs) return NULL;

    CFG *cfg = calloc(1, sizeof(CFG));
    cfg->func_name = strdup(f->name);

    /* 1. Identify Leaders and Create Blocks */
    BasicBlock *head = NULL, *tail = NULL;
    int bb_count = 0;

    IRInstr *curr = f->instrs;
    while (curr) {
        BasicBlock *new_bb = create_bb(bb_count++);
        new_bb->instrs = curr;
        
        if (!head) head = new_bb;
        if (tail) tail->next = new_bb;
        tail = new_bb;

        /* Walk until end of block */
        while (curr) {
            new_bb->last = curr;
            if (curr->kind == IR_GOTO || curr->kind == IR_IF || curr->kind == IR_RETURN) {
                curr = curr->next;
                break;
            }
            if (curr->next && curr->next->kind == IR_LABEL) {
                curr = curr->next;
                break;
            }
            curr = curr->next;
        }
    }
    cfg->blocks = head;
    cfg->entry = head;
    cfg->block_count = bb_count;

    /* 2. Link Edges */
    BasicBlock *bb = head;
    while (bb) {
        IRInstr *last = bb->last;
        if (last->kind == IR_GOTO) {
            BasicBlock *target = find_bb_by_label(head, last->label);
            if (target) add_succ(bb, target);
        } else if (last->kind == IR_IF) {
            BasicBlock *target = find_bb_by_label(head, last->label);
            if (target) add_succ(bb, target);
            if (bb->next) add_succ(bb, bb->next);
        } else if (last->kind == IR_RETURN) {
            /* No successors */
        } else {
            /* Fallthrough */
            if (bb->next) add_succ(bb, bb->next);
        }
        bb = bb->next;
    }

    return cfg;
}

void free_cfg(CFG *cfg) {
    if (!cfg) return;
    BasicBlock *bb = cfg->blocks;
    while (bb) {
        BasicBlock *next = bb->next;
        if (bb->preds) free(bb->preds);
        if (bb->succs) free(bb->succs);
        free(bb);
        bb = next;
    }
    free(cfg->func_name);
    free(cfg);
}

static void mark_reachable(BasicBlock *bb, int *reachable) {
    if (!bb || reachable[bb->id]) return;
    reachable[bb->id] = 1;
    for (int i = 0; i < bb->succ_count; i++) {
        mark_reachable(bb->succs[i], reachable);
    }
}

void eliminate_unreachable_blocks(CFG *cfg) {
    if (!cfg || !cfg->entry) return;
    int *reachable = calloc(cfg->block_count, sizeof(int));
    mark_reachable(cfg->entry, reachable);

    BasicBlock **curr = &cfg->blocks;
    while (*curr) {
        if (!reachable[(*curr)->id]) {
            BasicBlock *to_delete = *curr;
            *curr = to_delete->next;
            
            /* Remove this block from its successors' preds lists */
            for (int i = 0; i < to_delete->succ_count; i++) {
                BasicBlock *succ = to_delete->succs[i];
                for (int j = 0; j < succ->pred_count; j++) {
                    if (succ->preds[j] == to_delete) {
                        succ->preds[j] = succ->preds[--succ->pred_count];
                        break;
                    }
                }
            }
            /* Note: We don't free it immediately here because we'll free the whole CFG later,
             * but it's better to remove it from the list so flatten_cfg doesn't see it.
             * But we should free its dynamic arrays.
             */
             if (to_delete->preds) free(to_delete->preds);
             if (to_delete->succs) free(to_delete->succs);
             free(to_delete);
             cfg->block_count--;
        } else {
            curr = &((*curr)->next);
        }
    }
    free(reachable);
}

IRInstr* flatten_cfg(CFG *cfg) {
    if (!cfg || !cfg->blocks) return NULL;
    
    IRInstr *head = NULL, *tail = NULL;
    BasicBlock *bb = cfg->blocks;
    
    while (bb) {
        IRInstr *instr = bb->instrs;
        while (instr) {
            IRInstr *next_in_instr_list = instr->next;
            
            instr->next = NULL;
            if (!head) head = instr;
            if (tail) tail->next = instr;
            tail = instr;

            if (instr == bb->last) break;
            instr = next_in_instr_list;
        }
        bb = bb->next;
    }
    return head;
}

/* --- Local Optimizations (BB Scope) --- */

static int fold_constants(IRInstr *instr) {
    if (instr->kind != IR_BINOP) return 0;
    if (instr->left.is_const && instr->right.is_const) {
        int val = 0;
        int valid = 1;
        switch (instr->binop) {
            case '+': val = instr->left.const_val + instr->right.const_val; break;
            case '-': val = instr->left.const_val - instr->right.const_val; break;
            case '*': val = instr->left.const_val * instr->right.const_val; break;
            case '/': 
                if (instr->right.const_val != 0) val = instr->left.const_val / instr->right.const_val; 
                else valid = 0;
                break;
            case '%':
                if (instr->right.const_val != 0) val = instr->left.const_val % instr->right.const_val;
                else valid = 0;
                break;
            default: valid = 0; break;
        }
        if (valid) {
            instr->kind = IR_ASSIGN;
            instr->src = ir_op_const(val);
            return 1;
        }
    }
    return 0;
}

static int peephole_algebraic(IRInstr *instr) {
    if (instr->kind == IR_BINOP) {
        if (instr->binop == '+' ) {
            if (instr->right.is_const && instr->right.const_val == 0) {
                instr->kind = IR_ASSIGN;
                instr->src = instr->left;
                return 1;
            }
            if (instr->left.is_const && instr->left.const_val == 0) {
                instr->kind = IR_ASSIGN;
                instr->src = instr->right;
                return 1;
            }
        }
        if (instr->binop == '*') {
            if (instr->right.is_const && instr->right.const_val == 1) {
                instr->kind = IR_ASSIGN;
                instr->src = instr->left;
                return 1;
            }
            if (instr->left.is_const && instr->left.const_val == 1) {
                instr->kind = IR_ASSIGN;
                instr->src = instr->right;
                return 1;
            }
            if ((instr->right.is_const && instr->right.const_val == 0) ||
                (instr->left.is_const && instr->left.const_val == 0)) {
                instr->kind = IR_ASSIGN;
                instr->src = ir_op_const(0);
                return 1;
            }
        }
    }
    return 0;
}

typedef struct ConstVar {
    char *name;
    int val;
    struct ConstVar *next;
} ConstVar;

static void add_const(ConstVar **list, char *name, int val) {
    ConstVar *cv = malloc(sizeof(ConstVar));
    cv->name = strdup(name);
    cv->val = val;
    cv->next = *list;
    *list = cv;
}

static int get_const(ConstVar *list, char *name, int *val) {
    while (list) {
        if (strcmp(list->name, name) == 0) {
            *val = list->val;
            return 1;
        }
        list = list->next;
    }
    return 0;
}

static void remove_const(ConstVar **list, char *name) {
    ConstVar **curr = list;
    while (*curr) {
        if (strcmp((*curr)->name, name) == 0) {
            ConstVar *tmp = *curr;
            *curr = (*curr)->next;
            free(tmp->name);
            free(tmp);
            return;
        }
        curr = &((*curr)->next);
    }
}

static void clear_consts(ConstVar *list) {
    while (list) {
        ConstVar *tmp = list;
        list = list->next;
        free(tmp->name);
        free(tmp);
    }
}

static int propagate_constants(IRInstr *instr, ConstVar **consts) {
    int changed = 0;
    if (instr->kind == IR_ASSIGN) {
        if (!instr->src.is_const && instr->src.name) {
            int val;
            if (get_const(*consts, instr->src.name, &val)) {
                instr->src = ir_op_const(val);
                changed = 1;
            }
        }
    } else if (instr->kind == IR_BINOP) {
        if (!instr->left.is_const && instr->left.name) {
            int val;
            if (get_const(*consts, instr->left.name, &val)) {
                instr->left = ir_op_const(val);
                changed = 1;
            }
        }
        if (!instr->right.is_const && instr->right.name) {
            int val;
            if (get_const(*consts, instr->right.name, &val)) {
                instr->right = ir_op_const(val);
                changed = 1;
            }
        }
    } else if (instr->kind == IR_IF) {
        if (!instr->if_left.is_const && instr->if_left.name) {
            int val;
            if (get_const(*consts, instr->if_left.name, &val)) {
                instr->if_left = ir_op_const(val);
                changed = 1;
            }
        }
        if (!instr->if_right.is_const && instr->if_right.name) {
            int val;
            if (get_const(*consts, instr->if_right.name, &val)) {
                instr->if_right = ir_op_const(val);
                changed = 1;
            }
        }
    } else if (instr->kind == IR_RETURN && !instr->src.is_const && instr->src.name) {
        int val;
        if (get_const(*consts, instr->src.name, &val)) {
            instr->src = ir_op_const(val);
            changed = 1;
        }
    }

    if (instr->result) {
        if (instr->kind == IR_ASSIGN && instr->src.is_const) {
            remove_const(consts, instr->result);
            add_const(consts, instr->result, instr->src.const_val);
        } else {
            remove_const(consts, instr->result);
        }
    }
    return changed;
}

static void optimize_bb(BasicBlock *bb) {
    int changed = 1;
    while (changed) {
        changed = 0;
        ConstVar *consts = NULL;
        IRInstr *curr = bb->instrs;
        while (curr) {
            changed |= propagate_constants(curr, &consts);
            changed |= fold_constants(curr);
            changed |= peephole_algebraic(curr);
            if (curr == bb->last) break;
            curr = curr->next;
        }
        clear_consts(consts);
    }
}

/* --- Liveness Analysis --- */

static int set_contains(char **set, int count, const char *name) {
    if (!name) return 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(set[i], name) == 0) return 1;
    }
    return 0;
}

static void set_add(char ***set, int *count, const char *name) {
    if (!name || set_contains(*set, *count, name)) return;
    *set = realloc(*set, sizeof(char*) * (*count + 1));
    (*set)[(*count)++] = strdup(name);
}

static int set_union(char ***dest, int *dest_count, char **src, int src_count) {
    int changed = 0;
    for (int i = 0; i < src_count; i++) {
        if (!set_contains(*dest, *dest_count, src[i])) {
            set_add(dest, dest_count, src[i]);
            changed = 1;
        }
    }
    return changed;
}

static void set_free(char **set, int count) {
    for (int i = 0; i < count; i++) free(set[i]);
    if (set) free(set);
}

static void compute_use_def(BasicBlock *bb) {
    IRInstr *curr = bb->instrs;
    while (curr) {
        /* Check operands (use) */
        IROperand *ops[5] = {NULL};
        int num_ops = 0;
        if (curr->kind == IR_ASSIGN) { ops[0] = &curr->src; num_ops = 1; }
        else if (curr->kind == IR_BINOP) { ops[0] = &curr->left; ops[1] = &curr->right; num_ops = 2; }
        else if (curr->kind == IR_UNOP) { ops[0] = &curr->unop_src; num_ops = 1; }
        else if (curr->kind == IR_PARAM) { ops[0] = &curr->src; num_ops = 1; } /* ir_make_param uses src */
        else if (curr->kind == IR_IF) { ops[0] = &curr->if_left; ops[1] = &curr->if_right; num_ops = 2; }
        else if (curr->kind == IR_RETURN) { ops[0] = &curr->src; num_ops = 1; }
        else if (curr->kind == IR_LOAD) { ops[0] = &curr->base; ops[1] = &curr->index; num_ops = 2; }
        else if (curr->kind == IR_STORE) { ops[0] = &curr->base; ops[1] = &curr->index; ops[2] = &curr->store_val; num_ops = 3; }

        for (int i = 0; i < num_ops; i++) {
            if (ops[i] && !ops[i]->is_const && ops[i]->name) {
                if (!set_contains(bb->def, bb->def_count, ops[i]->name)) {
                    set_add(&bb->use, &bb->use_count, ops[i]->name);
                }
            }
        }

        /* Check result (def) */
        if (curr->result) {
            if (!set_contains(bb->use, bb->use_count, curr->result)) {
                set_add(&bb->def, &bb->def_count, curr->result);
            }
        }

        if (curr == bb->last) break;
        curr = curr->next;
    }
}

void compute_liveness(CFG *cfg) {
    if (!cfg) return;
    BasicBlock *bb = cfg->blocks;
    while (bb) {
        set_free(bb->use, bb->use_count); bb->use = NULL; bb->use_count = 0;
        set_free(bb->def, bb->def_count); bb->def = NULL; bb->def_count = 0;
        set_free(bb->live_in, bb->live_in_count); bb->live_in = NULL; bb->live_in_count = 0;
        set_free(bb->live_out, bb->live_out_count); bb->live_out = NULL; bb->live_out_count = 0;
        
        compute_use_def(bb);
        bb = bb->next;
    }

    int changed = 1;
    while (changed) {
        changed = 0;
        bb = cfg->blocks;
        while (bb) {
            /* live_out = Union of live_in of successors */
            for (int i = 0; i < bb->succ_count; i++) {
                changed |= set_union(&bb->live_out, &bb->live_out_count, bb->succs[i]->live_in, bb->succs[i]->live_in_count);
            }

            /* live_in = use Union (live_out - def) */
            int old_in_count = bb->live_in_count;
            set_union(&bb->live_in, &bb->live_in_count, bb->use, bb->use_count);
            for (int i = 0; i < bb->live_out_count; i++) {
                if (!set_contains(bb->def, bb->def_count, bb->live_out[i])) {
                    set_add(&bb->live_in, &bb->live_in_count, bb->live_out[i]);
                }
            }
            if (bb->live_in_count != old_in_count) changed = 1;

            bb = bb->next;
        }
    }
}

void eliminate_dead_code(CFG *cfg) {
    if (!cfg) return;
    compute_liveness(cfg);

    BasicBlock *bb = cfg->blocks;
    while (bb) {
        /* Work backwards within the block */
        /* Note: For simplicity, we recalculate liveness at each step if we delete something,
         * or just use the global live_out and track local liveness.
         */
        char **current_live = calloc(bb->live_out_count, sizeof(char*));
        int current_live_count = 0;
        for (int i = 0; i < bb->live_out_count; i++) set_add(&current_live, &current_live_count, bb->live_out[i]);

        /* Find last instruction to start backward walk */
        /* Since we have a linked list, we might need to find previous... 
         * or just use a temporary array of instructions in the block.
         */
         int instr_count = 0;
         IRInstr *cur = bb->instrs;
         while (cur) { instr_count++; if (cur == bb->last) break; cur = cur->next; }
         
         if (instr_count > 0) {
             IRInstr **instr_arr = malloc(sizeof(IRInstr*) * instr_count);
             cur = bb->instrs;
             for (int i = 0; i < instr_count; i++) { instr_arr[i] = cur; cur = cur->next; }

             for (int i = instr_count - 1; i >= 0; i--) {
                 IRInstr *instr = instr_arr[i];

                 /* If definition is not live and has no side effects, delete */
                 if (instr->result && !set_contains(current_live, current_live_count, instr->result)) {
                     /* Side effects check: calls are NOT dead even if result unused */
                     if (instr->kind != IR_CALL && instr->kind != IR_CALL_INDIRECT) {
                         /* Remove from block list */
                         if (i == 0) bb->instrs = instr->next;
                         else instr_arr[i-1]->next = instr->next;
                         
                         if (instr == bb->last) {
                             if (i == 0) bb->last = NULL;
                             else bb->last = instr_arr[i-1];
                         }
                         /* instr->next relinking is handled in flatten_cfg anyway but let's be safe if we reuse list */
                         continue; /* Don't update liveness for deleted instr */
                     }
                 }

                 /* Update current_live: remove result, add uses */
                 if (instr->result) {
                     for (int j = 0; j < current_live_count; j++) {
                         if (strcmp(current_live[j], instr->result) == 0) {
                             free(current_live[j]);
                             current_live[j] = current_live[--current_live_count];
                             break;
                         }
                     }
                 }
                 
                 IROperand *ops[5] = {NULL};
                 int num_ops = 0;
                 if (instr->kind == IR_ASSIGN) { ops[0] = &instr->src; num_ops = 1; }
                 else if (instr->kind == IR_BINOP) { ops[0] = &instr->left; ops[1] = &instr->right; num_ops = 2; }
                 else if (instr->kind == IR_UNOP) { ops[0] = &instr->unop_src; num_ops = 1; }
                 else if (instr->kind == IR_PARAM) { ops[0] = &instr->src; num_ops = 1; }
                 else if (instr->kind == IR_IF) { ops[0] = &instr->if_left; ops[1] = &instr->if_right; num_ops = 2; }
                 else if (instr->kind == IR_RETURN) { ops[0] = &instr->src; num_ops = 1; }
                 else if (instr->kind == IR_LOAD) { ops[0] = &instr->base; ops[1] = &instr->index; num_ops = 2; }
                 else if (instr->kind == IR_STORE) { ops[0] = &instr->base; ops[1] = &instr->index; ops[2] = &instr->store_val; num_ops = 3; }

                 for (int j = 0; j < num_ops; j++) {
                     if (ops[j] && !ops[j]->is_const && ops[j]->name) {
                         set_add(&current_live, &current_live_count, ops[j]->name);
                     }
                 }
             }
             free(instr_arr);
         }
         set_free(current_live, current_live_count);
         bb = bb->next;
    }
}

/* --- Dominator Analysis --- */

void compute_dominators(CFG *cfg) {
    if (!cfg || !cfg->entry) return;
    int n = cfg->block_count;

    BasicBlock *bb = cfg->blocks;
    while (bb) {
        if (bb->doms) free(bb->doms);
        bb->doms = malloc(sizeof(int) * n);
        for (int i = 0; i < n; i++) bb->doms[i] = 1;
        bb = bb->next;
    }

    /* Entry is dominated only by itself */
    for (int i = 0; i < n; i++) cfg->entry->doms[i] = 0;
    cfg->entry->doms[cfg->entry->id] = 1;

    int changed = 1;
    while (changed) {
        changed = 0;
        bb = cfg->blocks;
        while (bb) {
            if (bb == cfg->entry) { bb = bb->next; continue; }

            int *new_doms = malloc(sizeof(int) * n);
            for (int i = 0; i < n; i++) new_doms[i] = 1;

            if (bb->pred_count > 0) {
                /* Intersection of preds' doms */
                for (int i = 0; i < bb->pred_count; i++) {
                    BasicBlock *p = bb->preds[i];
                    for (int j = 0; j < n; j++) {
                        if (!p->doms[j]) new_doms[j] = 0;
                    }
                }
            } else {
                /* No preds (except entry, but we handled that) -> should be unreachable */
            }
            new_doms[bb->id] = 1;

            for (int i = 0; i < n; i++) {
                if (bb->doms[i] != new_doms[i]) {
                    changed = 1;
                    bb->doms[i] = new_doms[i];
                }
            }
            free(new_doms);
            bb = bb->next;
        }
    }
}

/* --- Loop Invariant Code Motion (Simplified) --- */

static int is_loop_invariant(IRInstr *instr, int *loop_blocks, int n, CFG *cfg) {
    if (instr->kind != IR_BINOP && instr->kind != IR_UNOP && instr->kind != IR_ASSIGN) return 0;
    /* Calls and loads are NOT invariant for now to avoid side effects/aliasing issues */
    
    IROperand *ops[2] = {NULL};
    int num = 0;
    if (instr->kind == IR_ASSIGN) { ops[0] = &instr->src; num = 1; }
    else if (instr->kind == IR_BINOP) { ops[0] = &instr->left; ops[1] = &instr->right; num = 2; }
    else if (instr->kind == IR_UNOP) { ops[0] = &instr->unop_src; num = 1; }

    for (int i = 0; i < num; i++) {
        if (!ops[i]->is_const && ops[i]->name) {
            /* Check if result is defined in the loop */
            BasicBlock *bb = cfg->blocks;
            while (bb) {
                if (loop_blocks[bb->id]) {
                    IRInstr *check = bb->instrs;
                    while (check) {
                        if (check->result && strcmp(check->result, ops[i]->name) == 0) return 0;
                        if (check == bb->last) break;
                        check = check->next;
                    }
                }
                bb = bb->next;
            }
        }
    }
    return 1;
}

void optimize_loops(CFG *cfg) {
    if (!cfg) return;
    compute_dominators(cfg);

    /* Identify natural loops: back-edge B -> H where H dominates B */
    BasicBlock *b = cfg->blocks;
    while (b) {
        for (int i = 0; i < b->succ_count; i++) {
            BasicBlock *h = b->succs[i];
            if (b->doms[h->id]) {
                /* Found a natural loop with header H and back-edge B -> H */
                /* Collect loop blocks */
                int *loop_blocks = calloc(cfg->block_count, sizeof(int));
                loop_blocks[h->id] = 1;
                loop_blocks[b->id] = 1;
                
                /* Simple BFS/DFS to find all blocks in loop */
                /* (Omitted for brevity, assuming simple loops for now or just the two blocks) */
                
                /* For each block in loop, identify invariants */
                /* For now, just check the header and back-edge tail */
                BasicBlock *members[] = {h, b};
                for (int m = 0; m < 2; m++) {
                    BasicBlock *lb = members[m];
                    IRInstr **instr_ptr = &lb->instrs;
                    while (*instr_ptr) {
                        IRInstr *instr = *instr_ptr;
                        if (is_loop_invariant(instr, loop_blocks, cfg->block_count, cfg)) {
                            /* Move to pre-header (simplified: first pred of H that isn't in loop) */
                            BasicBlock *pre = NULL;
                            for (int p = 0; p < h->pred_count; p++) {
                                if (!loop_blocks[h->preds[p]->id]) {
                                    pre = h->preds[p];
                                    break;
                                }
                            }
                            if (pre) {
                                /* Move instruction */
                                *instr_ptr = instr->next;
                                if (instr == lb->last) lb->last = NULL; /* Simplified */
                                
                                /* Insert into pre-header before its last jump */
                                IRInstr **pre_last_ptr = &pre->instrs;
                                if (!*pre_last_ptr) {
                                    pre->instrs = pre->last = instr;
                                    instr->next = NULL;
                                } else {
                                    IRInstr *prev = NULL;
                                    IRInstr *pcur = pre->instrs;
                                    while (pcur != pre->last) { prev = pcur; pcur = pcur->next; }
                                    /* pcur is pre->last (typically a jump) */
                                    if (prev) {
                                        prev->next = instr;
                                        instr->next = pcur;
                                    } else {
                                        pre->instrs = instr;
                                        instr->next = pcur;
                                    }
                                }
                                continue;
                            }
                        }
                        if (instr == lb->last) break;
                        instr_ptr = &instr->next;
                    }
                }
                free(loop_blocks);
            }
        }
        b = b->next;
    }
}

void optimize_program(IRProgram *prog) {
    if (!prog) return;

    IRFunc *f = prog->funcs;
    while (f) {
        CFG *cfg = build_cfg(f);
        if (cfg) {
            /* Phase 2: Local optimizations */
            BasicBlock *bb = cfg->blocks;
            while (bb) {
                optimize_bb(bb);
                bb = bb->next;
            }

            /* Phase 3: Global optimizations */
            eliminate_unreachable_blocks(cfg);
            eliminate_dead_code(cfg);

            /* Phase 4: Loop optimizations (Drafted, disabled for stability) */
            // optimize_loops(cfg); 

            f->instrs = flatten_cfg(cfg);
            free_cfg(cfg);
        }
        f = f->next;
    }
}
