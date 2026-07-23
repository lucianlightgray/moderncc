---
name: moderncc-windows-build
description: How to build and run the moderncc (mcc) test suite on this Windows host
metadata: 
  node_type: memory
  type: project
  originSessionId: 12bd2d8f-e136-4d9c-86c5-02aa3b2aea1f
---

Building/testing `moderncc` (the `mcc` compiler) on this Windows box uses the
**CLion-bundled mingw + cmake + ninja** (no gcc/cmake on PATH by default). In a
PowerShell session, prepend them to PATH:

```
$clion = "C:\Users\llg\scoop\persist\jetbrains-toolbox\apps\CLion\bin"
$env:Path = "$clion\mingw\bin;$clion\cmake\win\x64\bin;$clion\ninja\win\x64;" + $env:Path
```

Then configure into a fresh dir (the pre-existing `cmake-build-debug` has a
stale CLion absolute path that breaks `--build`):

```
cmake -S <src> -B <src>/build-win -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER="$clion/mingw/bin/gcc.exe"
cmake --build <src>/build-win -j
ctest --test-dir <src>/build-win -j 8
```

Key facts:
- On `_WIN32`, mcc defaults to the **PE target** (MSVC ABI): `long` is 4 bytes
  (LLP64) and `long double == double`. It still emits **ELF** relocatable
  objects (only the final exe/DLL is PE).
- Runtime headers (`runtime/include`, `runtime/win32/include`) and `.def` import
  libs are **copied into the build tree at configure time**, so editing one
  needs a reconfigure (`cmake build-win`) to take effect. `mccdefs.h` is the
  exception — it's embedded via `c2str` as a real build dependency.
- Goldens skip-gating lives in `tests/exec/goldens.h` (7th `req` field). The
  runner supports `os!=NAME[:reason]` to skip on a given target OS.
- `cli-suite` is skipped on Windows: its case commands are POSIX shell pipelines
  (`printf`/`grep`/`sed`) and `popen` routes through cmd.exe there.

Three build flavors now work on this host:
- **mingw (default)**: as above, `-G Ninja` + CLion gcc. 33/33 ctest green.
- **MSVC**: VS Community 2026 (v18) is installed; cl 19.51. Use the **Visual Studio
  generator** (finds cl itself, no vcvars; the Ninja+cl combo needs a vcvars env and
  hits a CONFIGURE_DEPENDS glob bug). `cmake -S <src> -B <bld> -G "Visual Studio 18
  2026" -A x64 -DMCC_TOOLCHAIN_PROFILE=msvc` then `cmake --build <bld> --config Debug`
  and `ctest --test-dir <bld> -C Debug`. The `msvc` profile builds with ONE_SOURCE=OFF.
  Now **33/33 with only 9 skips** (was 12): `mcctest`/`mcctest-bcheck`/`cli-suite`
  are enabled on Windows — see below.

**Enabling the gcc/sh-dependent suites on a Windows host** (2026-06-30): three
tests that used to skip now run, given CLion's mingw gcc + scoop's MSYS sh:
- `mcctest`/`mcctest-bcheck` no longer hard-skip under an MSVC host. The
  differential test now takes a **`MCC_REF_CC`** cache var (gcc-compatible
  reference cc); on an MSVC host it also auto-`find_program(gcc)`s. Configure with
  `-DMCC_REF_CC="$clion/mingw/bin/gcc.exe"`. `run_mcctest.cmake` prepends the
  ref cc's dir to PATH so an off-PATH gcc finds its sibling as/ld. The ref gcc
  links straight against `msvcrt.dll` (tests/support/msvcrt_start.c) to match
  mcc's PE ABI; output is byte-identical to mcc's (1035 lines).
- `cli-suite` no longer skips on WIN32. The runner (tests/cli/runner.c) routes
  each case through a Unix/MSYS **sh** (writes the pipeline to a scratch
  `_clicmd.sh`, runs `sh <script>` — dodges cmd.exe quoting) selected by
  **`MCC_TEST_SH`** (`find_program(sh bash)` on WIN32). ~102 OS-agnostic
  driver/preprocessor/diagnostics cases run; the 31 ELF-image cases self-skip via
  `os=linux`. Needs mingw **binutils (nm/readelf) on PATH** at test time (they
  read mcc's ELF *relocatable objects*, which are ELF even on PE). Run ctest with
  `$clion\mingw\bin;C:\Users\llg\scoop\shims` on PATH.
- `diff3-suite` + `preprocess-suite` now RUN on Windows (2026-06-30) — see the
  clang-toolchain note below. Still genuinely unskippable: `compile.ex4` (X11),
  `pe-wine-conformance` + 5 `macho-*` (wine / macOS / Linux ELF-trampoline+seccomp
  tooling + cross compilers), `i386-fastcall-abi` (see mingw-toolchain notes —
  wired but still skips for a deeper reason than -m32), `asm-gas-directives`
  (assembler lacks sgdtq/sidtq/swapgs).

**Self-contained downloadable clang toolchain — `cmake-clang`** (2026-06-30):
a CMake **`clang-toolchain`** target (mirrors `mingw-toolchain`) fetches a pinned,
SHA256-verified official LLVM release (22.1.8 `x86_64-pc-windows-msvc`, ~862 MB
.tar.xz) into `${MCC_CLANG_DIR}/cmake-clang/<subdir>/bin/clang.exe` (gitignored;
idempotent via `.mcc-clang-stamp`). `cmake --build <bld> --target clang-toolchain`.
This is the SECOND reference compiler `diff3-suite` + `preprocess-suite` need
(they skipped for lack of a clang). Once fetched, a **(re)configure** auto-wires
it as `DIFF3_CLANG` (via `mcc_clang_resolve()` + EXISTS), so both suites register
and run. URL/SHA/SUBDIR are overridable cache vars (`MCC_CLANG_URL/SHA256/SUBDIR`).
- The downloaded clang targets windows-msvc and **auto-detects the installed
  VS 2026 + Windows SDK** — plain `clang file.c -o f.exe` links a runnable PE on
  this host with no vcvars. (So mcc=msvcrt, gcc=mingw, clang=msvc CRTs; diff3
  absorbs CRT-format divergences as gcc!=clang impl-defined.)
- `diff3-suite` on Windows also needs **`MCC_TEST_SH`** (the same MSYS sh as
  cli-suite): its runner was ported to route system()/popen() through sh. Run
  ctest with `$clion\mingw\bin;C:\Users\llg\scoop\shims` on PATH. diff3 is slow
  (~140 s: three compilers × ~200 goldens, clang's msvc compiles dominate).
- Two goldens carry a **`diff3!=WIN32:`** req token (7th field) — a diff3-only
  skip the exec runner ignores: `memory_model` (mcc defines `__LLP64__`,
  gcc/clang-on-Win define no `__*LP*__`) and `c11_freestanding_headers` (mcc
  guarantees `__STDC_IEC_559__`/lock-free atomics that gcc/clang-on-Win lack) —
  cases where mcc is the *more complete* impl, so the gcc==clang consensus is
  wrong. See [[moderncc-windows-port-fixes]] seventh pass.

**Self-contained downloadable mingw toolchain — `cmake-mingw-*`** (2026-06-30):
a CMake `mingw-toolchain` target fetches a portable mingw-w64 GCC and unpacks it
under `${MCC_MINGW_DIR}/cmake-mingw-*` (default source root; gitignored). Build it
with `cmake --build <bld> --target mingw-toolchain`.
- `MCC_MINGW_SOURCE` = **winlibs** (default) or **multilib**. winlibs downloads
  TWO native compilers — `cmake-mingw-x86_64/mingw64/bin/gcc.exe` (64-bit, seh)
  and `cmake-mingw-i686/mingw32/bin/gcc.exe` (native 32-bit, dwarf) — from pinned,
  **SHA256-verified** GitHub release zips (brechtsanders/winlibs_mingw, GCC 16.1.0
  UCRT r3; URLs+hashes are cache vars `MCC_MINGW_WINLIBS_*`). multilib expects a
  single gcc that does -m32/-m64 from a user-supplied `MCC_MINGW_MULTILIB_URL`
  (SourceForge multilib builds are GCC ~9/2020, no stable checksummed host).
- Fetch is idempotent via a `.mcc-mingw-stamp` per tree (delete to refetch),
  generated driver = `<bld>/run_mingw_fetch.cmake`. WinLibs gcc **self-finds its
  as/ld** (no PATH needed), unlike CLion's mingw.
- Once established it auto-wires: on an MSVC host with `MCC_REF_CC` unset, mcctest/
  -bcheck default their reference cc to the downloaded x86_64 gcc (so they run with
  zero system gcc). And `MCC_TOOLCHAIN_PROFILE=mingw` builds mcc *with* the
  downloaded toolchain: any profile naming `mingw` forces the superbuild matrix
  (a leaf build never switches CMAKE_C_COMPILER), the `mingw-*` cell DEPENDS on
  `mingw-toolchain`, and it builds into `<parent>/mingw-native` + runs full ctest.
  Verified `-G Ninja -DCMAKE_C_COMPILER=<dl x86_64 gcc> -DMCC_TOOLCHAIN_PROFILE=mingw`
  → 33/33 (the VS generator can't drive gcc, so a mingw matrix parent needs Ninja
  or MinGW Makefiles).
- `i386-fastcall-abi` is now wired to the downloaded i686 gcc (native 32-bit, so
  the driver passes M32="" instead of -m32; multilib gets -m32). It STILL skips on
  Windows, but for the real reason: the test MIXES mcc-built and reference-cc-built
  objects in one link, and **mcc emits ELF objects while mingw gcc emits PE/COFF**
  (`mcc.o` magic `7f454c46`, `gcc.o` `64 86…` = AMD64 COFF) — incompatible. The
  driver now probes a mixed link up front and self-skips ("SKIP:" → ctest Skipped)
  instead of failing. So enabling it needs an ELF-emitting reference on Windows,
  which doesn't exist; `-m32`/multilib alone is insufficient.
- **superbuild matrix**: `-DMCC_TOOLCHAIN_PROFILE="gcc;msvc" -DMCC_SUPERBUILD_TEST=ON`
  builds+tests each toolchain in its own ExternalProject. msvc cells auto-pick the VS
  generator (vswhere → `MCC_MSVC_GENERATOR`). Verified both cells 33/33.

**Local Windows build-dir naming convention**: use the `cmake-windows-<flavor>`
prefix (canonical MSVC build = `cmake-windows-msvc`). All build-dir exclude sites
now recognise it consistently: `.gitignore`, the two `CMakeLists.txt` EXCLUDE
regexes (tags `GLOB_RECURSE` ~2777 + `CPACK_SOURCE_IGNORE_FILES` ~2824), and the
qemu-docker rsync. CMake bakes absolute paths into its cache, so you can't rename
a build dir in place — recreate it under the new name (a fresh configure).

**qemu-user cross matrix via Docker** (Docker Desktop needs WSL2; if it won't start,
`wsl --update` then restart it). The repo ships `tests/qemu/docker` + a `qemu-docker`
CMake target. Run directly: `docker build -t mcc-qemu tests/qemu/docker` then
`docker run --rm -v "<src>:/work" -v mcc-qemu-roots:/qemu-roots mcc-qemu` (cache vol
holds Gentoo stage3 rootfs). Narrow with `-e ARCHS=arm64 -e LIBCS=glibc`. Full grid
(x86_64;i386;arm;arm64;riscv64 × glibc;musl) = 21 tests, all green under qemu-user.
scoop's Windows qemu is **system-mode only** — user-mode qemu-* exists only inside the
Linux container, which is why this path needs Docker.
The entrypoint `run-matrix.sh` rsyncs `/work`→`/src`; it now excludes
`cmake-build*`/`cmake-windows-*`/`build-*`, so a host build mutating a build dir no
longer races the staging (previously an unexcluded `build-*` dir → rsync "files
vanished", exit 24). Verified: matrix green while a concurrent MSVC ctest wrote into
`cmake-windows-msvc/`.

**Reproducing the Linux CI from this Windows host — `mcc-ci` Docker image**
(2026-07-01): `tests/ci/docker` is a container-equivalent of the GitHub Actions
`linux` job. `docker build -t mcc-ci tests/ci/docker` then
`docker run --rm -e PRESET=linux-gcc -v "C:\Users\llg\Projects\moderncc:/work:ro" mcc-ci`
(PRESET = any CI preset: linux-gcc[-cross|-musl|-release], linux-clang[-cross|-release]).
As of 2026-07-02 the docker runners + CI workflows drive everything through named
CMake presets (`cmake --preset`), NOT hand-passed `-D` flags — run-ci.sh just
takes `PRESET`, stages the mount to /src, and builds in /src/cmake-build-<preset>.
CMakePresets.json is the single source of truth (see BUILD.md §2); preset names ==
CI job/matrix names. It's parity by construction. Three gotchas it handles that
bite a naive image: (1) ubuntu 24.04 apt cmake is 3.28 but the macho-native
driver needs `cmake_language(EXIT)` (>=3.29) — the image pulls current cmake
from Kitware's APT repo; (2) runs as a non-root `ci` user (root masks
permission-sensitive tests, e.g. the `grep` exec golden); (3) `run-ci.sh`
normalizes CRLF->LF on staging because a Windows autocrlf checkout otherwise
feeds CRLF sources that break LF-expecting tests. This image is how a
glibc-only fix (like the `<sys/cdefs.h>` shim that flips `mcctest-bcheck` green)
gets validated without a Linux box. macOS + Windows/MSVC cells can't be
containerized on a Linux host, so they stay in GitHub Actions.

**Inspecting arm64 (or any cross) codegen from this x86_64 host** (2026-07-02):
`build-cross/` holds prebuilt cross compilers (`mcc-arm64-win32.exe`,
`mcc-arm64.exe`, ...). To debug arm64 codegen without an arm64 box:
`mcc-arm64-win32.exe -c -o x.o x.c` then disassemble — the object is **ELF**
(mcc always emits ELF relocatables even for PE), so parse the ELF `.text` and
feed bytes to **capstone** (`pip`'s `capstone` is installed; `CS_ARCH_ARM64`).
Do NOT name the script `dis.py` — it shadows stdlib `dis` and breaks capstone's
`import inspect`. To rebuild a cross mcc: ninja is at
`$clion/bin/ninja/win/x64/ninja.exe`, target name is `mcc-arm64-win32.exe`, and
you must put `$clion/bin/mingw/bin` on PATH first (else `gcc` can't spawn `as`).

Class of bug this caught: **arm64-gen.c mask literals must be 64-bit**. mcc is
built on an LLP64 host (Windows → `unsigned long` is 32-bit), so `~0xffful`
against a `uint64_t` zero-extends to `0x00000000fffff000` and silently drops the
top 32 bits. In `arm64_gen_opic`'s `+` case this made a large constant
(`p -= 0x700000000042`) look like a 12-bit immediate → emitted `sub x0,x0,#0x42`.
Fixed with `~(uint64_t)0xfff`. Such bugs are invisible on an LP64 (Linux) host.

**Per-case CTest split changed the counts (commit 977183d2, "Split aggregate
test suites into per-case CTests")**: the exec/cli/diff3/parts/preprocess
corpora now register **one CTest per case**, so the old aggregate "53/53"
numbers are obsolete. Post-split sweep re-verified green **2026-07-04** (see
below). The suite count depends on whether the optional clang toolchain has
been fetched: **without clang** the ~260 three-way diff3/preprocess cases
self-skip → native **513/513**; **with clang** (`cmake --build <bld> --target
clang-toolchain`, auto-wired on next reconfigure) → native **775/775**. No code
fixes were needed for the 2026-07-04 sweep — the recent comment-strip/reformat/
split commits did NOT break any Windows preset; only README/`.gitignore` doc
updates (counts refreshed 53→775/773, fixed broken `docs/BUILD.md` link,
`12 cross`→11, added `/stage/` to .gitignore since the dist install prefix
wasn't ignored).

**Post-refactor re-sweep (2026-07-04, after the mccrt/vendoring/dist/bin2c
commits `b2ae4912`..`bbcc0e01`)** — big infra changes landed and I re-verified
every Windows preset green. What changed and bit:
- **Preset binaryDir renamed** `cmake-build-<preset>` → **`cmake-<presetName>`**
  (CMakePresets `_base`). Old `cmake-build-*` dirs at the repo root are stale
  cruft. `.gitignore` now ignores `/cmake-*/` + `/vendor/` + `/dist/`.
- **Toolchains are vendored** under **`vendor/`** now (autodetect is
  *vendor-first*), NOT the old `cmake-clang/` / `cmake-mingw-*/`. Exact dirs the
  resolvers EXISTS-check: `vendor/llvm-clang/clang+llvm-22.1.8-x86_64-pc-windows-msvc/bin/clang.exe`,
  `vendor/winlibs-mingw-w64-16.1.0-ucrt-x86_64/mingw64/bin/gcc.exe` (+ `-i686/mingw32`).
  A previously-fetched clang/winlibs in the OLD locations silently self-skips
  diff3/preprocess (native drops 775→513) — the fix is to MOVE them into the
  vendor layout (a same-drive rename; no ~1.5 GB re-download). No stamp needed;
  resolve just checks the binary EXISTS.
- **Runtime lib renamed to `mccrt`**; `MCC_EMBED_MCCRT` (bake `libmccrt.a` into
  mcc) defaults ON on ELF/Mach-O but is **forced OFF on WIN32** → Windows uses
  the sidecar `libmccrt.a`, built by mcc (configure logs "MCC_EMBED_MCCRT not
  supported on WIN32; using the sidecar libmccrt.a"). Windows unaffected by the
  embed path.
- Options were renamed to plain English (`MCC_ONE_SOURCE`→**`MCC_SINGLE_SOURCE`**,
  etc.); `bin2c.cmake`→`tools/bin2c.c` host tool; `dist/` is the shared
  install/output root (default `CMAKE_INSTALL_PREFIX`).
- **msvc is now 775/775, not 773.** `mcctest` registers AND passes on the MSVC
  host (its gcc ref auto-resolves to the vendored winlibs GCC via `MCC_REF_CC`),
  and with `vendor/llvm-clang` present the diff3/preprocess refs (gcc+clang)
  auto-resolve, so msvc matches the mingw hosts exactly (123 skips). The old
  "773 / two suites unregistered on an MSVC host" figure is stale; README prose
  updated (the ctest P/S matrix table was already correct).
- Doc fixes this pass: README `vendor/clang`→`vendor/llvm-clang` (×2) + the msvc
  773→775 prose; CMakeLists resolver comments `vendor/clang`→`vendor/llvm-clang`
  and `vendor/gcc/bin`→`vendor/gnu-gcc/bin`. BUILD.md was already current
  (vendor §10, EMBED_MCCRT WIN32 note, `cmake-<presetName>/`).
- **Non-Windows regression found (NOT fixed — out of the Windows scope):**
  `linux-clang`/`-cross`/`-release` fail to BUILD (before tests). The mccrt-embed
  path compiles the *whole runtime with the host CC*; the host clang rejects
  `runtime/lib/builtin.c` (`__builtin_bswap64` alias: LP64 `unsigned long` vs
  `unsigned long long` → "conflicting types") and `runtime/lib/atomic.c`
  ("cannot redeclare builtin function `__atomic_load/store/exchange/compare_exchange`").
  `-fno-builtin` and targeted `-fno-builtin-<name>` do NOT fix it (clang keeps
  `__builtin_*`/generic `__atomic_*` reserved). gcc tolerates all of it, so
  `linux-gcc*` (11 presets) are green 803/803. Windows dodges it entirely (EMBED
  forced OFF → sidecar built by mcc). Clean fixes would be either
  `#pragma redefine_extname` in the runtime, or forcing `MCC_EMBED_MCCRT=OFF`
  on clang hosts (mirror Windows). Left for the refactor author to choose.

**Coverage + CI fixes 2026-07-05 (commits `0e700c76`, `dad9854c` on main)** —
after the sweep below, three real fixes:
- **`package-dist` missing on the mingw dist superbuild** (`0e700c76`): the
  `dist-mingw` preset forces the superbuild matrix, whose parent returns early
  and never defines `package-dist`, so `ci dist`'s final
  `cmake --build --preset dist-mingw --target package-dist` died with
  "ninja: error: unknown target 'package-dist'". Fix mirrors the existing
  `bench` forwarding in `CMakeLists.txt`: forward `package-dist` to a leaf cell
  (overwrite `_pkg_cmds`, one package pass; dist tree is shared). MCC_DIST_
  VERSION/PLAT reach the cell via the preset-var snapshot forwarding.
- **PE zero-init TLS bug** (`dad9854c`, `src/objfmt/mccpe.c` `pe_set_tls`): a
  zero-initialised `__thread` var read GARBAGE on PE (only explicitly-init'd
  thread-locals worked). The Windows loader's primary-thread static-TLS setup
  did NOT honour `SizeOfZeroFill` here — it copied only `[Start,End)` and left
  the zero-fill span as uninitialised heap. Fix mirrors gcc/ld: fold the .tbss
  span into `EndAddressOfRawData` and emit `SizeOfZeroFill=0` (the .tbss image
  is already zero-backed; the loader zero-pads a section's vsz past its raw
  size). Arch-agnostic (x86_64/i386/arm64 PE). Diagnosed via a 3-var probe +
  block-dump + comparing gcc's TLS directory (gcc also uses ZeroFill=0). NOT
  caught before because all prior TLS coverage used *initialised* __thread vars.
- **Minimal `<pthread.h>` shim** (`runtime/win32/include/pthread.h`):
  pthread_create/join over msvcrt `_beginthreadex` + WaitForSingleObject (NOT
  raw CreateThread — CRT per-thread init for printf/malloc), propagates the
  routine's void* return. Ungated 3 exec goldens on WIN32 (tls, atomic_counter,
  atomic_inlang_rmw) — real TLS-codegen + C11-atomics-under-contention coverage.
  Windows per-case skips **126 → 123**; every preset still 782/782 (debug+msvc
  verified). versym stays skipped (glibc symbol versioning) w/ accurate reason.
- **Full pthread/C11-threads shim** (`df40f1ed`): extended the WIN32 pthread
  shim to the full subset the generic pthread-based `runtime/include/threads.h`
  needs — mutex→SRWLOCK (non-recursive), cond→CONDITION_VARIABLE+
  SleepConditionVariableSRW, once→InitOnceExecuteOnce, keys→Tls*, self/equal/
  detach/exit/cond_timedwait/trylock. Support: new `runtime/win32/include/
  sched.h` (sched_yield→SwitchToThread); `nanosleep`+`clock_gettime`+
  `CLOCK_REALTIME` live in **pthread.h NOT time.h** (mcc's own runtime .c files
  include <time.h> WITH <windows.h>, and a simplified GetSystemTimeAsFileTime
  proto in time.h clashes with winbase.h's LPFILETIME — "incompatible types for
  redefinition"; clock_gettime uses CRT `time()`); `ETIMEDOUT` in errno.h; 8
  kernel32.def exports (SRWLOCK/CONDITION_VARIABLE/InitOnce — absent from mcc's
  curated def; Sleep/SwitchToThread/GetSystemTimeAsFileTime/Tls* already there).
  Ungated exec/c11_threads + cli/tss_dtor_iterations_ice. **Skips 123→121;
  782/782 on debug+msvc.** Stress-tested c11_threads (4 thr × 50k mutex ops +
  condvar) 30× zero fails/hangs. diff3/c11_threads stays a self-skip (a *ref*
  compiler can't build it on Windows). Gotcha that bit: declaring Win32 APIs in
  a widely-included std header (time.h) breaks mcc's own build.
- **fesetround/fegetround in mccrt** (`44b7ffe8`): msvcrt exports neither, so
  added `runtime/win32/lib/fenv.c`, compiled into libmccrt for WIN32 targets
  ONLY (ELF/Mach-O get them from the system libm — would double-define). FE_* is
  the x87 CW rounding field (bits 10-11). x86: set BOTH x87 CW (fnstcw/fldcw)
  AND SSE MXCSR (stmxcsr/ldmxcsr, bits 13-14 = x87<<3), read back from MXCSR.
  arm64: FPCR bits 23:22 via mrs/msr fpcr (RN/RM/RP/RZ<->FE_* map). arm-wince:
  default-only stub. Wired via `mcc_build_mccrt` (win32 obj list + the
  `mcc_mccrt_source` win32 regex). Ungated exec/fenv_access_fold (also proves
  mcc honours `#pragma STDC FENV_ACCESS ON` — doesn't fold 1.0/5.0). **Skips
  121→120; 782/782 debug+msvc; cross builds fenv.c for all PE arches.** mcc's
  arm64 integrated asm DOES handle mrs/msr fpcr (verified by capstone-disasm of
  the arm64-win32 fenv.o — mask 3<<22, insert mapped RMode). Gotcha: on
  arm64-win32 `unsigned long` is 32-bit (LLP64) so __fpcr is w-reg, but RMode is
  in the low word so it's fine.
- **Still open:** the ~40 diff3 + ~40 cli WIN32 skips are legit ELF/Darwin/ABI/
  impl-defined gates (checked: complex_const_init_overflow=msvcrt inf format,
  tgmath_nexttoward=LLP64 long double==double, string_init=wchar_t width, etc.).
  Cumulative session skip reduction: **126 → 120** (tls/atomics/c11_threads/
  tss_dtor/fenv_access_fold enabled + a real PE zero-init TLS bug fixed).

**Sweep 2026-07-08 (at `c8890941`)** — regenerated the stale Windows "all green"
counts (the docs still cited the pre-`CONFIG_AST` 812/810 basis; TODO.md "Now"
item). `CONFIG_AST` is default-ON so the `exec-replay`/`exec-replay-tmpl` columns
now register on Windows PE too, matching the Linux basis. Actual `ctest` run, all
6 native presets **1415 registered / 0 fail**: `debug`/`cst`/`diagnostics` = 1252
run / 163 skip, `cross` = 1254 / 161, `release` = 1236 / 179, `msvc` (cl-built) =
1252 / 163. `msvc` now EQUALS debug (1415, not "two fewer" — wine/macho
register-and-self-skip, counted; the old label-filter note is stale). Change was
docs-only (`docs/NOTES.md` "Windows status" + `docs/TODO.md`); marked the item
`[x]` and recorded the decision NOT to add a strict count-checker (the registered
total intentionally tracks upstream test additions → a hard drift-fail would break
CI on every new test). Also: rebased over two active-remote commits — `3a155ada`
(AST float/double replay, const-pool ordinal reuse) + `73998bab` (diagcross -Wall
warnings) — and re-verified the two canonical PE gates (debug mingw + msvc cl)
1415/1415 each: the float/double replay is byte-clean on PE (LLP64). Docker
`linux-gcc` sanity = 1467/1467. See [[moderncc-windows-port-fixes]].

**Sweep 2026-07-06 (at `5a641555`)** — full Windows preset sweep + one real
harness fix (cli/diff3 shell-helper PATH self-sufficiency; see
[[moderncc-windows-port-fixes]] twelfth pass). Per-case counts rose again:
native `debug`/`release`/`diagnostics`/`cst`/`cross` = **810/810** (~120 skips;
cross 692+118), `matrix` = 4 cells × 810/810, `mingw` superbuild cell = 810/810,
`msvc` (VS gen) = **808/808** (686+122; its test preset also filters wine|macho).
`sanitize` = intentional configure-fatal; `dist-mingw`/`dist-msvc` build +
`package-dist` exit 0 (4 zips each); `local-ci` configures+builds clean. KEY new
behavior: the cli/diff3 suites no longer need the invoker to put git's usr/bin
(grep/sed) or mingw bin (nm/readelf) on PATH — CMakeLists prepends both dirs to
each case's PATH via `ENVIRONMENT_MODIFICATION` (nm falls back to the vendored
winlibs bin). So `ctest --preset <p>` is green with just cmake+ninja(+git sh) on
PATH. Gotcha caught: running two heavy ctest suites concurrently caused a
transient `gcc -E` pipe hang (0 CPU) — run heavy suites serially.

**Sweep 2026-07-05 (at `8c63efad`→`4f20552f`)** — full Windows preset sweep,
**no code fixes needed** (everything already green; only doc counts refreshed +
`local-ci` documented, commit `4f20552f` pushed to main). Current per-case
counts rose (new cases added): native **782/782** (126 env-gated skips) with the
vendored clang present for `debug`/`release`/`diagnostics`/`cross`/`msvc`;
**520/520** without clang (diff3/preprocess self-skip). `matrix` = 4 cells ×
782/782. `sanitize` still an intentional configure-fatal on WIN32. `dist-msvc`
+ `dist-mingw` both build the full artifact matrix (13 mcc variants: host
mcc/-static/-dynamic + 11 cross + libmcc-static/-dynamic); dist-mingw fetches
winlibs (x86_64+i686) into `vendor/`. Preset lineup unchanged from the list
below except `asan`→**`sanitize`** and a new **`local-ci`** orchestrator preset
(`MCC_LOCAL_CI_AS_TEST=ON`; `cmake/ci-local.cmake` runs msvc/mingw/dist-* the
host can run). Note: after fetching clang via `clang-toolchain` into an EXISTING
build dir, a plain reconfigure DOES wire diff3/preprocess (the `find_program`
NOTFOUND cache is overridden by `mcc_clang_resolve()` — DIFF3_CLANG cache still
shows NOTFOUND but the local var registers the suites); rebuild to compile
`diff3_runner`/`preprocess_runner`, then the count jumps 520→782.

**Full preset sweep (last run 2026-07-04, post per-case-split)** — everything
runs via `cmake --preset` (binaryDir = **`cmake-<presetName>`**, was
`cmake-build-<preset>`). Verified green on this host:
- Native (with clang fetched): debug/release/diagnostics/cross **775/775** each
  (mingw gcc 13.1; 123 env-gated skips); msvc (VS generator) **773/773** (2
  suites unregistered on an MSVC host, 121 skips); `mingw` superbuild (fetches
  winlibs into cmake-mingw-x86_64/, cell runs full ctest) green; `matrix`
  (gcc;clang × native;cross) **4 cells × 775/775** — clang auto-resolves from
  cmake-clang/. dist-msvc + dist-mingw build the full artifact matrix (host
  mcc/-static/-dynamic + libmcc-static/-dynamic + **11 cross** compilers).
  **dist-mingw** also ships each cross in a `-static` shape; **dist-msvc** does
  NOT (CMakeLists ~2224 gates the per-cross `-static` variant off under MSVC —
  `-static` is a GNU-ld flag link.exe rejects; only host `mcc-static` remains).
  `asan` preset = intentional configure fatal on WIN32 (no libasan in mingw).
  `parts-suite` skips on the PE/msvcrt target (covered in aggregate by mcctest).
- Two Windows-only test-harness bugs fixed during the 53-suite sweep (commit
  c63e4aa4): exec `asm_operand_modifiers` needed an `expect_win32` golden
  (LLP64 `unsigned long` is 32-bit, so `%q0` truncates to `3456789a`); and
  `ci-pkg-smoke`'s harness (`tools/mccharness.c` suite_pkgsmoke) staged
  `bin/mcc` without the host `.exe` suffix, so `ci pkg`'s `.exe`-aware probe
  reported "no staged install" — now stages with `HOST_EXE_SUFFIX`.
- Docker: all 15 linux-* presets 39/39 each + dist-linux-{gcc,clang}
  (entrypoint override for dist: no test preset exists, run configure+build
  only) via `docker run --rm -e PRESET=<p> -v <src>:/work:ro mcc-ci`; qemu
  grid 22/22 via mcc-qemu with the mcc-qemu-roots volume (cached sysroots ->
  ~25 s).
- ctest exit codes in PowerShell pipelines are unreliable (NativeCommandError
  wrapping); redirect to a file and grep "tests passed" instead.
- Gotcha: on reconfigure of an existing build dir the superbuild forwards
  CMAKE_INSTALL_PREFIX to cells even when it's the platform default
  (INITIALIZED_TO_DEFAULT is only true on the FIRST configure) — harmless
  for build/test, surprising for `cmake --install`.

See [[moderncc-windows-port-fixes]] for the bugs fixed to get the suite green.
