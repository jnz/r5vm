# Test AND, OR, XOR instructions (R-type)

.section .text
.globl _start

_start:
    # === AND Tests ===
    # Test 1: Basic AND
    li a1, 0xFF00FF00
    li a2, 0x0FF00FF0
    and a3, a1, a2      # a3 = 0x0F000F00
    li t0, 0x0F000F00
    bne a3, t0, fail

    # Test 2: AND with zero
    li a1, 0xFFFFFFFF
    and a4, a1, zero    # a4 = 0
    bne a4, zero, fail

    # Test 3: AND with self
    li a1, 0x12345678
    and a5, a1, a1      # a5 = 0x12345678
    li t0, 0x12345678
    bne a5, a1, fail

    # === OR Tests ===
    # Test 4: Basic OR
    li a1, 0xF0F0F0F0
    li a2, 0x0F0F0F0F
    or t1, a1, a2       # t1 = 0xFFFFFFFF
    li t0, 0xFFFFFFFF
    bne t1, t0, fail

    # Test 5: OR with zero
    li a1, 0xABCDEF01
    or t2, a1, zero     # t2 = 0xABCDEF01
    bne t2, a1, fail

    # === XOR Tests ===
    # Test 6: Basic XOR
    li a1, 0xAAAAAAAA
    li a2, 0x55555555
    xor t3, a1, a2      # t3 = 0xFFFFFFFF
    li t0, 0xFFFFFFFF
    bne t3, t0, fail

    # Test 7: XOR with self (should be zero)
    li a1, 0x12345678
    xor t4, a1, a1      # t4 = 0
    bne t4, zero, fail

    # Test 8: XOR with zero
    li a1, 0xDEADBEEF
    xor t5, a1, zero    # t5 = 0xDEADBEEF
    bne t5, a1, fail

    # All tests passed
    li a0, 0
    li a7, 0
    ecall

fail:
    li a0, 1
    li a7, 0
    ecall
