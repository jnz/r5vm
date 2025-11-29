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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h> /* for putchar in ecall */

#include "r5vm.h"

// ---- Functions -------------------------------------------------------------

void r5vm_reset(r5vm_t* vm)
{
    memset(vm->regs, 0, sizeof vm->regs);
    vm->pc = vm->entry;
}

/**
 * @brief Execute a single instruction.
 *
 * Decodes and executes one RISC-V instruction at the current program counter.
 * Updates registers and memory accordingly.
 *
 * @param vm Pointer to an initialized VM.
 * @return `true` if execution should continue, `false` on halt or error.
 */
static bool r5vm_step(r5vm_t* vm)
{
    bool retcode = true;
    /* fetch next instruction: */
    uint32_t inst =  vm->mem[(vm->pc + 0) & vm->mem_mask]
                  | (vm->mem[(vm->pc + 1) & vm->mem_mask] << 8)
                  | (vm->mem[(vm->pc + 2) & vm->mem_mask] << 16)
                  | (vm->mem[(vm->pc + 3) & vm->mem_mask] << 24);
#ifdef R5VM_DEBUG
    if (vm->pc+4 > vm->mem_size - 4) {
        r5vm_error(vm, "PC out of bounds", vm->pc, 0);
        return false;
    }
#endif
    vm->pc = (vm->pc + 4) & vm->mem_mask;
    /* decode/execute: */
    const uint32_t rd  = RD(inst);
    const uint32_t rs1 = RS1(inst);
    const uint32_t rs2 = RS2(inst);
    uint32_t* R = vm->regs;
    switch (OPCODE(inst))
    {
    /* _--------------------- R-Type instuctions ---------------------_ */
    case (R5VM_OPCODE_R_TYPE):
        switch (FUNCT3(inst)) {
        case R5VM_R_F3_ADD_SUB:
            R[rd] = (FUNCT7(inst) == R5VM_R_F7_SUB)
                        ? R[rs1] - R[rs2]
                        : R[rs1] + R[rs2];
            break;
        case R5VM_R_F3_XOR:  R[rd] = R[rs1] ^ R[rs2]; break;
        case R5VM_R_F3_OR:   R[rd] = R[rs1] | R[rs2]; break;
        case R5VM_R_F3_AND:  R[rd] = R[rs1] & R[rs2]; break;
        case R5VM_R_F3_SLL:  R[rd] = R[rs1] << (R[rs2] & 0x1F); break;
        case R5VM_R_F3_SRL_SRA:
                             if (FUNCT7(inst) == R5VM_R_F7_SRA)
                                 R[rd] = ((int32_t)R[rs1]) >> (R[rs2] & 0x1F);
                             else
                                 R[rd] = R[rs1] >> (R[rs2] & 0x1F);
                             break;
        case R5VM_R_F3_SLT:  R[rd] = ((int32_t)R[rs1] < (int32_t)R[rs2]); break;
        case R5VM_R_F3_SLTU: R[rd] = (R[rs1] < R[rs2]); break;
#ifdef R5VM_DEBUG
        default:
            r5vm_error(vm, "Unknown R-type funct3", vm->pc-4, inst);
            retcode = false;
#endif
        }
        break;
    /* _--------------------- I-Type instuctions ---------------------_ */
    case (R5VM_OPCODE_I_TYPE):
        switch (FUNCT3(inst)) {
        case R5VM_I_F3_ADDI:  R[rd] = R[rs1] + IMM_I(inst); break;
        case R5VM_I_F3_XORI:  R[rd] = R[rs1] ^ IMM_I(inst); break;
        case R5VM_I_F3_ORI:   R[rd] = R[rs1] | IMM_I(inst); break;
        case R5VM_I_F3_ANDI:  R[rd] = R[rs1] & IMM_I(inst); break;
        case R5VM_I_F3_SLTI:  R[rd] = ((int32_t)R[rs1] < IMM_I(inst)); break;
        case R5VM_I_F3_SLTIU: R[rd] = (R[rs1] < (uint32_t)IMM_I(inst)); break;
        case R5VM_I_F3_SLLI:
            if (FUNCT7(inst) == R5VM_I_F7_SLLI)
                R[rd] = R[rs1] << (IMM_I(inst) & 0x1F);
            break;
        case R5VM_I_F3_SRLI_SRAI:
            if (FUNCT7(inst) == R5VM_I_F7_SRLI)
                R[rd] = R[rs1] >> (IMM_I(inst) & 0x1F);
            else if (FUNCT7(inst) == R5VM_I_F7_SRAI)
                R[rd] = ((int32_t)R[rs1]) >> (IMM_I(inst) & 0x1F);
            break;
#ifdef R5VM_DEBUG
        default:
            r5vm_error(vm, "Unknown I-type funct3", vm->pc-4, inst);
            retcode = false;
#endif
        }
        break;
    /* _--------------------- AUIPC -----------------------------------_ */
    case (R5VM_OPCODE_AUIPC):
        R[rd] = vm->pc-4 + IMM_U(inst);
        break;
    /* _--------------------- LUI -------------------------------------_ */
    case (R5VM_OPCODE_LUI):
        R[rd] = IMM_U(inst);
        break;
    /* _--------------------- Load -----------------------------------_ */
    case (R5VM_OPCODE_LW):
        {
        const uint32_t addr = R[rs1] + IMM_I(inst);
#ifdef R5VM_DEBUG
        if (addr > vm->mem_size - 4)
        {
            r5vm_error(vm, "Memory access out of bounds", vm->pc-4, inst);
            retcode = false;
            break;
        }
#endif
        const uint8_t b0 = vm->mem[(addr + 0) & vm->mem_mask];
        const uint8_t b1 = vm->mem[(addr + 1) & vm->mem_mask];
        const uint8_t b2 = vm->mem[(addr + 2) & vm->mem_mask];
        const uint8_t b3 = vm->mem[(addr + 3) & vm->mem_mask];

        switch (FUNCT3(inst)) {
        case R5VM_I_F3_LB:  R[rd] = (int8_t)b0; break;
        case R5VM_I_F3_LH:  R[rd] = (int16_t)(b0 | (b1 << 8)); break;
        case R5VM_I_F3_LW:  R[rd] = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24); break;
        case R5VM_I_F3_LBU: R[rd] = b0; break;
        case R5VM_I_F3_LHU: R[rd] = b0 | (b1 << 8); break;
#ifdef R5VM_DEBUG
        default:
            r5vm_error(vm, "Unknown Load funct3", vm->pc-4, inst);
            retcode = false;
#endif
        }
        }
        break;
    /* _--------------------- Store ----------------------------------_ */
    case (R5VM_OPCODE_SW):
        {
        const uint32_t addr = R[rs1] + IMM_S(inst);
#ifdef R5VM_DEBUG
        if (addr > vm->mem_size - 4) {
            r5vm_error(vm, "Memory access out of bounds", vm->pc-4, inst);
            retcode = false;
            break;
        }
#endif
        switch (FUNCT3(inst)) {
        case R5VM_S_F3_SW: // 32-bit store (4 bytes)
            vm->mem[(addr + 3) & vm->mem_mask] = (R[rs2] >> 24) & 0xFF;
            vm->mem[(addr + 2) & vm->mem_mask] = (R[rs2] >> 16) & 0xFF;
            /* fall through */
        case R5VM_S_F3_SH: // 16-bit store (2 bytes)
            vm->mem[(addr + 1) & vm->mem_mask] = (R[rs2] >> 8) & 0xFF;
            /* fall through */
        case R5VM_S_F3_SB: // 8-bit store (1 byte)
            vm->mem[(addr + 0) & vm->mem_mask] = (R[rs2] >> 0) & 0xFF;
            break;
#ifdef R5VM_DEBUG
        default:
            r5vm_error(vm, "Illegal store width", vm->pc-4, inst);
            retcode = false;
#endif
        }
        }
        break;
    /* _--------------------- Branch ---------------------------------_ */
    case (R5VM_OPCODE_BRANCH):
        switch (FUNCT3(inst)) {
        case R5VM_B_F3_BEQ:  if (R[rs1] == R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
        case R5VM_B_F3_BNE:  if (R[rs1] != R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
        case R5VM_B_F3_BLTU: if (R[rs1] <  R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
        case R5VM_B_F3_BGEU: if (R[rs1] >= R[rs2]) vm->pc = ((vm->pc-4 + IMM_B(inst)) & vm->mem_mask); break;
        case R5VM_B_F3_BLT:  if ((int32_t)R[rs1] <  (int32_t)R[rs2]) vm->pc = (vm->pc-4 + IMM_B(inst)) & vm->mem_mask; break;
        case R5VM_B_F3_BGE:  if ((int32_t)R[rs1] >= (int32_t)R[rs2]) vm->pc = (vm->pc-4 + IMM_B(inst)) & vm->mem_mask; break;
#ifdef R5VM_DEBUG
        default:
            r5vm_error(vm, "Unknown Branch funct3", vm->pc-4, inst);
            retcode = false;
#endif
        }
        break;
    /* _--------------------- JAL ------------------------------------_ */
    case (R5VM_OPCODE_JAL):
        R[rd] = vm->pc;
        vm->pc = (vm->pc + IMM_J(inst) - 4) & vm->mem_mask;
        break;
    /* _--------------------- JALR -----------------------------------_ */
    case (R5VM_OPCODE_JALR):
        if (FUNCT3(inst) == 0x0)
        {
            R[rd] = vm->pc;
            vm->pc = ((R[rs1] + IMM_I(inst)) & ~1U) & vm->mem_mask;
        }
#ifdef R5VM_DEBUG
        else
        {
            r5vm_error(vm, "Unknown JALR funct3", vm->pc-4, inst);
            retcode = false;
        }
#endif
        break;
    /* _--------------------- System Call ----------------------------_ */
    case (R5VM_OPCODE_SYSTEM):
        {
        uint32_t syscall_id = vm->a7;
        switch (syscall_id) {
        case 0:
            retcode = false;
            break;
        case 1:
            putchar(vm->a0 & 0xff);
            fflush(stdout);
            break;
        default:
            // r5vm_error(vm, "Unknown ECALL", vm->pc-4, syscall_id);
            retcode = false;
        }
        }
        break;
    /* _--------------------- FENCE / FENCE.I --------------------------_ */
    case (R5VM_OPCODE_FENCE):
        // no-op
        break;
    default:
        r5vm_error(vm, "Unknown opcode", vm->pc-4, inst);
        retcode = false; // unhandled instuction
        break;
    }
    R[0] = 0; // enforce x0=0
    return retcode;
}

unsigned r5vm_run(r5vm_t* vm, unsigned max_steps)
{
    unsigned i;
    for (i = 0; i < max_steps || max_steps == 0; i++) {
        if (!r5vm_step(vm)) {
            break;
        }
    }
    return i;
}

