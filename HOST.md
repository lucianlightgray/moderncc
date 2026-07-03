# HOST.md — host-gating map & the `mcchost` centralization proposal

Companion to MAC.md and WIN.md. Those two catalog the Darwin and Windows gates;
this file catalogs the **HOST axis across all platforms** (including the
Linux/BSD/POSIX gates the other docs don't own) and proposes a single
`src/mcchost.{h,c}` API that becomes the source of truth for everything
platform-gated on the machine mcc *runs on*, across all triples.

Terminology (same as WIN.md): **HOST** = the machine mcc runs on
(`_WIN32`, `__APPLE__`, `__linux__`, `__FreeBSD__`, …). **TARGET** = what mcc
emits (`MCC_TARGET_PE`, `MCC_TARGET_MACHO`, `TARGETOS_*`). `MCC_IS_NATIVE` =
both agree. Only HOST-axis code moves into `mcchost`; TARGET gates stay in the
backends.

---

## 1. Complete host-gate inventory

Every site in the compiler proper that branches on a host macro, grouped by the
service it selects. (Windows/Darwin sites already in WIN.md/MAC.md are included
so this list is complete on its own.)

### 1.1 Filesystem & path semantics
| Site | Gate | Selects |
|---|---|---|
| `src/mcc.h:117-127` | `_WIN32` | `IS_DIRSEP` (`\` too), `IS_ABSPATH` (drive letters), `PATHCMP` (`stricmp`), `PATHSEP` (`;` vs `:`) |
| `src/mcc.h:1286-1292` | `_WIN32` | `mcc_fopen`/`mcc_fclose` binary-mode + slash-normalizing wrappers (else `fopen`) |
| `src/mcc.h:83-85` | — | `O_BINARY 0` shim on POSIX |
| `src/mcc.h:228-230` | `!_WIN32` | default `CONFIG_MCCDIR` `/usr/local/lib/mcc` |
| `src/libmcc.c:54-62` | `_WIN32` | `normalize_slashes()` |
| `src/libmcc.c:445-447` | `_WIN32` | `realpath` → `_fullpath` |
| `src/libmcc.c:698-700` | `_WIN32` | normalize slashes on opened filenames |
| `src/libmcc.c:1173-1229` | `TARGETOS_OpenBSD && !_WIN32` | host `<glob.h>` scan for versioned `lib*.so.*` |
| `src/mccpp.c:1710-1714` | `_WIN32` | case-insensitive include-cache hash |
| `src/mccpp.c:1927-1929` | `_WIN32` | slash normalization in `#line`/`__FILE__` |
| `src/mccdbg.c:1078, 2448` | `_WIN32` | slash normalization in DWARF comp-dir / coverage paths |
| `src/objfmt/mccelf.c:1560-1562` | `_WIN32` | slash normalization on tcov path |
| `src/objfmt/mccpe.c:6-12` | `!_WIN32` | `stricmp`/`strnicmp` → `strcasecmp` shims, `<sys/stat.h>` vs `<process.h>` |
| `src/objfmt/mccpe.c:915-917` | `!_WIN32` | `chmod 0777` the output executable |

### 1.2 Executable self-path / install dir
| Site | Gate | Selects |
|---|---|---|
| `src/libmcc.c:64-88` | `LIBMCC_AS_DLL` + `_WIN32` | `DllMain` + `GetModuleFileNameA` mccdir discovery |
| `src/libmcc.c:90-97` | `MCC_IS_NATIVE` (Win) | `GetSystemDirectoryA` system dir |
| `src/libmcc.c:813-847` | `CONFIG_MCC_AUTO_MCCDIR && !_WIN32` | self-path: `/proc/self/exe` (`__linux__`/`__CYGWIN__`), `/proc/curproc/exe` (`__NetBSD__`), `/proc/curproc/file` (`__FreeBSD__`/`__DragonFly__`), `_NSGetExecutablePath` (`__APPLE__`), then `stat` probing |

### 1.3 Process spawn / host tools
| Site | Gate | Selects |
|---|---|---|
| `src/mcctools.c:356-359` | `_WIN32` | `SearchPath` to locate a DLL |
| `src/mcctools.c:414-453` | `_WIN32` | `quote_win32` + `_spawnvp`/`_cwait` (`execvp` replacement) |
| `src/mcctools.c:467-469` | `_WIN32` | `.exe` suffix on cross-tool names |
| `src/mcctools.c:480-485` | `_WIN32` | `_CRT_glob`/`_dowildcard` CRT wildcard globals |
| `src/objfmt/mccpe.c:597-628` | `_WIN32` both ways | cv2pdb: `_spawnvp` vs `pe_shell_quote` + `system()` |
| `src/objfmt/mccmacho.c:2269-2279` | `CONFIG_CODESIGN` | `system("codesign -f -s - …")` on the output |

### 1.4 Executable memory / JIT (`-run`)
| Site | Gate | Selects |
|---|---|---|
| `src/mccrun.c:49-51, 65-74` | `_WIN32`, `__APPLE__` | `<sys/mman.h>`; pagesize via `_SC_PAGESIZE`/`getpagesize`; `<libkern/OSCacheControl.h>` |
| `src/mccrun.c:100-171` | `_WIN32` | `VirtualAlloc`/`VirtualFree` vs `malloc`+`mprotect`; `FreeLibrary` vs `dlclose`; `win64_del_function_table` |
| `src/mccrun.c:262-268` | `__APPLE__` | `CONFIG_RUNMEM_RO` (W^X toggle required on Darwin) |
| `src/mccrun.c:363-427` | `_WIN32` | `VirtualProtect` vs `mprotect` + `__clear_cache` (arm/arm64/riscv) |
| `src/mcc.h:967-969` | `_WIN64` | `run_function_table` SEH unwind-table state |

### 1.5 Fault handling / signal context (the worst cascade)
| Site | Gate | Selects |
|---|---|---|
| `src/mccrun.c:1118-1125` | `_WIN32`, `__OpenBSD__` | `<signal.h>`/`<sys/ucontext.h>` vs `ucontext_t = CONTEXT` |
| `src/mccrun.c:1127-1215` | full OS×arch matrix | `rt_getcontext`: pc/fp/sp extraction for `_WIN64`+arm64, `_WIN64`, `_WIN32`, and {`__i386__`,`__x86_64__`,`__arm__`,`__aarch64__`,`__riscv`} × {`__APPLE__`, `__FreeBSD__`/`__DragonFly__`, `__NetBSD__`, `__OpenBSD__`, `__dietlibc__`, glibc} |
| `src/mccrun.c:1217-1329` | `_WIN32`/`_WIN64` | POSIX `sigaction` handlers vs SEH `AddVectoredExceptionHandler`/`SetUnhandledExceptionFilter` |
| `src/mccrun.c:1350` | `__arm__ && !_WIN32` | arm frame-walk variant of `rt_get_caller_pc` |

### 1.6 Dynamic loading
| Site | Gate | Selects |
|---|---|---|
| `src/mcc.h:25-33` | `!_WIN32` | `<dlfcn.h>`, `<unistd.h>` |
| `src/mcc.h:1776-1788`, `src/mccrun.c:1424-1438` | `CONFIG_MCC_STATIC` | stub `dlopen`/`dlsym`/`dlerror` for static host builds |
| `src/libmcc.c:1093-1134` | `MCC_IS_NATIVE` (per target-format) | `dlopen(RTLD_GLOBAL\|RTLD_LAZY)` of ELF `.so` / Mach-O dylib+tbd for `-run` |
| `src/objfmt/mccelf.c:1000-1008` | `MCC_IS_NATIVE && !MCC_TARGET_PE` | `dlsym(RTLD_DEFAULT)` process-global symbol resolution |
| `src/objfmt/mccpe.c:1033-1042` | `MCC_IS_NATIVE` | `LoadLibraryA`+`GetProcAddress` import resolution for `PE_RUN` |

### 1.7 Threading / locks / time / environ
| Site | Gate | Selects |
|---|---|---|
| `src/mcc.h:1903-1949` | `_WIN32` / `__APPLE__` / else | `MCCSem`: `CRITICAL_SECTION` / `dispatch_semaphore_t` / POSIX `sem_t` |
| `src/mcc.c:270-279` | `_WIN32` | `GetTickCount` vs `gettimeofday` ms clock |
| `src/mccrun.c:182-190` | `__APPLE__` / BSDs / else | `_NSGetEnviron()` vs `extern char **environ` |

### 1.8 Native toolchain services (host SDK / linker discovery)
| Site | Gate | Selects |
|---|---|---|
| `src/objfmt/mccmacho.c:2296-2347` | `MCC_IS_NATIVE` | `dlopen("libxcselect.dylib")` SDK query + hardcoded CommandLineTools/Xcode.app fallbacks for lib & include paths |
| `src/objfmt/mccelf.c:120-122` | `MCC_IS_NATIVE && TARGETOS_BSD` | ELF interp override from `getenv("LD_SO")` |

### 1.9 Host-compiler shims (textual, header-only)
`src/mcc.h:35-115`: `<windows.h>`/`<io.h>`/`<direct.h>` includes, `inline`/
`snprintf` remaps, MSVC `strtoll`/`strtof` shims, `LIBMCCAPI` dllexport,
warning disables, `_M_ARM64`→`__aarch64__`, `va_copy`, `NORETURN`/`ALIGNED`/
`PRINTF_LIKE`/`FALLTHROUGH`, clang `offsetof`. Plus
`src/mccgen.c:2545-2548` (`_MSC_VER && __x86_64__` volatile long-double
workaround).

### 1.10 Outside the compiler proper (related, but separate axes)
- **`runtime/lib/*.c`, `runtime/include/*.h`** — gated on the OS the *compiled
  program* runs on (bcheck.c, alloca*.c, atomic.c, tcov.c, runmain.c, bt-*.c,
  mccdefs.h, float.h, stddef.h, stdint.h). These are compiled *by* mcc *for*
  the target and can never call mcc's host layer. Out of scope.
- **`tests/{cli,exec,diff3}/runner.c`, `tests/embed/*.c`** — host gates
  (`_mkdir`/`_popen`/quoting) in standalone test binaries. Candidate for a tiny
  shared `tests/support/hostcompat.h`, but not for `mcchost` (they don't link
  libmcc internals).
- **`CMakeLists.txt` / `cmake/`** — host probes (`sw_vers`, `cc -dumpmachine`,
  musl detect, `-lm -lpthread -ldl` selection, `MCC_TARGETOS` derivation).
  Stays in CMake; after centralization its only job on the C side is defining
  the `MCC_TARGET_*`/`TARGETOS_*`/`CONFIG_*` macros that `mcchost.h` consumes.
- **`tools/c2str.c:4-23`** — host→target macro substitution table for header
  generation; build tool, leave as is.

---

## 2. Proposed files and naming

**Recommended: `src/mcchost.h` + `src/mcchost.c`.** "host" is already the
codebase's established vocabulary for this axis (WIN.md/MAC.md), and it matches
the flat `mcc*.c` naming of `src/`. Alternatives considered:

- `mccos.c` — shorter, but ambiguous: "OS" could mean the *target* OS
  (`TARGETOS_*` is a target macro family here). Rejected.
- `mccnative.c` — collides with `MCC_IS_NATIVE`, which means host==target, a
  narrower concept than "host glue". Rejected.
- `mccsys.c` / `mccport.c` — vague; "port" suggests target porting.
- `src/host/host-win32.c`, `host-posix.c`, `host-darwin.c` + shared header —
  the right *eventual* shape if the file grows past ~2000 lines, but a poor
  starting point: three files to keep in signature-lockstep, and it breaks the
  flat ONE_SOURCE pattern. Start with one `.c` using clearly fenced
  per-section `#if` blocks; split later if needed, keeping `mcchost.c` as the
  umbrella that `#include`s the per-OS pieces so the build interface never
  changes.

Integration: `mcchost.h` is included at the top of `mcc.h` (it replaces
`mcc.h:25-127`); `mcchost.c` joins the ONE_SOURCE include list in
`libmcc.c` and `MCC_CORE_SRC` in CMake, compiled unconditionally for every
triple (cross compilers still need paths, spawn, clocks, self-path — the
JIT/fault sections inside it compile only under `MCC_IS_NATIVE`, as today).

---

## 3. The API

Prefix: `host_` for functions (internal `ST_FUNC` linkage), `HOST_` for
macros/constants. Design rule: **anything with runtime behavior is a real
function in `mcchost.c`; only textual/type shims and constant predicates stay
as macros in `mcchost.h`.** That keeps host `#if`s out of every other
translation unit and gives one place to breakpoint/instrument.

```c
/* ---- mcchost.h (sketch) ------------------------------------------- */

/* Normalized host identity — always defined, 0 or 1.  The ONLY place
   host macros are interpreted; everything else tests these. */
#define MCC_HOST_WIN32   0|1     /* from _WIN32   */
#define MCC_HOST_DARWIN  0|1     /* from __APPLE__ */
#define MCC_HOST_LINUX   0|1
#define MCC_HOST_BSD     0|1     /* Free/Net/Open/DragonFly */
#define MCC_HOST_POSIX   (!MCC_HOST_WIN32)

/* Compiler shims (moved verbatim from mcc.h:25-127): NORETURN, ALIGNED,
   PRINTF_LIKE, FALLTHROUGH, O_BINARY, va_copy, inline/snprintf remaps,
   MSVC arch-macro normalization, LIBMCCAPI. */

/* -- paths (macros: must work in constant contexts) ----------------- */
#define HOST_PATHSEP        ";" | ":"
#define HOST_IS_DIRSEP(c)   ...
#define HOST_IS_ABSPATH(p)  ...
#define HOST_PATHCMP        stricmp | strcmp
ST_FUNC char *host_path_normalize(char *path);      /* '\'->'/'; no-op POSIX  */
ST_FUNC char *host_path_canonical(const char *p, char *buf); /* realpath/_fullpath */
ST_FUNC int   host_path_hash_fold(int c);           /* include-cache case fold */
ST_FUNC FILE *host_fopen(const char *path, const char *mode);  /* binary mode */
ST_FUNC void  host_set_exec_bits(const char *file); /* chmod 0777; no-op Win  */

/* -- self location --------------------------------------------------- */
ST_FUNC int host_exe_path(char *buf, int size);
/* /proc/self/exe | /proc/curproc/{exe,file} | _NSGetExecutablePath
   | GetModuleFileNameA — one function, every strategy inside          */
ST_FUNC int host_system_dir(char *buf, int size);   /* Win native; else -1   */

/* -- process ---------------------------------------------------------- */
ST_FUNC int host_spawn_wait(const char *const argv[]);
/* argv-vector spawn with correct per-OS quoting; replaces execvp_win32,
   pe_shell_quote+system(cv2pdb), and system(codesign)                  */
ST_FUNC int host_exec_replace(char *const argv[]);  /* execvp semantics      */
ST_FUNC int host_find_tool(const char *name, char *buf, int size);
#define HOST_EXE_SUFFIX  ".exe" | ""

/* -- runnable memory (compiled only under MCC_IS_NATIVE) -------------- */
ST_FUNC size_t host_pagesize(void);
ST_FUNC void  *host_runmem_alloc(size_t size);
ST_FUNC void   host_runmem_free(void *ptr, size_t size);
enum { HOST_PROT_NONE, HOST_PROT_R, HOST_PROT_RW, HOST_PROT_RX, HOST_PROT_RWX };
ST_FUNC int    host_runmem_protect(void *p, size_t sz, int prot);
ST_FUNC void   host_icache_flush(void *p, size_t sz);
#define HOST_RUNMEM_RO  0|1        /* Darwin W^X: writes need a toggle  */
ST_FUNC void   host_unwind_register(void *base, void *table, int count);
ST_FUNC void   host_unwind_unregister(void *table);   /* Win64 SEH; no-op */

/* -- faults / backtrace (MCC_IS_NATIVE) ------------------------------- */
typedef struct HostFaultRegs { addr_t pc, fp, sp; } HostFaultRegs;
typedef int (*host_fault_fn)(void *osctx, int signum, void *addr);
ST_FUNC void host_fault_install(host_fault_fn fn);
/* sigaction | SEH vectored handler — one entry point                   */
ST_FUNC int  host_fault_regs(void *osctx, HostFaultRegs *r);
/* the entire mccrun.c:1127-1215 OS x arch matrix lives behind this     */

/* -- dynamic loading --------------------------------------------------- */
ST_FUNC void       *host_dlopen(const char *name);
ST_FUNC void       *host_dlsym(void *h, const char *sym);
ST_FUNC void       *host_dlsym_process(const char *sym); /* RTLD_DEFAULT   */
ST_FUNC void        host_dlclose(void *h);
ST_FUNC const char *host_dlerror(void);
/* CONFIG_MCC_STATIC stubs live inside mcchost.c, not scattered          */

/* -- locks / time / env ------------------------------------------------ */
typedef ... HostSem;   /* CRITICAL_SECTION | dispatch_semaphore_t | sem_t */
ST_FUNC void host_sem_init(HostSem *s);   /* + wait / post / destroy      */
ST_FUNC unsigned host_clock_ms(void);
ST_FUNC char **host_environ(void);

/* -- native toolchain services (MCC_IS_NATIVE; no-op/-1 elsewhere) ----- */
ST_FUNC const char *host_macos_sdk_root(void);      /* libxcselect + fallbacks */
ST_FUNC int  host_codesign_adhoc(const char *file); /* Darwin; 0 no-op else    */
ST_FUNC const char *host_elf_interp_override(void); /* getenv("LD_SO") on BSD  */
```

### Backend cleanliness this buys

The two object-format writers stop being host-aware entirely:

- `mccmacho.c` calls `host_codesign_adhoc()`, `host_macos_sdk_root()`,
  `host_dlopen/host_dlsym` — its `CONFIG_CODESIGN` block, xcselect dance, and
  hardcoded SDK paths move to `mcchost.c`.
- `mccpe.c` calls `host_spawn_wait()` for cv2pdb (killing `pe_shell_quote` +
  `system()`), `host_set_exec_bits()`, `host_dlsym`-family for `PE_RUN`
  imports, and the `stricmp` shim moves to the header.
- `mccelf.c` calls `host_dlsym_process()` and `host_elf_interp_override()`.
- `mccrun.c` shrinks to orchestration: every `#ifdef` in it today is host glue
  that becomes a `host_*` call.
- `src/arch/**` already contains **zero** host gates (verified) — the
  invariant below is cheap to adopt now.

### The invariant (and how to enforce it)

> Outside `src/mcchost.h` / `src/mcchost.c`, no file under `src/` may test
> `_WIN32`, `_WIN64`, `_MSC_VER`, `__MINGW*`, `__CYGWIN__`, `__APPLE__`,
> `__linux__`, `__FreeBSD__`, `__NetBSD__`, `__OpenBSD__`, `__DragonFly__`,
> `__ANDROID__`, or `__dietlibc__`. Backends may test only `MCC_TARGET_*`,
> `TARGETOS_*`, `MCC_IS_NATIVE`, and `CONFIG_*`.

Enforce with a trivial CI check (a `grep -rE` over `src/` excluding
`mcchost.*` added as a ctest); that is what keeps the file the *source of
truth* rather than a convention that erodes.

What deliberately stays put:
- `MCC_IS_NATIVE` derivation and host→default-target bridging
  (`mcc.h:149-170`) — needs both axes; it stays in `mcc.h`, but is the *only*
  host-macro use left there (whitelisted in the CI check, or moved into
  `mcchost.h` since host identity is its business).
- All TARGET gates (ABI, relocs, `leading_underscore`, `wchar_t` model,
  section names) — backend business, untouched.
- `runtime/`, `tests/`, `tools/c2str.c`, CMake — separate axes (§1.10).

---

## 4. Migration plan (each phase independently green)

1. **Header split.** Create `mcchost.h`; move `mcc.h:25-127` shims + path
   macros + the semaphore block; `mcc.h` includes it first. No behavior change.
2. **Stateless leaf functions.** `host_path_normalize` (replaces the 6
   `normalize_slashes` sites), `host_fopen`, `host_path_canonical`,
   `host_clock_ms`, `host_environ`, `host_pagesize`, `host_set_exec_bits`,
   `HOST_EXE_SUFFIX`. Delete the per-file copies.
3. **Self-path.** Collapse `libmcc.c:64-97` + `813-847` into
   `host_exe_path()`; `mcc_auto_mccdir` and the Win32 DLL path share it.
4. **Process spawn.** `host_spawn_wait`/`host_exec_replace`/`host_find_tool`;
   port `mcctools.c` impdef/cross-spawn, then cv2pdb, then codesign. (Also
   fixes the quoting asymmetry: codesign currently interpolates a filename
   into `system()` unquoted.)
5. **JIT memory + faults.** The big one: move `mccrun.c` memory management,
   protection, icache, SEH tables, `rt_getcontext` matrix, and handler
   installation behind `host_runmem_*`/`host_fault_*`.
6. **Dynamic loading + native services.** `host_dl*`, static-build stubs,
   `host_macos_sdk_root`, `host_elf_interp_override`; scrub `mccelf.c` /
   `mccpe.c` / `mccmacho.c` of host macros.
7. **CI guard.** Add the grep invariant test; update WIN.md/MAC.md host rows
   to point here.

Porting payoff: bringing up a new host (e.g. `__sun`, Haiku, a new libc) or a
new native arch's signal context becomes a diff to exactly one file, and every
host behavior (quoting, W^X, case folding) is testable through one seam.

---

## 5. Implementation status (implemented)

The plan above is implemented; `src/mcchost.h` + `src/mcchost.c` exist and
the `host-gate-invariant` ctest (`cmake/host_gate_check.cmake`) enforces the
invariant of §3 for every `#if/#ifdef/#ifndef/#elif` under `src/` (string
literals such as mccpp.c's target-predefine table are exempt by design).

Deltas from the §3 sketch, decided during implementation:

- **Path macros** kept their semantics but the call sites were renamed:
  `HOST_PATHSEP`, `HOST_PATHCMP`, `HOST_IS_DIRSEP`, `HOST_IS_ABSPATH`.
  `mcc_fopen`/`mcc_fclose` were renamed to `host_fopen`/`host_fclose`.
- `host_path_canonical(path)` returns a libc-malloc'd string (freed with
  `libc_free`), matching how `normalized_PATHCMP` consumed `realpath`.
- `host_find_tool(name, ext, buf, size)` takes the extension (`".dll"`)
  as a parameter; returns 1/0.
- `host_spawn_wait` quotes argv per host itself (fork/execvp on POSIX —
  no shell), which also fixed the unquoted `system("codesign …")` and the
  unquoted Win32 cv2pdb spawn; `host_codesign_adhoc()` wraps the
  CONFIG_CODESIGN policy so `mccmacho.c` calls it unconditionally.
- `host_runmem_alloc(&size, &ptr_diff)` owns the CONFIG_SELINUX double
  mapping, VirtualAlloc, and malloc+page-slack strategies; `HOST_PROT_*`
  values 0–3 deliberately match `mcc_relocate_ex`'s section-class
  arithmetic (rx, ro, rw, rwx). `HOST_RUNMEM_RO` honors a user-set
  `CONFIG_RUNMEM_RO`. `host_icache_flush` is called from
  `host_runmem_protect` for RX/RWX transitions.
- `host_unwind_register(table, size_bytes, base)` takes the .pdata byte
  size (RUNTIME_FUNCTION's size differs between x64 and arm64, so the
  division lives inside mcchost). `MCCState.run_function_table` is now an
  unconditional field (NULL outside Win64).
- **Faults:** mcchost owns handler installation, the pc/fp/sp extraction
  matrix (`host_fault_regs`), and signal unblocking
  (`host_fault_unblock`). mccrun.c registers one normalized callback
  (`rt_fault`) via `host_fault_install`, keyed by `HOST_FAULT_*` codes;
  `HOST_FAULT_OTHER_FMT` carries the per-host "caught signal/exception"
  message. Returning nonzero from the callback means "continue search"
  (Windows breakpoint/single-step).
- The CONFIG_MCC_STATIC dl stubs and the built-in `mcc_syms` table moved
  from mccrun.c into mcchost.c behind `host_dl*`; `RTLD_*` no longer
  leaks outside mcchost.
- `host_macos_sdk_root()` returns a cached static string (or NULL); the
  two hardcoded CommandLineTools/Xcode.app fallback *path lists* stayed in
  mccmacho.c — they are plain strings with no host gate.
- `runtime/lib/bt-exe.c` now includes `../mcchost.c` before `../mccrun.c`;
  under CONFIG_MCC_BACKTRACE_ONLY mcchost.c compiles only the fault
  section, exactly as mccrun.c compiles only its backtrace half.
- Additional shims that surfaced during the sweep: `MCC_HOST_WIN64` (for
  mccpe.c's IMAGE_OPTIONAL_HEADER32 pick on Win64 hosts),
  `HOST_VOLATILE_LDOUBLE` (the MSVC x64 long-double workaround from
  mccgen.c), `HOST_MPROTECT_FAILMSG`, `HOST_EXE_SUFFIX`, the
  `stricmp`/`strnicmp` POSIX shims, and `src/formats/elf.h` now tests
  `MCC_HOST_WIN32`. The `_CRT_glob`/`_dowildcard` CRT globals stayed in
  mcctools.c (they must live in the *driver*, not the library) gated on
  `MCC_HOST_WIN32`.
- `MCC_IS_NATIVE` derivation stayed in mcc.h but now tests
  `MCC_HOST_WIN32`/`MCC_HOST_DARWIN`, so mcc.h needs no whitelist in the
  CI check.
