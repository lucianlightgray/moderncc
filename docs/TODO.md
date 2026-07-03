# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Highest Priority

- [ ] Refactor/merge package.cmake into CMakeLists.txt relying on new tools as much as possible.
- [ ] Port all usages of qemu to use the new C tools pattern.
- [ ] **`release.yml` dist jobs not restructured onto `tools/ci run-preset`.**
  The single-source tooling exists and `ci.yml`'s `dist` job already consumes
  it (`dist-matrix` runs `ci matrix --filter dist-linux --json`, the `dist` job
  reads it via `fromJSON`). `release.yml`'s `dist-unix`/`dist-windows`/
  `dist-mingw` jobs were left intact: their matrix rows carry per-row `plat`
  strings and macOS/MSVC/MinGW runner context that `CMakePresets.json` doesn't
  hold.
- [ ] Investigate normalizing all tests to be CTests, examine multi-part tests
  and break down into smaller unit tests. Only full_language should take many
  small tests and put them together as one large suite of tests. Can mcctest
  variants, exec-suite, cli-suite, etc. be ported?
- [ ] On MacOS, verify that the arm-abi detection in `mccbuild --detect` is
  coded and faithful (macro scan of `__ARM_ARCH`/`__ARM_EABI__`/`__VFP_FP__`/
  `__ARM_PCS_VFP`/`__ARM_FEATURE_IDIV`).
- [ ] Implement `-fstack-protector` on MacOS.

## Notes

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
