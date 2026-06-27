# Autotools/Make → CMake: removal & modernization plan

A staged, verification-gated plan to (A) delete every autotools/`configure`/
hand-written-`Make` file now that `CMakeLists.txt` is the build, and (B) fold the
generated files/targets into modern, cross-platform-clean CMake idioms.

Status: **proposal**. Nothing here is executed yet. Each phase has an explicit
"done when" gate (`cmake -S . -B b && cmake --build b && ctest`, plus the
`gcc;clang × native;cross` matrix from TODO.md) so the build never breaks.

---

## Part 0 — Inventory & verdict

| File | Size | What it is | Verdict |
|---|---|---|---|
| `configure` | 21 KB | Hand-written POSIX-sh config script | **Delete** — fully ported to CMake options + detection |
| `Makefile` | 18 KB | Top build (tcc/libtcc/cross/docs/install) | **Delete** — ported (section 16 catalogs the few opt-out divergences) |
| `lib/Makefile` | 2.8 KB | libtcc1 runtime build | **Delete** — ported (`tcc_build_libtcc1`) |
| `tests/Makefile` | 10 KB | Test suite driver | **Delete** — ported (section 14 ctest) |
| `tests/tests2/Makefile` | 6.7 KB | tests2 runner | **Delete** — ported |
| `tests/pp/Makefile` | 1.3 KB | preprocessor tests | **Delete** — ported |
| `conftest.c` | 8.1 KB | **Dual**: configure runtime probe **+** the `c2str` codegen (`-DC2STR`) | **Keep the C2STR half** — extract to `c2str.c`; drop the probe half |
| `texi2pod.pl` | 11 KB | texinfo→POD→man for the doc build | **Keep** (used by the CMake doc build); perl-only, cross-platform |
| `win32/build-tcc.bat` | — | No-CMake native Windows bootstrap | **Phase 5 (optional)** — CMake+mingw / MSVC generator replaces it |
| `tests/test-win32.bat` | — | Windows test runner | **Phase 5 (optional)** |
| `config.mak` (generated) | — | Make-only; **no C source includes it** | Gone with the Makefiles |
| `config.h`, `tccdefs_.h`, `config.texi`, `c2str`, `libtcc.def` (generated) | — | Build outputs (gitignored) | Stay in the **build dir**; CMake regenerates |

Key fact: **the only non-deletable C file here is the `c2str` generator** inside
`conftest.c`. `CMakeLists.txt` already builds it (`add_executable(c2str conftest.c)`
and the cross-compile host-build path) to convert `include/tccdefs.h` → the
compiled-in `tccdefs_.h`. Everything else is pure build-system scaffolding.

---

## Part 1 — Remove autotools/configure (phased)

### Phase 1 — Pre-flight (no deletions)
1. **Confirm CMake parity** on a clean tree: `cmake -S . -B b -G Ninja &&
   cmake --build b && (cd b && ctest)` → 137/137; then the matrix
   `-DTCC_TOOLCHAIN_PROFILE="gcc;clang" -DTCC_TARGETS="native;cross"`.
2. **Re-home the few Makefile-only divergences** (section 16) so nothing is lost:
   - `<target>-libtcc1-usegcc` (host-cc-built libtcc1, faster bcheck objs) →
     add option `TCC_LIBTCC1_USEGCC` (build libtcc1 with `CMAKE_C_COMPILER`
     + `-fPIC -gdwarf` instead of the freshly-built tcc).
   - Fragile/iterated dev tests (test2/test3, memtest, dlltest, btest, cross-test,
     asmtest/weaktest/speedtest) → either port behind `TCC_BUILD_FRAGILE_TESTS`
     or explicitly document as dropped.
3. **Decide on the source-of-truth comments.** `CMakeLists.txt` cites
   `configure ~617`, `Makefile ~99-123`, `lib/Makefile`, etc. After deletion
   these become historical. Keep them (they explain *why*), but add a one-line
   header note: "line refs are to the pre-CMake autotools build, kept in git
   history at tag `pre-cmake`." Tag the last commit that still has them.

### Phase 2 — Extract the c2str generator from conftest.c
`conftest.c` mixes the configure probe (dead) with the `#if C2STR` codegen (live).
1. Create `c2str.c` containing **only** the `#if C2STR … #endif` body (drop the
   `#if C2STR` guard; it's now the whole file).
2. Repoint the two `CMakeLists.txt` sites (`add_executable(c2str conftest.c)`
   and the host-compile `execute_process(... conftest.c ...)`) at `c2str.c`, and
   drop `-DC2STR` (or keep it harmless).
3. Update `win32/build-tcc.bat`'s `conftest.c` reference (if Phase 5 keeps it).
4. Fix the `include/tccdefs.h` comment that mentions `conftest`.
- **Done when**: `tccdefs_.h` still regenerates and `CONFIG_TCC_PREDEFS=1` builds
  pass; `-DTCC_CONFIG_PREDEFS=OFF` (runtime tccdefs) also builds.

### Phase 3 — Delete the scaffolding
`git rm configure Makefile lib/Makefile tests/Makefile tests/tests2/Makefile
tests/pp/Makefile conftest.c` (conftest.c only after Phase 2 lands c2str.c).
- **Done when**: clean `cmake` configure+build+ctest + matrix all green, with
  **zero** references to the deleted files remaining
  (`grep -rIE 'config\.mak|/configure\b|conftest\.c'` is clean outside git
  history).

### Phase 4 — Prune `.gitignore` & docs
1. `.gitignore`: drop the now-obsolete Make-era patterns — `config*.mak`,
   `conftest*`, `c2str`, `*_.h`, `tcc_g`, `config*.h` (build is out-of-source;
   these never appear in the source tree). Keep editor/OS noise + `*.o/*.a` for
   in-tree safety.
2. **README**: replace the `./configure && make && make test && make install`
   block with the CMake quickstart (configure/build/test/install + the matrix
   and presets). This is the single most user-visible change.

### Phase 5 — Windows native scripts (optional, separate decision)
`win32/build-tcc.bat` + `tests/test-win32.bat` are a no-CMake Windows path.
CMake already targets Windows (MSVC generator, or mingw via `TCC_CONFIG_MINGW32`
+ a toolchain file). **Recommend removing** once a Windows CI lane proves the
CMake path, but keep them until then — they are the current zero-dependency
Windows bootstrap. Track as its own task.

---

## Part 2 — Fold/optimize generated files & targets

Goal: one generator per artifact, all output under the **build dir**, no tracked
generated files, clean under cross-compile.

### 2.1 `config.h` — keep the computed `file(WRITE)`
It emits a macro **only when it differs from the tcc.h default**, which a static
`config.h.in` template can't express cleanly. Keep the inline generator; it is
already the single source. (Optionally split the long heredoc into a
`tcc_emit_config_h()` function for readability — cosmetic.)

### 2.2 `tccdefs_.h` + `c2str` — fold into a host-tools sub-build
The hard cross-compile problem: `c2str` must run on the **build** host even when
the main build is cross-targeted. Today that's `find_program(TCC_HOSTCC)` +
`execute_process`. The modern, robust pattern (and we already have the superbuild
machinery from TODO.md):
- Add a tiny **`host-tools` ExternalProject**, force-configured with the host
  toolchain (no cross toolchain file), that builds just `c2str` and exports its
  path. The main build consumes that path to generate `tccdefs_.h`.
- Native builds skip the sub-build (host == target) and use the in-tree target.
- **Win**: cross-compiling no longer needs `TCC_HOSTCC` plumbing; it's the
  canonical CMake answer to "codegen tool that must run on the build machine."

### 2.3 `config.texi`, `libtcc.def`
- `config.texi` (one `@set VERSION` line) → keep `file(WRITE)`; it is trivial.
- `libtcc.def` → keep (generated by the freshly-built tcc for the Windows DLL).
- `config-nodes.md` (node-table render) already writes to the build dir only.

### 2.4 Target hygiene
- Keep `tcc_build_libtcc1()` as the single libtcc1 factory (native + each cross).
- Express libtcc's public headers with interface generator expressions:
  `target_include_directories(libtcc PUBLIC $<BUILD_INTERFACE:${SRC}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)` so build-tree and installed
  consumers both resolve `libtcc.h`.

---

## Part 3 — Modern cross-platform standards to adopt

These turn "works on my box" into "works/CI-tested everywhere".

1. **`CMakePresets.json`** (CMake 3.23+). Ship configure/build/test presets:
   `default`, `debug`, `release`, `cross-matrix`, `mingw`, `asan`. This replaces
   "remember the right `-D` flags" and is the portable, CI-native entry point —
   the biggest "functions well across all platforms" lever. Document in README.
2. **Installable package config** so downstream code can
   `find_package(tcc) ; target_link_libraries(app PRIVATE tcc::libtcc)`:
   - `install(TARGETS libtcc EXPORT tccTargets ...)`,
   - `install(EXPORT tccTargets NAMESPACE tcc:: ...)`,
   - `configure_package_config_file` + `write_basic_package_version_file` →
     `tccConfig.cmake` + `tccConfigVersion.cmake`.
   Today libtcc installs the lib + header but no package config — adding it makes
   libtcc a first-class CMake dependency on every platform.
3. **`CPack`** (TGZ/ZIP source + DEB/RPM/NSIS/productbuild binary) to replace
   `make tar`/the `dist` target with one cross-platform `cpack` invocation.
4. **Real cross via toolchain files** + `CMAKE_CROSSCOMPILING_EMULATOR`
   (qemu/wine) so the `TCC_TARGETS` matrix can build *and test* foreign-OS tcc
   binaries, closing the "foreign binary can't run on host" gap (TODO.md).
5. **`set(CMAKE_EXPORT_COMPILE_COMMANDS ON)`** for IDE/clangd/static-analysis
   across platforms.
6. **Language/standard hygiene**: `target_compile_features`/`CMAKE_C_STANDARD 11`
   where applicable; keep all flag tuning behind `check_c_compiler_flag` (already
   done for `-Wdeclaration-after-statement` etc.).
7. **CI matrix** (GitHub Actions): linux gcc+clang, macOS, Windows
   (MSVC + mingw), driven by the presets + the superbuild matrix; upload CPack
   artifacts. This is what actually *guarantees* cross-platform.

---

## Part 4 — Risks, parity gaps, rollback

- **libtcc1 `usegcc` variant** and the **fragile dev tests** are the only real
  Makefile-only behaviors; Phase 1.2 re-homes or explicitly drops them. Don't
  delete the Makefiles until that decision is recorded.
- **Perl dependency** for the man/info docs (`texi2pod.pl` + `makeinfo`). The doc
  build is already opt-in (`TCC_BUILD_DOC=OFF`); document the perl/makeinfo
  prerequisite, or (stretch) port `texi2pod.pl` to a CMake `-P` script.
- **Source-of-truth comments** go stale once the Makefiles are gone — mitigate by
  tagging `pre-cmake` and adding the header note (Phase 1.3).
- **Rollback**: each phase is a separate commit; `git revert` restores the
  scaffolding. Keep the `pre-cmake` tag as the canonical reference point.

---

## Part 5 — Sequenced checklist

- [ ] 1.1 Parity run (native + matrix) green on a clean tree.
- [ ] 1.2 `TCC_LIBTCC1_USEGCC` option; decide fragile-tests (port or document).
- [ ] 1.3 Tag `pre-cmake`; add the "historical line refs" header note.
- [ ] 2.x Extract `c2str.c` from `conftest.c`; repoint CMake + bat + tccdefs.h.
- [ ] 3.x `git rm` configure + all Makefiles + conftest.c; verify zero refs.
- [ ] 4.x Prune `.gitignore`; rewrite README to the CMake quickstart.
- [ ] 6.1 Host-tools ExternalProject for `c2str` (drop `TCC_HOSTCC` plumbing).
- [ ] 6.2 `CMakePresets.json` (+ README).
- [ ] 6.3 `install(EXPORT)` + `tccConfig.cmake` (find_package(tcc)).
- [ ] 6.4 `CPack` config; retire the `dist` target.
- [ ] 6.5 `$<BUILD_INTERFACE>`/`$<INSTALL_INTERFACE>` on libtcc; `EXPORT_COMPILE_COMMANDS`.
- [ ] 6.6 CI matrix (linux/macos/windows × gcc/clang/msvc/mingw) on presets.
- [ ] 5.x (optional) Remove `win32/*.bat` once Windows CI proves the CMake path.
