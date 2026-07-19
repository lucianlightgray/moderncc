# TODO

## How to process
- Mark an item in progress, commit+push to main, then start it.
- Implement gated behind a new env (default OFF ⇒ byte-identical); validate the gated-ON path to the M8 bar; independently re-verify the pass fires + correctness vs gcc; commit; prune the item.
- Completed items are pruned entirely; detail lives in git history.
- M8 bar: ctest byte-identity · `-O6` differential vs gcc/clang · 3-stage self-host fixpoint · UBSan/ASan · cross-arch (i386/arm32/riscv64/arm64) · differential fuzz (x86_64) · shadow-IV zero-divergence. The fuzzer + shadow-IV oracle are x86-only.
- Cross-arch checks: use `cmake-cross/mcc-i386` and `cmake-cross/mcc-arm64` (the `cmake-qemu-*` builds emit native x86_64 and lack the optimizer).

## JIT runtime (§26)
- Fix `MCC_AST_SEARCH=1 -O4` segfault on `src/mcc.c` (JIT-independent; scale/emit-size fault in the in-process combo search).
- Build a bounded UB-sound compile-time loop interpreter in `ast_eval_slice`; fold no-escape-pure slices to constants into the submitted arena.
- In-arena sub-region splice for slice ⊊ body (C2b partial-slice reconcile; needs a node-identity-stable splice primitive).
- Split the KGC key into (code-hash, data-hash); track poison per (code+data); feed poison as a search input (K2).
- Switch-table cover strategy row (dense→jump-table, sparse→perfect-hash; L6A).
- Unify KGC+poison into one {good,bad,unknown} LFU-bounded classified set (L7A).
- Persist poison to the mmap'd cache under the opt-in persistent-cache flag (J1A).
- Parser-less re-emit-only engine slice + `-ffunction-sections`/`--gc-sections` for the ~800 KB Tier-B embed; reconcile CMake `libmcc-static.a` so plain `-lmcc` prefers it (L9B).
- General value-range narrowing fold (J7A; needs the P1 VLat consumer).
- Small struct-by-value marshalling; promote `classify_x86_64_arg` (x86_64-gen.c) to `ST_FUNC` (K4A).
- Wire QSBR reclamation into the swap path + L2A quiescent points (function-entry + loop back-edges); un-defer J5 (K9).
- D3B nop-padded patchable-prologue in-place code-patch + int3/trap + dual-map page-flip patch rows (J10).
- Non-ABI register calling-convention kernel with boundary ABI harness (K7); inline-vs-shim search axis (K8); float + memory-boundary slices.
- Flip `MCC_AST_JIT_EVAL_GATE` default-on after 3 clean self-host + fuzz soaks; extend the oracle to statement-level control flow (7A/M8).
- AOT-static sink scorer: deterministic cost/emit-size model + static-analysis ranges (AstVLat) + gain-ordered time-budgeted scheduling.
- K6/L8A data→code substitution via a synthetic `.init_array` ctor.

## JIT arm64
- arm64 mode-6 dispatch for object output: harden the GOT/ABS64 slot so the function symbol survives external link, then drop the `!ast_search_env` guard.
- arm64 `mccjit_make_kgc_stub_mixed` (mixed GP+FP thunk: `movz/movk`→x0..x5 + `ldr d0..d7`).
- arm64 in-place trampoline patch row.

## JIT Windows / i386-PE
- i386-PE KGC/FP(x87)/mixed stub tail (i386 JIT promotion; also the Linux i386 gap).
- arm64-PE runtime-JIT (frameless-leaf return corruption + RtlAddFunctionTable/icache); needs arm64-Windows HW.
- MSVC-arm64 JIT-exec miscompile; needs arm64-Windows HW.
- Standalone `--embed-jit` blob for i386-PE / arm64-PE.

## AOT foundations
- Close the riscv64 Tier-3 self-host gap (6A; makes the M8 cross-arch gate real).
- Fix the emit-time value-axis framework full-state save/restore (promotion-plan arrays + `nocode_wanted` + register-allocator/`vtop` state); then enable the inline + promote search axes (3A).
- Fix the x86_64 mul-high register-allocation self-host miscompile, then flip `MCC_AST_DIVMAGIC` default-on; i386 64-bit divmagic runtime helper (4B).
- Flip `MCC_AST_SELECT`/`MCC_AST_ABS` default-on after soak; i386/arm 32-bit cmov (4B).
- Memo unification: migrate to `ComboMemo` + disk backing with a bulk-value compression mode; subsume the out-of-process superopt (5A/M2/M3).
- PR-C loop-IV monotonicity widening into `ast_vlat_context_at`, op-3/op-5 for-loops only; held until x86 fuzz soaks clean (P1/§32a).
- Predicate-vector 4th side-car index (P1).
- `context_in`/`context_out` memo-key consumer (P1).
- Descendant-indexed (DFS enter/exit) def/use extension for subtree-scoped write queries (P1).
- Flip `MCC_AST_VLAT` default-on; signed `/ %` (INT_MIN/−1 trap), `<<` value-count (P1/§29).
- Flip `MCC_AST_NARROW_ELIM` default-on: flow-sensitive facts + globals (§29).
- Flip `MCC_AST_ARGFWD` default-on; widen past single-use (§33c).
- Flip `MCC_AST_SPILL_SHARE` default-on; general per-value spill slots; riscv64 fixpoint (§36).
- Flip arm64 `opt_promote` default-on after tens-of-thousands-of-seeds soak; PR-3 callee-saved float pool v8–v15.
- riscv64 register promotion + qemu validation.
- Replace `ast_plan_promotion` heuristic with §36 coloring; fixpoint-gated + native arm64/riscv64.
- Verify Tier-4 inline (`ast/replay-inline-spec`) on riscv64, then ungate.

## Ungate campaign (flip every default-off feature on)
Endgame: once each gate's M8 soak is clean, flip it default-on and regenerate goldens. Beyond the per-gate flips in AOT foundations:
- Flip `MCC_AST_LICM_TEMP`, `MCC_AST_IVSR`, `MCC_AST_PRE`, `MCC_AST_SETHI_LEAF` default-on after soak.
- Resolve order-non-confluence, then flip `MCC_AST_REASSOC` default-on.
- Flip §27 `MCC_AST_INTERCHANGE`/`_FUSION`/`_TILE` default-on after soak.
- Wire `MCC_AST_COST` as the search/budget scorer, then flip it on.
- Flip the search family (`MCC_AST_SEARCH`/`_EMITSIZE`/`_EMITISO`/`_INLINE`/`_THREADS`/`_ORDERED`/`_ORDER`) on (needs 3A + §22 emit isolation).
- Flip `MCC_AST_PERFN_INPROC` on.
- Flip the runtime `MCC_AST_JIT` global gate + `_JIT_SPLICE`/`_JIT_DISPATCH` default-on after the full §26 validation-bar soak.

## Cross-arch parity (raise every arch to the x86_64 Tier-4 reference)
Goal: each arch matches x86_64 for self-host, promotion, cmov/csel, div-magic, JIT stub tail, ASan native-shadow, stack-protector, UBSan trap, TLS GD/LDM, Tier-4 replay-inline, over-align. Per-arch rollup (detail in the JIT/Sanitizers/AOT sections):
- riscv64: raise to Tier-4 — self-host, promotion, replay-inline, cmov, JIT stub tail, ASan native-shadow, stack-redzone.
- i386: UBSan trap, ASan native-shadow, stack-protector, TLS GD/LDM, 32-bit cmov, 64-bit div-magic helper, JIT stub tail, Tier-4 replay-inline.
- arm (armv7): raise from Tier-2 — self-host, cmov, UBSan trap, ASan native-shadow, stack-protector, JIT stub tail, replay-inline.
- arm64: TLS GD/LDM (currently LE only), mode-6 object-output dispatch, kgc-mixed stub, in-place patch row, flip `opt_promote` on.
- PE targets: UBSan handler ABI, asan-shadow, over-align on i386-PE/arm64-PE, standalone embed-jit blob, arm64-PE runtime JIT.
- Provide a weak-memory-model validator for aarch64/armv7 (qemu is x86-TSO); a linux-arm64 CI fuzz cell.

## Const-data (P2)
- Size-changing datacomp: `.init_array` decompress ctor (all 5 arches) + `__mcc_decompress` runtime (M6).
- TLS `tdata`→`tbss` + asan/bcheck zero-init `.bss` cases (M6z).
- §30 value-table dispatch (needs `.rodata` data-emission).

## CST (1A)
- slice-G multi-file `#include` stitching.
- `-g` from CST provenance (stands up the debugger + gdb suite); stop discarding the arena on the `--lsp`+`-g` path.
- Design `--hotreload` from reconciled CST snapshots.
- Revisit the Bind-marker (does CST supersede it?).
- Emit `CST_Error`/`CST_Missing` (error-recovery CST).

## Sanitizers
- Honor auto over-alignment under `-fsanitize=address` / `-b`.
- Validate + ungate `alignas` over-align on i386-PE / arm64-PE.
- UBSan `-recover` mode (parsed, ignored).
- clang-compatible `__ubsan_handle_*` diagnostic ABI.
- Port native-shadow ASan to riscv64; 39-bit-VA/bottom-up-mmap shadow-layout robustness; access-type READ/WRITE + region-relative locator; riscv64 stack-redzone.
- Decide compiler-rt-interop vs `libmccsan`.
- Explore `-fsanitize-coverage`, `-fsanitize=cfi`, `_FORTIFY_SOURCE`, freestanding/KASAN-style runtime sanitizer.

## Tests / infra
- Root-cause string-literal `L.N`/anon-symbol layout sensitivity (3 exec files, excluded from object-diff).
- Broaden the dg-error diagnostic tier toward gcc C99/C11.
- Test i386 TLS `R_386_TLS_GD/LDM` (needs i386 cross + 32-bit sysroot).
- Ungate `i386-fastcall-abi` (build the `mcc-i386` cross target).
- Audit `mcc_skip_test` per-triple ungating (i386-linux, aarch64/armv7-linux).
- Normalize CMake incrementally (autodetect + fold `.cmake` files, verifiable target).
- Cut CI wall-clock: gate `bench`, shard macOS ctest, prune matrix re-runs, profile Windows jobs.
- Root-cause the named promote/inline gap tests.
- Link-time/ABI differential fuzzer (mix mcc `.o` with gcc `.o`).
- Coverage-guided generation (gcov/Intel-PT into `tests/fuzz/gen.h`).
- EMI mutation (Orion/Athena/Hermes) targeting optimizer miscompiles.
- Self-host differential (compile `src/mcc.c` mcc vs gcc, diff over corpus).

## Search vocabulary / strategy long tail
- §27 loop-nest precision: symbolic bounds, fewer non-affine bail-outs, asttool dep suite (blocked by `MCC_INTERNAL`).
- §27 true 2-D loop tiling (strip both loops).
- §29 range/known-bits lattice remaining (PR-3+).
- §31 adaptive beam width; per-function scoping.
- §24 hot-slice budget allocation (needs §22 emit isolation).
- §25 `-g` hot-value cache.
- §33b post-graft window dataflow decision (splice-then-reanalyze vs two-pass).
- §33d seam peephole window; §33e window-level cache key; §25 scoring of the de-spill delta.
- §32c speculative arm insertion (revisit only with 3-stage self-host gate).
- §32a widening/fixpoint dataflow for cross-iteration value merging.
- §30 `switch`-arm detection form.
- §23 inline budgets as a §22 search value-axis (needs 3A); more param shapes; inline cross-TU static callees; heuristic non-static inlining.
- §22 arena-mutating pass-subset re-emit axis; promotion re-emit axis (needs 3A).
- §28 rewrite-rule IR; §28 instruction-level superoptimization.
- V-strategy variations (bfold/ident/narrow/cprop/cse/licm/dse/sccp/jt/bf/sethi/tco) — widen search vocabulary; see git history for per-row holds.
- Build the `H_e` epoch hash (invertible slot-keyed edit patch).
- `.rodata` data-emission project (prerequisite for §30).
- Design cross-TU LTO; separate `-O2`/`-O3` SSA drivers; time-budgeted engine; dependency-ordered `-O1`; broader template library.
- M7 formula-family unification; M7b graduate the disk search-memo into compiled-in strategies.
- FLOAT: emit-size scoring under the tick scheduler + JIT-runtime scoring; C11-thread pool with per-context state (needs §22 emit isolation).
- Revisit: `k` always-inline depth policy; size-gated outline; store factoring; template DSL past ~30 templates; per-function `-O1`; PP-as-executable-C JIT; human-friendly diagnostics vs terminal geometry.
