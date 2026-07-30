# `tests/gpu` — GPU-accelerated exhaustive value sweeps

**Not registered with CTest. Run it by hand.**

```sh
tests/gpu/run.sh              # full: N = 2^32
QUICK=1 tests/gpu/run.sh      # fast: N = 2^24
MCC=/path/to/mcc tests/gpu/run.sh
```

Exits `0` on success, `1` on failure, `77` when Vulkan, `glslc` or a GPU is
unavailable. Nothing here is wired into `CMakeLists.txt`, no `add_test` mentions
it, and no `file(GLOB)` in the build reaches this directory — every glob in
`CMakeLists.txt` is scoped to `tests/exec`, `tests/cst/fixtures`,
`tests/optfire`, `tests/diff/parts` or `tests/preprocess`. Adding files here
cannot change the ctest name set.

## Why it exists

Design notes and the decision table live in [`GPU.md`](GPU.md).
The short version: a full type × value conformance suite has to decide what
"100% value coverage" can mean, and that is settled by arithmetic rather than
effort. This directory holds the measurements that settled it.

| Domain | Size | Verdict |
|---|---:|---|
| 8/16-bit, unary and binary | ≤ 4.29e9 | exhaustive, seconds |
| 32-bit unary (incl. every `float` bit pattern) | 4.29e9 | exhaustive, ~10 s CPU / 10 ms GPU |
| 32-bit binary, any 64-bit | ≥ 1.8e19 | unreachable |

Measured cost of a full 2⁶⁴ sweep: **941 years** on one CPU core, **65 years**
on 16, **~1.6 years** on an RTX 5070 Ti. It is not a budget problem.

## What is actually covered

### `cpu/` — single-threaded exhaustive sweeps

- `sweep_int32.c` — every one of the 2³² bit patterns interpreted as both
  `int` and `unsigned`, confirming full range on each.
- `sweep_int64.c` — the 64-bit *lattice* (all 64 bit positions observed set and
  clear) plus a 2³² throughput probe used for the extrapolations above.
- `sweep_float.c` — every one of the 2³² `float` bit patterns, classified.
  Counts match the closed form exactly: `2` zeros, `2` infinities,
  `2·(2²³−1)` subnormals, `2·(2²³−1)` NaNs, `2·254·2²³` normals. Also asserts
  that exactly the NaNs compare unequal to themselves, and that
  `float→double→float` is bit-exact for every pattern **except** the
  `2·(2²²−1)` signaling NaNs, which are quieted. That last one is correct
  IEEE-754 behaviour, not a defect, and nothing else in `tests/` exercises it.

### `cpu/rev64_mt.c` — threaded reference

Bit-reversal over a strided 64-bit sweep, checked by two self-verifying
properties (`rev(rev(x)) == x`, popcount preserved) so no expected-value table
is needed. Takes `<threads> <count>`.

Aggregates are XOR, wrapping add and a popcount total — all associative *and*
commutative, so the result is **identical regardless of thread count**. An
earlier version folded per-thread FNV checksums sequentially and produced
different answers for 1 and 32 threads; any parallel cell that does that has
turned thread count into a hidden input and cannot be a golden.

Measured: 14.5× on 16 cores, then flat to 32 — the host is 16 physical cores
with SMT, and this kernel is ALU-latency-bound, so the second thread per core
finds no idle slots.

### `rev64_vk.c` + `rev64.comp` — GPU differential

Same sweep on the GPU. 64-bit values are carried as `uvec2`, so no
`shaderInt64` feature is required; reversal is `bitfieldReverse` per half with a
swap, and `i * ODD` uses `umulExtended`. Each workgroup XOR-reduces through
shared memory and issues one `atomicXor`, rather than 4.29e9 of them.

The GPU `xsum` must equal the CPU `xsum` bit-for-bit. It does, at every size
tested, across two entirely independent implementations. Sustained ~360 G/s,
flat from 2³² to 2⁴².

### `halves_vk.c` + `halves.comp` — the half-split cross-product

The interesting result. Rather than sweeping 2⁶⁴, sweep each 32-bit half
exhaustively while the other half walks a 139-value lattice, in both
directions: **1.19e12 values in ~4.2 s**, a 15-million-fold reduction against
2⁶⁴.

For **separable** operations this is not a sample but a *proof*.
`rev64(hi:lo) = rev32(lo):rev32(hi)`, so each output half depends on exactly
one input half, and sweeping each half over all 2³² values exercises every
mapping the function has. The same holds for popcount, the bitwise operators
and byte swap. Full coverage of a 2⁶⁴ domain at square-root cost.

For **coupled** operations it is not complete in principle. The shader also
checks `mul⁻¹(mul(x)) == x` using the multiplicative inverse of `ODD` mod 2⁶⁴,
which needs no reference table and exercises the full 64×64 multiplier.

## `known_positive/` — required failures

Per rule 18 of `docs/TODO.md`, a `0` from an instrument means nothing until the
instrument has produced a `1` on a known positive. These two shaders are
deliberately broken and `run.sh` **fails if they pass**:

- `crossterm.comp` — drops the `a.y * b.x` partial product.
- `bothhalves.comp` — perturbs the result only when both halves are nonzero.

Both were written to prove the half-split has a blind spot. **Both were
detected anyway**, which is the more useful finding:

- For a 64-bit multiply's low half, every partial product involves at most one
  half of `a`, so the two-direction sweep reaches each term even with the other
  half pinned to zero.
- The round-trip oracle computes an intermediate `m = x·ODD` whose halves are
  both populated even when `x`'s are not, so the second multiply is exercised
  regardless of how the input was split.

The residual gap is therefore narrower than expected: bugs keyed on a specific
*pair* of input halves with no intermediate mixing. That is what the 139-value
lattice cross-product covers.

## Known limitations

1. **The failure counters are 32-bit and wrap.** `bad_rev`/`bad_pop`/`bad_mul`
   are `uint` atomics. A run with more than 2³² failures reports a meaningless
   number — an observed run reported `4294967157` for ~1.19e12 actual failures.
   The zero-versus-nonzero verdict is sound; **the magnitudes are not**. Fixing
   it needs 64-bit atomics (`shaderInt64`) or a saturating clamp.
2. **This does not test mcc's code generation.** The GPU independently
   reimplements the operation, so it validates the algorithm and the reference
   model, not mcc's output. Its value is as a fast model-checker for expectation
   tables before they are baked into kernels as literals.
3. **`rev64_vk.c` supports `total` up to 2⁶⁴** via a 64-bit base, but
   `halves_vk.c` sweeps a 32-bit half and takes a 32-bit base.
4. Timings below ~50 ms are dominated by clock ramping; the 2³² GPU figure
   varies 0.010–0.021 s run to run.

## Why it stays manual

A Vulkan loader, a GPU driver and a physical device are three new ways for CI
to fail for reasons that have nothing to do with mcc. This suite should not
gate a compiler commit.
