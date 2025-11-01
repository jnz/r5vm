# Test JAL and JALR instructions

.section .text
.globl _start

_start:
    # Test 1: JAL - Jump and Link
    jal ra, func1       # Jump to func1, save return address in ra
    # Should return here
    li t0, 1
    bne t0, a1, fail    # Check that func1 executed

    # Test 2: JAL with x0 (no link)
    jal zero, func2     # Jump without saving return
    # Should never reach here
    j fail

after_func2:
    li t0, 2
    bne t0, a2, fail    # Check that func2 executed

    # Test 3: JALR - Jump and Link Register
    la t1, func3
    jalr ra, t1, 0      # Jump to address in t1
    # Should return here
    li t0, 3
    bne t0, a3, fail    # Check that func3 executed

    # Test 4: JALR with offset
    la t1, func4
    addi t1, t1, -4     # Offset by -4
    jalr ra, t1, 4      # Jump to t1+4 (effectively func4)
    # Should return here
    li t0, 4
    bne t0, a4, fail    # Check that func4 executed

    # All tests passed
    li a0, 0
    li a7, 0
    ecall

func1:
    li a1, 1            # Mark func1 executed
    jalr zero, ra, 0    # Return (jump to ra)

func2:
    li a2, 2            # Mark func2 executed
    j after_func2       # Jump directly (not return)

func3:
    li a3, 3            # Mark func3 executed
    jalr zero, ra, 0    # Return

func4:
    li a4, 4            # Mark func4 executed
    jalr zero, ra, 0    # Return

fail:
    li a0, 1
    li a7, 0
    ecall
