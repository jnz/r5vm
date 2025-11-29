CROSS = riscv64-unknown-elf
ARCH  = -march=rv32i -mabi=ilp32

all: firmware.elf firmware.r5m

firmware.elf: firmware.S
	$(CROSS)-gcc $(ARCH) -nostdlib -nostartfiles -Wl,--no-relax -T link.ld -o $@ $<

firmware.bin: firmware.elf
	$(CROSS)-objcopy -O binary $< $@

firmware.r5m: firmware.elf
	python ../r5asm/r5asm.py --verbose firmware.elf firmware.r5m

dump: firmware.elf firmware.bin
	xxd -c 4 firmware.bin
	echo ""
	$(CROSS)-objdump -d firmware.elf
	riscv64-unknown-elf-readelf -s firmware.elf > firmware.sym

clean:
	rm -f firmware.r5m firmware.elf firmware.bin

