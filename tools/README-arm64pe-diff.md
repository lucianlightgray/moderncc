# arm64-PE codegen validation harness

Validate **arm64-Windows (arm64-PE)** codegen **without arm64-Windows hardware**,
by byte-diffing the arm64-PE compiler's object emission against the
proven-correct **native-arm64 (ELF/Linux)** compiler's emission for the same
source.

This is the enabling infra for the "Tier-2" arm64-PE work items (over-align,
embed-jit blob, sanitizer codegen). It codifies the technique that already
caught a real arm64-PE bug: the stale-`.text` import-thunk corruption
(commit `80ea9843`), which was byte-identical on every linux-arm (ELF) cell and
divergent only on the two arm64-PE jobs.

## Files

| file | purpose |
|------|---------|
| `tools/arm64pe_diff.py` | the harness: compile → parse ELF → capstone disasm → classified byte-diff |
| `tools/build_arm64pe_cross.py` | build the arm64 cross pair (plain, or optimizer-enabled) |
| `tools/arm64pe_corpus/*.c` | built-in sample programs (leaf, extern/import, over-align, promote-switch, float rodata, long-width) |

## Why this works

* **mcc always emits ELF *relocatable objects*** — even for the PE target. Only
  the final linked exe is PE. So `mcc-arm64-win32 -c -o x.o x.c` and
  `mcc-arm64 -c -o x.o x.c` both yield ELF64 / `EM_AARCH64` (183) objects, parsed
  identically and disassembled with capstone (`CS_ARCH_ARM64` / `CS_MODE_ARM`).
* **The oracle:** the arm64 **ELF** codegen path runs on real silicon in the
  native `ubuntu-24.04-arm` CI cell, so it is the reference. Any difference in
  the PE object is either a benign, expected symbol-addressing / PE-structure
  difference, or a suspicious codegen divergence worth investigating.

> The script is deliberately **not** named `dis.py` — that shadows the stdlib
> `dis` module and breaks capstone's internal `import inspect`.

## Prerequisites

* Python 3 + `capstone` (`pip install capstone`; verified with 5.0.7).
* The arm64 cross pair `mcc-arm64.exe` (ELF) and `mcc-arm64-win32.exe` (PE).

Build the pair (Windows host, CLion mingw toolchain — `build_arm64pe_cross.py`
prepends the CLion `mingw`/`cmake`/`ninja` to PATH automatically):

```powershell
python tools/build_arm64pe_cross.py           # -> cmake-cross/
```

or by hand:

```powershell
$clion = "C:\Users\llg\scoop\persist\jetbrains-toolbox\apps\CLion\bin"
$env:Path = "$clion\mingw\bin;$clion\cmake\win\x64\bin;$clion\ninja\win\x64;" + $env:Path
cmake --preset cross
& "$clion\ninja\win\x64\ninja.exe" -C cmake-cross mcc-arm64.exe mcc-arm64-win32.exe
```

The harness auto-locates the pair in `cmake-cross/`, `build-cross/`, or
`cmake-cross-opt/` (or `$MCC_CROSS_DIR`, or `--mcc-elf`/`--mcc-pe`).

## Usage

```
python tools/arm64pe_diff.py FILE.c [FILE2.c ...]   # diff given sources
python tools/arm64pe_diff.py --corpus                # run the built-in corpus
python tools/arm64pe_diff.py --corpus --verbose      # + side-by-side disasm on any divergence
```

Options: `--mcc-elf PATH`, `--mcc-pe PATH`, `--cflags=-O1` (note the `=`, so
argparse doesn't mistake `-O1` for a flag), `--keep` (leave `.elf.o`/`.pe.o` for
manual `objdump`/`readelf`).

**Exit code** `0` if no *suspicious* divergence, `1` otherwise. Benign
divergences never fail the run.

### Reading the report

Each source prints `clean` or `SUSPICIOUS`, then lines marked:

* `.` — informational (section identical / present only in one target)
* `~ BENIGN` — an expected ELF-vs-PE difference (see classification below)
* `! SUSPICIOUS` — a divergence the harness cannot explain away → investigate.

### Classification rules

**Benign (expected, never fails):**

* **GOT-vs-direct symbol addressing.** ELF-Linux is PIC and reaches externs
  GOT-indirect (`ADR_GOT_PAGE` + `LD64_GOT_LO12_NC`); arm64-PE resolves the same
  symbol direct/image-relative (`ADR_PREL_PG_HI21` + `ADD_ABS_LO12_NC`). Same
  target symbol, different fixup class → benign. `.text` bytes that differ only
  *within a reloc's patched immediate field* are likewise benign.
* **PE-only unwind sections** `.pdata` / `.xdata` (SEH / `RtlAddFunctionTable`)
  vs **ELF-only** `.eh_frame` (DWARF CFI).
* **Read-only-data section rename:** ELF `.data.ro` ≙ PE `.rdata` (normalized so
  content is compared, not the name).
* **Local-label numbering** (`L.<n>`) differences (allocation-order artifact).

**Suspicious (fails the run):**

* Any `.text`/data byte difference **not** at a reloc immediate field.
* `.text` **size** differs.
* A reloc whose **target symbol differs**, or whose class differs **and is not**
  a known GOT-vs-direct swap, or whose addend differs for the same symbol.
* A reloc present in only one object.

## Built-in corpus

| file | exercises |
|------|-----------|
| `01_leaf_arith.c` | leaf integer codegen, width-stable types → `.text` must be **byte-identical** |
| `02_extern_import.c` | extern function + extern globals → the **import-thunk / GOT-vs-direct** path (the `80ea9843` class) |
| `03_overalign.c` | over-aligned stack + static objects (Tier-2 over-align work) |
| `04_promote_switch.c` | case ladder that shrinks under register promotion (the `80ea9843` shrink shape; needs the optimizer build to actually promote) |
| `05_float_rodata.c` | fp constants + rodata literal pool addressing |
| `06_long_width.c` | **expected-divergence** demo: `long` is 8 B (LP64/ELF) vs 4 B (LLP64/PE); the harness correctly flags this SUSPICIOUS |

Demonstration run (`cmake-cross` pair, default `-O0`):

```
=== 01_leaf_arith.c   -> clean       (.text 240 bytes IDENTICAL)
=== 02_extern_import.c -> clean       (.text differs only in reloc immediates; all g_counter/g_name/L.* relocs = benign GOT-vs-direct)
=== 03_overalign.c    -> clean       (4 reloc-immediate bytes to g_over; benign)
=== 04_promote_switch.c -> clean     (.text 532 bytes IDENTICAL)
=== 05_float_rodata.c -> clean       (.rodata 48 bytes IDENTICAL; literal-pool relocs benign)
=== 06_long_width.c   -> SUSPICIOUS  (.text size 192 vs 188; x-regs vs w-regs — the LP64/LLP64 long split)
```

`06_long_width.c` is the **intended** SUSPICIOUS result: it proves the harness
surfaces real type-width-driven codegen differences instead of silently passing
them. A human confirms it is the `long` ABI split, not a bug.

## Optimizer caveat — building an optimizer-enabled cross pair

**The cross compilers are built WITHOUT `MCC_CONFIG_OPTIMIZER`.** In
`CMakeLists.txt` the cross targets get only `${_cross_defs}` (the
`set(_cross_defs ${_cdefs} ${_cpath_defs} ...)` line in the `MCC_ENABLE_CROSS`
block), which omits `MCC_CONFIG_OPTIMIZER`. Consequently **`-O1` / AST-promotion
passes compile out to SILENT NO-OPS in the default cross build** — the
`04_promote_switch.c` shrink path is inert, and optimizer-only codegen bugs
cannot be reproduced with the plain pair.

To validate optimizer-dependent codegen, build an **optimizer-enabled** pair:

```powershell
python tools/build_arm64pe_cross.py --optimizer    # -> cmake-cross-opt/
```

This temporarily appends `MCC_CONFIG_OPTIMIZER=1` to `_cross_defs`, configures +
builds into `cmake-cross-opt/`, then **reverts `CMakeLists.txt` byte-identically**
(guaranteed via `try/finally`, even on failure — it never leaves the tree
modified). Then run the harness against that pair, at the optimization level you
want to exercise:

```
python tools/arm64pe_diff.py --corpus --cflags=-O1 \
    --mcc-elf cmake-cross-opt/mcc-arm64.exe \
    --mcc-pe  cmake-cross-opt/mcc-arm64-win32.exe
```

(or `set MCC_CROSS_DIR=cmake-cross-opt` and drop the `--mcc-*` overrides.)

Manual equivalent, if you prefer: edit the `set(_cross_defs ...)` line to add
`"MCC_CONFIG_OPTIMIZER=1"`, `cmake --preset cross -B cmake-cross-opt`, build the
two targets, then revert the edit.

## What this harness CAN and CANNOT catch

**CAN catch (static codegen / layout / relocation):**

* Wrong instruction selection or encoding in `.text` (the primary signal — any
  non-reloc byte divergence).
* `.text` **size / layout** drift, including the stale-tail class from
  `80ea9843` (a shorter promoted re-emit leaving live bytes) — validated by an
  injected-corruption negative-control test.
* Missing, extra, or mis-typed **relocations**; wrong reloc **target symbol** or
  **addend**; reloc classes that are not a known-benign GOT-vs-direct swap.
* Divergent **rodata / data** emission (literal pools, initializers, ctor lists).
* Type-width / ABI-driven codegen differences (surfaced as SUSPICIOUS for human
  classification — e.g. the LP64/LLP64 `long` split).

**CANNOT catch (native-only fault semantics — the object bytes look fine):**

* **SEH / `RtlAddFunctionTable` unwind correctness.** The harness sees
  `.pdata`/`.xdata` *exist* but does not validate that the PE loader's unwind
  actually walks the frames at runtime.
* **Instruction-cache / self-modifying-code coherence** (JIT/embed-blob patched
  code needs `IC IVAU`/`DSB`/`ISB` on real silicon; identical bytes say nothing
  about whether the flush is present/correct at run time).
* **Weak-memory ordering bugs.** arm64 is weakly ordered; a missing barrier can
  be byte-invisible and is masked by wine/qemu on x86 (which give strong x86-TSO
  ordering).
* **Wild-jump / bad-address fault behavior.** A wild branch *spins* under wine
  but *faults* on native arm64; byte-diff won't see the control-flow bug unless
  it also perturbs emitted bytes.
* Anything decided by the **final PE linker** (import thunk *placement*, IAT
  layout, section merging) — the harness diffs *object* emission; link-stage
  bugs like `80ea9843` are caught only because the corruption already shows in
  the object `.text`. Validate the linked exe separately.

### Optional runtime cross-check (necessary, not sufficient)

Running the linked arm64-PE exe under **wine-arm64** in a
`--platform linux/arm64` Docker container is a useful smoke test, but wine/qemu
on an x86 host gives strong x86-TSO ordering and **masks weak-memory bugs and
wild-jump faults**. Treat it as *necessary-not-sufficient*: the classified
byte-diff here is the primary signal.
