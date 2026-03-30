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


/* --- Control Flow Helpers --- */

static IRRelop negate_relop(IRRelop relop) {
    switch (relop) {
        case IR_LT: return IR_GE;
        case IR_GT: return IR_LE;
        case IR_LE: return IR_GT;
        case IR_GE: return IR_LT;
        case IR_EQ: return IR_NE;
        case IR_NE: return IR_EQ;
        default: return relop;
    }
}

static void free_instr_single(IRInstr *instr) {
    if (!instr) return;
    /* We ONLY free the instruction node. We DO NOT free the strings inside it, 
       because the frontend uses shared pointers directly from the Symbol Table. */
    free(instr);
}

static int eval_relop(int l, int r, IRRelop op) {
    switch (op) {
        case IR_LT: return l < r;
        case IR_GT: return l > r;
        case IR_LE: return l <= r;
        case IR_GE: return l >= r;
        case IR_EQ: return l == r;
        case IR_NE: return l != r;
        default: return 0;
    }
}

static IRInstr* simplify_control_flow(IRInstr *head) {
    if (!head) return NULL;
    int changed = 1;
    int iterations = 0;
    while (changed && iterations < 20) {
        changed = 0;
        iterations++;
        IRInstr **curr_ptr = &head;
        while (*curr_ptr) {
            IRInstr *curr = *curr_ptr;
            if (curr->kind == IR_IF && curr->if_left.is_const && curr->if_right.is_const) {
                if (eval_relop(curr->if_left.const_val, curr->if_right.const_val, curr->relop)) {
                    char *lbl = strdup(curr->label);
                    curr->if_left.name = NULL; 
                    curr->if_right.name = NULL; 
                    curr->kind = IR_GOTO;
                    curr->label = lbl;
                } else {
                    *curr_ptr = curr->next;
                    curr->next = NULL; 
                    free_instr_single(curr);
                    changed = 1;
                    continue;
                }
                changed = 1;
            }

            if (curr->kind == IR_GOTO || curr->kind == IR_RETURN) {
                while (curr->next && curr->next->kind != IR_LABEL) {
                    IRInstr *to_del = curr->next;
                    curr->next = to_del->next;
                    to_del->next = NULL;
                    free_instr_single(to_del);
                    changed = 1;
                }
            }

            if (curr->kind == IR_IF && curr->next && curr->next->kind == IR_GOTO &&
                curr->next->next && curr->next->next->kind == IR_LABEL &&
                strcmp(curr->label, curr->next->next->label) == 0) {

                char *target_L2 = strdup(curr->next->label);
                IRRelop neg_rel = negate_relop(curr->relop);
                IRInstr *goto_instr = curr->next;
                IRInstr *label_L1 = goto_instr->next;

                curr->label = target_L2;
                curr->relop = neg_rel;
                curr->next = label_L1;

                goto_instr->next = NULL;
                free_instr_single(goto_instr);
                changed = 1;
                continue;
            }

            if (curr->kind == IR_GOTO && curr->next && curr->next->kind == IR_LABEL &&
                strcmp(curr->label, curr->next->label) == 0) {

                *curr_ptr = curr->next;
                curr->next = NULL;
                free_instr_single(curr);
                changed = 1;
                continue;
            }

            if (curr->kind == IR_GOTO || curr->kind == IR_IF) {
                IRInstr *target = head;
                while (target) {
                    if (target->kind == IR_LABEL && strcmp(target->label, curr->label) == 0) {
                        if (target->next && target->next->kind == IR_GOTO) {
                            if (strcmp(curr->label, target->next->label) != 0) {
                                curr->label = strdup(target->next->label);
                                changed = 1;
                            }
                        }
                        break;
                    }
                    target = target->next;
                }
            }
            curr_ptr = &((*curr_ptr)->next);
        }
    }
    return head;
}

/* --- Local Optimizations (BB Scope) --- */

static void convert_to_assign(IRInstr *instr, IROperand new_src) {
    IROperand safe_src = new_src;
    if (new_src.name) safe_src.name = strdup(new_src.name);
    instr->kind = IR_ASSIGN;
    instr->src = safe_src;
}

static int fold_unop(IRInstr *instr) {
    if (instr->kind != IR_UNOP) return 0;
    if (instr->unop_src.is_const) {
        int val = 0;
        int valid = 1;
        switch (instr->unop) {
            case '-': val = -instr->unop_src.const_val; break;
            case '!': val = !instr->unop_src.const_val; break;
            default: valid = 0; break;
        }
        if (valid) {
            IROperand const_op; const_op.is_const = 1; const_op.const_val = val; const_op.name = NULL;
            convert_to_assign(instr, const_op);
            return 1;
        }
    }
    return 0;
}

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
                else {
                    printf("Semantic Error: Division by zero detected at line %d\n", instr->line);
                    valid = 0;
                }
                break;
            case '%':
                if (instr->right.const_val != 0) val = instr->left.const_val % instr->right.const_val;
                else {
                    printf("Semantic Error: Modulo by zero detected at line %d\n", instr->line);
                    valid = 0;
                }
                break;
            case '<': val = instr->left.const_val < instr->right.const_val; break;
            case '>': val = instr->left.const_val > instr->right.const_val; break;
            case T_LE: val = instr->left.const_val <= instr->right.const_val; break;
            case T_GE: val = instr->left.const_val >= instr->right.const_val; break;
            case T_EQ: val = instr->left.const_val == instr->right.const_val; break;
            case T_NEQ: val = instr->left.const_val != instr->right.const_val; break;
            case T_AND: val = instr->left.const_val && instr->right.const_val; break;
            case T_OR: val = instr->left.const_val || instr->right.const_val; break;
            default: valid = 0; break;
        }
        if (valid) {
            IROperand const_op; const_op.is_const = 1; const_op.const_val = val; const_op.name = NULL;
            convert_to_assign(instr, const_op);
            return 1;
        }
    }
    return 0;
}

static int peephole_algebraic(IRInstr *instr) {
    if (instr->kind == IR_BINOP) {
        if (instr->binop == '+' ) {
            if (instr->right.is_const && instr->right.const_val == 0) { convert_to_assign(instr, instr->left); return 1; }
            if (instr->left.is_const && instr->left.const_val == 0) { convert_to_assign(instr, instr->right); return 1; }
        }
        if (instr->binop == '-') {
             if (instr->right.is_const && instr->right.const_val == 0) { convert_to_assign(instr, instr->left); return 1; }
            if (!instr->left.is_const && !instr->right.is_const && instr->left.name && instr->right.name &&
                strcmp(instr->left.name, instr->right.name) == 0) {
                IROperand const_op; const_op.is_const = 1; const_op.const_val = 0; const_op.name = NULL;
                convert_to_assign(instr, const_op); return 1;
            }
        }
        if (instr->binop == '*') {
            if (instr->right.is_const && instr->right.const_val == 1) { convert_to_assign(instr, instr->left); return 1; }
            if (instr->left.is_const && instr->left.const_val == 1) { convert_to_assign(instr, instr->right); return 1; }
            if ((instr->right.is_const && instr->right.const_val == 0) || (instr->left.is_const && instr->left.const_val == 0)) {
                IROperand const_op; const_op.is_const = 1; const_op.const_val = 0; const_op.name = NULL;
                convert_to_assign(instr, const_op); return 1;
            }
        }
        if (instr->binop == '/') {
            if (instr->right.is_const && instr->right.const_val == 1) { convert_to_assign(instr, instr->left); return 1; }
            if (!instr->left.is_const && !instr->right.is_const && instr->left.name && instr->right.name &&
                strcmp(instr->left.name, instr->right.name) == 0) {
                IROperand const_op; const_op.is_const = 1; const_op.const_val = 1; const_op.name = NULL;
                convert_to_assign(instr, const_op); return 1;
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
            if (instr->left.name) instr->right.name = strdup(instr->left.name); 
            return 1;
        } else if (instr->left.is_const && instr->left.const_val == 2) {
            instr->binop = '+';
            instr->left = instr->right;
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
    ExprNode *e = malloc(sizeof(ExprNode)); 
    e->res = strdup(res); 
    e->l = l; if (l.name) e->l.name = strdup(l.name);
    e->r = r; if (r.name) e->r.name = strdup(r.name);
    e->op = op; 
    e->next = *list; 
    *list = e;
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
                ExprNode *tmp = *e; *e = (*e)->next; 
                free(tmp->res); 
                if (tmp->l.name) free(tmp->l.name);
                if (tmp->r.name) free(tmp->r.name);
                free(tmp);
            } else { e = &((*e)->next); }
        }
    }
}

static void clear_local_structs(ConstVar *c_list, CopyVar *cp_list, ExprNode *e_list) {
    while (c_list) { ConstVar *tmp = c_list; c_list = c_list->next; free(tmp->name); free(tmp); }
    while (cp_list) { CopyVar *tmp = cp_list; cp_list = cp_list->next; free(tmp->dest); free(tmp->src); free(tmp); }
    while (e_list) { 
        ExprNode *tmp = e_list; e_list = e_list->next; 
        free(tmp->res); 
        if (tmp->l.name) free(tmp->l.name);
        if (tmp->r.name) free(tmp->r.name);
        free(tmp); 
    }
}

static int propagate_constants_and_copies(IRInstr *instr, ConstVar **consts, CopyVar **copies) {
    int changed = 0;
    IROperand *ops[5] = {NULL}; int num_ops = 0;
    
    if (instr->kind == IR_ASSIGN) { ops[0] = &instr->src; num_ops = 1; }
    else if (instr->kind == IR_BINOP) { ops[0] = &instr->left; ops[1] = &instr->right; num_ops = 2; }
    else if (instr->kind == IR_UNOP) { ops[0] = &instr->unop_src; num_ops = 1; }
    else if (instr->kind == IR_IF) { ops[0] = &instr->if_left; ops[1] = &instr->if_right; num_ops = 2; }
    else if (instr->kind == IR_RETURN) { ops[0] = &instr->src; num_ops = 1; }
    else if (instr->kind == IR_PARAM) { ops[0] = &instr->src; num_ops = 1; } 
    else if (instr->kind == IR_LOAD) { ops[0] = &instr->base; ops[1] = &instr->index; num_ops = 2; }
    else if (instr->kind == IR_STORE) { ops[0] = &instr->base; ops[1] = &instr->index; ops[2] = &instr->store_val; num_ops = 3; }
    else if (instr->kind == IR_ALLOCA) { ops[0] = &instr->src; num_ops = 1; }
    else if (instr->kind == IR_CALL_INDIRECT) { ops[0] = &instr->base; num_ops = 1; }

    for (int i = 0; i < num_ops; i++) {
        if (ops[i] && !ops[i]->is_const && ops[i]->name) {
            int val; char *cpy;
            if (get_const(*consts, ops[i]->name, &val)) {
                ops[i]->is_const = 1; ops[i]->const_val = val; ops[i]->name = NULL;
                changed = 1;
            } else if ((cpy = get_copy(*copies, ops[i]->name)) != NULL) {
                ops[i]->is_const = 0; ops[i]->name = strdup(cpy); ops[i]->const_val = 0;
                changed = 1;
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
                IROperand src; src.is_const = 0; src.name = e->res; src.const_val = 0;
                convert_to_assign(instr, src);
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

typedef struct StoreRecord { 
    char *base;  
    char *index; 
    int has_const_index;
    int const_index_val;
    struct StoreRecord *next; 
} StoreRecord;

static void eliminate_dead_stores_local(BasicBlock *bb) {
    int count = 0;
    IRInstr *cur = bb->instrs;
    while (cur) { count++; if (cur == bb->last) break; cur = cur->next; }
    if (count == 0) return;

    IRInstr **arr = malloc(sizeof(IRInstr*) * count);
    int *keep = malloc(sizeof(int) * count);
    cur = bb->instrs;
    for (int i = 0; i < count; i++) { arr[i] = cur; keep[i] = 1; cur = cur->next; }

    StoreRecord *stores = NULL;

    for (int i = count - 1; i >= 0; i--) {
        IRInstr *instr = arr[i];

        if (instr->kind == IR_STORE) {
            int is_dead = 0;
            StoreRecord *s = stores;
            while (s) {
                if (s->base && instr->base.name && strcmp(s->base, instr->base.name) == 0) {
                    /* Base array matches. Check indices. */
                    if (instr->index.is_const && s->has_const_index) {
                        if (instr->index.const_val == s->const_index_val) {
                            is_dead = 1; break;
                        }
                    } else if (!instr->index.is_const && !s->has_const_index &&
                               instr->index.name && s->index && 
                               strcmp(instr->index.name, s->index) == 0) {
                        is_dead = 1; break;
                    }
                }
                s = s->next;
            }

            if (is_dead) {
                keep[i] = 0;
                continue;
            } else {
                StoreRecord *ns = malloc(sizeof(StoreRecord));
                ns->base = instr->base.name ? strdup(instr->base.name) : NULL;
                ns->has_const_index = instr->index.is_const;
                ns->const_index_val = instr->index.is_const ? instr->index.const_val : 0;
                ns->index = (instr->index.is_const || !instr->index.name) ? NULL : strdup(instr->index.name);
                ns->next = stores;
                stores = ns;
            }
        } else if (instr->kind == IR_LOAD) {
            StoreRecord **s = &stores;
            while (*s) {
                if ((*s)->base && instr->base.name && strcmp((*s)->base, instr->base.name) == 0) {
                    StoreRecord *tmp = *s;
                    *s = (*s)->next;
                    if (tmp->base) free(tmp->base); 
                    if (tmp->index) free(tmp->index); 
                    free(tmp);
                } else {
                    s = &((*s)->next);
                }
            }
        } else if (instr->kind == IR_CALL || instr->kind == IR_CALL_INDIRECT) {
            while (stores) {
                StoreRecord *tmp = stores;
                stores = stores->next;
                if (tmp->base) free(tmp->base); 
                if (tmp->index) free(tmp->index); 
                free(tmp);
            }
        }
    }
    
    IRInstr *new_head = NULL, *new_tail = NULL;
    for (int i = 0; i < count; i++) {
        if (keep[i]) {
            if (!new_head) new_head = arr[i];
            if (new_tail) new_tail->next = arr[i];
            new_tail = arr[i];
        } else {
            free_instr_single(arr[i]);
        }
    }
    if (new_tail) new_tail->next = NULL;
    bb->instrs = new_head;
    bb->last = new_tail;

    free(keep);
    free(arr);
    while (stores) {
        StoreRecord *tmp = stores; stores = stores->next;
        if (tmp->base) free(tmp->base); 
        if (tmp->index) free(tmp->index); 
        free(tmp);
    }
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
            changed |= fold_unop(curr);
            changed |= strength_reduction(curr);
            changed |= peephole_algebraic(curr);
            if (curr == bb->last) break;
            curr = curr->next;
        }
        clear_local_structs(consts, copies, exprs);
    }
    eliminate_dead_stores_local(bb);
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
        
        if (curr->kind == IR_ASSIGN) { ops[0] = &curr->src; num_ops = 1; }
        else if (curr->kind == IR_BINOP) { ops[0] = &curr->left; ops[1] = &curr->right; num_ops = 2; }
        else if (curr->kind == IR_UNOP) { ops[0] = &curr->unop_src; num_ops = 1; }
        else if (curr->kind == IR_PARAM) { ops[0] = &curr->src; num_ops = 1; } 
        else if (curr->kind == IR_IF) { ops[0] = &curr->if_left; ops[1] = &curr->if_right; num_ops = 2; }
        else if (curr->kind == IR_RETURN) { ops[0] = &curr->src; num_ops = 1; }
        else if (curr->kind == IR_LOAD) { ops[0] = &curr->base; ops[1] = &curr->index; num_ops = 2; }
        else if (curr->kind == IR_STORE) { ops[0] = &curr->base; ops[1] = &curr->index; ops[2] = &curr->store_val; num_ops = 3; }
        else if (curr->kind == IR_ALLOCA) { ops[0] = &curr->src; num_ops = 1; }
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
        char **current_live = NULL;
        int current_live_count = 0;
        for (int i = 0; i < bb->live_out_count; i++) set_add(&current_live, &current_live_count, bb->live_out[i]);

         int count = 0;
         IRInstr *cur = bb->instrs;
         while (cur) { count++; if (cur == bb->last) break; cur = cur->next; }
         
         if (count > 0) {
             IRInstr **arr = malloc(sizeof(IRInstr*) * count);
             int *keep = malloc(sizeof(int) * count);
             cur = bb->instrs;
             for (int i = 0; i < count; i++) { arr[i] = cur; keep[i] = 1; cur = cur->next; }

             for (int i = count - 1; i >= 0; i--) {
                 IRInstr *instr = arr[i];

                 if (instr->result && !set_contains(current_live, current_live_count, instr->result)) {
                     if (instr->kind != IR_CALL && instr->kind != IR_CALL_INDIRECT) {
                         keep[i] = 0; 
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
                 else if (instr->kind == IR_ALLOCA) { ops[0] = &instr->src; num_ops = 1; }
                 else if (instr->kind == IR_CALL_INDIRECT) { ops[0] = &instr->base; num_ops = 1; }

                 for (int j = 0; j < num_ops; j++) {
                     if (ops[j] && !ops[j]->is_const && ops[j]->name) {
                         set_add(&current_live, &current_live_count, ops[j]->name);
                     }
                 }
             }
             
             IRInstr *new_head = NULL, *new_tail = NULL;
             for (int i = 0; i < count; i++) {
                 if (keep[i]) {
                     if (!new_head) new_head = arr[i];
                     if (new_tail) new_tail->next = arr[i];
                     new_tail = arr[i];
                 } else {
                     free_instr_single(arr[i]);
                 }
             }
             if (new_tail) new_tail->next = NULL;
             bb->instrs = new_head;
             bb->last = new_tail;

             free(keep);
             free(arr);
         }
         set_free(current_live, current_live_count);
         bb = bb->next;
    }
}

static void mark_reachable_and_cleanup(CFG *cfg) {
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
             
             IRInstr *ins = to_delete->instrs;
             while(ins) {
                 IRInstr *nxt = ins->next;
                 free_instr_single(ins);
                 if (ins == to_delete->last) break;
                 ins = nxt;
             }

             if (to_delete->preds) free(to_delete->preds);
             if (to_delete->succs) free(to_delete->succs);
             free(to_delete);
             /* CRITICAL FIX: DO NOT DECREMENT cfg->block_count. 
                This shrinks array allocations while block IDs stay high, 
                causing catastrophic Out-Of-Bounds writes (malloc corrupted top size)! */
        } else {
            curr = &((*curr)->next);
        }
    }
    free(reachable);
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

/* Helper to safely deep-copy an instruction */
static IRInstr* copy_instr(IRInstr *src_inst) {
    if (!src_inst) return NULL;
    IRInstr *dup = malloc(sizeof(IRInstr));
    memcpy(dup, src_inst, sizeof(IRInstr));
    dup->next = NULL;

    /* Deep copy strings to prevent double-free crashes */
    if (src_inst->result) dup->result = strdup(src_inst->result);
    if (src_inst->label) dup->label = strdup(src_inst->label);
    if (src_inst->call_fn) dup->call_fn = strdup(src_inst->call_fn);

    /* Helper macro for IROperand deep copy */
    #define DUP_OP(op) if (!src_inst->op.is_const && src_inst->op.name) dup->op.name = strdup(src_inst->op.name)
    DUP_OP(src);
    DUP_OP(left);
    DUP_OP(right);
    DUP_OP(unop_src);
    DUP_OP(if_left);
    DUP_OP(if_right);
    DUP_OP(base);
    DUP_OP(index);
    DUP_OP(store_val);
    #undef DUP_OP

    return dup;
}

void unroll_loops(CFG *cfg) {
    if (!cfg) return;
    compute_dominators(cfg);

    BasicBlock *b = cfg->blocks;
    while (b) {
        for (int i = 0; i < b->succ_count; i++) {
            BasicBlock *h = b->succs[i];
            
            /* Detect Back-edge (Loop) */
            if (b->doms[h->id]) {
                
                /* Count instructions in the loop latch (body) */
                int count = 0;
                IRInstr *cur = b->instrs;
                while (cur && cur != b->last) { count++; cur = cur->next; }
                
                /* Safety limit: Only unroll small bodies to prevent massive code bloat */
                if (count == 0 || count > 15) continue;

                /* Duplicate the loop body */
                IRInstr *head_copy = NULL, *tail_copy = NULL;
                cur = b->instrs;
                while (cur && cur != b->last) {
                    // IRInstr *dup = copy_instr(cur);
                    // if (!head_copy) head_copy = dup;
                    // if (tail_copy) tail_copy->next = dup;
                    // tail_copy = dup;

                    if (cur->kind != IR_LABEL) { 
                        IRInstr *dup = copy_instr(cur);
                        if (!head_copy) head_copy = dup;
                        if (tail_copy) tail_copy->next = dup;
                        tail_copy = dup;
                    }
                    
                    cur = cur->next;
                }

                /* Splice the duplicate into the linked list right before the jump */
                if (head_copy) {
                    IRInstr *prev = b->instrs;
                    while (prev && prev->next != b->last) prev = prev->next;
                    
                    if (prev) {
                        prev->next = head_copy;
                        tail_copy->next = b->last;
                    }
                }
            }
        }
        b = b->next;
    }
}

static void merge_trivial_blocks(CFG *cfg) {
    if (!cfg || !cfg->blocks) return;
    int changed = 1;
    while (changed) {
        changed = 0;
        BasicBlock *bb = cfg->blocks;
        while (bb) {
            if (bb->succ_count == 1) {
                BasicBlock *succ = bb->succs[0];
                if (succ->pred_count == 1 && succ != bb && succ != cfg->entry) {
                    if (bb->last && bb->last->kind == IR_GOTO) {
                        IRInstr *p = bb->instrs, *prev = NULL;
                        while(p && p != bb->last) { prev = p; p = p->next; }
                        if (prev) prev->next = NULL;
                        else bb->instrs = NULL;
                        free_instr_single(bb->last);
                        bb->last = prev;
                    }
                    IRInstr *to_add = succ->instrs;
                    if (to_add && to_add->kind == IR_LABEL) {
                        IRInstr *lbl = to_add;
                        to_add = to_add->next;
                        free_instr_single(lbl);
                    }
                    if (to_add) {
                        if (bb->last) bb->last->next = to_add;
                        else bb->instrs = to_add;
                        bb->last = succ->last;
                    }
                    free(bb->succs);
                    bb->succ_count = succ->succ_count;
                    bb->succs = malloc(sizeof(BasicBlock*) * bb->succ_count);
                    for (int i = 0; i < bb->succ_count; i++) {
                        bb->succs[i] = succ->succs[i];
                        BasicBlock *child = bb->succs[i];
                        for (int j = 0; j < child->pred_count; j++) {
                            if (child->preds[j] == succ) child->preds[j] = bb;
                        }
                    }
                    succ->instrs = NULL; 
                    succ->last = NULL;
                    succ->pred_count = 0;
                    succ->succ_count = 0;
                    changed = 1;
                }
            }
            bb = bb->next;
        }
    }
}

/* --- Tail Call Optimization (TCO) Detection --- */

static int is_tail_position(IRInstr *instr) {
    /* Tail position means no further executable instructions except labels. */
    IRInstr *next = instr->next;
    while (next) {
        if (next->kind != IR_LABEL) return 0;
        next = next->next;
    }
    return 1;
}

static void detect_tail_calls(IRFunc *f) {
    if (!f || !f->instrs) return;

    IRInstr *curr = f->instrs;
    while (curr) {
        if (curr->kind == IR_CALL || curr->kind == IR_CALL_INDIRECT) {
            int is_tail = 0;
            IRInstr *next = curr->next;

            if (next && next->kind == IR_RETURN) {
                if (!curr->result && !next->src.name && !next->src.is_const) is_tail = 1;
                else if (curr->result && next->src.name && strcmp(curr->result, next->src.name) == 0) is_tail = 1;
            } else if (!next || is_tail_position(curr)) {
                /* Call at the end of a function (possibly with trailing labels) is tail position. */
                if (!curr->result) is_tail = 1;
                /* For calls returning a value followed by an implicit return through a temp var,
                   the previous condition is not enough; those patterns are already handled above. */
            }

            if (is_tail && curr->call_fn && f->name && strcmp(curr->call_fn, f->name) == 0) {
                /* Self-tail recursion; mark for codegen TCO. */
                curr->is_tail_call = 1;
            }
        }
        curr = curr->next;
    }
}

/* --- Main Optimization Pipeline --- */

void optimize_program(IRProgram *prog) {
    if (!prog) return;

    IRFunc *f = prog->funcs;
    while (f) {
        f->instrs = simplify_control_flow(f->instrs);

        CFG *cfg = build_cfg(f);
        if (cfg) {
            BasicBlock *bb = cfg->blocks;
            while (bb) {
                optimize_bb(bb);
                bb = bb->next;
            }

            mark_reachable_and_cleanup(cfg);
            eliminate_dead_code(cfg);

            merge_trivial_blocks(cfg);
            mark_reachable_and_cleanup(cfg); 

            optimize_loops(cfg); 
            unroll_loops(cfg);
            
            f->instrs = flatten_cfg(cfg);
            free_cfg(cfg);
            
            f->instrs = simplify_control_flow(f->instrs);
            
            cfg = build_cfg(f);
            if (cfg) {
                mark_reachable_and_cleanup(cfg);
                f->instrs = flatten_cfg(cfg);
                free_cfg(cfg);
            }
            
            f->instrs = simplify_control_flow(f->instrs);
            detect_tail_calls(f);
        }
        f = f->next;
    }
}