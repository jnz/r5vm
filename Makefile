# --- R5VM Host Makefile ----------------------------------------------------

R5VMFLAGS ?= -DR5VM_DEBUG

CC      ?= gcc
CFLAGS  ?= -m32 -std=gnu17 -O1 -Wall -Wextra $(R5VMFLAGS) -MMD -MP

TARGET  := r5vm
SRC_DIR := src
BUILD_DIR := build

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@$(MAKE) -C tests clean

test:
	@$(MAKE) -C tests run

-include $(DEP)

.PHONY: all clean test

