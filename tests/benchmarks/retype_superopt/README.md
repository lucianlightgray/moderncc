# retype_superopt — prototype for the O4 type-substitution superoptimizer (T-lin-10455)

A proof-of-concept for a proposed `-O4` optimization: for a hot scalar
computation, blindly re-type the value/accumulator to every other candidate C
type, JIT-run each variant against the original to prove identical
input→output behavior, and keep the fastest variant that is proven-equivalent.

This directory drives the mechanism end-to-end **externally**, using `mcc -run`
as the JIT and `mcc -O4` as the AOT benchmarker, to de-risk the eventual
in-compiler pass (which hooks the KGC differential engine — see the DETAILS
anchor). It is not wired into the compiler; it is the experiment that proves the
idea is sound and measures the win before the real integration is built.

## Scripts

- `gen.sh <workdir>` — emit the sum-of-squares kernel in every candidate type.
- `run.sh [benchN]` — kernel 1 (`acc += i*i`): the **soundness** demonstration.
  Every non-`int64` retype is *rejected* because the extreme input samples
  (46341² overflows int32; 3037000500 exceeds the float/double mantissa;
  4294967296 wraps the 32-bit types) expose divergence. Only `int64_t` survives.
- `run_div.sh [benchN]` — kernel 2 (`r ^= 2000000000 / ((i & 0xffff)+1)`, all
  operands provably < 2³¹): the **positive adopt** demonstration. `int32_t`,
  `uint32_t`, `int64_t`, `uint64_t` are all proven-equivalent; `float`/`double`
  are correctly rejected (integer ≠ FP division); the proven `int32_t` retype
  benchmarks ~1.16× faster than the `int64_t` baseline (32-bit `idiv` latency).

## The soundness boundary (why this must be VLAT-gated in-tree)

Sampled JIT-equivalence is **necessary but not sufficient**: a narrow input
sample "proves" an unsound retype. The rejection in kernel 1 works only because
the sample set includes the range boundaries. The in-compiler pass must gate
candidate legality on the value-lattice (`ast_vlat_narrowing`, proves a value
provably fits a narrower type) *before* trusting the KGC runtime differential —
the JIT check is the second line of defense, not the first.
