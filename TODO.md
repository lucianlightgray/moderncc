# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Platform spec — merged from HOST.md / MAC.md / WIN.md — 2026-07-03

The three platform-gating reference docs were rewritten into this section and
the files deleted (full prose: `git log -- HOST.md MAC.md WIN.md`). Every
"holds" statement below was implementation-verified against the tree on
2026-07-03 (see the verification sweep section) with the sweep's corrections
already baked in; treat them as invariants — breaking one is a deliberate
decision, not drift. The `[ ]` items are the open work the docs carried or
implied.

### Host axis (was HOST.md)

**Holds:**

- Two axes, kept strictly apart: **HOST** (`_WIN32`/`__APPLE__`/`__linux__`/
  BSDs — the machine mcc *runs on*) vs **TARGET** (`MCC_TARGET_PE`/
  `MCC_TARGET_MACHO`/`TARGETOS_*` — what it *emits*); `MCC_IS_NATIVE` = both
  agree (enables `-run`/JIT/backtrace). All HOST-axis code lives in
  `src/mcchost.{h,c}`; TARGET gates stay in the backends.
- **The invariant** (enforced by the `host-gate-invariant` ctest,
  `cmake/host_gate_check.cmake`): outside `mcchost.{h,c}`, no
  `#if/#ifdef/#ifndef/#elif` under `src/` may test `_WIN32`, `_WIN64`,
  `_MSC_VER`, `__MINGW*`, `__CYGWIN__`, `__APPLE__`, `__linux__`, the BSDs,
  `__ANDROID__`, or `__dietlibc__`; code tests only `MCC_TARGET_*`,
  `TARGETOS_*`, `MCC_IS_NATIVE`, `MCC_HOST_*`, `CONFIG_*`. Only directive
  lines are scanned (string literals like mccpp.c's predefine table are
  exempt by design).
- `mcchost.h` owns the normalized predicates (`MCC_HOST_WIN32/WIN64/DARWIN/
  LINUX/BSD/POSIX`), the host-compiler shims (system headers, MSVC
  `inline`/`snprintf`/`strtoll` remaps, `NORETURN`/`ALIGNED`/`PRINTF_LIKE`/
  `FALLTHROUGH`, `va_copy`, `O_BINARY`, `LIBMCCAPI`, `HOST_VOLATILE_LDOUBLE`
  for the MSVC x64 long-double folding bug, POSIX `stricmp`/`strnicmp`), and
  the path macros (`HOST_PATHSEP` `;`|`:`, `HOST_PATHCMP` `stricmp`|`strcmp`,
  `HOST_IS_DIRSEP`, `HOST_IS_ABSPATH`, `HOST_EXE_SUFFIX`, `CONFIG_MCCDIR`
  defaults incl. `host_w32_mccdir()` = exe-relative dir on Windows).
- `mcchost.c` compiles for every triple (first in libmcc.c's ONE_SOURCE list;
  in `MCC_CORE_SRC`) — cross compilers still need paths/spawn/clock/self-path.
  The JIT and fault sections compile only under `MCC_IS_NATIVE` (faults also
  `CONFIG_MCC_BACKTRACE`). `runtime/lib/bt-exe.c` includes `../mcchost.c`
  before `../mccrun.c`; under `CONFIG_MCC_BACKTRACE_ONLY` only the fault
  section compiles.
- **API rule:** anything with runtime behavior is a real function in
  `mcchost.c`; only textual/type shims and constant predicates are macros.
  The families: paths (`host_path_normalize`, `host_path_canonical` —
  libc-malloc'd, free with `libc_free` — `host_path_hash_fold`, `host_fopen`/
  `host_fclose`, `host_set_exec_bits`); self-location (`host_exe_path`:
  /proc/self/exe | /proc/curproc/{exe,file} | `_NSGetExecutablePath` |
  `GetModuleFileNameA`; `host_system_dir`); spawn (`host_spawn_wait` — quotes
  argv per host, fork/execvp on POSIX, **no shell** — `host_exec_replace`,
  `host_find_tool(name, ext, buf, size)`→1/0, `host_codesign_adhoc` wrapping
  the CONFIG_CODESIGN policy); time/env (`host_clock_ms`, `host_environ`);
  dynamic loading (`host_dlopen/dlclose/dlerror/dlsym/dlsym_process` — the
  `CONFIG_MCC_STATIC` stubs and built-in `mcc_syms` table live inside;
  `RTLD_*` never leaks out); runnable memory (`host_pagesize`,
  `host_runmem_alloc(&size, &ptr_diff)` owning the SELinux double-map /
  VirtualAlloc / malloc+page-slack strategies, `host_runmem_free`,
  `host_runmem_protect` with `HOST_PROT_RX/RO/RW/RWX` = 0–3 **load-bearing**
  (`mcc_relocate_ex` maps its section class onto them) and icache flush on
  RX/RWX, `host_unwind_register(table, .pdata byte size, base)` — the
  RUNTIME_FUNCTION size division lives inside since x64 ≠ arm64 —
  `HOST_RUNMEM_RO` Darwin W^X toggle honoring `CONFIG_RUNMEM_RO`); faults
  (`host_fault_install/regs/unblock`, `HOST_FAULT_*` codes,
  `HOST_FAULT_OTHER_FMT` — mcchost owns handler installation and the whole
  OS×arch mcontext pc/fp/sp matrix; mccrun registers one `rt_fault` callback;
  nonzero return = continue OS handler search, i.e. Windows trap); native
  services (`host_macos_sdk_root` cached — the hardcoded
  CommandLineTools/Xcode.app fallback *path lists* stay in mccmacho.c, plain
  strings — and `host_elf_interp_override` = `LD_SO` on BSD).
- Deliberate exceptions: `_CRT_glob`/`_dowildcard` stay in
  `mcctools.c` (driver-only, not library) gated `MCC_HOST_WIN32`;
  `MCC_IS_NATIVE` derivation stays in `mcc.h` testing `MCC_HOST_*` (no ctest
  whitelist); `MCCState.run_function_table` lives in the `MCC_IS_NATIVE`
  run-state block (NULL outside Win64).
- Out of mcchost's scope (separate axes): `runtime/lib` + `runtime/include`
  (gated on the OS the *compiled program* runs on — compiled by mcc for the
  target, can never call the host layer); `tests/{cli,exec,diff3}/runner.c` +
  `tests/embed/*.c` (standalone binaries, don't link libmcc internals); CMake
  host probes; `tools/c2str.c`'s host→target substitution table.
- Porting recipe: a new host OS/libc or a new native arch's signal context is
  a diff to `mcchost.c` only; quoting, W^X, and case folding are all testable
  through that one seam.

The lock primitive follows the `host_` convention since 2026-07-03:
`HostSem` with `HOST_SEM(s)`/`HOST_SEM_WAIT`/`HOST_SEM_POST` and inline
`host_sem_wait`/`host_sem_post` (mcchost.h; the target-runtime `bcheck.c`
keeps its own independent `WAIT_SEM`/`POST_SEM` — separate axis). The
test-runner host glue is consolidated in `tests/support/hostcompat.h`
(`HC_MKDIR`/`HC_RMDIR`, `HC_POPEN_SH`/`HC_SYSTEM_SH` sh-script emulation,
`HC_POPEN_CMD` cmd requoting, `hc_envv`, `hc_set_c_locale`), used by
`tests/{cli,exec,diff3}/runner.c` and `tests/embed/api_extra.c`.

**Open items:** *(none — structural note: if `mcchost.c` ever outgrows one
file (~820 lines today; threshold ~2000), split into per-OS pieces
`#include`d from `mcchost.c` so the build interface never changes.)*

### Darwin / Mach-O target (was MAC.md)

**Holds — controlling macros:**

- `MCC_TARGET_MACHO` comes from `MCC_HOST_DARWIN` when self-hosting, or from
  CMake for the `x86_64-osx`/`arm64-osx` cross targets. `__MACH__` is never a
  gate. `tools/c2str.c` maps `__APPLE__`→`MCC_TARGET_MACHO` for builtin defs.
- `CONFIG_NEW_MACHO` (modern chained-fixups Mach-O, macOS 11+) defaults on;
  CMake forces `no` when host `sw_vers` reports macOS < 11; `==0` selects the
  legacy `LC_DYLD_INFO` bind/rebase tables.
- `CONFIG_CODESIGN` (node `MCC_CONFIG_CODESIGN`, default `yes` on Darwin):
  ad-hoc `codesign -f -s -` after link via `host_codesign_adhoc()` —
  mandatory to exec on Apple Silicon.
- `MCC_USING_DOUBLE_FOR_LDOUBLE` on PE **and Mach-O/arm64** (Apple AArch64:
  `long double` == IEEE double, unlike Linux arm64's 128-bit quad).
  `runtime/include/float.h` mirrors it: `_WIN32 || (__APPLE__ &&
  __aarch64__)` ⇒ double limits; `(__aarch64__ && !__APPLE__) || __riscv` ⇒
  quad limits.
- PE or Mach-O ⇒ `ELF_OBJ_ONLY`. `DEFAULT_DWARF_VERSION` 2 on Mach-O
  (dsymutil/Apple tooling) vs 5 elsewhere; CMake overrides Darwin host builds
  to DWARF 4.

**Holds — driver / front end:**

- `leading_underscore = 1` on Mach-O (Darwin C ABI): mccgen prepends `_`,
  mccasm skips its `_` logic when off, mccpp predefines
  `__leading_underscore`, and it is a settable state field. mccpp also
  predefines `__APPLE__` for the *target* program.
- Native-only SDK borrow (mcc has no bundled Darwin libc):
  `mcc_add_macos_sdkpath`/`mcc_add_macos_sdkincludepath` add host SDK
  lib/include paths. Library search recognizes `.dylib` and the
  `lib%s.dylib`/`lib%s.tbd` patterns; Mach-O-only CLI options:
  `-compatibility_version`, `-current_version`, `-dynamiclib`,
  `-flat_namespace`, `-install_name`; the dllref/sym struct carries
  `install_name`/versions; triple component `apple-darwin`.
- `-fstack-protector` accepted only on x86_64 ELF; warns "only implemented on
  x86_64 ELF" otherwise (item below).

**Holds — writer & backends:**

- `mccmacho.c` (the whole Mach-O writer) is compiled only for Darwin targets.
  `LC_BUILD_VERSION` hardcodes `PLATFORM_MACOS`, minos/sdk 10.6 (item below);
  `LC_LOAD_DYLINKER` → `/usr/lib/dyld`. Mach-O links via stubs/chained
  fixups, not ELF PLT (`create_plt_entry`/`relocate_plt` exposed only when
  `!MCC_TARGET_MACHO || MCC_IS_NATIVE`).
- `mccelf.c` carve-outs: Mach-O routes GOT/relocation handling differently;
  the `#ifndef MCC_TARGET_MACHO` block skips **crtend/crtn addition** on
  Mach-O (the libmcc1 `mcc_add_support` call just above it is unconditional
  and runs on Mach-O too); `mcc_output_file` dispatches PE/Mach-O/ELF.
- arm64 (Apple Silicon): predefines `__arm64__` (Apple's spelling) alongside
  `__aarch64__`; Darwin TLS via the stack-based `arm64_macho_tls_addr`
  sequence (Mach-O has no ELF TPREL thread-pointer relocs); Apple variadic
  ABI puts **all** variadic args on the stack (`nx=nv=8`, unlike AAPCS64) —
  the `va_list`/HFA register-save-area machinery is ELF-only; `char` is
  signed on PE/Mach-O (`CHAR_IS_UNSIGNED` only on ELF arm64). x86_64: Mach-O
  GOT/symbol addressing and call-reloc sequences differ from ELF; PLT/GOT
  emission is ELF-only.
- `mccrun.c`: on Mach-O add `prog_base` to DWARF line PCs (image is
  slid/PIE).
- Runtime tree: `mccdefs.h` Apple shims (`__GNUC__ 4`, `__APPLE_CC__`,
  `__LITTLE_ENDIAN__`, builtin shims, `__builtin_va_list` struct,
  `__MAYBE_REDIR` shared with Linux); `bcheck.c` Darwin carve-outs (no
  `__malloc_hook` interposition, different mmap/region handling, different
  constructor registration — item below).
- CMake: Darwin sets `MCC_DLLSUF .dylib`; the NEW_MACHO/CODESIGN nodes are
  visible only on Darwin and warn as inert elsewhere.
- Tests/CI: Mach-O suites `macho-structural`, `macho-codegen-run` (+ qemu
  arm64 variant), `macho-image-run`, `macho-conformance-native`,
  `macho-apple-libc` — all label `macho`, `SKIP_RETURN_CODE 77`;
  `tests/qemu/macho/loader.c` is the standalone loader;
  `macho-libsystem-kernel-fused` requires a real macOS/darling host
  (`-DMCC_DARWIN_HOST=ON`; see `tests/qemu/apple-libc/PROVENANCE.md`). Label
  exclusions: only the **MSVC** test preset excludes `macho`
  (`qemu|wine|macho`); macOS excludes `qemu|wine` and **does run** the macho
  suites; Linux excludes only `qemu`. The `macos` CI job runs on `macos-15`;
  `release.yml` ships `clang-macos-arm64`.

Landed 2026-07-03: `-mmacosx-version-min=a.b.c` (alias `-mmacos-version-min=`)
sets `LC_BUILD_VERSION` minos/sdk (`MCCState.macos_version_min`, parsed via
`parse_version`, default 10.6 preserved; asserted by the new `versionmin`
check in `tests/qemu/validate_macho.cmake` on both osx cross targets), and
the `macos` preset now sets `MCC_DARWIN_HOST=ON` so the `macos-15` CI runner
(a real Darwin host) exercises `macho-libsystem-kernel-fused`.

**Classified (investigated 2026-07-03, not actionable):**

- **`-fstack-protector` stays x86_64-ELF-only; the warning is the contract.**
  The option is honored where implemented and warns "only implemented on
  x86_64 ELF" elsewhere — an explicit, tested behavior (not silent
  acceptance). Porting the canary prolog/epilog to arm64/Mach-O/PE is real
  backend work with no current user demand and no way to link
  `__stack_chk_fail` on the cross targets we can test (no target libc in
  CI); revisit if a port request materializes.
- **bcheck on Darwin: libSystem-internal allocations stay untracked by
  design.** Investigation result: Darwin takes the same `MALLOC_REDIR`
  define-`malloc`-in-image + `dlsym(RTLD_NEXT)` path as glibc
  (`bcheck.c:87,130,919-931`), so *user-code and mcc-emitted* allocations
  ARE tracked — mcc's Mach-O writer even binds imports flat-lookup
  (`mccmacho.c:1288,1321`, no `MH_TWOLEVEL`), which makes the executable's
  `malloc` authoritative for mcc-emitted code. What two-level namespace
  makes impossible to intercept cheaply is libSystem's *internal* calls to
  its own `malloc`; a miss is a silent pass-through (`bcheck.c:522-526`) —
  **false negatives on library-originated pointers, never crashes or false
  positives**. The two stronger mechanisms are disproportionate: a
  `malloc_zone_t` swap means reimplementing a full zone vtable, and
  `DYLD_INSERT_LIBRARIES`/`__DATA,__interpose` changes `-b` from a
  linked-in runtime into an env-activated dylib (SIP-hostile) and needs
  `S_INTERPOSING` section support the writer lacks. Optional hardening if
  ever wanted: seed more Darwin ctype/rune regions (`bcheck.c:1016-1018`)
  and add a structural Mach-O bcheck-link assertion under `tests/qemu`.

### Windows / PE target (was WIN.md)

**Holds — axes & configuration:**

- A Windows host defaults its **target** to `MCC_TARGET_PE` (the bridge that
  makes native builds produce PE); `MCC_IS_NATIVE` needs host+target OS *and*
  arch to agree. PE system include/lib search: `{B}/include`,
  `{B}/include/winapi`, `{B}/lib` (ELF uses the `/usr` triplet tree);
  `CONFIG_MCC_ELFINTERP` is `"-"` (no ELF interpreter).
- Driver: the whole PE backend enters via `#include "mccpe.c"`;
  output-format dispatch defaults to PE; library search tries `.def`/`.dll`
  patterns; `-lm` maps away (PE has no separate libm); the default
  auto-linked libraries are `msvcrt, kernel32, user32, gdi32`
  (`mccpe.c`, honors `-nostdlib`); the `-dumpmachine` triplet string ends
  `-mingw32`. PE-only CLI: `-impdef`, `-g.pdb`, `-large-address-aware`
  (`mcc_pe_set_dll_characteristics`), `-Wl,--oformat=pe-*`. `mcc.c`: PE help
  text, `" Windows"` version suffix, `.exe`/`.dll`/`.o` default extensions.
  `mcctools.c`: whole `mcc_tool_impdef()` (extract `.def` from a DLL),
  `-win32` infix in cross-compiler names.
- `mccrun.c`: PE targets relocate via `pe_output_file()`/`pe_imagebase`
  instead of ELF runtime/PLT.
- `mccpp.c`: wide-string literals encode UTF-16 with surrogate pairs;
  `_WIN32`/`_WIN64` are predefined for the *target* program.

**Holds — symbol/type model & codegen (`MCC_TARGET_PE`):**

- mccgen/mcctok: `dllimport`/`dllexport` attributes + `pe_check_linkage`;
  stdcall decoration `_func@N`; `__builtin_va_start` (PE x64) and `__chkstk`
  builtins; `wchar_t` = `unsigned short` (UTF-16); `dllimport` indirects
  through the IAT and is rejected in constant expressions; 8-byte
  double/long-long alignment on PE i386; VLA result handling on PE x64;
  `arch_transfer_ret_regs` runs only for RISCV64 / non-PE x86_64 — **PE
  skips it**; `sigsetjmp`/`siglongjmp` builtins are non-PE only.
- `mccpe.c` writer: entry points `pe_output_file`, `pe_load_file`,
  `pe_putimport`, `pe_add_unwind_data`; PE reloc types per arch
  (x86-64/x86/ARM/ARM64). Its host glue comes from mcchost
  (`MCC_HOST_WIN64` `IMAGE_OPTIONAL_HEADER32` pick, `host_spawn_wait` for
  cv2pdb, `host_set_exec_bits`).
- `mccelf.c` PE carve-outs: `.rdata` vs `.data.ro`; explicit entry-point
  symbol; x64 unwind symbol setup; skip STT_NOTYPE re-export normalization;
  no `_`-prefix for symbols containing `@`; symbol adds routed through
  `pe_putimport`; no `dlsym` (PE uses static import tables); DLL bounds-check
  init; no `mcc_add_runtime` (PE links no ELF runtime); final dispatch to
  `pe_output_file`.
- ABI, per arch: **x86_64** — 4 arg regs RCX/RDX/R8/R9 + 32-byte shadow
  space (vs SysV's 6); RSI/RDI callee-saved (struct-copy prologue saves
  them, inline-asm clobber set includes them on i386 too); no GOT (all
  RIP-relative; GOT relocs error); TLS via `_tls_index` +
  `gen_pe_tls_base()` + `R_X86_64_TPOFF32` (TEB, not FS/GS SysV TLS);
  alloca/VLA always through the page-probing helper; SysV stack canary off.
  **i386** — structs ≤8 bytes returned in EAX/EDX; `__chkstk`/`__alloca`
  probing for frames ≥4096; FS TEB TLS with `R_386_TLS_LE`. **arm64** — X18
  platform register reserved; ADRP addressing, no GOT; TLS at `x18+0x58` via
  `arm64_tls_base_x30()`; named args in regs / all variadic on stack with
  `arm64_pe_param_off()`; `__chkstk` ≥4096; `pe_add_unwind_data()` emits
  `.pdata`/`.xdata`; inline asm limited to X0–X17. Link-time: `RELATIVE`
  relocs subtract the image base; PC32 tolerates undefined symbols
  (imports); TLS offsets from the PE TLS base (no ELF TCB); arm64 emits NOP
  stubs for out-of-range weak symbols.
- `runtime/lib/` (gates mean "this object will run on Windows"): bt-exe.c
  finds the PE image base via `VirtualQuery`; bt-dll.c is the Windows-only
  backtrace DLL shim; bcheck.c uses CRITICAL_SECTION/TlsAlloc/
  GetCurrentThreadId (no mmap/fork/siglongjmp); tcov.c locks via
  `LockFileEx`; runmain.c iterates `.init_array` only off-Windows (PE enters
  via `mainCRTStartup` from win32/lib/crt1.c); alloca*.c do guard-page
  probing; atomic.c x64 uses the Win64 register convention.
- `runtime/win32/`: lib/ has crt1.c/wincrt1.c (+ crt1w/wincrt1w unicode
  variants — `__getmainargs`, `_controlfp`, ctors, main); **dllcrt1.c owns
  `_dllstart` and the DLL_PROCESS_ATTACH/DETACH dispatch; dllmain.c is only
  the overridable default `DllMain` stub**; crtinit.c shared init/fini
  iteration; `.def` exports (gdi32/kernel32/msvcrt/user32/ws2_32). include/
  is the bundled MinGW-derived target-header set: `winapi/` (including
  `windows.h` — it lives under winapi/, not the include root), `sec_api/`,
  `tchar.h`, `process.h`.
- CMake: no sanitizers on PE (no mingw libasan); shared-libmcc cross-builds
  require emulator or static; suffixes `.exe`/`.dll`/`.lib`;
  `MCC_TARGET_PE=1`; PE backend selected; install stages libmcc1.a,
  win32/include headers, `.def` files, examples; cross targets
  `i386-win32`, `x86_64-win32`, `arm64-win32`, **and `arm-wince`**.
- CI/tests: msvc job excludes `qemu|wine|macho` via the `_test-msvc` preset;
  mingw job is build-only (winlibs GCC); `run_pe_wine.cmake` runs
  x86_64/i386-win32 output under wine64/wine32 (skip 77, `-I
  runtime/win32/include`); `tests/support/msvcrt_start.c` is the bare-msvcrt
  PE startup (DLL import redirection, SEH setjmp intrinsics, mingw-w64 ctor/
  dtor hooks); the cli/exec runners carry their own Windows shims (see the
  hostcompat item above); the exec runner skips `elf`-labeled tests on
  Windows/Darwin.

**Open items:** none Windows-specific beyond the shared ones above
(stack-protector covers PE; hostcompat covers the test runners).

---

## BUILD.md / HOST.md / MAC.md / WIN.md verification sweep — 2026-07-03

Verified every substantive statement in the four docs against the tree
(CMakeLists.txt, CMakePresets.json, ci.yml/release.yml, cmake/, src/, runtime/,
tests/). Verdict: the described behavior is almost entirely implemented — the
HOST.md §5 centralization is real (`src/mcchost.{h,c}` exist, the
`host-gate-invariant` ctest passes by hand-reproduction, zero raw host macros
remain outside mcchost), every MAC.md/WIN.md gate exists, and all §13
validation rules are enforced. The open items below are the residue: a few
small **code gaps** and a set of **doc statements that don't match the code**.

*Follow-up landed same day:* HOST.md, MAC.md, and WIN.md were rewritten into
the **Platform spec** section above (with this sweep's corrections baked into
the statements — crtend vs libmcc1, the msvc-only `macho` label exclusion,
the `-dumpmachine` mingw32 string vs the real msvcrt/kernel32/user32/gdi32
auto-link list, the PE-skipped `arch_transfer_ret_regs`, winapi/windows.h
location, dllcrt1.c owning `_dllstart`, the arm-wince cross target) and the
three files were deleted; their per-file correction items were dropped as
moot. BUILD.md remains a live doc, so its correction items below stand.

### Code gaps (implementation work)

*All landed 2026-07-03:* the `asan` build preset and `asan`/`cross` test
presets exist (`matrix` deliberately has no test preset — the superbuild
`return()`s before test registration and tests via `MCC_SUPERBUILD_TEST=ON`
per sub-build); the `MCC_BUILD_DYNAMIC_EXE` HELP string matches the target;
`MCC_GNU_GCC` (FILEPATH, advanced) and `MCC_SUPERBUILD_CHILD` (option,
advanced) are declared cache variables; and the lock API was renamed to the
`host_` convention (`HostSem`/`HOST_SEM_WAIT`/`HOST_SEM_POST`).

### Doc corrections — BUILD.md

*All landed 2026-07-03:* the cross-target table now shows the real
`mcc-<arch>` target ids; the CI-artifact paragraph is scoped to the
linux/macos/msvc/mingw jobs (with the dist `package.cmake` path, the
test-only qemu job, and the macOS `-<cc>` artifact segment noted); wording
fixes applied ("always set" compile-commands, no `cross` meta-target, §13
notes the two rules enforced outside `mcc_validate_config()`); and the
preset section reflects the new `asan`/`cross` test presets, the `matrix`
exception, and `MCC_DARWIN_HOST=ON` on the macos preset.

---

## ✓ LANDED: `-S` (assembly output) — 2026-07-02

`mcc -S` now emits a gcc/clang-style AT&T assembly listing.  This closes the
last cluster of "not separately testable: requires `-S`" `[~]` rows in
[C9911.md](C9911.md): §5.1.2.3p6, §5.1.2.3p12 EX4 and §5.1.2.4p16 are retagged
`mcc:✓` (the volatile/atomic access ordering is now inspectable and matches
gcc -O0).

**How it works.** mcc is TCC-derived — the backend emits raw machine-code bytes
into the section data with no textual-assembly IR — so `-S` runs a normal
object-style compile (relocations kept symbolic, no link) and then disassembles
the populated sections back to a listing.  New code:

- `include/libmcc.h` — `MCC_OUTPUT_ASM`.
- `src/libmcc.c` — `MCC_OPTION_S` handler + `mcc_set_output_type` (compile-only,
  no startup/libs, symbolic relocs).
- `src/mcc.c` — `.s` default filename, per-file compile + `-S`-with-libs/many-files
  guards, help text.
- `src/objfmt/mccelf.c` — routes `MCC_OUTPUT_ASM` to `asm_output_file`.
- **`src/mccdis.c`** (new) — arch-independent driver: walks sections + symtab,
  emits `.text`/`.data`/`.rodata`/`.bss` directives, `.globl`/`.weak`/`.type`/
  `.size`, function + local (`.L<off>`) labels, `.byte`/`.long`/`.quad`/`.skip`
  data with symbolic relocations; collision-free names for duplicate local
  statics; two-pass local-label collection for branch targets.
- **`src/arch/x86_64/x86_64-dis.c`** (new) — reloc-aware x86-64 disassembler:
  integer + SSE + x87 subset the backend emits, AT&T syntax, symbolic call/rip/
  absolute operands.
- `src/mccasm.c` — fixed a latent bug the round-trip surfaced: `.file` cleared
  `PARSE_FLAG_TOK_STR` without restoring it, breaking a following `.section`.

**Validation (all green).**

- `mcc -S | gas` re-assembles and runs **functionally identical** to `mcc -c`
  across a 9-program corpus (recursion, floats, structs-by-value, function
  pointers, varargs, switch, strings, bit-twiddling).
- **100% instruction accuracy** vs `objdump -d` on all non-branch instructions
  (remaining diffs are gas's own branch-shortening / implicit-shift re-encodings,
  not decoder errors).
- All **13 mcc source files (~108k instructions)** disassemble and re-assemble
  with gas — **zero** unknown-byte fallbacks, zero bad mnemonics.
- New hermetic `dash-s-roundtrip` ctest: `mcc -S` → mcc's own assembler →
  mcc link → run, byte-identical output to the direct build (x86_64 + integrated
  assembler); `dash_S_emits_assembly` cli case updated.
- Bugs the tests caught and fixed: sign-extended imm8 display (`$0xff`→
  `$0xffffffff`), x87 length desync, `call *%r11d`→`*%r11`, and a reloc-window
  bug (disp8 spuriously matching the next insn's relocation → wrong stack slot →
  segfault).

**Follow-ups — all landed 2026-07-03:**

- **All five targets decode.** `<arch>-dis.c` exists for x86_64, i386, arm64,
  riscv64, and arm; `MCC_HAVE_DISASM` covers every target and the `.byte`
  dump fallback is gone. The driver grew per-arch reloc hooks
  (`mcc_disasm_reloc_size` / `mcc_disasm_reloc_addend_bias`), REL-target
  in-place addend reading, ARM `%function`/`%progbits` spellings, and skips
  for non-reassemblable metadata labels (`<no name>` riscv anchors, `$a`-style
  ARM mapping symbols). Fidelity tiers, each regression-tested:
  - **arm64 / riscv64: byte-exact** — `mcc -c` vs reassembled `mcc -S`
    section contents are identical (new `dash-s-bytes-<arch>` ctests via
    `tests/support/seccmp.c` + `tests/asm/run_dash_s_bytes.cmake`).
    Instructions the integrated assembler cannot express round-trip as
    commented `.long`/`.word` words (arm64 ~12%, riscv64 ~3%).
  - **x86_64 / i386: instruction-exact, behaviorally verified** — the
    assembler legally re-encodes some widths (imm32→imm8, rel32→rel8, eAX
    short forms), so equality is behavioral (`dash-s-roundtrip`) rather than
    byte-wise; i386 was validated over a 12-file corpus (1568 insns, zero
    unknown-encoding fallbacks) incl. an execution proof.
  - **arm: link-equivalent** — 74/74 files instruction-exact (~475K insns);
    local-branch imm24 fields differ by the gas −2 pipeline-bias convention
    (the linker recomputes them), so it is excluded from the byte-exact test.
- **`.cfi_*` unwind directives.** `-S` listings carry
  `.cfi_startproc/endproc/def_cfa*/offset` (decoded from the FDE the compile
  produced, emitted exactly when unwind tables are on), and the integrated
  assembler parses them and regenerates an equivalent `.eh_frame` (byte-
  identical on arm64/riscv64; on x86 identical whenever the text bytes are).
  `mccdbg.c`'s FDE writer is now shared (`mcc_eh_frame_fde` + helpers).
- **Scalar SSE in the integrated assembler.** ~30 mnemonic rows added to
  `x86_64-asm.h`/`i386-asm.h` (movss/movsd, arith, conversions, compares,
  unpck), byte-verified against binutils `as`; fixed the broken
  `movaps %xmm,%xmm` operand mask; `dash_s_roundtrip/prog.c` now exercises
  float/double/conversion paths hermetically.
- **Latent assembler bugs found & fixed along the way:** x86_64-dis dc/de
  x87 register-form naming used the legacy-AT&T swap (would re-assemble to
  the reversed operation — real for `long double`); arm64 `ARM64_OFFSET19/14`
  branch fields OR'd in unshifted (clobbering cond/rt); arm64 `mul` encoded
  as `madd …,x0` instead of Ra=xzr; arm64 32-bit `asr #imm` left N=1
  (invalid SBFM); arm/arm64 `gen_expr32` dropped the symbol reloc on
  `.long sym`.

**Still open (documented gaps, not regressions):**

- [ ] Integrated-assembler syntax gaps keep some reloc-carrying operands as
  commented raw words: arm64 `:got:`/`:lo12:` operands (ADR_GOT_PAGE /
  LD64_GOT_LO12_NC dropped on reassembly), riscv64 store-form
  `%pcrel_lo`/addend-carrying operands (10 reloc fields on 2 corpus files),
  i386 `sym@ntpoff` TLS, arm `vldr dN` parse bug + `[rn, #-0]` spelling
  (67 `.long` sites). Each is enumerated with encodings in the respective
  `<arch>-dis.c` header comment / agent-validated corpus.
- [ ] riscv64-asm mnemonic gaps that force `.long` fallbacks: flw/fsw
  dispatch missing (tokens exist), fmv.x.w/.d family, `fcvt.d.s` hardcodes
  rm=7 where codegen emits rm=0 (both valid, bytes differ), `call`/`tail`
  use rd=zero and leak a redundant reloc pair, `la` emits lw.
  (The `sraw`/`sraiw`-encode-as-srlw/srliw latent bug was fixed 2026-07-03,
  verified against spec encodings.)
- [ ] `-fverbose-asm`-style operand comments: meaningful comments need
  codegen-side variable/spill metadata that is discarded after emission;
  classified low-value (reloc symbol names are already printed).

---

## C9911 → full_language.c coverage sweep — 2026-07-01 (worked to completion 2026-07-01)

Drove every runtime-observable requirement in [C9911.md](C9911.md) into the
differential test `tests/diff/full_language.c` (69 3-way-validated functions,
~570 clauses; `mcctest` driver now links `-lm`). All follow-up items below have
been implemented or resolved; suite is 34/34 green (incl. both mcctest variants,
exec-suite, cli-suite) and mcc self-compiles all 8 of its own sources.

### Landed this round (implemented + verified)

- **#line presumed name in `__FILE__`** (§6.10.4p3) — FIXED in `mccpp.c`
  (`mccpp_putfile` no longer resolves a relative `#line`/marker name against the
  current file's dir; `__FILE__` is now verbatim like gcc/clang; `true_filename`
  still holds the real path). Full suite green.
- **§7.6 `<fenv.h>`, §7.3.7–9 complex libm, §7.25 `<tgmath.h>` evaluated dispatch**
  — added `s7_6_fenv_test` / `s7_1_complex_libm_test` / `s7_23_tgmath_eval_test`
  to full_language.c (unblocked by the `-lm` driver change); pass mcctest +
  mcctest-bcheck, byte-identical to gcc.
- **§7.28–7.31 `<uchar.h>/<wchar.h>/<wctype.h>` runtime** — added
  `tests/exec/types/wchar_library.c` + goldens.h entry (`wchar_library`, gated
  `os=linux`); exercises char16/32_t, mbrtoc*/c*rtomb, wcs*/wmem*, swprintf/
  swscanf, wcstod/wcstol family, isw*/tow*/wctrans, wcstok. exec-suite green.
  (Kept in the exec-suite rather than full_language.c: system `<wchar.h>`
  redefines `FILE`, clashing with the file's `mcclib.h` opaque `FILE`.)
- **Diagnostic/constraint requirements → `tests/cli/cases.h`** — added 10 validated
  cases covering the previously-uncovered high-value ones: `static_assert_fail`,
  `static_assert_nonconst` (§7.2), `switch_duplicate_case`, `goto_undefined_label`
  (§6.8), `redefinition_object`, `array_of_functions`, `conflicting_redecl`,
  `void_param_named` (§6.9), `bitfield_nonint` (§6.7.2.1), `computed_goto_ext`
  (§6.8.6.1 GNU ext — confirms mcc supports it). cli-suite green. The remaining
  routed §4/§5, §6.2–6.5, §6.10 and Annex diagnostics are already exercised by
  the existing cases.h / tests/diagnostics / tests/exec/errors corpus or are
  `mcc:✓` per C9911 (spot-verified: read-only/case-label/duplicate-case/flexible-
  array/_Alignas already covered); this backlog is closed as representative +
  pre-existing coverage.

### Resolved as NOT actionable (by-design / optional-feature / non-deterministic)

Consistent with the 2026-06-30 classification below; each verified this round:

- **`_Alignof` on an automatic `_Alignas` object** (§6.2.8p5, GNU ext): works for
  static/global objects (via `VT_SYM`); only automatic-storage objects return the
  natural alignment. A fix needs the local's alignment plumbed through the SValue;
  an attempted change regressed `_Alignof(a[0])`, so reverted. GNU-extension corner
  (ISO `_Alignof` takes a type), low value, high risk — left as a documented partial.
- **array-parameter `[const]` not enforced** (§6.7.6.3p4): verified mcc is
  uniformly permissive about body-assignment through *any* qualified parameter
  (`const int a`, `int *const a`, `int a[const]` are all silent; a plain
  `const int` **local** does warn) — this is mcc's documented permissive-by-default
  stance, not a bracket-specific bug.
- **bcheck miscompiles `_Complex ==`** (§6.5.9): `mcc -b` on a `_Complex`
  equality compare hits an invalid memory access (complex temporaries created by
  `cplx_local` aren't registered with the bounds checker). Real but niche
  (optional `-b` feature + complex equality), deep bounds-checker codegen with
  high regression risk; already worked around in full_language.c (`s7_1_complex`
  compares parts). Left as a documented known limitation. Repro:
  `double complex a=1+2*I,b=a; a==b;` + `-b`.
- **`runtime/include/stdatomic.h` is clang-incompatible** (§7.17): the shim uses
  GNU `__atomic_*` on `_Atomic`-qualified pointers (clang needs `__c11_atomic_*`)
  and its `__atomic_is_lock_free` decl collides with clang's builtin. mcc itself
  compiles+links+runs all of C11 atomics correctly, and clang is **not** the
  `mcctest` gate (mcc-vs-gcc); the shim's clang-compat is a portability nice-to-have,
  not a conformance gap. Left tracked as a known header-shim limitation.
- **§7.26 `<threads.h>` / §7.27 `timespec_get`/`TIME_UTC`**: runtime-observable
  values are non-deterministic (scheduling, thread ids, wall-clock) and need
  `-pthread`; enum constants are implementation-defined. Not expressible as a
  deterministic differential sub-test. (Compile/link is exercised elsewhere.)
- **§6.7.1p3 thread-local-init "leak" / §6.7.5p1 `_Alignas` on an automatic
  array**: flagged by the sweep but did **not** reproduce on minimal 3-way probes
  (all three agree: `41 0` and correctly aligned). False positives — not bugs.

## C9911 re-verification crawl — 2026-06-30 (present status of every flagged divergence)

Re-verified **all 169** `mcc:✗`/`mcc:~` rows in [C9911.md](C9911.md) empirically
against the live binary (`0.9.28rc mob@a25f149b`, x86_64 Linux), 3-way vs
gcc 15.3 / clang 22. Each row got a minimal probe run with the exact flags its
note names; library/runtime rows were compiled **and run**. Verdict tally:

| Verdict | Count | Meaning |
|---|---|---|
| **FIXED ✓** | 34 | mcc now matches the gcc==clang consensus (C9911 row is stale) |
| **CHANGED** | 11 | behavior moved since the map was written — partial improvement, not yet ✓ |
| **STILL diverges** | 118 | reproduces as recorded |
| **NOT-TESTABLE** | 6 | no `-S`/codegen-ordering/optimization-barrier probe exists |

Of the 118 that still diverge, **most are *not* actionable** (consensus
`✗`/`✗`/`✗`, no gcc==clang consensus, UB/recommended-practice, or mcc's
documented permissive-by-default philosophy). The genuinely actionable residue
is the short list under **Open items** below. Two C9911 rows that an automated
pass flagged as regressions were **false alarms** (the probe omitted `-Wall`):
`#pragma STDC …  BOGUS` (§6.10.6p2) and label-at-end-of-block (§6.8.3p5) both
still warn under `-Wall` — see notes.

### FIXED since the map was written (34 — C9911 rows now stale, retag to `mcc:✓`)

Confirmed working on the live binary; these need only a C9911 retag, no code:

- **C11 predefined macros / headers:** `__STDC_UTF_16__`/`__STDC_UTF_32__` now
  defined `1` (§6.10.8.3, L475/L476); `<threads.h>` + `thread_local` compile and
  run (§7.26.1, L539/L4703/L4705).
- **Same-scope redeclaration constraints now hard-error:** `typedef int T; int T;`
  (§6.2.3p1, L530); `typedef`'d function type as a definition (§6.9.1p2,
  L1923/L1961); incomplete return type `struct S f(void){}` (§6.9.1p3, L1924);
  incomplete parameter type in a definition (§6.9.1p7, L1938); object vs typedef
  name-space clash (§6.7.8p3, L1657).
- **C11-feature-in-C99 pedantic diagnostics now fire** under `-std=c99
  -pedantic-errors`: `_Thread_local` (L1326), `_Atomic(...)` (L1354), typedef
  redefinition incl. variably-modified (§6.7p3, L1310/L1311), `u`/`U`/`u8`
  char/string prefixes (§6.4.4.4/§6.4.5, L5057).
- **Non-ISO extensions now diagnosed** under `-pedantic-errors`: binary `0b…`
  constants (§6.4.4.1, L925), `\e` escape (§6.4.4.4, L926), zero-size array
  `int a[0]` (§6.7.6.2p1, L1582), K&R parameter names without types (§6.7.6.3p3,
  L1606).
- **tgmath fully repaired (§7.25, L4654/L4658/L4662/L4663/L4668/L4672/L4673):**
  `nexttoward(f,ld)`→`nexttowardf`, type-generic `creal`/`cimag`, correct
  element-type resolution table — all match gcc/clang now.
- **Complex `*`/`/` infinity-recovery (Annex G.5.1, L5897/L5898/L5903/L5904):**
  mcc now implements the §G.5.1 recovery tables at `-O0` and `-O2`;
  `(INF+0i)*(1+1i)` → `inf+inf·i`, matching gcc/clang.
- **printf format checking (§7.21.6.1, L3807):** `%d` vs non-int now warns under
  `-Wall`.
- `_Imaginary` / inline-static handled: static object in extern-inline now warns
  (§6.7.4p2, L1521).

### CHANGED — partial improvements, not yet at consensus (11)

- **§7.27.1/§7.27.2 `struct timespec`/`timespec_get`/`TIME_UTC` (L4911):** now
  compile in **default** mode (glibc POSIX gate), no longer needing `-std=c11`;
  `TIME_UTC`/`timespec_get` still gated on `-std=c11`.
- **§D UCN identifier validation (L6111/L6120/L6121):** the `\uXXXX` **escape**
  form is now validated (rejects out-of-range and initial combining marks,
  matching gcc/clang) — but **raw UTF-8** extended identifiers are still accepted
  unchecked. Partial.
- **§G.5.1p5 (L5905):** mcc's default complex path now recovers infinities (no
  longer naïve); `CX_LIMITED_RANGE ON` still has no observable effect.
- **§7.21.7.7 `gets` (L3978):** now warns `implicit declaration of 'gets'` (was
  silent); gcc/clang still hard-error (removed in C11) — improved, still diverges.
- **Annex I informative warnings now emitted under `-Wall -Wextra` (L6173/L6175/
  L6176/L6178/L6182):** multi-char constant, `i = i++` may-be-undefined, call
  without prototype, unused variable, value-computed-not-used. Informative-only,
  so mcc was conforming either way; these move it toward gcc/clang.

### Open items — actionable residual gaps

*(none — every actionable consensus gap has been implemented and removed. The
diagnostic gaps, raw-UTF-8 validation, and the foldable NaN/Inf builtins landed
this round; the remaining flagged rows are classified below as deliberate or
not-actionable, each with rationale.)*

### Classified — NOT actionable (no migration needed)

The remaining flagged rows are intentionally out of scope:

- **Deliberate, test-encoded design choices:**
  - §6.7.4p6 (L1529) — a plain `inline`-only function used without an external
    definition: mcc emits a **local (non-exported)** definition so the call
    resolves, where gcc/clang (all `-std` modes) give "undefined reference".
    mcc's *symbol export* already matches C99 (the plain-inline symbol is **not**
    globally emitted — verified by `tests/exec/functions_abi/inline.c` +
    `inline2.c`, golden `0 inline_inline_undeclared`); the permissive local
    emission is intentional (plain-`inline`-in-header "just works") and that
    comprehensive export-matrix test asserts it. Changing it would break the test
    and regress real code.
  - §6.10.8.1 (L473) — mcc advertises C99 (`__STDC_VERSION__ == 199901L`) so the
    handful of C11-only names need explicit `-std=c11`: `static_assert`
    (§7.2, L2198/L2211), `aligned_alloc` (§7.22, L4437), `TIME_UTC`/
    `timespec_get` (§7.27, L4908/L4948). mcc ships the full C11 freestanding
    surface — this is an advertised-version identity choice, not a missing
    feature; flipping the default is out of scope.
  - §6.7.2.1p2 (L1416) — a tagged member that declares nothing
    (`struct S { union T { int x; }; };`): the warning exists (mccgen.c:4550) but
    is gated off by `ms_extensions` (on by default) to support MS
    anonymous-by-tag members. Distinguishing an inline-defined tag from a
    referenced one would risk regressing that feature for a cosmetic,
    no-conformance-impact diagnostic.
- **Target-codegen limitation (works on arm64):** §7.16.1.4p3 / §7.15.4p3
  (L3214/L3317) — the `va_start` second-arg-not-last and register-parameter
  `-Wvarargs` checks fire on **arm64/arm** but are unreachable on **x86_64 SysV /
  i386**, where `va_start` expands to a frame-reading macro (mccdefs.h) that
  bypasses the parser hook (`check_va_start_last_param`, mccgen.c). Same target
  limitation noted in commit 06692300; correct usage stays clean on all targets.
- **Cosmetic message quality:** §6.7.9p13 (L1752) — a struct initialized from an
  incompatible scalar reports "'{' expected" rather than "incompatible type"
  (an error either way; rewording touches initializer parsing — not worth the
  regression risk).
- **`mcc:✗ gcc:✗ clang:✗` consensus** — all Annex G `_Imaginary`-type rows
  (§6.2.5p11, §6.7.2p2, §G.2–§G.7: L571/L1345/L5876–L5894/L5901/L5902/L5908/
  L5952). No compiler implements the optional imaginary types — no divergence.
- **No gcc==clang consensus** (mcc matching one side is defensible) — comma in a
  constant expression §6.6p3 (L1277, gcc rejects/clang accepts, mcc=clang);
  inline-definition constraints §6.7.4p2/p3 (L1522/L1523, gcc warns/clang
  varies); atomic-pointer `fetch_add` §7.17.7.5 (L3458, clang-only); atomic flag
  init §7.17.8.1 (L3473, gcc/clang split); `FLT_ROUNDS` after `fesetround`
  (L2470, gcc static like mcc).
- **UB / recommended-practice only** — hex-float-inexact diagnostic §6.4.4.2p7
  (L831, all three silent); incomplete generic-association §6.5.1.1p2 (L974/
  L1117, latent in all three); `__int128` optional extended type §6.2.5p4 (L563).
- **mcc permissive-by-default philosophy** (warns, errors under `-Werror`) —
  the modifiable-lvalue / const-assignment family §6.3.2.1p1, §6.5.2.4p1,
  §6.5.3.1p1, §6.5.4p2, §6.5.16.*, §6.7.3p6, §6.7.6.1p2 (L697/L1030/L1061/L1102/
  L1242/L1251/L1265/L1296/L1370/L1570/L1571/L1592); return-value mismatches
  §6.8.6.4p1 (L1883/L1884); old-style / implicit-int parameters §6.9.1p6, §6.9p1
  (L1928/L1930/L1959/L1963). mcc warns (exit 0) by default, matching its
  documented stance; gcc/clang hard-error. Counted as "accept" by C9911.
- **Optional / diagnostic-only with no consensus target** — atomic memory-order
  argument checks §7.17.7.* (L3425/L3432/L3445/L3478, all three silent at the
  source level); `#pragma STDC … BOGUS` §6.10.6p2 (L2100 — **mcc does warn under
  `-Wall`**, false-alarm regression report); FENV/FP_CONTRACT codegen effects
  §7.6.1, §F.8 (not separately observable); informative Annex I rows.
- **Not separately testable** — §5.1.2.3/§5.1.2.4 volatile & atomic ordering
  (L218/L227/L255), §7.6.1p2/p3 pragma state (L2361/L2365), §7.12.2p1 FP_CONTRACT
  contraction (L2728): all require `-S`/codegen inspection that mcc does not
  expose; observable behavior is correct.
- **Gated by the deliberate no-`__GNUC__` stance** — `<math.h>` `NAN` sign /
  `signbit` §7.12 (L2695/L2766) and `isgreater(NaN,…)` raising `FE_INVALID`
  §F.10.11 (L5868). Root cause: glibc gates its clean `NAN (__builtin_nanf(""))`
  behind `__GNUC_PREREQ(3,3)` and falls back to the trapping `(0.0f/0.0f)` for
  non-GNU compilers (gcc itself traps + prints `-nan` on that fallback — verified).
  **Landed:** mcc's `__builtin_{nan,nanf,nanl,inf,inff,infl,huge_val,huge_valf,
  huge_vall}` are now real foldable, non-trapping constant builtins (mccgen.c +
  mcctok.h; macros dropped from mccdefs.h) that match gcc exactly — clean `+nan`/
  `+inf`, usable in static initializers, no FE_INVALID/FE_OVERFLOW. The residual
  user-facing divergence would close only by predefining `__GNUC__`, a deliberate
  compiler-identity choice out of scope here (would broadly change which
  GNU-gated system-header paths mcc must support, risking the self-host fixpoint).

**Bottom line:** the 2026-06-30 verification found 34 already-closed gaps (retagged
`mcc:✓`) plus 11 partial improvements. This round then **implemented and removed
every remaining actionable item** (each with a cli/exec regression test, full
ctest 34/34, and the byte-identical self-host fixpoint kept green):

- **Diagnostics added/strengthened:** internal→external linkage clash §6.2.2p7;
  tentative-array assumed-one-element §6.9.2p3; braces-around-scalar under `-Wall`
  §6.7.9p11; static-used-never-defined §6.9p3; designated-init pedantic §6.7.9p1;
  label-at-end `-pedantic-errors` escalation §6.8.3p5; new `-Wstrict-prototypes`
  §6.11.6p1; **scanf** format-argument (pointed-to-type) checking §7.21.6.2;
  definition-time bad-`#` macro diagnostic §6.10.3.2p1.
- **Lexer:** raw-UTF-8 identifier validation against Annex D.1/D.2 §D.
- **Builtins:** `__builtin_{nan,nanf,nanl,inf,inff,infl,huge_val,huge_valf,
  huge_vall}` are now real foldable, non-trapping constant builtins matching gcc.

The flagged rows that remain are all **deliberate or not-actionable**, documented
with rationale in the Classified section above (C99 inline local-emission choice,
C99 default-std identity, `ms_extensions` tagged-member gate, x86_64 `va_start`
codegen limitation, cosmetic message wording, Annex-G `_Imaginary` consensus,
no-consensus splits, UB/recommended-practice, permissive-by-default philosophy,
not-separately-testable codegen items, and the `<math.h>` NaN behavior gated by
the deliberate no-`__GNUC__` stance).

---

WARNING!!! DO NOT DO!!! ACHTUNG!!!
• Implement/finish `-g` debugging/debugger and flesh out gdb/etc test cases
• Can a fully static build use a minimalistic `-run` to sidestep the dynamic linking limitations and use libc or musl in-memory?
• Use only human friendly warnings/errors, backed by tests that check formatted output against terminal dimensions/configuration
• CST Database for LSP and Optimization layer
• Database uses hierarchical incremental hashes to enable bidirectional lookups starting from any character index in the code
• Hot reload by saving/loading CST snapshots on the fly and on run with --hotreload arg
• Run hotreloads from reconcoliled CST snapshots
ACHTUNG!!! DO NOT DO!!! WARNING!!!
