    .section .text.entry
    .globl _start
_start:
    la sp, _stack_top

    # --- Clear BSS ---
    la a0, _sbss
    la a1, _ebss
1:  bgeu a0, a1, 2f
    sw   x0, 0(a0)
    addi a0, a0, 4
    j    1b
2:

    # --- DATA ---
    la a0, _sdata      # Target
    la a1, _edata
    la a2, _sidata     # Source
3:  bgeu a0, a1, 4f
    lw   t0, 0(a2)
    sw   t0, 0(a0)
    addi a0, a0, 4
    addi a2, a2, 4
    j    3b
4:

    call main
    mv a0, a0       # Return value from main()
    li a7, 0        # ECALL 0 = Exit
    ecall
5:  j 5b

