# Test ADD instruction (R-type)

.section .text
.globl _start

_start:
    # Test 1: Simple addition
    li a1, 10
    li a2, 20
    add a3, a1, a2      # a3 = 10 + 20 = 30
    li t0, 30
    bne a3, t0, fail

    # Test 2: Addition with zero
    li a1, 42
    add a4, a1, zero    # a4 = 42 + 0 = 42
    li t0, 42
    bne a4, t0, fail

    # Test 3: Negative numbers
    li a1, -5
    li a2, 3
    add a5, a1, a2      # a5 = -5 + 3 = -2
    li t0, -2
    bne a5, t0, fail

    # Test 4: Overflow (wraps around)
    li a1, 0x7FFFFFFF   # Max positive int32
    li a2, 1
    add t1, a1, a2      # t1 = 0x80000000 (wraps to negative)
    li t0, 0x80000000
    bne t1, t0, fail

    # Test 5: All zeros
    add t2, zero, zero  # t2 = 0
    bne t2, zero, fail

    # All tests passed
    li a0, 0            # success code
    li a7, 0            # exit syscall
    ecall

fail:
    li a0, 1            # failure code
    li a7, 0            # exit syscall
    ecall
