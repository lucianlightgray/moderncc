# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Open items (priority order)

- [ ] **Next conformance wins** (ranked; `docs/C9911.md`). Return-mismatch
  (§6.8.6.4p1) and implicit-function-declaration (§7.21.7.7) are now default
  errors; `va_start` non-last-param (§7.16.1.4p3) is warned. Remaining gaps by
  impact/effort/risk: (1) §6.9.1p6 implicit-int on K&R params → error. Needs a
  **targeted** change, not a flag flip: promoting the whole `warn_implicit_int`
  to `WARN_ERR` was tried and over-reaches — it breaks `cli/implicit_int_diag`
  and the `exec/errors_and_warnings` run-test, which rely on *general*
  implicit-int (`static x;`, defaulted return type) staying a warning. Erroring
  only the K&R-param site (`mccgen.c` FUNC_OLD, ~:10246) requires a dedicated
  warning flag + those two tests updated. Marginal payoff (K&R defs are rare), so
  deferred. (2) §6.7.4p6 inline-only linkage (mcc inlines+links where gcc/clang
  fail to link at `-O0`) — **needs a product decision, not a mechanical change**:
  mcc's current behavior is arguably *more* useful, so "fixing" it to match
  gcc/clang is a deliberate intent call for the maintainer, not a clear win.
  Done: §7.21.7.7 implicit-function-declaration → default error (`WARN_ON |
  WARN_ERR`, downgradable via `-Wno-error=`), validated by a corpus + self-host
  pass on macOS/arm64 and Linux/arm64.

- [ ] **Rosetta macOS x86_64 CI needs a real CI run to validate.** `ci.yml` +
  `release.yml` build the x86_64 macOS artifact on the arm64 `macos-15` runner
  (universal clang `-arch x86_64` + `MCC_TARGET_ARCH=x86_64`, Rosetta 2 runs the
  x86_64 test/host binaries). **Validated locally 2026-07-04 on an Apple-silicon
  host:** the `-DCMAKE_OSX_ARCHITECTURES=x86_64 -DMCC_TARGET_ARCH=x86_64` build
  produces an x86_64 Mach-O `mcc`; under Rosetta it compiles+links+runs a program
  and `-run` (JIT), and a ctest subset (hello/exec/framework) passes 10/10. Only
  the actual CI run on the runner remains unconfirmed.

## Notes

- `-fverbose-asm`-style operand comments: meaningful comments need
  codegen-side variable/spill metadata that is discarded after emission;
  classified low-value (reloc symbol names are already printed). Revisit
  only if a debugging workflow materializes that needs it.

---

ACHTUNG!!! DO NOT DO!!! WARNING!!!

• Implement/finish `-g` debugging/debugger and flesh out gdb/etc test cases
• Can a fully static build use a minimalistic `-run` to sidestep the dynamic linking limitations and use libc or musl in-memory?
• What is the purpose of libmccrt.a ? Can it be replaced with ctests?
• Use only human friendly warnings/errors, backed by tests that check formatted output against terminal dimensions/configuration
• CST Database for Debugging, LSP, and Optimization data/layers
• CST Database uses hierarchical incremental hashes to enable bidirectional lookups starting from any character index in any file
• Hot reload by saving/loading CST snapshots on the fly and on run with --hotreload arg
• Run hotreloads from reconcoliled CST snapshots

ACHTUNG!!! DO NOT DO!!! WARNING!!!

---
