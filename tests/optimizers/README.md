# Optimization coverage — SEE tests/optfire/ (T-lin-10466)

DESIGN CORRECTION (mcc-inventory agent, DETAILS#t-lin-10468-optimizer-gaps):
do NOT build a new harness — REUSE `tests/optfire/`. It already implements every
assertion needed via `optfire.sh` (5 modes) + CMake auto-globbing of the ledger
tables. Put partials in `tests/optfire/src/<opt>N.c` and register a row:

- **counter opts** (const-fold, cse, dse, sccp, vrp/range, licm, ivsr, pre,
  reassoc, narrow, divmagic, if-conv/select/abs, jt, switch/bf, tco, inline,
  bfold, ident, sra, sroa, ltemp, cload) → a `counters.txt` row
  `<name>|-O<n>|<counter>`: asserts the `--stats=4` counter incremented AND the
  `-On` runtime output == the `-O0` reference (fired AND correct).
- **no-counter / gated opts** (promote-arrow, chain-store, reg-color, spill-share,
  arg-forward, loop transforms) → a `differs.txt` row: `-f` vs `-fno-` objects
  byte-differ AND both == `-O0`.
- **codegen-shape** (branchless select→cmov/csel, divmagic→imul, if-conv drops a
  branch) → a `tests/cli/cases.h` `-S` asm-grep cell (à la arm64_disasm_*).
- **level curve pin** → a `levels.txt` row; **gate attribution** → `cdelta.txt`.
- arch-scope x86_64/arm64-only knobs via `arch.txt` (differs-<cpu>.txt).

Union optimization catalog (top-20 langs): DETAILS#t-lin-10467-lang-opt-catalog
(+ #t-lin-10467-langs-11-20). mcc knob→opt map + GAPS: DETAILS#t-lin-10468-optimizer-gaps.
Gaps are minted as T-lin-10469+ fix tasks.

BUILD DONE (T-lin-10466, DETAILS#t-lin-10466-build-done): the completeness map
lives in `tests/optfire/coverage.txt` — the 116-item union set per-item as
covered / partial / dev / base / gap / oos, guarded by the `optfire/coverage-
ledger` ctest (`coverage-check.sh`) so a renamed or deleted cell can't leave the
map stale. Single-opt partials + the `mix_*` composition cells assert each pass
fires with `-On` output == `-O0`; every gap row names an open T-lin-10469..10475.
