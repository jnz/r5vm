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

// ---- Macros ---------------------------------------------------------------

#define IS_POWER_OF_TWO(n)  ((n) != 0 && ((n) & ((n) - 1)) == 0)
/** Interprete as signed integer with sign extension */
#define SIGN_EXT32(x,bits)  ((int32_t)((x) << (32 - (bits))) >> (32 - (bits)))

/* Macros to extract fields from the 32-bit RISC-V instructions */
#define OPCODE(inst)        ((inst) & 0x7F) /**< opcode field */
#define RD(inst)            (((inst) >> 7)  & 0x1F) /**< destination register */
#define FUNCT3(inst)        (((inst) >> 12) & 0x07) /**< function 3 field */
#define RS1(inst)           (((inst) >> 15) & 0x1F) /**< source register 1 */
#define RS2(inst)           (((inst) >> 20) & 0x1F) /**< source register 2 */
#define FUNCT7(inst)        (((inst) >> 25) & 0x7F) /**< function 7 field */
/* Macros to extract immediate values from instructions */
#define IMM_I(inst)         SIGN_EXT32(((inst) >> 20) & 0xFFF, 12)
#define IMM_S(inst)         SIGN_EXT32(((((inst) >> 25) << 5) | \
                                       (((inst) >> 7) & 0x1F)), 12 )
#define IMM_U(inst)         ((uint32_t)(inst) & 0xFFFFF000)
#define IMM_B(inst)         SIGN_EXT32(((((inst) >> 31) & 0x1) << 12) | \
                                       ((((inst) >> 7)  & 0x1) << 11) | \
                                       ((((inst) >> 25) & 0x3F) << 5) | \
                                       ((((inst) >> 8)  & 0xF) << 1), 13)
#define IMM_J(inst)         SIGN_EXT32((((inst >> 31) & 0x1) << 20) | \
                                       (((inst >> 12) & 0xFF) << 12) | \
                                       (((inst >> 20) & 0x1) << 11) | \
                                       (((inst >> 21) & 0x3FF) << 1), 21)

// ---- Defines ---------------------------------------------------------------

#define R5VM_OPCODE_R_TYPE  0x33 /**< Register-Register operations */
#define R5VM_OPCODE_I_TYPE  0x13 /**< Register-Immediate operations */
#define R5VM_OPCODE_LW      0x03 /**< Load Instructions */
#define R5VM_OPCODE_SW      0x23 /**< Store Instructions */
#define R5VM_OPCODE_SYSTEM  0x73 /**< System Call Instructions */
#define R5VM_OPCODE_AUIPC   0x17 /**< Add Upper Immediate to PC */
#define R5VM_OPCODE_BRANCH  0x63 /**< Branch Instructions */
#define R5VM_OPCODE_LUI     0x37 /**< Load Upper Immediate */
#define R5VM_OPCODE_JAL     0x6F /**< Jump and Link */
#define R5VM_OPCODE_JALR    0x67 /**< Jump and Link Register */
#define R5VM_OPCODE_FENCE   0x0F /**< Fence Instructions (Noop for R5VM) */

/* Function 3 (F3) */
#define R5VM_R_F3_ADD_SUB   0x00 /**< Reg. Add / Subtract */
#define R5VM_R_F3_XOR       0x04 /**< Reg. Xor */
#define R5VM_R_F3_OR        0x06 /**< Reg. Or */
#define R5VM_R_F3_AND       0x07 /**< Reg. And */
#define R5VM_R_F3_SLL       0x01 /**< Reg. Shift Left Logical */
#define R5VM_R_F3_SRL_SRA   0x05 /**< Reg. Shift Right Logical/Arithmetic */
#define R5VM_R_F3_SLT       0x02 /**< Reg. Set Less Than */
#define R5VM_R_F3_SLTU      0x03 /**< Reg. Set Less Than Unsigned */

#define R5VM_I_F3_ADDI      0x00 /**< Add Immediate */
#define R5VM_I_F3_XORI      0x04 /**< Xor Immediate */
#define R5VM_I_F3_ORI       0x06 /**< Or Immediate */
#define R5VM_I_F3_ANDI      0x07 /**< And Immediate */
#define R5VM_I_F3_SLLI      0x01 /**< Shift Left Logical Immediate */
#define R5VM_I_F3_SRLI_SRAI 0x05 /**< Shift Right Logical/Arithmetic Immediate */
#define R5VM_I_F3_SLTI      0x02 /**< Set Less Than Immediate */
#define R5VM_I_F3_SLTIU     0x03 /**< Set Less Than Immediate Unsigned */

#define R5VM_I_F3_LB        0x00 /**< Load Byte */
#define R5VM_I_F3_LH        0x01 /**< Load Halfword */
#define R5VM_I_F3_LW        0x02 /**< Load Word: R[rd] = M[R[rs1]+SE(imm)] */
#define R5VM_I_F3_LBU       0x04 /**< Load Byte Unsigned */
#define R5VM_I_F3_LHU       0x05 /**< Load Halfword Unsigned */

#define R5VM_S_F3_SB        0x00 /**< Store Byte */
#define R5VM_S_F3_SH        0x01 /**< Store Halfword */
#define R5VM_S_F3_SW        0x02 /**< Store Word: M[R[rs1]+SE(imm)] = R[rs2] */

#define R5VM_B_F3_BEQ       0x00 /**< Branch if Equal */
#define R5VM_B_F3_BNE       0x01 /**< Branch if Not Equal */
#define R5VM_B_F3_BLT       0x04 /**< Branch if Less Than */
#define R5VM_B_F3_BGE       0x05 /**< Branch if Greater or Equal */
#define R5VM_B_F3_BLTU      0x06 /**< Branch if Unsigned Less Than */
#define R5VM_B_F3_BGEU      0x07 /**< Branch if Unsigned Greater or Equal */

/* Function 7 */
#define R5VM_R_F7_ADD       0x00 /**< Add for R-type F3=Add/Sub */
#define R5VM_R_F7_SUB       0x20 /**< Subtract for R-type F3=Add/Sub */
#define R5VM_R_F7_SRL       0x00 /**< Shift Right Logic. f. R-type F3=SRL/SRA */
#define R5VM_R_F7_SRA       0x20 /**< Shift Right Arith. f. R-type F3=SRL/SRA */
#define R5VM_I_F7_SRLI      0x00 /**< Shift Right Logic. Imm. I-type F3=SRLI/SRAI */
#define R5VM_I_F7_SRAI      0x20 /**< Shift Right Arith. Imm. I-type F3=SRLI/SRAI */
#define R5VM_I_F7_SLLI      0x00 /**< Shift Left Logic. Imm. for I-type F3=SLLI */

// ---- Functions -------------------------------------------------------------

bool r5vm_init(r5vm_t* vm, uint8_t* mem, uint32_t mem_size)
{
    if (vm) { memset(vm, 0, sizeof(r5vm_t)); }
    if (!vm || !mem_size || !mem || !IS_POWER_OF_TWO(mem_size)) {
        return false;
    }
    vm->mem = mem;
    vm->mem_size = mem_size;
    vm->mem_mask = mem_size - 1; /* mem_size is power of two */

    return true;
}

void r5vm_destroy(r5vm_t* vm)
{
    (void)vm;
}

void r5vm_reset(r5vm_t* vm)
{
    memset(vm->regs, 0, sizeof vm->regs);
    vm->pc = 0;
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
            r5vm_error(vm, "Unknown ECALL", vm->pc-4, syscall_id);
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

