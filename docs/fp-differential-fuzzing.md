---
name: fp-differential-fuzzing
description: "how to fuzz mcc's floating-point codegen against clang without false positives, and the fabs(-0.0) signed-zero bug class it found"
metadata:
  node_type: memory
  type: reference
  originSessionId: d8e85f80-7e78-45fe-a389-93af6d22f450
---

The integer fuzzer (`tests/fuzz/gen.h`) emits NO floating-point — FP codegen was
un-fuzzed. FP *differential* fuzzing is doable WITHOUT false positives if you control
the two sources of legitimate cross-compiler FP divergence:
1. **x87 excess precision** — avoid by fuzzing on **arm64** (this host; no excess
   precision) or x86_64 with SSE. i386 is UNRELIABLE for FP differential (x87).
2. **FMA contraction** — compile the reference with **`-ffp-contract=off`** so `a*b+c`
   is discrete mul+add, matching mcc (which does not fuse). Also `-fno-fast-math`.

With those, IEEE `+,-,*,/,sqrt` are correctly-rounded and **bit-identical** across
mcc and clang. Recipe (no docker, native arm64-macOS): generator at
`_fp/fpgen.py` emits provably-finite, NaN-free programs (guarded `/`, `sqrt(fabs())`,
clamp non-finite via `fin()`), bit-folds every local to an exit-code checksum (a
1-ULP diff flips it). Differential: `cmake-release/mcc -O2 p.c` vs
`clang -ffp-contract=off -fno-fast-math -O2 p.c`, run both natively, compare exit.
Only one real reference (clang) needed; the runner's 2-ref consensus is optional.
NOTE: this macOS host has NO real gcc (`/usr/bin/gcc` is a clang shim).

First bug it found (FIXED 2026-07-30, commit 3249304a): `__builtin_fabs(-0.0)`
returned -0.0. `__builtin_fabs/fabsf/fabsl` (runtime/include/mccdefs.h) and
`foldm_fabs` (mccgen.c) lowered fabs as `x < 0 ? -x : x`; `-0.0 < 0` is FALSE so the
sign bit survived. Fixed: macros `<= 0 ? 0.0 - (x) : (x)` (0.0-x canonicalizes both
zeros to +0.0); foldm_fabs clears the sign bit via a union mask. **Bug class to audit:
any `x<0?-x:x` on a FLOAT type mishandles -0.0** (integer abs is fine). mccdefs.h is
baked into mcc via c2str, so a rebuild propagates header fixes to every target.
Relates to the FP/compute codegen gap in docs/TODO.md.

AUDIT (2026-07-30, post-fix): the `x<0?-x:x` float-sign class is CLOSED — fabs was the ONLY buggy instance. Verified bit-exact vs clang that trunc/floor/ceil/round/fmin/fmax/copysign all give correct signed-zero results, signbit(-0.0)=1, and isnan/isinf/isfinite/isnormal/fpclassify/fmin/fmax/fdim are correct on Inf/NaN/subnormal. FP arithmetic (~2444 double+float seeds) and FP<->int conversions (fcvtzs/scvtf, ~220 seeds) also fuzz-clean.
