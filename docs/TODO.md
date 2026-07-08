# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

# AST first phase (docs/AST.md §16 "Short" + §17 replay-driver bring-up)

The intention IR alongside the CST. Gated by CMake `CONFIG_AST` (ON by default),
built as a pure side-channel like the CST — `-O0` stays a one-pass parse+compile
flow and byte-identical; `-O1` builds the AST and replays it through the existing
vstack API (`vpushi`/`gen_op`/`gv`/`vstore`/`gsym`/`gjmp`). The gate is the existing
`tests/exec` goldens re-run as an `-O1-replay` column asserting the same `expect`,
brought up one §17 category at a time.

- [x] **A1 — `CONFIG_AST` scaffolding.** CMake `MCC_AST` option (ON), `CONFIG_AST=1`
  define, `ast` preset, `libmcc.c` includes `src/mccast.c` (guarded); mccbuild
  `--ast` + BUILD.md node/preset rows so the config-drift gates stay green.
- [x] **A2 — `src/mccast.{c,h}` intention-IR library.** The 15 node kinds; per-function
  SoA arena (D-c: minimal, no hash-cons yet); builder API; textual `ast_dump`;
  CST-provenance id per node (§14); `ast_validate`. Self-contained (malloc un-poison).
- [x] **A3 — `tools/asttool.c` pure-lib TDD harness + `ast/*` ctests.** 5 suites
  (arena/validate/dump/cfg/provenance), 30 checks. Full `ast`-preset ctest 865/865.
- [x] **A4 — replay driver (`ast_replay_body`) over the vstack API.** In `gen_function`,
  when `MCC_AST_REPLAY` is set: build the intention tree while the parser runs, then
  **discard the parser's body emission** (`ind = body_ind`) and re-emit from the AST
  through the vstack API. **Byte-verify safety net (§17 straight-line tripwire):** the
  re-emitted body is compared to the parser's `-O0` bytes; on *any* mismatch the parser's
  emission is restored verbatim (bytes + `ind` + `rsym`). So correctness never depends on
  having modeled every vstack op — an unmodeled construct (unary `-`, a call, control
  flow) just diverges and falls back. Faithful captures re-emit **byte-for-byte identical**
  to `-O0` (the zero-template invariant, demonstrated). Off by default; `-O0` untouched.
  **Coverage: 119 / 238 exec golden source files have ≥1 function that faithfully replays**
  (int-constant/local/param arithmetic, calls, casts, control flow, array subscripting +
  scalar-array `{...}` initializers); the rest fall back. Full ast-preset ctest 1166/1166.
  - [x] rung 1: `return <integer-constant>;` → `vpushi`/`gfunc_return`/`gjmp`.
  - [x] rung 2: **integer-arithmetic return trees** (`return argc + 41;`,
    `return 2+3*4;`) via a **scoped vstack-mirror**: `ast_hook_vpush`/`ast_hook_genop`
    shadow the vstack during the return expression, capturing `Literal`/`Ref`/`Binary`;
    `gen_op` is modeled atomically (internal traffic ignored via `ast_in_op`, re-synced
    at its exit); unmodeled in-place transforms (`gen_cast`/`indir`/`gaddrof`) and
    non-reconstructable leaves (registers/symbols/floats) trip `ast_desync` → fall back.
    Leaves restricted to int-constants + frame-relative locals/params (re-push exactly
    after discard). Corpus column caught + drove fixes (cleanup/enum). Fixtures:
    `retexpr` (folds), `argc_expr` (param arithmetic).
  - [x] rung 3: **whole-body straight-line capture with local `Store`.** The mirror
    stays live across statements; `vstore`/`vswap`/`vpop` are modeled so local decls
    with initializers and assignment statements become `Store` effects in the
    BasicBlock. `int main(){int a=5,b=7; return a*b+7;}` replays byte-identically as
    `[Store(a,5), Store(b,7), Return]`. Unary minus (emitted `0-x` via `gen_op`) also
    replays now. Fixtures local_ret/local_two + cmp_fallback (safety net).
  - [x] rung 4a: **global references + relocation discard/verify.** Symbolic leaves
    (a global's address/lvalue — the Sym persists so re-push re-creates it) are
    captured; the safety net now discards the body's relocations with its text before
    replay and byte-verifies both, restoring verbatim on mismatch. Leaf SValues are
    finalized lazily at consumption (vsetc's hook fires before callers set `->sym`).
    `int g=7; int main(){return g+35;}` replays byte-identically. Fixture global_ref.
  - [x] rung 4b: **casts (`Convert`) + calls (`Invoke`).** `gen_cast` outside a modeled
    op becomes a `Convert` node (unblocking arg promotions/explicit casts). The
    `gfunc_call` boundary folds [callee, args] into an `Invoke`, suppresses the mirror
    across the call + result push (`ast_in_call`), and re-pushes the captured result;
    string-literal args ride as `Convert(Ref)` (their `.rodata` persists — only
    text+relocations are discarded/replayed). Bare call statements are BasicBlock
    effects. Diagnostics suppressed during replay (`warn_none`). Struct/two-register
    returns and indirect callees fall back. **Coverage 27→68/239 files.** Fixtures
    cast_expr, call_printf.
  - [x] rung 5: **control flow** (the CFG milestone D-b) — captured at the `block()`
    handler level, with replay re-issuing the parser's exact `gind`/`gvtst`/`gjmp`/
    `gsym` pattern (no backend jump hooks). Done: **comparisons** (`< > <= >= == !=`),
    **`if`/`if-else`** (nested BasicBlocks), **non-tail returns** (branch returns that
    jump to the epilogue), **`while`**, **`++`/`--`** (`inc()` modeled as `Unary`),
    **`for`** (init;cond;incr), **`do-while`**, **`break`/`continue`** (Jump nodes
    chaining onto the loop's replay-time chains). Loops are `If` nodes op==2 (while)/
    op==3 (for)/op==4 (do-while). Compound-assign (`+=`) and comma replay. **Memory
    model:** pointer deref/address-of (`Load`/`gaddrof`) and **array subscripting
    `a[i]`** (gen_op nesting counter for the recursive pointer-scale gen_op('*');
    `ast_bad_type` guards keep struct/union/bitfield/float on correct fallback).
    Expression-level control flow: **`?:` ternary** and **`&&`/`||` short-circuit**
    (register-coordinated branch values; nested short-circuit/ternary operands bail).
    **Scalar-array `InitList`** (`int a[N]={...}`): the zero-init `memset` a local
    aggregate emits is captured as a **void-effect `Invoke`** (a new
    `ast_hook_call_effect_end` — `gfunc_call` consumes every operand and pushes nothing,
    so no result rides the mirror; the `VT_VOID` type marks it so replay re-emits the
    call and leaves the stack empty), and each element `{...}` value is an ordinary
    `Store` effect. Fixture array_init. **Coverage 68→119/238 files (50%).** Remaining:
    first-phase gate (§15/§17) — the whole corpus green under **zero-template**
    replay, faithful captures re-emitting byte-for-byte — holds, so the replay
    driver is up and A7 (below) landed on top of it. **Deferred to Mid** (coverage
    widening, §16 Mid — not a first-phase blocker: the byte-verify net keeps the
    unmodeled tail on correct `-O0`): aggregate struct/union/bitfield `Store`/copy
    + member access `.`/`->` (aggregate deref bails), `switch`, `goto`, floats/wider
    types.
- [x] **A5 — parser AST-build hooks.** `ast_hook_stmt` (count + bail on unsupported
  leaf statements) and `ast_hook_return` (capture Return of an int constant) fire from
  the parser's statement/return positions, gated by `CONFIG_AST` + `ast_active`. Grew
  alongside A4's rungs into the full hook set (vpush/genop/vstore/convert/call/if/
  while/for/do/inc/indir/gaddrof/ternary/landor).
- [x] **A6 — differential-exec replay gate (three layers, all green).**
  - `tests/ast/replay.cmake` — 21 targeted fixtures that must *actually* replay (dump
    fired, not fall back), from `ret42` through `array_init`/`ternary`/`logand`, plus a
    `switch_fallback` safety-net case that must fall back to correct `-O0`, plus the A7
    `template-constfold` case (fold must fire *and* stay byte-faithful).
  - **`exec-replay/*` column** — the whole `tests/exec` golden corpus re-run
    with `MCC_AST_REPLAY=1`, asserting the same expected output. Functions the driver
    can lower go through the AST; everything else falls back to `-O0`, so the column
    stays green as the driver grows. Replay-induced safety verified: the discard is
    guarded by no-new-locals + no-new-relocations + pure-constant-return, so a body
    with a call / cleanup / global store (e.g. cleanup.c `test_ret`) correctly falls
    back instead of dropping work.
  - **`exec-replay-tmpl/*` column** — the same corpus re-run with the const-fold
    template *also* on (`MCC_AST_TEMPLATES=1`): the §15 whole-corpus per-template
    differential gate (input == output). Full ctest 1446/1446.
- [x] **A7 — first template = const-fold** (docs/AST.md §12/§15/§17-D-d). A tree-scope
  rewrite `Binary(op, Literal, Literal) → Literal(fold)` over the pure integer
  arithmetic/bitwise/shift subset, run on the AST *above* the emitter before replay
  (`ast_run_templates`/`ast_fold_rec`/`ast_fold_eval` in mccgen.c; new `ast_set_kind`/
  `ast_clear_children` builder API). The fold mirrors `gen_opic` exactly (same
  `value64` normalization / signed div), so it is **byte-neutral** — gen_op already
  folds adjacent constants at `-O0`, so a folded node re-emits bit-for-bit and the
  gen_function byte-verify net still governs correctness. Gated by `MCC_AST_TEMPLATES`
  so the zero-template invariant still owns the default replay column. §15 differential
  tests: pure-lib `ast/template` (rewrite API), `ast/template-constfold` (fold fires +
  byte-faithful), and the whole-corpus `exec-replay-tmpl/*` column. **This closes the
  AST first phase** (§17 "Replay driver — golden-TDD bring-up"); further templates
  (algebraic, dead-branch, jump-table) and coverage widening are §16 Mid.

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
- [x] **`exec/tls` skipped on arm64+WIN32 (decided: not an mcc defect —
  diagnosed as an MSVC-arm64 miscompile of mcc; mitigated with skip + timeout).**
  On the `msvc / arm64` runner, `exec/tls` intermittently hung (ctest 63 min,
  manual cancel). Root cause is **not** in mcc: **MSVC's arm64 code generator
  miscompiles mcc itself** on the static-`__thread` emission path. The
  MSVC-arm64-built `mcc.exe` nondeterministically drops functions when it
  compiles a `__thread` TU (`tls.c`): some builds/runs lose `main`, others
  truncate a trampoline → the linked exe hangs. Isolation was exhaustive and
  conclusive:
  - mcc's arm64 codegen is **correct** — the same mcc source built by **gcc**
    (x86_64 *and* arm64 Linux) and by **MSVC-x64 cross-targeting arm64-win32**
    all emit a byte-identical, deterministic, correct `tls.s` (50×/30× runs).
  - No mcc source UB: Valgrind clean; `-ftrivial-auto-var-init=pattern` clean.
  - Only **MSVC's arm64 backend building mcc** fails, and it is build/run
    nondeterministic (a later CI build was 0/30 corrupt), so it cannot be
    reproduced or bisected without an arm64 Windows + MSVC box.
  - `exec/tls` still runs (and passes) on **x86_64 WIN32** and on every gcc/clang
    arm64 target, so `__thread` codegen stays covered. The `msvc` ctest step also
    carries `--timeout 300` so any future flaky hang fails fast instead of
    stalling the job.
  - **Tried and reverted (435087ee):** a scoped `#pragma optimize("", off)` around
    the arm64 TLS-access codegen (`arm64_tls_base_x30` + `load`/`store`) did **not**
    fix the hang (`exec/tls` still timed out), so the miscompiled construct is
    *outside* that region. It also used raw `_MSC_VER`/`_M_ARM64`, which the
    `host-gate-invariant` test forbids outside `src/mcchost.{h,c}` — any future
    host-conditional must route through an `MCC_HOST_*` macro defined there.
  - _Next (needs arm64 Windows + MSVC):_ bisect the miscompiled mcc construct.
    A whole-mcc-TU `/Od` on that build is the last-resort blunt workaround, but
    the earlier `MCC_NOOPT` `/Od`-vs-`/O2` probe was inconclusive (the intermittent
    corruption did not reproduce that run), so even that is unverified.
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
- [x] **`va_start` non-last / `register` param check never fires on x86_64
  (decided 2026-07-07: deferred, diagnostic-only, boundary documented).**
  `C9911.md:3215` §7.16.1.4p3 — the SysV macro (`__builtin_va_start` in
  `runtime/include/mccdefs.h`) reads the reg-save area from the frame and never
  references `parmN`, so the misuse diagnostic (present on arm64/riscv64/PE via
  the real `TOK_builtin_va_start` case) is absent on x86_64-SysV and i386.
  → _Deferred:_ making the check target-independent needs x86_64-SysV to lower
  `va_start` through the real builtin (implement `gen_va_start` for SysV) instead
  of the frame-address macro — a codegen rework of the primary target's varargs
  for a diagnostic-only gain; not worth the risk without a driving need.
  Reference test to mirror once fixed: gcc `c-c++-common/Wvarargs-2.c`
  (`va_start` on a non-last / fixed-arg param). See docs/TESTS.md §6-A.3.
- [x] **External (SHN_UNDEF) thread-local symbols hard-error on Mach-O — TLV
  imports unimplemented (decided: intentional limitation, documented).**
  `src/objfmt/mccmacho.c:2099`. Locally-defined
  `__thread` works (TLV descriptors via `__tlv_bootstrap`); cross-module
  `extern __thread` errors. Documented as an intentional limitation in
  `docs/NOTES.md` (Platform ABI & runtime notes) — revisit only if a real
  cross-module-TLS-on-Darwin need appears; the fix is emitting TLV *import*
  descriptors.
- [x] **i386 fastcall/thiscall: non-register arg before a register arg
  unsupported (decided 2026-07-07: accepted limitation, documented).**
  `src/arch/i386/i386-gen.c` errors when a register-eligible integer arg follows a
  stack-spilled arg in a `__fastcall`/`__thiscall` call. Decided to document the
  accepted ABI limitation rather than rework the register assignment (the affected
  shapes are rare — a large by-value aggregate or over-width value ahead of a
  small integer in a Microsoft-convention prototype — and a fix can't be validated
  without an i386 runtime here). The diagnostic is the pinned boundary; see
  docs/NOTES.md "Platform ABI & runtime notes". The common register-then-stack
  ordering is fully supported (`i386-fastcall-abi`). Revisit if a real
  spilled-then-register prototype surfaces.
- [~] **Validate the remaining i386 TLS + x86_64 32[S] large-address pattern
  assumptions (x86_64 32[S] done; i386 TLS residual needs i386 cross + sysroot).**
  x86_64 GD/LD/IE/LE is now covered by the `tls-models`
  ctest (`tests/tls/`, links gcc/clang objects in all four models, dynamic +
  static) — that push fixed real bugs: TLSGD→LE used only the symbol's own
  section size for the TP offset (wrong with a 2nd TLS section), and static GD/LD
  links failed on `__tls_get_addr` (relaxed away, now resolved to 0).
  **(b) DONE (2026-07-07):** the `R_X86_64_32[S] out of range` check
  (`x86_64-link.c`) now has a positive test — `cli/x86_64_reloc_32s_range`
  (`cpu=x86_64,os=linux`) forces a >2 GB layout with two ~1.5 GB `.bss` arrays
  (NOBITS, so a cheap address-arithmetic check) and an absolute `movl $b+…` past
  +2 GB, asserting the diagnostic fires. **STILL OPEN (a):** the i386
  `R_386_TLS_GD/LDM` pattern paths (`i386-link.c`) need an i386 cross build +
  32-bit sysroot to exercise (not available in this env). → Add the i386 TLS gate
  under the cross preset when i386 runtime is available.
- [x] **ARM inline-asm `long long` operands unimplemented (decided 2026-07-07:
  documented subset).** `src/arch/arm/arm-asm.c` hard-errors on 64-bit GPR-pair
  operands. mcc's inline assembler is a modelled subset for what C inline `asm`
  needs, not a full ISA; the boundary is now documented in docs/NOTES.md
  "Inline-assembler coverage" (use two 32-bit operands / a vmov round-trip for
  64-bit values). Expand only when a real workload needs it.
- [x] **arm64 inline assembler errors on unmodeled mnemonics (decided 2026-07-07:
  supported subset documented).** `src/arch/arm64/arm64-asm.c` models the common
  subset (add/sub/logical/shift/mul/mov*/mrs·msr/ldr·str·ldp·stp/branches·cbz/
  adrp/barriers/ret/nop) and hard-errors by exact name on the rest (verified
  missing: `cmp`/`csel`/`udiv`/`madd`/`fadd`/`neg`/`ldxr`, …). Supported set +
  notable gaps are documented in docs/NOTES.md "Inline-assembler coverage"; the
  self-naming error is the pinned boundary. Expand the table when a real inline-asm
  workload needs a specific mnemonic.
- [x] **Resolve the 6 permanently-masked ARM asm encodings (2026-07-07: 4 fixed +
  byte-verified vs `arm-linux-gnueabi-as`; 2 shown to be non-defects).**
  - `mov #0xEFFF` / `mov #0x0201` — **FIXED.** mcc now synthesizes `movw Rd,#imm16`
    when a `mov` immediate is neither a rotated immediate nor an inverted one
    (`src/arch/arm/arm-asm.c`), matching GNU as byte-for-byte (`e30e2fff` /
    `e3004201`), with no regression on ordinary/`mvn`-form immediates.
  - `vmov.f32 r2,r3,d1` / `vmov.f32 d1,r2,r3` — **FIXED.** The 64-bit
    two-GPR↔doubleword transfer now works: a `d`-register operand promotes the
    op to double regardless of the `.f32` suffix (matches gas: `ec532b11` /
    `ec432b11`), no regression on `s`-reg / `.f64` d-d forms.
  - `b r3` / `bl r3` — **not encoding defects.** Byte-identical to gas, and mcc now
    also emits the matching EABI relocations (`R_ARM_JUMP24` for `b`, `R_ARM_CALL`
    for `bl`, was the legacy `R_ARM_PC24`) — verified a real `bl <sym>` links with
    `R_ARM_CALL`. The only residual is objdump's symbolic target annotation
    (`b 0 <r3>` vs `b 0x0`), a cosmetic REL-vs-RELA rendering difference immaterial
    for these invalid branch-to-register inputs, so they stay in `ARM_KNOWN_FAIL`
    with that documented. The 4 fixed entries were dropped from the mask.
  All four fixes are byte-verified against GNU as; x86_64 suite green (arm-asm.c is
  ARM-target-only). *(Aside noted during triage: mcc's `-c` ARM objects carry no
  `.ARM.attributes`, so a local `objdump` misdisassembles correct v6T2+ bytes —
  a harness-reliability nicety, not a codegen defect.)*
- [~] **Reference-harness `exec`/`diff3` goldens are effectively dead (validated
  2026-07-07).** The four `note:`-skipped goldens (inline multi-unit, backtrace,
  btdll, alias) carry full expected output but self-skip because each needs a
  bespoke harness the exec runner has no mode for (multi-variant compile,
  shared-lib, multi-TU symbol export). Determination: their behavior *is* covered
  by executing tests, so the goldens are documentation, not coverage holes —
  their `req` notes now say so. **inline**: the §6.7.4 emission matrix is now
  executed by `cli/c99_inline_emission_matrix` (compiles the real
  `exec/functions_abi/inline.c`, checks all four export categories via `nm`) plus
  the cli inline-linkage cases. **alias**: single-TU alias is executed by the
  `alias_single_tu` golden; only cross-TU alias resolution stays documentation.
  **backtrace/btdll**: bcheck *detection* is executed by the `bound_*` + `builtins`
  goldens; the formatted backtrace output (a debug aid) stays documentation — a
  multi-variant / shared-lib address-normalizing harness is not justified for it.
  → *Residual (optional):* build the multi-variant backtrace harness if formatted
  `-bt`/`-b` output ever needs execution-level pinning.
- [x] **Windows keeps diagnostic *auto*-color off (validated 2026-07-07).**
  Confirmed from the code: `-fdiagnostics-color=always` **does** force color on
  Windows — `diag_want_color` (`src/libmcc.c:553`) returns 1 for `diag_color==1`
  unconditionally, bypassing `host_stderr_isatty`. The only gap is *auto*-detection:
  `host_stderr_isatty` (`src/mcchost.c:21`) hardcodes `return 0` on `_WIN32`, so no
  color is auto-enabled even on a VT-capable Windows Terminal. **Decision:** keep
  the conservative default (explicit override works, which covers the real need);
  the auto-detect enhancement — return `_isatty(2) && GetConsoleMode(stderr) &
  ENABLE_VIRTUAL_TERMINAL_PROCESSING` — is fail-safe by construction but its
  *compilation* on the mingw/MSVC toolchains can't be validated from this Linux
  host, and a broken Windows build is worse than color-off, so it is deferred to a
  change made with a Windows toolchain in hand. Low priority.
- [x] `-fverbose-asm`-style operand comments (decided: won't-do, low-value):
  meaningful comments need codegen-side variable/spill metadata that is discarded
  after emission; classified low-value (reloc symbol names are already printed).
  Revisit only if a debugging workflow materializes that needs it.
- [x] **CST slice-I symbol resolution is last-declaration-wins (decided
  2026-07-07: intentional v1, pinned).** Root cause: `cst_hook_def`/`cst_hook_use`
  key def offsets by identifier token id in a single slot (`cst_defoff[v]`), so a
  redeclaration overwrites the previous def — no scope stack. A file-scope name
  shadowed inside a function resolves *both* uses to the same (last-declared) def.
  **Decision:** record as the intentional v1 boundary rather than build a scope
  stack (LSP-era work; the CST symref is a side-channel tooling aid, not codegen —
  codegen scoping is the parser's and is correct). Pinned by
  `tests/cst/symref/shadow.c` + `symref-shadow.cmake` (`cst/symref-shadow`), which
  asserts both `x` uses map to one def and fails with a pointer to this item if a
  scope-aware resolver ever splits them.
- [x] **CST slice-J macro-invocation v1 imprecisions (decided 2026-07-07:
  accepted v1, pinned).** Confirmed both: a function-like invocation's trailing
  `)` splits into a sibling `Paren` node (the `MacroInvocation` span stops before
  it), and an object-like macro used inside another macro's args stays a plain
  token (no nested `MacroInvocation`). The byte-identical round-trip — the
  load-bearing invariant — still holds in both. **Decision:** accept as the v1
  boundary (fixing the node shape risks the round-trip for a side-channel tooling
  aid; a precise expander is slice-J/LSP-era work) rather than reshape the nodes
  now. Pinned by `tests/cst/macro/macro_nesting.c` + `macro-nesting.cmake`
  (`cst/macro-nesting`), which asserts the clean round-trip, exactly one
  `MacroInvocation` (nested object macro unwrapped), and the split trailing-`)`
  `Paren` — and fails with a pointer to this item if a precise expander changes
  the shape.
- [x] **CST 5B incremental splice + `H_e` epoch hash are designed, not built
  (decided: deferred until the LSP/5B consumer lands).**
  NOTES CST §3.1/§10: the invertible epoch hash + tombstone sweep (O(1)-per-level
  incremental rehash for live edits) and the 5B splice are reserved (slot-key field
  + frontier-scoped `H_s`-recompute ship) but unbuilt; they're LSP/5B-era and gated
  on 4B rolling-hash + error-recovery + `Error`/`Missing` nodes. → Build when the
  LSP consumer lands. Note: D3 repurposed `slot_key` for branch tags, so an `H_e`
  build must reconcile that column's dual use.
- [x] **ARM (32-bit) direct branch can't reach past ±32MB — no veneers (decided:
  documented diagnostic is the pinned boundary until a >32MB image surfaces).**
  `encbranch` in `src/arch/arm/arm-gen.c` encodes `B`/`BL` with the 24-bit signed
  word displacement (±32MB reach); a target farther than that is currently a hard
  `mcc_error("branch target out of range ...")` (formerly a `FIXME:` inline
  comment, moved here 2026-07-07). Real toolchains instead synthesize a *veneer*
  (a long-branch trampoline island: load the absolute target into a scratch reg
  and `BX`, or an inline literal-pool `LDR pc,[pc,#-4]`) so arbitrarily large
  images link. → When an image that large actually surfaces, emit a veneer for
  out-of-reach `B`/`BL` instead of erroring; until then the error is the pinned
  boundary. The arm64 backend has the same limit as a plain
  `mcc_error("branch out of range")` (`src/arch/arm64/arm64-gen.c:241`) and would
  want the matching treatment.

---

# C99/C11 test-coverage backlog (from docs/TESTS.md)

Each item ports/mirrors a specific gcc/clang conformance test into an mcc test —
runtime cases go in `tests/exec/features_c99_c11/`, diagnostics/negatives in
`tests/diff/parts/` (or a new reject corpus). Reference paths are relative to
`~/Projects/gcc/gcc/testsuite` (gcc) and `~/Projects/llvm-project/clang/test/C`
(clang). Context + gap matrix: docs/TESTS.md §5–§6. The `va_start` diagnostic gap
(§6-A.3) is tracked with its own item above.

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
- [x] **FP Annex F wide-return intermediate precision (2026-07-07).** Added
  `exec/features_c99_c11/fp_wide_return.c` (3-way validated; runs the exec /
  exec-replay / exec-replay-tmpl / diff3 columns): a float/double return must
  remove extra range/precision — `FLT_MAX+FLT_MAX` / `DBL_MAX+DBL_MAX` narrow to
  +inf and a float-rounded quotient stays narrowed. Trivially holds on the
  FLT_EVAL_METHOD==0 targets (x86_64 SSE, arm64, riscv64) and exercises real x87
  narrowing on i386 (FLT_EVAL_METHOD==2) via the qemu matrix. `FLT_EVAL_METHOD`
  itself stays covered by `flt_eval_method.c`. _Ref:_ gcc `gcc.dg/c11-float-*.c`,
  clang `C11/n1365.c`, `C11/n1396.c`.
- [~] **`_Complex` diagnostics + Annex G special values.** _Annex G / CMPLX edges
  now covered_ by `exec/features_c99_c11/complex_cmplx_special.c` (3-way validated,
  runs the exec/exec-replay/exec-replay-tmpl/diff3 columns): CMPLX/CMPLXF/CMPLXL
  exactness with NaN/inf parts (n1464), `cabs` inf-part→+inf, `conj` exact sign
  flip, `cproj` inf→(+inf, copysign(0,imag)) — all matching gcc/clang. *Residual:*
  `_Complex` constraint *diagnostics* (folds into the negative-test-tier item
  below). _Ref:_ clang `C11/n1464.c`, `C11/n1514.c`; gcc `gcc.dg/c99-complex-{1,3}.c`.
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
