# TODO — port everything to native C

Goal: drive the entire project (build, tests, runtime support, tables) from C
source instead of shell / CMake / assembly / data files. This is the standing
plan; each phase is independently shippable and ordered easy → hard.

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

## Phase 1 — `stab.def` → C X-macro header  *(smallest, do first)*
`stab.def` is already a C-includable X-macro table (`__define_stab(...)`
invocations) `#include`d by the debug code. Port:
- Rename to an X-macro header (e.g. `stab.h` already exists as the public side;
  fold the table into a `stabs.h` consumed via `#define __define_stab(...)`),
  or convert to a static C array in `tccdbg.c`.
- Verify the stab/dwarf debug tests still pass.
Effort: ~1h. Risk: low. Pure C achievable.

## Phase 2 — runtime assembly `lib/*.S` + `win32/lib/chkstk.S` → C TUs
Move each runtime helper into a `.c` file. Per file:
- `lib/atomic.S` → **pure C** using tcc's C11 `_Atomic` / `__atomic_*` builtins
  (tcc already implements them; `lib/stdatomic.c` is precedent).
- `lib/alloca.S`, `lib/alloca-bt.S` → `.c` with a small inline-`__asm__` body
  (alloca must move `%rsp`/`sp`; bt variant adds the bounds-checking call). One
  `.c` per arch path, guarded by `__i386__`/`__x86_64__`/`__arm__`/…
- `lib/pic86.S` (i386 GOT/PC thunk), `win32/lib/chkstk.S` (MS stack probe) →
  `.c` with inline `__asm__` (a handful of instructions each).
- Update `CMakeLists.txt` / `tcc_build_libtcc1` source lists to the new `.c`.
Effort: ~1–2 days. Risk: medium (ABI-exact; self-compile by tcc). Mostly C,
small inline-asm islands. Validate libtcc1 + the bcheck/atomic tests per target.

## Phase 3 — `win32/lib/*.def` import lists → generated-from-C
The five Windows DLL export lists are linker data. Port:
- Add a tiny host C tool (`win32/mkdef.c`) that emits the `LIBRARY/EXPORTS`
  text, OR embed the export-name tables as C arrays consumed directly by tcc's
  import-lib builder (`tcc -impdef`/PE path) so the `.def` files are generated,
  not checked in.
- Drive generation from `CMakeLists.txt`; drop the committed `.def`.
Effort: ~1 day. Risk: low/medium (Windows-only path; hard to test off-Windows —
gate behind a PE/cross check, validate under the win32 bootstrap or wine).

## Phase 4 — test goldens `*.expect` → self-checking C  *(largest, 207 files)*
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

## Phase 5 — test-fixture assembly `tests/asmtest.S`, `tests/pp/{12,13,23}.S`
These are *inputs* that exercise tcc's assembler / preprocessor on `.S`, so they
stay assembly by nature. Port options:
- Re-express each as a C TU with the same body inside `__asm__("…")` (tests the
  integrated assembler through the C front end), or
- Keep them but treat as data consumed by the Phase 4 C runner.
Effort: ~0.5 day. Risk: low. (Lower priority — they’re test data, not shipped.)

## Phase 6 — build system `CMakeLists.txt` + `CMakePresets.json` + `*.bat`  *(hardest)*
Replace CMake with a native C build orchestrator. tcc is self-hosting, so:
- Write `build.c` (host-cc–compiled) that: compiles the libtcc TUs + backend
  with the host `cc`, links `tcc`, builds `libtcc1`/runtime (now Phase-2 `.c`),
  builds the cross `*-tcc`, and runs the Phase-4 C test runner. It subsumes the
  inline `cmake -P` drivers (move that logic into `build.c`).
- Fold `win32/build-tcc.bat` and `tests/test-win32.bat` into the same `build.c`
  (cross-platform), retiring the batch files and `CMakePresets.json`.
- Keep a thin `configure`-less entry (`cc build.c -o b && ./b`).
Effort: multi-week, large. Risk: high (loses CMake’s install/package/IDE
integration; must re-derive every config knob now in `CMakeLists.txt`). This is
the speculative end-state — sequence it LAST and only after Phases 1–5 remove
the test/runtime/data dependencies the build system currently wires together.

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
