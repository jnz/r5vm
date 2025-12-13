    .section .text.entry
    .globl _start
_start:
    la sp, _stack_top

    call main
    ebreak          # exit host vm
1:  j 1b

