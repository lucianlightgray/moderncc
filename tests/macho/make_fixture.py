#!/usr/bin/env python3
"""Emit a minimal foreign Mach-O relocatable (MH_OBJECT) for the loader tests.

mcc emits ELF relocatables on every target by design, so nothing in-tree can
produce the FOREIGN Mach-O object that mcc_object_type / the MACHO branch of the
libmcc.c loader switch must learn to consume. This writes one directly.

Layout: mach_header_64 + LC_SEGMENT_64 (one __TEXT,__text section) + LC_SYMTAB.
Two symbols so both halves of a reader are exercised: an external DEFINED symbol
in __text, and an external UNDEFINED one to be resolved by the linker.

Usage: make_fixture.py <out.o> [--arch x86_64|arm64]
"""
import struct, sys

MH_MAGIC_64, MH_OBJECT = 0xFEEDFACF, 0x1
CPU_X86_64, CPU_ARM64 = 0x01000007, 0x0100000C
LC_SEGMENT_64, LC_SYMTAB = 0x19, 0x2
N_EXT, N_SECT, N_UNDF = 0x01, 0x0E, 0x00
S_ATTR_PURE_INSTRUCTIONS, S_ATTR_SOME_INSTRUCTIONS = 0x80000000, 0x00000400

TEXT = {
    # mov eax, 42 ; ret
    "x86_64": bytes([0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3]),
    # mov w0, #42 ; ret
    "arm64": struct.pack("<II", 0x52800540, 0xD65F03C0),
}


def fixed(name, n=16):
    b = name.encode()
    if len(b) > n:
        raise ValueError("name too long: " + name)
    return b + b"\0" * (n - len(b))


def build(arch):
    text = TEXT[arch]
    cputype = CPU_X86_64 if arch == "x86_64" else CPU_ARM64
    cpusub = 3 if arch == "x86_64" else 0

    strtab = b"\0" + b"_mcc_fixture_defined\0" + b"_mcc_fixture_undefined\0"
    off_def = 1
    off_undef = 1 + len("_mcc_fixture_defined") + 1

    hdrsz, segsz, secsz, symsz = 32, 72, 80, 24
    sizeofcmds = segsz + secsz + symsz
    text_off = hdrsz + sizeofcmds
    symoff = text_off + len(text)
    nsyms = 2
    stroff = symoff + nsyms * 16

    hdr = struct.pack("<IiiIIII I", MH_MAGIC_64, cputype, cpusub, MH_OBJECT,
                      2, sizeofcmds, 0, 0)
    seg = struct.pack("<II16sQQQQiiII", LC_SEGMENT_64, segsz + secsz, fixed(""),
                      0, len(text), text_off, len(text), 7, 7, 1, 0)
    sec = struct.pack("<16s16sQQIIIIIIII", fixed("__text"), fixed("__TEXT"),
                      0, len(text), text_off, 4 if arch == "arm64" else 0,
                      0, 0,
                      S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS,
                      0, 0, 0)
    sym = struct.pack("<IIIIII", LC_SYMTAB, symsz, symoff, nsyms, stroff,
                      len(strtab))
    nlists = (struct.pack("<IBBHQ", off_def, N_EXT | N_SECT, 1, 0, 0)
              + struct.pack("<IBBHQ", off_undef, N_EXT | N_UNDF, 0, 0, 0))
    return hdr + seg + sec + sym + text + nlists + strtab


def main():
    if len(sys.argv) < 2:
        sys.stderr.write(__doc__)
        return 2
    arch = "x86_64"
    if "--arch" in sys.argv:
        arch = sys.argv[sys.argv.index("--arch") + 1]
    if arch not in TEXT:
        sys.stderr.write("unsupported arch %s\n" % arch)
        return 2
    with open(sys.argv[1], "wb") as f:
        f.write(build(arch))
    return 0


if __name__ == "__main__":
    sys.exit(main())
