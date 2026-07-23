---
name: mcctest-ref-crash-skip
description: mcctest now SKIPs (not fails) when the reference cc program crashes; the i686 crash is CI-host-msvcrt-specific
metadata: 
  node_type: memory
  type: project
  originSessionId: 68d371eb-5776-4408-98f4-2edd963fc4b6
---

The `mingw / i686` CI cell failed `mcctest` with the reference program (built
from `tests/diff/full_language.c` by the reference gcc against legacy msvcrt)
exiting `0xC0000005` (access violation), all 4 `run_to_retry` attempts.

**Fix (landed on main, commit after `a7bf197e`):** `suite_mcctest` in
`tools/mccharness.c` returns **77** (CTest skip) instead of 1 when the *reference*
program crashes persistently; `set_tests_properties(mcctest/-bcheck ... SKIP_RETURN_CODE 77)`
in CMakeLists.txt. Rationale: `refexe` is produced solely by the reference cc —
mcc never touches it — so a crash there is never an mcc regression; with the
oracle broken the only sound outcome is skip. The **mcc-program** crash path
stays a real failure (`return 1`).

**Why:** blaming mcc for a broken reference is wrong; a deterministic-crash of a
binary mcc didn't build should not fail mcc's CI.

**How to apply / root cause proven this session:** the crash is **specific to the
CI runner's Windows Server 2025 `msvcrt.dll`**, NOT codegen or the corpus. On this
Win10 host the identical mingw-gcc i686 build runs clean (exit 0, deterministic
26659 B, 5/5). Rebuilding the same corpus with a *second* codegen (the
`mingw-mstorsjo-llvm-msvcrt` clang, see [[windows-reference-toolchains]]) also runs
clean (exit 0, 26673 B — 14 B gcc/clang formatting delta, not a fault). Two codegens
agreeing it runs here isolates the fault to the Server-2025 msvcrt version. It is
therefore **not reproducible on this host, nor in a Linux Docker container** (wine
ships its own msvcrt; no Server-2025 Windows-container base on a Win10 host). Don't
re-hunt this as an mcc bug. Related: [[mcctest-win32-msvcrt-i386-excess-precision]],
[[mcctest-ast-promote-debug]], [[ci-log-zip-commit-ahead]].
