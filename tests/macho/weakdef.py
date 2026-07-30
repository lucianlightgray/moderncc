#!/usr/bin/env python3
"""Assert a symbol's nlist encoding in a linked Mach-O image.

The export trie and the symtab are written by two independent code paths in
mccmacho.c, and only the trie half was ever checked. A weak definition that
reaches the trie with EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION but whose nlist has
no N_EXT reads as `non-external` to nm, dsymutil and every debugger, and
cannot coalesce through the symtab -- the image still runs, so nothing else
in the suite notices.

Usage: weakdef.py <mach-o image> <symbol> weakdef|global|undef
"""
import struct, sys

MH_MAGIC_64 = 0xFEEDFACF
LC_SYMTAB = 0x2
N_EXT, N_TYPE, N_UNDF, N_SECT = 0x01, 0x0E, 0x00, 0x0E
N_WEAK_REF, N_WEAK_DEF = 0x0040, 0x0080


def symtab(blob):
    magic, _, _, _, _, ncmds, _, _, _ = struct.unpack_from("<IiiIIIIII", blob, 0)
    if magic != MH_MAGIC_64:
        sys.exit(f"not a 64-bit little-endian Mach-O (magic {magic:#x})")
    off = 32
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", blob, off)
        if cmd == LC_SYMTAB:
            symoff, nsyms, stroff, strsize = struct.unpack_from("<IIII", blob, off + 8)
            return symoff, nsyms, stroff, strsize
        off += cmdsize
    sys.exit("no LC_SYMTAB")


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: weakdef.py <mach-o image> <symbol> weakdef|global|undef")
    path, want_name, want_kind = sys.argv[1:]
    blob = open(path, "rb").read()
    symoff, nsyms, stroff, _ = symtab(blob)
    for i in range(nsyms):
        strx, n_type, n_sect, n_desc, n_value = struct.unpack_from(
            "<IBBHQ", blob, symoff + i * 16)
        end = blob.index(b"\0", stroff + strx)
        name = blob[stroff + strx:end].decode("utf-8", "replace")
        if name != want_name:
            continue
        ext = bool(n_type & N_EXT)
        defined = (n_type & N_TYPE) == N_SECT
        wdef, wref = bool(n_desc & N_WEAK_DEF), bool(n_desc & N_WEAK_REF)
        got = (f"n_type={n_type:#04x} n_sect={n_sect} n_desc={n_desc:#06x} "
               f"n_value={n_value:#x}")
        if want_kind == "weakdef":
            if not ext:
                sys.exit(f"FAIL: {name} is a weak definition but not N_EXT ({got})")
            if not defined or not wdef:
                sys.exit(f"FAIL: {name} is not N_SECT|N_WEAK_DEF ({got})")
            if wref:
                sys.exit(f"FAIL: {name} carries N_WEAK_REF on a definition, which "
                         f"tells the linker it may hide an explicitly weak "
                         f"symbol ({got})")
        elif want_kind == "global":
            if not ext or not defined or wdef or wref:
                sys.exit(f"FAIL: {name} is not a plain external definition ({got})")
        elif want_kind == "undef":
            if not ext or (n_type & N_TYPE) != N_UNDF:
                sys.exit(f"FAIL: {name} is not an external undefined symbol ({got})")
        else:
            sys.exit(f"unknown kind {want_kind}")
        print(f"ok: {name} {want_kind} {got}")
        return 0
    sys.exit(f"FAIL: no symbol {want_name} in {path}")


if __name__ == "__main__":
    sys.exit(main())
