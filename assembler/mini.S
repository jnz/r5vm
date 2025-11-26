    .section .text
    .globl _start

_start:
    # ---- Test 1: BLT ----------------------
    addi x1, x0, 0      # x1 = 0
    addi x2, x0, 5      # x2 = 5

loop_blt:
    addi x1, x1, 1      # x1++
    blt  x1, x2, loop_blt   # while x1 < x2

    # ---- Test 2: BGE ---------------------
    addi x3, x0, 5      # x3 = 5
    addi x4, x0, 0      # x4 = 0

loop_bge:
    addi x3, x3, -1     # x3--
    bge  x3, x4, loop_bge   # while x3 >= 0

    # ---- Test 3: LW ----------------------
    #   x5 = Address of mydata
    #   x6 = Value at offset 0 (0x11)
    #   x7 = Value at offset 4 (0x22)

    la   x5, mydata     # x5 = &mydata
    lw   x6, 0(x5)      # x6 = 0x11223344
    lw   x7, 4(x5)      # x7 = 0x55667788

    ebreak

    # ----------------- LW-Test ------------
    .section .data
mydata:
    .word 0x11223344
    .word 0x55667788

