# tests/benchmarks — threading benchmarks

C11 `<threads.h>` benchmarks used to compare mcc's two threading backends
against native toolchains:

| toolchain  | threads.h backend                         | parallelism      |
| ---------- | ----------------------------------------- | ---------------- |
| `gcc`      | glibc `<threads.h>` (pthread)             | real, multi-core |
| `clang`    | glibc `<threads.h>` (pthread)             | real, multi-core |
| `mcc-nat`  | mcc `<threads.h>` → pthread wrappers      | real, multi-core |
| `mcc-coop` | mcc `<threads.h>` → `-DMCC_THREADS_COOP`  | cooperative fibers (single core) |

Run the whole matrix at -O0/-O1/-O3 (output-verified, perf-instrumented):

```
tests/benchmarks/run.sh [N] [REPS]      # e.g. tests/benchmarks/run.sh 2000 3
```

## Kernels

Both are ports of the OpenMP entries in the Programming-Language-Benchmarks
corpus (`vendor/plb/bench/algorithm/spectral-norm/`), with the OpenMP
constructs rewritten to C11 `<threads.h>` so the same source builds under every
backend above:

- **`spectral_norm_forkjoin.c`** — from `spectral-norm/3.c`. Each
  `#pragma omp parallel for` becomes a fork-join: spawn `NT` workers over
  disjoint row ranges, join. Exercises `thrd_create`/`thrd_join` churn (many
  short-lived parallel regions).
- **`spectral_norm_barrier.c`** — from `spectral-norm/4.c`. A single long-lived
  team of `NT` workers runs the whole iteration loop, synchronizing phases with
  a reusable `mtx_t`/`cnd_t` barrier (`#pragma omp barrier` → condition-variable
  wait/broadcast). The SSE `vector_size(16)` math is de-vectorized to scalar
  doubles (mcc has no GCC vector types); the computed value is identical.

Both print the spectral norm; `run.sh` checks it against a serial reference.

### Porting notes / why only these two

Of the OpenMP plb C files, only spectral-norm 3.c and 4.c are cleanly portable
here: `spectral-norm/{5-im,6-im}.c` use `<emmintrin.h>` SSE intrinsics,
`mandelbrot/1-mffi.c` needs OpenSSL (`<openssl/md5.h>`), `binarytrees/2.c` needs
Apache APR (`apr_pools.h`), and `knucleotide/1.c` reads a FASTA file from stdin.

`<threads.h>` is included **first** and libc functions (`malloc`, `sqrt`, …) are
declared by hand instead of via `<stdlib.h>`/`<math.h>`: mcc's coop backend
defines its own `once_flag`, which collides with the `once_flag` glibc's
`<stdlib.h>` pulls in (`bits/types/once_flag.h`). Declaring the few functions we
use keeps one source building under all four toolchains.

Thread count is `NT` (default 4); override with `-DNT=<n>` (the runner builds
the serial reference with `-DNT=1`).
