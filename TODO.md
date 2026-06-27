# TODO — Hierarchical configuration graph

Status: **core implemented** (all inline in `CMakeLists.txt`, section `1z`
framework + section `2z` finalize). This file now records the as-built design,
the corrections a deep analysis forced on the original proposal, and the
remaining work.

The goal: model tcc's build configuration as a graph of nodes with
parent→child *reveal*, parent→child *derive*, and sibling *constrain* edges;
collapse low-level knobs under high-level **profiles**; and drive the `ccmake`/
`cmake-gui` menus off that graph — showing/hiding child options based on the
parent values that make them relevant. The knob ↔ preprocessor-flag catalog
lives in `CMakeLists.txt` section 3a; the node declarations (section 1z) are the
source of truth for the option table (render it with `-DTCC_REGEN_DOCS=ON`,
which writes `config-nodes.md` into the build dir).

## Design as built

Everything is inline in `CMakeLists.txt` (no `cmake/*.cmake` modules — the file
is single-source by project policy). A thin metadata layer wraps the ordinary
`option()`/`set(CACHE)` declarations; the cache entries are byte-identical to
before, so **every `-DTCC_*` keeps working unchanged** (verified).

- `tcc_config_node()` — declares a knob and registers its group / dropdown
  choices / relevance condition / default. Uses `cmake_parse_arguments(PARSE_ARGV)`
  so help strings with `;` survive.
- `tcc_apply_node_visibility()` — hides knobs whose `VISIBLE_WHEN` is false for
  the resolved target by flipping the cache **TYPE to INTERNAL**. This is
  **value-preserving and cosmetic** (verified): an explicit `-D` is never
  overridden, and relevance is still enforced by the consuming `if()` branch.
  This is the show/hide-by-parent automation.
- `tcc_validate_config()` — `STRINGS` does **not** enforce, so every enumerated
  node is range-checked (FATAL), plus the cross-cutting invalid-combination
  rules (FATAL / WARNING). Never mutates a value (strict back-compat).
- `tcc_report_config()` — STATUS summary of derived facts + every *visible* knob.
- Profiles: `TCC_TOOLCHAIN_PROFILE` (`auto|gcc|clang|tcc|msvc`) seeds safe
  feature defaults via `tcc_profile_seed`, which sets a node's DEFAULT only
  (a non-FORCE `option()`/`set(CACHE)`), so an explicit `-D`, a config-extra
  value, or a prior cache value ALWAYS wins — the "individual overrides"
  contract. (Trade-off: an in-place profile switch does not auto-revert a
  previously seeded knob; moot for the superbuild, where each child is a fresh
  dir.) A profile≠detected-compiler emits a warning.
- **Multi-toolchain × multi-target superbuild matrix**: `TCC_TOOLCHAIN_PROFILE`
  (toolchains) and `TCC_TARGETS` (cross-platform targets) are both ORDERED
  LISTS forming a build matrix. A single `native` cell with one `auto`/named
  toolchain = ordinary in-tree build (back-compat). Anything larger turns the
  top-level build into an orchestrator: one `ExternalProject` child per
  (toolchain, target) cell in `<build>/<tc>-<tg>`, each a fresh single-cell
  build that pins its own `CMAKE_C_COMPILER` + target args, then `return()`.
  - Targets within a toolchain build **simultaneously** (no inter-target
    `DEPENDS`); toolchain stages run **in order** by default (each stage waits
    for the whole previous stage). `TCC_SUPERBUILD_SEQUENTIAL=OFF` makes the
    entire matrix parallel.
  - Toolchain → compiler via `tcc_map_toolchain_cc` (override `-DTCC_CC_<name>`).
    Target → configure args via `tcc_map_target_args`: `native` (host),
    `cross` (`-DTCC_ENABLE_CROSS=ON`, host-runnable cross compilers), or a
    user `-DTCC_TARGET_ARGS_<name>="-Dk=v;..."`. Unknown target / missing
    compiler = FATAL.
  - `TCC_SUPERBUILD_TEST` (default ON) runs `ctest` per cell; user `-DTCC_*` are
    forwarded to every child.
  - Verified: `gcc;clang` × `native;cross` = 4 cells, gcc stage before clang
    stage, native+cross parallel within each, 137/137 tests every cell.
  - Fixed a pre-existing cross bug surfaced by `cross`: `arm-wince` libtcc1 was
    fed the x86 `win32/lib/chkstk.S` (`lib/Makefile` omits `chkstk.o` for
    `arm-wince`); chkstk is now i386/x86_64/arm64-win32 only.
- `tcc_generate_node_doc()` — renders the node registry as a Markdown table into
  the build dir (`config-nodes.md`), gated on `-DTCC_REGEN_DOCS=ON`; a convenience
  view, not a tracked file (the declarations are the source of truth).

## Corrections the analysis forced (vs the original proposal)

1. **`TCC_CPU`/`TCC_TARGETOS` are derived, not editable** — they are STATUS
   nodes. The target axis is chosen via the host / toolchain file /
   `TCC_CONFIG_MINGW32` (the *input* that forces WIN32) / `TCC_ENABLE_CROSS`.
2. **ARM ABI knobs are autodetected internals** (`_arm_eabi/_vfp/_hardfloat`,
   `TCC_CPUVER`), not options — reported via STATUS only.
3. **Profiles must not set `CMAKE_C_COMPILER` or `CC_NAME`** — compiler is
   locked before `project()`; `CC_NAME` stays derived and is consumed by tests.
4. **Back-compat is strict** — hiding is visibility-only (no value-forcing
   `cmake_dependent_option`); invalid combos are *validated*, not auto-mutated.
   `BCHECK`-without-`BACKTRACE` warns (does not flip the value).
5. **Deleted false edges** — `tests→coverage` (coverage is independent);
   `AUTO_TCCDIR↔TCCDIR` "coherence" (it is a fallback, not a constraint).
6. **`NEW_DTAGS` split from `DISABLE_RPATH`** — DTAGS is always ELF-relevant;
   only `DISABLE_RPATH` is shared-libtcc-gated.
7. **Dropped `PTR_SIZE/LONG_SIZE/LDOUBLE_SIZE` derived nodes** — CMake never
   computes them; ARM `LDOUBLE_SIZE` isn't a pure function of `TCC_CPU`.
8. **Diagnostic trio kept independent** — `TCC_BUILD_PROFILE/COVERAGE/SANITIZE`
   stay three orthogonal options (no `release|asan|…` enum, no subsuming
   `CMAKE_BUILD_TYPE`); validated against the host compiler instead.
9. **Tri-state STRINGs preserved** — `NEW_MACHO`/`CODESIGN` stay `''|yes|no|auto`.
10. **PIE→PIC implication left to the C source** (tcc.h) — no CMake auto-mutation.
11. **Min CMake bumped to 3.22** — for `cmake_language(EVAL)` + policy-clean
    grouped conditions (CMP0127).

## Validation rules wired (section 2z)

- FATAL (real breakage only): non-numeric `SEMLOCK` (breaks the C compile);
  `TCC_BUILD_SANITIZE`/`PROFILE` on a non-GCC/Clang host (`COVERAGE` excluded —
  it is compiled by the built tcc, not the host cc); `TCC_BUILD_PROFILE` on
  Darwin; unknown `TCC_TOOLCHAIN_PROFILE`/`TCC_TARGETS` entry; missing toolchain
  compiler.
- WARN (back-compat: never breaks a previously-valid value): unknown enum value
  (LIBC/DWARF/NEW_MACHO/CODESIGN — was silently accepted before); `BCHECK` w/o
  `BACKTRACE`; `DISABLE_RPATH` while static; `PREDEFS=OFF`; `LIBC=uClibc`;
  ELF-only knobs (incl. LIBC) on PE/Mach-O; Darwin knobs on non-Darwin;
  profile≠detected compiler.

Enum violations are WARN, not FATAL, because the old free-form `set(CACHE)`
accepted any value and the config.h emit still handles it — a hard error would
regress back-compat. `STRINGS` only seeds the GUI dropdown; it never enforces.

## Remaining / future work

- [ ] **Make ARM ABI overridable** (optional): rewrite the `cc -dM` detection to
      seed real cache vars + rewire the config.h emitter, with autodetect as the
      default and explicit override precedence. Currently STATUS-only.
- [ ] **Toolchain-file whole-build cross** (`CMAKE_CROSSCOMPILING`): today the
      build runs the target `tcc` on the host (libtcc1/tests/impdef). Add a
      `CMAKE_CROSSCOMPILING_EMULATOR`/wine path + validation, or FATAL clearly.
- [ ] **Grow the profile seed tables** honestly — only defaults the build does
      not already set, each marked as a real addition (not "matching current").
- [ ] **Auto-correct option** (behind a flag) for `BCHECK`-without-`BACKTRACE`
      and non-host-runnable `TCC_BUILD_TESTS`, if a non-strict mode is wanted.
- [x] **Port the remaining knob clusters into the graph** — runtime paths,
      build flags, `TCC_INSTALL_TCCDIR`, and the diagnostic trio are now ADVANCED
      nodes; the report + rendered node table cover all 36 user knobs.
      (`TCC_HOSTCC` stays a `find_program` result, surfaced via STATUS.)
- [x] **Fold the docs into CMakeLists.txt** — the preprocessor-flag catalog
      (former `docs/feature-matrix.md`) now lives in section 3a (incl. internal/
      host/dev macros in §9); the node table is rendered on demand into the build
      dir. The `docs/` folder is removed.

## Open questions

- `ccmake` only refreshes reveal/hide on re-configure (`c`) — one keystroke
  behind. Documented, acceptable.
- Profile seeding is now DEFAULT-only (non-FORCE), so `-D`/config-extra/prior
  values always win and there is no override-guard ambiguity to resolve. The
  accepted trade-off is that an in-place profile switch does not auto-revert a
  previously seeded knob (set it explicitly); this never matters for the
  superbuild, where each cell is a fresh build dir.
- A genuine cross-platform tcc BINARY for a foreign OS (a PE/Mach-O `tcc` via a
  toolchain file) still can't run on the host to build libtcc1/run tests — the
  `cross` target instead builds host-runnable cross *compilers*. Foreign-binary
  targets remain the toolchain-file/emulator item above.
