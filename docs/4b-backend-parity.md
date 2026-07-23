---
name: 4b-backend-parity
description: "4B backend-parity landed — branchless select (MCC_AST_SELECT), 64-bit divmagic (MCC_AST_DIVMAGIC), cond?1:0 desync fix, + a latent reemit-driver bug fix"
metadata: 
  node_type: memory
  type: project
  originSessionId: 8aaf941c-37d2-40c4-9669-2e5fabff1ecc
---

Landed 2026-07-15 (commit `feat(codegen): 4B backend-parity`, main). The three docs/TODO.md "4B backend-parity" items, built in parallel worktrees and reconciled:

- **A — branchless select** for min/max/sign. New per-arch `gen_cmov(rt,rf,rb,ll)` behind a neutral `gen_select(CType*)` (src/mccgen.c), called from the AST replay `AST_If` case (mccast.c ~4208). x86_64 `test+cmovne`; arm64 `subs+csel`; riscv64 base-ISA `neg/xor/and/xor` mask (no Zicond in this backend). New strategy `AST_STRAT_SELECT` + recognizer sibling to `ast_abs_try`, with a strict purity gate (`ast_sel_safe` — cmov evaluates BOTH arms, so only side-effect-free/non-faulting arms qualify). Gated `MCC_AST_SELECT`, default OFF → byte-neutral. `exec-select` ctest fixture + `select_branchless` golden.
- **B — cond?1:0 desync fix** (NOT gated, affects all compiles). The `is_cond_bool` fast path in `expr_cond` (was mccgen.c:9944) returned before `ast_hook_ternary_end`, permanently desyncing the AST side-car for exactly those ternaries. Removed the fast path; `is_cond_bool` deleted. Now `use_in_expr`/`nested`/`x?1:0` capture as `faithful`. See [[reemit-faithful-gate]].
- **C — 64-bit div/rem strength reduction**. Granlund-Montgomery 64-bit magic in `src/mccmagic.h` + per-arch `gen_mulh(sign)` (x86_64 `mul/imul` r/m64→rdx; arm64 `umulh/smulh`; riscv64 `mulhu/mulh`) via synthetic `AST_OP_MULH{U,S}`. Gated `MCC_AST_DIVMAGIC`, default OFF.

**Latent bug fixed (important):** the reemit driver in `ast_func_end` (mccast.c ~13215) extracted per-strategy hit counts for the 12 cycle strategies but NOT the standalone ones (divmagic/abs/reassoc/range) — so a **divmagic/abs/select-only fold mutated the AST but was silently discarded** (baseline shipped) unless another opt co-fired. This is why even the pre-existing 32-bit magic never emitted in isolation. Wired `do_divmagic`/`do_select` (from `sf[AST_STRAT_*]`) into every reemit-gating conjunction. If you add a future standalone strategy, wire it into ALL those conjunctions or it won't reemit.

**Still default-off / not done:** the TODO item (b) "optimal single-multiply form for signed / a==1 cases" (needs real temp-materialization) — NOT addressed; C keeps the dup-based 2x mul-high. Default-on for all three still gated on broader soak. x86_64/riscv64 were codegen-inspected only, not cross-run — see [[native-host-validation-gotchas]].
