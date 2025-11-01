# Test LUI and AUIPC instructions

.section .text
.globl _start

_start:
    # Test 1: LUI - Load Upper Immediate
    lui a1, 0x12345     # Load 0x12345000 into a1
    li t0, 0x12345000
    bne a1, t0, fail

    # Test 2: LUI with lower bits
    lui a2, 0xABCDE
    addi a2, a2, 0x678  # Combine: 0xABCDE000 + 0x678 = 0xABCDE678
    li t0, 0xABCDE678
    bne a2, t0, fail

    # Test 3: LUI with zero
    lui a3, 0
    bne a3, zero, fail

    # Test 4: AUIPC - Add Upper Immediate to PC
    auipc a4, 0         # a4 = current PC
    # We can't check exact value, but verify it's non-zero
    beq a4, zero, fail

    # Test 5: AUIPC with offset - verify the offset is added
    # Strategy: Two consecutive AUIPCs with different offsets
    auipc t1, 0         # t1 = PC (this instruction)
    auipc t2, 1         # t2 = PC + 0x1000 (this instruction, which is 4 bytes after previous)
    
    # t2 should be t1 + 0x1000 + 4 (because PC advanced by one instruction)
    li t0, 0x1004       # Load the expected offset
    add t3, t1, t0      # Expected value: t1 + 0x1004
    bne t2, t3, fail

    # Test 6: LUI with sign bit
    lui a6, 0x80000     # 0x80000000 (negative in signed interpretation)
    li t0, 0x80000000
    bne a6, t0, fail

    # All tests passed
    li a0, 0
    li a7, 0
    ecall

fail:
    li a0, 1
    li a7, 0
    ecall
