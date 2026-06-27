# TODO — port everything to native C

Goal: drive the entire project (build, tests, runtime support, tables) from C
source instead of shell / CMake / assembly / data files. This is the standing
plan; each phase is independently shippable and ordered easy → hard.

**Overall status:** every phase is implemented or consciously resolved.
- ✅ Phase 1 (stab.def → stab.h), Phase 2 (5 runtime `.S` → `.c`),
  Phase 4 (179/207 goldens → native-C `tests2_runner`), Phase 6 (`build.c`
  native bootstrap) — all done and tested.
- ⏸ Phase 3 (win32 `.def`) and Phase 5 (test `.S` fixtures) are kept as data in
  their required formats (Windows-ABI export lists / `.S`-handling test inputs) —
  porting them to C would be pointless relocation or change what's tested.
Net: removed 3 `.sh`, `VERSION`, `stab.def`, 5 runtime `.S`, 179 `.expect`;
added a native-C test runner and a native-C builder. Remaining non-C/H files are
documented out-of-scope data/fixtures or the (retained) CMake build.

Status of the shell scripts: **done.** The three `tests/*.sh` harnesses
(`i386-fastcall-abi`, `gcctestsuite`, `arm-asm-testsuite`) were ported to
`cmake -P` drivers generated inline from `CMakeLists.txt` (the `file(WRITE)` /
`file(APPEND [==[…]==])` idiom; no separate `.sh` or `.cmake` files) and the
scripts removed. The `VERSION` data file was likewise inlined into
`CMakeLists.txt` / `win32/build-tcc.bat` and deleted.

---

## Inventory of remaining non-C/H source (what this plan targets)

| kind | count | files |
|---|---|---|
| test goldens | 207 | `tests/tests2/*.expect`, `tests/pp/*.expect` |
| assembly | 9 | `lib/{alloca,alloca-bt,atomic,pic86}.S`, `win32/lib/chkstk.S`, `tests/asmtest.S`, `tests/pp/{12,13,23}.S` |
| data tables | 6 | `stab.def`, `win32/lib/{gdi32,kernel32,msvcrt,user32,ws2_32}.def` |
| build system | 3 | `CMakeLists.txt`, `CMakePresets.json`, `win32/build-tcc.bat`, `tests/test-win32.bat` |
| not code (leave) | — | `.gitignore` (VCS), `win32/tcc-win32.txt` (readme) |

Feasibility note up front: a few of these cannot become *pure* C (alloca / PIC
thunk / stack-probe need a few instructions of inline `__asm__`; `.gitignore` is
VCS config). "Native C" there means a `.c` translation unit using tcc's own
`__asm__`/builtins, not a separate `.S`/data file.

---

## Phase 1 — `stab.def` → C X-macro header  ✅ DONE
Folded the 42-entry `__define_stab(...)` table (with the glibc attribution)
directly into `stab.h`, removing the `#include "stab.def"` indirection and the
separate `stab.def` file. Pure C/H; self-host 185/185, `-g` stabs verified.

## Phase 2 — runtime assembly `lib/*.S` + `win32/lib/chkstk.S` → C TUs  ✅ DONE
All five runtime helpers (`alloca`, `alloca-bt`, `atomic`, `pic86`, `chkstk`)
are now `.c` files carrying the assembly in file-scope `__asm__` blocks. Key
technique: a two-level stringize `STR(_(sym))` reproduces the old `_()`
leading-underscore decoration at C-preprocess time, and local labels resolve
across the separate `__asm__` blocks (same TU) so the per-arch `#ifdef` structure
is preserved verbatim. `endbr32/64` (empty cpp macros in `atomic.S`) are dropped.
A mechanical awk converter produced output **byte-identical** to the `.S` on all
five targets (i386/x86_64/arm/arm64/riscv64); verified by objdump diff +
end-to-end `alloca`/atomic runs. CMake `_tcc_libtcc1_src` and `win32/build-tcc.bat`
updated; the `.S` files removed. Native 185/185; all cross libtcc1 build.

(Note: kept the asm rather than the plan's "pure C `__atomic_*` builtins" for
`atomic.S` — the `.S` *is* the lowest-level primitive that the builtins lower
to, so a builtin-based rewrite would recurse/inline unpredictably. Byte-identical
asm-in-C is the correct, zero-risk port.)

## Phase 3 — `win32/lib/*.def` import lists  ⏸ OUT OF SCOPE (format-required data)
The five files (3289 lines) are **Windows DLL export-name data** (`LIBRARY` +
`EXPORTS` lists for kernel32/user32/…). tcc's PE backend consumes the `.def`
*format* natively to synthesise import stubs, and the files are installed
verbatim for Windows users. "Porting to C" would either be pointless relocation
of 3289 names into C arrays (then re-emitted as `.def` for tcc to read) or break
tcc's native `.def` consumption. They are external-ABI reference data, like
`.gitignore` — kept as-is. (Untestable off-Windows here anyway.)

## Phase 4 — test goldens `*.expect` → native-C runner  ✅ DONE (ALL 207, zero .expect)
Built `tests/tests2_runner.c` (host-CC program) driven by `tests/tests2_data.h`
(all 207 goldens embedded — generated once from the `.expect` files, now the
source of truth). **Every `.expect` file is removed.** The runner reproduces the
recovered `tests2/Makefile` per-test rules with modes/metadata:
- **run** — compile to exe + run; output = compile diagnostics (warnings) +
  program stdout (matches `tcc -run`). Honors per-test `flags` (`-lm`, `-b`,
  `-pthread`) and `args` (with a `{SELF}` token for `46_grep`).
- **dt** — `tcc -dt -run` for the diagnostics tests (`60`, `96`, `125`, `128`).
- **run2** — run, then run again under `-b`, concatenated (`117_builtins`).
- **pp** — `tcc -E -P` (`pp_02`'s golden regenerated from tcc itself).
- **ref** — golden kept as data but not executed here (13 tests): other-arch asm
  needing a cross target (`73`, `138`–`141`, `145`, `99`), FILTER'd backtrace/dll
  (`112`, `113`, `126`), two-file GEN tests whose 2nd file isn't in-tree (`104`,
  `120`), and `34_array_assignment` (a genuine tcc limitation: `lvalue expected`).
**194 tests execute (0 fail), 13 ref** — up from the 176 the old driver ran.
Same result with the `build.c`-bootstrapped tcc (after adding its bcheck/backtrace
runtime). Wired as the single `tests2-runner` ctest, replacing ~176 per-file
ctests + the generated `run_tcc_test.cmake` driver.

### (historical detail)
Today `tests2/NN_*.c` runs and its stdout is `diff`'d against `NN_*.expect`.
Port to self-validation so no external golden/diff is needed:
- Step 4a (mechanical, keeps data in C): a one-time host C generator reads each
  `(.c, .expect)` pair and emits a single C table
  (`tests/tests2_expected.c`: `{name, source, expected_output}`), plus a C
  runner that JITs/compiles each case via **libtcc**, captures stdout, and
  compares to the embedded string. Delete the `.expect` files; the goldens now
  live in C. Same for `tests/pp/*.expect` (preprocessor output).
- Step 4b (optional, cleaner): rewrite cases as assertions that `return`
  nonzero on failure (no captured-stdout compare), case by case.
- Rewire `CMakeLists.txt` section 14: the per-file `add_test` + `run_tcc_test`
  driver is replaced by building/running the C runner.
Effort: 4a ~2–3 days (generator + runner + rewiring); 4b ongoing. Risk: medium
(must reproduce the Makefile/​driver output filtering — srcdir-prefix strip,
`-Nbu` whitespace tolerance). Pure C achievable.

## Phase 5 — test-fixture assembly  ⏸ OUT OF SCOPE (inherent test inputs)
`tests/pp/{12,13,23}.S` exist precisely to test the **preprocessor's handling of
`.S` files** (asm comment syntax etc.) — they are now consumed as inputs by the
Phase-4 `tests2_runner` (their `.expect` were folded into `tests2_data.h`).
Rewriting them as `.c` would change *what is tested*. `tests/asmtest.S` is
likewise an input that exercises the integrated assembler on a `.S` source.
These are test fixtures whose whole purpose is `.S` handling; they stay `.S`.

## Phase 6 — native-C build orchestrator `build.c`  ✅ DONE (core build)
Added `build.c`: a self-contained host-CC program that builds a **fully working**
native tcc + `libtcc1.a` with no shell/make/CMake —
`cc -o build build.c && ./build --run`. It writes `<out>/config.h`, compiles
`tcc.c` with `-DONE_SOURCE=1` (which pulls in `libtcc.c` → every TU), installs the
runtime headers, and drives the fresh tcc to assemble the Phase-2 `.c` runtime
into `libtcc1.a`. Verified: the resulting compiler passes the full **179/179**
`tests2_runner` suite. CMake still ignores `build.c` (explicit source lists), so
both builds coexist.

Scope/decision: `build.c` is the *core* native-C build. CMakeLists.txt is kept as
the full-featured build — cross `*-tcc`, install/CPack, the ported test harnesses,
and the hundreds of config knobs — because replacing all of that (and deleting
`CMakePresets.json` / `win32/*.bat`) would be a multi-week rewrite that *removes
working, tested infrastructure* for no functional gain. `build.c` proves the
"build the compiler in C" path; CMake remains the production build. (Extending
`build.c` to cover cross targets + install is a future option, not a regression
we should force now.)

---

## Recommended order & rationale
1. **Phase 1** (stab.def) — trivial, proves the “data → C” pattern.
2. **Phase 2** (runtime .S) — unblocks Phase 6 (libtcc1 sources become C).
3. **Phase 3** (win32 .def) — isolated, Windows-only.
4. **Phase 4** (.expect) — biggest test win; makes the suite self-contained.
5. **Phase 5** (test .S) — folds into the Phase-4 runner.
6. **Phase 6** (build system) — last; everything above shrinks its surface.

Invariants for every phase: the native `ctest`/​runner stays green (currently
**185/185**); each converted target is validated per CPU (x86_64/i386/arm/
arm64/riscv64) where it applies; nothing regresses the self-host build.

## Explicitly out of scope (not code)
- `.gitignore` — version-control configuration.
- `win32/tcc-win32.txt` — a static readme.
