.section .text
.globl _start

_start:

    # x10 = Zeiger auf results-Array (einmalig!)
    la x10, results

    ########################################
    # Test 1: BLT
    ########################################
    addi x5, x0, 0
    addi x6, x0, 5
1:
    addi x5, x5, 1
    blt x5, x6, 1b

    sw   x5, 0(x10)
    addi x10, x10, 4     # next slot


    ########################################
    # Test 2: BGE
    ########################################
    addi x5, x0, 5
    addi x6, x0, 0
2:
    addi x5, x5, -1
    bge  x5, x6, 2b

    sw   x5, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 3: LW
    ########################################
    la   x5, mydata
    lw   x6, 0(x5)
    lw   x7, 4(x5)

    sw   x6, 0(x10)
    addi x10, x10, 4
    sw   x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 4: SW+reload
    ########################################
    la   x5, mydata
    lw   x6, 4(x5)

    la   x7, myout
    sw   x6, 0(x7)
    lw   x8, 0(x7)

    sw   x8, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 5: XOR
    ########################################
    addi x5, x0, 0x55
    addi x6, x0, 0xAA
    xor  x7, x5, x6

    sw   x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 6: OR
    ########################################
    or   x7, x5, x6

    sw   x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 7: AND
    ########################################
    and  x7, x5, x6

    sw   x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 8: SLL
    ########################################
    addi x8, x0, 3
    sll  x7, x5, x8

    sw   x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 9: SRL
    ########################################
    addi x5, x0, 0xF0
    addi x6, x0, 4
    srl  x7, x5, x6

    sw   x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 10: SRA
    ########################################
    addi x5, x0, -16
    addi x6, x0, 2
    sra  x7, x5, x6

    sw   x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 11: SLT
    ########################################
    addi x5, x0, -5
    addi x6, x0, 10
    slt  x7, x5, x6

    sw   x7, 0(x10)
    addi x10, x10, 4

    addi x5, x0, 20
    addi x6, x0, -1
    slt  x7, x5, x6

    sw   x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 12: SLTU
    ########################################
    addi x5, x0, -1
    addi x6, x0, 1
    sltu x7, x5, x6

    sw   x7, 0(x10)
    addi x10, x10, 4

    addi x5, x0, 0
    addi x6, x0, 1
    sltu x7, x5, x6

    sw   x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 13: LOAD/STORE variants
    ########################################
    addi x5, x0, 0xAA
    la   x6, myout8
    sb   x5, 0(x6)
    lb   x7, 0(x6)

    sw   x7, 0(x10)
    addi x10, x10, 4

    addi x5, x0, 0x123
    la   x6, myout16
    sh   x5, 0(x6)
    lh   x7, 0(x6)

    sw   x7, 0(x10)
    addi x10, x10, 4

    # 16/8 bit loads from testbytes
    la   x5, testbytes
    lh   x6, 0(x5)
    lhu  x7, 0(x5)
    lh   x8, 2(x5)
    lhu  x9, 2(x5)
    lb   x11,0(x5)
    lbu  x12,0(x5)
    lb   x13,1(x5)
    lbu  x14,1(x5)

    sw x6, 0(x10);  addi x10, x10, 4
    sw x7, 0(x10);  addi x10, x10, 4
    sw x8, 0(x10);  addi x10, x10, 4
    sw x9, 0(x10);  addi x10, x10, 4
    sw x11,0(x10);  addi x10, x10, 4
    sw x12,0(x10);  addi x10, x10, 4
    sw x13,0(x10);  addi x10, x10, 4
    sw x14,0(x10);  addi x10, x10, 4


    ########################################
    # Test 14: LUI
    ########################################
    lui x5, 0xABCD

    sw   x5, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 15: JAL
    ########################################

    jal  x5, jal_target
    addi x6, x0, 1111    # if JAL fails, we land HERE
jal_target:
    addi x6, x0, 1234    # normal case: correct jump
    sw   x6, 0(x10)      # store marker
    addi x10, x10, 4
    sw   x5, 0(x10)      # return address
    addi x10, x10, 4

    ########################################
    # Test 16: JAL + JALR
    ########################################

    addi x5, x0, 0       # x5 = initial
    jal  x6, func_body   # x6 = return address, PC+4

    # Coming back
    sw x5, 0(x10)
    addi x10, x10, 4
    sw x6, 0(x10)
    addi x10, x10, 4

    ########################################
    # Test 17: SLLI
    ########################################
    addi x5, x0, 0x15    # 0b00010101
    slli x7, x5, 3       # expect 0xA8

    sw   x7, 0(x10)
    addi x10, x10, 4

    # shift by 0
    slli x7, x5, 0       # expect unchanged (0x15)

    sw   x7, 0(x10)
    addi x10, x10, 4

    # write into x0 should be ignored
    slli x0, x5, 4

    ########################################
    # Test 18: SRLI
    ########################################

    # --- Fall 1: normal (0xF0 >> 4 = 0x0F)
    addi x5, x0, 0xF0
    srli x7, x5, 4          # expect 0x0F

    sw   x7, 0(x10)
    addi x10, x10, 4


    # --- Fall 2: shamt = 0 (no change)
    srli x7, x5, 0          # expect 0xF0

    sw   x7, 0(x10)
    addi x10, x10, 4


    # --- Fall 3: großes Muster: x5 = 0x80000000
    lui  x5, 0x80000         # loads 0x80000000
    srli x7, x5, 1           # expect 0x40000000

    sw   x7, 0(x10)
    addi x10, x10, 4


    # --- Fall 4: weiteres Shiften
    # 0x80000000 >> 31 = 1
    srli x7, x5, 31          # expect 1

    sw   x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # Test 19: SRAI
    ########################################
    addi x5, x0, -16
    srai x7, x5, 2         # expect 0xFFFFFFFC

    sw x7, 0(x10)
    addi x10, x10, 4

    addi x5, x0, -1
    srai x7, x5, 5         # expect 0xFFFFFFFF

    sw x7, 0(x10)
    addi x10, x10, 4

    srai x7, x5, 0         # expect unchanged (-1)

    sw x7, 0(x10)
    addi x10, x10, 4

    ########################################
    # Test 20: SLTI
    ########################################
    addi x5, x0, -5
    slti x7, x5, 10        # 1

    sw x7, 0(x10)
    addi x10, x10, 4

    addi x5, x0, 20
    slti x7, x5, -1        # 0 (20 < -1 is false)

    sw x7, 0(x10)
    addi x10, x10, 4

    ########################################
    # Test 21: SLTIU
    ########################################
    addi x5, x0, -1        # 0xFFFFFFFF
    sltiu x7, x5, 1        # 0

    sw x7, 0(x10)
    addi x10, x10, 4

    addi x5, x0, 0
    sltiu x7, x5, 1        # 1

    sw x7, 0(x10)
    addi x10, x10, 4


    ########################################
    # TEST END
    ########################################

    ebreak

# -------------------------
# Fake Funktion
# -------------------------
func_body:
    addi x5, x0, 1234       # Marker: Funktion wurde ausgeführt

    # JALR Return
    jalr x0, 0(x6)          # goto return address

############################################################
# Data
############################################################
.section .data

results:
    .space 512   # plenty of space

mydata:
    .word 0x11223344
    .word 0x55667788

myout:
    .word 0
    .word 0

myout8:
    .byte 0x00

myout16:
    .half 0x0000

testbytes:
    .byte 0x80
    .byte 0x7F
    .byte 0xFF
    .byte 0x01

