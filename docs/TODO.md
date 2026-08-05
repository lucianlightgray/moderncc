# TODO

> History, landed-fix write-ups, superseded boards, and the "landmine" warnings that
> used to live here were moved to [`docs/ARCHIVED.md`](ARCHIVED.md) on 2026-08-05 for
> later validation. This file now holds only the configuration instructions below and
> present-tense, open items. File:line anchors are omitted on purpose — the archived
> ones had drifted 1000–1900 lines after merges; find code by symbol.

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

**Coverage owed.** `tests/optfire/flagsweep.sh` checks that every flag in the table
accepts both spellings (113/113) and that turning each on and off still computes the
reference answer on twelve exec goldens. The exec half is **not yet wired as ctest
cells** — that is the next job, and it matters: 43 of these flags are referenced
nowhere in `tests/`, `tools/` or `CMakeLists.txt`, which is exactly the population
`-fjit-splice` came from. The first version of that harness used two synthetic
programs and *passed* `-fjit-splice`; it now uses real goldens covering threads and
atomics, and fails it. A sweep that misses the bug you already have is worse than no
sweep, because it reads as coverage.

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

### Flag-sweep coverage
- Wire `tests/optfire/flagsweep.sh`'s exec half as ctest cells (see the config note
  above). 43 of the 113 `-f` flags are referenced nowhere in `tests/`/`tools/`/
  `CMakeLists.txt` — the population `-fjit-splice` came from.

### C2 gap — remaining Replay_IR fidelity work
- Close the open per-body byte divergences (the "largest first" list in the archive).
  **Fix at the USE site, never the CAPTURE site.**
- `full_language.c` still diverges at `-O0` on x86_64/i386 — an `AST_OP_ASM` replay
  defect (P4 defect 4), contained not closed.
- **Do not turn `-fno-replay-fallback` (`MCC_RIR_NOFB`) on by default until the
  byte-faithful step is green.** `keep = faithful || (nofb && replay_completed)` is
  load-bearing — ≥4 of the fallback bodies are genuinely wrong, not benign.
- `-fno-replay-fallback` + selfhost-jit-with-mcc blows RSS ~6.7×; bisect via
  `MCC_RIR_NOFB_SKIP`. Measure on the full suite, not the 317 exec goldens.
- Re-bank `o0-baseline` at HEAD on an x86_64 Linux host. (Verified 2026-08-05 that the
  Darwin side *reproduces*: all five PE keys are byte-identical to a Linux run at the
  same HEAD, and the two `*-osx` keys are measurable only on Darwin. Also fixed there:
  `o0_ab.sh`'s `x86_64` key used `$BUILD/mcc`, the build's own compiler, so on a
  non-x86_64-Linux host it measured the host's native target under the `x86_64` label and
  reported a plausible board of 276 objects for a key it never touched; it now checks the
  `mcc -v` target string and falls back to `mcc-x86_64` plus the usual sysroot
  requirement.)
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

### RIR cut — remaining cleanup
- Deletion residue, keep-or-delete each: `ast_verify_diff` (+ `_match` / `_dump_diff`),
  `ast_treechk` / `MCC_AST_TREECHK`, `ast_jit_guard_env` (declared, never assigned),
  `ast_rir_arena` (dead local), `tools/tracediff.sh`, the recorder half of
  `gate-ledger.sh`.
- **P6** — split the monolithic `src/mccast.c` (~17k lines) and rename `ast_*` → `ir_*`.
  Precondition still false: the `ir_` namespace already collides (~850 occurrences) and
  `targetgate` still whitelists `mccast.c`.
- Land the held `fix-imaginary` branch (its old blockers — `verify-baseline/`,
  `verify_ratchet.cmake` — are confirmed gone from the tree).

### Open codegen / front-end defects
- `__bf16`: finish encode/decode + ABI now that `VT_BTYPE` is 5 bits. **Do not alias
  `__bf16` onto `_Float16`** — distinct `c.i` storage, `is_float_abi`, libgcc name.
- 32-byte vectors are laid at 16-byte alignment (`MCC_MAX_ALIGN` cap) — open ABI
  decision; cross-TU to gcc is currently incompatible (struct-ABI, not SysV vector).
- `aligned(N)` bitfields: ~139 survivors.
- `expr_type()`'s unconditional `nocode_wanted++` evaluates VLA `sizeof`/`typeof`
  operands — violates C99 6.5.3.4p2.
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

### Intermittent / to-confirm
- `selfhost-fixpoint-memmodel-{O3,Os}` SIGSEGV'd once under heavy parallel load and
  never reproduced. Unresolved — may be the orchestration hazard (relinking `mcc` under
  a running `ctest` yields phantom regressions; give parallel agents isolated
  worktrees). See the archive's parallel-agents note.
- Reconcile the deliberate-red count: an older status snapshot says 7 `run-tier` TLS
  reds; the 2026-08-05 notes say only the 2 PE reds remain. **Do not "fix" the
  deliberate reds or weaken the corpus** — validate the true count against the tree; any
  *extra* failure is a regression.

### External suites
- The harness (`tools/xsuite.py`, `xsuite-report.py`, `selfhost-o3.py`,
  `selfhost-fixpoint.py`) exists; the boards need re-quoting at HEAD. It *understates*
  mcc — `KEEP_OPT_RE` drops `-Werror` / `-idirafter` / `${srcdir}`; fix that before a
  board is quoted again.
