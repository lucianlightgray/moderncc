# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Open items (priority order)

_From CI run [28741671440](https://github.com/lucianlightgray/moderncc/actions/runs/28741671440)
(`0e700c7`, push 2026-07-05) — overall **green** (0 failures across linux×18,
macos×4, msvc×2, mingw, qemu×10, dist×9). These are the warnings / skips / matrix
open questions that green run left standing._

### exec/tls — MSVC arm64 backend miscompiles mcc (skipped on arm64+WIN32)

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

### macho-* on native macOS

- [x] **`macho-*` native macOS — RESOLVED (2026-07-05): native arm64 path is
  already fully covered; residual SKIPs are x86_64-exec-only (CI-infra choice).**
  Reading the harness confirms the native arm64 Mach-O slice needs **no** new
  work: `macho-conformance-native` (`suite_machonative`, tools/mccharness.c:1681)
  compiles the `tests/qemu/conformance/*.c` programs with the **native** `mcc`
  (which on arm64 macOS emits arm64 Mach-O), links a full executable via the mcc
  driver, and **runs it natively** — end-to-end, no emulation. `macho-structural`
  (structural), `macho-stack-protector`, and `macho-universal` add to that, all
  PASS on macos-arm64. So proposed option (b) ("add native arm64-osx codegen-run
  variants") would only **duplicate** `macho-conformance-native` and was dropped.
  The three still-SKIPping tests (`macho-codegen-run` / `macho-image-run` /
  `macho-apple-libc`) are, by construction, **x86_64-Mach-O executed via a Linux
  loader** — gated by `host_is_x86_64()`; on Apple-Silicon macos runners (no
  Rosetta on the non-dist jobs) skipping is correct. The only path to run them on
  macos-cross is option (a): install Rosetta 2 and relax `host_is_x86_64()` to a
  runtime "can we exec an x86_64 Mach-O" probe — a **CI-infrastructure** decision,
  not an mcc gap; deferred. Original analysis retained below.
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

- [x] **cli suite — RESOLVED (2026-07-05): 11 format-agnostic cases ported
  SKIP→PASS on macOS; the rest confirmed genuinely ELF/DWARF-only. Full audit below.**
  gated by `cli-suite "structural readelf/nm suite runs on native host only"` / ELF-isms.
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

## Doc & code-comment audit — open items (2026-07-05)

_Derived from a full read of the project's documentation (`README.md`,
`docs/{BUILD,OPTIMIZE,PROFILING,C9911}.md`, the `tests/**` READMEs) and every
source/tool comment. Each item is a **claim to validate**, a **detail to fix**, or
a **feature to implement**. Items already settled in "Notes" below (inline-only
linkage, implicit-function-declaration→error, K&R implicit-int) are **not**
re-raised here. `validate` = confirm/measure the claim; `fix` = correct a real
divergence/robustness gap; `impl` = add a missing feature. None are urgent — the
suite is green — but each is a real, actionable thread._

### A. Documentation integrity & headline claims

- [x] **`docs/OPTIMIZE.md` stub with dead tooling refs — FIXED (2026-07-05).** The
  intro promised a DRY findings table but ended at the SQL with zero findings and
  pointed at an `analysis/` dir + `/tmp/dupfn.py` that do not exist in-tree.
  Reframed the doc as a **methodology/reproducible-query** note with an explicit
  "Status" banner clarifying the harness is a not-vendored scratch tool and no
  findings are currently recorded. (A future improvement is to actually vendor the
  `analysis/` harness and record concrete DRY findings.)
- [ ] **Reconcile divergent test-count claims across docs (validate).**
  `README.md:116` (39/39, 22/22) vs `README.md:127-129` (782/782, 520/520) vs
  `docs/PROFILING.md:384` (804/772) cite different totals with no stated basis.
  → Regenerate from one `ctest -N` per host/preset and state the per-case vs
  aggregate counting basis; make the docs cite the same source of truth.
- [ ] **Trace the "~100× faster than gcc -O2" headline to a measurement (validate).**
  `README.md:15` says "~100×"; `docs/PROFILING.md:204-217` measures 118–204×
  (TU/opt dependent); the `README.md:318-328` table shows 108–141×. → Pick the
  documented benchmark and make the headline a measured range, not a round number.
- [ ] **Re-measure & date-stamp the README speed/size table post-lexer-change
  (validate).** `README.md:318-328` (0.05 s; 7/19/108/141×) and the ~0.6 MB /
  ~1.3 MB size claims (`README.md:16,320`) predate the `TOK_HASH_SIZE` change and
  are toolchain/host-sensitive. → Re-run `mccbench` + `size`/`strip` a `dist-*`
  build; refresh, noting the host as PROFILING does.
- [ ] **PROFILING §4–§5 hot-path %/timings predate the `TOK_HASH_SIZE`
  16384→65536 change (validate).** §8 (dated one day later) changed the lexer, so
  the `next_nomacro` self-% and `-E` timings in §4–§5 may be stale. → Re-run §3–§5
  or annotate them as pre-change baselines.
- [ ] **Regenerate the dated "all green" status prose from CI (validate).**
  `README.md:110-151` narrates per-preset pass/skip counts across ~35 presets;
  this rots silently. → Derive from the latest workflow run, or add a check that
  fails when the prose diverges from actual CTest output.

### B. Conformance gaps (from `docs/C9911.md` `✗`/`~` rows)

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

### C. Codegen / backend correctness & hardening

- [x] **Unhandled reloc types silently miscompile instead of failing — FIXED
  (2026-07-05).** Every backend's `relocate()` `default:` used to
  `fprintf(stderr,"FIXME: handle reloc type …")` and continue, leaving bytes
  unrelocated but still emitting output. Now routed through `mcc_error_noabort`
  (the primitive already used throughout those files) in all five backends —
  `x86_64/arm64/riscv64/arm/i386-link.c` — so an unknown reloc records an error
  and fails the link instead of producing a silent miscompile. Verified the
  default is genuinely unreachable for supported code (macos-cross 810/810 still
  green: the full cross-codegen + dash-s-bytes + conformance surface links clean).
  _Remaining (lower value):_ a positive per-arch audit enumerating exactly which
  reloc types each `gen` path can emit vs. what `relocate()` handles.
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

### D. Assembler / inline-asm coverage

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

### E. Test-harness coverage integrity

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
- [~] **`objcheck minos` ignored `LC_VERSION_MIN_MACOSX` — FIXED; fat-slice
  selector still open (2026-07-05).** `macho_slice` now also reads the legacy
  `LC_VERSION_MIN_{MACOSX,IPHONEOS,TVOS,WATCHOS}` load commands (version at +8),
  with `LC_BUILD_VERSION` (+12) taking precedence when both are present — so a
  Mach-O using only the legacy command no longer reports `minos 0.0.0`.
  _Remaining:_ for a fat binary `macho_parse` still validates the first slice that
  parses; add an optional `--arch` selector (or iterate all slices) so a per-slice
  minos mismatch on a later slice is detected.
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

### F. Runtime / tooling robustness & smaller diagnostics

- [x] **`-gstabs` build warning on modern gcc — FIXED (2026-07-05, found during
  this preset validation).** `CMakeLists.txt:2356` fed gcc `-gstabs` when building
  the bcheck runtime object (DWARF-off config), but GCC 12 dropped stabs support,
  so ubuntu-24.04 gcc warned `switch '-gstabs' is no longer supported` on every
  linux-gcc build. Now gated `CMAKE_C_COMPILER_VERSION VERSION_LESS 12`, falling
  back to `-gdwarf` on gcc ≥ 12 (functionally equivalent debug info for that
  object). linux-gcc build warnings 1→0.
- [x] **`machofat` re-sign silently best-effort — FIXED (2026-07-05).**
  tools/machofat.c now inspects codesign's exit status: exit 127 (codesign not
  found, e.g. a non-Darwin combine) is still skipped silently as documented, but
  any other nonzero exit emits a `machofat: warning: codesign failed …` to stderr
  so an AMFI-rejectable output is surfaced rather than reported as success.
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

### G. Warnings surfaced by the arm64-Linux preset validation (2026-07-05)

_Found by running the `linux-gcc-diagnostics` (everything-on) preset under Docker
`ubuntu:24.04` on a native **arm64** host — CI runs that preset on **x86_64**,
where `arch/arm64/*` isn't the native backend and single-source `static`-unused
functions differ, so these were latent. All are pre-existing (not from the audit
changes) and non-fatal (diagnostics has no `-Werror`); tests stayed 811/811._

- [x] **arm64 disassembler dead LD1/ST1 comparisons — FIXED (2026-07-05).**
  `arm64-dis.c:829` OR'd four masked compares; two (`== 0x0c400000` /
  `== 0x0d400000`) required bit 22 that their masks (`0xbfbf0000`/`0xbf9f0000`)
  clear, so they were always false (`-Wtautological-compare`). The bit-22-agnostic
  1st/3rd compares already match both load and store forms (the `?"ld1":"st1"`
  picks from bit 22), so the two dead alternatives were removed — no behavior change.
- [x] **`-Wunused-function` on Mach-O-only helpers in single-source ELF builds —
  FIXED (2026-07-05).** `mcc_uleb128_size` (libmcc.c) and `host_macos_sdk_root`
  (mcchost.c) are used only by `objfmt/mccmacho.c`, absent from an ELF target, so
  as single-source `static` they warned unused. Tagged both `MAYBE_UNUSED` (the
  existing idiom used across `mcchost.c`). (`multisource` never warned — `ST_FUNC`
  is extern there.)
- [x] **Swept the remaining `linux-gcc-diagnostics` / `-release` warnings on arm64
  — FIXED (2026-07-05).** Two more Mach-O/host-only helpers (`host_spawn_wait`,
  `host_codesign_adhoc` in mcchost.c) tagged `MAYBE_UNUSED`; and three
  `-Wformat-truncation` warnings in `tools/build.c` (`detect_triplet`) silenced by
  bounding each `%s` with a precision that fits the destination (`cand[i]` is
  `[256]`, paths are `[512]`) — zero behavior change. Result: `linux-gcc-diagnostics`
  and `linux-gcc-release` now build **warning-clean on arm64** too (were 12 / 3),
  tests still 811/811.

Presets exercised locally for this change (0 failures, 0 warnings unless noted) —
`macos` 811/811 and `macos-cross` 810/810 natively (Mach-O/arm64); and, under
Docker `ubuntu:24.04` on a native **arm64** host (ELF), the linux config matrix:
`linux-gcc`, `linux-clang`, `linux-gcc-release` (3 pre-existing warns),
`linux-gcc-sanitize`, `linux-gcc-static`, `linux-gcc-pie`, `linux-gcc-dwarf`,
`linux-gcc-predefs-off`, `linux-gcc-multisource`, `linux-clang-release` all
811/811; `linux-gcc-asm-off` 779/779; `linux-gcc-diagnostics` 811/811 (12→**0**
warnings after the §G fixes; `-release` 3→**0**). The qemu-user matrix, msvc, mingw, and
the `dist-{msvc,mingw}` packaging presets need heavier infra (a qemu image +
Gentoo stage3 downloads, or a Windows host) — the CI matrix's job.

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
