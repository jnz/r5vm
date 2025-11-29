.section .text
.globl _start

_start:

    ########################################
    # Test 1: BLT
    ########################################
    addi x5, x0, 0
    addi x6, x0, 5
1:
    addi x5, x5, 1
    blt x5, x6, 1b

    la x10, results
    sw x5, 0(x10)                 # expected = 5


    ########################################
    # Test 2: BGE
    ########################################
    addi x5, x0, 5
    addi x6, x0, 0
2:
    addi x5, x5, -1
    bge x5, x6, 2b

    la x10, results
    addi x10, x10, 4
    sw x5, 0(x10)                 # expected = -1


    ########################################
    # Test 3: LW
    ########################################
    la x5, mydata
    lw x6, 0(x5)
    lw x7, 4(x5)

    la x10, results
    addi x10, x10, 8
    sw x6, 0(x10)                 # = 0x11223344
    sw x7, 4(x10)                 # = 0x55667788


    ########################################
    # Test 4: SW + reload
    ########################################
    la x5, mydata
    lw x6, 4(x5)                  # 0x55667788

    la x7, myout
    sw x6, 0(x7)
    lw x8, 0(x7)

    la x10, results
    addi x10, x10, 16
    sw x8, 0(x10)                 # stored+loaded value


    ########################################
    # Test 5: XOR
    ########################################
    addi x5, x0, 0x55
    addi x6, x0, 0xAA
    xor  x7, x5, x6               # 0xFF

    la x10, results
    addi x10, x10, 20
    sw x7, 0(x10)


    ########################################
    # Test 6: OR
    ########################################
    or x7, x5, x6                 # 0xFF

    la x10, results
    addi x10, x10, 24
    sw x7, 0(x10)


    ########################################
    # Test 7: AND
    ########################################
    and x7, x5, x6                # 0x00

    la x10, results
    addi x10, x10, 28
    sw x7, 0(x10)


    ########################################
    # Test 8: SLL
    ########################################
    addi x8, x0, 3
    sll  x7, x5, x8               # 0x55 << 3 = 0x2A8

    la x10, results
    addi x10, x10, 32
    sw x7, 0(x10)


    ########################################
    # Test 9: SRL
    ########################################
    addi x5, x0, 0xF0
    addi x6, x0, 4
    srl x7, x5, x6                # expect 0x0F

    la x10, results
    addi x10, x10, 36
    sw x7, 0(x10)


    ########################################
    # Test 10: SRA
    ########################################
    addi x5, x0, -16
    addi x6, x0, 2
    sra x7, x5, x6                # expect 0xFFFFFFFC

    la x10, results
    addi x10, x10, 40
    sw x7, 0(x10)


    ########################################
    # Test 11: SLT (signed)
    ########################################
    addi x5, x0, -5
    addi x6, x0, 10
    slt x7, x5, x6                # =1

    la x10, results
    addi x10, x10, 44
    sw x7, 0(x10)

    # Second case
    addi x5, x0, 20
    addi x6, x0, -1
    slt x7, x5, x6                # =0

    la x10, results
    addi x10, x10, 48
    sw x7, 0(x10)


    ########################################
    # Test 12: SLTU
    ########################################
    addi x5, x0, -1
    addi x6, x0, 1
    sltu x7, x5, x6               # =0

    la x10, results
    addi x10, x10, 52
    sw x7, 0(x10)

    addi x5, x0, 0
    addi x6, x0, 1
    sltu x7, x5, x6               # =1

    la x10, results
    addi x10, x10, 56
    sw x7, 0(x10)


    ########################################
    # Test 13: STORE/LOAD variants
    ########################################
    addi x5, x0, 0xAA
    la   x6, myout8
    sb x5, 0(x6)
    lb x7, 0(x6)

    la x10, results
    addi x10, x10, 60
    sw x7, 0(x10)

    addi x5, x0, 0x123
    la   x6, myout16
    sh x5, 0(x6)
    lh x7, 0(x6)

    la x10, results
    addi x10, x10, 64
    sw x7, 0(x10)

    # Signed/unsigned loads from testbytes
    la   x5, testbytes
    lh   x6, 0(x5)
    lhu  x7, 0(x5)
    lh   x8, 2(x5)
    lhu  x9, 2(x5)
    lb   x11,0(x5)
    lbu  x12,0(x5)
    lb   x13,1(x5)
    lbu  x14,1(x5)

    la x10, results
    addi x10, x10, 68
    sw x6, 0(x10)
    sw x7, 4(x10)
    sw x8, 8(x10)
    sw x9, 12(x10)
    sw x11,16(x10)
    sw x12,20(x10)
    sw x13,24(x10)
    sw x14,28(x10)


    ########################################
    # Test 14: LUI
    ########################################
    lui x5, 0xABCD

    la x10, results
    addi x10, x10, 100
    sw x5, 0(x10)


    ebreak


############################################################
# Data
############################################################
.section .data

results:
    .space 512          # genug Platz für viele Tests

# Deine bisherigen Daten:
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
