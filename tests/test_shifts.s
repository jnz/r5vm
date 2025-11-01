# Test SLL, SRL, SRA instructions (R-type and I-type)

.section .text
.globl _start

_start:
    # === SLL (Shift Left Logical) Tests ===
    # Test 1: Basic left shift
    li a1, 1
    li a2, 4
    sll a3, a1, a2      # a3 = 1 << 4 = 16
    li t0, 16
    bne a3, t0, fail

    # Test 2: SLLI (immediate)
    li a1, 3
    slli a4, a1, 3      # a4 = 3 << 3 = 24
    li t0, 24
    bne a4, t0, fail

    # Test 3: Shift by 0
    li a1, 0xABCDEF01
    sll a5, a1, zero    # a5 = 0xABCDEF01
    bne a5, a1, fail

    # === SRL (Shift Right Logical) Tests ===
    # Test 4: Basic right shift
    li a1, 0x80000000
    li a2, 4
    srl t1, a1, a2      # t1 = 0x08000000 (unsigned)
    li t0, 0x08000000
    bne t1, t0, fail

    # Test 5: SRLI (immediate)
    li a1, 128
    srli t2, a1, 3      # t2 = 128 >> 3 = 16
    li t0, 16
    bne t2, t0, fail

    # === SRA (Shift Right Arithmetic) Tests ===
    # Test 6: Sign extension
    li a1, 0x80000000   # Negative number
    li a2, 4
    sra t3, a1, a2      # t3 = 0xF8000000 (sign extended)
    li t0, 0xF8000000
    bne t3, t0, fail

    # Test 7: SRAI (immediate)
    li a1, -16
    srai t4, a1, 2      # t4 = -16 >> 2 = -4
    li t0, -4
    bne t4, t0, fail

    # Test 8: Positive number with SRA
    li a1, 0x7FFFFFFF
    srai t5, a1, 1      # t5 = 0x3FFFFFFF
    li t0, 0x3FFFFFFF
    bne t5, t0, fail

    # All tests passed
    li a0, 0
    li a7, 0
    ecall

fail:
    li a0, 1
    li a7, 0
    ecall
