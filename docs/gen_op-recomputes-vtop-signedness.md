---
name: gen_op-recomputes-vtop-signedness
description: "AST re-emit ignores a Binary node's recorded type — gen_op recomputes vtop signedness from operand usual-arithmetic-conversions"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 78b73548-cb59-49b4-bd97-c4d2a130c1b4
---

In mccast.c re-emit, `AST_Binary` lowers via `gen_op(bop)` (mccast.c ~4169), which
recomputes `vtop`'s type from the two operands' **usual arithmetic conversions** and
IGNORES the node's own `ast_set_type` recorded type. So any AST transform that builds a
Binary with mixed-sign children gets the operands' conversion signedness, not the type it
stamped on the node. Concretely: `S32 + U32` → **unsigned** int, and a later widening
(`(uint64_t)expr`) then zero-extends instead of sign-extending.

This bit `ast_divmagic_try_signed`: the signed-32 quotient was `q2(S32) + signbit(U32)`
(the `>>31` must be a logical/U32 shift), so the result went unsigned and a negative
quotient widened wrong. Fixed (`a1df06c1`) by wrapping `signbit` in an S32 `AST_Convert`
so the add stays signed. It's arch-neutral (decided in the shared AST layer) — showed as a
missing `movslq` on x86_64 and `uxtw`-vs-`sxtw` on arm64; needs a widening past 32 bits AND
register pressure (a lone `int d=x/C;` is fine because the low 32 bits are already correct).

Rule for future divmagic/AST-transform work: when a synthesized Binary's result signedness
matters downstream, force it with an explicit `AST_Convert` on the operand (or result) — do
not rely on `ast_set_type` on the Binary node. Repro/validate via Docker linux/amd64
(no native mcc-x86_64) + native arm64; see [[docker-amd64-repro]].
