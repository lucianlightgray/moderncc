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

**Before the default is flipped, one backstop is still owed.** `ast_func_end` computes
`keep = faithful || (ast_rir_nofb_env && ast_replay_completed)`, and `ast_replay_completed`
is set after the *first* replay and never cleared by the `posterr` arm that catches a
longjmp out of the optimizer emit. That is exactly how the `transparent_union` defect
shipped a body truncated at 52 of 92 bytes instead of falling back, and the comment above
the line already states the intent it violates: a body that longjmp'd out must never be
kept. It was deliberately left open while that defect was live, because adding it would
have hidden the defect rather than fixed it. With the defect closed, the reason to leave it
is gone and the reason to add it is not: with the gate off, any future longjmp after a
completed first replay ships a truncated function silently.

`inline=1, inline-functions=0` is worth naming as a state rather than a flag: `inline`
defaults off at `-O1` and on at `-O3`, `inline-functions` off at `-O1` and on at `-O2`, so
that state is reached by `-finline` at `-O1` and by `-fno-inline-functions` at `-O3` and by
**no single flip at `-O2`**. A sweep at one `-O` level calls two of those three green,
which is why `flagsweep.sh` now runs `-O1 -O2 -O3` rather than `-O2` alone.

## Branches deleted 2026-08-06 — recoverable by SHA

All non-`main` branches and every agent worktree were removed. The commits still exist in
the object store; `git checkout <sha>` or `git cherry-pick <sha>` recovers them until the
next `git gc` prunes unreachable objects, so recover anything wanted here sooner rather
than later.

**Re-audited 2026-08-06 — the "seven commits not on `main` in any form" reading was wrong
for the table below.** Every row but `a3c51e8d` had already been cherry-picked onto `main`
under a different SHA before the branches were deleted. Compared with context lines
stripped, the added/removed lines of each orphan and its twin are identical, so there is
nothing to land from them. The orphan SHAs are kept here only so a future reader who finds
one referenced elsewhere can resolve it.

| orphan sha | already on `main` as | subject |
| --- | --- | --- |
| `a3c51e8d` | **nothing — genuinely unlanded** | `fix(front-end)`: accept the imaginary suffix in either order and fold it — **this is the `fix-imaginary` branch the "RIR cut" section below still asks to land** |
| `a4217c24` | `5d52753d` | `fix(inline)`: gnu89 extern-inline redefinition, in every gnu89 mode |
| `c3ed8b2a` | `6cbbbc65` | `fix(gen)`: give the saved VLA parameter dimension tokens a single owner |
| `2bcd21d9` `b06dcf9d` | `1b78d132` `5a2f8970` | `fix(bitfields)`: width-64 bitfields marked `VT_BITFIELD` in packed contexts, plus its TODO update |
| `1e10dd1a` `22575f40` `6a6fe8f2` | `a170a134` `1df8f3b8` `97164575` | `feat(lex)`/`feat(pp)`: C23 `u8` character constants, `__has_attribute` as a builtin macro, invalid `##` paste is an error |

`b06dcf9d`'s content survived the TODO/ARCHIVED split rather than being lost: `2bcd21d9`
is named in `ARCHIVED.md`'s "Landed today" line, the `aligned(N)`-on-a-bitfield-member
residue it identified is archived open item 3, and its one-line form is the
"`aligned(N)` bitfields: ~139 survivors" entry under open codegen defects below. It does
not need re-applying.

Both fixes were re-verified live in the tree on 2026-08-06 rather than trusted from the
commit messages: the width-64 bitfield layout matches gcc 15 and clang 22 at `-O0..-O3`
for `-run` and compile+link, on x86_64, and on i386 and arm under qemu where it also
matches clang's `-fdump-record-layouts` for the arm triple; the gnu89 extern-inline
redefinition matches gcc across all fifteen `-std=`/`-f` modes at four opt levels. With
each fix reverted, both suite goldens fail.

From this session, researched but never landed — these have no twin on `main`:

| sha | subject |
| --- | --- |
| `ea67df7d` | byte-level RIR coverage census, gap enumeration and a coverage ratchet, with per-class reproducers under `tests/rir/gap/` |
| `d2e4c162` | strategy-registry TDD: all 18 rows in isolation and all 4,896 ordered triples, with a harness proven to catch a planted `ast_cse_kill` bug |
| `934b692e` | the region-granularity research that closed F3 as not viable, and its boundary-state inventory |

The first two are finished, gated work — they add ctest coverage that does not exist on
`main` today. Landing them is a cherry-pick plus a gate run, not a rewrite.

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

- **`run-tier/{x86_64,i386}-win32` are flaky under load, not level-dependent.** They came
  up red in the `-O2` sweep and green at the other three; re-run in isolation they pass
  6/6 at both `-O2` and `-O3`. It is the 32-way-parallel wine cells losing a race, not a
  codegen difference — do not chase it as an optimizer bug, and do not read a single
  full-suite run of these two as signal.

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
- Re-bank `o0-baseline` at HEAD on an x86_64 Linux host. **Not a macOS item — a Mac
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
- `selfhost-fixpoint-memmodel-{O3,Os}` SIGSEGV'd once under heavy parallel load and
  never reproduced. Unresolved — may be the orchestration hazard (relinking `mcc` under
  a running `ctest` yields phantom regressions; give parallel agents isolated
  worktrees). See the archive's parallel-agents note.
- ~~Reconcile the deliberate-red count.~~ **Settled 2026-08-05 by running them.** It was
  7; then **2**, both PE (`run-tier/{x86_64-win32,i386-win32}`, `tls` and `tls_threads`
  each); it is now **0** — the PE `-run` TLS defect is fixed (see the fix write-up above),
  so all four cells pass and `KNOWN_RED` is empty. x86_64-native, i386, arm and riscv64
  were closed earlier by the archive's `-run` TLS fixes. Any deliberate-red `-run` TLS
  cell reappearing is a regression. Two *further* cells became red only because the qemu
  sysroots now exist:
  `selfhost-qemu-{i386,arm}-O2`, which die on `MCC_MAX_ALIGN` — see below.

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
