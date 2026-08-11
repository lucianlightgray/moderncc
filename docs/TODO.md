# TODO

> History, landed-fix write-ups, superseded boards, and the "landmine" warnings that
> used to live here were moved to [`docs/ARCHIVED.md`](ARCHIVED.md) on 2026-08-05 for
> later validation. This file now holds only the configuration instructions below and
> present-tense, open items. File:line anchors are omitted on purpose — the archived
> ones had drifted 1000–1900 lines after merges; find code by symbol.
>
> **Second sweep, 2026-08-10.** Every named subject in this file was indexed and
> validated against the tree, and the finished work was moved to
> [`docs/ARCHIVED.md`](ARCHIVED.md): 85 whole sections (all 49 `Landed —` write-ups plus
> the dated 2026-08-06…08 measurement boards) and 22 finished subsections split out of
> sections that are still live. 21,255 lines → ~6,300. What the validation established,
> and what it means for reading the rest:
>
> - **All 84 `wt/*` branches are ancestors of `main`.** Every branch this file claims
>   landed, did. No section marked CLOSED or LANDED was found whose implementing code is
>   absent — that direction is clean, so the archived write-ups can be trusted.
> - **The reverse direction is not clean.** Three things this file presented as open are
>   done (the board's rank 1 and rank 6, and the item-7 `slice/src` addendum); each is now
>   struck through with the commit that closed it. Three more read as closed and are not —
>   see *Three items that read as closed and are not*, below.
> - **Numbered rows were deliberately not split out of their tables.** Rows are
>   cross-referenced by number across the docs ("board row 3", "A row 13"), so closed rows
>   stay in place, struck through, rather than moving.
> - **The former PLAN and DEVICE-LIBC documents were retired** into
>   [`docs/ARCHIVED.md`](ARCHIVED.md) in full on the same sweep. The cluster tokens this
>   file still cites by name — `L1`–`L8`, `L2′`, `S7b`, `D2b`, `D4b`, `I2`, `N10`/`N11` —
>   are defined there. `tools/docref-lint.py` was updated to match; anchors into the
>   retired files were rewritten to their `docs/ARCHIVED.md` line numbers.
> - **Factual corrections are marked inline** with the date and the code citation that
>   settles them. The largest is the `-O` ladder description under *The configuration
>   surface moved*, which was wrong on all three of its clauses.
>
> **Compiler wave, 2026-08-11.** Cross-vendor conformance went 485 → **493 of 493
> (100.00%)** on both the CPU baseline and the GPU ladder. Six defects fixed, one language
> feature added, four folds, five smoke fixtures. Summarised at the top of *STATE OF PLAY*;
> the detail sections are dated 2026-08-11 and sit inside it. **The board below was not
> otherwise reworked** — an open row is still open unless it says otherwise, and N1 is
> still open.
>
> **Second wave, 2026-08-11 afternoon — JIT/AOT differential and the search surrogate.**
> Five commits, `0dd6ea55..04f12187`. The embed-JIT surface was run against gcc,
> llvm-project and llvm-test-suite at every level it boots, which found a JIT-only
> miscompile and left three more open (**N8**); a degree-2 surrogate was fitted over the
> `-O13` gate cube, measured, and left off; and the smoke engine arm went six engines to
> nine. Summarised under *The second 2026-08-11 wave* in *STATE OF PLAY*. The new open
> items are **N8** (the three surviving JIT miscompiles) and **N9** (`-fno-opt-search-*`
> is a kill switch, not a sub-knob); **N5** went from two green-by-omission hazards to four.

## STATE OF PLAY — written for a context switch, 2026-08-11

> Read this first. It is a handoff, not a board. Everything below it is detail.
> It supersedes the 2026-08-09 handoff, which is wrong in five places, each named below.
> The 2026-08-10 handoff below it still stands; the 2026-08-11 wave is summarised next
> and did not invalidate any of it.

### The 2026-08-11 wave, in one place

**Cross-vendor conformance is at 493 of 493 (100.00%), on the CPU baseline and the GPU
ladder alike, with zero configuration divergence.** It was 485 when the wave opened.
Read the percentage with its denominator: 493 of the 800 sampled programs are
cross-adjudicable at all, and the other 306 are vendor-exclusive by construction, not by
omission. `jit/xoracle-coverage` prints both and refuses a collapsed denominator.

| landed this wave | where |
| --- | --- |
| `_Complex` GNU `?:` — two defects, one of them a compiler crash | `src/mccgen.c`, `src/mccast.c` |
| `__attribute__((ms_struct))` / `gcc_struct`, three zero-width-bitfield rules | `src/mccgen.c` |
| MS layout ignored `packed` — exposed by the above, not caused by it | `src/mccgen.c` |
| `fabs(x) < 0.0` → 0, and a literal condition deletes its dead arm | `src/mccast.c` |
| tautological `\|\|` / contradictory `&&` over one operand | `src/mccast.c` |
| zero-extend, signedness and offset seen through in relational operands | `src/mccast.c` |
| the strategy floor is enforced, not merely printed (`--min-strats`) | `tools/smokerun.c` |
| five new smoke fixtures, all oracle-adjudicated at every level and under replay | `tests/smoke/` |
| smoke compares six evaluation engines row by row, and the AST evaluator is measured at all for the first time | `tools/smokerun.c`, `cmake/smoke_engines_mutate.cmake` |

**Two results that matter more than the number.** One is that a fold which looked correct
shipped a miscompile and had to be reverted — see *The tautology fold, and the flag that
made it miscompile*, whose one-paragraph lesson is worth more than the fold. The other is
that `build2.c` was never a defect: it sat in the failure column for two rounds as a
timeout while printing output byte-identical to gcc's. That one was fixed in the harness,
and the 5x runtime gap it exposed is a real and unbanked performance finding.

**Nothing on the open board below was closed by this wave** except where a row says so.
N1 in particular is still open and acquired a concrete cost: two new folds had to avoid
their natural homes (`abs`, `range`) because those are write-only strategies.

### The unrecorded wave, 2026-08-11 — three shipped-level miscompiles, written up here late

`88b74abf..4e62dae8` landed between the two waves this file does describe, and **nothing in
this file recorded it** until the 2026-08-11 validation sweep found the gap. Three miscompiles
that affected shipped optimization levels, all three caught by the smoke oracles, all three
with fixtures. Written up now because the *lessons* are the part that was lost:

| commit | the defect | level |
| --- | --- | --- |
| `6772f701` | narrow-elim dropped the conversion and kept the wide type | `-O4` |
| `e9aa3783` | a statically-false `if` arm was deleted **with the `case` labels inside it**; also, XMM6–XMM15 were promoted unsaved on Win64 | `-O1` |
| `931f3137` | `ast_dep_direction` derived `<`/`=`/`>` in **index** space and handed it to callers reading **iteration** order — inverted for every down-counting loop, and fusion *and* interchange both believed it | `-O1`+ |

**`931f3137` is the one to read.** For an ascending loop index order and iteration order
coincide, so the bug is invisible until a loop counts down; then interchange turns a `(<,>)`
dependence into `(>,<)` and runs it backwards, and fusion hoists a read above the write it
depends on. gcc-15 and clang agree with each other and with the fixed compiler on both
reducers. It also closed three latent traps in the same two functions: a single `int64_t`
shared between two `ast_loop_iv` calls, so the outer loop's stride was overwritten by the
inner's before anything read it; both return values discarded, so an uninitialised stride was
used when a loop had no induction variable; and fusion never refused a pair with differing
strides, which the shared-variable bug had made unobservable. **A guard that cannot be
observed to be missing is the recurring shape on this board** — it is N1, N7 and this, three
times in one week.

Consequence for the counts above: *"six defects fixed … four folds, five smoke fixtures"* in
the file header **undercounts**. The tree carries seven `pass-*` fixtures from that day and
`--min-passes 150`.

### The second 2026-08-11 wave — the JIT/AOT differential and the search surrogate

Five commits, `0dd6ea55..04f12187`, on two subjects that turned out to be independent.

| landed | where |
| --- | --- |
| a parameter's array bound was evaluated outside the AST, so **every re-baked function dropped its side effects** | `src/mccgen.c`, `tests/jit/parity/vla_param_effect.c` |
| the `-O0`–`-O4` embed-JIT ladder over three external corpora, 6,623 programs per level | `tools/jitconform.py` (driver already existed; this is the measurement) |
| a degree-2 ANOVA/Walsh surrogate over the gate cube, exact int64, proved test-first | `src/mccsurro.h` |
| the surrogate wired into the `-O13` search behind `-fopt-search-predict`, **default off** | `src/mccast.c`, `src/mcc.c` |
| the smoke engine arm drives the JIT at every level it boots, not only `-O4` | `tools/smokerun.c` |

**The JIT half. The differential is the whole finding, and the fix fell out of it.** Running
the *same binary* under `MCC_JIT=1` and `MCC_JIT=0` at five levels separates three suspects
that no single-level run can: a divergence that is level-independent is not the optimizer,
and one that reverses between the two runs of one binary is not the AOT compiler. It
reported **five `JIT_MISCOMPILE` rows at every level**; `970217-1.c` reduced to
`static int bump(int i, int a[i++]) { return i; }` — AOT, gcc and clang all 11, JIT 10.
`func_vla_arg()` ran *before* `ast_func_begin()`, which is what opens the arena the AST
records into, so the bound's increment was emitted into the object and never into the AST.
Moving one call fixed it without moving an instruction. **Three rows survive and are
N8.**

**The search half, and it is a negative result banked on purpose.** The `-O13` pre-pass
already scores every single-bit toggle of the incumbent and then throws the numbers away.
`src/mccsurro.h` keeps them and fits the degree-≤2 expansion; `-fopt-search-predict` spends
that fit on four predicted masks. **It does not pay for itself** — over fifteen torture
programs with a fresh `XDG_CACHE_HOME` per run, cost 1065495942 on against 1065496244 off,
**+1.8% evaluations**, 1 better / 0 worse / 14 identical, and the direct diagnostic is
**0 of 200 predicted candidates improved on the incumbent**. Left in, off, with the flag
and the env so the measurement reproduces. Two things learned are worth more than the
feature and are recorded under *The `-O13` tier is dark on 13 of 22 strategies*: the
interactions are real and the first selection rule could not see them, and `combo_run`
already reaches those optima anyway.

**What this wave says about the board.** Nothing below was closed by it except where a row
says so. It added **N8** and **N9**, took **N5** from two green-by-omission hazards to four,
and it is the first
evidence for the general shape N1 and N7 both circle — *a mechanism nobody differentially
compares is a mechanism nobody has tested*. The embedded JIT had been covered by a compile
and a grep until 2026-08-11; two commits later it had a real miscompile to its name.

### The validation sweep, 2026-08-11 — what this file got wrong about itself

Every statement in this file was checked against the tree and against the commit messages
since `2993f8fb`, in four passes: named artifacts (symbols, paths, cells, flags, `file:line`
citations), open-vs-closed against git, internal contradictions, and what is history. The
headline is not any single row below — it is that **three of the errors were introduced by
the two most recent documentation commits, including one item that was ranked on the open
board hours after being invented.** This file's failure mode is not decay, it is confident
addition.

**Corrected in place by this sweep** (each verified against the tree, not against prose):

| was | is | how it was settled |
| --- | --- | --- |
| `-fno-opt-search-*` is a kill switch (**N9**, ranked) | **refuted** — `set_flag` matches by exact `strcmp`; baseline and `-fno-opt-search-fullset` both give 101 evaluations | ran it; read the parser |
| "13 of **21** strategies", "`-O0` 21 dark" (5 places) | **22** | `MCCSTATS_STRAT_N 22`, and the name table lists 22 (`bfold`…`cload`) |
| "**114** `-f` flags" | **115** | 115 distinct names extracted from `MCC_OPT_ROW(` in `src/mccopt.h`, no duplicates |
| "`wt/o4fold` folded `-O5`–`-O12` into `-O4`" | **hard error**; `MCC_DEV=1` makes them compile | ran `-O5/-O8/-O12/-O14`, and again under `MCC_DEV=1` |
| "9538 cells", "9151 cells" (3 places) | **9545** in `cmake-def` | `ctest -N` |
| "~85 s" / "~115 s" for `^smoke/` | **70.7 s**, 11/11 green | ran it |
| "the two 2026-08-11 waves added no cells" | **wrong**, ≥4 added | `ctest -N -R`; `CMakeLists.txt` diff |
| N8's "not duplicated there" | it is duplicated | read both copies |
| "`--min-engines 5` … 5 is deliberate" | retracted 45 lines above it | same section |

**Two live defects the sweep found that are not documentation problems at all:**

1. **`flagsweep/cover3-verify` is RED on `main`** — `69296b85` added a flag row and left the
   committed covering array at `flags=114`. Written up under *The configuration surface
   moved*. A wave landed a red cell and no one noticed for a day.
2. **Three shipped-level miscompile fixes are absent from this file entirely.** `6772f701`
   (narrow-elim dropped the conversion, `-O4`), `e9aa3783` (a dead `if`-arm took its `case`
   labels with it, `-O1`, plus XMM6–XMM15 promoted unsaved on Win64) and `931f3137` (a
   down-counting loop reversed its dependence directions and *both* fusion and interchange
   believed them). `grep` for the first two hashes returns nothing. They sit between
   `88b74abf` and `0dd6ea55` — **an entire unrecorded wave between the two that are
   recorded** — which also makes the 2026-08-11 header's "six defects, four folds, five smoke
   fixtures" an undercount against the seven `pass-*` fixtures and `--min-passes 150` the tree
   actually carries. This is the most important gap in the file: the *fixes* are in the tree
   and the *lessons* were never written down.

> **`docs/refs`'s `--min-refs` floor was lowered 600 → 440 on 2026-08-11, and that is a
> weakening — read the reason before raising it back or trusting it.** The floor exists so a
> linter that resolves *nothing* cannot render identically to a clean tree, and it **fired**
> during the archive migration: moving ~2,900 lines to `docs/ARCHIVED.md` took the resolvable
> citation count from 760 to **469**, because the citations went with the prose. `ARCHIVED.md`
> is out of the linter's scope by design, so those citations are now checked by nothing. Two
> **The floor is now the binding constraint on further archiving: the count is 456 against a
> floor of 440, so the next section moved will breach it.** Migration stopped there on
> purpose. What is still queued to move — the four 2026-08-08 sections, the 2026-08-07
> GPU-execution study and the struck half of *Present-tense open items* — is verified and
> listed in the commit history, but moving it requires deciding the floor question first.
> Two honest consequences of the change already made: the floor is a *coverage* guard whose
> subject legitimately shrank, and
> the count of symbol-at-a-location claims over `TODO.md` fell **68 → 23** for the same
> reason. If TODO.md shrinks much further the floor stops being meaningful and the real fix is
> to bring `ARCHIVED.md` into scope, not to keep lowering the number.

**RESOLVED 2026-08-11 — 241 rotted anchors stripped, and the check that depended on them was
preserved rather than sacrificed.** Line anchors on source citations went **443 → 108**. The
naive version of this cleanup is a net loss, and the measurement is why: `docref-lint`'s
*symbol* rule only fires on a symbol quoted **beside a location carrying a line anchor** — its
own docstring says the anchor "is what separates a claim from its negation", because this file
is full of true sentences of the form "*NAME* has zero hits in *some source file*", where
naming the file is not a claim that the symbol is in it. Stripping every
anchor takes symbol-at-a-location claims from **68 to 54**: fourteen working checks switched
off silently, which is the same failure this file keeps cataloguing. Re-running the strip with
the lint's own `SITE` regex as the protection rule holds it at **68 — unchanged — while still
removing 241 anchors**. The rule going forward: **an anchor that a cell verifies may stay; an
anchor nothing verifies is deleted, and the symbol name carries the citation.** The
description of the problem is kept below because it recurs.


The header says anchors were dropped on purpose in the 2026-08-05 sweep after drifting
1000–1900 lines. **313 of them are still here**, and of ~200 whose content was checked,
**~95 are wrong** — including 34 of 40 `CMakeLists.txt` anchors (everything below ~line 3300
has shifted), and essentially the whole `src/mccgpu.c` Metal block, which now points into
unrelated code. Two consequences worth acting on rather than lamenting: an anchor that lands
on `int i;` or a bare `}` is *indistinguishable from a correct one* to a reader who does not
open the file, and `docs/refs` (747 citations, green) does not check them — it validates
cross-document references, not `path:line` claims. **Prefer a symbol name; if a line number
is genuinely needed, it needs a cell that fails when it drifts.**

**Nine items were open on this board and are already fixed** — struck through in place below,
each with the commit: the `MCC_AST_EVAL_LADDER_GPU` stack smash (`1f7f6257`), `mcc_gpu.ok`
after `VK_ERROR_DEVICE_LOST` (`747709bc`), `rebuild_arena`'s `sscanf` and pointer interning
(`1b54c26e`), board row 1's missing `indirect` guard (`adf08e3b`, stated closed in two other
places already), `xoracle.py`'s registration (`f797074b`), the `spirv-val`/`glslc` "referenced
nowhere" row (`cmake/spvval.cmake` exists), `opt-cache-determinism`'s manifest row
(`e98fab0a`), `ast_env_gate`'s remaining greppers, and the three cross-vendor stragglers
`build2.c`/`20020720-1.c`/`20041114-1.c` (`92fddfdf`, `bc60a3be`).

**One substantive claim is false about today's tree**, beyond drifted anchors: the Metal
*"the two 'missing' functions are not missing, they are present and inert"* table says
`mcc_gpu_rw_supported`, `mcc_gpu_rw_arm` and `mcc_gpu_mem_backend` are stubs returning 0 on
the MSL arm. **All three are implemented** — `src/mccgpu.c` has `mcc_gpu_rw_supported`
returning **1** on the MSL arm (identical to the Vulkan arm), `mcc_gpu_rw_arm` recording
`mcc_gpu_rw_back = p`, and a real multi-line `mcc_gpu_mem_backend` using `mtl_bind_mem`. A
comment three lines above the first one even says *"which now returns 1"*. The whole table's
premise is stale, and it is load-bearing for the Metal parity estimate.

*(A second candidate was investigated and **rejected**: the sweep flagged
`ast_interchange` / `ast_fusion` / `ast_tile` as naming non-existent passes. They exist —
`ast_interchange_run`, `ast_fusion_run` and `ast_tile_run` are all real arena mutators in
`src/mccast.c`, called from the driver. The file is using the bare stem as shorthand, which is
fine. Recorded here because "symbol not found" from a grep of the exact string is a false
positive generator on this codebase's `_run` convention.)*

### How to validate — standing rule, 2026-08-10

**Validate new code with the smoke/fast tests only, using gcc-15 and clang-22 as the
oracles.** `ctest -R "^smoke/"` is ~15–90 s for 13.0M value cases across `-O0`–`-O4`, plus
the device arm and the divergence arm. Do not run the full suite to validate a change.

**As of 2026-08-11 it is eleven cells, not eight, and ~71 s in `cmake-def` on this host.**
*(Re-measured 2026-08-11 in the validation sweep: `ctest -R "^smoke/"` is **70.7 s**, 11/11
green. The "~85 s" and "~115 s" that stood here before were both wrong. Neither named a
build directory, which is why they were never reconcilable — quote the build dir with the
number or it is not a measurement.)* The three new
ones are the engine-parity arm — see *Smoke now compares six evaluation engines* below. It
adds ~32 s and is worth it: until it existed, `smokerun` set `MCC_FORCE_REPLAY=1` on every
compile it made, so the AST constant evaluator `-O0` actually ships was executed by no cell
in this suite. **The second 2026-08-11 wave added ~29 s to the same three cells** by driving
the JIT at `-O1`, `-O2` and `-O3` as well as `-O4` (`smoke/engines` 8.5 → 18 s,
`engines-identity` 36 → 65 s). No new cells; nine engines inside the existing three.

**As of 2026-08-10 smoke also reaches every optimizer strategy**: the subject fires 22 of 22
at `-O4` (it reached 8 before `tests/smoke/scases.h`), and every one of those shapes is
value-checked against both oracles. Read that as a coverage floor, not as proof each strategy
affects output — see open item N1, where seven of them are write-only.

The reasons are measured, not stylistic. The full suite is **~23 minutes and is not
`-j`-bound** — one cell alone is 1,378 s, so the wall floor is a single cell and buying
cores buys nothing. The top 0.1% of cells is 52% of all time; the median cell is 20 ms.
And breadth was never what caught the defects: of the five real bugs found on 2026-08-10,
**four are caught by nine cells in 0.47 s** and the fifth was caught by nothing at all,
because it was a value shape — an `f64` ternary — that no cell covered.

Consequences that follow, and they are not optional:

- **If a check cannot be expressed in smoke, extend smoke.** Reaching for another cell is
  how the inner loop decayed to 23 minutes in the first place.
- **The `--min-cases` floor is the anti-vacuity guard.** With no broad sweep behind it, a
  suite that silently runs nothing is indistinguishable from a green one — which this tree
  has already shipped nine times.
- **gcc and clang adjudicate wherever they can answer.** Where the result is UB or
  implementation-defined, or where the two references disagree with each other, pin mcc's
  answer as the golden and record the disagreement. Where mcc differs from **both**, that is
  a `diverge-both` row and a defect until proven otherwise — report it loudly, never bank it
  quietly. The `_Float16` intermediate-rounding divergence (item 22) shows both halves of
  the rule working: found exactly this way — gcc and clang agree with each other and mcc
  does not — and then *proven otherwise* by `wt/smokedepth`, which showed it is a
  flag-selectable evaluation format (`gcc -fexcess-precision=16` reproduces mcc bit for
  bit), not a wrong answer. The rows stay banked so the choice cannot change silently.

### Where the tree is

`main` at `3b225e3f`, **9545 cells in `cmake-def` on this host** — counted 2026-08-11 with
`ctest -N`, not added up. It was 9538 at `747709bc`.

> **Corrected 2026-08-11 (validation sweep).** This paragraph said "the two 2026-08-11 waves
> added no cells", which was wrong and contradicted *How to validate* eight lines above it,
> where the same file says the suite went from eight cells to eleven. The waves added at
> least four: `smoke/engines`, `smoke/engines-known-positive` and `smoke/engines-identity`
> (`3b84d616`), and `opt/selftest-surrogate` (`0dd6ea55`, `CMakeLists.txt` +4 lines). What is
> true is the narrower claim it was reaching for: **`tests/jit/parity/vla_param_effect.c` is
> not a cell** — `jit/run-parity-host` and `jit/kgc-route-parity` are each handed the
> `tests/jit/parity` *directory* and walk it, so a file added there widens two existing cells
> and the ctest total does not move. Do not generalise from a walked-directory fixture to the
> wave.

Count cells on the host rather
than adding them up — registration is glob-, loop- and capability-driven (Vulkan +
`spirv-val`, `objdump`), so the total is host-dependent. The pre-merge branch accounting that
used to sit here moved to `docs/ARCHIVED.md`; every branch it tracked has merged.

`-O4` is the consolidation of all 26 validated knobs, `-O5`–`-O12` and `-O14`+ are a hard
error, `-O13` is the search entry. Smoke covers `-O0`–`-O4`.

**The 2026-08-10 wave, nine commits `dd80e4fa..747709bc`, closed five defects and two board
items. Four of the five had a *wrong diagnosis on the board*, so read the landed sections
before trusting any summary of them:**

| what | was on the board as | actually |
| --- | --- | --- |
| item 26 | a stack-slot overlap, triggered by same-named locals | the arena replay handing out a recorded frame slot **too small** for the object going into it. Names irrelevant; the allocator is fine |
| item 25 | an archive member loaded twice | **two runtimes in one link.** `mccrt.o` is pulled once; host `libgcc.a` and `libmccrt.a` define 68 symbols strong in both |
| the ternary fold | not on the board at all | **a live miscompile on `main`** — the two-return `if` fold dropped the per-return conversion, `diverge-both` at `-O2`+ |
| the `-O13` order memo | not on the board at all | 4-bit packing for 22 strategies: six were **silently rewritten** into different strategies, and persisted to disk that way |
| strategy coverage | not on the board at all | smoke reached **8 of 22** strategies. Now 22 of 22 |

Items 4 (TCO through `AST_If` op 5) and 9 (`L2′(ii)`/`(iii)`, the GPU device-lost and quiesce)
are both **landed**. `L2` itself — wiring the device into `mccjit_shutdown()` — is
deliberately not done; its two preconditions are in the GPU landed section, and one of them
is a hazard *this wave created*.

Still open on the audit board: **21** (the `-O13` bail ratchet), **22** (a decision, not a
fix — and see the new finding that mcc is arguably the conforming one), **23** and **24**.

Three machines share this tree: this one (Linux/Vulkan), a Windows box and a Mac box.
Both peers were idle through this wave.

### The single most important finding: the byte gate was hiding two miscompiles

The `faithful` gate is not mis-designed, it was **mis-phased**. An unfaithful body runs zero
of the 22 optimizer strategies and is excluded from the inline pool and JIT dispatch — 66 of
3,040 bodies on `src/mcc.c` at `-O2`, **2.17% of bodies but 7.87% of body bytes**, mean
1,826 B against 474 B, and they are the self-host hot path (`decode`, `next_nomacro`,
`gen_cast`, `preprocess`). 95% of the divergence is *length* divergence.

`docs/ARCHIVED.md` already stated the rule — *"any pass that changes code must be an arena
rewrite in the post-fidelity strategy phase"* — and both normalisations were placed at the
end of `rir_to_arena`, upstream of the fidelity check. `rir_arena_normalise` now runs from
`ast_func_end` after `ast_fn_faithful` is computed and before `ast_slc_dump`/`ast_adump_body`.

**Then making that code actually ship turned two `tests/exec` goldens red** —
`wide_bitfield_arith` and `integer_promotion`, which are *exactly* the two bodies the wide
census reported as "discarded for replaying 40 bytes shorter than the parser". They were
shorter **and wrong**. The earlier claim that every discarded body is still correct was
inferred from `exec/` 347/347, but those cells compile at `-O0`, **where nothing re-emits** —
the discarded code was never run.

Both defects are one mistake: the lowering mirrors `gv()`, but bit-field arithmetic is
decided by `gen_op`, which reads `VT_BITFIELD`/`bs` off the SValue. Narrow fields lost the
integer promotion (`gen_op`'s UAC treat an `unsigned int` bit-field as *signed* unless
`bs == 32`); wide fields (`bs > 32`) cannot be lowered at all, because `gen_op` truncates
**every operation's result** to the field width — a property of the operation, not the
operand — and are now refused.

**So the bar for retiring the byte gate goes up, not down. The discard set demonstrably
contains defects.** Do not replace it with the cref oracle or the effect log: neither can run
where `faithful` runs. The oracle is out-of-process, ctest-only, needs gcc *and* clang, and
its unit is an integer expression subtree — never a function body, no stores, calls, globals
or memory. `ast_eval_slice_effect_bind` has **zero callers in `src/`**.

**The existing safety net is vacuous.** `rir-coverage.py --nofb-probe` reports 0 divergent
bodies at every level because it is wired to `--corpus exec`, and the entire divergent
population is in `src/mcc.c`, which that corpus excludes. "The divergent bodies are benign"
is **unmeasured, not measured-empty**. Re-arming it is ~10 lines and is the next step.

> **Superseded 2026-08-10** by "The benignity probe, re-armed" below. The corpus was the
> smaller of two causes; `read_rows` rejected every divergent row on every corpus. Now
> measured: 62 of 67 keepable bodies benign and 0 miscompiles at `-O2`, 1 miscompile at
> `-O0`, 5 bodies that no switch can keep.

### Threading — decided against the earlier architecture note

> **The 0% below and `wt/threadmap`'s 30.8% are the same finding, not a disagreement.**
> 0% is `P(independent)` over lock sites as they occur; 30.8% is
> `P(independent | the section is call-free)`. `P(elidable) = P(reachable) × P(disjoint |
> reachable)`, and real code's first term is ~0 — every one of the four pairs `wt/threadmap`
> found in three corpora comes back `unknown` blocked by an `AST_Invoke`, **never by
> aliasing**. So the elision is real where it can be reached, and what gates reach is
> inlining and graft, not alias analysis. Full derivation in the `wt/threadmap` section.

Measured: **0% of mutex-protected regions are derivably independent**, structurally rather
than for want of corpus. Over 335 real lock sites in qemu and musl, **19.4% have no lexically
matched unlock at all**, 78.1% contain a call, 21.1% have control flow leaving the region.
Every call-free candidate reaches a field through a pointer, which `ast_dep_decode` marks
`indirect`; `ast_dep_base_distinct` returns 1 only for distinct named global symbols. There
is no points-to set, no read/write-set on any IR object, and `restrict` is parsed and
discarded. The removable class is "one word, one RMW" — 8.1% — and it needs an atomic, not a
disjointness proof.

It would be built blind: 3,662 of the compiled vendor `.c` files contain not one threading
construct, and the two corpora with real concurrency density (musl `src/thread`, qemu) are on
disk and **excluded from every build**.

**S3 (refuse-and-diagnose, landed) now; S2 (create/join only, 1000–1300 lines) only once a
corpus exists; S1 (full takeover, 3000–6000 lines plus renegotiating the effect log's landed
cross-lane invariant) not on this evidence.** For each hard case — blocking on the world,
interleaving that is load-bearing, detached threads outliving `main` — the "serialise it"
option, which is what you get by default if nothing is decided, is the miscompile.
`wt/threadmap-int` gives S2 its target vocabulary and a measured predicate and stops there,
deliberately: **nothing rewrites an `AST_Invoke` of `pthread_create` into a published node,
and its "specified, not built" list stays specified.**

### Traps — the list stands, with three additions and one sharpened

1. **A bucket's name is not evidence of its contents.** Now **eight** confirmed misreads:
   `kind-basicblock`, `arity`, `op-ternary`, `block/capacity`, gap #3's 580, gap #4's 58,328,
   `slice/musl`'s 65-of-74, and `ref-not-local/global-aggregate`'s 3,481 — a class the fix
   it was cited for **cannot** move, because it is a node census asking the expression
   predicate and the node in it is an address. Attribute by the deepest node whose children
   all pass and prove the table is a partition by reproducing the parent count.
   **And check the attribution helper itself**: `refuse_parent_blocked` asked
   `refuse_local`, which refuses every member fold unconditionally, so its `parent-open`
   column could not report the one shape it was being read for.
2. `mcc` derives its include search from `argv[0]` — run both binaries of a byte-identity
   sweep from the same directory.
3. `.gitignore`'s `/vendor/` does not match a *symlink* named `vendor`. Never `git add -A`.
4. **Build `cmake-cross` before configuring `cmake-debug`**, and know which build you are
   measuring from: `cmake-debug` here is `MCC_ENABLE_CROSS=OFF`, so re-banking
   `tests/ast/o0-baseline/` from it would record **eleven of twelve target keys as NOT
   MEASURED** while turning the cells green. Bank from `cmake-cross`.
5. **Never `pkill` ctest.** Stop your own runs with the harness, not the process table.
6. A false generator expression expands to an empty *argument*, not to nothing — nine
   `slice/*` cells once reported `Passed` while executing zero checks. New suites go in
   `slicerun`'s `SUITES[]`.
7. **Device-cell failures usually mean contention.** Confirmed repeatedly this wave at load
   average 180–348: `gpu/ladder-gpu-parity` failed 3 of 10 concurrent and 0 of 7 serial,
   `run-tier/i386-win32` hit *"Maximum number of clients reached"*. Always check whether a
   **non-device** cell is also in the failing set before calling it a regression.
8. **NEW — a clean merge is not a correct merge.** Resolving `ast_eval_slice_globl` outside
   `ast_eval_slice_idx_base` builds, type-checks and passes **every cell**, while silently
   losing 758 indexed loads and 346 indexed stores. The node census is identical either way,
   because it asks the expression predicate. When two branches widen the same predicate,
   measure the fold — do not trust green. **Its mirror image, from `wt/globagg`:** widening
   `ast_eval_slice_globl` to admit a `VT_STRUCT` base — instead of resolving it in an
   address-only helper — also builds, type-checks and passes every cell, moves **62,228
   nodes into `accepted`**, and makes five value contexts read a live-in word at a struct's
   base offset as a value of struct type. The census can be moved by a change that computes
   nothing as easily as it can be left still by a change that computes everything.
9. **NEW — an empty ref list is not "touches no memory", and a predicate that fails open is
   a miscompile generator.** `ast_dep_collect` records only `Load` and `Store`-through-`Load`,
   so `g = g + 1` on a global produces **no ref at all**. `ast_region_disjoint`'s first
   version therefore answered *disjoint* for two critical sections that both increment the
   same global — which, since that verdict is what decides whether a mutex becomes a
   serializing edge or nothing, deletes a lock. Found by its own author before it shipped;
   fixed fail-closed with a kind whitelist plus an unresolved base for any store whose
   destination is neither `Ref` nor `Load`; pinned by `thread-census-control`, whose corpus
   is written in exactly the shape that broke. **Every collector in this tree is a partial
   model of memory. Before a predicate reads "no evidence" as "no conflict", check what its
   collector declines to record — and put a cell on the empty case, not the busy one.**
10. **NEW — the compiler's own runtime headers are part of every corpus.** The two nodes
   the merged thread classifier gained come from `runtime/include/threads.h`, not from any
   corpus source; a name-based census that only greps the corpus measures the files it
   was handed, not the code that compiles.
11. **NEW — a positional replay stream is a correctness contract, and nothing was checking
   it.** `ast_alloc_loc` replays recorded frame offsets by position on the assumption that
   the arena replay makes the same sequence of requests the parser made. It does not — any
   optimizer that folds a call away deletes that call's staging slot — and the entry handed
   back was never checked against the size or alignment of the object about to be stored in
   it. A 16-byte `_Complex double` on an 8-byte `_Complex float` slot overlapped its
   neighbour by a word and computed `x.re + x.im`. **When one pass replays another pass's
   decisions by index, the index is not the contract; record what the entry was *for* and
   check it on the way out.** Its sibling `rir_loc_replay` resyncs by output position and
   still does not check size — see the landed section.
12. **NEW — a fold is not a rewrite unless it preserves what the original spelling
   converted.** `rir_tern_normalise` turned `if (c) return A; return B;` into
   `return c ? A : B;`. Each `return` converts to the return type *independently*; a ternary
   first applies the usual arithmetic conversions *between the arms*. With mixed signedness
   those are different programs, and mcc gave `4294967295` where gcc and clang both gave
   `-1`. The control that makes it unarguable: writing the ternary **by hand** gives
   `4294967295` on all three, because that spelling really does carry the conversion.
   **When a normalisation rewrites one syntactic form into another, enumerate what the
   source form implied and the target form does not.**
13. **NEW — a veto in one pass to protect another pass hides whatever else it was
   catching.** The ternary fold refused `AST_Invoke` arms for no reason except keeping TCO
   firing. That veto was also, accidentally, the only thing stopping the conversion defect
   above from reaching call-valued arms — so removing it turned a hidden miscompile into a
   live one. Both of the fold's refusals are of this shape (the other protects SCCP).
   **A cross-pass veto is load-bearing in ways its comment does not say; measure what it
   blocks before lifting it, and land the fix underneath it first.**


### Residues kept from the archived 2026-08-10/11 landed write-ups

> The write-ups themselves (the five corrections, what the ten branches bought, the
> `wt/globagg` landing, the 2026-08-10 cross-vendor denominator, the 493-test GPU run,
> `ms_struct`, the four folds, 493-of-493, the tautology fold and the six-engines arm) are in
> [`docs/ARCHIVED.md`](ARCHIVED.md). Their lessons are already carried by the traps list and
> *Open, ranked* above. These three were verified open and are restated nowhere else.

**`tools/xoracle.py` advertises `MCC_JIT_GPU`, a knob that exists nowhere else in the tree**

Note the knob the tooling advertised does not exist: `tools/xoracle.py`'s usage string
offers `--mcc-env MCC_JIT_GPU=1`, and **`MCC_JIT_GPU` appears nowhere else in the tree**.
The working spelling is the two `MCC_AST_EVAL_LADDER*` variables above.

**`slice/quiesce` fails intermittently under `-j`, undiagnosed — and the obvious fix is wrong**

**Observed flake, not diagnosed: `slice/quiesce` fails intermittently under `-j`.**
Seen twice on 2026-08-10 — once beside a GPU cross-oracle run saturating the device from
24 threads, and once in a `-j6` ctest sweep with no external GPU consumer. It passes in
isolation every time, passes on `--rerun-failed`, and a targeted `-j8` loop over the four
device cells did **not** reproduce it, so "contention" is a hypothesis and not a
diagnosis. Nothing in that wave touched the device path, so it is not a regression from
it. Recorded rather than fixed, with the caveat that the obvious next step is cheap to
get wrong: the serialisation a contention theory would want does not exist at either
level — `MCC_GPU_LOCK` (`src/mccgpu.c:32`) is a `pthread_mutex`, hence per-process and
unable to order two ctest processes, and **`RESOURCE_LOCK` is used zero times in
`CMakeLists.txt`**, so no device cell excludes any other. Before adding one, get a
reproducer: a `RESOURCE_LOCK` that makes an unexplained flake disappear buys silence, not
a fix.

**~~`--min-engines 5` against nine engines~~ — CLOSED 2026-08-11**

**The floor now counts only engines that cannot skip on their own.** `smokerun` tracks
required and optional engines separately (`optional` was a declared-but-never-read field on
the engine table; only `e->gpu` drove the skip, which is why the floor could not tell them
apart), the floors are `--min-engines 8` against the 8 non-device engines, and the summary
line reports `required N of 8, optional N`. **A bare `--min-engines 8` over the total would
not have worked, and the known-positive proves it**: `smoke/engines-identity` gained a fourth
arm that drops one non-device engine with the new `--engines-drop`, and the run reports
`ran=8 of 9 (required 7 of 8, optional 1)` — 8 total, which a floor over the total accepts,
and 7 required, which this floor refuses. Original text:

**~~The `--min-engines` floor did not move with it, and that is now a live gap.~~**
`cmake/smoke_engines_mutate.cmake` and `CMakeLists.txt` still pass `--min-engines 5` (4 on
the third arm) against nine registered engines, eight of which need no device. The floor was
sized when 5-of-6 was "everything but the device"; at 5-of-9 **three engines can stop running
and the cell stays green**, which is the exact green-by-omission shape the arm exists to
refuse. Raising it needs the same care the 5 was chosen with — the device engine skips on its
own — so the honest form is a floor on the non-device engines plus the device counted
separately, not a bare `--min-engines 8`.


### Three items that read as closed and are not — checked 2026-08-10

Written down because a reader sweeping for finished work will mis-file all three. Each
had a *neighbouring* half land, which is what makes them look done.

1. **`tls_threads` under `MCC_JIT=1`.** All three cited lines are unchanged:
   `src/objfmt/mccelf.c` still gates on `s1->run_tls_active`, which is set only at
   `src/mccrun.c`. Nothing about this was fixed.
2. **The Vulkan use-after-free (`E2`).** Commit `747709bc` fixed the device-lost and
   quiesce halves — `mcc_gpu.ok = 0` at `src/mccgpu.c:1810`, the fault injector at
   `:1799`, `vkDeviceWaitIdle` gone from the quiesce at `:2677-2691` — and it is easy to
   read that as closing the row. It does not: **`grep submitted src/mccgpu.c` is 0**, so
   the `VK_TIMEOUT`-destroys-a-still-pending-command-buffer path has no `submitted` flag
   and no destroy-nothing label. That half is untouched.
3. **The emitter caps (`F7`).** `MCC_GPU_CODE_MAX` *was* raised (`src/mccgpu.h:44,48`),
   which reads as done. But the item's own point was that the **constant cache** binds
   first, and `SPV_MAX_CONST` (`src/mccgpu.h:1319`) and `MSL_MAX_CONST` (`:187`) are both
   still **512**.

### The audit residue this handoff does not mention — read before planning a wave

The board section below carries roughly **thirty FILED items** that this handoff never
names, and they are all still true of the tree. The shape they share is that a
measurement tool reports success over an empty or truncated subject:

- **Four tools that publish figures on this board are registered nowhere** —
  `xsuite-report.py`, `gate-ledger.sh`, `strategy-ledger.sh`, `c2_sweep.sh` have zero
  hits in `CMakeLists.txt` and `cmake/`.
- **`vendor/plb` does not exist**, so `runtime-bench.py` filters its corpus by
  `os.path.exists` and silently measures nothing; `tests/must-run.txt` records
  the resulting skip as permanent rather than fixing it. `opt-cache-determinism` is a
  permanent 77 for the same class of reason.
- **`tools/shadow-iv-sweep.sh` documents its own blindness in the source**: *"There is
  still no floor on this count, so a regression that stops 500 of 610 subjects building
  would report divergences=0 and PASS"*.
- ~~The `22 of 22` strategy coverage quoted under *How to validate* is **measured, not
  enforced**: `tools/smokerun.c` has no strategy-fire assertion at all, only the
  `--min-cases`/`--min-passes` floors, so a silent drop back to 8 firing strategies
  would keep the suite green.~~ **CLOSED 2026-08-11.** `smokerun --min-strats 22` now
  fails the run, fed by a `[strategy]` record `src/mccstats.c` emits and a census pass
  that refuses to floor nothing. `smoke/strats-known-positive` proves the floor reads the
  table it floors — 22 accepted, 23 refused, and the refusal must name `--min-strats`.
  Closing it also fixed the reason it had never been possible: `mcc_stats_env_init()` was
  linked into `mcc` and **never called**, so `MCC_STATS` was reachable only from the baked
  JIT engine and the STRATEGY category could not be turned on for an ordinary compile at
  all. Still read the count as a coverage floor, exactly as N1 says — it proves each
  strategy *fires*, not that each affects output.
- **`src/wide256_slice.h`'s I-6 is a live compiler segfault with a two-line fix and
  nothing watching it**: `src/mccrir.c` still tests only `if (!sv->sym …)`,
  with no `sv->r != VT_CMP` check, and `gv` still leaves `sym` stale. It is filed at the
  bottom of the 256-bit section, which is where it will be missed.

### Open, ranked

> **New this wave, and the first three outrank everything below them.** Numbering continues
> from the existing list rather than renumbering it; N1–N6 are the 2026-08-10 additions,
> N7 came with the engine-parity arm on 2026-08-11, and **N8–N9 with the JIT/AOT differential
> the same afternoon**. Order here is rank, not number — N8 and N9 sit above N6 because one
> is a wrong answer with a six-line reproducer and the other invalidates any measurement that
> used it.

**N1. Seven of 22 strategies are write-only.** `LTEMP, IVSR, PRE, RANGE, ABS, REASSOC,
INLINE` mutate the arena but their fire count never reaches the `do_*` disjunction that
triggers the re-emit, so their work is discarded unless something else fires. Measured:
`reassoc` alone on a leaf function at `-O4 -fno-promote-locals` is byte-identical while the
rewrite demonstrably happened. **This is a prerequisite for any mix-and-match work** and it
means the new 22-of-22 smoke coverage is a floor, not proof those seven affect output. The
fix is to let `ast_run_strat_cycle`'s return value drive the disjunction. See the research
section.

**N2. `rir_tvar_replay` and `rir_slot_replay` repeat the bug `dd80e4fa` fixed, unchecked.**
`rir_tvar_replay` is the *first* statement of `get_temp_local_var`, so it bypasses the
`size >= && align >=` invariant the same function enforces sixteen lines later, on the same
object class, in a function that already has the sizes. Neither was on the board.
`rir_loc_replay` is the same shape but measured clean (0 undersized over 620 smoke firings
and 1,013 torture firings) — that probe was reverted, not banked, so nothing pins it.

**N3. Item 24 is the real conformance defect of the 22/23/24 trio, and 23/24 share one
predicate.** Both are `t != VT_INT` in `gen_cvt_ftoi` on opposite arms; 24 is in-range, not
UB, and also hits `(short)`; 23 has no non-UB reproducer on x86-64. arm64/riscv64 are already
correct and are the reference. 24 needs an x87 `fistpl` sequence, **not** a wider convert —
the naive shared fix regresses a case where mcc currently matches both references. Item 22
may be no defect at all: mcc predefines `__FLT_EVAL_METHOD__ 0` and its per-operation
rounding is what that macro promises.

**N4. `-O13` is dark on 13 of 22 strategies**, at the one level with no bail ratchet (item
21). ~2.7 s, bit-deterministic, already digest-clean. `smokerun --max-level 13` is a silent
no-op. Bank *dark* strategies rather than fire counts and the existing `ratchet()` needs no
change. See the research section for the two traps.

**~~N5. Four green-by-omission hazards.~~ ALL FOUR CLOSED 2026-08-11.** ~~The device arm
computes two refusal categories and never calls `ratchet()` (`bails.txt` holds no `dev `
rows).~~ **The device arm now owns the `dev-` scope and ratchets it. The gap was bigger than
the two categories this row named: `device_probe` also ran the ladder census
(`scan_ladder(txt, 9)`) and threw away **531 refusals** — `no-static-type` 515,
`all-undefined` 15, `unsupported-op` 1 — all now banked. It needed a scope of its own first,
because `scope_match` matches on the text after the first space, so the obvious `own("slice-")`
would have matched the CPU arm's `O0`–`O4` rows and reported every one of them as an
IMPROVED-to-zero. The ladder census now takes a tag (`""` for the CPU arm, so its category
names are byte-identical, and `dev-` for the device), and the two `device-refused:*`
categories were renamed `dev-refused:*` so one scope owns all five.** Twelve `optfire/*.txt` tables drive
ctest registration by row count ~~with no `list(LENGTH)` guard — delete a row and the cell
silently stops existing~~ **— CLOSED 2026-08-11: both glob-driven loops now count the rows
they register and hard-error below a floor (52 for `differs*`/`cdelta*`, 125 for the
`counters*`/`levels*`/`defstate*` set). Proven by deleting one row and reading back
`registered 51 rows, below the floor of 52`. A ratchet, not an equality: adding rows is fine
and the floor is meant to be raised with them.** The durable shape is re-deriving a bank's expected row set from
`src/mccopt.h` and failing on `missing-row`, which is what the four banks that resisted
shrinkage do.

Two more, both found 2026-08-11 in the second wave:

- ~~**`jit/xoracle-conformance` drops its second corpus silently.**~~ **CLOSED 2026-08-11 —
  the arm has an `else()` that names the missing path, says the run is c-torture only, and
  says `jit/xoracle-coverage` will not reach `--min-cross 400` on one suite. Verified by
  configuring with a bogus `MCC_XSUITE_LLVMTS` and reading the message back.** The CMake arm at
  `CMakeLists.txt` adds `--testsuite`/`--suite ts-unittests` only under
  `if(EXISTS ${MCC_XSUITE_LLVMTS}/SingleSource/UnitTests)` and has **no `else()`** — no skip,
  no message. `--limit` is per-suite, so losing the arm halves the corpus, and the companion
  `jit/xoracle-coverage` then fails `--min-cross 400` against a single-suite denominator that
  tops out at 379. On this host the cache pointed at a path that does not exist, so the cell
  was **red for a configuration reason it reported as a coverage reason** — the worse failure,
  because it sends the reader to the wrong file. Repointed locally; the missing `else()` is
  the defect. Detail in the residues section at the end of this file.
- ~~**`--min-engines 5` against nine engines.**~~ **CLOSED 2026-08-11 — the floor counts
  required (non-device) engines and a known-positive drops one to prove it.** See *Smoke now compares six evaluation engines*
  above: the floor was sized for 5-of-6 and the arm is now 9, so three engines can go dark
  green. Same family, and it is the arm's own floor that has the hole.

**N7. The slice evaluator's arithmetic is unobservable: a wrong answer 66 million times per
compile changes nothing.** Found on 2026-08-11 while trying to demonstrate that the new
engine-parity arm has teeth, and it is the arm's own limit as much as the ladder's.
`ast_eval_binop` in `src/ast_eval_slice.h` is reached **66,436,580 times** compiling
`tests/smoke/subject.c` at `-O4 -DMCC_AST_EVAL_LADDER=1` (counted with an `fprintf` probe at
its head). Injecting `r = s + 1` into its 32-bit signed `+` arm and rebuilding leaves *all*
of the following byte-identical: `[ladder-self] pairs=624 certified=532 differ=0 refused=92
exact=86`, the whole `[ladder-cross]` line set, all 1782 rows of `--dump` on every one of
the six engines, the sweep digest, and `ctest -R "^smoke/"` — eleven cells, still green.
Only `[ladder-cross] points` moves, by 194 out of 13.6M, and nothing asserts on it. The
unsigned `+` arm behaves the same way.

Two distinct causes, and they need separating before either is fixed. **First**, `differ`
compares two arenas that are both evaluated by `ast_eval_binop`, so a shared fault cancels
by construction — self-comparison cannot detect it, and this is the "one engine compared
with itself" shape the engine arm exists to refuse, one level below where the arm looks.
**Second**, the injection did not even change *which* 532 pairs certified, so the
certifications this evaluator produces are not reaching codegen on this subject; if they
were, the engine arm's row comparison would have caught it, because that comparison is
against the AST evaluator and is genuinely independent. The fix for the first is an
independent oracle for the tree side. The fix for the second is a subject that makes a
certified equivalence change the emitted code — which is the same question N1 asks about
the seven write-only strategies, and is probably the same answer.

**N8. Three JIT-only miscompiles survive the `-O0`–`-O4` embed-JIT ladder.** New 2026-08-11,
and they outrank most of the numbered list below because each is a wrong answer with a
reproducer, not a coverage gap. Five `JIT_MISCOMPILE` rows at every level before `85bf6a3d`,
three after; each survivor **aborts under `MCC_JIT=1` and exits 0 under `MCC_JIT=0` from the
same binary** (rules the AOT compiler out) and is **level-independent** (rules the optimizer
out). The lead is `gcc.dg/torture/pr45830.c`, **reduced to six lines**:

    int bar(int x){ if (x==5 || x==19 || x==23 | x==26 || x==65) return 1; return 3; }

over `x` in `[0,70)`. gcc-15, clang and mcc's AOT path answer 1 at `x==23`; the JIT answers
3 — it drops the **left operand of the bitwise `|`** where that `|` sits between two
comparisons inside a `||` chain. AOT is correct at `-O0`–`-O4` **and `-O13`**, so no search
gate is involved, and **`MCC_JIT_LAZY=1` makes it correct**, so the defect is on the eager
install path only. Start at `815d2001` (the `||`/`&&` operand folds), `bc60a3be` (relational
see-through), and at whatever the eager path does that the lazy path does not. The other two
are `gcc.dg/pr96674.c` (`-fwrapv`) and `gcc.dg/fastmath-1.c` (`-ffast-math`), neither reduced.
Behind them, unexamined: ~80 `differ` and ~150 `refused` rows per level, where `refused` is a
front-end gap list and not a JIT one. Full detail in the residues section at the end of this
file.

**~~N9. `-fno-opt-search-<anything>` disables the whole search, not the sub-knob it names.~~
REFUTED 2026-08-11, the same day it was filed. There is no prefix match and no kill switch.**
The claim came from `69296b85`'s commit message, was promoted to this board in `3b225e3f`,
and does not survive contact with either the binary or the parser:

- **The parser cannot do what the item says.** `set_flag` (`src/libmcc.c`) matches with
  `strcmp(r, p->name)` — exact string equality against the generated table, with no prefix
  logic anywhere in the loop. A flag named `opt-search-fullset` cannot reach the row named
  `opt-search`.
- **It does not reproduce.** On `tests/smoke/subject.c` at `-O13`, baseline and
  `-fno-opt-search-fullset` both report **101 candidate evaluations** — identical, not zero.
  (Independently re-run on a second build with a third subject: 808/8383/303 evaluations,
  unchanged by all three sub-knobs.)
- **The likely origin, reproduced.** `0 candidate evaluations` is simply what `-O13` prints
  when the subject is too small to search: a two-line `int f(int,int)` reports **0 at
  baseline**, with no flag at all. Measure a sub-knob on a subject like that and the flag
  gets the credit for a zero it did not cause.

**What is left of it, and it is smaller and unranked:** whether the three sub-knobs do
anything at all in the *negative* direction is still unestablished — the counts are identical
with and without them, which is equally consistent with "inert". `-fopt-search-predict`
positively does work (it reports `predicted N candidate(s)`). So the open question is
"are the `no-` forms inert?", not "are they a kill switch", and nothing measured so far is
invalidated. **The lesson is the durable part: this item was filed from a commit message and
ranked without anyone re-running it, and it took one `strcmp` and one A/B to fall over.**

**N6. `L2` — wire the device into `mccjit_shutdown()`.** Unblocked as of 2026-08-10, with two
preconditions in the GPU landed section. One is a hazard *this wave created*: the quiesce now
unmaps the shared address space, so nothing may retain a `mcc_gpu_mem()` pointer across
shutdown. That hazard did not exist while the quiesce destroyed nothing.

1. ~~Re-arm the benignity probe~~ **DONE, 2026-08-10** — see the section below. The
   answer exists: at `-O1`/`-O2`/`-O3` every keepable divergent body is benign; at `-O0`
   one is a miscompile that segfaults the compiler.
2. ~~**`ast_eval_slice_globl`'s `VT_STRUCT` rejection**~~ **DONE, 2026-08-10** (`wt/globagg`),
   and the 3,481 was never the size of it — see the landed section.
3. ~~Re-take the two owed deltas~~ **DONE, 2026-08-10** — see the section below.
   `cleanup_symbols`'s `-O0` arena is the first named defect in the discard set that the
   byte gate is catching; finding out why it is wrong is the new open item here.
4. ~~`ast_tco_run` cannot see through an `AST_If` op 5, so the tail-recursive two-exit `if`
   stays out of reach and the ternary fold refuses that shape to keep TCO firing.~~
   **DONE, 2026-08-10** — and it could not land alone: the fold's `AST_Invoke` veto was
   hiding a live miscompile, so the return-conversion fix had to land first. See both landed
   sections.
5. `SR_GLOB_MAX` is 4,096 in `slicerun` with no reset between arenas and no message — the
   same silent cap `ast_adump_intern` just made loud. Fails safe; a coverage cliff.
   **Measured, and it is not binding today**: raising it to 65,536 leaves the whole gcc
   torture census byte-identical, so fewer than 4,096 distinct global symbols reach
   `slicerun_reloc` over all 15,923 bodies. Its sibling `SR_GLOB_STRIDE` (4 KiB per symbol,
   with no check that an object fits) is the one that would fail *wrong* rather than safe —
   a field or element past 4 KiB lands in the next symbol's key range and two distinct
   locations collapse to one live-in slot, which both executors and the oracle would then
   share and agree on. Raising the stride to 64 KiB is also byte-identical over gcc torture,
   so that hazard is unexercised there; it is not proven absent elsewhere, and the honest
   fix is a per-symbol range sized from the dumped extent rather than a fixed stride.
6. **Stop pricing global-data work on gcc c-torture.** `wt/globagg` measured −405 refused
   blocks there of which **384 are four macro-generated files**, and +39,203 indexed loads
   of which **39,092 are fourteen memory-op files**. The same fix on `src/*.c` is +2.82 pp of
   accepted nodes against +0.067 pp on torture. Every remaining gap in this area needs a
   hand-written denominator — `src/`, musl, qemu — before it is ranked.
7. ~~The `rir-coverage` lowerable bank was **already stale before this wave**: 0.0414pp of the
   0.0671pp drift accumulated over the 34 commits before it, inside the 0.05pp tolerance.
   Dilution by less-lowerable new code is normal and will recur; the metric needs either a
   corpus-normalised form or a scheduled re-take.~~ **CLOSED, 2026-08-10** — the corpus-
   normalised form landed after the mechanism recurred the same day. See the section below.
7. ~~**The funnel's largest named drop, `nslot < 1` at 352 blocks / 1.05%.**~~ **CLOSED,
   2026-08-10 (`wt/noslot`)** — and it was 242 / 0.70% once re-taken on merged `main`, not
   352. All 242 are `return <constant>`; **zero** are an unrecognised live-in and **zero**
   are a slot-model casualty. Nothing to fix, and the reason is now pinned by
   `slice/noslot-classes`. See the landed section below. **New and outranking most of this
   list: `src/mccast.c` gives 23 expression-slice mismatches and 1 frame mismatch against
   the device on unmodified `main`**, all on the `f64` ternary, and no committed cell runs
   `slicerun` over `src/` arenas so nothing catches it.
8. **The JIT's 47.1% `NOT_BAKED` is not the callee refusal — 3110 of 3118 programs never
   reach a bake site.** `wt/bakewiden` closed the refusal half (−140 refused sites, −38
   programs, 0 new DIFFER) and proved the rest is upstream in `ast_func_end`. Attributing
   those 3110 across `rir_try_active`, `ast_replay_ok`, `faithful && !ast_fn_hole` and
   `ast_jit_want` is the measurement that ranks every remaining JIT-coverage item.
   **Sharpened 2026-08-10**: the funnel is *six* predicates, not four — the plan omits
   `!ast_func_has_labeladdr` (a term of `ast_opt_ok`) and the `embed_jit || OUTPUT_MEMORY`
   plus `!ast_jit_slot_taken` gate. **The 47.1% is not computed in the compiler at all**;
   `NOT_BAKED` is assigned in `tools/jitconform.py` and there is no bake counter in `mcc`.
   Build one modelled on `rir_prod_why_name[]` — named reason, parallel count *and bytes*,
   `atexit` report — and split `ast_opt_ok` and `ast_jit_want` into their sub-terms or the
   largest bucket will be uninterpretable. Predicates 1, 3 and 5 have no signal today, which
   is why the measurement cannot be taken. Do **not** reuse `MCC_JIT_BAKE_WHY` (it already
   means per-site free text) and do not gate it with `ast_env_int`, which returns the default
   for any value ≤ 0.
9. ~~**Cluster L's next link is `L2′(ii)`/`(iii)`, both in `src/mccgpu.c`** — clear
   `mcc_gpu.ok` on `VK_ERROR_DEVICE_LOST` and give the Vulkan quiesce something to destroy.~~
   **DONE, 2026-08-10** — see the landed section. `L2` is unblocked on the hang axis, with
   two named preconditions.
   The pool half landed on `wt/jitshutdown`; wiring the device into `mccjit_shutdown()`
   before (ii) is fixed makes an unbounded `vkDeviceWaitIdle` reachable from `atexit` on
   every run.
10. **Inlining should consult a semantic-equivalence cache over a rolling window, not
    re-derive every graft.** Today a callee is grafted or not on a per-body yes/no
    (`ast_inline_graftable`, `mcc_slice_leaf_hook`, `keep_inline = ast_fn_faithful &&
    ast_inline_retain(...)`), and every optimization of the grafted body is recomputed from
    scratch at each site. The proposal: slide a **rolling window** across the arena, and for
    each window look up a cache of already-optimized slices keyed on a **semantic**
    signature; on a hit, reuse the optimized form instead of re-deriving it. That makes
    inlining incremental rather than all-or-nothing — a callee too large to graft whole can
    still contribute the windows that match — and it turns repeated shapes across a TU into
    one optimization each.

    What already exists to build on, and what each one teaches:
    - **A memo keyed by intention already exists.** `ast_search_key_salt(ast_intention_hash(...))`
      backs `mcc-search.memo`, and `wt/o4ticks` had to salt it with the twelve `so_axes[]`
      names because an entry created under one axis configuration was being reused under
      another. **A cache key that omits part of the context it was derived under is a
      miscompile generator**; that lesson transfers directly and is the first hazard here.
    - **`opt-slice` is the cautionary version of this idea.** It memoised *other knobs' gate
      bits* across processes via `$XDG_CACHE_HOME/mcc/sl-<salt>.ck`, changed **zero objects
      across 1,937 programs**, cost **33.7% of `-O12` compile time**, and made output
      irreproducible (`tools/opt-cache-determinism.py` FAILs at `-O9`/`-O12`). It is gated
      behind `MCC_DEV`. Any new cache has to beat that bar on all three axes: measurable
      win, bounded cost, and byte-reproducible output.
    - **The key must be semantic, not byte-identity.** That is the whole lesson of the
      `faithful` gate: byte-identity fails exactly when a normalisation is doing its job,
      and 2.17% of bodies / 7.87% of body bytes at `-O2` run zero optimizer strategies
      because of it. Two windows that compute the same function must hit the same entry
      even when their bytes differ.
    - **Equivalence has an adjudicator already.** The four `slice/cref-oracle-*` cells
      re-spell a slice as standalone C and require gcc **and** clang to agree with the CPU
      reference over real argument tuples. It is the right instrument to *validate* the
      key's equivalence classes offline, though it cannot run inline (out-of-process,
      ctest-only, and its unit is an integer expression subtree — no stores, calls, globals
      or memory).
    - **The window has a natural unit.** `ast_eval_slice`'s frame/expression slices are
      already a windowing of the arena with a live-in model and a device differential;
      `mcc_slice_frame_from_ast` and `ast_eval_slice_livein_ext` are where a window's
      boundary and its inputs are already computed.

    Sequence it as measurement first: how many windows in a real TU are semantically
    equivalent to another, and how much optimizer work is duplicated across them? `src/*.c`
    is the right denominator, not gcc c-torture (item 6). If the duplication is small, the
    cache is `opt-slice` again.

## Research — five horizontal slices over the open board, 2026-08-10

Five parallel read-only sweeps, one per cross-cutting layer, run to find where one
implementation serves several open items. Everything below is **unbuilt** unless it names a
commit. Claims I re-verified myself are marked; the rest are the sweep's, and a sweep that
reasons from source alone has been wrong on this board before.

### The single largest finding: 7 of 22 strategies are write-only

`ast_run_strat_cycle` returns a per-strategy fire array, but `src/mccast.c` unpacks it into
`do_*` flags for only **15** indices. Never read: **`LTEMP`, `IVSR`, `PRE`, `RANGE`, `ABS`,
`REASSOC`, `INLINE`**. That `do_*` disjunction is the *sole* trigger for the re-emit that
turns a mutated arena into bytes — so a strategy in the unread set mutates `ast_cur` and, if
nothing else fires, **the mutation is discarded**.

Measured by the sweep: `reassoc` alone, on a leaf function, at `-O4 -fno-promote-locals`,
produces byte-identical output while `-fdump-replay` shows the rewrite did happen. With
promotion on (the `-O4` default) it does land — but only because `do_promote`, computed
independently, forces the re-emit. **Six knobs are riding on someone else's re-emit**, and
every ROI number the search computes for them is measuring a benefit the compiler then throws
away.

This is the prerequisite for any mix-and-match work: until the cycle's return value drives the
re-emit, an A/B over strategy subsets measures "did some *other* strategy fire", not the
subset. It also means **the 22-of-22 fire counts now in smoke are a coverage floor, not proof
those seven strategies affect output** — they fire, and on the default ladder they land, but
the mechanism that makes them land is not their own.

### Couplings that make the registry less of a registry than it looks

- **`LICM` is a sensor, not a transform.** `ast_strat_licm` ignores both arguments and returns
  a global that **`ast_cse_run` zeroes**. Scheduling LICM before CSE reports a stale count;
  scheduling LTEMP before CSE has its contribution wiped before LICM can report it.
- **A strategy's precondition lives in five unlinked places** — the `sg_*` thunk, the
  `ast_*_env` global it reads, a **35**-term OR in `ast_search_gates_now` (re-counted
  2026-08-10; this said 36), the matching assignment
  in `ast_search_gates_set`, and `so_axes[]` in `src/mcc.c` (12 entries, which does not even
  match `ast_search_axis_env[]`'s 13). Adding a strategy means editing five tables correctly.
- **The ROI model assumes independence by construction** — it clones the pristine arena once
  per strategy and scores each alone, so composition effects (reassoc → bfold, ltemp → cse)
  are structurally invisible.
- **Two inline paths are mutually exclusive by hand**: `AST_STRAT_INLINE` is gated on
  `ast_inline_pass_env` while the `do_inline` path requires `!ast_inline_pass_env`. One of the
  22 slots is dead whenever the other is live.
- **`opt-slice` changed zero objects for a mechanical reason.** The window seam
  (`ast_slice_enum` + `AstSliceVisitFn`) has **three** registered visitors — re-counted
  2026-08-10; this said two — and **all are pure
  observers**; `ast_slice_enum` takes a `const AstArena *`, so there is no visitor that
  rewrites anything, and the single call in the compiler
  discards its result. It was a scan with disk persistence and no apply — which is also why it
  cost 33.7% of `-O12` and made output irreproducible.

### The record/replay family has nine consumers, not one

`dd80e4fa` fixed `ast_alloc_loc`. The same shape appears eight more times, ranked by how much
the consumer checks:

- **Full validation** (the two to copy): `mcc_effect_record`/`_replay`, and
  `ast_inline_cap_off[]` → `mcc_slice_leaf_scan_rec`, whose comment says it *replaced* a
  positional assumption. `ast_fconst_reuse` and `rir_hook_fconst_reuse` key on a `memcmp` and
  fail closed.
- **No validation**: `rir_loc_replay`, `rir_slot_replay`, `rir_tvar_replay`, `ir_cap_fconst_take`,
  `ir_cap_pred`, `cst_inc_tmpl`.

**`rir_tvar_replay` is the worst and is not on the board.** It is the *first* statement of
`get_temp_local_var`, so it bypasses the `size >= && align >=` invariant that the same
function enforces sixteen lines later — the exact bug `dd80e4fa` fixed, on the same object
class, in the function that already knows the sizes. `rir_slot_replay` likewise discards the
`size`/`align` its caller computed.

**One inferred hazard I measured and did not reproduce:** `ast_reemit` sets `ast_locrec_i = 0`
but never `ast_locrec_n`, which is zeroed only in `ast_func_begin` — so in principle a re-emit
replays the *last-parsed function's* record. I stamped the record with its owner and counted:
**278 takes across mcc's three largest TUs at `-O3`, plus the whole torture corpus, zero
foreign**. Real in shape, absent in fact, unpinned.

The proposed unification is one entry type carrying `{kind, size, align, pos, nc, owner}` and
one `mcc_rec_take` that skips by fit, resyncs by position where the stream wants it, and fails
closed — with `owner` closing the `ast_reemit` hazard for about three lines. `SR_GLOB_STRIDE`
is **not** served by it and should not be forced in: that one is an address-space partition,
and it is the only member of this family that can fail *wrong* rather than *safe*, because
both the executor and the C oracle decode into the same wrong symbol and agree.

### Items 23 and 24 are two defects sharing one predicate

Both are `t != VT_INT` in `gen_cvt_ftoi` (`src/arch/x86_64/x86_64-gen.c`) standing in for a
two-dimensional decision — destination width × signedness — and they bite on **opposite
arms**:

- **23** bites where `t != VT_INT` and spares `(int)`: the emitted convert is 64-bit and
  nothing narrows it. `(unsigned long long)(unsigned)d` for `d = 1e300` gives `2^63` against
  `0` from both references. **No non-UB reproducer exists on x86-64** — the 64-bit convert is
  exact whenever the value fits — so this is quality-of-implementation.
- **24** bites where `t == VT_INT` and spares `(unsigned)`: `long double` is narrowed through
  `double` by an `fstpl` before truncation, so `1 − 2⁻⁶⁴` rounds to `1.0`. **Not UB, in-range
  operand, and it also hits `(short)`** — a datum the board did not have, since `gen_cast`
  rewrites every sub-`int` destination to `VT_INT`. gcc emits a real 80-bit truncating
  `fistpl` sequence instead.

**arm64 and riscv64 are already correct** and are the reference implementation — they decode
width and signedness independently and dispatch `long double` to a 32-bit-destination helper.
This is an x86_64-only pair. Naively applying one fix to both regresses a currently-passing
case: converting `long double` at 64-bit then narrowing turns `(int)(long double)1e300` from
`-2147483648`, where mcc currently matches **both** references, into `0`. 24 needs `fistpl`,
not a wider convert — and note that adding a `__fixxfsi`-style helper would collide with the
libgcc overlap in item 25's territory. Emit the instructions, do not add the helper.

**Item 22 may not be a defect at all.** mcc predefines `__FLT_EVAL_METHOD__ 0`, and per C23
`0` means "evaluate to the range and precision of the type" — so mcc's per-operation `_Float16`
rounding matches its own advertised macro and gcc's default does not match gcc's. The one line
that implements the policy is the `gen_cast_s(VT_FLOAT16)` round-back in `gen_op`; but deleting
it alone is wrong, because the line above has already set the expression's *type* to `float`,
which `_Generic`/`sizeof`/`typeof` would see. An honest `=fast` needs an excess-precision bit
on `SValue` that does not exist.

### The `-O13` tier is dark on 13 of 22 strategies

Measured per level on the smoke subject: `-O0` 22 dark (no strategy runs at `-O0` at all),
`-O1` 12, `-O2`/`-O3` 9, `-O4` **1** (before the `bfold` shape landed; now 0 of 22), and
**`-O13` 13** — `ivsr, narrow, bfold, range, bf, divmagic, pre, ltemp, inline, abs, tco, licm,
reassoc`. That is the state the whole subject was in before `scases.h`, at the one level
nothing watches.

Every objection to watching it is gone: an `-O13` compile of the subject is **~2.7 s**, it is
**bit-deterministic** (882 TSV rows and 112 `fallback` rows across search budgets of 100, 1000
and 5000 ms and three repeats), and it already agrees with `-O0`–`-O4` on the value digest with
`failures=0`. **`smokerun --max-level 13` is currently a silent no-op**: `smk_maxlevel()` walks
`MCC_OPT_LIST` and returns 4, because 13 is `MCC_OPT_SEARCH_LEVEL`, not an `MCC_OPT_ROW`, and
`main` clamps to it.

**The ratchet wants inverted polarity.** A fire count must not fall, which is the wrong
direction for a monotone-decreasing bank. Bank the strategies that are **dark** instead: a
strategy going dark becomes a *new category* (hard fail), one lighting up prints `IMPROVED`,
and `ratchet()`, `bank_load`, `bank_write` and the file format need **no change at all**.
`--stats=4` costs nothing measurable, does not change codegen, and its text is already slurped
by the level pass. Two traps for whoever builds it: at `-O13` the panel prints once per search
phase and **the last one reads all zeros**, so the census must take the per-column max; and
`cload` was invisible until `52d8b66b` widened `MCCSTATS_STRAT_N`.

**The search is not limited by where it looks — measured 2026-08-11, do not re-derive it.**
A degree-2 surrogate over the gate cube (`src/mccsurro.h`, and `-fopt-search-predict` /
`MCC_SEARCH_PREDICT`, default off) was built, wired in and measured: **0 of 200 predicted
candidates improved on the incumbent**, +1.8% evaluations, 1 better / 0 worse / 14 identical
over fifteen torture programs with a fresh `XDG_CACHE_HOME` per run. Three things it
established, in the order they matter:

- **`combo_run` already finds the interactions a better proposal distribution would aim at.**
  Exhaustive subsets over an improvement-sorted item list, 64 candidates deep, reaches those
  optima without being told where they are. So a smarter *proposal* has nothing to win here;
  the untested question is whether it buys a smaller *budget* — hold total evaluations fixed
  and cut `AST_SEARCH_CAND_MAX` in exchange. That is the experiment, and it was not run.
- **The interactions are real; the first selection rule could not see them.** Ranking
  candidate pairs by lowest score picks the gates that did nothing — most single toggles score
  exactly at the incumbent, so "best" is a field of ties — and every interaction measured 0.
  Ranking by |effect| instead (sparsity-of-effects) shows `max|g|` up to 344064 against
  `max|d|` of 2293759: roughly **15% of the main-effect scale sits in pairwise terms**.
  **A predictor that reports "no signal" may be reporting on its own sampling.**
- **The single-toggle scores the `nitems > 6` pre-pass computes are still thrown away**
  apart from sorting the item list. That is a first-order model, measured and discarded, and
  it remains the cheapest unspent information in the search.

Read this beside N9: the same work found that `-fno-opt-search-*` is a kill switch, so any
earlier verdict on a search sub-knob obtained with one of those flags is void.

### Two more green-by-omission hazards

- ~~**The device arm computes a census and never ratchets it.**~~ **CLOSED 2026-08-11 — `own("dev-")` + `ratchet()`, 531 previously-discarded refusals now banked.** `device_probe` calls `cat_add`
  for `dev device-refused:unavailable` and `:no-dispatch`, `main` calls `bank_load`, but the
  `do_dev` branch never calls `ratchet()` and `bails.txt` holds no `dev ` rows. Both categories
  are unmeasured; the arm could start refusing every dispatch and stay green.
- ~~**Row-count-driven ctest registration.**~~ **CLOSED 2026-08-11 — row-count ratchets on
  both loops, verified by deletion.** Twelve `tests/optfire/*.txt` tables are read with
  `file(STRINGS …)` and turned into one cell per row, with no `list(LENGTH)` guard. Delete a
  row and the cell is never registered — ctest reports the same `N/N` with a smaller `N`.

**The generalisable rule**, and it is the one `bails.txt` lacked: the tree has **eleven**
`--min-*` floors (re-counted 2026-08-10; this said eight — they are `--min-adjudicated`,
`--min-cases`, `--min-cells`, `--min-chains`, `--min-cref`, `--min-depth`, `--min-nodes`,
`--min-pass`, `--min-passes`, `--min-refs`, `--min-slices`) and **every one floors a number
produced at runtime; not one floors the row set of a committed bank**. The re-derivation
shape that resists shrinkage — re-deriving the expected row set from `src/mccopt.h` and
failing on `missing-row` — is narrower than this claimed: `missing-row` appears in
`tools/optlevel-bench.py` only, not in four banks. That, not a `--min-rows N`, is the
durable shape.

## Open — the level differential, and the six wrong answers it surfaces

`optlevel/torture-differential` compiles and runs all 1693 `gcc.c-torture/execute`
programs at `-O0` to fix a reference (exit code + sha256 of stdout), drops the 85 the
reference sweep cannot use, and repeats at `-O1`–`-O4`. **~1 s per level at `--jobs 32`,
3.7 s for the cell** since `wt/o4fold` made `-O5`–`-O12` a **hard error** *(corrected
2026-08-11: this said "folded into `-O4`", which is not what the tree does — `mcc -O8` exits
non-zero with "reaches no optimization this build can enable", and only `MCC_DEV=1` compiles
it)*; it was 25 s over
thirteen levels. `-O13` is excluded on purpose: the search rung makes that one level a
sweep of its own. Against the two fixes reverted it reports 14 unknown divergences and
nothing else, which is the whole of what it was built to catch.

Note what did *not* exist before it: `slice/cref-oracle-gcc-c-torture-execute` consumes
the same corpus, but as a GPU conformance oracle against clang. Nothing compared mcc at
`-On` against mcc at `-O0`.

`tests/optfire/leveldiff-known.txt` started at fourteen rows and is now **twelve**: the
two computed-`goto` rows went stale and were dropped, and every surviving row's range
reads `1-4` rather than `1-12`. Five are `link_error()`/`link_failure()`
missed-optimization markers and are not wrong answers. **The other six are, and they are
the open item:**

| program | first level | how it fails |
| --- | --- | --- |
| `990208-1.c` | `-O1` | SIGABRT; address of a label inside a `static inline` |
| `builtin-constant.c` | `-O1` | SIGABRT; `__builtin_constant_p` answers 1 where it must answer 0 |
| `printf-chk-1.c` | `-O1` | SIGABRT in `__printf_chk`, and the `-O0` stdout is lost with it |
| `fprintf-chk-1.c` | `-O1` | same family |
| `vprintf-chk-1.c` | `-O1` | same family |
| `vfprintf-chk-1.c` | `-O1` | same family |

All six are at `-O1`, which ships. The two computed-`goto` rows that used to sit here
(`920302-1.c`, `comp-goto-1.c`, both first failing at what was then `-O6`) are **gone** —
they stopped diverging, the cell said so, and they were dropped at the `leveldiff` commit
before consolidation folded that band away. `return-addr.c` is the twelfth row and is not
a defect: it prints the addresses of its own locals, so its stdout can never agree across
levels.

A row that stops diverging fails the cell as loudly as a new divergence, so the table
cannot rot into lost coverage.

## Total lowering — the decided architecture, 2026-08-09

> **Bank note, 2026-08-09.** `tests/fmt/census-bank.json` was re-taken when the
> five-branch GPU merge landed. `fmt/census-bank` failed first and named every figure that
> moved, which is what it is for. Cause: new diagnostics, not new formatting work —
> `src/mccgpu.c` 30 → 33 printf-family sites (the host-pointer import reporting its
> alignment and imported range) and `src/mccgen.c` 28 → 29 (`-fdepth-census`). One of the
> new sites is an `snprintf`, so literal `snprintf` sites went 162 → 163 and accepted
> 148 → 149. **The ratio is unchanged at 91.4%** and no site changed its acceptance class;
> `blocked-on-pointer` went 109 → 110 with the new site.


> **This section is decisions, not measurements.** Every line below was chosen by the
> project owner in conversation on 2026-08-09 and supersedes any earlier ranking that
> treated device coverage as an incremental menu. Where a decision contradicts a measured
> refusal in a section below, the decision wins and the measurement becomes a work item.

**The correctness criterion is input/output equivalence, not byte-faithfulness.**
A transformed or optimized slice is correct when it produces the same outputs for the same
inputs — established by differential against an external oracle, which is what the four
`slice/cref-oracle-*` cells do by adjudicating slices against gcc and clang over real
argument tuples. `kept` (`tools/rir-coverage.py`) measures byte-identity of the arena
replay against the parser's bytes; that is a *proxy* for semantic equivalence and a
conservative one, since it fails exactly when a normalisation is doing its job. **A
transformation that moves bytes while preserving behaviour is a success.** `kept` remains
worth reporting as information; it is no longer a veto.

**Consequence, open and not yet acted on.** `ast_run_strat_seq` gates every optimizer
strategy on `faithful` — the byte test. Under this criterion that gate asks the wrong
question: a semantically-correct normalisation can silently disable optimizations for the
bodies it touches, not because they are wrong but because their bytes moved. Replacing it
with a semantic gate is now possible — the cref oracle establishes I/O equivalence and the
effect log makes observable behaviour comparable — and is the open item this raises.

**The governing rule.** Every AST/RIR node must lower to the GPU. A slice terminates only
where mcc **cannot see the callee's body** — `dlopen`, an unseen object, or an indirect
call whose target set is not resolvable. The test is **body visibility, not C linkage
class**: `extern int f(void);` whose definition exists anywhere in the program being
linked lowers like any other call. This turns the refusal census from a menu into a bug
list: every refusal that is not a body-invisible call is a defect with a fix owed.

**Recursion is not a terminator.** SPIR-V forbids recursive `OpFunctionCall`, but a
recursion inlined to a finite measured depth is ordinary nested code. The pipeline is:
run the non-linear or recursive body on the **CPU first with iteration and depth
counters** (the `-floop-census` machinery already has this shape); take the observed
maxima as thresholds; build **aggressively inlined slices up to those maxima** as an
optimization strategy; validate for correctness; and promote through the existing
hotpatch benchmark, which already scores a base and candidate wrap with `ast_cost_score`
and asks `ast_slice_promote_static` for KEEP or REJECT before splicing. Instrumentation
yields an *observed* bound and not a proof, so a **bailout guard is mandatory**: a runtime
excursion past the measured maximum falls back to the CPU rather than computing a
truncated answer.

**Inline asm lowers by lifting, not by pattern-matching.** `mccasm.c` already models each
instruction's semantics in order to assemble it; that model is reused to decompile an asm
block into RIR/AST nodes, which then lower through the ordinary path. One semantic model,
no per-idiom table.

**`setjmp`/`longjmp` lowers as a structured state machine** when both halves are inside
the slice: a per-lane status flag plus an outer loop that resumes at the recorded target,
which is expressible in SPIR-V's structured control flow.

**Observable effects run exactly once.** This is the constraint that shapes the rest.
The runtime gate is **always compare** — every dispatch runs the CPU reference and the
device and compares — which is sound for pure computation but would *duplicate* a
`volatile` store, an MMIO write or a timer read. So for effectful slices one side is
designated the executor for that dispatch and the other is **replayed effect-free against
a captured log of what the executor observed**. Every effect therefore needs a
record/replay representation. This is the most demanding decision in this section and it
is what makes a true differential possible for effectful code rather than only for pure
code.

**Globals reach the device as imported host pages.** `VK_EXT_external_memory_host`,
verified supported by lavapipe (`.EXT_external_memory_host = true`,
`minImportedHostPointerAlignment = 4096`, Mesa 26.0.x `lvp_device.c`), so CI on the Linux
runner can exercise it and `MCC_GPU_REQUIRED=ON` stays honest. Since mcc is also the
linker it can page-align `.data`/`.bss` and import the range as one allocation, which
avoids per-object alignment work entirely. A device without the extension must **skip
with a stated reason**, never pass vacuously.

**Dispatch comes from parallel loop iteration spaces** — one lane per iteration, using
`ast_loop_parallel_legal`, which became sound once array-decay `Load`s were typed.

**Inlining is unbounded and measured afterwards**, including any pin or ratchet that
moves.

**The honest consequence, recorded so nobody rediscovers it as a disappointment.** Under
always-compare the device produces **no wall-clock speed-up**, because the CPU reference
runs on every dispatch. What it produces is a second independent implementation checking
the first against real data rather than test data. That is a currency which converts —
the device differential has already caught three miscompiles that internal comparisons
were structurally blind to — whereas device-eligible blocks never had an exchange rate.

## The effect record/replay representation — the constraints it still binds (`wt/effectlog`)

> **Retitled 2026-08-10.** The format itself landed and its measurement narrative is in
> [`docs/ARCHIVED.md`](ARCHIVED.md); `src/mcceffect.h` is 894 lines and bound at
> `src/ast_eval_slice.h`. What is kept here is the part that is **not** done:
> the consumers. `volatile` is still a slice refusal (`src/mccast.c`) and there
> is no asm lifting, so the two subsections below are live constraints on work not yet
> written, not a record of work finished.

> This implements the **Observable effects run exactly once** decision above and nothing
> else. `volatile` lowering and inline-asm lowering both consume this; neither is touched
> here, and the shape this forces on them is written down at the end rather than built.

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *The record*.

### The keying axis: slice identity plus provenance, never input values

**Nothing in this format is keyed on an input value, and nothing in it should become so.**
An entry is identified by `site` — an `AstLocal` in the arena being executed — plus `lane`
and `seq`. That is a property of *code*, and a lowering is correct for all inputs or for
none, so a comparison establishes a property of the slice rather than of the argument
tuple it was run with. Value-keying is the `MCC_JIT_KGC` axis (memoise on known argument
values) and it inherits `KGC`'s defect class directly. The finite thing to cover is the
**slice set**, not the input domain.

So the log does not name its slice, and `MccEffectKey` is what binds it to one:

| field | what it is |
| --- | --- |
| `slice` | the slice identity — `ast_slice_ident_hash`, what `MccSliceKernel` already keys on |
| `off[]`, `nlive` | the live-in vector, which is the slice's calling convention |
| `prov[]` | one provenance class per live-in: `literal`, `computed`, `load`, `param`, `anonymous-call`, `unknown` |
| `nopaque` | **how many live-ins are `anonymous-call` or `unknown`**, maintained as they are added |
| `loghash`, `nrec`, `sealed` | the certificate: the hash and length of the effect log the certified run produced |

**`mcc_effect_traceable` is `nlive > 0 && nopaque == 0` — one integer compare, no walk.**
That is the whole point of `nopaque` existing as a counter rather than being derived: the
validity gate is consulted on every dispatch, so the question *"is this slice's provenance
fully traceable?"* must be O(1) or the gate becomes the bottleneck. Only `anonymous-call`
is untraceable — a pointer manipulated by an anonymously-linked `INVOKE`/`CALL` — which is
the same boundary as the terminator rule, and `nopaque` says *how many* rather than merely
whether, so a refusal can name its cause.

The classifier itself is deliberately not here. The dependency engine follows provenance;
this file is the representation it fills in and the gate reads, in the same shape as
`ast_eval_slice_obj_fn` — a hook the compiler leaves empty because the compiler does not
run slices.

**`MCC_JIT_KGC`'s missing admission test falls out as one expression.** The filed defect is
that side-effecting callees are admitted and memoised. `mcc_effect_memoisable` is
`traceable && sealed && !effectful`, and `effectful` is `nrec != 0` — a **non-empty effect
log is the refusal**. It needs no new machinery beyond binding a recorder around the run
`KGC` already performs. `mcc_effect_key_match` then re-checks a sealed certificate against
a log, so a cache entry cannot outlive the behaviour it certified; the cell proves it
catches both a log that grew an effect and a log perturbed after sealing.

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *The value-dependent residue, which is not this format's job*.

### The shape this forces on the `volatile` and asm consumers

Written down rather than implemented, as the task required:

1. **`site` is an `AstLocal` in the arena being executed.** Both sides must be executing
   the same arena, or the site ids do not name the same access. A device path that lowers
   from a *different* arena (an inlined or specialised copy) must carry a site mapping, not
   a second numbering. This is the one decision here that a consumer cannot work around.
2. **`width` is part of identity.** A `volatile` access must not be split into two narrower
   ones or merged with a neighbour by either side, because the replayed side would then ask
   for an effect the executor never performed. The existing sub-word store in
   `ast_eval_slice_bytes_store` is a read-modify-write of the containing word; for a
   `volatile` byte store that is already wrong on real hardware, and the log now makes it
   *visibly* wrong rather than silently so.
3. **One cursor per lane means cross-lane effect order is not validated.** See above. The
   eligibility predicate for effectful slices owes a refusal.
4. **`flags` is the extension point, not `addr`.** Inline asm needs more request identity
   than `VOLATILE` — a port number, a barrier class, an opcode — and it belongs in `flags`
   and `site`, never encoded into `addr`, which is compared as a region offset.
5. **A new address space costs a `space` tag and a `chan`, not a schema change.** MMIO gets
   `space=port`; a file gets `space=fd, chan=fd`. `addr` is an offset *within* `(space,
   chan)`, so nothing has to be smuggled into it.
6. **A read with a side effect is still an input effect.** A FIFO pop or a clear-on-read
   status word is `kind=load` — its value is supplied on replay, which is correct — but it
   is not undoable, so it must carry `NOUNDO` if a rewind consumer ever returns. It is the
   one shape where "input" and "reversible" come apart.

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *Measured here, and what is unverified*.

## arm64 and Metal — the context, the traps and the unmeasured, 2026-08-09 (`wt/arm64ctx`)

> **This is not a plan and not a spec.** `## Metal parity — the drop is reversed by decision`
> below is the spec; it owns the staging, the line estimates and the parity matrix, and
> nothing here re-prices any of it. This section is the *context* an implementer sitting in
> front of an Apple-silicon Mac would otherwise spend days rediscovering: what the Metal
> driver already is, which properties of this tree will waste their afternoon, which device
> facts we hold were measured on hardware we own and **not** on theirs, and — separately
> from Metal — what arm64 coverage this suite actually executes.
>
> **Every figure below was re-derived on 2026-08-09 against `ef8da0b1`**, with
> `src/mccgpu.h`, `src/mccslice.h` and `src/ast_eval_slice.h` clean at HEAD. Four branches
> were editing those three files concurrently, so treat the line anchors as dated readings
> and confirm by content — §2's first trap is precisely that this has already gone wrong
> three times.


### Residues kept from the archived arm64/Metal subsections

> §§1, 3, 4 and the verification block went to [`docs/ARCHIVED.md`](ARCHIVED.md); their arm sizes and `fprintf` counts were all superseded. These five were verified open and are restated nowhere else. §2 (traps), §5 (arm64 target) and §7 (CI reality) are kept in full below.

**Metal has no bounded fence timeout — a hung kernel hangs `ctest` (§1)**

thing Metal does not: a bounded fence timeout and a stranding protocol. `waitUntilCompleted`
at `src/mccgpu.c` blocks forever with no timeout, so `mcc_gpu_stranded` (`:2277`) is
permanently 0 under Metal — **a hung kernel hangs `ctest`, it does not fail it.**


**the third `MTLBuffer` at index 2 — what is missing is the *binding* (§1)**

So the runtime-side work is not "write three functions"; ~~it is **give three existing stubs a
body, and add the third buffer they need.**~~ **and as of 2026-08-11 it is not that either —
the bodies are written. What is left is the third buffer.** That third `MTLBuffer` is the only genuinely new
object: a persistent shared-storage allocation bound at index 2, whose `contents` pointer
`mcc_gpu_mem_backend` hands back with its length. The mechanism already exists —
`mtl_buffer` (`src/mccgpu.c`) already asks for `contents` at `:592` — what is absent
is *persistence*, because today both buffers are created and released per dispatch where the
Vulkan arm keeps resident ones. That, plus the emitter side the spec section stages, is the
~200 lines.


**division is excluded on both arms and in the CPU reference (§3)**

`spv_logical` first. **Division is excluded on both arms and in the CPU reference** — six
independent sites — because `OpFDiv` is 2.5 ULP by spec and bit-exactness is unattainable on
any conformant device. A software f64 divide would be *more* exact than the certified arm,
which is a differential failure, not a win. `float`/fp32, `long double` and int↔float
conversion in either direction are likewise excluded on both arms.

**three probes only a real Mac can run, and the two UNMEASURED device rows (§4)**

**Three probes worth running on a real Mac, all cheap, and the shape they take.** Because
the driver is `dlopen`-only, a probe needs **no Objective-C compiler and no Xcode project**
— roughly 60 lines of C that copy the plumbing from `src/mccgpu.c` and `:196-253`,
compile one hard-coded MSL string through `newLibraryWithSource:options:error:`, allocate
one shared `MTLBuffer`, dispatch a single lane, and print the result bits as hex. Values
must be read from the input buffer, never written as literals, or the Metal compiler folds
them.

1. **Contraction.** Emit `c = a*b + d` with `a = 1+2⁻²³`, `b = 1−2⁻²³`, `d = −1` in fp32 and
   compare against separate-rounding and fused-rounding host answers. Run it once with
   `setMathMode:` at `MCC_MTL_MATH_SAFE` and once without setting it at all. This settles
   whether the safe-math pin is doing the job the SPIR-V arm needs `NoContraction` for.
2. **Two-NaN payload tie-break, in software.** MSL has no `double`, so probe the *integer*
   layer this actually constrains: implement the certified soft-f64 `+` over `uint2`, feed
   two quiet NaNs with distinct payloads in both operand orders, and print the 64 result
   bits. The answer is a property of the algorithm, not the device — which is the point.
   **The device question that remains is the fp32 one**: two fp32 NaNs with distinct
   payloads through a native `+`, both orders. If Apple silicon takes the second operand
   like the NVIDIA part does, that is a second data point on a divergence nobody had filed;
   if it takes the first, the divergence is per-vendor and the soft-f64 must be written to
   the *host* convention on Darwin and the device convention elsewhere.
3. **fp32 denormal flush, re-confirmed under this driver.** `FLT_MIN*0.5` through the
   dispatch path in `src/mccgpu.c` rather than through a hand-built pipeline. The
   2026-08-08 reading was taken outside this driver; confirming it *inside* it costs one
   dispatch and removes an inherited assumption.

Every one of these is a thing the Mac-side implementer can settle in an afternoon and
nobody else can settle at all.


**the unmeasured / prose-only index (Verification)**

**Marked unmeasured, so it is not inherited as fact:** MSL contraction behaviour under
`setMathMode:`; the fp32 two-NaN payload tie-break on Apple silicon; fp32 denormal flush
observed from inside this driver rather than from outside it; and the Metal shader
toolchain's availability and standard-library naming across macOS versions, which is
external to this tree entirely. **Marked prose-only:** the standalone Vulkan probe that
produced the RTX 5070 Ti readings — the readings are banked, the tool is not in the tree.


### 2. Traps in this tree that will cost you time

**Trap 1 — the `is_float` guards are mirrored, and their line numbers have now been banked
wrong three times running.** `src/mccgpu.h` holds one MSL emitter and one SPIR-V emitter
behind a hard `#if MCC_GPU_LANG_MSL` (`:163`) / `#else` (`:1144`) / `#endif` (`:3121`), so
only one is ever compiled. Twelve `is_float` sites, **six per side**, all inside the single
expression dispatcher on each arm — `msl_expr` (`src/mccgpu.h:764-988`) and `spv_expr`
(`:2771-3091`). **Anyone who counts, edits or greps one emitter has touched half the
problem.**

| refuses | MSL | SPIR-V |
| --- | ---: | ---: |
| float literal | 771 | 2782 |
| float local read (`AST_Ref`) | 782 | 2798 |
| float const ref (`AST_Ref`) | 794 | 2813 |
| float frame load (`AST_Load`) | 807 | 2837 |
| int↔float cast (`AST_Convert`) | 822 | 2877 |
| float binary operator | 890 | 2984 |

Read as of 2026-08-09 against `ef8da0b1`, file clean. **Both sets previously banked in
`## Metal parity` §1 — MSL 781/792/804/817/832/899 and SPIR-V 2789/2805/2820/2844/2884/2990
— are wrong now, and the set banked before *those* was wrong then.** The cause is
mechanical and will happen again: §1's reading was taken at 09:36 on 2026-08-09, and
`6707857a` (the usual-arithmetic-conversions fix, merged as `2fbd830f`) landed at 11:20 the
same day and moved the MSL six down by 9–10 lines and the SPIR-V six down by 6–7. Nothing
in the tree pins these; `docs/refs` only checks that an anchor lands *inside* a file that
exists, which it still does. **Find them with `grep -n is_float src/mccgpu.h`, expect twelve
hits, and never quote a banked number.**

The `AST_Load` pair is the one that does not textually mirror: MSL writes a bail, SPIR-V
writes the same refusal as a positive admission condition. Three of the six pairs are
byte-identical. The SPIR-V side additionally carries `ast_eval_slice_f64t` and
`ast_eval_slice_ftype` — 13 sites, **zero** on the MSL side — because it has a real fp64
path; do not read those as guards you have to mirror.

**Trap 2 — a baseline `mcc` run from the wrong directory fabricates ~40 object diffs at
every optimisation level, `-O0` included.** `mcc_auto_mccdir` (`src/libmcc.c:940-961`)
derives the include search from `host_exe_path` (`src/mcchost.c`), falling back to
`argv[0]`. A baseline binary copied to `/tmp` therefore picks `/usr/include/stdint.h` where
the in-tree one picks `cmake-debug/include/stdint.h`; the anonymous-symbol counter then
differs by one on **every TU that includes a header**, and a byte-comparison sweep reports
~40 differences that are entirely the harness. Two branches hit this independently. Run the
baseline binary from *inside* its build directory. This will bite anyone doing a
"did my emitter change touch codegen?" sweep, which is exactly what Metal work needs.

**Trap 3 — `prec` is a macro with the whole amalgamation as its blast radius.**
`src/mccgen.c` is `#define prec (mcc_state->gen_prec)`, there is no `#undef` anywhere
in `src/`, and `src/libmcc.c` includes `mccgen.c` fourth — *before* `mccast.c`,
`mccgpu.c`, `mccrir.c` and the rest. In the default `MCC_AMALGAMATED` build the macro is
live for every file included after it. No collision exists today, which is exactly why it is
a trap: a local named `prec` in new GPU code compiles fine in a multi-TU build
(`MCC_SINGLE_SOURCE=OFF`) and breaks the default one. `precedence` (`src/mccgen.c`) is
the same shape.

**Trap 4 — build `cmake-cross` before you configure `cmake-debug`.** `mcc_cross_cc`
(`CMakeLists.txt`) falls back to an `EXISTS` test on the cross build directory, which
CMake evaluates at *configure* time. Configuring `cmake-debug` on a tree with no
`cmake-cross` present registers ~164 fewer cells — the `optfire-{arm64,i386,riscv64}` and
`*-docker` families — **and reports no skips**, so the loss is silent. Any cell count taken
from a `cmake-debug` configured first is low and worthless as a baseline. This is hazard 5
in *What is actually still open* below; it is repeated here because a Metal implementer's
first act on a new Mac is a fresh configure.

**Trap 5 — the Metal arm's `objc_msgSend` casting is silently arm64-only.**
`src/mccgpu.c` sends `maxThreadsPerThreadgroup` through plain `objc_msgSend` cast to
return an `MtlSize`, which is three `unsigned long` — 24 bytes. On arm64 that is correct: a
large struct returns indirectly through `x8` and the same entry point handles it. On x86_64
macOS a 24-byte struct return requires `objc_msgSend_stret`, which **does not appear
anywhere in this tree**. There are no `objc_msgSend_fpret` or `_stret` variants resolved at
`src/mccgpu.c` either. The backend has only ever been executed on Apple silicon, so
this has never fired. If anyone tries the Metal arm on an Intel Mac, `maxthreads` is the
first thing that will be garbage — and `mcc_gpu_init` refuses the device on it (`:441-448`),
so the symptom is "no usable Metal device" rather than a crash. **Not a defect to fix
blindly**; it is a documented restriction to make explicit, or a `stret` path to add.

**Trap 6 — the pipeline cache keys on a hash, not on the source.** `mtl_key`
(`src/mccgpu.c`) is FNV-1a over the source bytes and `mtl_pipeline` (`:523-585`)
matches on `(key, len)` only; the text itself is never compared. A collision at equal length
returns the wrong compiled pipeline. The Vulkan cache has the same shape. Worth knowing
before you debug a "kernel produced someone else's answer" report.

### 5. arm64 as a target, separately from Metal

**The short version: on a default configure, arm64 has almost no execution coverage. Nearly
everything green is cross-compile-and-inspect.**

| family | registered | executes arm64 code? |
| --- | --- | --- |
| `macho-structural` (`CMakeLists.txt`) | always | **no** — covers `arm64-osx` but the verdicts are object-structure checks |
| `macho-reloc-arm64` (`CMakeLists.txt`) | `if(UNIX)` | **no** — `clang`, `llvm-objdump`, `llvm-nm`; greps for `PAGE21`/`PAGOF12`/`BR26` |
| `macho-got-sub-arm64` (`CMakeLists.txt`) | `if(UNIX)` | **no** — `llvm-nm`/`llvm-objdump` |
| `macho-embedjit-arm64-osx` (`CMakeLists.txt`) | `if(UNIX)` | would, but its script exits 77 on any non-Darwin host |
| `jit/arm64-{dispatch,counter,kgc,kgcfp}` (`CMakeLists.txt`) | Linux only | yes under qemu — **but the programs are clang-built aarch64 validators with zero `mcc` involvement**. They prove the platform's icache/slot-swap/FP-KGC mechanics, not this compiler's codegen |
| `qemu-arm64-{glibc,musl}[-O2/-O3/-Os]`, `-exec` (`CMakeLists.txt`, `:7632`, `:7650`) | **opt-in only** — `MCC_QEMU_TESTS` defaults OFF at `CMakeLists.txt:7540` | yes, genuinely, plus a stage3 rootfs download |
| `qemu-arm64-osx` (`CMakeLists.txt`) | same opt-in gate | **yes — the single cell that executes mcc's arm64 Mach-O codegen**, ELF-linked against a glibc sysroot and run under `qemu-aarch64` |
| `run-parity-arm64` (`CMakeLists.txt`) | `if(UNIX)` | yes when it gets there — compares `-run` output at `MCC_JIT=0` against `MCC_JIT=1` against a golden. Skips 77 on a non-aarch64 host without both `mcc-arm64` and a vendored sysroot |
| `jit/xoracle-conformance` (`CMakeLists.txt`) | gated `MCC_PYTHON3 AND MCC_EMBED_JIT AND MCC_TARGET_IS_HOST AND UNIX AND (x86_64 OR arm64)` (`:6516`) | **arm64 is in scope** — but the differential runs in-process and cannot be emulated, so it needs a real arm64 host and the gcc torture corpus |
| `arm64-win32` — `cross/no-compiler-abort-arm64-win32` (`:4385`), `ast/rir-parity-arm64-win32[-Ox]`, the `ast/o0-baseline` bank | always | **zero programs, ever.** `run-tier/arm64-win32` is a hardcoded skip stub at `CMakeLists.txt`. Same for `arm-win32` and `arm-wince` |

**`jit/arm64-*` were reporting Passed on every skip path, and their history is therefore not
evidence.** Fixed at `0e5b5cf0`, which needed both halves: the four cells were registered
with no ctest skip property *and* their scripts returned `exit 0` on every bail. Current
state is `SKIP_RETURN_CODE 77` at `CMakeLists.txt` and `exit 77` at
`tests/qemu/jit_arm64_dispatch.sh` and its three siblings. Anyone reading a green
`jit/arm64-kgc` from before that commit is reading nothing.

**One live contradiction worth knowing.** The `run-tier/arm64-win32` skip reason at
`CMakeLists.txt` says no host here can execute an arm64 PE — and
`tools/arm64pe-wine-docker.sh` is a complete, working executor that builds a hello and a
hand-written arm64-PE JIT-dispatch validator with `mcc-arm64-win32` and runs both under wine
in a `linux/arm64` container. It is registered nowhere. That is the single highest-yield
change in this table and it is not Metal work.

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *6. arm64 ABI facts that recently landed and touch this work*.

### 7. CI reality — what you validate on your Mac, CI cannot re-validate

`## Metal parity` §4 owns this argument and prices the alternatives; three things belong
here as context rather than as plan.

**Nothing you measure locally can be reproduced by this project's CI.**
`MTLCreateSystemDefaultDevice` returns nil inside GitHub-hosted macOS runners; there is no
software Metal comparable to lavapipe, and none exists to be installed; and there is **no
offline SPIR-V or MSL validator in this tree at all** — zero hits for `spirv-val`,
SPIRV-Tools, `spirv-as`, `spirv-opt`, `glslang` or `metal-shaderconverter` anywhere in
`CMakeLists.txt`, `cmake/`, `tools/`, `src/` or `.github/`, re-confirmed 2026-08-09. The one
`glslc` reference is `tests/gpu/run.sh`, and that script is wired into no ctest cell and
no workflow. Every `xcrun` hit in the tree is `--show-sdk-path`. So there is not even a
"does it compile" backstop to fall back on.

**The consequence for design: any cell you add must be self-checking.** It cannot rely on a
reviewer's machine, a golden that a validator produced, or a CI run to catch its own decay.
The pattern the existing GPU cells already use is the one to copy: exit 77 for "no device"
with `SKIP_RETURN_CODE 77` and a matching `mcc_skip_test` stub on the dead branch (the three
`gpu/msl-slice-*` registrations at `CMakeLists.txt` are the model, and
`tools/regstub-lint.py` enforces that both branches register the same names); a mutation
mode that must report failure, so a cell that has gone blind is detectable; and a floor on
the row count, so a corpus that silently emptied fails rather than passes.

**One asymmetry to know about before you copy a registration.**
`gpu/spv-slice-differential` (`CMakeLists.txt`) runs its gate directly with only
`SKIP_RETURN_CODE 77`, so it **skips even under `MCC_GPU_REQUIRED=ON`**, where its
`-known-positive` and `-real` siblings go through `cmake/spvgate_mutate.cmake` and
`cmake/spvgate_real.cmake`, which turn 77 into a `FATAL_ERROR`. The `gpu/msl-slice-*` trio
inherits the same asymmetry. If a Metal cell must be armed on a self-hosted runner, it needs
the wrapper, not the bare registration.

**MoltenVK is the cheap alternative and it is half-installed already.** `brew install
molten-vk` is in both workflows (`.github/workflows/ci.yml:186-190`,
`.github/workflows/matrix.yml:125-129`), and it supplies a **loader only** — neither macOS
job passes `-DVulkan_INCLUDE_DIR`, so `find_package(Vulkan QUIET)`
(`CMakeLists.txt`) fails, the Darwin fallback at `:3550-3558` never fires, `spvgate` is
never built, and the three `gpu/spv-slice-*` names stay `mcc_skip_test` stubs on macOS. Two
things follow. On a **developer's** Mac, passing the headers makes the entire SPIR-V arm —
regions, binding 2, the format engine, frame kernels, dynamic indexing — run over Metal
today, with no MSL written. In **CI** it changes nothing, because MoltenVK still needs a
Metal device the runner does not have; the cells would build and then exit 77. The only
macOS gate cell is `{"macos-arm64-clang", "gpu-vulkan"}` (`tools/ci.c`), it sets
`MCC_GPU_REQUIRED=ON` itself, and no CI cell anywhere sets `-DMCC_GPU_BACKEND=metal` — so
the `gpu/msl-slice-*` trio has never been built by CI, not once. There are no self-hosted
runners; every `runs-on` in this repo is a GitHub-hosted image.

## Windows — the whole surface, enumerated, measured and priced, 2026-08-09 (`wt/winspec`)

> **Read §0 before anything else in this section.** Windows is not a thin cell that skips.
> It is **five targets, 2,932 lines of PE emitter, a 112-file CRT tree and 89 registered
> cells**, and it is the second-largest object format in the tree. It is also the one
> platform whose *object files cannot be linked by any other toolchain on that platform*
> — measured today, not inferred — and the one platform the two external cross-oracle
> corpora have never been pointed at. §0 states what can actually be validated and by
> what; §4 prices the largest coverage win available anywhere in this tree and reports a
> pilot that has already been run rather than proposing one.
>
> Every figure below was re-derived on this branch on 2026-08-09 against `2fbd830f`.
> Nothing is inherited. Where a number could not be re-derived it is marked
> **UNMEASURED**; where a claim survives only as prose it is marked **PROSE-ONLY**.


### Residues kept from the archived Windows subsections

> §§1, 2 and 4 went to [`docs/ARCHIVED.md`](ARCHIVED.md). §0, §3 (the five divergences), §5 and §6 are kept in full below. These eight were verified open against the tree and restated nowhere else.

**`i386-fastcall-abi`'s verdict does not say which oracle it used (§1)**

**`i386-fastcall-abi` is green here, and the configure-time probe is why.**
`mcc_mingw_resolve()` (`CMakeLists.txt:565`) composes `.exe` paths and performs **no
existence and no executability check**; the fix at `CMakeLists.txt` prefers the
i686 winlibs gcc, falls back to the multilib one with `-m32`, and then runs
`execute_process(COMMAND "${_fc_gcc}" --version)` — because a PE `gcc.exe` in a shared
`vendor/` satisfies `EXISTS` on Linux while being unrunnable. It works. The residual is
reporting: the cell's verdict does not distinguish "measured against mingw gcc" from
"fell back to the harness default", and the only trace is a configure-time
`message(STATUS ...)`.


**`compile.win32` can never be named in `must-run.txt` (§1 item 3)**

3. `compile.win32` registers `compile.win32.<name>` per example on Windows and the bare
   literal `compile.win32` on its skip branch, so **no `must-run.txt` row can ever name
   both states** — `tools/must-run.py` matches by exact string.

**not one PE or wine cell appears in `tests/must-run.txt` (§1 item 4)**

4. **Not one PE or wine cell appears in `tests/must-run.txt`.** Not `pe-wine-conformance`,
   not `pe-native-conformance`, not `run-tier/*-win32`, not `compile.win32`,
   not `def-verify`, not `pe/short-import`.

**`tools/i386win32-soak.sh` is registered nowhere and hardcodes a personal path (§1 item 6)**

6. `tools/i386win32-soak.sh` and `tools/arm64pe-wine-docker.sh` are **registered nowhere**.
   The second matters: it is the only arm64-PE *execution* path in the repo, and it
   contradicts the permanent-skip reason at `CMakeLists.txt` that says no host can
   run arm64 PE. `tools/i386win32-soak.sh` also hardcodes a personal absolute path as
   its default mingw prefix.

**the Vulkan loader's full Windows arm has never been executed (§2)**

| **GPU / Vulkan** | `libvulkan.so.1` | **the loader has a full Windows arm**: `vulkan-1.dll` in the soname list (`src/mccgpu.c`), `#define VKAPI_PTR __stdcall` (`src/mccgpu.c`), `ucrtbase.dll`/`msvcrt.dll` for the libm fallbacks | **has the code, has never executed it.** CI installs `vulkan-headers` on `windows-latest` so it *builds*; the `gpu-vulkan` feature is skipped on every Windows host. **UNMEASURED** whether a Windows Vulkan dispatch has ever run |

**`__int256` is unmeasured on every PE target — `wide256/gmp-diff` is native-host-only (§2)**

| `__int256` / `unsigned __int256` | full, `wide256/gmp-diff` 9,402 rows | **compiles on all five targets**, executes correctly under wine (`1<<200` × 3 → 3). Passed by memory on Win64 like any >8-byte aggregate — the `using_regs` rule at `src/arch/x86_64/x86_64-gen.c` sends anything not 1/2/4/8 bytes to memory, which is the correct Win64 treatment | **has it — but see below.** `wide256/gmp-diff`, the only proof `__int256` is *correct*, is a native-host cell. **UNMEASURED on any PE target** |

**the cross-vendor caveat: the pilot judged gcc's corpus with mingw-gcc (§4)**

**What the oracle would be.** `x86_64-w64-mingw32-gcc` / `i686-w64-mingw32-gcc` for the
gcc corpora, and `clang --target=x86_64-w64-mingw32` for the llvm corpora — preserving the
project's cross-vendor rule that a suite is never judged by its own vendor. **One caveat
must be stated rather than buried:** on the pilot host `clang --target=x86_64-w64-mingw32`
resolves the triple but has no mingw sysroot, so the *pilot above used mingw-gcc against
the gcc corpus*, which violates the cross-vendor rule. Its 1,505/10 is therefore a
**lower bound on agreement and an upper bound on nothing** — a same-vendor oracle
systematically under-reports. The real cell must either ship the llvm-mingw clang the
build already knows how to fetch (`MCC_LLVMMINGW_AARCH64_URL` in `cmake/winlibs.cmake` is
the same mechanism, aarch64-only today) or run the llvm corpora, judged by mingw-gcc,
where the vendors are correctly crossed by construction.

**what actually blocks W1 — no run-wrapper indirection exists anywhere (§4)**

**What breaks, concretely.** All four cross-oracle tools execute natively and have **no
wrapper hook whatsoever** — `xoracle.py`'s `Phase.execute`, `xsuite.py`'s runner,
`jitconform.py`'s `run_prog`, `gpuconform.py`'s `run` all call `subprocess.run` on the
produced binary directly. A tree-wide grep for `MCC_RUN_WRAPPER` / `MCC_EXEC_WRAPPER` /
`MCC_RUNNER` returns **nothing**; that variable does not exist. What *does* exist is
`MCC_TEST_RUNEMU` (read only by `tests/runner.c`) and the `RUN`/`ENVPFX` pair in
`tools/run-tier.sh`, which already selects wine correctly. All three also use
`preexec_fn=` for `RLIMIT_*`, which is POSIX-only — fine under wine-on-Linux, since the
*harness* stays on Linux and only the produced `.exe` goes through wine.

The work is: thread an optional wrapper list through the three execute sites and the build
sites, add `--target-mcc`/`--wrapper` arguments, and relax the `UNIX AND MCC_CPU
x86_64|arm64` guard at `CMakeLists.txt` so a PE triple with a wine on PATH
qualifies. Nothing structural resists it. The runner in §3 is 60 lines and already proves
the shape works end to end.

### 0. The testability answer, stated first

**Roughly two of the five Windows targets are executed anywhere, and the executed pair is
executed only under wine on a Linux host and only on 33 distinct programs.**

| what | how it is validated today | how much |
| --- | --- | --- |
| `x86_64-win32` | wine on Linux/macOS: `pe-wine-conformance`, `run-tier/x86_64-win32` | **18 + 15 = 33 programs** |
| `i386-win32` | same two cells | **18 + 15 = 33 programs** |
| `arm64-win32` | **compiled and byte-banked only.** `run-tier/arm64-win32` is a permanent `mcc_skip_test` (`CMakeLists.txt:4531-4534`) | **0 programs executed** |
| `arm-win32` | compiled and byte-banked only, same skip | **0 programs executed** |
| `arm-wince` | compiled and byte-banked only, same skip | **0 programs executed** |
| native Windows host | `pe-native-conformance` — CI only, `windows-latest`, MSVC-bootstrapped self-host | Skipped on every non-Windows host |

**The wine tier is trustworthy under parallel load, measured today.** The historic flake
was diagnosed and fixed (the wineserver-writeback race, `tools/run-tier.sh`), and
this session could not reproduce it: **two** full `ctest --test-dir cmake-debug -j32`
sweeps returned **9456 cells, 0 failures** (229.95 s and 213.12 s wall, 625 Skipped), and a targeted
`ctest -R "run-tier/.*-win32|run-tier/arm-wince|pe-wine-conformance" -j32 --repeat
until-fail:5` returned **8/8 passed, 0 flakes, 22.58 s** with no abandoned prefix left
behind. What remains is a *structural* hazard rather than an observed one: **no wine cell
carries `RUN_SERIAL` or `RESOURCE_LOCK`, and `RESOURCE_LOCK` is used nowhere in this
tree**, so the isolation is by-construction (each runner gets its own `mktemp -d`
`WINEPREFIX` and kills only its own suffix-matched `wineserver`) rather than enforced by
ctest. `mccharness`'s `pewine` arm never got the `run-tier.sh` teardown fix — it creates a
prefix under `${CMAKE_BINARY_DIR}/pe-wine-work/.wineprefix` and contains no `wineserver`
shutdown at all, so it leaves a live server holding that prefix after the cell reports.

**CI, as evidence.** Expanding every workflow once gives **195 job instances, 56 of them
Windows-touching**, but the number overstates: **18 of the 39 `matrix.yml` Windows stage2
cells are pure no-ops** that check out, install MSVC and ninja, then `exit 0` on the skip
guard. Wine is installed in CI **only on Linux and macOS runners, never on a Windows
runner** (`.github/workflows/ci.yml:117-137`, `:180-185`). The whole `windows-11-arm`
surface is nightly-only and two of its three appearances carry `continue-on-error`. Push
and PR see exactly **five** Windows job instances.

**The honest summary:** the surface *is* built everywhere, is byte-banked at `-O0` for all
five targets, and self-hosts natively on Windows in CI. What it is not is *differentially
executed*. Three of five targets have never run a program. The two that do run programs
run 33 of them, against goldens, with no external oracle. Compare Linux, where
`jit/xoracle-conformance` and `slice/cref-oracle-*` drive **1,693 + 1,745 + 671 + 226**
external programs through cross-vendor adjudication. That asymmetry is the finding.

### 3. Five divergences nobody had looked for — found in ten seconds

Before pricing the cross-oracle in §4, here is what it already found. A hand-written
runner compiled all **1,693** `vendor/gcc-c-torture-execute` programs with
`cmake-cross/mcc-x86_64-win32` at `-O0` and with `x86_64-w64-mingw32-gcc` 15.2.0 at `-O0`,
ran both under `wine64`, and compared exit status and CRLF-normalised stdout. **9.95 s
wall at 16 threads.**

| status | count |
| --- | ---: |
| agree | **1505** |
| oracle failed to compile (mingw-gcc) | 132 |
| mcc failed to compile | 46 |
| **DISAGREE** | **10** |

The first 300 on `i386-win32` against `i686-w64-mingw32-gcc`: **281 agree / 11 / 7 / 1**.

Each of the ten was then re-run on ELF (`cmake-debug/mcc` against system `gcc` 15, native
`-O0`) to separate a Windows defect from a pre-existing mcc-vs-gcc divergence:

| program | ELF verdict | reading |
| --- | --- | --- |
| `20021127-1` | mcc 134 / gcc 0 | target-independent (builtin `llabs` folding vs a user definition) |
| `20230630-2`, `20230630-4` | mcc 134 / gcc 0 | target-independent |
| `pr85156` | mcc 134 / gcc 0 | target-independent |
| `return-addr` | rc equal, **stdout differs** | target-independent (`__builtin_return_address`) |
| **`20101011-1`** | **ELF agrees** | **Windows-specific** — `-fnon-call-exceptions`, integer divide-by-zero. On Windows this raises SEH, not `SIGFPE`. Lands squarely on the missing-SEH row |
| **`pr92904`** | **ELF agrees** | **Windows-specific** — "PR target/92904", varargs with **over-aligned aggregates** (`__attribute__((aligned(16)))`, `(32)`). Lands squarely on the Win64 calling-convention row |
| **`pr23324`** | **ELF agrees** | **Windows-specific** — signed bitfields in nested structs plus an empty union. Struct/bitfield **layout** |
| **`pr36321`** | **ELF agrees** | **Windows-specific** — `__builtin_alloca(0)`; the program's own body contains `#ifdef _WIN32 abort();` |
| **`pr123864`** | **ELF agrees** | **Windows-specific** — `__builtin_mul_overflow_p` on `long long` × `~0U` |

**Five Windows-specific divergences, and every one of them lands on a row this section
already flagged as a gap.** That is the strongest available evidence that the parity
matrix is describing something real rather than cataloguing `#ifdef`s: the oracle found
the same holes independently, from the outside, in under ten seconds, on a corpus that has
been sitting in `vendor/` this whole time.

None of these five is triaged here and none is claimed as a proven miscompile — a
disagreement with one oracle is a subject for triage, not a verdict. Three of them
(`pr92904`, `pr23324`, `pr36321`) are the shapes where a Windows ABI genuinely differs
from SysV, so at least one is likely to resolve as "mcc is right and the corpus encodes
gcc's Windows behaviour". Which is exactly why the cell should exist: nobody can currently
tell.

### 5. The staged plan

Each stage lands independently and each carries a differential that fails if the stage did
nothing. Line estimates are for compiler/harness code, comments excluded per the standing
instruction.

**Stage W1 — the wine cross-oracle. ~400–640 lines.**
As priced in §4. Differential: a `--mutate` arm in the same shape as
`cmake/gpuconform_cref.cmake`'s — perturb one emitted constant and require the cell to go
red, so the oracle cannot pass by adjudicating nothing. Floors `--min-pass` and
`--max-differ` banked from the first clean run, never from the pilot. **Land this first.**
It is the cheapest stage, it is the only one that produces new *information* rather than
new code, and it will re-rank every stage below it.

**Stage W2 — triage the five. ~0–400 lines, unknown until W1 lands.**
`20101011-1`, `pr92904`, `pr23324`, `pr36321`, `pr123864`. Some will resolve as
"the corpus encodes gcc's Windows behaviour and mcc is right", which costs an exclusion
entry and a sentence. `pr92904` (over-aligned aggregates through Win64 varargs) is the one
most likely to be a real ABI defect and the one with the largest blast radius. Differential:
each finding gets an `tests/exec` golden with a `req: os=WIN32` line, so it is checked on
every Windows build forever. **Do not schedule W2 before W1** — triaging five programs by
hand when a cell could triage 1,693 is the wrong order.

**Stage W3 — a COFF object writer. ~900–1,400 lines.**
`coff_output_obj` alongside `elf_output_obj`, reusing the section/symbol/reloc model the
PE image writer already has, plus the inverse of `coff_map_reloc` (which already exists for
AMD64/I386/ARM64 in the read direction) and an ARM32 arm. This is what makes mcc a
*participating* Windows toolchain rather than a self-contained one: separate compilation,
`ar`/`lib` archives, linking against mingw or MSVC objects in both directions.
Differential: compile each half of a two-TU program with mcc and with mingw-gcc in all four
combinations, link with both linkers, run under wine, require all outputs identical — the
same four-way shape `tools/i386fastcall-docker.sh` already uses for the i386 ELF ABI.
This is the largest genuine gap and the one with the clearest user-visible payoff.

**Stage W4 — per-function `UNWIND_INFO`, and unwind for i386/arm. ~500–800 lines.**
Replace the single shared 8-byte blob at `src/objfmt/mccpe.c` with real prologue
opcodes derived from the frame the epilog already knows, so a Windows debugger and
`RtlUnwind` can walk mcc frames truthfully. Extend `.pdata` to arm-win32 (which today has
none) and add the i386 `FS:[0]` chain. Differential: a `CaptureStackBackTrace` /
`RtlVirtualUnwind` depth check across a known call chain, run under wine, plus the same
under `-O0`/`-O2`/`-O3`. Note this stage is **partly blocked on W3** for i386, since an
i386 SEH chain is meaningless without objects anyone can link.

**Stage W5 — CodeView `.debug$S`/`.debug$T`. ~1,200–1,800 lines.**
Line tables and symbol records to start; types after. Removes the `cv2pdb.exe` dependency
and makes mcc output debuggable by WinDbg/Visual Studio. Differential: the same shape
`dwarfgdb-docker.sh` uses on the ELF side — set a breakpoint at a named line, check the
reported frame and one local. **Requires W3**, because CodeView lives in COFF sections.

**Stage W6 — SEH statements (`__try`/`__except`/`__finally`). ~800–1,300 lines.**
Parser, scope tables, `__C_specific_handler` on x64 and the FS:[0] chain on i386.
Differential: filter-expression ordering, `__finally` on both normal and unwinding exit,
and `20101011-1`'s divide-by-zero. **Requires W4** — a handler needs a truthful
`UNWIND_INFO` to be reached at all.

**Stage W7 — arm-win32/arm-wince, or delete them. ~600–900 lines, or ~50.**
`arm-wince` is a byte-identical alias of `arm-win32` (§1) and `arm-win32` has no ARM PE
backend at all. The choice is: give ARM32 PE TLS, `__chkstk`, unwind and COFF relocations
(~600–900) and accept a target nobody can execute, or delete `arm-wince` and demote
`arm-win32` to an explicitly-compile-only target with the bank cells labelled as such
(~50 lines, mostly CMake and `tools/build.c`). **The second is correct.** Windows
CE has been end-of-life since 2013 and `arm-win32` targets a machine id (`0x01C0`) that
modern Windows does not load. Two of the twelve `-O0` bank keys are currently spending
budget proving that two identical configurations produce identical bytes.

**Housekeeping that is cheap and should ride along with W1, ~60–100 lines total:** give
`pe/short-import` and `exec-gatecombo/*` `else()` arms; give `pe-wine-conformance` the
`run-tier.sh` wineserver teardown; add `RESOURCE_LOCK "wine"` to the wine cells so the
structural hazard in §0 stops being structural; add `MCC_WINE` and `MCC_WINE_REQUIRED`
cache variables so a wine-less host can be made to fail rather than green-skip; add
`pe-wine-conformance` and `run-tier/{x86_64,i386}-win32` to `tests/must-run.txt`; correct
`tests/must-run.txt` to state both of `runtime-bench-gatewin`'s causes; add `arm-win32`
to `tools/build.c`; fix the stale i386 message at `CMakeLists.txt`.

### 6. The verdict, and the sequencing

**What is a real gap.**

1. **No COFF object writer.** This is the one. It is not a cell that skips; it is a
   capability the platform requires and the compiler does not have, and it was invisible
   because every Windows cell in the tree drives mcc all the way to a linked `.exe`, where
   the format is right. Measured today, in both directions, on a one-line function.
2. **No external oracle on any Windows target**, while Linux has four corpora and 4,335
   external programs. §3 shows the cost of that absence is not hypothetical: **five
   divergences, none previously known, in under ten seconds.**
3. **No CodeView/PDB.** A shipped Windows compiler whose output no Microsoft debugger
   reads is a real gap, not a preference.
4. **A single shared `UNWIND_INFO` blob on x64, and none at all on i386/arm.**
5. **No SEH statements.** Bounded and honest — but `20101011-1` shows it has observable
   consequences beyond `__try` itself.

**What merely looks like a gap because a cell skips.**

- `compile.win32`, `pe-native-conformance` — correct WIN32-host gating. They run in CI on
  `windows-latest` and are supposed to skip here.
- `runtime-bench-gatewin` — not a Windows cell at all. Misnamed, and dead for a missing
  `vendor/plb`.
- `embed-jit-smoke` — correctly gated; needs a mingw-hosted Windows build.
- The 23 `exec*/winarm64_interlocked` cells — a `req: cpu=arm64,os=WIN32` golden that no
  host in this tree can satisfy. Not a defect; but 23 cells that can never run anywhere are
  a reporting problem, and they are 26% of the Windows cell count.
- `run-tier/{arm64,arm}-win32`, `run-tier/arm-wince` — genuinely unrunnable, correctly
  skipped, with an accurate reason. `arm64-win32` is the one worth revisiting, because
  `tools/arm64pe-wine-docker.sh` exists and contradicts the reason text.
- The five `ast/rir-c2-<pe>` skips — the `MCC_REPLAY_IR_C2` default, nothing to do with
  Windows.
- **`arm-wince` in its entirety.** It looks like a fifth target; it is a duplicate.

**Sequencing against everything else open.** **W1 goes first and it should go before most
of what is currently ranked above it.** The argument is not that Windows matters more than
the GPU or the optimizer; it is that W1 is ~400–640 lines, runs in ~4 s, and is the only
open item that *converts an unmeasured surface into a measured one*. This file's cardinal
sin is agreement over nothing, and 2,932 lines of PE emitter validated by 33 goldens is
that sin at platform scale — the same defect `slice-census` had when nothing set
`MCC_SLICE_CENSUS_RUN`, except that the population is a whole object format. W1 is also
the only stage whose result changes the priority of the others: if W2's triage resolves
`pr92904` as a genuine Win64 varargs defect, that outranks W3.

**W3 (COFF writer) is the largest real gap and should be scheduled next**, but only after
W1, because W1 costs a fifth as much and will tell you whether the codegen underneath the
object format is even correct. Shipping an interoperable object format for a backend with
five unmeasured divergences would be shipping the divergences to more people.

**W5 (CodeView) and W6 (SEH) are correctly below the GPU and optimizer work.** They are
large, they are blocked on W3 and W4, and neither is a correctness gap — they are
integration gaps. They belong on the ladder, not at the top of it.

**W7 should be taken as a deletion, now, independent of everything else.** It is ~50 lines,
it removes a target that duplicates another byte for byte, and it stops two `-O0` bank keys
from spending budget proving a tautology.

**Below the line and deliberately:** anything requiring Windows-on-ARM hardware, a real
Windows CI runner beyond what `matrix.yml` already has, or a `.rc` compiler. All three are
real absences and none is on the critical path of anything above.
## Metal parity — the drop is reversed by decision, and this is the spec, 2026-08-09 (`wt/metalspec`)

> **This section overturns a refusal that is still live in this file.** `#### Metal —
> settled` and board row 3 record Metal as **decided and dropped** on 2026-08-09, on
> measured grounds: a frozen MSL arm half the size of the SPIR-V one, a two-line frame
> kernel arm, no region layer, and a format engine that is SPIR-V-only by construction.
> **The owner has reversed that decision and asked for the implementation spec.** This is
> that spec. The refusal is not deleted: every figure it rested on is re-derived below at
> §1, **and every one of them still reproduces — the divergence is wider than when it was
> banked, not narrower.** What changed is the decision, not the evidence, and §6 says so without
> softening it. Two things in this section are load-bearing before any line is written:
> **§4, because no GitHub-hosted macOS runner can execute a Metal kernel at all**, and
> **§4's MoltenVK column, which buys a superset of the user-visible outcome for roughly
> 1/300th of the code.**


> **§§0–5 archived 2026-08-11** to [`docs/ARCHIVED.md`](ARCHIVED.md) — the refusal grounds,
> the measured gap, the parity matrix, the runtime story and the testability section were an
> 2026-08-09 snapshot and several of their tree claims had gone stale (§0 ground 3 and §1's
> `is_float` count are both refuted by the tree today). **§6, the verdict, is kept below**, and
> so are the four things §6 depends on:

#### Residues kept from §§2–5

**the certified-set mirroring rules for a soft-f64 (Metal §2)**

**What `double` means for parity, precisely.** MSL has no 64-bit float type, so no MSL
kernel can hold an IEEE-754 double in a register. The parity question is therefore not "can
Metal do fp64" but "can Metal mirror the *certified* set". The certified set is exactly
`VT_DOUBLE` `+`, `-`, `*`, unary `-`, `!`, the six ordered comparisons and `&&`/`||` truth —
**3,965 tuples, exact bit equality, no tolerance** — and everything else is already excluded
on *both* arms with a cell that fails if the exclusion lapses: `/` and `%` at any width,
`VT_FLOAT`, `VT_LDOUBLE`, int↔float conversion in either direction, and mixed operands.

- **Mirrorable in software, exactly:** every certified operation. Soft-f64 add, subtract,
  multiply, negate, compare and truth-test are deterministic integer algorithms; a correct
  implementation is bit-exact by construction, including subnormals, both zeros and both
  infinities. Precedent in this tree: the soft-int64 kernel already in `msl_prelude`, and
  E6's measured 8,700-byte soft-f64 prelude.
- **Not mirrorable, and not needed:** division. It is 2.5 ULP by SPIR-V spec, is excluded on
  the Vulkan arm for exactly that reason, and a software implementation would be *more*
  exact than the certified arm — which is a differential failure, not a win.
- **The one genuine divergence risk:** the two-NaN payload tie-break. `x86-64` SSE returns
  the first operand's payload and the device returns the second; a software f64 returns
  whatever it is written to return, so it must be written to return the *device's* answer
  to stay green, and that answer is a device property nobody has measured on Metal.
  Mark that **UNMEASURED**.
- **The cheap alternative, already taken:** gate it off. `mcc_gpu_f64` already returns 0 on
  the Metal arm, so an f64 work item is admitted, refused by the emitter at
  `src/mccgpu.h`, and falls back to the CPU oracle. No crash, no wrong answer, no
  coverage. **This is parity of behaviour, not parity of capability**, and §5 stage M6
  prices both.


**the macOS Metal-toolchain hazard (Metal §3)**

**One Metal-specific hazard the Vulkan arm does not have.** On recent macOS the Metal
shader compiler is a separately downloadable toolchain component rather than part of Xcode,
and its standard library moved names between versions. Everything this backend emits is
compiled at runtime from source text through `newLibraryWithSource:`, so a toolchain that is
absent or a standard library that renamed a symbol is a *runtime* failure on the user's
machine, not a build failure here. Nothing in this tree pins the MSL language version.
**External, not re-derivable in this tree; mark UNMEASURED.**


**lavapipe on macOS — the highest-value experiment, still UNMEASURED (Metal §4)**

**And there is a third route nobody has priced, which is the only one that could give macOS
a real CI differential: lavapipe on macOS.** Mesa's software Vulkan ICD builds for macOS,
and the Linux cells already prove the SPIR-V arm runs correctly on it. An `MCC_VULKAN_LIB`
override already exists to point the loader at an arbitrary ICD (`src/mccgpu.c`),
so no code change is needed — only a build of the driver on the runner. **Whether a lavapipe
ICD can be obtained on a `macos-15` GitHub runner in reasonable time is UNMEASURED, and it
is the single highest-value experiment in this section**: it would make the macOS
`gpu-vulkan` cell a real device cell for the first time, at zero lines of backend code, and
it would do so for the arm that has 4,200 lines of capability rather than the one that has
1,755. Note also that `docs/ARCHIVED.md:22777-22788` is now stale on its own terms — it says Linux
CI installs "no lavapipe, no SwiftShader", and `.github/workflows/ci.yml:116` installs
`mesa-vulkan-drivers` today.


**the M1–M8 stage table (Metal §5) — §6 sequences by these labels, so they cannot leave**

### 5. The staged plan

Each stage is independently landable and independently testable, in the sense that its cell
exists and passes on a Darwin host with a Metal device. **On every CI runner this project
has, each of these differentials is a skip.** Estimates are the author's, calibrated against
the two recent priced items in this tree — `double` on the SPIR-V side was estimated
1,100–1,700 lines and landed; 256-bit `ymm` codegen was estimated 2,000–3,000 — and are
**UNMEASURED** in the sense this file uses the word.

| # | stage | work | est. lines | differential |
| ---: | --- | --- | ---: | --- |
| **M1** | live-slot store-back | the MSL twins of `spv_store_at_in`, `spv_store_live` and `spv_store_live_v`; make buffer 0 read-write in the emitted kernel and in `mcc_gpu_dispatch_locked`; `mcc_gpu_rw_supported` → 1 and `mcc_gpu_rw_arm` records the copy-back pointer | **200–300** | `gpu/msl-slice-differential` extended; `slice/ops`, `slice/gpu` on Darwin |
| **M2** | frame kernel | the MSL arm of `mcc_slice_frame_kernel_build`: statement stores, the `while`/`for` lowering with the `MCC_SLICE_TRIP_MAX` guard, and the return-value path. This is the single biggest emitter gap and it depends on M1 | **350–500** | `slice/frame`, `slice/frame-known-positive`, `gpu/msl-slice-real` |
| **M3** | dynamic element index | the MSL twins of `spv_slot_at`, `spv_dyn_elem`, `spv_load_live_dv`, `spv_store_live_dv`. Closes the `msl_expr` `dynidx` hole already filed against the gate | **150–250** | `slice/wide64`, the `dynidx` corpus in `mslgate` |
| **M4** | region layer and binding 2 | an `MslRegion` twin of `SpvRegion`, the branch-free bounds-and-alignment check that `spv_region_addr` performs, per-width load and store with read-modify-write below word size, the host-pointer subtraction `spv_mem_off` performs, and the shared-region constructor. Host side: a third `MTLBuffer` at index 2, `mcc_gpu_mem_backend` returning its `contents`, and the whole-workgroup constraint the SPIR-V arm already enforces | **400–600** | `slice/mem`, `slice/bytes`, `slice/deref`, `slice/real` |
| **M5** | format engine | an MSL half of `src/mccfmt.h` mirroring the seven `spv_fmt_*` functions against the 244-line SPIR-V half. Depends on M4 — every one of them addresses a region | **250–400** | `slice/fmt`, `slice/fmt-known-positive` |
| **M6a** | `double`, gated | **0 lines — already in the tree.** `mcc_gpu_f64` returns 0 on Metal and the emitter refuses. Add the missing ratchet: a cell that fails if an f64 module is ever built on the Metal arm | **30–60** | a new exclusion cell beside `f64_exclusions` |
| **M6b** | `double`, emulated | soft-f64 in `msl_prelude` for the certified set only — add, subtract, multiply, negate, compare, truth — plus an `f64` flag on `MslV`, a `used_f64` on `MslMod`, and pack/unpack at the buffer boundary. **Do not implement division.** The NaN payload tie-break must be written to match the device, and that answer is unmeasured | **700–1,100** | `slice/f64` on Darwin, against the 22-payload table |
| **M7** | gate parity | `mslgate`'s Metal-side plumbing for binding 2, so the differential can reach the region layer at all. Note the SPIR-V gate has the same defect from the other direction: `tools/spvgate.c` declares two bindings, not three | **150–250** | `gpu/msl-slice-real` over region cases |
| **M8** | CI | **nothing to write.** Requires a self-hosted Apple-silicon runner. Until one exists, M1–M7 land with every differential skipped | 0 | — |

**Totals.** M1–M5 plus M6a and M7: **1,530–2,360** lines. With M6b instead of M6a: **2,200–3,400**. That is the `ymm` band, not the `double` band, and it is materially below
what "multi-week rewrite" implied — because the runtime was already written.

**Ordering, and why.** M1 before M2 because the frame arm stores through the live-slot path.
M4 before M5 because every `spv_fmt_*` addresses a region. M3 is independent and is the
cheapest real gain. **M6b is last and should probably never be reached**: it is the largest
stage, it buys a type the corpus values at +0.0 device-executable points, and it is the only
stage whose correctness depends on an unmeasured device property.


### 6. The verdict, and the sequencing advice

**What it costs.** 1,530–2,360 lines for behavioural parity, 2,200–3,400 for capability
parity including fp64, across the emitter in `src/mccgpu.h`, the frame builder in
`src/mccslice.h`, a new MSL half of `src/mccfmt.h`, and about 200 lines of Metal host code.
Every line of it is a second implementation of something that already exists and is already
green.

**What it buys.** Three things, stated at their real size:

1. A second independent emitter against the same CPU reference — a genuine differential *of
   the emitter*, which is the one thing a single backend cannot give you. `## The Metal
   per-value differential, and N6 on Darwin` shows the shape works: 151.9 M points, zero
   mismatches.
2. Native device execution on the owner's own hardware without MoltenVK, at 150 µs versus
   181 µs dispatch and 5–20% off pipeline compiles.
3. The end of a divergence that grows on its own. That is real and it is the strongest
   argument for doing it — but it is an argument for *deciding*, and the decision has now
   been made in the other direction, which already stops the growth from being silent.

**What it does not buy, and this is the part that decides the sequencing.**

- **No CI differential, on any runner this project has.** §4.
- **No user reaches it.** `mcc_slice_frame_from_ast` is defined once (`src/mccslice.h:532`)
  and called **17 times, every one in `tools/slicerun.c`** — **zero call sites in `src/`**.
  `src/mccslice.h` has exactly one includer in the tree and it is that same test binary.
  The entire frame executor — the thing M2, M4 and M5 exist to complete on the Metal side —
  is unreachable from the compiler proper. The compiler's own slice sites pass `nlive = 0`
  and one tuple, and both kernel builders refuse `nlive < 1` by construction.
- **The lanes are not there.** The device-executable parallel-legal iteration-weighted
  fraction of the numeric corpus is **≈1.45%**, and landing `double` on the SPIR-V side
  moved it **≈1.45% → ≈1.45%** — because the points are in `static` arrays of 360,000 and
  65,536 elements against `MCC_SLICE_MAXSLOT` = 16, not in the type. Metal parity does not
  touch either constraint.

**The verdict, in one line: build it, but not now — and if the goal is "the device path
works on my Mac", spend ten lines on MoltenVK instead of two thousand on MSL.**

**Sequencing.** In order:

1. **First, the MoltenVK build-config change.** ~10 lines, turns three dead ctest names
   live on Darwin, and gives macOS the whole SPIR-V arm including regions and the format
   engine. It is strictly dominant over M1–M5 on every axis except fp64 and 20% of compile
   time. If it is not worth doing, native Metal parity is not worth 200× more.
2. **Then close the dispatch gap**, which is the only thing that would make either backend
   matter: a batch producer, or the `static`-storage and `MCC_SLICE_MAXSLOT` constraints
   that hold the corpus at 1.45%. Until a caller exists in `src/`, both arms are test-only.
3. **Then, if a self-hosted Apple-silicon runner exists**, M1 → M2 → M3, which is where the
   differential value is concentrated and which is about 700–1,050 lines.
4. **M4 and M5 only after a batch producer exists.** They complete a feature — the on-device
   formatter and the shared address space — whose SPIR-V original has no consumer either.
5. **M6b probably never.**

Deferring it, in the stated order, is not the same as refusing it. The refusal below is
superseded; the sequencing is not a veto, it is the price list.

## What is actually still open — swept and verified 2026-08-09 (`wt/sweep`)

Every section below this one was enumerated and each item classified OPEN / CLOSED-VERIFIED
/ CLOSED-UNVERIFIED / REFUSED / SUPERSEDED-STALE. Closure claims were checked by **running
the tool or the cell**, not by reading. **24 spot-checks were taken; 20 reproduced exactly,
3 did not, and one probe turned up a crash nobody had filed** — all four are in this table
below, at rows 1, 2, 5 and in the staleness list. The device-path verdict, ~~the Metal
drop,~~ chain-store re-promotion, `storeval-rot`'s demotion and `narrow`/`tree-copy-prop` were
each re-read against their measurements and are **settled**; nothing below re-litigates them.
**The Metal drop was reversed by decision on 2026-08-09 (`wt/metalspec`) — its evidence
still reproduces, its decision does not. See the spec at the head of this file.**
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
Anchors into other files (`src/`, `tools/`, `docs/ARCHIVED.md`, `CMakeLists.txt`) are unaffected and
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
| **4** | **`if-conversion-abs` ships at `MCC_OPTD_LEVEL(2)` and the freshly re-run bench says it makes code worse.** `tests/optfire/levelbench.tsv`: moves **1 of 17** kernels, `gain_movers` **−0.0334**, `branchy` **−0.5700** — a sign flip from the `+0.1905` / `+3.1843` it was promoted on. It is bucketed `ranked`, not `cost-no-gain`, so the ladder still treats it as a win. It is filed **only** in the failed-to-reproduce table at `:685`; no row of the ranking table owns it, and `:517` asserts "row 1 is the only unmeasured row left" | small (one level decision, the measurement already exists) | **emitted code** |
| **5** | **`MCC_MAX_UNARY_DEPTH` was mis-sized *and* it was one guard where the parser needs eight — DONE, and the eight are now watched.** `diag.parse-frames` re-prices `MCC_MAX_PARSE_DEPTH` against the frames it was sized on, every run; the per-level table it was sized from was re-derived and nine of its ten rows were low. **The two that were filed are now closed on `wt/depthholes`** — `parse_btype`'s `_Alignas` arm (SIGSEGV at **43,606**) and `mccasm.c`'s six-function `asm_expr` cycle (at **18,694**) are both charged to the shared budget, the frames bank is re-derived at **15 cycles** with the worst axis and the 532 KiB requirement unmoved, and `diag.parse-depth` carries both axes. **The third, `next()` self-recursing on `_Pragma` (`src/mccpp.c`, SIGSEGV at 130,794 consecutive `_Pragma` tokens), is closed on `wt/symguard`** — a `tail:` label and a `goto`, no budget level, and `diag.parse-depth` now carries a `pragmachain` case that asserts a clean compile rather than a diagnostic. **No parse-depth hole is currently open.** See "The parse-depth guard" below | done | **correctness** |
| **6** | **Nine number-producing tools are registered nowhere — the board says four.** `:1688-1696` names `xsuite-report.py`, `gate-ledger.sh`, `strategy-ledger.sh`, `c2_sweep.sh` and closes "Four tools left on this item." Also unregistered and board-quoted: `xsuite.py`, `xoracle.py`, `c2_equiv.sh`, `selfhost-o3.py`, `arm64pe_diff.py`. **~~`xoracle.py` is the sharpest~~ — `xoracle.py` WAS REGISTERED by `f797074b` (`jit/xoracle-coverage`, plus `tests/must-run.txt`); the four tools named in the audit residue are still unregistered, so that list is right and this row's nine is not (2026-08-11 sweep)**: `tests/optfire/levelpins.txt` pins `merge-constants` at level 2 on "two xoracle cases change verdict without it" — a shipped ladder pin whose only evidence comes from a tool no cell runs | medium (five more cells) | **census trust** |
| **7** | **`ast_env_gate` no longer exists in `src/` and four shell tools still grep for it.** `grep -rn ast_env_gate src/` is **0**; `tools/{c2_sweep,c2_equiv,gate-ledger,o0_ab}.sh` all still reference it. They fail loudly, which is the right mode, but this is the widest blocker in the file: it freezes `o0_ab.sh`'s gated half (twelve `*.gated.rir.txt` + `board.gated.txt`, uncovered by `ast/o0-baseline` and not pretending otherwise), blocks three of the four tools in row 5, and blocks the cheap "which `-O1` gate erases the 72 `len` bodies" experiment. The restoration recipe is already written down at `:9899-9906` | medium | **gate strength** |
| **8** | ~~**`spirv-val` and `glslc` are installed at `/usr/bin` and referenced nowhere in the build.** `grep -rn 'spirv-val\|glslc' CMakeLists.txt cmake/ tools/ src/` is empty.~~ **FALSE, and it was false when written (2026-08-11 sweep): `cmake/spvval.cmake` is a whole validator cell with a corruption known-positive, and `CMakeLists.txt` does `find_program(MCC_SPIRV_VAL NAMES spirv-val)`. This file's own "`spirv-val` wired | 152 modules validated" row already contradicted it.** 152/152 modules already validate by hand at `--target-env vulkan1.1`. One `find_program` and one `add_test` arm. The cheapest open item in the file, and it survives the device freeze because it validates what the emitter already ships | small | **device correctness** |
| **9** | **A stage-2 build dir does not rebuild when a header changes.** The stale binary is silent and plausible: it runs, it self-hosts, it passes. Workaround only (`rm cmake-<dir>/CMakeFiles/mcc.dir/src/mcc.c.o`); the fix is for `mcc` to emit a depfile for `CMAKE_DEPFILE_FLAGS_C`. This poisons **any** measurement taken from a stage-2 dir, which is most of the ladder work | medium | **measurement validity** |
| **10** | **D6 — `scalar_storage_order` / `ms_abi` are not implemented at all** (`grep -rn 'scalar_storage_order\|ms_abi' src/*.c src/*.h` → **0**), and mcc objects link against gcc's, so a mismatch is *silent* wrong codegen across a linker boundary. The only item in the codegen list with that property | large | **correctness** |
| **11** | **`selfhost-optbench.py --check` can pass over zero derivations.** `derive_levels` assigns `levels[f] = levels_now[f]`, so an all-`inert` run prints *"src/mccopt.h matches the ladder"* having derived nothing; the docstring says **48** level-assignable flags in five places and `flag_table()` yields **16**; no floor on `len(names)`; an empty sample list classifies `inert`. The board already carries "`selfhost-optbench --check` was not re-run" as a caveat, which is the same hole one level up | medium (several floors) | **census trust** |
| **12** | ~~**W8 — `selfhost-jit` heap-UAF of a `Sym` in the AST forward-inline re-emit path.**~~ **Closed 2026-08-09.** Not a cross-function refcount bug: a plain-local leaf's captured `sym` (needed during in-function replay by `wide256_sv_is_stable_lval`) dangles at re-emit after `ast_func_end`. Fixed by `ast_reemit_scrub_leaf_syms` (`src/mccast.c`), which nulls non-`VT_SYM` non-VLA local-leaf syms before re-emit replay. Oracle byte-identical; see the Windows/macOS host-items section | medium–large | **correctness** |
| **13** | **`run-tier/x86_64` fails `tls_threads` when `MCC_JIT=1` meets an active AST replay.** Localised to three lines: `mcc_jit_tls_slab` (`src/mcchost.c:1450`), the `mcc_run_pthread_create` binding (`src/objfmt/mccelf.c:974`) under `s1->run_tls_active`, set only on the interpreter relocate path in `tls_setup_linux` (`src/mccrun.c`). `--no-jit` does not suppress it. Note this contradicts `:10090`'s "the deliberate-red count is now 0", which is true only of the default configuration | small–medium | **correctness** |
| **14** | **`ptr_unlink` for-condition-store segfault** — root-caused to `rir_cf_cond`/`rir_docond`, needs a 5-fix/34-break discriminator. Orphaned: zero references anywhere else in this file | medium | **correctness** |
| **15** | ~~**`full_language.c` still diverges at `-O0` on x86_64/i386** — an `AST_OP_ASM` replay defect (P4 defect 4)~~ **CLOSED on `wt/replayfix`** — and it was not a divergence, the *compile failed*. The replay of an asm that defines a symbol raises a legitimate `assembler label already defined`, and the recovery `longjmp` unwound past `mcc_assemble_inline`'s epilogue, leaving the C parser reading tokens out of the dead `:asm:` buffer. See the landed section | done | **replay fidelity** |
| **16** | ~~**The `jit-splice` pin hides a live miscompile.**~~ **CLOSED on `wt/replayfix`** — and it was not a `jit-splice` bug. A body that takes a label address was miscompiled at `-O1`/`-O2`/`-O3`/`-Os` with no flags, and segfaulted the compiler under `mcc -run`; the pinned flag was the only thing exercising the rebase that exposed it. Pin lifted, `KNOWN_FLAKY_RED` emptied. See the landed section | done | **correctness** |
| **17** | **`-O3` re-emission leaves the pre-inline copy in `.text`** — **27 functions / 52,022 B, ~3.6%** of `.text`. Not a correctness bug; no cell, no bank, no entry in the codegen list | medium | **emitted code** |
| **18** | **`--mutate` is blind to `memcpy`, and the real gap is the corpus.** Four of six operator sites already perturb written memory and `g_frame_mismatch` exists; what is missing is **any `memcpy`/`memset` in the slice corpus to mutate**. Smaller than the debt as filed | small | **test strength** |
| **19** | **Debt 6-vi — the chain-store *member* fixture was never written.** Its stated blocker (debt #6a's `-O1` vstack underflow) has been gone since 2026-08-09. `exec-chainlive/*` covers the live half; the member half of the pairing has no cell | small | **regression cover** |
| **20** | **`flagsweep-cover` and `asm-gas-directives` are `mcc_skip_test` stubs — `cmake -E echo`, structurally incapable of failing.** `flagsweep-cover` hides 75 covering-array rows behind an opt-in that nothing runs; `asm-gas-directives` parks a real unimplemented feature (*"integrated assembler lacks sgdtq/sidtq/swapgs encodings"*) as an always-green cell. Neither is in `tests/must-run.txt`. There are **74** `mcc_skip_test` call sites, 17 live in this configuration | small each | **cells** |
| **21** | **Hazard 1 is still live: `BREAKEVEN` is a hand-pinned literal** at `tools/loop-census.py`, duplicated as `lc_thr[]` in C, and it cannot be gated (`--cost-synth` 77s with no device, `slice/cost` carries `SKIP_RETURN_CODE 77`). The provenance banner landed; the constant did not. **Un-pinning is ~15 lines of C + ~25 of Python**, and it is what the entire remaining integer lane source (`vlaloop`'s 64 trips against a frozen `8`) is adjudicated against | ~40 lines | **ns / lanes** |
| **22** | **`rir-coverage.py`'s `wide` denominator is "the files that happened to compile"** — an `os.walk` of seven directories with no manifest, so a source dropping out silently shrinks the ratchet. Cheap first step already named: bank `sources=N` and fail when it moves. Adjacent: `LOW_EXCLUDE` is a **filename suffix match with no count**, so renaming `mccgpu.c` produces a fake regression with no diagnostic | small (the floor) | **census trust** |
| **23** | **`rir-nofb-probe`, `--check-gap-dir` and `--check-low-dir` all pass over an empty input.** The bank already holds four empty `nofb_miscompiles` lists; gap fixtures cover **3 of 18** `UNF`+`WHY` classes | small per guard, medium for fixtures | **gate strength** |
| **24** | **`stratsweep.sh` and `flagsweep.sh` drop subjects silently.** `$WORK/skipped` is written and never counted; the only floor is `n > 0`, so a miscompile breaking 30 of 31 subjects prints `PASS stratsweep-iso all: 22 strategy/ies x 1 subjects`. Both already print the survivor count — pin it | small | **gate strength** |
| **25** | **The non-LVAL local `Ref` question is now answerable, not open** (`src/mccslice.h`, `:5685`). `wt/decaytype` fixed the identical defect in `ast_dep_decode` on 2026-08-09 — an `AST_Ref` accepted as a base address without checking `VT_LVAL` — with cell `id=25 dp_gptr_alias`. That answers the semantics in favour of "address" but did not touch `ast_eval_slice.h`'s `Ref` arm, `kind_ok` or `livein`. Blast radius **93 of 3994 accepted slices (2.3%)**; one directed test settles it | small | **reference correctness** |
| **26** | ~~**Cluster L is a dependency chain and its first link is unbuilt.** `L1` — give the JIT a shutdown — blocks `L2`/`L3`/`L4`/`L6`/`L7`/`L8`/`L9` by construction~~ — **CLOSED 2026-08-10 (`wt/jitshutdown`), and the row was wrong three ways.** The shutdown is `L2′(i)`, not `L1` (`L1` is device bring-up; `ARCHIVED.md:23151` says *"L2′ before L2"*). **There is no `L9`** — cluster L is `L1`–`L8` plus `L2′`, and this row is the only place in `docs/` the token appears. `L3` and `L6` were never blocked by a `pthread_t`: `L3` residency already landed, and `L6` is a predicate in `src/mccast.c`. What was real: workers `pthread_detach`ed with no handle retained. Now retained and joined; `mccjit_shutdown()` exists, drains, and is `atexit`-ordered ahead of the KGC flush. Genuinely unblocked: **`L2`'s precondition (i)**, **`L4`** on the lifetime axis, **`L8`** on the pool axis, **`L7(i)`**. Still blocking `L2`: `L2′(ii)`/`(iii)`, both `src/mccgpu.c`. Still blocking `L4b`: its own no-`mccjit_swap_lock` constraint, which is `S7b` | landed | **device lifetime** |
| **27** | **The gate-mask gap.** `ast_math_inline_env`, `ast_interchange`, `ast_fusion`, `ast_tile` and `loop-vlat` mutate the arena before the JIT's mask snapshot and carry no `AST_SG_*` bit, so the JIT cannot know what shaped the tree it is handed. Stated at `:8650` and again at `:7756`; no later mention. This is the same class of defect as row 1 of the board's own ranking (a predicate reaching emitted code without its guard) | design | **correctness** |
| **28** | **`storeval-callstore` is at `MCC_OPTD_LEVEL(2)` and was never ranked in either direction** (`src/mccopt.h:39`). The ICE that made its off-state unmeasurable was fixed at `:7629`; nobody has run the bench since. Adjacent and larger: **32 of the 34 demoted rows on rungs 10/11/12 are still unpriced** — only `narrow` and `tree-copy-prop` were measured, and rung 12 remains a deletion-candidate list nobody has read | one bench, then 32 | **emitted code** |
| **29** | **The `MCC_OPT_REPLAY_FALLBACK` flip is an untaken decision, and the fallback is silent either way.** No known defect blocks it (`:9126`), the backstop landed at `705f0b0f`, all four delta-debugged flag sets closed, and `rir-nofb-probe` banks zero miscompiles. Keeping the gate costs **2.0% of bodies but 10.2% of body bytes** getting no optimization at all at `-O1`. **Recommended under either decision and not done: make the divergence visible** — `rir_prod_note` only reports at `MCC_RIR_PROD>=2`, so in a default build a fallback leaves no trace | small (visibility), then a decision | **emitted code** |
| **30** | **No tree-recursion exec golden exists, and the failure mode is a GPU hang.** MSL compiles `fib(n)=fib(n-1)+fib(n-2)` and then **hangs the device at n=5** (`kIOGPUCommandBufferCallbackErrorHang`), taking sibling command buffers with it. Recorded as `ARCHIVED.md:22429` (C4); the golden was never written. Partly defused by the Metal drop, but the golden is a CPU-side conformance test and is still missing. Two more unused-as-conformance shapes named beside it: computed `goto` **into** a `for` body (`tests/diff/parts/legacy_expr.h`) and label arithmetic (`tests/exec/codegen/nodata_wanted.c`) | one golden | **conformance** |
| **31** | **Two device residuals nothing owns.** N11's duplicated upload in `ast_ladder_gpu_run` (the identical `tin` uploaded twice per rung) is untouched — `ARCHIVED.md:23013` says so. N10 picks `devs[0]` with no scoring while `VkPhysicalDeviceLimits` is fully transcribed at `src/mccgpu.c` and only `deviceName` is read — `ARCHIVED.md:23012`, "the `devs[0]` half of I2(D) is still open". Both are frozen with the device path; both are cheap and neither is written down as a row | ~10 lines / ~60 lines | **device correctness** |

Below the line, and deliberately: the device path's own open rows — float (**LANDED 2026-08-09 on `wt/fpwidth`: `double` only, `+ − *` and comparisons, bit-exact; `float`, division and int↔float excluded with cells. It moved the numeric corpus's device-executable fraction by ≈0.0 iteration-weighted points — the 79.2 were gated by `static` storage and `MCC_SLICE_MAXSLOT = 16`, not by `is_float`. See row 6 and the M6 section**), the dispatcher (three subsystems, priced nowhere), 115 indirect blocks, recursion (no data), the `pe` lowerable floors (stale-low, so they under-gate rather than false-fail), and debt #3's descriptor staleness (fixed, unreachable until binding 2 grows). All are frozen by the 2026-08-09 decision and none should be scheduled while that stands.

### Claimed closed, and nothing enforces it

These are the ones worth knowing about. None is a lie; each is a closure resting on prose,
on a single hand-run, or on a cell that cannot reach it.

1. **`slice-census`, `loop-census`, `loop-census-numeric`** — `registered` in the manifest, so `ci/must-run-registered` is green on all three, while no documented invocation runs any of them and one is red. Rows 1–2 above.
2. **`opt-cache-determinism` and `runtime-bench-gatewin`** — both permanent 77s. The manifest discloses this honestly, which is the right thing; nothing ratchets them back to live, and for the first the underlying defect still reproduces. Row 3.
3. **Debt #3, `mcc_vk_bind_mem` descriptor staleness** — the fix is in the tree (`dsdirty`, `src/mccgpu.c:1781/1894/1933`) and the debt states plainly that it is *"not test-covered, and cannot be"*, because both callers pass the constant `MCC_VK_MEM_DEFAULT`. A debt marked paid whose payment is unreachable by construction.
4. **Debt #7, "464 skipped cells"** — closes with *"`cmake-debug` now registers 9106, the same as `cmake-cross`"* (`:3693`). Hazard 5 measures exactly that claim as **false** (8972 vs 9136 by configure order) and says the fix *"did not close the gap — it moved it"*. The debt and the hazard contradict each other and both are written as current.
5. ~~**The Metal freeze** — *"Keep the `#if MCC_GPU_LANG_MSL` arms only where they already compile; add no more"* (`:483`) is enforced by nothing: no cell, no lint. The decision is right; it has no ratchet.~~ **MOOT 2026-08-09 (`wt/metalspec`): the freeze was reversed by decision.** There is nothing left to ratchet, because the arms are now meant to grow. The gap that replaces it is that **no runner this project has can execute a Metal kernel**, so the growth would land untested — see §4 of the spec at the head of this file.
6. **W3's 3-way-concurrent closure** — the best-evidenced result in the file (399 chains / 0 non-identical against a reverse-applied negative control, Fisher p = 0.0015) and no cell holds it. A regression reappears only as flake.
7. **The `mslgate` arm** (`:4033`) — compiles clean and links 51 `msl_*` refs. ~~Now moot under the Metal drop, but still written as an open verification gap.~~ **RE-OPENED 2026-08-09 (`wt/metalspec`) and it is no longer accurate to call it unexecuted**: `## The Metal per-value differential, and N6 on Darwin` records it running 151.9 M points with zero mismatches on an M1 Pro. It is unexecuted *in CI*, and cannot be executed there — see §4 of the spec at the head of this file. It is the harness every stage of that spec is differentiated against.
8. **`ast_eval_slice()`'s poison-flag fix** (debt row 2) — fixed in code and correct; no cell is named for it. Coverage is incidental via `slice/deref` / `slice/real` / `slice/musl`.
9. **The `narrow` pin** — the banked figure did not reproduce (banked **−0.60%** cpu / **1.91%** stage-1; re-read **−0.0088%** / **+0.876%**, about half). The pin rows now carry both numbers; the discrepancy was annotated, not explained, and no cell compares `levelpins.txt` against a re-take.

### Written as live, actually superseded

1. **Board row 1 and still-open row 1** (`:497`, `:507-514`, `:3015`) call the missing `indirect` guard on `ast_dep_base_distinct` **"UNMEASURED, and it is the top of the board."** It landed at **`adf08e3b`**. Verified in the tree: the parameter is `src/mccast.c`, the guard `:13350`, both emitting callers pass `0` (`:13516`, `:13566`), only the census site passes `ast_dep_alias_oracle_env` (`:13949`) — and the **22 `exec-{interchange,fusion,tile,search*}/loop_*` cells are registered and green**. *The board's number-one open row is closed.* Its own body says so at `:2104`.
2. **The registration figure is two generations stale.** `:16-17` say **9136**; `:624` and `:2656` say 9136 *"today"*; `:985` says **9149**; the tree says **9151** in both dirs. The `wt/gatefin` write-up raised it to 9149 and never propagated to the head; the merge with `wt/idiomcov` added the last two and nothing recorded it. Hazard 5's "164 low" delta is quoted against the stale pair.
3. ~~**Three counts of one list.** `:521` "**Twelve** have now failed to reproduce", `:670` "**The nine** figures that have failed to reproduce", `:697` "**Seven** headline figures" — and the table at `:675-687` has **thirteen** rows.~~ **CLOSED on `wt/docsync`.** The table is now the only place the count is stated; every other site points at it instead of restating it, and the two historical ordinals ("the seventh", "the thirteenth") are date-stamped as counts-at-the-time rather than current. `docs/refs` (`tools/docref-lint.py`) counts the table's rows and fails if the stated count moves off them, so this cannot drift again silently.
4. **`SKIP_RETURN_CODE` count.** `tests/must-run.txt` says **141**, `docs/ARCHIVED.md:22965` says **138**, `CMakeLists.txt` has **149**.
5. **`:9598` — "Deliberately not banked: byte faithfulness."** False at HEAD. `kept_coverage` is banked on all eight rows of `tests/rir/coverage-bank.json` **and enforced** (`tools/rir-coverage.py`, skipped only on an unbanked host format). The reversal is recorded at `:7886`; the C2 paragraph was never rewritten.
6. **`:9600-9602`** — "modelled 99.59% / 99.56%, capture 100.00%" is stale; the bank reads **100.0 / 100.0 / 99.9681 / 99.9681**.
7. **`:9584-9590`** — the lowerable floors are **three re-bankings** stale (`MCC_RIR_LOW_EXCLUDE`, the leaf graft, and the fourth re-bank).
8. **`:9624-9626`** — "do not turn `-fno-replay-fallback` on by default… ≥4 fallback bodies are genuinely wrong" is contradicted by the **newer** decision at `:9124-9154` (no known defect blocks it; suite green; `nofb_miscompiles` empty). The prohibition predates the `union_cast` / `transparent_union` / `chained_assign` fixes and has no subject.
9. **`:3116`** — "Five of the eight landed" over a list that holds **nine** items (0–7 plus 6a).
10. **`:3597`** — "`levelbench.tsv:47` is now line 51". The file is **29 lines / 16 data rows**; neither line exists. The same applies to every "24 of 47" / "32 of the 47" count in hazard 2.
11. **`:353-357`** — "turns a lavapipe/NVIDIA denormal disagreement into a hard CI failure no code change can fix" is refuted at `:404-408` and never struck; `:439` still lists the retracted reason as load-bearing in the recommendation.
12. ~~**`docs/ARCHIVED.md:22579`** marks E6 **"NEW 2026-08-08, OPEN"** while `ARCHIVED.md:22989` and `:6238` of this file both say E6 is closed permanently. It is also the line carrying the uncited lavapipe assertion.~~ **CLOSED on `wt/docsync`**: the E6 row now reads *"CLOSED 2026-08-09 — REFUSE permanently"*, carries the Mesa citation inline, and points at the closure below it; the retired plan's I2 row records that the `shaderFloat64` floor rests on a *source* reading of lavapipe, not a device one. Still open, same class: `docs/ARCHIVED.md:22452`/`:22465` "Vulkan — LIVE" vs `docs/ARCHIVED.md:23006` "CLOSED"; `docs/ARCHIVED.md:22708` "half-landed" vs `docs/ARCHIVED.md:23014` "CLOSED"; `docs/ARCHIVED.md:22965` N13 open vs `docs/ARCHIVED.md:23015` N13 closed. Those three are status contradictions in prose, which `docs/refs` cannot see — it checks that a citation resolves, not that two sentences agree.
13. **`:10011` duplicates `:10039`** (32-byte vector alignment), **`:9282` duplicates `:10076`**, **`:9759` duplicates `:10078`**, **`:10051`** sends a reader at a capture path that measures **100.000%**, and **`:10182`** still lists `__builtin_powi`/`powif` as missing after `:9303` closed them.
14. **E1's refusal-site count has now been stated three incompatible ways** and every one was written as current: `:7810` "**eight** separate sites", corrected at `:7851` to "**16**, not 8" with the sites enumerated, corrected again at `:100-113` to "**36 lines, 43 occurrences**, not the 1 + 4 + 6 = 11 this paragraph claims". Only the last is right — verified this sweep: `slice_inline.h:2`, `mccslice.h:4`, `mccgpu.h:12`, `ast_eval_slice.h:18`, because `mccgpu.h`'s block is mirrored across the two emitters and `ast_eval_slice.h`'s eighteen were never counted. `ARCHIVED.md:22574` still carries the "16 refusal sites, not 8" figure.
15. ~~**Seven `TODO` markers in the tree name sections of this file that do not exist**~~ **CLOSED on `wt/docsync`, and the item as filed was itself wrong on two counts.** It said "seven markers in `tests/`"; there are **five markers in four files** under `tests/`, and the count of seven only reaches seven by including two markers in `tools/` and by counting `macro-nesting.cmake`'s two separately. It also cited `tests/superopt/promote-floor.sh`; the marker is at **`:41`**. What each named and what was done: `tests/cst/macro-nesting.cmake` and `tests/cst/symref-shadow.cmake` cited `'CST slice-J'` / `'CST slice-I'`, which **never lived in this file** — they were sections of `docs/NOTES.md`, deleted wholesale at `bb2469bd` and not migrated, so the boundary each test pins is now stated in the test's own failure message and the dead pointer says where it went; `tests/jit/run-parity.sh` cited a "TODO KGC section" purged at `4ab363ce`, and now names `MCC_JIT_NEARMATCH` and `src/mccjit_embed.c` directly, with a note that the nearest live prose does **not** cover near-match parity; `tests/superopt/promote-floor.sh` cited `'Floor the search'`, pruned at `71f3330b`, and now names `so_unsetenv_axis` / `MCC_SO_PROMOTE_FLOOR` in `src/mcc.c`; `tools/embed-jit-smoke.py` cited "P0 step 5" and a "gcc-engine startup residual", **neither of which any row of this file owns**, and now says so rather than implying they are tracked. The three that pointed at the wrong row — `tools/fmt-census.py`, `tests/optfire/levelpins.txt` and `cmake/slicerun_census.cmake` — now cite section headings or the tool to re-run, not board ordinals, because board ordinals renumber.

### The lavapipe citation, now sourced — COPIED ACROSS on `wt/docsync`

This was filed further down this file and deferred to *"whoever owns E6"*. Nobody owned E6,
so it did not move for two sweeps; it has now been copied into `docs/ARCHIVED.md` without
waiting for an owner. It was **independently re-verified during the `wt/sweep` pass** against
`/var/cache/distfiles/mesa-26.0.8.tar.xz`, at exactly the cited lines:

```
src/gallium/frontends/lavapipe/lvp_device.c:454   .shaderFloat64 = (pdevice->pscreen->caps.doubles == 1),
src/gallium/drivers/llvmpipe/lp_screen.c:301      caps->doubles = true;
```

`docs/ARCHIVED.md`'s E6 row used to carry the bare assertion *"Vulkan's `shaderFloat64` is
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
(**166:1**) · ~~Metal (**1754** MSL lines against **3612** SPIR-V, a 2-line kernel arm, **0**
`msl_region*` symbols against **31** `spv_*`)~~ — **Metal is no longer settled: reversed by
decision 2026-08-09 (`wt/metalspec`), re-derived as 1755 / 3960 / 29, and priced. See the
spec at the head of this file** · the device-path freeze (**79.21** of the
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

## The board — re-derived 2026-08-09 against a retaken corpus

> **Body archived 2026-08-11.** The verdict, the ranking table, the provenance table,
> `S5′`, the Metal and D4b rows, the `snprintf` section, the fence wait and debts 2–6 were
> superseded by the 2026-08-10 and 2026-08-11 handoffs and moved to
> [`docs/ARCHIVED.md`](ARCHIVED.md). What remains here is what is still load-bearing: the
> ten hazards, the failed-to-reproduce table, the ~30 `FILED` items, the skip and
> registration sweeps, debts 0/1/7, and the residues below.


### Residues rescued from the sections archived on 2026-08-11

> The bodies around each of these went to [`docs/ARCHIVED.md`](ARCHIVED.md). Each was
> verified still open against the tree and **restated nowhere else in this file**, so it
> would have been destroyed by the migration. Kept verbatim, with its origin named.

**`ast_loop_parallel_legal`'s two stated limits (from S5′)**

Two limits are stated rather than hidden. First, the predicate assumes a store through a
pointer does not clobber a *named global scalar* read in the same loop's exit test —
closing that needs a memory model this tree does not have. Second, a reduction is `0` by
design: `s += a[i]` is not parallel without a reduction transform, and nobody has written
one.

**the corpus-iteration hazard: the total is one kernel's argv (from S5′)**

2. **The corpus's iteration count is dominated by one kernel's argv.** `matmul 600 8` is
   76.9% of all 2.25 billion iterations, and that size was picked by `runtime-bench.py` to
   take about a second, not to weight this census. The per-program table, not the corpus
   total, is the size-independent read — and it says three kernels of seventeen.


**D4b at `-O0`/`-O1`, where there is no pool at all**

**Still open here.** Callees defined after their callers, measured above at ~51 grafts and
about one census block, and therefore declined. `-O0`/`-O1`, where there is no pool at all
and a leaf resolver would have to be built from something else. And the graft still buys
the compiler nothing it emits — it feeds diagnostics and the ladder oracle, because debt
#0 means there is no frame dispatcher to feed.


**debt 6's residue: which strategy pessimizes `gen_op`**

   The residue is a genuine open question, and it is not about this flag: for these 37
   bodies the strategy suite makes the hot code **larger and slower**. `kept` counts
   bodies that run the strategies, not bodies the strategies helped. Anyone who re-opens
   `storeval-rot` should re-open it as "which strategy pessimizes `gen_op`", not as a
   ladder move.

**Ten hazards, stated rather than buried.**

1. **The break-even lanes are pinned by hand** in `tools/loop-census.py` —
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
6. **`tests/exec` and self-compile numbers are never comparable.** `docs/ARCHIVED.md`
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

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *LANDED — `tools/optlevel-bench.py`, the defect that started this*.

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
2. ~~`opt-cache-determinism` should be a `registered` row in `tests/must-run.txt` **and** a
   permanent 77 should be visible as such. Right now the tree reads it as green.~~
   **DONE since `e98fab0a` (2026-08-09)** — `tests/must-run.txt` carries the registered
   row with the PERMANENT-77 disclosure, and this file acknowledges it elsewhere.
   *(2026-08-11 validation sweep.)*

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

21. **The `-O13` `replay-fallback` regression is now UNMEASURED, and `smoke/native` went
    green because of that and not because it was fixed.** On pre-`wt/o4fold` `main` this
    cell failed: `O13
    replay-fallback:len` rose 18 → 42 and `:bytes` rose 3 → 7 when `wt/o4ticks` made one
    tick *finish* the search instead of abandoning it at 13 s, so more bodies diverge in
    bytes and fall back to the interpreter. Value digests are identical at every level, so
    no answer changed — it is a fidelity regression, not a miscompile. `wt/o4fold`
    retargeted smoke to `-O0`–`-O4`, which **stopped measuring `-O13` at all**; twenty rows
    left `tests/smoke/bails.txt`, the tool printed eight `IMPROVED … fell N -> 0` lines,
    and the cell is now green. **None of that is an improvement.** The defect is still
    there, at the one level nothing now watches. Two ways out, and one of them has to be
    taken: give the search tier its own smoke arm with its own bank (the honest fix — the
    search rung is a different subject from the value ladder and needs its own ratchet), or
    put `-O13` back in the main arm and re-bank the two rows red with a reason. Until then
    the search tier has **no bail ratchet at all**.
22. **CORRECTED, and it was backwards. mcc rounds every `_Float16` operation back to
    `_Float16`; gcc-15 and clang-22 keep the intermediate at `float` precision.** The
    2026-08-09 entry claimed the opposite ("mcc single-rounds through `float` where both
    references round per operation") and called it a conformance defect. `wt/smokedepth`
    probed all 12 two-operator `_Float16` shapes over 64³ = 262,144 triples each and the
    direction is unambiguous. Against a model that forces a rounded `_Float16` between the
    two operators, **mcc differs on 0 of 262,144 for every shape**; gcc and clang differ on
    3,058 (`a*b+c`), 9,508 (`(a+b)*c`), 26,183 (`a*b*c`), 27,331 (`(a*b)/c`) and so on.
    Against a model that keeps the intermediate in `float`, the counts swap exactly.
    **`gcc-15 -fexcess-precision=16` reproduces mcc bit for bit on all twelve shapes**, and
    `-fexcess-precision=standard`, `-std=c23` and clang's `-ffp-eval-method=source` all
    leave the references on the wide intermediate. `2.25f16 * 255.0f16 + 0.5f16` is
    `0x607d` under per-operation rounding (mcc, and gcc with `-fexcess-precision=16`) and
    `0x607c` with a `float` intermediate (gcc and clang by default); both compilers also
    return `0x607d` the moment the intermediate is spelled with a cast or stored to a
    variable, which is what proves the difference is the evaluation format and not the
    arithmetic. So this is a documented, flag-selectable evaluation-format choice, not a
    wrong answer, and mcc is on the side that matches the declared type of the operands.
    What remains open is a **decision**, not a fix: either keep per-operation rounding and
    say so, or add the knob. The `diverge-both:{fsweep,bsweep}.F16.{FMULADD,FSCALE}` and
    `f16.*.doubleround` rows stay banked in `tests/smoke/bails.txt` so the choice cannot
    change silently.
23. **`(unsigned int)` loses its 32-bit truncation when the cast is consumed in a wider
    expression.** `(unsigned long long)(unsigned)d` for a `volatile double d = 1e300`
    yields `0x8000000000000000` under mcc and `0` under both gcc-15 and clang-22; the same
    shape with `-3.0` yields `0xfffffffffffffffd` against `0x00000000fffffffd`. Assigning
    to an `unsigned int` variable first narrows correctly in every case, so the defect is
    in the cast expression, not the conversion. The inputs are out-of-range float to
    unsigned conversions and therefore UB, so this is a quality-of-implementation defect
    rather than a conformance one — but the *type* of the expression is `unsigned int` and
    its value is outside `unsigned int`, and that leaks into anything wider that reads it.
    Banked as `diverge-both:xsweep.{F16,F32,F64,F80}.UI` and `x.uint.from.1e300`, with
    `x.uint.var.1e300` and `x.uint.var.2p32` next to them as the passing controls.
24. **`long double` to `int` does not truncate toward zero for values just under 1.** Not
    UB, no out-of-range operand:

        volatile long double one = 1.0L, two = 2.0L, ep = LDBL_EPSILON;
        volatile long double h = ep / two, x = one - h;   /* 1 - 2^-64, exact */
        x == one   ->  0   everywhere
        (int)x     ->  1   mcc      0   gcc-15, clang-22
        (long)x    ->  0   mcc      0   gcc-15, clang-22

    mcc is self-inconsistent: the 64-bit conversion truncates and the 32-bit one does not.
    The observed values are consistent with the `long double` being narrowed to `double`
    (where `1 - 2^-64` rounds to `1.0`) before the integer conversion. Found by the F80
    boundary sweep: `bsweep.F80.FSELMIX{L,R,B}` diverge from both references on exactly the
    corpus entry `1 - LDBL_EPSILON/2`, 62 of 4,096 cases.
25. **FIXED 2026-08-10 (`4d8e03c4`), and the title below is wrong: nothing is loaded twice.**
    The fix is none of the three candidate directions listed further down — it is simpler.
    The embedded JIT engine blob references **zero** libgcc symbols (all 145 of its undefined
    symbols are libc/pthread/dl), so host `libgcc.a` was dead weight on the ELF embed-jit
    path and dropping it removes the conflict instead of arbitrating it. That also closed a
    silent divergence: because libgcc always preceded libmccrt, `--embed-jit` binaries had
    been using **libgcc's** semantics for all 68 shared helpers. The two link modes now
    agree. Scope is ELF only; the PE/mingw block is untouched and unmeasured. Original
    diagnosis, kept because the mechanism is still worth reading:
    `mccrt.o` is pulled exactly once. The duplicate is between **two different runtimes**:
    `--embed-jit` adds the host `libgcc.a` (via `MCC_EMBED_JIT_GCC_LIBDIR` in
    `mcc_add_jit_engine_embedded`, so the gcc-built engine blob can resolve its own helper
    references) and mcc's own `libmccrt.a` into the same link, and **68 symbols are defined
    strong in both**. Traced member by member on the reproducer: libgcc supplies
    `__lshrti3`, `__ashlti3`, `__ashrti3` and `__fixunsdfdi`; later the embedded archive is
    scanned, `__floatundisf` is still undefined and only mcc has it, so `mccrt.o` is pulled —
    and `mccrt.o` also defines `__fixunsdfdi`. Strong vs strong, hence the error. This is
    why a small standalone does not reproduce: it only demands helpers libgcc also has, so
    `mccrt.o` is never pulled. The overlap by member is `int128.o` 35, `float128.o` 24,
    `complexabi.o` 6, `mccrt.o` 3 (`__fixunssfdi`, `__fixunsdfdi`, `__fixunsxfdi`).
    **A plain `-O2` link never scans libgcc at all**, which is why only `--embed-jit` fails.

    **The obvious fix is wrong, and smoke caught it.** Marking those three weak in
    `runtime/lib/mccrt.c` makes the link succeed and the subject run — but mcc's own
    `__fixxfdi` *calls* `__fixunsxfdi`, so the weak symbol rebinds to libgcc underneath it
    and the result is a hybrid runtime that matches neither: `smoke/native` on that build
    reports `FAIL xrun x.sll.from.ldbl.1e300 got=0000000000000000 want=8000000000000000`
    and a changed digest, against `failures=0` for the plain link. In-range conversions were
    identical across mcc-plain, mcc-embed-jit and gcc-15 (`i25-conv 8a5c06b28f6b2a4a`), so
    the divergence is confined to the out-of-range UB cases the two runtimes answer
    differently. **Any fix here has to decide which runtime owns a shared helper and keep
    mcc's internal callers on one side of that line**; weak binding alone does not, because
    it silently splits a helper from its own wrapper. Note also that the hybrid already
    exists today for the symbols libgcc happens to supply first (`__ashlti3` and friends) —
    it is only invisible because those links do not error.

    Three candidate directions, none taken: give each shared helper a private
    `__mcc_`-prefixed definition that internal callers bind to plus a weak public alias;
    split `int128.o`/`float128.o`/`complexabi.o`/`mccrt.o` to per-helper members so a pull
    never drags an overlapping symbol; or stop adding host `libgcc.a` to the embed-jit link
    and serve the engine's helper references from `libmccrt.a`, which keeps one runtime and
    one set of answers if it covers what the gcc-built engine needs. Original entry:
    **`--embed-jit` loads an archive member twice when a softfloat helper is demanded in
    both link phases.** Any translation unit large enough to reach the JIT boot path and
    containing a single float-to-`unsigned long long` conversion fails to link:

        <embedded libmccrt.a>: error: '__fixunsdfdi' defined twice

    `__fixunssfdi`, `__fixunsdfdi` and `__fixunsxfdi` are each defined exactly once, in
    `mccrt.o`, so the member itself is being pulled in twice; `mcc_error_noabort("'%s'
    defined twice")` in `src/objfmt/mccelf.c` is the report site. Reproduced on **`main`**
    by adding six lines to `tests/smoke/subject.c` — one `static unsigned long long
    f(void) { volatile double d = 12.5; return (unsigned long long)d; }` called from
    `main` — and compiling `mcc -O2 --embed-jit`; it is not caused by the new tables. A
    small standalone with all three helpers does *not* reproduce it, so the second archive
    scan is reached only in a larger link. **Cost while it is open: the float-to-`unsigned
    long long` leg of `tests/smoke`'s conversion sweep cannot exist**, because the smoke
    subject is compiled with `--embed-jit` by the jit census arm. `SMX_F2I_TYPES` in
    `tests/smoke/fcases.h` is `{int, unsigned, long long}` for that reason alone; add
    `unsigned long long` back the day this lands. Note that mcc and clang agree and gcc
    differs on `(unsigned long long)1e300` (`2^63` vs `0`), so there is a real divergence
    sitting behind this hole.
26. **FIXED, 2026-08-10, and the diagnosis below is wrong in three places.** The cause is
    not a stack-slot overlap and has nothing to do with the locals sharing names: the arena
    replay consumes `ast_alloc_loc`'s recorded frame offsets positionally without checking
    that the entry fits, so once an optimizer drops a request the next `_Complex double`
    lands on an eight-byte `_Complex float` slot. See the landed section near the top of
    this file for the arithmetic, the blast radius and the one guard left unexercised. The
    folded `smc_run` is back in `tests/smoke/fcases.h`, so the shape is covered again and
    the cell is proven to fail without the fix. Original entry, kept for the record:
    **`_Complex` addition is miscompiled at `-O2` and above when same-named locals of
    different types live in sibling blocks.** The smoke `smc_run` was one function with
    three `if (tag == ...) { ... }` blocks, each declaring `volatile CTY vr, vi, wr, wi`
    for `CTY` = `float`, `double`, `long double`. At `-O0` and `-O1` every row is right; at
    `-O2`, `-O3` and `-O4` the `double` and `long double` blocks read `wr` as if it were
    `vi`, so `c64.add` on `(1.5, -2.5) + (0.25, 4.0)` returns `-1.0 + 1.5i` — `ar + ai`
    for the real part — instead of `1.75 + 1.5i`. The `float` block, which is first, is
    always correct. Which rows break moves when unrelated locals are added or removed,
    which is what a stack-slot overlap bug looks like rather than a fold bug: `volatile` on
    the row operands does not help, and the sweep, which calls the same function with
    runtime values, is correct at every level. Splitting the three blocks into three
    functions (`smc_run_C32/C64/C80`) cures it completely, and that is what
    `tests/smoke/fcases.h` now does — so **the shape is no longer covered by any cell**.
    Reproducer: `tests/smoke/fcases.h` at `wt/smokedepth`, with the three `SMC_BODY`
    expansions folded back into one function, plus a nine-line driver that calls
    `smc_run(smc_rows[0].tag, ...)` and prints the result.

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *Clean bills of health, because those are results too*.

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

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *Registration gates with no `else()` — closed on `wt/regstub`, and it was 537 cells, not 93*.

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

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *LANDED — the gated half of `ast/o0-baseline`, and the thirteen files that held nothing*.

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

### What is still open, with honest sizes

Four rows were added 2026-08-09 by the re-derivation at the head of this file; they are at
the top because three of them are the only rows here in a currency that converts.

| row | size, and the tool that produced it | currency |
| --- | --- | --- |
| ~~`ast_loop_interchange_legal` / `ast_dep_fusion_pair_illegal` call `ast_dep_base_distinct` with **no `indirect` guard**~~ | **CLOSED 2026-08-10, and the row was wrong twice over.** The guard was moved *into the callee* by `adf08e3b` (2026-08-09) and is now `src/mccast.c`: `if ((r1->indirect \|\| r2->indirect) && !allow_indirect) return 0;`. **The row also stated the polarity backwards.** `allow_indirect` is the *permissive* parameter, so the three emitting callers — `ast_loop_interchange_legal` (`src/mccast.c:13890`), `ast_dep_fusion_pair_illegal` (`:13940`) and `ast_rgn_pair` (`:14544`) — all pass `0` and are the *guarded* ones; the permissive caller is the **census** predicate `ast_loop_parallel_legal` (`:14321`), which passes `ast_dep_alias_oracle_env`. That is the inverse of "unlike the census predicate they reach emitted code". Second-order: the prescribed measurement ("a fuzz corpus of `p[i]`/`q[i]` nests at `-O12`") is **unrunnable on a stock build** — `-O5`–`-O12` are a hard error without `MCC_DEV` (`src/libmcc.c:2534`), and all three flags are `MCC_OPTD_DEV(MCC_OPTD_LEVEL(12))` (`src/mccopt.h:116-118`), so they are unreachable anyway | **correctness** |
| ~~`tests/optfire/levelbench.tsv` is stale by a generation and has no `--check`~~ | **CLOSED 2026-08-09 (`wt/ladder2` then `wt/gatefin`).** `wt/ladder2` re-measured it: the banked table is a fresh 16-row run matching `src/mccopt.h`'s 16 `LEVEL(1..3)` rows, with `gain_movers_pct`/`eff_movers` beside the diluted columns and a signed efficiency (`inline-functions`, `gain_movers` −1.96, fell from rank 4 to rank 9). `wt/gatefin` closed the fourth defect **in both its halves**: `--check` exists with `optbench/levelbench-bank` + a known-positive, and the `optlevel-bench` cell now compares its build-dir TSV back to `tests/optfire/`. See the audit section | **census trust** |
| the ~17× dilution of `gain_pct` (filed, not fixed, on `wt/benchtrap`) | **CLOSED 2026-08-09 by re-running the bench.** Measured dilution up to **17.4×** (`builtin-math-prepass` 0.3007 all-kernel vs **5.2372** over its one mover). `trunc32` moves 17/17 and its two gain columns agree to the digit, which is the control. It also **flipped a bucket** — `builtin-copysign`'s real **1.0076%** win read 0.0590% and was filed `cost-no-gain`, the bucket asserting no gain was found | **census trust** |
| `narrow` / `tree-copy-prop` "ranked on nothing" | **THE PREMISE WAS FALSE, and the measurement was taken anyway.** Both were already priced on the self-host axis in `levelpins.txt:196,227`; the `n/a` in `levelbench.tsv` was a **stale row for a flag that table no longer sweeps** (levels 10 and 11 against a `<= 3` filter). Re-taken n=25 paired: `narrow` +0.876% stage-1 for −0.0088% stage-2 (**100:1 against**, 25/25 reps), `tree-copy-prop` +0.799% for −0.0048% (**166:1**, 24/25). `rir-coverage` clear for both. **Levels unchanged** | emitted code |
| ~~float in the slice engine and the SPIR-V emitter~~ | **CLOSED 2026-08-10 — this row was stale and contradicted row 6 of the ranking table above it, which has said LANDED (`wt/fpwidth`) since 2026-08-09.** The code sides with the ranking table: `grep -c Float src/mccgpu.{h,c}` is **5** and **4**, not `0/0/0` — `SpvOpTypeFloat` (`src/mccgpu.h`), `SpvCapFloat64` (`:1316`), both emitted at `:1445-1446`, and `shaderFloat64` queried and enabled at `src/mccgpu.c`. Read row 6 above for what actually shipped and what stayed excluded | device-executable lanes |
| the device dispatcher | **Not merely absent — unwritable from what exists.** The compiler's two slice sites pass `nlive = 0` / one tuple; `mcc_slice_work_from_ast` refuses `cnt < 1`. Plus a write-back (`MCC_SLICE_MAXSLOT` = 16 dense 8-byte slots; a 600×600 tile does not fit one) and a per-compile correctness gate. Three subsystems, priced nowhere | device-eligible blocks |
| chain-store re-promotion (row 2) | **MEASURED 2026-08-09, and the answer is no.** The `kept` prize is **+2.60** points, not ~4.3 (`tools/rir-coverage.py`, `self`, `-O1`/`-O2`/`-O3`); the emitted-code half is stage-2 **−0.079%** and sieve **−1.97%** `instructions:u`, for **+1.50%** of stage-1. Pins unchanged | emitted code |
| `-O11` ICE, `vstack leak (1)` | **FIXED 2026-08-08**; it also silently miscompiled (6 wrong answers / 500 at `-O11`). Cell `exec-chainlive/*`, fuzz 900 programs 0/0 | correctness |
| `vstack leak (-1)`, debt #6a | **FIXED 2026-08-09**. It fired at shipped `-O1`/`-O2`/`-O3` (78/77/77 of 500) and never miscompiled — every imbalance is `(-1)`, so it always aborted. Cells `exec-storevalrot{1,2,3}/*`, fuzz 1400 programs 0 ICE / 0 wrong. Stage-1 `.text` byte-identical, so it cost nothing | availability |
| `storeval-rot` pays negative at `-O3` | **CLOSED 2026-08-09, level unchanged at 1.** The 1.69% is cold inline-clone bytes: with `-fno-inline` the same gap is **+124**, and the flag's *dynamic* cost is 0.232% with the inliner and 0.245% without it. Turning it off takes `rir-coverage` **red by 8.7 points** of `kept`, because its off-state is an incomplete replay path, not a lowering | emitted code |
| replay recompute reads a written target | **FIXED 2026-08-09 with #6a**: `c = 2 * (a = s + a)` returned the wrong answer under the shipped `-fno-replay-fallback` at `3ddd9933` with every `storeval-*` and `chain-store` flag off. Covered by `flagsweep-exec/replay-fallback` | correctness |
| `rir_op_effect`'s clear (row 4) | **CLOSED 2026-08-08**, and the "<1%" bound was wrong: measured **−8.30% / −9.44%** of stage-1 `-O3`/`-O2` CPU time (n=21 interleaved, ±0.55% floor), `instructions:u` −12.29%. 1,464 objects byte-identical | compile time |
| Metal, debt #4 (row 3) | ~~**DECIDED 2026-08-09: dropped.** Re-counted: 1754 vs **3612** lines (the 3578 was stale by `99e043c1`, and the MSL total is unchanged because nothing is added to it), **2**-line kernel arm, 0 `msl_region*` symbols against **31** `spv_*`. A rewrite, not a fix~~ — **REVERSED BY DECISION 2026-08-09 (`wt/metalspec`).** Re-derived: **1755 vs 3960**, **29** not 31, kernel arm still **2** lines. Priced at **1,530–2,360** lines behavioural / **2,200–3,400** with fp64; spec at the head of this file | ~~a decision~~ a priced plan with no CI differential |
| D4b leaf-inline pool cap (row 5) | ceiling **803** blocks, `slicerun --census`. **Cap removed 2026-08-08**: reach 41 → 72 grafts, and it delivered **one** more block (10,381 → 10,375 `inv-blocks`). Row closed, not advanced | device-eligible blocks |
| `snprintf` module budget (row 6) | **14 of 162** refused, was 22. The narrow 32-bit conversion path landed 6 sites 2026-08-09 (`142 → 148`, and `140 → 142` was a census bug, not work). 9 on the budget, 4 on flags, 1 on float. The closest miss is 762 words | device-accepted sites |
| the literal-run packing lever (row 6) | **MEASURED AND DROPPED 2026-08-09.** The filed "7 pure-literal refusals" do not exist — 5 of 15 carry no `%s` and every one of them carries integer conversions. 11 of 15 were over budget with *zero* literal bytes, so packing had a **4-site ceiling even if a literal byte were free**; the narrow conversion path then took 3 of those 4, leaving the lever with **one site** and a delicate emitter rewrite. Not implemented, deliberately | device-accepted sites |
| `tools/fmt-census.py` was an ungated second implementation | **CLOSED 2026-08-09.** It had already drifted: one item per literal byte instead of merged runs, 61 disagreements over 984 corpus formats, 2 over the 162 `snprintf` sites. Now gated by `fmt/census-oracle` (41,017 formats vs the real `mcc_fmt_compile`) and `fmt/census-oracle-known-positive` | census trust |
| the fence wait (unranked) | all-VRAM 1.0–20.2 ns/lane against all-sysmem 16–110 — **PROSE-ONLY**; no staging path exists and no payoff was estimated | device time, no subject |
| debt #1, `--mutate` blind to `memcpy` | smaller than filed: four of six operator sites already perturb written memory and `g_frame_mismatch` already exists. The real gap is that **no `memcpy`/`memset` exists in the slice corpus to mutate** | test strength |
| indirect callees | 115 blocks, and no device answer anywhere in `docs/ARCHIVED.md` | device-eligible blocks |
| recursion on device | **no data at all** beyond the ~130-function SCC and the dynamic depth figures (`unary()` peaks at 11, `ast_replay_bb` at 35, 1.75–3.5 MiB/lane) | unknown |
| debt #3, descriptor staleness | fixed, and **unreachable until binding 2 grows** — both callers pass the constant `MCC_VK_MEM_DEFAULT`, so no cell is possible yet | latent |
| `pe` lowerable floors | stale-low; could not be re-banked from an ELF host, so they under-gate on Windows rather than false-fail | gate strength |

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *~~The recommendation on direction~~ — SUPERSEDED 2026-08-09*.

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

The mechanism: `do_inline` requires `!ast_inline_pass_env` (`src/mccast.c`).
`INLINE_FUNCTIONS` is `MCC_OPTD_LEVEL(2)` and `OPT_PERFN_INPROC` is `MCC_OPTD_LEVEL(8)`
(`src/mccopt.h`, `:124`) — so the flag's own level is **six rungs above the level that
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
(`src/mcc.c`) scores on `so_fn_sizes`, the real emitted per-symbol size. Until the
in-process trial scores on something that predicts the object, no tier is justified.
Recorded so the next attempt starts from the metric, not from `levelpins.txt`.

### Open items 4 and 5 — ratios over the compiler's own source

Confirmed and sharpened: the same `src/mccgpu.c` edit moved `nodes_pct_loose` **down**
0.065pp while moving `bodies_pct` and `bytes_pct` **up** — one source edit moved two
banked ratios of the same census in opposite directions. Neither direction is a signal.

Two findings the items did not contain:

- **`corpus_config` has a hole.** `CORPUS_DEFS = ["MCC_DIAG"]` (`tools/rir-coverage.py`)
  is one entry, but `MCC_EMBED_JIT` (a user-visible CMake option, default ON) gates two
  whole translation units into `src/mcc.c` (`src/libmcc.c`). The guard that exists
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
(`CMakeLists.txt`), so it covers exactly one of the eight modes and none of the
run-side ones — which is precisely why "objects differ under all five target compilers"
did not settle it.

## Open now — the coroutine task, 2026-08-08

> Context: [`docs/ARCHIVED.md`](ARCHIVED.md) was reframed on 2026-08-08. The GPU is no longer a
> replacement execution engine; it is a **second executor behind the JIT's existing
> scheduler**, and a slice runs on the device only when `mccjit_bench_pair` measures it
> faster *including upload, dispatch, download and readback*. That is cluster S. This
> task is **S7b**, and it is the one item in cluster S that is worth doing on its own
> merits whether or not the GPU plan is adopted at all.

### Replace the C11 threading implementation with a single-threaded coroutine that ticks

**The claim.** Four separate open problems in this tree are the same object wearing four
names, and one task representation closes all of them:

1. ~~**The JIT pool has no shutdown** (L2′, `docs/ARCHIVED.md` cluster L)~~ — **AMENDED
   2026-08-10, and this item overstated its own claim.** The pool now drains and joins
   (`mccjit_shutdown`, `wt/jitshutdown`) **without** a task representation. The claim that
   *"a quit flag against an opaque `job->run(job)` that holds the process-global
   `mccjit_swap_lock` across an entire compile is a redesign"* is true of a **cancel** and
   false of a **drain**: `MccjitSwapJob` is run-to-completion with no resume state, so the
   flag is only ever checked where the worker is already suspended on the condvar and
   already between jobs. Nothing is checked inside `job->run`. What survives, and is the
   real S7b claim here: the teardown is **unbounded above** (one `job->run` per worker,
   and `L4b`'s own worst case is a multi-second module build), and **`L4b`'s hard
   constraint** — no `mccjit_swap_lock` across a device dispatch — still needs the lock
   narrowed to the codegen inside a tick.
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

- ~~**No coroutine, fiber, `ucontext`, `makecontext`/`swapcontext`, generator,
  continuation, scheduler, event-loop or ticking-task abstraction. Zero hits, all
  spellings.**~~ **This census is FALSE as of 2026-08-10 and is the stale half of this
  section.** `src/mcctask.h` now defines exactly the "Recommended shape" this section
  goes on to propose: `MccTask{tick, ctx, state, resume, ticks, next}`, `MccSched`,
  `mcc_task_init`, `mcc_sched_step`/`_run` — a switch-on-resume state machine, not
  `ucontext`. It already has consumers: `src/mccthread.h`, and
  `mcc_slice_tick_gpu`/`_cpu` at `src/mccslice.h`, driven from
  `tools/slicerun.c`. **The representation is built and proven adequate
  where it was built.** What remains open is only the *conversion* work below — items
  C3/C4/C5: `MccjitSwapJob.run` is still `job->run(job)` (`src/mccjit_embed.c`),
  `runtime/include/threads.h` still has no single-threaded backend and `mtx_timedlock`
  is still the 1 ms `nanosleep` spin (`:144`), and `tools/mcchv.c` still uses
  `thrd_create`/`thrd_sleep` (`:281,314,946`). Note also the anchor below has drifted:
  `MccjitSwapJob` is at `src/mccjit_embed.c`, not `:1288-1298`.
- The only *pre-existing* task-like construct was `MccjitSwapJob`: a `void (*run)(job)`
  on an intrusive FIFO, run to completion, no resume state.
- `setjmp`/`longjmp` exists but is **only** error unwinding (`mcc.h:32`, `libmcc.c:863`)
  and the public entry for running JIT'd `main` (`include/libmcc.h`). Every `longjmp`
  unwinds outward and discards; none is a continuation.
- Threading in the compiler proper is **pthreads only, no `<threads.h>`**: 139 tokens in
  `mccjit_embed.c`, 17 in `mccrun.c`, 6 in `mccast.c` (`ast_search_pool_pthreads`), 4 in
  `mccgpu.c`. Windows is a pthread-shaped shim (`src/mccjit_win32.h`, SRWLOCK /
  CONDITION_VARIABLE / INIT_ONCE / `_beginthreadex`).
- Atomics are GCC `__atomic_*` builtins with explicit ordering, not `<stdatomic.h>`. The
  ordering that matters is QSBR epoch publication (`:1813-1854`) and the three
  release-stores that publish freshly emitted code (`:5841`, `:6724`, `:9420`).

**The suspension points a conversion has to name.** These are the blocking calls inside
worker threads today:

| worker | site | what blocks |
| --- | --- | --- |
| `mccjit_pool_worker` | `src/mccjit_embed.c` | unbounded `pthread_cond_wait` (`:1347`), then `mccjit_swap_lock` held across the whole job |
| `ast_search_thread_fn` | `src/mccast.c:16877` | full AST re-optimization per candidate; can reach `flock(LOCK_EX)` at `:15546` and file I/O |
| `mccjit_qsbr_thread` | `:6976` | `nanosleep` 1 ms via `mccjit_pool_nap` (`:5731`) |
| `hv_optimizer` | `tools/mcchv.c` | `thrd_sleep` 5 ms (`:281`), `thrd_yield` (`:279`), `flock` (`:487`), `fsync` (`:503`) |
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

## Blocking — bugs and free wins from the decision investigation, 2026-08-08

Eight parallel investigations closed the fourteen open rows in
[`docs/ARCHIVED.md`](ARCHIVED.md)'s decision table (recorded there under *Decisions resolved*).
They also turned up nine defects and free wins that have **nothing to do with whether
that plan is adopted**. Ordered by severity; the first is a memory-safety bug.

**Read the host note first.** The 2026-08-07 study was measured on an Apple M1 Pro
through MoltenVK. This machine is Linux x86_64 with a **discrete NVIDIA RTX 5070 Ti**,
validation layers, `spirv-val` and `glslc`. Numbers below are from this host unless
stated, and they do not always match the M1's — the re-measured allocation census is
**2.45×** the M1 figure.

1. ~~**`MCC_AST_EVAL_LADDER_GPU=1` smashes the stack. One-line fix.**~~ **FIXED `1f7f6257`
   (2026-08-08).** `src/mccast.c` now declares
   `int32_t pin[64 * MCC_GPU_IN_SLOTS], pout[64 * MCC_GPU_OUT_SLOTS]` — exactly the fix
   prescribed below. It sat here unstruck for three days as the top memory-safety row of this
   list. *(2026-08-11 validation sweep.)* Original text:
   `src/mccast.c` declares `int32_t pin[64], pout[128]` — 256 B and 512 B, sized
   for the pre-`989e4b3b` ABI of 1 in-slot / 2 out-slots. `MCC_GPU_IN_SLOTS` is now 2
   and `MCC_GPU_OUT_SLOTS` is 3, so the warm-up dispatch **reads 512 B from the 256 B
   buffer** (`src/mccgpu.c`, Metal `:352`) and **writes 768 B into the 512 B one**
   (`:1510`, Metal `:388`). Under glibc's stack protector that is a hard SIGABRT, so
   **zero dispatches happen on any Linux host with a real ICD**; on Darwin it is a silent
   256-byte stack overwrite that happens to survive. `gpu/ladder-gpu-parity` fails
   correctly here via its `_disp EQUAL 0` `FATAL_ERROR`. **CI cannot see it**: the Linux
   cell installs `libvulkan-dev` — loader and headers, no ICD — so the cell green-skips.
   Fix: `int32_t pin[64 * MCC_GPU_IN_SLOTS], pout[64 * MCC_GPU_OUT_SLOTS];`

2. **The Vulkan dispatch frees resources under a live command buffer.**
   `src/mccgpu.c` waits 30 s; on any non-`VK_SUCCESS` it falls to `done:` at
   `:1515-1535` and destroys the fence, command pool, pipeline, pipeline layout, shader
   module, descriptor pool, descriptor set layout, mappings, memory and buffers. On
   `VK_TIMEOUT` the command buffer is still **pending**. Because buffers are created
   fresh per dispatch, the driver recycles the freed allocation into the next dispatch's
   `bin`/`bout` while a zombie kernel writes into it — **a timeout in dispatch N silently
   corrupts dispatch N+1**. Alongside: all ~13 Vulkan failure exits are diagnostically
   mute, and ~~`mcc_gpu.ok` is never cleared after `VK_ERROR_DEVICE_LOST`, so
   `mcc_gpu_quiesce`'s unbounded `vkDeviceWaitIdle` from `atexit` deadlocks after a hang.~~
   **That clause is CLOSED by `747709bc`** — `src/mccgpu.c` sets `mcc_gpu.lost = 1;
   mcc_gpu.ok = 0;` on device-lost and the quiesce no longer waits. **The `submitted`-flag
   half below is the part that is still open** (`grep -c submitted src/mccgpu.c` = 0), which
   is exactly the split *Three items that read as closed and are not* warns about.
   *(2026-08-11 sweep.)*
   The restructure needs a `submitted` flag and a second label that **destroys nothing**
   — leaking one dispatch's resources at a terminal error is strictly safer than a UAF
   against the GPU, and it is bounded because no further dispatch can occur.
   **Note the Metal half of this is already fixed** (`c6814625`); `docs/ARCHIVED.md`'s old
   "`src/mccgpu.c`" paragraph was stale and has been corrected.
   **Neither bug is reachable by any existing test** — `src/mccgpu.c`'s Vulkan path has
   zero direct coverage, since `gpu/spv-slice-*` use `spvgate`'s own duplicated Vulkan
   code. Cheapest regression test: make the hardcoded 30 s fence a named tunable and run
   one cell at 1 ns with validation layers on. Works here and on lavapipe, needs no fault.

3. **`ast_replay_bb`'s 35 KB frame is 93% one array. One declaration, 9.1× less stack.**
   `SValue sv_stack[VSTACK_SIZE + 1]` (`src/mccast.c`) is 32,832 B — `VSTACK_SIZE`
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
   `gen_op` rewrites `TOK_GE`→`TOK_UGE` at `src/mccgen.c` *after* the arena records
   the token — the same mechanism behind `ee1fa9e0`. Measured 0 occurrences across
   24,562 harvested nodes. This is 23% of the binary-op axis, findable by **enumeration
   alone**, no fuzzing needed.

5. **`spvgate` reports `OK` for a case that lowered nothing.**
   `tools/spvgate.c` prints `SKIP (not lowerable)` and `continue`s; `:1335`
   then prints `OK` because `case_bad` is still 0. That is the
   `cmake/ladder_gpu_parity.cmake` "zero dispatches, so identical verdicts prove nothing"
   failure mode, un-closed one level down. It matters beyond hygiene: it means the claim
   *"the synthetic suite showed 0 mismatches either way — only real arenas discriminate"*
   **cannot currently be distinguished from "the synthetic cases compared 0 points"**,
   and `b_ll_cmpu` does build the discriminating shape. ~10 lines: print per-case
   `compared=` and fail a case at 0. Do this before trusting any generator's yield.

6. ~~**`354e96f6` is half-landed, and it broke dump reproducibility.**~~ **BOTH HALVES
   CLOSED (2026-08-11 sweep):** `tools/spvgate.c` now `sscanf`s 14 fields with graded
   `nf < 7 / 12 / 13 / 14` fallbacks, and the dump interns pointers through
   `ast_adump_intern` (landed `1b54c26e`), so the ASLR non-determinism is gone. Original
   text:
   The dump emits 12 fields plus `[inv]` callee lines (`src/mccast.c`), but
   `rebuild_arena` still does `sscanf(...) != 7` (`tools/spvgate.c`) — the five new
   fields have **no consumer**. Worse, two of them are raw `(uintptr_t)Sym*`, so two
   identical self-compiles now differ under ASLR (identical under `setarch -R`; the
   7-field prefix is still identical). **That invalidates the H4′ evidence** in
   `docs/ARCHIVED.md` — "three `MCC_ARENA_DUMP` self-compiles byte-identical across all
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
   `memset(pout, …)` (`src/mccgpu.c`, Metal `:353`) is **100% dead** — every lane
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
   `src/mccgpu.c` takes the **first** type matching
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

Curated from [`docs/ARCHIVED.md`](ARCHIVED.md), which proposes moving AST/RIR execution onto the
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
   `op == 2` increments loop depth (`:4136`). So `src/mccgpu.h`'s `nchild == 3`
   gate is **refusing statements, correctly** — the honest figure is that only **11.5%
   of `AST_If` nodes are ternaries at all**, not that 66% are wrongly refused.

   The datum is still valuable, but it argues for something else: **73% of `AST_If`
   nodes are statement control flow**, and reaching them needs the control-flow machine
   (cluster C in `docs/ARCHIVED.md`), not a wider expression emitter. Re-file it there.

1. **Bank the baseline node census — static and dynamic — before any device work.**
   Cheap, zero-risk, and it makes every later claim interpretable. The dynamic half
   does **not** need an interpreter, contrary to what the plan first assumed:
   `mcc -ftest-coverage` self-build (0.5 s) then a self-compile (0.93 s, 10× baseline)
   emits a 4.7 MB gcov-format `.tcov` (`src/objfmt/mccelf.c`) —
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
   `src/mccgen.c` but **the arena records the pre-rewrite token** — `unsigned
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

   **Sites verified 2026-08-07** — `src/mccgpu.h` (MSL),
   `:1382,1394,1404,1419,1432,1444,1479` (SPIR-V), `src/mccast.c` (ladder
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
   modelled by the `RIR_M_LOAD` mark. And the `continue` at `src/mccrir.c` is not
   LOAD-specific — removing it runs `rir_reconcile` mid-region and truncates the
   shadow stack: **used 2657 → 1833, 882 bodies mismatching, kept 84.3% → 34.2%.**

3b. ~~original item 3 text~~ — superseded. Add a **per-opcode histogram** at
   `src/mccrir.c`, excluding `JMP`/`JMPCOND`/`JMPADDR`/`JMPAPPEND`/`GSYMADDR`
   (which are handled in other switches and are 68% of that arm's traffic). Then handle
   `RETVAL`, `MKPTR`, `VPUSHSYM`, and unblock `LOAD` from the `continue` at
   `src/mccrir.c`. **Four features, not 25** — 21 of the 25 fire zero times on
   this target and five are `#ifdef`-ed out on arm64.

4. ~~**Quote `kept_coverage`, not `modelled`.**~~ **DONE, and the diagnosis was
   half-wrong.** The tool already *printed* both (`tools/rir-coverage.py`); what
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
   were deleted.** `cmake/ladder_gpu_parity.cmake` matches `available=0` and
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
   `MSL_MAX_CONST` = 512 distinct constants (`src/mccgpu.h`, `:105`) — a 2049-node
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
    case; and `tests/exec/codegen/nodata_wanted.c` label *arithmetic*
    (`&&te0 - &&ts0`), which no device model handles.

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

## The configuration surface moved: read this before running any recipe below

**Every `MCC_AST_*` and `MCC_RIR_*` gate is now a `-f` flag.** 115 of them, generated
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

> **Corrected 2026-08-10.** `MCC_AST_BITFLAG` used to sit in this table as the retired
> spelling of `-ftree-switch-conversion`. It is **not retired**: `src/mccast.c`
> reads it as `ast_bitflag_min = ast_env_int("MCC_AST_BITFLAG", 5)`, repurposed from a
> boolean gate into a numeric threshold (clamped up to 5 below 3). `MCC_AST_BITFLAG=8`
> changes behaviour today, so it belongs in the paragraph below, not here. The other six
> rows above are correct — those names have no reader left.

**Still environment variables, deliberately.** The numeric tuning knobs
(`MCC_AST_CSE_WINDOW`, `MCC_AST_TILE_SIZE`, `MCC_AST_INLINE_DEPTH`, …) and the RIR
measurement handles this file's censuses are built on — `MCC_REPLAY_IR`,
`MCC_RIR_PROD`, `MCC_RIR_FORCE`, `MCC_FORCE_REPLAY` — are unchanged, so every
`[rir-prod]` / `[rir-total]` recipe still works verbatim. So are the `MCC_JIT_*`
knobs: the embed JIT runs inside programs mcc *compiled*, where a compiler flag
cannot reach it, which is the same reason `OMP_NUM_THREADS` is an environment
variable.

**`-O` is a ladder now, not a dial.** *Rewritten 2026-08-10 — the previous wording was
wrong on all three of its clauses, and this file already contradicted it in two other
places (the `wt/o4fold` and `wt/o4ticks` write-ups, both now in
[`docs/ARCHIVED.md`](ARCHIVED.md)).* What is actually true of the tree:

- **The shipped levels are 1, 2 and 4.** `src/mccopt.h` carries non-dev
  `MCC_OPTD_LEVEL` rows for those three classes only; **3 and 7 have no rows at all**.
  `-O4` is the top shipped rung — `opt_level_top()` (`src/libmcc.c`) returns 4
  without `MCC_DEV` and 12 with it.
- **`-O5`–`-O12` are `MCC_DEV`-only and are a hard error otherwise**, not an
  in-development ladder anyone can climb: `opt_level_reject()` (`src/libmcc.c`)
  rejects them ungated and names `MCC_DEV=1` as the escape.
- **Only `-O13` runs the strategy search, and its budget is ticks, not seconds.**
  `MCC_OPT_SEARCH_LEVEL` is 13 (`src/mccopt.h:14`) and the budget knob is
  `MCC_OPT_SEARCH_TICKS`, defaulting to 1 (`src/mccopt.h:15`); `-O14` and above are
  refused outright as "not an optimization level".

Anything in this file that says `-O4` meaning "search for 4s" means the shipped rung;
the search entry is `-O13`. A recipe that asks for `-O12` on a stock build does not
measure a weaker optimizer — it fails to compile. `MCC_OPT_SEARCH_LEVEL` in `mccopt.h`
is the single definition, and if a knob is ever placed at or past it `ast_opt_defaults`
raises `mcc_error` (`src/mccast.c:2245`) — a runtime failure on every compile, not a
build-time one, so a bad row ships and then fails loudly rather than failing to link.

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
spellings (114/114). `flagsweep-exec/<flag>` — **114 cells, one per `MCC_OPT_ROW`** —
turns the flag on and off at `-O2` and checks twelve exec goldens still compute the
`-O0` answer. `flagsweep-cover/<row>` runs the same corpus under a **3-way covering
array**, opt-in behind `-DMCC_FLAGSWEEP_FULL=ON` and the `flagsweep-full` label.
Together they cost **4.2s wall at `-j32`** and the covering array another 6.6s.

The table has **115 rows**, and **`flagsweep/cover3-verify` is RED on `main` right now
because of it — see the box below.** *Corrected 2026-08-10: this used to read "113 rows, not 115"
and explain the difference as a doc comment at `mccopt.h:63` plus the `#define` at
`:181`. Both citations were wrong — `src/mccopt.h` is 144 lines long, so there is no
`:181`, and `:63` is a real `MCC_OPT_ROW(REG_DISP, …)` row rather than a comment.
`grep -c 'MCC_OPT_ROW(' src/mccopt.h` is 115; subtracting the one `#define` at `:138`
leaves 114. `tests/optfire/cover3.txt` states the same figure independently as
`flags=114`.*

> **RED ON `main`, found 2026-08-11 in the validation sweep — the newest wave broke a
> committed bank and nothing said so.** `69296b85` added `MCC_OPT_ROW(OPT_SEARCH_PREDICT,
> "opt-search-predict", …)` to `src/mccopt.h` and did **not** regenerate
> `tests/optfire/cover3.txt`, which still says `flags=114`. `cover3.py verify` compares the
> committed array against `table_flags()` and fails:
> `FAIL cover3: the array does not match src/mccopt.h / flags in the table, absent from the
> array: opt-search-predict`. Confirmed by running the cell: **`flagsweep/cover3-verify`
> fails, rc=1.** The count is now **115** `MCC_OPT_ROW(` rows minus the one `#define`… which
> is 115 distinct flag *names* by direct extraction, so this paragraph's "115 − 1 = 114"
> arithmetic no longer holds either and both numbers moved for the same reason.
>
> **Do not blind-regenerate it.** `cover3.py gen` will make the cell green in one command,
> but this file's own standing rule is that a bank is a claim, not a cache: re-banking
> without saying what changed is how `ast/rir-c2-*` got mis-filed as a stale bank when it was
> a regression. The new row is a genuinely new flag, so regeneration is very probably right —
> it just has to be a deliberate commit that says so, not a side effect.

**34** flags were referenced nowhere in
`tests/`/`tools/`/`CMakeLists.txt` before this, counting a reference as the name on a
token boundary anywhere — `-f<name>`, `-fno-<name>`, or a bare `|`-field in the optfire
data files. Reading it strictly as "something passes this spelling" (`-f`/`-fno-` only)
the figure is **58**. The recorded 43 does not reproduce under either rule. Every one of
them owns a cell now — this is the population `-fjit-splice` came from. The first version of that harness used two synthetic programs and
*passed* `-fjit-splice`; it uses real goldens now and fails it. A sweep that misses the
bug you already have is worse than no sweep, because it reads as coverage.

**The covering array is a 3-wise guarantee, not enumeration.** Exhaustive three-deep is
C(114,3) × 2³ = 1,923,712 configurations. `tests/optfire/cover3.txt` is **74 rows** such
that every one of the 1,633,248 three-flag settings of the 108 varying flags appears in at
least one row — the identical guarantee for any bug needing three flags or fewer, and no
guarantee at all for one needing four. **108, not 114: six flags are pinned, so a bug that
needs one of them at its non-default setting is outside the guarantee by construction.**
*Corrected 2026-08-10: the base was stated as 113 and the varying count as 107, and a
later paragraph in this same section said the array "pins two flags", contradicting the
"six pinned" here. `tests/optfire/cover3.txt` settles all three — its header reads
`flags=114 vary=108 rows=74`, so six are pinned.* It is built by deterministic IPOG (`cover3.py gen`, no RNG, byte-identical
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
- ~~**`stage2 / windows / dynamic` CI was red (40 cells).**~~ **Closed 2026-08-09 down
  to a residual four.** The bulk were Windows-portability defects, now fixed and pushed:
  - `exec/fabs_edge` (×24): `mccmath.c` gated the single-precision math shims on
    `__i386__`, so x86_64/arm64 Windows had no `fabsf`/`hypotf` (the UCRT does not export
    them) — unresolved at link. It also imported the UCRT `fabs`, which returns a negative
    NaN with the sign bit set; the bit-exact golden rejects it. Now `fabs`/`fabsf` route
    through the sign-clear intrinsic and `hypotf` through `_hypot` for non-i386 Windows.
  - `fmt/census-*`, `docs/refs` (×6): `fmt-census.py` and `docref-lint.py` compared
    POSIX-style paths against native (backslash) `os.path.relpath`/`glob` output, so on
    Windows every source/citation read as missing. Normalised both sides.
  - `fmt/arena-census`, `slice-census`, `opt-determinism` (×5): these extract the build's
    `-D/-I` flags for `src/mcc.c` from `compile_commands.json` with `shlex.split`. On
    Windows the command carries backslash paths (`-IC:\...`) and POSIX `shlex` eats them
    as escapes, so every `-I` collapses and the stage2 compile fails with
    `libmcc.h not found`. Decode the path backslashes first, as `loop-census`/`rir-coverage`
    already did. `fmt/arena-census` also skips cleanly on the VS generator, which emits no
    `compile_commands.json`.
  - `loop-census` (×1): the temp-linked instrumented compiler missed its bundled headers;
    the PE self-compile now gets the same `-Bruntime/win32` the link step uses, so it finds
    `stdlib.h` (which on PE lives in `runtime/win32/include`).
  - **Residual four, each an owner call rather than a portability bug:** `rir-coverage`
    (PE `lowerable` floor is stale-high — a re-bank, once confirmed to be codegen evolution
    not a real regression); `ci/must-run-registered` (`ast/o0-baseline` is gated
    `UNIX AND NOT WIN32` yet `must-run.txt` requires it everywhere; `wide256/gmp-diff` needs
    libgmp — register-on-Windows vs. exempt is a coverage-policy decision); `cross/shadow-iv-x86_64`
    (the bash sweep reports 614/614 attempts failed — a deeper Windows-run issue); and
    `runtime-bench-check` (mcc-vs-reference divergence on signed-overflow UB in `branchy`
    and `%f` CRT rounding — not a portability bug).
- ~~**W8** — fix the `selfhost-jit` heap corruption.~~ **Closed 2026-08-09 on the
  native x86_64 Windows host, via the MSVC-ASan `mcc_s` oracle.** The deterministic
  crash is a heap-use-after-free of a `Sym` at `gaddrof` (`src/mccgen.c`, the
  `vtop->sym->type.t & VT_VLA` probe) during `ast_reemit` of `embed_resolve`.
  **Root cause, corrected from the 2026-08-05 note:** a leaf's captured `sym` is the
  *referencing frame's own local `Sym`*, stored verbatim by `rir_leaf_slot`
  (`src/mccrir.c`). That sym is required during in-function RIR replay —
  `wide256_sv_is_stable_lval` (`src/wide256_slice.h`) branches on it, so dropping it at
  capture regresses `exec-replay/int256` `test_convert` — but it dangles at re-emit,
  which runs at end-of-translation after `ast_func_end` has torn the frame down. It is
  not the cross-function refcount problem the old note guessed; there is no graft of the
  freed sym's owner. **Fix:** `ast_reemit_scrub_leaf_syms` (`src/mccast.c`) nulls the
  `sym` on plain-local leaves (`VT_LOCAL` without `VT_SYM`, and not VLA — detected off
  the node type, never by dereferencing the possibly-dangling pointer) before replay,
  for the re-emit root arena and for each callee arena grafted while `ast_in_reemit` is
  set. This matches AOT semantics, which carry no sym on a plain local lvalue and NULL
  on a wide256 local. Verified: the `mcc_s` + `tools/selfhost-jit.py` oracle passes with
  in-memory output **byte-identical to the AOT reference**, and the full native ctest
  suite shows every `int256` cell (all replay/reemit variants) green with no new reds.
- ~~**Windows build broken by `tools/slicerun.c`.**~~ **Closed 2026-08-09.** A merge
  landed `slicerun.c` including POSIX `<dlfcn.h>` and calling `setenv`/`unsetenv`
  unconditionally, so the whole MSVC build failed to compile/link (it supplies its own
  `host_dl*` because it does not link `mcchost.c`). Ported its `host_dl*` shims to
  `LoadLibrary`/`GetProcAddress` and added a `_putenv_s` `setenv`/`unsetenv` shim, both
  under `MCC_HOST_WIN32` (not raw `_WIN32`, which `host-gate-invariant` rejects). Full
  sanitize-msvc build is green; `host-gate-invariant` passes.
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

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *Closed 2026-08-05*.

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

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *Strategy-registry sweep — `tests/optfire/stratsweep.sh`*.

### C2 gap — remaining Replay_IR fidelity work

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *Byte-level coverage board, both RIR layers (2026-08-06, x86_64 Linux, gcc-built mcc)*.

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

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *Self-host fallback board at HEAD (2026-08-06, x86_64 Linux, mcc-built mcc)*.

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
- **`-fdump-loopdep` answers a different question than the transforms do, and it is the
  question nobody asked.** Found 2026-08-11 while reducing the down-counting dependence
  bug (`931f3137`). The dump runs at the top of the AST driver; `ast_interchange_run` and
  `ast_fusion_run` run near the bottom, after the tree has been rewritten. On the two-loop
  reducer the dump printed `fusion(#7,#30): ILLEGAL` and fusion then *fired on that exact
  pair* in the same compile — `ast_dep_same_trip` bails early at dump time and succeeds by
  the time the transform asks. So the dump does not say what any transform will do, and it
  reads as if it does; a wrong verdict there is indistinguishable from a right one. It cost
  ~40 minutes of chasing a phantom third bug. Either run the dump from the same point the
  transforms do, or label each line with the pipeline position it was taken at. Until then
  **do not use `-fdump-loopdep` to confirm a legality fix** — instrument the `_apply`
  function or diff the executable's output instead, which is what actually settled it.
- **Three JIT-only miscompiles survive the -O0…-O4 embed-JIT ladder, 2026-08-11.**
  **Promoted to N8 in *Open, ranked*.** *(That row claimed the detail was "not duplicated
  there"; it is — N8 restates the counts, the reduction and the two exclusion arguments.
  The two copies agree today. Corrected 2026-08-11 rather than deduplicated, because the
  ranked board and this residue list have different readers.)* The
  corpora named in the goal (gcc, llvm-project, llvm-test-suite) were qualified into 6,623
  oracle-adjudicated run-mode programs and driven through the embed JIT at every level with
  `tools/jitconform.py --phase check --surface embed`. **Five `JIT_MISCOMPILE` rows at
  every level before `85bf6a3d`, three at every level after** — both `970217-1.c` rows (the
  gcc copy and its llvm-test-suite duplicate) are the VLA-parameter bug and are closed.
  Each survivor aborts under `MCC_JIT=1` and exits 0 under `MCC_JIT=0` **from the same
  binary**, which is what rules the AOT compiler out; and each is level-independent, which
  rules the optimizer out.
  - `gcc.dg/torture/pr45830.c` — **reduced, and the reduction is six lines**:
    `int bar(int x){ if (x==5 || x==19 || x==23 | x==26 || x==65) return 1; return 3; }`
    over `x` in `[0,70)`. gcc-15, clang and mcc's AOT path all answer 1 at `x==23`; the JIT
    answers 3, i.e. it drops the **left operand of the bitwise `|`** where that `|` sits
    between two comparisons inside a `||` chain (the `|` is a deliberate typo in the
    upstream test and is exactly what makes it interesting). AOT is correct at `-O0`–`-O4`
    **and at `-O13`**, so this is not a gate the search turns on. `MCC_JIT_SEARCH=0/1`
    makes no difference; **`MCC_JIT_LAZY=1` makes it correct**, so the defect is on the
    eager install path only. Start at the `||`/`&&` operand folds `815d2001` and the
    relational see-through `bc60a3be`, both landed the same day, and at whatever the eager
    path does that the lazy path does not.
  - `gcc.dg/pr96674.c` — `-fwrapv` signed-overflow predicates. Not reduced.
  - `gcc.dg/fastmath-1.c` — `-ffast-math` sign-of-float comparisons. Not reduced.
  - Unexamined and larger: **~80 `differ` and ~150 `refused` rows per level.** `refused` is
    mcc declining to compile at all, which is a front-end gap list, not a JIT one.
- ~~**`-fno-opt-search-<anything>` disables the whole search, not the sub-knob it names.**~~
  **REFUTED 2026-08-11 — see N9 in *Open, ranked* for the disproof. `set_flag` matches by
  exact `strcmp`, and the counts are identical with and without the flags.** The original
  text is kept below because the *way* it was wrong is worth more than the claim.
  Found 2026-08-11 while trying to A/B `-fopt-search-predict`. `-fno-opt-search-fullset`,
  `-fno-opt-search-ordered` and `-fno-opt-search-predict` each take `-O13` from its normal
  candidate count to **`0 candidate evaluations`** — the parser is matching the `opt-search`
  prefix and turning off the master row. Every one of these flags is documented as a
  sub-knob and behaves as a kill switch, so any measurement that used one to isolate a
  sub-knob measured "search off" instead and would have read as "this knob is worth
  nothing". The A/B in `69296b85` had to go through the `MCC_SEARCH_PREDICT` env to get a
  real comparison. Audit anything that ever passed a `-fno-opt-search-*` flag.
- ~~**`jit/xoracle-conformance` drops its second corpus silently.**~~ **CLOSED 2026-08-11 (`else()` added).** **Promoted into N5's
  green-by-omission list in *Open, ranked*.** The CMake arm at
  `CMakeLists.txt` adds `--testsuite`/`--suite ts-unittests` only
  `if(EXISTS ${MCC_XSUITE_LLVMTS}/SingleSource/UnitTests)` and has **no `else()`** — no
  skip, no message, the suite just is not there. `--limit` is per-suite
  (`tools/jitconform.py`), so losing the arm halves the corpus, and the companion
  `jit/xoracle-coverage` cell then fails its `--min-cross 400` floor against a
  single-suite denominator that tops out at 379. On this host the cache pointed
  `MCC_XSUITE_LLVMTS` at a path that does not exist, so the cell was **red for a
  configuration reason that it reported as a coverage reason**. Repointed locally; the
  missing `else()` is still a defect — a corpus that vanishes must say so.
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

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *External suites — three-compiler board taken 2026-08-05 at `030fb4aa`*.

#### Types the front end cannot parse
`__m512` / `__m256h` / `__m128h` (52 cells), `__float128` / `_Float128` (28),
`_Complex _Float16` (9), `_BitInt` (4). `_BitInt` is C23-mandatory; the rest gate the
x86 ABI tests, which is why `gcc.target` sits ~2pp below the other suites.

#### Missing `__builtin_*` — 66 cells / 33 files
`__builtin_cpu_supports` and `__builtin_cpu_init`, `__builtin_setjmp` and
`__builtin_longjmp`, `__builtin_addc` and the `subc` family, `__builtin_powi` /
`__builtin_powif`, `__atomic_thread_fence`, `__builtin_eh_return_data_regno`,
`__builtin___fprintf_chk`, `__builtin_vprintf`.

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *Confirmations for the clusters the archive had ranked*.

## 256-bit integers: `__int256` / `unsigned __int256` (`wt/bits256`)

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *Which "256-bit" this is, and why*.

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

> Moved to [`docs/ARCHIVED.md`](ARCHIVED.md) 2026-08-10, validated complete against the tree: *Verification, this tree*.

## Carried forward from archived write-ups — open residues, 2026-08-10

These blocks were written inside `Landed —` sections as *"found on the way, not
fixed"* / *"still open in this area"* residues. The write-ups around them were
archived on 2026-08-10 once their branches were confirmed merged; these residues
are **not** closed and are reproduced here so archiving the finished work does not
bury them. Each names the section it came from. Where re-validation on 2026-08-10
changed the finding, the correction is marked inline.

> From the archived section *Landed — smoke reaches every optimizer strategy, and the 22nd was invisible to the panel, 2026-08-10*.

#### Still open in this area

- **`-O13` is dark on 13 of 22 strategies** — `ivsr, narrow, bfold, range, bf, divmagic, pre,
  ltemp, inline, abs, tco, licm, reassoc`. That is the state the whole subject was in before
  `scases.h`, at the one level nothing watches (open item 21). The objections to watching it
  are gone: an `-O13` compile of the subject is ~2.7 s, it is bit-deterministic across search
  budgets of 100/1000/5000 ms, and it already agrees with `-O0`–`-O4` on the value digest.
  `smokerun --max-level 13` is currently a **silent no-op**: `smk_maxlevel()` returns 4
  because 13 is `MCC_OPT_SEARCH_LEVEL`, not an `MCC_OPT_ROW`, and `main` clamps to it.
- **The ratchet wants the opposite polarity.** Banking *dark* strategies rather than fire
  counts fits the existing monotone-decreasing `ratchet()` with no change to it or the file
  format: a strategy going dark becomes a new category (hard fail), one lighting up prints
  `IMPROVED`. `--stats=4` costs nothing measurable and does not change codegen.
- **At `-O13` the panel prints once per search phase and the last one is all zeros.** A census
  must take the per-column max across panels or it will read every strategy as dark.
- ~~**The device arm computes a census and never ratchets it.**~~ **CLOSED 2026-08-11 — `own("dev-")` + `ratchet()`, 531 previously-discarded refusals now banked.** `device_probe` calls `cat_add`
  for `dev device-refused:unavailable` and `:no-dispatch`, but the `do_dev` branch never calls
  `ratchet()`, and `bails.txt` holds no `dev ` rows. Both categories are unmeasured.


> From the archived section *Landed — item 26 was not a stack-slot overlap, it was the replay frame record handing out a slot too small for what it was about to hold, 2026-08-10*.

#### Still open in this area

`ast_locrec_skip` is the third site that assumes the stream is positional: the inline graft
of a struct-returning callee burns one recorded entry to stay in step and then allocates its
return slot fresh, without reading the entry it consumed. Now that the cursor can skip
forward, a blind skip can eat an entry a later request would have matched. That is a
fidelity effect and not a correctness one — the later request skips forward or falls back —
and the byte-identity sweeps above show it is not causing drift today. It should still be
made to consume by fit rather than by count.

`rir_loc_replay` is the same shape with a better resync — it records the output position
`ind` at each allocation and advances the cursor past entries left behind by the emitted
code — and it **still never checks the size or alignment of the entry it returns**. It was
measured rather than assumed: a report-only probe on the entry the resync actually settles
on, after the cursor advance, fired **620 times on the smoke subject across `-O0`–`-O4` and
1,013 times over gcc c-torture at `-O2`, with zero undersized or under-aligned** — every
one an exact fit. So the position resync is holding, and the probe was reverted rather than
banked: it is a negative result about a hazard, not a ratchet. **Re-take it before trusting
that**, because nothing pins it: record `size`/`align` beside `rir_locrec[]`, compare them
against the request inside `rir_loc_replay` just before `*loc_out = rir_locrec[i++]`, and
count both firings and mismatches — the firing count is the anti-vacuity half and must be
reported with the mismatch count, or zero mismatches means nothing.


> From the archived section *Landed — two replay defects that were hidden rather than fixed (`wt/replayfix`), 2026-08-10*.

#### Not the same defect: the `shift=bad sfop=asm@2` fact is untouched and still open

`shift=bad sfop=asm@2 sdiff=1` reproduces **unchanged** after the fix: put `plt_target`
*above* its `asm("call plt_target")` and `MCC_REPLAY_IR=3 -O1` still reports it for
`call_plain`. Different mechanism — the integrated assembler resolves a same-TU `call` to
a direct displacement instead of a relocation, so the emitted **bytes** are
position-dependent; nothing to do with lexer state. It is **latent, not shipped**: an asm
body always trips `ast_arena_has_hole`, so `ast_opt_ok` is 0 and it never reaches any
shifted-base emit path. The real fix remains "emit a relocation for a same-TU `call`",
still not attempted, and `tests/exec/inline_asm/asm_reloc_suffix.c` still sidesteps it by
defining `plt_target` at the bottom of the file — **do not reorder that file**.


> From the archived section *Landed — two replay defects that were hidden rather than fixed (`wt/replayfix`), 2026-08-10*.

#### Found on the way, not fixed: **63** cells have never compiled their own `EXTRA`

> **Re-counted 2026-08-10, and the heading was wrong twice.** It said *six*; the names it
> then lists are *eight*; and the registry expands those names to **63 cells** —
> `ast/rir-position` (1) + `ast/rir-parity-*` (48) + `ast/rir-c2-*` (14). The blast radius
> is also wider than the eight names suggest: `-DEXTRA=` appears at **8** registration
> sites (`CMakeLists.txt`), and the four
> cross sites at `:4754+` pass an `MCCFLAGS` carrying only sysroot/runtime `-I`s
> (`CMakeLists.txt`) — never the repo root — so they fail the same
> way. The silent `continue` on non-zero `_rc` is confirmed at
> `tests/ast/rir_parity.cmake`, `tests/ast/rir_position.cmake` and
> `tests/ast/rir_c2.cmake`. The defect below is otherwise exactly as described.

`ast/rir-position`, `ast/rir-parity-{O0,O1,O2,O3}` and `ast/rir-c2-{O1,O2,O3}` all pass
`-DEXTRA=.../tests/diff/full_language.c`, and all of them invoke `mcc -w <opt> -c` with
**no `-I` and no `-DCC_NAME`**, so the file fails at `#include INC(42test)`, `_rc` is
non-zero, and the loop `continue`s. The EXTRA has contributed nothing to any of those
cells for as long as they have existed, and both scripts' "compiled nothing" floors are
per-corpus, not per-file, so it never showed. Arming it is one `-I` each — and it goes
**red**: at `-O0` the file has 303 bodies, 299 faithful, 1 empty, and three that are not
(`get_asm_string` `rerror`, `asm_local_statics` `runfaithful:reloc@-1`, `asm_dot_test`
`rdiverge:asmgen@41`), against `rir_parity`'s hard 100% bar. Those three are pre-existing
byte divergences the gate is masking, not this branch's; arming the cells is a separate
decision with three defects behind it.


> From the archived section *Landed — the search tier counts work, not seconds, 2026-08-10 (`wt/o4ticks`)*.

#### Three things this exposed that are still open

1. **The search memo was keyed without the axis configuration.** `ast_search_key_salt`
   folded version, triplet and ISA, so an entry recorded under one `(gate, budget, limit)`
   was reused under another and warm-cache output drifted from cold-cache output. Fixed
   here by salting with the twelve `so_axes[]` names plus `MCC_AST_FN_CONFIG`, and
   `opt-search-determinism` now compares a warm-cache run against the cold one so the
   class stays covered. **This is the sibling of `opt-cache-determinism`, and it means
   that cell's subject is no longer purely hypothetical**: with `-O13` writing 5 entries
   under `XDG_CACHE_HOME`, a four-state cache-identity claim at the search tier now has
   something to measure. `-fopt-slice`'s `sl-<salt>.ck` is a separate cache and was
   deliberately not touched.
2. **`-fopt-search-pthreads` faults.** The workers call `ast_search_score_one`, which
   writes the process globals `ast_cur` and the whole `ast_*_env` set. Fix or delete it;
   it is `MCC_OPTD_OFF` so nothing reaches it today.
3. **The superopt checkpoint is a resume, and a resume is not reproducible.**
   `so_ckpt_read` used to seed `best_gate` and all three cursors from
   `$XDG_CACHE_HOME/mcc/so-<key>.ck`, so the object depended on what the box had compiled
   before. It is now behind `MCC_SO_RESUME=1`, default off. The *write* is kept, so the
   file is still there for tooling and for the cache-identity question above.


> From the archived section *Landed — the JIT pool drains and joins, and the item that blocks cluster L is `L2′(i)`, not `L1`, 2026-08-10 (`wt/jitshutdown`)*.

#### Still open in this area

1. **The teardown is unbounded above.** See the tick split above.
2. **`MCCJIT_POOL_MAX` is 64** and `mccjit_pool_start` now clamps to it silently. The
   baked constructor passes a literal; nothing today asks for more than 2.
3. ~~`L2′(ii)` and `L2′(iii)` are unchanged and are what `L2` is now waiting on.~~ **Both closed 2026-08-10.**
4. ~~`mcc_gpu_quiesce`'s unbounded `vkDeviceWaitIdle` becomes reachable from `atexit` the
   moment `L2` wires the device into `mccjit_shutdown`. Clear `mcc_gpu.ok` on
   `VK_ERROR_DEVICE_LOST` **before** that wiring, not after.~~ **Done in that order,
   2026-08-10.** The wait is not merely bounded, it is gone.


> From the archived section *Landed — recursion lowers by depth-bounded expansion behind a bailout guard, and the wavefront has no candidate on this corpus, 2026-08-09 (`wt/depthinline`)*.

#### Still open

- The wavefront strategy itself is unbuilt, on the evidence above. If a corpus
  with wide recursion appears, the instrumentation and the cost estimate are the
  two things it would have needed and they are landed.
- **Only the ternary body shape is expanded** — `return c ? base : f(...)`. This
  is the binding limit, not a footnote: it is why the corpus payoff is zero, and
  24 of the corpus's 26 recursion-bearing files are behind it.
- Self-recursion resolves through `ast_cur` because `ast_inline_retain` runs at
  the *end* of a body, so a function is never in its own pool at the moment its
  arena is dumped. Mutual recursion still needs the pool and therefore still
  needs define-before-use.
- Effect ordering is untouched. Nothing here reorders anything — a depth
  expansion is the same evaluation order the call had — but a wavefront would,
  and `wt/effectlog` owns that representation.


> From the archived section *Landed — `rir_decayed_array` read a comparison's opcode as a `Sym *`, 2026-08-09 (`wt/decayfix`)*.

#### Still open — three unguarded `sym` dereferences, filed with their reachability

1. **`src/mccgen.c`, `unary()` case `'&'`.** `vtop->sym && vtop->sym->a.is_register` runs
   *before* the `test_lvalue()` that would reject a comparison, so `&(a < b)` reads
   `a.is_register` through the corrupted pointer. It does not fault today because the
   operand slot last held a real `Sym *`, so the pointer is mapped and the bit reads as
   zero — the `lvalue expected` diagnostic that follows is right by accident. The cheap
   fix is to move the register check after `test_lvalue()`.
2. **`src/mccgen.c`, `check_va_start_register` and `check_va_start_last_param`.** Both
   dereference `vtop->sym` under a bare null test, and the `va_start` argument reaches
   them through `parse_builtin_params`' `'e'` case, which does not coerce. `va_start(ap,
   a < b)` compiles clean today for the same accidental reason as (1).
3. **`src/mccast.c`, the builtin-fold ISA scan.** `((Sym *)(uintptr_t)a->sym[c])->v` on
   the first child of an `AST_Invoke`, guarded only by a null test and an `AST_Ref` kind
   test, with no `VT_SYM` test on the node's op. An indirect call through a local function
   pointer reaches it with the *variable's* `Sym`, a valid pointer, so the read is benign —
   but nothing enforces that.

The root-cause fix that would close all three at once is to make `vset_VT_CMP` write the
whole union and to have `gv` clear `sym` when it drops `VT_SYM` from `r`. Neither was
taken here: `vset_VT_CMP` is on the hot path of every comparison, `gv`'s surviving `sym`
is read as an identity key by `seqp_key` and is the target of the `addrtaken` write in
`unary()`, and this branch's contract was that emitted code must not change. The three
reader-side guards above are each a one-line change and carry none of that risk.


> From the archived section *The JIT, measured for the first time — 2026-08-09 (`wt/jitconform`)*.

#### Still open on the JIT after this branch

1. ~~**The KGC zero-extension miscompile above.**~~ FIXED on `wt/kgcfix`; the
   return-register convention section above records what it actually was and what it
   closed. ~~What replaces it at the top of this list is the **side-effect admission
   defect** in the same route — five distinct corpus programs, wrong answers reachable
   from ordinary C, and the reduced case is in this file.~~ Also FIXED, on `wt/kgcpure`:
   `ast_fn_purity` never saw `++`/`--` because they are `AST_Unary`, not `AST_Store`. The
   landed section above records the measurement, the 8-of-475 dispatch cost and the four
   cells. **Open residue, one line:** `ast_fn_purity` is now sound over the arena it is
   given, and nothing checks that the arena is the whole callee. `ast_arena_has_hole`
   covers inline asm and dangling register refs; whether `mccjit_intent_serialize` can
   hand the classifier a body with a *silently* elided statement is unmeasured. The
   whitelist makes an omission fail safe only if the omitted node would have been in the
   arena.
2. ~~**47.1% of programs cannot be baked at all.** The single largest lever on JIT
   coverage is the `VT_STATIC | VT_INLINE` callee refusal in `mccjit_intent_serialize` —
   a static helper called from the JIT'd function disqualifies the whole function, and
   that is the commonest shape in the corpus.~~ **The 47.1% reproduces; the attribution
   does not.** Measured on `wt/bakewiden` (see the landed section at the head of this
   file): the refusal costs **60** of those 3118 programs, and **3110 of them have no bake
   site at all** — they never reach `mccjit_intent_serialize`, so they are lost in
   `ast_func_end`'s gate chain. The refusal itself is now widened to bind a local callee by
   address (−140 refused bake sites, −38 `NOT_BAKED` programs, 0 new DIFFER). **What is
   still open here is the real lever: attributing the 3110 across `rir_try_active`,
   `ast_replay_ok`, `ast_opt_ok = faithful && !ast_fn_hole` and `ast_jit_want`.**
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


