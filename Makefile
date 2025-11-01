# --- R5VM Host Makefile ----------------------------------------------------

R5VMFLAGS ?= -DR5VM_DEBUG

CC      ?= gcc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra $(R5VMFLAGS)
TARGET  ?= r5vm
SRC     = main.c r5vm.c
OBJ     = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c r5vm.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
