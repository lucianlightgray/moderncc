---
name: mcc-codegen-gap-is-regalloc
description: "Why mcc-generated code is 2-4x slower than clang and why -O levels/superopt don't help"
metadata: 
  node_type: memory
  type: project
  originSessionId: 86ab7fa9-71be-4b61-af5e-0dc7eb027861
---

Profiled 2026-07-26 (arm64 macOS) on vendor/plb kernels {nbody/2.c, spectral-norm/3.c, nsieve/1.c}, min-of-5 runtime vs Apple clang -O2.

**Findings:**
- mcc-emitted code is 2.1x (nsieve, int) to 4.0x (spectral, FP÷) slower than clang; nbody 3.3x. mcc -O0/-O1/-O2/-O3 are all within noise of *each other* — the AST optimizer gives ~0 hot-loop speedup.
- Root cause is the **backend, not AST passes**: mcc promotes scalar locals to registers mainly for *leaf* functions into a tiny caller-saved pool (3 GP x86_64). Any function with a call (e.g. nbody advance() calls sqrt) "falls off the regalloc cliff" (mccast.c:1870) → memory-resident locals. nbody inner loop: mcc 221 loads/58 stores vs clang 29/9. Callee-saved promotion exists but gated `MCC_AST_PROMO_LEAF_CALLEE=0`, leaf-only.
- `-O<N>` for N>3 = -O3 codegen + N-second gate search (`mcc_superopt_search`/`_perfn` in mcc.c). Default objective is emit **size**; `MCC_AST_JITSCORE=1` flips to measured µs/run. Both search a 2576×144×5 config space *above* the fixed backend, each candidate a full-program subprocess recompile (~1100/45s). Neither beats -O3 runtime (nbody: O3 4.13s, -O45 size 4.31s, -O45 runtime 4.20s). The slice-cache refactor targets that recompile cost model.
- Hardware sqrt inlining is behind default-off `MCC_AST_MATH_INLINE`; enabling it emits fsqrt but does NOT speed nbody up (memory traffic dominates) — proves regalloc is #1.
- C99 `inline` (non-static) mishandled: spectral-norm/3.c fails to link at every -O (`unresolved reference to '_A'`) — mcc neither inlines all uses nor emits an out-of-line body.

**Strength / the moat:** mcc compiles its own ~100k-line amalgamation ~24x faster than clang -O2 (2.2s vs 52s), flat across -O0..-O3 (0.47s CPU). Any runtime fix must preserve this → favor a linear-scan promotion pass, not clang-style graph coloring.

**Highest-leverage fix:** register promotion for non-leaf hot functions (linear-scan over AST-arena vregs). Worth more than all 35 AST gates + the -O60 search. See related [[mcc-arm64-macos-jit-dead]].

Runtime JIT self-recompile is dead on arm64 macOS (0 mccjit_* calls; --embed-jit can't resolve _mccjit_boot_swap), so the "JIT recompiling mcc" scenario isn't demonstrable on this host.

**IMPORTANT correction after deeper research (2026-07-26):** the project is already far ahead of the naive "add a register allocator" plan. docs/TODO.md `### FP/compute codegen gap` has extensive same-day x86_64/gcc work: MATH_INLINE_PREPASS + PROMO_LEAF_CALLEE (measured win), xmm8-15 modeling (`MCC_AST_XMM_HI`), `MCC_SO_SPILL_SCORE`+`MCC_SO_DEFAULT_SEED` (spill-aware -O4 search, nbody 3.0→2.4× gcc), IVSR_PTR — all landed but gated default-OFF pending golden-regen. Key nuance: GP non-leaf promotion ALREADY works; the real cliff is FP-in-non-leaf (`xmm_max = has_call ? 0`, mccast.c:4581). And `MCC_AST_MATH_INLINE` is default-ON x86_64 but default-OFF arm64 (mccast.c:1950) — why arm64-macOS shows `bl _sqrt`. The two named open big wins: field-offset addressing-mode fold + auto-vectorization.

Added 9 code-anchored TODO.md items (2026-07-26): arm64 MATH_INLINE flip; non-leaf FP promotion w/ per-call live-range splitting; MCC_AST_FMOV_IMM + MCC_AST_LICM_FP (loop-invariant FP const hoisting); superopt register-promotion actuator axis; auto-vectorization impl plan; runtime benchmark harness (Tests/infra); C99 plain-inline weak-body fix (Other AOT); arm64-macOS JIT Mach-O object reader (the real fix is macho_load_object_file, not mcc_add_symbol).

Report artifact: https://claude.ai/code/artifact/3e8d3ad9-44a6-412c-ba82-7fba6b2cdb41
