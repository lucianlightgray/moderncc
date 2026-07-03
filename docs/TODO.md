# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Highest Priority

_No open items._

## Notes

- **Per-case CTest normalization (investigated, mechanism proven).** The
  aggregate suites (`exec-suite`, `cli-suite`, `diff3-suite`, `preprocess`,
  `mcctest`, `parts`) each run one C `runner` that loops an in-source case
  table. They *can* be split into one CTest per case: give the runner a
  `--list` (print names) and `--only <name>` (run one case, exit 0/1/77) pair,
  parse the case names out of the table header at configure time, and register
  `add_test(<suite>/<name> … --only <name>)`. `exec_runner` and `cli_runner`
  are done as the reference (`-DMCC_GRANULAR_TESTS=ON`; default OFF keeps the
  fast single-process aggregate). Granular mode is *faster* (exec: 31s `-j8`
  vs 75s aggregate) and reports/selects each case (`ctest -R exec/<name>`).
  The remaining runners (diff3/preprocess/mcctest/parts) follow the identical
  pattern; `full_language` stays a deliberate aggregate. The split forced two
  latent bugs into the open: a stale hand-maintained `mcc_goldens_count` (=264)
  had stranded 7 goldens untested — now `sizeof`-derived — and two cli cases
  shared the name `ucn_identifier_range` (second renamed `…_range2`).

- `-fverbose-asm`-style operand comments: meaningful comments need
  codegen-side variable/spill metadata that is discarded after emission;
  classified low-value (reloc symbol names are already printed). Revisit
  only if a debugging workflow materializes that needs it.

---

ACHTUNG!!! DO NOT DO!!! WARNING!!!

• Implement/finish `-g` debugging/debugger and flesh out gdb/etc test cases
• Can a fully static build use a minimalistic `-run` to sidestep the dynamic linking limitations and use libc or musl in-memory?
• What is the purpose of libmcc1.a ? Can it be replaced with ctests?
• Use only human friendly warnings/errors, backed by tests that check formatted output against terminal dimensions/configuration
• CST Database for Debugging, LSP, and Optimization data/layers
• CST Database uses hierarchical incremental hashes to enable bidirectional lookups starting from any character index in any file
• Hot reload by saving/loading CST snapshots on the fly and on run with --hotreload arg
• Run hotreloads from reconcoliled CST snapshots

ACHTUNG!!! DO NOT DO!!! WARNING!!!

---
