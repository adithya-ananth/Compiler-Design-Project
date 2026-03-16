  .text
  .globl main

main:
  # --- Prologue ---
  addi sp, sp, -2048
  sw ra, 2044(sp)
  sw s0, 2040(sp)
  addi s0, sp, 2048

  # Line 9: Store Array/Pointer
  addi t0, s0, -8
  li t1, 0
  li t4, 4
  mul t1, t1, t4
  la t2, vtable_S
  add t3, t0, t1
  sw t2, 0(t3)
  # Line 10: return
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

S_func:
  # --- Prologue ---
  addi sp, sp, -2048
  sw ra, 2044(sp)
  sw s0, 2040(sp)
  addi s0, sp, 2048


  # --- Default Epilogue ---
  lw ra, 2044(sp)
  lw s0, 2040(sp)
  addi sp, sp, 2048
  jr ra

