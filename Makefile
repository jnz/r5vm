# --- R5VM Host Makefile ----------------------------------------------------
CC      ?= gcc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra
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
