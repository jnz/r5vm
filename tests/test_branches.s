# Test BEQ, BNE, BLT, BGE, BLTU, BGEU instructions

.section .text
.globl _start

_start:
    # === BEQ (Branch if Equal) ===
    # Test 1: Should branch
    li a1, 42
    li a2, 42
    beq a1, a2, beq_ok1
    j fail
beq_ok1:
    # Test 2: Should not branch
    li a1, 10
    li a2, 20
    beq a1, a2, fail

    # === BNE (Branch if Not Equal) ===
    # Test 3: Should branch
    li a1, 10
    li a2, 20
    bne a1, a2, bne_ok1
    j fail
bne_ok1:
    # Test 4: Should not branch
    li a1, 100
    li a2, 100
    bne a1, a2, fail

    # === BLT (Branch if Less Than, signed) ===
    # Test 5: -5 < 3 (should branch)
    li a1, -5
    li a2, 3
    blt a1, a2, blt_ok1
    j fail
blt_ok1:
    # Test 6: 3 < -5 (should not branch)
    li a1, 3
    li a2, -5
    blt a1, a2, fail

    # === BGE (Branch if Greater or Equal, signed) ===
    # Test 7: 10 >= 10 (should branch)
    li a1, 10
    li a2, 10
    bge a1, a2, bge_ok1
    j fail
bge_ok1:
    # Test 8: -10 >= 5 (should not branch)
    li a1, -10
    li a2, 5
    bge a1, a2, fail

    # === BLTU (Branch if Less Than, unsigned) ===
    # Test 9: 5 < 10 unsigned (should branch)
    li a1, 5
    li a2, 10
    bltu a1, a2, bltu_ok1
    j fail
bltu_ok1:
    # Test 10: 0xFFFFFFFF < 10 unsigned (should not branch, -1 is large unsigned)
    li a1, 0xFFFFFFFF
    li a2, 10
    bltu a1, a2, fail

    # === BGEU (Branch if Greater or Equal, unsigned) ===
    # Test 11: 100 >= 50 unsigned (should branch)
    li a1, 100
    li a2, 50
    bgeu a1, a2, bgeu_ok1
    j fail
bgeu_ok1:
    # Test 12: 10 >= 0xFFFFFFFF unsigned (should not branch)
    li a1, 10
    li a2, 0xFFFFFFFF
    bgeu a1, a2, fail

    # All tests passed
    li a0, 0
    li a7, 0
    ecall

fail:
    li a0, 1
    li a7, 0
    ecall
