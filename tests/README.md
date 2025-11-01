# r5vm Test Suite

Automated testing framework for the r5vm RISC-V Virtual Machine.

## Quick Start

```bash
cd tests
make run
```

This will:
1. Build the test runner (`test_runner`)
2. Compile all `test_*.s` files to `.bin` binaries
3. Execute all tests and report results

## Writing Tests

### Minimal Test Template

```asm
.section .text
.globl _start

_start:
    # Your test code here
    li a1, 10
    li a2, 20
    add a3, a1, a2

    # Check result
    li t0, 30
    bne a3, t0, fail

    # Test passed
    li a0, 0        # a0 = 0 means success
    li a7, 0        # syscall 0 = exit
    ecall

fail:
    li a0, 1        # a0 != 0 means failure
    li a7, 0
    ecall
```

### Test Conventions

1. **Test files**: Name pattern `test_*.s`
2. **Entry point**: Must have `_start` label
3. **Exit mechanism**: Use `ecall` with `a7 = 0`
4. **Success/Failure**:
   - `a0 = 0` → Test PASSED
   - `a0 != 0` → Test FAILED (a0 = error code)

### Using Test Macros

Include `test_common.s` for helper macros:

```asm
.include "test_common.s"

_start:
    li a1, 42
    li a2, 42
    ASSERT_EQ a1, 42      # Assert a1 == 42
    ASSERT_EQ_REG a1, a2  # Assert a1 == a2
    TEST_PASS             # Exit with success
```

## Test Runner Features

### Register Validation

The test runner automatically validates VM register state after execution:

```c
// In your test runner configuration (future enhancement):
reg_expect_t checks[] = {
    {.reg_num = 10, .expected = 0x12345678},  // Check a0
    {.reg_num = 11, .expected = 0xDEADBEEF},  // Check a1
};
```

### Output Format

```
=== r5vm Test Runner ===

[TEST] test_add.bin                           ... PASS (42 steps)
[TEST] test_sub.bin                           ... PASS (38 steps)
[TEST] test_and.bin                           ... PASS (56 steps)
[TEST] test_branches.bin                      ... PASS (72 steps)

================================
Tests run:    4
Tests passed: 4
Tests failed: 0
================================
```

### Failure Details

On failure, the runner dumps the complete VM state:

```
[TEST] test_example.bin ... FAIL (a0=0x00000001, expected=0x00000000)

=== Register Dump ===
PC: 0x00000124
x0  (zero): 0x00000000  (0)      x1  (ra  ): 0x00000000  (0)
x2  (sp  ): 0x00008000  (32768)  x3  (gp  ): 0x00000000  (0)
...
=====================
```

## Makefile Targets

- **`make all`** - Build test runner and all test binaries (default)
- **`make run`** - Build and execute all tests
- **`make clean`** - Remove all build artifacts
- **`make coverage`** - Run tests with gcov coverage (future)
- **`make help`** - Show help message

## Adding New Tests

1. Create `test_mytest.s` in the `tests/` directory
2. Implement your test following the conventions above
3. Run `make run`

The build system automatically discovers and builds all `test_*.s` files.

## Coverage Analysis (Future)

```bash
make coverage
```

This will:
1. Rebuild with gcov instrumentation
2. Run all tests
3. Generate coverage report in `r5vm.c.gcov`

## Requirements

- **RISC-V Toolchain**: `riscv64-unknown-elf-*`
- **Host Compiler**: `gcc` (for test runner)
- **Make**: GNU Make


## Troubleshooting

### Test binary not created

Check that your test has:
- `.section .text`
- `.globl _start`
- Valid RISC-V assembly

### Test times out

Default timeout is 10,000 steps. If your test needs more:

```c
// Modify test_runner.c or pass max_steps
```

### Register validation fails

Ensure your test properly sets expected values before `ecall`.

