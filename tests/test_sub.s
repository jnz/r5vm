# Test SUB instruction (R-type)

.section .text
.globl _start

_start:
    # Test 1: Simple subtraction
    li a1, 50
    li a2, 30
    sub a3, a1, a2      # a3 = 50 - 30 = 20
    li t0, 20
    bne a3, t0, fail

    # Test 2: Result is zero
    li a1, 123
    sub a4, a1, a1      # a4 = 123 - 123 = 0
    bne a4, zero, fail

    # Test 3: Negative result
    li a1, 10
    li a2, 20
    sub a5, a1, a2      # a5 = 10 - 20 = -10
    li t0, -10
    bne a5, t0, fail

    # Test 4: Subtract from zero
    li a2, 42
    sub t1, zero, a2    # t1 = 0 - 42 = -42
    li t0, -42
    bne t1, t0, fail

    # Test 5: Underflow (wraps around)
    li a1, 0x80000000   # Min negative int32
    li a2, 1
    sub t2, a1, a2      # t2 = 0x7FFFFFFF (wraps to positive)
    li t0, 0x7FFFFFFF
    bne t2, t0, fail

    # All tests passed
    li a0, 0
    li a7, 0
    ecall

fail:
    li a0, 1
    li a7, 0
    ecall
