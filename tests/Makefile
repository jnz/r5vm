# r5vm Test Suite Makefile

# Toolchain
CROSS   ?= riscv64-unknown-elf
AS      = $(CROSS)-as
LD      = $(CROSS)-ld
OBJCOPY = $(CROSS)-objcopy
OBJDUMP = $(CROSS)-objdump
CC      = gcc

# Architecture
ARCH    = rv32i
ABI     = ilp32

# Flags
ASFLAGS = -march=$(ARCH) -mabi=$(ABI)
LDFLAGS = -T test.ld -nostdlib -m elf32lriscv

# Find all test .s files (exclude test_common.s)
TEST_SOURCES = $(filter-out test_common.s, $(wildcard test_*.s))
TEST_BINS    = $(TEST_SOURCES:.s=.bin)
TEST_ELFS    = $(TEST_SOURCES:.s=.elf)
TEST_LISTS   = $(TEST_SOURCES:.s=.list)

# VM source (one level up)
VM_DIR = ..
VM_SRC = $(VM_DIR)/r5vm.c
VM_HDR = $(VM_DIR)/r5vm.h

# Test runners
RUNNER = test_runner_advanced
RUNNER_CFLAGS = -Wall -Wextra -std=c99 -I$(VM_DIR) -DR5VM_DEBUG -O2

GCOVR ?= gcovr
COV_HTML = coverage.html
COV_TXT  = coverage.txt

.PHONY: all clean run help coverage disasm

all: $(RUNNER) $(TEST_BINS)

# Build advanced test runner (recommended)
$(RUNNER): test_runner_advanced.c $(VM_SRC) $(VM_HDR)
	@echo "[CC] $@"
	@$(CC) $(RUNNER_CFLAGS) -o $@ test_runner_advanced.c $(VM_SRC)

# Assemble test .s -> .o
%.o: %.s test_common.s
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) -o $@ $<

# Link test .o -> .elf
%.elf: %.o test.ld
	@echo "[LD] $@"
	@$(LD) $(LDFLAGS) -o $@ $<

# Convert .elf -> .bin
%.bin: %.elf
	@echo "[OBJCOPY] $@"
	@$(OBJCOPY) -O binary $< $@

# Generate disassembly listing
%.list: %.elf
	@echo "[OBJDUMP] $@"
	@$(OBJDUMP) -D $< > $@

# Run all tests with default runner (advanced)
run: all
	@echo ""
	@echo "Running tests..."
	@echo ""
	@./$(RUNNER) $(TEST_BINS)

# Generate disassembly for all tests
disasm: $(TEST_LISTS)
	@echo "Disassembly files generated: $(TEST_LISTS)"

# Run tests with gcov coverage
coverage: RUNNER_CFLAGS += -fprofile-arcs -ftest-coverage
coverage: clean
	@echo ""
	@echo "Building with coverage instrumentation..."
	@echo ""
	@# Build VM with coverage
	@$(CC) $(RUNNER_CFLAGS) -c -o r5vm_cov.o $(VM_SRC)
	@# Build test runner with coverage and link with instrumented VM
	@$(CC) $(RUNNER_CFLAGS) -o $(RUNNER)_cov test_runner_advanced.c r5vm_cov.o -lgcov
	@echo ""
	@echo "Building test binaries..."
	@$(MAKE) --no-print-directory $(TEST_BINS)
	@echo ""
	@echo "Running tests with coverage..."
	@echo ""
	@./$(RUNNER)_cov $(TEST_BINS)
	@echo ""
	@echo "Generating coverage report..."
	@# Only generate coverage for r5vm.c (suppress system headers)
	@gcov r5vm_cov.gcda -o . 2>&1 | grep -E "(r5vm\.c|Lines executed)" || true
	@echo ""
	@echo "Generating gcovr summary..."
	@echo ""
	@if command -v $(GCOVR) >/dev/null 2>&1; then \
		echo "[gcovr] text summary:"; \
		$(GCOVR) -r .. --filter "$(VM_SRC)" --txt -s > $(COV_TXT) || true; \
		cat $(COV_TXT); \
		echo ""; \
		echo "[gcovr] HTML report:"; \
		$(GCOVR) -r .. --filter "$(VM_SRC)" --html-details $(COV_HTML) || true; \
		if [ -f $(COV_HTML) ]; then \
			echo "✓ HTML report generated: $(COV_HTML)"; \
		else \
			echo "✗ HTML report not generated (check gcovr output)"; \
		fi; \
	else \
		echo "✗ gcovr not installed, skipping summary."; \
	fi
	@# Clean up unwanted .gcov files from system headers
	@rm -f stdio.h.gcov string_fortified.h.gcov 2>/dev/null || true
	@echo ""
	@if [ -f r5vm.c.gcov ]; then \
		echo "✓ Coverage report generated: r5vm.c.gcov"; \
		echo ""; \
		LINES=$(head -5 r5vm.c.gcov | grep "Lines executed" | cut -d: -f2); \
		echo "Coverage: $LINES"; \
		echo ""; \
		echo "Commands:"; \
		echo "  cat r5vm.c.gcov | less       # View full report"; \
		echo "  grep '####' r5vm.c.gcov      # Find untested lines"; \
		echo "  grep -A2 '####' r5vm.c.gcov  # Show untested code"; \
	else \
		echo "✗ Error: r5vm.c.gcov not found"; \
	fi

clean:
	@echo "Cleaning..."
	@rm -f $(RUNNER) $(RUNNER)_cov
	@rm -f *.o *.elf *.bin *.list
	@rm -f *.gcda *.gcno *.gcov
	@rm -f r5vm_cov.o
	@rm -f $(COV_HTML) $(COV_TXT) coverage.css coverage*.html
	@# Clean up system header .gcov files
	@rm -f stdio.h.gcov string_fortified.h.gcov 2>/dev/null || true

help:
	@echo "r5vm Test Suite"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build test runners and all test binaries (default)"
	@echo "  run          - Build and run all tests (uses RUNNER=$(RUNNER))"
	@echo "  disasm       - Generate disassembly listings (*.list)"
	@echo "  coverage     - Run tests with gcov coverage analysis"
	@echo "  clean        - Remove all build artifacts"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Test Runners:"
	@echo "  test_runner_advanced  - Advanced runner (supports .expect files)"
	@echo ""
	@echo "Adding new tests:"
	@echo "  1. Create test_<name>.s"
	@echo "  2. Optionally create test_<name>.expect for register validation"
	@echo "  3. Run 'make run'"
	@echo ""
	@echo "Example .expect file format:"
	@echo "  a0 = 0           # Success code"
	@echo "  a1 = 0x12345678  # Expected value in a1"
	@echo "  s0 = 42          # Expected value in s0"
	@echo ""
	@echo "Current tests:"
	@for test in $(TEST_SOURCES); do \
		echo "  - $$test"; \
	done
