# tests/benchmarks — C11 `<threads.h>` benchmark suite

Self-contained C ports of the Programming-Language-Benchmarks corpus
(`vendor/plb/bench/algorithm/`), used to compare mcc's code generation and its
two threading backends against native toolchains. Every kernel is ONE `.c` file
that builds unchanged under all of:

| toolchain     | threads.h backend                                   | parallelism                     |
| ------------- | --------------------------------------------------- | ------------------------------- |
| `gcc`         | glibc `<threads.h>` (pthread)                       | real, multi-core                |
| `clang`       | glibc `<threads.h>` (pthread)                       | real, multi-core                |
| `mcc-nat`     | mcc `<threads.h>` → pthread wrappers                | real, multi-core                |
| `mcc-coop`    | mcc `<threads.h>` → `-DMCC_THREADS_COOP`            | cooperative fibers (single core)|
| `mcc-coop-mn` | mcc coop + `-DMCC_COOP_MN` (M:N over nproc workers) | cooperative fibers, multi-core  |

## Running

```
tests/benchmarks/run.sh ["N1 N2 …"] [REPS]     # e.g. run.sh "1000 2000 4000" 3
```

`run.sh` builds every `*.c` here under all five toolchains at `-O0..-O3` (mcc
also `-O4`), then for **each input N** runs each build, checks its output against
a serial (`-DNT=1`) reference, and cross-checks the gcc and clang serial
references against each other. It reports `perf instructions:u` (stable) and
best-of-REPS wall-clock (advisory), and prints a final `SUMMARY`. A build that
hangs is caught by a per-run `timeout` and reported `HANG/TIMEOUT` rather than
blocking the run. Default inputs are `1000 2000 4000`; the first arg overrides.

Each kernel takes the work-scale `N` as `argv[1]` (default 2000). The scales are
tuned so runtime at `N=2000` is comparable across kernels (~ms–tens of ms).

## Kernels

All kernels lead with `#include <threads.h>` and hand-declare the few libc
functions they use instead of `#include`-ing `<stdlib.h>`/`<math.h>`/… — mcc's
coop backend defines its own `once_flag`, which collides with the one glibc's
`<stdlib.h>` pulls in. Output is deterministic and identical for any `NT` (the
`-DNT=1` serial reference must match every parallel build), so large streams are
folded to an FNV-1a checksum rather than printed. No third-party libraries.

| kernel                   | from plb            | threading (C11)                               | N = | output |
| ------------------------ | ------------------- | --------------------------------------------- | --- | ------ |
| `spectral_norm_forkjoin` | spectral-norm/3.c   | fork-join per `omp parallel for` region       | matrix dim | spectral norm |
| `spectral_norm_barrier`  | spectral-norm/4.c   | one long-lived team + `mtx`/`cnd` barrier     | matrix dim | spectral norm |
| `nsieve`                 | nsieve/1.c          | 3 independent sieve sizes on 3 workers        | ×1000/500/250 | prime counts |
| `mandelbrot`             | mandelbrot          | disjoint row bands, one worker per band       | image N×N | PBM checksum + setbits |
| `fannkuch_redux`         | fannkuch-redux      | permutation-index space split into NT blocks  | → n=8+N/1000 (≤11) | checksum + Pfannkuchen(n) |
| `binarytrees`            | binarytrees         | independent depths/phases across NT workers   | → depth 10+N/300 (≤16) | CLBG depth lines |
| `merkletrees`            | merkletrees         | independent tree jobs across NT workers       | → depth 10+N/300 (≤16) | root/verify lines |
| `coro_prime_sieve`       | coro-prime-sieve    | pipeline, one fiber per prime, `mtx`/`cnd` channels | # primes | Nth prime |
| `knucleotide`            | knucleotide/1.c     | 7 independent count tasks across NT workers   | ×1000 bases (gen in-proc) | freq tables + counts |
| `nbody`                  | nbody/2.c           | serial (step-sequential)                      | ×1000 steps | initial+final energy |
| `fasta`                  | fasta               | serial (LCG PRNG is sequential)               | ×1000 bases | base count + checksum |
| `lru`                    | lru                 | serial (shared cache state)                   | ×1000 ops | hit/miss counts |
| `edigits`                | edigits             | serial bignum (base-10⁹ limbs)                | ×8 digits of e | digit groups |

The DNA for `knucleotide` is generated in-process with the standard CLBG fasta
LCG (so no stdin file is needed); `mandelbrot`/`merkletrees` use a self-contained
integer hash instead of the OpenSSL/crypto the original plb entries pull in.

## Known backend limitation

`coro_prime_sieve` spawns one pipeline fiber **per discovered prime**, so its
live-fiber count grows with N. Under `mcc-coop-mn` (M:N) a cooperative fiber that
blocks on a channel `cnd_wait` currently blocks its underlying pthread **worker**
instead of parking the fiber and reusing the worker — so once the pipeline is
deeper than `nproc` workers the whole team starves and deadlocks (observed
threshold ≈ nproc). It runs correctly under `gcc`/`clang`/`mcc-nat` (real
preemptive threads) and `mcc-coop` (a single scheduler round-robins all fibers).
`run.sh` therefore **skips only the `coro_prime_sieve` × `mcc-coop-mn` pairing**
(printed as a `SKIP` line) and everything else runs. This is a runtime bug, not a
kernel defect — tracked as **T-lin-10525** (fix: fiber-park-on-block in the M:N
scheduler in `runtime/include/mcc_coop_threads.h`).
