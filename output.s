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
  mv t5, t0
  # Line 21: label
L0:
  # Line 12: if (...) goto L1
  mv t0, t5
  li t1, 2
  blt t0, t1, L1
  # Line 12: goto L3
  j L3
  # Line 21: label
L1:
  # Line 13: j = ...
  li t0, 0
  mv t3, t0
  # Line 20: label
L4:
  # Line 13: if (...) goto L5
  mv t0, t3
  li t1, 2
  blt t0, t1, L5
  # Line 13: goto L7
  j L7
  # Line 20: label
L5:
  # Line 14: t0 = ... + ...
  mv t0, t5
  mv t1, t5
  add t2, t0, t1
  mv t0, t2
  # Line 14: t1 = ... + ...
  mv t0, t3
  mv t1, t0
  add t2, t0, t1
  mv t0, t2
  # Line 14: t2 = ... * ...
  li t1, 4
  mul t2, t0, t1
  mv t0, t2
  # Line 14: Store Array/Pointer
  addi t0, s0, -48
  mv t1, t0
  li t2, 0
  add t3, t0, t1
  sw t2, 0(t3)
  # Line 15: k = ...
  li t0, 0
  mv t6, t0
  # Line 19: label
L8:
  # Line 15: if (...) goto L9
  mv t0, t6
  li t1, 2
  blt t0, t1, L9
  # Line 15: goto L11
  j L11
  # Line 19: label
L9:
  # Line 16: t3 = ... + ...
  mv t0, t5
  mv t1, t5
  add t2, t0, t1
  # Line 16: t4 = ... + ...
  mv t0, t6
  mv t1, t2
  add t2, t0, t1
  mv t0, t2
  # Line 16: t5 = ... * ...
  li t1, 4
  mul t2, t0, t1
  mv t0, t2
  # Line 16: Load Array/Pointer
  addi t0, s0, -16
  mv t1, t0
  add t2, t0, t1
  lw t3, 0(t2)
  mv t1, t3
  # Line 16: t7 = ... + ...
  mv t0, t6
  mv t1, t6
  add t2, t0, t1
  mv t0, t2
  # Line 16: t8 = ... + ...
  mv t0, t3
  mv t1, t0
  add t2, t0, t1
  mv t0, t2
  # Line 16: t9 = ... * ...
  li t1, 4
  mul t2, t0, t1
  mv t0, t2
  # Line 16: Load Array/Pointer
  addi t0, s0, -32
  mv t1, t0
  add t2, t0, t1
  lw t3, 0(t2)
  mv t0, t3
  # Line 16: t11 = ... * ...
  mv t0, t1
  mv t1, t0
  mul t2, t0, t1
  mv t1, t2
  # Line 17: t13 = ... + ...
  mv t0, t3
  mv t1, t2
  add t2, t0, t1
  mv t0, t2
  # Line 17: t14 = ... * ...
  li t1, 4
  mul t2, t0, t1
  # Line 17: Load Array/Pointer
  addi t0, s0, -48
  mv t1, t2
  add t2, t0, t1
  lw t3, 0(t2)
  mv t0, t3
  # Line 17: t16 = ... + ...
  add t2, t0, t1
  mv t0, t2
  # Line 18: Store Array/Pointer
  addi t0, s0, -48
  mv t1, t2
  mv t2, t0
  add t3, t0, t1
  sw t2, 0(t3)
  # Line 19: label
L10:
  # Line 15: t20 = ... + ...
  mv t0, t6
  li t1, 1
  add t2, t0, t1
  mv t0, t2
  # Line 15: k = ...
  mv t6, t0
  # Line 19: goto L8
  j L8
  # Line 19: label
L11:
  # Line 20: label
L6:
  # Line 13: t21 = ... + ...
  mv t0, t3
  li t1, 1
  add t2, t0, t1
  mv t0, t2
  # Line 13: j = ...
  mv t3, t0
  # Line 20: goto L4
  j L4
  # Line 20: label
L7:
  # Line 21: label
L2:
  # Line 12: t22 = ... + ...
  mv t0, t5
  li t1, 1
  add t2, t0, t1
  mv t0, t2
  # Line 12: i = ...
  mv t5, t0
  # Line 21: goto L0
  j L0
  # Line 21: label
L3:
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

