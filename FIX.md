# FIX.md — CI bench n/a: fixed bugs + open arm64 investigation

State dump of the 2026-07-10 debugging session that root-caused the all-n/a
`mcc` rows in the CI benchmark reports. Three bug classes are fixed and
committed (8191e9ea); one arm64 class is reproduced and heavily narrowed but
not yet root-caused. This file is the pickup point.

## Fixed (commit 8191e9ea)

1. **win64 alloca shadow space** (`runtime/lib/alloca.c`): the x86_64 `_WIN32`
   alloca returned the new rsp directly, but the next call's callee homes its
   register args in the 32 bytes at the caller's rsp, overwriting the block.
   `mcc_new()` hits this via `host_w32_mccdir(alloca(MAX_PATH))`
   (mcchost.h:181), so *every* mcc-built mcc.exe crashed at startup on
   Windows x86_64 → all stage2 bench rows n/a on mingw/sanitize-msvc-x86_64.
   Fix: allocate 32 extra bytes, return rsp+32. VLAs on PE route through the
   same runtime alloca (`gen_vla_alloc` forces `use_call` on PE), so both are
   covered. Verified under wine with a minimized repro
   (`use(fill(alloca(260)))` + `GetModuleFileNameA`).

2. **Tier-3 promote spills below the frame** (`src/mccast.c`): promote saved
   pinned callee-saved regs with `push` after the prologue. On PE the
   outgoing-call area (win64 home space + stack args, `gfunc_call` writes it
   rsp-relative into prologue-reserved `func_scratch`) sits at the frame
   bottom, so callees scribbled the pushed save slots → the caller's pinned
   register was restored corrupt. Symptoms: -O2 stage2 hangs on anything
   (miscompiled `cstr_cat` — its caller's pinned loop var trashed), -O3
   stage2 miscompiles the PP expression evaluator. Fix: save pins into
   rbp-relative frame slots allocated from `loc` at replay start (safe:
   `saved_loc` is the post-first-pass frame floor, disjoint from all recorded
   local offsets). PE -O2 self-host now reaches byte-identical stage3==stage4
   under wine; full matrix (default/-O1/-O2/-O3 × corpus/full_language/mcc.c)
   green on PE.
   Also added `MCC_AST_PROMOTE_LIMIT` / `MCC_AST_OPT_LIMIT` env gates
   (bisection tools, same pattern as `MCC_AST_INLINE_LIMIT`).

3. **Reference-compiler rows** :
   - `swab32` in tests/diff/parts/legacy_meta.h used `%h0` with constraint
     `"q"`; on win64 gcc -O1+ allocates r8-r15 for `"q"` → "extended
     registers have no high halves". Fixed to `"Q"`, and taught mcc's
     i386/x86_64 asm the `Q` constraint (same a/b/c/d pool as its `q`).
   - clang targeting MSVC doesn't define `__STDC__`/`__STDC_VERSION__`;
     tests/diff/parts/s6_10_4.h printed them as expressions. Now guarded.
   - cl.exe can never compile the GNU-C full_language.c; tools/bench.c now
     marks that workload `gnu_c` and prints a "skipped (GNU-C workload)" row
     for STYLE_CL compilers instead of n/a.

## FIXED: arm64 stage2 crashes — replay dropped vcheck_cmp before gfunc_call

Root cause (one line): `ast_replay_value`'s `AST_Invoke` case replayed the
call arguments then called `gfunc_call` directly, but the normal parser
(mccgen.c:9477) runs `vcheck_cmp()` immediately before `gfunc_call`. That
materializes a top-of-stack `VT_CMP` (a short-circuit boolean like the
`a && b` last arg of `map_add(...)` in `func_arg_test`). Without it the
replay left the argument as a live `VT_CMP` jump-chain value. arm64
`gfunc_call` then `vpushv`-copies that arg to a stack slot and `gv`s the
copy — resolving/patching the jfalse chain to a `B` — while the original
`VT_CMP` (same chain offset) is still on the vstack; materializing it a
second time (via `vcheck_cmp` inside `vseti`→`vsetc`) re-walks the
already-patched link (`0x14…` read as a chain offset) → SIGSEGV in
`gsym_addr`. x86_64 `gfunc_call` doesn't double-materialize a stacked
`VT_CMP`, so only arm64 (and PE-arm64) crashed. The double-resolve is
data-position-sensitive, which is why it was perturbation-fragile and only
tripped deep into `full_language.c`.

Fix: add `vcheck_cmp();` before `gfunc_call((int)nc - 1);` in the
`AST_Invoke` replay case (src/mccast.c), matching the capture path.
Verified on native arm64 Linux: st2 compiles full_language.c and self-hosts
mcc.c at -O0/-O1/-O2/-O3 (was SIGSEGV at -O1+); fixpoint byte-identical
stage2==stage3==stage4; exec + exec-replay/-tmpl/-promote suites 271/271.
Recheck macos-arm64 / msvc-arm64 on CI.

## (historical) Open: arm64 stage2 crashes (macos-arm64 + msvc-arm64 CI, qemu repro)

**Symptom (CI):** on arm64 (Darwin and Windows), the self-hosted stage2 mcc
segfaults compiling `tests/diff/full_language.c` at `-O1/-O2/-O3` (works at
default); `[-O3]` rows are n/a for all workloads on macos-x86_64 too (that
one untouched, see task below).

**Local repro (Linux, no arm64 hardware needed):**
- Cross mcc: `gcc -O1 -o mcc-arm64 src/mcc.c -DMCC_TARGET_ARM64=1
  -DMCC_AMALGAMATED=1 -DMCC_CONFIG_PREDEFS=1 -DMCC_CONFIG_OPTIMIZER=1
  -DMCC_VERSION=... -DCC_NAME=CC_gcc -DGCC_MAJOR=15 -DGCC_MINOR=0 -I src
  -I src/arch/* -I src/objfmt -I src/formats -I include -I <bd> -I .`
  where `<bd>` holds `mccdefs_.h` (tools/c2str runtime/include/mccdefs.h).
- Staging dir: runtime/include copied to `<bd>/include`, mccrt objects for
  arm64 (float128 + common + runmain/tcov/bt-exe/bt-log kept OUTSIDE the
  archive) built with the cross mcc, `libmccrt.a` at `<bd>` root.
- Stage2: `mcc-arm64 --sysroot=vendor/gentoo-stage3-arm64-glibc -B<bd>
  src/mcc.c -o st2 <same -I/-D set, CC_NAME=CC_mcc,
  -DMCC_CONFIG_AUTO_MCCDIR=1>`.
- Run: `qemu-aarch64 -L vendor/gentoo-stage3-arm64-glibc ./st2 -O1 -c
  tests/diff/full_language.c -I. -Iruntime/include -DCC_NAME=CC_mcc -w`
  → SIGSEGV (rc=139). Same at -O2/-O3; default OK; corpus.c OK at all
  levels.
- gcc reference build (native arm64 via binfmt+bwrap chroot into the
  sysroot): `bwrap --bind <sysroot> / --ro-bind <repo> /repo --bind <wd>
  /work --dev /dev --proc /proc --tmpfs /tmp aarch64-unknown-linux-gnu-gcc
  -O1 -o /work/mcc-gccref /repo/src/mcc.c <same flags>` → this binary
  compiles fl at -O1 fine.

**Crash signature** (stage2 built with `-bt`):
```
mcc.h:1626 read16le: invalid memory access
  by read32le / arm64-gen.c:206 gsym_addr / gsym / gvtst / vset_VT_JMP
```
i.e. `gsym_addr` walks a garbage jump-chain offset; SEGV pc/addr show
`section->data + garbage`.

**Facts established (all deterministic under qemu):**
- mcc-built arm64 binary (st2) crashes; gcc-built binary with identical
  source works. Not char-signedness (`-funsigned-char` x86_64 cross build is
  fine). Not `__OPTIMIZE__` glibc extern inlines (probe of
  atoi/bsearch/vprintf/putchar all correct; both `-O1 -U__OPTIMIZE__` and
  `-O0 -D__OPTIMIZE__` runtime *flags* were a confounded A/B — the real
  trigger is the *runtime* `-O1` replay while compiling fl, on an
  arm64-native mcc-built binary).
- Not the AST opt tiers: `MCC_AST_OPT_LIMIT=0` (new gate; disables all kept
  opt replays) still crashes; `MCC_AST_TEMPLATES=0` still crashes. The
  first (faithful) replay machinery alone triggers it.
- Not embed-jit (`--no-embed-jit` unchanged), not qemu guest stack size
  (`QEMU_STACK_SIZE=64M` unchanged). Real arm64 CI hardware shows it too,
  so not a qemu artifact.
- Object-mix bisect (non-amalgamated: gcc objects + one mcc object, linked
  with the cross mcc): the crashing file is **arm64-gen.o**; symbol-level
  mix (llvm-objcopy --keep-global-symbol) isolates **gsym_addr** — mcc's
  gsym_addr in an otherwise all-gcc binary crashes; complement (all mcc
  except gsym_addr) works.
- BUT mcc's gsym_addr passes a differential harness against gcc's (identical
  buffer outputs incl. chains, a-t==4, boundary cases), its disassembly
  audits correct instruction-by-instruction, and — key twist — an all-gcc
  build whose gsym_addr merely *calls a tiny pure gcc-compiled hook*
  (ring-buffer logger, no libc) **also crashes identically**. A duplicated
  untraced gcc arm64-gen.o via the same objcopy mechanism does NOT crash.
  So: **any non-leaf gsym_addr (any extra call/frame) triggers the crash;
  the leaf gcc version survives.** Perturbation-sensitivity, not gsym_addr
  semantics.
- gsym trace (ring buffer, fd 3): the final calls before SIGSEGV re-resolve
  the same chain head twice (`G 56280 56312`, then `G 56280 56320`) —
  walking an already-patched link word (a B-encoding) as a chain offset.
  Note: offsets repeat legitimately across capture/replay passes (replay
  rewinds `ind` and re-emits over the same bytes), so double-resolve alone
  may be normal; the *state* feeding vset_VT_JMP→gvtst (`vtop->jtrue/jfalse`)
  is what goes bad.
- ASan (x86_64 cross, same target/input, `detect_stack_use_after_return=1`):
  clean — no UAR/dangling reads on the shared code paths. Valgrind dies
  early on an unrelated illegal instruction (JIT/feature probe) before
  reaching the interesting part.
- SEGV capture in the hooked build: `pc=0x4700d4 addr=0x155d117b
  lr=0x470108` (mcc-linked exe has no symtab; symbolize by linking with a
  gcc-built map or use `qemu -d exec` around that pc).

**Working hypotheses, narrowed:** a call from gsym_addr clobbers state that
the leaf version preserves by accident. Registers audited (mcc uses only
x0/x1/x29/x30 there; hook is ABI-clean gcc code), so the surviving channels
are: (a) memory below SP scribbled by callee frames that something later
reads through a stale pointer *on arm64-host-only paths* (check
`MCC_HOST`-conditional code and setjmp/longjmp local rollback in
`ast_func_end` — the replay error trap; the mingw-SEH comment there shows
this frame has history); (b) glibc setjmp/longjmp register rollback
differences on arm64 vs x86_64 for non-volatile locals modified between
setjmp and the replay-bail longjmp (`mcc_error` → `ast_error_sink` path
fires on fl but not corpus — fl has functions whose replay errors!); the
`volatile int faithful` hints the authors know; audit every local of
`ast_func_end` (and the second replay block) modified after `setjmp` and
read after the bail, on arm64 register allocation.

**Suggested next steps:**
1. Instrument `ast_error_sink` / the `setjmp(...) != 0` bail branch with the
   ring-buffer logger (not fprintf) and confirm a replay bail happens just
   before the corrupt gsym sequence; log which function is being replayed.
2. If confirmed: memcmp-snapshot the text section + chain state before/after
   the bail to see what the bail path fails to restore (candidates: `ind`,
   `rsym`, `nocode_wanted`, `vtop`, reloc data_offset, `ast_rp_bsym/csym`).
3. The fix will likely be making the restore path in `ast_func_end`
   independent of post-setjmp locals (hoist reads before setjmp or mark
   volatile), or restoring a piece of global state the bail currently
   misses (jump-chain heads `rsym`/`vtop->jtrue/jfalse` are prime suspects
   given the crash is a stale/garbled chain).
4. Re-run: st2 (-O1/-O2/-O3) × (corpus/fl/mcc.c) under qemu; then the CI
   matrix. Also re-check macos-arm64 with the same fix.

## Open: macos-x86_64 `[-O3]` rows n/a

Untouched so far. Facts: -O2 (promote) works on Darwin x86_64, -O3 fails for
all workloads; Linux x86_64 -O3 self-host works locally. With the PE promote
fix landed, first retest CI: the PE -O3 failures were promote-caused, and the
Mach-O -O3 failure may share it (promote is active at -O3 too, and Darwin
x86_64 uses the same SysV-style rsp-relative... no shadow space, but the
frame-slot change also removed push/pop rsp movement that Mach-O unwind may
have disliked). If still red, reproduce via cross mcc→Mach-O
(`-DMCC_TARGET_MACHO=1`) — compile-only checks pass locally, so it needs
either a Darwin runner or the apple-libc harness (tests/qemu/apple-libc) to
execute stage2.

## Gates to run before closing (task list #6)

- ctest suite + diff3 + fixpointgate (byte-identical -O1 self-host) on
  linux-gcc preset; mcctest whole-corpus if touching codegen again.
- The promote fix changes ELF -O2/-O3 codegen (movs instead of pushes):
  corpus + full_language + self-host at -O2/-O3 verified locally; CI matrix
  pending.
