# MAC.md — macOS / Darwin / Mach-O gating map

A catalog of every place the moderncc source, build, and test tree branches on
Apple/Darwin/Mach-O, and **why**. Use this when porting, debugging macOS
codegen, or deciding where a new Darwin behavior belongs.

> **HOST rows centralized (see HOST.md).** Every HOST-axis gate below now
> lives in `src/mcchost.h` / `src/mcchost.c`; the original sites call a
> `host_*` function or test a normalized `MCC_HOST_*` predicate instead of
> raw host macros. Raw host-macro tests outside `mcchost.{h,c}` are rejected
> by the `host-gate-invariant` ctest. The HOST rows in the tables below
> describe the *behavior* that moved; their file:line references are
> historical. TARGET rows are unaffected.


## 1. The controlling macros

There are three distinct axes. Keep them separate:

| Macro | Meaning | Set by |
|-------|---------|--------|
| `MCC_TARGET_MACHO` | **Compiler target** is Mach-O (the code mcc *emits* is Darwin). | `src/mcc.h:152` from `__APPLE__` when self-hosting; from CMake `-DMCC_TARGET_MACHO=1` when cross-compiling. |
| `__APPLE__` | **Host** building mcc is macOS (affects mcc's own runtime: unwinding, semaphores, exe-path). | The host C compiler. |
| `MCC_IS_NATIVE` | Target object format **and** CPU match the host — i.e. mcc can run/JIT what it emits. | `src/mcc.h:157-170`, requires `__APPLE__ == MCC_TARGET_MACHO` etc. |
| `CONFIG_NEW_MACHO` | Emit modern chained-fixups Mach-O (macOS 11+) vs. legacy `LC_DYLD_INFO` bind tables. | `src/mcc.h:184-188` (default on); CMake forces `no` on host macOS < 11. |
| `CONFIG_CODESIGN` | Run `codesign -f -s -` on the linked image (Apple Silicon requires ad-hoc signature). | CMake `MCC_CONFIG_CODESIGN`, default `yes` on Darwin. |
| `MCC_USING_DOUBLE_FOR_LDOUBLE` | `long double == double` (64-bit). | `src/mcc.h:210-213` for PE **and** Mach-O/arm64. |

`__MACH__` is **not** used as a gate anywhere; the codebase keys on
`__APPLE__` / `MCC_TARGET_MACHO`. `tools/c2str.c:12` records the
`__APPLE__` → `MCC_TARGET_MACHO` mapping used by the preprocessor's builtin defs.

---

## 2. Core configuration — `src/mcc.h`

- **152-154** — `__APPLE__` ⇒ define `MCC_TARGET_MACHO 1` (self-hosting default).
- **157-170** — `MCC_IS_NATIVE` handshake: host OS macro must equal target OS
  macro *and* host CPU must equal target CPU. Gates the JIT / `-run` path and
  native-only SDK helpers.
- **184-188** — `CONFIG_NEW_MACHO` default-on (chained fixups). `==0` disables.
- **200-202** — Everything that is *not* PE and *not* Mach-O falls through to
  `TARGETOS_Linux` (BSD handled just above).
- **204-208** — PE or Mach-O ⇒ `ELF_OBJ_ONLY` (mcc can still emit ELF `.o` but
  the *executable* linker is format-specific); else `MCC_TARGET_UNIX`.
- **210-213** — `MCC_USING_DOUBLE_FOR_LDOUBLE` for PE and **Mach-O/arm64**.
  Apple AArch64 defines `long double` as IEEE-754 double, unlike Linux/arm64
  (128-bit quad). This is the root of the `long double == double` quirk noted in
  memory `macos-arm64-status`.
- **946-948** — Mach-O-only fields on the dllref/sym struct (`install_name`,
  `compatibility_version`) for `-dynamiclib`.
- **1571** — `create_plt_entry`/`relocate_plt` prototypes exposed when
  `!MCC_TARGET_MACHO || MCC_IS_NATIVE` (Mach-O uses stubs, not ELF PLT, except
  when running natively).
- **1723-1730** — Mach-O output/link entry points (`macho_output_file`,
  `macho_load_dll`, `macho_load_tbd`); the SDK-path helpers
  (`mcc_add_macos_sdkpath`, `mcc_add_macos_sdkincludepath`, `macho_tbd_soname`)
  are further gated by `MCC_IS_NATIVE` (they shell out to the host SDK).
- **1844-1848** — `DEFAULT_DWARF_VERSION 2` on Mach-O vs. 5 elsewhere (dsymutil /
  Apple tooling compatibility). CMake overrides host builds to DWARF 4.
- **1876+** — Host-side `MCCSem` semaphore: `__APPLE__` uses
  `dispatch_semaphore_t` (`<dispatch/dispatch.h>`); Windows uses CRITICAL_SECTION;
  else POSIX. This is mcc's *own* threading, keyed on host `__APPLE__`.

---

## 3. Driver / library — `src/libmcc.c`

- **38** — include the Mach-O writer header.
- **813-826** — host `__APPLE__`: `_NSGetExecutablePath` for auto-mccdir
  (no `/proc/self/exe` on Darwin).
- **829** — same `__APPLE__` branch inside the exe-path resolver.
- **889-890** — `MCC_TARGET_MACHO` ⇒ `s->leading_underscore = 1` (Darwin ABI
  prefixes symbols with `_`).
- **932** — Mach-O default output/section setup.
- **970-971** — `MCC_TARGET_MACHO && MCC_IS_NATIVE` ⇒ add the host SDK include
  path (`mcc_add_macos_sdkincludepath`).
- **995-997** — Mach-O ⇒ add SDK library path (`mcc_add_macos_sdkpath`).
- **1110, 1144** — Mach-O library search recognizes `.dylib`.
- **1301-1302** — Mach-O lib name patterns `lib%s.dylib` / `lib%s.tbd`
  (text-based-dylib stubs).
- **1515** — Mach-O default linker output branch.
- **1650-1656** — Mach-O-only CLI options: `-compatibility_version`,
  `-current_version`, `-dynamiclib`, `-flat_namespace`, `-install_name`.
- **1765** — `leading-underscore` exposed as a settable state field.
- **1842-1843** — target triple string component `apple-darwin`.
- **1857** — Mach-O output-format default.
- **2183-2189** — `-fstack-protector` accepted **only** on `x86_64 && !MACHO`
  (ELF); warns "only implemented on x86_64 ELF" otherwise.
- **2341+** — Mach-O handling of `-dynamiclib` / `-flat_namespace` option codes.

---

## 4. Preprocessor builtin defs — `src/mccpp.c`

- **3893-3894** — target is Mach-O ⇒ predefine `__APPLE__` for compiled code.
- **3970-3972** — when `leading_underscore`, predefine `__leading_underscore`.

## 5. Symbol emission — leading underscore

- `src/mccgen.c:759` — prepend `_` to symbol names when
  `leading_underscore && can_add_underscore`.
- `src/mccasm.c:43` — inline-asm symbol resolution skips the `_` prefix logic
  when `!leading_underscore`.

Both are driven by the flag set at `libmcc.c:889` for Mach-O.

---

## 6. Object format & linking

### `src/objfmt/mccmacho.c` — the entire Mach-O writer
Compiled **only** when the target is Darwin (added to `MCC_BACKEND_SRC` at
`CMakeLists.txt:1712`). Notable internal gates:
- `dyld_chained_*` structs and `#ifdef CONFIG_NEW_MACHO` blocks (410, 622, 1120,
  1224, 1569, 1615, 1687…) — modern chained-fixups path vs. legacy
  `LC_DYLD_INFO_ONLY` bind/rebase tables.
- **1707-1711** — `LC_BUILD_VERSION` hardcodes `PLATFORM_MACOS`, minos/sdk 10.6.
- **1700-1704** — `LC_LOAD_DYLINKER` → `/usr/lib/dyld`.
- **2269-2279** — `#ifdef CONFIG_CODESIGN`: shells out to
  `codesign -f -s - <file>` after link (ad-hoc signature; mandatory on
  Apple Silicon).
- **2317-2344** — `mcc_add_macos_sdkpath` / `mcc_add_macos_sdkincludepath`:
  hardcoded CommandLineTools and Xcode.app SDK locations for lib and include.

### `src/objfmt/mccelf.c` — ELF writer, Mach-O carve-outs
- **1096** — `#ifndef MCC_TARGET_MACHO`: GOT/relocation handling that Mach-O
  routes differently.
- **1492** — Mach-O-specific pointer/DWARF-EXE handling.
- **1685** — `#ifndef MCC_TARGET_MACHO`: libmcc1 support-lib linking path skipped
  on Mach-O.
- **2883** — `mcc_output_file` dispatch: `pe_output_file` / `macho_output_file` /
  ELF by target.

---

## 7. Architecture backends

### `src/arch/arm64/arm64-gen.c` (Apple Silicon)
- **28** — default `CHAR_IS_UNSIGNED`-style setup only for non-Mach-O/non-PE.
- **40-44** — predefine `__arm64__` (Apple's spelling) in addition to
  `__aarch64__` on Mach-O.
- **471-508** — `arm64_macho_tls_addr`: Darwin TLS via a stack-based sequence,
  because Mach-O has no ELF `TPREL` thread-pointer relocations. The `#ifndef
  MCC_TARGET_MACHO` block at 508 is the ELF/PE thread-pointer path.
- **582, 620, 717, 764** — load/store paths call `arm64_macho_tls_addr` for
  `VT_TLS` symbols on Mach-O; ELF uses `TPREL_HI12/LO12` relocations.
- **952-957** — Apple AArch64 variadic ABI: all variadic args go on the stack
  (`nx=nv=8`), unlike AAPCS64.
- **1364, 1467, 1538, 1557, 1569, 1583** — `#if !defined(MCC_TARGET_MACHO)`
  guards around `va_list`/HFA register-save-area machinery that only the
  ELF/AAPCS64 variadic model needs.

> Reminder (memory `arm64-load-rflag-mask`): new front-end `VT_*` r-field flags
> must be added to arm64 `load()`/asm masks or codegen asserts; x86 is unaffected.

### `src/arch/x86_64/x86_64-gen.c`
- **123** — non-PE/non-Mach-O default.
- **336-389, 497-619** — Mach-O GOT/symbol addressing and function-call reloc
  sequences differ from ELF (`elif defined(MCC_TARGET_MACHO)` arms).
- **1451, 1606, 1621** — `#ifndef MCC_TARGET_MACHO`: ELF-only PLT/GOT emission.

### `CMakeLists.txt` cross-target defs (2342, 2363)
- `x86_64-osx` ⇒ `MCC_TARGET_X86_64 MCC_TARGET_MACHO`
- `arm64-osx` ⇒ `MCC_TARGET_ARM64 MCC_TARGET_MACHO`

---

## 8. Host runtime (`-run` / backtrace) — `src/mccrun.c`

All keyed on **host** `__APPLE__` (not the target):
- **68-70** — `PAGESIZE` via `getpagesize()` + `<libkern/OSCacheControl.h>`
  (needed for I-cache flush on Apple Silicon JIT).
- **78** — feature block excluded on `__APPLE__` (and Win32).
- **182-184** — `_NSGetEnviron()` instead of `environ` for the `-run` main.
- **263** — `__APPLE__` page-align branch.
- **930-931** — `MCC_TARGET_MACHO`: add `prog_base` to DWARF line PC (image is
  slid/PIE).
- **1142, 1165, 1193** — signal/unwind register extraction from
  `uc_mcontext->__ss` (`__eip/__rip/__pc/__fp`) — Darwin's mcontext layout for
  i386 / x86_64 / **aarch64**.

---

## 9. Runtime headers & bcheck

### `runtime/include/float.h`
- **43-56** — `_WIN32 || (__APPLE__ && __aarch64__)` ⇒ `long double` limits ==
  `double` (`LDBL_MANT_DIG 53`). Mirrors `MCC_USING_DOUBLE_FOR_LDOUBLE`.
- **72** — `(__aarch64__ && !__APPLE__) || __riscv` ⇒ 128-bit quad limits. The
  `!__APPLE__` explicitly carves Apple Silicon out of the quad path.

### `runtime/include/mccdefs.h`
- **95-98** — host `__APPLE__` ⇒ `__GNUC__ 4`, `__APPLE_CC__`,
  `__LITTLE_ENDIAN__`.
- **195-197** — `__APPLE__` builtin shims (`__builtin_flt_rounds`,
  `__builtin_bzero`).
- **268** — `__APPLE__` `__builtin_va_list` struct definition.
- **336** — `__linux__ || __APPLE__` share the `__MAYBE_REDIR` redirection.

### `runtime/lib/bcheck.c` (bounds checker)
- **12, 96, 137, 1016, 1123** — `__APPLE__` carve-outs: no `__malloc_hook`
  interposition on Darwin, different mmap/region handling, and the
  init-array/`__attribute__((constructor))` registration path differs.

---

## 10. Build system — `CMakeLists.txt`

- **369-372** — warn that `CODESIGN`/`NEW_MACHO` config is inert on non-Darwin.
- **433** — `_darwin` visibility expression (`MCC_TARGETOS STREQUAL "Darwin"`).
- **1067-1072** — `MCC_CONFIG_NEW_MACHO` and `MCC_CONFIG_CODESIGN` config nodes,
  visible only on Darwin.
- **1128-1132** — Darwin sets `MCC_DLLSUF .dylib`.
- **1149-1165** — Darwin: default DWARF 4, `CODESIGN yes`; auto-disable
  `NEW_MACHO` when host `sw_vers` reports macOS < 11.
- **1507** — define `MCC_TARGET_MACHO=1` for the target.
- **1569-1570** — `NEW_MACHO=no` ⇒ define `CONFIG_NEW_MACHO=0`.
- **1712** — add `mccmacho.c` to backend sources on Darwin.
- **1516-1520** — host `-dumpmachine`/musl detection (not Apple-specific but part
  of the same host-probe block).

---

## 11. Tests & CI

### `.github/workflows`
- `ci.yml:47-49` — `macos` job on `macos-15`.
- `ci.yml:109` — native ctest run **excludes** `qemu|wine|macho` labels
  (Mach-O suites run separately / need a Darwin host).
- `release.yml:31,40` — `clang-macos-arm64` release artifact on `macos-15`.

### `tests/qemu/` Mach-O suites (all `SKIP_RETURN_CODE 77`, label `macho`)
- `validate_macho.cmake` → `macho-structural`
- `run_macho_codegen.cmake` → `macho-codegen-run` (+ `qemu` arm64 variant, CMake 4184)
- `run_macho_image.cmake` → `macho-image-run`
- `run_macho_native.cmake` → `macho-conformance-native`
- `run_macho_apple_libc.cmake` → `macho-apple-libc`
- `macho-libsystem-kernel-fused` (CMake 3968-3969) — **skipped** unless a real
  macOS or darling host (`-DMCC_DARWIN_HOST=ON`); libmalloc/dyld/pthread/GCD/ObjC
  are kernel-fused with no portable variant. See
  `tests/qemu/apple-libc/PROVENANCE.md`.
- `tests/qemu/macho/loader.c` — standalone Mach-O loader used by the image tests.

---

## 12. Quick "why is this gated" cheat sheet

| Symptom / area | Reason for the gate |
|---|---|
| `long double` behaves like `double` | Apple AArch64 ABI: no 80/128-bit long double. `MCC_USING_DOUBLE_FOR_LDOUBLE`. |
| Symbols have leading `_` | Darwin C ABI. `leading_underscore=1`. |
| TLS uses a stack sequence, not TPREL | Mach-O has no ELF thread-pointer relocations. `arm64_macho_tls_addr`. |
| Variadic args all on stack (arm64) | Apple AArch64 variadic ABI differs from AAPCS64. |
| No PLT, uses stubs / chained fixups | Mach-O linkage model. `CONFIG_NEW_MACHO`. |
| Binary gets `codesign`'d | Apple Silicon requires a (ad-hoc) signature to exec. `CONFIG_CODESIGN`. |
| DWARF 2/4 not 5 | Apple tooling (dsymutil) compatibility. |
| `-fstack-protector` warns | Only implemented for x86_64 ELF. |
| Hardcoded `/Library/Developer/CommandLineTools/...SDK` paths | mcc has no bundled Darwin libc; borrows the host SDK. Native-only. |
| Mach-O tests skipped in CI | Kernel-fused Apple libc needs a real Darwin/darling host. |
