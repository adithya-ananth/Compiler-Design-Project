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

    BasicBlock *head = NULL, *tail = NULL;
    int bb_count = 0;

    IRInstr *curr = f->instrs;
    while (curr) {
        BasicBlock *new_bb = create_bb(bb_count++);
        new_bb->instrs = curr;
        
        if (!head) head = new_bb;
        if (tail) tail->next = new_bb;
        tail = new_bb;

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
        } else if (last->kind != IR_RETURN) {
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
        if (bb->doms) free(bb->doms);
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
            
            for (int i = 0; i < to_delete->succ_count; i++) {
                BasicBlock *succ = to_delete->succs[i];
                for (int j = 0; j < succ->pred_count; j++) {
                    if (succ->preds[j] == to_delete) {
                        succ->preds[j] = succ->preds[--succ->pred_count];
                        break;
                    }
                }
            }
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
            if (instr->right.is_const && instr->right.const_val == 0) { instr->kind = IR_ASSIGN; instr->src = instr->left; return 1; }
            if (instr->left.is_const && instr->left.const_val == 0) { instr->kind = IR_ASSIGN; instr->src = instr->right; return 1; }
        }
        if (instr->binop == '*') {
            if (instr->right.is_const && instr->right.const_val == 1) { instr->kind = IR_ASSIGN; instr->src = instr->left; return 1; }
            if (instr->left.is_const && instr->left.const_val == 1) { instr->kind = IR_ASSIGN; instr->src = instr->right; return 1; }
            if ((instr->right.is_const && instr->right.const_val == 0) || (instr->left.is_const && instr->left.const_val == 0)) {
                instr->kind = IR_ASSIGN;
                instr->src = ir_op_const(0);
                return 1;
            }
        }
    }
    return 0;
}

static int strength_reduction(IRInstr *instr) {
    if (instr->kind == IR_BINOP && instr->binop == '*') {
        if (instr->right.is_const && instr->right.const_val == 2) {
            instr->binop = '+';
            instr->right = instr->left;
            /* DEEP COPY to prevent double free */
            if (instr->left.name) instr->right.name = strdup(instr->left.name); 
            return 1;
        } else if (instr->left.is_const && instr->left.const_val == 2) {
            instr->binop = '+';
            instr->left = instr->right;
            /* DEEP COPY to prevent double free */
            if (instr->right.name) instr->left.name = strdup(instr->right.name); 
            return 1;
        }
    }
    return 0;
}

typedef struct ConstVar { char *name; int val; struct ConstVar *next; } ConstVar;
typedef struct CopyVar { char *dest; char *src; struct CopyVar *next; } CopyVar;
typedef struct ExprNode { char *res; IROperand l; IROperand r; int op; struct ExprNode *next; } ExprNode;

static void add_const(ConstVar **list, char *name, int val) {
    ConstVar *cv = malloc(sizeof(ConstVar)); cv->name = strdup(name); cv->val = val; cv->next = *list; *list = cv;
}
static void add_copy(CopyVar **list, char *dest, char *src) {
    CopyVar *cv = malloc(sizeof(CopyVar)); cv->dest = strdup(dest); cv->src = strdup(src); cv->next = *list; *list = cv;
}
static void add_expr(ExprNode **list, char *res, IROperand l, IROperand r, int op) {
    ExprNode *e = malloc(sizeof(ExprNode)); e->res = strdup(res); e->l = l; e->r = r; e->op = op; e->next = *list; *list = e;
}

static int get_const(ConstVar *list, char *name, int *val) {
    while (list) { if (strcmp(list->name, name) == 0) { *val = list->val; return 1; } list = list->next; } return 0;
}
static char* get_copy(CopyVar *list, char *name) {
    while (list) { if (strcmp(list->dest, name) == 0) return list->src; list = list->next; } return NULL;
}

static void remove_const(ConstVar **list, char *name) {
    if (!list) return;
    ConstVar **curr = list;
    while (*curr) {
        if (strcmp((*curr)->name, name) == 0) { ConstVar *tmp = *curr; *curr = (*curr)->next; free(tmp->name); free(tmp); return; }
        curr = &((*curr)->next);
    }
}

// BUGFIX: Safely handle NULL checks to prevent segmentation faults during validation
static void invalidate_copies_and_exprs(CopyVar **copies, ExprNode **exprs, const char *name) {
    if (!name) return;
    
    if (copies) {
        CopyVar **c = copies;
        while (*c) {
            if (strcmp((*c)->dest, name) == 0 || strcmp((*c)->src, name) == 0) {
                CopyVar *tmp = *c; *c = (*c)->next; free(tmp->dest); free(tmp->src); free(tmp);
            } else { c = &((*c)->next); }
        }
    }
    
    if (exprs) {
        ExprNode **e = exprs;
        while (*e) {
            if (strcmp((*e)->res, name) == 0 || ((*e)->l.name && strcmp((*e)->l.name, name) == 0) || ((*e)->r.name && strcmp((*e)->r.name, name) == 0)) {
                ExprNode *tmp = *e; *e = (*e)->next; free(tmp->res); free(tmp);
            } else { e = &((*e)->next); }
        }
    }
}

static void clear_local_structs(ConstVar *c_list, CopyVar *cp_list, ExprNode *e_list) {
    while (c_list) { ConstVar *tmp = c_list; c_list = c_list->next; free(tmp->name); free(tmp); }
    while (cp_list) { CopyVar *tmp = cp_list; cp_list = cp_list->next; free(tmp->dest); free(tmp->src); free(tmp); }
    while (e_list) { ExprNode *tmp = e_list; e_list = e_list->next; free(tmp->res); free(tmp); }
}

static int propagate_constants_and_copies(IRInstr *instr, ConstVar **consts, CopyVar **copies) {
    int changed = 0;
    IROperand *ops[5] = {NULL}; int num_ops = 0;
    
    // Expanded to thoroughly support advanced arrays, pointers, and struct mechanics
    if (instr->kind == IR_ASSIGN) { ops[0] = &instr->src; num_ops = 1; }
    else if (instr->kind == IR_BINOP) { ops[0] = &instr->left; ops[1] = &instr->right; num_ops = 2; }
    else if (instr->kind == IR_UNOP) { ops[0] = &instr->unop_src; num_ops = 1; }
    else if (instr->kind == IR_IF) { ops[0] = &instr->if_left; ops[1] = &instr->if_right; num_ops = 2; }
    else if (instr->kind == IR_RETURN) { ops[0] = &instr->src; num_ops = 1; }
    else if (instr->kind == IR_PARAM) { ops[0] = &instr->src; num_ops = 1; } 
    else if (instr->kind == IR_LOAD) { ops[0] = &instr->base; ops[1] = &instr->index; num_ops = 2; }
    else if (instr->kind == IR_STORE) { ops[0] = &instr->base; ops[1] = &instr->index; ops[2] = &instr->store_val; num_ops = 3; }
    else if (instr->kind == IR_CALL_INDIRECT) { ops[0] = &instr->base; num_ops = 1; }

    for (int i = 0; i < num_ops; i++) {
        if (ops[i] && !ops[i]->is_const && ops[i]->name) {
            int val; char *cpy;
            if (get_const(*consts, ops[i]->name, &val)) {
                *ops[i] = ir_op_const(val); changed = 1;
            } else if ((cpy = get_copy(*copies, ops[i]->name)) != NULL) {
                *ops[i] = ir_op_name(cpy); changed = 1;
            }
        }
    }

    if (instr->result) {
        remove_const(consts, instr->result);
        invalidate_copies_and_exprs(copies, NULL, instr->result);
        
        if (instr->kind == IR_ASSIGN) {
            if (instr->src.is_const) add_const(consts, instr->result, instr->src.const_val);
            else if (instr->src.name) add_copy(copies, instr->result, instr->src.name);
        }
    }
    return changed;
}

static int eliminate_cse(IRInstr *instr, ExprNode **exprs) {
    if (instr->kind != IR_BINOP) {
        if (instr->result) invalidate_copies_and_exprs(NULL, exprs, instr->result);
        return 0;
    }
    
    ExprNode *e = *exprs;
    while (e) {
        if (e->op == instr->binop) {
            int left_match = (e->l.is_const && instr->left.is_const && e->l.const_val == instr->left.const_val) ||
                             (!e->l.is_const && !instr->left.is_const && e->l.name && instr->left.name && strcmp(e->l.name, instr->left.name) == 0);
            int right_match = (e->r.is_const && instr->right.is_const && e->r.const_val == instr->right.const_val) ||
                              (!e->r.is_const && !instr->right.is_const && e->r.name && instr->right.name && strcmp(e->r.name, instr->right.name) == 0);
            
            if (left_match && right_match) {
                instr->kind = IR_ASSIGN;
                instr->src = ir_op_name(e->res);
                return 1;
            }
        }
        e = e->next;
    }
    if (instr->result) {
        invalidate_copies_and_exprs(NULL, exprs, instr->result);
        add_expr(exprs, instr->result, instr->left, instr->right, instr->binop);
    }
    return 0;
}

static void optimize_bb(BasicBlock *bb) {
    int changed = 1;
    while (changed) {
        changed = 0;
        ConstVar *consts = NULL; CopyVar *copies = NULL; ExprNode *exprs = NULL;
        IRInstr *curr = bb->instrs;
        while (curr) {
            changed |= propagate_constants_and_copies(curr, &consts, &copies);
            changed |= eliminate_cse(curr, &exprs);
            changed |= fold_constants(curr);
            changed |= strength_reduction(curr);
            changed |= peephole_algebraic(curr);
            if (curr == bb->last) break;
            curr = curr->next;
        }
        clear_local_structs(consts, copies, exprs);
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
        IROperand *ops[5] = {NULL};
        int num_ops = 0;
        
        // Ensure complex instructions are safely covered
        if (curr->kind == IR_ASSIGN) { ops[0] = &curr->src; num_ops = 1; }
        else if (curr->kind == IR_BINOP) { ops[0] = &curr->left; ops[1] = &curr->right; num_ops = 2; }
        else if (curr->kind == IR_UNOP) { ops[0] = &curr->unop_src; num_ops = 1; }
        else if (curr->kind == IR_PARAM) { ops[0] = &curr->src; num_ops = 1; } 
        else if (curr->kind == IR_IF) { ops[0] = &curr->if_left; ops[1] = &curr->if_right; num_ops = 2; }
        else if (curr->kind == IR_RETURN) { ops[0] = &curr->src; num_ops = 1; }
        else if (curr->kind == IR_LOAD) { ops[0] = &curr->base; ops[1] = &curr->index; num_ops = 2; }
        else if (curr->kind == IR_STORE) { ops[0] = &curr->base; ops[1] = &curr->index; ops[2] = &curr->store_val; num_ops = 3; }
        else if (curr->kind == IR_CALL_INDIRECT) { ops[0] = &curr->base; num_ops = 1; }

        for (int i = 0; i < num_ops; i++) {
            if (ops[i] && !ops[i]->is_const && ops[i]->name) {
                if (!set_contains(bb->def, bb->def_count, ops[i]->name)) {
                    set_add(&bb->use, &bb->use_count, ops[i]->name);
                }
            }
        }

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
            for (int i = 0; i < bb->succ_count; i++) {
                changed |= set_union(&bb->live_out, &bb->live_out_count, bb->succs[i]->live_in, bb->succs[i]->live_in_count);
            }

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
        char **current_live = calloc(bb->live_out_count, sizeof(char*));
        int current_live_count = 0;
        for (int i = 0; i < bb->live_out_count; i++) set_add(&current_live, &current_live_count, bb->live_out[i]);

         int instr_count = 0;
         IRInstr *cur = bb->instrs;
         while (cur) { instr_count++; if (cur == bb->last) break; cur = cur->next; }
         
         if (instr_count > 0) {
             IRInstr **instr_arr = malloc(sizeof(IRInstr*) * instr_count);
             cur = bb->instrs;
             for (int i = 0; i < instr_count; i++) { instr_arr[i] = cur; cur = cur->next; }

             for (int i = instr_count - 1; i >= 0; i--) {
                 IRInstr *instr = instr_arr[i];

                 if (instr->result && !set_contains(current_live, current_live_count, instr->result)) {
                     if (instr->kind != IR_CALL && instr->kind != IR_CALL_INDIRECT) {
                         if (i == 0) bb->instrs = instr->next;
                         else instr_arr[i-1]->next = instr->next;
                         
                         if (instr == bb->last) {
                             if (i == 0) bb->last = NULL;
                             else bb->last = instr_arr[i-1];
                         }
                         continue; 
                     }
                 }

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
                 else if (instr->kind == IR_CALL_INDIRECT) { ops[0] = &instr->base; num_ops = 1; }

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
                for (int i = 0; i < bb->pred_count; i++) {
                    BasicBlock *p = bb->preds[i];
                    for (int j = 0; j < n; j++) {
                        if (!p->doms[j]) new_doms[j] = 0;
                    }
                }
            } else {
                for (int i = 0; i < n; i++) new_doms[i] = 0;
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

/* --- Loop Invariant Code Motion (LICM) --- */

static int is_loop_invariant(IRInstr *instr, int *loop_blocks, int n, CFG *cfg) {
    if (instr->kind != IR_BINOP && instr->kind != IR_UNOP && instr->kind != IR_ASSIGN) return 0;
    
    IROperand *ops[2] = {NULL};
    int num = 0;
    if (instr->kind == IR_ASSIGN) { ops[0] = &instr->src; num = 1; }
    else if (instr->kind == IR_BINOP) { ops[0] = &instr->left; ops[1] = &instr->right; num = 2; }
    else if (instr->kind == IR_UNOP) { ops[0] = &instr->unop_src; num = 1; }

    for (int i = 0; i < num; i++) {
        if (!ops[i]->is_const && ops[i]->name) {
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

    BasicBlock *b = cfg->blocks;
    while (b) {
        for (int i = 0; i < b->succ_count; i++) {
            BasicBlock *h = b->succs[i];
            if (b->doms[h->id]) {
                int *loop_blocks = calloc(cfg->block_count, sizeof(int));
                loop_blocks[h->id] = 1;
                loop_blocks[b->id] = 1;
                
                BasicBlock *members[] = {h, b};
                for (int m = 0; m < 2; m++) {
                    BasicBlock *lb = members[m];
                    IRInstr **instr_ptr = &lb->instrs;
                    while (*instr_ptr && *instr_ptr != lb->last) {
                        IRInstr *instr = *instr_ptr;
                        if (is_loop_invariant(instr, loop_blocks, cfg->block_count, cfg)) {
                            BasicBlock *pre = NULL;
                            for (int p = 0; p < h->pred_count; p++) {
                                if (!loop_blocks[h->preds[p]->id]) {
                                    pre = h->preds[p];
                                    break;
                                }
                            }
                            if (pre) {
                                *instr_ptr = instr->next;
                                IRInstr *pcur = pre->instrs, *prev = NULL;
                                while (pcur && pcur != pre->last) { prev = pcur; pcur = pcur->next; }
                                if (prev) { prev->next = instr; instr->next = pcur; }
                                else { pre->instrs = instr; instr->next = pcur; }
                                continue;
                            }
                        }
                        instr_ptr = &(*instr_ptr)->next;
                    }
                }
                free(loop_blocks);
            }
        }
        b = b->next;
    }
}

/* --- Loop Unrolling --- */
void unroll_loops(CFG *cfg) {
    if (!cfg) return;
    BasicBlock *b = cfg->blocks;
    while (b) {
        if (b->succ_count == 2 && (b->succs[0] == b || b->succs[1] == b)) {
            int count = 0; 
            IRInstr *curr = b->instrs;
            while (curr && curr != b->last) { count++; curr = curr->next; }
            
            if (count <= 10 && count >= 2) {
                IRInstr *clone_head = NULL, *clone_tail = NULL;
                curr = b->instrs;
                while (curr && curr != b->last) {
                    IRInstr *clone = malloc(sizeof(IRInstr));
                    memcpy(clone, curr, sizeof(IRInstr));
                    clone->next = NULL;
                    
                    /* DEEP COPY all strings to prevent double-frees */
                    if (curr->result) clone->result = strdup(curr->result);
                    if (curr->left.name) clone->left.name = strdup(curr->left.name);
                    if (curr->right.name) clone->right.name = strdup(curr->right.name);
                    if (curr->src.name) clone->src.name = strdup(curr->src.name);
                    if (curr->unop_src.name) clone->unop_src.name = strdup(curr->unop_src.name);
                    if (curr->base.name) clone->base.name = strdup(curr->base.name);
                    if (curr->index.name) clone->index.name = strdup(curr->index.name);
                    if (curr->store_val.name) clone->store_val.name = strdup(curr->store_val.name);
                    if (curr->if_left.name) clone->if_left.name = strdup(curr->if_left.name);
                    if (curr->if_right.name) clone->if_right.name = strdup(curr->if_right.name);
                    if (curr->label) clone->label = strdup(curr->label);
                    if (curr->call_fn) clone->call_fn = strdup(curr->call_fn);
                    
                    if (!clone_head) clone_head = clone;
                    if (clone_tail) clone_tail->next = clone;
                    clone_tail = clone;
                    curr = curr->next;
                }
                IRInstr *pcur = b->instrs, *prev = NULL;
                while (pcur && pcur != b->last) { prev = pcur; pcur = pcur->next; }
                
                if (prev) { prev->next = clone_head; clone_tail->next = pcur; }
                else if (clone_head) { b->instrs = clone_head; clone_tail->next = pcur; }
            }
        }
        b = b->next;
    }
}

/* --- Main Optimization Pipeline --- */

void optimize_program(IRProgram *prog) {
    if (!prog) return;

    IRFunc *f = prog->funcs;
    while (f) {
        CFG *cfg = build_cfg(f);
        if (cfg) {
            /* Phase 2: Local optimizations (CSE, CP, Fold, Alg, StrReduct) */
            BasicBlock *bb = cfg->blocks;
            while (bb) {
                optimize_bb(bb);
                bb = bb->next;
            }

            /* Phase 3: Global optimizations (DCE, Unreachable) */
            eliminate_unreachable_blocks(cfg);
            eliminate_dead_code(cfg);

            /* Phase 4: Loop optimizations (LICM, Unrolling) */
            optimize_loops(cfg); 
            // unroll_loops(cfg);

            f->instrs = flatten_cfg(cfg);
            free_cfg(cfg);
        }
        f = f->next;
    }
}