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

**Status: COMPLETE — no open items.** Every gap found by comparing `mcc
--help`/`-hh` and `src/libmcc.c`'s option table against gcc 15.3 / clang (then
probing the live binary 3-way) has been implemented or resolved, each with a cli
regression test and kept green through full ctest + the byte-identical self-host
fixpoint. Finished items are removed per the goal's convention; the landed work
(in git history) covers:

- **Modes:** `-fsyntax-only` (no output); `-S` recognized with a clear
  direct-to-object diagnostic.
- **Diagnostics:** `-Wpedantic`/`-Wno-pedantic`, `-Wvla`, `-Wundef`,
  `-Wunknown-pragmas`, `-Wimplicit-int`, `-Wsign-compare`, `-Wparentheses`,
  `-Wswitch`, `-Wshadow`, `-Wunused-variable`, `-Wunused-parameter`,
  `-Wunused-function`, `-Wunused-value`, `-Wuninitialized`; `-Wformat` now under
  `-Wall`; the `-Wextra` umbrella wired up; `-Wfatal-errors`/`-fmax-errors=N`.
- **Preprocessor / build:** `-MT`/`-MQ`, `-iquote`, `-idirafter`, `-imacros`.
- **Docs:** `-pedantic`/`-pedantic-errors` documented in `-hh`.

(Also fixed a latent `warn_num` selector leak surfaced while adding warn flags.)

---

# C9911 re-verification (fresh crawl) — COMPLETE

Re-crawled C9911.md's 35 `mcc:✗ gcc:✓ clang:✓` lines and re-probed each 3-way
against the live binary. ~30 were stale (fixed in prior rounds; their C9911
tags lagged reality). Four were genuinely open — **all now fixed** (each with a
cli test, full ctest 34/34, byte-identical self-host, and the C9911.md line
retagged `mcc:✓`):

- §7.26.1p3 `TSS_DTOR_ITERATIONS` made a usable integer constant.
- §7.25p7 `creal`/`cimag` tgmath dispatch by complex element type.
- §D/§6.4.2.1 Annex D.1 allowed-range check for UCNs in identifiers.
- §7.16.1.4p3 `va_start` non-last-parameter diagnostic (builtin-va_start targets).

The remaining `mcc:✗`-tagged C9911 lines are non-actionable: `mcc==gcc==clang`
(no divergence: §F.10.11 isgreater, §G.5.1 complex-inf — both now match),
permissive-by-default-then-`-Werror` (implicit decls / K&R implicit-int / `gets`),
or a deliberate divergence (§6.7.4p6 plain-inline link semantics — self-host
sensitive). See C9911.md's classification notes.

