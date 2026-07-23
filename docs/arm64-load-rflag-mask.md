---
name: arm64-load-rflag-mask
description: "arm64 load() uses exact-match on sv->r, so every new front-end r-flag must be added to its mask"
metadata: 
  node_type: memory
  type: project
  originSessionId: fd76f05b-cd72-4af1-bb46-2fb3059735f3
---

In `src/arch/arm64/arm64-gen.c`, `load()` decodes the value-stack register as
`int svr = sv->r & ~(VT_BOUNDED | VT_NONCONST | VT_NONLVAL)` and then compares
`svr` against *exact* patterns (`svr == (VT_LOCAL | VT_LVAL)`, etc.). The other
backends (x86_64/i386/arm/riscv64) instead test `fr & VT_VALMASK` plus
individual bits, so stray high `r`-flags are harmless there — but on arm64 any
unmasked flag bit makes `svr` match no case and hit `assert(0)` in `load`.

**Why:** any NEW front-end-only flag stored in the `r` field (the VT_NONCONST /
VT_NONLVAL family, 0x1000/0x2000) MUST be added to this mask (and the twin in
`arm64-asm.c` `arm64_memory_needs_address_reg`), or arm64 codegen crashes while
the qemu/x86 matrix stays green — so it slips past CI that only runs on x86.

**How to apply:** when adding a value-stack `r` annotation, grep `sv->r & ~(` in
`src/arch/arm64/` and extend the mask. Regression that caught this:
commit bf96980d added VT_NONLVAL but not the arm64 mask; `int a=g().x` /
`struct c=g()` aborted on the arm64 host. See [[macos-arm64-status]].

**Instance FIXED (2026-07-10): VT_VLA (0x400).** AST-replay optimized
self-host of `src/mcc.c` on arm64 (`-O1`/`-O2`/`-O3`) aborted:
`load(1, (32, 400, 0))` → `arm64-gen.c:650 assert(0)`, `sv->r == reg0|VT_VLA`
(mcc.c uses VLAs). Fixed by `76407be9` (§34): added `VT_MUSTCAST` (0xC00, whose
0x400 bit == VT_VLA) to the `arm64-gen.c:494` `svr` mask. `-O1+` arm64
self-host now runs. Confirms the rule: front-end r-flags must be added to this
mask; `-O0` `fixpoint-invariant` never exercises AST-replay so these slip past
CI until run natively at `-O1+`.

**RETRACTED (2026-07-10): the "`-O3` codegen non-determinism" was a harness
bug, not real.** `mcc -O3 -c src/mcc.c` gives a byte-identical object twice,
and the executable is identical when compiled to the *same* `-o` name. mcc
embeds the **output basename** into the binary, so compiling to `me1` vs `me2`
differs by exactly 2 bytes (`'1'` vs `'2'`) — my throwaway fixpoint harness
compiled each stage to a different name and read that as divergence. The real
`fixpointgate` renames a fixed name, so it is immune. With the same name, the
`-O3` 3-stage self-host reaches a byte-identical fixpoint under both
`MCC_AST_INLINE=1` and `=0` → **TODO N2 box-3 is DONE** and §32c's fixpoint
gate is available. Lesson: when hand-rolling a self-host fixpoint check,
compile every stage to the SAME `-o` name (basename is embedded).
