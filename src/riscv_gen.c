/**
 * riscv_gen.c — RISC-V assembly generation
 *
 * Uses RegAllocResult from the Chaitin register allocator.
 * For each IR variable:
 *   - If a physical register was assigned → use that register directly.
 *   - If spilled (or not in the map) → fall back to lw/sw via frame offset.
 *
 * The prologue now also saves/restores any callee-saved registers
 * (s1-s11) that were actually used by the register allocator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "symbol_table.h"
#include "reg_alloc.h"
#include "riscv_gen.h"

/* -----------------------------------------------------------------------
 * Stack-slot fallback (kept from original implementation for spilled vars
 * and named source variables not tracked by the allocator).
 * ----------------------------------------------------------------------- */
typedef struct {
    char name[64];
    int offset;
} VarOffset;

static VarOffset var_offsets[256];
static int var_count = 0;
static int current_temp_offset = 0;
static int param_idx = 0;
static RegAllocResult *cur_ra = NULL; /* NULL when running without allocator */

static void reset_offsets(int locals_size) {
    var_count = 0;
    current_temp_offset = -locals_size - 32; // Start temps below named locals and some buffer
    param_idx = 0;
}

/* Look up offset for a variable via symbol table first, then a local cache. */
/* Look up offset for a variable via symbol table first, then a local cache.
 * We add -32 to locals from the symbol table to skip the saved register area (ra, s0, s1-s4).
 */
static int get_offset(const char *name) {
    /* 1. Check Register Allocator SPILLS or TEMPS */
    if (cur_ra) {
        int spill = reg_alloc_spill_offset(cur_ra, name);
        if (spill != 0) return spill;
    }

    /* 2. Check Symbol Table (for variables with address taken, etc.) */
    Symbol *sym = lookup((char *)name);
    if (sym && sym->kind != SYM_FUNCTION && sym->kind != SYM_STRUCT) {
        /* Find saved_regs_size to correctly offset locals from the frame pointer */
        int callee_saves_count = 0;
        if (cur_ra) {
            for (int i = 0; i < RA_NUM_REGS; i++) {
                if (cur_ra->callee_used[i]) callee_saves_count++;
            }
        }
        int saved_regs_size = 8 + (callee_saves_count * 4);
        /* sym->frame_offset is negative (e.g., -4, -8). 
           We place it below the saved registers area. */
        return sym->frame_offset - saved_regs_size;
    }
    
    /* 3. Fallback for manually managed temps in this module */
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_offsets[i].name, name) == 0) return var_offsets[i].offset;
    }
    
    current_temp_offset -= 4;
    if (var_count < 256) {
        strncpy(var_offsets[var_count].name, name, 63);
        var_offsets[var_count].name[63] = '\0';
        var_offsets[var_count].offset = current_temp_offset;
        var_count++;
    }
    return current_temp_offset;
}

/* -----------------------------------------------------------------------
 * Per-function register allocation lookup (set before generating a function).
 * ----------------------------------------------------------------------- */

/*
 * Get the physical register name for a variable, or NULL if spilled/unknown.
 * Spilled variables are handled via the stack (get_offset path).
 */
static const char *get_reg(const char *name) {
    if (!cur_ra || !name) return NULL;
    return reg_alloc_lookup(cur_ra, name);
}

/* -----------------------------------------------------------------------
 * Load an operand into a destination register.
 *
 * Priority:
 *   1. Constant → li dst, val
 *   2. vtable ref → la dst, vtable_NAME
 *   3. Variable with register assigned → mv dst, phys_reg  (if dst != phys_reg)
 *   4. Variable spilled / not allocated → lw dst, offset(s0)
 * ----------------------------------------------------------------------- */
static void load_operand(FILE *out, IROperand op, const char *dst_reg) {
    if (op.is_const) {
        fprintf(out, "  li %s, %d\n", dst_reg, op.const_val);
        return;
    }
    if (!op.name) return;

    if (strncmp(op.name, "vtable_", 7) == 0 || strncmp(op.name, ".LC", 3) == 0) {
        fprintf(out, "  la %s, %s\n", dst_reg, op.name);
        return;
    }

    const char *phys = get_reg(op.name);
    if (phys) {
        /* Variable lives in a physical register */
        if (strcmp(phys, dst_reg) != 0)
            fprintf(out, "  mv %s, %s\n", dst_reg, phys);
        /* else: already in the right register — nothing to emit */
    } else {
        /* Spilled or stack variable */
        fprintf(out, "  lw %s, %d(s0)\n", dst_reg, get_offset(op.name));
    }
}

/*
 * Store a result (already computed in src_reg) to its destination.
 *
 * If the result variable has a physical register, emit mv dest_phys, src_reg.
 * If spilled, emit sw src_reg, offset(s0).
 */
static void store_result(FILE *out, const char *result_name, const char *src_reg) {
    if (!result_name) return;

    const char *phys = get_reg(result_name);
    if (phys) {
        if (strcmp(phys, src_reg) != 0)
            fprintf(out, "  mv %s, %s\n", phys, src_reg);
    } else {
        fprintf(out, "  sw %s, %d(s0)\n", src_reg, get_offset(result_name));
    }
}

/* Load the ADDRESS of a variable (for array/pointer operations). */
static void load_address(FILE *out, IROperand op, const char *dst_reg) {
    if (!op.name) return;

    if (strncmp(op.name, "vtable_", 7) == 0 || strncmp(op.name, ".LC", 3) == 0) {
        fprintf(out, "  la %s, %s\n", dst_reg, op.name);
        return;
    }
    Symbol *sym = lookup((char *)op.name);
    int off = get_offset(op.name);

    /* Pointers, parameters, and VLAs hold addresses; locals/arrays are addresses */
    if (sym && (sym->pointer_level > 0 || sym->kind == SYM_PARAMETER || sym->is_vla)) {
        /* Load the pointer value — may be register-allocated */
        const char *phys = get_reg(op.name);
        if (phys) {
            if (strcmp(phys, dst_reg) != 0)
                fprintf(out, "  mv %s, %s\n", dst_reg, phys);
        } else {
            fprintf(out, "  lw %s, %d(s0)\n", dst_reg, off);
        }
    } else {
        fprintf(out, "  addi %s, s0, %d\n", dst_reg, off);
    }
}

/* -----------------------------------------------------------------------
 * Prologue / Epilogue helpers
 * ----------------------------------------------------------------------- */

/* Emit callee-saved register saves into the frame. */
static void emit_callee_saves(FILE *out, RegAllocResult *ra, int frame_size) {
    if (!ra) return;
    int slot = frame_size - 12; /* ra at -4, s0 at -8 */
    for (int i = RA_FIRST_CALLEE_SAVED; i < RA_NUM_REGS; i++) {
        if (ra->callee_used[i]) {
            fprintf(out, "  sw %s, %d(sp)\n", RA_REG_NAMES[i], slot);
            slot -= 4;
        }
    }
}

static void emit_callee_restores(FILE *out, RegAllocResult *ra, int frame_size) {
    if (!ra) return;
    int slot = frame_size - 12;
    for (int i = RA_FIRST_CALLEE_SAVED; i < RA_NUM_REGS; i++) {
        if (ra->callee_used[i]) {
            fprintf(out, "  lw %s, %d(sp)\n", RA_REG_NAMES[i], slot);
            slot -= 4;
        }
    }
}

static int calculate_frame_size(IRFunc *func, RegAllocResult *ra) {
    int locals_size = 0;
    Symbol *fsym = lookup(func->name);
    if (fsym) locals_size = fsym->local_vars_size;

    int callee_saves_count = 0;
    if (ra) {
        for (int i = RA_FIRST_CALLEE_SAVED; i < RA_NUM_REGS; i++) {
            if (ra->callee_used[i]) callee_saves_count++;
        }
    }

    /* Standard fixed frame part */
    int saved_regs_size = 8 + (callee_saves_count * 4);
    int max_fixed_offset = saved_regs_size + locals_size;

    /* Scan allocator spills for even deeper offsets */
    if (ra) {
        for (int i = 0; i < ra->var_count; i++) {
            if (ra->reg_index[i] < 0) {
                int off = -ra->spill_offset[i];
                if (off > max_fixed_offset) max_fixed_offset = off;
            }
        }
    }

    /* Ensure we cover the fallback temp area if any were assigned (conservative) */
    /* Align to 16 bytes for RISC-V ABI compliance */
    return (max_fixed_offset + 31) & ~15;
}

/* -----------------------------------------------------------------------
 * Main code generation entry point
 * ----------------------------------------------------------------------- */
void riscv_generate(IRProgram *prog, RegAllocResult **ra_results, const char *filename) {
    FILE *out = fopen(filename, "w");
    if (!out) { perror("riscv_generate: fopen"); return; }

    fprintf(out, "  .text\n");
    fprintf(out, "  .globl main\n\n");

    /* Emit string literals */
    if (prog->strings) {
        fprintf(out, "  .section .rodata\n");
        StringLiteral *s = prog->strings;
        while (s) {
            fprintf(out, "%s:\n", s->label);
            fprintf(out, "  .asciz \"%s\"\n", s->value);
            s = s->next;
        }
        fprintf(out, "  .text\n\n");
    }

    IRFunc *func = prog->funcs;
    int func_idx = 0;

    while (func) {
        int locals_size = 0;
        Symbol *fsym = lookup(func->name);
        if (fsym) locals_size = fsym->local_vars_size;
        reset_offsets(locals_size);

        /* Select the per-function register allocation result (if available) */
        cur_ra = (ra_results && ra_results[func_idx]) ? ra_results[func_idx] : NULL;

        int frame_size = calculate_frame_size(func, cur_ra);

        fprintf(out, "%s:\n", func->name);

        /* --- PROLOGUE --- */
        fprintf(out, "  # --- Prologue (Frame Size: %d) ---\n", frame_size);
        fprintf(out, "  addi sp, sp, -%d\n", frame_size);
        fprintf(out, "  sw ra, %d(sp)\n", frame_size - 4);
        fprintf(out, "  sw s0, %d(sp)\n", frame_size - 8);
        emit_callee_saves(out, cur_ra, frame_size);
        fprintf(out, "  addi s0, sp, %d\n\n", frame_size);

        /* --- Tail recursion entry point --- */
        char tail_entry_label[128];
        snprintf(tail_entry_label, sizeof(tail_entry_label), "%s_tail_entry", func->name);
        fprintf(out, "  # Tail recursion entry point\n");
        fprintf(out, "%s:\n\n", tail_entry_label);

        /* --- Move parameters from a0-a7 to assigned locations --- */
        if (fsym && fsym->kind == SYM_FUNCTION) {
            for (int i = 0; i < fsym->param_count && i < 8; i++) {
                char arg_reg[4];
                snprintf(arg_reg, sizeof(arg_reg), "a%d", i);
                const char *var_name = fsym->param_names[i];
                const char *assigned_reg = reg_alloc_lookup(cur_ra, var_name);
                
                if (assigned_reg) {
                    if (strcmp(arg_reg, assigned_reg) != 0) {
                        fprintf(out, "  # Move param %s from %s to %s\n", var_name, arg_reg, assigned_reg);
                        fprintf(out, "  mv %s, %s\n", assigned_reg, arg_reg);
                    }
                } else {
                    /* Not in register, must be on stack (local variable area) */
                    fprintf(out, "  # Store param %s from %s to stack\n", var_name, arg_reg);
                    store_result(out, var_name, arg_reg);
                }
            }
        }

        /* --- Instruction emission --- */
        int skip_return = 0;
        IRInstr *instr = func->instrs;
        while (instr) {
            fprintf(out, "  # Line %d: ", instr->line);
            switch (instr->kind) {

                case IR_ASSIGN:
                    fprintf(out, "%s = ...\n", instr->result);
                    load_operand(out, instr->src, "t0");
                    store_result(out, instr->result, "t0");
                    break;

                case IR_BINOP:
                    fprintf(out, "%s = ... %c ...\n", instr->result, instr->binop);
                    load_operand(out, instr->left,  "t0");
                    load_operand(out, instr->right, "t1");

                    if      (instr->binop == '+') fprintf(out, "  add t2, t0, t1\n");
                    else if (instr->binop == '-') fprintf(out, "  sub t2, t0, t1\n");
                    else if (instr->binop == '*') fprintf(out, "  mul t2, t0, t1\n");
                    else if (instr->binop == '/') fprintf(out, "  div t2, t0, t1\n");
                    else if (instr->binop == '%') fprintf(out, "  rem t2, t0, t1\n");

                    store_result(out, instr->result, "t2");
                    break;

                case IR_UNOP:
                    fprintf(out, "%s = UnOp ...\n", instr->result);
                    if (instr->unop == '&') {
                        load_address(out, instr->unop_src, "t1");
                    } else {
                        load_operand(out, instr->unop_src, "t0");
                        if      (instr->unop == '-') fprintf(out, "  neg t1, t0\n");
                        else if (instr->unop == '!') fprintf(out, "  seqz t1, t0\n");
                        else                         fprintf(out, "  mv t1, t0\n");
                    }
                    store_result(out, instr->result, "t1");
                    break;

                case IR_IF:
                    fprintf(out, "if (...) goto %s\n", instr->label);
                    load_operand(out, instr->if_left,  "t0");
                    load_operand(out, instr->if_right, "t1");
                    switch (instr->relop) {
                        case IR_EQ: fprintf(out, "  beq t0, t1, %s\n", instr->label); break;
                        case IR_NE: fprintf(out, "  bne t0, t1, %s\n", instr->label); break;
                        case IR_LT: fprintf(out, "  blt t0, t1, %s\n", instr->label); break;
                        case IR_GT: fprintf(out, "  bgt t0, t1, %s\n", instr->label); break;
                        case IR_LE: fprintf(out, "  ble t0, t1, %s\n", instr->label); break;
                        case IR_GE: fprintf(out, "  bge t0, t1, %s\n", instr->label); break;
                        default: break;
                    }
                    break;

                case IR_GOTO:
                    fprintf(out, "goto %s\n", instr->label);
                    fprintf(out, "  j %s\n", instr->label);
                    break;

                case IR_LABEL:
                    fprintf(out, "label\n");
                    fprintf(out, "%s:\n", instr->label);
                    break;

                case IR_LOAD:
                    fprintf(out, "Load Array/Pointer\n");
                    load_address(out, instr->base,  "t0");
                    load_operand(out, instr->index, "t1");
                    if (instr->scale > 1)
                        fprintf(out, "  li t4, %d\n  mul t1, t1, t4\n", instr->scale);
                    fprintf(out, "  add t2, t0, t1\n");
                    fprintf(out, "  lw t2, 0(t2)\n");
                    store_result(out, instr->result, "t2");
                    break;

                case IR_ALLOCA:
                    fprintf(out, "Dynamic stack allocation (VLA)\n");
                    load_operand(out, instr->src, "t0"); /* size in bytes */
                    /* Round size up to multiple of 16 for alignment */
                    fprintf(out, "  addi t0, t0, 15\n");
                    fprintf(out, "  andi t0, t0, -16\n");
                    fprintf(out, "  sub sp, sp, t0\n");
                    fprintf(out, "  mv t1, sp\n");
                    store_result(out, instr->result, "t1");
                    break;

                case IR_STORE:
                    fprintf(out, "Store Array/Pointer\n");
                    load_address(out, instr->base,      "t0");
                    load_operand(out, instr->index,     "t1");
                    if (instr->scale > 1)
                        fprintf(out, "  li t4, %d\n  mul t1, t1, t4\n", instr->scale);
                    load_operand(out, instr->store_val, "t2");
                    fprintf(out, "  add t3, t0, t1\n");
                    fprintf(out, "  sw t2, 0(t3)\n");
                    break;

                case IR_PARAM:
                    fprintf(out, "Param\n");
                    load_operand(out, instr->src, "t0");
                    fprintf(out, "  mv a%d, t0\n", param_idx++);
                    break;

                case IR_CALL:
                    fprintf(out, "Call %s\n", instr->call_fn);
                    if (instr->is_tail_call && instr->call_fn && strcmp(instr->call_fn, func->name) == 0) {
                        /* Self-tail recursion: reuse the current stack frame. */
                        fprintf(out, "  # Tail recursive self-call: reuse frame and jump to body\n");
                        fprintf(out, "  j %s\n", tail_entry_label);
                        param_idx = 0;
                        skip_return = 1;
                        break;
                    }
                    if (instr->is_tail_call) {
                        /* Tail call to a different function: unwind frame and jump to callee. */
                        fprintf(out, "  # Tail call to another function: unwind current frame\n");
                        fprintf(out, "  addi sp, s0, -%d\n", frame_size);
                        emit_callee_restores(out, cur_ra, frame_size);
                        fprintf(out, "  lw ra, %d(sp)\n", frame_size - 4);
                        fprintf(out, "  lw s0, %d(sp)\n", frame_size - 8);
                        fprintf(out, "  addi sp, sp, %d\n", frame_size);
                        fprintf(out, "  j %s\n", instr->call_fn);
                        param_idx = 0;
                        skip_return = 1;
                        break;
                    }
                    fprintf(out, "  call %s\n", instr->call_fn);
                    if (instr->result && strlen(instr->result) > 0)
                        store_result(out, instr->result, "a0");
                    param_idx = 0;
                    break;

                case IR_CALL_INDIRECT:
                    fprintf(out, "Indirect Call (Polymorphism!)\n");
                    load_operand(out, instr->base, "t0");
                    fprintf(out, "  jalr ra, t0, 0\n");
                    if (instr->result && strlen(instr->result) > 0)
                        store_result(out, instr->result, "a0");
                    param_idx = 0;
                    break;

                case IR_RETURN:
                    if (skip_return) {
                        skip_return = 0;
                        break;
                    }
                    fprintf(out, "return\n");
                    if (instr->src.name || instr->src.is_const)
                        load_operand(out, instr->src, "a0");
                    
                    /* Restore Callee-Saves and Frame Pointers */
                    /* Re-adjust sp to fixed frame start in case of VLA */
                    fprintf(out, "  addi sp, s0, -%d\n", frame_size);
                    emit_callee_restores(out, cur_ra, frame_size);
                    fprintf(out, "  lw ra, %d(sp)\n", frame_size - 4);
                    fprintf(out, "  lw s0, %d(sp)\n", frame_size - 8);
                    fprintf(out, "  addi sp, sp, %d\n", frame_size);
                    fprintf(out, "  jr ra\n");
                    break;

                default:
                    fprintf(out, "  # Unimplemented IR instruction\n");
                    break;
            }
            instr = instr->next;
        }

        /* --- Default epilogue (reached only if no explicit return) --- */
        fprintf(out, "\n  # --- Default Epilogue ---\n");
        fprintf(out, "  addi sp, s0, -%d\n", frame_size);
        emit_callee_restores(out, cur_ra, frame_size);
        fprintf(out, "  lw ra, %d(sp)\n", frame_size - 4);
        fprintf(out, "  lw s0, %d(sp)\n", frame_size - 8);
        fprintf(out, "  addi sp, sp, %d\n", frame_size);
        fprintf(out, "  jr ra\n\n");

        func = func->next;
        func_idx++;
    }

    cur_ra = NULL;
    fclose(out);
}