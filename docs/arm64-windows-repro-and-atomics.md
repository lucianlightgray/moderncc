---
name: arm64-windows-repro-and-atomics
description: "How to reproduce/debug the arm64-Windows (msvc/arm64 CI) target in Docker, and why emulation can't catch its hangs"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 6890e596-20eb-411d-b48f-8fb508d5da0b
---

**UPDATE 2026-07-20 — wine-arm64 in Docker DOES boot + run mcc arm64-PE exes
(corrects the "wineboot c0000135 can't bootstrap a PE prefix" claim in
[[arm64-msvc-replay-miscompile]]).** Confirmed on this x86_64 host: `arm64v8/
ubuntu:24.04` under Docker qemu + `apt install wine wine64` (wine 9.0, aarch64-
windows PE backend). `wineboot -i` returns rc=0 and DOES create a working prefix;
the `c0000135` it prints is ONLY the ARM32/WoW `syswow64/sysarm32 rundll32.exe`
stub — **non-fatal**, the native ARM64 `system32` loader runs fine. A `hello.exe`
(machine 0xaa64) prints and exits 0. So wine-arm64 is a usable **codegen/logic**
validation path (NOT weak-memory/wild-jump — qemu x86-TSO still masks those; a wild
jump still spins-not-faults). Working recipe: build `mcc-arm64-win32` + `arm64-win32-
mccrt` via gcc-cross on an **amd64** ubuntu container (`-DMCC_ENABLE_CROSS=ON
-DMCC_ONE_SOURCE=ON`); stage sysroot `/sysroot/win32/{include,lib}` = `runtime/
win32/{include,lib}` + the runtime `.a` as BOTH `libmccrt.a` and `arm64-win32-
libmccrt.a`, and `/sysroot/include` = `runtime/include`; compile `mcc-arm64-win32
-B /sysroot/win32 -isystem /sysroot/include foo.c -o foo.exe` (the `-isystem` is
required or `stddef.h not found`); run in the arm64 container `WINEDEBUG=-all
timeout 90 wine foo.exe`. Bake `wineboot -i` into the image (cached); `wineserver -w`
never returns under emulation (harmless). Validated the landed arm64-PE over-align
change dynamically: `alignas_over.exe` → `OK` rc=0 (all `_Alignas` 16..256 stack
checks). **This recipe is now a committed script: `tools/arm64pe-wine-docker.sh`**
(2026-07-21, debian-bookworm wine-8.0 arm64 image, cached; args = the cross mcc +
a staged win32 sysroot; exits 0/1/77). It also validates the mode-6 dispatch-slot
frameless-leaf return path (`mccjit_patch_make_slot` stub) — ran correct under
wine, confirming NO mcc codegen bug in the arm64-PE runtime-JIT reproducible
portion; only the native-fault subset (RtlAddFunctionTable unwind, icache
coherence, wild-jump) needs real arm64-Windows HW. Gotcha: a single `printf` with MULTIPLE `%f` args hangs under wine/msvcrt
emulation (a wine quirk, NOT mcc — fp math + rodata addressing verified correct
otherwise). Implication: the still-open msvc/arm64 exec-replay root-cause MAY be
partially reproducible under wine after all (build the gcc-cross mcc, run its
emitted replay exes) — but the fault mode was "wild jump, spins-under-wine" so it
can still be masked; worth a retry given wine boots.

Debugging the `msvc / arm64` CI job (Windows-on-ARM, the only target with no local
machine here) via Docker on this x86_64 Windows host:

- mcc is a cross-compiler; `arm64-win32` is a real target (CMakeLists `MCC_X` list).
  Build the `cross` preset (`-DMCC_ENABLE_CROSS=ON`) on a plain **amd64** Linux
  container → get `mcc-arm64-win32` (reports "AArch64 Windows"). Its arm64 PE
  **codegen is identical** to what the CI arm64 box emits, so you can reproduce the
  exact machine code without arm64 hardware.
- The win32 header/def tree (`<mccdir>/win32/{include,lib}`) is only laid out by a
  **native Windows** install; on a Linux cross-build you must stage it by hand from
  `runtime/win32/{include,lib}` + the built `arm64-win32-libmccrt.a`, or the linker
  says `arm64-win32-libmccrt.a not found` / `pthread.h not found`.
- To *run* the arm64 PE: `--platform linux/arm64` container (qemu-emulated) + `apt
  install wine` → `wine foo.exe`. Enable arm64 emulation once with
  `docker run --privileged --rm tonistiigi/binfmt --install arm64`.
- The exec harness (`tests/exec/runner.c`) compiles-to-exe and runs natively at
  **default opt** (NOT `-run`/JIT, NOT `-O2` — those are mccharness subcommands).

CRITICAL CAVEAT: qemu-user on an x86 host gives the guest **strong x86-TSO ordering**,
so it **masks arm64 weak-memory bugs** (missing barriers) AND serializes threads, so it
masks true-parallelism deadlocks. A test that hangs on real arm64 silicon can pass under
wine/qemu here. So this setup reproduces **codegen/logic** bugs but NOT weak-memory or
real-parallelism bugs. Confirmed: all 4 threading tests (`c11_threads`, `atomic_counter`,
`atomic_inlang_rmw`, `tls`) pass under wine-arm64 despite the CI hang.

exec/tls hang root cause (msvc/arm64 CI, investigated 2026-07-05): the hung test is
`exec/tls` (#89) — static `__thread` access. Proven via a `ctest --timeout 300` patch
that names the stuck test. Root cause: **MSVC-on-arm64 miscompiles mcc itself.** The
mcc source, built by gcc (x86_64 host OR arm64 host, native), emits a complete correct
541-line `tls.s`; the same source built by MSVC-arm64 emits corrupt output (drops
`main`/`thread_func`, truncates `__mcc_once_tramp` to its prologue) → the linked exe
hangs. So it is NOT mcc's arm64 codegen (provably correct) and NOT host-arch UB (arm64
gcc build is fine) — it is an MSVC-arm64 codegen bug hit only on the static-`__thread`
emission path (only `tls` uses `__thread`; `c11_threads` uses `tss_`/TlsAlloc, atomics
use neither, so they pass). To reproduce the native build for diffing without arm64
Windows: build `mcc-arm64-win32` in an arm64 Linux container (stream source via
`docker exec mccarm tar ... | docker exec -i mccwine tar -x`; needs the `include/` dir
too) and diff its `-S tls.c` against the CI's native dump. Fix path: re-gate `exec/tls`
on arm64+WIN32 (stopgap) and/or find the MSVC-miscompiled construct (CI-bisect with
reduced /O on the arm64 msvc build).

Further isolation (conclusive): the bug is INTERMITTENT — the *same* MSVC-arm64 mcc
binary emits correct tls.s most runs, corrupt on some — so verdict on `ret`==6 + all
4 labels and measure a corruption RATE over N runs, not one dump. Only MSVC's *arm64
code generator* building mcc fails. Ruled out (deterministic/clean over 30-50x): gcc
on x64 AND arm64 Linux; MSVC-**x64** building mcc cross-targeting arm64-win32 (0/30 —
built locally with CLion's cmake + the default VS generator: `cmake -S <repo> -B
<build> -DMCC_TOOLCHAIN_PROFILE=msvc -DMCC_ENABLE_CROSS=ON` then `--build --config
Release --target mcc-arm64-win32`; no vcvars/ninja needed, cygwin ninja fails 0xc0135);
MSVC-x64 for x64-win32. No source UB: Valgrind clean AND `-ftrivial-auto-var-init=
pattern` clean. So not locally fixable/bisectable — needs an arm64 Windows+MSVC box.
RESOLVED 2026-07-05 (commit d7755718): `exec/tls` skipped on arm64+WIN32 via a new
`skipon=<cpu>/<os>[:reason]` runner gate (excludes one platform, runs elsewhere) —
see tests/exec/runner.c + goldens.h + docs/TODO.md. Still runs+passes on x86_64
WIN32 and all gcc/clang arm64 targets. The `msvc` ctest step keeps `--timeout 300`
as a backstop. The temporary O2/Od probe + MCC_NOOPT knob were removed.

Atomics finding (barrier audit): mcc does **not inline** atomics on arm64 — every op
lowers to a `bl __atomic_*` / `bl atomic_flag_*` library call, **identical** between the
ELF and PE targets. `runtime/lib/atomic.c` aarch64 code (release `stlrb`, acquire `ldar`,
RMW `ldaxr`/`stlxr`) is byte-identical ELF-vs-PE (only benign GOT-vs-direct symbol
addressing differs, in the unused `__mcc_atomic_lock_for` fallback). The aarch64 atomic
lib is already green on real arm64 Linux hardware. So an arm64 hang is **not** an atomic
memory-barrier codegen bug. See [[moderncc-windows-build]].
