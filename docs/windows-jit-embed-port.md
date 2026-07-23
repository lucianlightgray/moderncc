---
name: windows-jit-embed-port
description: "Windows JIT-embed port — DONE; runtime JIT works on PE, 32/32 jit selftests green"
metadata: 
  node_type: memory
  type: project
  originSessionId: 0b4b1eeb-858b-4f08-9c63-5f0bba0293be
---

`MCC_EMBED_JIT` (runtime-recompile JIT, `src/mccjit_embed.c`) is **fully ported to
the PE target and ungated** — `ctest -R jit/` = **32/32** on the mingw PE host.
Three layers, all landed this session (POSIX/ELF paths byte-unchanged):

**(1) OS primitives — `src/mccjit_win32.h`** (included under `#if MCC_HOST_WIN32`):
`mmap`/`munmap`/`msync`→`VirtualAlloc(PAGE_EXECUTE_READWRITE)` + a base→handle
registry so one munmap dispatches file-view vs VirtualAlloc; KGC `MAP_SHARED`
fd→`CreateFileMapping`/`MapViewOfFile`; `pthread_*`→SRWLOCK+CONDITION_VARIABLE+
`InitOnceExecuteOnce`+`_beginthreadex` (no winpthreads); `open`/`pread`/`ftruncate`/
`mkstemp`/`setenv`/`clock_gettime`(QPC)/`nanosleep`(spin-yield for <2ms, since
`Sleep(1)`≈15ms); MSVC `__atomic_*`→Interlocked; `_M_X64`/`_M_ARM64`→internal
`MCCJIT_X64`/`MCCJIT_ARM64` so MSVC-x64 also JITs. Selftests marked `PUB_FUNC`
(dllexport) so libmcc.dll exports them. fork selftest + atfork `#if !MCC_HOST_WIN32`.

**(2) Hand-written stubs → Microsoft x64 ABI.** The stubs were SysV-hardcoded
(pushed rdi/rsi/rdx/rcx/r8/r9, called the C helper arg0-in-rdi, no shadow space)
and segfaulted/hung under Win64. Ported (`#if MCC_HOST_WIN32` branches):
`mccjit_make_counter_stub` spills rcx/rdx/r8/r9 into the 6-slot regs array the
capture expects (stack args zeroed) + 32-byte shadow; `mccjit_make_kgc_stub_n`
spills rcx/rdx/r8/r9 (+caller stack, `movsxd` for narrow) and passes calln's
5th/6th args on the stack; `mccjit_make_kgc_stub_fp` same with xmm0-3 `movsd`.
Profile fixtures widened `long`→`long long` (LLP64 `long`=32-bit left the captured
register's upper bits caller-defined). **`mccjit_make_kgc_stub_mixed` is the one
deferral** — its thunk rebuilds a SysV call *by class* (gpv→rdi.., fpv→xmm..) but
Win64 is *positional* and needs a per-arg class vector; on WIN32 it returns NULL
(mixed sigs fall back to baseline, unmemoized) and `jit/selftest-mixed` skips.

**(3) `-run`/`--embed-jit` auto-JIT pipeline on PE.** Two PE-only gaps kept the
baked JIT ctor from firing: (a) `runtime/lib/runmain.c` had `run_ctors`/`_runmain`
`#ifndef _WIN32` → `mcc -run` never ran `.init_array` ctors on Windows (the PE
linker *does* synthesize `__init_array_start/end`, per `runtime/win32/lib/crtinit.c`)
— ungated; (b) `mccjit_embed_finalize` (emits the registry + `__mccjit_boot_all`
ctor) was only called from the ELF/Mach-O `mcc_add_runtime`, never PE's
`pe_add_runtime` (`src/objfmt/mccpe.c`) — added there. Plus the ctor's
`getenv("MCC_JIT")` is bound as a host symbol for PE in-memory relocates.

**Gotcha:** `tools/hostgate.c` bans raw host-OS macros (`_WIN32`,`_MSC_VER`,
`__MINGW32__`…) in `#if` outside `src/mcchost.{h,c}` — use **`MCC_HOST_WIN32`**
(always-defined 0/1) or `host_*`. Arch macros (`__x86_64__`,`_M_X64`) are fine.
`tools/targetgate.c` bans `MCC_TARGET_*` outside `src/arch/`. Both are ctest
invariants (`host-gate-invariant`/`target-gate-invariant`).

Still off on WIN32: the embed-blob (`--embed-jit` standalone exe) — mcc's ELF-only
linker can't consume the host CC's COFF `libmcc_jitengine.a` (`CMakeLists.txt`
~1951 guards it off). Separately found: the CLI `--jit-functions=f` stores `"=f"`
(the `=` isn't stripped) — a real CLI-parse bug (the API path is fine). Pre-existing
Windows ctest failures unrelated to this work: `run_atexit`/`errors_and_warnings`
(env), `sanitize_address` (no libasan), `config-defines`, `cross-factory`
(`ast_alloc_loc` amalgamation gating — fails identically on clean upstream).

See root `TODO.md` "Windows JIT-embed port" + [[moderncc-windows-build]] +
[[jit-dispatcher-x86-only-guard]].
