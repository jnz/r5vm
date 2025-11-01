# R5VM Test Common Macros
# Provides helper macros for writing RISC-V tests

.macro TEST_PASS
    li a0, 0        # a0 = 0 means success
    li a7, 0        # syscall 0 = exit
    ecall
.endm

.macro TEST_FAIL code
    li a0, \code    # a0 = error code
    li a7, 0        # syscall 0 = exit
    ecall
.endm

# Assert that register equals immediate value
.macro ASSERT_EQ reg, expected
    li t6, \expected
    bne \reg, t6, fail_\@
    j pass_\@
fail_\@:
    TEST_FAIL 1
pass_\@:
.endm

# Assert that two registers are equal
.macro ASSERT_EQ_REG reg1, reg2
    bne \reg1, \reg2, fail_\@
    j pass_\@
fail_\@:
    TEST_FAIL 1
pass_\@:
.endm

# Load immediate (handles large immediates)
.macro LI32 reg, value
    lui \reg, %hi(\value)
    addi \reg, \reg, %lo(\value)
.endm
