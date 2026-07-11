# tests/fuzz — differential random-program miscompile fuzzer

A whole-pipeline correctness engine for **mcc** (TODO.md §0). It generates
random UB-free C programs, compiles each through mcc and through `gcc` + `clang`
as a same-ISA majority-vote oracle, flags any divergence in observable behavior
(stdout + exit code), then **auto-reduces** and **auto-attributes** the offender
and graduates it into a permanent regression corpus.

It is tools-only: no compiler/codegen surface, so it carries none of the
byte-identity / self-host fixpoint risk of the ladder items, and it doubles as
the independent validation engine for the §41 default-ON gate sweep and a
miscompile detector for §29–§35.

## Pieces

| File | Role |
|------|------|
| `gen.h` | Deterministic, seed-driven generator. Emits a self-contained program that folds globals/functions/control-flow/bitfields/unions/arrays into a checksum, prints it, and returns it as the exit code. Every construct is UB-free by construction (unsigned wraparound; divisors forced non-zero; shift counts masked and operands widened to 64-bit; array indices masked in range). |
| `runner.c` | Orchestrator: generation → differential oracle → UBSan gate → reducer → triage → corpus. |
| `corpus/` | Graduated, reduced repros used as a regression lock. See `corpus/README.md`. |
| `bisect.sh` | `git bisect run` predicate wrapper around a corpus repro. |

## Oracle

For each program, `gcc` and `clang` at `-O2` establish a consensus. If they
disagree the program is dropped (implementation-defined / latent UB). Otherwise
mcc is swept over `-O0..-O3` (and, with `--gates`, each `MCC_AST_*` pass forced
on) and any mismatch with the consensus is a candidate miscompile. Before a
candidate is reported it is re-checked under `gcc -fsanitize=undefined,address`;
if the program has UB it is dropped, keeping the oracle sound.

## Running by hand

```sh
# from the build dir; mcc, bdir, idir as the other suites pass them
fuzz_runner <mcc> <bdir> <idir> <work> <gcc> <clang> [opts]

  --seed N       base seed          (env MCC_FUZZ_SEED, default 1)
  --count N      programs to try     (env MCC_FUZZ_COUNT, default 20)
  --gates        also sweep MCC_AST_* pass gates per program (env MCC_FUZZ_GATES)
  --corpus DIR   save reduced repros here / target of --replay
  --replay       regression-lock: replay every .c in --corpus
  --reduce FILE  ddmin-reduce FILE to a minimal repro (needs --corpus)
  --gen SEED     print one generated program to stdout and exit
  -v             verbose
```

A nightly campaign is just a large `--count` with `--gates --corpus
tests/fuzz/corpus`; runs are fully seed-reproducible.

## Triage

On a confirmed divergence the runner reduces the program (line-granularity
ddmin, guarded by re-confirming the divergence — a cvise/creduce fallback for
hosts without them) and sweeps `-O0..-O3 × MCC_AST_*` to name the exact level /
pass that flips it. `bisect.sh` extends this to `git bisect` over commits.

## ctest wiring

Three tests, all gated on a native x86_64 host with both `gcc` and `clang`
(they SKIP otherwise, reusing `MCC_DIFF3_GCC` / `MCC_DIFF3_CLANG`):

- `fuzz/smoke` — fixed seed band, `-O0..-O3`.
- `fuzz/matrix` — fixed seed band with the `MCC_AST_*` gate sweep (the §41 matrix).
- `fuzz/corpus` — replays the graduated corpus as a regression lock.
