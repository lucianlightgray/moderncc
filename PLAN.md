# PLAN Part 0 — Phased porting plan: non-C surface → in-tree C tools

**Status:** approved for execution (2026-07-03). This part is self-contained: everything needed
to execute it is stated here, with `file:line` references into the tree as of branch `mob`,
July 2026 (line numbers drift as files are edited; the non-C file index below this part
describes the same revision). Execute phases in order; each phase is independently landable.

## 0.1 Goal

Port as much of the repository's non-C surface (CMake logic, embedded `cmake -P` test drivers,
CI shell, packaging glue) as reasonably possible into in-tree C tools under `tools/`, backed by
enhancements to the `mcchost` host-abstraction API — so that build, test, and release paths are
normalized across hosts, external tool dependencies shrink (`otool`, `file(1)`, `rsync`, `sed`,
`cp`, `mkdir`, shell), and logic that today exists in 2–7 copies across CMake/YAML/shell exists
once, in C.

Rough scope: of ~7,000 lines of build/test/CI *logic* indexed below (excluding fixtures, `.def`
data, docs), about two-thirds is portable. What legitimately stays external: CMake's
cache/GUI/install/export/CPack machinery, `CMakePresets.json` (declarative schema), GitHub
Actions orchestration (checkout/artifacts/release), and the execution substrates themselves
(docker, qemu-user, wine, macOS/Xcode toolchain, reference gcc/clang used as differential
oracles).

## 0.2 Governing principle: DRY

**Every phase must reduce the number of places a fact or pattern lives; no phase may add a new
copy.** Concretely:

- A pattern ported to C must be *deleted* from its CMake/shell sites in the same change, or the
  CMake site must become a one-line invocation of the C tool. Never leave a C port and a CMake
  implementation of the same logic coexisting "temporarily".
- New C code shares helpers: one spawn wrapper, one glob walker, one stderr-line filter, one
  reference-compiler resolver — *not* per-tool re-implementations. If two tools need it, it goes
  in the shared tool-support layer (§0.5), not copy-pasted.
- Facts get one authoritative home. The config→defines mapping, the preset/matrix names, the
  skip-label taxonomy, the source-tree exclusion filter, the `.def` export sets — each must end
  the migration with exactly one source of truth, with all other appearances derived or deleted.
- The duplication catalog in §0.4 is the checklist: a phase touching any listed pattern must
  collapse **all** listed sites of that pattern, not just the convenient ones.

## 0.3 Baseline — what already exists in-tree (build on this, do not reinvent)

**Public embed API (`include/libmcc.h`)** — the only *linkable* surface: `mcc_new/mcc_delete`,
`mcc_set_options` (full CLI grammar as a string), `mcc_add_file`, `mcc_compile_string`,
`mcc_set_output_type` (`MCC_OUTPUT_MEMORY/EXE/DLL/OBJ/PREPROCESS/ASM`), `mcc_output_file`,
`mcc_run`, `mcc_relocate`, `mcc_get_symbol`, `mcc_list_symbols`, `mcc_set_backtrace_func`.
A C tool can drive full compilations in-process without spawning `mcc` at all.

**Driver verbs (`src/mcc.c`, parsing in `src/libmcc.c`)** — `-c`, `-S`, `-E` (+`-P/-dD/-dM`),
`-run`, `@listfile`, `-M/-MM/-MD/-MMD/-MF/-MP` (Makefile deps via `gen_makedeps`,
`src/mcctools.c:455`), `-ar` (`mcc_tool_ar`, `src/mcctools.c:26`; ELF-only, ops `crstvx`),
`-impdef` (`mcc_tool_impdef`, `src/mcctools.c:301`; PE targets only), `-print-search-dirs`,
`-vv`, `-bench`.

**Host layer (`src/mcchost.{h,c}`)** — the HOST-axis abstraction; invariant (enforced by ctest
`host-gate-invariant` via `cmake/host_gate_check.cmake`): outside `mcchost.{h,c}` no `src/`
file tests raw host macros. Existing families: paths (`host_path_normalize/canonical`,
`host_fopen/fclose`, `host_set_exec_bits`), self-location (`host_exe_path`,
`host_system_dir`), spawn (`host_spawn_wait(argv)` — fork/execvp | `_spawnvp`, **exit code
only**; `host_exec_replace`; `host_find_tool(name,ext,buf,size)` — **Windows `SearchPath`
only, POSIX stub returns 0**; `host_codesign_adhoc`), time/env (`host_clock_ms`,
`host_environ` — read-only), dlopen family, runnable-memory/JIT, fault handlers, `HostSem`.

**Linkage constraint:** everything in `mcchost.c`/`mcctools.c` is `ST_FUNC` (static under
`ONE_SOURCE`) — *not* exported from libmcc. Precedent for tools: `runtime/lib/bt-exe.c` does
`#include "../mcchost.c"`. Tools either amalgamate the same way or use the new tool-support
layer (§0.5). Tools must respect the host-gate invariant: no raw `_WIN32`/`__APPLE__` tests in
`tools/` either — extend mcchost instead.

**Existing tools & precedents:**
- `tools/build.c` (91 lines; CMake target `mccbuild`, `CMakeLists.txt:2683`): standalone
  bootstrap builder — `system()`-shells cc to build `src/mcc.c` with `-DONE_SOURCE=1` and a
  hardcoded native-x86_64 `-D` set (L42–54), `cp`s runtime headers, then drives the fresh `mcc`
  to compile `runtime/lib/*.c` and `mcc -ar` them into `libmcc1.a`. It already re-implements,
  for one target, CMakeLists' `_mccdefs` assembly (L1515), header staging (L2396), and the
  `mcc_build_libmcc1` recipe (L2230). This is the seed for Phase 4.
- `tools/c2str.c` (151 lines): build-time codegen (`mccdefs.h` → embedded string), wired at
  `CMakeLists.txt:1699–1727` with a host-cc cross branch.
- `tests/support/seccmp.c`: self-contained C ELF parser (both ELFCLASS) comparing section
  bytes — the accepted "parse object files in C, don't shell to readelf" precedent.
- `src/objfmt/mccmacho.c` (2,503 lines): contains a **complete Mach-O reader** —
  FAT/`MH_MAGIC_64` detection (`:2402,2430`), load-command walk by `cmdsize` (`:2434–2477`),
  `LC_BUILD_VERSION`/`minos` (`:87,247,1710–1711`). `src/objfmt/mccpe.c:58–135` has the full
  PE header structs. `src/objfmt/mccelf.c` likewise for ELF.
- Test runners `tests/exec/runner.c`, cli/diff3 runners: already C; the cmake `-P` wrappers
  around them are spawn+glob+compare shims.
- `mcc -impdef` already regenerates `libmcc.def` in-build (`CMakeLists.txt:2594–2601`) through
  `${MCC_EMULATOR}` — proof of the DLL→`.def` pipeline.
- Config-node doc generator `mcc_generate_node_doc` (`CMakeLists.txt:399–432`, invoked at
  `:1511`) renders the `mcc_config_node()` metadata to `${CMAKE_BINARY_DIR}/config-nodes.md` —
  the same metadata `docs/BUILD.md`'s tables duplicate by hand.

**Load-bearing contracts — preserve exactly:**
- ctest skip convention: driver exits **77** = skip (`SKIP_RETURN_CODE 77`), and the
  `mcc_skip_test` `SKIP:` regex pattern.
- ctest labels `qemu` / `wine` / `macho` — the presets' test filters
  (`filter.exclude.label = "qemu|wine|macho"`) depend on them.
- The host-gate invariant (above).
- `.def` file shape: `LIBRARY <dll>` / blank / `EXPORTS` / bare names (no ordinals/DATA) —
  exactly what `-impdef` emits (`src/mcctools.c:375–379`).
- `mcc -ar` unsupported ops (`h a b d i o p N`) and ELF-only limits; `-impdef` PE-only.
- `_mccdefs` semantics: the quote-free `_mccdefs_flags` variant feeds libmcc1 compilation.

## 0.4 Cross-domain duplication catalog (the DRY checklist)

Each entry lists the pattern, today's sites, and the single home it must end in.

1. **Resolve a reference compiler** (find genuine GNU gcc, reject clang-masquerading, prefer
   downloaded winlibs/clang, identify via `--version`/`-dumpversion`): `CMakeLists.txt:14–36`
   (macOS deployment target), `:193–214` (`mcc_find_gnu_gcc`), `:2932–2970` (diff3),
   `:3341–3377` (mcctest ref), `:3672` (i386-fastcall); plus `ci.yml:84–91` (brew gcc
   version-glob) and `ci.yml:120–127` (pwsh diff3 discovery). → one `resolve_reference_cc()`
   in the tool-support layer.
2. **Probe the host cc** (`--version`, `-dumpmachine`, `-dumpversion`, `-dM -E -`):
   `CMakeLists.txt:14`, `:1295`, `:1355`, `:1308` (ARM ABI), `:1551` (musl), `:3368`. → one
   `cc_probe()`.
3. **Compile → run → diff** (the differential-driver skeleton): mcctest, mcc_exe,
   asm-c-connect, dash-S roundtrip, preprocess 3-way `-E`, i386-fastcall, gcctestsuite,
   arm-asm, pe-native, pe-wine, macho×4, qemu-run, `tests/diff/run_parts.cmake`. → one
   harness binary (Phase 3).
4. **Spawn under emulator/launcher prefix** (`${MCC_EMULATOR}`, `qemu-<arch> -L <sysroot>`,
   wine + `WINEDEBUG/WINEPREFIX` env): `CMakeLists.txt:2324`, `:2411`, `:2748`, `:3973`,
   `:4306`, `:4001`. → launcher-prefix argv in `host_spawn_ex` (Phase 1).
5. **Glob-and-copy staging** (runtime `*.h`, win32 `*.h`+`*.def`, install trees, test globs):
   `CMakeLists.txt:2396`, `:2403–2408`, `:2665–2680`, `:1734/1738`, `:3156/3710/4295`. →
   `host_dir_walk` + `host_copy_file` (Phase 1) used by mccbuild/harness.
6. **Download → SHA256 → extract → stamp**: mingw/clang toolchains (`CMakeLists.txt:463–678`)
   and qemu Gentoo stage3 (`:4243–4286`). → stays in CMake (HTTPS is the blocker), but both
   sites must share one generated fetch-script template; optional Phase 6 moves hash+extract
   to C.
7. **Git stamp** (`git rev-parse`/`log`/`diff --quiet`): `CMakeLists.txt:1673–1693` (githash
   define) and `:4516–4527` (`dist`). → one spawn-capture git helper.
8. **Config → `-D` define list** (`_mccdefs`): `CMakeLists.txt:1515–1654` and duplicated in
   `tools/build.c:41–54`. → one C routine in mccbuild (Phase 4); CMake keeps only cache
   plumbing.
9. **Source-tree exclusion filter**: `CMakeLists.txt:4501` (tags) and `:4547–4549` (CPack
   source ignore); overlapping exclude list in `tests/ci/docker/run-ci.sh:29–48` and
   `.gitignore`. → one list, consumed everywhere.
10. **Mach-O type check, three inconsistent ways**: `file(READ LIMIT 4 HEX)` in
    `validate_macho.cmake:72` + `run_macho_apple_libc.cmake`; external `file(1)` in
    `run_macho_image.cmake` + `run_macho_native.cmake`; `otool -l` parse in
    `validate_macho.cmake:77–105`. → `tools/objcheck.c` over `mccmacho.c` (Phase 2).
11. **Configure→build→test→install sequence**: hand-repeated 7× — `run-ci.sh:56–69`,
    `ci.yml:92–101` (macos), `:128–137` (msvc), `:155–162` (mingw), `:204–210` (dist),
    `release.yml:41–49`, `:68–94` — with three different nproc idioms (`nproc`,
    `sysctl -n hw.ncpu`, `getconf _NPROCESSORS_ONLN`). → one `tools/ci.c` verb (Phase 5).
12. **Preset/matrix names**: every matrix row in `ci.yml`/`release.yml` duplicates a preset
    name in `CMakePresets.json`; PLAN notes ci.yml's `dist` job "mirrors what release.yml
    does". → matrix enumerated *from* `CMakePresets.json` (Phase 5).
13. **First-meaningful-stderr-line extraction** (filter warnings, find
    `undefined reference|undefined symbol|error:`): `run_macho_codegen.cmake:176–183`,
    `run_macho_image`/`apple_libc` (skip `stack`/`deprecat`), `validate_macho.cmake:59–64`
    (reclassify as skip). → one helper in the harness.
14. **Multi-candidate find_program** (`pick()` in `run_pe_wine.cmake:18–27`, otool probing in
    `validate_macho.cmake:20`, inline in `run_macho_codegen`). → candidate-list variant of
    `host_find_tool` (Phase 1).
15. **BUILD.md node tables vs `mcc_generate_node_doc`**: same `mcc_config_node()` metadata
    rendered twice (hand + generated). → generator emits the table section of `docs/BUILD.md`
    (Phase 2).

## 0.5 Phase 1 — mcchost build-tool primitives (enables everything else)

New API in `src/mcchost.{h,c}` (host-axis; keep `ST_FUNC`; tools reach it via a new
`tools/toolhost.h` that `#include`s `mcchost.c` bt-exe-style, plus a `tools/toolsupport.{h,c}`
for tool-only helpers that don't belong in the compiler's host layer):

```c
/* spawn with control — the keystone. All fields optional (NULL/0 = inherit). */
typedef struct HostSpawnOpts {
    const char *const *launcher;   /* emulator/wine/qemu prefix argv, NULL-terminated */
    const char *cwd;
    const char *const *env;        /* full replacement env, or NULL = inherit */
    const char *stdout_file;       /* redirect stdout to file */
    const char *stderr_file;       /* redirect stderr to file (may equal stdout_file) */
    char **stdout_buf, **stderr_buf; /* capture into libc-malloc'd buffers */
} HostSpawnOpts;
ST_FUNC int  host_spawn_ex(const char *const *argv, const HostSpawnOpts *o); /* exit code | -1 */

/* host_find_tool gains a real POSIX PATH search; add first-of-N candidates */
ST_FUNC int  host_find_tool_any(const char *const *names, const char *ext, char *buf, int size);

/* filesystem kit */
ST_FUNC int  host_mkdirs(const char *path);                    /* mkdir -p */
ST_FUNC int  host_copy_file(const char *src, const char *dst, int preserve_exec);
ST_FUNC int  host_stat(const char *path, int *is_dir, long long *size, long long *mtime);
typedef int (*host_walk_fn)(const char *path, int is_dir, void *ud);
ST_FUNC int  host_dir_walk(const char *dir, int recursive, host_walk_fn fn, void *ud);
```

In `tools/toolsupport.{h,c}` (shared by all tools, never per-tool copies):
glob matching over `host_dir_walk`, byte file-compare, `resolve_reference_cc()`, `cc_probe()`,
first-meaningful-stderr-line, exit-77 skip helper, small string/regex utilities.

Also in Phase 1: change `tools/build.c` to use the new kit instead of `system("mkdir -p")` /
`cp` / `system(cc ...)` — it becomes the first consumer and the proof the API is sufficient.

**Acceptance:** `mccbuild` runs shell-free (no `system()` of external commands other than the
compiler itself via `host_spawn_ex`); host-gate invariant still passes (tools included).

## 0.6 Phase 2 — zero-new-dependency wins

1. **`tools/objcheck.c`** — object-structure validator reusing the compiler's own readers:
   Mach-O mode (magic incl. FAT, load-command walk, `LC_BUILD_VERSION.minos` extraction —
   all already in `mccmacho.c:2402–2478`), PE mode (`MZ`/`PE\0\0`, machine, subsystem via
   `mccpe.c:58–135` structs), ELF already served by `seccmp`. Replaces: `otool`/`llvm-otool`
   in `tests/qemu/validate_macho.cmake` (including the `minos 12.3.1` regex subtest),
   `file(1)` in `run_macho_image.cmake`/`run_macho_native.cmake`, and the `file(READ HEX)`
   magic checks (catalog #10). Delete all three CMake variants; drivers call `objcheck`.
2. **`tools/hostgate.c`** — port `cmake/host_gate_check.cmake` (73 lines: glob `src/**.{c,h}`,
   scan `#if/#ifdef/#ifndef/#elif` lines for 15 banned raw host macros outside
   `mcchost.{h,c}`, report `file:line`, nonzero exit). C version should tokenize conditionals
   properly (the cmake regex at `:57` worries about string-literal false positives). Retire
   the `.cmake` file; ctest `host-gate-invariant` invokes the tool.
3. **`run_dash_s_bytes`** — fold the 35-line `tests/asm/run_dash_s_bytes.cmake` (3 spawns +
   `seccmp`) into `seccmp` itself or a 30-line driver in `tools/`; delete the `.cmake`.
4. **BUILD.md table generation** — extend `mcc_generate_node_doc` (or port it to
   `tools/genconfigdoc.c` reading node metadata exported by CMake) to regenerate the node
   tables inside `docs/BUILD.md` between markers, leaving prose hand-written (catalog #15).
   Add a ctest that fails when the checked-in tables drift.
5. **`tools/defcheck.c` (impdef merge)** — wrap the `-impdef` pipeline with a **union/merge
   policy** to maintain `runtime/win32/lib/{kernel32,msvcrt,user32,gdi32,ws2_32}.def`
   (3,290 lines): regenerating from modern DLLs must *not* drop the historical (Win9x-era)
   symbols present in the checked-in files; new exports are appended, order/format preserved
   (note the deliberate out-of-order `SetWindowLongPtrA/W` in user32.def). Verification mode
   for CI; regeneration mode for maintainers with reference DLLs.

## 0.7 Phase 3 — `tools/mccharness.c`: one conformance/differential runner

All drivers in catalog #3 share one skeleton: required-args guard → env skip checks
(**exit 77**) → find tools (skip if absent) → glob or fixed program list → per-program
compile → (link) → (structural check via objcheck) → run (optionally under launcher prefix) →
judge by exit code or byte-compare → first-meaningful-stderr-line on failure → remove
artifacts → exit 1/0. Build one harness parameterized (CLI flags or a tiny per-suite spec) by:
compiler(s), reference compiler(s), launcher argv, structural-check kind, program list/glob,
per-program env, skip list, output-normalization rules.

Migration table (each row: delete the cmake script / embedded `file(WRITE)` block, register
the harness invocation in `add_test` instead; ctest registration, fixtures, labels,
`SKIP_RETURN_CODE 77` stay in CMake):

| Driver (CMakeLists emit site or file) | Suite | Harness needs |
|---|---|---|
| `run_mcc_exe.cmake` (L2702–2717) | exec helper | emulator prefix |
| `run_mcctest.cmake` (L2719–2764) | mcctest differential | ref-cc resolve, PATH prepend, redirect-to-file, compare |
| `run_asm_c_connect.cmake` (L2766–2801) | combined-vs-separate link | compare |
| `run_dash_s_roundtrip.cmake` (L2803–2847) | -S roundtrip | regex sanity on `.s`, compare |
| `run_preprocess.cmake` (L3034–3251) | 3-way `-E` differential | gcc+clang+mcc, line-normalize, stderr scan, recursive glob, 77 |
| `run_i386_fastcall.cmake` (L3546–3665) | ABI interop | write generated C, gcc -m32, cross-link matrix |
| `run_gcctestsuite.cmake` (L3690–3779) | c-torture | glob, feature-gate text scan, `mcc.sum` summary |
| `run_arm_asm.cmake` (L3789–3934) | mcc vs GNU as | parse `arm-tok.h`, `as`/`objdump -S` capture, per-insn diff, known-fail list |
| `run_pe_native.cmake` (L4028–4091) | native PE conformance | per-CPU gating, 77 |
| `tests/qemu/run_pe_wine.cmake` | PE under wine | wine launcher + per-spawn env (`WINEDEBUG`, `WINEPREFIX`), `-B` staging |
| `tests/qemu/run_macho_codegen.cmake` | osx-cross codegen | shim/harness source generation, clang/lld/qemu-aarch64 |
| `tests/qemu/run_macho_image.cmake` | Mach-O image via loader | objcheck type check, generated mini-libc |
| `tests/qemu/run_macho_apple_libc.cmake` | vendored Apple libc | loader build (gcc), objcheck magic |
| `tests/qemu/run_macho_native.cmake` | native macOS | Darwin gate, probe-link, objcheck |
| `tests/qemu/validate_macho.cmake` | structural | objcheck replaces otool entirely (Phase 2) |
| `run_qemu_run.cmake` (L4294–4327) | qemu conformance | `qemu-<arch> -L <sysroot>` launcher, {default,pic} variants |
| `tests/diff/run_parts.cmake` | 3-compiler stdout diff | gcc+clang+mcc, compare |

~1,700 lines of cmake-script drivers become data + one binary. The generated-wrapper-source
tricks (Darwin exit stub, printf-trampoline shim.S, freestanding mini-libc) become string
tables in the harness. Reference compilers (gcc/clang), wine, qemu, the Mach-O loader
(`tests/qemu/macho/loader.c`, built with gcc) remain external by design — they are the oracle
or the substrate, not logic.

## 0.8 Phase 4 — generalize `tools/build.c` into `mccbuild`

Absorb, from CMakeLists, as pure C (CMake keeps only cache/GUI/option plumbing and calls the
tool where useful):

1. **Host/target detection** (`CMakeLists.txt:1143–1396`, ~240 lines): CPU map, OS dispatch +
   suffixes, compiler id, ARM ABI probe (`cc -dM -E -` macro scan), triplet via
   `cc -dumpmachine` + `/usr/lib/<triplet>/crti.o` probes, musl/uClibc hints from
   `/lib/ld-*`. Emits the same facts (`MCC_CPU/OS/triplet/arm_abi`). Uses `cc_probe()`.
2. **Config→defines core** (`_mccdefs`, `CMakeLists.txt:1515–1654`, ~215 lines) — the
   linchpin (catalog #8): one C table mapping resolved config to the `-D` list and the
   quote-free libmcc1 flags. Delete the duplicate in `build.c:41–54`. The config-node
   **validation matrix** (`CMakeLists.txt:243–378`) moves here too; CMake calls
   `mccbuild --validate-config` instead of duplicating rules.
3. **libmcc1 recipe** (`mcc_build_libmcc1`, `CMakeLists.txt:2212–2426`, ~215 lines):
   per-CPU/OS runtime-object selection, `-B` root, emulator-prefixed self-compile,
   `mcc -ar rcs`, header + win32 `.def` staging via the Phase-1 fs kit.
4. **Cross-compiler matrix** (`CMakeLists.txt:2428–2592`, ~165 lines): the 11-target factory
   is Domain-11 logic parameterized by target — pure define-list + spawn.
5. **Git stamp** helper (catalog #7) for the githash define.

Result: `mccbuild` can produce mcc + runtime for **every** supported target without CMake,
not just native x86_64; CMake remains the IDE/install/package front-end and its recipes call
into shared single-source logic where they overlap.

Explicitly **not** ported (inherently CMake): superbuild/`ExternalProject` (L800–971),
`install(EXPORT)`/`find_package(mcc)` config (L2604–2681), CPack (L4531–4550), preset schema,
`check_c_compiler_flag` results consumed by CMake targets, toolchain download (catalog #6),
package-manager `setup-*` targets (L679–798).

## 0.9 Phase 5 — CI thinning

1. **`tools/ci.c`** (or `mccbuild ci` verb): `stage` (rsync-equivalent tree copy with the
   single shared exclusion list, catalog #9, + CRLF→LF normalization over
   `*.c/*.h/*.cmake/*.txt/*.S/*.def` — replaces `tests/ci/docker/run-ci.sh:29–48`) and
   `run-preset <name>` (configure→build→test→install→package with one normalized parallelism
   probe — replaces the 7 duplicated blocks, catalog #11). ci.yml/release.yml jobs become:
   checkout → provision toolchain → `tools/ci run-preset X` → upload.
2. **Matrix single-sourcing** (catalog #12): a small generator (or ci.c verb) enumerates the
   CI matrix from `CMakePresets.json` so preset names exist in one place; ci.yml's `dist` job
   and release.yml's dist jobs share it (they are the same matrix, different version stamp:
   `ci-<sha12>` vs tag).
3. **Checksum merge**: the `cat checksums-*.txt > SHA256SUMS.txt` release step moves into
   `cmake/package.cmake` or ci.c. `package.cmake` itself stays (it is already the portable
   single-source packager using bundled `cmake -E tar` + `file(SHA256)`); porting it fully is
   Phase 6.
4. Dockerfiles remain pure toolchain layers; `run-ci.sh` shrinks to `exec tools/ci ...`;
   `tests/qemu/docker/run-matrix.sh`'s crt fixup and pre-fetch filter move into ci.c only if
   the matrix moves too — otherwise leave (container-internal glue).

## 0.10 Phase 6 (optional) — packaging/extraction codecs

Only if full normalization is wanted: in-tree SHA-256 (~200 lines) + gzip inflate/deflate +
tar/zip read/write (miniz-scale, freestanding C) would let `package.cmake` and the
rootfs/toolchain *extraction* halves move to C. **HTTPS download stays external regardless**
(CMake `file(DOWNLOAD)` or curl) — do not hand-roll TLS. Skip this phase if `cmake -E tar` +
`file(SHA256)` remain acceptable; they are bundled with CMake, not external tools.

## 0.11 Expected end state

- External tools eliminated: `otool`/`llvm-otool`, `file(1)`, POSIX `cp`/`mkdir`/`sed`/`rsync`
  shell-outs, ad-hoc pwsh/bash compiler discovery. Externals remaining by design: docker,
  qemu-user, wine, macOS toolchain, reference gcc/clang, git, package managers, CMake itself
  as IDE/install/package front-end.
- ~1,700 lines of `cmake -P` drivers → one harness + per-suite data; ~800–1,000 lines of
  CMakeLists probing/recipe logic → mccbuild; CI YAML/shell → orchestration-only.
- Each catalog entry in §0.4 has exactly one home; PLAN's non-C index (below) shrinks
  accordingly and should be re-generated after each phase.

---

# PLAN — Non-C file index

This document is a meticulous, line-indexed description of every non-C file in the mcc repository: the CMake build system, presets, CI workflows, Docker images, shell scripts, test-driver CMake scripts, assembly test fixtures, repository dot-files, and the Windows import-definition (`.def`) files. Markdown documentation files are indexed per-heading rather than per-line at the end.

Conventions:

- `L12` / `L14–37` refer to 1-indexed line numbers in the file named by the nearest enclosing `##` header (or, inside the `CMakeLists.txt` part, in `CMakeLists.txt` itself).
- A single multi-line command (a CMake call, a YAML step, a Dockerfile instruction) is documented as one ranged entry.
- Blank lines and pure separator comments are folded into the adjacent entry; informative comments are described.
- Line numbers are accurate as of the revision this index was written against (branch `mob`, July 2026); they will drift as files are edited.

## Contents

1. **`CMakeLists.txt`** (4,550 lines) — indexed in four ranges: 1–1150, 1151–2300, 2301–3450, 3451–4550.
2. **Build presets and CMake modules** — `CMakePresets.json`, `cmake/host_gate_check.cmake`, `cmake/package.cmake`.
3. **CI infrastructure** — `.github/workflows/ci.yml`, `.github/workflows/release.yml`, `tests/ci/docker/Dockerfile`, `tests/ci/docker/run-ci.sh`.
4. **QEMU / cross-target test harness** — `tests/qemu/docker/{Dockerfile,run-matrix.sh,README.md}`, `tests/qemu/run_macho_apple_libc.cmake`, `run_macho_codegen.cmake`, `run_macho_image.cmake`, `run_macho_native.cmake`, `run_pe_wine.cmake`, `validate_macho.cmake`.
5. **Miscellaneous test-harness and repo-config files** — `.gitattributes`, `.gitignore`, `tests/asm/run_dash_s_bytes.cmake`, `tests/diff/run_parts.cmake`, `tests/cli/asmadd.s`, `tests/preprocess/asm/*.S`, `tests/asm/gas_directives.S`.
6. **Windows import definitions** — `runtime/win32/lib/{kernel32,msvcrt,user32,gdi32,ws2_32}.def`.
7. **Documentation files** (per-heading index) — `README.md`, `docs/BUILD.md`, `docs/C9911.md`, `docs/TODO.md`, `tests/diff/parts/README.md`, `tests/qemu/apple-libc/PROVENANCE.md`.

# `CMakeLists.txt` (4,550 lines)

The single top-level CMake script that defines the entire build: project setup and host/toolchain detection, the `mcc_config_node` configuration-option framework, downloadable mingw/clang toolchains and the `ExternalProject` superbuild matrix, target OS/CPU finalization, the `libmcc` libraries and `mcc` executables (host and musl flavors), the self-hosted `libmcc1.a` target-runtime build, the eleven-target cross-compiler matrix, installation and CMake-package export, the full ctest suite registration (exec, cli, diff, preprocess, asm, ABI, conformance, QEMU matrix), and developer/packaging targets ending in CPack. Indexed below in four consecutive line ranges.

### Lines 1–1150

This opening region of `CMakeLists.txt` establishes the project's entire configuration infrastructure before any compiler targets are declared. It sets the version, works around a macOS/Homebrew-gcc deployment-target bug before `project()`, and then defines a home-grown "config node" framework (`mcc_config_node` and friends) that registers every user-facing build knob with metadata (type, default, group, choices, visibility condition) into global properties so it can later be validated, reported, and rendered into documentation. It defines toolchain mapping helpers (`mcc_map_toolchain_cc`, `mcc_find_gnu_gcc`, `mcc_map_target_args`), downloadable self-contained toolchains (winlibs/multilib mingw and an LLVM/clang release) with `mingw-toolchain` / `clang-toolchain` fetch targets, package-manager-based `setup-gcc`/`setup-wine`/`setup-mingw` install targets, and a superbuild matrix that fans a multi-toolchain/multi-target request out into `ExternalProject` sub-builds and then `return()`s. After the matrix bail-out, the region declares the bulk of the configurable nodes (build-target shapes, diagnostics variants, mcc language/runtime features, runtime search paths, extra flags) and begins host detection by mapping `CMAKE_SYSTEM_PROCESSOR` to a canonical `MCC_CPU` value.

#### Version and macOS gcc deployment-target workaround (L1–L36)

- **L1–L2** — `cmake_minimum_required(VERSION 3.22)` sets the minimum CMake version to 3.22, followed by a blank line.
- **L3** — Sets `MCC_VERSION` to the full version string `"1.0.0"`.
- **L4** — Uses `string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" ...)` to extract the leading numeric dotted portion of `MCC_VERSION` into `MCC_VERSION_NUMERIC`, so any non-numeric suffix (e.g. a pre-release tag) is stripped before the value is handed to `project(VERSION ...)`, which only accepts numeric versions.
- **L5–L13** — Blank separator lines.
- **L14–L36** — A pre-`project()` guard that runs only when the host is Apple (`CMAKE_HOST_APPLE`), `CMAKE_OSX_DEPLOYMENT_TARGET` is unset, and the `MACOSX_DEPLOYMENT_TARGET` environment variable is undefined. It determines the selected C compiler `_mcc_sel_cc` from `CMAKE_C_COMPILER` (L16), falling back to the `CC` environment variable if that is empty (L17–L19). If the compiler path matches "gcc" case-insensitively (L20), it runs `<cc> --version` (L21–L22) and checks that the run succeeded and the output contains "Free Software Foundation" — i.e. it is real GNU gcc, not Apple's clang masquerading as gcc (L23). It then queries the running macOS version via `sw_vers -productVersion` (L24–L26), defaulting `_mcc_osver` to `"11.0"` if that command fails or produces non-numeric output (L27–L29). Finally it caches `CMAKE_OSX_DEPLOYMENT_TARGET` to that OS version (L30–L31) and prints a status message (L32–L33) explaining the intent: Homebrew GNU gcc otherwise inherits a stale 10.6 default deployment target that breaks gcc's linker spec. This must happen before `project()` because that is where the deployment target is consumed.

#### Project declaration and early configuration (L37–L55)

- **L38–L42** — `project(mcc ...)` declares the project named `mcc` with `VERSION` set to the numeric `MCC_VERSION_NUMERIC`, enabled languages `C` and `ASM`, description "ModernCC", and homepage URL `https://github.com/lucianlightgray/moderncc.git`.
- **L44** — Enables `CMAKE_EXPORT_COMPILE_COMMANDS` so a `compile_commands.json` is generated for tooling.
- **L46–L48** — If policy `CMP0174` exists in the running CMake, sets it to `NEW` (this policy governs `cmake_parse_arguments` handling of empty single-value keyword arguments, which the config-node function relies on).
- **L50–L51** — Defines the cache variable `MCC_CONFIG_EXTRA` (type `FILEPATH`), defaulting to `config-extra.cmake` in the source directory, described as "Optional extra CMake config, included before options".
- **L52–L55** — If the file named by `MCC_CONFIG_EXTRA` exists, `include()`s it and prints a status message. This lets a user pre-seed variables (e.g. `MCC_SEED_*`) before any option is declared.

#### Config-node framework functions (L57–L132)

- **L57–L91** — `function(mcc_config_node name)` is the central declaration helper for every user-settable build knob. L58–L59 parse the arguments after `name`: boolean flags `EMPTY_OK` (empty string is a valid value) and `ADVANCED` (mark the cache entry advanced); single-value keywords `TYPE` (cache type, e.g. BOOL/STRING), `DEFAULT` (default value), `HELP` (docstring), `GROUP` (documentation grouping label), `VISIBLE_WHEN` (a CMake condition string gating GUI visibility); and multi-value keyword `CHOICES` (the allowed value set). L61–L64 compute the effective default `_def`: the declared `DEFAULT`, overridden by `MCC_SEED_<name>` if such a seed variable is defined (set by profiles or `config-extra.cmake`). L66–L70 create the cache entry — via `option()` for `TYPE BOOL`, otherwise via `set(... CACHE <TYPE> ...)`. L72–L78 attach the `STRINGS` cache property for GUI drop-downs when choices exist or `EMPTY_OK` is set, prepending an empty string entry when `EMPTY_OK`. L79–L81 apply `mark_as_advanced` when `ADVANCED` was given. L83–L90 record the node in global properties: the name is appended to the `MCC_NODES` list, and per-node properties `MCC_NODE_<name>_DEFAULT/_TYPE/_GROUP/_HELP/_CHOICES/_EMPTYOK/_WHEN` store the raw metadata for later validation, reporting, and doc generation.
- **L93–L108** — `function(mcc_apply_node_visibility)` iterates every registered node (L94–L95), reads its `VISIBLE_WHEN` condition and declared cache type (L96–L97), and evaluates the condition string at call time via `cmake_language(EVAL CODE "if(${_when}) ... set(_vis FALSE) ...")` (L98–L101). Nodes whose condition holds (or is empty) get their cache `TYPE` restored to the declared type (L102–L103), making them visible in cmake-gui/ccmake; nodes whose condition fails are demoted to `TYPE INTERNAL` (L104–L106), hiding them.
- **L110–L113** — `function(mcc_status_node name value)` appends `name` to the global `MCC_STATUS_NODES` list and stores `value` under `MCC_STATUS_<name>`; these represent derived (computed, not user-set) values that `mcc_report_config` later prints tagged `[derived]`.
- **L115–L128** — `function(mcc_snapshot_user_presets)` captures which `MCC_*` variables the user set explicitly, for forwarding into superbuild child cells. L116–L118 make it idempotent: if the internal cache variable `_MCC_PRESET_VARS` already exists it returns immediately (so the snapshot survives re-configures and is taken only on the very first run, before this file creates its own `MCC_*` cache entries). L119–L125 enumerate all cache variables and collect those matching `^MCC_` but not `^_MCC_` (internal ones). L126–L127 store the list as the `INTERNAL` cache variable `_MCC_PRESET_VARS`, documented as the set of user `-D` / config-extra `MCC_*` variables for superbuild forwarding.
- **L130–L132** — `macro(mcc_profile_seed var val)` simply sets `MCC_SEED_<var>` to `val` in the caller's scope; because `mcc_config_node` consults `MCC_SEED_*` when computing defaults, this is how toolchain profiles and compatibility shims influence node defaults without forcing the cache.

#### Toolchain mapping helpers (L134–L226)

- **L134–L185** — `function(mcc_map_toolchain_cc tc outvar)` maps a toolchain-profile name `tc` (auto/gcc/clang/mcc/msvc/mingw) to a concrete C compiler path returned in `outvar`. L135–L138: a user override `MCC_CC_<tc>` wins unconditionally. L139–L142: `auto` maps to the empty string (let CMake pick). L143–L149: `mingw` calls `mcc_mingw_resolve()` and returns the resolved downloaded x86_64 mingw gcc path `_MCC_MINGW_X86_64_GCC` (blank comment lines at L144–L145). L150–L160: `gcc` calls `mcc_find_gnu_gcc()` to locate a genuine GNU gcc (not clang pretending to be gcc) and returns it if found; on failure it falls through to the generic probe below. L161–L173: `clang` first probes for a system `clang` via `find_program` into the cache variable `MCC_TOOLCHAIN_CC_clang`; the comment at L162–L164 explains that with no system clang it falls back to the fetched cmake-clang toolchain (built via the `clang-toolchain` target), analogous to how `mingw` uses its downloaded gcc — so if the probe failed, it calls `mcc_clang_resolve()` and returns `_MCC_CLANG_EXE` when that path exists on disk. L174–L179: the generic fallback defines per-toolchain candidate name lists (`auto` → nothing, `gcc` → `gcc;cc`, `clang` → `clang`, `mcc` → `mcc`, `msvc` → `cl`) and runs `find_program` into `MCC_TOOLCHAIN_CC_<tc>` using `_names_<tc>`. L180–L184 return the found path or the empty string.
- **L187–L192** — Blank separator lines.
- **L193–L214** — `function(mcc_find_gnu_gcc outvar)` locates a real GNU gcc. L194–L197: the cache variable `MCC_GNU_GCC`, if set by the user, is returned as-is. L198–L212: otherwise it probes the candidate names `gcc-16` down through `gcc-10`, then `gcc`, then `cc` in order; for each name found (cached as `MCC_GNUGCC_<name>`), it runs `<candidate> --version` (L201–L203) and accepts the candidate only if the run succeeded, the output contains "Free Software Foundation", and the output does not contain "clang" (L206–L207) — this rejects Apple's `gcc` shim, which is clang. The first acceptable candidate is returned (L208–L209). L213 returns the empty string when nothing qualifies.
- **L216–L226** — `function(mcc_map_target_args name outvar)` maps a cross-platform target cell name to the extra CMake `-D` arguments its sub-build needs: a user-defined `MCC_TARGET_ARGS_<name>` variable wins (L217–L218); `native` maps to no extra args (L219–L220); `cross` maps to `-DMCC_ENABLE_CROSS=ON` (L221–L222); anything else returns the sentinel string `__UNKNOWN__` (L223–L224), which the superbuild loop later turns into a fatal error.

#### Config autocorrection and validation (L228–L378)

- **L229–L238** — `function(mcc_autocorrect name value reason)` force-overrides a config node in the cache: it reads the node's registered type and help text from the global properties (L230–L231), re-sets the cache entry with `FORCE` using `CACHE BOOL` for BOOL nodes or the recorded type otherwise (L232–L236), and logs `mcc config [autocorrect]: <name> -> <value> (<reason>)` (L237). Used only when the non-strict `MCC_CONFIG_AUTOCORRECT` mode is on.
- **L240–L242** — Blank separator lines.
- **L243–L378** — `function(mcc_validate_config)` performs all cross-option sanity checking; it is defined here and invoked later in the file. Its checks:
  - **L244–L259** — Generic choice validation: for every registered node with a `CHOICES` list, builds the allowed set (appending the empty string when `EMPTY_OK`) and emits a `WARNING` (not an error) if the current value is not in the set, suggesting a typo but proceeding.
  - **L261–L265** — `MCC_CONFIG_SEMLOCK`, when non-empty, must be all digits (`^[0-9]+$`); otherwise `FATAL_ERROR`.
  - **L267–L273** — If `MCC_BUILD_SANITIZE` or `MCC_BUILD_PROFILE` is enabled but the host compiler ID is not GNU or Clang, fails fatally: those variants need GCC/Clang flags; the message names the offending compiler ID and suggests disabling them or setting `-DCMAKE_C_COMPILER`.
  - **L274–L280** — `MCC_BUILD_SANITIZE` combined with a `WIN32` target OS is fatal: mingw GCC ships no libasan/libubsan, so the sanitizer build `mcc_s` cannot link on a PE/mingw target (coverage and profiling remain supported there).
  - **L281–L285** — `MCC_BUILD_PROFILE` on a Darwin target is fatal: the profiling variant `mcc_p` links `-static`, and Darwin has no static `crt0.o`.
  - **L286–L317** — Cross-compiling without an emulator: if `CMAKE_CROSSCOMPILING` is set and `MCC_EMULATOR` is empty, the just-built `mcc` is a foreign binary that cannot run on the host. With `MCC_CONFIG_AUTOCORRECT` on (L287–L306), it auto-corrects: `MCC_LIBMCC1_USEGCC` to ON (build libmcc1 with the host CC rather than by running the foreign mcc, L288–L291), `MCC_BUILD_TESTS` to OFF (tests cannot execute the foreign mcc, L292–L295), `MCC_BUILD_COVERAGE` to OFF (mcc_c is compiled by running the foreign mcc, L296–L299); but even then a WIN32 *shared* libmcc is fatal (L300–L306) because generating the import library requires running `mcc -impdef` on the host — the fix is a `CMAKE_CROSSCOMPILING_EMULATOR` or `-DMCC_BUILD_STATIC_LIB=ON`. Without autocorrect (L307–L316), it fails fatally with a message listing the three remedies: set an emulator (wine/qemu-arm), build host-runnable cross compilers via `-DMCC_ENABLE_CROSS=ON` with no toolchain file, or enable `MCC_CONFIG_AUTOCORRECT`.
  - **L319–L329** — `MCC_CONFIG_BCHECK` (bounds checker `-b`) with `MCC_CONFIG_BACKTRACE` off: the bcheck runtime is gated behind the backtrace runtime, so `mcc -b` would fail to link. Autocorrect mode forces `MCC_CONFIG_BACKTRACE` ON; strict mode only warns.
  - **L330–L334** — Warns that `MCC_DISABLE_RPATH` is a no-op when `MCC_BUILD_STATIC_LIB` is on, since rpath is only baked for a shared libmcc.
  - **L335–L342** — Warns about `MCC_BUILD_STATIC_EXE=ON` with `MCC_ONE_SOURCE=OFF` and `MCC_BUILD_STATIC_LIB=OFF`: `mcc-static` links `-static` against libmcc, but a shared-only libmcc (`libmcc.so`) cannot satisfy a static link; the remedies are enabling the static lib or keeping one-source mode so `mcc-static` is self-contained.
  - **L343–L346** — If the integrated assembler is disabled (`MCC_CONFIG_ASM` off) and `MCC_LIBMCC1_USEGCC` is off, unconditionally autocorrects `MCC_LIBMCC1_USEGCC` to ON, because libmcc1's `.S` sources must then be built by the host CC (mcc without asm support cannot assemble them).
  - **L347–L353** — Warns when `MCC_TOOLCHAIN_PROFILE` names a specific toolchain that differs from the detected host compiler `MCC_CC_NAME`: the profile only seeds defaults and does not switch compilers (that requires `-DCMAKE_C_COMPILER`).
  - **L354–L358** — Warns that `MCC_CONFIG_PREDEFS=OFF` makes mcc load `<mccdefs.h>` from the mccdir at runtime instead of compiling it into the binary, creating a runtime dependency.
  - **L359–L363** — Warns that `MCC_CONFIG_LIBC=uClibc` is a legacy selector that only emits `CONFIG_MCC_UCLIBC` with no path effect on modern hosts.
  - **L364–L370** — Warns that `MCC_CONFIG_NEW_DTAGS`, `MCC_CONFIG_PIE`, `MCC_CONFIG_PIC`, and a non-empty `MCC_CONFIG_LIBC` are ELF-only and therefore inert when the target OS is `WIN32` or `Darwin`.
  - **L371–L377** — Conversely, warns that `MCC_CONFIG_CODESIGN` and `MCC_CONFIG_NEW_MACHO` only affect Mach-O output and are inert on any non-Darwin target.

#### Config reporting and documentation generation (L380–L432)

- **L380–L397** — `function(mcc_report_config)` prints the configuration summary banner `================ mcc configuration ================` (L381), then every derived status node from `MCC_STATUS_NODES` with its stored value and a `[derived]` tag (L382–L386), then every registered config node with its current value (L387–L395) — skipping nodes whose *actual* cache TYPE is `INTERNAL` (L390–L393), i.e. those hidden by `mcc_apply_node_visibility` because their `VISIBLE_WHEN` condition failed — and closes with a matching `===` banner (L396).
- **L399–L432** — `function(mcc_generate_node_doc outfile)` renders the registered node metadata into a Markdown file. L400–L406 build the document header: an HTML comment noting it is rendered from "CMakeLists.txt section 1z" by this function, an `# mcc configurable nodes` title, prose pointing at the `mcc_config_node()` declarations as the source of truth (with the preprocessor-flag catalog in "section 3a"), and a seven-column table header (Node, Group, Type, Default, Choices, Shown when, Description). L407–L429 loop over `MCC_NODES`, fetch each node's group/type/default/choices/empty-ok/when/help properties (L409–L415), prefix `''` to the choices when empty is allowed or show `''` alone when there are no other choices (L416–L420), convert the semicolon list to comma-separated text (L421), render an empty default as `''` (L422–L424) and an empty visibility condition as "always" (L425–L427), then append one table row per node (L428). L430–L431 write the file and log its path.

#### Visibility condition strings, preset snapshot, top-level toolchain/target knobs (L434–L461)

- **L435** — Sets `_elf` to the condition string `NOT MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_TARGETOS STREQUAL "Darwin"`, used as the `VISIBLE_WHEN` gate for ELF-only options.
- **L436** — Sets `_darwin` to the condition string `MCC_TARGETOS STREQUAL "Darwin"`, the gate for Darwin-only options.
- **L438** — Calls `mcc_snapshot_user_presets()` now — before any `mcc_config_node` call creates its own `MCC_*` cache entries — so `_MCC_PRESET_VARS` captures only what the user set via `-D` or config-extra.
- **L440–L442** — Declares node `MCC_TOOLCHAIN_PROFILE` (STRING, default `auto`, group "Build targets", choices `auto gcc clang mcc msvc mingw`): the toolchain(s) to build with, in order, as a list (e.g. `gcc;clang;mcc`); a single entry acts as a profile that seeds defaults.
- **L444–L450** — Validates `MCC_TOOLCHAIN_PROFILE` immediately (it can be a list, which the generic choice check would not handle correctly): defines the allowed set `_tc_allowed` and iterates the list, failing fatally on any entry not in `auto gcc clang mcc msvc mingw`.
- **L452** — `option(MCC_SUPERBUILD_TEST ...)`, default ON: run `ctest` after each matrix sub-build.
- **L453** — `option(MCC_SUPERBUILD_SEQUENTIAL ...)`, default ON: build toolchain stages in order while targets within a stage stay parallel.
- **L454–L455** — `option(MCC_SUPERBUILD_CHILD ...)`, default OFF and marked advanced: an internal flag set on matrix child cells to suppress recursive superbuild expansion.
- **L456–L458** — Cache STRING `MCC_TARGETS`, default `native`: the cross-platform target(s) to build as a list (`native;cross;<custom>`); its `STRINGS` property offers `native` and `cross` in GUIs.
- **L459–L461** — Cache FILEPATH `MCC_GNU_GCC`, default empty and marked advanced: pins the real GNU gcc used by the `gcc` toolchain profile; empty means probe `gcc-16..gcc-10, gcc, cc` (consumed by `mcc_find_gnu_gcc`).

#### Downloadable mingw toolchain (L463–L588)

- **L463–L473** — Blank separator lines.
- **L474–L476** — Declares node `MCC_MINGW_SOURCE` (STRING, default `winlibs`, group "Build targets", advanced, choices `winlibs multilib`): selects what the `mingw-toolchain` target downloads — `winlibs` (dual-arch, current) or `multilib` (a single gcc handling `-m32`/`-m64`).
- **L477–L478** — Cache PATH `MCC_MINGW_DIR`, defaulting to the source directory: the parent directory that the downloaded `cmake-mingw-*` toolchain trees live in.
- **L481** — Sets the temporary `_wl` to the WinLibs GitHub release base URL for the `16.1.0posix-14.0.0-ucrt-r3` release, shared by both architecture URLs below.
- **L482–L484** — Cache STRING `MCC_MINGW_WINLIBS_X86_64_URL`: the WinLibs x86_64 (64-bit) toolchain zip URL (`winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r3.zip` under `_wl`).
- **L485–L487** — Cache STRING `MCC_MINGW_WINLIBS_X86_64_SHA256`: the pinned SHA256 (`4273565109...`) of that x86_64 zip.
- **L488–L490** — Cache STRING `MCC_MINGW_WINLIBS_I686_URL`: the WinLibs i686 (native 32-bit) toolchain zip URL (`winlibs-i686-posix-dwarf-gcc-16.1.0-mingw-w64ucrt-14.0.0-r3.zip`).
- **L491–L494** — Cache STRING `MCC_MINGW_WINLIBS_I686_SHA256`: the pinned SHA256 (`c4c7419f...`) of the i686 zip, followed by `unset(_wl)` to drop the temporary.
- **L498–L499** — Cache STRING `MCC_MINGW_MULTILIB_URL`, default empty: the archive URL for a single-gcc multilib toolchain; required when `MCC_MINGW_SOURCE=multilib`.
- **L500** — Cache STRING `MCC_MINGW_MULTILIB_SHA256`, default empty: optional SHA256 for that archive.
- **L501–L502** — Cache STRING `MCC_MINGW_MULTILIB_SUBDIR`, default `mingw64`: the subdirectory inside the multilib archive that holds `bin/gcc`.
- **L510–L524** — `function(mcc_mingw_resolve)` computes the expected on-disk paths of the downloaded mingw compilers without checking existence. It initializes `_x` (x86_64 gcc), `_i` (i686 gcc), and `_m` (multilib gcc) to empty (L511–L513). For `winlibs` (L514–L516), `_x` is `<MCC_MINGW_DIR>/cmake-mingw-x86_64/mingw64/bin/gcc.exe` and `_i` is `<MCC_MINGW_DIR>/cmake-mingw-i686/mingw32/bin/gcc.exe`. For `multilib` (L517–L520), `_m` is `<MCC_MINGW_DIR>/cmake-mingw-multilib/<MCC_MINGW_MULTILIB_SUBDIR>/bin/gcc.exe`, and `_x` is aliased to the same path since the multilib gcc serves the 64-bit role. L521–L523 export the three as `_MCC_MINGW_X86_64_GCC`, `_MCC_MINGW_I686_GCC`, and `_MCC_MINGW_MULTILIB_GCC` in the parent scope.
- **L528–L537** — Builds `_mingw_entries`, a list of `url|sha256|destination` records for the fetch script: for `winlibs`, two entries targeting `cmake-mingw-x86_64` and `cmake-mingw-i686` under `MCC_MINGW_DIR`; for `multilib`, one entry targeting `cmake-mingw-multilib`.
- **L539–L544** — Writes the generated fetch script `run_mingw_fetch.cmake` into the build directory: first a `set(SOURCE "<MCC_MINGW_SOURCE>")` line and the opening of a `set(ENTRIES` list (L539–L540), then one quoted entry line per record (L541–L543), then the closing paren (L544).
- **L545–L583** — Appends the static body of the fetch script as a bracket-quoted `[==[...]==]` literal (so it is written verbatim, evaluated only when the script runs). The body loops over `ENTRIES` (L548), splits each on `|` into URL, SHA, and destination (L549–L552), and fails fatally on an empty URL with a hint to set `-DMCC_MINGW_MULTILIB_URL` for the multilib source (L553–L557). If the destination already contains a `.mcc-mingw-stamp` file it is treated as present and skipped, with a note that deleting the stamp forces a refetch (L558–L562). Otherwise it downloads the archive to `<dest>.download` (L563–L564) with `SHOW_PROGRESS`, a 3600-second timeout, and `EXPECTED_HASH SHA256=` verification only when a hash was provided (L565–L569); a non-zero download status removes the partial file and aborts (L570–L574). On success it wipes and recreates the destination directory (L575–L576), extracts the archive there via `file(ARCHIVE_EXTRACT)` (L577–L578), deletes the downloaded archive (L579), writes the source URL into `.mcc-mingw-stamp` as the completion marker (L580), and logs completion (L581).
- **L585–L588** — `add_custom_target(mingw-toolchain ...)` runs the generated script via `cmake -P` with `USES_TERMINAL` (live progress output) and `VERBATIM`; the comment names the active `MCC_MINGW_SOURCE` and the `cmake-mingw-*` destination pattern. Building this target performs the download/extract.

#### Downloadable clang toolchain (L589–L678)

- **L589–L602** — Blank separator lines.
- **L603–L604** — Cache PATH `MCC_CLANG_DIR`, defaulting to the source directory: the parent directory the downloaded `cmake-clang` tree lives in.
- **L605–L615** — Computes host-dependent defaults for the LLVM release download. `_clang_url`, `_clang_sha`, and `_clang_subdir` start empty (L605–L607); only on 64-bit Windows (`WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8`, L608) are they populated: version `22.1.8`, archive top directory `clang+llvm-22.1.8-x86_64-pc-windows-msvc`, the corresponding `llvm-project` GitHub release `.tar.xz` URL, and its pinned SHA256 `d96c2cc1...` (L609–L613); `_clang_ver` is unset afterwards (L614). On other hosts the defaults stay empty and the user must supply a URL.
- **L616–L617** — Cache STRING `MCC_CLANG_URL`: the LLVM release archive URL for the `clang-toolchain` download, seeded from `_clang_url`.
- **L618–L619** — Cache STRING `MCC_CLANG_SHA256`: the archive's SHA256; empty skips verification.
- **L620–L621** — Cache STRING `MCC_CLANG_SUBDIR`: the top-level directory inside the LLVM archive (the one holding `bin/clang`).
- **L622–L624** — Unsets the three `_clang_*` temporaries.
- **L628–L634** — `function(mcc_clang_resolve)` computes the expected fetched clang executable path: empty when `MCC_CLANG_SUBDIR` is empty, otherwise `<MCC_CLANG_DIR>/cmake-clang/<MCC_CLANG_SUBDIR>/bin/clang<CMAKE_EXECUTABLE_SUFFIX>`; exported to the parent scope as `_MCC_CLANG_EXE`.
- **L637–L641** — Writes the generated fetch script `run_clang_fetch.cmake` into the build directory, baking in three `set()` lines: `URL` from `MCC_CLANG_URL`, `SHA` from `MCC_CLANG_SHA256`, and `DEST` as `<MCC_CLANG_DIR>/cmake-clang`.
- **L642–L673** — Appends the script's static body as a bracket literal. It fails fatally when `URL` is empty, telling the user to set `-DMCC_CLANG_URL` (and `-DMCC_CLANG_SUBDIR`) for this host (L645–L649). If `DEST/.mcc-clang-stamp` exists it reports "already present" and returns (L650–L653). Otherwise it downloads to `<DEST>.download` (L654–L655) with optional `EXPECTED_HASH SHA256` when `SHA` is non-empty (L656–L659), `SHOW_PROGRESS`, and a longer 7200-second timeout suited to the large LLVM archive (L660); a failed download removes the partial file and aborts (L661–L665). On success it wipes and recreates `DEST` (L666–L667), extracts (with a status note that the large archive takes a while, L668–L669), removes the archive (L670), writes the URL into `.mcc-clang-stamp` (L671), and logs completion (L672).
- **L675–L678** — `add_custom_target(clang-toolchain ...)` runs `run_clang_fetch.cmake` via `cmake -P` with `USES_TERMINAL VERBATIM`, fetching the self-contained LLVM/clang toolchain into `cmake-clang`.

#### Package-manager toolchain-setup targets (L679–L798)

- **L679–L695** — Blank separator lines.
- **L696–L697** — Sets `_toolchain_setup` to `run_toolchain_setup.cmake` in the build directory and begins `file(WRITE ...)` of that script with a bracket-quoted literal body (the literal spans through L782).
- **L698–L782** — The generated script body (written verbatim, executed later via `cmake -P`). L701 declares `cmake_minimum_required(VERSION 3.22)` inside the script; L702–L704 require a `-DTOOL=gcc|wine|mingw` argument or fail. L706–L710 probe for the package managers `brew`, `apt-get`, `dnf`, `pacman`, and `zypper` via `find_program`. L712 initializes the install command `_cmd` to empty. L713–L720: with Homebrew, the command per tool is `brew install gcc`, `brew install --cask --no-quarantine wine-stable`, or `brew install mingw-w64`. L721–L767: otherwise (Linux path), L723–L729 compute a `_sudo` prefix — empty when already root, else the found `sudo` binary — and then each package manager branch maps the tool to its distro package names and builds the full command: apt (L730–L738) installs `gcc` / `wine` / `gcc-mingw-w64 g++-mingw-w64` via `apt-get install -y`; dnf (L739–L747) uses `mingw64-gcc` for mingw; pacman (L748–L756) uses `mingw-w64-gcc` with `-S --noconfirm`; zypper (L757–L765) uses `mingw64-cross-gcc` with `install -y`. L769–L773 fail fatally when no supported package manager was found, telling the user to install the tool manually. L775–L776 pretty-print the command (semicolons replaced by spaces) as a status line; L777–L780 execute it and fail fatally on a non-zero return code; L781 reports success and reminds the user to re-run cmake so the new tool is detected.
- **L784–L787** — `add_custom_target(setup-gcc ...)` runs the setup script with `-DTOOL=gcc`; the comment explains it installs a GNU gcc reference, which widens diff3/preprocess test coverage.
- **L788–L791** — `add_custom_target(setup-wine ...)` runs it with `-DTOOL=wine`; wine lets the suite run mcc's PE output (the pe-wine-conformance tests).
- **L792–L795** — `add_custom_target(setup-mingw ...)` runs it with `-DTOOL=mingw`, installing a host mingw-w64 cross gcc to serve as the PE reference compiler.
- **L796–L798** — `add_custom_target(setup-toolchains ...)` is an umbrella target with no command of its own; `add_dependencies` makes it drive `setup-gcc`, `setup-wine`, and `setup-mingw` together.

#### Superbuild matrix (L800–L971)

- **L800–L802** — Computes `_tc_count` and `_tg_count` as the list lengths of `MCC_TOOLCHAIN_PROFILE` and `MCC_TARGETS`, and initializes `_is_matrix` to FALSE.
- **L807–L810** — Sets `_is_matrix` TRUE when a superbuild is needed: more than one toolchain, more than one target, any target set other than exactly `native`, or `mingw` appearing in the toolchain list (mingw always goes through the matrix because its compiler must be downloaded first).
- **L811–L971** — The superbuild block, entered only when `_is_matrix` is TRUE and this configure is not itself a matrix child (`NOT MCC_SUPERBUILD_CHILD`). It includes the `ExternalProject` module (L812), computes a human-readable `_order` string ("parallel", or "toolchains in order, targets parallel" when `MCC_SUPERBUILD_SEQUENTIAL`) (L813–L816), and logs the toolchains-by-targets matrix (L817–L818).
  - **L825–L855** — MSVC generator handling. Declares cache STRINGs `MCC_MSVC_GENERATOR` (empty = auto-detect the CMake generator for `msvc` cells) and `MCC_MSVC_PLATFORM` (default `x64`, passed as the generator platform `-A`). On Windows, when the generator is unset and `msvc` is among the requested toolchains (L829): if the current generator is already Visual Studio it is reused (L830–L831); otherwise `vswhere` is located under `%ProgramFiles(x86)%/Microsoft Visual Studio/Installer` (L833–L834) and queried for the latest installation version, whose major number maps 18/17/16 to "Visual Studio 18 2026" / "Visual Studio 17 2022" / "Visual Studio 16 2019" (L835–L846). If detection still yields nothing, configuration fails fatally with instructions to pass `-DMCC_MSVC_GENERATOR` (and optionally `-DMCC_MSVC_PLATFORM`) (L848–L853); on success the chosen generator and platform are logged (L854).
  - **L857–L863** — Builds `_forward`, the list of `-D<var>=<value>` arguments replicated into every child cell: every variable captured in `_MCC_PRESET_VARS` except the matrix-shaping ones (`MCC_TOOLCHAIN_PROFILE`, `MCC_TARGETS`, `MCC_SUPERBUILD_TEST`, `MCC_SUPERBUILD_SEQUENTIAL`), which must not recurse into children.
  - **L864–L969** — The nested cell-generation loops. `_prev_stage` (L864) tracks the previous toolchain stage's cell names for sequential ordering. The outer loop iterates toolchains (L865): each is mapped to a compiler via `mcc_map_toolchain_cc` (L866); `_cell_is_msvc` marks msvc cells, which get their compiler from the Visual Studio generator rather than a `-DCMAKE_C_COMPILER` (L867–L870); a non-`auto`, non-msvc toolchain whose compiler was not found is a fatal error suggesting installation or `-DMCC_CC_<tc>=<path>` (L871–L875). The inner loop iterates targets (L877): `mcc_map_target_args` supplies the cell's extra args, and the `__UNKNOWN__` sentinel becomes a fatal error naming the valid targets and the `-DMCC_TARGET_ARGS_<name>` escape hatch (L878–L883). Each cell is named `<toolchain>-<target>` (L884) and its configure args `_cargs` start with `-DMCC_TOOLCHAIN_PROFILE=<tc>`, `-DMCC_SUPERBUILD_CHILD=ON`, the forwarded user presets, and the target args (L885–L886). `CMAKE_BUILD_TYPE` is propagated when set (L887–L889). When the install prefix was set explicitly (not initialized-to-default), `-DCMAKE_INSTALL_PREFIX` is passed too — the comment (L891–L893) explains that cells must bake the superbuild's prefix at configure time because the mcc runtime dir installs to an absolute path, and otherwise each cell would default to a private `<cell>/install` (L890–L895). For non-msvc cells with a known compiler, `-DCMAKE_C_COMPILER=<cc>` is added (L896–L897); additionally on Windows, if `llvm-rc.exe` sits next to the compiler, `-DCMAKE_RC_COMPILER` is passed explicitly — the comment (L899–L901) explains that a GNU-like clang on Windows makes CMake demand a resource compiler, and the fetched cmake-clang ships `llvm-rc` beside clang but neither is on PATH (L898–L906). `_cell_gen_args` carries `CMAKE_GENERATOR`/`CMAKE_GENERATOR_PLATFORM` overrides only for msvc cells (L910–L914). The dependency list `_dep_list` gets the previous stage's cells under sequential mode (L915–L918) and always gets `mingw-toolchain` for mingw cells so the compiler is downloaded before configure (L919–L922); it is wrapped into a `DEPENDS` clause `_dep` only when non-empty (L923–L926).
  - **L927–L935** — `ExternalProject_Add(${_cell} ...)` creates the sub-build: source is this same source tree, binary dir is `<build>/<cell>`, `CMAKE_ARGS` are the assembled `_cargs`, plus the optional generator override; `CONFIGURE_HANDLED_BY_BUILD ON` lets the build step trigger reconfigures, `BUILD_ALWAYS ON` re-runs the cell build every time, `INSTALL_COMMAND ""` disables the ExternalProject install step (installation is dispatched separately below), and the optional `DEPENDS` clause is appended.
  - **L936–L951** — When `MCC_SUPERBUILD_TEST` is on, `ExternalProject_Add_Step(${_cell} ctest ...)` adds a step that runs `ctest --output-on-failure` in the cell's binary dir after the build (`DEPENDEES build`), on every build (`ALWAYS ON`), with terminal access. For msvc cells using a Visual Studio (multi-config) generator, `_cell_ctest_cfg` supplies the `-C <config>` argument — `Debug` when no `CMAKE_BUILD_TYPE` was given, else that build type (L939–L946).
  - **L952–L965** — Install dispatch. The comment (L952–L955) explains that `cmake --install <superbuild-dir>` fans out into each cell (the ExternalProject install step being disabled), that cells install to the prefix baked at configure time, and that a `--prefix` given at install time cannot re-root the absolute destinations. `_cell_cfg` appends ` --config <CMAKE_BUILD_TYPE>` when a build type is set (L956–L959). The `install(CODE ...)` snippet runs `cmake --install <build>/<cell>` at install time and fails fatally if the cell install returns non-zero (L960–L965).
  - **L966–L970** — Each cell name is collected into `_this_stage`; after the target loop, `_prev_stage` becomes `_this_stage` so the next toolchain stage can depend on it (L968). Once all cells are generated, `return()` (L970) ends the top-level configure — the superbuild produces only the matrix scaffolding and never the normal single-build targets.

#### Toolchain profile seeds and build-target config nodes (L973–L1058)

- **L973–L982** — Single-toolchain profile seeding, reached only in a non-matrix (or child) configure. Profile `mcc` seeds `MCC_ONE_SOURCE` ON (bootstrapping with mcc itself works best with the single-TU build); the `msvc` branch is intentionally empty apart from blank comment lines (L976–L981), reserving the spot without seeding anything.
- **L984–L985** — Declares node `MCC_ENABLE_CROSS` (BOOL, default OFF, group "Build targets"): build all cross compilers, the equivalent of the old Makefile's `make cross`.
- **L988–L994** — Backwards-compatibility shim: if the deprecated variable `MCC_BUILD_STATIC` is defined but the new `MCC_BUILD_STATIC_LIB` is not, warns that it is deprecated (explaining the split into `MCC_BUILD_STATIC_LIB` for libmcc.a-vs-.so and `MCC_BUILD_STATIC_EXE` for a static executable) and forwards its value as a seed for `MCC_BUILD_STATIC_LIB` via `mcc_profile_seed`.
- **L995–L996** — Node `MCC_BUILD_STATIC_LIB` (BOOL, default OFF, "Build targets"): build `libmcc.a` instead of a shared libmcc.
- **L997–L998** — Node `MCC_BUILD_STATIC_EXE` (BOOL, default OFF, "Build targets"): link the mcc executable(s) fully static with `-static`; the help notes that `-run` then resolves libc symbols via the built-in table, covering common symbols only.
- **L999–L1000** — Node `MCC_BUILD_DYNAMIC_LIB` (BOOL, default OFF, "Build targets"): also build a shared `libmcc.so` alongside the static `libmcc.a`.
- **L1001–L1002** — Node `MCC_BUILD_DYNAMIC_EXE` (BOOL, default ON, "Build targets"): build `mcc-dynamic`, a non-one-source driver translation unit linked against the primary libmcc; requires `MCC_ONE_SOURCE=OFF` and is skipped otherwise.
- **L1003–L1004** — Node `MCC_BUILD_MUSL` (BOOL, default OFF, "Build targets"): also build musl-targeting compiler variants (`mcc*-musl`) beside the glibc ones.
- **L1005–L1008** — If the host is Apple and `MCC_BUILD_STATIC_EXE` is on, forces it OFF (as a normal variable overriding the cache value for this configure) with a status message: macOS has no fully-static libc.
- **L1009–L1010** — Node `MCC_BUILD_STRIP` (BOOL, default OFF, "Build targets"): strip symbols from the mcc executable(s) at link time (`-s`).
- **L1011–L1013** — Node `MCC_DISABLE_RPATH` (BOOL, default OFF, "Build targets", `VISIBLE_WHEN "NOT MCC_BUILD_STATIC_LIB"`): do not bake `-rpath` into binaries linking `libmcc.so`; hidden in GUIs when the lib is static since it would be inert.
- **L1014–L1015** — Node `MCC_ONE_SOURCE` (BOOL, default ON, "Build targets"): build libmcc from a single translation unit, mirroring the Makefile's `ONE_SOURCE`.
- **L1016–L1017** — Node `MCC_BUILD_TESTS` (BOOL, default ON, "Build targets"): build and enable the test suite (the code derived from `tests/Makefile`).
- **L1018–L1019** — Node `MCC_DIAGNOSTICS` (BOOL, default OFF, group "Diagnostics", advanced): an everything-on diagnostics meta-switch enabling verbose warnings plus debug info and building the `mcc_s`/`mcc_p`/`mcc_c` variants.
- **L1020–L1058** — The `MCC_DIAGNOSTICS` expansion block, which seeds the individual diagnostics nodes (declared just below) rather than forcing them. It only seeds anything when the host compiler is GNU or Clang (L1026); otherwise (L1054–L1057) it logs that sanitize/coverage/profile need GCC/Clang and skips all three. Within the GCC/Clang branch: `MCC_BUILD_SANITIZE` is seeded ON (L1043) unless the target is PE/mingw (`WIN32 OR MCC_CONFIG_MINGW32`, L1032–L1034 — mingw ships no libasan/libubsan) or the compiler is GNU gcc on macOS (L1035–L1041 — Homebrew gcc ships no linkable libasan/libubsan there); `MCC_BUILD_COVERAGE` is always seeded ON (L1045); `MCC_BUILD_PROFILE` is seeded ON (L1052) except on Darwin (L1049–L1050 — static profiling needs a static `crt0.o`, which Darwin lacks). Interspersed blank comment lines (L1021–L1025, L1027–L1031, L1036–L1039, L1046–L1048) are placeholders.

#### Diagnostics, mcc-feature, runtime-path, and flag config nodes (L1060–L1141)

- **L1060–L1061** — Node `MCC_BUILD_PROFILE` (BOOL, default OFF, "Diagnostics"): also build `mcc_p`, the profiling variant compiled with `-pg -static`.
- **L1062–L1063** — Node `MCC_BUILD_COVERAGE` (BOOL, default OFF, "Diagnostics"): also build `mcc_c`, the coverage-instrumented variant.
- **L1064–L1065** — Node `MCC_BUILD_SANITIZE` (BOOL, default OFF, "Diagnostics"): also build `mcc_s` with `-fsanitize=address,undefined`.
- **L1066–L1067** — Node `MCC_LIBMCC1_USEGCC` (BOOL, default OFF, "Build targets"): build the native libmcc1 with the host CC instead of the just-built mcc; the help notes this makes bcheck faster.
- **L1068–L1069** — Node `MCC_CONFIG_AUTOCORRECT` (BOOL, default OFF, "Build targets", advanced): non-strict mode in which `mcc_validate_config` auto-corrects inert or non-runnable option combinations instead of only warning/failing.
- **L1071–L1072** — Node `MCC_CONFIG_MINGW32` (BOOL, default OFF, group "mcc features"): request a WIN32/mingw32 build target; described as an input that forces `MCC_TARGETOS=WIN32` (consumed by the OS detection at L1160).
- **L1073–L1074** — Node `MCC_CONFIG_BACKTRACE` (BOOL, default ON, "mcc features"): enable stack backtraces for `-bt` / `-run`.
- **L1075–L1077** — Node `MCC_CONFIG_BCHECK` (BOOL, default ON, "mcc features", `VISIBLE_WHEN "MCC_CONFIG_BACKTRACE"`): enable the bounds checker (`-b`); hidden when backtrace is off because bcheck depends on the backtrace runtime.
- **L1078–L1079** — Node `MCC_CONFIG_ASM` (BOOL, default ON, "mcc features"): enable the integrated assembler, covering inline asm, global asm, `.s` files, and asm labels.
- **L1080–L1081** — Node `MCC_CONFIG_PREDEFS` (BOOL, default ON, "mcc features"): compile `mccdefs.h` into the binary via the c2str mechanism (OFF makes it a runtime file dependency, as the validator warns).
- **L1082–L1084** — Node `MCC_CONFIG_PIE` (BOOL, default OFF, "mcc features", `VISIBLE_WHEN "${_elf}"`): let mcc generate position-independent executables; ELF-only, hidden on WIN32/Darwin targets.
- **L1085–L1087** — Node `MCC_CONFIG_PIC` (BOOL, default OFF, "mcc features", `VISIBLE_WHEN "${_elf}"`): position-independent code; likewise ELF-only.
- **L1088–L1089** — Node `MCC_WITH_SELINUX` (BOOL, default OFF, "mcc features"): use `mmap` for executable memory in `mcc -run` (the SELinux-compatible allocation strategy).
- **L1090–L1092** — Node `MCC_CONFIG_NEW_DTAGS` (BOOL, default OFF, "mcc features", `VISIBLE_WHEN "${_elf}"`): make mcc emit `DT_RUNPATH` instead of `DT_RPATH`; the help notes it affects mcc-emitted binaries and is always relevant on ELF.
- **L1094–L1095** — Node `MCC_AUTO_MCCDIR` (BOOL, default ON, "mcc features"): the build-tree mcc auto-discovers `libmcc1.a` and headers locally, falling back to the system `CONFIG_MCCDIR`.
- **L1097–L1099** — Node `MCC_CONFIG_LIBC` (STRING, default empty, "mcc features", choices `uClibc musl` with `EMPTY_OK`, `VISIBLE_WHEN "${_elf}"`): the target libc — empty (glibc default), uClibc, or musl.
- **L1100–L1101** — Node `MCC_CONFIG_DWARF` (STRING, default empty, "mcc features", choices `0 2 3 4 5` with `EMPTY_OK`): the DWARF debug-info version (2..5); empty means stabs.
- **L1102–L1103** — Node `MCC_CONFIG_SEMLOCK` (STRING, default empty, "mcc features"): the `CONFIG_MCC_SEMLOCK` value, free-form numeric (validated as digits-only by `mcc_validate_config`); empty defers to mcc.h's default of 1.
- **L1104–L1106** — Node `MCC_CONFIG_NEW_MACHO` (STRING, default empty, group "Darwin", choices `yes no auto` with `EMPTY_OK`, `VISIBLE_WHEN "${_darwin}"`): force the apple object format on/off/auto.
- **L1107–L1109** — Node `MCC_CONFIG_CODESIGN` (STRING, default empty, group "Darwin", choices `yes no auto` with `EMPTY_OK`, `VISIBLE_WHEN "${_darwin}"`): whether to run `codesign` on apple to sign executables.
- **L1112–L1127** — Eight advanced STRING nodes in group "Runtime paths", all defaulting to empty and each mapping directly to an option of the legacy `configure` script: `MCC_SYSROOT` (`--sysroot`, L1112–L1113), `MCC_TRIPLET` (`--triplet`, L1114–L1115), `MCC_SYSINCLUDEPATHS` (`--sysincludepaths`, colon-separated, L1116–L1117), `MCC_LIBPATHS` (`--libpaths`, colon-separated, L1118–L1119), `MCC_CRTPREFIX` (`--crtprefix`, colon-separated, L1120–L1121), `MCC_ELFINTERP` (`--elfinterp`, L1122–L1123), `MCC_SWITCHES` (`--mcc-switches`, L1124–L1125), and `MCC_OS_RELEASE` (`--os-release`, L1126–L1127).
- **L1128–L1133** — Three advanced STRING nodes in group "Build flags", defaults empty, mirroring configure's extra-flag options as space-separated strings: `MCC_EXTRA_CFLAGS` (`--extra-cflags`), `MCC_EXTRA_LDFLAGS` (`--extra-ldflags`), and `MCC_EXTRA_LIBS` (`--extra-libs`).
- **L1134–L1137** — When `MCC_EXTRA_CFLAGS` is non-empty, splits it with `separate_arguments(... NATIVE_COMMAND ...)` (respecting the host shell's quoting rules) into `_mcc_extra_cflags` and applies the result globally via `add_compile_options`.
- **L1138–L1141** — Symmetrically, a non-empty `MCC_EXTRA_LDFLAGS` is split into `_mcc_extra_ldflags` and applied globally via `add_link_options`. (`MCC_EXTRA_LIBS` is not consumed here; it is used later in the file.)

#### Host CPU detection (L1143–L1158)

- **L1144** — Copies `CMAKE_SYSTEM_PROCESSOR` into the working variable `_proc`.
- **L1145** — Initializes `_arm_cpuver` to empty (an ARM CPU-version holder filled in by later code outside this range).
- **L1146–L1158** — Maps `_proc` to the canonical `MCC_CPU` value, replicating the configure script's CPU switch: `x86`, `i386`–`i686`, `i86pc`, or `BePC` → `i386` (L1146–L1147); `x86_64`, `amd64`, `x86-64`, or `AMD64` → `x86_64` (L1148–L1149); `aarch64`, `arm64`, or `ARM64` → `arm64` (L1150–L1151); any processor starting with `arm` → `arm` (L1152–L1153); `riscv64` → `riscv64` (L1154–L1155); anything else is a fatal error `Unsupported CPU '<proc>'`, noted as matching configure's "Unsupported CPU" failure (L1156–L1158). (The `MCC_TARGETOS`/suffix selection that follows begins at L1160, outside this range.)

### Lines 1151–2300

This region takes the project from "options are declared" to "targets are defined." It detects the target OS and derives platform file suffixes, applies per-OS defaults (Darwin, the BSDs, Android/Termux), identifies the host compiler and probes ARM ABI features, finalizes and reports the configuration node system, assembles the `_mccdefs` compile-definition list that encodes the whole configuration, sets up include directories, computes the git hash, and builds the optional `c2str` codegen step for `MCC_CONFIG_PREDEFS`. It then collects the compiler's core and backend sources and defines the main build products: the `libmcc` libraries (static/shared, host and musl flavors, via the `mcc_add_libmcc` recipe function), the executables `mcc`, `mcc-static`, `mcc-dynamic` and their `-musl` mirrors, the profiling (`mcc_p`) and sanitizer (`mcc_s`) builds, and finally the `mcc_build_libmcc1` function that compiles the target runtime library `libmcc1.a` with the freshly built compiler itself (the last command in range, L2230, runs through L2382).

#### Target OS detection and platform suffixes (L1160–L1276)

(The CPU-detection `if` chain at L1146–1158 starts before this range and is owned by the previous chunk; the first command starting at or after L1151 is the OS dispatch at L1160.)

- **L1160–1185** — A five-way `if`/`elseif` chain that sets the target OS name `MCC_TARGETOS` plus the three platform file suffixes `MCC_LIBSUF`, `MCC_EXESUF`, and `MCC_DLLSUF`. The Windows branch (L1160–1164) fires when `WIN32` is true, when `CMAKE_SYSTEM_NAME` matches `Windows|MINGW|MSYS|CYGWIN`, or when the `MCC_CONFIG_MINGW32` option is on; it sets `MCC_TARGETOS` to `WIN32` with suffixes `.lib`/`.exe`/`.dll`. The Darwin branch (L1165–1169) sets `Darwin` with `.a`/empty/`.dylib`. The BSD branch (L1170–1174) matches `FreeBSD|OpenBSD|NetBSD|DragonFly` and passes `CMAKE_SYSTEM_NAME` through verbatim with `.a`/empty/`.so`. The Android branch (L1175–1179) sets `Android` with `.a`/empty/`.so`. The fallback `else` (L1180–1184) covers everything else (notably Linux) with `CMAKE_SYSTEM_NAME` as the OS name and `.a`/empty/`.so`.
- **L1187–1263** — Per-OS defaulting block keyed on `MCC_TARGETOS`. The **Darwin** branch (L1187–1200): defaults `MCC_CONFIG_DWARF` to `4` if empty (L1188–1190); defaults `MCC_CONFIG_CODESIGN` to `yes` if empty (L1191–1193); and, if `MCC_CONFIG_NEW_MACHO` is unset and this is not a cross build (L1194–1200), runs `sw_vers -productVersion` to read the macOS version and sets `MCC_CONFIG_NEW_MACHO` to `no` when the major version is below 11 (older macOS cannot consume the new Mach-O layout). The **BSD** branch (L1201–1212) fills in the platform-standard ELF interpreter path per OS when `MCC_ELFINTERP` is empty: FreeBSD `/libexec/ld-elf.so.1`, DragonFly `/usr/libexec/ld-elf.so.2`, NetBSD `/usr/libexec/ld.elf_so`, OpenBSD `/usr/libexec/ld.so`. The **Android** branch (L1213–1262) is the largest: L1214–1218 picks `_android_sysroot` as the Termux prefix `/data/data/com.termux/files/usr` when the `TERMUX_VERSION` environment variable is defined, else `/usr`; L1219–1221 defaults `MCC_SYSROOT` to that; L1222–1223 force-enables `MCC_CONFIG_NEW_DTAGS` and `MCC_DISABLE_RPATH` (Android's linker requires DT_RUNPATH-style tags and rpaths are handled via switches instead); L1224–1226 defaults DWARF version to 4; L1227–1229 enables `MCC_CONFIG_PIE` on every CPU except i386 (Android mandates PIE executables); L1230 turns off `MCC_BUILD_STATIC_LIB` (only shared libmcc on Android); L1231–1242 defaults `MCC_TRIPLET` per CPU to the Android triplets (`arm-linux-androideabi` — additionally seeding `_arm_cpuver` to 7 for ARMv7 — `aarch64-linux-android`, `x86_64-linux-android`, `i686-linux-android`); L1243–1247 sets `_android_s` to the string `64` when `MCC_CPU` ends in `64`, else empty, for use in system paths; L1248–1259 defaults the runtime search paths using the `{B}` (binary dir) / `{R}` (sysroot) placeholders — `MCC_SYSINCLUDEPATHS` to `{B}/include:{R}/include:{R}/include/<triplet>`, `MCC_LIBPATHS` to `{B}:{R}/lib:/system/lib<64?>`, `MCC_CRTPREFIX` to `{R}/lib`, and `MCC_ELFINTERP` to `/system/bin/linker<64?>`; L1260–1262 defaults `MCC_SWITCHES` to `-Wl,-rpath=<sysroot>/lib` so produced binaries find the Termux libc.
- **L1265–1267** — When `MCC_OS_RELEASE` was left empty and the build is not cross-compiling, it is defaulted to `CMAKE_HOST_SYSTEM_VERSION` (the host kernel release), which later becomes the `CONFIG_OS_RELEASE` define.
- **L1269–1273** — Sets `MCC_BIGENDIAN` to `ON` when `CMAKE_C_BYTE_ORDER` reports `BIG_ENDIAN`, otherwise `OFF`.
- **L1276** — Status message summarizing the resolved target: `mcc target: CPU=... OS=... bigendian=...`.

#### CPU define names and host-compiler identification (L1279–L1301)

- **L1279–1283** — Defines a lookup table of five variables `_mcc_cpu_define_<cpu>` mapping each supported CPU name to its preprocessor macro: `i386`→`MCC_TARGET_I386`, `x86_64`→`MCC_TARGET_X86_64`, `arm`→`MCC_TARGET_ARM`, `arm64`→`MCC_TARGET_ARM64`, `riscv64`→`MCC_TARGET_RISCV64`. The right one is dereferenced later at L1521 via `${_mcc_cpu_define_${MCC_CPU}}`.
- **L1284–1294** — Maps `CMAKE_C_COMPILER_ID` to a short host-compiler name `MCC_CC_NAME`: any Clang variant → `clang`, `ModernCC` (mcc building itself) → `mcc`, `MSVC` → `msvc`, `GNU` → `gcc`, anything else → `unknown`. This name drives many compiler-specific branches below and becomes the `CC_NAME=CC_<name>` define.
- **L1295–1301** — Parses `CMAKE_C_COMPILER_VERSION` with a `major.minor` regex into `MCC_GCC_MAJOR`/`MCC_GCC_MINOR`; if the version string does not match, both fall back to `0`. Despite the "GCC" name these hold the version of whatever host compiler is in use, and become the `GCC_MAJOR`/`GCC_MINOR` defines.

#### ARM ABI autodetection and its config nodes (L1303–L1351)

- **L1303–1306** — Initializes the four ARM feature flags `_arm_eabi`, `_arm_vfp`, `_arm_hardfloat`, `_arm_idiv` to `OFF` before probing.
- **L1307–1330** — When the target CPU is 32-bit `arm` and `_arm_cpuver` was not already seeded (e.g. by the Android branch), probes the host compiler's predefined macros: L1308 writes an empty file `_empty.c` into the build dir, and L1309–1312 runs `${CMAKE_C_COMPILER} -dM -E -` with that file as stdin, capturing the macro dump in `_cc_defs` (errors suppressed, exit code in `_cc_defs_rc`). If the probe succeeded (L1313), the dump is scanned: `__ARM_ARCH <n>` sets `_arm_cpuver` to the architecture version (L1314–1316); presence of `__ARM_EABI__` sets `_arm_eabi` (L1317–1319); `__VFP_FP__` or `__ARM_FP` sets `_arm_vfp` (L1320–1322); `__ARM_PCS_VFP` sets `_arm_hardfloat` (L1323–1325); `__ARM_FEATURE_IDIV` sets `_arm_idiv` (L1326–1328).
- **L1332–1336** — Five `mcc_profile_seed` calls that feed the detected values into the config-node system as profile-seeded defaults for `MCC_ARM_EABI`, `MCC_ARM_VFP`, `MCC_ARM_HARDFLOAT`, `MCC_ARM_IDIV`, and `MCC_CPUVER`, so the autodetected values become the effective defaults for the corresponding cache options.
- **L1337–1339** — Declares config node `MCC_ARM_EABI` as an advanced BOOL (default OFF) in group "ARM ABI", visible in the config UI only when `MCC_CPU STREQUAL arm`; help text notes it is autodetected from the host cc.
- **L1340–1342** — Same shape for `MCC_ARM_VFP` (ARM VFP floating-point unit).
- **L1343–1345** — Same shape for `MCC_ARM_HARDFLOAT` (ARM hard-float procedure call standard).
- **L1346–1348** — Same shape for `MCC_ARM_IDIV` (hardware integer divide, `__ARM_FEATURE_IDIV`).
- **L1349–1351** — Declares `MCC_CPUVER` as an advanced STRING node (default empty, `EMPTY_OK` so an empty value is legal), same ARM-only visibility; holds the ARM architecture version 4/5/6/7 for `CONFIG_MCC_CPUVER`.

#### Host triplet and libc discovery on native non-Windows builds (L1353–L1396)

- **L1353–1396** — Guarded on non-WIN32 target and not cross-compiling. Inner block L1354–1377: when `MCC_TRIPLET` is still empty, runs `${CMAKE_C_COMPILER} -dumpmachine` (L1355–1356) to get the compiler's machine triplet; L1364–1370 builds a candidate list `_triplet_cands` containing the dumpmachine string verbatim plus, when it matches `<arch>-(pc|unknown)-<rest>`, a normalized variant with the vendor field dropped (e.g. `x86_64-pc-linux-gnu` → `x86_64-linux-gnu`, the Debian multiarch spelling); the `foreach` at L1371–1376 accepts the first candidate for which `/usr/lib/<cand>/crti.o` exists or `/usr/include/<cand>` is a directory — i.e. the triplet that actually names multiarch directories on this system. (L1357–1363 and L1378–1386 are blank comment-placeholder lines.) L1387 prints the resolved `triplet`/`libpaths`/`crtprefix` status line. L1388–1395: when `MCC_CONFIG_LIBC` is unset, prints advisory hints — if `/lib/ld-uClibc.so.0` exists it suggests `-DMCC_CONFIG_LIBC=uClibc`, and if `/lib/ld-musl-<cpu>.so.1` exists it suggests `-DMCC_CONFIG_LIBC=musl`.

#### Host-compiler flag checks and diagnostics flag set (L1398–L1446)

- **L1398** — Includes CMake's `CheckCCompilerFlag` module for the `check_c_compiler_flag` calls below.
- **L1399–1406** — For host compilers other than mcc itself and MSVC, and additionally excluding clang (L1400): tests whether `-Wno-unused-result` is accepted (result cached in `MCC_HAS_WNO_UNUSED_RESULT`) and, if so, adds it globally to silence unused-result warnings on GCC-family compilers.
- **L1407–1410** — Workaround for old GCC: when the host is gcc with major version exactly 4 and minor NOT in 5–9 (i.e. gcc 4.0–4.4), adds `-D_FORTIFY_SOURCE=0` globally to disable fortify, which those versions mishandle.
- **L1412** — Initializes `MCC_DIAG_FLAGS` to an empty list; this accumulates the diagnostics/warning/debug flags applied per-target when `MCC_DIAGNOSTICS` is on.
- **L1413–1446** — When `MCC_DIAGNOSTICS` is enabled: the MSVC branch (L1414–1425) probes each of `/W4 /Zi` with `check_c_compiler_flag`, using `string(MAKE_C_IDENTIFIER "MCC_HAS_DIAGFLAG_<flag>" _v)` to derive a legal cache-variable name per flag, and appends each accepted flag to `MCC_DIAG_FLAGS` (L1415–1418 are blank comment placeholders). The non-MSVC branch (L1426–1443) does the same probe-and-collect over the GCC/clang set: `-Wall -Wextra -Wnull-dereference`, the suppressions `-Wno-sign-compare -Wno-unused-parameter -Wno-type-limits -Wno-unterminated-string-initialization -Wno-implicit-fallthrough`, plus the debug-friendly `-g3 -fno-omit-frame-pointer`. L1445 reports the final flag list with a status message.

#### Darwin and WIN32 host link options (L1448–L1474)

- **L1448–1461** — Darwin-host link setup, skipped when mcc builds itself: L1449 adds `-flat_namespace` globally (mcc's runtime expects flat symbol namespace); L1450–1452 additionally adds `-undefined warning` for clang older than major 15 (newer linkers reject it); L1453–1456 force-caches `CMAKE_OSX_DEPLOYMENT_TARGET` to `10.6` when unset, mirroring the old Makefile's `MACOSX_DEPLOYMENT_TARGET`; L1457–1460 adds `-arch x86_64` to both compile and link options when targeting x86_64 on an arm64 host (building the Intel-slice compiler under Rosetta).
- **L1463–1474** — WIN32-target link setup: L1464–1466 adds `-static` when the host compiler is gcc (fully static mingw binaries, no runtime DLL dependencies); L1467–1473 adds the strip flag `-s` at link time for compilers that are neither clang nor MSVC when the build type is not `Debug`/`RelWithDebInfo` (L1469–1471 are blank comment placeholders).

#### Install prefix, GNUInstallDirs, and config finalization (L1476–L1512)

- **L1476–1481** — When CMake's install prefix is still the platform default, it is force-overridden to the project-local `${CMAKE_BINARY_DIR}/install` (cached as PATH) and a status message explains that `-DCMAKE_INSTALL_PREFIX=/usr/local` restores a system install. This makes `cmake --install` safe by default.
- **L1483** — Includes `GNUInstallDirs`, defining `CMAKE_INSTALL_BINDIR`, `CMAKE_INSTALL_LIBDIR`, `CMAKE_INSTALL_FULL_LIBDIR`, etc. used throughout the install rules.
- **L1484–1485** — Declares config node `MCC_INSTALL_MCCDIR` (advanced STRING, default empty, group "Install"): the runtime `CONFIG_MCCDIR` directory; empty means `<prefix>/<libdir>/mcc`.
- **L1487–1491** — Converts the boolean `MCC_CONFIG_PREDEFS` option into a numeric `_mcc_predefs_value` of `1` or `0` for embedding into the `CONFIG_MCC_PREDEFS=<n>` define.
- **L1493–1496** — Four `mcc_status_node` calls recording, for the final configuration report: the CPU, the target OS, a formatted host-compiler string (`<name> (<id> <version>)`), and whether the build is cross-compiling.
- **L1498–1501** — Copies `CMAKE_CROSSCOMPILING_EMULATOR` into `MCC_EMULATOR` (used later to run the freshly built mcc under e.g. qemu) and records it as a status node only when actually cross-compiling.
- **L1503–1505** — When targeting 32-bit ARM, records a combined `arm_abi` status node showing eabi/vfp/hardfloat/idiv/cpuver in one line.
- **L1507–1512** — Config-node system finalization: `mcc_apply_node_visibility()` applies the `VISIBLE_WHEN` rules to hide irrelevant cache entries, `mcc_validate_config()` checks values, `mcc_report_config()` prints the grouped configuration report, and when `MCC_REGEN_DOCS` is on, `mcc_generate_node_doc` writes the node reference to `${CMAKE_BINARY_DIR}/config-nodes.md`.

#### Assembling the `_mccdefs` compile-definition list (L1515–L1654)

- **L1515–1519** — Macro `mcc_def_str(_var _name _val)`: if `_val` is non-empty, appends `<name>="<val>"` (a quoted string define) to the list variable named `_var`. Used to add the optional string-valued CONFIG defines without emitting empty ones.
- **L1521** — Seeds `_mccdefs` with the CPU macro selected from the L1279–1283 table: `${_mcc_cpu_define_${MCC_CPU}}=1` (e.g. `MCC_TARGET_X86_64=1`). `_mccdefs` is the master list of configuration defines applied to every compiler target (via the foreach at L2193) and, filtered, passed to mcc itself when building libmcc1.
- **L1523–1539** — ARM-only defines: appends `CONFIG_MCC_CPUVER=<n>` when `MCC_CPUVER` is set (L1524–1526), then `MCC_ARM_EABI=1`, `MCC_ARM_VFP=1`, `MCC_ARM_HARDFLOAT=1` for each enabled flag, and `__ARM_FEATURE_IDIV=1` when hardware divide was detected.
- **L1541–1549** — Object-format / target-OS define: `MCC_TARGET_PE=1` for WIN32, `MCC_TARGET_MACHO=1` for Darwin, `TARGETOS_ANDROID=1` for Android, and `TARGETOS_<name>=1` for each BSD (e.g. `TARGETOS_FreeBSD=1`).
- **L1551–1559** — musl host autodetection: when `MCC_CONFIG_LIBC` is empty, the compiler is not MSVC, the build is native, and the target is neither WIN32 nor Darwin, runs `-dumpmachine` on the host compiler; if the machine string contains `musl`, sets `MCC_CONFIG_LIBC=musl` and reports the defaulting.
- **L1560–1573** — musl ELF interpreter defaulting: when libc is musl and `MCC_ELFINTERP` is empty, derives the musl arch name `_muslarch` — `arm64`→`aarch64`; `arm`→`armhf` if hard-float else `arm` (L1563–1568); otherwise the CPU name verbatim — and sets `MCC_ELFINTERP` to `/lib/ld-musl-<arch>.so.1`.
- **L1575–1579** — Appends the libc-selection define: `CONFIG_MCC_MUSL=1` for musl or `CONFIG_MCC_UCLIBC=1` for uClibc.
- **L1581–1595** — Feature-toggle defines, each appended only when its option is on: `CONFIG_SELINUX=1` (`MCC_WITH_SELINUX`, L1581–1583), `CONFIG_MCC_PIE=1` (L1584–1586), `CONFIG_MCC_PIC=1` (L1587–1589), `CONFIG_NEW_DTAGS=1` (L1590–1592), `CONFIG_CODESIGN=1` (L1593–1595).
- **L1597–1608** — Negative toggles, appended as explicit `=0` when a default-on feature is disabled: `CONFIG_MCC_BCHECK=0` (bounds checking off, L1597–1599), `CONFIG_MCC_ASM=0` (inline assembler off, L1600–1602), `CONFIG_MCC_BACKTRACE=0` (L1603–1605); and `CONFIG_NEW_MACHO=0` when `MCC_CONFIG_NEW_MACHO` is the string `no` (L1606–1608).
- **L1610–1615** — Numeric-valued defines appended when their variable is non-empty: `CONFIG_DWARF_VERSION=<n>` from `MCC_CONFIG_DWARF` and `CONFIG_MCC_SEMLOCK=<v>` from `MCC_CONFIG_SEMLOCK`.
- **L1617–1623** — Seven `mcc_def_str` calls adding the optional quoted-string defines from the corresponding runtime-path variables: `CONFIG_MCC_SYSINCLUDEPATHS`, `CONFIG_MCC_LIBPATHS`, `CONFIG_MCC_CRTPREFIX`, `CONFIG_MCC_ELFINTERP`, `CONFIG_MCC_SWITCHES`, `CONFIG_TRIPLET`, `CONFIG_OS_RELEASE` — each emitted only when the source variable is non-empty.
- **L1625–1627** — Appends `CONFIG_SYSROOT="<path>"` when `MCC_SYSROOT` is non-empty (done manually rather than via the macro, same effect).
- **L1628–1635** — For non-WIN32 targets, computes `_mccdir` — `${CMAKE_INSTALL_FULL_LIBDIR}/mcc` when `MCC_INSTALL_MCCDIR` is empty, else the user's value — and appends `CONFIG_MCCDIR="<dir>"`, the runtime directory where mcc finds its libs/headers. WIN32 mcc locates files relative to the exe instead.
- **L1637–1643** — Builds `_mccdefs_common`, the small set of defines applied globally to *every* translation unit via `add_compile_definitions` (L1643): `MCC_VERSION="<ver>"`, `CC_NAME=CC_<host cc>`, `GCC_MAJOR`/`GCC_MINOR` (host compiler version), and `CONFIG_MCC_PREDEFS=<0|1>`.
- **L1646–1651** — Derives `_mccdefs_flags`: converts each `_mccdefs` entry into a `-D<def>` command-line flag, but *skips any entry containing a double quote* — the string-valued CONFIG defines cannot survive being passed on mcc's command line when compiling libmcc1, and they are irrelevant there. This flag list is consumed by `mcc_build_libmcc1`.
- **L1653–1654** — Status message summarizing the key build defines: CC_NAME, predefs, bcheck, and backtrace settings.

#### Include directories, githash, and the c2str predefs generator (L1656–L1731)

- **L1656** — Adds the binary directory to the global include path (for generated headers such as `mccdefs_.h`).
- **L1657–1666** — Adds the project's source include directories globally: `src`, all five arch backends (`src/arch/i386`, `x86_64`, `arm`, `arm64`, `riscv64`), `src/objfmt`, `src/formats`, and the public `include` directory.
- **L1668–1670** — When `MCC_AUTO_MCCDIR` is on, adds the global define `CONFIG_MCC_AUTO_MCCDIR=1` (mcc derives its runtime dir from the executable location).
- **L1673–1693** — Git version stamp: L1673 quietly finds Git; L1674 initializes `MCC_GITHASH` to empty. When Git exists and the source tree has a `.git` (L1675), it runs `git rev-parse --abbrev-ref HEAD` for the branch name (L1676–1678); if that yielded a branch (L1679), it formats `MCC_GITHASH` via `git log -1 --date=short --pretty=format:"%cd <branch>@%h"` (commit date, branch, short hash; L1680–1683), then runs `git diff --quiet` (L1684–1685) and appends a `*` to the hash when the tree is dirty (L1686–1688). L1691–1693 prints the resulting githash when non-empty; it later becomes the per-target `MCC_GITHASH="<...>"` define.
- **L1695–1731** — The `MCC_CONFIG_PREDEFS` machinery: mcc can embed `runtime/include/mccdefs.h` as a C string so it needs no file at runtime, generated by the `tools/c2str.c` tool. The cross-compiling branch (L1696–1715): `find_program(MCC_HOSMCC ...)` looks for a *host*-runnable C compiler among `$ENV{CC_FOR_BUILD}`, `$ENV{HOSMCC}`, `cc`, `gcc`, `clang` (L1697–1699); if none is found, a fatal error explains the three remedies (`-DMCC_HOSMCC=<cc>`, `CC_FOR_BUILD`, or `-DMCC_CONFIG_PREDEFS=OFF` to load the header at runtime instead) (L1700–1705); otherwise the tool is compiled immediately at configure time with `execute_process` into `${CMAKE_CURRENT_BINARY_DIR}/c2str_host` (L1706–1711, the command spans the chunk boundary from the previous section) and a nonzero exit code is fatal (L1712–1714); `_c2str_dep` is set to the produced file path (L1715). The native branch (L1716–1720) simply declares an `add_executable(c2str tools/c2str.c)` target and uses `$<TARGET_FILE:c2str>` as the tool path with the target itself as the dependency. L1721–1728 declares the `add_custom_command` producing `${CMAKE_CURRENT_BINARY_DIR}/mccdefs_.h` by running the c2str tool on `runtime/include/mccdefs.h`, depending on both the tool and the input header. L1729–1730 wraps the output in a named custom target `mcc_mccdefs`, which every compiler target `add_dependencies` onto so the header exists before compilation.

#### Source collection: core and backend (L1734–L1750)

- **L1734–1736** — Globs `MCC_CORE_SRC` with `CONFIGURE_DEPENDS` (re-glob on rebuild) from `src/*.c` plus `src/objfmt/mccelf.c` (ELF output is compiled on every platform), then removes `src/mcc.c` from the list — the driver TU belongs to the executables, not the library.
- **L1738–1741** — Globs `MCC_BACKEND_SRC` from `src/arch/${MCC_CPU}/*.c` (the code generator for the selected target CPU) and fails fatally if the glob is empty, i.e. no such backend directory exists.
- **L1742–1744** — For x86_64, additionally appends `src/arch/i386/i386-asm.c`: the x86-64 inline assembler reuses the i386 assembler source.
- **L1746–1750** — Appends the target object-format writer to the backend sources: `src/objfmt/mccpe.c` (PE) for WIN32 or `src/objfmt/mccmacho.c` (Mach-O) for Darwin; plain ELF targets need nothing extra since mccelf.c is already in the core list.

#### musl-target helper and the libmcc recipe functions (L1754–L1849)

- **L1754–1781** — Function `mcc_musl_target(cpu armhf out_interp out_triplet)`: pure lookup mapping a CPU (plus an `armhf` boolean for 32-bit ARM) to the musl dynamic-loader path and musl target triplet, returned through the two named output variables via `PARENT_SCOPE`. Mappings: `x86_64`→`/lib/ld-musl-x86_64.so.1` / `x86_64-linux-musl`; `i386`, `arm64` (as `aarch64`), `riscv64` analogously; `arm` picks `armhf`/`arm-linux-musleabihf` when hard-float else `arm`/`arm-linux-musleabi` (L1767–1774); unknown CPUs return empty strings (L1775–1778).
- **L1787–1790** — Comment block documenting the library output-name convention `libmcc[-static|-dynamic][-musl]`: the shape suffix is omitted when only the shared lib is built, so the default build produces a plain `libmcc`, matching the bare `mcc` executable convention.
- **L1791–1802** — Function `mcc_libmcc_output(kind is_musl outvar)`: computes that output name. Starts from `mcc` (L1792); appends `-static` when `kind` is `STATIC` (L1793–1794), or `-dynamic` when kind is SHARED *and* a static lib also exists (`MCC_BUILD_STATIC_LIB` — disambiguation, L1795–1797); appends `-musl` when `is_musl` (L1798–1800); returns via `PARENT_SCOPE` (L1801).
- **L1804–1807** — Comment describing `mcc_add_libmcc` as the single recipe for every libmcc library (host + musl, static + shared): naming, ONE_SOURCE rebuild deps, includes, system libs, extra libs, diagnostics; `is_musl` is a bool and `ARGN` carries extra compile definitions; it reads the shared `_one_src_deps` / `_mcc_extra_libs` computed once below.
- **L1808–1849** — Function `mcc_add_libmcc(tgt kind is_musl [extra defs...])`. L1809 creates the library target from `${LIBMCC_SRC}` with the given kind. L1810–1816: SHARED libs get the `LIBMCC_AS_DLL` private define (export annotations on Windows), `POSITION_INDEPENDENT_CODE ON`, and on Apple additionally `MACOSX_RPATH ON` and a `VERSION` of `${MCC_VERSION_NUMERIC}`. L1817–1819: as a special case, even a *static* lib gets PIC when the host compiler is clang on x86_64 (clang's default non-PIC objects would break consumers linking the archive into shared objects). L1820–1821 computes the output name via `mcc_libmcc_output` and applies it with an explicit `lib` prefix. L1822–1826: when `MCC_ONE_SOURCE` is off, defines `ONE_SOURCE=0` so `libmcc.c` compiles as a driver of separate TUs; when on, attaches `OBJECT_DEPENDS "${_one_src_deps}"` to `src/libmcc.c` so editing any amalgamated source retriggers the single-TU compile. L1827–1829 adds the `mcc_mccdefs` dependency when that generated-header target exists. L1830–1832 exports the public include directory with generator expressions: `$<BUILD_INTERFACE:...>/include` inside the build tree and `$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>` after installation. L1833–1839: on non-WIN32 links `m` publicly, then finds Threads and links `Threads::Threads` plus `${CMAKE_DL_LIBS}` when found. L1840–1842 links the parsed `_mcc_extra_libs` publicly if any. L1843–1845 adds `MCC_DIAG_FLAGS` privately under `MCC_DIAGNOSTICS`. L1846–1848 applies any extra compile definitions passed through `ARGN` (used by the musl variants to inject `_mccdefs_musl`).

#### libmcc source selection and the host libmcc targets (L1851–L1893)

- **L1851–1852** — `LIBMCC_ALL_SRC` is the union of core and backend sources with `src/mcctools.c` removed (mcctools is `#include`d by the mcc driver, not part of the library).
- **L1854–1858** — Chooses the actual library source list `LIBMCC_SRC`: the single amalgamation TU `src/libmcc.c` under `MCC_ONE_SOURCE`, else the full multi-TU list.
- **L1860–1874** — Shared one-time computations (per the comment at L1860–1862): L1863–1870 builds `_one_src_deps` — under ONE_SOURCE, every file in `LIBMCC_ALL_SRC` except `libmcc.c` itself, used as `OBJECT_DEPENDS` so the amalgamation rebuilds when any included source changes; L1871–1874 parses the space-separated `MCC_EXTRA_LIBS` string into the list `_mcc_extra_libs` with `separate_arguments(... NATIVE_COMMAND ...)`.
- **L1876–1882** — Creates the primary `libmcc` target (comment L1876–1877 notes the naming: `libmcc-static.a`, or `libmcc.so` when it is the only lib, else `libmcc-dynamic.so`; `_mccdefs` are applied later by the foreach at L2193): STATIC via `mcc_add_libmcc(libmcc STATIC FALSE)` when `MCC_BUILD_STATIC_LIB`, else SHARED.
- **L1884–1893** — Tracks which target is the shared lib and optionally adds a companion: `_libmcc_shared` is empty when the primary is static, else `libmcc` (L1885–1889); when both `MCC_BUILD_STATIC_LIB` and `MCC_BUILD_DYNAMIC_LIB` are on, a second target `libmcc_shared` is created as SHARED (named `libmcc-dynamic` by the output convention) and `_libmcc_shared` points at it (L1890–1893).

#### The `mcc` executable (L1895–L1923)

- **L1895–1898** — Comment (L1895–1897) explains the canonical binary: `src/mcc.c` defaults to `ONE_SOURCE=1` and `#include`s `libmcc.c`, so the entire compiler lives in this one TU and `mcc` does not link the libmcc library at all (dynamic, one-source). L1898 declares `add_executable(mcc src/mcc.c)`.
- **L1899–1900** — Attaches `OBJECT_DEPENDS` on `src/mcctools.c` (which mcc.c `#include`s) so edits to it retrigger the compile.
- **L1901** — Adds the source root as a private include dir (so `mcc.c` can `#include "src/..."` relative paths).
- **L1902–1908** — On non-WIN32: links `m` privately, finds Threads, and links `Threads::Threads` + `${CMAKE_DL_LIBS}` when found (dlopen/dlsym are needed for `-run`).
- **L1909–1911** — Links the parsed extra libs privately when `MCC_EXTRA_LIBS` is set.
- **L1912–1914** — Adds the `MCC_GITHASH="..."` private define when a githash was computed.
- **L1915–1917** — Depends on `mcc_mccdefs` when the predefs header target exists.
- **L1918–1920** — Adds the `-s` strip link option when `MCC_BUILD_STRIP` is on and the compiler is not MSVC.
- **L1921–1923** — Applies `MCC_DIAG_FLAGS` privately under `MCC_DIAGNOSTICS`.

#### The `mcc-static` executable (L1925–L1963)

- **L1925–1927** — Comment: `mcc-static` is fully static (`-static`), and `MCC_ONE_SOURCE` decides the path — ON gives a self-contained single TU like `mcc`, OFF gives a driver TU (`ONE_SOURCE=0`) linked against the libmcc archive.
- **L1928–1963** — Whole block gated on `MCC_BUILD_STATIC_EXE`. L1929–1931 creates `mcc_static` from `src/mcc.c` with the same mcctools.c `OBJECT_DEPENDS`. L1932 sets the output name `mcc-static`. L1933 adds the source-root include dir. L1934–1937: under multi-TU mode, defines `ONE_SOURCE=0` and links the `libmcc` library. L1938–1944 links m/Threads/dl on non-WIN32 as usual. L1945–1947 links extra libs. L1948–1953: for non-MSVC compilers, adds the `-static` link option, plus `-s` when stripping is requested. L1954–1956 adds the githash define. L1957–1959 depends on `mcc_mccdefs`. L1960–1962 applies diagnostics flags.

#### The `mcc-dynamic` executable (L1969–L2005)

- **L1969–1975** — Comment explaining why `mcc-dynamic` requires a multi-TU libmcc: the driver's `mcctools.c` calls libmcc-internal `ST_FUNC` helpers (`pstrcpy`, ...) which only get external linkage in a multi-TU build; an amalgamated libmcc keeps them `static`, and a shared libmcc never exports them on PE (no dllexport) — so mcc-dynamic links the *primary* `libmcc` (the static archive when `MCC_BUILD_STATIC_LIB=ON`), which resolves the helpers on every platform.
- **L1976–2002** — Gated on `MCC_BUILD_DYNAMIC_EXE AND NOT MCC_ONE_SOURCE`. L1977–1979 creates `mcc_dynamic` from `src/mcc.c` with the mcctools.c dep. L1980 names it `mcc-dynamic`. L1981 defines `ONE_SOURCE=0`. L1982 links `libmcc`. L1983–1990: when the primary libmcc is shared (not static) and rpaths are not disabled, sets `INSTALL_RPATH` — on Apple to `@executable_path/../${CMAKE_INSTALL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}` (relocatable plus absolute) and elsewhere to the absolute `${CMAKE_INSTALL_FULL_LIBDIR}` — so the installed exe finds `libmcc.so`. L1991–1993 githash define; L1994–1996 mcc_mccdefs dependency; L1997–1999 `-s` strip under MCC_BUILD_STRIP for non-MSVC; L2000–2002 diagnostics flags.
- **L2003–2005** — `elseif` fallback: when a dynamic exe was requested but `MCC_ONE_SOURCE=ON`, prints a status message that mcc-dynamic needs a multi-TU libmcc and is skipped.

#### The musl variant family (L2012–L2142)

- **L2012–2142** — Entire block gated on `MCC_BUILD_MUSL AND NOT MCC_TARGETOS STREQUAL "WIN32" AND NOT APPLE` (musl variants only make sense on ELF hosts). Contents:
- **L2013** — Calls `mcc_musl_target` with the current CPU and hard-float flag, receiving the musl ELF interpreter into `_mi` and the musl triplet into `_mt`.
- **L2015–2029** — Builds `_mccdefs_musl`, the musl flavor of the define list: copies every `_mccdefs` entry except any existing `CONFIG_MCC_ELFINTERP=`, `CONFIG_TRIPLET=`, and `CONFIG_MCC_MUSL=` entries (L2015–2022), then appends `CONFIG_MCC_MUSL=1` (L2023) and, when non-empty, the musl-specific `CONFIG_MCC_ELFINTERP="<_mi>"` (L2024–2026) and `CONFIG_TRIPLET="<_mt>"` (L2027–2029).
- **L2033–2047** — musl libraries, mirroring the host ones: `libmcc_musl` is built STATIC or SHARED depending on `MCC_BUILD_STATIC_LIB`, via `mcc_add_libmcc(... TRUE ${_mccdefs_musl})` (L2033–2037); `_libmcc_musl` records the primary target name (L2038); `_libmcc_musl_shared` is empty when static-primary, else `libmcc_musl` (L2039–2043); and when both static and dynamic libs are requested, a companion SHARED `libmcc_musl_shared` is added and recorded (L2044–2047).
- **L2050–2071** — `mcc_musl` executable (comment L2050: mirror of `mcc` — self-contained ONE_SOURCE, dynamic, musl-targeting): created from `src/mcc.c` with the mcctools.c `OBJECT_DEPENDS` (L2051–2053); gets the full `_mccdefs_musl` define set privately (L2054) — unlike host `mcc`, whose defines come from the foreach at L2193; source-root include dir (L2055); output name `mcc-musl` (L2056); links m and, when found, Threads + dl (L2057–2061); githash define (L2062–2064); mcc_mccdefs dependency (L2065–2067); `-s` strip under MCC_BUILD_STRIP for non-MSVC (L2068–2070); and installs to `${CMAKE_INSTALL_BINDIR}` (L2071).
- **L2073–2103** — `mcc_static_musl` (comment L2073: mirror of `mcc-static`; static, `MCC_ONE_SOURCE` decides the path), gated on `MCC_BUILD_STATIC_EXE`: same construction pattern — mcc.c + mcctools dep (L2075–2077), `_mccdefs_musl` defines (L2078), include dir (L2079), output name `mcc-static-musl` (L2080); under multi-TU mode adds `ONE_SOURCE=0` and links `${_libmcc_musl}` (L2081–2084); links m/Threads/dl (L2085–2089); non-MSVC gets `-static` plus optional `-s` (L2090–2095); githash (L2096–2098); mcc_mccdefs dep (L2099–2101); installed to the bindir (L2102).
- **L2106–2129** — `mcc_dynamic_musl` (comment L2106–2108: mirror of `mcc-dynamic`, NOT one-source, links libmcc-musl; multi-TU only; being ELF-only the shared musl lib would also resolve the internals, but it links the primary for one uniform code path), gated on `MCC_BUILD_DYNAMIC_EXE AND NOT MCC_ONE_SOURCE`: mcc.c + mcctools dep (L2110–2112); `ONE_SOURCE=0` plus the musl defines (L2113); output name `mcc-dynamic-musl` (L2114); links `${_libmcc_musl}` (L2115); sets `INSTALL_RPATH` to the full libdir when the lib is shared and rpath is not disabled (L2116–2118); githash (L2119–2121); mcc_mccdefs dep (L2122–2124); optional `-s` (L2125–2127); installed to the bindir (L2128).
- **L2132–2141** — Installs the musl libraries: `libmcc_musl` with RUNTIME→bindir (Windows DLL placement, moot here), LIBRARY and ARCHIVE→libdir (L2132–2135); and the companion `libmcc_musl_shared` with the same destinations when that target exists (L2136–2141).

#### Profiling and sanitizer builds (L2145–L2191)

- **L2145–2171** — `mcc_p` profiling build, gated on `MCC_BUILD_PROFILE`: created from `src/mcc.c` with the mcctools dep (L2146–2148); source-root include dir (L2149); compiled with `-pg` (gprof instrumentation, L2150) and the `MCC_PROFILE` define (L2151); L2152–2157 adds `CONFIG_MCC_STATIC` on non-WIN32 only — the comment explains that this define switches `-run` to the built-in symbol table, and that on WIN32 even a `-static` exe resolves libc via LoadLibrary (msvcrt.dll) and mingw's stdin/stdout/stderr macros are not addressable lvalues, so the define must not be used there; L2158 links with `-pg -static`; L2159–2160 links `m` explicitly (comment: mcc references libm sin/cos/... and a `-static` link needs it spelled out, like mcc); L2161–2164 Threads + dl when found; L2165–2167 githash define; L2168–2170 mcc_mccdefs dependency.
- **L2173–2191** — `mcc_s` sanitizer build, gated on `MCC_BUILD_SANITIZE`: same skeleton — mcc.c + mcctools dep (L2174–2176), include dir (L2177) — but compiled *and* linked with `-fsanitize=address,undefined` (L2178–2179); links m (L2180) and Threads/dl (L2181–2184); githash (L2185–2187); mcc_mccdefs dep (L2188–2190). No install rule; it is a developer tool.

#### Applying `_mccdefs` and `CONFIG_MCC_STATIC` to the built targets (L2193–L2209)

- **L2193–2197** — A `foreach` over the seven host-flavor target names `mcc mcc_static mcc_dynamic libmcc libmcc_shared mcc_p mcc_s`; for each that actually exists as a target, applies the full `_mccdefs` list as private compile definitions. This is why none of those targets set `_mccdefs` individually (the musl targets got `_mccdefs_musl` directly instead).
- **L2199–2209** — Gated on `MCC_BUILD_STATIC_EXE` and not MSVC/Apple/WIN32-target: the comment (L2201–2203) states that `CONFIG_MCC_STATIC` switches `-run` to the built-in symbol table (no dlsym) and must apply ONLY to the genuinely-static executables, while dynamic `mcc`/`mcc-dynamic` keep the dlsym path so `-run` can resolve arbitrary libc symbols. The `foreach` (L2204–2208) adds the define privately to `mcc_static` and `mcc_static_musl` when those targets exist.

#### libmcc1 runtime-library build machinery (L2212–L2382)

- **L2212** — Sets `MCC_LIBMCC1_EXTRA_O` to the list `runmain bt-exe bt-dll bt-log bcheck`: the runtime objects that are shipped as standalone `.o` files next to the archive rather than being archived into `libmcc1.a` (mcc links them individually for `-run`, backtrace, and bounds-check support).
- **L2214–2228** — Function `_mcc_libmcc1_src(obj outvar)`: maps a runtime object base name to its source file. L2215–2216 define the two source roots `_lib` (`runtime/lib`) and `_win` (`runtime/win32/lib`). Special cases: `crt1w` → win32 `crt1.c` and `wincrt1w` → win32 `wincrt1.c` (L2217–2220) — the `w` variants are wide-character (Unicode entry) builds of the same sources, distinguished later by a `-DUNICODE` style extra flag handled in the compile loop; names matching `crt1|wincrt1|dllcrt1|dllmain|winex` map to the win32 file of the same name (L2221–2222); `chkstk` maps to win32 `chkstk.c` (L2223–2224); everything else comes from `runtime/lib/<obj>.c` (L2225–2227). The result is returned via `PARENT_SCOPE`.
- **L2230–2382** — Function `mcc_build_libmcc1(tname mcctgt cpu os native archive objdir out_extra)` — the full recipe that compiles the target runtime library using an mcc build itself (or optionally the host gcc). Parameters: `tname` is the custom-target name (e.g. `libmcc1`), `mcctgt` the mcc executable target used as the compiler, `cpu`/`os` the runtime's target CPU and OS, `native` a boolean saying the runtime targets the build host (enables run-support objects and emulator launch), `archive` the output archive path, `objdir` the directory for archived objects, and `out_extra` the name of a parent-scope variable receiving the list of non-archived extra `.o` files. The command starts at L2230 and extends past the range boundary to L2382; it is documented fully here.
  - **L2231–2240** — Selects the base object list per CPU: `_common` is `stdatomic atomic builtin alloca alloca-bt complex` (L2231); i386/x86_64 use `libmcc1` + common (L2232–2233); arm adds `armeabi armflush` (L2234–2235); arm64/riscv64 use `lib-arm64` + common (L2236–2237); any other CPU is a fatal error (L2238–2239).
  - **L2242–2248** — Derives the booleans `_win` and `_osx` from the `os` parameter (`WIN32` → `_win ON`, `Darwin` → `_osx ON`).
  - **L2250–2261** — Native-only objects: `runmain` and `tcov` are always added for native runtimes (L2251); when `MCC_CONFIG_BACKTRACE` is on, `bt-exe` and `bt-log` are added (L2252–2253), plus `bt-dll` on Windows (L2254–2256), and `bcheck` when `MCC_CONFIG_BCHECK` is also on (L2257–2259) — the bounds checker depends on backtrace support.
  - **L2263–2283** — Per-OS objects: Windows adds the CRT startup family `crt1 crt1w wincrt1 wincrt1w dllcrt1 dllmain winex` (L2264) and `chkstk` on i386/x86_64/arm64 (L2265–2267); Darwin adds `va_list` only on x86_64 (L2268–2271); every other (ELF) OS adds per-CPU pieces — `pic86` on i386, `va_list` on x86_64, `armflush` on arm64, `lib-riscv` on riscv64 (L2273–2281) — plus `dsohandle` on all ELF targets (L2282).
  - **L2285–2292** — Builds `_arch_incs`, the `-I` flag list handed to the compiler: `src`, `include`, `src/formats`, all five `src/arch/*` directories, and `src/objfmt`.
  - **L2293–2304** — Chooses `_xflags`, the mcc compile flags: on Windows (L2293–2297) it is the quote-free `_mccdefs_flags` plus `-B<srcdir>/runtime/win32` (mcc's "root" for finding win32 lib/include), `-I<srcdir>/runtime/include`, `-I${CMAKE_BINARY_DIR}`, and the arch includes; elsewhere (L2298–2304, with two blank comment placeholder lines) the same but with `-fPIC` added and `-B<srcdir>/runtime` as the root.
  - **L2306–2307** — Computes `_extra_dir` as the directory containing the archive (extra `.o` files are placed beside it) and creates `objdir` at configure time.
  - **L2309–2322** — Optional host-gcc mode: `_use_gcc` turns ON only for the *native* runtime when `MCC_LIBMCC1_USEGCC` is set (L2309–2311). In that mode `_gccflags` is `_mccdefs_flags` plus `-fPIC -fno-omit-frame-pointer -Wno-unused-function -Wno-unused-variable`, the runtime include dir, the binary dir, and the arch includes (L2312–2316); and `_bflags_gcc` — extra debug flags used only for `bcheck` — is `-gstabs` when `MCC_CONFIG_DWARF` is empty, else `-gdwarf` (L2317–2321).
  - **L2324–2327** — `_launch` is set to `${MCC_EMULATOR}` for native runtimes (so the freshly built mcc can be executed under an emulator when cross-compiling), otherwise empty.
  - **L2329–2363** — The per-object compile loop: `_archived` and `_extra` accumulate outputs (L2329–2330). For each object name, `_mcc_libmcc1_src` resolves the source (L2332). In gcc mode (L2333–2340), the compiler is `${CMAKE_C_COMPILER}` with `_gccflags`, no target dependency and no emulator prefix, and `bcheck` gets the `_bflags_gcc` debug flags appended. In mcc mode (L2341–2348), the compiler is `$<TARGET_FILE:${mcctgt}>` with `_xflags`, depends on the `mcctgt` target, runs under `_launch`, and `bcheck` gets `-bt` (backtrace) appended. L2350–2356 routes the output: objects in `MCC_LIBMCC1_EXTRA_O` go to `<archive dir>/<obj>.o` and the `_extra` list, everything else to `<objdir>/<obj>.o` and `_archived`. L2357–2362 declares the `add_custom_command` that compiles `_src` to `_out` with `${_run} ${_cc} -c ... ${_flags}`, depending on the compiler target (if any) and the source, with comment `"<tname> -c <obj>"`.
  - **L2365–2379** — The archive step: in gcc mode (L2365–2371) `${CMAKE_AR} rcs` packs the archived objects; in mcc mode (L2372–2379) mcc's own built-in archiver is used via `${_launch} $<TARGET_FILE:mcctgt> -ar rcs`, additionally depending on the mcc target.
  - **L2380–2381** — Declares the named `ALL` custom target `${tname}` depending on the archive and the extra objects, and returns the extra-object list through `${out_extra}` to the parent scope.

### Lines 2301–3450

This region finishes the `mcc_build_libmcc1()` runtime-library build function, then instantiates it for the host target; it defines and drives the entire cross-compiler matrix (`mcc_add_cross_mcc()` plus the `MCC_ENABLE_CROSS` loop over eleven `cpu[-os]` targets, including musl variants); it handles Windows `libmcc.def` generation; it contains the whole installation section (binaries, libraries, headers, CMake package config files, per-target `mcc` directory layout); and it opens the `MCC_BUILD_TESTS` block: four embedded CMake "driver" scripts written into the build tree (`run_mcc_exe.cmake`, `run_mcctest.cmake`, `run_asm_c_connect.cmake`, `run_dash_s_roundtrip.cmake`), the `mcc_build` fixture, and the registration of the exec-, cli-, diff3-, parts-, preprocess-, embed-API, hello/vla/abi, and mcctest/mcctest-bcheck test suites, including the reference-compiler resolution logic and the large embedded `run_preprocess.cmake` differential-preprocessing script.

#### Tail of `mcc_build_libmcc1()` (L2301–L2382)

- **L2301–L2382** — The remainder of the `mcc_build_libmcc1()` function body (the non-Windows `_xflags` arm, output-directory setup, the optional host-gcc mode, the emulator launch prefix, the per-object compile loop, the archive step, the `ALL` custom target, and the closing `endfunction()`). The function starts at L2230 and is documented in full — including these lines — under the **Lines 1151–2300** section's entry for L2230–2382. Documentation of new commands resumes below at L2387.

#### Host libmcc1 build, runtime header staging, coverage binary (L2387–L2426)

- **L2387–L2391** — Chooses the host runtime archive location `MCC_LIBMCC1_ARCHIVE`: on a WIN32 target it is `${CMAKE_BINARY_DIR}/lib/libmcc1.a` (the PE layout keeps libraries under a `lib/` subdirectory), otherwise `${CMAKE_BINARY_DIR}/libmcc1.a` at the build root.
- **L2392–L2394** — Invokes `mcc_build_libmcc1` for the host: target name `libmcc1`, compiler target `mcc`, cpu `${MCC_CPU}`, OS `${MCC_TARGETOS}`, `native=ON`, the archive path chosen above, extra-object directory `${CMAKE_BINARY_DIR}/lib`, and output variable `LIBMCC1_EXTRA_OBJS` receiving the standalone CRT object paths (used later by the install rules).
- **L2396–L2397** — At configure time, copies all `*.h` files from `runtime/include/` into `${CMAKE_BINARY_DIR}/include`, so the build tree works as a self-contained `-B` directory with the runtime headers in place.
- **L2403–L2408** — WIN32-only build-tree staging: copies the Windows-specific headers from `runtime/win32/include/` into the build `include/` dir, globs all `*.def` import-library definition files from `runtime/win32/lib/` into `_win_build_defs`, and copies them into the build `lib/` dir so the in-tree compiler can link against Windows DLLs.
- **L2410–L2426** — Coverage self-build, gated on `MCC_BUILD_COVERAGE`: a custom command (L2411–2424) runs the built mcc (under `${MCC_EMULATOR}` if set) on `src/mcc.c` with `-ftest-coverage`, producing `${CMAKE_BINARY_DIR}/mcc_c` — a coverage-instrumented mcc compiled by mcc itself. The long `-I` list mirrors the compiler's own include requirements (binary dir, source root, `src`, `src/formats`, `src/objfmt`, `src/arch/i386`, `src/arch/${MCC_CPU}`, `include`), and both the source dir and binary dir are given as `-B` search roots. It depends on `mcc`, `libmcc1`, and the source file. L2425 wraps it in an always-built `mcc_cov` target.

#### `mcc_add_cross_mcc()` — cross-compiler executable factory (L2428–L2480)

- **L2428–L2432** — Explanatory comment plus function signature: `mcc_add_cross_mcc(base_tgt base_out static_tgt static_out defs)` builds a self-contained cross compiler `base_out` (dynamically linked) and, when `MCC_BUILD_STATIC_EXE` is on, a `static_out` sibling linked `-static`. `defs` is the *name* of a list variable holding the target's compile definitions (passed by name so the function can dereference it). The comment notes the naming convention `mcc-<arch>[-static][-musl]` with the musl suffix always last.
- **L2433–L2436** — Builds `_variants`, a list of `target|output|kind` records: always the dynamic variant, plus the static variant when `MCC_BUILD_STATIC_EXE` is on and the host compiler is not MSVC (MSVC has no `-static`).
- **L2437–L2447** — The variant loop: each record is split on `|` into `_vt` (target name), `_vo` (output name), `_vk` (kind, `dynamic` or `static`). L2442 creates the executable from `src/mcc.c`; L2443–2444 declare an `OBJECT_DEPENDS` on `src/mcctools.c` (which `mcc.c` textually includes, so edits to it must trigger a rebuild); L2445 sets the `OUTPUT_NAME` to `_vo`; L2446 adds the source root as a private include dir; L2447 applies the compile definitions via double dereference `${${defs}}` of the caller's list-variable name.
- **L2448–L2456** — Comment: cross compilers run on *this* host, so they link the host libc/libm/pthread/dl (the static built-in `-run` symbol table references libm symbols). On non-WIN32 target OS the target links `m`, then `find_package(Threads)` and, if found, `Threads::Threads` plus `${CMAKE_DL_LIBS}`.
- **L2457–L2465** — Static-kind handling: the comment explains that a static exe uses the built-in `-run` symbol table (no `dlsym`), like the host `mcc-static`, except on WIN32 where `-run` resolves via `LoadLibrary` even in a `-static` exe (and `&stdin` doesn't compile under mingw), so `CONFIG_MCC_STATIC` is defined only when the target OS is not WIN32 (L2461–2463). L2464 adds the `-static` link option unconditionally for static variants.
- **L2466–L2477** — Optional per-target extras: `MCC_DIAGNOSTICS` adds the `MCC_DIAG_FLAGS` warning set (L2466–2468); `MCC_GITHASH` embeds the git hash as a string define (L2469–2471); if the `mcc_mccdefs` generated-header target exists, the variant depends on it (L2472–2474); `MCC_BUILD_STRIP` (and not MSVC) adds `-s` to strip the binary at link time (L2475–2477).
- **L2478–L2480** — Installs each variant executable into `${CMAKE_INSTALL_BINDIR}`; `endforeach`/`endfunction` close the loop and function.

#### Cross-compiler matrix under `MCC_ENABLE_CROSS` (L2482–L2592)

- **L2482–L2487** — Opens the cross-build block. Computes `_mccdir_install`, the installed per-target runtime directory: defaults to `${CMAKE_INSTALL_FULL_LIBDIR}/mcc` when the `MCC_INSTALL_MCCDIR` cache option is empty, otherwise the user-supplied path.
- **L2489–L2490** — `MCC_X` lists the eleven cross targets built: `i386`, `x86_64`, `i386-win32`, `x86_64-win32`, `x86_64-osx`, `arm`, `arm64`, `arm64-win32`, `arm-wince`, `riscv64`, `arm64-osx`.
- **L2491–L2493** — Opens the per-target `foreach` loop; each iteration resets `_cdefs` (compile definitions) to empty and `_cos` (target OS) to the default `Linux`.
- **L2494–L2533** — The target dispatch chain setting `_ccpu` (cpu name), `_cos`, and `_cdefs` per target: `i386` → cpu i386, `MCC_TARGET_I386` (L2494–2496); `x86_64` → `MCC_TARGET_X86_64` (L2497–2499); `i386-win32` → cpu i386, OS WIN32, `MCC_TARGET_I386 MCC_TARGET_PE` (L2500–2503); `x86_64-win32` → x86_64/WIN32/PE (L2504–2507); `x86_64-osx` → OS Darwin, `MCC_TARGET_X86_64 MCC_TARGET_MACHO` (L2508–2511); `arm` → `MCC_TARGET_ARM MCC_ARM_VFP MCC_ARM_EABI MCC_ARM_HARDFLOAT` (VFP hard-float EABI Linux arm, L2512–2514); `arm64` → `MCC_TARGET_ARM64` (L2515–2517); `arm64-win32` → arm64/WIN32/PE (L2518–2521); `arm-wince` → the arm hard-float defines plus `MCC_TARGET_PE` with OS WIN32 (L2522–2525); `riscv64` → `MCC_TARGET_RISCV64` (L2526–2528); `arm64-osx` → arm64/Darwin/MACHO (L2529–2532). `endif()` at L2533.
- **L2535–L2546** — Second dispatch: `_ctriplet` is the Debian-style GNU multiarch triplet used to locate a cross sysroot under `/usr/<triplet>`, set only for the five Linux targets: `i686-linux-gnu`, `x86_64-linux-gnu`, `arm-linux-gnueabihf`, `aarch64-linux-gnu`, `riscv64-linux-gnu`; it stays empty for win32/osx targets (which have no such sysroot convention).
- **L2547–L2554** — When a triplet is known, `_cpath_defs` gets the baked-in path configuration defines for the cross compiler: `CONFIG_SYSROOT="/usr/<triplet>"` (the cross libc root), `CONFIG_MCC_CRTPREFIX="{R}/lib"` (CRT objects under the sysroot lib dir; `{R}` is mcc's runtime placeholder for the sysroot), `CONFIG_MCC_LIBPATHS="{R}/lib:{B}"` (library search: sysroot lib then mcc's `-B` dir), and `CONFIG_MCC_SYSINCLUDEPATHS="{B}/include:{R}/include"` (system includes: mcc's own headers first, then the sysroot's).
- **L2556–L2560** — Assembles `_cross_defs` = target cpu/format defines + path defines + `CONFIG_MCC_CROSSPREFIX="<target>-"` (the prefix mcc uses to find target-prefixed binutils/files). For WIN32-OS targets it also appends `CONFIG_MCCDIR="${_mccdir_install}/win32"` so the installed cross compiler looks in the win32 subdirectory of the mcc runtime dir.
- **L2561–L2564** — Comment (target ids match output names, same convention as the host mcc) plus the call `mcc_add_cross_mcc(mcc-${_t} "mcc-${_t}" mcc-${_t}-static "mcc-${_t}-static" _cross_defs)`, creating the dynamic `mcc-<arch>` and (when enabled) `mcc-<arch>-static` executables with the definitions list passed by name.
- **L2566–L2570** — Builds the cross runtime library for this target: `mcc_build_libmcc1(${_t}-libmcc1 mcc-${_t} ...)` with the target cpu/OS, `native=OFF` (so no emulator prefix and no gcc fallback), archive `${CMAKE_BINARY_DIR}/${_t}-libmcc1.a`, object dir `lib-${_t}`, extras into `_cross_extra`. L2569–2570 install the archive into `${_mccdir_install}`.
- **L2575–L2590** — Musl variants (blank separator lines L2572–2574 precede this), gated on `MCC_BUILD_MUSL` AND a non-empty triplet AND `_cos` being Linux: `mcc_musl_target("${_ccpu}" ON _cmi _cmt)` (L2576) maps the cpu to the musl ELF interpreter path `_cmi` and musl triplet `_cmt`. L2577–2581 build `_cpath_defs_musl`, identical in shape to the glibc path defines but with sysroot `/usr/<musl-triplet>`. L2582–2584 add `CONFIG_MCC_ELFINTERP="<interp>"` when the interpreter path is known (so binaries get the musl dynamic loader). L2585–2586 assemble `_cross_defs_musl` = cpu defines + musl path defines + `CONFIG_MCC_MUSL=1` + the same crossprefix. L2587–2589 (with a comment restating the "musl stays last" naming rule) call `mcc_add_cross_mcc` for `mcc-<t>-musl` and `mcc-<t>-static-musl`.
- **L2591–L2592** — `endforeach()` closes the target loop; `endif()` closes the `MCC_ENABLE_CROSS` block.

#### Windows libmcc.def generation (L2594–L2602)

- **L2594–L2602** — When targeting WIN32 and a shared libmcc target exists (`_libmcc_shared` set earlier): a custom command runs the built mcc with `-impdef` on the libmcc DLL (`$<TARGET_FILE:${_libmcc_shared}>`) to generate `${CMAKE_BINARY_DIR}/libmcc.def`, the export-definition file needed to create import libraries for the DLL; depends on `mcc` and the shared library. L2601 wraps it in the always-built `libmcc_def` target.

#### Installation and CMake package config (L2604–L2681)

- **L2604–L2609** — Includes `GNUInstallDirs` (defines `CMAKE_INSTALL_BINDIR`/`LIBDIR`/`INCLUDEDIR` etc.) and computes `_mccdir`, the installed mcc runtime directory, with the same default logic as `_mccdir_install`: `${CMAKE_INSTALL_FULL_LIBDIR}/mcc` unless `MCC_INSTALL_MCCDIR` overrides it.
- **L2611–L2617** — Installs the `mcc` executable into the bin dir; conditionally installs `mcc_static` and `mcc_dynamic` too, but only if those optional targets were created earlier in the file.
- **L2619–L2630** — Installs the `libmcc` library into the `mccTargets` export set with RUNTIME→bindir (DLLs), LIBRARY→libdir (shared libs), ARCHIVE→libdir (static libs/import libs), and INCLUDES usage-requirement pointing at the include dir; L2624–2630 do the same for `libmcc_shared` when that target exists.
- **L2631–L2632** — Installs the public API header `include/libmcc.h` into `${CMAKE_INSTALL_INCLUDEDIR}`.
- **L2634–L2639** — Includes `CMakePackageConfigHelpers`, sets `_mcc_cmakedir` to `${CMAKE_INSTALL_LIBDIR}/cmake/mcc` (the package-config location), and installs the `mccTargets` export as `mccTargets.cmake` with the `mcc::` namespace so consumers link `mcc::libmcc`.
- **L2640–L2641** — Writes the config template `mccConfig.cmake.in` into the build dir as a single inline string: `@PACKAGE_INIT@`, `include(CMakeFindDependencyMacro)`, `find_dependency(Threads)` (libmcc's public dependency), inclusion of the sibling `mccTargets.cmake`, and `check_required_components(mcc)`.
- **L2642–L2648** — `configure_package_config_file` expands the template into `mccConfig.cmake` for `INSTALL_DESTINATION ${_mcc_cmakedir}`; `write_basic_package_version_file` generates `mccConfigVersion.cmake` with `${PROJECT_VERSION}` and `SameMajorVersion` compatibility.
- **L2649–L2652** — Installs both generated package files (`mccConfig.cmake`, `mccConfigVersion.cmake`) into `${_mcc_cmakedir}`.
- **L2657–L2663** — Installs the host `libmcc1.a` archive together with the `LIBMCC1_EXTRA_OBJS` standalone CRT objects: into `${_mccdir}/lib` on WIN32 targets (matching the PE lib-subdirectory layout) or directly into `${_mccdir}` otherwise.
- **L2665–L2666** — Installs the runtime headers (`runtime/include/`, `*.h` only) into `${_mccdir}/include`.
- **L2668–L2681** — WIN32-only install rules: the win32 headers directory into `${_mccdir}/include` (L2669–2670); all `runtime/win32/lib/*.def` files (globbed into `_win_defs`) into `${_mccdir}/lib` (L2671–2672); the generated `libmcc.def` into the libdir, marked `OPTIONAL` since it only exists when the shared lib was built (L2673–2676); the `examples/` `*.c` sources into an `examples` install directory (L2677–2678); and `tests/embed/api_basic.c` alongside them as an embedding example (L2679–2680).

#### mccbuild helper and test-infrastructure setup (L2683–L2717)

- **L2683–L2685** — On UNIX hosts that are not cross-compiling, builds the `mccbuild` helper executable from `tools/build.c`.
- **L2687–L2691** — Opens the `MCC_BUILD_TESTS` block: `enable_testing()`, then three convenience variables used throughout the test registrations: `_mcc_exe` = `$<TARGET_FILE:mcc>` (generator expression for the built compiler path), `_bdir` = the build dir (passed as `-B`), `_idir` = the source-tree runtime include dir (passed as `-I`).
- **L2696–L2699** — Defines `mcc_skip_test(_name _reason)` (blank lines L2693–2695 precede it): registers a test whose command merely echoes `SKIP: <reason>` via `cmake -E echo` and sets `SKIP_REGULAR_EXPRESSION "SKIP:"` so ctest reports it as skipped rather than passed; any extra arguments (`${ARGN}`) are forwarded as additional test properties.
- **L2702–L2717** — Writes the compile-and-run driver `${CMAKE_BINARY_DIR}/run_mcc_exe.cmake`: L2703 seeds it with `set(EMU "${MCC_EMULATOR}")` (baking the configured emulator into the script), then L2704–2717 append a bracket-quoted (`[==[ ]==]`) literal script that: splits the `-DSRCS` and `-DRUNARGS` parameters into lists with `separate_arguments`; runs `${EMU} ${MCC} -B${BDIR} -I${IDIR} <srcs> -o ${OUTFILE}` and fails fatally with captured stdout/stderr if the compile returns nonzero; then executes the produced binary under the emulator with the run arguments and fails fatally if its exit code is nonzero.

#### Embedded test-driver scripts: mcctest, asm-c-connect, dash-S (L2719–L2847)

- **L2719–L2764** — Writes the differential-test driver `${CMAKE_BINARY_DIR}/run_mcctest.cmake` (EMU seeded on L2720, body appended as a bracket literal). The script: defines reference output paths `mcctest.gcc`/`mcctest.ref` under `OUTDIR` (L2722–2723); prepends the reference compiler's own directory to `PATH` (L2727–2734, `;` separator on Windows, `:` elsewhere) so a relocated gcc finds its companion tools; splits `REFFLAGS` and `TESTDEFS` (L2735–2736); builds `SRC` with the reference `CC` using `-I` for the runtime include dir, source dir, and build dir, the test defines, `-w -O0 -std=gnu11 -fno-omit-frame-pointer`, the target-specific reference flags, and `-lm` (L2737–2743, fatal on failure); runs the reference exe capturing stdout to `mcctest.ref` (L2744); splits `MCCARGS` (extra mcc arguments, e.g. `-b`) and defines the mcc-side paths `mcctest.mcc`/`mcctest.out` (L2745–2747); builds the same source with mcc under the emulator with the same defines plus `MCCARGS` (L2748–2754, fatal on failure); runs it capturing output (L2755); byte-compares the two outputs with `cmake -E compare_files` and, on mismatch, reads both files and fails fatally printing a `--- cc --- / --- mcc ---` diff dump (L2756–2763).
- **L2766–L2801** — Writes `${CMAKE_BINARY_DIR}/run_asm_c_connect.cmake`, which verifies that assembling/compiling `tests/asm/asm_c_connect/part1.c` and `part2.c` together in one mcc invocation (L2772–2779, producing `asm-c-connect` and its output `asm-c-connect.out1`) behaves identically to compiling each part separately with `-c` (L2780–2781, `acc1.o`/`acc2.o`) and linking the objects (L2782–2787, `asm-c-connect-sep`, run to `asm-c-connect.out2` on L2788–2789). L2792–2800 compare the two output files and fail fatally with a single-vs-separate dump on mismatch. This exercises inline-asm/C symbol interconnection across both build paths.
- **L2803–L2847** — Comment (L2803–2805) then the `-S` round-trip driver `${CMAKE_BINARY_DIR}/run_dash_s_roundtrip.cmake`: `mcc -S <src>` must produce an assembly listing that re-assembles with mcc's own integrated assembler into a program behaving identically to the directly-compiled one. The script hardcodes `SRC` to `tests/asm/dash_s_roundtrip/prog.c` (L2809) and proceeds in four commented steps: (1) direct compile + run → reference output `dashS.out1` (L2811–2818); (2) `mcc -S` emitting `dashS.s`, fatal on failure (L2819–2824); a sanity check (L2825–2829) that the listing contains a tab-indented `.text` directive, a `main:` label, and a `.size	main` directive, failing with the full listing otherwise; (3) re-assemble `dashS.s` and link to `dashS-via`, run → `dashS.out2` (L2830–2837); (4) `compare_files` on the two outputs, failing with a direct-vs-via-`-S` dump on mismatch (L2838–2846).

#### Build fixture, host-gate invariant, exec-suite (L2849–L2872)

- **L2850–L2852** — Registers the `mcc_build` test, whose command is `cmake --build ${CMAKE_BINARY_DIR}`, and marks it `FIXTURES_SETUP MCC_BUILT`: every test declaring `FIXTURES_REQUIRED MCC_BUILT` will force this build step to run (and pass) first.
- **L2854–L2861** — Comment (L2854–2857) documents the "Platform spec" invariant from TODO.md: raw host macros (`_WIN32`, `__APPLE__`, ...) may only be tested inside `src/mcchost.{h,c}`; everything else must use `MCC_HOST_*` macros or `host_*` functions, keeping mcchost the single source of truth for the HOST axis. The `host-gate-invariant` test enforces this by running `cmake -DSRCDIR=<src> -P cmake/host_gate_check.cmake`.
- **L2863–L2867** — Builds `exec_runner` from `tests/exec/runner.c` with `tests/exec` as a private include dir, then registers the `exec-suite` test passing it the mcc path, build dir, runtime include dir, the `tests` source directory, and the binary directory as positional arguments.
- **L2870–L2872** — `exec-suite` properties: requires the `MCC_BUILT` fixture; `SKIP_RETURN_CODE 77` (the runner exits 77 to signal a skip); and an `ENVIRONMENT` string handing the runner the test context — `MCC_TEST_EMU` (emulator), `MCC_TEST_CPU`, `MCC_TEST_OS`, plus three `$<IF:$<BOOL:...>,1,0>` generator expressions converting `MCC_CONFIG_ASM`, `MCC_CONFIG_BCHECK`, and `MCC_CONFIG_BACKTRACE` into `1`/`0` flags (`MCC_TEST_ASM`, `MCC_TEST_BCHECK`, `MCC_TEST_BACKTRACE`) so the runner can gate individual test cases on compiler features.

#### cli-suite registration (L2880–L2912)

- **L2880–L2881** — If `MCC_EMULATOR` is set (emulated host), the cli-suite is skipped via `mcc_skip_test` with the reason that the structural readelf/nm suite runs on a native host only.
- **L2890–L2895** — Otherwise, `_cli_sh` starts empty; on a WIN32 target, `find_program(MCC_TEST_SH NAMES sh bash ...)` looks for a POSIX shell (cached, with a DOC string explaining it runs the cli-suite pipelines on Windows) and `_cli_sh` is set to the result.
- **L2896–L2897** — If targeting WIN32 and no shell was found, the suite is skipped with the reason "needs a POSIX shell (no sh/bash found; set MCC_TEST_SH)".
- **L2898–L2911** — Otherwise the suite is registered: `cli_runner` is built from `tests/cli/runner.c` with `tests/cli` as a private include dir (L2899–2900); the `cli-suite` test passes the mcc path, build dir, include dir, a `cli-work` scratch directory in the build tree, and the `tests/cli` source dir (L2901–2903). L2904 assembles `_cli_env` with `MCC_TEST_CPU`, `MCC_TEST_OS`, the `MCC_TEST_ASM` 1/0 generator expression, and `MCC_TEST_DWARF=${MCC_CONFIG_DWARF}`; L2905–2907 append `MCC_TEST_SH` when a shell was found. L2908–2910 set the fixture requirement, `SKIP_RETURN_CODE 77`, and the environment. L2911–2912 close the inner and outer conditionals.

#### diff3 reference-compiler resolution and diff3-/parts-suite (L2927–L3022)

- **L2927–L2930** — Two cache `FILEPATH` options: `MCC_DIFF3_GCC` and `MCC_DIFF3_CLANG` let the user pin explicit gcc/mingw and clang reference compilers for the diff3 and preprocess suites, overriding auto-detection.
- **L2932–L2952** — `DIFF3_GCC` resolution: if the cache override is set, use it (L2932–2933). Otherwise `find_program(DIFF3_GCC NAMES gcc cc)` (L2935); then `mcc_find_gnu_gcc(_diff3_gnu_gcc)` (L2939–2942) preferentially replaces the result with a verified *GNU* gcc (guarding against systems where `gcc` is actually clang, e.g. macOS); finally, if still unset on a WIN32 host, `mcc_mingw_resolve()` is called and the downloaded winlibs `_MCC_MINGW_X86_64_GCC` is used when it exists (L2946–2951).
- **L2954–L2970** — `DIFF3_CLANG` resolution: cache override wins (L2954–2955); else `find_program(DIFF3_CLANG NAMES clang)` (L2957); if not found, `mcc_clang_resolve()` is called and the self-contained downloaded clang `_MCC_CLANG_EXE` is used when it exists (L2964–2969).
- **L2971–L2974** — `_diff3_sh_ok` gate: defaults `TRUE`, forced `FALSE` when targeting WIN32 without an `MCC_TEST_SH` POSIX shell (the diff3 runner needs shell pipelines on Windows).
- **L2975–L2993** — diff3-suite registration chain: when running natively (no emulator) with both reference compilers and a usable shell, build `diff3_runner` from `tests/diff3/runner.c` (with `tests/exec` on its include path, L2976–2977) and register `diff3-suite` passing mcc, bdir, idir, the tests source dir, a `diff3-work` scratch dir, and both reference compiler paths (L2978–2981). L2982–2985 build `_diff3_env` (`MCC_TEST_CPU`/`OS`/`ASM` and optional `MCC_TEST_SH`); L2986–2988 apply the fixture, skip code 77, and environment. The `elseif` (L2989–2990) skips with the missing-shell message; the final `else` (L2991–2992) skips with "needs native host + both gcc and clang (build the clang-toolchain target for a self-contained clang)".
- **L2995–L3022** — parts-suite. The comment (L2995–2997) explains it as a 3-way (gcc/clang/mcc) unit test of each `tests/diff/parts/run_*.c` wrapper — the same `parts/*.h` units that are `#include`-aggregated into `full_language.c` (run by mcctest), here exercised in isolation. On WIN32 (L2998–3010) the suite is skipped, with a long comment: the 3-way differential assumes all three compilers share one C99 libc, but on the PE target mcc links legacy msvcrt (no C99 `<complex.h>`/`<tgmath.h>`/`<fenv.h>` libm entry points; legacy printf formats like `1.#IND00`, 6-digit `%a`, ~17-digit `%f`) while gcc/clang references target UCRT (strict C99 `nan(ind)`, full precision; clang cannot even compile `<tgmath.h>` against UCRT's `struct _Fcomplex`), and unlike mcctest the parts wrappers carry no legacy-msvcrt reference flags — the units are still covered on Windows in aggregate by mcctest. The `elseif` (L3011–3019) registers `parts-suite` when native, both DIFF3 compilers exist, `MCC_CONFIG_ASM` is on, and `tests/diff/run_parts.cmake` exists: it invokes that script with `-DGCC/-DCLANG/-DMCC/-DBDIR/-DIDIR`, `-DPARTS` (the parts source dir) and `-DWORK` (a `parts-work` scratch dir), requiring the `MCC_BUILT` fixture. The `else` (L3020–3021) skips with "needs native host + gcc + clang + integrated assembler".

#### preprocess-suite: embedded 3-way `-E` differential driver (L3029–L3263)

- **L3029–L3033** — Opens the preprocess-suite block, gated on native execution (no emulator) plus both `DIFF3_GCC` and `DIFF3_CLANG`; sets `_preprocess_driver` to `${CMAKE_BINARY_DIR}/run_preprocess.cmake`.
- **L3034–L3251** — `file(WRITE ...)` emits the entire driver as one bracket-quoted literal (L3035–3057 are blank lines inside the literal, reserved comment space). Its contents:
  - **L3058** — `cmake_minimum_required(VERSION 3.20)` inside the script (needed for `cmake_language(EXIT)`).
  - **L3060–L3073** — Parameter validation: `MCC`, `BDIR`, `IDIR`, `TDIR` must be passed as `-D` definitions or the script fails fatally; `GCC` and `CLANG` default to the bare names `gcc`/`clang`; `V` (verbose) defaults to `0`.
  - **L3076–L3085** — `find_program` resolves `PP_GCC`/`PP_CLANG` from the given names; if either is missing the script prints `SKIP: no gcc`/`SKIP: no clang` and exits 77 (ctest skip code).
  - **L3102–L3116** — Same-compiler detection: resolves both paths with `REALPATH`; `PP_SAME` becomes `TRUE` if they are the same file, or (L3108–3112) if their `--version` outputs are non-empty and identical (catching distinct wrappers of one compiler). L3114–3116 print a note that with a single compiler there is no independent consensus, so implementation-defined divergences will be SKIPPED rather than FAILED.
  - **L3121–L3139** — `pp_norm(raw outvar)` normalizes preprocessor output for comparison: escapes backslashes and semicolons so the text survives CMake list handling (L3123–3124), splits on newlines into a list (L3125), then per line drops `#`-prefixed line markers (L3128–3130), collapses runs of spaces/tabs/CRs to a single space (L3131), strips leading/trailing whitespace (L3132), skips now-empty lines (L3133–3135), and re-accumulates the survivors newline-joined into the caller's variable (L3136–3138).
  - **L3142–L3154** — `pp_diag(compiler file outvar)` runs `<compiler> -E <file>` discarding stdout, captures stderr, lowercases it, and returns `TRUE` iff it matches `error|warning` — i.e. whether that compiler diagnoses the file during preprocessing.
  - **L3156–L3162** — Globs all `*.c` and `*.S` files recursively under `TDIR`, sorts them for deterministic order, and initializes counters `PASS`/`SKIP`/`FAIL` to 0 and the `FAILED` name accumulator to empty.
  - **L3164–L3243** — The main per-file loop (computes `rel`, the path relative to `TDIR`, on L3165) with three branches: (1) **`.S` files** (L3167–3181): a smoke test that `mcc -B -I -E` neither crashes nor hangs on assembler input (10-second `TIMEOUT`); exit 0 counts as PASS, anything else FAILs with an `(asm-E-crash/hang)` tag. (2) **files under `diagnostics/`** (L3183–3207): runs `pp_diag` with both reference compilers; if either diagnoses, mcc's `-E` stderr must also contain error/warning (PASS) or the file FAILs with `(no-diagnostic)`; if neither reference diagnoses, the case is SKIPped. (3) **all other `.c` files** (L3209–3242): runs `gcc -E` and `clang -E`, normalizes both with `pp_norm`; if the references disagree there is no consensus and the file is SKIPped (L3217–3221); on consensus, mcc's normalized `-E` output must equal it (PASS); if mcc diverges but `PP_SAME` is set the case is SKIPped with an explanatory message (one compiler is not independent consensus, L3231–3235); otherwise it FAILs (L3236–3240).
  - **L3245–L3250** — Prints the `PASS=/SKIP=/FAIL=` summary; if any test failed, prints the failed-file list and exits 1, else exits 0 explicitly.
- **L3252–L3259** — Registers the `preprocess-suite` test: `cmake -DMCC/-DBDIR/-DIDIR -DTDIR=<src>/tests/preprocess -DGCC=${DIFF3_GCC} -DCLANG=${DIFF3_CLANG} -P <driver>`; properties: `MCC_BUILT` fixture and `SKIP_RETURN_CODE 77` (matching the driver's skip exits).
- **L3260–L3263** — The `else` arm skips the suite with "needs native host + both gcc and clang (build the clang-toolchain target for a self-contained clang)".

#### Embedding-API, hello, VLA, and ABI tests (L3265–L3330)

- **L3265–L3271** — Builds `libmcc_test` from `tests/embed/api_basic.c`, linked against `libmcc` with the source root as include dir, and registers the `libtest` test passing `-B<builddir>` and `-I<runtime include>` so the embedded compiler instance can find its runtime; requires the `MCC_BUILT` fixture.
- **L3275–L3281** — Same pattern for the extended embedding API: `libmcc_test_extra` from `tests/embed/api_extra.c`, registered as `libtest-extra` with the same `-B`/`-I` arguments and fixture.
- **L3283–L3299** — Multithreaded embedding test, gated on `MCC_CONFIG_BACKTRACE` and the existence of `tests/embed/api_threaded.c`: builds `libmcc_test_mt` linked to `libmcc` (L3284–3286), links `Threads::Threads` when `find_package(Threads)` succeeds (L3287–3290), and registers `libtest-mt` passing the workload source `tests/embed/mt_workload.c` plus the usual `-B`/`-I` (L3291–3295), with the fixture. The `else` (L3297–3298) records a skip: "requires MCC_CONFIG_BACKTRACE (multithread libmcc test)".
- **L3301–L3312** — Hello-world tests, gated on `examples/ex1.c` existing: `hello-run` (L3302–3306) invokes the mcc binary directly with `-B`/`-I` and `-run examples/ex1.c` (in-memory compile-and-execute path); `hello-exe` (L3307–3311) uses the `_exe_driver` script with `-DSRCS=examples/ex1.c` and `-DOUTFILE=hello${MCC_EXESUF}` to test the compile-to-disk-then-run path. Both require `MCC_BUILT`.
- **L3314–L3320** — `vla_test-run`, gated on `tests/behavior/vla.c` existing: compiles and runs the variable-length-array behavior test through the same `_exe_driver` script, output `vla_test${MCC_EXESUF}`, with the fixture.
- **L3322–L3330** — ABI test, gated on `tests/embed/abi.c` existing: builds `abitest` linked against `libmcc` (the test uses the library API to compile ABI probes and calls them from host-compiled code), registers `abitest-cc` with `-B`/`-I` arguments and the fixture.

#### mcctest reference compiler, refflags, and mcctest/mcctest-bcheck registration (L3341–L3481)

- **L3341–L3346** — Cache option `MCC_REF_CC` (FILEPATH): an explicit GCC-compatible reference compiler for the differential tests (mcctest / mcctest-bcheck); the DOC string notes it defaults to the host compiler and is auto-detected from PATH on an MSVC host. L3344–3346 seed the working variables from the current facts: `_ref_cc` from the cache value, `_ref_cc_name` from `MCC_CC_NAME` (host compiler family), `_ref_cc_major` from `MCC_GCC_MAJOR` (host gcc major version).
- **L3347–L3364** — Auto-detection when `_ref_cc` is empty: if targeting WIN32 and the host compiler is not GNU, the comment (L3349–3353) explains that the WIN32 refflags below are mingw-gcc-specific (`-lgcc`/`-lmingwex`/legacy msvcrt.dll) so an MSVC or clang-msvc host cannot serve as the reference (clang-msvc cannot even compile `full_language.c` against UCRT `<fenv.h>`); therefore `mcc_mingw_resolve()` is run and the downloaded winlibs gcc `_MCC_MINGW_X86_64_GCC` is preferred (L3354–3356), falling back to `find_program(_mcc_ref_gcc NAMES gcc)` on PATH (L3357–3359). In every other configuration the reference is simply the host compiler `${CMAKE_C_COMPILER}` (L3361–3363).
- **L3365–L3377** — When the chosen reference differs from the host compiler, its identity must be re-probed: `execute_process` runs `<ref_cc> -dumpversion` (L3368–3370); if it succeeds and the output starts with digits, `_ref_cc_major` is set to that leading number via `CMAKE_MATCH_1` (L3371–3372), else to 0 (L3374); `_ref_cc_name` is forced to `gcc` (L3376) since only gcc-compatible references reach this path. These feed the `CC_NAME`/`GCC_MAJOR` test defines below.
- **L3379–L3411** — Target-specific reference-compiler flags `_mcctest_refflags` (default empty). For WIN32 (L3380–3406): `_msvcrt_start` points at `tests/support/msvcrt_start.c` (L3383), and the extensive comment (L3384–3404) explains the byte-identity problem: mcc's PE target links the legacy system msvcrt.dll whose printf prints `1.#IND00`/`1.#INF00`, 6-digit `%a` mantissas and ~17-significant-digit `%f`, while a UCRT-based mingw (e.g. winlibs, the pinned CI toolchain) routes printf to UCRT's strict C99 implementation (`nan(ind)`, full precision, and a 0xC0000409 fastfail on specifiers msvcrt tolerates) through two mechanisms — mingw's `<stdio.h>` inlining `__mingw_printf`, and libmsvcrt's import library binding printf to ucrtbase. The fix: `-D__USE_MINGW_ANSI_STDIO=0` suppresses the inline, `-nostdlib` drops `-lmsvcrt`, and the C runtime is resolved directly from `c:/windows/system32/msvcrt.dll` placed inside a `--start-group`/`--end-group` with `-lmingwex` (so libmingwex's helper functions — isblank, imaxdiv, lgamma, rint, the atoll/lldiv/strto*/snprintf wrappers — back-reference the same DLL); on an msvcrt-based mingw this is a no-op. Additionally (L3400–3404), some mingw runtimes reach SEH setjmp through the `__intrinsic_setjmp`/`__intrinsic_setjmpex` intrinsics whose defining archive is the un-linkable libmsvcrt, so `msvcrt_start.c` provides those symbols as tail jumps to msvcrt.dll's own `_setjmp`/`_setjmpex`. L3405–3406 set the resulting flag string: `-D__USE_MINGW_ANSI_STDIO=0 -nostdlib <msvcrt_start.c> -lgcc -Wl,--start-group -lmingwex c:/windows/system32/msvcrt.dll -Wl,--end-group -lkernel32`. For `arm` (L3407–3408) the flags are `-marm` (force ARM mode, matching mcc's non-Thumb codegen); for `i386` (L3409–3410) they are `-fno-PIC -fno-PIE -Wl,-z,notext` (non-PIC code with text relocations permitted, matching mcc's i386 output model).
- **L3412–L3481** — The mcctest/mcctest-bcheck registration if-chain (starts at L3412 and extends past L3450; documented fully). Branch 1 (L3412–3414): no reference compiler was found — both `mcctest` and `mcctest-bcheck` are skipped with a message naming the host compiler ID and suggesting `MCC_REF_CC` or gcc on PATH. Branch 2 (L3415–3430): the reference is GNU gcc on a Darwin host — both tests are skipped because a GNU gcc reference on Darwin/arm64 diverges from clang on implementation-defined/UB ABI corners (coverage instead comes from the Apple-clang mcctest and the diff3-suite). Branch 3 (L3431–3448): WIN32 target on arm64 — both skipped; the comment (L3432–3446) explains the only gcc-compatible reference in the arm64-Windows runner is an x86_64 mingw under emulation, which can never be byte-identical to native arm64 mcc: the `asm_test` in `legacy_meta.h` is x86-only inline asm gated on `__i386__`/`__x86_64__` (the x86_64 reference emits output where arm64 mcc hits the empty stub), and printf differs by libc (emulated winlibs mingw is UCRT-based printing C99 `nan`/`inf` and full precision, while mcc's arm64 target links legacy msvcrt printing `1.#QNAN0`/`1.#INF00` with 6-digit `%a`; `-D__USE_MINGW_ANSI_STDIO=0` cannot switch a UCRT mingw to msvcrt without linking msvcrt.dll directly, which bfd ld cannot do for the ARM64X DLL, and no native arm64 mingw ships in the runner); arm64 codegen is instead covered by exec-suite and pe-native-conformance. Branch 4 (L3449–3477): when `tests/diff/full_language.c` exists and `MCC_CONFIG_ASM` is on, `mcctest` is registered (L3450–3457) running the `_mcctest_driver` script with `-DCC` (reference compiler), `-DMCC`, `-DSRC=full_language.c`, `-DSRCDIR`, `-DBDIR`, `-DIDIR`, `-DOUTDIR=<builddir>`, the quoted `-DREFFLAGS` string, and `-DTESTDEFS="-DCC_NAME=CC_<name> -DGCC_MAJOR=<major>"` (letting the test source adapt to the reference compiler's identity), with the `MCC_BUILT` fixture. The rest of the chain — the fixture property, the bcheck sub-chain (`mcctest-bcheck` registration or skip), the final no-assembler `else`, and the closing `endif()` at L3481 — is documented entry-by-entry at the start of the **Lines 3451–4550** section (L3457–L3481).

### Lines 3451–4550

This final region of `CMakeLists.txt` lives almost entirely inside the test-registration block (`if(BUILD_TESTING ...)` closed at L4448) and then finishes with top-level developer/packaging targets. It registers the `mcctest` differential test's fixture properties and its bounds-checked variant; the x86 `asm-c-connect-test` and `-S` roundtrip tests; byte-exact `-S` roundtrips for the arm64/riscv64 cross compilers; three large inline-generated CMake driver scripts (the i386 `fastcall` ABI interop test, the gcc c-torture `gcctestsuite` target, and the `arm-asm-testsuite` target that diffs mcc's ARM assembler against GNU `as`); compile-only "orphan source" tests (examples, X11, win32 examples); PE conformance tests under wine and on native Windows; a family of Mach-O structural/codegen/image/libc tests using the osx cross compilers; the optional qemu-user cross-architecture conformance matrix (Gentoo stage3 download + run drivers, per arch × libc); and finally the top-level `qemu-docker` custom target, `tags`/`ETAGS`/`dist` developer targets, and CPack packaging configuration. Note: the `add_test(NAME mcctest ...)` command spanning L3450–3456 starts at L3450 and is owned by the previous chunk; documentation here begins at L3457.

#### mcctest fixture wiring and bounds-checked variant (L3457–L3481)

- **L3457** — Sets the `FIXTURES_REQUIRED MCC_BUILT` property on the `mcctest` test (registered at L3450–3456), so it only runs after the `MCC_BUILT` fixture-setup test (the build step) has succeeded.
- **L3458–3464** — If bounds checking is compiled in (`MCC_CONFIG_BCHECK`) but the target OS is `WIN32`, skips `mcctest-bcheck` via `mcc_skip_test` with an explanatory reason. The comment block (L3459–3463) explains why: mcc's bounds-checking runtime cannot instrument the PE/msvcrt path — the bounds-checked build faults inside msvcrt-invoked callbacks (the signal handler that `raise()` calls) and library calls such as `strftime` whose buffers the checker does not track; the plain `mcctest` still covers Windows codegen for this program.
- **L3465–3474** — Otherwise, if `MCC_CONFIG_BCHECK` is on (non-Windows): L3466 creates the `${CMAKE_BINARY_DIR}/bcheck` output directory; L3467–3473 registers the `mcctest-bcheck` test, invoking `cmake -P ${_mcctest_driver}` with the same parameters as `mcctest` (`-DCC` = reference compiler, `-DMCC` = mcc executable, `-DSRC` = `tests/diff/full_language.c`, `-DSRCDIR`/`-DBDIR`/`-DIDIR` for source/runtime/include dirs, `-DREFFLAGS` = reference-compiler flags, `-DTESTDEFS` defining `CC_NAME=CC_<refname>` and `GCC_MAJOR=<major>` so the test source can adapt to the reference compiler) but with two differences: `-DOUTDIR` points at the separate `bcheck` subdirectory and `-DMCCARGS=-b` passes mcc's `-b` bounds-checking flag. L3474 gives it the `FIXTURES_REQUIRED MCC_BUILT` dependency.
- **L3475–3477** — `else()` branch: when bounds checking is not built, `mcctest-bcheck` is registered as a skip with reason "requires MCC_CONFIG_BCHECK (bounds-checked differential test)".
- **L3478–3481** — Closing `else()` of the outer chain (reached when `tests/diff/full_language.c` is missing or `MCC_CONFIG_ASM` is off): skips both `mcctest` ("requires integrated assembler (MCC_CONFIG_ASM)") and `mcctest-bcheck` ("requires integrated assembler + MCC_CONFIG_BCHECK"), then `endif()` closes the whole reference-cc dispatch.

#### asm-c-connect and dash-s roundtrip tests (L3483–L3505)

- **L3483–3489** — If the native target CPU is i386 or x86_64 (regex `^(i386|x86_64)$`), the integrated assembler is enabled, and `tests/asm/asm_c_connect/part1.c` exists, registers the `asm-c-connect-test` test: runs `cmake -P ${_asmconnect_driver}` with `-DMCC` (mcc executable), `-DBDIR`/`-DIDIR` (runtime/include dirs), `-DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}/tests`, and `-DOUTDIR=${CMAKE_BINARY_DIR}`. L3489 requires the `MCC_BUILT` fixture. This test exercises mixed assembly/C linking on x86.
- **L3490–3493** — Otherwise skips `asm-c-connect-test` with the reason "requires x86 target with integrated assembler (host target: ${MCC_CPU})".
- **L3495–3501** — If the CPU is exactly `x86_64`, the assembler is enabled, and `tests/asm/dash_s_roundtrip/prog.c` exists, registers `dash-s-roundtrip`: runs `cmake -P ${_dashS_driver}` with the same `-DMCC/-DBDIR/-DIDIR/-DSRCDIR/-DOUTDIR` parameter set, verifying that compiling via `-S` then reassembling is behaviorally equivalent to direct `-c`. L3501 sets `FIXTURES_REQUIRED MCC_BUILT`.
- **L3502–3505** — Skip branch: `dash-s-roundtrip` requires an x86_64 target with the integrated assembler.

#### Byte-exact -S roundtrip on fixed-width cross targets (L3507–L3529)

- **L3507–3513** — Comment explaining the next tests: on the fixed-width cross targets (arm64, riscv64), the disassemblers guarantee that `mcc -c` and reassembled `mcc -S` output have byte-identical section contents. x86 targets are only covered behaviorally by `dash-s-roundtrip` because the assembler there legally re-encodes some immediate/branch widths; arm is link-equivalent but not object-byte-exact because its assembler uses the gas `imm24=-2` pipeline-bias convention on local-branch relocs where the compiler leaves 0, and `arm-link.c` recomputes the field either way.
- **L3514** — Builds the `seccmp` helper executable from `tests/support/seccmp.c`, a section-bytes comparison tool used by the byte-exact roundtrip driver.
- **L3515–3529** — `foreach(_dsb_arch arm64 riscv64)` loop. For each arch, if the `mcc-${_dsb_arch}` cross-compiler target exists (L3516), registers `dash-s-bytes-${_dsb_arch}` (L3517–3522): runs `tests/asm/run_dash_s_bytes.cmake` with `-DMCC=$<TARGET_FILE:mcc-${_dsb_arch}>` (generator expression resolving to the cross compiler binary), `-DSECCMP=$<TARGET_FILE:seccmp>` (the comparison helper), `-DSRC` = the same `dash_s_roundtrip/prog.c` source, and `-DWORK` = a per-arch work directory under the build tree. L3523–3524 sets `FIXTURES_REQUIRED MCC_BUILT`. Otherwise (L3525–3528) the test is skipped with "requires the mcc-<arch> cross compiler (cmake --preset cross)". L3529 ends the loop.

#### Known-blocked test and cache variables for external suites (L3531–L3542)

- **L3531–3537** — After five blank lines, permanently registers `asm-gas-directives` as a skip with the reason "blocked: integrated assembler lacks sgdtq/sidtq/swapgs encodings (gas_directives.S:811)" — documenting a known assembler gap rather than silently omitting the test.
- **L3539–3540** — Declares the cache variable `MCC_GCCTESTSUITE_PATH` (type PATH, default empty): the location of a gcc.c-torture directory consumed by the `gcctestsuite` custom target defined below.
- **L3541–3542** — Declares the cache variable `MCC_ARM_CROSS_COMPILE` (type STRING, default empty): a binutils prefix such as `arm-linux-gnueabi-` used by the `arm-asm-testsuite` target to find `as`/`objdump`.

#### i386 fastcall ABI interop driver script (L3544–L3665)

- **L3544** — Sets `_fastcall_drv` to `${CMAKE_BINARY_DIR}/run_i386_fastcall.cmake`, the path of a generated driver script.
- **L3545** — Writes the first line of the driver, baking the configure-time `BUILDDIR` (`${CMAKE_BINARY_DIR}`) into the script as a literal `set()`.
- **L3546–3665** — Appends the rest of the driver as a `[==[ ... ]==]` bracket literal (evaluated only when the test runs, not at configure time). The embedded script does the following:
  - **L3547–3554** — Defaults `GCC` to `gcc` if not passed via `-DGCC=`, then `find_program(_gcc "${GCC}")`; if no gcc is found it prints `SKIP: no gcc` and returns (the test's `SKIP_REGULAR_EXPRESSION` catches this).
  - **L3558–3561** — Defaults `M32` to `-m32` if undefined and splits it into the `_m32` argument list with `separate_arguments` (so an empty `-DM32=` yields no flag, used for a native i686 mingw gcc).
  - **L3562–3565** — Sets `_w` to the working directory `${BUILDDIR}/i386-fastcall-abi`, wipes and recreates it, and writes a trivial `m32probe.c` (`int main(void){return 0;}`).
  - **L3566–3571** — Probe-compiles `m32probe.c` with the reference gcc plus the `-m32` flags; if that fails, prints a SKIP message ("32-bit reference build unavailable") and returns — this handles hosts without 32-bit multilib support.
  - **L3572–3575** — If `IMCC` (the i386 mcc binary path passed via `-DIMCC=`) is undefined or missing, prints a SKIP and returns.
  - **L3580–3592** — Object-format compatibility probe: writes `fmtprobe.c` (a `probe_fn` definition) compiled with mcc `-c`, and `fmtmain.c` (a caller) compiled with gcc `-m32 -c`, then attempts to link both objects with gcc. If any of the three steps fails, prints a SKIP explaining the object-format mismatch (for example mcc emitting ELF vs a mingw gcc expecting PE/COFF) and returns.
  - **L3593–3600** — Writes `callee.c` containing five `__attribute__((fastcall))` functions covering ABI corner cases: `mix_ll` (int, long long, int — a 64-bit arg among register candidates), `small` (char/short/int sub-word promotion), `ptr2` (two pointer args in ECX/EDX), `ll_first` (long long as first arg), and `fs` (a 2-int struct `P2` passed by value between int args).
  - **L3601–3617** — Writes `caller.c` declaring the same five fastcall prototypes and a `main` that calls each with known values, OR-ing a distinct failure bit (1, 2, 4, 8, 16) into `f` for each wrong result, and returns `f` as the exit code — so exit 0 means all calls agreed on the ABI.
  - **L3618–3626** — Initializes `_fail 0` and defines the `_cc(cc out)` helper macro: runs `${cc} ${ARGN} -o ${out}`, and on nonzero exit prints a warning with the captured stdout/stderr and sets `_fail 1`.
  - **L3627–3630** — Compiles four objects: callee and caller with mcc (`e_t.o`, `l_t.o`) and with the reference gcc at `-m32 -O0` (`e_g.o`, `l_g.o`).
  - **L3631–3646** — Defines the `_check(name)` macro: links the given object pair with gcc `-m32` into `${_w}/run`; on link failure prints `FAIL <name> (link)` and sets `_fail`; on success executes the binary and reports `PASS` if it exits 0, otherwise `FAIL <name> (exit <rc>)`.
  - **L3647–3649** — Runs the three interop combinations: mcc caller with gcc callee, gcc caller with mcc callee, and mcc caller with mcc callee — validating both directions of fastcall ABI compatibility.
  - **L3650–3659** — Negative test: writes `unsup.c` declaring a fastcall function with a `double` before an integer register candidate and compiles it with mcc; mcc is expected to *reject* this unsupported case, so a successful compile is reported as `FAIL unsupported float-before-reg case should error` while a compile error is a PASS ("errors cleanly").
  - **L3660–3665** — Final verdict: prints `ALL OK` if `_fail` is 0, otherwise `message(FATAL_ERROR "i386-fastcall-abi: FAILURES")` to fail the ctest run. L3665 closes the bracket literal.

#### i386-fastcall-abi test registration (L3666–L3686)

- **L3666–3678** — If the `mcc-i386` cross target exists: after five blank lines (stripped comments), L3672 calls `mcc_mingw_resolve()` (a helper defined earlier that locates mingw gcc binaries and sets `_MCC_MINGW_*` variables). L3673 initializes `_fc_gcc_args` to empty; L3674–3675 — if a native i686 mingw gcc was found (`_MCC_MINGW_I686_GCC` exists), passes `-DGCC=<that gcc>` and `-DM32=` (empty, since the i686 compiler needs no `-m32`); L3676–3677 — else if a multilib mingw gcc exists, passes it with `-DM32=-m32`. If neither exists the driver falls back to plain `gcc -m32`.
- **L3679–3682** — Registers the `i386-fastcall-abi` test: `cmake -DIMCC=$<TARGET_FILE:mcc-i386> ${_fc_gcc_args} -P ${_fastcall_drv}`. L3681–3682 sets `FIXTURES_REQUIRED MCC_BUILT` and `SKIP_REGULAR_EXPRESSION "SKIP:"` so any of the driver's `SKIP:` messages marks the test skipped rather than passed/failed.
- **L3683–3686** — Skip branch when the `mcc-i386` cross-compiler target is not configured.

#### gcctestsuite driver script and target (L3688–L3785)

- **L3688–3689** — Sets `_gccts_drv` to `${CMAKE_BINARY_DIR}/run_gcctestsuite.cmake` and writes its first line baking in `BUILDDIR`.
- **L3690–3779** — Appends the driver body as a bracket literal:
  - **L3691–3697** — Resolves `TESTSUITE_PATH` from the `-D` argument or the `TESTSUITE_PATH` environment variable; if still empty or not a directory, prints `SKIP: gcc testsuite not found (set MCC_GCCTESTSUITE_PATH)` and returns.
  - **L3698–3703** — Requires `-DMCC=` to point at an existing mcc binary (FATAL_ERROR otherwise); defaults `BDIR` to the directory containing the mcc binary if not supplied.
  - **L3704–3709** — Creates the run directory `${BUILDDIR}/gcctestsuite`; builds the base flag list `_flags` = `-B${BDIR} -DNO_TRAMPOLINES` (pointing mcc at the build tree's runtime and disabling trampoline-requiring tests), appending `-I${IDIR}` and `-I${IDIR}/include` when an include dir was passed.
  - **L3710–3720** — Globs all `.c` files from the torture suite's `compile/`, `execute/`, and `execute/ieee/` directories, then scans each file's text and adds its basename to the `_skip` list if it uses features mcc does not support: `_builtin_` (GCC builtins), `Complex`/`complex` types, `__int128_t`/`__uint128_t`, or vector extensions (`vector`, `vector_size`, `__vector_size__`).
  - **L3721–3722** — Appends a hardcoded set of additional known-unsupported files to `_skip`: `20000120-2.c`, the four `mipscop-*.c` MIPS-coprocessor tests, four long-double/float `fp-cmp-*` variants, and `pr38016.c`.
  - **L3723–3727** — Initializes counters `_ok` (passed), `_sk` (skipped), `_fa` (compile-failed), `_xf` (executable-failed) to 0 and the `_sum` results string to empty.
  - **L3728–3747** — Compile-only pass: for each file in `compile/*.c`, runs `mcc ${_flags} -o tst.o -c <src>`. Classifies as SKIP if the diagnostic output matches "cannot use local functions" (nested-function tests), PASS on exit 0, SKIP if the basename is in `_skip`, else FAIL; increments the matching counter and appends a `<RESULT>: <path>` line to `_sum`.
  - **L3748–3773** — Execute pass: for each file in `execute/*.c` and `execute/ieee/*.c`, compiles and links with `-lm`; the same SKIP/FAIL classification applies to the compile step, and on successful compile the produced `tst` binary is run — exit 0 counts PASS, nonzero counts FAILEXE (`_xf`).
  - **L3774–3778** — Writes the full per-test summary plus the four totals to `${BUILDDIR}/mcc.sum` (mirroring DejaGnu's `.sum` convention) and prints the four totals ("N test(s) ok/skipped/failed/exe failed."). Notably the script never hard-fails: it is a reporting target, not a pass/fail ctest.
- **L3780–3785** — Registers the `gcctestsuite` custom target (not a ctest): runs the driver with `-DMCC=$<TARGET_FILE:mcc>`, `-DBDIR=${CMAKE_BINARY_DIR}`, `-DIDIR=${CMAKE_CURRENT_SOURCE_DIR}`, and `-DTESTSUITE_PATH=${MCC_GCCTESTSUITE_PATH}`. `DEPENDS mcc` rebuilds the compiler first, `USES_TERMINAL` gives live output, and the comment reads "gcctestsuite: gcc c-torture over mcc".

#### arm-asm-testsuite driver script and target (L3787–L3942)

- **L3787–3788** — Sets `_armasm_drv` to `${CMAKE_BINARY_DIR}/run_arm_asm.cmake` and writes its `BUILDDIR` header line.
- **L3789–3934** — Appends the driver body as a bracket literal. This script cross-checks mcc's ARM inline assembler against GNU binutils, mnemonic by mnemonic:
  - **L3790–3795** — Defaults `CROSS_COMPILE` to the empty string and `ARM_VFP` to ON when not passed.
  - **L3796–3805** — Locates `${CROSS_COMPILE}as` and `${CROSS_COMPILE}objdump`; prints a SKIP and returns if either is missing, or if the ARM mcc binary at `-DMCC=` does not exist.
  - **L3806–3808** — FATAL_ERROR if the `TOKH` parameter (path to `arm-tok.h`) does not exist — this file is the source of truth for the mnemonic list.
  - **L3809–3811** — Sets the work directory `_w` = `${BUILDDIR}/arm-asm-testsuite`, wiping and recreating it.
  - **L3812–3828** — Parses `arm-tok.h` line by line to build the `_mnem` mnemonic list: skips lines without `DEF_ASM`, and skips lines marked "not useful", `#define` lines, comment lines, and the `DEF_ASM_CONDED_WITH_SUFFIX(x...)` family. Lines matching `DEF_ASM_CONDED_VFP_F32_F64(<name>)` expand into both `<name>.f32` and `<name>.f64`; other `DEF_ASM*(...)` matches have `", "` replaced by `.` (joining macro arguments into a dotted mnemonic) and are appended.
  - **L3829–3835** — Filters out register/operand tokens that the token header also defines (`r0`..., `c#`, `p#`, `s#`, `d#`, `fp`, `ip`, `sp`, `lr`, `pc`, `asl`, `apsr_nzcv`, `fpsid`, `fpscr`, `fpexc`) so only real instruction mnemonics remain, replacing `_mnem` with the filtered list `_f2`.
  - **L3836–3868** — Defines `_combos`, a large fixed list of operand-string combinations to try after each mnemonic: three/four-register forms, every shift kind (`asl/lsl/asr/lsr/ror/rrx`) by immediate and by register, immediate operands (including boundary encodings like `#0xEFFF`, `#0x0201`, `#0xFFFFFF00`), register lists `{r3,r4,r5}` with and without writeback (`r2!`), all the load/store addressing modes (offset, pre-index `]!`, post-index, negative register, shifted-register index), coprocessor operand forms (`p10, #7, c2, c0, c1, #4` etc.), VFP register lists (`{d3-d4}`, `{s4-s31}`), VFP arithmetic and move forms (`s2, s3, s4`, `d2, d3`, float immediates like `#-0.1796875`), core-to-VFP transfers (`r2, r3, d1`, `s1, r2`), status-register moves (`r2, fpscr`, `apsr_nzcv, fpscr`), conversions (`s3, d4`), and the empty string (bare mnemonic).
  - **L3869–3880** — Defines the `_dump(obj outvar)` function: runs `objdump -S` on an object file and keeps only lines starting with offset `0:` (the encoded first instruction), returning them via `PARENT_SCOPE` — this is the normalized encoding used for comparison.
  - **L3881–3883** — Initializes `_total`/`_okc` counters and the `_failed` list.
  - **L3884–3918** — The main double loop over mnemonics × operand combos: mnemonics starting with `v` (VFP) get `-mfpu=vfp` added to the `as` options, or are skipped entirely if `ARM_VFP` is off (L3885–3892). For each combination, the text `"<mnem> <args>"` is first assembled with GNU `as -mlittle-endian` (L3895–3897); if GNU `as` rejects it the combination is silently skipped (it is not a valid instruction form), otherwise `_total` is incremented and the expected encoding dumped (L3898–3902). The same text is then wrapped as `__asm__("...");` in a C file and compiled with mcc `-c` (L3903–3905); an mcc failure prints "warning: '<text>' did not work in mcc" and moves on (L3906–3909). Finally the two objdump extracts are compared: match increments `_okc`, mismatch appends to `_failed` and warns "did not match GNU as" (L3910–3917).
  - **L3919** — Prints the summary "<ok> of <total> tests succeeded.".
  - **L3920–3933** — Defines `_known`, the accepted known-failure list (`bl r3`, `b r3`, `mov r2, #0xEFFF`, `mov r4, #0x0201`, `vmov.f32 r2, r3, d1`, `vmov.f32 d1, r2, r3`); iterates `_failed`, printing each failure and setting `_st 1` only for failures *not* in the known list; if any unknown failure occurred, aborts with `FATAL_ERROR "arm-asm-testsuite: FAILURES"`. L3934 closes the bracket literal.
- **L3935–3942** — Only if the `mcc-arm` cross target exists, registers the `arm-asm-testsuite` custom target: runs the driver with `-DMCC=$<TARGET_FILE:mcc-arm>`, `-DCROSS_COMPILE=${MCC_ARM_CROSS_COMPILE}` (the cache variable from L3541), and `-DTOKH=${CMAKE_CURRENT_SOURCE_DIR}/src/arch/arm/arm-tok.h`; `DEPENDS mcc-arm`, `USES_TERMINAL`, comment "arm-asm-testsuite: mcc vs GNU as over arm-tok.h". No skip branch — the target simply is not created without the arm cross compiler.

#### Orphan-source compile-only tests (L3944–L3989)

- **L3944–3948** — Globs (with `CONFIGURE_DEPENDS`, so the glob re-runs when files change) the "orphan" sources into `_orphan_srcs`: all `examples/*.c`, plus three specific test files that have no dedicated harness — `tests/behavior/bounds_stress.c`, `tests/behavior/floating_point.c`, and `tests/diagnostics/compile_errors.c`. These get compile-only smoke tests.
- **L3949–3958** — After six blank lines: L3955–3956 sets `_x11_search`, the list of candidate X11 header locations (`${MCC_SYSROOT}/usr/include`, `/usr/include`, `/opt/X11/include`, `/usr/X11R6/include`, `/usr/local/include`, Homebrew's `/opt/homebrew/include`). L3957 unsets any cached `MCC_X11_INCLUDE_DIR` so the search re-runs each configure, and L3958 does a `find_path` for `X11/Xlib.h` restricted (via `NO_DEFAULT_PATH`) to that explicit list.
- **L3959–3976** — Per-orphan-source loop: extracts the basename without extension into `_n` (L3960), reads the file text (L3961), and initializes `_orphan_x11_inc` to empty (L3962). If the source includes `X11/Xlib.h` (L3963): on a WIN32 target or when no X11 headers were found, the `compile.${_n}` test is skipped with "X11 example; <X11/Xlib.h> not available on this host/target" and the loop continues (L3964–3967); otherwise `_orphan_x11_inc` is set to `-I${MCC_X11_INCLUDE_DIR}` because, as the comment at L3968–3970 notes, X11 headers may live outside mcc's default search path (e.g. Homebrew's `/opt/homebrew/include`). L3972–3974 registers `compile.${_n}`: runs `${MCC_EMULATOR} ${_mcc_exe} -B${_bdir} -I${_idir} [X11 include] -c <file> -o ${CMAKE_BINARY_DIR}/orphan_${_n}.o` — compile only, no link or execution (`MCC_EMULATOR` wraps the compiler when it must run under emulation). L3975 requires the `MCC_BUILT` fixture.
- **L3977–3989** — WIN32-only extension: globs `runtime/win32/examples/*.c` (L3978) and registers a `compile.win32.${_n}` test for each (L3979–3986), invoking mcc with `-B` pointed at the win32 runtime directory in the *source* tree, `-I` on the win32 runtime include dir plus the normal include dir, compiling to `orphan_win_${_n}.o`, again with `FIXTURES_REQUIRED MCC_BUILT`. On non-WIN32 targets (L3987–3989), a single placeholder skip `compile.win32` records "requires WIN32 target (win32 runtime examples)".

#### PE conformance under wine (L3991–L4012)

- **L3991–3998** — After seven blank lines, `find_program(MCC_WINE ...)` looks for a wine binary under the names `wine`, `wine64`, or the specific `wine64-proton-10.0.4`.
- **L3999–4000** — Declares the cache variable `MCC_CROSS_DIR` (type PATH, default `${CMAKE_SOURCE_DIR}/cmake-build-cross`): the build directory containing the cross compilers, used by this wine test and by the Mach-O tests below.
- **L4001–4008** — If wine was found and the `mcc-x86_64-win32` cross compiler binary exists in `MCC_CROSS_DIR`, registers `pe-wine-conformance`: runs `tests/qemu/run_pe_wine.cmake` with `-DSRC` (source tree), `-DXB` (cross build dir), and `-DWORK=${CMAKE_BINARY_DIR}/pe-wine-work`. Properties (L4007–4008): `FIXTURES_REQUIRED MCC_BUILT`, label `wine`, and `SKIP_RETURN_CODE 77` so the driver can signal a runtime skip by exiting 77.
- **L4009–4012** — Skip branch: "requires wine + the win32 cross compilers (cmake --preset cross)".

#### Native PE conformance driver and test (L4014–L4104)

- **L4014–4027** — After blank separator lines, gated on `MCC_TARGETOS STREQUAL "WIN32" AND NOT CMAKE_CROSSCOMPILING` (i.e. a genuinely native Windows build): L4027 sets `_pe_native_drv` to `${CMAKE_BINARY_DIR}/run_pe_native.cmake`.
- **L4028–4091** — Writes the driver script as a bracket literal:
  - **L4035–4037** — Requires `-DMCC=`, `-DB=` (build dir), `-DSRC=` (source dir), and `-DWORK=`; FATAL_ERROR with a usage line otherwise.
  - **L4038** — Sets `CONF` to `${SRC}/tests/qemu/conformance`, the shared conformance-program directory reused by the qemu matrix.
  - **L4039–4046** — Exits with code 77 (skip) if the native mcc binary is missing or the build tree lacks `lib/libmcc1.a` (the runtime archive), each with a `SKIP:` message.
  - **L4047–4049** — Creates the work directory, initializes `status 0`, and globs all conformance `.c` programs.
  - **L4050–4063** — Per-program loop begins by extracting the basename `n`. The long comment (L4052–4059) documents a known arm64 gap: arm64 PE thread-local storage still access-violates at runtime — the codegen (`arm64-gen.c` `arm64_tls_base_x30`, the x18 TEB sequence) and linking (`arm64-link.c`: TPREL = val − tls_start) are implemented and disassemble correctly against the arm64-win32 cross tools, but the built exe faults on the arm64 runner and the root cause cannot be reproduced without an arm64 host; both tls tests pass on x86_64/i386 where PE TLS is proven. Accordingly L4060–4063 skips the `tls` and `tls_aggr` programs when `CPU` is `arm64`, printing a SKIP line and continuing.
  - **L4064–4075** — Compiles each program to `${WORK}/pe_native_<n>.exe` with `-B${B}`, `-I${B}/include`, and the win32 + generic runtime include dirs; on compile failure, extracts the first line of stderr (regex strips everything after the first newline), prints `FAIL <n> (compile): <firstline>`, sets `status 1`, and continues.
  - **L4076–4086** — Runs the produced exe with output redirected to `${WORK}/pe-native-run.out`; exit 0 prints `PASS <n>`, otherwise `FAIL <n> (run, rc=...)` and `status 1`. The exe is deleted after each run (L4085).
  - **L4087–4090** — Exits 1 if any program failed, else exits 0, via `cmake_language(EXIT ...)`.
- **L4092–4100** — Registers `pe-native-conformance`: runs the driver with `-DMCC=${_mcc_exe}`, `-DB=${_bdir}`, `-DSRC`, `-DWORK=${CMAKE_BINARY_DIR}/pe-native-work`, and `-DCPU=${MCC_CPU}` (consumed by the arm64 tls skip). Properties: `FIXTURES_REQUIRED MCC_BUILT` and `SKIP_RETURN_CODE 77`.
- **L4101–4104** — Skip branch on non-native-Windows configurations: "native PE runtime conformance (only on a native WIN32 host)".

#### Mach-O test family (L4106–L4216)

- **L4106–4112** — After blank separator lines, `find_program(MCC_OTOOL ...)` searches for a Mach-O inspection tool under the names `llvm-otool` or `otool`.
- **L4113–4120** — If otool was found and `${MCC_CROSS_DIR}/mcc-x86_64-osx` exists, registers `macho-structural`: runs `tests/qemu/validate_macho.cmake` with `-DSRC`, `-DXB=${MCC_CROSS_DIR}`, and `-DWORK=${CMAKE_BINARY_DIR}/macho-work`; properties `FIXTURES_REQUIRED MCC_BUILT`, label `macho`, `SKIP_RETURN_CODE 77`. This validates the structural correctness of emitted Mach-O files.
- **L4121–4124** — Skip branch: "requires llvm-otool/otool + the osx cross compilers (cmake --preset cross)".
- **L4126–4139** — Gated only on the x86_64-osx cross compiler existing (no otool needed), registers `macho-codegen-run`: runs `tests/qemu/run_macho_codegen.cmake` with the same `-DSRC/-DXB` plus `-DWORK=${CMAKE_BINARY_DIR}/macho-codegen-work`; same `MCC_BUILT` fixture, `macho` label, and skip code 77.
- **L4140–4143** — Skip branch: "requires the x86_64-osx cross compiler (cmake --preset cross)".
- **L4145–4157** — Same gate; registers `macho-image-run` via `tests/qemu/run_macho_image.cmake` with `-DWORK=${CMAKE_BINARY_DIR}/macho-image-work` and identical properties — exercising full Mach-O image production/execution.
- **L4158–4161** — Skip branch, same reason as above.
- **L4163–4178** — After eight blank lines, same x86_64-osx gate; registers `macho-apple-libc` via `tests/qemu/run_macho_apple_libc.cmake` with `-DWORK=${CMAKE_BINARY_DIR}/macho-apple-libc-work` and the same properties — testing against Apple libc behavior.
- **L4179–4182** — Skip branch, same reason.
- **L4184–4200** — After ten blank lines, unconditionally registers `macho-conformance-native`: runs `tests/qemu/run_macho_native.cmake` with `-DSRC`, `-DMCC=$<TARGET_FILE:mcc>` (the native compiler), `-DBDIR=${CMAKE_BINARY_DIR}`, and `-DWORK=${CMAKE_BINARY_DIR}/macho-native-work`; properties `FIXTURES_REQUIRED MCC_BUILT`, label `macho`, `SKIP_RETURN_CODE 77` (the driver self-skips on non-Darwin hosts).
- **L4202–4216** — After blank lines, L4212 declares the option `MCC_DARWIN_HOST` (default OFF) — "Tests run on a macOS/darling host with real libSystem". L4213–4216: when it is OFF, `macho-libsystem-kernel-fused` is registered as a skip with a detailed reason: libmalloc/locale-stdio/dyld/pthread/GCD/ObjC require a macOS or darling host (`-DMCC_DARWIN_HOST=ON`); the functionality is kernel-fused with no portable variant, referencing `tests/qemu/apple-libc/PROVENANCE.md`.

#### qemu-user cross conformance matrix (L4218–L4444)

- **L4218–4228** — After nine blank lines, declares the option `MCC_QEMU_TESTS` (default OFF): "Download Gentoo stage3 rootfs and run cross conformance tests under qemu-user". Everything through L4444 is inside `if(MCC_QEMU_TESTS)` (L4229).
- **L4230–4237** — Four cache variables configuring the matrix: `MCC_QEMU_MIRROR` (STRING, default `https://distfiles.gentoo.org/releases`) — base URL of the Gentoo releases mirror; `MCC_QEMU_ARCHS` (STRING, default `x86_64;i386;arm;arm64;riscv64`) — architectures to exercise; `MCC_QEMU_LIBCS` (STRING, default `glibc;musl`) — C libraries to exercise; `MCC_QEMU_DLDIR` (PATH, default `${CMAKE_BINARY_DIR}/qemu-roots`) — where downloaded rootfs trees are extracted.
- **L4239–4287** — Generates the fetch driver `run_qemu_fetch.cmake` (`_qfetch`) as a bracket literal:
  - **L4243–4245** — Returns immediately if the `MARKER` file already exists (rootfs already fetched — makes the fixture idempotent).
  - **L4246–4250** — Downloads the Gentoo "latest stage3" pointer file from `PTRURL` to `${DEST}.ptr` with a 120-second timeout; FATAL_ERROR on failure.
  - **L4253–4263** — Parses the pointer file: scans each line for a `.tar.(xz|bz2|gz)` reference and captures the first whitespace-delimited token as the relative tarball path `_rel`; FATAL_ERROR if none was found.
  - **L4264–4270** — Creates `DEST`, then downloads `${MIRRORBASE}/${_rel}` to `${DEST}.tar` with progress display and a 1800-second timeout; FATAL_ERROR on failure.
  - **L4274–4284** — Extraction: prefers a real `tar` binary with `--no-same-owner --exclude=./dev/*` (device nodes cannot be created unprivileged), falling back to `cmake -E tar xf` in the destination directory; FATAL_ERROR if extraction fails.
  - **L4285–4286** — Removes the tarball and writes the marker file containing the fetched release path, completing the idempotence contract.
- **L4289–4328** — Generates the run driver `run_qemu_run.cmake` (`_qrun`) as a bracket literal:
  - **L4294–4296** — Creates `WORKDIR`, globs the conformance sources from `SRCDIR`, initializes the `_fails` accumulator.
  - **L4299–4300** — Builds `_common`, the flags shared by every compile: `-B${MCCBASE}` (mcc's runtime from the build tree), `--sysroot=${SYSROOT}` (the extracted Gentoo rootfs), `-isystem ${SYSROOT}/usr/include`, and `-L` on the rootfs's `usr/lib64`, `lib64`, `usr/lib`, and `lib` directories (covering both 64-bit and 32-bit/musl layouts).
  - **L4301–4323** — For each conformance source and for each of two modes — `default` (no extra flags) and `pic` (`-fPIC -pie`) — compiles with mcc into `${WORKDIR}/<name>.<mode>`; a compile failure appends a detailed entry to `_fails`; on success the binary is executed as `${QEMU} -L ${SYSROOT} <out>` (qemu-user with the rootfs as the ELF-interpreter/library prefix), and a nonzero exit appends a run-failure entry.
  - **L4324–4327** — FATAL_ERROR listing all failures if `_fails` is non-empty, otherwise a status line "qemu conformance: all programs passed".
- **L4330** — Sets `_qsrc` to `${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/conformance`, the conformance-program directory.
- **L4331–4361** — `foreach(_arch IN LISTS MCC_QEMU_ARCHS)`: a dispatch chain maps each architecture to four values — `_gdir` (Gentoo mirror subdirectory), `_qbin` (qemu-user binary name), `_gptr` (glibc stage3 pointer filename), `_mptr` (musl stage3 pointer filename). Mappings: `x86_64` → amd64 / `qemu-x86_64` / `latest-stage3-amd64-openrc.txt` / musl variant; `i386` → x86 / `qemu-i386` / i686 pointers; `arm` → arm / `qemu-arm` / armv7a_hardfp pointers; `arm64` → arm64 / `qemu-aarch64` / arm64 pointers; `riscv64` → riscv / `qemu-riscv64` / rv64_lp64d pointers. An unknown arch triggers a WARNING and `continue()` (L4358–4361).
- **L4363–4371** — Compiler selection for the arch: if `_arch` equals the native `MCC_CPU`, `_qcc` is `$<TARGET_FILE:mcc>` (the native compiler); else if the `mcc-${_arch}` cross target exists, `$<TARGET_FILE:mcc-${_arch}>`; else empty. L4371 `find_program(_QEMU_${_arch} ${_qbin})` locates the arch's qemu-user binary in an arch-specific cache variable.
- **L4373–4392** — Inner `foreach(_libc IN LISTS MCC_QEMU_LIBCS)`: composes the test name `_tn` = `qemu-<arch>-<libc>`; selects `_ptr` = the glibc or musl pointer filename, skipping unknown libc values with reason "unknown libc '<libc>'" and label `qemu` (L4375–4382). Then two more skip guards: no compiler for the arch (message suggesting `-DMCC_ENABLE_CROSS=ON`, L4383–4388) and missing qemu binary (suggesting the qemu-user package, L4389–4392).
- **L4394–4401** — Sets `_root` = `${MCC_QEMU_DLDIR}/<arch>-<libc>` and registers the fetch test `${_tn}-fetch`: runs `_qfetch` with `-DPTRURL` (mirror + gdir + autobuilds + pointer file), `-DMIRRORBASE` (mirror + gdir + autobuilds), `-DDEST=${_root}`, and `-DMARKER=${_root}/.fetched`. Properties: `FIXTURES_SETUP FX_${_tn}` (making it a per-combination fixture provider), label `qemu`, and a 2000-second timeout for the large download.
- **L4403–4410** — Registers the actual conformance test `${_tn}`: runs `_qrun` with `-DMCC=${_qcc}`, `-DMCCBASE=${CMAKE_BINARY_DIR}`, `-DSYSROOT=${_root}`, `-DQEMU=${_QEMU_${_arch}}`, `-DSRCDIR=${_qsrc}`, and `-DWORKDIR=${CMAKE_BINARY_DIR}/qemu-work/<arch>-<libc>`. Properties: `FIXTURES_REQUIRED "MCC_BUILT;FX_${_tn}"` (needs both the built compiler and the fetched rootfs), label `qemu`, 600-second timeout.
- **L4411–4412** — Close the libc and arch loops.
- **L4414–4440** — After twelve blank lines: if both `arm64` is in `MCC_QEMU_ARCHS` and `glibc` is in `MCC_QEMU_LIBCS` (checked with the `;list;` MATCHES `;item;` idiom, L4426–4427), registers `qemu-arm64-osx` (L4428–4432): reuses `tests/qemu/run_macho_codegen.cmake` but with `-DARCH=arm64` and `-DSYSROOT=${MCC_QEMU_DLDIR}/arm64-glibc` — i.e. runs arm64 Mach-O codegen output under the qemu arm64 glibc rootfs. Properties (L4433–4435): fixtures `MCC_BUILT;FX_qemu-arm64-glibc`, labels `qemu;macho`, `SKIP_RETURN_CODE 77`, 600-second timeout. Otherwise (L4436–4440) it is skipped with "requires arm64+glibc in the qemu matrix and the arm64-osx cross compiler (cmake --preset cross)" and label `qemu`.
- **L4442–4443** — Configure-time status message announcing the qemu matrix is enabled (`ctest -L qemu`) with the configured arch and libc lists.
- **L4444** — `endif()` closing `if(MCC_QEMU_TESTS)`.

#### End of test block (L4446–L4448)

- **L4446–4447** — Configure-time status message summarizing what was registered: "mcc tests: registered exec (golden) + preprocess + embed/abitest/diff + hello/vla + orphan compile checks (ctest)".
- **L4448** — `endif()` closing the whole `BUILD_TESTING` test-registration block that this region has been inside.

#### qemu-docker developer target (L4450–L4486)

- **L4450–4462** — After eleven blank lines (top level of the file now), `find_program(DOCKER_EXE NAMES docker)` looks for a docker binary; everything through L4482 is gated on it being found.
- **L4463–4466** — Two cache variables: `MCC_QEMU_DOCKER_ARCHS` and `MCC_QEMU_DOCKER_LIBCS` (STRING, default empty) — ARCHS/LIBCS values to pass into the docker container's matrix, where empty means "use the image default".
- **L4467–4473** — Builds `_qd_env`, the docker `-e` environment argument list: appends `-e ARCHS=<value>` and/or `-e LIBCS=<value>` only when the corresponding cache variable is non-empty.
- **L4474–4482** — Registers the `qemu-docker` custom target with two commands: first `docker build -t mcc-qemu` on the `tests/qemu/docker` directory (building the test image), then `docker run --rm` mounting the source tree at `/work` and a named volume `mcc-qemu-roots` at `/qemu-roots` (persisting downloaded rootfs trees across runs), with the optional env overrides, running image `mcc-qemu`. `USES_TERMINAL VERBATIM` and comment "qemu-user cross matrix in Docker (image mcc-qemu)".
- **L4483–4486** — Else branch: status message that docker was not found so the `qemu-docker` target is unavailable, pointing at `tests/qemu/docker/README.md`.

#### tags, ETAGS, and dist developer targets (L4488–L4529)

- **L4488–4501** — After nine blank lines, `file(GLOB_RECURSE MCC_TAGFILES ...)` collects every `.c`, `.h`, and `.S` file in the source tree (L4497–4500), then L4501 filters out anything under build/IDE/VCS directories via one exclusion regex matching `/cmake-build-*/`, `/cmake-windows-*/`, `/cmake-mingw-*/`, `/cmake-clang/`, `/build-*/`, `/.git/`, `/.idea/`, and `/.cache/` path components.
- **L4502–4508** — `find_program(CTAGS_EXE ctags)`; if found, defines the `tags` custom target that runs ctags over `MCC_TAGFILES` with the source directory as the working directory (`VERBATIM`, and a comment echoing the full file list).
- **L4509–4515** — Symmetric `find_program(ETAGS_EXE etags)` and, if found, the `ETAGS` custom target producing an Emacs TAGS file over the same file list.
- **L4516–4529** — If Git was found (from an earlier `find_package(Git)`) and the source tree has a `.git` directory: L4517–4519 runs `git rev-parse --short=7 HEAD` in the source directory, capturing the 7-character commit hash into `_dist_hash` (errors suppressed, trailing whitespace stripped). If a hash was obtained (L4520), L4521 sets `_dist` = `moderncc-mob-<hash>` and L4522–4527 defines the `dist` custom target: `git archive --format=tar --prefix=${_dist}/ HEAD -o ${CMAKE_BINARY_DIR}/${_dist}.tar`, producing a prefixed source tarball of HEAD in the build directory (`VERBATIM`, comment "git archive -> <name>.tar"). L4528–4529 close the two conditionals.

#### CPack packaging configuration (L4531–L4550)

- **L4531–4537** — After a blank line, sets the core CPack metadata: `CPACK_PACKAGE_NAME` = "mcc", `CPACK_PACKAGE_VENDOR` = "ModernCC", `CPACK_PACKAGE_VERSION` = `${PROJECT_VERSION}`, `CPACK_PACKAGE_DESCRIPTION_SUMMARY` = `${PROJECT_DESCRIPTION}`, `CPACK_PACKAGE_HOMEPAGE_URL` = `${PROJECT_HOMEPAGE_URL}`, and `CPACK_PACKAGE_CONTACT` = "moderncc-devel".
- **L4538–4540** — If a `COPYING` file exists in the source root, sets `CPACK_RESOURCE_FILE_LICENSE` to it so package generators embed the license.
- **L4541–4545** — Selects the binary package generator by host platform: `ZIP` on Windows (`WIN32`), `TGZ` (gzipped tar) elsewhere.
- **L4546** — Sets `CPACK_SOURCE_GENERATOR` to `TGZ;ZIP`, producing source packages in both formats.
- **L4547–4549** — Sets `CPACK_SOURCE_IGNORE_FILES`, the regex list of paths excluded from source packages: `.git`, all `cmake-build-*`/`cmake-windows-*`/`cmake-mingw-*`/`cmake-clang`/`build-*` build directories, `.idea`, and any `.o`/`.a` object and archive files.
- **L4550** — `include(CPack)`, which consumes the variables above and generates the `package`/`package_source` targets. This is the last line of the file.

# Build presets and CMake modules

## `CMakePresets.json` (438 lines)

The repository's CMake presets file (schema version 3, requiring CMake >= 3.22), consumed by `cmake --preset`, `cmake --build --preset`, and `ctest --preset` — both by developers locally and by CI, whose job names mirror the preset names (`linux-gcc-*`, `macos*`, `msvc`, `mingw`, `qemu-*`, `dist-*`). It defines three sections: `configurePresets` (a hidden `_base`/`_ninja` root plus developer presets, CI matrix presets, qemu cross-test presets, and `dist-*` release-packaging presets that feed `cmake/package.cmake` via the `stage/` install prefix), `buildPresets` (one thin wrapper per configure preset), and `testPresets` (hidden label-filter bases `_test-elf`/`_test-macos`/`_test-msvc`/`_test-qemu` specialized per configure preset). The cache variables it sets (`MCC_BUILD_*`, `MCC_CONFIG_*`, `MCC_ENABLE_CROSS`, `MCC_TOOLCHAIN_PROFILE`, `MCC_TARGETS`, `MCC_QEMU_*`, `MCC_DIAGNOSTICS`) are the project options declared in the top-level `CMakeLists.txt`; the preset file is thus the canonical index of supported build configurations (see also `docs/BUILD.md`).

### File header

- **L1–3** — Opening brace; `"version": 3` selects presets schema v3 (the first with `condition`/`toolchainFile` support and the version matching CMake 3.21+), and `cmakeMinimumRequired` pins CMake 3.22.0 as the minimum for using this file.

### Configure presets — hidden bases

- **L5** — Start of the `configurePresets` array.
- **L6–10** — Hidden preset `_base`: the root of every configure preset. Its only content is `binaryDir: "${sourceDir}/cmake-build-${presetName}"`, so every preset builds in a sibling per-preset directory (e.g. `cmake-build-debug`) inside the source tree. It deliberately sets no generator, so presets inheriting only `_base` (namely `msvc` and `dist-msvc`) use CMake's platform default generator (Visual Studio on Windows).
- **L11–16** — Hidden preset `_ninja`: inherits `_base` and adds `"generator": "Ninja"`. Almost all other presets descend from this, making Ninja the standard generator everywhere except the MSVC presets.

### Configure presets — developer presets

- **L18–29** — Preset `debug`, display name "Debug (Ninja)", inherits `_ninja`. Cache variables: `CMAKE_BUILD_TYPE=Debug`; `MCC_BUILD_MUSL=OFF` (no musl-linked sibling binaries); `MCC_CONFIG_BCHECK=ON` (bounds-checking support compiled in); `MCC_CONFIG_BACKTRACE=ON` (backtrace support compiled in); `MCC_BUILD_STRIP=OFF` (keep symbols). This is the baseline that nearly all Linux/CI presets inherit and then override.
- **L30–41** — Preset `release`, display name "Release (ONE_SOURCE dynamic exe, stripped, musl, no bcheck/backtrace)", inherits `debug` and inverts its knobs: `CMAKE_BUILD_TYPE=Release`, `MCC_BUILD_MUSL=ON` (also build musl variants), `MCC_BUILD_STRIP=ON` (strip binaries), `MCC_CONFIG_BCHECK=OFF` and `MCC_CONFIG_BACKTRACE=OFF` (drop the debug-oriented runtime features). Per the display name it keeps the default single-translation-unit (ONE_SOURCE) dynamic executable shape.
- **L42–47** — Preset `asan`, display name "Sanitizer build (mcc_s)", inherits `debug` and adds only `MCC_BUILD_SANITIZE=ON`, producing the sanitizer-instrumented `mcc_s` binary.
- **L48–53** — Preset `diagnostics`, display name "Everything-on diagnostics (warnings + debug + mcc_s/mcc_p/mcc_c)", inherits `debug` and sets `MCC_DIAGNOSTICS=ON`, an umbrella switch enabling extra warnings plus the sanitizer/profiling/coverage sibling binaries named in the display string.
- **L54–59** — Preset `cross`, display name "Native + all cross compilers", inherits `debug` and sets `MCC_ENABLE_CROSS=ON` so the build also produces every `mcc-<arch>` cross compiler alongside the native one.
- **L60–68** — Preset `matrix`, display name "Toolchain x target matrix (gcc;clang x native;cross)", inherits `debug` and sets two semicolon-list variables: `MCC_TOOLCHAIN_PROFILE=gcc;clang` and `MCC_TARGETS=native;cross`, requesting a superbuild-style 2x2 matrix of host toolchains against target sets.

### Configure presets — CI Linux gcc family

- **L70–75** — Preset `linux-gcc`, display name "CI linux / gcc / Debug", inherits `debug` and pins `CMAKE_C_COMPILER=gcc`. It is the parent of the whole `linux-gcc-*` CI family.
- **L76–81** — Preset `linux-gcc-cross` ("CI linux / gcc / Debug / cross"): `linux-gcc` plus `MCC_ENABLE_CROSS=ON`.
- **L82–87** — Preset `linux-gcc-musl` ("CI linux / gcc / Debug / musl"): `linux-gcc` plus `MCC_BUILD_MUSL=ON`, adding the musl-linked binary variants.
- **L88–98** — Preset `linux-gcc-release` ("CI linux / gcc / Release"): `linux-gcc` overridden with `CMAKE_BUILD_TYPE=Release`, `MCC_BUILD_STRIP=ON`, `MCC_CONFIG_BCHECK=OFF`, `MCC_CONFIG_BACKTRACE=OFF` — the same Release flip as the `release` preset but without musl and with gcc pinned.
- **L99–104** — Preset `linux-gcc-static` ("CI linux / gcc / static exe (CONFIG_MCC_STATIC)"): `linux-gcc` plus `MCC_BUILD_STATIC_EXE=ON`, building the statically linked executable shape (the display name ties this to the `CONFIG_MCC_STATIC` project define).
- **L105–110** — Preset `linux-gcc-onesource-off` ("CI linux / gcc / multi-TU (MCC_ONE_SOURCE=OFF)"): `linux-gcc` plus `MCC_ONE_SOURCE=OFF`, exercising the multi-translation-unit build instead of the default amalgamated single-source build.
- **L111–116** — Preset `linux-gcc-asm-off` ("CI linux / gcc / integrated asm off (libmcc1 via host cc)"): `linux-gcc` plus `MCC_CONFIG_ASM=OFF`, disabling mcc's integrated assembler so the `libmcc1` runtime is compiled by the host C compiler instead.
- **L117–122** — Preset `linux-gcc-predefs-off` ("CI linux / gcc / runtime mccdefs (MCC_CONFIG_PREDEFS=OFF)"): `linux-gcc` plus `MCC_CONFIG_PREDEFS=OFF`, switching predefined-macro handling from a compiled-in table to runtime `mccdefs` generation.
- **L123–128** — Preset `linux-gcc-pie` ("CI linux / gcc / PIE + PIC codegen"): `linux-gcc` plus both `MCC_CONFIG_PIE=ON` and `MCC_CONFIG_PIC=ON`, enabling position-independent-executable and position-independent-code code generation.
- **L129–134** — Preset `linux-gcc-dwarf` ("CI linux / gcc / DWARF-5 debug info"): `linux-gcc` plus `MCC_CONFIG_DWARF=5`, selecting DWARF version 5 debug-info emission (a numeric value, not a boolean).
- **L135–140** — Preset `linux-gcc-diagnostics` ("CI linux / gcc / diagnostics (warnings + mcc_s/mcc_p/mcc_c)"): `linux-gcc` plus `MCC_DIAGNOSTICS=ON`, the CI counterpart of the developer `diagnostics` preset.

### Configure presets — CI Linux clang family and asan

- **L141–146** — Preset `linux-clang` ("CI linux / clang / Debug"): inherits `debug` (not `linux-gcc`) and pins `CMAKE_C_COMPILER=clang`.
- **L147–152** — Preset `linux-clang-cross` ("CI linux / clang / Debug / cross"): `linux-clang` plus `MCC_ENABLE_CROSS=ON`.
- **L153–163** — Preset `linux-clang-release` ("CI linux / clang / Release"): `linux-clang` with the same four Release overrides as `linux-gcc-release` (`CMAKE_BUILD_TYPE=Release`, `MCC_BUILD_STRIP=ON`, `MCC_CONFIG_BCHECK=OFF`, `MCC_CONFIG_BACKTRACE=OFF`).
- **L164–169** — Preset `linux-gcc-asan` ("CI linux / gcc / sanitizer (mcc_s, -fsanitize=address,undefined)"): `linux-gcc` plus `MCC_BUILD_SANITIZE=ON`; per the display name the sanitizer build uses address + undefined-behavior sanitizers. It is placed after the clang presets in the file, closing the Linux CI group.

### Configure presets — CI macOS

- **L171–180** — Preset `macos` ("CI macos / Debug (compiler from $env{CC}, real Darwin host)"): inherits `_ninja` directly (not `debug`, so it does not inherit the bcheck/backtrace settings). Cache variables: `CMAKE_BUILD_TYPE=Debug`; `CMAKE_C_COMPILER=$env{CC}`, taking the compiler from the `CC` environment variable at configure time (CI sets it to the runner's clang); `MCC_DARWIN_HOST=ON`, telling the build it runs on a genuine Darwin host rather than a cross environment.
- **L181–186** — Preset `macos-cross` ("CI macos / Debug / cross"): `macos` plus `MCC_ENABLE_CROSS=ON`.

### Configure presets — CI Windows

- **L188–198** — Preset `msvc` ("CI windows / MSVC / Release (default VS generator)"): inherits `_base` only, so no generator is set and CMake picks the default Visual Studio multi-config generator on Windows. Cache variables: `CMAKE_BUILD_TYPE=Release` (largely informational under a multi-config generator; the actual configuration comes from the build/test presets); `MCC_TOOLCHAIN_PROFILE=msvc` selecting the MSVC toolchain profile; and `MCC_DIFF3_GCC=$env{MCC_DIFF3_GCC}` / `MCC_DIFF3_CLANG=$env{MCC_DIFF3_CLANG}`, forwarding environment-provided paths to reference gcc/clang compilers used by three-way diff comparison tests (empty when the env vars are unset).
- **L199–207** — Preset `mingw` ("CI windows / mingw / Release (superbuild fetches winlibs GCC)"): inherits `_ninja`; sets `CMAKE_BUILD_TYPE=Release` and `MCC_TOOLCHAIN_PROFILE=mingw`. Per the display name, selecting the mingw profile triggers a superbuild step that downloads a winlibs GCC toolchain rather than requiring one preinstalled.

### Configure presets — qemu cross-test family

- **L209–220** — Preset `qemu` ("qemu-user cross matrix (all archs x glibc;musl)"): inherits `_ninja`. Cache variables: `CMAKE_BUILD_TYPE=Debug`; `MCC_ENABLE_CROSS=ON` (the cross compilers are the test subjects); `MCC_QEMU_TESTS=ON` registering qemu-user-mode tests; `MCC_QEMU_LIBCS=glibc;musl` so every architecture is tested against both C libraries; and `MCC_CROSS_DIR=${sourceDir}/cmake-build-${presetName}`, pointing the cross-toolchain root at the preset's own build directory. With no `MCC_QEMU_ARCHS` restriction, this base runs all architectures.
- **L221–226** — Preset `qemu-x86_64`: `qemu` restricted with `MCC_QEMU_ARCHS=x86_64`.
- **L227–232** — Preset `qemu-i386`: `qemu` restricted with `MCC_QEMU_ARCHS=i386`.
- **L233–238** — Preset `qemu-arm`: `qemu` restricted with `MCC_QEMU_ARCHS=arm`.
- **L239–244** — Preset `qemu-arm64`: `qemu` restricted with `MCC_QEMU_ARCHS=arm64`.
- **L245–250** — Preset `qemu-riscv64`: `qemu` restricted with `MCC_QEMU_ARCHS=riscv64`. Each per-arch preset exists so CI can run one architecture per job.

### Configure presets — distribution (`dist-*`) family

- **L252–267** — Hidden preset `_dist`: inherits `_base` (no generator) and defines the common release-packaging configuration: `CMAKE_BUILD_TYPE=Release`; `MCC_BUILD_TESTS=OFF` (no test targets in a dist build); `MCC_ONE_SOURCE=OFF` (multi-TU build, required to produce real libraries); `MCC_BUILD_STATIC_LIB=ON` and `MCC_BUILD_DYNAMIC_LIB=ON` (build both `libmcc` flavors for the embed-API bundle); `MCC_CONFIG_BCHECK=OFF` and `MCC_CONFIG_BACKTRACE=OFF` (release feature set); `MCC_ENABLE_CROSS=ON` (ship the cross compilers); and `CMAKE_INSTALL_PREFIX=${sourceDir}/stage`, so `cmake --install` populates the `stage/` directory that `cmake/package.cmake` consumes.
- **L268–273** — Hidden preset `_dist-ninja`: `_dist` plus `"generator": "Ninja"`, the base for every dist preset except MSVC.
- **L274–284** — Preset `dist-linux-gcc` ("Release dist / linux / gcc (static+dynamic lib+exe, stripped, musl, cross)"): `_dist-ninja` plus `CMAKE_C_COMPILER=gcc`, `MCC_BUILD_STATIC_EXE=ON` (add the static executable), `MCC_BUILD_STRIP=ON`, and `MCC_BUILD_MUSL=ON` (musl sibling binaries as well).
- **L285–295** — Preset `dist-linux-clang` ("Release dist / linux / clang (...)"): identical to `dist-linux-gcc` except `CMAKE_C_COMPILER=clang`.
- **L296–306** — Preset `dist-macos` ("Release dist / macos / clang (dynamic, no musl, cross)"): `_dist-ninja` with `CMAKE_C_COMPILER=clang` and the three Linux extras explicitly turned off — `MCC_BUILD_STATIC_EXE=OFF` (fully static executables are not viable on macOS), `MCC_BUILD_STRIP=OFF`, and `MCC_BUILD_MUSL=OFF` (musl is Linux-only).
- **L307–316** — Preset `dist-mingw` ("Release dist / windows / mingw (static+dynamic lib+exe, stripped, cross)"): `_dist-ninja` plus `MCC_TOOLCHAIN_PROFILE=mingw`, `MCC_BUILD_STATIC_EXE=ON`, and `MCC_BUILD_STRIP=ON` (no musl, being Windows).
- **L317–326** — Preset `dist-msvc` ("Release dist / windows / MSVC (static+dynamic lib, static exe, stripped, cross)"): inherits `_dist` directly (default Visual Studio generator, like `msvc`) plus `MCC_TOOLCHAIN_PROFILE=msvc`, `MCC_BUILD_STATIC_EXE=ON`, and `MCC_BUILD_STRIP=ON`. L327 closes the `configurePresets` array.

### Build presets

- **L329** — Start of the `buildPresets` array. Every build preset is a minimal `{name, configurePreset}` pair whose name equals its configure preset, so `cmake --build --preset <x>` mirrors `cmake --preset <x>`.
- **L330–335** — Build presets for the developer configure presets: `debug`, `release`, `asan`, `diagnostics`, `cross`, `matrix`.
- **L337–351** — Build presets for the fifteen Linux CI configure presets, in the same order as they were defined: `linux-gcc`, `linux-gcc-cross`, `linux-gcc-musl`, `linux-gcc-release`, `linux-gcc-static`, `linux-gcc-onesource-off`, `linux-gcc-asm-off`, `linux-gcc-predefs-off`, `linux-gcc-pie`, `linux-gcc-dwarf`, `linux-gcc-diagnostics`, `linux-clang`, `linux-clang-cross`, `linux-clang-release`, `linux-gcc-asan`.
- **L353–354** — Build presets `macos` and `macos-cross`.
- **L356–357** — Build presets `msvc` and `mingw`. The `msvc` entry additionally sets `"configuration": "Release"` because its Visual Studio generator is multi-config and needs the configuration chosen at build time; the Ninja-based `mingw` does not.
- **L359–364** — Build presets for the qemu family: `qemu`, `qemu-x86_64`, `qemu-i386`, `qemu-arm`, `qemu-arm64`, `qemu-riscv64`.
- **L366–371** — Build presets for the dist family: `dist-linux-gcc`, `dist-linux-clang`, `dist-macos`, `dist-mingw`, and `dist-msvc` (the latter again with `"configuration": "Release"` for the multi-config VS generator). L371 closes the array.

### Test presets

- **L373–378** — Start of `testPresets` and hidden base `_test`, which sets `output.outputOnFailure: true` so ctest prints a failing test's output — the one behavior shared by every test preset.
- **L379–384** — Hidden preset `_test-elf`: `_test` plus `filter.exclude.label = "qemu"`. Used by all native-Linux presets: qemu-labelled tests are excluded, but wine and Mach-O tests can run (Linux hosts can exercise wine and produce Mach-O output).
- **L385–390** — Hidden preset `_test-macos`: `_test` plus exclude-label regex `"qemu|wine"`; macOS runners additionally cannot run wine tests.
- **L391–396** — Hidden preset `_test-msvc`: `_test` plus exclude-label regex `"qemu|wine|macho"`; the MSVC Windows environment also skips Mach-O tests.
- **L397–402** — Hidden preset `_test-qemu`: `_test` plus `filter.include.label = "qemu"`, inverting the filter so qemu presets run only the qemu-labelled tests.
- **L404–408** — Test presets for the developer configure presets `debug`, `release`, `asan`, `diagnostics`, and `cross`, all inheriting `_test-elf`. There is no test preset for `matrix` (nor for `mingw` or the dist presets).
- **L410–424** — Test presets for all fifteen Linux CI configure presets (same names and order as the build presets), each pairing its configure preset with `_test-elf`.
- **L426–427** — Test presets `macos` and `macos-cross`, inheriting `_test-macos`.
- **L429** — Test preset `msvc`, with `"configuration": "Release"` (multi-config generator) and inheriting `_test-msvc`.
- **L431–436** — Test presets for `qemu` and the five per-arch qemu presets, each inheriting `_test-qemu` so only qemu-labelled tests execute. L437–438 close the `testPresets` array and the JSON document.

## `cmake/host_gate_check.cmake` (73 lines)

A standalone CMake script (run in script mode with `cmake -DSRCDIR=<repo>/src -P host_gate_check.cmake`) that enforces the project's "host-gate invariant": outside the designated host-abstraction pair `src/mcchost.h`/`src/mcchost.c`, no source file may test raw host-platform macros (`_WIN32`, `__APPLE__`, `_MSC_VER`, etc.) in preprocessor conditionals; everything else must use the normalized `MCC_HOST_*`, `MCC_TARGET_*`, `TARGETOS_*`, `MCC_IS_NATIVE`, or `CONFIG_*` predicates that `mcchost.h` exports. It is registered as the ctest test `host-gate-invariant`, so the invariant is checked on every test run; the authoritative spec lives in TODO.md under "Platform spec". The script exits fatally with a file:line listing on any violation.

- **L1–12** — Header comment block: names the check and its ctest registration (`host-gate-invariant`) and spec location (TODO.md "Platform spec"); states the invariant (no raw host-macro tests under `src/` outside `mcchost.{h,c}`; backends restricted to `MCC_TARGET_*`, `TARGETOS_*`, `MCC_IS_NATIVE`, `CONFIG_*`, and `MCC_HOST_*`); clarifies scope — only preprocessor conditionals (`#if`/`#ifdef`/`#ifndef`/`#elif`) count, so string literals (such as the target predefine table in `mccpp.c`) and comments mentioning a macro are allowed; and gives the invocation usage line.
- **L14–16** — Argument validation: if `SRCDIR` was not passed via `-D`, abort with `message(FATAL_ERROR)` telling the caller to pass `-DSRCDIR=<repo>/src`.
- **L18–21** — Defines the list `_host_macros`, the fifteen banned raw host macros: `_WIN32`, `_WIN64`, `_MSC_VER`, `__MINGW32__`, `__MINGW64__`, `__CYGWIN__` (Windows family); `__APPLE__`; `__linux__`; `__FreeBSD__`, `__FreeBSD_kernel__`, `__NetBSD__`, `__OpenBSD__`, `__DragonFly__` (BSD family); `__ANDROID__`; and `__dietlibc__`.
- **L23** — `file(GLOB_RECURSE)` collects every `*.c` and `*.h` file under `${SRCDIR}` into `_files`, so the whole source tree is scanned.
- **L25–30** — Initializes the empty `_violations` accumulator and begins the per-file loop; inside it, `get_filename_component(... NAME)` extracts the basename and files named exactly `mcchost.h` or `mcchost.c` are skipped with `continue()` — these two are the only files allowed to test raw host macros.
- **L32–44** — Cheap prefilter (announced by the comment on L32): the file is read whole into `_content`, `_hit` starts `OFF`, and a `string(FIND)` substring probe over every banned macro sets `_hit ON` and breaks on the first mention; files that never mention any host macro at all are skipped (L42–44) before the expensive line scan.
- **L46–48** — Prepares the line scan (comment on L46: flag host macros only inside preprocessor conditionals). Existing semicolons in the content are escaped (`;` -> `\;`) so they survive list handling, then a regex replace turns each `\r?\n` (handling both LF and CRLF) into `;`, converting the file content into the CMake list `_lines`.
- **L49–54** — Line counter `_ln` starts at 0 and is incremented with `math(EXPR)` at the top of each iteration so reported line numbers are 1-based. Lines not matching `^[ \t]*#[ \t]*(if|ifdef|ifndef|elif)` — i.e. anything that is not a preprocessor conditional directive (allowing whitespace before and after the `#`) — are skipped, which is what exempts string literals and comments.
- **L55–61** — For each conditional line, every banned macro is tested with the word-boundary regex `(^|[^A-Za-z0-9_])${_m}([^A-Za-z0-9_]|$)`; the comment on L56 explains why plain substring matching is not used: `MCC_HOST_WIN32` must not trip the `_WIN32` check. On a match, `file(RELATIVE_PATH)` computes the path relative to `SRCDIR` and a `src/<rel>:<line>: <line text>` entry is appended to `_violations`.
- **L62–63** — Close the per-line and per-file `foreach` loops.
- **L65–71** — Failure path: if `_violations` is non-empty, `list(JOIN ... "\n  ")` formats the entries one per indented line and `message(FATAL_ERROR)` aborts (making the ctest test fail) with a message naming the invariant, prescribing the fix (use `MCC_HOST_*` or a `host_*` function from `mcchost.h`), pointing at the TODO.md spec, and listing every violation.
- **L73** — Success path: `message(STATUS "host-gate invariant OK: ...")` confirms that no raw host-macro tests exist outside `src/mcchost.{h,c}`.

## `cmake/package.cmake` (176 lines)

A CMake script-mode packaging tool (invoked as `cmake -DVER=<version> -DPLAT=<platform> [-DSTAGE=<dir>] [-DOUT=<dir>] [-DFORMAT=tgz|zip] -P cmake/package.cmake`) that assembles the release bundles from a staged install tree — the `stage/` directory populated by `cmake --install` under the `dist-*` presets (whose `CMAKE_INSTALL_PREFIX` is `${sourceDir}/stage`). It is the portable single-source replacement for the former `package.sh`/`package.ps1` pair and is what release CI runs after building a dist preset. From `stage/` it lays out staging trees under `pkg/` and emits up to three archives plus a checksum file into `OUT` (default `./out`): `mcc-<ver>-<plat>` (host compiler binaries plus the mcc runtime dir), `libmcc-<ver>-<plat>` (the embed API: headers, `libmcc*` libraries, cmake package files), `mcc-cross-<ver>-<plat>` (the `mcc-<arch>` cross compilers and their runtime archives), and `checksums-<plat>.txt` in `sha256sum` format. Binary and library names follow the suffix convention documented in `docs/BUILD.md`.

### Header, arguments, and defaults (L1–47)

- **L1–17** — Header comment: states the script's purpose (assemble release bundles from `stage/`), its lineage (portable replacement for `package.sh` + `package.ps1`), the full command line with required `VER`/`PLAT` and optional `STAGE`/`OUT`/`FORMAT` arguments, the note that `VER` may carry a leading `v` (stripped) and that `FORMAT` defaults to `zip` on a Windows host and `tar.gz` elsewhere, and an inventory of the four outputs with their contents (the `mcc` bundle's host compilers listed as `mcc`, `mcc-static`, `mcc-dynamic` plus `-musl` siblings; the `libmcc` embed-API bundle; the `mcc-cross` bundle; and the sha256 checksums file), ending with a pointer to the naming convention in BUILD.md.
- **L19** — `cmake_minimum_required(VERSION 3.18)` — a lower floor than the presets file since the script uses only long-stable commands (3.18 covers `file(SHA256)`, `--format=zip` tar, etc.).
- **L21–24** — Validates that both `VER` and `PLAT` were defined via `-D`, aborting with usage otherwise; then `string(REGEX REPLACE "^v" "")` strips an optional leading `v` from `VER` into the lowercase working variable `ver`, so both `v1.2` and `1.2` produce identical archive names.
- **L26–33** — Directory defaults, with the comment noting that in script mode `CMAKE_SOURCE_DIR` is the current working directory: `STAGE` defaults to `<cwd>/stage`, `OUT` defaults to `<cwd>/out`, and `pkg` is fixed at `<cwd>/pkg` (the intermediate staging area whose subdirectories become the archives' top-level directories).
- **L35–47** — Archive-format selection: if `FORMAT` was not passed, `CMAKE_HOST_WIN32` chooses `zip` on Windows hosts and `tgz` elsewhere; then `_ext` is derived — `"zip"` for zip, `"tar.gz"` for everything else — and is used both in output filenames and to pick the tar mode in `_archive`.

### Platform shapes and preflight (L49–74)

- **L49–56** — Sets `_x`, the executable suffix: `".exe"` when `CMAKE_HOST_WIN32`, empty otherwise. The comment notes a second role of the following list: it also partitions `bin/` — the cross bundle is defined as `bin/mcc-*` minus these host shapes.
- **L56** — Defines `host_exes`, the six non-cross host compiler shapes: `mcc`, `mcc-static`, `mcc-dynamic`, `mcc-musl`, `mcc-static-musl`, `mcc-dynamic-musl`.
- **L58–63** — libdir detection: if `${STAGE}/lib64` exists (multilib GNU installs where `GNUInstallDirs` chose `lib64`), `libdir` is `lib64`; otherwise `lib`. All later library lookups go through `${STAGE}/${libdir}`.
- **L65–67** — Preflight check: if `${STAGE}/bin/mcc${_x}` does not exist there is no staged install, so the script aborts with a fatal error telling the caller to run `cmake --install` first.
- **L69–70** — Clean-slate setup: `file(REMOVE_RECURSE)` deletes any previous `pkg/` and `OUT` directories, then `file(MAKE_DIRECTORY)` recreates both empty, guaranteeing reproducible bundle contents.
- **L72–74** — `_bin_perms` lists the permissions applied to copied executables (owner rwx, group r-x, world r-x, i.e. 0755), restoring execute bits that could be lost in staging; `_names` is initialized empty and accumulates the archive filenames for the checksum step.

### `_archive` macro (L76–91)

- **L76–91** — Macro `_archive(d)`, documented by the L76 comment as archiving `${pkg}/${d}` into `${OUT}/${d}.${_ext}` with `${d}` as the archive's top-level directory. It shells out to `${CMAKE_COMMAND} -E tar` (portable, no external tar/zip dependency) with `WORKING_DIRECTORY ${pkg}` so archive paths are relative: `tar cf ... --format=zip "${d}"` for zip (L78–81) or `tar czf ... "${d}"` for gzipped tar (L82–85). The exit code is captured in `_rc`; any nonzero result is fatal with a message naming the failed bundle (L87–89). On success the archive filename is appended to `_names` (L90).

### `mcc` bundle — host compilers + runtime (L94–114)

- **L94–96** — Section comment ("mcc: host compiler binaries + the mcc runtime dir (libmcc1.a + headers)"); `d` is set to `mcc-${ver}-${PLAT}` and the bundle skeleton `${pkg}/${d}/bin` and `${pkg}/${d}/lib` is created.
- **L97–102** — Loops over the six `host_exes` shapes and, for each that exists in `${STAGE}/bin` (with the `_x` suffix), copies it into the bundle's `bin/` with `FILE_PERMISSIONS ${_bin_perms}`. The existence check makes every shape optional — a build without musl or static variants still packages cleanly.
- **L103–110** — Windows-only DLL shipping, explained by the L103–104 comment: on Windows the shared `libmcc` is installed into `bin/` and `mcc-dynamic.exe` links against it, and since Windows has no rpath the runtime DLL(s) must sit next to the executables. Under `CMAKE_HOST_WIN32`, `file(GLOB)` collects `${STAGE}/bin/libmcc*.dll` and copies each into the bundle's `bin/`.
- **L111–114** — If the runtime directory `${STAGE}/${libdir}/mcc` exists (containing `libmcc1.a` and the compiler's headers), it is copied wholesale into the bundle's `lib/`; then `_archive("${d}")` produces `mcc-<ver>-<plat>.<ext>`.

### `libmcc` bundle — embed API (L117–136)

- **L117–119** — Section comment ("libmcc: embed API — headers + libmcc archives/libs + cmake package"); `d` becomes `libmcc-${ver}-${PLAT}` and its `lib/` subdirectory is created.
- **L120–122** — If `${STAGE}/include` exists, the whole staged include tree (the public embed-API headers) is copied into the bundle root as `include/`.
- **L123–129** — `file(GLOB)` gathers every library artifact across platform conventions — `libmcc*.a` and `libmcc*.so` from `${libdir}` (Unix static/shared), `libmcc*.dylib` (macOS), `libmcc*.lib` (MSVC import/static libs), and `libmcc*.dll` from `bin/` (Windows runtime DLLs) — and copies each into the bundle's `lib/`.
- **L130–132** — If `${STAGE}/${libdir}/cmake` exists (the exported CMake package config, e.g. `find_package(mcc)` files), it is copied into the bundle's `lib/`.
- **L133–136** — The `${libdir}/mcc` runtime directory is copied into this bundle's `lib/` as well (embedders also need `libmcc1.a` and the runtime headers when driving compilation via the library); `_archive("${d}")` then produces `libmcc-<ver>-<plat>.<ext>`.

### `mcc-cross` bundle — cross compilers (L139–162)

- **L139–141** — Section comment ("mcc-cross: the mcc-&lt;arch&gt; cross compilers + their runtime archives"); `d` becomes `mcc-cross-${ver}-${PLAT}` and both `bin/` and `lib/mcc/` are created inside it.
- **L142–143** — `file(GLOB)` collects everything matching `${STAGE}/bin/mcc-*` into `_crossexes` and `_found` is initialized `FALSE`; the flag later decides whether a cross bundle is emitted at all.
- **L144–153** — For each candidate, the basename is taken, a trailing `.exe` is regex-stripped so the comparison is suffix-independent, and `list(FIND host_exes ...)` checks whether it is one of the six host shapes; if so it is skipped with `continue()` (the L149 inline comment: host shapes like `mcc-static`, `mcc-musl` are not cross compilers — this implements the partition promised at L49–50). Genuine cross compilers are copied into the bundle's `bin/` with executable permissions and `_found` flips to `TRUE`.
- **L154–157** — `file(GLOB)` collects the per-target runtime archives `${STAGE}/${libdir}/mcc/*-libmcc1.a` (e.g. `arm64-libmcc1.a`) and copies each into the bundle's `lib/mcc/`, so each cross compiler ships with its target's runtime.
- **L158–162** — Conditional emission: only if at least one cross compiler was found is `_archive("${d}")` called; otherwise a `STATUS` message reports that `stage/bin` contained no cross compilers and the cross bundle is skipped (the case for builds configured without `MCC_ENABLE_CROSS`).

### Checksums and summary (L165–176)

- **L165–171** — Checksum generation, with the section comment specifying the `sha256sum` output format `"<hash>  <filename>"` (two spaces): `_sums` starts empty, and for every archive name recorded in `_names`, `file(SHA256)` hashes the file in `OUT` and appends a formatted line; the result is written to `${OUT}/checksums-${PLAT}.txt`, verifiable directly with `sha256sum -c`.
- **L173–176** — Final summary: prints `== packaged (<ext>) ==` as a `STATUS` message followed by one indented `STATUS` line per produced archive, giving the caller (and CI logs) a manifest of what was built.

# CI infrastructure

## `.github/workflows/ci.yml` (214 lines)

The main continuous-integration workflow for the mcc compiler. It runs on every push, every pull request, and on manual dispatch, and fans out into five independent jobs: a large Docker-based Linux matrix covering compiler/linkage/feature-toggle presets, a macOS matrix (clang and Homebrew gcc), an MSVC Windows matrix (x86_64 and arm64), a native MinGW Windows build via a superbuild, a QEMU cross-architecture job, and a `dist` packaging smoke job. Every preset name maps onto an entry in the repository's `CMakePresets.json`; the Linux jobs delegate the actual configure/build/test cycle to the container image built from `tests/ci/docker/Dockerfile`, whose entrypoint is `tests/ci/docker/run-ci.sh`.

### Header: triggers, concurrency, permissions

- **L1** — Workflow display name: `CI`.
- **L3–6** — Triggers: any `push` (all branches, no filter), any `pull_request`, and manual `workflow_dispatch`, all with default (empty) configuration.
- **L8–10** — Concurrency group `ci-${{ github.ref }}` serializes runs per ref; `cancel-in-progress` is the expression `github.event_name == 'pull_request'`, so superseded PR runs are cancelled to save runner time while push/dispatch runs are allowed to finish.
- **L12–13** — Workflow-level `permissions` restrict the `GITHUB_TOKEN` to `contents: read` only (least privilege; the workflow never writes to the repo or releases).

### Job `linux` (L15–63) — Dockerized preset matrix

- **L16–18** — Job `linux`; the display name is templated as `<preset> / <arch>` and the runner comes from the matrix (`matrix.runner`), so x86_64 rows use `ubuntu-latest` and arm64 rows use ARM runners.
- **L19–22** — Matrix strategy with `fail-fast: false` so one failing preset does not cancel the others; matrix entries are given as an explicit `include` list rather than a cross-product.
- **L23** — Comment marking the first group of rows: "core linkage/compiler axes (both arches)".
- **L24–29** — Six x86_64 rows on `ubuntu-latest`: `linux-gcc`, `linux-gcc-cross`, `linux-clang`, `linux-clang-cross`, `linux-gcc-release`, and `linux-gcc-musl` — covering both compilers, the cross-compilation presets, a release-mode build, and a musl-libc build.
- **L30–33** — Four arm64 rows on `ubuntu-24.04-arm`: `linux-gcc`, `linux-gcc-cross`, `linux-clang-release`, and `linux-gcc-musl` — a reduced but representative subset of the x86_64 axes on the second architecture.
- **L34** — Comment marking the second group: "config/feature-toggle coverage (x86_64) — BUILD.md §14", i.e. these rows exist to exercise build-option toggles documented in BUILD.md section 14.
- **L35–42** — Eight feature-toggle rows, all x86_64 on `ubuntu-latest`: `linux-gcc-static` (static linking), `linux-gcc-onesource-off` (unity/one-source build disabled), `linux-gcc-asm-off` (assembly paths disabled), `linux-gcc-predefs-off` (predefined-macro machinery disabled), `linux-gcc-pie` (position-independent executable), `linux-gcc-dwarf` (DWARF debug-info option), `linux-gcc-diagnostics` (diagnostics feature build), and `linux-gcc-asan` (AddressSanitizer build).
- **L43–44** — Job-level env var `ART` names the artifact as `mcc-<preset>-<arch>`; it is reused for the output directory, tarball name, and uploaded artifact name.
- **L46** — Step: check out the repository with `actions/checkout@v5`.
- **L47–48** — Step "build CI runner image": `docker build -t mcc-ci tests/ci/docker`, building the runner image (below) fresh on every job.
- **L49–57** — Step "run <preset>": creates `ci-out/$ART` and does `chmod -R 777 ci-out` so the container's unprivileged `ci` user can write into it, then `docker run --rm` with `-e PRESET=<preset>` selecting the CMake preset, `-v "$PWD:/work:ro"` mounting the checkout read-only at `/work`, and `-v "$PWD/ci-out/$ART:/out"` mounting the writable install/export directory at `/out`, running image `mcc-ci` (whose entrypoint is `run-ci.sh`).
- **L58–59** — Step "pack build targets": `tar czf "$ART.tar.gz" -C ci-out "$ART"` compresses the exported install tree into a single tarball.
- **L60–63** — Step: `actions/upload-artifact@v7` uploads that tarball under the artifact name `${{ env.ART }}`.

### Job `macos` (L65–105) — native macOS builds

- **L65–67** — Job `macos`, named `<preset> / <cc>`, pinned to the `macos-15` (Apple Silicon) runner.
- **L68–75** — `fail-fast: false` matrix of four rows crossing the two presets `macos` and `macos-cross` with the two compilers `clang` (Apple system compiler) and `gcc` (Homebrew GCC), so both the native and cross presets are verified with each compiler family.
- **L76–77** — Job env `ART=mcc-<preset>-<cc>-arm64` (arm64 is hard-coded since macos-15 runners are Apple Silicon).
- **L79** — Checkout via `actions/checkout@v5`.
- **L80–83** — Step "install toolchain": `brew untap aws/tap 2>/dev/null || true` first removes a tap known to break `brew install` on GitHub runners (failure tolerated), then `brew install cmake ninja gcc` installs the build tools and Homebrew GCC.
- **L84–91** — Step "resolve compiler (CC for the preset)": for the gcc rows it globs `$(brew --prefix)/bin/gcc-[0-9]*`, sorts version-numerically with `sort -V`, takes the newest with `tail -1`, and appends `CC=<path>` to `$GITHUB_ENV` (Homebrew installs versioned binaries like `gcc-14`, and plain `gcc` on macOS is clang); for clang rows it simply exports `CC=clang`.
- **L92–93** — Step "configure": `cmake --preset <preset>` with `-DCMAKE_INSTALL_PREFIX=$PWD/ci-out/$ART` so the later install lands in the artifact directory.
- **L94–95** — Step "build": `cmake --build --preset <preset>` parallelized with `-j"$(sysctl -n hw.ncpu)"` (the macOS way to count CPUs).
- **L96–97** — Step "test": `ctest --preset <preset>` with the same parallelism.
- **L98–101** — Step "install + pack build targets": `cmake --install cmake-build-<preset>` (the preset's binary directory), then tars `ci-out/$ART` into `$ART.tar.gz`.
- **L102–105** — Upload the tarball with `actions/upload-artifact@v7` under name `${{ env.ART }}`.

### Job `msvc` (L107–141) — Windows MSVC builds

- **L107–109** — Job `msvc`, named `msvc / <arch>`, runner taken from the matrix.
- **L110–115** — `fail-fast: false` matrix of two rows: x86_64 on `windows-latest` and arm64 on `windows-11-arm`, covering both Windows architectures.
- **L116–117** — Job env `ART=mcc-msvc-<arch>`.
- **L119** — Checkout via `actions/checkout@v5`.
- **L120–127** — Step "resolve diff3 reference compilers (mingw gcc + clang)", run with `shell: pwsh` and `continue-on-error: true` (the reference compilers are optional): it looks up `gcc` and `clang` on PATH via `Get-Command -ErrorAction SilentlyContinue`, and for each one found appends `MCC_DIFF3_GCC=`/`MCC_DIFF3_CLANG=` (with backslashes converted to forward slashes for CMake) to `$GITHUB_ENV`. These feed the test suite's three-way "diff3" comparison against reference compilers.
- **L128–129** — Step "configure": `cmake --preset msvc`, passing `CMAKE_INSTALL_PREFIX` built from `$PWD.Path` with backslashes replaced by `/` (PowerShell string manipulation to produce a CMake-safe path) pointing at `ci-out/${env:ART}`.
- **L130–131** — Step "build": `cmake --build --preset msvc -j` (default parallelism).
- **L132–133** — Step "test": `ctest --preset msvc` (no `-j`, run serially).
- **L134–137** — Step "install + pack build targets": `cmake --install cmake-build-msvc --config Release` (the `--config` flag is required because MSVC generators are multi-config), then tar the install tree to `${env:ART}.tar.gz`.
- **L138–141** — Upload with `actions/upload-artifact@v7` as `${{ env.ART }}`.

### Job `mingw` (L143–166) — native MinGW build via superbuild

- **L143–147** — Job `mingw`, display name `mingw / native`, on `windows-latest`, with env `ART=mcc-mingw-x86_64`.
- **L149** — Checkout via `actions/checkout@v5`.
- **L150–152** — `TheMrMilchmann/setup-msvc-dev@v4` with `arch: x64` loads the MSVC developer environment into the job (needed so CMake/Ninja can find a working host toolchain alongside the fetched GCC).
- **L153–154** — Step "install ninja": `choco install ninja -y` via Chocolatey.
- **L155–158** — Step "configure + build (superbuild fetches winlibs GCC)": `cmake --preset mingw` with a forward-slash-normalized `CMAKE_INSTALL_PREFIX` into `ci-out/${env:ART}`, then `cmake --build --preset mingw -j`; per the step name, the mingw preset is a superbuild that downloads a winlibs GCC toolchain itself, so no MinGW install step is needed. This preset's build also runs whatever validation the superbuild encodes (there is no separate ctest step).
- **L159–162** — Step "install + pack build targets": `cmake --install cmake-build-mingw`, then tar `ci-out/$ART` into `${env:ART}.tar.gz`.
- **L163–166** — Upload with `actions/upload-artifact@v7` as `${{ env.ART }}`.

### Job `qemu` (L168–185) — emulated multi-arch testing

- **L168–170** — Job `qemu`, named `qemu / <arch> (glibc+musl)`, on `ubuntu-latest`; each arch is tested against both C libraries inside the container.
- **L171–174** — `fail-fast: false` matrix over the plain list `arch: [x86_64, i386, arm, arm64, riscv64]` — the target architectures mcc supports under emulation.
- **L176** — Checkout via `actions/checkout@v5`.
- **L177–178** — Step "build runner image": `docker build -t mcc-qemu tests/qemu/docker` (a different image from the mcc-ci one; it lives under `tests/qemu/docker`).
- **L179–185** — Step "run qemu-<arch> x {glibc,musl}": `docker run --rm` with `-e PRESET=qemu-<arch>` selecting the per-arch preset, the repo mounted read-write at `/work` (`-v "$PWD:/work"`, unlike the read-only mount in the linux job), and a named Docker volume `mcc-qemu-roots` mounted at `/qemu-roots` to cache the downloaded sysroots between what the container builds; no artifact is exported.

### Job `dist` (L187–214) — packaging smoke test

- **L187–189** — Job `dist`, named `<preset> / <arch>`, runner from the matrix; this job proves the release-style `dist-*` presets and the packaging script work on every push, mirroring what `release.yml` does at tag time.
- **L190–197** — `fail-fast: false` matrix of four rows: `dist-linux-gcc` and `dist-linux-clang`, each on x86_64 (`ubuntu-latest`) and arm64 (`ubuntu-24.04-arm`).
- **L199** — Checkout via `actions/checkout@v5`.
- **L200–203** — Step "install toolchain": `sudo apt-get update` then `sudo apt-get install -y --no-install-recommends cmake ninja-build gcc clang` (both compilers installed so either preset works from the same step).
- **L204–210** — Step "configure + build + install + package": a single shell block under `set -eux` (echo commands, fail on error, fail on unset vars) that runs `cmake --preset <preset>`, `cmake --build --preset <preset> -j"$(nproc)"`, `cmake --install cmake-build-<preset>`, and finally the standalone packaging script `cmake -DVER="ci-${GITHUB_SHA::12}" -DPLAT="<preset>-<arch>" -P cmake/package.cmake` — the version string is `ci-` plus the first 12 characters of the commit SHA, distinguishing CI packages from tagged releases.
- **L211–214** — `actions/upload-artifact@v7` uploads everything the package script left in `out/*` under artifact name `mcc-<preset>-<arch>`.

## `.github/workflows/release.yml` (123 lines)

The release workflow. It fires when a `v*` tag is pushed (or on manual dispatch), builds distributable packages with the `dist-*` CMake presets on Linux (gcc/clang, x86_64/arm64), macOS, MSVC (x86_64/arm64), and MinGW, and then a final `publish` job downloads all build artifacts, merges the per-platform checksum files into one `SHA256SUMS.txt`, and creates a GitHub Release with auto-generated notes. It shares `cmake/package.cmake` and the `dist-*` presets with ci.yml's `dist` job, but stamps packages with the tag name instead of a SHA.

### Header: triggers, permissions, concurrency

- **L1** — Workflow display name: `Release`.
- **L3–6** — Triggers: `push` filtered to tags matching `v*`, plus manual `workflow_dispatch: {}` (which lets the build jobs be exercised without a tag; publishing is separately gated).
- **L8–9** — `permissions: contents: write` — required so the publish step can create a GitHub Release.
- **L11–13** — Concurrency group `release-${{ github.ref }}` with `cancel-in-progress: false`, so concurrent release runs for the same ref queue rather than kill each other (a release build must never be cancelled midway).

### Job `dist-unix` (L17–55) — Linux and macOS packages

- **L17–18** — Job `dist-unix`, display-named by `matrix.plat`.
- **L19–27** — `fail-fast: false` matrix of five rows pairing an OS runner, a platform label, and a dist preset: `gcc-linux-x86_64` (ubuntu-latest, `dist-linux-gcc`), `gcc-linux-arm64` (ubuntu-24.04-arm, `dist-linux-gcc`), `clang-linux-x86_64` (ubuntu-latest, `dist-linux-clang`), `clang-linux-arm64` (ubuntu-24.04-arm, `dist-linux-clang`), and `clang-macos-arm64` (macos-15, `dist-macos`).
- **L28** — `runs-on: ${{ matrix.os }}`.
- **L30** — Checkout via `actions/checkout@v5`.
- **L31–35** — Step "install toolchain (linux)", conditioned on `runner.os == 'Linux'`: apt-get update plus `--no-install-recommends` install of cmake, ninja-build, gcc, and clang.
- **L36–40** — Step "install toolchain (macos)", conditioned on `runner.os == 'macOS'`: the same `brew untap aws/tap` workaround as ci.yml (tolerated failure), then `brew install cmake ninja` (no gcc — macOS releases are clang-built).
- **L41–49** — Step "configure + build + install" under `set -eux`: `cmake --preset <preset>`, `cmake --build --preset <preset>` parallelized with `-j"$(getconf _NPROCESSORS_ONLN)"` (POSIX-portable CPU count that works on both Linux and macOS, unlike `nproc`), `cmake --install cmake-build-<preset>`, and, on macOS only, `strip -x stage/bin/mcc stage/bin/mcc-* 2>/dev/null || true` to strip local symbols from the staged binaries (best-effort; failures ignored).
- **L50–51** — Step "package": `cmake "-DVER=${{ github.ref_name }}" "-DPLAT=<plat>" -P cmake/package.cmake` — the version is the tag name itself.
- **L52–55** — `actions/upload-artifact@v7` uploads `out/*` as `release-<plat>`.

### Job `dist-windows` (L57–78) — MSVC packages

- **L57–58** — Job `dist-windows`, display-named by `matrix.plat`.
- **L59–65** — `fail-fast: false` matrix of two rows: `msvc-windows-x86_64` on `windows-latest` and `msvc-windows-arm64` on `windows-11-arm`; `runs-on` comes from `matrix.runner`.
- **L67** — Checkout via `actions/checkout@v5`.
- **L68–72** — Step "configure + build + install": `cmake --preset dist-msvc`, `cmake --build --preset dist-msvc -j`, then `cmake --install cmake-build-dist-msvc --config Release` (`--config` needed for the multi-config MSVC generator). No explicit toolchain install — MSVC is preinstalled on the Windows runners.
- **L73–74** — Step "package": `cmake "-DVER=${{ github.ref_name }}" "-DPLAT=<plat>" -P cmake/package.cmake`.
- **L75–78** — Upload `out/*` as `release-<plat>` with `actions/upload-artifact@v7`.

### Job `dist-mingw` (L80–100) — MinGW package

- **L80–82** — Job `dist-mingw`, fixed display name `mingw-windows-x86_64`, on `windows-latest` (no matrix — one configuration only).
- **L84** — Checkout via `actions/checkout@v5`.
- **L85–87** — `TheMrMilchmann/setup-msvc-dev@v4` with `arch: x64` to provide the host developer environment, mirroring ci.yml's mingw job.
- **L88–89** — Step "install ninja" via `choco install ninja -y`.
- **L90–94** — Step "configure + build + install (superbuild fetches winlibs GCC)": `cmake --preset dist-mingw`, `cmake --build --preset dist-mingw -j`, `cmake --install cmake-build-dist-mingw` — the superbuild preset downloads its own winlibs GCC toolchain.
- **L95–96** — Step "package": `cmake "-DVER=${{ github.ref_name }}" "-DPLAT=mingw-windows-x86_64" -P cmake/package.cmake` (platform label hard-coded).
- **L97–100** — Upload `out/*` as `release-mingw-windows-x86_64`.

### Job `publish` (L102–123) — assemble and publish the GitHub Release

- **L102–106** — Job `publish` ("publish release"), which `needs: [dist-unix, dist-windows, dist-mingw]` so it runs only after every build job succeeds; guarded by `if: startsWith(github.ref, 'refs/tags/v')` so a `workflow_dispatch` run builds but never publishes; runs on `ubuntu-latest`.
- **L108–110** — `actions/download-artifact@v7` with `path: dl` downloads every artifact from the run into `dl/` (no `name` filter, so all `release-*` artifacts arrive in per-artifact subdirectories).
- **L111–117** — Step "collect + combine checksums" under `set -eux`: `mkdir -p out`; `find dl -type f ! -name 'checksums-*.txt' -exec cp {} out/ \;` flattens every package file (everything except per-platform checksum files) into `out/`; `find dl -type f -name 'checksums-*.txt' -exec cat {} + > out/SHA256SUMS.txt` concatenates all the per-platform checksum files into a single combined `SHA256SUMS.txt`; `ls -l out` logs the final contents for the run log.
- **L118–123** — Step "publish GitHub Release" using `softprops/action-gh-release@v3` with `files: out/*` (attach every file), `generate_release_notes: true` (GitHub-generated changelog), and `fail_on_unmatched_files: true` (hard failure if the glob matches nothing, guarding against publishing an empty release).

## `tests/ci/docker/Dockerfile` (73 lines)

Builds the `mcc-ci` container image that ci.yml's `linux` job uses to run every Linux preset. It is an Ubuntu 24.04 image with an up-to-date CMake from Kitware's apt repository, gcc and clang as reference compilers, Wine for running Windows-cross-built test binaries, and an unprivileged `ci` user; its entrypoint is the `run-ci.sh` staging/build/test script. Notably, the file's leading and inter-stanza lines (L1–15, L17–21, L29–33, L45–46, L52–58, L63–64) are entirely blank — there are no comments; structure is conveyed only by the grouping of the RUN stanzas.

### Base image and Kitware apt repository

- **L16** — `FROM ubuntu:24.04` — Ubuntu 24.04 "noble" as the base (the preceding L1–15 are blank lines).
- **L22–28** — First `RUN`: installs the bootstrap packages `ca-certificates` (TLS trust for the download), `gpg` (key dearmoring), and `wget` (fetching) with `DEBIAN_FRONTEND=noninteractive` and `--no-install-recommends`; then fetches Kitware's archive signing key with `wget -qO-`, dearmors it into `/usr/share/keyrings/kitware-archive-keyring.gpg`, writes a `deb [signed-by=...] https://apt.kitware.com/ubuntu/ noble main` source line to `/etc/apt/sources.list.d/kitware.list`, and removes `/var/lib/apt/lists/*` to keep the layer small. This gives the image a newer CMake than Ubuntu ships.

### Toolchain installation and sanity check

- **L34–43** — Second `RUN`: apt-installs the working toolchain, again noninteractive and without recommends: `cmake` (from the Kitware repo just configured), `ninja-build` (the generator the presets use), `build-essential` (make, libc headers, base gcc/g++ metapackage), `gcc` and `clang` (the two reference compilers the test suite compares against), `wine64` (to execute Windows binaries produced by the `*-cross` presets), `git` (version stamping / any in-build git use), and `rsync` (used by run-ci.sh to stage the mounted repo); apt lists are removed afterwards.
- **L47–50** — Third `RUN`, a build-time sanity check: `set -e`, then a `for cc in gcc clang` loop that fails the image build with "missing reference compiler $cc" if either compiler is absent from PATH, echoes "reference compilers present (gcc + clang)", and prints the first line of `cmake --version` so the resolved CMake version is visible in the build log.

### User, environment, entrypoint

- **L59–61** — Fourth `RUN`: creates the unprivileged user `ci` with a home directory (`useradd --create-home ci`), makes `/src` and `/build` directories, and `chown`s them to `ci:ci` so the entrypoint can stage sources and build without root.
- **L65–66** — `ENV WINEDEBUG=-all` (silence all Wine debug channels so cross-test output stays clean) and `WINEPREFIX=/home/ci/.wine` (put the Wine prefix in the ci user's writable home).
- **L68–69** — `COPY run-ci.sh /usr/local/bin/run-ci` installs the runner script under its command name, and `RUN chmod +x /usr/local/bin/run-ci` marks it executable.
- **L71–73** — `USER ci` drops to the unprivileged user, `WORKDIR /work` sets the default directory to the expected repo mount point, and `ENTRYPOINT ["/usr/local/bin/run-ci"]` makes the container run the CI script directly (so `docker run` arguments become extra ctest arguments via the script's `"$@"`).

## `tests/ci/docker/run-ci.sh` (69 lines)

The container-side entrypoint of the `mcc-ci` image. Given a repo mounted (usually read-only) at `/work` and a preset name in `$PRESET`, it rsync-stages the sources to a writable `/src`, normalizes CRLF line endings (so Windows-checkout sources don't break LF-expecting tests), then runs the standard `cmake --preset` / `cmake --build` / `ctest` cycle, and finally `cmake --install`s the results into `/out` when a writable directory is mounted there — which is how ci.yml's `linux` job harvests its artifacts. All scenario logic lives in `CMakePresets.json`; this script deliberately owns only staging and EOL fixup.

### Header comment and shell options (L1–16)

- **L1–15** — Shebang `#!/usr/bin/env bash` followed by the header comment block explaining the contract: the workflow selects the scenario via `PRESET` while the script owns only container-side staging and EOL fixup (with the example invocation `docker run --rm -e PRESET=linux-gcc -v "$PWD:/work:ro" mcc-ci`); and documenting the optional writable `/out` mount, noting that `CMAKE_INSTALL_PREFIX=/out` must be set at configure time because the mcc runtime-dir install destination is an absolute path baked into the install rules.
- **L16** — `set -euo pipefail`: exit on any command failure, treat unset variables as errors, and fail pipelines on any component's failure.

### Inputs and validation (L18–27)

- **L18–19** — Path constants: `SRC_MOUNT=/work` (where the workflow mounts the repo) and `SRC=/src` (the writable staging directory created in the Dockerfile).
- **L21** — `PRESET="${PRESET:?...}"` — mandatory environment variable; if unset the `:?` expansion aborts with the message "set PRESET to a CMake preset name (e.g. linux-gcc); see CMakePresets.json".
- **L22** — `JOBS="${JOBS:-$(nproc)}"` — build/test parallelism, overridable via env, defaulting to the container's CPU count.
- **L24–27** — Mount sanity check: if `$SRC_MOUNT/CMakeLists.txt` does not exist, print "error: mount the mcc repo at /work (docker run -v \"$PWD\":/work:ro ...)" to stderr and exit with status 2 (distinct from a build failure's status).

### Staging and line-ending normalization (L29–48)

- **L29–30** — Echo the `==> staging /work -> /src` progress marker and `mkdir -p "$SRC"` (defensive; the directory already exists in the image).
- **L31–38** — `rsync -a --delete` copies the mounted repo into `/src`, excluding host build directories and metadata that must not leak into the container build: `cmake-build*`, `cmake-windows-*`, `cmake-mingw-*`, `cmake-clang`, `build-*`, and `.git`. The `--delete` makes restaging exact if a container/volume is reused.
- **L40–41** — Comment explaining the next block: a Windows checkout with `autocrlf` would otherwise feed CRLF sources that break LF-expecting tests, so normalization happens on the staged copy (the read-only mount cannot be modified anyway).
- **L42–48** — If `NORMALIZE_EOL` is 1 (its default via `${NORMALIZE_EOL:-1}`, so it can be disabled by setting it to anything else), echo a progress line and run `find "$SRC" -type f` restricted to source-like extensions (`*.c`, `*.h`, `*.cmake`, `*.txt`, `*.S`, `*.def`) with `-exec sed -i 's/\r$//' {} +` to strip trailing carriage returns in place.

### Configure, build, test, export (L50–69)

- **L50–54** — `OUT=/out` and an empty `EXTRA_CONFIG=()` array; if `/out` exists and is writable (i.e. the workflow mounted an export volume), append `-DCMAKE_INSTALL_PREFIX=$OUT` to the configure arguments — set here, at configure time, per the header comment's baked-absolute-path caveat.
- **L56–58** — `cd "$SRC"`, echo `==> configuring (preset=$PRESET)`, and run `cmake --preset "$PRESET" "${EXTRA_CONFIG[@]}"`.
- **L60–61** — Echo `==> building (-j$JOBS)` and run `cmake --build --preset "$PRESET" -j"$JOBS"`.
- **L63–64** — Echo `==> testing (preset=$PRESET)` and run `ctest --preset "$PRESET" -j"$JOBS" "$@"`, forwarding any container command-line arguments as extra ctest options (possible because the script is the image ENTRYPOINT).
- **L66–69** — If `/out` exists and is writable, echo `==> exporting build targets -> /out` and run `cmake --install "cmake-build-$PRESET"`, populating the mounted artifact directory with bin/, lib*/, and include/ trees; with no writable `/out` the script ends after testing. Under `set -e`, any failing step aborts the container with a nonzero exit, which fails the workflow step.

# QEMU / cross-target test harness index

Note on line numbering: several of these files begin with (and are interspersed with) runs of genuinely blank lines where header comments were evidently stripped; per the rules those blanks are folded into the adjacent entries.

## `tests/qemu/docker/Dockerfile` (56 lines)

Builds the `mcc-qemu` Docker image: a Debian-based Linux environment that supplies the user-mode QEMU emulators (which do not exist on macOS Homebrew) plus a full native toolchain, so the `MCC_QEMU_TESTS` cross-conformance matrix can run on any host with a Linux Docker daemon. It is built either by hand (`docker build -t mcc-qemu tests/qemu/docker`) or by the `qemu-docker` custom target that the top-level `CMakeLists.txt` registers when `docker` is found; the image's entrypoint is `run-matrix.sh` (installed as `/usr/local/bin/run-matrix`), and any `docker run` arguments pass through it to `ctest`.

- **L1–10** — Blank lines (stripped header comment block).
- **L11** — `FROM debian:stable-slim`: the single base image; Debian stable-slim is chosen because it packages user-mode `qemu-user` binaries and a current toolchain in a small footprint.
- **L12–25** — Blank separator lines.
- **L26–40** — Single `RUN` layer: `apt-get update` then a non-interactive (`DEBIAN_FRONTEND=noninteractive`) `apt-get install -y --no-install-recommends` of every package the matrix needs, followed by `rm -rf /var/lib/apt/lists/*` to drop the apt cache and keep the layer small. Packages and their roles: `build-essential` (native gcc/g++/make/libc headers — builds mcc itself, the cross compilers, and serves as one of the two "diff3" reference compilers), `cmake` (configure/build/test driver), `ninja-build` (generator used by the presets), `make` (fallback/auxiliary build tool), `git` (repo metadata/version stamping during configure), `ca-certificates` (TLS trust for downloading Gentoo stage3 sysroots over HTTPS), `qemu-user` (the user-mode emulators `qemu-x86_64`, `qemu-i386`, `qemu-arm`, `qemu-aarch64`, `qemu-riscv64` that execute the cross-compiled conformance binaries), `tar`, `xz-utils`, `bzip2` (extracting the compressed stage3 rootfs tarballs), `rsync` (used by `run-matrix.sh` to stage the mounted repo into `/src`), `clang` and `lld` (the second diff3 reference compiler and the linker used for e.g. the aarch64 ELF link in `run_macho_codegen.cmake`).
- **L41–44** — Blank separator lines.
- **L45–50** — Sanity-check `RUN` layer with `set -e`: a `for` loop over the five required user-mode emulators (`qemu-x86_64 qemu-i386 qemu-arm qemu-aarch64 qemu-riscv64`) verifying each with `command -v`, printing `missing <q>` and exiting 1 if absent, then echoing "qemu-user emulators present"; a second loop verifies both `gcc` and `clang` exist (printing `missing diff3 reference <cc>` on failure) and echoes "diff3 reference compilers present (gcc + clang)". This makes a broken base-image change fail at image build time rather than at test time.
- **L52** — `VOLUME /qemu-roots`: declares the mount point for the persistent named volume that caches the large (~250 MB per cell) downloaded Gentoo sysroots across container runs.
- **L53** — `WORKDIR /work`: the directory where the caller is expected to bind-mount the mcc repository.
- **L54–55** — `COPY run-matrix.sh /usr/local/bin/run-matrix` installs the driver script into the image, and `RUN chmod +x` makes it executable.
- **L56** — `ENTRYPOINT ["/usr/local/bin/run-matrix"]`: the container always runs the matrix driver; trailing `docker run` arguments become `run-matrix` arguments and are ultimately forwarded to `ctest`.

## `tests/qemu/docker/run-matrix.sh` (73 lines)

The container-side driver installed as the image entrypoint. Its job is deliberately narrow (as its header comment says): stage the bind-mounted repo into an internal copy, wire the persistent rootfs cache volume into the CMake configure, then delegate configure/build/test to a named CMake preset (`qemu` or a per-arch `qemu-<arch>`), plus one workaround — copying x86_64 multilib crt objects from `lib64/` to `lib/`. Everything else (the actual matrix definition) lives in `CMakeLists.txt`/`CMakePresets.json`.

- **L1** — `#!/usr/bin/env bash` shebang.
- **L2–12** — Header comment: describes the script as the qemu-user cross-matrix driver that owns only container-side staging, the persistent rootfs cache wiring, and the x86_64 multilib crt fixup; gives the canonical invocation (`docker run --rm -e PRESET=qemu-arm64 -v "$PWD:/work" -v mcc-qemu-roots:/qemu-roots mcc-qemu`); and notes that `-e ARCHS=arm64 -e LIBCS=glibc` allows local subsetting without a per-arch preset.
- **L13** — `set -euo pipefail`: exit on any command failure, treat unset variables as errors, and fail a pipeline if any stage fails.
- **L15–17** — Path constants: `SRC_MOUNT=/work` (where the host repo is bind-mounted), `SRC=/src` (the internal staging copy, so Linux build artifacts never leak back into the host tree), `ROOTS=/qemu-roots` (the cache volume for downloaded sysroots).
- **L19** — `PRESET="${PRESET:-qemu}"`: the CMake preset to use, from the `PRESET` env var, defaulting to `qemu` (the full grid); CI passes per-arch presets like `qemu-x86_64`.
- **L20** — `JOBS="${JOBS:-$(nproc)}"`: build parallelism, overridable via env, defaulting to the container's CPU count.
- **L21** — `BUILD="$SRC/cmake-build-$PRESET"`: the binary directory the preset will use, computed so the pre-fetch `ctest --test-dir` call (L69) can target it directly.
- **L23–26** — Guard: if `$SRC_MOUNT/CMakeLists.txt` does not exist, the repo was not mounted; print a usage error to stderr and exit 2.
- **L28–29** — Echo the staging step and `mkdir -p` both `$SRC` and `$ROOTS`.
- **L30–37** — `rsync -a --delete` from the mount to `/src`, excluding host build trees (`cmake-build*`, `cmake-windows-*`, `cmake-mingw-*`, `cmake-clang`, `build-*`) and `.git`; `--delete` keeps repeated runs in a long-lived container consistent with the mount.
- **L39** — `cd "$SRC"`: all subsequent CMake work happens in the staged copy.
- **L41–45** — Comment plus the `overrides` bash array: the rootfs cache dir cannot live in the preset because it is a container mount, so `-DMCC_QEMU_DLDIR="$ROOTS"` is always appended; if the `ARCHS` or `LIBCS` env vars are non-empty they are forwarded as `-DMCC_QEMU_ARCHS=...` / `-DMCC_QEMU_LIBCS=...` cache overrides on top of the preset defaults.
- **L47–48** — Echo and run `cmake --preset "$PRESET" "${overrides[@]}"` to configure.
- **L50–51** — Echo and run `cmake --build --preset "$PRESET" -j"$JOBS"` to build mcc plus the cross compilers.
- **L53–64** — Comment ("glibc x86_64 multilib rootfs ships crt objects under lib64/; mcc looks in lib/") and the `fixup_multilib` function: for every directory matching `$ROOTS/x86_64-*` (i.e. each fetched x86_64 sysroot), skip unless it is a directory and unless `usr/lib64/crt1.o` exists; then copy each of `crt1.o crti.o crtn.o Scrt1.o gcrt1.o Mcrt1.o` from `usr/lib64` into `usr/lib` (with `cp -f`) so mcc's startup-object search succeeds, and echo which root was fixed.
- **L66–70** — Comment and pre-fetch step: run only the sysroot-download tests (`ctest --test-dir "$BUILD" -R 'qemu-x86_64-.*-fetch' --output-on-failure`) so the x86_64 sysroots exist before the real conformance tests; `|| true` makes this a no-op when the matrix contains no x86_64 cells; then call `fixup_multilib`.
- **L72–73** — Echo and run the full matrix: `ctest --preset "$PRESET" "$@"`, forwarding any container arguments (e.g. `-R qemu-arm64-glibc`) straight to ctest.

## `tests/qemu/docker/README.md`

Documentation for running the qemu-user cross-conformance matrix in Docker; the human-facing companion to the Dockerfile and `run-matrix.sh`. Per-section summary:

- **Intro (L1–12)** — Explains that this image runs the `MCC_QEMU_TESTS` matrix (defined in `CMakeLists.txt`) inside Linux because macOS cannot: Homebrew's `qemu` ships only system-mode emulators, while the harness needs user-mode `qemu-x86_64`/`qemu-aarch64`/`qemu-arm`/`qemu-i386`/`qemu-riscv64`, which build only on Linux. Describes the per-`(arch × libc)` flow: download a minimal Gentoo stage3 sysroot (glibc and musl), cross-compile `tests/qemu/conformance/*.c` with the matching `<arch>-mcc`, and run each self-checking program under `qemu-<arch> -L <sysroot>`, in both default codegen and `-fPIC -pie` modes.
- **Prerequisites on macOS (L14–21)** — A Linux Docker daemon is needed; shows the Colima setup (`brew install colima docker; colima start --cpu 4 --memory 6 --disk 60`).
- **Build (L23–27)** — `docker build -t mcc-qemu tests/qemu/docker`.
- **Run (L29–42)** — The canonical `docker run` from the repo root: mount the repo at `/work` and the named volume `mcc-qemu-roots` at `/qemu-roots` so the large Gentoo downloads are cached; notes the repo mount is staged into an internal `/src` copy so no Linux build artifacts pollute the macOS tree.
- **Narrow the matrix (L44–67)** — Configuration is preset-driven (`PRESET`, default `qemu` = full grid; CI uses per-arch `qemu-<arch>`); `ARCHS`/`LIBCS` env vars are overrides on top of the preset, and trailing arguments pass through to `ctest`. Four worked examples: one arch via its preset, one fast smoke-test cell (`ARCHS=x86_64 LIBCS=glibc`), musl-only rows, and passing a ctest `-R` filter.
- **Env table + closing notes (L69–77)** — Table of the four env vars (`PRESET` default `qemu`; `ARCHS` default from preset; `LIBCS` default from preset `glibc;musl`; `JOBS` default `$(nproc)`); notes the ~250 MB-per-cell first-run download reused from `/qemu-roots`, and that this is the containerized form of the CI job described in `TODO.md` §10.6.2.

## `tests/qemu/run_macho_apple_libc.cmake` (214 lines)

A `cmake -P` script mode driver that proves mcc's x86_64 Mach-O toolchain can compile and link Apple's *genuine* vendored libc sources (FreeBSD-derived string routines, libplatform, and the `_simple_*` printf family under `tests/qemu/apple-libc/`) into Mach-O images, and then execute them on a Linux x86_64 host through the custom user-space Mach-O loader (`tests/qemu/macho/loader.c`). It is registered in `CMakeLists.txt` as ctest test `macho-apple-libc` (fixture `MCC_BUILT`, label `macho`, `SKIP_RETURN_CODE 77`) with `-DSRC=<source dir> -DXB=${MCC_CROSS_DIR} -DWORK=<binary dir>/macho-apple-libc-work`, and only when `${MCC_CROSS_DIR}/mcc-x86_64-osx` exists.

### Argument validation and setup

- **L1–28** — Blank lines (stripped header comment block).
- **L29–31** — Guard: if any of `SRC`, `XB`, or `WORK` is not defined, `message(FATAL_ERROR)` with the usage string `cmake -DSRC=<src> -DXB=<cross-build> -DWORK=<work> -P run_macho_apple_libc.cmake`. `SRC` is the repo root, `XB` the cross-compiler build tree, `WORK` a scratch directory.
- **L33–35** — Derived paths: `AL = ${SRC}/tests/qemu/apple-libc` (the vendored Apple sources plus shim headers and conformance drivers), `MCC = ${XB}/mcc-x86_64-osx` (the x86_64 Mach-O cross compiler), `OSXRT = ${XB}/lib-x86_64-osx` (the prebuilt osx runtime objects).

### Environment skip checks (all exit 77, ctest's skip code)

- **L40–44** — `cmake_host_system_information(... QUERY OS_PLATFORM)` into `_hostarch`; if the host is not `x86_64`, message `SKIP: host is not x86_64` and `cmake_language(EXIT 77)` — the Mach-O image contains host-native x86_64 code that the loader maps and jumps into directly.
- **L45–48** — If `${MCC}` does not exist, skip with `SKIP: no mcc-x86_64-osx` (the `cross` preset was not built).
- **L49–53** — `find_program(GCC NAMES gcc)`; if no gcc, skip with `SKIP: no gcc for the loader` (gcc builds the Linux-side loader binary).
- **L54–57** — If `${AL}/src/strcspn.c` is absent (sentinel for the vendored Apple sources), skip with `SKIP: vendored Apple sources absent`.
- **L59** — `file(MAKE_DIRECTORY "${WORK}")` creates the scratch directory.

### Loader build

- **L62–71** — `execute_process` running `${GCC} -O2 ${SRC}/tests/qemu/macho/loader.c -o ${WORK}/machoload` with `RESULT_VARIABLE _lrc`, `OUTPUT_QUIET`, and stderr captured in `_lerr`. On a nonzero result the first stderr line is extracted via `string(REGEX REPLACE "\n.*" "" ...)` and the script skips (exit 77) with `SKIP: cannot build Mach-O loader (no seccomp?)` — the loader needs Linux seccomp headers to translate Darwin syscalls, so a failed build is an environment limitation, not an mcc bug.

### Wrapper source and inputs

- **L80–94** — `file(WRITE ${WORK}/wrap.c ...)` generates a freestanding wrapper compiled by mcc into the image: it typedefs `size_t`, declares `int cmain(void)` (the renamed test `main`), defines `osx_exit(int)` as inline asm issuing the Darwin BSD `exit` syscall (`eax = 0x2000001`, code in `edi`, `syscall`) which the loader's seccomp trap translates into the process exit code; `main` calls `osx_exit(cmain())` and spins forever as a safety net; `abort` maps to exit code 99. It also stubs `errno`, `write`, `vm_allocate`, `vm_deallocate`, and `mach_task_self` — Darwin kernel primitives referenced by the vendored `_simple_*` code but (per the comment) never reached on the `_simple_vsnprintf` path — returning failure/zero so the link resolves.
- **L96** — `CFLAGS` list: `-nostdlib` (no host libc; everything is self-contained) and `-I${AL}/shim-include` (minimal replacement headers standing in for Apple's SDK headers).
- **L98–103** — Build the `RTOBJS` list: for each of `va_list` and `builtin`, append `${OSXRT}/<o>.o` if it exists — mcc runtime support objects (varargs helpers, builtins) needed by the generated code.
- **L105** — `set(status 0)`: the aggregate pass/fail flag.

### Compiling the vendored Apple sources

- **L110–129** — Build the `OBJS` list: for each of the three vendored source directories (`${AL}/src` — FreeBSD-derived string functions, `${AL}/src-libplatform`, `${AL}/src-simple` — the `_simple_*` printf family), `file(GLOB)` the `*.c` files, `list(SORT)` them for deterministic order, and compile each with `${MCC} ${CFLAGS} -c <f> -o ${WORK}/o_<name>.o` (`OUTPUT_QUIET`, stderr in `_cerr`). On a nonzero result the first stderr line is reported as `FAIL apple-libc/<n> (compile): ...`, `status` is set to 1, and the inner loop `break()`s; on success the object is appended to `OBJS`.
- **L131–133** — If the compile stage failed (`status` nonzero), abort immediately with `cmake_language(EXIT 1)` — a real test failure, not a skip.
- **L135–144** — Compile `wrap.c` with the same flags to `${WORK}/wrap.o`; on failure report `FAIL apple-libc (wrap compile)` with the first stderr line and exit 1.

### The run_image function and its three invocations

- **L149–160** — `function(run_image src label)`, step 1: compile the conformance driver `src` with `${MCC} ${CFLAGS} -Dmain=cmain -c` to `${WORK}/test.o` (renaming its `main` so the wrapper's `main` controls exit). On failure, report `FAIL <label> (test compile)` with the first stderr line, set `status 1` in `PARENT_SCOPE`, and return.
- **L162–182** — Step 2: link with `${MCC} -nostdlib test.o ${OBJS} wrap.o ${RTOBJS} -o ${WORK}/<label>.macho`. On failure, split stderr into lines and scan for the first line that (lower-cased) matches neither `stack` nor `deprecat` — filtering executable-stack and deprecation warnings so the reported line is the actual error — report `FAIL <label> (link)`, set `status 1` in the parent scope, and return.
- **L184–191** — Step 3, structural check: `file(READ ... LIMIT 4 HEX)` reads the image's first four bytes; they must be one of the six Mach-O magics (`cffaedfe`/`cefaedfe` — 64/32-bit MH_MAGIC as stored little-endian, `feedfacf`/`feedface` — the byte-swapped forms, `cafebabe`/`bebafeca` — fat binaries). Otherwise `FAIL <label>: not a Mach-O image` and `status 1`.
- **L193–202** — Step 4, execution: run `${WORK}/machoload ${WORK}/<label>.macho` (output and stderr quiet). Result 0 yields `PASS <label> (Apple's genuine libc executed as a Mach-O image)`; anything else yields `FAIL <label> (run, rc=...)` with a note that the rc maps to the self-checking test's return code, and sets `status 1`.
- **L204–205** — `file(REMOVE)` deletes the `.macho` image before returning; `endfunction`.
- **L207–209** — The three test cases: `apple_string_conf.c` as label `apple-libc-freebsd` (FreeBSD string routines), `apple_libplatform_conf.c` as `apple-libc-libplatform`, and `apple_simple_conf.c` as `apple-libc-simple-printf`.
- **L211–214** — Final verdict: exit 1 if any case failed, otherwise `cmake_language(EXIT 0)`.

## `tests/qemu/run_macho_codegen.cmake` (208 lines)

A `cmake -P` driver that validates the *codegen* of the osx cross compilers without needing a Mach-O runtime environment: it compiles each conformance program with `mcc-<arch>-osx` into a Mach-O object file, then links that object into an ordinary ELF executable using the host toolchain plus a generated assembly shim that trampolines Darwin's underscore-prefixed libc symbols (`_printf` etc.) to the native libc, and runs the result. It is registered twice in `CMakeLists.txt`: as ctest test `macho-codegen-run` (no `ARCH`, so x86_64; fixture `MCC_BUILT`, label `macho`) with `-DSRC/-DXB/-DWORK`, and as `qemu-arm64-osx` (with `-DARCH=arm64` and `-DSYSROOT=${MCC_QEMU_DLDIR}/arm64-glibc`; fixtures `MCC_BUILT;FX_qemu-arm64-glibc`, labels `qemu;macho`, timeout 600) when the qemu matrix includes arm64+glibc — the arm64 variant links against the fetched glibc sysroot and runs under `qemu-aarch64`.

### Inputs and per-arch configuration

- **L1–13** — Blank lines then `cmake_minimum_required(VERSION 3.20)`.
- **L16–18** — Guard: `SRC`, `XB`, and `WORK` must all be defined, else `FATAL_ERROR "run_macho_codegen.cmake: SRC, XB and WORK are required"`.
- **L19–21** — `ARCH` defaults to `x86_64` when undefined or empty.
- **L22–24** — `SYSROOT` defaults to the empty string (only meaningful for arm64).
- **L25** — `CONF = ${SRC}/tests/qemu/conformance`, the shared self-checking C program suite.
- **L28–32** — `find_program(GCC NAMES gcc)`; without gcc there is no ELF harness compiler, so skip (exit 77) with `SKIP: no gcc to build the ELF harness`.
- **L35–59** — `ARCH STREQUAL "x86_64"` branch: sets `MCC = ${XB}/mcc-x86_64-osx` and `OSXRT = ${XB}/lib-x86_64-osx`; runs `uname -m` via `execute_process` (stripped) and skips if the host machine is not `x86_64` (the Mach-O object's code must run natively); skips if the cross compiler or `${OSXRT}/atomic.o` (sentinel for the runtime objects) is missing; sets `RUNTIME` to `atomic.o stdatomic.o va_list.o builtin.o`; sets `SKIP_PROGS` to `tls tls_aggr` (Mach-O TLS metadata is not ELF-linkable); and sets the shim assembly dialect `BR = "jmp"` and `PLT = "@PLT"` for x86 PLT-indirect tail jumps.
- **L60–100** — `ARCH STREQUAL "arm64"` branch: `MCC = ${XB}/mcc-arm64-osx`, `OSXRT = ${XB}/lib-arm64-osx`; skips if the compiler or `atomic.o` is missing; requires `clang` (`find_program`) and verifies via `clang -print-targets` (output in `_clang_targets`, stderr quiet) that the word `aarch64` appears with non-identifier boundaries — clang performs the cross ELF link; requires `qemu-aarch64` (`find_program`); requires `SYSROOT` to be a non-empty existing directory (the arm64 glibc sysroot fetched by the qemu matrix), else `SKIP: no arm64 glibc sysroot (${SYSROOT})`. `RUNTIME` is `atomic.o stdatomic.o builtin.o lib-arm64.o` (no `va_list.o`; arm64 has its own combined runtime object). `SKIP_PROGS` is larger — `tls tls_aggr control_libc floats_libc libc libc_struct varargs_fp` — because Darwin-flavored TLS and libc-variadic calls cannot be bridged into an ELF/glibc link on aarch64 (those programs are covered on x86_64-osx instead, per the L142 skip message). `BR = "b"` and `PLT = ""` for plain AArch64 branches.
- **L101–104** — Any other `ARCH` value skips with `SKIP: unknown arch '<ARCH>'` (exit 77).
- **L107–109** — If `${OSXRT}/complex.o` exists it is appended to `RUNTIME` (complex-arithmetic runtime support, present only when built).
- **L111** — Create the `WORK` directory.

### Generated harness and shim

- **L114–115** — `file(WRITE ${WORK}/harness.c ...)`: a two-line C harness declaring `extern int osx_main(void) __asm__("_main")` and defining `main` to return it — this bridges the Mach-O object's underscore-prefixed `_main` entry into the ELF world's `main`.
- **L118–132** — Build the shim assembly string `_shim`: a `.text` section, a `.macro tramp dar, nat` that declares `\dar` global and emits `\dar: ${BR} \nat${PLT}` (a one-instruction tail-jump trampoline from the Darwin-mangled name to the native libc symbol); then one `tramp _<name>, <name>` per entry in `_libcnames` (the 21 libc functions the conformance programs use: `memset memcpy memmove memcmp malloc calloc realloc free printf snprintf strcmp strncmp strcpy strlen abort qsort strtod strtold div ldiv lldiv`); a special-case `tramp __setjmp, _setjmp` (Darwin `setjmp` is `__setjmp`, glibc's non-signal-saving variant is `_setjmp`); and a `.note.GNU-stack` section marking the stack non-executable. The result is written to `${WORK}/shim.S`.

### Per-program loop

- **L135–136** — `file(GLOB)` all `${CONF}/*.c` into `_progs` and sort for deterministic order; **L138** initializes `_status 0`.
- **L139–144** — For each program, take the basename (`NAME_WE`); if it is in `SKIP_PROGS`, print `SKIP osx-${ARCH}/<n> (libc-variadic/TLS not ELF-linkable; covered on x86_64-osx)` and continue.
- **L147–155** — Compile: `${MCC} -I${SRC}/runtime/include -c <f> -o ${WORK}/o.o` (result in `_rc`, stderr in `_err`, stdout quiet). On failure, `string(REGEX MATCH "^[^\n]*")` extracts the first stderr line and the script prints `FAIL osx-${ARCH}/<n> (compile): ...`, sets `_status 1`, and continues to the next program.
- **L158–172** — Link the Mach-O object into an ELF executable. x86_64: `${GCC} harness.c o.o shim.S ${RUNTIME} -o ${WORK}/run` (host gcc assembles the shim and links against native glibc). arm64: `${CLANG} --target=aarch64-linux-gnu --sysroot=${SYSROOT} -fuse-ld=lld -isystem ${SYSROOT}/usr/include harness.c shim.S o.o ${RUNTIME} -L{usr/lib, lib, usr/lib64, lib64 under the sysroot} -o ${WORK}/run` — a full cross link against the fetched glibc sysroot using lld, with explicit `-L` paths covering both lib and lib64 layouts.
- **L173–187** — On link failure, split stderr into lines (`REGEX MATCHALL "[^\n]+"`), scan for the first line whose lower-cased form matches `undefined reference|undefined symbol|error:` (skipping warnings), and report `FAIL osx-${ARCH}/<n> (link): <line>`, `_status 1`, continue.
- **L190–196** — Run: natively (`${WORK}/run`) on x86_64, or under `${QEMU} -L ${SYSROOT} ${WORK}/run` on arm64 (`-L` points qemu's ELF interpreter/library lookup at the sysroot). Output and stderr are quiet; only the exit code matters (the programs are self-checking).
- **L197–203** — Exit code 0 prints `PASS osx-${ARCH}/<n> (${ARCH}-osx codegen executed)`, otherwise `FAIL osx-${ARCH}/<n> (run, rc=...)` and `_status 1`; the `run` binary is removed each iteration.
- **L206–208** — If `_status` is nonzero, `cmake_language(EXIT 1)`; otherwise the script falls off the end (implicit success).

## `tests/qemu/run_macho_image.cmake` (181 lines)

A `cmake -P` driver that tests the full Mach-O *image* pipeline on a Linux x86_64 host: compile a fixed set of conformance programs with `mcc-x86_64-osx`, link each into a complete Mach-O executable together with a generated freestanding mini-libc wrapper, verify the file type, and execute it via the user-space Mach-O loader. Compared to `run_macho_codegen.cmake` (which tests the object-level codegen through an ELF link), this exercises mcc's own Mach-O linker and the loader; compared to `run_macho_apple_libc.cmake`, the libc here is a tiny generated one rather than Apple's real sources. Registered in `CMakeLists.txt` as ctest test `macho-image-run` (fixture `MCC_BUILT`, label `macho`, `SKIP_RETURN_CODE 77`) with `-DSRC/-DXB=${MCC_CROSS_DIR}/-DWORK=<binary dir>/macho-image-work`, gated on `mcc-x86_64-osx` existing.

### Setup and skip checks

- **L1–13** — Blank lines (stripped header comment).
- **L14–16** — Guard: `SRC`, `XB`, `WORK` required, else `FATAL_ERROR "usage: -DSRC= -DXB= -DWORK= -P run_macho_image.cmake"`.
- **L18–20** — `CONF = ${SRC}/tests/qemu/conformance`, `MCC = ${XB}/mcc-x86_64-osx`, `OSXRT = ${XB}/lib-x86_64-osx`.
- **L23–27** — `cmake_host_system_information(... OS_PLATFORM)`; skip (77) with `SKIP: host is not x86_64` otherwise, since the loader executes the image's x86_64 code natively.
- **L28–31** — Skip if the cross compiler is missing (`SKIP: no mcc-x86_64-osx`).
- **L32–36** — `find_program(GCC gcc)`; skip if absent (`SKIP: no gcc for the loader`).
- **L37–40** — Skip if `${OSXRT}/atomic.o` is missing (`SKIP: no x86_64-osx runtime objects`).
- **L42** — Create `WORK`.
- **L45–54** — Build the loader: `${GCC} -O2 ${SRC}/tests/qemu/macho/loader.c -o ${WORK}/machoload`, capturing result, stdout, and stderr. On failure extract the first stderr line (handling `\r\n` via `"\r?\n.*"`) and skip (77) with `SKIP: cannot build Mach-O loader (no seccomp?)`.

### Generated freestanding wrapper libc

- **L60–104** — `file(WRITE ${WORK}/wrap.c [==[...]==])` writes the wrapper using a bracket literal (so no escaping). Contents: declaration of `int cmain(void)`; `osx_exit(int)` inline asm issuing Darwin syscall `0x2000001` (exit) so the loader's syscall translation delivers the exit code; `main` calling `osx_exit(cmain())` then spinning; `abort` exiting 99. A comment (L66–69) explains this is a tiny freestanding libc covering only functions the codegen/runtime may implicitly emit (aggregate init → `memset`, struct copy → `memmove`, `va_arg` → `memcpy`, etc.), sufficient for the codegen conformance programs, while `libc.c` — which tests the target C library itself — genuinely needs macOS libSystem and is excluded from this driver's program list. The implementations: byte-loop `memset`, `memcpy`, an overlap-safe bidirectional `memmove`, `memcmp`, `strlen`, `strcmp`, `strcpy`; a 64 KiB static-buffer bump allocator `malloc` (rounds requests to 16 bytes, returns null on exhaustion) with a no-op `free`; and a minimal `snprintf` supporting `%d %u %x %s %c %%`, built from `emit_` (bounds-checked single-char emit that still counts overflow) and `emitu_` (base-10/16 unsigned formatting) helpers on top of `__builtin_va_list`/`__builtin_va_start`/`__builtin_va_arg` — deliberately exercising mcc's Mach-O varargs codegen; it NUL-terminates within the buffer and returns the would-be length.
- **L107–109** — Compile the wrapper: `${MCC} -nostdlib -c wrap.c -o wrap.o` with both output and errors quiet and *no* result check — a compile failure would surface as a missing-object error at the link step of every program.
- **L111** — `_status 0`.

### Per-program loop (fixed list)

- **L117–127** — `foreach(t atomics control integers floats lexical aggregates varargs libc)` — eight explicitly listed programs (unlike the glob-driven drivers), chosen to be runnable against the mini-libc. Compile: `${MCC} -nostdlib -Dmain=cmain -I${SRC}/runtime/include -c ${CONF}/${t}.c -o ${WORK}/c.o`; on failure print `FAIL osx/<t> (compile)` (no error text), set `_status 1`, continue.
- **L130–151** — Link into a Mach-O image: `${MCC} -nostdlib c.o wrap.o ${OSXRT}/atomic.o ${OSXRT}/stdatomic.o ${OSXRT}/va_list.o ${OSXRT}/builtin.o -o ${WORK}/<t>.macho`. On failure, normalize stderr (strip `\r`, split on `\n`), scan for the first line not containing `stack` or `deprecat` (lower-cased) to skip benign warnings, print `FAIL osx/<t> (link): <line>`, set `_status 1`, continue.
- **L154–162** — File-type check: `file -b ${WORK}/<t>.macho` (result in `_frc`, output in `_ftype`, stderr quiet), stripped; the description must match `^Mach-O`, else `FAIL osx/<t>: not a Mach-O`, `_status 1`, continue.
- **L165–175** — Execute: `${WORK}/machoload ${WORK}/<t>.macho` (quiet). Result 0 → `PASS osx/<t> (Mach-O image loaded + executed)`; otherwise `FAIL osx/<t> (run, rc=...)` and `_status 1`. The image is removed each iteration.
- **L178–181** — Final verdict: `cmake_language(EXIT 1)` if `_status` nonzero, else explicit `cmake_language(EXIT 0)`.

## `tests/qemu/run_macho_native.cmake` (125 lines)

A `cmake -P` driver for the fully native macOS case: when the build host is Darwin and the just-built native `mcc` links real Mach-O executables against the system toolchain/libSystem, it compiles and runs the *entire* conformance suite natively — including the libc-dependent and TLS programs that every Linux-hosted Mach-O driver must skip. Registered unconditionally in `CMakeLists.txt` as ctest test `macho-conformance-native` (fixture `MCC_BUILT`, label `macho`, `SKIP_RETURN_CODE 77`) with `-DSRC=<source dir> -DMCC=$<TARGET_FILE:mcc> -DBDIR=<binary dir> -DWORK=<binary dir>/macho-native-work`; on non-Darwin hosts it self-skips at runtime.

- **L1–18** — Blank lines (stripped header comment).
- **L19–23** — Argument guard as a loop: `foreach(_req SRC MCC BDIR WORK)`, and if `${_req}` (dereferenced name) is not defined, `FATAL_ERROR "missing required -D${_req}=..."` naming the specific missing argument. `MCC` here is the native compiler binary (not a cross build dir) and `BDIR` is the build tree passed via `-B` so mcc finds its runtime library.
- **L25–26** — `CONF = ${SRC}/tests/qemu/conformance`; `INC = ${SRC}/runtime/include`.
- **L29–32** — Host check: `CMAKE_HOST_SYSTEM_NAME` must be `Darwin`, else skip (77) with `SKIP: host is not Darwin (native Mach-O needs a macOS host)`.
- **L36–39** — Skip if `${MCC}` does not exist or is a directory (`SKIP: no native mcc (${MCC})`) — guards against a stale or malformed `-DMCC` value.
- **L43–44** — Create `WORK` and write a trivial `probe.c` (`int main(void){return 0;}`).
- **L46–56** — Probe link: `${MCC} -B${BDIR} probe.c -o ${WORK}/probe` (result `_probe_rc`, stdout quiet, stderr into `_probe_err`, which is persisted to `${WORK}/probe.err` for diagnosis). If it fails, the first stderr line is extracted and the script skips (77) with `SKIP: native mcc cannot link an executable: ...` — e.g. missing Xcode command-line tools is an environment problem, not a test failure.
- **L58–67** — Probe type check: `file -b ${WORK}/probe` (output stripped) must match `^Mach-O`, else skip (77) with `SKIP: native mcc does not target Mach-O (<type>)` — e.g. an mcc configured for a different default target.
- **L72–74** — `PROGS`: an explicit sixteen-entry list — `atomics control integers floats lexical aggregates varargs complex_annexg control_libc floats_libc libc libc_struct varargs_fp vla tls tls_aggr` — the full conformance suite, feasible here because real libSystem and native TLS are available.
- **L76–82** — `set(status 0)` and the loop; a missing `${CONF}/${t}.c` is a hard `FAIL <t> (missing source)` (status 1) rather than a skip, so suite drift is caught.
- **L84–95** — Compile+link each program: `${MCC} -B${BDIR} -I${INC} ${CONF}/${t}.c -o ${WORK}/${t}` (stdout quiet, stderr saved to `${WORK}/${t}.err`). On failure, print the first stderr line as `FAIL osx/<t> (compile): ...`, set status 1, continue.
- **L97–106** — Per-binary type check: `file -b` on the output must match `^Mach-O`, else `FAIL osx/<t>: not a Mach-O image`, status 1, continue.
- **L108–119** — Execute the binary directly (`RESULT_VARIABLE _run_rc`, output/stderr quiet); rc 0 → `PASS osx/<t> (native Mach-O executed)`, else `FAIL osx/<t> (run, rc=...)` and status 1; the binary is removed each iteration.
- **L122–125** — Final verdict: exit 1 if any failure, else explicit exit 0.

## `tests/qemu/run_pe_wine.cmake` (123 lines)

A `cmake -P` driver that runs the PE/Windows cross targets under Wine: for each of `x86_64-win32` and `i386-win32`, it stages a per-target `-B` directory with the win32 runtime pieces, compiles every conformance program with the matching `mcc-<tgt>` cross compiler into a `.exe`, and executes it under the appropriate wine binary. Registered in `CMakeLists.txt` as ctest test `pe-wine-conformance` (fixture `MCC_BUILT`, label `wine`, `SKIP_RETURN_CODE 77`) with `-DSRC/-DXB=${MCC_CROSS_DIR}/-DWORK=<binary dir>/pe-wine-work`, gated on wine being found and `${MCC_CROSS_DIR}/mcc-x86_64-win32` existing (a parallel inline `run_pe_native.cmake`, generated in `CMakeLists.txt`, covers native Windows hosts).

- **L1–10** — Blank lines (stripped header comment).
- **L11–13** — Guard: `SRC`, `XB`, `WORK` required, else `FATAL_ERROR` with the usage string.
- **L15** — `CONF = ${SRC}/tests/qemu/conformance`.
- **L18–27** — Helper `function(pick out)`: iterates the remaining arguments (`${ARGN}`) as candidate program names, calling `find_program(_pick_${c} NAMES "${c}")` for each (per-candidate cache variables so results don't collide); the first hit is stored into the caller's `out` variable via `PARENT_SCOPE` and the function returns; if none are found, `out` is set to the empty string.
- **L29–30** — `pick(WINE64 wine64 wine wine64-proton-10.0.4)` and `pick(WINE32 wine wine32 wine-proton-10.0.4)`: locates 64- and 32-bit-capable wine binaries, including specific Proton-versioned names as fallbacks (plain `wine` is acceptable for both since modern wine is multi-arch).
- **L31–34** — If neither was found, skip (77) with `SKIP: no wine found`.
- **L38–39** — Environment setup for every wine invocation: `WINEDEBUG=-all` silences wine's diagnostic channels (keeping test output clean and fast) and `WINEPREFIX=${WORK}/.wineprefix` points wine at an isolated, test-owned prefix instead of the user's `~/.wine`.
- **L41–42** — `status 0` (aggregate failure flag) and `any_target 0` (tracks whether at least one target actually ran, to distinguish skip from pass).
- **L43–57** — `foreach(tgt x86_64-win32 i386-win32)`: sets `MCC = ${XB}/mcc-${tgt}` and continues with `SKIP <tgt>: no mcc-<tgt>` if that cross compiler was not built; selects `WINE = ${WINE32}` for `i386-win32` and `${WINE64}` otherwise, continuing with `SKIP <tgt>: no matching wine` if empty; on reaching this point sets `any_target 1`.
- **L60–77** — Stage the per-target compiler home `B = ${WORK}/B-${tgt}`: `file(REMOVE_RECURSE)` then create `${B}/lib`. Gather `${SRC}/runtime/win32/lib/*.def` (import-library definition files for the Windows system DLLs) into `_defs`, `${XB}/lib-${tgt}/*.o` (the prebuilt win32 runtime/CRT objects) into `_objs`, and the archive path `${XB}/${tgt}-libmcc1.a` into `_a`. Copy the defs into `${B}/lib`; copy the objects into both `${B}/lib` and `${B}` itself, and likewise the archive if it exists — the duplication covers both locations mcc's `-B` search may probe.
- **L79–95** — Glob all conformance sources and, per program `n`: compile+link with `${MCC} -B${B} -I${SRC}/runtime/win32/include -I${SRC}/runtime/include ${f} -o ${WORK}/pe_${tgt}_${n}.exe` (result `crc`, stdout `cout`, stderr `cerr`). On failure, report the first stderr line as `FAIL <tgt>/<n> (compile): ...` and set `status 1`, continuing to the next program.
- **L96–100** — Blank lines (a stripped comment, evidently about the wine invocation/output redirection that follows).
- **L101–110** — Run: `${WINE} ${exe}` with both stdout and stderr redirected to the same file `${WORK}/wine-run.out` (`OUTPUT_FILE`/`ERROR_FILE`), keeping wine chatter out of the ctest log while remaining inspectable; rc 0 → `PASS <tgt>/<n>`, else `FAIL <tgt>/<n> (run, rc=...)` and `status 1`.
- **L111–113** — Remove the `.exe` after each run; close both loops.
- **L115–118** — If `any_target` is still 0 (no win32 cross compiler existed for either target), skip (77) with `SKIP: no win32 cross-mcc for any target` — reached only when wine exists but the cross build is absent.
- **L120–123** — Final verdict: exit 1 on any failure, else explicit exit 0.

## `tests/qemu/validate_macho.cmake` (122 lines)

A `cmake -P` structural validator for mcc's Mach-O output: for each osx cross target it links the conformance programs into Mach-O executables (never running them), checks the 64-bit Mach-O magic, and asks `otool`/`llvm-otool -l` to parse the load commands; it additionally verifies that `-mmacosx-version-min` lands in `LC_BUILD_VERSION`. Registered in `CMakeLists.txt` as ctest test `macho-structural` (fixture `MCC_BUILT`, label `macho`, `SKIP_RETURN_CODE 77`) with `-DSRC/-DXB=${MCC_CROSS_DIR}/-DWORK=<binary dir>/macho-work`, gated on `llvm-otool`/`otool` and `mcc-x86_64-osx` being present at configure time.

- **L1–12** — Blank lines (stripped header comment).
- **L13–15** — Guard: `SRC`, `XB`, `WORK` required, else `FATAL_ERROR` with the usage string.
- **L17** — `CONF = ${SRC}/tests/qemu/conformance`.
- **L20–24** — `find_program(_otool NAMES llvm-otool otool PATHS /usr/lib/llvm/22/bin)`: prefers LLVM's otool, also probing a slotted Gentoo LLVM 22 install path that is not on `PATH`; without any parser, skip (77) with `SKIP: no Mach-O parser (otool/llvm-otool)`.
- **L26–28** — Create `WORK`; initialize `_status 0` and `_ran_any 0` (whether any target had a cross compiler).
- **L30–36** — `foreach(tgt x86_64-osx arm64-osx)`: `_mcc = ${XB}/mcc-${tgt}`; if absent, `SKIP <tgt>: no mcc-<tgt>` and continue to the next target; otherwise set `_ran_any 1`. Both architectures are validated structurally because parsing needs no execution.

### Per-program link and structural checks

- **L38–47** — Glob the conformance sources; for each basename `n`, skip `aggregates`, `libc`, and `varargs` up front with `SKIP <tgt>/<n> (needs macOS libSystem)` — these reference libc symbols that only a real macOS SDK link could resolve (the preceding blank lines L41–43 are a stripped comment to that effect).
- **L49–69** — Link: `${_mcc} -I${SRC}/runtime/include ${f} -o ${WORK}/macho_${tgt}_${n}` (result `_rc`, stdout `_out`, stderr `_err`). On failure, `string(FIND)` probes stderr for `unresolved reference` and `not found`; if either substring is present the failure is reclassified as `SKIP <tgt>/<n> (needs macOS libSystem)` (an unlisted program that also needs libSystem), otherwise the first stderr line (handling `\r?\n`) is reported as `FAIL <tgt>/<n> (link): ...` with `_status 1`; either way, continue.
- **L72–87** — Structural validation of a successful link: `file(READ ... LIMIT 4 HEX)` must yield exactly `cffaedfe` (the on-disk little-endian encoding of `MH_MAGIC_64`; note this validator, unlike `run_macho_apple_libc.cmake`, accepts only 64-bit thin images), else `FAIL <tgt>/<n>: bad Mach-O magic (<hex>)`. If the magic is right, run `${_otool} -l ${exe}` (all output quiet; only the exit code matters): rc 0 means the load commands parsed and prints `PASS <tgt>/<n> (valid Mach-O)`, else `FAIL <tgt>/<n>: otool could not parse load commands` with `_status 1`.
- **L88–89** — Remove the executable; end of the per-program loop.

### -mmacosx-version-min / LC_BUILD_VERSION subtest

- **L91–98** — Comment: `-mmacosx-version-min` must land in `LC_BUILD_VERSION` (the default being 10.6). Writes a trivial `int main(void){return 0;}` to `${WORK}/macho_${tgt}_versionmin.c` and links it with `${_mcc} -I${SRC}/runtime/include -mmacosx-version-min=12.3.1 ... -o ${WORK}/macho_${tgt}_versionmin`.
- **L99–111** — If that link fails, `FAIL <tgt>/versionmin (link): <full stderr>` and `_status 1`. Otherwise run `${_otool} -l` capturing the load-command dump in `_lcs`; the test passes only if otool succeeded *and* the dump matches `minos 12\.3\.1` — proving the flag was encoded into `LC_BUILD_VERSION` — printing `PASS <tgt>/versionmin (LC_BUILD_VERSION minos 12.3.1)`; otherwise `FAIL <tgt>/versionmin: minos 12.3.1 not in load commands` and `_status 1`.
- **L112–113** — Remove both the versionmin executable and its source; end of the per-target loop.
- **L115–122** — If `_ran_any` is 0 (no osx cross compiler at all), skip (77) with `SKIP: no osx cross compilers (mcc-<tgt>) for any target`; then exit 1 if `_status` is nonzero, else explicit exit 0.

# Miscellaneous test-harness and repo-config files

## `.gitattributes` (14 lines)

Repository-wide git attributes file whose sole job is line-ending policy: it pins shell scripts and Dockerfiles to LF endings regardless of the checkout platform, so that a Windows clone (with `core.autocrlf` active) still produces files that bash and Docker can execute. It is consumed directly by git itself; no build script reads it.

- **L1–8** — Eight blank lines with no content; the file's active rules only begin at line 9.
- **L9** — `*.sh    text eol=lf`: marks all `.sh` files as text and forces LF line endings in the working tree.
- **L10** — `*.bash  text eol=lf`: the same LF-normalization rule for `.bash` files.
- **L11–12** — Two blank separator lines between the shell-script group and the Docker group.
- **L13** — `Dockerfile      text eol=lf`: forces LF endings on any file named exactly `Dockerfile`.
- **L14** — `*.dockerfile    text eol=lf`: extends the same rule to files using the `.dockerfile` extension.

## `.gitignore` (12 lines)

Top-level git ignore list covering the project's out-of-source build directories, the release-packaging scratch directories produced by `cmake/package.cmake`, and JetBrains IDE metadata. All patterns are anchored to the repository root with a leading `/`, so identically named subdirectories deeper in the tree would still be tracked. Consumed by git only.

- **L1** — `/cmake-build-*/`: ignores all CMake build trees following the `cmake-build-<preset>` naming convention (e.g. `cmake-build-cross`, `cmake-build-linux-gcc-onesource-off` seen in the working copy).
- **L2** — `/cmake-windows-*/`: ignores Windows-preset build directories.
- **L3** — `/cmake-mingw-*/`: ignores MinGW cross-build directories.
- **L4** — `/cmake-clang/`: ignores the (single, unsuffixed) clang build directory.
- **L5** — `/build-*/`: catch-all for any other `build-` prefixed trees, followed by a blank line (L6).
- **L7** — Comment explaining that the next three entries are release-packaging scratch directories used by `cmake/package.cmake` as the install prefix and bundle staging areas.
- **L8–10** — `/stage/`, `/pkg/`, `/out/`: the three packaging scratch directories, followed by a blank line (L11).
- **L12** — `/.idea/`: ignores JetBrains IDE project metadata.

## `tests/asm/run_dash_s_bytes.cmake` (35 lines)

Standalone CMake script (run with `cmake -P`) implementing the byte-exact `-S` roundtrip test: it compiles a C source directly to an object file, compiles the same source to assembly and reassembles that, then requires the two objects to have byte-identical section contents. It is registered as the `dash-s-bytes-arm64` and `dash-s-bytes-riscv64` ctest cases in `CMakeLists.txt` (around L3516–3523), which pass the cross compilers `mcc-arm64`/`mcc-riscv64`, the `seccmp` section-comparison helper built from `tests/support/seccmp.c`, and `tests/asm/dash_s_roundtrip/prog.c` as the input.

- **L1–8** — Header comment: explains that `mcc -c prog.c` and `mcc -c (mcc -S prog.c)` must produce identical section contents; that this byte-exact form is only run on the fixed-width targets (arm64, riscv64) whose disassemblers guarantee byte identity, while x86 uses the behavioral `dash-s-roundtrip` test instead because the assembler legally re-encodes some immediates/branches; and documents the usage line with the four required `-D` variables (`MCC`, `SECCMP`, `SRC`, `WORK`).
- **L10–14** — Argument validation: a `foreach` over the four required variable names that raises `message(FATAL_ERROR "run_dash_s_bytes: missing -D<v>")` for any that is undefined.
- **L16** — `file(MAKE_DIRECTORY "${WORK}")` creates the per-test work directory.
- **L18–24** — Defines a `run()` helper function that wraps `execute_process`, capturing stdout/stderr and the return code, and fails the script with the full command line plus captured output if the command exits nonzero.
- **L26–28** — The three pipeline steps: compile `${SRC}` directly to `${WORK}/ref.o` (reference object), compile the same source with `-S` to `${WORK}/prog.s`, then assemble that generated assembly to `${WORK}/rt.o` (roundtrip object).
- **L30–35** — Runs `${SECCMP} ref.o rt.o` via a raw `execute_process` (not the helper, so its diagnostic output can be echoed first via `message` on L32), and fails with `"-S roundtrip is not byte-identical"` if the section comparison exits nonzero.

## `tests/diff/run_parts.cmake` (67 lines)

Standalone `cmake -P` driver for the `parts-suite` differential test: it globs every `tests/diff/parts/run_*.c` wrapper, builds each with gcc, clang, and mcc, executes all three binaries, and requires their stdout to be byte-identical across compilers. It is registered as the `parts-suite` ctest case by `CMakeLists.txt` (around L3012–3018), which supplies the compiler paths and directories via `-D` arguments. Each wrapper is a standalone unit test of one `parts/*.h` unit; the same units are also aggregated by `#include` into `tests/diff/full_language.c` for the all-in-one C11 test.

- **L1–6** — Header comment describing the three-way byte-identical-stdout contract, the relationship to `full_language.c`, and the argument list: `-DGCC=`, `-DCLANG=`, `-DMCC=`, `-DBDIR=`, `-DIDIR=`, `-DPARTS=`, `-DWORK=`.
- **L7–10** — `cmake_minimum_required(VERSION 3.16)`; creates `${WORK}`; globs `${PARTS}/run_*.c` into `_wraps` and sorts the list for deterministic test order.
- **L12–13** — Initializes the `_fail` and `_ok` counters to 0.
- **L14–16** — Opens the per-wrapper `foreach` loop, extracts the basename without extension into `_name`, and sets the shared build flags `-I${PARTS} -w -O0 -std=gnu11 -lm` (warnings suppressed, no optimization, GNU C11, math library).
- **L18–26** — gcc leg: builds `${WORK}/<name>.gcc`; on build failure emits `SEND_ERROR` with the captured output, bumps `_fail`, and `continue()`s to the next wrapper; on success runs the binary with stdout redirected to `<name>.out.gcc` via `OUTPUT_FILE`.
- **L28–36** — clang leg: identical build-run-or-fail pattern producing `<name>.clang` and `<name>.out.clang`.
- **L38–46** — mcc leg: same pattern but the mcc invocation additionally passes `-B${BDIR}` (compiler-support/driver directory) and `-I${IDIR}` (mcc's runtime include directory) before `-I${PARTS}`, producing `<name>.mcc` and `<name>.out.mcc`.
- **L48–51** — Two `cmake -E compare_files` invocations: gcc-vs-clang output into `_dc` and gcc-vs-mcc output into `_dm`.
- **L52–61** — If either comparison differs, reads the gcc and mcc output files and emits a `SEND_ERROR` showing both comparison codes and the full `--- gcc --- / --- mcc ---` outputs side by side, incrementing `_fail`; otherwise increments `_ok`. L62 closes the loop.
- **L64–67** — Prints the summary `parts-suite: <ok> unit(s) 3-way-identical, <fail> diverged` and converts any failures into a terminal `FATAL_ERROR` so ctest reports the test as failed.

## `tests/cli/asmadd.s` (5 lines)

Minimal hand-written AT&T-syntax x86-64 assembly fixture for the CLI test `assemble_dot_s_file` in `tests/cli/cases.h` (L172–174, gated `cpu=x86_64,os=linux,asm`): the harness runs `mcc -B{B} -I{I} asmadd.s asmmain.c -o ae && ae` and expects the output `42\n`. It proves mcc's driver can accept a plain `.s` file (no preprocessing) on the command line, assemble it with the integrated assembler, and link it against a C translation unit (`tests/cli/asmmain.c`, whose `main` prints `asm_add(41)`).

- **L1** — `.text`: places the code in the text section.
- **L2** — `.globl asm_add`: exports the symbol so the C caller in `asmmain.c` can link against it.
- **L3** — `asm_add:`: label defining the function entry point.
- **L4** — `leal 1(%rdi), %eax`: computes the first integer argument (System V ABI: `%edi`, addressed here through `%rdi`) plus 1 into the 32-bit return register, implementing `asm_add(x) == x + 1` in a single instruction.
- **L5** — `ret`: returns; with the caller passing 41 the program prints 42.

## `tests/preprocess/asm/gas_comments.S` (6 lines)

Preprocessor golden fixture registered as `pp_gas_comments` in `tests/exec/goldens.h` (L234) and run by `tests/exec/runner.c` in `pp` mode (`mcc -E -P file.S`); the golden output is exactly `.text\nendtext:\n`. It verifies that in assembler (`.S`) preprocessing mode, `#`-lines whose text is not a recognized directive are treated as gas line comments and stripped — critically including a comment containing an unbalanced single quote (`modelist'`), which must not derail tokenization — while genuine directives like `#ifdef` on adjacent lines are still honored. The comment text is lifted from a real Linux-kernel boot-code comment, the classic case that breaks naive assembler preprocessing.

- **L1** — `# \`modelist' label. Each video mode record looks like:` — a gas-style comment line with an unmatched apostrophe; must be discarded, not parsed as a directive or an unterminated character constant.
- **L2** — `#ifdef AAA`: a real preprocessor conditional; `AAA` is undefined, so the enclosed line is skipped.
- **L3** — The same apostrophe-bearing comment line (without the leading backtick) inside the false conditional, checking that comment/quote handling also stays sane in skipped groups.
- **L4** — `#endif` closing the conditional.
- **L5–6** — `.text` and `endtext:` — the only two lines that must survive preprocessing, matching the golden output exactly.

## `tests/preprocess/asm/ex_table_macro.S` (8 lines)

Preprocessor golden fixture registered as `pp_ex_table_macro` in `tests/exec/goldens.h` (L233), run by `tests/exec/runner.c` in `pp` mode. It reproduces the Linux-kernel `__ex_table` exception-fixup idiom to test GNU named variadic macro parameters (`y...`), multi-statement macro bodies glued with backslash continuations and `;`, gas numeric local labels with backward/forward references (`9999b`, `6001f`), and a `.section` directive with quoted flags inside a macro expansion. The golden output is the single expanded line `    9999: 1: movw (%esi), %bx; .section __ex_table, "a"; .long 9999b, 6001f ;` followed by `6001:`.

- **L1–4** — `#define SRC(y...)` with a three-line continued body: emits local label `9999:` followed by the variadic argument `y` and a `;`, then `.section __ex_table, "a";` (allocatable exception-table section), then `.long 9999b, 6001f ;` recording the faulting-instruction address (backward ref) and fixup address (forward ref). The trailing backslash on L4 continues onto the blank L5, ending the macro.
- **L5–6** — Blank lines (L5 is consumed by the trailing continuation of the macro definition).
- **L7** — `SRC(1: movw (%esi), %bx)`: invokes the macro with an argument that itself contains a label definition (`1:`), a comma inside the memory operand, and a register — all of which must pass through the variadic parameter intact.
- **L8** — `6001:` — the fixup label that resolves the macro's `6001f` forward reference.

## `tests/preprocess/asm/line_markers.S` (19 lines)

Preprocessor golden fixture registered as `pp_line_markers` in `tests/exec/goldens.h` (L244), run in `pp` mode with golden output `1\n3\n20\n22\n30\n40 "line_markers.S"\n50 "file1"\n60 "file2"\n`. It probes line-marker handling in assembler mode: a bare `# <number>` line (GNU cpp output-style marker without a filename) must be treated as a comment and not change `__LINE__`, `# line N` must behave as a real `#line` directive, comment text that merely starts with digits (`# 64mb`) must not be mistaken for a marker, and `#line` arguments must undergo macro expansion for both the line number and the filename.

- **L1** — `__LINE__` — expands to `1` (first golden line), establishing the baseline.
- **L2–3** — `# 10` followed by `__LINE__`: the bare numeric hash-line is ignored as a gas comment, so L3 expands to `3` (not 11), which is what the golden asserts.
- **L4–5** — `# line 20` then `__LINE__`: the explicit `line` keyword makes it a real directive, so the next line reports `20`.
- **L6–7** — `# 64mb` then `__LINE__`: a comment beginning with digits but with trailing letters; it is ignored and `__LINE__` continues from the previous directive, yielding `22`.
- **L8–9** — `# line 30` then `__LINE__`, giving `30` — a second confirmation after the non-marker comment.
- **L10–12** — `#define LINE1 40`, `# line LINE1`, then `__LINE__ __FILE__`: the line-number argument is macro-expanded, producing `40 "line_markers.S"` (the filename is unchanged).
- **L13–15** — `#define LINE2 50`, `# line LINE2 "file1"`, then `__LINE__ __FILE__`: mixes a macro line number with a literal filename, producing `50 "file1"`.
- **L16–19** — `#define LINE3 60` and `#define FILE "file2"`, then `# line LINE3 FILE` and `__LINE__ __FILE__`: both arguments are macros, producing `60 "file2"`.

## `tests/asm/gas_directives.S` (1025 lines)

Large GNU-assembler coverage fixture for mcc's integrated assembler, exercising both gas directives and essentially the whole classic x86 AT&T instruction repertoire (i386 and x86_64 variants selected with `#ifdef __i386__` / `#ifdef __x86_64__`). The embedded line marker `# 28 "asmtest.S"` on L27 shows it is adapted from TinyCC's `tests/asmtest.S`. It is not currently wired into any passing test: `CMakeLists.txt` L3536–3537 registers `asm-gas-directives` as a skipped test with the reason "blocked: integrated assembler lacks sgdtq/sidtq/swapgs encodings (gas_directives.S:811)", so the file is a dormant target the assembler must eventually assemble in full.

### Data and alignment directives (L1–25)
- **L1–4** — Leading blank lines.
- **L5–12** — Data-emission directives: `.byte 0xff`, comma-separated lists via `.byte 1, 2, 3`, `.short`, `.word`, `.long`, and `.int` (each with `1, 2, 3`), then `.align 8` followed by a single `.byte 1` to verify padding.
- **L14–18** — `.balign 4, 0x92` (byte-alignment with explicit fill), `.align 16, 0x91` (alignment with fill byte), `.skip 3` (default fill), `.skip 15, 0x90` (explicit fill), and `.string "hello\0world"` with an embedded NUL escape.
- **L21–25** — Defines the classic two-level `__stringify(n)`/`stringify(n)` C-macro pair and uses `.asciz stringify(BLA)` (stringizing an undefined identifier into `"BLA"`), bracketed by `.skip 8,0x90` padding on each side so the string is easy to locate in the output.

### Line marker, labels, and symbol-difference expressions (L27–38)
- **L27–28** — The comment-annotated line marker `# 28 "asmtest.S"` (simultaneously a line directive and a trailing `#` line comment) followed by a plain `movl %eax, %ebx`.
- **L30–38** — Labels `L1:`, `L2:`, `var1:`; `mov 0x10000, %eax` absolute load; `movl $L2 - L1, %ecx` using a label-difference immediate; `nop ; nop ; nop ; nop` testing multiple `;`-separated statements on one line; and `mov var1, %eax` loading from a symbolic address.

### Basic mov forms and 64-bit immediates (L41–69)
- **L41–55** — `mov`/`movl` register-to-register, absolute-address loads and stores in 32/16/8-bit widths (`%eax`/`%ax`/`%al`), immediates to `%edx`/`%dx`/`%cl`, and size-suffixed `movb`/`movw`/`movl` in both directions against the full scaled-index operand `0x100(%ebx,%edx,2)`.
- **L57–67** — Immediate-width edges: `$0x1122` to `%si`, `$0x112233`, `$0x80000000`, `$-0x7fffffff`; then an x86_64 block testing `mov`/`movq` of a 32-bit immediate (`$0x11223344`) and a 40-bit immediate (`$0x1122334455`) into `%rbx`, plus `movl $0x11334455,(%rbx)`.
- **L69** — `mov %eax, 0x12(,%edx,2)`: index-only (no base) scaled addressing.

### Control/debug/segment registers and REX GPRs (L71–104)
- **L71–88** — Conditional block: on i386, moves involving `%cr3`, `%tr3` (test register), `%db3`, and `%dr6`; on x86_64, the `%cr3`/`%db3`/`%dr6` equivalents with 64-bit GPRs plus `%cr8` in both directions. Unconditionally, `movl %fs, %ecx` and `movl %ebx, %fs` segment-register moves.
- **L90–104** — x86_64-only: `movq` between every extended-register pair `r8–r15`, and single-operand `inc %r9b`, `dec %r10w`, `not %r11d`, `negq %r12`, `decb %r13b`, `incw %r14w`, `notl %r15d`, sweeping byte/word/dword/qword sub-registers of the REX set.

### Sign/zero extension (L106–132)
- **L106–117** — `movsbl`/`movsbw`/`movswl` sign-extending loads, `movzbl`/`movzbw`/`movzwl` zero-extending loads, the suffixless `movzb` form with both `%eax` and `%ax` destinations, and a plain `mov $0x12345678,%eax`.
- **L119–132** — x86_64: `movzb` to `%rax`, `movzbq`, `movsbq`, `movzwq`, `movswq`, `movslq %eax, %rcx`, then `mov` of the 32-bit immediate `$0x12345678` into `%rax`/`%rdx`/`%r10` (movabs not required) and the full 64-bit immediate `$0x123456789abcdef0` into `%rax`/`%rcx`/`%r11`.

### Push/pop and xchg (L134–164)
- **L134–158** — Arch-split push forms (`pushl`/`push %eax`/`push %cs` on i386; `pushq`/`push %rax` on x86_64), then common `pushw %ax`, `push %gs`, immediate pushes `$1`/`$100`, memory `push 0x42(%eax)` and `pop 0x43(%esi)`; the matching pop split (`popl`/`pop %eax`/`pop %ds` vs `popq`/`pop %rax`), plus `popw %ax` and `pop %fs`.
- **L160–164** — `xchg` register-register both orders, register-memory (`%bx` with `0x10000`), and memory-register in dword and byte widths.

### Port I/O and lea/segment loads (L166–194)
- **L166–183** — `in` from immediate port `$100` into `%al`/`%ax`/`%eax`, `in` via `%dx` in all three widths, one-operand `inb/inw/inl %dx`; `out` to immediate port in three widths and `outb/outw/outl` via `%dx`.
- **L185–194** — `leal`/`lea 0x1000(%ebx), %ecx`; i386-only far-pointer loads `les`/`lds`/`lss`, and unconditional `lfs`/`lgs`.

### ALU immediates and addressing-mode sweep (L196–250)
- **L196–223** — `addl`/`add` with immediates probing the imm8 sign-extension boundary from both sides: `$0x123`, `$-16`, `$-0x123`, `$1`/`$-1` in 16- and 32-bit, `$127`, `$-128` (twice), `$-129`, `$128`, `$255`, `$256`; byte ops `andb $0xf, %ah`, `andb $-15, %cl`, `xorb $127, %dh`, `cmpb $42, (%eax)`; and `addl $0x123` against absolute, base+disp, base+index*scale, `(%esp)`, the constant-expression displacement `(3*8)(%esp)`, `(%ebp)`, and bare `(%esp)`, plus `cmpl $0x123, (%esp)`.
- **L225–241** — x86_64 REX/byte-register interactions: `xor` mixing `%ah`, `%r8b`, `%r9b`, `%sil`, `%cl`; `add` of 32-bit registers into memory addressed through 32-bit (`%r8d`, `%r10d`, `%r11d`) and 64-bit (`%r9`, `%r12`–`%r15`) bases/indexes with and without scale, including `0x1000(%rbx,%r12,8)` and the address-size-override form `0x1000(%ebp,%r9d,8)`; `movb` immediates into `%ah`, `%bpl`, `%dil`, `%r12b`.
- **L243–250** — `add` and `or` register↔memory in both directions at dword (`%eax`), word (`%dx`/`%si`), and byte (`%cl`/`%dl`) widths.

### inc/dec, test, not/neg, mul/div, shifts (L252–307)
- **L252–255** — `inc %edx`, sized memory forms `incl`/`incb 0x10000`, `dec %dx`.
- **L257–265** — `test` with immediate against `%al`/`%cl`, size-suffixed `testl`/`testb`/`testw $1, 0x1000`, register-register, register-memory, and memory-register forms.
- **L267–275** — `not` and `neg` on a register and in explicit `w`/`l`/`b` memory widths.
- **L277–292** — Multiply/divide: one-operand `imul %ecx`, `mul %edx`, `mulb %cl`; two-operand `imul %eax, %ecx` and `imul 0x1000, %cx`; three-operand immediate forms `imul $10, %eax, %ecx` and `imul $10, %ax, %cx`; two-operand-immediate shorthand `imul $10/$0x1100000/$1, %eax`; `idivw 0x1000`, `div %ecx`, `div %bl`, and the explicit two-operand `div %ecx, %eax`.
- **L294–307** — `and $15,%bx` and `and $-20,%edx`; `shl` in implicit-1, immediate, and `%cl` forms; `shld` and `shrd` double-precision shifts each in `$1`, `%cl`, and implicit-`%cl` two-operand forms.

### Calls, jumps, returns, and conditional branches (L309–390)
- **L309–330** — `call` to absolute `0x1000`, to label `L4`, indirect through a register (`*%eax` / `*%rax` per arch), indirect through memory `*0x1000`, and to the undefined external `func1`; `.global L5,L6` declaring two symbols in one directive with the labels defined at L322–323; far calls: i386 `lcall $0x100, $0x1000` (immediate segment:offset) versus x86_64 memory-indirect `lcall *0x100` and `lcall *(%rax)`.
- **L332–348** — `jmp` absolute, memory-indirect `*(%edi)`, register-indirect per arch, `*0x1000`; far jumps: i386 `ljmp $0x100, $0x1000` versus the 64-bit branch's indirect `ljmp *0x100`, `ljmp *(%rdi)`, and explicitly sized `ljmpl`/`ljmpw *(%esi)`.
- **L350–364** — `ret` and `ret $10` (stack-pop return), arch-suffixed `retl`/`retq` with and without pop counts, `lret` and `lret $10` far returns, and `enter $1234, $10` frame setup.
- **L366–382** — Label `L3:`; conditional jumps `jo`/`jnp`/`jne`/`jg` first to absolute addresses `0x1000`–`0x1003` and then backward to `L3`; the loop family `loopne`/`loopnz`/`loope`/`loopz`/`loop` and `jecxz`, all targeting `L3`.
- **L385–390** — `setcc` byte-setters: `seto`/`setc %al`, the suffixed alias `setcb %al`, memory destinations `setnp 0x1000` and `setl 0xaaaa`, and `setg %dl`.

### x87 floating point (L392–542)
- **L392–407** — `fadd` family: no-operand, `%st(1), %st`, reversed `%st(0), %st(1)`, single-operand `%st(3)`; `fmul %st(0),%st(0)` and `%st(0),%st(1)`; `faddp` with `%st(5)`, no operand, and `%st(1), %st`; memory forms `fadds` (32-bit real), `fiadds` (16-bit int), `faddl` (64-bit real), `fiaddl` (32-bit int).
- **L409–459** — The same operand-form matrix repeated for `fmul`/`fmulp`/`fmuls`/`fimuls`/`fmull`/`fimull` (L409–420), `fsub`/`fsubp` plus its memory forms (L422–433), the reversed `fsubr`/`fsubrp` family (L435–446), and `fdiv`/`fdivp` with `fdivs`/`fidivs`/`fdivl`/`fidivl` (L448–459); the p-forms of sub/subr/div are exercised with `%st(5)` and bare.
- **L461–475** — Compare family: `fcom %st(3)` and memory `fcoms`/`ficoms`/`fcoml`/`ficoml`; `fcomp %st(5)`, bare `fcomp`, `fcompp`, and the four `fcomps`/`ficomps`/`fcompl`/`ficompl` memory forms.
- **L477–491** — Load/store: `fld %st(5)`, `fldl`/`flds`/`fildl` memory loads, `fst %st(4)`, `fstp %st(6)`, `fstpt 0x1006` (80-bit) and `fbstp 0x1008` (packed BCD); `fxch` bare and with `%st(4)`; `fucom %st(6)`, `fucomp %st(3)`, `fucompp`.
- **L493–511** — Control/state instructions in wait and no-wait pairs: `finit`/`fninit`, `fldcw`, `fnstcw`/`fstcw`, `fnstsw`/`fstsw` each to an absolute address and to `(%eax)`, `fnclex`/`fclex`, `fnstenv`/`fstenv`/`fldenv`, `fnsave`/`fsave`/`frstor`, and `ffree %st(7)`/`ffreep %st(6)`.
- **L513–542** — No-operand instructions: `ftst`, `fxam`; the constant loaders `fld1`, `fldl2t`, `fldl2e`, `fldpi`, `fldlg2`, `fldln2`, `fldz`; the transcendental/utility set `f2xm1`, `fyl2x`, `fptan`, `fpatan`, `fxtract`, `fprem1`, `fdecstp`, `fincstp`, `fprem`, `fyl2xp1`, `fsqrt`, `fsincos`, `frndint`, `fscale`, `fsin`, `fcos`; and `fchs`, `fabs`, `fnop`, `fwait`.

### Atomics, conditional moves, interrupts (L544–590)
- **L544–559** — `bswap %edx`/`bswapl %ecx`; `xadd` register form plus `xaddb`/`xaddw`/`xaddl` to memory; `cmpxchg` register form plus `cmpxchgb`/`cmpxchgw`/`cmpxchgl` to memory; `invlpg 0x1000`; `cmpxchg8b 0x1002`; x86_64 `cmpxchg16b` with `(%rax)` and `(%r10,%r11)` operands.
- **L561–572** — x87 conditional moves `fcmovb`/`fcmove`/`fcmovbe`/`fcmovu`/`fcmovnb`/`fcmovne`/`fcmovnbe`/`fcmovnu` (all `%st(5), %st`) and the EFLAGS-setting compares `fcomi`/`fucomi`/`fcomip`/`fucomip`.
- **L576–587** — Integer `cmovcc`: memory sources (`cmovo`/`cmovs 0x1000, %eax`), register forms `cmovns`, 16-bit `cmovne %ax, %si`, explicitly suffixed `cmovbw` and `cmovnbel`; x86_64 adds `bswapq %rsi`/`%r10` and 64-bit `cmovz %rdi,%rbx` and `cmovpeq %rsi, %rdx`.
- **L589–590** — Software interrupts `int $3` and `int $0x10`.

### System, flag, and no-operand instructions (L592–647)
- **L592–614** — i386-only `pusha`/`popa`; flag operations `clc`, `cld`, `cli`, `clts`, `cmc`, `lahf`, `sahf`; arch-split `pushfl`/`popfl` versus `pushfq`/`popfq` plus suffixless `pushf`/`popf`; `stc`, `std`, `sti`.
- **L615–631** — i386-only BCD/decimal instructions `aaa`, `aas`, `daa`, `das`, `aad`, `aam`, and `into`; then the sign-extension set in both Intel-style (`cbw`, `cwd`, `cwde`, `cdq`) and AT&T-style (`cbtw`, `cwtd`, `cwtl`, `cltd`) mnemonics.
- **L632–647** — `leave`, `int3`, `iret` with `w`/`l` suffixes and x86_64-only `iretq`; `rsm`, `hlt`, `wait`, `nop`; the VMX set `vmcall`, `vmlaunch`, `vmresume`, `vmxoff`.

### Prefixes (L650–677)
- **L650–660** — An `#if 0` block parking the unimplemented `aword`/`addr16` size-override mnemonics, then the bare prefix opcodes `lock`, `rep`, `repe`, `repz`, `repne`, `repnz` each on its own line followed by `nop`.
- **L662–677** — The same prefixes applied to instructions two ways: `;`-separated on one line (`lock ;negl (%eax)`, `wait ;pushf`, `rep ;stosb`, `repe ;lodsb`, `repz ;cmpsb`, `repne;movsb`, `repnz;outsb`) and as true same-statement prefixes (`lock negl (%eax)` through `repnz outsb`).

### CPU-management and 64-bit extensions (L679–718)
- **L679–686** — `invd`, `wbinvd`, `cpuid`, `wrmsr`, `rdtsc`, `rdmsr`, `rdpmc`, `ud2`.
- **L687–709** — x86_64-only: `syscall`, `sysret`, `sysretq`; fences `lfence`, `mfence`, `sfence`; `prefetchnta 0x18(%rdx)`, `prefetcht0`/`t1`/`t2`, `prefetchw`; `clflush 0x1000(%rax,%rcx)`; `fxsaveq`/`fxrstorq` with `(%rdx)`/`(%r11)` and `(%rcx)`/`(%r10)`; non-temporal stores `movnti %ebx`, suffixed `movntil`, 64-bit `movnti %rax`, and `movntiq %r8`.
- **L711–718** — `lar` with every operand-size pairing of `%ax`/`%eax` sources against `%dx`/`%edx` destinations, plus x86_64 `%rdx` destinations.

### MMX basics and string instructions (L719–748)
- **L719–733** — `emms`; `movd` in all four directions between GPR/absolute memory and `%mm2`–`%mm5`; `movq` between memory and `%mm2`/`%mm4`; `pand` from memory and register; `psllw` by immediate, by memory operand, and by `%mm2`.
- **L735–748** — String/misc instructions including the less common gas alias mnemonics: `xlat`, `cmpsb`, `scmpw` (alias of cmpsw), `insl`, `outsw`, `lodsb`, `slodl` (lodsl alias), `movsb`, `movsl`, `smovb` (movsb alias), `scasb`, `sscaw` (scasw alias), `stosw`, `sstol` (stosl alias).

### Bit scan/test and count instructions (L750–790)
- **L750–759** — `bsf` and `bsr` from memory; the bit-test quartet in register (`bt`/`btc`/`btr`/`bts %edx, 0x1000`) and immediate-suffixed (`btl`/`btcl`/`btrl`/`btsl $2, 0x1000`) forms.
- **L761–790** — `popcnt`, `lzcnt`, and `tzcnt`, each exercised as: 16-bit register form plus the explicit `w`-suffixed alias, 32-bit form (register or memory source, with `l`-suffixed variants such as `lzcntl 8(%edi)` and `tzcntl -24(%edi)`), and an x86_64 block of 64-bit register and memory forms with `q` suffixes over registers including `%r8`, `%r11`, `%r12`, and `%r15`.

### Protected-mode/descriptor-table instructions (L792–850)
- **L792–804** — i386-only `boundl %edx, 0x10000`, `boundw %bx, 0x1000`, and `arpl %bx, 0x1000`; then `lar 0x1000, %eax` and the descriptor-table load/store set `lgdt`, `lidt`, `lldt`, `sgdt`, `sidt`, `sldt`, all with absolute-address operands.
- **L805–820** — x86_64-only: `lgdtq`, `lidtq`, `sgdtq`, `sidtq`, `swapgs` (this block — cited as `gas_directives.S:811` in the CMakeLists skip reason — is what currently blocks the `asm-gas-directives` test, the integrated assembler lacking the `sgdtq`/`sidtq`/`swapgs` encodings), and after several blank lines `str %edx` and `str %r9d`.
- **L822–832** — `lmsw 0x1000`, `lsl 0x1000, %ecx`, `ltr` with memory and `%si` operands, `smsw 0x1000`, `str` with memory, `%ecx`, and `%dx` operands; segment-verification `verr` and `verw`.
- **L834–850** — i386-only segment push/pop of `%ds` in suffixless, `w`, and `l` forms; `fxsave 1(%ebx)` and `fxrstor 1(%ecx)` with odd displacements; immediate pushes split as `pushl $1` (i386) vs `pushq $1` (x86_64) plus common `pushw $1` and `push $1`.

### Predefined-macro check and `.type` variants (L852–869)
- **L852–854** — `#ifdef __ASSEMBLER__` around `inc %eax`, verifying the preprocessor predefines `__ASSEMBLER__` when handling `.S` input.
- **L856–869** — ELF-only block (`#ifndef _WIN32`): nine labels `ft1:`–`ft9:` defined on a single line above `xor %eax, %eax; ret`, then `.type` in all eight accepted spellings of the type argument — `STT_FUNC`, `@STT_FUNC`, `%STT_FUNC`, `"STT_FUNC"`, `function`, `@function`, `%function`, `"function"` — applied to `ft1`–`ft8`.

### Repetition, sections, local labels, and symbol aliasing (L871–920)
- **L871–875** — `pause`, then `.rept 6` / `nop` / `.endr` emitting six nops, and `.fill 4,1,0x90` emitting four 0x90 bytes.
- **L877–885** — Section-stack directives: `.section .text.one,"ax"` with a `nop`, `.previous` to return, then nested `.pushsection .text.one,"ax"` / `.pushsection .text.two,"ax"` each with a `nop` and two matching `.popsection`s.
- **L887–903** — Kernel-style `__bug_table` pattern: `1: ud2`, `.pushsection __bug_table,"a"`, `.align 8`, label `2:` followed by `.long` entries built from local-label arithmetic in both operand orders (`1b - 2b`, `0x600000 - 2b`, `1b + 42`, `43 + 1b`, `2b + 144`, `145 + 2b`), a `.word 164, 0` pair, `.org 2b+32` advancing to a fixed offset from label `2`, and an arch-split pointer-sized `.quad 1b` (x86_64) versus `.long 1b` (i386) before `.popsection`.
- **L904–909** — Labels `3:` and `4:` around `mov %eax,%ecx`, then in a pushed `.text.three` section a `.skip` whose length is the relational expression `(-((4b-3b) > 0) * 2)` with fill `0x90`, testing comparison operators in directive expressions.
- **L911–920** — Symbol binding and aliasing: `.globl overrideme` immediately weakened by `.weak overrideme`; `.globl notimplemented` with the label and a `ret`; the alias established twice — via `.set overrideme, notimplemented` and via the assignment syntax `overrideme = notimplemented` — and finally an actual `overrideme:` label with its own `ret` overriding the weak alias.

### MMX/SSE movd/movq matrix (L922–956)
- **L922–939** — `movd` in all combinations: 32-bit GPR→`%mm`/`%xmm`, memory→`%mm`/`%xmm`, and the reverse stores; the x86_64 block repeats the matrix with 64-bit GPRs (`%rsi`, `%rdi`, `%r12`) and REX-addressed memory (`(%rbx)`, `(%r8)`, `(%r13)`).
- **L941–956** — `movq` variants: memory↔`%mm`, memory→`%xmm`, `%mm4, %mm5` register move; x86_64 adds GPR↔MMX/XMM (`%rcx`→`%mm1`, `%rdx`/`%r13`→`%xmm2`/`%xmm3`), memory→`%xmm3`, and the reverse `%mm1`→`%rdx`, `%xmm3`→`%rcx`, `%xmm4`→`(%rsi)`.

### MMX/SSE integer-op macro sweep (L958–1025)
- **L958–965** — Defines `TEST_MMX_SSE(insn)`, expanding to the mm-mm, xmm-xmm, and memory-to-xmm (`(%ebx), %xmm3`) forms of an instruction, and `TEST_MMX_SSE_I8(insn)`, which adds `$0x42` immediate forms against `%mm4` and `%xmm4`.
- **L967–989** — `TEST_MMX_SSE` invocations for the pack/arithmetic/logic/compare set: `packssdw`, `packsswb`, `packuswb`, `paddb`, `paddw`, `paddd`, `paddsb`, `paddsw`, `paddusb`, `paddusw`, `pand`, `pandn`, `pcmpeqb`, `pcmpeqw`, `pcmpeqd`, `pcmpgtb`, `pcmpgtw`, `pcmpgtd`, `pmaddwd`, `pmulhw`, `pmullw`, `por`, `psllw`.
- **L990–1004** — The shift instructions, each invoked with both macros (register/memory forms via `TEST_MMX_SSE` and immediate forms via `TEST_MMX_SSE_I8`): `psllw`, `pslld`, `psllq`, `psraw`, `psrad`, `psrlw`, `psrld`, `psrlq`.
- **L1005–1018** — Remaining `TEST_MMX_SSE` invocations: `psubb`, `psubw`, `psubd`, `psubsb`, `psubsw`, `psubusb`, `psubusw`, `punpckhbw`, `punpckhwd`, `punpckhdq`, `punpcklbw`, `punpcklwd`, `punpckldq`, `pxor`.
- **L1020–1025** — Direct `cvtpi2ps` conversions from `%mm1` and from memory into `%xmm2`, then the SSE integer min/max extensions via the macro: `pmaxsw`, `pmaxub`, `pminsw`, `pminub`.

# Windows import definitions

The five `.def` files under `runtime/win32/lib/` are module-definition files that serve as mcc's Windows import libraries. Each is a plain-text list of a DLL's exported symbol names under a `LIBRARY <dll>` / `EXPORTS` header. mcc's PE linker consumes them directly instead of binary `.lib` import libraries: when resolving `-l<name>`, `src/libmcc.c` (line 1243) searches `%s/%s.def` and `%s/lib%s.def` before `.dll`, and `src/objfmt/mccpe.c` (lines 1105, 1850) dispatches on the `.def` extension to parse the file and synthesize the import thunks for the named DLL. The build copies them into place with `file(GLOB ... runtime/win32/lib/*.def)` + `file(COPY ... DESTINATION ${CMAKE_BINARY_DIR}/lib)` (CMakeLists.txt line 2406) and installs them to `<mccdir>/lib` (line 2671); the reverse tool, `mcc -impdef <lib>.dll`, generates a `.def` from a DLL (used at line 2596 to produce `libmcc.def` from `libmcc.dll`). None of the five files uses ordinals, `DATA` keywords, or `Name=Forward` forwarders — every entry is a bare exported name, one per line.

## `runtime/win32/lib/kernel32.def` (776 lines)

Import-library definition for `kernel32.dll`, the core Win32 base-services DLL. It gives mcc-linked PE programs access to process/thread control, file and console I/O, memory management (Virtual/Heap/Global/Local), synchronization, locale/NLS, and module loading. The export list is a single alphabetical run (including a number of Win9x-era 16↔32-bit thunk entries such as `Callback*`, `FT_*`, `QT_Thunk`, `MapLS`/`MapSL`, and `SMapLS*`/`SUnMapLS*` that stem from the Win95 export table this list descends from). This DLL is part of mcc's default auto-link set for the PE target (msvcrt/kernel32/user32/gdi32, per docs/TODO.md).

- **L1** — `LIBRARY kernel32.dll`: names the DLL the imports resolve to.
- **L2** — Blank separator line.
- **L3** — `EXPORTS`: begins the export-name list; every following line is one exported symbol.
- **L4–L15** — Atom, exception-handler, and console/backup exports: AddAtomA, AddAtomW, AddVectoredContinueHandler, AddVectoredExceptionHandler, AllocConsole, AllocLSCallback, AllocSLCallback, AreFileApisANSI, AttachConsole, BackupRead, BackupSeek, BackupWrite. AllocLSCallback/AllocSLCallback are Win9x thunk-era entries.
- **L16–L22** — Beep, resource-update, and serial-comm DCB exports: Beep, BeginUpdateResourceA, BeginUpdateResourceW, BuildCommDCBA, BuildCommDCBAndTimeoutsA, BuildCommDCBAndTimeoutsW, BuildCommDCBW.
- **L23–L40** — Named-pipe calls plus Win9x callback thunk stubs: CallNamedPipeA, CallNamedPipeW, then the 16-entry Callback4 through Callback64 family (Callback12, Callback16, Callback20, Callback24, Callback28, Callback32, Callback36, Callback4, Callback40, Callback44, Callback48, Callback52, Callback56, Callback60, Callback64, Callback8) — 16-bit callback thunk generators from the Win95 export table.
- **L41–L62** — Cancel/comm/handle/compare/copy exports: CancelDeviceWakeupRequest, CancelIo, CancelWaitableTimer, ClearCommBreak, ClearCommError, CloseHandle, CloseProfileUserMapping, CloseSystemHandle, CommConfigDialogA, CommConfigDialogW, CompareFileTime, CompareStringA, CompareStringW, ConnectNamedPipe, ContinueDebugEvent, ConvertDefaultLocale, ConvertThreadToFiber, ConvertToGlobalHandle, CopyFileA, CopyFileExA, CopyFileExW, CopyFileW.
- **L63–L94** — Object/file/process creation (`Create*` run), CreateConsoleScreenBuffer through CreateWaitableTimerW: notable entries CreateDirectoryA/W (+Ex), CreateEventA/W, CreateFiber, CreateFileA, CreateFileW, CreateFileMappingA/W, CreateIoCompletionPort, CreateMutexA/W, CreateNamedPipeA/W, CreatePipe, CreateProcessA, CreateProcessW, CreateRemoteThread, CreateSemaphoreA/W, CreateThread, CreateToolhelp32Snapshot; also Win9x-isms CreateKernelThread and CreateSocketHandle.
- **L95–L111** — Debug/delete/device exports: DebugActiveProcess, DebugBreak, DefineDosDeviceA, DefineDosDeviceW, DeleteAtom, DeleteCriticalSection, DeleteFiber, DeleteFileA, DeleteFileW, DeviceIoControl, DisableThreadLibraryCalls, DisconnectNamedPipe, DosDateTimeToFileTime, DuplicateHandle, EndUpdateResourceA, EndUpdateResourceW, EnterCriticalSection.
- **L112–L144** — Enumeration and exit exports (`Enum*` run EnumCalendarInfoA through EnumUILanguagesW, covering calendar/date/locale/resource/codepage enumerators), then EraseTape, EscapeCommFunction, ExitProcess, ExitThread, ExpandEnvironmentStringsA, ExpandEnvironmentStringsW.
- **L145–L161** — Win9x flat-thunk helper exports: FT_Exit0, FT_Exit12, FT_Exit16, FT_Exit20, FT_Exit24, FT_Exit28, FT_Exit32, FT_Exit36, FT_Exit4, FT_Exit40, FT_Exit44, FT_Exit48, FT_Exit52, FT_Exit56, FT_Exit8, FT_Prolog, FT_Thunk.
- **L162–L203** — Fatal-exit, file-time, console-fill, find, flush, format, and free exports, FatalAppExitA through FreeSLCallback: notable FileTimeToLocalFileTime, FileTimeToSystemTime, FindClose, FindFirstFileA, FindFirstFileW (+Ex variants), FindNextFileA/W, FindResourceA/W (+Ex), FlushFileBuffers, FlushViewOfFile, FormatMessageA, FormatMessageW, FreeConsole, FreeEnvironmentStringsA/W, FreeLibrary, FreeLibraryAndExitThread, FreeResource.
- **L204–L383** — Long alphabetical `Get*` run (plus GenerateConsoleCtrlEvent at L204), GetACP (L205) through GetWriteWatch (L383): the process/environment/locale/file query surface. Functionally notable entries: GetCommandLineA (L222) / GetCommandLineW (L223), GetConsoleMode (L230), GetCurrentDirectoryA/W (L237–238), GetCurrentProcess (L239), GetCurrentProcessId (L240), GetCurrentThread (L241), GetCurrentThreadId (L242), GetEnvironmentStrings (L255), GetEnvironmentVariableA/W (L258–259), GetExitCodeProcess (L261), GetFileAttributesA/W (L263, 266), GetFileSize (L268), GetFileType (L270), GetFullPathNameA/W (L271–272), GetLastError (L280), GetLocalTime (L281), GetModuleFileNameA/W (L290–291), GetModuleHandleA (L292) / GetModuleHandleW (L295), GetProcAddress (L316), GetProcessHeap (L319), GetStartupInfoA/W (L338–339), GetStdHandle (L340), GetSystemDirectoryA/W (L348–349), GetSystemInfo (L350), GetSystemTime (L352), GetSystemTimeAsFileTime (L354), GetTempPathA/W (L360–361), GetTickCount (L368), GetVersion (L376), GetVersionExA/W (L377–378), GetWindowsDirectoryA/W (L381–382); Win9x oddities GetDaylightFlag (L245), GetProcessFlags (L318), GetProductName (L326), GetLSCallbackTarget/Template (L277–278), GetSLCallbackTarget/Template (L334–335).
- **L384–L404** — Global memory/atom API run: GlobalAddAtomA, GlobalAddAtomW, GlobalAlloc, GlobalCompact, GlobalDeleteAtom, GlobalFindAtomA, GlobalFindAtomW, GlobalFix, GlobalFlags, GlobalFree, GlobalGetAtomNameA, GlobalGetAtomNameW, GlobalHandle, GlobalLock, GlobalMemoryStatus, GlobalReAlloc, GlobalSize, GlobalUnWire, GlobalUnfix, GlobalUnlock, GlobalWire.
- **L405–L420** — Heap API run: Heap32First, Heap32ListFirst, Heap32ListNext, Heap32Next (toolhelp), HeapAlloc, HeapCompact, HeapCreate, HeapDestroy, HeapFree, HeapLock, HeapReAlloc, HeapSetFlags, HeapSize, HeapUnlock, HeapValidate, HeapWalk.
- **L421–L428** — Init and interlocked exports: InitAtomTable, InitializeCriticalSection, InitializeCriticalSectionAndSpinCount, InterlockedCompareExchange, InterlockedDecrement, InterlockedExchange, InterlockedExchangeAdd, InterlockedIncrement.
- **L429–L450** — Pointer-validation / status exports: InvalidateNLSCache, IsBadCodePtr, IsBadHugeReadPtr, IsBadHugeWritePtr, IsBadReadPtr, IsBadStringPtrA, IsBadStringPtrW, IsBadWritePtr, IsDBCSLeadByte, IsDBCSLeadByteEx, IsDebuggerPresent, IsLSCallback, IsProcessorFeaturePresent, IsSLCallback, IsSystemResumeAutomatic, IsValidCodePage, IsValidLanguageGroup, IsValidLocale, K32Thk1632Epilog, K32Thk1632Prolog, K32_NtCreateFile, K32_RtlNtStatusToDosError (the last four are Win9x kernel thunk/NT-mapping internals).
- **L451–L473** — Locale-map, library-load, and Local memory exports: LCMapStringA, LCMapStringW, LeaveCriticalSection, LoadLibraryA, LoadLibraryExA, LoadLibraryExW, LoadLibraryW, LoadModule, LoadResource, LocalAlloc, LocalCompact, LocalFileTimeToFileTime, LocalFlags, LocalFree, LocalHandle, LocalLock, LocalReAlloc, LocalShrink, LocalSize, LocalUnlock, LockFile, LockFileEx, LockResource.
- **L474–L494** — Win9x mapping thunks plus file mapping and move: MakeCriticalSectionGlobal, MapHInstLS, MapHInstLS_PN, MapHInstSL, MapHInstSL_PN, MapHModuleLS, MapHModuleSL, MapLS, MapSL, MapSLFix, MapViewOfFile, MapViewOfFileEx, Module32First, Module32Next, MoveFileA, MoveFileExA, MoveFileExW, MoveFileW, MulDiv, MultiByteToWideChar, NotifyNLSUserCache.
- **L495–L528** — Open/peek/query exports: OpenEventA/W, OpenFile, OpenFileMappingA/W, OpenMutexA/W, OpenProcess, OpenProfileUserMapping, OpenSemaphoreA/W, OpenThread, OpenVxDHandle, OpenWaitableTimerA/W, OutputDebugStringA, OutputDebugStringW, PeekConsoleInputA/W, PeekNamedPipe, PostQueuedCompletionStatus, PrepareTape, Process32First, Process32Next, PulseEvent, PurgeComm, QT_Thunk (Win9x quick thunk), QueryDosDeviceA/W, QueryNumberOfEventLogRecords, QueryOldestEventLogRecord, QueryPerformanceCounter, QueryPerformanceFrequency, QueueUserAPC.
- **L529–L556** — Exception raise, console/file read, and register/release exports: RaiseException, ReadConsoleA, ReadConsoleInputA/W, ReadConsoleOutputA/W (+Attribute/Character), ReadConsoleW, ReadDirectoryChangesW, ReadFile, ReadFileEx, ReadFileScatter, ReadProcessMemory, RegisterServiceProcess, RegisterSysMsgHandler, ReinitializeCriticalSection, ReleaseMutex, ReleaseSemaphore, RemoveDirectoryA, RemoveDirectoryW, RequestDeviceWakeup, RequestWakeupLatency, ResetEvent, ResetNLSUserInfoCache, ResetWriteWatch, ResumeThread.
- **L557–L564** — Rtl runtime exports (SEH/unwind and memory intrinsics mcc's PE runtime relies on): RtlAddFunctionTable, RtlDeleteFunctionTable, RtlFillMemory, RtlInstallFunctionTableCallback, RtlMoveMemory, RtlUnwind, RtlUnwindEx, RtlZeroMemory.
- **L565–L584** — Win9x segmented-pointer thunk exports: SMapLS, SMapLS_IP_EBP_12 through SMapLS_IP_EBP_8 (offsets 12, 16, 20, 24, 28, 32, 36, 40, 8), SUnMapLS, and SUnMapLS_IP_EBP_12 through SUnMapLS_IP_EBP_8 (same offset set).
- **L585–L664** — Long alphabetical `Set*` run (plus ScrollConsoleScreenBufferA/W at L585–586 and SearchPathA/W at L587–588), SetCalendarInfoA through SignalSysMsgHandlers: notable SetConsoleCtrlHandler (L600), SetConsoleMode (L603), SetCurrentDirectoryA/W (L611–612), SetEndOfFile (L616), SetEnvironmentVariableA/W (L617–618), SetErrorMode (L619), SetEvent (L620), SetFileAttributesA/W (L623–624), SetFilePointer (L625) / SetFilePointerEx (L626), SetFileTime (L627), SetHandleCount (L629), SetLastError (L631), SetPriorityClass (L638), SetStdHandle (L643), SetThreadPriority (L654), SetUnhandledExceptionFilter (L657), SignalObjectAndWait (L663).
- **L665–L672** — Sleep/switch/time exports: SizeofResource, Sleep, SleepEx, SuspendThread, SwitchToFiber, SwitchToThread, SystemTimeToFileTime, SystemTimeToTzSpecificLocalTime.
- **L673–L689** — Terminate/toolhelp/TLS exports: TerminateProcess, TerminateThread, Thread32First, Thread32Next, ThunkConnect32, TlsAlloc, TlsAllocInternal, TlsFree, TlsFreeInternal, TlsGetValue, TlsSetValue, Toolhelp32ReadProcessMemory, TransactNamedPipe, TransmitCommChar, TryEnterCriticalSection, UTRegister, UTUnRegister.
- **L690–L713** — Unmap/unlock/version/virtual-memory exports: UnMapLS, UnMapSLFixArray, UnhandledExceptionFilter, UninitializeCriticalSection, UnlockFile, UnlockFileEx, UnmapViewOfFile, UpdateResourceA, UpdateResourceW, VerLanguageNameA, VerLanguageNameW, VerSetConditionMask, VerifyVersionInfoA, VerifyVersionInfoW, VirtualAlloc, VirtualAllocEx, VirtualFree, VirtualFreeEx, VirtualLock, VirtualProtect, VirtualProtectEx, VirtualQuery, VirtualQueryEx, VirtualUnlock.
- **L714–L747** — Wait and write exports: WaitCommEvent, WaitForDebugEvent, WaitForMultipleObjects(Ex), WaitForSingleObject(Ex), WaitNamedPipeA/W, WideCharToMultiByte, WinExec, WriteConsoleA/W (+Input/Output/Attribute/Character variants, L724–732), WriteFile, WriteFileEx, WriteFileGather, WritePrivateProfileSectionA/W, WritePrivateProfileStringA/W, WritePrivateProfileStructA/W, WriteProcessMemory, WriteProfileSectionA/W, WriteProfileStringA/W, WriteTapemark.
- **L748–L758** — Underscore-prefixed 16-bit-era file I/O and debug exports: _DebugOut, _DebugPrintf, _hread, _hwrite, _lclose, _lcreat, _llseek, _lopen, _lread, _lwrite, dprintf.
- **L759–L776** — Lowercase `lstr*` string exports (each in bare, A, and W forms): lstrcat, lstrcatA, lstrcatW, lstrcmp, lstrcmpA, lstrcmpW, lstrcmpi, lstrcmpiA, lstrcmpiW, lstrcpy, lstrcpyA, lstrcpyW, lstrcpyn, lstrcpynA, lstrcpynW, lstrlen, lstrlenA, lstrlenW.

## `runtime/win32/lib/msvcrt.def` (1321 lines)

Import-library definition for `msvcrt.dll`, the Microsoft C runtime that serves as mcc's libc on the PE/Windows target (the "msvcrt" row in README's libc table). It exports the full ANSI C library (stdio, stdlib, string, math, time, locale), the underscore-prefixed Microsoft CRT extensions (`_open`, `_beginthreadex`, `_snprintf`, the `_mbs*` multibyte and `_w*` wide-char families), CRT startup internals that mcc's `crt1.c` startup code needs (`__getmainargs`, `__set_app_type`, `_initterm`), and a block of MSVC C++-mangled exception/RTTI entry points. Several entries are actually data objects in the real DLL (for example `_HUGE`, `__argc`, `__argv`, `_iob`, `_environ`, `_ctype`, `_pctype`, `_sys_errlist`, `_timezone`, `_tzname`, `_winver`) but are listed as plain names — the file uses no `DATA` keyword.

- **L1** — `LIBRARY msvcrt.dll`: names the DLL the imports resolve to.
- **L2** — Blank separator line.
- **L3** — `EXPORTS`: begins the export-name list.
- **L4–L54** — MSVC C++-decorated (name-mangled) exports, the only decorated names in the five files: constructors/destructors/operators for `__non_rtti_object`, `bad_cast`, `bad_typeid`, `exception`, and `type_info` (`??0...@@QEAA@...`, `??1...@@UEAA@XZ`, `??4...` assignment, `??8`/`??9` type_info comparison, `??_7...@@6B@` vtables, `??_F` default ctor closures); scalar/vector `operator new`/`delete` (`??2@YAPEAX_K@Z`, `??3@YAXPEAX@Z`, `??_U`, `??_V`); and handler setters `?_query_new_handler`, `?_query_new_mode`, `?_set_new_handler`, `?_set_new_mode`, `?_set_se_translator`, `?before@type_info`, `?name@type_info`, `?raw_name@type_info`, `?set_new_handler`, `?set_terminate`, `?set_unexpected`, `?terminate@@YAXXZ`, `?unexpected@@YAXXZ`, `?what@exception`. The `AEBV`/`PEAX_K` codes mark these as the x64 manglings.
- **L55–L79** — CRT debug-heap API run: _CrtCheckMemory, _CrtDbgBreak, _CrtDbgReport, _CrtDbgReportV, _CrtDbgReportW, _CrtDbgReportWV, _CrtDoForAllClientObjects, _CrtDumpMemoryLeaks, _CrtIsMemoryBlock, _CrtIsValidHeapPointer, _CrtIsValidPointer, _CrtMemCheckpoint, _CrtMemDifference, _CrtMemDumpAllObjectsSince, _CrtMemDumpStatistics, _CrtReportBlockType, _CrtSetAllocHook, _CrtSetBreakAlloc, _CrtSetDbgBlockType, _CrtSetDbgFlag, _CrtSetDumpClient, _CrtSetReportFile, _CrtSetReportHook, _CrtSetReportHook2, _CrtSetReportMode.
- **L80–L111** — C++ exception/RTTI runtime and time-format helpers: _CxxThrowException, _Getdays, _Getmonths, _Gettnames, _HUGE (a data export: the double HUGE_VAL), _Strftime, _W_Getdays, _W_Getmonths, _W_Gettnames, _Wcsftime, _XcptFilter, __AdjustPointer, __C_specific_handler (the x64 SEH personality routine), __CppXcptFilter, __CxxFrameHandler, __CxxFrameHandler2, __CxxFrameHandler3, __DestructExceptionObject, the ten __ExceptionPtr* entries (Assign, Compare, Copy, CopyException, Create, CurrentException, Destroy, Rethrow, Swap, ToBool), __RTCastToVoid, __RTDynamicCast, __RTtypeid, __STRINGTOLD.
- **L112–L163** — Double-underscore CRT internals: locale accessors ___lc_codepage_func, ___lc_collate_cp_func, ___lc_handle_func, ___mb_cur_max_func, ___setlc_active_func, ___unguarded_readlc_active_add_func; the startup surface __argc, __argv (data), __badioinfo, __crtCompareStringA/W, __crtGetLocaleInfoW, __crtGetStringTypeW, __crtLCMapStringA/W, __daylight, __dllonexit, __doserrno, __dstbias, __fpecode, __getmainargs (mcc's crt1 entry hook), __initenv, __iob_func (FILE table accessor), __isascii, __iscsym, __iscsymf, __lc_codepage, __lc_collate_cp, __lc_handle, __lconv_init, __mb_cur_max, __pctype_func, __pioinfo, __pwctype_func, __pxcptinfoptrs, __set_app_type, __setlc_active, __setusermatherr, __strncnt, __threadhandle, __threadid, __toascii, __unDName, __unDNameEx (demanglers), __uncaught_exception, __unguarded_readlc_active, __wargv, __wcserror, __wcserror_s, __wcsncnt, __wgetmainargs (wide startup hook), __winitenv.
- **L164–L190** — `_a*` extensions: _abs64, _access, _access_s, _acmdln (data: command line), _aexit_rtn, the aligned-allocation family _aligned_free, _aligned_free_dbg, _aligned_malloc, _aligned_malloc_dbg, _aligned_offset_malloc(_dbg), _aligned_offset_realloc(_dbg), _aligned_realloc(_dbg), _amsg_exit, _assert, _atodbl(_l), _atof_l, _atoflt_l, _atoi64(_l), _atoi_l, _atol_l, _atoldbl(_l).
- **L191–L253** — `_b*`–`_c*` extensions: _beep, _beginthread, _beginthreadex, _c_exit, _cabs, _callnewh, _calloc_dbg, _cexit, console I/O _cgets(_s), _cgetws(_s), _cprintf family (plain/_l/_p/_p_l/_s/_s_l), _cputs, _cputws, _cscanf family, _cwprintf family, _cwscanf family; _chdir, _chdrive, _chgsign(f), _chmod, _chsize(_s), _chvalidator(_l), floating-point control _clearfp, _control87, _controlfp(_s), _copysign(f); _close, _commit, _commode (data), _creat, _create_locale, _crtAssertBusy, _crtBreakAlloc, _crtDbgFlag (data), _ctime32(_s), _ctime64(_s), _ctype (data), _cwait.
- **L254–L298** — `_d*`–`_f*` extensions: _daylight (data), _difftime32, _difftime64, _dstbias (data), _dup, _dup2, _ecvt(_s), _endthread, _endthreadex, _environ (data), _eof, _errno (per-thread errno accessor), the eight _exec* variants (_execl, _execle, _execlp, _execlpe, _execv, _execve, _execvp, _execvpe), _exit, _expand(_dbg), _fcloseall, _fcvt(_s), _fdopen, _fgetchar, _fgetwchar, _filbuf, _fileinfo (data), _filelength(i64), _fileno, _findclose, _findfirst(64/i64), _findnext(64/i64), _finite(f), _flsbuf.
- **L299–L356** — `_f*`–`_get_*` extensions: _flushall, _fmode (data), _fpclass(f), _fpreset, positional/locale printf _fprintf_l/_p/_p_l/_s_l, _fputchar, _fputwchar, _free_dbg, _free_locale, _freea(_s), _fscanf_l, _fscanf_s_l, _fseeki64, _fsopen, _fstat(64/i64), _ftime(32/64, _s), _fullpath(_dbg), _futime(32/64), _fwprintf_l/_p/_p_l/_s_l, _fwscanf_l, _fwscanf_s_l, _gcvt(_s), and the _get_* accessor family (_get_current_locale, _get_doserrno, _get_environ, _get_errno, _get_fileinfo, _get_fmode, _get_heap_handle, _get_osfhandle, _get_osplatform, _get_osver, _get_output_format, _get_pgmptr, _get_sbh_threshold, _get_wenviron, _get_winmajor, _get_winminor, _get_winver, _get_wpgmptr).
- **L357–L379** — Console-get, cwd, and heap extensions: _getch, _getche, _getcwd, _getdcwd, _getdiskfree, _getdrive, _getdrives, _getmaxstdio, _getmbcp, _getpid, _getsystime, _getw, _getwch, _getwche, _getws, _gmtime32(_s), _gmtime64(_s), _heapchk, _heapmin, _heapset, _heapwalk.
- **L380–L389** — Math/init/data entries: _hypot, _hypotf, _i64toa(_s), _i64tow(_s), _initterm, _initterm_e (CRT initializer walkers used at startup), _invalid_parameter, _iob (data: the FILE array).
- **L390–L475** — `_is*` classification run with `_l` locale variants: _isalnum_l through _isxdigit_l, including the single-byte/multibyte families _ismbb* (alnum/alpha/graph/kalnum/kana/kprint/kpunct/lead/print/punct/trail), _ismbc* (alnum/alpha/digit/graph/hira/kata/l0/l1/l2/legal/lower/print/punct/space/symbol/upper), _ismbslead, _ismbstrail, _isnan, _isnanf, and the wide _isw*_l set (_iswalnum_l … _iswxdigit_l), plus _isctype(_l), _isleadbyte_l.
- **L476–L507** — Conversion, search, and misc extensions: _itoa(_s), _itow(_s), Bessel _j0, _j1, _jn, _kbhit, _lfind(_s), _local_unwind, _localtime32(_s), _localtime64(_s), _lock, _locking, _logb(f), _lrotl, _lrotr, _lsearch(_s), _lseek, _lseeki64, _ltoa(_s), _ltow(_s), _makepath(_s), _malloc_dbg.
- **L508–L645** — Long `_mb*` multibyte-string run, _mbbtombc (L508) through _mbtowc_l (L645): byte-classification _mbbtype, _mbcasemap, JIS/JMS conversion _mbcjistojms(_l), _mbcjmstojis(_l), character ops _mbclen, _mbctohira/_mbctokata/_mbctolower/_mbctombb/_mbctoupper, _mbctype (data), and the full _mbs* string family (bt ype, cat, chr, cmp, coll, cspn, dec, dup, icmp, icoll, inc, len, lwr, the _mbsnb* byte-counted set, the _mbsn* set, nextc, pbrk, rchr, rev, set, spn, spnp, str, tok, upr — most with `_l`, `_s`, and `_s_l` variants), plus _mbstowcs_l, _mbstowcs_s_l, _mbstrlen(_l), _mbstrnlen(_l), _mbsupr family.
- **L646–L703** — `_mem*`–`_s*` extensions: _memccpy, _memicmp(_l), _mkdir, _mkgmtime(32/64), _mktemp(_s), _mktime32, _mktime64, _msize(_dbg), _nextafter(f), _onexit, _open, _open_osfhandle, _osplatform, _osver (data), _pclose, _pctype (data), _pgmptr (data), _pipe, _popen, _printf_l/_p/_p_l/_s_l, _purecall, _putch, _putenv(_s), _putw, _putwch, _putws, _pwctype (data), _read, _realloc_dbg, _resetstkoflw, _rmdir, _rmtmp, _rotl(64), _rotr(64), _scalb(f), _scanf_l, _scanf_s_l, _scprintf family, _scwprintf family, _searchenv(_s).
- **L704–L718** — `_set*` control extensions: _set_controlfp, _set_doserrno, _set_errno, _set_error_mode, _set_fileinfo, _set_fmode, _set_output_format, _set_sbh_threshold, _seterrormode, _setjmp, _setjmpex (the CRT setjmp intrinsics), _setmaxstdio, _setmbcp, _setmode, _setsystime.
- **L719–L758** — Bounded printf/scanf, spawn, and stat extensions: _sleep, the _snprintf family (_snprintf, _snprintf_c, _snprintf_c_l, _snprintf_l, _snprintf_s, _snprintf_s_l), _snscanf family, _snwprintf family, _snwscanf family, _sopen(_s), the eight _spawn* variants (_spawnl, _spawnle, _spawnlp, _spawnlpe, _spawnv, _spawnve, _spawnvp, _spawnvpe), _splitpath(_s), _sprintf_l/_p_l/_s_l, _sscanf_l, _sscanf_s_l, _stat, _stat64, _stati64, _statusfp.
- **L759–L809** — `_str*` string extensions: _strcmpi, _strcoll_l, _strdate(_s), _strdup(_dbg), _strerror(_s), _stricmp(_l), _stricoll(_l), _strlwr family, _strncoll(_l), _strnicmp(_l), _strnicoll(_l), _strnset(_s), _strrev, _strset(_s), _strtime(_s), _strtod_l, _strtoi64(_l), _strtol_l, _strtoui64(_l), _strtoul_l, _strupr family, _strxfrm_l, _swab, _swprintf(_c/_c_l/_p_l/_s_l), _swscanf_l, _swscanf_s_l, _sys_errlist, _sys_nerr (both data).
- **L810–L841** — Tell/time/conversion extensions: _tell, _telli64, _tempnam(_dbg), _time32, _time64, _timezone (data), _tolower(_l), _toupper(_l), _towlower_l, _towupper_l, _tzname (data), _tzset, _ui64toa(_s), _ui64tow(_s), _ultoa(_s), _ultow(_s), _umask(_s), _ungetch, _ungetwch, _unlink, _unlock, _utime, _utime32, _utime64.
- **L842–L895** — `_v*` varargs printf run: the console _vcprintf and _vcwprintf families, _vfprintf_l/_p/_p_l/_s_l, _vfwprintf_l/_p/_p_l/_s_l, _vprintf_l/_p/_p_l/_s_l, _vscprintf(_l/_p_l), _vscwprintf(_l/_p_l), _vsnprintf(_c/_c_l/_l/_s/_s_l), _vsnwprintf(_l/_s/_s_l), _vsprintf_l/_p/_p_l/_s_l, _vswprintf(_c/_c_l/_l/_p_l/_s_l), _vwprintf_l/_p/_p_l/_s_l.
- **L896–L1043** — Long `_w*` wide-character CRT run, _waccess (L896) through _wutime64 (L1043): wide mirrors of the whole extension surface — _wasctime, _wassert, _wchdir, _wchmod, _wcmdln (data), _wcreat, the _wcs* extensions (_wcsdup, _wcsicmp, _wcsicoll, _wcslwr/_wcsupr families, _wcsnicmp, _wcsnset, _wcsrev, _wcsset, _wcstoi64/_wcstoui64, _wcstombs_l), _wctime(32/64), _wctomb_l, _wctype (data), _wenviron (data), the _wexec* eight, _wfdopen, _wfindfirst/_wfindnext (+64/i64), _wfopen(_s), _wfreopen(_s), _wfsopen, _wfullpath, _wgetcwd, _wgetdcwd, _wgetenv(_s), _winmajor, _winminor, _winver (data), _winput_s, _wmakepath, _wmkdir, _wmktemp, _wopen, _woutput_s, _wperror, _wpgmptr (data), _wpopen, _wprintf_l/_p/_p_l/_s_l, _wputenv(_s), _wremove, _wrename, _write, _wrmdir, _wscanf_l, _wscanf_s_l, _wsearchenv, _wsetlocale, _wsopen(_s), the _wspawn* eight, _wsplitpath, _wstat(64/i64), _wstrdate, _wstrtime, _wsystem, _wtempnam, _wtmpnam, _wtof(_l), _wtoi(64), _wtol, _wunlink, _wutime(32/64).
- **L1044–L1046** — Bessel functions of the second kind: _y0, _y1, _yn.
- **L1047–L1078** — Standard C run, part 1 (abort through div): abort, abs, acos(f), asctime(_s), asin(f), atan, atan2(f), atanf, atexit, atof, atoi, atol, bsearch(_s), btowc, calloc, ceil(f), clearerr(_s), clock, cos(f), cosh(f), ctime, difftime, div.
- **L1079–L1118** — Standard C run, part 2 (exit through fwscanf_s): exit, exp(f), fabs, the stdio core fclose, feof, ferror, fflush, fgetc, fgetpos, fgets, fgetwc, fgetws, floor(f), fmod(f), fopen(_s), fprintf(_s), fputc, fputs, fputwc, fputws, fread, free, freopen(_s), frexp, fscanf(_s), fseek, fsetpos, ftell, fwprintf(_s), fwrite, fwscanf(_s).
- **L1119–L1162** — Standard C run, part 3 (getc through logf): getc, getchar, getenv(_s), gets, getwc, getwchar, gmtime, is_wctype, the ctype set isalnum … isxdigit (including the isw* wide set and iswascii/isleadbyte), labs, ldexp, ldiv, lldiv, localeconv, localtime, log, log10(f), logf.
- **L1163–L1213** — Standard C run, part 4 (longjmp through sinhf): longjmp, malloc, mblen, mbrlen, mbrtowc, mbsdup_dbg, mbsrtowcs(_s), mbstowcs(_s), mbtowc, memchr, memcmp, memcpy(_s), memmove(_s), memset, mktime, modf(f), perror, pow(f), printf(_s), putc, putchar, puts, putwc, putwchar, qsort(_s), raise, rand(_s), realloc, remove, rename, rewind, scanf(_s), setbuf, setjmp, setlocale, setvbuf, signal, sin(f), sinh(f).
- **L1214–L1258** — Standard C run, part 5 (sprintf through time): sprintf(_s), sqrt(f), srand, sscanf(_s), the core string set strcat(_s), strchr, strcmp, strcoll, strcpy(_s), strcspn, strerror(_s), strftime, strlen, strncat(_s), strncmp, strncpy(_s), strnlen, strpbrk, strrchr, strspn, strstr, strtod, strtok(_s), strtol, strtoul, strxfrm, swprintf(_s), swscanf(_s), system, tan(f), tanh(f), time.
- **L1259–L1282** — Standard C run, part 6 (tmpfile through vwprintf_s): tmpfile(_s), tmpnam(_s), tolower, toupper, towlower, towupper, ungetc, ungetwc, utime, vfprintf(_s), vfwprintf(_s), vprintf(_s), vsnprintf, vsprintf(_s), vswprintf(_s), vwprintf(_s).
- **L1283–L1321** — Standard C run, part 7, wide-string tail (wcrtomb through wscanf_s): wcrtomb(_s), the wcs* set wcscat(_s), wcschr, wcscmp, wcscoll, wcscpy(_s), wcscspn, wcsftime, wcslen, wcsncat(_s), wcsncmp, wcsncpy(_s), wcsnlen, wcspbrk, wcsrchr, wcsrtombs(_s), wcsspn, wcsstr, wcstod, wcstok(_s), wcstol, wcstombs(_s), wcstoul, wcsxfrm, wctob, wctomb(_s), wprintf(_s), wscanf(_s).

## `runtime/win32/lib/user32.def` (658 lines)

Import-library definition for `user32.dll`, the Win32 windowing/USER subsystem: window creation and management, the message loop, dialogs, menus, clipboard, keyboard/mouse input, DDE, and hooks. It is in mcc's default PE auto-link set, so programs using `<windows.h>` GUI calls link without an explicit `-luser32`. The list is alphabetical with one notable irregularity — `SetWindowLongPtrA`/`SetWindowLongPtrW` are inserted out of order immediately after their `Get` counterparts (L341–342) — and it retains some Win9x-internal entries (`InitSharedTable`, `InitTask`, `SysErrorBox`, `WinOldAppHackoMatic`, `YieldTask`, `WNDPROC_CALLBACK`).

- **L1** — `LIBRARY user32.dll`: names the DLL the imports resolve to.
- **L2** — Blank separator line.
- **L3** — `EXPORTS`: begins the export-name list.
- **L4–L30** — Keyboard-layout, window-geometry, menu-append, and paint exports: ActivateKeyboardLayout, AdjustWindowRect, AdjustWindowRectEx, AlignRects, AllowSetForegroundWindow, AnimateWindow, AnyPopup, AppendMenuA, AppendMenuW, ArrangeIconicWindows, AttachThreadInput, BeginDeferWindowPos, BeginPaint, BlockInput, BringWindowToTop, BroadcastSystemMessage(A/W), CalcChildScroll, CallMsgFilter(A/W), CallNextHookEx, CallWindowProcA, CallWindowProcW, CascadeChildWindows, CascadeWindows.
- **L31–L61** — Clipboard-chain, display-settings, character-conversion, and check exports: ChangeClipboardChain, ChangeDisplaySettingsA/W (+Ex), ChangeMenuA/W, CharLowerA/W (+Buff), CharNextA/W (+Ex), CharPrevA/W (+Ex), CharToOemA/W (+Buff), CharUpperA/W (+Buff), CheckDlgButton, CheckMenuItem, CheckMenuRadioItem, CheckRadioButton.
- **L62–L98** — Coordinate, close, copy, and creation exports: ChildWindowFromPoint(Ex), ClientThreadConnect, ClientToScreen, ClipCursor, CloseClipboard, CloseDesktop, CloseWindow, CloseWindowStation, CopyAcceleratorTableA/W, CopyIcon, CopyImage, CopyRect, CountClipboardFormats, CreateAcceleratorTableA/W, CreateCaret, CreateCursor, CreateDesktopA/W, CreateDialogIndirectParamA/W, CreateDialogParamA/W, CreateIcon, CreateIconFromResource(Ex), CreateIconIndirect, CreateMDIWindowA/W, CreateMenu, CreatePopupMenu, CreateWindowExA, CreateWindowExW, CreateWindowStationA/W.
- **L99–L130** — DDEML (Dynamic Data Exchange) run: DdeAbandonTransaction, DdeAccessData, DdeAddData, DdeClientTransaction, DdeCmpStringHandles, DdeConnect, DdeConnectList, DdeCreateDataHandle, DdeCreateStringHandleA/W, DdeDisconnect, DdeDisconnectList, DdeEnableCallback, DdeFreeDataHandle, DdeFreeStringHandle, DdeGetData, DdeGetLastError, DdeImpersonateClient, DdeInitializeA/W, DdeKeepStringHandle, DdeNameService, DdePostAdvise, DdeQueryConvInfo, DdeQueryNextServer, DdeQueryStringA/W, DdeReconnect, DdeSetQualityOfService, DdeSetUserHandle, DdeUnaccessData, DdeUninitialize.
- **L131–L160** — Default window procedures, destroy, dialog, and dispatch exports: DefDlgProcA/W, DefFrameProcA/W, DefMDIChildProcA/W, DefWindowProcA, DefWindowProcW, DeferWindowPos, DeleteMenu, DestroyAcceleratorTable, DestroyCaret, DestroyCursor, DestroyIcon, DestroyMenu, DestroyWindow, DialogBoxIndirectParamA/W, DialogBoxParamA/W, DispatchMessageA, DispatchMessageW, DlgDirListA/W, DlgDirListComboBoxA/W, DlgDirSelectComboBoxExA/W, DlgDirSelectExA/W.
- **L161–L190** — Drag/draw and enable/end exports: DragDetect, DragObject, DrawAnimatedRects, DrawCaption(TempA/W), DrawEdge, DrawFocusRect, DrawFrame, DrawFrameControl, DrawIcon, DrawIconEx, DrawMenuBar(Temp), DrawStateA/W, DrawTextA, DrawTextExA/W, DrawTextW, EditWndProc, EmptyClipboard, EnableMenuItem, EnableScrollBar, EnableWindow, EndDeferWindowPos, EndDialog, EndMenu, EndPaint, EndTask.
- **L191–L222** — Enumeration and find exports: EnumChildWindows, EnumClipboardFormats, EnumDesktopWindows, EnumDesktopsA/W, EnumDisplayDevicesA/W, EnumDisplayMonitors, EnumDisplaySettingsA/W (+Ex), EnumPropsA/W (+Ex), EnumThreadWindows, EnumWindowStationsA/W, EnumWindows, EqualRect, ExcludeUpdateRgn, ExitWindowsEx, FillRect, FindWindowA, FindWindowExA/W, FindWindowW, FlashWindow, FlashWindowEx, FrameRect, FreeDDElParam.
- **L223–L355** — Long alphabetical `Get*` run, GetActiveWindow (L223) through GetWindowWord (L355): the query surface for windows, input, and UI state. Functionally notable entries: GetAsyncKeyState (L226), GetCapture (L227), GetClassInfo/ClassLong/ClassName (L230–238), GetClientRect (L239), GetClipboardData (L241), GetCursorPos (L250), GetDC (L251) / GetDCEx (L252), GetDesktopWindow (L253), GetDlgItem (L256), GetDlgItemTextA/W (L258–259), GetFocus (L261), GetForegroundWindow (L262), GetKeyState (L272), GetKeyboardState (L277), GetMenu (L281) and the GetMenu* family, GetMessageA (L295) / GetMessageW (L299), GetMonitorInfoA/W (L300–301), GetParent (L308), GetScrollInfo (L316), GetSysColor (L321), GetSysColorBrush (L322), GetSystemMenu (L323), GetSystemMetrics (L324), GetUpdateRect (L330), GetWindowDC (L337), GetWindowLongPtrA (L339) / GetWindowLongPtrW (L340) followed out-of-alphabetical-order by SetWindowLongPtrA (L341) / SetWindowLongPtrW (L342) — the two Set entries are embedded inside the Get run so the 64-bit window-data accessors sit together — then GetWindowLongA/W (L343–344), GetWindowPlacement (L347), GetWindowRect (L348), GetWindowTextA/W (L350, 353), GetWindowThreadProcessId (L354).
- **L356–L404** — GrayString, IME, rect-math, and Is-predicate exports: GrayStringA/W, HasSystemSleepStarted, HideCaret, HiliteMenuItem, the IME block IMPGetIMEA/W, IMPQueryIMEA/W, IMPSetIMEA/W, ImpersonateDdeClientWindow, InSendMessage(Ex), InflateRect, InitSharedTable, InitTask (Win9x internals), InsertMenuA/W, InsertMenuItemA/W, InternalGetWindowText, IntersectRect, InvalidateRect, InvalidateRgn, InvertRect, IsCharAlpha(Numeric)A/W, IsCharLowerA/W, IsCharUpperA/W, IsChild, IsClipboardFormatAvailable, IsDialogMessage(A/W), IsDlgButtonChecked, IsHungThread, IsIconic, IsMenu, IsRectEmpty, IsWindow, IsWindowEnabled, IsWindowUnicode, IsWindowVisible, IsZoomed.
- **L405–L430** — Timer-kill and resource-loading exports: KillTimer, LoadAcceleratorsA/W, LoadBitmapA/W, LoadCursorA/W, LoadCursorFromFileA/W, LoadIconA, LoadIconW, LoadImageA, LoadImageW, LoadKeyboardLayoutA/W, LoadMenuA/W, LoadMenuIndirectA/W, LoadStringA, LoadStringW, LockSetForegroundWindow, LockWindowStation, LockWindowUpdate, LookupIconIdFromDirectory(Ex).
- **L431–L459** — Mapping, message-box, monitor, and OEM-conversion exports: MapDialogRect, MapVirtualKeyA/W (+Ex), MapWindowPoints, MenuItemFromPoint, MessageBeep, MessageBoxA, MessageBoxExA/W, MessageBoxIndirectA/W, MessageBoxW, ModifyAccess, ModifyMenuA/W, MonitorFromPoint, MonitorFromRect, MonitorFromWindow, MoveWindow, MsgWaitForMultipleObjects(Ex), NotifyWinEvent, OemKeyScan, OemToCharA/W (+Buff).
- **L460–L503** — Offset/open/post/register/release exports: OffsetRect, OpenClipboard, OpenDesktopA/W, OpenIcon, OpenInputDesktop, OpenWindowStationA/W, PackDDElParam, PaintDesktop, PeekMessageA, PeekMessageW, PlaySoundEvent, PostMessageA, PostMessageW, PostQuitMessage, PostThreadMessageA/W, PtInRect, RealChildWindowFromPoint, RealGetWindowClass, RedrawWindow, RegisterClassA, RegisterClassExA/W, RegisterClassW, RegisterClipboardFormatA/W, RegisterDeviceNotificationA/W, RegisterHotKey, RegisterLogonProcess, RegisterNetworkCapabilities, RegisterSystemThread, RegisterTasklist, RegisterWindowMessageA/W, ReleaseCapture, ReleaseDC, RemoveMenu, RemovePropA/W, ReplyMessage, ReuseDDElParam.
- **L504–L520** — Screen/scroll and SendMessage family: ScreenToClient, ScrollDC, ScrollWindow, ScrollWindowEx, SendDlgItemMessageA/W, SendIMEMessageExA/W, SendInput, SendMessageA, SendMessageCallbackA/W, SendMessageTimeoutA/W, SendMessageW, SendNotifyMessageA/W.
- **L521–L587** — Long `Set*` run, SetActiveWindow (L521) through SetWindowsHookW (L587): notable SetCapture (L522), SetCaretPos (L524), SetClassLongA/W (L525–526), SetClipboardData (L528), SetCursor (L530), SetCursorPos (L531), SetDlgItemInt/TextA/W (L535–537), SetFocus (L539), SetForegroundWindow (L540), SetKeyboardState (L542), SetMenu (L545) and SetMenu* family, SetParent (L554), SetPropA/W (L557–558), SetRect (L559), SetScrollInfo/Pos/Range (L561–563), SetSysColors (L565), SetTimer (L569), SetWinEventHook (L573), SetWindowLongA (L576) / SetWindowLongW (L577), SetWindowPlacement (L578), SetWindowPos (L579), SetWindowRgn (L580), SetWindowTextA/W (L581–582), SetWindowsHookA (L584), SetWindowsHookExA/W (L585–586); also Win9x-internal SetDeskWallpaper (L533), SetDesktopBitmap (L534), SetLogonNotifyWindow (L544), SetShellWindow (L564), SetWindowFullScreenState (L575).
- **L588–L604** — Show/subtract/switch/system exports: ShowCaret, ShowCursor, ShowOwnedPopups, ShowScrollBar, ShowWindow, ShowWindowAsync, SubtractRect, SwapMouseButton, SwitchDesktop, SwitchToThisWindow, SysErrorBox, SystemParametersInfoA, SystemParametersInfoW, TabbedTextOutA/W, TileChildWindows, TileWindows.
- **L605–L651** — Translation, tracking, unhook/unregister, update, and validation exports: ToAscii(Ex), ToUnicode(Ex), TrackMouseEvent, TrackPopupMenu(Ex), TranslateAccelerator(A/W), TranslateMDISysAccel, TranslateMessage, UnhookWinEvent, UnhookWindowsHook(Ex), UnionRect, UnloadKeyboardLayout, UnlockWindowStation, UnpackDDElParam, UnregisterClassA/W, UnregisterDeviceNotification, UnregisterHotKey, UpdateWindow, the Win9x-internal UserClientDllInitialize, UserIsSystemResumeAutomatic, UserSetDeviceHoldState, UserSignalProc, UserTickleTimer, ValidateRect, ValidateRgn, VkKeyScanA/W (+Ex), WINNLSEnableIME, WINNLSGetEnableStatus, WINNLSGetIMEHotkey, WNDPROC_CALLBACK, WaitForInputIdle, WaitMessage, WinHelpA, WinHelpW, WinOldAppHackoMatic (Win9x relic), WindowFromDC, WindowFromPoint, YieldTask.
- **L652–L658** — Underscore/lowercase tail: _SetProcessDefaultLayout, keybd_event, mouse_event (legacy input injectors), wsprintfA, wsprintfW, wvsprintfA, wvsprintfW.

## `runtime/win32/lib/gdi32.def` (337 lines)

Import-library definition for `gdi32.dll`, the Win32 Graphics Device Interface: device contexts, drawing primitives, bitmaps and DIBs, fonts and text, pens/brushes/palettes, regions, clipping, coordinate transforms, metafiles, printing (StartDoc/EndDoc), and ICM color management. It is in mcc's default PE auto-link set. The list is alphabetical; it includes the Win9x-internal `ByeByeGDI` and closes with three lowercase entries, two of which (`pfnRealizePalette`, `pfnSelectPalette`) are function-pointer exports in the real DLL, listed here as plain names.

- **L1** — `LIBRARY gdi32.dll`: names the DLL the imports resolve to.
- **L2** — Blank separator line.
- **L3** — `EXPORTS`: begins the export-name list.
- **L4–L29** — Document/path/arc/blt and metafile-copy exports: AbortDoc, AbortPath, AddFontResourceA, AddFontResourceW, AngleArc, AnimatePalette, Arc, ArcTo, BeginPath, BitBlt, ByeByeGDI (Win9x internal), CancelDC, CheckColorsInGamut, ChoosePixelFormat, Chord, CloseEnhMetaFile, CloseFigure, CloseMetaFile, ColorCorrectPalette, ColorMatchToTarget, CombineRgn, CombineTransform, CopyEnhMetaFileA/W, CopyMetaFileA/W.
- **L30–L69** — GDI object creation (`Create*` run), CreateBitmap through CreateSolidBrush: CreateBitmap, CreateBitmapIndirect, CreateBrushIndirect, CreateColorSpaceA/W, CreateCompatibleBitmap, CreateCompatibleDC, CreateDCA, CreateDCW, CreateDIBPatternBrush(Pt), CreateDIBSection, CreateDIBitmap, CreateDiscardableBitmap, CreateEllipticRgn(Indirect), CreateEnhMetaFileA/W, CreateFontA, CreateFontIndirectA, CreateFontIndirectW, CreateFontW, CreateHalftonePalette, CreateHatchBrush, CreateICA/W, CreateMetaFileA/W, CreatePalette, CreatePatternBrush, CreatePen, CreatePenIndirect, CreatePolyPolygonRgn, CreatePolygonRgn, CreateRectRgn, CreateRectRgnIndirect, CreateRoundRectRgn, CreateScalableFontResourceA/W, CreateSolidBrush.
- **L70–L96** — Coordinate-mapping, deletion, and enumeration exports: DPtoLP, DeleteColorSpace, DeleteDC, DeleteEnhMetaFile, DeleteMetaFile, DeleteObject, DescribePixelFormat, DeviceCapabilitiesEx(A/W), DrawEscape, Ellipse, EnableEUDC, EndDoc, EndPage, EndPath, EnumEnhMetaFile, EnumFontFamiliesA/W (+Ex), EnumFontsA, EnumFontsW, EnumICMProfilesA/W, EnumMetaFile, EnumObjects.
- **L97–L119** — Region-equality, escape, clipping, ext-drawing, path-fill, and Gdi batch exports: EqualRgn, Escape, ExcludeClipRect, ExtCreatePen, ExtCreateRegion, ExtEscape, ExtFloodFill, ExtSelectClipRgn, ExtTextOutA, ExtTextOutW, FillPath, FillRgn, FixBrushOrgEx, FlattenPath, FloodFill, FrameRgn, GdiComment, GdiFlush, GdiGetBatchLimit, GdiPlayDCScript, GdiPlayJournal, GdiPlayScript, GdiSetBatchLimit (the GdiPlay* entries are Win9x-era journal internals).
- **L120–L220** — Long alphabetical `Get*` run, GetArcDirection (L120) through GetWorldTransform (L220): the DC/object/text query surface. Functionally notable entries: GetBitmapBits (L122), GetBkColor (L124) / GetBkMode (L125), GetCharWidth32A/W (L132–133), GetClipBox (L140), GetCurrentObject (L144), GetCurrentPositionEx (L145), GetDIBColorTable (L147), GetDIBits (L148), GetDeviceCaps (L149), GetEnhMetaFile family (L151–157), GetFontData (L158), GetGlyphOutline(A/W) (L161–163), GetKerningPairs(A/W) (L167–169), GetMapMode (L173), GetNearestColor (L179), GetObjectA (L181) / GetObjectW (L183), GetObjectType (L182), GetOutlineTextMetricsA/W (L184–185), GetPaletteEntries (L186), GetPath (L187), GetPixel (L188), GetPolyFillMode (L190), GetROP2 (L191), GetRegionData (L194), GetRgnBox (L195), GetStockObject (L196), GetStretchBltMode (L197), GetSystemPaletteEntries (L198), GetTextAlign (L200), GetTextColor (L204), GetTextExtentExPointA/W (L205–206), GetTextExtentPoint32A (L207) / GetTextExtentPoint32W (L208), GetTextFaceA/W (L211–212), GetTextMetricsA (L213) / GetTextMetricsW (L214), GetViewportExtEx/OrgEx (L215–216), GetWindowExtEx/OrgEx (L218–219), GetWorldTransform (L220).
- **L221–L253** — Clipping, line, mask-blt, offset, paint, metafile-play, and polygon exports: IntersectClipRect, InvertRgn, LPtoDP, LineDDA, LineTo, MaskBlt, ModifyWorldTransform, MoveToEx, OffsetClipRgn, OffsetRgn, OffsetViewportOrgEx, OffsetWindowOrgEx, PaintRgn, PatBlt, PathToRegion, Pie, PlayEnhMetaFile(Record), PlayMetaFile(Record), PlgBlt, PolyBezier, PolyBezierTo, PolyDraw, PolyPolygon, PolyPolyline, PolyTextOutA/W, Polygon, Polyline, PolylineTo, PtInRegion, PtVisible.
- **L254–L271** — Palette-realize, rectangle, DC save/restore, and selection exports: RealizePalette, RectInRegion, RectVisible, Rectangle, RemoveFontResourceA/W, ResetDCA/W, ResizePalette, RestoreDC, RoundRect, SaveDC, ScaleViewportExtEx, ScaleWindowExtEx, SelectClipPath, SelectClipRgn, SelectObject, SelectPalette.
- **L272–L318** — DC-attribute `Set*` run, SetAbortProc through SetWorldTransform: SetAbortProc, SetArcDirection, SetBitmapBits, SetBitmapDimensionEx, SetBkColor, SetBkMode, SetBoundsRect, SetBrushOrgEx, SetColorAdjustment, SetColorSpace, SetDIBColorTable, SetDIBits, SetDIBitsToDevice, SetDeviceGammaRamp, SetEnhMetaFileBits, SetFontEnumeration, SetGraphicsMode, SetICMMode, SetICMProfileA/W, SetLayout, SetMagicColors (Win9x internal), SetMapMode, SetMapperFlags, SetMetaFileBitsEx, SetMetaRgn, SetMiterLimit, SetObjectOwner, SetPaletteEntries, SetPixel, SetPixelFormat, SetPixelV, SetPolyFillMode, SetROP2, SetRectRgn, SetStretchBltMode, SetSystemPaletteUse, SetTextAlign, SetTextCharacterExtra, SetTextColor, SetTextJustification, SetViewportExtEx, SetViewportOrgEx, SetWinMetaFileBits, SetWindowExtEx, SetWindowOrgEx, SetWorldTransform.
- **L319–L334** — Printing, stretch-blt, path-stroke, and text-out exports: StartDocA, StartDocW, StartPage, StretchBlt, StretchDIBits, StrokeAndFillPath, StrokePath, SwapBuffers (OpenGL buffer swap), TextOutA, TextOutW, TranslateCharsetInfo, UnrealizeObject, UpdateColors, UpdateICMRegKeyA, UpdateICMRegKeyW, WidenPath.
- **L335** — gdiPlaySpoolStream: lowercase Win9x print-spooler internal.
- **L336** — pfnRealizePalette: exported palette-realize hook; a function-pointer (data-like) export in the real DLL, listed as a plain name.
- **L337** — pfnSelectPalette: exported palette-select hook; same function-pointer nature as pfnRealizePalette.

## `runtime/win32/lib/ws2_32.def` (198 lines)

Import-library definition for `ws2_32.dll`, the Winsock 2 sockets library. Unlike the other four DLLs it is not in the default auto-link set — programs pass `-lws2_32` and mcc resolves it to this file. It covers the modern Unicode addrinfo/inet helpers, the full `WSA*` Winsock 2 application API, the `WSC*` catalog/service-provider install API, the `Wah*` Winsock helper functions, and finally the classic lowercase BSD sockets API (`socket`, `connect`, `send`, `recv`, ...). The ordering is ASCII-alphabetical, which places all uppercase families before the lowercase BSD tail.

- **L1** — `LIBRARY ws2_32.dll`: names the DLL the imports resolve to.
- **L2** — Blank separator line.
- **L3** — `EXPORTS`: begins the export-name list.
- **L4–L17** — Modern Unicode/extended name-resolution exports: FreeAddrInfoEx, FreeAddrInfoExW, FreeAddrInfoW, GetAddrInfoExA, GetAddrInfoExCancel, GetAddrInfoExOverlappedResult, GetAddrInfoExW, GetAddrInfoW, GetHostNameW, GetNameInfoW, InetNtopW, InetPtonW, SetAddrInfoExA, SetAddrInfoExW.
- **L18–L20** — Legacy and SPI upcall exports: WEP (the 16-bit "Windows Exit Procedure" relic), WPUCompleteOverlappedRequest, WPUGetProviderPathEx (WinSock Provider Upcall entries used by layered service providers).
- **L21–L98** — Winsock 2 application API (`WSA*` run), WSAAccept through WSApSetPostRoutine: notable WSAStartup (L92) and WSACleanup (L34) (mandatory init/teardown), WSAGetLastError (L51) / WSASetLastError (L87), WSASocketA (L90) / WSASocketW (L91), WSAConnect (L36) and WSAConnectByList/ByNameA/W (L37–39), WSASend (L81), WSASendMsg (L83), WSASendTo (L84), WSARecv (L76), WSARecvFrom (L78), WSAIoctl (L62), WSAPoll (L73), WSAEventSelect (L50), WSAAsyncSelect (L31) and the WSAAsyncGet* database family (L25–30), the event objects WSACreateEvent/WSACloseEvent/WSASetEvent/WSAResetEvent/WSAWaitForMultipleEvents (L40, 35, 86, 80, 97), WSAAddressToStringA/W (L22–23), WSAStringToAddressA/W (L93–94), WSADuplicateSocketA/W (L41–42), WSAEnumProtocolsA/W (L48–49), WSAEnumNetworkEvents (L47), WSAGetOverlappedResult (L52), WSAJoinLeaf (L64), the WSALookupService* name-space queries (L65–69), WSANSPIoctl (L70), byte-order helpers WSAHtonl/WSAHtons/WSANtohl/WSANtohs (L58–59, 71–72), blocking-hook relics WSAIsBlocking/WSASetBlockingHook/WSAUnhookBlockingHook/WSACancelBlockingCall (L63, 85, 96, 33), and provider/service-class management WSAAdvertiseProvider, WSAProviderCompleteAsyncCall, WSAProviderConfigChange, WSAUnadvertiseProvider, WSAInstallServiceClassA/W, WSARemoveServiceClass, WSAGetServiceClassInfoA/W, WSAGetServiceClassNameByClassIdA/W, WSASetServiceA/W, WSAEnumNameSpaceProviders(Ex)A/W, WSAGetQOSByName, WSApSetPostRoutine.
- **L99–L138** — Winsock catalog/provider install API (`WSC*` run), WSCDeinstallProvider through WSCWriteProviderOrderEx: provider install/update/remove (WSCInstallProvider, WSCInstallProviderEx, WSCInstallProvider64_32, WSCInstallProviderAndChains64_32, WSCUpdateProvider(32/Ex), WSCDeinstallProvider(32/Ex)), namespace management (WSCInstallNameSpace(32/Ex/Ex2/Ex32), WSCUnInstallNameSpace(32/Ex2), WSCEnableNSProvider(32), WSCEnumNameSpaceProviders32, WSCEnumNameSpaceProvidersEx32), protocol enumeration (WSCEnumProtocols(32/Ex)), provider info/path (WSCGetProviderInfo(32), WSCGetProviderPath(32), WSCSetProviderInfo(32)), application categories (WSCGetApplicationCategory(Ex), WSCSetApplicationCategory(Ex)), and ordering (WSCWriteNameSpaceOrder(32), WSCWriteProviderOrder(32/Ex)). The `32` suffixes are the WOW64 variants.
- **L139–L162** — Winsock helper API (`Wah*` run): WahCloseApcHelper, WahCloseHandleHelper, WahCloseNotificationHandleHelper, WahCloseSocketHandle, WahCloseThread, WahCompleteRequest, WahCreateHandleContextTable, WahCreateNotificationHandle, WahCreateSocketHandle, WahDestroyHandleContextTable, WahDisableNonIFSHandleSupport, WahEnableNonIFSHandleSupport, WahEnumerateHandleContexts, WahInsertHandleContext, WahNotifyAllProcesses, WahOpenApcHelper, WahOpenCurrentThread, WahOpenHandleHelper, WahOpenNotificationHandleHelper, WahQueueUserApc, WahReferenceContextByHandle, WahRemoveHandleContext, WahWaitForNotification, WahWriteLSPEvent (internal helper surface used by mswsock/LSPs).
- **L163** — __WSAFDIsSet: the helper backing the `FD_ISSET` macro.
- **L164–L198** — Classic lowercase BSD sockets API: accept, bind, closesocket, connect, freeaddrinfo, getaddrinfo, gethostbyaddr, gethostbyname, gethostname, getnameinfo, getpeername, getprotobyname, getprotobynumber, getservbyname, getservbyport, getsockname, getsockopt, htonl, htons, inet_addr, inet_ntoa, inet_ntop, inet_pton, ioctlsocket, listen, ntohl, ntohs, recv, recvfrom, select, send, sendto, setsockopt, shutdown, socket.

# Documentation files

## `README.md`

The project front page for ModernCC (`mcc`): a small, fast, one-pass C11 compile-and-link compiler derived from TinyCC, targeting 5 architectures × 3 object formats (ELF, PE/COFF, Mach-O) from one source tree, with in-memory execution (`-run`) and an embeddable `libmcc` library. It leads with feature and comparison tables (vs gcc/clang/mingw/msvc), then covers building with CMake presets (including current all-green Linux and Windows status paragraphs), the binary/library naming convention, usage examples, the CTest suite organization with per-toolchain coverage, compile-speed benchmarks, the qemu cross-target × libc conformance matrix, CI labels, and licensing (382 lines).

- **ModernCC (`mcc`)** (title section) — Feature summary tables: targets, formats, libcs, modes, speed, size, assembler, safety options, and cross-compilation.
- **Comparisons** — Support matrices comparing mcc against gcc, clang, mingw, and msvc by target/format and by capability, plus toolchain notes and the libc coverage table.
- **Building / Configuration** — CMake preset workflow (`cmake --preset debug` etc.), the dated Linux and Windows all-preset status paragraphs, the option table (`MCC_BUILD_TESTS` through `MCC_QEMU_TESTS`), and the `mcc[-<arch>][-static|-dynamic][-musl]` naming convention for binaries and libraries.
- **Usage** — Command-line examples for compile+link, `-run`, `-c`, `-S`, and cross-compiling against a sysroot under qemu-user.
- **Testing** — The CTest suite layout by directory (`tests/exec`, `preprocess`, `diff`, `diff3`, `cli`, `embed`, `diagnostics`, `asm`, `behavior`, `qemu`), a per-toolchain pass/skip coverage matrix with ten explanatory footnotes, and the compile-speed and footprint benchmark table.
- **Cross-target × libc matrix (qemu-user)** — How `MCC_QEMU_TESTS=ON` fetches Gentoo stage3 rootfs images and runs the conformance suite for every arch × glibc/musl combination, plus the Docker runner for off-Linux hosts and the CI label scheme (`qemu`/`wine`/`macho`) with the Mach-O-by-host explanation.
- **Repository layout** — One-line map of the top-level directories (src, include, runtime, examples, tests, tools).
- **License** — TinyCC derivation notice and LGPL-2.1 licensing.

## `docs/BUILD.md`

The complete catalog of every CMake cache value that influences an mcc build — type, default, allowed values, and the "relevance gate" deciding when each matters — intended to drive CI and choose worthwhile build combinations (451 lines). Its source of truth is the `mcc_config_node()` declarations in CMakeLists.txt. The preamble explains the three executable shapes (`mcc`, `mcc-static`, `mcc-dynamic`), the cross-compiler target table (`mcc-i386` through `mcc-arm64-osx`), and the suffix convention, before fourteen numbered sections.

- **1. Standard CMake values** — Non-mcc-specific cache values that still change the build (CMAKE_BUILD_TYPE, CMAKE_C_COMPILER, CMAKE_CROSSCOMPILING_EMULATOR, CMAKE_TOOLCHAIN_FILE, install prefix, generator).
- **2. CMakePresets.json — ready-made configurations** — The full preset catalog and naming conventions: developer presets (debug/release/asan/diagnostics/cross/matrix), CI presets (linux-*, macos, msvc, mingw), qemu presets, and dist presets, plus artifact-upload mechanics.
- **3. Build-target knobs** — The `GROUP "Build targets"` options: toolchain profile, cross compilers, static/dynamic lib and exe switches, musl variants, strip, ONE_SOURCE, tests, libmcc1 compiler choice.
- **4. Diagnostics / instrumentation** — MCC_DIAGNOSTICS, MCC_BUILD_SANITIZE, MCC_BUILD_PROFILE, MCC_BUILD_COVERAGE and their GCC/Clang-host requirements.
- **5. mcc feature toggles** — The `CONFIG_*`-baking options: mingw32 target, backtrace, bounds checker, integrated assembler, predefs, PIE/PIC, SELinux, DWARF version, Darwin Mach-O/codesign knobs.
- **6. Runtime path overrides** — STRING values mirroring `configure --…` flags (sysroot, triplet, include/lib/crt paths, ELF interpreter, install dir).
- **7. Extra build flags** — MCC_EXTRA_CFLAGS / MCC_EXTRA_LDFLAGS / MCC_EXTRA_LIBS passthroughs.
- **8. ARM ABI** — Autodetected ARM ABI overrides (EABI, VFP, hard-float, IDIV, CPU version), gated on `MCC_CPU==arm`.
- **9. Superbuild / matrix orchestration** — Values controlling the multi-toolchain/multi-target superbuild (MCC_TARGETS, MCC_SUPERBUILD_*, MSVC generator/platform, per-toolchain compiler overrides).
- **10. Self-contained toolchain downloads** — Pinned-URL/SHA256 values for the `mingw-toolchain` and `clang-toolchain` fetch targets.
- **11. Test-suite knobs** — Test-only values: reference compiler, gcc torture-suite path, Darwin host flag, and the qemu matrix controls (mirror, archs, libcs, download dir).
- **12. Derived / status values** — Read-only computed values (MCC_CPU, MCC_TARGETOS, MCC_CC_NAME, emulator, arm_abi) to assert in CI, never set.
- **13. Cross-value validation** — The configure-time checks `mcc_validate_config()` enforces, listing each fatal/warn/autocorrect combination.
- **14. Suggested test matrix** — A practical numbered list of build combinations covering the meaningful axes per host, instrumentation, ELF toggles, cross/multi-toolchain, and the qemu runtime matrix.

## `docs/C9911.md`

A 6,709-line clause-by-clause conformance "source of truth" for C99 (N1256) and C11 (N1570): every normative requirement is rewritten as a paraphrased checkbox with a precise `§clause·paragraph` citation and a three-way status tag (`mcc:`/`gcc:`/`clang:` with `✓`/`✗`/`~`), totaling 3,837 requirement checkboxes of which 2,867 are confirmed `mcc:✓`. Sections follow the standard's own numbering (§4–§5 environment, §6 language, §7 library, annexes); update notes record the 2026-06-30 re-verification crawl and the 2026-07-02 `-S` landing that retagged previously untestable rows.

- **Legend** — Defines the checkbox format, the per-compiler status symbols (`✓`/`✗`/`~`), C99/C11 lineage tags, and the "spec; not separately testable" marker.
- **How it is organized** — Explains that sections follow the standard's numbering and notes the C99→C11 clause-7 renumbering.
- **Table of contents** — Linked index of every section below.
- **§4 Conformance & §5 Environment** — Requirements on conformance, translation phases, diagnostics, hosted/freestanding startup, program execution, C11 memory model, character sets, and translation/numerical limits, plus added cross-check subsections (predefined macros, freestanding headers, ATOMIC init).
- **§6.2.1-§6.2.4 Scopes, linkages, name spaces, storage durations** — Identifier scope/linkage/namespace and storage-duration rules.
- **§6.2.5-§6.2.8 Types, representations, compatible/composite types, alignment** — Type taxonomy, object representation, compatibility, and C11 alignment requirements.
- **§6.3 Conversions** — Arithmetic and other operand conversions (integer promotions, usual arithmetic conversions, pointer conversions).
- **§6.4 Lexical elements** — Keywords, identifiers, universal character names, constants, string literals, punctuators, comments.
- **§6.5.1-§6.5.4 Primary/postfix/unary expressions** — Expression rules from primary expressions through casts.
- **§6.5.5-§6.5.14 Binary operators** — Multiplicative through logical-OR operator requirements.
- **§6.5.15-§6.5.17 Conditional/assignment/comma & §6.6 Constant expressions** — Ternary, assignment, comma, and constant-expression rules.
- **§6.7.1-§6.7.3 Declarations: storage/type specifiers/qualifiers** — Storage-class specifiers, type specifiers, and qualifiers.
- **§6.7.2.1-§6.7.2.4 Struct/union/enum/atomic specifiers** — Aggregate, enumeration, and C11 atomic type-specifier rules.
- **§6.7.4-§6.7.5 Function specifiers & alignment specifier** — `inline`, `_Noreturn`, and `_Alignas`.
- **§6.7.6 Declarators** — Pointer, array, and function declarator rules.
- **§6.7.7-§6.7.9 Type names/typedef/initialization** — Type names, typedef, and initializer semantics (including designated initializers).
- **§6.8 Statements and blocks** — Labeled, compound, selection, iteration, and jump statement requirements.
- **§6.9 External definitions** — Translation-unit, function-definition, and tentative-definition rules.
- **§6.10.1-§6.10.3 Conditional inclusion, source inclusion, macro replacement** — Preprocessor `#if`/`#include`/macro-expansion requirements.
- **§6.10.4-§6.10.9 Line/error/pragma/null/predefined macros/_Pragma** — The remaining preprocessor directives and predefined macros.
- **§7.1-§7.5 Library intro, assert, complex, ctype, errno** — Library conventions and the first four headers.
- **§7.6-§7.8 fenv, float, inttypes** — Floating-environment, floating-limit, and integer-format headers.
- **§7.9-§7.11 iso646, limits, locale** — Alternative spellings, integer limits, and locale support.
- **§7.12 `<math.h>`** — The full math-library requirement set.
- **§7.13-§7.16 setjmp, signal, stdalign, stdarg** — Non-local jumps, signals, alignment macros, and varargs.
- **§7.16-§7.18 stdarg/stdbool/stdint-or-atomic boundary** — The C99/C11 renumbering boundary covering stdbool and stdatomic.
- **`<stddef.h>` & `<stdint.h>`** — Common definitions and exact-width integer types.
- **§7.21 `<stdio.h>`** — Stream I/O, formatted I/O, and file-operation requirements.
- **§7.22 `<stdlib.h>`** — Conversions, random numbers, memory management, environment, searching/sorting, and multibyte utilities.
- **§7.23 `<string.h>` & §7.24 `<tgmath.h>`** — String handling and type-generic math.
- **§7.25-§7.27 threads(C11)/time** — C11 threads and time utilities.
- **§7.28-§7.31 uchar(C11)/wchar/wctype** — Unicode, wide-character, and wide-classification headers plus future library directions.
- **Annexes F, G, K (normative)** — IEC 60559 floating-point, complex arithmetic, and bounds-checking-interface requirements.
- **Annexes C, D, E, H, I, J (sequence points, UCN ranges, limits, portability)** — The cataloging annexes: sequence points, UCN identifier ranges, implementation limits, and portability issues.

## `docs/TODO.md`

The live work tracker (718 lines) with a `[ ]`/`[~]`/`[x]` legend, currently holding five dated top-level sections: the merged platform specification that replaced the deleted HOST.md/MAC.md/WIN.md reference docs, the verification sweep that validated those docs against the tree, a landed-feature record for `-S`, and two C9911-conformance work logs (a coverage sweep into the differential test and a full re-verification crawl of every flagged divergence).

- **Platform spec — merged from HOST.md / MAC.md / WIN.md — 2026-07-03** — The three platform-gating reference docs rewritten as verified "holds" invariants plus open `[ ]` items, in three subsections: Host axis, Darwin/Mach-O target, and Windows/PE target.
- **BUILD.md / HOST.md / MAC.md / WIN.md verification sweep — 2026-07-03** — Records the verification of every substantive statement in the four docs against the tree, with residual "Code gaps" (since landed) and "Doc corrections — BUILD.md" subsections.
- **✓ LANDED: `-S` (assembly output) — 2026-07-02** — Documents the implemented `mcc -S` AT&T assembly listing (compile then disassemble the populated sections), the files it touched, and the C9911 rows it made testable.
- **C9911 → full_language.c coverage sweep — 2026-07-01** — Log of driving every runtime-observable C9911 requirement into `tests/diff/full_language.c` (69 3-way-validated functions, ~570 clauses), with "Landed this round" and "Resolved as NOT actionable" subsections.
- **C9911 re-verification crawl — 2026-06-30** — Empirical re-check of all 169 flagged `mcc:✗`/`mcc:~` rows against the live binary with a verdict tally (34 FIXED, 11 CHANGED, 118 still-diverging, 6 not-testable), followed by FIXED/CHANGED lists, an empty "Open items" section (all actionable gaps landed), and the "Classified — NOT actionable" rationale list.

## `tests/diff/parts/README.md`

A short (33-line) explainer for the `tests/diff/parts/` directory: each `*.h` is a main-free, `#include`-free C11 test unit that serves double duty — compiled standalone by a `run_<slug>.c` wrapper against real system headers for the 3-way gcc/clang/mcc byte-identical-stdout `parts-suite`, and aggregated into `../full_language.c`'s minimal `mcclib.h` environment for the `mcctest` differential gate. It notes the `s7_28` wchar unit is parts-suite-only because real `<wchar.h>` clashes with the aggregate's opaque `FILE`.

- **tests/diff/parts** (title section) — Explains the dual-role design: header-free units compiled both as isolated 3-way differential tests and as one aggregated all-in-one C11 test, proving feature coherence across environments.
- **Kinds of unit** — Distinguishes dual-use C9911 units (`s04.h`, `s6_*.h`, `s7_*.h`, `coherency.h`) from aggregate-only ordered `legacy_*.h` chunks of the historical tcctest body, which shrank full_language.c from ~7100 to under 400 lines.

## `tests/qemu/apple-libc/PROVENANCE.md`

Provenance documentation (103 lines) for the vendored, verbatim Apple open-source libc sources used by the `macho-apple-libc` test: real Apple `Libc` and `libplatform` code is compiled by mcc for x86_64 Darwin, linked into Mach-O images, and executed on a Linux host via a loader/trampoline, with no function bodies modified — only a tiny `shim-include/` compat layer replaces the macOS SDK. It also documents, with empirical evidence, which parts of libSystem are kernel-fused and genuinely require macOS or darling.

- **Vendored Apple libc sources (real-target-libc Mach-O testing)** (title section) — States that these are verbatim copies of Apple's libSystem source code, how they are compiled and run, and that `shim-include/` is only build glue.
- **`src/` — Apple `Libc`, `string/FreeBSD/`** — Table mapping the six vendored FreeBSD string files (strcspn, strpbrk, strsep, memmem, strchrnul, strnstr) to their upstream `apple-oss-distributions/Libc` paths.
- **`src-libplatform/` — Apple `libplatform`, `src/string/generic/`** — The core string/memory routines (strlen, memcpy, memset, etc.), explaining that Apple's portable C implementations are the functional equivalents of the commpage-selected assembly and thus legitimately run off-Darwin.
- **`src-simple/` — Apple `libplatform`, `src/simple/string_io.c`** — Apple's self-contained `_simple_vsnprintf` formatted-output engine, why it has no FILE/locale/malloc dependency, and how its unused Mach-VM grow-path symbols are stubbed.
- **`shim-include/` compat layer (no source bodies touched)** — Enumerates the freestanding compat definitions (`__FBSDID`, `LONG_BIT 64`, `__weak_reference` via asm `.set`, `_PLATFORM_OPTIMIZED_* 0`) that let the sources build without a macOS SDK.
- **The remaining boundary (genuinely needs macOS/darling)** — Documents with evidence why libmalloc, FILE/locale stdio, dyld, pthread/GCD, and the ObjC runtime are structurally fused to the Darwin kernel and are exercised only where the platform is available.
