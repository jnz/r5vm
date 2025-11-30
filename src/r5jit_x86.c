/*
 * R5VM - Minimal RISC-V RV32I Virtual Machine
 *       _____
 *      | ____|
 *  _ __| |____   ___ __ ___
 * | '__|___ \ \ / / '_ ` _ \
 * | |   ___) \ V /| | | | | |
 * |_|  |____/ \_/ |_| |_| |_|
 *
 * Copyright (c) 2025 Jan Zwiener
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
    Important:
    edi register points to r5vm_t* vm for JIT code
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h> /* offsetof(...) */

#include "r5vm.h"
#include "r5jit.h"
#include "hires_time.h"

// ---- Defines ---------------------------------------------------------------

#define OFF_PC    (offsetof(r5vm_t, pc)) /* pc offset (bytes) in vm_t */
#define OFF_X(n)  ((uint8_t)offsetof(r5vm_t, regs) + (n)*4)
#define OFF_MEM   (offsetof(r5vm_t, mem))
#define OFF_MASK  (offsetof(r5vm_t, mem_mask))

// ---- x86 Emit --------------------------------------------------------------

static void emit1(r5jitbuf_t* jit, uint8_t v) {
    assert(jit->pos < jit->mem_size);
    if (jit->pos < jit->mem_size) {
        jit->mem[ jit->pos++ ] = v;
    }
    else {
        jit->error = true;
    }
}

static void emit4(r5jitbuf_t* jit, uint32_t v) {
    emit1(jit, (v >>  0) & 0xff );
    emit1(jit, (v >>  8) & 0xff );
    emit1(jit, (v >> 16) & 0xff );
    emit1(jit, (v >> 24) & 0xff );
}

static uint8_t hex(uint8_t c) {
    if ( c >= 'a' && c <= 'f' ) {
        return 10 + c - 'a';
    }
    if ( c >= 'A' && c <= 'F' ) {
        return 10 + c - 'A';
    }
    if ( c >= '0' && c <= '9' ) {
        return c - '0';
    }
    assert(0);
    return 0;
}

static void emit(r5jitbuf_t* jit, const char *string) {
    int c1, c2;
    while (true) {
        c1 = string[0];
        c2 = string[1];
        emit1(jit, (hex(c1) << 4) | hex(c2));
        if (string[2] == '\0')
            break;
        string += 3;
    }
}

static uint32_t calc_rel32(r5jitbuf_t* b, void* target)
{
    uint8_t* next = &b->mem[b->pos] + 4;  // position after imm32
    return (uint32_t)((uint8_t*)target - next);
}

void __cdecl r5vm_handle_ecall(r5vm_t* vm) /* emit_ecall is using a cdecl call */
{
    if (vm->a7 == 1) {
        putchar(vm->a0);
    }
}

// ---- Op Codes --------------------------------------------------------------

static void r5jit_emit_prolog(r5jitbuf_t* b, const r5vm_t* vm) {
    emit1(b, 0x57);                // push edi
    emit1(b, 0x53);                // push ebx
    emit1(b, 0xBF); emit4(b, (uint32_t)vm);  // mov edi, vm
}

static void r5jit_emit_epilog(r5jitbuf_t* b) {
    emit1(b, 0x5B);   // pop ebx
    emit1(b, 0x5F);   // pop edi
    emit1(b, 0xC3);   // ret
}

static void r5jit_exec(r5vm_t* vm, r5jitbuf_t* jit)
{
    (void)vm;
    r5jitfn_t func = (r5jitfn_t)jit->mem;
    func();
}

static void emit_add(r5jitbuf_t* b, int rd, int reg1, int reg2) {
    if (rd == 0) { return; }
    // mov eax, [edi + disp8]
    // Byte encoding: 8B 47 xx (8-bit displacement from edi)
    // 8B: MOV (32bit), ModRM: 0x47 mod=01 (disp8), reg=000 (EAX), rm=111 (EDI)
    emit(b, "8B 47");
    emit1(b, OFF_X(reg1)); // register between 0 and 31
    emit(b, "03 47");      // add eax, [edi + disp8]
    emit1(b, OFF_X(reg2));
    emit(b, "89 47");      // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_addi(r5jitbuf_t* b, int rd, int reg1, int imm) {
    if (rd == 0) { return; }
    // mov eax, [edi + disp8]
    // Byte encoding: 8B 47 xx (8-bit displacement from edi)
    // 8B: MOV (32bit), ModRM: 0x47 mod=01 (disp8), reg=000 (EAX), rm=111 (EDI)
    emit(b, "8B 47");
    emit1(b, OFF_X(reg1)); // register between 0 and 31
    if (imm != 0) {
        emit1(b, 0x05);    // add eax, imm32
        emit4(b, imm);
    }
    emit(b, "89 47");      // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_xori(r5jitbuf_t* b, int rd, int rs1, int imm) {
    if (rd == 0) { return; }
    // R5VM_I_F3_XORI:  R[rd] = R[rs1] ^ IMM_I(inst); break;
    emit(b, "8B 47");     // mov eax, reg1
    emit1(b, OFF_X(rs1)); // register between 0 and 31
    emit(b, "35");        // xor eax, IMM
    emit4(b, imm);
    emit(b, "89 47");     // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_ori(r5jitbuf_t* b, int rd, int rs1, int imm) {
    if (rd == 0) { return; }
    // R5VM_I_F3_ORI:   R[rd] = R[rs1] | IMM_I(inst); break;
    emit(b, "8B 47");     // mov eax, reg1
    emit1(b, OFF_X(rs1)); // register between 0 and 31
    emit(b, "0d");        // or eax, IMM
    emit4(b, imm);
    emit(b, "89 47");     // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_andi(r5jitbuf_t* b, int rd, int rs1, int imm) {
    if (rd == 0) { return; }
    // R5VM_I_F3_ANDI:  R[rd] = R[rs1] & IMM_I(inst); break;
    emit(b, "8B 47");     // mov eax, reg1
    emit1(b, OFF_X(rs1)); // register between 0 and 31
    emit(b, "25");        // and eax, IMM
    emit4(b, imm);
    emit(b, "89 47");     // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_slti(r5jitbuf_t* b, int rd, int rs1, int imm) {
    if (rd == 0) { return; }
    // R5VM_I_F3_SLTI:  R[rd] = ((int32_t)R[rs1] < IMM_I(inst)); break;
    emit(b, "8B 47");     // eax = R[rs1]
    emit1(b, OFF_X(rs1));
    emit(b, "3d");        // cmp eax, imm32 (signed compare)
    emit4(b, imm);
    emit(b, "0F 9C C0");  // setl al   (signed <)
    emit(b, "0F B6 C0");  // movzx eax, al
    emit(b, "89 47");     // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_sltiu(r5jitbuf_t* b, int rd, int rs1, int imm) {
    if (rd == 0) { return; }
    // R5VM_I_F3_SLTIU: R[rd] = (R[rs1] < (uint32_t)IMM_I(inst)); break;
    emit(b, "8B 47");     // eax = R[rs1]
    emit1(b, OFF_X(rs1));
    emit(b, "3D");        // Compare unsigned: cmp eax, imm
    emit4(b, (uint32_t)imm);
    emit(b, "0F 92 C0");  // setb al   (unsigned <)
    emit(b, "0F B6 C0");  // movzx eax, al
    emit(b, "89 47");     // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_slli(r5jitbuf_t* b, int rd, int rs1, int imm) {
    if (rd == 0) { return; }
    // R[rd] = R[rs1] << (IMM_I(inst) & 0x1F);
    emit(b, "8B 47");     // eax = R[rs1]
    emit1(b, OFF_X(rs1));
    if ((imm & 0x1f) != 0) {
        emit(b, "C1 E0"); // shl eax, imm & 0x1f
        emit1(b, imm & 0x1f);
    }
    emit(b, "89 47");     // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_srli(r5jitbuf_t* b, int rd, int rs1, int imm) {
    if (rd == 0) { return; }
    // R[rd] = R[rs1] >> (IMM_I(inst) & 0x1F);
    emit(b, "8B 47");     // eax = R[rs1]
    emit1(b, OFF_X(rs1));
    if ((imm & 0x1f) != 0) {
        emit(b, "C1 E8"); // shr eax, shamt (logical)
        emit1(b, imm & 0x1f); // shift amount
    }
    emit(b, "89 47");     // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_srai(r5jitbuf_t* b, int rd, int rs1, int imm) {
    if (rd == 0) { return; }
    // R[rd] = ((int32_t)R[rs1]) >> (IMM_I(inst) & 0x1F);
    emit(b, "8B 47");     // eax = R[rs1]
    emit1(b, OFF_X(rs1));
    if ((imm & 0x1f) != 0) {
        emit(b, "C1 F8"); // sar eax, shamt (arith signed)
        emit1(b, imm & 0x1f); // shift amount
    }
    emit(b, "89 47");     // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_sub(r5jitbuf_t* b, int rd, int rs1, int rs2) {
    if (rd == 0) { return; }
    // mov eax, [edi + disp8]
    // Byte encoding: 8B 47 xx (8-bit displacement from edi)
    // 8B: MOV (32bit), ModRM: 0x47 mod=01 (disp8), reg=000 (EAX), rm=111 (EDI)
    emit(b, "8B 47");
    emit1(b, OFF_X(rs1)); // register between 0 and 31
    emit(b, "2B 47"); // sub eax, [edi + disp8]
    emit1(b, OFF_X(rs2));
    emit(b, "89 47");   // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_beq(const r5vm_t* vm, r5jitbuf_t* b,
                     int rs1, int rs2, int immb) {
    // R5VM_B_F3_BEQ:  if (R[rs1] == R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
    int target_pc = (vm->pc-4 + immb) & vm->mem_mask;
    emit(b, "8b 47"); emit1(b, OFF_X(rs1));  // eax = R[rs1] (mov eax, [edi + rs1*4])
    emit(b, "8b 5f"); emit1(b, OFF_X(rs2));  // ebx = R[rs2]
    emit(b, "39 D8"); // cmp eax, ebx
    emit(b, "75 06"); // jne (conditional jump over next 6 bytes)
    emit(b, "FF 25"); // jmp [jit->instruction_pointers + target_pc]
    emit4(b, (uint32_t)(b->instruction_pointers + target_pc)); // x86-32 bit only

    /* Debug Printf
    printf("BEQ Jump target r5_pc %05x addr: [%p] = 0x%08x)\n",
        target_pc, (void*)(b->instruction_pointers + target_pc),
        b->instruction_pointers[target_pc]);
    */
}

static void emit_bne(const r5vm_t* vm, r5jitbuf_t* b,
                     int rs1, int rs2, int immb)
{
    // R5VM_B_F3_BNE:  // if (R[rs1] != R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
    int target_pc = (vm->pc - 4 + immb) & vm->mem_mask;
    emit(b, "8b 47"); emit1(b, OFF_X(rs1));  // eax = R[rs1] (mov eax, [edi + rs1*4])
    emit(b, "8b 5f"); emit1(b, OFF_X(rs2));  // ebx = R[rs2]
    emit(b, "39 D8"); // cmp eax, ebx
    emit(b, "74 06"); // je +6 (skip jump if equal)
    emit(b, "FF 25"); // jmp [instruction_pointers + target_pc]
    emit4(b, (uint32_t)(b->instruction_pointers + target_pc));
}

static void emit_bltu(const r5vm_t* vm, r5jitbuf_t* b,
                      int rs1, int rs2, int immb)
{
    // R5VM_B_F3_BLTU: // if (R[rs1] <  R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
    int target_pc = (vm->pc - 4 + immb) & vm->mem_mask;
    emit(b, "8b 47"); emit1(b, OFF_X(rs1));  // eax = R[rs1] (mov eax, [edi + rs1*4])
    emit(b, "8b 5f"); emit1(b, OFF_X(rs2));  // ebx = R[rs2]
    emit(b, "39 D8"); // cmp eax, ebx
    emit(b, "73 06"); // jae +6 (unsigned: if eax >= ebx, skip)
    emit(b, "FF 25"); // jmp [instruction_pointers + target_pc]
    emit4(b, (uint32_t)(b->instruction_pointers + target_pc));
}

static void emit_bgeu(const r5vm_t* vm, r5jitbuf_t* b,
                      int rs1, int rs2, int immb)
{
    // R5VM_B_F3_BGEU: // if (R[rs1] >= R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
    int target_pc = (vm->pc - 4 + immb) & vm->mem_mask;
    emit(b, "8b 47"); emit1(b, OFF_X(rs1));  // eax = R[rs1] (mov eax, [edi + rs1*4])
    emit(b, "8b 5f"); emit1(b, OFF_X(rs2));  // ebx = R[rs2]
    emit(b, "39 D8"); // cmp eax, ebx
    emit(b, "72 06"); // jb +6 (unsigned: skip if eax < ebx)
    emit(b, "FF 25"); // jmp [instruction_pointers + target_pc]
    emit4(b, (uint32_t)(b->instruction_pointers + target_pc));
}

static void emit_blt(const r5vm_t* vm, r5jitbuf_t* b,
                     int reg1, int reg2, int immb)
{
    // R5VM_B_F3_BLT:  if ((int32_t)R[rs1] <  (int32_t)R[rs2]) vm->pc = (vm->pc-4 + IMM_B(inst)) & vm->mem_mask; break;
    int target_pc = (vm->pc-4 + immb) & vm->mem_mask;
    emit(b, "8B 47"); emit1(b, OFF_X(reg1)); // eax = rs1 | mov eax, [edi + OFF_X(rs1)] (mit disp8)
    emit(b, "8B 5F"); emit1(b, OFF_X(reg2)); // ebx = rs2 | mov ebx, [edi+off]
    emit(b, "39 D8"); // cmp eax, ebx
    emit(b, "7D 06"); // jge (conditional jump over next 6 bytes)
    emit(b, "FF 25"); // jmp [jit->instruction_pointers + target_pc]
    emit4(b, (uint32_t)(b->instruction_pointers + target_pc)); // x86-32 bit only
}

static void emit_bge(const r5vm_t* vm, r5jitbuf_t* b,
                     int reg1, int reg2, int immb)
{
    // R5VM_B_F3_BGE: if ((int32_t)R[rs1] >= (int32_t)R[rs2]) vm->pc = (vm->pc-4 + IMM_B(inst)) & vm->mem_mask; break;
    int target_pc = (vm->pc - 4 + immb) & vm->mem_mask;

    emit(b, "8B 47"); emit1(b, OFF_X(reg1)); // eax = rs1
    emit(b, "8B 5F"); emit1(b, OFF_X(reg2)); // ebx = rs2
    emit(b, "39 D8"); // cmp eax, ebx
    emit(b, "7C 06"); // jl +6 bytes  (if eax < ebx, skip)
    emit(b, "FF 25"); // jmp [instruction_pointers[target_pc]]
    emit4(b, (uint32_t)(b->instruction_pointers + target_pc));
}

static void emit_lw(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int immb) {
    if (rd == 0) { return; }
    //  const uint32_t addr = (R[rs1] + IMM_I(inst)) & vm->mem_mask;
    //  R5VM_I_F3_LW:  R[rd] = vm->mem[addr]; break;
    emit(b, "8B 47");       // mov eax, [edi + disp8]
    emit1(b, OFF_X(rs1));   // eax = R[rs1] + IMM
    if (immb != 0) {
        emit(b, "05");      // add eax, imm32
        emit4(b, immb);
    }
    emit(b, "25");          // and eax, mem_mask
    emit4(b, vm->mem_mask); // eax &= mem_mask
    emit(b, "8B 9F");       // mov ebx, [edi + disp32]
    emit4(b, OFF_MEM);      // ebx = vm->mem
    // eax = *(uint32_t*)(ebx + eax)
    emit(b, "8B 04 03");    // mov eax, [ebx + eax]
    emit(b, "89 47");       // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));    // store into rd
}

static void emit_lh(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int immb)
{
    if (rd == 0) { return; }
    // signed 16-bit load
    //  const uint32_t addr = (R[rs1] + IMM_I(inst)) & vm->mem_mask;
    //  R5VM_I_F3_LW:  R[rd] = vm->mem[addr]; break;
    emit(b, "8B 47");       // mov eax, [edi + disp8]
    emit1(b, OFF_X(rs1));   // eax = R[rs1] + IMM
    if (immb != 0) {
        emit(b, "05");      // add eax, imm32
        emit4(b, immb);
    }
    emit(b, "25");          // and eax, mem_mask
    emit4(b, vm->mem_mask); // eax &= mem_mask
    emit(b, "8B 9F");       // mov ebx, [edi + disp32]
    emit4(b, OFF_MEM);      // ebx = vm->mem
    // ax = *(int16_t*)(ebx + eax)
    emit(b, "66 8b 04 03"); // mov ax, [ebx + eax]
    emit(b, "98");          // cwde (sign extend AX to EAX)
    emit(b, "89 47");       // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));    // store into rd
}

static void emit_lb(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int immb)
{
    if (rd == 0) { return; }
    // signed 8-bit load
    //  const uint32_t addr = (R[rs1] + IMM_I(inst)) & vm->mem_mask;
    //  R5VM_I_F3_LW:  R[rd] = vm->mem[addr]; break;
    emit(b, "8B 47");       // mov eax, [edi + disp8]
    emit1(b, OFF_X(rs1));   // eax = R[rs1] + IMM
    if (immb != 0) {
        emit(b, "05");      // add eax, imm32
        emit4(b, immb);
    }
    emit(b, "25");          // and eax, mem_mask
    emit4(b, vm->mem_mask); // eax &= mem_mask
    emit(b, "8B 9F");       // mov ebx, [edi + disp32]
    emit4(b, OFF_MEM);      // ebx = vm->mem
    // al = *(int8_t*)(ebx + eax)
    emit(b, "8a 04 03");    // mov al, [ebx + eax]
    emit(b, "66 98");       // cbw (sign extend AL to AX)
    emit(b, "98");          // cwde (sign extend AX to EAX)
    emit(b, "89 47");       // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));    // store into rd
}

static void emit_lhu(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int immb)
{
    if (rd == 0) { return; }
    // unsigned 16-bit load
    //  const uint32_t addr = (R[rs1] + IMM_I(inst)) & vm->mem_mask;
    //  R5VM_I_F3_LW:  R[rd] = vm->mem[addr]; break;
    emit(b, "8B 47");       // mov eax, [edi + disp8]
    emit1(b, OFF_X(rs1));   // eax = R[rs1] + IMM
    if (immb != 0) {
        emit(b, "05");          // add eax, imm32
        emit4(b, immb);
    }
    emit(b, "25");          // and eax, mem_mask
    emit4(b, vm->mem_mask); // eax &= mem_mask
    emit(b, "8B 9F");       // mov ebx, [edi + disp32]
    emit4(b, OFF_MEM);      // ebx = vm->mem
    // ax = *(uint16_t*)(ebx + eax)
    emit(b, "66 8b 04 03"); // mov ax, [ebx + eax]
    emit(b, "25"); emit4(b, 0xffff); // and eax, 0xffff
    emit(b, "89 47");       // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));    // store into rd
}

static void emit_lbu(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int immb)
{
    if (rd == 0) { return; }
    // unsigned 8-bit load
    //  const uint32_t addr = (R[rs1] + IMM_I(inst)) & vm->mem_mask;
    //  R5VM_I_F3_LW:  R[rd] = vm->mem[addr]; break;
    emit(b, "8B 47");       // mov eax, [edi + disp8]
    emit1(b, OFF_X(rs1));   // eax = R[rs1] + IMM
    if (immb != 0) {
        emit(b, "05");      // add eax, imm32
        emit4(b, immb);
    }
    emit(b, "25");          // and eax, mem_mask
    emit4(b, vm->mem_mask); // eax &= mem_mask
    emit(b, "8B 9F");       // mov ebx, [edi + disp32]
    emit4(b, OFF_MEM);      // ebx = vm->mem
    // al = *(uint8_t*)(ebx + eax)
    emit(b, "8a 04 03");    // mov al, [ebx + eax]
    emit(b, "25"); emit4(b, 0xff); // and eax, 0xff
    emit(b, "89 47");       // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));    // store into rd
}

static void emit_auipc(r5vm_t* vm, r5jitbuf_t* b, int rd, uint32_t immu)
{
    if (rd == 0) { return; }
    // R[rd] = vm->pc-4 + IMM_U(inst);
    uint32_t target_pc = (vm->pc-4 + immu) & vm->mem_mask;
    emit(b, "B8");    // mov eax, imm32
    emit4(b, target_pc);
    emit(b, "89 47");   // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_lui(r5vm_t* vm, r5jitbuf_t* b, int rd, uint32_t immu)
{
    if (rd == 0) { return; }
    // R[rd] = IMM_U(inst);
    emit(b, "b8"); // mov eax, IMM_U
    emit4(b, immu);
    emit(b, "89 47");   // mov [edi + disp8], eax
    emit1(b, OFF_X(rd));
}

static void emit_sw4(r5vm_t* vm, r5jitbuf_t* b, int rs1, int rs2, int imm_s)
{
    // addr = (R[rs1] + IMM_S) & vm->mem_mask;
    // *(uint32_t*)(&vm->mem[addr]) = R[rs2];
    emit(b, "8B 47"); emit1(b, OFF_X(rs1)); // mov eax, [edi + OFF_X(rs1)]
    if (imm_s) { emit(b, "05"); emit4(b, imm_s); } // add eax, imm32
    emit(b, "25"); emit4(b, vm->mem_mask);  // and eax, vm->mem_mask
    emit(b, "03 87"); emit4(b, OFF_MEM);    // add eax, [edi + OFF_MEM]
    emit(b, "8B 5F"); emit1(b, OFF_X(rs2)); // mov ebx, [edi + OFF_X(rs2)]
    emit(b, "89 18");                       // mov [eax], ebx
}

static void emit_sw2(r5vm_t* vm, r5jitbuf_t* b, int rs1, int rs2, int imm_s)
{
    // addr = (R[rs1] + IMM_S) & vm->mem_mask;
    // *(uint16_t*)(&vm->mem[addr]) = R[rs2] 0xFFFF;
    emit(b, "8B 47"); emit1(b, OFF_X(rs1)); // mov eax, [edi + OFF_X(rs1)]
    if (imm_s) { emit(b, "05"); emit4(b, imm_s); } // add eax, imm32
    emit(b, "25"); emit4(b, vm->mem_mask);  // and eax, vm->mem_mask
    emit(b, "03 87"); emit4(b, OFF_MEM);    // add eax, [edi + OFF_MEM]
    emit(b, "66 8b 5f"); emit1(b, OFF_X(rs2)); // mov bx, [edi + OFF_X(rs2)]
    emit(b, "66 89 18");                    // mov [eax], bx
}

static void emit_sw1(r5vm_t* vm, r5jitbuf_t* b, int rs1, int rs2, int imm_s)
{
    // addr = (R[rs1] + IMM_S) & vm->mem_mask;
    // *(uint8_t*)(&vm->mem[addr]) = R[rs2] 0xFF;
    emit(b, "8B 47"); emit1(b, OFF_X(rs1)); // mov eax, [edi + OFF_X(rs1)]
    if (imm_s) { emit(b, "05"); emit4(b, imm_s); } // add eax, imm32
    emit(b, "25"); emit4(b, vm->mem_mask);  // and eax, vm->mem_mask
    emit(b, "03 87"); emit4(b, OFF_MEM);    // add eax, [edi + OFF_MEM]
    emit(b, "8a 5f"); emit1(b, OFF_X(rs2)); // mov bl, [edi + OFF_X(rs2)]
    emit(b, "88 18");                       // mov [eax], bl
}

static void emit_xor(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int rs2)
{
    if (rd == 0) { return; }
    // R5VM_R_F3_XOR:  R[rd] = R[rs1] ^ R[rs2]; break;
    emit(b, "8B 47"); emit1(b, OFF_X(rs1)); // mov eax, [edi + OFF_X(rs1)]
    emit(b, "33 47"); emit1(b, OFF_X(rs2)); // xor eax, [edi + OFF_X(rs2)]
    emit(b, "89 47");       // mov [edi + disp8], eax
    emit1(b, (uint8_t)OFF_X(rd));
}

static void emit_or(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int rs2)
{
    if (rd == 0) { return; }
    // R5VM_R_F3_OR:  R[rd] = R[rs1] | R[rs2]; break;
    emit(b, "8B 47"); emit1(b, OFF_X(rs1)); // mov eax, [edi + OFF_X(rs1)]
    emit(b, "0b 47"); emit1(b, OFF_X(rs2)); // or  eax, [edi + OFF_X(rs2)]
    emit(b, "89 47");       // mov [edi + disp8], eax
    emit1(b, (uint8_t)OFF_X(rd));
}

static void emit_and(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int rs2)
{
    if (rd == 0) { return; }
    // R5VM_R_F3_AND:  R[rd] = R[rs1] & R[rs2]; break;
    emit(b, "8B 47"); emit1(b, OFF_X(rs1)); // mov eax, [edi + OFF_X(rs1)]
    emit(b, "23 47"); emit1(b, OFF_X(rs2)); // and eax, [edi + OFF_X(rs2)]
    emit(b, "89 47");       // mov [edi + disp8], eax
    emit1(b, (uint8_t)OFF_X(rd));
}

static void emit_sll(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int rs2)
{
    if (rd == 0) { return; }
    // R5VM_R_F3_SLL:  R[rd] = R[rs1] << (R[rs2] & 0x1F); break;
    emit(b, "8b 4f"); emit1(b, OFF_X(rs2)); // mov ecx, [edi + OFF_X(rs2)]
    // emit(b, "83 e1 1f");    // and ecx, 0x1f
    emit(b, "8B 47"); emit1(b, OFF_X(rs1)); // mov eax, [edi + OFF_X(rs1)]
    emit(b, "d3 e0");       // shl eax, cl
    emit(b, "89 47");       // mov [edi + disp8], eax
    emit1(b, (uint8_t)OFF_X(rd));
}

static void emit_srl(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int rs2)
{
    if (rd == 0) { return; }
    // R[rd] = R[rs1] >> (R[rs2] & 0x1F)
    emit(b, "8b 4f"); emit1(b, OFF_X(rs2)); // ECX = R[rs2]
    // emit(b, "83 e1 1f");      // and ecx, 0x1f
    emit(b, "8b 47"); emit1(b, OFF_X(rs1)); // EAX = R[rs1]
    emit(b, "d3 e8"); // shr eax, cl   (logical)
    emit(b, "89 47"); emit1(b, OFF_X(rd)); // R[rd] = eax
}

static void emit_sra(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int rs2)
{
    if (rd == 0) { return; }
    // R[rd] = ((int32_t)R[rs1]) >> (R[rs2] & 0x1F)
    emit(b, "8b 4f"); emit1(b, OFF_X(rs2)); // ECX = R[rs2]
    // emit(b, "83 e1 1f");      // and ecx, 0x1f
    emit(b, "8b 47"); emit1(b, OFF_X(rs1)); // EAX = R[rs1]
    emit(b, "d3 f8"); // sar eax, cl   (arithmetic)
    emit(b, "89 47"); emit1(b, OFF_X(rd)); // R[rd] = eax
}

static void emit_slt(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int rs2)
{
    if (rd == 0) { return; }
    emit(b, "8B 47"); emit1(b, OFF_X(rs1)); // eax = R[rs1]
    emit(b, "8B 5F"); emit1(b, OFF_X(rs2)); // ebx = R[rs2]
    emit(b, "39 D8"); // cmp eax, ebx
    emit(b, "0F 9C C0"); // setl al   (signed compare)
    emit(b, "0F B6 C0"); // movzx eax, al
    emit(b, "89 47"); emit1(b, OFF_X(rd)); // R[rd] = eax
}

static void emit_sltu(r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int rs2)
{
    if (rd == 0) { return; }
    // R5VM_R_F3_SLTU: R[rd] = (R[rs1] < R[rs2]);
    emit(b, "8B 47"); emit1(b, OFF_X(rs1)); // eax = R[rs1]
    emit(b, "8B 5F"); emit1(b, OFF_X(rs2)); // ebx = R[rs2]
    emit(b, "39 D8"); // cmp eax, ebx
    emit(b, "0F 92 C0"); // setb al (unsigned compare)
    emit(b, "0F B6 C0"); // movzx eax, al
    emit(b, "89 47"); emit1(b, OFF_X(rd)); // R[rd] = eax
}

static void emit_jal(const r5vm_t* vm, r5jitbuf_t* b, int rd, int imm_j)
{
    // R[rd] = vm->pc;
    // vm->pc = (vm->pc + IMM_J(inst) - 4) & vm->mem_mask;
    uint32_t target_pc = (vm->pc-4 + imm_j) & vm->mem_mask;

    // mov DWORD PTR [edi + OFF_X(rd)], target_pc
    if (rd != 0) {
        emit(b, "c7 47"); // R[rd] = vm->pc
        emit1(b, OFF_X(rd));
        emit4(b, vm->pc);
    }
    emit(b, "FF 25"); // jmp [jit->instruction_pointers + target_index]
    emit4(b, (uint32_t)(b->instruction_pointers + target_pc)); // x86-32 bit only
}

static void emit_jalr(const r5vm_t* vm, r5jitbuf_t* b, int rd, int rs1, int imm_i)
{
    // R[rd] = vm->pc;
    // target_pc = ((R[rs1] + IMM_I(inst)) & ~1U) & vm->mem_mask;
    // mov DWORD PTR [edi + OFF_X(rd)], target_pc
    if (rd != 0) { emit(b, "c7 47"); emit1(b, OFF_X(rd)); emit4(b, vm->pc); } // R[rd] = vm->pc
    emit(b, "8b 47"); emit1(b, OFF_X(rs1)); // eax = R[rs1] (mov eax, [edi + rs1*4])
    if (imm_i != 0) {
        emit(b, "05");  // eax += imm_i (add eax, imm_i)
        emit4(b, imm_i);
    }
    emit(b, "c1 e0 02"); // shl eax, 0x2 (multiply by 4 for index into instruction_pointers)
    emit(b, "25"); emit4(b, vm->mem_mask & ~1U); // mask risc-v target_pc and clear bit 0
    emit(b, "05"); emit4(b, (uint32_t)b->instruction_pointers); // add risc-v to x86 mapping table
    emit(b, "ff 20"); // jmp [eax]
}

static void emit_ecall(const r5vm_t *vm, r5jitbuf_t *b)
{
    emit1(b, 0x57); // push edi    ; argument: vm*
    emit1(b, 0xE8);
    emit4(b, calc_rel32(b, r5vm_handle_ecall));
    emit1(b, 0x83); // cdecl call: add esp, 4  ; to cleanup stack
    emit1(b, 0xC4);
    emit1(b, 4);
}

// ---- Compiler --------------------------------------------------------------

static bool r5jit_step(r5vm_t* vm, r5jitbuf_t* jit)
{
    bool retcode = true;
    /* fetch next instruction: */
    uint32_t inst =  vm->mem[(vm->pc + 0) & vm->mem_mask]
                  | (vm->mem[(vm->pc + 1) & vm->mem_mask] << 8)
                  | (vm->mem[(vm->pc + 2) & vm->mem_mask] << 16)
                  | (vm->mem[(vm->pc + 3) & vm->mem_mask] << 24);
    vm->pc = (vm->pc + 4) & vm->mem_mask;
    /* decode/execute: */
    const uint32_t rd  = RD(inst) & 0x7f;
    const uint32_t rs1 = RS1(inst) & 0x7f;
    const uint32_t rs2 = RS2(inst) & 0x7f;
    switch (OPCODE(inst))
    {
    /* _--------------------- R-Type instuctions ---------------------_ */
    case (R5VM_OPCODE_R_TYPE):
        switch (FUNCT3(inst)) {
        case R5VM_R_F3_ADD_SUB:
            if (FUNCT7(inst) == R5VM_R_F7_SUB)
                emit_sub(jit, rd, rs1, rs2); // R[rd] = R[rs1] - R[rs2]
            else
                emit_add(jit, rd, rs1, rs2); // R[rd] = R[rs1] + R[rs2]
            break;
        case R5VM_R_F3_XOR:  emit_xor(vm, jit, rd, rs1, rs2); break; // R[rd] = R[rs1] ^ R[rs2]; break;
        case R5VM_R_F3_OR:   emit_or(vm, jit, rd, rs1, rs2); break;  // R[rd] = R[rs1] | R[rs2]; break;
        case R5VM_R_F3_AND:  emit_and(vm, jit, rd, rs1, rs2); break; // R[rd] = R[rs1] & R[rs2]; break;
        case R5VM_R_F3_SLL:  emit_sll(vm, jit, rd, rs1, rs2); break; // R[rd] = R[rs1] << (R[rs2] & 0x1F); break;
        case R5VM_R_F3_SRL_SRA:
                             if (FUNCT7(inst) == R5VM_R_F7_SRA)
                                 emit_sra(vm, jit, rd, rs1, rs2); // R[rd] = ((int32_t)R[rs1]) >> (R[rs2] & 0x1F);
                             else
                                 emit_srl(vm, jit, rd, rs1, rs2); // R[rd] = R[rs1] >> (R[rs2] & 0x1F);
                             break;
        case R5VM_R_F3_SLT:  emit_slt(vm, jit, rd, rs1, rs2); break; // R[rd] = ((int32_t)R[rs1] < (int32_t)R[rs2]); break;
        case R5VM_R_F3_SLTU: emit_sltu(vm, jit, rd, rs1, rs2); break; // R[rd] = (R[rs1] < R[rs2]); break;
#ifdef R5VM_DEBUG
        default:
            r5vm_error(vm, "Unknown R-type funct3", vm->pc-4, inst);
            jit->error = true;
            retcode = false;
#endif
        }
        break;
    /* _--------------------- I-Type instuctions ---------------------_ */
    case (R5VM_OPCODE_I_TYPE):
        switch (FUNCT3(inst)) {
        case R5VM_I_F3_ADDI:  emit_addi (jit, rd, rs1, IMM_I(inst)); break; // R[rd] = R[rs1] + IMM_I(inst); break;
        case R5VM_I_F3_XORI:  emit_xori (jit, rd, rs1, IMM_I(inst)); break; // R[rd] = R[rs1] ^ IMM_I(inst); break;
        case R5VM_I_F3_ORI:   emit_ori  (jit, rd, rs1, IMM_I(inst)); break; // R[rd] = R[rs1] | IMM_I(inst); break;
        case R5VM_I_F3_ANDI:  emit_andi (jit, rd, rs1, IMM_I(inst)); break; // R[rd] = R[rs1] & IMM_I(inst); break;
        case R5VM_I_F3_SLTI:  emit_slti (jit, rd, rs1, IMM_I(inst)); break; // R[rd] = ((int32_t)R[rs1] < IMM_I(inst)); break;
        case R5VM_I_F3_SLTIU: emit_sltiu(jit, rd, rs1, IMM_I(inst)); break; // R[rd] = (R[rs1] < (uint32_t)IMM_I(inst)); break;
        case R5VM_I_F3_SLLI:
            if (FUNCT7(inst) == R5VM_I_F7_SLLI)
                emit_slli(jit, rd, rs1, IMM_I(inst)); break; // R[rd] = R[rs1] << (IMM_I(inst) & 0x1F);
            break;
        case R5VM_I_F3_SRLI_SRAI:
            if (FUNCT7(inst) == R5VM_I_F7_SRLI)
                emit_srli(jit, rd, rs1, IMM_I(inst)); // R[rd] = R[rs1] >> (IMM_I(inst) & 0x1F);
            else if (FUNCT7(inst) == R5VM_I_F7_SRAI)
                emit_srai(jit, rd, rs1, IMM_I(inst)); // R[rd] = ((int32_t)R[rs1]) >> (IMM_I(inst) & 0x1F);
            break;
#ifdef R5VM_DEBUG
        default:
            r5vm_error(vm, "Unknown I-type funct3", vm->pc-4, inst);
            jit->error = true;
            retcode = false;
#endif
        }
        break;
    /* _--------------------- AUIPC -----------------------------------_ */
    case (R5VM_OPCODE_AUIPC):
        emit_auipc(vm, jit, rd, IMM_U(inst)); // R[rd] = vm->pc-4 + IMM_U(inst);
        break;
    /* _--------------------- LUI -------------------------------------_ */
    case (R5VM_OPCODE_LUI):
        emit_lui(vm, jit, rd, IMM_U(inst)); // R[rd] = IMM_U(inst);
        break;
    /* _--------------------- Load -----------------------------------_ */
    case (R5VM_OPCODE_LW):
        {
        switch (FUNCT3(inst)) {
        case R5VM_I_F3_LB:  emit_lb(vm, jit, rd, rs1, IMM_I(inst)); break; // R[rd] = (int8_t)b0; break;
        case R5VM_I_F3_LH:  emit_lh(vm, jit, rd, rs1, IMM_I(inst)); break; // R[rd] = (int16_t)(b0 | (b1 << 8)); break;
        case R5VM_I_F3_LW:  emit_lw(vm, jit, rd, rs1, IMM_I(inst)); break; // R[rd] = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24); break;
        case R5VM_I_F3_LBU: emit_lbu(vm, jit, rd, rs1, IMM_I(inst)); break; // R[rd] = b0; break;
        case R5VM_I_F3_LHU: emit_lhu(vm, jit, rd, rs1, IMM_I(inst)); break; // R[rd] = b0 | (b1 << 8); break;
#ifdef R5VM_DEBUG
        default:
            r5vm_error(vm, "Unknown Load funct3", vm->pc-4, inst);
            jit->error = true;
            retcode = false;
#endif
        }
        }
        break;
    /* _--------------------- Store ----------------------------------_ */
    case (R5VM_OPCODE_SW):
        {
        switch (FUNCT3(inst)) { // vm->mem[addr & vm->mem_mask] = R[rs2]
        case R5VM_S_F3_SW: emit_sw4(vm, jit, rs1, rs2, IMM_S(inst)); break; // 32-bit store (4 bytes)
        case R5VM_S_F3_SH: emit_sw2(vm, jit, rs1, rs2, IMM_S(inst)); break; // 16-bit store (2 bytes)
        case R5VM_S_F3_SB: emit_sw1(vm, jit, rs1, rs2, IMM_S(inst)); break; // 8-bit store (1 byte)
#ifdef R5VM_DEBUG
        default:
            r5vm_error(vm, "Illegal store width", vm->pc-4, inst);
            jit->error = true;
            retcode = false;
#endif
        }
        }
        break;
    /* _--------------------- Branch ---------------------------------_ */
    case (R5VM_OPCODE_BRANCH):
        switch (FUNCT3(inst)) {
        case R5VM_B_F3_BEQ:  emit_beq(vm, jit, rs1, rs2, IMM_B(inst)); break;  // if (R[rs1] == R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
        case R5VM_B_F3_BNE:  emit_bne(vm, jit, rs1, rs2, IMM_B(inst)); break;  // if (R[rs1] != R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
        case R5VM_B_F3_BLTU: emit_bltu(vm, jit, rs1, rs2, IMM_B(inst)); break; // if (R[rs1] <  R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
        case R5VM_B_F3_BGEU: emit_bgeu(vm, jit, rs1, rs2, IMM_B(inst)); break; // if (R[rs1] >= R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
        case R5VM_B_F3_BLT:  emit_blt(vm, jit, rs1, rs2, IMM_B(inst)); break;  // if ((int32_t)R[rs1] <  (int32_t)R[rs2]) vm->pc = (vm->pc-4 + IMM_B(inst)) & vm->mem_mask; break;
        case R5VM_B_F3_BGE:  emit_bge(vm, jit, rs1, rs2, IMM_B(inst)); break;  // if ((int32_t)R[rs1] >= (int32_t)R[rs2]) vm->pc = (vm->pc-4 + IMM_B(inst)) & vm->mem_mask; break;
#ifdef R5VM_DEBUG
        default:
            r5vm_error(vm, "Unknown Branch funct3", vm->pc-4, inst);
            jit->error = true;
            retcode = false;
#endif
        }
        break;
    /* _--------------------- JAL ------------------------------------_ */
    case (R5VM_OPCODE_JAL):
        emit_jal(vm, jit, rd, IMM_J(inst)); // R[rd] = vm->pc; vm->pc = (vm->pc + IMM_J(inst) - 4) & vm->mem_mask;
        break;
    /* _--------------------- JALR -----------------------------------_ */
    case (R5VM_OPCODE_JALR):
        if (FUNCT3(inst) == 0x0)
        {
            emit_jalr(vm, jit, rd, rs1, IMM_I(inst)); // R[rd] = vm->pc; vm->pc = ((R[rs1] + IMM_I(inst)) & ~1U) & vm->mem_mask;
        }
#ifdef R5VM_DEBUG
        else
        {
            r5vm_error(vm, "Unknown JALR funct3", vm->pc-4, inst);
            jit->error = true;
            retcode = false;
        }
#endif
        break;
    /* _--------------------- System Call ----------------------------_ */
    case (R5VM_OPCODE_SYSTEM):
        if (FUNCT3(inst) == 0) {
            uint32_t imm12 = (inst >> 20) & 0xFFF;
            uint32_t syscall_id = vm->a7;
            if (imm12 == 0) { /* ecall */
                emit_ecall(vm, jit);
            }
            else if (imm12 == 1) {  /* ebreak */
                r5jit_emit_epilog(jit);
            }
            else {
                r5vm_error(vm, "Unknown system call", vm->pc-4, inst);
                jit->error = true;
                retcode = false;
            }
        }
        else
        {
            r5vm_error(vm, "Unknown system call", vm->pc - 4, inst);
            jit->error = true;
            retcode = false;
        }
        break;
    /* _--------------------- FENCE / FENCE.I ------------------------_ */
    case (R5VM_OPCODE_FENCE):
        emit(jit, "90"); // NOP
        break;
    default:
        r5vm_error(vm, "Unknown opcode", vm->pc-4, inst);
        jit->error = true;
        retcode = false; // unhandled instuction
        break;
    }
    return retcode;
}

// ---- JIT-Entry -------------------------------------------------------------

void r5jit_dump(const r5jitbuf_t* jit)
{
    // objdump -D -b binary -mi386 -M intel jit.bin
    FILE* f = fopen("jit.bin", "wb");
    fwrite(jit->mem, 1, jit->pos, f);
    fclose(f);
}

bool r5jit_compile(r5vm_t* vm, r5jitbuf_t* jit)
{
    bool success = true;
    uint32_t pc;

    r5jit_emit_prolog(jit, vm);
    for (pc = vm->code_offset; pc < vm->code_offset + vm->code_size; pc+=4) {
        assert(pc < vm->code_size);
        jit->instruction_pointers[pc] = (unsigned) &jit->mem[jit->pos];
#if 0
        printf("MAP: r5 pc 0x%06x -> [%p] = 0x%08x)) -- ", pc,
            (void*)&jit->instruction_pointers[pc],
            jit->instruction_pointers[pc]);
        printf("%02x ", (vm->mem[vm->pc + 3]) & 0xff);
        printf("%02x ", (vm->mem[vm->pc + 2]) & 0xff);
        printf("%02x ", (vm->mem[vm->pc + 1]) & 0xff);
        printf("%02x ", (vm->mem[vm->pc + 0]) & 0xff);
        printf("\n");
#endif

        vm->pc = pc;
        if (r5jit_step(vm, jit) == false) {
            success = false;
            jit->error = true;
            break;
        }
    }
    r5jit_emit_epilog(jit); // to be safe

    return success;
}

bool r5jit_x86(r5vm_t* vm)
{
    bool success = false;
    uint8_t* mem = NULL;
    unsigned* instruction_pointers = NULL;
    size_t mem_size = vm->mem_size;

    mem = r5jit_get_rwx_mem(mem_size);
    if (!mem) {
        goto cleanup;
    }

    instruction_pointers = malloc(vm->code_size * 4); /* map r5 pc to x86 pc*/
    if (!instruction_pointers) {
        goto cleanup;
    }

    r5jitbuf_t jit = { .mem = mem,
                       .mem_size = mem_size,
                       .pos = 0,
                       .instruction_pointers = instruction_pointers,
                       .error = false, };

    if (r5jit_compile(vm, &jit)) {
        r5jit_dump(&jit);

        {
        hi_time t0 = hi_time_now();
        r5jit_exec(vm, &jit);
        hi_time t1 = hi_time_now();
        printf("dt: %.3f us (JIT)\n", 1000000.0*hi_time_elapsed(t0, t1));
        }

        success = true;
    }

cleanup:
    r5jit_free_rwx_mem(mem);
    free(instruction_pointers);
    return success;
}

