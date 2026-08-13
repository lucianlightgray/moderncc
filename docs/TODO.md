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
> **Emit-coverage inventory, 2026-08-11.** The four layers were instrumented at their own
> decision points (`src/mccinv.h`, `MCC_INV=1`, non-perturbing and validated as such) and
> `-O0` catalogued against `-O1` over 300 torture programs. Headline: **at `-O0` the RIR and
> AST layers are absent, not idle** (0 of 1932 bodies recorded, no `ast.*` counter at all);
> **`--embed-jit` rather than the `-O` level is what turns the pipeline on at `-O0`**; and
> **`-O1` emits byte-identical objects to `-O0` for 185 of 284 programs**. Full catalog in
> [`docs/EMIT-COVERAGE.md`](EMIT-COVERAGE.md), summary and reconciliations in *STATE OF PLAY*.
> It also supplies the bake counter item 8 said `mcc` did not have.
>
> **Emit map, 2026-08-11 — the RIR layer was invisible, and the gap is now named.** The trace map
> of a compile was missing a tenth of itself: `src/mccrir.c` contained no `MCC_TRACE`, so
> `tools/tracegate.c` exempted it and the layer that makes the record/skip decision emitted **zero**
> events. 637 sites were added (line count unchanged at 6,492, objects byte-identical, 72/72
> regression cells green), and RIR now shows as **9.99% of an 18.2M-event compile**. Two findings.
> **(1)** `g()` writes every machine-code byte **1.0000×** at `-O0` self-host (an exact identity),
> **1.95×** under `-O0 --embed-jit` (parser pass + exactly one replay) and **2.71×** at `-O1` — so
> the extra passes at `-O1` are the optimizer trying more than one candidate, not the record/replay
> pipeline, and *any* instrument counting emit calls over-reports surviving bytes by up to 2.71×.
> **(2)** The recorded-but-not-verdicted gap `EMIT-COVERAGE.md` could not explain **closes exactly
> on all four pipeline arms**, via two distinct mechanisms — replay aborting into `ast_func_end`'s
> `setjmp` else-arm (which sits outside the counters), and `ast_replay_ok()` false. Which one
> dominates is target-dependent, so a corpus average hides it. Full map in
> [`docs/EMIT-MAP.md`](EMIT-MAP.md); banked as `emit-map-full-language` / `emit-map-selfhost`.
> **SRA, 2026-08-12 — landed off by default, because the measurement says it does nothing yet.**
> `-ftree-sra` is strategy 23, `ast_sra_run`, with a `--stats=4` counter and — unlike the seven
> N1 names — wired into all five `do_*` sites so its work actually reaches `AST_PF_EMIT`. The
> legality rule is one test rather than a checklist: **a candidate dies unless every `Ref` at its
> frame offset is child 0 of a member access**, which subsumes address-taken, pass- and
> return-by-value and whole-struct assignment (the last has no node of its own — the copy is
> generated inside `vstore()` at replay). **It is off by default because it is byte-neutral**:
> across 291 exec programs at `-O4` it fires on 21 files and changes **1 object**, and the reason
> is structural — `Unary(MEMBER, Ref base)` and a plain `Ref` at `base+k` emit identical code in
> this compiler, so an in-place decomposition has no headroom. Neither hoped-for payoff appears:
> promotion will not take the decomposed scalars (byte-identical across `-fpromote-locals`
> crossed with `-ftree-sra` four ways, with and without preserving `sym`), and it does not help
> the device either — SRA fires **0** times on `tests/smoke/subject.c`, whose 81 member refusals
> are on structs it cannot legally touch. **N22** is where the real optimization is. Two defects
> were found and fixed on the way: `ast_promo_size_unknown` is inside an arch-conditional block
> and its use broke all five cross targets, and `stratsweep.sh`'s registry mirror plus its
> `STRAT_NONE` sentinel had to move for a 23rd strategy to exist at all. `strat-dark:sra` is
> banked deliberately — a strategy that is off by default is dark at every level by construction.
>
> **arm64/macOS wave, 2026-08-13 — the inner loop was measuring a distribution, and five of six
> "blocked" rows were not.** Cluster A closed in full (A1–A7), cluster B to 7 of 8, cluster C to
> 5 of 7. **N26 closed and its diagnosis was wrong**: `flagsweep-exec` was not `-j` contention —
> a cell times out *alone*, and `atomic_counter` run 24× on an idle machine is **bimodal**, 18
> runs at 0.45–2.83 s and 6 at 51–121 s. The cause is that `PIN` is `taskset`, which macOS lacks,
> so the mechanism built to prevent exactly this was a silent no-op here; `taskpolicy -b` fixes
> it and the family is **119/119 in 2694 s at `-j4`, zero timeouts**. **N24 closed**: one
> `strcmp` in `mccjit_bind_apply` compared the blob's undecorated key against a `_`-prefixed
> Mach-O symtab, so no bind ever applied and the host resolver silently bound libc's `random()` —
> a Mach-O defect, not an arm64 one. **N30 found and root-caused**: `_Float16` negation quiets
> signaling NaNs, 1022 of 65536 patterns, because it lowers through a promote-to-`float` round
> trip; the fix is written and verified (0 mismatches, digests match gcc-16 and clang) but backed
> out — it fails under `MCC_RIR_PROD=2` + `MCC_RIR_ABORTWHY=1`, which places the remaining defect
> in the RIR replay path rather than the negation.
>
> **arm64/macOS wave, 2026-08-13 (second) — the Mac's whole "belongs to the Mac" list closed, and
> two of the three were already done before the list was written.** **N29 closed**: the smoke
> oracle now takes the already-resolved `DIFF3_GCC`, so this host adjudicates with Homebrew GCC
> 16.1.0 against Apple clang 21.0.0 and `diverge-both` is live — `differing=275`, split 243
> diverge-one / 22 refs-disagree / 10 diverge-both, smoke **12/12 in 209.5 s**. **All ten
> diverge-both are NaN sign or payload**, established by replaying `smc_hash` point by point with
> a harness whose digests reproduce the subject's byte for byte on all three compilers: 112 points
> where the references agree and mcc differs, 46 sign-bit-only, 66 payload, **zero finite**. The
> board's "2 real" was one defect (N30) counted twice. **N30 re-verified where it was first found
> and backed out**: 0 mismatches over all 65536 `_Float16` patterns at every level `-O0`–`-O4`, 0
> errors under the `MCC_RIR_PROD=2 MCC_RIR_ABORTWHY=1` configuration that defeated attempt 3, and
> the predicted `mov w30, #0x8000 / eor w0, w0, w30` read out of the Mach-O object.
> **`slice/census` and the Metal region differential were both already closed in the tree** —
> re-banked with its bisect at `ac69a0fd`, and the MoltenVK step landed at `2eee6c41` — so the
> twelve `gpu/spv-*`/`gpu/msl-*` cells including the binding-2 region arm are green here in 74.8 s.
>
> **What the wave actually bought is three new rows, all of them about instruments.** **N36**:
> mcc's over-wide bit-field semantics are split between the references — 168 of 185 `bf*` rows
> match gcc, the 17 `*.EQM1` rows match clang, and one reference showed both as the same verdict.
> **N37**: the refs-disagree class introduced *to fix N29's own triage* is computed per digest, so
> a category with one reference disagreement clears every other point in it — `csweep.C64/C80.CDIV`
> and `CDIVSEL` each hide 283 refs-agree points, 44 finite, three of them a 53%-relative-error
> complex divide rather than rounding. And **`ci/must-run-registered` is red on this host**, four
> must-run cells unregistered on Darwin, which is the third time in three days a red was found on
> a machine because someone ran the cell rather than read the table. Three green-by-omission GPU
> registrations were armed on the way (`SKIP_RETURN_CODE 77` under `MCC_GPU_REQUIRED=ON`).
>
> **The board's own top four did not survive checking.** Rank 3 (six binary opcodes with "zero
> coverage") had been closed eight days earlier. Rank 2 (`MCC_RIR_STAMP`) does not do what it
> advertises — no emitter *or* dump reads `ast_stype_*`. Four claimed couplings were refuted by
> name. **The 34 `diverge-both` categories decompose completely**: 22 references-disagree, 10
> implementation-defined NaN sign/payload, 2 real (N30) — established on the harness's own corpus
> after a hand-built one inverted the answer.
>
> **Instrument findings outlast most of the fixes.** `diverge-both` conflated "mcc contradicts a
> consensus" with "all three differ", so `smokerun` grew a third verdict class. This host has a
> **real GNU gcc** (`/opt/homebrew/bin/gcc-16`) that the smoke oracle never looked for, so
> `diverge-both` was structurally unreachable here rather than merely unused. `rir-coverage`
> enforced 2 comparisons and skipped 3 because `residual`/`kept_coverage` were banked flat; a
> per-format schema takes it to **20 enforced, 0 skipped**. Three gates that could not fail were
> floored, two fixture directories that passed empty were floored, and `slice/census` was
> **bisected** to `82f39935` — a deliberate priced change, not drift.
>
> **The pattern worth carrying: a failed approach is evidence about the approach, not the
> problem.** B8 was a file-format parser, C1 a bank schema, `slice/census` a bisect, B1 a guard
> that made the hazard impossible, and N30's premise was wrong because `R_RET` returns `REG_IRET`
> for `VT_FLOAT16` — a half never sits in an FP register. Corrections to this file's own rows are
> recorded inline: **N27 and N28 were filed wrong and struck.**
>
> **Windows wave, 2026-08-13 — W7 landed as a deletion and the wt/winspec housekeeping bundle
> closed.** `arm-wince` is gone: a byte-identical alias of `arm-win32` (same cdefs, same `-O0`
> object sha256s and forced-Replay_IR counters), so the target, its three `arm-wince.*` banks, the
> `o0_ab.sh` twin check and both board rows were deleted and the `o0-baseline` measurable floor
> dropped **8 → 7**. `arm-win32` stays an explicitly compile-only target and `tools/build.c`
> gained the `arm-win32` entry it had been missing. The housekeeping bundle landed with it:
> `exec-gatecombo/*` got its `else()` arm (348 visible Skips off-WIN32 instead of vanishing),
> `pe-wine-conformance` now tears its wineserver down (0 leftover servers, measured),
> `RESOURCE_LOCK "wine"` on the wine cells, three PE/wine rows added to `tests/must-run.txt` with
> `runtime-bench-gatewin`'s dual 77-cause corrected, and `MCC_WINE` / `MCC_WINE_REQUIRED` so a
> wine-less host fails loudly rather than green-skipping (verified: required + no wine → FAIL, else
> SKIP). Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md). **W3 (COFF object writer) landed
> flag-gated on the actual Windows box, 2026-08-13.** `mcc -c -Wl,-oformat=coff` now emits a real
> Microsoft COFF object (`coff_output_obj`, `src/objfmt/mccpe.c`) that links bidirectionally with the
> *vendored* `x86_64-w64-mingw32-gcc` — the mingw oracle the Linux-written plan called absent is
> present here, and PE runs natively, so W3's four-way differential runs with no wine and is enforced
> by `pe/coff-obj-diff`. Default `-c` still emits ELF (banks + `arm64pe_diff.py` untouched; the
> default-flip + re-bank is the follow-up). **W1, W2, W4–W6 remain open:** W1's external corpus
> (`vendor/gcc-c-torture-execute`) is genuinely absent (the mingw-oracle half of its blocker was
> not); W4
> (`UNWIND_INFO`), W5 (CodeView) and W6 (SEH) are the large object-format/debug/exception backends,
> each needing a differential loop this host cannot run. The `arm64-win32`/`arm-win32` execution
> host items still need Windows-on-ARM hardware this box does not have. The "stale i386 message"
> the plan cited has no locatable subject — every i386 message was checked and none is stale.
> **arm64/macOS wave, 2026-08-12 — the inner loop did not exist on one of the three machines,
> and turning it on found two miscompiles.** `ctest -R "^smoke/"` was **12 of 12 red on the Mac**
> and had been for as long as the suite existed there. None of it was a compiler defect: the
> subject asserted three x86 hardware facts as though they were properties of C — that
> `__int128` exists (so `fcases.h` did not compile and the suite **ran nothing while reporting
> twelve results**), that integer division faults, and that an out-of-range float→int conversion
> yields the x86 indefinite value. Separating each into its two arms let the value sweep reach
> its real subjects, and it immediately produced **two live `diverge-both` miscompiles** — a
> ternary merge and an in-place `gen_cast` each writing the register of a *promoted local*,
> which `get_reg` protects and nothing protected against in-place writes — plus an
> `--embed-jit` link failure on Mach-O (baked blobs and their bind slots were placed at
> alignment 1, which chained fixups refuse and ELF tolerates). Two board rows closed on the
> way: **N17** (three counters, and `rir.rec == ast.body + ast.abort + ast.noreplay` now holds
> with no residual) and **N1** (the seven write-only strategies now reach `AST_PF_EMIT`; it
> costs 224/448/16 bytes of self-host at `-O1`/`-O2`/`-O4`, on purpose). **N8**'s lead was
> already closed by N20 and is now pinned by a parity fixture.
>
> **The wave then kept going, and most of what it found was instrumentation rather than
> compiler.** **N15** closed — `-g -run` SIGSEGVing the moment the JIT bakes is one misordered
> line in `mcc_tcov_block_begin`, which read `s1->dState->tcov_data` above its own
> `test_coverage == 0` early-out; harmless on the AOT path where `dState` exists, fatal in the
> JIT's re-emit where it does not. `MCC_OUTPUT_MEMORY` is now in the bake gate, checked by
> differential over all 310 `tests/exec` programs at `-O1 -g -run --jit`. **N21** closed as
> *not a defect*: the ladder census is deterministic and the six bytes that moved were
> `__TIME__`. **N16** does not reproduce — 294 objects, zero moved — and the same `__TIME__`
> landmine most likely explains the original report. **N25** closed the day it opened. And
> **five of the seven standing reds** closed: `flagsweep/cover3-verify`,
> `ci/registration-stubs`, `fmt/census-bank`, `cross/shadow-iv-x86_64` and
> `runtime-bench-check`, of which the last three were harness bugs that had been reported as
> compiler-adjacent facts.
>
> Two harness findings outlive the port. **The two "independent" references on that host were
> one compiler**: `/usr/bin/gcc` on macOS is a clang shim, so every `diverge-one` was being
> reported as a `diverge-both` — 271 rows of this suite's loudest verdict, produced by asking
> one compiler whether it agreed with itself. `smokerun` now detects the shared family and
> refuses. **And `tests/smoke/bails.txt` was a single-target bank in a three-machine tree**;
> it is now keyed by target, with x86_64-linux and Windows keeping exactly the file and the
> numbers they have. New rows: **N24** and **N25**, plus **N23**, which was opened as a
> conformance defect and corrected to a gcc/clang disagreement the same day — the correction
> is kept because a one-reference host cannot tell those two apart, and that is the durable
> finding. Detail in
> [`docs/ARCHIVED.md`](ARCHIVED.md).

> **`--jit-always-gpu`, 2026-08-11 — the device is at verdict parity with the CPU, and the
> boundary is not what it was thought to be.** The flag arms the ladder equivalence oracle
> unconditionally (in this compiler and baked into the output's JIT via the generated
> constructor, env override `MCC_JIT_ALWAYS_GPU`), arms the pair census that feeds it, and
> counts every refusal instead of falling back silently. Measured on an RTX 5070 Ti:
> **self-hosting `src/mcc.c` runs 249,556 rungs, 499,113 dispatches and 4,768,438,116 lanes on
> the device with `refused=0`**, and the `[ladder-self]` and `[ladder-cross]` panels are
> identical to the CPU arm across **24,307 pairs and 2,357,085,080 evaluated points, differ=0**,
> with the emitted object byte-identical. **The boundary is struct member access, not anonymous
> link-time functions**: on `tests/smoke/subject.c` all 81 emitter refusals are `AST_Unary` with
> `AST_OP_MEMBER` (`emitter-refused=81 by-node Unary=81 / by-op member=81`) — the refusal
> reasons are newly attributable, since a `spv_expr` refusal previously carried no reason at
> all. No refusal was attributable to a link-time or anonymous symbol on either subject. The
> cost is real and points the other way from the name: the device arm is **2.5–2.7× slower**
> (self-host 124.3 s against 49.1 s), which is the lane-economics result the break-even table
> already predicted. Cells `gpu/always-gpu-parity` and `gpu/always-gpu-parity-full-language`;
> **N21** is the one defect the exercise turned up.
>
> **Third sweep, 2026-08-11 — this file is now open items only.** Everything finished moved to
> [`docs/ARCHIVED.md`](ARCHIVED.md): the ten dated 2026-08-11 write-ups and measurement boards
> (including the emit-coverage catalog and the emit map), the four closed board rows
> (**N4**, **N5**, **N9**, **N20**) and **57 closed sub-items** lifted out of sections that are
> still live. **5,309 → 4,102 lines.** Nothing was deleted. Every migrated row and item is left
> here as a one-line struck stub under its original number or list marker, because rows are
> cross-referenced by number across these documents and a reader following "N5" or "item 7" must
> land on the closure rather than on nothing. **Inline corrections were deliberately not moved** —
> a `~~wrong~~ right` inside a live sentence is not a finished item, it is how this file records
> that a live claim was repaired, and lifting it would leave the sentence meaningless. The live
> rows that cite migrated evidence — N15, N16, N17, N18, N19 — each restate the number they turn
> on, so the board still argues from evidence with the derivations one file away.
>
> It leaves three ranked items — **N17** (an aborted replay increments nothing, so every `ast.*`
> number here is a lower bound; now the top of the live ranking, one line to fix), **N18** (the
> `-O0 --embed-jit` arm is the least faithful on both targets and no cell watches it), **N19** (the
> byte census is x86_64-only, since arm/arm64/riscv64 `o()` bypasses `g()`) — and two notes too
> small to rank: the 31-byte `full_language.c -O0` residual is unattributed (self-host has none),
> and `ir_cap`'s trace sites fire ~375k times at `-O0` where the layer is inactive, because the
> macro shims log entry before the `ir_cap_active` early-out. It also **confirms** rather than
> revises the self-host/torture faithfulness split this file already records: 2.21% on `src/mcc.c`
> at `-O1` against the 2.17% previously measured at `-O2`.
>
> **JIT/debug wave, 2026-08-11 — `-g` bakes, and two reds diagnosed.** Three commits,
> `957169fa..a404d8c9`. `--embed-jit` is now the only gate on baking, so a `-g` build
> hot-swaps (`boot=1254 swapped=191`, was `0/0`); `jit/gdb-debuggable` and `jit/selfhost-opt`
> pin what that buys and where it stops; and the four red `ast/o0-baseline*` cells were a
> **stale bank, not an `-O0` bug** — all five are green. Two new ranked items came out of it,
> **N15** (a live `-g -run` SIGSEGV) and **N16** (`85bf6a3d` moves `-O0` objects while
> claiming otherwise). Sections dated 2026-08-11 inside *STATE OF PLAY*.
>
> **Board-work wave, 2026-08-11 — N5, N4 and F7.** Six commits, `2d04a308..a6f46ff2`, working
> the open board easiest-first. **N5 is closed in full** (all four green-by-omission hazards),
> **N4 is closed** (`smoke/strat-dark` banks the strategies `-O13` leaves dark), and **F7 is
> closed as *not binding*** — the constant caps were A/B'd at 512 and 4096, found
> byte-identical with `rejected-modules=0`, and left at 512. Summarised under *The board-work
> wave* in *STATE OF PLAY*, including what each closure left behind. Smoke is now twelve cells
> and ~117 s.
>
> **Second wave, 2026-08-11 afternoon — JIT/AOT differential and the search surrogate.**
> Five commits, `0dd6ea55..04f12187`. The embed-JIT surface was run against gcc,
> llvm-project and llvm-test-suite at every level it boots, which found a JIT-only
> miscompile and left three more open (**N8**); a degree-2 surrogate was fitted over the
> `-O13` gate cube, measured, and left off; and the smoke engine arm went six engines to
> nine. Summarised under *The second 2026-08-11 wave* in *STATE OF PLAY*. The new open
> items are **N8** (the three surviving JIT miscompiles) and **N9** (`-fno-opt-search-*`
> is a kill switch, not a sub-knob); **N5** went from two green-by-omission hazards to four.

> **x86_64-linux wave, 2026-08-13 — the board's Linux-only backlog was reachable all along, and
> the host had two reds nobody was watching.** The gcc checkout the board named as *"the
> highest-leverage host change available anywhere"* is present at `~/Projects/gcc`, and with it
> **N8 closes in full**: neither `gcc.dg/pr96674.c` nor `gcc.dg/fastmath-1.c` reproduces at HEAD,
> and rebuilding the compiler at `2aa3e599^` reproduces both — so N20 closed all three of that
> row's programs, not the one it was credited with. `tests/jit/parity/relop_eq_pair.c` pins the
> four shapes `jit_replay_parity.sh` never covered (`==` as combiner, two distinct parameters,
> `unsigned`, and float compares through a pointer). **`optlevel/torture-differential` closes**,
> which makes the standing-reds table empty for the first time — the two stale group-(a) rows are
> dropped whole, because no level survives and `parse_known` raises on an empty level spec.
>
> **Two reds were red at `52e7e850` on this host and appear in no table in this file.**
> `stratsweep/check` is **N31**: `STRAT_NONE` was 25 and `AST_STRAT_COUNT_MAX` is 25, so the
> sentinel was *rejected* by the order parser and the "registry-disabled" baseline every
> `stratsweep` mode compares against was an ordinary `-O2` — measured, `MCC_AST_STRAT_ORDER=25`
> is byte-identical to a plain `-O2` and `=24` is not. And `fmt/census-bank` had drifted three
> `src/mccast.c` sites, attributed to `42cfc8ac`'s `MCC_SROA_WHY` report and re-taken with the
> attribution the rule demands. Both were found by *running* things, not by sweeping: neither
> matches `^rir-`, `^flagsweep` or `^smoke/`, which is the same regex-completeness failure that
> hid `rir/drop-ratchet` on arm64.
>
> **The three vacuity floors were five, and the sharp hole was not the one filed.**
> `flagsweep/accept` printed `PASS … 118 flags accept -f and -fno-` and returned **0** with
> `MCC=/nonexistent/mcc`; the tab-anchoring story the entry told had already been fixed by
> `7be4fba5`. Closed with a negative control. Adjacent, and invisible from the scripts: `_ss_rows`
> in `CMakeLists.txt` was a hand-written mirror of `STRAT_NAMES` that had drifted two rows, so
> **`sra` and `sroa` had no `stratsweep/iso-*` cell at all**; it is derived and floored now
> (9561 → 9565 cells). The `[ladder-*]` panel's `points=` is on its own line, which unblinds
> **two** consumers, not the one the board named — and it retires the `--jit-always-gpu`
> headline's points figure, which was read from the GPU arm and never compared.
>
> **The standing validation rule was not executable on this host either, and that is N32.**
> `ctest -R "^smoke/"` had `smoke/divergence` red at `52e7e850` — no value changed and no compiler
> defect: `5c71bc20` added a third verdict class and banked only the arm64 file, so the x86_64
> bank described a classifier that no longer exists. The principle the arm64 wave stated —
> *x86_64-linux and Windows keep exactly the numbers they have* — is right for **values** and
> wrong for **classes**. Re-banked with all fourteen moved figures attributed. **`slice/cref-oracle`
> is a second pre-existing red here** and is filed as **N33**: it emits zero bodies, zero slices
> and zero tuples, which matters because it is the adjudicator N7 and N6.10 both lean on. And
> **N34** is the one that stops the gate finishing at all: `MCC_AST_EVAL_LADDER_GPU=1` segfaults
> the compiler **at exit** on `int x;` at any level on either device — the object is written and
> byte-correct, the crash is in `mcc_gpu_quiesce()` — which times out five of the twelve smoke
> cells. **Seven of twelve are green here and five are unrunnable**, so quote the rule with that
> attached until it is fixed.
>
> **Second pass, later the same day: N33, N34 and item 24 all closed, the two harness ones were
> never compiler defects, and running the rest of the suite found three more reds (N35).** **N34** is `atexit` ordering — the ladder registered its exit
> handler *before* the warmup that makes the loader `dlopen` the NVIDIA ICD, so the ICD's own
> later-registered handler ran first, unmapped 36 MB, and mcc's quiesce dispatched into the hole.
> One line moved; the five device/engines cells go from `TIMEOUT` to 146 s green and the smoke
> gate is **12 of 12** here for the first time. **N33** is a wedged X server: `DISPLAY=:1.0` with
> `XDG_SESSION_TYPE=tty`, an X socket that accepts and never answers, and an ICD that opens a
> display inside `vkCreateInstance` with no timeout — `vulkaninfo` hangs the same way with no mcc
> involved. All **18** `slice/*` cells now run with `DISPLAY` scrubbed, floored at 18, and
> `slice/cref-oracle` goes from failing at 180 s to passing at 8.4 s. `gpuconform.py` was
> *reporting a total tool hang in the exact words of an empty funnel*, which is what sent an hour
> of investigation at arena capture; it now names the stalled programs. **Item 24** — the real
> conformance defect of the 22/23/24 trio — is fixed with the x87 `fistpl` sequence the board
> prescribed, and the board's warning is pinned: the out-of-range rows still give the x87 integer
> indefinite that a 64-bit-convert-then-narrow would have turned into 0.
>
> **N30 was re-measured here and the fix is now a decision rather than a sweep.** The mechanism is
> confirmed in `unary()` and the correction is one `^ 0x8000` (`_Float16` lives in a GPR, not an
> SSE register) — but on x86_64 **mcc matches gcc at every level and clang only at `-O0`**, and
> *clang disagrees with itself across `-O`*. IEEE 754 §5.5.1 says `fc01` and every `fe01` is a
> softfp non-conformance, mcc's included; landing it makes mcc right and makes it a
> `diverge-both` against this host's pair. Not landed. Detail under N30 and in
> [`docs/ARCHIVED.md`](ARCHIVED.md).

> **Second x86_64-linux wave, 2026-08-13 — three board rows closed, and two of my own claims in
> this file corrected.** **N30** is fixed: the negation itself was understood, but three attempts
> at a constant/runtime split in `unary()` all died under `MCC_RIR_PROD=2 MCC_RIR_ABORTWHY=1`,
> because the AST replay rebuilds a Binop's children with their *recorded* types and a parse-time
> retype is invisible to the arena. The fix is one arm on `gen_opf`'s `TOK_NEG` case — the
> primitive the IR layer already captures as `AST_OP_FNEG`, which `mccast.c` already special-cases
> for `VT_FLOAT16` — so parse and replay perform one identical atomic operation. **"Restore the
> type before the node is recorded", the route this file called the smaller of the two and where
> to start, is refuted by experiment.** All five targets checked; **arm was silently wrong** (its
> `gen_negf` is `0 - x`, an arithmetic op that quiets a signaling NaN) and riscv64 needed nothing.
> 0 mismatches over 65536 patterns; `mcc-differs-from-both` **39 → 31**.
>
> **N35 closed in full** — all five of its reds. Three were harness (a cell that could not execute,
> a cell missing from a gate's `else()`, a two-suite floor on a one-suite host); two needed a
> compiler attribution and now have one. **`eee6c1f2` is exonerated for the census drop** — it never
> touched the `wide` block. The cause is `85bf6a3d`, and the drop splits per body into **−0.1127pp
> real loss** (five named VLA-parameter bodies) and −0.0578pp dilution. Only two figures were
> banked; a plain `--update-bank` would have rewritten fifteen and was reverted.
>
> **Two corrections to what I wrote here earlier in the day.** I predicted N30's fix would make
> `bsweep.F16.FNEG` a `diverge-both` on x86_64 — it does not: the category stays `diverge-one` and
> changes sides, because clang-22 already did the IEEE-correct thing. And eight `diverge-both`
> categories that fell to zero are **item 24's** `fistpl` fix, not N30's; the cell never flagged
> them because "better" is not a ratchet violation.

> **Cross-cutting wave, 2026-08-13 — the open items were read four at a time for shared surfaces,
> and the overlaps mattered more than the items.** Full detail under *Cross-cutting research* inside
> *STATE OF PLAY*; two commits, `8a70f8b7` and `5fceb897`. **Three of the four remaining "decisions"
> are measurements, not decisions** — sweep row 29 (bodies 2.18% but bytes **7.65%, not the recorded
> 10.2%**, and never a byte upside on any corpus), item 22 (mcc is the conformant side; gcc's
> default contradicts its own `__FLT_EVAL_METHOD__ 0`) and `int128-signedness` (the dispatcher and
> all 39 declarations are orphaned in the header, and are injected into every TU mcc compiles).
> **S7b is the only one that really is a decision**, and the only item bound by others.
>
> **N7's central question is answered**: the GPU arm *does* catch an injection into the 32-bit
> signed `+` arm, `[ladder-cross] points` moving by **exactly 194 — N7's own recorded figure** — and
> the short-circuit on `hr >= 0` makes it a genuine independent evaluator for the rungs it accepts.
> Read the qualification before closing the row: `pairs`, `certified`, `differ`, `refused`, `exact`
> and every histogram stay **byte-identical**, so the arm notices without classifying, and the teeth
> are a coverage-index artefact. The recorded obstruction was also wrong — `TIMEOUT 120` is per
> `execute_process`, and `gpu/ladder-gpu-parity` carries no ctest timeout at all.
>
> **Six asserted couplings were tested and refuted; one this file never named is real** —
> `src/mccast.c::ast_func_end` holds every Cluster-A counter site *and* N7's census entry within 110
> lines. **The instrument thesis is confirmed, with one shared mechanism: absent reads as zero, and
> nothing fails** — `inv.get(k, 0)`, a silent truncation past `MCC_INV_MAX`, two optional anchors,
> and `low_body_index()` returning `None` to degrade a per-body ratchet into the corpus-wide
> percentage it exists to replace. Making those loud is the highest-value change in the cluster and
> is small in each.
>
> **Two landed on the spot.** `smokerun`'s triage printed 128-bit digests through `%.16s`, so all
> three arms read as identical zeros and the print destroyed item 22's own evidence — the thesis
> with a *literal* zero. And `slicerun_reloc` aliased an over-stride global onto its neighbour's
> slot and returned a confident wrong answer; it now refuses, which the ladder already handles as
> no-result. **Corrections to this file's own figures**: `sizeof(SValue)` is 80 (not 64), `sv_stack`
> 41,040 B (not 32,832), and the `MCC_INV_MAX` hazard is real but mis-sized — 16 keys are
> registered, not 12, so it lands at **28 of 32, under the cap**. **`slice/quiesce`'s red is
> structural**: two runs of the same binary on the same commit gave 55/55 and 54/55.
> **Left open deliberately**: `tools/node-census.py` is an unrecorded third `[inv]`-prefix consumer
> that **silently appends garbage** — a wrong number with no signal, the worst of the three modes.

> **Instrument wave, 2026-08-13 — the cluster thesis was acted on, and acting on it moved a
> published number by nine points.** Every "absent reads as zero" the cross-cutting research named
> is now loud, and the two rows that depended on them are closed. **N6.8 closed, and the figure it
> underpins was wrong in the direction the row predicted and by more than it estimated.**
> `jit.embed`/`jit.embed_bytes` count the append to `mccjit_embed_fns` — the list that is actually
> baked — placed after that append rather than at the `mccjit_embed_note` gate, which has five early
> returns behind it. Measured immediately: self-host `-O0 --embed-jit` is **1278 bakes against 1577
> stash attempts of 3202 verdicted**, so the **49%** bake rate published in `EMIT-MAP.md` and
> `EMIT-COVERAGE.md` is **39.91%**; `full_language.c` is 39 against 41 of 299, so 14% is **13.04%**.
> Both documents are corrected in place rather than re-banked silently.
>
> **N18 is now watched, and the cell cost 1.7 s rather than the 7200 s the row was priced at.** The
> refutation that `emit-map.py` does not unblock N18 was correct and is now executable:
> `tools/inv-faithful.py` takes two `MCC_INV=1` compiles and subtracts, needs **no trace build and
> no opt-in**, and reproduces the whole N18 table on this host — `full_language.c` **3.32% → 4.68%,
> +1.36 pt**, self-host **2.18% → 3.37%, +1.19 pt**, against the +1.36/+1.20 the row recorded.
> Registered as `ast/inv-faithful` with a known-positive that injects a +5.00 pt gap and requires
> the bank to catch it; both are `must-run`, because a cell with no host prerequisite has no honest
> reason to skip.
>
> **The per-body lowerable ratchet was disarmed and is now armed, with teeth.** `low_body_index()`
> keys on the **translation unit** as well as `(file, func)` — the compiler reports the lexer's
> current file, so a header body is reported once per TU that includes it, and that is not something
> the compiler can fix. `tests/rir/lowerable-bodies.tsv` grew a `tu` column (legacy 5-column rows
> still read, so the 3060 banked `self`/`macho` bodies were not retired by the schema change) and a
> **`wide`/`elf` inventory of 4574 bodies** was taken — the branch of the hedge this file said would
> have fired too. `rir-coverage-census` now attributes **4550 bodies at `-O0`, 0 gone, 0 new**
> instead of printing "attribution is unavailable". Proved with a known-positive: perturbing two
> banked masks makes it name **`<command line>:__mcc_ov_calc [in …/overflow_inline.c]`** and its
> `overflow_narrow.c` twin — the exact pair whose collision disarmed it. That probe also found a
> defect in this wave's own change (three format strings not widened with the key), which is the
> argument for building the known-positive before believing the green.
>
> **`--update-bodies` is a new switch, and the split is the point.** `--update-bank-low` wrote the
> body inventory *and* the percentage floors. Adding bodies to the inventory cannot hide a
> regression — the comparison is over the intersection of banked and present bodies — while moving a
> floor can and needs an attribution first. They are two decisions and are now two flags. **No floor
> was moved by this wave and `tests/rir/coverage-bank.json` is byte-identical.**
>
> **The three silent degradations are loud.** `emit-map.py`'s two optional anchors are **required**
> (a dropout used to report both counters as 0 and inflate `gap_unexplained` by exactly the aborts
> it stopped seeing); it now **cross-checks `dropout_abort` against `ast.abort + ast.abort_post`**,
> which is the check its own docstring has always claimed and never performed, and fails on a
> mismatch; `int(v)` on the counter channel is guarded; and `MCC_INV_MAX` truncation reports
> `inv.dropped=N` on the dump line instead of dropping in silence, which both tools treat as fatal.
> **The `[inv]` prefix collision is retired rather than documented**: the counter channel is
> `[invcount]`, so the three consumers of `MCC_ARENA_DUMP`'s `[inv] <node> <callee>` grammar —
> including `node-census.py`, the silent-garbage one left open above — can no longer meet it.
>
> **`int128-signedness` deleted, as the research said.** 44 lines: `__mcc_ov_disp`, its `_ti` arm and
> all 39 `__mcc_*o_*` declarations, orphaned in `runtime/include/mccdefs.h` because `__mcc_ov_gen`
> never mentions the dispatcher. Re-verified here before deleting — `nm -u` on a program using all
> four `__builtin_*_overflow` widths shows **zero** undefined `__mcc_*o_*` — and they were injected
> into **every TU** under the default `MCC_CONFIG_PREDEFS=1`. 391 overflow/int128/builtin cells green
> after.
>
> **A correction to the research this wave implemented**: `gap_explained` does **not** read 0 when an
> anchor drops out. Only `dropout_abort` does; `dropout_no_replay` is derived from per-body trace
> counts that need neither optional anchor, so it degrades to that term alone. The conclusion is
> unchanged and the fix is the same one.
>
> **Found while banking, NOT fixed, and left red-adjacent on purpose: `tests/emitmap/bank.json`'s
> `selfhost` cell is stale against its own tree and cannot say so.** Banked `emit_amplification`
> 2.7112 / `pct_faithful` 97.79 / `replay_per_verdict` 1.6205; measured on a trace build **2.712 /
> 97.82 / 1.6214**. **Measured again at `HEAD` in a clean worktree and the answer is identical**, so
> this predates this wave and none of it is attributable to it — which is why the three figures were
> **restored to their banked values** rather than taken with the new key. Only
> `anchor_abort_matches_inv` was added. The mechanism is the tolerances: `TOL` allows 0.05 on
> amplification and **1.0 on every percentage**, so a bank can drift from the tree indefinitely and
> report `bank OK` the whole way. **This is the cluster thesis again, one level up** — not an absent
> number read as zero, but a present number wide enough to be unfalsifiable. Whoever attributes the
> drift should tighten `TOL` in the same change, or the next drift is invisible too.

## STATE OF PLAY — written for a context switch, 2026-08-11

> Read this first. It is a handoff, not a board. Everything below it is detail.
> It supersedes the 2026-08-09 handoff, which is wrong in five places, each named below.
> The 2026-08-10 handoff below it still stands; the 2026-08-11 wave is summarised next
> and did not invalidate any of it.

> **The 2026-08-11 wave write-ups and measurement boards moved to**
> [`docs/ARCHIVED.md`](ARCHIVED.md) **on 2026-08-11**, when this file was reduced to open
> items only. That is where the emit-coverage catalog, the emit map, the `-g`-bakes and
> `o0-baseline` write-ups, the zero-AOT `--embed-jit` self-host measurement and the three
> 2026-08-11 wave summaries now live. **The open rows they produced stayed here** — N15 and
> N16 from the JIT/debug wave, N17, N18 and N19 from the emit map, and N20 struck in place.
> Each of those rows restates the number it depends on, so this file still argues from
> evidence; the full derivations are one file away.

### x86_64-linux, end of 2026-08-13 — where this host actually stands

> Written after the day's three waves, because every number in the sections below it is older
> than the tree. **Count on the host you are standing on** is this file's own rule and this is
> that count.

**9930 cells in `cmake-def`.** The gate families, each run on a quiet machine:

| family | result |
| --- | --- |
| `ctest -R "^smoke/"` | **12 of 12**, ~112 s — the standing rule is executable here for the first time |
| `^exec` | **7675 of 7675** |
| `^slice/` | **55 of 55**, including the 1693-program `cref-oracle-gcc-c-torture-execute` |
| `jit/ ast/ rir optlevel diff3/ superopt/ fmt/ docs/ ci/` — 235 cells | **234 of 235** |

**Re-taken end of 2026-08-13 after the instrument wave: 9932 cells** — the two new ones are
`ast/inv-faithful` and its known-positive, both `must-run` and 1.7 s each. `^exec` re-run whole
because the wave deletes 44 lines from the text injected into every TU: **8023 of 8023**, zero
failures. `rir-coverage-census` **passes here**, which corrects this file in two places: the
2026-08-13 note that "the pair stays red until the ELF half is attributed" is stale — both figures
were banked with attribution on the same day — and the cell was verified green at `HEAD` in a
worktree *before* the wave, so nothing in it turned a red green.

**The one red is not a red.** `rir-nofb-probe-self` fails under `-j` and passes alone — measured
four times now: fails at 20.6 s and 84.2 s contended, passes at 539.7 s and 726.9 s alone. It does
not hit its `TIMEOUT`; it exits early with a failure, so it reads as an ordinary defect. N26's
shape on a different cell.

> **Fifth measurement, 2026-08-13 evening — and it is recorded here rather than added to that
> series, because it does not belong to it.** The cell **passed at 1032.74 s** inside a
> `-j8` run of its own 577-cell family. That is neither of the two states above: contended *and*
> passing, at 1.4× the slowest solo time. **It is not evidence either way**, because the machine
> was not quiet in the sense the series means — an unrelated process held roughly six cores for the
> whole run, and by the time this cell was the last one left it had the box nearly to itself. Two
> uncontrolled variables moving at once is one measurement of nothing. It is written down so the
> next person does not read `577/577` as a fifth data point for "fails under `-j`", and so the
> series keeps its meaning: **the four figures above were taken deliberately, this one was
> observed.** Whoever settles this cell should fix the experiment before adding runs to it —
> `RESOURCE_LOCK`, a stated load, or both.

**Two prerequisites this host is missing, both of which silently shrink coverage rather than
failing:**

- `vendor/gentoo-stage3-{i386,arm,arm64,riscv64}-glibc` — four of the thirteen `ast/o0-baseline`
  keys cannot be measured, so they are **one row short** until a host with the sysroots re-banks.
  `qemu-aarch64`, `qemu-arm` and `qemu-riscv64` *are* installed, so the sysroots are the only
  thing between this box and execution-level coverage of three cross targets.
- `MCC_XSUITE_LLVMTS` — `jit/xoracle-coverage` skips rather than failing now, but `--min-cross 400`
  is a two-suite floor and one suite tops out at 379.

**And one that was missing and is not any more:** `vendor/gcc-c-torture-execute`, symlinked at
`~/Projects/gcc`. Until it was, `optlevel/torture-differential` skipped silently — a red recorded
from another tree is not a red you can watch from this one until its prerequisite exists locally.

### The suite's standing reds — whole-suite run, 2026-08-11

A full `ctest` over `cmake-def` (19k cells) leaves **7 red**. All seven were verified
pre-existing by reverting `src/mccrir.c` and `src/mccjit_embed.c` to `HEAD`, rebuilding and
re-running: they fail identically without this wave's changes. **None is a compiler defect** —
four are banks or tables that the tree moved out from under, and each names its own fix.
Listed because a whole-suite run is expensive and this list did not exist anywhere.

| cell | why | fix |
| --- | --- | --- |
| ~~`flagsweep/cover3-verify`~~ — re-opened and **CLOSED AGAIN 2026-08-12** | closed once (two flags had accumulated, `opt-search-predict` and `tree-sra`; regenerated to 76 rows over 111 varying flags). **`42cfc8ac` then added `tree-sroa` and `tree-sroa-params` and nobody regenerated again** — `cover3.txt` said `flags=116` where `src/mccopt.h` yields 118, and `cover3.py verify` exited 1 naming both. **A closure recorded on the same day the surface moved under it** | regenerated: **78 rows covering all 1,873,088 3-way settings of 113 varying flags of 118**. The standing "do not blind-regenerate" rule was honoured — the pin decision was taken first and is a no-op, since neither new flag is in the class `PINS` holds (flags whose *off* state is a diagnostic detour); **the same five flags remain pinned and the pin diff is empty** |
| ~~`optlevel/torture-differential` (+`-known-positive`)~~ **CLOSED 2026-08-13** | **0 unknown divergences.** It failed on *staleness*: `20020720-1.c` and `20041114-1.c` no longer diverge at `-O1`–`-O4`, so the known table over-claimed. **This is the ratchet working** — two known divergences were fixed and nobody dropped the rows | both rows dropped from `tests/optfire/leveldiff-known.txt` **in full**, not trimmed: no level survives for either program, and `parse_known` raises on an empty level spec. Table is 14 rows → 10. Both cells green |
| ~~`fmt/census-bank` (+`-known-positive`)~~ **CLOSED 2026-08-12** | every moved figure attributed to the commit that moved it; this session added zero printf sites | re-taken, attribution in [`docs/ARCHIVED.md`](ARCHIVED.md) |
| ~~`runtime-bench-check`~~ **CLOSED 2026-08-12** | `tests/runtime/branchy.c` seeded its data with signed overflow, so the kernel disagreed with its own fresh-reference-build oracle about the input | LCG computed in `unsigned`; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md) |
| ~~`cross/shadow-iv-x86_64`~~ **CLOSED 2026-08-12** | three harness bugs, none in the compiler: a hardcoded `cmake-debug` no tree has, an arch table that names a CPU and not an object format, and `timeout` which macOS lacks | fixed, plus the compile floor; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md) |
| ~~`ci/registration-stubs`~~ **CLOSED 2026-08-12** | it was **three** cells and **three** `else()` branches, not two and two: `jit/xoracle-coverage` is dropped by the corpus-exists test *and* by the outer python3/embed-jit/host-arch gate, so the lint had to be re-run twice before it went quiet | stubs added |

**All seven are closed as of 2026-08-13** — five on 2026-08-12, the last two with the
known-table drop above. **`fmt/census-bank` then went red again inside the same session**, when
`ca4ed540` added one `snprintf` in `so_fn_sizes`' Mach-O arm; re-taken with that attribution. The
lesson is not that the cell is noisy — it is that this bank moves with *any* commit that adds a
format call anywhere in `src/`, so it is red on a merge more often than on a mistake, and the
attribution step is the whole value. The rule this file
states — that a ratchet re-banked without a reason trains its readers to re-bank without
looking — is why `fmt/census-bank` was left red for a day and not why it should stay red: the
tool asks for the attribution, and once every moved figure has a commit against it the
attribution exists and re-taking *records* the movement instead of erasing it. Getting it
required checking that this session moved nothing, which it did not.

**The table was never the whole list, and a whole-suite run is not what found the gap.**
`stratsweep/check` was red at `52e7e850` on x86_64-linux and appears nowhere above, for the
same reason `rir/drop-ratchet` was missing from the arm64 table: it was reached by *running the
family*, not by a sweep. Its closure is **N31** below, and it is not a bank drift — the
sentinel it guards had silently stopped meaning what the whole `stratsweep` family assumes it
means. **Corollary worth keeping: `optlevel/torture-differential` had also been skipping here**,
because `vendor/gcc-c-torture-execute` is deliberately not vendored and nothing on this host had
symlinked it; a red recorded from another tree is not a red you can watch from this one until
its prerequisite exists locally.

**The arm64/macOS host has three standing reds of its own**, all verified pre-existing by
rebuilding at `7c2d3305`, none of them in the table above because that table was taken on
x86_64-linux where all three pass:

| cell | what it says |
| --- | --- |
| ~~`jit/bind-local`~~ **CLOSED 2026-08-12** | was **N24** — zero local binds across 6 programs, plus `collide_libc` giving 45314 under `MCC_JIT_KGC=0` against 58700 under `MCC_JIT=0`. Root cause was one `strcmp` in `mccjit_bind_apply` comparing the blob's undecorated bind key against a Mach-O symtab's `_`-prefixed names; both cells green and the JIT family is 65/65 |
| ~~`slice/census`~~ **CLOSED 2026-08-13** | was `blocks` 983 against a banked 990 on the `arm64`/`Darwin` column. The bisect this row demanded was done and the column re-banked with attribution at `ac69a0fd`: the mover is `82f39935`, a deliberate priced change (the ternary fold must not hide a tail call or a constant condition), which published its own cost on gcc.c-torture in its commit message and landed **46 minutes after** the column was set on a host that does not run this suite. Only `blocks` moved; the other five keys still match exactly. **Re-verified green here 2026-08-13, 1.33 s.** The re-bank instruction in the cell's own `FATAL_ERROR` was wrong and is fixed — it named `tools/slice-census.py`, a different instrument that prints none of these keys; the producer is `slicerun --census` |
| `rir/drop-ratchet` | **New to this table 2026-08-12, and pre-existing** — verified by rebuilding `src/` at `6c09fb7a` and re-running: it fails identically without this session's changes. `regaddi` is banked as a dropping reason but **does not appear in the report at all**, which the cell itself says is either a measurement that did not run or a reason that genuinely stopped dropping. It was missing from this table because every earlier sweep here filtered on `^rir-` (hyphen), which does not match `rir/drop-ratchet` (slash) — a reminder that a regex-selected sweep is only as complete as its regex |
| ~~`superopt/global-reload` (+`-known-positive`)~~ **CLOSED 2026-08-12** | **not a compiler defect** — `tests/superopt/globreload.awk` was written against GNU binutils objdump on ELF and four of its assumptions are format, not fact |

**`superopt/global-reload` looked like the one real defect of the three and was not.** Four
assumptions in the fact extractor are objdump-format, not compiler behaviour: Mach-O's leading
underscore (`_spin_wait` never matched `spin_wait`); LLVM objdump printing branch targets as
`0x148` where GNU prints `148`; LLVM objdump printing relocation offsets zero-padded to 16
digits where `pad()` pads to 8, so **every** in-loop test compared a 16-char string against an
8-char one and answered no; and `adrp x30, 0x0 <_spin_wait>` being read as a backward branch,
because the matcher never looked at the mnemonic. One more is a real ABI difference rather than
a format one: an arm64 GOT access is a `ADR_GOT_PAGE`/`LD64_GOT_LO12_NC` pair at consecutive
instructions, so `across_call` showed 4 relocations where the cell expects 2 accesses.

The extractor now unmangles one leading underscore, accepts an optional `0x`, normalises
offsets by stripping leading zeros before padding, requires the token before the target to be a
branch mnemonic (`b`, `b.cc`, `cb[n]z`, `tb[n]z`, `j*`, `loop*` — **not** `call`/`bl`, which
would make a backward call a loop), and collapses a relocation pair to the same symbol four
bytes apart into one access. Both cells pass on arm64/Mach-O.

**It was also already broken on ELF, silently.** Run against an x86_64 ELF object with LLVM's
objdump, the old extractor emits **zero** `LOOP` rows — so every loop check on that toolchain
was passing by measuring nothing. The `REL` rows are identical before and after; only the
offsets are normalised. Not re-verified against GNU binutils objdump, which is what the Linux
box has and what the extractor was written for; the changes are shaped to be no-ops there.

### Cross-cutting research, 2026-08-13 — four clusters read together, and what it changed

> Four read-only investigations over deliberately **overlapping** slices — the measurement
> instruments, the "decide first" items, the device cluster and the oracle cluster — asked for
> shared surfaces, ordering and contradictions rather than per-row detail. The headline is that
> **three of the four "decisions" are measurements**, one open row's central question is **now
> answered**, and several couplings this file asserts are refuted while one it never named is real.

**N7's central question is answered — the GPU arm catches the injection.** Run on this host after
the `points=` split: baseline `differing-files=0`, and with `r = s + 1` injected into
`ast_eval_slice.h`'s 32-bit signed `+` arm, `differing-files=4` and
`always_gpu_parity.sh` fails with `[ladder-cross] points=13663924 → 13664118`. **The delta is
exactly 194 — N7's own recorded figure.** The mechanism is confirmed in code: on `hr >= 0` the hook
short-circuits the rung and `ast_eval_binop` is never called, so the arm is a genuine independent
evaluator for the rungs it accepts.

**But read the qualification before closing N7.** In all four differing files, `pairs`,
`certified`, `differ`, `refused`, `exact` and every histogram are **byte-identical**; only `points`
moves, and not even consistently in one direction (one file's GPU count is *lower*). `points` is a
how-far-did-the-sweep-get counter, not a verdict, so the teeth are an incidental coverage-index
artefact. **What closed is the narrower claim that the arm is blind. N7's real work — an
independent oracle for the tree side — stands.**

**And the obstruction this file recorded was wrong.** `TIMEOUT 120` in `ladder_gpu_parity.cmake` is
**per `execute_process`**, i.e. per single-file compile (~0.5 s measured), and
`gpu/ladder-gpu-parity` carries **no ctest `TIMEOUT` property at all**. All four device cells are
green here in 145 s together. The "cannot finish inside `TIMEOUT 120`" note is struck.

#### The couplings, tested rather than asserted

This file has a bad record here — it recorded four refuted couplings on 2026-08-12 — so each was
checked against the code.

| claim | verdict |
| --- | --- |
| N7, N18, N6.8, the lowerable ratchet and N6.10 share machinery (`mccinv.h`, `emit-map.py`) | **refuted.** They are **two disjoint clusters plus a loner**: N18+N6.8 on `src/mccinv.h`/`emit-map.py`; N7+N6.10 on `src/ast_eval_slice.h`; the ratchet on `rir-coverage.py`/`mccrir.c`, sharing nothing with either |
| the ratchet needs a compiler change to `[rir-low-body]`'s row format | **refuted.** `census()` already holds `src` per row-batch — the fix is pure Python, no format change, no invalidated captures |
| N6.8's counter should be wired at the `mccjit_embed_note` gate | **refuted.** There are **five early returns after that gate**, including the `mccjit_intent_serialize` failure N6.8 blames `stash_leaf` for. It must go after the `e->next = mccjit_embed_fns` append |
| `emit-map.py` unblocks N18 | **refuted.** The whole N18 table reproduces **in 0.75 s** with `MCC_INV=1` on the ordinary build — no trace build, no `emit-map.py`. A cell watching N18 needs two compiles and a subtraction, not the 7200 s opt-in trace cell |
| item 22 and sweep row 29 share a policy | **refuted.** Both are settled by measurement, for unrelated reasons. What they *do* share is a file: `tests/smoke/bails.txt`, which is where both "say so" arms already live |
| item 22's `SValue` cost couples to another row | **refuted.** No other row names those `memcpy` sites; they cost nothing and couple to nothing |
| **`ast_func_end` is where three of the five actually meet** | **real, and this file never named it.** `src/mccast.c::ast_func_end` contains every Cluster-A counter site *and* N7's census entry within 110 lines — `mcc_inv_add ast.body`, the `ast_jit_dispatch_env` gate, three `mccjit_embed_note` calls, `ast.abort`/`ast.noreplay`, `ast_ladder_census`, and `jit.baked` |

#### One failure mode under three instruments — the thesis, confirmed

The working hypothesis was *an instrument that measures something adjacent to the thing it names*.
It holds, and the mechanism is the same in all three: **absent reads as zero, and nothing fails.**
`emit-map.py` uses `inv.get(k, 0)` throughout; `mcc_inv_add` truncates silently past `MCC_INV_MAX`;
`find_anchors` makes two of five anchors optional, so if they stop resolving both dropout counters
read 0 and `gap_explained` reads 0 with no diagnostic; `low_body_index()` returns `None` and the
whole per-body ratchet degrades **to the corpus-wide percentage it exists to replace**. **Making
those three loud is the single highest-value change across the cluster**, and it is small in every
one of them.

**~~Making those three loud~~ — DONE 2026-08-13, and it was small in every one of them as
predicted.** Both optional anchors are required and `die()` naming what stopped resolving; the
truncation reports `inv.dropped=N` on the dump line and both consumers treat it as fatal;
`low_body_index()` keys on the TU and returns *which* of its three causes fired. Added on top,
because it was the one thing the thesis implied and nobody had written: **`emit-map.py` now
cross-checks `dropout_abort` against `ast.abort + ast.abort_post` and fails on a mismatch**, which
is the check its docstring has claimed since the file was created. **One correction to the thesis
as stated:** `gap_explained` does not read 0 when an anchor drops out — `dropout_no_replay` comes
from per-body trace counts that need no optional anchor, so it degrades to that term alone. Same
fix, same conclusion; the number in the sentence was wrong.

**One correction to this file's own arithmetic while there:** the `MCC_INV_MAX` hazard is real but
mis-sized. The tree registers **16** keys, not the 12 recorded here, so six reasons × (count+bytes)
lands at **28 of 32** — *under* the cap. The conclusion (own table, own report line) survives; the
stated reason does not, and anyone sizing the work from that sentence over-builds.

**The `[inv]` prefix collision has three consumers, not one.** `fmt-census.py` raises an uncaught
`ValueError`; **`tools/node-census.py` silently appends garbage** — a wrong number with no signal,
the worst mode, and a consumer this file does not record; `tools/slicerun.c` silently truncates.
And `emit-map.py`'s `[inv]` channel is generic **only for integers** — `run()` does `int(v)`
unconditionally, so the moment a reason *name* goes on that line it dies. That is the argument for
a new prefix that actually bites, because unlike the collision it needs no unusual env var.

**~~The collision~~ — RETIRED 2026-08-13, by taking the prefix rather than documenting it.** The
counter channel is `[invcount]`; `MCC_ARENA_DUMP`'s `[inv] <node> <callee>` keeps `[inv]`, and its
three consumers can no longer meet the counter grammar under `MCC_ARENA_DUMP=-`. It cost one
`fprintf` and two lines in `emit-map.py` — the only consumer the counter channel ever had, which is
why this was cheap and why it should not have waited. `int(v)` is now guarded and names the
offending token, so a reason *name* on that line is a diagnosis rather than a traceback. The
argument the research made for a distinct prefix stands and is the reason it was done this way
rather than by hardening three parsers.

#### The three "decisions" that are measurements

**Sweep row 29 — settled three ways; keep the gate.** Its whole case rested on *"2.0% of bodies but
10.2% of body bytes getting no optimization at all"*, and none of that argument survives.
*(a) Structurally the flag does not gate the optimizer at all*: `ast_rir_nofb_env` has exactly one
consumer and it feeds `keep` — whether to restore `orig` — while the strategy admission gate reads
`faithful`, computed earlier by byte-comparing the replay. Flipping it does not hand those bodies
the optimizer; it retains an unoptimized, byte-divergent replay body. *(b) Empirically the flip
costs bytes on every corpus*: `src/mcc.c` **+840** at `-O1` and **+830** at `-O2`,
`tools/slicerun.c` **+517** at both, `tools/mcchv.c` unchanged. **Never negative — there is no byte
upside at all.** *(c) The figure itself has moved*: re-measured, bodies 2.18% (three instruments
agree) but bytes **7.65%, not 10.2%**, and the average discarded body is 1,764 B against a kept
476 B, not "2585 against 470". And leg 2 is worse than "one entry is banked": `nofb_miscompiles` is
a **known-bad allowlist**, so `O0: src/mcc.c::cleanup_symbols` is a *known miscompile of the
compiler's own source under the flip*. **The visibility half is confirmed cheap** — `rir_prod_gate
< 2` returns *after* all the counter accumulation, so "make it visible" is a print, not a
computation. The only decision left is the shape of the notice.

**Item 22 — settled by witness; mcc is the conformant side.** `2.25f16*255.0f16+0.5f16` gives mcc
`607d`, gcc-15 and clang `607c`, and gcc-15 `-fexcess-precision=16` `607d`. Decisively:
**gcc advertises `__FLT_EVAL_METHOD__ 0` in both modes**, so gcc's default contradicts its own
macro and mcc's behaviour is the one that matches it. Not a conformance defect. The "does not
predefine it" correction is half right — mcc *does* predefine it, but from
`runtime/include/mccdefs.h`, so the correction's operational conclusion (the fix is written against
the headers, not the compiler) stands.

**And the instrument that should have shown this was printing zeros — fixed (`8a70f8b7`).**
`smokerun`'s divergence triage formatted 128-bit F-row digests through a **`%.16s`** field. Every
F-row digest shares a leading run, so all three arms printed as the same zeros and the line carried
**no information at all** — the one print whose subject is this item destroyed this item's own
evidence, on all three verdict classes (`REFS-DISAGREE`, `DIVERGE-BOTH`, `DIVERGE`). Widened to
`%.32s`; `f16.muladd.doubleround.run` now reads `mcc=…607d gcc=…607c clang=…607c`, which is the
witness above, printed by the harness rather than reconstructed by hand. **This is the cluster
thesis in its purest form** — *absent reads as zero, and nothing fails* — and it is the only
instance found so far where the zero was **literal**.

**And item 22's cost model is attached to the wrong mechanism.** `sizeof(SValue)` is **80, not 64**,
with a **12-byte interior hole and 8 bytes of tail padding** — a new field costs zero size and zero
offset change. The bulk copies are all `sizeof`-driven (nine `memcpy` + two `memmove` across seven
functions, not "eight places") and need **zero edits**; `SValue` is never `memcmp`'d and never
serialized, so "a new field joins the RIR replay record" is not a correctness hazard. **`CType.t`
is genuinely full, but `VT_BTYPE` is not** — it is `0x1f` with 16 of 32 encodings used, so a
`VT_FLOAT16_WIDE` costs no new bits and never touches `SValue`. **The fact that refutes this cost
model was already in this file** under *"Couplings deleted, because verification refuted them"*,
established for the wide256 cluster and never connected here. The real cost is breadth — 838
`VT_BTYPE` uses to audit — which is mechanical and greppable, a very different estimate.

**~~`int128-signedness` — settled; delete it.~~ — CLOSED IN FULL 2026-08-13.** The header half
landed first: the 44 lines below are gone from `runtime/include/mccdefs.h`; `nm -u` was re-run here
first and still shows zero undefined `__mcc_*o_*`, and 391 overflow/int128/builtin cells are green
after. **The two `runtime/lib/` halves were then taken as well, as the decision they were, on
x86_64-linux the same day.** `runtime/lib/builtin.c` loses `MCC_OV_SMALL_{S,U}` /
`MCC_OV_BIG_{S,U}` and their 14 instantiations, `runtime/lib/int128.c` loses `MCC_OV_WRAP` and its
6, plus `mcc_ov_{from,to}_abi`, the `mcc_ov_{s,u}int128` ABI typedefs and the three
`__mcc_{add,sub,mul}o_uti_impl` that only the wrappers reached. `libmccrt.a` goes from **20
exported `__mcc_*o_*` symbols to none**; the three `__mcc_{add,sub,mul}o_ti_impl` survive as file
statics, and `__addvti3`/`__subvti3`/`__mulvti3` are still `T`. 391 overflow/int128/builtin cells
green, `ctest -R "^smoke/"` 12 of 12 in 227.4 s.

**What made it safe to take rather than a sweep, restated as the argument and not the conclusion:**
with the declarations already gone, no TU this compiler preprocesses can *name* the symbols —
`__builtin_{add,sub,mul}_overflow` expand to `__mcc_ov_gen`, whose `__mcc_ov_calc`/`_w` path is
`static __inline` and handles `__int128` inline, verified here by compiling all four widths and
reading `nm -u`: **zero** undefined `__mcc_*o_*`, at `-O2` and with `MCC_CONFIG_PREDEFS=0`. The
skew the row feared cannot occur inside a prefix (`runtime/include/` and the archive install into
the same `_mccdir`), there is no runtime ABI to break (a static archive), and no symbol/ABI
baseline test exists to contradict.

**Found on the way, and it is a compiler-side residue this file had no row for.**
`src/arch/x86_64/x86_64-gen.c::gen_ovf_addsub` — with `ovf_inline_on()`, i.e. `MCC_OVERFLOW_INLINE`
— exists to intercept a *call* to a symbol named `__mcc_{add,sub,mul}o_*` and emit the
add/sub/imul-plus-`seto` inline instead. Nothing can make that call any more: the header half
already removed the only declarations, so the interceptor has been unreachable from any
mcc-compiled TU since that commit, and this half removes the definitions it would otherwise have
fallen back to. **It is deliberately left in place** — it is the one thing that still makes a
hand-written `extern int __mcc_addo_i(long long, long long, int *);` work now that the library
symbol is gone, and deleting emitted-code paths on a dead-declaration argument is a second
decision, not this one. Its two bodies are banked in `tests/rir/lowerable-bodies.tsv`
(`gen_ovf_addsub`, `ovf_inline_on`) and **no cell exercises the path** — worth knowing before
anyone reads that inventory as coverage.

As originally settled:
`nm -u` on a program using all four
`__builtin_*_overflow` widths shows **zero** `__mcc_*o_*` references: `__mcc_ov_gen` never mentions
`__mcc_ov_disp` at all, so the dispatcher and all 39 declarations are orphaned *in the header*.
Three facts shrink the decision to nothing: **no symbol/ABI baseline test exists** anywhere in the
tree; `runtime/include/` and the archive **install into the same `_mccdir`**, so the "new `.a` +
old header" skew the row worries about cannot occur within a prefix; and it is a **static
archive** — no runtime ABI, and any rebuild takes the new header too. **Free win the row does not
name:** those 44 dead lines are injected into **every TU mcc compiles** under the default
`MCC_CONFIG_PREDEFS=1`.

**S7b is the one that really is a decision** — and the only item bound by others: `L4b` and
`jit-teardown-unbounded` both wait on it, which is the strongest "deciding one settles another"
link on the board. Its census has drifted: the pthread counts are now `mccjit_embed.c` **159**
(was 139), `mccrun.c` 23, `mccast.c` 4, `mccgpu.c` 3; there are **four** in-tree consumers of
`threads.h`, not one; workers are **not** `pthread_detach`ed any more (L2′ landed); and every line
number in its suspension-point table is stale by 200–1,300 lines. Its own argument is *stronger*
than written — `sv_stack` is 41,040 B, not the 32,832 B recorded.

**Two numbers to correct regardless of any decision:** row 29's 10.2% (now 7.65%, and attributed to
the wrong predicate), and `sizeof(SValue)`/`sv_stack` — **80 / 41,040**, not 64 / 32,832, which is
a 25% undercount on an N8 stack-frame row.

#### The device cluster — N34's rule generalises, and it names the order

**Wiring the device into `mccjit_shutdown()` is the correct fix for N6/`L2`, and its acceptance
test already exists and already passes.** `slicerun --only quiesce` is green here, and its own
comment names the target: *"the call that cluster L wants to reach from `mccjit_shutdown`"*.
`mcc_gpu_quiesce` is idempotent — it clears `mcc_gpu.dev` and guards everything on it — so a second
call issues no Vulkan at all.

**But it is safe today by accident, not by rule.** `atexit(mccjit_shutdown)` is registered at two
sites: one after `ast_ladder_gpu_setup` (safe by construction) and one in `mccjit_kgc_register`
that **nothing sequences against the device coming up**. That is N34's exact geometry; it survives
only because `ast_ladder_gpu_report` quiesced first.

**So N34's declined belt-and-braces fix becomes correct the moment L2 lands.** N34 rejected
`atexit(mcc_gpu_quiesce)` in `mcc_gpu_init` because it moves teardown ahead of `mccjit_shutdown`,
`mcc_stats_finish` and `rir_report` — **that objection dissolves once `mccjit_shutdown` quiesces
anyway**, because the second quiesce is a proven no-op. And it is the only registration correct *by
construction*, since `mcc_gpu_init` is what calls `vkCreateInstance`. It also closes N34's stated
residual, which L2 alone does not: `atexit(ast_ladder_gpu_report)` sits **outside** the
`if (mcc_gpu_emit(...))` guard, so if the warmup ever fails to dispatch, N34 returns.

**The `mcc_gpu_mem()` retention hazard is real and it is in `slicerun`, not the JIT.**
`src/mccjit_embed.c` never calls `mcc_gpu_mem`. `frame_ptr_arm` stores the base into the
file-static `ast_eval_slice_rw`/`_rw_base`/`_rw_nbyte` and is called unconditionally before every
suite — **including `suite_quiesce`**, which then unmaps the region while those globals still point
into it. Not currently dereferenced afterwards, so it has never fired: **the invariant is
unenforced, not satisfied.** Nulling them in the release path turns a silent use-after-unmap into a
null deref.

**MoltenVK imports the whole hazard class onto macOS, and that is not written down.** Darwin's
Metal `mcc_gpu_quiesce` destroys neither device nor instance and never clears `mcc_gpu.dev` — the
"quiesce destroys nothing" shape that predates the hazard. MoltenVK moves macOS onto the
destroy-everything Vulkan path with MoltenVK itself as a `dlopen`'d ICD registering its own
teardown. **"~10 lines, half-built" is accurate for the build config and wrong for the risk**: it
ports N6/L2 and N34 to a platform that has never had them, on a host nobody can reproduce them on.

**Order: `atexit(mcc_gpu_quiesce)` in `mcc_gpu_init` → L2 → MoltenVK, never MoltenVK first.**
`SR_GLOB_STRIDE` is **genuinely independent** — pure arithmetic, no device lifetime — and its
honest fix is smaller than the row implies, because `slicerun_obj` already returns a per-object
extent indexed by the same pair `slicerun_reloc` receives. **Acted on the same day** (`8a70f8b7`):
that extent is exactly what the new guard compares against the stride, so the correctness half cost
two lines and the row is now a coverage item. See item 5 under the funnel list.

#### The oracle cluster — one amendment covers every case

This file's rule treats *the reference's answer* as a scalar. It is a function of five variables,
and each open case is a different one going plural: implementation (covered), **implementation
count = 1** (N23, N29), **version** (N29's two hosts), **documented flag** (item 22), and **`-O`
level / fold-vs-run** (N30). The smallest amendment:

> …or where **any** reference fails to give one answer. A reference adjudicates only where its
> answer is invariant across the axes we vary it on — implementation, version, `-O` level,
> documented flag, and fold-versus-run. Where a reference is multi-valued on any axis, the case is
> latitude: pin mcc's answer, record the disagreement, and **name the axis**. Two references are
> two only if they are two implementations.

**Live proof, at the harness's own level.** `SMK_REF_FLAGS` is `-O1`, and on N30's `0x7c01`:
gcc-15 gives `fold=fc01 run=fe01` at `-O1` — **the reference contradicts itself, at the one level
the harness looks at**, and the harness books the two columns as independent verdicts without ever
asking whether they agree. **A fourth class on the fold/run axis is free** — both columns are
already parsed, zero extra compiles. A class keyed on `-O` level is structural (one `ref_build` per
reference) and should wait.

**There are three independent reference resolvers with different ladders**, and this is not
Mac-specific: `mcc_find_gnu_gcc` prefers **gcc-16** and is `--version`-validated; `MCC_SMOKE_GCC`
prefers **gcc-15** and is validated by nothing; `smokerun`'s own default is bare `gcc-15`. **On any
host with both installed, `slice/cref-oracle` adjudicates against gcc-16 while `smoke/divergence`
adjudicates against gcc-15, in the same tree, at the same time, with no record of either.**

**No bank records a reference version anywhere.** Rows are `<scope> <key> <count>`; the only
provenance is hand-written prose that the documented re-bank procedure destroys. **The landmine:**
a reference upgrade moves ratchet counts with zero signal in the diff, reported as a regression or
an `IMPROVED`. **It has already fired and the bank shows it for free** — `bails-arm64-macos.txt`
has 253 `diverge-one` and **zero** of the two classes that require a second implementation. That
histogram *is* N29's fingerprint, and a three-line assertion catches the whole class.

**The overlap that matters: one change settles four items.** Resolve `MCC_SMOKE_GCC`/`_CLANG`
through the already-audited `DIFF3_*` path, and have `bank_write` emit the resolved pair's
`--version` as a machine-readable line the ratchet checks. That is N29's prescribed fix verbatim,
plus the two-ladder split, plus the audit gap, plus retiring smokerun's private family probe. It
does **not** settle N30 — that needs the fold/run class. **Two changes, this one first.**

### Next steps, prioritised — re-ranked 2026-08-13 (FOURTH pass, after the instrument wave)

> **The third pass's ranks 4, 5 and half of 8 are closed**, and the wave that closed them changed
> what should come next, so this is a re-rank and not a re-check. The rule is unchanged:
> cheapest-with-teeth first, then by shared surface, then by severity, with the host stated on
> every entry. **The third pass is kept below in full, struck where it is closed**, because its
> refutations are still load-bearing.
>
> **What is now first on THIS host is rank 3 of the third pass, unmoved and unclaimed:
> `if-conversion-abs`.** Every entry above it in that list has either closed or belongs to the Mac.
> It has been the strongest *measured* item on the board across three consecutive passes and it has
> been skipped each time, which is a fact about the ranking rather than about the item: it is the
> only entry whose cost is a **bench run on a quiet host** rather than a code change, and every
> pass has preferred the code change. **This host is not quiet** — an unrelated process held ~6
> cores through the 2026-08-13 evening run — so whoever takes it must say what the machine was
> doing, or the n≥20 pairing buys nothing.
>
> **Live board rows: N3 (items 22 and 23), N6, N7, N29 — four of thirty-five.** N18 leaves this
> line: its instrument half is closed and watched, and what remains of it is a defect, tracked in
> the row rather than as a next step. N6.8 is closed in full.
>
> **New, from doing the work rather than from planning it:** `tests/emitmap/bank.json`'s
> tolerances are wide enough to be unfalsifiable (`TOL` 1.0 on every percentage, and the selfhost
> cell is *already* drifted inside them); and `rir-coverage.py`'s `wide` corpus reports **9
> source(s) failed to compile** and computes every percentage over the remainder. Both are the
> cluster thesis one level up — a number that is present, and cannot fail.

**~~1. N29 — flip the smoke oracle, and take the 12 candidates with it.~~ — CLOSED 2026-08-13 on
the Mac, both halves in one commit.** The flip is the one line the entry priced. The triage that
gated it went further than the entry's estimate in both directions: the candidates are **10, not
11** (the entry's arithmetic; the row's own enumeration is 12 and N30 closed two of them), and
**all ten are NaN sign or payload — none is real**, against the entry's "2 real, of which N30 was
one". The other survivor the earlier triage counted was `bsweep.F16.FNEG`, i.e. N30 again under a
second name, so the "2 real" was one defect double-counted.

**What makes the closure stick is the method, not the verdict.** The ten were replayed point by
point with a harness extracted from `smc_hash` **whose digests reproduce the subject's byte for
byte on all three compilers** — without that check a per-case replay is a different program from
the cell and proves nothing about it. 112 points where the references agree and mcc differs, all
112 a quiet NaN in both answers, 46 sign-bit-only and 66 payload, zero finite. smoke is 12/12 in
209.5 s at `-j2` with the flip in.

**It cost two new rows, and they are worth more than the closure.** **N36**: mcc's over-wide
bit-field semantics are split — 168 of 185 `bf*` rows match gcc, the 17 `*.EQM1` rows match
clang, and a one-reference panel showed both arms as the same `diverge-one`. **N37**: the
refs-disagree class this file introduced *to fix N29's own triage* is computed per digest, so
`csweep.C64/C80.CDIV` and `CDIVSEL` are cleared while each holds 283 points where the references
agree and mcc differs, 44 of them finite and three of them not rounding at all. The lesson the
entry was already carrying one level up holds one level down: **a verdict class is only as good
as the granularity it is computed at.**

**2. N7 — re-run the injection under the GPU arm.** *Any host with a working device; this one.*
The instrument this row needed now exists: the `[ladder-*]` panel prints `points=` on its own line,
so both `ladder_gpu_parity.cmake` and `always_gpu_parity.sh` can see the one field N7's `r = s + 1`
injection moves. **This is the cheapest remaining step on the board and it is the one that turns
N7 from a story into a measurement.** Budget an unsandboxed run: `ladder_gpu_parity.cmake` gives
each arm `TIMEOUT 120` and the device arm is 2.5–2.7× slower by design. Residual the arm still
cannot see even then: the `n == 0` const case, the corner sweep, the observed rung, and every rung
the GPU refuses.

**3. `if-conversion-abs` ships at level 2 and its own bench says it makes code worse. — NOW FIRST
on x86_64-linux, 2026-08-13, by everything above it closing or belonging to the Mac.** *Any host;
an arm64 re-take is meaningful.* Unmoved through three passes and still the strongest *measured*
item on the board. **Two things to settle before starting, both learned the hard way this
evening.** *(a) The machine.* This is the only entry on the list whose deliverable is a
measurement, so it is the only one contention can silently corrupt — and this host had an
unrelated ~6-core process running through the evening. `rir-nofb-probe-self` **passed at 1032.74 s
under that load**, which is worth exactly one sentence and no bank row: it neither confirms nor
refutes the "fails under `-j`, passes alone" note, because the load was not the `-j` and was not
measured. Take the bench on a quiet box and say so. *(b) The table re-take is the larger half.*
Demoting the flag breaks `levelbench.tsv`'s assertion that its 16 rows match `src/mccopt.h`'s 16
`LEVEL(1..3)` rows, and the fan-out is already recorded here: `cover3.txt`, `defstate.txt` and a
tab-anchored `sed`. Budget for the fan-out, not for the one-line level change.

Original entry: `gain_movers −0.5668`, `gain_pct −0.0334`, `branchy −0.5700`, reproduced to the
printed digit across two runs, plus a compile-time dividend the row never quotes (`cost_self
0.2109`, `cost_corpus 0.0716`). It is arch-neutral — `ast_abs_env` is an AST rewrite with no
`arch.txt` row. Two caveats: the flag moves **1 of 17 kernels**, so budget a paired n≥20 run; and
demoting it breaks `levelbench.tsv`'s assertion that its 16 rows match `src/mccopt.h`'s 16
`LEVEL(1..3)` rows.

**~~4. Cluster 2 — the counter substrate, with N6.8 first.~~ — CLOSED 2026-08-13.** All three
hazards discharged, and the figure the row existed to correct moved further than it predicted.
`jit.embed`/`jit.embed_bytes` count the append to `mccjit_embed_fns`, placed **after** the append
rather than at the `mccjit_embed_note` gate (five early returns behind it, including the
`mccjit_intent_serialize` refusal). Self-host `-O0 --embed-jit`: **1278 bakes, 1577 stash attempts,
3202 verdicted — 39.91%, not the published 49%**; `full_language.c` 39/41 of 299 — **13.04%, not
14%**. `EMIT-MAP.md` and `EMIT-COVERAGE.md` corrected in place. `MCC_INV_MAX` truncation now
reports `inv.dropped=N` on the dump line, and **the `[inv]` collision is retired rather than
worked around** — the counter channel is `[invcount]`, which the three consumers of
`MCC_ARENA_DUMP`'s grammar cannot meet. Banked by `ast/inv-faithful`.

**~~5. The per-body lowerable ratchet is disarmed and nobody noticed.~~ — CLOSED 2026-08-13.**
`low_body_index()` keys on the TU as well as `(file, func)`; `lowerable-bodies.tsv` gained a `tu`
column that still reads the 3060 legacy 5-column rows, so the schema change retired no banked body;
and a **`wide`/`elf` inventory of 4574 bodies** was taken — the other branch of the hedge, which
would indeed have fired. `rir-coverage-census` now attributes **4550 bodies at `-O0`, 0 gone, 0
new**. **The known-positive is the part worth copying**: perturbing two banked masks makes the cell
name `<command line>:__mcc_ov_calc` in each of its two TUs — the exact collision that disarmed it —
and building that probe is what caught three format strings in this wave's own change that had not
been widened with the key. **New switch `--update-bodies`** writes only the inventory;
`--update-bank-low` still writes the floors with it. Adding bodies cannot hide a regression, moving
a floor can, so they are two decisions and now two flags. No floor moved and
`tests/rir/coverage-bank.json` is byte-identical.

**Not closed by the above, and re-filed here because the run surfaced it:** the `wide` census
reports **9 source(s) failed to compile** and computes every percentage over the rest. Identical at
`HEAD` and after, so pre-existing — it is sweep row 22's "denominator is the files that happened to
compile", now with a count attached. A source dropping out still shrinks the ratchet silently.

**6. ~~Cluster 1 — `src/mccast.c`, three items, not eight.~~ — TWO ITEMS, NOT THREE, and the word
"Unchanged" was wrong when it was written 2026-08-13.** *Any host to write; the two x86_64-gated
`rf-1` cells are named below.* **Sweep row 27 closed 2026-08-12 at `0e91e31d`, the day before this
entry called it open** — the five arena mutators all carry gate bits now (`AST_SG_VLAT`,
`AST_SG_MATHPRE`, `AST_SG_INTERCHANGE`, `AST_SG_FUSION`, `AST_SG_TILE`, `src/mccgate.h`), produced
by `ast_search_gates_now`, consumed by `ast_search_gates_set`, and the real mask reaches both JIT
ingestion points and is serialized as `warm_gates`. Two things this file also has wrong about it:
`AST_GATE_BITS` is 48 and the bits run to **46**, not 41; and rank 2's supporting sentence
"`AstReemitFn` does not even record where the orphan was" is **refuted** — B2 gave it `body_ind`,
`body_len`, `reloc0` and `rel_len` on 2026-08-12.

**Sweep row 25 is done — 2026-08-13, and it is filed in the wrong cluster.** The defect is entirely
in `src/ast_eval_slice.h`, not `src/mccast.c`; rank 6 of the *second* pass files it correctly. See
its own entry below.

**What is actually left of this cluster is one defect at two altitudes: sweep row 17 and `rf-1`.**
Both are "bytes that reach `.text` that no size metric attributes", and both measure with the same
expression, `ind - ast_body_ind_sv`. Row 17 is **worse than its own row says on this tree**:
re-measured 2026-08-13 at `-O3` on a self-compile it is **45 functions / 77,914 B ≈ 5.05% of
emitted body bytes**, against the banked 27 / 52,022 / 3.6%; the counters (`ast.orphan_fn`,
`ast.orphan_bytes`, `ast.orphan_relbytes`) now exist under `MCC_INV=1` but **no cell and no bank
reads them**, so the row's "no cell, no bank" clause still stands. `rf-1`'s severity is lower than
the row implies and this file never says so: **both its call sites are unreachable in a shipped
configuration** — `OPT_SEARCH_EMIT_SIZE` is `MCC_OPTD_OFF` and `OPT_PERFN_INPROC` is a level-8 dev
flag whose own gate wants `INLINE_FUNCTIONS` off. Its x86_64-gated pair is `cli/perfn_inproc` and
`cli/perfn_search` — and both are gated on **`os=linux` as well**, which this file omits, while
`optfire/perfn_inproc` and `flagsweep-exec/opt-perfn-inproc` cover the same flag on every arch. The
per-symbol size oracle both halves want already exists out of process as
`so_fn_sizes`/`so_macho_fn_sizes`, used by `mcc_superopt_perfn`.

**7. N6 — `L2`, wire the device into `mccjit_shutdown()`.** *Any host with a device.* Its two
preconditions are discharged; the live hazard is that the quiesce unmaps the shared address space,
so nothing may retain a `mcc_gpu_mem()` pointer across shutdown. **N34 is the cautionary
precedent**: the same teardown, reached in the wrong `atexit` order, segfaulted every compile that
touched the device.

**8. N18 — the `-O0 --embed-jit` faithfulness gap. — WATCHED 2026-08-13; the gap itself is what is
left.** *Any host; nothing here is arch-gated any more.* The row is no longer unobserved: the
**cheapest item in the cluster is done in both halves**. `emit-map.py` now reads
`ast.abort`/`ast.abort_post`/`ast.noreplay` and **fails** when `dropout_abort` disagrees with them —
the check its docstring claimed for as long as the docstring existed — and its two optional anchors
are required, so a dropout is a diagnosis rather than a zero. And the gap has a cell:
`ast/inv-faithful` (+ known-positive) reproduces `full_language.c` **3.32% → 4.68% (+1.36)** and
self-host **2.18% → 3.37% (+1.19)** from two `MCC_INV=1` compiles, in **1.7 s on an ordinary
build** — which is why the "do not bank from this one" caution no longer applies to it: its bank
key carries the arch and it needs no trace build to be re-taken anywhere. **What is still open is
the defect, not the instrument**: why the `--embed-jit` re-emit is 1.2–1.9 points less faithful
than `-O1` on every target and both architectures measured. The emit-map cells stay opt-in and
trace-only; nothing about them changed except that they can now fail.

**9. N3's residue — items 23 and 22.** *x86_64-only; this host.* Item 24 is fixed. **23** is
quality-of-implementation with no non-UB reproducer on x86-64, so it is genuinely low. **22**
(`_Float16` evaluation format) is *not code, decide first*: mcc predefines `__FLT_EVAL_METHOD__ 0`
and its per-operation rounding is what that macro promises, so the honest reading is that the
"defect" is a documented choice — and the fix is written against
`runtime/include/{mccdefs,float.h}`, not the compiler.

**Not code, decide first:** sweep row 29 (the `MCC_OPT_REPLAY_FALLBACK` flip — **make the fallback
visible under either answer**, and note one of its four legs is falsified: `nofb_miscompiles` is no
longer empty) and the coroutine task S7b. `int128-signedness` **leaves this list 2026-08-13 by
being finished, not by being re-scoped.** It left once already that morning, when the 44 header
lines turned out to be declarations rather than exports; the `runtime/lib/` half that remained
*was* a decision about exported runtime symbols, and it was taken the same day — 20 exported
`__mcc_*o_*` symbols gone from `libmccrt.a`, the three live `_ti_impl` kept as statics. The row
above carries the argument. **The one thing it left behind is a compiler-side residue, not a
runtime one:** `x86_64-gen.c::gen_ovf_addsub` intercepts calls that nothing can now make, and is
kept on purpose.

**~~Belongs to the Mac:~~ — ALL THREE CLOSED 2026-08-13, and two of the three were already done
when this line was written.** N29 is closed above and cost N36 and N37. `slice/census`'s
`arm64`/`Darwin` column was re-banked with its bisect at `ac69a0fd` and is green here in 1.33 s.
The Metal region differential's MoltenVK step landed at `2eee6c41` and the whole SPIR-V arm —
including `gpu/spv-mem-binding`, the binding-2 region cell — is green on this host in 74.8 s; the
real gap was that CI installs `molten-vk` without `vulkan-headers`, which is now fixed. What is
left on the Mac is new: **N36**, **N37**, and `ci/must-run-registered`, which is red here and
which no table in this file listed.

### Next steps, prioritised — SUPERSEDED, re-verified against the tree 2026-08-12 (second pass)

> **Every entry below was checked against the code by a read-only sweep before it was ranked,
> and the previous ranking did not survive it.** Two of its top four are gone: rank 3 was
> **already closed** eight days before it was written, and rank 2 does not do what it claims.
> The ordering rule is unchanged — cheapest-with-teeth first, then by shared implementation
> surface, then by severity — but the entries are now sized from the code rather than from the
> row that filed them. Host is stated on every entry.
>
> **What the previous pass got wrong, in one place:**
>
> | old rank | claim | verdict |
> | --- | --- | --- |
> | 1 | N26 is `-j` contention, fix with `PROCESSORS` | **wrong mechanism** — a cell times out *alone*; see N26's closure |
> | 2 | `MCC_RIR_STAMP` flip is free and the best value per line | **refuted** — no emitter *or* dump reads `ast_stype_*`, so the flip alone changes nothing it advertises; it also breaks two ratchets and is invisible on this host |
> | 3 | six binary opcodes have zero coverage | **already closed 2026-08-08**, archived 2026-08-10 — `slice/ops` enumerates all six with a `g_op_seen[]` ratchet and skips 77 rather than green |
> | 6 | eight `src/mccast.c` items share `ast_func_end` | **three do**; the four depth-inline rows are in `src/slice_inline.h`, and the eighth — the bare ID this file wrote as `res-d4b`, defined nowhere and now retired — was already fixed by `0cfe71a7` |
> | 7 | N19 forces the `emit-map.py` refactor N6.8 wants | **refuted** — the two edit sets are disjoint; N6.8 arrives on an already-generic channel |
> | 8 | `rir_fcrec[]` is the consume-by-fit pattern to copy | **refuted** — it keys on one byte and is an equality relation, not a fit relation |

**~~1. `flagsweep/cover3-verify` is red right now, and this file records it as closed.~~ — DONE
2026-08-12.** Regenerated to 78 rows / 113 varying flags of 118, same five pins, cell green.
See the standing-reds table.

**~~2. Three floors that make three gates structurally unable to fail.~~ — DONE 2026-08-13, and
the entry was wrong about which hole is the sharp one.** It is **five** `exit 77` sites, not
three: `stratsweep.sh` has one per mode (`iso`, `seq`, `perm3`) and `flagsweep.sh` has one each
for `exec` and `cover`. All five now `exit 1` with the drop list printed, which is correct
because each sits *after* a `missing`/`missing_named_subjects` guard that landed in `7be4fba5`:
once every named subject is known present, a zero count can only mean the `-O0` reference build
or the admission run failed for all of them, and there is no legitimate skip left on that path.
Verified by running each with `MCC=/nonexistent/mcc` — all five were green before and are red
now.

**The tab-anchoring story was already fixed and this entry did not know it.** `7be4fba5` added
the row-count cross-check (`flags_from_table` tab-anchored against `flag_rows_in_table`
whitespace-anchored), so reindenting `src/mccopt.h` fails loudly today. **The residual hole is
larger than the one described**: `flagsweep/accept` discards the driver's exit status (`|| true`)
and only grep-matches the message, so with `MCC=/nonexistent/mcc` it printed
`PASS flagsweep-accept: 118 flags accept -f and -fno-` and returned **0** — green for a compiler
that does not exist. That, not the row count, is the guard `dev-gate` has and `accept` lacked.
Closed with a negative control: a bogus `-fnot-a-flag-xyzzy` must be reported as unsupported
before the sweep is believed.

**One thing this entry could not have found, because it is in CMake and not in the scripts.**
`_ss_rows` was a hand-written mirror of `STRAT_NAMES` and had drifted two rows — **`sra` and
`sroa` had no `stratsweep/iso-*` cell at all**, and `stratsweep/check` could not see it because it
compares the *script's* list to the registry, never CMake's list to either. `_ss_rows` is now
derived from `stratsweep.sh names` and floored at 24. +4 cells (9561 → 9565).

**~~3. N24 is root-caused, and it is a Mach-O defect, not an arm64 one.~~ — DONE on the Mac,
2026-08-12.** The prescription below was right: fix the comparison side, not the serialization
side. *Small, and only that
host could do it.* The bind blob is *correct* — it carries `_random`'s real link-time address under
the key `random` — and `mccjit_bind_apply` compares that key against a symtab whose names carry
Mach-O's leading underscore. `strcmp("_random", "random")` never matches, so no bind is applied,
the symbol stays undefined, and the host resolver silently binds libc's `random()` instead. That
is the whole of `collide_libc`'s `45314` vs `58700`. Every other name lookup on this path builds
the `_`-prefixed buffer; this is the only raw comparison. **Fix the comparison side, not the
serialization side** — the blob format is written by ELF hosts too. Second half of the work:
`SHN_ABS` will fire on the Mach-O relocator for the first time, which is unproven.

**4. `if-conversion-abs` ships at level 2 and its own bench says it makes code worse.** *Still
the strongest measured item on the board, and still not one line.* `gain_movers −0.5668`,
`gain_pct −0.0334`, `branchy −0.5700`, reproduced to the printed digit across two full runs, plus
a compile-time dividend the row never quotes (`cost_self 0.2109`, `cost_corpus 0.0716`). It is
genuinely arch-neutral — `ast_abs_env` is an AST rewrite with no `arch.txt` row — so an arm64
re-take is meaningful. Two caveats this file understates: the flag moves **1 of 17 kernels**, so
budget a paired n≥20 run rather than one shot; and demoting it breaks `levelbench.tsv`'s own
assertion that its 16 rows match `src/mccopt.h`'s 16 `LEVEL(1..3)` rows, forcing a table re-take.

**~~5. Make the ladder's own differential able to see the field that moves.~~ — DONE 2026-08-13,
and it was two blind gates, not one.** `ast_ladder_dump_one` now prints `points=` on its own line
and `secs=`/`us-per-pair=` on a second, so both consumers start working as already written:
`cmake/ladder_gpu_parity.cmake`'s `list(FILTER … EXCLUDE REGEX "secs=")` **and**
`tests/gpu/always_gpu_parity.sh`'s `grep -v 'secs='`, which the entry did not mention and which
covers `[ladder-self]` as well as `[ladder-cross]`. A field filter in CMake would have fixed one
and left the other blind, and would also have had to strip `us-per-pair=`; the split makes the
invariant structural — **any panel line without `secs=` is semantic**. Census taken to confirm
the premise: 8 lines per tag, exactly one of which carries `secs=`, and it is the only one
carrying `points=`. No consumer needed them on one line (`tools/smokerun.c` reads
`[ladder-self] pairs=`; `always_gpu_parity.sh` reads `points=` line-agnostically).

**What this now exposes, and it must not be re-masked.** The `--jit-always-gpu` headline at the
top of this file — *"the panels are identical to the CPU arm across 24,307 pairs and
2,357,085,080 evaluated points"* — was produced by `always_gpu_parity.sh`, which **masked the
`points` line out of its own diff**: that figure was read from the GPU arm and never compared.
If either GPU cell goes red now, that is the measurement N7 asked for. Owed next, and it is the
whole point of the fix: **re-run N7's `r = s + 1` injection under the GPU arm.** Not done here —
this host has the device but a census run under `MCC_AST_EVAL_LADDER_GPU=1` did not finish inside
`ladder_gpu_parity.cmake`'s own `TIMEOUT 120`, so the re-take needs an unsandboxed run.

**6. N7, restated — it has one cause, not two.** *Medium-to-large. Any host for the fix.* The
second cause is a category error and should be **deleted from the row**: the 532 certifications
are produced by `ast_ladder_census`, which runs *after* the re-emit, tallies into two file-static
structs read only by an `atexit` dump, and frees its arena. There is no gate, no consumer and no
surviving state — nothing for N1's mechanism to transfer to. Worse, the invariance cited as
evidence is *entailed* by the first cause: the self-pair compares `E` against a structural copy of
`E` under the same evaluator, so "the 532 did not move" is a theorem, not a measurement. The real
row is one line: **the ladder has no independent oracle.** Rank 5 is its cheapest partial answer;
N6.10 is where "does a certification reach codegen" gets a real one.

**~~7. N2 — the two unchecked replay slot streams.~~ — CLOSED IN FULL 2026-08-13**, A1 through A7
plus A6's `nc[]` resync. *Small. Any host to write.* Understated
rather than overstated: the streams do not skip the check, they have **nowhere to put the data** —
no `sz`/`al` arrays exist on either. And a defect fell out of verifying it: **`dd80e4fa`'s fix is
bypassed entirely on the C2 path**, because `ast_alloc_loc` runs `rir_loc_replay` before
`ast_locrec_take`, so whenever `rir_c2_active` the checked path is never reached. Copy
`ast_locrec_take`, not `rir_fcrec[]`. Smallest of the family is `rir-locrec-skip-byfit`:
`ast_locrec_skip` is a blind cursor bump whose size is already in hand four lines above.

**8. Cluster 1 — `src/mccast.c`, three items, not eight.** *Any host to write; both `rf-1` cells
are x86_64-gated.* Sweep rows 17 and 27 and `rf-1` genuinely share `ast_func_end`. Rows 17 and
`rf-1` are **the same defect at two altitudes** — bytes that reach `.text` that no size metric
attributes — and `rf-1` is really two call sites, since `ast_search_emit_size` scores the whole
superopt search on the identical local length and traces the data/rodata deltas it then discards.
Do **sweep row 25 before N7**: a wrong non-LVAL `Ref` decode is exactly the fault class N7 proves
the current differential cannot see.

**9. Cluster 2 — the counter substrate, with its coupling corrected.** *Any host for the code.*
N6.8 first, because the figure it underpins is wrong: **`jit.baked` counts attempts, not
acceptances** (`mccjit_embed_stash_leaf` returns early on serialize failure), the stash is
**single-slot** so it counts overwrites, and the path that actually bakes into the binary —
`mccjit_embed_note` — is uncounted. The published 587/1894 and 1550/3163 are the leaf-stash path.
Two hazards before writing it: `MCC_INV_MAX` is **32** and `mcc_inv_add` truncates silently, and
the `[inv]` prefix is already taken by an incompatible grammar in `tools/slicerun.c` that
`fmt-census.py` would raise on. **The real N18↔N19 coupling is the bank key**, which has no host
or arch term — banking emit-map cells here would overwrite the x86_64 numbers.

**~~10. `slice/census`.~~ — DONE.** Bisected on the Mac to `82f39935`, a deliberate priced change
rather than drift. *Investigation. The Mac.* It was a bisect over 102 `src/`
commits on the `arm64`/`Darwin` column. **New datum from x86_64-linux, 2026-08-13: the
`x86_64`/`Linux` column is green in 5.70 s** — and it had been *unmeasurable* here, not
passing, because the cell hung indefinitely in the Vulkan device probe until N33's `DISPLAY`
scrub landed. It was found wedged at **90 minutes**. So the two columns are independent and
the arm64 red really is column drift; whoever takes the bisect can use this column as the
control.

**Newly found, unfiled, and each cheap enough to take on sight:**
- **Metal's `mcc_gpu_mem()` succeeds and hands back a pointer no kernel can address** — the
  encoder binds only buffers 0 and 1 and both MSL kernels declare two. Its neighbour's failure
  string still says the window does not exist, which is now false. **Fail closed (~20 lines)
  before anyone builds on it**; a silent wrong answer is worse than the absent cell.
- **~~`slice/cref-oracle` runs clang against clang on this host, with no probe.~~ — REFUTED
  2026-08-13. There are two probes, not zero**, and this bullet is a duplicate of the already-struck
  **N28**: `mcc_find_gnu_gcc` runs `--version` and rejects anything not Free-Software-Foundation
  or matching `clang`, and `mcc_compiler_family` (`__clang__` vs `__GNUC__`, added by `c319bdef`)
  is applied to the resolved `DIFF3_GCC`/`DIFF3_CLANG` pair. **The one real residual is that
  `MCC_DIFF3_SAME_FAMILY` is set and never read** — the probe warns, it does not gate, and the
  `slice/cref-oracle*` cells register unconditionally on `DIFF3_GCC AND DIFF3_CLANG`. That item
  was filed under *the single-family fallback* in the shared-surface ranking and is **closed
  2026-08-13** — the cells skip with a reason when the pair is one family; this bullet is deleted
  rather than kept. Testable here by forcing the branch with
  `-DMCC_DIFF3_GCC=$(command -v clang)`.
- **~~`sweep row 23`'s "four empty `nofb_miscompiles` lists" is stale.~~ — VERIFIED and corrected
  2026-08-13, and it was five false sentences, not two.** `tests/rir/coverage-bank.json` holds
  **three empty lists and one entry** — `O0: ["src/mcc.c::cleanup_symbols"]`, banked by `5d75acd8`
  because a stage-1 shipping its replay bytes segfaults. Every claim of "four empty" or "banks
  zero miscompiles" in this file is now marked at its site.
- **~~`res-d4b` and its two siblings are already fixed~~ — VERIFIED 2026-08-13, blocks struck.**
  All three of *Still open — three unguarded `sym` dereferences*: the `unary()` `'&'` arm by
  `3fe04727` (with `tests/diagnostics/dg-error/address_of_comparison.c` pinning it), both
  `check_va_start_*` arms by `7a9a8e9b`/`0e9ac0fd`, and the builtin-fold ISA scan — `res-d4b`
  itself — by `0cfe71a7`. The block's own preferred *root-cause* fix (make `vset_VT_CMP` write the
  whole union) was **not** taken and still is not; the three reader-side guards were the intended
  resolution and the block says so. The `rir_decayed_array` residue is fixed by `81a81f1b`
  (`rir_decayed_array` decides both `r` shapes before touching `sym`; `wide256_settle` is gone
  from `src/`). **`res-d4b` and `int128-signedness` are dangling identifiers** — introduced as
  bare IDs by `f551680a` and defined nowhere — so the mentions are being removed rather than
  cross-referenced.
- **~~`int128-signedness` is now dead-code cleanup only.~~ — VERIFIED 2026-08-13.** `__mcc_ov_disp`
  has zero call sites anywhere in `src/`, `runtime/`, `tests/` or `tools/`;
  `__builtin_{add,sub,mul}_overflow` expand to `__mcc_ov_gen`, whose inline
  `__mcc_ov_calc_w`/`__mcc_ov_calc` path handles `__int128` through `__mcc_ov_is_wide` and never
  reaches the out-of-line `__mcc_addo_*` helpers. The cleanup deletes `__MCC_OV_DECL`/`_W` and
  their 14 instantiations plus `__mcc_ov_disp{,_ti}` from `runtime/include/mccdefs.h`, the
  `MCC_OV_SMALL_S/U` and `BIG_S/U` macros and their 14 instantiations from `runtime/lib/builtin.c`,
  and `__mcc_{add,sub,mul}o_{ti,uti}_impl` plus `MCC_OV_WRAP` and `mcc_ov_{from,to}_abi` from
  `runtime/lib/int128.c`. ~~**Not deleted here**: these are exported runtime symbols, and nothing
  in-tree links them, but an out-of-tree consumer or an older `mccdefs.h` would. It is a decision,
  not a sweep.~~ **All three files are now done — the decision was taken 2026-08-13 rather than
  deferred again**, and the row above states the argument. One correction to the list as written:
  `__mcc_{add,sub,mul}o_ti_impl` are **not** deleted from `runtime/lib/int128.c`, because
  `__addvti3`/`__subvti3`/`__mulvti3` call them; only the `_uti_impl` three, which nothing but the
  wrappers reached, go with the wrappers.

**Belongs to the Linux box, do not start here:** ~~N8's two unreduced survivors~~ (**closed
2026-08-13 on that box — see N8**), items 23 and 24
(`gen_cvt_ftoi` — 24 is the real conformance defect and needs an emitted x87 `fistpl`, not a
wider convert), `bl-7`'s re-bank, `rf-4/5` and `ci-4` (both need two `mcc` builds on one ELF
host), `run-tier/x86_64`'s `tls_threads`, every `diff3/*` cell, and `d6-sso-msabi`. **A single
gcc checkout on that box unblocks N8 and `jit-offx86-unmeasured` together** — the highest-leverage
host change available anywhere on this board. *(2026-08-13: the checkout is there, at
`~/Projects/gcc`, and it is what closed N8. `llvm-test-suite` is still absent, so
`jit/xoracle-coverage` cannot reach `--min-cross 400` on one suite. **A second prerequisite the
board never named**: `vendor/gcc-c-torture-execute` must be symlinked at that checkout or
`optlevel/torture-differential` silently skips.)*

**Only this host can do it:** ~~N24~~ (closed). ~~And the Metal region differential.~~
**MEASURED AND LARGELY WRONG, 2026-08-13 — the region differential already runs on this host and
has since 2026-08-07.** Three corrections, in the order they matter.

*(a) The MoltenVK step is not "half-built", it landed.* `2eee6c41` (2026-08-08, 14 lines of
`CMakeLists.txt`) added the `find_library(MCC_MOLTENVK_LIB MoltenVK)` fallback, and
`ARCHIVED.md` has recorded since then that this row is stale. What made the row *look* live is a
second false premise: it says the fallback "never fires" because no macOS job passes
`-DVulkan_INCLUDE_DIR`. But `find_package(Vulkan QUIET)` sets `Vulkan_INCLUDE_DIR` **even when it
fails** — `FOUND=FALSE INC=/opt/homebrew/include LIB=NOTFOUND` on this box — so with
`vulkan-headers` installed the fallback fires with no `-D` flags at all.

*(b) So the SPIR-V arm, regions included, is green here.* Twelve `gpu/spv-*` and `gpu/msl-*`
cells in **74.8 s**, and the one the region item is actually about — `gpu/spv-mem-binding`,
which is the only cell that reaches binding 2 — reports `mem lanes=64 through binding 2, 0 bad`
on an Apple M1 Pro. `a918f003` already closed the M7 defect this file still records (two
descriptors, not three): `gpu_run` builds and writes three.

*(c) What was really missing was in CI, and it is failure mode 1 of `must-run.txt` verbatim.*
`brew deps molten-vk` is empty — the formula ships no Vulkan headers — and neither macOS
workflow installed any, so on a GitHub runner `Vulkan_INCLUDE_DIR` is NOTFOUND, the fallback
cannot fire, and all seven `gpu/spv-*` names silently become stubs. **Fixed 2026-08-13**: both
workflows install `vulkan-headers` and assert the header exists, and `CMakeLists.txt` finds its
own headers on Darwin when `find_package` leaves the variable empty.

**What is genuinely left is the native MSL region arm, unchanged and unstarted** — `MslRegion`,
an MSL `mem_case()` (still the literal `"the binding-2 case has no Metal arm"` stub at
`tools/spvgate.c`), a third `MTLBuffer` at index 2, and a `gpu/msl-mem-binding` pair. M4 + M7,
550–850 lines. No stub name is reserved for it, so `tools/regstub-lint.py` enforces nothing
there. **It is no longer the only way to get regions on a Mac, which is the whole point of the
sequencing advice** — that advice was right and has already been taken.

**Not code, decide first:** item 22 (`_Float16` evaluation format — and note the premise needs
fixing: mcc does **not** predefine `__FLT_EVAL_METHOD__`, its headers define it, so the "document
it" arm is written against `runtime/include/{mccdefs,float.h}`. The "no spare `SValue` bit" claim
is true at bit level — `r` sums to exactly `0xFFFF` and `CType.t` uses all 32 — but a new *field*
fits in existing padding; what makes it costly is that `SValue` is bulk-`memcpy`'d as opaque
state in eight places, so a new field joins the RIR replay record), sweep row 29 (the
`MCC_OPT_REPLAY_FALLBACK` flip — **make the fallback visible under either answer**, and re-check
its evidence per the correction above), and the coroutine task S7b.

### The whole-suite state on arm64/macOS — measured 2026-08-12

A full `ctest -j4` over `cmake-macos` reached **8975 of 9548** before it was stopped for the
machine. What it establishes, and what it does not:

- **`runtime-bench-check`, `superopt/global-reload` and `-known-positive` all pass**, and
  `cross/shadow-iv-x86_64` now **skips with a reason** instead of reporting a vacuous sweep —
  the four closures verified in place rather than by assertion.
- **`slice/census` is the one real red**, and it is a drifted column, not a defect. See the
  arm64 standing-reds table. *(Closed 2026-08-13 — bisected to `82f39935` and re-banked at
  `ac69a0fd`; green here in 1.33 s. **But "the one real red" did not survive 2026-08-13**:
  `ci/must-run-registered` is red on this host too and no table in this file listed it. See the
  standing-reds table.)*
- **13 timeouts, all `flagsweep-exec`, all contention** — that is **N26**, and until it is fixed
  a full run on this host cannot distinguish a regression from a scheduling artefact.
- **`jit/bind-local` (N24), `flagsweep/cover3-verify`, `ci/registration-stubs` and
  `fmt/census-bank` were not reached.** The first is a known red; the other three were fixed
  earlier in the same session and are verified only by targeted runs. **A clean full-suite number
  for this host does not yet exist** — do not quote one.

### How to validate — standing rule, 2026-08-10

**Validate new code with the smoke/fast tests only, using gcc-15 and clang-22 as the
oracles.** `ctest -R "^smoke/"` is ~15–90 s for 13.0M value cases across `-O0`–`-O4`, plus
the device arm and the divergence arm. Do not run the full suite to validate a change.

> **2026-08-13: `ctest -R "^smoke/"` is 12 of 12 green on x86_64-linux for the first time, in
> 115.15 s wall in `cmake-def` — and it took three fixes to get there.**
> `smoke/engines-identity` 80.0 s, `smoke/strat-dark` 47.1 s, `smoke/engines-known-positive`
> 30.2 s, `smoke/engines` 28.6 s, `smoke/device` 12.9 s, `smoke/slice-bails` 12.9 s,
> `device-known-positive` 7.5 s, everything else under 5 s. **Do not quote a contended number**:
> the same twelve cells run alongside the `slice/*` family time out at 900 s, because both
> families queue on one device.
>
> **The three fixes, in the order they were needed.** It arrived at **7 of 12 green and 5 unrunnable**: `smoke/divergence` was a stale
> bank (**N32**, now `0 worse, 0 better, 0 new` over 1782 rows), and the other five were one
> defect — **N34**, an exit-time segfault in `mcc_gpu_quiesce()` under
> `MCC_AST_EVAL_LADDER_GPU=1`, caused by `atexit` ordering against the NVIDIA ICD's own handler.
> Both are closed. **The third is N33's**: the five device cells hang on an unresponsive X server
> because the Vulkan ICD opens a display inside `vkCreateInstance`, and N34's fix alone does not
> reach that — every cell now runs with `DISPLAY` and `WAYLAND_DISPLAY` scrubbed, floored at 9000
> so the scrub cannot silently shrink, and only the `wine` cells are exempt.
>
> **The trap this leaves behind is worth more than the timings.** Five unrunnable cells and five
> passing ones read the same in a `-R "^smoke/"` summary skimmed for the pass count, and that is
> how a hung device arm survived here. See also **N33**: eighteen `slice/*` cells hang on an
> unresponsive X server because the Vulkan ICD opens a display inside instance creation, and the
> cell reports it as an empty funnel. Those cells now run with `DISPLAY` scrubbed.
>
> **This rule was only executable on one of the three machines until 2026-08-12.** On the Mac
> the suite was 12 of 12 red and had been since it existed there — see the wave note above.
> It is now **12 of 12 green on arm64/macOS**, against a target-keyed bank
> (`tests/smoke/bails-arm64-macos.txt`; x86_64-linux and Windows still read `bails.txt`
> unchanged). Two standing reds on that host are outside smoke and predate the wave:
> `flagsweep/cover3-verify`, which wants `tests/optfire/cover3.py gen` — **done 2026-08-12, see
> the standing-reds table** — and `jit/bind-local`, which is **N24**.
>
> **Re-measured 2026-08-12: it is 200 s at `-j2`, not 108 s, and `smoke/strat-dark` is 193 s of
> that on its own.** The 108 s figure is stale rather than wrong-at-the-time: `strat-dark` sweeps
> the strategy registry, and strategies 23 (`sra`) and 24 (`sroa`) landed after it was taken.
> **The cost is not the Darwin pin** — with the pin reverted the same cell measures 194.2 / 187.4
> / 187.4 s over three runs, so `taskpolicy -b` is free here to within noise and buys the tail
> removal for nothing. It does mean this file's own warning now binds: `strat-dark` is 96% of the
> smoke gate on this host, and the rule says drop that cell rather than the rule if it stops
> being affordable. **Nobody has run this rule on the Windows box**; assume the same class of
> breakage there until someone does.

**As of the 2026-08-11 board-work wave it is twelve cells and ~117 s in `cmake-def` on this
host** — `smoke/strat-dark` is 44 s of that on its own, which is the honest price of watching
`-O13` and is called out where the cell is registered. It nearly doubled the suite; if it ever
stops being affordable, drop that cell rather than the rule, because it is the only one whose
subject is a level the sweep cannot reach. The eleven-cell figure below still describes
everything else:

**As of 2026-08-11 it was eleven cells, not eight, and ~71 s in `cmake-def` on this host.**
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

`main` at `a43b422a`, **9548 cells in `cmake-def` on this host** — unchanged by the
emit-coverage instrument, which adds counters rather than cells (`jit/selfhost-opt` and
`jit/gdb-debuggable` are the two new ones) — counted 2026-08-11 with
`ctest -N`, not added up. It was 9538 at `747709bc` and 9545 before `smoke/strat-dark`.

> **2026-08-13, end of day: 9930 on x86_64-linux.** It was 9561 at `52e7e850` and 9565 after this
> file's first wave of the day; the rest arrived with the Windows/wine tiers from the other host.
> The count is host- and prerequisite-dependent — see *where this host actually stands*, above.
> **9932 after the instrument wave**: `ast/inv-faithful` and its known-positive, which are the
> first cells on this board to watch N18 at all and cost 1.7 s each because they read counters
> rather than a trace.
>
> **2026-08-13, x86_64-linux: 9561 at `52e7e850`, 9565 after this wave.** The +4 is
> `stratsweep/iso-{sra,sroa}` and their `isofull` skip stubs, which appeared the moment
> `_ss_rows` stopped being a hand-written mirror. **The count is host- and prerequisite-dependent
> and always has been** — on this host `optlevel/torture-differential` and its `-known-positive`
> registered as real cells only after `vendor/gcc-c-torture-execute` was symlinked at
> `~/Projects/gcc`, and `jit/xoracle-coverage` still cannot reach `--min-cross 400` because
> `llvm-test-suite` is absent. Count on the host you are standing on, and say what it had.

> **2026-08-12, and the number is a coincidence worth not misreading.** The arm64/macOS host
> counted **9545** before this wave and **9548** after, which is the same total as `cmake-def`'s
> and is not the same set. The three added are the cells `ci/registration-stubs` was flagging as
> dropped rather than skipped (`jit/gdb-debuggable`, `jit/selfhost-opt`, `jit/xoracle-coverage`);
> they now register as skips with a reason, which is the whole point of that lint — the total
> stops depending on what the host happens to have. **Count on the host you are standing on.**

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
`gen_cast`, `preprocess`). 95% of the divergence is *length* divergence. *(This is `-O2` on
`src/mcc.c`. The independent 2026-08-11 count on gcc torture at `-O1` is 0.47% of bodies —
a different corpus and a different level, so it neither confirms nor refutes this; see the
emit-coverage section. **The emit map then measured `src/mcc.c` directly at `-O1`: 70 of 3165,
2.21%, against the 2.17% here — so this figure is confirmed by a second instrument at a second
level, and the gap to torture is a corpus difference, not a measurement error. See N18.**)*

`docs/ARCHIVED.md` already stated the rule — *"any pass that changes code must be an arena
rewrite in the post-fidelity strategy phase"* — and both normalisations were placed at the
end of `rir_to_arena`, upstream of the fidelity check. `rir_arena_normalise` now runs from
`ast_func_end` after `ast_fn_faithful` is computed and before `ast_slc_dump`/`ast_adump_body`.

**Then making that code actually ship turned two `tests/exec` goldens red** —
`wide_bitfield_arith` and `integer_promotion`, which are *exactly* the two bodies the wide
census reported as "discarded for replaying 40 bytes shorter than the parser". They were
shorter **and wrong**. The earlier claim that every discarded body is still correct was
inferred from `exec/` 347/347, but those cells compile at `-O0`, **where nothing re-emits**
*(measured directly 2026-08-11: `-O0` records 0 of 1932 bodies and builds no arena — see the
emit-coverage section, and again on two named targets, 0 of 3165 self-host and 0 of 303 on
`full_language.c`, in the emit map)* —
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
3. ~~The emitter caps (`F7`).~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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
  permanent 77 for the same class of reason. *(The separate `runtime-bench-check`
  red was something else and is closed — see the standing-reds table.)*
- ~~**`tools/shadow-iv-sweep.sh` documents its own blindness in the source**: *"There is
  still no floor on this count, so a regression that stops 500 of 610 subjects building
  would report divergences=0 and PASS"*.~~ **CLOSED 2026-08-12.** There is a floor now:
  `MCC_SHADOW_MIN_CLEAN_PCT`, default 80, compared against `clean * 100 < n * pct`, and the
  failure message says how many subjects `divergences=0` is actually a statement about.
  Measured on arm64: **292 of 308 compile at `-O1`**, so 80 is a real floor with headroom
  and not a number chosen to pass. Teeth proven by running at 99, which fails.
- ~~The `22 of 22` strategy coverage quoted under *How to validate* is measured, not enforced: `tools/smokerun.c`…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
- ~~**`src/wide256_slice.h`'s I-6 is a live compiler segfault with a two-line fix and
  nothing watching it**: `src/mccrir.c` still tests only `if (!sv->sym …)`,
  with no `sv->r != VT_CMP` check, and `gv` still leaves `sym` stale.~~ — **FALSE since
  `81a81f1b`, corrected 2026-08-13.** `rir_decayed_array` decides both `r` shapes before touching
  `sym`, and `tests/exec/types/int256.c`'s `test_replay_cmp` is the thing watching it. The
  *second* clause is still true and is the part worth keeping: `gv` does leave `sym` stale, so the
  hazard survives for any future reader that does not test `r` first. The prediction in the last
  sentence held exactly — it was filed at the bottom of the 256-bit section and it was missed,
  for four days after it stopped being true.

### Open, ranked

> **Status after the 2026-08-11 emit-map wave: N4, N5 and N9 are closed and struck in place;
> N1, N2, N3, N6, N7, N8 are open, N15/N16 came from the JIT/debug wave, and N17/N18/N19 are
> new.** The numbering skips N10–N14 on
> purpose: those tokens are already in use in this file for the archived device cluster, and
> reusing them would make "N13" mean two unrelated things. Numbering continues rather than
> renumbering —
> N1–N6 are the 2026-08-10 additions, N7 came with the engine-parity arm on 2026-08-11, and
> N8–N9 with the JIT/AOT differential the same afternoon. Order here is rank, not number, and
> closed rows stay in place so a reader following a cross-reference lands on the closure
> rather than on nothing.
>
> **N20 was found and fixed on 2026-08-11** — a silent `--embed-jit` miscompile, root-caused to
> `ast_configure()` never running inside the JIT, so every ALWAYS-class replay-correctness flag
> was off during the runtime re-emit. Two cells now guard it. It is struck in place below, and
> it closes the coverage half of N18. The general lesson is worth more than the fix: **a
> correctness gate that only runs on the AOT path is not a correctness gate**, because the JIT
> re-emits the same arenas with no faithfulness comparison to fall back on.
>
> **Ranking as of 2026-08-13, after the instrument wave: N3 (items 22 and 23), N6, N7 and N29 —
> four of thirty-five.** N18's instrument half closed and it leaves the live list; what is left of
> it is a defect nobody has looked at, stated in its own row. **N6.8 closed in full**, and it is the
> row worth reading before filing another counter: the figure it corrected was wrong in the
> direction predicted and by more than estimated, because the counter was at the *call site* of a
> function that can refuse. **The rule it yields:** count where the thing becomes true, not where
> it is attempted — the two differ by 299 bodies on this tree and nobody noticed for as long as the
> counter existed.
>
> **Superseded, for context — after the second pass: N2, N3, N6, N7, N18, N19, N24, N29 and N30
> — nine of thirty-four.** N33 and N34 closed the day they were filed, and N3 lost item 24, so
> the live rows are **N3 (items 23 and 22 only), N6, N7, N18 and N29** — five of
> thirty-five. **N2 closed in full on 2026-08-13** when A6's `nc[]` resync landed, and **N19**
> closed the same day. **N35 is three reds this file had no row for**, all
> pre-existing at `c7df5209` and all found by running the 526-cell
> `jit/ ast/ rir optlevel diff3/ superopt/ fmt/ docs/ ci/` family rather than by sweeping. N8 closed in full on the Linux box (N20 closed all three of its programs,
> and the pre-fix A/B proves it), N26 and N27 and N28 are struck, and **N31** is new — a defect
> the board had no row for and no red for, because the cell that caught it matched none of the
> sweep regexes. **N32** and **N33** are the same story twice more — two cells red at `52e7e850`
> on this host that no table in this file listed, one of them the standing inner-loop gate.
> N7 is still first, and rank 5's split has now given it the instrument it was
> missing; what it is waiting on is one unsandboxed re-run of the injection under the GPU arm.
>
> **The superseded 2026-08-12 line, for context: N2, N3, N6, N7, N8, N18,
> N19, N24 and N26 — nine of twenty-six.** The board's own order is superseded by
> *Next steps, prioritised* above, which merges these rows with the items outside the ranked
> board and states the host constraint on each; what follows here is the per-row detail.** N22 landed as strategy 24 with one part open (the ABI
> gather/scatter); N15, N16, N17, N21, N23, N25 and N1 all closed, and their write-ups moved to
> [`docs/ARCHIVED.md`](ARCHIVED.md). **N7 is now first**: it is the same question N1 answered —
> work a mechanism performs that nothing observes — one level down, on the slice evaluator, and
> N1's closure is the first thing to read before starting it. Then N8, whose two survivors need
> the Linux box. See *Implementation order by shared surface* for the ordering that groups these
> with everything outside the ranked board.
>
> **The superseded ranking, for context: N22, then N7, then N8.** N17 and N1 are closed and
> struck in place; N8 lost its lead to N20 and its two survivors need the Linux box. N22 is
> first because it is a known-cause fix with a written-and-backed-out prototype behind it. N23 was
> opened as a conformance defect and closed the same day as a gcc/clang disagreement mcc
> resolves on purpose; the correction is left in place because the mistake is instructive.
>
> **The previous ranking, for context: N17, then N8, then N22, then N1, then N7.** N22 is placed high
> because it is a known-cause fix with a written-and-backed-out prototype behind it, not an
> investigation: the rewrite worked, the allocator state leaking through discarded scoring runs
> is what broke it, and that is a bounded change.
>
> **The earlier ranking, for context: N17, then N8, then N1, then N7.** N17 goes first because it is
> cheap (one counter, in an arm that is already braced) and because it is a *measurement*
> defect: until it lands, every coverage and faithfulness percentage in this file — including
> the ones the two sections above are built on — is a lower bound by an unreported amount, and
> N18 is unquotable without it. N8 is three wrong answers with a six-line reproducer for one of
> them. N1 and N7 are the same question asked twice — work that a mechanism performs and
> nothing observes — and N17 is a third instance of exactly that shape, now on the instruments
> themselves, which strengthens the argument for treating it as structural rather than as
> separate rows. N2, N3 and N6 are unchanged and unstarted; N19 is small and can wait for
> whoever next needs a non-x86_64 byte number.

**~~N1. Seven of 22 strategies are write-only.~~ — CLOSED 2026-08-12.** `unread` sums `sf[]` for `LTEMP, IVSR, PRE, RANGE, ABS, REASSOC, INLINE` and joins all three spellings of the `do_*` disjunction. Witnessed on the exact case this row named — `reassoc` alone at `-O4 -fno-promote-locals` was byte-identical across `-freassoc-assoc`/`-fno-` before and differs after; same for `-ftree-vrp`. **The other five cannot witness it either way**, because their flags gate more than their strategy. It costs bytes on purpose: self-host moves at every level and grows 224 B at `-O1`, 448 B at `-O2`, 16 B at `-O4`. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

**~~N2. `rir_tvar_replay` and `rir_slot_replay` repeat the bug `dd80e4fa` fixed, unchecked.~~ — CLOSED 2026-08-13 except for `rir_tvrec`'s `nc[]` resync.** All three streams carry `sz`/`al` and share one `rir_rec_take` (A1–A7); the C2 bypass is fixed; and
`rir/rec-miss` pins the failure arm with a forced-miss injection that moves 3 objects over 6 subjects. **Two things this host added that the closure did not have.** *(1)* A5's arm64 slot site is compiled out on Mach-O, so it had never been compiled where it lives; a cross build here compiles it and the force-miss injection proves an HFA `va_arg` subject **reaches** it on `mcc-arm64` and does not on `mcc-x86_64`. Execution is still owed — `qemu-aarch64` is present, the arm64 sysroot is not. *(2)* `rir/rec-miss` could not run at all as landed; see N35.
**N2 is now closed in full.** A6's second half — the missing `rir_tvrec` `nc[]` resync — landed
2026-08-13, so all four record streams gate their cursor advance identically and no caller of
`rir_rec_take` passes `NULL` any more. **It is byte-neutral, which the A6 row predicted it would
not be**: 1,199 object pairs against a clean-HEAD control over the whole `tests/exec` corpus at
all four levels plus the standalone `src/*.c` TUs, 0 moved, and 598 more under
`MCC_RIR_REC_FORCE_MISS=1` on both sides, 0 moved. A latent divergence closed rather than a bug
fixed.

As originally filed:
`rir_tvar_replay` is the *first* statement of `get_temp_local_var`, so it bypasses the
`size >= && align >=` invariant the same function enforces sixteen lines later, on the same
object class, in a function that already has the sizes. Neither was on the board.
`rir_loc_replay` is the same shape but measured clean (0 undersized over 620 smoke firings
and 1,013 torture firings) — that probe was reverted, not banked, so nothing pins it.

**Verified 2026-08-12, and the row is understated in three ways.**
- **The streams do not skip the check; they have nowhere to put the data.** `rir_slotrec[]` and
  `rir_tvrec[]` carry `pos`/`nc` only — no `sz`/`al` array exists on either, so this is a record
  widening and not a predicate. `rir_hook_slot_replay`/`_record` take no arguments and read the
  global `loc`, so their signatures move too. Both `rir_slot_replay` call sites already hold the
  sizes.
- **A second defect, not previously filed: `dd80e4fa`'s fix is bypassed entirely on the C2 path.**
  `ast_alloc_loc` runs `rir_loc_replay` *before* `ast_locrec_take`, so whenever `rir_c2_active`
  the checked path is never reached. The fix this file banked is inert on one arm — the same
  shape as N26's `PIN` and N25's reference pair.
- **`rir_tvar_replay` has the weakest resync of the four**, not merely the missing fit check: the
  other three gate their cursor advance on an `RIR_NOEVAL_MASK`/position test, and `rir_tvrec`
  has no `nc[]` array at all.

**Copy `ast_locrec_take`, not `rir_fcrec[]`** — see the refutation in the re-verified
shared-surface ranking. Smallest member of the family is `rir-locrec-skip-byfit`:
`ast_locrec_skip` is a blind `ast_locrec_i++` whose size is already computed four lines above its
only call site. **Host:** neutral to write; the arm64 slot site is inside
`#if !defined(MCC_TARGET_MACHO)` and so is compiled out here.

**N3. ~~Item 24 is the real conformance defect of the 22/23/24 trio~~ — ITEM 24 CLOSED
2026-08-13; 23 and 22 remain.** Both were `t != VT_INT` in `gen_cvt_ftoi` on opposite arms; 24 was
in-range, not UB, and also hit `(short)`; 23 has no non-UB reproducer on x86-64.
arm64/riscv64 are already correct and are the reference.

**24 is fixed exactly as this row prescribed — the x87 sequence, not a wider convert.** The
`VT_LDOUBLE`/`t == VT_INT` arm was `gen_cvt_ftof(VT_DOUBLE)` followed by a 32-bit `cvttsd2si`, so
an 80-bit value was *rounded* to `double` before being *truncated*: `(int)(1 − 2⁻⁶⁴)` gave **1**
where both references give 0, and the same for `(short)` and `(signed char)`, at every level.
It now emits `fnstcw`/`fnstcw`/`orw $0xc00`/`fldcw`/`fistpl`/`fldcw` against an 8-byte frame slot
— the control word is copied and modified *in memory*, so the sequence needs no scratch GPR
before the final load. **The row's warning was right and is now pinned**: `(int)1e300L`,
`(int)-1e300L` and `(int)NaN` all still give `-2147483648`, the x87 integer indefinite, which a
64-bit-convert-then-narrow would have turned into 0. `tests/exec/types/ldouble_to_signed.c` banks
all nine rows across 23 cells including `diff3`.

**One finding on the way that belongs to Cluster A rather than here.** The first version took its
scratch slot with a raw `loc = (loc - 8) & -8`, which is what the surrounding backend code does in
the prolog paths — and smoke came back with `replay-fallback:len` risen 2 → 4 and a new
`replay-fallback:bytes` category **at every one of the five levels**, with zero value failures.
Routing the same allocation through `ast_alloc_loc(8, 8)` makes all of it disappear. So a frame
allocation made outside the recorded channel is *silently* a replay-fidelity regression, the bail
ratchet is what notices, and that is a concrete instance of the hazard Cluster A is about.

Still open: **23** (quality-of-implementation, no non-UB reproducer) and **22**, which may be no
defect at all — mcc predefines `__FLT_EVAL_METHOD__ 0` and its per-operation
rounding is what that macro promises.

**~~N4. `-O13` is dark on 13 of 22 strategies~~ — CLOSED 2026-08-11** (and it is 12, not 13). Write-up moved to [`docs/ARCHIVED.md`](ARCHIVED.md).

**~~N5. Four green-by-omission hazards.~~ — ALL FOUR CLOSED 2026-08-11.** Write-up moved to [`docs/ARCHIVED.md`](ARCHIVED.md).

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

**Restated 2026-08-12: this row has ONE cause, not two, and the second should be deleted.**

**The cause that is real.** `differ` compares two arenas that are both evaluated by
`ast_eval_binop`, so a shared fault cancels by construction — self-comparison cannot detect it,
and this is the "one engine compared with itself" shape the engine arm exists to refuse, one
level below where the arm looks. The fix is an independent oracle for the tree side.

**The cause that was a category error.** The row claimed the unchanged 532 certifications meant
"certifications are not reaching codegen", and that this was N1's question one level down.
Verification refutes both halves:
- **N1's mechanism cannot transfer, because N7's path has none of its three ingredients.** N1 was
  a *gate* defect: real state was mutated (`ast_cur`), a consumer existed (the re-emit), and a
  disjunction omitted seven terms. `ast_ladder_census` runs *after* the whole replay/re-emit
  block, tallies into two file-static structs whose only reader is an `atexit` dump, and frees
  its arena. There is no gate, no consumer, and no surviving state. Certifications "not reaching
  codegen" is the census's design, not a defect.
- **The evidence was entailed by the first cause and therefore carried no information.** The
  self-pair compares an expression against a verbatim structural copy of itself under the same
  evaluator, and the injection sits after the range early-return so it changes no definedness.
  `av == bv` on every point of every self-pair is then a theorem. **"The 532 did not move" was
  never a measurement.**
- In the configuration described there is also no codegen consumer of `ast_eval_binop` at all:
  `opt-slice`, the only in-emit slice consumer, is dev-gated at level 9, and the real consumers
  are on JIT bake paths a plain `-c` compile never enters. Essentially all 66,436,580 hits are
  the instrument measuring itself.

**~~Cheapest real progress, and it needs no new oracle~~ — TAKEN 2026-08-13, and it worked.** The
GPU arm now catches the `r = s + 1` injection: `differing-files=0` clean, `4` injected, and
`[ladder-cross] points` moves **by exactly 194**, this row's own figure. **But only `points` moves**
— every verdict field is byte-identical, so the teeth are a coverage-index artefact rather than an
oracle. What closed is "the arm is blind"; the independent tree-side oracle is still the work. See
*Cross-cutting research* for the run and the timing correction.

**The original text:** the GPU arm already *is* an independent
evaluator — with `MCC_AST_EVAL_LADDER_GPU=1` the hook short-circuits the rung and
`ast_eval_binop` is never called for those points — and `gpu/ladder-gpu-parity` diffs the two
censuses textually. It missed the injection because it drops the one line that moved:
`ladder_gpu_parity.cmake` filters `secs=` to suppress timing noise, and `points` is printed on
that line. **Split the points/secs line, or filter the field rather than the row (~10 lines), then
re-run the injection under the GPU arm.** Residual CPU-only surface no oracle covers even then:
the `n == 0` const case, the corner sweep, the observed rung, and every rung the GPU refuses.

**~~N8. Two unreduced JIT-only miscompiles remain; the lead is closed.~~ — CLOSED IN FULL
2026-08-13 on the Linux box. `2aa3e599` (N20) closed all three, not one.** The lead
`gcc.dg/torture/pr45830.c` was already struck. The two survivors — `gcc.dg/pr96674.c` and
`gcc.dg/fastmath-1.c` — pass the JIT/AOT differential at `-O0`–`-O4` × {eager, lazy} ×
{`--embed-jit`, `-run`} × {the harness's dg flags, `-fwrapv` alone, no flags, real
`-ffast-math`}. **This is a fix and not a broken harness**: the compiler was rebuilt at
`2aa3e599^` and both reproduce there (`rc=134` under the JIT against `rc=0` AOT, every level),
and stop at `2aa3e599`. Root cause is N20's: `ast_configure()` is called from `mccgen_compile()`,
which the JIT's runtime path never enters, so `replay-cmp-materialize` was off during the re-emit
and **any binary operator with two pending `VT_CMP` operands** was miscompiled. Both programs are
that shape — `(b == 0) | (a < b)` and `(d[0] > 0) == (d[1] > 0)`.

**The coverage gap this closed on, now filled.** `tests/embed/jit_replay_parity.sh` enumerates
`& | ^ + *` of two `int` compares against **one** parameter, and covers neither `==` as the
combining operator, nor two *different* parameters, nor `unsigned` operands, nor *float* compare
operands. `tests/jit/parity/relop_eq_pair.c` adds all four; `jit/run-parity-host` and
`jit/kgc-route-parity` walk the directory, so it needs no registration. The float form must go
through a pointer — the scalar `int f(float x, float y){ return (x>0)==(y>0); }` did **not**
reproduce even pre-N20.

**Harness note kept because the row's own label was misleading:** `-ffast-math` is not in
`xsuite.KEEP_OPT_RE`, so `gcc.dg/fastmath-1.c` has never been run with the flag it exists to
test. The oracle loses it too, so the comparison stays valid — but do not read "`-ffast-math`" in
a jitconform row as meaning the flag was passed.

Behind them, still unexamined: ~80 `differ` and ~150 `refused` rows per level, where `refused` is a front-end gap list and not a JIT one.

**~~N9. `-fno-opt-search-<anything>` disables the whole search, not the sub-knob it names.~~ — CLOSED 2026-08-11.** Write-up moved to [`docs/ARCHIVED.md`](ARCHIVED.md).

**~~N15. `-g -run` SIGSEGVs the moment the JIT is allowed to bake.~~ — CLOSED 2026-08-12.** One misordered line in `mcc_tcov_block_begin`, which read `s1->dState->tcov_data` above its own `test_coverage == 0` early-out; harmless where `dState` exists, fatal in the JIT's re-emit where it does not. `MCC_OUTPUT_MEMORY` is now in the bake gate, checked by differential over all 310 `tests/exec` programs at `-O1 -g -run --jit`. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

**~~N16. `85bf6a3d` moves `-O0` objects, and its commit message says it does not.~~ — SETTLED 2026-08-12 on arm64-osx: it does not.** A/B'd through `tools/o0_ab.sh`: 294 objects, zero rows different. The original report is most likely the `__TIME__` landmine now recorded under **N21**. Found on the way and fixed: `o0_ab.sh` dropped its own native key on any host that is not x86_64-Linux. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

**~~N17. An aborted replay increments nothing, so every `ast.*` number in this file is a lower bound.~~ — CLOSED 2026-08-12.** Three counters, not one: `ast.abort`, `ast.abort_post` and `ast.noreplay`. `rir.rec == ast.body + ast.abort + ast.noreplay` now holds with no residual on `full_language.c` at `-O1`/`-O2`/`-O3`/`-O4` and at `-O0 --embed-jit` (276 = 271 + 2 + 3) and on self-host at `-O1` and `-O4` (3141 = 3140 + 0 + 1), so the recorded-but-not-verdicted gap closes. **Coverage and faithfulness percentages in this file are quotable again**, and `ast.abort_post` names an event that had none: a body that takes the verdict and *then* longjmps out of the optimizer strategy phase. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

**N18 — the title is now false, and the row stays open for the half it was never about.**
Something watches the arm: `ast/inv-faithful` (+ known-positive), `must-run`, 1.7 s, two `MCC_INV=1`
compiles and a subtraction, banked in `tests/emitmap/faithful.json`. It reproduces this row's table
on this host at **+1.36** (`full_language.c`) and **+1.19** (self-host) against the +1.36/+1.20
recorded below, and it needs neither `MCC_CONFIG_TRACE=ON` nor an opt-in, so the "do not bank from
this host" caution does not apply to it — the key carries the arch and any host can re-take it.
**Also corrected from this row: its bake figures are the wrong counter.** 1550 of 3163 and 41 of
299 are `jit.baked`, i.e. leaf-stash attempts; the bodies that actually reach `mccjit_embed_fns`
are **1278 of 3202 (39.91%)** and **39 of 299 (13.04%)** — see N6.8. **What is open is the defect:**
why the `--embed-jit` re-emit is 1.2–1.9 points less faithful than `-O1` on every target and both
architectures measured. Nobody has looked at a single one of the unfaithful bodies.

As originally filed:
**N18. Nothing watches the `-O0 --embed-jit` arm, and it is the least faithful one on both
targets.** New 2026-08-11. First, the part that is *confirmation, not news*: this file's standing
rule — quote 2.0% for the self-host hot path and 0.47% for torture, never one for the other — is
now backed by a third instrument and a second level. The emit map measures **70 unfaithful of
3165 verdicted on `src/mcc.c` at `-O1` — 2.21%**, against the 2.17% previously measured at `-O2`
on the same source. Two levels, two instruments, 0.04 points apart: the self-host figure is real
and the corpus/self-host split is not an artefact.

The open part is the fourth row:

| target | arm | verdicted | unfaithful |
| --- | --- | ---: | ---: |
| self-host | `-O1` | 3165 | 70 — 2.21% |
| `full_language.c` | `-O1` | 301 | 10 — 3.32% |
| self-host | `-O0 --embed-jit` | 3163 | **108 — 3.41%** |
| `full_language.c` | `-O0 --embed-jit` | 299 | **14 — 4.68%** |

**`-O0 --embed-jit` is less faithful than `-O1` on both targets** — +1.20 and +1.36 points, the
same direction `EMIT-COVERAGE.md` result 4 saw as +12 bytes and a 1885 → 1883 faithful drop, now
reproduced at 10× the sample on named inputs. **The coverage half of this row is closed by
`mcctest-embedjit` (see N20), which runs the `full_language.c` differential with `--embed-jit`;
what remains open is the faithfulness gap itself.** It matters more than it did: since `3e0f1e8d` made
`--embed-jit` the only gate on baking, this is the arm a `-g --embed-jit` build takes, and it
bakes **1550 of 3163** bodies self-host — so its unfaithful bodies are the ones silently excluded
from dispatch in a debug configuration. (`-run` is deliberately *not* in this arm; that exclusion
is **N15**.) No cell watches it: `tests/ast/rir_parity.cmake` compiles with `-w ${OPT} -c` and
never passes `--embed-jit`, so the banked `ast/rir-parity` cells cover `-O0`–`-O3` on the
non-baking path only. Interacts with **N17** — every row here is a lower bound.

**N18 reproduces on arm64/macOS, on a second object format, with `MCC_INV` alone** — measured
2026-08-12, no trace build needed:

| target | arm | verdicted | unfaithful | |
| --- | --- | ---: | ---: | --- |
| `full_language.c` | `-O1` | 271 | 6 — 2.21% | |
| `full_language.c` | `-O0 --embed-jit` | 271 | 11 — **4.06%** | +1.85 pt |
| self-host | `-O1` | 2959 | 61 — 2.06% | |
| self-host | `-O0 --embed-jit` | 2959 | 99 — **3.35%** | +1.29 pt |

Same direction and magnitude as the x86_64 table (+1.36 / +1.20), so the finding is
architecture-independent. Bake rate here is 46/271 (17%) and 1436/2959 (48.5%) against 41/299
(14%) and 1550/3163 (49%) on x86_64 — the input-dependence claim now has an arch dimension too.

**The coverage half needs no tool change**: the bank key is already
`"%s|%s|%s" % (target, opt, "jit" if embed_jit else "nojit")`, so two `add_test` blocks mirroring
the existing `-O1` pair with `--embed-jit` plus `--update-bank` closes it. **But do not bank it
from this host until the key carries one**: `tools/emit-map.py` has **no host or arch term
anywhere**, while `tools/rir-coverage.py` keys its floors by `MCC_HOST_*` for exactly this
reason, and `emit_amplification` — an x86-only-meaningful quantity — is in `BANK_KEYS`. Banking
here would overwrite the x86_64 numbers. **That is the real N18↔N19 coupling**; the
`emit-map.py`-refactor coupling this file asserted between N19 and N6.8 does not exist.

**Also verified: N17's closure does not fully reproduce here.** `rir.rec == ast.body + ast.abort
+ ast.noreplay` holds exactly on `full_language.c` (276 = 271 + 2 + 3) at both `-O1` and
`-O0 --embed-jit`, but self-host is **off by one** (2961 ≠ 2959 + 0 + 1) on both arms — one body
that entered `rir_hook_body_begin` and reached none of the three exits, which is N17's own shape
with one instance surviving. At 1/2961 = 0.03% it is two orders below N18's 1.2-point effect, so
N18's conclusion stands; the unqualified *"no residual"* does not. **Cheapest item in this
cluster:** `emit-map.py` still derives its dropout numbers from trace anchors and never reads
the now-authoritative `ast.abort`/`ast.abort_post`/`ast.noreplay` keys, despite its docstring
claiming `MCC_INV=1` supplies the authoritative totals — adding them plus a cross-check is a few
lines and turns that residual into a watched quantity.

**~~N19. The byte census exists only for x86_64.~~ — the hazard is CLOSED 2026-08-13; the limitation stands and is now enforced.** Small, and a limitation rather than a defect.
The amplification in the section above works because `g()` is the single byte primitive on
x86_64 — `o()`, `gen_le16/32/64` and every encoder bottom out in it. On arm, arm64 and riscv64
each `o()` writes `cur_text_section->data` directly and **bypasses `g()` entirely**, and the
arm/arm64 assemblers add a second independent cursor writer each; PLT/GOT/veneer/JIT-stub bytes
reach the section through `section_ptr_add` on every target. So a byte census on any non-x86_64
arch needs a different primitive set, and no arch other than x86_64 currently has one.

**Verified 2026-08-12, with two corrections and one new hazard.** The primitive structure
described is exact — `o()` on x86_64 and i386 is `while (c) { g(c); c >>= 8; }` and their
`gen_le32/64` are 4/8 `g()` calls, while arm64/riscv64 `o()` does a direct `write32le` into
`cur_text_section->data` and arm does four direct byte stores, with a second independent cursor
writer in each of the arm and arm64 assemblers. **Correction: "x86_64-only" describes the
measurement taken, not the reachable set — i386 shares the same single primitive** and would
work today. **New hazard: there is no arch guard, so the tool does not skip on arm64** — `g()` is
still reached through `mccgen.c`'s shared paths, so it would print and bank a small nonzero
`emit_amplification` under a host-less key. ~~**A silently-wrong green, not a red**, and it
compounds with the missing host term in the bank key described under N18. The honest interim is
one line (detect a non-x86 target and `return 77`)~~ — **both taken 2026-08-13.**
`emit-map.py` now probes the *target under test* rather than the host (`-dM -E` on `/dev/null`,
matching `__x86_64__`/`__i386__`/`__aarch64__`/`__arm__`/`__riscv`) and returns 77 with the reason
on anything else. Verified on real cross compilers, not simulated: `mcc-arm64` and `mcc-riscv64`
each skip with the arch named, and `mcc-i386` is correctly admitted — which is this row's own
correction, that i386 shares the single primitive and would work today.

**And the bank key carries the arch**, which closes the coupling N18 called *"the real N18↔N19
coupling"*: `bkey` went `target|opt|jit` to `arch|target|opt|jit`, and the two existing rows were
migrated to `x86_64|…` rather than orphaned, because that is the host they were taken on. Banking
an emit-map cell from another machine can no longer overwrite the x86_64 numbers. **The real fix
is still open**: arch anchors plus a per-tag byte weight, since `g_bytes_written` hardcodes one
byte per event. The tool is live, not bit-rotted: all five anchors still resolve against the
current tree.

**~~N22. SRA's real optimization is the separate-slot variant.~~ — LANDED 2026-08-12 as strategy 24.** `ast_sroa_run`, `-ftree-sroa` and `-ftree-sroa-params`, off by default. It keys on the frame range rather than the node shape, because the front end folds most member accesses to a raw `Ref` at `base+k` before the arena exists — which is why the in-place form was both byte-neutral *and* rare. Fires on 9 of 280 exec files against `sra`'s 3, and self-hosts at `-O4`, which the backed-out prototype could not. The allocator constraints this row named are both discharged. **Still open: the ABI-temp gather/scatter for whole-struct uses** — 145 refusals, attributed by `MCC_SROA_WHY`. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

**~~N23. Wide bit-field arithmetic is truncated at every operation.~~ — NOT A DEFECT, corrected 2026-08-12 the same day it was filed.** It is a gcc/clang disagreement and mcc follows gcc deliberately (`b3c660f1`, four gcc c-torture tests). The evidence was already in the x86_64 bank as `diverge-one` and had not been read. **The durable finding is the shape of the mistake**: on a single-reference host, *"mcc is wrong"* and *"the references disagree and mcc picked one"* are the same observation. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

**N31. `rir-nofb-probe-self` is flaky, and the mechanism is not known.** New 2026-08-12. Observed
**fail, fail, pass, pass on identical code**: it failed inside a 212-cell `-j4` sweep, failed
again running alone under `ctest` (786 s), then passed run directly (exit 0) and passed again
under `ctest` (746 s). The runs that failed were both taken while this machine was saturated by
other jobs of mine; the two that passed were quieter. **That is correlation, not a mechanism**,
and no failing output was captured, so what it actually reported is unknown.

**What it is not: `__TIME__`.** N21's standing trap was the obvious suspect — the probe compares
stage-2 object bytes and neither `tools/rir-coverage.py` nor its registration sets
`SOURCE_DATE_EPOCH`, and `mcc` does honour that variable. But **nothing under `src/` uses
`__DATE__` or `__TIME__`** — `src/mcctok.h` defines the tokens and `src/mccpp.c` implements them,
and there is no user — so compiling `src/mcc.c` embeds no timestamp and the trap does not apply
here. Recorded because it is a convincing dead end that costs an hour to walk down twice.

**What it exonerates.** The probe was suspected of catching a real fidelity regression from the
slot/tvar record widening. It does not: with that change in the tree it reports **0 MISCOMPILE at
every level** — 103 divergent bodies at `-O0` (101 benign, 2 vacuous), 64 at `-O1`, 62 at `-O2`
and `-O3`, all benign — on two separate clean runs.

**One constant worth separating from the flake:** at `-O0` the control *"DOES NOT reproduce a
plain build (object differs, 0 of 27 workload items differ)"*, while `-O1`/`-O2`/`-O3` all
reproduce. That line is present in the **passing** runs too, so it is a standing property of the
`-O0` arm and not the failure. An object that differs while every workload item agrees is its own
small open question.

**Before treating this cell as evidence either way, capture a failing run's output** — every
observation so far is a bare pass/fail from `ctest`, which is exactly the shape this file warns
about elsewhere.

**~~N26. `flagsweep-exec` self-contends under `-j`, so which of its 140 cells pass is a function of machine speed.~~ — CLOSED 2026-08-12, and the contention diagnosis was wrong.** A cell times out *alone*, so `-j` cannot be the cause. `atomic_counter` run 24× on an idle machine is **bimodal** — 18 runs at 0.45–2.83 s and 6 (25%) at 51–121 s — and each cell drew from it 12 times. The mechanism is that `PIN` is `taskset`, which macOS lacks, so the pin that exists to stop exactly this was a silent no-op on Darwin. Fixed with `taskpolicy -b`; `stratsweep.sh` had the identical gap. **No `PROCESSORS` property was added.** Validated 119/119 green at `-j4`, zero timeouts, slowest cell 91.5 s. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

**~~N27. Metal's `mcc_gpu_mem()` succeeds and returns a pointer no kernel can address.~~ —
OVERSTATED AS FILED, corrected 2026-08-12.** The three facts are right: Metal's
`mcc_gpu_mem_backend` returns a real shared window, the encoder binds **only** buffers 0 and 1,
and both MSL kernel signatures declare exactly two. **The prescription was wrong.** Failing the
function closed was tried and it turns `slice/mem` red, because that suite asserts the capability
exists — and its own comment says the omission is deliberate: *"Deliberately NOT gated on
`backend_has_regions()`: this suite tests the host mapping only ... No kernel addresses it, so it
is live on a backend whose emitter has no region layer yet."* So `mcc_gpu_mem`'s contract is *is
there a host-visible shared window*, not *can a kernel read it*, and returning 1 on Metal is
correct under it. Reverted.

**What was genuinely false is the neighbouring reason string, and that is fixed.**
`mcc_gpu_host_import_align_backend` claimed *"the Metal arm has no shared window to import into
(`mcc_gpu_mem_backend` returns 0 there)"* — untrue since that function returns 1. It now names the
real obstruction: there is no binding for an imported region to arrive on. **The lesson is one
this file keeps relearning** — a test's stated rationale is evidence about intent, and a "silent
wrong answer" reading that contradicts it is usually the reading that is wrong.

**~~N28. `slice/cref-oracle` adjudicates clang with clang on this host.~~ — WRONG AS FILED,
corrected 2026-08-12 the same day, and the correction is much larger than the row.**

The row asserted "no `gcc-1x` on this machine". **False. Homebrew GCC 16.1.0 is installed at
`/opt/homebrew/bin/gcc-16`** (`__GNUC__ 16`, no `__clang__`). `mcc_find_gnu_gcc` finds it and
`DIFF3_GCC` resolves to it, so `slice/cref-oracle` and the `diff3/*` family have had a genuine
cross oracle here all along. The stale `DIFF3_GCC:FILEPATH=/usr/bin/gcc` in `cmake-macos`'s cache
is a leftover, shadowed by the normal variable — which is exactly what the comment above that
`find_program` says it is there to prevent.

**A compiler-family probe was added anyway** (`mcc_compiler_family`, `__clang__` vs `__GNUC__`),
because the fallback to `find_program(NAMES gcc cc)` is still live for hosts without a real gcc,
and a same-family pair is now named at configure time instead of being silently registered as a
cross oracle.

**~~N29. The smoke oracle resolution misses the real gcc.~~ — CLOSED 2026-08-13, both halves, and
the triage found two things the row did not predict.** The fix is the one line the row named:
`CMakeLists.txt` hands the smoke arm the already-resolved `DIFF3_GCC` instead of a version
ladder, so the oracle is Homebrew GCC 16.1.0 against Apple clang 21.0.0 and `diverge-both` is a
live verdict here. `rows=1772 comparable=1766 differing=275`, split **243 diverge-one / 22
refs-disagree / 10 diverge-both**; smoke is **12 of 12 in 209.5 s at `-j2`** in `cmake-macos`
with the flip in place, and the bank is re-taken with the classification written into its header.

**The candidates are 10, not the 11 the next-steps entry says** — the row's own enumeration is
12 and N30 closed `bsweep.F16.FNEG.{fold,run}`, which falls to zero on the same take.

**All ten are NaN sign or payload, and this is established per case rather than per digest.**
The five subjects — `csweep.{C32.CMULADD, C64.CMUL, C64.CMULADD, C80.CMUL, C80.CMULADD}` in
their fold and run columns — were replayed point by point with a harness extracted from
`smc_hash`, **whose csweep digests reproduce the subject's byte for byte on all three
compilers**, which is what makes the per-case rows admissible as evidence about the cell. 112
points where the two references agree and mcc differs; every one of the 112 is a quiet NaN in
both answers, **46 differing in the sign bit alone and 66 in the payload, zero in a finite
value**. IEEE 754 leaves both unspecified. Banked, not fixed.

**Two findings the second reference paid for that the row did not anticipate, filed as N36 and
N37.** The first is that mcc's over-wide bit-field semantics are split between the two
references; the second is that the refs-disagree class this file introduced to fix N29's own
triage has the same defect one level down. Both are below.

**As originally filed:**

**N29. The smoke oracle resolution misses the real gcc, so this host has been validating against
one compiler when two were installed.** New 2026-08-12, and it supersedes N28. `CMakeLists.txt`
does `find_program(MCC_SMOKE_GCC NAMES gcc-15 gcc)` — **`gcc-16` is not in that list**, `gcc-15`
is absent here, so it falls through to `/usr/bin/gcc`, i.e. Apple clang. `smokerun` then does the
right thing (N25's guard fires: *"are the same implementation family (clang) … dropping the
second and reporting every difference as diverge-one"*), so the loudest verdict class in the
suite has been **structurally unreachable on this host**, not merely unused.

Re-run with `MCC_SMOKE_GCC=/opt/homebrew/bin/gcc-16`, the arm behaves completely differently:
all 277 `DIVERGE` rows gain a real `clang=` column, 34 categories get **better**, 58 are new, and
**34 `diverge-both` categories appear** — mcc differing from *both* independent references. They
are not scattered. They fall in exactly two families:

| family | what it is |
| --- | --- |
| `bsweep.F16.*` (`FMULADD`, `FNEG`, `FSCALE`, …) | `_Float16` — **this is item 22's subject**, which this file currently files as "may be no defect at all" |
| `csweep.C32/C64/C80.*` (`CMUL`, `CMULADD`, `CDIV`, `CDIVSEL`) | `_Complex` at all three widths |

Both land on areas this file already suspects, which is corroboration rather than noise. Per this
file's own standing rule a `diverge-both` is **a defect until proven otherwise and must never be
banked**, so the 34 are candidate defects and item 22's "no defect at all" reading now has
evidence against it.

**x86_64-linux is not affected, checked 2026-08-13.** `gcc-15` is present here, so
`find_program(MCC_SMOKE_GCC NAMES gcc-15 gcc)` resolves to `/usr/bin/gcc-15` (real GNU) against
`MCC_SMOKE_CLANG=/usr/lib/llvm/22/bin/clang-22`, and the pair is a genuine cross oracle. **This
row is a property of the version ladder, not of the host** — the ladder happens to name the
version this box has and not the one the Mac has, which is exactly why the fix is to prefer the
already-resolved real-GNU `DIFF3_GCC` rather than to add `gcc-16` to the list.

**Deliberately not flipped.** Pointing `MCC_SMOKE_GCC` at `gcc-16` makes `smoke/divergence` exit
1, and smoke is the standing inner-loop gate — turning it red blocks every subsequent validation
until the 34 are triaged, which is a scheduling decision and not one to take silently. The fix is
one `find_program` line (prefer the already-resolved real-GNU `DIFF3_GCC` rather than a version
ladder that rots). **Do it together with triaging the 34**, not before.

**Triage, 2026-08-12 — and the first thing it found is that `diverge-both` is two verdicts
wearing one name.** Splitting the 34 on whether the two references agree with each other:

| class | n | meaning |
| --- | ---: | --- |
| **references AGREE, mcc differs** | **12** | a real consensus disagreement — defect until proven otherwise |
| references disagree, mcc differs from both | 22 | UB or implementation-defined; this file's own rule says **pin mcc's answer and record the disagreement**, which is the opposite of treating it as a regression |

So **22 of the 34 are not defects at all**, and the harness cannot say so: `diverge-both` fires
whenever mcc matches neither reference, without asking whether the references matched *each
other*. `shl.si.w.fold` is the clean illustration — `mcc=1 gcc=0 clang=0x80000000`, a shift by the
full width, i.e. textbook UB where all three are entitled to differ. **This is N23's lesson one
level up**: N23 recorded that a *one*-reference host cannot separate "mcc is wrong" from "the
references disagree"; it turns out the *two*-reference verdict does not separate them either.
`smokerun` needs a third class (references-disagree) before the flip, or 22 false regressions
land with the 12 real ones.

**The 12 candidates:** `bsweep.F16.FNEG.{fold,run}`, `csweep.C32.CMULADD.{fold,run}`,
`csweep.C64.{CMUL,CMULADD}.{fold,run}`, `csweep.C80.{CMUL,CMULADD}.{fold,run}`.

**N36. mcc's over-wide bit-field semantics are split between the two references, and one
reference could not see it.** New 2026-08-13, from N29's flip. This file already records that
mcc follows **gcc** on wide bit-fields: `bf_trunc` in `gen_op` reduces the result of every binary
op on a field wider than 32 bits to the field width, which is gcc's `TYPE_PRECISION` reading of
C11 6.7.2.1p5 and not clang's convert-to-declared-type, and `b3c660f1` did it deliberately to fix
four gcc c-torture tests. With a real gcc in place that is now proved on this host rather than
inferred from the x86_64 bank: **168 of the 185 `bf*` rows match gcc exactly**.

**The other 17 match clang.** Every `*.EQM1` / `*.eqm1` row — `f == -1` on an over-wide unsigned
field at its maximum — has gcc answering 1 and mcc and clang answering 0. So the compiler takes
gcc's field-width precision for the arithmetic and clang's declared-type conversion for the
comparison, and **no single reference can be cited for the pair**. A one-reference panel reported
both arms as the same undifferentiated `diverge-one`, which is exactly why this went unseen: the
verdict class was right and the evidence behind it was one compiler.

Left banked rather than fixed, deliberately. Which half to move is a semantics decision — the
whole area is implementation-defined and this file's rule for implementation-defined answers is
to pin mcc's and record the disagreement. What is *not* defensible is being internally
inconsistent about which implementation is being followed, so the row stays open as a decision
rather than a bug. Size: the comparison path, not `bf_trunc`.

**N38. Eight cells vanished on Darwin behind a copy-pasted predicate, and `must-run.txt` was the
only thing that noticed.** New 2026-08-13, **partly closed the same day**. `CMakeLists.txt` had one
`if(UNIX AND NOT CMAKE_CROSSCOMPILING AND NOT MCC_EMULATOR AND NOT MCC_TARGETOS STREQUAL "WIN32"
AND NOT MCC_TARGETOS STREQUAL "Darwin")` covering eight cells **with no `else()` arm at all**, so on
this host they were not registered, not skipped, and not counted. `git log -L` on those two lines
terminates at `e98fab0a`, whose message never says the words Darwin, macOS, Mach-O or Apple — the
predicate tail is byte-identical to a `cc -dumpmachine` musl probe at `CMakeLists.txt` from
`ce45b08e`, five weeks earlier, where excluding Darwin is correct and irrelevant here. **It is a
copy-pasted predicate, not a decision.** `tools/regstub-lint.py` cannot catch it: `MCC_TARGETOS` is
in its `IDENTITY` exemption set, so a chain gated on it is treated as a different suite. Four of
the eight were in `tests/must-run.txt`, so `ci/must-run-registered` was red here — **a fifth
standing red on this host that no table in this file listed**, which is now the third instance of
that shape in three days after N31, N32 and N33.

**`wide256/gmp-diff` and its known-positive are now registered for real and pass here**: 9402
oracle rows against libgmp at `-O0 -O1 -O2 -O3 -Os`, in 4.2 s. Nothing in that cell was ever
Linux-only — `__int256` is memory-backed precisely so it is not, and `cefd0017` banked arm64 under
qemu. The single obstacle was that `cmake/wide256_diff.cmake` probes GMP with a bare `-lgmp` and
Homebrew's prefix is not on Apple clang's default search path, so the cell would have skipped
honestly rather than run. `find_path`/`find_library` on the Vulkan pattern, forwarded as
`GMP_INC`/`GMP_LIBDIR`. **This is the only oracle-backed proof `__int256` is right and it had never
run on an arm64 host natively.**

**The `ast/o0-baseline` quartet is a visible skip with a stated reason, and the reason is the
interesting part.** The `arm64-osx` key already exists, needs no sysroot, and `o0_ab.sh` already
falls back to the native `mcc` when `key_is_native` matches the banner — which it does here. But
the banked column was taken by the *Linux* cross compiler, so it carries that host's system
headers: re-deriving all 294 rows natively gives **96 matching and 198 differing, 0 failing to
compile**, and the split is diagnostic — the no-`#include` files match byte for byte while 211 of
the 294 pull a system header. **mcc's codegen is host-invariant for this key; the header set is
not.** Registering the cell against that column would go red for a reason that is not an mcc
defect, which is the one thing this harness exists to avoid. The two `must-run` rows are demoted
to `registered` with that written into the manifest, because "the native key needs no sysroot and
no cross compiler, so a skip here is always a bug" is true on Linux and false here.

**What closes it properly, and why it is not cheap.** Either pin the header set for the `*osx`
keys (a macOS SDK sysroot under `vendor/`, mirroring `vendor/gentoo-stage3-*`; nothing like it
exists), or bank a distinct native key — but `o0_ab.sh` refuses `O0_AB_BANK` with `measurable`
("the board is an eleven-row artefact and `all` is the only spelling that demands all eleven"), so
a new key needs a host that can reach every key, which is the Linux cross box, which by
construction cannot take the native Darwin column. **That circularity is the real cost.**

**~~N39. `build/fragments-are-not-tus` has a host-compiler-dependent pin.~~ — FOUND AND CLOSED
2026-08-13, and the cell is registered for real on Darwin as a result.** `mccjit_embed.c` now
includes `mccinv.h`, which it always used and never included. The header is self-contained
(`stdio`/`stdlib`/`string`) and guarded, and `mcc` is a unity build where `mccgen.c` already pulls
it in, so the fix is **byte-neutral by construction and measured as such**: `src/mcc.c.o` has the
same sha256 before and after, from a rebuild that did recompile it. The standalone-failure set is
now `mccast.c mccircap.c mccrir.c` on this host — the pinned three, no per-host pin needed — and
`build/fragments-are-not-tus` and its known-positive run green here in 11.2 s.

**As found:**

**N39. `build/fragments-are-not-tus` has a host-compiler-dependent pin.** New 2026-08-13, found by
running the cell on Darwin for the first time. The check itself is portable — it reads the build's
own `-D`/`-I` back out of `compile_commands.json` and compiles each `src/*.c` standalone — and it
runs here. But the standalone-failure set is **four** on this host against the pinned **three**:
`mccjit_embed.c` joins `mccast.c`, `mccircap.c` and `mccrir.c` because Apple clang 21 rejects its
call to an undeclared `mcc_inv_add` as an error where the Linux host compiler does not. The three
pinned files fail for a structural reason the cell's own message explains (their whole body sits
behind `#if (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))` without including `src/mcc.h`);
`mccjit_embed.c` fails for a diagnostic-strictness reason, which is a different thing and should
not be pinned as though it were the same. **Two fixes, and the smaller one is better**: declare
`mcc_inv_add` where `mccjit_embed.c` can see it standalone, which makes the set three everywhere
and needs no per-host pin. It was registered as a visible skip on the way to this, which is
what it should have been all along.

**N37. The refs-disagree class is a property of a whole digest, so a category containing one
reference disagreement is cleared for every other point in it.** New 2026-08-13, **and its
instrument half is CLOSED the same day; what is left is the compiler half.** `smokerun` now
re-runs every `diverge-refs` category through a new `--points <category>` mode on the smoke
subject and banks `div diverge-masked:<category>` **valued at the number of points inside it where
the two references agree and mcc differs** — a monotone-decreasing ratchet on exactly the quantity
the digest verdict was clearing, not a note. Seven rows on this host: 587 each for
`csweep.C32.{CDIV,CDIVSEL}`, 283 each for the C64 and C80 twins, 10 for `csweep.C32.CMUL`. **It
reproduces the hand triage below to the point**, which is what made it worth landing rather than
recording. Known-positive taken by hand: perturbing one banked count 283 → 282 gives
`FAIL bail ratchet div diverge-masked:csweep.C64.CDIV rose 282 -> 283`.

**A second defect fell out of building it, and it was the more serious one.** The bank's scope
match compares the text after the first space in a key, and the arm owns `diverge-`, so
`refs-disagree` rows were **outside the scope entirely**: never ratcheted, and — because
`bank_write` copies every *unowned* bank row forward while also writing the run's own categories —
**duplicated on every re-bank**. Measured: one extra `--divergence --rebank` took the 22 rows to
44. All four classes are now `diverge-one` / `diverge-both` / `diverge-refs` / `diverge-masked`,
one prefix, and a second re-bank is byte-idempotent. Both banks were renamed in place.

**`tests/smoke/bails.txt` needs one re-bank on x86_64-linux and that cell is red there until it
gets one** — the `diverge-masked` rows for that host cannot be measured from here, and they are
meant to be read before they are banked. The header says so at the top of the file.

**As filed, and the hand triage still stands as the evidence:** it is N29's
own finding one level down. N29 established that `diverge-both` conflated "mcc contradicts a
consensus" with "the references disagree and mcc picked neither", and `smokerun` grew a third
verdict class to separate them. That class is computed on the **category digest**: if the two
references differ anywhere in a category, the whole category is `refs-disagree` and every point
in it is cleared.

**Measured, on the same per-case replay that triaged N29.** `csweep.C64.CDIV`, its C80 twin and
both `CDIVSEL` columns are classified refs-disagree and therefore banked as
implementation-defined. Each holds **283 points where the references DO agree and mcc differs**,
and **44 of those are finite values rather than NaNs**. Of C64.CDIV's 60 differing finite
components, 52 are 1 ulp and 5 are 2 ulps — but **three are not rounding at all**. The worst is a
real part of `-1.694065894339194e-21` against a consensus `-1.1102230245141343e-21`, a **53%
relative error**, on a complex divide whose real part cancels catastrophically. Complex division
accuracy is not specified by C outside Annex G, so this is quality-of-implementation rather than
a conformance defect — but it is exactly the class the harness exists to surface, and the harness
currently reports it as cleared.

**Two separable items.** The instrument one is the real row: a category should carry its own
per-point split, so `refs-disagree` means "every differing point had disagreeing references" and
not "at least one did". Cheap, and it is the only way any of the numbers above can be re-taken by
a cell rather than by hand. The compiler one is the complex-divide accuracy — gcc and clang both
route through the `__divdc3` lineage (Smith's algorithm with scaling) and mcc does not; whether
to follow them is a decision, not a bug report.

**~~N30. `_Float16` negation quiets signaling NaNs.~~ — CLOSED 2026-08-13 on x86_64-linux, and
the arena problem attempt 3 hit is closed with it.** The finished fix is not a constant/runtime
split in `unary()` at all: it is **one line removed and one arm added to a primitive that was
already captured**.

`unary()`'s `'-'` case now calls plain `gen_opif(TOK_NEG)` for `_Float16` — the promote/negate/
demote round trip is gone — and the f16 sign flip lives in `gen_opf`'s `TOK_NEG` arm, which is the
*captured* primitive (`IR_CAP_W1(gen_opf, IR_OP_OPF)`). That is what makes it arena-invisible:
the RIR turns `IR_OP_OPF(TOK_NEG)` into an `AST_OP_FNEG` node, `mccast.c` **already** special-cases
`AST_OP_FNEG` on `VT_FLOAT16`, and the replay re-invokes the same primitive. Parse and replay do
one identical atomic thing.

**Why attempt 3 could not work, established by experiment rather than argument.** The AST replay
of a Binop is `ast_replay_value(child0); ast_replay_value(child1); gen_op(bop)` — the children are
rebuilt with their **recorded** types. Attempt 3 retyped `vtop` to `unsigned short` at parse time,
which the arena never saw, so replay reconstructed an f16 operand and handed it to `gen_op('^')`.
Removing the type *restore* changes nothing — tested, still 2 errors — so "restore the type before
the node is recorded", the smaller of the two routes this file proposed, is **refuted**; only the
atomic route works. `ir_cap_depth` suppresses nested capture, which is why the arm has to sit
inside `gen_opf` and not in `gen_opif` or `unary()`.

**A four-line reproducer, replacing the whole-smoke-subject one:**

```c
long double f(long double a){ volatile _Float16 va = (_Float16)(a); return (long double)(-(_Float16)(va)); }
```
under `MCC_RIR_PROD=2 MCC_RIR_ABORTWHY=1`. `volatile` is the ingredient — it keeps the operand an
lvalue the arena records.

**All five targets, and only one of them needed thought.** `gen_negf` is `#define`d to the backend
`gen_opf` on x86_64/i386/arm64, so each got an f16 arm: x86_64 and i386 emit a single
`xor $0x8000, %eax`, arm64 `mov w30, #0x8000; eor w0, w0, w30`. **riscv64 needed nothing** — its
generic `gen_negf` already flips the top bit of the high byte in memory, which is correct for a
2-byte half. **arm was the one that was wrong**: its `gen_negf` is `0 - x`, an *arithmetic*
operation that quiets a signaling NaN, so it now routes f16 through the same generic bit flip.
Verified by relocation census on all five cross compilers: `extendhf`/`truncsf` helper calls went
2 → 0 on arm and are 0 everywhere else, and the emitted instruction was read out of the object on
each target.

**Results.** Exhaustive 65536-pattern sweep: **0 mismatches**. The `MCC_RIR_PROD=2
MCC_RIR_ABORTWHY=1` configuration that defeated attempt 3: **0 errors**. smoke **12/12**, exec
**7675/7675**. The divergence arm improves: `differing` 259 → 251 and **`mcc-differs-from-both`
39 → 31**. The `-O0` baseline moves exactly one object on each of the eight measurable target
keys — `tests/exec/types/float16.c`, the only `_Float16` subject — re-banked from the cross build.

**Eight of those improvements are not N30's and the bank now says so.** `bsweep.F80.FSELMIX{B,L,R}`
and `xsweep.F80.SI` fell to zero because of **item 24's `fistpl` fix** earlier in this session,
which the divergence cell never flagged because "better" is not a ratchet violation. Recorded as
note 15 in `bails.txt`. N30's own category, `bsweep.F16.FNEG`, does **not** change class — see the
correction below.

**As originally filed:**

**N30. `_Float16` negation quiets signaling NaNs. Root-caused 2026-08-12; a real defect, and the
first one the second oracle paid for.** Sweeping all 65536 `_Float16` bit patterns through `-v`:
gcc-16 and clang produce identical results, mcc differs on **1022** of them. 1022 is exactly the
number of signaling `_Float16` NaNs — 2 signs × 511 nonzero mantissas with the quiet bit clear.
Every one is the same error: `7c01 → fe01` where the answer is `fc01`, i.e. **mcc sets the quiet
bit (0x0200) while flipping the sign**. IEEE 754 makes negation a non-arithmetic sign-bit flip
that must neither signal nor quiet.

**It is `_Float16`-specific**, which names the mechanism: the identical sNaN through `float`
(`7f800001 → ff800001`) and `double` (`7ff0…01 → fff0…01`) is correct in mcc, so f32/f64 negate
natively while f16 negation is lowered through a promotion to `float` and back.

**The exact lowering, located 2026-08-12** — `unary()`'s `case '-'` in `src/mccgen.c`:

```
} else if (is_float16(vtop->type.t)) {
        gen_cast_s(VT_FLOAT);
        if (!gen_negf_const_ref())
                gen_opif(TOK_NEG);
        gen_cast_s(VT_FLOAT16);
}
```

The `f16 → f32 → f16` round-trip is the quieting: a conversion is an arithmetic operation and
signals on an sNaN, returning a quiet one, where negation must do neither.

**The fix has more substrate than expected, and this is the actionable part.**
`gen_negf_const_ref` **already flips the sign byte in place** — it copies the constant to
`rodata`, computes the sign byte via `float_sign_byte(bt, size)` and inverts it — which is
precisely the operation `_Float16` needs. It simply excludes the type:
`if (bt != VT_FLOAT && bt != VT_DOUBLE && bt != VT_LDOUBLE) return 0;`. Admitting `VT_FLOAT16`
there, and calling it **before** the promotion rather than between the two casts, fixes the
constant path with no new codegen.

**Do not fix only that half.** Today `.fold` and `.run` produce the *same* wrong digest
(`a2b76db693d8cd25`), so they agree with each other; fixing the constant path alone would make
them disagree, converting one conformance defect into a fold/run inconsistency, which this file
treats as the more serious class.

**Attempted twice, backed out twice, and the route is now fully specified — 2026-08-13.**

**Attempt 1 was the wrong target.** arm64's native `fneg h0` is `0x1ee14000` (ftype 3, verified
against clang's own output), and `gen_opf`'s `TOK_NEG` arm is two characters from emitting it.
It aborts: `Assertion failed: (0), function load, file arm64-gen.c, line 720` — `load`'s
register-to-register arm has no `VT_FLOAT16` case. **But that is irrelevant**, because the value
is never in an FP register: `R_RET` returns **`REG_IRET`** for `VT_FLOAT16`, so a half already
lives in a *general* register. Generalising from that failed approach to "needs FP register
support" was wrong.

**Attempt 2 found the right operation and two real constraints.** Retype the vstack entry to
`unsigned short`, `gen_op('^')` with `0x8000`, retype back — no promotion, so the payload
survives. It **works**: the exhaustive sweep over all 65536 half patterns goes 1022 mismatches to
**0**, mcc/gcc-16/clang all digest `6d24f705d63b0383`, and `bsweep.F16.FNEG` leaves the
divergence arm's diverge-both list (12 → 10). It is backed out only because of *where* it breaks:

1. **A bare retype breaks non-scalar contexts** — `tests/smoke/fcases.h` fails to compile with
   `invalid operand types for binary operation`.
2. **Forcing the value to a register first (`gv(MCC_RC_INT)`) fixes that and breaks the other
   end** — `'_Float16' conversion is not a load-time constant`, because a static initializer
   needs the negation folded, not emitted.

**So the finished shape is a constant/runtime split**: fold the sign bit in the `SValue` for a
compile-time constant, and do the `^ 0x8000` retype for the runtime case. Both halves must land
together — fixing only the constant path makes `.fold` and `.run` disagree.

**Attempt 3 built exactly that and still fails one configuration — 2026-08-13.** The split is
written and the representation facts behind it are confirmed: `_Float16` constants are stored in
`cv->i` as the 16-bit pattern (`write16le(d, (uint16_t)cv->i)`), and `MCC_RC_TYPE` returns
`MCC_RC_INT` for `VT_FLOAT16`, so both halves are expressible.

```
if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST)
        vtop->c.i ^= 0x8000;
else    /* retype to unsigned short, gen_op('^') 0x8000, retype back */
```

With it: the exhaustive sweep is **0 mismatches**, `tests/smoke/subject.c` compiles clean both
with and without `-DSM_REF_BUILD=1`, and the load-time-constant error is gone. **But
`smoke/native` and `smoke/strats-known-positive` still fail**, with
`fcases.h:570` / `:803 invalid operand types for binary operation` — under a compile
configuration `smokerun` uses that a direct `-O1 -Itests/smoke` build does **not** reproduce.
Reverted; smoke is 12/12 again.

**ISOLATED 2026-08-13. The failing configuration is `MCC_RIR_PROD=2` together with
`MCC_RIR_ABORTWHY=1`** — `smokerun`'s `set_census_env` sets both. Neither reproduces alone, and
`MCC_FORCE_REPLAY`, `MCC_AST_EVAL_LADDER` and linking-versus-`-c` are all **irrelevant**, which
is what made this look configuration-shaped rather than code-shaped. Standalone reproducer:

```
MCC_RIR_PROD=2 MCC_RIR_ABORTWHY=1 mcc -w -O1 -Itests/smoke tests/smoke/subject.c -lm -o /tmp/x
  -> tests/smoke/fcases.h:570: error: invalid operand types for binary operation
  -> tests/smoke/fcases.h:803: error: invalid operand types for binary operation
```

So the defect is **in the RIR production-replay path, not in the negation**: the arena records
the retyped `unsigned short` operand and the replay rebuilds it as something `gen_op` then
rejects. Both cited lines are function *closing* braces, so the reported line is a boundary and
the offending operand is inside the `SMF_ARM` expansion above it.

**That reframes the remaining work.** Either the retype must be invisible to the arena — do the
XOR without changing `vtop->type.t`, or restore the type before the node is recorded — or the
capture must carry the original `VT_FLOAT16` type across the round trip. The first is the
smaller change and is where to start.

**Nothing else blocks the oracle flip.** With this fixed the divergence arm is 10 diverge-both,
all of them the implementation-defined complex NaN selection reduced above, and 22
references-disagree.

**Reproducer, four lines and no corpus:** negate every one of the 65536 `_Float16` bit patterns
and compare against `bits ^ 0x8000`. gcc-16 and clang give 0 mismatches; mcc gives 1022.



**Mechanism confirmed in the code, and the x86_64 arm re-measured 2026-08-13 — the fix is a
decision, not a sweep.** `unary()`'s `'-'` case in `src/mccgen.c` does exactly what the row
predicts for `is_float16`: `gen_cast_s(VT_FLOAT)`, then `TOK_NEG`, then `gen_cast_s(VT_FLOAT16)`.
**This is the same conclusion the backed-out arm64 attempt above reached, arrived at from the
other side, and the two together name the cheap route.** That attempt tried the *native* `fneg h0`
and hit `load`'s missing `VT_FLOAT16` case — the register layer cannot hold a half in an FP
register. It does not have to: `R_RET` and `MCC_RC_TYPE` both route `VT_FLOAT16` to
`REG_IRET`/`MCC_RC_INT`, so the value is already a 16-bit pattern in a **general** register on
every target, and the sign flip is an integer `^ 0x8000` that needs no FP path at all. That is the
form to try next; it is untested, and it is not what was backed out.

**What stops it being obvious is that the references do not agree with each other across hosts,
or with themselves across levels.** Full 65536-pattern sweep on x86_64-linux, gcc 15.3.0 and
clang 22.1.8:

| arm | 0x7c01 → | differs from mcc |
| --- | --- | ---: |
| mcc, all levels | `fe01` | — |
| gcc `-O0` and `-O2` | `fe01` | 0 |
| clang `-O0` | `fe01` | 0 |
| **clang `-O2`** | **`fc01`** | **1022** |
| gcc-16 and clang on arm64/macOS (N30 as filed) | `fc01` | 1022 |

So on this host **mcc matches gcc at every level and clang only at `-O0`**, and *clang disagrees
with itself* across levels — at `-O2` it folds the negation to a sign-bit xor, below that it goes
through the softfp promotion and quiets exactly as mcc does. A separate constant-fold probe
splits them again: gcc-15 folds `-(_Float16)0x7c01` to `fc01` while its runtime answer is `fe01`.
**IEEE 754 §5.5.1 is unambiguous — negate is non-arithmetic and must not quiet — so `fc01` is
right and every `fe01` is a softfp-lowering non-conformance**, mcc's included.

**~~The decision the fix forces, stated so it is not taken by accident.~~ — I predicted this
would make `F16.FNEG` a `diverge-both` on x86_64, and it does not. Measured 2026-08-13 after
landing:** the category stays `diverge-one` and simply **changes sides** —
`mcc=7dd4c179… gcc=e2a741a1… clang=7dd4c179…`, where before it was `mcc=e2a741a1… gcc=e2a741a1…
clang=7dd4c179…`. mcc used to agree with gcc-15 and now agrees with clang-22. So there was no
decision to force: the harness's clang reference already did the IEEE-correct thing, and the fix
moves mcc onto it. The reasoning that produced the wrong prediction is still worth keeping,
because the *shape* of it recurs — **a `diverge-both` verdict is only as good as the reference
pair's own agreement, and this pair does not agree with itself across `-O`** — it just did not
bind here. Banked as note 16 in `bails.txt` so nobody reads the unchanged category as an unchanged
answer.

**The 10 complex categories — partly triaged 2026-08-12, and still open.**

First, **`C80` is `C64` on this host.** Their digests are byte-identical for both mcc and the
references, because arm64/macOS `long double` *is* `double`. So the ten categories are really
three computations, and any `C80` finding here is a duplicate — a `C80`-specific defect is not
observable on this machine at all.

Sweeping complex `*` and `*`+ over 900 pairs built from the same corpus classes the harness uses
(min/max normals, subnormals, ±0, ±inf, default NaN, **sNaN**, and the ordinary ratios):

| comparison | rows differing of 900 |
| --- | ---: |
| **gcc-16 vs clang** | **167** |
| mcc vs gcc | 10 |
| **references agree AND mcc differs** | **1** |

**The references disagree with each other on 167 of 900 rows** — complex multiply with extreme
operands is largely latitude, not consensus, which is why this whole family reads as loud and
means little. The single consensus row is `NaN × sNaN`: mcc yields the default quiet NaN
`7ff8000000000000` where both references yield `7ff8000000000001`, i.e. they propagate the sNaN
operand's payload and mcc does not. **IEEE 754 §6.2.3 leaves which input NaN is propagated
implementation-defined**, so this is IDB and not a defect — and mcc's payload handling is
otherwise correct: `sNaN*2`, `qNaN*2` (payload 5), `sNaN+2` and the scalar complex case are all
byte-identical across the three compilers.

**REDUCED 2026-08-12, on the harness's own corpus this time — and they are not defects.**

The instrument to use was `tests/smoke/subject.c --dump`, which emits per-`(type, op)` `V csweep`
rows built from the same `smf_bfill_F64` corpus the digests are taken over. Built with mcc,
gcc-16 and clang at the harness's own `-O1 -ffp-contract=off -DSM_REF_BUILD=1`, it reproduces the
five categories exactly: `C32.CMULADD`, `C64.CMUL`, `C64.CMULADD`, `C80.CMUL`, `C80.CMULADD` —
5 of 48 rows where the references agree and mcc differs.

Reducing per pair over all 4096 `(i, j)` combinations of that corpus, in every one of the five:

| category | divergent pairs | both sides NaN | non-NaN |
| --- | ---: | ---: | ---: |
| `C32.CMULADD` | 32 | 32 | **0** |
| `C64.CMUL` | 16 | 16 | **0** |
| `C64.CMULADD` | 19 | 19 | **0** |
| `C80.CMUL` | 16 | 16 | **0** |
| `C80.CMULADD` | 19 | 19 | **0** |

**102 divergences, every one NaN-against-NaN, none touching a finite value.** They differ only in
the NaN's sign bit (`fff8…` vs `7ff8…`) or its payload (`7ffc…` vs `7ff8…`), and all of them are
in the *imaginary* component. IEEE 754 leaves both open: §6.2.3 makes the propagated payload
implementation-defined and the sign of a NaN result unspecified. **So these ten categories are
IDB and should be pinned, not fixed** — which is what this file's own rule prescribes for a case
the references cannot adjudicate.

**Trap that nearly produced a false result, worth more than the finding.** A first pass reported
*32 real defects* in `C32.CMULADD`. It was the classifier, not the compiler: `smf_enc` encodes
per type, so C32 rows carry **float** bit patterns in a 64-bit field, and `0x7fc00000` — a float
quiet NaN — does not parse as a double NaN. Testing double exponent bits against float-encoded
values turned 32 implementation-defined NaNs into 32 fabricated defects. **A NaN classifier must
know the width of what it is classifying.**

**Consequence for the flip.** The original 34 now decompose completely: 22 references-disagree,
10 implementation-defined NaN selection, and **2 real — N30's `bsweep.F16.FNEG`**. Fix or
deliberately bank N30 and the oracle flip has nothing left blocking it.

**~~N25. `smokerun`'s reference pair is unchecked everywhere except the divergence arm.~~ — CLOSED 2026-08-12, same day it was opened.** `pass_oracle()` runs the same `__clang__`/`__GNUC__` probe and says which it got. No pass row changed answer, which is the expected result — the defect was in what the line claimed. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

**~~N21. The ladder census makes compilation non-deterministic.~~ — NOT A DEFECT, closed 2026-08-12.** Six bytes differ, the disassembly is identical, and the six bytes are `__TIME__`. The census only makes the compile slow enough for two runs to straddle a second. **STANDING TRAP: any object-identity comparison over `tests/exec` or `tests/diff` must export `SOURCE_DATE_EPOCH`** — it accounted for three separate claims in one day. `tests/gpu/always_gpu_parity.sh` got its object assertion back. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

**~~N34. The ladder's GPU arm crashes the compiler at exit, on any input, at any level, on either
device.~~ — CLOSED 2026-08-13, the same day, and the cause is `atexit` ordering.**
`ast_ladder_gpu_setup` registered `atexit(ast_ladder_gpu_report)` **before** the warmup dispatch
that first brings the device up. That warmup is what makes the Vulkan loader `dlopen` the NVIDIA
ICD, which pulls in `libEGL_nvidia.so.0` and lets it register an `atexit` handler of its own — and
`atexit` runs LIFO, so the ICD's later-registered handler runs *first*, tears down its EGL state,
drops the last reference on `libnvidia-eglcore` and friends, and the loader unmaps 36 MB. mcc's
handler then calls `mcc_gpu_quiesce()`, whose first Vulkan call dispatches into a now-unmapped
page. Moving the `atexit` to *after* the warmup makes mcc's handler the later registration and
therefore the earlier to run. **One line moved.**

Confirmed by backtrace (`mcc_vk_release` → `mcc_gpu_quiesce` → `ast_ladder_gpu_report` →
`__run_exit_handlers`, faulting in a hole `info proc mappings` shows is unmapped), by
`LD_DEBUG=files` naming the four NVIDIA modules destroyed between the warmup line and the crash,
and by a breakpoint on `dlclose` whose caller is an atexit handler inside `libEGL_nvidia.so.0`.
**Neither of the two leads this row offered was implicated**: nothing retains a `mcc_gpu_mem()`
pointer (N6.1), and mcc does not `dlclose` libvulkan itself. **The hazard was already written down
in this tree** — `src/mccast.c` carries a note describing exactly this ordering rule for the
*slice-inline* handler, and the ladder's own registration violated it.

**It closes the smoke gate on this host**: `smoke/engines` 27.6 s, `-known-positive` 25.9 s,
`engines-identity` 77.6 s, `smoke/device` 7.4 s, `device-known-positive` 7.3 s — the five cells
that were timing out, now 6/6 in 146 s. **Residual, deliberately not taken:** the fix depends on
the warmup reaching `mcc_gpu_run`, which is guarded by `if (mcc_gpu_emit(...))`. `mcc_gpu_emit` is
pure codegen and cannot touch Vulkan, so the guard cannot fail for a device reason — but if it
ever did, the ICD would load at the first real dispatch, after the `atexit`, and the hazard would
return. The belt-and-braces variant is `atexit(mcc_gpu_quiesce)` at the end of `mcc_gpu_init`
(idempotent, so the later call becomes a no-op); it was not taken because it also moves the
teardown ahead of `mccjit_shutdown`, `mcc_stats_finish` and `rir_report`, and the design note in
`src/mccgpu.c` says the single exit-time teardown hangs off `ast_ladder_gpu_report` on purpose.

The report as filed, kept because the bisection is the transferable part:

```
$ echo 'int x;' > e.c
$ MCC_AST_EVAL_LADDER=1 MCC_AST_EVAL_LADDER_GPU=1 mcc -w -O1 -c e.c -o e.o
[ladder-gpu] init ok dev=NVIDIA GeForce RTX 5070 Ti Laptop GPU qfam=0 score=5564
[ladder-gpu] warmup rc=1 (582 units)
Segmentation fault (core dumped)
```

**The compile itself is fine and the crash is at process exit.** `e.o` is written, is 1052 bytes,
and is byte-identical to the one the CPU arm produces — so this is not a codegen defect and
nothing it emits is suspect. The last thing printed is the warmup diagnostic;
`ast_ladder_gpu_report`'s own `[ladder-gpu] tried= … rungs=` line never appears, and the first
statement of that `atexit` handler is **`mcc_gpu_quiesce()`**. That is where to look, and **it is
the hazard N6.1 already names** — *"the quiesce now unmaps the shared address space, so nothing
may retain a `mcc_gpu_mem()` pointer across shutdown"*, a hazard that row records as created by
the wave which made the quiesce destroy things.

What the bisection rules out, all measured:

| varied | result |
| --- | --- |
| input program | `int x;` with no functions crashes exactly like `tests/smoke/subject.c` |
| `-O1` vs `-O4` | identical |
| `MCC_GPU_DEVICE=0` (AMD 610M / RADV) vs `=1` (RTX 5070 Ti) | identical, so not a driver quirk |
| `MCC_GPU_NO_HOST_IMPORT=1` | identical, so not the host-pointer import path |
| the harness | reproduces standalone; the same compile without the two env vars is clean |

**With a display attached it hangs instead of crashing, and that is the same bug wearing a
different coat.** Under `DISPLAY=:1.0` the process blocks in `poll` at **0% CPU** with no
`/dev/dri/*` descriptor open, holding one socket, having loaded `libxcb-dri3` alongside both
`libvulkan_radeon.so` and the NVIDIA stack — a display-server round-trip in the same teardown
that segfaults when there is nobody to answer. **Not a sandbox artefact**: it reproduces
identically with the tool sandbox disabled.

**Consequences to hold onto.** (1) `smoke/engines`, `smoke/engines-known-positive`,
`smoke/engines-identity`, `smoke/device` and `smoke/device-known-positive` all hit their ctest
`TIMEOUT` on this host, so **the standing inner-loop rule cannot complete here** even with N32
fixed — seven cells of twelve are green and five are unrunnable. (2) It is very likely the same
obstruction the rank-5 re-take needs cleared: a census under `MCC_AST_EVAL_LADDER_GPU=1` cannot
finish inside `ladder_gpu_parity.cmake`'s `TIMEOUT 120`, which is exactly what was seen. (3) The
`--jit-always-gpu` measurement at the top of this file was taken on this class of hardware and
completed, so this is a regression or a configuration difference, not a permanent property of the
host — **do not read that write-up as evidence the arm works today.**

**The `reg` fixture swap, and two corrections to C5's caveat.** The caveat asked for a non-VLA
reduction and asserted one existed in the corpus, citing `bounds/bound_signal.c` (`reg=3`),
`bounds/bound_setjmp.c` (`reg=2`) and `vla/basic.c` (`reg=2`). **Two of those three are VLAs
too** — `bound_signal.c` has `int arr[n]` and `bound_setjmp.c`'s `stack()` has two `int a[n_x]` —
so the evidence offered for the claim was not evidence. The claim was nonetheless right: a sweep
of all 841 `tests/**/*.c` at `-O1` finds **62 files still emitting `reg`**, among them
`tests/exec/statements/switch.c`, `goto.c`, `ternary_op.c` and `codegen/cmp_invert.c`.

**The replacement is a `switch` with at least one `case`:**

```c
int f(int x) { switch (x) { case 1: return 1; } return 0; }
```

`reg` at `-O0`, `-O1`, `-O2` and `-O3`, with an empty blocker set otherwise. The mechanism is
`mccgen.c`'s switch epilogue: the control value is stashed in `sw->sv` *before* the body is
parsed and re-pushed and `gv`'d after it, so every `gcase` comparison has an operand that is
already a machine register when the arena records it, and `ast_low_node` classifies
`v < VT_CONST` as `AST_LOW_REG`. That is precisely the escape the caveat kept missing with
`a<b`, `a<b ? a : b` and friends — those materialise *into* the arena; the switch value is
materialised *out of band*. Measured negatives, all `blockers=-`: `default:` alone, an empty
`switch(x){}`, `if`/`goto`, `(a<b)&&(b<a)`, `x?y:0`, a `while` loop. Robust across 1 case, 8
dense cases (the `gcase_jumptable` path) and a `long` control value; `gcase` is in
target-independent `mccgen.c` and the class comes from `VT_VALMASK`, so it should hold on
PE/cross.

**A second finding, now unpinned by anything.** The `reg` the old fixture still showed at `-O0`
was a harness artefact, not a level effect: `rir-coverage.py`'s `run_one` sets
`MCC_FORCE_REPLAY=1` **only** at `-O0`. Holding that constant, `-O0` gives `opaque,reg` and `-O1`
gives `opaque`, and with force-replay off `-O0` records no body at all. `MCC_RIR_LOW_DUMP='*'`
shows exactly what differs: the VLA body records a trailing orphan root `Ref op=0 t=0x1024 -> reg`
(VALMASK 0 = hardware register 0) at `-O0` and never at `-O1`, with every other node
byte-identical. So VLA lowering leaks a live-register `Ref` into the arena at `-O0` under
force-replay, and the fixture was banking the leak. Harmless as far as anything measures, and
after the swap **nothing in the suite depends on it** — which is the argument for writing it down
rather than chasing it.

**Two cells fail under `-j` and pass alone — and `slice/quiesce`'s reason is structural.**
`slice/quiesce` **destroys the device** (that is its whole subject), so running it concurrently
with the other device-touching `slice/*` cells is inherently racy: 54 of 55 green at `-j4` with
`quiesce` the only failure, and **17.8 s green alone**. Demonstrated rather than inferred: two runs
of the *same* guarded `slicerun` binary, back to back on `8a70f8b7`, returned **55/55 and 54/55**,
`quiesce` the only cell that moved. Same code, same commit, both verdicts — so the variable is what
shared the machine and nothing else. It is not a flake to be re-run until it
passes — it is a cell that cannot share a machine with its own family, and the honest fix is a
ctest `RESOURCE_LOCK` on the device rather than a retry.

**`rir-nofb-probe-self` fails under `-j` and passes alone — measured three times, 2026-08-13.**
Not a red and not filed as one, but it will waste somebody's afternoon. Contended in a `-j4`/`-j6`
sweep it **fails** at 20.6 s and at 84.2 s; run by itself it **passes** at 539.7 s and at 726.9 s.
It is not hitting a `TIMEOUT` — it exits early with a failure — so the symptom is a red cell and
not a timeout row, which is what makes it look like a real defect. This is **N26's shape on a
different cell**: whether it passes is a function of what else is running. Anything this file says
about `rir-nofb-probe-self` must state what shared the machine, and the honest way to check it is
alone.

**~~N35. Five reds arrived from the arm64 host's last two waves, all verified pre-existing.~~ —
ALL FIVE CLOSED 2026-08-13.** Three
at `c7df5209` and two more at `5825d894`, which landed mid-wave. Three were harness — a cell that
could not execute, a cell missing from a gate's `else()`, and a two-suite floor on a one-suite
host — and two needed a compiler attribution, which they now have. Found by running the whole `jit/ ast/ rir optlevel diff3/ superopt/
fmt/ docs/ ci/` family — 526 cells — on x86_64-linux, and confirmed by building `c7df5209` into a
scratch tree and re-running there.

| cell | what it says | reading |
| --- | --- | --- |
| ~~`rir-coverage-census`~~ **CLOSED 2026-08-13, and my attribution of it was wrong** | `-O0 lowerable[elf] bodies_pct regressed: 15.4134% < banked 15.4642%` | **`eee6c1f2` is exonerated** — it never touched the `wide` block, which is the corpus the failing cell measures; `bank['wide']` is byte-identical across `eee6c1f2~1 → eee6c1f2`. The real cause is **`85bf6a3d`**, and the drop is two things at once. Re-banked with the attribution, per the standing rule. Detail below |
| ~~`rir-lowerable-classes`~~ **CLOSED 2026-08-13** | `reg.c -O1/-O2/-O3: lowerable class reg no longer reproduces` (`-O0` still did) | **C5's caveat came true, and the follow-up it asked for is done.** The `reg` class was never in trouble — **62 of 841 sources still emit it at `-O1`**; only the VLA fixture stopped. Replaced with a non-VLA `switch` shape that yields `reg` **and nothing else** at all four levels, so the fixture is single-class now and the shared-VLA coupling with `opaque.c` is severed. Detail below |
| ~~`jit/xoracle-coverage`~~ **CLOSED 2026-08-13** | cannot reach `--min-cross 400` on one suite | **a missing prerequisite reported as a failure.** `MCC_XSUITE_LLVMTS` has no checkout here, and the `else()` branch that handles it already carried a comment saying *exactly* what would happen — *"the cell then fails for what reads as a coverage reason when the cause is a path that does not exist"* — printed a `STATUS` line, and then registered the cell anyway. It now skips with that reason. **`--min-cross 400` is a two-suite floor and one suite tops out at 379, so the cell could only ever fail**; `jit/xoracle-conformance` is unaffected because its floor is `--min-pass 100` |
| ~~`ci/registration-stubs`~~ **CLOSED 2026-08-13** | `1 of 57 capability-gated registration chain(s) drop cells instead of skipping them` | **the same commit that added `rir/rec-miss` did not add its skip stub.** The lint names the branch and the line; one `mcc_skip_test` closes it. Worth noting the lint found this *after* the cell was made runnable — a cell that cannot execute and a cell that is missing from a gate's `else()` are two different reds from one commit, and only the second is something a sweep can find |
| ~~`rir/rec-miss`~~ **CLOSED 2026-08-13** | `execute_process given unknown argument "ENVIRONMENT"` | **the cell could never run.** `execute_process` has no `ENVIRONMENT` option — that belongs to `set_tests_properties` — so `5825d894`'s new cell died on its first CMake statement, at both of its two injection sites. Rewritten as `${CMAKE_COMMAND} -E env MCC_RIR_REC_FORCE_MISS=1 …`. **It is a good cell**: 6 subjects, both floors present (`_ran < 4` and `_moved == 0`), and it reports *"the injection moved 3 object(s), so the frontier fallback is the code under test and not a no-op"* |

**The census drop, attributed per body — and it is a real loss, not just dilution.** Bisected on
this host over the `wide` corpus at `-O0`: `17ac9ab4` reproduces the banked 15.4642 to four
places, `0176c562` is the last passing commit, `890a822c` the first failing, and HEAD is 15.4066.
Diffing the per-body inventory `9fe32126 → HEAD`, matched on `(file, func)`:

| | bodies | lowerable | pct |
| --- | ---: | ---: | ---: |
| pre-existing bodies, then | 4436 | 691 | 15.5771% |
| pre-existing bodies, now | 4436 | 686 | **15.4644%** — a real loss of −0.1127pp |
| the 97 bodies that joined | 97 | 12 | 12.3711% — dilution of −0.0578pp |

`gone = 0`, `gained = 0` beyond those five. **The five that stopped being lowerable are all
VLA-parameter shapes** — `vla/basic.c::grid_trace`, `star_vla_prototype.c::{inner,both}_star`,
`array_qual_params.c::diag`, `vla_param_side_effects.c::nested` — and they were lost at
**`85bf6a3d`**, which moved `func_vla_arg()` *inside* the recorded arena (`src/mccgen.c`) so the
JIT would stop dropping the bound's side effects. Each gains exactly one `reg`-class `Ref` for the
bound left in a hardware register. **Four of the five recover at `-O1`**, which is why only `-O0`
went red while `O1`–`O3` sit at 15.4570 against a 15.4242 floor.

**So this is a correctness fix being paid for in a coverage number, and the right action is to
re-bank.** But for those five bodies HEAD would be 15.5165%, above the red line; dilution alone
would also have cleared it. The loss is the larger and the binding half.

**~~Two follow-ups so this never needs a hand-diff again.~~ — BOTH DONE 2026-08-13.** *(1)* the key
carries the TU; *(2)* the `wide`/`elf` inventory is taken — **4574 bodies**, and the cell now
attributes 4550 at `-O0` with 0 gone and 0 new. Two amendments to the plan as written below. The
first option was taken and the second was **rejected on inspection**: dropping `<command line>`
rows loses real bodies from the attribution set and leaves the `bitfields.c` pair colliding, so it
fixes neither half. And the inventory was taken with a **new `--update-bodies`**, not
`--update-bank-low`, because the latter also rewrites the percentage floors — which is the blind
re-bank this very section warns about, arriving as a convenience. The floors are untouched.

As originally filed:
The cell said *"attribution is
unavailable"*, and the second half of that hedge is the true one: 4550 `[rir-low-body]` rows are
emitted and reconcile exactly with the aggregate, but they collapse to **4538 keys**, so
`low_body_index()` returns `None` and the per-body ratchet is disarmed for the whole corpus. The
12 duplicates are five keys — `<command line>::__va_arg_inline` ×7 and two `__mcc_ov_*` helpers,
one row per TU, plus `bitfields.c::{dump,main}` ×2 because `bitfields_ms.c` `#include`s
`bitfields.c`. **(1)** key `low_body_index()` on the compiled TU as well as `(file, func)`, or drop
`<command line>` rows and dedupe. **(2)** then take a `wide`/`elf` inventory with
`--update-bank-low`: `tests/rir/lowerable-bodies.tsv` holds **only `self`/`macho`** today, so even
the *other* branch of the hedge would have fired.

**What was actually banked, and what was deliberately not.** Two figures moved, both attributed:
`wide.O0.lowerable.elf.bodies_pct` 15.4642 → 15.4066, and `sources_wide` 383 → 384 files with its
sha. **Nothing else.** A plain `--update-bank` was tried first and reverted: it rewrites the whole
`wide` block — `bodies` 4400 → 4738, `coverage`, `kept_coverage` and `residual` migrating to the
per-format schema, fifteen or so figures at once — of which exactly two had an attribution. That
is the blind re-bank this file warns about, arriving as a *convenience* rather than as a mistake,
which is the form it usually takes. `O1`–`O3` needed no change: they measure 15.457 against a
15.4242 floor, because four of the five lost bodies recover at `-O1`.

**`rir-coverage-census` also has a second, smaller cause that is this wave's**, and the two must not be
conflated: `corpus wide drifted: banked 383 file(s) … this run walked 384`, because
`tests/exec/types/ldouble_to_signed.c` joins the `wide` corpus. **Neither half was re-banked.**
The manifest half would be a legitimate attributed re-take; the ELF-floor half is a *regression*
the ratchet is there to catch, and re-banking it would erase what `eee6c1f2` moved before anyone
has said what moved in the compiler. This file's own rule — a ratchet re-banked without a reason
trains its readers to re-bank without looking — applies to the pair, so the pair stays red until
the ELF half is attributed.

**The `-O0` baseline was re-banked, and that one *is* fully attributed.** Adding any file to
`tests/exec` moves `ast/o0-baseline` on every target key, and this wave's `gen_cvt_ftoi` fix moves
object hashes as well. Re-taken across all thirteen keys from a cross build; the diff is exactly
(a) `aggregate_perm.c`'s hash on **`x86_64` and `x86_64-osx` only** — the two keys that have both
an 80-bit `long double` and the x86_64 backend, which is the precise footprint of the fix, and
`x86_64-win32`/`i386-win32` correctly do **not** move because Windows `long double` is 64-bit —
and (b) one new `ldouble_to_signed.c` row per key.

**Two traps found while doing it, both worth more than the re-bank.** *(1)* `tools/o0_ab.sh`'s
own header gives the re-bank command as
`C2_NO_EXTRA=1 O0_AB_BANK=1 O0_AB_GATES=1 tools/o0_ab.sh b all <dir>` and **it does not work** —
`-fopt-slice` is dev-gated and *refused rather than ignored*, so every key reports
`0 of 308 corpus files produced an object` and the harness correctly refuses to bank an empty
board. The gated half needs **`MCC_DEV=1`**, which the check cell passes and the documented bank
command omits. *(2)* **Four of the thirteen keys — `i386`, `arm`, `arm64`, `riscv64` — cannot be
measured on this host at all**: they need `vendor/gentoo-stage3-<arch>-glibc` sysroots that are
absent, and the tool refuses to measure them rather than reporting a plausible board from a fifth
of the corpus. They were left untouched, so **those four keys are now one row short** and must be
re-banked on a host that has the sysroots. `ast/o0-baseline` runs `measurable` and will not notice
here; a host with the sysroots will.

**~~N32. The standing inner-loop gate was red on the host its own rule was written for.~~ —
FOUND AND CLOSED 2026-08-13.** `ctest -R "^smoke/"` on x86_64-linux at `52e7e850`:
**`smoke/divergence` FAILs**, verified pre-existing by rebuilding a clean worktree at HEAD. Not a
compiler defect and not a value change — the bank and the classifier stopped agreeing:

| what moved | rows | why |
| --- | ---: | --- |
| `diverge-both` → `refs-disagree` | 10 | `5c71bc20` gave `smokerun` a third verdict class for *the two references disagree with each other*. Same rows, same values, new class: `bsweep.F16.FSCALE`, `csweep.{C32,C64}.CMULADD`, the four `shl.*` |
| new `diverge-one` | 4 | `bsweep.{F32,F64}.FSCALE`, unbanked here and **already triaged on the arm64 bank as its note 6** — of 8192 sweep points the differing ones are all NaN results differing in the sign bit alone, which IEEE 754 leaves unspecified; mcc agrees with gcc-15 on every row |

**The mechanism is the one this file keeps recording, and this is its sharpest instance.** The
arm64/macOS wave landed a *classifier* change and banked only `bails-arm64-macos.txt`, on the
stated and reasonable principle that *"x86_64-linux and Windows keep exactly the file and the
numbers they have"*. That principle is right for **values** and wrong for **classes**: keeping the
numbers is what made the x86_64 bank describe a classifier that no longer exists. **A
target-keyed bank does not make a harness change target-local.** Nobody ran smoke on this host
between the two, so the gate the standing rule points at was red for a day on the one machine the
rule was written for.

Re-banked with all fourteen moved figures attributed, per the standing rule, and the hand-written
header restored by hand with two new notes (13 and 14) — `smokerun --rebank` regenerates the data
rows and destroys the triage header, which the header itself warns about. Nothing else in the
bank moved: the data-row diff is exactly those 14 lines.

**~~N33. `slice/cref-oracle` produces zero bodies, zero slices and zero tuples on x86_64-linux.~~
— CLOSED 2026-08-13, and it is not a compiler defect. It is a wedged X server, and the cell said
"empty funnel" when it meant "the tool never returned".**

Both halves of the funnel are healthy. `MCC_ARENA_DUMP` produces a 6551-byte `arena.txt` for
`tests/gpu/cref/arith.c` at `-O1`, and `slicerun --arenas` on that same file reports
`bodies=3 slices=7 tuples=56`. The failing step is `slicerun` **never returning**:
`gpuconform.py` runs it with `timeout=180`, `run()` returns `("timeout", "")`, `slicerun_counts("")`
yields `{}`, and every counter defaults to zero. The 180 s the cell took was ten programs timing
out in parallel.

`slicerun` hangs in `probe_device()` → `mcc_gpu_init` → `vkCreateInstance`, at **0% CPU**, in
`poll` on a unix socket, with `libGLX_nvidia` and `libX11-xcb` mapped. **The host has
`DISPLAY=:1.0` while `XDG_SESSION_TYPE=tty`**: `/tmp/.X11-unix/X1` accepts connections and never
answers, and the NVIDIA ICD opens X inside instance creation with no timeout. `timeout 40
vulkaninfo --summary` hangs identically with no mcc code involved, and `env -u DISPLAY vulkaninfo`
returns instantly. mcc requests **zero** instance extensions — the path is pure compute and has no
use for a display at all.

**Two fixes landed, and the blast radius was wider than the row — twice.** *(1)* All **18**
`slice/*` cells drive `slicerun` and every one of them brings a Vulkan instance up, so one
unresponsive display server wedges the whole family — `slice/census` was found hung at 90 minutes
on this exact probe, which is also the *other* half of the `slice/census` mystery this file has
been carrying. **Scrubbing only `slice/*` was not enough**: `smoke/engines*` and `smoke/device*`
bring the same instance up through `eng-gpu`, and they went on hanging until the scrub covered
them too. It now covers **every** cell except the `wine`-labelled ones, floored at 9000, on the
principle the diagnosis established — mcc requests zero Vulkan instance extensions, so no cell in
this suite has any use for a display, and any cell that can be wedged by one is a cell whose
colour is a property of the machine. `slice/cref-oracle` goes from failing at 180 s to
passing at **8.4 s**. *(2)* `gpuconform.py` recorded `slicerun_rc == "timeout"` per program and
threw it away, so a total tool hang was reported in the exact words of an empty funnel — that is
what sent this investigation at arena capture for an hour. It now names the stalled programs and
fails on them explicitly. The `--work` path is absolutised at the same time, because
`build_and_run` writes `<work>/o_<tag>` and then execs it with `cwd=work`, so a relative `--work`
resolves twice and misclassifies every program as `norun`.

**The durable rule: a gate that can be wedged by an unrelated daemon is a gate whose colour is a
property of the machine.** The same shape as N26's `taskset`, N25's reference pair and N32's bank
— and the reason it took so long here is that the instrument reported the symptom in the
vocabulary of a different fault.

Headless, the cell is emphatic: `funnel bodies=65 slices=93 tuples=744 gpu-slices=93
dispatches=152`, `cref fragments=93 tuples=726`, `mismatch=0`.

The report as filed:

**~~N33 as originally filed.~~** New 2026-08-13, and **pre-existing** — verified by building a clean worktree at `52e7e850` and
re-running, where it fails identically. The cell qualifies all 10 programs against both oracles
and then reports `funnel bodies=0 slices=0 tuples=0 gpu-slices=0 dispatches=0`, so
`gpuconform_cref.cmake` fails on *"no oracle compiled a single emitted slice, so the CPU reference
was compared against nothing"* and on the 300-tuple floor. **It matters more than a red usually
does**: this is the adjudicator N7 and N6.10 both lean on, and *Implementation order by shared
surface* names it as the instrument that would validate a semantic-equivalence key offline. Its
floors are working exactly as designed — the cell refuses to report OK over nothing — so what is
open is why the slice funnel emits nothing here, not whether the cell is right to complain.

**Not root-caused, and one wrong lead is recorded so nobody re-walks it.** Running
`tools/gpuconform.py` by hand with a *relative* `--work` classifies all 10 programs out as
`norun`, because `build_and_run` puts the executable at `<work>/o_<tag>` and then runs it with
`cwd=work`, so a relative path is resolved twice. The ctest cell passes an absolute
`-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cref-oracle` and does not hit it. **That is a harness
sharp edge and not this red** — reproduce with an absolute `--work` or the failure mode changes
under you. Worth fixing on its own (`os.path.abspath` on `--work`), since a relative path silently
turns the whole corpus into `norun` and the cell would then fail for the wrong stated reason.

**~~N31. `stratsweep`'s "registry-disabled" baseline had silently stopped disabling anything.~~ —
FOUND AND CLOSED 2026-08-13, and it was red at `52e7e850` on x86_64-linux with no row anywhere in
this file.** `STRAT_NONE` is the sentinel every `stratsweep` mode compares against: a slot inside
`MCC_AST_STRAT_ORDER`'s range but past `AST_STRAT_COUNT`, so the runner skips it and the compile
runs *no* optimizer strategy. It was **25**. `AST_STRAT_COUNT_MAX` is **25**, and
`ast_strat_order`'s parser requires `val < AST_STRAT_COUNT_MAX` — so 25 was **rejected**, the
whole default order stayed in place, and the "disabled" baseline was an ordinary `-O2`.
Measured, not inferred: on `tests/exec/optimizer/loop_fusion.c`, `MCC_AST_STRAT_ORDER=25 -O2` is
**byte-identical to a plain `-O2`** and `=24` is not; same at `-O4` on `loop_interchange.c`.

**Two things this is worth more than its one-line fix.** *(1)* It was `sroa` taking row 23 that
broke it — the SRA/SROA wave moved the sentinel once, for strategy 23, and the second landing
walked past the same edge. The gap is now one slot wide, so a 25th strategy re-breaks it and
`stratsweep/check` says so; there is no headroom left and `AST_STRAT_COUNT_MAX` must move with
the next row. *(2)* **The cell that caught it was itself red and invisible.** `stratsweep/check`
does not match `^rir-`, `^flagsweep`, or any of the regexes the standing-reds sweeps used, and it
is not reached by `ctest -R "^smoke/"`. It was found by running the family. All 120 stratsweep
cells are green against the corrected baseline.

**N6. `L2` — wire the device into `mccjit_shutdown()`.** Unblocked as of 2026-08-10, with two
preconditions in the GPU landed section. One is a hazard *this wave created*: the quiesce now
unmaps the shared address space, so nothing may retain a `mcc_gpu_mem()` pointer across
shutdown. That hazard did not exist while the quiesce destroyed nothing.

1. ~~Re-arm the benignity probe~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
2. ~~`ast_eval_slice_globl`'s `VT_STRUCT` rejection~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
3. ~~Re-take the two owed deltas~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
4. ~~`ast_tco_run` cannot see through an `AST_If` op 5, so the tail-recursive two-exit `if` stays out of reach and…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
5. `SR_GLOB_MAX` is 4,096 in `slicerun` with no reset between arenas and no message — the
   same silent cap `ast_adump_intern` just made loud. Fails safe; a coverage cliff.
   **Measured, and it is not binding today**: raising it to 65,536 leaves the whole gcc
   torture census byte-identical, so fewer than 4,096 distinct global symbols reach
   `slicerun_reloc` over all 15,923 bodies. Its sibling `SR_GLOB_STRIDE` (4 KiB per symbol,
   ~~with no check that an object fits~~) ~~is the one that would fail *wrong* rather than safe~~ —
   a field or element past 4 KiB lands in the next symbol's key range and two distinct
   locations collapse to one live-in slot, which both executors and the oracle would then
   share and agree on. **The fail-wrong half is closed** (`8a70f8b7`): `slicerun_reloc` now
   compares the dumped extent against the stride and **refuses the slice** when it does not
   fit, which is the disposition the ladder already handles — a refusal is no result, not
   agreement. Raising the stride to 64 KiB is also byte-identical over gcc torture,
   so the hazard is unexercised there; it is not proven absent elsewhere. What remains is
   only the *coverage* half — a per-symbol range sized from the dumped extent would accept
   the over-wide symbols the guard now declines rather than merely failing safe on them —
   and it is no longer a correctness item. Slice family 55/55 with the guard in.
6. **Stop pricing global-data work on gcc c-torture.** `wt/globagg` measured −405 refused
   blocks there of which **384 are four macro-generated files**, and +39,203 indexed loads
   of which **39,092 are fourteen memory-op files**. The same fix on `src/*.c` is +2.82 pp of
   accepted nodes against +0.067 pp on torture. Every remaining gap in this area needs a
   hand-written denominator — `src/`, musl, qemu — before it is ranked.
7. ~~The `rir-coverage` lowerable bank was already stale before this wave: 0.0414pp of the 0.0671pp drift…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
7. ~~The funnel's largest named drop, `nslot < 1` at 352 blocks / 1.05%.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
8. **The JIT's 47.1% `NOT_BAKED` is not the callee refusal — 3110 of 3118 programs never
   reach a bake site.** `wt/bakewiden` closed the refusal half (−140 refused sites, −38
   programs, 0 new DIFFER) and proved the rest is upstream in `ast_func_end`. Attributing
   those 3110 across `rir_try_active`, `ast_replay_ok`, `faithful && !ast_fn_hole` and
   `ast_jit_want` is the measurement that ranks every remaining JIT-coverage item.
   **Sharpened 2026-08-10**: the funnel is *six* predicates, not four — the plan omits
   `!ast_func_has_labeladdr` (a term of `ast_opt_ok`) and the `embed_jit || OUTPUT_MEMORY`
   plus `!ast_jit_slot_taken` gate. **The 47.1% is not computed in the compiler at all**;
   `NOT_BAKED` is assigned in `tools/jitconform.py` and ~~there is no bake counter in `mcc`~~
   **— there is one as of 2026-08-11: `jit.baked` in `src/mccinv.h`, an `atexit` counter wired
   at the embed stash. It is not yet the *attributed* counter this item asks for (no named
   reason, no bytes), but it settles the raw figure: over 300 torture programs, `--embed-jit`
   bakes 587 of 1894 verdicted bodies at `-O0` and 589 at `-O1` — about 31%, measured in the
   compiler rather than inferred from a harness. See the emit-coverage section above.
   **The bake rate is strongly input-dependent and 31% is not a general figure**: the emit map
   puts it at **1550 of 3163 (49%)** on `src/mcc.c` and **41 of 299 (14%)** on
   `full_language.c`, both at `-O0 --embed-jit` — a 3.5× spread across three corpora, so this
   item's percentage must be quoted with its input attached.**
   Build the attributed version modelled on `rir_prod_why_name[]` — named reason, parallel
   count *and bytes*, `atexit` report — and split `ast_opt_ok` and `ast_jit_want` into their sub-terms or the
   largest bucket will be uninterpretable. Predicates 1, 3 and 5 have no signal today, which
   is why the measurement cannot be taken. Do **not** reuse `MCC_JIT_BAKE_WHY` (it already
   means per-site free text) and do not gate it with `ast_env_int`, which returns the default
   for any value ≤ 0.

   **Verified 2026-08-12, and every bake figure quoted above is against the wrong denominator.**
   `jit.baked` has three defects: it is incremented at the *stash* gate and
   `mccjit_embed_stash_leaf` returns without doing anything when `mccjit_intent_serialize` fails,
   so it **counts attempts, not acceptances** — which is precisely what `src/mccinv.h` documents
   it as; `mccjit_embed_stash_leaf` is **single-slot** (it frees and overwrites one blob), so it
   counts overwrites of a one-entry slot; and **the path that actually bakes into the binary is
   uncounted** — that is `mccjit_embed_note`, appending to `mccjit_embed_fns`, whose gate is
   exactly the six predicates this item asks to attribute. **So 587/1894, 589/1894, 1550/3163 and
   41/299 are all the leaf-stash path.** Wire the attributed counter at the `mccjit_embed_note`
   gate or it will restate the same wrong denominator with names attached. Two hazards found
   with it: `MCC_INV_MAX` is **32** and `mcc_inv_add` truncates **silently** past it — six reasons
   × (count + bytes) on top of the 12 keys already registered lands at or past the cap, losing
   the last-registered rows with no diagnostic, which is a second argument for the
   `rir_prod_why_name[]` model (own table, own report line) over more `mcc_inv_add` keys. And the
   `[inv]` stderr prefix is **already taken** by an incompatible grammar in `tools/slicerun.c`
   that `tools/fmt-census.py` parses positionally — a `k=v` token there raises an uncaught
   `ValueError`. Use a new prefix.

   Confirmed present and unchanged: `ast_jit_slot_taken` exists (this item doubted it), and two
   of `ast_jit_want`'s three sub-terms already print under `MCC_JIT_VERBOSE`, so two of the six
   reasons need a counter and no new logic.
9. ~~Cluster L's next link is `L2′(ii)`/`(iii)`, both in `src/mccgpu.c` — clear `mcc_gpu.ok` on…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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

## Implementation order by shared surface — re-verified 2026-08-12 (second pass)

> **This supersedes the index below it.** Eight read-only sweeps re-derived every cluster
> against the tree rather than against the rows that filed them, and the ordering changed
> because several of the claimed couplings do not exist. The rule is unchanged — clusters
> ordered by how much implementation surface they genuinely share, most first — but a coupling
> is listed here only with the symbol that proves it.
>
> **Couplings deleted, because verification refuted them:**
> - **N19 does not force the `emit-map.py` refactor N6.8 wants.** N6.8 arrives on the `[inv]`
>   channel, which `run()` already parses generically (`k, _, v = kv.partition("=")`), so it
>   costs zero tool changes; N19 lives entirely in `find_anchors`/the tag dispatch/the byte
>   arithmetic. **Disjoint edit sets.**
> - **`rir_fcrec[]` is not the consume-by-fit exemplar.** Its forward-skip keys on
>   `rir_fcrec_cplx`, one byte, and its key is a `memcmp` *equality* relation; the size/align
>   problem needs a `>=` *fit* relation that deliberately accepts an over-large entry. Copy
>   `ast_locrec_take`.
> - **The `VT_BTYPE` bit budget does not tie the type cluster to wide256.** `VT_BTYPE` is
>   `0x001f` with **16 of 32 encodings used**, and `__int256`, `_BitInt`, `__m512` and
>   `_Complex _Float16` consume **zero** encodings each — they are all `VT_STRUCT` + a `Sym`
>   attr bit. The scarce-field premise is false. What actually ties them is `MCC_MAX_ALIGN`.
> - **The four depth-inline rows are not in `ast_func_end`.** They are `src/slice_inline.h`.

**Rank 1 — `src/mccopt.h`, the highest-fan-out file in the tree.** One `MCC_OPT_ROW` added or
moved fans out to `levelbench.tsv`, `cover3.txt`, `defstate.txt`, a tab-anchored `sed` in
`flagsweep.sh`, a `string(REGEX MATCHALL)` in `CMakeLists.txt`, and 118 `flagsweep-exec` cells.
**This is not hypothetical — SROA just did it and `cover3-verify` is the casualty.** Items: the
live `cover3` red, sweep 4 (`if-conversion-abs`), `filed-14` (`defstate` cannot distinguish "off
by default" from "does not exist" — and it is **15** rows, not 13), the `accept` floor, and sweep
28. Do the regeneration and the floors together; they are one read of one file.

**Rank 2 — `ast_func_end`, `src/mccast.c` (~700 lines).** Sweep row 17 (the pre-inline copy left
in `.text`; `ast_reemit` appends and never rewinds, and `AstReemitFn` does not even record where
the orphan was — `AstBaselineFn` is the shape to copy), sweep row 27 (five arena mutators with no
`AST_SG_*` bit, so `ast_reemit_with_gates` cannot reproduce the tree it is handed), `rf-1`, and
**N6.8's counters, which are wired in this same window**. Rows 17 and `rf-1` are one defect at two
altitudes: bytes in `.text` that no size metric attributes. **Trap:** `AST_GATE_BITS` is 48 and
bits run to 41; row 27's five land at 46, inside budget — a sixth wave is not, and a bit above 47
silently aliases two search keys on disk.

**Rank 3 — `tools/rir-coverage.py`'s check block + `tests/rir/coverage-bank.json`.** Sweep 22
(the `wide` denominator is an unmanifested `os.walk`; the bank has no `sources` key), sweep 23
(three independent empty-input paths — `--check-gap-dir`, `--check-low-dir`, `--nofb-probe` —
none with a floor; gap fixtures cover 3 of 18 classes and low fixtures 6 of 7), `ci-4`, `rf-4/5`,
the `pe` floors (unchanged since 2026-08-07 and now ~0.58 pp below where `elf` lands), and
**`bl-7`'s re-bank**. All of them route through one predicate, `unbanked_host` — change it once.
**Ordering constraint that survives verification:** `rf-4/5` forecloses `ci-4`'s "raise `--tol`"
option; run `--classify` first, `--tol` never before. **Host:** on macOS `host_objfmt` returns
`macho`, which is unbanked, so this entire cluster asserts nothing here.

**Rank 4 — the record/replay entry unification (`mcc_rec_take`).** N2,
`rir-locreplay-sizecheck`, `rir-locrec-skip-byfit`, plus the three no-validation consumers this
file lists elsewhere (`ir_cap_fconst_take`, `ir_cap_pred`, `cst_inc_tmpl`) and the **live
`ast_reemit` cursor hazard** — `ast_search_emit_size` resets `ast_locrec_i` while only
`ast_func_begin`/`ast_teardown` reset `ast_locrec_n`, and `rir_prod_replay_begin` does the same
for all four cursors. One entry type `{kind, size, align, pos, nc, owner}` and one take-by-fit
serves all of them. **`SR_GLOB_STRIDE` is explicitly not served by it — do not force it in.**
`phase-f-arena-fidelity` is an index label over one unscoped line and is not a peer of the other
three; scope it before estimating it.

**Rank 5 — `src/mccjit_embed.c`.** **N24** (root-caused: one `strcmp` in `mccjit_bind_apply`),
`jit-poolmax-64` (one line to make a silent clamp loud), `embedjit-warn-under-w` (the honest
one-line fix loses the `warning:` prefix and the `-Werror` interaction; doing it properly needs a
note class `-w` cannot silence), `jit-lazy-build-fail`, `jit-teardown-unbounded`, **N6.8's
attempt-vs-accept split** (`stash_leaf` at one site, `embed_note` at the other), and **N6/`L2`**,
which wires the device into `mccjit_shutdown()`. **Read the atexit-ordering comment in
`src/mccast.c` before touching teardown** — it warns in-tree that registering an extra handler
reorders `ast_ladder_gpu_report`'s device teardown into a call through an unmapped page, and
`L2` adds exactly that second handler. `jit-teardown-unbounded`'s endorsed fix is the `tick`
split, i.e. `L2′`/S7b — large, not small. **`jit-lazy-build-fail` shares nothing with the rest
and is host-neutral, which makes it the safest start** — but its reachability claim needs
correcting: `tools/embed-jit-smoke.py` is Windows/PE-gated, so lift its 8-line program out rather
than running the tool.

**Rank 6 — `src/ast_eval_slice.h`.** Sweep row 25 (the non-LVAL local `Ref`; the file's own
comment says "OPEN QUESTION, not resolved here", and `ast_dep_decode` is the answered sibling to
copy), **N7**, N6.10's window (`ast_eval_slice_livein_ext`, `mcc_slice_frame_from_ast`), the E1
refusal-site row, and `mcc_slice_inl_body_ok`, which calls `ast_eval_slice_kind_ok` on every leaf.
**Do 25 before N7** — a wrong `Ref` decode is precisely the fault class N7 proves the current
self-comparison cannot detect. The naive fix for 25 was reverted for a real reason: `spvgate` and
`slicerun` write plain `VT_LOCAL` for value references, so a blanket refusal breaks the harnesses
first.

**Rank 7 — `MCC_MAX_ALIGN`: one decision, five call sites, and the real cross-cluster tie.**
`vec32-align` (a 32-byte `__m256` is laid at 16, silently ignoring its own header's
`__aligned__(32)`), `types-m512-avx` (needs 64), `mk_wide256_type`, the bare `aligned` default,
and the varargs `pr92904` row. Raising it is one constant per arch and an **ABI break against
gcc/clang objects**, which is why it is a decision. **Decide it before anything downstream** —
both `__m512` and `__int256`'s own layout sit under it.

**Rank 8 — `parse_btype`, `src/mccgen.c`.** `bf16-abi` (token exists, zero code behind it;
`is_float_abi` special-cases `VT_FLOAT16` in a way that would be wrong for `__bf16`),
`types-complex-float16` (a deliberate one-line refusal inside working machinery — delete and
chase), `types-float128` (**cheapest of the group by substrate**: `runtime/lib/float128.c` is a
complete soft-quad already in tree and `default_debug[19]` holds a reserved sentinel, so the work
is front-end only — but decide first whether it collides with `long double`, which is already
16 bytes on arm64/riscv64), `types-bitint` (C23-mandatory, large, and needs a per-declaration
width that no bit field can carry), ~~`int128-signedness`~~ (**CLOSED 2026-08-13** — it was
dead-code cleanup only, and all three files are done: `__mcc_ov_disp` had zero call sites because
`__builtin_*_overflow` expands to `__mcc_ov_gen`'s inline path and never reaches the out-of-line
helpers, so the header declarations, the `runtime/lib/builtin.c` macros and the
`runtime/lib/int128.c` wrappers all went. It **leaves this rank** — the switch it was grouped
under is shared, but this row no longer has a switch arm to add). The switch is shared; the bit
budget is not scarce.

**Rank 9 — wide256.** `int256-float-conv`, `int256-literal-suffix` (`CValue.q` already carries
four limbs, so the work is confined to the lexer's accumulator), `int256-dwarf`,
`int256-inline-arith`, `vec32-align`. **`int256-inline-arith` has a hazard this file does not
name:** the arena is 128-bit-wide only — `ast_sv_hi` returns 0 for `VT_STRUCT` and `w2`/`w3` have
nowhere to go — which is safe today only because `ast_bad_type` excludes `VT_STRUCT` from capture.
Inlining puts 256-bit values where the capture layer can see them, so it can silently break arena
fidelity. **Blocking correction: `tests/wide256/` is not self-oracled.** Its oracle is
`#include <gmp.h>` and the CMake probe passes no `-I/-L`, so on this Mac with Homebrew GMP it
**skips 77 and has never run in `cmake-macos` at all**. A ~6-line CMake fix is the true unblocker
and is strictly cheaper than the item it unblocks.

**Rank 10 — `gen_cvt_ftoi`, isolated by verification as well as by claim.** Items 23 and 24 are
opposite arms of one `t != VT_INT` in a 35-line function and must be two changes: 24 narrows a
`long double` through `double` before truncating; 23 never decodes signedness at all (the word
`VT_UNSIGNED` does not appear in the function). arm64 and riscv64 both decode the two dimensions
independently and are the reference. **x86_64-only, and this host cannot tell `diverge-one` from
`diverge-both`**, which is the distinction that separates 23 (QoI) from 24 (conformance).

**Ordered by value, not by surface — the things that share nothing and are worth doing anyway:**
~~the `cover3` regeneration~~ (done 2026-08-12), ~~the three vacuity floors~~ (**five**, done
2026-08-13), ~~the ladder points/secs split~~ (done 2026-08-13), ~~Metal's `mcc_gpu_mem()` failing
closed~~ (**tried and reverted — see N27; the contract is the host window, not kernel
addressability**), and ~~`slice/cref-oracle`'s single-family fallback~~ (done 2026-08-13).
**All five are now closed or refuted.** The last one was the precise item this line was reaching
for: `MCC_DIFF3_SAME_FAMILY` was computed by `mcc_compiler_family` and then read by nothing, so a
same-family pair was *named* at configure time and still *registered* as a cross oracle. The five
`slice/cref-oracle*` cells now skip with a reason when the pair is one family; verified by forcing
the branch with `-DMCC_DIFF3_GCC=$(command -v clang)`, and unchanged on a host with a real GNU
gcc.

## Implementation plans — derived and part-executed 2026-08-12

> Three read-only sweeps turned the top clusters into ordered edit lists. What follows is the
> executable form: **each row is a task**, smallest-first within its cluster, with the
> dependency and the host constraint stated. Rows marked **DONE** landed in this wave.
>
> **Two rows below were investigated and deliberately NOT changed.** Both looked like cheap
> wins and both have a hazard that only shows up in the code. They are written up as tasks
> rather than as fixes because doing them blind is worse than leaving them.

### Cluster A — record/replay entry unification (was Rank 4)

**The priority changed once `rir_c2_active` was traced.** It is *not* dev-build-only. Besides
the `#if MCC_REPLAY_IR_C2` arm (off in every build dir here), `rir_prod_replay_begin()` sets it,
and that is called from `ast_func_end` under `if (ast_rir_used)` with **no compile gate**. Since
`ast_replay_env` is `optimize >= 1 || embed_jit || …`, **every `-O1`+ compile in a stock build
runs with `rir_c2_active == 1`**, takes `ast_alloc_loc`'s `rir_loc_replay` early return, and
never reaches `ast_locrec_take`. And when `rir_prod_env` is on the parser's own arena is freed,
so the C2 arm is the *only* first replay at `-O1`+. **The `dd80e4fa` bypass is the live defect;
N2's record widening is its prerequisite**, not the other way round.

| # | edit | size | depends | host |
| --- | --- | --- | --- | --- |
| ~~A1~~ | ~~`ast_locrec_skip` takes `(size, align)`~~ **DONE 2026-08-12** (`8a92ee01`) | small | — | any |
| ~~A2~~ | ~~hoist the record reset out of `rir_try_active`~~ **DONE 2026-08-12**. The hazard was held shut by three separate gates rather than by the reset | 2 lines | — | any |
| ~~A3~~ | ~~widen `rir_locrec` with `sz`/`al` + `rir_locrec_min`~~ **DONE 2026-08-12**, resync then fit | small | — | any |
| ~~A4~~ | ~~the C2-bypass fix~~ **DONE 2026-08-12.** The replay now feeds the fit check and falls back to the frontier allocator below `rir_locrec_min`, not a bare bump. Validated on the **whole exec corpus: 840 pass / 0 fail** over 280 programs at `-O1`/`-O2`/`-O3`, plus ast+smoke 141/141 | small | A3 | any |
| ~~A5~~ | ~~widen `rir_slotrec`; `rir_hook_slot_replay/_record` take `(size, align)`~~ **DONE, and verified reachable on Linux 2026-08-13.** Both hooks take `(size, align)` and `rir_slotrec_sz/_al` exist. **The host annotation was right and is now discharged in part**: the arm64 site at `src/arch/arm64/arm64-gen.c` is inside `#if !defined(MCC_TARGET_MACHO)`, so it is compiled out on the machine A5 was written on. A cross build here compiles it, and upstream's own `MCC_RIR_REC_FORCE_MISS=1` injection proves it is *reached*: an HFA `va_arg` subject compiled with `mcc-arm64` moves its object under the injection and the identical subject on `mcc-x86_64` does **not**. **Still owed: execution.** `qemu-aarch64` is installed but `vendor/gentoo-stage3-arm64-glibc` is not, so nothing arm64 can be linked or run here | small | — | arm64 site is `#if !defined(MCC_TARGET_MACHO)` — **Linux** |
| ~~A6~~ | ~~widen `rir_tvrec` **and** add its missing `nc[]` resync~~ **BOTH DONE**; the widening earlier, the `nc[]` resync 2026-08-13. `rir_tvrec` now records `nocode_wanted` like the other three streams and passes it to `rir_rec_take`, so the literal `NULL` A7 exposed at the shared call is gone and all four streams gate their cursor advance the same way. **The row's prediction is refuted and that is the interesting part** — see below | small | A5 shape | any |
| ~~A7~~ | ~~the `mcc_rec_take` unification~~ **DONE 2026-08-13**, scoped to the three streams that share semantics; `rir_fcrec` stays separate because its key is a `memcmp` equality relation, not a `>=` fit relation. `rir_tvrec`'s missing `nc` array is now visible as a `NULL` at the call rather than buried in a fourth copy of the loop. **840/0 on the full exec corpus.** See the coverage hole it exposed, below | medium | A3/A5/A6 | any |

**A6's second half is byte-neutral, and the row said it would not be.** The advice to land it
separately was taken and the measurement was worth taking: *"the fit skip is byte-neutral and the
`nc` resync is not"* is **false**. A/B'd against a clean-HEAD control build over the whole
`tests/exec` corpus — **1,199 object pairs, 0 moved**: 598 at `-O1`/`-O2`, 598 at `-O0`/`-O4`, and
the three `src/*.c` TUs that compile standalone at `-O0`/`-O2`/`-O4`. It is byte-neutral **under
the failure arm too**: 598 more pairs with `MCC_RIR_REC_FORCE_MISS=1` on both sides, 0 moved.
Smoke is 12/12 with no bail-bank movement, and `rir/rec-miss`, `rir/drop-ratchet` and
`ast/o0-baseline` are green.

**So why land it.** Byte-neutral is not the same as inert: the `nc` term is what stops the cursor
advancing past an entry recorded under `nocode_wanted`, and the other three streams have gated on
it since A3. What the measurement establishes is that **no reachable input currently distinguishes
the two**, which makes this a latent divergence closed rather than a bug fixed — and it means the
next reader of `rir_rec_take` no longer has to explain why one of its four callers passes `NULL`.
The honest summary is that the row was right to demand a separate commit and wrong about what the
commit would show.

**A4 in detail, because the obvious fix is wrong.** The two arms of `ast_alloc_loc` consume two
*different* streams with two *different* cursors: `rir_locrec` (resynced against `ind`) and
`ast_locrec` (cursor order). Running `ast_locrec_take` first under `rir_c2_active` would consume
`ast_locrec` entries while `rir_locrec_i` never advances, desynchronising every later
`ast_alloc_loc` in the body. The arms are not even mutually reachable: `rir_prod_replay_begin`
sets `ast_replaying = 0; ir_cap_replaying = 1;`, so the second arm is dead on that path — swapping
the tests would switch streams, not reorder them. **The replay must feed the fit check**, and the
fallback must be the frontier allocator, not a bare bump: on the replay path `loc` is assigned
from the record, so the record's unconsumed remainder lies *below* it and a bare bump allocates
into an offset the replay is about to hand out. `ast_alloc_temp_loc`'s guard is false during the
C2 replay, so extract its frontier body into an unguarded helper and call that.

**Regression subject:** extend `tests/exec/structs_unions/inline_sret_locrec.c` rather than
adding a file — its golden and per-target `o0-baseline` hashes already exist. It needs all four
of: a dropped allocation request, a size cliff right after it, a live neighbour the overlap
corrupts, and `-O1`+ with a graftable inline. **Land the subject red first**; a cell that has
never failed is the thing this whole cluster is about.

**The coverage hole A7 exposed, which is worth more than the de-duplication.**

Writing the shared helper I gave it an index return and made failure `return 0` — but index 0 is
a legitimate hit, so a *failed* take handed back `val[0]`, the wrong frame offset, whenever the
record was exhausted or nothing fit. **Nothing caught it.** ast + smoke stayed 141/141 and 102
`tests/exec` programs stayed green with the bug in place. I found it by re-reading the code, not
by running anything.

The reason is measurable: instrumenting the take with a hit/miss counter gives **811 takes and
0 misses over 42 `tests/exec` files at `-O2`**. The no-fit path never fires on this corpus, so
neither the sentinel nor anything else on that branch is exercised.

**That has a consequence beyond A7: A4's frontier fallback is on the same branch.** The
`rir_loc_replay` failure arm — where the C2 path now allocates below `rir_locrec_min` instead of
bare-bumping into an offset the replay is about to hand out — is reached exactly when a take
misses.

**CLOSED 2026-08-13, and the fixture is an injection rather than a subject.** Eight candidates
built to force a miss — a large inlined array, an inlined `sret` struct, nested inlines, complex
temporaries, a VLA mixed with fixed locals, an `sret` chain, six graft sites, and `-O1 -finline` —
produced **0 misses between them**. The branch looks structurally unreachable on the prod path:
the record is built by the same `ast_alloc_loc` sequence the replay re-walks, so a fitting entry
is always there. Hunting harder for a natural subject is the wrong instrument.

`MCC_RIR_REC_FORCE_MISS=1` forces every take to miss, and **the whole corpus still computes the
right answers** — 102 of 102 programs agree with `-O0` at `-O2` with it on. That is the fallback
under test, and `rir/rec-miss` now pins it: six subjects, each compared against `-O0`, plus an
anti-vacuity check that the injection **moved at least one object**, because a hook that changes
nothing would make the cell green while testing the same path as every other run. It currently
reports 6 subjects and 2 objects moved.

**The honest reading is that A4/A5/A6's failure arm is fail-safe defensive code**, not a live
path — but it is now correct-by-test rather than correct-by-argument, and A7's `return 0`
sentinel bug would have been caught by this cell.

### Cluster B — `ast_func_end` (was Rank 2)

| # | edit | size | depends | host |
| --- | --- | --- | --- | --- |
| ~~B1~~ | ~~`ast_search_emit_size` leaks `.data`/`.rodata`~~ **DONE 2026-08-13, and the hazard I filed against it was wrong** — see below | 4 lines | — | any |
| ~~B2~~ | ~~`struct AstReemitFn` gains `body_ind/body_len/reloc0/rel_len`~~ **DONE 2026-08-12.** Needed `ast_body_ind_sv`/`ast_reloc0_sv` hoisted above their first use — they were declared ~13k lines below `ast_reemit_retain` in the fragment's translation order | small | — | any |
| ~~B3~~ | ~~five `AST_SG_*` bits at 42–46~~ **DONE 2026-08-12**. Bit **47 is now the last disk-safe one** — `AST_GATE_BITS` is 48 and the memo packs its magic above it | small | — | any |
| ~~B4~~ | ~~`ast_jit_submit_aot` passes `ast_search_gates_now()`~~ **DONE 2026-08-12**. The zero was **not inert** — `have_override && override_mask` is false for 0, so AOT submissions were recompiled by `ast_reemit_extern` under ambient gates and `warm_gates` stayed 0, disabling warm start for them. Two no-ops from one literal | 3 lines | — | arm64 |
| ~~B5~~ | ~~add the five to `ast_search_gates_now`/`_set`~~ **DONE 2026-08-12**, and **B6 with it**: they are deliberately absent from `ast_search_searchable`, because none of the five is idempotent on its own output and the strat cycle never invokes them, so letting the search flip them would ask it to re-run non-idempotent transforms | small | B3 | any |
| ~~B6~~ | ~~orphan report~~ **DONE 2026-08-12** as `ast.orphan_fn` / `_bytes` / **`_relbytes`** — the third was renamed from `_relocs` on noticing it holds reloc *bytes*, not a count. **First measurement: 1 orphaned body / 48 bytes across 150 `tests/exec` files at `-O2`**, so row 17's 27-function 52 KB figure is a self-host property, not a corpus one | small | B2 | any |
| B7 | **`ast_reemit` emits no `mcc_debug_funcend` and no FDE**, so after a forward-inline re-emit `-g` and `.eh_frame` describe the *dead* range and the live body has neither. **Sized 2026-08-13 and it is a feature, not a fix:** the normal path calls `mcc_debug_funcend(mcc_state, ind - func_ind)` from `gen_function`, and the line records it closes were emitted *by the parser* against the original range. `ast_reemit` replays an arena, and **the arena carries no per-node line information** — so correcting only the FDE range would leave the line table pointing at dead code, and correcting the line table means teaching the arena to carry line numbers. Same shape as N30's runtime half. B6's `ast.orphan_*` counters already name the event | large | B6 | assert on Linux |
| ~~B8~~ | ~~`so_fn_sizes` has no Mach-O arm~~ **DONE 2026-08-12.** `so_macho_fn_sizes` walks `LC_SYMTAB` and `LC_SEGMENT_64`, keeps `N_SECT` symbols in `__text` at or above its address, sorts by address and takes next-symbol deltas with the section end closing the last one — Mach-O has no `st_size`, so the size must be derived. Verified against `nm`: `alpha` 56 = 0x38, `beta` 60 = 0x3c, `main` 92 = 0x5c, exactly the banked deltas. **`rf-1` is now measurable on this host for the first time** | medium | — | macOS only |

**Row 27's real question, answered: the JIT should refuse, never re-run.** The five mutators
rewrite `ast_cur` **in place**, and both JIT ingestion points run downstream of that and pass the
same `ast_cur` — so re-running would double-apply, and none of the five is idempotent on its own
output. What is actually missing is env state, not transforms: `ast_search_gates_set` writes 34
flags and the five mutator envs are outside it, and `ast_vlat_env` is read on the *replay* path,
so a recompile with it in a different state emits different code from the same tree. Also
`ast_math_inline_prepass_env` is `MCC_OPT_TLS`, so route the mask through the intent blob's
existing `warm_gates` rather than ambient statics. **Bit 47 is the last disk-safe bit** — the memo
packs its magic above `AST_GATE_BITS`, so a bit at 48+ silently collides two gate sets on one
disk key with no diagnostic.

**Also found: the literal zero mask is not inert.** `mccjit_embed.c`'s dispatch tests
`have_override && override_mask`, which is **false** for 0, so an AOT submission recompiles under
*ambient* gates via `ast_reemit_extern`, and `warm_gates == 0` means the warm-start path never
fires for AOT-submitted functions at all. B4 turns two silent no-ops back on.

### Cluster C — `tools/rir-coverage.py` (was Rank 3)

| # | edit | size | depends | host |
| --- | --- | --- | --- | --- |
| ~~C1~~ | ~~the loud-skip question~~ **RESOLVED 2026-08-12 by removing the question.** Gave `residual` and `kept_coverage` a per-format schema (reader *and* writer), then banked `macho`. The cell went **2 enforced / 3 skipped → 20 enforced / 0 skipped** on this host. No policy call needed: the skips existed because the values were banked flat, not because partial enforcement was intended | small | — | any |
| ~~C2~~ | ~~`sources` manifest floor~~ **DONE 2026-08-12.** Banked as `sources_{self,exec,wide}` = `{n, sha}` — **host-independent, it is the tree not the host**, so unlike the lowerable floors it needs no per-format schema. Verified by hiding one `tests/exec` file: fails naming 307→306 and the sha change. **Trap hit while doing it:** running `--update-bank` to produce the manifest also wrote a whole new `exec` ratchet entry with macho-derived coverage into the host-less schema; reverted, and the manifests computed directly instead | small | — | any |
| ~~C3~~ | ~~gap-dir floor~~ **DONE 2026-08-12.** Fails on empty input (verified both directions) and now prints the honest denominator: *3 class(es) of 18 covered* | small | — | any |
| ~~C4~~ | ~~low-dir floor~~ **DONE 2026-08-12.** Same shape; prints *7 blocker class(es) of 7 covered* and names any class lacking a fixture. The LOWCLS↔compiler parity check is **not** done and stays open | small | — | any |
| ~~C5~~ | ~~write `tests/rir/low/reg.c`~~ **DONE 2026-08-12** — low fixtures are now **7 of 7** classes. See the caveat below | ~8 lines | — | any |
| C6 | nofb floor keyed `(corpus, fmt, opt)`; also fix `--update-bank`'s union merge, which can only grow the list | medium | — | values need ELF |
| C7 | `pe` staleness marker — `pe` floors frozen at `c954b223` while `elf` moved ~0.58 pp, and nothing says so | small | — | values need PE |

**C5's caveat, and it is a real weakness of the fixture that landed.** `reg` could not be
isolated. Every shape that reproduces it here is a VLA — `int f(int n){ int v[n]; return n; }`
reports `[blockers opaque,reg]`, and that is the *fewest* classes any candidate produced. The
obvious spellings do **not** reach it: `(a<b)&&(b<c)`, `a<b ? a : b`, `int t=(a<b)`, a `register`
variable and a `setjmp` return all lower cleanly with no blocker at all, because the comparison
is materialised before the arena records it. So `reg.c` and `opaque.c` now share one mechanism —
**a change to VLA lowering breaks both fixtures at once, and neither would isolate which class
regressed.** The class is real and occurs naturally (`bounds/bound_signal.c` `reg=3`,
`bounds/bound_setjmp.c` `reg=2`, `vla/basic.c` `reg=2`), so a non-VLA reduction exists in the
corpus; finding it is the follow-up. Until then the honest reading of "7 of 7 classes covered" is
**6 mechanisms, not 7**.

**Two corrections to fold in:** `CORPUS_DEFS` is already `["MCC_DIAG","MCC_EMBED_JIT"]` — closed,
do not re-file. And **`asm` and `regdangle` have no producing site anywhere in `src/`** — they
exist only in the name table, so the honest gap denominator is **16, not 18**, and the headline
is *3 of 16*. `reg`, `posterr`, `bytes`/`len` are the cheap fixtures; `ovf`/`unbal`/`invalid`/
`unsafe` should be deferred (capacity constants and "needs a bug" respectively).

### The two rows that were investigated and deliberately left alone

**~~B1 — the `ast_fconst_n` aliasing hazard.~~ — REFUTED 2026-08-13, and the restore landed.**
The row claimed rewinding `.rodata` without also rewinding `ast_fconst_n` would leave pool
entries pointing at space the next constant reuses — two constants on one offset, a miscompile
generator. **That cannot happen.** `ast_fconst_record` opens with

```
if (!ast_active || ast_replaying)
        return;
```

and `ast_search_emit_size` sets `ast_replaying = 1` before `ast_replay_body`, so **the pool does
not grow during a trial at all**. The two recorders called ahead of that guard are covered too:
`rir_hook_fconst_record` gates on `rir_capture_live()`, which is
`rir_active && !ast_replaying && !ir_cap_replaying`, and advances `rir_fcrec_n` only inside it.
Nothing retains an offset into the bytes a trial allocates, so restoring both section cursors is
safe.

Fixed: `data_section->data_offset` and `rodata_section->data_offset` are restored beside
`ast_cur`, before `ast_scratch_measure_exit`. 270/270 on ast + smoke + optfire.

**Landed as correctness-by-construction, not as a measured win, and that distinction is the
honest one.** A save-then-never-restore is a leak by inspection, but it could not be shown
firing: with `-fopt-search-emit-size` on at `-O13`, objects are **byte-identical** before and
after on a subject built to materialise several distinct float constants. So either the trial
replay reuses the pool rather than allocating, or this subject does not reach the path. **The
open question is now reachability, not safety** — and note `ast_search_emit_size` runs only under
`-fopt-search-emit-size`, which is `MCC_OPTD_OFF`, so nothing ships through it today.

**C1 — `rir-coverage` reports Passed on this host while skipping its three most valuable
comparisons.** Verified live: 12 skips (`kept_coverage`, `.text` residual and the whole lowerable
ratchet, at each of `-O0`–`-O3`), and the cell still returns 0 because `checked` is non-empty —
capture and modelled coverage are enforced everywhere. **This is a disclosed partial, not a
silent green**: the PASS line itself prints `N skipped as host-specific`, and the `SKIP` lines
name every one. So flipping it to `return 77` is a *policy* decision about whether partial
enforcement counts as a pass, and it would discard the checks that do run. **The substantive fix
is different**: give `arena` a per-format schema for `residual` and `kept_coverage` (reusing
`low_floor`'s "legacy flat reads as elf" rule), then bank `macho` — at which point the ratchet
arms here for real. Banking `macho` *before* that schema exists would arm two comparisons against
ELF's values and produce a **false red** — `residual` is banked 0 while this host legitimately
measures 120, because `mcc_tlv_thunk` is 120 bytes of raw asm with no C body on Darwin.


## Implementation order by shared surface — SUPERSEDED, indexed 2026-08-12 (first pass)

> ~~The first-pass index.~~ **Superseded the same day by the re-verified ranking above**, which
> refutes four of its couplings by name. Moved in full to [`docs/ARCHIVED.md`](ARCHIVED.md);
> its per-item file and symbol lists are still the fastest way to find code.

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

Measured per level on the smoke subject: `-O0` 22 dark (no strategy runs at `-O0` at all —
**independently confirmed 2026-08-11 from the other side: `MCC_INV=1` shows `-O0` records 0 of
1932 bodies and increments no `ast.*` counter at all, so there is no arena for a strategy to
run on** — and a third time by the emit map, where `-O0` writes each byte exactly once,
1,662,057 emitted for 1,662,057 surviving on `src/mcc.c`, against 2.71× at `-O1`),
`-O1` 12, `-O2`/`-O3` 9, `-O4` **1** (before the `bfold` shape landed; now 0 of 22), and
**`-O13` 13** — `ivsr, narrow, bfold, range, bf, divmagic, pre, ltemp, inline, abs, tco, licm,
reassoc`. That is the state the whole subject was in before `scases.h`, at the one level
nothing watches.

Every objection to watching it is gone: an `-O13` compile of the subject is ~~**~2.7 s**~~
**47 s as re-measured 2026-08-11 — the 2.7 s predates `scases.h`**, it is
**bit-deterministic** (882 TSV rows and 112 `fallback` rows across search budgets of 100, 1000
and 5000 ms and three repeats), and it already agrees with `-O0`–`-O4` on the value digest with
`failures=0`. **~~`smokerun --max-level 13` is currently a silent no-op~~ — fixed 2026-08-11, it now
refuses and names `--strat-dark`**: `smk_maxlevel()` walks
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

- ~~The device arm computes a census and never ratchets it.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
- ~~Row-count-driven ctest registration.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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

`tests/optfire/leveldiff-known.txt` started at fourteen rows and is now **ten**: the
two computed-`goto` rows went stale and were dropped, `20020720-1.c` and `20041114-1.c`
followed them on 2026-08-13 for the same reason, and every surviving row's range
reads `1-4` rather than `1-12`. Three are `link_error()`/`link_failure()`
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
before consolidation folded that band away. `return-addr.c` is the tenth row and is not
a defect: it prints the addresses of its own locals, so its stdout can never agree across
levels.

**Four rows have now gone stale the same way, which makes the pattern the point.** The two
computed-`goto` rows, then `20020720-1.c` (`fabs(x) < 0.0` not folded to false) and
`20041114-1.c` (a `||` of a signed test and an unsigned wrap test not folded to 1) on 2026-08-13.
All four were group (a) — *missed optimizations mcc later learned* — and in every case the
optimizer improved and nobody dropped the row. **Dropping does not weaken the ratchet**:
`leveldiff.py` fails on any divergence *not* in the table, so a regression comes straight back as
a `NEW` unknown. And the rows must go whole, not be trimmed to a narrower level range —
`parse_known` does `int(chunk)` per comma chunk and raises on an empty level spec.

A row that stops diverging fails the cell as loudly as a new divergence, so the table
cannot rot into lost coverage.

**Instrument note, 2026-08-13, and it belongs to `docs/refs` rather than here but has nowhere
else to live.** `tools/docref-lint.py` defaults `--min-refs` to **600** while `docs/refs`
registers it at **440**, and the tree currently resolves **510**. So running the lint by hand
prints a failure and exits 1 on a tree whose gate is green — the tool's own default has been
above the real count since the doc-trimming waves moved citations into
[`docs/ARCHIVED.md`](ARCHIVED.md). Not a defect and not a red; recorded because "I ran the lint
and it failed" is otherwise an hour of re-diagnosis.

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

**Stage W3 — a COFF object writer. ~~~900–1,400 lines.~~ — first cut LANDED 2026-08-13,
flag-gated.** `coff_output_obj` now exists in `src/objfmt/mccpe.c` and emits a Microsoft COFF
object (`IMAGE_FILE_HEADER` + section table + section-definition AUX symbols + `MccCoffRel`
records) when `-c` is given `-Wl,-oformat=coff` on a PE target (`MCC_OUTPUT_FORMAT_COFF`, wired
through `mcc_set_linker`'s `oformat=` arm and preserved past `mcc_set_output_type`'s OBJ→ELF
reset). It is the inverse of `coff_map_reloc`: x86_64 fully (`REL32`/`ADDR32`/`ADDR64`/`ADDR32NB`/
`SECREL`, RELA addends materialised into the section field), i386 and arm64 arms mirrored from the
reader, arm32 unsupported. **Default `-c` still emits ELF** — deliberately, so the `*-win32.obj.txt`
o0-baseline banks and `tools/arm64pe_diff.py`'s ELF-vs-ELF codegen diff are untouched; flipping the
default to COFF and re-banking those is the remaining follow-up. Verified on the native Windows box
against the vendored `x86_64-w64-mingw32-gcc` 16.1.0 (no wine needed): the four-way
mcc×mingw compile/link matrix all print identically and exit 0, `-O0`/`-O2`, over data/bss/rodata/
float programs; enforced forever by `pe/coff-obj-diff` (`tests/cross/coff-obj-diff.sh`, native WIN32
+ mingw, `SKIP_RETURN_CODE 77`) whose ELF negative-control arm makes it fail if the writer did
nothing. Not yet done here: per-function COMDAT/weak-external aux records, `.debug$*` (that is W5),
and the arm32 reloc arm. The original plan text follows.

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

**~~Stage W7 — arm-win32/arm-wince, or delete them. ~600–900 lines, or ~50.~~ — CLOSED
2026-08-13 as the deletion; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).** `arm-wince` was a
byte-identical alias of `arm-win32`; it is gone, `arm-win32` stays as an explicitly compile-only
target, the `o0_ab.sh` twin check is retired, the three `arm-wince.*` banks are deleted and the
`-O0` measurable floor dropped 8 → 7. `arm-win32` keeping no ARM PE *execution* backend is fine
— nothing here can execute an ARM PE anyway.

**Housekeeping that is cheap and should ride along with W1, ~60–100 lines total** — *mostly
landed 2026-08-13; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md):* ~~give `pe/short-import` and~~
`exec-gatecombo/*` ~~`else()` arms~~ (done; `pe/short-import` already had one); ~~give
`pe-wine-conformance` the `run-tier.sh` wineserver teardown~~ (done, in `suite_pewine`); ~~add
`RESOURCE_LOCK "wine"` to the wine cells~~ (done); ~~add `MCC_WINE` and
`MCC_WINE_REQUIRED` cache variables so a wine-less host can be made to fail rather than
green-skip~~ (done — `MCC_WINE_REQUIRED=ON` → FAIL exit 1, verified); ~~add `pe-wine-conformance`
and `run-tier/{x86_64,i386}-win32` to `tests/must-run.txt`~~ (done); ~~correct
`tests/must-run.txt` to state both of `runtime-bench-gatewin`'s causes~~ (done); ~~add `arm-win32`
to `tools/build.c`~~ (done, folded into W7); **still open (unlocated):** fix the stale i386 message
at `CMakeLists.txt` — every i386 message was checked and none reads as stale, so nothing to fix.

**W3 landed flag-gated 2026-08-13 on the Windows box; W1, W2, W4–W6 remain open.** The "blocked
here" framing above was written from the Linux/WSL host and is partly false on the actual Windows
checkout: the mingw oracle it calls absent is *vendored*
(`vendor/winlibs-mingw-w64-16.1.0-ucrt-x86_64/.../x86_64-w64-mingw32-gcc.exe`) and PE runs
natively, so W3's differential loop — "this host cannot run" — runs here in ~1 s with no wine.
W3's COFF writer is in and enforced (see the Stage W3 note). W1 (cross-oracle) is still genuinely
short its *corpus*: `vendor/gcc-c-torture-execute` is not in this checkout, so a native W1 cell could
only run the internal `tests/exec`/`gpu/cref` corpora (written to pass mcc — low information), and
it cannot reproduce the five §3 divergences or unblock W2. W4 (per-function `UNWIND_INFO`), W5
(CodeView) and W6 (SEH) are the large backend gaps and are unchanged; W5 is unblocked by W3's COFF
sections.

### 6. The verdict, and the sequencing

**What is a real gap.**

1. **~~No COFF object writer.~~ — first cut landed 2026-08-13 (flag-gated).** This was the one:
   a capability the platform requires and the compiler did not have, invisible because every Windows
   cell in the tree drives mcc all the way to a linked `.exe`, where the format is right. `mcc -c`
   now emits COFF under `-Wl,-oformat=coff` and links both directions against mingw (proven on the
   native box, `pe/coff-obj-diff`); default `-c` still ELF pending the re-bank. What is still missing:
   COMDAT/weak aux records, the arm32 reloc arm, and making it the default.
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

**~~W7 should be taken as a deletion, now, independent of everything else.~~ — DONE 2026-08-13.**
It removed a target that duplicated another byte for byte and stopped two `-O0` bank keys from
spending budget proving a tautology; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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
| **17** | **`-O3` re-emission leaves the pre-inline copy in `.text`** — ~~**27 functions / 52,022 B, ~3.6%** of `.text`~~ **re-measured 2026-08-13 on an `-O3` self-compile: 45 functions / 77,914 B ≈ 5.05% of emitted body bytes.** `ast_reemit` appends at the current end of `.text` and re-points the symbol at the new copy; nothing rewinds to `AstReemitFn.body_ind` and no later pass compacts, so the old body and its relocations stay in the object unreferenced. Not a correctness bug. **The counters now exist** — `ast.orphan_fn`, `ast.orphan_bytes`, `ast.orphan_relbytes` under `MCC_INV=1` — but no cell, no bank and no entry in the codegen list reads them, so the clause survives the instrument | medium | **emitted code** |
| **18** | **`--mutate` is blind to `memcpy`, and the real gap is the corpus.** Four of six operator sites already perturb written memory and `g_frame_mismatch` exists; what is missing is **any `memcpy`/`memset` in the slice corpus to mutate**. Smaller than the debt as filed | small | **test strength** |
| **19** | **Debt 6-vi — the chain-store *member* fixture was never written.** Its stated blocker (debt #6a's `-O1` vstack underflow) has been gone since 2026-08-09. `exec-chainlive/*` covers the live half; the member half of the pairing has no cell | small | **regression cover** |
| **20** | **`flagsweep-cover` and `asm-gas-directives` are `mcc_skip_test` stubs — `cmake -E echo`, structurally incapable of failing.** `flagsweep-cover` hides 75 covering-array rows behind an opt-in that nothing runs; `asm-gas-directives` parks a real unimplemented feature (*"integrated assembler lacks sgdtq/sidtq/swapgs encodings"*) as an always-green cell. Neither is in `tests/must-run.txt`. There are **74** `mcc_skip_test` call sites, 17 live in this configuration | small each | **cells** |
| **21** | **Hazard 1 is still live: `BREAKEVEN` is a hand-pinned literal** at `tools/loop-census.py`, duplicated as `lc_thr[]` in C, and it cannot be gated (`--cost-synth` 77s with no device, `slice/cost` carries `SKIP_RETURN_CODE 77`). The provenance banner landed; the constant did not. **Un-pinning is ~15 lines of C + ~25 of Python**, and it is what the entire remaining integer lane source (`vlaloop`'s 64 trips against a frozen `8`) is adjudicated against | ~40 lines | **ns / lanes** |
| **22** | **`rir-coverage.py`'s `wide` denominator is "the files that happened to compile"** — an `os.walk` of seven directories with no manifest, so a source dropping out silently shrinks the ratchet. Cheap first step already named: bank `sources=N` and fail when it moves. Adjacent: `LOW_EXCLUDE` is a **filename suffix match with no count**, so renaming `mccgpu.c` produces a fake regression with no diagnostic | small (the floor) | **census trust** |
| **23** | **`rir-nofb-probe`, `--check-gap-dir` and `--check-low-dir` all pass over an empty input.** ~~The bank already holds four empty `nofb_miscompiles` lists~~ — **false since `5d75acd8` (2026-08-10): three are empty and `O0` holds `src/mcc.c::cleanup_symbols`, a real miscompile where a stage-1 shipping its replay bytes segfaults**; gap fixtures cover **3 of 18** `UNF`+`WHY` classes | small per guard, medium for fixtures | **gate strength** |
| **24** | **`stratsweep.sh` and `flagsweep.sh` drop subjects silently.** `$WORK/skipped` is written and never counted; the only floor is `n > 0`, so a miscompile breaking 30 of 31 subjects prints `PASS stratsweep-iso all: 22 strategy/ies x 1 subjects`. Both already print the survivor count — pin it | small | **gate strength** |
| ~~**25**~~ **CLOSED 2026-08-13 on x86_64-linux, and the blast radius it quotes does not reproduce** | **The non-LVAL local `Ref` question is now answerable, not open.** `wt/decaytype` fixed the identical defect in `ast_dep_decode` on 2026-08-09 — an `AST_Ref` accepted as a base address without checking `VT_LVAL` — with cell `id=25 dp_gptr_alias`. That answers the semantics in favour of "address" but did not touch `ast_eval_slice.h`'s `Ref` arm, `kind_ok` or `livein`. **All three now test `!(t & VT_ARRAY)`**, which is the narrowing the earlier attempt missed: refusing every non-LVAL local `Ref` was tried and reverted because `tools/slicerun.c` and `tools/spvgate.c` both write plain `VT_LOCAL` for a *value* reference — but every one of those is scalar-typed, and all eight array bases in the harnesses are built `VT_PTR \| VT_ARRAY`. The same discrimination was already being made two functions away, in `ast_eval_slice_globl` and in `ast_eval_slice_idx_base`, so the local arm was the odd one out rather than the convention. ~~Blast radius **93 of 3994 accepted slices (2.3%)**~~ — **measured, and it is zero.** One `MCC_ARENA_DUMP` over all 308 `tests/exec` programs at `-O1`, replayed through both classifiers: `blocks=3227 eligible=1220` **identical**, and the only figure that moves is `inv-sole-blocker` **124 → 117**. The guard fires on 7 blocks and every one of them was already refused for another reason, so no accepted slice was being decoded wrongly on this corpus. **The directed test is what settles it, not the census**: `slice/frame` now asserts that an array-typed local `Ref` in a value position is refused, with a scalar control beside it, and the cell **fails at `HEAD` without the guard** and passes with it | small | **reference correctness** |
| **26** | ~~**Cluster L is a dependency chain and its first link is unbuilt.** `L1` — give the JIT a shutdown — blocks `L2`/`L3`/`L4`/`L6`/`L7`/`L8`/`L9` by construction~~ — **CLOSED 2026-08-10 (`wt/jitshutdown`), and the row was wrong three ways.** The shutdown is `L2′(i)`, not `L1` (`L1` is device bring-up; `ARCHIVED.md:23151` says *"L2′ before L2"*). **There is no `L9`** — cluster L is `L1`–`L8` plus `L2′`, and this row is the only place in `docs/` the token appears. `L3` and `L6` were never blocked by a `pthread_t`: `L3` residency already landed, and `L6` is a predicate in `src/mccast.c`. What was real: workers `pthread_detach`ed with no handle retained. Now retained and joined; `mccjit_shutdown()` exists, drains, and is `atexit`-ordered ahead of the KGC flush. Genuinely unblocked: **`L2`'s precondition (i)**, **`L4`** on the lifetime axis, **`L8`** on the pool axis, **`L7(i)`**. Still blocking `L2`: `L2′(ii)`/`(iii)`, both `src/mccgpu.c`. Still blocking `L4b`: its own no-`mccjit_swap_lock` constraint, which is `S7b` | landed | **device lifetime** |
| ~~**27**~~ **CLOSED 2026-08-12 at `0e91e31d`, confirmed against the tree 2026-08-13** | **The gate-mask gap.** `ast_math_inline_env`, `ast_interchange`, `ast_fusion`, `ast_tile` and `loop-vlat` mutate the arena before the JIT's mask snapshot and carry no `AST_SG_*` bit, so the JIT cannot know what shaped the tree it is handed. ~~Stated at `:8650` and again at `:7756`; no later mention.~~ **All five now have bits** (`AST_SG_VLAT`..`AST_SG_TILE`, bits 42–46 in `src/mccgate.h`), produced in `ast_search_gates_now`, consumed in `ast_search_gates_set`, and the real mask reaches `mcc_jit_submit_ast` and all three `mccjit_embed_note` calls and is serialized as `warm_gates`. The five stay out of `ast_search_searchable` **on purpose** — none is idempotent on its own output, which is the "refuse, never re-run" decision recorded below, not an omission | design | **correctness** |
| **28** | **`storeval-callstore` is at `MCC_OPTD_LEVEL(2)` and was never ranked in either direction** (`src/mccopt.h:39`). The ICE that made its off-state unmeasurable was fixed at `:7629`; nobody has run the bench since. Adjacent and larger: **32 of the 34 demoted rows on rungs 10/11/12 are still unpriced** — only `narrow` and `tree-copy-prop` were measured, and rung 12 remains a deletion-candidate list nobody has read | one bench, then 32 | **emitted code** |
| **29** | **The `MCC_OPT_REPLAY_FALLBACK` flip is an untaken decision, and the fallback is silent either way.** No known defect blocks it (`:9126`), the backstop landed at `705f0b0f`, all four delta-debugged flag sets closed, and ~~`rir-nofb-probe` banks zero miscompiles~~ — **false since `5d75acd8`: `O0` banks `src/mcc.c::cleanup_symbols`, so one of this recommendation's four legs is gone; re-check the other three before acting on it**. ~~Keeping the gate costs **2.0% of bodies but 10.2% of body bytes** getting no optimization at all at `-O1`.~~ — **re-measured 2026-08-13: 2.18% of bodies (three instruments agree) but 7.65% of bytes, and the capability claim is structurally wrong** — `ast_rir_nofb_env` feeds `keep`, not the strategy gate, which reads `faithful`. Flipping it hands nobody the optimizer, and costs +840/+830/+517 bytes on the three corpora that move. **Keep the gate.** **Recommended under either decision and not done: make the divergence visible** — `rir_prod_note` only reports at `MCC_RIR_PROD>=2`, so in a default build a fallback leaves no trace | small (visibility), then a decision | **emitted code** |
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
5. ~~The Metal freeze — *"Keep the `#if MCC_GPU_LANG_MSL` arms only where they already compile; add no more"*…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
6. **W3's 3-way-concurrent closure** — the best-evidenced result in the file (399 chains / 0 non-identical against a reverse-applied negative control, Fisher p = 0.0015) and no cell holds it. A regression reappears only as flake.
7. **The `mslgate` arm** (`:4033`) — compiles clean and links 51 `msl_*` refs. ~~Now moot under the Metal drop, but still written as an open verification gap.~~ **RE-OPENED 2026-08-09 (`wt/metalspec`) and it is no longer accurate to call it unexecuted**: `## The Metal per-value differential, and N6 on Darwin` records it running 151.9 M points with zero mismatches on an M1 Pro. It is unexecuted *in CI*, and cannot be executed there — see §4 of the spec at the head of this file. It is the harness every stage of that spec is differentiated against.
8. **`ast_eval_slice()`'s poison-flag fix** (debt row 2) — fixed in code and correct; no cell is named for it. Coverage is incidental via `slice/deref` / `slice/real` / `slice/musl`.
9. **The `narrow` pin** — the banked figure did not reproduce (banked **−0.60%** cpu / **1.91%** stage-1; re-read **−0.0088%** / **+0.876%**, about half). The pin rows now carry both numbers; the discrepancy was annotated, not explained, and no cell compares `levelpins.txt` against a re-take.

### Written as live, actually superseded

1. **Board row 1 and still-open row 1** (`:497`, `:507-514`, `:3015`) call the missing `indirect` guard on `ast_dep_base_distinct` **"UNMEASURED, and it is the top of the board."** It landed at **`adf08e3b`**. Verified in the tree: the parameter is `src/mccast.c`, the guard `:13350`, both emitting callers pass `0` (`:13516`, `:13566`), only the census site passes `ast_dep_alias_oracle_env` (`:13949`) — and the **22 `exec-{interchange,fusion,tile,search*}/loop_*` cells are registered and green**. *The board's number-one open row is closed.* Its own body says so at `:2104`.
2. **The registration figure is two generations stale.** `:16-17` say **9136**; `:624` and `:2656` say 9136 *"today"*; `:985` says **9149**; the tree says **9151** in both dirs. The `wt/gatefin` write-up raised it to 9149 and never propagated to the head; the merge with `wt/idiomcov` added the last two and nothing recorded it. Hazard 5's "164 low" delta is quoted against the stale pair.
3. ~~Three counts of one list. `:521` "Twelve have now failed to reproduce", `:670` "The nine figures that have…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
4. **`SKIP_RETURN_CODE` count.** `tests/must-run.txt` says **141**, `docs/ARCHIVED.md:22965` says **138**, `CMakeLists.txt` has **149**.
5. **`:9598` — "Deliberately not banked: byte faithfulness."** False at HEAD. `kept_coverage` is banked on all eight rows of `tests/rir/coverage-bank.json` **and enforced** (`tools/rir-coverage.py`, skipped only on an unbanked host format). The reversal is recorded at `:7886`; the C2 paragraph was never rewritten.
6. **`:9600-9602`** — "modelled 99.59% / 99.56%, capture 100.00%" is stale; the bank reads **100.0 / 100.0 / 99.9681 / 99.9681**.
7. **`:9584-9590`** — the lowerable floors are **three re-bankings** stale (`MCC_RIR_LOW_EXCLUDE`, the leaf graft, and the fourth re-bank).
8. **`:9624-9626`** — "do not turn `-fno-replay-fallback` on by default… ≥4 fallback bodies are genuinely wrong" is contradicted by the **newer** decision at `:9124-9154` (no known defect blocks it; suite green; ~~`nofb_miscompiles` empty~~ — **`O0` has held one entry since `5d75acd8`**). The prohibition predates the `union_cast` / `transparent_union` / `chained_assign` fixes and has no subject.
9. **`:3116`** — "Five of the eight landed" over a list that holds **nine** items (0–7 plus 6a).
10. **`:3597`** — "`levelbench.tsv:47` is now line 51". The file is **29 lines / 16 data rows**; neither line exists. The same applies to every "24 of 47" / "32 of the 47" count in hazard 2.
11. **`:353-357`** — "turns a lavapipe/NVIDIA denormal disagreement into a hard CI failure no code change can fix" is refuted at `:404-408` and never struck; `:439` still lists the retracted reason as load-bearing in the recommendation.
12. ~~`docs/ARCHIVED.md:22579` marks E6 "NEW 2026-08-08, OPEN" while `ARCHIVED.md:22989` and `:6238` of this file…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
13. **`:10011` duplicates `:10039`** (32-byte vector alignment), **`:9282` duplicates `:10076`**, **`:9759` duplicates `:10078`**, **`:10051`** sends a reader at a capture path that measures **100.000%**, and **`:10182`** still lists `__builtin_powi`/`powif` as missing after `:9303` closed them.
14. **E1's refusal-site count has now been stated three incompatible ways** and every one was written as current: `:7810` "**eight** separate sites", corrected at `:7851` to "**16**, not 8" with the sites enumerated, corrected again at `:100-113` to "**36 lines, 43 occurrences**, not the 1 + 4 + 6 = 11 this paragraph claims". Only the last is right — verified this sweep: `slice_inline.h:2`, `mccslice.h:4`, `mccgpu.h:12`, `ast_eval_slice.h:18`, because `mccgpu.h`'s block is mirrored across the two emitters and `ast_eval_slice.h`'s eighteen were never counted. `ARCHIVED.md:22574` still carries the "16 refusal sites, not 8" figure.
15. ~~Seven `TODO` markers in the tree name sections of this file that do not exist~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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
2. ~~`tests/optfire/levelbench.tsv` is a null-experiment factory, and it is also a generation stale.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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

8. ~~`tools/fmt-census.py`'s site census is unguarded.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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
2. ~~`opt-cache-determinism` should be a `registered` row in `tests/must-run.txt` and a permanent 77 should be…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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
   already holds ~~four empty~~ **three empty and one one-entry** `nofb_miscompiles` lists and the cell passes no floor. Same
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
13. ~~`tools/idiomgate.c`'s invariant covers 17 of 37 config macros, from two hand-typed lists.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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
17. ~~Four shell tools grep for `ast_env_gate`, which no longer exists in `src/`.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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
3. ~~`MCC_SLICE_CENSUS_RUN` is undocumented and `-L census` reports green while three of its six cells 77. Either…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
4. ~~Registration gates with no `else()` skip stub — cells that *vanish* rather than skip, which is the class…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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

`tests/ast/o0-baseline/` was last taken at `bc85ce70` (2026-08-03) when this was written;
**re-taken 2026-08-11 at `a404d8c9`, all twelve rows** — see *The four red `ast/o0-baseline`
cells* above. `tools/o0_ab.sh` has
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

1. ~~The `--arenas=` figure needs re-taking against one TU.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
2. ~~`idiomgate`'s subject is four.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
3. ~~`tools/o0_ab.sh`'s gated half stays frozen~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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
| **`tests/emitmap/bank.json`'s tolerances cannot fail** — new 2026-08-13, found while banking. `TOL` is 0.05 on `emit_amplification` and **1.0 on every percentage**, and the `selfhost` cell is *already* drifted inside them: banked 2.7112 / 97.79 / 1.6205, measured **2.712 / 97.82 / 1.6214** on a trace build, and **identical at `HEAD` in a clean worktree**, so the drift predates the wave that found it and has no attribution. The figures were **restored, not re-banked**. Two jobs, in this order: bisect the drift, then tighten `TOL` — re-banking first destroys the only evidence of when it moved. Same shape as the cluster thesis with the sign flipped: not an absent number read as zero, but a **present number wide enough to be unfalsifiable** | census trust |
| **`rir-coverage.py`'s `wide` corpus silently drops 9 sources** — new 2026-08-13, and it is sweep row 22 with a count finally attached. The run prints `9 source(s) failed to compile` and computes every percentage over the remainder; identical at `HEAD` and after, so pre-existing. The denominator is still "the files that happened to compile", and the cheap first step named there — bank `sources=N` and fail when it moves — is still unclaimed. **Now cheaper than when it was filed**, because the per-body inventory landed: 4574 named bodies is a manifest, so a source dropping out is a diff rather than a percentage | census trust |
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

1. ~~The JIT pool has no shutdown (L2′, `docs/ARCHIVED.md` cluster L)~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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

- ~~No coroutine, fiber, `ucontext`, `makecontext`/`swapcontext`, generator, continuation, scheduler, event-loop or…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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

1. ~~`MCC_AST_EVAL_LADDER_GPU=1` smashes the stack. One-line fix.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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
   `SValue sv_stack[VSTACK_SIZE + 1]` (`src/mccast.c`) is ~~32,832~~ **41,040** B — `VSTACK_SIZE`
   is 512 and `sizeof(SValue)` is ~~64~~ **80** (measured 2026-08-13; the 64 was a 25% undercount)
   because `CValue` carries a `long double` — declared
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

6. ~~`354e96f6` is half-landed, and it broke dump reproducibility.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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

0. ~~Accept two-child `AST_If` in both emitters.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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

2. ~~int64 in the emitters, as a `uint2` pair.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

3. ~~The four RIR opcodes that actually fire.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

4. ~~Quote `kept_coverage`, not `modelled`.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

5. ~~Give the GPU cells teeth.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

6. ~~Build `spvgate` locally.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

7. **Raise the emitter caps, and fix the one that binds first.** `SPV_MAX_CONST`/
   `MSL_MAX_CONST` = 512 distinct constants (`src/mccgpu.h`, `:105`) — a 2049-node
   arithmetic chain fails on the **constant cache**, not on module size. Measured
   emitter cost is 11.7–20.4 SPIR-V words per node, so `MCC_GPU_CODE_MAX`'s 8192 words
   is ~400–700 nodes; the largest real invoke-free region is 1114 nodes. Neither cap is
   a device limit — Vulkan sets no module-size bound.

8. ~~Measure the Metal/Vulkan dispatch round-trip.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

9. ~~The Metal path reports device failures as success.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

10. ~~Stop destroying device buffers per dispatch — free performance.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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
  `nofb_miscompiles` list is ~~empty for O0/O1/O2/O3~~ **empty for O1/O2/O3 only — `O0` holds `src/mcc.c::cleanup_symbols` as of `5d75acd8`**. It used to hold `union_cast::main`.
- The 3-way covering array varies `replay-fallback` across 74 rows over 107 flags and
  finds nothing.

**What the gate costs while it stays on.** `ast_run_strat_seq` gates every one of the 22
strategies on `faithful`, so a byte-divergent body receives *no* optimization at all. At
`-O1` that is 2.18% of bodies but ~~**10.2%**~~ **7.65%** of body bytes (re-measured 2026-08-13), because a discarded body averages
2585 bytes against 470 for a kept one — the gate is withholding the optimizer from the
largest functions in the program. That is a capability cost, not a correctness one.

> **Re-measured independently 2026-08-11, on a different corpus, and the two do not
> transfer.** `src/mccinv.h` counts **9 unfaithful of 1894 verdicted bodies at `-O1` — 0.47%**
> over 300 `gcc.c-torture/execute` programs, against the 2.0% above on `src/mcc.c`. **Both can
> be true**: the figures differ by corpus, not by date, and this file's own standing rule is
> that a gap in this area needs a hand-written denominator rather than torture's. Quote the
> 2.0% for the self-host hot path and the 0.47% for torture, never one for the other. See the
> emit-coverage section in *STATE OF PLAY*.
>
> **Settled 2026-08-11 by a third instrument.** The emit map measured `src/mcc.c` at `-O1`
> directly: **70 unfaithful of 3165 verdicted, 2.21%**, against the 2.17% at `-O2` above. Two
> levels, two instruments, 0.04 points apart — the self-host figure is not a level artefact and
> not an instrument artefact, and the "quote by corpus" rule above is now the measured answer
> rather than a caution. The map also puts `full_language.c` at 3.32% and both targets' `-O0
> --embed-jit` arm worse still; see **N18**.

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
- ~~`stage2 / windows / dynamic` CI was red (40 cells).~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
- ~~W8 — fix the `selfhost-jit` heap corruption.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
- ~~Windows build broken by `tools/slicerun.c`.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
- ~~W3 residual — the 3-way-concurrent stress re-run.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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
- ~~Wire `tests/optfire/flagsweep.sh`'s exec half as ctest cells.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
- ~~Unpin `replay-fallback` from `tests/optfire/cover3.py`.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
- ~~Root-cause the two masked-by-the-gate replay defects.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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
- ~~Land the held `fix-imaginary` branch.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

### Open codegen / front-end defects
- `__bf16`: finish encode/decode + ABI now that `VT_BTYPE` is 5 bits. **Do not alias
  `__bf16` onto `_Float16`** — distinct `c.i` storage, `is_float_abi`, libgcc name.
- 32-byte vectors are laid at 16-byte alignment (`MCC_MAX_ALIGN` cap) — open ABI
  decision; cross-TU to gcc is currently incompatible (struct-ABI, not SysV vector).
- `aligned(N)` bitfields: ~139 survivors.
- ~~`expr_type()`'s unconditional `nocode_wanted++`.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
- ~~`selfhost-qemu-{i386,arm}-O2`~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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
- ~~`-fno-opt-search-<anything>` disables the whole search, not the sub-knob it names.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
- ~~`jit/xoracle-conformance` drops its second corpus silently.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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
- ~~The two win32 `-run` cells are an intermittent wine flake.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
- ~~Reconcile the deliberate-red count.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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

### ~~Latent defect found on the way, not fixed here~~ — CLOSED, verified 2026-08-13

**`rir_decayed_array`'s unguarded `sv->sym` dereference was fixed by `81a81f1b`** and this block
sat here for four days describing the pre-fix code. `src/mccrir.c` now decides both `r` shapes
(`as_sym`, `as_loc`) and returns before touching `sym`; the `wide256_settle` side-step it
describes is gone from `src/` and `tests/exec/types/int256.c` pins the shape with
`test_replay_cmp`. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).

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
- ~~The device arm computes a census and never ratchets it.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).


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
3. ~~`L2′(ii)` and `L2′(iii)` are unchanged and are what `L2` is now waiting on.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
4. ~~`mcc_gpu_quiesce`'s unbounded `vkDeviceWaitIdle` becomes reachable from `atexit` the moment `L2` wires the…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).


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

#### ~~Still open — three unguarded `sym` dereferences, filed with their reachability~~ — ALL THREE CLOSED, verified 2026-08-13

The block described the pre-fix code and was carried forward on 2026-08-10 without being
re-checked: it was written inside `81a81f1b` on 2026-08-09 at 10:30 and all three fixes landed
the same evening, 21:44–21:48. `unary()`'s `'&'` arm runs `test_lvalue()` before the register
check (`3fe04727`, pinned by `tests/diagnostics/dg-error/address_of_comparison.c`); both
`check_va_start_*` arms open with an `r`-shape test (`7a9a8e9b`, `0e9ac0fd`); and the
builtin-fold ISA scan tests `ast_op(a, c) & VT_SYM` (`0cfe71a7`, the row this file called
`res-d4b`). **The root-cause fix the block preferred — make `vset_VT_CMP` write the whole union —
was not taken and still is not**; `vset_VT_CMP` writes only `cmp_op`/`jfalse`/`jtrue`. That is by
design: the block's own argument is that the three reader-side guards carry none of the hot-path
risk. Write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).


> From the archived section *The JIT, measured for the first time — 2026-08-09 (`wt/jitconform`)*.

#### Still open on the JIT after this branch

1. ~~The KGC zero-extension miscompile above.~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
2. ~~47.1% of programs cannot be baked at all. The single largest lever on JIT coverage is the `VT_STATIC |…~~ — closed; write-up in [`docs/ARCHIVED.md`](ARCHIVED.md).
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


