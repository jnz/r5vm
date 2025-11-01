/* r5vm.ld */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : {
        *(.text.entry)
        *(.text*)
    } > RAM

    .rodata : {
        *(.rodata*)
    } > RAM

    .data : {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > RAM AT > RAM

    _sidata = _sdata; /* for CRT0 */

    .bss (NOLOAD) : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > RAM

    . = ALIGN(4);
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}

