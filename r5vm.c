#include "r5vm.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ---- Defines ---------------------------------------------------------------

#define IS_POWER_OF_TWO(n)  ((n) != 0 && ((n) & ((n) - 1)) == 0)

#define SIGN_EXT(x,bits)    ((int32_t)((x) << (32 - (bits))) >> (32 - (bits)))

#define OPCODE(inst)        ((inst) & 0x7F)
#define RD(inst)            (((inst) >> 7)  & 0x1F)
#define FUNCT3(inst)        (((inst) >> 12) & 0x07)
#define RS1(inst)           (((inst) >> 15) & 0x1F)
#define RS2(inst)           (((inst) >> 20) & 0x1F)
#define FUNCT7(inst)        (((inst) >> 25) & 0x7F)
#define IMM_I(inst)         SIGN_EXT(((inst) >> 20) & 0xFFF, 12)
#define IMM_S(inst)         SIGN_EXT(((((inst) >> 25) << 5) | \
                                      ((inst >> 7) & 0x1F)), 12)
#define IMM_U(inst)         ((uint32_t)(inst) & 0xFFFFF000)
#define IMM_B(inst)         SIGN_EXT(((((inst) >> 31) & 0x1) << 12) | ((((inst) >> 7) & 0x1) << 11) | ((((inst) >> 25) & 0x3F) << 5) | ((((inst) >> 8) & 0xF) << 1), 13)
#define IMM_J(inst) \
    SIGN_EXT( \
        (((inst >> 31) & 0x1) << 20) | \
        (((inst >> 12) & 0xFF) << 12) | \
        (((inst >> 20) & 0x1) << 11) | \
        (((inst >> 21) & 0x3FF) << 1), \
        21)

#define R5VM_OPCODE_R_TYPE  0x33
#define R5VM_OPCODE_I_TYPE  0x13
#define R5VM_OPCODE_LW      0x03
#define R5VM_OPCODE_SW      0x23
#define R5VM_OPCODE_SYSTEM  0x73
#define R5VM_OPCODE_AUIPC   0x17
#define R5VM_OPCODE_BRANCH  0x63
#define R5VM_OPCODE_LUI     0x37
#define R5VM_OPCODE_JAL     0x6F
#define R5VM_OPCODE_JALR    0x67
#define R5VM_OPCODE_FENCE   0x0F

/* Function 3 (F3) */
#define R5VM_R_F3_ADD_SUB   0x00
#define R5VM_R_F3_XOR       0x04
#define R5VM_R_F3_OR        0x06
#define R5VM_R_F3_AND       0x07
#define R5VM_R_F3_SLL       0x01
#define R5VM_R_F3_SRL       0x05
#define R5VM_R_F3_SRA       0x05
#define R5VM_R_F3_SLT       0x02
#define R5VM_R_F3_SLTU      0x03

#define R5VM_I_F3_ADDI      0x00
#define R5VM_I_F3_XORI      0x04
#define R5VM_I_F3_ORI       0x06
#define R5VM_I_F3_ANDI      0x07
#define R5VM_I_F3_SLLI      0x01
#define R5VM_I_F3_SRLI_SRAI 0x05
#define R5VM_I_F3_SLTI      0x02
#define R5VM_I_F3_SLTIU     0x03

#define R5VM_I_F3_LB        0x00
#define R5VM_I_F3_LH        0x01
#define R5VM_I_F3_LW        0x02 // Load Word: R[rd] = M[R[rs1]+SE(imm)]
#define R5VM_I_F3_LBU       0x04
#define R5VM_I_F3_LHU       0x05

#define R5VM_S_F3_SB        0x00
#define R5VM_S_F3_SH        0x01
#define R5VM_S_F3_SW        0x02 // Store Word: M[R[rs1]+SE(imm)] = R[rs2]

#define R5VM_B_F3_BEQ       0x00 // Branch if Equal
#define R5VM_B_F3_BNE       0x01 // Branch if Not Equal
#define R5VM_B_F3_BLT       0x04 // Branch if Less Than
#define R5VM_B_F3_BGE       0x05 // Branch if Greater or Equal
#define R5VM_B_F3_BLTU      0x06 // Branch if Unsigned Less Than
#define R5VM_B_F3_BGEU      0x07 // Branch if Unsigned Greater or Equal

/* Function 7 */
#define R5VM_R_F7_ADD       0x00
#define R5VM_R_F7_SUB       0x20
#define R5VM_R_F7_SRL       0x00
#define R5VM_R_F7_SRA       0x20
#define R5VM_I_F7_SRLI      0x00
#define R5VM_I_F7_SRAI      0x20
#define R5VM_I_F7_SLLI      0x00

// ---- Functions -------------------------------------------------------------

bool r5vm_init(r5vm_t* vm, size_t mem_size, uint8_t* mem)
{
    if (vm == NULL || mem_size == 0 || mem == NULL)
    {
        return false;
    }

    if (!IS_POWER_OF_TWO(mem_size))
    {
        return false;
    }

    memset(vm, 0, sizeof(r5vm_t));
    vm->mem = mem;
    vm->mem_size = mem_size;
    vm->mem_mask = (uint32_t)mem_size - 1;
    return true;
}

void r5vm_destroy(r5vm_t* vm)
{
    memset(vm, 0, sizeof(r5vm_t));
}

void r5vm_reset(r5vm_t* vm)
{
    memset(vm->regs, 0, sizeof vm->regs);
    vm->pc = 0;
}

bool r5vm_load(r5vm_t* vm, const void* bin, size_t len)
{
    if (bin == NULL || len == 0 || len > vm->mem_size)
    {
        return false;
    }
    memcpy(vm->mem, bin, len);
    return true;
}

bool r5vm_step(r5vm_t* vm)
{
    bool retcode = true;
#ifdef R5VM_DEBUG
    if (vm->pc > vm->mem_size - 4)
    {
        r5vm_error(vm, "PC out of bounds", vm->pc, 0);
        return false;
    }
#endif

    uint32_t inst =  vm->mem[(vm->pc + 0) & vm->mem_mask]
                  | (vm->mem[(vm->pc + 1) & vm->mem_mask] << 8)
                  | (vm->mem[(vm->pc + 2) & vm->mem_mask] << 16)
                  | (vm->mem[(vm->pc + 3) & vm->mem_mask] << 24);
    vm->pc += 4;
    const uint32_t rd  = RD(inst);
    const uint32_t rs1 = RS1(inst);
    const uint32_t rs2 = RS2(inst);
    uint32_t* R = vm->regs;

#ifdef R5VM_DEBUG
    uint8_t  debug_rd     = RD(inst);
    uint8_t  debug_rs1    = RS1(inst);
    uint8_t  debug_rs2    = RS2(inst);
    uint16_t debug_funct3 = FUNCT3(inst);
    uint16_t debug_funct7 = FUNCT7(inst);
    int16_t  debug_imm_i  = IMM_I(inst);
    int16_t  debug_imm_s  = IMM_S(inst);
    uint32_t debug_imm_u  = IMM_U(inst);
#endif

    uint8_t opcode = OPCODE(inst);
    switch (opcode)
    {
    /* _--------------------- R-Type instuctions ---------------------_ */
    case (R5VM_OPCODE_R_TYPE):
        if ((FUNCT3(inst) == R5VM_R_F3_ADD_SUB) &&
            (FUNCT7(inst) == R5VM_R_F7_ADD))
        {
            R[RD(inst)] = R[rs1] + R[rs2];
        }
        else if ((FUNCT3(inst) == R5VM_R_F3_ADD_SUB) &&
                 (FUNCT7(inst) == R5VM_R_F7_SUB))
        {
            R[RD(inst)] = R[rs1] - R[rs2];
        }
        else if ((FUNCT3(inst) == R5VM_R_F3_XOR))
        {
            R[RD(inst)] = R[rs1] ^ R[rs2];
        }
        else if ((FUNCT3(inst) == R5VM_R_F3_OR))
        {
            R[RD(inst)] = R[rs1] | R[rs2];
        }
        else if ((FUNCT3(inst) == R5VM_R_F3_AND))
        {
            R[RD(inst)] = R[rs1] & R[rs2];
        }
        else if ((FUNCT3(inst) == R5VM_R_F3_SLL))
        {
            R[RD(inst)] = R[rs1] << (R[rs2] & 0x1F);
        }
        else if ((FUNCT3(inst) == R5VM_R_F3_SRL) && (FUNCT7(inst) == R5VM_R_F7_SRL))
        {
            R[RD(inst)] = R[rs1] >> (R[rs2] & 0x1F);
        }
        else if ((FUNCT3(inst) == R5VM_R_F3_SRA) && (FUNCT7(inst) == R5VM_R_F7_SRA))
        {
            R[RD(inst)] = ((int32_t)R[rs1]) >> (R[rs2] & 0x1F);
        }
        else if ((FUNCT3(inst) == R5VM_R_F3_SLT))
        {
            R[RD(inst)] = ((int32_t)R[rs1] < (int32_t)R[rs2]) ? 1 : 0;
        }
        else if ((FUNCT3(inst) == R5VM_R_F3_SLTU))
        {
            R[RD(inst)] = (R[rs1] < R[rs2]) ? 1 : 0;
        }
#ifdef R5VM_DEBUG
        else
        {
            r5vm_error(vm, "Unknown R-type funct3", vm->pc-4, inst);
        }
#endif
        break;
    /* _--------------------- I-Type instuctions ---------------------_ */
    case (R5VM_OPCODE_I_TYPE):
        if (FUNCT3(inst) == R5VM_I_F3_ADDI)
        {
            R[RD(inst)] = R[rs1] + IMM_I(inst);
        }
        else if (FUNCT3(inst) == R5VM_I_F3_XORI)
        {
            R[RD(inst)] = R[rs1] ^ IMM_I(inst);
        }
        else if (FUNCT3(inst) == R5VM_I_F3_ORI)
        {
            R[RD(inst)] = R[rs1] | IMM_I(inst);
        }
        else if (FUNCT3(inst) == R5VM_I_F3_ANDI)
        {
            R[RD(inst)] = R[rs1] & IMM_I(inst);
        }
        else if ((FUNCT3(inst) == R5VM_I_F3_SLLI))
        {
            if (FUNCT7(inst) == R5VM_I_F7_SLLI)
            {
                R[RD(inst)] = R[rs1] << (IMM_I(inst) & 0x1F);
            }
#ifdef R5VM_DEBUG
            else
            {
                r5vm_error(vm, "Unknown I-type SLLI funct7", vm->pc-4, inst);
                retcode = false;
                break;
            }
#endif
        }
        else if ((FUNCT3(inst) == R5VM_I_F3_SRLI_SRAI))
        {
            if (FUNCT7(inst) == R5VM_I_F7_SRLI)
                R[RD(inst)] = R[rs1] >> (IMM_I(inst) & 0x1F);
            else if (FUNCT7(inst) == R5VM_I_F7_SRAI)
                R[RD(inst)] = ((int32_t)R[rs1]) >> (IMM_I(inst) & 0x1F);
#ifdef R5VM_DEBUG
            else
            {
                r5vm_error(vm, "Unknown I-type SRLI/SRAI funct7", vm->pc-4, inst);
                retcode = false;
            }
#endif
        }
        else if (FUNCT3(inst) == R5VM_I_F3_SLTI)
        {
            R[RD(inst)] = ((int32_t)R[rs1] < IMM_I(inst)) ? 1 : 0;
        }
        else if (FUNCT3(inst) == R5VM_I_F3_SLTIU)
        {
            R[RD(inst)] = (R[rs1] < (uint32_t)IMM_I(inst)) ? 1 : 0;
        }
#ifdef R5VM_DEBUG
        else
        {
            r5vm_error(vm, "Unknown I-type funct3", vm->pc-4, inst);
            retcode = false;
        }
#endif
        break;
    /* _--------------------- AUIPC -----------------------------------_ */
    case (R5VM_OPCODE_AUIPC):
        R[RD(inst)] = vm->pc - 4 + IMM_U(inst);
        break;
    /* _--------------------- LUI -------------------------------------_ */
    case (R5VM_OPCODE_LUI):
        R[RD(inst)] = IMM_U(inst);
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

        if (FUNCT3(inst) == R5VM_I_F3_LW)
        {
            R[RD(inst)] = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
        }
        else if (FUNCT3(inst) == R5VM_I_F3_LB)
        {
            R[RD(inst)] = (int8_t)b0;
        }
        else if (FUNCT3(inst) == R5VM_I_F3_LH)
        {
            R[RD(inst)] = (int16_t)(b0 | (b1 << 8));
        }
        else if (FUNCT3(inst) == R5VM_I_F3_LBU)
        {
            R[RD(inst)] = b0;
        }
        else if (FUNCT3(inst) == R5VM_I_F3_LHU)
        {
            R[RD(inst)] = b0 | (b1 << 8);
        }
#ifdef R5VM_DEBUG
        else
        {
            r5vm_error(vm, "Unknown Load funct3", vm->pc-4, inst);
            retcode = false;
        }
#endif
        }
        break;
    /* _--------------------- Store ----------------------------------_ */
    case (R5VM_OPCODE_SW):
        {
        const uint32_t addr = R[rs1] + IMM_S(inst);
        const uint32_t val  = R[rs2];
#ifdef R5VM_DEBUG
        if (addr > vm->mem_size - 4)
        {
            r5vm_error(vm, "Memory access out of bounds", vm->pc-4, inst);
            retcode = false;
            break;
        }
#endif
        if (FUNCT3(inst) == R5VM_S_F3_SW)
        {
            vm->mem[(addr + 0) & vm->mem_mask] = (val >> 0)  & 0xFF;
            vm->mem[(addr + 1) & vm->mem_mask] = (val >> 8)  & 0xFF;
            vm->mem[(addr + 2) & vm->mem_mask] = (val >> 16) & 0xFF;
            vm->mem[(addr + 3) & vm->mem_mask] = (val >> 24) & 0xFF;
        }
        else if (FUNCT3(inst) == R5VM_S_F3_SH)
        {
            vm->mem[(addr + 0) & vm->mem_mask] = (val >> 0)  & 0xFF;
            vm->mem[(addr + 1) & vm->mem_mask] = (val >> 8)  & 0xFF;
        }
        else if (FUNCT3(inst) == R5VM_S_F3_SB)
        {
            vm->mem[(addr + 0) & vm->mem_mask] = (val >> 0)  & 0xFF;
        }
#ifdef R5VM_DEBUG
        else
        {
            r5vm_error(vm, "Unknown Store funct3", vm->pc-4, inst);
            retcode = false;
        }
#endif
        }
        break;
    /* _--------------------- Branch ---------------------------------_ */
    case (R5VM_OPCODE_BRANCH):
        if (FUNCT3(inst) == R5VM_B_F3_BEQ)
        {
            if (R[rs1] == R[rs2])
            {
                vm->pc = vm->pc - 4 + IMM_B(inst);
            }
        }
        else if (FUNCT3(inst) == R5VM_B_F3_BNE)
        {
            if (R[rs1] != R[rs2])
            {
                vm->pc = vm->pc - 4 + IMM_B(inst);
            }
        }
        else if (FUNCT3(inst) == R5VM_B_F3_BLT)
        {
            if ((int32_t)R[rs1] < (int32_t)R[rs2])
            {
                vm->pc = vm->pc - 4 + IMM_B(inst);
            }
        }
        else if (FUNCT3(inst) == R5VM_B_F3_BGE)
        {
            if ((int32_t)R[rs1] >= (int32_t)R[rs2])
            {
                vm->pc = vm->pc - 4 + IMM_B(inst);
            }
        }
        else if (FUNCT3(inst) == R5VM_B_F3_BLTU)
        {
            if (R[rs1] < R[rs2])
            {
                vm->pc = vm->pc - 4 + IMM_B(inst);
            }
        }
        else if (FUNCT3(inst) == R5VM_B_F3_BGEU)
        {
            if (R[rs1] >= R[rs2])
            {
                vm->pc = vm->pc - 4 + IMM_B(inst);
            }
        }
#ifdef R5VM_DEBUG
        else
        {
            r5vm_error(vm, "Unknown Branch funct3", vm->pc-4, inst);
            retcode = false;
        }
#endif
        break;
    /* _--------------------- JAL ------------------------------------_ */
    case (R5VM_OPCODE_JAL):
        R[RD(inst)] = vm->pc;
        vm->pc = vm->pc - 4 + IMM_J(inst);
        break;
    /* _--------------------- JALR -----------------------------------_ */
    case (R5VM_OPCODE_JALR):
        if (FUNCT3(inst) == 0x0)
        {
            R[RD(inst)] = vm->pc;
            vm->pc = (R[rs1] + IMM_I(inst)) & ~1U;
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
        switch (syscall_id)
        {
        case 0:
            retcode = false;
            break;
        case 1:
            putchar(vm->a0 & 0xff);
            fflush();
            break;
        default:
            r5vm_error(vm, "Unknown ECALL", vm->pc - 4, syscall_id);
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

int r5vm_run(r5vm_t* vm, int max_steps)
{
    int i;
    for (i = 0; i < max_steps || max_steps < 0; ++i)
    {
        if (!r5vm_step(vm))
        {
            break;
        }
    }
    return i;
}

