---
name: windows-reference-toolchains
description: Portable MSVC + llvm-msvcrt reference toolchains installed on this Windows host for differential/PE debugging
metadata: 
  node_type: memory
  type: reference
  originSessionId: 68d371eb-5776-4408-98f4-2edd963fc4b6
---

Two reference C toolchains installed on this host (2026-07-13) for reproducing
the mcc PE/msvcrt differential path locally, beyond CLion's mingw gcc and the
vendored winlibs gcc:

**Portable MSVC** (standalone, no Visual Studio) — `C:\Users\llg\portable-msvc\msvc\`.
Installed via mmozeiko's `portable-msvc.py` (canonical "portable msvc"; pulls only
from official `aka.ms/vs/*` channels; `--accept-license --host x64 --target x86,x64`;
667 MB). Layout: `VC\Tools\MSVC\14.51.36231\bin\Hostx64\{x64,x86}\cl.exe` + `Windows Kits\`.
Env is set by `setup_x64.bat` / `setup_x86.bat` (in the `msvc\` dir). The **x86 cl
is Version 19.51.36248 — byte-identical toolset to the CI runner** (CI logs "MSVC
19.51.36248.0"). Note `cl` targets **UCRT**, not legacy `msvcrt.dll`. Gotcha: this
host has `NoDefaultCurrentDirectoryInExePath` set, so cmd won't find `setup_x86.bat`
or a freshly-built `foo.exe` in cwd — call them as `.\setup_x86.bat` / `.\foo.exe`.
The already-installed VS 2026 (`cl 19.51`) is the same toolset; this portable copy
is a VS-independent alternative.

**llvm-mingw targeting msvcrt** — scoop `mingw-mstorsjo-llvm-msvcrt` (22.1.8-20260616,
clang 22), at `C:\Users\llg\scoop\apps\mingw-mstorsjo-llvm-msvcrt\current\bin`.
Provides `i686-w64-mingw32-clang.exe` (+ x86_64/arm64), Target `i686-w64-windows-gnu`,
linking legacy **msvcrt** — a SECOND i686→msvcrt codegen to diff against mingw gcc.
To build the `tests/diff/full_language.c` corpus with it under mcc's msvcrt refflags,
four deltas from the gcc refflags were needed (lld is stricter than bfd ld):
- link the **32-bit** msvcrt: `c:/windows/syswow64/msvcrt.dll` (system32's is 64-bit;
  bfd ld imports a 64-bit dll by name into a 32-bit exe, lld rejects the machine
  mismatch);
- drop `-lgcc`, add compiler-rt builtins: `lib/clang/22/lib/windows/libclang_rt.builtins-i386.a`
  (for `__muldc3`/`__divdc3`/VLA chkstk);
- `-Wl,--allow-multiple-definition` (msvcrt_start.c stubs `__chkstk`, compiler-rt also defines it);
- `-Wno-error=int-conversion` (clang 22 promotes it to a default error; the harness `-w` doesn't cover errors).

**llvm-mingw targeting UCRT** — scoop `mingw-mstorsjo-llvm-ucrt` (22.1.8-20260616,
clang 22), at `C:\Users\llg\scoop\apps\mingw-mstorsjo-llvm-ucrt\current\bin`
(added 2026-07-21). Same `<arch>-w64-mingw32-clang.exe` layout but links **UCRT**
(`api-ms-win-crt-*.dll`) — the modern-CRT counterpart to the msvcrt variant above,
for msvcrt-vs-UCRT differential tests.

**niXman mingw (UCRT gcc)** — scoop `mingw` (16.1.0-rt_v14-rev1) at
`C:\Users\llg\scoop\apps\mingw\current\bin\gcc.exe`; despite the plain name it is a
**UCRT** build (`x86_64-...-ucrt-rt_v14`). So the full local CRT×compiler matrix is:
legacy-msvcrt {mstorsjo-clang, mstorsjo-gcc}, UCRT {mstorsjo-clang, niXman-gcc,
vendored-winlibs-gcc 16.1.0}, plus mcc (links raw `msvcrt.dll`).

Used 2026-07-21 to disambiguate the `diff3/licm`+`diff3/cse` mingw-CI "regression":
built mcc from the suspect commit with BOTH CLion gcc 15.2.0 and the vendored winlibs
gcc 16.1.0, ran `licm.c`/`cse.c` across the whole CRT×compiler matrix — **all agree
on the correct value** (`licm chk=2286992590266 g=5`, `cse chk=5411311812928774202
g=22`). So mcc's Win64 codegen is correct AND the `%llu`/msvcrt-truncation theory is
false: modern `msvcrt.dll` prints `%llu` >2³² correctly (verified msvcrt binaries
import raw `msvcrt.dll` via objdump). **RESOLVED — there was no regression:** the
"red between #1044/#1052" premise was mis-recorded (via `gh`: no red run ever
existed; the SELECT/ABS-flip commits `c37316b4`→`228713a9` were pushed after the
green #1044/`08d0c667` but never got a completed CI run — `cancel-in-progress` on
main + multi-commit push only runs the tip). Dispatched CI run **#1057** (the first
real test of the flip on the live mingw runner): fully GREEN — `diff3/cse` +
`diff3/licm` **Passed**, 100% of 5484, all Windows cells green. See docs/TODO.md
`[RESOLVED]` bullet + [[ci-log-zip-commit-ahead]].

Practical `gh` notes for this repo: token via `GH_TOKEN` env (gh installed via
scoop, on `~/scoop/shims`); `ci.yml` triggers on every push+`workflow_dispatch`
with `concurrency: ci-<ref>` + `cancel-in-progress` on main, so a docs push
re-runs the whole matrix and a rapid multi-commit push only CI-tests the tip.
`gh workflow run ci.yml --ref main` dispatches; the slow cell is the `mingw`
superbuild (`mingw / x86_64`, ~6-8 min queue+run). Job logs (`gh run view --job
<id> --log`) are only served AFTER the whole run completes, not per-job mid-run.

See [[mcctest-ref-crash-skip]] for what these were used to prove, and
[[mcctest-win32-msvcrt-i386-excess-precision]] / [[moderncc-windows-build]].
