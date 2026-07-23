---
name: ci-workflows-generated
description: .github/workflows/*.yml are generated from C-string templates in tools/ci_artifacts.h; never hand-edit the yml
metadata: 
  node_type: memory
  type: project
  originSessionId: f9e91aac-7bac-422b-b92b-c4ffb33379bf
---

The `.github/workflows/*.yml` files (ci, matrix, bench, dist, release, fuzz-nightly) plus the docker/run-*.sh artifacts are GENERATED from C-string templates in `tools/ci_artifacts.h` via `ci emit --write` (EMIT_FILES table). The ctest `ci-artifact-drift` runs `ci emit --check` and fails if a checked-in file diverges from its template.

**Why:** hand-editing a workflow .yml directly (without updating its `TMPL_*` template) makes `ci-artifact-drift` fail on every ctest-running preset, turning all of main CI red. This happened twice around commit 3c685843 (bench macos `CC: clang` fix edited into bench.yml but not TMPL_BENCH_YML).

**How to apply:** to change a workflow, edit the `TMPL_*` string in `tools/ci_artifacts.h`, rebuild (`cc -O2 -o /tmp/mcc-ci tools/ci.c tools/toolsupport.c`), run `ci emit --write`, then `ci emit --check`. The CI job *schedule* (matrix cells) is a separate source of truth: the preset ledger in `tools/ci.c` (`ci plan --job <linux|matrix|macos|msvc|qemu|...>`), cross-checked by `ci parity` (ctest `preset-parity-invariant`) against CMakePresets.json and the `ci local` tables (`par_local_set`). A preset is "covered" if a scanned workflow (the `WF[]` list in `do_parity`) references it via `--job X` (→ `plan_presets`) or names it literally. To split a job into its own workflow like bench/matrix: give it a `ci plan --job X` branch, a new `TMPL_*_YML` + EMIT_FILES entry, add the yml to `do_parity` WF[], and drop the preset from the old job's plan.
