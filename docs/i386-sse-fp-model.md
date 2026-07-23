---
name: i386-sse-fp-model
description: "mcc-i386 uses SSE FP (FLT_EVAL_METHOD=0), not x87; codegen diffs need gcc -mfpmath=sse"
metadata: 
  node_type: memory
  type: project
  originSessionId: 10e773c4-467a-4c00-ba6c-1c5adf74523e
---

mcc-i386 evaluates floating point in SSE registers (FLT_EVAL_METHOD 0, same as gcc -m64),
NOT the x87 stack. `gcc -m32` defaults to x87 (FLT_EVAL_METHOD 2, 80-bit excess-precision
intermediates), so any FP-rounding-sensitive program legitimately diverges between mcc-i386
and a plain `gcc -m32` reference — not a codegen bug.

Consequence: the i386 codegen differential (`tools/i386diff-docker.sh`) must build its gcc
reference with `-msse2 -mfpmath=sse` to share mcc's FP model. Surfaced by the d3 program
(20-iter float/double accumulation) giving mcc=187 vs gcc-x87=188; with SSE they agree.
Verified: all 14 programs match at -O0 and -O2 in a linux/386 container. See
[[i386-fpic-tls-miscompile]] and [[cross-arch-exec-matrix]].
