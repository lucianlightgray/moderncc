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

- [~] **Widen replay coverage (§16 Mid).** **This session (2026-07-08) landed 11 milestones**
  covering every major target and most of the tail: floats/double, call-result stores, scalar
  struct member access (`.`/`->`), struct copy/deref, `switch` dispatch, named `goto`/labels,
  **struct-return callees** (`return s`), **by-value struct args** (`f(s)`), **bit-field member
  access**, **struct-return callers** (`struct r=f()`, register-return), and **`f().x`** (member
  of an rvalue struct) — plus two latent correctness bugs fixed on the way (vpop call
  double-emit; float const-pool duplication) and a switch-replay segfault guarded. Fixtures:
  `ast/replay-{float_ops,call_store,struct_member,struct_copy,switch_dispatch,goto_dispatch,struct_return,struct_byval_arg,bitfield,struct_ret_caller}`.
  **Remaining long tail** — four genuinely large/niche items, each needing dedicated
  infrastructure or elaborate reproduction (all fall back correctly today, no crashes):
  - ~~struct-return callers~~ **LANDED 2026-07-08** (register-return form) — `struct r = f()`
    replays. The post-call register→temp reconstruction is reproduced in the Invoke replay,
    and the result temp uses an ordinal frame-slot table (`ast_alloc_loc`/`ast_locrec`, the
    `ast_fconst` pattern) so its offset matches the parse-build; `-O0` stays byte-identical
    (the record is passive). Fixture `ast/replay-struct_ret_caller`. **The sret
    hidden-pointer form (large structs, ret_nregs==0) now replays too** (fixture
    `ast/replay-struct_ret_sret`): `ast_alloc_loc` wraps *both* the register-return temp and
    the sret temp, so replay reserves the same ordinal slot and re-pushes the captured result.
    Took three attempts — the frame-slot `loc` reuse must be coordinated across both sites, and
    the bug lived outside the byte-verified body (epilog/temp reservation), so the test suite
    (not byte-verify) caught the regressions. **The arch-transfer form (ret_nregs<0, mixed
    INT+SSE structs) now replays too** — same sret temp slot + `arch_transfer_ret_regs`; only
    **variadic struct returns** still bail. `f().x` (a struct-call result used directly as a
    member base) also replays — `ast_hook_member_end` threads the base's non-lvalue bit
    (VT_NONLVAL) through to replay. **Struct-return callers are now complete (all ABI forms).**
  - ~~bit-field member Store~~ **LANDED 2026-07-08** — the read-modify-write mask/shift
    (`adjust_bf`/`load_packed_bf` in `gv`, the mask/shift in `vstore`) runs inside the
    suppressed `gv`/`vstore`, so bit-field member access + `Store` + arithmetic all replay
    (member_end/vstore/genop now admit VT_BITFIELD lvalues/operands; feared crash never
    materialized — the shift/mask is fully suppressed). Signed + unsigned. Fixture
    `ast/replay-bitfield`.
  - **`_Complex`** — VT_STRUCT+`is_complex`; needs the build/extract `Convert` modeled.
  - **VLA/`alloca`** — needs the machine-tier `StackAlloc`/`StackSave`/`StackRestore` op (§4),
    a new mechanism, not just a hook.
  - **short-circuit sub-cases** — a plain assignment (`r = a&&b`) and a direct return
    (`return a&&b`) already replay. Still falling back: the *decl-initializer* form
    (`int r = a&&b`, a different init store path) and a VT_CMP used *arithmetically*
    (`(a&&b)+1`, materialized to 0/1 by `gv` outside a suppressed op), plus nested VT_CMP
    operands (bail in `ast_hook_landor_operand`). Value-level (safe — no crash), niche.

  _Baseline (predates this session's widening):_ ≥119/238 exec golden source files replay
  ≥1 function. Measured outcome buckets across the exec corpus (per function):
  ~283 replay, ~200 bail (unsupported construct), ~116 desync (mirror lost sync), ~89
  skip (struct/float/aggregate return via `ast_bad_type`), ~67 unfaithful (byte
  mismatch), ~39 empty. Landed: `for(;;)` loops (`If` op==5; `ast/replay-for_infinite`).
  **Landed 2026-07-08 — `float`/`double` (§A3 floats).** `ast_bad_type` now allows plain
  `float`/`double` (still bails `long double`/`_Complex`-pair `VT_QFLOAT`/struct/bitfield):
  fp arithmetic (`gen_opif`), int↔fp + fp-resize casts (`gen_cast`), fp comparisons, float
  params/returns, and float local-store chains all replay. The one blocker — a `float`/
  `double` constant is materialized by `gv()` into a fresh rodata slot + anon symbol, so
  replaying `gv` made a *second* slot and the reloc diverged → the parse-build now **records
  each const-pool symbol and replay reuses them ordinally** (`ast_fconst`/`ast_replaying`),
  keeping the relocation byte-identical. Fixture `ast/replay-float_ops`. Next targets by bail
  volume: none of the big buckets remain. **Landed 2026-07-08 — `goto`/labels.** A `label:`
  is a `Jump` op==4 marker (ival=token), a `goto` a `Jump` op==5 (both effects in their BB).
  Replay keeps a per-function label table (token → {jind, jnext}) reproducing the parser's
  forward-chain (`gjmp` onto jnext) / backward-jump (`gjmp_addr(jind)`) / definition-backpatch
  (`gsym`). Modeled for plain named gotos; VLA/cleanup-scope/computed-goto bail (their
  `pending_gotos`/`try_call_cleanup_goto` machinery is unmodeled). Forward + backward, out of
  nested loops, over blocks, multiple labels, and goto-from-a-switch-case all replay; fixture
  `ast/replay-goto_dispatch` (safety-net fixture now `ld_fallback`, `long double`).
  **Landed 2026-07-08 — `switch` dispatch.** A switch is an `If(op==6)` [value, bodyBB];
  `case`/`default` labels are `Jump` markers (op==2 [ival=v1,fbits=v2] / op==3) inside the
  body. Capture: `ast_hook_switch_begin` (before `sw->sv=*vtop--`, so the value leaf is
  finalized against the right vtop) + `_case`/`_default` + `_body_end` (suppresses the
  dispatch epilogue's vstack ops via `ast_in_call`) + `_end`. Replay rebuilds a `switch_t`,
  emits the value, jump-over, replays the body (markers record `cr->ind`/`def_sym`), then
  `case_sort` + `gcase` reproduce the binary-search dispatch (with the global `cur_switch`
  set so `case_cmp` sees signedness). **Safety:** the controlling value must be a reloadable
  leaf (Ref/Literal) — a computed value lives in a register the body clobbers, and since a
  fatal replay error is NOT caught by byte-verify (unlike a byte mismatch), computed-value
  switches bail (this fixed a segfault on grep's `switch(tolower(...))`). Fall-through,
  break, ranges (`case a ... b`), unsigned, and nested if/while/switch in cases all replay;
  fixture `ast/replay-switch_dispatch`. **Landed 2026-07-08 — scalar struct member access (`.`/`->`).**
  Member access does uncaptured in-place `vtop->type` retypes (to `char*` for the byte-offset
  add, back to the member type), so the fine-grained op tree can't be replayed (the offset
  would scale by `sizeof(base)`). Folded into one coarse `Unary(AST_OP_MEMBER[_ARROW])` node
  (ival=offset, type=member): `ast_hook_member_begin`/`_end` suppress the internal
  indir/gaddrof/vpushi/gen_op and replay reproduces the parser's exact sequence. `vpush` now
  admits struct/union **lvalue** leaves (a reconstructable frame/global address; bit-field/
  `long double`/`_Complex` still out), and `call_begin` guards by-value struct args so they
  fall back. Reads + writes, local `.` and pointer `->`, and struct-by-value *params* replay;
  fixture `ast/replay-struct_member`. **Landed 2026-07-08 — struct copy + deref.** Struct→struct
  `vstore` now records a `Store` (the aggregate memmove/`gen_struct_copy` runs with its internal
  ops suppressed by `ast_in_op`); replay reproduces the copy. The `indir` guard now allows
  deref-to-struct (a reconstructable lvalue, not a register value), so `*a = *b`, `q = *b`, and
  `(*p).x` replay. Fixture `ast/replay-struct_copy`. Still falling back (future work): struct
  **returns** (sret), by-value struct **args** (call-site guarded), bit-field member `Store`
  (shift/mask desugar), and the `InitList` node. Each construct lands with its `ast/replay-*`
  fixture and the whole-corpus `exec-replay` / `exec-replay-tmpl` columns staying green.
  _Node-level gap ledger (docs/AST.md §A3, feature-complete = the 15 kinds):_ the unbuilt
  kinds are **`InitList`** (aggregate/compound-literal init still reaches codegen as scalar
  `Store`s or a `memset`/`memcpy` `Invoke` — the §7 blob/`memset` grouping node does not
  exist yet) and TU-level **`TranslationUnit`** (per-fn arena only; needed for LTO/static
  localization, Long). `Store` lacks the bitfield shift/mask desugar + aggregate copy;
  `Invoke` lacks sret / by-value-struct args. **Landed 2026-07-08 — call-result stores:**
  `T x = f();` / `x = f();` (both initializer and assignment, `int`/`double` alike) now replay.
  Root cause was a latent double-emit bug: a `Store` leaves the RHS value on the mirror as the
  assignment's result, and `ast_hook_vpop` re-added that `Invoke`/`Unary` as a *bare* BB effect
  → replay emitted the call twice. Fixed by only re-adding when the node is still unparented
  (`ast_parent==AST_NONE`); a value already consumed by a `Store` is parented to it. Fixture
  `ast/replay-call_store`. `long double`/`_Complex` still desync. _Note the two orderings differ:_ by **bail volume**
  `switch`/`goto` lead, but by **combined payoff** floats/wider types led (now landed).
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
- [x] **Regenerate the Windows "all green" counts + add a divergence check
  (both hosts now cite the same per-case basis).**
  The Linux side was regenerated earlier (2026-07-07: `debug` = 1447 registered /
  1279 run / 168 env-gated skips). **Windows now regenerated (2026-07-08,
  main@9544d719) from an actual `ctest` run on this host** — every native preset
  registers **1415** per-case tests, **0 fail**: `debug`/`cst`/`diagnostics` =
  1252 run / 163 skip, `cross` = 1254 / 161, `release` = 1236 / 179, `msvc`
  (cl-built) = 1252 / 163. The prior 812/810 figures predated the `CONFIG_AST`
  `exec-replay`/`exec-replay-tmpl` columns; the regenerated Windows basis now
  matches the Linux one (per-case, replay columns included), and `msvc` no longer
  reads "two fewer" (the `wine`/`macho` cases register-and-self-skip, counted).
  `docs/NOTES.md` "Windows status" cites these figures. _Divergence check —
  decided NOT to add the strict count-checker_ (the `tools/ckbuildmd.c`-style grep
  of NOTES.md vs `ctest -N`): the registered total is documented to **track
  upstream test additions**, so a hard drift-fail would break CI on every
  legitimate new test; the real divergence risk was the two hosts citing different
  *bases*, which the regeneration resolves.
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
