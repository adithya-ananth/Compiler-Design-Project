  .text
  .globl main

  .section .rodata
.LC0:
  .asciz "Tail-Recursive Factorial is %d\n"
  .text

main:
  # --- Prologue (Frame Size: 32) ---
  addi sp, sp, -32
  sw ra, 28(sp)
  sw s0, 24(sp)
  sw s1, 20(sp)
  addi s0, sp, 32

  # Tail recursion entry point
main_tail_entry:

  # Line 12: Param
  li t0, 7
  mv a0, t0
  # Line 12: Param
  li t0, 1
  mv a1, t0
  # Line 12: Call fact_tail
  call fact_tail
  mv t3, a0
  # Line 13: Param
  la t0, .LC0
  mv a0, t0
  # Line 13: Param
  mv t0, t3
  mv a1, t0
  # Line 13: Call printf
  call printf
  # Line 14: return
  li a0, 0
  addi sp, s0, -32
  lw s1, 20(sp)
  lw ra, 28(sp)
  lw s0, 24(sp)
  addi sp, sp, 32
  jr ra

  # --- Default Epilogue ---
  addi sp, s0, -32
  lw s1, 20(sp)
  lw ra, 28(sp)
  lw s0, 24(sp)
  addi sp, sp, 32
  jr ra

fact_tail:
  # --- Prologue (Frame Size: 32) ---
  addi sp, sp, -32
  sw ra, 28(sp)
  sw s0, 24(sp)
  addi s0, sp, 32

  # Tail recursion entry point
fact_tail_tail_entry:

  # Move param n from a0 to t5
  mv t5, a0
  # Move param accumulator from a1 to t4
  mv t4, a1
  # Line 2: if (...) goto L2
  mv t0, t5
  li t1, 1
  bgt t0, t1, L2
  # Line 8: label
L0:
  # Line 3: return
  mv a0, t4
  addi sp, s0, -32
  lw ra, 28(sp)
  lw s0, 24(sp)
  addi sp, sp, 32
  jr ra
  # Line 8: label
L2:
  # Line 8: t0 = ... - ...
  mv t0, t5
  li t1, 1
  sub t2, t0, t1
  mv t3, t2
  # Line 8: Param
  mv t0, t3
  mv a0, t0
  # Line 8: t1 = ... * ...
  mv t0, t5
  mv t1, t4
  mul t2, t0, t1
  mv t3, t2
  # Line 8: Param
  mv t0, t3
  mv a1, t0
  # Line 8: Call fact_tail
  # Tail recursive self-call: reuse frame and jump to body
  j fact_tail_tail_entry
  # Line 8: 
  # --- Default Epilogue ---
  addi sp, s0, -32
  lw ra, 28(sp)
  lw s0, 24(sp)
  addi sp, sp, 32
  jr ra

