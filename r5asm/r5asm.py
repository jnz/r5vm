#!/usr/bin/env python3
"""
r5m image format:

    magic       : uint32  "r5vm" = 0x6d763572
    version     : uint16  = 2
    flags       : uint16  (bit0: 1 = RV64, 0 = RV32; others reserved)
    entry       : uint32  entry address (ELF e_entry or override)
    load_addr   : uint32  base load address (first PT_LOAD or override)
    ram_size    : uint32  total RAM size in bytes
    code_offset : uint32  file offset to CODE
    code_size   : uint32  size of CODE in bytes
    data_offset : uint32  file offset to DATA (0 if no data)
    data_size   : uint32  size of DATA in bytes
    bss_size    : uint32  total BSS bytes (memsz - filesz over PT_LOADs)
    total_size  : uint32  total file size in bytes
    reserved    : 20 bytes, zero
"""

import sys
import argparse
import struct
from elftools.elf.elffile import ELFFile

R5VM_MAGIC   = 0x6d763572  # "r5vm" LE
R5VM_VERSION = 2

R5M_HEADER_FMT  = "<IHHIIIIIIIII20s"
R5M_HEADER_SIZE = struct.calcsize(R5M_HEADER_FMT)


# ---------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------

def parse_args(argv):
    p = argparse.ArgumentParser(
        description="Pack RISC-V ELF into r5m image"
    )
    p.add_argument("input",  help="Input ELF file")
    p.add_argument("output", help="Output .r5m file")
    p.add_argument("-v", "--verbose", action="store_true")
    p.add_argument("--rv64", action="store_true", help="Mark as RV64")
    p.add_argument("--entry", help="Override entry address")
    p.add_argument("--load-addr", help="Override base load address")
    return p.parse_args(argv)


def parse_int_auto(v):
    if v is None:
        return None
    v = v.lower()
    if v.startswith("0x"):
        return int(v, 16)
    return int(v, 10)


def collect_load_segments(elf):
    return [ph for ph in elf.iter_segments() if ph["p_type"] == "PT_LOAD"]


def extract_sections_by_name(elf, wanted):
    """Concatenate bytes of sections listed in `wanted` (in file order)."""
    out = []
    for sec in elf.iter_sections():
        if sec.name in wanted:
            out.append(sec.data())
    return b"".join(out)


def get_symbol_value(elf, name):
    for sec in elf.iter_sections():
        if sec["sh_type"] == "SHT_SYMTAB":
            for sym in sec.iter_symbols():
                if sym.name == name:
                    return sym["st_value"]
    return None


# ---------------------------------------------------------------------
# Build r5m
# ---------------------------------------------------------------------

def build_r5m(path_in, path_out, rv64=False, entry_override=None,
              load_addr_override=None, verbose=False):

    with open(path_in, "rb") as f:
        elf = ELFFile(f)

        entry = elf.header["e_entry"]
        if entry_override is not None:
            entry = entry_override

        # Load segments for load_addr, RAM origin, BSS
        load_segments = collect_load_segments(elf)
        if not load_segments:
            raise SystemExit("ERROR: no PT_LOAD segments")

        load_addr = min((ph["p_paddr"] or ph["p_vaddr"])
                         for ph in load_segments)
        if load_addr_override is not None:
            load_addr = load_addr_override

        # -----------------------------------------------------------------
        # RAM origin + size (authoritative, from ELF)
        # -----------------------------------------------------------------
        ram_origin = min(ph["p_vaddr"] for ph in load_segments)

        stack_top = get_symbol_value(elf, "_stack_top")
        if stack_top is None:
            raise SystemExit(
                "ERROR: _stack_top symbol not found in ELF.\n"
                "Define stack in linker script, e.g.:\n"
                "  _stack_top = ORIGIN(RAM) + LENGTH(RAM);"
            )

        ram_size = stack_top - ram_origin
        if ram_size <= 0:
            raise SystemExit("ERROR: invalid RAM size computed")

        # BSS size = sum(memsz - filesz)
        bss_size = 0
        for ph in load_segments:
            memsz = ph["p_memsz"]
            filesz = ph["p_filesz"]
            if memsz > filesz:
                bss_size += (memsz - filesz)

        # ------------------------------
        # Extract CODE and DATA
        # ------------------------------
        CODE_SECTIONS = [".text", ".text.init"]
        DATA_SECTIONS = [
            ".rodata", ".srodata", ".data", ".sdata"
        ]

        code_bytes = extract_sections_by_name(elf, CODE_SECTIONS)
        data_bytes = extract_sections_by_name(elf, DATA_SECTIONS)

        if verbose:
            print("Extracted CODE sections:")
            for s in CODE_SECTIONS:
                if elf.get_section_by_name(s):
                    print("  ", s)

            print("Extracted DATA sections:")
            for s in DATA_SECTIONS:
                if elf.get_section_by_name(s):
                    print("  ", s)

            print("CODE size:", len(code_bytes))
            print("DATA size:", len(data_bytes))
            print("BSS size :", bss_size)
            print("RAM size :", ram_size)

    # ------------------------------
    # Build header
    # ------------------------------
    flags = 0
    if rv64:
        flags |= 1  # RV64 flag

    code_offset = R5M_HEADER_SIZE
    code_size   = len(code_bytes)

    if data_bytes:
        data_offset = code_offset + code_size
        data_size   = len(data_bytes)
    else:
        data_offset = 0
        data_size   = 0

    total_size = R5M_HEADER_SIZE + code_size + data_size
    reserved   = b"\x00" * 20

    header = struct.pack(
        R5M_HEADER_FMT,
        R5VM_MAGIC,
        R5VM_VERSION,
        flags,
        entry,
        load_addr,
        ram_size,
        code_offset,
        code_size,
        data_offset,
        data_size,
        bss_size,
        total_size,
        reserved,
    )

    if verbose:
        print("Writing:", path_out)
        print("R5M header:")
        print(f"  magic      = 0x{R5VM_MAGIC:08x}")
        print(f"  version    = {R5VM_VERSION}")
        print(f"  flags      = 0x{flags:04x} ({'RV64' if rv64 else 'RV32'})")
        print(f"  entry      = 0x{entry:08x}")
        print(f"  load_addr  = 0x{load_addr:08x}")
        print(f"  ram_size   = {ram_size} (0x{ram_size:x})")
        print(f"  code_off   = {code_offset} (0x{code_offset:x})")
        print(f"  code_size  = {code_size} (0x{code_size:x})")
        print(f"  data_off   = {data_offset} (0x{data_offset:x})")
        print(f"  data_size  = {data_size} (0x{data_size:x})")
        print(f"  bss_size   = {bss_size} (0x{bss_size:x})")
        print(f"  total_size = {total_size} (0x{total_size:x})")

    # ------------------------------
    # Write output file
    # ------------------------------
    with open(path_out, "wb") as out:
        out.write(header)
        out.write(code_bytes)
        if data_bytes:
            out.write(data_bytes)

    if verbose:
        print("Done.")


# ---------------------------------------------------------------------
# main
# ---------------------------------------------------------------------

def main():
    a = parse_args(sys.argv[1:])
    build_r5m(
        path_in=a.input,
        path_out=a.output,
        rv64=a.rv64,
        entry_override=parse_int_auto(a.entry),
        load_addr_override=parse_int_auto(a.load_addr),
        verbose=a.verbose,
    )


if __name__ == "__main__":
    main()

