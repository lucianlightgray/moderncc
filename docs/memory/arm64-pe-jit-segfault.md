---
name: arm64-pe-jit-segfault
description: "msvc/arm64 CI: 7 jit/selftest-* SEGFAULT. CONFIRMED = MSVC-arm64 optimizer miscompiling mcc/libmcc (same root cause as exec-replay); fixed by /Od for MSVC+arm64. See arm64-msvc-replay-miscompile."
metadata:
  node_type: memory
  type: project
  originSessionId: 12624ee5-ae46-4b17-946a-780c2e47e2f7
---

**CONFIRMED correct on native arm64 2026-07-19 (Docker):** all 7 of these JIT
selftests PASS when mcc is built natively in a linux/arm64 container (gcc, ELF,
MCC_EMBED_JIT ON) — part of a full 5218-test native run with ZERO mcc failures (the
sole non-pass is a cmake-3.28 skip artifact; the 16 grep fails were my own CRLF-
staging bug, all pass after LF-normalizing the tree). So the arm64
runtime-JIT logic is sound; the msvc/arm64 SEGFAULTs are MSVC miscompiling mcc's JIT
engine OR arm64-PE-native-only paths. Full analysis: [[arm64-msvc-replay-miscompile]].

**NOT RESOLVED (correction 2026-07-18): /Od DID NOT FIX THIS.** Run `80205187949`
@ `a7c38f1a` (with /Od applied — verified `<Optimization>Disabled` in the vcxproj)
still SEGFAULTs the same 7 jit selftests. At /Od there is no optimizer to blame →
the "MSVC optimizer" theory is disproven; the wine "/Od validation" was a false
positive (wine masks the native fault). See [[arm64-msvc-replay-miscompile]] update
for the full analysis + options (skip-gate on arm64/WIN32). Original notes below.

--- ORIGINAL (optimizer theory — disproven) ---

The claim below that this is "the same root cause as [[arm64-msvc-replay-miscompile]]"
(MSVC's ARM64 `/O1`//`/O2` optimizer miscompiles mcc/libmcc), fixed by forcing `/Od`
for MSVC+arm64 in CMakeLists — is DISPROVEN (see above). Directly reproduced: a `jit_selftest.exe` built with
`cl 19.51.36248 for ARM64` at **/O2** (libmcc.c+jit_selftest.c, +MCC_EMBED_JIT) run
under wine-arm64 prints "stashed leaf-int intent = 442 bytes" then hangs (= the CI
segfault) / errors "initializer overflow"; at **/Od** libmcc is correct. The
investigation notes below (arm64 JIT logic proven correct on Linux; clang-PE repro
was a dead end) still stand — they're what ruled OUT an mcc bug before the MSVC
optimizer was pinned. Original notes:

---


CI run 80010350041 (commit `cdb1b45e`): the `msvc/arm64` + `sanitize-msvc/arm64`
jobs SEGFAULT 7 JIT selftests — **leaf-int, kgc, purity, struct, liverun, poison,
fparg** (out of ~30). The passing set is largely decision-logic tests (lazy, pool,
eligibility, observability, evalgate, bench, fork…); the failing set is exactly the
ones that **JIT-compile a function and CALL it** (leaf-int/liverun = direct recompile
+call; kgc/purity/struct/poison/fparg = KGC verify-stubs). Companion to
[[arm64-msvc-replay-miscompile]] (the other 22 failures on the same jobs).

`jit_selftest*` are HOST programs: `libmcc` (built by the CI's MSVC-arm64) linked with
the test's `main`, doing runtime JIT natively on the arm64-Windows runner.

**arm64 JIT *logic* is correct** — built mcc natively on arm64-Linux (gcc, in an
arm64 ubuntu container under qemu) with MCC_CONFIG_OPTIMIZER+MCC_EMBED_JIT and ran the
selftests: **leaf-int, kgc, purity, struct, poison all PASS** (matches the documented
30/30 on arm64 macOS/Linux). So the recompile/stub/ABI logic is sound on arm64 (AAPCS64
is effectively identical Linux-vs-Windows for these scalar cases; icache flush +
RtlAddFunctionTable unwind reg are both present in the arm64-PE MEMORY path —
`mcchost.c host_runmem_protect`/`host_icache_flush` (FlushInstructionCache) and
`mccrun.c` copy==3).

**Could NOT build a valid correct-compiler arm64-PE repro.** Cross-built libmcc+
jit_selftest to arm64 PE with llvm-mingw clang 21 (`aarch64-w64-mingw32-clang`, has a
real windows.h) and ran under wine-arm64: it HANGS — but the hang is in
`preprocess_start` for **every** compile (even `mcc.exe -c trivial.c` hangs; `mcc -v`
is fine). Since CI's MSVC-built `mcc` compiles thousands of files fine on arm64-Windows,
**this hang is an artifact of my clang build** (a clang-21 miscompile or a
missing-MCC_CONFIG-define issue in the hand-rolled compile line), NOT the CI bug — so
this repro path is a dead end for the JIT segfault. (Repro recipe still useful: see
[[arm64-windows-repro-and-atomics]] + [[arm64-msvc-replay-miscompile]] for the
gcc-cross + wine-arm64 harness that DID work for the codegen/replay class.)

**Best inference:** since (a) arm64 JIT logic is proven correct on Linux, (b) the ABI/
icache/unwind paths for arm64-PE are present and correct, and (c) the sibling
exec-replay failures on the SAME jobs are a proven MSVC-arm64 miscompile of mcc, the
7 JIT segfaults are most likely **the same MSVC-arm64 miscompile class** — MSVC-arm64
mis-emitting mcc's JIT engine (stub byte-emit / intent-serialize / recompile) code,
faulting when the freshly-generated function is called. Not confirmed on hardware.

**Fix options (unverifiable without an arm64-Windows+MSVC box):** (a) skip-gate the 7
selftests on arm64+WIN32-MSVC (matches the `exec/tls` precedent); (b) build mcc.exe FOR
arm64 with the local portable MSVC arm64 cross tools (see [[windows-reference-toolchains]])
+ run under wine-arm64 to get the real repro, then bisect the miscompiled construct.
The `/O` bisect + reduced-opt probe is the tls playbook. [[always-commit-and-push-after-green]]
