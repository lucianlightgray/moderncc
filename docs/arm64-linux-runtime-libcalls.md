---
name: arm64-linux-runtime-libcalls
description: "arm64 Linux atomics/bounds/complex CI failures were unresolved libgcc/compiler-rt libcalls, not weak-memory"
metadata: 
  node_type: memory
  type: project
  originSessionId: 0777336a-bfa5-439a-9368-a43684a7fa0b
---

The long-standing `ubuntu-24.04-arm` CI failures (atomics in all jobs, bounds when bcheck on, complex under clang `-O2`) were NOT a weak-memory/barrier bug — they were **"unresolved reference" link errors** for compiler-runtime helpers that mcc doesn't link. Fixed 2026-07-04 (commit 5241b402). Reproduced on real arm64 by running the repo's `tests/ci/docker` image under **Docker on Apple Silicon** (Docker runs `linux/arm64` containers natively → genuine aarch64 execution; `qemu-user` hid it because x86-TSO masks nothing here — the real cause was link-time, not runtime).

Two root causes, both because a **gcc/clang-built runtime** (`_use_gcc` = ON whenever `native AND (MCC_EMBED_MCCRT OR MCC_MCCRT_USE_HOSTCC)`; EMBED is ON by default on Linux) emits calls into libgcc/compiler-rt that mcc's link (libc only, no `-lgcc`) can't resolve:

1. **atomics + bounds** — gcc/clang default to `-moutline-atomics` on aarch64, lowering atomic ops to libgcc's `__aarch64_{cas,swp,ldadd,...}_{acq,rel,acq_rel}` outline helpers. `runtime/lib/atomic.c` (and `bcheck.c`, which calls `atomic_fetch_add` on its `no_checking`/`never_fatal` ints — hence the bounds suite failing by the *same* cause) then reference them → unresolved. Fix: append `-mno-outline-atomics` to the gcc-built-runtime flags (`_gccflags`, CMakeLists §~2300) when `MCC_CPU STREQUAL arm64`, guarded by `check_c_compiler_flag`. gcc then emits inline LL/SC (`ldxr`/`stxr`) — self-contained.

2. **complex under clang `-O2`** — clang (not gcc) emits `__unordtf2` (128-bit `long double` unordered/NaN compare; aarch64 has no hardware binary128, so long double is soft-float) for the NaN checks in the complex routines. mcc's `runtime/lib/lib-arm64.c` provided the *ordered* tf comparisons (`__eqtf2`/`__lttf2`/`__getf2`/…) but never `__unordtf2`, because **mcc's own arm64 codegen only emits the ordered ones** (`arm64-gen.c` maps `<`/`==`/… to those TOK___*tf2). Added `int __unordtf2(long double a, long double b){ return f3_cmp(a,b) == 2; }` — `f3_cmp` already returns sentinel `2` for the unordered case.

**Why macOS/arm64 always passed** (so it only showed on Linux CI): on Darwin, EMBED is force-disabled (the sidecar-libmccrt fallback), so the runtime is **mcc-built**, and mcc lowers atomics to its own `__atomic_*_N` helpers (no `__aarch64_*`) and never emits `__unordtf2`. See [[macos-arm64-status]].

Result: `linux-gcc` and `linux-clang-release` both 808/808 on native arm64 (were failing). General lesson: an mcc runtime object built by a *foreign* compiler (EMBED/USE_HOSTCC) can pull in that compiler's arch runtime-lib ABI (`__aarch64_*`, `__*tf*`, etc.); either suppress it at build (`-mno-outline-atomics`) or provide the helper in `runtime/lib/lib-<arch>.c`.
