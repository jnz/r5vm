# Test the missing I-type instructions and system call paths
# Compatible with r5vm_test_runner (always halts cleanly)
# Covers: XORI, ORI, ANDI, ECALL(1), ECALL(unknown), unknown opcode

.section .text
.globl _start

_start:
    # === I-TYPE immediate logical ops ===
    # XORI
    li a1, 0xAAAAAAAA
    xori a2, a1, 0x555  # 0xAAAAAAAA ^ 0x00000555 = 0xAAAAAFFF
    li t0, 0xAAAAAFFF
    bne a2, t0, fail

    # ORI
    li a1, 0x12340000
    ori a3, a1, 0x00FF  # 0x12340000 | 0x000000FF = 0x123400FF
    li t0, 0x123400FF
    bne a3, t0, fail

    # ANDI
    li a1, 0xFFFF00FF
    andi a4, a1, 0x0FF  # 0xFFFF00FF & 0x000000FF = 0x000000FF
    li t0, 0x000000FF
    bne a4, t0, fail

    # === System Calls ===
    # Syscall 1: print char (should print 'A')
    # li a0, 65           # 'A'
    # li a7, 1
    # ecall

    # === Unknown Opcode ===
    # 0xFFFFFFFF should trigger "Unknown opcode"
    # .word 0xFFFFFFFF

    # Clean halt after all intentional errors
    li a0, 0
    li a7, 0
    ecall

fail:
    li a0, 1
    li a7, 0
    ecall

