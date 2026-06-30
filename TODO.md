# TODO — C9911 gap implementation tracker

Working tracker for the standing goal: migrate the actionable gaps from
[C9911.md](C9911.md) (the `mcc:✗`/`mcc:~` items where mcc diverges from the
gcc==clang consensus) into here, implement them easiest-first, and **remove each
item once it is completely implemented and verified** — the permanent landed
record lives in C9911.md's *Landed* appendix sections (and git history).

**C9911.md is partly stale** (generated before some merges): several items it
flags as `mcc:✗` are already fixed. So *re-verify every candidate 3-way against
the live binary* before implementing — confirm it's a real gap, prune it if not.
Each fix ships a cli/exec regression test; keep full ctest + the byte-identical
self-host fixpoint green. (C9911's Appendix holds the *deliberate* DIFFs — those
are intentional and out of scope.)

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Open items

*(none — every actionable C9911 gap is migrated and resolved. Finished items are
removed from this file as the goal directs; see C9911.md's "Landed" sections for
the full record, most recently **Landed round 3**: §6.10.8.3 `__STDC_UTF_16__`/
`__STDC_UTF_32__` predefined on all targets, and §7.26.1p3 `thread_local` in the
bundled `<threads.h>` shim.)*

## Remaining C9911 `mcc:✗`/`mcc:~` markers — classified, not open

The residual divergence markers in C9911.md are **not** actionable consensus
gaps; they need no migration:

- **`mcc:✗(consensus) gcc:✗ clang:✗`** — the Annex G `_Imaginary`-type rows
  (§G.2–G.7). No compiler implements the optional imaginary types; no divergence.
- **UB / recommended-practice only** — e.g. §7.16.1.4p3 (`va_arg` second-arg UB),
  §6.4.4.2p7 (hex-float-inexact *recommended* diagnostic). No required diagnostic.
- **No gcc==clang consensus** — e.g. §6.7.4p2/p3 inline-definition constraints
  (gcc and clang disagree / are `~`); mcc matching one side is defensible.
- **Permissive-by-default, diagnoses under `-Werror`** — implicit decls, K&R
  implicit-int, etc.: mcc warns (errors under `-Werror`), matching its documented
  philosophy; C9911 counted the exit-0 default as accept.
- **Deliberate divergences** — recorded in C9911.md's Appendix (out of scope).

---

# CLI-flag parity with gcc/clang (C99/C11 compiler)

Gaps found by comparing `mcc --help`/`-hh` and `src/libmcc.c`'s option table
against gcc 15.3 / clang command-line arguments, then probing the live binary
3-way. These are command-line / diagnostic features a C99/C11 compiler is
normally expected to provide; none affect the C9911 clause coverage above.
Severity tags: **[core]** expected of any C compiler · **[diag]** a common
warning · **[build]** build-system integration · **[doc]** documentation only.

## Compiler modes

- [ ] **[core] `-S` (emit assembly) is rejected** ("invalid option -- '-S'").
  gcc/clang both produce a `.s`. mcc is direct-to-object, so true `.s` emission is
  a large effort; at minimum recognize `-S` and either emit a disassembly listing
  or fail with a clear "assembly output not supported" message rather than
  "invalid option". *Verify scope, then implement or document.*

## Diagnostics — warning coverage (`-Wall`/`-Wextra` are thin)

mcc only emits a handful of warnings; gcc/clang `-Wall`/`-Wextra` catch far more.
Each below was confirmed: mcc=0 warnings where gcc/clang warn. (mcc already does
`-Wreturn-type`-style "missing return", implicit-function-declaration, and the
new `-Wformat`.) Most need scope/dataflow tracking — size each before starting.

- [ ] **[diag] `-Wunused-variable` / `-Wunused-function`** — unused locals and
  unused `static` functions (under `-Wall`). Needs use-tracking per scope/TU.
- [ ] **[diag] `-Wunused-value`** — a statement with no effect (`1+1;`).
- [ ] **[diag] `-Wunused-parameter`** — under `-Wextra`.
- [ ] **[diag] `-Wuninitialized`** — use of an uninitialized local. Needs simple
  dataflow; can be conservative to avoid false positives.
- [ ] **[diag] `-Wparentheses`** — assignment used as a truth value (`if (x = 1)`).
- [ ] **[diag] `-Wsign-compare`** — signed/unsigned comparison (under `-Wextra`).
- [ ] **[diag] `-Wswitch`** — `switch` on an enum that omits a case / lacks `default`.
- [ ] **[diag] `-Wshadow`** — a declaration shadowing one in an outer scope.
- [ ] **[diag] `-Wimplicit-int`** — named flag for the implicit-`int` diagnostic
  mcc already emits as a bare warning (give it a controllable `-W[no-]` name).
- [ ] **[diag] `-Wextra` umbrella is inert** — accepted but enables nothing. Wire
  it to a set (unused-parameter, sign-compare, …) like gcc/clang.
- [ ] **[diag] consider enabling `-Wformat` under `-Wall`** (gcc/clang do). Held
  opt-in for self-host safety; revisit once the checker has soaked.

## Preprocessor / dependency-generation flags

- [ ] **[build] `-imacros <file>`** rejected — like `-include` but contributes only
  the file's macro definitions (no text). Standard gcc/clang option. Needs a
  preprocessor pass that scans the file for `#define`s and discards its tokens;
  a plain `#include` would wrongly pull in declarations, so kept out of the
  `-iquote`/`-idirafter` batch.

## Diagnostics control (minor)

- [ ] **[diag] `-Wfatal-errors` / `-fmax-errors=N`** accepted but inert — should
  stop after the first (or N-th) error. Currently mcc swallows them silently.
