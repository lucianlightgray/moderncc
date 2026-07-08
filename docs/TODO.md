# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (migrated to docs/NOTES.md).

---

# AST — coverage widening (docs/AST.md §16 Mid)

The AST intention-IR **first phase (A1–A7) is complete and green** — intention-IR
library, vstack-replay driver, three-layer differential gate, and the const-fold
template. The completion record is in docs/NOTES.md ("Completed work — AST
intention-IR (first phase: replay driver + first template)"); the design is in
docs/AST.md. `-O0` codegen is byte-identical with `CONFIG_AST` on; the driver runs
only under `MCC_AST_REPLAY`, and the byte-verify net keeps every unmodeled construct
on correct `-O0` fallback — so widening coverage cannot break output correctness,
only add (or fail to add) replayed functions.

- [~] **Widen replay coverage (§16 Mid).** ≥119/238 exec golden source files replay
  ≥1 function today. Measured outcome buckets across the exec corpus (per function):
  ~283 replay, ~200 bail (unsupported construct), ~116 desync (mirror lost sync), ~89
  skip (struct/float/aggregate return via `ast_bad_type`), ~67 unfaithful (byte
  mismatch), ~39 empty. Landed this pass: `for(;;)` loops (`If` op==5 — no gvtst, empty
  break chain; fixture `ast/replay-for_infinite`). Next targets by bail volume:
  **`switch`** (~28 fns — the dispatch is a `gcase` binary-search comparison tree
  emitted *after* the fall-through body, so byte-exact replay must reconstruct the
  sorted `case_t`/`ind` layout), **`goto`/labels** (~25 fns — unstructured jumps with
  forward references), then floats/wider types and aggregate struct/union/bitfield
  `Store`/copy + member access `.`/`->`. Each construct lands with its `ast/replay-*`
  fixture and the whole-corpus `exec-replay` / `exec-replay-tmpl` columns staying green.
  _Node-level gap ledger (docs/AST.md §A3, feature-complete = the 15 kinds):_ the unbuilt
  kinds are **`InitList`** (aggregate/compound-literal init still reaches codegen as scalar
  `Store`s or a `memset`/`memcpy` `Invoke` — the §7 blob/`memset` grouping node does not
  exist yet) and TU-level **`TranslationUnit`** (per-fn arena only; needed for LTO/static
  localization, Long). `Store` lacks the bitfield shift/mask desugar + aggregate copy;
  `Invoke` lacks sret / by-value-struct args; `Convert`/`Literal`/`Binary` desync on floats
  and `_Complex`. _Note the two orderings differ:_ by **bail volume** `switch`/`goto` lead,
  but by **combined payoff** floats/wider types lead (they dominate the ~89 skip + much of
  the ~116 desync buckets — a larger slice than the ~28+~25 `switch`/`goto` bails).
- **Reprioritized (2026-07-08, docs/AST.md §A1):** liveness-steered **register promotion**
  (mem2reg of address-not-taken locals) is the real -O1 payoff → moved to **early Mid**,
  right after the zero-template invariant is green. The broader template library beyond
  const-fold (algebraic, dead-branch, jump-table) is **demoted to Long** (structural
  rewrites are the smaller win). Long-horizon items (virtual always-inline over the shared
  store, cross-TU LTO, `-g` from provenance, hot-reload snapshots, **separate `-O2`/`-O3`
  replay drivers / SSA**) stay design in docs/AST.md §16 Mid/Long.

## AST — decided-with-revisit-trigger backlog (docs/AST.md "Revisit triggers")

Each is a closed decision; the item is the named condition that would reopen it.

- [ ] **Verify the CST answers every `-g` and LSP question without friction/contention**
  (does the CST's lexical-scope spans + source ranges cover debugger scope queries, hover
  types, go-to-def, live ranges) — if a gap surfaces, that reopens the dissolved `Bind`
  marker (docs/AST.md §B1). Until then `Bind` stays fully dissolved into liveness.
- [ ] **Acceptance bar (§C1):** the `-O1` replay driver is "done" only when every
  `tests/exec` golden is green under the `-O1-replay` column **and** GCC's own test suite
  passes under both `mcc -O0` and `mcc -O1`. Wire up the GCC-suite run as an AST gate.
- [ ] **`k` value:** raise the always-inline depth `k` above the `k=1`/widen-on-back-edge
  default only under `-O2`/`-O3` or an explicit size budget (`k≈log_b(budget)`).
- [ ] **Size-gated outline:** land as a later binding-graph template (swap an inline binding
  for a `Call` to a materialized standalone when rendered-size × site-count > budget);
  v1 stays strict always-inline.
- [ ] **Store factoring:** pull `store`/`binding`/`render` into the structure-agnostic
  engine at the *first virtual-inline render* — the shared-storage mechanism for the
  `CONFIG_CST || CONFIG_AST` overlap; neither subsystem ever depends on the other.
- [ ] **Template DSL / data-driven registry:** revisit a declarative pattern DSL past ~30
  templates; keep the function-pointer registry interface uniform so it can go data-driven.
- [ ] **Per-function `-O1` mode:** consider only if `-O1`'s multi-pass (defer-to-TU) compile
  latency becomes a complaint.
- [ ] **PP-as-executable-C (JIT):** parked; promoted-not-necessitated by the
  include-permutation analysis.

---

# Now

- [~] Normalize as much of the CMake code as possible: 1) minimize gating instead preferring autodetecting the existence of tools and enabling as many tests/targets/configs as are available on the host, 2) reduce CMake usage by relying on `tools` where advantageous, 3) fold in separate .cmake files into CMakeLists.txt.
  _Assessment (2026-07-07):_ **(3) largely moot** — there are no external `.cmake`
  *modules* to fold: `CMakeLists.txt` is already monolithic, the only `include()`s
  are the optional `config-extra.cmake` + standard CMake modules (ExternalProject,
  GNUInstallDirs, …), and the `run_*_fetch.cmake` helpers are already generated
  inline via `file(WRITE …)`; the `tests/*.cmake` are `-P` driver scripts that run
  in a fresh process and *cannot* be folded. **(1) substantially in place** — 25
  `find_program()` autodetections drive the toolchain/reference-cc/emulator
  discovery, and `ci local`/the presets enable what the host supports. **(2)** the
  `tools/` family (`build.c`, `ckbuildmd.c`, `hostgate.c`, and this session's
  `ckconfig.c`) already offloads config-emission + invariant checks from CMake.
  What remains is an open-ended "as much as possible" polish with real
  CI-breakage risk across the ~35 presets/platforms not testable from one Linux
  host — pursue incrementally with a specific, verifiable target, not as a sweep.
- [~] **Regenerate the Windows "all green" counts + add a divergence check
  (Linux done; Windows residual needs a Windows CI run).**
  The Linux side is now regenerated from an actual `ctest -N` run and date-stamped
  (2026-07-07: `debug` = 1447 registered / 1279 run / 168 env-gated skips; the
  `docs/NOTES.md` "Build status" + Profiling §7 + "Compile speed & footprint" table
  + `README.md` headline all cite that single per-case basis and the re-measured
  0.07 s / 76–91× vs -O2 / 0.72 MB / 1.45 MB figures). **Residual (not locally
  reproducible):** the Windows counts (812/810) predate the `CONFIG_AST` replay
  columns (~+640 cases) — regenerate them from the next Windows CI run so both hosts
  cite the same basis (a note to that effect is in the doc). Optional durable fix:
  a checker (mirror `tools/ckbuildmd.c`) that greps the NOTES.md registered-count
  against `ctest -N` and fails on drift.
- [~] **Validate the remaining i386 TLS large-address pattern assumptions
  (x86_64 32[S] done; i386 TLS residual needs i386 cross + sysroot).**
  x86_64 GD/LD/IE/LE is covered by the `tls-models` ctest (`tests/tls/`, links
  gcc/clang objects in all four models, dynamic + static) — that push fixed real bugs
  (TLSGD→LE used only the symbol's own section size for the TP offset; static GD/LD
  links failed on `__tls_get_addr`). The `R_X86_64_32[S] out of range` check
  (`x86_64-link.c`) now has a positive test — `cli/x86_64_reloc_32s_range`
  (`cpu=x86_64,os=linux,asm`) forces a >2 GB layout with two ~1.5 GB `.bss` arrays and
  an absolute `movl $b+…` past +2 GB, asserting the diagnostic fires. **STILL OPEN:**
  the i386 `R_386_TLS_GD/LDM` pattern paths (`i386-link.c`) need an i386 cross build +
  32-bit sysroot to exercise (not available in this env). → Add the i386 TLS gate under
  the cross preset when an i386 runtime is available.

- [ ] **Investigate the macOS CI benchmark reporting gcc "n/a" for the
  full-language config.** gcc should be available/enabled for `full_language` on
  macOS — find why the bench probe skips or fails to detect it (likely the
  Homebrew `gcc` vs Apple-clang-`gcc`-shim distinction, or a `full_language`
  feature the shim can't compile) and fix it so the column reports a real number
  instead of `n/a`.

---

# Skipped-test ungating audit (per triple)

For each triple below: **when the current host has enough access to build/run
*all* of that triple's tests**, rigorously re-evaluate every test it currently
`mcc_skip_test`s or self-skips (rc 77). For each skip, decide whether a
*legitimate* subset can actually run on this platform and ungate that subset
(narrow the gate, split the case, or drop the guard); leave genuinely
unsupportable cases skipped with the reason kept current. Don't ungate blind —
verify the ungated part passes on the real target, not just that it compiles.

Access status is from this host (x86_64 Linux; qemu-aarch64/arm/i386, wine,
mingw x86_64 + i686 present; no macOS SDK/osxcross, no i386-Linux sysroot, no
arm64 mingw). Re-probe before acting — availability changes per CI runner.

- [ ] **x86_64-linux (native).** _Access: full now._ Audit the host-native skips
  first — these have no emulation excuse: `cli-suite` native-only structural
  readelf/nm, `static-glibc-run` (no static glibc), `parts-suite`/`diff3-suite`
  (needs both gcc + clang), `mcctest` (GCC-compatible ref cc). Confirm each guard
  still reflects a real host gap vs a stale assumption.
- [ ] **i386-linux.** _Access: blocked — qemu-i386 present but no 32-bit Linux
  sysroot._ Ties into the open i386-TLS item in "Now". Ungate once a 32-bit
  sysroot is on the runner; until then the skips are legitimate.
- [ ] **aarch64-linux.** _Access: partial — qemu-aarch64 present, but qemu models
  x86-TSO so weak-memory atomics/bounds tests can't be faithfully validated
  (see [[arm64-native-ci-failures]])._ Ungate only the memory-model-independent
  subset under qemu; leave the ordering-sensitive cases for a native arm64 runner.
- [ ] **armv7-linux.** _Access: partial — qemu-arm present, same x86-TSO caveat._
  Same split as aarch64: ungate memory-model-independent tests under qemu, defer
  ordering-sensitive ones to native hardware.
- [ ] **x86_64-windows (mingw + wine).** _Access: available — x86_64-w64-mingw32
  + wine present._ Re-evaluate the `pe-wine-conformance` / `mcctest` PE skips: how
  much of the full_language differential runs under wine byte-identically, and
  which cases are genuinely wine-limited vs conservatively gated.
- [ ] **i386-windows (mingw + wine).** _Access: available — i686-w64-mingw32 +
  wine present._ Same audit as x86_64-windows for the 32-bit PE path.
- [ ] **arm64-windows.** _Access: blocked — no arm64 gcc/mingw reference; the only
  reference is emulated x86_64 mingw, which can't be byte-identical to native
  arm64 mcc (see the `mcctest` skip reason)._ Leave `mcctest`/`mcctest-bcheck`
  skipped until a native arm64-Windows reference cc exists; codegen stays covered
  by exec/* goldens + pe-native-conformance.
- [ ] **x86_64-Darwin.** _Access: blocked — no macOS SDK/osxcross on host._ Ungate
  the 2-slice / macho differential skips only on a runner with the SDK + cross
  toolchain; the exec-based macho harnesses already self-skip off-x86_64.
- [ ] **arm64-Darwin.** _Access: blocked — no macOS SDK/osxcross on host._ Same as
  x86_64-Darwin; also revisit the documented external `__thread` Mach-O limitation
  when a real arm64 macOS runner is available.

---

# C99/C11 test-coverage backlog (from docs/TESTS.md)

Each item ports/mirrors a specific gcc/clang conformance test into an mcc test —
runtime cases go in `tests/exec/features_c99_c11/`, diagnostics/negatives in
`tests/diff/parts/` (or a new reject corpus). Reference paths are relative to
`~/Projects/gcc/gcc/testsuite` (gcc) and `~/Projects/llvm-project/clang/test/C`
(clang). Context + gap matrix: docs/TESTS.md §5–§6. Landed C99/C11 additions are
recorded in docs/NOTES.md ("Landed: C99/C11 test-coverage additions & fixes").

## Real semantic/diagnostic gaps — fix mcc, then add the test


## Coverage-depth gaps — mcc passes but under-tests vs gcc/clang; add tests

- [~] **UCN-in-identifier breadth.** Basic runtime is covered by
  `tests/exec/lexical/ucn_identifiers.c` (`\u`/`\U`, raw UTF-8, raw≡escaped); the
  **invalid-UCN rejection** cases (§6.4.3) are now pinned by
  `cli/c11_ucn_basic_latin_reject` (`A` in an identifier) and
  `cli/c11_ucn_surrogate_reject` (`\uD800`), both matching gcc/clang rc=1.
  *Remaining = the smaller tail: UCN in different token positions and
  normalization/UAX#31 breadth.* _Ref:_ gcc `gcc.dg/ucnid-*.c`; clang
  `C99/n717.c` (UCN grammar), `C11/n1518.c` (UAX#31).
- [~] **Negative/diagnostic test tier.** _Established_ in `tests/cli/cases.h`
  (grep-the-message pattern): `c99_fam_not_last`, `c11_alignas_underalign`,
  `c99_vla_goto_into_scope`, `c99_vla_switch_into_scope`, `c11_noreturn_returns`,
  plus this session's `c99_kr_implicit_int`, `c99_inline_no_extern_def`,
  `c11_ucn_basic_latin_reject`, `c11_ucn_surrogate_reject`, and
  `c11_signed_unsigned_reject` (type-specifier `signed`+`unsigned`; the "too many
  basic types" excess is already in `errors_and_warnings.c`). *Remaining
  (continuous): broaden toward the ~70% of gcc's C99/C11 files that are `dg-error`
  negatives* — the highest-volume seed is gcc `gcc.dg/c99-typespec-1.c` (1055
  dg-error over every type-specifier combo), plus `c11-align-3.c` and the
  `c99-flex-array-*` / `c11-*` negative files.

---

ACHTUNG!!! DO NOT DO!!! WARNING!!!

* Use only human friendly warnings/errors, backed by tests that check formatted output against terminal dimensions/configuration
* Implement/finish `-g` debugging/debugger and flesh out gdb/etc test cases, check against gcc and clang sources of truth
* Optimization -O1...100 levels measured in max seconds to spend optimizing?
* Hot reload by saving/loading CST snapshots on the fly and on run with --hotreload arg
* Run hot-reloads from reconciled CST snapshots

ACHTUNG!!! DO NOT DO!!! WARNING!!!

---
