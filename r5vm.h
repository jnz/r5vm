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
        uint32_t regs[32];  /**< Integer register file (x0–x31) */
        int32_t  regsi[32]; /**< Signed view of integer registers */
        struct {
            uint32_t zero, ra, sp, gp, tp;
            uint32_t t0, t1, t2;
            uint32_t s0, s1;
            uint32_t a0, a1, a2, a3, a4, a5, a6, a7;
            uint32_t s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
            uint32_t t3, t4, t5, t6;
        };
    };

    uint32_t pc;       /**< Program counter (byte address) */
    uint8_t* mem;      /**< Pointer to VM memory buffer */
    size_t   mem_size; /**< Total memory size in bytes (must be power of two) */
    uint32_t mem_mask; /**< Address mask for wrapping memory accesses */
} r5vm_t;

// ---- Lifecycle -------------------------------------------------------------

/**
 * @brief Initialize a VM instance.
 *
 * Clears internal state and binds the VM to a given memory region.
 * The memory size must be a power of two for address wrapping to work.
 *
 * @param vm        Pointer to an uninitialized VM instance.
 * @param mem_size  Size of memory in bytes (power of two).
 * @param mem       Pointer to allocated memory buffer (must be pre-loaded with
 *                  code/data).
 * @return `true` if initialization succeeded, `false` on invalid parameters.
 */
bool    r5vm_init(r5vm_t* vm, size_t mem_size, uint8_t* mem);

/**
 * @brief Destroy a VM instance.
 *
 * Clears all internal fields of the VM structure.
 * Does **not** free the memory buffer; ownership remains with the caller.
 *
 * @param vm Pointer to a VM instance.
 */
void    r5vm_destroy(r5vm_t* vm);

// ---- Execution control -----------------------------------------------------

/**
 * @brief Reset CPU registers and program counter.
 *
 * Sets all general-purpose registers to zero and resets `pc` to 0.
 *
 * @param vm Pointer to a VM instance.
 */
void    r5vm_reset(r5vm_t* vm);

/**
 * @brief Execute a single instruction.
 *
 * Decodes and executes one RISC-V instruction at the current program counter.
 * Updates registers and memory accordingly.
 *
 * @param vm Pointer to an initialized VM.
 * @return `true` if execution should continue, `false` on halt or error.
 */
bool    r5vm_step(r5vm_t* vm);

/**
 * @brief Run the VM for a given number of steps.
 *
 * Executes up to `max_steps` instructions, or indefinitely if `max_steps < 0`.
 * Stops when a halt condition or error occurs.
 *
 * @param vm         Pointer to an initialized VM.
 * @param max_steps  Maximum instruction count, or -1 for unlimited.
 * @return Number of executed steps before halting.
 */
int     r5vm_run(r5vm_t* vm, int max_steps);

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
void    r5vm_error(r5vm_t* vm, const char* msg, uint32_t pc, uint32_t instr);

#endif // R5VM_H

