#!/usr/bin/env python3
"""arm64-PE codegen validation harness (byte-diff vs proven native-arm64 ELF).

Validates arm64-Windows (arm64-PE) codegen WITHOUT arm64-Windows hardware by
byte-diffing the arm64-PE mcc's emission against the proven-correct native-arm64
(ELF/Linux) mcc's emission for the same source.

Why this works
--------------
* mcc always emits ELF *relocatable objects* even for the PE target (only the
  final linked exe is PE). So `mcc-arm64-win32 -c -o x.o x.c` and
  `mcc-arm64 -c -o x.o x.c` both produce ELF64/EM_AARCH64 objects that can be
  parsed identically and disassembled with capstone (CS_ARCH_ARM64/CS_MODE_ARM).
* The arm64 ELF codegen path is exercised on real silicon by the native
  ubuntu-24.04-arm CI cell, so it is the oracle: any difference in the PE
  object's .text is either (a) a benign, expected symbol-addressing / PE-struct
  difference, or (b) a suspicious codegen divergence worth investigating.

This is the technique that already caught the stale-.text import-thunk
corruption (commit 80ea9843): identical ELF codegen on every linux-arm cell,
divergent only on the two arm64-PE jobs.

Deliberately NOT named dis.py: that shadows the stdlib `dis` module and breaks
capstone's internal `import inspect`.

Usage
-----
    python tools/arm64pe_diff.py FILE.c [FILE2.c ...]   # diff given sources
    python tools/arm64pe_diff.py --corpus               # run built-in corpus
    python tools/arm64pe_diff.py --corpus --verbose      # + full disasm on diff

Options
    --mcc-elf PATH    override arm64 ELF cross compiler
    --mcc-pe  PATH    override arm64-PE cross compiler
    --cflags "..."    extra flags passed to both compilers (e.g. "-O1")
    --keep            keep the temp objects for manual inspection
    --verbose         print full side-by-side disasm when a section diverges

Exit code: 0 if no SUSPICIOUS divergence in any input, 1 otherwise.
Benign divergences never fail the run (they are expected ELF-vs-PE differences).
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile

try:
    import capstone
except ImportError:
    sys.stderr.write("error: capstone not installed. `pip install capstone`\n")
    sys.exit(2)

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)

# ---------------------------------------------------------------------------
# Locating the cross compilers
# ---------------------------------------------------------------------------
# The cross build lands the arm64 pair here (cross preset -> ${sourceDir}/cmake-cross).
# build-cross/ is the documented prebuilt location; check both.
_CANDIDATE_DIRS = [
    os.environ.get("MCC_CROSS_DIR", ""),
    os.path.join(REPO, "cmake-cross"),
    os.path.join(REPO, "build-cross"),
    os.path.join(REPO, "cmake-cross-opt"),
]


def _exe(name):
    return name + (".exe" if os.name == "nt" else "")


def find_compiler(basename, override):
    if override:
        cand = override if os.path.exists(override) else override + (
            ".exe" if os.name == "nt" and not override.endswith(".exe") else "")
        if not os.path.exists(cand):
            sys.exit(f"error: --mcc override not found: {override}")
        return os.path.abspath(cand)
    for d in _CANDIDATE_DIRS:
        if not d:
            continue
        p = os.path.join(d, _exe(basename))
        if os.path.exists(p):
            return os.path.abspath(p)
    sys.exit(
        f"error: could not find {basename}. Build the arm64 cross pair first:\n"
        f"  cmake --preset cross && ninja -C cmake-cross mcc-arm64.exe mcc-arm64-win32.exe\n"
        f"or pass --mcc-elf / --mcc-pe explicitly.\n"
        f"(searched: {[d for d in _CANDIDATE_DIRS if d]})"
    )


# ---------------------------------------------------------------------------
# Minimal ELF64 reader (little-endian, the only layout mcc emits)
# ---------------------------------------------------------------------------
class Elf:
    def __init__(self, path):
        self.path = path
        b = open(path, "rb").read()
        self.b = b
        if b[:4] != b"\x7fELF" or b[4] != 2 or b[5] != 1:
            raise ValueError(f"{path}: not a little-endian ELF64 object")
        self.e_machine = struct.unpack_from("<H", b, 0x12)[0]
        e_shoff = struct.unpack_from("<Q", b, 0x28)[0]
        e_shentsize = struct.unpack_from("<H", b, 0x3A)[0]
        e_shnum = struct.unpack_from("<H", b, 0x3C)[0]
        e_shstrndx = struct.unpack_from("<H", b, 0x3E)[0]
        keys = "name typ flags addr off size link info align entsz".split()
        self.sh = []
        for i in range(e_shnum):
            vals = struct.unpack_from("<IIQQQQIIQQ", b, e_shoff + i * e_shentsize)
            self.sh.append(dict(zip(keys, vals)))
        stroff = self.sh[e_shstrndx]["off"]
        for s in self.sh:
            s["nm"] = self._cstr(stroff + s["name"])
        # symbol table (for reloc target names)
        self.syms = []
        symtab = self._by_name(".symtab")
        if symtab:
            strt = self.sh[symtab["link"]]["off"]
            for o in range(symtab["off"], symtab["off"] + symtab["size"], 24):
                nm, info, other, shndx, val, sz = struct.unpack_from("<IBBHQQ", b, o)
                self.syms.append(self._cstr(strt + nm))

    def _cstr(self, off):
        e = self.b.index(b"\x00", off)
        return self.b[off:e].decode("utf-8", "replace")

    def _by_name(self, nm):
        for s in self.sh:
            if s["nm"] == nm:
                return s
        # fall back to a normalized (aliased) match: e.g. asking for ".rodata"
        # resolves this object's ".data.ro" (ELF) or ".rdata" (PE).
        target = _norm_sec(nm)
        for s in self.sh:
            if _norm_sec(s["nm"]) == target:
                return s
        return None

    def section_bytes(self, nm):
        s = self._by_name(nm)
        if not s:
            return b""
        if s["typ"] == 8:  # SHT_NOBITS (.bss) -- no file bytes
            return b"\x00" * s["size"]
        return self.b[s["off"]: s["off"] + s["size"]]

    def relocs(self, sec_nm):
        """Return list of (offset, type_num, sym_name, addend) for section sec_nm.
        sec_nm may be a normalized/aliased name (e.g. .rodata)."""
        out = []
        sec = self._by_name(sec_nm)
        real = sec["nm"] if sec else sec_nm
        rela = self._by_name(".rela" + real)
        if rela:
            for o in range(rela["off"], rela["off"] + rela["size"], 24):
                off, inf, add = struct.unpack_from("<QQq", self.b, o)
                typ = inf & 0xFFFFFFFF
                sym = inf >> 32
                out.append((off, typ, self.syms[sym] if sym < len(self.syms) else str(sym), add))
        return out

    def code_section_names(self):
        # SHT_PROGBITS with SHF_EXECINSTR(0x4)
        return [s["nm"] for s in self.sh if s["typ"] == 1 and (s["flags"] & 0x4)]

    def all_section_names(self):
        return [s["nm"] for s in self.sh if s["nm"]]


# ---------------------------------------------------------------------------
# arm64 relocation classification
# ---------------------------------------------------------------------------
# Canonical R_AARCH64_* numbers (src/formats/elf.h).
R_AARCH64 = {
    0: "NONE", 257: "ABS64", 258: "ABS32", 259: "ABS16",
    260: "PREL64", 261: "PREL32", 262: "PREL16",
    273: "LD_PREL_LO19", 274: "ADR_PREL_LO21",
    275: "ADR_PREL_PG_HI21", 276: "ADR_PREL_PG_HI21_NC", 277: "ADD_ABS_LO12_NC",
    278: "LDST8_ABS_LO12_NC", 279: "TSTBR14", 280: "CONDBR19",
    282: "JUMP26", 283: "CALL26",
    284: "LDST16_ABS_LO12_NC", 285: "LDST32_ABS_LO12_NC",
    286: "LDST64_ABS_LO12_NC", 299: "LDST128_ABS_LO12_NC",
    311: "ADR_GOT_PAGE", 312: "LD64_GOT_LO12_NC", 313: "LD64_GOTPAGE_LO15",
}


def reloc_name(t):
    return "R_AARCH64_" + R_AARCH64.get(t, f"0x{t:x}")


# A pair (ELF-side type, PE-side type) that is a known-benign encoding of the
# *same* symbol reference: ELF-Linux uses GOT-indirect page addressing for
# extern data (PIC), arm64-PE resolves it direct (image-relative page+lo12).
# Both name the same target symbol; only the fixup class differs. This is the
# "GOT-vs-direct" benign class from the prior analysis.
_BENIGN_RELOC_SWAPS = {
    frozenset({"ADR_GOT_PAGE", "ADR_PREL_PG_HI21"}),
    frozenset({"LD64_GOT_LO12_NC", "LDST64_ABS_LO12_NC"}),
    frozenset({"LD64_GOT_LO12_NC", "ADD_ABS_LO12_NC"}),
    frozenset({"ADR_GOT_PAGE", "ADR_PREL_PG_HI21_NC"}),
}

# Sections whose presence/absence is a pure PE-vs-ELF structural artifact,
# never a codegen bug in itself:
#   .eh_frame  -> ELF DWARF CFI          .pdata/.xdata -> PE SEH/unwind
#   *reloc lists on those follow suit.  TLS/import structures likewise.
_PE_ONLY_SECTIONS = {".pdata", ".xdata"}
_ELF_ONLY_SECTIONS = {".eh_frame", ".eh_frame_hdr", ".note.GNU-stack"}

# Read-only data carries the same content under a target-specific *name*: mcc
# names it .data.ro for ELF and .rdata for PE. Normalize so the data comparison
# lines the pair up instead of diffing each against an absent same-name section.
# (Also .text.<fn> COMDAT-style splits fold to .text, if any appear.)
_SEC_ALIAS = {".data.ro": ".rodata", ".rdata": ".rodata"}


def _norm_sec(nm):
    return _SEC_ALIAS.get(nm, nm)


def _local_label(sym):
    # mcc numbers anonymous local labels L.<n> (string literals, jump targets).
    # The <n> is an allocation-order artifact and differs benignly ELF-vs-PE.
    return sym.startswith("L.") or sym.startswith(".L")


def _strip_local_num(sym):
    return "L.*" if _local_label(sym) else sym


# ---------------------------------------------------------------------------
# Disassembly
# ---------------------------------------------------------------------------
_MD = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)


def disasm(code):
    out = []
    for ins in _MD.disasm(code, 0):
        out.append((ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))
    consumed = out[-1][0] + 4 if out else 0
    if consumed < len(code):
        # trailing bytes capstone could not decode (padding/data literal)
        out.append((consumed, code[consumed:].hex(), ".byte", "<undecoded tail>"))
    return out


# ---------------------------------------------------------------------------
# Compile
# ---------------------------------------------------------------------------
def compile_obj(mcc, src, obj, cflags):
    cmd = [mcc, "-c", "-o", obj, src] + cflags
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(obj):
        raise RuntimeError(
            f"compile failed ({os.path.basename(mcc)}): {' '.join(cmd)}\n{r.stdout}{r.stderr}"
        )
    return obj


# ---------------------------------------------------------------------------
# Diff one source
# ---------------------------------------------------------------------------
class Report:
    def __init__(self, src):
        self.src = src
        self.suspicious = []   # list[str]
        self.benign = []       # list[str]
        self.info = []         # list[str]

    def sus(self, m):
        self.suspicious.append(m)

    def ben(self, m):
        self.benign.append(m)

    def note(self, m):
        self.info.append(m)


def diff_relocs(sec, elf, pe, rep):
    er = elf.relocs(sec)
    pr = pe.relocs(sec)
    # index by code offset
    em = {off: (t, s, a) for (off, t, s, a) in er}
    pm = {off: (t, s, a) for (off, t, s, a) in pr}
    for off in sorted(set(em) | set(pm)):
        e = em.get(off)
        p = pm.get(off)
        if e and p:
            et, es, ea = e
            pt, ps, pa = p
            en, pn = reloc_name(et), reloc_name(pt)
            same_sym = _strip_local_num(es) == _strip_local_num(ps)
            if et == pt and same_sym and ea == pa:
                continue  # identical
            swap = frozenset({R_AARCH64.get(et, ""), R_AARCH64.get(pt, "")})
            if same_sym and swap in _BENIGN_RELOC_SWAPS:
                rep.ben(f"reloc @{sec}+0x{off:x}: {es} "
                        f"ELF={R_AARCH64.get(et,en)} vs PE={R_AARCH64.get(pt,pn)} "
                        f"(GOT-indirect vs direct -- expected)")
            elif same_sym and ea != pa:
                rep.sus(f"reloc @{sec}+0x{off:x}: {es} same class {en} "
                        f"but addend ELF={ea} PE={pa}")
            elif not same_sym:
                rep.sus(f"reloc @{sec}+0x{off:x}: DIFFERENT target "
                        f"ELF={es}({en}) vs PE={ps}({pn})")
            else:
                rep.sus(f"reloc @{sec}+0x{off:x}: {es} class differs "
                        f"ELF={en} vs PE={pn} (not a known benign swap)")
        elif e and not p:
            et, es, ea = e
            rep.sus(f"reloc @{sec}+0x{off:x}: only in ELF: {es} {reloc_name(et)}")
        else:
            pt, ps, pa = p
            rep.sus(f"reloc @{sec}+0x{off:x}: only in PE: {ps} {reloc_name(pt)}")


def diff_code_section(sec, elf, pe, rep, verbose):
    ec = elf.section_bytes(sec)
    pc = pe.section_bytes(sec)
    if ec == pc:
        rep.note(f"{sec}: {len(ec)} bytes IDENTICAL")
    else:
        # The .text may differ ONLY in the reloc'd immediate fields; the reloc
        # diff pass classifies those. Here we report the raw byte divergence and,
        # if it is not explainable by reloc sites, escalate.
        n = min(len(ec), len(pc))
        difs = [i for i in range(n) if ec[i] != pc[i]]
        reloc_offs = {o for (o, _, _, _) in elf.relocs(sec)} | {o for (o, _, _, _) in pe.relocs(sec)}
        # bytes within 4 of a reloc site are the patched immediate -> benign
        unexplained = [i for i in difs if not any(o <= i < o + 4 for o in reloc_offs)]
        if len(ec) != len(pc):
            rep.sus(f"{sec}: SIZE differs ELF={len(ec)} PE={len(pc)} bytes")
        if unexplained:
            rep.sus(f"{sec}: {len(unexplained)} byte diff(s) NOT at a reloc "
                    f"site (offsets {[hex(x) for x in unexplained[:8]]}"
                    f"{'...' if len(unexplained) > 8 else ''})")
        else:
            rep.ben(f"{sec}: {len(difs)} differing byte(s), all within reloc "
                    f"immediate fields (symbol addressing) -- expected")
        if verbose:
            _print_side_by_side(sec, ec, pc)
    # relocation classification (independent of byte identity)
    diff_relocs(sec, elf, pe, rep)


def diff_data_section(sec, elf, pe, rep):
    ec = elf.section_bytes(sec)
    pc = pe.section_bytes(sec)
    if ec == pc:
        if ec:
            rep.note(f"{sec}: {len(ec)} bytes IDENTICAL")
        return
    if len(ec) != len(pc):
        rep.sus(f"{sec}: data SIZE differs ELF={len(ec)} PE={len(pc)}")
        return
    reloc_offs = {o for (o, _, _, _) in elf.relocs(sec)} | {o for (o, _, _, _) in pe.relocs(sec)}
    difs = [i for i in range(len(ec)) if ec[i] != pc[i]]
    unexplained = [i for i in difs if not any(o <= i < o + 8 for o in reloc_offs)]
    if unexplained:
        rep.sus(f"{sec}: {len(unexplained)} data byte diff(s) not at a reloc "
                f"(offsets {[hex(x) for x in unexplained[:8]]})")
    else:
        rep.ben(f"{sec}: differs only within relocated pointer slots -- expected")
    diff_relocs(sec, elf, pe, rep)


def _print_side_by_side(sec, ec, pc):
    de = disasm(ec)
    dp = disasm(pc)
    print(f"    --- side-by-side disasm of {sec} (ELF | PE) ---")
    for i in range(max(len(de), len(dp))):
        le = de[i] if i < len(de) else (0, "", "", "")
        lp = dp[i] if i < len(dp) else (0, "", "", "")
        mark = " " if (le[1:] == lp[1:]) else "*"
        ls = f"{le[0]:04x} {le[2]:6s} {le[3]}"[:38]
        rs = f"{lp[0]:04x} {lp[2]:6s} {lp[3]}"[:38]
        print(f"    {mark} {ls:<38} | {rs}")


def diff_source(src, mcc_elf, mcc_pe, cflags, keep, verbose):
    rep = Report(src)
    with tempfile.TemporaryDirectory() as td:
        eobj = os.path.join(td, "elf.o")
        pobj = os.path.join(td, "pe.o")
        compile_obj(mcc_elf, src, eobj, cflags)
        compile_obj(mcc_pe, src, pobj, cflags)
        if keep:
            base = os.path.splitext(os.path.basename(src))[0]
            import shutil
            shutil.copy(eobj, os.path.join(os.getcwd(), base + ".elf.o"))
            shutil.copy(pobj, os.path.join(os.getcwd(), base + ".pe.o"))
            rep.note(f"kept {base}.elf.o / {base}.pe.o")
        elf = Elf(eobj)
        pe = Elf(pobj)
        if elf.e_machine != 183 or pe.e_machine != 183:
            rep.sus(f"unexpected e_machine ELF={elf.e_machine} PE={pe.e_machine} "
                    f"(expected 183/EM_AARCH64)")
        # code sections
        code = sorted(set(elf.code_section_names()) | set(pe.code_section_names()))
        for sec in code:
            diff_code_section(sec, elf, pe, rep, verbose)
        # notable data sections that carry codegen (rodata literals, ctors).
        # Use normalized names so .data.ro (ELF) and .rdata (PE) line up as one.
        for sec in (".rodata", ".data", ".data.rel.ro"):
            if elf.section_bytes(sec) or pe.section_bytes(sec):
                diff_data_section(sec, elf, pe, rep)
        # structural section presence (normalized so target-renamed sections
        # such as .data.ro/.rdata are not falsely reported as one-sided)
        es = {_norm_sec(n) for n in elf.all_section_names()}
        ps = {_norm_sec(n) for n in pe.all_section_names()}
        for sec in sorted((es | ps)):
            in_e, in_p = sec in es, sec in ps
            if in_e and in_p:
                continue
            base = sec[5:] if sec.startswith(".rela") else sec
            if base in _PE_ONLY_SECTIONS and in_p:
                rep.ben(f"section {sec}: PE-only (SEH/unwind) -- expected")
            elif base in _ELF_ONLY_SECTIONS and in_e:
                rep.ben(f"section {sec}: ELF-only (DWARF CFI) -- expected")
            elif sec.startswith(".rela"):
                # reloc section for a section we already classified; skip noise
                continue
            else:
                where = "ELF" if in_e else "PE"
                rep.note(f"section {sec}: present only in {where}")
    return rep


# ---------------------------------------------------------------------------
# Corpus
# ---------------------------------------------------------------------------
def corpus_dir():
    return os.path.join(HERE, "arm64pe_corpus")


def corpus_files():
    d = corpus_dir()
    if not os.path.isdir(d):
        return []
    return sorted(os.path.join(d, f) for f in os.listdir(d) if f.endswith(".c"))


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sources", nargs="*", help="C source files to diff")
    ap.add_argument("--corpus", action="store_true", help="run the built-in corpus")
    ap.add_argument("--mcc-elf", default="", help="path to arm64 ELF cross compiler")
    ap.add_argument("--mcc-pe", default="", help="path to arm64-PE cross compiler")
    ap.add_argument("--cflags", default="", help="extra flags for both compilers")
    ap.add_argument("--keep", action="store_true", help="keep the .o objects")
    ap.add_argument("--verbose", action="store_true", help="full disasm on divergence")
    args = ap.parse_args(argv)

    sources = list(args.sources)
    if args.corpus:
        sources += corpus_files()
    if not sources:
        ap.error("no sources given; pass FILE.c or --corpus")

    mcc_elf = find_compiler("mcc-arm64", args.mcc_elf)
    mcc_pe = find_compiler("mcc-arm64-win32", args.mcc_pe)
    cflags = args.cflags.split() if args.cflags else []

    print(f"arm64-PE codegen validation harness")
    print(f"  ELF oracle : {mcc_elf}")
    print(f"  PE target  : {mcc_pe}")
    if cflags:
        print(f"  cflags     : {' '.join(cflags)}")
    print()

    total_sus = 0
    for src in sources:
        try:
            rep = diff_source(src, mcc_elf, mcc_pe, cflags, args.keep, args.verbose)
        except (RuntimeError, ValueError) as e:
            print(f"[ERROR] {src}: {e}\n")
            total_sus += 1
            continue
        verdict = "SUSPICIOUS" if rep.suspicious else "clean"
        print(f"=== {os.path.relpath(src, REPO)}  ->  {verdict}")
        for m in rep.info:
            print(f"    . {m}")
        for m in rep.benign:
            print(f"    ~ BENIGN     {m}")
        for m in rep.suspicious:
            print(f"    ! SUSPICIOUS {m}")
        print()
        total_sus += len(rep.suspicious)

    print("-" * 60)
    if total_sus:
        print(f"RESULT: {total_sus} suspicious divergence(s) -- investigate.")
        return 1
    print("RESULT: no suspicious divergences. arm64-PE codegen matches the "
          "native-arm64 ELF oracle (benign symbol-addressing / PE-struct "
          "differences only).")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
