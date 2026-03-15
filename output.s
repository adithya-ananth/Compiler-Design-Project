  .text
  .globl main

main:
  # --- Prologue ---
  addi sp, sp, -2048
  sw ra, 2044(sp)
  sw s0, 2040(sp)
  addi s0, sp, 2048

  # Line 12: i = ...
  li t0, 0
  sw t0, -36(s0)
  # Line 21: L0:
  # Line 12: if (...) goto L1
  blt t0, t1, L1
  # Line 12:   j L3
  # Line 21: L1:
  # Line 13: j = ...
  li t0, 0
  sw t0, -40(s0)
  # Line 20: L4:
  # Line 13: if (...) goto L5
  blt t0, t1, L5
  # Line 13:   j L7
  # Line 20: L5:
  # Line 14: t0 = ... * ...
  lw t0, -36(s0)
  li t1, 2
  mul t2, t0, t1
  sw t2, -44(s0)
  # Line 14: t1 = ... + ...
  lw t0, -40(s0)
  lw t1, -44(s0)
  add t2, t0, t1
  sw t2, -48(s0)
  # Line 14: t2 = ... * ...
  lw t0, -48(s0)
  li t1, 4
  mul t2, t0, t1
  sw t2, -52(s0)
  # Line 14: Store Array/Pointer
  add t3, t0, t1
  sw t2, 0(t3)
  # Line 15: k = ...
  li t0, 0
  sw t0, -56(s0)
  # Line 19: L8:
  # Line 15: if (...) goto L9
  blt t0, t1, L9
  # Line 15:   j L11
  # Line 19: L9:
  # Line 16: t3 = ... * ...
  lw t0, -36(s0)
  li t1, 2
  mul t2, t0, t1
  sw t2, -60(s0)
  # Line 16: t4 = ... + ...
  lw t0, -56(s0)
  lw t1, -60(s0)
  add t2, t0, t1
  sw t2, -64(s0)
  # Line 16: t5 = ... * ...
  lw t0, -64(s0)
  li t1, 4
  mul t2, t0, t1
  sw t2, -68(s0)
  # Line 16: Load Array/Pointer
  add t2, t0, t1
  lw t3, 0(t2)
  sw t3, -72(s0)
  # Line 16: t7 = ... * ...
  lw t0, -56(s0)
  li t1, 2
  mul t2, t0, t1
  sw t2, -76(s0)
  # Line 16: t8 = ... + ...
  lw t0, -40(s0)
  lw t1, -76(s0)
  add t2, t0, t1
  sw t2, -80(s0)
  # Line 16: t9 = ... * ...
  lw t0, -80(s0)
  li t1, 4
  mul t2, t0, t1
  sw t2, -84(s0)
  # Line 16: Load Array/Pointer
  add t2, t0, t1
  lw t3, 0(t2)
  sw t3, -88(s0)
  # Line 16: t11 = ... * ...
  lw t0, -72(s0)
  lw t1, -88(s0)
  mul t2, t0, t1
  sw t2, -92(s0)
  # Line 16: temp_mul = ...
  lw t0, -92(s0)
  sw t0, -96(s0)
  # Line 17: t12 = ... * ...
  lw t0, -36(s0)
  li t1, 2
  mul t2, t0, t1
  sw t2, -100(s0)
  # Line 17: t13 = ... + ...
  lw t0, -40(s0)
  lw t1, -100(s0)
  add t2, t0, t1
  sw t2, -104(s0)
  # Line 17: t14 = ... * ...
  lw t0, -104(s0)
  li t1, 4
  mul t2, t0, t1
  sw t2, -108(s0)
  # Line 17: Load Array/Pointer
  add t2, t0, t1
  lw t3, 0(t2)
  sw t3, -112(s0)
  # Line 17: t16 = ... + ...
  lw t0, -112(s0)
  lw t1, -96(s0)
  add t2, t0, t1
  sw t2, -116(s0)
  # Line 17: temp_add = ...
  lw t0, -116(s0)
  sw t0, -120(s0)
  # Line 18: t17 = ... * ...
  lw t0, -36(s0)
  li t1, 2
  mul t2, t0, t1
  sw t2, -124(s0)
  # Line 18: t18 = ... + ...
  lw t0, -40(s0)
  lw t1, -124(s0)
  add t2, t0, t1
  sw t2, -128(s0)
  # Line 18: t19 = ... * ...
  lw t0, -128(s0)
  li t1, 4
  mul t2, t0, t1
  sw t2, -132(s0)
  # Line 18: Store Array/Pointer
  add t3, t0, t1
  sw t2, 0(t3)
  # Line 19: L10:
  # Line 15: t20 = ... + ...
  lw t0, -56(s0)
  li t1, 1
  add t2, t0, t1
  sw t2, -136(s0)
  # Line 15: k = ...
  lw t0, -136(s0)
  sw t0, -56(s0)
  # Line 19:   j L8
  # Line 19: L11:
  # Line 20: L6:
  # Line 13: t21 = ... + ...
  lw t0, -40(s0)
  li t1, 1
  add t2, t0, t1
  sw t2, -140(s0)
  # Line 13: j = ...
  lw t0, -140(s0)
  sw t0, -40(s0)
  # Line 20:   j L4
  # Line 20: L7:
  # Line 21: L2:
  # Line 12: t22 = ... + ...
  lw t0, -36(s0)
  li t1, 1
  add t2, t0, t1
  sw t2, -144(s0)
  # Line 12: i = ...
  lw t0, -144(s0)
  sw t0, -36(s0)
  # Line 21:   j L0
  # Line 21: L3:
  # Line 23: return
  li a0, 0
  lw ra, 2044(sp)
  lw s0, 2040(sp)
  addi sp, sp, 2048
  jr ra

  # --- Default Epilogue ---
  lw ra, 2044(sp)
  lw s0, 2040(sp)
  addi sp, sp, 2048
  jr ra

