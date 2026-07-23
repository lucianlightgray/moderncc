---
name: ci-workflows-canonical
description: .github/workflows/*.yml are the hand-edited source of truth; bootstrap via the ci composite action; ci parity checks presets
metadata:
  node_type: memory
  type: project
---

The `.github/workflows/*.yml` files (ci, matrix, bench, dist, release, fuzz-nightly) are the **canonical, hand-edited source**. Edit them directly.

**History (2026-07-23, commit cd7c042a):** they used to be GENERATED from C-string templates in `tools/ci_artifacts.h` via `ci emit --write`, guarded by the `ci-artifact-drift` ctest. That whole system — `ci_artifacts.h`, the `emit` verb, the drift test — was **removed** as inverse-DRY (the templates were a committed mirror of files that also lived on disk). If you see a reference to `ci emit`, `ci_artifacts.h`, `TMPL_*`, `@BOOT@`, or `EMIT_FILES`, it is stale.

**How to apply:**
- To change a workflow, edit the `.yml` directly. No regeneration step.
- Every workflow bootstraps the `ci` driver through the `.github/actions/ci` composite action, which compiles `tools/ci.c` for the host and puts `mcc-ci` on `PATH`. Do not copy-paste a `cc`/`cl` bootstrap into a `run:` block. **Gotcha:** GitHub runs composite bash with `set -e`, so a failing command substitution in an assignment (`x=$(... && ...)`) aborts the step — compute conditionals with an explicit `if` (this bit the macOS `-ldl` probe, fixed in 3ff73d9c).

**Preset schedule is still a single source of truth** in the preset ledger in `tools/ci.c`: `ci plan --job <linux|macos|macos-x86|windows|qemu|bench|matrix|dist-unix|dist-windows>` emits the matrix cells, and `ci parity` (ctest `preset-parity-invariant`) cross-checks every CMakePresets.json preset against the `ci local` tables (`par_local_set`) and the workflows. A preset is "covered" if a scanned workflow (the `WF[]` list in `do_parity` — now ci, matrix, release, dist, bench, fuzz-nightly) references it via `--job X` (→ `plan_presets`) or names it literally. `ci parity` reads the **live** `.yml` from disk, so it needs no generated copy.

**Ledger-driven workflows:** bench.yml is a `plan` + single matrix job fed by `ci plan --job bench`; fuzz-nightly.yml runs the campaign via the `ci fuzz` verb (configure debug → build `mcc fuzz_runner` → run campaign). To split a job into its own workflow: add a `ci plan --job X` branch, a `plan_presets` branch, and the new yml to `do_parity`'s `WF[]`.

Related: [[commit-push-main]], [[cross-mcc-lacks-optimizer]].
