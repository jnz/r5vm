# --- R5VM Bare-Metal Makefile ------------------------------------
CROSS   = riscv64-unknown-elf
ARCH    = rv32i
ABI     = ilp32
CFLAGS  = -march=$(ARCH) -mabi=$(ABI) -ffreestanding -nostdlib -nostartfiles -O2
LDFLAGS = -T r5vm.ld

SRC     = crt0.s main.c
OBJ     = $(SRC:.c=.o)
OBJ     := $(OBJ:.s=.o)

all: vm.bin

%.o: %.s
	$(CROSS)-as -march=$(ARCH) -mabi=$(ABI) -o $@ $<

%.o: %.c
	$(CROSS)-gcc $(CFLAGS) -c -o $@ $<

vm.elf: $(OBJ) r5vm.ld
	$(CROSS)-gcc $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) -lgcc

vm.bin: vm.elf
	$(CROSS)-objcopy -O binary $< $@
	$(CROSS)-objdump -D $< > vm.dis

clean:
	rm -f *.o *.elf *.bin *.dis

