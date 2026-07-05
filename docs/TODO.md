# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Open items (priority order)

_From CI run [28741671440](https://github.com/lucianlightgray/moderncc/actions/runs/28741671440)
(`0e700c7`, push 2026-07-05) — overall **green** (0 failures across linux×18,
macos×4, msvc×2, mingw, qemu×10, dist×9). These are the warnings / skips / matrix
open questions that green run left standing._

### exec/tls — MSVC arm64 backend miscompiles mcc (known issue)

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
  - _Next:_ re-enable once on an arm64 Windows + MSVC host (or a newer MSVC);
    then bisect the miscompiled mcc construct and, if it is `/O2`-specific, wrap
    just that function in `#pragma optimize("",off)`.

### macho-* on native macOS

- [~] **`macho-*` native macOS — partially enabled (2026-07-05).**
  Root cause found and fixed for the structural test: the four tests gated on
  `EXISTS "${MCC_CROSS_DIR}/mcc-x86_64-osx"`, a **configure-time** file check that is
  always false during a self-contained `MCC_ENABLE_CROSS` build (the cross compiler
  is built later, by that same build). Changed the gate to accept the in-tree
  `TARGET mcc-x86_64-osx` too — **scoped to Darwin** (`_have_osx_cross` in
  CMakeLists.txt) so Linux/`cross` behaviour is left exactly as before (the
  exec-based macho harnesses run x86_64 code via a custom loader and I could not
  verify them on an x86_64-Linux runner from this host). On the `macos-cross` jobs
  (which build `mcc-x86_64-osx` in `all`) the tests are now registered instead of
  dropped at configure.
  - **`macho-structural` now RUNS natively on macos-cross** (verified: PASS on
    macos-arm64) — it is structural-only (parses the emitted Mach-O with `objcheck`,
    host-independent), so no x86_64 execution is needed.
  - **`macho-codegen-run` / `macho-image-run` / `macho-apple-libc` still SKIP**, now
    at runtime with `SKIP: host is not x86_64`: each *executes* the emitted
    **x86_64** Mach-O and is hard-gated by `host_is_x86_64()` (compile-time
    `__x86_64__`) in `tools/mccharness.c`. On the Apple-Silicon `macos` runners
    (arm64, Rosetta not installed on the non-dist jobs) this is correct. Remaining
    work to cover them natively: either (a) install Rosetta 2 on the macos-cross
    jobs and relax `host_is_x86_64()` to a runtime "can we exec an x86_64 Mach-O"
    probe, or (b) add native **arm64-osx** variants (`mcc-arm64-osx` +
    `--arch arm64`) so the run path exercises the native slice with no emulation.
    The plain (non-cross) `macos` jobs still cover native Mach-O end-to-end via the
    unconditional `macho-conformance-native` + `macho-stack-protector` +
    `macho-universal` tests (all PASS).

## CI skip-validation audit (2026-07-05, run 28741671440)

**Guiding principle:** a SKIP must be justified *per platform*. Suite-level gates
(e.g. "needs gcc **and** clang", "needs POSIX shell", "native host only") currently
drop whole groups on macOS even when an individual test's **exec-only or 2-way
(mcc-vs-clang) subset** would run fine, or when the test is natively applicable to
this arch. For every entry below: confirm the gate is genuinely unsatisfiable on
that platform; **if any subset can run, split it out and run it** rather than
skipping the whole test. 106 unique skipped tests observed across the 4 macOS jobs.

- [x] **diff3 suite — AUDITED; premise corrected (2026-07-05).** The TODO's premise
  ("on macOS gcc *is* Apple clang → suite dropped") is wrong for the CI macos jobs:
  they brew-install a distinct **GNU gcc-16**, so the 3-way runs. Read the runner
  (`tests/diff3/runner.c`): a `gcc != clang` divergence is **already** classified
  impl-defined and PASSES (line ~350) — it is *not* a skip source. So the remaining
  per-case macOS skips are, verified with the runner:
  - **`ref-cant-build`** — a reference compiler (brew gcc-16 or Apple clang) can't
    compile that case on Darwin (Darwin header/flag limitations): `alignas`,
    `cleanup`, `old_func`, `c11_threads`, `arm64_extasm`, … A fair differential is
    impossible when a reference won't build; the `exec/` golden already runs these
    natively (e.g. `exec/arm64_extasm` PASSES on macos-arm64), so codegen *is*
    covered — only the 3-way byte-diff is unavailable. Not an mcc gap.
  - **`__MCC__` sources** — `scopes`, `struct_init`, `floating_point_literals` carry
    an `#ifdef __MCC__` section, so gcc/clang compile a different program; the runner
    marks them MCC-ONLY and skips on **every** platform (not Darwin-specific).
  - **arch/ISA-gated** — `asm_goto`/`asm_*_x86`/`al_ax_extend`/`fastcall` (x86),
    `riscv_asm` (riscv), `winarm64_interlocked` (win-arm64): correct to skip.
  No safe per-case win remains without repairing the reference-compiler builds on
  Darwin (fragile, and orthogonal to testing mcc).

- [x] **exec suite — per-test runner gates: CONFIRMED INTENDED (2026-07-05).**
  Audited every `exec/` case that SKIPs on macos-arm64 against the golden `flags`
  field in `tests/exec/goldens.h` and the gate in `tests/exec/runner.c`. All are
  correctly gated, none is a macOS-specific miss:
  - Arch/ISA-specific inline asm — `flags` carries `cpu=x86` / `cpu=x86_64` /
    `cpu=i386` / `cpu=riscv64` / `cpu=arm64,os=WIN32`: `asm_goto`, `asm_lvalue_cast`,
    `asm_operand_modifiers`, `asm_constraints_x86`, `al_ax_extend`, `fastcall`,
    `riscv_asm`, `winarm64_interlocked`. Cannot run on Darwin/arm64 by construction.
  - ELF-only GAS features — `flags` carries `asm,elf`: `asm_data_directives`,
    `asm_sections` (Mach-O has no equivalent for those `.section`/data directives).
  - Reference-harness placeholder goldens — `flags` carries `note:…`, which the
    runner skips on **every** platform (they also SKIP on x86_64 Linux):
    `inline` (multi-unit symbol-export harness), `alias` (multi-unit alias harness),
    `backtrace`/`btdll` (backtrace reference harness), `array_assignment` (xfail
    whole-array assignment). Not a Darwin gap — they need a reference harness that
    does not exist on any target yet.

- [ ] **cli suite — gated by `cli-suite "structural readelf/nm suite runs on native host only"` / ELF-isms.**
  Many are genuinely ELF/Linux-only (`shared_dyn_soname`, `rpath_new_dtags_runpath`,
  `tls_segment_and_run`, `fpic_pie_dyn`, `fno_pic_exec`, `section_attribute`,
  `visibility_attribute`, `fvisibility_hidden_default_wins`, `stack_protector_on/off`,
  the `debug_*`/`dwarf*`/`stabs` group). But several are format-agnostic and should
  have a Mach-O equivalent on macOS: `dumpmachine`, `uchar_header`,
  `builtin_nan_inf_const`, `builtin_signbit_no_trap`, `complex_creal_function`,
  `constructor_init_array` (→ `__mod_init_func`), `leading_underscore` (Mach-O
  *does* lead with `_`), `symbol_type_func_object`, `assemble_dot_s_file`,
  `dash_S_emits_assembly`, `common_symbol_merge`, `weak_override_multi_tu`,
  `atomic_*`, `nostdinc_drops_system`. Validate each; port the format-agnostic ones
  to run on Darwin:
  - _Port mechanism:_ each case in `tests/cli/cases.h` skips via its requirement
    string (`"cpu=x86_64,os=linux"` → `os_eq(os,"linux")` in `tests/cli/runner.c`).
    A port loosens the requirement to `os!=WIN32` (runs on x86_64+arm64 Linux and
    macOS, still excludes PE); where the `expect` pipeline greps ELF/x86_64 literals
    it is made target-adaptive (the runner exports `MCC_TEST_CPU`/`MCC_TEST_OS`, so
    it can branch to `nm`/`otool` on Darwin). Every ported case keeps its ELF path
    byte-identical so x86_64 Linux does not regress.
  - **Ported 2026-07-05 (11 tests, SKIP→PASS on macos, verified locally):**
    - [x] `cli/uchar_header` `cli/builtin_nan_inf_const` `cli/builtin_signbit_no_trap`
      `cli/complex_creal_function` `cli/nostdinc_drops_system`
      `cli/weak_override_multi_tu` `cli/common_symbol_merge`
      `cli/function_data_sections_accepted` — gate relaxed only (identical output).
    - [x] `cli/dumpmachine` — now asserts an arch-prefixed triple (`TRIPLE_OK`)
      instead of the literal `x86_64`, valid on every native target.
    - [x] `cli/symbol_type_func_object` — Darwin branch uses `nm` (T→FUNC, D→OBJECT);
      Linux keeps the identical `readelf -s` branch.
    - [x] `cli/dash_S_emits_assembly` — greps made underscore-agnostic (`^_?answer:`)
      so the Mach-O `_answer:` label matches; mcc emits `.text` + `@function` on both.
  - **Confirmed ELF/DWARF-only (correct to skip on Darwin), left gated:**
    the `debug_*`/`dwarf*`/`stabs` group, `fno_pic_exec`/`fpic_pie_dyn`/
    `shared_dyn_soname`/`shared_dynamic_tags`/`rpath_new_dtags_runpath`/
    `relocatable_partial_link`/`rdynamic_exports_main`/`section_attribute`/
    `visibility_attribute`/`fvisibility_hidden_default_wins`/`strip_symbols`/
    `stack_protector_on`/`stack_protector_off`/`tls_segment_and_run`/
    `fcommon_vs_default` (ELF nm B/C letters), `constructor_init_array` (ELF
    `.init_array` section), `atomic_inlang_aggregate`/`atomic_rmw_unsupported`
    (tagged `elf`), `assemble_dot_s_file` (x86 `.s`), `leading_underscore`
    (ELF-specific flag semantics; Mach-O leads with `_` by default).

- [x] **preprocess 3-way cases — RESOLVED (2026-07-05).**
  `suite_preprocess` (tools/mccharness.c) required a gcc==clang *consensus* before
  checking mcc, so cases where the references diverge (brew gcc-16 vs Apple clang on
  Darwin) were dropped. Added a 2-way fallback: when gcc≠clang, mcc PASSES if it
  matches **either** reference (conformant with at least one mainstream compiler);
  matching neither stays a SKIP, so the change never introduces a FAIL and is safe
  on every platform. Recovered 5 of the 6 macOS preprocess skips (incl.
  `directives_in_args`, `variadic/gnu_comma_paste`) SKIP→PASS.
  - [x] `preprocess/conditional/directives_in_args`
  - [x] `preprocess/variadic/gnu_comma_paste`
  - [x] `preprocess/expansion/standard_example` — **CONFIRMED mcc-CORRECT, not a gap
    (2026-07-05).** This is the canonical C standard §6.10.3.4 macro-expansion
    example. Diffing all three: the *only* difference is one cosmetic space — mcc
    emits `f(2 * (2+(3,4)-0,1))`, while gcc-16 **and** clang emit `2 +(3,4)` (a space
    after the object-like `x`→`2` expansion). **mcc matches the C standard's own
    printed example** (`2+`, no space); gcc/clang add impl-defined whitespace around
    the expansion. Inter-token whitespace in `-E` output is impl-defined (§6.10p1),
    so all three are conforming — and mcc is if anything the closest to the standard
    text. The harness correctly SKIPs (verified `FAIL=0`); changing mcc to match
    gcc/clang would diverge it from the standard and risk the other preprocess
    goldens. No action.

- [x] **standalone / cross-target tests — AUDITED (2026-07-05).** Each confirmed
  either correctly gated or already covered elsewhere:
  - **Already run on `macos-cross`** (skip only on the *non-cross* `macos` job,
    which has no cross compilers — correct): `dash-s-bytes-arm64`,
    `dash-s-bytes-riscv64` (verified PASS on macos-cross; gated on `TARGET
    mcc-<arch>`, i.e. the byte-exact `-S` output of the arch cross compiler).
  - **x86-specific integrated-assembler tests** (correct to skip on arm64):
    `asm-c-connect-test` ("requires x86 target"), `dash-s-roundtrip` ("requires
    x86_64 target"), `asm-gas-directives` (blocked even on x86 — the integrated
    assembler lacks `sgdtq`/`sidtq`/`swapgs` privileged encodings),
    `i386-fastcall-abi` (i386 ABI).
  - **PE / cross-target** (correct to skip on a non-Windows host): `compile.win32`,
    `pe-native-conformance` (native WIN32 host only), `pe-wine-conformance` (needs
    wine + the win32 cross compilers).
  - [x] `mcctest` / `mcctest-bcheck` — **CONFIRMED INTENDED (2026-07-05).** The
    macos-gcc / macos-cross-gcc jobs resolve `CC` to a real Homebrew **GNU gcc**
    (not Apple clang — the job does `gcc-<N>` off `brew --prefix`), and the
    `mcctest` differential compares mcc byte-for-byte against that reference.
    `CMakeLists.txt` skips it on Darwin because a GNU gcc reference diverges from
    mcc (which matches the Apple/clang ABI + impl-defined choices) on
    impl-defined / UB corners; the Apple-clang `mcctest` (macos-clang jobs) plus
    the diff3-suite already cover that surface. Decision, not a gap.

- [x] **Larger-input lexer benchmarking + hot-cluster attack — DONE (2026-07-05).**
  Set up a large-TU lexer benchmark with `hyperfine` (release mcc built by
  `clang -O2`) on identifier-dense 2 MB / 60k-line TUs — σ ≈ 1.5–2 %, enough signal
  to separate a small lexer change from noise (the 416-line `full_language.c`
  couldn't). (The amalgamation `-E src/mcc.c` hit a nested-quoted-include
  resolution quirk when >2 `-I` dirs are passed on the command line; the synthetic
  identifier-heavy TU is a cleaner, more direct stress of the §5a intern-hash
  cluster anyway.)
  - **Attacked the intern-hash cluster via `TOK_HASH_SIZE` (the §5a lever) and
    applied it:** raised `TOK_HASH_SIZE` 16384 → 65536 in `src/mcc.h`. Rigorous
    `hyperfine` A/B: **1.06 ± 0.04× faster** at high load factor (pathological, 60k
    unique idents) and a consistent **1.03 ± 0.02× faster** at a realistic load
    factor (3k unique, heavily reused) — statistically significant, with lower σ.
    Intermediate 32768 gave no realistic-density gain (a threshold effect), so 65536
    is the right step. The cost is a **fixed** +384 KB hash table (128 KB → 512 KB)
    that does *not* scale with input, so it leaves mcc's low-RSS profile intact.
    Correctness is unaffected (65536 is still a power of two for the
    `h & (TOK_HASH_SIZE-1)` mask); full `macos` suite 811/811 after the change.
  - `TOK_HASH_FUNC` (per-byte hash cost) left as-is: it is already a cheap
    shift-add-mix; changing it risks worse distribution for no measured gain.

Resolved 2026-07-05 (run 28741671440 follow-ups): **CI Node.js 20 deprecation**
(`actions/cache@v4` → `@v5`, Node-24 native; every other action already on
checkout@v5 / artifact@v7); **bench aarch64 CPU model** (`decode_arm_midr` in
`tools/bench.c` maps `/proc/cpuinfo` `CPU implementer`+`CPU part` → friendly name,
e.g. `ARM Neoverse-N1`, with vendor/hex fallback); **bench dead JUnit lookup**
(`write_tests` now emits nothing when no XML is present instead of the misleading
"no JUnit file at …" line); **linux + qemu job summaries** (new
`ci junit-summary <xml>` verb renders a PASS/FAIL/SKIP markdown table into
`$GITHUB_STEP_SUMMARY`; `ci run-preset`/`ci qemu` now pass `--output-junit` so the
18-cell linux matrix and 10 qemu cells surface their ctest results — benchmark
report intentionally omitted under qemu-user, where emulated timings are noise);
**exec-suite skip audit** and **macos-gcc `mcctest` skip** confirmed intended
(above); **macho-structural native**, **11 cli-case ports**, **preprocess 2-way
fallback**, the **diff3 / standalone / exec skip audits**, and the **lexer
`TOK_HASH_SIZE` optimization** (16384→65536; measured 1.03–1.06× faster `-E`)
(above).

Presets exercised locally (beyond CI), all 0 failures unless noted — `macos` and
`macos-cross` natively, the rest via Docker `ubuntu:24.04` on the same `ci
run-preset`/`ci qemu` path CI uses (also exercising the new `--output-junit` /
`ci junit-summary` wiring):
- `macos` 811/811, `macos-cross` 810/810 (Mach-O/arm64)
- the full linux config matrix on aarch64 ELF, all 0 failures: `linux-gcc`,
  `linux-gcc-release` 811/811, `linux-gcc-sanitize` 811/811 (no UB in the changed
  harness), `linux-gcc-diagnostics` (everything-on warnings: 0 in any changed
  file), `linux-gcc-static`, `linux-gcc-multisource`, `linux-gcc-predefs-off`,
  `linux-gcc-pie`, `linux-gcc-dwarf`, `linux-clang-cross` (all 811/811), and
  `linux-gcc-asm-off` (779/779 after the arm64-asm gating fix below)
- **the full 10-cell qemu-user matrix** — all 5 arches × both libcs
  (x86_64, i386, arm, arm64, riscv64 × glibc, musl), every cell 0 failures
  (the 32-bit and x86 cells run under nested emulation on the arm64 host);
  validates the qemu `--output-junit` + `ci junit-summary` path
- `dist-macos` (native, build rc=0), `dist-linux-gcc` + `dist-linux-clang` (Docker
  aarch64, build rc=0) — the release-packaging presets
- `cli` (11 ports) + `preprocess` (42) re-run on both aarch64 and x86_64 Linux
- _Environment-limited, not code:_ `linux-gcc-musl` and the first `dist-linux-gcc`
  attempt failed only because the barebones container lacked `ca-certificates` for
  the TLS `git clone` of musl/vendor sources — installing it makes both succeed
  (root cause confirmed). The other qemu arches (x86_64/i386/arm/riscv64) run under
  nested emulation; the remaining linux config variants are permutations; **msvc /
  mingw / dist-msvc / dist-mingw need a Windows host**. These are the CI matrix's job.

Three errors surfaced by this local preset testing were fixed: a
`-Wformat-truncation` in `ci.c`'s junit buffers; `bcheck.c`'s unconditional
`regparm` `FASTCALL` (x86-only; broke host-clang builds off-x86 — now guarded to
i386/x86_64); and the arm64 inline-asm goldens (`arm64_encoding`/`_errors`/
`_extasm`) which declared only `cpu=arm64` and so failed on an arm64 host with the
integrated assembler off instead of skipping — now gated `cpu=arm64,asm`. `decode_arm_midr` validated against a real aarch64 `/proc/cpuinfo`
(`CPU implementer 0x61` → "Apple"; no `model name` field, confirming the original
`cpu : ?`).

Verification for the test-enablement batch, on **three platforms** (0 failures):
`macos` 811/811 and `macos-cross` 810/810 locally (Mach-O/arm64); the 11 cli ports
+ full 42-case preprocess suite re-run under Docker `ubuntu:24.04` on **native
arm64 Linux** *and* **x86_64 Linux** (ELF) — 12/12 cli, 42/42 preprocess on each.
So the widened `os!=WIN32` gate and the preprocess 2-way fallback are confirmed on
both POSIX arches and both object formats, and the original x86_64-Linux ELF path
does not regress. (The amd64 gcc occasionally segfaults under QEMU on this arm64
host — a `ninja -k 0` retry cleared the transient crash.)

Resolved 2026-07-04: arm64 atomics/bounds/complex link
failures (outline-atomics + `__unordtf2`), Rosetta x86_64 macOS (validated
end-to-end locally under Rosetta — build, `-run`, ctest subset; CI confirms on
push), the Mach-O fat-binary suite (landed with the universal-binary work), and
the conformance batch (see Notes). Broader conformance tracking continues in
`docs/C9911.md`.

## Notes

- **Conformance (2026-07-04).** Now default errors (downgradable via
  `-Wno-error=`): return-mismatch (§6.8.6.4p1) and implicit-function-declaration
  (§7.21.7.7); `va_start` non-last-param (§7.16.1.4p3) is warned. Two further
  candidate flips were evaluated and **intentionally not made** — they contradict
  mcc's demonstrated design, so they are decisions, not gaps: (a) *implicit-int on
  K&R params → error* — mcc deliberately **supports** K&R old-style functions (the
  `#if __MCC__` block in `tests/diff/parts/legacy_preproc.h` exists specifically to
  test mcc compiling them, where gcc/clang reject them), so it stays a warning; a
  whole-`warn_implicit_int` flip also over-reaches (breaks `cli/implicit_int_diag`
  + `exec/errors_and_warnings`). (b) *inline-only linkage (§6.7.4p6)* — mcc
  inlines-and-links an inline function with no external definition, which the
  standard *permits*; strictly more useful than gcc/clang's `-O0` link failure, so
  it stays.

- **`static` reentrancy sweep (done; from the former `STATIC.md`).** Every
  per-compilation mutable static in `src/` was moved off static storage —
  per-call scratch onto the stack, persistent per-instance state rehomed into
  `MCCState` (the `#define name mcc_state->name` idiom). `mccgen.c` and `mccpp.c`
  now have **zero** mutable statics; per-function backend state (`arch/*-gen.c`)
  collapsed onto shared `cg_*` fields since only the active target compiles.
  Verified `debug` + `cross` (all five backends), 804/804.
  - *Deliberately retained as process-global* (moving into `MCCState` would only
    relocate a global — the async/loader contexts reach them with no `MCCState`):
    async signal/fault handler state (`mccrun.c` `g_rc`/`g_s1`/`signal_set`,
    `mcchost.c` `host_fault_cb`); the Windows DLL loader `mcc_module`;
    process-wide allocator accounting (`libmcc.c` `mem_debug_chain`/`mem_cur_size`/
    `mem_max_size`/`nb_states`/`reallocator`); immutable cached singletons
    (`libmcc.c` `auto_mccdir_buf`, `mcchost.c` `host_macos_sdk_root`, `mcc_syms[]`).
    Do not "fix" these in a future sweep.
  - *Bugs fixed along the way* (both covered by tests, in git history): complex-type
    cache use-after-free across TUs on one `MCCState` (now cleared in
    `mccgen_finish`; `tests/embed/api_extra.c` `test_multi_tu_complex`); qemufetch
    dir-ordering (`host_mkdirs(dest)` hoisted before the `curl -o` download in
    `tools/mccharness.c`).

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
