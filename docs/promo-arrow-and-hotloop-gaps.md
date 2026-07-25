---
name: promo-arrow-and-hotloop-gaps
description: MCC_AST_PROMO_ARROW promotes pointers used via `->`; the remaining nbody advance() hot-loop gaps (addressing mode, LSR, reg pool)
metadata:
  node_type: memory
  type: project
---

**Landed 2026-07-25 (commit `d24ec4ab`) — `MCC_AST_PROMO_ARROW` (default OFF).** TODO AOT item (2).

`ast_plan_promotion`'s poison loop (mccast.c ~4406) poisoned a local for ANY unary with a local-Ref child (`coff[j]==off`, sz==0 for non-ADDR ops). `p->field` = `Unary(AST_OP_MEMBER_ARROW, Ref(p))`, so member access poisoned the pointer `p` → hot pointers (nbody `advance()` p=&b[i]/q=&b[j], highest promo weights 13/13) were never promoted. Fix: skip the poison for `AST_OP_MEMBER_ARROW` (a `->` only reads the pointer value + derefs to other memory; the pointer slot doesn't escape). Gated default-off ⇒ byte-identical. Verified p promoted to a reg (r10), accessed register-based.

**State of `advance()` (with `MCC_AST_OPASSIGN=1 MCC_AST_MATH_INLINE_PREPASS=1` so it's replayable + sqrt-inlined): 144 insns vs gcc 102** (was 184 pre-sqrt-inline). PROMO_ARROW promotes the pointers but barely moves the headline count because of THREE stacked gaps (rough impact order):

1. **(2c) member-access addressing mode** — mcc emits `mov p,%rax; add $off,%rax; mov (%rax)` (~3 insns/field) instead of `mov off(%p),…` (1 insn). Likely the biggest remaining win; a base+displacement codegen change (arch-specific). Compounds with the now-working pointer promotion.
2. **(2a′) tiny leaf promotion pool** — only 3 GP + 2 XMM caller-saved (`ast_promo_caller`/`ast_promo_xmm`). gcc also uses callee-saved (rbx/r12-r15 + xmm w/ save/restore) in hot leaves. Expanding the leaf pool (emit push/pop) would let mcc hold all of {p,q,i,j,dx,dy,dz,d2,mag}.
3. **(2b) LSR** — 2 residual `imul $0x38` for `&b[i]`/`&b[j]` (gcc 0). A genuinely new loop-strength-reduction pass.

None alone closes the gap; they stack. Validation recipe: [[docker-optimizer-mcc-validation]]. Related gates: [[opassign-recorder-fix]], [[math-inline-prepass]]. All three optimizer gates are default-off pending cross-arch + AOT==JIT-arm64 + fuzz soak before flipping. Push constraint: [[push-to-main-needs-authorization]].
