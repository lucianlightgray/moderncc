# tests/diff/parts

Each `*.h` here is a **main-free C11 test unit** with **no `#include`s of its own** —
the includer supplies the environment. This lets one source serve two roles:

- **Isolated unit test** — `run_<slug>.c` pulls in real system headers via
  `std_env.h`, then the unit, then a `main`. The **parts-suite** ctest compiles
  every `run_*.c` with **gcc, clang and mcc** and requires byte-identical stdout
  (3-way differential).
- **All-in-one C11 test** — `../full_language.c` sets up its `mcclib.h`-based
  environment and `#include`s every unit, running them together (the `mcctest` /
  `mcctest-bcheck` differential gate) plus a `coherency` unit that composes
  features from several units in one flow.

Because the units carry no headers, the same code compiles both against the
minimal `mcclib.h` surface (in full_language.c) and against the full real libc
(in the wrappers), proving the features are coherent across environments.

`s7_28` (wchar/uchar) is parts-suite-only: real `<wchar.h>` redefines `FILE`,
which clashes with full_language.c's opaque `mcclib.h` `FILE`.

## Kinds of unit

- **Dual-use C99/C11 units** (`s04.h`, `s6_*.h`, `s7_*.h`, `s7_libm.h`,
  `s_*.h` — e.g. `s_annCDE.h`, `s_annFGK.h`, `s_stddef.h` —, `coherency.h`) —
  self-contained; both a 3-way parts-suite unit *and* aggregated.
- **Aggregate-only legacy units** (`legacy_*.h`) — the historical tcctest body,
  split into ordered thematic chunks. They share file-scope macros/globals with
  each other and with full_language.c's prelude, so they are `#include`d **in
  order** (never reordered — chunk boundaries only fall where the preprocessor
  `#if` depth is 0) and are not standalone parts-suite units. This reduced
  full_language.c from ~7100 to ~415 lines: it is now the prelude/harness
  (including the `#include`-mechanism preprocessor tests) + the ordered part
  `#include`s + `main`.
