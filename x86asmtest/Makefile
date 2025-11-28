CC = gcc
AS = gcc
OBJDUMP ?= objdump
CFLAGS = -m32 -O0
ASFLAGS = -m32
LDFLAGS = -m32 -nostdlib -Wl,-e,_start

all: test.exe listing

test.o: test.s
	$(AS) $(ASFLAGS) -c $< -o $@

test.exe: test.o
	$(CC) $(LDFLAGS) $< -o $@

listing: test.exe
	$(OBJDUMP) -d -Mintel test.exe > test.lst

bytelist: test.exe
	$(OBJDUMP) -d -Mintel --insn-width=15 test.exe

clean:
	rm -f test.o test.lst test.exe

