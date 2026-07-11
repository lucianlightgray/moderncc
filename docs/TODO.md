# TODO

Sorted by number of open questions/ambiguities (first-round unknowns + the
sub-questions immediately following them), most-open first.

## AST substrate + unified optimizer — see `docs/AST.md`

Collapse the three optimization drivers (the `ast_func_end` pipeline, the §22
`AST_PF_EMIT` trial, the `mcc.c` out-of-process search) into one side-car
substrate + one memo + one strategy engine, shared by the AOT backend and a live
JIT. This reframes/subsumes several items below (§21 cache key, §22 emit
isolation, §28 rewrite IR, §33b/e seam+window keys, §30 predicate bitset, H_e
epoch hash, the time-budgeted engine, per-function `-O1`, PP-as-executable JIT).

Implementation (staged; each gated by faithful replay + corpus differential +
the 3-stage self-host fixpoint):

- [ ] **Step 1 — def/use projection + `cprop_escapes` bitmap** — collapse the
  ~20 whole-arena slot rescans (`ast_local_is_readonly`, `ast_licm_written`,
  `ast_cprop_escapes` (11 sites), `ast_ivsr_count_writes`) into one O(n) per-slot
  side-table built in `ast_func_end`. Pure query, no mutation — the first proof
  of the side-car discipline. First PR.
- [ ] **Step 2 — per-node property memos** — bottom-up bit arrays for the
  monotone subtree predicates (`ast_ident_pure`, `ast_sccp_has_label`,
  `ast_cse_regpure`, `ast_cprop_safe`); O(1) on re-ask.
- [ ] **Step 3 — structural Merkle hash for `ast_ident_same`** — per-node subtree
  fingerprint turning the ~15 lockstep equality walks into O(1) compares. Needs
  the parent-chain edit-repatch invalidation discipline.
- [ ] **Step 4 — `Strategy` objects wrapping the 13 passes** — frozen table
  consumed deterministically at `-O1..-O3` behind `MCC_AST_ENGINE=strategy`;
  flip the default only after byte-identical/better differential vs the legacy
  pipeline + self-host fixpoint. (needs steps 1-3)
- [ ] **Step 5 — coroutine strategies + optional C11 thread pool + live -O4+
  search** — stackless `step()` state machines; NCores-1 pool confined to
  -O4+/JIT; best-first frontier checkpointed to the disk-backed memo. (needs
  step 4)

Research / investigative:

- [ ] **Design the exhaustive-equivalence checker** — the UB-modeling slice
  executor that proves a speculative rewrite over the *context-restricted* input
  domain; shared by the -O4+ round-robin and the JIT; sanity-check time tracked
  separately from apply time. (the correctness gate for all speculation)
- [ ] **Design the bidirectional incremental tree/stack hash + `context_in` /
  `context_out`** — the index that produces the `slice (X) context` memo key and
  restricts the proof domain. (feeds the H_e O(1) accumulator)
- [ ] **Design runtime guarded deopt (OSR)** — guard = the proof's domain
  restriction; anonymous-data guard-fail dispatches to a matching proven variant
  or the static fallback while the new data is bounds-checked. Highest-risk
  component; realizes the re-contextualization assumption.
- [ ] **Settle the global naming authority** — the `(tag,id)` merge of the AST
  `slot_key` vs `cst_mark_branch` PPConditional tags (`mcccst.c:544/1112`); only
  bites when the H_e accumulator lands (step 5+).
- [ ] **Decide the worst-case-vs-average scoring axis** for branch-heavy code
  (the one remaining OPEN scoring question).

## Bugs — surfaced by the conformance-test expansion (concrete repros)

- [x] **Complex `==`/`!=` ignored the real component** — FIXED: `gen_complex_op`
  emitted both float `ucomisd` back-to-back, so the first (real) `VT_CMP`'s flags
  were clobbered by the second before either `sete`; result was `im_eq & im_eq`.
  `gv(MCC_RC_INT)` now materializes the real compare before the imaginary one.
  Regression cases added to `exec/complex_annexg`; 3-stage self-host fixpoint +
  qemu arm64/riscv64 green.
- [x] **UCN-started identifier after a punctuator → "stray '\\'"** — FIXED: the
  `PEEKC` operator-lookahead macro ran a following `\` through `handle_stray`,
  which errored on `\u`/`\U`. It now leaves a UCN `\` for the next token (guarded
  on `p[1] != 'u' && p[1] != 'U'`), so `&é`, `.ü`, `a+é`, etc. lex.
  Line-continuation and stray-`\` diagnostics preserved; `exec/ucn_identifiers`
  tightened to the adjacent forms.
- [ ] **Local auto over-alignment > 16 not honored at `-O0`** — `alignas(32)`/
  `alignas(64)` on a stack (auto) variable yields only 16-byte alignment at
  `-O0` (correct at `-O1+`); statics/globals are fine. Found while widening
  `exec/alignas_over` (the test now asserts only the guaranteed 16-byte local
  alignment). Needs `-O0` stack-realignment for over-aligned locals.

## 6 — open design space

- [ ] **Research the §28 rewrite-rule IR** — match→rewrite templates over the
  captured arena that the §22/§24 search composes into compound transforms, scored
  by §25, cached by §21, each rule differential-tested against the faithful replay
  before it may fire. (IR form? how does the search compose rules? scoring hook?
  cache key? the per-rule soundness gate?)

## 5 — many open questions

- [ ] **Explore a link-time/ABI differential fuzzer** — mix mcc `.o` with gcc
  `.o`, cross-check struct-return/varargs/`long double`/bitfield layout (the
  current fuzzer is deliberately tools-only, single whole-program).
- [ ] **Build the §27 loop-nest analysis foundation** — a loop-nest model over the
  `AST_If` op 2..5 forms, a conservative dependence test (subscript direction
  vectors, bail-to-"no"), and a legality check. (no new node kind)

## 4 — several open questions

- [ ] **Decide the §33b post-graft window dataflow (the pivot)** —
  splice-then-reanalyze (A: copy the callee subtree into the caller arena so one
  join pass sees the merged window) vs two-pass hand-off (B: thread the caller's
  exit facts into `ast_inline_graft` as the callee replay's entry facts).
  Deliverable is the A-vs-B decision + arena/gate design.
- [ ] **Build scratch-`Section` emit isolation for §22** — redirect
  `cur_text_section` (+ reloc, `ind`, symbol scope) to a throwaway `Section` per
  measurement, measure, discard, emit the winner once. In-place save/restore was
  proven insufficient (`ast_promo_entry_init` desyncs). The real production
  consumer of `ast_arena_clone` (today only in `tools/asttool.c`); milestone-scale.
- [ ] **Explore EMI mutation (Orion/Athena/Hermes)** targeting optimizer
  miscompiles.
- [ ] **Design the broader template library** (algebraic/dead-branch/jump-table).

## 3 — a few open questions

- [ ] **Decide compiler-rt-interop vs `libmccsan`** — shapes recover-mode/ASan
  downstream.
- [ ] **Investigate the §33d seam peephole window / McKeeman peephole** — a
  store-to-slot immediately followed by a load-from-the-same-slot straddling the
  inline boundary. Resolve whether a bounded 2–3-op window elision preserves the
  pass-1 faithfulness contract, or must run only in pass-2 replay under a
  differential exec gate.
- [ ] **Revisit §32c genuinely-speculative arm insertion (deferred by design)** —
  inserting E into an arm where it is not guaranteed to reach a post-join use can
  pessimize cold paths and is the class that killed the earlier prototype (arm64
  self-host miscompile). Only revisit with the 3-stage self-host fixpoint as the
  gate. (PRE hoist-only ships: `MCC_AST_PRE`, default off)
- [ ] **Explore coverage-guided generation** — gcov / Intel-PT feedback into
  `tests/fuzz/gen.h` (today purely deterministic seed-driven).
- [ ] **Build the `.rodata` data-emission project** — the `AstKind` enum has no
  array/global/static-data kind and no pass emits initialized data; add a
  table-symbol+initializer emitter wired into the replay/rewrite lifecycle.
  Prerequisite for §30 value-table dispatch.
- [ ] **Close the riscv64 Tier-3 backend gap** that blocks full `src/mcc.c`
  self-host (real-program codegen is correct; the whole-compiler self-host is not).
- [ ] **Build a systematic negative/`dg-error` diagnostic tier** — gcc's C99/C11
  files are ~70% diagnostic.
- [ ] **Build the `H_e` epoch hash** — invertible slot-keyed O(1) edit patch;
  designed, not built. Must reconcile the `slot_key` dual-use with the
  `cst_mark_branch` PPConditional tags (`mcccst.c:544`, invoked at `mcccst.c:1112`).
- [ ] **Design cross-TU LTO.**
- [ ] **Design separate `-O2`/`-O3` SSA drivers.**
- [ ] **Design a full `-g` debugger + gdb test suite.**

## 2 — two open questions

- [ ] **Port native-shadow ASan (inline probe + `mccasan.c` runtime) to
  arm64/riscv64** — the native shadow is x86_64/ELF-only end-to-end; those arches
  only have the separate bcheck-based `-fsanitize=address` today.
- [ ] **Implement arm64/riscv64 native-shadow stack-redzone instrumentation** via
  the `gfunc_prolog`/`gfunc_epilog` hooks (x86_64/ELF-only today). (needs the
  native-shadow port)
- [ ] **Implement UBSan `-recover` mode** — `sanitize-recover=undefined` is parsed
  but silently ignored; no recover state var or codegen.
- [ ] **Explore a self-host differential** — compile `src/mcc.c` with mcc vs gcc
  and diff the two compilers' behavior over the corpus.
- [ ] **Explore a freestanding/KASAN-style sanitizer for the runtime itself.**
- [ ] **Inline cross-TU static callees.** (§23 step 3)
- [ ] **Explore heuristic non-static inlining** (optional). (§23 step 4)
- [ ] **Implement §24 hot-slice budget allocation** — use the landed
  `MCC_AST_COST` model to allocate `optimize_search_seconds` to the top functions
  first; rank by `-g` profile entry-frequency, else `node# × loop-nest-depth ×
  call-out-count`. (needs §22)
- [ ] **Implement the §25 `-g` hot-value cache** — log function-argument and
  branch/switch key values + frequencies beside the opt checkpoint cache; seed
  each strategy's `MIN..MAX` from the observed hot range. Feeds §29 + §30.
  (`MCC_AST_JITSCORE` already ships.)
- [ ] **Embed the §26 per-function intention trees + libmcc slice** into `-O4+`
  output — the ~800 KB slice is the dominant size/build-system cost.
- [ ] **Implement §26 hot-function recompile + hot-swap** — recompile via the
  embedded `mcc_relocate`, hot-swap through an atomic-pointer slot +
  triple-buffer/RCU reclamation.
- [ ] **Explore §28 instruction-level superoptimization** over a fixed emitted
  window (optional).
- [ ] **Build the §29 integer range/known-bits lattice** — shared prerequisite for
  the narrowing residue.
- [ ] **Implement §30 value-table dispatch** for bit-flag clusters with *differing*
  bodies. (needs `.rodata` data-emission)
- [ ] **Refactor the §31 scheduler to a static-vtable strategy registry** — passes
  are invoked by a hardcoded env-gated `if` chain today.
- [ ] **Build widening/fixpoint dataflow for §32a** cross-loop-iteration value
  merging (none present today).
- [ ] **Implement §33c argument de-spill / caller-value forwarding** — forward a
  caller's live single-use value directly into the callee's first param use (the
  non-const generalization of the const `ast_argsub` channel); legality = param
  read-once before any store, operands unclobbered. (needs §33b's seam; optionally
  §32c)
- [ ] **Design the §33e window-level cache key** — `ast_intention_hash` runs
  pre-graft over the caller arena, excluding the callee body, so a window transform
  needs a window-level key or an accepted first-graft cache miss.
- [ ] **Extend §35 to an n-ary reassociation-aware ordering** past top-level
  commutative pairs (reassociation itself stays out — not commutative-safe).
- [ ] **Implement §36 spill-slot sharing** — extend the `MCC_AST_COLOR` interval
  sharing to spilled ranges; subsumes the A1 backward-liveness item.
  Fixpoint-gated + native arm64/riscv64.
- [ ] **Normalize CMake incrementally** — autodetect + enable-what-the-host-
  supports, offload gating to `tools/`, fold `.cmake` files in — with a verifiable
  target, not a sweep (CI-breakage risk across ~35 presets/platforms).
- [ ] **Implement slice-G multi-file `#include` stitching** — currently main-file
  only (the one open CST slice).
- [ ] **Root-cause the named promote/inline gap tests.**
- [ ] **Revisit PP-as-executable-C JIT** (the broader form; `-fmacro-eval`
  shipped).
- [ ] **Design a time-budgeted engine.**
- [ ] **Design dependency-ordered `-O1`.**
- [ ] **Design `-g` from provenance.**
- [ ] **Design human-friendly diagnostics** tested against terminal geometry.
- [ ] **Design `--hotreload` from reconciled CST snapshots.**

## 1 — one open question

- [ ] **Implement richer `__asan_report_*` diagnostic formatting** — today
  `runtime/lib/mccasan.c` only has a one-line SIGILL handler.
- [ ] **Implement the clang-compatible `__ubsan_handle_*` diagnostic ABI** — trap
  mode ships (`ud2` on x86_64, `brk` on arm64/riscv64); no handler ABI exists.
- [ ] **Implement a PE/mingw trap-mode UBSan** — trap mode is gated ELF-only.
- [ ] **Implement a persistent miscompile-class scoreboard** — `campaign` mode
  dedups classes in-memory only; no on-disk artifact.
- [ ] **Explore `-fsanitize-coverage`** — feeds the coverage-guided fuzzer.
- [ ] **Explore `-fsanitize=cfi` hardening** (absent today).
- [ ] **Explore `_FORTIFY_SOURCE`-style hardening** (absent today;
  `-fstack-protector` already ships with real x86_64/arm64 canary codegen).
- [ ] **Add the §22 promotion re-emit axis** on top of emit isolation. (needs the
  scratch-`Section` isolation)
- [ ] **Add the §22 arena-mutating pass-subset re-emit axis** on top of emit
  isolation. (needs the scratch-`Section` isolation; inline-size axis
  `MCC_AST_PERFN_INPROC` already ships)
- [ ] **Widen the §23 inliner budgets** — bigger node/graft/depth (current
  `ast_graft_budget_max = 2048`, `AST_INLINE_MAX_DEPTH 8`). Byte-identity-gated,
  then registered as a §22 search knob. (§23 step 1)
- [ ] **Add more §23 param shapes.** (§23 step 2)
- [ ] **Add the `--jit-threads` flag** — does not exist yet (§26).
- [ ] **Build the §26 ELF `.init_array` ctor** spawning the `--jit-threads` pool.
- [ ] **Enforce the `--jit-max-duration` runtime bound** — parsed but not enforced
  (§26). (run §26 LAST; builds on §25 + §21)
- [ ] **Implement the §27 interchange rewrite** + re-run the §22 search after the
  nest changes. (needs the loop-nest analysis foundation)
- [ ] **Implement §27 loop fusion.** (needs the loop-nest analysis foundation)
- [ ] **Implement §27 loop tiling.** (needs the loop-nest analysis foundation)
- [ ] **Extend §29 narrowing to non-distributive `/ % << >>` + comparisons** —
  `ast_narrow_binop` handles only the distributive `+ - * & | ^` today. (needs the
  lattice; `MCC_AST_NARROW` truncation-sink narrowing ships default-on -O2)
- [ ] **Implement §29 outer-narrow elimination** — drop a cast when the value
  provably fits. (needs the lattice)
- [ ] **Add the §30 `switch`-arm detection form.**
- [ ] **Implement §31 adaptive beam width.**
- [ ] **Implement §31 per-function scoping.**
- [ ] **Wire §25 scoring of the §33e de-spill delta.**
- [ ] **Register the §35 Sethi–Ullman ordering as a §31 search strategy** —
  `MCC_AST_SETHI` is called inline in the emit loop today. (needs the §31 registry)
- [ ] **Replace the `ast_plan_promotion` heuristic with §36 coloring outright**
  (not just filter it). Fixpoint-gated + native arm64/riscv64.
- [ ] **Verify Tier-4 inline (`ast/replay-inline-spec`) on riscv64/other arches,
  then ungate** — registered on x86_64 + arm64; skip-gated elsewhere.
- [ ] **Extend the arm64 backend register model for Tier-3 register promotion** —
  `MCC_NB_REGS=28` doesn't expose x19–x28 — + qemu validation. (promotion analysis
  is arch-agnostic and reused)
- [ ] **Extend the riscv64 backend register model for Tier-3 register promotion**
  + qemu validation.
- [ ] **Test the i386 TLS `R_386_TLS_GD/LDM` paths** (`i386-link.c`; i386-gen.c
  only emits `R_386_TLS_LE`, so GD/LDM are untested) — needs an i386 cross + a
  32-bit sysroot.
- [x] **Expand VLA goto/switch-into-scope diagnostic tests.**
- [x] **Expand FP eval-method / Annex-F wide-return tests.**
- [x] **Expand `_Complex` Annex-G edge-case tests.**
- [ ] **Audit each `mcc_skip_test` for per-triple ungating** — i386-linux blocked
  (no 32-bit sysroot); aarch64/armv7-linux partial (qemu is x86-TSO — only the
  memory-model-independent subset); arm64-windows blocked (no native arm64 ref cc).
- [ ] **Verify `Poison` lowering.**
- [ ] **Verify the `TranslationUnit` node.**
- [ ] **Revisit the `Bind`-marker** — only if the CST can't answer a `-g`/LSP query.
- [ ] **Revisit the `k` always-inline depth policy.**
- [ ] **Revisit size-gated outline.**
- [ ] **Revisit store factoring** (shared render engine).
- [ ] **Revisit the template DSL past ~30 templates.**
- [ ] **Revisit per-function `-O1` mode.**

## 0 — fully specified or execution-blocked (no open design questions)

- [x] **Add CMake auto-link of `runtime/lib/mccasan.c`** — `-fasan-shadow` now
  auto-links `mccasan.o` (built beside `libmccrt.a`, x86_64/ELF-native only) ahead
  of libc via `mcc_add_support`; manual-link path retained by one cli case.
- [ ] **Verify the three landed §38 msvc-arm64 FIX fixes** (the
  `vcheck_cmp`-before-`gfunc_call` guard, the `ast_fn_faithful` reemit gate, the
  x86_64-only promote frame-slot change) on a Windows-arm64 runner. (macos and
  ELF-linux cells confirmed)
- [ ] **Ungate the `i386-fastcall-abi` test** — registered but `mcc_skip_test`'d;
  needs an i386 cross + an ELF-32 reference.
- [x] **Expand flexible-array-member tests** (mcc ~1 vs gcc 13).
- [x] **Expand `_Noreturn` tests** (1 vs 5).
- [x] **Expand `_Alignas`/`_Alignof` tests.**
- [x] **Expand UCN-in-identifier breadth tests.**
