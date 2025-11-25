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

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp
#include <ctype.h> // for isspace, tolower
#include <inttypes.h> // for PRIu32
#include <assert.h>

#include "r5vm.h"

// -------------------------------------------------------------

#define R5VM_MIN_MEM_SIZE   (64 * 1024) // 64 KiB

// -------------------------------------------------------------

static size_t parse_mem_arg(const char* s)
{
    char *end;
    unsigned long val = strtoul(s, &end, 0); // allows 0x... Hex
    while (isspace((unsigned char)*end)) end++;

    switch (tolower((unsigned char)*end)) {
        case 'k': val *= 1024UL; break;
        case 'm': val *= 1024UL * 1024UL; break;
        case 0: break;
        default:
            fprintf(stderr, "warning: unknown suffix '%c' in mem size, using bytes\n", *end);
    }
    return (size_t)val;
}

// -------------------------------------------------------------

static bool load_file(const char* path, uint8_t** out_mem, size_t*
                      out_mem_size, size_t override_mem, size_t* code_size)
{
    FILE* f = fopen(path, "rb");
    if (!f) { perror("fopen"); return false; }

    fseek(f, 0, SEEK_END);
    const long fsize = ftell(f);
    rewind(f);
    if (fsize <= 0) {
        fclose(f);
        fprintf(stderr, "error: empty file\n");
        return false;
    }

    // If override_mem is less than fsize, error
    if (override_mem && override_mem < (size_t)fsize) {
        fclose(f);
        fprintf(stderr, "error: override mem size %zu is less than file size %ld\n",
                override_mem, fsize);
        return false;
    }

    // Heuristic: +25% or at least fsize+R5VM_MIN_MEM_SIZE
    size_t base_mem = (size_t)fsize + ((size_t)fsize / 4);
    if (base_mem < (size_t)fsize + R5VM_MIN_MEM_SIZE) {
        base_mem = (size_t)fsize + R5VM_MIN_MEM_SIZE;
    }

    // If user override, use that
    size_t total_mem = override_mem ? override_mem : base_mem;
    // Make sure that total_mem is power of two
    size_t pow2_mem = 64;
    while (pow2_mem < total_mem)
        pow2_mem *= 2;
    total_mem = pow2_mem;

    uint8_t* mem = calloc(total_mem, 1);
    if (!mem) {
        fclose(f);
        perror("calloc");
        return false;
    }

    size_t nread = fread(mem, 1, (size_t)fsize, f);
    if (nread != (size_t)fsize) {
        fprintf(stderr, "error: fread failed (read %zu of %zu bytes)\n", nread, (size_t)fsize);
        fclose(f);
        free(mem);
        return false;
    }
    fclose(f);

    fprintf(stderr, "[r5vm] program=%ld bytes (%ld.%02ld KiB), allocated=%zu bytes (%zu KiB)%s\n",
        fsize,
        fsize / 1024, (fsize % 1024) * 100 / 1024,
        total_mem,
        total_mem / 1024,
        override_mem ? " (user override)" : "");

    *out_mem = mem;
    *out_mem_size = total_mem;
    *code_size = fsize;
    return true;
}

// -------------------------------------------------------------

static void r5vm_dump_state(const r5vm_t* vm)
{
    if (!vm) return;

    fprintf(stderr, "---- R5VM STATE DUMP ----\n");
    fprintf(stderr, " PC:  0x%08X\n", vm->pc);

    for (int i = 0; i < 32; i++) {
        // 8 registers per line, column aligned
        if (i % 8 == 0)
            fprintf(stderr, " x%-2d:", i);
        fprintf(stderr, " %08X", vm->regs[i]);
        if (i % 8 == 7 || i == 31)
            fprintf(stderr, "\n");
    }

    fprintf(stderr, " MEM: 0x%p .. 0x%p (%" PRIu32 " bytes)\n",
            (void*)vm->mem, (void*)(vm->mem + vm->mem_size - 1), vm->mem_size);
    fprintf(stderr, "--------------------------\n");
}

void r5vm_error(r5vm_t* vm, const char* msg, uint32_t pc, uint32_t instr)
{
    fprintf(stderr, "R5VM ERROR at PC=0x%08X: %s (instr=0x%08X)\n", pc, msg, instr);

    r5vm_dump_state(vm);
}

// -------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <binary> [--mem N|Nk|Nm]\n", argv[0]);
        return 1;
    }

    size_t override_mem = 0;
    if (argc >= 4 && strcmp(argv[2], "--mem") == 0)
        override_mem = parse_mem_arg(argv[3]);

    uint8_t* mem = NULL;
    size_t mem_size = 0;
    uint32_t code_size = 0;
    if (!load_file(argv[1], &mem, &mem_size, override_mem, &code_size)) {
        return 1;
    }

    r5vm_t vm;
    if (!r5vm_init(&vm, mem, (uint32_t)mem_size, code_size)) {
        fprintf(stderr, "error: r5vm_init failed\n");
        free(mem);
        return 1;
    }
    r5vm_reset(&vm);

    // clone vm
    uint8_t* jitmem = malloc(vm.mem_size);
    assert(jitmem);
    memcpy(jitmem, vm.mem, vm.mem_size);

    // Run in interpreter
    r5vm_run(&vm, 0);

    r5vm_t vmjit;
    r5vm_init(&vmjit, jitmem, vm.mem_size, vm.code_size);
    r5vm_reset(&vmjit);

    // Run in JIT
    r5jit_x86(&vmjit);

    // compare result
    for (int i = 0; i < 32; i++) {
        if (vm.regs[i] != vmjit.regs[i])
        {
            printf("Warning register %i mismatch: %i vs. %i\n", i, vm.regs[i], vmjit.regs[i]);
        }
    }

    // cleanup
    r5vm_destroy(&vm);
    r5vm_destroy(&vmjit);
    free(mem);
    free(jitmem);

    return 0;
}

