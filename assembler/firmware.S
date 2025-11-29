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
    la   x5, mydata     # x5 = &mydata
    lw   x6, 0(x5)      # x6 = 0x11223344
    lw   x7, 4(x5)      # x7 = 0x55667788

    # ---- Test 4: SW ----------------------
    la   x8, myout      # x8 = &myout
    sw   x6, 0(x8)      # myout[0] = x6
    sw   x7, 4(x8)      # myout[1] = x7
    lw   x6, 4(x8)      # x6 = myout[1]

    # ---- Test 5: XOR ---------------------
    xor  x9, x6, x7     # x9 = x6 ^ x7

    addi x10, x0, 0x55
    addi x11, x0, 0xAA
    xor  x12, x10, x11   # x12 = 0xFF

    # ---- Test 6: OR ----------------------
    # Test: OR sollte Bits setzen
    # x13 = x10 | x11  (0x55 | 0xAA = 0xFF)
    or   x13, x10, x11

    # ---- Test 7: AND ---------------------
    # x14 = x10 & x11  (0x55 & 0xAA = 0x00)
    and  x14, x10, x11

    # ---- Test 8: SLL ---------------------
    # x15 = x10 << 3   (0x55 << 3 = 0x2A8)
    addi x16, x0, 3
    sll  x15, x10, x16

    # ---- Test 9: SRL ---------------------
    addi x20, x0, 0xF0       # 0b11110000
    addi x21, x0, 4
    srl  x22, x20, x21       # expected 0x0F

    # ---- Test 10: SRA --------------------
    addi x23, x0, -16        # 0xFFFFFFF0 => signed negative
    addi x24, x0, 2
    sra  x25, x23, x24       # expected 0xFFFFFFFC  (sign-extended)

    # ---- Test 11: SLT (signed) ----------
    addi x13, x0, -5         # x13 = -5
    addi x14, x0, 10         # x14 = 10
    slt  x15, x13, x14       # expected: x15 = 1 (x13 < x14)

    addi x16, x0, 20 # x16 = 20
    addi x17, x0, -1 # x17 = -1
    slt  x18, x16, x17     # expected: x18 = 0

    # ---- Test 12: SLTU (unsigned) -------
    # x19 = 0xFFFFFFFF (unsigned)
    # x20 = 1
    # x21 = (x19 < x20) => 0   (unsigned!)
    addi x19, x0, -1        # -1 as 0xFFFFFFFF
    addi x20, x0, 1
    sltu x21, x19, x20      # expected: x21 = 0

    addi x22, x0, 0 # x22 = 0
    addi x23, x0, 1 # x23 = 1
    sltu x24, x22, x23      # expected: x24 = 1

    # ---- Test 13: SB (8-bit Store) --------
    addi x20, x0, 0xAA       # x20 = 0xAA
    la   x21, myout8         # x21 = &myout8
    sb   x20, 0(x21)         # myout8[0] = 0xAA
    # reload to verify
    lb   x22, 0(x21)         # x22 = 0xAA

    # ---- Test 14: SH (16-bit Store) -------
    addi x23, x0, 0x1234     # x23 = 0x1234
    la   x24, myout16        # x24 = &myout16
    sh   x23, 0(x24)         # myout16[0] = 0x1234
    # reload to verify
    lh   x25, 0(x24)         # x25 = 0x1234

    # ----------------- End --------------
    ebreak

    # ----------------- Data -------------
    .section .data
mydata:
    .word 0x11223344
    .word 0x55667788

myout:
    .word 0
    .word 0

