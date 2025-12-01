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

#ifndef R5JIT_H
#define R5JIT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "r5vm.h"

// ---- Defines ---------------------------------------------------------------

typedef int (*r5jitfn_t)();

// ---- VM data structure -----------------------------------------------------

typedef struct {
    uint8_t*  mem; /**< JIT generated x86 code */
    size_t    mem_size; /**< bytes in mem */
    size_t    pos; /**< Current JIT generator offset in mem (bytes) */
    unsigned* instruction_pointers; /**< map r5 program counter to x86 pc*/
    bool      error; /**< set to true if something is bad */
} r5jitbuf_t;

// ---- Functions -------------------------------------------------------------

/**
 * @brief Get memory that can be written to and allows execution.
 *
 * @param bytes How much memory to allocate.
 * @return pointer to RWX memory.
 */
void* r5jit_get_rwx_mem(size_t bytes);

/**
 * @brief Free memory from r5jit_get_rwx_mem.
 * @param mem Memory r5jit_get_rwx_mem(...).
 */
void r5jit_free_rwx_mem(void* mem, size_t bytes);

#endif // R5VM_H

