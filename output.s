  .text
  .globl main

  .section .rodata
.LC10:
  .asciz "\n"
.LC9:
  .asciz "%d\t"
.LC8:
  .asciz "\nResultant Matrix C:\n"
.LC7:
  .asciz "%d"
.LC6:
  .asciz "B[%d][%d]: "
.LC5:
  .asciz "\nEnter elements for Matrix B (%dx%d):\n"
.LC4:
  .asciz "%d"
.LC3:
  .asciz "A[%d][%d]: "
.LC2:
  .asciz "Enter elements for Matrix A (%dx%d):\n"
.LC1:
  .asciz "%d"
.LC0:
  .asciz "Enter the size of the array: "
  .text

main:
  # --- Prologue (Frame Size: 160) ---
  addi sp, sp, -160
  sd ra, 152(sp)
  sd s0, 144(sp)
  sd s1, 136(sp)
  sd s2, 128(sp)
  sd s3, 120(sp)
  sd s4, 112(sp)
  sd s5, 104(sp)
  sd s6, 96(sp)
  sd s7, 88(sp)
  sd s8, 80(sp)
  sd s9, 72(sp)
  addi s0, sp, 160

  # Line 3: Param
  la t0, .LC0
  mv a0, t0
  # Line 3: Call printf
  call printf
  # Line 4: t0 = UnOp ...
  addi t1, s0, -92
  mv t3, t1
  # Line 4: Param
  la t0, .LC1
  mv a0, t0
  # Line 4: Param
  mv t0, t3
  mv a1, t0
  # Line 4: Call scanf
  call scanf
  # Line 10: Param
  la t0, .LC2
  mv a0, t0
  # Line 10: Param
  lw t0, -92(s0)
  mv a1, t0
  # Line 5: t1 = ... * ...
  li t0, 4
  lw t1, -92(s0)
  mul t2, t0, t1
  mv t3, t2
  # Line 10: Param
  lw t0, -92(s0)
  mv a2, t0
  # Line 5: t2 = ... * ...
  mv t0, t3
  lw t1, -92(s0)
  mul t2, t0, t1
  mv t3, t2
  # Line 5: Dynamic stack allocation (VLA)
  mv t0, t3
  addi t0, t0, 15
  andi t0, t0, -16
  sub sp, sp, t0
  mv t1, sp
  mv s7, t1
  # Line 5: Dynamic stack allocation (VLA)
  mv t0, t3
  addi t0, t0, 15
  andi t0, t0, -16
  sub sp, sp, t0
  mv t1, sp
  mv s2, t1
  # Line 5: Dynamic stack allocation (VLA)
  mv t0, t3
  addi t0, t0, 15
  andi t0, t0, -16
  sub sp, sp, t0
  mv t1, sp
  mv s4, t1
  # Line 10: Call printf
  call printf
  # Line 11: i$5 = ...
  li t0, 0
  mv s5, t0
  # Line 15: t7 = UnOp ...
  addi t1, s0, -128
  mv s8, t1
  # Line 18: label
L0:
  # Line 11: if (...) goto L3
  mv t0, s5
  lw t1, -92(s0)
  bge t0, t1, L3
  # Line 18: label
L1:
  # Line 12: j$6 = ...
  li t0, 0
  mv s6, t0
  # Line 16: t9 = ... * ...
  mv t0, s5
  lw t1, -92(s0)
  mul t2, t0, t1
  mv s1, t2
  # Line 17: label
L4:
  # Line 12: if (...) goto L7
  mv t0, s6
  lw t1, -92(s0)
  bge t0, t1, L7
  # Line 17: label
L5:
  # Line 13: Param
  la t0, .LC3
  mv a0, t0
  # Line 13: Param
  mv t0, s5
  mv a1, t0
  # Line 13: Param
  mv t0, s6
  mv a2, t0
  # Line 13: Call printf
  call printf
  # Line 15: Param
  la t0, .LC4
  mv a0, t0
  # Line 15: Param
  mv t0, s8
  mv a1, t0
  # Line 15: Call scanf
  call scanf
  # Line 16: t10 = ... + ...
  mv t0, s6
  mv t1, s1
  add t2, t0, t1
  mv t3, t2
  # Line 12: t12 = ... + ...
  mv t0, s6
  li t1, 1
  add t2, t0, t1
  mv t4, t2
  # Line 16: Store Array/Pointer
  mv t0, s7
  mv t1, t3
  slli t1, t1, 2
  lw t2, -128(s0)
  add t0, t0, t1
  sw t2, 0(t0)
  # Line 12: j$6 = ...
  mv t0, t4
  mv s6, t0
  # Line 17: goto L4
  j L4
  # Line 17: label
L7:
  # Line 11: t14 = ... + ...
  mv t0, s5
  li t1, 1
  add t2, t0, t1
  mv t3, t2
  # Line 11: i$5 = ...
  mv t0, t3
  mv s5, t0
  # Line 18: goto L0
  j L0
  # Line 18: label
L3:
  # Line 21: Param
  la t0, .LC5
  mv a0, t0
  # Line 21: Param
  lw t0, -92(s0)
  mv a1, t0
  # Line 21: Param
  lw t0, -92(s0)
  mv a2, t0
  # Line 21: Call printf
  call printf
  # Line 22: i$5 = ...
  li t0, 0
  mv s5, t0
  # Line 26: t15 = UnOp ...
  addi t1, s0, -132
  mv s8, t1
  # Line 29: label
L8:
  # Line 22: if (...) goto L11
  mv t0, s5
  lw t1, -92(s0)
  bge t0, t1, L11
  # Line 29: label
L9:
  # Line 23: j$6 = ...
  li t0, 0
  mv s6, t0
  # Line 27: t17 = ... * ...
  mv t0, s5
  lw t1, -92(s0)
  mul t2, t0, t1
  mv s1, t2
  # Line 28: label
L12:
  # Line 23: if (...) goto L15
  mv t0, s6
  lw t1, -92(s0)
  bge t0, t1, L15
  # Line 28: label
L13:
  # Line 24: Param
  la t0, .LC6
  mv a0, t0
  # Line 24: Param
  mv t0, s5
  mv a1, t0
  # Line 24: Param
  mv t0, s6
  mv a2, t0
  # Line 24: Call printf
  call printf
  # Line 26: Param
  la t0, .LC7
  mv a0, t0
  # Line 26: Param
  mv t0, s8
  mv a1, t0
  # Line 26: Call scanf
  call scanf
  # Line 27: t18 = ... + ...
  mv t0, s6
  mv t1, s1
  add t2, t0, t1
  mv t3, t2
  # Line 23: t20 = ... + ...
  mv t0, s6
  li t1, 1
  add t2, t0, t1
  mv t4, t2
  # Line 27: Store Array/Pointer
  mv t0, s2
  mv t1, t3
  slli t1, t1, 2
  lw t2, -132(s0)
  add t0, t0, t1
  sw t2, 0(t0)
  # Line 23: j$6 = ...
  mv t0, t4
  mv s6, t0
  # Line 28: goto L12
  j L12
  # Line 28: label
L15:
  # Line 22: t22 = ... + ...
  mv t0, s5
  li t1, 1
  add t2, t0, t1
  mv t3, t2
  # Line 22: i$5 = ...
  mv t0, t3
  mv s5, t0
  # Line 29: goto L8
  j L8
  # Line 29: label
L11:
  # Line 32: i$5 = ...
  li t0, 0
  mv s5, t0
  # Line 41: label
L16:
  # Line 32: if (...) goto L19
  mv t0, s5
  lw t1, -92(s0)
  bge t0, t1, L19
  # Line 41: label
L17:
  # Line 33: j$6 = ...
  li t0, 0
  mv s6, t0
  # Line 34: t24 = ... * ...
  mv t0, s5
  lw t1, -92(s0)
  mul t2, t0, t1
  mv s8, t2
  # Line 36: t27 = ... * ...
  mv t0, s5
  lw t1, -92(s0)
  mul t2, t0, t1
  mv s1, t2
  # Line 40: label
L20:
  # Line 33: if (...) goto L23
  mv t0, s6
  lw t1, -92(s0)
  bge t0, t1, L23
  # Line 40: label
L21:
  # Line 34: t25 = ... + ...
  mv t0, s6
  mv t1, s8
  add t2, t0, t1
  mv t3, t2
  # Line 34: Store Array/Pointer
  mv t0, s4
  mv t1, t3
  slli t1, t1, 2
  li t2, 0
  add t0, t0, t1
  sw t2, 0(t0)
  # Line 35: k$7 = ...
  li t0, 0
  mv t6, t0
  # Line 37: t37 = ... + ...
  mv t0, s6
  mv t1, s1
  add t2, t0, t1
  mv t5, t2
  # Line 39: label
L24:
  # Line 35: if (...) goto L27
  mv t0, t6
  lw t1, -92(s0)
  bge t0, t1, L27
  # Line 39: label
L25:
  # Line 36: t31 = ... * ...
  mv t0, t6
  lw t1, -92(s0)
  mul t2, t0, t1
  mv t3, t2
  # Line 36: t28 = ... + ...
  mv t0, t6
  mv t1, s1
  add t2, t0, t1
  mv t4, t2
  # Line 36: t32 = ... + ...
  mv t0, s6
  mv t1, t3
  add t2, t0, t1
  mv t3, t2
  # Line 36: Load Array/Pointer
  mv t0, s7
  mv t1, t4
  slli t1, t1, 2
  add t2, t0, t1
  lw t2, 0(t2)
  mv t4, t2
  # Line 36: Load Array/Pointer
  mv t0, s2
  mv t1, t3
  slli t1, t1, 2
  add t2, t0, t1
  lw t2, 0(t2)
  mv t3, t2
  # Line 37: Load Array/Pointer
  mv t0, s4
  mv t1, t5
  slli t1, t1, 2
  add t2, t0, t1
  lw t2, 0(t2)
  mv s9, t2
  # Line 36: t34 = ... * ...
  mv t0, t4
  mv t1, t3
  mul t2, t0, t1
  mv t3, t2
  # Line 37: t39 = ... + ...
  mv t0, s9
  mv t1, t3
  add t2, t0, t1
  mv t3, t2
  # Line 35: t44 = ... + ...
  mv t0, t6
  li t1, 1
  add t2, t0, t1
  mv t4, t2
  # Line 38: Store Array/Pointer
  mv t0, s4
  mv t1, t5
  slli t1, t1, 2
  mv t2, t3
  add t0, t0, t1
  sw t2, 0(t0)
  # Line 35: k$7 = ...
  mv t0, t4
  mv t6, t0
  # Line 39: goto L24
  j L24
  # Line 39: label
L27:
  # Line 33: t46 = ... + ...
  mv t0, s6
  li t1, 1
  add t2, t0, t1
  mv t3, t2
  # Line 33: j$6 = ...
  mv t0, t3
  mv s6, t0
  # Line 40: goto L20
  j L20
  # Line 40: label
L23:
  # Line 32: t48 = ... + ...
  mv t0, s5
  li t1, 1
  add t2, t0, t1
  mv t3, t2
  # Line 32: i$5 = ...
  mv t0, t3
  mv s5, t0
  # Line 41: goto L16
  j L16
  # Line 41: label
L19:
  # Line 44: Param
  la t0, .LC8
  mv a0, t0
  # Line 44: Call printf
  call printf
  # Line 45: i$5 = ...
  li t0, 0
  mv s5, t0
  # Line 51: label
L28:
  # Line 45: if (...) goto L31
  mv t0, s5
  lw t1, -92(s0)
  bge t0, t1, L31
  # Line 51: label
L29:
  # Line 46: j$6 = ...
  li t0, 0
  mv s6, t0
  # Line 47: t50 = ... * ...
  mv t0, s5
  lw t1, -92(s0)
  mul t2, t0, t1
  mv s2, t2
  # Line 49: label
L32:
  # Line 46: if (...) goto L35
  mv t0, s6
  lw t1, -92(s0)
  bge t0, t1, L35
  # Line 49: label
L33:
  # Line 47: t51 = ... + ...
  mv t0, s6
  mv t1, s2
  add t2, t0, t1
  mv t3, t2
  # Line 47: Load Array/Pointer
  mv t0, s4
  mv t1, t3
  slli t1, t1, 2
  add t2, t0, t1
  lw t2, 0(t2)
  mv t3, t2
  # Line 48: Param
  la t0, .LC9
  mv a0, t0
  # Line 48: Param
  mv t0, t3
  mv a1, t0
  # Line 46: t54 = ... + ...
  mv t0, s6
  li t1, 1
  add t2, t0, t1
  mv s1, t2
  # Line 48: Call printf
  call printf
  # Line 46: j$6 = ...
  mv t0, s1
  mv s6, t0
  # Line 49: goto L32
  j L32
  # Line 49: label
L35:
  # Line 50: Param
  la t0, .LC10
  mv a0, t0
  # Line 45: t56 = ... + ...
  mv t0, s5
  li t1, 1
  add t2, t0, t1
  mv s1, t2
  # Line 50: Call printf
  call printf
  # Line 45: i$5 = ...
  mv t0, s1
  mv s5, t0
  # Line 51: goto L28
  j L28
  # Line 51: label
L31:
  # Line 53: return
  li a0, 0
  addi sp, s0, -160
  ld s1, 136(sp)
  ld s2, 128(sp)
  ld s3, 120(sp)
  ld s4, 112(sp)
  ld s5, 104(sp)
  ld s6, 96(sp)
  ld s7, 88(sp)
  ld s8, 80(sp)
  ld s9, 72(sp)
  ld ra, 152(sp)
  ld s0, 144(sp)
  addi sp, sp, 160
  jr ra

  # --- Default Epilogue ---
  addi sp, s0, -160
  ld s1, 136(sp)
  ld s2, 128(sp)
  ld s3, 120(sp)
  ld s4, 112(sp)
  ld s5, 104(sp)
  ld s6, 96(sp)
  ld s7, 88(sp)
  ld s8, 80(sp)
  ld s9, 72(sp)
  ld ra, 152(sp)
  ld s0, 144(sp)
  addi sp, sp, 160
  jr ra

