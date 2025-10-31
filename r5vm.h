#ifndef R5VM_H
#define R5VM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define R5VM_VERSION     "0.1.0"
#define R5VM_BASE_ISA    "RV32I"

struct r5vm
{
    union {
        uint32_t regs[32];
        int32_t  regsi[32];
        struct {
            uint32_t zero, ra, sp, gp, tp;
            uint32_t t0, t1, t2;
            uint32_t s0, s1;
            uint32_t a0, a1, a2, a3, a4, a5, a6, a7;
            uint32_t s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
            uint32_t t3, t4, t5, t6;
        };
    };

    uint32_t pc;
    uint8_t* mem;
    size_t   mem_size;
    uint32_t mem_mask;
};
typedef struct r5vm r5vm_t;

// ---- Lifecycle -------------------------------------------------------------
bool    r5vm_init(r5vm_t* vm, size_t mem_size, uint8_t* mem);
void    r5vm_destroy(r5vm_t* vm);

// ---- Execution control -----------------------------------------------------
void    r5vm_reset(r5vm_t* vm);
bool    r5vm_load(r5vm_t* vm, const void* bin, size_t len);
bool    r5vm_step(r5vm_t* vm);
int     r5vm_run(r5vm_t* vm, int max_steps);

// ---- Error -----------------------------------------------------------------
void    r5vm_error(r5vm_t* vm, const char* msg, uint32_t pc, uint32_t instr);

#endif // R5VM_H

