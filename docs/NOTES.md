# ModernCC — Notes & Detailed Overview

Supplementary reference material moved out of the top-level
[README](../README.md) to keep it focused on getting started. This file
collects the non-essential overview information: toolchain comparisons,
build-status reports, per-toolchain test coverage, and performance benchmarks.

## Comparisons

`Y` = supported, `~` = partially supported, `-` = not supported.

| Target / Format             | mcc | gcc | clang | mingw | msvc |
|-----------------------------|:---:|:----:|:-----:|:-----:|:---:|
| x86_64                      |  Y  |  Y   |   Y   |   Y   |  Y  |
| i386                        |  Y  |  Y   |   Y   |   Y   |  Y  |
| arm                         |  Y  |  Y   |   Y   |   ~   |  Y  |
| arm64                       |  Y  |  Y   |   Y   |   Y   |  Y  |
| riscv64                     |  Y  |  Y   |   Y   |   -   |  -  |
| ELF output                  |  Y  |  Y   |   Y   |   -   |  -  |
| PE/COFF output              |  Y  |  Y   |   Y   |   Y   |  Y  |
| Mach-O output               |  Y  |  ~¹  |   Y   |   -   |  -  |
| Multi-target from one build |  Y  |  Y   |   Y   |   -   |  -  |

¹ `gcc` with Apple patches supports Mach-O

---

| Capability                              | mcc | gcc | clang | mingw | msvc |
|-----------------------------------------|:---:|:---:|:-----:|:-----:|:----:|
| Compile + link in one step              |  Y  |  Y  |   Y   |   Y   |  Y   |
| `-run` (execute in memory, no a.out)    |  Y  |  -  |   -   |   -   |  -   |
| Embeddable compiler library             |  Y  | ~²  |  ~³   |   -   |  -   |
| Integrated assembler (no external `as`) |  Y  |  -  |   Y   |   -   |  Y   |
| Inline asm / `asm goto`                 |  Y  |  Y  |   Y   |   Y   |  ~⁴  |
| Runtime backtraces (`-bt`)              |  Y  |  ~  |   ~   |   ~   |  ~   |
| glibc + musl via `--sysroot`            |  Y  |  Y  |   Y   |   -   |  -   |
| Optimizing codegen                      |  -  |  Y  |   Y   |   Y   |  Y   |
| C99                                     |  Y  |  Y  |   Y   |   Y   |  Y   |
| C11                                     |  Y  |  Y  |   Y   |   ~   |  Y   |
| Single-pass / fast compile              |  Y  |  -  |   -   |   -   |  -   |
| Tiny footprint (~1 MB)                  |  Y  |  -  |   -   |   -   |  -   |

² via `libgccjit`
³ via `libclang`
⁴ MSVC has no inline `asm` on x64 (intrinsics only)

---

| Toolchain  | Notes                                          |
|------------|------------------------------------------------|
| **clang**  |                                                |
| **gcc**    | requires cross-compilers to target multi-archs |
| **mingw**  | same as gcc                                    |
| **MSVC**   | breaks C99/C11 standards; quirky               |
| **mcc**    | self-hosts and cross-compiles                  |

---

| libc                   | Via                                  | Coverage              |
|------------------------|--------------------------------------|-----------------------|
| **glibc** (ELF)        | `--sysroot` / default                | full                  |
| **musl** (ELF)         | `--sysroot` / auto-detected fallback | full                  |
| **msvcrt** (PE)        | Windows/PE target                    | wine + native Windows |
| **libSystem** (Mach-O) | macOS/Darwin target                  | qemu + native MacOS   |

## Platform ABI & runtime notes

**`long double`.** mcc follows each target's platform ABI, matching the native
compiler byte-for-byte:

| Target                              | `long double`            |
|-------------------------------------|--------------------------|
| x86_64 / i386 (ELF)                 | 80-bit x87 extended      |
| arm64 / riscv64 (Linux ELF)         | 128-bit IEEE quad        |
| **arm64 Darwin (Apple)**            | **64-bit — same as `double`** |
| **any PE / Windows target (MSVC ABI)** | **64-bit — same as `double`** |

On Apple arm64 and on all PE targets, `long double` is `double`
(`MCC_USING_DOUBLE_FOR_LDOUBLE`), which is the Apple / MSVC ABI, not an mcc
limitation — `sizeof(long double) == 8` there, agreeing with clang / MSVC.
The `cli/apple_arm64_long_double_is_double` test pins this on macOS arm64.

**`<math.h>` is host-provided.** mcc ships no freestanding `runtime/include/math.h`;
math declarations come from the host/target libc's `<math.h>`, and the math
functions link against the host `libm` (glibc/musl `-lm`, Apple libSystem, or
msvcrt). A non-glibc *freestanding* host with no `<math.h>` must supply one;
mcc does not synthesize the header or the transcendental functions.

**Mach-O thread-local storage.** A `__thread` variable *defined* in the module
compiles to a proper TLV descriptor (`__tlv_bootstrap`) and works. Referencing an
`extern __thread` variable *defined in another module/dylib* is an intentional
limitation — mcc errors "Mach-O: external thread-local '<x>' is unsupported"
rather than emitting a TLV import descriptor. Keep cross-module thread-locals
inside one TU on Darwin, or expose them through an accessor function.

**win32 runtime shims are intentional subsets** (`runtime/win32/include`):
`<pthread.h>` mutexes are non-recursive (`PTHREAD_MUTEX_RECURSIVE` is accepted
but behaves as a plain `SRWLOCK`), thread-key destructors are not run at thread
exit, and there is no cancellation; `<sched.h>` is a minimal shim, not a full
POSIX scheduling interface; `<fenv.h>` has no rounding-control-register access
on non-x86/arm64 PE arches (only the default rounding mode). Callers targeting
win32 should not rely on the missing semantics.

**Inline-assembler coverage is a modelled subset, not a full ISA.** mcc's
integrated inline assembler models the operations real C inline `asm` needs
(memory barriers, register moves, loads/stores, arithmetic, branches, syscalls),
not the entire target instruction set — it is not a drop-in for a standalone `as`
on hand-written assembly. Unmodelled mnemonics hard-error with the exact name
(`"ARM64 instruction '<m>' not implemented"` / the ARM equivalent), so the
boundary is self-documenting.
- **arm64** (`src/arch/arm64/arm64-asm.c`): supported = `add(s)`/`sub(s)`,
  `and(s)`/`orr`/`eor`, `asr`/`lsl`/`lsr`/`ror`, `mul`, `mov`/`movk`/`movn`/`movz`,
  `mrs`/`msr`, `ldr(b/h)`/`ldp`/`str(b/h)`/`stp`, `b`/`b.<cond>`/`bl`/`blr`/`br`/
  `cbz`/`cbnz`, `adrp`, `isb`/`dsb`/`dmb`, `ret`, `nop`. Not modelled (error):
  comparisons (`cmp`/`cmn`/`tst`), conditional-select (`csel`/`cset`), division
  (`udiv`/`sdiv`), multiply-add (`madd`/`msub`), FP/SIMD arithmetic
  (`fadd`/`fmov`/…), negation aliases (`neg`/`mvn`), and atomics (`ldxr`/`stxr`).
- **arm (32-bit)** (`src/arch/arm/arm-asm.c`): 64-bit (`long long`) inline-asm
  *operands* are unmodelled — the 64-bit GPR-pair case hard-errors. Use two 32-bit
  operands, or a `vmov`/memory round-trip, for 64-bit values in ARM inline asm.

Expanding either table toward full ISA coverage is deferred until a real
inline-asm workload needs a specific missing mnemonic; the error names exactly
what to add.

**i386 `__fastcall` / `__thiscall` argument ordering.** A register-eligible
integer argument that follows a stack-spilled argument in a `__fastcall`
(`ecx`,`edx`) or `__thiscall` (`ecx`) call is an **accepted limitation**: mcc
errors "fastcall with a non-register argument before an integer register argument
is not supported" (`src/arch/i386/i386-gen.c`) rather than assigning the trailing
register slot out of order. In practice the affected shapes are rare (a large
by-value aggregate or an over-`ecx`/`edx`-width value ahead of a small integer in
a Microsoft-convention prototype); the diagnostic is the pinned boundary. The
straight-ahead register-then-stack ordering — the overwhelmingly common case — is
fully supported (`i386-fastcall-abi` test, i386 cross target). Revisit only if a
real prototype with the spilled-then-register ordering surfaces.

## Build status

**Linux status (2026-07-07, gcc 15.3.0 / clang 22.1.8):** every Linux preset is
green — `debug`, `release`, `sanitize`, `diagnostics`, `cross`, `matrix`
(gcc/clang × native/cross superbuild), all 15 `linux-*` CI presets, both
`dist-linux-*` packagings, and the full `qemu` cross×libc matrix.

**Counting basis — per-case (`ctest -N`).** The suite registers one CTest per
case: the `exec`/`cli`/`diff3`/`parts`/`preprocess`/`ast` corpora each fan out to
one test per row, and (with `CONFIG_AST` default-ON) the AST replay driver adds
its `exec-replay/*` + `exec-replay-tmpl/*` columns over the whole `exec` corpus.
On the host of record the `debug` preset registers **1447** cases: **1279 run and
pass, 168 environment-gated self-skip, 0 fail** (the skips are the cross-toolchain,
`wine`, `macho`, `qemu`, and network-`*-fetch` cases that are absent on a bare
Linux host — they self-skip rather than fail, and the exact total tracks upstream
test additions). Other presets differ only by their skip set: `asm-off`
legitimately registers fewer (its integrated-assembler cases drop). The `qemu`
matrix runs all 5 arches × glibc+musl + `qemu-arm64-osx` once the `*-fetch` steps
download the target sysroots. With the `cross` toolchain built (`MCC_CROSS_DIR`,
default `cmake-cross`), the wine PE-conformance and the six host-runnable Mach-O
drivers run natively and pass too.

**Windows status (2026-07-08, main@ab7f5ee9, mingw gcc 13.1 / MSVC 19.51 / clang 22):**
every Windows-runnable preset is green. The suite is registered one CTest
per case (the `exec`/`cli`/`diff3`/`parts`/`preprocess`/`ast` corpora fan out —
now including the `CONFIG_AST` `exec-replay`/`exec-replay-tmpl`/`exec-replay-promote`
replay columns, so the Windows basis matches the Linux one), so the counts are
per-case: every native preset registers **~1696** cases with **0 failures** and
~180–200 environment-gated self-skips (the exact total tracks upstream test additions
— new platform-gated cases register and self-skip here; the whole-corpus
`exec-replay-promote` column that `d6740df9` added is the ~281-case jump from the
prior 1415 basis). Regenerated from an actual Windows `ctest` run: `debug`, `ast`
and `cst` = **1515 run / 181 skip** (1696 registered), `cross` = **1517 / 179**,
`release` = **1494 / 202**, `diagnostics` = **1516 / 181** (1697 — +`sanitize-smoke`),
`sanitize` = **1516 / 181** (1697) and `sanitize-msvc` = **1495 / 200** (1695).
`msvc` (VS generator, cl-built) = **1515 / 179** (1694): it runs the very same
**1515** cases as `debug` and all pass — it registers two fewer skip-stubs only
because the two macOS-only `macho-*` cross cases (`macho-conformance-native`,
`macho-stack-protector`) aren't emitted under the VS generator (they
register-and-self-skip on the Ninja presets). On the MSVC
host `mcctest` still registers and passes — its gcc reference auto-resolves to
the vendored winlibs GCC (`MCC_REF_CC`) — so the MSVC pass count tracks the
mingw hosts. Those totals assume the vendored clang toolchain is present (`cmake
--build <bld> --target clang-toolchain`, or drop it under `vendor/llvm-clang`;
auto-wired on the next reconfigure); without it the ~260 three-way
`diff3`/`preprocess` cases self-skip. The `cli`/`diff3` suites are now
**self-sufficient for their shell helpers**: on WIN32 the harness prepends both
the resolved `MCC_TEST_SH` directory (which bundles `grep`/`sed`/`printf`) and a
resolved `nm`/`readelf` directory (found on `PATH`, else the vendored winlibs
`bin`) to each case's `PATH` via `ENVIRONMENT_MODIFICATION`, so the suites pass
without the invoker adding a UNIX-tools or binutils directory to `PATH` (git
`sh` + `cmake` alone suffice; on an MSVC host the binutils come from the
vendored winlibs). `mingw` (superbuild; fetches the pinned winlibs GCC and tests
with it) and `matrix` (gcc/clang × native/cross — 4 cells, each on the same
per-case registration as the native/cross presets above, clang resolved from the
fetched `vendor/llvm-clang`) are green too. Both `dist-*` packagings
build the full artifact matrix — mcc + `-static`/`-dynamic` +
`libmcc-static`/`-dynamic` + all 11 cross compilers; `dist-mingw` additionally
ships each cross compiler in a fully-static (`-static`) shape, while `dist-msvc`
keeps only the host `mcc-static` (`-static` is a GNU-ld flag `link.exe` doesn't
take, so the per-cross static shapes are intentionally skipped under MSVC). The
in-tree build/CI tools carry their own ctests that pass here too —
`host-gate-invariant`, `git-stamp`, `def-verify`, `build-md-nodes`,
`config-defines`, `host-detect`, `cross-factory`, `ci-matrix`, `ci-pkg-smoke`,
`qemu-fetch-parse`. **`sanitize` now runs on Windows too** (commit `ccba8d22`): it is
no longer a configure-fatal there. On **mingw** it resolves to **trap-mode UBSan**
(`-fsanitize=undefined -fsanitize-trap=undefined -fno-sanitize=alignment`) — no
libasan/libubsan runtime needed, and `-fno-sanitize=alignment` drops mcc's one
intentional unaligned-access trip — while the new **`sanitize-msvc`** preset builds an
**MSVC AddressSanitizer** (`/fsanitize=address`) `mcc_s`. A `sanitize-smoke` ctest
compiles+links+runs a program with the instrumented `mcc_s`; both are green on the
post-promote per-case basis (see the counts above): `sanitize` mingw **1516 / 181**
(1697) and `sanitize-msvc` **1495 / 200** (1695). `diagnostics` still builds the coverage +
profile variants (and skips `mcc_p` on Windows/Darwin). The PE target gets native-only
extra coverage
(`pe-native-conformance`, `compile.win32.*`); remaining skips are
environment- or libc-gated with reasons (wine, macOS, X11, ELF-emitting
32-bit reference, the osx/arm64/riscv64 cross drivers when the `cross`
toolchain isn't built, msvcrt's reduced libm/complex surface for
`parts/*`, and PE bounds-checking for `mcctest-bcheck`). The `linux-*`
presets, `dist-linux-*` packagings, and the qemu grid also run from Windows,
via the Docker runners (`tests/ci/docker`, `tests/qemu/docker`).

**mingw-w64 SEH miscompile of `gen_function` (fixed):** `05685c99` gave
`gen_function` a nested `setjmp` (the AST-replay error trap). On a mingw-gcc
`-O2`/`-O3` build that made `RtlVirtualUnwind` fault nondeterministically (~30%)
whenever an `error1()` `longjmp` to the outer per-file trap (`mcc_compile`)
unwound *through* `gen_function`'s frame — i.e. any error-recovering compile,
e.g. `exec/errors_and_warnings`, `exec-replay/atomic_misc`,
`exec-replay-promote/nodata_wanted` under `-dt`. It only bit the `mingw` CI job
(ELF/Mach-O `longjmp` does no stack unwinding, and MSVC's own setjmp is
unaffected). Fix: build just `gen_function` unoptimized on mingw-gcc hosts
(`__attribute__((optimize("O0")))`, gated `MCC_HOST_WIN32 && __GNUC__ &&
!__clang__`) — the hot codegen work stays optimized in its `block()`/`gexpr()`
callees, so the cost is negligible.

## Per-toolchain test coverage

`P` = passes, `S` = skipped-with-reason (environment/config-gated, not a
failure), `—` = not applicable.

| `ctest` suite | Win mingw | Win gcc | Win msvc | Lin gcc | Lin clang | mac clang |
|---|:--:|:--:|:--:|:--:|:--:|:--:|
| `exec/*` (golden run/diff)            | P | P | P | P | P | P |
| `mcctest`¹                            | P | P | P | P | P | P |
| `mcctest-bcheck`¹                     | S | S | S | P | P | P |
| `sanitize-smoke`¹¹                    | P | P | P | P | P | P |
| `preprocess/*`²                       | P | P | P | P | P | P |
| `diff3/*`²                            | P | P | P | P | P | P |
| `parts/*`² (per-unit 3-way diff)⁹     | S | S | S | P | P | P |
| `cli/*` (readelf/nm structural)³      | P | P | P | P | P | P |
| `libtest` / `-extra` / `-mt`, `abitest-cc` | P | P | P | P | P | P |
| `hello-run` / `hello-exe`, `vla_test-run`  | P | P | P | P | P | P |
| `compile.*` (orphan `-c`)⁴            | P | P | P | P | P | P |
| `asm-c-connect-test`                  | P | P | P | P | P | S |
| `dash-s-roundtrip` (`-S` → asm → run)¹⁰ | P | P | P | P | P | S |
| `asm-gas-directives`⁵                 | S | S | S | S | S | S |
| `i386-fastcall-abi`⁶                  | S | S | S | P | P | S |
| `compile.win32.*` / `pe-native-conformance` | P | P | P | — | — | S |
| `pe-wine-conformance` (label `wine`)⁷ | S | S | S | P | P | S |
| `macho-*` (8 drivers, label `macho`)⁷ | S | S | S | P | P | P |
| qemu cross×libc matrix (label `qemu`)⁸| S | S | S | P | P | S |

¹ Differential vs. a GCC-compatible reference cc (needs the integrated
assembler); MSVC host auto-detects a mingw/winlibs `gcc` (`MCC_REF_CC`).
`-bcheck` variant also needs `MCC_CONFIG_BCHECK`, and skips on the PE/msvcrt
target, where mcc bounds-checking is unsupported (faults in msvcrt
callbacks/library calls).
² Needs **two distinct** references (gcc *and* clang) or the three-way
differential self-skips. On macOS `gcc`/`cc` are the Apple clang shim, so a
genuine Homebrew `gcc-<n>` (installed by `setup-gcc`) is auto-detected. On
Windows, Linux (x86_64/arm64) and Apple-silicon macOS no system clang is
needed: `cmake --build <bld> --target clang-toolchain` fetches a pinned,
SHA256-verified LLVM into `vendor/llvm-clang/`, auto-wired by the next
reconfigure (both suites then run and pass); `ci local` runs the fetch
automatically when it probes no system clang.
³ Needs POSIX `sh` (`MCC_TEST_SH`) + `nm`/`readelf`; ~31 ELF-image cases
self-skip on a PE target. On WIN32 the harness auto-prepends the `sh` directory
(for its bundled `grep`/`sed`/`printf`) and a resolved `nm`/`readelf` directory
(system `PATH`, else the vendored winlibs `bin`) to each case's `PATH`, so no
UNIX-tools/binutils directory need be on the invoker's `PATH`.
⁴ X11 example (`compile.ex4`) skips when `<X11/Xlib.h>` is absent.
⁵ Always skipped: integrated assembler lacks a few GAS encodings
(`sgdtq`/`sidtq`/`swapgs`).
⁶ Needs the i386 cross compiler (`mcc-i386`, preset `cross`) + an ELF-emitting
32-bit reference cc; skips on Windows (mingw `gcc` emits PE/COFF).
⁷ Both need the cross toolchain (preset `cross`, or a populated
`MCC_CROSS_DIR` — default `cmake-cross`): wine + the win32 cross
compilers for `pe-wine-conformance`; the osx cross compilers +
`llvm-otool`/`otool` for the Mach-O drivers. Linux: six host-runnable drivers
pass (`macho-structural`, `macho-codegen-run`, `macho-image-run`,
`macho-apple-libc`, `macho-stack-protector`, and `macho-universal` — the last is the
`machofat` fat-binary combiner, 1-slice; its 2-slice case self-skips without the
`x86_64-osx` cross + SDK); `macho-conformance-native` and
`macho-libsystem-kernel-fused` skip (need Darwin/darling — the latter also gated on
`MCC_DARWIN_HOST=ON`). macOS: `macho-structural`, `macho-conformance-native`,
`macho-stack-protector`, and `macho-universal` are native (the last shelling to
`xcrun --show-sdk-path`); the Linux-approximation drivers self-skip off x86_64.
⁸ Windows runs it via the Docker runner; a Linux host with `qemu-user` runs it
natively (`ctest -L qemu`).
⁹ Native 3-way per-unit differential of each `tests/diff/parts/run_*.c`
wrapper; needs a shared C99 libc across gcc/clang/mcc, so the PE/msvcrt
target skips (the same units are covered in aggregate by `mcctest`).
¹⁰ Hermetic `mcc -S` → mcc's own assembler → mcc link → run, byte-identical
to the direct build; needs x86_64 + the integrated assembler (skips on arm64
macOS). With the cross compilers built, `dash-s-bytes-{arm64,riscv64}`
additionally assert **object-byte-exact** `-S` roundtrips on the fixed-width
targets (i386/arm are instruction-exact; their assemblers legally re-encode
some widths/branch fields).
¹¹ Registered only when the separate `mcc_s` is built (the `sanitize`/
`sanitize-msvc`/`diagnostics` presets, `MCC_BUILD_SANITIZE=ON`); `—`/absent on the
plain presets. The instrumentation differs by host: **Lin gcc/clang** ASan+UBSan,
**Win mingw** trap-mode UBSan (no runtime lib), **Win msvc** ASan (`/fsanitize=address`),
**mac clang** ASan+UBSan (a Homebrew GNU-`gcc` host instead skips `mcc_s` — no linkable
libasan/libubsan). `-fno-sanitize=alignment` throughout (mcc's one intentional
unaligned-access idiom).

## Compile speed & footprint

Compiling `mcc`'s own whole-compiler TU (`src/mcc.c`, `SINGLE_SOURCE=1`, ~108 k
lines) to an object, best of 3, on the host of record (Ryzen 9 8940HX, Gentoo
6.18, gcc 15.3.0 / clang 22.1.8, release `mcc`, 2026-07-07). Stripped release
binaries: dynamic `mcc` ≈ **0.72 MB**, `mcc-static` (musl) ≈ **1.45 MB** (both up
from the pre-CST/AST 0.6 / 1.3 MB — the CST and AST side-channel subsystems are
now compiled in by default).

| Compiler | Time | vs mcc |
|---|--:|--:|
| **mcc**       | **0.07 s** | 1× |
| clang `-O0`   | 0.35 s | 5× slower |
| gcc `-O0`     | 0.95 s | 14× slower |
| clang `-O2`   | 5.17 s | 76× slower |
| gcc `-O2`     | 6.17 s | 91× slower |

## Profiling — measured results & lexer-optimization findings

Moved from [PROFILING.md](PROFILING.md), which keeps the reproducible *method*
(§1 the builds, §2 the compile invocations, §3 the timing method). These are the
results and analysis: the seven-build spread, `perf` attribution, the applied
lexer optimization, its validation, and the conclusions. Section numbers (§4–§8)
and cross-references are preserved from `PROFILING.md`. Host of record: Linux
x86-64 (Gentoo, kernel 6.18), 32 cores, gcc 15.3.0, clang 22.1.8; reference date
2026-07-06. Re-measured with every `mcc` built as a plain `-O0 -g` debug build
(sanitizers off) and the static/musl self-host rows added — see §1/§4e.

### 4. The spread — nine builds, two workloads

All figures ms unless noted, mean ± σ over ≥40 runs (≥8 for the multi-second
`-O2` amalgamation), pinned to core 2, quiet-system config (§3a). Lower is
better. Baseline for every ratio is **`mcc-self`**. **Every `mcc` build here is a
plain `-O0 -g` debug build** with sanitizers off (§1) — so the `mcc` rows measure
a *debug* compiler; a release (`-O2`) `mcc` runs ~3–4× faster still, widening
every gap below.

#### 4a. `full_language.c` (small TU — the `-O0` differential fixture)

| config        | preprocess `-E` |  compile `-c` |  compile→exe |     run exe |
|---------------|----------------:|--------------:|-------------:|------------:|
| gcc-debug     |    34.67 ± 0.41 |  500.0 ± 1.7  |  530.2 ± 1.9 | 19.56 ± 0.12|
| gcc-release   |           n/a ¹ |         n/a ¹ |        n/a ¹ |       n/a ¹ |
| clang-debug   |    41.49 ± 3.99 |  331.7 ± 1.3  |  379.7 ± 1.6 | 20.99 ± 0.22|
| mcc-gcc       |    24.08 ± 0.29 |  71.43 ± 0.81 | 75.38 ± 0.46 |           — |
| mcc-clang     |    21.91 ± 0.34 |  92.44 ± 0.42 | 97.53 ± 0.91 |           — |
| **mcc-self**  |    22.14 ± 0.26 |  68.78 ± 0.43 | 72.67 ± 0.23 | 21.47 ± 0.22|

`run exe` is codegen-identical across the three compilers (§1): mcc 21.47, gcc
19.56, clang 20.99. Ratio vs `mcc-self`, `compile→exe`: gcc-debug **7.3×**,
clang-debug **5.2×**. `mcc-self` builds the TU end to end in 73 ms — 7.3× under
gcc's `-O0` and 5.2× under clang's `-O0`. (The old RelWithDebInfo spread put
`mcc-self` at 33 ms / 16×; the mcc binary is now `-O0`-built, hence slower, but
the reference compilers are unchanged so the *shape* holds.)

> ¹ `gcc -O2` cannot build `full_language.c`. Its `__bug_table` inline-asm
> (`tests/diff/parts/legacy_meta.h:364`) defines a *global* symbol inside
> `get_asm_string()`; at `-O2` gcc inlines that function into every caller and
> emits the symbol once per copy → assembler `symbol 'some_symbol' is already
> defined`. The fixture is genuinely `-O0`-only (`clang -O2` compiles it via the
> `#ifndef __clang__` branch but the binary then `SIGABRT`s on the same
> assumptions). Preprocessing is optimization-independent, so a `-release -E`
> would equal `-debug`'s. The optimized-compile spread lives on `src/mcc.c` (§4b).

#### 4b. `src/mcc.c` (large TU — the amalgamation, carries the release columns)

| config          | preprocess `-E` |   compile `-c` | `-c` vs mcc-self |
|-----------------|----------------:|---------------:|-----------------:|
| gcc-debug       |     129.0 ± 1.3 |    2245.0 ± 6.7 |           8.0×  |
| gcc-release     |       = debug ³ |   16058 ± 25   |           57.4×  |
| clang-debug     |     99.0 ± 0.6  |    1108.3 ± 2.5 |           4.0×  |
| clang-release   |       = debug ³ |   13395 ± 118  |           47.9×  |
| mcc-gcc         |     106.7 ± 0.6 |   290.5 ± 7.8  |            1.04× |
| mcc-clang       |      96.5 ± 0.5 |   369.0 ± 1.0  |            1.32× |
| **mcc-self**    |      97.3 ± 1.0 |   279.9 ± 0.8  |            1.00× |
| mcc-musl        |     114.0 ± 0.5 |   307.4 ± 6.9  |            1.10× |
| mcc-static      |     113.5 ± 1.0 |   304.9 ± 0.8  |            1.09× |

`mcc-self` compiles the entire ~100 k-line amalgamation to an object in **280 ms**;
`gcc -O2` needs **16.1 s** for the same TU — a **57×** gap — and `clang -O2`
13.4 s (48×). Even against a plain `-O0` reference, a *debug* `mcc` is 4.0–8.0×
faster; a release `mcc` (the shipped build) widens that severalfold. This is the
near-`-O0`, integrated-assembler design paying off, and the gap grows with TU
size and opt level.

> ³ preprocessing is optimization-independent, so `gcc-release`/`clang-release`
> `-E` equal their `-debug` values (measured once each).

The three new libc rows — **`mcc-musl`** (musl-linked, dynamic), **`mcc-static`**
(musl, fully static), and their `mcc-gcc`/`mcc-clang`/`mcc-self` glibc siblings —
are all now producible (§4e); the musl-linked `mcc` binary runs ~10 % slower than
its glibc twin (`307` vs `280` ms `-c`; the musl allocator on this workload), and
static vs dynamic musl is within noise (`305` vs `307`).

#### 4c. The self-host cost inverts at `-O0`

With every `mcc` built `-O0` (not RelWithDebInfo), the "which compiler built
`mcc`" comparison flips from the old RelWithDebInfo result:

- **`mcc-self` is now the *fastest* glibc `mcc`.** It compiles the amalgamation in
  **280 ms** and preprocesses in **97 ms** — beating the gcc-`-O0`-built `mcc-gcc`
  (290 / 107 ms) and the clang-`-O0`-built `mcc-clang` (369 / 96 ms). `mcc`'s own
  near-`-O0` codegen produces a *faster* `mcc` binary than `gcc -O0` or `clang -O0`
  do; `clang -O0` is the worst of the three (369 ms `-c`).
- **The old "~1.7× bootstrap cost" was an artifact of the RelWithDebInfo baseline.**
  When `mcc-gcc`/`mcc-clang` were `-O2` builds they beat `mcc-self` 1.7×; at an
  even `-O0` footing the direction reverses — `mcc`'s codegen is competitive with,
  and here ahead of, an unoptimized gcc/clang. Since all three emit byte-identical
  code (§1), this is purely a speed-of-the-driver effect. (A release `mcc` would be
  faster than all of these; the `-O0` spread is what this doc measures.)

#### 4d. Memory — peak RSS (`/usr/bin/time -v`, compile `-c` of `src/mcc.c`)

```sh
taskset -c 2 /usr/bin/time -v <compile -c cmd> 2>&1 | grep 'Maximum resident'
```

| config        | peak RSS |
|---------------|---------:|
| gcc-debug     |  157 MB  |
| gcc-release   |  329 MB  |
| clang-debug   |  144 MB  |
| clang-release |  242 MB  |
| mcc-gcc       | 29.4 MB  |
| **mcc-self**  | 29.7 MB  |
| mcc-musl      | 29.3 MB  |
| mcc-static    | 28.7 MB  |

`mcc`'s footprint is ~**29 MB** where `gcc -O2` needs **329 MB** (11×) and
`clang -O2` 242 MB — flat across all `mcc` builds (same allocator, same codegen),
so the libc/link choice in §4b costs *time*, not *memory*. (This is up from the
prior ~12 MB figure: the CST subsystem is now on by default and the `-O0` `mcc`
binary is larger; the relative story is unchanged.)

#### 4e. The static / musl self-host variants — now producible

All three extra rows in §4b are real builds now, not `n/a`. Two things changed
since the earlier "environment-limited" note:

- **A musl sysroot is built from vendored source, on any host.**
  `cmake --build <dir> --target vendor-musl musl-sysroot` clones musl `v1.2.5`
  and builds `vendor/musl-sysroot` (headers + crt + `libc.a/.so` + the
  `ld-musl-*.so.1` loader). The `MCC_BUILD_MUSL` variants bake that loader as the
  interpreter, so **`mcc-musl`** is a genuine musl-linked binary
  (`readelf -l` → `…/vendor/musl-sysroot/lib/ld-musl-x86_64.so.1`), not a glibc
  one. The self-hosted musl `mcc` is built with `mcc-musl` as the compiler
  (`CMAKE_C_COMPILER=…/mcc-musl`, `MCC_TOOLCHAIN_PROFILE=mcc`).
- **`mcc`'s own `-static` self-link was fixed** (2026-07-06). It previously left
  `__ehdr_start`, the 128-bit soft-float `__multf3/…`, and `_Unwind_*` unresolved;
  `mcc` now synthesizes `__ehdr_start`, carries the soft-float + weak unwind stubs
  in `mccrt`, and resolves static IFUNC/TLS. So **`mcc-static`** — a fully static,
  self-hosted musl `mcc` — links and runs (`mcc-musl -static`, no interpreter).

One caveat remains: a fully-static link against **glibc** (not musl) now gets all
the way through IFUNC resolution and TLS setup but then hits a glibc `libc.a`
archive-member gap (`__pthread_initialize_minimal`), so the static row measured
here is the **musl** one; glibc-static via `mcc` is tracked in `docs/TODO.md`.
The gcc-driven `linux-gcc-static` preset still builds a glibc `mcc-static` fine.

### 5. Where the time goes — `perf` self%

With `perf_event_paranoid=1` (§3a), user-space sampling needs no root. Attribute
against the gcc-built profiling compiler (`cmake-prof-gcc`, now `-O0 -g`, which
symbolizes cleanly with no inlining) on the include-heavy amalgamation:

```sh
P=cmake-prof-gcc
taskset -c 2 perf record --call-graph=fp -o perf.data -- \
  sh -c "for i in \$(seq 1 40); do $P/mcc $ARGS -o /tmp/pp.o; done"   # $ARGS = §2b
perf report -i perf.data --stdio --no-children | grep -E '\[\.\]|\[k\]' | head -12
```

The `-E` and `-c` profiles now diverge sharply, because the **CST subsystem is on
by default** (`MCC_CST`) and its per-node hash-consing runs only in the compile,
not in bare preprocessing:

```
compile -c (src/mcc.c):            preprocess -E (src/mcc.c):
  21.8%  cst_mix64      <- CST        18.2%  next_nomacro   <- lexer core
   8.5%  cst_hash_bytes    hash        9.4%  get_tok_str
   6.2%  next_nomacro                  4.5%  tok_str_add2
   3.0%  cst_build_sourcefile          4.4%  _IO_fputs (libc; -E output)
   2.1%  tok_str_add2                  4.2%  mcc_preprocess
   1.4%  preprocess_skip               3.7%  preprocess_skip
   1.3%  unary / cst_node_at           2.7%  next
   ~1%   cst_render_node / cst_alloc_node / cst_leaf_kinded …
```

**Two different hot spots depending on phase.** In `-E`, the lexer still dominates
(`next_nomacro` + `next` ≈ 21%) exactly as before — the §6/§8 lexer story holds
for preprocessing. But in a full `-c`, **CST hash-consing is now #1**:
`cst_mix64` + `cst_hash_bytes` are **~30%** of self-time (the amalgamation
`#include`s every `src/*.c`, and each file is interned into a hash-consed
`SourceFile` template — see docs/CST.md), pushing `next_nomacro` down to 6%. On an
include-heavy compile the CST hasher, not the lexer, is the first thing to
optimize (tracked in `docs/TODO.md`).

#### 5a. Which instructions inside `cst_mix64` (perf annotate)

```sh
perf annotate -i perf.data --stdio -s cst_mix64 | sort -rn | head
```

`cst_mix64` is a multiply-xor hash finalizer; its cost is exactly the mix itself —
the `imulq %rdx,%rax` and the two `xorq %rax,-8(%rbp)` fold steps are ~23%
combined, with the trailing `retq` (call overhead, `-O0`) another ~10%. It is
called once per interned CST node, so the win is *fewer calls* (coarser hashing
granularity / caching) rather than a cheaper mix.

The whitespace-skip loop (mccpp.c ~2779) **does not appear** — it is cold on this
TU.

### 6. Lexer optimization — what's applied, and what doesn't fit

The lexer is already tight enough that per-token dispatch micro-optimizations
land within the ±1% measurement noise on this TU, and `perf annotate` shows why —
they do not touch the two hot instruction clusters in §5a. Of four "less work per
token" changes considered, only idea #1 is applied:

| # | Idea | Effect (preprocess `-E`) | Status |
|---|------|--------------------------|--------|
| 1 | Cache the interned `TokenSym` in `next_nomacro`; read `ts->sym_define` in `next()` instead of re-deriving via `define_find(t)` | neutral, but strictly fewer ops/identifier, no downside | **applied** |
| 2 | SWAR word-at-a-time scanning | whitespace SWAR **slightly negative** (single-space gaps dominate → SWAR preamble is pure overhead on a cold path); identifier SWAR unfit (below) | **not applied** |
| 3 | Fold `hi\|=c` UTF-8 high-bit detect into an `IS_UTF8` char-class bit | neutral; adds one first-char table load (the original has the char free in-register) → marginally *more* work | **not applied** |
| 4 | Hoist `parse_flags & PARSE_FLAG_ASM_FILE` out of the number-scan loop | neutral; the term is already short-circuited off the hot digit path and numbers are cold | **not applied** |

**Why #2's identifier variant does not fit (beyond the measurement):** the
id-continue predicate `isidnum_table[c] & (IS_ID|IS_NUM)` covers multiple
disjoint ranges *and* two entries (`$`, `.`) that `set_idnum()` toggles at
runtime (mccpp.c ~4004) — so a hardcoded SWAR predicate would be wrong under
`#pragma`/asm-mode. And because short identifiers dominate, a separate SWAR
boundary pass + separate hash pass is two passes where there is now one → a
likely regression. SWAR is the textbook lever but it does not fit this lexer.

#### The kept change (idea #1)

`src/mccpp.c`, +3/−1:

```c
#define tok_ts (mcc_state->tok_ts)           // MCCState member (embeddable/thread-safe)

/* in next_nomacro(), where both identifier paths converge: */
tok_ts = ts;
tok = ts->tok;

/* in next(): */
if (t >= TOK_IDENT && (parse_flags & PARSE_FLAG_PREPROCESS)) {
        Sym *s = tok_ts->sym_define;         // was: define_find(t)
```

**Correctness:** every token value `>= TOK_IDENT` (256) is produced by the
identifier path in `next_nomacro`, which always sets `ts` — the special tokens
(`TOK_PPNUM=0xcd`, `TOK_PPSTR=0xce`, `TOK_TWOSHARPS=0xa3`, `TOK_EOF=-1`, …) are
all `< 256`. So whenever `next()` takes the `t >= TOK_IDENT` branch, `tok_ts` is
the freshly-interned symbol and `tok_ts->sym_define` is exactly what
`define_find(t)` (`table_ident[t-TOK_IDENT]->sym_define`, mccpp.c ~1343) would
return — minus the subtract, bounds-check and array load.

### 7. Validation matrix

The change is validated across the **20 Linux-runnable presets** (each the full
per-case CTest suite — see [Build status](#build-status) for the counting basis;
the `debug` preset registers 1447 cases / 1279 run / 168 env-gated skips as of
2026-07-07, `asm-off` legitimately fewer), all **100% green**:

```
debug  sanitize  diagnostics  linux-gcc  linux-clang
linux-gcc-multisource  linux-gcc-asm-off  linux-gcc-predefs-off
linux-gcc-pie  linux-gcc-dwarf  linux-gcc-diagnostics
linux-gcc-release  linux-clang-release  linux-gcc-sanitize  linux-gcc-static
release  linux-gcc-musl  cross  linux-gcc-cross  linux-clang-cross
```

The 5 `qemu-*` presets are **excluded**: they fail identically at baseline
because their `*-fetch` steps download target glibc/musl sysroots, which needs
network the host lacks (environmental, not a code failure). macOS / MSVC / mingw
presets do not run on a Linux host.

Per-implementation smoke sets for the affected lexer areas:

```sh
ctest --preset debug -R 'mcctest|preprocess|lex|ident|whitespace|comment|string'   # lexer
ctest --preset debug -R 'utf8|ucn|ident|raw_utf8'                                    # idea #3
ctest --preset debug -R 'integer|float|hex|literal|suffix|asm|imaginary|complex'     # idea #4
```

### 8. Conclusions — where a *measurable* win would come from

- For **preprocessing** the lexer is the right place to look (`next_nomacro` is #1
  by a wide margin), but the per-token dispatch micro-costs targeted by ideas
  #1–#4 are already below the noise floor on a 416-line TU. For a full **compile**
  the picture changed: CST hash-consing (`cst_mix64`, §5) is now the hottest code
  on include-heavy input and is the higher-value target.
- The two genuinely hot instruction clusters are the **identifier interning hash
  chain walk** and the **per-byte hash computation** — not whitespace, not
  number scanning, not the `define_find` lookup. A measurable win has to come
  from those, not from more per-token dispatch micro-tuning.
  - **Applied (2026-07-05): `TOK_HASH_SIZE` 16384 → 65536.** A `hyperfine` A/B on
    identifier-dense 2 MB TUs measured a consistent **1.03–1.06× faster** `mcc -E`
    (statistically significant, ±0.02–0.04) by lowering the intern-hash load factor
    so the chain walk shortens. The intermediate 32768 showed no realistic-density
    gain (threshold effect). Cost is a *fixed* +384 KB hash table that does not
    scale with input, so the low-RSS profile is preserved; correctness is unaffected
    (still a power of two for the `& (TOK_HASH_SIZE-1)` mask; full suite green).
    `TOK_HASH_FUNC` itself was left alone — it is already a cheap shift-add-mix.
- **The larger input separates signal from noise.** 416 lines is too small to
  separate a 1–2% lexer change from noise. The amalgamation spread (§4b) does
  better: `src/mcc.c` preprocesses in ~107 ms (`mcc-gcc`) with σ ≈ 0.6 ms — a
  ~0.6 % floor vs `full_language.c`'s ~1 %, and 4× the absolute signal — so a
  lexer change shows up there long before it clears noise on the small TU. Lexer
  experiments belong on `-E src/mcc.c`, not `full_language.c` (tracked in
  `docs/TODO.md`).

- **The full spread (§4) reframes "fast" quantitatively.** Even a *debug* (`-O0`)
  `mcc` beats `gcc -O2` on the amalgamation by **57×** in time and **11×** in peak
  RSS, and the gap grows with TU size, opt level, and a release `mcc` build. At an
  even `-O0` footing `mcc`'s own codegen is no longer a bootstrap *cost*:
  `mcc-self` is the **fastest** of the three glibc `mcc` builds (§4c), ahead of a
  gcc-`-O0`- or clang-`-O0`-built `mcc`. Making the shipped (`-O2`) `mcc` faster
  still is a codegen-backend project, separate from the front-end work above.

## C99/C11 conformance — mcc project status: deliberate differences & limitations

Moved from the appendix of [C9911.md](C9911.md), whose clause-by-clause map
remains the source of truth for *what the standards require* and *the
per-compiler status*. This records the things that are mcc-project-specific
rather than standard-derived: mcc's conformance philosophy, host/platform
limitations, and its **deliberate** documented differences from gcc/clang (`[~]`
items kept on purpose). Marker legend: `[ ]` open · `[~]` partial/deliberate-DIFF.

### Conformance philosophy & severity tags

Goal: implement every detail of C99 (N1256) and C11 (N1570), backed by regression
tests across all targets (x86_64, i386, ARM, AArch64, RISC-V 64), platforms
(ELF/PE/Mach-O), and both glibc and musl. The default standard is **C11**
(`cversion=201112`, set in `mcc_new()` at `src/libmcc.c:911`, and predefined as
`__STDC_VERSION__`); `-std=c99` selects C99, `-std=c17`/`-std=c23` the later ones.

mcc's stance on constraint violations is **permissive-by-default**: many
constraint violations are emitted as a *warning* (a diagnostic of any severity
satisfies the standard's "shall ... a diagnostic" requirement) and are upgraded
to hard errors under `-Werror`; `-pedantic`/`-pedantic-errors` add the ISO
pedantic diagnostics. gcc/clang choosing to make some of these hard errors by
default is their policy, not a standard mandate — hence several `[~]`/`[DIFF]`
entries below are *conformant* differences, not defects.

Test discipline: every fix ships with a cli/exec/diff regression test; the suite
(`ctest`, the cli+exec suites, the diff3 differential suite, the macho/wine/qemu
matrices) and the byte-identical 3-stage self-host fixpoint stay green. The
project has a history of false-positive audit entries — re-verify any candidate
gap 3-way against the live binary before acting on it.

Severity tags used below: **[BUG]** wrong codegen/crash/hang/rejects-valid ·
**[FEATURE]** absent standard feature · **[DIAG]** undiagnosed or
wrong/misleading diagnostic · **[OPT]** standards-optional · **[TASK]**
test/infra · **[LIMITATION]** host/platform constraint outside mcc · **[DIFF]**
deliberate, tested difference from a reference compiler (not a defect).

### Platform / host limitations

- [ ] **[LIMITATION] The kernel-fused Mach-O / libSystem path needs a macOS or darling host.**
  The self-contained Mach-O tests run on any host: structural validation
  (`macho-structural`), Darwin-codegen execution against host glibc
  (`macho-codegen-run`), Mach-O image execution via the in-repo loader
  (`macho-image-run`), and Apple's own string/memory + self-contained printf
  sources run as Mach-O images (`macho-apple-libc`). The remainder of
  `libSystem` is kernel-fused and unrunnable off Darwin: `libmalloc`
  (magazine/nano/xzone zones on `os_unfair_lock`/Mach VM), FILE-backed/locale
  `stdio` (`xlocale`/`__sFILE`/gdtoa), and `dyld`/`pthread`/GCD/ObjC/Mach IPC.
  These require `-DMCC_DARWIN_HOST=ON` (a macOS or darling host); the boundary
  is documented in `tests/qemu/apple-libc/PROVENANCE.md`.

- [ ] **[TASK] Systematic diagnostic-coverage sweep.**
  Each `[DIAG]`/`[FEATURE]` item ships with a cli/exec test that asserts the
  diagnostic fires and that the valid forms still compile. A full mechanical
  parity sweep across all ~312 `mcc_error`/`mcc_warning`/`mcc_pedantic` call
  sites in `src/` — to confirm the diagnostics that predate this tracker each
  have a regression test — remains an ongoing audit.

- [ ] **[DIFF] diff3 differential divergences on macOS.**
  The `diff3` suite compiles each golden with `mcc`, gcc, and clang and flags
  cases where `mcc` differs from a gcc==clang consensus. Four such divergences
  remain on macOS, none of which is an `mcc` defect (`predefined_macros`
  __STDC_VERSION__ default; `bitfields_ms` implementation-defined layout;
  `cleanup` teardown ordering; `c11_freestanding_headers` where `mcc` is the
  most conformant of the three). The suite returns non-zero on any divergence
  by design; these four are the expected residual on a macOS host.

---

### §6.5–6.6 Expressions & constant expressions

- [ ] **[DIAG] §6.5.16.1 — assignment to a const-qualified struct/union is diagnosed (as a warning).**
  Audit re-check correction: `a = b` where `a` is `const struct S` *does* warn
  "assignment of read-only location" — `vstore` calls `verify_assign_cast`
  (`src/mccgen.c:3555`) whose const check (`:3458`) fires for aggregates too,
  exactly as for the scalar case. mcc's long-standing stance is to *warn* on
  assignment-to-const (scalar and aggregate alike); gcc/clang *error*. Only the
  severity differs. Promoting to a default-on error would be more conformant but
  is a broad change to a long-standing lenient warning — deferred.
  3-way: mcc=warns | gcc=error | clang=error.

- [ ] **[DIAG] §6.6p3 — comma operator in an integer constant expression diagnosed only under -pedantic.**
  A required constant expression "shall not contain ... comma operators" outside
  an unevaluated subexpression. `int a[(1,2)];` is `mcc_pedantic`
  (`src/mccgen.c` in `gexpr`, ~`:8024`): silent at default level, but `-pedantic` warns and
  `-pedantic-errors` hard-errors. clang is also lenient at default; gcc rejects
  the file-scope case by default via its VLA-at-file-scope path. Effectively
  handled — consider whether to promote to a default-on warning. Low priority.

---

### §6.7 Declarations

- [ ] **[DIAG] §6.7.4p2 — `_Noreturn` on a non-function object diagnosed only under -pedantic.**
  Function specifiers shall declare only functions. `_Noreturn int x;` is
  diagnosed via `mcc_pedantic` (`src/mccgen.c` declarator storage-class check, ~`:10274`, gated on bit 128 = the
  `_Noreturn` keyword vs `__attribute__((noreturn))`): silent at default,
  `-pedantic` warns, `-pedantic-errors` errors. gcc warns by default; clang
  errors by default. Effectively handled under `-pedantic`; consider promoting
  to a default-on warning to match gcc. (The parallel `inline int x;` is a
  default-on hard error at `src/mccgen.c` ~`:10270` — intentionally asymmetric since
  gcc treats `_Noreturn` on an object as an extension.)

---

### §6.10 / §5 Preprocessor & environment

- [ ] **[DIFF] §6.10.3.3p3 — invalid token paste is warn-and-continue (deliberate leniency).**
  `C(+,-)` / `C(*,/)` emit a warning and continue (rc=0, output `+ -` / `* /`).
  The result is UB (no diagnostic strictly mandated); mcc deliberately recovers
  rather than aborting, and this is locked in by the `pp_invalid_paste` exec
  golden. gcc/clang hard-error; this is an intentional, tested difference, kept.
  (The genuinely-broken comment-introducer case `//`/`/*` IS a hard error now.)
  3-way: mcc=warn(rc=0) | gcc=error | clang=error.

---

### Additional deliberate differences & gaps

#### §6.4 / §6.10 lexical & preprocessor

- [ ] **[BUG] §5.1.1.2 / §6.10 — `\`+whitespace+newline not spliced (now terminates, still not gcc-compat).**
  The earlier hang is fixed; the residual is behavioral: `-c` hard-errors "stray
  '\'" and `-E` emits a stray `\` token, whereas gcc/clang treat `\`+ws+NL as a
  line-continuation splice with a `-Wbackslash-newline-escape` warning. Making
  `handle_stray` consume `\`+whitespace+newline as a warned splice would match
  the references. Low priority (extension; mcc's strict-ISO rejection is
  defensible).
  3-way: mcc=stray-`\`/no-splice | gcc/clang=splice+warn.

#### §6.7 / §6.2 / §6.9 declarations

- [ ] **[DIFF] §6.7.2.1 — no-declarator tagged struct *member*: mcc matches gcc `-fms-extensions` (silent).**
  `struct S { struct T { int x; }; };` — re-verified 3-way: gcc *default* (no
  `-fms-extensions`) warns "declaration does not declare anything"; but gcc
  **`-fms-extensions` is silent even under `-pedantic`**, and clang
  `-fms-extensions -pedantic` instead warns "anonymous structs are a Microsoft
  extension". mcc enables MS extensions by default (so the nested tag becomes an
  anonymous member), which matches gcc `-fms-extensions` exactly — silent. The two
  references disagree once MS extensions are on, and mcc tracks gcc; adopting
  clang's MS-extension warning would diverge from gcc. Left as a defensible DIFF.
  (`src/mccgen.c` already warns when `ms_extensions == 0`.)

- [ ] **[DIFF] §6.8.6.4p1 — `return <expr>;` in `void` / `return;` in non-`void`: CONFORMANT (diagnosed as a default-on warning).**
  §6.8.6.4 is a *constraint*, which requires only that a conforming implementation
  issue **a diagnostic** — it does not mandate an error. mcc emits a default-on
  warning ("void function returns a value" / "'return' with no value") and
  upgrades it to an error under `-Werror`, so the standard's requirement is met.
  This is mcc's deliberate, **consistent** lenient-warning stance for constraint
  violations — verified identical to the const-assignment §6.5.16.1 case (both
  warn by default, both become errors under `-Werror`). gcc/clang choosing to make
  it a hard error by default is their policy, not a standard requirement. Left as a
  conformant DIFF (forcing an error would diverge from mcc's coherent
  permissive-by-default / `-Werror`-enforces philosophy for no conformance gain).

#### §7 library / floating-point builtins

- [ ] **[FEATURE] §F/§7.12 — GCC/Clang floating-point builtins (constants + classification done).**
  Added to `runtime/include/mccdefs.h` as constant-foldable macros (`#ifndef`-
  guarded so the BSD/Apple defines win where present): `__builtin_inf`/`inff`/
  `infl`, `huge_val`/`valf`/`vall`, `nan`/`nanf`/`nanl`/`nans*` (inf = folded
  overflow product, nan = `0.0/0.0`), and the `isnan`/`isinf`/`isfinite`/
  `isunordered`/`isgreater`/`isgreaterequal`/`isless`/`islessequal`/`islessgreater`
  classification + comparison builtins. Direct use compiles, links, and folds in
  a constant expression (`static int a=__builtin_isnan(0.0/0.0)`), matching gcc on
  all 5 arches. exec `fp_builtins`. Also added `fabs`/`fabsf`/`fabsl`,
  `signbit`/`signbitf`/`signbitl` (incl. `-0.0` detection via `1/x`), and
  `copysign`/`copysignf`/`copysignl` as constant-foldable macros — verified 3-way.
  `fpclassify`/`isnormal`: **work correctly via glibc + `-lm`** (verified:
  `fpclassify` returns `FP_ZERO`/`FP_NORMAL`/`FP_INFINITE`/`FP_NAN`/`FP_SUBNORMAL`
  and `isnormal` is correct for normal/zero/subnormal/inf). They route to glibc's
  `__fpclassify` function (not the gcc builtin) because mcc's predefined
  `__GNUC__ 4` (4.2.1) is below glibc's `__GNUC_PREREQ(4,4)` threshold for the
  type-aware `__builtin_fpclassify`, so they need `-lm` where gcc's builtin does
  not — a usability DIFF, not a correctness gap. They cannot be replaced by clean
  constant-foldable macros: the subnormal test needs the *per-type* smallest-normal
  threshold, which a type-agnostic macro can't know (gcc uses a type-aware
  builtin). The macro builtins above also multi-evaluate their argument (intrinsics
  don't). The `NAN`-sign and `signbit`-return behavior matches gcc/clang: mcc
  predefines `__GNUC__ 4`, so glibc's `<math.h>` takes its GCC path — `NAN` prints
  `nan` and `signbit` returns a normalized `0`/`1` (§7.12p5, §7.12.3.6p3).

### Landed: macOS bench gcc column + Darwin skip audit (2026-07-08)

Closed the three macOS docs/TODO.md items on a native arm64 macOS host (SDK +
full Homebrew cross toolchain present).

- **Bench `full-language` gcc column no longer reads `n/a`.** Two root causes,
  both macOS-only. (1) `tools/bench.c` detected `/usr/bin/gcc` (the Apple clang
  shim) as its "gcc" column and passed it `-DCC_NAME=CC_gcc`; `full_language.c`
  guards a K&R path with `#if CC_NAME == CC_clang`, so Apple-clang-as-gcc took the
  non-clang branch and modern clang errors on the `int`-conversion → n/a. Now the
  "gcc" column routes through `ts_resolve_reference_cc()` (genuine GNU gcc only —
  skips clang/Apple-LLVM, requires gcc/GCC/Free-Software; candidate list widened to
  `gcc-16…gcc-10`), so `-DCC_NAME=CC_gcc` reaches the real Homebrew gcc, mirroring
  CMake's `mcc_find_gnu_gcc()`. (2) genuine gcc-16 then ICE'd (`assemble_alias`) on
  `__attribute__((weak, alias(…)))` in `tests/diff/parts/legacy_builtins.h` — alias
  is unsupported on Mach-O (why the block already excluded `__clang__`); now also
  excludes `__APPLE__` (defined by gcc/clang/mcc on Darwin, not mingw/Linux) on both
  the alias def and the paired `some_lib_func` print, keeping every Darwin compiler
  on the same `some_lib_func=444` path (output-neutral for the mcctest differential).
- **Darwin skip audit (x86_64 + arm64).** Re-evaluated every rc-77 skip on the
  `macos`/`macos-cross` presets; all are legitimate. The 2-slice universal /
  `macho-structural` / `dash-s-bytes-*` differentials run under `macos-cross`; the
  loader-based `macho-codegen-run`/`macho-image-run`/`macho-apple-libc` self-skip
  (Linux-only oracle) and are covered natively by `macho-conformance-native`.
- **Fixed a real red the audit surfaced:** the just-landed Tier-3
  `ast/replay-promote` fixture was registered unconditionally, but promotion v1 is
  x86_64-only (`#if …MCC_TARGET_X86_64`, pins R11/R10/R9/R8), so it failed on arm64.
  Now gated to `MCC_CPU==x86_64` (self-skip elsewhere); the output-identical
  `exec-replay-promote` corpus keeps coverage on other targets.
- **External `__thread` Mach-O limitation revisited** on the real runner: the
  cross-module `extern __thread` hard-error (`src/objfmt/mccmacho.c`) still fires
  correctly at link; lifting it needs TLV import descriptors + GOT-indirect loads
  (non-trivial, currently-green subsystem, no driving need) — decision stands, now
  pinned by `cli/macho_extern_tls_unsupported` (`os=Darwin`).

### Landed: C99/C11 test-coverage additions & fixes (2026-07)

Closed from the [docs/TESTS.md](TESTS.md) gap analysis. Each mirrors a named
gcc/clang conformance test; runtime cases self-check by printing `OK`,
diagnostics grep the message text via `tests/cli`. All verified against the
gcc/clang references and green across ctest + the qemu matrix.

- **[BUG] `__WCHAR_TYPE__` signedness on ARM/AArch64 Linux** — `mccdefs.h` emitted
  `int` for all `__linux__`, but the ARM EABI / AArch64 ABI (and glibc/musl)
  mandate `unsigned int`, so mcc's `<stddef.h>` clashed with the musl-arm sysroot
  (`incompatible redefinition of 'wchar_t'` when a libc header pulled in wchar_t
  before `<stddef.h>`). Fixed with a Linux-ARM `#elif` → `unsigned int`; macOS ARM
  stays `int` (macOS wchar_t is signed), x86/i386/riscv64 unchanged. Reproduced and
  verified under real qemu-arm-musl (sizeof==4, unsigned, `L"…"` literals,
  `wcslen`). Regression-guarded by `flexarray_runtime.c`, which first exposed it.
- **Flexible array members** — runtime `exec/features_c99_c11/flexarray_runtime.c`
  (sizeof==offsetof, heap alloc/access, char-FAM string, typedef FAM) + diagnostic
  `cli/c99_fam_not_last` (FAM not at end of struct). Ref gcc `c99-flex-array-*`.
- **`_Noreturn`** — `exec/features_c99_c11/noreturn.c` (`_Noreturn` keyword +
  `<stdnoreturn.h>` `noreturn` macro, pre/post placement, genuine non-return via
  longjmp) + diagnostic `cli/c11_noreturn_returns` (a returning noreturn function
  warns). Ref gcc `c11-noreturn-2`.
- **`_Alignas`/`_Alignof`** — runtime `exec/features_c99_c11/alignas_over.c`
  (over-alignment addresses, `alignof(max_align_t)`, member/offset alignment) +
  diagnostic `cli/c11_alignas_underalign` (alignment below the type minimum). Ref
  gcc `c11-align-*`.
- **VLA jump-into-scope** — diagnostics `cli/c99_vla_goto_into_scope` and
  `cli/c99_vla_switch_into_scope` (goto/switch into a variably-modified
  declaration's scope is rejected); the legal jump-*out* case is exercised at
  runtime by `exec/vla/label.c`. Ref gcc `c99-vla-jump-*`.
- **FP evaluation-method** — `exec/features_c99_c11/flt_eval_method.c`:
  `FLT_EVAL_METHOD ∈ {-1..2}`, `float_t`/`double_t` widths track the reported
  method (§7.12p2), `<float.h>` characteristics sane. Ref gcc `c11-float-*`, clang
  `C11/n1365`. (Annex-F wide-return intermediate precision stays open — see TODO.)
- **Feature-subset / `__STDC_NO_*` macros** — the header *content* macros
  (`ATOMIC_*_LOCK_FREE`, `__STDC_IEC_559{,_COMPLEX}__`, `__STDC_ISO_10646__` guarded
  `!_WIN32`, `__CHAR16/32_TYPE__`, limits, stdint, iso646) are asserted by
  `exec/features_c99_c11/c11_freestanding_headers.c`; the compiler-level
  §6.10.8 feature macros by `exec/features_c99_c11/feature_macros.c`. The latter
  pins the mandatory §6.10.8.1 macros (`__STDC__`==1, `__STDC_HOSTED__`==1,
  `__STDC_VERSION__`>=201112L, `__STDC_UTF_16/32__`==1) and the §6.10.8.3
  conditional-feature opt-outs (`__STDC_NO_ATOMICS/COMPLEX/THREADS/VLA__` all
  absent, each corroborated by exercising the feature). It is written to agree
  across mcc/gcc/clang and every hosted target incl. PE — version macros use `>=`
  and it avoids the `!_WIN32`-only content macros — so it also runs under diff3.
- **FP Annex F wide-return intermediate precision** — `exec/features_c99_c11/
  fp_wide_return.c` (3-way validated; runs the exec / exec-replay / exec-replay-tmpl /
  diff3 columns): a float/double return must remove extra range/precision —
  `FLT_MAX+FLT_MAX` / `DBL_MAX+DBL_MAX` narrow to +inf and a float-rounded quotient stays
  narrowed. Trivially holds on the `FLT_EVAL_METHOD==0` targets (x86_64 SSE, arm64,
  riscv64) and exercises real x87 narrowing on i386 (`FLT_EVAL_METHOD==2`) via the qemu
  matrix. Ref gcc `gcc.dg/c11-float-*.c`, clang `C11/n1365.c`, `C11/n1396.c`.
- **`_Complex` Annex G special values / CMPLX** — `exec/features_c99_c11/
  complex_cmplx_special.c` (3-way validated; exec/exec-replay/exec-replay-tmpl/diff3):
  CMPLX/CMPLXF/CMPLXL exactness with NaN/inf parts (n1464), `cabs` inf-part→+inf, `conj`
  exact sign flip, `cproj` inf→(+inf, copysign(0,imag)) — all matching gcc/clang.
  `_Complex` constraint *diagnostics* fold into the open negative-test-tier item. Ref
  clang `C11/n1464.c`, `C11/n1514.c`; gcc `gcc.dg/c99-complex-{1,3}.c`.

## CST database — design record (frozen spec §0–§11)

The authoritative CST design, folded in from the former `docs/PLAN.md` (frozen
spec) + `docs/IMPLEMENTATION.md` (build order) + `docs/CST.md` (post-completion
decisions) when those files were consolidated (2026-07-06). Section numbers are
preserved so the `NOTES CST §N` citations throughout `src/mcccst.{c,h}`,
`src/mcc.h`, `src/mccpp.c`, `src/mccgen.c` still resolve here. The subsystem is
**complete and gated** (see the slice-by-slice completion record + the D1–D5
gap-closure record below); this section is the *why*, those are the *what
landed*. Everything here is verified against the shipped source unless flagged.

### §0 — North-star invariants (hold at every commit; each is a *tested* gate)
1. **Byte-identical codegen** CST-in or CST-out — the CST is a pure side-effect,
   never feeds a codegen decision (gate §8.5). 2. **Zero-cost when off** — one
   CMake node → PP define; off ⇒ every hook compiles to nothing and `mcccst.c` is
   an empty TU. 3. **Total self-containment** — the CST shares no memory with the
   compiler: own arena, own copy of each file's bytes, own interned strings, all
   cross-refs as CST-internal node-ids (never `Sym*`, never `BufferedFile.buffer`
   pointers). The compiler is an oracle consulted during construction, never a
   backing store. 4. **Pure reflection** — round-trips to byte-identical source;
   every datum derivable from source text alone. 5. **No new external deps.**

### §1 — Frozen design decisions
- **Representation:** flat data-oriented SoA — `kind[] parent[] first_child[]
  next_sib[] width[] struct_hash[] trivia_hash[] sym_ref[]` + reserved
  `slot_key[]` + a `Token` leaf side-table. Index-addressed, mmap-serializable
  1:1. Children **linked** (`first_child`/`next_sib`), not contiguous ranges, so a
  future 5B splice is a pointer patch, no array shift.
- **Node identity:** tagged 64-bit `(u32 file : u32 local)`. SoA indexed by bare
  `u32` local; any cross-file ref (sym use→def, `IncludeDirective` target, dedup
  entry) stores the full 64-bit id. Reserves multi-file + dedup room so id width
  never migrates.
- **Construction:** side-recording from the existing `mccgen.c` recursive descent;
  oracle-only. **Positioning:** relative widths (green-node style); absolute char
  offset never stored/hashed; a **mandatory** `offset→node` index is a rebuildable
  accelerator over the canonical widths (LSP hits it per keystroke).
- **Hashing:** non-crypto 128-bit Merkle, two channels — position-independent
  **structural** `H_s` (identity/reuse/dedup/TDD) + separate **trivia/layout**
  `H_t` (comment/whitespace edits).
- **Persistence:** v1 = mmap-able arena snapshot behind a versioned header
  (magic + format-version + endianness + section table) — never a raw dump.
- **Symbols:** consult the live `Sym` at build to learn a use's def-site, store
  only `use-node → def-node` ids (8A). **Includes:** per-file subtrees stitched by
  id — each file its own `TranslationUnit`/`SourceFile` over its own owned buffer;
  `IncludeDirective` holds a cross-file node-id (the LSP document model; one edited
  header reparses without rebuilding every includer).
- **Trivia:** `(kind, rel-span)[]` on leaves, excluded from `H_s`. **Resilience:**
  `Error`/`Missing` node kinds reserved now (recovery is an LSP-era addition) so
  adding recovery never reshapes the node format.
- **Consumer order (each a later milestone):** TDD "pure reflection" harness
  (done) → `-g` debugging → LSP → optimization layer.

### §2 — Node schema (all reserved kinds now *produced* after D1)
Structural: `TranslationUnit Declaration FunctionDef Declarator ParamList
StructOrUnion Enum TypeName Initializer CompoundStmt If/While/For/Do/Switch/
Return/Goto/Label/ExprStmt Binary Unary Call Member Index Cast Cond Comma Paren
Primary`. Preprocessor (concrete): `MacroInvocation IncludeDirective PPDirective
PPConditional`. Leaf: `Token` (token-kind + owned byte span + trivia pieces).
Still reserved-only: `Error`, `Missing` (LSP resilience). `Comment` was reserved
in the frozen plan but is now produced (D1d). Width is **relative** = children +
own leaf bytes + attached trivia; absolute offset = prefix-sum of preceding
siblings' widths + parent offset, served by the mandatory `offset→node` index.

### §3 — Hashing specification
`H_s(leaf) = mix(salt(token_kind), bytes(leaf_span))` (trivia excluded);
`H_s(internal): h = salt(kind, child_count); for c: h = mix(h, H_s(c))` — order-
and kind-sensitive, child-count salt disambiguates `a+b` vs `a+(b)`. `H_t` folds
comment/whitespace bytes + widths so format-only edits localize without dirtying
`H_s`. Incremental update rehashes only root-path nodes whose child-hash multiset
changed — O(depth); this *is* the hierarchical incremental hash.

**§3.1 — Invertible epoch hash `H_e` + tombstone sweep (5B-era, DESIGNED / NOT
BUILT).** The deferred incremental-rehash layer for live edits. Canonical `H_s`
is non-invertible, so a changed child forces an O(fanout) sibling re-scan; in C
the trees are wide-and-shallow so fanout dominates. Design: a dual-hash where the
combine is a slot-keyed group op `H_e(parent) = Σ_i M(key_i)·H_e(child_i) (mod
2^128)`, `M(key)` odd — subtractable in O(1) per level (`H_e += M(key_i)·(new −
old)`; a removed child becomes a Null **tombstone** with `H_e = 0`, dropping its
term exactly). Edits do O(1) algebraic patches into a dirty-set with no ancestor
walk; an explicit later **sweep** compacts tombstones, renormalizes slot keys, and
reconciles `H_s` over the touched frontier only. Changes the cost model (per-level
O(fanout)→O(1)), not the O(depth) bound. Four caveats to design around: slot-bind
via `M(key_i)` never a bare sum (order-collisions); use **stable/fractional keys**
so inserts don't renumber siblings; linear combiner is weaker collision-resistance
so `H_e` never replaces `H_s` as the content address; Null must be true group
identity **and** zero-width until swept or offset arithmetic drifts. Reserved for
now: the per-node slot-key field + frontier-scoped `H_s`-recompute (both shipped).
**Drift note:** the frozen plan said `slot_key` stays *unused/0 in v1*; D3 now
**repurposes `slot_key`** to hold the 1-based branch-body tag
(`cst_mark_branch`, `src/mcccst.c:512`). It is still 0 for the epoch-hash purpose,
but no longer universally zero — a future `H_e` build must account for that.

### §4 — Memory & preprocessor model
CST owns the source (per-file owned buffer copied on first read; leaf spans point
into that copy, survives `BufferedFile` recycle, serializes fixup-free). Owned
`CstArena` bump allocator, `free`/`snapshot`/`load` as a unit. Symbol refs are
node-ids (no `Sym*` in CST memory). **Preprocessor/macros** (highest-risk, §11):
parser sees post-expansion tokens (`next()`), but pure-reflection needs *written*
source — so leaf spans are captured at the lexer/source boundary and a macro use
becomes a `MacroInvocation` node whose span covers the use-text (round-trip/LSP)
and whose children hold the expansion (for future `-g`/opt). Phased: M1 handled
the expansion-transparent subset, full fidelity landed as dedicated milestone Mμ.

### §5 — Byte-offset facility
As shipped: a per-file base `cst_base` (`src/mcc.h:464`), maintained in
`handle_eob`, combined with buffer-pointer arithmetic so `abs_off(p) == cst_base
+ (p - buffer)`. `cst_base` advances by the discarded-window length at each chunk
refill; offsets captured as absolute values at token start/end so they survive
mid-token refills. (The frozen plan's earlier "monotonic per-byte counter" phrasing
was never built — this base-offset model is the shipped one.) Zero-cost off.

### §6 — Integration points (hooks)
Recording driven from the existing descent; hook macros expand to nothing when
off. Primary `mccgen.c` sites: `decl()` (Declaration/FunctionDef), `struct_decl()`
/`post_type()`/`type_decl()` (type structure), `block()`/`gexpr_decl()`/`lblock()`
(statements), the `unary`/`expr`/`gexpr` expression family, `next`/`skip` (leaf
capture). Pattern: a small explicit node stack; each grammar fn brackets with
`cst_open(kind)`/`cst_close()`. Debug builds assert stack balance (each open
closed once; empty at `decl()` end) — catches hook-coverage drift directly.

### §7 — CMake gating
`mcc_config_node(MCC_CST TYPE BOOL DEFAULT ON GROUP "Advanced" ADVANCED)`
(`CMakeLists.txt:1087`) — default **ON**; codegen byte-identical on/off, guarded
by §8.5. ON ⇒ `PRIVATE CONFIG_MCC_CST=1` + `src/mcccst.c` in sources; a `cst`
preset in `CMakePresets.json`; `#if CONFIG_MCC_CST` guards every hook + new file.

### §8 — TDD "pure reflection" gates (`tools/csttool` + `tests/cst/`)
1. **Round-trip:** CST→source byte-identical over the corpus (strongest
   proof). 2. **Span coverage:** child spans tile the parent, sum-of-widths ==
   parent, every source byte covered once. 3. **Bidirectional lookup:**
   offset→node→span round-trips; def↔use ids round-trip. 4. **Hash invariance:**
   `H_s` unchanged by whitespace/comment edits, changes iff token structure does;
   identical subtrees share a hash. 5. **Codegen-identity:** mcc with/without
   `CONFIG_MCC_CST` emits identical output over the corpus (protects §0.1).
   6. **Snapshot round-trip:** dump→reload identical tree+hashes; version/endian
   skew rejected cleanly.

### §9 — Milestones (all landed) & consumers (future)
M0 scaffolding · M1 leaf+owned-source+round-trip · M2 structure+widths+lookup ·
M3 hashing+snapshot ("CST complete for source-of-truth") · Mμ macro fidelity ·
M4 symbol refs — **all done** (slice records below). **M5+ consumers are separate
future plans** (see TODO Later): `-g` (span↔PC via `mccdbg.c`) → LSP (8C lazy
resolution + `Error`/`Missing` resilience + 5B incremental splice + 6B store) →
optimization layer.

### §10 — Deferred / reserved (roadmap; status as of 2026-07-06)
- **5B incremental splice** — DEFERRED. Needs 4B rolling-hash re-lex-window
  finder, parser error-recovery (LSP-era), `Error`/`Missing` nodes. Batch rebuild
  is the same code minus the splice.
- **`H_e` epoch hash + tombstone sweep** (§3.1) — DEFERRED (designed; slot-key
  field + frontier-scoped recompute reserved/shipped).
- **4C hash-consing** (physical subtree dedup keyed by `H_s`) — **now BUILT**,
  pulled forward by D3 as the content-addressed `SourceFile` template store
  (`cst_store_intern`).
- **6B content-addressed store** — PARTIAL: the in-memory `H_s`-keyed store ships
  (D3); the on-disk archival format layered over the 6A working image (backbone
  for hot-reload / reconciled-CST snapshots — see TODO Later) is still deferred.
- **9B trivia-as-nodes** (`Comment`) — **now DONE** (D1d).
- **6C embedded KV** — REJECTED (dependency conflict); LSP queries synthesized
  from hand-rolled `offset→node`/`name→def`/`def→uses` indices over the flat
  arrays.

### §11 — Risk register (each pinned to a tripwire test)
Macro-fidelity vs pure reflection (grown under the round-trip corpus, not up
front) · hook-coverage drift (span-coverage/tiling §8.2 + debug balance assert) ·
zero-cost-off regressions (codegen-identity §8.5 in CI) · width/offset arithmetic
bugs (tiling invariant §8.2).

### The seam (as-shipped interface contract, from the former IMPLEMENTATION.md §3)
Pure slices (B store, C hashing, D geometry, E serialization) publish plain
functions in `src/mcccst.h`; compiler-side slices drive them mostly through the
`CST_*` hook macros (`CST_OPEN`/`CST_OPEN_AT`/`CST_MARK`/`CST_CLOSE`/`CST_LEAF`,
all `((void)0)` when off). The deferred-capture model (slice H) also calls a few
hook functions directly under `#if CONFIG_MCC_CST` — `cst_hook_token` (leaf
capture; the `CST_LEAF`/`cst_hook_leaf` pair ended up **vestigial**, 0 uses),
`cst_hook_def`/`cst_hook_use` (slice I), `cst_hook_wrap` (slice J), `cst_cur_tok_off`.
All structure/hashing/geometry/IO stays inside `src/mcccst.c` behind those
functions, so §0.2 stays mechanical. The per-slice `mcccst_{hash,geom,io}.c` split
sketched in the original build order was **not taken** — all slices live in the
single `src/mcccst.c`; the harness is the single file `tools/csttool.c`.

---

## Completed work — CST database (all vertical slices landed)

The CST (Concrete Syntax Tree) database subsystem is complete — every
vertical slice (S0, B–J, the weaves, FINAL) landed and is gated in CTest.
This is the folded completion record moved out of `docs/TODO.md`; the design
lives in [§ CST database — design record](#cst-database--design-record-frozen-spec-011) above, the
implementation in `src/mcccst.{c,h}` (+ `tools/csttool`, `tests/cst/`).
`MCC_CST` is now built **on by default** (CMakeLists.txt:1087), codegen
byte-identical either way. The slice-H/G follow-ups noted here originally
(Declaration/FunctionDef grouping + ParamList; per-file include stitching) were
subsequently closed by the D1b/D3 gap-closure work (see the next section);
what remains open is the future-consumer / 5B roadmap in `docs/TODO.md`.

Two headline deliverables, implemented via the vertical slices below:
- [x] CST Database for Debugging, LSP, and Optimization data/layers — the
  side-recorded, self-contained CST substrate is built, populated from real
  compilation, round-trips byte-identically, hashes, serializes, and carries
  symbol refs. The *consumer* layers (-g/LSP/opt) are separate future plans
  (NOTES CST §9 M5+); this delivers the data layer they build on.
- [x] CST Database uses hierarchical incremental hashes to enable bidirectional
  lookups starting from any character index in any file — 128-bit hierarchical
  Merkle hashing (struct + trivia channels, frontier-scoped incremental rehash,
  epoch-hash seam reserved) + mandatory offset→node index; bidirectional
  lookup verified (§8.3) on 308 corpus files.

Legend for slices: each has a status line. `[ ]` open · `[~]` in progress ·
`[x]` done. Slice IDs + dependencies are given per-slice below (the `Deps:` lines).

### S0 — Gating & harness skeleton  ·  status: [x]
Deps: — · Kind: build · PLAN §7, §8
- [x] `MCC_CST` config node in CMakeLists.txt (mirror diagnostics node) → `CONFIG_MCC_CST`
- [x] `cst` preset in CMakePresets.json (configure/build/test, mirrors diagnostics)
- [x] `src/mcccst.{c,h}` present, self-guarded; `#include "mcccst.c"` in libmcc.c
- [x] `CONFIG_MCC_CST` source guards + no-op hook macros in mcccst.h
- [x] `tools/csttool` self-contained harness (#includes mcccst.c, links no compiler)
- [x] `tests/cst/{store,hash,geom,serial}` registered; 4/4 green via ctest
- [x] Codegen-identity gate (§8.5): mcc CST-on vs CST-off byte-identical over 42 files
- Notes: define appended to `_mccdefs` (CMakeLists.txt ~1642); mcccst.c un-poisons
  malloc/realloc/free (mcc.h:1215) via push/pop_macro for self-containment. mcc
  binary grows (~26KB, dead code until Weave 1) but its *output* is identical.

### B — Node store core (pure)  ·  status: [x]
Deps: S0 · PLAN §1, §2
- [x] `CstArena` growable SoA store + free/reset
- [x] SoA columns incl. reserved `slot_key`; linked `first_child`/`next_sib`
- [x] Tagged id scheme: `u32` local index + 64-bit `(file,local)` cross-file
- [x] `cst_node_open/close`, `cst_leaf`, append_child, column accessors
- [x] Synthetic-tree builder in harness + topology/id round-trip tests (cst/store)
- Notes: width accumulates bottom-up (leaf sets own len; close bubbles into parent).

### C — Hashing library (pure)  ·  status: [x]
Deps: S0 · PLAN §3, §3.1
- [x] 128-bit non-crypto hash (two lanes, splitmix-style finalizer)
- [x] `cst_hash_leaf` (kind salt + token bytes, trivia carved out)
- [x] `cst_hash_internal` (salt(kind,count) + Merkle fold)
- [x] `cst_hash_eq`, frontier-scoped `cst_rehash_frontier`; epoch-hash seam reserved
- [x] Invariance property tests (cst/hash): ws-invariance, token-sensitivity,
      identical-subtree equality, child-count salt, frontier==full
- Notes: leaf token bytes = owned span minus leading-trivia prefix (`tok_rel`).

### D — Geometry & offset→node index (pure)  ·  status: [x]
Deps: B · PLAN §1, §2, §5
- [x] Relative-width finalize on close; `cst_abs_offset` prefix-sum
- [x] Mandatory `offset→node` index build + `cst_node_at` (binary search)
- [x] Tiling invariant test (§8.2) + per-offset round-trip (§8.3) (cst/geom)
- Notes:

### E — Serialization (pure)  ·  status: [x]
Deps: B (+G stub) · PLAN §1, §8.1, §8.6
- [x] Versioned snapshot header (magic+version+endian) save/load, all columns
- [x] `cst_reflect` CST→source emitter (emits owned leaf spans in DFS order)
- [x] Save/load equality + version-skew rejection + reflect round-trip (cst/serial)
- Notes:

### F — Byte-offset facility (compiler)  ·  status: [x]
Deps: — · PLAN §5
- [x] Monotonic byte cursor `BufferedFile.cst_base` (guarded), maintained in
      handle_eob so abs_off(p) == cst_base + (p - buffer)
- [x] Correct across handle_eob refills (validated: 12KB file round-trips)
- [x] Validated via round-trip over 305 corpus files (offset model exercised)
- Notes: cursor advances by discarded-window length at each refill; captured as
  absolute values at token start/end so it survives mid-token refills.

### G — Owned source & trivia (compiler)  ·  status: [x]
Deps: B, F · PLAN §4, §1
- [x] Per-file byte copy into arena (cst_slurp of the main file) = LSP doc model
- [x] Trivia classified on real leaves (cst_leading_trivia: whitespace + line/
      block comments) → tok_rel set → excluded from H_s. Verified: whitespace/
      comment-only edits leave H_s unchanged, token edits change it (cst/hashinv)
- [x] Round-trip proves owned buffer + spans correct over corpus (309/0)
- [ ] Per-file subtree ownership for include stitching (main file only; multi-
      file include stitching is an LSP-era refinement, PLAN §1 Includes)
- Notes: trivia lumped as one leading WS piece per leaf; finer per-piece
  classification (separate comment pieces, 9B Comment nodes) is a later refinement.

### H — Recording hooks (WEAVE 1 + M2)  ·  status: [x]
Deps: B, D, F, G · PLAN §6
- [x] Leaf capture at next_nomacro exit (mccpp.c:2996), [prev_end,end) tiling;
      cst_capture_begin/end bracket mccgen_compile in mcc_compile
- [x] Deferred-capture model resolves single-pass lookahead skew: flat leaves
      (round-trip) + structural specs as leaf-index ranges materialized in
      cst_hook_end (node spans [open_count-1, close_count-1))
- [x] Structural brackets on the single-exit grammar functions: block()
      (statement kinds If/While/For/Do/Switch/Return/Goto/CompoundStmt/ExprStmt),
      type_decl() (Declarator), struct_decl() (StructOrUnion). basic.c: 33 flat →
      357 nodes; round-trip stays exact.
- [x] Corpus gate (cst_validate): round-trip §8.1 + tiling §8.2 + offset→node
      §8.3 all pass on 312/312 compilable files, 0 failures. Balance verified: 0
      hook imbalances across the corpus (temporary instrumentation, now assert).
- [x] Debug-build balance assert in cst_hook_end (CST_ASSERT cst_sstop==1);
      fires in assert-enabled builds (sanitize/diagnostics).
- [x] Expression nodes via retroactive range-based wrapping (cst_hook_open_at +
      cst_nest_specs rebuilds nesting from leaf-range containment, solving the
      precedence-climbing left-recursion cleanly): CST_Binary (only when an infix
      operator is present — no degenerate chains), CST_Cond (?:), assignment as
      Binary, CST_Comma, and postfix CST_Member/CST_Index/CST_Call. Verified on
      real code: `p[0].x` → Member(Index(p,0),x), `f(1,2)` → Call, in one flat
      Binary. Crash-hardened: empty-range specs (macro-expanded exprs) dropped.
- [~] Minor remaining (non-blocking refinements): top-level Declaration/
      FunctionDef grouping and ParamList — decl()/post_type() have many
      exit/continue points inside loops, making per-item retroactive wrapping
      awkward for marginal gain (Declarator + Compound already capture the
      structure). lblock()/gexpr_decl() are low-value wrappers.
- Coverage now: TU, Compound + all statement kinds, Declarator, StructOrUnion,
  Binary, Cond, Comma, Member, Index, Call, MacroInvocation, Token — a rich,
  correct, round-trip-preserving concrete syntax tree.

### WEAVE 2 — Hash & snapshot online  ·  status: [x]
- [x] Hashing runs on every real tree (cst_rehash_all in cst_hook_end)
- [x] Snapshot save/load of real compiled trees: reload validates + identical
      struct hash (MCC_CST_SNAPSHOT), gated in ctest via roundtrip.cmake (§8.6)
- Notes: moved up from the milestone list — landed together with M2.
- Notes (verified 2026-07-05 against source):
  - PLAN's `expr`/`cond_expr`/`binary` DO NOT EXIST. Real expr cascade:
    `gexpr`(7962)→`expr_eq`(7910)→`expr_cond`(7785)→`expr_lor`(7665)→
    `expr_land`(7659)→`expr_or`(7648)→`expr_xor`(7639)→`expr_and`(7630)→
    `expr_cmpeq`(7619)→`expr_cmp`(7607)→`expr_shift`(7596)→`expr_sum`(7585)→
    `expr_prod`(7574)→`unary`(6453). `expr_const`(8007). Each cascade level is
    single-exit fall-through — trivial to bracket.
  - `decl(int l)`@10074 has MULTIPLE returns (10089/10128/10386/10394) — needs a
    goto-epilogue or wrap at the caller for `cst_close`.
  - `block(int flags)`@8515 single-exit (converges at 8942) but has `again:`@8525
    loop + gotos — place `cst_open` AFTER the `again:` label or guard re-entry.
  - Token consumption = direct `next()`(mccpp.c:4057)/`skip()`(mccpp.c:71) calls;
    hook leaves either by wrapping those or reading `tok` at boundaries.
  - Add `#include "mcccst.h"` after mcc.h:190; hook prototypes near mcc.h:1462;
    mirror `CONFIG_MCC_ASM` #ifdef style (mccgen.c:10233).

### I — Symbol refs (WEAVE 3)  ·  status: [x]
Deps: H, B · PLAN §1(Symbols), §4
- [x] Def hook at type_decl_1 (declarator name) + use hook at unary() identifier;
      token-value→def-offset side table, resolved to node-ids in cst_hook_end
- [x] Stored as tagged `(file,local)` sym_ref; survives snapshot (cst/sym)
- [x] def↔use correctness verified on real code (cst/symref): p→param,
      myglobal→global, helper→function, local→local all correct
- Notes: v1 is last-declaration-wins (no scope stack) — shadowing across scopes
  can mis-resolve; documented. `name→def`/`def→uses` reverse indices: reuse
  sym_ref column, build lazily when the LSP consumer needs them.

### J — Macro fidelity / Mμ (WEAVE 3)  ·  status: [x]
Deps: H, F · PLAN §4, §11
- [x] Expansion-transparent subset (PLAN §4 M1): macro invocations captured as
      written source leaves; round-trips byte-identical over the corpus.
- [x] `MacroInvocation` nodes: at the macro-expansion boundary in next()
      (mccpp.c:4113), the written macro-use span (name + args) is wrapped as a
      CST_MacroInvocation via cst_hook_wrap. Object-like and function-like both
      covered; nests correctly with enclosing expressions. Gated by cst/macro
      (>=3 MacroInvocation nodes + round-trip on SQUARE/LIMIT/ADD fixture).
- Notes / accepted v1 imprecisions (PLAN §11 "grow under test"): (1) function-
  like invocations may exclude the trailing ')' to avoid overrunning the next
  construct (arg-scan lookahead varies); (2) object-like macros used *inside*
  another macro's args stay plain tokens; (3) expansion *children* (for -g/opt)
  are not attached — the node reflects *written* source, which is what round-
  trip/LSP need. All are refinements, not correctness gaps.

### FINAL — corpus & hardening  ·  status: [x]
- [x] All gates (round-trip §8.1 + tiling §8.2 + offset→node §8.3) over 308/308
      compilable corpus files; snapshot §8.6 + hash §8.4 gated in ctest
- [x] §0.1/§0.2 codegen-identity gate holds; full ctest CST-ON 819/819 and
      CST-OFF 811/811 (shared-file edits inert when off)
- [x] Risk items pinned: hook-coverage→cst_validate tiling; zero-cost-off→codegen
      gate; width arithmetic→tiling invariant; macro→round-trip corpus
- Notes: "tests2" corpus not present in this tree; used tests/** (379 files).

## Completed — CST next-phase gap closure (docs/CST.md D1–D5, 2026-07-06)

The CST D1–D5 decision plan (recorded in the design record above, folded from the
former `docs/CST.md`) is fully landed on top of the
vertical slices above: node-kind fill-in, PP-concrete capture, and the
`SourceFile` template/binding/render model. Migrated here from `docs/TODO.md`.
`MCC_CST` stays **on by default** (`CMakeLists.txt:1087`); codegen is
byte-identical CST-on vs CST-off. The CST hooks are pure side-effect recording —
the compiler never reads the CST — so the §8.5 codegen-identity invariant holds
for *any* hook change; the live risk is CST round-trip/tiling correctness, gated
by the `cst/*` ctest suite (20/20 green in the `cst`-on build; verified 2026-07-06).

- [x] **D1a — Expression fill-in (`Unary`/`Cast`/`Paren`/`Primary`).** Retroactive
  range-wrap in `unary()` (mccgen.c): prefix-op → `Unary`, `(type)e` → `Cast`,
  `(e)` → `Paren`, atoms → `Primary`. Gated `cst/kinds-expr`.
- [x] **D1b — Declaration structure via D2 range-wrap** (`Declaration`,
  `FunctionDef`, `ParamList`, `Enum`, `TypeName`, `Initializer`, `Label`). Gated
  `cst/kinds-decl`.
- [x] **D1c — PP-concrete** (`IncludeDirective`, `PPDirective`, `PPConditional`),
  full-concrete: capture *all* `#if`/`#else` branches as concrete nodes. Prereq
  for D3. Gated `cst/kinds-pp`.
- [x] **D1d — `Comment` promotion** (line/inline/block), `H_t`-only so §8.4 holds.
  Gated `cst/kinds-comment`.
- [x] **D3+D5 — `SourceFile` template + renderer.** The full template/binding/
  render model (mcccst.{c,h}): full-concrete branch tagging
  (`cst_mark_branch`/`slot_key`), a content-addressed store with pure-`H_s(body)`
  hash-consing + dedup (`cst_store_*`), per-instance recursive bindings
  (`cst_binding_*`), and the `render(template, binding)` fold with a threaded PP
  environment (`cst_render`) — plus `cst_render_identity` as the round-trip
  oracle. The **headline recursive re-include branch-selection gate**
  (`cst/template`) passes all five assertions. **Live capture wired** (`cst/incstore`):
  every real `#include` during a compile interns its file as a hash-consed
  `SourceFile` template (`cst_hook_include`, mccpp `parse_include`) and binds the
  `IncludeDirective` node to it — two `#include`s of one header (incl. via a
  nested header and guard-skipped repeats) collapse to a single template id.
  - [x] **DEBUG-build hash-collision tripwire.** `cst_store_intern` verifies, in
    a debug build, that an existing same-`H_s` entry reflects byte-identically
    to the interned body; on mismatch it `abort()`s with a fatal "hash collision
    … cst_hash_* must be fixed" — never silently deduping two different bodies.
  - [x] *Full-concrete live templates.* `cst_build_sourcefile` lexes each
    captured file line-by-line into a real tree: every line a leaf, each
    `#if/#else/#endif` a `PPConditional` whose branch bodies (dead branches
    included) are tagged `CompoundStmt` groups (`cst_mark_branch`) a binding can
    select among. Verified via the `MCC_CST_STORE` dump: increment.h → 2
    `PPConditional`, leaf.h → 1 (its guard), all `render_identity` round-trip
    exactly (`cst/incstore`, `cst/increment`).
- [x] **FINAL** — every `cst/*` gate green over the corpus; §0.1/§0.2 re-confirmed:
  CST-on ctest **830/830**, CST-off **811/811**, object output **byte-identical**
  CST-on vs CST-off across the sampled corpus, and the CST-off build compiles the
  now-empty `mcccst.c` (zero-cost-off). CST D1–D5 plan complete.

## Completed — docs-accuracy reconciliation (2026-07-06)

A validation pass over `docs/*.md` against the codebase (three agents + spot
checks) found the rest accurate and these items stale; all were fixed in the docs
and re-verified against the source. Migrated here from `docs/TODO.md`.

- **`MCC_CST` default OFF→ON.** `BUILD.md` (row moved out of the §4 Diagnostics
  table into §5 with `DEFAULT ON` / `GROUP "Advanced"`, "(experimental)" dropped)
  and `PLAN.md §7` (snippet now `DEFAULT ON GROUP "Advanced" ADVANCED`) matched to
  `CMakeLists.txt:1087`.
- **`cst` preset documented.** Added the `cst` developer preset to `BUILD.md §2`
  and named `local-ci` in the "no test preset" exception list.
- **Default C standard corrected C99→C11.** The `NOTES.md` conformance section now
  says the default is C11 (`cversion=201112`, set in `mcc_new()` at
  `src/libmcc.c:911`, predefined as `__STDC_VERSION__`); `-std=c99` selects C99.
  (Was wrongly "C99, `cversion=199901`, `src/libmcc.c:836`" — `:836` is unrelated
  `#else` code and `199901` is only the `-std=c99` value.)
- **PLAN §5 byte-cursor rewritten.** Now describes the shipped `cst_base +
  (p - buffer)` model (`src/mcc.h:464`; `cst_base` bumped in `handle_eob`) instead
  of the never-built "monotonic per-byte counter"; `IMPLEMENTATION.md §2-F` matched.
- **IMPLEMENTATION.md §3 seam refreshed.** Relabelled as the original design
  sketch plus an as-shipped correction: `CST_LEAF(tk, off, n)`, added `CST_OPEN_AT`
  / `CST_MARK`, and the rule now notes the direct `cst_hook_*` calls and that
  `CST_LEAF`/`cst_hook_leaf` are vestigial (0 uses; leaves captured via
  `cst_hook_token`). The §0 "only the hook macros" line was softened to match.
- **IMPLEMENTATION.md §2 file names fixed.** The per-slice
  `src/mcccst_{hash,geom,io}.c` (never created) → the single `src/mcccst.c`;
  `tools/csttool/` directory → the single file `tools/csttool.c`.
- **NOTES.md Profiling snippet fixed.** `static TokenSym *tok_ts; // file-scope` →
  the real `#define tok_ts (mcc_state->tok_ts)` (`src/mccpp.c:12`), an `MCCState`
  member.
- **Drifted conformance refs re-anchored.** `src/mccgen.c` comma-in-ICE
  `:7448`→~`:8059`, `_Noreturn`-on-object `:9509`→~`:10319`, `inline`-on-object
  `:9504`→~`:10315` (now symbol-anchored; the `:3557`/`:3456` const-assignment refs
  were already accurate).

Still open in `docs/TODO.md` (these need a measurement/CI run, not a doc edit):
the test-count reconcile, the "~100×" headline, the speed/size table re-measure,
the PROFILING §4–§5 pre-`TOK_HASH_SIZE` baselines, and the "all green" status
prose regeneration.

## Completed work — AST intention-IR (first phase: replay driver + first template)

The AST intention-IR first phase is complete — the side-channel IR library, the
vstack-replay driver, the three-layer differential gate, and the first
optimization template all landed and are green. Migrated here from `docs/TODO.md`
"AST first phase (A1–A7)"; the design lives in [docs/AST.md](AST.md) (§16 Short +
§17 replay bring-up), the implementation in `src/mccast.{c,h}` and the
`MCC_AST_REPLAY` / `MCC_AST_TEMPLATES` paths of `src/mccgen.c` (+ `tools/asttool`,
`tests/ast/`, the `exec-replay*` columns). `CONFIG_AST` is ON by default; `-O0`
codegen is byte-identical either way (the driver only runs when `MCC_AST_REPLAY`
is set). Coverage widening past the first phase (Mid horizon — `switch`/`goto`/
floats/aggregates) is tracked open in `docs/TODO.md`.

- **A1 — `CONFIG_AST` scaffolding.** CMake `MCC_AST` option (ON), `CONFIG_AST=1`
  define, `ast` preset, `libmcc.c` includes `src/mccast.c` (guarded); mccbuild
  `--ast` + BUILD.md node/preset rows so the config-drift gates stay green.
- **A2 — `src/mccast.{c,h}` intention-IR library.** The 15 node kinds; per-function
  SoA arena (D-c: minimal, no hash-cons yet); builder API; textual `ast_dump`;
  CST-provenance id per node (§14); `ast_validate`. Self-contained (malloc un-poison).
- **A3 — `tools/asttool.c` pure-lib TDD harness + `ast/*` ctests.** 5 suites
  (arena/validate/dump/cfg/provenance), 30 checks.
- **A4 — replay driver (`ast_replay_body`) over the vstack API.** In `gen_function`,
  when `MCC_AST_REPLAY` is set: build the intention tree while the parser runs, then
  **discard the parser's body emission** (`ind = body_ind`) and re-emit from the AST
  through the vstack API. **Byte-verify safety net (§17 straight-line tripwire):** the
  re-emitted body is compared to the parser's `-O0` bytes; on *any* mismatch the
  parser's emission is restored verbatim (bytes + `ind` + `rsym`). So correctness never
  depends on having modeled every vstack op — an unmodeled construct just diverges and
  falls back. Faithful captures re-emit **byte-for-byte identical** to `-O0` (the
  zero-template invariant). Off by default; `-O0` untouched. **Coverage: ≥119 / 238
  exec golden source files have ≥1 function that faithfully replays** (int-constant/
  local/param arithmetic, calls, casts, control flow incl. `for(;;)`, array
  subscripting + scalar-array `{...}` initializers); the rest fall back.
  - rung 1: `return <integer-constant>;` → `vpushi`/`gfunc_return`/`gjmp`.
  - rung 2: **integer-arithmetic return trees** via a **scoped vstack-mirror**
    (`ast_hook_vpush`/`ast_hook_genop` shadow the vstack; `gen_op` modeled atomically via
    `ast_in_op`; unmodeled in-place transforms and non-reconstructable leaves trip
    `ast_desync` → fall back). Leaves = int-constants + frame-relative locals/params.
  - rung 3: **whole-body straight-line capture with local `Store`.** `vstore`/`vswap`/
    `vpop` modeled so local decls with initializers and assignments become `Store`
    effects. `int main(){int a=5,b=7; return a*b+7;}` replays byte-identically.
  - rung 4a: **global references + relocation discard/verify.** Symbolic leaves captured;
    the safety net discards the body's relocations with its text before replay and
    byte-verifies both. Leaf SValues finalized lazily at consumption.
  - rung 4b: **casts (`Convert`) + calls (`Invoke`).** `gen_cast` outside a modeled op →
    `Convert`; the `gfunc_call` boundary folds [callee, args] into an `Invoke`, suppresses
    the mirror across the call + result push (`ast_in_call`), re-pushes the result;
    string-literal args ride as `Convert(Ref)`. Struct/two-register returns + indirect
    callees fall back. Diagnostics suppressed during replay (`warn_none`).
  - rung 5: **control flow** (CFG milestone D-b) — captured at the `block()` handler
    level, replay re-issuing the parser's exact `gind`/`gvtst`/`gjmp`/`gsym` pattern:
    comparisons, `if`/`if-else`, non-tail returns, `while`, `++`/`--` (`Unary`),
    `for`(init;cond;incr), `for(;;)` (op==5, no gvtst, empty break chain), `do-while`,
    `break`/`continue` (Jump nodes). Loops are `If` nodes op==2 (while)/3 (for)/4
    (do-while)/5 (for(;;)). Compound-assign (`+=`) and comma replay. Memory model:
    pointer deref/address-of and array subscripting `a[i]`; `ast_bad_type` keeps
    struct/union/bitfield/float on correct fallback. Expression-level control flow:
    `?:` ternary and `&&`/`||` short-circuit. Scalar-array `InitList` — the zero-init
    `memset` a local aggregate emits is captured as a **void-effect `Invoke`**
    (`ast_hook_call_effect_end`), each element `{...}` value an ordinary `Store`.
- **A5 — parser AST-build hooks.** `ast_hook_stmt` (count + bail on unsupported leaf
  statements) and `ast_hook_return`, gated by `CONFIG_AST` + `ast_active`; grew into the
  full hook set (vpush/genop/vstore/convert/call/if/while/for/do/inc/indir/gaddrof/
  ternary/landor).
- **A6 — differential-exec replay gate (three layers, all green).**
  `tests/ast/replay.cmake` — targeted fixtures that must *actually* replay (dump fired),
  from `ret42` through `array_init`/`ternary`/`logand`/`for_infinite`, plus a
  `switch_fallback` safety-net case (must fall back to correct `-O0`) and the
  `template-constfold` case (fold must fire *and* stay byte-faithful). **`exec-replay/*`
  column** — the whole `tests/exec` corpus re-run with `MCC_AST_REPLAY=1`, asserting the
  same expected output; functions the driver can lower go through the AST, everything
  else falls back. **`exec-replay-tmpl/*` column** — the same corpus re-run with the
  const-fold template also on (`MCC_AST_TEMPLATES=1`): the §15 whole-corpus per-template
  differential gate (input == output).
- **A7 — first template = const-fold** (docs/AST.md §12/§15/§17-D-d). A tree-scope rewrite
  `Binary(op, Literal, Literal) → Literal(fold)` over the pure integer arithmetic/bitwise/
  shift subset, run on the AST *above* the emitter before replay (`ast_run_templates`/
  `ast_fold_rec`/`ast_fold_eval`; new `ast_set_kind`/`ast_clear_children` builder API).
  The fold mirrors `gen_opic` exactly (same `value64` normalization / signed div), so it
  is **byte-neutral** — gen_op already folds adjacent constants at `-O0`, so a folded node
  re-emits bit-for-bit and the byte-verify net still governs correctness. Gated by
  `MCC_AST_TEMPLATES`. This **closes the AST first phase** (§17); further templates
  (algebraic, dead-branch, jump-table) and coverage widening are §16 Mid.

## AST replay coverage widening (§16 Mid — 2026-07-08, 19 milestones)

Building on the first-phase replay driver, the `MCC_AST_REPLAY` path was widened to
cover essentially all of C, one construct at a time, each landing with an
`ast/replay-*` fixture and the whole-corpus `exec-replay`/`exec-replay-tmpl` columns
staying green. The design + per-item detail is in [docs/AST.md](AST.md) §A3 and the
per-milestone entries in `docs/TODO.md`; `-O0` byte-identity was preserved throughout
(all new work is `CONFIG_AST`-gated and replay-path-only, with the byte-verify net as
the backstop). All local presets (`ast`/`debug`/`cst`/`release`) 100% green (1483
per-case tests).

**Landed (19):** float/double (arith, casts, comparisons, params/returns, const-pool
ordinal reuse); call-result stores (`T x = f()`); scalar struct member access
(`.`/`->`); struct copy/deref (`a=b`, `*a=*b`, `(*p).x`); `switch` dispatch (cases,
ranges, fall-through, default, nested); named `goto`/labels (forward + backward);
struct-return **callees** (`return s`); by-value struct **args** (`f(s)`); bit-field
member access (read/write); struct-return **callers** — **all four ABI forms**
(register-return, sret hidden-pointer, arch-transfer, variadic); `f().x` (member of
an rvalue struct); `_Complex` arithmetic; `__real__`/`__imag__`; `_Complex` casts;
`_Complex` imaginary literals (`r + 2.0i`); short-circuit results used as values
(`int r=a&&b`, `(a&&b)+1`).

**Two latent correctness bugs fixed** (surfaced by the widening, guarded by fixtures):
- **vpop call double-emit** — a `Store` leaves the RHS value on the capture mirror as
  the assignment's result; `ast_hook_vpop` re-added that already-parented `Invoke` as a
  bare BB effect, so replay emitted the call twice. Fixed by only re-adding an unparented
  node.
- **float const-pool duplication** — `gv()` materialized a float/double constant into a
  fresh rodata slot; replaying `gv` made a *second* slot, diverging the relocation. The
  parse-build now records each const-pool symbol and replay reuses them ordinally
  (`ast_fconst`).
- Plus a **switch-replay segfault** guarded: the controlling value must be a reloadable
  leaf (a computed value lives in a register the body clobbers), fixed after a crash on
  grep's `switch(tolower(...))`.

**Two reusable enablers** (see AST.md §A3 "Key mechanisms"):
- **Ordinal frame-slot reuse** (`ast_alloc_loc`/`ast_locrec`) — replay reserves the same
  anonymous frame offsets the parse-build used (struct-return temps, `_Complex` temps).
  Its absence caused a real frame-layout regression *outside* the byte-verified body,
  caught by the exec suite, not byte-verify.
- **Suppress-and-fold** — coarse-capture an operation whose internal ops would desync the
  mirror (member access, `vcheck_cmp` VT_CMP→0/1, `gen_complex_cast`, imaginary literals),
  reproduced faithfully at replay.

**Query-first framing (docs/AST.md §18, 2026-07-08).** The vstack/ABI backend is
**feature-complete C11 — this exec suite is the proof.** The parser is just one driver
demonstrating the opcodes span the language; the replay driver is a *second* driver of
the *same* ops and never fakes/reimplements anything (no faux-stack, no abstract
machine). Reaching `-O0` parity = "use the tools it already has"; `-O1` beats `-O0` only
via two AST-only queries — inline (Tier 4) and register-promote (Tier 3). Consequently
**every remaining gap is an AST-*query* gap, not a codegen gap:** the op exists, the
driver lacks the query to drive it. C11 flatten hazards (`setjmp`/VLA/signals) relocate
to **guard queries** (§18.4), not new machinery.

**Remaining (3, all fall back correctly — tracked open in docs/TODO.md) — all Tier-2
query gaps (§18.3), *not* codegen gaps (`-O0` compiles all three):** VLA/`alloca` (the
`StackAlloc`/`StackSave`/`StackRestore` ops exist; missing the **lexical-scope-edge
query** for LIFO save/restore placement); the `__builtin_complex`-based `I` unit
(`r + i*I`, needs the **rodata-const-symbol-reuse query** — the `ast_fconst` ordinal
pattern; a plain leaf capture link-errors); and nested short-circuit operands
(`(a&&b)||c`, needs the **nested landor-chain query** — the flat gvtst reproduction
segfaults on grep). Each was attempted; the two broken attempts (link error, segfault)
were reverted rather than left in the tree.

## Completed — Now-queue decisions, limitations & boundaries (2026-07-07/08)

Triaged items migrated from `docs/TODO.md` "Now": each was resolved as a documented
decision or accepted platform/ABI limitation and is pinned by a boundary test.

- **`exec/tls` on `msvc / arm64` — not an mcc defect.** The `msvc/arm64`-built
  `mcc.exe` nondeterministically drops/truncates functions when it compiles a
  `__thread` TU (`tls.c`) → the linked exe hangs. Root cause is **MSVC's arm64 backend
  miscompiling mcc itself**, not mcc's codegen: the same mcc source built by gcc
  (x86_64 + arm64 Linux) and by MSVC-x64 cross-targeting arm64-win32 all emit a
  byte-identical, correct `tls.s` (50×/30× runs); Valgrind + `-ftrivial-auto-var-init`
  clean. Mitigated with a skip on arm64+WIN32 + a `--timeout 300` on the msvc ctest
  step; `__thread` codegen stays covered on x86_64-WIN32 and every gcc/clang arm64
  target. A scoped `#pragma optimize("",off)` around the arm64 TLS-access codegen did
  **not** fix it (reverted 435087ee — also used raw `_MSC_VER`/`_M_ARM64`, which the
  `host-gate-invariant` test forbids outside `src/mcchost.{h,c}`). Bisecting the
  miscompiled construct needs an arm64 Windows + MSVC box.
- **`va_start` non-last / `register` param misuse diagnostic absent on x86_64-SysV /
  i386 (deferred, diagnostic-only).** The SysV `__builtin_va_start` macro reads the
  reg-save area and never references `parmN`, so the misuse warning (present on
  arm64/riscv64/PE via the real `TOK_builtin_va_start`) can't fire. Making it
  target-independent needs SysV to lower `va_start` through the real builtin
  (`gen_va_start`) — a varargs codegen rework of the primary target for a diagnostic-only
  gain; not worth the risk without a driving need. Ref to mirror once fixed: gcc
  `c-c++-common/Wvarargs-2.c`.
- **External (`SHN_UNDEF`) TLS symbols hard-error on Mach-O (intentional limitation).**
  `src/objfmt/mccmacho.c`. Locally-defined `__thread` works (TLV descriptors via
  `__tlv_bootstrap`); cross-module `extern __thread` errors. The fix is emitting TLV
  *import* descriptors — revisit only if a real cross-module-TLS-on-Darwin need appears.
- **i386 fastcall/thiscall: a non-register arg before a register arg is unsupported
  (accepted limitation).** `src/arch/i386/i386-gen.c` errors when a register-eligible
  integer arg follows a stack-spilled arg in a `__fastcall`/`__thiscall` call. The
  affected shapes are rare (a large by-value aggregate or over-width value ahead of a
  small integer) and a fix can't be validated without an i386 runtime here; the
  diagnostic is the pinned boundary. Register-then-stack ordering is fully supported
  (`i386-fastcall-abi`).
- **ARM (32-bit) inline-asm `long long` operands unimplemented (documented subset).**
  `src/arch/arm/arm-asm.c` hard-errors on 64-bit GPR-pair operands. mcc's inline
  assembler is a modelled subset for what C inline `asm` needs, not a full ISA (use two
  32-bit operands / a vmov round-trip for 64-bit values).
- **arm64 inline assembler errors on unmodeled mnemonics (supported subset documented).**
  `src/arch/arm64/arm64-asm.c` models the common subset (add/sub/logical/shift/mul/mov*/
  mrs·msr/ldr·str·ldp·stp/branches·cbz/adrp/barriers/ret/nop) and hard-errors by exact
  name on the rest (`cmp`/`csel`/`udiv`/`madd`/`fadd`/`neg`/`ldxr`, …). The self-naming
  error is the pinned boundary; expand the table when a real inline-asm workload needs a
  mnemonic.
- **Six permanently-masked ARM asm encodings resolved (4 fixed + byte-verified, 2
  non-defects).** `mov #0xEFFF` / `mov #0x0201` now synthesize `movw Rd,#imm16` when an
  immediate is neither rotated nor inverted (matches GNU as `e30e2fff`/`e3004201`).
  `vmov.f32 r2,r3,d1` / `vmov.f32 d1,r2,r3` — a `d`-register operand promotes the op to
  double regardless of the `.f32` suffix (`ec532b11`/`ec432b11`). `b r3` / `bl r3` are
  not encoding defects (byte-identical to gas; mcc now emits `R_ARM_JUMP24`/`R_ARM_CALL`);
  the only residual is objdump's symbolic target annotation, so they stay in
  `ARM_KNOWN_FAIL` documented. All four fixes byte-verified vs `arm-linux-gnueabi-as`.
- **ARM/arm64 direct branch can't reach past ±32MB — no veneers (documented boundary).**
  `encbranch` (`src/arch/arm/arm-gen.c`) encodes `B`/`BL` with the 24-bit signed word
  displacement (±32MB); a farther target is a hard `mcc_error("branch target out of
  range")` (arm64 has the matching limit in `arm64-gen.c`). Real toolchains synthesize a
  veneer (long-branch trampoline); emit one for out-of-reach `B`/`BL` when an image that
  large actually surfaces.
- **Windows keeps diagnostic *auto*-color off (validated).** `-fdiagnostics-color=always`
  *does* force color on Windows (`diag_want_color` returns 1 for `diag_color==1`
  unconditionally); only *auto*-detection is off (`host_stderr_isatty` hardcodes
  `return 0` on `_WIN32`). Explicit override covers the real need; the auto-detect
  enhancement (`_isatty(2) && GetConsoleMode(...) & ENABLE_VIRTUAL_TERMINAL_PROCESSING`)
  can't be compile-validated from this Linux host, and a broken Windows build is worse
  than color-off, so it is deferred to a change made with a Windows toolchain in hand.
- **Reference-harness `exec`/`diff3` goldens are documentation, not coverage holes.** The
  four `note:`-skipped goldens (inline multi-unit, backtrace, btdll, alias) carry full
  expected output but self-skip because each needs a bespoke harness the exec runner has
  no mode for; their behavior *is* covered by executing tests (the §6.7.4 emission matrix
  by `cli/c99_inline_emission_matrix`; single-TU alias by `alias_single_tu`; bcheck
  detection by the `bound_*`/`builtins` goldens). Their `req` notes now say so. Optional
  residual: a multi-variant backtrace harness if formatted `-bt`/`-b` output ever needs
  execution-level pinning.
- **`-fverbose-asm`-style operand comments — won't-do (low-value).** Meaningful comments
  need codegen-side variable/spill metadata that is discarded after emission; reloc
  symbol names are already printed. Revisit only if a debugging workflow needs it.
- **CST slice-I symbol resolution is last-declaration-wins (intentional v1).**
  `cst_hook_def`/`cst_hook_use` key def offsets by identifier token id in a single slot
  (`cst_defoff[v]`) — no scope stack — so a file-scope name shadowed inside a function
  resolves both uses to the last-declared def. Recorded as the v1 boundary (the CST
  symref is a side-channel tooling aid, not codegen; codegen scoping is the parser's and
  is correct); a scope-aware resolver is LSP-era work. Pinned by `tests/cst/symref/
  shadow.c` + `cst/symref-shadow`.
- **CST slice-J macro-invocation v1 imprecisions (accepted v1).** A function-like
  invocation's trailing `)` splits into a sibling `Paren` node, and an object-like macro
  used inside another macro's args stays a plain token (no nested `MacroInvocation`). The
  byte-identical round-trip (the load-bearing invariant) holds in both; a precise expander
  is slice-J/LSP-era work. Pinned by `tests/cst/macro/macro_nesting.c` + `cst/macro-nesting`.
- **CST 5B incremental splice + `H_e` epoch hash designed, not built (deferred to the
  LSP/5B consumer).** The invertible epoch hash + tombstone sweep and the 5B splice are
  reserved (slot-key field + frontier-scoped `H_s`-recompute) but unbuilt; gated on 4B
  rolling-hash + error-recovery + `Error`/`Missing` nodes. Note: D3 repurposed `slot_key`
  for branch tags, so an `H_e` build must reconcile that column's dual use.


## Migrated from code comments (2026-07-06 comment-strip)

The codebase's inline comments were removed in favour of self-documenting code;
the non-obvious *why* — design rationale, cross-compiler/ABI gotchas, and
deliberate decisions — was moved here. Open-work items went to `docs/TODO.md`.

### CST recording — deferred-capture implementation (`src/mcccst.c`)

§6 above describes the hook *API* (`cst_open`/`cst_close` bracketing). The
*implementation* does not build the tree live: mcc is a single-pass parser with
one token of lookahead, so when a grammar function brackets a construct its first
token has **already** been lexed and captured. Capture therefore records a flat,
source-ordered **leaf list** plus structural **specs** as leaf-index ranges, and
materializes the nested tree in `cst_hook_end`.

- **Lookahead rule.** A node opened while `tok` is its first token spans leaves
  `[open_leafcount - 1, close_leafcount - 1)` — `cst_hook_close` excludes the
  lookahead token that belongs to the next construct.
- **Spec nesting (`cst_nest_specs`).** Parent/child links are rebuilt purely from
  `[first_leaf, last_leaf)` containment (not the open-time parent) so
  retroactively-opened expression nodes nest correctly. All grammar constructs
  are disjoint-or-nested, so a stack over the sorted specs yields the true tree.
  *Partial-overlap* specs (e.g. a `Declaration` whose start-mark is stale across
  an `#include` that straddles a PP-boundary node) are **dropped**, not tiled
  under two siblings — tiling them would break the round-trip oracle (§8.1).
  *Empty* specs (`first == last`, i.e. retroactive wraps around macro-expanded
  expressions that captured no source leaves) are also dropped, except the root.
- **Sort tie-break (`cst_spec_cmp`).** Order is `first_leaf` asc, then wider range
  first (`last_leaf` desc), then later-opened first (id desc). A grouping wrapper
  (`Initializer`/`Declaration`/…) is opened *after* the sub-expression it contains
  (retroactive range-wrap); when its span coincides byte-for-byte with its single
  child (e.g. `int c = (int)a;` where the `Initializer` equals the `Cast`) the
  higher id must sort first so the containment stack makes it the parent.
- **Self-contained memory.** `src/mcccst.c` uses only the C library so
  `tools/csttool.c` can link it alone (invariant §0.3). `mcc.h` poisons
  `malloc`/`realloc`/`free` to force mcc's allocators, so the file `push_macro`s
  and `#undef`s them (a no-op in the standalone `csttool` build).
- **Column reuse.** An `IncludeDirective`'s cross-file target (the interned
  `SourceFile` template id) is stored in the `sym_ref` column as `(template_id, 0)`;
  a `PPConditional` branch ordinal is stored in the reserved `slot_key` column
  (the D3 `slot_key` dual-use is also noted in §10 / TODO). `cst_store_intern`
  dedups by pure `H_s(root)`; a non-`NDEBUG` build verifies the two entries reflect
  byte-identically as a hash-collision tripwire before fusing them.

### Diagnostics — deliberate error-vs-warning choices (`src/libmcc.c`)

- **Calling an undeclared function is a hard error** (C99 §6.9.1/§7.1.4 constraint
  violation, not a warning). This prevents implicit-`int`/pointer-truncation
  miscompiles and makes autoconf-style feature detection behave. See
  `docs/C9911.md` §6.5.1p2 (undeclared identifier).
- **Return-type mismatch is an error**, downgradable via
  `-Wno-error=return-type` / `-Wno-return-type`: a `return <value>;` in a `void`
  function, or a value-less `return;` in a non-`void` function.
- **Caret rendering** never colorizes the embedder callback path (`error_func`)
  — embedders want plain text; `auto` follows the tty. The `CString` printf
  helpers keep `data[size]` a NUL not counted in `size`, so `"%s"` stops there.

### Runtime library — cross-compiler & ABI notes (`runtime/`)

The runtime is built by the *host* cc (gcc/clang/MSVC) as well as by mcc, so many
constructs exist to satisfy a foreign toolchain's type checker or assembler:

- **Builtin-name conflicts → assembler `.set` / asm labels.** Clang refuses to
  see several libcall names *defined* in C because it types them as builtins
  (`__atomic_*`, `__builtin_bswap64` as `uint64_t`, the four generic `libatomic`
  entry points). Mach-O also rejects non-weak `__attribute__((alias))` and gcc
  ICEs on `weak,alias` there. The runtime therefore exports those symbols through
  an assembler `.set` / asm label rather than a C alias
  (`builtin.c`, `stdatomic.c`, `atomic.c`).
- **`stdatomic.h` pointee types** must match Clang's builtin prototypes: a plain
  `void *` pointee conflicts with `__atomic_is_lock_free`; `bcheck.c`'s
  `never_fatal` is a plain `int` (not `_Atomic`) because the non-`_c11`
  `__atomic_fetch_add` rejects an `_Atomic(int)*` pointee under Clang.
- **Mach-O assembler quirks.** A foreign (host-CC) Mach-O assembler has no
  `.type`/`.size` directives and needs assembler-local labels to start with `L`,
  not `.L` (`atomic.c`).
- **`regparm`** is x86-only: gcc silently ignores it off-x86, clang errors — apply
  it only where meaningful (`bcheck.c`).
- **Cache-flush builtins.** `armflush.c` / `lib-riscv.c` exist only for the mcc
  build; gcc and clang provide the cache flush as a builtin (there is no
  `__arm64_clear_cache` / `__riscv64_clear_cache` symbol outside mcc).
- **`__unordtf2`** helper: clang `-O2` (unlike gcc) emits it for the
  NaN/unordered checks in the 128-bit `long double _Complex` routines;
  `f3_cmp` already returns the unordered sentinel `2` (`lib-arm64.c`).
- **win32 `fenv`.** The rounding-mode value is the C99 `FE_*` encoding, which
  matches the x87 control-word field (bits 10-11): `FE_TONEAREST` 0x000,
  `FE_DOWNWARD` 0x400, etc. On x86 set both the x87 CW and the SSE `MXCSR`
  (bits 13-14 == the x87 field `<< 3`) since scalar FP uses SSE while `long
  double` uses x87. On arm64 the mode is `FPCR` bits 23:22 (00 RN, 01 RP, 10 RM,
  11 RZ).
- **win32 `errno`.** The POSIX thread codes are not in msvcrt's set; the
  `<pthread.h>`/`<threads.h>` shims use the same values mingw does.
- **win32 `pthread`.** The control block is intentionally *leaked* rather than
  freed under the still-running thread; `pthread_self`/`equal` only need to be
  self-consistent for identity checks, so the thread id serves as the token.

### Tooling (`tools/`) — design & portability notes

- **`csttool.c`** is a pure-library harness proving slices B–E against synthetic
  trees before Weave 1; it links nothing from the compiler (invariant §0.3).
  Gates: hashing invariance (struct hash matches, trivia hash differs), symbol
  refs as node-ids (def↔use round-trip), D1d comment promotion (excluded from
  `H_s`, feeds `H_t`), and the D3/D5 template + content-addressed store dedup.
- **`bench.c`** (default-off; CI enables via `MCC_BENCH`; host-only): `ru_maxrss`
  is KiB on Linux but bytes on Darwin; `/proc` and `/sys` files report
  `st_size == 0` so they must be streamed; aarch64 has no model name so the CPU
  name is decoded from `MIDR` (implementer, part); CPUID leaf-1 ECX bit 31 means
  running under a hypervisor; `cl` rejects GNU probe flags (MSB8066) so its
  version is parsed from the banner; the JUnit parse is bounded so a later case's
  `<skipped>` can't leak in.
- **`ci.c`**: stage3 CRT lands in `usr/lib64` but the multilib driver looks in
  `usr/lib`, so it is mirrored; x86_64 sysroots are pre-fetched idempotently via
  a marker file; the dist flow is exactly `configure→build→install→(strip)→
  bench→package-dist` from `release.yml`; `MCC_BUILD_STRIP` is off for
  `dist-macos` (a full strip trips codesigning); `ctest` wants the JUnit path as
  a separate token (`--output-junit=…` is rejected); in the scratch/out cleanup,
  `stage == out` so `out` must never be removed; the `bundle-` asset prefix
  signals the archive that holds the others; the JUnit summarizer is
  self-contained so linux/qemu jobs still emit PASS/FAIL/SKIP.
- **`mccharness.c`**: with no gcc/clang consensus it does a 2-way check that
  matches *either* reference and never introduces a FAIL (impl-defined
  divergence); `curl -o` fails on a fresh tree (a docker volume masks this, local
  ctest hit it) so the dir is created first; the 1-slice arm64 suite is the hard
  guarantee, 2-slice needs cross+SDK and self-skips.
- **`machofat.c`** is a post-link combiner (a single mcc can't emit fat):
  page-alignment (arm64 16 KiB, others 4 KiB; `≥` the arch page size) keeps
  signatures/segments valid; it ad-hoc re-signs because Apple silicon rejects a
  mismatched signature / stale AMFI cache; `codesign` exit 127 means absent
  (skip), other nonzero means it ran and failed (surface it).
- **`objcheck.c`**: legacy `version_min` packs X.Y.Z at offset 8 vs
  `LC_BUILD_VERSION` `minos` at 12; slice offsets are per sub-arch for fat, single
  0 for thin; `--arch` restricts to the named sub-arch, else a later-slice
  mismatch still fails.
- **`build.c` / `bin2c.c`**: `build.c` bounds a `%s` so gcc
  `-Wformat-truncation` sees it fits; `bin2c.c` runs at build time so its input
  may be a build product (e.g. `libmccrt.a`).

### Test harness — directive & runner semantics (`tests/`)

- **`skipon=<cpu>/<os>[:reason]`** excludes exactly one cpu/os combination while
  running everywhere else — unlike the require-style gates, which are inclusive
  (`tests/exec/runner.c`).
- **`MCC_TEST_RUNEMU`** launches the *compiled exe* under an emulator (e.g.
  `qemu-aarch64 -L <sysroot>`) while the cross compiler itself runs natively with
  `MCC_TEST_SYSROOT` flags; only `run`-mode goldens apply (the `-run`/JIT modes
  need a native/foreign compiler).
- **CST fixtures keep their comments on purpose.** `tests/cst/**` and
  `tests/exec/preprocessor/comment.c` treat comments as parsed input (captured as
  trivia, folded into `H_t`, or the literal subject of the test), so those
  comments are payload, not decoration, and were left in place.

## Migrated from code comments (2026-07-09 — full-tree strip)

Design and implementation notes migrated out of the source code. Every code
comment in the tree was moved here (the code itself now carries none), organized
as a per-file prose summary. Section headings are repo-relative paths.

Not migrated (comments left in place because they are load-bearing):

- `tests/exec/programs/grep.c` — the `grep` exec test runs the built program
  **over `grep.c` itself** and expects to match its `/* vim: … */` modeline, so
  its comments are test data.
- `tests/cst/kinds/comment.c`, `tests/exec/preprocessor/comment.c`,
  `tests/preprocess/asm/gas_comments.S`, `tests/cst/hashinv/{compact,spaced,changed}.c`
  — tests whose subject *is* comment handling / comment-and-whitespace hash
  invariance.
- `runtime/win32/include/winapi/winerror.h`, `winnt.h` — third-party Win32
  headers (carry upstream license/copyright text).

Related change: because comment removal can shift source line numbers,
`tests/exec/runner.c` now normalizes `file.c:NN:` → `file.c:N:` in both the
expected and actual output before comparing (`mask_linenos`), so the exec
goldens no longer pin exact diagnostic line numbers. `tests/diff3/runner.c` is
deliberately left exact — it is a live gcc/clang/mcc differential over program
*stdout* (compiler diagnostics are discarded during its build), so cross-compiler
agreement stays byte-exact.

---

### The compiler core — `src/`

#### `src/mccgen.c` — AST intention-IR replay and the `-O1` optimizer

This file carries the parser-side build hooks, the replay emit driver, and the
optimization tiers for the AST intention IR (see `docs/AST.md`). It is the bulk
of the migrated documentation.

**Replay architecture (§10/§17).** When `MCC_AST_REPLAY` is set, `gen_function`
builds a per-function intention tree while the parser runs, then discards the
parser's body emission and re-emits from the AST through the same vstack API.
Anything the builder does not yet understand sets `ast_bail`, and `gen_function`
keeps the parser's (correct, `-O0`) emission instead — so the path is safe over
the whole corpus and grows one construct at a time. It is off by default; the
`-O0` path never touches any of it. The `MCC_AST_*` environment flags gate the
layers independently for testing: `REPLAY` (build+replay), `REPLAY_DUMP` (dump
trees), `TEMPLATES` (optimization templates), `PROMOTE` (Tier-3 register
promotion), `NO_CALLFUL` (force call-free promotion only), `INLINE` (Tier-4
virtual always-inline).

**The vstack mirror.** A small shadow stack mirrors the vstack while an
expression is evaluated, so its intention tree can be captured op by op. It is
kept synced to `vtop`'s depth after every modeled primitive; any unmodeled depth
change or an in-place cast trips `ast_desync` and the function falls back to the
parser's emission. State flags track whether the mirror is live (`ast_capture`),
its depth relative to the capture base, and whether we are inside a `gen_op`/
`vstore` (`ast_in_op`) or across a call boundary (`ast_in_call`) whose internal
vstack traffic must be ignored. Leaves are re-read from the live vstack at
consumption time because a push hook fires inside `vsetc` but callers finish a
leaf afterwards (`vpushsym` / the identifier path set `->sym` and adjust `->c.i`
only after `vsetc` returns) — eager capture can be incomplete, so it is finalized
when the value is consumed. Only leaves that re-push faithfully after the body is
discarded are safe: an integer constant, a frame-relative local/parameter lvalue
(fixed offset), or a symbolic constant/lvalue (a global's address — the `Sym`
persists). Anything holding a register, or a float value materialized by
soon-discarded code, is not reconstructable and desyncs.

**Block-scoped-symbol re-emit hazard (`ast_reemit_poison`).** Immediate replay
re-pushes captured `Sym` pointers while they are still live. End-of-TU
re-emission does not: a block-scoped symbolic reference (an inner extern/function
redeclaration) or a block-scoped aggregate *type* ref is freed at block close and
recycled by later codegen, so its pointer dangles by re-emit time. Seeing one
poisons re-emission for that function; grafting/immediate replay are unaffected.

**Const-pool / frame-slot ordinal reuse (§A3, §18.3).** Body-emitted rodata
constants (scalar `float`/`double` pools and `_Complex` const-folds/casts) are
materialized fresh on every emit pass. Because discard/replay only rewinds text +
text relocations, a naive replay would allocate a *second* rodata slot and a new
anonymous symbol, so the relocation content would diverge even though the value
is identical. Instead the parse-build records each such symbol (its ELF symbol
index `sym->c`) in emission order, and replay reuses them ordinally — referencing
the same ELF symbol so the relocation is byte-identical and skipping the
re-materialization. The same ordinal trick handles struct-return result temps:
a `loc = (loc - size) & -align` frame-slot decrement is recorded at build and the
offset reused at replay (`ast_alloc_loc`), so temp offsets match the parse-build
and the frame stays parse-final. Outside a replay-candidate build these recorders
are exactly the original decrement, so `-O0` stays byte-identical.

**Control-flow capture.** Effects append to the currently open `BasicBlock`;
`if` branches open nested BasicBlocks pushed on a small stack. Loops and
conditionals are captured as `If` nodes with an `op` discriminator: `if` (plain),
`op==2` while-loop, `op==3` for, `op==4` do-while, `op==5` ternary, `op==6`
switch; `for(;;)` with no controlling expression uses `op==5`/an empty break
chain. Replay re-issues the parser's exact `gind`/`gvtst`/`gjmp`/`gsym` pattern
for each shape, so no backend jump primitive needs hooking. `break`/`continue`
are `Jump` nodes chaining onto the loop's replay-time break/continue chains;
named labels and `goto` are `Jump` markers (`op==4` label / `op==5` goto)
resolved through a per-function replay label table reproducing the parser's
forward-chain / backward-jump / definition-backpatch dance. `switch` bodies carry
`case` (`Jump op==2`, `ival=v1`, `fbits=v2`) and `default` (`op==3`) markers; the
dispatch epilogue (`case_sort`/`gcase` binary search) is suppressed during
capture and reproduced at replay from a rebuilt `switch_t`. The controlling value
must be a reloadable leaf (a frame/global lvalue or constant) because the
dispatch re-reads it after the body. VLA/cleanup/computed-goto scope machinery is
unmodeled and bails.

**Short-circuit and ternary.** `&&`/`||` are captured as a `Binary` node
(`TOK_LAND`/`TOK_LOR`) with all operands as children; the `VT_CMP`→0/1
materialization (setcc) and the `gvtst` chaining are suppressed, and replay
re-materializes the chain when the consuming op runs (reproducing `expr_landor`'s
`gvtst` chain + `gvtst_set`). Only the all-runtime `VT_CMP` form is modeled;
constant operands / the materialized 0-1 form desync. A nested short-circuit
operand rides as a child and replay's `AST_Binary` case recurses. A ternary
`c ? a : b` is captured as an `If(op==5)` `[cond, true, false]`, with the branch
coordination (`save_regs`/`gvtst`/`gjmp`/`gsym`/`gv`/`move_reg`) suppressed and
only the branch values captured; only the runtime-scalar case is modeled.

**Memory model, members, aggregates, `_Complex`, VLAs.** `indir` (deref) wraps
the top address in a `Load`; `gaddrof` wraps it in `Unary(AST_OP_ADDR)` — both
re-issue the primitive at replay so a computed-address lvalue (`a[i]`, `*p`) is
reconstructed from its address expression, not a non-reproducible register.
Member access `.`/`->` cannot be replayed op-by-op (the parser's in-place
`vtop->type` retypes fire no hooks and would scale the offset wrongly), so the
whole access is folded into one `Unary(AST_OP_MEMBER[_ARROW])` node (`ival` =
field byte offset) with the internal ops suppressed; replay reproduces the exact
`indir`/`gaddrof`/retype-to-`char*`/`+offset`/retype-to-member/mark-lvalue
sequence, preserving the base's non-lvalue bit. Struct→struct assignment is an
aggregate copy (`memmove`/`gen_struct_copy`) recorded as a `Store`; bit-field
destinations record a `Store` and let replay reproduce the read-modify-write
inside the suppressed `vstore`. Imaginary literals (`2.0i`) fold their `0 + val*i`
construction into one `Unary(AST_OP_IMAG)`; `__builtin_complex`/`I` capture the
rodata-const result as a single `Ref` leaf (its anon `Sym` persists). VLA
declarations capture the machine-tier alloc sequence
(`gen_vla_sp_save`/`gen_vla_alloc`) as one coarse `Unary(AST_OP_VLA)` effect and
the paired SP restore at a scope edge as either a `Return` annotation
(function-scope) or an `AST_OP_VLA_RESTORE` effect (a nested block's `}`); `loc`
is not decremented at replay. The PE alloc path moves the frame and is not
modeled.

**Calls.** Before `gfunc_call` the mirror holds `[callee, arg0..arg_{n-1}]`;
these fold into an `Invoke`, the mirror is suppressed across the call and result
push, and the `Invoke` is pushed as the result. A call captured while byte
emission is suppressed (`nocode_wanted`: an unevaluated `_Generic`/`typeof`/
`sizeof` operand or a dead branch) is a phantom and abandons replay for the
function — replaying it would emit stray bytes/relocations. Register-return,
sret hidden-pointer, and arch-transfer (mixed INT+SSE) struct returns are all
modeled via ordinal frame slots, including variadic struct returns (the
struct-return ABI is independent of varargs). The callee must be a reconstructable
reference — a direct function symbol or a pointer variable — a computed callee (a
ternary/call-result/member `s.fn()`) replays as a value whose function-type `ref`
the driver cannot reconstruct and would SIGSEGV before byte-verify, so it bails
(surfaced by the gcc c-torture gate `pr34768-1/-2`). By-value struct args are
attempted (with the byte-verify net as backstop); bit-field/`long double`/
`_Complex`-pair args are an ABI copy this rung does not model and bail.

**Tier-3 register promotion (§4/§10/§18.2).** The first optimization that
deliberately beats `-O0`: keep an address-not-taken scalar local in a register
across the function instead of spilling it, eliminating its load/store traffic.
A read pushes the value as register-resident (`gv` copies it to a temp when
consumed, leaving the pin intact); a write forces the value into the pinned
register with the implicit assign-cast a memory store would apply. The register
is seeded from the local's stack slot at function entry, so promotion is valid
across arbitrary control flow and even for a parameter or a local read before
written — that is what makes loop variables promotable. Two pin pools: **call-free**
functions use caller-saved `R10`/`R9`/`R8` (R10 is used nowhere in the x86_64
backend; R8/R9 only in the call-arg arrays; `R11` is excluded — it backs `load`/
GOTPCREL/TLS) with no save/restore; **call-ful** functions use callee-saved
`RBX`/`R12`-`R15` (the backend never touches them, the ABI preserves them across
the internal calls) pushed at entry and popped at the single return funnel.
Floats promote into caller-saved `XMM6`/`XMM7` (call-free only). Poison analysis:
an offset is poisoned unless every reference is a full-width GP scalar — a
`Ref` under a `Unary` (`&x`, member base, `++`/`--`) needs an lvalue the
register-resident rvalue cannot supply (`MEMBER_ARROW` stays poisoned because its
lowering folds the offset in place and would clobber the pinned pointer); an
array's whole slot range is poisoned because a constant-index element looks
scalar but the address escapes via decayed-pointer arithmetic; `volatile`/
`_Atomic` locals must stay in memory. Candidates are weighted by loop-depth-scaled
reference frequency (an inner-loop use contributes `2^depth`) and the hottest
win the scarce pins (selection is a heuristic — any valid subset is correct).
`inline asm` (may clobber pins) and VLAs (callee-saved push/pop would race the
runtime `rsp` moves) exclude a function. Promotion is opt-in (`MCC_AST_PROMOTE`)
and bypasses byte-verify (a promoted function's bytes differ from `-O0` by
construction), so the exec-golden corpus is the gate.

**Tier-4 virtual always-inline (§9/§13/§19).** A within-TU `static` leaf-ish
helper whose captured body is graftable (small, non-variadic, VLA-free, internal
linkage) is retained keyed by its `Sym` instead of freed, so a later caller can
graft it in place of a boundary `Call`. Non-leaf callees graft recursively
(cycle-guarded by a graft stack and depth). A `setjmp`-family callee is excluded
(inlining would capture the caller's frame). Grafting **deletes the ABI**: every
param — scalar, arm64-positive, or by-value struct — is materialized into a fresh
caller-frame slot, so register-vs-memory classification no longer matters; the
whole callee frame is shifted below the caller's live locals and any positive
param extent (`hi`-based bias, §19.2). Each `return EXPR` coalesces its value into
a dedicated result slot (a phi via memory, so several grafts feeding one call each
own a distinct slot) and non-tail returns jump to a graft-local inline-end join.
The callee's control flow is isolated via a label floor. **Per-site specialization
(§19.3):** a constant argument bound to a read-only param is constant-propagated
(substituted at the param's `Ref` sites) instead of stored-and-reloaded, so
`gen_op`/`gvtst` fold it and — inside the graft (pass 2, not byte-verified) — a
condition that folds to a compile-time constant selects its taken branch and
drops the dead one entirely. **Defer-to-TU:** a function that calls a `static`
function not yet retained at its emission (a possible forward-inline miss) is
retained and, at end-of-TU (all callees now retained), re-emitted at a fresh text
offset with the forward callee grafted and its symbol repointed; referenced rodata
`Sym`s are pinned so they survive to re-emit time. Tier-4 is opt-in
(`MCC_AST_INLINE`) and not auto-enabled by `-O1` — it is broadly functional but
can blow up combinatorially self-compiling mcc, so it stays behind explicit opt-in
until hardened. Tier-3 is the documented "first opt that beats `-O0`."

**Const-fold template (§12/§15).** The first optimization template: `Binary(op,
Literal, Literal)` with a foldable integer op rewrites in place to a `Literal`.
It runs on the tree before replay; because `gen_op` already folds adjacent
constants at `-O0`, the template is byte-neutral (replay pushes the very `SValue`
`gen_op` would have produced, and byte-verify confirms it). Only the pure
arithmetic/bitwise/shift subset is folded — comparisons and `&&`/`||` have
special replay paths. The fold arithmetic mirrors `gen_opic` exactly (same
`value64` normalization / signed division).

**`-O1` wiring and the safety net.** `-O1+` engages the replay-driven optimizer.
Replay is arch-general and faithfulness-gated (each function byte-verifies against
`-O0` and falls back otherwise), so it is safe on every target; register
promotion and virtual-inline are x86_64-validated (the pin pools are x86_64
register indices) and gated on the target. `gen_function` wraps the whole
re-emission in a nested error trap and a diagnostic sink: replay drives the same
vstack ops the parser did, so a construct captured imperfectly can raise
`mcc_error` mid-emit — that must never fail a program `-O0` compiles, so the trap
catches it, restores the parser's saved `-O0` bytes, and continues (the same
fallback a byte-mismatch takes). Diagnostics are suppressed during replay (the ops
already warned during the parse-build). The cleanup floor is raised so the trap's
`longjmp` frees only what replay pushed, never the outer compile's live
`stk_data`. Because the AST captures rodata-const leaves as raw `Sym` pointers
that may already sit on the sym free-list, the pre-replay free-list is hidden for
the duration so every replay allocation is fresh (the two-pass promote path would
otherwise re-read a corrupted pointer in pass 2). Pass 1 replays with no
promotion and byte-verifies; pass 2 applies the byte-diverging transforms
(virtual-inline graft + register promotion, which compose). A faithful body with
no pass-2 transform restores the `-O0` final `loc` (an unfaithful pass-1 replay
can leave `loc` shallower, and `gfunc_epilog` sizes the frame from it — a
too-small frame clobbers the locals below, e.g. gcc c-torture `20020215-1`).

**Inline linkage (§6.7.4/§6.9.1).** A plain `inline` definition (no `extern`, no
`static`) provides no external definition — mcc emits it only when the function
acquired an external definition (`VT_INLINE` cleared) or is a used static-inline;
a referenced plain inline still has its body parsed so the §6.7.4p3 diagnostics
fire, then the generated definition is discarded so the symbol stays undefined
(as in gcc/clang). A `static inline` definition following an `extern`/plain
prototype must preserve its own `static` (internal-linkage) so it is emitted —
otherwise it collapses to a plain external inline, which §6.7.4p7 leaves
undefined and which breaks the win32 libm shims. A K&R identifier-list parameter
with no matching declaration defaulting to `int` is a C99+ constraint violation
(§6.9.1p6/§6.7.2p2; C89 allowed it).

#### `src/mccast.h` / `src/mccast.c` — the AST library

The AST is an intention IR alongside the CST (`docs/AST.md`): where the CST is
byte-faithful concrete syntax, the AST is intention — desugared, type-resolved,
post-preprocessor. It is a pure side-channel like the CST: `-O0` never builds or
reads it; `-O1` builds it (lowered from the typed CST / parser) and replays it
through the existing vstack API. The header declares only the structure-agnostic
arena/builder/dump API; types and symbols ride as opaque handles (`type_t`
carries the `CType.t` bit-field, `type_ref`/`sym` carry opaque `Sym*` casts) that
the full build fills in, the replay driver in `mccgen.c` reconstructs, and the
pure-library harness (`tools/asttool.c`) treats as tags.

Node kinds: **structural** (TranslationUnit, BasicBlock, …), **terminators**,
and **values** — `Ref` (address of a named object), `Literal` (int/float
constant), `Load` (explicit read of an address), `Store(addr, value)` (explicit
write), `Unary` (neg/bitnot single-operand machine ops), `Binary` (control-free
binary op, `op` = token value), `Convert` (genuine value conversion), `Invoke`
(abstract call: callee + arg values), `InitList` (aggregate initializer), plus a
recovery kind. The arena is a structure-of-arrays node store, one `AstArena` per
function (minimal, no hash-consing until virtual-inline lands); node 0 is the
first node created and doubles as the root. In-place mutation used by the
optimization templates retags a node and orphans its former children (they stay
in the per-function arena, unreferenced — no compaction until hash-consing). The
dump/reflection API prints a single printable token for a node's operator (the
ASCII operators print directly, multi-char tokens fall back to a numeric tag) and
validates structural invariants (mutually consistent parent/child links, `nchild`
matching the sibling chain, every reachable node's kind in range). The
amalgamated build poisons `malloc`/`realloc`/`free` so the compiler routes
through its tracked allocators; the AST library is a standalone side-channel with
its own lifetime, so it un-poisons them for the span of the file (as `mcccst.c`
does). The parser-side build hooks fire from the same parse positions as the CST
hooks and capture typed vstack values; they live in `mccgen.c` (where `vtop` is
visible) and are no-ops when `CONFIG_AST` is off or replay is not requested. All
of it is under `CONFIG_AST`.

#### `src/mcc.h`

`mcc_error`'s `longjmp` cleanup frees `stk_data` down to a floor rather than 0,
so a nested error trap (the AST `-O1` replay) can preserve the outer compile's
live cleanup entries; the floor is 0 for the normal top-level path.

#### `src/mcchost.h`

The native-backtrace helper prototypes are gated on the same condition as their
definitions (`mcchost.c`, used by `mccrun.c` only under the native backtrace
path) — declaring them unconditionally makes them unused functions when that path
is compiled out (e.g. the macos preset builds backtrace off).

The `host_fault_install`/`host_fault_regs`/`host_fault_unblock` prototypes for
that block live in **`src/mcc.h`** (right after the `MCC_IS_NATIVE` derivation),
**not** in `mcchost.h` — deliberately. `mcc.h` includes `mcchost.h` *before* it
derives `MCC_IS_NATIVE` (the derivation needs `mcchost.h`'s `MCC_HOST_*`), so the
gate `#if defined MCC_IS_NATIVE && defined CONFIG_MCC_BACKTRACE` would be false if
evaluated inside `mcchost.h`. The amalgamated build masks that (the definitions in
`mcchost.c` precede the call in `mccrun.c`, so no prototype is needed), but the
multisource build compiles `mccrun.c` as a separate TU and needs the visible
prototype — moving it below the derivation makes both builds see it. Do not move
these back into `mcchost.h`.

#### `src/arch/arm/arm-asm.c`

Three GNU-as-compatibility encoder notes: (1) a `mov Rd, #imm16` whose value is
neither a rotated nor an inverted immediate (e.g. `0xEFFF`, `0x0201`) synthesizes
`movw Rd, #imm16` exactly as GNU as does (MOV only, no shift, value in
`0..0xFFFF`, unconditional v6T2+; wider values still error — they would need a
`movw`+`movt` pair or a literal pool). (2) A `d`-register operand in a `vmov` is
the 64-bit two-GPR↔doubleword transfer (`vmov Rt, Rt2, Dm`); gas accepts a `.f32`
suffix on this form, so the operand type promotes to double regardless of the
mnemonic suffix. (3) Branch relocations follow the EABI as GNU as does:
`R_ARM_CALL` for `bl`, `R_ARM_JUMP24` for `b` (the linker handles all three types;
`R_ARM_PC24` is the legacy form).

#### `src/arch/x86_64/x86_64-gen.c`

Notes on register-indirect addressing under Tier-3 promotion. A base `[rv + c]`
is either the `TREG_MEM` indirect-with-disp form or a plain register holding an
address (a promoted pointer): a base whose low 3 bits are `100` (rsp/r12) needs a
SIB byte (`0x24`), and one that is `101` (rbp/r13) cannot use `mod=00` (that
encodes disp32-no-base) so it carries an explicit disp8 even when `c==0`. Because
the normal allocator never bases off r12/r13, every register it does use is
byte-identical to the historic `mod=00`/`mod=10` encoding. `TREG_MEM` keeps its
historic disp32 form. `REX.B` is taken from the destination base so a store
through a high-register base (r8-r15, a promoted pointer) is addressed correctly,
byte-identical for every base the normal allocator uses.

---

### Tools — `tools/`

#### `tools/ckconfig.c`

A config-drift checker for the `CONFIG_MCC_*` preprocessor surface. mcc's
build-time configuration flows CMake option → emitted `-DCONFIG_MCC_X` →
`#if CONFIG_MCC_X` in the code, with an in-code `#ifndef`-guarded default in a
header. ckconfig cross-checks the two ends so the mapping cannot rot: (a) a
`CONFIG_MCC_X` the code *reads* but that neither `CMakeLists.txt` mentions nor a
header `#define`s (an implicit/undefined config), and (b) one CMake emits as a
`-D` but the code never reads (a dead emission). Known-intentional exceptions are
listed in `ALLOW_*` with a rationale — e.g. an opt-in "backtrace-only" build
variant set outside the CMake surface, and a legacy uClibc provenance marker
nothing reads. Names a header `#define`s are code-internal (constant or
overridable default) and never "undefined". `tools/build.c` is a second emitter
(mccbuild): its `EMIT()` string literals are scanned as provider mentions, not
code reads, so a config it supplies is not flagged implicit; `tools/` headers may
`#define` code-internal `CONFIG_MCC_*` (e.g. `TOOLHOST`), collected as defines
only. Companion to `docs/CONFIG.md` and `BUILD.md`; `--list` prints the full
inventory. Mirrors `tools/hostgate.c`'s file-walking style.

#### `tools/mccharness.c`

The GCC-c-torture differential AST gate (`docs/AST.md §C1`). `--ast <mode>`
compiles/runs each test at `-O0` (baseline) *and* under an AST column
(`replay`/`promote`/`inline`/`inline-tmpl`), and only a test that PASSES at `-O0`
but FAILS under the column counts as a regression (the gate's exit status) —
baseline `-O0` gaps versus the GCC suite are not the driver's concern, since the
replay path is byte-identical-or-fallback and must match `-O0` test-for-test.
A baseline of **known pre-existing AST-column gaps** is maintained per column so
the gate stays green on "no new regression": the `REPLAY` set is the sound
foundation and must stay tiny (`pr51581-1/-2` — a replay leaves the `#if`
const-expr evaluator's shared vstack/jump state inconsistent, the parked `-O1`
self-compile issue; `20070919-1` — a block-scoped VLA-member struct causes cyclic
type recursion in `aggr_has_const_member`); `PROMOTE`/`INLINE` are the `-O1`
transform-soundness backlog (they diverge from `-O0` by construction, so
byte-verify cannot catch them — the gate is their only net), and both inherit the
replay set. Env handling is a portable per-process `setenv`/clear (the harness is
single-threaded, so `setenv` around a child spawn is safe). `run_one` returns
0=pass / 1=compile-fail / 2=exe-fail / 3=the "cannot use local functions" skip.

#### `tools/asttool.c`

The pure-AST-library unit harness: builds `2 + 3 * 4` as an intention tree and
checks geometry; verifies reuse-after-reset produces a fresh valid tree; builds a
minimal function shell (an entry BasicBlock terminated by `Return(value)`);
checks node provenance (every node can carry its origin CST node id, §14); and
exercises the template rewrite API (§12) — `ast_set_kind` + `ast_clear_children`
collapse a `Binary(Literal, Literal)` subtree into a `Literal` in place, folding
`2 + 3 * 4` bottom-up to a single `Literal 14`.

#### `tools/bench.c`

A cross-compiler benchmark. It measures every detected compiler twice — at its
default level (≈ `-O0`) and at its first optimization level — interleaving the two
per compiler so the report pairs them for direct comparison. The optimization
flag is pre-spelled for the compiler's style (`-O1`, or MSVC `/O1`); mcc's `-O1`
engages the AST replay optimizer.

---

### Tests — `tests/`

#### AST replay fixtures — `tests/ast/`

**`replay.cmake`** is the differential-exec driver (§17): it compiles `SRC` twice
— through the `-O0` parser→emit path and with the replay driver on
(`MCC_AST_REPLAY=1`) — links, runs both, and asserts the same exit code. Extra
list parameters assert a specific path *fired* (rather than silently falling back)
via the dump: `REPLAYED` (named functions actually replayed), `PROMOTES`
(Tier-3 promoted ≥1 local; a list so both the call-free and call-ful pools are
asserted), `INLINES` (Tier-4 grafted a named callee, no boundary call),
`SPECIALIZES` (per-site constant-arg specialization fired, §19.3), `REEMITS`
(defer-to-TU re-emitted a function with a forward-declared callee inlined),
`FOLDS` (the const-fold template fired), and `NOREPLAY` (the function must *not*
faithfully replay — proving the byte-verify net falls back to `-O0`). Lists are
comma-separated because a literal `;` would split the `add_test` COMMAND; `OUT`
is ensured to exist before the first compile because mcc won't create the output
dir. A promoted/inlined function's bytes diverge from `-O0`, so byte-verify is
bypassed and the run/exit-code equality is the gate.

The per-construct fixtures each prove one capture path replays faithfully and
still exits equal to the `-O0` build:

- `bitfield.c` — bit-field member read + write (the mask/shift runs inside the
  suppressed `gv`/`vstore`).
- `call_store.c` — storing a call result straight into a local (init and assign);
  regression guard for the `vpop` double-emit bug (the store's leftover rvalue
  must not be re-added as a bare BasicBlock effect).
- `complex_arith.c` — `_Complex` arithmetic + `__real__`/`__imag__` extraction
  (routes to `gen_complex_op`; the result temp is an ordinal frame slot).
- `complex_ctor.c` — `_Complex` construction from the imaginary unit `I`
  (`re + im*I`); the rodata `_Complex` const and the float→double widening const
  reuse their ELF symbols ordinally so relocations are byte-identical.
- `complex_imag.c` — an imaginary literal `2.0i` builds a `0 + val*i` pair folded
  into one `Unary(AST_OP_IMAG)`.
- `constfold.c` — the const-fold template collapses an all-constant return
  expression to a single `Literal` (byte-neutral: `gen_op` already folds these).
- `float_ops.c` — float/double constants, local stores, arithmetic, int↔fp casts,
  and a float comparison, including const-pool reuse.
- `goto_dispatch.c` — named forward/backward `goto`/labels via label markers and
  the replay-time label table.
- `inline.c` — the broad Tier-4 fixture: multi-statement straight-line bodies with
  relocating locals; internal control flow with a single tail return; early+tail
  returns coalescing through per-graft result slots (phi via memory); scalar-float
  params/return; recursive non-leaf grafts; self-contained `switch`/loop control
  flow; struct-by-value return and params (register-class `sumpt` with a negative
  uniform bias, memory/stack-passed `sumbig` with a positive per-param remap,
  `addpt` taking and returning a struct); scalar-arg wrappers so `main`'s own
  replay stays faithful; label scoping via the label floor; and two defer-to-TU
  cases (a plain forward-caller `fwd_sum` re-emitted with `fwd_callee` grafted,
  and a broadened one referencing an anon rodata string and a struct type that the
  value/type-ref pins keep alive to re-emit time). The final sum minus a constant
  yields 42.
- `inline_spec.c` — Tier-4 per-site specialization (§19.3): each `choose` call
  passes a constant flag bound to a read-only param, so the dead branch (which
  returns a different value than the live one) is eliminated; a runtime arg still
  binds via a slot (specialization is per-arg). The exec-golden equality is the
  real gate; `SPECIALIZES` proves the substitution fired.
- `ld_fallback.c` — `long double` is not modeled, so the function must fall back
  without faithfully replaying (proving the safety net).
- `promote.c` — Tier-3 promotion across straight-line code, a loop (the
  accumulator/loop-carried value live in registers while `i`, which uses `++`, is
  poisoned and stays in memory), pointer promotion (`p[i]` derefs the register
  value; exercises high-register SIB/`REX.B` encoding), a call-ful case (pins into
  callee-saved regs that survive the calls), and float promotion (`XMM6`/`XMM7`,
  call-free only).
- `short_circuit.c` — `&&`/`||` results used as values (stored / arithmetic) and
  nested operands (`(a&&b)||c`) as values, mixed globals, and branch conditions,
  plus a bare-global condition (`if (g)` whose leaf `->sym` is finalized so replay
  reconstructs it).
- `struct_byval_arg.c` — passing a struct by value as a call argument (both caller
  and by-value-param callee replay).
- `struct_copy.c` — struct assignment/copy, direct and through pointers, plus
  `(*p).x` member access on a dereferenced struct pointer.
- `struct_member.c` — scalar struct member access, `.` and `->`, read and write,
  via the coarse `Unary(MEMBER)` capture.
- `struct_ret_caller.c` — calling a register-return struct function and using the
  result (post-call register→temp reconstruction with an ordinal slot).
- `struct_ret_sret.c` — the sret hidden-pointer ABI (`ret_nregs==0`, a >16-byte
  struct): the caller allocates the result temp (ordinal slot) and passes its
  pointer; replay reserves the same slot.
- `struct_ret_variadic.c` — a variadic struct-returning call (the struct-return
  ABI is independent of varargs).
- `struct_return.c` — a struct-returning function (`return s`) replays; the caller
  side still falls back (its result-temp `loc` offset diverges on replay), so
  `REPLAYED` targets `make`, not `main`.
- `switch_dispatch.c` — switch dispatch (value + cases + default + fall-through +
  break, including a case range); captured as `If(op==6)` with markers, replay
  rebuilds the `switch_t` and reproduces `case_sort` + `gcase`.
- `vla.c` — VLAs (`int a[n]`), the lexical-scope-edge query: the size computation
  is an ordinary captured `Store`; the machine-tier alloc is one coarse
  `Unary(AST_OP_VLA)`, the paired SP restore a `Return` annotation (function-scope)
  or an `AST_OP_VLA_RESTORE` effect (nested block `}`); `loc` is not decremented at
  replay.

#### CST tests — `tests/cst/`

Driver scripts:

- `roundtrip.cmake` — the strongest pure-reflection proof (PLAN §8.1): compiles
  `SRC` with the CST self-check on and asserts the recorded tree reflects back to
  byte-identical source.
- `kinds.cmake` — node-kind coverage gate (D4): with the tree dump on, asserts
  every kind name in `KINDS` is produced (a coverage assertion, since round-trip
  alone passed while these kinds were reserved-but-unproduced); the dump prints
  internal nodes as `<Kind> [lo,hi)`, so the check anchors on the trailing
  space+bracket (so e.g. `Paren` does not match `ParamList`). It also re-checks
  the round-trip.
- `hashinv.cmake` — structural-hash invariance gate (slice C/G): `compact.c` and
  `spaced.c` share token structure but differ in whitespace/comments → equal root
  `H_s`; `changed.c` changes a token → different `H_s`.
- `increment.cmake` / `incstore.cmake` — the D3 hash-consed-`SourceFile` claims.
  `increment.h` re-includes itself several times under an incrementing,
  depth-gated macro (`#if (INCREMENTING) < 3`); every pass reads the same bytes so
  all collapse to ONE content-addressed template (a file's bytes fix its template
  regardless of include context), and the template is full-concrete (its `#if`
  depth-gate + successor table are `PPConditional` nodes) and renders back to its
  exact bytes. `incstore.cmake` is the live-capture form: `driver.c` pulls `leaf.h`
  in via `wrap.h` and directly twice; all references collapse to two templates
  (`wrap.h`, `leaf.h`), `leaf.h` deduped across three references, and both direct
  `#include "leaf.h"` nodes must bind to the same (non-`0xffffffff`) template id.
- `macro.cmake` / `macro-nesting.cmake` — slice-J macro fidelity: macro uses must
  become `CST_MacroInvocation` nodes AND the written source must round-trip
  byte-identically. `macro-nesting.cmake` pins the two accepted v1 imprecisions
  (see the fixture below); the byte-identical round-trip is the load-bearing
  invariant, and a future precise expander would change the node shape and require
  updating the assertions.
- `symref.cmake` / `symref-shadow.cmake` — slice-I symbol-ref resolution. The
  correctness driver asserts each use of `NAME` maps to a def whose spanned text
  is also `NAME`; the shadow driver pins the documented v1 boundary — a file-scope
  name shadowed inside a function resolves both uses to the same def
  (last-declaration-wins, no scope stack).

Fixtures: `fixtures/basic.c` (comments, operators, declarations for round-trip;
freestanding so it compiles with a bare `-c`), `fixtures/literals.c` (string/char/
number literals with escapes and adjacent-string concatenation),
`fixtures/preproc.c` (directives, inactive `#if` branches whose bytes must still be
reflected verbatim as inter-token source, macro invocations). `kinds/decl.c`
(D1b — Declaration/FunctionDef/ParamList/Enum/TypeName/Initializer/Label via
retroactive range-wrap), `kinds/expr.c` (D1a — Unary/Cast/Paren/Primary plus the
existing Binary/Call/Member/Index), `kinds/pp.c` (D1c — IncludeDirective/
PPDirective/PPConditional captured at the preprocessor boundary, never reaching
the post-expansion parser). `incstore/{driver.c,increment.h,increment_driver.c}`
and `symref/{refs.c,shadow.c}` are the fixtures for the drivers above.
`macro/macro_nesting.c` documents the two v1 imprecisions: (1) the trailing `)` of
a function-like invocation splits into a sibling `Paren` node (not inside the
`MacroInvocation` span), and (2) an object-like macro used inside another macro's
argument list stays a plain token (not its own `MacroInvocation`); the written
source still round-trips byte-identically. `macro/macros.c` and
`symref/refs.c`/`shadow.c` restate the same invariants briefly.

#### C99/C11 feature exec tests — `tests/exec/features_c99_c11/`

Each mirrors the runtime half of the corresponding gcc/clang conformance test and
prints `OK`:

- `alignas_over.c` — `_Alignas`/`_Alignof` over-alignment (C11 §6.7.5, §6.5.3.4):
  over-aligned objects get the requested runtime alignment, `alignas(alignof(
  max_align_t))` works, member/offset alignment holds, and a smaller-than-natural
  request is a no-op-min (never under-aligns).
- `complex_cmplx_special.c` — C11 §7.3.9.3 `CMPLX` + Annex G edges: `CMPLX(x,y)`
  builds a complex with parts exactly `x`,`y` even when a part is NaN/inf (unlike
  `x + y*I`); `cabs` of anything with an infinite part is `+inf`; `cproj` maps any
  infinite part to `(+inf, copysign(0,imag))`; `conj` flips the imaginary sign
  exactly. Checks are normalized to booleans.
- `feature_macros.c` — C11 §6.10.8 predefined feature-test macros, checked
  portably (complements `c11_freestanding_headers.c`): mandatory
  translation-environment macros (compared with `>=`) and the conditional
  `__STDC_NO_*` opt-out macros, each absence corroborated by exercising the
  feature (atomics/complex/threads/VLAs), so macro and capability can never
  disagree. Written to agree across mcc/gcc/clang and every hosted target (PE
  included; the exec/diff3 harness always builds hosted).
- `flexarray_runtime.c` — flexible array members (C11 §6.7.2.1p18): `sizeof`
  excludes the flexible member (`== offsetof`), heap allocation with a trailing
  array, read/write, char-FAM string storage, a typedef'd FAM. Includes
  `<stddef.h>` after the C library headers, which also regression-guards the
  ARM/AArch64 `__WCHAR_TYPE__` fix (this include order once triggered an
  incompatible `wchar_t` redefinition on the musl-arm sysroot).
- `flt_eval_method.c` — `FLT_EVAL_METHOD`, `float_t`/`double_t` (§5.2.4.2.2,
  §7.12p2): the macro is in `{-1,0,1,2}` and the widths match the reported method
  (only asserted for the well-defined methods; `-1` is indeterminate), with
  neighbouring `<float.h>` characteristics sane.
- `fp_wide_return.c` — C99 §5.1.2.3 / Annex F.6: a function returning
  `float`/`double` must remove extra range/precision (the returned value is the
  declared type, not a wider evaluation-format intermediate). `FLT_MAX + FLT_MAX`
  is finite in `long double` but overflows `float`, so the return must narrow to
  `+inf` regardless of evaluation format; a value whose float-rounded result
  differs from its `long double` value must be seen float-rounded by the caller,
  and re-narrowing is idempotent. On `FLT_EVAL_METHOD == 0` targets the narrowing
  is a no-op; on i386 x87 (`== 2`) it exercises real narrowing.
- `noreturn.c` — `_Noreturn` functions (C11 §6.7.4): a function that genuinely
  does not return (via `longjmp`), exercising both the `_Noreturn` keyword and the
  `<stdnoreturn.h>` `noreturn` macro, in both placements.

#### CLI cases, diff parts, and other drivers

- `tests/cli/cases.h` — a table of CLI test cases pinning several standard
  boundaries: a K&R identifier-list param defaulting to `int` (C99+ constraint
  violation vs. C89-valid — the pair pins the boundary, §6.9.1p6/§6.7.2p2); the
  `inline` external-definition matrix (a plain `inline` leaves the symbol
  undefined `nm 'U'`, an added `extern` exports it `'T'`) run over the exhaustive
  multi-unit `inline.c` so the otherwise reference-only exec golden actually
  executes; invalid universal-character-name rejections in identifiers (§6.4.3 —
  basic-latin range and surrogates), whose valid side lives in
  `exec/lexical/ucn_identifiers.c`; `signed`/`unsigned` mutual exclusion (§6.7.2p2,
  distinct from the "too many basic types" excess); and an x86_64 link range check
  — an absolute `R_X86_64_32S` reference past +2 GB must be rejected, forced with
  two ~1.5 GB `.bss` (`NOBITS`, so cheap) arrays and an absolute `movl` to `b`'s
  tail.
- `tests/diff/parts/legacy_aggregates.h`, `legacy_preproc.h` — K&R
  identifier-list definitions with *explicit* parameter declarations: still
  exercise the `FUNC_OLD` parser path but stay conforming under C99+ (implicit-int
  params were removed in C99).
- `tests/static/run_static.cmake` + `smoke.c` — the static-glibc smoke test:
  link `SRC` fully static with mcc, run it, and compare stdout. The `add_test` is
  only created when a static glibc `libc.a` is present, so a link failure here is a
  real defect, not a missing-toolchain skip. `smoke.c` exercises the three linker
  fixes that make `mcc -static` work under glibc: the weak-undef guard via
  GOTPCREL (`__libc_start_main`'s optional hooks, reached by any static startup
  that touches `printf`), the GOTTPOFF IE→LE TLS relaxation + local-exec TPOFF
  (glibc's internal `__thread` state, reached through `isdigit`/`toupper`'s ctype
  tables), and the `.tdata` TLS init image (must be copied, not zero-filled — the
  nonzero-initialized `__thread` objects read back their initializers). Output is
  normalized so it is identical across glibc and musl ctype encodings.
- `tests/tls/run_models.cmake` — drives the four x86-64 TLS models through mcc's
  linker: the reference compiler emits the object, mcc links it dynamically (and
  fully static when `STATIC=1`); a pattern-match abort ("unexpected
  R_X86_64_TLSGD pattern" etc.) or a wrong runtime value fails the test, pinning
  the tight codegen↔linker coupling.
