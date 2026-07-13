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

Net shape: **P0 free money → P1 the multiplier → P2 the root-blocker → P3 the milestone**, with substrate-
unify floating and the two hard forks pre-resolved so no phase stalls on a decision.

### Resolved this session — 7 decisions folded in

The matrix cross-examination surfaced seven forks between the implementation and this roadmap; all are now
locked. Several are **keystone unblocks** (one landing clears a cluster), so the phase order below is revised.

- **1A — CST gets a real consumer.** slice-G multi-file `#include` stitching → `-g`-from-provenance (also
  feeds the debugger suite). Promotes CST from `[DEFER]` to an active workstream **[CST]**, run independently
  of the optimizer path (after P0).
- **2B — port the JIT stub tail to arm64.** The mode-6 slot / KGC stub / trampoline / counter get an arm64
  emission path so §26 validates on the arm64-macOS dev host, not just x86-Linux CI. **Reinterprets D7 as
  "x86_64-first," not "only."** Slots into P3's tail after the x86_64 milestones close.
- **3A — fix the emit-time value-axis framework** (full-state save/restore: promotion plan + allocator +
  `vtop` + `nocode_wanted`). **Keystone unblock** — clears the §22 promotion axis, §23 inline budgets, M1's
  scoring gain, and inline/promote in the search in one landing. Promoted ahead of the search-scoring items
  that depend on it (P1-adjacent).
- **4B — dedicated backend-parity session** (cmov/csel emission + a temp-materialization mechanism). Clears
  ABS, 64-bit div-magic, branchless-select, and — with the x86_64 mul-high regalloc fix — DIVMAGIC together.
  Promotes the `[DEFER]` backend-parity cluster to a scheduled session gating the P0 held items.
- **5A — migrate the memo to `ComboMemo` now.** M2/M3 promoted from FLOAT to active: one `ComboMemo` + disk
  backing, retiring the out-of-process superopt's second measurement engine.
- **6A — close the riscv64 Tier-3 self-host gap.** **Validation-infra unblock** — makes the M8 cross-arch
  gate real on riscv64. Done early; everything gated on cross-arch rides on it.
- **7A — promote `eval_slice` to a hard gate. N := 3** clean self-host + fuzz soak cycles, then flip from
  shadow-only to a hard per-strategy gate (rides on 2B for the cross-arch signal).

**Revised order:** **6A** (validation infra) · **3A** (value-axis keystone) → **P0** batch flip + **4B**
(backend parity, clears the held DIVMAGIC/ABS) → **P1** (lattice) · **5A** (memo) → **P2** (const-data) →
**P3** (JIT tails) → **2B** (arm64 stub) → **7A** (hard gate). **[CST] 1A** runs in parallel after P0.

### P0 — Default-on sweep · *warm-up; near-zero resistance*

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

### P1 — Unified value lattice · *keystone; highest multiplier* — **resolves Fork L**

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

### P2 — Const-data rewrite · *root-blocker clear*

The `AST_Data` kind + the size-preserving in-place re-emit primitive (`ast_data_reemit`) are landed on the
section-level side-car (`ast_hook_data` fires at TU scope, no per-function arena). **Remaining:** the
size-CHANGING datacomp rewrite = M6 (C) `.init_array` decompress ctor + (D) `__mcc_decompress` runtime (multi-
backend, breaks link-time-constant consumers) + M4(b/c) score-fold. Details in `[P2] M5`/M6.

- **Decouples the M4↔M6 circularity:** once a transform *owns* a candidate's bytes the data-delta is per-
  candidate and transform-attributed, so M4(b/c) folds into the score without shared-rodata order-noise.
  Direction: M5 rewrite node → M6 owned delta → M4 fold.
- **Also unblocks:** §30 value-table dispatch.

### P3 — Guarded-deopt Stage 1 · *capability milestone*

Shipped as §26 M1–M3 (baseline retention + machine-byte-splice entry dispatcher + non-null speculative
specialization + `--jit-functions`), search-selectable via gate bits 40/41. **M6 (pool) also landed** (commit
457ca8a1). The remaining §26 runtime tail (M4/M5/M5b/M5c/M8 tails, x86_64-only) is tracked in NEXT MILESTONE.

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

Entry-guarded variant dispatch with a runtime recompiler + hot-swap. **Critical path M1 → M2 → (M3) → M4 →
M5 → M6**, with M7/M8 attaching independently after M2. **M2 alone is a shippable, complete guarded-deopt JIT;
M4 is the size/build gate for everything runtime.**

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

**Reusable infra (verified grounding).** `-run` compile-to-executable-memory (`mcc_run`, `mccrun.c`;
`host_runmem_alloc` RWX / W^X dual-map + `host_runmem_protect` + `host_icache_flush`) + `mcc_relocate` (rejects
double-relocate); D3=A entry dispatcher sidesteps the static `E8 rel32` problem — call sites unchanged, the
dispatcher reads a swappable data pointer flipped by one aligned 8-byte atomic store; `.init_array` ctor
emission wired; C11 `<threads.h>` is a real pthread shim; entry-prepend prior art = `ast_tco_run`.

**Milestones (dependency-ordered):**

- [~] **M4 — scaffold + Stage-1/2 re-emit landed; static link + Tier-B size deferred** — `src/mccjit_embed.c`
  serializes a fn's intent (SoA arena + name strings + signature block + salt) and re-emits it cross-session
  via `ast_reemit`. Embed-into-output works (a compiled program self-hot-swaps its own leaf fn via an
  `.init_array` ctor calling `mccjit_boot_swap`). Stage-2 (pointer params + external calls; callees bind via
  `dlsym(RTLD_DEFAULT)`) and structs/unions (`MCCJIT_ROLE_STRUCT`) landed. **Remaining:** (1) bitfields
  (`VT_BITFIELD`) + flexible array members; (2) static `libmcc.a` link (E1a) instead of the dynamic dep;
  (3) a per-sym blob **registry** + one generic ctor (one ctor per fn today); (4) fix the non-fatal
  `libmccrt.a not found` on the call-bearing embed link; (5) ~800 KB Tier-B size validation.
- [~] **M5 — dispatch (mode 6) + full in-process hot-swap loop landed; in-program wiring deferred** —
  `MCC_AST_JIT_DISPATCH=6` emits the indirect variant-slot entry (`jmp *SLOT(%rip)` → 8-byte writable `.data`
  slot). The complete recompile→publish→swap loop works (`mcc_jit_recompile_blob` + `mcc_jit_publish` aligned
  `__ATOMIC_RELEASE` swap), including a genuine const-param-specialized variant. x86_64 ELF only (D7).
  **Remaining:** connect the in-*program* mode-6 slot to the runtime recompile — needs the per-sym blob
  registry (an in-memory `-run` program gets a slot pointing at the AOT body with no recompile hookup); QSBR
  reclamation later. Trigger/pool = M6 (landed).
- [~] **M5b — runtime known-good cache + differential deopt-verify — mechanism + live integration landed;
  policy + FP args deferred** — `MccjitKgc` = sorted set of fixed-width live-in tuples backed by an `mmap`'d
  file; HIT → variant, MISS → run baseline + variant, match → insert, mismatch → return the baseline result (a
  provably-WRONG variant never returns a wrong answer). Live dispatcher integration (a hand-emitted x86_64 stub
  routing 1–6 SysV int/ptr args through `mccjit_kgc_calln`) + the concurrency lock landed. **Remaining:** (1)
  float/SSE + struct-by-value args (the stub emits only GP `mov`/`movsxd` and rejects FP/struct signatures —
  falls back to the non-KGC direct trampoline); (2) mismatch → invalidate-vs-recompile policy — the per-stub
  flag word is written on mismatch but never consulted, so every post-mismatch call keeps double-executing;
  (3) skip the miss-check when the M8 static oracle proves the value in-domain.
- [~] **M5c — pure classifier landed; pure/impure slicing + custom ABI deferred** — the whole-function purity
  classifier `ast_fn_purity` (IMPURE / TIER1 memory-value-dependent / TIER0 register-value-only), wired into
  M5b via `MccjitKgc.memoize_ok`. **Remaining (net-new backend work):** statement-level pure/impure **slicing**;
  the bespoke **off-C-ABI register calling convention** for pure kernels (`gfunc_prolog` spills all params to
  frame); how a pure slice's live-ins key the M5b cache; interaction with inlining; partial-specializing an
  impure bound call without losing ABI compliance.
- [~] **M6 — trigger/pool: LANDED** (commit 457ca8a1) — N-worker shared queue + async lazy promotion +
  hot-counter (`MccjitCounterState`, threshold default 1000, `MCC_JIT_HOT_THRESHOLD`). x86_64-only counter stub.
- [ ] **[DEFER] M7 — `jit-patchpoint` strategy (D3B, optional)** — 4th jit row: nop-padded patchable prologue
  for in-place code-patch hot-swap. M5's pointer-swap dispatcher is the primary mechanism. Deferrable.
- [~] **M8 — `eval_slice` soundness oracle (W3) — oracle landed + now bites; hard-gate promotion deferred** —
  `src/ast_eval_slice.h`: independent AST-over-values UB oracle (`defined=0` on div/mod-by-0, `INT_MIN/-1`, bad
  shift, signed overflow). Enumerates `AST_Return` value-slices and checks every spec return value is in the
  baseline's defined-value set over the guarded env (mode 4 exact const; mode 5 mixed-radix sampling, caps
  `DOMAIN_CAP=4096`/`SAMPLE_CAP=8`). Covers straight-line/ternary returns; statement control flow/calls/memory
  are out of scope. **Remaining (7A):** promote from shadow-only to a hard per-strategy gate after **N := 3**
  clean self-host + fuzz soak cycles (rides on 2B for the cross-arch signal); extend to statement-level control
  flow.

**Optional AST-strategy rows** (dispatcher already search-selectable via gate bits 40/41):

- [ ] **§26 `jit-profile` strategy row** — live-in range-capture instrumentation (the M6 hot-counter trigger
  source, D5). Also what makes dispatch mode 5 bite: its range-guard bound comes from the *static*
  `ast_vlat_context` fact — entry params usually carry only the trivial type-full range, so mode 5 emits a
  redundant `[INT_MIN,INT_MAX]` assertion (sound, deopt-protected, no pruning) until a runtime range exists.

**Research / open questions:**

- [ ] **Research generalizing the W2.3 speculative guard beyond non-null** — alias facts, value ranges, and
  additional live-in domains as further speculative specializations. (which facts have no existing fold
  consumer, as non-null did, and thus need a new mini-pass?)

**Decisions (all settled with the user):** **D1=B** (embedded), **D2=A** (recompile = re-invoke the engine),
**D3=A** (entry dispatcher; code-patch D3B = `jit-patchpoint`), **D4=A** (runtime-observed live-in range),
**D5=both** (startup `.init_array` ctor AND `jit-profile` hot counter), **D6=deopt-first**, **D7=ELF x86_64**,
**D8=pthread pool**. Deopt-arm mechanism = **B (machine-byte splice)**, not AST-level.

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

- [ ] **Port native-shadow ASan (inline probe + `mccasan.c` runtime) to arm64/riscv64** — the native shadow is
  x86_64/ELF-only end-to-end; those arches only have the separate bcheck-based `-fsanitize=address` today.
- [ ] **Implement arm64/riscv64 native-shadow stack-redzone instrumentation** via the `gfunc_prolog`/
  `gfunc_epilog` hooks (x86_64/ELF-only today). (needs the native-shadow port)
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
  save-area shares one spill slot per distinct register. **Remaining:** general per-value spill slots (backend
  `get_temp_local_var` recycles by liveness; user-local offsets front-end-fixed); run the COLOR+SHARE self-host
  fixpoint now that the `MCC_AST_COLOR=1` segfault is fixed (556de5c2); native arm64/riscv64.
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

- [ ] **Preserve the faulting address to the asan-shadow trap** — the `-fasan-shadow` SIGILL report has the
  class, pc, shadow byte, and granule offset but is missing the faulting data address, access type, and size,
  and the "Shadow bytes around the buggy address" hex dump. Root cause: codegen traps with only the shadow byte
  (rax) and granule offset (rdx) live — the fault address is not carried to the `ud2`.
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
- [~] **Extend the arm64 backend register model for Tier-3 register promotion — PR-1 landed (callee-saved
  x19–x22, `MCC_AST_PROMOTE` default off).** The whole `ast_promo_*` block (pools + `ast_plan_promotion` +
  entry/exit save-restore + the store-rewrite replay hook) was x86_64-`#if`-gated; extended those three
  guards to arm64 and defined arm64 pools. **Register model:** `MCC_NB_REGS` 28→32; four promotion-only
  callee-saved slots `MCC_TREG_SAVED(0..3)` at indices 28–31 with `reg_classes[]=0` (so the general
  allocator never picks them — promotion drives them via the `load(reg,…)` path, not a reg-class); `intr()`
  maps 28–31→physical x19–x22; `IS_FREG` tightened to the range `[F0,F7]` so the new int indices aren't
  misclassified as float. **No prolog/epilog change needed** — `ast_promo_entry_init`/`_exit_restore` save
  the incoming callee-saved value to a stack slot and restore it at the single epilog (arch-agnostic
  store/load), and callee-saved regs survive every call (incl. hidden libcalls), so callful promotion is
  sound by construction. Pools (`mccast.c`): arm64 **callee = {x19,x20,x21,x22}** (callful fns); **caller
  (leaf) and float pools held empty** — a caller-saved value is clobbered by any hidden libcall (e.g.
  arm64-Linux quad `long double`), and >4 int regs / any leaf/float pool needs indices >31 → a 64-bit pin
  mask (`ast_pinned_regs`/`sv->pinned` are `unsigned` today). `opt_promote` stays 0 on arm64 ⇒ default
  byte-neutral. Validated on native arm64/macOS (Mach-O): default exec **296/296** byte-neutral; forced
  `MCC_AST_PROMOTE=1` exec **296/296**; `exec-replay-promote` **296/296** (now real arm64 coverage, was a
  no-op); disasm shows `callful`'s 4 locals held in x19–x22 across `bl` with `stur`/`ldur` save-restore;
  **self-host** — `mcc.c` recompiled with promotion on → a working stage2 mcc that itself compiles+promotes
  correctly (the heavy-TU DIVMAGIC-class check); new `ast/replay-promote` arm64 variant asserts `callful`
  promotes (leaf/float abstain). **Remaining (PR-2):** widen the pin mask to 64-bit → add x23–x28 + a
  caller/leaf pool (with a hidden-libcall guard, e.g. treat `long double` ops as callful) + a float pool
  (v-regs); run the x86-style qemu-arm64-Linux cross differential + the COLOR+SPILL_SHARE self-host
  fixpoint on arm64 ([[macos-arm64-status]]). Then flip `opt_promote` on for arm64 after broad exposure.
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
