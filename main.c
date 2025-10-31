// main.c – simple R5VM loader & runner
#include <stdio.h>
#include <stdlib.h>
#include "r5vm.h"   // dein VM-Header

// -------------------------------------------------------------

static bool load_file(const char* path, size_t* out_size,
                      uint8_t* mem, const size_t mem_size) {
    FILE* f = fopen(path, "rb");
    printf("[r5vm] opening file: %s\n", path);
    if (!f) {
        perror("fopen");
        return false;
    }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    if (size <= 0) {
        fclose(f);
        fprintf(stderr, "error: empty file\n");
        return false;
    }
    rewind(f);

    if (size > mem_size) {
        fclose(f);
        fprintf(stderr, "error: binary too large (%zu bytes, mem: %zu bytes)\n", size, mem_size);
        return false;
    }

    printf("[r5vm] loading %s (%zu bytes)\n", path, size);
    if (fread(mem, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        fprintf(stderr, "error: fread failed\n");
        return false;
    }
    fclose(f);
    if (out_size) *out_size = (size_t)size;
    return true;
}

// -------------------------------------------------------------

void r5vm_error(r5vm_t* vm, const char* msg, uint32_t pc, uint32_t instr)
{
    fprintf(stderr, "R5VM ERROR at PC=0x%08X: %s (instr=0x%08X)\n", pc, msg, instr);
    exit(1);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <binary>\n", argv[0]);
        return 1;
    }

    r5vm_t vm;
    static uint8_t mem[65536];
    size_t mem_size = sizeof(mem);

    fprintf(stdout, "[r5vm] R5VM v%s - Base ISA: %s\n", R5VM_VERSION, R5VM_BASE_ISA);
    fprintf(stdout, "[r5vm] Memory %zu bytes\n", mem_size);

    size_t bin_size = 0;
    if (!load_file(argv[1], &bin_size, mem, mem_size))
    {
        return -1;
    }

    if (!r5vm_init(&vm, mem_size, mem)) {
        fprintf(stderr, "error: r5vm_create failed\n");
        return 1;
    }

    r5vm_reset(&vm);
    printf("[r5vm] starting execution...\n");
    int rc = r5vm_run(&vm, -1);

    printf("[r5vm] finished, rc=%d\n", rc);

    r5vm_destroy(&vm);
    return 0;
}
