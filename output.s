  .text
  .globl main

main:
  # --- Prologue (Frame Size: 48) ---
  addi sp, sp, -48
  sw ra, 44(sp)
  sw s0, 40(sp)
  addi s0, sp, 48

  # Tail recursion entry point
main_tail_entry:

  # Line 3: y = ...
  li t0, 5
  mv t3, t0
  # Line 2: x = ...
  li t0, 10
  mv t4, t0
  # Line 6: t0 = ... + ...
  mv t0, t3
  li t1, 2
  add t2, t0, t1
  mv t3, t2
  # Line 6: x = ...
  mv t0, t3
  mv t4, t0
  # Line 4: i = ...
  li t0, 0
  mv t3, t0
  # Line 8: label
L0:
  # Line 5: if (...) goto L2
  mv t0, t3
  li t1, 10
  bge t0, t1, L2
  # Line 8: label
L1:
  # Line 7: t1 = ... + ...
  mv t0, t3
  li t1, 1
  add t2, t0, t1
  mv t3, t2
  # Line 7: i = ...
  mv t0, t3
  mv t3, t0
  # Line 8: goto L0
  j L0
  # Line 8: label
L2:
  # Line 9: return
  mv a0, t4
  addi sp, s0, -48
  lw ra, 44(sp)
  lw s0, 40(sp)
  addi sp, sp, 48
  jr ra

  # --- Default Epilogue ---
  addi sp, s0, -48
  lw ra, 44(sp)
  lw s0, 40(sp)
  addi sp, sp, 48
  jr ra

