    .text
    .globl _start
_start:
    call main
    mv a1, a0
    li a0, 0
    li a7, 93
    ecall
