#!/usr/bin/env python3
"""
r5asm: pack a RISC-V ELF into an r5vm firmware container (.r5m).

Supported ELF layouts:
  1) Separate PT_LOAD segments for CODE (executable) and DATA (non-executable)
  2) Single PT_LOAD segment containing .text/.rodata/.data
     (common in simple bare-metal setups)

.r5m layout (little endian):

    [Header] (64 bytes)
    [CODE bytes]
    [DATA bytes]

Header fields (all little endian):

    magic       : uint32  "r5vm" = 0x6d763572
    version     : uint16  = 1
    flags       : uint16  (bit0: 1 = RV64, 0 = RV32; others reserved)
    entry       : uint32  entry address (ELF e_entry or override)
    load_addr   : uint32  base load address (first PT_LOAD or override)
    code_offset : uint32  file offset to CODE
    code_size   : uint32  size of CODE in bytes
    data_offset : uint32  file offset to DATA (0 if no data)
    data_size   : uint32  size of DATA in bytes
    bss_size    : uint32  total BSS bytes (memsz - filesz over PT_LOADs)
    total_size  : uint32  total file size in bytes
    reserved    : 24 bytes, zero
"""

import sys
import argparse
import struct
from elftools.elf.elffile import ELFFile

R5VM_MAGIC   = 0x6d763572  # "r5vm" little endian
R5VM_VERSION = 1

# struct format: little-endian, 64 bytes total
# I   = uint32 magic
# H H = uint16 version, flags
# 8x uint32 = entry, load_addr, code_offset, code_size,
#             data_offset, data_size, bss_size, total_size
# 24s = reserved[24]
R5M_HEADER_FMT  = "<IHHIIIIIIII24s"
R5M_HEADER_SIZE = struct.calcsize(R5M_HEADER_FMT)


# ------------------------------------------------------------
# CLI parsing
# ------------------------------------------------------------
def parse_args(argv):
    p = argparse.ArgumentParser(
        description="Pack a RISC-V ELF into an R5M firmware file for r5vm."
    )
    p.add_argument("input", help="Input ELF file")
    p.add_argument("output", help="Output .r5m file")

    p.add_argument(
        "--rv64",
        action="store_true",
        help="Mark image as RV64 (default: RV32)",
    )
    p.add_argument(
        "--entry",
        help="Override entry address (hex or dec). Default: ELF e_entry",
    )
    p.add_argument(
        "--load-addr",
        help=(
            "Override base load address (hex or dec). "
            "Default: minimum PT_LOAD p_paddr/p_vaddr."
        ),
    )
    p.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Verbose output",
    )
    return p.parse_args(argv)


def parse_int_auto(s):
    """Parse integer in decimal or 0x-prefixed hex."""
    if s is None:
        return None
    s = s.strip().lower()
    base = 16 if s.startswith("0x") else 10
    return int(s, base)


# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------
def collect_load_segments(elf):
    """Return list of all PT_LOAD segments."""
    return [ph for ph in elf.iter_segments() if ph["p_type"] == "PT_LOAD"]


def extract_sections(elf, names):
    """
    Concatenate the raw bytes of all sections whose name is in `names`,
    in the order they appear in the ELF.
    """
    chunks = []
    for sec in elf.iter_sections():
        if sec.name in names:
            chunks.append(sec.data())
    return b"".join(chunks)


# ------------------------------------------------------------
# Core packer
# ------------------------------------------------------------
def build_r5m(input_path, output_path, rv64=False,
              entry_override=None, load_addr_override=None,
              verbose=False):

    # Keep the ELF file open while pyelftools reads from it.
    with open(input_path, "rb") as f:
        elf = ELFFile(f)

        entry = elf.header["e_entry"]

        load_segments = collect_load_segments(elf)
        if not load_segments:
            raise SystemExit("ERROR: No PT_LOAD segments found.")

        # Base load address: minimum p_paddr (or p_vaddr as fallback)
        load_addr = min((ph["p_paddr"] or ph["p_vaddr"])
                        for ph in load_segments)

        code_seg = None
        data_seg = None
        bss_size = 0

        # First pass: try to classify segments into CODE (executable) and DATA (non-executable)
        for ph in load_segments:
            is_exec = bool(ph["p_flags"] & 0x1)  # PF_X
            memsz  = ph["p_memsz"]
            filesz = ph["p_filesz"]

            if memsz > filesz:
                bss_size += (memsz - filesz)

            if is_exec:
                code_seg = ph
            else:
                data_seg = ph

        # Case 1: two separate PT_LOAD segments for code and data
        if code_seg is not None and data_seg is not None:
            if verbose:
                print("Using segment-based extraction (distinct CODE and DATA).")
            code_bytes = code_seg.data()
            data_bytes = data_seg.data()

            if verbose:
                seg_addr_code = (code_seg["p_paddr"] or code_seg["p_vaddr"])
                seg_addr_data = (data_seg["p_paddr"] or data_seg["p_vaddr"])
                print(
                    f"CODE segment: addr=0x{seg_addr_code:08x}, "
                    f"filesz=0x{code_seg['p_filesz']:x}, "
                    f"memsz=0x{code_seg['p_memsz']:x}"
                )
                print(
                    f"DATA segment: addr=0x{seg_addr_data:08x}, "
                    f"filesz=0x{data_seg['p_filesz']:x}, "
                    f"memsz=0x{data_seg['p_memsz']:x}"
                )

        else:
            # Case 2: single PT_LOAD (text + data in one), so split by sections
            if verbose:
                print("Using section-based extraction (single PT_LOAD).")

            code_bytes = extract_sections(
                elf, [".text", ".text.init", ".rodata"]
            )
            data_bytes = extract_sections(
                elf, [".data"]
            )

    # At this point, ELF file is closed, all bytes are in memory.

    # Apply overrides
    if entry_override is not None:
        entry = entry_override
    if load_addr_override is not None:
        load_addr = load_addr_override

    # Flags
    flags = 0
    if rv64:
        flags |= 0x0001  # bit0 = RV64

    # File layout
    code_offset = R5M_HEADER_SIZE
    code_size   = len(code_bytes)

    if len(data_bytes) > 0:
        data_offset = code_offset + code_size
        data_size   = len(data_bytes)
    else:
        data_offset = 0
        data_size   = 0

    total_size = R5M_HEADER_SIZE + code_size + data_size
    reserved   = b"\x00" * 24

    # Build header
    header = struct.pack(
        R5M_HEADER_FMT,
        R5VM_MAGIC,
        R5VM_VERSION,
        flags,
        entry,
        load_addr,
        code_offset,
        code_size,
        data_offset,
        data_size,
        bss_size,
        total_size,
        reserved,
    )

    if len(header) != R5M_HEADER_SIZE:
        raise RuntimeError("Internal error: header size mismatch")

    if verbose:
        print("R5M header:")
        print(f"  magic      = 0x{R5VM_MAGIC:08x}")
        print(f"  version    = 1")
        print(f"  flags      = 0x{flags:04x} ({'RV64' if rv64 else 'RV32'})")
        print(f"  entry      = 0x{entry:08x}")
        print(f"  load_addr  = 0x{load_addr:08x}")
        print(f"  code_off   = {code_offset} (0x{code_offset:x})")
        print(f"  code_size  = {code_size} (0x{code_size:x})")
        print(f"  data_off   = {data_offset} (0x{data_offset:x})")
        print(f"  data_size  = {data_size} (0x{data_size:x})")
        print(f"  bss_size   = {bss_size} (0x{bss_size:x})")
        print(f"  total_size = {total_size} (0x{total_size:x})")

    # Write .r5m
    with open(output_path, "wb") as out:
        out.write(header)
        out.write(code_bytes)
        if data_size > 0:
            out.write(data_bytes)

    if verbose:
        print(f"Wrote {output_path} ({total_size} bytes)")


# ------------------------------------------------------------
# Entry point
# ------------------------------------------------------------
def main(argv=None):
    args = parse_args(argv or sys.argv[1:])

    entry_override     = parse_int_auto(args.entry)
    load_addr_override = parse_int_auto(args.load_addr)

    try:
        build_r5m(
            input_path=args.input,
            output_path=args.output,
            rv64=args.rv64,
            entry_override=entry_override,
            load_addr_override=load_addr_override,
            verbose=args.verbose,
        )
    except SystemExit:
        raise
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

