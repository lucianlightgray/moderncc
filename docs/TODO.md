# TODO

> History, landed-fix write-ups, superseded boards, and the "landmine" warnings that
> used to live here were moved to [`docs/ARCHIVED.md`](ARCHIVED.md) on 2026-08-05 for
> later validation. This file now holds only the configuration instructions below and
> present-tense, open items. File:line anchors are omitted on purpose — the archived
> ones had drifted 1000–1900 lines after merges; find code by symbol.

## The JIT, measured for the first time — 2026-08-09 (`wt/jitconform`)

The JIT had never been measured against an external corpus. Everything below is the
first such measurement. Harness: [`tools/jitconform.py`](../tools/jitconform.py), which
reuses the corpus collector in [`tools/xsuite.py`](../tools/xsuite.py) and the
oracle-qualification phase of [`tools/xoracle.py`](../tools/xoracle.py), and scores each
suite by the **other** vendor's compiler — gcc's tests by clang, llvm's by gcc.

**Corpus provenance, so the number reproduces.** Three read-only checkouts: gcc at
`basepoints/gcc-17-2762-g9d8f85ca333`, llvm-project at
`llvmorg-23-init-21709-g0f1f456263b5`, llvm-test-suite at `63a9fd935`.

- **gcc side, judged by clang 22.1.8** — `gcc/testsuite/gcc.c-torture/{execute,compile,unsorted}`,
  `gcc.dg`, `gcc.misc-tests`, `c-c++-common`, `gcc.target/{i386,x86_64}`, run-mode only.
- **llvm side, judged by gcc 15.3.0** — `llvm-test-suite/SingleSource/{Regression/C,UnitTests}`,
  plus `llvm-project/compiler-rt/test/builtins/Unit/*_test.c` paired with its
  `compiler-rt/lib/builtins` implementation.

Two provenance caveats that change how the llvm-side number reads.
**(1)** `llvm-project` itself has no executable C corpus — `llvm/test/ExecutionEngine`
holds zero `.c` files and `clang/test` is lit/FileCheck over IR. Only `llvm-test-suite`
and the compiler-rt builtins Unit tests are runnable.
**(2) 1543 of the 1580 qualified `SingleSource/Regression/C` programs are a vendored
copy of gcc's own c-torture suite** (`SingleSource/Regression/C/gcc-c-torture/`). They
are *not* an independent corpus; what they add is a second, different adjudicator over
programs clang already judged. The genuinely llvm-authored executable corpus here is
small: 37 non-vendored Regression/C programs, 149 `SingleSource/UnitTests`, and 148
compiler-rt builtins/tests — **334 programs**. Deduplicating the vendored overlap, the
measurement covers **5084 distinct programs**, not 6627.

**Classify-out count.** 7759 run-mode programs were collected; **6627 entered the oracle
set and 1132 (14.6%) were classified out** — 1088 the oracle itself could not build
(475 of those are LoongArch `lsxintrin.h`/`lasxintrin.h`, AVX-512 `m512_test_util.h` and
PowerPC `altivec.h` intrinsic tests that cannot build on this host at all), 35 whose
`-O0` and `-O2` oracle binaries disagree (undefined-behaviour-sensitive), 7
nondeterministic across two runs of the same binary, 1 oracle timeout, 1 oracle bad
flag. A program both compilers reject is not an mcc failure and is not counted as one.
`llvm-test-suite`'s SingleSource is pre-C99 K&R-era C and is compiled `-std=gnu89
-fcommon` — its own `SingleSource/Regression/C/CMakeLists.txt` passes `-Wno-implicit-int`
for the same reason. Without that flag a C23-default gcc 15 refuses 640 of them on
implicit-int and implicit-declaration alone, and they would classify out as untestable
rather than measure anything. Oracle and mcc get the identical flag.

**The coverage number, x86_64 Linux, `-O2`, against those 6627 programs.**

| surface | AGREE | UNSUPPORTED | DIFFER | MCC-REJECTED |
|---|---|---|---|---|
| `--embed-jit` bake, run under `MCC_JIT=1` | **3251 (49.1%)** | 3118 | 113 | 145 |
| `-run --jit` in-process | **3235 (48.8%)** | 3120 | 101 | 171 |

AGREE means the runtime JIT engine was *observed to boot inside the program* — the
harness only counts a pass when `MCC_JIT_VERBOSE=1` produced an `mccjit-boot[...]` or
`mccjit-lazy[install]` line — **and** the program's exit status and stdout matched the
cross oracle byte for byte. A program the engine never touched is recorded as
UNSUPPORTED, never as coverage.

**What the JIT refuses that the static path accepts: 3118 of 6627 (47.1%).** These are
`NOT_BAKED` — `--embed-jit` linked no engine into the output because
`mccjit_intent_serialize` (`src/mccjit_intent.c`) could not serialize the function. Its
refusal conditions are `ast_arena_has_asm`, a named symbol that is neither a data symbol
nor an identifier-range token, and any called symbol that is `VT_STATIC | VT_INLINE`.
The static path compiles all 3118 fine. The engine booted in 3313 programs, and its own
routing verdict there was `refused` 2154, `swapped` 778, `kept-aot` 381 — so it declines
a second time, at runtime, in 65% of the programs it had agreed to bake.

**MCC-REJECTED is a language gap, not a JIT verdict**, and is bucketed separately by the
harness: of the 145, the top reasons are `unresolved reference` 42 (mostly missing
builtins), `cannot use local functions` 29 (GNU nested functions), 10 unusable flags, and
a long tail of one- and two-count parse/semantic gaps. Nothing in that bucket is counted
against the JIT.

**Per suite (embed surface, AGREE / oracle-qualified):** `gcc.target` 1112/1222,
`gcc.dg` 687/1831, `gcc.c-torture/execute` 623/1605, `llvm:ts-regression` 611/1580,
`llvm:builtins` 126/139, `llvm:ts-unittests` 53/149, `c-c++-common` 33/87,
`llvm:compiler-rt` 5/9, `gcc.misc-tests` 1/5.

### OPEN, and it is a miscompile: the KGC route zero-extends nothing

**22 records on the embed surface and 14 on `-run` produce a different answer under
`MCC_JIT=1` than the same binary produces under `MCC_JIT=0`.** Same object code, same
libc, only the runtime JIT differs — so nothing about the AOT compiler is implicated.
Deduplicating the vendored gcc-c-torture copy inside llvm-test-suite, that is **16
distinct programs**, 14 on the embed surface and 8 on `-run`. Most abort, because the
torture tests are self-checking and call `abort`. The union:
`20050502-1.c`, `941014-2.c`, `970217-1.c`, `builtin-prefetch-4.c`, `loop-3.c`,
`loop-3b.c`, `pr109986.c`, `pr17377.c`, `pr39240.c`, `pr65215-2.c`, `pr65215-3.c` in
`gcc.c-torture/execute`; `gcc.dg/fastmath-1.c`, `gcc.dg/pr96674.c`,
`gcc.dg/torture/pr45830.c`, `gcc.dg/torture/pr126136.c`; and
`llvm-test-suite/SingleSource/UnitTests/Threads/tls.c`. (`pr17377.c` reached the harness
only through llvm-test-suite's vendored copy: the gcc-side original carries
`dg-require-effective-target return_address`, which `xsuite.py`'s DejaGnu handling skips,
and llvm-test-suite ships it stripped of directives. `tls.c` is genuinely llvm's. So the
second corpus did earn its keep even where it duplicates the first.)

Minimal reproducer, banked at [`tests/jit/known-bad/kgc_zext_ret.c`](../tests/jit/known-bad/kgc_zext_ret.c):

```c
int printf(const char *, ...);
unsigned int foo(unsigned int x) { return x; }
unsigned long long lo(unsigned long long *x) { return foo(*x >> 32); }
int main(void) { unsigned long long l = 0xfeedbea800000000ULL; printf("lo=%llx\n", lo(&l)); return 0; }
```

```
MCC_JIT=0 mcc -O2 -run tests/jit/known-bad/kgc_zext_ret.c   ->  lo=feedbea8          (gcc, clang agree)
MCC_JIT=1 mcc -O2 -run tests/jit/known-bad/kgc_zext_ret.c   ->  lo=fffffffffeedbea8
MCC_JIT=1 MCC_JIT_KGC=0 ...                                 ->  lo=feedbea8
```

The JIT widens an `unsigned int`-returning call to `unsigned long long` with **sign**
extension. Replacing `unsigned int foo` with `int foo` makes all three agree on
`fffffffffeedbea8`, which is the correct answer for a signed callee — so the JIT is
behaving as though the callee's return type had lost its `unsigned`. `MCC_JIT_VERBOSE=1`
reports `route=kgc ... swapped` on the failing run, and `MCC_JIT_KGC=0` is a complete
workaround, so the defect is in the known-good-constant specialization route, not in the
direct recompile. Reproduces identically on both JIT surfaces. **Not fixed** — the fault
is somewhere in the intent-blob round trip of the callee signature
(`mccjit_intent_serialize` / `mccjit_intent_deserialize` / `mccjit_build_rec` in
`src/mccjit_intent.c`) or in the KGC variant builder in `src/mccjit_embed.c`; narrowing
it further needs more time than this branch had.

The existing `jit/run-parity-host` cell (`tests/jit/run-parity.sh`) runs exactly this
`MCC_JIT=0` vs `MCC_JIT=1` differential and is green, because its corpus is five
hand-written programs in `tests/jit/parity/` and none of them return a high-bit-set
32-bit unsigned value through a widening call. Dropping `kgc_zext_ret.c` into
`tests/jit/parity/` would turn that cell red immediately; it is deliberately parked one
directory away, in `tests/jit/known-bad/`, until the bug is fixed.

### The two new cells, and what arms them

- **`jit/xoracle-known-positive`** — `tools/jitconform.py --phase selfcheck`. Needs no
  corpus and no second compiler. It runs two programs through the real check path with
  four fixed oracle records: a JIT-hot one that must reach `PASS` (proving the engine
  actually booted — if it does not, the cell says so and fails), and falsified expected
  exits on both a JIT-engaged and a JIT-declined program, both of which must come back
  `DIFF_EXIT`.
- **`jit/xoracle-conformance`** — qualifies and checks a deterministic slice of at most
  400 programs per suite from `gcc.c-torture/execute` (clang as oracle) and
  `llvm-test-suite/SingleSource/UnitTests` (gcc as oracle), so the cell itself runs both
  oracle directions. `--min-pass 100 --max-miscompile 1`, ~47 s. Current measurement:
  493 oracle-qualified, **167 AGREE** (125 gcc-side, 42 llvm-side), 315 NOT_BAKED,
  5 MCC-REJECTED, 6 DIFFER of which **1 is the KGC miscompile above** — that is what
  `--max-miscompile 1` banks. A second miscompile fails the cell.
  `MCC_XSUITE_GCC` and `MCC_XSUITE_LLVMTS` are cache PATHs defaulting to
  `$ENV{HOME}/Projects/{gcc,llvm-test-suite}`; without the gcc corpus the cell registers
  as a *skip with the reason*, never as a silent pass, and without llvm-test-suite it
  measures the gcc direction alone. `--limit` is applied per suite to a path-sorted
  oracle set, so the slice does not drift with thread-completion order.

The ablations, quoted. Floor: `--min-pass 9999` gives
`jitconform: FAIL floor: 125 programs ran correctly under the JIT, below the --min-pass
9999 floor -- an empty or vacuous corpus must fail here, not pass quietly`. Miscompile
pin: `--max-miscompile 0` gives `jitconform: FAIL miscompile: 1 programs where MCC_JIT=1
and MCC_JIT=0 disagree in the same binary, above the --max-miscompile 0 pin`. Absent
corpus: configuring with `-DMCC_XSUITE_GCC=/nonexistent/gcc` registers the cell and
ctest reports `jit/xoracle-conformance (Skipped)`. Known-positive: deleting the two
oracle comparisons from `check_embed` turns the selfcheck into
`hot-lie want=DIFF_EXIT got=PASS FAIL` / `cold-lie want=DIFF_EXIT got=NOT_BAKED FAIL` /
`jitconform selfcheck: FAIL (2 case(s))`, exit 1.

### Routing note for the `__int128` and float/double work

The compiler-rt builtins corpus is the densest `__int128` and soft-float set available,
and it isolates cleanly: **131 of 148 pass, and every one of the 14 that differ has
`aot_agrees: false`** — the JIT and the AOT path agree with each other and both differ
from gcc, so these are language/runtime gaps, not JIT defects. They are exactly:
`divtf3`, `divtc3`, `multc3`, `extendxftf2`, `trunctfxf2`, `fixunstfdi`, `floatditf`,
`floattitf`, `floatunsitf`, `floatunditf`, `floatuntitf` (all `tf`/`xf` — `long double`
and `__float128` soft float), `muloti4` (`__int128` overflow multiply), and
`compiler_rt_logbl`/`compiler_rt_scalbnl` (`long double` math). One more,
`trampoline_setup_test.c`, is refused outright with `error: cannot use local functions`.
The per-record verdicts are in `<build>/jitconform-all/jit-embed-O2.jsonl`; each carries
`got_head`/`want_head` for the first differing output.

### Still open on the JIT after this branch

1. **The KGC zero-extension miscompile above.** Highest priority; it is a wrong-answer
   bug reachable from ordinary C.
2. **47.1% of programs cannot be baked at all.** The single largest lever on JIT
   coverage is the `VT_STATIC | VT_INLINE` callee refusal in `mccjit_intent_serialize` —
   a static helper called from the JIT'd function disqualifies the whole function, and
   that is the commonest shape in the corpus.
3. **`--embed-jit` suppresses its own no-bake warning under `-w`.** `mcc_warning` is
   routed through the warning machinery, so `mcc -w --embed-jit` silently produces an
   engine-less binary. `tools/jitconform.py` had to detect the bake by searching the
   output for the engine's own strings instead. Callers who pass `-w` get no signal.
4. **The lazy route fails to build a variant on programs the sync route handles.**
   `MCC_JIT_LAZY=1 MCC_JIT_HOT_CALLS=1` on the `tools/embed-jit-smoke.py` program prints
   `mccjit-lazy[promote]: build failed (1/3) ... giving up, baseline is final` three
   times while `MCC_JIT=1` alone reaches `route=direct ... kept-aot` on the same binary.
   Not measured at corpus scale here.
5. **Nothing measured off x86_64.** The engine compiles for x86_64, arm64 and i386
   (`MCCJIT_X64` / `MCCJIT_ARM64` / `MCCJIT_I386` in `src/mccjit_embed.c`); only x86_64
   was run. The `jit/arm64-*` qemu cells remain the only arm64 evidence.

## What is actually still open — swept and verified 2026-08-09 (`wt/sweep`)

Every section below this one was enumerated and each item classified OPEN / CLOSED-VERIFIED
/ CLOSED-UNVERIFIED / REFUSED / SUPERSEDED-STALE. Closure claims were checked by **running
the tool or the cell**, not by reading. **24 spot-checks were taken; 20 reproduced exactly,
3 did not, and one probe turned up a crash nobody had filed** — all four are in this table
below, at rows 1, 2, 5 and in the staleness list. The device-path verdict, the Metal
drop, chain-store re-promotion, `storeval-rot`'s demotion and `narrow`/`tree-copy-prop` were
each re-read against their measurements and are **settled**; nothing below re-litigates them.
**Float support was reopened by decision and landed on 2026-08-09 (`wt/fpwidth`) — see row 6
and the M6 section. Its headline finding is that the row's own price was wrong about which
gate was binding, so do not quote the +79.2.**

**Reading the `:NNNN` anchors below — the `+137` rule that used to be here was WRONG, and
this is a correction, not a refresh.** It claimed *"an anchor `:N` here is at line `N + 137`
today — `:16` is now `:153`, `:3015` is now `:3152`."* Both worked examples are false: line
153 is *"Named, with the cause of each, at `-O0`/`-O1`…"*, not the registration figure, and
line 3152 is the `MCC_ARENA_DUMP` paragraph, not the `indirect`-guard row. **There is no
single offset**, because more than one section was inserted ahead of the body at different
times. Measured on this tree, two clusters:

- anchors in the low thousands and below drift by **+468** — `:521` → `:989`, `:670` →
  `:1138`, `:675-687` → `:1143-1155`, `:685` → `:1153`, `:697` → `:1165`;
- anchors from roughly `:3600` up drift by **+802 to +804** — `:3693` → `:4497`, `:9598` →
  `:10400`, `:9600-9602` → `:10402-10404`, `:9584-9590` → `:10386-10392`.

Treat every `:NNNN` in this file as approximate and confirm by content, not by arithmetic.
Anchors into other files (`src/`, `tools/`, `PLAN.md`, `CMakeLists.txt`) are unaffected and
are checked by `docs/refs` (`tools/docref-lint.py`). This is the drift the header warns
about; the arithmetic that was supposed to defuse it did not.

Tree state at the time of the sweep: `cmake-cross` built before `cmake-debug` was
configured, both register **9151** cells, `tools/selfhost-smoke.py cmake-debug` green, all
34 `slice/*` + `gpu/*` cells green on a real device, `must-run: 60 row(s) satisfied`.

**The honest headline: no ranked row of the board is materially unfinished. What is left is
gates that do not reach, two live compiler defects nothing watches, and a set of figures
that will corrupt the next measurement if quoted.** Rows 1–5 below are the only ones I would
schedule.

| # | row | size | currency |
| ---: | --- | --- | --- |
| **1** | ~~**`slice-census` is RED on this tree and no documented invocation runs it.**~~ **CLOSED on `wt/censusfix`** — both halves. The 9 `src_fail` sources were a corpus defect, not compiler defects: the walk handed `tests/exec/*.c` to a bare `mcc -c`, ignoring the flags and the `req` each source's row in `tests/exec/goldens.h` declares. The two `--verify` overruns were a denominator defect: slice extents are measured on the *replay* and were being compared against the *parser's* body length, which only coincide when the body is faithful, and neither of those two is. See the Landed section below for both readings | — | **cells** |
| **2** | ~~**`ctest -L census` runs 3 of its 6 cells and reports 6.**~~ **CLOSED on `wt/censusfix`** — all five census cells now carry their own `ENVIRONMENT` switch, and a new `census/gates-armed` cell fails if any `census`-labelled cell is gated on an opt-in switch its registration does not set. `ctest -L census` is **7/0 with nothing Skipped** and needs no exported variable at all. `MCC_RIR_CENSUS` was in the same state as the other two — named nowhere in `CMakeLists.txt` — so `rir-coverage-census` and `rir-nofb-probe` only ever ran because somebody typed it | — | **cells** |
| **3** | **`-fopt-slice` makes object output depend on the optimizer's disk cache, and nothing watches it.** Reproduced verbatim today: `python3 tools/opt-cache-determinism.py cmake-debug/mcc src/mcc.c --opt=-O3 --from-build cmake-debug -- -fopt-slice` → `cold/self/foreign-tu = daffa4e023f9`, **`foreign-fl = 1dbdfbe1bc0c`**, `cache entries written: 2`, `FAIL`. The cell is a **permanent 77** because the flag is `MCC_OPTD_LEVEL(9)` and has no subject at any shipped level, so the defect is invisible rather than absent. Decide: own the pass, or delete it | unknown (a pass) | **correctness / determinism** |
| **4** | **`if-conversion-abs` ships at `MCC_OPTD_LEVEL(2)` and the freshly re-run bench says it makes code worse.** `tests/optfire/levelbench.tsv:20`: moves **1 of 17** kernels, `gain_movers` **−0.0334**, `branchy` **−0.5700** — a sign flip from the `+0.1905` / `+3.1843` it was promoted on. It is bucketed `ranked`, not `cost-no-gain`, so the ladder still treats it as a win. It is filed **only** in the failed-to-reproduce table at `:685`; no row of the ranking table owns it, and `:517` asserts "row 1 is the only unmeasured row left" | small (one level decision, the measurement already exists) | **emitted code** |
| **5** | **`MCC_MAX_UNARY_DEPTH` was mis-sized *and* it was one guard where the parser needs eight — DONE, and the eight are now watched.** `diag.parse-frames` re-prices `MCC_MAX_PARSE_DEPTH` against the frames it was sized on, every run; the per-level table it was sized from was re-derived and nine of its ten rows were low. **Still open, filed with crash depths:** two more recursion points bypass the shared budget entirely and SIGSEGV at the 8 MiB default — `parse_btype`'s `_Alignas` arm at **43,609**, and `mccasm.c`'s six-function `asm_expr` cycle at **~19,000**. See "The parse-depth guard" below | small (two more wrappers) | **correctness** |
| **6** | **Nine number-producing tools are registered nowhere — the board says four.** `:1688-1696` names `xsuite-report.py`, `gate-ledger.sh`, `strategy-ledger.sh`, `c2_sweep.sh` and closes "Four tools left on this item." Also unregistered and board-quoted: `xsuite.py`, `xoracle.py`, `c2_equiv.sh`, `selfhost-o3.py`, `arm64pe_diff.py`. **`xoracle.py` is the sharpest**: `tests/optfire/levelpins.txt:78` pins `merge-constants` at level 2 on "two xoracle cases change verdict without it" — a shipped ladder pin whose only evidence comes from a tool no cell runs | medium (five more cells) | **census trust** |
| **7** | **`ast_env_gate` no longer exists in `src/` and four shell tools still grep for it.** `grep -rn ast_env_gate src/` is **0**; `tools/{c2_sweep,c2_equiv,gate-ledger,o0_ab}.sh` all still reference it. They fail loudly, which is the right mode, but this is the widest blocker in the file: it freezes `o0_ab.sh`'s gated half (twelve `*.gated.rir.txt` + `board.gated.txt`, uncovered by `ast/o0-baseline` and not pretending otherwise), blocks three of the four tools in row 5, and blocks the cheap "which `-O1` gate erases the 72 `len` bodies" experiment. The restoration recipe is already written down at `:9899-9906` | medium | **gate strength** |
| **8** | **`spirv-val` and `glslc` are installed at `/usr/bin` and referenced nowhere in the build.** `grep -rn 'spirv-val\|glslc' CMakeLists.txt cmake/ tools/ src/` is empty. 152/152 modules already validate by hand at `--target-env vulkan1.1`. One `find_program` and one `add_test` arm. The cheapest open item in the file, and it survives the device freeze because it validates what the emitter already ships | small | **device correctness** |
| **9** | **A stage-2 build dir does not rebuild when a header changes.** The stale binary is silent and plausible: it runs, it self-hosts, it passes. Workaround only (`rm cmake-<dir>/CMakeFiles/mcc.dir/src/mcc.c.o`); the fix is for `mcc` to emit a depfile for `CMAKE_DEPFILE_FLAGS_C`. This poisons **any** measurement taken from a stage-2 dir, which is most of the ladder work | medium | **measurement validity** |
| **10** | **D6 — `scalar_storage_order` / `ms_abi` are not implemented at all** (`grep -rn 'scalar_storage_order\|ms_abi' src/*.c src/*.h` → **0**), and mcc objects link against gcc's, so a mismatch is *silent* wrong codegen across a linker boundary. The only item in the codegen list with that property | large | **correctness** |
| **11** | **`selfhost-optbench.py --check` can pass over zero derivations.** `derive_levels` assigns `levels[f] = levels_now[f]`, so an all-`inert` run prints *"src/mccopt.h matches the ladder"* having derived nothing; the docstring says **48** level-assignable flags in five places and `flag_table()` yields **16**; no floor on `len(names)`; an empty sample list classifies `inert`. The board already carries "`selfhost-optbench --check` was not re-run" as a caveat, which is the same hole one level up | medium (several floors) | **census trust** |
| **12** | **W8 — `selfhost-jit` heap-UAF of a `Sym` in the AST forward-inline re-emit path.** Root-caused, verified unfixed: `ast_inline_retain` (`src/mccast.c:3533`) and `ast_reemit_retain` (`:3561`) are still called at `:19217-19218` with no refcounting across cross-function grafts. It has a deterministic oracle (MSVC-ASan `mcc_s` + `tools/selfhost-jit.py`) | medium–large | **correctness** |
| **13** | **`run-tier/x86_64` fails `tls_threads` when `MCC_JIT=1` meets an active AST replay.** Localised to three lines: `mcc_jit_tls_slab` (`src/mcchost.c:1450`), the `mcc_run_pthread_create` binding (`src/objfmt/mccelf.c:974`) under `s1->run_tls_active`, set only on the interpreter relocate path in `tls_setup_linux` (`src/mccrun.c:451`). `--no-jit` does not suppress it. Note this contradicts `:10090`'s "the deliberate-red count is now 0", which is true only of the default configuration | small–medium | **correctness** |
| **14** | **`ptr_unlink` for-condition-store segfault** — root-caused to `rir_cf_cond`/`rir_docond`, needs a 5-fix/34-break discriminator. Orphaned: zero references anywhere else in this file | medium | **correctness** |
| **15** | **`full_language.c` still diverges at `-O0` on x86_64/i386** — an `AST_OP_ASM` replay defect (P4 defect 4). Contained, not closed; zero later references | medium | **replay fidelity** |
| **16** | **The `jit-splice` pin hides a live miscompile.** `tests/optfire/cover3.py:44` pins it with the reason *"miscompiles `programs/random_stuff` at `-O2`; OFF in `mccopt.h` for that reason"*. That miscompile appears nowhere in the codegen-defect list | medium | **correctness** |
| **17** | **`-O3` re-emission leaves the pre-inline copy in `.text`** — **27 functions / 52,022 B, ~3.6%** of `.text`. Not a correctness bug; no cell, no bank, no entry in the codegen list | medium | **emitted code** |
| **18** | **`--mutate` is blind to `memcpy`, and the real gap is the corpus.** Four of six operator sites already perturb written memory and `g_frame_mismatch` exists; what is missing is **any `memcpy`/`memset` in the slice corpus to mutate**. Smaller than the debt as filed | small | **test strength** |
| **19** | **Debt 6-vi — the chain-store *member* fixture was never written.** Its stated blocker (debt #6a's `-O1` vstack underflow) has been gone since 2026-08-09. `exec-chainlive/*` covers the live half; the member half of the pairing has no cell | small | **regression cover** |
| **20** | **`flagsweep-cover` and `asm-gas-directives` are `mcc_skip_test` stubs — `cmake -E echo`, structurally incapable of failing.** `flagsweep-cover` hides 75 covering-array rows behind an opt-in that nothing runs; `asm-gas-directives` parks a real unimplemented feature (*"integrated assembler lacks sgdtq/sidtq/swapgs encodings"*) as an always-green cell. Neither is in `tests/must-run.txt`. There are **74** `mcc_skip_test` call sites, 17 live in this configuration | small each | **cells** |
| **21** | **Hazard 1 is still live: `BREAKEVEN` is a hand-pinned literal** at `tools/loop-census.py:125`, duplicated as `lc_thr[]` in C, and it cannot be gated (`--cost-synth` 77s with no device, `slice/cost` carries `SKIP_RETURN_CODE 77`). The provenance banner landed; the constant did not. **Un-pinning is ~15 lines of C + ~25 of Python**, and it is what the entire remaining integer lane source (`vlaloop`'s 64 trips against a frozen `8`) is adjudicated against | ~40 lines | **ns / lanes** |
| **22** | **`rir-coverage.py`'s `wide` denominator is "the files that happened to compile"** — an `os.walk` of seven directories with no manifest, so a source dropping out silently shrinks the ratchet. Cheap first step already named: bank `sources=N` and fail when it moves. Adjacent: `LOW_EXCLUDE` is a **filename suffix match with no count**, so renaming `mccgpu.c` produces a fake regression with no diagnostic | small (the floor) | **census trust** |
| **23** | **`rir-nofb-probe`, `--check-gap-dir` and `--check-low-dir` all pass over an empty input.** The bank already holds four empty `nofb_miscompiles` lists; gap fixtures cover **3 of 18** `UNF`+`WHY` classes | small per guard, medium for fixtures | **gate strength** |
| **24** | **`stratsweep.sh` and `flagsweep.sh` drop subjects silently.** `$WORK/skipped` is written and never counted; the only floor is `n > 0`, so a miscompile breaking 30 of 31 subjects prints `PASS stratsweep-iso all: 22 strategy/ies x 1 subjects`. Both already print the survivor count — pin it | small | **gate strength** |
| **25** | **The non-LVAL local `Ref` question is now answerable, not open** (`src/mccslice.h:264`, `:5685`). `wt/decaytype` fixed the identical defect in `ast_dep_decode` on 2026-08-09 — an `AST_Ref` accepted as a base address without checking `VT_LVAL` — with cell `id=25 dp_gptr_alias`. That answers the semantics in favour of "address" but did not touch `ast_eval_slice.h`'s `Ref` arm, `kind_ok` or `livein`. Blast radius **93 of 3994 accepted slices (2.3%)**; one directed test settles it | small | **reference correctness** |
| **26** | **Cluster L is a dependency chain and its first link is unbuilt.** `L1` — give the JIT a shutdown — blocks `L2`/`L3`/`L4`/`L6`/`L7`/`L8`/`L9` by construction (`:8044` says so). Workers are `pthread_detach`ed at `src/mccjit_embed.c:1375` into an unbounded `pthread_cond_wait` at `:1341`/`:1347` with no `pthread_t` retained. `L5` landed as L3 residency (**32×** on fixed cost); nothing else in the cluster has. It is the same defect as open item 4 at `:8657`, `PLAN.md:916`, and the coroutine task's item 1 — **four rows naming one blocker** | redesign | **device lifetime** |
| **27** | **The gate-mask gap.** `ast_math_inline_env`, `ast_interchange`, `ast_fusion`, `ast_tile` and `loop-vlat` mutate the arena before the JIT's mask snapshot and carry no `AST_SG_*` bit, so the JIT cannot know what shaped the tree it is handed. Stated at `:8650` and again at `:7756`; no later mention. This is the same class of defect as row 1 of the board's own ranking (a predicate reaching emitted code without its guard) | design | **correctness** |
| **28** | **`storeval-callstore` is at `MCC_OPTD_LEVEL(2)` and was never ranked in either direction** (`src/mccopt.h:39`). The ICE that made its off-state unmeasurable was fixed at `:7629`; nobody has run the bench since. Adjacent and larger: **32 of the 34 demoted rows on rungs 10/11/12 are still unpriced** — only `narrow` and `tree-copy-prop` were measured, and rung 12 remains a deletion-candidate list nobody has read | one bench, then 32 | **emitted code** |
| **29** | **The `MCC_OPT_REPLAY_FALLBACK` flip is an untaken decision, and the fallback is silent either way.** No known defect blocks it (`:9126`), the backstop landed at `705f0b0f`, all four delta-debugged flag sets closed, and `rir-nofb-probe` banks zero miscompiles. Keeping the gate costs **2.0% of bodies but 10.2% of body bytes** getting no optimization at all at `-O1`. **Recommended under either decision and not done: make the divergence visible** — `rir_prod_note` only reports at `MCC_RIR_PROD>=2`, so in a default build a fallback leaves no trace | small (visibility), then a decision | **emitted code** |
| **30** | **No tree-recursion exec golden exists, and the failure mode is a GPU hang.** MSL compiles `fib(n)=fib(n-1)+fib(n-2)` and then **hangs the device at n=5** (`kIOGPUCommandBufferCallbackErrorHang`), taking sibling command buffers with it. Recorded as `PLAN.md:475` (C4); the golden was never written. Partly defused by the Metal drop, but the golden is a CPU-side conformance test and is still missing. Two more unused-as-conformance shapes named beside it: computed `goto` **into** a `for` body (`tests/diff/parts/legacy_expr.h:60-95`) and label arithmetic (`tests/exec/codegen/nodata_wanted.c:48,76`) | one golden | **conformance** |
| **31** | **Two device residuals nothing owns.** N11's duplicated upload in `ast_ladder_gpu_run` (the identical `tin` uploaded twice per rung) is untouched — `PLAN.md:1059` says so. N10 picks `devs[0]` with no scoring while `VkPhysicalDeviceLimits` is fully transcribed at `src/mccgpu.c:604-711` and only `deviceName` is read — `PLAN.md:1058`, "the `devs[0]` half of I2(D) is still open". Both are frozen with the device path; both are cheap and neither is written down as a row | ~10 lines / ~60 lines | **device correctness** |

Below the line, and deliberately: the device path's own open rows — float (**LANDED 2026-08-09 on `wt/fpwidth`: `double` only, `+ − *` and comparisons, bit-exact; `float`, division and int↔float excluded with cells. It moved the numeric corpus's device-executable fraction by ≈0.0 iteration-weighted points — the 79.2 were gated by `static` storage and `MCC_SLICE_MAXSLOT = 16`, not by `is_float`. See row 6 and the M6 section**), the dispatcher (three subsystems, priced nowhere), 115 indirect blocks, recursion (no data), the `pe` lowerable floors (stale-low, so they under-gate rather than false-fail), and debt #3's descriptor staleness (fixed, unreachable until binding 2 grows). All are frozen by the 2026-08-09 decision and none should be scheduled while that stands.

### Claimed closed, and nothing enforces it

These are the ones worth knowing about. None is a lie; each is a closure resting on prose,
on a single hand-run, or on a cell that cannot reach it.

1. **`slice-census`, `loop-census`, `loop-census-numeric`** — `registered` in the manifest, so `ci/must-run-registered` is green on all three, while no documented invocation runs any of them and one is red. Rows 1–2 above.
2. **`opt-cache-determinism` and `runtime-bench-gatewin`** — both permanent 77s. The manifest discloses this honestly, which is the right thing; nothing ratchets them back to live, and for the first the underlying defect still reproduces. Row 3.
3. **Debt #3, `mcc_vk_bind_mem` descriptor staleness** — the fix is in the tree (`dsdirty`, `src/mccgpu.c:1781/1894/1933`) and the debt states plainly that it is *"not test-covered, and cannot be"*, because both callers pass the constant `MCC_VK_MEM_DEFAULT`. A debt marked paid whose payment is unreachable by construction.
4. **Debt #7, "464 skipped cells"** — closes with *"`cmake-debug` now registers 9106, the same as `cmake-cross`"* (`:3693`). Hazard 5 measures exactly that claim as **false** (8972 vs 9136 by configure order) and says the fix *"did not close the gap — it moved it"*. The debt and the hazard contradict each other and both are written as current.
5. **The Metal freeze** — *"Keep the `#if MCC_GPU_LANG_MSL` arms only where they already compile; add no more"* (`:483`) is enforced by nothing: no cell, no lint. The decision is right; it has no ratchet.
6. **W3's 3-way-concurrent closure** — the best-evidenced result in the file (399 chains / 0 non-identical against a reverse-applied negative control, Fisher p = 0.0015) and no cell holds it. A regression reappears only as flake.
7. **The `mslgate` arm** (`:4033`) — compiles clean and links 51 `msl_*` refs; never executed. Now moot under the Metal drop, but still written as an open verification gap.
8. **`ast_eval_slice()`'s poison-flag fix** (debt row 2) — fixed in code and correct; no cell is named for it. Coverage is incidental via `slice/deref` / `slice/real` / `slice/musl`.
9. **The `narrow` pin** — the banked figure did not reproduce (banked **−0.60%** cpu / **1.91%** stage-1; re-read **−0.0088%** / **+0.876%**, about half). The pin rows now carry both numbers; the discrepancy was annotated, not explained, and no cell compares `levelpins.txt` against a re-take.

### Written as live, actually superseded

1. **Board row 1 and still-open row 1** (`:497`, `:507-514`, `:3015`) call the missing `indirect` guard on `ast_dep_base_distinct` **"UNMEASURED, and it is the top of the board."** It landed at **`adf08e3b`**. Verified in the tree: the parameter is `src/mccast.c:13347`, the guard `:13350`, both emitting callers pass `0` (`:13516`, `:13566`), only the census site passes `ast_dep_alias_oracle_env` (`:13949`) — and the **22 `exec-{interchange,fusion,tile,search*}/loop_*` cells are registered and green**. *The board's number-one open row is closed.* Its own body says so at `:2104`.
2. **The registration figure is two generations stale.** `:16-17` say **9136**; `:624` and `:2656` say 9136 *"today"*; `:985` says **9149**; the tree says **9151** in both dirs. The `wt/gatefin` write-up raised it to 9149 and never propagated to the head; the merge with `wt/idiomcov` added the last two and nothing recorded it. Hazard 5's "164 low" delta is quoted against the stale pair.
3. ~~**Three counts of one list.** `:521` "**Twelve** have now failed to reproduce", `:670` "**The nine** figures that have failed to reproduce", `:697` "**Seven** headline figures" — and the table at `:675-687` has **thirteen** rows.~~ **CLOSED on `wt/docsync`.** The table is now the only place the count is stated; every other site points at it instead of restating it, and the two historical ordinals ("the seventh", "the thirteenth") are date-stamped as counts-at-the-time rather than current. `docs/refs` (`tools/docref-lint.py`) counts the table's rows and fails if the stated count moves off them, so this cannot drift again silently.
4. **`SKIP_RETURN_CODE` count.** `tests/must-run.txt:3` says **141**, `docs/PLAN.md:1011` says **138**, `CMakeLists.txt` has **149**.
5. **`:9598` — "Deliberately not banked: byte faithfulness."** False at HEAD. `kept_coverage` is banked on all eight rows of `tests/rir/coverage-bank.json` **and enforced** (`tools/rir-coverage.py:1105-1109`, skipped only on an unbanked host format). The reversal is recorded at `:7886`; the C2 paragraph was never rewritten.
6. **`:9600-9602`** — "modelled 99.59% / 99.56%, capture 100.00%" is stale; the bank reads **100.0 / 100.0 / 99.9681 / 99.9681**.
7. **`:9584-9590`** — the lowerable floors are **three re-bankings** stale (`MCC_RIR_LOW_EXCLUDE`, the leaf graft, and the fourth re-bank).
8. **`:9624-9626`** — "do not turn `-fno-replay-fallback` on by default… ≥4 fallback bodies are genuinely wrong" is contradicted by the **newer** decision at `:9124-9154` (no known defect blocks it; suite green; `nofb_miscompiles` empty). The prohibition predates the `union_cast` / `transparent_union` / `chained_assign` fixes and has no subject.
9. **`:3116`** — "Five of the eight landed" over a list that holds **nine** items (0–7 plus 6a).
10. **`:3597`** — "`levelbench.tsv:47` is now line 51". The file is **29 lines / 16 data rows**; neither line exists. The same applies to every "24 of 47" / "32 of the 47" count in hazard 2.
11. **`:353-357`** — "turns a lavapipe/NVIDIA denormal disagreement into a hard CI failure no code change can fix" is refuted at `:404-408` and never struck; `:439` still lists the retracted reason as load-bearing in the recommendation.
12. ~~**`docs/PLAN.md:625`** marks E6 **"NEW 2026-08-08, OPEN"** while `PLAN.md:1035` and `:6238` of this file both say E6 is closed permanently. It is also the line carrying the uncited lavapipe assertion.~~ **CLOSED on `wt/docsync`**: the E6 row now reads *"CLOSED 2026-08-09 — REFUSE permanently"*, carries the Mesa citation inline, and points at the closure below it; `PLAN.md`'s I2 row records that the `shaderFloat64` floor rests on a *source* reading of lavapipe, not a device one. Still open, same class: `PLAN.md:498/511` "Vulkan — LIVE" vs `:1052` "CLOSED"; `PLAN.md:754` "half-landed" vs `:1060` "CLOSED"; `PLAN.md:1011` N13 open vs `:1061` N13 closed. Those three are status contradictions in prose, which `docs/refs` cannot see — it checks that a citation resolves, not that two sentences agree.
13. **`:10011` duplicates `:10039`** (32-byte vector alignment), **`:9282` duplicates `:10076`**, **`:9759` duplicates `:10078`**, **`:10051`** sends a reader at a capture path that measures **100.000%**, and **`:10182`** still lists `__builtin_powi`/`powif` as missing after `:9303` closed them.
14. **E1's refusal-site count has now been stated three incompatible ways** and every one was written as current: `:7810` "**eight** separate sites", corrected at `:7851` to "**16**, not 8" with the sites enumerated, corrected again at `:100-113` to "**36 lines, 43 occurrences**, not the 1 + 4 + 6 = 11 this paragraph claims". Only the last is right — verified this sweep: `slice_inline.h:2`, `mccslice.h:4`, `mccgpu.h:12`, `ast_eval_slice.h:18`, because `mccgpu.h`'s block is mirrored across the two emitters and `ast_eval_slice.h`'s eighteen were never counted. `PLAN.md:620` still carries the "16 refusal sites, not 8" figure.
15. ~~**Seven `TODO` markers in the tree name sections of this file that do not exist**~~ **CLOSED on `wt/docsync`, and the item as filed was itself wrong on two counts.** It said "seven markers in `tests/`"; there are **five markers in four files** under `tests/`, and the count of seven only reaches seven by including two markers in `tools/` and by counting `macro-nesting.cmake`'s two separately. It also cited `tests/superopt/promote-floor.sh:39`; the marker is at **`:41`**. What each named and what was done: `tests/cst/macro-nesting.cmake` and `tests/cst/symref-shadow.cmake` cited `'CST slice-J'` / `'CST slice-I'`, which **never lived in this file** — they were sections of `docs/NOTES.md`, deleted wholesale at `bb2469bd` and not migrated, so the boundary each test pins is now stated in the test's own failure message and the dead pointer says where it went; `tests/jit/run-parity.sh` cited a "TODO KGC section" purged at `4ab363ce`, and now names `MCC_JIT_NEARMATCH` and `src/mccjit_embed.c` directly, with a note that the nearest live prose does **not** cover near-match parity; `tests/superopt/promote-floor.sh` cited `'Floor the search'`, pruned at `71f3330b`, and now names `so_unsetenv_axis` / `MCC_SO_PROMOTE_FLOOR` in `src/mcc.c`; `tools/embed-jit-smoke.py` cited "P0 step 5" and a "gcc-engine startup residual", **neither of which any row of this file owns**, and now says so rather than implying they are tracked. The three that pointed at the wrong row — `tools/fmt-census.py`, `tests/optfire/levelpins.txt` and `cmake/slicerun_census.cmake` — now cite section headings or the tool to re-run, not board ordinals, because board ordinals renumber.

### The lavapipe citation, now sourced — COPIED ACROSS on `wt/docsync`

This was filed further down this file and deferred to *"whoever owns E6"*. Nobody owned E6,
so it did not move for two sweeps; it has now been copied into `docs/PLAN.md` without
waiting for an owner. It was **independently re-verified during the `wt/sweep` pass** against
`/var/cache/distfiles/mesa-26.0.8.tar.xz`, at exactly the cited lines:

```
src/gallium/frontends/lavapipe/lvp_device.c:454   .shaderFloat64 = (pdevice->pscreen->caps.doubles == 1),
src/gallium/drivers/llvmpipe/lp_screen.c:301      caps->doubles = true;
```

`docs/PLAN.md`'s E6 row used to carry the bare assertion *"Vulkan's `shaderFloat64` is
optional but present on both the NVIDIA host and lavapipe"*, and PLAN's I2 row (the
`shaderFloat64 = TRUE` refusal floor, "see E6") rested on it. **Both now carry the two
lines above and the fact that lavapipe's half is a source reading, not a device reading.**
Two residues survive that and are genuinely open:

- **`ci.yml:116` does not pin the Mesa version** (`libvulkan-dev mesa-vulkan-drivers`, confirmed), so *"CI's lavapipe"* remains an inference from *"26.0.x lavapipe"*. Pinning it is one line.
- **A *runtime* fp64 denormal reading from lavapipe** has never been taken. Both devices advertise neither fp64 denorm mode, which makes them consistent on paper and says nothing about what either computes. There is no lavapipe ICD on this host (`/usr/share/vulkan/icd.d/` holds `nvidia_icd.json` only).

### Settled — do not reopen without new evidence

Each of these was measured, declined, and re-read during this sweep. The measurement is in
the file beside the row.

Chain-store re-promotion (**+2.60** points of `kept` bought **−0.079%** stage-2 for
**+1.50%** stage-1, 19:1 against) · `storeval-rot` demotion (off-state is an incomplete
replay path; `kept` **91.978 → 83.242**) · `narrow` (**100:1**) and `tree-copy-prop`
(**166:1**) · Metal (**1754** MSL lines against **3612** SPIR-V, a 2-line kernel arm, **0**
`msl_region*` symbols against **31** `spv_*`) · the device-path freeze (**79.21** of the
corpus's **80.66** parallel-legal points are `double`; **1.45** are integer) · float in the
emitter (priced at ~1,100–1,700 lines; `float` is unreachable bit-exactly and worth
**+0.0**, `double` is reachable and worth **+79.2** over a corpus that cannot reach either
divergence class) · per-region keep/restore (**4.0%** of 129,861 regions clean at both
boundaries; `ast_replay_body` has no entry point below the body) · frame-local base
distinctness (**0.72%** self-compile, **absent** on the corpus) · `revargs` (21 nodes, one
body, evaluation order only) · `%p` on the device (glibc prints `(nil)`) ·
`__builtin_object_size` subobject-from-declared-array-type (reverted, regressed 5 tests;
`-1` is the safe answer).

I found no decision on that list that looks wrongly settled.

## Landed — the docs cite the tree, and now something checks it, 2026-08-09 (`wt/docsync`)

Nothing in this tree had ever opened a file under `docs/`. Every citation in every board
was unverified prose, and the class had already produced defects nobody had noticed.

**The sweep, run as `tools/docref-lint.py` over the pre-`wt/docsync` docs.** Live boards
(`PLAN.md`, `TODO.md`, `DEVICE-LIBC.md`, `README.md`): **1,227 checkable citations — 796
in-tree paths, 372 line anchors, 59 symbol-at-a-location claims — of which 7 dangle. A
99.43% hit rate**, plus the failed-to-reproduce table's count, which was stated three
ways. Including `ARCHIVED.md`: **1,912 citations (1,198 / 591 / 123), 20 dangling,
98.95%.** Read the right way round, that number is not reassuring: the documentation is
overwhelmingly *decorative* — of 372 line anchors the tool can prove only that 371 land
inside a file that exists, never that they land on the right line — and the 0.57% that was
load-bearing enough to break is exactly the part somebody had acted on.

**Four defects, not stale figures, found by the sweep and fixed:**

1. `PLAN.md` named `MCC_MAX_UNARY_DEPTH 2048` at what was then line 241 of `src/mccgen.c`.
   The symbol was deleted by `wt/unarydepth`, one guard replaced by
   `MCC_MAX_PARSE_DEPTH 512` charged at eight parser entry points.
   The **peak-11 measurement beside it was taken against the old budget and
   is marked stale rather than re-taken** — re-running it is a measurement, not a
   reconciliation.
2. **`README.md` advertised four `-DMCC_CONFIG_*` build options the build has not had
   since `a55c0a07`** — `MCC_CONFIG_ASM`, `MCC_CONFIG_OPTIMIZER`, `MCC_CONFIG_LSP`,
   `MCC_CONFIG_DIAG_RT`. This is the loudest thing in the sweep and it is **wrong, not
   stale**: passing one leaves a silently ignored cache entry, and `tools/ckretired.c`
   already treats the family as retired. Removed, with the runtime successors named.
3. `tests/must-run.txt` attributed the `552 of 12,957` loop-slice figure to the
   `slice-census` cell, which runs `--corpus wide` — a corpus whose `t=0` slice count is
   **81,615**. **Wrong, not stale**; see the `wide` collision above.
4. The `+137` anchor-drift rule at the head of this file was **wrong in both of its own
   worked examples**, and there is no single offset. Corrected in place with the two
   measured clusters.

Plus one dead line anchor (`levelbench.tsv:47`, a 30-line file), the three-way count of
the failed-to-reproduce table, E6 marked OPEN in `PLAN.md` while two other places said
closed, the lavapipe citation finally copied across, and ten `TODO` markers in `tests/`,
`tools/` and `cmake/` — seven naming doc sections that do not exist, three naming board
rows that have renumbered — of which the item filing them was itself wrong about the
count and about one line number.

**The gate.** `docs/refs` (`tools/docref-lint.py`) + `docs/refs-known-positive`, both
`must-run`. Four rules, each scoped so a design doc naming its own unbuilt work cannot trip
it: a rooted path must exist; a `file:line` must land inside that file; a
project-namespaced symbol quoted *beside a `file:line`* must occur in that file; and the
failed-to-reproduce table's row count must equal the one sentence that states it. Run
against the pre-`wt/docsync` docs it reports all eight, defect 1 included, by name.
`tools/docref-allow.txt` holds the five citations that must **not** resolve, each with its
reason, and an entry that starts resolving fails the cell.

`ARCHIVED.md` is out of the gate's scope by its own header — it is a snapshot kept for
later validation, and most of what it cites was deleted after the snapshot was taken.
Its 13 dangling citations are real and are listed by `--include-archived`; demanding they
resolve would demand that history be rewritten.
## Landed — `ast_eval_slice` disagrees with C, and only a cross oracle could see it, 2026-08-09 (`wt/gpuconform`)

### The defect

`ast_eval_slice` takes a binary operator's working type from **child 0 alone**
(`src/ast_eval_slice.h:790`) and never consults the second operand, so C's usual
arithmetic conversions are not applied. Minimal case:

```c
int f(int x) { return x < 4ULL; }   /* f(-3) */
```

gcc, clang, **and mcc's own generated code** all answer 0. `ast_eval_slice` answers 1: it
derives `is64=0, uns=0` from the `int` operand and does a signed 32-bit compare, where C
converts the `int` to `unsigned long long` first. The same rule is mirrored in both device
emitters (`spv_expr`, `msl_expr`), so the CPU-versus-device differential that every other
`slice/*` cell runs **cannot see this** — the two runners are wrong in exactly the same way
because they are two readings of one model. Real shape it came from: `i < LEN(array)` where
`LEN` is `sizeof(a)/sizeof(a[0])`, i.e. the ordinary signed/unsigned comparison.

This is not a miscompile of shipped code — `ast_eval_slice` has no codegen caller — but it
is a wrong answer from the thing the device path exists to substitute for, and it would
become a miscompile the moment a frame kernel were dispatched for real.

Blast radius, measured with `--cref-all`: **13,981 of 39,209** gcc-torture slices (35.7%),
**2,471 of 6,603** llvm-test-suite Regression/C slices, **265 of 6,545** UnitTests slices
contain at least one operator pair the rule mishandles. Every disagreement found across all
four corpora falls in this one family — classified, there is no residue:

| sub-case | Regression/C fragments | example |
| --- | ---: | --- |
| mixed signedness **and** width | 139 | `(int)e0 < (unsigned long long)2` |
| mixed signedness | 18 | `(int)e0 * 2 + 4 - (unsigned int)8` |
| mixed width | 10 | `(unsigned int)e0 - (unsigned long long)32` |
| narrow unsigned operand (C promotes to `int`, the evaluator does not) | 6 | `(unsigned char)e0 - (unsigned char)e1` |

With those slices excluded, the differential is **clean over the entire gcc torture corpus**:
170/170 batches, **201,383 tuples**, under gcc and clang at `-O0` and `-O2`. That is the
guarantee `slice/cref-oracle` now enforces; the excluded set is the debt.

### Two device divergences, same root cause, that no cell in the tree could reach

The habit of deriving a type from **one** child rather than from the operand pair also puts
the CPU and the device into disagreement — which `slice/real` asserts cannot happen. Both
were found by external corpora on a real device; the tree's own `tests/exec` reaches
neither.

1. **A ternary whose arms differ in signedness.**
   `llvm-test-suite/SingleSource/UnitTests/SignlessTypes/div.c`, the `X == Y ? 0 : Y` shape
   with `unsigned X, Y`. `ast_eval_slice_wtype`'s `AST_If` arm takes child 1's type, which is
   the `int` literal `0`, so the device narrows the result to **signed** int while the CPU
   returns the taken arm fitted to its own **unsigned** type: `cpu=4294967293 gpu=-3`,
   3 tuples of 8.
2. **A `_Bool` frame return.** `gcc.c-torture/execute/20230510-1.c`, `ret cpu=9/1 gpu=1/1` —
   one runner fits the return to the declared type and the other does not. One occurrence in
   32,867 compared frame runs.

### What was added

`slicerun --cref DIR` re-emits every accepted expression slice as standalone C — a Ref
becomes a read of a variable of the Ref's own declared type, a Binary becomes the C operator
with C's own conversions — and bakes the CPU reference's answer in as the expected value, so
gcc and clang can adjudicate. Only tuples the reference calls *defined* are compared, which
is exactly the set where the emitted program has no undefined behaviour to hit. The four
operators with no natural C spelling (`TOK_SHR`, `TOK_SAR`, `TOK_UDIV`, `TOK_UMOD`) are
transcribed instead, output narrow included — a half-transcription would charge the
evaluator for a conversion the emitter skipped, which is how the first draft produced 38
false positives.

`--cref` emits only slices where every binary operator already has both operands at one
working type, so the cell asserts a real guarantee rather than banking one number over a
mixed population; `--cref-all` lifts that and is how the defect above is measured.
`--mutate` extends to the emitted C, so one flag arms the known-positive on both arms.

`slicerun --refusals` attributes every refused node to the first guard that rejects it. The
predicates carry no reason channel — `mcc_slice_work_from_ast` has 5 bare `return 0` sites
and the frame path 42 — so this re-decides each node from `tools/` rather than editing
`src/mccslice.h` and `src/ast_eval_slice.h`, which other work is in. It will drift if those
guards change; the check against that is the accepted/refused totals, which come from the
real predicate.

`tools/gpuconform.py` drives an external corpus end to end and `cmake/gpuconform_cref.cmake`
lands it as `slice/cref-oracle` over the checked-in fixtures in `tests/gpu/cref/`, plus four
`slice/cref-oracle-*` corpus cells that skip with a stated reason when `vendor/` has no
checkout. A program counts only when the cross oracle and the suite's own compiler agree at
both `-O0` and `-O2`, with stdout hashed into the verdict so a nondeterministic program
disagrees with itself and is classified out rather than counted.

### The funnel, with a count at every stage

All figures measured on `wt/gpuconform` off `d67f16b5`, **before `wt/fpwidth` lands
`double`** — a funnel taken after that is not comparable to this one. Device present
(NVIDIA RTX 5070 Ti, Vulkan), so no stage is skipped. Corpus revisions:
gcc `9d8f85ca3335` (`basepoints/gcc-17-2762-g9d8f85ca333`), llvm-test-suite `63a9fd93580b`,
llvm-project `0f1f456263b5` (`llvmorg-23-init-21709-g0f1f456263b5`).

| stage | gcc torture | lts Regression/C | lts UnitTests | compiler-rt Unit |
| --- | ---: | ---: | ---: | ---: |
| programs in corpus | 1693 | 1745 | 671 | 226 |
| **classified out** (oracles disagree or both reject) | **200** | **866** | **521** | **81** |
| adjudicated (both oracles pass, `-O0` and `-O2`) | 1493 | 879 | 150 | 145 |
| mcc accepts the source | 1483 | 871 | 147 | 34 |
| …produced ≥1 recorded body | 1479 | 871 | 147 | 34 |
| …produced ≥1 expression slice | **986** | **582** | **80** | **3** |
| …lowered ≥1 slice to SPIR-V | 986 | 582 | 80 | 3 |
| …dispatched to the device | 1479 | 871 | 147 | 34 |
| slices / tuples compared | 39,209 / 313,672 | 5,270 / 42,160 | 653 / 5,224 | 8 / 64 |
| device dispatches | 73,555 | 7,339 | 959 | 43 |
| frame accepted / built / **compared** | 33,226 / 32,867 / **32,867** | 1,422 / 1,198 / **1,198** | 332 / 159 / **159** | 10 / 1 / **1** |
| device-vs-CPU disagreements | **1** (frame) | 0 | **1** (expression) | 0 |

No stage collapses to zero on the two large corpora. `compiler-rt/test/builtins/Unit` is the
exception and is reported as such: 3 programs of 226 produce a slice at all, because 105 of
its 111 mcc rejections were the harness not handing mcc the corpus's `-I` paths — a
front-end/header bucket, not a slice-engine refusal. That is fixed in `tools/gpuconform.py`;
the row above is the pre-fix measurement and should be retaken.

`classified out` is the number that decides whether the harness is vacuous. On gcc torture
it is 194 programs where the two oracles disagree with each other (overwhelmingly `-O0` vs
`-O2` under signed-overflow UB) plus 5 that neither compiles and 1 that aborts under all
four; on the llvm sets it is dominated by programs that need a build harness this driver
does not provide.

### Where the slice engine refuses

Attributed over the whole gcc torture corpus: 15,923 bodies, 1,172,443 nodes, of which
**373,780 (31.9%) are accepted** and 798,663 refused. 119,363 basic blocks, of which
**33,349 (27.9%)** are accepted by the frame predicate. Node-share, then share of bodies in
which the cause appears at all:

| cause | nodes | node-share | body-share |
| --- | ---: | ---: | ---: |
| `ref-not-local` (a global, not a frame slot) | 141,027 | 12.03% | 90.17% |
| `child-refused` (node itself fine) | 134,199 | 11.45% | 81.86% |
| `kind-basicblock` (statement boundary) | 119,363 | 10.18% | 100.00% |
| `op-unary` (unary op outside `- ~ !`) | 107,643 | 9.18% | 81.71% |
| `arity` | 71,016 | 6.06% | 78.61% |
| `kind-invoke` | 67,119 | 5.72% | 83.33% |
| `load-not-allowed` (`allow_load=0` for expression slices) | 47,818 | 4.08% | 73.55% |
| `op-ternary` | 32,435 | 2.77% | 70.00% |
| `no-working-type` | 32,385 | 2.76% | 67.04% |
| `kind-store` | 28,284 | 2.41% | 85.29% |
| **`type-float`** | **5,654** | **0.48%** | **1.88%** |
| `type-nonint` | 4,795 | 0.41% | 5.64% |
| `kind-return` | 4,696 | 0.40% | 24.83% |
| `kind-jump` / `type-bad` / `kind-storeval` / `op-binary` | 1,244 / 676 / 308 / 1 | <0.11% each | — |

### Open, and ranked

1. **Fix the working-type derivation.** `ast_eval_slice_rec`'s `AST_Binary` arm should apply
   the usual arithmetic conversions across the operand pair instead of taking child 0's type.
   Both emitters need the same change or the differential flips from silent agreement to
   loud disagreement. The `--cref-all` arm is the regression test and is already wired.
2. **The `_Bool` frame return divergence.** One case, reproducible, and it is a genuine
   CPU-versus-device disagreement of the kind `slice/real` asserts cannot happen.
3. **`is_float` is not the binding constraint it is filed as.** Measured on real
   application-shaped C it is **0.48% of refused nodes and 1.88% of bodies**; the structural
   refusals — globals, invokes, statement boundaries — are twenty times larger. The `double`
   work on `wt/fpwidth` is worth having, but it will not move the funnel much.

## Landed — the census label arms its own cells, and `slice-census`'s corpus is the goldens table, 2026-08-09 (`wt/censusfix`)

Closes open rows 1 and 2 and filed item 3 above. Three separate defects were stacked behind
one red cell, and only one of them was in the census tool.

### The 9 `src_fail` sources were the corpus, not the compiler

Named, with the cause of each, at `-O0`/`-O1`/`-O2`/`-O3` alike — the set does not vary by
level:

| source | what it wanted | class |
| --- | --- | --- |
| `tests/exec/lexical/trigraphs.c` | `-trigraphs` | flags its golden already declares |
| `tests/exec/types/std_gated_keywords.c` | `-std=c2y -pedantic-errors` | same |
| `tests/exec/functions_abi/gnu89_extern_inline_redef.c` | `-std=gnu89` | same |
| `tests/exec/features_c99_c11/builtins_extra.c` | `-Itests/support` (for `vlog.h`) | include path the runner passes and the census did not |
| `tests/exec/arch/arm64_encoding.c` | `req: cpu=arm64,asm` | not buildable on this target |
| `tests/exec/arch/arm64_extasm.c` | `req: cpu=arm64,asm` | same |
| `tests/exec/arch/winarm64_interlocked.c` | `req: cpu=arm64,os=WIN32` | same |
| `tests/exec/functions_abi/fastcall.c` | `req: cpu=i386` | same |
| `tests/exec/pointers_arrays/array_assignment.c` | `req: note:whole-array assignment (b = a) is invalid C; mcc rejects it like gcc/clang` | rejected on purpose |

**The `src/` fragment trap was checked and is not the explanation.** `slice-census.py`'s
`wide` corpus is `src/mcc.c` plus a walk of `tests/exec`, `tests/behavior`, `tests/ast` and
`examples` — it never walks `src/`, so `mccast.c`, `mccircap.c` and `mccrir.c` were never in
it and none of the 9 is a fragment. The 9-vs-3 coincidence was a coincidence.

### Two corpora were both called `wide` — RESOLVED by disclosure, not by renaming, `wt/docsync`

`rir-coverage.py`'s `wide` is a **different** set from `slice-census.py`'s: it also walks
`tests/asm`, `tests/runtime` and `tests/static`, so it is 380 walked sources against
slice-census's 359 walked / **346 declared**, and it is differently *constructed* as well —
slice-census filters and re-flags through `tests/exec/goldens.h` and hard-fails on a
compile error, rir-coverage compiles all 380 with bare `mcc -c` and silently drops the ~9
that fail. They are not two sizes of one population.

**Neither was renamed, and the reason is that renaming was the more expensive wrong fix.**
`rir-coverage`'s `wide` is a bank key in `tests/rir/coverage-bank.json` and the corpus
selector of the `rir-coverage-census` cell; renaming it edits a ratchet that is not this
branch's, against a 0.05-point tolerance across four levels. Renaming *slice-census*'s
instead is free — it banks nothing — but it would not fix the actual defect, which is that
**a figure could not be attributed to a corpus at all**: neither tool printed which corpus
it had walked, so a pasted log carried no provenance and two `wide` figures could be
compared silently. Both tools now print `corpus=…` **with its membership spelled out** on
every report header (`wide[slice-census] = …` / `wide[rir-coverage] = …`) and in
slice-census's `--quiet` one-liner, alongside the source count. That is the cheap correct
fix: the name stays ambiguous, the output does not.

**The collision already produced one wrong attribution, and it is a wrong claim, not a
stale one.** The `552 of 12,957` loop-slice figure is recorded below as
`--corpus self`, and `tests/must-run.txt` attributed the same figure to the `slice-census`
cell — which runs `--corpus wide`, whose `t=0` slice count is **81,615**, not 12,957. The
manifest row has been corrected; the figure itself is a `self` number and is unchanged.

**The fix is that `tests/exec/goldens.h` is the authority on what a `tests/exec` source is.**
It is the same table `tests/runner.c` executes. `slice-census.py` now reads it: it passes
each source's declared `flags` (minus `-l`/`-L`, which `mcc -c` refuses outright —
`mcc: error: cannot specify libraries with -c` — and which affect linking, not translation),
adds the runner's own `-Iruntime/include -Itests/support`, and honours `req` through a port
of `req_met()` restricted to the clauses that decide whether a *compile* can happen. The
run-time clauses (`asm`, `bcheck`, `backtrace`, `diff3!=`) are deliberately **not** honoured:
this census never runs the program, so dropping a source over them would shrink the
denominator for no reason. Every source goldens.h excludes is **named in the report**, with
its reason, so the denominator stays auditable. A source that fails to compile is still a
hard failure — the `src_fail` gate was not touched, and it now names the offenders.

### The 2 `--verify` overruns were the denominator, not the attribution

`cst_alloc_node: slice bytes 595/595 exceed body bytes 560` and `rir_low_set: 345/307`. Both
bodies have **`faithful=0`**, and that is the whole of it. `bytes=` is `body_len`, the length
the *parser* emitted; the per-statement extents are measured on the *replay*. Equal length is
a precondition of equal content, so for a faithful body the two are the same number — but an
unfaithful body is under no obligation to emit the parser's byte count, and neither of these
two does. `RIRPRODDUMP=cst_alloc_node` prints it directly:

```
[ast-postreplay] cst_alloc_node loc=-16 saved_loc=-20 newlen=616 bodylen=560
[ast-postreplay] rir_low_set     loc=-32 saved_loc=-48 newlen=345 bodylen=307
```

Different frame layout, more bytes, and the attribution was charged the difference. The
`[slice-fn]` record now carries **`rbytes=`**, the replay's own length, `verify_fn()` measures
slice extents against it, and the check got *stronger* rather than weaker in three ways: a
faithful body whose `rbytes != bytes` is now an error (that identity used to be assumed), the
statement attribution is now checked against `rbytes` as well as the slice extents, and the
report prints `body bytes N (replayed M, +x%)` so the drift is on the page instead of hidden
in a ratio. With the right denominator, statement attribution reads **100.000% of replayed at
every level** — it was never approximate; it was being divided by the wrong number.

Ablated, all three cells fail as claimed:

```
slice-census: -O2: 2 of 3 sources did not compile … tests/exec/arch/arm64_encoding.c
  (unknown opcode 'mrs'); tests/exec/functions_abi/fastcall.c (bad operand with opcode 'pushl')
slice-census: -O1 cst_alloc_node: slice bytes 595/595 exceed replayed body bytes 560
  (parser 560) by more than 32                     [rbytes reverted to body_len]
slice-census: -O0 host_stderr_isatty: faithful body replayed 14 bytes against a 13-byte
  parser body                                      [rbytes perturbed by +1]
census-armed: slice-census: slice-census.py gates it on MCC_SLICE_CENSUS_RUN, and the cell's
  ENVIRONMENT does not set it. The cell will report Skipped and ctest will count it as passed
```

### Both readings, and which one the board should quote

Same compiler, same tool, same four levels; the only difference is the corpus and the
denominator. **Quote the right-hand column.** The left-hand one is an average over the 350
sources that happened to survive out of 359 attempted, which is not a defined population.

| | old: 359 walked, **9 failing**, shares over the survivors | new: 346 declared, **0 failing** |
| --- | --- | --- |
| sources | 359 (failed 9) | 346 (failed 0), 13 excluded by `goldens.h` and named |
| modelled bodies `-O0` | 4321 (faithful 4192) | 4196 (faithful 4067) |
| modelled bodies `-O1`/`-O2`/`-O3` | 4328 (faithful 4248/4251/4251) | 4202 (faithful 4122/4125/4125) |
| body bytes `-O1`..`-O3` | 3,563,866 | 3,542,417 (replayed 3,543,317, **+0.025%**) |
| attribution | 100.025% *of parser body bytes* | **100.000% of replayed** |
| `t=0` slices `-O1`..`-O3` | 81,738 | **81,615** |
| `t=0` bytes `-O1`..`-O3` | 1,487,094 B (41.73%) | **1,483,262 B (41.87%)** |
| `t=1` bytes `-O2`/`-O3` | 1,587,674 B (44.55%) | **1,583,924 B (44.71%)** |

**The number barely moved, and that is the finding, not an excuse.** The 9 sources were tiny;
losing them cost 0.15 points of the byte share. A gate that fires on a defect worth 0.15
points is a gate working correctly — the alternative reading, the one this cell had before
`7c5c736e` added `src_fail`, is that 11 of 12 sources could have dropped out and the cell
would still have passed on the twelfth.

**13 excluded, not 9 — and 8 of the 13 do compile here.** Only 5 of the 13 were among the 9
failures. The other 8 were verified by hand to build cleanly on `x86_64/Linux` and are
dropped anyway: five carry a `note:` (`inline.c`, `alias.c`, `backtrace.c`, `btdll.c`,
`stdcountof_header.c` — documentation rows whose coverage lives in a named cell elsewhere),
and three are arch goldens whose C body happens to be portable (`arm64.c`, `arm64_errors.c`
under `cpu=arm64`, `riscv_asm.c` under `cpu=riscv64`).

That cost is deliberate, and it is the one judgement call in this branch. The corpus is now
*"the goldens `tests/runner.c` runs on this target"* — one sentence, mechanically checkable,
and derived from a table nobody maintains with this census in mind. The alternative, *"the
goldens this target runs, plus the eight that happen to build"*, is decided by outcome, which
is the gate-lowering move this cell was red for catching, and it is a list that rots.
`array_assignment.c` is the proof that the weaker rule is unsafe: it carries a `note:` and it
does **not** compile, so `note:` cannot be assumed to mean *"compilable, just not run"*. The
8 are worth 1.4% of the corpus and are named on stdout on every run, so anyone who wants them
back can see exactly what they would be arguing for.

### The label arms its own cells

`grep -c 'MCC_SLICE_CENSUS_RUN\|MCC_LOOP_CENSUS_RUN\|MCC_RIR_CENSUS' CMakeLists.txt` was
**0**; it is **5** now, one `ENVIRONMENT` property per census cell. `MCC_RIR_CENSUS` was
*not* wired the way the recipes in this file assume — it was in exactly the same state as the
two undocumented switches, and `rir-coverage-census`/`rir-nofb-probe` ran only because the
recipe told people to export it by hand.

Always-on rather than opt-in, because the cost is not the reason they were gated:
`slice-census` **7.9 s**, `loop-census` **3.1 s**, `loop-census-numeric` **7.9 s**,
`rir-coverage-census` **~24 s**, `rir-nofb-probe` **14.9 s** — 40 s of wall clock added to a
full run, `ctest -L census` **98 s** end to end. The `--opt-in` flags stay in the tools,
where they do their real job of warning a human before a hand-typed run.

`tools/census-armed.py` (cell `census/gates-armed`, `must-run`) closes the loop structurally
rather than by list: for every `census`-labelled cell that passes `--opt-in` it reads the tool
named on the command line, recovers the variable that tool's own guard consults, and requires
the cell's `ENVIRONMENT` to set it to something other than empty or `0`. A census cell added
tomorrow with a new switch is caught by the same rule; a switch renamed in the tool but not in
`CMakeLists.txt` is caught as a mismatch instead of as a silent skip. It never returns 77 —
a cell whose job is to make a skip visible must not be able to skip.

`slice-census` is promoted `registered` → **`must-run`** in `tests/must-run.txt`: its opt-in
guard is now its only `return 77`, so a Skipped there can only mean the arming was lost.
`loop-census`, `loop-census-numeric`, `rir-coverage-census` and `rir-nofb-probe` are left at
`registered` on purpose — they have other, legitimate 77 paths (7 and 4 of them respectively)
that are properties of the host, and promoting them would be a claim about hosts this branch
did not measure. Worth someone's time to audit those paths and promote what deserves it.

### Verified

`cmake-cross` built before `cmake-debug` was configured (hazard 5), `vendor/` symlinked from
the primary checkout. `ctest -N` registers **9156** — the 9155 baseline plus
`census/gates-armed`, the only cell this branch adds.

Full `ctest -j16`: **9156 cells, 0 failures**. `ctest -L flagsweep` **119/0**,
`-L stratsweep` **30/0**, `ctest -L census` **7/0 with nothing Skipped** (and identically
7/0 under the old `MCC_RIR_CENSUS=1` recipe, which is now redundant rather than wrong),
`python3 tools/must-run.py --build cmake-debug` **65 row(s) satisfied**, and
`python3 tools/selfhost-smoke.py cmake-debug` OK from the repo root.
`tests/optfire/*` untouched.
## `ast/rir-c2-*` — the fourteen bodies, named and attributed, 2026-08-09 (`wt/ric2`)

The previous sweep turned `MCC_REPLAY_IR_C2` into a real `mcc_config_node`, measured
**gap 14** at `-O1` / **13** at `-O2`/`-O3` against a banked `0`, and deliberately left both
the default and the bank alone. This section names all fourteen. **They are not fourteen
bugs. One was, and it is fixed; the other thirteen are three causes, and twelve of them are
fixtures that did not exist when `BANKGAP=0` was taken.**

Measured with `-DMCC_REPLAY_IR_C2=ON` on x86_64 Linux, corpus `tests/exec` +
`tests/diff/full_language.c`, the exact cell in `tests/ast/rir_c2.cmake`. A failing body is
`[rir-c2part] <fn> ok=0`; the per-body dumps are `[rir-c2op]`, `[rir-c2len]`,
`[rir-c2byte]` at `MCC_REPLAY_IR=5`.

| # | body | file | verdict |
| --- | --- | --- | --- |
| 1 | `extend_brk` | `tests/exec/codegen/codeopt.c` | **REGRESSION, FIXED below** |
| 2 | `compile` | `tests/exec/programs/grep.c` | `-O1` only; `storeval-callstore` left level 1 at `1ad3f1aa` |
| 3–13 | `rot_member_chain`, `rot_deep_member`, `rot_array_chain`, `rot_const_left`, `rot_call_arg`, `rot_member_of_member`, `rot_volatile_mid`, `rot_volatile_elem`, `rot_narrow_mid`, `rot_dead_mid`, `rot_impure_target` | `tests/exec/statements/chained_assign.c` | eleven fixtures **added by `99f6afd9`**, the storeval-rot fix, whose own commit message records them as *"inherently discarded by the byte gate"* |
| 14 | `main` | `tests/exec/types/const_member_copy.c` | fixture added by `89003447`; equal length, equivalent encoding |

`BANKGAP=0`/`BANKFN=1149` were pinned at `3395f5f2` and tightened to `1150` at `bc85ce70`.
The population today is `fn=1309`. **Twelve of the fourteen are bodies added after
`bc85ce70`** — rows 3–14 — so comparing a gap over 1309 bodies against a bank taken over
1149 is the same apples-to-oranges the `BANKFN` floor exists to catch, pointing the other
way. Both regressions (rows 1 and 2) were found by `git bisect run` over the 450 commits
`bc85ce70..HEAD`, rebuilding with `-DCMAKE_C_FLAGS=-DMCC_REPLAY_IR_C2=1` at each step.

### Row 1 — `__builtin_expect` stopped being code-neutral at `72fedcf1`. FIXED

`72fedcf1` ("register-pointee address-of, C23 one-arg `va_start`, complex `++`/`--`")
changed the second argument of `__builtin_expect` and `__builtin_expect_with_probability`
from `expr_const64()` to `expr_eq(); vpop();`, so that a non-constant argument evaluates
instead of ICE-ing. That is correct, but `expr_const64()` parses under
`nocode_wanted += CONST_WANTED_BIT` and `expr_eq()` does not, and `vsetc()` materialises a
pending `VT_CMP` into a register **only when `nocode_wanted` is 0**. So pushing the second
argument on top of a live comparison started forcing it out:

```
  parser: 48 83 f8 00  0f 94 c0  0f b6 c0  83 f8 00  0f 84 ..   cmp/sete/movzx/cmp/je
  rir   : 48 83 f8 00                                0f 85 ..   cmp/jne
```

Nine bytes, at every `-O` level, for every `__builtin_expect` over a comparison. The C2 arm
was reporting it as a re-emit divergence (`want=150 got=141`) because the arena re-emit
folds the comparison straight into the branch, which is what the parser did before
`72fedcf1`.

**Fix:** the two discarded operands parse under `nocode_wanted++`, the same treatment
`72fedcf1` itself gave the third argument of `__builtin_expect_with_probability` and the
c23 `va_start` second argument. `codeopt.c` `.text`: `-O0` 842 → **833**, `-O1` 832 →
**823**. `tests/ast/o0-baseline/*.obj.txt` re-taken on all twelve keys
(`C2_NO_EXTRA=1 O0_AB_BANK=1 tools/o0_ab.sh cmake-cross all …`, then the same with
`O0_AB_GATES=1`): **exactly one row moves per key, and it is `codeopt.c` on all twelve.**
No `.rir.txt` counter moved. Gap 14 → **13** at `-O1`, 13 → **12** at `-O2`/`-O3`.

New cell `cli/builtin_expect_is_code_neutral` pins the invariant directly — the object for
`if (__builtin_expect(!!(g==0),0))` must be byte-identical to the object for `if (!!(g==0))`,
and the same for `__builtin_expect_with_probability`. Ablated (fix reverted, `cmake-debug`
rebuilt):

```
FAIL  builtin_expect_is_code_neutral
  --- expected ---   expect=NEUTRAL / prob=NEUTRAL / END
  --- got ---        expect=COSTS   / prob=COSTS   / END
```

`ast/o0-baseline` ablates on the same revert with
`o0_ab: x86_64 -- an -O0 object moved. The AST recorder does not run at -O0, so nothing in
the cut had any business touching these bytes.`

### Row 2 — `grep.c:compile` is a level-placement consequence, not a defect

`switch (tolower(c = *s++))`. The parser keeps the stored value live in a register
(`mov [rbp-0x1c],ecx` / `mov rdi,rcx`); the replay reloads it (`mov eax,[rbp-0x1c]` /
`mov rdi,rax`), three bytes longer. The live mark comes from `ast_finalize_storevals`,
which is **gated on optimisation flags while the parser's vstack discipline is not**, so
moving a `storeval-*` row off a level necessarily costs replay fidelity at that level.
`1ad3f1aa` moved `storeval-callstore` 1 → 2 (and `chain-store` 1 → 3, `chain-store-live`
1 → 2, `chain-store-member` 1 → 3). Confirmed by single-flag probe: `-O1 -fstoreval-callstore`
takes `grep.c` from `c2ok=13/14` to `c2ok=14/14`, and the body is already clean at `-O2`
and `-O3`. **That is the whole of the `-O1` vs `-O2`/`-O3` difference in the gap.** Nothing
to fix here; it is the ladder decision showing up on a second axis. Worth knowing before
the next level move: `-f<row>` demotions have a replay-fidelity price the ladder benchmarks
do not see.

### Rows 3–13 — the eleven `chained_assign.c` `rot_*` fixtures

They are minimal reproducers for `q->x = c = a = e`, added by `99f6afd9` together with the
fix for the `-O1`/`-O2`/`-O3` `vstack leak (-1)` ICE. They diverge for two reasons, and the
split was measured by ablating each half of that fix behind a `getenv` in a scratch build:

| ablation | bodies still failing | what it proves |
| --- | ---: | --- |
| none (HEAD) | 11 | — |
| revert the `ast_replay_value_inner` reload fallback | 1 | five bodies come from the **reload** arm: `rot_deep_member`, `rot_array_chain`, `rot_const_left`, `rot_call_arg`, `rot_member_of_member` |
| revert the `ast_finalize_storevals` reload-ok guard | 6 | five bodies come from the **declined live mark**: `rot_volatile_mid`, `rot_volatile_elem`, `rot_narrow_mid`, `rot_dead_mid`, `rot_impure_target` |
| both | 1 | `rot_member_chain` is a third cause |

**Both halves are load-bearing and neither may be removed.** Compiling
`chained_assign.c` with either ablation, at `-O1`/`-O2`/`-O3`/`-O11`: the reload ablation
fails to compile (the `vstack leak` the fix was for), the guard ablation **segfaults the
compiler**. HEAD compiles all four levels and matches `gcc -O0` output.

`rot_member_chain` is the structural one and it names the real limitation. The parser
evaluates the whole chain's lvalues first and the value last (`push &q->x; push &c;
push &a; eval s+a; vstore; vstore; vstore`), so the address load never clobbers the live
value. The arena hoists each store into its own statement, so `Store(q->x, StoreVal)` loads
`q` **after** the value is live, `mov rax,[rbp-24]` clobbers `eax`, and the replay reloads
`c`. `ast_storeval_push_leaf` correctly refuses to grant the rot mark for a non-leaf target
(`q->x` is `Unary deref(Ref q)`), because the push does emit code. Reproducing the parser
needs the outer store's *address* hoisted above the preceding chained statements —
`AST_FB_STORE_CHAIN_MEMBER` in `ast_replay_bb` already does exactly that for a two-store
chain, and extending it to a 3-deep chain through a non-leaf target is the open item.
**Not attempted here**; it is a replay-shape change with ICE risk and it wants its own
branch and its own random-program cross-validation, the way `99f6afd9` had.

**This is a production cost, not only a measurement artefact.** `RIRPRODDUMP=<fn>` shows the
*production* AST replay diverging by the identical lengths — `rot_member_chain` 60 → 63,
`rot_volatile_mid` 97 → 109, `rot_deep_member` 119 → 128, `rot_dead_mid` 122 → 128,
`rot_impure_target` 137 → 149 — so these bodies fall back to the parser's bytes and every
optimiser strategy applied to them is discarded. That is the same quantity
`tools/rir-coverage.py` reports as `kept`, and it is why `storeval-rot`'s off-state is
already recorded in this file as *"an incomplete replay path"*. A fixpoint that grants the
live mark whenever the ancestor store is itself live was tried and **changes nothing**: the
outer `Store(q->x, StoreVal)` is a discarded statement, so it never carries the mark for an
inner store to inherit.

### Row 14 — `const_member_copy.c:main`, the one `bytes=1`

`__imag__ cp[0]` on a `double _Complex *`. Equal length, different encoding:

```
  parser: 48 8b 00  48 83 c0 08  f3 0f 7e 05 ..  66 0f 2e 00        mov/add rax,8/movq/ucomisd [rax]
  rir   : 48 8b 00              f3 0f 7e 05 ..  66 0f 2e 80 08000000 mov/movq/ucomisd [rax+8]
```

The parser materialises the `+8` into the register; the arena carries the displacement on
the node and the re-emit folds it into the addressing mode. Both are 391 bytes and the same
instruction count. `rir_c2_equiv_proven()` does not prove it (`c2equiv=0 c2unproven=1`), but
proving it would not move the gap — the cell counts `c2try - c2ok`, not `c2unproven`. This
is the arena being *more* canonical than the parser, which is the direction the C2 arm wants;
"fixing" it means either making the parser fold (a broad codegen change with no measured
gain) or making the arena reproduce a redundant `add` (wrong direction). **Legitimate
divergence, correct as it stands.**

### The default: `MCC_REPLAY_IR_C2` stays **OFF**, and the banks stay at `0`

The board today is `gap 13 (bytes=1 len=12)` at `-O1` and `gap 12 (bytes=1 len=11)` at
`-O2`/`-O3`, over `fn=1309 faithful=1274`, `c2skip=0 c2err=0 c2invalid=0`.

- The tree's own recorded gate for flipping it (`docs/ARCHIVED.md`, P4) is a key reaching
  **100% on the `all` corpus**. No key is there, and `exec` alone is `1258/1271` = 99.0%.
- Flipping it on today means either fourteen red cells or re-banking `0 → 13`, and inflating
  a ratchet to match the day's measurement is what the previous agent refused for exactly
  the right reason. Twelve of the thirteen now have a named cause and a commit, which is a
  justification for a *future* re-bank, but a re-bank is only worth taking together with the
  default flip and a re-measurement of all eleven cross keys — which needs a cross build
  with the arm compiled in, and is a separate piece of work.
- The switch is discoverable now (`-DMCC_REPLAY_IR_C2=ON`), the skip message names it, and
  this section is the board it produces. That is the honest state: a ratchet that is red
  when armed, with every red body named, rather than a green one that asserts nothing.

**Open, in the order the measurement ranks them:** (a) hoist the chained store's outer
target address, closing up to eleven bodies and the same eleven `kept` points in production
— the `AST_FB_STORE_CHAIN_MEMBER` generalisation above; (b) decide whether `storeval-*`
demotions should carry their replay-fidelity cost into the ladder's cost model, which would
put row 2 back at `-O1`; (c) then, and only then, flip the default and re-bank all fourteen
cells from measurement.

## The board — re-derived 2026-08-09 against a retaken corpus

Supersedes the "Next, in order" list further down this file, the 2026-08-08 board, and that
board's own 2026-08-09 revision. Nothing here is patched forward: every figure below was
re-taken today with the committed tool named beside it, on this tree, and the ones that did
not come back are listed as failures rather than quietly corrected.

`ctest --test-dir cmake-cross -N` registers **9155** cells on this host and `--test-dir
cmake-debug -N` registers **9155** as well — *but only if `cmake-cross` already exists when
`cmake-debug` is configured*. Configured against an absent `cmake-cross` it registers
**8972**. See hazard 5. (The count was 9136 when this board was derived and 9151 before
`wt/envgate`, which adds four: `ast/o0-baseline-gated` and `build/fragments-are-not-tus`,
each with its known-positive. The 8972 figure has not been re-taken since; it is the
*shape* of hazard 5, not a current reading.)

### The verdict, and why it re-ranks everything

The board has to answer one question honestly, and the previous two versions answered
different halves of it:

> A sound **80.60%** parallel-legal iteration-weighted fraction on a numeric corpus —
> **3.67%** without its hottest loop — against a compiler that has **no batch producer**.
> Does that overturn "freeze the device path"?

**It does not, and the reason is new.** The reason is not the one the last board gave
("there is no lane source"), which the corpus disproved. It is that **the lanes that exist
are in a type neither executor implements**, behind a producer that does not exist.

#### 1. The self-compile is barren, and it is barren structurally

`tools/loop-census.py cmake-debug --top 20`, self-compile of `src/mcc.c` at `-O2`, 2035
loops instrumented / 602 entered / 26,103,304 entries / ~52.15M iterations:

| | |
| --- | ---: |
| raw iteration-weighted fraction at each loop's own break-even | **52.51%** |
| **parallel-legal iteration-weighted fraction** | **0.01%** |
| the same with `--alias-oracle` | **0.01%** |
| iteration share `par=1` / `par=0` / `par=?` | 0.35% / 3.88% / **95.77%** |
| `why= body-unsafe` (the loop calls a function, or uses `asm`/`volatile`) | **50.04%** |
| `why= not-analyzable` (a label or `goto`, or no affine IV) | **33.82%** |
| hottest loop (`ast_strpool_find_or_add`, `mccast.c:3722`) | 11.4% |

**83.86% of every iteration in a self-compile is in a loop that calls a function or
contains a `goto`.** Those are properties of the program. No dependence arithmetic, and no
alias oracle — measured, byte for byte, at zero points — converts them. This half of the
old verdict survives intact and got stronger: it now has a measured *reason* rather than a
hand-read list of twelve array fills.

#### 2. The numeric corpus does have lanes, and the predicate now proves them soundly

`tools/loop-census.py cmake-debug --corpus runtime --top 20`, the 17 in-tree kernels of
`tools/runtime-bench.py`'s `KERNELS` with their argv unchanged, 2,246,355,539 iterations:

| | |
| --- | ---: |
| raw iteration-weighted fraction | **97.76%** |
| **parallel-legal iteration-weighted fraction** | **80.60%** |
| the same with `--alias-oracle` | **80.60%** — the two reports are byte-identical (`diff` empty) |
| with the hottest loop (`matmul.c:22`) removed | **3.67%** |
| with the largest program (`matmul.c`) removed | **3.66%** |
| entered loops `par=1` / `par=0` / `par=?` | 26 / 17 / 34 |

`bases-may-alias-indirect` no longer appears in the corpus histogram **at all** — not
0.00%, the row is absent — because `wt/decaytype` converted the whole of it. That work was
real and it was sound. `-fdep-alias-oracle` is now worth exactly zero here.

#### 3. And the lanes are floating-point, which neither executor can run — NEW, and it decides the row

This is the measurement the last two boards did not take, and it is the one that settles
the question. The corpus's parallel-legal iterations are not distributed; they are three
kernels, and two of them are `double`:

| kernel | share of all corpus iterations | `par=1` share of the program | scalar type of the hot loop |
| --- | ---: | ---: | --- |
| `matmul.c` | 77.07% | 99.83% | **`double`** (`static double a[600][600]`, `c[i][j] += aik * b[k][j]`) |
| `loopnest.c` | 2.29% | 99.23% | **`double`** (same i-k-j nest, `static double a[256][256]`) |
| `vlaloop.c` | 2.15% | 63.68% | `int` (`buf[i] = (buf[i] + t + i) & 0xffff`, 64 trips) |
| the other fourteen | 18.49% | ≤ 0.83% each | mixed |

Splitting the 80.66 points of `par=1` iterations by that column:

| | points of all corpus iterations |
| --- | ---: |
| `par=1` **and** floating-point (`matmul` + `loopnest`) | **79.21** |
| `par=1` **and** integer (`vlaloop` 1.37, everything else 0.08) | **1.45** |

> **SUPERSEDED IN PART, 2026-08-09 (`wt/fpwidth`).** Everything in this section's tables
> still reproduces. What does not survive is the inference drawn from it: the paragraph
> below is right that the engine refused floating point, and wrong that the refusal was what
> stood between 1.45% and 80.60%. `double` now runs on the device and the fraction is
> **still ≈1.45%**, because `matmul` and `loopnest` index `static` arrays of 360,000 and
> 65,536 elements and the live-in model addresses frame locals one slot per element against
> `MCC_SLICE_MAXSLOT = 16`. The `gd.c`/`gi.c` demonstration below now reads
> `slices=2 gpu-slices=2 ... OK` for the `double` body. See the M6 section further down.

**The slice engine and both of its executors refuse floating point by construction.**
`mcc_slice_work_from_ast` rejects any slice whose root type `is_float` (`src/mccslice.h:133`),
and there are four more `is_float` refusals in `src/mccslice.h` and six in `src/mccgpu.h`.
The SPIR-V emitter has no float support to refuse *with*: `grep -c Float src/mccgpu.h
src/mccgpu.c src/mccslice.h` is **0, 0, 0** — no `OpTypeFloat`, no `OpFAdd`, no `OpFMul`,
nowhere in the device layer.

> **CORRECTION 2026-08-09 (`wt/spvfloat`), the "eleven sites" undercount.** The `grep -c
> Float` **0, 0, 0** reproduces exactly, and so does every corpus figure above (80.60 /
> 80.66 / 79.21 / 1.45 — see the pricing section below). **The refusal-site count does
> not.** `grep -c is_float` over the slice engine returns **`slice_inline.h:2`,
> `mccslice.h:4`, `mccgpu.h:12`, `ast_eval_slice.h:18` — 36 lines, 43 occurrences**, not
> the 1 + 4 + 6 = 11 this paragraph claims. Two independent causes, and both of them
> matter for the price:
>
> - **`mccgpu.h` is 12, not 6, because the block is mirrored.** Lines 780/791/803/816/831/898
>   are the MSL arm and 2640/2651/2664/2681/2719/2786 are the SPIR-V arm — the same six
>   guards written twice. Anyone counting one emitter counted half the work.
> - **`ast_eval_slice.h`'s 18 were never counted at all**, and they are the expensive ones:
>   that file is the **CPU reference**, and it is the reason the price is not what this
>   board assumed. See below.

Demonstrated rather than argued, with the committed harness:

```
MCC_ARENA_DUMP=$D cmake-debug/mcc -c gd.c -o /dev/null -O2   # double gd(double x, double y) { return x*y + x - y*3.0; }
cmake-debug/slicerun --arenas $D --limit 400
  → bodies=1 slices=0 tuples=0 gpu-slices=0 ... FAIL (no real slice became schedulable work)

MCC_ARENA_DUMP=$D cmake-debug/mcc -c gi.c -o /dev/null -O2   # int    gi(int x, int y)       { return x*y + x - y*3;   }
cmake-debug/slicerun --arenas $D --limit 400
  → bodies=1 slices=1 tuples=8 gpu-slices=1 ... OK
```

Same shape, same node count, one scalar type apart: the integer body becomes schedulable
work and the `double` body produces **zero slices**. So the honest restatement of the
corpus result is:

> **The device-executable parallel-legal iteration-weighted fraction of the numeric corpus
> is ≈1.45%, not 80.60%**, and 94% of that 1.45 is one 64-trip integer loop inside
> `vlaloop`. 79.21 of the 80.60 points are arithmetic the stack declines before dependence
> analysis is ever consulted.

The 80.60% is a real and sound measurement of what `ast_loop_parallel_legal` can *prove*.
It is not a measurement of what the device can *run*, and the two were being read as one
number.

#### 4. There is still no batch producer, and the two halves have disjoint input domains

Verified by inspection today, not inherited:

- `MccSliceWork` (`src/mccslice.h:15-29`) does run a block over `ntuple` argument tuples:
  `mcc_slice_work_bind` sets `ntuple`, `mcc_slice_run_cpu` loops `while (w->done <
  w->ntuple)` gathering `w->in[done * nlive + j]`, and `mcc_slice_frame_run_gpu` is "one
  dispatch, `ntuple` independent frames".
- **No file in `src/` constructs an `MccSliceWork`.** `src/mccslice.h` has exactly one
  includer in the whole tree, `tools/slicerun.c:30`, and that binary does not link
  `libmcc` (`CMakeLists.txt:3611` — it compiles `tools/slicerun_arena.c` and `src/mccgpu.c`
  and brings its own arena).
- `mcc_slice_frame_from_ast` is defined once (`src/mccslice.h:521`) and called **16 times,
  all in `tools/slicerun.c`**. Zero call sites in `src/`.
- The compiler's only two slice-evaluation sites are `ast_jit_const_fn`
  (`src/mccast.c:16636`) and `ast_jit_fold_consts` (`:16685`). Both pass `NULL` offsets,
  `NULL` values, **`nlive = 0`**, and evaluate **one** tuple. The surrounding loop in the
  second walks *nodes*, not argument tuples.
- And `mcc_slice_work_from_ast` **rejects `cnt < 1`** (`src/mccslice.h:119`). The batching
  machinery refuses precisely the zero-live-in shape the compiler produces, and the
  compiler never produces the shape the batching machinery wants. **The two halves have
  disjoint input domains.** This is stronger than "the caller is missing": the caller
  cannot be written by connecting what exists.

`ast_loop_parallel_legal` — the analysis that would have to nominate the batch — is
declaration `src/mccast.h:218`, definition `src/mccast.c:13891`, and **two call sites, both
`fprintf`** (`ast_loop_par_census:14034`, `ast_loopdep_dump:14053`). It cannot reach
emitted code, which is also why `-fdep-alias-oracle` is safe to ship.

#### The argument for resuming, stated at its strongest

It is not weak and it should be read before the recommendation.

`docs/PLAN.md`'s S5c is explicit that the batch comes from "slices already inside a loop
over independent tuples", supplied by `ast_loopdep` — so `ast_loop_parallel_legal` is not
an unrelated analysis, it *is* the batch producer's prerequisite, and it now works. H6 says
the project's answer arrives when "the break-even column is uniformly above the batch sizes
the loop-nest analyses can supply", and on `matmul 600` the inner loop supplies **600
lanes** against a break-even of 48 at 15 nodes and 24 at 31 — the first time in this
file that a measured batch has cleared a measured bar by an order of magnitude. The JIT is
not a side project either: `MCC_CONFIG_JIT` is on by default and bakes the runtime JIT
"into every program mcc builds (and mcc's own `-run`)", so the target workload is *programs
mcc compiles*, of which the self-compile is one unrepresentative sample and the corpus is
seventeen more. Meanwhile the device path's cost is now **sunk** — predicate, both
executors, emitter, format engine, leaf inliner, region layer, `*p` lowering, all built,
all differentially tested — and it has been the tree's most productive bug-finder: the
`spvgate` differential and the ladder census produced the arena-fidelity diagnosis, the
`for`-increment-before-body bug, and the two miscompiles fixed this month. Freezing a
mechanism that finds miscompiles in order to save effort on a mechanism that is already
built is a strange trade.

#### The recommendation

**Freeze the SPIR-V device path. Do not write the `mcc_slice_frame_from_ast` caller. Drop
Metal.** Keep every existing device cell green as regression cover, and keep the ladder
census (`MCC_AST_EVAL_LADDER_CENSUS`) and `spvgate` running, because those are the parts
that find bugs and neither of them needs a dispatcher.

The grounds, in the order of their weight:

1. **The corpus's lanes are floating-point and the stack has no floating point.** 79.21 of
   80.66 points. Closing that is not a caller — it is `OpTypeFloat`, `OpFAdd`, `OpFMul`,
   `OpFOrd*`, conversions, a CPU reference that matches them bit for bit, and a
   differential that is stable under rounding. Nothing in the tree has any of it, and the
   `%f`/`%g` row of the `snprintf` work already refused device float formatting on exactly
   this reasoning ("disproportionate risk … matching glibc's rounding would make the
   differential unstable"). That judgement does not become wrong when the arithmetic is
   arrays instead of formats.
2. **The remaining integer lane source is 1.45 points, 94% of it one loop in one
   synthetic kernel.** `vlaloop`'s inner loop is 64 trips against a hand-pinned break-even
   of 48 — inside the noise of a constant that hazard 1 says was never measured properly.
3. **The caller cannot be written by connecting what exists.** Zero live-ins against a
   producer that refuses zero live-ins. A dispatcher needs a batch source, a write-back
   path for `c[i][j]` (frames are dense 8-byte slots capped at `MCC_SLICE_MAXSLOT` = 16, so
   a 600×600 tile does not fit one at all), and a per-compile correctness gate. That is
   three new subsystems, priced nowhere.
4. **The self-compile is barren for reasons no work removes**, and it is the one workload
   this project can measure whenever it likes.

#### The counter-argument, at equal length, because it is the useful part

1. **"Device-executable" is a property of today's stack, not of the workload, and this
   board has just made the same mistake it accuses the last one of.** The last board read
   `par=?` as "not parallel" and the corpus proved it wrong. This board reads `is_float` as
   "not lanes" — but `is_float` is six lines of refusal in files we own, not a fact about
   `matmul`. A SPIR-V compute shader doing `double` FMA is the single most ordinary thing a
   GPU does; the reason mcc cannot emit one is that nobody wrote it, which is precisely the
   category of objection the 80.60% result demolished for aliasing. **The honest label for
   row 1's headline is therefore "≈1.45% today, 80.60% ceiling", and this board's
   recommendation rests on declining to pay for the difference, not on the difference being
   unreachable.** Nobody has priced float in the emitter. It is UNMEASURED.
   — **ANSWERED 2026-08-09 (`wt/spvfloat`), and this point is half right.** "Six lines" is
   36 (the mirrored MSL/SPIR-V blocks, plus 18 in the CPU reference nobody counted), and the
   price is ~1,100–1,700 lines. But the objection fails for a reason better than cost:
   `is_float` is *not* only a choice we own. **`shaderDenormPreserveFloat32 = false` is a
   fact about the device**, fp32 denormals are measurably flushed on it, and `OpFDiv` is
   2.5 ULP by specification — so a bit-exact `float` path is unreachable no matter who
   writes it. `double` is reachable and is where all 79.2 points are. See "The price,
   measured 2026-08-09" below.
2. **The 3.67% collapse cuts both ways.** Yes, `matmul 600 8` is 77% of the corpus and its
   argv was chosen by `runtime-bench.py` to take about a second. But dense matrix multiply
   being over-represented in a benchmark suite is not evidence that it is rare in
   programs — it is over-represented in benchmark suites *because* it is common in the
   workloads people accelerate. The corpus was assembled to exercise codegen, not to look
   parallel, and it still came back 80.60%. A roster picked adversarially against this
   project produced the best possible result for it.
3. **Freezing has a compounding cost that nothing on this board prices.** Metal already
   demonstrates it: 1754 lines against 3612 (re-counted today; the board's 3578 is stale by
   a commit), a kernel arm that is two lines returning 0, and zero `msl_region*` symbols —
   a divergence that grew *while the arm was nominally maintained*. A frozen SPIR-V arm
   will do the same to itself against the AST, and the differential that currently catches
   miscompiles will decay into a differential that catches nothing because it cannot lower
   anything current. **"Freezing is not deleting" was true when the stack matched the
   compiler; it stops being true the moment the compiler moves.**
4. **The bug-finding yield is real and this board is not counting it.** Two silent
   miscompiles were caught this month by machinery built for the device, and
   `ast_loop_parallel_legal` is a host-usable dependence analysis regardless of where it
   dispatches — row 1 below shows two shipped transforms that need exactly the soundness
   fix it already has. Ranking the device column last while spending the top row on a
   defect the device work discovered is, at minimum, an accounting choice worth stating.

**Both sides of 1 are honest, and the recommendation stands on cost, not on impossibility.**
The measurement that would overturn it is named in row 6: price float in the SPIR-V emitter
and the CPU reference. If it comes back cheap, this verdict is wrong.

#### The price, measured 2026-08-09 (`wt/spvfloat`) — PRICED, NOT PAID

It did not come back cheap, and the reason is not the emitter. **The verdict stands, on a
ground the board did not have: bit-exactness is unattainable for `float` on the reference
device, and unpinnable in principle for division on every conformant device.** Nothing was
implemented. What follows is why, and what it would take to reopen.

**Everything in the premise reproduces except the site count.** `grep -c Float` is 0, 0, 0.
`tools/loop-census.py cmake-debug --corpus runtime` gives 80.66% `par=1`, 80.60%
iteration-weighted, hottest loop `id=2000005 matmul/main matmul.c:22` at 76.92%,
`id=11000015 loopnest/main loopnest.c:44` at 2.24%, `id=12000002 vlaloop/work` at 1.37%.
`matmul.c:4` and `loopnest.c:6` are `static double` arrays; 76.92 + 2.24 + `nbody.c:27`'s
0.06 ≈ **79.2 points of `double`**, and vlaloop is **94% of the 1.45-point integer
remainder**. The site count is wrong (36, not 11 — corrected above).

**The two hazards were measured on the real device, not argued from the spec.** A
standalone Vulkan probe (`glslc` + `vkQueueSubmit`, device created with
`shaderFloat64 = VK_TRUE`, values passed through an SSBO so nothing constant-folds) on the
NVIDIA RTX 5070 Ti:

| probe | host | device | verdict |
| --- | --- | --- | --- |
| `a*b+c`, fp64, `a=1+2⁻⁵²`, `b=1−2⁻⁵²`, `c=−1` | `0` (separate) / `−4.93e−32` (fma) | `0` | **did not contract** |
| `a*b+c`, fp32, same shape at 2⁻²³ | `0` (separate) / `−1.42e−14` (fma) | `0` | **did not contract** |
| `DBL_MIN*0.5` (fp64 denormal) | `1.1125369292536007e−308` | `1.1125369292536007e−308` | **preserved** |
| `FLT_MIN*0.5` (fp32 denormal) | `5.87747175e−39` | **`0`** | **FLUSHED — host/device disagree** |

`spirv-dis` confirms the module carries **no `NoContraction` decoration**, so the two
non-contractions are the driver's discretion, not a guarantee — contraction is permitted by
default and must be pinned. That part is cheap and sound: `NoContraction` is core SPIR-V,
needs no capability and no device property.

**The blocker is denormals, and it is not fixable from our side.** `vulkaninfo` on the same
device:

```
shaderDenormPreserveFloat32     = false     shaderDenormFlushToZeroFloat32 = false
shaderDenormPreserveFloat64     = false     shaderDenormFlushToZeroFloat64 = false
shaderRoundingModeRTEFloat32/64 = true      shaderSignedZeroInfNanPreserveFloat32/64 = true
shaderFloat64                   = true
```

Both denormal execution modes are unavailable for both widths, so the shader may declare
**neither** `DenormPreserve` **nor** `DenormFlushToZero` — denormal behaviour is whatever
the driver does, and the measurement above shows what it does: **preserves fp64, flushes
fp32.** Rounding mode and signed-zero/Inf/NaN *are* pinnable. Denormals are not.

**This inverts the brief's fallback.** "Start with `float` if `double` is blocked" is
exactly backwards:

- **`float` (fp32) is the arithmetically broken one** — a measured, reproducible,
  unpinnable host/device disagreement — **and it buys zero corpus points.** `mathfun.c` is
  the only `float` user in all 17 kernels and its loop is not in the `par=1` set.
- **`double` (fp64) is the clean one on this device** and is worth the whole 79.2.

**Vulkan's `OpFDiv` is 2.5 ULP, not correctly rounded** (spec Table 80, "Precision and
Operation of SPIR-V Instructions"; `OpFAdd`/`OpFSub`/`OpFMul` *are* correctly rounded, and
doubles are only promised "at least that of single precision"). So float division is
non-bit-exact **by specification, on every conformant device, forever**. Any float device
path must refuse `/` permanently or abandon exact comparison.

**The comparison that would be sound, and why.** Exact bit equality of the 64-bit payload —
integer compare of the bit pattern, differing NaN payloads counted as failure — matching the
discipline the integer differential already uses (`tools/slicerun.c:3022` exact `!=` plus a
definedness flag, `:3163` byte-exact `memcmp` of the shared region). **No tolerance.** A ULP
tolerance would be actively worse than nothing here: the corpus is mul/add over order-1
values where any real miscompile produces a *large* error, while the only legitimate
divergence (denormal flush) produces a difference a ULP window would also swallow. The
tolerance would hide exactly the bug class it was introduced for.

And bit-exactness *is* reachable for the 79.2 points, which is what makes this expensive
rather than simply impossible:

- The `par=1` loop in `matmul` is **`matmul.c:22`, the innermost `j`** of the i-k-j nest.
  Parallelising `j` does **not** reorder the `k` reduction into `c[i][j]` — `c[i][j] += aik
  * b[k][j]` accumulates in source order per lane. **No reassociation hazard.**
- `OpFAdd`/`OpFMul` are correctly rounded, so they are bit-exact with the host by definition.
- `NoContraction` pins FMA; RTE and SignedZeroInfNanPreserve are advertised true.
- fp64 denormals measured preserved, and `matmul`/`loopnest` never generate one anyway.

**Which is precisely the trap.** Such a differential would be green — and green *over a
corpus that cannot reach either divergent region*. `--mutate` would still pass its own test
(the 1-bit XOR at `tools/spvgate.c:654` is a 1-ULP perturbation on a double, comfortably
detectable), so the cells would satisfy the letter of the known-positive rule while being
**structurally unable to fail on denormals or on division** — the cardinal sin restated one
level up. Five cells passing over nothing was this month's lesson; this would be the same
shape with better arithmetic.

**Worse, the verdict would be device-dependent.** Denormal behaviour is unpinnable *and*
per-device, so the same bit-exact differential can return different answers on different
CI cells. `ci.yml:152` passes `-DMCC_GPU_REQUIRED=ON` on every Linux stage2 cell, which
turns a lavapipe/NVIDIA denormal disagreement into a hard CI failure **no code change can
fix**.

**RESOLVED 2026-08-09 (`wt/gatefin`) — lavapipe HAS `shaderFloat64`, read from Mesa source,
not inferred.** There is still no lavapipe ICD on this host (`/usr/share/vulkan/icd.d/`
holds `nvidia_icd.json` only; Mesa 26.0.8 here is built `VIDEO_CARDS="nvidia"`, so its
installed file list contains no Vulkan component at all and `find / -name 'libvulkan_lvp*'`
returns nothing), so this is a source reading and is labelled as one. The source was to
hand: `/var/cache/distfiles/mesa-26.0.8.tar.xz`, the Gentoo distfile for the Mesa actually
installed. Two lines decide it, and both are unconditional:

```
src/gallium/frontends/lavapipe/lvp_device.c:454   .shaderFloat64 = (pdevice->pscreen->caps.doubles == 1),
src/gallium/drivers/llvmpipe/lp_screen.c:301      caps->doubles = true;
```

`lvp_get_features()` fills `device->vk.supported_features` at `lvp_device.c:1443`, and
`lvp_device.c:1050` confirms the driver identity (`.driverID = VK_DRIVER_ID_MESA_LLVMPIPE`).
`caps->doubles` sits in a flat run of unconditional assignments in
`llvmpipe_init_screen_caps()` — no `#if`, no LLVM-version gate, no `debug_get_bool_option`.
A grep for `doubles` across `drivers/llvmpipe/`, `frontends/lavapipe/` and `auxiliary/draw/`
returns exactly three hits: those two plus `lp_screen.c:485 .lower_doubles_options`, a NIR
option. `DRAW_USE_LLVM` (`lp_screen.c:143`) is in `llvmpipe_init_shader_caps()` and touches
only const-buffer and sampler limits; it does not reach `caps->doubles`. Confirmed
byte-identical at the same line numbers in `mesa-26.0.7.tar.xz`. **Scope of the claim:
Mesa 26.0.x. The Mesa version on the CI runner's `mesa-vulkan-drivers` is not pinned in
`ci.yml:116` and was not read, so "CI's lavapipe" remains an inference from "26.0.x
lavapipe".**

`docs/PLAN.md`'s E6 assertion — the thing this went looking for — is therefore **correct**,
and PLAN's I2 row (the `shaderFloat64 = TRUE` refusal floor, "see E6") rests on it. It was
left uncited here because this branch owned the audit section and not the E-track rows;
**`wt/docsync` copied the citation across on 2026-08-09**, so both PLAN rows now carry it
and both say plainly that lavapipe's half is read from Mesa source, not from a device.

**Which means row 6's float row does NOT close permanently — and the denormal question is
live.** The consequence chain in the paragraph above now runs to its end, and the answer is
that the two devices *agree*. `lvp_device.c:1073-1077` gives lavapipe's fp64 float-controls:

```
.shaderDenormFlushToZeroFloat64 = false,
.shaderDenormPreserveFloat64    = false,
.shaderRoundingModeRTEFloat64   = true,
.shaderRoundingModeRTZFloat64   = false,
.shaderSignedZeroInfNanPreserveFloat64 = true,
```

Set that beside the measured NVIDIA `vulkaninfo` banked at `:297-304`, which reports both
fp64 denorm modes false as well. So `ci.yml:152`'s `MCC_GPU_REQUIRED=ON` does **not** turn a
denormal *advertisement* disagreement into an unfixable CI failure: neither device advertises
either fp64 denorm mode, and a differential that pins nothing about denormals is consistent
across both. **What that does not buy is agreement on denormal *behaviour*.** Advertising
neither mode means the implementation is unconstrained by `VK_KHR_shader_float_controls`, so
lavapipe's LLVM-generated fp64 and NVIDIA's may still differ on a denormal input while both
report the same feature bits. `:344`'s "fp64 denormals measured preserved" is a measurement of
**the NVIDIA device only** and must not be quoted of lavapipe. The recommendation below is
unchanged: what closed row 6 was never the `shaderFloat64` bit, it was the ~1,100–1,700 lines
and the green-by-corpus-construction differential.

**The line estimate, against the actual code — ~1,100–1,700 lines, not "a handful".** The
board's "six lines of refusal in files we own" is right about the *gates* and wrong about
what the gates are holding back:

| | work | est. lines |
| --- | --- | ---: |
| **CPU reference value model** | `src/ast_eval_slice.h` is **`int64_t`-only** — no tagged union, no float lane. `ast_eval_narrow:62`, `ast_eval_binop:70`, `ast_eval_slice_fit:255`, `ast_eval_slice_env:234`, `ast_eval_slice_rec:639` (every AST kind), `bytes_load/store:349,372`, plus `MccSliceWork.out[]`, `mcc_slice_run_cpu` (`mccslice.h:156`) and the frame interpreter (`:595`, `:723`) | **350–550** |
| **SPIR-V emitter** | `OpTypeFloat` 32/64, `OpFAdd/FSub/FMul/FDiv/FNegate`, `OpFOrd*`, `OpConvertSToF/FToS/FConvert`, float constants (fp64 = 2 literal words), typed load/store, `NoContraction` threaded through `spv_emit3` | **400–550** |
| **`Float64` capability plumbing** | `VkPhysicalDeviceFeatures` is a **bodyless forward typedef** (`mccgpu.c:769`), `vkGetPhysicalDeviceFeatures` is **not in the loader X-macro** (`:1373`), and `pEnabledFeatures = NULL` (`:1623`). Needs the struct body, the loader entry, the query and the enable — **twice**, because `tools/spvgate.c:225` carries its own duplicated Vulkan | **80–120** |
| **Buffer/ABI** | all three SSBOs are `OpTypeRuntimeArray` of **int32, ArrayStride 4** (`mccgpu.h:1662`, `:1600`). fp64 needs a `uvec2`↔`double` `OpBitcast` (reusing E2b's pair machinery, the cheap route) or a second aliased binding at stride 8 | **60–100** |
| **Slice-engine gates** | the 36 lines above — each becomes type-directed dispatch, not a deletion | **80–150** |
| **MSL parity** | **MSL has no `double` at all** (`PLAN.md` E6). Either gate the Metal arm off for fp64 — which also strands the macOS `gpu-vulkan` cell, since MoltenVK-on-Metal has no fp64 either — or write a software f64 the size of E2b's int64 emulation | **50** *(gate)* / **600+** *(emulate)* |
| **Tests** | new suites, `--mutate` hooks, `must-run.txt` registration, and **new seeds**: `seed_value()` (`slicerun.c:2971`) is `{0,1,-1,2,7,-3,1000,-12345}` — integers, which would exercise none of this | **200–300** |

**What the corpus number becomes if the block is lifted.** `double` add/sub/mul only, no
division: **≈1.45% → ≈80.6%** device-executable parallel-legal (+79.2). `float`: **+0.0**,
and it is the width that cannot be made exact. That asymmetry is the whole result — the
points are all in the one type that works, and the type the brief proposed starting with is
worth nothing and is broken besides.

#### M6 was implemented after all — 2026-08-09 (`wt/fpwidth`). Read this before quoting anything above it.

The recommendation below was overturned by decision, not by argument, and `double` was
built. What follows is what it cost, what it is certified to do, and the two things the
differential found that were not in anyone's price.

**The corpus verdict, and it is the opposite of the +79.2 above.** The predicate ceiling
reproduces exactly (`tools/loop-census.py cmake-debug --corpus runtime --levels O2 --top 20
--opt-in` → 97.76% raw, **80.60%** parallel-legal, `matmul.c:22` 76.92%, `loopnest.c:44`
2.24%, `vlaloop.c:13` 1.37%). The **device-executable** parallel-legal iteration-weighted
fraction goes **≈1.45% → ≈1.45%**. Every `double` par=1 loop in the corpus — 76.92 + 2.24 +
`nbody.c:27` 0.06 + `matmul.c:14` 0.02 + three `loopnest` loops at 0.01 — indexes a
**`static`** array, and `static` is not a frame local. Even as locals they would not lower:
`MCC_SLICE_MAXSLOT` is **16** and every array element takes a slot, against 360,000 elements
in `matmul` and 65,536 in `loopnest`. This was checkable before the row was priced and
nobody checked it; the price was written as though `is_float` were the only gate.

**What did move**, measured over the same 17 kernels by harvesting each one's arena and
running `slicerun --arenas`: f64 device work went from **0** to **40 expression slices and 3
frame runs**, in 6 of the 17 kernels (`regpress` 26, `mandelbrot` 6+1, `mathfun` 3, `calls`
2+2, `nbody` 2, `poly` 1), 378 slices and 97 frame kernels compared in total, **0
mismatches**. Real work, dispatched and compared bit-exactly; just not the hot loops.

**And a second correction, from the conformance side (`gcc.c-torture`, 1,172,443 nodes,
31.9% accepted): `type-float` is 0.48% of node refusals and 1.88% of bodies**, against
`ref-not-local` 12.03%, `child-refused` 11.45%, `kind-basicblock` 10.18%, `op-unary` 9.18%.
Twenty times smaller than the structural refusals. **Do not quote `double` as unblocking the
funnel on a general corpus.** The numeric corpus is where the type mattered, and there it
was outranked by storage class.

**Certified bit-exact**, over a 22-payload table crossed with itself (484 tuples per row) —
`VT_DOUBLE` only, `+`, `-`, `*`, unary `-`, `!`, the six ordered comparisons, and `&&`/`||`
truth: **3,965 tuples, exact bit equality of the 64-bit payload, no tolerance.** The table
carries +0.0, −0.0, both infinities, `DBL_MAX`, `DBL_MIN`, the largest and smallest
subnormals, 1.0+1ulp and three quiet NaNs with distinct payloads, so the negative classes
are in the input set rather than absent from it.

**Excluded, each with a cell that fails if the exclusion lapses** (`f64_exclusions`):
`/` and `%` at any width (`OpFDiv` is 2.5 ULP by spec — not bit-exact on any conformant
device, ever), all of `VT_FLOAT` (fp32 denormals measurably flush on this device and
`shaderDenormPreserveFloat32` is false, so no execution mode can pin it), `VT_LDOUBLE`,
int↔float conversion in both directions, and **mixed int/float operands in both orders**.
Metal is gated off rather than emulated: `mcc_gpu_f64()` returns 0 unconditionally on the
Metal arm, because MSL has no `double`.

**Two device readings, taken at runtime over real work rather than from feature bits:**

1. **fp64 denormals are PRESERVED on the NVIDIA RTX 5070 Ti — 373 denormal-touching tuples
   compared bit-exactly.** This is the *runtime* reading the row above asked for and never
   had. It is not asserted: the cell requires the device to match one of the two models
   the spec permits (preserve, or flush-with-sign) **consistently**, prints which, and
   fails if it matches neither or mixes them. A lavapipe runner that flushes reports
   "FLUSHES" and stays green; a device that does something else is a failure. Neither
   device advertises either fp64 denorm execution mode, so this could not have been
   predicted from `vulkaninfo`.
2. **NEW, and nobody had filed it: the two-NaN payload tie-break diverges.** When both
   operands of an fp64 arithmetic op are NaN with different payloads, IEEE-754 does not say
   which propagates — and **x86-64 SSE returns the first operand's payload while the device
   returns the second.** 18 tuples. Every *single*-NaN tuple matches exactly, payload
   included, so this is one specific tie-break and not "NaNs are unreliable". The cell
   asserts the IEEE-legal model (the result is one of the two operand payloads) rather than
   asserting the divergence away. This is a third permanently unpinnable class beside fp32
   denormals and `OpFDiv`, and it was found by running the differential, not by reading a
   spec.

**A miscompile found and fixed on the way in, and it predates this branch in kind.**
`is_float(ast_type_t(a, child))` is not a float test: an indexed element's node carries type
**0** in every real arena, its element type living on the object. So `(int)c[0]` over a
`double c[4]` passed the `AST_Convert` guard in `ast_eval_slice_rec`, in
`ast_eval_slice_kind_ok` and in `spv_expr`, and the emitter then ran `spv_val_lo` on an
`OpTypeFloat 64` value — an **invalid module** (`spirv-val`: *"Reached non-composite type
while indexes still remain to be traversed"* on `OpCompositeExtract %uint %double 0`) that
**segfaulted inside `libnvidia-glvkspirv.so`**. Fixed at all three sites plus the
value/destination type-match guards on frame stores and returns. `spirv-val` now passes on
every module the probe emits; it is still referenced nowhere in the build (row 8).

**And a performance trap, plus the one cell that can see it — OPEN.** The first cut of the
float type inference had no early-out for a node whose own type word is already a plain
integer type, so on an all-integer tree it walked the whole subtree instead of answering
from the node in front of it: O(n²) per slice, and `slicerun --cost-synth` went from
**21.6s to over 600s**. `slice/cost` is the only cell in the suite that can catch an
interpreter slowdown, because it is the only one that measures time — and **it has no
`TIMEOUT` property**, so what it actually did was hang the whole `ctest` run rather than
fail it. Two hours were spent finding that by hand. Fixed (23.2s, 7% over baseline, and the
cell passes in 23.35s), but the cell should get a `TIMEOUT` and, better, a banked duration:
a 28× regression that presents as "ctest never finishes" is indistinguishable from a
flaky machine.

Adjacent and also fixed here: `ast_eval_slice_wtype`'s `AST_Load` arm returns the element
type of an indexed access, and once `ast_eval_slice_dynidx` accepts a `double` element type
that arm could return `VT_DOUBLE` — a value every one of its callers reads as "not 64-bit,
signed" and narrows to int32. It now returns 0 for a float element, which is what its
integer-width contract always promised. Anyone widening `dynidx` again should re-read that
arm first.

**The interaction with the usual-arithmetic-conversions bug, stated rather than left to be
found.** `ast_eval_slice` takes a Binary's working type from **child 0 alone** and never
consults operand 1, so C's usual arithmetic conversions are never applied; `spv_expr` and
`msl_expr` mirror the rule, which is why the existing differential is blind to it. **That
bug is not touched here** — a separate agent owns it. The float lane is written so it does
not depend on the broken rule: `ast_eval_slice_ftype` is a **separate** function from
`ast_eval_slice_wtype` (which is unchanged, byte for byte), it over-reports for a Binary
(either operand float ⇒ subtree float), and every acceptance site demands **both** operands
be float. Over-reporting can therefore only turn an acceptance into a refusal, and the
answer does not depend on operand order — `1.5 + x` and `x + 1.5` are both refused, and
there is a cell for both orders over five operators. **What will need revisiting when the
conversion fix lands:** the mixed-operand refusals become the place to implement promotion
instead, and `ast_eval_slice_f64_op` is where the promoted operator set would be widened.
Nothing else in the float path reads a working type.

**Recommendation: do not implement. The row is priced and closed.** ~~Not because the emitter~~
*(superseded 2026-08-09 by the section above; kept because the reasoning about `float`,
division and denormals is still correct and is why those three are excluded.)* Not because the emitter
is hard — it is ordinary — but because the honest version costs ~1,100–1,700 lines across
two backends and a CPU reference that has no float representation at all, and buys a
differential that is **green by corpus construction**, **unable to fail on its two real
divergence classes**, **restricted to Vulkan on a discrete GPU**, and **liable to hard-fail
CI on a device nobody has measured**.

**The counter-argument, because it is not weak.** 79.2 points is a 55× increase in
device-executable coverage, the `matmul` reduction genuinely does not reassociate, `OpFAdd`
and `OpFMul` genuinely are correctly rounded, fp64 denormals genuinely are preserved on the
device we have, and `--mutate` genuinely would prove the new cells can fail. A `double`-only,
`+`/`−`/`*`-only path with `NoContraction` and RTE pinned would be bit-exact on the corpus
and is defensible on its own terms. The answer is that "bit-exact on a corpus that cannot
reach the divergent region" is the exact claim this project has already been burned by
five times this month, and the burn is worse here because the divergence is **unpinnable**
rather than merely unimplemented — no future commit fixes `shaderDenormPreserveFloat32 =
false`.

**What would reopen it**, in order of decisiveness:

1. A device advertising `shaderDenormPreserveFloat32/64 = true` (making both widths
   pinnable), *or* a decision to restrict the device path to fp64 and refuse `/` — the
   latter is free and is what a reopened row should assume.
2. ~~A measured `vulkaninfo` from lavapipe under CI conditions. If it lacks `shaderFloat64`,
   the 79.2 points are unreachable on the only device the suite tests against and the row
   is closed permanently rather than provisionally.~~ **SETTLED 2026-08-09 (`wt/gatefin`),
   and it did not reopen the row.** Mesa 26.0.8's `lvp_device.c:454` reads `shaderFloat64`
   off `llvmpipe`'s `caps->doubles`, set unconditionally at `lp_screen.c:301` — see the
   RESOLVED block above. So the 79.2 points are *reachable* on lavapipe and the row closes
   **provisionally, not permanently**. It stays closed on the line count and on the
   green-by-corpus-construction argument, neither of which this touched. Still open, and
   now the only device question left: a *runtime* fp64 denormal reading from lavapipe.
   Both devices advertise neither fp64 denorm mode, which makes them consistent on paper
   and says nothing about what either computes.
3. A corpus kernel whose hot loop is `float`, which would change the +0.0 above.

#### Metal — settled, 2026-08-09: dropped

The last board said "decide it, do not pay it down" and left it undecided. Deciding it:
**Metal is not a device target.** Re-counted today with a nesting-aware pass over the
`#if MCC_GPU_LANG_MSL` / `#else` arms — **1754 MSL lines against 3612 SPIR-V** (`mccgpu.c`
653/1461, `mccgpu.h` 1008/1794, `mccslice.h` 27/65, `spvgate.c` 66/292); the board's 3578
was stale by commit `99e043c1` and the MSL side matched to the line *because nothing has
been added to it*. `mcc_slice_frame_kernel_build`'s Metal arm is two lines returning 0
(`src/mccslice.h:1282-1285`); `grep -rn msl_region src/ tools/` is **0** against 31 distinct
`spv_*` symbols reached from `mccslice.h` alone; `src/mccfmt.h:453` gates the whole format
emitter `!MCC_GPU_LANG_MSL`; and `tools/slicerun.c` carries exactly one `#if`, for
`AST_EVAL_SLICE_PROVIDED`, so it cannot compile against the Metal arm even in principle.
Keep the `#if MCC_GPU_LANG_MSL` arms only where they already compile; add no more. This is
an owner's decision recorded so it stops being taken by default, one landing at a time.

### The ranking, and the currency each row is paid in

Rows are ordered by expected payoff **in a currency that has a demonstrated exchange rate in
this tree**. Two currencies convert: *correctness* and *the compiler's own emitted code or
wall-clock*. One currency has a measured rate: `kept` buys stage-2 at **0.03% per point**
(+2.60 points bought −0.079%, row 2). One currency has **no** exchange rate here and every
row denominated in it ranks below every row that is not: *device-eligible blocks* and
*device-accepted sites*.

| # | row | currency | expected payoff |
| ---: | --- | --- | --- |
| 1 | `ast_loop_interchange_legal` / `ast_dep_fusion_pair_illegal` consult `ast_dep_base_distinct` with **no `indirect` guard**, and unlike the census predicate they reach emitted code | **correctness** | UNMEASURED, and it is the same defect class that produced two miscompiles this month |
| 2 | ~~`tests/optfire/levelbench.tsv` is a generation stale and has no `--check`~~ | **census trust** — every future ladder decision is priced off it | ~~32 of 47 rows name flags no longer at levels 1–3~~ — **LANDED 2026-08-09 (`wt/gatefin`).** The stale generation was re-measured by `wt/ladder2`; `--check` now exists, has a ctest cell and a known-positive, and the build-dir TSV is compared back to `tests/optfire/`. See the audit section |
| 3 | Metal | **a decision** | free to make, grows with every SPIR-V landing. **Decided above: dropped** |
| 4 | the device path | **device-eligible blocks** — no exchange rate | **Frozen above, and the freeze now rests on a different reason than the one it was taken for.** ≈1.45% device-executable lanes on the best corpus anyone has found — **still ≈1.45% after row 6 landed `double` (`wt/fpwidth`, 2026-08-09).** The 79.21 float points were never gated by `is_float`. They are gated by **`static` storage and by `MCC_SLICE_MAXSLOT = 16`**: `matmul.c:22` (76.92% of all corpus iterations) and `loopnest.c:44` (2.24%) index `static double [600][600]` and `[256][256]`, the live-in model addresses frame **locals** only, and it gives every array element its own slot. Measured three ways on this tree — a 4-element **local** `double` array lowers and dispatches; the same source with `static` arrays does not; a 64-element **local** array does not. Float was necessary, and nowhere near sufficient |
| 5 | `snprintf` module budget | **device-accepted sites** — no exchange rate | banked at 148/162; the 7th site buys one site, the 8th needs `MCC_GPU_CODE_MAX` raised. **Stop** |
| 6 | float in the slice engine and the SPIR-V emitter | **device-executable lanes** | ~~UNMEASURED and unpriced~~ · ~~PRICED 2026-08-09 (`wt/spvfloat`), NOT PAID~~ — **LANDED 2026-08-09 (`wt/fpwidth`), and it bought +0.0 iteration-weighted points, not +79.2. The `+79.2` estimate was wrong, and the reason is in the next row.** `double` now runs on the device: `OpTypeFloat 64`, `OpCapability Float64`, `OpFAdd/FSub/FMul/FNegate` every one decorated `NoContraction`, the six `OpFOrd*`/`OpFUnordNotEqual` comparisons, `shaderFloat64` queried and enabled at `vkCreateDevice`, and a uvec2↔double `OpBitcast` at the buffer boundary so the ABI, the stride and the lo/hi packing are all unchanged. `float`, `long double`, division at any width and int↔float conversion are **excluded, each with a cell that fails if the exclusion lapses**. New cells `slice/f64` + `slice/f64-known-positive` (9161 → **9163**). See the M6 section below for what is certified, what was measured on the device, and the two things this found that nobody had filed |
| 7 | chain-store re-promotion | emitted code | **MEASURED, refused.** +2.60 `kept` → −0.079% stage-2 for +1.50% stage-1; 60× worse than `divmagic`'s rung |
| 8 | `storeval-rot` demotion | emitted code | **MEASURED, refused.** Its off-state is an incomplete replay path, `kept` 91.978 → 83.242 |
| 9 | `narrow` (rung 10) and `tree-copy-prop` (rung 11) priced on the self-host axis | emitted code | **MEASURED 2026-08-09, both levels unchanged.** `narrow` +0.876% of stage-1 for −0.0088% of stage-2 (**100:1 against**); `tree-copy-prop` +0.799% for −0.0048% (**166:1**). Both are worse than the 19:1 the ladder already refused at row 7. Neither was "priced on nothing" — that premise was false, see the correction in the audit section |

Row 1 is first because it is the only row on this board that could be a wrong answer in
shipped output, and because it is cheap to settle: `-floop-interchange`, `-floop-fusion` and
`-floop-block` are all `MCC_OPTD_LEVEL(12)` (`src/mccopt.h:109-111` — note the flag is
spelled `-floop-block`, **not** `-floop-tile`, which does not exist), all three are bound in
`ast_configure` (`src/mccast.c:2384-2386`) and all three run from `ast_func_end`
(`:18586-18590`) inside the emission path, calling a *mutating* apply. `ast_tile_run` reuses
`ast_loop_interchange_legal`, i.e. the unguarded predicate. A fuzz corpus of `p[i]`/`q[i]`
nests through two global pointers at `-O12` is the measurement.

Row 2 is second because it is the instrument every other ranking uses. See hazard 2. It is
now **landed**, so row 1 is the only unmeasured row left above the priced ones.

### Where every number on this board comes from

The recurring failure of this file is headline figures with no script behind them. **The
count is the failed-to-reproduce table below and nowhere else** — do not restate it here;
that is how it came to be written as twelve, nine and seven in three places at once. The
two that failed on `wt/benchtrap` failed for a new
reason — the measuring tool was defective, not the transcription. The three added on
`wt/ladder2` are a third reason again: `reg-color` and `if-conversion-abs` were correctly
measured on a **tree that has since moved**, and the `narrow` / `tree-copy-prop` claim was a
**stale row misread as an unmeasured one**. Age is now as good a reason to distrust a figure
here as method is. Every number below names the committed
tool and the corpus that produces it, or is marked **PROSE-ONLY** / **UNMEASURED** /
**DERIVED**.

| number | tool | corpus |
| --- | --- | --- |
| 2035 loops instrumented, 602 entered, 26,103,304 entries, ~52.15M iterations, 52.51% raw, **0.01%** parallel-legal, **0.01%** with `--alias-oracle`, `body-unsafe` 50.04% + `not-analyzable` 33.82% | `tools/loop-census.py cmake-debug --top 20`, and `… --alias-oracle` | self-compile of `src/mcc.c` @ `-O2` |
| 77 loops entered, 19,803,274 entries, 2,246,355,539 iterations, 97.76% raw, **80.60%** parallel-legal, **80.60%** with `--alias-oracle` (reports byte-identical), 3.67% hottest-loop-removed, 3.66% `matmul.c`-removed, 26 loops `par=1` | `tools/loop-census.py cmake-debug --corpus runtime --top 20`, and `… --alias-oracle` | the 17 in-tree kernels of `tools/runtime-bench.py`'s `KERNELS`, argv unchanged |
| per-program `par=1`: `matmul` 99.83%, `loopnest` 99.23%, `vlaloop` 63.68%, all other 14 ≤ 0.83% | the same run's per-program table | the same |
| **79.21 points floating-point / 1.45 points integer** of the corpus's 80.66 `par=1` points | **DERIVED**: the per-program table above × the scalar type of each kernel's hot loop, read from `tests/runtime/*.c` | the same |
| a `double` body gives `slices=0 gpu-slices=0` and `FAIL`; the identical `int` body gives `slices=1 gpu-slices=1` and `OK` | `mcc -O2 -c` under `MCC_ARENA_DUMP`, then `slicerun --arenas <dump> --limit 400` | a two-line fixture, both spellings |
| **0** occurrences of `Float` in `src/mccgpu.h`, `src/mccgpu.c`, `src/mccslice.h` | `grep -c` — **no committed tool** | the device layer |
| `kept` **83.090 / 91.913 / 91.978 / 91.978** (`self`), **92.881 / 96.546 / 96.597 / 96.597** (`wide`), at `-O0`..`-O3` | `tools/rir-coverage.py cmake-debug --corpus self --levels O0,O1,O2,O3`; `MCC_RIR_CENSUS=1 … --corpus wide … --opt-in` | `self` = `src/mcc.c`; `wide` = 380 sources |
| **552 slices of 12,957 contain a loop (4.26%)** | `tools/slice-census.py cmake-debug --corpus self --levels O0,O1,O2,O3` | self-compile |
| 172 `snprintf` sites, 162 literal, **148 accepted (91.4%)**, 100 carrying `%s`, 9 budget / 4 flag / 1 float; return consumed at 26 of 162 | `tools/fmt-census.py --check` — **gated**, `fmt/census-bank` + known-positive | `src/*.c`, the 18-file roster pinned in `CORPUS` |
| **10,423** Invoke-blocked blocks, **86 (0.825%)** unblocked by the `snprintf` family alone, 10th among single-callee unblocks behind `_mcc_error` 432, `fprintf` 214, `printf` 160, `memcpy` 126 — **RE-TAKEN 2026-08-09** on the de-duplicated corpus. The old `29,309 / 242` was the same measurement over a dump that recorded each body **2.8636** times; the counts deflate, the share does not (0.826% → 0.825%) | `tools/fmt-census.py --arena-check=<build-dir>` — **gated**, `fmt/arena-census-bank` + known-positive | `src/mcc.c`, the one real TU, @ `-O1`: 2,880 arenas over **2,880 distinct bodies**, 19,901 non-empty blocks |
| twelve-key `-O0` object `sha256` + forced-Replay_IR counters, 304 files, `bar=OK` on every key | `tools/o0_ab.sh` — **gated**, `ast/o0-baseline` + known-positive | `tests/exec`; re-taken 2026-08-09 after 426 unwatched commits, **28 objects had moved** |
| 947 non-empty blocks, **454** Invoke-blocked, 319 eligible, **33** sole-blocker | `slicerun --arenas <dump> --census` | `tests/exec`, 60 files @ `-O1` |
| 19,454 blocks, **10,238** Invoke-blocked, 7,524 (73.49%) all-internal, 4,029 eligible, **803** sole-blocker | `slicerun --arenas <dump> --census` | `mcc -O2 -c src/mcc.c`, `MCC_RIR_PROD=2` |
| break-even lanes 322 / 108 / 48 / 24 / 23 / 8 | `slicerun --cost-synth` | synthetic, this host — **and see hazard 1** |
| 143 corpus `frame-compared`, `*p` 0 → **159**, pointer `++`/`--` 0 → **168** | `slicerun` with and without `--no-ptr` | 440-block musl/string corpus |
| **1754** MSL lines against **3612** SPIR-V; 2-line Metal kernel arm; **0** `msl_region*` symbols against **31** `spv_*` reached from `mccslice.h` | `grep` + a nesting-aware `awk` over the `#if MCC_GPU_LANG_MSL` arms — **no committed tool** | `src/mccgpu.{c,h}`, `src/mccslice.h`, `tools/spvgate.c` |
| ~~25.3M entries, 162.7M iterations, 85.45% / **65.75%** / **1.88%**~~ | — | **STALE.** Pre-`pvokclear`. Do not quote; the parallel-legal figure is 0.01% |
| ~~corpus **1.39%** parallel-legal, **79.35%** `bases-may-alias-indirect`~~ | — | **STALE.** Pre-`wt/decaytype`. The predicate answers 80.60% on its own and that reason row no longer exists |

**Ten hazards, stated rather than buried.**

1. **The break-even lanes are pinned by hand** in `tools/loop-census.py:125` —
   `BREAKEVEN = [(3, 322), (7, 108), (15, 48), (31, 24), (63, 23), (127, 8)]` — a literal,
   read back from no run. If the bench moves, every fraction scored against it moves and
   nothing notices. Worse, only the 3-, 7- and 15-node rows of that bench were ever signal:
   at 511 nodes a *fixed* binary ranged 13.9–64.1 ns/lane across three runs. The 23 and the
   8 are noise frozen into a constant — and `vlaloop`'s 64 trips, the entire remaining
   integer lane source, are adjudicated against exactly that frozen region. **The tool now
   says so in its own output** (2026-08-09, `wt/benchtrap`): a `BREAKEVEN_PROVENANCE` banner
   prints directly under the headline fraction and names all three limits, including a third
   nobody had written down — `breakeven_trips` stops at 127 nodes and holds every larger loop
   body to 8 trips where `--cost-synth` measured 7/5/3/2, so the fractions are biased
   *downward* by an unknown amount. The constant itself is still pinned; see the audit
   section below for the cost of un-pinning it.
2. **~~`tests/optfire/levelbench.tsv` is a null-experiment factory, and it is also a
   generation stale.~~ ALL FOUR DEFECTS CLOSED — three on `wt/benchtrap` and `wt/ladder2`,
   the fourth by re-running the bench 2026-08-09.** The table is now a fresh measurement of
   the **16** flags `src/mccopt.h` actually has at levels 1–3, with `gain_movers_pct` /
   `eff_movers` beside the diluted columns and a signed efficiency. Kept below as the
   statement of what was wrong, because three of the four are live traps in any table of
   this shape. Per-defect status is inline. Four separate defects, all verified today:
   - **24 of its 47 rows have an empty `fires_kernels`** — the flag changes no kernel object
     at all — and **11 of those carry a `gain_pct` of `-0.0000`**, a geometric mean of
     `instructions:u` over binaries that are bit-identical. A `-0.0000` is not a rounded
     zero: it requires a *timing difference between two identical objects*, which
     `tools/optlevel-bench.py` collects because the timed executable lives in a
     `tempfile.mkdtemp` path whose length changes the loader's work. **Any of the eleven can
     be quoted as "measured no effect" the way `storeval-rot`'s was.**
   - The tool's only null guard is `bucket = "inert"`, which requires the flag to change
     nothing in the corpus *and* the self-compile *and* all 17 kernels. **Zero rows in the
     shipped file are `inert`.** The real guard — "changes no kernel object; nothing to
     adjudicate" — exists at `optlevel-bench.py:302-306` but runs only under
     `--cycles --from-json` and never touches the TSV.
   - `efficiency` is `gain / cost` when `cost > 0.005` and `inf` otherwise, and `has_gain`
     tests `abs(gain)`, **not its sign**. So a flag that makes emitted code *worse* at
     near-zero cost is awarded `+inf` and sorted to the top: `promote-leaf-callee`
     (`gain=-0.3360`) sits at **rank 2 of the whole table**. **FIXED 2026-08-09
     (`wt/ladder2`)** — and it was still live: in the fresh run `inline-functions`
     (`gain_movers` **−1.9557**) sorted to **rank 4** before the fix and sits at **rank 9**
     with `-inf` after it. Efficiency now carries the sign of the gain; `--selfcheck`'s
     `worseflag` row fails if it stops doing so.
   - **32 of the 47 rows name flags that are no longer at levels 1–3.** The file was written
     at `1ad3f1aa`; `893c1e84` moved 34 rows. `src/mccopt.h` has **16** `LEVEL(1..3)` rows
     today, `builtin-math-fabs` has no row at all, and `optlevel-bench.py` has no `--check`
     arm (`selfhost-optbench.py` does). CMake writes the TSV to the *build* dir, never back
     to `tests/optfire/`, so nothing compares them. **CLOSED 2026-08-09 by re-running the
     bench**: the banked table is 16 rows against `src/mccopt.h`'s 16, `builtin-math-fabs`
     has a row, and the stale-row trap is now itself documented — reading a stale row's
     `n/a` as "this flag is unpriced" is what produced the false `narrow` /
     `tree-copy-prop` claim in the audit section below. ~~There is still **no `--check`
     arm**, so nothing stops the file going stale again; that is the remaining debt.~~
     **`--check` LANDED 2026-08-09 (`wt/gatefin`), both halves** — the arm itself and the
     CMake half that wrote to the build dir and never compared back. See the audit section.
3. **A per-flag sweep cannot price a family, and the failure reads like a win.**
   `optlevel-bench.py` builds jobs as `[("__base__", []), ("__noise__", [])] + [(n,
   ["-fno-"+n])]` against an `-O3` base, so it can only ever take a shipped flag *out* —
   flags at rungs 4–12 produce no row. `selfhost-optbench.py` does have an add-one-in arm,
   but it adds a **singleton**, never a family, and carries the same `<= 3` filter. And a
   flag that suppresses optimization reads exactly like a flag that is efficient:
   `-fchain-store` alone measures −1.859% of stage-1 while taking `kept` 91.978 → 84.04.
   **A flag that moves `rir-coverage` must be priced with the coverage beside the counter,
   or the sign is meaningless.**
4. **16,537 is unreconstructible.** No corpus reaches it — the amalgamated self-compile
   gives 10,238, the 15 sources separately and de-duplicated give 10,239, and the same 15
   *with* the dump's duplicate body records give 28,753. Anything still resting on it is
   dead until re-taken with a named tool.
5. **ctest registration is configure-order dependent, and the debt-#7 fix did not close
   the gap — it moved it.** `mcc_cross_cc` (`CMakeLists.txt:3340`) falls back to
   `EXISTS "${MCC_CROSS_DIR}/mcc-<arch>"`, which CMake evaluates **at configure time**.
   Measured today: configuring `cmake-debug` with no `cmake-cross` present registers
   **8972** cells; building `cmake-cross` and re-running `cmake --preset debug` registers
   **9136**, matching `cmake-cross` exactly. The 164 missing cells are the same 144
   `optfire-{arm64,i386,riscv64}/*` + 20 `*-docker` the debt describes. **Any cell count
   taken from a `cmake-debug` configured first is 164 low and reports no skips.**
6. **`tests/exec` and self-compile numbers are never comparable.** `docs/DEVICE-LIBC.md`
   forbids `tests/exec` for the libc phase: `printf` alone is 35% of its Invoke nodes and
   its libc ceiling is 3 blocks against the compiler's 734 — the two disagree by **47×**.
   Never put them in one column.
7. **`tools/fmt-census.py` is a second implementation of `mcc_fmt_compile`.** ~~and nothing
   gates it against the C one~~ — **CLOSED**, and re-verified 2026-08-09: `fmt/census-oracle`
   pipes ~41,000 formats (every literal in `src/*.c` plus a seeded walk over the item/arg/
   literal-run/budget boundaries) through `slicerun --fmt-verdict`, which calls the real
   `mcc_fmt_compile`, and diffs **both** the verdict and the accepted cost. It has a floor
   (`len(fmts) < 1000` fails) and a known-positive that re-injects the historical drift. The
   cost-model constants `C_BASE`/`C_BYTE`/`C_HEX`/`MAXCOST` are therefore continuously
   diffed and are *not* hand-pinned in the dangerous sense. Three residues survive and are
   filed below: three port branches with no subject in either corpus half, an ungated
   `AST_INVOKE = 11` in the `--arenas=` path, and the fact that the board row this tool
   feeds was never regenerated after the drift closed. Row 4 of the table above reads
   `172 sites, 162 literal, 140 accepted (86.4%), 98 carrying %s, 17 budget / 4 flag / 1
   float`. Re-run on this tree it is **162 literal, 148 accepted (91.4%), 100 carrying
   `%s`, 9 budget / 4 flag / 1 float** — four of the six figures moved, `140 → 148` past
   the `140 → 142` correction filed elsewhere on this board. The oracle verifies the
   *port*; nothing verifies the *doc row*, and this is a re-run, not a measurement.

8. ~~**`tools/fmt-census.py`'s site census is unguarded.**~~ **CLOSED 2026-08-09,
   `wt/gateall`** — see the registration sweep. `fmt/census-bank` pins all fourteen figures
   of row 4 against `tests/fmt/census-bank.json`, `fmt/census-bank-known-positive` proves
   the comparison is live, both are in `tests/must-run.txt`, and `CORPUS` is now an explicit
   roster that fails in both directions rather than a glob. **The figures did not move**:
   the site census scans each file's own text and never follows an `#include`, so the
   amalgamation never double-counted it. What the amalgamation *does* inflate is the
   `--arenas=` row below, and that one was re-taken on `wt/arenaretake`: the factor is
   **2.8636**, the counts deflate by it, the share does not move, and
   `fmt/arena-census-bank` now fails on a double-counted dump.
9. **The self-compile census total is not deterministic.** Three runs of the identical
   command gave `iterations` = 52,152,515 / 52,152,453 / 52,152,533. Every loop count, every
   `par=` column and every printed percentage is stable; the drift is a few dozen iterations
   in one `not-analyzable` loop, from the randomized temp-dir output path. **Quote 52.15M,
   never an exact figure** — the previously banked 52,077,202 is not reproducible to the
   digit and neither is any successor.
10. **`--alias-oracle` numbers are properties of a ceiling, not of a workload.** They now
   coincide with the shipped predicate on both corpora, which retires the old "never quote
   80.60% without 1.39% beside it" — but the discipline it encoded is what section 3 of the
   verdict is about, and it now applies to `is_float` instead: **never quote 80.60% without
   1.45% beside it.**

**The 13 figures that have failed to reproduce**, kept as a list because the pattern is
the finding. This sentence is the only statement of the count in this file, and
`docs/refs` (`tools/docref-lint.py`) derives the number from the table's own rows and fails
if the two disagree — so the count moves when a row is added, or the cell goes red:

| figure | actual | how it failed |
| --- | --- | --- |
| `+168` blocks unblocked by `snprintf` | 246, then 242, now **86 (0.825%)** | prose only, no script |
| `12,901 / 78.01%` internal-only | 803 / 73.49% | ad-hoc pass, 16× |
| `16,537` Invoke-blocked | 10,238 | unreconstructible, hazard 4 |
| `65.75%` / `1.88%` parallel-legal | **0.01%** | one `memset`-shaped loop, fixed at `415b736c` |
| `140/162` formats accepted | 142, now **148** | the census port had drifted from `mcc_fmt_compile` |
| `~4.3` points of `kept` | **+2.60** | measured before a correctness fix removed 1.7 of them |
| `storeval-rot` "0.0000% of emitted instructions" | −2.31% stage-1, −0.232% stage-2 | **a correctly computed number of the wrong quantity**: a geomean over unmoved binaries |
| `25,700` Invoke-blocked / `246` = 0.96% | **10,423 / 86 = 0.825%** | re-taken twice: the ratio moved at the first re-take and the ranking survived; the second re-take de-duplicated the corpus, and **only the counts moved** |
| `cmake-debug` registers `9106`, same as `cmake-cross` | **8972 vs 9136** unless cross is built first | configure-order dependence, hazard 5 |
| `reg-color` `gain 0.1796`, ranked on `interp` +1.9263 | **0.0017**, `gain_movers` **0.0019**, `cost-no-gain` | re-run 2026-08-09; the tree moved since `1ad3f1aa`, not a tool defect |
| `if-conversion-abs` `gain +0.1905`, `branchy` +3.1843 | **−0.0334**, `branchy` **−0.5700** | a **sign flip** — the flag now makes `branchy` worse. Same cause |
| `narrow` / `tree-copy-prop` are "ranked on nothing" | both priced in `levelpins.txt` on the self-host axis | a **stale** `levelbench.tsv` row read as an **unmeasured** one; see the correction below |
| `idiomgate` covers "4 of 37 config macros" | **17 of 37**, now **29 of 37** | the `4` counted rule-*firings*, not macros reached; fourteen correct `#if MCC_CONFIG_MACHO_CHAINED_FIXUPS` scored zero |

Three more, smaller, recorded so they are not re-quoted: `self` `kept` `83.122 → 91.960` is
**83.090 → 91.978**; `wide` `93.006 → 96.653` is **92.881 → 96.597**; the self-compile's
`bases-may-alias-indirect` at `0.71%` is today **0.00%** (one loop, 32 iterations) — the
0.71 was `bases-may-alias`, a *different* reason row, now 0.72%. None of these is a
regression: every `rir-coverage` measurement sits at or above its banked floor and the cell
passes. They are stale prose.
### The measurement-tool audit — 2026-08-09, `wt/benchtrap`

**Seven** headline figures had failed to reproduce *when this audit was taken on
2026-08-09*; the list has grown since and its current count is the table above, not this
number. At least two of those seven failed because a **tool** was wrong rather than a
transcription. The `storeval-rot`
`0.0000%` (debt #6a) and the `fmt-census` drift are the two. This section is the sweep that
followed: every committed measurement tool read for four specific failure modes, with the
cheap fixes landed on this branch and the rest filed precisely. **No pin value changed and
nothing was re-measured.**

The four modes, because naming them is most of the work:

| mode | the shape | why it survives review |
| --- | --- | --- |
| **null experiment** | a statistic computed over an empty or degenerate subject set and printed as a number | `0.0000` from "measured, effect is zero" and `0.0000` from "there was nothing to measure" are the same six characters |
| **second implementation** | the tool reimplements compiler logic in Python instead of asking the compiler | it agrees on the day it is written, and drifts silently after |
| **hand-pinned constant** | a frozen number presented in output alongside measured ones | nothing in the output distinguishes the two |
| **silent denominator** | a share whose population is "whatever compiled" or "whatever the dump contained" | the ratio stays plausible while the corpus moves under it |

#### LANDED — `tools/optlevel-bench.py`, the defect that started this

`gain` is a geometric mean of dynamic `instructions:u` over the 17 `tests/runtime` kernels.
When a flag changes **zero kernel objects**, that geomean runs over 17 pairs of
bit-identical binaries and returns `0.0000` — correctly computed, of a quantity nobody
measured. That is the cell the ladder write-up quoted as "`storeval-rot` … changes 0.0000%
of emitted instructions" for a flag that costs **2.31% of stage-1**.

Verified as filed: of `tests/optfire/levelbench.tsv`'s 47 rows, **24 change no kernel
object** (23 measured plus the excluded `inline-functions`) and **11 of the 23 carried a
`-0.0000` gain**. Any of the eleven was quotable exactly as `storeval-rot` was.

What landed:

- A **`no-kernel-subject` bucket**. `cost-no-gain` is an assertion that a gain was measured
  and found absent; these rows never had a subject, so they no longer claim one.
- Every kernel-derived column — `gain_pct`, `efficiency`, `text_kernels_pct`, `best_kernel`,
  `best_kernel_pct` — reads **`n/a`** on such a row, and on an `error` row, where the
  configuration never built and its `0.0000` cost columns and `0` fire counts were equally
  fabricated.
- A new **`kernels_moved`** column, `0/17` … `17/17`. The honesty axis was already there as
  `fires_kernels`, and it was a comma-separated name list that read as blank; a fraction in
  its own column is what a reader scanning `gain_pct` will actually see.
- `tests/optfire/levelbench.tsv` **relabelled in place** — 23 rows to `n/a` and the new
  bucket, one error row to `n/a` throughout, header note recording exactly what changed.
  Row order changed with the bucket rank, so the `levelbench.tsv:47` anchor in debt #6a now
  points at line 51. **No value was edited and nothing was re-run.**
- Two new ctest cells, on the `fmt/census-oracle-known-positive` pattern:
  **`optbench/null-subject`** runs `optlevel-bench.py --selfcheck` over a synthetic
  three-row table (a flag that fires in the corpus but moves no kernel, a flag that really
  moves one, and a configuration that would not build) and fails if any kernel-derived
  column of the first or third parses as a float; **`optbench/null-subject-known-positive`**
  asserts the clean run is green, then re-runs with `--mutate`, which restores the old
  zero-reporting behaviour, and fails if the check still passes. The mutated run reproduces
  the original defect exactly: `nullflag gain_pct reads 0.0001`. Neither cell needs `perf`
  or a built `mcc`, so both are `must-run`; `fmt/census-oracle` and its known-positive were
  added to `tests/must-run.txt` at the same time, having been missing since they landed.

**What the eleven rows actually measure now.** All eleven are `no-kernel-subject`: their
`cost_self_pct` / `cost_corpus_pct` / `text_self_pct` / `fires_corpus` columns are real
measurements of the *compiler*, and nothing in this table says anything about the emitted
code. Eight of the eleven — `fmov-imm`, `gcse`, `zero-initialized-in-bss`, `tree-dse`,
`storeval-calllast`, `chain-store-live`, `narrow-fix`, `tree-switch-conversion` — fire on
1–9 of 293 corpus objects, so they are plausibly genuinely inert on numeric kernels and are
fine once labelled. Three need a different instrument, not a different label:

| row | why the kernel table structurally cannot see it |
| --- | --- |
| `storeval-rot` | already priced on the right axis: 2.31% of stage-1 and 0.232% of stage-2 `instructions:u`, and 8.7 points of `rir-coverage` `kept` if it is turned off. See debt #6a; the row stays at 1 |
| `narrow` | 15 corpus objects and a **2.17% `cost_self`** — the second-largest compile-time cost in the whole table with no emitted-code reading at all. It is a compiler-side transform priced only on the compiler. ~~and the ladder currently ranks it on nothing~~ **that last clause was FALSE — see the correction below** |
| `tree-copy-prop` | 15 corpus objects, 1.26% `cost_self` / 0.70% `cost_corpus`, same shape, same correction |

The instrument those three want is the one debt #6a used: a stage-1/stage-2 self-compile
read under `instructions:u`, i.e. `tools/selfhost-optbench.py`, not a numeric kernel. That
is filed below, not done — it is a measurement, and this was an audit.

**CORRECTED 2026-08-09 (`wt/ladder2`): the last sentence of the `narrow` row above was
false, and it is the kind of false this board exists to catch.** Neither `narrow` nor
`tree-copy-prop` was ever "on the ladder on the strength of a number that was never taken".
Both are priced in `tests/optfire/levelpins.txt` on exactly the stage-1/stage-2 axis this
section recommends — `narrow|10|stage-2 -0.60% cpu, counter agrees (-0.016%), and 1.91% of
stage-1 on top` (`:196`) and `tree-copy-prop|11|1.53% of stage-1 cpu time, stage-2 unmoved`
(`:227`). What is `n/a` in `levelbench.tsv` is a **stale row for a flag the kernel table no
longer sweeps at all**: `narrow` is `MCC_OPTD_LEVEL(10)` and `tree-copy-prop` is `LEVEL(11)`
(`src/mccopt.h:66-67`), both far outside `optlevel-bench.py`'s `<= 3` filter, so the 2.17%
`cost_self` quoted above was measured in a generation when they still sat at levels 1–3.
The kernel table is not their instrument and has not been since `893c1e84`. Reading a
stale row's `n/a` as "unpriced" is the mirror image of reading a null row's `0.0000` as
"measured zero" — same file, same trap, opposite direction.

They were re-priced anyway, because the pins deserved a re-take. **Both levels are
confirmed and neither moved.**

| | `narrow` (rung 10) | `tree-copy-prop` (rung 11) | how |
| --- | ---: | ---: | --- |
| stage-1 `instructions:u` | **+0.8762%** | **+0.7991%** | n=25 paired, IQR 0.0024% / 0.0029%, **25/25 reps positive** |
| stage-1 CPU | +1.3925% | +1.4984% | same run, control **+0.093%**, in band against the 0.55% floor |
| stage-2 `instructions:u` | **−0.0088%** | **−0.0048%** | **25/25** and **24/25** reps negative, control +0.0010% |
| stage-2 CPU | — | — | **not quoted**: the control read −0.479% against a 0.435% floor, out of band |
| stage-2 object | byte-identical | byte-identical | correctness gate, `md5 2f48cd51…`, all four arms |
| `rir-coverage` `kept` | 83.087 / 91.912 / 91.977 / 91.977 | 83.089 / 91.913 / 91.978 / 91.978 | control 83.089 / 91.913 / 91.978 / 91.978, all three PASS |
| verdict | **100:1 against** | **166:1 against** | vs the **19:1** that got `chain-store` refused |

Both effects are real and both are tiny. The sign is believable *because* hazard 3 was
checked: `narrow` moves `kept` by **0.001** points and `tree-copy-prop` by none, against
`-fchain-store`'s **−7.9** — neither is buying its stage-2 number by making bodies
unfaithful and skipping the strategy suite. And neither is a rung-12 no-op: both change the
stage-1 object (`1f3fa3d9…`, `31f93151…` against a base of `0d6e5cae…`), which is also the
evidence that the passes still fire.

The one number that did not reproduce is `narrow`'s own banked pin: `levelpins.txt` had
`stage-2 -0.60% cpu, counter agrees (-0.016%), and 1.91% of stage-1`, and this run reads
about **half** of each (−0.0088% counter, +0.876% stage-1). `tree-copy-prop`'s banked
`1.53%` of stage-1 CPU reproduces at **1.4984%**. The pin rows now carry both.

Method, and why not a committed tool: `optlevel-bench.py` filters to `level <= 3` and both
flags are at 10/11, and it only ever takes a shipped flag *out* — these two ship **off** at
`-O3`, so the experiment is **add-one-in**, which is the arm `selfhost-optbench.py` has but
behind the same `<= 3` filter. So, by hand, exactly as debt #6a was:

```
FLAGS=$(python3 -c "import json,shlex;cc=json.load(open('cmake-debug/compile_commands.json'));\
r=[x for x in cc if x['file'].endswith('/mcc.c')][0];\
print(' '.join(a for a in shlex.split(r['command'])[1:] if a.startswith(('-I','-D'))))")
# stage-1, one arm; the base arm is the same line without the -f flag
perf stat -e instructions:u,task-clock cmake-debug/mcc $FLAGS -O3 -fnarrow -c src/mcc.c -o o1.o
cmake-debug/mcc o1.o cmake-debug/CMakeFiles/mcc.dir/mccrt_blob.c.o \
    cmake-debug/CMakeFiles/mcc.dir/mccjit_blob.c.o -o mcc1 $(cat cmake-debug/selfhost-link-libs.txt)
# stage-2, the fixed job
perf stat -e instructions:u,task-clock ./mcc1 $FLAGS -I runtime/include -O2 -c src/mcc.c -o o2.o
```

Each read gets a **private, cold `XDG_CACHE_HOME`** (opt-slice memoises to
`~/.cache/mcc/sl-*.ck`, so a warm cache measures the cache). Arms are interleaved within a
rep and compared **pairwise per rep**, not median-to-median: the box was carrying other
agents at load 5–34, and a paired sign test over 25 reps is what makes a −0.0088% effect
readable at all. A first attempt at n=21 had to be discarded — its stage-1 CPU control read
**+1.225%** against the 0.55% floor with 27–59% within-arm spread, which is exactly the
"repeat until the control is in band" the previous ladder agent described. `rir-coverage`
needs a directory of symlinks to `cmake-debug` whose `mcc` is a wrapper appending the flag,
since it scrubs `MCC_TEST_OPT` and has no switch for extra `-f` knobs.

**~~The dilution hazard the fix does NOT close, filed rather than half-done.~~ CLOSED
2026-08-09 (`wt/ladder2`), by re-running the bench.** `gain` was a geomean over **all 17**
kernels even when only one moved. `chain-store` read `0.1169` against its own `sieve` delta
of `1.9658` — a real ~2% win diluted ~17× by sixteen pairs of identical binaries. The
arithmetic reproduces exactly: `1.020052^(1/17) = 1.0011686`, i.e. `0.1169`, so the
mechanism was never in doubt, only its size.

What landed: a **`gain_movers_pct`** column (geomean over the kernels the flag actually
moved) and an **`eff_movers`** column beside the old ones, which are kept because they are
a different quantity rather than a wrong one — `gain_pct` is the movers figure diluted by
this kernel set's reach, and it is a payoff estimate only if you believe these 17 kernels
are your program mix, which nothing establishes. **The ladder ranks on `gain_movers_pct`,
and the table is sorted by `eff_movers`.** The bench was re-run rather than backfilled,
because per-kernel deltas are not in the TSV.

Measured dilution on the fresh table, up to **17.4×**: `builtin-math-prepass` reads
`0.3007` all-kernel against **`5.2372`** over the one kernel it moves; `builtin-copysign`
`0.0590` against **`1.0076`**; `builtin-math-fabs` `0.8065` against **`7.0664`**.
**`trunc32` is the control that proves the mechanism**: it moves 17/17, so its two gain
columns agree to the digit (`0.6005` / `0.6005`). The columns diverge exactly and only as
reach falls short of total, which is what a dilution is.

**The part the filing got wrong: it is not only a magnitude error, it flips buckets.**
`has_gain` thresholds the gain at `GAIN_NOISE = 0.10`, and it was thresholding the *diluted*
number. So `builtin-copysign`'s real **1.0076%** win on `mathfun` arrived as `0.0590%` and
was filed **`cost-no-gain`** — the bucket whose whole meaning is "a gain was looked for and
found absent". A true measurement was being filed under a claim of absence, which is the
same failure this section was written about, one layer down. `has_gain` now tests
`gain_movers`, and `--selfcheck`'s `diluteflag` row is the known-positive: it fails if
bucketing goes back to the diluted number.

**A third defect, hazard 2's own bullet 3, was still live in the fresh table and is now
fixed too.** `efficiency` was `+inf` for any flag that cleared the gain floor at near-zero
cost, tested on `abs(gain)` — so a flag that makes emitted code *worse* for free was ranked
top. In the fresh run `inline-functions` (`gain_movers` **−1.9557**) sorted to **rank 4**
with `eff_movers = inf`, above every flag that pays. Efficiency now carries the sign of the
gain (`copysign`), and `--selfcheck`'s `worseflag` row gates it.

#### LANDED — the rest of the sweep

| tool | landed on `wt/benchtrap` |
| --- | --- |
| `tools/loop-census.py` | `--corpus runtime` with an empty roster **fails** instead of printing a complete report of `0.00%`. Both headline fractions print **`n/a`** rather than `0.00%` on an empty denominator, as do the two "without the hottest program" lines. `report()` fails outright when zero iterations were executed. The corpus mode's `runtime totals` line now sums each program's **real** `[trip-tot]`, where it used to synthesize one from the tally itself with `exits=0 overflow=0` hard-coded — a tautological cross-check that also asserted no loop ever exited and nothing was lost to the `LC_MAX` id cap. `THRESHOLDS` is cross-checked against the buckets the instrumented binary actually used (`lc_thr[]` in `runtime/lib/loopcensus.c` is a second copy of the same six numbers). Plus the `BREAKEVEN` provenance banner in hazard 1 above |
| `tools/runtime-bench.py` | `--gates` took `NAME=VALUE` **environment** knobs and put them in the child's environment. The optimizer knobs moved to argv (`MCC_OPT_ROW`), so every `--gates` configuration compiled the **identical binary** and the delta columns reported one program against itself — a `+0.0%` instruction column that is maximally convincing and completely empty. `GATE_WINS` was moved to flags when this bit; this path was not. It now takes flags and **refuses** the retired spelling, the way `tools/selfhost-smoke.py` does. Separately: a run where every kernel's *reference* build or run failed printed `runtime-bench: OK (0 kernels x 1 config(s), output verified vs gcc)` and exited 0 — `runtime-bench-check` going green having compiled nothing with `mcc`. That is now a failure |
| `tools/node-census.py` | `KINDS` was a positional retype of `enum AstKind`; it is now diffed against `src/mccast.h` on every run. Inserting a kind mid-enum shifted every label by one with no other symptom (plausible percentages, wrong names, and a bank comparison diffing mismatched kinds); appending a 15th silently shrank the **denominator** and inflated every share including the `external_invokes_on_cpu` ratio the tool ratchets on. Also, `make_dump` discarded the compile's return code and tested only that the file existed — `MCC_ARENA_DUMP` is append-mode and line-buffered, so an ICE part-way through leaves a valid-looking partial dump, and the bank compares **ratios**, which survive truncation. A nonzero compile is now a hard failure |
| `tools/slice-census.py` | `src_fail` was counted and never checked, and reached stdout only through `report()`, which `--quiet` suppresses. `slice-enum` is `--quiet --min-slices 100` over 12 pinned sources: 11 of the 12 could stop compiling and the cell would pass on the twelfth with no mention of the loss. `src_fail > 0` and `fn_n == 0` now fail |
| `tools/selfhost-fixpoint.py` | **the same argv/environment bug as `runtime-bench --gates`, and it had disarmed four ctest cells.** Trailing arguments were turned into environment variables; the four cells that drive this file pass `-f` spellings, so `-fdivmagic` became an environment variable *named* `-fdivmagic`. `selfhost-fixpoint-gates` and the three `selfhost-fixpoint-memmodel-*` cells were byte-for-byte reruns of the plain cell while printing `knobs=[...]` as if they were not. Flags now go on argv and a non-flag argument is refused. Confirmed live: `selfhost-fixpoint-gates` now produces `o1=3411935` against the plain cell's `3413759`, i.e. the twelve flags demonstrably reach the compiler, and the fixpoint still holds |
| `tools/untyped-probe.py` | `nodes=0` printed `0.000%` unknown and exited 0. The docstring calls that percentage "the denominator that has to go to zero first", so **a completely broken run rendered as the goal being achieved** — the return code of the probe compile was never checked either. Now a hard failure |
| `tools/shadow-iv-sweep.sh` | `clean=` counted anything that did not print the divergence string, including hard compile errors, missing sysroots and `timeout` kills, and the vacuity guard tested `clean > 0` — so a sweep in which nothing compiled printed `attempts=610 clean=610 divergences=0  PASS`. `clean` and `failed` are now separate and both reported. **On this tree 20 of 610 attempts were in the wrong column**; see the filed item on the missing floor |
| `tools/strategy-ledger.sh` | the `awk` block ends `print "FAIL: ... the ledger is vacuous"; exit 1`, and it was the **left side of a pipe into `sort`**, so its exit status was discarded and under `set -eu` the pipeline reported `sort`'s 0. The FAIL was a printed string, not a failure. It now runs to a file and the status is checked |
| `tools/slicerun.c --census` | the only one of the three report modes in that function with no zero-subject exit — its siblings already say `FAIL (the cost table has no rows)` and `FAIL (no real slice became schedulable work)`. An empty or unparseable dump printed ten `census:` lines of zeros and exited 0. `blocks == 0` now fails |
| `tools/opt-cache-determinism.py` | see the LOST SUBJECT item below — it now counts what was actually written to `XDG_CACHE_HOME` and refuses to make its claim over an empty one |
| `tools/opt-determinism.py` | `--runs 1` printed `determinism: OK (1 runs of X byte-identical)` having compared nothing against anything. `--runs < 2` is now refused |

Full `ctest` after all of it: **9138 cells, 0 failures** — the 9136 baseline plus the two new
`optbench/null-subject*` cells.

#### LANDED — `optlevel-bench.py --check`, and the last two unregistered number-producers, 2026-08-09 (`wt/gatefin`)

This closes hazard 2's remaining debt and the two cheapest rows the registration sweep left
open. **No pin value in `tests/optfire/` changed.** Full `ctest` after it: **9149 cells, 0
failures** — the 9143 baseline plus six, and `cmake-cross -N` and `cmake-debug -N` still
agree at 9149.

**1. `--check` has two halves because the gap had two halves, and the second one is the one
that was easy to miss.** The arm was missing *and* CMake wrote the fresh TSV to
`${CMAKE_BINARY_DIR}/levelbench.tsv` and never compared it back to `tests/optfire/`. Fixing
only the first would have left a `--check` nothing ran against a fresh run. Both are closed:

- `optlevel-bench.py --check` with no `--mcc` is the **ladder half** — it needs neither
  `perf` nor a built compiler, reads `tests/optfire/levelbench.tsv` against `src/mccopt.h`,
  and reports three shapes: a row naming a flag no longer at levels 1–3 (the shape 32 of the
  old 47 rows had), a row whose `level` column drifted off what ships, and **a shipped rung
  with no row at all**. The third is the one that reads as a zero: a rung nothing priced
  looks exactly like a rung that cost nothing, which is the mirror of the null-row defect
  the `--selfcheck` arm already gates.
- `optlevel-bench.py --check --fresh PATH` adds the **value half**. The `optlevel-bench`
  cell now passes `--check --bank …/tests/optfire/levelbench.tsv`, so the ladder bench's own
  build-dir output is compared back on every run, and `--check` refuses outright if `--out`
  and `--bank` resolve to the same file — comparing the bank against a run that just
  overwrote it is the vacuous pass this was meant to prevent.

**The tolerance is stated, and most of the comparison does not use one.** `level`, `bucket`,
`kernels_moved`, `fires_corpus` and `corpus_total` are compared **exactly**: none of the five
is a timing. Two are decisions and three are counts of objects whose sha256 changed, and no
tolerance on a gain column can express "this row now measures a different thing". The
gain/cost columns are
compared against `max(0.05 pp, 10% relative)`. The floor is 100× the self noise and 80× the
corpus noise this table's own header reports (0.0005% and 0.0006%, the base configuration
measured twice), so counter jitter cannot meet it; the relative band is what one
host's instruction counts may differ from another's without the ranking moving. A banked
`n/a` against a fresh number, or the reverse, is **always** a failure regardless of tolerance
— that is exactly the `storeval-rot` shape, and it is a change in what was measured.

**2. The known-positive, `optbench/levelbench-bank-known-positive`.** `--check --mutate`
rebuilds the 1ad3f1aa generation in memory — it renames one row onto a flag at rung 11,
drifts one row's level, and deletes one shipped rung's row — and then requires `--check` to
report **all three** shapes, not merely a nonzero count. If any shape goes unreported the
tool prints `BLIND:` and exits **0**, so the cmake driver's mutated-arm assertion fires.
Verified by pointing the driver at a stub that always exits 0:

```
CMake Error at cmake/optlevel_bench_bank_mutate.cmake:19 (message):
  optbench/levelbench-bank-known-positive: the table was put back to a
  generation stale -- a row naming a flag no longer at levels 1-3, a row
  whose level drifted off what src/mccopt.h ships, and a shipped rung with no
  row at all, which is the shape 32 of the old 47 rows had and the shape that
  made narrow/tree-copy-prop's stale rows get read as unmeasured -- and
  --check still passed, so it is comparing nothing
```

And verified against a **real** tree change rather than a stub: moving `trunc32` from
`MCC_OPTD_LEVEL(1)` to `(2)` in `src/mccopt.h` (reverted) turns `optbench/levelbench-bank`
red with `FAIL trunc32: the table says level 1, src/mccopt.h ships it at level 2. The row was
measured against a ladder that no longer exists`, and turns the known-positive red on its
*clean* arm with "the unmutated check is already failing, so this cell cannot say anything" —
which is the two-sided guarantee behaving as designed.

**3. The two cheapest remaining registrations.** Both were named last round as
number-producers with no cell. Both now have one and a known-positive, and both grew a
77 path so a host that cannot run them says so instead of failing for the wrong reason.

| cell | subject | known-positive |
| --- | --- | --- |
| `opt-determinism` | 4 runs of `src/mcc.c` at `-O3`, byte-identical objects. Takes its `-D`/`-I` set from `compile_commands.json` via a new `--from-build`, the same idiom `opt-cache-determinism.py` already uses, so the subject is the program the build compiles and not a guessed approximation | `--mutate` compiles run 0 at `-O0` and the rest at `-O3`, so the objects genuinely differ and the tool must say so. A determinism gate that passes over objects that differ is comparing a file against itself, and its `OK` line is the strongest-looking vacuous pass in the tree |
| `untyped-probe` | `unknown`/`knownvoid` shares at `-O0..-O3` over the self-compile — the denominator the equivalence-oracle work has to drive to zero | `PROBE_ENV=MCC_RIR_PROD=0` stops the compiler emitting the `[rir-untyped]` record the probe reads, so `nodes=0`. The probe's existing zero-subject guard must fire; if it exits 0 the driver fails |

Both known-positives were proved to fire the same way, by pointing their driver at a stub
that always exits 0:

```
CMake Error at cmake/opt_determinism_mutate.cmake:22 (message):
  opt-determinism-known-positive: run 0 was compiled at -O0 and the rest at
  -O3, so the objects genuinely differ, and the tool still reported every run
  byte-identical. ...

CMake Error at cmake/untyped_probe_known_positive.cmake:20 (message):
  untyped-probe-known-positive: MCC_RIR_PROD=0 stops the compiler emitting
  the [rir-untyped] record the probe reads, so nodes=0 and every share has an
  empty denominator -- and the probe still exited 0. ...
```

`untyped-probe.py` also stopped tracebacking on a missing `compile_commands.json`, a missing
`mcc` or a `compile_commands.json` with no `mcc.c` record — all three are now 77 with the
reason, because reconstructing the flag set is the whole point of reading that file and a
guessed one would census a different program.

Manifest rows: `optbench/levelbench-bank` and its known-positive are **`must-run`** (python3
and the source tree, nothing else). `opt-determinism` and `untyped-probe` and theirs are
**`registered`** — each has an `mcc_skip_test` arm for emulated and cross builds, which is
why `cmake-cross` and `cmake-debug` both register 9149 rather than diverging.

#### LOST SUBJECT — `opt-cache-determinism` has been passing over an empty cache

Not a tool defect and worth its own heading, because the mechanism is one this board has
not seen before: **a ladder demotion silently removed a cell's subject and the cell went on
passing.**

`tools/opt-cache-determinism.py` compares four cache states and asserts all four objects are
byte-identical, concluding "the disk cache is a side channel, not an input to codegen". The
cache in question is the opt-slice memo, `sl-<target-salt>.ck`. `src/mccopt.h` carries
`opt-slice` at **`MCC_OPTD_LEVEL(9)`** — off at every shipped level. So at `-O3`, which is
what the ctest cell runs, **nothing is ever written to `XDG_CACHE_HOME`**, all four objects
are trivially identical, and the tool printed its strongest claim on the strength of there
being no cache.

Measured today, with the subject restored:

```
python3 tools/opt-cache-determinism.py cmake-debug/mcc src/mcc.c \
        --opt=-O3 --from-build cmake-debug -- -fopt-slice
cold        ba19af67…   self  ba19af67…   foreign-tu  ba19af67…   foreign-fl  81178595…
cache entries written under XDG_CACHE_HOME: 2
FAIL: the object depends on the optimizer's disk cache state
```

**The defect the tool was written to catch is still there.** It is simply unreachable from
the shipped ladder. The cell now counts cache entries and returns **77 with a message that
names this as a lost subject rather than a pass** — it does not fail, because turning a
long-green cell red over a pre-existing compiler defect is a different decision than this
audit gets to make. Two things follow and neither is done here:

1. Someone owns `-fopt-slice`'s cache-state dependence, or the pass is deleted. A rung-9
   flag whose off-state is the only tested state is not a demotion, it is a dead pass with a
   live bug in it.
2. `opt-cache-determinism` should be a `registered` row in `tests/must-run.txt` **and** a
   permanent 77 should be visible as such. Right now the tree reads it as green.

#### FILED — found, not fixed, with what a reader would wrongly conclude

Ordered by how much a currently-quoted number depends on it.

1. **`BREAKEVEN` is still hand-pinned, and un-pinning it is not a five-line job.**
   `tools/loop-census.py`, `BREAKEVEN` / `THRESHOLDS`. Sixteen printed statistics and two
   JSON fields are scored against it, including both headline fractions. The banner now
   says so, which is the honest half. The real fix is blocked on a second copy: the
   numerator is accumulated **inside the instrumented binary**, bucketed by `lc_thr[]` in
   `runtime/lib/loopcensus.c`, so a measured break-even of 310 cannot be used without the
   C side learning it too. Estimated: ~15 lines of C (read the thresholds from an env var
   at first `__mcc_loop_census_enter`, `LC_NGE` stays 6) plus ~25 of Python (run
   `slicerun --cost-synth`, parse — it is already tab-separated with `nodes` in column 1
   and `breakeven_ntuple` in column 6, and its `SIZES[]` land on exactly the node keys
   `BREAKEVEN` uses — and fall back to the pinned table with a loud banner). **And it
   cannot be gated:** `cost_synth` returns 77 without a device and `slice/cost` carries
   `SKIP_RETURN_CODE 77`, so on every GPU-less box the only cell that could regenerate the
   table silently skips. *A reader concludes `52.51% raw / 0.01% parallel-legal` was scored
   against a break-even measured on the machine that produced it.*
2. **`loop-census.py`'s node count is a Python model of `ast_count`.** `loop_nodes` /
   `calibrate_bytes_per_node` estimate a loop body's node count from its byte extent, and
   that estimate is what picks each loop's `BREAKEVEN` row — so it sits directly under the
   headline. It is avoidable and cheaply: `ast_loop_par_census` already runs at
   `ast_func_end` with the arena live and already emits `[loopar] id= par= why=` per loop.
   Adding `nodes=%ld` to that one `fprintf` deletes the estimator, the calibration pass and
   its extra full compile, and the `toks` fallback. One line of C plus a `parse_map` key.
3. **No cell covers the statistic `loop-census.py` publishes.** `loop-census-control` is a
   genuinely good ground-truth cell — nine hand-derived expectations at all four `-O`
   levels, a row-count check, a negative control (no flag, no data) and a perturbation
   (move a bound, the numbers move) — and its CMake comment and `must-run.txt` row are both
   correctly scoped to "the trip census". But `--selftest` calls neither `report()` nor
   `tally()`, so `breakeven_trips`, `loop_nodes`, `weighted_num`, `par_num`, both headline
   fractions and the entire `--corpus runtime` path have **no control at all**.
   `known_trips.c` has known trip counts *and* known body sizes, so `--selftest` could
   compute `weighted_num` over it against a hand-checked value in ~10 lines. Also
   `loop-census-parallel` and `node-census` are not in `tests/must-run.txt`, so a permanent
   77 on either is invisible.
4. **`rir-coverage.py`'s ratchet denominator is "the files that happened to compile".**
   Compile failures are counted and printed but never gate, and every banked ratio
   (`modelled_coverage`, `kept_coverage`, `coverage`) is over the survivors. Dropping files
   shrinks `text`, `fn_bytes` and `residual` **together**, so the residual reconciliation
   does not fire. Worse for the `wide` corpus: it is an `os.walk` of seven directories with
   no manifest, and the bank records `corpus_config` (the `-D` axis) but **not** the file
   list, count, or a hash — so any commit that adds a `.c` test file moves every banked
   percentage with no signal, and `--update-bank` then locks the drift in. The docstring
   promises the opposite ("a build that differs exits 77 rather than reporting the dilution
   as a regression"); that promise covers only the `-D` axis. `slice-enum` pins its 12
   sources explicitly in `CMakeLists.txt` — that is the right pattern and `wide` does not
   use it. *A reader concludes "kept coverage 91.960% of the corpus"; the true statement is
   "of the part of the corpus that still compiles at this level".* Cheap first step: bank
   `sources=N` and fail when it moves.
5. **`rir-coverage.py` prints seven historical numbers at runtime as if this run produced
   them.** The partial-skip message is a block of string literals containing `100.000%`,
   `120`, `82.520`, `92.92-93.01` and the `96.156`-vs-`83.219` story, and one of them is
   introduced by the word *measured*. *A reader of a ctest log attributes them to this run
   on this host.* They are not recomputed and cannot go stale visibly. Delete or recompute.
6. **`rir-coverage.py`'s `--nofb-probe` passes vacuously.** With zero byte-divergent bodies
   it prints its summary and exits 0, which is the same output as probing nothing; the bank
   already holds four empty `nofb_miscompiles` lists and the cell passes no floor. Same
   family: `--check-gap-dir` and `--check-low-dir` iterate `os.listdir` and return 0 on an
   **empty** fixture directory, and the gap fixtures cover 3 of the 18 `UNF`+`WHY` classes.
7. **`LOW_EXCLUDE = "src/mccgpu.c,src/mccgpu.h"` is a filename suffix match with no
   count.** It removes the GPU emitter from both numerator and denominator of the lowerable
   percentage, `print_lowerable` announces `denominator: %d arena-modelled bodies` without
   mentioning the excision, and the compiler reports no excluded-body count. A change to
   the constant is caught by `corpus_config`; **renaming or splitting `mccgpu.c` is not** —
   the exclusion silently stops matching and the ratchet then compares a full-corpus number
   against an excluded-corpus floor, which is a fake regression with no diagnostic.
8. **`node-census.py`'s bank never compares `bodies` or `nodes`.** It holds
   `bodies: 2765, nodes: 429114` and checks only that no kind went to exactly zero, that
   invokes did not appear from nothing, and one one-sided ratio with a `0.05` tolerance —
   against a documented dilution of `0.1381pp`, i.e. 2.8× the tolerance. *A reader concludes
   `node-census: OK` means the census covered the same corpus as the bank.* With the
   partial-dump hole now closed the exposure is smaller, but the floor is still absent.
   Related: `ceilings()` re-derives device eligibility in Python by treating every non-Invoke
   node as eligible, which is not what `mcc_slice_work_from_ast` accepts;
   `slicerun --census` already computes this properly at block granularity. So
   `external_invokes_on_cpu 99.254%` is a call-density statistic, not a ceiling — exactly
   the criticism the file levels at its *sibling* ratio without noticing it applies to the
   gated one.
9. **`runtime-bench.py`'s `GATE_WINS` ratchet has had no subject in this checkout for as
   long as `vendor/` has been absent.** Its one entry points at `vendor/plb/spectral-norm`,
   which does not exist here, so `runtime-bench-gatewin` takes `return 77` on every run and
   `SKIP_RETURN_CODE 77` makes that green. *A reader concludes a passing
   `runtime-bench-gatewin` re-confirmed the `chain-store` 8% win.* A `registered` row in
   `tests/must-run.txt` would have surfaced this immediately; it is the strongest candidate
   in the tree for one. Its thresholds are pinned too — `GATE_WIN_NOISE_MAX = 0.12` accepts
   a box drifting 11.9% against itself as a valid measurement of an 8% win — and nothing
   re-derives them.
10. **`runtime-bench.py --assert-baseline` asserts over whatever intersection exists.** The
    only stored baseline holds **5** kernels against `KERNELS`'s 20, and two of the five
    need the absent `vendor/plb`. Kernels missing from either side print `-` and count as
    neither regressed nor improved, with no floor on the intersection size — contrast
    `fmt-census.py`, which fails when its corpus drops below 1000. *A reader concludes a
    passing `--assert-baseline` means no kernel regressed.* Also note `KERNELS` declares 20
    and the board's row 6 says "the 17 in-tree kernels"; the tool prints only a count, never
    the roster, and its JSON carries neither.
11. **`fmt-census.py` residues.** (a) `AST_BB = 0` / `AST_INVOKE = 11` / `AST_NONE` are
    hand-pinned duplicates of `AstKind` used by the `--arenas=` path, which the oracle does
    not cover — that path produces the `25,700 Invoke-blocked / 246 (0.96%)` row of the
    table above, and one enumerator inserted before `AST_Invoke` silently reclassifies every
    block. (This is the same defect just fixed in `node-census.py`; the fix transplants.)
    (b) Three port branches have no subject in either corpus half: the `L` length modifier
    (`%Lf` — `slicerun.c`'s own refusal table *does* cover it, so the C side is tested and
    the port side is not), the `j` and `t` modifiers, and the space flag. (c)
    `largest accepted program: %d words` prints `0` when nothing was accepted.
12. **`tools/selfhost-optbench.py --check` passes when nothing was derived.** `levels` is
    populated only when the greedy stage runs, so `--check --stage marginal` prints nothing
    and returns 0; and when greedy *does* run, `derive_levels` assigns every unmeasured flag
    `levels[f] = levels_now[f]` with the note "nothing measured to move it on" — so a
    degenerate run in which every flag came out `inert` produces `levels == levels_now`
    identically and the tool prints *"src/mccopt.h matches the ladder derived from these
    measurements"* over **zero derivations**. This board already carries the caveat
    "`selfhost-optbench --check` was not re-run"; the tool cannot distinguish "re-run and
    agreed" from "re-run and measured nothing". Same file: the docstring says **48**
    level-assignable flags in five places and the live `flag_table()` yields **16**
    (`MCC_OPTD_LEVEL(1)` ×10, `(2)` ×6, `(3)` ×0 — the other 33 sit at rungs 10/11/12), the
    count is never printed or written into the TSV header, and there is no floor on
    `len(names)`; with an empty flag table `full` and `empty` collapse to the same cache key
    `"(empty)"` and the "all-on vs all-off" header reports a `0.0000` delta between one
    measurement and itself. Also `pct(a, 0)` returns `0.0`, and a flag with an empty sample
    list gets `gain = 0.0` and classifies **`inert`** — "this flag changes nothing" is the
    recorded verdict for "this flag was never sampled".
13. ~~**`tools/idiomgate.c`'s invariant covers 17 of 37 config macros, from two hand-typed
    lists.**~~ **CLOSED 2026-08-09 on `wt/idiomcov`.** Coverage **17/37 → 29/37**; the
    remaining 8 are refused by name in the tool. See *LANDED — `tools/idiomgate.c`'s
    denominator* below for the derivation, the corrected reading of the `4`, the five
    idiom violations the widened check found in the tree, and what each newly covered
    macro now guarantees.
14. **`tests/optfire/optfire.sh defstate` cannot tell "off by default" from "does not
    exist".** An unrecognised option is a *warning* in `src/libmcc.c`, not an error, so a
    deleted or renamed flag compiles fine, the two objects match, and `got=off`. Thirteen
    rows of `defstate.txt` assert `off` and **every one would stay green if its flag were
    removed from `src/mccopt.h`**. `cdelta` mode is immune because it also requires
    `c1 > 0`. Adjacent: `flagsweep.sh accept` derives its flag list with a `sed` that
    requires a literal leading tab in `mccopt.h`; reindent that file and `n=0` and the cell
    prints `PASS flagsweep-accept: 0 flags accept -f and -fno-`. `cover` mode has the
    corresponding guard; `accept` does not — and `accept` is the only thing standing between
    a renamed flag and vacuously-green `flagsweep-exec`, `flagsweep-cover` and `defstate`.
15. **`tests/optfire/stratsweep.sh` drops subjects when the compiler under test is wrong.**
    A subject is admitted only if its `-O2` baseline already matches its `-O0` reference —
    deliberate and well argued in the header — but the consequence is that a broad `-O2`
    regression *removes subjects from the sweep* rather than failing it. `$WORK/skipped` is
    written and never printed or counted, and the only floor is `n > 0`, so a miscompile
    breaking 30 of 31 subjects yields `PASS stratsweep-iso all: 22 strategy/ies x 1
    subjects`. Same family: `flagsweep.sh` drops a subject on a failed `-O0` build before
    `ran` is incremented, floor `ran > 0`. Both print the survivor count; neither pins it.
    (`stratsweep.sh check` is the *good* pattern and is clean: `STRAT_NAMES` is a second
    implementation of `ast_strategies[]` and the mode exists precisely to gate it against
    `src/mccast.c` name-for-name and in order.)
16. **`tools/shadow-iv-sweep.sh` still has no floor on its failure count.** The `clean`/
    `failed` split landed above, and on this tree the sweep reports `attempts=610 clean=590
    failed=20 divergences=0`. Those 20 are almost certainly subjects that legitimately need
    flags the sweep does not pass, but nothing pins the number, so a regression that stopped
    500 of 610 building would still print `divergences=0` and PASS. Pin it or classify the
    20.
17. ~~**Four shell tools grep for `ast_env_gate`, which no longer exists in `src/`.**~~
    **CLOSED 2026-08-09 (`wt/envgate`).** All four are repointed at `src/mccopt.h`, and the
    thirteen stranded `*.gated.*` bank files are regenerated and registered. See *LANDED —
    the gated half of `ast/o0-baseline`* below for the history, the option taken and the
    cell. In short: the gates were not renamed, they were **converted** — `a55c0a07` turned
    112 `MCC_AST_*`/`MCC_RIR_*` environment gates into `-f` flags generated from one table,
    so the successor of `ast_env_gate("MCC_AST_…", o4 || s1->optimize >= 1)` is *"every
    `MCC_OPTD_LEVEL(n)` row of `src/mccopt.h`, spelled `-f<name>`"* — **54** rows on this
    tree. The derivation now reads the table that *defines* the knobs rather than a second
    copy of it, so it cannot go stale the way the old regex did. The old residues, resolved:
    the all-or-nothing guard is no longer the only guard (`o0_ab.sh` additionally requires
    the gated counters to differ from the ungated bank's, because **mcc ignores an unknown
    `-f` silently** and a zero-count check cannot see a wrong-but-nonempty list); and
    `c2_sweep.sh`'s claim of parity with a non-existent `cmake/rir_parity.cmake` is gone
    with the comment that made it.
18. **`tools/spvgate.c --arenas` lacks the zero-comparison guard its built-in-case mode
    has.** The case mode gained `FAIL (0 defined points compared -- proves nothing)` after
    exactly this incident; `arena_mode` prints `compared=` and gates only on `dispatches`,
    so a run where every dispatched point was vacuous prints `compared=0 vacuous=N
    mismatches=0` and `spvgate: OK`. The *cells* are protected (`cmake/spvgate_real.cmake`
    requires `slices=[1-9]` and a red `--mutate` arm); the hazard is the hand-run
    invocation, which is how every arena figure quoted on this board was taken.
19. **`tools/xsuite-report.py` prints `0.0%` for suite/opt pairs that never ran.** `tally`
    is a `defaultdict` indexed over the full `suite × opt` cross-product, so a suite that ran
    only at `-O0` gets an `-O3` row of zeros and a `0.0%` rate — a reader scanning the table
    sees a total-failure suite where nothing ran. Separately the rate is pass-of-*admitted*:
    `SKIP`/`REFSKIP` are removed from the denominator, so a change that skips more tests
    **raises** the reported rate. The board carries that caveat in prose ("26,281 of 47,919
    files are skipped by directives before anything runs"); the tool's own column is labelled
    simply `rate`.
20. **`selfhost-smoke.py`'s second correctness check is optional and silent.** If
    `tests/exec/programs/quicksort.c` is absent the whole array/sort check vanishes with no
    message and the tool still prints "produced correct executable**s**", leaving `fib(10)`
    — which exercises essentially no codegen — as the entire gate. *A reader concludes a
    green `selfhost-smoke` validated the self-hosted compiler on real array and loop code.*
    Everything else about this tool is **clean**: the brief's worry that it degrades when
    run from the wrong directory does **not** reproduce — `root` comes from `__file__`, every
    subprocess passes `cwd=root`, missing pieces all `sys.exit` loudly, non-flag knobs are
    rejected outright, compile flags are lifted from `compile_commands.json`, and all four
    `subprocess.run` calls use `check=True`. Its one hand-pinned constant, the `["-lm",
    "-ldl"]` fallback when `selfhost-link-libs.txt` is absent, produces a loud link failure
    rather than a wrong number.

#### Clean bills of health, because those are results too

- **`selfhost-smoke.py`** — clean apart from item 12 above.
- **`loop-census.py`'s parsers.** `parse_map` / `parse_trips` classify nothing. `par=` and
  `why=` come only from `ast_loop_parallel_legal`; `?` is never collapsed into `0` and the
  three counts are reported separately. `runtime_corpus()` **imports** `runtime-bench.py`'s
  `KERNELS` rather than restating it — the right defence against corpus curation. A kernel
  that fails to build **aborts** rather than shrinking the denominator. The six
  fixed-threshold tables are `BREAKEVEN`-independent, which is a real escape hatch from
  item 1. `--selftest` and `--partest` are genuine ground truth with negative and
  perturbation controls.
- **`rir-coverage.py`'s core.** `parse_report`/`merge` are pure accumulation of
  compiler-emitted fields. The self-reconciliation gates (`unf_sum != fallback`,
  `body != used+fallback+skip`) turn an accounting drift into a hard failure and are the
  tool's strongest property. A **missing** bank is a failure, not a pass — most census tools
  get this backwards. The `checked`/`skipped` ledger is precisely the anti-null-experiment
  discipline this audit is about: every skip names what it dropped, `return 77` happens only
  when `checked` is empty, and a pass prints `PASS: %d comparison(s) enforced`. Items 5–7 are
  places that discipline was not applied, not places it is wrong.
- **`slice-census.py`'s `verify_fn`** is the best measurement code in the family: structural
  invariants derived from the definition, checking the compiler against itself (coverage
  monotone under transparency, a call-free body is exactly one slice covering all attributed
  bytes, `inv_graft == 0 ⇒ t=1 ≡ t=0`). It cannot pass by being inert. Byte extents come
  from the compiler's replay counter, not from a Python model. Both tools scrub inherited
  `MCC_*` variables so an ambient setting cannot alter a measurement.
- **`runtime-bench.py`'s counter layer.** `counter_backend` demands a **non-zero count from a
  real workload** rather than trusting `which()` — exemplary. `run_once_cpu` uses `wait4`
  rusage with an honest write-up of why wall-clock lied; `baseline_report` returns `None`
  rather than a number when nothing is comparable, which is correct handling of "no
  subject"; `baseline_snapshot` refuses to bank milliseconds; output is checked on every run
  so fast-but-wrong fails; and the table tells the reader in as many words to judge on the
  instruction columns because a time delta with no instruction delta is layout.
- **`fmt-census.py`'s oracle** — see hazard 4. It also has an explicit corpus floor, which
  is the single most transplantable idea in this audit.
- **`tools/must-run.py`** is the model for the whole family. Every degenerate input is an
  explicit refusal: an empty manifest exits 2 (*"the manifest is empty, so it asserts
  nothing"*), a `ctest -N` that listed nothing exits 2 (*"refusing to report a clean manifest
  against an empty build"*), no `testcase` elements exits 2, and the docstring states
  *"Never 77: a manifest that cannot be checked is a failure, not a skip"*.
- **`tests/optfire/cover3.py`** — `verify` re-derives from `mccopt.h` rather than trusting
  `gen`, checks row width and alphabet, checks each pinned column is genuinely constant, and
  walks all C(k,3)×8 triples. **`tests/optfire/stratsweep.sh check`** exists specifically to
  gate a second implementation (`STRAT_NAMES` against `ast_strategies[]`, name for name and
  in order) — that is what a disclosed second implementation should look like.
  **`tools/gate-ledger.sh`** runs a *control* gate the compiler never reads and subtracts its
  run-to-run noise from every real gate, and prints the floor rather than hiding it.
  **`tools/arm64pe_diff.py`**, **`tools/selfhost-o3.py`**, **`tools/embed-jit-smoke.py`** and
  **`slicerun --cost-synth`** are clean.
- **`cmake/slicerun_musl.cmake`** has all five teeth in one place — a corpus floor, a
  dump-exists check, non-zero `slices`, non-zero `frame-compared`, non-zero `frame-mem`, and
  a `--mutate` known-positive. Every LANDED fix above and filed items 6, 18 and the lost
  subject are a missing instance of one of those five. **It is the template.**
- **`tests/optfire/levelbench-cycles.tsv`** — `cycles_adjudicate` measures only the kernels a
  flag actually moves and says "changes no kernel object; nothing to adjudicate" otherwise.
  The tool always knew; only the TSV writer did not.

#### Not touched, and why

`tests/optfire/leveltime.tsv` reports a `gain_pct` for rows whose `stage1_obj` column reads
`identical` — a stage-2 time over a byte-identical compiler, i.e. the same null experiment.
It is left alone deliberately: the column **is** there, the header explains that those rows'
"true effect is exactly zero, so the spread of what they measured IS the floor", and
`levelpins.txt` records that the file is "a record of one campaign's numbers, not a running
ledger". Relabelling it would edit a campaign record. The `n/a` rule should apply to
whatever regenerates it next.

### The skip audit — 2026-08-09, `wt/skipaudit`

The suite reports **9151 cells, 0 failures, 424 skipped**, and until this branch nobody had
read the skip list. A skip is the same defect class the last four rounds kept finding,
wearing a different hat: in a green run *"not applicable on this host"* and *"this stopped
working and someone disabled it"* render identically. Worse, the audit found a second hat —
**cells whose skip path returns 0**, which render as *Passed*.

**The first thing to record is that ctest's own record of a skip carries no reason.** Every
one of the 424 `<skipped>` elements in the JUnit XML — the artefact the board quotes and the
artefact `tools/must-run.py --results` consumes — says either `SKIP_RETURN_CODE=77` or
`SKIP_REGULAR_EXPRESSION_MATCHED` and nothing else. The reason is on the cell's stdout or
stderr, which ctest discards for a non-failing test. Reconstructing the table below required
re-running all 424 cells under `ctest -V`. **`mcc_skip_test` does report why** (it echoes
`SKIP: <reason>` and matches on it), and so do the `dockergate.sh`, `ts_skip()` and
`tests/runner.c` paths; the loss is in the results file, not the cells.

#### The breakdown, 424 cells, x86_64 Linux, `cmake-cross` built first

| category | cells | what it is |
| --- | ---: | --- |
| **Environmental — needs a host, device or toolchain this machine is not** | **219** | |
| `exec-*/{arm64,arm64_encoding,arm64_errors,arm64_extasm,fastcall,riscv_asm,winarm64_interlocked}` | 154 | 7 goldens × 22 pipelines; `cpu=`/`os=` mismatch |
| `*-docker` | 25 | `docker info` fails, or the bind mount is invisible inside the container |
| macho/Darwin: `macho-*`, `cli/macho_*`, `run-tier/*-osx`, `cli/apple_arm64_long_double_is_double` | 18 | needs a Darwin host (or `-DMCC_DARWIN_HOST=ON`) |
| `diff3/*` wrong arch, or `<2` reference compilers could build the source | 15 | 6 arch/OS, 9 where gcc 15 and clang 22 in their default configuration reject the program |
| Windows: `run-tier/arm{64,}-win32`, `run-tier/arm-wince`, `compile.win32`, `pe-native-conformance` | 5 | wine emulates x86 PE only and qemu-user cannot load PE |
| `tls-shared` | 1 | arm64 ELF only |
| `preprocess/expansion/standard_example` | 1 | impl-defined divergence, gcc != clang, so there is no consensus to compare against |
| **Environmental, but the prerequisite exists in the checkout** | **32** | |
| `cross/*`, `run-tier/{i386,arm,arm64,riscv64}`, `selfhost-qemu-*`, `selfhost-arm64-native`, `run-parity-*` | 32 | `vendor/gentoo-stage3-*` is `.gitignore`d, so **every git worktree skips all 32** unless `vendor/` is symlinked in. They run in the primary checkout. This is not a defect in the cells; it is a property of how the tree is branched, and it is worth knowing before quoting a skip count taken in a worktree |
| **Permanently disabled by design, documented, never runs on any host** | **147** | |
| `exec-*/{inline,backtrace,btdll,alias,array_assignment,stdcountof_header}` | 132 | 6 goldens × 22 pipelines carrying a `note:` req, which `req_met()` fails unconditionally. All six notes were checked; all six still hold. The four that delegate coverage elsewhere name cells that exist and run (`cli/c99_inline_emission_matrix`, `exec/alias_single_tu`, the `bound_*`/`builtins` goldens, which do run because `MCC_TEST_BCHECK=1`). `stdcountof_header`'s claim was re-verified against this host: gcc 15.3.0 still has no `<stdcountof.h>`, clang 22.1.8 does |
| `diff3/*` `note:`, `bcheck`, and `#ifdef __MCC__` | 14 | 6 + 4 + 4; `portable_req()` rejects mcc-only bounds-checking and mcc-only source outright |
| `asm-gas-directives` | 1 | a permanent 77 carrying a named blocker: the integrated assembler lacks `sgdtq`/`sidtq`/`swapgs` (`gas_directives.S:811`) |
| **Opt-in by design, off by default** | **9** | |
| ~~`rir-coverage-census`, `rir-nofb-probe` (`MCC_RIR_CENSUS`), `loop-census`, `loop-census-numeric` (`MCC_LOOP_CENSUS_RUN`), `slice-census` (`MCC_SLICE_CENSUS_RUN`)~~ | ~~5~~ **0** | **`wt/censusfix`: all five are armed by their own registration and none of them skips any more.** The five cost 40 s of wall clock between them, which was never the reason they were opt-in — the `--opt-in` flag exists so that a developer typing the tool by hand is told what it costs, and CMake simply never set what the flag asks for. The tools keep the flag; the cells set the switch |
| `jit/selftest-{observability,bench,benchwire}` | 3 | `-DMCC_DEV=ON` |
| `flagsweep-cover` | 1 | `-DMCC_FLAGSWEEP_FULL=ON`, then `ctest -L flagsweep-full` |
| **Lost subject, already filed and already carrying a manifest row** | **2** | `opt-cache-determinism`, `runtime-bench-gatewin` |
| **THE FINDING — silently disabled** | **15** | `ast/rir-c2-*` (14) and `superopt-perfn-cache` (1); both below |

**The docker row is not a stable 25.** Re-running the suite the docker cells split their
reasons between *"docker daemon not available"* and *"docker cannot see `<path>`"* — two
mutually exclusive claims, since the second requires the first to have succeeded. Under
`-j32` `docker info` times out and the cell reports the first; run less loaded, some of the
same cells get further and one (`selfhost-riscv64-docker`) runs to completion and passes.
A skip that is a function of machine load is a skip that will read as *"not applicable
here"* forever. Filed as item 5 below.

**One caveat on the opt-in row, because the verification recipe hides it.** `-L census`
selects **6** cells, and `MCC_RIR_CENSUS=1 ctest -L census` reports 6 run and 0 failed — but
`MCC_RIR_CENSUS` only arms two of them. `slice-census` wants `MCC_SLICE_CENSUS_RUN=1` and
`loop-census`/`loop-census-numeric` want `MCC_LOOP_CENSUS_RUN=1`, so three of the six still
77 inside a run whose headline says the census label is green. `MCC_SLICE_CENSUS_RUN` is
additionally named **nowhere else in the tree** — not in this file, not in the README — so
`slice-census` is a cell with a manifest row and no findable switch.

**Closed on `wt/censusfix`.** The recipe is now plain `ctest -L census`: **7 cells, 0
failed, 0 Skipped**, no exported variable. The `6` in the paragraph above was itself two
different things added together — five census cells plus the `mcc_build` fixture ctest pulls
in for `FIXTURES_REQUIRED` — which is why "6 registered, 3 run" was so easy to read as
"6 run". `census/gates-armed` now makes the shape unrepresentable: it fails if any
`census`-labelled cell passes `--opt-in` to a tool whose opt-in guard reads a variable the
cell's `ENVIRONMENT` does not set, and it refuses to pass at all if the label selects fewer
than five cells.

#### THE FINDING — `ast/rir-c2-*`: fourteen cells, a banked ratchet, and a macro nothing ever defined

`tests/ast/rir_c2.cmake` is registered fourteen times (`-O1/-O2/-O3` plus eleven cross
targets) with **`BANKGAP=0 BANKSKIP=0 BANKFN=1150`** — a real three-way ratchet on the C2
re-emit: how many function bodies the Replay-IR C2 arm can re-emit byte-identically to what
the parser produced. Every one of the fourteen skips, on every host, with:

> `rir_c2: c2try=0 on the probe — the C2 re-emit needs a -DMCC_REPLAY_IR_C2=1 build; SKIP`

`MCC_REPLAY_IR_C2` is a compile-time macro that `src/mccrir.c:12-13` defaults to `0`, and
**no CMakeLists, preset, workflow or toolchain file in this tree has ever defined it to 1.**
The skip condition is therefore not a host fact. It is unsatisfiable by construction, and it
has been quietly true for the whole life of the cells. `docs/TODO.md` does not mention
`rir-c2` anywhere: fourteen cells and a pinned bank that no board entry has ever quoted.

**Re-enabled, it is red.** Built with `MCC_EXTRA_CFLAGS=-DMCC_REPLAY_IR_C2=1` (native
`x86_64`, 305 sources: `tests/exec/**.c` + `tests/diff/full_language.c`):

| level | population | c2ok | gap vs banked `0` |
| --- | --- | --- | ---: |
| `-O1` | `srcs=305 ok=295 fn=1309 faithful=1274` | `1257/1271` | **14** |
| `-O2` | `srcs=305 ok=295 fn=1309 faithful=1274` | `1258/1271` | **13** |
| `-O3` | `srcs=305 ok=295 fn=1309 faithful=1274` | `1258/1271` | **13** |

```
CMake Error at tests/ast/rir_c2.cmake:110 (message):
  rir_c2: gap 14 against a banked 0 — 14 more body(ies) whose C2 re-emit
  does not reproduce the parser's bytes
```

`c2skip=0` and `c2err=0 c2invalid=0` throughout, so the fourteen are not declined re-emits
or crashes: they are `bytes=1 len=13` at `-O1` — one body whose re-emitted bytes differ and
thirteen whose length differs. `fn=1309 >= BANKFN=1150`, so the population is not the
explanation; the gap is.

**Landed on this branch:** `MCC_REPLAY_IR_C2` is now an `mcc_config_node` (BOOL, default
OFF, group Development) that appends `MCC_REPLAY_IR_C2=1` to `_mccdefs`, so the switch the
skip message names is a real, discoverable, documented build option instead of a macro you
have to know to inject through `MCC_EXTRA_CFLAGS`. **The default is unchanged and no bank
was re-pinned** — re-banking `0` to `14` is exactly the move that makes a ratchet mean
nothing, and the whole point of the cell is that somebody has to look at the fourteen. Three
`registered` rows are in `tests/must-run.txt` so the family cannot be deleted quietly.

**Not chased here:** which fourteen bodies, and whether the C2 arm regressed or was banked
optimistically. `BANKGAP=0` was pinned by somebody who had the macro on; nothing in the tree
records when, and `git log` on the bank is the next step.

#### THE SECOND FINDING — `superopt-perfn-cache` asked the compiler a question it could not hear. **RE-ENABLED, and green**

`superopt-perfn-cache` drives `mccharness perfncache`, which compiles a three-function
program three times (cold / warm / one function edited) and requires the per-function
superopt checkpoint cache to report **0, then 3, then 2** functions cached. It reported:

> `SKIP: superopt per-function search never ran (driver fell back to a plain compile)`

The diagnosis was wrong, and in the most expensive direction: the search *was* running.
`perfn_run()` invoked `mcc -O13 -v` and scanned **stdout** for `superopt-perfn:`. The line is
emitted by `mcc_logf_v(s->verbose, MCC_LOG_DEBUG, ...)`, which is bit 6 of the verbosity mask
and writes to **stderr** — `-v` sets bit 0. So the harness looked for a line at a verbosity
that does not include it, on a stream it does not carry, concluded the feature was absent,
retried four times, and skipped. `MCC_LOG=64` would not have helped either: `mcc_log_enabled_v`
consults `s->verbose` only and ignores the `MCC_LOG` floor.

Two-character fix in `tools/mccharness.c` (`-v` → `-v64`, and swap the stdout/stderr
buffers). The cell now runs and passes, asserting the real thing:

```
[DEBUG] superopt-perfn: 3 functions (0 cached) in 77ms, total .text 233
[DEBUG] superopt-perfn: 3 functions (3 cached) in 9ms, total .text 233
```

#### THE THIRD FINDING — twenty-three registrations whose skip path reported *Passed*

These never appear in the 424 at all. They are in the 8727 that passed.

| cell(s) | the shape |
| --- | --- |
| `jit/arm64-{dispatch,counter,kgc,kgcfp}` | registered with **no `set_tests_properties` at all** — no `SKIP_RETURN_CODE`, no `SKIP_REGULAR_EXPRESSION` — and all three skip paths in `tests/qemu/jit_arm64_*.sh` `echo "SKIP: …"` then **`exit 0`**. On any host without clang or `qemu-aarch64` (i.e. most CI images) four arm64 JIT validators report Passed having compiled nothing. They do run on this host, which is why nothing ever noticed |
| `superopt/promote-floor` | declares `SKIP_RETURN_CODE 77` and **never emits 77**: a missing subject `continue`s the loop and the script exits `$rc` = 0. A vanished `tests/superopt/src/spillheavy.c` would read as a pass |
| `gpu/ladder-gpu-parity` | `cmake/ladder_gpu_parity.cmake` sees `available=0`, prints *"no usable GPU device, skipping"* and `return()`s — exit 0. This cell is in `tests/must-run.txt` as the CPU/GPU oracle parity gate |
| `gpu/spv-slice-known-positive`, `gpu/spv-slice-real`, `gpu/msl-slice-*`, `slice/*-known-positive`, `slice/*-lohi-fallback`, `slice/musl`, `opt-determinism-known-positive`, `untyped-probe-known-positive` | same shape via `cmake/{spvgate_mutate,spvgate_real,slicerun_mutate,slicerun_musl,opt_determinism_mutate,untyped_probe_known_positive}.cmake`: the child's 77 is caught, a reason is printed, and the driver `return()`s 0. **`slice/musl` is doing this today** — `vendor/musl-src` is absent in a worktree, so it printed *"vendor/musl-src absent, skipping"* and reported Passed in 0.00 sec |
| `tools/slicerun.c` | the device-differential bail was a bare `return 77` with **no message at all** — the only fully silent skip found in the tree |

All fixed on this branch: the `return()`s became `cmake_language(EXIT 77)`, the four
`exit 0`s became `exit 77`, `promote-floor.sh` counts subjects and 77s when it measured
none, `slicerun.c` says why, and all twenty-three registrations gained
`SKIP_RETURN_CODE 77` so the honest answer reaches the results file. On this host every one
of them still passes, because this host has a device, clang, `qemu-aarch64` and (once
`vendor/` is present) `musl-src` — which is precisely why none of it was ever noticed.

#### Also fixed: two cells that skipped without saying why

- **`diff3/*`** — `portable_req()` returned a bare 0 and the caller `continue`d **printing
  nothing**, so 29 diff3 cells reported Skipped with no reason recoverable even under `-V`.
  It now fills a reason buffer and the caller prints `SKIP  <name> -- <why>`, with a
  separate `not-portable` column in the summary line so it is not conflated with
  `ref-cant-build`.
- **`tests/ci/target-link-gate.sh`** — skipped with the message `no cc`. It now names the
  compiler `$CC` resolved to.

#### Verified

`cmake-cross` built before `cmake-debug` was configured (hazard 5). `ctest -N` registers
**9151**. Full `ctest -j32`: **9151 cells, 0 failures, 390 skipped** — the 424 baseline
minus the 32 `vendor/` sysroot cells, minus `superopt-perfn-cache` and `i386-fastcall-abi`,
which this branch put back. Nothing turned red that was not red before, and the only cell
this branch could have turned red is `ast/rir-c2-*`, which is left skipping on the shipped
default with its bank untouched. `ctest -L flagsweep` 119/0, `-L stratsweep` 30/0,
`MCC_RIR_CENSUS=1 ctest -L census` 6/0 **with three of the six still Skipped** (see the
caveat above), `python3 tools/must-run.py --build cmake-debug` 64 rows satisfied, and
`python3 tools/selfhost-smoke.py cmake-debug` OK.

#### Filed by this sweep, not fixed

1. **The results file still cannot carry a reason.** Everything above had to be recovered by
   re-running 424 cells under `-V`. The cheap fix is a `tools/must-run.py --skip-audit`
   mode that classifies a JUnit file against a declared taxonomy and fails on an
   unclassified skip — the same shape as the manifest itself, applied to skips rather than
   registrations. Not built here; this section is its first output, produced by hand.
2. **Nine `diff3/*` cells skip because of the runner's fixed build line, not the host.**
   `old_func`, `grep` and `types` compile cleanly under `gcc -std=gnu17` and fail only
   because gcc 15 defaults to `gnu23` (K&R `()` now means `(void)`); `ternary_op` fails only
   on gcc 14+'s promotion of `-Wint-conversion` to an error; `atomic_aggregate` and
   `atomic_inlang_rmw` fail at **link** for want of `-latomic`. That is five three-way
   consensus cells lost to a toolchain default drifting under a hard-coded command line —
   the same shape as the `-O0` bank, one layer down. `alignas`, `builtin_inf_nan` and
   `asm_lvalue_cast` are genuinely non-portable and should stay skipped.
3. ~~**`MCC_SLICE_CENSUS_RUN` is undocumented** and `-L census` reports green while three of
   its six cells 77. Either document the two missing switches next to `MCC_RIR_CENSUS`, or
   give the label one arming variable.~~ **Done on `wt/censusfix`, by the third option this
   line did not consider: the label arms its own cells, so there is no switch left for a
   reader to know about.**
4. ~~**Registration gates with no `else()` skip stub** — cells that *vanish* rather than skip,
   which is the class `tests/must-run.txt` was built for and still cannot see: 86
   `stratsweep/isofull-*` and `stratsweep/perm3-*` under `MCC_STRATSWEEP_FULL`, the whole
   `qemu-*` block under `MCC_QEMU_TESTS`, `selfhost-jit` and `embed-jit-smoke` under
   `MCC_PYTHON3`/`MCC_EMBED_JIT`, and the seven `macho-libsystem/*` cells whose OFF-branch
   stub is registered under the *unrelated* name `macho-libsystem-kernel-fused`, so no
   manifest row can name both states.~~ **CLOSED on `wt/regstub`.** See the section below;
   the list above was a quarter of it, and the 164 the class is named after were **still
   vanishing** when this branch started.
5. **The 24 in-container `apt-get … || exit 77` sites** in `tools/*-docker.sh`. A transient
   network failure silently converts a real differential test into a skip. They are honest
   about *what* failed; nothing distinguishes "no docker on this host" from "the mirror was
   down for 30 seconds", and only the first is a legitimate skip.
6. **`tools/spvgate.c`'s `VK_HOST` macro** skips with `spvgate: no usable vulkan host, line
   %d rc=%d` — a source line number and a raw `VkResult`. It is the one remaining skip
   message in the tree that a reader cannot act on.
7. **`vendor/` is `.gitignore`d, so 32 cells skip in every worktree.** Any measurement taken
   on a branch — which is how every one of the last five rounds was taken — silently loses
   the whole cross/qemu/sysroot tier unless somebody symlinks `vendor/` in. Worth a line in
   the build instructions at the head of this file rather than a fix. **The 424 baseline this
   section audits is a no-`vendor/` number**; with `vendor/` symlinked in, the same tree and
   the same commit report **390**, still 0 failures.

#### Registration gates with no `else()` — closed on `wt/regstub`, and it was 537 cells, not 93

The class is the one that ate 164 cells (144 `optfire-*` + 20 `*-docker`) once already. The
fix credited with closing it, `mcc_cross_cc()`, **did not close it**: it made the gate's
condition true more often, by letting `cmake-debug` find `cmake-cross`'s binaries. The
`if(UNIX AND NOT _ofx_cc STREQUAL "")` around the optfire cross loop still had no `else()`,
so on a host with no cross build at all the same 144 cells still evaporated. That is
measurable, and it was measured before anything was changed.

**The measurement.** A deliberately capability-poor configure — `PATH` reduced to a curated
bin with no `docker`, no `wine`, no `qemu-*`; `-DMCC_CROSS_DIR=<empty dir>`;
`-DMCC_VENDOR_DIR=<empty dir>` — against the same commit as a fully-equipped one:

| configure | `ctest -N` before | `ctest -N` after |
| --- | ---: | ---: |
| fully equipped (cross built, docker, wine, qemu, `vendor/`) | 9161 | **9397** |
| capability-poor (none of the above) | **8624** | **9397** |
| gap | **537 cells silently absent** | **0** |

`comm` over the two sorted name lists is now empty in both directions. Only the pass/skip
split moves: the equipped tree reports **9397 cells, 0 failures, 620 skipped**; the
capability-poor tree registers the same 9397 names, with the extra ones registered as
`SKIP: <reason>` echoes rather than as nothing.

What the 537 were: **144** `optfire-{arm64,i386,riscv64}/*` and **20** `*-docker` — the
original 164, still gone; **296** `diff3/*`, **41** `preprocess/*`, **33** `parts/*` and
**7** `fuzz/*`, which collapsed into a single `diff3-suite` / `preprocess-suite` /
`parts-suite` / `fuzz-suite` stub each, so 377 named cells were represented by 4 names no
manifest row could match. On top of those, gates that hold on this host but drop cells
elsewhere: 86 `stratsweep-full`, 75 `flagsweep-cover/*`, 59 `qemu-*`, 51 `jit/selftest-*`
under `MCC_EMBED_JIT`, 26 selfhost/census cells under `MCC_PYTHON3`, 15 bench/determinism
cells under a second `MCC_PYTHON3`, the 7 `macho-libsystem/*`, plus `sanitize-*`,
`gpu/spv-*`, `gpu/msl-*`, `ubsan/*`, `cli/*`, `mccbuild`'s six, and
`superopt/promote-floor`.

**The rule that stops it coming back** is `tools/regstub-lint.py`, registered as
`ci/registration-stubs` and modelled on `census/gates-armed`: structural, not a list. It
parses `CMakeLists.txt` and `cmake/*.cmake`, recovers which variables are *probe-rooted* —
reachable by assignment or by an enclosing gate from `find_program`/`find_path`/
`find_library`/`find_file`, `find_package`, `execute_process`, `option`,
`mcc_config_node`, a `set(... CACHE ...)`, `if(TARGET ...)`, or an `if(EXISTS ...)` on a
path outside the source tree, including through out-parameters of functions such as
`mcc_cross_cc()` — and requires every `if/elseif/else` chain that registers a test and
consults one to **register the same set of cell names on every branch**, the implicit
absent `else()` counted as the empty set. Names are compared as written, `${...}` included,
which is what catches a stub filed under a name unrelated to the cells it stands in for.
It exempts exactly two things and says so: the chain containing `enable_testing()`, and
variables that name *the target that was asked for* rather than the host's equipment
(`MCC_CPU`, `MCC_TARGETOS`, `CMAKE_CROSSCOMPILING`, `WIN32`, …) — two targets are two
suites; a host with docker and the same host without are one suite. It reads **48**
capability-gated chains and never exits 77. Run against `d67f16b5`, the commit this branch
started from, it reports **37 of those 48 broken** — the four filed instances were a tenth
of the population.

Ablated by putting back the old `macho-libsystem-kernel-fused` stub name and deleting the
`else()` that stubs the `stratsweep-full` rows, it fails:

> `regstub-lint: CMakeLists.txt:5047: if(MCC_STRATSWEEP_FULL) registers`
> `` `stratsweep/isofull-${_ss_row}` `` `and has no else(). It is gated on`
> `MCC_STRATSWEEP_FULL, declared by mcc_config_node at CMakeLists.txt:1271, so where that`
> ``does not hold the cell is not registered at all -- invisible to `ctest -N`, to``
> `tools/must-run.py and to every count in docs/TODO.md. Add an else() that`
> `mcc_skip_test()s the same name with a reason`
>
> `regstub-lint: CMakeLists.txt:7329: if(MCC_DARWIN_HOST AND CMAKE_SYSTEM_NAME STREQUAL`
> `"Darwin") is gated on MCC_DARWIN_HOST, declared by option at CMakeLists.txt:7328, and`
> `` its if branch at line 7329 does not register `macho-libsystem-kernel-fused`. Every ``
> `branch of a capability gate registers the same cells; …`
>
> `regstub-lint: 2 of 48 capability-gated registration chain(s) drop cells instead of`
> `skipping them`

and pointed at a tree with no gates at all it refuses to report a pass:

> `regstub-lint: only 0 capability-gated registration chain(s) found, expected at least 30.`
> `Either the parser stopped early or the probe-rooting analysis collapsed; a check that`
> `inspected nothing must not be reported as passing`

**Counts that moved, and why.** `ctest -N` 9161 → **9397**. `-L stratsweep` 30 → **116**
(116/0, 86 Skipped — the opt-in tier is now visible as 86 skips instead of absent).
`-L flagsweep` 119 → **193** (the single `flagsweep-cover` stub became the 75 real row
names, 193/0 with 75 Skipped). `-L qemu` 14 → **73** (73/0, 59 Skipped). `-L macho` 18 →
**25**. `-L census` stays **7**, 7/0 with nothing Skipped. `must-run.py` 64 → **66 rows
satisfied** (a row was added for `ci/registration-stubs`). `tools/selfhost-smoke.py
cmake-debug` OK. Anything quoting 9161, 119 or 30 is now stale.

**What the lint deliberately does not catch.** A gate whose skip branch registers a single
wildcard name such as `${_tn}` cannot be distinguished statically from one registering the
whole family; the qemu block was rewritten by hand for that reason. And the two identity
axes above are exempt by design, so an arm64 host and an x86_64 host may still register
different counts — that is a different suite, not a lost cell.

#### THE FOURTH FINDING — `vendor/`'s mingw shadows a working reference. **RE-ENABLED, and green**

Symlinking `vendor/` in did not only recover 32 cells; it *lost* one, which is how it was
found. `i386-fastcall-abi` passed without `vendor/` and reported

> `SKIP: no gcc`

with it. `mcc_mingw_resolve()` finds the vendored
`vendor/winlibs-mingw-w64-16.1.0-ucrt-i686/mingw32/bin/**gcc.exe**` — a Windows PE binary —
and hands it to `mccharness i386fastcall` as `--gcc` on a Linux host, where nothing can
execute it. The `elseif` chain tested `EXISTS`, which a PE file on Linux satisfies. So in the
**primary checkout**, which has `vendor/`, the i386 fastcall ABI differential has been
skipping — and because the same checkout also runs the 32 sysroot cells, the two effects
cancel in the headline and neither is visible in a count.

Fixed by probing the candidate with `--version` at configure time and falling back to the
harness default when it does not run, with a `message(STATUS)` saying so. The cell runs and
passes again.

### The registration sweep — 2026-08-09, `wt/gateall`

The audit above named the defect class: **a recorded number that nothing forces to stay
true.** It fixed the tools. This branch closes the other half — the tools that were correct
and simply unwatched — by making the numbers breakable. **No pin value changed, no
`tests/optfire/*` value was edited, and nothing was re-measured except where a bank was
re-taken and the diff is set out in full below.**

**Five new cells** — 9138 → **9143** — **nineteen new `tests/must-run.txt` rows**, and one
finding that only exists because a bank got read for the first time in six days.

#### THE FINDING — 28 `-O0` objects moved and nothing noticed for 426 commits

`tests/ast/o0-baseline/` was last taken at `bc85ce70` (2026-08-03). `tools/o0_ab.sh` has
never been a ctest cell, so between then and `a2733199` — **426 commits, 209 of them
touching `src/`** — nothing compared anything against it.

Re-taken here. The diff is not the `+N lines, -0` shape a clean re-bank has:

| | |
| --- | --- |
| **+300 lines** | 25 corpus files added to `tests/exec` since the bank, × 12 keys. 277 → 304 files, 270 → 295 objects on `x86_64` |
| **~296 hashes** | objects whose `sha256` moved |
| **−0** | nothing was dropped |

On `x86_64`, 32 of the moves split **4 / 28**. Four are files whose source changed —
`loop_{fusion,interchange,tile}.c` (`adf08e3b`, the `ast_dep_base_distinct` indirect guard
the board's top row is about) and `chained_assign.c`. **The other 28 have byte-identical
sources.**

Those 28 were adjudicated, not assumed. Running *this* `mcc` over the whole tree as it
stood at `bc85ce70` — old sources, old `runtime/include`, which itself moved 10,166 lines
in the window — reproduces **exactly 28 differing hashes and no others, twice**. So neither
the corpus nor the headers explain them: the compiler does. They cluster on float and
complex (`complex*.c`, `tgmath_dispatch.c`, `libm_builtin_fold.c`, `fenv_access_fold.c`,
`flt_eval_method.c`, `fp_wide_return.c`, `floating_point.c`, `math_library.c`), on
predefined macros (`feature_macros.c`, `predefined_macros.c`, `line_directive.c`,
`variadic_macros.c`), on ABI (`int128.c`, `struct_abi.c`, `struct_byval.c`,
`variadic_promotions.c`, `std_short_enums.c`) and on the overflow builtins — which is
exactly where `_Float16` on all five backends, the complex sign-of-zero fix,
`__has_attribute` as a builtin macro, the `gnu23` default and the vector/riscv64 ABI fixes
landed in this window. All 304 files still compile and every `tests/exec` cell over them is
green, so these read as 28 **intended** codegen changes that drifted past a baseline nobody
was reading.

**The point is not the 28. The point is that they arrived as one indistinguishable blob.**
A tripwire nobody runs cannot tell you which commit moved a byte; it can only tell you, six
days later, that something did. With `ast/o0-baseline` registered, each future move lands on
the commit that causes it.

~~Thirteen banked files are deliberately **not** re-taken.~~ **Re-taken 2026-08-09
(`wt/envgate`); see the next section.**

#### LANDED — the gated half of `ast/o0-baseline`, and the thirteen files that held nothing

**The history first, because "the symbol is gone" was the wrong reading.** `ast_env_gate`
was not renamed; it was **converted**. `a55c0a07` (*"one runtime `-f` surface, four
compile-time switches"*, 2026-08-04) turned 112 `MCC_AST_*`/`MCC_RIR_*` environment gates
into `-f`/`-fno-` flags generated from a single table, `src/mccopt.h`, and re-laid `-O` as a
ladder: 1–3 settled, 4–12 one in-development optimizer each, 13+ the strategy search. So the
gated half's *subject* survives in full. At `a55c0a07^` the old regex named **29** gates (not
the "38" every comment claimed); of those, **six** became `MCC_OPTD_ALWAYS` (they need no
forcing — that is `a55c0a07`'s *"the arena replayed WRONG at `-O0`"* fix), **two** became
`MCC_OPTD_SPECIAL`, and the remaining **21** are `MCC_OPTD_LEVEL(n)` rows spread over
`n = 1, 2, 10, 11, 12`. That last spread is why `ARCHIVED.md`'s guess that the successor set
is *"`MCC_OPTD_LEVEL(n)` with `n <= 3`"* is wrong: only 8 of the 21 sit at `n <= 3`, so it
would silently drop 13 members of the very set it claims to reconstruct — `gcse`,
`tree-pre`, `tree-vrp`, `narrow`, the three `sethi-ullman` rows, the three `chain-store`
rows, `call-window`, `narrow-elim` and `tree-const-load`, all of which the ladder moved out
to 10–12. **The successor taken here is every `MCC_OPTD_LEVEL(n)` row, all 54 of them**,
which is the ladder's own definition of "off at `-O0`, on at some `-O`", derived per run and
fatal at zero.

Not taken: the `MCC_OPTD_SPECIAL` rows. Their defaults are expressions, not table entries,
and some are per-target — `reg-disp` is `optimize >= 1` only under `MCC_TARGET_X86_64`, and
the archive records that forcing it on `arm64` fabricates a divergence. A target-independent
`-f` set cannot do that, and the `arm-win32 == arm-wince` twin check passes on both counters
and object `sha256`, which is the cheapest evidence that it did not.

**The option taken was Regenerate, and the reason is that the thirteen files were provably
empty of information.** Before re-banking, the committed `*.gated.*` files were compared to
the *ungated* bank of the day they were taken (`7ae6bec9^`): **all thirteen are
byte-identical to their ungated counterparts** — every key, and `board.gated.txt` to
`board.txt`. Forcing the 29 environment gates on moved not one of the five counters the bank
records on any of the twelve keys. So the frozen half was never a second measurement; it was
a copy, and it had been one since `03e3735b`. Retiring it would have lost nothing — but
regenerating it costs 30 seconds and, for the first time, produces a row that differs.

**The re-bank, split by cause.** All thirteen files move. Two causes, and they are separable:

| | |
| --- | --- |
| **corpus** | `files=277 → 304`, `objects` +25 per key. The same 25 `tests/exec` fixtures the ungated half already absorbed at `7ae6bec9`; this half simply never caught up. |
| **mechanism** | with the corpus held equal — both legs taken on *this* tree, same 304 files, same compilers — the gated leg now differs from the ungated one on **every** key: `fn` +2 (`x86_64`) to +68 (`i386`), `empty` 35 → 37 everywhere, `faithful` +0 to +66, `objects` unchanged, `bar=OK` on all twelve. |

No third cause: the object half (`<key>.obj.txt`) is not re-taken and does not move, because
measurement A never sees `O0_AB_GATES`.

The `x86_64` row moves least (+2) and every cross key moves ~+60. That is a property of the
key's *flags*, not of the knobs: `x86_64` is the only key `o0_ab.sh` runs with no
`-I runtime/include` and no sysroot, so `-freemit-templates` has far fewer inline bodies to
re-emit. Per-file it is the same effect — 82 `[rir-*]` lines differ on `x86_64` even where the
totals nearly agree.

**What protects it now.** **mcc ignores an unknown `-f` silently** — verified,
`mcc -O0 -fnosuchflagzz -c` exits 0 — so "derived a non-empty list" is *not* evidence that
anything was forced on, and the old zero-count guard could not have caught a
wrong-but-nonempty list. `o0_ab.sh` therefore refuses to bank or pass a gated row
whose counters equal the ungated bank's, by name and with the reason. Ablated two ways: the
committed thirteen fail it (that is how they were caught), and `O0_AB_NOGATES=1` derives the
54 knobs and then drops them, producing

> `x86_64: the gated counters are identical to the ungated bank's. 54 knob(s) were passed as
> -f and not one body changed shape, which is what a silently-ignored flag name looks like.`

**The other three tools are repointed too**, at the same table and with the same fatal-at-zero
guard. `tools/c2_sweep.sh` passes the 54 as `-f` on the compile line (`C2_FORCE=1` at `-O0`
on `x86_64` now reads `files=304 ok=295 fn=1374 faithful=1337` instead of exiting 1);
`tools/c2_equiv.sh` splices them through `MCC_TEST_OPT`, which is the only channel it has,
since it drives `exec_runner` rather than `mcc`; `tools/gate-ledger.sh` toggles each row as
`-f<name>`/`-fno-<name>` and its control cell is now `gateledger-control`, a flag name the
table does not contain. It reads **115 knobs at `-O1`: 4 change the AST, 51 change only the
object, 60 never fire** — the first ledger this tool has produced since `a55c0a07`. Those
three are repointed and run, **not** registered; that half of open item 4 stays open.

#### LANDED — the cells

| cell | what it holds | known-positive, and the text it produces |
| --- | --- | --- |
| **`ast/o0-baseline`** | the per-key `-O0` object `sha256` bank and the forced-Replay_IR counters, over the `measurable` key set: every key whose compiler *and* sysroot are present, with the dropped ones named | **`ast/o0-baseline-known-positive`** takes measurement A at `-O1` (`O0_AB_MUTATE=1`), so every banked hash must move: `o0_ab: x86_64 -- an -O0 object moved`, 244 hash lines |
| **`ast/o0-baseline-gated`** (2026-08-09) | the other twelve `*.gated.rir.txt` and `board.gated.txt`: the same forced-Replay_IR counters with all 54 `MCC_OPTD_LEVEL(n)` knobs of `src/mccopt.h` forced on as `-f<name>`, derived per run from the table that defines them | **`ast/o0-baseline-gated-known-positive`** derives the 54 and then drops them (`O0_AB_NOGATES=1`): `x86_64: the gated counters are identical to the ungated bank's. 54 knob(s) were passed as -f and not one body changed shape, which is what a silently-ignored flag name looks like.` |
| **`build/fragments-are-not-tus`** (2026-08-09) | that `src/*.c` is **not** a set of translation units: compiled standalone with the build's own `-D`/`-I`, read back from `compile_commands.json`, exactly `mccast.c`, `mccircap.c`, `mccrir.c` exit non-zero, and the pin is by name | **`build/fragments-are-not-tus-known-positive`** adds `-DMCC_AMALGAMATED=0`, the define the one build that *does* compile fragments separately uses, which switches those three bodies off entirely: `the set of src/*.c that do NOT compile as their own translation unit is now [], pinned [mccast.c mccircap.c mccrir.c]` |
| **`fmt/census-bank`** | the fourteen figures the board's row 4 quotes — `172` sites, `162` literal, `148` accepted, `100` carrying `%s`, `9` budget / `4` flag / `1` float, return consumed at `26` of `162` — plus the per-file site counts, against `tests/fmt/census-bank.json` | **`fmt/census-bank-known-positive`** reintroduces the port's literal-run drift: `accepted banked 148, now 143`, `refused_budget banked 9, now 15` |
| **`idiom-gate-known-positive`** | that `idiom-gate-invariant` can fail at all | since 2026-08-09, five must-fail probes: all four violation shapes over 16 named macros (`17 violation(s)`), an unregistered `MCC_CONFIG_*`, a contradicted no-subject refusal, the zero-conditional floor, **and** an empty directory, which used to print `OK` |

`o0_ab.sh` and `idiomgate.c` gained the floors that make those cells mean something:

- **`o0_ab.sh`** could bank, or agree with, a baseline of nothing — an empty `<key>.obj.txt`
  diffs clean against an empty bank. Now: a corpus floor (`O0_AB_MIN_FILES`, default 64), a
  `nobj == 0` refusal, a **key floor** (`O0_AB_MIN_KEYS`, set to 8 under `MCC_ENABLE_CROSS`
  and 1 otherwise) so that *"this build has no cross compilers"* and *"this check has no
  subject"* stop being the same green tick, and a refusal to bank from `measurable` at all.
  Ablated: with no cross build, `FAIL -- 1 measurable key(s) is below the floor of 12`.
- **`tools/idiomgate.c`** (filed item 13's first half) printed **no subject count at all**,
  so a walk that read zero files was character-for-character identical to a clean run. It
  gained `idiom-gate subject: 130 file(s) scanned, 1739 conditional(s) examined, 4
  test(s) of the 17 named config macro(s)` and a fail on a zero in any of the three. That
  wording is **superseded** — the run line and the walk both changed the same day.
  **That `4` was read as a coverage figure and it was not one** — see the correction in
  *LANDED — `tools/idiomgate.c`'s denominator* below. Item 13's *other* half — 17 of 37
  `MCC_CONFIG_*` covered, from two hand-typed lists — is closed there too.

#### LANDED — `tools/idiomgate.c`'s denominator, and the `4` that was not a coverage figure

**The real denominator is 37, and it is now derivable rather than typed.** 41 distinct
`MCC_CONFIG_*` identifiers occur in `.c`/`.h`/`.inc` under `src` + `tools` + `runtime` +
`include`; four of them (`_AST`, `_CST`, `_BCHECK`, `_BACKTRACE`) occur *only* inside
`tools/ckretired.c`'s `RETIRED[]` and are that tool's subject, not this one's. 41 − 4 = **37**,
reproducible with

```
grep -rhoE 'MCC_CONFIG_[A-Z0-9_]+' --include=*.c --include=*.h --include=*.inc \
     src tools runtime include | sort -u
```

and **the gate now takes that derivation itself**: it harvests every `MCC_CONFIG_*` token
from the text of every file it walks, skipping only the two files that hold a *registry* of
names rather than a use of one (`tools/idiomgate.c`, `tools/ckretired.c`), and fails if the
harvest and the registry disagree in either direction. The harvest returns exactly the 37
rows, zero unknown. The 37 is no longer a number anyone types.

A note on how those four retired names are excluded, because the first attempt got it wrong
in an instructive way: `idiomgate.c` originally carried them in a `RETIRED[]` array of its
own, and `retired-macro-invariant` **failed the tree** — `ckretired` bans those spellings
everywhere under `src`/`tools`/`runtime` except its own file, and a second copy of the list
is exactly what it exists to prevent. They are excluded by *skipping `ckretired.c`* instead,
so there is still only one place in the tree that spells them.

Of those 37, **6 are CMake cache variables** that appear in C only inside string literals
(`_JIT` in `src/mcc.c`'s `--help`, `_DWARF`/`_NEW_MACHO`/`_MINGW` in `tools/ci.c`'s matrix,
`_LIBC`/`_AUTOCORRECT` in `tools/ckconfig.c`'s allow-lists) and are never emitted as a `-D`,
and **2 more** (`_UCLIBC`, `_OPTIMIZER`) are emitted as a `-D` and read by no conditional —
`ckconfig --list` already reports `MCC_CONFIG_UCLIBC (DEAD)`. **The preprocessor-testable
surface is therefore 29, and all 29 occur in a conditional today.**

**The `4` measured rule-firings, not macros, and reading it as coverage was wrong.**
`g_tests++` sat inside `is_value_kind`/`is_flag_kind`, which were only consulted where a
*violation was possible* — `#ifdef`/`#ifndef`/`defined()` on a value-kind macro, or a
flag-kind macro used as a value. Correct usage incremented nothing. Decomposed file by file
against the old binary, the entire 4 comes from **two files**: `src/mcc.h` (2) and
`src/mccdefaults.h` (2) — the four `#ifndef X` + `#define X` default-provider pairs on
`MACHO_CHAINED_FIXUPS`, `NEW_DTAGS`, `PIE`, `PIC`. `src/objfmt/mccmacho.c`, which holds
**fourteen** `#if MCC_CONFIG_MACHO_CHAINED_FIXUPS`, contributed **0**; so did `mccrun.c`'s
four `#ifndef MCC_CONFIG_BACKTRACE_ONLY` and `mcchost.c`'s `#ifdef MCC_CONFIG_TOOLHOST`.
All 17 named macros did occur in a conditional, and a wrong idiom on any of them would have
been caught. **The honest before-figure is 17 of 37 (45.9%)**, not 4 of 37; the board's
*"4 of 37 config macros have their idiom checked"* is withdrawn.

| | before | after |
| --- | --- | --- |
| enforced idiom | **17 / 37 (45.9%)** | **29 / 37 (78.4%)** |
| refused by name, with a reason | 0 (20 silently exempt) | **8** |
| conditional sites ruled on | 4 (rule-firings only) | **72** |
| walk | `src` `tools`, 130 files, 1739 conditionals | `src` `tools` `runtime` `include`, **307** files, **3808** conditionals |

Both lines are now printed on every run, so `OK` is never unqualified:

```
idiom-gate subject: 307 file(s) scanned, 3808 conditional(s) examined, 72 config-macro site(s) ruled on
idiom-gate coverage: 29 of 37 registered MCC_CONFIG_* macro(s) carry an enforced idiom (78.4%),
  29 of those reached by a conditional (100.0%); 8 refused with a reason (--registry lists them)
```

**The twelve macros added, and what each now guarantees.** Value-kind means numeric, so the
canonical test is `#if X` and `#ifdef`/`defined()` on it is a bug; flag-kind means a string
literal or presence-only, so the canonical test is `#ifdef`/`defined()` and using it as a
value is a bug. Every one has a fixture in `tests/idiom/known-positive` that makes the gate
name it:

| macro | kind | the failure the gate can now produce |
| --- | --- | --- |
| `MCC_CONFIG_TRACE` | value | `#ifdef` **and** `#ifndef` with no default `#define` |
| `MCC_CONFIG_CPUVER` | value | `defined()` |
| `MCC_CONFIG_DWARF_VERSION` | value | `#ifdef` |
| `MCC_CONFIG_SEMLOCK` | value | `defined` |
| `MCC_CONFIG_RUNMEM_RO` | value | `#ifdef` |
| `MCC_CONFIG_AUTO_MCCDIR` | value | `defined()` |
| `MCC_CONFIG_SYSROOT` | flag | used as a value in `#if` |
| `MCC_CONFIG_CROSSPREFIX` | flag | used as a value in `#elif` |
| `MCC_CONFIG_CRTPREFIX` | flag | used as a value in `#if` |
| `MCC_CONFIG_LIBPATHS` | flag | used as a value in `#if` |
| `MCC_CONFIG_SYSINCLUDEPATHS` | flag | used as a value in `#if` |
| `MCC_CONFIG_ELFINTERP_ARMHF` | flag | used as a value in `#if` |

`MCC_CONFIG_ELFINTERP` is a thirteenth in practice: it was matched only by a
`len > 20 && !strncmp(tok, "MCC_CONFIG_ELFINTERP", 20)` prefix hack that would equally have
claimed any `MCC_CONFIG_ELFINTERP*` name. Both names are explicit rows now and both have
fixtures.

**Five genuine violations, found by the widened check and fixed, not excluded.** All five are
behaviour-preserving: each macro is emitted only as `=1` or not at all, so `defined X` and `X`
agree on every configuration.

| site | was | now |
| --- | --- | --- |
| `src/mcclog.h:159` | `#if defined(MCC_CONFIG_TRACE) && MCC_CONFIG_TRACE` | `#if MCC_CONFIG_TRACE` |
| `src/libmcc.c:920` | `#if defined MCC_CONFIG_AUTO_MCCDIR && MCC_HOST_POSIX` | `#if MCC_CONFIG_AUTO_MCCDIR && MCC_HOST_POSIX` |
| `src/libmcc.c:1043` | `#elif defined(MCC_TARGET_ARM) && defined(MCC_CONFIG_CPUVER)` | `#elif defined(MCC_TARGET_ARM)` |
| `runtime/lib/bcheck.c:163` | `#if defined MCC_CONFIG_MUSL \|\| defined __ANDROID__` | `#if MCC_CONFIG_MUSL \|\| defined __ANDROID__` |
| `runtime/lib/bcheck.c:1132` | `!defined MCC_CONFIG_MUSL` | `!MCC_CONFIG_MUSL` |

The `libmcc.c:1043` conjunct was **dead-true**: `src/mcc.h:129` includes `arm-gen.h` on every
ARM target and `arm-gen.h:14` defaults `MCC_CONFIG_CPUVER` to 5, so `defined(MCC_CONFIG_CPUVER)`
could not be false under `defined(MCC_TARGET_ARM)`. That is exactly the reader-misleading shape
the value-kind rule exists to catch, and it had been sitting under the old exemption.

**The eight refusals, each carrying its reason in the tool** (`idiomgate --registry` prints
them, and `idiom-gate-invariant` runs with `--registry` so the ctest log carries the whole
table):

| macro | why no idiom can be enforced |
| --- | --- |
| `MCC_CONFIG_UCLIBC` | emitted by `CMakeLists.txt` and `tools/build.c`, read by no conditional; `ckconfig` reports it `DEAD` and lists it in `ALLOW_DEAD` |
| `MCC_CONFIG_OPTIMIZER` | emitted by `tools/build.c`, read by no conditional |
| `MCC_CONFIG_JIT` | CMake cache variable; selects `MCC_JIT_DEFAULT`, never emitted as a `-D` |
| `MCC_CONFIG_LIBC` | CMake cache variable; selects `MCC_CONFIG_MUSL`/`_UCLIBC`, never emitted as a `-D` |
| `MCC_CONFIG_DWARF` | CMake cache variable; selects `MCC_CONFIG_DWARF_VERSION` |
| `MCC_CONFIG_NEW_MACHO` | CMake cache variable; selects `MCC_CONFIG_MACHO_CHAINED_FIXUPS` |
| `MCC_CONFIG_MINGW` | CMake cache variable; selects the PE target |
| `MCC_CONFIG_AUTOCORRECT` | CMake cache variable; relaxes `mcc_validate_config` |

Six of the eight are not preprocessor macros at all, so `78.4%` understates what is
reachable: **29 of the 29 testable macros are covered**, and the gate prints that as its
second fraction.

**Four new floors, so the count cannot rise while the guarantee stays hollow:**

1. a `MCC_CONFIG_*` anywhere in the walked tree with **no registry row** fails the run. This
   is the half of item 13 that mattered — *"the denominator shrinks silently every time a
   `MCC_CONFIG_*` is added"* is now impossible.
2. a row of kind value/flag with **zero** conditional sites fails, naming itself. A macro
   whose last conditional is deleted stops counting toward coverage instead of padding it.
3. a row registered as *reachable by no conditional* that **acquires** one fails. A refusal
   that cannot be contradicted is not a refusal.
4. a row whose name occurs **nowhere** in the tree fails. `tools/idiomgate.c` skips itself
   during the name harvest, so its own registry strings cannot satisfy this.

`idiom-gate-known-positive` now runs six probes, five of which must fail, and asserts the
violation *count* and every macro *name*: `17 violation(s)` over 16 macros on
`tests/idiom/known-positive`; `MCC_CONFIG_NOT_A_REAL_KNOB` on `tests/idiom/unregistered`;
`MCC_CONFIG_UCLIBC:` and `MCC_CONFIG_JIT:` contradicting their refusals on
`tests/idiom/no-subject`; the zero-conditional floor naming the 13 unreached macros when the
fixture directory is walked without `--subset`; and `tests/idiom/empty`, still refused.

Verified with `cmake-cross` built before `cmake-debug` was configured (hazard 5):
`cmake-debug` **9143 cells, 0 failures**, `cmake-cross` **9143 cells, 0 failures**,
`-L flagsweep` **119**, `-L stratsweep` **30**, `MCC_RIR_CENSUS=1 -L census` **6**, and
`tools/selfhost-smoke.py cmake-debug` green. No cell was added — the two existing idiom
cells carry all of it.

#### LANDED — `tools/fmt-census.py`'s corpus, and both readings

Hazard 8 is closed for the default mode. Two things changed and it matters which:

**The roster is pinned, and the numbers did not move.** `CORPUS` now lists the eighteen
`src/*.c` explicitly instead of globbing them, and `--check` fails in **both** directions
before it compares a single figure — a listed file that is gone, and a `src/*.c` nobody
listed. Ablated: `src/mccnewpass.c is on disk and not in CORPUS`, and `CORPUS lists
src/mcchost.c, which is not on disk`. **The roster is the same eighteen files the glob
returned and every printed figure is identical.** `172 / 162 / 148` is unchanged and the
board's row 4 stands.

**The double-counting was never in this census.** `src/*.c` is not eighteen translation
units — `libmcc.c` `#include`s fifteen siblings and `mcc.c` `#include`s `libmcc.c` and
`mcctools.c` — but the site census scans each file's own text and **never follows an
`#include`**, so no body was ever counted twice. Also explained rather than left mysterious:
`TUs: 16` over eighteen files, because `mccdbg.c` and `mccjit_intent.c` hold no
printf-family call site at all. Both are pinned at `0`, so a file that stops contributing
fails instead of quietly shrinking a denominator.

**Where the double-counting really is: the `--arenas=` row.** The docstring's
`for f in src/*.c; do … mcc -c -O1 …; done` compiles what it is given, so that loop compiles
most of the tree several times over. The docstring is corrected to the one real TU. This
branch left the row as an upper bound of unknown tightness — correctly, because arming the
recorder is a measurement and this branch was doing registration. **It was re-taken on
`wt/arenaretake`; the section below has both readings and the tightness is no longer
unknown.**

#### LANDED — the D4b `--arenas=` row, re-taken. **The duplication factor is 2.8636, and the share the board quotes survives it intact**

The row above left `8,250 arenas / 29,309 Invoke-blocked / 242 (0.826%)` as an upper bound of
unknown tightness. It is measured now, and the tightness turns out to be two different
answers for the two halves of the figure: the **counts** are inflated ~2.81–2.86×, and the
**share** is not inflated at all.

**The duplication factor, derived rather than borrowed.** Every `[arena]` record is one body
and carries that body's name in its `fn=`, so records-against-distinct-names measures the
double-counting directly — no structural fingerprint, no guessing. The loop spelling emits
**8,250 records over 2,881 distinct bodies = 2.8636**, and the multiplicity histogram says
exactly where it comes from:

| bodies recorded | 1× | 2× | 3× | 4× | 5× | 6× |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| count | 51 | 346 | **2,441** | 35 | 4 | 4 |

The 2,441 at exactly 3× are the ordinary case: a source is compiled once as a fragment, once
inside `libmcc.c`, and once inside `mcc.c`. The 4–6× tail is the `static inline` bodies in
the shared headers, which every fragment that uses them emits its own copy of —
`read16le`, `read32le`, `dwarf_read_uleb128` and `dwarf_read_sleb128` (`src/mcc.h`) land six
times each, `ast_gate_from_so` / `ast_gate_to_so` (`src/mccgate.h`) five.
**Not the 2.8× carried in from the `10,239 vs 28,753` body count** — that figure was a
different count on a different corpus; this one is re-derived here and happens to agree,
which is the only reason it is safe to say they agree.

**Both readings, side by side.** Same compiler (`cmake-debug`), same level, same tool; the
only difference is what gets compiled:

| | loop over `src/*.c` (as banked) | `src/mcc.c`, the one real TU |
| --- | ---: | ---: |
| `[arena]` records | 8,250 | **2,880** |
| distinct bodies | 2,881 | **2,880** |
| duplication factor | **2.8636** | **1.0000** |
| non-empty blocks | 56,284 | **19,901** |
| Invoke-blocked | 29,309 | **10,423** |
| unblocked by the `snprintf` family alone | 242 | **86** |
| that share | 0.8257% | **0.8251%** |

Deflation, count by count: arenas 2.8646, blocks 2.8282, Invoke-blocked 2.8119,
`snprintf`-only 2.8140. **The share moves by 0.0006 points.** The contamination was very
nearly uniform, which is what you would expect of a corpus that is the same tree repeated,
and it is why the *ranking* this row fed was never wrong even while its counts were.

**Quote the right-hand column: 10,423 Invoke-blocked, 86 (0.825%).** There is no
two-translation-units ambiguity to split here, and that is worth stating because it is the
usual reason a de-duplication is contestable. `src/*.c` is not eighteen translation units —
seventeen of the eighteen are textual fragments that exist only to be `#include`d, the build
compiles exactly one object from `src/`, and three of the seventeen (`mccast.c`,
`mccircap.c`, `mccrir.c`) **exit non-zero even with the build's own `-D`/`-I`**, so part of
the left-hand column is records from compiles that failed. The extra records are not "the
same body reached through a second TU"; they are the same body reached through one real TU
and up to five make-believe ones. The check that settles it: the loop's set of body names is
a strict superset of the TU's by **exactly one name**, `is_float_abi` — an `ST_INLN` in
`mcc.h` that only gets a body emitted when an arch fragment is compiled standalone, i.e. a
body no build ever produces. Every other body the loop saw, the single TU also saw, once.

**The three non-compiling fragments were adjudicated 2026-08-09 (`wt/envgate`): by design,
not a defect — and now gated.** The three are the only `src/*.c` that do not `#include
"mcc.h"`, so they never define `MCC_INTERNAL`, and their entire bodies sit behind
`#if (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))`. Outside their include context
they do not parse (`unknown type name 'Sym'`, `'IrCapOp'`, `'CType'`). The one build that
*does* compile the fragments as separate TUs — `-DMCC_SINGLE_SOURCE=OFF`, which the default
`ON` hides — compiles them with `-DMCC_AMALGAMATED=0`, which switches those bodies **off**:
`ninja libmcc` there is green, and `src/mccast.c.o` is 1,464 bytes with **0** defined
symbols against `src/libmcc.c.o`'s 148,576 and **120**. So neither spelling produces a
translation unit: without the define three of eighteen fail to parse and the rest duplicate
the amalgamation; with it, the three are empty. **`build/fragments-are-not-tus`** pins that
by name, with `-DMCC_AMALGAMATED=0` as its known-positive, so the next tool that reaches for
`for f in src/*.c` meets a red cell before it meets a plausible number.

**One figure drifted, and it is not the one you would expect.**
The banked block count for the loop spelling was **56,281**; the identical spelling gives
**56,284** today. Three blocks, 0.005%, from source that has moved since — the *age* failure
mode, the third of the three this board tracks. It is not worth a row in the
failed-to-reproduce table and it is not being given one; it is worth knowing that the loop
reading reproduces to five figures. And the `242`/`29,309` pair itself is **not** a
failed-to-reproduce entry either: it reproduces exactly, on exactly the corpus its row named.
The corpus was the defect, not the transcription and not the tool. What changed in that table
is only the successor column.

**The gate, and it can fail three ways.** `tools/fmt-census.py --arena-check=<build-dir>`
arms the recorder itself — it reads the `-D`/`-I` for `src/mcc.c` back out of the build's own
`compile_commands.json`, the way `slice-census.py` and `selfhost-smoke.py` already do — and
compares against `tests/fmt/arena-census-bank.json`. Three assertions of deliberately
different kinds:

- **the de-duplication invariant, exact**: one record per distinct body. This is the defect
  itself, and a cell that cannot fail on it cannot protect the corrected number.
- **the counts, as floors at 90%**: the corpus is the compiler's own source and moves on
  nearly every commit, so an equality here would be a cell against the project; a floor still
  catches a dump that silently loses bodies.
- **the share, as a ±0.10-point band** around 0.825%. Wide enough that ordinary churn does
  not move it — the entire 2.86× deflation moved it 0.0006 — and narrow enough that a change
  of method cannot hide in it.

`fmt/arena-census-known-positive` proves all three: it runs the clean check (must pass), then
`--mutate-arenas=dup` (census the dump twice — the loop-spelling shape), `=shrink` (stop at a
thousand bodies) and `=share` (score a block unblocked when *any* callee is in the family
rather than every one), and fails if any of the three still passes the bank. Ablated: the
three mutations report 2.0000× duplication, four counts under their floors, and 2.763%
against a banked 0.825%.

**What this is worth, stated so nobody re-reads it as a payoff.** D4b is denominated in
device-eligible blocks; `mcc_slice_frame_from_ast` is still a single definition in
`src/mccslice.h` with no caller anywhere in `src/`, and 86 is 86 blocks of a currency with no
exchange rate. This is a bookkeeping correction and was
sized like one: the whole re-take is a **1.18 s** cell plus a 4.87 s known-positive, and the
corrected number costs nothing to keep honest from here.

#### LANDED — `tests/must-run.txt`, nineteen rows

The `fmt/census-oracle` pair had never been added to the manifest; nor had thirteen other
registered cells that publish a figure this board quotes. Every one is `registered` rather
than `must-run` where it honestly 77s on some host, because a manifest that demanded they
run everywhere would assert something false. Two of the thirteen are the reason the section
exists:

- **`opt-cache-determinism`** — a **permanent 77**, exactly as the LOST SUBJECT item above
  describes. The row does not fix it; it stops the tree reading it as green. This is the
  `registered` row that item asked for.
- **`runtime-bench-gatewin`** — a **permanent 77** since `vendor/plb` left the checkout.
  Filed item 9 called it *"the strongest candidate in the tree"* for a row. It now has one.

Also added: `node-census`, `loop-census`, `loop-census-numeric`, `loop-census-parallel`
(filed item 3 named the last two), `slice-enum`, `slice-census`, `runtime-bench-check`,
`rir-coverage-census`, `rir-gap-classes`, `rir-lowerable-classes`, `rir-nofb-probe`, plus
all six new cells. `optlevel-bench` and `selfhost-optbench` are deliberately **not** added:
they are gated on `MCC_OPT_LADDER_BENCH`, which is OFF by default, so a `registered` row
would be false on every normal build.

#### Clean bills of health from this sweep

- **`tools/o0_ab.sh`'s refusal discipline was already good** and is why the re-bank was
  trustworthy: it refuses a key with no sysroot in as many words (*"measured as unmeasurable
  rather than measured"*), refuses a run with no `[rir-total]`, refuses zero derived gates,
  and runs an `arm-win32` == `arm-wince` twin check as *"the cheapest available proof that a
  run measured what it thinks it did"*. The four floors added above are the same idea
  applied to the three inputs it did not check.
- **`tools/fmt-census.py`'s corpus floor** (`< 1000` formats fails the selfcheck) transplants
  directly; `--check` now carries the same shape as a `< 100` literal-site floor.
- **The `-O0` bank's own arithmetic is sound.** `bar=OK` on all twelve keys, `faithful +
  empty == fn` on every row, `arm-win32` and `arm-wince` identical on both counters and
  object `sha256`, and the whole twelve-key board reproduces in 29 seconds.

#### Filed by this sweep, not fixed

1. ~~**The `--arenas=` figure needs re-taking against one TU.**~~ **CLOSED 2026-08-09,
   `wt/arenaretake`.** The recorder is armed by the tool itself now
   (`--arena-check=<build-dir>`), the factor is 2.8636, and the corrected
   `10,423 / 86 (0.825%)` has a cell.
2. ~~**`idiomgate`'s subject is four.**~~ **WITHDRAWN 2026-08-09 — the claim did not
   reproduce.** The `4` counts rule-*firings*, not macros reached: `g_tests++` only ran where
   a violation was possible, so `src/objfmt/mccmacho.c`'s fourteen correct
   `#if MCC_CONFIG_MACHO_CHAINED_FIXUPS` scored zero. All 17 named macros were reached, and
   *"4 of 37 config macros have their idiom checked"* was never true; **17 of 37** was.
   Coverage is now **29 of 37** with the other 8 refused by name. See *LANDED —
   `tools/idiomgate.c`'s denominator* above. This was the thirteenth headline figure in this
   project to fail to reproduce **as the list stood when this was written**, and the third
   whose cause was a count taken once and read as something it did not measure. For the
   current count, read the failed-to-reproduce table, not this ordinal.
3. ~~**`tools/o0_ab.sh`'s gated half stays frozen**~~ **CLOSED 2026-08-09 (`wt/envgate`).**
   The thirteen files are regenerated against `src/mccopt.h` and `ast/o0-baseline-gated`
   covers them, with a known-positive that the committed thirteen would have failed. See
   *LANDED — the gated half of `ast/o0-baseline`*.
4. **The remaining not-a-cell tools.** ~~`tools/opt-determinism.py`,
   `tools/untyped-probe.py`,~~ `tools/xsuite-report.py`, `tools/gate-ledger.sh`,
   `tools/strategy-ledger.sh` and `tools/c2_sweep.sh` all publish or feed a board figure and
   are registered nowhere. ~~The last three are additionally blocked on filed item 17.~~
   **No longer blocked** — item 17 closed 2026-08-09 and `gate-ledger.sh` and `c2_sweep.sh`
   both run and produce their figures again (115 knobs / 4 FIRES / 51 OBJONLY / 60 NEVER at
   `-O1`; `files=304 ok=295 fn=1374` forced at `-O0` on `x86_64`). They are still registered
   nowhere, which is now the *whole* of what is left on them.
   ~~`opt-determinism.py` and `untyped-probe.py` are the two cheapest remaining
   registrations in the tree — both are pure-CPU, both already refuse their degenerate
   inputs after the last sweep, and neither needs a bank invented for it.~~ **Both LANDED
   2026-08-09 (`wt/gatefin`)**, each with a known-positive; see the audit section. Four
   tools left on this item, none blocked on anything but the work.

### 1. S5′ — the iteration distribution, and the measurement that prices every row below

> **Read `#### 3. And the lanes are floating-point` at the head of this file before quoting
> anything in this row.** Every subsection below is kept for its method, its controls and
> its history, and each carries its own supersession banner where a figure moved. The one
> thing none of them say, because it was not measured until 2026-08-09, is that **79.21 of
> the corpus's 80.66 parallel-legal points are `double` arithmetic that neither executor
> implements**. The `65.75%` / `1.88%` pair and the `1.39%` / `79.35%` pair below are both
> **STALE** and must not be quoted again in either direction.

`-floop-census` + `runtime/lib/loopcensus.c` + `tools/loop-census.py` now take the
measurement `docs/PLAN.md` calls "the single measurement that decides whether the project
has a subject". Ground truth first: `loop-census-control` compiles a program whose trip
counts are known by construction and checks the histogram against them at `-O0/1/2/3`,
with a negative control (no flag, no data) and a perturbation (move a bound, the numbers
move). It is `must-run`.

Self-compile of `src/mcc.c` at `-O2`, 1979 loops instrumented, 597 entered. **Every figure
in the next two tables predates `415b736c` (`wt/pvokclear`), which replaces the `rir_pvok`
clear with a high-water-mark `memset` and takes a self-compile from ~163M iterations to
52.1M. They are kept for the shape of the distribution, not for their values; the current
numbers are in `#### The second workload` below.**

| | |
| --- | ---: |
| loop entries | 25,336,468 |
| iterations | 162,656,621 |
| entries with 0 trips | 9,764,126 (38.5%) |
| entries lost (`return`/`goto`/`longjmp` out of the loop) | 2,242,930 (8.85%) |
| stray exits (`goto` *into* a body) | 929 (0.004%, all in `parse_comment`) |
| **iteration-weighted fraction at each loop's own break-even** | **85.45%** |
| the same with the single hottest loop removed | **59.73%** |
| **the same, restricted to loops `ast_loop_parallel_legal` proves parallel** | **65.75%** |
| the same with the single hottest loop removed | **1.88%** |

| trips | 1 | 2 | 3-4 | 5-8 | 9-16 | 17-32 | 33-64 | 65-128 | 129-256 | 257-512 | 513-1024 | 1025+ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| entries | 18.33M | 2.26M | 1.10M | 758K | 200K | 105K | 49.7K | 29.3K | 18.1K | 231K | 4.0K | 4.2K |
| share | 79.4% | 9.8% | 4.8% | 3.3% | 0.9% | 0.5% | 0.2% | 0.1% | 0.1% | 1.0% | 0.02% | 0.02% |

Read those two rows together. **79.4% of loop entries run exactly one iteration**, and the
85.45% is iteration-weighted, so it is carried by a handful of long runs: one loop,
`rir_op_effect` at `mccrir.c:2959`, is **63.9% of every iteration in the compile**, and the
top ten are 78.7%. Against the fixed bar the answer barely moves with the threshold —
87.4% at trips≥8, 78.3% at trips≥322 — which is the same fact: the distribution is
bimodal, not graded. The subject, if there is one, is roughly ten loops, most of them the
compiler's own RIR/AST bookkeeping.

The break-even table the fraction is scored against:

| nodes | 3 | 7 | 15 | 31 | 63 | 127 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| break-even lanes | **322** | **108** | **48** | 24 | 23 | 8 |

Only **4.3%** of census slices contain a loop at all (`tools/slice-census.py`, "slices
containing a loop"), which is why this is the binding half rather than eligibility.

#### `par=` answers now, and the answer kills the dispatch column

`ast_loop_parallel_legal(AstArena *, AstLocal)` landed on `wt/parlegal`. It returns 1
(provably no dependence carried by this loop), 0 (a carried dependence is *proven*), or -1
(the analysis declines) — and `-floop-census` now emits a `[loopar] id= par=` record per
loop from `ast_func_end`, where the arena exists, so `par=` is `1` / `0` / `?` instead of a
literal question mark. `?` is never collapsed into `0`; the three counts are reported
separately. `-O0` builds no arena, so every loop is `?` there — that is the negative
control, not a bug.

Self-compile of `src/mcc.c` at `-O2`, 2017 loops instrumented, 597 entered:

| par= | static loops | entered | entries | iterations | share of iterations |
| --- | ---: | ---: | ---: | ---: | ---: |
| **1** (parallel) | 55 | 12 | 231,716 | 108,407,312 | **65.86%** |
| **0** (carried) | 85 | 15 | 49,804 | 1,990,963 | 1.21% |
| **?** (declined) | 1877 | 570 | 25,331,783 | 54,203,641 | 32.93% |

| | raw | parallel-legal |
| --- | ---: | ---: |
| iteration-weighted fraction at each loop's own break-even | 85.45% | **65.75%** |
| the same, hottest loop removed (of all iterations) | 59.71% | **1.88%** |
| at trips≥8 | 87.39% | 65.84% |
| at trips≥322 | 78.33% | 65.62% |

**Read the second row, not the first.** 65.75% looks like a subject and is not one. It is
one statement:

```c
/* src/mccrir.c:2959 and again at :3286, inside rir_op_effect */
for (q = o->vs_n + 1; q <= VSTACK_SIZE; q++)
        rir_pvok[q] = 0;
```

That single `for` is **63.88% of every loop iteration in the compile** (105.1M of 164.6M),
its twin at `:3286` is another 1.32%, and together they are **65.2 of the 65.75 points**.
It is a `memset` of a 512-byte flag array, entered 205,764 times to clear a mean of 511
bytes. It is genuinely parallel — the predicate is right about it — and it is worth
exactly nothing to a device: the fix is `memset`, or not clearing at all.

Take those two out and the parallel-legal iteration-weighted fraction is **1.88% of all
iterations** (5.19% of what remains). The complete measured lane source is **twelve
loops**, every one of them read by hand:

| id | site | what it is |
| ---: | --- | --- |
| 1294, 1297 | `mccrir.c:2959`, `:3286` | `rir_pvok[q] = 0` |
| 1242 | `mccrir.c:925` | `rir_vslbl[i] = rir_vslbl2[i] = -1; rir_vscapt[i] = 0` |
| 614 | `mccast.c:1852` | `ast_strat_order[i] = i` |
| 1202 | `mccast.c:18350` | `sf[si] = 0` |
| 713, 726, 737 | `mccast.c:4470/4564/4617` | `cweight[j] = 0`, `careg[j] = -1`, `colorable[j] = 1` |
| 596 | `mccast.c:1334` | `color[i] = -1` |
| 746 | `mccast.c:4684` | `ast_promo_save_slot[i] = i` |
| 633 | `mccast.c:2300` | `reg_classes[hr] \|= MCC_RC_FLOAT`, 8 iterations total |
| 1850 | `asm-constraints.inc.c:45` | `sorted_op[i] = i`, 7 iterations total |

Twelve array fills. Nine of them run fewer than 70,000 iterations across an entire
self-compile. **There is no dispatch site in this workload.** The AST-slice engine is a
lowering achievement — the predicate, both executors, the SPIR-V emitter and the leaf
inliner all work and are all tested — but the compiler contains no loop that both carries
enough iterations to clear break-even *and* has independent lanes to give it. Stop ranking
dispatch work off the 85.45%. The unranked fence-wait row below, and debt #0, both say the
same thing from the other end.

The 63.9%-in-`rir_op_effect` line above is still true and still worth acting on, but not
as a dispatch target: two thirds of all loop iterations in a self-compile are a redundant
512-byte clear. That is a `memset` and a liveness question, not a GPU.

**What the predicate refuses, and why each refusal is deliberate.** It declines (`?`) on:
a call, `asm`, a `return`/`goto`/label/`case` in the body, `AST_StoreVal` (a store used as
a value, which `ast_dep_collect` does not model as a store), any `AST_OP_*` outside a
fixed safe list (atomics, VLA, `va_arg`, `OPASSIGN`), a `volatile` type, an
address-escaping local, a base it cannot resolve (`!r->ok`), more than 64 distinct scalars
or `AST_DEP_MAXREF` refs, a conservative direction vector, a scalar written only under a
condition with no unconditional definition ahead of it, and — this one is not in the
classical recipe — **any memory read in the exit test**, because a loop whose trip count is
data-dependent has no lane count to hand a dispatcher even when no data dependence is
carried. It also refuses to trust `ast_dep_base_distinct` between two refs when either
reached its base through a `Load`: `p[i]` and `q[i]` for distinct global pointers `p`, `q`
are *not* distinct objects, and the shared decoder had been treating them as such. That
gate is new (`AstDepRef.indirect`) and is not read by `ast_loop_interchange_legal` or
`ast_loop_fusion_legal`, so their behaviour is unchanged.

**Priced 2026-08-09.** That gate is correct and it is the single most expensive refusal on
array code. `mcc` marks *every* access to a 2-D array `INDIRECT` — `a[i][j]` decodes through
an `AST_Load` between the two subscript peels (`mcc -O2 -fdump-loopdep`), even though `a` is
a static array whose base is fully known and no memory is read there — so the gate that
exists to stop `p[i]`/`q[i]` also stops `a[i][j]`/`b[i][j]`. It costs **79.35% of all
iterations** in the numeric corpus and **0.71%** in a self-compile. The clean fix is to tell
an array-decay `Load` from a pointer read; that was attempted and abandoned, because
`ast_type_t` on those nodes returns 0, so the decoder has no type to test. Anyone picking
this up needs to add the type, not the test.

It answers `0` (proven carried) on: a scalar read upward-exposed in the body and written in
it (`s += a[i]`, `p++` under `*p`), a store to a fixed symbol address (`gsum += b[i]`), a
store and a ref to the same base at distance ≠ 0 in this loop's direction component
(`a[i] = a[i-1]`, `a[i] = a[i+1]`, `a[i] = a[i-8]`), and two same-base refs that are both
subscript-free, i.e. the same address every iteration.

Two limits are stated rather than hidden. First, the predicate assumes a store through a
pointer does not clobber a *named global scalar* read in the same loop's exit test —
closing that needs a memory model this tree does not have. Second, a reduction is `0` by
design: `s += a[i]` is not parallel without a reduction transform, and nobody has written
one.

**The controls.** `tests/loopcensus/known_deps.c` holds 21 loops of known dependence
structure and `known_deps.expect` the verdict each must get; `loop-census-parallel`
(`tools/loop-census.py --partest`) checks every one at `-O1/-O2/-O3`, asserts separately
that **no loop the expectations call carried ever comes back `par=1`**, checks that `-O0`
answers `?` everywhere, and perturbs the source (drop the `a[i-1]` read, turn `s +=` into
`s =`) to show both flip to `par=1` — so the predicate reads the dependence and not the
loop shape. It was also broken twice on purpose to prove the cells bite: disabling the
direction-component test made `dp_fwd`, `dp_bwd`, `dp_dist8` and both nested carried cases
report `par=1` (5 unsound verdicts, 18 failures); disabling the upward-exposed-scalar test
made `dp_reduce` and `dp_cond_scalar` report `par=1`.

Three caveats that the next user of this number must not drop:

1. **Body size in AST nodes is a conversion, not a reading.** The arena is built from the
   RIR recording *after* the body is parsed, so `block()` has no node count to report. The
   compiler emits what it knows exactly — code `bytes` (instrumentation excluded) and
   preprocessed `toks` — and `loop-census.py` divides bytes by a bytes-per-node constant
   measured on the same TU in the same run from `MCC_SLICE_CENSUS` (**3.75**, median over
   24,747 slices). The per-threshold sweep is printed so the conclusion can be read
   without that constant.
2. ~~**`par=?` in the `[loop]` record is still a question mark.**~~ **ANSWERED
   2026-08-08 — no lane source in this workload — and *bounded* 2026-08-09: every record
   now carries `why=`, and on the self-compile 83.9% of iterations decline because the loop
   calls a function or contains a `goto`, so the bucket is genuine here and convertible
   elsewhere. See `#### The second workload`.**
3. **Ids are per-`mcc`-process.** Linking two `-floop-census` objects from separate
   invocations would collide. The tool compiles one TU.

#### The second workload — the conclusion survives, the reasoning does not, 2026-08-09

Everything above was measured on one workload, and it is the least representative one that
exists for this question: a compiler self-compile is pointer-chasing, allocation and switch
dispatch. `tools/loop-census.py --corpus runtime` now takes the identical measurement over
standalone numeric programs. **The corpus is not chosen here.** It is
`tools/runtime-bench.py`'s `KERNELS` table, *imported* by the census rather than restated,
with its per-kernel argv used unchanged — a roster fixed long before this question was
asked, and fixed to exercise codegen (integer divide, switch dispatch, struct copy, call
depth, narrowing, string work), not to look parallel. Five of its seventeen kernels are not
array code at all — `interp` is a bytecode VM, `hashmap` is chained hashing, `calls` is a
recursion ladder, `strproc` is tokenisation, `divmod` is scalar integer arithmetic — and
they are in the corpus because they were already in it. The three vendor/plb kernels are
absent from this checkout and are named as skipped by the tool. Cell: `loop-census-numeric` (label `census`, `--opt-in`, 5.1 s).

    tools/loop-census.py cmake-debug --corpus runtime --levels O2 --top 20 --opt-in
    tools/loop-census.py cmake-debug --corpus runtime --alias-oracle

| | self-compile @ `415b736c` | 17-kernel numeric corpus |
| --- | ---: | ---: |
| loops entered | 600 | 77 |
| loop entries | 26,066,284 | 19,803,274 |
| iterations | 52,077,202 | 2,246,355,539 |
| entries running exactly 1 trip | 79.96% | **1.35%** |
| entered loops `par=1` / `par=0` / `par=?` | 9 / 15 / 576 | 14 / 17 / 46 |
| iteration share `par=1` / `par=0` / `par=?` | 0.35% / 3.88% / **95.77%** | 1.45% / 13.55% / **85.00%** |
| raw iteration-weighted fraction at break-even | 52.51% | **97.76%** |
| **parallel-legal iteration-weighted fraction** | **0.01%** | **1.39%** |
| the same with the single hottest loop removed | 0.01% | 1.39% |
| **the same with `--alias-oracle`** | **0.01%** | **80.60%** |
| `--alias-oracle`, hottest loop removed | — | **3.67%** |
| hottest loop | `ast_strpool_find_or_add`, 11.4% | `matmul.c:22`, **76.9%** |
| top ten loops | 33.8% | 96.8% |

**This table is the state before `wt/decaytype`.** Its corpus column's `1.39% / 80.60%`
split no longer exists: the shipped predicate now answers 80.60% on its own and the oracle
row is worth zero. Read it for the shape of the two workloads, then read
`#### LANDED — the 79.35% converts *soundly*` below for the current values.

Read the last three rows together. The shipped predicate says *both* workloads are barren,
so the headline generalises. It generalises for opposite reasons.

**Why each workload refuses.** Every `[loopar]` record now carries `why=`, and the census
prints an iteration-weighted histogram of those reasons. A reason that names a weakness of
the analysis is convertible in principle; one that names a property of the program is not.

| reason | self-compile | corpus | kind |
| --- | ---: | ---: | --- |
| `body-unsafe` (the loop calls a function, or uses `asm`/`volatile`) | **50.04%** | 1.45% | program |
| `not-analyzable` (a label or `goto` in the loop, or no affine IV) | **33.82%** | 4.15% | program, mostly |
| `bases-may-alias-indirect` | 0.71% | **79.35%** | analysis |
| `no-iv-nest` / `no-bound` / `cond-loads` / `ref-not-affine` | 11.20% | 0.05% | mixed |
| `dep-direction-unknown` | 0.00% | 0.01% | analysis |

**83.9% of the self-compile's iterations are in a loop that calls a function or contains a
`goto`.** Those need inlining or goto-tolerant loop analysis, not dependence arithmetic.
**79.35% of the corpus's iterations are one alias question.**

**The bound on `par=?`, measured rather than argued.** `-fdep-alias-oracle` makes the
dependence code assume two distinct base symbols never alias even when the address chain
went through a load. That assumption is **unsound in general** — `p[i]` and `q[i]` through
two distinct global pointers have distinct base symbols and may be the same memory — and it
is safe to ship only because `ast_loop_parallel_legal` has no caller outside this census and
`-fdump-loopdep` (verified: `grep -rn ast_loop_parallel_legal src tools include runtime`
gives one declaration, one definition, and two diagnostic call sites), so it cannot reach
emitted code. `flagsweep-exec/dep-alias-oracle` gates that it changes no exec result. With
it on:

- the corpus goes **1.39% → 80.60%**, so ~79.2 of the 79.35 points really do convert to
  `par=1` rather than falling to the next refusal;
- the self-compile goes **0.01% → 0.01%**. Zero points. A perfect alias oracle is worth
  nothing to the compiler.

So the defensible ceilings are: **self-compile ≤ ~12%** (95.77% declines, of which 83.86%
is calls-and-`goto`s that no dependence work touches), with the *measured* alias
contribution being **0.00 points**; **corpus ≤ ~85%** (the whole `par=?` bucket), of which
**79.2 points are measured convertible** and about 1.9 more are `sieve`'s
`for (j = i*i; j < n; j += i)` — parallel, refused only for a symbolic stride. The residual
~4% is genuine: `strproc`'s `while (*p)` pointer walk, `interp`'s VM, and reductions
(`acc += …`), which the predicate refuses **by design** — nobody has written a reduction
transform, and that is a different capability from a stronger dependence test.

**Now apply the same skepticism to this headline that killed the last one.** The 80.60%
collapses to **3.67%** when the single hottest loop is removed, exactly as 65.75% collapsed
to 1.88%. Per program, under the oracle, only three of seventeen kernels have any lanes at
all: `matmul` 99.83%, `loopnest` 99.23% (whose hot nest *is* the same i-k-j matmul), and
`vlaloop` 63.68%. The other fourteen are under 1%. The corpus's entire lane source is dense
matrix multiply.

But the two collapses are not the same finding. The self-compile's hot loop was
`rir_pvok[q] = 0`, a 512-byte clear whose correct fix is `memset` — and `415b736c` has
since applied it, which is why that 65.75% is now 0.01%. The corpus's hot loop is
`c[i][j] += aik * b[k][j]` over three distinct 600×600 static `double` arrays. Its correct
fix is not `memset`. It is the one workload shape a device path exists for, and here the
oracle is not merely an upper bound: `a`, `b` and `c` are distinct file-scope statics, so
for these specific loops the assumption is *true*, and `par=1` is the correct answer.

**What this does and does not license.** It does not license resuming the device path: a
17-kernel micro-benchmark suite whose lane source is one BLAS-3 kernel is not evidence that
real programs `mcc` compiles have one. It does license retiring the claim that *the
measurement* has settled the question. What has been measured is that the compiler is
barren and cannot be made otherwise, and that on array code the predicate's own alias gate —
documented above as deliberate, and it is correct — is worth 79 points of visibility. If
anyone wants to reopen the device column, the next measurement is a real numeric
application, not a third synthetic corpus, and the cheapest predicate work with a measured
payoff is teaching `AstDepRef` to tell an array-decay `Load` from a pointer read.

Two hazards this section introduces, stated rather than buried:

1. **`--alias-oracle` numbers are not properties of a workload.** They are properties of a
   ceiling. Never quote 80.60% without the 1.39% next to it.
2. **The corpus's iteration count is dominated by one kernel's argv.** `matmul 600 8` is
   76.9% of all 2.25 billion iterations, and that size was picked by `runtime-bench.py` to
   take about a second, not to weight this census. The per-program table, not the corpus
   total, is the size-independent read — and it says three kernels of seventeen.

#### LANDED — the 79.35% converts *soundly*, and the oracle it was measured against was not the only unsound thing here, 2026-08-09

Branch `wt/decaytype`. The row above closed with "the cheapest predicate work with a
measured payoff is teaching `AstDepRef` to tell an array-decay `Load` from a pointer read".
That is done, by the route the earlier attempt filed rather than the one it abandoned: the
representation now carries the fact, and the alias gate is untouched.

**What was actually wrong.** `rir_hook_indir()` marks `RIR_M_LOAD` on *every* `indir()`,
including the one that peels `a[i]` out of `a[i][j]`. For a pointer-to-array, `indir()`
yields an array-typed value and deliberately does **not** set `VT_LVAL` — no memory is
read, the value *is* the address. But the replay built that `AST_Load` with no type at all,
so `ast_type_t` answered `0` and `ast_dep_decode` could only guess, guessed "indirection",
and set `AstDepRef.indirect`. Two fixes, both in the construction/decode, none at a query
site:

- `src/mccrir.c`, `RIR_M_LOAD`: when the pre-`indir` `SValue` is a pointer whose pointee is
  a (non-VLA) array, stamp the `AST_Load` with that array type. `ast_type_t` now answers
  correctly on decay nodes instead of `0`.
- `src/mccast.c`, `ast_dep_decode`: an `AST_Load` whose type is an array is an address
  computation, so it no longer sets `indirect`. The gate in `ast_loop_parallel_legal` is
  unchanged.

**A second, pre-existing unsoundness, found while checking the first.** The same decoder
accepted an `AST_Ref` as a *base address* without looking at `VT_LVAL`. A global pointer
read is `AST_Ref` with `op & VT_LVAL` and a non-array type — the symbol is the pointer
variable, not the object — so the shipped predicate answered

    int *gp, *gq;
    for (int i = 0; i < n; i++) gp[i] = gq[i] + 1;

`parallel(#5): legal` on pristine `70b92fb3`, with **no** `-fdep-alias-oracle`. That is
precisely the assumption the census docstring attributes to the oracle alone ("`p[i]` and
`q[i]` through two distinct global pointers have distinct base symbols and may be the same
memory"), and it was already being made by default. `ast_dep_decode` now sets `indirect` on
any lvalue `Ref` of non-array type. Cell `id=25 dp_gptr_alias` in
`tests/loopcensus/known_deps.expect` comes back `par=1` on the pristine tree and `par=?`
now.

**What is proved now, and what is still refused.** Proved: two references whose address
chains reach *distinct declared symbols* through pure address arithmetic, including
array-decay steps — `a[i][j]` vs `b[i][j]` on distinct file-scope arrays. That is the same
`ast_dep_base_distinct` rule 1-D global arrays have always used; nothing was weakened to
get it. Still refused, deliberately: anything reached through a genuine pointer load
(`p[i][j]` vs `q[i][j]`, whether the pointers are parameters or globals); any pair of
*frame-local* arrays, because `ast_dep_base_distinct` returns 0 for `base_kind == 2` and a
frame offset can also be a temp holding a loaded pointer; and `a[i][j]` vs `a[k][l]`, which
still goes through the direction test.

**Measured** (`MCC_LOOP_CENSUS_RUN=1 tools/loop-census.py cmake-debug --corpus runtime`,
and the same with `--alias-oracle`; 17 kernels, 2,246,355,539 iterations, identical to the
run in the table above):

| | before | after | `--alias-oracle` ceiling |
| --- | ---: | ---: | ---: |
| parallel-legal iteration-weighted fraction | 1.39% | **80.60%** | 80.60% |
| entered loops `par=1` | 14 | **26** | 26 |
| `bases-may-alias-indirect`, iteration-weighted | **79.35%** | **0.00%** | 0.00% |
| hottest loop removed | 1.39% | **3.67%** | 3.67% |
| largest program (`matmul.c`) removed | 1.39% | **3.66%** | 3.66% |
| self-compile of `src/mcc.c` | 0.01% | **0.01%** | 0.01% |

The after column and the oracle column are the *same report, byte for byte* (`diff` of the
two census outputs is empty). All 79.35 points convert, and they convert soundly:
`-fdep-alias-oracle` is now worth **zero** on this corpus, having been worth 79.2 points
yesterday. It is not removed — it still bounds workloads whose bases are pointers.

**The same skepticism, applied to this headline.** It does not survive it, and it was never
going to: 76.9% of the corpus's iterations are one loop, `matmul.c:22`, and 80.60% becomes
**3.67%** the moment that loop is dropped and **3.66%** when the whole `matmul.c` program
is. Under the new predicate exactly the same three kernels of seventeen carry everything
that the oracle said would — `matmul`, `loopnest` (whose hot nest *is* the same i-k-j
matmul) and `vlaloop`. What changed is that the number is now the predicate's own answer
rather than a ceiling; what did not change is that the corpus's lane source is still dense
matrix multiply, and that is still not evidence about real programs. Hazard 1 above is
retired only in the sense that the two columns now coincide; hazard 2 stands unaltered.

**Emitted code did not move.** `ast_loop_parallel_legal` still has exactly two call sites,
both diagnostic (`grep -rn ast_loop_parallel_legal src tools include runtime` → one
declaration, one definition, `ast_loop_par_census`, `ast_loopdep_dump`), so the decode
change cannot reach codegen; the `mccrir.c` type stamp can, so it was measured rather than
argued. 721 TUs from `tools/`, `runtime/lib/` and `tests/**` compiled at `-O0/-O1/-O2/-O3`
= **2884 objects, 2884 byte-identical**. Eight of them hashed differently across the two
runs and all eight are `__TIME__` string literals: compiling them back-to-back with the
pre-change and post-change binaries gives identical bytes at all four levels. The only
other object that moved is `tests/loopcensus/known_deps.c`, whose source this change edits.

**The cell that fails without it.** `tests/loopcensus/known_deps.c` gains `m2[32][32]`,
`gp`/`gq`, and three functions; `known_deps.expect` gains four rows. On pristine
`70b92fb3`, `tools/loop-census.py cmake-debug --partest` reports **12 mismatches** at
`-O1/-O2/-O3`: `dp_nest_two_arrays` (both loops) wants `par=1` and answers `par=?`, and
`dp_gptr_alias` wants `par=?` and answers `par=1`. `dp_nest_two_rowptrs` — the same nest
through two `int (*)[32]` parameters — must stay `par=?` in both trees, and does; it is the
soundness control, not a positive.

**What this does not do.** Frame-local 2-D arrays (`double x[N][N]; double y[N][N];` inside
a function) still decline, because base distinctness is only implemented for symbols. See
`#### LANDED — the unguarded callers were a live miscompile` below for what happened to the
rest of this paragraph: the unaudited callers were reachable and are fixed, and the
frame-local item was measured and does not pay.

#### LANDED — the unguarded callers were a live miscompile, in all three shipped passes

**The prediction was right and it understated the blast radius.** The row above flagged
`ast_loop_interchange_legal` and `ast_dep_fusion_pair_illegal` as calling
`ast_dep_base_distinct` with no `indirect` guard, and asked whether that could reach emitted
code. It could, in **three** passes, not two — `-floop-interchange`, `-floop-block` and
`-floop-fusion`, all `MCC_OPTD_LEVEL(12)`, all on by default at `-O12`. Every one of them
produced a wrong answer on pristine `72c60f84`, with **no** `-fdep-alias-oracle`:

| pass | reproducer (globals `gp`/`gq` both assigned the same object) | correct | shipped |
| --- | --- | ---: | ---: |
| `-floop-interchange`, `-O12` | `int (*gp)[8],(*gq)[8];` `for i for j gp[j][i] = gq[j-1][i+1]+1;` | `1526249087836454304` | `843238774219177898` |
| `-floop-block` | same nest, constant bounds | `8322330699240940126` | `-6047003803493180258` |
| `-floop-fusion` | `int *gp,*gq;` `for i gp[i]=…;` then `for i out[i]=gq[i+1];` | `-6383020598026989609` | `-6585256009372028400` |

`-fdump-loopdep` on the first prints `interchange(outer#5,inner#17): legal` over two refs
both marked `INDIRECT` whose direction vector is `(<,>)` — the one shape interchange must
never take. The dependence is real; the pass only failed to see it because two distinct
*pointer variables* were read as two distinct *objects*.

**Two things had to be true at once, and both are easy to hit.** The bases must be pointer
loads (so `indirect` is set and the guard was the only thing standing between the pass and a
wrong answer), and the loops must clear the pass's own preconditions. The second is what
made the first fusion attempt read as unreachable: `ast_dep_same_trip` needs
`ast_loop_bounds`, which wants a **constant** bound, so `for (i = 0; i < n; i++)` declines
and `for (i = 0; i < 32; i++)` fuses. A reachability probe that only tried variable bounds
would have reported this row closed. It is not.

**The fix.** `ast_dep_base_distinct` takes an explicit `allow_indirect` argument and refuses
`indirect` bases unless the caller opts in. `ast_loop_parallel_legal` passes
`ast_dep_alias_oracle_env` — the deliberately-unsound census ceiling, unchanged, and now the
*only* place that can reach it. The three emitting passes pass `0` and are sound
unconditionally, including under `-fdep-alias-oracle`, which previously could not have
helped them because they never consulted the flag at all. Making the parameter explicit
rather than reading the env global inside the predicate is the point: a future caller has to
write the `0` or the `1`, so it cannot inherit the hole by omission the way these three did.

**Emitted code did not move where it must not.** 366 TUs from `src/`, `runtime/`, `tools/`
and `tests/**` compiled at `-O0/-O1/-O2/-O3` with the pre- and post-change compilers =
**1464 objects, 1464 byte-identical, zero differing** (no `__TIME__` noise: this corpus
produced none). At `-O12`, where the three passes are live, **363 of 366 are byte-identical**
and the three that differ are exactly `tests/exec/optimizer/loop_{interchange,fusion,tile}.c`
— the files this change adds the aliasing cases to. Nothing else in the corpus was relying on
the unsound answer, which is why this was invisible: the hole was wide, and the corpus never
stepped in it.

**The cells that fail without it.** `tests/exec/optimizer/loop_interchange.c` gains
`RP`/`RQ` (`int (*)[N]`) and `rowptr_skew()`, `loop_tile.c` gains `aliased_rowptr_skew()`,
`loop_fusion.c` gains `aliased_ptr_backward_dep()`, each aliased onto one array in `main`;
three `tests/exec/goldens.h` strings move. Ablating only `src/mccast.c` and rebuilding:
**17 cells fail** — `exec-interchange/{loop_interchange,loop_tile}`,
`exec-fusion/loop_fusion`, `exec-tile/{loop_interchange,loop_tile}`, and
`exec-search{,-emitsize,-emitiso,-threads}/{loop_interchange,loop_fusion,loop_tile}` — and
all 17 pass with the fix. The `tests/optfire/` interchange/fusion/tile pins use file-scope
`static` arrays with direct bases, so the guard cannot reach them and they are untouched.

**Frame-local array distinctness (`base_kind == 2`): measured, and it does not pay.** The
paragraph above says the blocker is that a `base_kind == 2` offset cannot be told apart from
a compiler temp holding a loaded pointer. That is **no longer true** — the `indirect` flag
already separates them, and `-fdump-loopdep` shows it directly: two local `int[64]`s decode
as `base=@-264` / `base=@-520` with no `INDIRECT`, two local `int *`s decode as
`base=@-32 INDIRECT` / `base=@-40 INDIRECT`, and a local array against a local pointer gets
one of each. The folding hazard does not bite either: `*(x + 8 + i)` decodes as
`base=@-264[8][1*@-268]`, i.e. the constant becomes a *subscript*, not a second base offset,
and local struct/union members decline at decode. So the work is smaller than this row
claims. It still should not be done, because the prize is not there:

| | self-compile of `src/mcc.c` | 17-kernel runtime corpus |
| --- | ---: | ---: |
| `bases-may-alias` (non-indirect), iteration-weighted | **0.72%** (6 loops) | **0.00%** (absent) |
| current parallel-legal fraction | 0.01% | 80.60% |

0.72% is an *upper* bound — it counts every non-indirect base mismatch, including
symbol-vs-local pairs that frame-local distinctness would not convert. On the corpus the
reason does not appear at all. Converting all of it would move the self-compile from 0.01%
to at most ~0.7% on a workload where 83.9% of iterations are in loops that call a function or
contain a `goto`. Not worth the soundness surface. **Do not implement without a workload
that shows the reason.**

### 2. ~~Replay fidelity — ~4.3 points of `kept`~~ — MEASURED 2026-08-09; the prize is 2.60 points and it does not pay

**Both halves are now measured and the pins do not move.** The row was a licence to
re-measure, so it was re-measured, on a fixed snapshot of `src/` + `include/`, `n=17`–`21`
interleaved reps, against `cmake-debug/mcc`. The compile-time half costs **+1.499%** of
stage-1 `instructions:u` for `-fchain-store -fchain-store-live -fchain-store-member`
(5,439,530,105 → 5,521,074,220; two independent arms of the same config agree to
±0.001%, so the counter's own floor is 0.003%), and **+1.627%** of stage-1 CPU time at
n=21 against an identical-config floor of **+0.455%** measured in the same run — 3.6× the
floor with the counter agreeing in sign. The emitted-code half — the number this
row said was **UNMEASURED**, and the first move it asked for — is:

| what | with the family on | metric |
| --- | ---: | --- |
| stage-2, the emitted compiler doing a fixed `-O2` self-compile | **−0.079%** | `instructions:u`, floor 0.004% |
| the same, on the clock | −0.220%, **inside** the ±0.435% floor | CPU time, n=21, control +0.002% |
| `sieve`, the one kernel of 17 whose object changes at all | **−1.97%** | `instructions:u`, min of 5 |
| output of every changed kernel and of both stage-2 compilers | identical | `cmp` / `md5sum` |
| `tests/exec` + `tools` + `runtime/lib` + kernels, 357 TUs | 3 change, net **−75** instructions | `objdump -d` |
| stage-1 `src/mcc.c` object | −21 at `-O2`, **+1,723** at `-O3` | `objdump -d` |

So the trade is **1.50% of compile time for 0.079% of stage-2**, a 19:1 loss on the
self-host axis, plus a real but singular 1.97% on `sieve`. Compare `divmagic`, which the
same rule keeps at rung 2: 0.44% of stage-1 for 28.2% on `divmod`, sixty times the
efficiency. `chain-store` stays at 11, `chain-store-live` at 10, `chain-store-member` at
11; no level moved, `tests/optfire/leveltime.tsv` and `defstate.txt` are untouched,
`levelpins.txt` gained the note and nothing else, and
`optfire{,-i386,-riscv64,-arm64}/chainstore` still fire.

**The sieve number is not new, which is the useful part.** `tests/optfire/leveltime.tsv:130`
already banked `chain-store sieve +26.190 +1.9658 32.18 inside layout floor` — the
layout-immune counter had said 1.97% all along and the row was demoted on the correct
reading that the *clock's* +26% was inside sieve's 32% floor. This re-measurement
reproduces **1.9655%** independently, four figures, from a different harness. What has
changed since is reach, not size: after `56da2ab6`, `-fchain-store` **alone** changes
**0 of 17** kernel objects — sieve included, the very kernel that row was measured on —
and takes all three flags to get the same transform back.

**A per-flag sweep can no longer price this row, and the way it fails is instructive.**
`chain-store` alone measures **−1.859%** of stage-1 `instructions:u`, i.e. *cheaper than
the base*, and its four changed TUs are all **smaller** (net −81), and `src/mcc.c` shrinks
4,203 instructions at `-O3`. None of that is optimization: the flag makes 34 more bodies
unfaithful (2,816 → 2,782 replayed, `kept` 91.968 → 84.04), they stop running the strategy
suite, they stop being retainable by the inliner, and the object shrinks because less
happened to it. **A flag that saves compile time by suppressing optimization reads
exactly like a flag that is efficient**, and on this row it also reads as a code-size win.
That is a second way this tree's per-flag harness reports a number of the wrong quantity,
alongside the `fires`/`gain` trap in debt #6a. The rule it earns: *a flag that moves
`rir-coverage` must be priced with the coverage beside the counter, or the sign is
meaningless.*

**The 4.3 points did not reproduce, and the reason is a correctness fix, not a
mismeasurement.** With the family on, `self` `kept` measures **93.673 / 94.483 / 94.566 /
94.566** at `-O0`..`-O3` against a base of 83.069 / 91.903 / 91.968 / 91.968 — **+2.60**
points at the three shipped levels, not +4.3. The 96.204/96.232/96.301/96.301 quoted
below was taken at `55aaecb2`, before `56da2ab6` made `rir_chain_dup_ok()` refuse the
`ast_dup_sub` branch when the dup'd value reads the source store's target or carries an
`AST_StoreVal`. That fix removed a silent miscompile; it also removed 1.7 points of the
prize, which is the correct order of priorities and worth recording as such.

**And the exchange rate itself is now measured, for the first time.** +2.60 points of
`kept` bought 0.079% of stage-2. At that rate `kept` is worth about 0.03% of stage-2 per
point — which is why the `storeval-rot` row below, where 8.7 points of `kept` move in the
*other* direction and stage-2 moves 0.232% the *wrong* way for the flag that supplies
them, is not a contradiction but the same coin: `kept` says how many bodies run the
strategy suite, not whether the suite helped them. Row 1's "the currency that converts"
should be read with that constant attached.

Reproduce:

```
python3 tools/rir-coverage.py <bd> --corpus self --levels O0,O1,O2,O3
```
where `<bd>` is a directory of symlinks to `cmake-debug` whose `mcc` is a two-line
wrapper appending the three flags — `rir-coverage.py` has no switch for extra `-f` knobs
and scrubs `MCC_TEST_OPT`, so a wrapper is the only way to drive it, and that is why this
number had never been taken.

#### The record the row was opened on

`ast_run_strat_seq` gates **every** optimization strategy on `faithful`, so a body whose
arena replay does not reproduce the parser's bytes gets *no* strategies at all. Debt #6
below root-caused and fixed the fidelity bug — an unconditional `ast_dup_sub` collapse in
`rir_to_arena()`'s `IR_OP_VSTORE` case that was only tagged under the env flag — and
`tools/rir-coverage.py` measured the move: `kept` 83.122 → **91.960** on `self` and
93.006 → **96.653** on `wide`. `rir-coverage-census` is green after being red. Note the
correction inside that debt: the failure was at **-O1 as well**, not only -O2/-O3.

The same tool measured what is left. With `-fchain-store -fchain-store-live
-fchain-store-member` all on, `self` reached **96.204 / 96.232 / 96.301 / 96.301**, so
roughly **4.3 further points** of bodies are unfaithful for an *optimization* reason rather
than a defect — which means those bodies run no strategies today and would run all of them.
*(Superseded: those figures predate `56da2ab6` and now read 93.673 / 94.483 / 94.566 /
94.566, i.e. +2.60 points. See the re-measurement above.)*

This was the largest remaining number on the board denominated in **emitted code**, the
currency row 1 says still converts. Three things stood in the way, in order:

1. ~~**`mcc -O11` ICEs**~~ **FIXED 2026-08-08, and it was worse than an ICE.** `vstack
   leak (1)` on a chain feeding a 3-deep chain needed `chain-store` **and**
   `chain-store-live` together, which is why `-O0`..`-O3` were clean and nothing had ever
   seen it. A leak and an underflow *cancel*, so at `1fa038ee` the same shape also
   **silently miscompiled**: over 500 random chain-heavy functions at `-O11`, 212 ICE and
   **6 more return wrong answers**. Fixed at the source (`rir_chain_dup_ok` refuses the
   unfaithful dup; `ast_revoke_chainstores` drops half-pairs), cell `exec-chainlive/*`.
   Debt #6 carries the full diagnosis. **This row is no longer blocked by an ICE.** Debt
   #6a — a separate pre-existing `vstack leak (-1)` that fired at `-O1`/`-O2`/`-O3` — is
   fixed as of 2026-08-09, so the member fixture it used to block can now go in.
2. `tests/optfire/{defstate.txt,levelpins.txt,leveltime.tsv}` pin `chain-store` at level 11
   on measured stage-1 compile cost. Re-promotion is a compile-time-against-emitted-code
   trade, and those pins are the record of the compile-time half. This row is not a licence
   to re-promote; it is a licence to re-measure.
3. ~~**What the 4.3 points are worth in emitted code is UNMEASURED.**~~ **DONE
   2026-08-09** — see the table at the top of this row. It was not done with either named
   tool. `optlevel-bench.py` toggles one flag *off* from the shipped set and these three
   are already off, so it cannot reach them at all; `selfhost-optbench.py` does have an
   add-one-in arm, but it adds **one** flag, and this row's whole finding is that one of
   the three on its own reads backwards. What answers it is a stage-2 build with all
   three forced on, benchmarked against the shipped stage-2 on `instructions:u`.

Reach, already measured: over `tests/exec` + `tests/optfire/src` at `-O10`, `-fchain-store`
changes bytes in 3 files on x86_64 and 2 on arm64, and in **0** on i386/riscv64 post-fix.
So the emitted-code half may well come back small — which is exactly why it gets measured
before it gets promoted. It came back small: 3 TUs of 357 with the whole family on, net
−75 instructions, and one kernel.

### 3. Metal — ~~decide it, do not pay it down~~ — DECIDED 2026-08-09: dropped

**The decision is taken at the head of this file (`#### Metal — settled`): Metal is not a
device target.** Everything below is the evidence it was taken on, with today's re-count.

Debt #4 quantified the divergence and the answer is that this is a multi-week rewrite, not
a debt: **1754 MSL lines against 3612 SPIR-V** (`mccgpu.c` 653/1461, `mccgpu.h` 1008/1794,
`mccslice.h` 27/65, `spvgate.c` 66/292), `mcc_slice_frame_kernel_build`'s Metal arm is
**two lines returning 0** (`src/mccslice.h:1282-1285`), there are **zero `msl_region*`
symbols in `src/`** (re-verified 2026-08-09, 0 grep hits) against **31** distinct `spv_*`
symbols reached from `mccslice.h` alone, and `tools/slicerun.c` carries no backend `#if` at
all — its single conditional is `AST_EVAL_SLICE_PROVIDED` — so it cannot compile against the
Metal arm even in principle. `src/mccfmt.h:453` gates the whole device format emitter
`!MCC_GPU_LANG_MSL`, so that feature was already SPIR-V-only by construction.

**Three of those figures were stale and the direction of the error is the point.** The
board's `3578` / `1763` / `62` / `25` were taken before `99e043c1` (the `*p` and pointer
`++`/`--` landing) grew the SPIR-V arms; the MSL total of **1754 still matches to the
line**, because nothing has been added to it. The divergence measured itself while the arm
was nominally maintained, which is the whole argument for deciding rather than deferring.

It ranked this high because it is the only row that costs nothing to *decide* and grows more
expensive with every landing on the SPIR-V arm. Given row 1 — there is no dispatch site on
the SPIR-V arm either — the honest default is **drop Metal as a device target** and keep the
`#if MCC_GPU_LANG_MSL` arms only where they already compile. That is an owner's decision,
not a measurement; it was ranked here so that it stopped being taken by default, one landing
at a time.

### 4. ~~`rir_op_effect`'s 512-byte clear~~ — CLOSED 2026-08-08. It was **8.3–9.4% of stage-1**, not "well under 1%", and the arithmetic that said otherwise divided by the wrong self-compile time

```c
/* was, at both sites inside rir_op_effect */
for (q = o->vs_n + 1; q <= VSTACK_SIZE; q++)
        rir_pvok[q] = 0;
```

**The board's own bound was wrong by a factor of ~180, and the error is worth more than the
fix.** "Against a self-compile that takes ~90 s of `mcc`, that bounds the row well under 1%
of wall-clock" rests entirely on that 90 s, and **no self-compile on this host takes
anything like it**: plain `mcc -O3 -c src/mcc.c` is **0.50 s**, and the same compile under
`MCC_ARENA_DUMP` with `MCC_RIR_PROD=2` — the D4b row's own instrumented configuration,
which is the only other place this file says "~90 s" — is **0.98 s**. The pool-cap row two
sections down independently measured 0.5641 s for the same workload and agrees. Divide
105.1M one-byte stores by 0.5 s instead of 90 s and the arithmetic bound is ~10%, which is
what the clock then said. **A denominator carried between rows is a measurement only if
someone re-takes it.**

**Measured, and it is the largest compile-time item ever taken off this tree.** Method,
`cmake-debug` `mcc` (the `-g`, no-`-O` host build, which is the stage-1 reference compiler
these rows are denominated in), fixed snapshot of `src/` + `include/` in a scratch tree so
both binaries compile identical bytes:

| | before | after | delta | floor |
| --- | ---: | ---: | ---: | ---: |
| stage-1 `-O3` CPU time, median of n=21 interleaved | 0.5083 s | 0.4661 s | **−8.30%** | ±0.55% |
| stage-1 `-O2` CPU time, median of n=21 interleaved | 0.5003 s | 0.4531 s | **−9.44%** | ±0.55% |
| `instructions:u`, `perf stat -r 5`, `-O3` | 5,465,561,225 | 4,794,023,991 | **−12.29%** | — |
| `cycles:u`, same run | 2,563,123,360 | 2,320,810,912 | −9.45% | — |

The layout-immune counter agrees in sign and exceeds the time delta, which is the shape a
real removal of work has and a layout accident does not. Run-to-run sd was 1.82% / 1.38%
of the mean at `-O3` and 0.73% / 0.44% at `-O2`, so the delta is 5–20 sd.

**`perf` located it before the fix and confirms it after.** 20 self-compiles under
`perf record -F 9999 -e cycles:u` (101,801 samples): `rir_op_effect` was **11.15%** of user
cycles, and summing the per-instruction samples over the two clear loops gives **89.52% of
that symbol** (88.01 for the `PUSHLIT`/`VSETC` site, 1.51 for the `VPUSHSYM` twin) —
**9.98% of the whole self-compile**. That is 255M cycles for the census's 107.3M
iterations, i.e. **2.4 cycles per iteration**, exactly an unoptimised loop with a
stack-resident counter; and the 671.5M-instruction delta over 107.3M iterations is
**6.3 instructions per iteration**, exactly the 7-instruction body `-O0` emits. The census
count and the clock agree to within a rounding, which is the cross-check that makes this a
measurement rather than a coincidence. After the fix `rir_op_effect` is **1.33%** and
`__memset_avx512_unaligned_erms` is unmoved at 1.66% — the new `memset` calls are too short
to appear.

**The fix is a high-water mark, and it is provably equivalent, not merely tested.** A new
`rir_pvhw` holds the largest index that may be non-zero. Both sites clear only
`(vs_n, rir_pvhw]` with a `memset` and then set `rir_pvhw = vs_n`; `rir_to_arena`'s existing
`memset(rir_pvok, ...)` zeroes it. The invariant is `rir_pvok[q] == 0` for every
`q > rir_pvhw`, and the whole array has exactly five writers and one reader
(`rir_prov_ok`), so it closes by inspection: the two sites re-establish it, the two clears
in `rir_leaf_slot` only lower entries, and the reset zeroes both. The old loop's
postcondition — everything above `vs_n` is zero — is preserved bit for bit, so no
observation point can tell the two versions apart.

**Emitted code is unchanged, checked and not assumed.** 366 TUs across `tests/exec`,
`tools` and `runtime/lib` × `-O0/-O1/-O2/-O3` = 1,464 compiles: **1,400 objects
byte-identical, 0 differing**, 64 TUs that fail to compile standalone on both binaries
alike. The amalgamated self-compile object is byte-identical at `-O2` and `-O3` as well.
`ctest` 9120/0, `-L flagsweep` 118/0, `-L stratsweep` 30/0, `MCC_RIR_CENSUS=1 -L census`
green, `tools/selfhost-smoke.py` green. No ratchet moved and none was re-banked.

**What this leaves for row 1, re-run rather than predicted.**
`MCC_LOOP_CENSUS_RUN=1 tools/loop-census.py cmake-debug --levels O2` on the fixed tree:
total loop iterations in a self-compile **164.6M → ~52.2M**, and the parallel-legal
iteration-weighted fraction **65.75% → 0.01%**. Not the 1.88% the section above predicted
with the hottest loop removed — that figure still counted the twin, which is also gone now.
The raw, dependence-ignored fraction is 52.46%, and the hottest loop in the compiler is now
`ast_strpool_find_or_add` (`mccast.c:3720`) at 11.3% of all iterations, `par=?`. The twelve
array-fill loops are all that is left on the parallel side and they total under 180,000
iterations in an entire self-compile.

**The verdict is unchanged and is now much harder to argue with.** It was already written
against the with-it-removed figure; the removal has happened and the number came back an
order of magnitude *smaller* than predicted. The 65.75% / 1.88% pair in the verdict section
and in section 1 is stale as of this landing — left in place deliberately, because that
section is being edited elsewhere, but read those two numbers as **0.01%** and treat the
`rir_op_effect` rows in its loop table as retired.

### 5. D4b — internal calls on the device — **803 blocks**, in a currency that does not convert

**12,901 / 78.01% does not reproduce, and the number that predicts payoff is 16× smaller.**
The census is now a committed tool rather than an ad-hoc pass: `slicerun --arenas <dump>
--census` joins the per-`AST_BasicBlock` unit it already had to the callee class the
`[inv]` records already carried, using `tools/node-census.py`'s classification (a callee
is INTERNAL when some body in the same dump defines it, INDIRECT when the dump wrote `?`,
EXTERNAL otherwise). Measured 2026-08-08:

| | `tests/exec` 60 @ `-O1` | self-compile (`mcc -O2 -c src/mcc.c`, `MCC_RIR_PROD=2`) |
| --- | ---: | ---: |
| bodies / non-empty blocks | 344 / 947 | 2,810 / 19,454 |
| blocks eligible today | 319 | 4,029 |
| blocks containing ≥1 `AST_Invoke` | **454** | **10,238** |
| — all callees internal | 169 (37.2%) | **7,524 (73.49%)** |
| — all callees external | 197 (43.4%) | 1,231 (12.02%) |
| — mixed | 87 (19.2%) | 1,368 (13.36%) |
| — any indirect callee | 1 (0.2%) | 115 (1.12%) |
| **`Invoke` is the SOLE blocker** | **33** | **803** |
| blocks that are nothing but calls | 9 | 926 |
| unblocked by the leaf inliner below | 7 | 4 |

The `tests/exec` row reproduces `docs/DEVICE-LIBC.md`'s 454 exactly, so the disagreement
is not one of method. The compiler row's base is 10,238, not 16,537, and the internal-only
share is 73.49%, not 78.01% — close enough that the *ratio* survives, far enough that the
*count* does not: 7,524 against 12,901.

**And 73.49% is not a payoff figure at all.** It is an eligibility statement about the
`AST_Invoke` node alone — "no callee in this block is external or indirect" — and it makes
no claim that anything else in the block lowers. The figure that does make that claim is
the last-but-two row: blocks `mcc_slice_frame_stmt_ok` refuses today and would accept if
every `Invoke` in them were free, counted only when every argument of every such `Invoke`
is itself lowerable. That is **803 blocks: 7.84% of the Invoke-blocked set, 4.13% of all
blocks, and +19.9% on the 4,029 eligible today.** It is the block-granularity twin of what
`rir_low_take` already banks per body, where `call` is the sole blocker of ~0.01% of body
bytes, and the two agree on direction by a wide margin.

**Re-ranking, second pass, 2026-08-08 evening.** 803 blocks is still the largest single
number anyone has measured at block granularity on the compiler corpus, and its original
headline was 16× too large. Both facts are now beside the point: 803 is denominated in
*device-eligible blocks*, and row 1 shows that currency has no exchange rate in this
workload — nothing dispatches these blocks, and debt #0 says why. This row keeps its
position for its evidence and for the leaf inliner it produced, not because the number
predicts a speed-up. The old cross-reference to "rows 1 and 2" is retired: both were
re-taken with committed tools on the same day (see the provenance table above).

The `tests/exec` column is now **ratcheted** by `slice/census`
(`cmake/slicerun_census.cmake`): the corpus-and-classifier figures (947, 454, 169/197/87/1)
are asserted exactly, and the two predicate-dependent ones are floors, because a cell that
forbade those from rising would be a cell against the project. The compiler column is not
banked — one self-compile is 5 s of census but ~90 s of `mcc`, which is a `ctest -L census`
opt-in and not a default cell.

**The first increment is landed, and it is not `OpFunctionCall`.** SPIR-V compute has no
recursion and the corpus has a ~130-function SCC, so the call boundary cannot be taken in
one step. `src/slice_inline.h` takes the case that has no boundary: a callee whose whole
body is `BasicBlock { Return <pure expression over the parameters> }` is substituted into
the caller's arena, argument subtrees for parameter refs, with `AST_Convert` nodes
materialising C's argument and return conversions. Every consumer downstream then sees a
tree with no `AST_Invoke` in it, which is the point — the board's hard precondition is
that the CPU reference has **no `AST_Invoke` case at all** (still true: zero occurrences
in `ast_eval_slice.h` and `mccslice.h`), so an arm that teaches one executor about Invoke
and not the other is a vacuous differential. Rewriting the shared tree makes the reference
arm and the emitter arm the same arm.

Refused, because a wrong graft would be identical in both executors and the differential
could not see it: indirect callees, callees that touch memory, callees with control flow
or a second exit, and any body whose used frame offsets are not exactly `-8, -16, …`
(mcc spills incoming scalars in declaration order; verified against `asr32(x, n)` in
`tests/exec/arch/arm64_encoding.c`, whose `>>` has the `-8` ref on the left, and against
`mix3(a, b) = a * 3 + b` in the fixture). `MCC_SLICE_INL_DUMP=1` prints each graft.

`slice/inline` (`cmake/slicerun_inline.cmake`, fixture `tests/gpu/inline_leaf.c`) is the
three-toothed cell the board asked for, and all three are real on this host:

- `invoke-inlined=4` — the graft fires,
- `frame-stmts` 0 → 3 and `frame-compared` 2 → 3 — without the graft this corpus compares
  two frame runs with **zero statements** between them, so the un-inlined arm is provably
  blind and a green cell there would mean nothing,
- `--mutate` gives `frame-mismatches=1` with the graft and `frame-mismatches=0` without
  it, so the redness is attributable to the inlined call rather than to the
  expression-slice arm reddening the process on its own.

On the whole `tests/exec` corpus the same switch takes `frame-compared` 244 → 251,
`frame-stmts` 201 → 236 and `dispatches` 1,023 → 1,114 with `invoke-inlined=130` and zero
mismatches. `--no-inline` restores the old behaviour.

**Still open.** Real calls — anything with a store, a loop, a second return, or its own
call — are untouched, and the 803 figure is what they would be worth in total. Indirect
callees (115 blocks) still have no device answer anywhere in the plan. Recursion still has
no data: `docs/PLAN.md`'s ~130-function SCC and the dynamic depth figures (`unary()` peaks
at 11, `ast_replay_bb` at 35 with a 27,424-byte frame → 1.75–3.5 MiB/lane) are the whole
of it.

**The compiler now sets the hook — measured 2026-08-08.** `mcc_slice_leaf_hook` is a
host-supplied resolver because the callee body is not in the caller's arena. `mcc` now
supplies one: `ast_slice_leaf_pool` in `src/mccast.c` takes the callee `Sym` from child 0
of the `AST_Invoke` and looks it up in `ast_inline_pool`, the compiler's own retained-body
pool (`ast_inline_body` does not exist; `ast_inline_pool` + `ast_root` is the whole
interface). Refusals stay exactly the harness's, because they all live in
`mcc_slice_leaf_scan`, which is unchanged: indirect callee (no `Sym`, no pool entry),
memory, control flow, a second exit, or used frame offsets that are not exactly
`-8, -16, …`. `ast_arena_has_hole` is checked on the pool arena as well. The pool is
strictly better identified than the harness's resolver — `Sym` pointers, not names, so the
two-static-functions-with-one-name hazard `leaf_offer` guards against cannot arise.

**When the pool is populated, and by how much.** `ast_inline_retain` runs at the end of
each function body, so at the moment `mcc` grafts into caller *N* the pool holds bodies
`1..N-1` — a callee defined after its caller is never resolved. `ast_fn_inlinable` gates
it on `-finline-functions`-or-`-finline` and on `VT_STATIC` (or a C99 weak inline body),
so the pool is **empty at `-O0` and `-O1`** and the hook has nothing to resolve there;
`-O2` and `-O3` populate it. Measured on the fixture: 0 grafts at `-O0`/`-O1`, 4 at `-O2`
and `-O3`, and `MCC_SLICE_INL_DUMP=1` prints all four, with `mix3(x,y)` → `x*3+y` and
`mix3(y,x)` → `y*3+x` on distinct `Sym`s, which is the argument-order check.

**Where it is wired, and where it deliberately is not.** The graft runs on a *clone* at
the compiler's two slice consumers — `ast_adump_body` (the `MCC_ARENA_DUMP` arenas the
harness reads) and `ast_ladder_census` (the compiler's own CPU-vs-GPU slice oracle). It
does **not** touch `ast_cur`, so no byte of emitted code changes: `ctest` is 8948/0 and
`tools/selfhost-smoke.py` is green with it on by default.

**Self-compile: `invoke-inlined=40` out of 20,219 `AST_Invoke` nodes seen**, on
`mcc -O3 -c src/mcc.c`. Census over the compiler-grafted dump against the same dump with
`MCC_AST_SLICE_INLINE=0`, both read with `--no-inline` so the harness grafts nothing:
`eligible` 3,821 → 3,822, `inv-blocks` 9,212 → 9,208, `inv-sole-blocker` 754 → 753. **One
block.** The reason is not the graft, it is the pool: `AST_INLINE_MAX` is 512 and
`ast_inline_n` saturates at 512 long before the leaf-shaped callees are reached, so the
resolver answers for a fixed early prefix of the translation unit. The harness's
name-indexed table over the same dump grafts 118 where the pool grafts 40, and that 3×
gap is the pool cap plus the `static`-only and define-before-use rules — none of which are
properties of the graft.

**The pool cap is gone, the reach nearly doubled, and it bought one more block —
2026-08-08.** `ast_inline_pool` was a fixed `AST_INLINE_MAX`-entry array searched by
**linear scan** from eight call sites, seven of them once per `AST_Invoke` node. That is
what the cap was really protecting: not memory (~200 B/entry) but the scan, so raising the
constant alone trades a reach limit for a compile-time regression. Measured, and it does:
`AST_INLINE_MAX 8192` with the scan left in place costs **+1.23% of stage-1 CPU time**
(n=21 interleaved, `-O3`, fixed snapshot of `src/mcc.c`), which is outside `levelpins`'
own ±0.55% stage-1 floor and would be a real regression.

So the scan went first. The pool is now a `mcc_realloc`'d array with an open-addressing
`Sym *` → slot index beside it (`ast_inline_index`/`ast_inline_find`, load factor ≤ 0.5,
cleared in `ast_configure`, freed in `ast_teardown`), every one of the eight scans is an
O(1) probe, and with the lookup O(1) the cap has nothing left to protect and is deleted —
the pool grows to whatever the TU has. Stage-1 CPU time, same n=21 interleaved runs over
the same fixed snapshot:

| build | `-O3` stage-1 | `-O2` stage-1 |
| --- | ---: | ---: |
| cap 512, linear scan (before) | 0.5641 s | 0.5558 s |
| cap 512, hashed | −1.90% | −1.78% |
| **unbounded, hashed (shipped)** | **−0.42%** | **−0.37%** |
| cap 8192, linear scan | +1.23% | — |

**So the reach was bought for nothing**, which is the point: the shipped row is −0.42% /
−0.37%, i.e. *inside* the ±0.55% floor and therefore not a cost and not a win either — the
index pays for the larger pool, no more. The middle row is what the index is worth on its
own (−1.9%, outside the floor) and the last row is the regression that raising the
constant alone would have shipped. Peak RSS 44.7 → 48.4 MiB (+3.7 MiB, the 552 extra
retained bodies). Emitted `-O3` object 3,516,447 → 3,569,983 bytes (+1.52%), because
`AST_STRAT_INLINE` reads this same pool and now sees 1,060 candidate bodies rather than
the first 512 — **removing the cap changes codegen, it is not a census-only knob**. It
changes it only where the cap actually bound: over 344 TUs (`tests/exec`, `tools`,
`runtime/lib`) the before and after binaries emit byte-identical objects at `-O0`, `-O1`
and `-O2`, because none of those TUs has 512 retainable bodies. In this repo the
amalgamated compiler is the only TU that does.

**What it bought, measured like-for-like** (both binaries over the *same* snapshot source,
so the base dump is the same 19,844 blocks; both censuses `--no-inline`):

| | grafts | `eligible` | `inv-blocks` | `inv-sole-blocker` |
| --- | ---: | ---: | ---: | ---: |
| before, cap 512 | 41 | 4,361 → 4,362 | 10,381 → 10,377 | 835 → **834** |
| after, unbounded | **72** | 4,361 → **4,363** | 10,381 → **10,375** | 835 → **833** |

**One block became two.** The pool high-water is 1,060 and no longer saturates, the
compiler's resolver now answers 72 of the 125 leaf grafts the harness finds over the same
dump (was 41), and the census moved by one more block. The bases differ from the 3,821 /
9,212 / 754 triple above because that was taken before the `*p` landing moved `eligible`;
the before-row here is this tree's own baseline, re-taken.

**Which of the remaining limits bind, measured.** Not the body-size limit:
`MCC_AST_INLINE_NODES=100000` more than doubles the pool (1,064 → 2,285 bodies, this
tree's own source) and the grafts stay at **72** — leaf-shaped callees are small, so
`ast_inline_node_limit` (64) is not on this path at all. Barely the `VT_STATIC` gate:
lifting it in a throwaway build takes the pool to 1,394 and the grafts 72 → **74**, worth
two. That leaves **~51 of the 53 residual
grafts on the define-before-use rule** — `ast_inline_retain` runs at the end of each body,
so caller *N* only ever sees callees `1..N-1`. A deferred second pass would have to retain
every body's arena to the end of the TU, and on the evidence above 51 more grafts are
worth about one more census block. **Measured, not worth it** — the ordering rule stays.

**Dispatch does follow, but through binding 1, not binding 2.** Debt #0 is still true:
`mcc_slice_frame_from_ast` has no call site outside `tools/slicerun.c`, so no frame kernel
is dispatched by the compiler. The ladder census is a different consumer and it does reach
the device. With the compiler's own `-O3` arena inliner disabled (`MCC_AST_INLINE_LIMIT=0`,
so the `AST_Invoke` nodes actually survive to the census) the leaf graft takes
`ladder-self` pairs 6 → 9, `ladder-cross` pairs 2 → 9, GPU rungs 28 → 48 and
**dispatches 57 → 97** (lanes 666,556 → 1,061,644) with every verdict unchanged
(certified 6/6 → 9/9, differ 0). Without that knob the number is zero at `-O3`, and that
is itself the finding: **`AST_STRAT_INLINE` has already eaten these calls out of the arena
by the time the ladder census runs**, so at `-O3` the leaf graft's marginal value at that
particular consumer is nil. It is the pre-optimisation `ast_adump_body` arena where the
graft has something to do.

`slice/mcc-leaf-graft` (`cmake/mcc_leaf_graft.cmake`) is the three-toothed cell for the
compiler path, and it is the compiler's graft under test rather than the harness's:
both arms pass `--no-inline` and assert `invoke-seen=0 invoke-inlined=0`, so anything the
grafted arm gains, it gained because `mcc` emitted an arena with no `AST_Invoke` in it.

- `[slice-inline] invoke-inlined=4` on a real `-O2` compile,
- `frame-stmts` 0 → 3 and `frame-compared` 2 → 3 — the ungrafted arm compares frame runs
  with zero statements in them and is provably blind,
- `--mutate` gives `frame-mismatches=1` on the grafted dump and `0` on the ungrafted one.

Negative control taken: flipping the `MCC_AST_SLICE_INLINE` default to 0 turns the cell
red.

**Still open here.** Callees defined after their callers, measured above at ~51 grafts and
about one census block, and therefore declined. `-O0`/`-O1`, where there is no pool at all
and a leaf resolver would have to be built from something else. And the graft still buys
the compiler nothing it emits — it feeds diagnostics and the ladder oracle, because debt
#0 means there is no frame dispatcher to feed.

**And the honest verdict on the pool row: it is closed, and it was not worth much.** The
limiter really was the cap, the cap really is gone, the reach really did go 41 → 72, and
the whole of it is **one additional block out of 19,844** on a number that is a lowering
census, not a speed-up — the parallel-legal iteration-weighted fraction in this workload
is **0.01%** (the `~1.88%` this paragraph carried is **STALE**, pre-`415b736c`) and nothing
dispatches a binding-2 kernel, so no result on this row can be a
performance win. Nor is the compile time: the shipped build lands inside the stage-1
floor, so the honest statement is that the extra reach cost nothing, not that it gained
anything. Anyone reading this row for the next increment should read the 803-block figure
at the top of it, not this paragraph.

Verified: `cmake-cross` **9114 cells, 0 failures** (with `cmake-debug` and `cmake-cross`
both built — the same command registers **9136** today, and see hazard 5 for why the same
count taken from a `cmake-debug` configured first is 164 low),
`tools/selfhost-smoke.py cmake-debug` green, and the three ratchets that
could have seen this — `rir-coverage`, `node-census`, `rir/drop-ratchet` — all pass
unchanged, so the arena node counts did not move enough to dilute and nothing was
re-banked.

### 6. `snprintf` — `%s` has landed; what is left is the module budget, not the engine

Rewritten 2026-08-09 on `wt/fmtbudget`. Every number below is reproducible from
`tools/fmt-census.py`, and the numbers are now the compiler's own rather than a port's:

```
tools/fmt-census.py --oracle=cmake-debug/slicerun --refused
```

makes the census ask `slicerun --fmt-verdict`, which calls the real `mcc_fmt_compile`.
Without `--oracle` it still answers from the Python port; the port is kept because it
makes the tool runnable with no build, and it is now *gated* rather than trusted.

**The port had drifted, and the 140/162 this row used to quote was wrong.** `fmt_compile`
in `tools/fmt-census.py` appended one item per literal byte where `mcc_fmt_lit` merges a
run into a single item, so any format with more than 24 leading literal characters tripped
the port's `MCC_FMT_MAXITEM` check and was reported `module budget` while the compiler
accepted it. Over the 984 printf-family literal formats in `src/*.c` the two disagreed at
**61 formats**; restricted to the 162 `snprintf` sites it cost **two**, so the real
pre-branch verdict was **142/162 (87.7%) with 15 budget refusals**, not 140/162 with 17.
The port also charged `\n` and `\t` as two literal bytes each, because it never decoded C
escapes; that one does not move the 162 but did overstate every affected cost. Both are
fixed, and the *class* is closed rather than re-documented as debt:

| cell | what it does |
| --- | --- |
| `fmt/census-oracle` | pushes the 984 corpus formats plus 40,033 generated ones through the port and through `mcc_fmt_compile` and fails on the first disagreement in verdict *or* cost. 41,017 formats, 0 disagreements, 0.9 s |
| `fmt/census-oracle-known-positive` | puts the literal-run bug back with `--mutate` and requires the check to go red. 759 disagreements, so the cell is not comparing nothing |

The generated corpus is deterministic and deliberately walks the limits the drift lived
at: literal runs of 1/23/24/25/191/192/193/200 bytes with and without a trailing
conversion, and random mixtures of every conversion spelling in the corpus including the
refused ones.

**The `(tag, value)` array is not needed and was not built.** 162 of 172 `snprintf` call
sites in `src/*.c` carry a compile-time-constant format, so the format is parsed at *emit*
time and the emitter lays down straight-line code: constant byte runs stored directly, one
inlined conversion per specifier. That removes three problems at once — no `%` scanner on
device, no format string to place in binding 2, and no tag array, because the emitter
already knows each argument's `type_t`. If the 10 non-literal sites are ever wanted the
shape is `{u32 tag; u32 pad; u64 value}`, 16 bytes, tag = the AST `type_t`; do not invent
a second type enum.

**Site coverage, measured by running the real compiler over the corpus:**

| | sites | share of 162 |
| --- | ---: | ---: |
| accepted by `mcc_fmt_compile` today | **148** | **91.4%** |
| — of those, sites carrying at least one `%s` | 100 | 61.7% |
| refused: the straight-line program exceeds the module budget | 9 | 5.6% |
| refused: flag, width or precision on a signed conversion | 4 | 2.5% |
| refused: `%.17g` | 1 | 0.6% |
| refused: `%p` | 0 | 0.0% |

142 → 148 is the narrow-conversion path landed on this branch, below. 140 → 142 is the
census bug above, not work.

`MCC_FMT_R_PTR` fired at **109 sites before and 0 after**. `%p` is 0 of 162 `snprintf`
sites — it appears only in `fprintf` — and it stays refused on purpose: glibc prints
`(nil)` for a null pointer, so a device `%p` that printed `0x0` would be a silent
wrong answer at the first integration, for zero corpus payoff. Of the 109 `%s` sites,
**100 are enabled and 9 are not**: eight exceed the module budget (`%s%s-%s` ×2,
`equiv rung=%s n=%d exact=%d inferred=%lu points=%lu`,
`differ rung=%s smallest-width=%d n=%d a=%lld b=%s%lld`, two long diagnostic sentences
in `mccgen.c`, `%s/kgc-%016llx-%u-%lu.z`, `arity %s n=%u nc=%u op=%d`) and one carries
`%2d` (`%s %2d %d`). `%smcc-me-%u-%u.c` and `%smcc-tmp-%u-%u.tmp` were on that list and
are now accepted.

**"52 of 162, 32.1%" was never the compiler's answer, and the correction is downward.**
That figure came from classifying each site by the set of conversion letters it uses,
which ignores flags and ignores whether the program fits. Feeding the same 162 formats
through the tranche-1 `mcc_fmt_compile` gives **49 accepted (30.2%)** — three sites carry
`%02d`/`%-10d`/`%2d` and were always refused — and applying the module budget that now
exists knocks that to **42 (25.9%)**. So the honest before/after for this row is
**42 → 148 sites**, and the gap between 52 and 49 is a defect in the old census, not a
regression.

Landed on `wt/fmt`, extended on `wt/fmtstr`, budget work on `wt/fmtbudget`:

| piece | where |
| --- | --- |
| format compiler, literal → program of runs, conversions and string copies | `mcc_fmt_compile`, `src/mccfmt.h` |
| cost model and the budget refusal | `mcc_fmt_cost` / `MCC_FMT_MAXCOST` |
| CPU reference, hand-written over region bytes | `mcc_fmt_exec` / `mcc_fmt_int` / `mcc_fmt_str` / `mcc_fmt_putb` / `mcc_fmt_getb` |
| device emitter, straight-line, no loop, no branch | `spv_fmt_emit` / `spv_fmt_int` / `spv_fmt_str` / `spv_fmt_putb` / `spv_fmt_getb` |
| 32-bit path for conversions with no length modifier | the `!it->wide` arms of `mcc_fmt_int` and `spv_fmt_int`; `MCC_FMT_NDEC32` / `MCC_FMT_NHEX32` / `MCC_FMT_C_DEC32` / `MCC_FMT_C_HEX32` |
| differential over every destination byte and the length | `suite_fmt`, `tools/slicerun.c`; cells `slice/fmt`, `slice/fmt-known-positive` |
| the compiler answering the census directly | `slicerun --fmt-verdict`, hex in / verdict out; cells `fmt/census-oracle`, `fmt/census-oracle-known-positive` |
| predicted-against-emitted words, printed by the cell | `slicerun fmt --fmt-cost-report` |

Covered: `%d %i %u %x %X %c` with the `l`/`ll`/`z`/`t`/`j`/`h`/`hh` spellings, literal
runs, `%%`, zero- or space-padded minimum width on the unsigned and hex conversions, and
now `%s` with minimum width, left-justify (`-`), constant precision (`%.5s`) and star
precision (`%.*s`). Star precision is its own program item that consumes its own
argument, so `narg` still equals the number of arguments the call passes. Explicitly out:

- `%p` — 0 sites, and `(nil)`. See above.
- `%f` `%g` `%e` `%a` — 14 float specifiers in the whole `printf` family across `src/*.c`,
  and exactly **one** of them at an `snprintf` site (`%.17g`, 0.4% of the 230 `snprintf`
  specifiers). **Out of scope permanently, not deferred.** Device float formatting is
  disproportionate risk, and matching glibc's rounding would make the differential
  unstable.
- `vsnprintf` — 0 of its 4 sites has a literal format.
- flags `+` space `#`, `*` width, and width or precision on a *signed* conversion — the
  sign/pad interleaving rule (`%05d` of -42 is `-0042`, `%5d` is `  -42`) is real work for
  4 remaining corpus sites: `%02d` 3, `%-10d` 2, `%2d` 1. `-` and precision *are*
  implemented, but only on `%s`, where they are a pad count and a length cap rather than
  an interleaving.

**A `%s` copies at most `MCC_FMT_MAXSTR - 1` = 27 bytes, and that is a budget decision.**
Every byte of a string costs a load, a definedness-free range gate and an unconditional
read-modify-write store: ~229 SPIR-V words, measured. `MCC_GPU_CODE_MAX` is 16,384 words
on the SPIR-V arm and a wide conversion still spends 4,700 (`%llx`) to 6,900 (`%lld`) of
it, so the cap is what decides how many *sites* fit, not how long a string is worth
copying. Re-measured 2026-08-09 with the oracle and the narrow conversion path in place
(the old 20 → 142 / 24 → 140 / 28 → 140 / 30 → 133 / 32 → 130 row was taken from the
drifted port and against the old cost model):

| `MCC_FMT_MAXSTR` | 16 | 20 | 24 | 26 | **28** | 30 | 32 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| sites accepted | 151 | 151 | 149 | 148 | **148** | 143 | 141 |

28 is still the knee and the cliff is still just above it — 30 costs five sites and 32
costs seven — but the shape below it changed: 20 now buys **three** sites (both
`%s%s-%s` and `%s/kgc-%016llx-%u-%lu.z`) for eight fewer characters, where before it
bought two. Left at 28 anyway: a shorter cap truncates real strings, which is the kind of
approximation this row refuses everywhere else.

**Unterminated strings, and pointers that are not in binding 2, both have a defined
answer and the two executors reach it identically.** The scan is a fixed 28 iterations
with a monotone `alive` flag, never a loop to a NUL. A byte read past the end of binding 2
reads as 0, so it terminates the string; a run of non-NUL bytes longer than 27 is
truncated at 27 and the *returned length is the truncated length*, which is the one place
this deliberately differs from `snprintf` — an argument longer than the cap does not
report its true length. A pointer whose 64-bit distance from the binding-2 base does not
fit in 32 bits (`NULL`, any host address outside the mapping) is forced out of range and
formats as the empty string; it does not poison, fault or read host memory. `suite_fmt`
carries all of those cases: empty, exactly-at-the-cap, an eight-byte run at the very top
of the buffer with no NUL after it, `NULL`, and an out-of-mapping host address.

**The return-value claim was transferred from the wrong function.** `printf` discards its
return at 80/80 sites; **`snprintf` does not** — the byte count is consumed at 26 of 162
literal-format sites (16.0%): `p += snprintf(...)` accumulations and
`if (snprintf(...) >= size)` truncation checks. Return plumbing is not free and is
implemented: `spv_fmt_emit` returns the untruncated length, snprintf semantics, and
`suite_fmt` diffs it at four buffer sizes including 0 and 1.

**+168 does not reproduce.** No script in the tree derived it and the derivation was prose
only. Re-measured over `src/*.c` at `-O1` with `tools/fmt-census.py --arenas=`, 7,314
arenas, 50,045 non-empty `AST_BasicBlock`s:

| | count |
| --- | ---: |
| Invoke-blocked blocks | **25,700** |
| unblocked by `snprintf` alone | **246** (0.96%) |
| unblocked by the `snprintf`/`vsnprintf`/`sprintf` family | **261** |
| rank of `snprintf` among single-callee unblocks | **8th**, behind `_mcc_error` 1254, `fprintf` 554, `memcpy` 366, `_mcc_warning` 327, `ast_next_sib` 283, `mcc_pedantic` 264, `expect` 249 |

The board's 16,537/+168 and this 25,700/+246 are the same measurement over a tree three
days apart and 18 sources rather than 15; the *share* barely moves (1.02% → 0.96%), so the
ranking survives, but **quote the reproducible number, not the prose one**. Every count in
this table is on the contaminated loop corpus and is inflated ~2.86×; the current figures
are `10,423` / `86` (0.825%) — see the `--arenas=` re-take. Scaling by the
site share now enabled gives roughly **213 of the 246 blocks**; that is an estimate,
because the format string bytes live in rodata and never enter the arena, so the dump
cannot classify a block by its format.

**"The next lever is the literal run" was wrong, and the measurement is what says so.**
The filed claim was that a literal byte costs 152 words, that seven of the seventeen
budget refusals carry no `%s` at all, and that those are therefore "pure literal text"
waiting on a packing scheme. Checked against the compiler: it is **five of fifteen** that
carry no `%s`, and **none of the five is pure literal** — every one of them carries
integer conversions, and two of them are over budget on those conversions alone. Splitting
each of the 15 budget refusals into its fixed (conversion) cost and its literal-byte count:

| | |
| --- | ---: |
| refusals whose conversions alone exceed 16,384 words, with zero literal bytes | **11 of 15** |
| rescued by packing literals at 100 words/byte (a 1.5× win) | 2 |
| rescued at 76 words/byte (2×) | 4 |
| rescued at **0** words/byte — literals made entirely free | **4** |

So a literal-packing scheme had a ceiling of 4 sites no matter how good it is, and to
reach that ceiling it had to get a literal byte under 84 words. **Not implemented**: the
binding constraint is the conversion, not the run. Three of those four were then taken by
the narrow conversion path below, for free, so the lever's remaining reach is **one
site**.

**What was implemented is the narrow conversion path, and it is worth 6 sites.** The cost
that dominates is `MCC_FMT_C_DEC` = 6,900 words for *one* decimal conversion — twenty
`spv_udiv64` software divisions of a 64-bit value. Three decimal conversions are 20,700
words and exceed the budget before anything else in the format is emitted. But `%d`, `%u`
and `%x` with no length modifier take an `int`/`unsigned` through varargs, `it->wide` is
already 0 for them, and `fmt_fit` already narrows the argument, so the 64-bit machinery
was being spent on a 32-bit value. `mcc_fmt_int` and `spv_fmt_int` now take a 32-bit path
when `!it->wide`: ten native `OpUDiv` steps instead of twenty software ones for decimal,
eight `OpShiftRightLogical` instead of sixteen for hex. Measured emitted words, from
`slicerun fmt --fmt-cost-report`:

| format | before | after |
| --- | ---: | ---: |
| `%d` | 7,644 | **3,056** |
| `%u` | 7,400 | **2,899** |
| `%x` | 5,444 | **2,447** |
| `%lld` / `%llu` / `%llx` | unchanged | 7,644 / 7,400 / 5,444 |

`MCC_FMT_C_DEC32` = 2,400 and `MCC_FMT_C_HEX32` = 1,750 are the model's bounds over those,
and `fmt_case`'s `code.n <= p.cost` assertion holds on every format. The six sites this
turns from refused to accepted are `root width %u != src %u` and
`reflect size %zu != src %u` (`mcccst.c`), `%smcc-me-%u-%u.c` and `%smcc-tmp-%u-%u.tmp`
(`mcchost.c`), `\tfirst=%d\tend=%d\tblen=%d\tnlen=%d` (`mccrir.c`) and the 75-literal-byte
JIT source template in `mccjit_embed.c`. **All six are in `suite_fmt`'s differential
list**, so the win is a device kernel that agrees with the CPU reference byte for byte
over real work, not a census number: 1,984 lanes and 253,952 destination bytes compared,
and `--mutate` is red (32 failing checks across the 31 formats, against 0 clean).

`FMT_NSLOT` went from 4 to `MCC_FMT_MAXARG + 1`, because a four-argument format could not
previously be dispatched at all — `fmt_kernel` refused `p->narg > 3` and the suite had
never contained one.

**The nine that are left, by how far over 16,384 words each one is.** The narrow path does
not help a conversion that really is 64 bits, and nothing shrinks `%s`.

| format | cost | over by | why it stays refused |
| --- | ---: | ---: | --- |
| `arity %s n=%u nc=%u op=%d` (`mccrir.c`) | 17,146 | **762** | the closest miss on the board, 4.6%. One `%s` at 6,542 plus four narrow decimals |
| `'%s' has internal linkage but is referenced in an inline function with external linkage` (`mccgen.c`) | 20,282 | 3,898 | 85 literal bytes at 152 = 12,920 words. **The only remaining site literal packing would reach**, and it needs only 106 words/byte to do it |
| `%s%s-%s` ×2 (`libmcc.c`) | 20,598 | 4,214 | 3 × 6,542 = 19,626 words of `%s` alone. Only a smaller `MCC_FMT_MAXSTR` fits it, and that truncates strings |
| `%lu.%lu.%lu` (`mcchost.c`) | 21,824 | 5,440 | three *wide* decimals, 20,700 words. `%lu` is `unsigned long`; narrowing it would be wrong on LP64 |
| `%s/kgc-%016llx-%u-%lu.z` (`mccjit_embed.c`) | 25,162 | 8,778 | `%s` + `%016llx` + `%lu`, three expensive items |
| `string literal of length %ld … ISO C%s …` (`mccgen.c`) | 30,950 | 14,566 | 94 literal bytes is 14,288 words — even free literals leave it 278 over |
| `equiv rung=%s n=%d exact=%d inferred=%lu points=%lu` (`mccast.c`) | 31,890 | 15,506 | `%s` + two wide + two narrow decimals |
| `differ rung=%s smallest-width=%d n=%d a=%lld b=%s%lld` (`mccast.c`) | 38,128 | 21,744 | two `%s` + two wide + two narrow decimals |

**Where this row stops, and the cost that says so.** Six sites landed for one contained
change confined to `mcc_fmt_int`, `spv_fmt_int` and two cost constants. The *seventh*
costs a literal-run packing scheme — runtime destination alignment, per-byte room masks
folded into a word mask, and a matching rewrite of the CPU reference — and buys **exactly
one site**. The eighth would need `MCC_GPU_CODE_MAX` raised, which is a cross-cutting
change to the whole GPU arm: `docs/PLAN.md` N2 already records that Vulkan sets no
module-size bound, that raising it is a prerequisite for the interpreter, and that the MSL
arm already uses 65,536 — so it is a real option, just not one this row should spend.

**The payoff on all of it is still device-*eligibility*, not device execution.**
`mcc_slice_frame_from_ast` has no caller in `src/`, `mcc_fmt_compile` has no caller
outside `tools/slicerun.c`, and the parallel-legal iteration fraction on a self-compile is
0.01%. Stop here. ~~The next thing that makes this row worth more is row 1's `a[i][j]` /
`b[i][j]` aliasing representation — 79.35% of numeric-corpus iterations are refused only
for that~~ — **that landed on `wt/decaytype` and it did not make this row worth more.** The
79.35 points converted, the corpus reads 80.60%, and 79.21 of those points are `double`
arithmetic the device layer has no opcode for. **Nothing makes this row worth more short of
floating point in the emitter**, and certainly not the fourteen remaining format strings.

**What is left, and it is not the `%` engine.** The formatter is verified as a region
primitive; wiring it to an `AST_Invoke` in statement position needs three things that do
not exist:

1. `ast_slc_callee_sym` and `ast_slc_invclass` are `static` in `src/mccast.c` and depend on
   `Sym` and `ast_inline_pool`, neither visible from `src/mccslice.h` (which `slicerun`
   compiles without a symbol table). They need a hook in the manner of
   `ast_eval_slice_obj_fn`, not a second callee resolver and not a duplicate.
2. The destination. `snprintf`'s *first* argument is a pointer even when the format has no
   `%s`, so a local `char buf[N]` destination is either item 1 again or a byte-addressed
   frame — and the frame is dense 8-byte slots capped at `MCC_SLICE_MAXSLOT` = 16, so a
   64-byte buffer does not fit it at all. `suite_fmt` sidesteps this by addressing a
   lane-private slice of binding 2 directly, which is what a real emitter would also have
   to do. The `%s` *source* does not have this problem: it is read straight out of
   binding 2 at whatever offset the pointer maps to, by the same subtract-and-test gate
   `spv_mem_off` uses for `*p`.
3. The host drain. No ring header, producer index or consumer loop exists; the place for
   one is beside the `mcc_gpu_rw_back` copy-back inside `mcc_gpu_dispatch_locked`, under
   the GPU lock, after the fence. `docs/PLAN.md` J3a' requires a side-effect watermark
   before any stdout post, so tranche 1 writes to binding 2 and the host reads it; nothing
   is emitted to stdout from the device path.

No MSL twin was written and none is needed: binding 2 does not exist in the Metal backend
(`mcc_gpu_mem_backend` returns 0 there and `mcc_gpu_rw_supported()` is 0), so
`src/mccfmt.h`'s device half is compiled only on the SPIR-V arm and the Metal path
declines the whole feature rather than diverging from it.

One emitter property worth keeping: every byte store in `spv_fmt_putb` is unconditional.
A byte the run must not write becomes a read-modify-write with a zero keep-mask at a
clamped offset, which rewrites the word it just read. A branch instead would put
`spv_region_addr`'s definedness update inside a conditional block and force an `OpPhi` per
byte. The `%s` arm keeps that property despite having a data-dependent length: the copy is
a fixed 28-iteration unrolled scan, the "this byte is part of the string" predicate is a
monotone `alive` chain of `OpLogicalAnd`, and it reaches the store as the write-enable of
an otherwise unconditional `spv_fmt_putb`. `spv_fmt_getb` is the read twin and is
deliberately *not* `spv_load_region`: it clamps rather than poisoning, because a bounded
scan reads past the string on purpose and must not report the module undefined for it.
The destination must still be a region only one lane writes — `spv_fmt_putb` fails the
module for a `shared` region, by the same test `spv_store_region` uses.

**The cost model is an over-estimate, and a cell enforces it — and now prints it.**
`mcc_fmt_cost` predicts the emitted word count from the item list; `mcc_fmt_compile`
refuses with `MCC_FMT_R_ROOM` when the prediction exceeds `MCC_FMT_MAXCOST`. `fmt_case`
asserts `code.n <= p.cost` on every format it builds, so if the emitter is ever made
cheaper or dearer the cell goes red rather than the module silently overflowing. The
`slice/fmt` cell now runs with `--fmt-cost-report`, which prints predicted against emitted
for every format, so the constants can be recalibrated from a ctest log instead of by
patching the runner: that is exactly how `MCC_FMT_C_DEC32` and `MCC_FMT_C_HEX32` were set.

The margin is tighter on the wide arm than the narrow one, which is where the model
matters: `%lld` 7,644 emitted against 7,720 predicted (1.0%), `%016llx` 7,736 against
7,952 (2.7%), `%s` 7,264 against 7,362 (1.3%), `%s:%s` 13,784 against 14,056 (1.9%). The
narrow constants were rounded up rather than fitted, so `%d` is 3,056 against 3,220 (5.1%)
and `%u/%x` is 4,678 against 5,122 (8.7%). A literal byte is priced at 152 and measures
about 136, from `[%d]` − `%d` = 272 words for two bytes and `n=%d.` − `%d` = 403 for three.

The mutation operator for this cell perturbs a **destination byte**, not the return value.
Verified on an RTX 5070 Ti: `slice/fmt` is green over 1,984 lanes and 253,952 compared
destination bytes across 31 formats, `--mutate` is red with 32 failing checks, and a one-bit
error in the string copy at source byte 5 is caught in destination *words 1 and 2* — not
only at byte 0 — including in the unterminated-run lane (`5a5a5a5a` → `5a5a7a5a`).

### Unranked — the fence wait, and there is nothing to wait for

**Read row 1's `par=` section before ranking this.** *(Retaken 2026-08-09: the fraction below
is **0.01%**, not 1.88%, and the twelve array fills are not the shape of the answer. The
conclusion this row draws from it is unchanged and, if anything, harder.)* The
parallel-legal iteration-weighted
fraction on a self-compile is ~~**1.88% once the one `memset`-shaped loop is removed**~~, and
the entire lane source is twelve array-fill loops. Per-lane cost is not the binding
constraint on this workload; the absence of lanes is. Everything below is still true and
still the right fix *if* a caller ever appears — it is no longer a ranked item.

Host marshalling is now 5.73 ns/lane debug, 3.03 release, out of a 33.7 ns/lane total —
it is no longer where the time is. `vkWaitForFences` is, and it is memory-type sensitive:
**all-VRAM 1.0–20.2 ns/lane against all-sysmem 16–110**. `DEVICE_LOCAL` buffers plus a
staging buffer and `vkCmdCopyBuffer` would get both halves. **Not measured, no payoff
estimated** — no staging path exists and it collides with `mcc_gpu_rw_back`'s read-back.

**PROSE-ONLY.** No committed script derives 1.0–20.2 or 16–110. `slicerun --cost-synth`
measures per-lane cost but not memory-type sensitivity, and nothing in the tree sweeps
`vkWaitForFences` against the allocation type. Treat both figures as unreproduced.

### Retired — ~~`*p` and pointer `++`/`--`~~, LANDED 2026-08-08

Row retired. The pointer-value question was answered **(c) a pointer stays a host
address, and lowering is gated dynamically**: binding 2 is permanently mapped and
host-coherent, so an address inside it already has a stable byte offset, and the map is
one 64-bit subtract plus a test that the high half came out zero. Both executors read and
write the *same physical bytes*. Write-up and residual hazards: "Landed — `*p`, pointer
`++`/`--`" below. What is still refused, and why, is in that section's table.

### What is still open, with honest sizes

Four rows were added 2026-08-09 by the re-derivation at the head of this file; they are at
the top because three of them are the only rows here in a currency that converts.

| row | size, and the tool that produced it | currency |
| --- | --- | --- |
| `ast_loop_interchange_legal` / `ast_dep_fusion_pair_illegal` call `ast_dep_base_distinct` with **no `indirect` guard** | **UNMEASURED, and it is the top of the board.** Verified 2026-08-09: the guarded call is `src/mccast.c:13949` (census only), the unguarded ones are `:13516` and `:13566`. `-floop-interchange`, `-floop-fusion` and `-floop-block` are `MCC_OPTD_LEVEL(12)` (`src/mccopt.h:109-111`), bound at `src/mccast.c:2384-2386`, run from `ast_func_end` at `:18586-18590` into a *mutating* apply. `ast_tile_run` reuses the interchange predicate. The measurement is a fuzz corpus of `p[i]`/`q[i]` nests at `-O12` | **correctness** |
| ~~`tests/optfire/levelbench.tsv` is stale by a generation and has no `--check`~~ | **CLOSED 2026-08-09 (`wt/ladder2` then `wt/gatefin`).** `wt/ladder2` re-measured it: the banked table is a fresh 16-row run matching `src/mccopt.h`'s 16 `LEVEL(1..3)` rows, with `gain_movers_pct`/`eff_movers` beside the diluted columns and a signed efficiency (`inline-functions`, `gain_movers` −1.96, fell from rank 4 to rank 9). `wt/gatefin` closed the fourth defect **in both its halves**: `--check` exists with `optbench/levelbench-bank` + a known-positive, and the `optlevel-bench` cell now compares its build-dir TSV back to `tests/optfire/`. See the audit section | **census trust** |
| the ~17× dilution of `gain_pct` (filed, not fixed, on `wt/benchtrap`) | **CLOSED 2026-08-09 by re-running the bench.** Measured dilution up to **17.4×** (`builtin-math-prepass` 0.3007 all-kernel vs **5.2372** over its one mover). `trunc32` moves 17/17 and its two gain columns agree to the digit, which is the control. It also **flipped a bucket** — `builtin-copysign`'s real **1.0076%** win read 0.0590% and was filed `cost-no-gain`, the bucket asserting no gain was found | **census trust** |
| `narrow` / `tree-copy-prop` "ranked on nothing" | **THE PREMISE WAS FALSE, and the measurement was taken anyway.** Both were already priced on the self-host axis in `levelpins.txt:196,227`; the `n/a` in `levelbench.tsv` was a **stale row for a flag that table no longer sweeps** (levels 10 and 11 against a `<= 3` filter). Re-taken n=25 paired: `narrow` +0.876% stage-1 for −0.0088% stage-2 (**100:1 against**, 25/25 reps), `tree-copy-prop` +0.799% for −0.0048% (**166:1**, 24/25). `rir-coverage` clear for both. **Levels unchanged** | emitted code |
| float in the slice engine and the SPIR-V emitter | **UNMEASURED and unpriced, and it is the measurement that would overturn the verdict.** `grep -c Float src/mccgpu.{h,c} src/mccslice.h` = 0/0/0; `is_float` is refused at 5 sites in `mccslice.h` and 6 in `mccgpu.h`. It costs `OpTypeFloat`, the F-opcodes, conversions, a bit-exact CPU reference and a differential that is stable under rounding | device-executable lanes |
| the device dispatcher | **Not merely absent — unwritable from what exists.** The compiler's two slice sites pass `nlive = 0` / one tuple; `mcc_slice_work_from_ast` refuses `cnt < 1`. Plus a write-back (`MCC_SLICE_MAXSLOT` = 16 dense 8-byte slots; a 600×600 tile does not fit one) and a per-compile correctness gate. Three subsystems, priced nowhere | device-eligible blocks |
| chain-store re-promotion (row 2) | **MEASURED 2026-08-09, and the answer is no.** The `kept` prize is **+2.60** points, not ~4.3 (`tools/rir-coverage.py`, `self`, `-O1`/`-O2`/`-O3`); the emitted-code half is stage-2 **−0.079%** and sieve **−1.97%** `instructions:u`, for **+1.50%** of stage-1. Pins unchanged | emitted code |
| `-O11` ICE, `vstack leak (1)` | **FIXED 2026-08-08**; it also silently miscompiled (6 wrong answers / 500 at `-O11`). Cell `exec-chainlive/*`, fuzz 900 programs 0/0 | correctness |
| `vstack leak (-1)`, debt #6a | **FIXED 2026-08-09**. It fired at shipped `-O1`/`-O2`/`-O3` (78/77/77 of 500) and never miscompiled — every imbalance is `(-1)`, so it always aborted. Cells `exec-storevalrot{1,2,3}/*`, fuzz 1400 programs 0 ICE / 0 wrong. Stage-1 `.text` byte-identical, so it cost nothing | availability |
| `storeval-rot` pays negative at `-O3` | **CLOSED 2026-08-09, level unchanged at 1.** The 1.69% is cold inline-clone bytes: with `-fno-inline` the same gap is **+124**, and the flag's *dynamic* cost is 0.232% with the inliner and 0.245% without it. Turning it off takes `rir-coverage` **red by 8.7 points** of `kept`, because its off-state is an incomplete replay path, not a lowering | emitted code |
| replay recompute reads a written target | **FIXED 2026-08-09 with #6a**: `c = 2 * (a = s + a)` returned the wrong answer under the shipped `-fno-replay-fallback` at `3ddd9933` with every `storeval-*` and `chain-store` flag off. Covered by `flagsweep-exec/replay-fallback` | correctness |
| `rir_op_effect`'s clear (row 4) | **CLOSED 2026-08-08**, and the "<1%" bound was wrong: measured **−8.30% / −9.44%** of stage-1 `-O3`/`-O2` CPU time (n=21 interleaved, ±0.55% floor), `instructions:u` −12.29%. 1,464 objects byte-identical | compile time |
| Metal, debt #4 (row 3) | **DECIDED 2026-08-09: dropped.** Re-counted: 1754 vs **3612** lines (the 3578 was stale by `99e043c1`, and the MSL total is unchanged because nothing is added to it), **2**-line kernel arm, 0 `msl_region*` symbols against **31** `spv_*`. A rewrite, not a fix | a decision |
| D4b leaf-inline pool cap (row 5) | ceiling **803** blocks, `slicerun --census`. **Cap removed 2026-08-08**: reach 41 → 72 grafts, and it delivered **one** more block (10,381 → 10,375 `inv-blocks`). Row closed, not advanced | device-eligible blocks |
| `snprintf` module budget (row 6) | **14 of 162** refused, was 22. The narrow 32-bit conversion path landed 6 sites 2026-08-09 (`142 → 148`, and `140 → 142` was a census bug, not work). 9 on the budget, 4 on flags, 1 on float. The closest miss is 762 words | device-accepted sites |
| the literal-run packing lever (row 6) | **MEASURED AND DROPPED 2026-08-09.** The filed "7 pure-literal refusals" do not exist — 5 of 15 carry no `%s` and every one of them carries integer conversions. 11 of 15 were over budget with *zero* literal bytes, so packing had a **4-site ceiling even if a literal byte were free**; the narrow conversion path then took 3 of those 4, leaving the lever with **one site** and a delicate emitter rewrite. Not implemented, deliberately | device-accepted sites |
| `tools/fmt-census.py` was an ungated second implementation | **CLOSED 2026-08-09.** It had already drifted: one item per literal byte instead of merged runs, 61 disagreements over 984 corpus formats, 2 over the 162 `snprintf` sites. Now gated by `fmt/census-oracle` (41,017 formats vs the real `mcc_fmt_compile`) and `fmt/census-oracle-known-positive` | census trust |
| the fence wait (unranked) | all-VRAM 1.0–20.2 ns/lane against all-sysmem 16–110 — **PROSE-ONLY**; no staging path exists and no payoff was estimated | device time, no subject |
| debt #1, `--mutate` blind to `memcpy` | smaller than filed: four of six operator sites already perturb written memory and `g_frame_mismatch` already exists. The real gap is that **no `memcpy`/`memset` exists in the slice corpus to mutate** | test strength |
| indirect callees | 115 blocks, and no device answer anywhere in `docs/PLAN.md` | device-eligible blocks |
| recursion on device | **no data at all** beyond the ~130-function SCC and the dynamic depth figures (`unary()` peaks at 11, `ast_replay_bb` at 35, 1.75–3.5 MiB/lane) | unknown |
| debt #3, descriptor staleness | fixed, and **unreachable until binding 2 grows** — both callers pass the constant `MCC_VK_MEM_DEFAULT`, so no cell is possible yet | latent |
| `pe` lowerable floors | stale-low; could not be re-banked from an ELF host, so they under-gate on Windows rather than false-fail | gate strength |

### ~~The recommendation on direction~~ — SUPERSEDED 2026-08-09

> **Superseded by `#### The recommendation` and `#### The counter-argument` at the head of
> this file.** The conclusion is unchanged — freeze, no caller, drop Metal — but **every
> ground stated below has since been retaken and two of the three are wrong.** The 1.88% is
> 0.01% (`415b736c` fixed the `memset`-shaped loop the correction was hand-derived around),
> and "no lane source" is false: the numeric corpus has 80.60%. What replaces those grounds
> is that 79.21 of those 80.66 points are `double` arithmetic that neither executor
> implements, and that the compiler's slice sites and the batching machinery have disjoint
> input domains. The counter-argument below is kept because points 3 and 4 still hold and
> are argued at greater length in the new section; points 1 and 2 have been **answered** —
> the `par=?` bound was taken (83.86% of self-compile iterations are calls and `goto`s) and
> the second corpus was run.

**Stop investing in the device path.** Concretely: freeze SPIR-V emitter feature work at
what is banked, do **not** write the missing `mcc_slice_frame_from_ast` caller, drop Metal
(row 3), and keep the existing device cells green purely as regression cover for a lowering
stack that is finished and proven. Spend the next weeks on rows 2 and 4 and on the `-O11`
ICE, all of which are denominated in currencies this workload actually pays in.

The grounds were three measurements, not a preference. Two did not survive:

- ~~`tools/loop-census.py`: **1.88%** parallel-legal iteration-weighted fraction once one
  `memset`-shaped loop and its twin are removed, spread over twelve array fills, nine of
  them under 70,000 iterations in a whole self-compile.~~ **STALE — it is 0.01%, and the
  twelve array fills are not the shape of the answer.**
- `tools/slice-census.py --corpus self` ("slices containing a loop"): only **4.3%** of
  census slices contain a loop at all, so even a workload *with* lanes meets a thin
  eligibility surface here. **Re-taken 2026-08-09 and it holds: 552 of 12,957 slices,
  4.26%.** This is a `self` figure — `wide` enumerates 81,615 `t=0` slices, so the two
  must never be compared.
- Inspection: **0** call sites for `mcc_slice_frame_from_ast` in `src/`, and no device
  consultation of any kind during an ordinary `mcc -O2 -c`. **Re-verified 2026-08-09: the
  definition is `src/mccslice.h:521` and all 16 call sites are in `tools/slicerun.c`.**

**The counter-argument, stated fairly, because it is not weak:**

1. **`par=?` is 32.93% of iterations and 570 of the 597 entered loops.** The predicate
   *declines* on the overwhelming majority of what it examined, and `?` is not "not
   parallel" — it is "`ast_loop_parallel_legal` gave up". The verdict therefore rests on the
   twelve loops it could prove, not on 597 it ruled out. **How much of the `?` bucket a
   stronger predicate could convert is UNMEASURED, and no upper bound has been taken.** The
   refusal list is in row 1 and is long: a call anywhere in the body, `asm`, a
   `return`/`goto`/label, `AST_StoreVal`, >64 distinct scalars, a conservative direction
   vector, and any memory read in the exit test. If someone wants to overturn this
   recommendation, that bound is the measurement to take, and it is a real one.
2. **The corpus is mcc compiling mcc.** mcc's job is compiling other people's C, and a
   compiler is close to a worst case for lane structure: pointer chasing, early exits,
   data-dependent trip counts. Nobody has run `-floop-census` over a numeric or
   image-processing workload. Against this: `docs/DEVICE-LIBC.md` already rules `tests/exec`
   unrepresentative in the other direction, and no third corpus has been proposed, so "some
   other program might have lanes" is at present a hope with no census behind it. It would
   be cheap to take — `-floop-census` works on any TU — and that is the second measurement
   that could overturn this.
3. **Freezing is not deleting.** The predicate, both executors, the emitter, the format
   engine and the leaf inliner are tested and differentially verified, and the cells that
   guard them are cheap to keep. If a subject ever appears, the stack is there. The
   recommendation is to stop *adding*, not to remove.
4. **The device work has been a productive way of finding compiler bugs.** Building `*p`
   found and fixed two pre-existing defects — the loop merge discarding the exiting
   iteration's definedness, and `--mutate` blind to `++`/`--`. `ast_loop_parallel_legal` is
   a dependence analysis the host optimizer can use whether or not a GPU ever runs, and it
   already found that the shared decoder treated `p[i]` and `q[i]` as distinct objects.
   Whatever is decided about dispatch, that yield is real and is not an argument for
   dispatch.

### Deprioritised, each with the measurement that says so

- **`AST_StoreVal`** — 971 nodes (3.0%) but it is a vstack-ordering marker referencing its
  `AST_Store` by `ival`, so it is subsumed by whatever handles that store. ~0 standalone.
- **`AST_If` op 8 / op 9 / `.field`-`&` store destinations** — all three measured at **+0**
  against the real predicate, not a model. Zero-payoff results 6, 7 and 8.
- **`switch` (op 6)** — 7 nodes in 8 already-blocked blocks, a 0.84% ceiling.
- **N14 per-lane globals** — re-scoped: it applies to running mcc itself on the device
  (cluster B), and the emitter path carries no C globals. Not on the critical path.
- **The allocator** — +50 blocks, 0.30%. "The worst value-per-risk item in the census."

### Debts that will corrupt the next measurement if left

Five of the eight landed on `wt/debts` and `wt/ptrval`, 2026-08-08. Two of the diagnoses
recorded below were **wrong**, and the corrections are worth more than the fixes: #7 was
not about `cmake-cross`, and #1 is about a third of the size it was written up as. Each
item now says what was measured, not what was assumed.

0. **Nothing dispatches a binding-2 kernel except `tools/slicerun.c`, and as of
   2026-08-08 we know there is nothing for it to dispatch.** The predicate, both executors
   and the emitter are done; there is no *caller* in the compiler, because
   `mcc_slice_frame_from_ast` has **0 call sites in `src/`** — `src/mccslice.h`, which
   defines it, is included by `tools/slicerun.c` and by nothing in `src/`, and
   `tools/slicerun.c` does not link `libmcc`. (Correction, 2026-08-08 evening:
   `src/slice_inline.h` **is** now compiled into `mcc`, via `src/mccast.c` under
   `AST_EVAL_SLICE_PROVIDED`, as of the leaf graft. It carries the inliner, not the
   dispatcher.)
   Everything under "Landed — `*p`" is measured on the harness, so it is a lowering result,
   not a speed-up. **The missing caller is no longer the top of this debt — the missing
   work is.** `ast_loop_parallel_legal` now answers the census's `par=` field, and on a
   self-compile the parallel-legal iteration-weighted fraction is ~~**65.75%, of which 65.2
   points are one 512-byte `memset` inside `rir_op_effect`**; with it removed the figure is
   **1.88%**, spread over twelve array-fill loops, nine of which run under 70,000 iterations
   in the whole compile~~ — **STALE, retaken 2026-08-09: it is 0.01%, the `memset` was fixed
   at `415b736c`, and the numeric corpus reads 80.60% of which only 1.45 points are a type
   either executor can run.** Writing the caller would give it nothing to call — and, added
   2026-08-09, **it cannot be written by connecting what exists**: the compiler's two
   slice-evaluation sites pass `nlive = 0` and one tuple, and `mcc_slice_work_from_ast`
   refuses `cnt < 1`. The two halves have disjoint input domains. See row 1 and the verdict.
1. **`--mutate` is blind to `memcpy` — OVERSTATED, and much smaller than written.**
   The write-up said the operator must move to written memory and the harness needs a
   frame-buffer comparison mode. Both already exist: four of the six operator sites
   already perturb written memory (`mccslice.h` two sites, `slicerun.c` two more), and
   `slicerun.c` has a frame-buffer comparison mode (`g_frame_mismatch`). **The real gap
   is that there is no `memcpy`/`memset` in the slice corpus to mutate yet** — the
   operator has nothing to bite on, which is a corpus problem, not a harness rewrite.
   Still open, still owned elsewhere; the "larger than the emitter work it guards"
   claim is retracted.
2. ~~**`ast_eval_slice()` sets `ast_eval_slice_undef` but neither resets nor returns it.**~~
   **FIXED 2026-08-08**, in the same branch as row 1 — which is precisely what made it
   reachable, since relaxing `kind_ok` to admit `*p` is the change the old write-up warned
   about. `ast_eval_slice` now clears the flag at entry and returns `d && !undef`. The two
   `spvgate` sites that did it by hand are now redundant and harmless.
3. **`mcc_vk_bind_mem` descriptor staleness — FIXED.** `mcc_vkr` grew an `int dsdirty`,
   `mcc_vk_bind_mem` sets it after the recreate, and `mcc_vk_bind_buffers` folds it into
   `grew` (and clears it) before the `if (!grew) return 1;` early-out. That covers the
   `mcc_gpu_mem_backend` path too, since dispatch calls `bind_buffers` every time.
   **Not test-covered, and cannot be**: both callers still pass the constant
   `MCC_VK_MEM_DEFAULT`, so binding 2 never grows and the write is unreachable. The
   first caller that grows it makes the fix live and makes a cell possible; write the
   cell then.
4. **The Metal arm diverges further with every landing — quantified, and it is a
   multi-week rewrite, not a debt to pay down. DECIDED 2026-08-09: dropped as a device
   target; see board row 3 and `#### Metal — settled` at the head of this file.** Measured
   over the `#if MCC_GPU_LANG_MSL` / `#else` arms — the four SPIR-V figures below are
   **STALE**, re-counted 2026-08-09 as **1754 MSL against 3612 SPIR-V**, `mccgpu.h`
   1008/**1794**, `mccslice.h` 27/**65**, and **31** distinct `spv_*` symbols; the MSL
   totals did not move at all, which is the divergence measuring itself:
   ~~**1754 MSL lines against 3578 SPIR-V**, split
   `mccgpu.c` 653/1461, `mccgpu.h` 1008/1763, `mccslice.h` 27/62, `spvgate.c` 66/292.~~
   `mcc_slice_frame_kernel_build`'s Metal arm is ~~three~~ **two** lines returning 0
   (`src/mccslice.h:1282-1285`). There are
   **no `msl_region*` symbols at all**, against ~~25~~ **31** distinct `spv_*` symbols
   reached from
   `mccslice.h` alone. `tools/slicerun.c` carries one `#if` (`AST_EVAL_SLICE_PROVIDED`)
   and none for the backend, so it cannot compile against the Metal arm. Nothing here is
   a fix; treat it as a decision about whether Metal stays a target.
5. **`MCC_GPU_REQUIRED` absent from CI — FIXED.** It was defined in `CMakeLists.txt`
   (default `OFF`) and turned on in exactly one place, `tools/ci.c`'s `gpu-vulkan`
   feature, gated `OS_MAC`. Every Linux and Windows ctest job ran with it `OFF`. Now
   `ci.yml`'s `stage2-linux` installs `mesa-vulkan-drivers` (lavapipe — `libvulkan-dev`
   alone is loader plus headers with **no ICD**) and passes `-DMCC_GPU_REQUIRED=ON`.
   `matrix.yml` had two live defects fixed at the same time: its `stage2` job ran the
   macOS `gpu-vulkan` cell — which sets `MCC_GPU_REQUIRED=ON` itself — with **no
   `brew install molten-vk`**, so every device cell hit `FATAL_ERROR` nightly; and it
   never installed `libvulkan-dev`, so its own `[ -d /usr/include/vulkan ]` guard
   silently dropped `-DVulkan_INCLUDE_DIR` and three `gpu/spv-slice-*` cells with it.
   Windows is deliberately **not** armed: vcpkg `vulkan-headers` is headers only, no ICD.
   Note for anyone tempted to lean on `must-run.py --results`: it cannot catch any of
   this. All four `gpu/*` rows and every device-bearing `slice/*` row in
   `tests/must-run.txt` are `registered`, and `--results` only checks `must-run` rows.
6. **The lowerable ratchet was measuring noise — FIXED at the root, not by widening
   `--tol`.** The arithmetic was: floor 25.9207, `--tol` 0.05, so threshold 25.8707;
   baseline 25.8724 (0.0017 inside); merged 25.8706 (0.0001 outside). The denominator
   rose 434,204 → 434,401 nodes while absolute lowerable nodes rose 112,339 → 112,385.
   **It fired on dilution while real coverage improved.** Cause: the census subject
   `src/mcc.c` amalgamates `libmcc.c`, which `#include`s the device layer, so the
   ratchet that guards the SPIR-V emitter had the SPIR-V emitter in its own denominator.
   Fixed by splitting the subject: `src/mccrir.c` gained `MCC_RIR_LOW_EXCLUDE`, a
   comma-separated suffix list whose bodies are dropped from the **lowerable** numerator
   and denominator both (`rir_low_excluded`, applied in `rir_low_take`), and
   `tools/rir-coverage.py` sets it to `src/mccgpu.c,src/mccgpu.h` and records it in the
   bank's `corpus_config`, so a run without the exclusion skips instead of reporting the
   difference as a regression. Coverage and byte accounting are untouched — they are not
   self-referential. The device layer is 122 of 2812 bodies and ~17,000 of ~434,000
   nodes. Re-banked `self`/elf floors (`--update-bank-low --rebank-config`):

   | | O0 | O1 | O2 | O3 |
   | --- | ---: | ---: | ---: | ---: |
   | `nodes_pct_strict` | 26.1790 | 26.1576 | 26.1576 | 26.1576 |
   | `nodes_pct` | 41.8190 | 41.8112 | 41.8112 | 41.8112 |
   | `nodes_pct_loose` | 66.2243 | 66.2367 | 66.2367 | 66.2367 |
   | `bodies_pct` | 9.1856 | 9.2159 | 9.2159 | 9.2159 |
   | `region_nodes_pct` | 16.9886 | 17.0023 | 17.0023 | 17.0023 |
   | denominator, nodes | 417,392 | 417,056 | 417,056 | 417,056 |

   Re-banked again 2026-08-08, and for the same reason: the compiler-side leaf graft
   (board item 3) adds bodies to `src/mccast.c` that are not whole-body lowerable, and the
   denominator went 417,392 → 422,275 nodes at O0. `nodes_pct` and `nodes_pct_loose` rose
   (41.8190 → 41.9326, 66.2243 → 66.2852) while `nodes_pct_strict` and `bodies_pct` fell,
   and the absolute strict-lowerable node count rose 109,268 → 110,310 — dilution again,
   not regression. Verified by reverting the change and re-running: `rir-coverage` is
   green on the unmodified tree against the old floors.

   | | O0 | O1 | O2 | O3 |
   | --- | ---: | ---: | ---: | ---: |
   | `nodes_pct_strict` | 26.1230 | 26.1019 | 26.1019 | 26.1019 |
   | `nodes_pct` | 41.9326 | 41.9250 | 41.9250 | 41.9250 |
   | `nodes_pct_loose` | 66.2852 | 66.2975 | 66.2975 | 66.2975 |
   | `bodies_pct` | 9.1210 | 9.1510 | 9.1510 | 9.1510 |
   | `region_nodes_pct` | 17.0152 | 17.0288 | 17.0288 | 17.0288 |
   | denominator, nodes | 422,275 | 421,939 | 421,939 | 421,939 |

   `--tol` is left at 0.05 deliberately: the point was that the gate could not tell
   dilution from regression, and that is what changed. Two things stay open. The `pe`
   floors could not be re-banked from an ELF host; they are stale-low, so they
   under-gate on Windows rather than false-fail. And **`rir-coverage-census` (the `wide`
   corpus) was red and was red before this — now GREEN, and the cause was a
   replay-fidelity bug, not a census artefact.** Two of the three failures were
   lowerable dilution (`bodies_pct` 15.1452 < 15.6420, `nodes_pct_strict` 26.9255 <
   27.1327; the exclusion moved both *up*, to 15.4913 / 27.0635). The third was not:
   `kept coverage` failed at **-O1, -O2 and -O3** — the earlier note said "-O2/-O3",
   which was wrong — 92.9416 / 93.0058 / 93.0058 against 98.3968 / 98.3841 / 98.3840
   banked. -O0 alone passed, and that asymmetry was the tell.

   `rir_to_arena()`'s `IR_OP_VSTORE` case collapsed a childless `AST_StoreVal` over its
   `AST_Store` source **unconditionally**, but tagged the resulting `Store` with fbit
   `1u` only when `ast_chainstore_env` was set. Both consumers of that bit — the
   coalescing poison loop in `ast_func_end` and `ast_finalize_chainstores` — are gated
   on the same flag family, so with the flags off the arena kept a collapsed chain that
   nothing re-expanded and the replay's bytes drifted from the parser's. `chain-store`
   went to level 3 at `1ad3f1aa` (killing -O1) and to level 11 at `893c1e84` (killing
   -O2/-O3); -O0 never had the flag, which is why -O0 alone still matched its bank. Not
   a miscompile — modelled coverage stayed 100%, `nofb_miscompiles` is empty, and the
   byte gate restored the parser's bytes — but it cost optimization, because
   `ast_run_strat_seq` gates every strategy on `faithful`.

   Fixed in `src/mccrir.c` by splitting the two collapse branches: reusing the source
   `Store` in the value position (`ast_detach_last_child` succeeds) is a faithful
   re-nesting of a node the parser already had there and stays unconditional; *rewriting*
   `a = b` into `a = <copy of b's value>` (`ast_dup_sub`) is the chain-store optimization
   proper and is now gated on `ast_chainstore_env`. The `1u` tag is set whenever either
   fires, so it records the structural fact and the consumers keep their own gates. The
   `-O` levels in `src/mccopt.h` are untouched — `tests/optfire/{defstate.txt,levelpins.txt,
   leveltime.tsv}` pin `chain-store` at 11 on measured stage-1 compile cost, and this is
   not a licence to re-promote it. Measured effect (`kept`, elf/x86-64):

   | | O0 | O1 | O2 | O3 |
   | --- | ---: | ---: | ---: | ---: |
   | `self` before | 82.939 | 82.998 | 83.122 | 83.122 |
   | `self` after | 82.939 | **91.904** | **91.960** | **91.960** |
   | `wide` before | 92.923 | 92.942 | 93.006 | 93.006 |
   | `wide` after | 92.923 | **96.604** | **96.653** | **96.653** |

   Both banks re-banked at `--update-bank --update-bank-low` (the fix moves the arena's
   node count, so `self`'s `nodes_pct_strict` drops 26.151→26.125 / 26.130→26.104 —
   inside the tolerance it had been sitting on, which is why the `self` cell went red on
   lowerable while `kept` rose 9 points). `wide` needed re-banking regardless: the corpus
   has grown to 380 sources (9 pre-existing negative/arch tests still fail to compile)
   and `corpus_config` guards only `MCC_DIAG`/`MCC_EMBED_JIT`, not test-file count. With
   all three of `-fchain-store -fchain-store-live -fchain-store-member` on, `self` reaches
   96.204 / 96.232 / 96.301 / 96.301, so ~4.3 points of the old gap is still recoverable
   by the two optimization passes and is not a fidelity defect. The cell is still
   `--opt-in` with `LABELS census`, which is how it stayed red unnoticed.

   **Follow-up: the fix took `optfire-{i386,riscv64}/chainstore` red, and the cell was
   right to notice.** Those two cells assert `-fno-chain-store` and `-fchain-store`
   differ at `-O10`; after the fix they were byte-identical. The tempting reading — that
   the tag is now set regardless of the flag, so both configurations behave alike — is
   **wrong, and was checked**: instrumenting both collapse branches shows the `detach`
   branch never fires for that fixture on any target, so every collapse goes through the
   gated `dup` branch and the two arenas really do differ. What vanished is the *byte*
   difference, and the reason is the bug: pre-fix, `-fno-chain-store` left an unmarked
   collapsed chain, the replay went unfaithful, the body fell back to the parser's bytes
   and `ast_run_strat_seq` refused to run any strategy on it — *that* was the difference
   the cell read. Proof it was the bug and not the pass: **post-fix `-fno-chain-store` is
   byte-identical to pre-fix `-fchain-store`** on both targets. The flag-off path now
   reaches the state the flag used to be needed for.

   The pass is not dead there. A chain that FEEDS a second chain (`c = b = a = s + 1;
   a = b = c * 3 + a;`) still differs on all four targets, so `relay()` was added to
   `tests/optfire/src/chainstore.c` and the four cells are green again on the pass's real
   effect rather than on the bug's. `differs.txt` records why `relay()` is load-bearing.
   Two measurements sized the reach: over `tests/exec` + `tests/optfire/src` at `-O10`,
   `-fchain-store` changes bytes in 3 files on x86_64 and 2 on arm64, and in **0** on
   i386/riscv64 post-fix against 2 pre-fix — and both of those two were the synthetic
   optfire fixtures, never a real program.

   **Found on the way, unrelated to this fix and pre-existing at `1fa038ee`: `mcc -O11`
   ICEs on a chain feeding a 3-deep chain.**

   ```c
   int f(int s) { int a, b, c;  c = b = a = s + 1;  a = b = c = a * 2 + b;  return a + b + c; }
   ```

   `error: internal compiler error: vstack leak (1)`. It needs `chain-store` **and**
   `chain-store-live` together — `-O10 -fchain-store` ICEs, `-O10 -fchain-store
   -fno-chain-store-live` does not, and `-O11` ICEs with no flags at all because both are
   default-on there. `-O0`..`-O3` are clean, which is the only reason this has never been
   seen: the shipped ladder never turns the pair on.

   **FIXED, and it was not only an ICE — it silently miscompiled.** The assertion is
   `mcc_error` in `check_vstack()`, not an `assert`, so it is present in release and
   `NDEBUG` builds and a leak always fails the compile. But the leak and its mirror, an
   *underflow*, cancel: when one statement orphans a producer and a later one orphans a
   consumer, the depth is back to zero at function end, `check_vstack()` is happy, and
   the consumer stored whatever unrelated value was on the vstack. Over 500 random
   chain-heavy functions at `-O11`, `1fa038ee` compiles 212 with `vstack leak` and
   **silently returns wrong answers for 6 more**; `-O0`/`-O3` and `-fno-chain-store` are
   correct on all of them. Post-fix the same corpus is 0 and 0 at every configuration
   tried (`-O11`, `-O11 -fno-chain-store-live`, `-O11 -fno-chain-store`, the whole family
   off, `-O10`, `-O3`).

   One root cause, in `rir_to_arena()`'s `IR_OP_VSTORE` collapse, with two symptoms.
   `ast_dup_sub` rewrites `b = (a = E)` into `a = E; b = E'` where `E'` is a *copy* of
   `E`. When `E` reads `a` — `c = e = d = (s * d * s)` — the copy is not an expression
   that can be evaluated at `b`'s position, because `a` has already been written. The
   arena is a lie about what those statements compute, and it stays consistent only as
   long as nothing evaluates the copy. The `1u` fbit records this, and
   `ast_finalize_chainstores` pairs the run so the value is computed once
   (`AST_FB_STORE_VALUE_LIVE` on the producer, `AST_FB_STORE_CHAIN_REUSE` on the
   consumer); the faithfulness gate then passes and the strategies run. Both symptoms are
   the pairing being taken apart afterwards:

   - The marks are set on the first (faithfulness) replay and never cleared. `ast_dse`
     then poisons a store in the middle of a marked run — legitimately, it is dead — and
     the surviving partner keeps a mark whose other half no longer exists. A producer
     with no consumer leaks one vstack entry; a consumer with no producer takes an entry
     it did not put there. That is the ICE and the miscompile.
   - `ast_cprop`/`ast_cse` read the copy as if it described the value that store
     computes, and fold the *destination* to a constant derived from re-reading `a`
     after `a` was written (`b = a = s * 7 - a` with `a` known zero folds `b` to `0`).
     That is a miscompile with no vstack imbalance at all.

   Fixed in two places, and the fix is deliberately at the source rather than a guard on
   each consumer — an earlier attempt guarded `ast_dse`/`ast_cse`/`ast_cprop` on the `1u`
   bit, which is correct but suppresses the pass outright and took
   `optfire-{i386,riscv64}/chainstore` red for exactly the reason that row exists.

   1. `src/mccrir.c`: `rir_chain_dup_ok()` refuses the `ast_dup_sub` branch when the
      value expression reads the source store's target (or, for a non-leaf target such as
      `q->x`, reads any memory at all), and when it contains an `AST_StoreVal` — that
      node is a single-use reference to a value already on the vstack and duplicating it
      double-consumes. The collapse then falls back to leaving the `StoreVal` in place,
      which is what `-fno-chain-store` does and is correct. Every `1u`-tagged store the
      arena still contains is now one whose value child really is evaluable at its own
      position, so `dse`/`cse`/`cprop` need no special case and the pass keeps its full
      effect on the shapes that motivated it — `a = b = s`, `relay()`, and the whole
      `optfire` chainstore fixture are untouched.
   2. `src/mccast.c`: `ast_revoke_chainstores()` runs at the top of `ast_replay_body()`,
      before `ast_finalize_storevals`/`ast_finalize_chainstores`, and drops a pairing
      mark whose partner is no longer an adjacent `AST_Store` carrying the matching mark.
      `AST_FB_STORE_CHAIN_LIVE` was added so a `VALUE_LIVE` set by the chain pairing can
      be revoked without touching one set by `ast_finalize_storevals`. It only ever
      *subtracts*: the pairing decision is still made on the faithful arena, where it is
      known sound, and is only invalidated, never re-derived onto an optimized one.
      Revocation must skip non-`Store` nodes entirely rather than clearing their bits —
      `RIR_M_CASE` stores case data, not flags, in an `AST_Jump`'s `fbits`, and clearing
      bit 8/9/10 there rewrites a switch label. That mistake took
      `flagsweep-exec/replay-fallback` and `rir-coverage` red and is what those two cells
      caught.

   `ast_finalize_chainstores` also gained a mutual-exclusion guard: a store could be
   marked `CHAIN_REUSE` by the live pair with its predecessor *and* `CHAIN_MEMBER` by the
   member pair with its successor, and `ast_replay_bb` tests `CHAIN_MEMBER` first and
   never consumes the predecessor's live value. That is worth 83 of 400 `vstack leak (1)`
   ICEs on a struct/member chain corpus at `-O11`.

   Regression cell: `exec-chainlive/{chained_assign,assign_value_effects,dead_store_elim,
   cse,local_const_prop,region_store}` at `-O11 -fchain-store -fchain-store-live
   -fchain-store-member`. `exec-chainstore` pins only `-fchain-store`, which leaves
   `chain-store-live` off and never reaches the pairing pass, and no `-O` level below 11
   turns the family on, so nothing existing could have caught this. Five functions were
   added to `tests/exec/statements/chained_assign.c` — the 3-deep and 4-deep chains, a
   chain feeding a chain, a chain whose value reads its own target, and a chain whose
   target is killed by a later store. The cell fails on the unfixed tree with the ICE,
   and fails again with each of the two fixes ablated in turn (revocation off →
   `vstack leak (1)`; `rir_chain_dup_ok` off → wrong answers).

   Two things this did **not** fix and did not try to. The `-O` levels in `src/mccopt.h`
   and `tests/optfire/{defstate.txt,levelpins.txt}` are untouched — this removes a
   blocker on re-promoting the family, it is not a licence to re-promote it. And the
   member half of the pairing has no cell: every fixture found that reaches the
   `CHAIN_REUSE`/`CHAIN_MEMBER` collision also tripped debt #6a below at `-O1`, so it
   could not go into the shared `tests/exec` corpus. **Debt #6a is fixed as of
   2026-08-09** and that block is gone; the member fixture is still owed.
6a. **`storeval-rot` underflowed the vstack at `-O1` — FIXED 2026-08-09. It never
   miscompiled: the failure mode is an abort, measured, not assumed.** Found while
   fuzzing debt #6; reproduced identically at `1fa038ee` and on `3ddd9933`.

   ```c
   struct S { int x; };
   int f(int s) {
   	struct S r;
   	struct S *q = &r;
   	int a = 1, c = 3;
   	q->x = c = a = s + a;
   	return a + c + q->x;
   }
   ```

   `error: internal compiler error: vstack leak (-1)` at `-O1`, `-O2`, `-O3` and `-O11`;
   `-O0` is clean and `-fno-storeval-rot` cleared it at every level. Unlike debt #6 this
   needed no chain-store flag and landed on the shipped ladder.

   **The miscompile question, answered.** Debt #6 cancelled a leak against an underflow
   and returned wrong answers on 6 of 500 at `-O11`, so the same experiment was run here
   before anything was changed: 1400 random struct/pointer/chain programs with checkable
   output, each cross-validated against `gcc -O0`, `gcc -O2` and `mcc -O0` before use,
   then run at `-O1`/`-O2`/`-O3`/`-O11`, `-O3 -fno-storeval-rot`, `-O11 -fno-chain-store`
   and `-O11` with the whole chain-store family forced on. **Every imbalance this defect
   produces is `(-1)`** — 115 of 115 ICE messages over the first two corpora at `-O3` and
   106 of 106 at `-O11`, no `(+1)` anywhere. It never pairs with a leak of the opposite
   sign, so it never cancels, so it always aborts: **0 wrong answers over 1400 programs
   and every configuration above, before the fix**.
   That is the difference from debt #6 and it is why this was an availability bug, not a
   correctness one. It did also fault: one program in 500 segfaulted the compiler at
   `-O11`, non-deterministically (the underflowed read below `vstack[0]` lands on
   whatever precedes the array), so the fault is real but not reliable.

   **Root cause.** `ast_finalize_storevals`' rot path marks the innermost store
   `AST_FB_STORE_VALUE_LIVE | AST_FB_STORE_LIVE_ROT`, and the `AST_StoreVal` replay for a
   live store pushes nothing because the value is already on the vstack. The middle store
   of the chain is *not* marked live, so the outermost store's `AST_StoreVal` fell through
   to `ast_replay_value(a, ast_child(a, st, 1))` and re-evaluated the middle store's value
   child — the same already-consumed `AST_StoreVal` node — taking the entry a second time.
   Same shape as the `AST_StoreVal` case `rir_chain_dup_ok()` refuses in debt #6: a
   single-use reference to a live value evaluated twice. It fired on 78 of 500 at `-O1`,
   77 at `-O2`/`-O3`, 63 at `-O11` on a one-statement corpus, and 98 of 400 at `-O11` on a
   corpus carrying `volatile`, narrowing and bit-field targets.

   **The fix, in two halves, both at the source.** Neither half disables a pass. The
   recompute was the wrong default, not a special case that needed one more exception, so
   the fallback was inverted rather than patched.
   - `ast_replay_value_inner`'s `AST_StoreVal` fallback now **reloads the store's target**
     whenever that target is safe to re-evaluate, and only recomputes the value expression
     when it is not. That is the branch `ast_val_has_call` already took, and it is the
     faithful answer in general: the target holds the post-conversion value that C says
     the assignment yields, so `char c; q->x = c = a = s * 100 + 7;` now agrees with gcc
     where recomputing the value subtree skipped the narrowing.
   - `ast_finalize_storevals` refuses the live mark when any ancestor `AST_Store` on the
     walk from the `AST_StoreVal` up to the basic block holds the node in its *value* child
     and has a target that cannot be re-evaluated: `volatile`, a call, a nested store
     (`arr[i++]`), a VLA. Without this half a `volatile int` middle target still ICE'd —
     the guard is load-bearing, not belt and braces.

   **After: 1400 programs × 7 configurations, 0 ICE, 0 wrong answers, 0 faults.**

   **A second, older defect fell out of the same inversion, and it is why the fallback was
   inverted instead of special-cased.** The recompute path is unfaithful whenever the
   stored value reads the store's own target, because by replay time the target has already
   been written:

   ```c
   int c1(int s) { int a = 1, c = 3; c = 2 * (a = s + a); return a + c; }
   ```

   At `3ddd9933`, with **all** `storeval-*` flags and `chain-store` forced off, this
   returns the wrong answer under `-fno-replay-fallback` at `-O1` and `-O3` — the replay
   recomputes `s + a` after `a` was updated. Nothing to do with `storeval-rot`; the
   always-on `replay-fallback` was discarding the unfaithful body and hiding it, which is
   also why no existing cell saw it. `-fno-replay-fallback` is a shipped `-f` flag with a
   `flagsweep-exec` rung of its own, so this was a live wrong-answer path. The inverted
   fallback fixes it: the same program is correct at `-O0`/`-O1`/`-O3` with and without
   `-fno-replay-fallback`. Adding the reproducing shapes to
   `tests/exec/statements/chained_assign.c` is what turned `flagsweep-exec/replay-fallback`
   red on the half-fix, and it is green with the inversion — that cell is now the
   regression cover for this half.

   **What this does *not* buy, said plainly.** These chains stay **unfaithful**: with
   `MCC_RIR_PROD=2` every new fixture lands in `disc fallback/len` at `-O1`/`-O2`/`-O3`,
   so the parser's bytes ship and no arena strategy runs on those bodies. That is not a
   shortfall of the fix, it is what the shape is: for `q->x = c = a = s + a` the parser
   pushes *all three* addresses before evaluating anything, while the arena holds three
   sibling stores and replays address-value-store three times. `ast_storeval_push_leaf`'s
   leaf requirement is exactly the condition under which that reordering emits no bytes and
   is therefore invisible; a member target emits an address computation, so the reordering
   shows and the byte gate discards it — with or without this fix. The bug was that the
   compiler *aborted* instead of falling back. Making these bodies faithful is a different
   and larger job (the middle store would have to become live too) and is not attempted
   here.

   **The bank had to move, and the reason is the corpus, not the compiler.** Adding eleven
   inherently-discarded bodies drops `wide` `kept` from 96.576 to 96.5436 at `-O1` and from
   96.626 to 96.5941 at `-O2`/`-O3`, which is more than the 0.05 tolerance had left. Two
   measurements say the compiler is not what moved:
   with the *original* fixture, the fixed compiler reports `kept` 92.907 / 96.576 / 96.627
   / 96.627 against the pre-fix compiler's 92.906 / 96.576 / 96.626 / 96.626 — the same
   number. And `3ddd9933` was already **50 bodies stale** against its own bank (4387
   measured, 4337 banked) and already sat 0.0285 under the banked `-O1` `kept`, i.e. it had
   spent 57% of the tolerance before this branch existed. `tests/rir/coverage-bank.json`
   was refreshed with `--update-bank` for `wide` only; `self` is untouched and
   `rir-coverage` was green throughout.

   **Cost in emitted code: zero.** Compiling the *unmodified* stage-1 `src/mcc.c` at
   `3ddd9933` with the pre-fix and the fixed compiler gives a **byte-identical `.text`** at
   `-O1`, `-O2` and `-O3` (368,376 / 383,388 / 405,242 instructions either way; the objects
   differ only in a header field). An earlier reading of "+74 instructions" was an artifact
   of measuring the fix's own added source into the object — measure the compiler, not the
   compiler plus the patch. Compare the alternative the brief asked to be priced: turning
   the pass off with `-fno-storeval-rot` moves the same object to 398,390 at `-O3`,
   **−1.69%** — see the note below.

   Regression cells: `exec-storevalrot{1,2,3}/{chained_assign,assign_value_effects,
   local_const_prop,region_store}` — twelve new cells, `-O{1,2,3} -fstoreval-rot
   -fstoreval-constl -fstoreval-calllast`, pinning the flag explicitly at all three
   shipped levels. Eleven functions were added to
   `tests/exec/statements/chained_assign.c`: the reproducer, a chain through nested
   members, an array-element outer target, the `constl` variant, a chain inside a call
   argument, a `volatile` middle target under a member and under an array element, a
   narrowing (`char`/`short`) middle target, a middle store killed by a later write, and
   an impure outer target (`arr[i++]`).

   **Both halves are ablated and both go red, on the shipped ladder.** Six cells fail
   either way — `exec-storevalrot{1,2,3}/chained_assign`, `exec-chainstore/chained_assign`,
   `exec-chainlive/chained_assign`, `flagsweep-exec/replay-fallback` — and the two halves
   fail on *different* functions, which is the point:
   - replay half off → `tests/exec/statements/chained_assign.c:134: error: internal
     compiler error: vstack leak (-1)` (line 134 is `rot_member_chain`, the reproducer).
   - finalize guard off → `tests/exec/statements/chained_assign.c:203: error: internal
     compiler error: vstack leak (-1)` (line 203 is `rot_volatile_elem`, the `volatile`
     middle target under `arr[s & 1]`).

   `exec/chained_assign` at `-O0` stays green through both ablations, which is the control
   that says the cells are reading the optimizer and not the fixture.

   **Follow-up worth a row of its own: `storeval-rot` is a pessimization at `-O3`.**
   Measured on stage-1 `src/mcc.c` at `3ddd9933`, instruction counts from `objdump -d`,
   flag on against `-fno-storeval-rot`, and identical for the pre-fix and fixed compilers:
   `-O1` 368,376 vs 368,396 (**−20**, on is better), `-O2` 383,388 vs 383,213 (**+175**,
   off is better), `-O3` 405,242 vs 398,390 (**+6,852, off is 1.69% better**). The `-O3`
   gap is pre-existing and nothing to do with this fix. It also contradicts the ladder
   write-up's "`storeval-rot` … changes 0.0000% of emitted instructions", which holds at
   `-O1` and is off by 1.69% at `-O3`. No pin was touched here; this wants its own
   measurement and its own demotion argument, and the `-O2` sign says the answer may be
   "demote to a level, not off".

   **It got that measurement — 2026-08-09 — and the row stays at 1. The 1.69% is real
   and it is the wrong number.** Three findings, in the order they were taken.

   **(a) Where the 0.0000% came from, because that matters more than the row.** It is
   one cell: the `gain_pct` column of the `storeval-rot` row of
   `tests/optfire/levelbench.tsv` — cited here as `:47` when the file had 47 lines, and
   **that anchor is dead: the file is 30 lines / 16 data rows today and the row is at
   `tests/optfire/levelbench.tsv:26`**. Banked by `1ad3f1aa` and quoted in `893c1e84`'s commit message and
   at the ladder write-up below. `tools/optlevel-bench.py`'s `gain` is a **geometric mean
   of dynamic `instructions:u` over the 17 `tests/runtime` kernels** — not a static count,
   and not `src/mcc.c`. The same row's `fires_kernels` column is **empty**: the flag
   changed **zero of 17** kernel objects, so `gain` was a geomean over binaries that were
   bit-identical and **could only ever be 0.0000**. The tool knew — `optlevel-bench.py`
   has a "changes no kernel object; nothing to adjudicate" path — and the honesty axis
   fired correctly. The write-up quoted the derived zero instead of the "did not fire".
   Re-checked on today's tree: `storeval-rot` still changes **0 of 17** kernels at `-O2`
   and `-O3`, so no runtime-kernel benchmark can ever say anything about this flag, and
   driving `tools/runtime-bench.py` through an `--mcc` wrapper would be a null experiment
   by construction. **This was the seventh headline figure on this board that did not
   reproduce *as the list stood when this was written* (read the failed-to-reproduce table
   for the current count), and it failed differently from the other six: nothing was
   mismeasured, a
   correctly computed number of the wrong quantity was quoted as if it were the right
   one.** The rule it earns: *a ratio whose denominator did not move is not a measurement
   of zero effect, it is a measurement of no coverage* — quote `fires`, never `gain`,
   when `fires` is 0. And this is not a one-off: **24 of `levelbench.tsv`'s 47 rows have
   an empty `fires_kernels`**, i.e. change no kernel object at all, and **11 of those**
   carry a `gain_pct` of `-0.0000` that is a division over unchanged binaries. Any of the
   eleven can be quoted the same way this one was. **Verified independently 2026-08-09 —
   all three counts are exact — and three further defects were found in the same file;
   they are hazard 2 at the head of this board. The one that matters most for anyone
   reading this row: 32 of those 47 rows name flags that are no longer at levels 1–3 at
   all, so the table is a generation stale and nothing in CMake compares it.**

   > **CLOSED at the tool 2026-08-09 (`wt/benchtrap`).** The eleven can no longer be quoted
   > that way: `tools/optlevel-bench.py` buckets such a row `no-kernel-subject`, gives it a
   > `kernels_moved` column reading `0/17`, and prints `n/a` in `gain_pct`, `efficiency`,
   > `text_kernels_pct`, `best_kernel` and `best_kernel_pct` — as it now does for `error`
   > rows too, whose `0.0000` cost columns were equally fabricated. Two ctest cells,
   > `optbench/null-subject` and `optbench/null-subject-known-positive`, hold the rule and
   > the second one deliberately restores the old behaviour and must go red. The banked
   > table was relabelled in place with no value changed, so **`levelbench.tsv:47` is now
   > line 51**. `narrow` and `tree-copy-prop` are two more rows in this family that want a
   > stage-1/stage-2 instrument rather than a label. Full write-up in the measurement-tool
   > audit section near the top of this file, including the dilution hazard the fix does not
   > close: `gain` is still a geomean over all 17 kernels even when only one moved.

   **(b) The 1.69% is the inliner, and it is cold.** `-O3` differs from `-O2` by exactly
   one thing — `grep -c "MCC_OPTD_LEVEL(3)" src/mccopt.h` is **0**, and
   `src/mccast.c:2235` turns `MCC_OPT_INLINE` on at `optimize >= 3`. Reproduced on this
   tree (`70b92fb3`, so slightly different absolutes): `-O1` 366,000 vs 366,020 (−20, on
   better), `-O2` 379,404 vs 379,280 (+124), `-O3` 401,236 vs 394,404 (+6,832, 1.73%).
   Add `-fno-inline` at `-O3` and the gap collapses to **+124** — identical to `-O2` —
   and `-O2 -finline` reproduces the full +6,832. So the entire 1.69% is inline
   replication, which is why it appeared only after `ffa6cf16` deleted the 512-body pool
   cap: `src/mcc.c` is the only TU in the tree big enough to have blown it. **Corollary
   the ladder should act on: `tests/optfire/levelbench.tsv` is stale with respect to
   `ffa6cf16` for every row whose effect scales with inlining, not just this one.**

   And those 6,832 instructions are **cold**. Stage-2 — two compilers built from the
   identical snapshot, differing only in this flag, verified to emit a **byte-identical**
   object (`md5 10f27ec3…`), then benchmarked compiling that snapshot at `-O2`, `n=15`
   interleaved, each arm duplicated so the run carries its own noise control:

   | stage-2 config | `instructions:u`, median | vs shipped |
   | --- | ---: | ---: |
   | `-O3` (shipped) | 5,928,147,497 | — |
   | `-O3 -fno-storeval-rot` | 5,914,420,963 | **−0.232%** |
   | `-O3 -fno-inline` | 5,936,353,998 | +0.138% |
   | `-O3 -fno-inline -fno-storeval-rot` | 5,921,800,105 | −0.107% |

   The flag's dynamic cost is **0.232% with the inliner and 0.245% without it** — the same
   number either way, so it is *not* inline-mediated at all. The +6,832 cold bytes are
   worth nothing; the +124 hot ones are worth 0.23%, and a per-function diff says where:
   `gen_op` +55, `parse_atomic` +46, `parse_number` +17, `expr_cond` +17, `parse_string`
   +16 — 30 functions differ and the growth is in the parser/codegen hot set. Two
   identical arms of each config bracket the noise at **0.004%**, so 0.232% is ~60σ.

   In `leveltime.tsv`'s own columns, the row should read `storeval-rot 1 1 -2.210 +0.034
   -0.2327 differs - - -`, against the banked `-0.211 2.241 -0.0245`. Every field moved
   and none changed sign on the counter: the compile-time cost is **10× larger** than
   banked, the counter's stage-2 verdict is **9.5× larger**, and the banked `+2.241` of
   stage-2 *time* was layout — a clean n=21 re-read with an identical-config control of
   **+0.002%** puts it at **+0.034%**, i.e. nothing. Stage-1: n=21 interleaved, CPU time
   512.60 ms → 501.27 ms with the flag off (**−2.210%**) against an identical-config
   floor measured at **+0.455%** in the same run, and `instructions:u` agreeing at
   **−2.313%**. That is 4.9× the floor with the layout-immune counter agreeing in sign
   and magnitude, which is the strongest form this tree's rule recognises.

   **(c) So every axis says demote, and the row stays anyway.** `-fno-storeval-rot` is
   not an alternative lowering — its `levelpins.txt` row already said its off-state is "an
   incomplete path". Measured, that is worth **8.7 points of `kept`**: `tools/rir-coverage.py
   --corpus self` reads 83.069 / **83.103** / **83.242** / **83.242** with the flag off
   against 83.069 / 91.903 / 91.968 / 91.968 shipped, and the run **FAILS** at `-O1`,
   `-O2` and `-O3` against the bank. Per TU: 2,816 → 2,779 bodies replayed, 1,366,667 →
   1,237,966 `used` bytes. Demoting the row therefore means banking an **8.7-point
   coverage regression** — twice what board row 2 was trying to win — to buy 2.31% of
   compile time and 0.23% of stage-2. That is not a trade this tree should take, so
   `src/mccopt.h:40` stays at `MCC_OPTD_LEVEL(1)` and no pin moved.

   The residue is a genuine open question, and it is not about this flag: for these 37
   bodies the strategy suite makes the hot code **larger and slower**. `kept` counts
   bodies that run the strategies, not bodies the strategies helped. Anyone who re-opens
   `storeval-rot` should re-open it as "which strategy pessimizes `gen_op`", not as a
   ladder move.

   Reproduce (all of it, from the repo root, with `cmake-debug` built):

   ```
   FLAGS=$(python3 -c "import json,shlex;cc=json.load(open('cmake-debug/compile_commands.json'));\
   r=[x for x in cc if x['file'].endswith('/mcc.c')][0];\
   print(' '.join(a for a in shlex.split(r['command'])[1:] if a.startswith(('-I','-D'))))")
   cmake-debug/mcc $FLAGS -O3 -fno-storeval-rot -c src/mcc.c -o /tmp/m.o
   objdump -d --no-show-raw-insn /tmp/m.o | grep -cE '^[[:space:]]+[0-9a-f]+:'
   MCC_RIR_PROD=2 MCC_RIR_PROD_OUT=/tmp/p.txt cmake-debug/mcc $FLAGS -O3 -fno-storeval-rot -c src/mcc.c -o /tmp/m.o
   ```
7. **The "464 skipped cells under `debug`" number was wrong, and so was its cause —
   PARTLY FIXED.** It was not `MCC_CROSS_DIR` and not a missing `cmake-cross`:
   `MCC_CROSS_DIR` defaults correctly, cross cells are registered unconditionally with
   `FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT"` and `SKIP_RETURN_CODE 77`, and the
   `mcc_cross_build` fixture builds the dir at test time. Measured on the current tree,
   two separate things were happening and neither was 464:
   - **The `native` label filter hides cells — 39 before the second fix below, 59
     after.** `ctest --test-dir cmake-debug -N` reported 8942 against 8903 for
     `-N -L native`, and now reports 9106 against 9047. The `debug` test preset inherits
     `_test-native`, which is `"filter": {"include": {"label": "native"}}`, so every
     `qemu`/`wine`/`macho`/`docker` cell is excluded before it runs. Added test preset
     **`debug-all`** (same configure preset, no label filter) rather than dropping the
     filter, so the fast native loop is preserved.
   - **`if(TARGET mcc-<arch>)` silently dropped 164 cells at configure time.** 8942
     `add_test` in `cmake-debug` against 9106 in `cmake-cross`; the difference is
     exactly 144 `optfire-{arm64,i386,riscv64}/*` and 20 `*-docker`, none of which had
     an `else()`, so they vanished without a skip line. The guards asked whether the
     target exists *in this build*, not whether the binary exists in `MCC_CROSS_DIR` —
     so `cmake-cross/mcc-i386` could be present and `i386-fastcall-abi` still be an
     echo-SKIP. Replaced with `mcc_cross_cc(<arch> <ccvar> <fixvar>)`, which prefers
     `$<TARGET_FILE:mcc-<arch>>`, falls back to `${MCC_CROSS_DIR}/mcc-<arch>` with
     `MCC_CROSS_BUILT` added to the fixtures, and yields empty otherwise. `cmake-debug`
     now registers **9106**, the same as `cmake-cross`. `arm-asm-testsuite` keeps the
     `if(TARGET ...)` form on purpose: it is an `add_custom_target` with
     `DEPENDS mcc-arm`, not a test cell.

   Measured after the fix, on this host, `MCC_CROSS_DIR` pointing at the built
   `cmake-cross`:

   | run | cells | passed | skipped | failed |
   | --- | ---: | ---: | ---: | ---: |
   | `ctest --preset debug` | 9047 | 8672 | 375 | **0** |
   | the 59 cells `debug-all` adds (`-LE native`) | 61¹ | 15 | 46 | **0** |

   ¹ ctest pulls the `mcc_build` and `mcc_cross_build` fixtures in regardless of the
   label filter, so 59 + 2. **The real skip count under `debug` is 375, not 464**, and
   286 of those are `exec-*` feature skips that have nothing to do with cross. All 144
   newly-unmasked `optfire-*` cells ran and passed; `dash-s-bytes-arm64`,
   `dash-s-bytes-riscv64` and `i386-fastcall-abi` went from echo-SKIP to real passing
   runs. Every one of the 20 newly-unmasked `*-docker` cells skips here, and so do the
   five that were already registered: `tools/*-docker.sh` probes whether the container
   can see its bind mount and exits 77 when it cannot, which it cannot for a build dir
   under `.claude/worktrees/`. That is the host, not the cells.

   `tools/{selfhost-run-parity,shadow-iv-sweep,qemu-selfhost}.sh` hardcoded
   `$root/cmake-cross` and ignored `MCC_CROSS_DIR`; they now honour it, and the cells
   that drive them pass it through in `ENVIRONMENT`. `cmake/cross_build.cmake` gained an
   opt-in `MCC_CROSS_REQUIRED` (default `OFF`) that turns its silent
   missing-cache `return()` into a `FATAL_ERROR`, so a missing cross build can never
   again present as a wall of green skips.

   Legitimate residual skips on this host, left alone: three `run-tier/*-win32|wince`
   are unconditional; every `macho-*` that needs real Darwin. `qemu-arm` and `qemu-i386`
   **are** on PATH here, contrary to an earlier note — the `selfhost-qemu-*` cells skip
   on a missing vendored sysroot, not a missing emulator.

## Landed — `*p`, pointer `++`/`--`, and a host address that lowers, 2026-08-08

### The decision, and why the two rejected options were rejected

**(c) a pointer stays a host address; lowering is gated dynamically.** Binding 2
(`mcc_vkr.{bmem,mmem,pmem,bmemsz}`, `MCC_VK_MEM_DEFAULT` = 1 MiB, `HOST_VISIBLE |
HOST_COHERENT`, permanently mapped, exposed by `mcc_gpu_mem`) means an address inside
`[pmem, pmem+bmemsz)` **already** has a stable byte offset. So the address→offset map is
one 64-bit subtract and one test that the high half is zero — `ast_eval_slice_rw_addr` on
the reference, `spv_mem_off` on the device, instruction for instruction — and the low half
is handed on either way so that `spv_region_addr` / `ast_eval_slice_addr_ok`, not the map,
decides what an out-of-range access reads. The CPU reference and the device then read and
write the **same physical bytes**: no mirror, no copy-back, no aliasing question, no
extent question.

(a) mirroring host allocations was rejected for having no extent source —
`ast_eval_slice_obj_fn` is NULL inside the compiler and resolves an extent only for a
local object `Ref`, never for a pointer parameter — and because aliasing would diverge
silently. (b) binding-2-origin-only was rejected as circular: the only honest producer is
the allocator, ranked last on the board at +50 blocks / 0.30%.

### Measured, 440-block musl/string corpus, same dump before and after

`slicerun --no-ptr` reproduces the "before" column exactly, in the same binary, by
leaving the mapping unarmed — which is also the switch that proves the numbers are the
pointer work and not something else that moved.

| | before | after |
| --- | ---: | ---: |
| corpus `frame-compared` | 94 | **143** |
| eligible blocks (`--census`) | 120 | **174** |
| `*p` loads lowered, of 159 | 0 | **159** |
| pointer `++`/`--` lowered, of 168 | 0 | **168** |
| `*p = v` stores lowered, of 21 | 0 | **10** |
| blocks with a `*p`, of which eligible | 63 / 0 | 63 / **54** |
| runs that actually dereference the mapping (`frame-mem`) | 0 | **49** |

`memcmp` / `strcmp` / `strncmp` / `memchr` — the four the item was justified by, and **0**
on the board — are now **6 frame runs compared on the device**: `memchr` 3, `memcmp` 1,
`strcmp` 1, `strncmp` 1. Neighbours came with them: `strspn` 4, `strlen` 4, `strlcpy` 5.

### The 11 `*p = v` stores that still refuse, and why that is deliberate

Binding 2 is one region shared by every lane, so it is a `spv_region_shared`, and
`spv_store_region` sets `m->failed` for a sub-word store there: the read-modify-write it
compiles to is not atomic against another lane writing a different byte of the same word,
and nothing at emit time can prove two lanes' pointers are in disjoint words. So a `char`
store through a pointer refuses; 4- and 8-byte ones lower. `memcpy`'s byte loop is on the
wrong side of that line and stays there until either `StorageBuffer8BitAccess` or a
disjointness proof exists.

### Two defects found by building this, both pre-existing

**The loop merge discarded the exiting iteration's definedness.** `mcc_slice_spv_stmt`
took `m->def` at `l_merge` from `d_phi`, the value *entering* the header — so an undefined
access in the condition on the iteration that fails the test was thrown away, while the
CPU reference's sticky flag kept it. Reachable before this branch through `arr[i]` in a
loop condition; unreachable in practice because no corpus loop had one. `*p` in a loop
condition is the common case, and `strlcpy` failed on it immediately. Fixed by carrying
`d_exit` out of `l_test` (or out of `l_cont` for `do`), both of which dominate the merge.

**`--mutate` was blind to `++`/`--`.** The operator perturbed stores only, so a block whose
whole effect was `n--, l++, r++` — which is most of what the four named functions
contribute — was dispatched, compared and reported clean no matter what the kernel did.
With the perturbation added to that arm, the four-function cell goes from 1 red run of 6
to 6 of 6.

### Proof the new cells can fail

Each break applied alone, built, run, reverted. `musl` is the 143-run corpus, `four` is
the `memcmp`/`strcmp`/`strncmp`/`memchr` subset.

| deliberate break | musl red | four red |
| --- | ---: | ---: |
| none | 0 | 0 |
| device pointer `++` loses its element-size scale | **19** | **1** |
| CPU `*p` load reads one byte over | **7** | 0 |
| CPU `*p = v` store writes `val+1` | **5** | 0 |
| device loop merge reverted to `d_phi` | **1** | 0 |
| `--mutate` | **115** | **6** |

The `*p = v` row is caught **only** by the byte-for-byte comparison of the mapping — the
stored value reaches no frame slot and no return value — which is what makes step 4 of
the plan (move the harness's allocations inside the mapping) load-bearing rather than
decorative. The two zero cells in the `four` column are honest and worth stating: the six
runs those four functions contribute are pointer-increment blocks, not deref blocks. The
derefs are in the loop *conditions*, which belong to the enclosing `AST_If` and are
compared as part of a different block.

### Residual hazards, none papered over

- **`nbyte` is the whole 1 MiB.** An access that leaves its object but stays in the buffer
  is in range, and both executors agree on garbage. This is a real weakening of "no
  PageFault by construction" — the property still holds, but it now bounds a wild pointer
  to the whole shared region rather than to one object. `SpvRegion.nbyte` is already a
  parameter; narrowing it needs a per-object extent for a *pointer*, which is exactly what
  option (a) was rejected for not having.
- **A workgroup is dispatched whole, and there is no lane guard.** Lanes past `ntuple` get
  a zeroed frame, and a zeroed slot read as a pointer is an address like any other: it
  maps somewhere in the shared region and *stores* there, which the reference never does.
  This is what made `slice/real` fail with `cpu=00 gpu=ff` at byte 0. Nothing in the ABI
  carries a lane count, so `mcc_slice_run_frame_gpu` now **refuses** a binding-2 kernel
  unless `ntuple` is a whole multiple of `MCC_GPU_LOCAL_SIZE`, and the harness dispatches
  64 tuples. A real caller must obey the same rule or add the guard.
- **Lane isolation in the harness is by construction, not by proof.** Each tuple gets an
  8 KiB window and the seed pattern makes every walk terminate within tens of bytes, so no
  lane reaches another's window. A pointer that walked far enough would, and both
  executors would then disagree because the reference runs its 64 tuples sequentially.
- **Debt #3 becomes reachable the moment binding 2 grows.** It was not grown here.

## Landed — `cli/perfn_inproc` is green, and the pass was never inert, 2026-08-08

The cell asserted `DIFFER` and got `SAME` across at least six probed commits. Two readings
were on the board: the case needs a discriminating input, or the pass is inert and earns
no level. **It is the first, and the second is now refuted by measurement rather than left
open.** Nothing in the compiler changed; the test did.

### The input was the whole problem

The case's own source cannot observe `-fopt-perfn-inproc` at any tier — `-O3`, `-O8` and
`-O12` all give 2006 B with the flag on and off. The sibling input in
`tests/optfire/src/perfn_inproc.c`, already green in `optfire/perfn_inproc`, observes it
immediately:

| input, `-fno-inline-functions -O3` | flag off | flag on | |
| --- | ---: | ---: | --- |
| the old `big`/`tiny` case | 2006 B | 2006 B | SAME at `-O3`/`-O8`/`-O12` |
| `chunk`/`driver` shape | **3382 B** | **2022 B** | **DIFFER, −40.2%** |

So the earlier note that "the case disables the precondition of the thing it is testing"
is **wrong**: `ast_inline_pass_env` is `mcc_opt(s1, MCC_OPT_INLINE_FUNCTIONS)`, so
`-fno-inline-functions` sets it to 0 and is what makes `!ast_inline_pass_env` — and hence
`do_inline` — reachable. That flag is the precondition, not the obstacle. What the old
case lacked was a callee whose graft is a size win.

### What the cell asserts now

1. `-fno-inline-functions -O3`, flag off vs on → **DIFFER**. The pass fires and shrinks.
2. Default inliner on, flag off vs on → **SAME**. This pins the gate-ordering fact the
   previous investigation established: `INLINE_FUNCTIONS` is level 2 and
   `OPT_PERFN_INPROC` is level 8, so at any tier where the flag is on by default the
   post-capture inliner has already closed the gate.
3. The grafted binary runs, `rc=80`, matching `-O0`. The pass must not change behaviour.

Every `{MCC}` invocation now sets `XDG_CACHE_HOME={W}/pfic`, closing the unsound premise
recorded earlier: `ast_slice_consume` reads `~/.cache/mcc/sl-*.ck` whose salt excludes the
`-f` flags. The `-fopt-slice` / `-fno-opt-slice` pair is gone — it was never what made the
flag observable, and it made the case depend on a cache the case did not isolate.

**Observed red both ways.** Dropping `-fopt-perfn-inproc` from arm 1 turns the cell red;
restoring it turns it green. Determinism checked over three fresh cache dirs: 3382 → 2022
every time. `optfire/perfn_inproc`, `flagsweep-exec/opt-perfn-inproc` and
`cli/perfn_search` all stay green.

**`SAME` was never banked**, per the standing instruction. The verdict recorded is the one
the measurement supports: the pass works, it is worth −40% on a shape that suits it, and
it still does not earn a lower tier — the corpus measurement that made real code 0.333%
bigger stands untouched, as does the diagnosis that the trial's selection metric
(`ind - ast_body_ind_sv`) does not predict object size.

## Merged — five parallel branches, and the ratchet re-banked for the fourth time, 2026-08-08

The five sections below landed on separate branches and were merged in one pass:
`wt/spvcols`, `wt/frameops`, `wt/marshal`, `wt/harness`, `wt/deref`. Textual auto-merges
in `src/mccslice.h`, `src/mccgpu.h`, `tools/slicerun.c` and `CMakeLists.txt` were checked
by symbol afterwards rather than trusted: the `for`-loop child reorder survives in all
three statement switches, `spv_region_shared` and the `shared`-flag refusal coexist with
the binding-2 `id_mem` work, and the known-positive list is the union
`wide64 ops frame gpu sched bytes deref`.

`slice/*` + `gpu/*` + `must-run`: **30 of 30**. Full suite: **8940 pass, 2 fail**, both
pre-existing and both reproduced on a detached build of `22accb27`:
`cli/perfn_inproc` (expects `DIFFER`, gets `SAME` — the row already documented as red on
purpose) and `rir-coverage`, treated below.

### `rir-coverage` failed by one ten-thousandth of a point, and it is dilution again

`nodes_pct_strict` at `-O0` came in at **25.8706%** against a banked floor of **25.9207%**
with `--tol 0.05` — over by **0.0001 points**. Bisected against a detached `22accb27`
build before touching the bank, because the standing warning says a ratio over the
compiler's own source is not a regression signal in either direction:

| | `22accb27` | merged |
| --- | ---: | ---: |
| arena nodes (denominator) | 434,204 | 434,401 (**+197**) |
| `nodes_pct_strict` | 25.8724% | 25.8706% |
| absolute lowerable nodes | ~112,339 | **~112,385 (+46)** |
| margin against the floor | +0.0017 inside | −0.0001 outside |

**Absolute lowerable nodes rose while the ratio fell** — the same signature as the three
previous re-bankings, and the same cause: `src/mcc.c` amalgamates `src/mccgpu.c`,
`src/mccgpu.h` and `src/ast_eval_slice.h`, so adding device-layer code enlarges the census
subject. The baseline was already sitting **0.0017 points inside the tolerance band**, so
this was one edit away from failing regardless of what the edit was. Re-banked with
`--update-bank-low` for `self` at `O0,O1,O2,O3`; `rir-coverage`, `node-census`,
`rir-gap-classes` and `rir-lowerable-classes` all green after.

Worth saying plainly: a floor whose baseline margin is 0.0017 points is not measuring
anything. It fires on the next commit to touch the device layer whatever that commit does.

## Landed — `spvgate` was measuring a type-stripped arena, 2026-08-08

### What it was blind to

`MCC_ARENA_DUMP` writes 14 columns per node. `tools/slicerun.c` reads all 14.
`tools/spvgate.c` read **7**, its `RawNode` carried 6 fields plus the id, and
`rebuild_arena` passed a hard-coded `0` for `type_ref`. Columns 8–14 had zero consumers
and `ast_eval_slice_obj_fn` was never installed, so `ast_eval_slice_dynidx` returned 0 at
its first line on every node. `gpu/spv-slice-real` therefore ran the emitters over an
arena that **structurally could not exhibit a runtime-index shape** — the whole B1
feature — and any conclusion drawn from `spvgate --arenas` about typed shapes was drawn
from an arena with the types removed. The previous section's closing note, "`spvgate` is
unaffected (its `sscanf` reads 7 fields)", was the bug, stated as a reassurance.

Measured on the corpus `gpu/spv-slice-real` uses (`tests/exec/{expressions,codegen}/*.c`
at `-O2`, all 32 files, 161 bodies, 25,398 dump lines): **14** `Binary('+')` nodes over a
`VT_LOCAL|VT_ARRAY` base, and **14 of 14** carry a non-zero extent *and* element type in
columns 13/14. The information was in the file the whole time.

### The reader was only half of it, and the second half was louder

Widening the `sscanf` and installing the hook, changing nothing else, took the corpus from
0 mismatches to **374,986**. That is not an emitter divergence — it is `spvgate`'s own
`collect_lives`, a homegrown live-in collector that walks `AST_Ref` nodes only. An indexed
object needs one live-in slot **per element**, laid out consecutively, because the device
resolves an element as `slot_of(base) + index` at run time; `ast_eval_slice_livein_obj`
exists to do exactly that and `collect_lives` did not know about it. So the CPU reference
refused (no env entry for `base + elem*esize`) while the device happily read a neighbouring
live-in's slot. Replacing `collect_lives` with the shared `ast_eval_slice_livein` fixes it
and makes the two tools agree by construction: `spvgate` now accepts **572** slices, the
same 572 `slicerun` accepts on the identical dump.

### `ast_eval_slice()` drops the poison flag it sets

With `MAX_LIVE` raised from 4 to 8 (`MCC_SLICE_MAXLIVE` is 8; a 4-element array plus its
index is 5 slots, so 4 admits only constant-index shapes) the corpus showed 4 definedness
divergences, all `cpu=1 gpu=0`, 1,056 lanes. The device was right. An out-of-range index
sets `ast_eval_slice_undef`, but the `ast_eval_slice()` wrapper neither resets nor returns
it — only `mcc_slice_frame_exec_cpu2` reads that flag. Every other caller is safe today
only because `ast_eval_slice_kind_ok(..., allow_load=0)` refuses all Loads on the
expression path, so no production caller can reach a `dynidx`; `spvgate` lowers through
`spv_expr` with no such gate and is the first caller that can. Handled in `spvgate` the
way the frame runner handles it. **The gap in `ast_eval_slice()` itself is still there and
is a live trap for the next caller that admits Loads** — it is inert in the compiler only
because `ast_eval_slice_obj_fn` is NULL there.

### Before and after, on the real corpus

| | before | after |
| --- | ---: | ---: |
| arenas | 161 | 161 |
| bodies-with-lowerable-slice | 114 | 114 |
| slices | 576 | **572** |
| slices carrying a resolved runtime index | **0** (by construction) | **8** |
| dispatches | 2,675 | 2,645 |
| points | 37,600,204 | 43,363,136 |
| compared | 36,979,227 | **41,168,239** (+11.3%) |
| vacuous | 620,977 | 2,194,897 |
| mismatches | 0 | 0 |

Slices *fell* by 4 because object expansion lets a larger subtree lower at a higher node,
and `scan_subtree` stops at the first node that lowers — 4 fewer, larger slices. Of the
new comparisons, **920,192 lanes** are value-compared on runtime-index shapes and
**1,573,920** are lanes where both executors independently agreed the index left its
object. Neither number could be non-zero before.

So the blindness was real **and** it had a measurable cost: this is not one of the
zero-payoff results. The corpus was never exercising B1 through `spvgate` at all.

### Proving the new path is not itself blind

`compared` rising is necessary but not sufficient — both executors can make the same
mistake and agree. Mutating the emitter directly, one edit at a time:

| emitter mutation | corpus mismatches | verdict |
| --- | ---: | --- |
| `spv_dyn_elem` masked element `u` → `u ^ 1` | **370,624** | caught |
| `spv_dyn_elem` bound `u < nelem` → `u < nelem + 1` | **0** at `MAX_TUPLES` 2^18 | **not caught** |
| same, at `MAX_TUPLES` 2^20 | **131,072** | caught |

The bound's upper edge was a cell that could not fail. A 5-live slice only fits rungs
w=1 and w=2 under a 2^18 tuple cap, so its index only ever took the values {0, 1, −2, −1}
— `u == nelem` was never sampled and an off-by-one in the bound was invisible while the
masking half of the same three-instruction sequence was fully checked. Raising
`MAX_TUPLES` to 2^20 admits the w=4 rung for 5-live slices (index range [−8, 7]), which
straddles every element count in the corpus (3, 4, 5, 6). Cost measured on this box:
full corpus 14.1s → 15.5s, `gpu/spv-slice-real` 25.1s → 34.2s. The built-in `CASES` suite
is unchanged by the cap — its widest reachable span is 2^16.

`--mutate` still fails on both the suite (rc=1) and the corpus (rc=1), including on a
corpus filtered to only the 7 arenas containing an indexed object (rc=1). `--corrupt`
is byte-for-byte unchanged against baseline: words 1/2/5 rc=0, word 3 rc=134, word 12
rc=136 — words 1/2/5 flipping bits the driver tolerates is pre-existing and not a
regression.

### `type_ref` restoration was possible

Restored, using the interned dense ids exactly as `slicerun.c` does, alongside `sym`,
`fbits`, and the `bp`/`bs` bitfield pair via `ast_set_type_bf`. These are interned ids
installed into pointer-shaped slots, which is safe here for the same reason it is safe in
`slicerun`: nothing on this path dereferences them (zero uses of
`ast_sym`/`ast_type_ref`/`ast_fbits`/`ast_type_bp`/`ast_type_bs` in `ast_eval_slice.h`,
`mccgpu.h`, `mccslice.h`). It changes no measured number on this corpus — it is there so
the two readers agree rather than because anything reads it yet.

### Unverified

The `mslgate` arm (`SPVGATE_MSL=1`) is APPLE-only and **was not executed**. It was
verified to compile: `cc -DSPVGATE_MSL=1 -fsyntax-only` is clean and the object links 51
`msl_*` references. `msl_expr` has no `dynidx` arm at all, so on that arm runtime-index
shapes are refused at `trial_lower` and `runtime-idx` will read 0 — no crash and no false
agreement, just no coverage.

## Landed — the `for` loop ran its increment before its body, 2026-08-08

Four candidate relaxations of the frame predicate were measured before any of them was
built. All four are worth **+0 blocks**, so none was built. What the measuring turned up
instead is a semantic bug in a shape that is already landed: `mcc_slice_frame_stmt_ok`,
`mcc_slice_frame_exec_stmt` and `mcc_slice_spv_stmt` all read `AST_If` op 3 as
`{cond, body, incr}`. The arena says `{cond, incr, body}`.

### The bug, and why 244 clean device comparisons could not see it

`ast_loop_parts` reads op 3 as `incr = child 1, body = child 2`, and a directed dump
confirms the arena agrees: `for (i = 0; i < n; i++) { s = s + 5; }` at `-O1` emits
`If(op 3){ Binary(TOK_LT), BasicBlock{Unary(TOK_INC) i}, BasicBlock{Store s} }`. The three
frame switches took child 1 as the body and ran child 2 after it, so every `for` loop in
the frame path ran its increment **before** its body.

Measured on a real arena, not argued: `int f(void){ int s=0,i; for(i=0;i<3;i++) s=s+i;
return s; }` compiles and prints **3**; the frame CPU reference returned **6** — 1+2+3
instead of 0+1+2. Both executors read the same two child slots, so both were wrong in the
same direction and the CPU-vs-device differential agreed with itself: 244 frame runs
compared, 0 mismatches, before the fix and after it. This is the same failure mode as the
`base + K` byte-offset fold recorded below — a wrong answer both executors share is
exactly what a differential cannot report.

Nothing shipped wrong. The only callers of `mcc_slice_frame_*` today are in
`tools/slicerun.c`; the compiler never acts on a frame result yet, so the bug was latent
and would have gone live the day the frame runner is wired into the compiler.

Fixed in all three switches. The `slice/frame` suite had a `while` case and no `for` case,
which is precisely why this survived; it now has one, built in the arena's child order and
asserting what C means (`sum(0..n-1) = n(n-1)/2`, never `n(n+1)/2`) rather than what the
other executor thinks. **Observed red first**: 4 of 177 checks failed on the sum
assertion, 0 after. Corpus after the fix is unchanged at 319 accepted / 244 compared / 201
statements / 0 mismatches, as it must be — the fix changes values, not acceptance.

### Zero payoff #6: condition-less `for` (`AST_If` op 8)

Measured before building, and not built.

| corpus | bodies | non-empty blocks | eligible | op-8 nodes | ineligible blocks containing op 8 | eligible with op 8 admitted |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| first 60 of `tests/exec` (the standard one) | 344 | 947 | 319 | **0** | 0 | **319 (+0)** |
| all 304 of `tests/exec` | 1271 | 3203 | 1126 | **4** | 3 | **1126 (+0)** |

Op 8 does not occur at all in the standard corpus, and in the full corpus its 4 instances
sit in 3 blocks that something else blocks as well. The delta was measured by admitting
op 8 in `mcc_slice_frame_stmt_ok` and re-running the census, not by counting constructs —
the B1 lesson.

There is also a structural reason not to want it, independent of any corpus. A
condition-less `for` has no exit except `break` or `return` inside its body, and both are
`AST_Jump`/`AST_Return` in statement position, refused everywhere in the frame path. So
the only op-8 loop the predicate could ever admit is one with **no exit at all**, which
runs to `MCC_SLICE_TRIP_MAX` (65536 iterations), is then marked undefined, and falls back
to the CPU: a dispatch that is guaranteed to be wasted. Both corpus instances confirm the
shape — `tests/exec/statements/empty_for.c` exits by `break`, and
`tests/exec/codegen/nocode_wanted.c` is `for (;;) printf("error\n")`, an `Invoke` with no
exit. Admitting op 8 would buy zero blocks and cost dispatches.

### Zero payoff #7: `x ?: y` (op 9) — plus a re-measurement that confirms #5

- **Op 9** is refused on arity by `ast_eval_slice_kind_ok`. Population: **2 nodes** in
  both corpora, in **1** ineligible block, **0** of them at a block's top level. Relaxing
  the arity test to admit a 2-child op 9 leaves eligible at 319 and 1126. **+0.**
- **Store destinations.** Census of all 987 store destinations in the standard corpus:
  834 local `Ref`, **10** `.field` (`AST_OP_MEMBER`), **0** `&` (`AST_OP_ADDR`), 143
  other; full corpus 70539 / 259 / 0 / 1185. Folding the store destination through
  `ast_eval_slice_frame_off` — the resolver the *load* path already uses — leaves eligible
  at 319 and 1126. **+0.** The earlier "+0 for folding a store destination through
  `.field`/`&`" recorded under B1 was a model; this patched
  `mcc_slice_frame_stmt_ok` itself and reaches the same number, so that line is now
  confirmed against the real predicate and needs no revisit.

### `switch` (op 6) is not a quick win and the ceiling is 8 blocks

Population: **7 nodes** in **8** ineligible blocks (6 at top level) in the standard
corpus; 35 nodes in 41 blocks (30 top level) in the full one. Not started, and the ceiling
is what argues against starting: 8 of 947 blocks (0.84%) *if* the switch were the only
blocker, and the op-8, op-9 and store-destination results above all say it will not be.
It also needs a genuinely new control-flow shape (`OpSwitch` plus case merges) and every
non-degenerate switch separates its cases with `break`, i.e. `AST_Jump`, which the frame
path refuses everywhere. Measure the only-blocker figure before anyone starts it.

### The measurement harness, and its positive control

`slicerun --arenas <dump> --limit 0 --quiet` over the standard corpus reproduces the
shipped reference exactly — 783 expression slices, frame-accepted 319, frame-stmts 201, 0
mismatches — so it was trusted only after it did. Note `--limit 0`: the `slice/real` ctest
cell passes `--limit 400`, which stops the scan at body 218 of 344 and reports 157/98
rather than 319/201. Both are correct; they are different scans.

New `slicerun --census` mode: counts non-empty `AST_BasicBlock`s, how many
`mcc_slice_frame_from_ast` accepts, the op-6/8/9 populations split by "anywhere in an
ineligible block" versus "at that block's top level", and store destinations by shape. Its
eligible count is the real predicate, and equals `frame-accepted` on the same corpus.
Earlier sections quote **358** eligible blocks; that figure came from a model of the
predicate, not from `mcc_slice_frame_from_ast`, and the real number on the same 947 blocks
is 319.

Positive control, because a census that always prints the same number proves nothing:
disabling the `x++`/`x--` arm drops eligible from 319 to **228** (−91). The zero deltas
above are therefore zeros the harness could have seen move.

### One accident turned into a check

`ast_eval_slice_kind_ok`'s `AST_If` arm tested only `ast_nchild(a, n) != 3` and never
looked at `ast_op`. It was correct only because every 3-child `AST_If` that is not a
ternary has `AST_BasicBlock` arms, which its own recursion refuses — an undocumented,
unasserted invariant that a future 3-child op with expression arms would have broken
silently, admitting a statement as a ternary. It now requires op 5 or op 7 explicitly.
Measured to change nothing: eligible 319 / 1126, 783 slices, 244 compared, 0 mismatches.

`slicerun frame --mutate` still exits non-zero, so the known-positive is still positive.

### Open, and not mine: `cli/perfn_inproc` is red on `main`

A full `ctest` run is 8934 passed / 1 failed, and the one failure reproduces on a clean
checkout of `main` with the working tree stashed, byte for byte: the cell expects
`-fopt-perfn-inproc` to change the object file when `-fopt-slice` is on (`DIFFER`) and
gets `SAME`. So either the flag no longer does anything under `-fopt-slice` at `-O3`, or
the cell's expectation is stale. Unrelated to this work, and left as found.

## Landed — host marshalling profiled, and the 207 ns lever found already spent, 2026-08-08

### The headline number in this file was 9x stale, and its name pointed at the wrong phase

This file said: *"the ~207 ns/lane is host-side marshalling — pack, copy into the
write-combined mapping, copy out, unpack. 207 ns to move 20 bytes is ~100 MB/s."* Both
halves are wrong today, and the second half was wrong when it was written.

Measured first, on a 3-node synthetic slice at 65,536 lanes, mean of 5 dispatches, debug
build, by bracketing each phase with `CLOCK_MONOTONIC` (throwaway instrumentation, not
committed):

| phase | before, ns/lane | share |
| --- | ---: | ---: |
| scratch sizing | 0.03 | 0.1% |
| pack int64 → int32 lo/hi into scratch | **7.48** | 22% |
| `memcpy` scratch → mapped input | 0.37 | 1.1% |
| command record + submit | 0.14 | 0.4% |
| `vkWaitForFences` | **16.25** | 48% |
| `memcpy` mapped output → scratch | 0.81 | 2.4% |
| unpack + narrow into the caller's arrays | **7.40** | 22% |
| **total** | **33.72** | |

So the whole per-lane cost was **33.7 ns, not 207**. Host marshalling was 16.1 of it
(48%), and the two phases the sentence names first — *"copy into the mapping, copy
out"* — are **1.18 ns/lane between them, 3.5%**. The cost was in the two pack/unpack
loops, which the sentence lists last.

### Where 207 actually came from: it is the ReBAR read-back, and it reproduces exactly

Forcing each memory type with the existing `MCC_GPU_MEMTYPE` override, same build, same
sweep (all ten rows, so these are not single samples):

| memory type | flags | host write in | host read out | fence wait | gpu ns/lane |
| --- | --- | ---: | ---: | ---: | ---: |
| 3 (today's pick) | `HOST_VISIBLE\|COHERENT\|CACHED`, sysmem | 0.4–1.1 | **0.4–1.0** | 16–100 | 19–110 |
| 2 | `HOST_VISIBLE\|COHERENT`, sysmem uncached | 0.6–1.7 | **19.8–21.8** | 14–142 | 25–112 |
| 4 | `DEVICE_LOCAL\|HOST_VISIBLE\|COHERENT`, ReBAR VRAM | 0.7–0.8 | **198–370** | **1.0–20.2** | **200.5–206.4** |

Type 4's rows read 206.4, 205.1, 204.8, 200.5 ns/lane. That is the 207. The archived
figure was measured against ReBAR, where the mapping really was uncached-across-PCIe and
reading it really did cost ~200 ns/lane — and the later `HOST_CACHED` fix already
collected almost all of that win. **The lever this file advertised as "the one nobody
costed" had in fact been spent by the memory-type change, and the sentence was never
re-measured.** Cost of not re-measuring: the break-even table quoted alongside it (451
lanes at 3 nodes) is not what the code does; it is 322–363.

### What changed: the pack loop was a `memcpy`, and the `memcpy` was unnecessary

On a little-endian host with 8-byte `int64_t`, an `int64_t[]` **is** the device's
`(lo, hi)` `int32` pair array, byte for byte. `mcc_slice_run_gpu` was reading the
caller's `int64` array, splitting each element into two `int32`s, writing them into a
malloc'd staging array, and then handing that array to a `memcpy` into the mapping — three
passes to produce bytes identical to the ones it started with. It now hands the caller's
buffer straight to the driver when `mcc_slice_lohi_native()` says the layouts coincide
(checked at runtime — byte-order probe, `sizeof`, and `MCC_GPU_IN_SLOTS == 2`; a
big-endian or padded host keeps the explicit loops).

`mcc_slice_run_frame_gpu` had the same shape on **both** sides — pack in, unpack out — plus
a `malloc`/`free` pair per dispatch. Both loops and one allocation are gone on the native
path; the device writes results back into the caller's `frames` array directly.

That leaves the pack/unpack loops as code no little-endian host ever runs, which is how
an arm rots. `MCC_SLICE_NO_LOHI=1` forces `mcc_slice_lohi_native()` to 0, and two new
cells — `slice/ops-lohi-fallback` and `slice/frame-lohi-fallback` — run the existing
clean-then-mutate driver with it set, so the fallback is exercised **and** proven
non-blind. Both were observed red before being believed: breaking the expression-path
pack (`>> 32` → `>> 31`) fails `ops-lohi-fallback` alone, and breaking the frame-path pack
(`v` → `v + 1`) fails `frame-lohi-fallback` alone, with every native-path cell still green
in both cases. The first break does *not* fail the frame cell, because that suite's slot
values are all below 2^31 and the two shifts agree there — worth knowing before trusting
either cell as a general guard.

Incidental, found while restructuring that function: its `malloc` failure path returned
without freeing `ob`. Now freed.

The unpack loop that remains recomputed `ast_eval_slice_is64(k->wtype)` and
`k->wtype & VT_UNSIGNED` **once per lane** for a value that cannot change inside the
dispatch. Hoisting them is 7.40 → 4.26 ns/lane in the debug build, i.e. 42% of that
phase was loop-invariant work.

After, same conditions:

| phase | before | after | release build, after |
| --- | ---: | ---: | ---: |
| pack | 7.48 | **0.00** | 0.00 |
| `memcpy` in | 0.37 | 0.55 | 0.78 |
| `memcpy` out | 0.81 | 0.92 | 1.27 |
| unpack | 7.40 | **4.26** | **0.98** |
| **host marshalling** | **16.06** | **5.73** | **3.03** |
| fence wait | 16.25 | 20.80 | 22.65 |

The release column is there because the debug build is `-O0` and the unpack figure is
mostly loop overhead a real build deletes anyway: **in an optimized build host
marshalling is ~1.6–3.0 ns/lane and everything else per-lane is the device.** Anyone
reading the debug profile as the production cost will over-weight the host by ~3x.

### Before/after, three interleaved runs of each binary

`slicerun --cost-synth`, medians of 3, debug, same machine, alternating binaries so drift
hits both:

| nodes | 3 | 7 | 15 | 31 | 63 | 127 | 255 | 511 | 1023 | 2047 |
|---|---|---|---|---|---|---|---|---|---|---|
| gpu ns/lane before | 22.72 | 33.93 | 103.65 | 88.14 | 61.29 | 88.10 | 100.86 | 58.73 | 70.44 | 59.15 |
| gpu ns/lane after | **16.78** | **25.10** | **93.62** | **84.04** | 64.00 | 84.55 | 87.62 | 60.88 | 103.60 | 54.35 |
| break-even before | 363 | 115 | 53 | 24 | 12 | 7 | 5 | 4 | 2 | 2 |
| break-even after | **322** | **108** | **48** | 24 | 23 | 8 | 7 | 5 | 3 | 2 |

Break-even is recomputed against the **pooled** CPU median at each node count, because
`cpu_ns_per_tuple` drifts ±8% run to run on a code path this change does not touch, and
the raw table's break-even column moves more from that drift than from the device column.

**Only the 3-, 7- and 15-node rows are signal** (−26%, −26%, −10%). From 63 nodes up the
device wait dominates and its run-to-run spread swamps a ~8 ns/lane host saving: for a
*fixed* binary the per-lane figure ranged 13.9–64.1 at 511 nodes and 68.6–113.6 at 1023
across three runs. Any single-run comparison at those sizes, in either direction, is
noise. The `gpu_fixed_ns` column is worse still — it is derived from a 64-lane
measurement — and this change does not touch it.

Real corpus (`tests/exec/statements/for.c`, one 3-node `nlive=1` slice), medians of 5
interleaved runs: **87.23 → 79.02 ns/lane (−9.4%)**. Smaller than the synthetic drop
because `nlive=1` halves the input bytes the pack was touching. Break-even on that row is
**not reportable**: `cpu_ns_per_tuple` came out 75.3 vs 99.3 for identical CPU code, so
`cpu − gpu` is inside its own error bar both before and after.

### Bought exactly zero

- **Skipping the padding-tail `memset`.** It never runs in the measurements it was
  supposed to help: `cap` rounds `ntuple` up to a multiple of 64, the sweep uses 64 and
  65,536, and both are already multiples of 64. Worst case is 63 lanes of a batch, so it
  cannot be a per-lane cost at all. Not implemented.
- **Avoiding re-allocate/re-map.** Already done by `mcc_vk_bind_buffers`, which only grows
  and only rewrites descriptors on growth. Measured at 0.01–0.08 ns/lane after the first
  dispatch. Nothing to win.
- **"The mapping is write-combined and the unpack reads it."** It is not. It is memory
  type 3, `HOST_CACHED`, and reads cost 0.4–1.0 ns/lane. The hypothesis was right about
  the mechanism and two memory-type generations out of date about the facts.
- **Splitting the memory type by direction** — input in ReBAR VRAM (host writes stream
  fine at 0.7 ns/lane) and output in host-cached sysmem (host reads at 0.4 ns/lane) — on
  the theory that the fence wait is the kernel reading system RAM. It is not: the wait
  came out **35–110 ns/lane against 16–110 for all-sysmem**, no better, and the whole
  difference is inside the run-to-run spread. Reverted.

### The next lever, and it is the device, not the host

With every buffer in VRAM the fence wait is **1.0–20.2 ns/lane**; with every buffer in
system memory it is **16–110**. The kernel is an order of magnitude faster on device-local
memory, and the only reason the code does not use it is that the host then pays 198–370
ns/lane to read the results back across PCIe. Both halves are avoidable at once by
`DEVICE_LOCAL` buffers plus a host-cached staging buffer and `vkCmdCopyBuffer` on the DMA
engine in each direction, which is how every other Vulkan compute application does this.

**This is not measured.** No staging path exists to measure, and the numbers above only
bound the two endpoints. Building it is a real change to the command recording — transfer
usage bits, two pipeline barriers, a second buffer pair — and it collides with the
`mcc_gpu_rw_back` read-back path, which reads binding 0 back into the caller and would
need the input buffer staged in both directions. Estimated payoff is deliberately not
stated here; the last three estimates in this file were wrong by 2–9x.

Also unresolved and cheap to check first: the wait for a 3-node kernel over 65,536 lanes
is ~1.1–1.5 ms on an RTX 5070 Ti. That is ~1.2 GB/s of effective traffic for a perfectly
coalesced access pattern, and it is not obviously all PCIe — `nvidia-smi` sampled during
the sweep shows the memory clock flipping between 810 MHz and 11,001 MHz, which would
explain the bimodal ~60/~95 ns/lane the table keeps producing at fixed node counts.

### Darwin: the reported break was real

`mcc_gpu_dispatch_rw2` and `mcc_gpu_mem` were defined **after** `#endif /* MCC_GPU_LANG_MSL */`,
in code common to both backends, while referencing `mcc_gpu_rw_back`, `mcc_vkr`,
`mcc_vk_resident`, `mcc_vk_bind_mem` and `MCC_VK_MEM_DEFAULT` — every one of which is
defined only inside the `!MCC_GPU_LANG_MSL` arm. Verified by symbol, not by eye: after the
fix, the region below the `#endif` contains zero occurrences of `mcc_vk*`, `Vk*`, `vkCmd*`,
`VK_*` or `MCC_VK_*`.

The shared locking and `mcc_gpu_closing` handling stay in one place; the three
backend-specific pieces become `mcc_gpu_rw_supported`, `mcc_gpu_rw_arm` and
`mcc_gpu_mem_backend`, defined in both arms. The Metal arm returns **failure**, not a
fake success: it has no resident buffer set, so there is nothing to read back into `inout`
and no shared address space to hand out, and a caller that gets 0 falls back to the CPU
oracle exactly as it does on a host with no driver. (The Metal `mcc_gpu_dispatch_locked`
also `memcpy`s unconditionally into `out`, which `mcc_gpu_dispatch_rw` passes as NULL —
that path is now unreachable rather than a null dereference.)

**Unverified by execution.** There is no Darwin host here and no way to compile the Metal
arm; the claim rests on the symbol audit above and on the Linux build and `slice/*` +
`gpu/*` suites staying green.

## Landed — three more blind differentials, the sub-word atomicity decision, 2026-08-08

### Three cells that could not fail now can, and one that was lying about why

The rule from last session — *a cell that consumes a subprocess must assert on its exit
status, and a cell that compares two things must assert that it compared something* — had
three suites still outside it. Re-measured on this machine at the merge base, clean run
then `--mutate`, exit code and the runner's own tally:

| suite | clean | `--mutate` before | `--mutate` after | known-positive arm |
| --- | --- | --- | --- | --- |
| `wide64` | rc 0, 154/0 | rc 1, 154 checks 67 fail | unchanged | already had one |
| `ops` | rc 0, 162/0 | rc 1, 162/52 | unchanged | already had one |
| `frame` | rc 0, 157/0 | rc 1, 157/14 | unchanged | already had one |
| `gpu` | rc 0, 32/0 | rc 1, 32/11 | unchanged | **added** |
| `sched` | rc 0, 15/0 | rc 1, 15/5 | unchanged | **added** |
| `bytes` | rc 0, 27/0 | **rc 0, 27/0** | rc 1, 39 checks 8 fail | **added** |
| `mem` | rc 0, 7/0 | rc 0, 7/0 | rc 0, 7/0 | exempt, see below |

`gpu` and `sched` were free: both were already mutation-sensitive and simply had no arm
registered. `bytes` was not, and the reason is worth recording — `--mutate` is injected at
the four sites inside `mcc_slice_kernel_build` / `mcc_slice_frame_kernel_build`, and
`bytes_kernel` hand-rolls its own module, so the perturbation never reached it. It now
carries the same `lo ^ 1` before the store that the other four sites carry.

Observed red, which is the only evidence that counts here. Forcing `mcc_slice_set_mutate`
to ignore its argument and rebuilding turns **all six** arms red at once, each with the
driver's own diagnosis — `wide64`, `ops`, `frame`, `gpu`, `sched`, `bytes`, 6 of 7 cells
failed, every one of them saying *"every ... kernel was perturbed and the differential
still reported clean, so it is blind"*. Restored, all six pass, and the whole
`^slice/|^gpu/|must-run` selection is 26/26.

### The device-less arm was reporting blindness when it meant absence

Found while adding the two new arms, and it was already live. `slicerun` only exits 77 for
`gpu`, `wide64`, `ops`, `fault` and `mem`; `frame`, `sched` and `bytes` run their CPU half
and exit **0** with no device. Measured with `MCC_VULKAN_LIB=/nonexistent`: all three
returned rc 0 for *both* arms. So on any host without a device the driver reached its last
branch and issued `FATAL_ERROR ... so it is blind` — about a host that had never dispatched
anything. `slice/frame-known-positive` has been failing that way, with that message, on
every device-less host since it was registered.

The fix is a `--device-or-skip` flag: the runner exits 77 before running anything if no
device came up, and the driver asks for it on both arms. Measured after: `frame`, `sched`
and `bytes` all report *"no usable device, skipping"* at rc 0 device-less, and with
`MCC_GPU_REQUIRED=ON` the same run is *"no usable device, but MCC_GPU_REQUIRED is set"* at
rc 1. The verdict is now about the differential rather than about the host.

`tests/must-run.txt` gains the three arms as `registered` rows, matching every other arm,
which inherits its parent's skip. `must-run: 26 row(s) satisfied`.

### `slice/mem` is exempt from a known-positive, and that is the honest answer

`suite_mem` dispatches no kernel of its own. Its seven checks are host-side properties of
the mapped region — mappability, a non-NULL host pointer, a ≥1 MiB extent, offset 0 zeroed,
region identity across a second `mcc_gpu_mem` call, and host-write persistence. There is no
computation for a mutation to perturb and no comparison for it to blind, so an arm that
perturbed every kernel and then asserted the region is still mappable would be theatre: it
would pass for reasons unrelated to what it claims to prove.

What stands in for it is that `mem` already exits 77 without a device (measured), so it
cannot green-via-silent-noop the way `frame`/`sched`/`bytes` could. If `mem` ever grows a
device kernel that writes into the region and a host check on what it wrote, that check is
a differential and this exemption should be deleted rather than defended.

### Open item 0 decided — the allocator never co-locates two lanes' sub-word objects

A shared-region sub-word store is a read-modify-write: load the containing word, mask,
insert, store the word back. Two lanes writing different bytes of one word therefore lose
one of the two writes. Decided now, before the allocator exists, on the same grounds the
NULL reservation was: it costs nothing today and cannot be retrofitted cheaply.

The three candidates, and why the third wins:

1. **Byte-granular atomics.** Not available. SPIR-V's `OpAtomic*` operate on 32-bit
   integers under `Shader` (64-bit under a further capability); there is no 8-bit atomic,
   and MSL's atomics are 32/64-bit too. Measured here: `OpAtomic` does not appear anywhere
   in `src/mccgpu.h`. This option is off the table outright, not merely expensive.

2. **`StorageBuffer8BitAccess`.** Technically correct where present — distinct bytes are
   distinct memory locations, so a byte-granular store is not an RMW and there is no race
   to lose. Measured on this machine, RTX 5070 Ti Laptop GPU, `apiVersion 1.4.329`, driver
   NVIDIA 595.84: `VK_KHR_8bit_storage` present, `storageBuffer8BitAccess`,
   `uniformAndStorageBuffer8BitAccess`, `storagePushConstant8` and `shaderInt8` all true.
   That is one device, and it is the wrong question for a portable compiler. The right one
   is what the repo can reach: `mcc_gpu_vk_init` requests `ai.apiVersion =
   VK_API_VERSION_1_1`, and `vkCreateDevice` is handed a `memset`-zeroed
   `VkDeviceCreateInfo` — zero device extensions, zero feature structs chained on `pNext`.
   The feature exists on the hardware and is unusable by this process. Adopting it means an
   API-version bump, a per-device feature query, a `pNext` chain at device creation, an
   `OpCapability StorageBuffer8BitAccess` variant of the emitter, **and** the shift/mask
   fallback anyway for every device that answers false. Two emitters and a runtime branch,
   to buy something option 3 gives for nothing. (The promotion history — core in Vulkan 1.2
   as `VkPhysicalDeviceVulkan12Features::storageBuffer8BitAccess`, optional there — is from
   knowledge, not measured here. The decision does not turn on it: the repo cannot reach
   the feature at any version today.)

3. **The allocator never co-locates two lanes' sub-word objects in one word.** Chosen.
   It works on every device, needs no feature query, no fallback path and no second
   emitter, and it makes the racy case *unrepresentable* rather than merely unlikely.

**Consequences for the future allocator, which is what this decision is for.** In a region
more than one lane can reach, a 4-byte word has exactly one owning lane. Concretely: the
allocation granularity and the alignment for any object reachable by more than one lane are
both 4 bytes, so a `char` or `short` that two lanes might write costs a full word. Lane-
private regions are unaffected and keep packing at natural alignment — the frame path
already relies on that and does not change. The cost is padding in a heap that does not
exist yet; the alternative is a data race that no differential can see, because both
executors would agree on whichever write happened to survive.

**And it has teeth today.** `SpvRegion` gains a `shared` flag with an
`spv_region_shared(var, base, nbyte)` constructor beside the existing `spv_region`, whose
signature is unchanged so the binding-2 deref work in flight is not disturbed.
`spv_store_region` refuses outright — `m->failed = 1`, no instruction emitted — when the
region is shared and the width is under 4, so the kernel fails to build rather than
emitting a store that races. Loads are untouched: a narrow read is a plain read and is not
an RMW. `suite_bytes` asserts it in twelve new checks that need no device: the four
sub-word types are refused in a shared region, the same four still emit in a lane-private
one, and the four word-or-wider types are admitted in a shared region. Observed red by
deleting the refusal and rebuilding — `slicerun bytes` went to rc 1, 39 checks, 4 failures,
all four naming the sub-word store. Restored, rc 0, 39/0.

### CI does not set `MCC_GPU_REQUIRED` on any Linux or Windows job

Reported because it is a real gap, not because it was fixed here. `MCC_GPU_REQUIRED` does
not appear anywhere under `.github/`. Its one CI use is the `gpu-vulkan` feature row in
`tools/ci.c`, `-DMCC_GPU_BACKEND=vulkan;-DMCC_GPU_REQUIRED=ON`, and that row is gated
`OS_MAC` — the comment there says Darwin is the only host where the backend choice is worth
a cell, which is true of the *backend* and says nothing about the *requirement*. So every
Linux and Windows ctest job configures with the default `OFF`, and all six device cells go
green-via-skip if the ICD is missing. `tools/must-run.py --results` would catch it, but only
for `must-run` rows, and every device cell is `registered`.

So "the device was present at configure time but skipped at run time" is not detectable
today on the hosts that actually run the device cells. The cheap fix is one flag on the
Linux job that has a working ICD, not new harness machinery; it is left for whoever owns
the workflow matrix, since choosing which runners are guaranteed to have a device is a CI
decision and not a compiler one.

## Landed — binding 2 reaches the device, and `*p` is measured rather than assumed, 2026-08-08

### What landed

Two things, both neutral on the corpus and both proven able to fail.

1. **The shader declares binding 2.** `SpvMod` gains `id_mem`; `spv_module_begin` emits a
   third `OpVariable` of the existing `id_ptr_buf` type and decorates it `DescriptorSet 0`
   / `Binding 2`. The host side already had all of it — the 3-entry descriptor set layout,
   `bmem`/`mmem`/`pmem`, `dbi[2]`, and `mcc_gpu_mem` — so this is the last missing half.
   `spirv-val` accepts 7 of 7 modules: one that declares the variable and never uses it
   (the shape every existing kernel now has), and six deref kernels at widths 1, 1u, 2, 4,
   8 and 8u. All 22 pre-existing `slice/*` and `gpu/*` cells stayed green across the change.

2. **`slice/deref` and `slice/deref-known-positive`**, 43 checks. A hand-rolled kernel
   builds a `SpvRegion` over `id_mem` and loads and stores through it. Nothing in
   `spv_load_region` / `spv_store_region` needed changing: a region is (var, base, nbyte),
   and pointing `var` at binding 2 instead of binding 0 is the whole edit. The host seeds
   the region through `mcc_gpu_mem` before submit and reads it after the fence, which is
   the only coherence the mapping offers.

### Sub-word atomicity: option (a), lane-private sub-regions

Explicitly chosen and explicitly narrow. Each lane owns a **disjoint 64-byte, 16-whole-word**
sub-region of binding 2 at byte offset 4096. No two lanes share a word, so the
read-modify-write that a 1- or 2-byte store compiles to cannot race, and every width is
exercised including the sub-word ones that are the entire reason the region layer exists.

The shared-heap case — two lanes writing different bytes of one word — is **not tested and
is not claimed to work**. It is still open decision item 0. Confining the first increment
this way was preferred over restricting to 4- and 8-byte access, because restricting the
width would have left the RMW path unexercised on binding 2, and that path is the one with
the hazard in it.

That the confinement is load-bearing is measured, not asserted: collapsing the per-lane
base to a single shared region makes **14 of 43 checks fail**.

### Proving the cell can fail

Three separate perturbations, each observed red, then reverted:

| perturbation | result |
| --- | ---: |
| `--mutate` (the kernel returns one bit wrong) | **28 of 43 fail** |
| one expected `def` verdict flipped in the directed table | **1 fails** |
| lane-private region base collapsed to a shared one | **14 fail** |

The mutation had to be injected by hand. `mcc_slice_frame_kernel_build`'s perturbation does
not reach a hand-rolled module, which is exactly why `slice/bytes` and `slice/mem` are not
mutation-sensitive today; without the injection the known-positive arm would have passed
vacuously.

Because the same author wrote both the device path and the reference, the differential
alone would not have seen a shared mistake. So 16 lanes across 6 type-shapes assert
**hand-computed constants** — value, definedness and every one of the 16 region words —
worked out from the seed pattern rather than from the CPU reference. Sign extension is
separated from zero extension deliberately (`0x83828180` seeds, so byte *b* reads `0x80+b`):
signed byte 0 is −128 and unsigned byte 0 is 128, and a load that got the extension wrong
on both executors would still fail here. Under `--mutate` all 6 directed value checks and
all 6 directed memory checks fail.

The table was then checked against a third, fully independent oracle: a small C program
compiled by the host compiler that builds the same 64-byte buffer and performs the same
loads and stores through real pointers. All **12 in-range directed lanes agree exactly** —
value and every changed word — across the device, the hand-written table and host C. The
out-of-range lanes have no C analogue, since they are the cases C leaves undefined and the
region layer answers with select-to-0; those are checked against the rule alone.

### What this did NOT move, and the number is 0

Measured on one dump of `vendor/musl-src/src/string` at `-O1` (83 bodies), before and after:

| | before | after |
| --- | ---: | ---: |
| corpus `frame-compared` | 94 | **94** |
| `memcmp` / `strcmp` / `strncmp` / `memchr` / `strspn` `frame-compared` | 0 | **0** |

Nothing new lowers. Step 3 — admitting the `*p` shape into `mcc_slice_frame_stmt_ok` — was
not attempted, and the measurement below is why that is a decision rather than a shortfall.

### `*p` is `Load(Ref[LOCAL|LVAL])`, and pointer `++` is the co-requisite

Two corrections to the premise, both measured over the same corpus.

**The shape is a single `Load`, not a nested one.** `*l` in `memcmp` is
`Load(Ref[op=0x132, ival=−32, t=VT_PTR])` — one `Load` over a pointer-typed local Ref.
This matches the store-destination table already on the board (`Load(Ref[LOCAL|LVAL])`,
45 nodes); a nested `Load(Load(...))` does not occur at all. Counted over the corpus:
**159** `Load(Ref[LOCAL, VT_PTR])` sites, and **all 159 carry Load type 0**. So every one is
refused today by `ast_eval_slice_intt(0) == 0`, and none is silently misread as a read of
the pointer variable itself — which is what would happen if any of them ever carried a
type, because the `frame_off` branch would swallow it first. Worth a guard whenever this
is opened up.

**The width is already available.** `ast_adump_etype` returns the *pointed-to* type for any
`VT_PTR` node, so the 14th column gives the access width for all 159 sites with no new
plumbing: `memcmp`'s `l` reads `0x261` = `const unsigned char`. The 13th/14th column work
done for `arr[i]` covers `*p` too.

**But `*p` alone cannot move the headline four off zero.** Pointer increment is refused by
the same predicate (`mcc_slice_frame_stmt_ok` excludes `VT_PTR` from `++`/`--` because it
scales by element size), and every one of the named functions needs it:

| | pointer `++`/`--` nodes |
| --- | ---: |
| `memcmp` | 2 |
| `strcmp` | 2 |
| `strncmp` | 2 |
| `memchr` | 3 |

Corpus-wide: **440** BasicBlocks, **63** contain a `*p` load, **41 of those 63 also contain
a pointer `++`/`--`** and stay refused regardless, leaving **22** as the *upper bound* on
what admitting `*p` alone could add — an upper bound, since Invoke and the other blockers
still apply to some of them. There are **169** pointer `++`/`--` nodes and **21**
`Store(*p, v)` nodes in the corpus.

So `*p` and pointer arithmetic are one item, not two, and doing the first without the second
buys at most 22 blocks and exactly 0 of the functions the item was justified by.

### The open question that has to be answered first

What a pointer *value* means. The region layer is ready and the widths are known, but a
frame slot holding a pointer holds a **host address**, not a binding-2 byte offset, and
nothing yet maps one to the other. Seeding slots with synthetic values and calling them
offsets would make both executors agree — they would agree on the range verdict, the loaded
bytes and the stored bytes — while lowering nothing anybody could actually run. That is
agreement without correctness, and it would move `frame-compared` while moving no real
work, which is the specific overstatement this file exists to prevent.

SUPERSEDED 2026-08-08 by "Landed — `*p`, pointer `++`/`--`, and a host address that
lowers". The pointer-value question was answered with a third option neither candidate
here anticipated: keep the host address and convert it at the point of use, because
binding 2 is permanently mapped and both executors can therefore address the same bytes.
The four functions are no longer 0.

### A latent stale-descriptor bug, for whoever grows binding 2 first

Not hit today and not fixed here, because nothing yet calls it the wrong way — but growing
binding 2 is the first thing a heap needs, so it will be hit. `mcc_vk_bind_mem(want)`
destroys and recreates `bmem` when `want > bmemsz`, and it does **not** rewrite the
descriptor set. The only thing that writes descriptors is `mcc_vk_bind_buffers`, which
returns early unless `grew` is set — and it sets `grew` for `bmem` only on the
`!mcc_vkr.bmem` first-creation path. So a direct `mcc_vk_bind_mem(bigger)` leaves
descriptor 2 pointing at a destroyed buffer. Today both callers pass
`MCC_VK_MEM_DEFAULT`, so the size never grows and the path is unreachable. Either set
`grew` on any recreation, or rewrite the descriptor inside `mcc_vk_bind_mem`.

## Landed — B1 runtime addressing, and a per-width region layer, 2026-08-08

### The payoff is 19 blocks, not 41, and the difference is not effort

Measured before building, the same way the last three items were, and then measured
again after: frame slices **300 → 319 (+19, +6.3%)**, statements **182 → 201 (+10.4%)**,
0 mismatches. The model that produced the prediction reproduced today's numbers exactly
(947 blocks, 300 eligible, 182 statements) before it was trusted for tomorrow's, which is
the only reason to believe it.

The 41-block estimate counted address expressions, not blocks that only those expressions
block. Classifying all 143 non-plain store destinations in the corpus:

| destination | nodes | frame-addressable? |
| --- | ---: | --- |
| `Ref[SYM\|LVAL]` — a global | 46 | no: a host address, and per-lane globals is N14 |
| `Load(Ref[LOCAL\|LVAL])` — `*p` | 45 | no: the local *holds* a host pointer |
| **`Load(Binary('+')(Ref[LOCAL\|ARRAY], i))` — `arr[i]`** | **20** | **yes — this is the whole payoff** |
| `Unary(ARROW)(...)` — `p->f` | 8 | no, and never can (replay does `indir()` first) |
| `Load(Convert(...ptr...))` — `*(p+K)` | 13 | no |
| other | 11 | no |

Split by where the index appears: `arr[i]` in **store** position is +18 blocks, in **load**
position +1, both +19. Constant-index `arr[K]` is **+0**, and folding a store destination
through `.field`/`&` is **+0** — the fourth and fifth zero-payoff results in a row, both
recorded rather than quietly dropped.

### The 13th column was not the prerequisite. The 14th is

The brief said the extent column had unblocked this. It had not, and the reason is worth
writing down because it was only visible by dumping a directed case:

- `arr[i]` replays as `gen_op('+')` on a `VT_PTR|VT_ARRAY` base, which scales `i` by the
  **element** size. Verified: `int arr[4]` at −24 with `arr[2]` emits `Lit(2)`, not
  `Lit(8)`.
- The `Load` and the `Binary` above it carry **type 0 in every real arena** — 5 of 5 in a
  directed case, all 143 in the corpus. So the tree records neither the access width nor
  anything to derive it from.

The extent therefore gives no element count to bound `i` against and no width to narrow a
stored value to. A consumer holding only the extent must guess both, and a guessed width
is a wrong answer that *both* executors would agree on — precisely what a differential
cannot see. So `ast_adump_etype` adds a 14th column: the pointee type word, computed from
the real `CType` before interning, exactly as the extent is. Verified `int arr[4]` → 3
(`VT_INT`), `char buf[10]` → 1 (`VT_BYTE`), `long big[3]` → 0x1004 (`VT_LLONG|VT_LONG`).
Older dumps read it as 0 = unknown and refuse. `spvgate` is unaffected (its `sscanf`
reads 7 fields).

### And the const-offset fold that "was worth zero blocks" was also wrong

`ast_eval_slice_frame_off` folded `base + K` as a **byte** offset. For a pointer or array
base that is the wrong address by a factor of the element size. It gained zero blocks and
so was never caught by the block count; it could not be caught by the differential either,
because both executors used the same wrong key and therefore agreed. It now refuses a
pointer/array base outright and leaves that shape to the code that knows the element size.
Corpus is unchanged at 783 expression slices / 0 mismatches, confirming nothing depended
on it.

### One slot per element, not a byte-addressed frame — and why that was the better trade

The plan called for a byte-addressed frame, which forces per-width store/load because a
32-bit local at −12 and one at −8 are adjacent words and `spv_store_live_v`'s two-word
store clobbers the neighbour. Instead each admitted array gets a **contiguous run of
ordinary 8-byte slots, one per element**, addressed as `slot_of(base) + index`. Slots stay
disjoint, so the proven two-word store is reused verbatim: no per-width work is needed on
this path at all, and the 300 previously-clean slices cannot regress. Contiguity is
enforced rather than assumed — a run that would interleave with an already-mapped offset
is refused.

Raising `MCC_SLICE_MAXSLOT` from 16 to 32 or 64 was measured and changes nothing
(319 either way), so slot capacity is not what binds.

### Bounds safety, by construction, on both executors

`ok = idx <u nelem` ; `def &= ok` ; `idx &= (nspan − 1)`, where `nspan` is the element
count rounded up to a power of two and the object owns that many slots. The mask is
relative to **the object's own run**, not the lane's region, so the worst an out-of-range
access can do is touch another element of the same array — and the run is discarded
anyway because the same comparison cleared `def`.

The CPU reference does not refuse. It reaches the identical verdict *and writes the
identical masked element*, because the device cannot refuse mid-kernel and a reference
that bailed would disagree with it on every frame slot as well as on the flag. `def` is
therefore compared for runs that carry a runtime index even when they have no `Return`,
which is new: for those runs the out-slot flag is a real verdict rather than the dummy
the comment used to call it.

### The region layer — per-width, and parameterised rather than frame-local

Added because a shared heap has adjacent objects of different widths **by construction**,
so the two-word hazard is the normal case there rather than an edge case. `SpvRegion` is
`(variable, base word, byte extent)` and `spv_load_region`/`spv_store_region` take
`(region, byte offset, type)`. Nothing in them knows whether the region is one lane's
frame or a buffer shared with the host; that is entirely which base is passed, so a second
binding needs a different `SpvRegion` and no new emitter code.

- 1 and 2-byte accesses are shift/mask, and a sub-word **store** is a read-modify-write of
  the containing word, because SPIR-V has no 8-bit storage without
  `StorageBuffer8BitAccess`. **Within one lane's region that is a narrow store; in a
  region shared between lanes it is not atomic**, and two lanes writing different bytes of
  one word would race. That is a property of who hands out shared addresses, and it is
  unresolved.
- Bounds use a **select, not the planned power-of-two mask**: `ok = off <=u nbyte − width
  && aligned && nbyte >=u width`, then `off = ok ? off : 0`. The mask needs the region
  padded to a power of two, and an unpadded 48-byte region would let a masked offset reach
  byte 63 — into whatever follows. Corrupting a neighbour is strictly worse than
  corrupting yourself; 0 is in range for every region that can hold the access at all, so
  the select cannot leave the region and has no padding precondition.

`slice/bytes` is a direct differential over 8 type/signedness combinations × 14 offsets
(aligned, last legal, one past, misaligned, wild, negative), comparing every word of every
lane's region and the verdict flag — 27 checks. **It was proved able to fail**: adding
back the two-word store for a 4-byte type made it red immediately with
`load=0 store=7 word 1 want=00000007 got=00000000`, i.e. exactly the neighbour clobber the
layer exists to prevent.

### Numbers

| | before | after |
| --- | ---: | ---: |
| corpus frame slices | 300 | **319** |
| corpus frame statements | 182 | **201** |
| corpus frame mismatches | 0 | **0** |
| corpus expression slices / mismatches | 783 / 0 | 783 / 0 |
| `--mutate` frame mismatches (known-positive) | 40+ | **63** |
| `--mutate` slice mismatches | — | 6149 |
| `slice/frame` checks | 122 | **157** |
| `slice/bytes` checks | — | **27** |
| ctest | 8932, 1 red | **8933, 1 red** (`cli/perfn_inproc`, red on purpose) |
| `VUID`/validation errors, `frame` and `bytes` | 0 | **0** |

`rir-coverage` needed no re-banking: `nodes_pct` at `-O1` is **41.420%** against a banked
floor of 41.4175%, and the absolute count rose too — 433,646 arena nodes at 41.420% is
~179,616 lowerable against ~178,419 before.

### Open after this

1. **A sub-word store to a region shared between lanes is not atomic.** Read-modify-write
   of the containing word is correct for a private region and racy for a shared one. Needs
   either byte-granular atomics, `StorageBuffer8BitAccess` where available, or an
   allocator that never puts two lanes' sub-word objects in one word.
2. **`*p`, `p->f` and globals are still refused** — 99 of the 143 non-local store
   destinations. They are host addresses, and they are exactly what the mapped-buffer
   address space is for; they are not frame work and no widening of the frame reaches
   them.
3. **The Metal arm returns 0 for every frame kernel and for the region layer.** Declared,
   not silently divergent, but the divergence is now larger than it was.
4. **A 64-bit index is refused rather than approximated** — every index in the corpus is a
   plain `int`, and a wide one would have to agree bit for bit between a host `int64` and
   a device lo/hi pair before the mask applies.

## Landed — the three runners exist and are proven by test, 2026-08-08

The JIT/GPU/coroutine runners are now the top of the board, and the first rung is
built. Two new header-only libraries and one new test tool, seven new ctest cells,
all green, and the device path is covered by a known-positive so it cannot pass
vacuously.

| file | what it is |
| --- | --- |
| `src/mcctask.h` | the tick task and its single-threaded scheduler — `MccTask`, `MccSched`, `mcc_sched_step/run/quit/pending`. No threads, no `ucontext`, no stack per task |
| `src/mccslice.h` | the work item and both executors — `MccSliceWork`, `mcc_slice_work_from_ast`, `mcc_slice_run_cpu`, `MccSliceKernel`, `mcc_slice_kernel_build/run_gpu/free`, and `mcc_slice_task_cpu/gpu` which wrap either executor as a schedulable tick |
| `tools/slicerun.c` | the suites, plus an `--arenas` mode that turns real recorded bodies into work items |
| `cmake/slicerun_real.cmake` | the real-corpus cell, with four teeth |
| `cmake/slicerun_mutate.cmake` | the 64-bit known-positive |

**Cells:** `slice/task` (15 checks), `slice/work` (11), `slice/cpu` (29), `slice/gpu`
(32), `slice/sched` (15), `slice/wide64` (143), `slice/wide64-known-positive`,
`slice/real`. **245 checks, 0 failures.** `slice/gpu` and `slice/wide64` carry
`SKIP_RETURN_CODE 77`.
`slicerun` dispatches through `src/mccgpu.c` rather than bringing its own Vulkan the
way `spvgate` does, so it needs no SDK, builds on every host, and is **the first cell
that covers the production device layer's dispatch path directly** — `docs/PLAN.md`
records that path as having zero direct coverage.

**What the tests prove, in the order they prove it.**

1. **A tick is a real suspension point.** A task yielding three times is ticked exactly
   four times; two tasks interleave `1,2,1,2,1,2` rather than draining one first; a
   two-round budget leaves both pending with exact partial state; `mcc_sched_quit` is
   observed *between* ticks, so a stopped scheduler runs nothing and resuming drains
   the queue to the same final values. **That is L2′ in miniature** — the quit flag the
   JIT pool cannot have while a worker holds `mccjit_swap_lock` across an opaque
   `job->run(job)`.
2. **The AST can hand out work.** `mcc_slice_work_from_ast` turns a subtree into
   `(root, live-in offsets, node count, result width)`, with the offsets in
   first-encounter order because that ordering *is* the kernel ABI. Slices containing a
   store, containing a float, or carrying more live-ins than the ABI holds are refused.
3. **The CPU runner returns the expected values, and the answer does not depend on how
   the work was carved up.** A five-tuple batch of `3*x0 + x1` gives `5, 22, -5, 0, 299`
   in one tick and the identical five values one tuple per tick. Division by zero comes
   back defined=0 rather than trapping, so an undefined lane is distinguishable from a
   lane that legitimately produced zero.
4. **The device runner agrees, lane for lane.** Same work item, same tuples, compared
   against the CPU runner on both value and definedness, with `dispatches > 0` asserted
   so "no device" cannot masquerade as "all green". A second run of a built kernel does
   not re-emit — S6's amortization, asserted rather than assumed.
5. **Both runners schedule together.** A CPU task at a budget of two and a device task
   in one `MccSched`, drained by the same loop, producing identical output; the CPU task
   takes exactly three ticks for five tuples at a budget of two.
6. **Real bodies, not synthetic ones.** `slice/real` compiles 60 files of `tests/exec`
   under `MCC_ARENA_DUMP`, rebuilds every recorded arena, and runs every lowerable
   subtree through both executors: **344 bodies → 177 work items → 1416 tuples → 0
   mismatches**, 172 of the 177 also lowering to the device. The mutated build produces
   1370 mismatches, so the differential is not blind.

### The one real bug the real corpus found: inferred result widths are not schedulable

The first real-corpus run was **43 mismatches in 3200 tuples**, all of the same shape —
the device's low word correct and its high word carrying a carry bit where the sign
should be (`cpu=-4` against `gpu=8589934588` = `0x1_FFFFFFFC`).

It is not a device bug and not an emitter bug. `ast_eval_slice_wtype` falls back to a
*child's* type when a node is itself untyped, and the two executors then disagree about
what the fallback meant: the CPU evaluator widens the whole expression to the inferred
width, while the emitter widens each node by its own declared type. A 32-bit add whose
result is inferred 64-bit therefore comes back with the carry in the high word.

The ladder already knows about this hazard — it is what `MCC_AST_EVAL_LADDER_STRICT_TYPE`
and the `res->inferred` counter exist for. **But an oracle and a runner want opposite
defaults.** An oracle comparing two expressions can tolerate a shared approximation,
because both sides get it equally wrong. A runner cannot: the two sides are different
implementations, and an inferred width is precisely where they diverge.

**Rule, now enforced in `mcc_slice_work_from_ast`: a slice whose result width has to be
guessed is not schedulable work.** The root's own declared type must be a usable integer
type. With that rule the same corpus gives 0 mismatches. Two consequences worth carrying
into the plan:

- **It costs eligibility, and the cost is not yet measured.** Strict typing rejects
  slices the emitter would happily lower. How many is a number the H6 table should
  carry, alongside the `MCC_RIR_STAMP=2` question — the stamped type view is documented
  to take typed-node coverage from 65.8% to 100.0% for byte-identical objects, so most
  of this loss may be recoverable rather than inherent. **Setting `MCC_RIR_STAMP=2` on
  the dump did not change the mismatch count**, so the stamping that fixes the *ladder's*
  readback is not the same thing as the declared type this rule needs; that gap is
  unresolved and is the first thing to measure.
- **A second finding, smaller but sharp: the device out-slot ABI is not
  self-describing.** `mcc_gpu_dispatch` returns a raw `{lo, hi, defined}` triple, and
  the high word is meaningful only for a 64-bit result — otherwise it is whatever the
  emitter left there. The caller must narrow to the slice's own result type, which is
  why `MccSliceKernel` carries `wtype`. Skipping that fit reads a correct 32-bit answer
  back as a wrong 64-bit one, and it was the first of the two bugs in this run.

### 64-bit fidelity, verified at full width — and the SIGFPE it found

`slice/wide64` (143 checks) and `slice/wide64-known-positive`. **143 checks, 0 failures,
31 dispatches** — one per conversion case plus one per binary op, so every case really
ran rather than being skipped. The mutated build produces 63 failures.

**Why this needed doing at all.** The emitters have supported 64-bit since `989e4b3b`,
as a `uint2` lo/hi pair with no `Int64` capability declared, and `spvgate` covers it with
20 `ll-` cases. But every 64-bit value those cases ever see is built by `mk_up` — a
1..16-bit rung value shifted up by a constant — so **the high word only ever holds the
patterns that one construction happens to produce.** Full-width values had never been fed
to the 64-bit path. The values that matter for pair emulation are exactly the ones that
construction cannot make: the 2^32 carry boundary in both directions, a low word of all
ones against a clear high word, `INT64_MIN`, `INT64_MAX`, and patterns whose halves have
nothing to do with each other.

**What is now covered.** A 16-value hard corpus crossed with itself, 256 tuples per op,
device against CPU on both value and definedness, with each case asserting it compared at
least one *defined* tuple so an all-undefined case cannot report clean:

- **Identity round trip** — a bare 64-bit live-in straight back out, expected value = the
  input, all 64 bits. This needs no oracle and isolates the lo/hi packing from arithmetic.
- **Widening** — `int32 → int64` sign-extends (`-1` arrives as `-1`, not `4294967295`).
- **Narrowing** — `int64 → int32` keeps the low word and re-signs it.
- **Signed**: `+ - * / % & | ^`, `< <= > >= == !=`.
- **Unsigned**: `+ - * / %`, `TOK_ULT/UGE/ULE/UGT`.
- **Shifts**: `SHL`/`SAR` signed and `SHL`/`SHR` unsigned at counts `{0, 1, 31, 32, 33,
  63}` — 32 is where a naive pair shift breaks, and a hard value used as a shift count is
  almost always out of range, so the counts are a separate list rather than the corpus.

**The bug: `ast_eval_slice.h:117` aborts the compiler on `-1 * INT64_MIN`, on x86 only.**

```c
r = (int64_t)(ua * ub);
if (r / a != b || (a == INT64_MIN && b == -1) || (b == INT64_MIN && a == -1))
```

The two guards are correct and they are in the wrong place. `||` short-circuits left to
right, so `r / a` is evaluated *first*; with `a == -1` and `b == INT64_MIN` the product
wraps back to `INT64_MIN` and `INT64_MIN / -1` traps. **SIGFPE, in shipped compiler code**
— `ast_eval_slice.h` is compiled into `mcc` and this is the evaluator the width ladder
runs, so any slice multiplying `-1` by `INT64_MIN` aborts the process.

**It is also a J1 host divergence, which is the more interesting half.** arm64's `sdiv`
does not trap on `INT64_MIN / -1`; it yields `INT64_MIN`, the first clause reads
`INT64_MIN != INT64_MIN` = false, and the third guard then correctly returns 0. So the
same slice **refuses cleanly on arm64 and kills the compiler on x86** — a divergence that
no existing cell could see, because nothing had ever fed the evaluator a full-width
`INT64_MIN`.

Fix is a reorder: hoist the two guards ahead of the division. Verified byte-neutral —
**114 objects across `tests/exec` are identical before and after**, which is expected,
since the only reachable change is crash → refusal. Nothing that previously returned a
value returns a different one.

### H6 is measured, L3 residency landed, and S5's break-even estimate was 2 orders out

**Phase 0a is done.** Both executors are timed — the device figure spans pack, dispatch,
readback and unpack, because that is what a caller pays. `slicerun --cost` emits the
table over real arenas; `slicerun --cost-synth` sweeps synthetic slices, and that sweep
is the `slice/cost` cell, ratcheted on `win-within-N-lanes` being nonzero so the
measurement cannot silently stop measuring.

**First table said "never" for every row — and it was measuring an artifact.** Fixed cost
came out at **~640 µs/dispatch**, five times the plan's 117 µs figure, because every
dispatch still rebuilt all ten Vulkan object classes with `VK_NULL_HANDLE` for the
pipeline cache. So cluster **L3 landed here**, in `src/mccgpu.c`: a resident descriptor
set layout, descriptor pool, descriptor set, command pool, command buffer and fence; a
real `VkPipelineCache`; a 64-entry `(code, nlive) → (module, layout, pipeline)` cache; and
persistent input/output buffers that grow and rebind rather than being allocated, mapped,
unmapped and freed every call. Nothing on the dispatch path is destroyed any more.

| | before | after |
| --- | ---: | ---: |
| fixed cost per dispatch | ~640,000 ns | **~20,000 ns** (32×) |
| per-lane cost | ~176 ns | **~107 ns** |

The remaining per-lane figure is almost entirely host-side marshalling — the
int64→two-int32 pack, the copy into the write-combined mapping, the copy out and the
unpack-and-fit. It is flat in slice size, which is what makes the next table readable.

**The synthetic sweep, and the result that matters:**

| slice nodes | cpu ns/tuple | gpu fixed ns | gpu ns/lane | break-even lanes |
| ---: | ---: | ---: | ---: | ---: |
| 3 | 68 | 18,894 | 208 | never |
| 7 | 189 | 16,465 | 207 | never |
| 15 | 404 | 20,448 | 208 | **104** |
| 31 | 783 | 18,391 | 205 | **32** |
| 63 | 1,465 | 29,621 | 207 | **24** |
| 127 | 2,664 | 29,023 | 217 | **12** |
| 255 | 4,549 | 21,614 | 229 | **5** |
| 1023 | 14,998 | 73,259 | 302 | **5** |
| 2047 | 28,756 | 55,571 | 292 | **2** |

CPU cost is linear in node count at ~14 ns/node; device per-lane cost is flat. So the
device crosses over as soon as a slice costs more than ~207 ns/tuple on the CPU, which is
**about 15 nodes**, and from ~30 nodes it needs only tens of lanes.

**This refutes S5's estimate.** The plan projected break-even at **~1,177 lanes** from a
100 ns CPU slice against a 117 µs fixed cost, and concluded a scalar bid could never win
and only large data-parallel loops mattered. Both inputs were wrong in the same
direction: residency takes the fixed cost to 20 µs, and real slices worth offloading are
much more than 100 ns of CPU work. **Break-even is 5–104 lanes over the range that
matters, not four figures.** S5c's "bid only on loops" is still the right v1 policy, but
the loop no longer has to be enormous — a 30-node slice inside a 32-iteration loop is
already at break-even, and `ast_loopdep` certainly supplies those.

**And the real blocker is now visible, and it is not the device.** Every slice in the real
corpus is **3–4 nodes** — squarely in the "never" band. Two causes, both in the harness
rather than the hardware: `scan_subtree` is greedy top-down and takes the *first*
qualifying node, and the strict-width rule rejects the large untyped `Binary` roots that
would otherwise be the big slices. So the question the plan should now be asking is not
"can the device win?" — measured, yes, from 15 nodes — but **"why are our slices 4 nodes
when the device needs 15?"** That is S1's unit-of-work question, and it is where the next
effort belongs.

### The strict-width rule is gone, and removing it took three real bug fixes

The earlier workaround — "a slice whose result width has to be guessed is not schedulable
work" — was measured and it cost **99.2% of all candidates**: 3157 of 3181 lowerable
subtrees in the corpus are untyped at the root, and **every subtree of 15 nodes or more,
the band where the device starts to win, was among them.** Max typed subtree: 7 nodes.
Max untyped: 22. So the rule was not a conservative default, it was the whole blocker.

`MCC_RIR_STAMP=2` does not help — measured at `=0`, `=1`, `=2` and with `MCC_RIR_PROD`
1 and 2, Binary typed coverage in the dump stays at **1.2%**. The stamping that fixes the
ladder's readback is a different thing from the declared type on the production arena.

So instead of avoiding the divergence, it was chased down. Three separate bugs, found by
running the real-corpus differential with the rule off and dumping the first divergent
tree each time. Mismatches went **60 → 55 → 2 → 0**.

**1. The CPU evaluator did not narrow a live-in to the type of the Ref reading it.**
`ast_eval_slice.h`, both the `AST_Ref` local arm and the `AST_Load` arm, did `*out = v;`
with the raw environment word — while the `AST_Literal` arm three lines below already
called `ast_eval_slice_fit`. The device narrows per Ref, because `spv_load_live_v` is
handed the ref's own type. A `unsigned int` live-in holding `-12345` therefore read as
`-12345` on the CPU and `4294954951` on the device, and both then widened *correctly*
from different starting points. **The device was right and the compiler's own evaluator
was wrong.** The ladder never saw it because `ast_ladder_gpu_run` pre-fits every input to
its live-in's type before handing it to either side — a real precondition that was
nowhere stated and nothing enforced.

**2. Both emitters did not narrow a live-in below 32 bits.** `spv_load_live_v` and
`msl_load_live_v` take a width *flag*, not a type, so they can deliver 32 or 64 bits and
never `VT_BOOL`/`VT_BYTE`/`VT_SHORT`. With bug 1 fixed the mirror image appeared: a
`signed char` live-in holding 1000 read as `-24` on the CPU, correctly, and as `1000` on
the device. Fixed in both arms by passing the load through `spv_fit_v`/`msl_fit_v`.

**3. The runner over-narrowed the device result.** Fitting the returned value to the
slice's declared type is wrong whenever that type is finer than 32 bits: under C's
integer promotions — and under `ast_eval_binop`, which only distinguishes 32 from 64 —
an `unsigned char`-typed `255 + 1` evaluates to **256**, and fitting that back to
`unsigned char` gives 0. The correct final step is `ast_eval_narrow(v, is64, uns)`,
which is exactly what the CPU applies.

**What it bought:** the strict-width rule is deleted. Real corpus goes from **177 slices
to 783** at 0 mismatches, and in cost mode **75 of 400 slices now have a finite
break-even where previously none did**.

### Two device-layer bugs fixed, both with regression cells

**The Vulkan pending-command-buffer use-after-free (blocking item 2) is fixed and now
tested.** `vkWaitForFences` failing fell through to `done:`, which destroyed the fence,
command pool, pipeline, layout, shader module, descriptor pool, descriptor set layout,
both mappings, both allocations and both buffers — while the command buffer was still
pending. The driver recycled that memory into the next dispatch underneath a zombie
kernel. Now a failed wait strands: nothing is destroyed, `mcc_gpu.ok` is cleared, a
`stranded` counter is bumped, and the failure is reported under the existing diag var.
Leaking one process's objects is bounded, because no further dispatch can occur.

Testable at last because the hardcoded 30 s fence timeout is now `MCC_GPU_FENCE_NS`. The
`slice/fault` cell sets it to 1 ns, which times out with a genuinely pending command
buffer on a real device — no fault injection, no hang. **Verified as a known-positive:**
with the strand logic reverted the cell fails 4 checks and reports `dispatches=3`, i.e.
the post-timeout dispatch *succeeded* on recycled memory. That is the bug, reproduced.

**`mcc_gpu_mem_index` picked the worst memory type (blocking item 9).** It took the first
`HOST_VISIBLE|HOST_COHERENT` type, which here is `memoryTypes[2]` — plain system RAM, not
even `HOST_CACHED`. It now scores, preferring `DEVICE_LOCAL` then `HOST_CACHED` with
first-match order as the tie-break, so a device with one qualifying type behaves exactly
as before. On this host it now selects `memoryTypes[4]`, `flags=0x7`, the ReBAR heap.

**Blocking item 8 done:** `memset(pout, …)` was 100% dead and is gone; `memset(pin, …)`
now clears only the `[ntuple, cap)` padding tail instead of the whole buffer, which the
`memcpy` immediately overwrote. The mapping is write-combined, so these were not free.

### N9 is closed — the six uncovered opcodes now have device coverage

`TOK_UDIV`, `TOK_UMOD`, `TOK_PDIV`, `TOK_UGE`, `TOK_ULE`, `TOK_UGT` each had an MSL arm, a
SPIR-V arm and a CPU arm and were exercised by nothing, because `gen_op` substitutes them
*after* the arena records the token, so no harvested corpus can contain one. Enumeration
was the only route. `slice/ops` is a 52-row op matrix — 32-bit signed, 32-bit unsigned and
64-bit, each over a full cross product of hard values — **162 checks, 12,249 defined
tuples compared, 53 dispatches, 0 failures**, with `slice/ops-known-positive` proving it
can fail. The six are asserted individually by a coverage bitmap, so a row that stops
lowering is a failed assertion rather than a quietly narrower matrix.

### `rir-coverage` lowerable floors re-banked — dilution, and the evidence for saying so

Adding L3 residency to `src/mccgpu.c` moved `nodes_pct_loose` from 65.9111% to 65.8457%
at `-O1` and failed the ratchet. **That is not a lowerability regression.** `src/mcc.c`
amalgamates `src/mccgpu.c` (`src/libmcc.c:12`) and `rir-coverage --corpus self` measures
the compiler's own source, so editing the device layer edits the census subject. The
tool's docstring already says the `self` percentages "are only comparable across builds
that compile the same source into `src/mcc.c`", but `corpus_config` tracks build options
only and cannot see a source edit.

Measured both ways rather than assumed:

| | clean HEAD | with L3 residency |
| --- | ---: | ---: |
| arena-modelled bodies | 2767 | 2776 |
| arena nodes | 429,240 | 430,087 |
| loose lowerable | 65.909% | 65.846% |
| **absolute lowerable nodes** | **282,904** | **283,204** |

Lowerable nodes went **up by ~300**. The 847 added nodes are Vulkan object management —
calls, globals, struct writes — and only about 35% of them are lowerable against a 65.9%
average, so the ratio dilutes while the absolute count rises. Re-banked with
`--update-bank-low`, which touches `lowerable` only and not `kept_coverage` or
`residual`; the host is Linux/elf, so F4's "do not bank macho floors" does not apply.

Bisected before re-banking: reverting `src/ast_eval_slice.h` alone moved the figure by
0.0006pp, so the live-in fit is not the cause; reverting everything restored the banked
value. This is the same class as open item 5 (`node-census`'s `all_invokes_on_cpu` moving
because `src/mcc.c` grew), and it is an argument for that item's conclusion: **a ratio
over the compiler's own source is not a regression signal in either direction.**

### Four more board items closed — N7, N8, blocking item 5, open item 3

**N7 — the arena dump is reproducible again.** `354e96f6` added `sym` and `type_ref` as
raw `(uintptr_t)` columns, so two identical compiles differed under ASLR and the H4′ bank
built on dump byte-identity was invalid. Both columns are now **interned by
first-encounter order** — deterministic for a deterministic compile, so the ids are
stable across runs, and they are dense small integers rather than addresses, which is
also what makes them usable by a consumer at all. An address was never an identity
anything downstream could match on. Verified: 20-file dump is byte-identical across two
runs **and** identical between `setarch -R` and a normal ASLR run.

**N8 — `ast_replay_bb`'s frame is 87× smaller, and the stack ceiling drops 16×.**
`SValue sv_stack[VSTACK_SIZE + 1]` is 32,832 bytes declared inside the `AST_OP_ASMGEN`
arm, but C allocates the whole frame at entry and `ast_replay_bb` is recursive, so every
level paid for the inline-asm path. The arm is now a `noinline` callee, which pays it
once and only when ASMGEN fires. Deliberately *not* a file-scope buffer: `mcc_error`
longjmps straight out of that code, and a shared buffer would be clobbered for whichever
outer frame catches it — which is the trap the plan flagged.

| | before | after |
| --- | ---: | ---: |
| `ast_replay_bb` frame | `sub $0x1000` (4096 B; the 32 KB array is spilled beyond) | **`sub $0x198` (408 B)** |
| self-compile peak stack | 1024 KiB (segfaults at 512) | **≤64 KiB** |

The plan predicted 9.1× and 112 KiB; measured is **16× and ≤64 KiB**. Byte-neutrality was
checked the only way that means anything — *the same input compiled by both compilers*,
not the compiler compiling its own changed source, which is the confound that made the
first attempt look like a regression: **132 objects across `-O0..-O3`, byte-identical.**

**Blocking item 5 — `spvgate` no longer reports OK for a case that lowered nothing.** A
case whose rungs all skipped, or that compared only vacuous points, printed `OK` because
`case_bad` was still 0, so "0 mismatches" could not be distinguished from "0 points
compared". Each case now tracks its own compared count, prints it, and **fails** at zero.
All 38 cases currently report real point counts; mutation is still detected.

**Open item 3 — `matrix.yml` no longer drops three GPU cells.** It now installs
`libvulkan-dev` and passes `-DVulkan_INCLUDE_DIR=/usr/include` when the headers are
present, which `ci.yml` already did. Without them `spvgate` does not build and
`gpu/spv-slice-{differential,known-positive,real}` are never registered — 8913 cells
instead of 8916, with nothing reporting the loss. The manifest (N13) is still the real
fix; this closes the specific hole.

### Phase −1 is complete — and it was mostly already done

Measured rather than assumed. The per-opcode histogram the phase asks for **already
exists**: `rir_drop_note()` records every opcode that reaches `src/mccrir.c`'s bare
`default:`, excluding exactly the five CFG opcodes the plan names
(`JMP`/`JMPCOND`/`JMPADDR`/`JMPAPPEND`/`GSYMADDR`), and reports them as `[rir-drop-op]`.
And of the four opcodes the phase says still need handlers, **three have them**
(`RETVAL`, `MKPTR`, `VPUSHSYM`) and `LOAD` no longer drops at all.

Self-compile of `src/mcc.c` at `-O1` under `MCC_RIR_PROD=2`: **one** opcode still reaches
the default arm, `regaddi`, **4 times**. Not the four the plan lists.

What was genuinely missing is the half that makes it stick. Nothing asserted the
histogram — the drop set was diagnosable but not gated, which is how it stayed invisible
long enough to be written up as four gaps that were already three-quarters closed. New
cell **`rir/drop-ratchet`**: compiles `src/mcc.c`, parses `[rir-drop-op]`, and fails if
any opcode outside the allowlist drops or any allowlisted one drops more often.

**The first version of that cell was vacuous and I nearly shipped it.** It read
`MCC_REPLAY_IR_OUT`; the drop lines go to `MCC_RIR_PROD_OUT`. It found zero drop lines,
so its loop had nothing to check, and it passed every deliberately-broken bank thrown at
it — a lowered count, and an allowlist naming an opcode that does not exist. The fix that
matters is not the filename: it is the added requirement that **every allowlisted opcode
must actually appear in the report**, so "no output" and "no drops" stop being the same
observation. Both regression modes are now verified to fail:

```
rir/drop-ratchet: silently dropped opcodes regressed: regaddi=4 > banked 3
rir/drop-ratchet: 'nothing' is banked as dropping but does not appear in the report
```

This is the third time in this session that a cell passed while measuring nothing — after
`spvgate` printing OK for a case that lowered nothing, and the GPU cells that needed a
mutation switch before their differentials meant anything. **N13's must-run manifest is
not a nicety; it is the general form of a bug this board keeps rediscovering.**

### N13 — the must-run manifest exists

`tests/must-run.txt` (18 rows) + `tools/must-run.py` + the `ci/must-run-registered` cell.
Two checks, separated because they fail for different reasons:

- **`registered`** — the cell must appear in the build's test list. This is the
  matrix.yml class of bug: a missing dependency removes cells and the suite still reports
  success. Runs on every build, everywhere, as a normal ctest cell.
- **`must-run`** — additionally, it must not report Skipped. Needs a *full-suite* results
  file (a `-R` subset legitimately lacks most rows), so it is opt-in and wired into
  `ci.yml`'s three summary steps.

Exit codes are 0/1/2 and deliberately **never 77**: a manifest that cannot be checked is
a failure, not a skip — that is the entire point of the file. Both halves verified
against deliberately broken inputs: an absent cell reports `NOT REGISTERED`, and a
results file missing a `must-run` row reports `NOT RUN`.

The manifest is where the "is this cell real?" question gets a durable answer. The
session found three separate cells that passed while measuring nothing; this is the
general form of that check, and `tools/ci.c`'s `GATE_CELLS[]` is the same idea one
altitude too high — it gates host×feature jobs, not test names.

### N12 — done, and its stated payoff does not exist

`rebuild_arena` now consumes all 12 dumped fields instead of 7 (`type_ref`, `bp`, `bs`,
`sym`, `fbits`), which N7 made possible: those columns are interned ids now, not
addresses. They are still installed into pointer-shaped slots, which is safe here only
because **nothing on this path dereferences them** — verified as zero uses of `ast_sym`,
`ast_type_ref`, `ast_fbits`, `ast_type_bp` and `ast_type_bs` across `ast_eval_slice.h`
and `mccgpu.h`. A consumer that needs the real `Sym` needs a side table, not this.

**But the row's claim that it "raises the 28.6% lowerable lower bound for free" is
false**, and the same grep is why: if no field is read by the lowerability predicate or
by either emitter, no field can change lowerability. Measured to be sure rather than
argued — same corpus, before and after: **783 slices, 6264 tuples, 0 mismatches, byte for
byte identical.** The change is still right, because a faithful rebuild is worth having
and the fields are now correct rather than dropped; it just buys no coverage. Recorded so
the row is not reopened expecting a number.

### Final state of this session's work

Full suite: **8929 of 8930 pass.** The one failure is `cli/perfn_inproc`, which open
item 1 documents as red on purpose. `tools/must-run.py --results` is satisfied against
that full-suite JUnit file: 18 of 18 rows.

New cells, all green and all with teeth: `slice/{task,work,cpu,sched,gpu,wide64,ops,
fault,cost,real}`, `slice/{wide64,ops}-known-positive`, `rir/drop-ratchet`,
`ci/must-run-registered`.

Bugs fixed in shipped code, each with a regression cell and each verified to fail before
the fix: the `-1 * INT64_MIN` SIGFPE, the CPU evaluator not narrowing live-ins to the
Ref's type, both emitters not narrowing live-ins below 32 bits, the Vulkan
pending-command-buffer use-after-free, the worst-memory-type selection, two dead memsets,
`spvgate` reporting OK for a case that lowered nothing, and an ASLR-varying arena dump.

### Next, in order — SUPERSEDED 2026-08-08, see "The board" at the top of this file

Kept for the record. Item 0 is decided; item 1 is measured and turns out to be two
co-requisite items, not one; item 3 names statement-`If` and loops as future work when
both shipped the same day this was written.

0. ~~**The sub-word atomicity decision, before the allocator exists.**~~ **Decided**
   2026-08-08 — the allocator never co-locates two lanes' sub-word objects in one word.
   Reasoning, the measured portability of `StorageBuffer8BitAccess`, the consequences for
   the allocator, and the `spv_region_shared` refusal that enforces it are at the top of
   this file.

1. **Pointer deref (`*p`) against binding 2.** This is what `memcmp`/`strcmp`/`strncmp`
   need, and it is the difference between musl's arithmetic halves lowering and musl
   lowering. The region layer is already parameterised for it, so no new emitter concept
   is required — only a second `SpvRegion` and the address resolution.

2. **`snprintf` via the `(tag, value)` array.** +168 blocks, and the varargs objection is
   gone. Only the `%` engine remains.

3. **Extend frame storage past today's subset.** Statement-`If`, `while`/`for`/`do`,
   `x++`/`x--`, `arr[i]` on both sides of a store and a trailing `Return` have all
   landed, and the frame runner is already wired into `scan_subtree`, so the real-corpus
   differential covers it (319 accepted / 244 compared / 201 statements over 947 blocks).
   What is left, with the 2026-08-08 measurements attached: `AST_StoreVal` (3.0% of
   nodes -- needs the `AST_Store` it references, not a store of its own) and stores whose
   destination is not a local (153 of 987, 15.5%), of which only the global and `*p`
   shapes remain unmeasured. The three statement shapes still refused were measured and
   are worth **+0 blocks** each -- see the 2026-08-08 section at the top of this file --
   so none of them is the next increment.
2. **S5' -- the iteration distribution, and it must be dynamic.** Static trip count does
   not exist in this tree: no function computes one, `ast_loop_bounds` gives a constant
   *IV bound* and only when the init is a literal in the preceding statements, and
   `ast_loopdep` has no `ast_loop_parallel_legal` (though one is ~30 lines from the
   existing direction-vector machinery). The measurement wanted is a per-loop trip
   histogram over a self-compile, in the manner of `MCC_SLICE_CENSUS`. Less urgent than
   it was -- the bar is now **451 lanes at 3 nodes and 27 at 31 nodes**, not ~1,177 --
   but still the binding half, because only 4.3% of `self`-corpus census slices contain a
   loop at all.
4. **Then the JIT seam** (S2/S4/S8): narrow `mccjit_swap_lock`, graduate the bench out
   of `MCC_DEV_ENV` and flip it fail-closed for device candidates, and add the fourth
   branch to `mccjit_lazy_entry`.
5. **Remaining board items not yet touched:** N12 (`rebuild_arena` reads 7 of 12 fields
   -- now worth revisiting, since N7 made `sym`/`type_ref` interned ids rather than
   addresses, so they are finally consumable), N14 (per-lane writable globals cap lanes
   at 15), Phase -1b (bank the baseline census), and open items 1, 2, 4 and 5, which are
   decisions rather than code.

   **Closed this session:** N7, N8, N9, N4 (the Vulkan UAF), blocking items 5, 8 and 9,
   open item 3, N13, cluster L3 residency, H6/Phase 0a, and Phase -1.

Runtime JIT threads and build parallelism are both held at 16 for now; the scheduler
added here is single-threaded, so it introduces no new threads at all.

## Landed — the shared address space, real musl on the device, and three of my own bugs

### musl is the device libc — it compiles and lowers today

`vendor/musl-src` is in the tree with full source and `vendor/musl-sysroot` carries the
generated headers, so **a device libc is the existing lowering pointed at musl's own C**,
not something to write in SPIR-V and re-verify against the standard.

| | |
| --- | ---: |
| `musl/src/string` TUs mcc compiles | **65 of 74** |
| expression slices lowered / mismatches | **475 / 0** |
| frame runs accepted / built / **compared** | 120 / 94 / **94** |
| frame mismatches | **0**, and 3800+40 under `--mutate` |

Per function: `memcpy` 6 verified frame runs (86 expression slices), `memset` 7 (44),
`strlen` 1 (3). **`memcmp`, `strcmp`, `strncmp`, `memchr` lower zero frame runs** — they
walk memory through a pointer, which needs binding 2. That zero is the measure of the
remaining work, not a failure of the approach. The 9 rejected TUs are internal-dependency
cases (`strchr.c` wants `__strchrnul`), an ordinary cross-TU call.

`slice/musl` ratchets it: a 20-TU compile floor so a near-empty corpus cannot pass
vacuously, a required nonzero `frame-compared`, and a mutation arm. Verified red when the
floor is raised.

### The shared CPU<->GPU address space (binding 2)

One host-mapped storage region every lane sees — globals image, heap, printf ring — where
a pointer is a byte offset. **Offset 0 is reserved as NULL**, decided before any allocator
exists because a bump allocator handing out 0 returns a pointer equal to NULL and
`malloc`'s result is null-checked at 66/66 measured sites. Shared across lanes, not
per-lane: a heap each lane sees separately is not a heap.

Host and device see the same bytes at command-buffer granularity, which is all that
exists — mid-kernel host→device writes are invisible by every qualifier. So the host seeds
before submit and drains after completion, never during, and that is why the printf ring
is device-writes-only. `slice/mem` covers mappability, the NULL reservation, region
identity across calls, and host-write persistence.

### B1 runtime addressing, and a region layer that is not frame-specific

`arr[i]` load and store, plus a **region-parameterised** per-width access layer —
`SpvRegion{var, base, nbyte}` and `(region, byteoff, type)` — so binding 2 needs a
different region and no new emitter code. Corpus 300 → **319 accepted, 244 verified**.

Two honest corrections from that work. The payoff is **+19 blocks, not the 35 estimated**:
of 143 non-plain store destinations only 20 are `arr[i]`, the rest being globals and `*p`.
And the **13th dump column was not the prerequisite** — `arr[i]` replays through `gen_op`
on an array base so the index is in *elements*, and the `Load`/`Binary` above it carry
type 0 in 143 of 143 real arenas. A 14th column (pointee type) was needed as well.

Bounds use a **select-to-0, not the planned power-of-two mask**: masking an unpadded
48-byte region reaches byte 63, i.e. into the *next lane*. Corrupting a neighbour is worse
than corrupting yourself, and 0 is always in range.

### Three bugs in code I wrote this session

**1. I was overstating verified coverage by 2.4×.** `frame-slices` counted what the
predicate *accepted*, incrementing before `mcc_slice_frame_kernel_build` could refuse it.
174 of 300 runs were never built, never dispatched, never compared — and the gap was
almost entirely `return expr;`-only runs, so **the feature that bought the most headline
coverage bought the least verification**. That is the "cell that cannot fail" pattern
appearing in the coverage metric itself. Now three counters — accepted / built /
**compared** — and `slice/real` asserts on the last. Most of the gap was not inherent: a
run with no stores but a Return still computes a value, and letting it build took verified
runs 126 → 225.

**2. A latent out-of-bounds read in the mismatch diagnostic.** It read
`MccSliceFrame.stmt[]`, a field my own `top[]` refactor orphaned and never writes. It
therefore described arena node 0, and `ast_child(a, 0, 1)` returns `AST_NONE`, which
`ast_kind` uses as an array index. **Reachable exactly when the differential first catches
a real bug.** Field deleted, diagnostic rewritten.

**3. The constant-offset fold was wrong for pointer bases.** `ast_eval_slice_frame_off`
folded `base + K` as *bytes*; for a pointer or array base the index is in *elements*. It
gained zero blocks, so the count never caught it, and the differential could not — both
executors used the same wrong key. Now refused. **Fourth shared-mistake divergence this
session, third in my own code.**

### Varargs is not the obstacle it looked like

A variadic call at the AST level is just children with static types:
`snprintf(buf, 64, "%d %ld %s", a, b, s)` is an `AST_Invoke` with 7 children whose
`type_t` are `0x5`, `0x3`, `0x1004`, `0x5`. `va_list`, `gp_offset` and the register save
area are **host codegen below the AST** and never appear at the call site. The device owns
its calling convention, so varargs lowers to a `(tag, value)` array built at emit time,
tags free.

That moves `snprintf` from last to early in the sequencing: **+168 blocks, #2 by marginal
gain**, and the only remaining work is the `%` engine, since 64-bit division already
exists and is verified at full width by `slice/wide64`.

### Open hazard, routed and not yet resolved

**A sub-word store is read-modify-write of the containing word, and in the *shared* region
that is not atomic** — two lanes writing different bytes of one word race. In a private
frame it is fine. Three ways out: byte atomics, `StorageBuffer8BitAccess`, or an allocator
that never co-locates two lanes' sub-word objects. **The allocator route is free if taken
before the allocator is written**, which is the current position.

### Corrections to numbers previously recorded here

- The **392 / 148 / 48** eligibility partition is not reproducible against the shipped
  predicate. A model that reproduces `slicerun`'s output bit-for-bit (300 slices, 182
  statements) gets **300 / 647 / 364 / 283**. The shape holds — Invoke dominates, B1 is
  the largest non-Invoke item — but the specific figures were from a model that diverged
  from the code and should be re-derived before being planned against.
- The arena dump drops `ast_stype_t`, the RIR shadow type: **293 `Load` nodes carry
  `type_t == 0`** and are refused for that alone. Every corpus percentage quoted in
  `src/mccslice.h`'s comments was computed against a replica missing a channel the real
  arena has. Same class as the `size` column, and it wants the same fix.
- A full device libc is worth **4.4–4.7% of Invoke-blocked blocks**, against **78.0%** for
  D4b. See [`docs/DEVICE-LIBC.md`](DEVICE-LIBC.md). D2b is a *latency* lever (77.8% of
  dynamic crossings), not a coverage lever; both numbers are true and measure different
  things.

## Landed — HOST_CACHED memory, and device frame storage, 2026-08-08

### The memory type was the whole per-lane cost, and my earlier fix picked the worst one

Blocking item 9 said `mcc_gpu_mem_index` takes the first `HOST_VISIBLE|HOST_COHERENT`
type and should prefer `DEVICE_LOCAL` (ReBAR). I implemented that. **Measured, it is the
worst of the three available types by 16.8×.**

| type | flags | ns/lane, 63-node slice | ns/lane, 511-node slice |
| --- | --- | ---: | ---: |
| 2 — HOST_VISIBLE\|COHERENT (system RAM) | 0x6 | 132.8 | 54.2 |
| **3 — + HOST_CACHED** | **0xe** | **99.7** | **13.4** |
| 4 — DEVICE_LOCAL\|HOST_VISIBLE\|COHERENT (ReBAR) | 0x7 | 215.8 | 224.9 |

The reason is which side of the bus does the traffic. On the **emitter** path the kernel
touches each live-in once and writes one result, while the host packs, uploads, downloads
and unpacks every lane — so an uncached readback across PCIe is essentially the entire
per-lane cost. I2(D)'s argument for `DEVICE_LOCAL` is about the **B1 interpreter**, where
the kernel does the memory traffic instead. Scoring now puts `HOST_CACHED` first and keeps
device-local as the tie-break, with `MCC_GPU_MEMTYPE` to force an index, because the right
answer is device-specific and had to be measurable to be found.

**This changes the H6 verdict completely.** Every synthetic slice now breaks even:

| nodes | 3 | 7 | 15 | 31 | 63 | 127 | 255 | 511 | 1023 | 2047 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| break-even lanes, before | never | never | 104 | 32 | 24 | 12 | 5 | 5 | 5 | 2 |
| **after** | **451** | **146** | **66** | **27** | **16** | **8** | **6** | **4** | **3** | **2** |

`win-within-65536-lanes` went from 0/10 to **10/10**. A 3-node slice — the corpus mode —
is now offloadable at 451 lanes, where before no batch size could win. The plan's S5
estimate of ~1,177 lanes has now been wrong twice in the same direction, both times
because a cost that looked like hardware was software.

### Device frame storage — Store and Load now lower

The measured reason lowerable subtrees end is **statement boundaries**: over 32,373 corpus
nodes, `Invoke` 4.0% + `Store` 3.0% + `BasicBlock` 3.0% + `StoreVal` 3.0% + `Return` 1.2%
terminate subtrees, against a census that is 80% expression nodes (Literal 31.4%, Ref
23.7%, Convert 14.5%, Binary 10.4%). Expressions are not scarce — C statements are short,
and `Store`/`StoreVal` alone are 1958 nodes, one per assignment.

So the device now has a **frame**: storage it can load from and store to.

- **No new binding and no ABI bump.** The input buffer was never decorated `NonWritable`,
  so it is already a read-write storage buffer. Slots are dense indices over the distinct
  local offsets a run touches; the host seeds every slot, the kernel reads and writes them
  in place, and `mcc_gpu_dispatch_rw` reads the whole frame back.
- **`spv_store_live` / `spv_store_live_v`** are the store counterparts of the existing
  `spv_load_live`, handling the 32- and 64-bit halves of the lo/hi pair.
- **`MccSliceFrame`** carries the slot map and the statement list;
  `mcc_slice_frame_exec_cpu` is the CPU reference and `mcc_slice_frame_kernel_build` emits
  the kernel — statements in order, each storing its narrowed result to its destination
  slot, so **a later statement reads what an earlier one wrote, on the device.**
- **v1 scope, deliberately narrow:** `AST_Store` with a local `Ref` destination — 834 of
  987 stores in the corpus, **84.5%** — sequenced by `AST_BasicBlock`. `AST_StoreVal` is
  excluded because it is not a store: it is a vstack-ordering marker for the replay
  machinery that references its `AST_Store` by `ival`.

**Extended past v1 the same day, because the v1 subset was measurably too narrow.** Over
947 non-empty `AST_BasicBlock`s in the corpus, v1 covered **46 (4.9%)**. The blockers, by
blocks affected: `Return` 353, `Invoke` 294, `If` 238, `Unary` 119, non-local dest 58,
`Jump` 25, `Binary` 21. `Return` is both the largest and the cheapest — a run ending in
`Return(expr)` puts its value in the **out slots that already exist**, so it needs no ABI
change at all. Adding it takes eligibility from 44 to **255 of 947 (26.9%), 5.5×**.

The `Unary` statements are `TOK_INC`/`TOK_DEC` (107 and 11 occurrences) — side-effecting,
so correctly excluded rather than skippable.

**Wired into the real-corpus differential, which immediately found two bugs.** Running
frame slices over recorded arenas: **202 frame slices, 51 statements**, and 35 mismatches
on the first run.

1. **Harness bug (34 of them).** A run with no `Return` still has its out slots written by
   `spv_main_end`, so the flag there is a dummy rather than a verdict; the CPU reported
   undefined and the device reported defined-0. Only compare a returned value when the run
   actually returns one.
2. **A real emitter bug (the last one).** `spv_store_live_v` sign-extended the high word
   unconditionally, so storing `-2` into an `unsigned int` slot left `-2` where the CPU
   has `4294967294`. The high word has to follow the value's own signedness — which is
   exactly what `spv_widen` already does, and what the store failed to mirror. Same class
   as the three width divergences fixed earlier, and found the same way.

**Statement-`if` landed too**, and the reason it was tractable is the frame itself: the
two arms communicate through memory, so the device needs `OpSelectionMerge`, two blocks
of stores and a merge label — but **no `OpPhi` for the value**. Only the definedness flag
needs a phi, so an undefined condition or operand in one arm is not laundered by the
other arm being clean. Without a frame this is the ternary machinery again, with value
merging; with one it is a branch and two store lists.

`AST_If` with `op == 0` and 2 or 3 children, recursive to depth 8, nested blocks allowed.
Corpus: **208 frame slices, 73 statements, 0 mismatches; 40 under mutation.** The gain
over Return-only is modest (202 → 208, 51 → 73 statements) because most `If`-containing
blocks carry a second blocker as well — usually `Invoke`.

Real corpus now: **208 frame slices, 73 statements, 0 mismatches; 40 under mutation.**

`slice/frame`, 53 checks: slot map ordering, single-statement value, **two-statement
sequencing where statement 2 consumes statement 1's store**, an 8-frame CPU/device
differential over every slot, the `Return` path on both executors including its
definedness, and refusals for a store to a global and for a block containing a call.
`slice/frame-known-positive` proves it can fail. The Metal arm returns 0 for frame kernels
for now — declared, not silently divergent.

**Loops landed too — the emitter now has `OpLoopMerge`, where it had zero.** `while`
(op 2, `{cond, body}`), `for` (op 3, `{cond, body, incr}`) and `do` (op 4, `{body, cond}`)
— shapes taken from the corpus, not from the grammar. Same reason as statement-`if`: the
loop-carried values live in the **frame**, so the header carries phis only for the trip
counter and the definedness flag, never for a value. Without a frame every loop-carried
variable needs its own header phi and this is a different project.

**The trip cap is load-bearing, not a safety blanket.** An unbounded device loop is
exactly the occupancy watchdog hazard cluster C is about, and this tree has no static trip
count to bound one with — `ast_loop_bounds` gives a constant *IV bound* and only when the
init is a literal in the preceding statements. So every loop gets `MCC_SLICE_TRIP_MAX`
(1<<16, C3's value), and exceeding it marks the run **undefined rather than truncated**,
so the host falls back to the CPU instead of trusting a partial answer. The CPU reference
applies the identical cap, or the two executors would disagree precisely at the boundary.
At the merge, `def = d_phi AND (i < TRIP_MAX)`; `i_phi` dominates the merge, so the
over-budget test needs no extra phi.

Test: `sum(0..n-1)` over five different trip counts, CPU and device agreeing on every
slot. `op 7` is **not** a loop despite appearing in the loop op range — its children are
`{Ref|Cvt, Lit, Cvt}`, an expression shape, so it is excluded rather than guessed at.

**`x++` / `x--` as statements — the largest single eligibility jump, and it needed no
address space.** The `Unary` row read 0% coverage against real code because the emitter
handles `- ~ ! TOK_NEG` and real arenas contain `AST_OP_ADDR` (312), `AST_OP_MEMBER`
(275), `TOK_INC` (113) and `TOK_DEC` (18). ADDR and MEMBER do need B1. **INC and DEC do
not** — as a *statement* the value is discarded, so pre and post are the same thing, and
on an integer local the whole operation is `frame[slot] +/- 1`. Pointers are excluded
because they scale by element size, which is the part that genuinely needs an address
space.

Measured effect on the corpus: frame slices **209 → 300 (+44%)**, statements
**76 → 182 (+139%)**, 0 mismatches. Eligible blocks 254 → 350 of 947 (26.8% → 37.0%).

### Constant-offset `.field` / `&` resolved — correct, and worth zero blocks

The B1 research said 72% of address-shaped `Unary` nodes resolve to a constant offset from
a local, so they need no address space at all: the resolved offset is just another
frame-slot key, carried by the existing `(off[], val[])` environment with no ABI change,
and the device still sees a constant `OpAccessChain`. Verified independently before
building: **`AST_OP_MEMBER` 244/275 (88.7%), `AST_OP_ADDR` 222/312 (71.2%)**;
`AST_OP_MEMBER_ARROW` resolves 0 of 59 and never can, because its replay does `indir()`
first (`src/mccast.c:5177`) — it loads a pointer, and no constant folding crosses that.

Implemented in `ast_eval_slice_frame_off` and threaded through the CPU evaluator's `Load`
arm, `kind_ok`, `livein`, and **both** emitters' `Load` arms. The safety property that
made it low-risk: it only ever turns a **refusal into an acceptance**, so no slice that
lowered before can change.

**And it gains zero blocks.** Eligible blocks: 358 with it, 358 without. Struct-field
access is essentially never the *only* thing blocking a block — those blocks carry an
`Invoke` or a non-local store as well. This is the third such negative result this
session, after `allow_load` (0.9% of nodes, no change) and N12 (all 12 dump fields, no
change). Recorded because the reasoning was sound and only the measurement settles it: it
raises node-level `Unary` coverage from 0% to ~56%, and moves nothing that matters.

### `rir-coverage` re-banked again, same cause

`nodes_pct` 41.5127% → 41.4175% at `-O1`. Same dilution: `ast_eval_slice.h` and
`mccgpu.h` are both amalgamated into `src/mcc.c`, so editing them edits the census
subject. Nodes 429,240 → 430,787; **absolute lowerable nodes went up** — default level
178,306 → 178,419 (+113), loose 282,908 → 283,600 (+692). Third occurrence, and the
strongest evidence yet for open item 5's conclusion that a ratio over the compiler's own
source is not a regression signal in either direction.

### B1 minimal — measured payoff FIRST this time: 41 blocks

After three sound-but-zero-payoff changes in a row (`allow_load`, N12, constant-offset
`.field`), the payoff was measured **before** building this one. Modelling a
byte-addressable frame — runtime-indexed loads/stores through a base that resolves to a
local, plus address-taken locals — over the same 947 blocks:

| | blocks |
| --- | ---: |
| eligible today | 358 |
| eligible with B1 | **399** |
| **gained** | **41 (+11.5%)** |

So this one is worth building, and it is the first remaining item that is.

**Design, and the one thing that makes it non-trivial.** The frame is currently
`nslot` × 8 bytes, and `spv_store_live_v` writes **two words per store** (lo, then
sign/zero-extended hi). That is sound today only because dense slots are disjoint. Under
byte addressing a 32-bit local at frame offset −12 and another at −8 are adjacent words,
and the two-word store clobbers the neighbour. **Byte addressing therefore forces
per-width store/load** — 1 word for ≤32-bit, 2 for 64-bit, and shift/mask for sub-word
since SPIR-V has no 8-bit storage without `StorageBuffer8BitAccess`. That, not the
layout, is the actual cost.

**Sizing is not a constraint.** Measured per-block span of touched locals: median 12 B,
p90 88 B, p99 428 B, max 1560 B. A 256 B/lane region covers 95.6% of blocks. N14's
per-lane-globals ceiling does not bind — it binds on *globals*, which is the 46
global-`Ref` stores and 47 `ADDR(global)` nodes that a frame-scoped space refuses anyway.

**Bounds safety, and it must be by construction.** J3b says any `PageFault` is our own
bug and must fail loudly, so an out-of-range device store has to be impossible rather
than merely detected. Cheapest sound form, three instructions and no branch, only on
dynamic indices: `ok = idx <u extent`, `def = def AND ok`, `idx = idx AND (extent_pow2−1)`.
The mask must be relative to **this lane's own region**, so the worst an out-of-range
store can do is corrupt this lane's frame — which is then discarded because `def` is
false. That closure argument is what keeps "no PageFault is reachable" true.

**B1 splits cleanly in two, and only one half is buildable today.** Classifying every
address expression in the corpus:

| shape | count | needs |
| --- | ---: | --- |
| `base + CONSTANT` | **21** | nothing — folds to a constant offset |
| `base + RUNTIME index` | **40** | the object's **extent**, to bound the index |
| base is not a frame offset | 24 | a host pointer; out of scope for a frame |
| other shapes | 54 | — |

**The constant half is landed** — `ast_eval_slice_frame_off` now folds `base + K` for a
literal K, exactly as it already folds `.field`, in the CPU evaluator and both emitters.

**The extent column is landed.** `ast_adump_size` computes the byte extent from the real
`CType` *before* interning (the dumped `type_ref` is a dense id, not a pointer) and emits
it as a 13th dump column; `rebuild_arena` reads it, treating a missing column as 0 =
unknown, so an older dump refuses rather than guesses. Verified on a directed case:
`int arr[4]` → 16, `char buf[7]` → 7, `long big[3]` → 24, `int i` → 4 — **full array
extents, not element sizes**, which is what a bounds check actually needs. Populated for
**1079 of 1080 local `Ref`s (99.9%)**. Dump stays byte-identical across runs and between
`setarch -R` and normal ASLR, and `spvgate` is unaffected because its `sscanf` reads the
first 7 fields.

So the runtime-index half is no longer blocked on missing information. What remains for
it is the emitter work: per-width store/load (the current two-word store would clobber a
neighbouring local under byte addressing) and the three-instruction masked index with
`def` poisoning.

**The prerequisite as originally diagnosed:** To mask a
dynamic index into the object it indexes, you need the object's size, and
`MCC_ARENA_DUMP` emits 12 fields and **none of them is a size**. Without it there are only
bad options: mask against the whole lane region and a legitimate `arr[3]` silently reads
the wrong word; or poison `def` whenever the index is not provably in range, which
poisons every legitimate access too and yields correct-but-useless runs. Neither is
shippable. **The prerequisite is a 13th dump column carrying the referenced object's byte
size for local `Ref`s**, plus the matching `rebuild_arena` read — after which the mask has
something sound to mask against and the J3b "no PageFault reachable by construction"
argument closes.

That is why B1's 41 blocks are not 41 blocks of available work: 21 address expressions
are done, and the rest is gated on a dump change, not on emitter effort.

**Recommended sequencing:** additive, not a rewrite. Keep the proven slot path for runs
that need no address, and select the byte path only for runs that do. The 300 currently
clean frame slices then cannot regress, at the cost of two frame implementations until
the byte path is proven.

### `gpu/ladder-gpu-parity` could not fail on a crash — fixed

`cmake/ladder_gpu_parity.cmake` called `execute_process` twice with **no
`RESULT_VARIABLE`**, so it never looked at either arm's exit status and only grepped
stdout. A compiler that dumped core on every file would produce two arms whose census
lines were both absent, compare equal, and the cell would report PASS. Every other GPU
driver script in `cmake/` already captured the status — `spvgate_real` 3, `spvgate_mutate`
1, `slicerun_real` 3, `slicerun_mutate` 2, `rir_drop_ratchet` 1 — this one had zero.

Both statuses are now checked and reported with the failing file and the captured output.
Verified against a compiler stub that raises SIGSEGV: it now fails with
`the CPU arm failed on ... (rc=1); a crash here made this cell pass vacuously before the
status was checked`, where before it passed.

**Fourth instance of the same class this session**, after `spvgate` printing OK for a case
that lowered nothing, the GPU differentials before a mutation switch existed, and my own
first `rir/drop-ratchet` reading the wrong report file. The pattern is consistent enough
to be worth stating as a rule: **a cell that consumes a subprocess must assert on its exit
status, and a cell that compares two things must assert that it compared something.**

### OPEN — is a non-LVAL local `Ref` an address or a value?

Found while modelling B1, tried as a fix, and **reverted** because the evidence does not
yet settle it. Recording it rather than acting on it.

**The evidence for "address".** In store-destination position the two forms separate
cleanly: `arr[i] = v` has an array base of `Ref[VT_LOCAL]` with `VT_LVAL` **clear**
(op `0x32`, 24 occurrences), while `*p = v` has `Ref[VT_LOCAL|VT_LVAL]` (op `0x132`, 45).
That is the classic distinction — a decayed array name is its address and is not an
lvalue; a pointer variable is an lvalue whose value is a pointer.

**`ast_eval_slice.h` checks neither**, in `ast_eval_slice_rec`'s `Ref` arm, in `kind_ok`,
and in `livein` — all three test `(op & VT_VALMASK) == VT_LOCAL && !(op & VT_SYM)` and
then look the offset up in the environment. If the "address" reading is right, an address
is being evaluated as a value in **93 of 3994 accepted maximal slices (2.3%)**, and the
differential is structurally blind to it because *both* executors make the identical
mistake.

**The evidence against.** This tree's own test tools — `tools/spvgate.c:493` and
`tools/slicerun.c`'s `mk_ref` — both write plain `VT_LOCAL` with no `VT_LVAL` for what is
unambiguously a value reference, and spvgate's 38 cases pass against real device
execution at every rung. So within the evaluator's own convention, `VT_LOCAL` alone
means "the value at this frame offset", and `VT_LVAL` may simply not be part of the
arena's contract at that position.

**Why it matters and why it was not guessed at.** For the *ladder* the question is moot:
it compares two expressions over one environment, so a shared convention cancels. For a
*runner* executing real frame values, one of the two readings produces wrong answers.
Refusing non-LVAL local Refs was implemented and reverted when it rejected the entire
existing test corpus — which is itself the strongest argument that the convention is
deliberate. What would settle it: build a slice over a real `int arr[4]` and a real
`int *p`, run it through `ast_eval_slice` with a known frame, and compare against what
the compiled program computes. That is one directed test, not an inference.

**What remains, and why each is genuinely large rather than deferred by choice:**
`Invoke` (294 blocks) is D4b — it needs the call boundary, not an emitter change. Stores whose
destination is not a local (58 blocks) need a real address space, cluster B1, not a
frame. `AST_StoreVal` (971 nodes) is not a store at all — it is a vstack-ordering marker
referencing its `AST_Store` by `ival`, so it is subsumed by whatever handles the store it
points at.

## Open now — research findings on the open items, 2026-08-08

Four investigations. Two of them overturned the item's own premise, and two hypotheses
that read well from the code were falsified by measurement.

### Open item 1 — `cli/perfn_inproc`: **neither (a) nor (b). The pass cannot fire at any tier.**

The item asks whether the test needs a discriminating input or the pass is inert.
**The pass is not inert — it is one of the largest single-flag size wins in the tree**, and
the discriminating input already exists and is already green in a sibling harness.

| configuration | `-fno-opt-perfn-inproc` | `-fopt-perfn-inproc` |
| --- | ---: | ---: |
| `-O12 -fno-inline-functions` (what `optfire/perfn_inproc` runs) | **3686 B** | **2230 B** (−39.5%) |
| `-O12`, default inliner on | 2262 B | **2262 B — SAME** |
| `-O0` / `-O1` / `-O2` / `-O3` / `-O8`, flag added to the default | — | **SAME at every one** |

The mechanism: `do_inline` requires `!ast_inline_pass_env` (`src/mccast.c:17993`).
`INLINE_FUNCTIONS` is `MCC_OPTD_LEVEL(2)` and `OPT_PERFN_INPROC` is `MCC_OPTD_LEVEL(8)`
(`src/mccopt.h:108`, `:124`) — so the flag's own level is **six rungs above the level that
disables it**, and at -O0/-O1 the other half of the gate (`ast_has_graftable_call` needing
`ast_inline_env`) is off instead. There is no tier at which adding the flag changes a byte.

**So "does not earn its level" is true, but for a gate-ordering reason, not inertness.**
Banking `SAME` would record the right verdict from the wrong evidence and bury a working
pass. Note also the cell's `-fopt-slice` premise is unsound independently: `ast_slice_consume`
loads `~/.cache/mcc/sl-*.ck` whose salt excludes the `-f` flags, and the case does not
isolate `XDG_CACHE_HOME` the way `cli/perfn_search` does.

**On promoting it to a real tier — tried, measured, and the answer is no.** The standing
rule is to promote a strategy when cost/benefit proves it belongs, so the gate was
actually fixed and the corpus measured rather than reasoned about.

The gate fix is one clause: let the trial run when `ast_perfn_inproc_env` is set, since
`-fopt-perfn-inproc` does not *choose* to graft — it emits both ways and keeps the
smaller, so the suppression is what makes it unreachable rather than what makes it safe.
With that in, the flag stops being inert at `-O3` (2230 → 2198 on the optfire case, −32 B).
`-O1`/`-O2` stay at zero because `ast_inline_env` is `-O3`-and-above, so there are no
graft candidates below it.

Then the corpus, at `-O3` over `tests/exec`:

| | objects | total bytes |
| --- | ---: | ---: |
| `-fno-opt-perfn-inproc` | 143 | 3,969,259 |
| `-fopt-perfn-inproc` | 143 | **3,982,475** |
| delta | 26 changed | **+13,216 B, +0.333%** |

**It makes real code bigger.** The 39.5% win is real but is a property of a hand-shaped
case, not of the corpus. So the change was reverted and the flag stays at level 8.

**The diagnosis this leaves is more useful than the level question.** The trial selects on
`ind - ast_body_ind_sv` — the emitted length of *that function's body* — and that local
metric does not predict object size: grafting can keep a callee alive that would otherwise
be dropped, and it moves reloc and section content the body length never sees. So the
correct item is not "which tier" but **"the selection metric measures the wrong thing"**,
and the out-of-process variant already demonstrates the fix — `mcc_superopt_perfn`
(`src/mcc.c:1133`) scores on `so_fn_sizes`, the real emitted per-symbol size. Until the
in-process trial scores on something that predicts the object, no tier is justified.
Recorded so the next attempt starts from the metric, not from `levelpins.txt`.

### Open items 4 and 5 — ratios over the compiler's own source

Confirmed and sharpened: the same `src/mccgpu.c` edit moved `nodes_pct_loose` **down**
0.065pp while moving `bodies_pct` and `bytes_pct` **up** — one source edit moved two
banked ratios of the same census in opposite directions. Neither direction is a signal.

Two findings the items did not contain:

- **`corpus_config` has a hole.** `CORPUS_DEFS = ["MCC_DIAG"]` (`tools/rir-coverage.py:203`)
  is one entry, but `MCC_EMBED_JIT` (a user-visible CMake option, default ON) gates two
  whole translation units into `src/mcc.c` (`src/libmcc.c:20-23`). The guard that exists
  to catch corpus-shape changes cannot see the largest one.
- **Item 4 is probably a real bug, not host sensitivity.** The documented host-sensitive
  case is elf/x86-64 vs darwin/aarch64 — *different arch*. gcc-hosted vs stage2 is the
  **same** host, arch, and object format, and `host_objfmt()` returns `elf` for both, so
  `unbanked_host` is false and both are fully gated. Time-budgeted search and disk memos
  are both ruled out (level 13 and level 9; the bank is O0–O3). Two of 2765 bodies
  replaying byte-identically under one build and not another is a divergence between two
  builds of the same program — and `selfhost-fixpoint` is structurally blind to it,
  because an unfaithful replay restores the parser's bytes, so both objects are identical
  either way. **Do not raise `--tol` until `--classify` has named the two bodies.**

`--corpus exec` would structurally fix item 5 (it is the only corpus excluding
`src/mcc.c`), but needs a bank, a fix to the per-format floor schema, and a pinned file
list; and it does not touch item 4 at all.

### S5′ / grow-the-slice — two plausible fixes measured and rejected

**Rejected 1: `allow_load = 1`.** Every call site passes `allow_load = 0`, both emitters
already implement `AST_Load(Ref local)`, and the CPU evaluator resolves it through the
same environment — so relaxing it looked like the cheapest possible lever. **Measured: it
changes nothing.** Same 783 slices, same 0 mismatches. The corpus has **293 Load nodes in
32,373 (0.9%)**, of which only 97 are `Load(local Ref)`. Locals are read through bare
`Ref`, not `Load`.

**Rejected 2: "make `scan_subtree` pick the largest subtree, not the first."**
`ast_eval_slice_kind_ok` is downward-closed — a node is accepted only if every descendant
is — so the highest accepted node on a path already *is* the maximal lowerable subtree.
The greedy scan was never losing anything.

**The measured answer.** Terminators, as a share of all 32,373 corpus nodes:

| terminator | nodes | % |
| --- | ---: | ---: |
| `Invoke` | 1288 | 4.0% |
| `Store` | 987 | 3.0% |
| `BasicBlock` | 971 | 3.0% |
| `StoreVal` | 971 | 3.0% |
| `Return` | 383 | 1.2% |
| statement-`If` (not the ternary) | 295 | 0.9% |
| `Load` | 293 | 0.9% |

Against a node census that is **80% expression** (Literal 31.4%, Ref 23.7%, Convert 14.5%,
Binary 10.4%). The expression nodes are there; they are chopped into 3–4 node pieces by
**statement boundaries** — `Store`/`StoreVal` alone are 1958 nodes, one per assignment.

**This is S1b, confirmed by measurement rather than by argument: C expression trees are
small because C statements are small, and no relaxation of the expression predicate can
change that.** The unit has to be the statement run, and the ABI blocker is exactly the
thing that terminates the subtrees — multiple outputs, one per store.

And a second number that constrains it: at 1 lane the device costs ~20,207 ns against
~14 ns/node on the CPU, so **break-even at one lane is ~1,443 nodes** — larger than the
biggest invoke-free region in the tree (1,114). Only **4.3% of `self`-corpus census slices
contain a loop at all**. So growing the unit is necessary and *not sufficient*; S5′ is the binding
half, and it needs a **dynamic** trip-count histogram, because static trip count does not
exist in this tree (no function computes one; `ast_loop_bounds` gives a constant IV bound,
not a count, and only when the init is a literal in the preceding statements).

**The lever nobody costed:** the ~207 ns/lane is host-side marshalling — pack, copy into
the write-combined mapping, copy out, unpack. 207 ns to move 20 bytes is ~100 MB/s. If
that drops to 20 ns/lane, a **3-node** slice breaks even at 417 lanes and a 15-node slice
at 52, and the corpus we already have becomes eligible without growing a single slice.

> **Superseded 2026-08-08 — see the marshalling section at the top of this file.** The 207
> was measured against ReBAR memory and was the host *read-back*, not the pack/unpack; the
> `HOST_CACHED` memory-type change already collected it. Re-measured per-lane cost is
> 22.7 ns at 3 nodes, of which host marshalling is 16.1 (debug) / ~3 (release).

### N14 and S3′

**N14's numbers do not reproduce and its framing is wrong.** Re-measured: `globals_rw` is
**6,209,784 B (5.92 MiB)**, not 5.82; `.tbss` is 99% one JIT slab; `image_ro` is 4.75 MiB,
not 1.62. The lane ceiling comes out ~21, not 15 — 15 and 51 are only consistent with an
unstated ~36 MiB reservation. And the row calls 4 MiB of tables "read-mostly" when
**only 320 KiB actually is**: `rir_xt`/`rir_pt` are pointer-keyed append caches reset per
TU (not shareable at all), `ast_memo_pk`/`try`/`io_raw`/`merged`/`grad` are 1.72 MiB of
pure scratch (lazily allocatable, which is a bigger and cheaper win than sharing), and
1.56 MiB is dead unless `-fopt-slice` is on.

**But the decisive point is scoping:** N14's per-lane globals apply to running *mcc
itself* on the device (cluster B's interpreter). The emitter path measured in S5 carries
no C globals whatsoever. Since A3 was reversed to enter via the emitter, **N14 is not on
the critical path for anything currently planned** and should be re-scoped as an
interpreter precondition.

**S3′ is worse than the row states.** When 15 siblings block on `MCC_GPU_LOCK`, the one
holding it runs its *CPU* arm against an idle machine — so the device is penalised ~16×
*and* the CPU is simultaneously credited a contention discount. The bias is multiplicative
in both directions. And the majority vote is itself invalid: 16 siblings measuring one
serialized queue are 16 correlated samples, not 16 independent ones.

`MCC_GPU_LOCK` is now *more* necessary, not less: L3's residency is a singleton, and every
field in it (`cb`, `fence`, `dset`, the shared `bin`/`bout` and their mappings, the
pipeline cache array) is externally-synchronized-per-spec or outright shared data. The
90% fix is to replicate the resident state per context and then narrow the lock to
`vkQueueSubmit`, leaving `vkWaitForFences` — where the 20 µs lives — outside it. Ordering
is forced: replicate first, narrow second.

### Open item 2 — `optfire/ident_shift` on arm64

The pass is arch-neutral: `ast_ident_adopt` (`src/mccast.c:7637`) has no arch conditionals,
the gate is `MCC_OPTD_ALWAYS`, and `arch.txt` has no `ident_shift` row. `-O12` is one rung
below `MCC_OPT_SEARCH_LEVEL`, so it is not a search-nondeterminism cell either. The likely
reading is an unrelated arm64 `-O12` codegen bug for which this cell is only the messenger
— which the failure line would settle, because the run loop tries `-fno-ident-shift`
**first**.

Three things the item did not know:

1. **There are eight message sites, not six** — `optfire.sh:30` and `:31` also print
   `FAIL ident_shift:` before the differ arm is reached.
2. **The log probably already exists.** `mcc-ci stage3 --consume test` passes
   `--output-on-failure`, so the raw job log has the line verbatim; only the *step
   summary* (which is what was read) omits it, because `junit-summary` emits test names
   only.
3. **`optfire.sh` discards compiler stderr** at `:30`, `:49`, `:74`, `:76`, `:88`. If the
   mode turns out to be a build failure, the nightly log will *still* not say why, and a
   second cycle is burned. Echoing captured stderr and the counter value on the failure
   paths fixes the class for all 123 optfire cells, not just this one.

The local cross loop already runs this cell for arm64 but forces `OPTFIRE_NORUN=1`
(`CMakeLists.txt:4526`), so it covers exactly one of the eight modes and none of the
run-side ones — which is precisely why "objects differ under all five target compilers"
did not settle it.

## Open now — the coroutine task, 2026-08-08

> Context: [`docs/PLAN.md`](PLAN.md) was reframed on 2026-08-08. The GPU is no longer a
> replacement execution engine; it is a **second executor behind the JIT's existing
> scheduler**, and a slice runs on the device only when `mccjit_bench_pair` measures it
> faster *including upload, dispatch, download and readback*. That is cluster S. This
> task is **S7b**, and it is the one item in cluster S that is worth doing on its own
> merits whether or not the GPU plan is adopted at all.

### Replace the C11 threading implementation with a single-threaded coroutine that ticks

**The claim.** Four separate open problems in this tree are the same object wearing four
names, and one task representation closes all of them:

1. **The JIT pool has no shutdown** (L2′, `docs/PLAN.md` cluster L). Workers sit in an
   unbounded `pthread_cond_wait` (`src/mccjit_embed.c:1347`) and are `pthread_detach`ed
   at `:1375`, so no `pthread_t` is retained and joining is structurally impossible. A
   quit flag checked *between ticks* is a few lines. A quit flag against an opaque
   `job->run(job)` that holds the process-global `mccjit_swap_lock` across an entire
   compile (`:1353-1355`) is a redesign.
2. **D1e is already a coroutine, on the device side.** The measured-and-winning boundary
   mechanism is a pre-enqueued self-skipping resume chain: each command buffer loads the
   state vector, checks for a host reply, and exits immediately if absent. That is a
   tick. 12.4 µs median, measured 2026-08-08 (section below).
3. **C3b's step budget is a tick a third time** — suspend at loop top, `1<<16` steps,
   resume.
4. **mcc ships a C11 `<threads.h>` and barely uses it.** `runtime/include/threads.h` is
   217 lines of C11-over-pthreads, `#include_next`-passthrough when the host libc has
   one. Its only in-tree consumer is `tools/mcchv.c`. Its `mtx_timedlock` (`:144`) is a
   **1 ms `nanosleep` polling spin**, not a real timed lock.

**What exists today, so the scope is honest.** A census over `src/`, `runtime/`,
`tools/`, `include/` (excluding `vendor/`) found:

- **No coroutine, fiber, `ucontext`, `makecontext`/`swapcontext`, generator,
  continuation, scheduler, event-loop or ticking-task abstraction. Zero hits, all
  spellings.** The only task-like construct is `MccjitSwapJob`
  (`src/mccjit_embed.c:1288-1298`): a `void (*run)(job)` on an intrusive FIFO, run to
  completion, no resume state.
- `setjmp`/`longjmp` exists but is **only** error unwinding (`mcc.h:32`, `libmcc.c:863`)
  and the public entry for running JIT'd `main` (`include/libmcc.h:73`). Every `longjmp`
  unwinds outward and discards; none is a continuation.
- Threading in the compiler proper is **pthreads only, no `<threads.h>`**: 139 tokens in
  `mccjit_embed.c`, 17 in `mccrun.c`, 6 in `mccast.c` (`ast_search_pool_pthreads`), 4 in
  `mccgpu.c`. Windows is a pthread-shaped shim (`src/mccjit_win32.h:272-372`, SRWLOCK /
  CONDITION_VARIABLE / INIT_ONCE / `_beginthreadex`).
- Atomics are GCC `__atomic_*` builtins with explicit ordering, not `<stdatomic.h>`. The
  ordering that matters is QSBR epoch publication (`:1813-1854`) and the three
  release-stores that publish freshly emitted code (`:5841`, `:6724`, `:9420`).

**The suspension points a conversion has to name.** These are the blocking calls inside
worker threads today:

| worker | site | what blocks |
| --- | --- | --- |
| `mccjit_pool_worker` | `src/mccjit_embed.c:1341` | unbounded `pthread_cond_wait` (`:1347`), then `mccjit_swap_lock` held across the whole job |
| `ast_search_thread_fn` | `src/mccast.c:16877` | full AST re-optimization per candidate; can reach `flock(LOCK_EX)` at `:15546` and file I/O |
| `mccjit_qsbr_thread` | `:6976` | `nanosleep` 1 ms via `mccjit_pool_nap` (`:5731`) |
| `hv_optimizer` | `tools/mcchv.c:252` | `thrd_sleep` 5 ms (`:281`), `thrd_yield` (`:279`), `flock` (`:487`), `fsync` (`:503`) |
| `mccjit_bench_sibling_thread` | `:3338` | pure CPU, joined at `:3384` — the only joined threads in the file |

**Recommended shape.**

- A `MccTask` with an explicit resume state and a `tick()` returning
  `{done, yielded, blocked-on}` — a state machine, not `ucontext`. Rationale: `ucontext`
  is absent on Windows, deprecated on Darwin, and needs a stack per task, which collides
  directly with N8's stack findings (peak 992 KiB before the frame hoist, 112 KiB after).
  The project already writes hand-rolled machine-code stubs; a switch-on-resume-state
  task is well within its idiom and is the only form that ports to the device, where
  there is no stack to swap.
- `MccjitSwapJob.run` becomes `tick`, and `mccjit_pool_worker`'s `for(;;)` becomes a
  loop over `tick` with the quit flag checked between ticks. **That single change is
  L2′.** Narrow `mccjit_swap_lock` to the codegen inside the tick rather than holding it
  across the tick — the lock exists because the compile path runs on process-global
  state (`mccjit_last_*`, `cur_text_section`, `funcname`), which is a smaller region than
  the job.
- `runtime/include/threads.h` gains a **single-threaded backend**: `thrd_create` enqueues
  a task, `mtx_lock`/`cnd_wait` become yields, `thrd_join` runs the scheduler until the
  target completes. Selected at build time. The C11 API stays exactly as it is — the
  point is to remove the *dependency on OS threads*, not the interface, and
  `tests/exec/features_c99_c11/c11_threads.c` (4 threads × 50,000 steps, exercising
  `thrd`/`mtx`/`cnd`/`tss`/`call_once`) is the acceptance test that already exists.
- **Convert `tools/mcchv.c` first.** It is the only real C11-threads consumer in the
  tree, it is self-contained, and its optimizer thread already has explicit
  `thrd_yield`/`thrd_sleep` suspension points to convert. It is the cheapest possible
  proof that the representation is adequate.

**What this does not do, stated so it is not discovered later.** A single-threaded
coroutine backend removes concurrency, not just threads. `mccjit_bench_pair`'s sibling
threads (`:3346-3384`) exist to measure throughput under contention and **must stay real
threads** — a coroutine cannot measure 16-way contention. Likewise `ast_search_pool_pthreads`
is there for wall-clock search throughput. The correct end state is *both*: tasks as the
scheduling unit, OS threads as an optional execution substrate underneath them, which is
also what lets the same task type be ticked by the GPU pool.

**Order.** After Phase 0a instrumentation, alongside Phase 1's L2′. It is a prerequisite
for the tick-based framing of D1e (Phase 5) and C3b, and it is the mechanism L2′ needs.

## Open now — caveats left by the CI matrix replication, 2026-08-08

Six items, each with the decision or measurement that closes it. Detail and evidence are
in *The Linux stage2 matrix, replicated locally* and *`rir-coverage` and `node-census`
re-banked* below; this list exists so none of them is only findable inside a write-up.

1. **RESOLVED 2026-08-08 — the case needed a discriminating input; `SAME` was never
   banked. See the `cli/perfn_inproc` section at the top of this file. The claim below
   that the flag "changes no byte" is true only of the old case's own source: on the
   `chunk`/`driver` shape it is 3382 → 2022 B at `-fno-inline-functions -O3`.**
   ~~`cli/perfn_inproc` is red on purpose. Do not bank `SAME` to make it green.~~
   `-fopt-perfn-inproc` changes no byte at `-O3/-O8/-O10/-O12`, with or without
   `-freemit-templates`, with the inliner on or off, toggled on *or* off from its
   default, and on a fat callee inlined at eight sites. It is **not a recent
   regression** — `SAME` at `879bf988`, `c2838c61`, `109d407a`, `6c46618e`, `1ad3f1aa`
   — and `6c46618e`, whose sole purpose was fixing this cell, never did.
   **Decide which of two things is true:** the case needs an input that discriminates,
   or the pass is inert and earns no level. Banking `SAME` asserts the second without
   establishing it.

2. **`optfire/ident_shift` on arm64 is unreproduced, and the failure line would settle
   it.** It passes here under gcc, under stage2 self-host, with and without Vulkan, at
   load average 23 (40/40), its objects differ under all five target compilers, and the
   arm64 binaries print `178` under `qemu-aarch64`. `optfire.sh` prints exactly one of
   six distinct `FAIL ident_shift:` lines and separates "pass DID NOT FIRE" from
   "output differs from `-O0`" from "build/run failed" — three different fixes.
   **Needs: the `FAIL ident_shift:` line from the arm64 nightly job.**

3. **`matrix.yml` silently drops three GPU gate cells on every Linux cell.**
   `ci.yml` installs `libvulkan-dev` (`:115`) and passes `-DVulkan_INCLUDE_DIR=/usr/include`
   (`:150`); `matrix.yml` does **neither** (`:151`). Without headers `spvgate` does not
   build, so `gpu/spv-slice-{differential,known-positive,real}` are never registered —
   8913 tests instead of 8916 — and nothing reports the loss. This is exactly the N13
   must-run-manifest hole, and it is also what made identifying the failing cell an
   exercise in index arithmetic. **One line to fix; the manifest is the real fix.**

4. **`kept_coverage` is host-sensitive and the bank must come from the stage2 tree.**
   The gcc-hosted and stage2 self-hosted compilers disagree: `fallback 98 / kept 82.7770`
   against `100 / 82.7139` at `-O0`. That 0.06pp spread is **outside the tool's
   `--tol` of 0.05pp**, so banking from a gcc host re-breaks CI, which tests the stage2
   tree. The floors are currently the lower of the two.
   **Decide: raise `--tol`, or make the metric host-stable, or write the "bank from
   stage2" rule into `rir-coverage.py` so it cannot be got wrong silently.**

5. **`node-census`'s `all_invokes_on_cpu` may not be worth gating.** It is a ratio over
   the compiler's own source, so it moves whenever call density changes — it fell
   94.9385% → 94.8004% purely because `src/mcc.c` amalgamated ~2700 new lines. It is not
   a regression signal in either direction. The **external-only** ceiling is the number
   the plan's headline rests on and it held at 99.2540%. **Decide whether the
   all-invokes leaf should be gated at all, or reported only.**

6. **`8fd8c54e` moved the `wide` corpus, not `self`.** Re-measured on the merged tree:
   `self` `-O1` kept is **82.777%**, unchanged, so the re-bank in `e2b8bdc4` is still
   the right floor. Recorded because the two changes look like they collide and do not.

## `kept_coverage` is host-sensitive by ~1 point, not ~13 — correcting `55d83d53`

**The 12.9-point host gap that commit reported does not exist, and the error is
instructive: it compared a post-change measurement against a pre-change bank.**

`55d83d53` justified skipping `kept_coverage` on Darwin by building `db7c6829` — the
commit that wrote the bank — and measuring 83.219% at `-O1` against the banked 96.156%,
concluding the difference was the host axis. But `e2b8bdc4`'s bisect (Linux, one Debug
build per commit) shows elf/x86-64 kept was 96.162% only up to `6c46618e` and **82.520%
from `1ad3f1aa` onward** — "opt(ladder): the -O levels, measured", which moved eleven
passes across the ladder. And `db7c6829` is a **descendant** of `1ad3f1aa`
(`git merge-base --is-ancestor` confirms). So the elf host at `db7c6829` was already at
~82.5%, not 96.156%; the bank was simply stale, because the `kept_coverage` gate did not
land until `78d4856f`, four commits later. **~12.2 of the 12.9 points were bank
staleness and ~0.7 was the host.**

Measured at HEAD on darwin/aarch64 against the refreshed bank:

| corpus | level | this host | banked (elf/x86-64) | verdict as a floor |
| --- | --- | ---: | ---: | --- |
| self | -O0 | 84.1121 | 82.7139 | **+1.40, passes** |
| self | -O1 | 84.2229 | 82.7723 | **+1.45, passes** |
| self | -O2/-O3 | 84.4751 | 82.8846 | **+1.59, passes** |
| wide | -O0 | 93.355 | 92.7507 | +0.60, passes |
| wide | -O1 | 93.420 | 98.3968 | **−4.98, fails** |
| wide | -O2 | 93.465 | 98.3841 | **−4.92, fails** |

So the two corpora disagree about the size of the effect, which is the part worth
keeping. **`self` is gateable on Darwin today** — it clears the refreshed floor at every
level. **Nothing is armed on the strength of the self margin**, and the tool's stale
96.156-vs-83.219 justification string has been replaced with the numbers above.

**`wide`'s 5 points were the same failure mode a second time — SETTLED, and it was not
the host.** This section guessed "host axis or residual Darwin modelling gap"; the answer
is neither. elf/x86-64 measures wide at **92.9416 / 93.0058 / 93.0058** at -O1/-O2/-O3
against that same 98.4 bank, i.e. Linux is within a tenth of darwin's 93.4 and it is the
*bank* that predates the `chain-store` demotion — exactly the `1ad3f1aa` staleness this
section had just finished diagnosing for `self`, missed for `wide` because `e2b8bdc4`
re-banked only `self`. The underlying defect was a replay-fidelity bug in
`rir_to_arena()` (see debt #6); with it fixed, elf `wide` is 92.923 / 96.604 / 96.653 /
96.653 and re-banked. **Lesson, third time: before attributing a gap to a host, measure
the other host.**

Two durable lessons. A ratchet whose gate lands *after* its bank is written has a
window in which the bank silently rots, and `78d4856f` sat four commits behind
`1ad3f1aa`. And **"fixed commit" is not the same as "fixed bank"** — pinning the commit
made the comparison look controlled while the bank it was compared against came from
somewhere else entirely.

**Still open:** a per-format schema for `residual` and `kept_coverage`, without which
neither can be armed off elf. (wide's 5 points is settled — stale bank, see above.)

## D1e is measured, and it wins — 2026-08-08, Apple M1 Pro

The experiment `docs/PLAN.md` called "the highest-value single experiment left, and it
decides the D1 row" has been run. **A speculative pre-enqueued self-skipping resume
chain reaches 12.4 µs median / 19.1 µs mean, beating the ~30 µs projection by 2.4× and
the doorbell's 24 µs by 1.47× on the mean.** D1e is the recommendation and the N5 sweep
constant is obsolete.

Harness: one `StorageModeShared` buffer with the post block, result block, 64-word state
vector and progress counters on **four separate pages**, so no host and GPU write ever
share a line. A refill thread keeps *depth* command buffers committed-but-uncompleted;
each dispatch loads `post_seq`/`result_seq`, **returns immediately if equal**, else sums
all 64 state words and writes the result. The returned token is checked against
`tok ^ 0x5a5a5a5a ^ Σstate`, so a stale doorbell, a stale state vector *or* a stale token
all abort the run — the state term is what makes this a payload test and not a flag test.
Every variant ran in ≥7 separate processes, N=3000–20000 after 300–1000 warm-up, on AC.

| variant | median | p90 | p99 | mean |
| --- | ---: | ---: | ---: | ---: |
| D1a, `waitUntilCompleted` | 175–192 | 203–229 | 245–638 | 177–181 |
| D1a, spin-poll shared memory | 67–70 | 90–95 | 130–151 | — |
| pipelined submit, never waiting | **19.6–20.3 µs/CB** throughput | | | |
| **D1e, depth 128, no sweep** | **12.2–12.8** | 44.7–46.1 | 58.3–62.1 | **19.0–19.3** |
| D1e with a 32 KB sweep added | 80.5–81.7 | 88.4–95.5 | 92.1–98.6 | — |
| **D1b, doorbell, 32 KB sweep** | **23.9** | 31.8–31.9 | 32.0–43.8 | 27.5–28.5 |

**The banked table reproduces**, which is what licenses comparing the new row to it: D1a
175–192 against 144–180, pipelined 19.6–20.3 against 19.5, doorbell 23.9 against 24. The
doorbell's cost is confirmed 100% sweep with a zero intercept — 8 KB → 8.0, 16 KB → 15.8,
32 KB → 23.9 µs, ~0.99 µs/KB.

**The projection was one CB period pessimistic.** The measured mean, 19.1 µs, *is* one
pipelined CB period, not one and a half.

**Cache visibility — the load-bearing claim — holds with no sweep.** Across every
no-sweep run, depths 1 to 1024 including two 200,000-iteration soaks, **~800,000
host→device posts produced 0 stale reads, 0 token mismatches and 0 first-read retries.**
Rule of three bounds the per-crossing stale rate at **< 4 × 10⁻⁶** — under 0.011 expected
failures across a self-compile's 2,851 residual crossings, against D1b's banked ~2.9. And
it was tested genuinely speculatively, not as a saturated pipeline: at 1000 µs host
think-time the chain ran **1,032,267 CB starts to serve 20,500 requests — 49 skips per
serve — with the median still 12.08 µs and zero aborts.** Adding a 32 KB sweep costs
80.7 µs against 12.4 and buys nothing. **D1e does not collapse into D1b.**

**D1b is worse than its banked note says.** At 8 KB the doorbell **served with a correct
flag and a stale payload** — 3 token mismatches at 8 KB, 13 at 4 KB. The sub-threshold
failure mode on this host is not only the documented silent hang but **silent wrong
answers**: the doorbell word arrives and the state vector does not. That is an argument
against D1b independent of latency.

**The "~64-CB `MTLSharedEvent` deadlock" is not an event bug.** It is
`MTLCommandQueue`'s default `maxCommandBufferCount` of 64: committing the 65th blocks the
caller, and if all 64 wait on an event only that thread can signal, the process
self-deadlocks. Bisected exactly — depth 64 runs, 65 and everything above deadlock at the
first signal with `cbstart=0`. `newCommandQueueWithMaxCommandBufferCount:2048` makes the
same blocking chain run to depth 1024 (107 µs at 128, 137 at 1024, reproducing the banked
125 µs). **It does not apply to non-blocking chains at all**: with the default queue D1e
simply runs at effective depth 64, because `commit` back-pressures until a CB retires.

Depth sweep at `maxCommandBufferCount=2048`, 30,000 iterations per depth in separate
processes — flat from 16 to 1024, zero aborts across 180,000 iterations:

| depth | 16 | 64 | 128 | 256 | 512 | 1024 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| median | 12.50 | 12.38 | 12.38 | 12.46 | 12.54 | 12.62 |
| p99 | 97.62 | 62.25 | 60.29 | 60.92 | 84.62 | 62.38 |

Depth matters only below 16: depth 4 is 30–35 µs and **depth 1 is 137–208 µs, i.e. D1a** —
the control proving the win comes from pre-enqueueing, not from the kernel shape.

**Contention behaves oppositely for the two designs.** In reps where the pipelined probe
degraded 20 → 226–329 µs/CB and D1a's median went 180 → 7986 µs, **D1e held at
12.2–15.0 µs**: short CBs interleave with other clients. D1b is robust for an
unacceptable reason — the persistent kernel monopolises the GPU, and when it could not it
was descheduled in visible 16 ms quanta. D1e's *tail* does suffer (one rep carried a
111 ms outlier pulling its mean to 229 µs); D1b's does not.

**Where D1b still wins: the tail above ~p85.** D1b is tightly quantised to its poll
period (p99 = 32.0 µs) while D1e is p90 ≈ 45, p99 ≈ 60. If a hard tail bound ever matters
more than throughput that is D1b's only remaining advantage, and it is bought with a
hardware-specific sweep constant, a silent-corruption failure mode below threshold, and
GPU monopolisation.

Boundary-cost table re-run at D1e's 19.1 µs mean against the 0.093 s baseline: 944,327
crossings → 18.0 s = 194×; +D2b (210,089) → 4.01 s = 43×; +B3b (9,671) → 0.185 s =
**2.0×**; +B6c (2,851) → 0.0545 s = **0.59×**. The plan's ordering claim survives — all
four reductions are still wanted — but softens: D1e reaches 2.0× on three of four where
D1b reaches 2.9×.

**Not measured.** No device-side interpreter work between resumes — the state vector is
64 words and each CB is otherwise empty, so this is boundary cost alone and says nothing
about a real suspend/resume against a 64 MiB B1 buffer or a larger per-CB working set.
Single-lane throughout; **lane parallelism is D1b's actual justification and is untouched
by this result.** The refill thread burns a full core spinning and its cost inside a real
compiler was not modelled. The MoltenVK arm was not exercised, so nothing here says the
visibility holds through MoltenVK or on Linux/NVIDIA. `maxCommandBufferCount` was raised
to 2048 with no memory or driver cost measured. And the **"by spec" half of the plan's
claim was not verified** — what holds is an empirical bound of 4 × 10⁻⁶ on this device and
this OS build, which is evidence, not a guarantee.

## The Darwin path was executed — 2026-08-08, Apple M1 Pro, macOS 26.5.2

Everything below is measured on an M1 Pro, on AC power, **under heavy contention**
(five concurrent agents, load 6.9–30.2). Timings are min-of-N, which is the right
estimator because contention only adds; where a quiet window was caught the numbers
agreed to within 3%.

`otool -L mcc` shows **`libobjc.A.dylib` and nothing else** — no Metal.framework, no
libvulkan. The `dlopen` loader claim in "Darwin is reasoned, not run" holds, now run.

### E6 is closed as a negative result — refuse `double` on Metal, permanently

MoltenVK reports `shaderFloat64 = 0` and does not emulate; `shaderInt64 = 1`, so the
int64 rung was emulated *by choice* while f64 has no native option on either arm here.

The payoff is zero, measured three ways:

- **0 of 7,971 ladder type-rejections are float**, over two corpora including one
  deliberately FP-saturated. 100% of the dominant `no-static-type` refusal is the
  untyped-node artifact, not float.
- **0 slices blocked solely by float** across 104,237 slices from 189 files. On a
  corpus 27.5% float-carrying the payoff is **2 slices**, both `UNARY REF FLOAT` — a
  sign-bit flip needing no arithmetic.
- Float is *not* rare — 1.068% `DOUBLE` + 0.028% `FLOAT` of arena nodes, 1.673%
  float-touching, 2.95% of `Binary` — and that makes it worse, not better. Every
  float-carrying slice is co-blocked by `STORE` 1523, `RETURN` 1016, `BB` 858,
  `INVOKE` 298, `FOR` 282. **The blocker is E4, not E6.**

Cost is ~48 refusal sites against int64's 16, plus the first `OpTypeFloat` in the
SPIR-V arm, a `Float64` capability, and the hard-wired `int` storage-buffer element
type. A soft-f64 MSL prelude measures 8,700 B hand-minified (~12–16 KB emitted, 3–4×
the entire current prelude) and **78.6 ms per pipeline compile**.

**The cheap `float` rung does not exist.** Apple Silicon flushes subnormals
unconditionally in `MTLMathModeSafe`, `Relaxed` and MSL 3.2 alike — it is the FPU, not
the compiler, matching `shaderDenormPreserveFloat32 = 0`. `denorm + 0.0 → 0.0` and
`denorm / 0.0 → NaN` where IEEE says `Inf`. So bit-exact native `float` is unreachable
and E3 would need soft-float32 with per-operation exponent guards. Under safe math
`+ - * /` are correctly-rounded RNE with **zero residual** once subnormal flushing and
NaN payloads are accounted for; `sqrt` never is (155,585 residual mismatches in 1M).
`int↔float` conversion is bit-exact in all three directions, 0 of 1,000,000.

E6 leaves *Still genuinely open* and joins F5 and E5 as a closed row with a measured
zero. The trigger to revisit is **E4 landing**, not any FP measurement.

### The interpreter step rate — C3's `1<<20` budget is 16× too large

A `glslc`-built C1a dispatch loop (verified by `spirv-dis` as 1 `OpLoopMerge` + 1
`OpSelectionMerge` + 1 `OpSwitch` at every size, and `spirv-val`-clean at every size,
which the C findings could not check). 32 B node stride, 4096 nodes, arms that cannot
fold into one another. GLSL and MSL emitted from **one generator**, and at 320 arms all
four mixes produce **bit-identical output through MoltenVK/SPIR-V and native
Metal/MSL**.

ns/step, 1 lane, by switch arm count:

| arms | `reg` | `fetch` | `real` | `mem` |
| ---: | ---: | ---: | ---: | ---: |
| 8 | 120 | 205 | 239 | 405 |
| 32 | 248 | 314 | 333 | 733 |
| 128 | 582 | 681 | 730 | 1128 |
| 320 | 903 | 1031 | 1029 | 1563 |

**Quote ~1 µs/step, 1.0–1.4 M steps/s per lane**, bracketed 300 ns (32 arms) to 1.6 µs
(320 arms, memory every step). The measured *floor* — pure register arithmetic, 8 arms,
no device memory — is **120 ns/step**. **C3's structural 20–100 ns bracket is optimistic
by 7–50× and its lower bound is unreachable in any configuration constructible here.**
And these arms are 3 ALU ops; a real AST arm does operand decode, stack traffic, type
dispatch and gas accounting, so every figure is a *lower bound*.

**Per-lane latency is flat across a 16,384× change in occupancy** (1063 ns/step at 64
lanes, 1101 at 16,384). The single-lane rate is the real per-lane latency, not an
under-occupancy artifact: aggregate reaches **1.49e10 steps/s** while single-lane stays
at 1.0e6. All of the device's power is width. 64 *converged* lanes cost the same wall
clock as one and deliver 64×; **full divergence costs ~10×** (10,670 ns/step/lane at
`real`/320). That prices B5/N14 exactly.

**Switch width is the dominant controllable cost**: ~2.5 ns per added arm per step,
decelerating but far closer to linear than logarithmic — a chain/tree lowering, not a
jump table. A 14-kind switch to a 320-arm switch is ~4×. Whether a hierarchical or
computed-goto lowering beats it is the obvious next experiment and is unmeasured.

**Dispatch latency, measured directly**: Metal p50 **150 µs** (min 132, p90 187),
MoltenVK p50 181 µs. The Metal figure reproduces the banked 150 µs exactly. So C3's
rule window is `100·L = 15 ms` to `2 s / 20 = 100 ms` — **6.7× wide, not 33×**; the 33×
assumed a ~30 µs latency and the real one is 5× that.

`1<<20` steps costs 0.251–1.079 s per round, i.e. **2.5–10.8× over the 2 s/20 ceiling at
every switch width**. The one power of two satisfying both bounds at every width is
**`1<<16` = 65,536 steps** (rounds of 15.7–67.4 ms). **Bank the rule, not the number**:
`budget = T_target / step_cost`, and `1<<16` is this host's instantiation at 1 µs/step.

**The node count that follows.** `1<<16` steps at ~730 ns is 65,536 node visits in
48 ms — 1074 median 61-node bodies, or 2642 mean 24.8-node invoke-free regions, or 9.3
executions of the 7019-node `unary_nested`. **One pass over the whole 374,310-node
self-compile arena is 5.71 rounds and 0.27 s of single-lane device time — 3× the entire
0.093 s CPU self-compile.** That is the number the interpreter's performance story has
to answer and it is not in the plan today.

### Driver compile time — the SPIRV-Cross hypothesis is refuted

MoltenVK cold, fresh process, never-before-compiled module. `vkCreateShaderModule` is a
memcpy (0.34 ms at 99k words); **SPIRV-Cross and the Metal compile both live inside
`vkCreateComputePipelines`** and cannot be separated by API call.

| words | total cold | native Metal equivalent | ratio |
| ---: | ---: | ---: | ---: |
| 1649 | 120.6 ms | 62.5 ms | 1.93× |
| 8050 | 160.7 | — | 1.47× |
| 15958 | 245.0 | 109.5 | 1.22× |
| 24906 | 433.1 | 201.5 | 1.21× |
| 49740 | 1321.9 | — | 1.10× |
| 99373 | 5948.5 | 5657.7 | 1.05× |

**MoltenVK is not slow because of SPIRV-Cross.** Translation is 5–20% and its share
*shrinks* with size. The seconds are in the Metal back end, and native Metal pays 95% of
the same bill. **Choosing the Metal arm over MoltenVK buys 5–20% of compile time and
nothing else** — this is not an argument for either backend.

What matters is superlinearity: net of a ~75 ms fixed first-pipeline cost, cost grows
with exponent ≈1.0 to 16k words, 1.65 at 25k, 1.80 at 50k, **2.23 at 100k** — linear
(~14 µs/word) below ~25k words and quadratic above ~50k. **That makes A4's density
estimate load-bearing**: 15–25k words is 245–433 ms; 50–100k is 1.3–5.9 s.

**A cross-process cache exists whether you ask for one or not** — the macOS system
shader cache gives 120 ms vs 5948 ms at 99k words, a 49× difference, so quoting a
warm number as cold would badly mislead. A persisted `VkPipelineCache` works on
MoltenVK and collapses every size to **1.6–3.1 ms, flat**.

**Per-region marginal cost, 30 *distinct* modules back-to-back in one warm process** —
the A1a number, and the reps above measure a cache hit, not this:
**MoltenVK 5.1 ms + 8.5 µs/word; native Metal 4.0 ms + 6.9 µs/word.**

**A1a is not viable as "emit every region", on driver compile time alone.** 374,310
nodes at a mean 24.8-node region ≈ **15,100 regions inside a 0.093 s compile** — a demand
of ~162,000 regions/s against a driver supply of **~52/s** at 1649 words. Shortfall
**~1100–3100×**. Whole-function granularity is 2452 bodies × ~12 ms ≈ 29 s, still ~316×.
Neither cache helps: both fire only on a *repeat* of the same module, which on a cold
compile never happens. This is a new quantitative argument the plan does not make, and it
is independent of the size cap, of correctness and of the step rate.

**A1b's one-time 15–25k-word compile is acceptable** — 245–433 ms cold, 2–3 ms with a
persisted cache. Caveat the plan should record: it is **per process**, so a compiler
invoked 2452 times pays it 2452 times unless the cache blob (120 KB at 25k words) is
persisted to disk. **A1c now has a budget**: at 5–19 ms per novel module a 1 s
per-process specialization budget buys **50–200 compiled regions** — a hard cap of order
100, not a preference.

### Not measured

NVIDIA/AMD/Intel back-end compile time (no such device here); Windows TDR; register
pressure and AIR quality (no AGX disassembler — `maxTotalThreadsPerThreadgroup` stayed
1024 up to 99,373 words, which is weak evidence of no cliff and nothing more); a real
interpreter arm; realistic partial divergence (only the two extremes were run);
`MTLBinaryArchive`; gas surcharging for D2b primitives.

## The Metal per-value differential, and N6 on Darwin — 2026-08-08, Apple M1 Pro

A second pass on the same host as the section above, on a disjoint subject: the MSL
gate that `f716cf8d` made dual-backend, the N6 stack overflow, and the suite. macOS
26.5.2, `MTLCreateSystemDefaultDevice` returns **`Apple M1 Pro`**, MoltenVK 1.4.2 at
`/opt/homebrew/Cellar/molten-vk/1.4.2/lib/libMoltenVK.dylib`. **The run was cut short
by a scheduled reboot**; what did not finish is named at the end rather than guessed at.

`cmake-mtl` (Release, `MCC_GPU_BACKEND=metal`) built with **no Darwin breakage and no
source change**. Nothing in `CMakeLists.txt`, `cmake/` or `tests/` needed editing.

### The loader claim, executed

`otool -L cmake-mtl/mcc` is **`libSystem.B.dylib` and `libobjc.A.dylib`, nothing else**.
Under `DYLD_PRINT_LIBRARIES` the same process loads
`/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation` and
`/System/Library/Frameworks/Metal.framework/Versions/A/Metal` at runtime. Every bail-out
in `mcc_mtl_load` has a `MCC_AST_EVAL_LADDER_GPU_DIAG` message and **none fired**, so
Metal, `objc_getClass`, `sel_registerName`, `objc_msgSend` and `fegetenv`/`fesetenv` all
resolved; `NSAutoreleasePool` and `NSString` are exercised by the device-name path that
printed `Apple M1 Pro`. A warm-up dispatch completes: `dispatches=1 lanes=64`.

The vulkan arm on the same host links **only `libSystem`** and `dlopen`s MoltenVK — so
neither arm links its driver, which was the whole claim.

### The Metal per-value differential — 151.9 M points, zero mismatches

This is the row PLAN listed as *"the clearest gap in the ladder's evidence"*. It is
closed. `mslgate` drives `msl_expr`/`msl_module_finish` and dispatches through
`mccgpu.c`; `nm -u` shows **0 Vulkan symbols in `mslgate` and 39 in `spvgate`**, so the
two arms are genuinely separate code, not one path wearing two names.

Arenas harvested with `MCC_ARENA_DUMP`, then replayed per value against the CPU oracle:

| corpus | arenas | bodies | slices | dispatches | points | compared | vacuous | mismatches |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| synthetic, 38 cases | — | — | — | 152 | 2,500,856 | 2,359,788 | 141,068 | **0** |
| `spvgate_real` cell corpus, 40 files | 163 | 116 | 646 | 3,025 | 42,207,184 | 41,586,207 | 620,977 | **0** |
| all of `tests/exec`, 304 files | 1,325 | 616 | 2,359 | 11,129 | 153,158,012 | 151,870,645 | 1,287,367 | **0** |

`compared` is the count of points where CPU and GPU **agreed the slice is defined** and
the 64-bit value was then compared bit-exactly; `vacuous` is where both agreed it is
undefined. A definedness disagreement counts as a mismatch, so **0 mismatches over the
wide corpus also means the definedness predicate matched on all 153,158,012 points**,
not merely on the 151.9 M that carried a value.

The gate is not blind: `--mutate` over the 40-file corpus reports
**mismatches=41,586,207**, i.e. every compared point, and the synthetic mutate reports
2,359,788. Both exit non-zero as the cell requires.

### Metal and MoltenVK, compared directly on one corpus

The only host in the project that can run both arms. Same arena dump, same run:

| | arenas | slices | dispatches | points | compared | vacuous | mismatches | rejected |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `mslgate` (native Metal) | 1,325 | 2,359 | 11,129 | 153,158,012 | 151,870,645 | 1,287,367 | 0 | 0 |
| `spvgate` (SPIR-V/MoltenVK) | 1,325 | 2,359 | 11,129 | 153,158,012 | 151,870,645 | 1,287,367 | 0 | 0 |

**Every counter is identical and there is no divergence to report.** The two emitters
select the same slices, lower the same count, and agree with the CPU on every point.
`gpu/ladder-gpu-parity` likewise reports **1767 dispatches, 0 differing files on both
arms** — the same 1767 the Linux/Vulkan host banked.

Wall clock, back-to-back but **under contention, so indicative only and not bankable**:
13.3 s for the Metal arm against 81.8 s for MoltenVK on the wide corpus.

### `spirv-val` and `glslc` are installed here, and nothing in the build uses them

Contrary to the briefing, both are present (SPIRV-Tools / shaderc **v2026.3**). But
`spirv-val`, `SPIRV_VAL`, `glslc` and `GLSLC` appear **nowhere** in `CMakeLists.txt`,
`cmake/*.cmake` or `tools/spvgate.c`, so no cell validates SPIR-V on any host. Run by
hand over the 152 modules dumped with `SPVGATE_DUMP`: **152 of 152 valid at
`--target-env vulkan1.1`, 0 invalid.** Wiring this into the `spv` cells is free evidence
and is not done.

### N6 on Darwin — it survived, and stack layout is the reason, not luck

`docs/TODO.md` predicted "a silent 256-byte stack overwrite that happens to survive"
where Linux took a hard `SIGABRT`. **Confirmed, and the mechanism is now known.** Three
builds of the pre-fix source (`int32_t pin[64], pout[128]`), Apple clang, arm64:

| build | result |
| --- | --- |
| Release, as shipped | **rc=0, 3/3 runs**, correct output |
| Release `-fstack-protector-strong` | **rc=0**, canary never fires |
| Release `-fsanitize=address` | **hard abort**, `stack-buffer-overflow`, `READ of size 512` |

The plain Release build already carries `___stack_chk_guard` — Apple clang emits it by
default here — so the "add a stack protector" experiment was already running, and it
still does not fire. The disassembly says why. Pre-fix frame is `sub sp, sp, #0x340`
(832 B) and clang lays it out **`pout` low, `pin` high**:

| object | extent | size |
| --- | --- | ---: |
| `pout` | `[sp+0x30, sp+0x230)` | 512 B |
| `pin` | `[sp+0x230, sp+0x330)` | 256 B |
| canary | `[sp+0x338, sp+0x340)` | 8 B |
| saved `x29`/`x30`, `x19`–`x24` | `[sp+0x340, sp+0x380)` | 64 B |

`mcc_gpu_dispatch(code, n, in, ntuple, nlive, out)` is called with `in = sp+0x230` and
`out = sp+0x30`, `ntuple = 64`. So the 768-byte write to `out` covers
`[sp+0x30, sp+0x330)` — **exactly `pout` ∪ `pin`**. The 256-byte overrun lands entirely
inside `pin`, which is dead after the call, and stops **8 bytes short of the canary**.
It cannot reach the return address. The 512-byte read from `in` runs to `sp+0x430`, 256 B
past `pin`, over the canary, the saved registers and the caller's frame — read-only, and
it only feeds garbage into a warm-up buffer whose result is discarded.

So the survival is a property of clang's layout choice on this target, not of the bug
being harmless. ASan reverses the order (`pin` at `[48,304)`, `pout` at `[368,880)`) and
therefore catches it immediately, on the **read**, before the write is ever attempted.
Post-fix the frame is `sub sp, sp, #0x540`, exactly 512 B larger, as the slot counts
predict.

The pre-fix binary is also **behaviourally identical**: over 120 files from `tests/exec`
compiled `-O2 -c` with both ladder gates on, **111 objects byte-identical, 0 differing**;
the 9 remaining are x86/Windows-specific sources that fail to compile identically under
both. N6 was real, and on this target it was latent.

### The `gpu/*` cells — seven, not four, and `MCC_GPU_REQUIRED` exposes nothing

PLAN's "Darwin now registers all four `gpu/*` cells" is **stale**. Measured:

| build | cells | which |
| --- | ---: | --- |
| `cmake-mtl` (metal) | **7** | `ladder-gpu-parity`, `spv-slice-{differential,known-positive,real}`, `msl-slice-{differential,known-positive,real}` |
| `cmake-mvk` (vulkan) | **4** | the same minus the `msl` trio |

It is seven and not eight because `ladder-gpu-parity` is one cell shared by both arms.
The vulkan arm gets four because **`MCC_GPU_LANG_MSL` is derived from the backend, not a
cache option** — `-DMCC_GPU_LANG_MSL=1` on a `MCC_GPU_BACKEND=vulkan` configure is
silently discarded with an `unused-cli` warning and the MSL emitter stays off. Worth
knowing before someone tries to get both gates out of one build dir.

All 7 pass in `cmake-mtl`. Re-run in a separate `cmake-mtl-req` configured with
**`-DMCC_GPU_REQUIRED=ON`**: **all 7 pass again.** The option's only reach is the
`-DMCC_GPU_REQUIRED=` it forwards to four `cmake -P` cells, and on a host with a real
device it turns no skip into a failure because **nothing was skipping**. Its value here
is the negative result: it proves the cells exercised the device rather than quietly
returning early.

### The suite — partial, one real failure, and 70 budget timeouts

The full `ctest` on `cmake-mtl` was killed by the reboot at **7,986 of 8,906**:
**7,487 passed, 429 skipped, 70 timeouts, 0 failures**. A resumed pass over the
non-`flagsweep` remainder (`-I 7967,8906 -LE flagsweep`) reached 792 of 824 before it too
was stopped, and found **exactly one failure**.

**`runtime-bench-check` — a real arm64 Darwin divergence, and not a GPU one.**

```
FAIL branchy [defaults]: output mismatch
    want: branchy 487419720 122294685.000000
    got:  branchy -7621192680 -921869400.000000
```

`tools/runtime-bench.py` takes `expect` from running the **reference compiler's** binary,
so this is mcc disagreeing with the host `cc` on `tests/runtime/branchy.c`, in both the
`long` accumulator and the `double`. The gate set is `[defaults]`, i.e. **every GPU env
gate off**, and `ast_ladder_gpu_setup` returns before its first statement in that
configuration, so the N6 line cannot be implicated. **Classified as a genuine codegen
defect, unrelated to the GPU work.** It was *not* reproduced against a non-GPU build, so
"pre-existing" is an inference from the gate state, not a measurement.

**70 × `flagsweep-exec/*` `***Timeout` at 300 s — a budget artifact, not a defect.**
Zero timeouts occurred anywhere outside `flagsweep-exec`, and **0 of the 114 cells
passed**, which is the shape of a budget problem rather than a flaky one. The cells carry
a hard `TIMEOUT 300`. Each does 6 `corpus_run`s at `-O1/-O2/-O3` × on/off, and each
`corpus_run` builds, links and runs every subject **twice** (an `-O0` reference and the
flagged build) — 12 such halves. One half measured **32.7 s for 24 buildable subjects**,
putting the intrinsic cost at **~392 s against a 300 s budget** before any contention.
`PIN` is also empty on Darwin by design, since there is no `taskset`, so the Linux
one-core pinning that the budget was presumably calibrated against does not apply.
**The decisive measurement — one cell, standalone, on an idle machine — was not taken**,
so "the budget is too small on this host" remains the leading explanation and not a
proven one.

### Not measured in this pass

The full suite never completed on either build dir: **920 tests of `cmake-mtl` were never
run**, and the resumed pass left 32 of its 824. **The `cmake-mvk` suite was never run at
all** — that build was configured and built, and only `ladder-gpu-parity` was executed
against it, so the Metal-vs-MoltenVK comparison above rests on the two gate binaries and
that one cell, not on a suite-wide diff. The remaining 44 `flagsweep-exec` cells did not
run. GPU init cost per process was attempted but every sample was taken under contention
severe enough that the *baseline* varied 4× (31.5 vs 7.5 ms/run for the same no-GPU
workload), so **no init-cost number from this pass is bankable** and the Linux 135 ms
figure still has no Darwin counterpart. `runtime-bench-check` was not bisected and not
run against a non-GPU build.

## Where RIR replay accuracy stands — 2026-08-06

RIR is **two layers**, and conflating them is what made the older boards unreadable. The
op-stream capture is the ground truth; the arena is a second derivation built from it, and
only the arena has ever had a gap.

| layer | coverage, by emitted body bytes |
| --- | --- |
| capture (`rir_verify`, `MCC_REPLAY_IR=3`) | **100.000%**, `fallback=0`, on both corpora |
| arena (production, `MCC_RIR_PROD`) | **100.000%** self at `-O0`/`-O1`, **99.965%** at `-O2`/`-O3`, residual 0 |

Measure it with `ctest -R rir-coverage`; the numbers are banked in
`tests/rir/coverage-bank.json` and ratcheted. **The ratchet gates percentages and the
residual, never body totals** — the census body total is not stable run-to-run at fixed
HEAD (two consecutive full-suite runs gave `used=95823` and `used=95971` while every class
count was byte-identical), so compare class counts, never totals.

Refusal classes closed on 2026-08-06: `asm`, `regdangle`, `bail`, `mismatch` — all report
`used` now. `noops` is 338 provably empty bodies (`len=0 rirn=0 capn=0`), which is not a
gap; `revargs` is one body, assessed and deliberately left.

**The `abort` class is closed for the lost-cast family — 2026-08-06.** A cast that changed
the static type but emitted no machine code left no trace in the arena, so a chain like
`((Sym *)(uintptr_t)x)->v` handed the replay's `->` an integer and it died with
`pointer expected`. `rir_hook_cast_type()` now records the post-cast `CType` (`t`, `ref`,
`bp`/`bs`) as a `RIR_M_CASTT` mark at every explicit cast, and `rir_to_arena` turns it into
an `AST_Convert` carrying the *final* static type of the chain, not the type of the last
code-emitting cast. Self corpus: gap 5411 B / 5 bodies → **0 B at `-O0`/`-O1`**, 5834 B / 6
bodies → **467 B / 1 body at `-O2`/`-O3`**, and `kept` rose 89.8% → 96.0% because the
restored pointer type also makes the replay pick the same code paths.

**What is left is one body, 467 B, and it is not a type.** `host_runmem_alloc` in
`src/mcchost.c` aborts with `ast-replay: storeval-arg stack underflow`: `ast_storeval_*`
marks its `AST_Invoke` `AST_FB_CALL_STOREVAL_ARG`, and at replay the vstack holds fewer
live entries than the `vrotb(3)`/`vswap()` rotation needs. That AST pass only runs from
`-O2`, which is why the class vanishes at `-O0`/`-O1` and why `tests/rir/gap/abort.c`
carries a `rir-gap-levels: O2,O3` marker. Reduced reproducer: `p = f(n += g());`.

Fallback rows in the census now carry `first`/`end`/`blen`/`nlen`, which localises a
divergence to a byte span within the body — the standing "fix at the USE site" item needs
exactly that. Example row: a 479-byte `host_runmem_alloc` diverging only at bytes 428–457.

**Assembly is modelled, not opaque.** `asm goto` labels, `volatile`, and the
`"memory"`/`"cc"`/`"flags"` clobbers were all being discarded and are now captured; the
effect set (`reads`/`writes`/`clobbers`/`mem`/`pins`) is decoded onto the node and eight
strategies are re-admitted to asm-bearing bodies. The right mental model is a **pin**, not
a barrier: `subst_asm_operands` bakes concrete registers in at parse time, so `narrow`,
`ltemp`, `sethi`, `reg-color` and promotion break an asm without moving anything across it.

## The slice equivalence oracle is an exhaustive width ladder — 2026-08-06

`ast_eval_slice_equiv` in `src/ast_eval_slice.h` used to sample eight arbitrary points
(`{0, 1, -1, 2, 7, -3, 100, 12345}`, rotated across the live-ins) and return
`evaluated > 0` — it certified equivalence off **one** successful evaluation while the
other seven may have bailed. Behind `MCC_AST_EVAL_LADDER=1` (default OFF) it is now an
exhaustive ladder. The old sampler is still there as `ast_eval_slice_equiv_seed` and is
what runs with the toggle off; `src/mcc.c` objects are byte-identical to the unpatched
compiler at `-O0`/`-O1`/`-O2`/`-O3` with the toggle off, proven with `cmp`.

**The ladder.** Live-ins are collected with their declared type. Rungs, climbed in order:

| rung | domain | certifies? |
| --- | --- | --- |
| `const` | all live-ins zero, 1 evaluation | only when there are no live-ins |
| `w1`..`w32` | every tuple of `e_i = min(w, typewidth_i)`-bit patterns | yes, if the rung completes |
| `corners` | `{0, 1, -1, TYPE_MIN, TYPE_MAX, TYPE_MAX-1}` per live-in | no — falsifier only |
| `observed` | real argument tuples recorded by the JIT | yes, but never `exact` |

A `w`-bit pattern is **sign-extended then fitted to the live-in's type**, so an unsigned
live-in gets both the zero neighbourhood and the `2^n - 1` neighbourhood at every rung —
the carry boundaries `seed[8]` never reached. The rungs nest, so the first rung that
disagrees is the *smallest* width at which the two forms differ, and that is what the
oracle reports. When `e_i == typewidth_i` for every live-in the rung is exhaustive over
the true input space and the verdict is marked `exact`; the ladder then stops early.

**A rung completes or nothing is certified.** A point where the *source* form is
undefined (signed overflow, division by zero, out-of-range shift) is vacuous — undefined
behaviour constrains nothing — but a point where the source is defined and the
replacement is not counts as a difference. A rung with zero non-vacuous points refuses
(`all-undefined`). A rung that does not fit the budget is not partially run.

**Budget.** `MCC_AST_EVAL_LADDER_BUDGET` (default `1<<20` points) caps a rung. The
2^32 rung is a GPU rung: on this CPU a single `n=1`, `w=32` sweep of `int x*2+1` against
`x+x+1` is 4,295,033,110 evaluations in **6m03s** (~85 ns/point, two tree walks each).
At the default budget an `n=1` slice therefore stops at `w16` and an `n=2` slice at `w8`.
Nothing above the budget is guessed: the fallback is `ast_slice_ladder_observed_source()`,
wired in `mccjit_kernel_search_from_blob` to `MccjitIntent.param_off[]` +
`MccjitCounterState.sample[]`/`argmin`/`argmax` — the JIT's own recorded argument tuples.
With no source registered the verdict is `refused over-budget`, never a certification.

**Measured over a full `src/mcc.c` self-compile** (`MCC_AST_EVAL_LADDER_CENSUS=1`, `-O2`,
default budget; 19,206 slice-vs-copy pairs and 81,361 cross pairs between distinct slices
of the same body):

- certifiable, self pairs: seed **19,193 / 19,206 (99.93%)**, ladder **18,940 (98.62%)**.
  Decided at `const` 2,442, `w2` 1, `w4` 259, `w8` 2,723, `w16` 13,515, `w32` 0 — the
  13,515 at `w16` stop there only because 2^32 exceeds the CPU budget. 2,616 are `exact`.
- **the old oracle was certifying non-equivalent forms.** Of the 16,122 cross pairs
  `seed[8]` called equivalent, the ladder refutes **1,449 (9.0%)** with a counterexample:
  smallest differing width `w1` 203, `w2` 6, `w4` 273, `w8` 674, `w16` 230, corners 63.
  The 3 is the tell — `seed[8]` contains no 3, so `x == 3` versus `0` certifies clean and
  the ladder refutes it at `w4`. That case is `jit/selftest-sliceladder`.
- refused for want of a type: **252 self pairs (1.31%)** and 1,794 cross pairs (2.20%)
  have a leaf — live-in, literal or convert — with no static type. Under
  `MCC_AST_EVAL_LADDER_STRICT_TYPE=1`, which also rejects an interior node whose
  arithmetic width had to be inferred from a child, that becomes **896 (4.67%)** and
  6,263 (7.70%). Those are the numbers the arena `CType` stamping should drive to zero.
  Today the width is inferred for 2,088 interior nodes across 896 self pairs.
- cost: at the default budget the oracle is **1.5 ms per self pair / 199 us per cross
  pair**, 45.3 s of oracle time on a self-compile whose baseline is 0.37 s. At
  `MCC_AST_EVAL_LADDER_BUDGET=4096` it is **13.2 us / 1.7 us** and adds 0.43 s to the
  whole self-compile for 100,564 pairs.

**No GPU path is wired.** `tests/gpu/` has a fixed-shader Vulkan harness, not a general
AST interpreter; emitting a compute shader per slice at compile time is the open work.
Everything above runs on the CPU and the default suite is green with no GPU present.

## Slices between anonymous invokes — the A1 census, 2026-08-06

The GPU-offload question is "what is the largest chunk of a body that runs without ever
calling out". `MCC_SLICE_CENSUS=<path|->` makes the compiler answer it, one line per
slice and one per body, and `tools/slice-census.py` aggregates it.
`ctest -R slice-enum` verifies the enumerator (0.1 s); `ctest -L census -R slice-census`
runs the full sweep.

**The boundary.** A slice is a *maximal run of consecutive statements inside one statement
list whose subtrees contain no opaque `AST_Invoke`*. Maximal in both directions: the
neighbouring statement has a call, and the parent statement has a call. A call-free loop
is not split out — it is an ordinary statement, so a call-free loop nest comes through as
one slice. **Statement-granular, not expression-granular**: the call-free half of
`a[i] + h(s,i)` is not a slice. That case is exactly what the pre-existing machinery in
`src/mccast.c` already does — `ast_slice_win_root_ok` roots an expression *window* at a
Binary/Unary/Convert/Load, `ast_slice_ident_hash` gives it an alpha-equivalent identity,
and `-fopt-slice` (rung 10) memoises those windows to a disk cache for JIT specialisation.
Windows are 3..65536 nodes of pure integer arithmetic and are a different object from a
slice; nothing here duplicates them.

**Anonymity is measured, not assumed.** An `AST_Invoke` carries the callee as child 0.
Four classes, counted per body: `indirect` (child 0 is not a `Ref` to a `VT_FUNC` `Sym`),
`opaque-direct` (a `Sym` with no body retained in this TU), `retained` (in the graft pool,
not graftable) and `graftable`. The first two are the truly anonymous ones. Two partitions
are reported side by side: `t=0` treats every invoke as a boundary, `t=1` lets graftable
invokes be transparent, because from `-O2` the reemit path really does graft them (checked:
`-O2` and `-O3` both emit zero `call` for a static callee in the pool). `t=1 - t=0` is the
inlining effect and nothing else.

**Byte extents are measured, not modelled.** `ast_replay_bb` brackets each statement's
replay with the code index `ind`, so a slice's byte extent is the sum of what its
statements actually emitted. It reconciles to 99.995% of body bytes on `src/mcc.c` at
`-O1..-O3` and 100.053% at `-O0`; the drift is peepholes that rewind `ind` and so
double-count a retracted byte. The gate allows `max(32, bytes/50)` of slack per body.
The measured replay is the faithfulness replay, where `ast_inline_active` is 0, so both
partitions are attributed against the *un-grafted* emission: `t=1` says how many of the
caller's own bytes stop being cut by a boundary, not what the post-inline body would
measure.

**Headline, `src/mcc.c` amalgamated, x86_64.** 2589 modelled bodies, 1,323,733 body bytes.

| | `-O0` | `-O1` | `-O2` | `-O3` |
| --- | --- | --- | --- | --- |
| anonymous invokes | 100.0% | 100.0% | 84.6% | 84.6% |
| slices (`t=0`) | 11847 | 11847 | 11847 | 11847 |
| body bytes in a slice (`t=0`) | 41.42% | 41.44% | 41.44% | 41.44% |
| body bytes in a slice (`t=1`) | 41.42% | 41.44% | **44.68%** | **44.68%** |
| slices (`t=1`) | 11847 | 11847 | 11628 | 11628 |

**`-O` barely moves this.** The production arena handed to `ast_func_end` is the same at
`-O1`, `-O2` and `-O3` — the strategy passes run after it — so the `t=0` partition is
identical at all three. The only thing `-O` changes is graftability, and grafting every
graftable callee buys **3.2 points of body bytes** (548,599 → 591,386 B) while *reducing*
the slice count by 219. That is the whole inlining effect, and it is small because 84.6%
of invokes remain anonymous after it: mcc calls out to libc and to non-static helpers far
more than it calls static leaves.

**The shape is the result, and it is discouraging.** Of 11847 slices at `-O2`: 46% are
≤4 nodes and carry 9.6% of slice bytes; 65% are ≤8 nodes. The tail is thin — 292 slices
(2.5%) are ≥65 nodes, and they carry 26.0% of slice bytes. Only **512 slices (4.3%)
contain a loop at all**, and those hold **9.58% of body bytes**. Slices at loop depth ≥1
are 3728 of 11847. The largest are real but few: `cplx_const_int_div` (956 nodes, 8 loops),
`ast_opt_defaults` (803), `gen_opq_fold` (560), `ast_eval_binop` (495). 19.2% of bodies are
wholly call-free but they are small — 5.47% of body bytes.

`--corpus exec` (301 files, 1249 bodies) is *worse* and should not be read as a second
opinion: 96.7% of its slices are 3–4 nodes, because the corpus is assert-and-`printf`
straight-line code. Its one big slice, `classify` at 5128 nodes / 27,339 B, is a generated
table.

**Read this as a negative result with a narrow positive.** Straight-line call-free code is
plentiful (41% of bytes) but atomised into fragments too small for a kernel launch. The
prospects for a shader live entirely in the ~500 loop-bearing slices holding ~9.6% of body
bytes, and the next question for anyone continuing is whether those 500 are
*data-parallel*, not merely call-free — `ast_loop_analyzable` / `ast_loopdep_dump` already
answer that per loop and were not consulted here.

**Known limits of the enumerator.** A loop's *condition* is not a statement list, so a
call-free condition inside a call-containing loop is never a slice. Bodies the arena never
modelled emit no record, so `fn_n` is the modelled population, not everything compiled
(at `-O0` the arena is off entirely unless `MCC_FORCE_REPLAY=1`, which the tool sets). A
statement list wider than 512 statements is flushed in chunks and flagged `ovf=1`; that
never fires on either corpus.
## A slice actually ran on the GPU — 2026-08-06

`src/mccspv.h` emits a SPIR-V 1.3 compute module from an arena subtree plus its
live-in slot offsets; `tools/spvgate.c` dispatches it and compares against the
real `ast_eval_slice`. The node set is exactly `ast_eval_slice_rec`'s — Literal,
Ref, Load, Convert, Unary, Binary, If — and integer widths ≤32, which is what the
width ladder certifies. `ast_eval_slice_is64` types are refused, not approximated.

RTX 5070 Ti, 11 cases, 44 modules, 723,932 lanes: **666,450 compared points,
0 mismatches**, 44/44 pass `spirv-val`. Sweeps are exhaustive per ladder rung
(1/2/4/8/16 bits). Points where `ast_eval_binop` reports the source undefined
(signed overflow, `/0`, `INT_MIN/-1`, out-of-range shift) are *vacuous* and
excluded — 57,482 of them — so the comparison covers only what C defines.

**Short-circuit is control flow, not `OpSelect`.** `a ? b/c : d` and n-ary
`&&`/`||` emit `OpSelectionMerge`/`OpBranchConditional`/`OpPhi`, phi predecessors
taken from the *last* block of each arm. Relying on an untaken arm's division
being discarded is not the same as not computing it.

**Undefinedness is modelled, not taken on the CPU's word.** The shader mirrors
`ast_eval_binop`: signed overflow on `+`/`-` via `((a^r) & (b^r)) < 0`, on `*`
via `r/a != b` guarded at `a == 0`, `/0`, `INT_MIN/-1`, and shift counts outside
`[0,32)`. The flag rides in `SpvMod.def`, is phi'd through both arms of every
`If` and `&&`/`||` link, and lands in `out[2*lane+1]` beside the value. The gate
requires it to agree with `ast_eval_slice`'s verdict, so the 3.7M vacuous points
are *verified* rather than skipped.

**A flag computed beside a poisoned invocation is worthless.** The first version
computed the flag correctly and still performed the undefined division in the
same invocation; SPIR-V leaves that undefined, and the driver returned
`defined=1` for `0/0` while the disassembly plainly showed `defined=0`. Divisors
and shift counts are now guarded through `OpSelect` so the kernel never executes
UB at all. `divraw`/`remraw`/`shiftraw`/`ovf`/`ovfadd` exist to exercise this;
the older synthetic set had *no* undefined points once `cmp` stopped being a
shift (vacuous 57482 → 0), so it could not have caught any of it.

Three defects the differential caught, none of which a passing test would show:

- the entry point listed its StorageBuffer variables, legal only from SPIR-V 1.4;
  at 1.3 the module is invalid and the driver may do anything — this produced the
  first wrong answers;
- `OpUDiv`/`OpUMod` require an unsigned result type, and emitting them with `%int`
  **still returned correct answers** — only `spirv-val` caught it;
- signed `%` is synthesised as `a - b*(a/b)`, mirroring `ast_eval_binop`, rather
  than trusting `OpSRem` at `1 % -1` and `-8 % 3`.

**Real slices, not hand-built ones.** `MCC_ARENA_DUMP=<path>` serialises every
modelled body's production arena at the same hook `MCC_SLICE_CENSUS` uses;
`spvgate --arenas` rebuilds them and walks top-down for maximal subtrees the
emitter accepts with ≤4 live-ins. The hook is inert when unset — 0 per-test
verdict changes over 4907 cross-oracle cases.

| corpus | slices | compared | clean | mutated |
| --- | --- | --- | --- | --- |
| gcc c-torture, **all 1693 files** | 25,877 (12,157/16,208 bodies) | **1,695,607,075** | 0 | — |
| gcc c-torture, 300 files | 858 (460/1910 bodies) | 53,678,914 | 0 | **all** |
| compiler-rt builtins, 96 | 401 (73/192 bodies) | 23,174,609 | 0 | **all** |
| synthetic, 13 cases | — | 855,556 | 0 | **all** |

The full sweep is 127,355 GPU dispatches over 1,699,304,114 lanes, 0 mismatches
and 0 rejected modules. **75% of modelled bodies (12,157 of 16,208) contain at
least one slice the emitter lowers** — far above the 24% the first 300-file
sample suggested, so the alphabetical head of that suite is not representative.

**`TOK_SHL` is `'<'` (60) and `TOK_SAR` is `'>'` (62); comparisons are
`TOK_LT`=156, `TOK_GT`=159.** Real slices produced 131,627 mismatches on first
run, all one shape: `TOK_LT`/`TOK_GT` returned signed comparisons unconditionally
while `TOK_LE`/`TOK_GE` respected `uns`. `ast_eval_binop` compares *after*
`ast_eval_narrow`, which zero-extends unsigned types, so an unsigned `TOK_LT`
carries unsigned semantics. **The synthetic `cmp` case never caught this because
it was written with `'<'` and had therefore been exercising a shift, not a
comparison.** That is the argument for real corpora in one sentence.

**`gpu/spv-slice-known-positive` and `gpu/spv-slice-real` are the cells that
matter.** They require the mutated build to FAIL, reject exit 77 (a skipped
known-positive is not a pass), and `-real` additionally fails when *zero* slices
lowered — a differential over an empty set is green for the same reason a blind
one is.

**The first known-positive was worse than useless and cost two defects to
discover.** Rewriting every `OpIAdd`→`OpISub` also rewrote `spv_load_live`'s
addressing, so indices went negative, the shader read far out of bounds, the
device faulted, and every later allocation returned
`VK_ERROR_OUT_OF_DEVICE_MEMORY`. It was testing error handling, not detection —
and found `gpu_run` leaking buffers/memory/descriptor pools on both early-return
paths, and a rejected module being a silent `continue`, so a mutation producing
invalid SPIR-V would have been reported OK. Rejects now count as failures, every
path frees, and the mutation is `OpBitwiseXor val, 1` on the result before the
store: touches no addressing, cannot fault, perturbs every point. Detection went
from 6 of 11 cases to total.

**The trap for anyone extending this.** `mccast.c`'s fallback `ast_eval_slice`
stub returns 1 *without writing `*out`*, so the obvious build compares
uninitialised memory and passes everything. `spvgate` hard-errors unless
`AST_EVAL_SLICE_PROVIDED` is set. Two TUs are needed because `mccast.c` compiles
standalone only *without* `mcc.h` while the evaluator needs it for `VT_*`.

## The ladder oracle replays on the GPU — 2026-08-07

`ast_eval_ladder_rung()` delegates its replay to the device when `MCC_GPU` is
compiled in (CMake option, **ON** by default since the driver is `dlopen`ed —
see "`MCC_GPU` is ON by default" below) and `MCC_AST_EVAL_LADDER_GPU=1` is set. Both arenas are lowered and
dispatched over the rung's whole tuple space; the verdict is rebuilt on the host
from the returned `(value, defined)` pairs. **Only the replay moves — the verdict
logic is not reimplemented on the device.** The rung falls back to the scalar
loop whenever either arena will not lower, the live-in count is out of range, or
the space exceeds 2^20, so the GPU path is an accelerator, never a coverage
change. `gpu/ladder-gpu-parity` pins it: verdicts identical GPU-vs-CPU, 1404
dispatches, 0 differing files, and **zero dispatches is a hard failure** because
identical verdicts prove nothing if the GPU never ran.

**Cross-oracle with the GPU live during every compilation.** 4907 cases, gcc
judged by clang and clang by gcc, census mode on so the GPU actually replays:
**0 behavioural changes** against the CPU baseline, `DIFF_EXIT` 53 and
`DIFF_STDOUT` 19 identical. The only 5 transitions are `PASS -> MCC_NOBUILD`,
all compile `timeout` on the huge generated `memclr`/`memcpy-a*` bodies, which
census makes slow. Nothing miscompiled.

**The JIT's own path reaches the device, not just the census.** Every production
caller of `ast_slice_equiv` is in `mccjit_embed.c`, which links `libmcc`, so
`MCC_GPU` applies there too. `jit/selftest-sliceladder` — a JIT code path, not a
diagnostic — replays on the GPU: 30 dispatches, 263,312 lanes, and its output is
**byte-identical** to the CPU run.

That byte-for-byte diff caught what the cell could not, because the cell pins
verdicts and not values. The GPU path stored the raw `int32` into
`res->diff_a`/`diff_b` where the CPU stores the value fitted to the source type;
for an unsigned root that is a zero-extension, so one counterexample printed
`a=4294967295` on the CPU and `a=-1` on the GPU. **The verdict was right and the
diagnostic was wrong**, which is the worse way round — a counterexample is what a
human reads when the oracle refutes something. Both are now fitted through
`ast_eval_slice_fit` with the root's own width type, and the rung refuses the GPU
path when either root has no static type rather than guessing one.

## The runtime JIT never consults the oracle — measured, 2026-08-07

**`mcc` cannot currently be shown to run RIR replays on the GPU during ordinary
compilation or ordinary JIT execution, and the reason is structural rather than
configurational.** This was chased all the way down; the negative result is worth
more than the knobs tried.

`ast_slice_equiv()` — the only entry to the ladder, hence the only route to the
GPU — is reachable in production from exactly one place: `mccjit_slice_search()`
→ `mccjit_kernel_search_from_blob()`. Everything else calling it is a selftest.
And `mccjit_slice_search` opens with

    if (!async || mccjit_last_allfp || mccjit_last_ret_wide || np < 1 || np > 3
        || st->nsample <= 0 || !st->blob) return NULL;

where `np` is `mccjit_last_nparam`, a **global set by a previous build**. On a
slot's first promotion nothing has populated it, so the guard rejects before
`mccjit_kernel_search_from_blob` is ever called. The runtime swap path instead
verifies with the KGC scalar/purity check, which never asks the ladder anything.

Confirmed end to end on a program built `--embed-jit --jit-threads 2 -lvulkan`,
with a `noinline` hot pure scalar function so calls actually reach the slot:
pool starts (`live=2`), stub installs, threshold trips, `promote-async` fires,
the variant is promoted `route=kgc` and the answer is correct — and
`[ladder-gpu]` prints **zero** times. The same binary prints 30 dispatches when
the oracle *is* called, so the backend is live and the absence is real, not a
misconfiguration. Knobs tried without effect: `MCC_JIT_LAZY`,
`MCC_JIT_SEARCH_SLICE`, `MCC_JIT_SEARCH_MS`, `MCC_JIT_HOT_CALLS`,
`--jit-threads`.

Two consequences, and neither is a measurement problem:

1. **The cross-oracle run needs `MCC_AST_EVAL_LADDER_CENSUS=1` for the device to
   do any work at all.** Without it the same green board appears with the GPU
   idle, so census is what makes that run mean anything — and census is a
   diagnostic, not production.
2. **Making the claim true is a JIT design change**, not further instrumentation:
   the promotion path would have to verify candidates through the ladder instead
   of, or in addition to, the KGC check. That is a behavioural change to how the
   JIT admits variants and is deliberately not attempted here.

`-O2` and `-fopt-slice` dispatch nothing either, for the same reason: neither
consults the equivalence oracle.

**Two emitter defects surfaced only here, and `spirv-val` accepted both.** They
crashed NVIDIA's SPIR-V compiler (`libnvidia-glvkspirv`) with SIGILL:
`OpBitwiseOr %int` fed a `%uint` operand to convert the invocation id (legal,
but `OpBitcast` is the idiom), and the signed multiply-overflow check emitted
`r / select(a == 0, 1, a)`, which becomes a wholly constant-foldable division
when `a` is a literal — `9 * x`, a shape the synthetic cases never produced
because they all put the constant second. It is now `OpSMulExtended` with a
high-word test and emits no division at all.

**A dispatch cap was added as a "mitigation" and then reverted, and that is the
lesson.** The crash correlated with dispatch count on the first file, so a cap
looked principled; `MCC_AST_EVAL_LADDER_GPU_MAX=1` still crashed on
`vla-stexp-9.c`, refuting it in one command. Shipping the cap would have buried
two real emitter bugs behind a magic number.

**Still not claimed.** Everything in the section above runs through `spvgate`
offline; the ladder path is the only thing the compiler itself dispatches.

### The runtime JIT does carry the backend — 2026-08-07, superseding the below

The crash was root-caused and fixed; the section that follows is kept for the
eliminations it records, but its conclusion is obsolete.

**Cause: the NVIDIA driver initialises its shader-compiler state lazily, and if
that first touch happens on a JIT worker thread it comes up broken** — the NULL
context at `cmp 0x10(%r8)` in libnvidia-gpucomp. Seventeen isolated probes all
came back clean because every one of them did its first dispatch on the main
thread. `ast_ladder_gpu_setup()` now dispatches one trivial module at JIT boot
from `mccjit_boot_swap_async`, on the main thread before the worker pool starts.
`pr50729` goes 4/40 (40/40 under dlopen) to **0/40**.

Two further defects, both found by the cross-oracle rather than by inspection:
the warm-up raised `FE_INEXACT` and broke `builtin-fp-int-inexact`, which checks
FP exception flags — `mcc_gpu_run` now saves and restores `fenv_t`; and
`mccjit_slice_search` gated on `mccjit_last_nparam`, a global set by a *previous*
build, so the production slice-search path was dead code on a first promotion.

**Result.** Cross-oracle over 4907 cases with `--embed-jit --jit-threads 2` and
the GPU live in compiled programs: **0 behavioural regressions**, `DIFF_EXIT` 53
and `DIFF_STDOUT` 19 matching the CPU baseline exactly.

**How much GPU work the suites actually do, measured rather than assumed.** Over
a 575-program random sample, **3 programs fire ladder rungs** (6 rungs); the rest
of the 312 dispatches are the one-per-process warm-up. `pr50729` fires in
**20/20** runs (81 rungs) and passes 20/20. So the suites do exercise GPU replay,
but in well under 1% of programs — most gcc tests have no function meeting the
slice-search criteria, and lowering `MCC_JIT_HOT_CALLS` to 2 does not change
that. Quote the 3-in-575 figure, not the dispatch count, which is dominated by
warm-ups.

### The runtime JIT cannot carry the backend yet — 2026-08-07 (superseded)

Wiring `MCC_GPU` into `libmcc_jitengine` does make a compiled program's runtime
JIT replay on the device, and it works: `--embed-jit --jit-threads 2` on a
`noinline` hot pure function gives 5 rungs / 10 dispatches / 131,628 lanes with
the answer matching gcc, clang and the JIT-off build. **It is off anyway,
because it corrupts memory.** `pr50729.c` segfaults **4 of 40 runs** with the
backend present and **0 of 40** without; `pr100499-1.c` did too until the mutex
and exit-quiesce landed, and is now 0/40.

The fault, caught under gdb: `cmp 0x10(%r8),%rdi` inside `libnvidia-gpucomp.so`
with `si_addr = 0x0` — the driver dereferences a **NULL context pointer**, which
is per-thread/per-instance driver state being absent rather than anything wrong
with the shader or our own allocations.

Eliminated, each with a decisive test, so nobody repeats them:

| hypothesis | test | result |
| --- | --- | --- |
| invalid shader | `spirv-val` + standalone dispatch of all 10 modules | all valid, all `rc=0` |
| data race | mutex + exit quiesce | fixed `pr100499-1`, not `pr50729` |
| stack exhaustion | 64MB worker stack | no change |
| Vulkan-in-thread under mcc | threaded probe, gcc vs mcc | both clean |
| lazy init inside worker | eager main-thread init | no change |
| static TLS surplus | `GLIBC_TUNABLES` | no change |
| dispatch count / module order | cap sweep | independent |
| JIT-worker environment | dedicated GPU dispatch thread | 6/40, no change |

**Narrowed to one call.** Instrumenting every step of the dispatch puts the
fault exactly at `vkCreateShaderModule` -- the driver compiling the SPIR-V,
which is what the `libnvidia-gpucomp` backtrace already said. The bytes are
*verified intact at that call site*: magic `07230203`, 223 words, hash printed
immediately before the call. The identical module compiles and dispatches fine
from a standalone gcc-built process (`spvgate --spv`). So the driver faults
compiling a valid module, and only inside an mcc-produced executable.

Two further eliminations from that round: the SPIR-V buffer is **not** corrupted
by using the compiler's allocator off-thread -- switching the GPU path to plain
`malloc` changes nothing (40/40) -- and `dlopen`ing libvulkan instead of linking
it makes the crash *deterministic* (40/40 vs 4/40) under both `RTLD_LOCAL` and
`RTLD_GLOBAL`. The deterministic `dlopen` build is therefore the reproducer to
debug with; it is far easier to work with than the 10% linked case.

**Nor is it threading.** The same mcc-built probe, moved into a spawned pthread
so it matches the failing context, still makes 50 clean `vkCreateShaderModule`
calls on the crashing module -- as does the gcc build. Combined with the result
below, the isolation is complete: not the module, not the executable, not the
thread. The only thing left that differs is that **the JIT is running in the
process**, and it crashes before it has reemitted anything.

**It is not mcc's code generation.** The obvious suspicion -- that mcc-produced
executables are somehow malformed in a way the driver trips over -- is wrong. A
small program compiled *by mcc* that loads the exact crashing module and calls
`vkCreateShaderModule` on it 50 times completes cleanly, as does the same
program built by gcc. So the executable is fine and the module is fine; what
differs in the failing case is that **the JIT has been running in the process**.
That points the remaining investigation at what the JIT does to process state --
`host_runmem_alloc` and the `mprotect` of code pages, the hot-patch, the fork
handlers -- and away from codegen and from the shader, which is where the first
several rounds of this were wasted.

**Tooling limits, so nobody wastes time on them.** glibc's heap checkers
(`MALLOC_CHECK_=3`, `MALLOC_PERTURB_`) report nothing and the crash is unchanged,
so this is not heap-metadata corruption. **Valgrind cannot run mcc-produced
executables at all** -- it dies with SIGILL in `_dl_start` before `main`, so the
obvious instrumentation is unavailable here and that is worth knowing before
reaching for it.

At the crash point the JIT has done: lazy stub install (an RWX mmap with
hand-assembled machine code), counter ticks, blob deserialize and arena build,
then the ladder. It has *not* yet reemitted a kernel -- `ast_slice_ladder_explain`
runs inside `mccjit_kernel_search_from_blob`, before `mccjit_slice_search`
reemits. So the suspect set is small: the stub's RWX mapping, `host_runmem_alloc`,
and the arena work.

**The stub's RWX mapping is not it either.** A probe that mmaps a 4096-byte
`PROT_READ|WRITE|EXEC` page, writes to it, and then compiles the crashing module
50 times in a spawned thread runs clean under both mcc and gcc. So the suspect
set is down to `host_runmem_alloc` -- which may not even have run at the crash
point -- and the embedded front-end itself: `mccjit_kernel_search_from_blob`
calls `mcc_new()`, `mccpp_new()`, `mccgen_init()` and `mcc_enter_state()`, so a
whole compiler instance is initialising in that worker thread with mcc's global
state and allocator, alongside the driver. That is where to look next.

**The front-end in a worker thread is not it either.** A probe linking `libmcc`
that creates and destroys four `MCCState` compiler instances *in a spawned
thread* and then compiles the crashing module 50 times in that same thread runs
clean. So mcc's global state and allocator being live in the thread the driver
compiles on is not sufficient to trigger it.

Seventeen things are now excluded and none of them is the cause, which is worth
saying plainly: the remaining difference between the clean probes and the
failing program is the *whole* JIT promotion flow running -- counter stub,
counter ticks with argument capture, blob deserialize, `ast_slice_search`,
purity/certifiability analysis, the ladder, and `host_runmem_alloc` -- and no
single piece reproduced alone. The next step is bisecting the flow itself by
disabling stages, not by guessing at mechanisms.

**Measure with 40 runs, not 5.** Two separate times a stale embedded engine blob
made an intermittent crash look deterministic, and once made a broken build look
clean; `cmake --build` does not reliably regenerate the blob after a CMake-level
change to the engine target. Rebuild the whole project and re-measure before
believing any result in this area.

Second, independent reason it is off: linking Vulkan into the engine makes
**every** `--embed-jit` program need `-lvulkan` to link at all.

**The clean integration point is `ast_eval_ladder_rung()`, and its prerequisite
is now met.** That function is an embarrassingly-parallel loop over `space`
codes, each replaying both arenas at one point, and the JIT already invokes it
through `mccjit_kernel_search_from_blob`. It *is* bulk RIR replay, so moving it
to the GPU makes the claim literally true while doing work the JIT already wants
done — unlike speculative offload, where the census says the payoff is thin
(1.91% loop-bearing slices). It could not have been attempted before the
definedness model landed: a rung must decide vacuity itself, and until the
shader modelled UB the CPU had to visit every point anyway, which is the whole
cost the GPU was meant to remove. What remains is plumbing, not semantics —
emit both arenas, compare on device, and reduce. The obstacle is that it puts
Vulkan in the compiler's link, so it wants a `dlopen` loader behind a build gate
rather than `-lvulkan`.

Two smaller debts: `ast_bad_type` and `is_float` are duplicated in the gate
because they are static/inline inside `mccast.c`'s `MCC_INTERNAL` half, and the
gate needs two TUs because `mccast.c` compiles standalone only *without*
`mcc.h`.

## A full suite run with `MCC_GPU=ON`, and what it puts on the device — 2026-08-07

Nobody had run `ctest` against a GPU-enabled build; no build directory had the
option on. Configured one (`cmake-gpu`, Release, `MCC_GPU=ON`, embed-JIT on) and
ran all 8850 cells: **13 fail, and none of them is codegen.** Two are red at HEAD
in `cmake-release` as well and have nothing to do with the option
(`host-gate-invariant` on `mccgpu.h`'s raw `__unix__`, `trace-gate-invariant` on
13 uninstrumented bodies). The other eleven are the option's own build wiring:
`mcc` compiling `mcc` gets no `-lvulkan`, so `fixpoint-invariant`,
`selfhost-smoke`, both `selfhost-output-parity` cells and the five
`selfhost-fixpoint` cells die on `unresolved reference to 'vkCreateInstance'`,
and `rir-coverage` fails its ratchet because `MCC_GPU=ON` adds GPU source to the
self corpus and dilutes every lowerable percentage by ~0.1pp. **The ratchet is
config-sensitive; bank and check it on a default build only.**

> Every link failure in this paragraph is gone: the backend is `dlopen`ed and
> `MCC_GPU` is the default. See "`MCC_GPU` is ON by default" below. The ratchet
> sentence still holds — the default it refers to is now `MCC_GPU=ON`.

`trace-gate` is pointing at a real defect, not a style rule. `ast_opt_defaults`
opens with `#endif MCC_TRACE("enter\n");` — the instrumentation landed on the
`#endif` line of the `MCC_GPU` guard above it, so it is preprocessor trailing
tokens in *both* configurations and that function has silently had no trace hook
since. mcc itself warns about it when self-compiling; gcc does not.

**What the suite actually dispatches.** The four `gpu/*` cells are the only ones
that reach the device on their own, and they do real work: `ladder-gpu-parity`
1413 dispatches with 0 differing files, `spv-slice-differential` 72 dispatches /
1,184,616 lanes / 0 mismatches, `spv-slice-known-positive` catching the mutation
at all 1,126,578 compared points, `spv-slice-real` 1659 dispatches / 23,117,154
lanes over 355 slices from 88 bodies, 0 mismatches and 0 rejected modules. That
is ~3200 dispatches per suite run, all of it compiler-side or offline.

**The JIT selftest cells reach the device too, but only if asked.** They run
CPU-only under `ctest` because nothing sets the environment. With
`MCC_AST_EVAL_LADDER=1 MCC_AST_EVAL_LADDER_GPU=1`, `jit/selftest-sliceladder` is
15 rungs / 31 dispatches / 263,376 lanes and `jit/selftest-slicekernel` is 12
rungs / 25 dispatches / 271,652 lanes, both **byte-identical** to their CPU runs.
`jit/selftest-slicereemit` never asks the ladder anything.

### Re-measuring what compiled programs do — 600 programs, and the answer is one

Same method as the 3-in-575 note above, re-run at this HEAD on a fresh sample of
600 qualified oracle cases built `-O2 --embed-jit --jit-threads 2 -lvulkan`.
**327 programs boot the backend and 262 never do** (no function qualifies, so the
JIT never boots and neither does Vulkan); 11 more fail to build for ordinary
front-end reasons — intrinsics, `link_error` probes — none of them GPU-related.
Every one of the 327 dispatches **exactly once**, and that once is the warm-up:
the dispatch histogram is `{1: 327}` in both configurations measured. Behaviour
is unchanged apart from one crash, below: the two exit mismatches with the ladder
on are both already `DIFF_EXIT` in the CPU baseline.

Rungs fire only with the ladder *and* the lazy slice-search path switched on
(`MCC_AST_EVAL_LADDER=1 MCC_JIT_LAZY=1 MCC_JIT_SEARCH_SLICE=1`); with the shipped
defaults the rung histogram is `{0: 600}`. With all three on it is `{0: 326, 5:
1}` — **one program in 600 reaches the ladder at runtime, and its five rungs all
fall back to the CPU before dispatching.** The program is
`gcc.c-torture/execute/930921-1.c`, whose hot function is a `unsigned long long`
multiply-shift divide-by-3, and `spv_expr` returns 0 for any 64-bit type. So the
honest figure is not "1 in 600 fires" but **zero device work beyond the warm-up
in 600 programs**, and the binding constraint is the emitter's 32-bit ceiling,
not the promotion threshold.

### `c23-tag-alias-1.c` segfaults — and the GPU has nothing to do with it

Found by the sample and fixed the same day. The defect is in the JIT intent
deserializer and predates every line of GPU code; **the device only ever supplied
wall-clock time.** Keeping the wrong first reading here on purpose, because the
correlation was strong enough to publish and still wrong.

What the sample saw: a program built `--embed-jit --jit-threads 2 -lvulkan` from
`gcc.dg/c23-tag-alias-1.c` died on a JIT worker 40/40 with
`MCC_AST_EVAL_LADDER_GPU=1`, 0/40 with it unset, 0/10 under `MCC_JIT=0` or
`MCC_JIT_LAZY=1`, and 0/10 with the device forced away by
`VK_ICD_FILENAMES=/nonexistent.json` while the warm-up still built and emitted its
module. That last row read as proof that the dispatch reaching a real driver was
the trigger. **It is not.** An `LD_PRELOAD` that calls the real `vkCreateInstance`
and then reports failure — no device, no shader module, no dispatch — still
crashes 40/40, while one that merely *sleeps 400 ms* in place of a stubbed
`vkCreateInstance` is 0/40. The eager pool's workers are `pthread_detach`ed and
nothing joins them, so a program that returns from `main()` in microseconds exits
before the worker reaches the replay. Vulkan initialisation is simply the slowest
thing that had ever been put in front of it. Adding a busy loop to `main()` with
every GPU variable unset, on a default non-GPU build with no `-lvulkan`,
reproduces the identical fault 40/40.

**The mechanism.** `mccjit_role_for_base()` keys the serialized `Sym` type graph
on `t & VT_BTYPE`. An enum's base type is `VT_INT`, so every enum tag interns as
`MCCJIT_ROLE_PLAIN`, for which `mccjit_emit_type_record` writes no payload and
`mccjit_build_rec` has no case — it rebuilds as `NULL`. The deserializer already
knew this: `mccjit_strip_enum()` exists and is applied to arena node types and to
the rebuilt function's return and parameter types for exactly this reason. It was
never applied *inside* `mccjit_build_rec`, so types stored in the reconstructed
record graph kept their `VT_ENUM` bit with a null ref. `enum bar *a` came back as
a pointer to `{VT_ENUM|VT_UNSIGNED|VT_INT, NULL}`, `indir()` lifted it onto the
vstack during `ast_replay_bb` → `vstore`, and `gen_cast`'s
`IS_ENUM(type->t) && type->ref->c < 0` dereferenced the null. Fixed at the four
sites that store a type in a record; `tests/jit/parity/enum_ptr.c` pins it and
fails on the pre-fix engine.

**Two lessons worth more than the fix.** A discriminator that removes a
*capability* does not isolate a *mechanism* if it also changes timing — forcing
the ICD away removed ~400 ms as surely as it removed the device, and only the
sleeping stub separated the two. And the 600-program run with the slice-search
knobs on did not show this crash while the default-knob run did, which looked like
evidence about promotion paths and was evidence about how long each configuration
kept the process alive.

**Still open, and it is what made this look GPU-specific for a month.** The eager
pool's detached workers have no exit quiesce: the `[ladder-gpu]` atexit report
printed *before* the worker's fault, so compiler code was running on a worker
while `exit()` was already draining atexit handlers. Fixing that means a
join/shutdown path in `mccjit_pool_start`, which is a separate design change.

## `MCC_GPU` is ON by default, and the loader is what makes that safe — 2026-08-07

`option(MCC_GPU ... ON)`, on every triple. The backend itself did not change;
what changed is that it is no longer in anybody's link.

### The `dlopen` loader works on current main

It was tried and rejected once, because it made the driver fault *deterministic*
— 40/40 rather than 4/40. That fault was root-caused at `588a43c7` (the JIT
intent deserializer rebuilding enum types with a null ref) and `dlopen` was never
implicated in it: being slower, it simply let a detached JIT worker reach the
faulting replay every time instead of one time in ten. Re-tested at this HEAD.
`src/mccgpu.h` now compiles with `VK_NO_PROTOTYPES` and binds forty core entry
points through `host_dlopen`/`host_dlsym` at first use; `ldd mcc` shows no
`libvulkan`, and the device still comes up (`available=1 device=NVIDIA GeForce
RTX 5070 Ti Laptop GPU`). Full `ctest`, 8850 cells: green.

### Both mechanisms, because they answer different questions

`dlopen` removes the *library* from every link. It does nothing about the
*headers* — `mccgpu.h` still needs `vulkan/vulkan.h` for the structs and enums,
and a host with no Vulkan SDK has no such header. So `MCC_GPU=ON` also probes for
the header and turns itself off with a status message when it is missing.
`find_package(Vulkan REQUIRED)` is gone; nothing in a default configure can fail
for want of a GPU stack.

| host | configure | link | runtime |
| --- | --- | --- | --- |
| headers + loader + driver | GPU on | no `-lvulkan` | device used |
| headers, no loader or driver | GPU on | no `-lvulkan` | `mcc_vk_load` fails, CPU oracle |
| no headers | GPU off, status message | no GPU source at all | CPU oracle |

Row 2 is `VK_ICD_FILENAMES=/nonexistent.json ctest`: **2 failures out of 8850,
both in `spvgate`, both fixed here** — `spvgate` reported a driverless host as
`vulkan call failed rc=-9` and exited 1 instead of 77, so `gpu/spv-slice-real`
called it a CPU/GPU disagreement. Instance creation, device enumeration and
device creation now exit 77 (`VK_HOST`), which is a host fact rather than a
defect, and `spvgate_mutate.cmake` skips on 77 instead of calling the
known-positive vacuous. Row 3 is a configure with `-DCMAKE_IGNORE_PATH=/usr/include;...`,
the only way to hide the headers on this host: 8846 cells (the three `gpu/spv-*`
cells do not exist without `spvgate`), green.

**`MCC_VULKAN_LIB` / `MCC_METAL_LIB` override the soname search and do not fall
back**, which is how you simulate "no Vulkan installed at all" without touching
`/usr`. Setting either to a nonexistent path leaves the compiler on the CPU
oracle and everything still correct.

### `--embed-jit` programs link nothing extra now

With the hard link, `mcc --embed-jit prog.c` died on **42** unresolved references:
the forty `vk*` symbols and `fegetenv`/`fesetenv`. The forty go away with
`dlopen`. The two `fenv` ones do not — they live in libm, which the baked engine
has no business dragging into a plain C program — so they are resolved with
`host_dlsym_process` and, failing that, an explicit `dlopen` of `libm.so.6`, and
**the GPU refuses to initialise if neither resolves**, so the `FE_INEXACT`
save/restore that `builtin-fp-int-inexact` depends on can never be silently
skipped. An `--embed-jit` program now links exactly what a `MCC_GPU=OFF` build's
does — `libc` and nothing else — and still reaches the device: 4 rungs, 7
dispatches, 616 lanes under `--jit-threads 2`.

### `mccjit_boot_swap` never set the backend up at all

`ast_ladder_gpu_setup()` was called from `mccjit_boot_swap_async` only. A program
built *without* `--jit-threads` — the default — takes the **sync** boot path, so
`ast_ladder_gpu_hook` was never installed and `MCC_AST_EVAL_LADDER_GPU=1` did
nothing whatsoever in it. Every runtime measurement in the sections above used
`--jit-threads 2` and so could not see this. Both boot paths warm up now, on the
main thread, which is also what the NVIDIA lazy-init fix requires.

### The cross compilers had no backend either

`mcc_add_cross_mcc` never applied `MCC_GPU_DEFS`, so all twelve `mcc-<arch>`
binaries were built without the oracle's device path no matter how the option was
set. They get it now — `mcc-arm64` and `mcc-x86_64-win32` both report
`available=1` on this host — and because it is `dlopen`, the `-static` variants
build as well: `host_dlopen` returns NULL under `MCC_CONFIG_STATIC` and the
oracle stays on the CPU, where a static `-lvulkan` would have failed the link
outright.

### Darwin is reasoned, not run

`MTLCreateSystemDefaultDevice` was the only symbol `mccmtl.h` needed from
`-framework Metal`; everything else already went through `objc_msgSend`. It is
`dlopen`ed from `/System/Library/Frameworks/Metal.framework/Metal`, with
Foundation opened first so `objc_getClass("NSString")` and `NSAutoreleasePool`
resolve, and `fegetenv`/`fesetenv` come from `libSystem`. `MCC_GPU_LIBS` on
Darwin is `objc` alone — `libobjc.A.dylib`, present on every macOS and not a GPU
dependency. **None of the Darwin path has been executed**; this host is Linux.
It is the one part of this change that wants a real machine before it is trusted.

### What "enabled" means at runtime — leave all four gates off

`MCC_GPU=ON` compiles the backend in. It does not switch it on. Four environment
gates stand between the option and a dispatch, and every one of them should stay
off by default:

| gate | recommendation | evidence |
| --- | --- | --- |
| `MCC_AST_EVAL_LADDER_GPU` | **off** | 135 ms of device init per compiler process, measured; buys zero device work |
| `MCC_AST_EVAL_LADDER` | **off** | free at compile time, but pointless without a consumer |
| `MCC_JIT_LAZY` | **off** | changes promotion policy; nothing here measured it |
| `MCC_JIT_SEARCH_SLICE` | **off** | same |

The measurement that settles the first row: compiling a trivial translation unit
30 times is **1.2 ms/run** plain, **1.4 ms/run** with `MCC_AST_EVAL_LADDER=1`,
and **136.5 ms/run** with `MCC_AST_EVAL_LADDER_GPU=1` as well — **135 ms of
Vulkan instance/device creation per process**, a 97× penalty on short compiles.
Against that, the 600-program census above says the rung histogram with the
shipped defaults is `{0: 600}`, and with all the JIT knobs on it is `{0: 326, 5:
1}` whose five rungs all fall back to the CPU. So defaulting the GPU gate on
would cost 135 ms in every one of the ~8850 suite processes and return no device
work at all. **The option belongs on; the runtime gate does not, and it should
not move until `spv_expr` handles 64-bit types** — that ceiling, not the
promotion threshold, is what the census found binding.

`MCC_AST_EVAL_LADDER` alone is free (1.55 → 1.57 ms/run on
`tests/exec/optimizer/licm.c`) because ordinary compilation never calls
`ast_slice_equiv`; it costs only where the JIT consults the oracle. It is still
not worth defaulting on, because with the GPU gate off it only adds CPU oracle
work to the JIT's promotion path, which is the thing nothing has benchmarked.

### The lowerable ratchet was re-banked, deliberately

`MCC_GPU=ON` adds the GPU source to the `self` corpus and dilutes every lowerable
percentage — `-O1` `nodes_pct` 41.885% → 41.583%, `bodies_pct` 9.149% → 9.041%,
and similarly at every level. The bank records `corpus_config`, so before
re-banking the ratchet simply *skipped* on a default build, which is worse than a
slightly lower number: it stops gating. Re-banked with `--rebank-config
--update-bank --update-bank-low` on the new default configuration, and
`tests/rir/coverage-bank.json` recorded `MCC_GPU=1`. Anyone comparing to the
older figures should compare against the pre-change bank, not read this as a
regression — nothing about lowering changed.

## The GPU oracle is two files now, and one of them is a real TU — 2026-08-07

`mccmsl.h`, `mccmtl.h`, `mccspv.h`, `mccvk.h` and the old `mccgpu.h` were five
header-only blobs textually included into `mccast.c`, two of which existed only
because the other three could not be. They are `src/mccgpu.{c,h}`, and the split
between them is *what depends on the AST*, not *which vendor*:

  - `mccgpu.h` holds the shader emitters — SPIR-V, or MSL when
    `MCC_GPU_LANG_MSL` — and stays header-only, because they need the AST
    accessors and the `ast_eval_slice` width helpers in the includer's scope.
  - `mccgpu.c` holds the device layer: the vendored Vulkan ABI, both `dlopen`
    loaders, and dispatch. `code` reaches it as bytes, so it touches no AST and
    compiles standalone. It is a real translation unit in a
    `MCC_SINGLE_SOURCE=OFF` build and an `#include` in `libmcc.c` otherwise.

Nothing about what runs on the device changed: every emitter and dispatch line
moved verbatim. What changed is the seam. `mcc_gpu_dispatch` is one extern taking
`const void *` instead of two statics taking `const uint32_t *`/`const char *`;
the per-backend `MccGpu` is behind `mcc_gpu_stats()` rather than reached into by
name from `ast_ladder_gpu_report`; `SPV_*`/`MSL_*` allocator macros are one
`MCC_GPU_*` set; and the emitter and oracle halves carry their own include
guards rather than riding on `MCC_GPU_H`, because in an amalgamated build
`mccgpu.c` and `mccast.c` include the header wanting different halves of it.

**`stdio.h` must precede `mcchost.h`**, which the old arrangement never had to
discover: `mcchost.h` declares `host_fopen` in terms of `FILE` and on MSVC
macro-defines `vsnprintf`, which `<stdio.h>` rejects if it arrives second. A
header-only file included halfway down `mccast.c` is always past that; a real TU
is not. `mccgpu.h` now includes `<stdio.h>` itself so it is self-contained.

`tools/spvgate.c` takes `#define MCC_GPU_EMITTER 1` and gets the SPIR-V emitter
with none of the device layer, which is what keeps its own `vulkan/vulkan.h`
from colliding with the vendored declarations. It also pins `MCC_GPU_LANG_MSL 0`
— it is a SPIR-V gate on every host, including one with MoltenVK.

Provenance moved with the code: `src/mccvk.LICENSE` is `src/mccgpu.LICENSE`.

**The Darwin half is still unexecuted.** It was reasoned, not run, before this
change and it is reasoned, not run, after it; consolidation does not make it
tested. A real macOS host is still what that path wants.
**`MCC_GPU` has since left `CORPUS_DEFS` entirely**, because the backend is always
compiled and is therefore no longer a configuration the corpus can vary by; the
banked percentages stand unchanged, having already been measured with it in.
`MCC_DIAG` took its place, and it is the live example of why the mechanism exists:
`MCC_DIAG` compiles allocator/Sym/optrace instrumentation into `src/mcc.c`, which
diluted every lowerable percentage ~0.06pp and turned the diagnostics CI cell red
on a non-regression. `corpus_config` now reads `{"MCC_DIAG": "0"}`. **Any future
build option that changes which source `src/mcc.c` amalgamates has to join
`CORPUS_DEFS` in the same commit that adds the option** — otherwise it either
fails the ratchet on a non-regression or, worse, makes it skip and silently stop
gating.

### `MCC_GPU` is gone entirely — the backend is always compiled, 2026-08-07

The option no longer exists. The vendored Vulkan ABI slice the backend binds — 40
entry points, their handle and flag types, 30 structs — landed as `src/mccvk.h`
under Apache-2.0 with provenance in `src/mccvk.LICENSE`, so
`#include <vulkan/vulkan.h>` is gone from `src/` and there is nothing left for a
guard to protect. **That file has since been folded into `src/mccgpu.c`** by the
header consolidation described above; the vendoring and its licence note are
unchanged, only the file it lives in. All seven
`#if MCC_GPU` sites are deleted; `grep -rn '^#\s*\(if\|ifdef\|elif\).*MCC_GPU' src/`
returns nothing. `find_package(Vulkan)` and the three per-target
`MCC_GPU_DEFS/LIBS/INCLUDE_DIRS` blocks went with it; the surviving Darwin `objc`
link is a plain `if(APPLE)`, which is platform divergence rather than a feature
toggle. `MCC_GPU_PROVIDED` was kept verbatim and renamed
`MCC_COMPUTE_BACKEND_PROVIDED` — it is the Metal-over-Vulkan stand-in mechanism,
not a feature gate, and the rename only exists so the final grep returns nothing.

**The layout was verified rather than assumed**: a sizeof/alignof/offsetof/constant
census compiled against both the vendored and the system headers agrees on **242
facts on x86_64 and on i386** — i386 matters because non-dispatchable handles
change representation there. The no-headers case was proven by bind-mounting an
empty directory over `/usr/include/vulkan`, confirming gcc could no longer find
the header, and building from scratch: `mcc` built and still reached the device.

**One real bug fell out of compiling the backend unconditionally.** `mcc_fe_bind()`
took `fegetenv`/`fesetenv` addresses directly on Windows while dlsym-ing them
everywhere else, and mcc's own PE runtime does not provide them, so
`run-tier/x86_64-win32` and `run-tier/i386-win32` failed on `unresolved reference
to 'fegetenv'`. They are resolved from `ucrtbase.dll`/`msvcrt.dll` at first use
like every other host now, which removed the backend's last link-time reference on
any target.

**A trap for the next person, and it cost a red suite here.** The declarations of
`ast_ladder_gpu_setup`/`ast_ladder_gpu_report` must be visible in *every*
configuration, because `ast_opt_defaults` now calls setup unconditionally. They
were first moved into `mccast.h` inside the `#if MCC_EMBED_JIT` block, which is
invisible to the cross compilers — 24 cross/qemu/wine cells failed on an implicit
declaration while native stayed green. They live outside that block now. The
amalgamated build cannot observe either mistake: it concatenates every TU, so a
missing header declaration and a wrongly-guarded one both compile. Only
`multisource` and the cross tiers can see them.

`tools/spvgate.c` still includes the system header and links the loader, so its
three `gpu/spv-*` cells stay conditional on `find_package(Vulkan)`. It is a
developer differential harness rather than part of `mcc`, and it needs the actual
loader library, not just declarations.

## The `-O` ladder was re-derived on the clock — 2026-08-07

**34 of 47 level-assignable rows came off `-O1`/`-O2`/`-O3`.** The ladder is now
`L1 7 · L2 6 · L3 0`, with rungs 4–9 keeping one in-development optimizer each and
three new grouped rungs: **10 = pessimization, 11 = compile cost with no payoff,
12 = no effect and no cost**. Cumulative semantics hold, so `-O4` is still a
superset of `-O3`. The old "rungs 4–12 are one optimizer each" convention did not
survive 34 demotions and is retired.

Measured head-to-head against the previous ladder, self-compiling `src/mcc.c`,
CPU time, 10 interleaved repetitions each:

| level | old | new | delta | object |
| --- | ---: | ---: | ---: | ---: |
| `-O1` | 0.218 s | 0.214 s | **+1.91%** | −0.02% |
| `-O2` | 0.278 s | 0.252 s | **+9.34%** | +0.09% |
| `-O3` | 0.322 s | 0.256 s | **+20.53%** | **−2.19%** |

Every level compiles faster and emits a faster compiler.

**The measurement finding matters more than the ranking.** CPU time barely works
at flag granularity, and it took two controls to establish that. Fifteen rows
produce a stage-1 object **byte-identical** to the base — true effect exactly
zero — yet their measured stage-2 times span +0.063% to −0.435%; that spread *is*
the floor. And linking a padding TU ahead of an otherwise unchanged kernel, where
no emitted instruction differs and only addresses move, shifts CPU time by interp
34.3%, sieve 32.2%, loopnest 8.5%, divmod 5.6%. **Code layout is worth roughly 30×
any individual flag.** So a time delta is credited only when a layout-immune
counter agrees in sign; that rule dissolved a bogus +2.2–2.8% "win" cluster
including `storeval-rot`, which changes 0.0000% of emitted instructions, and
`chain-store`'s apparent +26% on sieve, which sat entirely inside sieve's own 32%
floor.

**Both of those two sentences are wrong as written, and 2026-08-09 measured how.**
The `storeval-rot` 0.0000% is `levelbench.tsv:47`'s `gain_pct`, a geomean of
dynamic kernel `instructions:u` over 17 kernels **none of which the flag changes**
— a ratio with an unmoved denominator, not a measurement of no effect. Its real
static reading on `src/mcc.c` is +124 at `-O2` and +6,832 at `-O3`, and its real
dynamic reading is 0.232% of stage-2. And `chain-store`'s sieve win was correctly
rejected at +26% on the clock, but on the layout-immune counter the family is
**−1.97%** on sieve, so there was a real ~2% there under a 32% floor. Neither
correction moves a level — see debt #6a and board row 2 — but the phrasing rule
does: **when `fires` is 0, quote `fires`, never `gain`.**

> **The phrasing rule is now a tool rule, 2026-08-09 (`wt/benchtrap`).** That row's
> `gain_pct` reads **`n/a`**, its bucket is `no-kernel-subject` and it carries
> `kernels_moved 0/17`, so the sentence above cannot be written again from the
> table. 23 rows were relabelled the same way with no value changed, and the row
> order moved with the new bucket rank — the anchor is **`levelbench.tsv:51`**, not
> `:47`. `optbench/null-subject-known-positive` fails if the tool is ever put back.
> See the measurement-tool audit section near the top of this file.

**Removing all the candidates at once is what caught the one real mistake.** With
all 35 off, `-O2` came out 0.52% slower. `reg-color` alone explained it: per-flag
it measured −0.12%, inside the floor and therefore invisible, but restoring it
gives 0.560 s against 0.578 s reduced and 0.575 s before — 3.1% faster than the
reduced ladder and 2.6% faster than the one it replaced. It went back;
`spill-share` layered on top made it worse again and stayed off. **Any future
demotion campaign should run the same all-off experiment**, because a flag that
only pays in combination is invisible to per-flag measurement by construction.

Two instruction/CPU-time inversions are now on record and neither is a rounding
error. `divmagic` retires **2.24× more** instructions and runs **13.3% faster**
(IPC 4.00 vs 1.53) — ranked on instructions it is the worst row in the table, so
it keeps its level on cpu-time merit. `builtin-math` inverts the other way and is
covered below.

The artifacts are re-runnable, not prose: `tools/selfhost-optbench.py` (stage-1 /
stage-2 self-host ladder; `--check` re-derives the assignment and fails if
`src/mccopt.h` disagrees), `tools/optlevel-bench.py`, `tools/opt-cache-determinism.py`,
and the banked tables `tests/optfire/{levelbench,selfhost-levelbench,levelbench-cycles,leveltime}.tsv`
plus `levelpins.txt`, which records every override with the measurement behind it.
Both benchmarks are ctest cells behind `MCC_OPT_LADDER_BENCH`, default OFF, label
`optbench`.

### What the JIT can and cannot recover from a demotion

The demotions split across a boundary that is not obvious from `mccopt.h`, and it
decides whether a demotion is reversible at runtime.

Per body, AST-rewriting passes mutate `ast_cur` **in place** (`ast_math_inline_run`,
`ast_interchange_run`, `ast_fusion_run`, `ast_tile_run`), *then* the gate mask is
snapshotted by `ast_search_gates_now()`, and the replay-time strategies are applied
during `ast_replay_body` from that mask. `mccjit_embed_note` serializes the arena
verbatim alongside the mask, and at runtime `mccjit_lazy_build_masked` rebuilds the
warm variant and searches other masks.

So **gate-mask rows stay searchable** — `narrow`, `sethi-ullman`, `tree-vrp`,
`if-conversion`, `tree-reassoc`, `tree-pre`, `tree-loop-im`,
`tree-switch-conversion`, `tree-ccp-iterate`, `narrow-fix`, `tree-dse`,
`optimize-sibling-calls` are `AST_SG_*` bits, and demoting one only moves the JIT's
starting variant. **Arena-mutating rows are irrecoverable**: `builtin-math`,
`builtin-math-prepass`, `loop-interchange`, `loop-fusion`, `loop-block` and
`loop-vlat` have no `AST_SG_*` bit, so the serialized AST simply never contains
those rewrites and no runtime search can rediscover them. A knock-on:
`ast_search_floor` is captured from the current defaults, so a demotion also lowers
the floor the search may never turn back off.

**The GPU is not a codegen path and never was.** `ast_ladder_gpu_run` takes *two*
arenas and roots, lowers both, and brute-forces them over an input tuple space to
compare outputs — it is the equivalence checker for the evaluation ladder,
accelerating optimization *validation*. Nothing about it emits code for user
programs, which is why "should it get CPU-optimized input first" is not a question
that applies to it.

## Four Linux CI cells were red, and only one was cosmetic — 2026-08-07

All twelve Linux stage2 cells were reproduced locally with `ci stage1` / `ci stage2
<feature>` / `ci stage3` rather than read from a log. Four were red; the useful
part is that **three of the four are invisible to the default build by
construction**.

- **`multisource`** — build failure, 8825/8854 failing. `ast_ladder_gpu_setup` was
  declared inside `mccast.c`'s body. The amalgamated build concatenates every TU so
  it compiled; a real multi-TU build has no declaration. **The default build cannot
  observe a missing header declaration**, so every such omission ships green until
  this cell runs.
- **`diagnostics`** — 23 cells. A genuine leak, not a test artifact:
  `free_inline_functions` frees `fn->func_str` only when `fn->sym` is set, which is
  right for the emission path (`begin_macro(str, 1)` takes ownership and *then*
  nulls sym) and wrong for `drop_gnu_inline_body`, which nulls sym and hands the
  token string to nobody. Every gnu89 extern-inline redefinition leaked it, and
  `gen_inline_functions` skips `!sym` entries so it was unreachable garbage. The
  program's own output was already `OK` — **only `MCC_DIAG`'s allocator tracking
  could see it**.
- **`sanitize`** — `sanitize-selfcheck`, fixed by the same leak fix.
- **`macho`** — `cli/perfn_inproc` asserted that `-fopt-perfn-inproc` is observable
  at `-O3` by *relying on* `opt-slice` being on there, so it tracked where a flag
  happened to live rather than the dependency it meant to test. It passes
  `-fopt-slice` explicitly now and survives any future ladder move.

The lesson worth carrying: a cell that encodes a *default* rather than its own
premise breaks whenever the default moves, and the amalgamation hides an entire
class of cross-TU mistakes.

## An entire inliner ships unreachable, and it does not earn a level — 2026-08-07

Two ICEs turned out to be one bug, and finding it exposed something larger.

**The bug.** When a call's result is discarded with `(void)`, RIR records the
`AST_Invoke` node as `VT_VOID`. The direct-call replay honours that — after
`gfunc_call` it pushes nothing, and the statement level pops nothing.
`ast_inline_graft`, which replaces the same Invoke with the callee's body,
unconditionally pushed its return slot. One value in, none out, once per discarded
call, which is why the leak count read 1, 2 or 4 depending on how many the function
had. The graft path now pops when the node it replaced was `VT_VOID` — the contract
the direct call already obeyed. Reproducer is two lines:

    static int f(void) { return 1; }
    void g(void) { (void)f(); }

`mcc -O1 -finline -fno-inline-functions -c`. Clean without the `(void)`, clean with
a non-static callee. Emitted `src/mcc.c` is **byte-identical at `-O0`/`-O1`/`-O2`/
`-O3`/`-Os`**, so the fix costs nothing.

**`storeval-callstore` was never the culprit in the second ICE.** Its ON state
merely produces the AST shape whose Invoke reaches the graft, so it looked like the
cause when it was the doorway. Its level had been justified only by "cannot be
measured with it off"; that excuse is gone.

**The structural finding.** `do_inline` requires `MCC_OPT_INLINE` on *and*
`MCC_OPT_INLINE_FUNCTIONS` off (`ast_has_graftable_call` needs `ast_inline_env`;
`do_inline` needs `!ast_inline_pass_env`). No shipped level produces that
combination — `INLINE` is `MCC_OPTD_SPECIAL` at `optimize>=3` and `INLINE_FUNCTIONS`
is on from `-O2`. **The capture-time graft inliner therefore never runs at any `-O`
level as shipped**, only under an explicit `-finline -fno-inline-functions`. That is
why a crash this simple survived: nothing reachable exercised the code.

**Validated on the clock, and it does not pay.** Comparing `-O2` default against
`-O2 -finline -fno-inline-functions` on the self-compile, CPU time, interleaved:

| | compile time | emitted object | stage-2 runtime |
| --- | ---: | ---: | ---: |
| graft vs default | +0.80% (N=12) | **+7.88%**, +256 KB | **−0.40%** (N=14) |

Both time figures sit inside the noise floor; the size increase does not. It emits
**7.9% more code for no measurable speed in either direction**, so under the rule
that only speed decides a level, it does not earn one and was not promoted. It
stays reachable only by explicit flag.

**What would change that verdict.** The graft is an alternative inlining strategy,
not an increment on the ordinary one — the two are mutually exclusive by
construction, so the honest comparison is always A-or-B, never A-plus-B. Anyone
revisiting it should either make the two composable, or find the corpus where
grafting beats `inline-functions` outright; measuring it against nothing will keep
producing the answer above.

## `reemit-templates`: the cost was not where the name pointed — 2026-08-07

It was the most expensive single item in the compiler and survived only because it
gates a strategy family. Profiling rather than guessing moved it **−13.05% of
stage-1 CPU** (21× the 0.607% floor), with instructions retired agreeing in sign at
**2,861.7M → 2,280.4M (−20.31%)**, and **emitted code byte-identical** across all
1465 objects `tests/exec` compiles at five levels — so stage-2 is unchanged by
construction. Independently re-measured here at **+10.36% on a self-compile with a
0-byte object delta**, under load, which is the same result.

**`sg_templates` gates ten rows of `ast_strategies[]`, eight of them shipped**, and
outside that table the flag does only three things. The flag *is* the strategy
family — there was nothing incidental to peel away, which is why "make it
unnecessary" was never an option and "make it cheap" was the only route.

**Where the time actually went, and it was not the strategies.** An `LD_PRELOAD`
counter found **923,403 `getenv()` calls per self-compile** — 524,491 `RVATTR` from
`ast_replay_value` (once per replayed AST node) and 377,110 `MCC_REPARENT_DBG` from
`ast_add_child` (once per edge) — worth 6.6% of stage-1 cycles, inside the reemit
pipeline and **gated by nothing**. Reading them once in `ast_configure` takes it to
50 calls. **Check for this pattern before profiling anything else in this compiler:
a diagnostic env var read on a per-node or per-edge path is invisible to every
existing measurement and costs more than the passes being measured.**

The other half is that **seven of the eight shipped strategies fire on zero bodies**
of an `-O2` self-compile (bfold, cse, tco, cload at 0; cprop 3, dse 3, jt 7, sccp 14;
only ident at 1600 of 2626). They walked every body to find nothing, and now return
early when their candidate set is empty.

**Caveat carried forward**: `selfhost-optbench --check` was not re-run. `src/mccopt.h`
is untouched so no row moves, and the gain side of every ratio is unchanged because
objects are byte-identical — but the cost side moved down, so the greedy level
boundaries are not provably unshifted.

## `fabs` was broken in two places, one of which no flag could reach — 2026-08-07

The `builtin-math` demotion was caused by a lowering defect, and fixing it turned
up that there were **two** defective paths, not one.

`__builtin_fabs` was a **macro** in `runtime/include/mccdefs.h` —
`__builtin_signbit(x) ? -x : x` — so it never reached `gen_fabs` at all. That is
the `movmskpd`/`cmp`/`je` version, present at **every `-O` level including `-O0`**
and reachable by no flag, which is why it never showed up as a flag's cost.
`fabs()` calls went through `gen_fabs`, which was the `andb $0x7f` store-forwarding
version recorded in `levelpins.txt`. Both are now one sequence:

    pcmpeqd %xmm1,%xmm1 ; psrlq $0x1,%xmm1 ; andpd %xmm1,%xmm0

no memory round-trip, no branch (`andps`/`psrld` for float). Verified bit-exact
against gcc on `-0.0`, `+0.0`, `-inf`, NaN sign and quietness, and the float path.
arm64 `fabs d0,d0`, riscv64 `fabs.d`, i386 x87 `fabs` and both win32 targets are
unregressed, and `__builtin_fabs` now reaches those too instead of the branchy
macro; 32-bit arm keeps the macro because it has no `gen_fabs`.

**Measured, CPU time, N=9 interleaved, floors re-taken with padding TUs** (mathfun
0.67%, branchy 0.53%, nbody 1.18%):

| | CPU |
| --- | ---: |
| mathfun, fabs row on vs off | 152.5 → **110.4 ms (−27.3%)** |
| mathfun, the *old* fold | 151.9 → **283.6 ms (+85.7%)** on fewer instructions |
| branchy, the *old* fold | **+47.0%** |
| nbody, `builtin-math` | 334.9 → **299.8 ms (−10.5%)** |

The old fold was not merely failing to pay — it was **actively costing 85.7% on
mathfun while retiring fewer instructions**, which is the sharpest illustration in
this file of why instructions retired cannot adjudicate a lowering.

**The split.** A new `builtin-math-fabs` row carries the fabs lowering and
`builtin-math` keeps sqrt, subordinate the same way `builtin-math-prepass` is;
`-ffast-math` sets both optflags and `cli/fast_math_implies_no_math_errno` still
reads `plain=1 fast=0 off=1`. All three rows moved **10 → 1**. Note nbody's entire
win is carried by the *prepass*, not by `builtin-math` alone. The `-O1` placement
rests on reasoning plus an xoracle behaviour check at `-O1` (identical verdicts on
all 4907 programs), not on an `-O1` benchmark — neither lowering introduces a loop
or a nested call and both *remove* a call.

**A much larger bug found on the way: `mccrir.c` had no `IR_OP_FABS` case.** A
parse-time `gen_fabs` produced an IR op the AST builder silently dropped, replay
emitted different bytes, the body came back unfaithful — and **every AST
optimization was then skipped for that entire function**. So a single unhandled IR
opcode silently disabled the optimizer for any body containing it. `math_prepass.c`
now inlines 4 of 4 sqrt calls at `-O1` where the old compiler managed 3 of 4 at
`-O10`. **Worth auditing the rest of the `IR_OP_*` set for the same gap**; the
failure mode is silent and costs everything downstream of it.

Second, smaller: with `__builtin_fabs` a macro, `ast_expr_nonneg` could never prove
`sqrt(fabs(x)+1)` non-negative. It can now.

**`AST_SG_BFOLD_SQRT` does not cover the same win** — checked rather than assumed.
It gates *entry* to the sqrt row in `ast_bfold_run`, a superset: `-fno-bfold-sqrt`
loses both the `sqrt(4.0)` constant fold and the inline, while `-fno-builtin-math`
loses only the inline. The inline additionally needs `ast_math_inline_env`, which
has **no `AST_SG_*` bit**, so the replay gate mask still cannot reach it. The JIT
gets the win back only because the row now defaults on from `-O1`; **the gate-mask
gap is still open**, and it is the same arena-mutating-vs-gate-mask boundary
described above.

## Two coverage gaps, audited — `mccrir.c` opcodes and the shader emitter, 2026-08-07

These compound: an arena that never recorded an operation cannot be lowered to the
device either, so the first gap silently caps the second.

### `mccrir.c` drops 25 of the 67 IR opcodes, silently

`IR_OP_FABS` was not a one-off. The opcode list is an X-macro in `src/mccircap.c`
(**67** entries); `src/mccrir.c` has a `case` for **42**. The switch that builds the
arena ends in

    default:
      break;

so an opcode with no case is **silently skipped** — no `rir_arena_mismatch++`, no
diagnostic, nothing. The arena then describes a body the compiler never emitted,
replay produces different bytes, the body comes back unfaithful, and **every AST
optimization is skipped for that entire function**. It is safe — the AOT body is
kept, so no wrong code — but it is invisible, and it costs the optimizer wholesale
wherever it happens.

The 25 with no case, all of them reachable from the capture dispatch:

    ASAN_MARK_WRITE  ASAN_SHADOW  CMOV      COPYSIGN   FILLNOPS
    LOAD             MKPTR        MULH      MULWIDEN   RAW
    REGADDI          RETVAL       ROUND     SQRT       STRUCTCOPY
    TCOV             TRAP         UBSAN_NULLPTR  VLA_ALLOC  VLA_RESULT
    VLA_SPREST       VLA_SPSAVE   VPUSHSYM  X87POP     XFERRET

Spot-checked `SQRT`, `COPYSIGN`, `ROUND`, `CMOV`, `MULH`, `STRUCTCOPY`,
`VLA_ALLOC`, `RETVAL` and `LOAD`: each has exactly one emitter in `mccircap.c` and
zero handlers in `mccrir.c`. `SQRT` and `COPYSIGN` sitting in that list next to the
`FABS` that was just fixed is the obvious place to start. `RAW` is the documented
escape hatch and is expected here; the rest are not.

**The cheapest possible first move is a diagnostic, not a feature.** The `FABS`
case already does `rir_arena_mismatch++` when its operand is missing, and 30 sites
in the file use that counter — the `default:` arm simply does not. Making it count
turns all 25 silent gaps into visible ones in one line, and tells you which of them
actually occur in real code before anyone writes 25 handlers.

### The shader emitter is 32-bit, integer-only, and seven node kinds wide

This is the binding constraint on GPU coverage, and it is why the census found
**zero device work beyond the one-per-process warm-up across 600 programs**: rungs
fire, then fall back.

Both emitters — SPIR-V and the MSL one beside it — handle exactly `AST_Literal`,
`AST_Ref`, `AST_Load`, `AST_Convert`, `AST_Unary`, `AST_Binary`, `AST_If`. Within
those:

  - **64-bit is refused at eight separate sites** (`ast_eval_slice_is64`). The one
    program in 600 that reached the ladder was `930921-1.c`, whose hot function is
    an `unsigned long long` multiply-shift — it fell back for exactly this reason.
  - **Floating point is refused outright** in `AST_Binary` (`is_float` on either
    operand) and in the width checks elsewhere.
  - **`AST_Unary` accepts four operators**: `-`, `TOK_NEG`, `~`, `!`.

So the equivalence ladder can only validate 32-bit integer expression slices over
seven node kinds. Widening this is what turns the GPU from a warm-up into a working
oracle, and the order that buys the most coverage per unit of work is **64-bit
integers first** (one refusal predicate, eight sites, and it unblocks the only
program known to have reached the ladder), then **float/double**, then the
remaining unary and binary operators.

### Corrections to the audit above — measured 2026-08-07

The audit's counts hold; three of its conclusions do not. Measured on an instrumented
copy compiling `src/mcc.c` at `-O2` (`-O1` identical, `-O0` zero — the arena path is
off there).

1. **"All 25 are reachable from the capture dispatch" is false. Exactly four fire:
   `RETVAL` (4001 hits), `MKPTR` (3590), `VPUSHSYM` (3059), and `LOAD` (0 — it never
   reaches the switch at all, killed by the `continue` at `src/mccrir.c:4678-4681`
   when any of nine depth/pending flags is set).** The other 21 fire zero times;
   `X87POP`, `XFERRET`, `VLA_RESULT`, `REGADDI` and `STRUCTCOPY` are `#ifdef`-ed out
   on arm64 and *cannot* occur (`src/mccgen.c:7-43`); `RAW` is handled outside the
   switch (`src/mccrir.c:4943,5309,5471,5782`) and belongs on no gap list.
2. **The proposed one-line fix would break the compiler.** Five opcodes that reach the
   `default:` arm — `JMP`, `JMPCOND`, `JMPADDR`, `JMPAPPEND`, `GSYMADDR` — are
   correctly handled in *other* switches (`src/mccrir.c:965,969,987,1013,1029` and
   `:4711,4717,4723,4739,4749`). They are **68% of that arm's traffic** (23,040 of
   33,690 hits). Wiring `rir_arena_mismatch++` into the bare `default:` would mark
   ~74% of all bodies unfaithful. The counter must be a **per-opcode histogram with
   those five excluded**.
3. **The causal chain "silently skipped → replay differs → unfaithful → every AST
   optimization skipped" is refuted as a general claim.** Per-body correlation:
   `bodies_with_drop=1823`, `bodies_nodrop=633`, `unf_with_drop=81`, `unf_nodrop=14`.
   74.2% of bodies contain a dropped opcode and **95.6% of those still replay
   byte-identically and are used**. Unfaithful rate is 4.4% with drops vs 2.2%
   without — a 2× relative risk, not a mechanism. `RETVAL`/`MKPTR`/`VPUSHSYM` are
   compensated by the region/reconcile machinery.
4. **"64-bit is refused at eight sites" undercounts by half: it is 16** — 7 in the MSL
   emitter, 7 in SPIR-V, 2 in the ladder hook (`src/mccgpu.h:500,512,522,539,552,564,
   601,1382,1394,1404,1419,1432,1444,1479`; `src/mccast.c:15759,15766`). Better news
   the audit missed: **the CPU slice evaluator already does int64** — `src/ast_eval_
   slice.h:382,422` use `is64` to *select* width and pass it to `ast_eval_binop`.
   Only the emitters and the ladder gate refuse. But int64 also needs a SPIR-V
   type/ABI change: the module declares only `SpvCapShader` (`src/mccgpu.h:943-944`)
   and the storage buffer is a runtime array of 32-bit ints with `ArrayStride 4`.
5. **A refusal the audit did not list — but it is correct, not a gap.** The emitters
   require `nchild == 3` for `AST_If` (`src/mccgpu.h:1526`), and 65.6% of `AST_If`
   nodes are two-child. That is **not** 66% of a handled kind being wrongly refused:
   cross-tabulating by `op` shows the two-child nodes are statement-`if`s (`op == 0`,
   8755 of them) and loop regions, none of which produce a value an *expression*
   emitter could return. Only **`op == 5 && nchild == 3` — the ternary, 1678 nodes,
   11.5% of `AST_If`** — is value-producing. See item 0 of the priority board.

## The RIR coverage headline is the wrong metric — 2026-08-07

`tools/rir-coverage.py:924` computes `modelled = used + (fallback_bytes − abort_bytes)`.
**A body that replayed to different bytes still counts as "modelled."** That is why the
banked figure reads 100.000%/99.965% while the compiler is not, in fact, reproducing
100% of anything.

Scale, measured two ways — **the single-file and whole-corpus figures differ a lot, and
conflating them overstates the gap:**

| corpus | modelled | kept | bytes losing AST optimization |
| --- | --- | --- | --- |
| `src/mcc.c` alone, `-O2` | 99.9675% | **81.70%** | 18.3% |
| `src/mcc.c` alone, `-O1` | 100.000% | **81.42%** | 18.6% |
| **self corpus**, `-O1`, measured on this host | 100.000% | **97.784%** | 2.216% |
| self corpus, banked `-O1` / `-O2` | 100.000% / 99.9666% | 96.156% / 96.127% | ~3.9% |

So `src/mcc.c` is an outlier, not the norm — the whole-corpus gap is ~2–4%, not 18%.

**The reporting was already honest; the ratchet was not.** `rir-coverage.py:943-945`
has always printed `MODELLED … (kept … + discarded by byte compare …)`, and both
figures are banked (`:924-925`). But the regression check gated **only**
`modelled_coverage` and `residual` — `kept_coverage` was banked and never enforced, so
the number that actually means "bodies ship optimized" could fall arbitrarily far with
no test failing. **Fixed:** a `kept_coverage` regression check now sits beside the
modelled one. Verified both ways — it fires on a doctored bank
(`kept coverage regressed: 97.7839% < banked 99.9000%`) and stays silent against the
real banked values.

**Caveat on where this is enforced:** `rir-coverage` **does not run on Darwin at all.**
`tools/rir-coverage.py:836-848` returns 77 when a corpus has no banked lowerable floors
for the host, and macho has none for either corpus — so `rir-coverage` and
`rir-coverage-census` both skip locally, and the new gate (like the old ones) is
exercised only on elf/pe hosts. Banking macho floors is a separate decision.

## The Linux stage2 matrix, replicated locally — 2026-08-08

`mcc-ci plan --job stage2` enumerates 11 runnable Linux cells. All 11 were built as real
stage2 self-hosts (`mcc-ci stage2 <feature> --mcc <stage1>`) and one of them —
`dynamic`, configured **without Vulkan headers**, which is what `matrix.yml`'s stage2
step does since it passes no `-DVulkan_INCLUDE_DIR` — was carried through
`mcc-ci stage3 --consume test` verbatim: parallel ctest, 900 s timeout, junit out.

**Which cell CI was reporting, established by arithmetic rather than assumption.** The
pasted failure list keys tests by index, and indices are assigned in registration order,
so a missing cell shifts everything after it:

| configuration | `optfire/ident_shift` index |
| --- | ---: |
| this host, Vulkan headers present, x86_64 | 7832 |
| − the 3 `gpu/spv-slice-*` cells (`spvgate` needs Vulkan headers to build) | 7829 |
| − `optfire/regdisp` (`tests/optfire/arch.txt` gates it to x86_64) | **7828** ← the CI run |

So the failing cell is **arm64, without Vulkan headers** — `matrix.yml`'s nightly
`stage2-nightly` plan, which includes `linux-arm64-gcc`, not `ci.yml`'s gate plan.
Confirmed the −3 half directly: a no-Vulkan configure registers 8913 tests against 8916
and puts `ident_shift` at 7829.

**Six real failures the replication found, all one root cause.** `optfire/{pre,ltemp,licm}`
and their `level-` twins: *"pass DID NOT FIRE (… =0 at -O10)"*. The `-O10` in
`tests/optfire/counters.txt` and `levels.txt` is a "parked above the shipped ladder"
sentinel, and `1ad3f1aa` moved these passes past it — `tree-loop-im` to **11**
(`src/mccopt.h:118`) and `tree-pre` to **12** (`:121`), the latter pinned in
`levelpins.txt:198`. The expectation files were not updated. Measured firing levels,
**identical on x86_64, arm64, i386 and riscv64**, so this is not arch-specific:

| counter | flag | declared level | fires at |
| --- | --- | ---: | --- |
| `ltemp` | `tree-loop-im` | 11 | `-O11` |
| `licm` | `tree-loop-im` | 11 | `-O11` |
| `pre` | `tree-pre` | 12 | `-O12` |

Fixed by naming the real rung in `counters.txt`, and in `levels.txt` by pinning the
**boundary** (`-O10:off,-O11:on`) rather than just moving the sentinel, so a future move
in either direction is caught. All 123 `optfire/` cells pass.

**Note the inversion, because it is not explained.** On this host these six fail and
`optfire/ident_shift` passes — under gcc, under stage2 self-host, with and without
Vulkan, at load average 23 (40/40), and its objects differ on all five targets. CI
reports the opposite. Since the six are deterministic and arch-independent, CI's list of
three was either partial or from a different tree.

### `cli/perfn_inproc` — OPEN, and do not bank the expectation

The seventh failure from the full run. Expects `DIFFER\nSAME`, gets `SAME\nSAME`:
`-fopt-perfn-inproc` no longer changes the object even with `-fopt-slice` forced on.

**It is not a recent regression.** Probed at `879bf988`, `c2838c61`, `109d407a`,
`6c46618e` and `1ad3f1aa` with the current case text: `SAME` at every one. **`6c46618e`,
whose entire purpose was to fix this cell, never fixed it** — it made the `-fopt-slice`
premise explicit, correctly, but the case still cannot observe the flag.

**Why it cannot.** The pass is guarded by `ast_perfn_inproc_env && do_inline`
(`src/mccast.c:18038`), and `do_inline` (`:17943`) needs `ast_has_graftable_call`. The
case's own `-fno-inline-functions` drives the `inline` counter to **0**, so the guard is
unreachable — the case disables the precondition of the thing it is testing. But
removing that flag does not rescue it either: with the inliner on (`inline=3`) the
objects are still byte-identical, at `-O3`, `-O8`, `-O10` and `-O12`, with and without
`-freemit-templates`, and equally when toggling the flag **off** where it is default-on
(level 8). A deliberately fat callee inlined at eight sites — where refusing to inline
must be shorter — is also byte-identical, 2198 bytes either way.

The machinery is wired, not dead: `pf_best` is consumed at `src/mccast.c:18051`, outside
the search block, so the default path emits with `do_inline` and the search path emits
the shorter of `ui=0`/`ui=1`. It simply never picks `ui=0`.

**Two readings, and they need different fixes.** Either the case needs an input that
discriminates, or the pass has become inert and earns no level — the same shape as
"an entire inliner ships unreachable" above. **Flipping the expectation to `SAME` would
bank the second reading without establishing it**, which is why it is left red here
rather than quietly made green.

## `rir-coverage` and `node-census` re-banked — and what actually moved, 2026-08-08

Both cells were red on the Linux stage3 run. Neither is a regression in the compiler;
both are banks that were never refreshed after a landed change. Bisected rather than
assumed, because a 13-point drop is exactly the shape a real defect would have.

**`rir-coverage`: the -O ladder moved the number, and `1ad3f1aa` is the commit.**
Measured `kept_coverage` at -O1 across the range, one Debug build per commit:

| commit | | fallback bodies | kept -O1 |
| --- | --- | ---: | ---: |
| `879bf988` | the last commit that banked it | 25 | **96.162%** |
| `109d407a` | | 25 | 96.162% |
| `6c46618e` | | 25 | 96.162% |
| **`1ad3f1aa`** | **opt(ladder): the -O levels, measured** | **98** | **82.520%** |
| `893c1e84` | | 98 | 82.520% |
| `db7c6829` | | 98 | 82.520% |
| `57442404` | the bank commit — *bank already stale here* | 98 | 82.520% |
| `b740ae46` | HEAD | 98 | 82.772% |

`1ad3f1aa` moved eleven passes across the ladder on measured CPU time. That changes
which bodies replay byte-identically, so 73 more bodies fall back to direct emission.
**The bank was not refreshed and nothing caught it**, because the `kept_coverage` gate
did not exist yet — `78d4856f` added it four commits later, and item 10 below already
records that it "has never been run against a clean HEAD build". This is that run.

**It is a coverage number, not a correctness one.** Across all four levels the run
still reports `MODELLED 100.000%` (99.967% at -O2/-O3), `GAP 0.000%`, `rerror 0`,
`unfaithful 0 B`. Nothing replays to *wrong* bytes; more bodies decline to replay.
-O0 is unaffected and in fact went **up** (82.4583 → 82.7139), which is the expected
signature: the arena path only runs at `-O1`+, so a ladder change cannot touch -O0.

**The lowerable floors moved for a different and duller reason — the corpus grew.**
`bodies` 2721 → 2767 and `nodes` 420152 → 429213, because `src/mcc.c` amalgamates
`src/mccgpu.h` (+1319 lines at `989e4b3b`), `src/mccrir.c` (+854) and
`src/ast_eval_slice.h` (+564). Percentages over a bigger denominator with the same
blocker mix drift down: `bodies_pct` 9.0408 → 8.9266. `corpus_config` cannot see this
— it tracks `MCC_DIAG` only, and no build option shaped this growth; the source did.

**`node-census`: the same corpus growth, nothing else.** `invoke_internal`
18035 → 19111, `invoke_sites` 21266 → 22312 against `nodes` 420152 → 429213, so
`all_invokes_on_cpu` = 94.9385% → 94.8004%. The external-only ceiling is **unchanged
in substance** at 99.2540% (banked 99.231%), which is the number the plan's headline
actually rests on. A ratchet on `all_invokes_on_cpu` moves whenever the compiler's own
call density changes and is not a regression signal in either direction — worth
revisiting whether that leaf should be gated at all.

**Banked from the stage2 self-host tree** (`CC=mcc`), because that is the tree
`ci stage3 --consume test` runs against. Verified green on **both** the stage2 tree and
a gcc-hosted tree. That mattered: the two hosts do not agree exactly — stage2 reports
`fallback 100 / kept 82.7139` at -O0 against gcc's `98 / 82.7770` — so the floors were
taken from the lower of the two. The spread is 0.06pp at -O0 and 0.005pp at -O1,
against the tool's `--tol` of 0.05pp, so **-O0 is inside the tolerance only because the
bank was taken on the losing host.** Bank from stage2, not from a gcc host.

Untouched by the re-bank, verified by comparison: the `wide` corpus, every `pe`
lowerable floor, `corpus_config`, and `nofb_miscompiles`.

## The device becomes a JIT-lifetime resource — plan amended 2026-08-08

[`docs/PLAN.md`](PLAN.md) gained **cluster L**: Metal and Vulkan are initialized once,
with the JIT, in the emitted `.init_array` constructor; stay warm and resident for the
process; are dispatched by the JIT's own pool; are routed to per function through the
JIT's own hot-patch slots; and are torn down once, with the JIT. Rows **N1** and **N6**
are superseded by it. The items below are in dependency order — **each one blocks the
next**, and the first three are prerequisites, not the feature.

**The shape of the problem, verified at `b740ae46`.** The way *in* already exists:
`mccjit_boot_swap` calls `ast_ladder_gpu_setup()` at `src/mccjit_embed.c:1893` and
`mccjit_boot_swap_async` at `:1906`. The way *out* does not exist at all — the JIT's
only exit hook is `atexit(mccjit_kgc_flush_all)` (`:2810`), its pool workers are
`pthread_detach`ed (`:1375`) into an infinite cond-wait loop (`:1341`) and are never
signalled or joined, and `mcc_gpu_quiesce`'s single caller is registered only under
`MCC_AST_EVAL_LADDER_GPU` (`src/mccast.c:15881`). So **"torn down once with the JIT" is
a construction, not a move.**

1. **Give the JIT a shutdown. Nothing else in cluster L can land first.**
   `mccjit_pool` (`src/mccjit_embed.c:1302-1309`) needs a quit flag, a
   `pthread_cond_broadcast`, and either a join or a counted in-flight barrier; the
   workers are detached today so there is nothing to join. Then a single
   `mccjit_shutdown()` registered `atexit` once, ordered **ahead of**
   `mcc_stats_finish`'s hook (`src/mccstats.c:541-546`), which drains the pool, then
   quiesces the device, then flushes KGC. Note this also closes an existing hole the
   board already records: the atexit stats report can race a live detached worker.

2. **Clear `mcc_gpu.ok` on `VK_ERROR_DEVICE_LOST` before step 1 ships.**
   `mcc_gpu_quiesce` does an unbounded `vkDeviceWaitIdle` (`src/mccgpu.c:1303`). Today
   that is unreachable in a normal build because nothing registers the hook. Step 1
   makes it run on **every** exit, which converts a latent deadlock into a live one.
   Ordering matters: this lands with or before the shutdown, never after.

3. **Make Vulkan quiesce actually tear down.** `src/mccgpu.c:1299-1305` sets
   `mcc_gpu_closing` and waits — no `vkDestroyDevice`, no `vkDestroyInstance` — and the
   `dlopen` handle is never closed anywhere in the file. Metal's quiesce (`:241-249`)
   releases the PSO cache but not `dev`/`queue`. A JIT-lifetime device that leaks its
   device and instance to process exit buys nothing over the current behaviour.

4. **Move bring-up into the constructor and make it a bring-up.**
   `ast_ladder_gpu_setup` currently emits a literal-7 kernel and **dispatches** it
   (`src/mccast.c:15891-15900`) — a warm-up, not an init — and it is called *once per
   baked function* from `mccjit_boot_swap`. Replace with a `mcc_gpu_boot()` called once
   from `__mccjit_boot_all` (`src/mccjit_embed.c:2013-2031`), under a real
   `pthread_once` rather than `mcc_gpu.tried`, which is set *before* the work succeeds
   (`src/mccgpu.c:209-211`, `:1212-1214`) and therefore makes a failed init sticky.
   Keep the self-test dispatch as opt-in. Payoff: the **130 ms** one-time init moves out
   of whichever dispatch happens to be first and into `.init_array`, concurrent with the
   JIT's own boot, and "is there a device" becomes answerable *before* the first offload
   decision.

5. **Make the resident set resident — and note where the win actually is.**
   Vulkan creates its compute pipeline with `VK_NULL_HANDLE` for the pipeline cache
   (`src/mccgpu.c:1471`) and destroys the shader module, pipeline layout and descriptor
   set layout alongside it every dispatch (`:1456`, `:1462`, `:1417` → `:1525`, `:1523`,
   `:1529`). Under an interpreter kernel there is **one** module for the life of the
   process, so this converts a per-dispatch driver compile into a single boot-time one.
   **That, not buffer reuse, is the win** — the held-buffer A/B was within noise at
   every size from 64 to 262,144 tuples and was reverted, and that result stands.
   Buffer residency is still required, but as a *correctness* precondition for the B1
   address space, not as a latency item.

6. **Route offload through the hot-patch slot, not a new mechanism.**
   Every JIT-targeted function already begins with an indirect jump through
   `__mccjit_slot_<fn>` — `ff 25 disp32` on x86-64 (`src/mccast.c:18106-18111`),
   `adrp/add/ldr/br x16` on arm64 (`:18194-18199`) — with the AOT body spliced six (or
   sixteen) bytes behind it, and takeover is one release store via `mcc_jit_publish`
   (`src/mccjit_embed.c:9414-9421`). A GPU-resident variant publishes the same way.
   Consequence worth stating: a device fault becomes a **slot re-publish back to the AOT
   body**, so `PLAN.md`'s J3a′ whole-run abandon-and-restart is needed only when the
   fault happened past the side-effect watermark.

7. **`ast_gpu_want` must be its own predicate. Do not inherit `ast_jit_eligible`.**
   `ast_jit_want` (`src/mccast.c:1776`) requires `ast_jit_eligible` (`:1753`): no
   varargs, no `switch` in the body, ≤6 params, **≥1 param**, and every parameter and
   return in the scalar GP/double set, plus no VLA (`:1779`). Those are **host-ABI
   trampoline constraints, not device constraints.** A GPU path that inherits them
   cannot reach the 99.4% ceiling — it cannot even reach the front end, which is full of
   `switch`. Share the slot machinery and the `-jit-functions` selection
   (`ast_jit_selected`, `:1720`, where an empty list already means every function);
   carry device constraints (no `long double`, no inline `asm`, no computed goto)
   separately.

8. **Three hazards this coupling creates, none of which bites today.**
   (i) `pthread_atfork` (`src/mccjit_embed.c:1336`) resets the pool in the child
   (`:1324`); Vulkan and Metal handles are **not** valid across `fork`, so the child
   handler must mark the device dead rather than inherit it. (ii) `MCC_GPU_LOCK` and
   `MCC_GPU_UNLOCK` are `((void)0)` on non-POSIX (`src/mccgpu.c:28-29`) — with a pool
   dispatching that is a live race on `mcc_gpu.dispatches`/`lanes`, on the Metal PSO
   cache and on the resident buffer. (iii) `mcc_gpu_stats` (`:1545`) reads the counters
   outside the lock. All three are currently benign only because exactly one thread ever
   dispatches, and it dispatches almost never.

9. **Hard constraint on the pool: never hold `mccjit_swap_lock` across device work.**
   That lock (`src/mccjit_embed.c:1300`) serializes every JIT compile process-wide — the
   pool may have N workers but only one `job->run(job)` executes at a time (`:1353-1355`).
   A 15–25k-word module build is plausibly seconds on MoltenVK (SPIRV-Cross → MSL →
   Metal); holding that lock would stall all JIT compilation behind one shader.

10. **`--embed-jit`: dlopen, never link.** The precedent is already in the tree and was
    established for this exact reason — `objc_getClass`/`sel_registerName`/`objc_msgSend`
    are resolved through `host_dlsym_process` at Metal-load time rather than linked,
    because *"an `--embed-jit` program has no business linking libm"*
    (`src/mccgpu.c:32-33`). `MCC_JIT=0` must leave a baked program with no device
    bring-up at all, and a missing ICD must be a clean refusal at boot.

**Two corrections this amendment forced, both already applied to `PLAN.md`.**
`mcc_relocate_ex`'s `mem = 0` pass does **not** yield image offsets —
`src/mccrun.c:750` reads `s->sh_addr = mem ? addr + offset : 0`, so a null pass zeroes
every `sh_addr` and only the total padded size survives; Phase 1 needs either a one-line
change there or a separate layout pass. And **N6 is fixed** at `b740ae46`: the warm-up's
`pin`/`pout` are heap-allocated and slot-sized (`src/mccast.c:15794-15798`) and freed on
both exits — the row below is retained for the record only.

**One item this unblocks.** M5/D1e was recorded as runnable only on a Metal box. That is
wrong: pre-enqueued command buffers are not Metal-specific and this host has a discrete
NVIDIA device with validation layers. What D1e is actually blocked on is cluster L —
there is no resident command pool or command-buffer ring to pre-enqueue *into*.

## Blocking — bugs and free wins from the decision investigation, 2026-08-08

Eight parallel investigations closed the fourteen open rows in
[`docs/PLAN.md`](PLAN.md)'s decision table (recorded there under *Decisions resolved*).
They also turned up nine defects and free wins that have **nothing to do with whether
that plan is adopted**. Ordered by severity; the first is a memory-safety bug.

**Read the host note first.** The 2026-08-07 study was measured on an Apple M1 Pro
through MoltenVK. This machine is Linux x86_64 with a **discrete NVIDIA RTX 5070 Ti**,
validation layers, `spirv-val` and `glslc`. Numbers below are from this host unless
stated, and they do not always match the M1's — the re-measured allocation census is
**2.45×** the M1 figure.

1. **`MCC_AST_EVAL_LADDER_GPU=1` smashes the stack. One-line fix.**
   `src/mccast.c:15892` declares `int32_t pin[64], pout[128]` — 256 B and 512 B, sized
   for the pre-`989e4b3b` ABI of 1 in-slot / 2 out-slots. `MCC_GPU_IN_SLOTS` is now 2
   and `MCC_GPU_OUT_SLOTS` is 3, so the warm-up dispatch **reads 512 B from the 256 B
   buffer** (`src/mccgpu.c:1403`, Metal `:352`) and **writes 768 B into the 512 B one**
   (`:1510`, Metal `:388`). Under glibc's stack protector that is a hard SIGABRT, so
   **zero dispatches happen on any Linux host with a real ICD**; on Darwin it is a silent
   256-byte stack overwrite that happens to survive. `gpu/ladder-gpu-parity` fails
   correctly here via its `_disp EQUAL 0` `FATAL_ERROR`. **CI cannot see it**: the Linux
   cell installs `libvulkan-dev` — loader and headers, no ICD — so the cell green-skips.
   Fix: `int32_t pin[64 * MCC_GPU_IN_SLOTS], pout[64 * MCC_GPU_OUT_SLOTS];`

2. **The Vulkan dispatch frees resources under a live command buffer.**
   `src/mccgpu.c:1507-1509` waits 30 s; on any non-`VK_SUCCESS` it falls to `done:` at
   `:1515-1535` and destroys the fence, command pool, pipeline, pipeline layout, shader
   module, descriptor pool, descriptor set layout, mappings, memory and buffers. On
   `VK_TIMEOUT` the command buffer is still **pending**. Because buffers are created
   fresh per dispatch, the driver recycles the freed allocation into the next dispatch's
   `bin`/`bout` while a zombie kernel writes into it — **a timeout in dispatch N silently
   corrupts dispatch N+1**. Alongside: all ~13 Vulkan failure exits are diagnostically
   mute, and `mcc_gpu.ok` is never cleared after `VK_ERROR_DEVICE_LOST`, so
   `mcc_gpu_quiesce`'s unbounded `vkDeviceWaitIdle` from `atexit` deadlocks after a hang.
   The restructure needs a `submitted` flag and a second label that **destroys nothing**
   — leaking one dispatch's resources at a terminal error is strictly safer than a UAF
   against the GPU, and it is bounded because no further dispatch can occur.
   **Note the Metal half of this is already fixed** (`c6814625`); `docs/PLAN.md`'s old
   "`src/mccgpu.c:352-356`" paragraph was stale and has been corrected.
   **Neither bug is reachable by any existing test** — `src/mccgpu.c`'s Vulkan path has
   zero direct coverage, since `gpu/spv-slice-*` use `spvgate`'s own duplicated Vulkan
   code. Cheapest regression test: make the hardcoded 30 s fence a named tunable and run
   one cell at 1 ns with validation layers on. Works here and on lavapipe, needs no fault.

3. **`ast_replay_bb`'s 35 KB frame is 93% one array. One declaration, 9.1× less stack.**
   `SValue sv_stack[VSTACK_SIZE + 1]` (`src/mccast.c:5823`) is 32,832 B — `VSTACK_SIZE`
   is 512 and `sizeof(SValue)` is 64 because `CValue` carries a `long double` — declared
   unconditionally in the prologue of a recursive function, for an `AST_OP_ASMGEN` arm
   that only fires on inline asm. Measured by building two control trees in scratchpad:
   frame **35,424 → 2,592 B**, self-compile peak stack **1024 KiB → 112 KiB**, and the
   output objects are **byte-identical at `-O0`…`-O3`**. The shipping form must not be
   `static` — it has to survive the `longjmp` in `mcc_error` — so use an explicit
   save-area or move the ASMGEN arm into a `noinline` helper. ~10 lines.
   Two related findings: the 930 KiB/992 KiB peak **only exists at `-O1`+**, because
   `ast_replay_env` requires `optimize >= 1` (`-O0` peaks at 20,672 B, 49× less); and
   `MCC_MAX_UNARY_DEPTH 2048` is mis-sized. **Closed — see "The parse-depth guard"
   below; the per-level figure quoted here as 1.15 KiB is 1,088 B measured exactly.**

4. **Six binary opcodes have zero test coverage of any kind.**
   `TOK_UDIV`, `TOK_UMOD`, `TOK_PDIV`, `TOK_UGE`, `TOK_ULE`, `TOK_UGT` each have an MSL
   arm (`src/mccgpu.h`), a SPIR-V arm and a CPU-reference arm in `ast_eval_binop`, at 32
   **and** 64 bits, and nothing exercises them. (`TOK_SHR` and `TOK_ULT` have exactly one
   synthetic case each.) They are **structurally unreachable from harvested arenas**:
   `gen_op` rewrites `TOK_GE`→`TOK_UGE` at `src/mccgen.c:4455` *after* the arena records
   the token — the same mechanism behind `ee1fa9e0`. Measured 0 occurrences across
   24,562 harvested nodes. This is 23% of the binary-op axis, findable by **enumeration
   alone**, no fuzzing needed.

5. **`spvgate` reports `OK` for a case that lowered nothing.**
   `tools/spvgate.c:1250-1254` prints `SKIP (not lowerable)` and `continue`s; `:1335`
   then prints `OK` because `case_bad` is still 0. That is the
   `cmake/ladder_gpu_parity.cmake` "zero dispatches, so identical verdicts prove nothing"
   failure mode, un-closed one level down. It matters beyond hygiene: it means the claim
   *"the synthetic suite showed 0 mismatches either way — only real arenas discriminate"*
   **cannot currently be distinguished from "the synthetic cases compared 0 points"**,
   and `b_ll_cmpu` does build the discriminating shape. ~10 lines: print per-case
   `compared=` and fail a case at 0. Do this before trusting any generator's yield.

6. **`354e96f6` is half-landed, and it broke dump reproducibility.**
   The dump emits 12 fields plus `[inv]` callee lines (`src/mccast.c:13580`), but
   `rebuild_arena` still does `sscanf(...) != 7` (`tools/spvgate.c:1083`) — the five new
   fields have **no consumer**. Worse, two of them are raw `(uintptr_t)Sym*`, so two
   identical self-compiles now differ under ASLR (identical under `setarch -R`; the
   7-field prefix is still identical). **That invalidates the H4′ evidence** in
   `docs/PLAN.md` — "three `MCC_ARENA_DUMP` self-compiles byte-identical across all
   374,310 node records" was measured before this commit. Any bank keyed on the dump is
   unbankable until the pointers are interned.

7. **The untyped-readback diagnosis was wrong, and the fix is free.**
   The claim that types are missing because they live in the `st_*` side table is half
   right; the mechanism is worse. `st_*` is written only from `src/mccrir.c` and only
   when `rir_stamp_env` is set — `MCC_RIR_STAMP`, **off by default** (`:5949`). In
   production the type is not in a side table, it is **absent**. And the artifact is far
   broader than `Load`: **39,640 of 39,643 `Binary` nodes read back untyped** (9.2% of
   the arena, the most important kind for the emitters) against 7,195 `Load` at 1.7%.
   Typed-node coverage goes **65.8% → 100.0%** at `MCC_RIR_STAMP=2`, and `=0`/`=1`/`=2`
   produce **byte-identical objects** — verified `cmp`-clean at 3,302,415 bytes.

8. **Two dead memsets and a duplicated upload in the dispatch path.**
   `memset(pout, …)` (`src/mccgpu.c:1404`, Metal `:353`) is **100% dead** — every lane
   `< cap` writes all three out slots unconditionally and `out` is read only for
   `t < ntuple`. `memset(pin, …)` is needed only for the `[ntuple, cap)` tail, whose
   outputs are discarded and whose divisions are already guarded. And
   `ast_ladder_gpu_run` emits modules *a* and *b* and calls `mcc_gpu_run` twice with the
   **identical** `tin`, re-allocating and re-copying each time. Per lane per dispatch:
   56 B moved, **28 B dead**, 32 B duplicated. The existing cells are **lane-bound**
   (`gpu/spv-slice-real` is 1,659 dispatches / 23.1M lanes), so this is a bigger lever
   than the per-dispatch fixed cost. Measured on this host: **130 ms one-time Vulkan
   init, 117 µs per dispatch, 72 ns per lane**, with 24 ioctl / 3.9 openat / 2.3 mmap /
   2.1 munmap **per dispatch** — buffers are created *and destroyed* every time.

9. **`mcc_gpu_mem_index` picks the worst memory type on this machine.**
   `src/mccgpu.c:1307-1320` takes the **first** type matching
   `HOST_VISIBLE|HOST_COHERENT`. Here that is `memoryTypes[2]`, on the 46.5 GiB
   **system-RAM** heap and **not** `HOST_CACHED`. `memoryTypes[3]` adds `HOST_CACHED`;
   `memoryTypes[4]` is `DEVICE_LOCAL|HOST_VISIBLE|HOST_COHERENT` on the 11.94 GiB ReBAR
   heap. Under a B1-style address space every interpreted load and store would be a PCIe
   transaction. The measured ~0.4 GB/s effective bandwidth is consistent with an uncached
   write-combined mapping. Also `devs[0]` (`:1259`) is still chosen with no scoring, and
   `VkPhysicalDeviceLimits` is fully transcribed at `:604-711` while **only `deviceName`
   is ever read**.

10. **Nothing asserts that any skippable cell must run.**
    There are **138 `SKIP_RETURN_CODE 77` registrations**. The GPU cells *lie* —
    `cmake/ladder_gpu_parity.cmake` and `cmake/spvgate_real.cmake` `return()` with exit 0
    after zero work and ctest prints **PASS**. `rir-coverage` is honest — it exits 77 and
    names the reason — but *nothing anywhere says it must not skip on a given host*.
    Related, and currently unnoticed: `wide`'s `lowerable` in
    `tests/rir/coverage-bank.json` is still the **legacy flat form**, so
    `rir-coverage-census` silently 77-skips on **PE** as well as macho; and the
    `kept_coverage` gate added by `78d4856f` **has never been run against a clean HEAD
    build** — the bank is 29 commits stale. The general fix is a checked-in
    `test-name: hosts-where-SKIP-is-a-failure` manifest consumed by one post-ctest cell;
    `tools/ci.c`'s `FEATURES[]`/`GATE_CELLS[]` is the same idea one altitude too high.

## Top priority — the decisive wins from the GPU-execution study, 2026-08-07

Curated from [`docs/PLAN.md`](PLAN.md), which proposes moving AST/RIR execution onto the
GPU until only link-time `AST_Invoke` remains on the CPU. **These do not depend on that
plan being adopted.** Each is measured, each is cheap relative to its payoff. Ordered by
measured value per unit of work; item 0 is retained as a record of a withdrawn item and
item 4 is done.

**The headline that came out of the study:** the internal/external `AST_Invoke` split is
**87.65% internal / 12.12% external** (16,260 vs 2,248 of 18,550 sites). So the ceiling
is **95.0% static if all calls stay on the CPU, but 99.40% static and 99.80–99.98%
dynamic if internal calls run on device.** That 4.4-point gap is the whole difference
between a curiosity and a result, and it makes "internal calls stay on device" the
single load-bearing decision in the plan.

**The counterweight, now with a measured latency rather than an assumed one.** The
Metal round-trip is **144–180 µs** (not the 20 µs the plan assumed), and a persistent
doorbell kernel is **24 µs**. Against 944,327 crossings that is **1523×** the 0.093 s
baseline naively, and **still 16×** with device `str*`/`mem*` *and* a device allocator.
Only all four reductions together — doorbell, device str/mem, device allocator, file
staging — get under the baseline. So: **crossing reduction is a ~100× lever and the
doorbell is a 6× lever; reduce first.** And the offload unit must be the whole function
(75.9% of bodies contain zero external invokes), never the 24.8-node region, which at
150 µs costs 6 µs of boundary per node against a ~10 ns CPU node — a 600× floor.

**One hazard worth stating up front:** the doorbell works only because the GPU's L1 is
*not* coherent with the CPU mid-kernel. Host→device writes are invisible by every
qualifier MSL offers (there is no system scope, and all device atomics are forced to
`memory_order_relaxed`); the only thing that works is a **≥32 KB cache-eviction sweep
per poll**. Below that threshold it fails **silently as a hang** — 12 KB ran 516 rounds
then stopped. That constant is hardware-specific and spec-unsanctioned, and must be a
named, tested, tunable element rather than an implementation detail.

0. ~~**Accept two-child `AST_If` in both emitters.**~~ **Withdrawn — it was a category
   error, and the measurement that produced it was mis-framed.** The emitters are
   *expression* evaluators; a two-child `AST_If` is a statement-if, which has no value
   for them to produce. Cross-tabulating a real arena dump (2746 bodies / 419,936
   nodes, `MCC_ARENA_DUMP` with the build's own `-D`/`-I` set) by child count **and
   `op`**:

   | nchild | op | count | share of `AST_If` | what it is |
   | --- | --- | --- | --- | --- |
   | 2 | 0 | 8755 | 60.2% | statement `if`, no `else` — **no value** |
   | 3 | 0 | 1887 | 13.0% | statement `if`/`else` — **no value** |
   | 3 | 5 | **1678** | **11.5%** | **the ternary — the only value-producing form** |
   | 3 | 3 | 1402 | 9.6% | loop region — no value |
   | 2 | 2/6/4/8 | 778 | 5.4% | loop/region forms — no value |

   `op == 5 && nchild == 3` is exactly what `ast_abs_try` (`src/mccast.c:11515`) keys on
   for the ternary; `op == 0` is the statement-if checked at `:9028`, `:9256`, `:9888`;
   `op == 2` increments loop depth (`:4136`). So `src/mccgpu.h:1526`'s `nchild == 3`
   gate is **refusing statements, correctly** — the honest figure is that only **11.5%
   of `AST_If` nodes are ternaries at all**, not that 66% are wrongly refused.

   The datum is still valuable, but it argues for something else: **73% of `AST_If`
   nodes are statement control flow**, and reaching them needs the control-flow machine
   (cluster C in `PLAN.md`), not a wider expression emitter. Re-file it there.

1. **Bank the baseline node census — static and dynamic — before any device work.**
   Cheap, zero-risk, and it makes every later claim interpretable. The dynamic half
   does **not** need an interpreter, contrary to what the plan first assumed:
   `mcc -ftest-coverage` self-build (0.5 s) then a self-compile (0.93 s, 10× baseline)
   emits a 4.7 MB gcov-format `.tcov` (`src/objfmt/mccelf.c:1614-1646`) —
   **789,238,394 block executions** over 48,909 blocks, joining to 2380/2452 bodies
   (98.9% of nodes). Bank: the per-kind histogram both ways, the internal/external
   invoke split, and the dead-node fraction. **Why it matters:** 58.4% of bodies
   (50.3% of static nodes) never execute during a self-compile, and the top 250 bodies
   — 27% of static nodes — carry **97% of dynamic weight**. Any static-only percentage
   is half-vacuous without this.

2. ~~**int64 in the emitters, as a `uint2` pair.**~~ **DONE — `989e4b3b`.** Both
   emitters handle 64-bit as an emulated pair; ABI widened to `MCC_GPU_IN_SLOTS 2` /
   `MCC_GPU_OUT_SLOTS 3`. `spvgate` went 18 cases / 1.18M points → **38 cases /
   2,500,856 points**, plus 889 real slices at 58.2M points, ladder parity at 1767
   dispatches on both backends, and the `930921-1.c` shape from 0 dispatches to 11.
   `MCC_GPU_CODE_MAX` 8192 → 16384 words (SPIR-V only) for the ~500-word udiv64
   helper. No refusal was added anywhere.

   **It also exposed a latent bug in the reference evaluator, fixed in `ee1fa9e0`.**
   `ast_eval_binop` compared `TOK_LT/LE/GT/GE` as signed regardless of operand
   signedness. Invisible at 32 bits (narrowing makes signed and unsigned agree),
   divergent at 64. Reachable because `gen_op` rewrites `TOK_GE`→`TOK_UGE` at
   `src/mccgen.c:4455` but **the arena records the pre-rewrite token** — `unsigned
   long long a >= b` and `long long a >= b` both come back as op `0x9d`. Measured with
   the emitters unsigned-aware and the evaluator left signed: **1,382,356 mismatches
   over 42.2M points**; 0 with both fixed. Latent, not an active miscompile —
   compilers with and without the fix emit byte-identical objects for `src/mcc.c` at
   `-O0`/`-O2`/`-O3`. The oracle certifies slice equivalence, so a wrong verdict is a
   licence to rewrite incorrectly; it just had not been cashed in. **Note the
   synthetic suite showed 0 mismatches either way — only real arenas discriminate.**

2b. ~~**OPEN: the MSL emitter has no per-value differential in-tree.**~~ **DONE —
   `f716cf8d`.** `tools/spvgate.c` is dual-backend from one source (`SPVGATE_MSL`),
   registering `gpu/msl-slice-{differential,known-positive,real}`. Both arms report
   identical counters (38 cases / 2,500,856 points / 0 mismatches; 646 real slices /
   42.2M points / 0 mismatches) and both fail under `--mutate`. `gpu/` is now 8 cells.
   Two details worth keeping: the Metal arm drives `msl_*` directly rather than
   `mcc_gpu_emit`, because `--mutate` must splice between `msl_expr` and
   `msl_main_end` and `mcc_gpu_emit` has no such hook — an earlier scratch harness
   built on it **accepted `--mutate` and silently did nothing**, i.e. a gate that could
   not fail. And the cells are guarded on `APPLE AND MCC_GPU_LANG_MSL_VALUE`, not bare
   `APPLE`, because `mccgpu.c` compiles one device layer per `MCC_GPU_BACKEND` and a
   Darwin vulkan tree has no Metal layer to dispatch through.

2c. **OPEN, superseded framing: the old 2b text.** SPIR-V has
   `spvgate` (38 cases, bit-exact per point); Metal has only `gpu/ladder-gpu-parity`,
   which compares *verdicts*. The int64 MSL half was verified with a scratch harness
   that is not checked in, so today Metal's arithmetic is gated only by "the two
   oracles reached the same conclusion", not "every lane produced the same bits".
   The fix is **not** to check in the 879-line near-duplicate: make `tools/spvgate.c`
   itself dual-backend (only ~117 of its 1244 lines are Vulkan-specific) and add a
   second CMake target from the same source, mirroring how `src/mccgpu.h` already
   selects emitters from one file.

2c. ~~int64 site list~~ — superseded by the above. 16 refusal sites (7 MSL, 7 SPIR-V, 2 in
   the ladder hook), *not* the eight previously recorded. The CPU evaluator already
   handles int64, so this is emitter-and-ABI work only: the SPIR-V module declares just
   `SpvCapShader` and the storage buffer is a runtime array of 32-bit ints with
   `ArrayStride 4`, so the two-slot layout is needed either way. Prefer the emulated
   pair over the native `Int64` capability for the first rung — it needs no capability
   query, no feature-floor plumbing, and works on both backends unmodified. This
   unblocks `930921-1.c`, the one program in 600 known to have reached the ladder.

   **Sites verified 2026-08-07** — `src/mccgpu.h:500,512,522,539,552,564,601` (MSL),
   `:1382,1394,1404,1419,1432,1444,1479` (SPIR-V), `src/mccast.c:15774,15781` (ladder
   hook). **Not started, and deliberately so:** this is not a matter of deleting 16
   predicates. Each emitter needs 64-bit `add/sub/mul/div/mod/shift/cmp` with the same
   overflow-and-definedness modelling `ast_eval_binop` already does at 64 bits, *and*
   the dispatch ABI must widen from one `int32` slot per live-in and two per result to
   a two-slot encoding, which changes `mcc_gpu_dispatch`, the tuple packing in
   `ast_ladder_gpu_run`, and every existing emitter test. It is the largest item on
   this board by a wide margin and it touches the only GPU path that currently works
   and is guarded by five green cells. It should land as its own series with the
   differential harness extended first, not folded into an unrelated batch.

3. ~~**The four RIR opcodes that actually fire.**~~ **DONE — `5cffb874` (histogram)
   and `8da21c5e` (handlers).** The histogram is now empty. Output is bit-for-bit
   unchanged: the `src/mcc.c` object is byte-identical across 3,685,254 bytes.

   **The honest result is that the payoff is zero, not small.** These opcodes are
   information-free with respect to the arena — the reconcile/stamp machinery already
   reconstructs their effect from the captured vstack, which is exactly what the 95.6%
   figure in the audit corrections was measuring. What it bought: a clean histogram so
   any *future* dropped opcode is visible, **3607 corrected stale pointer types**
   (`rir_stamp_sv`'s retype loop bails on `VT_PTR`), a closed provenance hazard, and
   two checked invariants. If the board expected coverage here, it will not come from
   here — the 254,424 lost bytes are 88 bodies failing on replay *length*.

   **`IR_OP_LOAD` cannot be unblocked, established two ways.** It wraps the backend
   `load(r, sv)` (register materialization); the AST-level dereference is already
   modelled by the `RIR_M_LOAD` mark. And the `continue` at `src/mccrir.c:4678` is not
   LOAD-specific — removing it runs `rir_reconcile` mid-region and truncates the
   shadow stack: **used 2657 → 1833, 882 bodies mismatching, kept 84.3% → 34.2%.**

3b. ~~original item 3 text~~ — superseded. Add a **per-opcode histogram** at
   `src/mccrir.c:3264`, excluding `JMP`/`JMPCOND`/`JMPADDR`/`JMPAPPEND`/`GSYMADDR`
   (which are handled in other switches and are 68% of that arm's traffic). Then handle
   `RETVAL`, `MKPTR`, `VPUSHSYM`, and unblock `LOAD` from the `continue` at
   `src/mccrir.c:4678-4681`. **Four features, not 25** — 21 of the 25 fire zero times on
   this target and five are `#ifdef`-ed out on arm64.

4. ~~**Quote `kept_coverage`, not `modelled`.**~~ **DONE, and the diagnosis was
   half-wrong.** The tool already *printed* both (`tools/rir-coverage.py:943-945`); what
   it did not do was **ratchet** `kept_coverage` — banked at `:925`, never enforced, so
   the number meaning "body bytes that ship optimized" could regress freely. A gate now
   sits beside the modelled check. Also corrected: the honest whole-corpus kept figure
   is **96–98%**, not the 81.4% I quoted — that was `src/mcc.c` alone, which is an
   outlier. **Remaining work:** `rir-coverage` skips entirely on Darwin
   (`:836-848`, no banked lowerable floors for macho), so this gate is enforced only on
   elf/pe. Decide whether to bank macho floors.

5. ~~**Give the GPU cells teeth.**~~ **DONE (mechanism); one CI flip left, deliberately
   not made blind.** Added `MCC_GPU_REQUIRED` (a `mcc_config_node` BOOL, default OFF)
   which turns the three "no usable device, skipping" early-returns into
   `FATAL_ERROR`s — `cmake/ladder_gpu_parity.cmake`, `cmake/spvgate_real.cmake`,
   `cmake/spvgate_mutate.cmake`, threaded through their `add_test` invocations.
   Verified all three directions: **device + `ON` → 5/5 pass**; **no device + `ON` →
   fails with the explanatory message**; **no device + `OFF` → skips and passes**, so a
   developer machine without a GPU is unaffected. Portable to the declared CMake 3.22
   minimum (`cmake_language(EXIT)` would need 3.29).

   **Left open on purpose:** setting `MCC_GPU_REQUIRED=ON` on the `gpu-vulkan` CI cell
   (`tools/ci.c`) is a one-line change, but whether the GitHub `macos-15` runner
   actually exposes a usable device is unknown from here — the "1413 dispatches"
   evidence in commit e3882880 is local, not CI. Flipping it blind could turn a green
   cell red; flipping it *knowingly* is the entire point. Run it once and see.

5b. **Original finding, retained.** Measured: **CI would stay green if both device backends
   were deleted.** `cmake/ladder_gpu_parity.cmake:27-30` matches `available=0` and
   returns exit 0; `spvgate_real.cmake:27` and `spvgate_mutate.cmake:4` skip and
   succeed; Linux CI installs `libvulkan-dev` (loader + headers, **no ICD**) and Windows
   installs headers only. The `gpu-vulkan` cell is macOS + MoltenVK, and nothing asserts
   its device is real. Fix: every GPU cell emits a dispatch count and `FATAL_ERROR`s at
   zero — the tooth `ladder_gpu_parity.cmake:47-50` already has — and add
   `mesa-vulkan-drivers` (lavapipe) to a Linux cell, one apt package that makes the
   Vulkan arm tested on the only OS where Vulkan is the default backend.

6. ~~**Build `spvgate` locally.**~~ **DONE — and the diagnosis was wrong.** It is not
   the headers: `/opt/homebrew/opt/vulkan-headers/include/vulkan/vulkan.h` **exists**
   (keg-only, unlinked). What is missing is the **loader** — there is no
   `/opt/homebrew/lib/libvulkan*`. `spvgate` calls `vkCreateInstance` directly and
   deliberately avoids `mccgpu.c`'s dlopen path, so it needs something to link against;
   MoltenVK exports the Vulkan entry points and serves. Recipe:

   ```
   cmake -S . -B <build> \
     -DVulkan_INCLUDE_DIR=/opt/homebrew/opt/vulkan-headers/include \
     -DVulkan_LIBRARY=/opt/homebrew/lib/libMoltenVK.dylib
   ```

   Result: `spvgate` builds and runs on the M1 Pro — **18 cases, 72 dispatches,
   1,184,616 lanes, 1,126,578 compared, 58,038 vacuous, 0 mismatches** — and all three
   previously-unbuilt cells go live. Full local GPU suite now **5/5 passing**:
   `ladder-gpu-parity`, `spv-slice-differential`, `spv-slice-known-positive`,
   `spv-slice-real`. **Worth landing as a CMake hint** so Darwin developers get the
   SPIR-V gate without knowing this incantation — currently a Darwin checkout silently
   tests one GPU cell instead of four. (Harmless link warning: MoltenVK is built for
   macOS 12.0, the project targets 11.0.)

7. **Raise the emitter caps, and fix the one that binds first.** `SPV_MAX_CONST`/
   `MSL_MAX_CONST` = 512 distinct constants (`src/mccgpu.h:764`, `:105`) — a 2049-node
   arithmetic chain fails on the **constant cache**, not on module size. Measured
   emitter cost is 11.7–20.4 SPIR-V words per node, so `MCC_GPU_CODE_MAX`'s 8192 words
   is ~400–700 nodes; the largest real invoke-free region is 1114 nodes. Neither cap is
   a device limit — Vulkan sets no module-size bound.

8. ~~**Measure the Metal/Vulkan dispatch round-trip.**~~ **DONE — and it refutes the
   plan's central assumption by 7.5×.** Measured on the M1 Pro, separate processes,
   fresh buffers, magic-token-verified, N=2000–3000 after warm-up:

   | mechanism | median | p99 |
   | --- | ---: | ---: |
   | dispatch-per-region (`waitUntilCompleted`) | **144–180 µs** | 224–240 |
   | same, spin-poll shared memory | 101–105 µs | 159–169 |
   | **persistent kernel + doorbell** | **24 µs** | 56 |
   | pipelined submit, never waiting | 19.5 µs/CB (throughput, not latency) | |

   Against 944,327 crossings and a 0.093 s baseline, dispatch-per-region is **141.7 s =
   1523× slower**, not the 203× the 20 µs assumption predicted. Even with device
   `str*`/`mem*` **and** a device allocator it is **1.45 s = 16×**. Only
   doorbell + device str/mem + device allocator + file staging — **all four** — gets
   under the baseline (0.068 s). **Crossing reduction is a ~100× lever; the doorbell is
   a 6× lever. Reduce first.**

9. ~~**The Metal path reports device failures as success.**~~ **DONE, fixed.**
   `src/mccgpu.c` called `waitUntilCompleted` and then unconditionally `memcpy`d the
   output and set `rc = 1`, never reading `[cb status]` or `[cb error]` — so a watchdog
   kill, page fault or hang was reported as **a successful dispatch with garbage
   output**. Now checks status against `MTLCommandBufferStatusCompleted` and refuses via
   the existing `mtl_report_err`. Verified: dispatch still succeeds
   (`dispatches=1 lanes=64`) and `gpu/ladder-gpu-parity` passes. This mattered because
   the failure surfaces are real and were being swallowed: `…ErrorImpactingInteractivity`
   (occupancy kill), `…ErrorPageFault` (OOB device write), `…ErrorHang`, and
   `…ErrorInnocentVictim` — an unrelated command buffer discarded because another faulted.

10. ~~**Stop destroying device buffers per dispatch — free performance.**~~
    **WITHDRAWN — the premise does not reproduce.** The claim was +56 µs per dispatch
    (39% of the round-trip) for allocating fresh buffers. Implemented buffer holding
    (grow-on-demand, released at quiesce), verified correct across a 64→512→64→4096→
    128→16384→64 size sequence with every lane checked, then **A/B'd it against
    fresh-per-dispatch in the same binary**:

    | ntuple | in/out bytes | fresh | held |
    | ---: | --- | ---: | ---: |
    | 64 | 256 B / 512 B | 192 / 190 µs | 193 / 188 µs |
    | 1024 | 4 KB / 8 KB | 193 µs | 202 µs |
    | 16384 | 64 KB / 128 KB | 226 µs | 226 µs |
    | 262144 | 1 MB / 2 MB | 342 µs | 346 µs |

    **No benefit at any size**, including the 4 KB case the original measurement used —
    `newBufferWithLength:` is cheap on unified memory, and the ~190 µs floor is
    dominated by fixed dispatch cost. The change was reverted rather than shipped on a
    refuted rationale. When Phase 1 builds a device-resident address space the buffer
    lifetime will be driven by *that* design, not by this stub. The untested half of the
    original item — spin-polling a result word instead of `waitUntilCompleted`, claimed
    at −45 µs — remains open and is the more promising of the two.

11. **Add a tree-recursion exec golden.** `tests/exec/functions_abi/recursion.c` is
    *linear* `factorial`, which MSL silently linearizes into a loop — it passes without
    exercising recursion at all. Measured: MSL **compiles** `fib(n)=fib(n-1)+fib(n-2)`
    with runtime `n` and then **hangs the GPU at n=5**
    (`kIOGPUCommandBufferCallbackErrorHang`), taking sibling command buffers with it as
    innocent victims. The failure mode is silent acceptance, not a compile error, so
    only a tree-recursive golden will catch it. Also already in tree and unused as
    conformance: `tests/diff/parts/legacy_expr.h:60-95 goto_test()` — computed goto
    through a label table **jumping into a `for`-loop body**, the unstructured-entry
    case; and `tests/exec/codegen/nodata_wanted.c:48,76` label *arithmetic*
    (`&&te0 - &&ts0`), which no device model handles.

## Two bugs found by widening the test matrix — 2026-08-07

Both were found by enabling `MCC_ENABLE_CROSS=ON` and running the docker/wine/qemu
cells, which are not exercised by a default Darwin build.

1. **`src/mcc.c` forced every linker on Darwin to supply `libobjc`, and four separate
   places had to learn it the hard way. Fixed at the root instead.**

   Symptoms, all `unresolved reference to '_objc_msgSend' / '_objc_getClass' /
   '_sel_registerName'`, all confirmed pre-existing by stashing every local change and
   reproducing on a pristine tree:

   | cell | why it linked `src/mcc.c` without `-lobjc` |
   | --- | --- |
   | `cross-factory`, `cross-factory-i386` | `tools/build.c` adds `-lm -ldl -lpthread` under `MCC_HOST_POSIX`, nothing for Metal |
   | `run-tier/x86_64-osx`, `run-tier/arm64-osx` | the Mach-O bootstrap in `tools/run-tier.sh` passes `$SDKL` only |
   | `macho-embedjit-arm64-osx` | `--embed-jit` bakes the JIT engine archive into a **user's** program |

   Commit `d3b76220` fixed the CMake targets by adding `objc` to
   `MCC_COMPUTE_BACKEND_LIBS`; it could not fix the script-driven ones, and it
   fundamentally cannot fix `--embed-jit`, where the program being linked belongs to
   the user. Patching each site with `-lobjc` would have been three workarounds and
   left `--embed-jit` a product wart.

   **The file already knew the answer.** `src/mccgpu.c:32-33` says *"fegetenv/fesetenv
   are resolved dynamically for the same reason the drivers are: an --embed-jit program
   has no business linking libm."* The Objective-C runtime is the same case and was the
   one thing still resolved at link time. **Fixed** by resolving `objc_getClass`,
   `sel_registerName` and `objc_msgSend` through `host_dlsym_process` at Metal-load
   time, with a `/usr/lib/libobjc.A.dylib` fallback and a clean refusal if absent —
   exactly the shape the `fegetenv` block above it already had.

   Result: **`nm -u cmake-gpu/mcc` reports no undefined `objc` symbols at all**, the
   GPU still dispatches (`available=1 device=Apple M1 Pro`), and all seven affected
   cells pass — `cross-factory`, `cross-factory-i386`, `run-tier/{x86_64,arm64}-osx`,
   `macho-embedjit-arm64-osx`, plus both GPU cells. The three `-lobjc` workarounds I
   had already written were reverted; the root fix carries all of them. Note
   `MCC_COMPUTE_BACKEND_LIBS objc` in `CMakeLists.txt:526` is now redundant too, left
   in place as belt-and-braces rather than churned in the same change.

3. **`runtime-bench-check` fails on a pristine tree — pre-existing, not investigated.**
   `FAIL branchy [defaults]: output mismatch` — wants `487419720 122294685.000000`,
   gets `-7621192680 -921869400.000000`. The magnitudes suggest a width/accumulation
   difference rather than a link or environment problem. Confirmed pre-existing by
   stashing all local work. Left open: it is unrelated to anything in this batch, and
   guessing at it would have mixed an unrelated fix into a GPU/RIR change set.

2. **Every docker cell fails misleadingly when the build directory is outside Docker's
   shared paths.** On macOS a bind mount of an unshared path (e.g. anything under
   `/private/tmp`) silently produces an **empty** directory inside the container rather
   than an error, so the host-side objects the test just wrote are invisible and the
   container reports `cannot find m0.o` or `gcc: error: def.o: No such file`. Seven
   cells failed this way purely because of *where the build tree lived*; the same cells
   pass from a tree under `$HOME`. **Fixed:** `dg_need_mount` in `tools/dockergate.sh`
   writes a sentinel, checks it is visible inside a container, and `dg_skip`s (77) with
   an actionable message otherwise. Wired into all 21 docker scripts. Verified both
   directions: unshared path → `rc=77` with the explanation, shared path → `rc=0` and
   the test runs. This converts a confusing red into an honest skip, which is the same
   posture the repo already takes for a missing device or a missing toolchain.

## Open, in the order the measurements rank them — 2026-08-07

Everything here is measured or reproduced, not speculative.

1. **Audit `IR_OP_*` for missing `mccrir.c` cases.** `IR_OP_FABS` had none, and the
   consequence was not a wrong answer but a silently unfaithful body, which skips
   *every* AST optimization for that function. One opcode disabled the optimizer
   wherever it appeared. Nothing says it is the only one.
2. **The gate-mask gap.** `ast_math_inline_env`, `ast_interchange`, `ast_fusion`,
   `ast_tile` and `loop-vlat` mutate the arena before the JIT's mask snapshot and
   have no `AST_SG_*` bit, so the JIT can never search them — a demotion of any of
   them is permanent, and a promotion only reaches the JIT via the default level.
3. **The 34 demoted rows on rungs 10/11/12**, in that order: 10 is measured
   pessimization, 11 is compile cost with no payoff, 12 is no effect and no cost —
   the last group being deletion candidates rather than repair candidates.
4. **The detached JIT workers have no exit quiesce.** The `[ladder-gpu]` atexit
   report printed *before* a worker's fault, i.e. compiler code runs on a worker
   while `exit()` is draining atexit handlers. Needs a join/shutdown path in
   `mccjit_pool_start`.
5. **`storeval-callstore`'s level is now unjustified in both directions** — the ICE
   that made its off-state unmeasurable is fixed, so it can finally be ranked.
6. **`selfhost-optbench --check` has not been re-run** since the reemit-templates
   cost fell; no row moved, but the greedy boundaries are not provably unshifted.
7. ~~**`tests/ast/o0-baseline/` is stale at HEAD** — 277 banked files against 303 in
   the tree.~~ **CLOSED 2026-08-09, `wt/gateall`.** Re-taken at 304 files on all twelve
   keys and registered as `ast/o0-baseline`, so it can no longer go stale unnoticed. The
   re-bank was not routine: 28 objects with byte-identical sources had moved. See the
   registration sweep. The thirteen `*.gated.*` files remain frozen on filed item 17.
8. **`if-conversion-abs`** ships on a +2.13% margin against a 1.76% floor, the
   thinnest thing on the ladder.

Known flake, not a defect: `run-tier/i386-win32` can fail at `-j32` with `wine
client error: recvmsg: Connection reset by peer`, and passes on rerun or at `-j24`.

## Each suite is now scored by the other vendor's compiler — 2026-08-06

`tools/xoracle.py` judges gcc's tests with clang and clang's with gcc, on exit
status and stdout bytes, never on a suite's own directives. `qualify` builds each
run-mode test with the oracle at `-O0` *and* `-O2`, runs both, and runs the `-O0`
binary twice; a test joins the oracle set only if all three agree, which is what
keeps UB-sensitive and nondeterministic programs from later surfacing as phantom
miscompiles (23 of 5352 excluded on exactly that basis). `check` replays the
cached set against one mcc configuration, so configurations diff test-by-test.

**The llvm direction needed rescuing.** clang's suite is compile-and-FileCheck:
of 10861 C files just 14 are execution tests. The real execution corpus is
`compiler-rt/test/builtins/Unit`, which `xsuite.py` skips wholesale as
`runtime-lib-suite`. Pairing `X_test.c` with `lib/builtins/X.c` and compiling both
with the compiler under test lifts that direction 7 → 139 cases.

`LINK_POLICY` is kept distinct from `MCC_NOBUILD`: mcc embeds `libmccrt.a`, which
already defines `__udivmodti4`, so linking compiler-rt's copy collides where
gcc's lazily-resolved libgcc does not. Folding that into a failure count would
overstate the defect rate.

4907 qualified cases: **-O0 96.70%, -O2 96.56%**, 72 behavioural divergences at
both levels with identical membership — so front-end/semantic, not optimizer. The
12 on the builtins side are all `tf`/`ti` (`__float128`, `long double`,
`__int128`), which `ast_bad_type()` already rejects. Re-run after the CType
stamping, width ladder and SPIR-V commits: **0 per-test verdict changes** at
either level.

## Byte coverage is a proxy. The real metric is the lowerable census — 2026-08-06

Byte coverage only proves the arena can regenerate **the same target's** code. The goal is
retargeting slices between anonymous `invoke` nodes to GPU compute shaders, which is
strictly stronger. The **lowerable census** measures that directly and lives beside
`modelled`/`gap`, not instead of it — byte coverage is still the single-target regression
detector and is untouched.

The predicate is **one function**, `ast_low_node()` in `src/mccast.c`. Tightening or
loosening it is a one-place change. Every arena node gets exactly one class:

| class | meaning |
| --- | --- |
| `ok` | none of the below |
| `asm` | an inline-asm node (`AST_OP_ASM`/`ASMGEN`/`ASMOPS`) |
| `reg` | a `Ref` naming host machine state: a hardware register (`valmask < VT_CONST`), or `VT_CMP`/`VT_JMP`/`VT_JMPI` |
| `opaque` | `AST_Poison`, or an op with no portable meaning: VLA, VLA_RESTORE, VAARG, VASTART, GGOTO, the atomics, the complex builders |
| `call` | `AST_Invoke` — a slice is the code *between* anonymous invokes, so an interior invoke ends the region by definition |
| `type` | not type-complete: `ast_bad_type()`, a `Convert` with no operand, or a dereference (`Load`/`Store`/MEMBER/MEMBER_ARROW) whose address operand derives from no node carrying a pointer/array/struct type |
| `frame` | the value's meaning is a host frame offset: `AST_OP_ADDR`, a `VT_LLOCAL` `Ref`, or a `VT_LOCAL` `Ref` (see levels) |
| `global` | a `Ref` carrying `VT_SYM` — a host link-time address |

A node is **clean** when its whole subtree is `ok`, i.e. it roots a maximal lowerable
region. A body is lowerable when every node in its arena is `ok`.

The `type` clause independently rediscovered the lost-cast defect from the arena alone,
with no replay, and `rir_hook_cast_type()` closed it on 2026-08-06. What that bought the
lowerable census was almost nothing, and **that null result is the most useful thing this
census has said so far**. Splitting `type` into its three sub-clauses over the self corpus
at `-O2` (temporarily relabelling them, then reverting):

| sub-clause | share of nodes | blocks | SOLE blocker of | after the cast fix |
| --- | --- | --- | --- | --- |
| `ast_bad_type()` — struct, bitfield, `long double`, `__int128` | 0.823% | 42.597% of body bytes | 0.127% | 0.822% / 42.576% / 0.127% |
| deref whose address operand derives from no typed node | 0.706% | 50.959% of body bytes | 0.237% | 0.705% / 50.840% / 0.233% |
| `Convert` with no operand | ~0 | ~0 | ~0 | unchanged |

The deref clause barely moved even though **every** `abort` in the corpus was fixed,
because the lost cast was never what dominated it. The real cause is that the arena is
**type-sparse**: gate `ast_low_node()` on the node's static type alone and **35.6% of
arena nodes carry no static type at all**, in 100% of bodies. `ast_low_base_ptr()` is a
depth-8 search *around* that hole.

### Closing the hole — `MCC_RIR_STAMP`, 2026-08-06

`rir_to_arena` now stamps every node with the static `CType` of the value it produces.
Untyped share on the self corpus, `-O0`/`-O1`/`-O2`/`-O3`, 407,913 nodes:

| | unknown | explicitly void |
| --- | --- | --- |
| `MCC_RIR_STAMP=0` (default) | 35.570% / 35.598% | 0 |
| `MCC_RIR_STAMP=1` observed only | 17.618% | 3.574% |
| `MCC_RIR_STAMP=2` observed + derived | **0.010%** (42 nodes) | 14.95% |

`=1` alone leaves every value-producing node typed but nothing else: what remains at that
level is `Store` 21,044, `BasicBlock` 19,127, `If` 12,625, `Invoke` 8,809, `Jump` 5,392,
`Return` 4,707 — statements and void calls, all of which `=2` resolves structurally.
Value nodes are already done at `=1`: `Binary` 88, `Load` 67, `Unary` 8, `Ref`/`Literal`/
`Convert`/`StoreVal` 0.

Three channels, all in `src/mccrir.c`:

1. **Observed** — `rir_stamp_deep()` runs at the head of `rir_stamp_sv()` and, for every
   shadow-stack slot holding a still-untyped node, defers `(node, kind, CType)` from the
   parser's own `SValue`. `rir_stamp_flush()` replays the log at the end of
   `rir_to_arena()`, first observation wins. This is the parser's answer, not a re-derivation.
2. **Leaves** — `rir_leaf_slot()` stamps at creation, which is the only way to reach the
   14,485 dead unparented `VT_CONST` `Literal`s (3.6% of the whole arena!) that the vstack
   refill in `rir_reconcile_sv()` creates at slot 0 and nothing ever consumes.
3. **Derived** (`=2` only) — `rir_stamp_derive()`: pointer arithmetic (`Binary +`/`-` with a
   pointer operand), dereference (`Load` of a `VT_PTR`), `Store`/`StoreVal`/`Return` from
   their operand, `Invoke` from the callee `Sym`'s return type, and control nodes
   (`BasicBlock`, `If`, `Jump`) as explicitly void.

**`VT_VOID` is `0`, so "void" and "unset" are the same bit pattern** in `type_t`. A shader
generator must tell "this node yields nothing" from "we do not know what this node yields",
so the stamp carries its own `st_have` bit. 14.95% of the arena is genuinely void:
19,127 `BasicBlock` + 12,626 `If` + 5,392 `Jump` + 14,485 dead literals + 8,732 `void`
calls + 562 `void` returns.

**What is still unknown is 42 nodes**: 34 `Binary` whose result the vstack never witnessed
(folded address arithmetic retained as a child) and 8 `Unary` inline-asm ops, which have no
C type by construction.

The wide corpus agrees and is a wider sample of C: 1,052,863 nodes at `-O2`, **29.442% →
0.014%** unknown (151 nodes — 107 inline-asm `Unary`, 44 `Binary`), 12.890% explicitly void.

**The stamp does NOT live in `type_t`, and that is the load-bearing finding.** The first
implementation wrote the observed types straight into `type_t`/`type_ref`/`type_bp`/`type_bs`
— the columns that were "already there and simply unset". With the toggle on that
**miscompiled**: `exec/cast_operator`, `exec/complex`, `exec/integer_promotion`,
`exec/lex_extras`, `fuzz/smoke` and both `selfhost-output-parity` cells failed, `__va_arg_inline`
started aborting the replay with `invalid operand types for binary operation`, and `kept`
fell 96.0% → 93.0%. `type_t` is not a *description* of the node, it is the **emitter's own
input**: `ast_replay_*` and every AST strategy pass read it, and they were all written
against an arena where `t == 0` means "leave this alone". Adding true facts to that column
changes what the compiler emits.

So the static type is a **second, parallel view**: `st_have`/`st_t`/`st_ref`/`st_bp`/`st_bs`,
lazily allocated exactly like `wide_hi`/`wide_r2`, read through `ast_stype_t()` /
`ast_stype_ref()` / `ast_stype_bp()` / `ast_stype_bs()` / `ast_stype_known()`, which fall
back to `type_t` when it is set. Nothing in the emitter reads them. `ast_low_node()` and
`ast_low_base_ptr()` do. Result: **`src/mcc.c` objects are byte-identical with the toggle
on and off at all four levels**, and the whole 8845-cell suite is green with
`MCC_RIR_STAMP=2` except the two census ratchets, which move by design.

### What type-completeness costs the lowerable census — it is worse, and that is the point

Same binary, same corpus (2631 bodies, 407,913 nodes), level-1 (default) locals:

| | `-O0` | `-O1` | `-O2`/`-O3` |
| --- | --- | --- | --- |
| whole bodies lowerable | 9.205% → **7.684%** | 9.236% → **7.678%** | 9.236% → **7.678%** |
| of modelled body bytes | 2.257% → **1.572%** | 2.278% → **1.559%** | 2.186% → **1.528%** |
| nodes in a maximal region | 41.755% → 41.432% | 41.747% → 41.424% | 41.747% → 41.424% |
| nodes in regions ≥16 | 5.014% → 4.075% | 5.018% → 4.078% | 5.018% → 4.078% |
| `type` nodes_pct | 1.518% → **2.313%** | 1.519% → **2.315%** | 1.519% → **2.315%** |
| `type` sole_bytes_pct | 0.377% → **1.062%** | 0.416% → **1.135%** | 0.367% → **1.025%** |
| `type` bytes blocked | 62.234% → 74.537% | 62.178% → 74.538% | 61.297% → 73.717% |
| `frame` sole_bytes_pct | 0.256% → 0.025% | 0.257% → 0.025% | 0.249% → 0.026% |
| `global` sole_bytes_pct | 1.224% → 0.799% | 1.226% → 0.801% | 1.242% → 0.805% |

**The census was flattered by the hole.** `ast_bad_type(0)` is false, so an untyped node
scored `ok` — the predicate said "lowerable" about values whose width and signedness it did
not know. With the hole closed, 0.80% of all arena nodes turn out to carry a type no shader
can represent (struct, bitfield, `long double`, `__int128`), `type` overtakes `frame` and
`global` as a sole blocker, and 1.56 points of "lowerable bodies" evaporate. The equivalence
oracle could never have run on the 9.2% figure; 7.68% is the number that was always true.

Byte coverage is unmoved by the toggle at every level (100.000% / 100.000% / 99.965% /
99.965%, gap 0 / 0 / 467 B / 467 B), which is the proof that the two views are independent.

**Cost.** Compile time for `src/mcc.c`, best of 3, two independent runs: `-O0` −1.8%..−1.0%,
`-O1` +1.7%..+2.6%, `-O2` +2.0%, `-O3` +0.4%..+1.8% — at the noise floor. Peak RSS: `-O0`
+0.6%/−0.2%, `-O2` +2.0%/+1.6% at `=1`/`=2`. The five shadow columns are 15 B/node and are
`calloc`ed only on the first `ast_set_stype()`, so with the toggle off they cost one NULL
test per query and nothing else.

**Next.** The `type` class is now dominated by `ast_bad_type()` — aggregates and the
non-portable scalars — not by missing information. Splitting a struct-typed value into its
scalar fields, or admitting fixed-layout aggregates into the shader ABI, is what moves it
now. `ast_low_base_ptr()`'s depth-8 search is dead weight under `MCC_RIR_STAMP=2` and can be
deleted once the toggle becomes the default.

**The `VT_LOCAL` rule is the one genuinely open question** (a local's `ival` is the
parser's own frame offset), so it is a level and all three are measured in one walk:

- **0 strict** — every `VT_LOCAL` `Ref` is frame-dependent.
- **1 default** — a `VT_LOCAL` `Ref` is frame-*in*dependent iff no node anywhere in the same
  arena takes the address of a local (no `AST_OP_ADDR`, no `VT_LLOCAL` `Ref`) **and** the
  `Ref`'s own type is a scalar the arena models. With no address-of-local in the body no
  slot's address is observable, so every slot is a pure value cell and its offset is only a
  name.
- **2 loose** — a `VT_LOCAL` `Ref` never disqualifies.

Pointer *values* are allowed at every level: a pointer reaching a region is a buffer
binding to resolve at the region boundary, not a frame dependency.

Numbers at `2aeb2989`, identical across `-O0..-O3` to within 0.1pp and byte-identical
across six consecutive runs:

| | self | wide |
| --- | --- | --- |
| whole bodies lowerable | 9.15–9.18% of bodies, 2.17–2.27% of body bytes | 15.62–15.63% of bodies, 1.19–1.22% of body bytes |
| nodes inside a maximal lowerable region (strict / default / loose) | 26.26 / 41.61 / 66.12% | 27.12 / 34.89 / 64.25% |
| nodes in regions ≥3 nodes (default) | 16.71–16.73% | 9.74–9.75% |
| nodes in regions ≥16 nodes (default) | 4.97% | 2.92% |

**What dominates.** By nodes rejected, `frame` is far and away the largest disqualifier —
13.7% of self nodes and **25.1% of wide nodes**, roughly triple the next class. `global`
(9.1% self / 10.9% wide) and `call` (5.1% / 8.7%) follow; `type` is only 1.5% / 0.9% of
nodes but blocks 62% / 80% of body *bytes* because it lands in the big bodies. `asm` and
`opaque` are noise (<0.02%). So the single most valuable thing to settle is whether the
arena needs a value layer above `AST_Ref`'s frame offsets: moving `frame` from level 0 to
level 2 alone lifts region coverage from 26% to 66% on self and 27% to 64% on wide.

Provisional, in order of how much they would move the number:

1. The **region boundary** is "maximal clean subtree". The real definition is maximal
   call-free slices between anonymous invokes; when that lands, only the maximality test in
   `ast_low_census()` changes.
2. The **locals verdict** is level 1 by assertion, not by an alias audit. An audit that
   distinguishes address-taken from genuinely-value `Ref`s replaces the whole-body `pinned`
   flag with a per-slot one.
3. `global` is rejected outright. A `Ref` to a read-only global array is exactly a shader
   buffer binding, so some of that 9–11% is recoverable, not a real blocker.
4. Region size thresholds are 3 and 16 nodes (`AST_LOW_MIN_REGION`/`AST_LOW_BIG_REGION` in
   `src/mccrir.h`), picked to match the existing slice window and a plausible kernel.

Run it with `ctest -R rir-coverage` (self, gated) or `ctest -L census` (wide). The predicate
itself is pinned per class by `ctest -R rir-lowerable-classes` against `tests/rir/low/*.c`,
one file per class — that cell is what catches a predicate that silently stops firing.
`MCC_RIR_LOW_DUMP=<func>` prints the per-node classification of one body.
**Only percentages are banked, never totals.**
## The lowerable ratchet is self-referential — read before re-banking

`tools/rir-coverage.py`'s `lowerable` floors are measured over `src/mcc.c`, which
amalgamates the compiler's own source. **Any code added to `src/` moves the number**, and
diagnostic code moves it the wrong way: the `MCC_AST_REFCENSUS`/`MCC_AST_FRAMEPERT`
instrumentation added 207 lines of env parsing, file I/O and classification to
`src/mccast.c`, all of it inherently non-lowerable, and `bodies_pct` fell 9.1821% ->
9.1048% on the self corpus at every level. The ratchet fired correctly; nothing regressed
in capability. Instrumenting for lowerability made lowerability measure worse.

Consequences, in order:
- **A `lowerable` regression is not evidence of a defect until you have checked whether
  `src/` grew.** Diff the corpus first, then investigate.
- Re-bank with `python3 tools/rir-coverage.py <build-dir> --update-bank-low`. It prints
  the pre-bank check's FAIL lines and then banks, which reads alarmingly; the banked value
  afterwards is what counts.
- The metric needs one of: a tolerance band, exclusion of diagnostic-only code from the
  corpus, or a corpus that is not the compiler itself. Until then this will recur on every
  substantial `src/` change, and the temptation each time is to re-bank without checking
  which of the two causes applies.

The byte-coverage floors do not have this problem to the same degree — they are a ratio of
modelled to emitted bytes, so new code lands on both sides.

## How frame-bound is the arena? — measured 2026-08-06

> **Framing correction, same day.** This section was written to test whether frame-bound
> Refs obstruct lowering slices to GPU compute shaders. **They do not, and the question is
> retired.** An address is a 64-bit integer and a shader computes it exactly: `tests/gpu/
> rev64.comp` already takes a base address as `base_lo`/`base_hi` push constants and does
> `add64`/`mul64` over `uvec2` with `umulExtended`, on hardware with no native 64-bit int.
> A frame offset is an operand, not an obstacle; the memory it reaches is a buffer
> binding. The `frame`, `global` and `laddr` classes that the lowerable census counts as
> disqualifiers are all just integers, and counting them as blockers understates the
> lowerable fraction badly. What genuinely cannot lower is what is not computation over
> buffers: an opaque `invoke`, host machine code (`asm`, <0.02% of nodes), and — the real
> one — **a lost static type**, because exact arithmetic requires knowing the width and
> signedness of what is being computed. The measurements below stand as measurements; the
> conclusion drawn from them does not.


A local is not named in the arena. `rir_leaf_slot` writes the parser's `sv->c.i` into the
node's `ival` and that integer **is** the variable's identity: `ast_du_build` hashes slots
by `int off`, `ast_cprop_escapes`/`ast_local_is_readonly`/`ast_promo_off`/`ast_argsub_off`
and `ast_eval_slice_livein` all key on it, and `gfunc_epilog` derives the frame size from
`loc` alone — nothing in the arena states it, which is why `ast_reemit_with_gates` has to
rescan every Ref for the minimum offset before replaying.

Two diagnostics, both off by default, quantify this.

`MCC_AST_REFCENSUS=<path>` dumps one line per `AST_Ref` at `ast_func_end`: class, offset,
`op`, `type_t`, the size of the maximal call-free subtree containing it, and a slice id.
Over amalgamated `src/mcc.c` (2307 functions, 356941 nodes, 101011 Refs) and `tests/exec`
(1285 functions, 25238 Refs, excluding the `translation_limits.c` outlier):

| class | mcc.c | tests/exec | share of Refs in call-free slices >= 32 nodes |
| --- | --- | --- | --- |
| `lval` scalar, non-escaping — SSA-renamable | 46.85% | 41.19% | 70.30% / 85.77% |
| `laddr` address-taken slot | 17.70% | 9.56% | 16.06% / 8.54% |
| `lagg` struct/array/VLA | 3.64% | 7.27% | 4.48% / 1.85% |
| `sym` symbol-relative — already frame-free | 31.77% | 40.87% | 9.00% / 3.39% |

66.5% of all Refs are frame-resident, but only 286 of 214889 sit above the frame pointer.
The bigger a call-free slice gets, the purer it gets: 53% of slices of >= 8 nodes and
47% of >= 32 nodes are *Ref-closed* (every Ref is `lval`/`sym`/`reg`). The blocker in the
rest is `laddr` first (2115 slices), `lagg` second (848). Slot reuse is rare and bounded:
in `src/mcc.c` only 6.90% of distinct offsets ever carry two different `VT_BTYPE`s, and
the median function has none.

`MCC_AST_FRAMEPERT=<base>[:<scale>[:<pad>]]` re-lays-out the frame at replay time: every
own-frame Ref offset below the post-prolog watermark `rir_body_loc_sv` maps to
`floor + base + (off - floor) * scale`, `ast_alloc_loc`'s recorded slots are reallocated
(with `pad` extra bytes) and bound into a per-function map, and `loc` is re-derived. Under
`-O2 -fno-replay-fallback`, over 8819 ctest cells:

| perturbation | cells failed | pass |
| --- | --- | --- |
| none | 0 | 100% |
| `-64:1` rigid 64-byte shift | 156 | 98.2% |
| `-256:1` rigid 256-byte shift | 159 | 98.2% |
| `0:2` every gap doubled | 201 | 97.7% |
| `-64:4` shifted and gaps x4 | 186 | 97.9% |

**The count is flat in the magnitude and the kind of the perturbation.** Dilating the
whole layout 4x costs 30 more cells than nudging it 64 bytes. So the arena is not a script
for one layout — it is layout-agnostic except for a fixed set of constructs that stash a
frame offset in a *second* channel the Refs do not own. 140 cells fail under all four:
111 of them assert an implementation outcome (`ast/replay-*` demands byte-faithful replay,
`optfire/*` demands a specific pass fires) and must fail under any perturbation. The 26
behavioural ones name the second channels exactly: aggregate ABI slots (`struct_byval`,
`struct_return`, `union_byval`, `return_struct_in_reg`, `aggregate_perm`, `alignas_over`),
`r2` pair slots (`complex`, `complex_annexg`, `c11_imaginary_suffix`, `float16`), VLA
bookkeeping (`vla/basic`, `vla_empty_init`), `__attribute__((cleanup))`, atomics — and the
entire JIT slice path (9 `jit/selftest-*`, `run-opt/native`, `jit-submit-aot-diff`).

That last cluster is the one that matters for GPU lowering. `ast_slice_ident_hash` already
canonicalises offsets to dense indices via `ast_sid_off`, so a slice's *identity* is
frame-free — but `ast_slice_splice` copies `ast_ival` verbatim and `ast_slice_live_ins`
hands back raw `int32_t *offs`, so a kernel can only be installed into a site whose frame
happens to match. Making Refs frame-independent is the smaller job; it is the four
side-channels and the slice ABI that need the work.

## The configuration surface moved: read this before running any recipe below

**Every `MCC_AST_*` and `MCC_RIR_*` gate is now a `-f` flag.** 113 of them, generated
from one list in `src/mccopt.h`, which also generates the driver's option table so the
two cannot drift. `-fno-<name>` turns a knob off, `-f<name>` on.

This matters for reading the rest of this file, because **a retired variable in the
environment is ignored silently** — no warning, no error, exit 0. A recipe that still
spells one will run, print a plausible board, and measure nothing. That failure mode
cost three separate defects during the conversion: the superopt search explored a
space where nothing changed and passed; `runtime-bench`'s ratchet read its win as
+0.0% instead of erroring; `optfire`'s extra-gate column stopped firing. The runnable
recipes in this file were repointed; prose that merely *names* a retired variable was
left as history.

Translation for the ones this file uses most:

| was | now |
| --- | --- |
| `MCC_RIR_NOFB=1` | `-fno-replay-fallback` |
| `MCC_AST_PROMOTE=0` | `-fno-promote-locals` |
| `MCC_AST_TEMPLATES=0` | `-fno-reemit-templates` |
| `MCC_AST_INLINE=0` | `-fno-inline` |
| `MCC_AST_SEARCH=1` | `-fopt-search` |
| `MCC_AST_NARROW_FIX=1` | `-fnarrow-fix` |
| `MCC_AST_BITFLAG=1` | `-ftree-switch-conversion` |

**Still environment variables, deliberately.** The numeric tuning knobs
(`MCC_AST_CSE_WINDOW`, `MCC_AST_TILE_SIZE`, `MCC_AST_INLINE_DEPTH`, …) and the RIR
measurement handles this file's censuses are built on — `MCC_REPLAY_IR`,
`MCC_RIR_PROD`, `MCC_RIR_FORCE`, `MCC_FORCE_REPLAY` — are unchanged, so every
`[rir-prod]` / `[rir-total]` recipe still works verbatim. So are the `MCC_JIT_*`
knobs: the embed JIT runs inside programs mcc *compiled*, where a compiler flag
cannot reach it, which is the same reason `OMP_NUM_THREADS` is an environment
variable.

**`-O` is a ladder now, not a dial.** 1–3 are the settled levels; **4–12 are one
in-development optimizer each**, cumulative; **13 and up run the strategy search with
the level as its budget in seconds**. Anything in this file that says `-O4` meaning
"search for 4s" now means "`-O3` plus one x86-only pass" — the search entry is
`-O13`. `MCC_OPT_SEARCH_LEVEL` in `mccopt.h` is the single definition, and
`ast_opt_defaults` refuses to build if a knob is ever placed at or past it.

A pass that is merely unfinished belongs on the ladder; one that is **known to
miscompile does not**. `-fjit-splice` was off-by-default, exercised by nothing, and
segfaults a threads program; putting it on rung 12 made every level above it emit
wrong code, and the suite caught it. It is back to opt-in.

**Compile-time is down to four switches** outside target/host: `MCC_DIAG`
(instruments mcc as it runs — allocator/Sym/optrace, plus the warning set and
`mcc_s`/`mcc_c`), `MCC_DEV` (differential self-checking that aborts on divergence —
the AST side-car coherence oracle, and the JIT fault injectors, which therefore
cannot be switched on in a shipped compiler), `MCC_CONFIG_TRACE` (`MCC_TRACE` alone,
~12,600 call sites), and `MCC_EMBED_JIT`. Deleted outright, now always compiled in:
`MCC_CONFIG_OPTIMIZER`, `MCC_CONFIG_DIAG_RT`, `MCC_CONFIG_LSP`, `MCC_CONFIG_ASM`,
`MCC_REPLAY_IR` (the compile-time half), `MCC_IR_CAPTURE`, `MCC_PROFILE` with
`mcc_p`. Their capabilities are reachable at runtime instead: `-O0`, `-b`/`-bt`/
`-fsanitize=bounds`, `--lsp`, `-fasm`/`-fno-asm`.

**Coverage, as wired.** `tests/optfire/flagsweep.sh` is three ctest surfaces, all under
the `flagsweep` label. `flagsweep/accept` checks every flag in the table accepts both
spellings (113/113). `flagsweep-exec/<flag>` — **113 cells, one per `MCC_OPT_ROW`** —
turns the flag on and off at `-O2` and checks twelve exec goldens still compute the
`-O0` answer. `flagsweep-cover/<row>` runs the same corpus under a **3-way covering
array**, opt-in behind `-DMCC_FLAGSWEEP_FULL=ON` and the `flagsweep-full` label.
Together they cost **4.2s wall at `-j32`** and the covering array another 6.6s.

The table has **113 rows**, not 115: `grep -c 'MCC_OPT_ROW('` counts the doc comment at
`mccopt.h:63` and the `#define` at `:181`. **34** flags were referenced nowhere in
`tests/`/`tools/`/`CMakeLists.txt` before this, counting a reference as the name on a
token boundary anywhere — `-f<name>`, `-fno-<name>`, or a bare `|`-field in the optfire
data files. Reading it strictly as "something passes this spelling" (`-f`/`-fno-` only)
the figure is **58**. The recorded 43 does not reproduce under either rule. Every one of
them owns a cell now — this is the population `-fjit-splice` came from. The first version of that harness used two synthetic programs and
*passed* `-fjit-splice`; it uses real goldens now and fails it. A sweep that misses the
bug you already have is worse than no sweep, because it reads as coverage.

**The covering array is a 3-wise guarantee, not enumeration.** Exhaustive three-deep is
C(113,3) × 2³ = 1,872,568 configurations. `tests/optfire/cover3.txt` is **74 rows** such
that every one of the 1,587,880 three-flag settings of the 107 varying flags appears in at
least one row — the identical guarantee for any bug needing three flags or fewer, and no
guarantee at all for one needing four. **107, not 113: six flags are pinned, so a bug that
needs one of them at its non-default setting is outside the guarantee by construction.** It is built by deterministic IPOG (`cover3.py gen`, no RNG, byte-identical
on any host) and **`flagsweep/cover3-verify` re-proves the property on every ctest run**
rather than trusting the generator: it re-derives the flag list from `mccopt.h`, so a new
`MCC_OPT_ROW` that nobody regenerated for fails the cell.

**Six flags are pinned out of the array**, each a debt, not a decision:
- `jit-splice` at 0 — the known miscompile. It is also the one `KNOWN_RED` entry in
  `flagsweep.sh` (`jit-splice:on:random_stuff`), so `flagsweep-exec/jit-splice` is XFAIL
  rather than red, and self-cleaning: if it starts passing the cell fails and says so.
- `replay-cmp-materialize`, `replay-landor-invert`, `storeval-call`, `replay-while-comma`
  and `replay-loopcond-store`, all at their `MCC_OPTD_ALWAYS` default of 1. These are
  bisection handles and replay-fidelity knobs, not alternative lowerings: their off-state
  is a deliberately incomplete path kept so a suspected arena defect can be confirmed.
  Sweeping them off measures the handle, not the compiler.

**`replay-fallback` is no longer pinned.** The array now varies it, which is what the
byte-gate removal needs evidence about. What that exposed, delta-debugged to minimal flag
sets over all 281 runnable `tests/exec` programs at `-O1/-O2/-O3/-Os`:

| minimal set | subjects | ships today? |
|---|---|---|
| `-fno-replay-fallback` + `inline=1, inline-functions=0` | `transparent_union`, `union_byval` | masked by the gate — **fixed** |
| `-fno-replay-fallback -fno-chain-store-live` | `chained_assign` | masked by the gate — **fixed** |
| `-fbuiltin-math-errno` | `libm_builtin_fold` | **shipped — fixed** |
| `-fno-reg-disp` + `inline=1, inline-functions=0` | `const_member_copy` | **shipped — fixed** |

All four are closed. The two masked ones were the same class as `union_cast` — real replay
defects the byte compare hid — and they were the last known blockers on flipping the
default. `flagsweep-cover` rows 17, 26, 28, 41 and 42 are **green** now; the earlier claim
that they are red described the tree before `a4d28f03`. Plain `-O0`…`-O3`/`-Os`, with and
without `-fno-replay-fallback`, are clean on all 281 subjects.

~~Before the default is flipped, one backstop is still owed.~~ **Landed in `705f0b0f`.**
`ast_func_end` computes `keep = faithful || (ast_rir_nofb_env && ast_replay_completed)`,
and `ast_replay_completed` was set after the *first* replay and never cleared by the
`posterr` arm that catches a longjmp out of the optimizer emit — so with the fallback off,
a body that aborted mid-emit was kept anyway. That is exactly how the `transparent_union`
defect shipped a body truncated at 52 of 92 bytes instead of falling back. It was
deliberately left open while that defect was live, because adding it then would have turned
the defect green by hiding it. No effect on a default build: with the fallback on, the same
arm already clears `faithful` and `keep` is `faithful` alone.

### Flipping `MCC_OPT_REPLAY_FALLBACK` off by default — the decision, not a task

**No known defect blocks it.** Everything below is measured on `main`, not argued:

- The whole suite is green under `MCC_TEST_OPT="-O2 -fno-replay-fallback"`, and under
  `-O1 … -finline` and `-O3 … -fno-inline-functions`, the two states that reach
  `inline=1, inline-functions=0` and that no single flip reaches at `-O2`.
- The per-body benignity probe (`rir-nofb-probe`, opt-in `-L census`) keeps one divergent
  body at a time and diffs the program: **zero miscompiles** at any level. The banked
  `nofb_miscompiles` list is empty for O0/O1/O2/O3. It used to hold `union_cast::main`.
- The 3-way covering array varies `replay-fallback` across 74 rows over 107 flags and
  finds nothing.

**What the gate costs while it stays on.** `ast_run_strat_seq` gates every one of the 22
strategies on `faithful`, so a byte-divergent body receives *no* optimization at all. At
`-O1` that is 2.0% of bodies but **10.2% of body bytes**, because a discarded body averages
2585 bytes against 470 for a kept one — the gate is withholding the optimizer from the
largest functions in the program. That is a capability cost, not a correctness one.

**What it still buys.** It converts a replay defect into a missed optimization rather than
a wrong program, and it is nearly free (the `orig` buffer is already copied for the restore
path). It cannot be kept *and* the bytes optimized for the same body — those are mutually
exclusive by construction.

The residual risk is not a known bug but the shape of the evidence: a 3-way array cannot
see a 4-flag interaction, and the suite is the only oracle. Recommendation is to keep
computing `faithful` permanently regardless, and — whichever way the default goes — make
the divergence **visible**, which it is not today: `rir_prod_note` only reports at
`MCC_RIR_PROD>=2`, so in a default build a fallback is silent. `union_cast` was wrong for
as long as it existed and nobody knew, because the gate suppressed the symptom. That is
worth fixing under either decision.

`inline=1, inline-functions=0` is worth naming as a state rather than a flag: `inline`
defaults off at `-O1` and on at `-O3`, `inline-functions` off at `-O1` and on at `-O2`, so
that state is reached by `-finline` at `-O1` and by `-fno-inline-functions` at `-O3` and by
**no single flip at `-O2`**. A sweep at one `-O` level calls two of those three green,
which is why `flagsweep.sh` now runs `-O1 -O2 -O3` rather than `-O2` alone.

## Branches deleted 2026-08-06 — all seven were already landed

All non-`main` branches and every agent worktree were removed. An earlier version of this
section listed seven commits as "not on `main` in any form". **That was wrong, and the
method is the lesson:** it used `git rev-list --count main..<branch>`, which counts by SHA
reachability, and a cherry-pick lands the same change under a new SHA. Every one of the
seven has a payload-identical twin already on `main`, confirmed by comparing added and
removed lines with context stripped:

| orphan | already on `main` as |
| --- | --- |
| `a4217c24` gnu89 extern-inline redefinition | `5d52753d` |
| `2bcd21d9` width-64 bitfields in packed contexts | `1b78d132` |
| `b06dcf9d` its TODO update | `5a2f8970` |
| `c3ed8b2a` VLA parameter dimension token ownership | `6cbbbc65` |
| `1e10dd1a` C23 `u8` character constants | `a170a134` |
| `22575f40` `__has_attribute` as a builtin macro | `1df8f3b8` |
| `6a6fe8f2` invalid `##` paste is an error | `97164575` |

`a3c51e8d`, the held `fix-imaginary` branch, is the one case where no single twin exists —
and it is still redundant. `main` carries all three of its hunks across `1d3f2719` (float
`iF`/`iL` order) and `38508d54` (the three early suffix sites, needed for the integer cases
`9iu`/`11il`, plus the `CONST_WANTED` rodata fold). **Applying it now would regress:** its
fold has no `CONST_WANTED` guard and no `gen_cast` on either part, so an integer imaginary
literal would push raw integer bits through a `VT_DOUBLE` `init_putv`, and its third hunk
is a workaround for a cache-aliasing bug `38508d54` deleted outright. The branch is
therefore closed, not pending.

`38508d54` did ship with **zero tests**, and no fixture in the tree used a reversed-order
imaginary suffix, so `a3c51e8d`'s test was salvaged and landed on its own. It is
load-bearing against the code as it sits on `main`: removing the early suffix sites gives
`invalid number` on `9iu`, and disabling the `CONST_WANTED` fold gives `initializer element
is not constant` on a file-scope `1.0i`.

The three commits from this session's research that were genuinely unlanded are now all in:
`ea67df7d` (byte-level RIR coverage census and ratchet), `d2e4c162` (the strategy-registry
sweep) and `934b692e` (the region-granularity research that closed F3). **Neither of the
last two was a straight cherry-pick** — re-measurement contradicted several of their own
claims, so budget that rather than a replay for anything else recovered this way. See
"Strategy-registry sweep" below for what changed.

### Open: long-double complex static initializer

`double _Complex z = 1.0iL;` at file scope dies with `internal error: static initializer
for unknown base type 7`. So do `0x1p4iL`, `1.0Li`, `float _Complex` targets, and the
underlying `double _Complex z = __builtin_complex(0.0L, 1.0L);`. The imaginary suffix is
incidental — the `CONST_WANTED` fold emits a `VT_LDOUBLE`-based rodata complex and the
static-init path has no complex-to-complex narrowing for it. `double`-to-`float` narrowing
works, so it is long-double-specific; function scope is fine at every spelling; gcc accepts
all of them.

## Invariants the code no longer states — all comments were removed 2026-08-06

Every `/* */` and `//` was stripped from `src/`, `include/`, `runtime/`, `tools/` and
`tests/` on 2026-08-06. There were **no** `TODO`/`FIXME`/`XXX` markers among them — the
only such hits were references pointing here — so no open work was lost. What was lost is
a set of prohibitions, and violating four of them caused real bugs during that day's work.
They are recorded here because nothing else records them.

- **An asm node's `sym` is not a `Sym`.** `rir_to_arena` packs `(is_output, out_reg)` into
  it, so `out_reg == -1` reads as `0xffffffff00000000`, and `ival` is an offset into the
  per-function `ir_cap_raw` pool. So an arena carrying `AST_OP_ASM`/`ASMGEN`/`ASMOPS` can
  neither be walked as if every `sym` were a `Sym` pointer, nor outlive the function that
  built it. `mccjit_intent_serialize` walks every node and interns `ast_sym`; that is the
  fourth path by which an arena escapes `ast_func_end`, beside the three retain paths, and
  missing it segfaulted the compiler under `-run` at `-O1`+.
- **`ast_loc_low` is not a usable frame floor for the inline graft.** It drifts down as
  each graft runs and never covers the graft return slot. Use `ast_graft_base`, set once
  per emit to the parser's frame bottom. Using `ast_loc_low` broke sibling reuse and put
  graft #2's parameter on graft #1's result (`exec/struct_packed_indirect`).
- **During replay `ast_alloc_loc` *assigns* `loc` from the recorded list rather than
  decrementing it**, so `loc` can come back shallower than the caller's own declared
  locals. The parser's frame bottom is `saved_loc`, not the last replayed temp.
- **A record/replay side channel must be skipped, not consumed, on a key mismatch.** There
  are four (`rir_locrec`, `rir_slotrec`, `rir_tvrec`, `rir_fcrec`), each a body-global
  array with one monotonic cursor keyed by absolute code offset. A consumer that takes an
  entry another will ask for desynchronizes every later lookup. `rir_hook_fconst_reuse`
  matched only on kind and not on value, and handed a `0.5` multiply the label belonging
  to `1.0f`.
- **A body that longjmp'd out of the emit must never be kept** — see the `posterr` item
  under the C2 gap section.
- **Syms allocated under the debug allocator belong to the slab and must not be freed.**
- **`-fno-asm` stops the `asm` keyword being recognised, as in gcc. It does not stop
  inline assembly reaching the backend by other spellings.**
- **Building an inner mcc for the JIT self-host wants a small `-DMCC_JIT_TLS_MAX`** — the
  inner compiler never hosts the slab.

## Present-tense open items — validated 2026-08-05 at `9b83dc05`

Only open, actionable work is listed. Directly-relevant "do not" caveats are kept with
their task; the broader landmine set is in [`ARCHIVED.md`](ARCHIVED.md).

### Windows / macOS host items
- **W2** — `arm64-win32`, `arm-win32`, `arm-wince` execution. Compiled and byte-compared
  only; needs Windows-on-ARM hardware (wine runs x86 PE only; qemu-user cannot load PE).
  `arm-win32` and `arm-wince` must read identically on every counter — a differential
  where they disagree is a harness bug, not a codegen one.
- **W5** — mcc cannot self-host on Windows arm64: stage1 takes `0xC0000005` on
  `lib/atomic.c`, `lib/alloca.S`, `lib/alloca-bt.S`, `lib/builtin.c` (host ABI: varargs,
  alloca, stack probe). Needs Windows-on-ARM hardware.
- **W8** — fix the `selfhost-jit` heap corruption. Root-caused (2026-08-05) to a
  heap-use-after-free of a `Sym` in the AST forward-inline re-emit path: the sym is
  retired by `ast_func_end` and then read by `ast_reemit_forward_inlines` →
  `ast_inline_graft` → `ast_replay_value` → `gaddrof`. `ast_reemit_retain` /
  `ast_inline_retain` under-count cross-function graft references, so a retained
  forward-inline body's syms get dropped. Release builds recycle via `sym_free_first`
  (hence the nondeterministic `0xC0000374`/`0xC0000005`); the MSVC-ASan `mcc_s` binary
  makes it deterministic — `mcc_s` + `tools/selfhost-jit.py` is the verification
  harness. The fix needs the full suite as a regression gate, not just the ASan oracle.
- ~~**W3 residual** — the 3-way-concurrent stress re-run.~~ **Closed 2026-08-05 on a
  macOS arm64 host, with a negative control.** 399 chains at the fix, 0 non-identical,
  over default `-O`, `-O1`, `-Os`, `-O3`, the 12-flag gates set and
  `memmodel-{O2,O3,Os}`, at concurrency 3, 6 and 10. A zero alone would prove nothing if
  the stress never reached the condition here, so it was paired with a twin built from
  `git archive HEAD` with `5aa66d88`'s `src/mccast.c` hunk reverse-applied — only the
  `ast_divmagic_invalidate` hook differing. **The defect reproduces on macOS arm64**:
  5 non-identical in 150 chains, 2 of them in 30 at exactly the Windows-matching cell,
  strictly bimodal at a constant −336, never serial. Rule of three bounds the fixed side
  at 0.75% (1 in 133); Fisher exact one-sided p = 0.0015. The claim is that the original
  failure mode at its actual rate is gone, not that the compiler is proven deterministic.
- **One-off, unexplained**: `selfhost-fixpoint-memmodel-{O3,Os}` SIGSEGV'd once in stage2
  (mcc1 compiling `src/mcc.c`) during a `macos-cross` run, mid-compile at ~2.5 s against
  a normal ~5.5 s. It survived 38 clean re-runs (4/4, then until-fail:10 over both cells,
  then a 28-minute load-reproduction at `-j8`). Two unconfirmed causes: memory/CPU
  pressure, or a mid-edit source tree — `src/mccgen.c`, which `src/mcc.c` amalgamates,
  was being written while that suite ran, and the re-runs all recompile from the tree and
  pass, which favours the second. **Durable lesson: separate build directories are not
  isolation, because `selfhost-*` and `mcctest` compile the live `src/` tree.** If it
  recurs on an idle machine with a clean tree it is real and wants a core dump.

### Closed 2026-08-05
- **Subscripting a pointer to function was accepted silently.** `f[0]` and `0[f]` are a
  constraint violation at every pedantry level; mcc emitted nothing at all, so
  `gcc.dg/pointer-arith-{1,2,3}.c` all compiled clean where gcc and clang reject. The
  check goes in the subscript path after `gen_op('+')` — which is where `f[0]` and
  `0[f]` converge — and not in `indir()`, because dereferencing a function pointer is
  legal (`(*f)()` is just `f()`) and `indir()` returning early on `VT_FUNC` is correct.
  An array *of* function pointers is unaffected: `a[0]` yields `VT_PTR`, not `VT_FUNC`.
  Regression cell `diag.dg-error.function_pointer_subscript` covers both the rejection
  and the legal neighbours. This closes 3 of the 288 missing-diagnostic files; the
  `void *` arithmetic *warnings* those same tests also expect are still not emitted.
- **`__builtin_powi` / `powif` / `powil` did not exist.** Inline definitions in
  `mccdefs.h`, since the generic `__builtin_` → libm alias path cannot express them:
  it types every argument alike and forwards to the same-named libm symbol, but powi
  takes an `int` exponent and libm has no `powi` — gcc lowers to libgcc `__powidf2`.
- **`__builtin_expect` discarded its second operand's side effects.** Both it and
  `__builtin_expect_with_probability` parsed the operand under `nocode_wanted++`, so
  `__builtin_expect(c, z++)` emitted no increment: `gcc.c-torture/execute/pr85156.c`
  returned 10 where 11 is required. Silent wrong code with no diagnostic — the second
  operand is an ordinary argument that gcc and clang both evaluate, and only the
  *probability* operand of the `_with_probability` form has to be a constant, so that
  one keeps its `nocode_wanted` guard. Found by the three-compiler board below.
  Full suite after the change: **8576 cells, the 4 documented reds, nothing new** —
  `run-tier/{x86_64,i386}-win32` (`tls`, `tls_threads`) and
  `selfhost-qemu-{arm,i386}-O2` (`MCC_MAX_ALIGN`), all four with their recorded causes.
- **`stage2 / macos / macos-arm64-clang / pe` was red in CI**, and would have been on any
  host with wine. `stage3 --consume test` runs `ctest` with **no label filter**, so the
  `wine`-labelled `run-tier/x86_64-win32` and `run-tier/i386-win32` cells run — and both
  are documented deliberate reds (`tls` and `tls_threads`, the open `-run` TLS defect).
  `tools/run-tier.sh` had no expected-failure mechanism: any bad program failed the whole
  cell, so a known-red cell could only ever be a red CI job. Adding the macOS `pe` gate
  cell is what surfaced it.

  Fixed with a `KNOWN_RED` list in `tools/run-tier.sh` — `<triple>:<program>` pairs, at
  present the four `{x86_64-win32, i386-win32} × {tls, tls_threads}`. A listed program
  that fails reports `XFAIL` and does not fail the cell; **anything unlisted still fails
  exactly as before**, which is this file's own rule that any further failure is a
  regression. The list is self-cleaning: a listed program that starts *passing* fails the
  cell with *"N KNOWN_RED program(s) now pass — drop them from KNOWN_RED"*, so it cannot
  rot into silent coverage loss. Verified both directions. The PE cells now read
  `12/14 OK under both JIT tiers, 2 known-red`.

  **Delete the four entries when the `-run` TLS defect is fixed** — the cell will tell
  you to. **Done:** the defect is fixed (next item) and `KNOWN_RED` is now empty; both
  PE cells read `14/14 programs OK under both JIT tiers`.
- **The PE `-run` TLS defect is fixed** — `{x86_64-win32, i386-win32} × {tls, tls_threads}`
  all pass under both JIT tiers. Windows has no loader step during `-run` to lay out a
  guest's implicit TLS, so the guest read whatever `gs:[0x58][_tls_index]` happened to
  hold and every `__thread` with a non-zero initializer came back 0. It now borrows mcc's
  own implicit-TLS block, mirroring the Linux slab model: `tls_setup_pe` (mccrun.c) flags
  the run and records the slab's bias inside mcc's block via `host_run_tls_slab_tpoff`
  (mcchost.c reads it off the live TEB), the x86 `TPOFF32` relocations fold every guest
  TLS offset into `mcc_jit_tls_slab`, and `tls_seed_pe` fills the slab from the guest
  template and repoints the guest's `_tls_index` at mcc's own. Threads the guest starts
  get a fresh zeroed block, so their `_beginthreadex` import is bound to a wrapper
  (`mcc_run_beginthreadex`) that reseeds the slab on entry. All Windows-only, no macros
  gating it behind the POSIX paths. Native x86_64 Linux `-run` tier still `14/14`.
- **`__has_builtin` lied about the fourteen `__builtin___*_chk` builtins.**
  `pp_has_builtin_arg` answered true only for predefined builtin tokens or `#define`d
  macros, but `runtime/include/mccdefs.h` supplies the `_chk` family as `static __inline`
  *functions*, so `__has_builtin(__builtin___sprintf_chk)` reported **0**. They are now
  in that function's `untokenized[]` list, beside the `va_*` entries already there for
  the same reason.

  This was deferred once because `__has_builtin(__builtin___*_chk)` is what gates the
  FORTIFY blocks in libc headers, so answering true could reroute every
  `str*`/`mem*`/`*printf` call through the `_chk` inlines. **Measured, that does not
  happen on Darwin**: Apple additionally gates those blocks on `_USE_FORTIFY_LEVEL > 0`,
  which mcc does not satisfy, so no call site is rewritten and the objects are
  **byte-identical** before and after on a `sprintf`+`memcpy`+`strcpy` program. Full
  suite after the change: **8498/8498, exit 0.** The correctness fix lands; the fortify
  question is untouched and stays open for a libc that gates only on `__has_builtin`.

### Flag-sweep coverage
- ~~Wire `tests/optfire/flagsweep.sh`'s exec half as ctest cells.~~ **Closed** — 113
  `flagsweep-exec/<flag>` cells, one per `MCC_OPT_ROW`, plus a 74-row 3-way covering
  array behind `-DMCC_FLAGSWEEP_FULL=ON`. See "Coverage, as wired" above.
- ~~Unpin `replay-fallback` from `tests/optfire/cover3.py`.~~ **Closed** — the array
  varies it and the five bisection-only knobs are pinned in its place. Delete the
  `jit-splice` pin and the one `KNOWN_RED` entry when that pass is fixed.
- ~~Root-cause the two masked-by-the-gate replay defects.~~ **Closed in `a4d28f03`.**
  `transparent_union`/`union_byval`: `ast_inline_graft` bound by-value parameters with a
  plain `vstore()`, whose assignment conversion does not exist for a `transparent_union`
  parameter — the parser never performs it either, since `gfunc_param_typed` consults
  `transparent_union_member` and casts to the *member* type. The graft raised
  `cannot convert 'struct A *' to 'union <anonymous>'` and longjmp'd out of the
  post-optimization emit. Fixing the cast then exposed a frame overlap underneath it,
  unreachable while the error aborted the graft first: the graft takes its frame base from
  `loc`, but during replay `ast_alloc_loc` *assigns* `loc` from the recorded list rather
  than decrementing, so `loc` can come back shallower than the caller's own locals. Hence
  `ast_graft_base`, set once per emit to the parser's frame bottom. `ast_loc_low` is not a
  usable floor — it drifts down per graft and broke `exec/struct_packed_indirect`.
  `chained_assign`: `rir_hook_fconst_reuse` matched only on the complex/scalar kind and
  never on the value, unlike its ast-side twin `ast_fconst_reuse`, so a drifted cursor
  handed the `0.5` multiply the label belonging to `1.0f`.
- **The per-flag cells cannot see a two-flag bug and the array cannot see a four-flag
  one.** `flagsweep-exec/inline` was green on the `const_member_copy` miscompile because
  that one needs `-fno-reg-disp` too; only `flagsweep-cover` found it. When a fix lands
  for a multi-flag defect, add a subject that makes *one* of its flags sufficient —
  `structs_unions/inline_sret_locrec` is that subject for the `ast_locrec` desync, and it
  is red under `flagsweep-exec/inline` at `-O1` and `flagsweep-exec/inline-functions` at
  `-O3` if the fix is reverted.
- The corpus has a hole worth closing: an injected `x + 1 → x` fold in the ident-arith
  pass was invisible to all twelve goldens when gated on a `LEVEL(4)` flag until it was
  moved from `-` to `+`. `x - 1` in a returned expression is not exercised.
- The array pins two flags, so it is a 3-wise guarantee over **111** of the 113, and a
  bug needing four specific flags is out of reach by construction. Raising strength to 4
  is ~4× the rows; it has not been measured.

### Strategy-registry sweep — `tests/optfire/stratsweep.sh`

The `-f` surface above is one axis; the *order* of `ast_strategies[]` is another, and
`MCC_AST_STRAT_ORDER` is what makes it testable from outside the compiler. The sweep asks
the two questions nothing else does: does a row hold up when it is the only row that runs,
and do two rows commute. Both green on this tree at every depth measured.

- The registry has **22** rows and all 22 are covered. An earlier cut of this sweep tested
  rows 4..21 only, on the theory that `bfold`/`ident`/`narrow`/`cprop` are always-on
  folders not worth isolating; they isolate fine — each is green alone over the whole
  admitted corpus — so "always on" was never a reason to leave a quarter of the table
  unmeasured. The triple space is therefore 22·21·20 = **9240**, not 18·17·16 = 4896.
- `stratsweep/check` is the thing that keeps the sweep honest: `STRAT_NAMES` must equal
  `ast_strategies[]` name for name and in order, and `STRAT_NONE` must still be an empty
  slot below `AST_STRAT_COUNT_MAX`. A row added to the registry and not to the sweep is a
  row the sweep silently skips while still printing PASS, so both are hard failures.
- **Admission is the load-bearing part.** A `tests/exec` program is used only if its `-O0`
  build runs, two `-O0` runs agree, *and* `-O2` with the whole registry disabled
  (`MCC_AST_STRAT_ORDER=23`) still agrees with `-O0`. Without the third condition every
  `-O2` defect outside the registry, and every program that prints a stack address, lands
  on whichever strategy happens to be under test. 282 of 300 admitted; the 18 drops are
  all at the `-O0` build step (they want `-trigraphs`, a `-std=` gate, an arm64 target, or
  a second TU). `-O2` minus the whole registry matching `-O0` on all 282 is itself a
  result.
- **Two tiers, split by corpus and by how much of the triple space is walked — not by
  which rows are looked at.** Default: all 22 rows in isolation plus six 1/1024
  permutation shards over the 31-program subject list, 29 cells, 49–145 sec\*proc
  depending on machine load, no cell over 20s. Opt-in (`-DMCC_STRATSWEEP_FULL=ON`, label `stratsweep-full`): the
  same 22 rows over the whole admitted corpus plus all 9240 triples bare and all 9240
  with rows 0..3 in front — 86 cells, 6440 sec\*proc, 7 min at `-j16`, **all green
  2026-08-06**, and the shards partition the space exactly (24 of 289 + 8 of 288 = 9240).
  A full-corpus isolation cell cannot be in the default gate —
  admission alone builds 282 programs twice, so none of them finishes under a minute.
  The shard divisor is the knob that holds the default tier down: these cells are
  registered last and are the tail of the whole `ctest` run, so the slowest one sets the
  suite's wall time. At 1/512 they were 31s each.
- **Subjects run with cwd inside the cell's own workdir.** `tests/exec/programs/stdio.c`
  writes and re-reads `fred.txt` in the current directory, so any two cells running it
  from a shared cwd race. Measured: of four full-corpus isolation runs started together
  from one directory, one dropped `programs/stdio` as nondeterministic and three admitted
  it; with per-cell cwd, four of four admit all 282. This is the likeliest explanation
  for the one unreproducible red an earlier cut of this harness produced, which landed on
  `iso-bf` for no reason connected to the bitflag pass. `tests/runner.c` already does the
  same thing — any new harness that runs `tests/exec` programs must too.
- The subject list is the flagsweep twelve **plus the strategy-named optimizer goldens**,
  and the second block is not decoration. A planted `ast_cse_kill` bug (deleting the
  read-invalidation half, so CSE reuses a cached expression across a store to one of its
  operands) is completely invisible to the twelve; it is caught by `optimizer/cse` and
  `types/int_conversion`. Same lesson as `flagsweep.sh`'s `-fjit-splice` subject, arriving
  a second time. Re-run 2026-08-06 against the shipped harness: `iso-cse` and all four
  `perm3-*` go red naming exactly those two subjects, while the other 21 isolation cells
  stay green — the signal is specific, not blanket.
- **Prefixing rows 0..3 masks bugs, so the bare mode is the sensitive one.** Under the
  planted bug both `perm3-based-*` stay green while all four `perm3-*` go red. Both modes
  are registered because they are not the same test. Keep it that way.
- Subject binaries run pinned to one CPU. `atomic_counter` is 16 threads on one cache
  line: 2.0s unpinned, 0.05s pinned, and unpinned it alone was most of a pass. Every
  atomic instruction still executes and `tests/exec` still runs these goldens unpinned;
  what is given up is true simultaneity.
- A mismatch is re-run four times before it is called a failure, and a non-recurring one
  is printed on stderr and counted in the PASS line rather than swallowed. That is for
  fork/exec losing under load, not for miscompiles — any recurrence is a hard failure.
- Not covered: one host triple (x86_64 Linux), `-O2` only, and triples — not 4-deep, and
  not the full 22-row order.

### C2 gap — remaining Replay_IR fidelity work

#### Byte-level coverage board, both RIR layers (2026-08-06, x86_64 Linux, gcc-built mcc)

Body counts overweight tiny functions: at `-O1` on `src/mcc.c` the arena falls back on
**2.0% of bodies** (52 of 2581) but **10.2% of body bytes**, because the average
fallback body is 2585 B and the average kept body is 470 B — 5.5x. Bytes are the honest
denominator and the census counts them: `tools/rir-coverage.py`, gated by
`ctest -R rir-coverage`.

**RIR is two layers and only the second has a gap.** The op-stream capture
(`rir_verify` under `MCC_REPLAY_IR=3`) is the 100% baseline; the arena
(`rir_to_arena` → `rir_prod_take` → `ast_replay_body`) is what production ships and
what loses bodies. Everything below is the arena unless it says capture.

The split that matters is semantic, not byte-exact:

| bucket | meaning |
| --- | --- |
| **kept** | modelled, replayed, byte-identical to the parser — shipped from the arena |
| **discarded** | modelled, replay ran to completion, the memcmp against the parser's second derivation disagreed (`len`/`bytes`/`rellen`/`relcontent`/`posterr`) so the parser's bytes were restored. Coverage exists; the byte gate threw it away |
| **gap** | never modelled (`skip:*`) or modelled but the replay did not complete (`abort`). The only true coverage gap |

`src/mcc.c` compiled by the built `mcc`, `-O0` forced with `MCC_FORCE_REPLAY=1`,
percentages of **body bytes** (`.text` minus prologue/epilogue):

| level | capture | arena modelled | of which kept | discarded | **gap** |
| --- | --- | --- | --- | --- | --- |
| `-O0` (forced) | **100.000%** | **100.000%** | 81.676% | 18.324% | **0.000%** (0 B, 0 bodies) |
| `-O1` | **100.000%** | **100.000%** | 95.981% | 4.019% | **0.000%** (0 B, 0 bodies) |
| `-O2` | **100.000%** | 99.965% | 95.951% | 4.014% | **0.035%** (467 B, 1 body) |
| `-O3` | **100.000%** | 99.965% | 95.951% | 4.014% | **0.035%** (467 B, 1 body) |

Prologues and epilogues are modelled by neither layer, by construction, and are now by far
the largest non-covered slice of `.text` — 5.5%, two orders of magnitude bigger than
the gap.

The wide corpus (`rir-coverage`'s, not `slice-census`'s: `src/mcc.c` + every
`tests/{exec,behavior,ast,asm,runtime,static}/**.c` + `examples`, **363 files when this was
written — STALE, the walk is 380 today**, 9 of which are negative tests that do not
compile) agrees:
modelled 100.000% / 100.000% / 99.986% / 99.986%, gap 0 B at `-O0`/`-O1` and the same
467 B at `-O2`/`-O3` — **the same body as the self corpus**, so what remains lives in
`src/`.

#### The gap, enumerated — three classes, each with a minimal reproducer

`tests/rir/gap/<class>.c` is one file per class and `ctest -R rir-gap-classes` compiles
each at all four levels and fails if a class stops reproducing. A fixture whose class is
only reachable under an optimization that does not run at every level declares that with a
`/* rir-gap-levels: O2,O3 */` comment on the first line.

| class | what RIR cannot model | reproducer | bytes in the gap |
| --- | --- | --- | --- |
| `abort` | the replay **re-runs the emitter's own invariants** and the arena does not carry enough to satisfy them. `MCC_RIR_ABORTWHY=1` prints the message `ast_error_sink` swallows. One body left, `host_runmem_alloc`: `ast-replay: storeval-arg stack underflow`, an `AST_FB_CALL_STOREVAL_ARG` invoke whose vstack rotation finds fewer live entries than it needs | `abort.c` (`-O2`,`-O3` only) | 467 B / 1 body at `-O2`,`-O3`; **0 B** at `-O0`,`-O1` |
| `skip:noops` | a body with no ops at all (an empty function) | `noops.c` | 0 B by definition |
| `skip:replayok` | the arena was built and taken, but `ast_replay_ok()` refuses it at `ast_func_end` — previously invisible, it fell out of the census entirely until `rir_prod_why_set("replayok")` was added | `replayok.c` | 0 B on both corpora |

`capbad`, `unbal`, `ovf`, `invalid`, `unsafe` and `revargs` are **never hit** on either
corpus (`revargs` needs `-freverse-funcargs`, off by default). `asm`, `regdangle`,
`bail` and `mismatch` were live classes with reproducers when this board was first
drafted; **all four have since been closed** — their reproducers now report `used` at
every level and the files are gone.

**So the arena gap is now one body, 467 B, and only from `-O2`.** Closing it means
recording how many vstack entries the store-value rotation expects, the same shape of fix
as `RIR_M_CASTT`: a missing fact at the capture site, not a divergence at the use site.

#### Do not read `discarded` as either a defect or as covered

`ast_run_strat_seq` gates **every** optimizer strategy on `faithful`
(`if (faithful && ast_strategies[si].gate())`), so a discarded body receives zero
optimization — the discarded bucket is a *capability* cost, not just bookkeeping. To
tell whether it is also a correctness cost, `tools/rir-coverage.py --nofb-probe` keeps
one divergent body at a time (`-fno-replay-fallback` plus `MCC_RIR_NOFB_SKIP` for its
siblings) and diffs the program against the shipped compiler's output. On `tests/exec`:
9 divergent bodies at `-O0` and 2 at `-O1`/`-O2`/`-O3`, **all benign, no miscompiles**.
The bank therefore holds an *empty* miscompile set per level and the cell fails on the
first new one. (`structs_unions/union_cast.c::main` was a real miscompile when this was
first measured; it no longer reproduces.)

#### The capture layer measures 100.000% on both corpora

`MCC_REPLAY_IR=3` over `src/mcc.c` and over the wide corpus: `fallback` 0, `rerror` 0,
**100.000% of body bytes faithful at all four levels**. So the claim "RIR covers all
machine code generation" is *true of the op stream* and false only of the arena.

- **The `IR_OP_RAW` escape hatch does not explain the arena's gap.** RAW is the
  capture's memcpy-the-bytes fallback and the arena has no equivalent, so it was the
  obvious suspect. Measured: **0 bodies** use it in `src/mcc.c` or anywhere in the wide
  corpus, and **0.00% of the arena's lost bytes** are in a body that used it.
- **RIR does not run at all under `-g` or on `extern inline` bodies**
  (`rir_started = (rir_env || rir_prod_env) && !debug_modes && !cur_func_inline_extern`).
  On the wide corpus that is 43 functions no layer ever sees. Reported as `unnoted` and
  never counted as covered.

#### What reconciles

The harness checks its own arithmetic: `.text == Σ per-function bytes + Σ re-emitted
bytes + residual`, and `Σ body bytes == used + fallback + skip`. Both hold exactly
(residual **0**, delta **0**) at all four levels on the self corpus, and the census's
own invariant `Σ[rir-prod-unfaithful] == fallback` holds on every row.

On the wide corpus the residual is **−119 bytes over 3.5 MB (0.003%)** in both layers,
and every byte of it is named, per file: `+1/+6/+14/+14` in `asm_outside_function.c`,
`al_ax_extend.c` and `asm_c_connect/part{1,2}.c` — file-scope `asm` emits into `.text`
outside any function, so no body owns those bytes; `−78/−38/−38` in `inline.c` and
`c99inline_{a,b}.c` — `extern inline` bodies are generated (and counted by the
per-function hook) and then not placed in `.text` at all. Reported, not hidden.

**It only reconciles because `-O3` re-emission is accounted for, and that uncovered a
real defect.** At `-O3` — and only at `-O3`, because `MCC_OPT_INLINE` is
`optimize >= 3` — `ast_reemit_forward_inlines()` re-emits 27 functions / 52,022 B at the
end of `src/mcc.c` so they can inline a callee defined later. The *first* emission is
left in `.text` with no symbol pointing at it. Not a correctness bug — nothing branches
there — but `-O3` ships ~3.6% more `.text` than it needs to, and any per-body byte
accounting that does not know about re-emission silently attributes the pre-inline copy.

#### The ratchet

**Re-banked 2026-08-06 for the width-ladder oracle.** `src/ast_eval_slice.h` and the
census block in `src/mccast.c` add ~700 lines of new compiler source, so every `lowerable`
percentage is measured over a different denominator. `bodies_pct` fell 9.1328% -> 9.0189%
(`-O0`) because the added bodies are `fprintf`/`snprintf` reporting code, which `call` and
`global` block; in the same move `nodes_pct` rose 41.6543% -> 41.8098% and
`region_nodes_pct` 16.7579% -> 16.8908%, because the ladder's arithmetic *is* lowerable at
region granularity. Re-banked with `tools/rir-coverage.py <bdir> --update-bank-low`. No
lowering capability regressed; only the mix of the compiler's own source changed.


`tests/rir/coverage-bank.json` banks, per level and per layer, the **modelled**
percentage — kept + discarded, i.e. everything that is *not* the gap. `ctest -R
rir-coverage` fails if it regresses by more than 0.05 pp, or if the *magnitude* of the
byte-accounting residual grows — a residual that moves toward 0 is better accounting
and must not fail the gate. **Deliberately not banked: byte faithfulness.** A ratchet on `kept`
would lock in the wrong invariant and fight every future improvement that makes the
optimizer *change* bytes. Banked at HEAD on the self corpus: modelled **99.59%** at
`-O0`/`-O1` and **99.56%** at `-O2`/`-O3`, capture **100.00%** at every level,
residual **0**.

`ctest -R rir-gap-classes` is the other half: it fails when a gap class stops
reproducing, so closing one is a deliberate act with a bank update rather than a silent
no-op. Four classes were closed that way between the first draft of this board and
this one.

**Bank classes and percentages, never totals.** The body *total* of a census taken over
a whole `ctest` run is not stable at fixed HEAD (two consecutive runs gave `used=95823`
and `used=95971` with every class count byte-identical), so nothing derived from a
varying denominator is banked. The fixed-file-list censuses used here *are*
deterministic — two consecutive wide runs were byte-identical.

Default cost is `rir-coverage` 4.8 s + `rir-gap-classes` 0.05 s on an idle box. The
wide-corpus census and the per-body benignity probe are opt-in:
`MCC_RIR_CENSUS=1 ctest -L census` (`rir-coverage-census` 17 s, `rir-nofb-probe` 3.6 s);
they skip with 77 otherwise so the default suite stays fast.

- Close the open per-body byte divergences (the "largest first" list in the archive).
  **Fix at the USE site, never the CAPTURE site.**
- `full_language.c` still diverges at `-O0` on x86_64/i386 — an `AST_OP_ASM` replay
  defect (P4 defect 4), contained not closed.
- **Do not turn `-fno-replay-fallback` (`MCC_RIR_NOFB`) on by default until the
  byte-faithful step is green.** `keep = faithful || (nofb && replay_completed)` is
  load-bearing — ≥4 of the fallback bodies are genuinely wrong, not benign.
- `-fno-replay-fallback` + selfhost-jit-with-mcc blows RSS ~6.7×; bisect via
  `MCC_RIR_NOFB_SKIP`. Measure on the full suite, not the 317 exec goldens.
- **`exec/union_cast` is closed** — it was the sole consistent native failure under
  `-fno-replay-fallback` at every `-O` level, and the whole remaining correctness
  justification for the byte gate. `rir_stmt` appended a statement straight into the
  basic block whenever `rir_hold_inline` declined to hold it, even with an inline-held
  `Invoke` still queued from earlier in the op stream, so the held call was re-emitted
  behind it by the next `rir_ihold_bind`. `take((union U) 13)` is the two-line
  reproducer: `gen_union_cast` lays the temp down as `memset(tmp,0,n)` then
  `tmp.i = 13`, the `memset` is held because `printf`'s callee is already on the
  shadow stack, and the store lands at a shallower shadow depth than the hold, so the
  arena read `tmp.i = 13; memset(tmp,0,n)` and the callee saw 0. Whole suite under
  `MCC_TEST_OPT="-O2 -fno-replay-fallback"`: zero replay failures. The body is now
  byte-faithful too, so it stops falling back and reaches the 22 optimizer passes.
- `run-tier/x86_64` fails `tls_threads` whenever `MCC_JIT=1` meets an active AST
  replay (`MCC_FORCE_REPLAY=1` at `-O0`, or plain `-O1`/`-O2`); `MCC_JIT=0` is green
  at every level and `MCC_RIR_PROD=0` does not help, so this is **not** an arena
  defect and not a `-fno-replay-fallback` one. The child thread reads every
  non-zero-initialised `__thread` as 0: the `-run` TLS slab is a `__thread` array
  inside mcc (`mcc_jit_tls_slab`, src/mcchost.c:1450) that glibc zeroes per thread,
  and the re-seeding wrapper is installed by binding the program's `pthread_create`
  to `mcc_run_pthread_create` (src/objfmt/mccelf.c:974) under `s1->run_tls_active`,
  which `tls_setup_linux` (src/mccrun.c:451) only sets on the interpreter relocate
  path. `--no-jit` does not suppress it — `MCC_JIT=1` still wins.

#### The refusal census bottoms out at 359, and 338 of those are `{ }` (2026-08-06)

`MCC_RIR_PROD=2` + `MCC_RIR_PROD_OUT` over a full `ctest -j32` at ambient
`MCC_TEST_OPT`, `\r` stripped. Before this day's work the classes were
`noops`=338, `mismatch`=42, `revargs`=21. After: `noops`=338, `revargs`=21,
**`mismatch`=0**. Gate: `ctest -j32` green and the whole suite green under
`MCC_TEST_OPT="-O2 -fno-replay-fallback"`.

- **`mismatch` was two sites, both only reachable from the self-hosting cells.**
  A previous instrumentation pass over `src/` + `tests/` at `-O1`/`-O2`/`-O3`
  found nothing because the trigger is `src/mcc.c` compiled *amalgamated* by a
  built mcc, which is what `tools/selfhost-smoke.py` does and no other cell does.
  Reproduce by hand: take the `-D`/`-I` flags out of
  `cmake-release/compile_commands.json` for `mcc.c` and run
  `mcc <flags> -O2 -c src/mcc.c`. Both sites are fixed; see the two commits.
  One was a fixed `AstLocal args[32]` in the arena's `IR_OP_CALL` (mcc's own
  `rir_report` calls `fprintf` with 33 arguments); the other was region marks
  recorded under `DATA_ONLY_WANTED` — a function-scope
  `static const … off[] = { offsetof(…), … }` — surviving into the arena when
  the ops from the same window are already dropped by `rir_op_effect`.
- **All 338 `noops` are provably empty.** Instrumented `rir_prod_take`'s `!nops`
  arm to log `ind - rir_body_ind_sv`, `rir_n` and `ir_cap_n` over a full suite:
  338 rows, every one `len=0 rirn=0 capn=0`. 37 distinct names, all `{ }` bodies
  (`void_foo` in `exec/features_c99_c11/generic.c`, `func_ull_ull` in
  `exec/bounds/stack_safe.c`, the whole `inline`/`inline2` linkage matrix, …).
  There is no arena to build and no bytes to lose, so counting them in the
  coverage gap overstates it by 94% of the gap. `rir_prod_report` now also prints
  a **derived** `[rir-prod-cov] nonempty= modelled= refused= empty=` line;
  `used`/`fallback`/`skip` and every `[rir-prod-why]` count are untouched, so
  every board above stays comparable. Today's suite reads
  `nonempty=98727 modelled=98706 refused=21 empty=338` against
  `used=95971 fallback=2735 skip=359`. Note the body **total** moves a little
  between runs at fixed HEAD — two consecutive censuses gave `used`=95823 and
  95971 — while `fallback`, `skip` and every `[rir-prod-why]` count were
  identical. Compare the class counts across runs, not the totals.
- **`revargs` (21) is one body, and it is not worth closing.** Measured by
  lifting the refusal behind an env probe: under `-O2 -fno-replay-fallback` the
  *only* thing the whole suite notices is `exec/errors_and_warnings` printing
  `122333` where the golden says `333221` — and the ` 1 2 3` beside it is already
  right, so the Invoke's children, types and values all survive; the gap is
  purely evaluation order. The bit is cheap (`fbits` is a `uint64_t`, bit 22 is
  the highest one taken, and the arena is never serialized — there is **no**
  schema to revise, contrary to the older note). The emitter is not: `AST_Invoke`
  already carries the sret pre-alloc, the storeval-arg rotation, the
  indirect-call type fixup and the noreturn `CODE_OFF`, and a reverse child order
  multiplies the states each of those must be right in, while every pass that
  reads an Invoke's children as evaluation order (inline grafting, the call
  window, the argument analyses) becomes conditionally correct. Against that: one
  distinct body in the entire corpus (the 21 rows are that one `main` recompiled
  by 21 cells), a refusal that is safe by construction, a flag that is off by
  default, and a single golden line as the only coverage the new path would get.
  If it is ever picked up, the cheap half is narrowing the refusal from
  whole-translation-unit to bodies that actually contain a call — it does not
  help this corpus, where the one body does contain calls.

#### The fallback census was unreadable until 2026-08-06 — six defects

All six are fixed; every fallback board taken before this date is suspect, and the
`[rir-prod-why]` column of one is worthless by construction. The board below is the
first that **reconciles** — `sum([rir-prod-unfaithful]) == fallback` at all four levels.
Keep that identity as the check on any future census: it was 48-vs-49 at `-O2` before
the last two fixes, and that one missing body turned out to be a real defect class.

- **`[rir-prod-why]` could never print a line.** `rir_prod_why_n[]` was incremented
  only on the `fallback` verdict, but `rir_prod_take()` sets `rir_prod_why = ""` on
  entry and every path that reaches a `used`/`fallback` verdict returned non-NULL — so
  the histogram was fed exclusively with the empty string and `rir_prod_report` printed
  nothing, every run. The codes it names (`asm`, `regdangle`, `mismatch`, …) only ever
  live on the `nomodel` → `skip` path, which was not counted. This is the mechanical
  reason behind the archive's "`rir_prod_why` is empty in every fallback observed" —
  that was an artefact of where the counter sat, not a fact about the bodies. Now the
  skip path feeds the why-histogram and fallbacks feed a new
  `[rir-prod-unfaithful]` one.
- **`rir_unfaithful_why` was never reset per body.** It is assigned in exactly one
  place, at the `faithful` compare in `ast_func_end`. A body whose replay longjmp'd out
  before reaching that compare inherited the *previous* body's reason, so an aborted
  replay was silently attributed to `len` or `bytes`.
- **A global could not carry the reason at all — `ast_func_end` nests.** Resetting the
  global per body is not enough: nested bodies run their own compare and overwrite it,
  and at `-O2`/`-O3` the JIT-dispatch path runs *between* the outer body's compare and
  its census note. The reason is now a per-body `volatile` local (`ast_unf_why`) that is
  published to the global only in the statement before `rir_prod_note`, so nothing
  between the two can clobber it. It is `volatile` for the same reason `faithful` is —
  it is written inside a `setjmp` region.
- **A byte-faithful replay could still fall back, with no bucket for it.** The last
  unreconciled body at `-O2`/`-O3` (`host_runmem_alloc`) completes its replay *and
  passes the byte compare*, then a later stage inside the same protected region raises
  an error; the `else` arm clears `faithful` and the body is restored. `abort` is the
  wrong name for it — the replay was fine, the **post-replay** work failed — so it now
  has its own bucket, `posterr`. Distinguishing the two matters: `abort` is a
  replay-fidelity defect, `posterr` is not, and lumping them hides both.
- **`MCC_RIR_PROD_OUT` dropped the reason column.** The file sink wrote four fields
  ending in `rir_prod_why`; only the stderr sink wrote `rir_unfaithful_why`. Since
  `rir_prod_why` is always empty for a fallback (above), every fallback in a
  `MCC_RIR_PROD_OUT` census carried *no* attribution at all — and that file is the only
  sink that survives a full-suite run. Both sinks now emit the same five columns.
- **The stderr line glued the two reasons together** (`"%s%s"`), so a non-empty pair
  printed as `bailbytes`. They are separate tab-delimited columns now.

#### Full-suite board with the stage-2 compiler (2026-08-06)

`ctest -j32` in a `MCC_TOOLCHAIN_PROFILE=mcc` build dir, ambient `MCC_TEST_OPT` set per
level. 8577 cells, 403 skipped, ~225 s per level. Identical at all four levels:
**8172 pass, 2 fail** — `selfhost-qemu-arm-O2` and `selfhost-qemu-i386-O2`, the
`-fif-conversion` defect recorded under "Open codegen / front-end defects". Nothing else
is red, and no cell is level-dependent.

- **`run-tier/{x86_64,i386}-win32` were flaky under load, not level-dependent. Fixed.**
  They came up red in the `-O2` sweep and green at the other three; re-run in isolation
  they passed 6/6 at both `-O2` and `-O3`. It was never codegen and never wine emulation:
  the corpus always passed and the cell died in its own teardown. A red run says

      [i386-win32] -run tier: 15/15 programs OK under both JIT tiers
      rm: cannot remove '/tmp/tmp.W5WwhnLAbB/.wineprefix': Directory not empty

  The wineserver outlives its last client and flushes `user.reg` and `userdef.reg` back
  into `WINEPREFIX` as it exits. `trap 'rm -rf "$work"' EXIT` fired the instant the
  corpus loop ended, emptied `.wineprefix`, the departing server recreated a registry
  file inside it, and the closing `rmdir` failed with `ENOTEMPTY`. That `rm` was the last
  command the shell ran, so under `set -e` its status became the cell's status. Load
  stretches the server's shutdown, which is why only a full parallel suite tripped it.
  `tools/run-tier.sh` now shuts the prefix's own wineserver down and waits for it before
  removing anything, and routes teardown through a `cleanup()` that restores the body's
  status and cannot fail. Measured with both cells run concurrently under a 30-way CPU
  burn, 20 iterations each: 7 of 40 red before, 0 of 40 after, and 0 red across seven
  consecutive full `ctest` sweeps. Every abandoned `/tmp/tmp.*/.wineprefix` on a host is
  one lost race, so `ls -d /tmp/tmp.*/.wineprefix` dates the recurrences directly.

- **A shared build dir is not a place to measure a flake.** Several concurrent
  `ctest` runs in one binary dir overwrite each other's `Testing/Temporary/LastTest.log`
  and `LastTestsFailed.log`, so a cell can appear in `LastTestsFailed.log` for a run that
  exited 0 — the entry belongs to somebody else's run. Read a cell's own stdout, not the
  summary files, and give each concurrent sweep its own build dir. The `-run` PE cells
  are unaffected by the sibling hazard in `work=$(mktemp -d "$bdir/run-tier.XXXXXX")`:
  that branch is taken only when `DOCKERPLAT` is set, which the `win32` triples never do.

#### A stage-2 build dir does not rebuild when a header changes

`MCC_TOOLCHAIN_PROFILE=mcc` build dirs do not track the amalgamated headers: after
editing `src/mccast.c` and `src/mccrir.c`, `cmake --build cmake-selfhost` reported
`ninja: no work to do` while `CMakeFiles/mcc.dir/src/mcc.c.o` was eight minutes older
than both sources. Under `MCC_SINGLE_SOURCE` only `src/mcc.c` is a ninja input and every
other `.c` arrives through it, so the header dependency has to come from a compiler
depfile — which the gcc-built dir gets and the mcc-built dir does not. The stale binary
is silent and plausible: it runs, it self-hosts, it passes. **Any measurement taken from
a stage-2 dir without first forcing the object out is suspect.** Workaround is
`rm cmake-<dir>/CMakeFiles/mcc.dir/src/mcc.c.o` before building; the fix is to make mcc
emit a depfile CMake's `CMAKE_DEPFILE_FLAGS_C` can consume for this profile.

#### Self-host fallback board at HEAD (2026-08-06, x86_64 Linux, mcc-built mcc)

`mcc.c` compiled by the stage-2 compiler; `-O0` needs `MCC_FORCE_REPLAY=1` because
`ast_replay_env` is `optimize >= 1 || embed_jit || forced`, so the census is otherwise
empty there. Bodies: 2538.

| level | used | fallback | skip | `len` | `bytes` | `abort` | `posterr` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `-O0` (forced) | 2426 | 120 | 12 | 97 | 18 | 5 | 0 |
| `-O1` | 2500 | 48 | 12 | 23 | 20 | 5 | 0 |
| `-O2` | 2499 | 49 | 12 | 23 | 20 | 5 | 1 |
| `-O3` | 2499 | 49 | 12 | 23 | 20 | 5 | 1 |

The `abort` column here and in the whole-suite board below is stale: `RIR_M_CASTT` took
this one to 0 / 0 / 1 / 1 later the same day. The rest of both boards still holds and the
reasoning below them is unaffected.

- **The `-O0` fallback set is a strict superset of the `-O1` set**, and the extra is
  one single failure mode. 120 ⊃ 48; exactly 0 bodies fall back at `-O1` that do not
  also fall back at forced `-O0`; and all **72** of the `-O0`-only bodies are `len` —
  not "mostly", all of them. So the `-O1` ladder does not introduce replay divergence,
  it *removes* 72 length divergences that the `-O0` emit path has on its own. That is
  the opposite of the intuition the census was built on, and it relocates the defect:
  the residual divergence is not the optimizer, it is the replay's length accounting on
  the unoptimized emit path, which some `-O1`-gated pass happens to paper over.
- **Next step on the census, and it is a cheap one:** find which `-O1` gate erases the
  72. Re-run the forced-`-O0` census adding one `MCC_OPTD_LEVEL(1)` row at a time
  (`MCC_FORCE_REPLAY=1 MCC_RIR_FORCE=1 … -f<name>`) and watch `len` fall from 97. Since
  the 72 are a clean set difference with a single reason code, whichever gate collapses
  them is pointing straight at the length mismatch's cause — and unlike the `bytes`
  bodies, this one does not need a per-body byte diff to make progress on.
- `-O2` and `-O3` differ from `-O1` by exactly one body, `host_runmem_alloc`, and are
  identical to each other — and that one body is the `posterr` above, i.e. **not** a
  replay divergence. On the fidelity axis the ladder above `-O1` is completely flat: the
  same 48 bodies, in the same three buckets, at all three levels. Any future claim that
  an `-O2`/`-O3` pass improved or regressed replay fidelity has to beat that flatness
  first.
- The 12 `skip`s are constant at every level: `regdangle`=8, `asm`=2, `mismatch`=2.

#### Whole-suite fallback board (same day, same compiler, all 8577 ctest cells)

`MCC_RIR_PROD=2` + `MCC_RIR_PROD_OUT` over a full `ctest -j32`, ambient `MCC_TEST_OPT`
per level, `MCC_FORCE_REPLAY=1` on the `-O0` row. Every row reconciles.

| level | bodies | used | fallback | rate | skip | `len` | `bytes` | `relcontent` | `abort` | `posterr` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `-O0` (forced) | 254430 | 242724 | 5909 | 2.32% | 5797 | 4020 | 1614 | 15 | 235 | 25 |
| `-O1` | 223195 | 213371 | 4586 | 2.05% | 5238 | 2897 | 1447 | 15 | 202 | 25 |
| `-O2` | 223077 | 213301 | 4538 | 2.03% | 5238 | 2898 | 1398 | 15 | 202 | 25 |
| `-O3` | 222733 | 213035 | 4460 | 2.00% | 5238 | 2820 | 1398 | 15 | 202 | 25 |

- The self-compile board's shape holds at suite scale: `len` is the **majority bucket at
  every level** (68% of `-O0` fallbacks, 63% at `-O1`+), and forced `-O0` carries ~1100
  more `len` bodies than any optimized level. Whatever the length defect is, it is one
  population, it is the biggest one, and it is worst on the unoptimized emit path.
- The skip census is *byte-identical* across `-O1`/`-O2`/`-O3` (5238, and every
  sub-code equal). `relcontent` (15) and `posterr` (25) are constant on all four rows.
  The ladder moves `len`/`bytes` only, and only slightly.
- `-O0` skips 5797 vs 5238 — the extra is mostly `noops` (1729 vs 1470) and `asm`
  (908 vs 721), i.e. bodies the pre-flight declines to model at all.
- Reading the raw `MCC_RIR_PROD_OUT` file: the wine `mcc.exe` cells write CRLF, so 16
  `-O0` fallback rows arrive as `len\r`. Strip `\r` before tallying or the biggest
  bucket splits in two. The file is otherwise intact under 32-way concurrent append —
  0 malformed rows in 254430.
- `keep = faithful || (nofb && replay_completed)` reduces to `keep = faithful` in every
  default build: `MCC_OPT_REPLAY_FALLBACK` is `MCC_OPTD_ALWAYS` in `mccopt.h`, so
  `ast_rir_nofb_env` is always 0 unless `-fno-replay-fallback` is passed. The comment
  above that line in `mccast.c` claims the no-fallback path is the default — it is not,
  and the TODO entry above (do not turn it on) is the accurate one.
- ~~Re-bank `o0-baseline` at HEAD on an x86_64 Linux host.~~ **DONE 2026-08-09,
  `wt/gateall`**, on exactly such a host, all twelve keys, and registered as a cell so the
  next drift surfaces on the commit that causes it. Everything below still describes the
  constraint correctly and is kept because the *next* re-bank faces it too. **Not a macOS
  item — a Mac
  cannot do this, by construction.** The five ELF glibc keys need the Gentoo stage3
  sysroots under `vendor/`, and the `<arch>-fetch` cells that download them are gated on
  `if(NOT _QEMU_${arch}) continue()` — i.e. on a qemu-user binary being present, which is
  Linux-only. So on a Mac those cells never register and the sysroots cannot be fetched
  through the supported path at all. The narrower true constraint is *a host with the
  sysroots*, not x86_64 specifically: cross output is host-independent (proven below),
  so an arm64 Linux host with the sysroots would bank the cross keys identically. What
  genuinely wants x86_64 Linux is the `x86_64` key itself. (Verified 2026-08-05 that the
  Darwin side *reproduces*: all five PE keys are byte-identical to a Linux run at the
  same HEAD, and the two `*-osx` keys are measurable only on Darwin. Also fixed there:
  `o0_ab.sh`'s `x86_64` key used `$BUILD/mcc`, the build's own compiler, so on a
  non-x86_64-Linux host it measured the host's native target under the `x86_64` label and
  reported a plausible board of 276 objects for a key it never touched; it now checks the
  `mcc -v` target string and falls back to `mcc-x86_64` plus the usual sysroot
  requirement.)
- ~~**`C2_FORCE=1` is dead, and was failing silently until 2026-08-05.**~~ **REPOINTED
  2026-08-09 (`wt/envgate`), filed item 17.** Both scripts now derive the set from
  `src/mccopt.h`. The restoration recipe below was followed only in part, on purpose: the
  set taken is **every** `MCC_OPTD_LEVEL(n)` row (54), not the `LEVEL(1)` rows plus
  per-target `MCC_OPT_SPECIAL`s — the `LEVEL(1)`-only reading drops thirteen members of the
  historical set that the `-O` ladder moved out to levels 10–12, and the `SPECIAL` rows are
  exactly the per-target hazard this bullet warns about, so a target-independent set cannot
  fabricate the `reg-disp`-on-arm64 divergence it describes. Everything below stands as the
  record of why. Both scripts' original text follows:
- **`C2_FORCE=1` is dead, and was failing silently until 2026-08-05.** Both
  `tools/c2_sweep.sh` and `tools/c2_equiv.sh` derive the forced-`-O0` gate list by regex
  for `ast_env_gate("MCC_AST_…", o4 || s1->optimize >= 1)`; `ast_env_gate` no longer
  exists anywhere in `src/` after the `-f` conversion, so the regex matches zero. The
  authors' guard — refuse rather than "measure `-O0` with every pass off and call it
  parity" — **could never fire**: `ngate=$(… | grep -c .)` exits 1 on zero and `set -e`
  killed the script at that assignment, before the diagnostic printed (`exit 1`, zero
  bytes of stdout *and* stderr). `|| true` on both counts is fixed; **the derivation is
  still dead**, so it now fails loudly. Any forced-`-O0` row taken today is worthless.
  To restore it: scrape every `MCC_OPTD_LEVEL(1)` row of `src/mccopt.h` (21) and add the
  `MCC_OPT_SPECIAL` rows whose expression is `o4 || optimize >= 1` — **those are
  per-target**. `reg-disp` is `optimize >= 1` only under `MCC_TARGET_X86_64`; forcing it
  on arm64 moves `const_member_copy.c::main` by one `len` unit and fabricates a
  divergence. Correct sets: arm64 = 21 + `ivopts` + `builtin-minmax` + `builtin-fma`;
  x86_64 = 21 + `ivopts` + `reg-disp`. Pass as `-f<name>` on the compile line, not as
  env, with `MCC_FORCE_REPLAY=1 MCC_RIR_FORCE=1`. `MCC_AST_INT128=1` no longer exists
  either. Reproduce single files under `sh`, not `zsh` — zsh does not word-split
  unquoted `$FLAGS`, so mcc silently ignores it and you measure plain `-O0`.
- **Settled 2026-08-05, both were open questions in the archive.** (i) `arm64-osx`'s
  forced-`-O0` board *is* identical to its `-O1` board, per file as well as on all four
  sub-counters — but the control no longer reproduces: `x86_64-osx` is now also
  identical, so the equality is a property of the **tree**, not of the key, which is the
  opposite of the inference drawn from it. Likely `a55c0a07`, which moved six
  replay-fidelity knobs to always-on and removed the `-O0`-only gap. (ii) The
  `fuzz/runner.c` live-on-Linux / fallback-on-Mach-O split was **code drift, not a
  per-key pre-flight difference**, bisected to `c5db86ae` (`AST_FB_LOAD_LVAL`); all three
  bodies are live on every measurable key at HEAD. The axis was also misread — at
  `b89723f9` the fallback set is arm64/arm/i386 against x86_64, on **both sides** of the
  ELF/PE/Mach-O line. And the "pre-flight" framing had a wrong premise: `rir_prod_why` is
  empty in every fallback observed, so nothing declined; the fallback is the post-replay
  byte compare.
- `ptr_unlink` for-condition-store segfault: root-caused, fix open (the `rir_cf_cond` /
  `rir_docond` machinery; a 5-fix / 34-break discriminator).

#### Per-region keep/restore — measured, and refused (2026-08-06)

Keep/restore in `ast_func_end` is whole-body: one divergent byte discards the entire
replay. At ~50 regions per body the obvious idea is to make the unit a region instead.
It was measured at suite scale and it does not work. Two independent full `ctest` runs
(98930 bodies, 2669 fallbacks) agree to the row. **Do not build this**, and do not
re-open it without first reading the last bullet.

- **The regions are not a cover of the function body.** `RIR_R_BODY` is a *loop* body
  (`rir_hook_do_body_begin` / `for_body_begin` / `while`), not the function's. Nothing
  wraps a whole function, so straight-line top-level statement code sits inside zero
  regions. Measured consequence: for **1340 of 2566** fallbacks with a byte span (52.2%)
  there is no enclosing region at all, so there is no region to swap. Deduped to the 147
  distinct fallback signatures it is still 35.4%.
- **The byte disagreement is positionally local but structurally not containable.**
  Median divergent span is 6.7% of the body and touches 4 of ~36 regions; but the median
  *smallest enclosing region* is 100% of the body. Split by reason, the 584 `bytes`
  (same-length) fallbacks are genuinely tight — median span 2.1% of the body, 84.6%
  within 10% — while the 1964 `len` fallbacks are not, because a length change at offset
  X invalidates every PC-relative displacement crossing X. `len` is 76.5% of the
  population, which is the same thing the boards above say from the other direction.
- **Boundary state, over 129861 regions of one self-compile.** Only **4.0%** of regions
  (5256) have an empty value stack at *both* boundaries, no open return chain, and no
  break/continue/goto/label crossing them. 53.4% have a live hard register at a
  boundary, 29.2% carry a `VT_CMP`/`VT_JMP`/`VT_JMPI` (pending flags, or another jump
  chain embedded in the value stack), 48.0% have `rsym != 0`. The clean ones are exactly
  the statement-shaped kinds — `then` 32%, `else` 38%, `incr` 49%, `body` 29%, `for`
  24%, `do` 76%, `synth` 2% — and zero of `if`/`while`/`switch`/`ternary`/`landor`/
  `call`/`cond`/`inc`/`member`/`tarm`/`lsup`/`lopnd`/`vstore`/`cvt` qualify.
  Cross-referenced against where divergences land, only 22.9% of fallbacks are even
  enclosed by a *kind* that can qualify, before the ~30% per-instance filter.
- **What is region-local: the value stack, and only at statement boundaries.
  Everything else in the save/restore set is body-global.** `rsym` is the head of a
  linked list threaded through the text itself — `gjmp` stores the predecessor's text
  offset in the jump's own disp32 and `gsym_addr` walks it — and it is resolved by
  `gsym(rsym)` in `gen_function` *after* `ast_func_end` returns, so at the decision
  point the chain is live and physically interleaved with the code of every region.
  Break/continue are the same construction (`*cur_scope->bsym = gjmp(*cur_scope->bsym)`),
  as is the alloca chain in `mcc_state->cg_func_alloca`. `loc`, `anon_sym`,
  `nb_temp_local_vars` and `arr_temp_local_vars` are monotonic per-body accumulators —
  `get_temp_local_var` reuses slots by scanning the live value stack, so the table at a
  region's entry is a function of every preceding region. The relocation section is an
  append-only cursor.
- **The decisive obstacle: the replay is not a pure function of the arena.** It consumes
  four record/replay side channels — `rir_locrec`, `rir_slotrec`, `rir_tvrec`,
  `rir_fcrec` — each a single body-global array with one monotonic cursor advanced by
  comparing recorded positions against the *current* `ind`. Replaying one region in
  isolation would have to seek four cursors, and the keying is by absolute code offset,
  which any splice changes. This is not a per-region-kind soundness question; it is that
  **`ast_replay_body` has no entry point below the body at all.** Anything that revisits
  this has to make those four cursors seekable first, and that is the whole cost before
  any of the coverage arithmetic above starts to matter.
- `ast_baseline_splice` is what the splice primitive would have to look like, and shows
  the cost from the other end: memcpy, re-issue every relocation rebased off
  `src_base`, then walk the saved chain head and re-link every node into the current
  `rsym`. It handles exactly one chain and is only ever used for a whole body emitted at
  a fresh `ind`, with all other chains already resolved.
- **Arithmetic of the verdict.** Sound for at most 7 of 21 region kinds
  (`then`/`else`/`incr`/`body`/`for`/`do`/`synth`), covering 4.0% of regions, reaching
  maybe 7% of divergences, and only for the 23.5% of same-length fallbacks that are
  region-enclosed at all. The salvage argument is also weaker than it looks: the oracle
  that matters is the goldens, not the memcmp, and a divergent region is usually correct
  code that merely encodes differently. **If the byte compare is ever replaced by the
  goldens as the gate, this direction loses its remaining motivation entirely** — there
  is nothing left to restore.
- What survives in the tree: `rir_prod_span` appends `first`/`end`/`blen`/`nlen` to the
  `fallback` rows of an `MCC_RIR_PROD_OUT` census — where in the body the replay first
  disagrees and how far the disagreement runs. That is kept because the standing "close
  the open per-body byte divergences, fix at the USE site" item needs it, not because of
  this question. The region-pairing columns (`rall`/`rhit`/`rmin`/`rminkind`/`rdepth`)
  and the `MCC_RIR_BND=1` per-kind boundary table produced the numbers above and are
  **not** in the tree; they answered a closed question. Recover them from `934b692e` if
  the numbers ever need retaking.

### RIR cut — remaining cleanup
- Deletion residue, keep-or-delete each: `ast_verify_diff` (+ `_match` / `_dump_diff`),
  `ast_treechk` / `MCC_AST_TREECHK`, `ast_jit_guard_env` (declared, never assigned),
  `ast_rir_arena` (dead local), `tools/tracediff.sh`, the recorder half of
  `gate-ledger.sh`.
- **P6** — split the monolithic `src/mccast.c` (~17k lines) and rename `ast_*` → `ir_*`.
  Precondition still false: the `ir_` namespace already collides (~850 occurrences) and
  `targetgate` still whitelists `mccast.c`.
- ~~Land the held `fix-imaginary` branch.~~ **Closed** — redundant with `38508d54`; see the branch section above.

### Open codegen / front-end defects
- `__bf16`: finish encode/decode + ABI now that `VT_BTYPE` is 5 bits. **Do not alias
  `__bf16` onto `_Float16`** — distinct `c.i` storage, `is_float_abi`, libgcc name.
- 32-byte vectors are laid at 16-byte alignment (`MCC_MAX_ALIGN` cap) — open ABI
  decision; cross-TU to gcc is currently incompatible (struct-ABI, not SysV vector).
- `aligned(N)` bitfields: ~139 survivors.
- ~~`expr_type()`'s unconditional `nocode_wanted++`.~~ Already fixed: `expr_type_vm`
  re-parses and evaluates a variably-modified operand. Of the 8 tests once attributed to
  it only `vla-14`, `vla-24` and `vla-stexp-1` still fail, each for a different reason.
- ~~`selfhost-qemu-{i386,arm}-O2`~~ **Fixed 2026-08-06.** Both targets now reach a
  byte-identical `s2 == s3` fixpoint at `-O1` and `-O2`. Two things are worth keeping:
  - **The `MCC_MAX_ALIGN` diagnosis was wrong.** The `alignment of 16 is larger than
    implemented` error at `mccdefs.h:528` was a *symptom*; `MCC_MAX_ALIGN` is not on
    that path at all. `parse_one_attribute` errors when `n != 1 << (ad->a.aligned - 1)`,
    i.e. when the `aligned : 5` bitfield fails to round-trip — and it failed because
    **every** bitfield store in the stage-2 compiler was writing 0.
  - **Root cause: `-fif-conversion` marked ternaries whose arms are two-word types.**
    `ast_sel_gpr` accepted `VT_LLONG`, which is a register *pair* on a 32-bit target —
    which is exactly why only the two 32-bit ELF targets failed. The marked body in
    `mcc.c` was `vstore`'s bitfield mask, `bits >= 64 ? ~0ULL : (1ULL << bits) - 1`. The
    replay path materialises the condition into a GPR *before* evaluating both arms;
    the arms need `r0:r1` and call `__aeabi_llsl`, so the condition is spilled and then
    reloaded into `r0` — on top of the mask's low word. The mask came out 0, so every
    bitfield store wrote 0. `gen_select` already refuses two-word types and falls back
    to `gen_select_branch`, so marking them bought nothing and only took the riskier
    both-arms-first path; the fix is one guard in `ast_sel_gpr` and is a no-op on
    64-bit, where `long long` is a single word.
  - Regression test: `tests/exec/codegen/select_two_word.c`. Verified to fail (`mask 62`
    and `mask 63` come back with a corrupted low word) on both arm and i386 at `-O2`
    with the guard disabled, and to pass at `-O0`..`-O3` on x86_64, arm and i386 with it.
    It only bites at `-O2`+, so keep it in a corpus the optfire/exec-search cells run.
- 32-byte vectors are laid at 16-byte alignment (`MCC_MAX_ALIGN` cap on i386/arm is 8,
  16 elsewhere) — open ABI decision, and a real constraint, but it was **not** the cause
  of the self-host failure above.
- Register-array decay; const-parameter assignment; `_Atomic` complex.
- Varargs pr92904 (32-byte-aligned param when caller align == 16); `__int128` old
  signedness coercion.
- `__builtin_object_size` subobject-from-declared-array-type: reverted (regressed 5
  tests); `-1` is the safe answer — **do not re-introduce**.
- Nested member designator `{.a.a=1, .a.b=2}`.
- **D6** — scalar_storage_order / ms_abi is the most dangerous open item: mcc objects
  link against gcc's, so a mismatch is *silent* wrong codegen.
- Declined upstream `7f7845cd` (VT_VOID); i386 `R_386_TLS_GOTIE` gap.
- Raise arena fidelity / finish the capture path (Phase F).
- Four hand-reproduced wrong-answer defects from the three-compiler board are written up
  under "External suites" below, where the board that found them is. (A fifth,
  `__builtin_expect` dropping side effects, is fixed and closed above.)

### Intermittent / to-confirm

**The orchestration hazard is confirmed, not a suspicion — and it manufactured three
separate false results on 2026-08-06.** Parallel agents were each given the *same absolute
path* to the main checkout's build dir. Consequences, all observed:
- Concurrent `ctest` runs race on `Testing/Temporary/LastTest.log` and
  `LastTestsFailed.log`, so one agent read another agent's failure as its own.
- A rebuild swaps `mcc` out mid-run, so a census can silently mix two trees into one board.
- `tests/exec/programs/stdio.c` writes `fred.txt` into the **current directory**; 18
  full-corpus cells sharing one cwd made `programs/stdio` look nondeterministic, which is
  what the "`iso-bf` flake" actually was. Each sweep cell gets its own cwd now.

Give every parallel agent its own worktree **and its own build directory**, and never a
path into the shared checkout. Related, and the same root cause in miniature: `cd` persists
between shell invocations, so a build launched from the wrong directory fails while
`grep -c "error:"` on its log prints 0 — check the build's exit status, not a grep.

- `superopt/promote-floor` failed once in eleven full-gate runs and passes in isolation.
  Not chased. May be the hazard above rather than a real intermittent, since those runs
  were concurrent with other agents.
- `selfhost-fixpoint-memmodel-{O3,Os}` SIGSEGV'd once under heavy parallel load and
  never reproduced. Same suspicion, now much better founded.
- ~~The two win32 `-run` cells are an intermittent wine flake.~~ **Not wine, and not a
  flake in the tree — fixed in `36e20b30`.** `tools/run-tier.sh` tore its work tree down
  with `trap 'rm -rf "$work"' EXIT`. The wineserver outlives its last client and flushes
  `user.reg`/`userdef.reg` back into the prefix as it exits, so `rm` emptied
  `.wineprefix`, the departing server recreated a registry file in it, and the closing
  `rmdir` failed `ENOTEMPTY`. That `rm` was the last command the shell ran, so under
  `set -e` **its status became the cell's status** — the cells passed 15/15 programs and
  then died in their own cleanup. ~17% per cell run under load, 7 red in 40 before, 0 in
  40 after, and 0 red across 11 full sweeps. Every lost race abandoned a
  `/tmp/tmp.*/.wineprefix`; 47 had accumulated at one or two per hour, matching "red in
  nearly every gate all day", and post-fix sweeps create none. Teardown now shuts down
  that prefix's own wineserver and can no longer decide the cell's exit status.
- ~~Reconcile the deliberate-red count.~~ **Settled 2026-08-05 by running them.** It was
  7; then **2**, both PE (`run-tier/{x86_64-win32,i386-win32}`, `tls` and `tls_threads`
  each); it is now **0** — the PE `-run` TLS defect is fixed (see the fix write-up above),
  so all four cells pass and `KNOWN_RED` is empty. x86_64-native, i386, arm and riscv64
  were closed earlier by the archive's `-run` TLS fixes. Any deliberate-red `-run` TLS
  cell reappearing is a regression. Two *further* cells became red only because the qemu
  sysroots now exist, `selfhost-qemu-{i386,arm}-O2` — **both fixed 2026-08-06**; the
  `MCC_MAX_ALIGN` attribution was wrong and the cause was `-fif-conversion` selecting
  two-word arms on 32-bit targets. See "Open codegen / front-end defects".

### External suites — three-compiler board taken 2026-08-05 at `030fb4aa`

The cross-oracle run the old entry here asked for is **done**, and it replaces the
`b8f1a80b` numbers that used to be quoted in this slot. Method: render the harness plan
once, then run it three times — clang, gcc, mcc — over both trees, and keep only the
cells where **clang and gcc both PASS**. That set is *agnostic*: portable C two
independent compilers agree on, under a plan the harness demonstrably renders correctly.
mcc's misses on it are gaps. Its misses anywhere else are somebody's extension and are
deliberately not counted.

21,638 runnable tests × {`-O0`,`-O2`} = **43,318 cells** common to all three boards.
**37,885 (87.5%) are agnostic; mcc passes 36,430 of them — 96.2%.** The 1,455 misses:

| cells | files | bucket |
| ---: | ---: | --- |
| 574 | 288 | mcc **accepts** what both reject — missing diagnostics |
| 400 | 203 | long tail, one to six cells each |
| 135 | 76 | dead call survives to link — a fold mcc does not do |
| 66 | 33 | `__bf16` unsupported (already open above) |
| 66 | 33 | missing `__builtin_*` |
| 61 | 32 | wrong or missing runtime answer |
| 52 | 26 | `__m512` / `__m256h` / `__m128h` |
| 47 | 25 | other unresolved reference |
| 28 | 14 | `__float128` / `_Float128` |
| 9 | 5 | `_Complex _Float16` |
| 8 | 4 | compound-literal initializer (already open above) |
| 4 | 2 | `_BitInt` |
| 4 | 2 | `__seg_fs` / `__seg_gs` (already open above) |
| 1 | 1 | ICE |

Reproduce with `tools/xsuite.py --mcc <cc> --out <dir> --gcc <gcctree> --llvm <llvmtree>
--opt=-O0 --opt=-O2`, once per compiler, then intersect on `(file, opt)`.

**Caveats that must travel with this board.** Samples were re-verified by replaying the
exact per-test command line against all three compilers: 12/12 confirmed for the XPASS
bucket, 11/12 for FAIL — budget ~8% replay noise on run-mode cells. `ET_FALSE` hides
whole feature families (`__int128`, vectors, LTO, profiling) from *all three* boards
equally, so "87.5% agnostic" describes the tests the harness admits, not the trees:
26,281 of 47,919 files are skipped by directives before anything runs. `KEEP_OPT_RE` does
still drop `-idirafter` and `${srcdir}` — but it does **not** drop `-Werror`, contrary to
the note this entry replaces.

#### Missing diagnostics — 574 cells / 288 files, the largest single bucket
mcc compiles these clean; gcc and clang both reject. No dominant cause — the top
families by file count are attribute-argument validation (`access`, `assume`,
`counted_by`, `malloc`, `format`, ~20 files), universal-character-name validation
(out-of-UCS-codespace and malformed `\u`), `void *` and function-pointer arithmetic
pedwarns, C++ raw strings not rejected in C, C90 non-lvalue-array rules, and asm operand
addressability. Each is a handful of files; there is no one fix.

#### Folds mcc does not do, surfacing as link failures — 135 cells / 76 files
The gcc torture suite guards `extern void link_error(void)` behind a condition the
optimizer must prove false; when the fold happens the symbol is never referenced and the
link succeeds. **58 of these fail at `-O0`**, where gcc and clang still fold them in the
front end — so this is not an `-O2` pipeline gap. Recurring shapes: `fabs(x) < 0.0`,
`__builtin_constant_p` chains, and signed-overflow reasoning.

#### Newly-confirmed wrong-answer defects, each reproduced by hand at `-O2`
- ~~`__builtin_expect` discards side effects in its second operand.~~ **Fixed** — see
  the closed list above.
- **`-fno-wrapv` does not enable signed-overflow simplification.** `fwrapv-2.c`:
  `(2*x)/2` must fold to `x`; mcc computes the wrapped value and the test aborts.
- **Builtin math folding loses to a local definition.** `20021127-1.c` defines an
  aborting `llabs` and requires the compiler to fold `llabs(-1)` to 1 regardless.
- **GNU range designators in nested initializers produce wrong data.** `gnu99-init-1.c`
  mixes `[2 ... 4][0 ... 1][2 ... 3] = 1` with later overriding designators. Adjacent to
  the nested-member-designator item above, but a distinct bug.
- **C90 `if`-controlling-expression scope.** `c90-scope-1.c`: under `-std=iso9899:1990` a
  struct declared in an `if` condition belongs to the enclosing scope, not to the `if`.
  mcc applies C99 scoping in both modes.

Do **not** refile the rest of that bucket: the `builtin-object-size` /
`builtin-dynamic-object-size` cells are the deliberately-declined item above, and
`vla-14` / `vla-24` are already open.

#### Types the front end cannot parse
`__m512` / `__m256h` / `__m128h` (52 cells), `__float128` / `_Float128` (28),
`_Complex _Float16` (9), `_BitInt` (4). `_BitInt` is C23-mandatory; the rest gate the
x86 ABI tests, which is why `gcc.target` sits ~2pp below the other suites.

#### Missing `__builtin_*` — 66 cells / 33 files
`__builtin_cpu_supports` and `__builtin_cpu_init`, `__builtin_setjmp` and
`__builtin_longjmp`, `__builtin_addc` and the `subc` family, `__builtin_powi` /
`__builtin_powif`, `__atomic_thread_fence`, `__builtin_eh_return_data_regno`,
`__builtin___fprintf_chk`, `__builtin_vprintf`.

#### Confirmations for the clusters the archive had ranked
Still open, now measured on the agnostic set: `__seg_fs`/`__seg_gs` (4 cells),
`alias("x")` resolved by C identifier instead of asm name (10), compound literal as an
initializer without braces (8, all `gnu89-init-*` and `pedwarn-init`). The std-gated
pedwarn batch and `#pragma GCC system_header` are inside the 400-cell long tail, whose
own top signatures are `initializer element is not constant` (20), `invalid array size`
(18), `constant expression expected` (16) and `string constant expected` (14).

#### Optimizer ladder board, `-O1`/`-O2`/`-O3`, same method

Re-run with the ladder instead of `-O0`/`-O2`, agnosticism decided **per level** so a
test is judged against oracles running the same optimizer level. 64,977 common cells,
56,808 agnostic.

| suite | cells | agnostic | `-O1` | `-O2` | `-O3` |
| --- | ---: | ---: | ---: | ---: | ---: |
| gcc:c-c++-common | 3162 | 83.9% | 833/884 | 833/884 | 833/884 |
| gcc:c-torture/compile | 5514 | 93.6% | 1711/1721 | 1711/1721 | 1711/1721 |
| gcc:c-torture/execute | 5127 | 93.8% | 1585/1603 | 1584/1602 | 1584/1602 |
| gcc:gcc.dg | 34818 | 88.0% | 9826/10217 | 9823/10213 | 9821/10211 |
| gcc:gcc.misc-tests | 96 | 78.1% | 24/25 | 24/25 | 24/25 |
| gcc:gcc.target | 8166 | 92.1% | 2362/2508 | 2362/2508 | 2362/2508 |
| llvm:clang | 8046 | 73.6% | 1856/1975 | 1856/1975 | 1856/1975 |
| llvm:compiler-rt | 45 | 46.7% | 7/7 | 7/7 | 7/7 |
| **TOTAL** | | | **96.1%** | **96.1%** | **96.1%** |

**The optimizer introduces no failures.** Of 18,928 files agnostic at all three levels,
**exactly one** has an mcc verdict that changes with `-O`, and it moves the *good* way:
`gcc.dg/torture/20240517-1.c` is FAILEXE at `-O1` and passes at `-O2`/`-O3`. Nothing
regresses from `-O1` to `-O3`. That is the single most useful line on this board — it
says the remaining 736 misses per level are front-end and diagnostic work, not
optimizer bugs, and it is why the buckets above carry no `-O`-specific column.

This board also cross-validates the first one: the XPASS count fell by exactly the 3
files the function-pointer-subscript fix closed, and FAIL by the 3 the powi commit
closed, with everything else unmoved.

- **String literals are not merged until `-O2`.** The one level-dependent cell.
  `20240517-1.c` needs `"Hello"` and `"Hello" + 1` to denote the same object; gcc and
  clang merge identical string literals from `-O1` (`-fmerge-constants`), mcc only from
  `-O2`, so the test aborts at `-O0` and `-O1`. The test's own
  `-fmerge-all-constants` is dropped by `KEEP_OPT_RE`, so this is mcc's *default*
  behaviour at `-O1` being measured, which is the thing that differs.

#### Harness defect found and fixed while taking this board
`tools/xsuite.py` buffered the child's stdout in the parent for *every* mode, including
`-E`, where it is never read. One gcc.dg preprocessor test drove clang to emit multi-GB
of expansion; the parent held 7.4 GB resident, one worker went uninterruptible in
page-fault and the GIL starved the other nine. Throughput collapsed from ~240 results/s
to 8/s and the clang board could not finish. Preprocess stdout now goes to `DEVNULL`,
run-mode stdout likewise (nothing reads it — `dg-output` is ignored), and captured
stderr is capped at 1 MiB.

That fix was **incomplete and the ladder board hit the other half**: capping the string
after `capture_output` still buffers the whole stream first. `gcc.dg/builtin-stdc-bit-1.c`
is a *stderr* bomb under clang (it does not know `__builtin_stdc_bit_*`), and the parent
reached 12.7 GB with the same D-state/GIL-starvation collapse. Child output now goes to
a file that `RLIMIT_FSIZE` actually binds — lowered from 1 GiB to 64 MiB, which is ample
for these `.o`s — and only the first 1 MiB is read back. That test now records a clean
TIMEOUT, and the resumed board ran at 12,000 results/45 s with the parent at 3.7 MB.

**Two traps this cost a run each, worth knowing before quoting a board.** A compiler
copied out of the build tree cannot find its own freestanding headers: an `mcc` pinned
to `/tmp` failed 2,321 cells on `stddef.h not found` and scored 84.4% instead of 96.1%.
A smoke test using only `__builtin_*` will not catch it — include something. And do not
relink `mcc` while a board is running against it; pin by leaving the binary in place and
not building, not by copying it elsewhere.

Unfixed and still latent: `subprocess.run`'s timeout kills the driver but not the
`-cc1` grandchild, so orphaned compilers accumulate and keep burning CPU; `preexec_fn`
is also documented-unsafe under threads.

## The parse-depth guard — closed, and it was five holes not one — 2026-08-09

`MCC_MAX_UNARY_DEPTH 2048` is gone. The parser now shares one budget,
`MCC_MAX_PARSE_DEPTH 512` (`src/mcc.h`), charged and released by
`mcc_parse_depth_enter/leave` (`src/mccgen.c`) at **eight** recursion entry points:
`unary`, `expr_cond`, `block`, `post_type`, `type_decl_1`, `decl_initializer_1`,
`struct_decl` (`src/mccgen.c`) and `macro_subst` (`src/mccpp.c`). Each is a thin
wrapper over a renamed `*_nested` body, the shape `unary`/`unary_nested` already had.
The counter is reset in `preprocess_start`, which runs on every entry path
(`-c`, `-E`, `-S`, asm) and is therefore the correct place to absorb the `longjmp`
out of `mcc_error` that skips every pending `leave`.

**The per-level cost. Re-derived 2026-08-09 on `wt/framewatch`; the table below is the
corrected one and the row it replaced was low on nine axes of ten.** The original was a
gdb `$sp` sample taken at the `*_nested` entry, which misses the thin
`mcc_parse_depth_enter` wrapper's own frame, so every cycle containing one was under-
counted by exactly that wrapper: paren by 16 B, macro by 64, struct by 48, initializer
braces by 48, blocks and `for` by 32. `?:` was 48 B *high* and parenthesised declarators
were out by more than 3×. Two methods now agree instead of one: the frames summed from
the x86-64 prologues in the built binary, and the slope of the depth at which each axis
dies under `ulimit -s` 512 / 1024 / 2048 KiB. They agree on all thirteen axes to within
**0.6%**, so the cycles below are the real ones, taken from gdb backtraces at
`mcc_parse_depth == 200`:

| axis | cycle | B per source level | depth units | B per depth unit |
| --- | --- | ---: | ---: | ---: |
| `sizeof sizeof …` | `unary`+`expr_type`+`expr_type_vm`+`unary_nested` | **992** | 1 | **992** |
| casts, `!` chains | `unary`+`unary_nested` | **848** | 1 | 848 |
| `*&` chains | 2× (`unary`+`unary_nested`) | **1,696** | 2 | 848 |
| macro nesting | `macro_subst`+`macro_arg_subst2`+`macro_arg_subst`+`macro_subst_tok`+`macro_subst_nested` | **848** | 1 | 848 |
| nested `struct` | `struct_decl`+`parse_btype`+`struct_decl_nested` | **576** | 1 | 576 |
| nested `(…)` | `unary`+`unary_nested`+`gexpr`+`expr_eq`+`expr_cond`+`expr_cond_nested` | **1,104** | 2 | 552 |
| nested initializer braces | `decl_initializer_1`+`decl_initializer`+`decl_initializer_nested` | **416** | 1 | 416 |
| `for`/`while` nesting | `block`+`lblock`+`block_nested` | **400** | 1 | 400 |
| block / `if` nesting | `block`+`block_nested` | **336** | 1 | 336 |
| `?:` chains | `expr_cond`+`expr_cond_nested` | **192** | 1 | 192 |
| parenthesised / fn-ptr declarators | `type_decl_1`+`type_decl_nested` | **192** | 1 | 192 |

The conclusions the sizing rests on all survive re-measurement. `unary_nested` is
**832 B** exactly, and is 75% of the 1,104 B paren cycle and 84% of the binding
992 B `sizeof` cycle. `sizeof` is still the worst axis per depth unit at **992 B**, so
512 levels still cost 496 KiB of recursion. The rlimit cross-check reproduces too:
`paren` last compiles at depth 459 / 934 / 1,882 under `ulimit -s` 512 / 1024 / 2048 KiB
against the filed 462 / 947 / 1,912, inside the noise band described below.

**What the old guard admitted.** 2048 × 1,088 B = 2,176 KiB, so `MCC_MAX_UNARY_DEPTH`
needed a stack larger than the 2 MiB a Linux *thread* gets by default. Reproduced
verbatim at `afe3fa18` with 2,040 nested parens: `ulimit -s 2048` → **exit 139
(SIGSEGV)**, `4096` → exit 0, `8192` → exit 0.

**Worse: seven of the eight paths had no guard at all**, so they were unbounded at any
stack. Measured crash depths on the unfixed tree at the 8 MiB default — every one of
these SIGSEGVs a *default* shell, no `ulimit` needed:

| path | crashes at depth (8 MiB) |
| --- | ---: |
| macro nesting `M(M(M(…)))` | 16,383 |
| nested `struct` definitions | 16,383 |
| nested `{}` blocks, `if`, `while`, `for` | 32,767 |
| nested initializer braces | 32,767 |
| `?:` chains | 65,535 |
| parenthesised / function-pointer declarators | 131,071 |

Three shapes are genuinely iterative and never crash, checked to depth 262,143:
long binary-operator chains (`expr_infix`), `&&`/`||` chains, and comma chains.
`assign`, `arraydecl` and nested calls hit `memory full (vstack)` — a clean diagnostic,
already correct.

**Sizing.** 512 × 992 B (the worst per-unit axis) ≈ 496 KiB. Bisecting the rlimit
across all thirteen axes at depth 20,000: the smallest stack on which every one of them
reaches the diagnostic instead of the guard page is **520 KiB**. That fits Windows'
1 MiB default reserve with 480 KiB spare, the glibc 2 MiB thread default 4× over, and
the 8 MiB main-thread default 16× over. It does **not** fit musl's 128 KiB thread
default — mcc on a musl thread needs an explicit `pthread_attr_setstacksize`.

**What a conforming program is entitled to, versus what mcc supports.** C11 §5.2.4.1
requires an implementation to translate at least one program containing 127 nesting
levels of blocks, 63 of parenthesised expressions within a full expression, 63 of
parenthesised declarators, 63 of conditional inclusion, 63 of nested structure or union
definitions, and 12 declarators modifying a type. Because the budget is *shared*, the
worst case is all of them nested inside one another — so that program was built and
compiled rather than argued about. It reaches **`mcc_parse_depth` = 256, exactly half
of 512**, and both mcc and `gcc -std=c11 -pedantic` accept it. mcc's headroom over the
standard's floor is therefore **2×** with every minimum simultaneously in force, and
much larger per-axis: 256 nested parens, 512 nested blocks, 512 nested declarators.
The floor program is regenerated and compiled by the new cell, so a future attempt to
buy safety by lowering the constant will fail loudly instead of silently dropping below
conformance. **512 is not a limit any hand-written C reaches**: the high-water mark over
every `.c` file in `src/`, `tests/`, `tools/`, `runtime/` and `examples/` is **130**,
and that is `tests/exec/statements/translation_limits.c`, which exists to be extreme.

**The cell.** `diag.parse-depth` (`tests/diagnostics/parse-depth.sh`, registered beside
the `dg-error` glob) generates all thirteen axes at depth 40,000 plus the C11 floor
program, and requires each to exit with `program nests too deeply` — never a signal,
never rc 0. It **lowers its own stack rlimit to 1024 KiB and refuses to run if it
cannot**, saying so on stdout: a pass at the runner's inherited 8 MiB would prove
nothing, since the whole defect is that the admitted depth outgrows a small stack.
Three `dg-error` drop-ins (`parse_depth_parens`, `parse_depth_blocks`,
`parse_depth_macro`) assert the same diagnostic with no rlimit involved at all.

Ablation against `afe3fa18` rebuilt in scratchpad — 13 of 14 cases:

```
parse-depth: SIGNAL rc=139 (signal 11) on paren
parse-depth: SIGNAL rc=139 (signal 11) on cast
parse-depth: SIGNAL rc=139 (signal 11) on sizeofchain
parse-depth: SIGNAL rc=139 (signal 11) on derefchain
parse-depth: SIGNAL rc=139 (signal 11) on ternary
parse-depth: SIGNAL rc=139 (signal 11) on blocks
parse-depth: SIGNAL rc=139 (signal 11) on ifchain
parse-depth: SIGNAL rc=139 (signal 11) on forchain
parse-depth: SIGNAL rc=139 (signal 11) on initbraces
parse-depth: SIGNAL rc=139 (signal 11) on declparen
parse-depth: SIGNAL rc=139 (signal 11) on declfnptr
parse-depth: SIGNAL rc=139 (signal 11) on structnest
parse-depth: SIGNAL rc=139 (signal 11) on macronest
parse-depth: 13 of 14 cases failed at a 1024 KiB stack
```

The fourteenth is the C11 floor program, which compiles on both trees — it is a
conformance guard, not a crash guard, and it is meant to be insensitive to the fix.
The three `dg-error` cells ablate as
`expected compile to FAIL for …/parse_depth_parens.c, but it succeeded (rc=0)`.

**Emitted code is unchanged.** 900 TUs (`tests/`, `examples/`, `runtime/`, `src/`,
`tools/`) compiled by both trees at `-O0`–`-O3`: **2,995 objects byte-identical, 0
differing**, 604 failing identically on both (the negative-diagnostic corpus), and **1**
self-unstable under a back-to-back recompile of the *same* binary — the `__TIME__`
case, which is why the sweep re-compiles before believing any diff.

**The census had to be re-banked, and it is dilution, not regression** — exactly the
case "The lowerable ratchet is self-referential" describes. Eight wrappers plus
`mcc_parse_depth_enter`/`leave` add 14 tiny non-lowerable bodies to `src/mcc.c`. The
absolute lowerable count is **658 bodies before and after**; only the denominator moved,
4,241 → 4,255, so `wide` `bodies_pct` fell 15.5152% → 15.4642% at `-O0` and 15.4748% →
15.4242% at `-O1`–`-O3`. Re-banked with `--update-bank-low`; nothing else in
`tests/rir/coverage-bank.json` moved.

Verification on this tree, `cmake-cross` built before `cmake-debug` was configured:
`ctest` **9159/9159** (9155 + `diag.parse-depth` + three `dg-error` drop-ins),
`-L flagsweep` 119/119, `-L stratsweep` 30/30, `MCC_RIR_CENSUS=1 ctest -L census` 6
registered / 3 skipped / 0 failed, `tools/selfhost-smoke.py cmake-debug` green,
`tracegate src` and `schemagate src` both OK.

### The frames are watched now — `diag.parse-frames` — 2026-08-09 (`wt/framewatch`)

The guard is a *depth* budget whose safety comes entirely from a per-level cost measured
once. **`tools/parse-frames.py` re-measures that cost on every `ctest` run**, reads the
frame of all 24 functions on the thirteen recursion cycles out of the built `mcc`'s
x86-64 prologues (8 B return address, 8 per `push`, plus `sub $N,%rsp`), prices
`MCC_MAX_PARSE_DEPTH` against them, and fails if the requirement drifts. The bank is
`tests/diagnostics/parse-frames.json`; the cycle model is `AXES` in the tool, because it
is hand-derived and regenerating it would defeat the point.

**Static frames, not the rlimit bisect, and the reason is a measurement.** The bisect the
previous author proposed is not deterministic near its own threshold: `sizeofchain` at
depth 20,000 fails **15/15** runs at 506 KiB, **13/15** at 508 and 510, **7/15** at 512,
**5/15** at 514 and **0/15** at 516 KiB, because argv, environ and the kernel's stack
randomisation all live inside the rlimit. The filed **520 KiB** is right — it is the
first reliably-passing multiple of 8 — but a gate that flips run to run across an 8 KiB
band is worse than none. The static sum has no such band, is exact against the bisect to
0.6% on every axis, costs one `objdump`, and names the function. The bisect is kept as
`--bisect` for re-deriving the bank, and its per-axis output is stored in the bank as
provenance that nothing compares against.

**What it asserts.** Five gates, each ablated:

| gate | fires when | ablation |
| --- | --- | --- |
| drift | requirement > banked + 16 B/level | `volatile char pad[256]` in `unary_nested` → `unary_nested grew 832 -> 1088 B (+256) and is on 4 of 13 recursion cycles (cast, derefchain, paren, sizeofchain)` … `needs 644 KiB of host stack, banked 516 KiB` |
| ceiling | requirement > 768 KiB | `pad[1024]` → `1028 KiB … past the 768 KiB ceiling` |
| model | a banked function is not in the binary | run against an `-O2` mcc → `10 function(s) … are not in … at all … has stopped watching anything` |
| budget | `MCC_MAX_PARSE_DEPTH` moved without a re-derivation | 512 → 900 → `The budget moved without the stack it costs being re-derived: 900 levels … need 891 KiB` |
| coverage | the set of `mcc_parse_depth_enter()` call sites moved (banked `mccgen.c x7, mccpp.c x1`) | guard `asm_expr_unary` → `called from mccasm.c x1, mccgen.c x7, mccpp.c x1, banked mccgen.c x7, mccpp.c x1 … so the new one's per-level cost is in nobody's budget` |

The 16 B/level tolerance is one x86-64 stack-alignment quantum on the binding cycle —
below any local a person would add, above the padding a compiler point release can
shuffle. The 768 KiB ceiling is 75% of the 1024 KiB rlimit `parse-depth.sh` lowers itself
to, so the frame cell fails first and names a cause instead of the sibling cell failing
later as thirteen SIGSEGVs. That ordering was checked: at `pad[2048]` the model says
1,540 KiB and `diag.parse-depth` then dies on exactly the three `unary_nested` axes
(`cast`, `sizeofchain`, `derefchain`) while `paren`, whose cost is halved over two depth
units, survives.

**Which build the bank describes: `-O0`, and it is the worst case.** `cmake-debug`
compiles `src/mcc.c` with `-g` and no `-O`, and that is what the bank is taken from.
The same source at `-O2` needs **452 KiB** rather than 516, and its worst axis moves from
`sizeof` to macro nesting, because gcc inlines the eight thin wrappers into their
`*_nested` bodies — ten of the 24 banked symbols stop existing. So the released and
self-hosted compilers, which are built optimised, are strictly *safer* than the number
banked here, and the `-O0` bank bounds them. It also means the bank cannot be checked
against an optimised build at all, which is why the cell is registered only for an
unoptimised gcc x86-64 host build and why a missing symbol is a failure rather than a
skip.

### Two more recursion points the eight did not cover — filed, not fixed

Probed on this tree at the default 8 MiB stack, no `ulimit` needed. Both bypass
`mcc_parse_depth` completely, so `MCC_MAX_PARSE_DEPTH` does not bound them:

| path | shape | crashes at |
| --- | --- | ---: |
| `parse_btype` `TOK_ALIGNAS` arm (`src/mccgen.c`) calls `parse_btype` directly, not through any of the eight | `int _Alignas(_Alignas(_Alignas(int) int) int) x;` | **43,609** |
| the assembler expression parser, `asm_expr`/`asm_expr_logic`/`asm_expr_cmp`/`asm_expr_sum`/`asm_expr_prod`/`asm_expr_unary` (`src/mccasm.c`), a six-function cycle that never touches `mcc_parse_depth` | `__asm__(".set zz, ((((…1…))))");` | **~19,000** (clean at 18,000, SIGSEGV at 20,000) |

The `_Alignas` one is the sharper of the two: `int _Alignas(_Alignas(int) int) x;` is an
mcc extension — `gcc -std=c11` rejects it with *"expected specifier-qualifier-list before
`_Alignas`"* — so it is unbounded recursion reachable from a plain `mcc -c` on grammar
mcc alone accepts. Both are cheap to fix the same way the eight were, and neither is
fixed here.

**Probed and clean, so that the next reader does not re-probe them.** None of these
crashes at depth 40,000: array-typedef chains (`typedef A0 A1[1];` ×n, then `sizeof`,
which drives `type_size`), pointer-typedef chains assigned to each other (`compare_types`
/ `is_compatible_types`), nested `struct` *member* chains with and without `-g`
(`mcc_get_dwarf_info`, `find_field`), and `#if` with nested parentheses. The `mccast.c`
replay and optimiser walkers are bounded transitively by the parse guard even with
inlining amplifying AST depth: sixteen `static` functions chained through
`ast_inline_depth_max`, each carrying a 250-deep parenthesised expression, compiles clean
at `-O0`, `-O1`, `-O2` and `-O3`.

Still stale and untouched here: `docs/PLAN.md:435` names `MCC_MAX_UNARY_DEPTH 2048` at
`src/mccgen.c:241`; that symbol no longer exists.

Verification for `wt/framewatch`, `cmake-cross` built before `cmake-debug` was
configured: `ctest` **9162/9162** (9161 + `diag.parse-frames`), `-L flagsweep` 119/119,
`-L stratsweep` 30/30, `ctest -L census` 7/7 with nothing Skipped,
`tools/selfhost-smoke.py cmake-debug` green, `ci/must-run-registered` 66 rows.
**`src/` is byte-identical to `d67f16b5`** — the change is one tool, one bank, a
`CMakeLists.txt` registration and a manifest row — and the sweep says so independently:
900 TUs at `-O0`–`-O3`, **2,996 objects byte-identical, 0 differing**, 604 failing
identically on both, 0 self-unstable.
**Left open, deliberately.** The guard is a *depth* budget with a per-level cost fixed
by measurement of the `-O0` build; it is not a stack-headroom probe. That is the right
trade here — the diagnostic must be reproducible for the cell to be worth anything, and
an rlimit-derived limit would fire at a different depth on every host — but it means the
bound is only as good as the frame sizes it was measured against. **Anything that grows
`unary_nested`'s 832-byte frame silently erodes the margin, and nothing watches it.**
The cheapest watchdog is the `520 KiB` figure above: re-run the rlimit bisect after any
change to the frames of the eight guarded functions. `docs/PLAN.md` used to name the old
`MCC_MAX_UNARY_DEPTH 2048` budget at a `src/mccgen.c` anchor for a symbol that no longer
exists; **fixed on `wt/docsync`**, and `docs/refs` (`tools/docref-lint.py`) now fails on a
symbol quoted beside a file:line it does not occur at, so that shape cannot recur silently.
change to the frames of the eight guarded functions. `docs/PLAN.md:435` still names
`MCC_MAX_UNARY_DEPTH 2048` at `src/mccgen.c:241`; that symbol no longer exists.

## 256-bit integers: `__int256` / `unsigned __int256` (`wt/bits256`)

### Which "256-bit" this is, and why

Two readings were live. The survey settled it before any code was written.

**256-bit SIMD was already there.** `__attribute__((vector_size(32)))` parses, computes
and is passed today: a vector is an anonymous struct carrying `SymAttr.is_vector`
(`mk_vector_type` / `is_vector_type` in `src/mccgen.c`), the size cap is on the element
*count* (`MCC_VECTOR_MAX_ELEM` = 1024), and `runtime/include/avxintrin.h` already
defines `__m256`, `__m256d`, `__m256i` plus the AVX/AVX2 intrinsic bodies as pure C over
that extension. What is missing is not correctness but (a) 32-byte *alignment* — capped
at `MCC_MAX_ALIGN` in `mk_vector_type`, already an open row in this file — and (b) real
`ymm` codegen, which needs a VEX encoder (none exists anywhere in `src/arch/`), a
width-aware register model (`MCC_NB_REGS` is a flat 33 scalar slots) and a
size-parametric spiller (`save_reg_upstack` reduces a spilled value to a bare base type
with no `ref`, so a `VT_STRUCT` vector cannot survive it). That is the multi-week
rewrite; a conservative estimate is 2,000–3,000 lines across the x86-64 backend, the
allocator and the SysV classifier, and it buys speed on a path that is already correct.

**256-bit integers did not exist at all.** No `_BitInt`, no `__int256`, and `__int128`
is x86-64-ELF-only (`MCC_HAVE_INT128`), modelled as a register *pair* (`r`/`r2`,
`qexpand`/`qbuild`). So the integer axis was the real gap, and the instruction was to
prefer it. This section is the integer one.

### The shape, and why it is not a register quad

`SValue` has exactly two register fields, `r` and `r2`. A 256-bit value needs four
64-bit registers, so the `__int128` scheme does not extend without rewriting the value
stack, the spiller and every backend's `load`/`store`. It is instead **memory-backed**,
following the precedent the tree already set for vectors and `_Complex`: `__int256` is
an anonymous four-limb struct of `unsigned long long` (`long long` for the signed
variant) carrying a new one-bit `SymAttr.is_wideint`. Signedness is read off the first
limb's type rather than a second attribute bit, so the change costs **one** bit of the
three that were free in `SymAttr`.

Everything structural then falls out of the struct path that every target already
implements: `sizeof` 32, `_Alignof` 16 (`min(32, MCC_MAX_ALIGN)`, so 8 on i386/arm),
little-endian limb order, struct members, arrays, assignment, `va_arg`, and the ABI.

| file | what is new |
| --- | --- |
| `src/mcc.h` | `MCC_WIDE256_BITS`/`_LIMBS`/`_SIZE`; `CValue.q` widened `{lo,hi}` → `{lo,hi,w2,w3}`; `SymAttr.is_wideint`; `gen_wide256_type_cache[2]` + `gen_wide256_limb_tok` |
| `src/mcctok.h` | `TOK_INT256` and fifteen `__mcc_i256_*` helper tokens (placed **outside** the `MCC_ARM_EABI` guard — inside it the arm build cannot see them) |
| `src/wide256_arith.h` | the 4-limb kernel: add/sub/mul/div/mod/shift/compare/neg/not, shared verbatim by the compiler's folder and the runtime |
| `src/wide256_slice.h` | `mk_wide256_type`, `gen_wide256_op`, `gen_wide256_cast`, `wide256_deconst`, `wide256_settle`, `wide256_init_putv` |
| `src/mccgen.c` | `is_wide256_type`/`wide256_is_unsigned`; hooks in `gen_op`, `gen_cast`, `combine_types`, `verify_assign_cast`, `vstore`, `gen_assign_cast`, `gaddrof`, `inc`, `init_putv`, `decl_initializer_nested`, `type_to_str`, `parse_btype`, and the cast-to-non-scalar guard in `unary` |
| `src/mccpp.c` | `__SIZEOF_INT256__` predefine |
| `runtime/lib/int256.c` | the `__mcc_i256_*` exports; added to the `_common` mccrt object list, so every cpu gets it |

Runtime values go through the helper calls; compile-time constants are folded in the
compiler with the *same* kernel and land in `.data` through `init_putv`, so
`static __int256 g = ((__int256)1 << 200) + 3;` is a load-time constant.

### What is gated off, and why

Each of these is a hard error with its own message, proved by
`tests/exec/types/int256_gates.c` (a `dt` golden — sixteen arms, fifteen of them
diagnostics):

- **`__int256` ↔ `float`/`double`/`long double`** — *"conversion between `__int256` and
  floating-point types is not supported"*. Deliberate: a conversion routine would be the
  one part of this work with **no independent oracle** (GMP is exact-integer; gcc has no
  256-bit integer type to compare against), so it would be agreement with nothing. The
  refusal is cheap to lift once an oracle exists.
- **`switch` on `__int256`** — *"switch value not an integer"*. `expr_case_const` tops
  out at 128 bits.
- **bitfields, `vector_size`, `_Complex __int256`, `long __int256`, `__int256 int`,
  pointer↔`__int256` casts, constant division by zero, non-constant load-time
  initialisers** — all rejected.

`__int256` itself is **not** gated by target: because it is memory-backed it works
everywhere, and that is verified below on all five.

### The ABI decision, per target

There is no new ABI. A 256-bit value is passed and returned exactly as the target's
existing 32-byte struct: **memory class** on x86-64 SysV (`classify_x86_64_arg` sends
anything over 16 bytes to `x86_64_mode_memory`), indirect via the sret pointer on arm64,
memory on riscv64 (over 2×XLEN), stack on i386 and arm AAPCS. This is the only choice
that needs no backend change and no new classification rule, and it is the same rule
gcc/clang apply to a 32-byte aggregate. Alignment is 16 where `MCC_MAX_ALIGN` allows it
and 8 on i386/arm; nothing external interoperates with `__int256`, so this is a
definition rather than a constraint.

Because it is an mcc extension there is no cross-toolchain compatibility claim: a
`__int256` never crosses a TU boundary to gcc.

### The differential, and its oracle

`wide256/gmp-diff` (`cmake/wide256_diff.cmake`, subject `tests/wide256/subject.c`,
oracle `tests/wide256/oracle.c`). The oracle is **libgmp** — arbitrary-precision integers
reduced mod 2^256, compiled by the *host* C compiler. It shares no line of code with mcc,
so an agreement is evidence and a disagreement is an mcc defect.

The corpus is 18 operands (0, ±1, `INT256_MIN`, `INT256_MAX`, each limb boundary
2^64/2^128/2^192 and their predecessors, mixed-limb patterns) crossed with itself for
+ − × ÷ % & | ^ signed *and* unsigned, ten comparisons, 14 shift counts
(0, 1, 31, 32, 63, 64, 65, 127, 128, 191, 192, 255, **256**, **−5**) for `<<`, `>>`
arithmetic and `>>` logical, and conversions to and from `signed char`, `unsigned char`,
`short`, `unsigned short`, `int`, `unsigned`, `long long`, `unsigned long long` and
`_Bool` in both directions. **9,402 rows.** Every row is emitted twice: once from
runtime values loaded by `memcpy` (so the print path does not depend on the shift
implementation being right) and once from `static const` arrays built by the
*compile-time folder*, so the folder and the runtime are both on the hook.

Result: **9402/9402 agree, at `-O0`, `-O1`, `-O2`, `-O3` and `-Os`.**

Cross-target, driven by hand with `cmake-cross` and qemu-user against the same oracle
output: **arm64, riscv64, i386 and arm each produce all 9,402 rows byte-identical at
`-O0` and `-O2`**, and x86-64 natively. That is the ABI claim, tested rather than argued.

Two semantics this differential *defines* rather than discovers, because C leaves them
undefined and the oracle had to be told: a shift count outside `[0, 256)` yields 0 (or
all sign bits for `>>` on a signed value), and division by zero yields an all-ones
quotient with the dividend as remainder — no trap. A **constant** division by zero is
still a hard compile error, so the two never disagree observably.

### Cells that can fail

`wide256/gmp-diff-known-positive` perturbs one corpus operand and one folded constant and
requires the differential to go red; both rows are in `tests/must-run.txt`. It reports
`clean OK, mutation detected`. The differential also carries a floor — under 9,000 oracle
rows is a hard failure, so a truncated or empty subject cannot agree with an empty
expectation.

Two ablations were taken against the *implementation*, not the test:

1. Borrow propagation dropped in the 4-limb subtract (`borrow = b1 | b2` → `borrow = b1`
   in `src/wide256_arith.h`). `wide256/gmp-diff` and `exec/int256` both fail;
   the text is *"wide256/gmp-diff: mcc's `__int256` disagrees with GMP at
   -O0;-O1;-O2;-O3;-Os. The oracle is libgmp compiled by the host C compiler and shares
   no code with mcc, so a disagreement is an mcc defect -- fix the compiler or the
   runtime, do not re-pin the expectation. First differences (-O0): ...
   < smod 2 9 ffff...ffff / > smod 2 9 0000...ffff"*.
2. Sign extension removed from the front end (`wide256_store_int` forced to zero-extend).
   Same two cells fail, first difference at row 7541,
   *"< fromsc 2 0 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"*.

Plus 44 exec cells: `int256` and `int256_gates` are `dt` goldens, so each runs through all
22 exec pipelines (`exec/`, `-O1`, `-O3`, `-Os`, `replay`, `replay-tmpl`,
`replay-promote`, `narrowfix`, `chainstore`, `ivsrptr`, `vlat`, `select`, `zerobss`,
`interchange`, `fusion`, `tile`, `mergestrings`, `search`, `search-emitsize`,
`search-emitiso`, `search-threads`, `gatesoff`). `dt` mode deliberately registers **no**
`diff3` cell — gcc and clang cannot compile `__int256`, so a three-way consensus cell
there would be a permanent skip.

### Emitted code for existing types

Two independent checks, both clean.

1. **TU sweep.** 493 sources under `tests/exec`, `tests/diff`, `tests/idiom`,
   `tests/optfire/src`, `tests/behavior`, `tests/runtime` and `runtime/lib`, compiled at
   `-O0`–`-O3` by the pre-change `mcc` and the post-change `mcc`: **1,972 objects,
   1,915 byte-identical, 0 self-unstable, 56 not compilable standalone by either (skipped
   identically), 1 spurious** — `tests/exec/preprocessor/predefined_macros.c` at `-O3`,
   which embeds `__TIME__`; a back-to-back recompile of that one file is byte-identical.
2. **The `-O0` object bank.** `tests/ast/o0-baseline/*.obj.txt` was re-taken on all
   twelve target keys. Every one gained **exactly two** lines (the two new goldens) and
   **changed none** — so no pre-existing object hash moved on x86_64, i386, arm, arm64,
   riscv64, their `-win32`/`-wince` variants or the two `-osx` ones.

### Banks re-taken, and why

- `tests/rir/coverage-bank.json` (`--update-bank-low`): the new lowering is call-heavy
  and struct-typed, so it dilutes `self`'s `nodes_pct_strict` 26.1250% → 26.0727% at
  `-O0` and 26.1038% → 26.0519% at `-O1`–`-O3`. Denominator growth, not a regression.
- `tests/ast/o0-baseline/` (both boards, ungated first): the corpus is
  `find tests/exec -name '*.c'`, which now finds two more files. 306 files per key.
- `src/wide256_arith.h` is named `.h`, not `.inc.c`, on purpose: `tools/fmt-census.py`
  globs `src/*.c` and treats anything unlisted as a corpus that moved under the census.

### Parser depth

**No new recursion and no growth in any guarded frame.** `gen_wide256_op` and
`gen_wide256_cast` are called from `gen_op`/`gen_cast`, which are not among the eight
`MCC_MAX_PARSE_DEPTH` entry points; the recursion they do add is bounded at one
(`gen_wide256_op` re-enters `gen_op` only after narrowing a 256-bit shift *count* to
`int`, at which point neither operand is 256-bit). `parse_btype` gained one `case` and
one finalisation block, no call.

### Latent defect found on the way, not fixed here

`rir_decayed_array` in `src/mccrir.c` dereferences `sv->sym` after only a `!sv->sym`
null test. For a `VT_CMP` `SValue` that field aliases `cmp_op`/`cmp_r`, and after `gv()`
materialises a `VT_CMP` the field is left stale rather than cleared — `gv` assigns
`vtop->r` without touching `vtop->sym`. Either shape reaches `rir_leaf_slot` as a small
integer masquerading as a `Sym *` and segfaults the compiler. It is reachable only when a
comparison or a `_Bool` cast survives into a captured vstack snapshot, which is why it had
never fired. This work walked into it twice and side-steps it with `wide256_settle`
(materialise, then clear `sym`) rather than changing `mccrir.c`, because that file is
under concurrent edit. **The underlying hazard is still there for the next caller.** The
cheap fix is to reorder the test in `rir_decayed_array` to check `sv->r != VT_CMP` first,
or to clear `sym` in `gv`'s `VT_CMP` path; neither was taken here.

### Still open

- Float conversions, gated above. Needs an oracle before it needs code.
- `_BitInt(N)`, still absent; `__int256` does not advance it, though the 4-limb kernel and
  the memory-backed struct representation are the two pieces a fixed-width `_BitInt` would
  reuse.
- No `__int256` literal suffix — the same gap `__int128` has. Constants are written as
  shifted/or-ed 64-bit literals and folded.
- DWARF describes an `__int256` as its underlying anonymous four-limb struct, so a
  debugger prints limbs rather than a value. `type_to_str` already says `__int256` in
  diagnostics; the debug-info side was not done.
- Arithmetic is a call per operation. Inlining add/sub/bitwise is a straightforward
  follow-up; nothing here was measured for speed, only for correctness.

### Verification, this tree

`cmake-cross` built before `cmake-debug` was configured; both register **9207** cells
(9161 + 44 exec + 2 wide256). Full `ctest` **9207/9207, 0 failures**;
`-L flagsweep` 119/119; `-L stratsweep` 30/30; `-L census` 7/7 with nothing skipped;
`python3 tools/selfhost-smoke.py cmake-debug` green.
