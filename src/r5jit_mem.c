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

#if defined(_WIN32)

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <windows.h> /* VirtualAlloc */

#include "r5jit.h"

void* r5jit_get_rwx_mem(size_t bytes)
{
    return VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE,
                                          PAGE_EXECUTE_READWRITE);
}

void r5jit_free_rwx_mem(void* mem, size_t bytes)
{
    if (mem) {
        VirtualFree(mem, 0, MEM_RELEASE);
    }
}

#endif /* _WIN32 */

#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>

void* r5jit_get_rwx_mem(size_t bytes)
{
    size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
    size_t alloc = (bytes + pagesize - 1) & ~(pagesize - 1);

    void* p = mmap(NULL, alloc,
                   PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);

    return (p == MAP_FAILED) ? NULL : p;
}

void r5jit_free_rwx_mem(void* mem, size_t bytes)
{
    if (!mem) return;

    size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
    size_t alloc = (bytes + pagesize - 1) & ~(pagesize - 1);

    munmap(mem, alloc);
}
#endif /* Not Windows */
