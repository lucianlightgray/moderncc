# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

# Now

## CST database — next-phase gap closure (docs/CST.md D1–D5)

All original vertical slices (S0, B–J, WEAVE 2, FINAL) landed and are gated in
CTest; the record lives in
[docs/NOTES.md § Completed work](NOTES.md#completed-work--cst-database-all-vertical-slices-landed).
`MCC_CST` is built **on by default** (`CMakeLists.txt:1087`); codegen is
byte-identical CST-on vs CST-off. The CST hooks are pure side-effect recording —
the compiler never reads the CST — so the §8.5 codegen-identity invariant holds
for *any* hook change; the live risk is CST round-trip/tiling correctness, gated
by the `cst/*` ctest suite. Driving the [docs/CST.md](CST.md) decision plan:

- [x] **D1a — Expression fill-in (`Unary`/`Cast`/`Paren`/`Primary`).** Retroactive
  range-wrap in `unary()` (mccgen.c): prefix-op → `Unary`, `(type)e` → `Cast`,
  `(e)` → `Paren`, atoms → `Primary`. Gated `cst/kinds-expr`.
- [x] **D1b — Declaration structure via D2 range-wrap** (`Declaration`,
  `FunctionDef`, `ParamList`, `Enum`, `TypeName`, `Initializer`, `Label`).
- [ ] **D1c — PP-concrete** (`IncludeDirective`, `PPDirective`, `PPConditional`),
  full-concrete: capture *all* `#if`/`#else` branches as concrete nodes. Prereq
  for D3.
- [ ] **D3+D5 — `SourceFile` template + renderer.** Content-addressed pure-`H_s`
  hash-consing, per-instance binding, `render(template, binding)` fold with
  threaded PP environment. Headline gate: recursive re-include branch selection.
- [ ] **D1d — `Comment` promotion** (line/inline/block), `H_t`-only so §8.4 holds.
- [ ] **FINAL** — re-run every gate over the corpus; re-confirm §0.1/§0.2.

---

# Later

- [ ] **`exec/tls` skipped on arm64+WIN32 (`skipon=arm64/WIN32`, 2026-07-05).**
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
- [ ] **Reconcile divergent test-count claims across docs (validate).**
  `docs/NOTES.md` "Build status" (39/39, 22/22 and 782/782 / 520/520) vs its
  Profiling §7 validation matrix (804/772) cite different totals with no stated
  basis (all three moved out of README/PROFILING in the 2026-07-06 reorg).
  → Regenerate from one `ctest -N` per host/preset and state the per-case vs
  aggregate counting basis; make the docs cite the same source of truth.
- [ ] **Trace the "~100× faster than gcc -O2" headline to a measurement (validate).**
  `README.md:15` says "~100×"; `docs/NOTES.md` Profiling §4b measures 118–204×
  (TU/opt dependent); the NOTES.md "Compile speed & footprint" table shows
  108–141×. → Pick the documented benchmark and make the headline a measured
  range, not a round number.
- [ ] **Re-measure & date-stamp the README speed/size table post-lexer-change
  (validate).** The `docs/NOTES.md` "Compile speed & footprint" table (0.05 s;
  7/19/108/141×) and the ~0.6 MB / ~1.3 MB size claims (`README.md:16` + that
  table) predate the `TOK_HASH_SIZE` change and are toolchain/host-sensitive. →
  Re-run `mccbench` + `size`/`strip` a `dist-*` build; refresh, noting the host
  as PROFILING does.
- [ ] **PROFILING §4–§5 hot-path %/timings predate the `TOK_HASH_SIZE`
  16384→65536 change (validate).** §8 (dated one day later) changed the lexer, so
  the `next_nomacro` self-% and `-E` timings now in `docs/NOTES.md` Profiling
  §4–§5 may be stale. → Re-run the PROFILING §3 method + §4–§5, or annotate them
  as pre-change baselines.
- [ ] **Regenerate the dated "all green" status prose from CI (validate).**
  `docs/NOTES.md` "Build status" (moved from README) narrates per-preset
  pass/skip counts across ~35 presets; this rots silently. → Derive from the
  latest workflow run, or add a check that fails when the prose diverges from
  actual CTest output.
- [ ] **`atomic_fetch_add/sub` on `_Atomic` pointer types is rejected (impl).**
  `C9911.md:3460` §7.17.7.5p2: mcc errors ("integral or integer-sized pointer
  target type expected"); clang scales by pointee size. → Add pointer-operand
  handling (ptrdiff_t semantics); test vs clang's element-scaled result.
- [ ] **`<threads.h>` resolves to the bundled pthread shim, not the host header
  (fix).** `C9911.md:4900` §7.26.1p3 — root cause of the C11-threads divergences
  (`_Noreturn thrd_exit`, `thrd_sleep` return contract, `TIME_UTC` gating). →
  Prefer the host `<threads.h>` when present, or align the shim's decls; add tests.
- [ ] **`va_start` non-last / `register` param check never fires on x86_64
  (impl).** `C9911.md:3215` §7.16.1.4p3 — the SysV macro never references `parmN`,
  so the (already-warned elsewhere) misuse diagnostic is absent on the primary
  target. → Move the check into the semantic layer so it fires target-independently.
- [ ] **`const`-lvalue `++`/`--` and same-type nonscalar casts only warn (fix).**
  `C9911.md:1032/1063` (§6.5.2.4/§6.5.3.1) and `:1104` (§6.5.4p2). Mirrors the core
  comment at `src/mccgen.c:3462` ("assignment of read-only location" is a
  warning). gcc/clang error. → Promote read-only-modify to a constraint error;
  honor `-pedantic-errors` for the nonscalar cast.
- [ ] **Add `inline int main` / internal-linkage-in-inline diagnostics (impl).**
  `C9911.md:1524-1525` §6.7.4p2/p3 — low-risk diagnostic-only additions.
- [ ] **Document (or bundle) the missing freestanding `<math.h>` (fix).**
  `C9911.md:2708` §7.12 — no `runtime/include/math.h` (confirmed); relies wholly
  on host libm, so a non-glibc/freestanding host has no `<math.h>`. → Note the
  host-libm dependency in README/BUILD, or ship a minimal header.
- [ ] **Surface the arm64-Darwin `long double == double` quirk in public docs
  (validate).** `README.md:356-358` presents arm64 Darwin as fully covered; the
  `MCC_USING_DOUBLE_FOR_LDOUBLE` aliasing (maintainer memory only) is a real
  conformance caveat. → Document where arm64-Darwin support is claimed; assert the
  intended `long double` behavior in a test.
- [ ] **Cross-check the three divergent TLS-offset conventions per psABI
  (validate).** x86_64 subtracts the aligned block (`x86_64-link.c:377`), arm64
  adds a bare `+16` TCB magic constant (`arm64-link.c:369`), riscv64 uses raw
  `val - tls_start` with **no** bias (`riscv64-link.c:355`). → Confirm each matches
  its psABI variant; add a `__thread` (zero- and nonzero-init) correctness test per
  arch, esp. riscv64; name the arm64 constant.
- [ ] **Implement 64-bit bit-field width (impl).** `src/mccgen.c:4485`
  `mcc_error("field width 64 not implemented")` rejects a valid `:64` bit-field on
  an LP64 base type (appears in real headers). → Implement, or document as a hard
  limit.
- [ ] **Support forward `__alias__` targets (impl).** `src/mccgen.c:10379`
  "unsupported forward __alias__ attribute" — gcc allows aliasing a not-yet-defined
  symbol. → Defer alias resolution to an end-of-TU fixup pass.
- [ ] **Widen or hard-error `__mode__(...)` coverage (fix).** `src/mccgen.c:3943`
  warns and **ignores** unlisted modes, silently mistyping (e.g. `DI`/`TI`). →
  Confirm the supported set covers the SDK/runtime headers; add `DI` (and `TI`
  where the ABI has 128-bit) or promote unknown modes to an error.
- [ ] **External (SHN_UNDEF) thread-local symbols hard-error on Mach-O (impl).**
  `src/objfmt/mccmacho.c:2085` "unsupported". → Implement TLV import descriptors, or
  document as an intentional limitation.
- [ ] **Parse 64-bit Mach-O fat archives (impl).** `src/objfmt/mccmacho.c:2380`
  rejects `FAT_MAGIC_64`/`FAT_CIGAM_64` (only 32-bit fat headers parsed); modern
  toolchains emit 64-bit fat. → Parse `fat_arch_64` entries.
- [ ] **ARM far-branch has no veneer — errors past ±32 MB (fix).**
  `src/arch/arm/arm-gen.c:329` `"FIXME: function bigger than 32MB"`. → Emit a
  long-branch trampoline/island, or downgrade to a documented diagnostic (not FIXME).
- [ ] **i386 fastcall/thiscall: non-register arg before a register arg
  unsupported (impl).** `src/arch/i386/i386-gen.c:530`. → Handle the
  spilled-then-register ordering, or document the accepted ABI limitation.
- [ ] **Unify + extend mixed-encoding-prefix string concatenation (fix).**
  `src/mccgen.c:9315` and `:9553` duplicate the "different encoding prefixes"
  error. → Deduplicate into one helper; decide which C11 §6.4.5p5 combinations to
  accept (gcc/clang accept more).
- [ ] **Validate the x86_64/i386 TLS GD/LD and 32[S] pattern-match assumptions
  (validate).** `x86_64-link.c:303/317/202` and `i386-link.c:201/240` abort on
  "unexpected …pattern" / out-of-range — tight codegen↔linker coupling. → Add
  regression tests covering GD/LD/IE/LE forms and a large-address case; pin the
  expected code sequences with a comment so a codegen change is caught.
- [ ] **ARM inline-asm `long long` operands unimplemented (impl).**
  `src/arch/arm/arm-asm.c:2465` hard-errors — handle the 64-bit register-pair case.
- [ ] **arm64 inline assembler errors on unmodeled mnemonics (impl).**
  `src/arch/arm64/arm64-asm.c:1877` (+ `:1298/:1441/:1651`). → Enumerate the common
  missing mnemonics; expand the table or document the supported subset.
- [ ] **Resolve/remove the 6 permanently-masked ARM asm encodings (fix).**
  `ARM_KNOWN_FAIL` (tools/mccharness.c:2549) never fails on `bl r3`, `b r3`,
  `mov #0xEFFF`, `mov #0x0201`, two `vmov.f32` forms — real encoding defects. → Fix
  the `mov #imm`/`vmov.f32` cases and drop the entries.
- [ ] **`.cfi` ops per function are a fixed cap (fix).** `src/mccasm.c:974`
  `ASM_CFI_MAX` hard-errors on large hand-written/generated unwind tables. →
  Validate headroom or make the buffer growable.
- [ ] **`gcctestsuite` tallies failures but always returns 0 (validate).**
  tools/mccharness.c:1243 — the GCC-testsuite sweep cannot gate CI. → Confirm it is
  intentionally non-gating (document it) or return nonzero past a baseline budget.
- [ ] **`gcctestsuite` skip heuristic is a whole-file substring match (fix).**
  `gccts_skiplisted` (tools/mccharness.c:1111) drops any file whose *contents*
  mention `complex`/`vector`/`__int128`/`_builtin_` anywhere (comments/strings
  included). → Tighten to token/decl matching or an explicit skip list.
- [ ] **Log the preprocess "matches EITHER reference" cases (validate).**
  tools/mccharness.c:841 — the 2-way fallback assumes any gcc/clang divergence is
  impl-defined; a case where mcc coincidentally matches the wrong reference scores
  PASS. → Log which cases take this branch so divergences can be reviewed.
- [ ] **`ckbuildmd` type-drift check is presence-only + prefix-matched (fix).**
  tools/ckbuildmd.c:98 only checks type when the cell starts with a TYPEKW, and
  `strncmp` lets `INT` match `INTEGER`. → Treat documented-but-mistyped as drift;
  use exact type equality.
- [ ] **JUnit summarizers count `notrun`/`<skipped>` as skips (validate).**
  tools/ci.c:1200, tools/bench.c:395 — a fixture-setup failure surfacing as
  `notrun` would be under-reported as a benign skip. → Confirm ctest emits
  `<failure>` for setup failures.
- [ ] **`hostgate` scans only `.c`/`.h` (validate).** tools/hostgate.c:84 — the
  "no raw host macros outside mcchost.{c,h}" invariant misses `.S`/`.inc`/generated
  sources. → Confirm none use raw host macros, or extend the walk.
- [ ] **Reference-harness `exec`/`diff3` goldens are effectively dead (validate).**
  `tests/exec/goldens.h:19/53/54/62` (inline multi-unit, backtrace, btdll, alias)
  carry full expected output but SKIP for lack of a reference harness. → Confirm
  each is exercised elsewhere (mcctest/diff); otherwise wire up the harness.
- [ ] **Re-enable or delete the disabled bit-field-layout struct test (impl).**
  `tests/diff/parts/legacy_aggregates.h:824` `#if 0` "until further clarification
  re GCC compatibility" — mcc's layout for that mixed int/char bit-field shape is
  untested. → Resolve the GCC-compat question and re-enable, or remove with rationale.
- [ ] **Whole-array assignment: decide implement vs. keep xfail (impl).**
  `tests/exec/goldens.h:161` `array_assignment` (GNU extension) has a ready golden
  waiting behind `note:unsupported`. Cross-refs the exec-suite audit above. →
  Implement and activate the golden, or record as intentionally unsupported.
- [ ] **`__has_builtin`/`__has_feature`/… hard-coded to 0 (validate).**
  src/mccpp.c:1521 — SDK headers may mis-detect features mcc actually provides. →
  Answer truthfully where cheap (e.g. `__has_attribute` for honored attributes);
  document the 0-default.
- [ ] **`mcc -ar` rejects `[abdiopN]` positional flags (impl/doc).**
  src/mcctools.c:22 handles only `[crstvx]`; build systems using insert modes
  break. → Implement `a`/`b`/`i`, or document the supported subset clearly.
- [ ] **Windows keeps diagnostic color off unconditionally (validate).**
  src/mcchost.c:21 — suppresses color even on VT-enabled Windows Terminal. →
  Probe `ENABLE_VIRTUAL_TERMINAL_PROCESSING`; confirm `-fdiagnostics-color=always`
  still forces it. Low priority.
- [ ] **Add a regression test for cross-TU `_Complex` memo dangling-sym clearing
  (validate).** src/mccgen.c:643 asserts the complex-type cache is cleared in
  `mccgen_finish` to avoid reusing syms into a freed `global_stack` across TUs on
  one persisted `MCCState`. → Compile two `_Complex`-using TUs through one
  embedder `MCCState` under ASan.
- [ ] **Static-assert exactly one backend is compiled (validate).**
  `src/mcc.h:984` collapses per-function backend state onto shared `cg_*` fields
  "because only the active target is compiled". → Add a build-time check guarding
  that assumption.
- [ ] `-fverbose-asm`-style operand comments: meaningful comments need
  codegen-side variable/spill metadata that is discarded after emission;
  classified low-value (reloc symbol names are already printed). Revisit
  only if a debugging workflow materializes that needs it.

---

ACHTUNG!!! DO NOT DO!!! WARNING!!!

* Change MCC_VERSION everywhere to be YYYYMMDDHHMMSS (use two int's in code for major YYYYMMDD and minor HHMMSS)
* Normalize as much of the CMake code as possible: minimize gating instead preferring autodetecting the existence of tools and enabling as many tests/targets/configs as are available on the host
* Use only human friendly warnings/errors, backed by tests that check formatted output against terminal dimensions/configuration
* Can a fully static build use a minimalistic `-run` to sidestep the dynamic linking limitations and use libc or musl in-memory?
* Implement/finish `-g` debugging/debugger and flesh out gdb/etc test cases, check against gcc and clang sources of truth
* Optimization -O1...100 levels measured in max seconds to spend optimizing?
* Hot reload by saving/loading CST snapshots on the fly and on run with --hotreload arg
* Run hot-reloads from reconciled CST snapshots

ACHTUNG!!! DO NOT DO!!! WARNING!!!

---
