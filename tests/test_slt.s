# Test SLT, SLTI, SLTU, SLTIU instructions

.section .text
.globl _start

_start:
    # === SLT (Set Less Than, signed) ===
    # Test 1: -5 < 3 (true)
    li a1, -5
    li a2, 3
    slt a3, a1, a2      # a3 = 1
    li t0, 1
    bne a3, t0, fail

    # Test 2: 10 < 5 (false)
    li a1, 10
    li a2, 5
    slt a4, a1, a2      # a4 = 0
    bne a4, zero, fail

    # Test 3: Equal values
    li a1, 42
    li a2, 42
    slt a5, a1, a2      # a5 = 0
    bne a5, zero, fail

    # === SLTI (Set Less Than Immediate, signed) ===
    # Test 4: 5 < 10 (true)
    li a1, 5
    slti t1, a1, 10     # t1 = 1
    li t0, 1
    bne t1, t0, fail

    # Test 5: -10 < -5 (true)
    li a1, -10
    slti t2, a1, -5     # t2 = 1
    li t0, 1
    bne t2, t0, fail

    # === SLTU (Set Less Than Unsigned) ===
    # Test 6: 5 < 10 unsigned (true)
    li a1, 5
    li a2, 10
    sltu t3, a1, a2     # t3 = 1
    li t0, 1
    bne t3, t0, fail

    # Test 7: 0xFFFFFFFF < 10 unsigned (false, -1 is large unsigned)
    li a1, 0xFFFFFFFF
    li a2, 10
    sltu t4, a1, a2     # t4 = 0
    bne t4, zero, fail

    # Test 8: 10 < 0xFFFFFFFF unsigned (true)
    li a1, 10
    li a2, 0xFFFFFFFF
    sltu t5, a1, a2     # t5 = 1
    li t0, 1
    bne t5, t0, fail

    # === SLTIU (Set Less Than Immediate Unsigned) ===
    # Test 9: 5 < 10 unsigned (true)
    li a1, 5
    sltiu s0, a1, 10    # s0 = 1
    li t0, 1
    bne s0, t0, fail

    # Test 10: 100 < 50 unsigned (false)
    li a1, 100
    sltiu s1, a1, 50    # s1 = 0
    bne s1, zero, fail

    # Test 11: Sign extension edge case
    # SLTIU sign-extends the immediate, so -1 becomes 0xFFFFFFFF
    li a1, 0xFFFFFFFE   # Large unsigned value
    sltiu s2, a1, -1    # Compare with 0xFFFFFFFF
    li t0, 1            # 0xFFFFFFFE < 0xFFFFFFFF
    bne s2, t0, fail

    # All tests passed
    li a0, 0
    li a7, 0
    ecall

fail:
    li a0, 1
    li a7, 0
    ecall
