# TODO

Three layers. **§ System matrix** (next) is the orientation — the four subsystems (CST · AST · AOT ·
JIT), their macro features, gating posture, and coverage boundaries, reconciled against `src/` this
session. **§ Strategic path** is the authoritative execution order — the recommended sequence, the
resolved forks, and what is deferred. Everything below is the **reference library**: the open +
partially-landed backlog and the long tail, retagged with the phase that consumes each item
(`[P0]`/`[P1]`/`[P2]`/`[P3]`/`[FLOAT]`/`[DEFER]`). Completed items are pruned; a still-open item is
reduced to what is left. When path and library disagree, the path wins.

---

## System matrix — CST · AST · AOT · JIT

Four subsystems share one parse pass and one gate vocabulary (`src/mccgate.h`), but diverge sharply in
maturity and reach. Macro-feature status is **L** landed · **~** partial (tail open) · **○** built but
unconsumed · **✗** not built.

### CST — concrete syntax tree · `src/mcccst.{c,h}` (~1640 L)

| Macro feature | State | Detail |
|---|---|---|
| Lossless byte-exact CST + round-trip | L | SoA `CstArena`, 37 `CstKind`, width-tiling validated (`cst_validate`) |
| Content-addressed `#include` store | ~ | data path built (`CstStore` intern/dedup); **not driver-wired** into the consuming file |
| Symbol def→use xref | ○ | `cst_hook_def/use` resolved via `cst_node_at`; **no query surface** |
| Positional query (offset→node) | L | `cst_node_at` |
| Snapshot save/load | ○ | versioned, endian-tagged; **self-check only**, no driver path |
| Merkle struct+trivia hash | L | incremental-reconcile primitive |
| **Product consumer** | **✗** | `cst_capture_end` **discards the arena unconditionally**. No LSP server, no `-g`-from-CST, no `--hotreload`. The whole subsystem is a validated substrate + test harness — every product capability is latent. |

**Coverage:** single main-file only (multi-file `#include` stitching = the one open CST slice, "slice-G");
architecture-independent (pure byte-offset logic, no arch `#ifdef`). `CST_Error`/`CST_Missing` kinds are
declared but never emitted (no error-recovery CST).

### AST — side-car substrate + unified optimizer + search · `src/mccast.c` (13049 L)

| Macro feature | State | Detail |
|---|---|---|
| 20-row strategy pipeline `ast_strategies[]` | L | `ast_func_end` runs the frozen table (cycled to fixpoint under `MCC_AST_CYCLE`) |
| Side-car indices (epoch-invalidated) | ~ | `ast_hash_*` / `ast_du_*` / `ast_memo_*` built; **4th index (predicate-vector) NOT built** |
| Value lattice `AstVLat` | L | interval + known-bits, region-scoped per-use projection; gated `MCC_AST_VLAT` (off) |
| `combo_run` `-O4+` search | L | subset/order lattice over baseline gates; opt-in `MCC_AST_SEARCH`; fork-pool scoring |
| Memo | ~ | in-mem `AstSearchMemo[4096]` + disk `MSZ1`; **3 memos not yet unified** (+ `ComboMemo`, out-of-proc `SoPfCkpt`) — M2/M3 |
| Loop-nest §27 (interchange/fusion/tile) | L | `AstLoopInfo` + dep test; all gated off |
| `eval_slice` soundness oracle | ~ | AST-over-values UB oracle; **shadow-only** (hard-gate deferred) |
| const-data re-emit (`AST_Data`, M5) | L | size-preserving in-place; size-**changing** datacomp (M6) open |

**Coverage:** replay on `-O1+`; register-promote is **x86_64-only** (`opt_promote`); validation = the M8
7-gate bar. **Held** (default-off pending soak/backend): `DIVMAGIC` (x86_64 self-host mul-high miscompile),
`ABS` (needs cmov), `COLOR` (fix landed, soaking), `REASSOC` (order-non-confluent), `VLAT` (queued; PR-C
IV-widening held), §27 passes, §26 JIT.

### AOT — codegen backend · `src/mccgen.c` (12955 L) + `src/arch/*` + `src/objfmt/*`

| Macro feature | State | Detail |
|---|---|---|
| Code generation (AST-replay-driven) | L | frontend records AST, `ast_replay_*` re-drives the same `*-gen.c` emitters |
| 5 arch backends | ~ | x86_64 Tier-4 (ref) · arm64 Tier-3 · i386/arm Tier-2 · riscv64 Tier-3 **self-host gap** |
| 3 object formats | ~ | ELF (mature) · Mach-O (x86_64+arm64) · PE (no UBSan/asan-shadow, over-align off) |
| Assembler / disassembler | L | GAS-style asm; full 5-arch disasm |
| Own linker (self-host) | L | per-arch `*-link.c`; external GNU ld hits the overlapping-FDE `.eh_frame` quirk |
| Debug info | L | DWARF + stabs (`mccdbg.c`) |
| Sanitizers | ~ | ASan native-shadow **x86_64/ELF-only**; bcheck ELF+PE; UBSan trap x86_64/arm64/riscv64 non-PE; stack-protector x86_64+arm64 |
| Reg alloc / promote / color | ~ | AST-level analysis (arch-agnostic); promote wired x86_64; arm64 x19–x28 gap, riscv64 gap |

**Coverage boundaries (measured):** **cmov/csel emitted on NO arch** (codegen has none; only asm/disasm
know the mnemonics). div-magic is a 32-bit **AST fold**; 64-bit needs a per-arch `mulh`/`__int128` primitive.
Over-alignment gated off on PE and under asan/bcheck. Self-host: x86_64 full 3-stage fixpoint · arm64 via
qemu+musl · **riscv64 blocked (Tier-3 gap)** · i386/arm cross-conformance only. qemu is x86-TSO → cannot
validate the aarch64/armv7 memory model.

### JIT — runtime recompile + guarded deopt (§26) · `src/mccjit_embed.c` (3059 L) + `src/mccrun.c`

| Macro feature | State | Detail |
|---|---|---|
| Baseline retention (deopt fallback) | L (M1) | `ast_baseline_splice` — retained AOT bytes+relocs |
| Machine-byte-splice entry dispatcher | L (M2) | modes 1–6; "M2 alone is a shippable guarded-deopt JIT" |
| Non-null speculative spec + `--jit-functions` | L (M3) | search-selectable via gate bits 40/41 |
| Cross-session re-emit / embed self-swap | ~ (M4) | intent serialize + `.init_array` ctor; **remaining:** bitfields, static-link, per-sym registry, Tier-B size |
| Mode-6 slot + in-process hot-swap loop | ~ (M5) | recompile→publish→atomic-swap works; **remaining:** in-*program* slot→recompile wiring, QSBR |
| Known-good cache + differential deopt-verify | ~ (M5b) | `MccjitKgc` mmap tuple set; **remaining:** FP/struct args, mismatch policy, oracle-skip |
| Purity classifier | ~ (M5c) | `ast_fn_purity` whole-fn; **remaining:** statement-level slicing, off-C-ABI register kernels |
| N-worker pool + hot-counter trigger | L (M6) | shared queue + async lazy promotion |
| `jit-patchpoint` (D3B) | ✗ (M7) | deferred; pointer-swap is primary |
| `eval_slice` hard-gate | ~ (M8) | oracle bites in shadow mode; hard-gate promotion deferred |

**Coverage boundary (the sharp one):** the recompile *engine* (`mcc_run`/`mcc_relocate`/`host_runmem`,
incl. arm64/macOS `MAP_JIT` W^X) is **cross-arch**; the dispatch/stub *tail* (mode-6 slot, KGC stub,
trampoline, counter) is **hand-emitted x86_64-ELF-only machine bytes** (`__x86_64__`), validated on Linux/x86
CI only — not the arm64-macOS dev host. Signatures restricted to **1–6 GP int/ptr args, non-FP/non-struct
return** (KGC stub emits only `mov64`/`movsxd`; FP/struct fall back to the direct trampoline, no verify).
Build gate `MCC_EMBED_JIT` off; runtime `MCC_AST_JIT` off.

### Cross-subsystem overlap (the intersections)

- **AST↔AOT — one replay path.** `ast_reemit_retain` stashes byte-faithful bodies serving **three** consumers:
  inline-graft, JIT deopt fallback, const-data re-emit. Optimizations mutate the AST *between* record and
  replay; the same `*-gen.c` emitters produce both faithful and optimized bytes.
- **AST↔JIT — not a fork.** The JIT's compile-time pieces are dispatcher modes emitted inside `ast_func_end`,
  driven by gate bits 40/41 in the same `AstGateMask` the superopt search scores. Recompile = re-invoke the
  whole engine (`mcc_new`→`mccgen_init`→`ast_reemit_extern`→`mcc_relocate`). The genuinely JIT-only runtime:
  `MccjitKgc`, the hand-emitted stubs, the pool, the boot-swap ctor.
- **AOT↔JIT — shared engine, split tail.** `mcc_run`/`mcc_relocate`/`host_runmem` are shared `-run` infra;
  the divergence is the x86_64-only stub layer above them.
- **CST↔everything — observer only.** Shares the parse pass and one namespace slot (`MCC_NS_CST_BRANCH`),
  injects one field (`cst_base`) into the lexer, and otherwise has no pointers into AST/codegen and no
  influence on output. Freed immediately after capture.
- **Search substrate — vocabulary unified, memos not.** `mccgate.h` (`AstGateMask`, `AST_SG_*`) is the single
  shared gate definition across `mccast.c`, `tools/asttool.c`, `mccjit_embed.c`. But three measurement engines
  persist (in-proc search memo, `ComboMemo`, out-of-proc superopt) — M2/M3 collapse them.

---

## Gating ledger — default state by posture

Defaults set in `ast_configure()` (`mccast.c:1097+`). Three postures:

**Default-ON (tied to `-O`):** `MCC_AST_TEMPLATES`/replay (`-O1+`); at `-O2+`: `SETHI`, `BITFLAG`,
`CPROP_JOIN`, `NARROW`(+`_FIX`), `SCCP_FIX`, `DSE_CALL`, `TCO_PTR`, `CSE_COMM`, `RANGE`, `CSE_JOIN`,
`CALL_WINDOW`, `ZERO_BSS`, `MERGE_STRINGS`, `PROMOTE` (x86_64-only); at `-O3` non-size: `INLINE`. Ident/reassoc/
bfold/narrow-class sub-knobs default 1 (bite only when parent on).

**Opt-in, default-OFF (landed, cleared or clearing the bar):** `NARROW_ELIM`, `VLAT`, `ARGFWD`, `SETHI_NARY`,
`SPILL_SHARE`, `INLINE_PASS`, `CYCLE`, `COLOR`, `LICM_TEMP`, `IVSR`, `PRE`, `SETHI_LEAF`, `COST`,
`PERFN_INPROC`, §27 (`INTERCHANGE`/`FUSION`/`TILE`), the search family (`SEARCH`/`_EMITSIZE`/`_EMITISO`/
`_INLINE`/`_THREADS`/`_ORDERED`/`_ORDER`).

**Held (default-OFF, blocked — see coverage ledger):** `DIVMAGIC`, `ABS`, `REASSOC`, `VLAT` PR-C IV-widening,
`JIT`/`JIT_SPLICE`/`JIT_DISPATCH` (§26).

**CST:** build `MCC_CONFIG_LSP` (default ON) + runtime `--lsp` (opt-in) + `MCC_CST_*` env probes (all off,
result discarded). No gate bit.
**JIT build:** `MCC_EMBED_JIT` (default OFF); runtime `MCC_AST_JIT` (off), `MCC_AST_JIT_DISPATCH` modes 1–6.

## Coverage ledger — arch × capability

| Capability | x86_64 | arm64 | i386 | arm | riscv64 |
|---|---|---|---|---|---|
| Backend tier | Tier-4 ref | Tier-3 | Tier-2/3 | Tier-2 | Tier-3 |
| Self-host 3-stage | ✅ | qemu | cross | cross | **✗ gap** |
| Reg promotion | ✅ | x19–x28 gap | n/a | n/a | gap |
| Tier-4 replay-inline | ✅ | ✅ | skip | skip | **unverified** |
| UBSan trap | ✅ (non-PE) | ✅ | ✗ | ✗ | ✅ |
| ASan native-shadow | ✅ **only** | ✗ | ✗ | ✗ | ✗ |
| Stack protector | ✅ | ✅ (Mach-O) | ✗ | ✗ | ✗ |
| TLS GD/LDM | ✅ | LE only | **untested** | — | — |
| cmov/csel codegen | ✗ | ✗ | ✗ | ✗ | ✗ |
| div-magic (32-bit fold) | ✅* | ✅* | ✅* | ✅* | ✅* |
| JIT dispatch/stub tail | ✅ **only** | ✗ | ✗ | ✗ | ✗ |

\* AST fold, but `DIVMAGIC` held default-off. **Validation bar (M8, applied per gated item):** (a) ctest
byte-identity · (b) `-O6` differential vs gcc/clang · (c) self-host 3-stage fixpoint (via mcc's own linker
+ `mccrt_blob`) · (d) UBSan/ASan · (e) cross-arch i386/arm32/riscv64/arm64 (qemu-docker) · (f) differential
miscompile fuzz (~100 seeds, x86_64-Linux-only) · (g) `MCC_CONFIG_AST_SHADOW` zero-divergence. **The fuzzer
and the shadow-IV oracle are x86-only — the held items (DIVMAGIC/COLOR/VLAT-PR-C) all wait on x86 fuzz soak,
and none of the M8 gates covers the memory model on weak-ordering arches.**

---

## Strategic path — least resistance × greatest gains

**Ordering rule.** At each step take the lowest-resistance item among the highest remaining gain, and make
it unblock the next. **Guardrail (every phase):** the full M8 bar (above). Every step stays byte-neutral on
the default path until its own opt-in flag; nothing here may regress `-O1..-O3`.

Net shape (this session, **JIT-first**): **finish the x86_64 live JIT → deepen it → cross-arch + hard-gate
it → then the shared foundations its default-on + AOT-convergence need → const-data → CST.** The JIT capability
tail rides the already-landed §26 M1–M3 + M6 on x86_64, so it needs no foundation work to *start*; only the
JIT's convergence with AOT (the AOT-static scorer, data→code) and its default-on flip (cross-arch + hard gate)
wait on the shared foundations (P1 lattice, P2 const-data, 6A/2B cross-arch), which follow.

### Master execution order — JIT-first

Authoritative sequence. The thematic phase subsections (P0/P1/P2/P3/FLOAT/DEFER) below are the *detail*; this
list is the *order*. Each item's rationale is its `[decision]` tag — the 7 strategic forks (1A–7A), the JIT
tiers (J*/K*/L*) in §26, or a phase bucket. Guardrail (M8 bar) applies to every step.

**Phase 1 — verified, shippable live JIT on x86_64** *(lowest resistance, headline capability; all on landed infra)*
1. ✅ **[J2B/K3A/J8B]** refuse-to-JIT eligibility gate at *selection* — DONE. `ast_jit_want`/`ast_jit_eligible`
   (`src/mccast.c`) refuse at all three `ast_jit_selected` sites (dispatch, baseline-retain, embed-stash) so an
   ineligible fn never gets an intent blob; eligibility = 1–6 GP-int/ptr args + GP-int (non-void/FP/struct)
   return, refusing FP/struct-by-value/void/bitfield/variadic/>6-arg/0-arg. Runtime defense-in-depth: the
   `mccjit_last_kgc_ok` gate now also rejects bitfield + `FUNC_ELLIPSIS`, and the silent direct-trampoline
   fallback (`mccjit_boot_swap_run`/`mccjit_lazy_build`) is gated behind the explicit `MCC_JIT_NO_KGC` unsafe
   escape hatch — by default an unverifiable variant keeps the AOT baseline (`refused-unverified`). Test:
   `jit/selftest-eligibility` (12 cases).
2. ✅ **[L11A]** runtime robustness — DONE. `pthread_atfork` (registered once via `pthread_once` in
   `mccjit_pool_start`) resets the worker-pool code cache in the child: prepare acquires `qlock`+`swap_lock`,
   parent releases, child drops the phantom job queue + `started`/`nworkers` and reinits the sync objects so a
   forked child inherits no held lock and no orphaned worker. Signal-safety: no recompile ever runs in a signal
   handler (no `sigaction`/`signal` in the TU) and the dispatcher slot load is an `__ATOMIC` load; PIC/PLT
   variant calls already reuse the reloc path (`ast_reemit_extern`→`mcc_relocate`→`dlsym(RTLD_DEFAULT)`), no
   change needed. Test: `jit/selftest-fork` (child sees reset pool + runs installed variant + restarts pool;
   parent pool intact) — verified to fail without the handler.
3. ✅ **[hardened-env + observability]** DONE. `mccjit_feasible()` (probe: `mmap` a `PROT_READ|WRITE|EXEC`
   page + execute a 6-byte `mov eax,imm; ret`, `pthread_once`-cached) gates `mccjit_boot_swap`/`_async`; a
   W^X-denied host silently keeps the AOT baseline (+ `MCC_JIT_VERBOSE` note), never errors. `perf-<pid>.map`:
   `mccjit_perf_map_emit` appends `<hexaddr> <hexsize> <fn>` (from the variant's ELF `st_value`/`st_size`) to
   `/tmp/perf-<pid>.map` for each recompiled body, opt-in via `MCC_JIT_PERF_MAP` (default off → clean /tmp +
   reproducible/CI-safe). Test: `jit/selftest-observability` (`MCC_JIT_FORCE_INFEASIBLE` gives the fallback path
   teeth).
4. ✅ **[J3A]** DONE — per-sym blob registry + one generic ctor → **live in-program recompile** (the "real
   live JIT"). `mccjit_embed_finalize` now emits ONE `__mccjit_registry[] = {{&slot,blob,len},…}` table + ONE
   `__attribute__((constructor)) __mccjit_boot_all` that loops the table (was N per-fn ctors). The MEMORY/`-run`
   gap is closed: the mode-6 slot is now named + blob-stashed for MEMORY too (mccast.c guard relaxed), the
   MEMORY early-return in finalize is gone, and the boot engine symbols are wired into the run image via
   `mcc_add_symbol(mccjit_boot_swap[_async])` so `_runmain`'s `.init_array` pass calls the generic ctor →
   self-recompile → slot hot-swap, all in-process. Verified end-to-end on both disk (`route=kgc … swapped`,
   correct result) and `mcc -run`. Test: `jit/selftest-liverun` (in-process `mcc_run`; perf-map presence proves
   the recompile fired in the ctor — teeth-verified). Standalone disk exe still needs `libmcc` on the loader
   path — that's the deferred **[J4A]** static-link (step 7), not J3A.
5. **[J1A/K1C/K2/L6A/L7A]** mismatch policy — ⏳ CORE DONE (runtime poison/deopt), search-side deferred.
   ✅ **[J1A+K1C] DONE:** `mccjit_kgc_calln` now tracks per-variant `hits`/`misses`; on a mismatch/hit *ratio*
   (default ≥50% over ≥8 verified calls, `MCC_JIT_POISON_PCT`/`MCC_JIT_POISON_MIN`) it flips the variant to
   `poisoned` → **permanent baseline-only deopt** (no more double-execution — fixes the documented "flag written
   but never consulted" bug). In-memory / ephemeral by default (matches the reproducibility seam). Test:
   `jit/selftest-poison`. **DEFERRED (needs the AOT search engine / opt-in persistence):** **[K2]** split the
   KGC key into (code-hash, data-hash) + poison-as-search-INPUT; **[L6A]** the switch-table cover shape as a
   benchmarked strategy row (the 99%-variant + 1%-cover compound); **[L7A]** the unified {good,bad,unknown}
   LFU-bounded classified set; **[J1A persist]** poison persisted to the `mmap`'d cache under the opt-in
   persistent-cache flag. These ride the AOT `-O4` search integration (Phase 4).
6. **[K5/L4A/L5A]** best-of-3 promotion — ⏳ SCORER DONE, live wiring rides J6A. ✅ `mccjit_bench_pair`
   (`src/mccjit_embed.c`) runs a **best-of-3** wall-clock benchmark of a candidate vs the incumbent over a
   caller-supplied live-in tuple set (L4A), promoting only when the candidate is faster by more than a
   **hysteresis margin** — the **incumbent wins ties** (L5A) — with a **deterministic inner-iteration cap**
   (`MCC_JIT_BENCH_ITERS`, not wall-clock) so the verdict is reproducible. Test: `jit/selftest-bench`
   (faster→promote, slower→keep, equal→keep; stable over 10 runs). ✅ **[L4A] source wired:**
   `mccjit_promote_by_profile` feeds the scorer the **real observed** live-ins J6A captured on the hot counter
   (`st->sample`) — never synthetic inputs (a synthetic pointer/divisor would crash the callee); with no
   samples yet it allows promotion (no basis to reject). Test: `jit/selftest-l4a` (scores fast-vs-slow over the
   counter-captured distribution: faster→promote, slower→keep, no-samples→allow). **Remaining:** call
   `mccjit_promote_by_profile` from the live promotion path (`mccjit_counter_tick`/`mccjit_lazy_build`) — needs
   the raw candidate+incumbent variant pointers + nargs threaded to the promote site (the counter state already
   carries the samples); gate it opt-in behind `MCC_JIT_BENCH` first.
7. **[J4A/L9B]** static-link ship gate — ⏳ SELF-CONTAINMENT DONE, size-slice deferred. ✅ **[J4A] core:** an
   `--embed-jit` exe now static-links the engine archive and runs **standalone — no `libmcc.so` at load** — and
   still self-recompiles + hot-swaps. Enabled by `mcc.c` marking the engine link `AFF_WHOLE_ARCHIVE` (the generic
   ctor that references `mccjit_boot_swap` is synthesized during output, *after* the archive is linked, so
   on-demand extraction would miss it — whole-archive pulls the member up-front; harmless for the dynamic `.so`
   fallback, which is unchanged). Mechanism: `MCC_EMBED_JIT_LIB=<libmcc-static.a>` (the archive ships as
   `libmcc-static.a` under `MCC_BUILD_STATIC_LIB`, not `libmcc.a`, so the explicit-path env is the wiring — auto
   `-lmcc` still resolves the dynamic lib). Test: `jit/standalone-static` (builds an embed exe, asserts no libmcc
   NEEDED, runs it with `LD_LIBRARY_PATH` cleared, checks the swap fired). **DEFERRED — the size slice:**
   **[L9B]** a parser-less re-emit-only engine slice + `-ffunction-sections`/`--gc-sections` to hit the ~800 KB
   Tier-B target (today the whole-archive pull is ~2 MB — the full compiler, since the engine re-invokes
   `mcc_new`/`mcc_relocate`); and reconciling the CMake `libmcc-static.a` name so plain `-lmcc` prefers it.

**Phase 2 — specialization depth + reclamation**
8. ✅ **[J6A]** DONE — `jit-profile` runtime live-in capture riding the D5 hot counter. The counter stub now
   spills the 6 GP arg registers and passes their address to `mccjit_counter_tick(st, regs)` (one extra
   `mov rsi,rsp`); `mccjit_counter_capture` accumulates a **per-param min/max range** (the runtime range that
   makes dispatch mode 5's guard bite + feeds J7A) plus a small **ring of real observed tuples** — the *safe*
   live-in set (real values, no synthetic pointer/divisor) that closes step 6's L4A live-in-source gap for the
   K5 scorer. Captured only during the cold phase (`!promoted`), under the existing counter lock. Test:
   `jit/selftest-profile` (1-arg range `[-3,100]`; 2-arg per-param `p0=[-2,3] p1=[7,20]`; sample ring matches).
   Consumers next: J7A (step 9) reads the range; wiring the K5 scorer onto `st->sample` is a small step-6 follow-on.
9. **[J7A]** value-range speculative guard — ⏳ CONST-COLLAPSE DONE, general narrowing deferred. Now that J6A
   supplies the runtime per-param range, the actionable speculation is the collapsed case: a param observed to
   hold a single value over the whole profile (`argmin==argmax`) is a speculative constant. `mccjit_profile_pick_const`
   detects it (only over the real `nargs`, skipping the counter's unused arg-reg captures) and
   `mccjit_recompile_profiled` const-specializes on it (reusing the mode-4 `spec_fold`) — guarded downstream by
   the KGC differential verify (a wrong fold returns baseline) + the K1C poison policy (a frequently-wrong fold
   is discarded), so no static soundness proof is needed at recompile. Test: `jit/selftest-vrange` (profile
   `a==7` → variant folds `a`: `v(999,5)=705` proves it ignores the passed arg; varying params → unspecialized).
   **DEFERRED:** the general (non-collapsed) range-narrowing fold — injecting `[lo,hi]` as a VLat fact so the
   optimizer narrows/eliminates — needs the P1 VLat consumer; the compile-time mode-5 guard already emits the
   range check but has no fold yet either.
10. **[K4A/L12A]** marshalling coverage — ⏳ SCALAR ALL-DOUBLE DONE, mixed/struct deferred. Widened JIT
    eligibility past GP-int to the **all-double SSE class** (1-6 `double` params + `double` return):
    `ast_jit_eligible`/`mccjit_last_kgc_ok` admit it (`mccjit_last_allfp`), a new hand-emitted stub
    `mccjit_make_kgc_stub_fp` `movsd`-spills xmm0-7 and routes through `mccjit_kgc_calln_fp` +
    `mccjit_invoke_fp` (fixed `double(double,…)` casts — the compiler places args in xmm), and the differential
    verify compares the **raw return bits** (a faithful recompile is bit-identical; +0/-0 or NaN-bit drift is
    conservatively a mismatch → returns baseline). Test `jit/selftest-fparg`: all-double detect, faithful FP
    verify, a divergent variant (g) caught+flagged, and **end-to-end `mcc -run` dispatch** (`main()=15`,
    fp-stub swapped — validating the movsd stub through its correct `jmp *slot` entry). **DEFERRED:** mixed
    GP+FP signatures and small struct-by-value (≤16B) — both need per-arg register-class routing, which C's
    fixed casts can't express; the clean enabler is promoting mcc's own `classify_x86_64_arg` (x86_64-gen.c)
    from `static` to `ST_FUNC` and classifying at intent-build time, then a class-vector-driven marshaller.
11. **[K9/L2A/L3]** QSBR reclamation — ⏳ EPOCH PRIMITIVE DONE, swap-wiring deferred. Built the QSBR core in
    `src/mccjit_embed.c`: a per-thread epoch registry (`mccjit_qsbr_register`/`_unregister`/`_quiescent`, the
    hot path lock-free) + `mccjit_qsbr_retire(ptr,size)` (tags an old variant with the bumped global epoch,
    leak-and-caps if limbo overflows) + `mccjit_qsbr_reclaim` (frees a retiree once `min(local) >= tag`, i.e.
    every registered thread has hit a quiescent state since the swap). Pointer-swap stays the correctness
    default; **the default is still leak-and-cap** — QSBR only frees once wired, so a bug can never free a live
    variant. Test `jit/selftest-qsbr` (retire→retain, partial-quiesce→retain, all-quiesce→reclaim,
    no-threads→immediate, MT smoke: 8 retirees reclaimed / 0 leaked). **DEFERRED — the integration:** retire the
    old variant on the hot-swap path (needs the old region's ptr/size + MCCState lifetime), the **[L2A]**
    quiescent-point instrumentation (function-entry + loop back-edges of the variant), and un-deferring J5. This
    also lets the L11A `pthread_atfork` child reset the QSBR registry (noted there).
12. **[J10]** hot-patch strategy family — ⏳ 2 MECHANISMS + HARNESS DONE, D3B in-place-codegen deferred. Hot-patch
    is now a benchmarkable family, not one mechanism: `jit/selftest-patch` builds two direct-callable dispatch
    shapes — **pointer-swap slot** (`jmp *[rip+slot]`, swap = one atomic store) and **in-place trampoline
    rewrite** (`movabs rax,imm; jmp rax`, swap = rewrite the 8-byte immediate) — validates both **functionally**
    (initial dispatch + each swap redirects to a new target) and runs the **benchmark harness**: steady-state
    ns/call (direct ≈1.0 vs slot/trampoline ≈2.5 — the ~1.5ns indirection cost), swap-latency ns/swap
    (~0.7-0.8), and code-cache footprint (16/12 B). This is the J10 measurement infra the search consumes.
    **DEFERRED:** the real **D3B** nop-padded patchable-prologue in-place code-patch (AOT prologue emission +
    aligned atomic 5/8-byte patch + icache flush, QSBR-gated via step 11) and int3/trap + dual-map page-flip
    variants; and feeding the measured winner into the dispatcher's per-function/workload ranking.

**Phase 3 — pure-kernel backend + cross-arch + hard-gate**
13. **[J9A/K7/K8]** M5c pure/impure slicing — ⏳ ANALYSIS FOUNDATION ONLY; the backend (largest net-new
    investment) stays deferred. Landed the **partition analysis** `ast_fn_slice_profile` (`src/mccast.c`): it
    profiles a function into `impure_ops` (the C-ABI boundary ops — Store / Invoke / volatile that must stay on
    the ABI), `loads`, and `pure_compute` (the extractable register-value kernel) — a function with many
    pure_compute + few impure_ops is the strong slicing candidate. This is the analysis that the non-ABI kernel
    codegen (K7) and inline-vs-shim (K8) will consume. Test: `jit/selftest-slice` (pure fn→0 boundaries; 1 store
    →1; 2 stores→2; a call→1; loads counted). **DEFERRED — the actual backend, a multi-week project unsuitable
    for an autonomous step (miscompile-risky):** the statement-level slicing *transform* (extract the pure
    kernel, keep impure ops as bound C-ABI calls); **[K7]** the non-ABI register calling convention as a second
    codegen row (`gfunc_prolog` spills all params to frame today — the kernel would keep them in registers +
    carry its own pre/post-Call ABI harness at boundaries); **[K8]** inline-vs-shim as a benchmarked search axis.
14. **[2B]** port the dispatch/stub tail to arm64 — ⏳ MECHANISM VALIDATED + pipeline established; libmcc port
    remaining. Proved the core arm64 JIT dispatch end-to-end under `qemu-aarch64`: a hand-emitted AArch64
    pointer-swap dispatch (`ldr x16,[pc+8]; br x16` reading a swappable data slot) + the **arm64 icache
    maintenance an arm64 JIT MUST do** (`dc cvau; dsb ish; ic ivau; dsb ish; isb` — the reason the tail is a
    per-arch hand-emitted primitive, not shared with x86). Test `jit/arm64-dispatch` (freestanding, built with a
    cross `clang --target=aarch64 -fuse-ld=lld -nostdlib -static`, run under `qemu-aarch64`; self-skips without
    the toolchain). Validation infra now ready: **llvm-mc** for byte-exact AArch64 encodings + **clang/lld/qemu**
    for execution. **DEFERRED — the interdependent libmcc port** (all one arm64 convention): the mode-6 slot
    emission in `src/mccast.c` (the arm64 `ldr x16,[slot]; br x16` prologue + slot in `.data` + icache flush) and
    the `mccjit_embed.c` stubs (`mccjit_make_trampoline`/`_counter_stub`/`_kgc_stub_n`/`_kgc_stub_fp`, currently
    `#if __x86_64__`-only) as `#elif __aarch64__` branches, then run the jit selftests on an arm64 libmcc build.
15. ✅ **[7A]** promote `eval_slice` to a hard per-strategy gate — DONE (opt-in; default-on flip awaits the soak).
    The UB-soundness oracle now runs in **production** (not just shadow builds) and, under the opt-in
    `MCC_AST_JIT_EVAL_GATE`, **refuses** an unsound spec-slice — discards the speculative clone and falls back to
    the unspecialized dispatch (always correctness-safe: never emits an unsound variant). Shadow builds still
    hard-`abort` on divergence. `MCC_AST_EVAL_FORCE_UNSOUND` is a test hook; `ast_jit_eval_refused_count()`
    exposes the refusal tally. Test `jit/selftest-evalgate` (forced-unsound→refused+correct; sound→not
    over-refused; gate-off→no refusal). **Remaining (the flip, not the code):** flip the gate default-on after
    **N := 3** clean self-host + fuzz soaks (measuring the false-positive/over-refusal rate). Its cross-arch
    signal is the **JIT-capable** arches (x86_64 + arm64-via-2B), so it rides 2B, **not** 6A — riscv64 has no JIT tail, so
    the riscv64 self-host gap (step 16) is orthogonal to the JIT hard-gate.

**Phase 4 — shared foundations the JIT default-on + AOT-convergence need** *(also the standalone optimizer gains)*
16. **[6A]** close the riscv64 Tier-3 self-host gap → makes the M8 cross-arch gate real (validation-infra unblock).
17. **[3A]** fix the emit-time value-axis framework (keystone: unblocks §22/§23/inline-promote/M1 scoring).
18. **[4B]** backend-parity session (cmov/csel + temp-materialization) → clears ABS, DIVMAGIC, 64-bit div-magic,
    branchless-select; then flip the held P0 items.
19. **[5A]** memo unification (`ComboMemo` + disk) — the one content-addressed store the AOT and JIT searches share.
20. **[P1]** the unified value lattice → feeds the **AOT-static sink scorer**.
21. **[AOT-static sink scorer / L1B]** deterministic cost/size scoring + static-analysis ranges + gain-ordered,
    memo-pinned search — the AOT side of "AOT `-O4` *is* the JIT" (`-O4` stays off the byte-identity bar).

**Phase 5 — const-data + the JIT data path**
22. **[P2]** const-data rewrite (M5 → M6).
23. **[K6/L8A]** data→code substitution (compile-time, via a synthetic ctor; needs P2/M5).

**Parallel / non-blocking:**
- **[CST] 1A** — slice-G stitching → `-g`-from-provenance. Independent of the optimizer path; run whenever
  attention frees after the JIT phases.
- **P0 ready-batch flip** — near-zero resistance, orthogonal; flip already-cleared gates (`NARROW_ELIM`, `VLAT`,
  `ARGFWD`, `SETHI_NARY`, `SPILL_SHARE`, `INLINE_PASS`, `CYCLE`, `COLOR`) in batches as soak time accrues, not
  gated on a phase. The *held* items (DIVMAGIC/ABS) unblock at step 18 (4B).

### The seven strategic decisions (rationale for the phase-4/5/CST steps)

- **1A — CST gets a real consumer.** slice-G stitching → `-g`-from-provenance (also feeds the debugger suite).
  Promotes CST from `[DEFER]` to active **[CST]**, independent of the optimizer path.
- **2B — port the JIT stub tail to arm64** (step 14). Reinterprets D7 as "x86_64-first," not "only." — ⏳
  dispatch mechanism + icache-flush validated under qemu-aarch64 (`jit/arm64-dispatch`); llvm-mc + clang/lld/qemu
  pipeline ready; the mccast.c mode-6 slot + mccjit_embed.c stub arm64 branches are the remaining port.
- **3A — fix the emit-time value-axis framework** (step 17). Keystone: clears §22 promotion, §23 inline budgets,
  M1 scoring, inline/promote in the search.
- **4B — backend-parity session** (step 18): cmov/csel + temp-materialization; clears ABS/DIVMAGIC/64-bit-divmagic/
  branchless-select together.
- **5A — memo unification** (step 19): one `ComboMemo` + disk store, retiring the out-of-process superopt engine.
- **6A — riscv64 Tier-3 self-host** (step 16): validation-infra unblock; makes the cross-arch gate real.
- **7A — `eval_slice` hard gate** (step 15): N := 3 soaks, rides 2B. — ✅ gate DONE (opt-in
  `MCC_AST_JIT_EVAL_GATE`, refuses unsound specs; `jit/selftest-evalgate`); default-on flip awaits the soaks.

### P0 — Default-on sweep · *near-zero resistance; parallel/non-blocking* — ready-batch interleaves anytime; held items at step 18 (4B)

Flip knobs that already cleared the full M8 bar from opt-in to default. Gain lands on every compile; no new
code, only golden churn. Also shakes out the search vocabulary before P1..P3.

- **HELD opt-in (default `0`):**
  - `MCC_AST_DIVMAGIC` — **blocked by an x86_64-specific self-host miscompile**: default-on at `-O2`, stage2
    (compiled with DIVMAGIC) **SIGSEGVs recompiling `src/mcc.c`** (deterministic stack-overflow via runaway
    self-recursion). NOT a wrong quotient — a **non-local x86_64 mul-high (`imul; shr $0x20`) register-pressure
    codegen bug** that only manifests on a heavy TU (i386/arm64 self-host + the exec corpus + `-g` builds are
    clean). Fix the 64-bit/mul-high x86_64 register allocation (the "optimal 1×-multiply form" — needs a real
    temp-materialization mechanism) before flipping — **scheduled as the 4B backend-parity session**.
  - `MCC_AST_ABS` — held on a perf judgment: its branchless bit-trick (`(x^(x>>31))-(x>>31)`) vs a
    well-predicted branch is a genuine tradeoff, and gcc's chosen form is `neg;cmovs` (cmov), which mcc lacks.
    Revisit when the cmov backend lands — **the 4B backend-parity session** (cmov/csel emission).
- [ ] **Next default-on batch (campaign endgame)** — after broad exposure/soak, flip the opt-in gates that
  already cleared the full bar: `MCC_AST_NARROW_ELIM`, `MCC_AST_VLAT`, `MCC_AST_ARGFWD`, `MCC_AST_SETHI_NARY`,
  `MCC_AST_SPILL_SHARE`, `MCC_AST_INLINE_PASS`, `MCC_AST_CYCLE`, and `MCC_AST_COLOR` (its self-host miscompile
  is fixed, 556de5c2, but it needs a post-fix soak). DIVMAGIC/ABS stay held.
- Not in P0: the inline/promote value axes — they want emit-size scoring (needs §22 scratch-`Section`
  isolation) **and the emit-time value-axis framework is currently unsound** (both inline and promote axes
  fail 4/296 and 3–12/296 on the corpus — see the §22 promotion-axis item). They stay `[FLOAT]`, blocked.

### P1 — Unified value lattice · *master-order step 20; feeds the AOT-static scorer* — **resolves Fork L**

Build §29 range/known-bits and `context_in`/`context_out` as **one** artifact with two projections, not two
lattices. One integer value-domain lattice over locals, mining the dominating-`AST_If` predicate source; §29
reads the narrowing-residue projection, `context_in` reads the reaching-context / memo-key projection (a 4th
side-car predicate-vector reads a third view later). Hooks the existing `ast_du_*` / `ast_hash_*` epoch
machinery. The side-car (`AstVLat`), both projections (`ast_vlat_narrowing`/`ast_vlat_context`), the
region-scoped per-use projection (`ast_vlat_context_at`), and the first value-changing consumer (path-
sensitive narrowing) are landed gated `MCC_AST_VLAT`, byte-neutral by default.

**Remaining:** the memo-key context consumer, the predicate-vector 4th index, and **PR-C** (loop-IV
monotonicity widening, the §32a core) — spec'd + **held** (miscompile-sensitive; its validator, the
differential fuzzer, is x86-only). Details in the `[P1] context_in / context_out` item. Comparison-operand
narrowing is DROPPED as backend-redundant.

- **Unblocks in one build:** §29 non-distributive narrowing (`/ % << >>`), §29 outer-narrow elimination,
  V-cprop(c) known-bits/range variant, V-cse(c) redundant-load elimination — and pre-pays `eval_slice`'s
  enumeration bound for P3.
- **Resolves Fork L:** *same lattice, two projections.* "Shares representation, differs in scope."

### P2 — Const-data rewrite · *master-order steps 22–23 (with K6/L8A data→code)* — *root-blocker clear*

The `AST_Data` kind + the size-preserving in-place re-emit primitive (`ast_data_reemit`) are landed on the
section-level side-car (`ast_hook_data` fires at TU scope, no per-function arena). **Remaining:** the
size-CHANGING datacomp rewrite = M6 (C) `.init_array` decompress ctor + (D) `__mcc_decompress` runtime (multi-
backend, breaks link-time-constant consumers) + M4(b/c) score-fold. Details in `[P2] M5`/M6.

- **Decouples the M4↔M6 circularity:** once a transform *owns* a candidate's bytes the data-delta is per-
  candidate and transform-attributed, so M4(b/c) folds into the score without shared-rodata order-noise.
  Direction: M5 rewrite node → M6 owned delta → M4 fold.
- **Also unblocks:** §30 value-table dispatch.

### P3 / JIT — **now the lead (master-order Phases 1–3)** · *capability milestone*

Core shipped as §26 M1–M3 (baseline retention + machine-byte-splice entry dispatcher + non-null speculative
specialization + `--jit-functions`), search-selectable via gate bits 40/41; **M6 (pool) also landed** (commit
457ca8a1). **This session prioritizes the JIT first:** the remaining tail (M4/M5/M5b/M5c/M8 + the J*/K*/L*
decisions) is sequenced as master-order **Phases 1–3** and detailed in **NEXT MILESTONE (§26)** below — that
section is the JIT rationale + milestone status; the *order* is the master list above.

### [5A·ACTIVE] — Substrate unification · *now active (was FLOAT); maintenance gain* — **resolves Fork C**

Finish M1–M3 / M7 leftovers: `ComboMemo` + MSZ1 as the one memo, one eviction, one key
(`ast_search_key_salt` ≡ `so_pf_key` already converged). Retires the out-of-process superopt's second engine.

- **Resolves Fork C:** adopt **whole-file rewrite** (the working in-proc model); add a `claim` sub-record to
  the MSZ1 container *only if/when* distributed work-stealing is actually wanted. Not on the capability path —
  slot it as a palate-cleanser, never ahead of P1/P2.
- Rolled-in sub-decisions: the int-axis vocabulary (budgets/levels with no gate bit) — quantize into
  `AstGateMask` bits vs. a new `combo_run` parameter dimension; M7b `jit.h` graduation stays `[DEFER]`.

### [DEFER] — after P1–P3 land

- **Backend parity vs gcc** — cmov/csel branchless select, 64-bit div-magic (needs `mulh`/`__int128`),
  boolean-normalizing ternary. High per-compile gain but a 5-backend grind + a missing primitive; pick up
  opportunistically per-backend.
- **§28 rewrite-rule IR** — Explore-tier; gate behind P1–P3.

### Campaign queue — JIT/AST autonomous campaign

Per-item pattern: implement gated behind a new env (default OFF → default byte-identical); validate the
gated-ON path to the full M8 bar; independently re-verify firing (a quick throwaway test often does NOT fire
the pass — confirm via `-v128` TRACE or an object-diff) plus correctness vs gcc; commit; update TODO.

- [ ] **1.** §24 hot-slice ranking (uses the landed `ast_loop_depth`; **BLOCKED — no consumer yet:** the
  search budget is applied per-function at full value in `ast_func_end`, so a hotness ranking is inert until
  cross-function budget *allocation* exists — needs §22 emit isolation; do §22/M2 first) · §32a widening
  dataflow · §30 value-table dispatch (needs the P2 `.rodata` project) · FLOAT combo M2/M3 (search-infra) ·
  V-* strategy-decomposition follow-ons · the §26 marginal tail (float/struct KGC args, static-link E1a,
  bitfields, M7 patchpoint). (**Host note:** the §26 JIT tail is x86_64-ELF-only, D7.)
- [ ] **2. Endgame:** flip the validated gates default-on — the P0 "next default-on batch" item.

---

## AST substrate + unified optimizer · [FLOAT reference]

Collapse the three optimization drivers (the `ast_func_end` pipeline, the §22 `AST_PF_EMIT` trial, the
`mcc.c` out-of-process search) into one side-car substrate + one memo + one strategy engine, shared by the
AOT backend and a live JIT. This reframes/subsumes several items below (§21 cache key, §22 emit isolation,
§28 rewrite IR, §33b/e seam+window keys, §30 predicate bitset, H_e epoch hash, the time-budgeted engine,
per-function `-O1`, PP-as-executable JIT). The staged rollout (naming partition, three side-car indices,
strategy engine, live `-O4+` search) is in place; `-O1..-O3` never search and stay byte-reproducible. Runtime
JIT + guarded deopt is the separate post-rollout milestone (NEXT MILESTONE below).

Open scoring/parallelism continuations:

- [ ] **Step 5+ — emit-size scoring under the *tick* scheduler + JIT-runtime scoring** — emit-size scoring is
  run-to-completion per candidate today because the fair-interleave tick scheduler thrashes the shared
  ltemp/fconst emit state across candidates; making it tick-interleavable needs per-context emit state (the
  C11-thread item). JIT-runtime scoring — wiring the shipped `MCC_AST_JITSCORE` runtime measurement into the
  ranking key — is the other half. (needs §22 scratch-`Section` emit isolation)
- [ ] **Step 5+ — C11-thread pool with `_Thread_local` per-context state** — the fork pool (COW isolation)
  already covers candidate *scoring* with no thread-local marking, so this is only needed for interior /
  tick-mode parallelism. Its own gated change (side-car shadow + fixpoint + fuzz).
- [ ] **Step 5+ — widen the search space** — the candidate set is the subset lattice of
  `searchable = base | opt-in-knobs`. **Still open:** the **inline/promote axes** (want emit-size scoring —
  inline/promote effects are emit-time; **unblocked by the 3A framework fix**), and the search-mode superopt
  shadowing (templates-gated knobs only fire in perfn mode → M3 wiring, done under 5A).
- [ ] **Step 5+ — disk-backed cross-build memo (refcounted, LFU-evicted, compressed)** — the per-function
  winner persists across builds in the compressed "MSZ1" whole-file container, evicting the lowest-refcount
  quarter at the shared 10 GiB cap. **Still open (M2/M3):** unify with the out-of-process `pf-*.ck` format so
  the in-process search fully subsumes `mcc_superopt_perfn`; raise `AST_SEARCH_MEMO_CAP` if the 4096-entry hot
  set proves too small; throttle the per-accessor dir-walk on very large caches.

### Substrate indices/analyses designed but not built · [P1 reference]

The rollout built three of the four planned side-car indices (`ast_hash_*`, `ast_du_*`, `ast_memo_*`) plus
the strategy engine and search. These have no symbol in `src/` today:

- [ ] **[P1] Predicate-vector projection — the 4th side-car index** — a packed bitset of tested-predicate
  truths over ≤8 named slots in a window (the `predicate_vector(cursor, keys≤8) -> bitset` verb), the semantic
  sibling of the structural hash, for **branch coalescing** — generalizes `ast_bf_run` (V-bf) + the §30 value-
  table dispatch. Distinct from the §30 *transform*: this is the index it would read.
- [ ] **[P1] `context_in` / `context_out` value-domain fact lattice** — the value-domain restriction on
  live-in slots: a bounded backward walk collecting the equality/range predicates of dominating `AST_If`
  conditions, O(fixpoint) first / O(1) warm. It is the checker's enumeration bound (`eval_slice`, §26 Stage 4)
  and the memo's *context* key. The unified `AstVLat` side-car, the whole-function projection
  (`ast_vlat_context`), the region-scoped per-use projection (`ast_vlat_context_at`), and the first region-
  scoped consumer (path-sensitive narrowing) are landed gated `MCC_AST_VLAT`. **Remaining (PR-C — the §32a
  core, MISCOMPILE-SENSITIVE, held):** admit loop-carried IVs to `ast_vlat_context_at` so a body use of
  induction var `i` gets the guard-derived range (`i < N` → `i ≤ N-1`). **Soundness precondition:** apply the
  loop bound to an IV body use ONLY for op-3/op-5 for-loops (single IV write is the `incr` clause → body has
  zero IV writes → every body use sees the guarded value → sound); op-2/4 (while/do-while) write IN the body,
  UNSOUND unless the use provably dominates the write (defer, or add a dominance check). Honor strict-vs-non-
  strict bounds; const bounds only (`AST_LOOP_BOUND_CONST`) for the first cut; the lower bound needs an init
  field (`AstLoopInfo` has stride but not init) + monotonicity. **Validation gap:** the differential fuzzer is
  x86_64-Linux-only and SKIPS on arm64/mac; no whole-function meet baseline for a written local, so the ⊆-meet
  shadow assert does NOT cover IVs (needs an IV-specific oracle). **Keep gated OFF until the x86 differential
  fuzz soaks clean.** Then feed `ast_vlat_context` into a memo/`eval_slice` key. Overlaps but is not §29.
- [ ] **[P1] Descendant-indexed (DFS enter/exit) def/use extension** — so the two *subtree-scoped* write
  queries `ast_licm_written` (cse/licm) and `ast_ivsr_count_writes` (ivsr) become O(1) table lookups. The
  whole-function `ast_du_*` table subsumes only the two whole-arena scanners; "written under node n" needs a
  descendant range index. Both remain recursive subtree walks today.

### Macro roadmap — collapse both searches + const-data onto one substrate · [M1–M3/M7/M7b = FLOAT · M4–M6 = P2]

Grounded by two audits: (i) the out-of-process superopt duplicates **every** concern of the in-process
`ast_search` on a second substrate; (ii) the substrate target (`src/mcccombo.h`) and its four migration
call-sites already exist. Order is dependency order (M4 before M6; M5 before M6).

- [~] **[FLOAT] M1 — live -O4 search on `combo_run`** — core landed (subset mode; order-honoring emit +
  row-order search + memo order persistence; `MCC_AST_CYCLE`; the arena inliner PR-1 `MCC_AST_INLINE_PASS`;
  the DFS/BFS/PRODUCT traversal walks — all default byte-neutral). **Remaining:** `ast_fc_forecast` best-first
  ordering (the open M1(c) synergy); a *scoring* gain needs a pass whose reordering changes cost/size — the
  future inline/promote (D6, gated on §22 isolation + the unsound-framework fix) or a size-scored reassoc;
  inliner PR-2+ (callees with LOCALS + control flow — §34b-risky frame-offset + label/switch/break-continue
  remap — plus struct-return / const-arg specialization); sequence-with-repetition encoding; runner-as-strategy
  + memo identity (D2b); the unified score/forecast estimator (D4/M7); all-opts-as-strategies (D6, gated §22).
- [ ] **[5A·ACTIVE] M2 — unify the memo on `ComboMemo` + disk backing.** a) key = `ast_intention_hash`; b) value =
  winner record stored best-of-3 compressed (the "MSZ1" logic moves into `ComboMemo`); c) refcount + LFU
  eviction under the shared 10 GiB cap. The version/triplet salt (`ast_search_key_salt`, FNV over
  `MCC_VERSION_STR` + `MCC_CONFIG_TRIPLET`) is landed. **Remaining:** the `ComboMemo`-struct migration (a)+(c)
  — the current disk memo is the hand-rolled `AstSearchMemo`/MSZ1 path, not yet the `ComboMemo` type. *Synergy:*
  the shadow oracle `MCC_CONFIG_AST_SHADOW` validates a cache hit == recompute.
- [ ] **[5A·ACTIVE] M3 — subsume the out-of-process superopt** (`mcc_superopt_perfn`/`mcc_superopt_search`,
  `mcc.c:922/1053`) onto the substrate. a) map perfn `{1,3,7}` config bits and the search 3-axis int product
  into the `sel[]`/gate vocabulary; b) fold `pf-*.ck`/`so-*.ck` into the compressed container; c) reconcile
  concurrency — per-key `flock` + claim-cursor work-stealing (`so_claim`) vs the memo's whole-file rewrite.
  The record fields (`score`/`tried`) and the lossless config↔gate mapping (`src/mccgate.h`, selftested via
  `tools/asttool.c`) are landed but NOT yet wired into a unified search. **Remaining:** that wiring + the
  `budget` int-axes (node/graft/bitflag levels, which carry no gate bit); expose `tried` in ordering.
- [ ] **[P2] M4 — extend scoring to data/rodata.** a) snapshot `data_section`/`rodata_section` offsets before
  replay and diff after (the `ast_search_emit_size` hook + `-v128` TRACE is landed); b) combined score = text
  delta + data/rodata delta; c) add a data-size term to `ast_cost_score`. **M4(b)+(c) score-folding is
  DEFERRED with a measured reason:** the replay re-emits `.rodata` float constants shared across candidate
  clones (`ast_fconst_reuse` no-op under replay), so the per-candidate rodata delta is order-dependent noise;
  folding it changes selection unfairly, and an attempt that *restored* the offset **miscompiled**. Score stays
  **text-only** until M6's data-**rewrite** provides a real per-candidate delta. The snapshot must **not**
  rewind data/rodata (shared, deliberately grown).
- [ ] **[P2] M5 — const-data emission foundation.** The visibility side-car (`ast_hook_data`), the `AST_Data`
  kind, and the size-preserving in-place re-emit primitive (`ast_data_reemit`) are landed. **Why `AST_Data` is
  NOT a per-function node:** `ast_hook_data` fires at parse time for TU-level globals/statics where there is no
  per-function `ast_cur` arena, so the rewrite operates on the section-level side-car (`ast_data_recs`). A
  future TU-level data-node home is the remaining structural piece if the search/replay lifecycle ever scores
  data rewrites per candidate. **Remaining (the actual datacomp rewrite):** a *size-changing* rewrite needs M6
  (C) `.init_array` ctor + (D) `__mcc_decompress` runtime. *Synergy:* also unblocks §30.
- [ ] **[P2] M6 — datacomp: const-data compression pass** (codegen-layer, opt-in; **not** an AST strategy).
  **(A) Target:** string literals · `static const` arrays · both; threshold by size×entropy. **(B) Codec:**
  per-blob best via `combo_pack`, or `combo_pipeline_search` for a chain. **(C) Decompression:** eager
  `.init_array` ctor · lazy first-use guard · both. **(D) Runtime:** new `__mcc_decompress` in `runtime/`, call
  via `vpush_helper_func`+`gfunc_call`. **Blockers (audited):** breaks link-time-constant consumers;
  `const`→writable `.bss`; multi-backend ctor synthesis (all 5 arches). The candidate-ID analysis
  (`ast_data_estimate`) + round-trip gate (`ast_data_roundtrips`) are landed. **Remaining:** the actual (C)
  ctor + (D) runtime, which need M5's non-neutral rewrite. **Gate:** off; fires only when M4 says it net-shrinks.
- [ ] **[P0] M6z — zero-init `.bss` placement** — landed default-on at `-O2+` (`MCC_ZERO_BSS`), guarded to a
  provably-safe subset (initializer emitted no relocation is the critical guard). **Remaining:** TLS
  `tdata`→`tbss` and the asan/bcheck cases (excluded by guards today).
- [ ] **[FLOAT] M7 — formula-family unification** (long tail). a) expose cost/ratio formulas as fold-math
  builtins; b) make the forecast ensemble a first-class `combo` formula family; c) one `-f` front — extend
  `fold-math` or add a gate. *Synergy:* one enumerator over {strategies, predictors, codecs}.
- [ ] **[DEFER] M7b — graduate the disk search-memo into compiled-in strategies** (`cache` →
  `src/algorithms/jit.h`). A new `tools/` utility + CMake target reads the shared cache dir and materializes
  each hot memoized winner as a `jit_graduated_table` entry, registered in `ast_strategies[]` so a discovered
  gate config ships compiled-in. **Open questions:** (a) gate-mask replay (v1) vs synthesizing a new
  `AstStrategy.apply`; (b) key stability (version/triplet salt in `jit.h`); (c) the removal step's verification
  gate; (d) when the tool runs. *Synergy:* the AOT dual of the §26 runtime JIT. Gated by M8.
- [ ] **[guardrail] M8 — validation gates** (= the coverage-ledger bar; apply to each of M1–M7). Behavior-
  preserving steps (M1 subset, M2, M3) stay byte-identical; M4–M7 gated opt-in, change bytes only under flag.

### Strategy-variation catalog — widen the search vocabulary · [P0 default-on candidates + FLOAT]

Of the 20 `ast_strategies[]` rows, most implement a single algorithmic variation. Each variation below is a
candidate **search knob** — a distinct `AstStrategy` row or a per-strategy parameter. The M1(c) precondition
applies to any *ordering*/*pipeline* variant: the emit path must honor the discovered per-fn order.

**Holds (do NOT re-attempt):** `licm` core is not separable from `cse` (`ast_licm_at_loop` reads the LIVE CSE
availability window at the exact walk position); `cprop`+`sccp` stay FUSED (joint fixpoint); per-node-bundle
row-splits are non-neutral. Governing distinction: a **gate-split** (per-family `if(gate)` inside one pass) is
byte-neutral by construction; a **row-split** (new reorderable row) is byte-neutral ONLY for an independent
whole-arena pass.

- [ ] **V-bfold** (`ast_bfold_run`) — **remaining under (a):** `fmod` needs a real exact-remainder kernel;
  `nearbyint`/`rint` need the (d) rounding-mode gate; `ldexp`'s `int` 2nd arg doesn't fit the same-btype `ab[]`
  loader; `pow/exp/log/sin/cos/hypot` fold in `-ffold-math` — don't duplicate. b) `fma` DROPPED. c) `fmin(x,+inf)`
  etc. UNSOUND for NaN; `copysign(x,C)` DROPPED. d) `FLT_ROUNDS`/errno gate for `-frounding-math` (open).
- [ ] **V-ident** — a) strength reduction backend-redundant — skip; b) fast-math-gated float identities; d) a
  worklist/BFS ordering variant.
- [ ] **V-narrow** — b) replace the type-width heuristic with demanded-bits/known-bits; c) comparisons DROPPED
  (backend-redundant). (`/ % << >>` narrowing landed gated `MCC_AST_VLAT`.)
- [ ] **V-cprop** — a) promote the join/per-block choice to a first-class strategy pair; b) copy propagation;
  c) known-bits/range lattice variant.
- [ ] **V-cse** — a) hash-based value-numbering (LVN/GVN); c) redundant-load elimination (needs the §29 lattice).
  (join/comm/window knobs landed.)
- [ ] **V-licm** — a) discover loop-invariant subexprs directly; b) fixpoint + hoist to outermost level; c)
  preheader creation + hoist invariant loads/stores. Caveat: `licm` folds are counted inside `cse`.
- [ ] **V-dse** — a) global backward-liveness across blocks; b) partial-dead-store; c) track stores across
  `AST_If`/loop children. (see-through-calls landed default-on.)
- [ ] **V-sccp** — a) **true** SCCP (constant lattice + CFG-edge worklist); b) switch/computed-branch folding.
  (cprop+sccp fixpoint fusion landed default-on + wired as `AST_SG_SCCPFIX`.)
- [ ] **V-jt** — a) real jump threading through a determining predecessor; b) duplicate-condition threading;
  c) correlated-condition threading; d) hammock merge.
- [ ] **V-bf** — b) windows >64 via multi-word masks; c) `switch`→jump-table/bitmask sibling; d) perfect-hash
  for sparse sets. (range predicates landed default-on.)
- [ ] **V-sethi** — a) extend the leaf-aware metric to memory-vs-register refs; b) full Sethi-Ullman labeling;
  c) reassociation to rebalance associative chains; d) deterministic tie-break when `l == r`.
- [ ] **V-tco** — a) break param cycles via temporaries; b) general/sibling tail calls via a tail-call ABI;
  c) float/struct params (int+pointer landed); d) tail-recursion-modulo-accumulator.

### Confirmed backend codegen gaps vs gcc · [4B — SCHEDULED: one backend-parity session]

**4B bundles these three into one dedicated per-backend session**, because they share two missing primitives —
**conditional-move emission** (cmov/csel, absent from every backend's codegen) and a **temp-materialization
mechanism** (Store-to-fresh-local + Loads). Landing those clears branchless-select, the signed/`a==1` divmagic
form, and — with the x86_64 mul-high regalloc fix — unblocks `MCC_AST_ABS` and `MCC_AST_DIVMAGIC` default-on.
Grind = 5 backends (x86 `cmov`, arm64 `csel`, riscv branchless-arith fallback; per-arch `mulh` for 64-bit).

- [ ] **Branchless select for min/max/abs/sign** (`cmov`/`csel`). **Measured:** mcc emits compare + branch;
  gcc emits `cmovle`/`cmovge`/`neg;cmovs`. **mcc's code GENERATOR emits no `cmov` on any arch** — `cmov`
  appears only in the disassembler/assembler. Needs new conditional-move emission per backend (x86 `cmov`,
  arm64 `csel`, riscv branchless-arith fallback), plus a safe-to-cmov analysis. Also blocks re-enabling
  `MCC_AST_ABS`.
- [ ] **Branchless boolean-normalizing ternary `cond?1:0`** (frontend codegen, NOT an AST fold). `expr_cond`'s
  `is_cond_bool` fast path lowers via branches AND returns before `ast_hook_ternary_end` — so these ternaries
  DESYNC and the AST optimizer never captures them. Fix: materialize the condition branchlessly (`setCC`/`cset`).
  Target-sensitive, churns goldens; incidentally fixes the AST-desync.
- [ ] **Constant integer division/remainder strength reduction** (magic-number multiply). 32-bit landed
  (`src/mccmagic.h` + `ast_divmagic_run`, `MCC_AST_DIVMAGIC` opt-in; ⚠ NOT default-on-ready — P0). **Open:**
  (a) **64-bit** — needs the HIGH 64 bits of a 64×64→128 product (`mulh`), which mcc's type system can't
  express (`__int128` is a parse error) — a per-backend primitive (x86_64 `mulq`, arm64 `umulh`/`smulh`, riscv
  `mulhu`, i386→runtime helper). (b) the **optimal 1×-multiply form** for the signed / `a==1` cases → needs a
  real temp-materialization mechanism. **⚠ Cross-arch validation caveat:** `cmake-qemu-*` emit native x86_64;
  use `cmake-cross/mcc-i386` and `cmake-cross/mcc-arm64` for real cross-arch checks.

## NEXT MILESTONE — runtime JIT + guarded deopt (§26) · [core COMPLETE — M1–M3 + M6 done · remaining = tails + M7]

Entry-guarded variant dispatch with a runtime recompiler + hot-swap. **This is the session's lead workstream
— its execution ORDER is the master list above (Phases 1–3); this section is the milestone status + the
J*/K*/L* decision rationale.** **Critical path M1 → M2 → (M3) → M4 → M5 → M6**, with M7/M8 attaching
independently after M2. **M2 alone is a shippable, complete guarded-deopt JIT; M4 is the size/build gate.**

**Baseline & cache model.** The JIT *baseline* is the AOT-compiled function that ships in the object (final
emit at the chosen `-O`), NOT the pre-fold body. At runtime the JIT produces a *further*-optimized variant
specialized to an observed context, keyed by a hash of that context; the cache maps `key → best-known
variant`, and the dispatcher **deopts to the AOT baseline on guard-fail / key-miss**.

**Global gate `MCC_AST_JIT` (default off)** until the full validation bar passes, then a P0-style flip. Build
gate `MCC_EMBED_JIT` (default off) adds the ~800 KB embed. **The runtime dispatch/stub tail is x86_64-ELF-only
hand-emitted machine bytes (D7), validated on Linux/x86 CI only — not the arm64-macOS dev host; the recompile
engine underneath is cross-arch. Supported signatures: 1–6 GP int/ptr args, non-FP/non-struct return.**
**2B (scheduled after the x86_64 tails close):** give the mode-6 slot / KGC stub / trampoline / counter an
arm64 emission path so §26 validates on the dev host — reinterpreting D7 as "x86_64-first," not "only." This
is the prerequisite for a meaningful cross-arch 7A hard-gate and for any JIT default-on flip.

**Architecture — the JIT is mostly Strategy objects, not a separate subsystem.** The compile-time pieces are
(optionally) new rows in the same `ast_strategies[]` table the search consumes; only a thin runtime remains.
Stage 1 shipped via **mechanism B — machine-byte splice** (the deopt arm reinstalls the retained AOT baseline
bytes with rebased relocations), NOT the AST-level rows; those rows stay optional (gate bits 40/41).

**Unifying principle — AOT `-O4` *is* the JIT (locked this session).** The AOT `-O4` search and the runtime
JIT are the **same engine over the same strategy pool**; the only difference is the **output sink** — AOT
emits the winning variant as static code that ships in the build, the JIT hot-swaps it at runtime. `mcc` the
backend does **not** run a JIT "alongside" AOT: at `-O4` (or `--jit`, or a JIT-default target) the optimizer
*is* the JIT, run to a static-emit sink. The JIT only runs because the backend was given `-O4` or the frontend
`--jit` (or the target defaults JIT-on). Corollaries (they govern every K-decision below):

- **Nothing is binary.** Every choice — signature/marshalling coverage (K4), C-ABI-vs-register calling
  convention (K7), inline-vs-shim (K8), how-to-hot-patch (J10), quiescence (K9) — is a **strategy row** the
  permutations×combinations search enumerates under sane per-platform limits, and **benchmark-always-wins**
  decides promotion. There is no fixed answer; there is a pool and a scorer.
- **The search space is code+data jointly**, not code alone. The const-data rewrite (P2/M6), the exhaustive
  data cache (K6), and the KGC poison set (K2) are all inputs the search consumes.
- **Promotion gate = best-of-3 self-benchmark (K5).** A candidate is promoted over the incumbent only after it
  passes the range/soundness sanity tests AND wins a best-of-3 benchmark against the currently-selected variant.
- **Compound paths emerge (K2).** A 99%-match specialized variant + a switch-table covering the 1% poisoned
  (code+data) misses is a *compound* optimization the search should discover and promote when it benchmarks
  better than the previous best.
- **The one seam (RESOLVED): the *scorer* is sink-dependent — same engine, same pool, different objective +
  different range source.** The runtime-JIT sink uses a **wall-clock** best-of-3 benchmark (K5) fed by
  **runtime-observed** live-in ranges (J6A). The AOT-static sink **cannot** use wall-clock without breaking the
  M8 byte-identical self-host bar, so it scores with a **deterministic cost/emit-size model** and derives its
  ranges from **static analysis / const-folding** (the `AstVLat`/§29 lattice min/max) instead of runtime
  observation. This is opt-in via `-O4+` and is **explicitly "known to be less effective than runtime values,"
  but must be no worse than gcc/clang inferring ranges by the same/similar static methods** (the parity bar).
  To spend a limited compile-time budget well, the AOT sink **sorts the strategy pool by expected gain — biggest
  known gains first** (gain-ordered, time-budgeted scheduling; consumes the `ast_fc_forecast` best-first work of
  M1(c) + the §24 hot-slice ranking + §31 beam). So "one engine" is literally true for the *strategy pool and
  search*; the *objective function and range source* differ by sink — and that difference is exactly what keeps
  the AOT build byte-reproducible while the runtime JIT stays free to benchmark. **New work item below:
  "AOT-static sink scorer."**

**Reusable infra (verified grounding).** `-run` compile-to-executable-memory (`mcc_run`, `mccrun.c`;
`host_runmem_alloc` RWX / W^X dual-map + `host_runmem_protect` + `host_icache_flush`) + `mcc_relocate` (rejects
double-relocate); D3=A entry dispatcher sidesteps the static `E8 rel32` problem — call sites unchanged, the
dispatcher reads a swappable data pointer flipped by one aligned 8-byte atomic store; `.init_array` ctor
emission wired; C11 `<threads.h>` is a real pthread shim; entry-prepend prior art = `ast_tco_run`.

**Resolved this session — JIT forks (J1–J10, folded into the milestones below):**

- **J1A — mismatch = invalidate → permanent-deopt for that key**, discard the whole variant after K distinct-key
  mismatches. **The KGC invalidation persists to the `mmap`'d on-disk cache** so a future run/compile with
  matching code+data inherits the "known-bad variant" verdict instead of re-learning it. (M5b)
- **J2B — close the silent unverified path.** Restrict JIT eligibility to the verified GP-int signature set and
  **refuse to JIT** everything else (no unverified direct-trampoline fallback). Extend the marshaller to SysV
  SSE + small struct-by-value later (then those become eligible). (M5b)
- **J3A — build the per-sym blob registry + one generic ctor** → live in-program recompile (clears M4 item 3). (M5/M4)
- **J4A — static-link `libmcc.a` into the embed (E1a)**; accept ~800 KB, validate Tier-B; kills the dynamic dep
  + the `libmccrt.a not found` wart. (M4)
- **J5 — DEFER reclamation** until memory usage becomes an issue (no interim bounded pool). (M5)
- **J6A — build the `jit-profile` row as the D5 hot-counter's co-instrumentation** (runtime range capture rides
  the existing counter) → makes mode 5 bite.
- **J7A — value-range is the next speculative fact** (W2.3), after J6 supplies the runtime range source. — ⏳
  const-collapse DONE (`mccjit_profile_pick_const` + `mccjit_recompile_profiled`: a param observed constant in
  the J6A profile is speculatively const-folded, KGC/poison-guarded; `jit/selftest-vrange`); general range
  narrowing deferred to the P1 VLat consumer.
- **J8B — refuse-to-JIT bitfield/FAM-bearing fns now** (cheap gate); serialize them later, low priority. (M4)
- **J9A — build the M5c pure-kernel path** (statement-level pure/impure slicing + the off-C-ABI register calling
  convention). Promoted from deferred to active.
- **J10 — hot-patch is a STRATEGY FAMILY, not one mechanism.** The JIT should implement many *how-to-patch*
  strategies (pointer-swap dispatcher, D3B nop-pad patchpoint, and further variants) as search-selectable rows,
  and a new item benchmarks/profiles permutations of them (see the "hot-patch strategy family" item below).

**Resolved this session — JIT tier-2 forks (K1–K8; K9 open):**

- **K1C — poison is ratio-based**, not a fixed count: a variant is discarded when its mismatch/hit *ratio*
  crosses a threshold, so a rarely-wrong variant survives (and feeds the K2 compound path) while a broadly-wrong
  one is dropped. (M5b)
- **K2 — split the code-key from the data-key; track poison as (code+data).** The KGC/memo key becomes a pair
  (code-hash, data-hash) and poison is recorded per code+data tuple. **The poison set is a search INPUT:** a
  99%-match variant whose 1% misses are covered by a synthesized switch-table is a compound optimization the
  permutations×combinations search should find and promote when it benchmarks better than the previous best.
  (M5b + the search)
- **K3A — refuse-to-JIT at selection time** (`ast_jit_selected`), before the pool job spawns — the unverified
  path is then unreachable. (M5b)
- **K4A — signature/marshalling coverage is itself a strategy row** in the pool, with sane per-platform limits
  (scalar FP xmm0–7 + struct-by-value ≤16 B first; MEMORY-class later). Not a hardcoded gate. (M5b + the search)
  — ⏳ **scalar all-double DONE** (`mccjit_make_kgc_stub_fp`/`_calln_fp`/`_invoke_fp`, `jit/selftest-fparg`);
  mixed GP+FP + struct-by-value deferred (need `classify_x86_64_arg`→ST_FUNC + a class-vector marshaller).
- **K5 — promotion gate = best-of-3 self-benchmark.** After the newest most-optimized code+data variant passes
  the range/soundness sanity tests, it runs a best-of-3 benchmark **against the currently-selected variant**;
  it is promoted only if it wins. (the runtime-JIT scorer; see the ⚠ seam above for the AOT-static scorer) — ✅
  **scorer DONE** (`mccjit_bench_pair`, best-of-3 + L5A tie/margin + deterministic cap, `jit/selftest-bench`);
  live wiring rides J6A's real live-in capture (step 8).
- **K6 — the sorted mmap'd data cache is exhaustive + content-addressed by incremental hash.** On a hash match
  against a previously-computed optimizer output, the data emission is **replaced/optimized-out with the already-
  optimized code** (data → code substitution). **TODO — work through the details** (direction of substitution,
  what "the optimized code for this data" means when the data has no owning function, interaction with P2/M5's
  section-level `ast_data_recs` and the const-data rewrite): the mechanism is clear, the lifecycle is not.
- **K7 — implement BOTH register optimizers, they coexist in the pool.** A C-ABI-compliant register optimizer
  and a non-ABI register optimizer are separate strategy rows; when an Invoke/Call requires C-ABI compliance,
  the non-ABI kernel carries its **own pre/post-Call harness** to meet the ABI at the boundary. Nothing is
  binary — the search picks per call/workload. (M5c)
- **K8 — inline-vs-shim is a search axis**, not a fixed choice: permute × combine, benchmark wins. (M5c)
- **K9 — RESOLVED: QSBR.** Build QSBR (quiescent-state-based reclamation) now — it is the general primitive for
  *both* in-place-patch quiescence and old-variant reclamation, so it un-defers J5 and admits the in-place
  code-patch (D3B) mechanism to the benchmarkable pool. Per-thread quiescent-point instrumentation is gated
  behind the `-O4`/`--jit` runtime sink (AOT-only builds pay nothing). **The correctness floor holds:** pool
  membership is correctness-gated per platform (a wrong quiescence choice crashes), and *within* the correct
  set the benchmark decides — and once in-place patch exists, the **quiescence mechanism itself** (QSBR vs
  stop-the-world vs signal-safepoint) becomes its own benchmarkable axis. Near-term pool = pointer-swap +
  dual-map page flip (both quiescence-free) + in-place patch (QSBR-gated).
- **RESOLVED (reproducibility): ephemeral-default runtime cache.** The unification makes this cheap: AOT `-O4`
  (what M8 self-host validates) uses only the deterministic scorer + static facts and **never touches the
  runtime KGC/benchmark cache**, so it is byte-reproducible by construction. The on-disk KGC/benchmark cache
  exists only at the runtime-JIT sink; default it **ephemeral** (per-run) for JIT'd-program determinism +
  CI/fuzz safety, with **opt-in persistence** for production — J1A's poison persistence bites only under that
  flag. Self-host/fuzz reproducibility is unaffected either way.
- **RESOLVED (hardened-env W^X-denied fallback): boot-probe.** The `.init_array` ctor probes JIT feasibility
  (`MAP_JIT` / RWX); on failure the program silently runs the AOT baseline (deopt-first already provides it) +
  a `MCC_JIT_VERBOSE` note. Never errors.
- **RESOLVED (observability): emit a `perf-<pid>.map`** for JIT'd variants (cheap; unblocks `perf` profiling of
  JIT'd frames). Runtime DWARF/unwind deferred — though the in-place-patch cold-only quiescence path would want
  it, QSBR (K9) doesn't, so unwind stays deferred.

**Resolved this session — JIT tier-3 forks (L1–L12):**

- **L1B — AOT budget = wall-clock explore + memo-pinned winner.** The `-O4+` search explores under a wall-clock
  budget but persists the winner to the memo, so rebuilds replay deterministically; `-O4` stays *off* the M8
  byte-identity bar (only `-O1..-O3` are on it). Seam closed — consistent with the existing search design.
- **L2A — quiescent-point placement = function-entry + loop back-edges** (guarantees a compute-bound thread
  still reaches a quiescent state), gated behind the runtime sink; fall back to pointer-swap where the poll is
  absent.
- **L3 — occupancy is a RECLAMATION problem, not a patch-safety problem** (perspective shift). **Pointer-swap
  is the always-correct default:** it never mutates bytes a thread may execute (the thread finishes on the old
  variant), and it is **transparent to arbitrary address-taking** — `&func` returns the stable dispatcher entry
  (`jmp *SLOT`), and saved `&&label`/return addresses stay valid because the old region stays mapped (in-place
  patch, by contrast, would sabotage exactly those saved addresses). So **correctness needs no occupancy
  accounting.** The residual "when may I free the old variant?" is reclamation: **leak-and-cap now → QSBR
  per-thread epoch (contention-free) / `membarrier` + conservative stack-and-return-address scan when memory
  matters.** The stack-scan is the *self-accounting-against-unknown-threads* option (checks real occupancy,
  handles unregistered threads, zero per-call cost; needs return-addr/unwind info → couples reclamation to the
  deferred unwind observability). **REJECTED: a per-call volatile entry/exit counter** — it is the slowest safe
  option (contended atomic RMW per call + cache-line ping-pong) AND unsafe under non-local exit (longjmp /
  exception / cancellation skips the decrement → the count sticks > 0 → the patcher stalls forever). In-place
  patch (D3B) stays gated behind a search-*proved* safety property (no non-local exit through the region + all
  callers instrumented); absent the proof, pointer-swap wins by default. **This refines K9:** QSBR is still
  built, but its role is *reclamation*, not patch-safety.
- **L4A — benchmark against the KGC-recorded live-ins** (the real observed distribution). (K5) — ✅ the scorer
  `mccjit_bench_pair` takes the live-in tuple set as input; **the real-distribution source rides J6A** (step 8),
  since synthetic inputs are unsafe to feed an arbitrary callee.
- **L5A — incumbent-wins-on-tie** (hysteresis, kills promote↔deopt oscillation) + a deterministic iteration
  cap on the benchmark. (K5) — ✅ DONE in `mccjit_bench_pair` (hysteresis margin `MCC_JIT_BENCH_MARGIN_PCT`
  default 6%; deterministic inner-iteration cap `MCC_JIT_BENCH_ITERS`).
- **L6A — switch-table cover shape is a benchmarked strategy row** (dense→jump-table, sparse→perfect-hash/
  binary-search); fires when the compound (variant + cover) beats the base. (K2)
- **L7A — one classified set {good, bad, unknown}** keyed (code,data), LFU-bounded; abandon a base variant and
  re-search when its poison ratio exceeds the K1C threshold. (K2, unifies the KGC and the poison set)
- **L8A — K6 data→code substitution = compile-time only, data→code**, via a synthetic `.init_array` ctor
  (reuses M6 datacomp's ctor machinery); ownerless data gets that ctor. Resolves the K6 lifecycle TODO.
- **L9B — ship a parser-less re-emit-only engine slice** for the embed (codegen + opt + objfmt, no C front-end)
  — most recompiles are re-emit-from-serialized-intent (`ast_reemit`), which never re-parses, so the Tier-B
  embed can be materially smaller than the full ~800 KB compiler. **Refines D2=A** (re-invoke the engine, but
  the re-emit slice, not the parser). (M4/J4A)
- **L10C — the opt-in persistent cache is documented-untrusted** (the user owns its integrity, like any build
  artifact). Default stays ephemeral; persistence is an explicit opt-in the user vouches for. (No HMAC/re-verify
  layer — accepted risk in persistent mode.)
- **L11A — runtime robustness is one work item** (not a fork): `pthread_atfork` resets the code cache + QSBR
  registry; recompile is pool-thread-only (the dispatcher's atomic-load is async-signal-safe); PIC/PLT calls
  reuse the existing reloc path.
- **L12A — per-platform marshalling limits track the ABI** (SysV: 6 GP + 8 SSE + struct ≤16 B on x86_64;
  AAPCS per arm64), expanded as each arch stub lands (2B). (J2B/K4A)

**Milestones (dependency-ordered):**

- [~] **M4 — scaffold + Stage-1/2 re-emit landed; static link + Tier-B size deferred** — `src/mccjit_embed.c`
  serializes a fn's intent (SoA arena + name strings + signature block + salt) and re-emits it cross-session
  via `ast_reemit`. Embed-into-output works (a compiled program self-hot-swaps its own leaf fn via an
  `.init_array` ctor calling `mccjit_boot_swap`). Stage-2 (pointer params + external calls; callees bind via
  `dlsym(RTLD_DEFAULT)`) and structs/unions (`MCCJIT_ROLE_STRUCT`) landed. **Remaining:** (1) ✅ **[J8B]** DONE —
  refuse-to-JIT bitfield (`VT_BITFIELD`) + FAM/struct-by-value fns is folded into the Phase-1 step-1 eligibility
  gate (`ast_jit_eligible`; runtime `mccjit_last_kgc_ok` also rejects bitfield/variadic); (2) **[J4A/L9B]** static-link a **parser-less re-emit-only engine slice** (E1a: codegen + opt +
  objfmt, no C front-end — `ast_reemit` re-emits from serialized intent without re-parsing, so the embed is
  materially smaller than the full ~800 KB compiler; **refines D2=A** = re-invoke the engine, but the re-emit
  slice) instead of the dynamic dep; validate Tier-B; (3) ✅ **[J3A]** DONE — a per-sym blob **registry** + one
  generic ctor (`__mccjit_registry[]` + `__mccjit_boot_all`, replacing the per-fn ctors); wired for both disk
  and in-program `-run`, see M5; (4) ✅ **[J4A]** the call-bearing embed link is now self-contained — static-link
  the engine archive whole (`AFF_WHOLE_ARCHIVE`) via `MCC_EMBED_JIT_LIB=<libmcc-static.a>`; the exe has no
  `libmcc.so`/`libmccrt.a` runtime dep and self-recompiles standalone (`jit/standalone-static`); (5) **[J4A/L9B]**
  the ~800 KB Tier-B size slice is still open — needs the parser-less re-emit-only engine slice +
  `-ffunction-sections`/`--gc-sections` (today whole-archive pulls ~2 MB, the full compiler).
- [~] **M5 — dispatch (mode 6) + full in-process hot-swap loop landed; in-program wiring DONE (J3A)** —
  `MCC_AST_JIT_DISPATCH=6` emits the indirect variant-slot entry (`jmp *SLOT(%rip)` → 8-byte writable `.data`
  slot). The complete recompile→publish→swap loop works (`mcc_jit_recompile_blob` + `mcc_jit_publish` aligned
  `__ATOMIC_RELEASE` swap), including a genuine const-param-specialized variant. x86_64 ELF only (D7).
  ✅ **[J3A] DONE:** the in-*program* mode-6 slot is now connected to the runtime recompile via the per-sym blob
  registry + one generic ctor; an in-memory `-run` program self-recompiles + hot-swaps its own slot at startup
  (`_runmain` `.init_array` → `__mccjit_boot_all` → `mccjit_boot_swap`). **[J5→K9: un-deferred]** old-variant
  reclamation is now built via **QSBR** — K9 requires QSBR for in-place-patch quiescence, and the same primitive
  gives principled reclamation, so J5 is no longer deferred (it rides K9's QSBR). Trigger/pool = M6 (landed).
- [~] **M5b — runtime known-good cache + differential deopt-verify — mechanism + live integration landed;
  policy + FP args deferred** — `MccjitKgc` = sorted set of fixed-width live-in tuples backed by an `mmap`'d
  file; HIT → variant, MISS → run baseline + variant, match → insert, mismatch → return the baseline result (a
  provably-WRONG variant never returns a wrong answer). Live dispatcher integration (a hand-emitted x86_64 stub
  routing 1–6 SysV int/ptr args through `mccjit_kgc_calln`) + the concurrency lock landed. **Remaining:** (1)
  ✅ **[J2B]** DONE — the silent unverified path is closed (Phase-1 step 1): eligibility is refused at selection
  (`ast_jit_eligible`) and the runtime direct-trampoline fallback now fires only under the explicit
  `MCC_JIT_NO_KGC` unsafe escape hatch, so by default an FP/struct/unverifiable variant keeps the AOT baseline
  instead of running unverified. Still to extend the stub to SysV SSE (xmm0–7) + small
  struct-by-value later, then those become eligible (**[K4A]** this coverage is itself a strategy row with sane
  per-platform limits, not a hardcoded gate); (2) **[J1A/K1C]** mismatch policy — ✅ **runtime core DONE:**
  `mccjit_kgc_calln` tracks per-variant `hits`/`misses` and, on a mismatch/hit **ratio** threshold (K1C:
  default ≥50% over ≥8 verified calls; env `MCC_JIT_POISON_PCT`/`MCC_JIT_POISON_MIN`), flips the variant to
  `poisoned` → **permanent baseline-only deopt** (J1A). This fixes the double-execution bug — the poison flag is
  now consulted (baseline-only fast path at `calln` entry). In-memory / ephemeral default (`jit/selftest-poison`).
  **Still DEFERRED (ride the AOT search integration / opt-in persistence):** **[K2]** split the key into a
  code-hash + data-hash pair, record poison per (code+data), and feed the poison set as a search **input** — a
  99%-match variant + a switch-table over the 1% misses (**[L6A]**) is a compound path the search promotes if it
  benchmarks better; **[L7A]** unify KGC + poison into one {good,bad,unknown} LFU-bounded classified set; persist
  poison to the `mmap`'d cache **only under the opt-in persistent-cache flag** (default ephemeral — see the
  reproducibility seam); (3) skip the miss-check when the M8 static oracle proves in-domain.
- [~] **[J9A·ACTIVE] M5c — pure classifier + slice-profile analysis landed; the slicing/non-ABI BACKEND is the
  deferred remainder** — the whole-function purity classifier `ast_fn_purity`
  (IMPURE / TIER1 memory-value-dependent / TIER0 register-value-only), wired into M5b via
  `MccjitKgc.memoize_ok`, plus the ⏳ **partition analysis** `ast_fn_slice_profile` (impure_ops / loads /
  pure_compute, `jit/selftest-slice`) — the slicing-candidacy analysis K7/K8 consume. **The net-new backend
  work stays deferred (miscompile-risky, multi-week):** statement-
  level pure/impure **slicing** (partition into pure kernels + impure C-ABI "bound" ops); **[K7]** implement
  BOTH a C-ABI-compliant and a non-ABI register calling convention as coexisting strategy rows (`gfunc_prolog`
  spills all params to frame today) — when an Invoke/Call requires C-ABI compliance the non-ABI kernel carries
  its own pre/post-Call harness at the boundary; **[K8]** inline-vs-shim for kernel invocation is a search axis
  (permute × combine, benchmark wins); how a pure slice's live-ins key the M5b cache; interaction with inlining;
  partial-specializing an impure bound call without losing ABI compliance.
- [~] **M6 — trigger/pool: LANDED** (commit 457ca8a1) — N-worker shared queue + async lazy promotion +
  hot-counter (`MccjitCounterState`, threshold default 1000, `MCC_JIT_HOT_THRESHOLD`). x86_64-only counter stub.
- [ ] **[J10·ACTIVE] M7 — hot-patch strategy FAMILY** (was: the single `jit-patchpoint` row). Hot-patching is
  not one mechanism — the JIT should implement **many *how-to-patch* strategies** as search-selectable rows, so
  the dispatch/swap mechanism is itself a dial the search scores (like the opt-level dial). Known members: (a)
  M5's indirect pointer-swap dispatcher (landed); (b) D3B nop-padded patchable prologue for in-place code-patch
  (`jit-patchpoint`); (c) further variants (int3/trap-based patch, per-call-site trampoline rewrite, dual-map
  atomic page flip). Each is a row selectable via the gate vocabulary; correctness is the same guarded-deopt
  contract regardless of *how* the swap lands. **New benchmark item below.**
- [~] **[J10] Benchmark/profile permutations of JIT hot-patch strategies** — ⏳ harness DONE. `jit/selftest-patch`
  measures **swap latency**, **steady-state call overhead**, and **code-cache footprint** for two mechanisms
  (pointer-swap slot + in-place trampoline immediate-rewrite) against the direct-call floor. **Remaining:** the
  cross-thread quiescence-cost metric (rides QSBR step 11), more mechanisms (D3B nop-pad, dual-map flip), and
  feeding the winner into the search's per-function/workload ranking. Now unblocked (≥2 strategies exist).
- [~] **M8 — `eval_slice` soundness oracle (W3) — oracle landed + now bites; hard-gate promotion deferred** —
  `src/ast_eval_slice.h`: independent AST-over-values UB oracle (`defined=0` on div/mod-by-0, `INT_MIN/-1`, bad
  shift, signed overflow). Enumerates `AST_Return` value-slices and checks every spec return value is in the
  baseline's defined-value set over the guarded env (mode 4 exact const; mode 5 mixed-radix sampling, caps
  `DOMAIN_CAP=4096`/`SAMPLE_CAP=8`). Covers straight-line/ternary returns; statement control flow/calls/memory
  are out of scope. ✅ **7A DONE (opt-in):** the oracle now runs in production and, under
  `MCC_AST_JIT_EVAL_GATE`, refuses an unsound spec-slice (falls back to the unspecialized dispatch — always
  correctness-safe); shadow still aborts. Test `jit/selftest-evalgate`. **Remaining:** flip the gate default-on
  after **N := 3** clean self-host + fuzz soaks (rides 2B for the cross-arch signal); extend to statement-level
  control flow.

**Optional AST-strategy rows** (dispatcher already search-selectable via gate bits 40/41):

- [x] **[J6A] §26 `jit-profile` strategy row** — DONE. Live-in range-capture instrumentation rides the D5 hot
  counter (`MccjitCounterState`), not a separate pass: the counter stub spills the 6 GP arg regs and passes
  their address to `mccjit_counter_tick`, and `mccjit_counter_capture` accumulates per-param `argmin/argmax`
  (the runtime range that makes mode 5's guard bite — until now it emitted a redundant `[INT_MIN,INT_MAX]`
  assertion) plus a ring of real observed tuples (`st->sample`, the safe live-in set for K5). Cold-phase only,
  under the counter lock. Test `jit/selftest-profile`. **Unblocks J7 (step 9, value-range speculation)** and
  supplies L4A's live-in source for the step-6 K5 scorer. **[K5] Promotion gate:** once the newest most-optimized code+data
  variant passes the range/soundness sanity tests, it runs a best-of-3 benchmark against the currently-selected
  variant and is promoted only if it wins (the runtime-JIT wall-clock scorer; the AOT-static sink uses the
  deterministic cost/size scorer instead — the ⚠ seam).
- [ ] **[AOT-static sink scorer] `-O4+` deterministic search** — the AOT sink of the one engine: (a) score with
  the deterministic cost/emit-size model (no wall-clock); (b) derive live-in min/max ranges from static analysis
  / const-folding (`AstVLat`/§29), the static analog of J6A's runtime range capture; (c) **gain-ordered,
  time-budgeted strategy scheduling** — sort the pool by expected gain, biggest first, so a limited compile
  budget captures the largest wins (consumes M1(c) `ast_fc_forecast` best-first + §24 hot-slice ranking + §31
  beam). **Bar:** opt-in, "known less effective than runtime values" but **no worse than gcc/clang static range
  inference** (parity). Depends on P1 (the VLat lattice) for the range source.
- [ ] **[K9/L3] Reclamation (QSBR) — pointer-swap is the correctness default** — pointer-swap never mutates
  live bytes and is transparent to arbitrary address-taking (`&func` → stable dispatcher entry; saved
  `&&label`/return addrs stay valid via the still-mapped old region), so **correctness needs no occupancy
  accounting**. Occupancy is only a *reclamation* question — when to free an old variant — solved in tiers:
  **(1)** leak-and-cap now; **(2)** QSBR per-thread epoch (contention-free; quiescent points at **[L2A]**
  function-entry + loop back-edges, runtime-sink-gated); **(3)** `membarrier` + conservative stack/return-addr
  scan when memory matters (self-accounting against unregistered threads; needs unwind info). **No per-call
  entry/exit counter** (slowest + unsafe under non-local exit). In-place patch (D3B) is gated behind a
  search-*proved* safety property (no non-local exit + all callers instrumented); else pointer-swap wins by
  default. Once ≥2 mechanisms exist the quiescence/reclaim mechanism is itself a benchmarkable axis.
  — ⏳ **epoch primitive DONE** (`mccjit_qsbr_*`: register/quiescent/retire/reclaim, `min(local)>=tag` grace
  rule; `jit/selftest-qsbr`); default still leak-and-cap until the swap-path retire + L2A quiescent points are
  wired.
- [x] **[hardened-env] Boot-probe JIT feasibility** — DONE. `mccjit_feasible()` (`pthread_once`-cached) probes
  by `mmap`-ing a `PROT_READ|WRITE|EXEC` page and executing a tiny x86_64 stub; it gates `mccjit_boot_swap` and
  `mccjit_boot_swap_async`, so a W^X-denied / hardened host silently keeps the AOT baseline (deopt-first) + a
  `MCC_JIT_VERBOSE` note, never errors. `MCC_JIT_FORCE_INFEASIBLE` forces the fallback (test hook).
- [x] **[observability] Emit `perf-<pid>.map`** — DONE, opt-in via `MCC_JIT_PERF_MAP` (default off keeps /tmp
  clean and runs reproducible/CI-safe). `mccjit_perf_map_emit` appends `<hexaddr> <hexsize> <fn>` per recompiled
  body (address+length from the variant state's ELF `st_value`/`st_size` via `find_elf_sym(js->symtab,…)`) to
  `/tmp/perf-<pid>.map`, so `perf` symbolizes both the specialized variant and its retained baseline; the KGC
  stub routes through the normally-symbolized `mccjit_kgc_calln`. Runtime DWARF/unwind still deferred (QSBR
  needs none; the L3 `membarrier`+stack-scan reclamation tier does — returns as a *reclamation* dependency if
  leak-and-cap/epoch prove insufficient).
- [x] **[L11A] Runtime robustness** (one item, not a fork) — DONE. `pthread_atfork` resets the worker-pool code
  cache across `fork()` (`mccjit_atfork_prepare`/`_parent`/`_child` registered once via `pthread_once` in
  `mccjit_pool_start`): the child drops the phantom job queue, resets `started`/`nworkers`, and reinits
  `qlock`/`qcond`/`swap_lock`, so it inherits no held lock and no orphaned detached worker. QSBR registry reset
  folds in when K9/QSBR lands (Phase-2 step 11). Signal-safety holds but the *reason* is narrower than first
  written: recompile is **not** pool-thread-only (it also runs synchronously on the calling thread — sync boot,
  sync-fallback, lazy-when-pool-cold, selftests), but it **never runs in a signal handler** (the TU installs no
  signal handlers) and the dispatcher's swap-slot read is an `__ATOMIC_RELEASE`/`ACQUIRE` load, which is
  async-signal-safe. PIC/PLT calls from a variant already reuse the reloc path (`ast_reemit_extern` →
  `mcc_relocate` → `dlsym(RTLD_DEFAULT)` for `SHN_UNDEF`), no change needed. Per-instance KGC/counter-state
  locks are out of scope (held only in brief non-blocking critical sections; their stack/free lifecycle
  precludes a global reset registry — that belongs to the deferred QSBR/reclamation tier). Test:
  `jit/selftest-fork`. **Aside (pre-existing, separate bug):** `jit/selftest-pool` SIGSEGVs on a clean tree —
  root cause is that executing a **pool-worker-built KGC stub** crashes (the fork test's child deliberately
  avoids worker-built stubs for this reason); tracked separately, not part of L11A.
- [ ] **[K6/L8A] Exhaustive content-addressed data cache + data→code substitution** — the sorted `mmap`'d data
  cache is keyed by incremental hash; on a hash match against a previously-computed optimizer output, **[L8A]**
  replace the data emission with the already-optimized code, **compile-time only, direction data→code**, via a
  synthetic `.init_array` ctor (reuses M6 datacomp's ctor machinery); ownerless data gets that ctor. Interacts
  with P2/M5's section-level `ast_data_recs` + the const-data rewrite; depends on P2/M5. Ties to M2's `ComboMemo`
  unification (5A) — the data cache and the code memo share one content-addressed store.

**Research / open questions:**

- [~] **[J7A] Generalize the W2.3 speculative guard beyond non-null — value-range is the next fact** — it is
  the one candidate with a landed consumer (dispatch mode 5's range guard) and a cheap runtime source (J6A's
  `jit-profile` range capture). Do it after J6A. Later candidates (alias/points-to, type-tag/discriminant) have
  no existing fold consumer and each needs a new mini-pass — deferred behind value-range.

**Decisions (all settled with the user):** **D1=B** (embedded), **D2=A** (recompile = re-invoke the engine —
**refined by L9B: a parser-less re-emit-only slice, not the full compiler**), **D3=A** (entry dispatcher;
code-patch D3B is now one member of the **J10 hot-patch strategy family**, not the sole mechanism), **D4=A**
(runtime-observed live-in range), **D5=both** (startup `.init_array` ctor AND `jit-profile` hot counter),
**D6=deopt-first**, **D7=x86_64-first** (2B ports the tail to arm64; was "ELF x86_64 only"), **D8=pthread
pool**. Deopt-arm mechanism = **B (machine-byte splice)**, not AST-level. Tier-2/3 refinements (J*/K*/L*) are
in the blocks above; the AOT↔JIT unification (**AOT `-O4` *is* the JIT**, sink-dependent scorer) is in the
Architecture/Unifying-principle paragraphs.

## CST — concrete syntax tree · [1A — ACTIVE: slice-G stitching → `-g`-from-provenance]

The CST is a byte-exact lossless side-car (`src/mcccst.c`) built during the normal parse and **discarded
immediately** (`cst_capture_end` → `cst_arena_free`, returns NULL). Every downstream capability is latent.
The engine, store, snapshot, hashing, and sym-xref all pass their unit + round-trip tests (`tools/csttool.c`,
`tests/cst/*`), but no driver path consumes the tree. **1A promotes CST to an active workstream: wire the
first real consumer.** Sequence: slice-G stitching (the tree-completeness prerequisite) → `-g`-from-provenance
(the first consumer; also stands up the debugger + gdb test suite). Runs independently of the optimizer path
(after P0). First step is to stop discarding the arena on the `--lsp`+`-g` path and hang the DWARF emitter off
`cst_node_at` provenance.

- [ ] **[1A·step 1] Implement slice-G multi-file `#include` stitching** — currently main-file only; includes
  are captured as separate line-granular templates, never spliced into the consuming file's tree at the
  include site during a real compile. Prerequisite for a whole-TU `-g` index.
- [ ] **[1A·step 2] `-g` from provenance** (CST → DWARF) — the first product consumer; stands up the debugger
  + gdb test suite. (Merges the "Design a full `-g` debugger + gdb test suite" long-tail item.)
- [ ] **Design `--hotreload` from reconciled CST snapshots** — the snapshot + Merkle-reconcile primitives
  exist; no command.
- [ ] **Revisit the `Bind`-marker** — only if the CST can't answer a `-g`/LSP query (open: does the CST
  supersede the separate Bind mechanism?).
- Latent/stubbed (not previously tracked): `CST_Error`/`CST_Missing` node kinds never emitted (no error-
  recovery CST); `cst_build_sourcefile` include templates are line-granular (coarser than the main-file token
  CST); snapshot format is endian-tagged (rejects cross-endian loads).

## Bugs — surfaced by the conformance-test expansion (concrete repros)

- [ ] **Honor auto over-alignment under `-fsanitize=address` / `-b`** — the over-align indirect path in
  `decl_initializer_alloc` is gated off when `asan_g`/`bcheck` is active (native-shadow stack instrumentation
  and the bcheck redzone both assume an rbp-relative slot), so `alignas(32+)` autos are under-aligned there.
  Needs the shadow/redzone bookkeeping to follow the runtime-aligned pointer, or a separate slot scheme.
- [ ] **Extend auto over-alignment to the PE (Windows) targets** — x86_64/arm64/i386 PE are gated off
  (`STACK_OVERALIGN_MAX` undefined) because PE routes VLA alloc through `__chkstk`/alloca (align-16 only);
  needs the helper parameterized on alignment + a bare-`VT_LLOCAL` load case. Validate on a Windows-arm64/x64
  cell.
- [ ] **Root-cause the string-literal `L.N`/anon-symbol layout sensitivity** — 3 exec files (atomic_aggregate,
  c11_freestanding_headers, c11_threads) shift internal `L.N`/anon-symbol numbering under ANY source change;
  currently excluded from the object-diff oracle.
- [ ] **Add a strict-c89-vs-gnu89 discriminator** — `gnu_ext` is a hardcoded `1` in `mcc_new` (`libmcc.c`),
  never cleared, so pedantic diags fire under both `-std=c89` and `-std=gnu89` with `-pedantic-errors`; a true
  split needs a new state field.
- [ ] **Research the §28 rewrite-rule IR** — match→rewrite templates over the captured arena that the §22/§24
  search composes into compound transforms, scored by §25, cached by §21, each rule differential-tested against
  the faithful replay before it may fire. (IR form? how does the search compose rules? scoring hook? cache key?
  the per-rule soundness gate?)

## Long tail — buckets by open-question count · [DEFER unless phase-tagged]

The `## 5 … ## 0` buckets below are the reference backlog, ordered most-open-first. Default status is
`[DEFER]`; items a phase pulls forward carry an inline tag and are sequenced by § Strategic path.

## 5 — many open questions

- [ ] **Explore a link-time/ABI differential fuzzer** — mix mcc `.o` with gcc `.o`, cross-check struct-return/
  varargs/`long double`/bitfield layout (the current fuzzer is tools-only, single whole-program).
- [~] **§27 loop-nest analysis foundation — model + dependence/legality landed; precision remaining** — the
  loop-nest model over `AST_If` op 2..5 (`AstLoopInfo` epoch-guarded side-cache; `ast_loop_depth/_parent/_iv/
  _bounds/_analyzable`), the conservative dependence test (`ast_dep_decode` affine decode, GCD/divisibility
  proof, else direction vectors), and the legality API (`ast_loop_interchange_legal`, `ast_loop_fusion_legal`)
  are landed with live consumers (§27 interchange/fusion/tiling). **Remaining:** evaluating symbolic bounds;
  dependence-test precision (fewer non-affine bail-outs); a dedicated asttool suite (blocked — the dep functions
  live inside `#ifdef MCC_INTERNAL`, which `tools/asttool.c` excludes).

## 4 — several open questions

- [ ] **Decide the §33b post-graft window dataflow (the pivot)** — splice-then-reanalyze (A) vs two-pass
  hand-off (B). The scratch-`Section` emit isolation this rides on is landed (`MCC_AST_SEARCH_EMITISO`,
  text+reloc isolated, data/rodata shared-and-grown; the INLINE axis `MCC_AST_SEARCH_INLINE` also landed).
  **Remaining:** the PROMOTE axis is deferred (corrupts allocator/frame state the scratch doesn't snapshot);
  inline as a freely-reorderable mid-sequence graft is deferred.
- [ ] **Explore EMI mutation (Orion/Athena/Hermes)** targeting optimizer miscompiles.
- [ ] **Design the broader template library** (algebraic/dead-branch/jump-table).

## 3 — a few open questions

- [ ] **Decide compiler-rt-interop vs `libmccsan`** — shapes recover-mode/ASan downstream.
- [ ] **Investigate the §33d seam peephole window** — a store-to-slot immediately followed by a load-from-the-
  same-slot straddling the inline boundary. Resolve whether a bounded 2–3-op window elision preserves the
  pass-1 faithfulness contract, or must run only in pass-2 replay under a differential exec gate.
- [ ] **Revisit §32c genuinely-speculative arm insertion (deferred by design)** — inserting E into an arm where
  it is not guaranteed to reach a post-join use can pessimize cold paths and is the class that killed the
  earlier prototype (arm64 self-host miscompile). Only revisit with the 3-stage self-host fixpoint as the gate.
  (PRE hoist-only ships: `MCC_AST_PRE`, default off.)
- [ ] **Explore coverage-guided generation** — gcov / Intel-PT feedback into `tests/fuzz/gen.h`.
- [ ] **Build the `.rodata` data-emission project** — the `AstKind` enum has no array/global/static-data kind
  and no pass emits initialized data; add a table-symbol+initializer emitter wired into the replay/rewrite
  lifecycle. Prerequisite for §30 value-table dispatch. (Overlaps P2/M5 — the `AST_Data` foundation is landed;
  this is the per-function-node home M5 flagged as remaining.)
- [ ] **[6A·PRIORITIZED] Close the riscv64 Tier-3 backend gap** that blocks full `src/mcc.c` self-host (real-
  program codegen is correct; the whole-compiler self-host is not). **This is the cross-arch validation choke
  point** — riscv64 is in the M8 cross-arch gate but cannot self-host, so nothing validates end-to-end there.
  6A does it early: it makes the M8 cross-arch gate real on riscv64, which every cross-arch-gated item rides on.
- [~] **Build a systematic negative/`dg-error` diagnostic tier** — the first tier landed
  (`tests/diagnostics/dg-error/*.c` + `run_dgerror.cmake`, leading `/* dg-error: <substring> */`, glob +
  CONFIGURE_DEPENDS). **Remaining:** broaden toward gcc's C99/C11 diagnostic files.
- [ ] **Build the `H_e` epoch hash** — invertible slot-keyed O(1) edit patch; designed, not built. (The
  `slot_key -> branch_tag` naming split it needed is done — `src/mccname.h`; only the H_e patch remains.)
- [ ] **Design cross-TU LTO.**
- [ ] **Design separate `-O2`/`-O3` SSA drivers.**
- [ ] **Design a full `-g` debugger + gdb test suite.** → folded into **[1A·step 2]** (`-g`-from-CST-provenance
  stands up the debugger + gdb suite).

## 2 — two open questions

- [~] **Port native-shadow ASan to arm64/riscv64 — arm64 PR-1 landed (heap detection, `-fasan-shadow`).** The
  x86_64-only inline probe + runtime now works on arm64-Linux. **Inline probe** (`arm64-gen.c`
  `gen_asan_shadow_check`): mirrors x86's algorithm in ~13 arm64 insns — `stp x16,x17` scratch save;
  `shadow=(int8)*((addr>>3)+OFF)` via `arm64_movimm(x17,OFF)`+`add …,lsr #3`+`ldrsb`; `cbz` fast-path;
  `(addr&7)+sz-1 < shadow` partial-granule check; `brk #0` on poison (w17=shadow, w16=granule for the handler);
  `ldp` restore. Transparent to the pointer reg even when it IS x16/x17 (save-then-`mov`, restore at `ok:`).
  Enabled on arm64 in `mccgen.c`/`mcc.h`/`mccelf.c` (`#if X86_64||ARM64`) + CMake builds `mccasan.o` on
  arm64-linux (NOT Darwin — ELF-only). **Runtime** (`mccasan.c`, `#if __aarch64__`): `brk`→**SIGTRAP** handler
  reads `regs[17]`/`regs[16]`; one sparse `MAP_NORESERVE` shadow region `[OFF, 2^45+OFF)` covers the whole
  48-bit top-down VA. **Validated end-to-end on real arm64-Linux** (native-in-container, `tests/qemu/native-
  optcheck.sh` path): probe disasm exact; heap-buffer-overflow → trap `shadow byte 0xfa`; heap-use-after-free →
  `0xfd`; clean programs (incl. `-O1` optimizer+asan) no false positive, exit 0. Default macOS/x86 unaffected
  (gated behind `-fasan-shadow`). **PR-2 (stack redzone) also landed** — see the next item. **Globals work on
  arm64 with NO extra code** (validated 2026-07-13): the `asan_g` global-emission path (`mccgen.c`) and
  `asan_register_globals` (runtime) are both arch-agnostic, so PR-1's enablement already emits the
  `__asan_globals` table and poisons global right-redzones — global-buffer-overflow traps `shadow byte 0xf9`,
  clean global/static access is false-positive-free. **So arm64 native-shadow ASan now covers all four core
  classes** (heap-overflow 0xfa · use-after-free 0xfd · stack-overflow 0xf2 · global-overflow 0xf9).
  **Remaining:** 39-bit-VA / bottom-up-mmap shadow-layout robustness (assumes 48-bit top-down); the
  faulting-address + shadow dump (shared with x86 — the "1 — one open question" item); then riscv64.
- [~] **arm64 native-shadow stack-redzone — landed (PR-2).** `arm64-gen.c` `gen_asan_stack_{prolog,epilog}`
  wired into `gfunc_prolog`/`gfunc_epilog`: the prologue reserves 4 insns and the epilogue (once all locals
  are known via `add_asan_locals`) patches in `__asan_stack_enter(table, x29)` there + emits
  `__asan_stack_leave(table, x29)`, preserving the return value (x0 + d0 via `fmov`) across the leave call.
  Locals sit below x29 like x86 locals below rbp, so the runtime's `fp+offset` poison/unpoison works
  unchanged. **Validated on real arm64-Linux:** stack-buffer-overflow → trap `shadow byte 0xf2`; clean
  stack code (arrays/structs) at -O0/-O1/-O2 no false positive; heap detection still works. **Remaining:**
  riscv64 stack-redzone (needs the riscv64 native-shadow port first). (x86_64 already ships this.)
- [ ] **Implement UBSan `-recover` mode** — `sanitize-recover=undefined` is parsed but silently ignored; no
  recover state var or codegen.
- [ ] **Explore a self-host differential** — compile `src/mcc.c` with mcc vs gcc and diff the two compilers'
  behavior over the corpus.
- [ ] **Explore a freestanding/KASAN-style sanitizer for the runtime itself.**
- [ ] **Inline cross-TU static callees.** (§23 step 3)
- [ ] **Explore heuristic non-static inlining** (optional). (§23 step 4)
- [ ] **Implement §24 hot-slice budget allocation** — use the landed `MCC_AST_COST` model to allocate
  `optimize_search_seconds` to the top functions first; rank by `-g` profile entry-frequency, else `node# ×
  loop-nest-depth × call-out-count`. (needs §22; the `ast_loop_depth` factor is landed)
- [ ] **Implement the §25 `-g` hot-value cache** — log function-argument and branch/switch key values +
  frequencies beside the opt checkpoint cache; seed each strategy's `MIN..MAX` from the observed hot range.
  Feeds §29 + §30. (`MCC_AST_JITSCORE` already ships.)
- [ ] **Explore §28 instruction-level superoptimization** over a fixed emitted window (optional).
- [ ] **[P1] Build the §29 integer range/known-bits lattice** — shared prerequisite for the narrowing residue.
  Built in P1 as the one lattice with two projections; the representation (`AstVLat`) + both projection
  accessors + the first value-changing narrowing consumer (unsigned `/ %` + `<<`const, gated `MCC_AST_VLAT`)
  are landed. **Remaining (PR-3+):** signed `/ %` (INT_MIN/−1 trap divergence), `<<` value-count; comparisons
  SKIP; then flip `MCC_AST_VLAT` default-on once broadly exposed.
- [ ] **Implement §30 value-table dispatch** for bit-flag clusters with *differing* bodies. (needs `.rodata`
  data-emission)
- [ ] **Build widening/fixpoint dataflow for §32a** cross-loop-iteration value merging (none present today).
- [~] **§33c argument de-spill / caller-value forwarding — landed gated (`MCC_AST_ARGFWD` default off)** —
  forwards a caller value into the callee's single param use via `ast_argsub`, eliding the spill. **Remaining:**
  widen past single-use (needs the §33b seam); an argfwd-exercising self-host binary is blocked by the mcc-
  linker segfault on inlined `mcc.c` + GNU-ld eh_frame quirk; flip default-on after exposure.
- [ ] **Design the §33e window-level cache key** — `ast_intention_hash` runs pre-graft over the caller arena,
  excluding the callee body, so a window transform needs a window-level key or an accepted first-graft miss.
- [~] **§36 spill-slot sharing — landed gated (`MCC_AST_SPILL_SHARE` off)** — the callee-save COLOR promotion
  save-area shares one spill slot per distinct register. **arm64 COLOR+SHARE self-host fixpoint now VALIDATED**
  (2026-07-13, unblocked by the arm64 promotion PRs — COLOR/SPILL_SHARE live inside the now-arm64-enabled
  `ast_plan_promotion`/`ast_promo_save_plan`): on native arm64/macOS, `PROMOTE+COLOR+SPILL_SHARE` exec 296/296,
  and `mcc.c` recompiled with `MCC_AST_PROMOTE=1 MCC_AST_COLOR=1 MCC_AST_SPILL_SHARE=1` reaches a **byte-identical
  stage2==stage3 fixpoint** (2068166 B); the plain `PROMOTE`-only fixpoint is byte-identical too. **Remaining:**
  general per-value spill slots (backend `get_temp_local_var` recycles by liveness; user-local offsets
  front-end-fixed); the riscv64 fixpoint (needs riscv64 promotion first — its own TODO item).
- [ ] **Normalize CMake incrementally** — autodetect + enable-what-the-host-supports, offload gating to
  `tools/`, fold `.cmake` files in — with a verifiable target, not a sweep (CI-breakage risk across ~35
  presets/platforms).
- [ ] **Cut CI wall-clock — attack the long-pole jobs** (~24 min end-to-end). Critical path is macOS + Windows
  + matrix jobs; native Linux is fast (ctest ~60s). Biggest sinks: the `bench` target (~500s) on macOS/dist —
  gate to one fast native/nightly runner; macOS ctest ~7× native (~431s) — shard + shrink emulated subset;
  matrix jobs re-run full ctest per cell (~430s) — parallelize/prune; Windows msvc/sanitize-msvc/mingw
  ~900-970s — profile build-vs-test split and cache/prune.
- [ ] **Root-cause the named promote/inline gap tests.**
- [ ] **Revisit PP-as-executable-C JIT** (`-fmacro-eval` shipped).
- [ ] **Design a time-budgeted engine.**
- [ ] **Design dependency-ordered `-O1`.**
- [ ] **Design human-friendly diagnostics** tested against terminal geometry.

## 1 — one open question

- [~] **Preserve the faulting address to the asan-shadow trap — done for arm64 AND x86_64.** Both inline probes
  now carry the full faulting address to the trap and the runtime (`mccasan.c`) prints the **faulting address**,
  the **access size** (= granule − (addr&7) + 1), and a **"shadow bytes around" hex dump** with the buggy
  granule bracketed — e.g. `at faulting address 0x…502c / access size 04 / … 00 00 00 fa fa 00 00 fa [fa] fa …`.
  arm64: address in **x15** (x16=granule, x17=shadow; x15/x30 saved as a 2nd stp pair). x86_64: address in
  **rcx** (rax=shadow, rdx=granule; `push rcx`/`pop rcx` added, `mov rcx,r` on the slow path before `ud2` so
  `gsym` auto-patches the jump offsets) — `on_sigill` reads `REG_RCX`. arm64 fully run-validated on real
  arm64-Linux (heap/stack/global reports + clean/no-FP + exec 296/296 + macOS ctest 5170); the x86_64 probe
  **encoding is disasm-verified** (via `cmake-cross/mcc-x86_64`) and its runtime path mirrors the run-validated
  arm64 one — end-to-end x86 run is left to the native x86 CI runner (an emulated-amd64 container qemu-user
  OOM-kills on the x86 ASan runtime's ~17 TB `MAP_NORESERVE` shadow map — a pre-existing emulation limit,
  unrelated to this change). **Remaining:** access type (READ/WRITE — the
  probe in `indir` doesn't distinguish load vs store); the region-relative locator ("N bytes after M-byte
  region").
- [ ] **Implement the clang-compatible `__ubsan_handle_*` diagnostic ABI** — trap mode ships (`ud2` x86_64,
  `brk` arm64/riscv64); no handler ABI exists.
- [ ] **Implement a PE/mingw trap-mode UBSan** — trap mode is gated ELF-only.
- [ ] **Explore `-fsanitize-coverage`** — feeds the coverage-guided fuzzer.
- [ ] **Explore `-fsanitize=cfi` hardening** (absent today).
- [ ] **Explore `_FORTIFY_SOURCE`-style hardening** (absent; `-fstack-protector` already ships with real
  x86_64/arm64 canary codegen).
- [ ] **[3A·SCHEDULED] Add the §22 promotion re-emit axis** on top of emit isolation (scratch-`Section`
  isolation is landed + CI-locked). The axis lets `ast_search_emit_size` measure WITH promotion ON and score
  promote on/off. **3A schedules the framework fix that unblocks this** (below). **A
  prototype was attempted and REVERTED — the measurement is leakier than the scratch guard catches.** For the
  next attempt: (1) `AST_PF_EMIT`'s register-pin loop iterates `ast_promo_n` UNCONDITIONALLY, so a stale plan
  pins wrong registers → SIGSEGV (`if (!do_promote) ast_promo_n = 0;` fixes THAT class, 12/296 → 3/296); (2)
  `AstScratchSave` restores `ast_promo_{n,callful,save_loc,total}` but NOT the plan arrays, nor `nocode_wanted`,
  nor the register-allocator/`vtop` interior state that `ast_promo_entry_init`'s `store`/`gv` touch. A safe
  landing needs full promotion-plan + allocator-state save/restore. **SHARED-DEFECT:** the emit-time value-axis
  measurement is unsound for the INLINE axis too — `exec-search-inline` (`-O4` + emitsize + emitiso +
  SEARCH_INLINE) fails 4/296 (same leak class). **3A = fix the framework's full-state save/restore ONCE
  (promotion plan arrays + `nocode_wanted` + register-allocator/`vtop` interior state, not just the scratch
  cursor set), then both the inline axis and the promotion/budget axes ride it. This is the keystone unblock —
  it clears §22 promotion, §23 inline budgets, M1's scoring gain, and inline/promote in the search together.
  Until it lands, do not enable `MCC_AST_SEARCH_INLINE`/`_PROMOTE` in any default or CI path.**
- [ ] **Add the §22 arena-mutating pass-subset re-emit axis** on top of emit isolation. (inline-size axis
  `MCC_AST_PERFN_INPROC` already ships.)
- [ ] **Register the §23 inline budgets as a §22 search value-axis** — the graft/node/depth runtime knobs all
  landed; exposing them to the search needs emit-size scoring (a value axis). **Unblocked by 3A** (the
  emit-time value-axis framework full-state save/restore fix — see the §22 promotion-axis item). (§23 step 1)
- [ ] **Add more §23 param shapes.** (§23 step 2)
- [~] **§27 loop tiling — landed (`MCC_AST_TILE`, default off; `MCC_AST_TILE_SIZE` default 32)** — tile-and-
  interchange: strip-mines the inner loop of a 2-deep perfect nest and hoists the strip loop OUTERMOST.
  **Remaining (v1 scope):** one tile per function; inner bound must be a `Ref(j) < LiteralM` const; only unit
  inner stride; the outer loop is NOT also strip-mined (true 2-D cache tiling = strip BOTH → 4-deep); no
  reuse/footprint heuristic tuning yet.
- [~] **[P1] Extend §29 narrowing to non-distributive `/ % << >>` + comparisons** — `ast_narrow_binop_ranged`
  (gated `MCC_AST_VLAT`) covers **unsigned `/ %` + `<<`const** and **`>>`** (constant count [0,31] + op0-fit,
  signedness-aware). **Remaining:** signed `/ %` (INT_MIN/−1 trap divergence), `<<` value-count, comparisons
  (likely SKIP); then flip `MCC_AST_VLAT` default-on.
- [~] **[P1] §29 outer-narrow elimination — landed gated (`MCC_AST_NARROW_ELIM` default off)** — `ast_narrow_elim`
  drops a redundant narrowing `AST_Convert` when the operand provably fits. **Remaining:** flow-SENSITIVE facts
  so guard-derived sub-ranges fire (AstVLat is flow-insensitive today); globals (no frame-offset fact); flip
  default-on after exposure.
- [ ] **Add the §30 `switch`-arm detection form.**
- [ ] **Implement §31 adaptive beam width.**
- [ ] **Implement §31 per-function scoping.**
- [ ] **Wire §25 scoring of the §33e de-spill delta.**
- [ ] **Replace the `ast_plan_promotion` heuristic with §36 coloring outright** (not just filter it).
  Fixpoint-gated + native arm64/riscv64.
- [ ] **Verify Tier-4 inline (`ast/replay-inline-spec`) on riscv64/other arches, then ungate** — registered on
  x86_64 + arm64; skip-gated elsewhere.
- [~] **Extend the arm64 backend register model for Tier-3 register promotion — PR-1+PR-2 landed (full
  x19–x28 callee + x9–x15 leaf + v2–v5 float pools, `MCC_AST_PROMOTE` default off).** The whole `ast_promo_*`
  block (pools + `ast_plan_promotion` + entry/exit save-restore + the store-rewrite replay hook) was
  x86_64-`#if`-gated; extended those three guards to arm64 and defined arm64 pools. **PR-1:** `MCC_NB_REGS`
  28→32; four promotion-only callee-saved slots at indices 28–31 (`reg_classes[]=0`, promotion drives them
  via the `load(reg,…)` path, not a reg-class); `intr()` maps them to x19–x22; `IS_FREG` tightened to
  `[F0,F7]` so the new int indices aren't read as float. No prolog/epilog change — `ast_promo_entry_init`/
  `_exit_restore` save the incoming callee-saved value to a stack slot + restore at the single epilog
  (arch-agnostic store/load), callee-saved regs survive every call ⇒ callful promotion sound by
  construction. **PR-2:** widened the pin mask to **64-bit** (`ast_pinned_regs`/`AstScratchSave.pinned`
  `unsigned`→`uint64_t`, `1u<<`→`(uint64_t)1<<` across mccgen.c/mccast.c/x86_64-gen.c — x86 unaffected,
  all its indices <32); `MCC_NB_REGS` 32→38 exposing **x19–x28** (indices 28–37); arm64 pools now **callee =
  {x19..x28}** (callful, 10 regs), **caller/leaf = {x9..x15}** (7 caller-saved, no save needed), **float =
  {v2..v5}** (leaf). **Key fix:** the leaf/float pools use *allocatable* registers whose `reg_classes[]`
  carries the generic `RC_INT`/`RC_FLOAT` bit, so `gv(reg_classes[reg])` matched *any* int/float reg and
  the entry-load landed in x0/v0, not the target → miscompile. Fixed by masking the generic bit at the two
  promo-write `gv` sites: `gv(reg_classes[reg] & ~(MCC_RC_INT|MCC_RC_FLOAT))` isolates the single-register
  class (RC_R(x)/RC_F(x)). **x86-neutral by construction** — x86 promo regs are either class-0 (load path)
  or single-bit (RC_R8/RC_XMM6, no generic bit set) so the mask is a no-op there. Leaf caller-saved is
  guarded against the one arm64 scalar op that lowers to a libcall (quad `long double`, arm64-Linux) — an
  arm64-gated `has_call=1` when a `VT_LDOUBLE` node is present routes it to the saved callee pool.
  `opt_promote` stays 0 on arm64 ⇒ default byte-neutral. **Validated on native arm64/macOS (Mach-O):** full
  ctest **5169/5169**; default exec 296/296 byte-neutral; forced `MCC_AST_PROMOTE=1` exec 296/296;
  `exec-replay-promote` 296/296; `ast/replay-promote` now unified with x86 (asserts loopy+callful+sumptr+
  fdot all promote); disasm confirms leaf `loopy` holds `s`/`v` in x9/x10 (initialized + updated) and
  callful holds locals in x19–x28 across `bl` with stack save-restore; **self-host** — `mcc.c` recompiled
  with promotion (all pools) → a working stage2 mcc that itself compiles+promotes correctly; the
  **PROMOTE-only and PROMOTE+COLOR+SPILL_SHARE self-host fixpoints are byte-identical** (stage2==stage3,
  2068166 B). **arm64-LINUX validation now DONE (2026-07-13, via `tests/qemu/native-optcheck.sh` — a native
  optimizer-enabled build inside an arm64 Debian container under colima; the `cmake-cross/mcc-arm64` path is a
  dead end there — it's built WITHOUT `MCC_CONFIG_OPTIMIZER`, so no promotion fires on it).** On real
  arm64-Linux (`sizeof(long double)==16`, quad): exec suite **296/296** default, `MCC_AST_PROMOTE=1`, AND
  `PROMOTE+COLOR+SPILL_SHARE`; leaf `loopy` promotes into caller-saved x9–x15 (9 refs); and the **`long double`
  guard is confirmed on real quad `long double`** — `ld_leaf` (a long-double fn) has **0** caller-saved x9–x15
  refs, so a hidden `__addtf3`/`__multf3` `bl` cannot clobber a promoted value (macOS can't test this —
  `MCC_USING_DOUBLE_FOR_LDOUBLE` gives no `VT_LDOUBLE` nodes) ([[macos-arm64-status]]). **Remaining (PR-3):**
  callee-saved float pool (v8–v15) for callful float promotion (niche — x86 doesn't do callee-saved float
  promotion either; needs a float-index renumber to stay inside `IS_FREG`'s range, e.g. v0–v15 at 20–35, int
  callee x19–x28 at 36–45); then flip `opt_promote` on for arm64 after a broad-exposure soak (the arm64-Linux
  M8 evidence above clears the correctness bar; the flip is now a soak/judgment call, no longer blocked).
  **SOAK IN PROGRESS (2026-07-13):** the primary miscompile catcher — the differential fuzzer (mcc[PROMOTE] vs
  gcc 14.2 vs clang 19.1) — was previously unrunnable on the dev host (macOS gcc==clang), but the arm64-Linux
  container gives distinct gcc+clang+native-arm64. Ran it with `MCC_AST_PROMOTE=1` forced: seeds 1–1500 (1487
  agree) + seeds 1501–6500 (4961 agree) + a 500-seed `--gates` promotion×other-gates batch (497 agree) =
  **~7000 seeds, 0 miscompiles** (≈55 UB/impl-def-dropped). Reproduce with `tests/qemu/native-optcheck.sh`'s
  container: build mcc + `cc tests/fuzz/runner.c` + `MCC_AST_PROMOTE=1 ./runner mcc B idir work --ref gcc … --ref
  clang … --count N`. x86 promotion is already soaked continuously by CI's `fuzz/matrix-*` (`--gates` includes
  PROMOTE); arm64 promotion soaks manually via the container fuzz (the CMake fuzz-suite is x86_64-gated — CI
  can't run it on arm64; a linux-arm64 CI fuzz cell would make this continuous). **Flip once the arm64 seed
  count is well into the tens of thousands clean across sessions.**
- [ ] **Extend the riscv64 backend register model for Tier-3 register promotion** + qemu validation.
- [ ] **Test the i386 TLS `R_386_TLS_GD/LDM` paths** (`i386-link.c`; i386-gen.c only emits `R_386_TLS_LE`) —
  needs an i386 cross + a 32-bit sysroot.
- [ ] **Audit each `mcc_skip_test` for per-triple ungating** — i386-linux blocked (no 32-bit sysroot);
  aarch64/armv7-linux partial (qemu is x86-TSO — only the memory-model-independent subset). arm64-windows is
  **no longer blocked** — CI runs a native `windows-11-arm64` cell (MSVC 2022 ARM64) that passes the full
  suite; revisit the arm64-windows `mcc_skip_test`s for ungating.
- [ ] **Revisit the `k` always-inline depth policy.**
- [ ] **Revisit size-gated outline.**
- [ ] **Revisit store factoring** (shared render engine).
- [ ] **Revisit the template DSL past ~30 templates.**
- [ ] **Revisit per-function `-O1` mode.**

## 0 — fully specified or execution-blocked (no open design questions)

- [ ] **Ungate the `i386-fastcall-abi` test** — the CMake is already conditionally ungated on
  `if(TARGET mcc-i386)` with `mcc_skip_test` only as the else-fallback; the remaining blocker is building the
  `mcc-i386` cross target via `cmake --preset cross` (the ELF-32/`gcc -m32` reference is available on Linux
  hosts with 32-bit multilib).
