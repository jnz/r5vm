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

/**
 * @mainpage R5VM - Minimal RISC-V RV32I Virtual Machine
 *
 * @section intro Introduction
 *
 * R5VM is a compact virtual machine that emulates a 32-bit RISC-V (RV32I) core
 * with a flat 32-bit address space. It is designed for simplicity, readability,
 * and portability. Suitable for educational use or lightweight emulation.
 *
 * @section usage Quick Start
 *
 * 1. Include `r5vm.h` in your project.
 * 2. Allocate a memory buffer and load your compiled RV32I program into it.
 *    (see the `guest/` example for how to produce a binary such as `vm.bin`).
 * 3. Create and initialize the VM:
 *    @code
 *    r5vm_t vm;
 *    r5vm_init(&vm, memory, memory_size);
 *    r5vm_reset(&vm);
 *    @endcode
 * 4. Run the program:
 *    @code
 *    r5vm_run(&vm, 0); // 0 = unlimited steps
 *    @endcode
 * 5. Implement `r5vm_error()` in your code to handle runtime errors.
 * 6. After execution, inspect registers or memory as needed.
 *
 * @section notes Notes
 *
 * - `mem_size` must be a power of two for address wrapping to work.
 * - `r5vm_destroy()` clears state but does not free the memory buffer.
 * - The base ISA supported is **RV32I** (no M/A/F/D extensions).
 *
 * @section license License
 * This project is released under the MIT License.
 */

#ifndef R5VM_H
#define R5VM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ---- Defines ---------------------------------------------------------------

/** @brief Version string of the R5VM runtime. */
#define R5VM_VERSION     "0.1.0"

/** @brief Base RISC-V ISA implemented by this VM. */
#define R5VM_BASE_ISA    "RV32I"

/** @brief .r5m Header Identifier (r5vm in little endian) */
#define R5VM_MAGIC       0x6d763572u

// ---- .r5m header structure -------------------------------------------------

#pragma pack(push, 1)
typedef struct {

    union {
        uint32_t magic;
        char magic_str[4];
    };
    uint16_t version;
    uint16_t flags;
    uint32_t entry;
    uint32_t load_addr;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t total_size;
    uint8_t  reserved[24];
} r5vm_header_t;
#pragma pack(pop)

// ---- VM data structure -----------------------------------------------------

/**
 * @brief CPU and memory state of the R5VM virtual machine.
 *
 * The VM emulates a minimal RV32I core with a flat 32-bit address space.
 * General-purpose registers are accessible both as an array (`regs`) and
 * through named aliases (e.g., `a0`, `sp`, `t0`, etc.).
 */
typedef struct r5vm_s
{
    union {
        uint32_t regs[32];  /**< Raw 32-bit integer registers (x0–x31). */
        int32_t  regsi[32]; /**< Signed 32-bit view of the same registers */
        struct {
            uint32_t zero; /**< x0: Hardwired zero reg. (writes ignored). */
            uint32_t ra;   /**< x1: Return address register (used by jumps). */
            uint32_t sp;   /**< x2: Stack pointer. */
            uint32_t gp;   /**< x3: Global data pointer. */
            uint32_t tp;   /**< x4: Thread local storage pointer. */
            uint32_t t0;   /**< x5: Temporary register 0. */
            uint32_t t1;   /**< x6: Temporary register 1. */
            uint32_t t2;   /**< x7: Temporary register 2. */
            uint32_t s0;   /**< x8: Saved register 0 / frame pointer (fp). */
            uint32_t s1;   /**< x9: Saved register 1. */
            uint32_t a0;   /**< x10: Argument/return value 0. */
            uint32_t a1;   /**< x11: Argument/return value 1. */
            uint32_t a2;   /**< x12: Argument register 2. */
            uint32_t a3;   /**< x13: Argument register 3. */
            uint32_t a4;   /**< x14: Argument register 4. */
            uint32_t a5;   /**< x15: Argument register 5. */
            uint32_t a6;   /**< x16: Argument register 6. */
            uint32_t a7;   /**< x17: Argument register 7 / syscall number. */
            uint32_t s2;   /**< x18: Saved register 2. */
            uint32_t s3;   /**< x19: Saved register 3. */
            uint32_t s4;   /**< x20: Saved register 4. */
            uint32_t s5;   /**< x21: Saved register 5. */
            uint32_t s6;   /**< x22: Saved register 6. */
            uint32_t s7;   /**< x23: Saved register 7. */
            uint32_t s8;   /**< x24: Saved register 8. */
            uint32_t s9;   /**< x25: Saved register 9. */
            uint32_t s10;  /**< x26: Saved register 10. */
            uint32_t s11;  /**< x27: Saved register 11. */
            uint32_t t3;   /**< x28: Temporary register 3. */
            uint32_t t4;   /**< x29: Temporary register 4. */
            uint32_t t5;   /**< x30: Temporary register 5. */
            uint32_t t6;   /**< x31: Temporary register 6. */
        };
    };

    uint32_t pc;          /**< Program counter (byte offset into "mem") */
    uint8_t* mem;         /**< Pointer to VM memory buffer */
    uint32_t mem_size;    /**< Total memory size in bytes (must be power of 2) */
    uint32_t mem_mask;    /**< Address mask for sandbox memory accesses */
    uint32_t code_offset; /**< Offset in mem for .code section */
    uint32_t code_size;   /**< Bytes of instructions. */
    uint32_t data_offset; /**< Offset in mem for .data section */
    uint32_t data_size;   /**< Bytes in .data section */
    uint32_t bss_offset;  /**< Offset in mem for .bss section */
    uint32_t bss_size;    /**< Bytes in .bss section */
    uint32_t entry;       /**< Program entry point (read-only) */
} r5vm_t;

// ---- Execution control -----------------------------------------------------

/**
 * @brief Reset CPU registers and program counter.
 *
 * Sets all general-purpose registers to zero and resets `pc` to 0.
 *
 * @param vm Pointer to a VM instance.
 */
void r5vm_reset(r5vm_t* vm);

/**
 * @brief Run the VM for a given number of steps.
 *
 * Executes up to `max_steps` instructions, or indefinitely if `max_steps == 0`.
 * Stops when a halt condition or error occurs.
 *
 * @param vm         Pointer to an initialized VM.
 * @param max_steps  Maximum instruction count, or 0 for unlimited.
 * @return Number of executed steps before halting.
 */
unsigned r5vm_run(r5vm_t* vm, unsigned max_steps);

// ---- Error -----------------------------------------------------------------

/**
 * @brief Error handler for fatal VM conditions.
 *
 * Called by VM on illegal instructions, invalid memory accesses, or
 * other unrecoverable faults.
 *
 * Users need to implement this function for error handling.
 *
 * @param vm     Pointer to the VM instance where the error occurred.
 * @param msg    Human-readable error message.
 * @param pc     Program counter at the time of error.
 * @param instr  Faulting instruction word.
 */
void r5vm_error(r5vm_t* vm, const char* msg, uint32_t pc, uint32_t instr);

// ---- Macros ---------------------------------------------------------------

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

#endif // R5VM_H

