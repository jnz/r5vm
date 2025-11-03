# --- R5VM Bare-Metal Makefile ------------------------------------

# Cross prefix and default toolchain
CROSS   ?= riscv64-unknown-elf

# Only define if unset or implicit default:
ifneq ($(filter undefined default,$(origin CC)),)
  CC := $(CROSS)-gcc
endif
ifneq ($(filter undefined default,$(origin AS)),)
  AS := $(CROSS)-as
endif
ifneq ($(filter undefined default,$(origin LD)),)
  LD := $(CROSS)-gcc
endif
ifneq ($(filter undefined default,$(origin OBJCOPY)),)
  OBJCOPY := $(CROSS)-objcopy
endif
ifneq ($(filter undefined default,$(origin OBJDUMP)),)
  OBJDUMP := $(CROSS)-objdump
endif
ifneq ($(filter undefined default,$(origin SIZE)),)
  SIZE := $(CROSS)-size
endif


# Architecture and ABI
ARCH    = rv32i
ABI     = ilp32
OPT     ?= -O2

SRC     = crt0.s benchmark.cpp qvmlib.c
OBJ     = $(SRC:.c=.o)
OBJ     := $(OBJ:.s=.o)

# Default flags (GCC)
CFLAGS  = -march=$(ARCH) -mabi=$(ABI) $(OPT)
ASFLAGS = -march=$(ARCH) -mabi=$(ABI)
LDFLAGS = -T r5vm.ld -nostdlib -nostartfiles
LIBS    = -lgcc

# --- Clang adjustments --------------------------------------------
ifeq ($(CC),clang)
  CROSS   :=
  AS      = clang
  LD      = clang
  OBJCOPY = llvm-objcopy
  OBJDUMP = llvm-objdump
  SIZE    = llvm-size

  CFLAGS  = --target=riscv32-unknown-elf -march=$(ARCH) -mabi=$(ABI) \
            -ffreestanding $(OPT)
  ASFLAGS = --target=riscv32-unknown-elf -march=$(ARCH) -mabi=$(ABI)
  LDFLAGS = -T r5vm.ld -nostdlib
  LIBS    = -lgcc
endif

$(info [r5vm] compiler:      $(CC))
$(info [r5vm] optimization:  $(OPT))
$(info [r5vm] arch/abi:      $(ARCH)/$(ABI))

# --- Include optional local overrides -----------------------------
-include config.mk

# --- Build rules --------------------------------------------------
all: cppbenchmarkvm.bin

%.o: %.s
	$(AS) $(ASFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

cppbenchmarkvm.elf: $(OBJ) r5vm.ld
	$(LD) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)
	@echo
	@echo "Section sizes (bytes):"
	@$(SIZE) -A -d $@

cppbenchmarkvm.bin: cppbenchmarkvm.elf
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -D $< > cppbenchmarkvm.list

clean:
	rm -f *.o *.elf *.bin *.list

