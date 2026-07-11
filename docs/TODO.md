# TODO.md — open work

The single tracker of remaining, incomplete work. Completed items and their
history have been removed; this file is only what is *left* to do. Reference
documentation of how the shipped compiler behaves lives in
[MCC.md](MCC.md)/[EXCESS.md](EXCESS.md); the `-O4` search is walked through in
[OPT.md](OPT.md).

**Standing gates for every item:** `ctest` green; items touching `src/` also
need the 3-stage byte-identical self-host fixpoint (gate off AND forced-on for a
new optimizer pass); items touching backend or format files add the local matrix
/ a qemu spot-check. New optimizer passes land behind a default-off `MCC_AST_*`
gate and are validated by the §0 differential fuzzer.

---

## Sanitizers (§0 / §0b follow-ups)

- **Native-shadow ASan follow-ups:** richer `__asan_report_*` diagnostic
  formatting, CMake auto-link of `runtime/lib/mccasan.c`, and arm64/riscv64
  stack-redzone instrumentation (heap+global already work on those arches;
  stack is x86_64/ELF-only via the `gfunc_prolog/epilog` hooks).
- **Deferred UBSan modes:** `-recover` mode + the clang-compatible
  `__ubsan_handle_*` diagnostic ABI (trap mode ships); a PE/mingw trap-mode
  UBSan.
- **Exploratory (explore before scoping):** EMI mutation (Orion/Athena/Hermes)
  targeting optimizer miscompiles; coverage-guided generation (gcov / Intel-PT
  feedback into `tests/fuzz/gen.h`); a link-time/ABI differential fuzzer (mix
  mcc `.o` with gcc `.o`, cross-check struct-return/varargs/`long
  double`/bitfield layout); a self-host differential (compile `src/mcc.c` with
  mcc vs gcc, diff the two compilers' behavior over the corpus); a
  miscompile-class scoreboard; `-fsanitize-coverage` (feeds the coverage-guided
  fuzzer); `-fstack-protector` / `-fsanitize=cfi` / `_FORTIFY_SOURCE`-style
  hardening; a freestanding/KASAN-style sanitizer for the runtime itself; the
  compiler-rt-interop-vs-`libmccsan` decision (shapes recover-mode/ASan
  downstream).

## Optimizer ladder

- **§22 true per-function re-emit — promotion / pass-subset axes BLOCKED.** The
  inline-size axis ships (`MCC_AST_PERFN_INPROC`). The promotion axis and the
  arena-mutating pass-subset axis need **true emit isolation** — redirect
  `cur_text_section` (+ reloc, `ind`, symbol scope) to a throwaway scratch
  `Section` per measurement, measure, discard, then emit the winner once into
  the real section. In-place state save/restore was proven insufficient
  (`ast_promo_entry_init` desyncs across re-emits). This "scratch sub-context"
  is the real production consumer of `ast_arena_clone`; milestone-scale.
- **§23 widen the inliner envelope.** Order: (1) bigger node/graft/depth
  budgets, (2) more param shapes, (3) cross-TU static callees, then optionally
  heuristic non-static inlining. Each byte-identity-gated with the inline exec
  column, then registered as a §22 search knob.
- **§24 hot-slice budget allocation.** Use the landed `MCC_AST_COST` model to
  allocate `optimize_search_seconds` to the top functions first (needs §22).
  Rank by `-g` profile entry-frequency when present, else the static proxy
  `node# × loop-nest-depth × call-out-count`.
- **§25 `-g` hot-value cache.** Instrument `-g` builds to log function-argument
  and branch/switch key values with frequencies into a cache beside the opt
  cache; seed each strategy's `MIN..MAX` from the observed hot range and
  frequency-weight probe order. Feeds §29 (Convert ranges) and §30 (bit-flag
  buckets). (The JIT cpu+RSS scoring tier `MCC_AST_JITSCORE` already ships.)
- **§26 `--embed-jit` runtime engine (largest — run LAST).** Make `--embed-jit`
  do its runtime job: an ELF `.init_array` ctor spawning the `--jit-threads`
  pool, the per-function intention trees + a libmcc slice embedded in the
  output, hot functions recompiled via the embedded `mcc_relocate` and hot-
  swapped through an atomic-pointer slot + triple-buffer/RCU reclamation,
  bounded at runtime by `--jit-max-duration`. Dominant cost is embedding the
  ~800 KB compiler slice into every `-O4+` output (a size/build-system problem).
  Builds on §25 + §21.
- **§27 loop-nest interchange/fusion/tiling (very large).** Analysis/structural
  halves are unblocked (§32: no new node kind). Needs a loop-nest model over the
  `AST_If` op 2..5 forms, a conservative dependence test (subscript direction
  vectors, bail-to-"no"), a legality check, the interchange rewrite, and a re-run
  of the §22 search after the nest changes.
- **§28 dynamic algorithm generation (very large, research).** Replace the fixed
  pass menu with a rewrite-rule IR (match→rewrite templates over the captured
  arena) that the §22/§24 search composes into compound transforms, scored by
  §25, cached by §21, each synthesized rule differential-tested against the
  faithful replay before it may fire. Optional: instruction-level
  superoptimization over a fixed emitted window.
- **§29 narrowing residue.** Non-distributive `/ % << >>` + comparisons, and
  outer-narrow elimination (drop a cast when a value provably fits) — both need
  an integer range/known-bits lattice. Fold into the §22 search under §28's
  oracle. (`MCC_AST_NARROW` truncation-sink narrowing ships default-on -O2.)
- **§30 bit-flag residue.** Value-table dispatch for clusters with *differing*
  bodies — blocked on a **`.rodata` data-emission project** (the AST layer has
  no array/global/static-data node kind and no pass emits initialized data; a
  table-symbol+initializer emitter wired into the replay/rewrite lifecycle is
  its own milestone). Also the `switch`-arm detection form.
- **§31 scheduler residue.** The static-vtable strategy registry refactor,
  adaptive beam width, and per-function scoping.
- **§32a residue.** Cross-loop-iteration value merging (needs widening/fixpoint
  dataflow).
- **§32c residue — genuinely-speculative arm insertion (deferred by design).**
  PRE is complete for the safe hoist-only model. Inserting E into an arm where
  it is not guaranteed to reach a post-join use can pessimize cold paths and is
  the exact class that killed the earlier prototype (arm64 self-host
  miscompile). Only revisit with the 3-stage self-host fixpoint as the gate —
  adversarial exec + byte-identity are known-insufficient here.
- **§33b unified post-graft window dataflow (the pivot).** Decide
  splice-then-reanalyze (A: copy the callee subtree into the caller arena so one
  join pass sees the merged window) vs two-pass hand-off (B: thread the caller's
  exit facts into `ast_inline_graft` as the callee replay's entry facts). (A) is
  more powerful but fights the faithfulness-gate architecture (pass-2 replay is
  never byte-verified, so a post-graft pass needs its own soundness gate).
  Deliverable is the A-vs-B decision + arena/gate design.
- **§33c argument de-spill / caller-value forwarding.** Forward a caller's live
  single-use value directly into the callee's first param use instead of
  round-tripping through the biased slot (the non-const generalization of the
  const `ast_argsub` channel). Legality = param read-once, before any store to
  it, operands unclobbered to that read. Multi-read/post-store params fall back
  to today's spill. Needs §33b's seam + optionally §32c.
- **§33d seam peephole window (narrowest, highest risk).** A store-to-slot
  immediately followed by a load-from-the-same-slot straddling the inline
  boundary — the residue §33c cannot forward. This is also the long-open
  **McKeeman peephole** item (adjacent redundant load/store elision in the
  vstack emitter). Resolve whether a bounded 2–3-op window elision preserves the
  pass-1 faithfulness contract, or must run only in pass-2 replay under a
  differential exec gate.
- **§33e window-level scoring & cache key.** `ast_intention_hash` runs pre-graft
  over the caller arena, so it excludes the callee body — a window transform
  needs a window-level key or an accepted first-graft cache miss. Plus §31
  strategy registration and §25 scoring of the de-spill delta.
- **§35 Sethi–Ullman residue.** Register it as a §31 search strategy; extend
  past top-level commutative pairs to an n-ary reassociation-aware ordering
  (reassociation itself is not commutative-safe, so it stays out).
- **§36 Chaitin–Briggs residue.** Spill-slot sharing (coloring gives interval
  sharing for free once extended to spilled ranges), then *replace* (not just
  filter) the `ast_plan_promotion` heuristic outright. Subsumes the A1
  backward-liveness spill-slot-sharing item. Each fixpoint-gated + native
  arm64/riscv64.
- **Tier-4 inline per-arch enablement.** `ast/replay-inline-spec` is registered
  on x86_64 + arm64; riscv64/other arches stay gated until per-arch verified.

## CI / hardware-blocked (no local runner)

- **§38 msvc-arm64 re-check** of the three landed FIX fixes (the
  `vcheck_cmp`-before-`gfunc_call` guard, the `ast_fn_faithful` reemit gate, the
  x86_64-only promote frame-slot change) — needs a Windows-arm64 runner; no
  local equivalent. (macos-x86_64/arm64 and ELF-linux cells are confirmed.)
- **Full `src/mcc.c` self-host on riscv64** — blocked by the pre-existing Tier-3
  riscv64 backend gap (real-program codegen is correct; the whole-compiler
  self-host is not).
- **arm64/riscv64 Tier-3 register promotion** — needs a backend register-model
  extension (arm64 `MCC_NB_REGS=28` doesn't expose x19–x28) + qemu validation.
  The arch-agnostic promotion *analysis* is reused.
- **i386 TLS `R_386_TLS_GD/LDM`** paths (`i386-link.c`) — need an i386 cross
  build + a 32-bit sysroot (x86_64 GD/LD/IE/LE covered by `tls-models`).
- **`i386-fastcall-abi`** test — needs an i386 cross + an ELF-32 reference.

## Conformance / test-depth (§6-B)

- Deepen under-tested areas where mcc passes but under-tests vs gcc/clang:
  flexible array members (mcc ~1 vs gcc 13), `_Noreturn` (1 vs 5),
  `_Alignas`/`_Alignof`, VLA goto/switch-into-scope diagnostics,
  UCN-in-identifier breadth, FP eval-method / Annex-F wide returns, `_Complex`
  Annex-G edge cases, and a systematic negative/`dg-error` diagnostic tier
  (gcc's C99/C11 files are ~70% diagnostic).

## Build / infra

- **CMake normalization (incremental only).** Prefer autodetect +
  enable-what-the-host-supports, offload gating to `tools/`, fold `.cmake` files
  in — but pursue with a verifiable target, not a sweep (CI-breakage risk across
  ~35 presets/platforms; largely already monolithic).
- **Skipped-test ungating audit (per triple).** Re-evaluate each
  `mcc_skip_test` as a host can build/run that triple: i386-linux blocked (no
  32-bit sysroot); aarch64/armv7-linux partial (qemu is x86-TSO — can't validate
  weak-memory atomics, only the memory-model-independent subset); arm64-windows
  blocked (no native arm64 ref cc).

## CST

- **slice-G multi-file `#include` stitching** — currently main-file only (the
  one open CST slice).
- **`H_e` epoch hash** — invertible slot-keyed O(1) edit patch; designed, not
  built. A future `H_e` must reconcile the `slot_key` dual-use with the
  `cst_mark_branch` PPConditional tags (`mcccst.c:512`).

## Docs

- Fill the thin-5W/H nouns flagged in [MCC.md](MCC.md) §Verification: `Poison`
  lowering, `TranslationUnit` node, the long-horizon design-only nouns
  (`-g`/LSP, LTO, SSA drivers, hot-reload, jump-table/algebraic templates), and
  the named promote/inline gap tests without a root cause.

## Parked — record of decisions, not active work

- **Revisit-triggers:** `Bind`-marker (reopen only if the CST can't answer a
  `-g`/LSP query); `k` always-inline depth policy; size-gated outline; store
  factoring (shared render engine); template DSL past ~30 templates; per-function
  `-O1` mode; PP-as-executable-C JIT (the broader form; `-fmacro-eval` shipped).
- **Long-horizon (design only):** broader template library
  (algebraic/dead-branch/jump-table); a time-budgeted engine; dependency-ordered
  `-O1`; cross-TU LTO; `-g` from provenance; hot-reload CST snapshots; separate
  `-O2`/`-O3` SSA drivers.
- **ACHTUNG — explicitly fenced off (DO NOT DO):** human-dimension-aware
  diagnostics tested against terminal geometry; a full `-g` debugger + gdb test
  suite; `-O1..100` max-seconds-to-optimize levels (superseded by the `-O<N>`
  search); `--hotreload` from reconciled CST snapshots.
