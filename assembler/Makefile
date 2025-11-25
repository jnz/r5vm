CROSS = riscv64-unknown-elf
ARCH  = -march=rv32i -mabi=ilp32

all: mini.elf mini.bin

mini.elf: mini.S
	$(CROSS)-gcc $(ARCH) -nostdlib -nostartfiles -Wl,--no-relax -o $@ $<

mini.bin: mini.elf
	$(CROSS)-objcopy -O binary $< $@

dump: mini.elf mini.bin
	$(CROSS)-objdump -d mini.elf
	xxd -c 4 mini.bin

clean:
	rm -f mini.elf mini.bin

