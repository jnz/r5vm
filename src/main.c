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
#include "r5jit.h"
#include "hires_time.h"

// -------------------------------------------------------------

#define R5VM_MIN_MEM_SIZE   (64 * 1024) // 64 KiB
#define RED   "\x1b[31m"
#define RESET "\x1b[0m"
// -------------------------------------------------------------

/** @brief Helper function: Parse memory size argument from command line.
 *
 * Supports suffixes:
 *  - k: kilobytes
 *  - m: megabytes
 *
 * @param s Input string
 * @return Parsed size in bytes
 */
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
            fprintf(stderr, "warning: unknown suffix '%c' in mem size, using "
                            "bytes\n", *end);
    }
    return (size_t)val;
}

// -------------------------------------------------------------

/** @brief Read little-endian 16-bit value from byte array. */
static uint16_t rd16le(const uint8_t* p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/** @brief Read little-endian 32-bit value from byte array. */
static uint32_t rd32le(const uint8_t* p)
{
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

/** @brief Calculate actual memory size for machine. Make sure it is a power of two. */
static uint32_t mem_size_power2(size_t override_mem, size_t fsize)
{
    size_t total_mem = (size_t)fsize;
    if (total_mem < override_mem) {
        total_mem = override_mem;
    }
    // Make sure that total_mem is power of two
    size_t pow2_mem = 2;
    while (pow2_mem < total_mem)
        pow2_mem *= 2;
    total_mem = pow2_mem;
    return total_mem;
}

/** @brief Load r5vm binary from file into VM instance.
 *
 * Target image in vm->mem after this call:
 * Loads .CODE to load_addr, .DATA after that, zeroes .BSS.
 * So vm->mem looks like this:
 * +-----------------+  <- load_addr
 * |      .CODE      |
 * +-----------------+
 * |      .DATA      |
 * +-----------------+
 * |      .BSS       |
 * +-----------------+
 * |                 |
 * +-----------------+ <- vm->mem_size
 *
 *
 * @param path Path to .r5m file
 * @param vm Pointer to (unintialized) r5vm struct to initialize
 * @param mem_size_requested Requested memory size (0 = use image default)
 * @return 0 on success, negative value on error
 */
int r5vm_load(const char* path, r5vm_t* vm, size_t mem_size_requested)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open file: %s", path);
        return -1;
    }

    r5vm_header_t h = { 0 };
    r5vm_header_t hdr_raw;
    if (fread(&hdr_raw, sizeof(hdr_raw), 1, f) != 1) {
        fprintf(stderr, "Could not load header from file: %s", path);
        fclose(f);
        return -2;
    }
    memcpy(h.magic_str, hdr_raw.magic_str, 4);
    h.version     = rd16le((uint8_t*) &hdr_raw.version);
    h.flags       = rd16le((uint8_t*) &hdr_raw.flags);
    h.entry       = rd32le((uint8_t*) &hdr_raw.entry);
    h.load_addr   = rd32le((uint8_t*) &hdr_raw.load_addr);
    h.ram_size    = rd32le((uint8_t*) &hdr_raw.ram_size);
    h.code_offset = rd32le((uint8_t*) &hdr_raw.code_offset);
    h.code_size   = rd32le((uint8_t*) &hdr_raw.code_size);
    h.data_offset = rd32le((uint8_t*) &hdr_raw.data_offset);
    h.data_size   = rd32le((uint8_t*) &hdr_raw.data_size);
    h.bss_size    = rd32le((uint8_t*) &hdr_raw.bss_size);
    h.total_size  = rd32le((uint8_t*) &hdr_raw.total_size);

    if (memcmp(h.magic_str, R5VM_MAGIC_STR, 4) != 0) {
        fprintf(stderr, "Invalid .r5m header");
        fclose(f);
        return -3;
    }
    if (h.flags & 1) {
        fprintf(stderr, "64-bit image not supported");
        fclose(f);
        return -4;
    }

    if (h.version != R5VM_FILE_VERSION) {
        fprintf(stderr, "Unsupported file version");
        fclose(f);
        return -4;
    }

    // RAM needed for loaded image (virtual size)
    uint32_t mem_size = mem_size_power2(mem_size_requested, h.ram_size);
    assert((mem_size & (mem_size - 1)) == 0);

    uint8_t* mem = calloc(1, mem_size); /* .BSS included */
    if (!mem) {
        fprintf(stderr, "Could not allocate: %i bytes", mem_size);
        fclose(f);
        return -5;
    }

    // Bounds check: load_addr + image must fit
    uint64_t needed = (uint64_t)h.code_size + h.data_size + h.bss_size;
    if (h.load_addr + needed > h.ram_size) {
        fprintf(stderr, "Unsupported load address: %u (memory: %u)",
                        h.load_addr, mem_size);
        free(mem);
        fclose(f);
        return -6;
    }

    // Load .CODE
    if (fseek(f, h.code_offset, SEEK_SET) != 0) {
        fprintf(stderr, "Could not read .code section");
        free(mem);
        fclose(f);
        return -7;
    }
    size_t n = fread(&mem[h.load_addr], 1, h.code_size, f);
    if (n != h.code_size) {
        fprintf(stderr, "Could not read .code section");
        free(mem);
        fclose(f);
        return -7;
    }

    // Load .DATA
    if (h.data_size > 0) {
        if (fseek(f, h.data_offset, SEEK_SET) != 0) {
        fprintf(stderr, "Could not read .data section");
            free(mem);
            fclose(f);
            return -7;
        }

        n = fread(&mem[h.load_addr + h.code_size], 1, h.data_size, f);
        if (n != h.data_size) {
            fprintf(stderr, "Could not read .data section");
            free(mem);
            fclose(f);
            return -7;
        }
    }

    fclose(f);

    // Initialize VM struct
    memset(vm, 0, sizeof(*vm));
    vm->mem         = mem;
    vm->mem_size    = mem_size;
    vm->mem_mask    = mem_size - 1;
    vm->code_offset = h.load_addr;
    vm->code_size   = h.code_size;
    vm->data_offset = h.load_addr + h.code_size;
    vm->data_size   = h.data_size;
    vm->bss_offset  = vm->data_offset + h.data_size;
    vm->bss_size    = h.bss_size;
    vm->entry       = h.entry & vm->mem_mask;

    r5vm_reset(vm);
    printf("vm: %s\n", path);
    if (h.ram_size != vm->mem_size) {
        printf("ram: 0x%08zx (RAM requirement specified in .r5m)\n", (size_t)h.ram_size);
    }
    printf("mem: 0x%08zx\n",   (size_t)vm->mem_size);
    printf("msk: 0x%08zx\n",   (size_t)vm->mem_mask);
    printf("bss: %zu bytes\n", (size_t)vm->bss_size);
    printf("txt: %zu bytes\n", (size_t)vm->code_size);
    printf("dat: %zu bytes\n", (size_t)vm->data_size);

    return 0;
}

// -------------------------------------------------------------

/** @brief Helper function: Dump VM state (PC, registers, memory range) to
 * stderr.
 *
 * @param vm Pointer to R5VM instance
 */
static void r5vm_dump_state(const r5vm_t* vm)
{
    if (!vm) return;

    fprintf(stderr, "----- R5VM STATE DUMP -----\n");
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
    fprintf(stderr, "---------------------------\n");
}

/** @brief Runtime Interpreter error handler: Print error message to stderr and
 * dump VM state.
 *
 * @param vm Pointer to R5VM instance
 * @param msg Error message
 * @param pc Program counter (RISC-V) where error occurred
 * @param instr Instruction that caused the error
 */
void r5vm_error(r5vm_t* vm, const char* msg, uint32_t pc, uint32_t instr)
{
    fprintf(stderr, "R5VM ERROR at PC=0x%08X: %s (instr=0x%08X)\n", pc, msg, instr);

    r5vm_dump_state(vm);
}

/** @brief JIT/AOT compilation error handler: Print error message to stderr.
 *
 * @param jit Pointer to current JIT buffer
 * @param msg Error message
 * @param pc Program counter (RISC-V, not e.g. x86) where error occurred
 * @param instr Instruction that caused the error
 */
void r5jit_error(r5jitbuf_t* jit, const char* msg, uint32_t pc, uint32_t instr)
{
    (void)jit;
    fprintf(stderr, "R5JIT ERROR at PC=0x%08X: %s (instr=0x%08X)\n", pc, msg, instr);
}

/** @brief Helper function: Dump memory differences between two memory areas
 * and print areas that differ to stdout.
 *
 * @param a First memory area (pointer)
 * @param b Second memory area (pointer)
 * @param len Length of memory areas to compare in bytes
 */
static void r5vm_dump_memdiff(const uint8_t* a, const uint8_t* b, size_t len)
{
    assert(a && b);
    const size_t block = 4;

    for (size_t addr = 0; addr < len; ) {
        if (a[addr] == b[addr]) {
            addr++;
            continue;
        }

        size_t start = addr & ~(block - 1);
        size_t end = start + block;
        if (end > len) { end = len; }

        printf("0x%08zx ", start);
        for (size_t i = start; i < end; i++) {
            printf("%02x ", a[i]);
        }
        printf("| ");
        for (size_t i = start; i < end; i++) {
            if (a[i] != b[i]) { printf(RED); }
            printf("%02x ", b[i]);
            if (a[i] != b[i]) { printf(RESET); }
        }
        printf("\n");

        addr = end;
    }
}

// -------------------------------------------------------------

/** @brief Main entry point: Load R5VM binary, run guest image. */
int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <binary> [--mem N|Nk|Nm]\n", argv[0]);
        return 1;
    }

    size_t override_mem = 0;
    if (argc >= 4 && strcmp(argv[2], "--mem") == 0)
        override_mem = parse_mem_arg(argv[3]);

    // ----------- INTERPRETER --------
    r5vm_t vm;
    char* path = argv[1];
    if (r5vm_load(path, &vm, override_mem) != 0) {
        return -2;
    }
    // Run in interpreter
    {
        hi_time t0 = hi_time_now();
        r5vm_run(&vm, 0);
        hi_time t1 = hi_time_now();
        printf("dt: %.3f us (interpreter)\n", 1000000.0*hi_time_elapsed(t0, t1));
    }

    // ------------- JIT --------------
    r5vm_t vmjit;
    if (r5vm_load(path, &vmjit, override_mem) != 0) {
        return -2;
    }
    bool r5jit_x86(r5vm_t* vm);
    r5jit_x86(&vmjit);

    // ---------------------------------
    // compare result
    // ---------------------------------
    assert(vm.zero == 0);
    if (memcmp(vm.regs, vmjit.regs, sizeof(vm.regs)) != 0)
    {
        printf(RED "Error:" RESET " register mismatch between interpreter and JIT\n");
        r5vm_dump_state(&vm);
        r5vm_dump_state(&vmjit);

        r5vm_dump_memdiff((uint8_t*)vm.regs, (uint8_t*)vmjit.regs, sizeof(vm.regs));
    }
    if ((vm.mem_size != vmjit.mem_size) ||
        (memcmp(vm.mem, vmjit.mem, vm.mem_size) != 0))
    {
        printf(RED "Error:" RESET " memory mismatch between interpretor and JIT\n");
        r5vm_dump_memdiff(vm.mem, vmjit.mem, vm.mem_size);
    }

    // Free memory
    free(vm.mem);
    free(vmjit.mem);

    return 0;
}

