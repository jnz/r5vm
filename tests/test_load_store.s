# Test LW, LH, LB, LHU, LBU, SW, SH, SB instructions

.section .text
.globl _start

_start:
    # Set up stack pointer to safe area
    li sp, 0x8000

    # === Word (32-bit) operations ===
    # Test 1: SW/LW
    li a1, 0xDEADBEEF
    sw a1, 0(sp)        # Store word
    lw a2, 0(sp)        # Load word
    bne a1, a2, fail

    # === Halfword (16-bit) operations ===
    # Test 2: SH/LH (signed)
    li a1, 0xFFFF8000   # -32768
    sh a1, 4(sp)        # Store halfword
    lh a2, 4(sp)        # Load halfword (sign-extended)
    li t0, 0xFFFF8000
    bne a2, t0, fail

    # Test 3: SH/LHU (unsigned)
    li a1, 0x0000FFFF
    sh a1, 8(sp)        # Store halfword
    lhu a2, 8(sp)       # Load halfword unsigned
    li t0, 0x0000FFFF
    bne a2, t0, fail

    # === Byte (8-bit) operations ===
    # Test 4: SB/LB (signed)
    li a1, 0xFFFFFF80   # -128
    sb a1, 12(sp)       # Store byte
    lb a2, 12(sp)       # Load byte (sign-extended)
    li t0, 0xFFFFFF80
    bne a2, t0, fail

    # Test 5: SB/LBU (unsigned)
    li a1, 0x000000FF
    sb a1, 16(sp)       # Store byte
    lbu a2, 16(sp)      # Load byte unsigned
    li t0, 0x000000FF
    bne a2, t0, fail

    # Test 6: Unaligned access (byte-level)
    li a1, 0x12345678
    sw a1, 20(sp)
    lb a2, 20(sp)       # Load lowest byte
    li t0, 0x78
    bne a2, t0, fail
    lb a3, 21(sp)       # Load second byte
    li t0, 0x56
    bne a3, t0, fail

    # Test 7: Multiple stores
    li a1, 0xAA
    li a2, 0xBB
    li a3, 0xCC
    li a4, 0xDD
    sb a1, 30(sp)
    sb a2, 31(sp)
    sb a3, 32(sp)
    sb a4, 33(sp)
    lw a5, 30(sp)       # Load as word: 0xDDCCBBAA
    li t0, 0xDDCCBBAA
    bne a5, t0, fail

    # All tests passed
    li a0, 0
    li a7, 0
    ecall

fail:
    li a0, 1
    li a7, 0
    ecall
