# R5VM — Minimal RISC-V RV32I Virtual Machine

![logo](r5vmlogo.jpg)

*A compact and educational RISC-V emulator written in pure C.*

![C](https://img.shields.io/badge/language-C-blue)
![License: MIT](https://img.shields.io/badge/License-MIT-green)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)

**R5VM** is a compact, educational **RISC-V RV32I** virtual machine written in pure C.
It can execute small RISC-V bare-metal programs compiled with e.g. `gcc` into flat binary images.

R5VM focuses on simplicity. Unlike WebAssembly (WASM) or full RISC-V emulators,
it’s small enough to read and understand in one sitting, making it ideal for
education, debugging, and embedding rather than speed or completeness.

To integrate R5VM into another project, just drop in `r5vm.c` and `r5vm.h` - no
additional dependencies or build steps required.

## Features

- Full **RV32I base instruction set**
  (R/I/S/B/U/J types, including LUI/AUIPC/JAL/JALR)
- Simple, portable C (C11) code (builds with GCC, Clang, or MSVC)
- Easy embedding into other projects
- No dependencies, freestanding-friendly
- Deterministic execution, ideal for testing and teaching

## Directory Structure

```
r5vm/
├── main.c          # host loader & runner
├── Makefile        # run make to build host application "r5vm"
├── r5vm.c/.h       # VM core
├── guest/
│   ├── main.c      # guest example .c template
│   ├── r5vm.ld     # linker script for guest example
│   └── Makefile    # Run make to build guest binary "vm.r5m"
├── r5asm/
│   └── r5asm.py    # Tool to convert ELF to guest "*.r5m" images
└── visualstudio/
    └── r5vm.sln    # Visual Studio 2026 project file for host program
```

---

## Building the Host VM (`r5vm`)

For the 32-Bit JIT part, 32-Bit libraries are required as prerequisite:
```bash
sudo apt install gcc-multilib libc6-dev-i386
```bash

### GCC
```bash
make
```

### clang
```bash
make CC=clang
```

### Visual Studio (`r5vm.exe`)

Open `visualstudio/r5vm.sln` and build/run the project in Visual Studio.

## Running a Guest Program (`vm.r5m`)

### Building and Running a Guest Program with `gcc`

A minimal RISC-V toolchain (e.g. `riscv64-unknown-elf-gcc`) is required.

**Note:** On Ubuntu, install the toolchain via:
```bash
sudo apt install gcc-riscv64-unknown-elf
```

Inside the `guest/` directory, the provided Makefile builds a the guest into a binary.

The Makefile calls `r5asm.py` to convert the ELF output into a flat binary
image for the VM, but this relies on Python 3 being installed and the
`pyelftools` package being available:

```bash
pip install pyelftools
```

After installing the `r5asm.py` prerequisites, build the guest program with:

```bash
cd guest
make
```

This produces:

- `vm.r5m` - binary image (feed this to the host vm)
- `vm.elf` - ELF executable (in case you need it)
- `vm.list` - disassembly (to analyze the assembler output)

### Building the Guest Program with `clang`

```bash
cd guest
make CC=clang
```

**Note:** On Ubuntu, install the clang/LLVM toolchain via:

```bash
sudo apt install clang lld llvm
```

### Run the Program in the VM

```bash
./r5vm guest/vm.r5m
```

Optional: specify memory size explicitly (but the required memory is stored in
the `.r5m` file header):

```bash
./r5vm guest/vm.r5m --mem 2M
```

### Example Output from `guest/vm.r5m`:

```
Hello, world!
```

---

## Error Handling and State Dump

When an error occurs (invalid instruction, memory fault, etc.),
`r5vm_error()` is called.
By default, it prints the error and a full CPU state dump before exiting.

Example output:
```
R5VM ERROR at PC=0x00000014: Unknown opcode (instr=0xFFFFFFFF)
---- R5VM STATE DUMP ----
 PC:  0x00000014
 x0: 00000000 x1: 00000000 x2: 00001000 x3: 00000000 ...
 MEM: 0x00000100 .. 0x000100FF (65536 bytes = 64.00 KiB)
--------------------------
```

You can override the error handler by defining your own `r5vm_error()`.

---

## Guest Linker Script (`r5vm.ld`)

The guest program is linked into a single flat 64 KiB RAM image:

```ld
MEMORY {
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
}
```

---

## License

MIT License - free for educational and commercial use.
(c) 2025 Jan Zwiener, all rights reserved.

