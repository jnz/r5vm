.intel_syntax noprefix

# test assembler for RISC-V Store (SW)

# addr = R[rs1] + IMM_S
# vm->mem[addr] = R[rs2]  (32-Bit)

.global _main
_main:
    mov eax, [edi + 0xcc] # 0xcc = rs1
    add eax, 0x1111111   # IMM_I (32-bit)
    and eax, 0xFFFFFF # immediate and mask
    add eax, [edi + 0xFF] # 0xFF = OFF_MEM
    mov ebx, [edi + 0xdd] # 0xdd = rs2
    mov [eax], ebx

    mov DWORD PTR [edi + 0x0c], 0xf0
    mov DWORD PTR [edi + 0x0d], 0x0f
    mov eax, [edi + 0x0c]
    xor eax, [edi + 0x0d]

    ret

