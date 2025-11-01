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

    uint32_t pc;       /**< Program counter (byte offset into "mem") */
    uint8_t* mem;      /**< Pointer to VM memory buffer */
    uint32_t mem_size; /**< Total memory size in bytes (must be power of two) */
    uint32_t mem_mask; /**< Address mask for sandbox memory accesses */
} r5vm_t;

// ---- Lifecycle -------------------------------------------------------------

/**
 * @brief Initialize a VM instance.
 *
 * Clears internal state and binds the VM to a given memory region.
 * The memory size must be a power of two for address wrapping to work.
 *
 * @param vm        Pointer to an uninitialized VM instance.
 * @param mem       Pointer to allocated memory buffer (must be pre-loaded with
 *                  code/data).
 * @param mem_size  Size of mem buffer in bytes (power of two).
 * @return `true` if initialization succeeded, `false` on invalid parameters.
 */
bool r5vm_init(r5vm_t* vm, uint8_t* mem, uint32_t mem_size);

/**
 * @brief Destroy a VM instance.
 *
 * Clears all internal fields of the VM structure.
 * Does **not** free the memory buffer. Ownership remains with the caller.
 *
 * @param vm Pointer to a VM instance.
 */
void r5vm_destroy(r5vm_t* vm);

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

/**
 * @brief Execute a single instruction.
 *
 * Decodes and executes one RISC-V instruction at the current program counter.
 * Updates registers and memory accordingly.
 *
 * @param vm Pointer to an initialized VM.
 * @return `true` if execution should continue, `false` on halt or error.
 */
bool r5vm_step(r5vm_t* vm);

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

#endif // R5VM_H

