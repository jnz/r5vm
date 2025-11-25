    .section .text
    .globl _start

_start:
    addi x1, x0, 0      # x1 = 0
    addi x2, x0, 5      # x2 = 5

loop:
    addi x1, x1, 1      # x1++
    blt  x1, x2, loop   # if (x1 < x2) jump loop

    ebreak              # stop

