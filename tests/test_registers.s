# Test that validates specific register values
# This test demonstrates register state checking

.section .text
.globl _start

_start:
    # Load known values into registers
    li a0, 0            # Must be 0 for success
    li a1, 0x12345678
    li a2, 0xDEADBEEF
    li a3, 42
    li a4, -100
    li t0, 0xAAAAAAAA
    li t1, 0x55555555

    # Perform some operations
    add s0, a1, a3      # s0 = 0x123456A2
    sub s1, a2, a3      # s1 = 0xDEADBEC5
    and s2, t0, t1      # s2 = 0x00000000
    or  s3, t0, t1      # s3 = 0xFFFFFFFF
    xor s4, t0, t1      # s4 = 0xFFFFFFFF

    # Exit successfully
    li a7, 0
    ecall
