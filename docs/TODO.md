# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Open items (priority order)

- [ ] **arm64 native CI: atomics/bounds/complex failures** (`ubuntu-24.04-arm`,
  `ci.yml`). Highest priority. Common cluster = atomics (all jobs); bounds only
  when bcheck on (gcc/Debug), complex only under clang `-O2`. **Not reproducible
  under qemu-user** (x86-TSO hides arm64 weak-memory): reconfirmed 2026-07-04 —
  scalar atomic load/store and RMW compound-assignment both pass with `mcc-arm64`
  under `qemu-aarch64` against a real arm64 glibc sysroot, so single-thread
  codegen is correct. Most likely root cause: missing acquire/release barrier
  (`dmb`/`ldar`/`stlr`) in mcc's arm64 atomic path, exposed only on real hardware.
  Runtime atomic.o/complex.o are host-cc built on native (`_use_gcc`), so the
  weakness is in mcc's own emission around atomics, not the outline funcs. **To
  fix, need an arm64 host OR the `ctest --output-on-failure` assertion diffs** —
  the *kind* of wrongness (wrong value vs crash vs bcheck-msg vs timeout) picks
  the cause. **Update 2026-07-04 (native arm64 macOS):** the "missing barrier in
  mcc emission" hypothesis is *disproved for the scalar load/store path* — `mcc -S`
  shows `_Atomic` seq-cst load/store emit `bl __atomic_load_N` / `__atomic_store_N`
  with order `0x5`, i.e. ordering is delegated to the host-cc-built runtime helpers
  (which carry the barriers), not plain `ldr`/`str`. So load/store codegen is
  correct; remaining suspects are the RMW/CAS path, the `-b` bounds runtime, or the
  `-O2` complex outline funcs. The **full 809-test suite passes on macOS/arm64**, so
  the failure is specific to the Linux-arm64 hardware+glibc runtime and still needs
  that repro.

- [ ] **Next conformance wins** (ranked; `docs/C9911.md`). Return-mismatch
  (§6.8.6.4p1) is now done (default error). Remaining mcc-specific gaps by
  impact/effort/risk: (1) §7.21.7.7 implicit-function-declaration → default error
  — very high impact (prevents 64-bit-return truncation miscompiles, fixes
  autoconf feature detection), low effort, **but high risk** (legacy C trips it
  constantly; needs a full corpus + self-host pass before flipping). (2) §6.9.1p6
  implicit-int on K&R params → error. (3) §7.16.1.4p3 `va_start` non-last-param
  warning — **already implemented** (`check_va_start_last_param` +
  `check_va_start_register`, `src/mccgen.c`; gated by `-Wvarargs`). (4) §6.7.4p6
  inline-only linkage (mcc inlines+links where gcc/clang fail to link at `-O0`;
  arguably more useful — decide intent before changing).

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
