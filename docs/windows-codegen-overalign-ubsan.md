---
name: windows-codegen-overalign-ubsan
description: x86_64-PE auto over-alignment + trap-mode UBSan enabled; the win64 alloca shadow-space gotcha
metadata: 
  node_type: memory
  type: project
  originSessionId: 0b4b1eeb-858b-4f08-9c63-5f0bba0293be
---

Two x86_64-Windows/PE codegen features enabled (commit `b4c8e3c7`), both
byte-neutral on ELF (guarded on `MCC_TARGET_PE`):

**Auto stack over-alignment** (`_Alignas(32+)` locals) on x86_64-PE. Was gated off
(`STACK_OVERALIGN_MAX` defined only for `x86_64 && !PE`, `mccgen.c`). The over-align
path reuses `gen_vla_alloc`, whose PE branch calls `alloca`.

**KEY GOTCHA — win64 `alloca` shadow-space ABI** (`runtime/lib/alloca.c`): win64
alloca 16-aligns and returns the block in **RAX = RSP + 32**, i.e. ABOVE a 32-byte
shadow space it reserves below, so the caller's *next* `call` can home its register
args at [rsp,rsp+32) without corrupting the alloca'd block. So RSP ≠ the block on
Windows (unlike SysV/ELF where the block is at RSP). Consequence: over-aligning by
masking RSP (`and rsp,-align`) puts the object below the shadow, and the next
call/VLA corrupts it. Correct fix: round **RAX** (with `align` extra bytes of
headroom) — `add rax,align-1; and rax,-align` — leaving rsp/shadow intact, and save
the rounded RAX via `gen_vla_result` (not `gen_vla_sp_save`). The plain-VLA path
(`mccgen.c` `#if PE && x86_64`) already did this RAX/RSP split; over-align now
mirrors it. Test: `tests/exec/features_c99_c11/alignas_over.c`, ungated for
`__x86_64__` on `_WIN32`. i386/arm64-PE had `STACK_OVERALIGN_MAX` pre-defined but
are still unvalidated on Windows (test stays x86_64-gated there).

**Trap-mode UBSan** (`-fsanitize=undefined`) on x86_64-PE — `ubsan-suite` 11/11.
Trap mode is pure `ud2`, no runtime handler, so it just works on PE: the trap
crashes the process with `EXCEPTION_ILLEGAL_INSTRUCTION` (0xC000001D) as SIGILL on
ELF — no SEH handler / `.pdata` needed (the trap is meant to abort). Dropped
`!defined MCC_TARGET_PE` from the `do_sanitize_undefined` gate (`libmcc.c`), removed
the CMake `NOT WIN32` on `ubsan-suite`, and taught `run_ubsan.cmake` that a fired
trap = "not a clean 0..127 exit" (Windows reports the crash code negative/>2^31, not
128+signo).

See [[moderncc-windows-build]]. Pre-existing Windows ctest failures unrelated to
this: `run_atexit`/`errors_and_warnings`/`sanitize_address`/`config-defines`/
`cross-factory`.
