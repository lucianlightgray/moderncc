# tests/fuzz — design notes / rationale

## Why a custom generator and reducer instead of csmith + cvise

csmith, cvise, and creduce are not installed on the dev/CI host (only `gcc`,
`clang`, and the sanitizer stack are). TODO.md §0 lists "a small custom
generator" as an explicit option, so the harness ships a self-contained C
generator and a line-granularity ddmin reducer with no external dependencies.
If csmith/cvise are later installed they can be dropped in front of the same
oracle and corpus without touching the oracle.

## Why integer-only, UB-free generation

The oracle demands that a *correct* program produce identical observable output
under gcc, clang, and mcc, so the generator must emit only programs whose
behavior is fully defined and implementation-independent:

- All arithmetic is on `unsigned long` (wraparound is defined). Signed values
  are only produced through explicit casts of in-range unsigned values.
- Division/modulo divisors are forced to `(x & 0xffff) + 1` — never zero, and
  unsigned so no `INT_MIN / -1`.
- Shift counts are masked to `& 63` and the shifted operand is widened to
  `unsigned long`, so the count is always `< width` (this was the one UB found
  during bring-up: a 32-bit `unsigned int` operand shifted by up to 63).
- Array indices are masked into `[0,8)`; no pointers, no uninitialized reads.
- Floating point is deliberately excluded: even IEEE basic ops can differ in
  contraction/rounding across compilers, which would produce oracle noise
  rather than real mcc bugs.

A UBSan/ASan re-check gates every reported divergence as a backstop, so any UB
that slips past the generator invariants is dropped rather than mis-reported.

## Reducer limitation

Line-based ddmin cannot remove a declaration and its (non-adjacent) use in one
step, so reduced repros may retain some dead scaffolding. The result is always
*valid and still-diverging* (the gcc+clang build guard rejects any broken
candidate); it is just not always minimal. This is the accepted trade for a
zero-dependency reducer.

## Accepted-divergence baselines

If a graduated repro turns out to be an implementation-defined difference rather
than an mcc bug, rename it `corpus/accept_<name>.c` and record why here.

(none yet — mcc agreed with the gcc==clang consensus on every seed of the
bring-up campaign.)
