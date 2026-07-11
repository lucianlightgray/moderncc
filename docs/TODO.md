# TODO.md — settled gate work, in order of complexity

The decided, no-further-discussion items migrated out of GATED.md §7
(audit of 2026-07-09, commit `eb1fd4ef`; decision rounds 1-2 of 2026-07-09
folded in — GATED.md, SPLIT.md, and DEGLOBAL.md are historical planning
docs no longer in the repo). Ordered simplest → most complex; do them
top-down. Still open elsewhere: DEGLOBAL.md's stage-2 spelling (decided at
Stage-1 fallout), and the deferred/closed GATED.md items (format arc,
TargetDesc) which are **not** work.

Standing gates for every item: `ctest` green; items touching `src/` also need
the 3-stage byte-identical self-host fixpoint; items touching backend or
format files add the local matrix / a qemu spot-check as noted.

## 0. [x86_64-host] Differential random-program miscompile fuzzer + auto-reducer (major, NEW 2026-07-10)

A whole-pipeline correctness engine: generate random C programs, run each
through **mcc** and through **gcc + clang** as a same-ISA reference oracle,
flag any divergence in observable behavior, then **automatically reduce** the
offending program to a minimal repro and **bisect** it to the exact `-O` level
/ `MCC_AST_*` pass / git commit responsible. Found repros graduate into the
permanent ctest corpus.

**Why an x86_64 host (and why it barely overlaps anything here).** Differential
testing is only meaningful with a *trusted reference compiler executing on the
same target ISA* as mcc's output — on the repo's x86_64-Linux dev/CI box both
gcc **and** clang are first-class native oracles (arm64-darwin has no native
gcc and a far thinner reference/tooling stack). The mature generator
(csmith / YARPGen) and reducer (cvise / creduce) ecosystems and the full
sanitizer/valgrind stack are all native x86_64-Linux. Orthogonality: every
other TODO item (§1–§42) tunes one pass, codegen order, config macro, or ABI
detail; this is a *tools-only QA subsystem* over the entire front-to-back
pipeline (preprocess → parse → AST optimizer → codegen → link), complementing
the existing **fixed-corpus** diff3 / mcctest / c-torture harnesses with the
missing pieces: **random generation, automatic minimization, and
auto-attribution**. It has no compiler-codegen surface, so it carries none of
the byte-identity / fixpoint risk the ladder items do — and it becomes the
independent validation engine for §41 (the default-ON ungate sweep) and a
miscompile detector for §29–§35.

### Investigation tasks
- [x] **Generator selection** — csmith/cvise/creduce are absent on the dev/CI
      host, so a **small custom generator** (`tests/fuzz/gen.h`) is the primary,
      seed-deterministic and UB-free by construction (bitfields, unions, arrays,
      switch/for/if, bounded recursion; integer-only — see `tests/fuzz/NOTES.md`
      for why float/`_Complex`/VLA/asm are excluded from the always-green band).
      csmith/YARPGen remain a drop-in secondary in front of the same oracle.
- [x] **Oracle design** — gcc + clang at `-O2` as the majority-vote consensus;
      observable behavior = stdout + exit code (the generated `main` folds all
      globals into a printed checksum and returns it); references-disagree ⇒
      drop as impl-defined/latent-UB; every reported divergence is re-checked
      under `gcc -fsanitize=undefined,address` and dropped if the program has UB.
- [x] **mcc-under-test matrix** — `runner.c` sweeps `-O0..-O3` and (with
      `--gates`) each `MCC_AST_*` pass forced on; `fuzz/matrix` ctest exercises
      it. Divergence is attributed to a single (level, gate). This is the §41
      coverage.
- [x] **Reduction pipeline** — line-granularity **ddmin** (`--reduce`, a
      cvise/creduce fallback for hosts without them) with an interestingness
      predicate that re-confirms the mcc-vs-oracle divergence and rejects any
      candidate gcc/clang can't build; emits a self-contained `.c` into
      `tests/fuzz/corpus/`.
- [x] **Triage / bisection** — auto-sweep over (a) `-O` level and (b) which
      `MCC_AST_*` gate flips it, recorded in the saved repro's header;
      (c) `tests/fuzz/bisect.sh` is a `git bisect run` predicate over a repro.
- [~] **Dynamic-analysis layer** — mcc's *output* is built+run under
      ASan/UBSan as the oracle's soundness gate, and compiled-program
      crashes/timeouts are classified separately from wrong-answers. Building
      mcc *itself* under ASan (the `sanitize` preset already exists) and a
      valgrind pass over the corpus remain as follow-ups. PROGRESS
      (2026-07-10): exercised `mcc_s` (the `sanitize` preset = host
      ASan+UBSan) over ~120 exec/behavior test programs + a full `src/mcc.c`
      self-compile; it surfaced one hot-path latent UB in mcc itself —
      `cstr_cat` (`mccpp.c:437`) calling `memmove(NULL, str, 0)` on an empty
      CString (UBSan: "null pointer passed as argument 1, declared nonnull",
      121 hits). Fixed by guarding the no-op `memmove` with `if (len)`
      (behavior-identical; fixpoint still stage2==3==4). mcc_s is now
      UBSan-clean over that corpus. A *wider* `mcc_s` sweep (473 test sources ×
      `-O0`+`-O2`) then surfaced a heap-use-after-free that turned out to be a
      **SYM_DEBUG-build artifact, now fixed — see §0c** (the sanitize build had
      the sym-deferral compiled out; production was always correct). The
      mcc_s corpus sweep is now **wired as the green `sanitize-selfcheck`
      ctest** (full `tests/exec` × `-O0..-O3` under ASan+UBSan, 0 diagnostics).
      Valgrind pass attempted but **environment-blocked**: valgrind 3.27.1 on
      this Gentoo host SIGILLs in `_dl_start` (rtld.c:565) for *every* dynamic
      binary incl. `/bin/true` — an unsupported-ISA-in-the-loader incompat, not
      an mcc issue; the ASan+UBSan `mcc_s` path already provides the
      memory-safety validation, so valgrind is redundant here and skipped.
- [x] **CI integration & budget** — runs are seed-reproducible
      (`MCC_FUZZ_SEED`/`MCC_FUZZ_COUNT`/`MCC_FUZZ_GATES`); the graduated corpus
      is a regression-lock (`fuzz/corpus`). LANDED (2026-07-11): the *scheduled*
      nightly campaign is now wired — `tests/fuzz/campaign.sh` loops the runner
      over fresh seed batches until a wall-clock budget is spent or **K
      consecutive batches surface no new miscompile class** (attribution =
      `-O` level + `MCC_AST_*` gate the runner blames), deduping classes in
      `seen-classes.txt`; `.github/workflows/fuzz-nightly.yml` runs it on a
      nightly `cron` (+ `workflow_dispatch` with budget/batch inputs) against
      native gcc+clang, uploads any new `repro_*.c` as artifacts, and turns the
      job red exactly when a new class is found. Verified locally: the driver
      converges via the stop rule, dedups, respects the budget, and exits 0 on
      the (currently clean) corpus; YAML + `sh -n` validated.
- [x] **Graduation path** — a confirmed, UB-free, reduced divergence is written
      to `tests/fuzz/corpus/repro_seed<N>.c` (header records seed + attribution)
      and guarded by the `fuzz/corpus` replay; accepted impl-defined `[DIFF]`s
      are re-baselined as `corpus/accept_<name>.c` (see `corpus/README.md`).

### Sub-feature discovery topics (explore before scoping)
- **Equivalence-Modulo-Inputs (EMI)** mutation (Orion/Athena/Hermes style):
      prune or inject provably-dead code under a recorded profile so the
      reference output is invariant — targets *optimizer* miscompiles
      specifically and dovetails with the §32/§33 dataflow work.
- **Coverage-guided generation** — feed mcc's own edge coverage (gcov, or
      Intel-PT branch trace on x86_64) back into the generator to steer toward
      unexercised paths.
- **Link-time / ABI differential** — mix mcc `.o` with gcc `.o` (and vice
      versa) and cross-check struct-return, varargs, `long double`, bitfield
      layout, alignment — an ABI fuzzer orthogonal to the codegen passes.
- **Self-host differential** — compile `src/mcc.c` with mcc vs gcc and diff the
      *two resulting compilers'* behavior over the corpus — a semantic check
      strictly stronger than the byte-identity fixpoint.
- **Sanitizer-diff bootstrap** — compare a future mcc `-fsanitize=undefined`
      against clang's on the same UB program; seeds a real sanitizer feature.
- **Miscompile-class scoreboard** — tag each divergence by pass / ISA / C
      construct and rank them, to steer where optimizer and codegen effort
      goes next.

**Landing shape:** tools-only + a growing `tests/fuzz/` corpus; no
compiler-codegen change (no fixpoint/byte-identity gate on the harness
itself). First increment = the minimal loop: `csmith → {mcc,gcc,clang} -O2 →
compare exit+stdout hash → on divergence, cvise-reduce → save repro`, then
layer the matrix, triage, and dynamic-analysis passes on top.

**LANDED (2026-07-10).** `tests/fuzz/` = `gen.h` (custom generator) + `runner.c`
(oracle + UBSan gate + ddmin reducer + `-O`/gate triage) + `corpus/` +
`bisect.sh`, with `README.md`/`NOTES.md`. Wired as three x86_64-only ctests
(`fuzz/smoke` `-O0..-O3`, `fuzz/matrix` gate sweep, `fuzz/corpus` regression
replay), gated on native gcc+clang (reuses `MCC_DIFF3_GCC/CLANG`; SKIP
otherwise). Bring-up campaign: **200 seeds × (-O0..-O3 + 7 gates), 0
miscompiles**, so the corpus starts empty (no known mcc bug). Remaining Bucket-0
follow-ups are the two `[~]` items above (mcc-self ASan/valgrind; a *scheduled*
nightly campaign with crash-dedup/stop-rule) plus the discovery topics below
(EMI, coverage-guided, ABI/self-host/sanitizer-diff, scoreboard).

## 0c. [x] RESOLVED: `mcc_s` (SYM_DEBUG) was defer-blind — the "AST-replay UAF" was a sanitizer-build artifact, not a production bug

Found 2026-07-10 by the §0 dynamic-analysis layer (mcc built under host
ASan+UBSan = `mcc_s`, the `sanitize` preset). A wide sweep (473 test sources ×
`-O0..-O3`) hit a **heap-use-after-free** at `-O1..-O3` in the AST-replay path:
`AST_Load` → `indir()` (`mccgen.c:5870`) → `pointed_type` dereferences a
`type.ref` `Sym*` (a `mk_pointer` sym from a cast/compound-literal) that
`block()`'s `prev_scope`→`sym_pop` already freed.

**Root cause = the SYM_DEBUG build, not the optimizer.** `sym_free`
(`mccgen.c:845`) had the deferral **inside** `#ifndef MCC_SYM_DEBUG`:

```c
#ifndef MCC_SYM_DEBUG
#if MCC_CONFIG_OPTIMIZER
    if (ast_sym_defer(sym)) return;   // production: defer during replay window
#endif
    ...pool on sym_free_first...
#else
    mcc_free(sym);                    // SYM_DEBUG: NO defer — always frees!
#endif
```

So `mcc_s` (the only `MCC_SYM_DEBUG=1` target) **disabled the very deferral
mechanism that keeps replay-referenced syms alive**, guaranteeing a false
positive on every replayed function. In the production build `ast_sym_defer`
moves each freed sym to `ast_sym_deferred` for the whole `ast_func_begin`→
`ast_func_end` window (any function that replays had `defer_on` set), so
`type.ref` stays valid — **no production UAF, no miscompile**.

**Fix (touches only the SYM_DEBUG build; production byte-identical).**
1. `mccgen.c:sym_free` — moved the `ast_sym_defer` check *outside*
   `#ifndef MCC_SYM_DEBUG` so the sanitize build defers exactly like production
   (production branch unchanged → identical output).
2. `mccast.c` deferral flush — under `MCC_SYM_DEBUG`, `mcc_free` the deferred
   syms when the window closes (instead of pooling), so `mcc_s` still detects a
   *genuine* post-window use-after-free rather than leaking.

After the fix the whole `tests/exec` tree × `-O0..-O3` + a `src/mcc.c`
self-compile is **0 sanitizer diagnostics** — proving the production sym
lifetime is actually correct. Gate: production 3-stage fixpoint stage2==3==4
byte-identical, ctest 1874/1874, `sanitize` preset builds. New regression
lock: **`sanitize-selfcheck`** ctest (`tests/sanitize/run_selfcheck.cmake`)
compiles the full `tests/exec` corpus under `mcc_s` at `-O0..-O3` and fails on
any ASan/UBSan report — this is the "mcc-self ASan" half of §0's
dynamic-analysis `[~]` follow-up, now green and wired.

## 0b. [x86_64-host] `-fsanitize=` instrumentation suite — mcc as a sanitizer provider (major, NEW 2026-07-10)

Make **mcc emit sanitizer instrumentation**: `-fsanitize=undefined` (UBSan —
signed overflow, shift-out-of-range, div-by-zero, null/misaligned deref,
VLA/array bounds, bool/enum load, pointer overflow, …) and `-fsanitize=address`
(ASan — shadow memory + redzones + stack/global poisoning + allocator
interceptors), with `-fsanitize=thread`/`memory` as an **x86_64-only stretch**.
This is the "real sanitizer feature" §0's sanitizer-diff sub-topic seeds.

**Why x86_64-host.** The sanitizer runtime ecosystem is x86_64-first: TSan and
MSan are essentially x86_64-only (their shadow schemes / instrumentation are
x86_64-centric and unported to most arches), ASan's shadow-offset and
allocator/interceptor model and compiler-rt are bring-up-on-x86_64, and clang's
mature x86_64 sanitizers are the *differential reference* to validate mcc's
instrumentation against (directly via §0). The x86_64-Linux dev/CI box has the
full stack; arm64-darwin's support is thinner (no MSan, restricted TSan).

**Why it barely overlaps.** The whole optimizer ladder (§18–§36) *removes* work
at compile time; codegen items (§35/§36) are order/regalloc; conformance is
front-end. Sanitizers are the orthogonal **instrumentation + runtime** axis —
*inserting* checks and a shadow-state runtime. The only nearby item is the
existing **bcheck** bounds checker (`MCC_CONFIG_DIAG_RT`, §7/§13), which is a
much narrower bounds-only tool; UBSan/ASan are a broader distinct subsystem that
could eventually *share* bcheck's redzone/runtime machinery (consolidation, not
duplication). Distinct too from building *mcc itself* under host ASan/UBSan (the
`sanitize` preset / `MCC_BUILD_SANITIZE`) — that hardens the compiler; this
makes the compiler a sanitizer *provider* for user code.

### Investigation tasks
- [x] **UBSan first** (cheapest, arch-neutral, highest ROI): enumerate the UB
      classes to check and decide the emission point (front-end AST insertion
      vs codegen), `-trap` vs `-recover` modes, and the diagnostic ABI —
      clang-compatible `__ubsan_handle_*` calls (link against system UBSan) vs
      a minimal self-contained `libmccsan`. DECIDED (first increment, landed):
      emission at the **codegen backend** (`x86_64-gen.c:gen_opi`, right where
      the arithmetic instruction is emitted, guarded by
      `mcc_state->do_sanitize_undefined`), **trap mode** (`ud2` = SIGILL, no
      diagnostic ABI, no runtime library needed). Checks: signed `+`/`-`/`*`
      overflow (via the CPU `OF` flag → `jno`), out-of-range/negative shift
      count (unsigned `cmp count,width; jb`), integer divide-by-zero
      (`test div,div; jnz`). `-recover` mode + `__ubsan_handle_*` ABI deferred.
- [x] **Runtime model / compiler-rt interop:** can mcc-instrumented objects
      link+run against the *system* ASan/UBSan runtime (reuse clang's
      ecosystem via the `__asan_*`/`__ubsan_handle_*` ABI), or do we ship our
      own runtime? Investigate ABI compatibility and the trade-off. RESOLVED
      for the first increment: **trap mode needs no runtime at all** (`ud2` is
      self-contained, links against nothing). The compiler-rt/`libmccsan`
      decision is deferred to the recover-mode / ASan increments, where a
      runtime *is* required.
- [ ] **ASan design (x86_64 first):** the 1/8 shadow scheme
      (`(addr>>3)+offset`), stack/global/heap redzones, entry/exit stack
      poisoning, global registration, malloc/free interceptors; the shadow
      offset per target; ELF vs Mach-O differences.
- [x] **Optimizer interaction (critical):** sanitizer checks are
      side-effecting and must survive the AST optimizer — the §30/§32/§33
      passes must NOT fold/DCE check nodes; decide whether instrumentation runs
      before or after the replay optimizer, and confirm the default (non-`-f
      sanitize`) build stays byte-identical (new flag is default-off → no
      fixpoint/byte-identity risk for default codegen). CONFIRMED: because the
      checks are emitted at the *backend* (every path — `-O0` direct and AST
      replay reemit — funnels through `gen_opi`), they survive **all** `-O0..-O3`
      (verified: the trap fires at each level, clean programs stay silent). The
      default build is byte-identical (all new code behind the default-off
      flag): 3-stage self-host fixpoint stage2==3==4, full ctest 1874/1874.
- [x] **Differential validation via §0:** mcc-UBSan must flag the same UB
      clang's UBSan does over §0's UB corpus (this is exactly §0's
      sanitizer-diff sub-topic, now with a real subject). DONE: mcc
      `-fsanitize=undefined` and clang `-fsanitize=undefined
      -fsanitize-trap=undefined` both trap (SIGILL) on signed-add/sub/mul
      overflow, OOB shift, and div-by-zero, and both run clean programs to the
      same output.
- [~] **Scope/stretch + per-arch gating:** UBSan (all arches), ASan (x86_64 +
      arm64), TSan/MSan (x86_64-only stretch or documented out-of-scope) —
      gated per-target like the Tier-4 inliner. UBSan trap mode landed
      **x86_64 ELF/Mach-O only** for now (the checks live in `x86_64-gen.c`;
      the `-fsanitize=` flag warns/erases on other targets and PE). Porting the
      four checks to arm64/riscv64 backends + ASan/TSan/MSan remain.
- [x] **Test matrix:** a `tests/sanitize/` corpus with one program per UB /
      memory-error class asserting the check fires (and clean programs stay
      silent), across `-O0..-O3` and with the optimizer forced on. LANDED:
      `tests/sanitize/ubsan/` = 7 `trap_*.c` (signed add/sub/mul, llong mul,
      OOB shift, negative shift, div-by-zero) + 2 `clean_*.c` (arith,
      unsigned-wrap), driven by `run_ubsan.cmake` which sweeps `-O0..-O3` and
      asserts trap-fires / clean-silent; wired as 10 x86_64-only ctests
      (`ubsan/*`).

### Sub-feature discovery topics (explore before scoping)
- **compiler-rt interop vs `libmccsan`** — share clang's runtime (ABI-compat)
      or a small self-contained one; the decision shapes everything downstream.
- **bcheck consolidation** — seed ASan's redzone/shadow model from the existing
      `MCC_CONFIG_DIAG_RT` bounds machinery rather than a parallel system.
- **Trap vs recover vs abort**, and a minimal **PE/mingw trap-mode UBSan**
      (mirrors the existing MSVC/mingw host-sanitize story).
- **`-fsanitize-coverage`** — coverage instrumentation that feeds §0's
      coverage-guided fuzzing: the two x86_64-host features compose.
- **Adjacent hardening** — `-fstack-protector`, `-fsanitize=cfi` (vtable/type
      control-flow integrity), `_FORTIFY_SOURCE`-style checks — x86_64-first.
- **UBSan as a conformance amplifier** — run the existing exec corpus under
      mcc-UBSan to surface latent UB in mcc's own tests and in mcc output.
- **Freestanding / KASAN-style** sanitizer mode for the runtime itself.

**Landing shape:** a default-off `-fsanitize=<checks>` flag (non-sanitized
codegen byte-unchanged → no fixpoint/byte-identity risk); UBSan first
(front-end check emission + minimal runtime), then ASan on x86_64; each check
class lands with a `tests/sanitize/` golden and a clang differential (§0).
First increment = `-fsanitize=undefined` for signed-overflow + shift +
div-by-zero + null-deref in trap mode.

**LANDED (2026-07-10, first increment).** `-fsanitize=undefined` in **trap
mode** on **x86_64 ELF/Mach-O**. Flag parsing in `libmcc.c` (`MCC_OPTION_f`
case: `sanitize=`/`no-sanitize=` with a comma list — `undefined`,
`signed-integer-overflow`, `integer-divide-by-zero`, `shift[-exponent|-base]`,
`bounds` all enable it; `address`/`thread`/`memory` return a clear
"not yet implemented" error; unknown checks error). State: a plain
`unsigned char do_sanitize_undefined` in `MCCState` (not gated on
`MCC_CONFIG_DIAG_RT` — independent of the bcheck ladder). Predefines
`__MCC_SANITIZE_UNDEFINED__`. Codegen: `gen_ubsan_check(cc)` in
`x86_64-gen.c` emits `j<cc> .ok; ud2; .ok:` at four sites in `gen_opi` —
signed `+`/`-` and `*` overflow (OF→`jno`), variable-count shift `>=` width or
negative (`cmp ecx,width; jb`), and divide-by-zero (`test div,div; jnz`); all
guarded by `do_sanitize_undefined && !nocode_wanted` so the default path is
untouched. `-hh`/`-h` help updated. Gates: full ctest **1874/1874** (10 new
`ubsan/*`), 3-stage self-host fixpoint **stage2==3==4 byte-identical**, debug
+ release + cross + multisource presets build clean, clang differential agrees
on all four UB classes. Deferred: signed INT_MIN/-1 division (hardware `#DE`
already faults it), constant-count OOB shift (masked at compile time),
null-deref (hardware SIGSEGV already faults it — lower marginal value than the
silent-UB arithmetic/shift checks), `-recover` mode + `__ubsan_handle_*` ABI,
ASan, arm64/riscv64 ports.

## 1. Delete `TAL_INFO` (trivial)

- [x] Remove the `#if TAL_INFO` counters/report from `mccpp.c:113-266` and the
      `nb_peak/nb_total/peak_p` fields. It is defined nowhere, enabled by
      nothing, and adds fields to the hottest allocator's header when hand-set.

## 2. Give `mccast.h:80` a named macro (trivial)

- [x] `mccast.h:80` tests the `_MCC_H` include guard to detect "am I included
      from inside mcc.h" — replace with an explicit macro (e.g.
      `MCC_INTERNAL`) defined by `mcc.h` itself. No behavior change.

## 3. Close the `__GNU__` hostgate straggler (trivial)

- [x] `mcc.h:178` `#if defined __GNU__` (Hurd elfinterp default) is the one raw
      host-OS macro outside `mcchost.*`; add `__GNU__` to the BANNED list in
      `tools/hostgate.c:91-92` and route the test through the host/target
      abstraction. Gate: `host-gate-invariant` ctest stays green.

## 4. Resolve the five `#if 1` leftovers (small)

- [x] `mccgen.c:98` — collapse `precedence_parser` to always-on; delete the
      legacy `expr_prod`/`expr_sum` cascade under `#ifndef precedence_parser`
      (`mccgen.c:7944`) after confirming nothing else references it.
- [x] `mccgen.c:9448` — `init_assert()` on/off pair: tie to `NDEBUG` instead of
      a hardcoded `#if 1 / #else` no-op define.
- [x] `mccpe.c:555` — keep the live ELF→COFF export loop, delete the `#if 0`
      `.file`-aux variant at `mccpe.c:545`, drop the gate.
- [x] `mccpe.c:1140` — keep the `.def`-export branch, delete the disabled trace
      variant at `mccpe.c:1107`, drop the gate.
- [x] `mccmacho.c:2001` — chained-fixup pointer format choice
      (`DYLD_CHAINED_PTR_64` vs `_64_OFFSET`): keep both branches but select by
      a *named* constant, not a bare `#if 1`.

## 5. Purge the obvious dead `#if 0` blocks (small)

Delete outright (verify each still-referenced-nowhere while removing):

- [x] `mcc.h:1847` — disabled duplicate tool prototypes.
- [x] `i386-gen.c:759` — old `gjmp_cond_addr`, superseded by `gjmp_cond`.
- [x] `mccpp.c:1059` — unused `tok_size()` helper.
- [x] `mccpe.c:317` — unused `pe_sec_flags[]` table.
- [x] `arm-gen.c:172` — doubly-nested FPA/OABI deprecation warnings.
- [x] `mccgen.c:2286` — old EQ/NE shortcut, superseded by `gvtst_set`.
- [x] `mccgen.c:10865` — stray `if(tok=='{') expect(";")` fragment.

(The dormant *trace* blocks — `mccgen.c:521,725`, `mccelf.c:655`,
`mccpe.c:1107,1354` — are handled by item 13, not deleted here.)

## 6. Sanitizer pairing for the memory debuggers (small, CMake)

- [x] Auto-define `MEM_DEBUG` and `SYM_DEBUG` for the `mcc_s` sanitizer target
      (`CMakeLists.txt` ~:2237 area) so they stop being source hand-edits.
      Keep them off every other target — their presence changes allocator
      layout/locking. Gate: `sanitize` preset builds + sanitize-smoke.

## 7. Merge BCHECK/BACKTRACE into one `MCC_CONFIG_DIAG_RT` ladder (small-medium)

Decided 2026-07-09: the two interdependent booleans become one three-level
knob — `MCC_CONFIG_DIAG_RT = off | backtrace | bounds` — making the invalid
BCHECK-without-BACKTRACE state unrepresentable.

- [x] Mechanism ("enum?" — yes, at each layer's native kind): CMake cache
      STRING with `set_property(... STRINGS "off;backtrace;bounds")` (the
      configure-time enum/dropdown); emitted to C as a **numeric level macro**
      (0/1/2) since `#if` cannot evaluate C enums; a mirrored C `enum` for
      runtime-code readability.
- [x] Replace `#ifdef MCC_CONFIG_BCHECK` → level ≥ 2 tests,
      `MCC_CONFIG_BACKTRACE` → level ≥ 1; delete the CMake dependency warning
      (`CMakeLists.txt:296`). `MCC_CONFIG_BACKTRACE_ONLY` (the amalgamation
      slice selector) is untouched — it is structural, not part of the ladder.
- [x] Presets: Debug ⇒ `bounds`, `release` ⇒ `off`, per current defaults;
      ckconfig ledger updated. Runtime flags `-b`/`-bt` and their
      "not built into this mcc" errors unchanged in behavior.
- [x] Gates: debug + release presets, `config-drift-invariant`, and a PE-cell
      build (bcheck must remain strippable there — unsupported on msvcrt).

## 8. Investigate, then resolve, the three non-obvious `#if 0` blocks (medium)

Finish-or-purge decisions; the investigation *is* the task:

- [x] `mccpp.c:1302` — `TOK_GET` function-call variant vs the inline fast
      macro: confirm full equivalence, then delete the gate and the slow path.
- [x] `mccelf.c:3587` — disabled `DT_RPATH`/`DT_NEEDED` processing loop: verify
      the live dll-ref resolution covers every case it handled; if yes delete,
      if no finish it (it predates the current loader logic).
- [x] `mccrun.c:623,691,747` — the abandoned DWARF directory-table path in the
      `-run` backtrace line-info parser: completing it yields correct backtrace
      paths for sources compiled outside the cwd. Decide finish vs purge by
      writing the failing case first (a `-bt` test with `-I`-relative sources).

## 9. `CONFIG_RUN_MMAP_EXEC` → runtime probe (medium)

- [x] In `host_runmem_alloc`/`host_runmem_protect` (`mcchost.c:952-983`):
      attempt plain `mmap(RWX)`/`mprotect(RWX)`; on `EACCES`/`EPERM` fall back
      to the file-backed dual-mapping path; probe once, cache the result.
      Remove the "rebuild with -DMCC_RUN_MMAP_EXEC" failmsg (`mcchost.h:252`);
      keep the CMake knob only as a force-override. Constraint: the fallback
      must be chosen before `mcc_relocate_ex`'s two-pass layout
      (`mccrun.c:265,296`) since `ptr_diff` affects addressing. Gate: `-run`
      tests on Linux + a forced-fallback run (`MCC_RUN_MMAP_EXEC=ON` build or a
      seccomp/SELinux sandbox if available).

## 10. Unbake the default strings into per-target C tables (medium)

Decided 2026-07-09 (packaging policy): defaults move from CMake `-D` string
baking into ordinary per-target/per-libc tables in source — one place,
greppable, testable. **No config file**, and **no new environment variable
names without explicit sign-off** (mcc already honors the established ones:
`C_INCLUDE_PATH`, `CPATH`, `LIBRARY_PATH`, `DYLD_FRAMEWORK_PATH`,
`mcc.c:224-243`; nothing new gets invented here).

- [x] Move into the table: `MCC_CONFIG_SYSINCLUDEPATHS` (`libmcc.c:1021`),
      `MCC_CONFIG_LIBPATHS` (`:1040`), `MCC_CONFIG_CRTPREFIX` (`:1053`),
      `MCC_CONFIG_ELFINTERP(_ARMHF)` (`mcc.h:177-193` → `mccelf.c:111-114` —
      it is already a per-CPU macro table; this makes it a *data* table),
      the PIE/PIC default selectors (`libmcc.c:1007,919`), and the musl bundle
      (`MCC_CONFIG_MUSL` triple string + the `{B}`/`{R}` sysroot overrides,
      keyed by a per-libc table row).
- [x] `MCC_CONFIG_SWITCHES` (`libmcc.c:950`) folds into the same table as a
      per-target default-options string through the existing
      `mcc_set_options` sink — table entry, **not** an env var.
- [x] CMake: the emission blocks (`CMakeLists.txt:1720-1749` area) stop
      injecting the covered `-D` strings; preset-level overrides (musl, cross)
      select table rows instead of overriding macros. CMakeLists stays
      comment-free.
- [x] All values remain runtime-overridable via the existing flags
      (`-I`/`-L`/`-B`/`--sysroot`/`-dynamic-linker=`); behavior with no flags
      must be identical before/after per target.
- [x] Gates: `config-drift-invariant` (retiring macros will trip
      emitted-but-unread — update the ledger), release + musl + cross presets,
      `ci pkg` dist artifacts diffed, qemu spot-check (loader paths are
      security-sensitive: wrong default = silent mislink).

## 11. Build the three gate tripwires (medium, tools + ctest)

Decided 2026-07-09: targetgate + deadgate + idiomgate (directive ratchet
declined; fmtgate moot with the format arc deferred). Same recipe as
`tools/hostgate.c`: banned-pattern scan + explicit allowlist + ctest
invariant, wired like `host-gate-invariant`.

- [x] **targetgate** — `MCC_TARGET_*` in PP conditionals is permitted only in
      `src/arch/` plus a frozen allowlist of today's legitimate consumer
      files (`mcc.h`, `mccgen.c`, `mccdbg.c`, `objfmt/*`, `libmcc.c`,
      `mccpp.c`, `mccrun.c`, `mcc.c`, `mccasm.c`, `mcctok.h`, `mccast.c`,
      `mcctools.c` — finalize the list from the audit counts). With TargetDesc
      declined, this fence is what keeps the ~200 scattered sites
      frozen-not-growing: new files cannot introduce target conditionals.
- [x] **deadgate** — no new `#if 0` / bare `#if 1` anywhere; the GATED.md §5
      inventory is the initial frozen allowlist, which items 4, 5, and 8
      shrink to empty.
- [x] **idiomgate** — one canonical test form per config macro (the
      `mcc.h:77-103` normalization pattern until item 16 lands, then the
      item-16 canonical form). Lowest standalone value of the three; build it
      last and let item 16's grep-gates absorb it if that ships first.
- [x] Gates: the three new ctest invariants green on the full matrix; ledger
      lives in the tools (single source of truth, `tools/ci.c` style).

## 12. Re-gate AST and CST under their parent-feature CONFIG gates (medium-large)

Decided 2026-07-09: not "drop the gates" — **replace subsystem-internal gates
with the user-facing parent feature's CONFIG gate from CMake**, default ON.
The subsystems stay strippable, but by the name of what the user loses, not
the name of the internal IR.

- [x] `MCC_CONFIG_AST` / `MCC_AST` → the optimizer feature gate (name per item-16
      rules; working proposal `MCC_CONFIG_OPTIMIZER` — it gates what `-O1+`
      does as implemented). ~103 sites re-spelled; **no code motion**
      (AST-replay positional sensitivity — declaration-level edits only).
- [x] `MCC_CONFIG_CST` / `MCC_CST` → the LSP feature gate (working proposal
      `MCC_CONFIG_LSP` — it gates `--lsp` capture as implemented). ~49 sites;
      csttool builds against the same gate.
- [x] Semantics when OFF: unchanged from today's OFF builds — `-O1+` degrades
      to `-O0`-equivalent output, `--lsp` errors like `-b` does in a
      bcheck-less build. `-O0` output stays byte-identical ON vs OFF (the
      existing invariant).
- [x] Normalize the test idiom in the same sweep (kills the
      `#if defined(X) && X` vs `#ifdef X` split for these two families).
- [x] CMake/presets: `MCC_AST`/`MCC_CST` options rename; the `ast`/`cst`
      experiment presets become the feature-off axes; MCC.md/EXCESS.md ledger
      rows update.
- [x] Gates: ON/OFF × single/multisource builds, ctest, `-O0` byte-identical
      cross-check, whole-corpus mcctest at `-O0..-O3` for the ON build,
      self-host fixpoint.

## 13. Runtime `--debug=<cat>` diagnostics (medium-large)

Policy (final): every trace/dump *implementation* compiles always; activation
is runtime via a category bitmask over the existing `g_debug` int
(`-d<num>`, `libmcc.c:59,2146`).

- [x] Define categories: `reloc, inc, pp, struct, tok, pe, ver, asm` (+ `sym`
      for the dormant symbol traces). Map `--debug=a,b` → `g_debug` bits;
      keep `-d<num>` as the raw form.
- [x] Convert the eight macro families: `DEBUG_RELOC` (also drop the force-
      `#undef` at `mccelf.c:3`), `INC_DEBUG` (delete — fold unique messages
      into the existing `-vv` include tree at `mccpp.c:1495`), `PP_DEBUG`
      (keep the `PP_PRINT` wrapper; fast path pays one branch), `BF_DEBUG`,
      `PARSE_DEBUG`'s token-echo half (`mccpp.c:3515`; the `len=1` half stays
      compile-time), `PE_PRINT_SECTIONS`, `DEBUG_VERSION`, `ASM_DEBUG`'s trace
      half (its "bad op table" checks at `i386-asm.c:951`, `mccasm.c:1445-1491`
      become always-on cheap asserts).
- [x] Convert the dormant `#if 0` traces worth keeping (`mccgen.c:521` pv/psyms
      dumps → `sym`, `mccelf.c:655`, `mccpe.c:1354`); delete the rest
      (`mccgen.c:725`, `mccpe.c:1107` — superseded by categories above).
- [x] Document in `mcc -hh`; bench before/after on the mcc rows (expected ~0:
      all sites are cold or one-branch guarded).

## 14. Structural ungating where free (medium-large)

- [x] Delete `NEED_RELOC_TYPE`/`NEED_BUILD_GOT`: compile `code_reloc`,
      `gotplt_entry_type`, `create_plt_entry`, `relocate_plt`,
      `build_got_entries` unconditionally in all five `*-link.c` + the
      `mccelf.c:1140` consumer; remove the definitions at `mcc.h:1664-1669`.
      Cost: a few KB of unused code in PE/Mach-O builds. riscv64-link.c already
      lives this way (it never had the `NEED_RELOC_TYPE` guard).
- [x] Convert the three `#if SHT_RELX == SHT_RELA` compile branches
      (`mccelf.c:697,1014,1845`) to runtime `if` on a has-addend predicate —
      the fourth site (`mccelf.c:700`) already does exactly this. The
      `ElfW_Rel` *type* stays compile-time (no runtime type selection in C).
- [x] Gate: full local matrix (PE + Mach-O cells are where unused-function
      fallout appears) + per-target `mcc -c` object comparison (must be
      byte-identical — no executable text changes).

## 15. Execute SPLIT.md (large)

- [x] The `TARGET_DEFS_ONLY` → `<arch>-<part>.h` split, per
      SPLIT.md §4-6: 13 headers created, 13 `.c` skeletons
      removed, `mcc.h:217-239` ladder rewritten, macro gone from the tree.
      Gates as specified there (matrix, ctest incl. drift invariants,
      self-host fixpoint, per-target byte-identical objects).
      Sequencing: land before any DEGLOBAL.md stage begins — both touch every
      backend file.

## 16. Rename every PP gate macro to the naming standard (widest surface — run LAST)

Deliberately last: items 1-15 delete or re-spell whole macro families outright
(`TARGET_DEFS_ONLY`, `USING_GLOBALS` via DEGLOBAL, the debug traces,
`NEED_*`, the `#if 0/1` toggles, the BCHECK ladder, the AST/CST parent
gates), so the rename surface shrinks with every item above. Supersedes
GATED.md item 9(a)'s narrower `CONFIG_AST` rename.

**The rules:**

- [x] **Public** (user/build-facing, settable via CMake or `-D`):
      `MCC_CONFIG_<WHAT_IT_DOES>`. **Private** (derived, per-target constants,
      structural): `MCC_<WHAT_IT_IS>`. Nothing gate-like without one of the
      two prefixes.
- [x] Names state **what the feature does as implemented** — plain English,
      not intent, not history, not relative terms. The repeat offenders:
      `CONFIG_NEW_MACHO` ("new" relative to what?) → e.g.
      `MCC_CONFIG_MACHO_CHAINED_FIXUPS` (that *is* what it selects);
      `ELF_OBJ_ONLY` (reads as "only ELF objects", means "this format emits
      ELF *only as* `-c` objects, no ELF executables") → renames to say the
      true condition; `CONFIG_RUN_MMAP_EXEC` (names the syscall, not the
      behavior) → `MCC_CONFIG_RUN_DUALMAP` or similar once item 9's probe
      lands; `MCC_IS_NATIVE` → `MCC_TARGET_IS_HOST` (that is the actual test,
      `mcc.h:63-75`); `PROMOTE_RET` → `MCC_RET_PROMOTES_INT` (small integer
      returns are promoted); `SINGLE_SOURCE` → `MCC_AMALGAMATED` (one TU is
      what it does).
- [x] **No derived negatives**: `MCC_DISABLE_ASM` (the inverse shadow of
      `CONFIG_MCC_ASM`, `mcc.h:101-103`) is deleted, one positive macro
      remains. Same for any `*_ONLY`/`*_OFF` shadows found in the sweep.
- [x] **One test idiom** per macro kind (boolean config: `#if`, never mixed
      `#ifdef`/`#if defined(X) && X`).
- [x] **Prefix flip**: today the C macros are `CONFIG_MCC_*` while the CMake
      options are already `MCC_CONFIG_*` (`MCC_CONFIG_ASM` option →
      `CONFIG_MCC_ASM` define) — the *CMake side wins*; the C side renames to
      match. Every `CONFIG_*` without the `MCC_` root renames too
      (`CONFIG_SYSROOT` → `MCC_CONFIG_SYSROOT`, `CONFIG_DWARF_VERSION`,
      `CONFIG_CODESIGN`, `CONFIG_MCCBOOT`, `CONFIG_TRIPLET`, `CONFIG_MCCDIR`,
      `CONFIG_OS_RELEASE`, …). Private per-target constants gain the `MCC_`
      prefix (`PTR_SIZE` → `MCC_PTR_SIZE`, `LDOUBLE_SIZE`, `NB_REGS`,
      `RC_*`/`TREG_*` stay as-is only if kept backend-private after SPLIT —
      decide per family during the sweep, but no unprefixed name may cross a
      file boundary).
- [x] **CMake refactor where it contradicts**: `CMakeLists.txt` emission
      blocks (`:1638-1749` area) emit the new names; `mcc_config_node` names,
      `CMakePresets.json` cache vars, and the `tools/ci.c` preset ledger stay
      in lockstep (one atomic commit — no alias/transition period; the presets
      are the single source of truth). NOTE: CMakeLists.txt stays
      comment-free.
- [x] **Tooling follows**: `tools/ckconfig.c` scanner prefix `CONFIG_MCC_` →
      `MCC_CONFIG_` — which automatically brings the formerly-unprefixed
      macros under drift audit (closing the ckconfig blind spot from GATED.md
      §3); `tools/hostgate.c` untouched (bans *system* macros, which don't
      rename); item 11's idiomgate switches to the new canonical form;
      grep-gates in CI: zero hits for `CONFIG_MCC_`, bare `CONFIG_[A-Z]`, and
      the retired names.
- [x] Gates: full local matrix + qemu spot-check, `config-drift-invariant` +
      `host-gate-invariant` + `preset-parity-invariant` + the three item-11
      invariants, 3-stage self-host fixpoint, and per-target `mcc -c`
      byte-identical objects (renames move no code). Docs sweep last:
      MCC.md/EXCESS.md/BUILD.md references update in the same commit.

## 17. PP macro evaluation through the AST `-run` bridge (exploratory)

- [x] Without implementing new AST nodes, use the ast API to hook the
      preprocessor and treat identifiers as if they were variables that can
      be evaluated by calling `-run` internally — enough to calculate even a
      recursive `ret macro-converted-to-a-function(value)` — unless this
      would break the C standard in a destructive way. Landed as opt-in
      `-fmacro-eval`: fires only where the program was otherwise ill-formed
      (implicit function declaration), converts the macro to a real function
      and evaluates the call in a spawned `-run` child; driving cli tests
      `macro_eval_recursive` / `macro_eval_off_by_default`.

## 18. Persistent JIT-optimization cache keyed by AST-intention hash

The cache is the hypervisor's memory across runs: for each function it
stores the best JIT-optimized machine-code version already found and the
last optimizer-search attempt's inputs, indexed by the **binary hash of
the original AST "intention" tree** (`mccast` AstArena) so a run resumes
the search instead of restarting cold — and a different intention (any
tree edit) simply misses.

- [x] Add `host_cache_dir(char *buf, int size)` to `mcchost.*` that
      resolves the OS's most standardized per-user cache directory,
      appends `mcc/`, and creates it (`host_mkdirs`) — no new env-var
      names invented, only the platform-established ones, routed through
      the host abstraction (no raw host-OS macros, so `host-gate-invariant`
      stays green):
      - Linux/BSD/Hurd: `$XDG_CACHE_HOME`, else `$HOME/.cache` (XDG Base
        Directory spec).
      - macOS (`MCC_HOST_DARWIN`): `$HOME/Library/Caches`.
      - Windows (`MCC_HOST_WIN32`): `%LOCALAPPDATA%`, else
        `%USERPROFILE%\AppData\Local`.
      Return <0 and let callers stay tolerant when no home/cache dir is
      resolvable (sandboxes, `$HOME` unset).
- [x] Compute a stable **intention hash** over the captured AST tree
      (kinds + ops + type/sym/const payloads in canonical child order —
      the replay-relevant fields, *not* addresses or capture-order slot
      ids) so it is reproducible across runs and processes. This is the
      cache key; a matching hash means "same intention, reuse the prior
      optimizer result", a miss means "new intention, search fresh".
- [x] Cache entry per key = the previously JIT-optimized version of that
      function (the relocatable/position-independent code bytes the
      hypervisor produced) **plus the last iteration attempt's optimizer
      inputs** (the search state / permutation weights / gate settings
      that produced it) so the next run warm-starts from where the last
      search left off rather than re-deriving it. File is keyed under the
      cache dir by `MCC_VERSION` + target triple + intention hash so a
      compiler-version or target bump can't load a stale/foreign blob.
- [x] The hypervisor (`tools/mcchv.c`) reads the cache on start (seed the
      pattern permutation and JIT kernel from the stored entry when the
      intention hash matches) and writes back the improved version at the
      end; a cold miss reproduces today's from-scratch behavior exactly.
      Ship a cli/unit test: resolve dir, round-trip an entry, prove a hash
      match warm-starts and a hash mismatch (edited intention) misses, and
      skip cleanly where no writable home exists.

**LANDED (2026-07-10).** `ast_intention_hash(AstArena*, root)` joins the
public arena API (`mccast.h`): FNV-1a over kind/op/type_t/nchild in
canonical child order with identifiers alpha-renamed to first-seen
positional slots; `type_ref`, `cst` provenance, and `AST_Ref` frame
offsets excluded, so the hash is reproducible across processes and build
modes (amalgamated and multisource produce the identical key). Per-function
hashes accumulate in `ast_func_end` (pre-pass, before the tree mutates)
and surface through two new `LIBMCCAPI` entries: `mcc_intention_hash(s)`
and `mcc_cache_dir(buf,len)` (wrapping `host_cache_dir`; `mcchv`'s
homegrown env ladder deleted — `MCCHV_CACHE_DIR` survives as the test
override, and with no resolvable dir the hypervisor prints `cache
disabled` and skips caching). The hypervisor keys entries by
`MCC_VERSION` + triplet + the AST intention hash of the *canonical*
baseline-kernel source (pattern addresses replaced by a symbolic
`hv_pat_base` so the tree is run-stable) compiled at `-O1` in a throwaway
state; optimizer-off builds fall back to the pattern-table hash. Entry v2
persists the best config, its measurements, and the last search attempt's
inputs — TPE observation set (≤64, best-first), linear sweep cursor, RNG
state — so a warm hit installs the stored best kernel immediately and the
search *continues* (cursor/observations verified advancing across runs);
the optimized code itself is re-derived deterministically from the stored
config because the kernel source embeds run-local store addresses (§21's
machine-code-tier NOTE: regeneration is the re-verification). Writes use
the §21 protocol: `flock` on a sibling `.lock`, A/B keep-best merge
(smaller measured cpu wins, cursors take max), tmp + `fsync` + `rename`.
Tests: `ast/intention` (asttool: stability, alpha-rename invariance,
edit sensitivity, offset exclusion) and `hypervisor-cache`
(`tests/hvcache.sh`: cold→warm→resume, key stability, edited intention
misses, empty-HOME skip). Gates: ctest 1860/1860, fixpoint
stage2==3==4, optimizer-off + multisource cells green.

## 19. BUG: float/double-return inline graft miscomputes at -O3 — FIXED

- [x] Surfaced while validating local const-prop (independent of it —
      reproduces on stock compiler and with `MCC_AST_TEMPLATES=0`): a
      `double`-returning function inlined into a caller at `-O3`
      (`MCC_AST_INLINE`) produces the wrong value; `int`-return is fine.
      The AST inline graft (`ast_reemit_forward_inlines` / replay inline
      driver in `mccast.c`) mishandles the float return register/slot.
      Not caught by the exec-replay ctest columns (none enable inline).
      Repro: a small `double f(...){...}` called in `main` at -O3, diff
      vs `-O0`. Fix the float-return graft; add an inline-column exec
      golden with float/double returns so the suite covers it.

# arm64 native validation queue (run on Mac — qemu can't reproduce)

Native-arm64-only checks for the A→B→D→C build order. Per the standing
optimizer gate (§32: "native arm64 spot-check for every increment") and the
known-issue that qemu can't reproduce arm64 native failures (x86-TSO vs the
real weak memory model; atomics/bounds/complex diverge only on real
ubuntu-24.04-arm / Apple silicon). x86_64 ctest + fixpoint are the local
gate; these are the checks that must run on the Mac before each increment is
considered green. Ordered to match the A→B→D→C plan.

## N1. A — §31 join-pass search registration (landed x86_64, arm64 spot-check pending)

Landed: `MCC_AST_CPROP_JOIN`/`MCC_AST_CSE_JOIN` registered as budget
dimensions of the `-O<N>` search (`so_setenv_cfg`, `SO_BUDGET_SPACE`
36→144, `SO_CKPT_FMT` 5→6). x86_64: full ctest 1862/1862 + fixpoint
byte-identical.

- [x] `mcc -O4 <file>` on arm64-darwin over a TU with same-key `||`
      clusters + loop-invariant subexpressions: search runs (135 evals in
      ~4s), `so-<key>.ck` v6 round-trips (fmt byte `06`, progress cursor
      persists across a 2nd warm-start run), no miscompile (output byte-for-
      byte matches the `-O0` reference).
- [x] Confirm `-O0..-O3` output byte-identical arm64 (the join dims only
      fire under `-O4+` search workers; default build must be untouched) —
      all four `-O0..-O3` objects hash-identical.
- [x] 3-stage self-host fixpoint on arm64-darwin (search plumbing is
      byte-identity-neutral, but confirm on native) — `fixpoint-invariant`
      ctest (native cmake-macos) green.

## N2. B — §32c synthetic-temp infrastructure (arm64/riscv64 FIRST)

Bring-up order is arm64/riscv64 before x86_64 by design: the fatal
promotion-poison desync (Obstacle B, `ast_plan_promotion` src/mccast.c ~:2350)
lives in a block gated `#if MCC_CONFIG_OPTIMIZER && defined(MCC_TARGET_X86_64)`
(stub returns 0 elsewhere), so arm64/riscv64 are inherently safe from it —
validate the fresh-temp carve there first, add the x86_64 poison guard second.

NOTE (2026-07-10, native arm64): the §32c *value-materializing LICM* pass is
not yet landed (`ast_plan_promotion` is the x86_64-gated stub returning 0 on
arm64). But the synthetic-temp **carve infrastructure** it needs *is* landed
and exercised: the §23 inliner carves a fresh strictly-negative slot
(`loc = (loc - rsz) & -align;` mccast.c ~:2029) to materialize every grafted
call's return value. So the frame-carve + unwind invariant below is validated
against real synthetic temps (the inliner's), not a hypothetical: any
strictly-negative / non-16-aligned local offset is carved and unwound
correctly. Exercised with a spill/`pad[37]`/`pad[23]`-heavy TU that drives
`loc` strongly negative and non-aligned — the identical `gfunc_epilog` path a
synthetic temp hits.

BOX-3 STATUS (2026-07-10, native arm64): two layers, in order.
1. **arm64 `-O1+` self-host `load()` assert — FIXED.** First attempt aborted:
   any AST-replay optimized self-host of `src/mcc.c` on arm64 (`-O1/-O2/-O3`)
   hit `load(1,(32,400,0))` → `arm64-gen.c:650 assert(0)`, where `0x400 ==
   VT_VLA` reaches `load()` whose `svr` mask didn't strip it. Resolved by
   `76407be9` (§34: added `VT_MUSTCAST` — `0xC00`, covering `VT_VLA`'s `0x400`
   bit — to the `arm64-gen.c:494` mask), a fresh instance of the standing
   arm64 "`load()` exact-match r-flag mask" hazard. With that, `-O1+` arm64
   self-host now *runs* (`mcc -O1 src/mcc.c` rc 0).
2. **`-O3` self-host fixpoint — DONE (the earlier "non-determinism" was a
   test-harness artifact, now retracted).** `-O3` codegen IS deterministic:
   `mcc -O3 -c src/mcc.c` produces a byte-identical object twice, and the
   full executable is identical when compiled to the *same* `-o` name. The
   earlier "different bytes" was exactly 2 bytes — the **embedded output
   basename** (`me1` vs `me2`) — because the throwaway harness compiled each
   stage to a *different* name; the real `fixpointgate` renames a fixed name,
   so it is immune. Compiling each stage to the same name, the 3-stage
   self-host fixpoint holds byte-identically under *both* fresh-temp settings:
   `MCC_AST_INLINE=1` → stage2==stage3==stage4, and `MCC_AST_INLINE=0` →
   stage2==stage3==stage4 (the ON and OFF fixpoints differ from each other,
   as expected — inlining changes the code — but each is its own byte-stable
   fixpoint). This also retires the false "-O3 determinism" blocker on §32c.

- [x] Frame carve on arm64: `gfunc_epilog` `diff = (-loc + 15) & ~15;`
      (src/arch/arm64/arm64-gen.c ~:1625) — validated the carve covers a
      non-16-aligned local block: prologue `sub sp, sp, #112` (and #48/#32/#16
      across the TU) all 16-aligned, epilogue restores sp via `add sp, x29, #0`
      (frame-pointer relative) so it is balanced for *any* carve value incl. a
      future synthetic strictly-negative temp; `-O0..-O3` all run to the same
      correct result (stack balanced).
- [x] **Unwind data**: arm64 unwind uses `-loc` (arm64-gen.c ~:1638) —
      validated `-bt16` backtrace faults inside a `pad[37]` carved leaf and
      walks correctly up through 5 carved recursive frames → `mid` → `main`
      (the qemu-untrustworthy unwind path), native cmake-macos.
- [x] 3-stage self-host fixpoint on arm64-darwin with the fresh-temp pass
      forced ON and OFF — DONE. Each stage compiled to a fixed `-o` name (as
      `fixpointgate` does): `MCC_AST_INLINE=1` → stage2==stage3==stage4
      (byte-identical), `MCC_AST_INLINE=0` → stage2==stage3==stage4. Both
      reach their own byte-stable fixpoint natively at `-O3`. (The `-O1+`
      `load()` assert that first blocked this was fixed by §34/`76407be9`;
      the "-O3 non-determinism" I briefly reported was a harness bug —
      different `-o` names embed different basenames — now retracted.)
- [ ] Then repeat for riscv64 (`riscv64-gen.c ~:848`) — deferred to CI (no
      native riscv64 box; host is arm64-darwin).

## N3. D — §30/§32a-b transform refinements

New §30 branchless encodings are integer-arithmetic and arch-neutral, but the
`VT_CMP`-consumption hazard they must avoid (the reversed-operand `ast_bf_build`
order, TODO §30) was *originally an arm64 bug* (the `vcheck_cmp`-before-
`gfunc_call` fix). So any new encoding (`&&`-of-`!=`, biased range,
value-table) needs native arm64 confirmation the comparison flags aren't
clobbered.

- [x] Edge-key sweep of the landed §30 shift-mask encoding on arm64-darwin
      (−2³¹/−65/−1/0/63/64/2³²+5 × int/uint/llong/ullong/char/short),
      hash-checksummed vs the arm64 `-O0` reference — transform confirmed
      firing (`[ast-bitflag]` on the int cluster; the `key<64` unsigned
      guard correctly rejects negatives/out-of-window keys), FNV checksum
      byte-identical to the transform-off reference across the full edge
      set. (No `&&`-of-`!=`/biased/value-table encoding is landed yet; only
      the shift-mask form exists to sweep.)
- [x] `--ast` c-torture columns native arm64 for §32a/§32b: `ast-cprop-join`
      ctest (Test #1818) + the 949 exec-replay columns green on cmake-macos;
      `cpropjoin.sh` OK natively (all CPROP_JOIN×CSE_JOIN×{O0..O3} combos
      match the O0 rc, incl. `cse_join` through an `ext()` Invoke and the
      escaped-pointer arm); both joins force-on over the 251-file exec
      corpus at -O2 = 241 compiled, 0 regressions vs joins-off.

## N4. C — §25 frontend-JIT scoring tier

The JIT `-run` measurement path (`mcc_relocate` + `host_runmem_alloc` RWX)
and RSS sampling are OS/arch-sensitive; Apple-silicon W^X (JIT entitlement /
`MAP_JIT` + `pthread_jit_write_protect_np`) differs from Linux.

LANDED (2026-07-10, native arm64): the JIT cpu-scoring tier is now
implemented — `MCC_AST_JITSCORE` (default off) makes the whole-TU `-O<N>`
search score each candidate by best-of-3 `-run` wall time (`so_run_score`/
`so_spawn_run` via `wait4`) with `getrusage` peak RSS, instead of `.text`
size. The flag folds into `so_key` so time-scored and size-scored
checkpoints never collide; only link-to-exe TUs are timed, compile-only
(`-c`/`-S`/`-E`/`-r`/`-shared`) TUs fall back to the size objective.
Default build byte-identical (`-O0..-O3` hash-identical, gate is opt-in).

- [x] `-O4+` with the JIT **cpu-scoring** tier active on arm64-darwin:
      candidate `-run` executes under the watchdog, best-of-K timing +
      `getrusage` peak RSS are sane, non-runnable TUs fall back to the size
      objective — `MCC_AST_JITSCORE=1 mcc -O4 -v` on a runnable TU reports
      `-> 7344 us/run, peak RSS 3536 KiB` and the emitted binary is correct;
      a `-c` TU reports `-> 268 .text` (size fallback); size- and time-scored
      runs write two distinct `so-*.ck`; runaway candidates are killed by the
      per-run watchdog and the search still completes.
- [x] Confirm the W^X / `MAP_JIT` runmem path (src/mcchost.c
      `host_runmem_alloc`) works for candidate JIT on Apple silicon —
      `mcc -run` JIT-executes correctly natively (`host_runmem_dual()`
      detects W^X and takes the RW-alias + RX-view dual-map path; three TUs
      incl. args + fp + calls run to results byte-matching their compiled
      references).

# Superoptimizer / JIT ladder (open)

## Implementation status (2026-07-10, in progress)

**See [STATUS.md](STATUS.md) for the consolidated snapshot** (rung-by-rung
state, user-facing flags/env, architecture, and the de-risked recipes for
the remaining builds). Summary below.

Landed, all presets/ctest green + fixpoint byte-identical at each step:
- **§20 done** — `host_cache_dir()` in mcchost (`97846575`).
- **§21 first increment** — whole-TU resumable checkpoint on the `-O<N>`
  search: warm-start-and-continue, flock + A/B keep-best + durable-atomic
  (`da123a35`). **Second increment (2026-07-10): two-tier per-fn keys** —
  the `MCC_AST_PERFN` search persists a per-function checkpoint
  (`pf-<key>.ck`, key = alpha-renamed §18 intention hash + `MCC_VERSION` +
  triplet; children dump per-fn hashes via `MCC_AST_HASH_OUT`, driver
  adopts converged functions instantly and probes only untried configs,
  same flock/keep-best/durable protocol; `superopt-perfn-cache` ctest:
  cold 0-cached → warm all-cached → edit one fn → only it misses).
  Remaining: two-phase cursor, multi-objective (land with §22/§25).
- **flags** — `--clear-cache`, `--jit-max-duration`, `--jit-functions` +
  `host_rmrf` (`4394057f`).
- **§23 first increment** — inline node/graft budgets are searchable env
  knobs (default-preserving), added to the `-O<N>` space (`7e630c75`).
  Remaining: param shapes, cross-TU static.
- **§25 static objective** — search scores by `.text` size (ELF section
  sum), not whole-object (`382b7169`). Remaining: JIT cpu/mem tier (needs
  the subprocess watchdog).
- **§31 substantially landed** — the search is a **3-strategy portfolio
  scheduler** (gate + budget + promote/opt-limit strategies, greedy
  composition, exponentially-doubling time slices, adaptive base slice,
  resumable per-strategy cursors in the checkpoint, fmt v5 as of
  `676c1836`) with
  **SIGTERM/INT/HUP save-checkpoint-and-stop** and a **per-candidate
  subprocess watchdog** (timeout-kill + abandon-in-flight on stop)
  (`f67c2234`, `c5f3349f`, `35a8ef70`, `e3a2f2d7`). Remaining: the static
  vtable registry refactor, adaptive beam width, per-function scoping.

- **§24 first increment** — the static hot-slice **cost model** (node ×
  loop-nest-depth × call-outs, per function) computed in `ast_func_end`,
  gated behind `MCC_AST_COST` (default off, byte-identity held), reporting
  the ranking (`3e81960d`). Remaining: use it to allocate the search budget
  per function (needs §22).
- **§29 first increment** — redundant integer-cast elimination as a
  provably-correct extension of the proven `ast_ident_run` replay pass
  (identity cast, and lossless widen-then-narrow round-trip) (`ad55ede8`).
  Exec golden across all four replay columns; gcc/O0/O3-tmpl agree.
  Remaining: range-narrowing (needs range analysis), search-gated re-typing.

- **§22 functional** — sidestepped the fragile re-emit rewrite entirely.
  Sub-step 1: `MCC_AST_FN_CONFIG` per-function pass-gate override
  (`323a15a6`). Sub-step 2: `mcc_superopt_perfn` (`MCC_AST_PERFN` + `-O4+`)
  enumerates `STT_FUNC` sizes from the object symbol table and greedily
  picks each function's best config by measured per-function size — **real
  per-function search, no `ast_func_end` surgery** (`11bb8323`). Also gives
  §21 its per-fn tier and §31 its per-fn scoping.
- **§24 functional** — cost model (`3e81960d`) + the per-function search now
  sorts functions biggest/hottest-first so a limited budget lands where it
  pays (`ad5cfee2`).

- **§30 detection increment** — same-key equality-cluster detection over
  the control-flow (`AST_If`) form under `MCC_AST_BITFLAG` (`fb845871`).
  The mask-encoding transform then **landed** (`ast_bf_run`, see the §30
  section), and the threshold is now a **searched dimension** of the
  `-O<N>` budget space (`676c1836`). Remaining: the listed §30 extensions.
- **§26 manifest increment** — `-O4+ --embed-jit` resolves + reports the
  runtime-JIT manifest (`--jit-functions`/`--jit-max-duration`) (`95037e96`).
  Remaining: the runtime engine (ELF ctor + embedded libmcc slice + RCU
  patching) — the large subsystem.
- **§18 done** (`400d144a`) — `ast_intention_hash` public arena API +
  `mcc_intention_hash`/`mcc_cache_dir` LIBMCCAPI; mcchv persistent cache
  v2 keyed by `MCC_VERSION` + triplet + the AST intention hash of the
  canonical baseline kernel, storing the search-resume inputs; §21-style
  durable writes; `ast/intention` + `hypervisor-cache` tests.

**Every rung §18-§26, §29-§32 now has a real, tested increment** (and
§18/§20 are done outright), 1862 ctest + fixpoint byte-identical
throughout. The remaining *full* build is the §26 runtime engine — a
focused session. §27/§28 were deferred behind the value-reference-node
decision (per the design round); that decision is now **resolved in §32**
(feasibility study, 2026-07-10): no new node kind — §27/§28 are unblocked
for their analysis/structural halves, and the value-materializing halves
wait on the §32c synthetic-temp infrastructure. §32a (structured-join
const-prop) and §32b (cross-join CSE/LICM) are **landed** (notes in §32);
the next frontier build is **§32c** (scoped in §32: carve hook +
promo-poison guard + `AST_Convert` wrap + store-dominance rule, then
fresh-temp LICM → PRE → IV strength reduction).


The `-O<N>` (N>=4) compile-time search landed at `f959078e`
(`mcc_superopt_search` in `src/mcc.c`): it re-compiles the whole TU under
different AST pass-configs via child workers and keeps the smallest object.
These items close the gaps that survey exposed — no persistent cache, a
conservative-not-aggressive inliner, and uniform (no hot-slice) budget
spend — ordered simplest → hardest, each building on the previous.

**Curation (2026-07-10, decision round).** Build order starts at the cache
(§20→§21). The cache is a **resumable search checkpoint, not a skip-on-hit
lookup** — on a hit the search *continues* spending this run's budget from
the stored frontier and writes an advanced snapshot back, so budget
accumulates across runs (run 3 picks up where run 2 stopped). Cache keys on
**both tiers** (whole-TU fast path + per-function intention hash). Scoring
is **both**: static object size always, JIT-measured cpu+RSS when the TU is
runnable (§25). The value-reference/SSA node and the transforms it unblocks
(§27 loop interchange, the compositional half of §28) are **deferred** —
parked as research, reassessed only after §20-25 land.

## 20. `host_cache_dir()` — portable per-user cache dir (small)

Shared foundation for §18 and §21/§25/§26. Factor the cache-dir resolver
out of `tools/mcchv.c` (`hv_cache_path`) into `host_cache_dir(char *buf,
int size)` in `mcchost.*`, routed through the host abstraction so
`host-gate-invariant` stays green:
- Linux/BSD/Hurd: `$XDG_CACHE_HOME`, else `$HOME/.cache`, + `mcc/`.
- macOS (`MCC_HOST_DARWIN`): `$HOME/Library/Caches/mcc`.
- Windows (`MCC_HOST_WIN32`): `%LOCALAPPDATA%`, else
  `%USERPROFILE%\AppData\Local`, + `mcc\`.
Create via `host_mkdirs`; return <0 when no home is resolvable (sandboxes)
and keep callers tolerant. Repoint `mcchv` at it with no behavior change.
Builds on nothing.

## 21. Resumable-checkpoint search cache (medium) — NEXT

Today `mcc -O<N>` re-runs the full ~1296-config search cold every
invocation; nothing persists. Make the cache a **resumable checkpoint of
the search**, not a skip-on-hit lookup. Persist per entry: the best config
found so far, its score, and the search frontier (the seeds/permutation
already explored, plus TPE/linear state). On a hit, do NOT stop at the
cached best — reload the frontier and **continue** searching for this run's
full `optimize_search_seconds`, then write the advanced snapshot back. So
run 1 searches N s cold, run 2 warm-starts from run 1's frontier and
searches N s more, run 3 continues again: the budget accumulates across
runs and the best is monotonically non-worse until the space is exhausted
(then hits are instant). Key on **both tiers** — a whole-TU fast path
(hash of the input) and a per-function fallback (per-function `AstArena`
intention hash, §18) so editing one function only re-opens that function's
search. Every key carries `host_cache_dir()` + `MCC_VERSION` + target
triple so a compiler/target bump misses cleanly. Builds on §20; shares
§18's keying discipline. Test: run1 cold → run2 continues (best strictly
non-worse, more configs covered) → edit one fn → only that fn's tier
misses; corrupt/foreign snapshot is ignored not trusted.

Implementation (decided 2026-07-10):
- **On-disk record = raw struct dump.** One `fwrite` of a fixed struct
  `{ u64 version_id, key_hash; u32 best_seed, next_seed; u64 cpu_ns; u32
  peak_kb, size_bytes; }` — the best-so-far plus its three §31 objective
  components. No parser. A struct-layout or `MCC_VERSION` change just misses
  (the key carries the version), so brittleness across builds is harmless.
  Kept current incrementally (see below), so an interrupt/crash loses at
  most the in-flight eval, never committed progress.
- **Frontier = two-phase coarse-to-fine cursor.** The per-strategy search
  is macro-then-micro (§31): a greedy binary divide-and-conquer narrows the
  strategy's `[MIN,MAX)` range (probe lo/mid/hi, keep the better half) until
  the remaining range fits one time slice, then an exhaustive sweep of it.
  Persist the phase, the surviving `[lo,hi)` macro bounds, the micro sweep
  cursor, and the best `{seed, cpu_ns, peak_kb, size}` — so a resumed run
  continues mid-narrowing or mid-sweep exactly where it stopped.
- **Write protocol = lock + durable-atomic + A/B keep-best.** Hold an
  advisory lock on the entry (`host_lock`: POSIX `flock(LOCK_EX)`, Windows
  `LockFileEx`; auto-released on close/crash, no stale locks), re-read the
  current record (B), and write
  back only the better of B vs this run's result (A) by §31's lexicographic
  comparator (faster → memory → size; non-runnable TUs skip the cpu axis);
  on a full objective tie keep the larger `next_seed`. Commit durably:
  write temp → `fsync(temp)` → `rename(temp, target)` → `fsync(dir)`, so a
  crash or power loss can never leave a torn or unflushed record — a reader
  always sees the whole old record or the whole new one, never a fragment.
  A run that made less progress or found a worse best can never clobber a
  better cached optimizer.
- **Incremental, never end-only.** The record is rewritten (durable-atomic,
  above) on a **fixed ~5 s interval** during the search (and immediately on
  the save-and-stop signal, which never waits for the tick) — not just at
  search end. So a readily-available recovery point always exists on disk;
  an interrupt, uncatchable SIGKILL, or power loss lose at most ~5 s of
  progress plus the in-flight eval, never more. This is what lets §31's stop
  return in ~1-2 s without finishing any eval.
- **No eviction.** One small file per TU/fn under `host_cache_dir()`;
  the dir grows with distinct inputs and is user/OS-clearable. Ship a
  documented `mcc --clear-cache` (equivalent to removing the dir).
- **Key = fully alpha-renamed intention hash.** The per-function key
  hashes the replay-relevant fields in canonical child order — kind, op,
  `type_btype`, const payload, `sym_role` — with **every identifier
  normalized to a positional placeholder** (locals/params AND
  callees/globals: 1st-local, 1st-callee, …), excluding addresses,
  capture-order slots, and source positions. **Lookup order: TU-tier first**
  (an unchanged file hits the whole-TU "all converged" marker instantly),
  **else the per-function tier** (only edited functions re-search; the rest
  warm-start from their per-fn hits). So structurally identical
  functions/TUs share a checkpoint regardless of names — maximal hits, and
  safe because a config-only hit merely *warm-starts* a search that
  self-corrects (A/B keep-best). The whole-TU key is the same normalization
  over the post-preprocess form. NOTE: a machine-code / JIT-result tier
  (§18/§25) must additionally key on **real callee identity** (or re-verify
  on hit) — for stored executable bytes a wrong match is incorrect, not
  merely suboptimal.

**Per-fn tier LANDED (2026-07-10).** `mcc_superopt_perfn` persists a
per-function checkpoint `pf-<key>.ck` under `host_cache_dir()`: fixed
`SoPfCkpt {fmt, best_cfg, key, best_size, tried}` raw-dump records, key =
§18 `ast_intention_hash` (alpha-renamed, computed in `ast_func_end` and
exported from the worker child via `MCC_AST_HASH_OUT`, an internal
driver↔child channel like `MCC_AST_FN_CONFIG`) folded with
`MCC_VERSION_STR` + triplet. The driver adopts each converged function's
best config without re-probing (`tried` bitmask over the 3-config space),
probes only untried configs, and writes back after every measurement
(incremental, interrupt-safe) through the same flock + A/B keep-best +
tmp/fsync/rename protocol. Editing one function re-opens only that
function's search; structurally identical functions share entries by
construction. `superopt-perfn-cache` ctest drives cold → all-cached →
one-fn-edit; full ctest 1861/1861 + fixpoint stage2==3==4. Remaining
here: the two-phase coarse-to-fine cursor and the multi-objective record
(land with §22/§25 as decided).

## 22. Per-function search granularity (medium-large)

The search is whole-TU: one global pass-config scored by total object
size. Move the choice into the per-function replay (`ast_func_end`),
picking the size-best pass subset per captured function. This is the unit
§18's intention hash was designed for (per-function `AstArena`) and turns
the §21 cache and the §24 selector per-function. Keep `-O0..-O3`
byte-identical (still gated on `optimize_search_seconds>0`).

Decided 2026-07-10:
- **Per-function replaces whole-TU.** Each function is searched
  independently in `ast_func_end`; the "TU config" is just the composed
  per-function winners, and the whole-TU cache tier degrades to a
  fast-path "all functions already converged" marker. One search model,
  naturally per-function-cached (§21) and hot-sliceable (§24).
- **Config applied in-process via the `do_*` flags.** Replace the
  `MCC_AST_*` env-gate reads on the search path with an in-process
  per-function config struct that sets `do_bfold/promote/inline/...`
  directly — the search becomes an in-compiler loop over one function's
  replay, no `fork`, no env. The env gates stay as a manual override for
  debugging. (Retires the child-process model of §f959078e for the
  per-function path.)

**Blocker found (2026-07-10, from reading `ast_func_end`).** The sibling
passes (`ast_bfold_run`/`ast_ident_run`/…) **mutate the captured `AstArena`
destructively in place**, and the re-emit replays that one mutated tree
once. So a per-function search that tries *several* configs cannot just
re-run passes on the same `ast_cur` — the first config's mutation poisons
the tree for the next. The prerequisite is therefore an **AST clone** (deep
copy of the `AstArena` for the function) so each config attempt starts from
a pristine capture; then, per attempt, reset `ind` / the positional pools
(`ast_fconst_i`, `ast_locrec_i`) exactly as the single re-emit does today,
replay, measure the emitted length (`ind - ast_body_ind_sv`), and keep the
smallest. That clone + repeatable-reset is the real work of §22 and must be
validated across all four exec-replay columns + fixpoint at each step.

Builds on §21 + the capture/replay driver.

## 23. Widen the inliner envelope — the "aggressive" mode (medium-large, correctness-sensitive)

`ast_inline_*` is deliberately conservative: static-only, node budget 64,
<=6 scalar/simple-struct params, depth 8, graft budget 2048, excludes
VLA/setjmp/complex — complete within that envelope, not aggressive. Behind
searchable gates (`MCC_AST_INLINE_LIMIT` and new siblings), widen it:
larger node/graft budgets, more param shapes, cross-TU static callees,
heuristic non-static inlining — each extension guarded so the
capture/replay byte-identity invariant and the exec-replay inline column
stay green, and each added to the §22 search space so the compiler
auto-tunes the limits instead of hardcoding them. Builds on the existing
inliner + §22.

Decided 2026-07-10: extension order = **(1) bigger node/graft/depth
budgets, (2) more param shapes, (3) cross-TU static callees** — biggest
wins and least new correctness surface first; heuristic non-static
inlining stays last/optional. Each lands byte-identity-gated with the
inline exec column, then becomes a §22 search knob.

## 24. Hot-window / slice selector (large)

The search spends the budget uniformly, with no notion of where effort
pays. Add a static cost model (captured node count, loop-nest depth from
the `AST_If` op 2..5 forms, call-out count as a hotness proxy) that ranks
functions and allocates `optimize_search_seconds` to the top slices
first, so `-O128` deepens the search on the functions that dominate
size/cost instead of re-confirming trivia (today O4 and O128 converge to
the same output). Builds on §22 (+ benefits from §23's widened space).

Decided 2026-07-10: rank by **-g profile entry-frequency when present**
(the §25 value cache extended with function-entry counts), else fall back
to the **static proxy `node# × loop-nest-depth × call-out-count`**.
Accurate with a profile, still works without one.

## 25. Frontend-JIT candidate measurement — the second scoring tier (large)

§21-24 score by static object size — always available, but a proxy, not
runtime cost. Add JIT measurement as a **second scoring tier**, not a
replacement: static size stays the default/fallback objective; when the TU
has a runnable entry/harness, JIT-run each candidate (`MCC_OUTPUT_MEMORY` +
`mcc_relocate` + `-run`, the `tools/mcchv.c` model), timing + RSS-sample
it, and let the measured cpu+memory decide — the original `-O<N>` intent
("more efficient as told by both memory and cpu"). Non-runnable/library
TUs transparently keep the size objective. Reuse §18's JIT-result cache.
Builds on §22 + §18.

Decided 2026-07-10:
- **`--jit-functions=<sym,...>` (default `main`)** selects what to
  benchmark/optimize. JIT sites attach at the **lowest common ancestor**
  (dominator) of the listed symbols in the static call graph
  (`main,child1,child2`→ main; `child1,child2`→ their shared parent);
  functions in disjoint call trees get one site per independent root;
  indirect/recursive calls are opaque, so attach at the nearest known
  ancestor. When neither a `main` nor a named function is runnable the TU
  falls back to the size/memory objective.
- **`-g` hot-value cache** (debug builds): instrument `-g` builds to log
  function-argument and branch/switch **key values with frequencies** into
  a value cache stored beside the §21 optimization cache (same
  `host_cache_dir()`). The search then sets each strategy's `MIN..MAX` to
  the observed hot range and **frequency-weights** its probe order —
  optimizing for the values that actually run. Feeds §29 (Convert ranges)
  and §30 (bit-flag bucket quantization) directly. Absent the cache, the
  search uses default full ranges.
- **Measurement = best-of-K min wall-ns + peak RSS, adaptive K.** Score cpu
  by the **minimum** wall-ns over K taskset-pinned runs (noise only ever
  adds time, so min is the cleanest true-cost estimate), plus peak RSS from
  `getrusage`. `K = 3..15`, growing while the min's relative stdev stays
  > 5% (cheap on stable hosts, more samples only when noisy). Portable (no
  perf counters).
- **`-g` logging mechanism.** Under `-g` the compiler inserts minimal
  logging at fn-entry and branch/switch keys into an in-memory freq-counted
  table (grows freely, bounded only by a cap on the flushed file size),
  flushed to the value cache on exit — accumulating across runs.
- **Oracle inputs (shared by §28/§29/§30).** The differential-equivalence
  test runs original vs transformed over `harness inputs ∪ -g hot values ∪
  per-type edges` (min/max/0/overflow boundary/non-representable float) —
  consistent with scoring, targeting values that actually occur.
- **Sampling-soundness policy.** A sampled/differential-only equivalence is
  **never** trusted in the shipped `-O0..-O3` static object — that requires
  a static proof or exhaustive enumeration. The **runtime JIT (§26) may**
  fire sampled-equivalent transforms because it shadow-validates each
  against the original on real inputs before swapping and rolls back on any
  divergence. Keeps the binary provably correct; lets the runtime speculate.

## 26. `--embed-jit` runtime self-optimizer (largest — run LAST)

Make `--embed-jit` (default on, flag plumbed at `f959078e`) do its runtime
job: embed an always-on optimizer into the output that, while the program
runs, JIT-recompiles its own hot functions in a background C11-thread pool
and hot-swaps the faster versions in place — the original hypervisor
vision, bounded at *runtime* by `--jit-max-duration` (default 600 s), not by
the compile-time `optimize_search_seconds`. Add `--jit-max-duration=<sec>`
(default 600, `0`=unlimited) alongside the existing `--embed-jit` flag.
Needs an ELF `.init_array` ctor, an embedded libmcc/JIT slice, the captured
intention trees carried in the binary, and safe live function patching.
Builds on §25 (JIT measurement) + §21 (cache).

Decided 2026-07-10: the embedded runtime optimizer runs to the **smaller of
permutation exhaustion or a wall-clock cap**.
- **Permutation space = `(MAX − MIN) × strats × depth(k)`** — the value
  range each strategy sweeps (`MIN..MAX`), times the number of §31
  strategies, times the best-carry depth `depth(k)`.
- **`k` is adaptive** — set from the wall-clock time the *previous*
  iteration took: a fast iteration deepens `k`, a slow one shrinks it, so
  each iteration stays inside its time slice (never a days-long single
  step).
- **Wall-clock cap = `--jit-max-duration` seconds (default 600).** A
  compile-time flag baked into the binary, adjustable up/down at compile
  time; `0` = unlimited ("converge forever"). Distinct from the
  compile-time `optimize_search_seconds` (§31) — this bounds the *runtime*
  optimizer.
- Resumes across program runs from the §21 checkpoint shipped inside the
  binary, so a long-lived or repeatedly-run program converges over time
  rather than re-searching cold; the cap applies per program run.
- **Sites = the `--jit-functions` LCA set (§25).** The runtime workers
  embed at the same lowest-common-ancestor sites `--jit-functions` selects
  (default `main`), so the compile-time and runtime optimizers target the
  same code.
- **Live patching = atomic function-pointer indirection + triple-buffer /
  RCU reclamation.** Optimizable calls dispatch through a swappable pointer
  slot; the optimizer builds the new version into a spare buffer and
  `atomic_store`s the slot — lock-free, no code overwrite, no W^X. In-flight
  calls finish on the old body; a **retired body is freed only after
  consensus** via **epoch-based reclamation (RCU)**: a global epoch counter,
  threads announce their epoch at call boundaries, and a body swapped out at
  epoch E is freed once all threads have advanced past E. **Triple**
  buffering gives slack so the writer rarely stalls on reclamation.
- **Hot detection = per-site atomic call counters, hottest first.** A cheap
  `atomic ++count` at each site orders the work (highest count first) and
  also refines the `-g` hot-value cache (§25) / hot-slice ranking (§24).
- **Shadow-validate before swap.** Because the runtime may use sampled
  equivalence (§25 policy), each candidate is shadow-run against the
  original on real inputs and only swapped in if it matches; any later
  divergence rolls back to the previous buffer (the triple-buffer keeps it).
- **Thread pool = default `cores-1`, `--jit-threads=<n>` override.** Scales
  to spare cores for fast convergence, leaving one for the program; tunable
  per build.

**Infra found (2026-07-10).** The pieces exist: mcc already emits
`.init_array` constructors (`SHT_INIT_ARRAY` in ELF/Mach-O/PE;
`__attribute__((constructor))`), and the JIT half is the `-run` path —
`MCC_OUTPUT_MEMORY` + `mcc_relocate` (`src/mccrun.c`) + `host_runmem_alloc`
(RWX memory). So §26 assembles as: (a) at `-O4+ --embed-jit`, synthesize a
constructor into `.init_array` that spawns the `--jit-threads` pool; (b)
embed the per-function intention trees (`AstArena`, §18) as a data blob in
the binary; (c) the pool recompiles hot functions via the embedded
`mcc_relocate` JIT and hot-swaps through the atomic-pointer slot with
triple-buffer/RCU reclaim. **The dominant cost is embedding a libmcc slice
(the ~800 KB compiler) + the intention blob into every `-O4+` output** — a
size/build-system problem as much as a codegen one. That's why it's the
largest, run-LAST rung.

## 27. Loop-nest reordering / interchange (very large — DEFERRED, prerequisite-gated)

Deferred (2026-07-10): parked as research behind the value-reference/SSA
node decision; do not start until §20-25 land and that node is reassessed.
**Reassessed — see §32**: the legality analysis (conservative dependence
test) and the interchange rewrite (structural subtree swap) need no new
node; only value-materializing follow-ons wait on §32c.
No loop-nest transform exists today (only LICM + TCO touch the `AST_If`
op 2..5 captures; the nest is never rearranged). Interchange (`for x; for
y` → `for y; for x`), fusion, and tiling all need what the frontier is
blocked on: a value-reference / SSA-like node so a subscript's provenance
is analyzable. Prereq: land that node (unblocks CSE/GVN too). Then: a
loop-nest model over the op 2..5 forms, a conservative dependence test
(array-subscript direction vectors; bail to "no" on anything unproven), a
legality check, the interchange rewrite, and a re-run of the §22
per-function search so opportunities are re-evaluated *after* the nest
changes. Builds on the frontier value-reference node + §22. Gate every
rewrite on `-O0..-O3` byte-identity + a new loop-nest exec-golden column.

## 28. Dynamic algorithm generation / pass composition (very large — DEFERRED, research)

Deferred (2026-07-10): the compositional/rewrite-rule half waits on the
value-reference node reassessment after §20-25; only revisit then.
**Reassessed — see §32**: the rule engine and algebraic/structural rules
are φ-free over the existing retag primitives; only rules that materialize
a shared runtime value wait on §32c.
Both searches today tune a fixed pass menu; neither composes primitives
into new transforms. Replace the menu with a small rewrite-rule IR — a
peephole grammar of match→rewrite templates over the captured `AstArena`
— that the §22/§24 search *combines* into compound transforms (a
candidate = an ordered rule sequence, the "0..maxuint" seed decoding to a
rule program), scored by §25 JIT measurement and cached by §21. Optional
stretch: instruction-level superoptimization over a fixed emitted window
(enumerate/lower short instruction sequences, keep the observably-equal
fastest). This is the only rung that makes `-O<N>` genuinely *generate*
algorithms rather than select among hand-written ones. Builds on §22 +
§24 + §25; needs a soundness oracle (differential test each synthesized
rule against the faithful replay before it may fire).

## 29. `Convert` representation optimizer — search type conversions (large, correctness-sensitive)

PHASE-(a) INVESTIGATED (2026-07-10): the "standalone bounded safe-narrowing
first cut" does NOT decompose into a clean local pass in this architecture — it
reduces to the Bucket-B range-analysis project. Reasons (all from source):
(1) `AST_Binary` replay recomputes the result type from operand types via
`gen_op` (`mccast.c:2516-2533`) and ignores the node's stored type, so a
computation cannot be narrowed by retyping its Binary node — only `AST_Convert`
/`AST_Literal` have emission-driving types; (2) `gen_cast` constant-folds a cast
of a literal (`mccgen.c:3278-3332`), so narrowing a constant or exact float
literal is a byte-identical no-op; (3) there is no range/known-bits facility —
`ast_cprop_*` tracks only exact constants (`ast_cprop_kval`); (4) the one useful
local narrowing (`(i64)(i32)e`→inner, widen-of-narrower) is ALREADY handled by
the landed redundant-cast pass (`ast_ident_convert`, `mccast.c:3611-3620`). So
the local-only provable-safe narrowing set is empty (already-handled or no-op).
Every *beneficial* narrowing (wide expression computed in a narrower type) needs
an integer range/known-bits lattice + a use-site (context) oracle + multi-node
restructuring (following the `ast_bf_build`/`ast_tco_run` node-construction
precedent) — i.e. phase-(b), folded into the §22 search under the §28 oracle as
already planned. Tracked as Bucket-B; nothing safe to land standalone.

A search-driven pass that tries re-typing values via the `AST_Convert`
node: for each value/expression, cast it to alternative representations
(narrow/widen integer widths, signed↔unsigned, int↔float↔double) and keep
the one that scores best (§25 size/JIT) among those **provably
value-equivalent**. "Cast to all other types" is a brute-force dimension
over the type lattice per value, so it is a *search* transform, not an
always-on pass, and every candidate cast is gated by the same soundness
oracle §28 needs: the re-typed computation must round-trip / stay in range
(static range analysis, or a differential test against the faithful replay
before the cast may fire). Wrong casts change semantics — the gate is
hard: bail to the original type on anything unproven, never fold a cast
that could overflow/lose precision/alias-pun. Staged: (a) standalone
bounded first cut — provably-safe narrowing only (`i64→i32` when the value
range fits, `f64→f32` when exactly representable), lands like the other
AST passes; (b) full form — the §22 search picks per-value representations
under the oracle, scored by §25 and cached by §21. Builds on §22 + §25 +
§28's soundness oracle. Gate on `-O0..-O3` byte-identity + an exec-golden
column that checks the re-typed result equals the original across edge
values (min/max, overflow boundary, non-representable floats).

Decided 2026-07-10:
- **Oracle = static range proof, differential-test fallback.** Fire when
  statically proven (value range fits the narrower type; float exactly
  representable). Else, if the TU is JIT-runnable, run original vs re-typed
  over edge + sampled inputs and fire only on a match (bit-identical by
  default); else keep the original type. Shares §28's oracle.
- **Conversion set = integers + int↔float.** Integer narrow/widen and
  signed↔unsigned where range-safe; `int↔f32/f64` where exact. No
  pointer/aggregate re-typing (aliasing/ABI hazards).
- **Float rule = exact by default, within-ulp behind an opt-in flag.**
  Default folds float re-typings only when they round-trip bit-exactly
  (byte-identity preserved). A relaxed-FP flag (the `-ffold-math` family)
  additionally allows ≤k-ulp precision loss — never on by default; under
  it the differential test's criterion loosens from bit-identical to ≤k
  ulp.
- **Staging = standalone safe-narrowing pass first.** Phase (a): a bounded
  pass at `-O1+` doing only provably-safe integer narrowing (byte-identity
  gated), landing like the other AST passes. Phase (b): fold the broader
  oracle-gated re-typing (incl. exact/relaxed float) into the §22 search,
  scored by §25 and cached by §21.

## 30. Bit-flag conditional optimizer — permute quantized bit encodings (large, correctness-sensitive)

A search-driven pass that finds conditionals dispatched on one or more
integer-valued keys (variables, enums, whole-number values) and re-encodes
the branch logic as bit-flag tests over a packed bitfield, searching
permutations of the bit layout for the encoding that scores best (§25
size/JIT). It recognizes clusters of comparisons / `switch` arms keyed on
the same value(s), packs the predicates into a mask, and replaces the
branch cascade with bitwise tests / a computed index (bit-test, `popcount`,
mask-then-jump-table). "Permute quantized bit flags" = the search
dimension: the assignment of predicates→bits and of value ranges→quantized
flag buckets is permuted to minimize branches / maximize bit-parallel
evaluation. Every candidate encoding is gated by the §28/§29 soundness
oracle — the re-encoded dispatch must yield the identical outcome for every
key value: exhaustive static enumeration when the key domain is small or
enum-bounded, differential test otherwise. Complements §29 (which re-types
the values; this re-encodes the conditionals over them) and can compose
with it in the §22 search. Builds on §22 (search) + §25 (scoring) + §28's
oracle. Gate on `-O0..-O3` byte-identity + an exec-golden that sweeps every
key value through original vs re-encoded dispatch.

Decided 2026-07-10: detection scope = **same-key comparison clusters +
`switch` arms** (if/else-if runs and switches testing the same key(s)
against constants/enum labels) — a finite, enumerable key domain the oracle
proves exhaustively. Boolean bundles and range/interval quantization are
later extensions, not the first cut.

**Finding (2026-07-10, from a detection prototype).** A short-circuit
`if (x==a || x==b || x==c)` is **lowered to control flow (nested `AST_If`
branches on the same key), NOT an `AST_Binary` `TOK_LOR` node** — only the
*value* form (`int r = a||b`) yields a binary node. So §30 detection can't
walk expression nodes; it must analyze the `AST_If` chain: an `AST_If`
whose condition is `key==const`, whose else-branch is another such
`AST_If` on a structurally-equal key, etc. That control-flow walk (plus the
`switch` form) is the real detection half of §30, and building the bit-mask
replacement then needs new multi-node AST construction (the pattern no
current pass uses). Both are why §30 is a focused effort, not a quick
expression-pass extension.

**Recipe (2026-07-10, de-risked).** Node-building in a pass is a *validated*
pattern — `ast_tco_run` (src/mccast.c ~4178) already builds `AST_Ref/
Convert/Store/Jump` and re-parents the root, and the re-emit handles it. So
§30's transform follows it. Correct **branchless, UB-free** encoding (no
control-flow restructuring, no short-circuit):
`res = ((unsigned)key < 64) & (int)((MASK >> ((unsigned)key & 63)) & 1)`
— the `& 63` keeps the shift amount in `[0,63]` (no UB at any key), the
`< 64` guard zeroes out-of-range keys; verified at `key`∈set / ∉set / =64 /
=−1. Build ≈13 integer nodes (two `Convert(key→unsigned)`, `<`, `&63`, the
`MASK` u64 literal, `>>`, `&1`, outer `&`) reusing a deep-cloned `key`
subtree (`ast_dup_sub`); adopt into the cluster root. Reads of a duplicated
local don't call `ast_alloc_loc`, so the `ast_locrec` pool is unaffected —
the desync risk that blocked it is gone. Gate behind the
existing `MCC_AST_BITFLAG` (default off) and validate at edge keys across
all four exec-replay columns + fixpoint.

**Empirical finding (2026-07-10, prototyped + reverted).** There is **no
expression-level shortcut**: `||` is *always* lowered to control flow in
the captured AST — even a materialized `10 + (x==1||x==3||...)` does **not**
produce an `AST_Binary TOK_LOR` node. A value-form pass built to the recipe
above compiled and was correct but **never fired** on any real `||`. So the
transform must operate on the **`AST_If` chain** (the detection form): read
the chain's per-arm `key==const`, and — following the `ast_tco_run`
branch-restructuring precedent (it already rewrites control flow: labels +
jumps + built nodes) — replace the chain with a single `AST_If` whose
condition is the branchless bit-test above, sharing the arms' body (when
they're identical) or a mask-indexed value table (when they differ). The
node-building and the bit-test formula are validated; the remaining work is
the `AST_If`-chain collapse, which is the genuinely fragile part.

**LANDED (2026-07-10) — `ast_bf_run` transform, both forms.** Correction to
the finding above: it holds only for the *materialized value* form (which
desyncs capture). In **condition position** `x==a||x==b||x==c` is captured
as one flat n-ary `AST_Binary TOK_LOR` whose children are the `==`
comparisons (see `ast_hook_landor_operand`; replay re-materializes the
short-circuit via `gvtst`). So the transform has two entry points sharing
one builder (`ast_bf_build` + new deep-clone `ast_dup_sub`):
`ast_bf_try_lor` rewrites a same-key LOR-of-eq node in place (covers `if`,
`while`, `do`, ternary conds), and `ast_bf_try_if` collapses the else-if
`AST_If` chain (identical `ast_ident_same` bodies, read-only walk first,
consumed nodes retagged `AST_Poison`, final non-matching `else` survives as
the collapsed else-arm). **Replay hazard found and designed around:** the
recipe's operand order `guard & bit` miscompiled — replay pushes leaves
without the parser's `vcheck_cmp`, so the `VT_CMP` produced by `<` is
clobbered by the shift-chain emission (same class as the arm64
`vcheck_cmp`-before-`gfunc_call` fix). Encoding is therefore reversed —
`(int)((MASK >> ((unsigned)key & 63)) & 1) & ((unsigned)key < 64)` — so the
comparison is consumed by `gen_op('&')` immediately. Guards: key
`ast_ident_pure` + `ast_ident_intt`, consts `< 64` via `ast_ident_cval`,
key width picks u32/u64 mask math (`VT_LLONG` keys use 64-bit unsigned
ops — no truncation at keys ≥ 2³²), label-bearing subtrees never dropped.
Threshold: `MCC_AST_BITFLAG=N` (N≥3) with plain `=1` defaulting to 5, the
measured x86_64 size break-even (mask test ≈ constant 151 B vs ≈12 B per
compare+branch rung; n=5 equal, n≥6 smaller, plus branch-count win).
Validated: edge keys −2³¹/−65/−2/−1/0/1/5/7/33/62/63/64/2³²+5 across
int/uint/llong/ullong/char/short keys and lor/chain/chain-no-else/mixed/
while/ternary forms, hash-checksummed against gcc and `-O0..-O3` gate-off
(byte-identical rc); must-not-fire shapes verified (consts ≥64, cnt<min,
differing bodies, impure key, negative consts); `cli/bitflag_transform`
added (ctest 1858/1858); fixpoint gate stage2==3==4. Remaining §30
extensions (search-integration era): value-table dispatch for differing
bodies, `switch`-arm form, `&&`-of-`!=` complement, biased encodings for
const ranges not in `[0,63]`.

**Search registration LANDED (2026-07-10).** The threshold is a fourth
budget dimension of the `-O<N>` whole-TU search: `so_setenv_cfg` sets
`MCC_AST_BITFLAG` from `so_bf[] = {0, 3, 5, 9}` (budget space 9→36,
checkpoint fmt 4→5 so stale snapshots miss cleanly), and the per-cluster
`bitflag-candidate` report is suppressed inside search workers. Verified
end-to-end: on a 7-way same-key `||` cluster the search's checkpointed
winner is `best_budget=9` — bitflag threshold 3 beats every off-config on
`.text`. **Bugfix caught while validating:** `so_copy` wrote the winning
candidate with `fopen("wb")` mode 0644, so every *fresh* `-O4+`
executable since `f959078e` lost its exec bit — it now propagates the
candidate's mode. ctest 1861/1861 + fixpoint green.

## 31. Strategy-portfolio scheduler — the governing search architecture (large)

Unifies every optimizer methodology under one meta-search. Each methodology
is a **strategy**: a black-box optimizer that, given an iteration budget,
returns its **best(k)** candidates — the pass-config search (§22, the first
strategy, ex-§f959078e child model), the aggressive inliner (§23), Convert
re-typing (§29), the bit-flag encoder (§30), and later the rewrite-rule
generator (§28). Each registers itself; strategies land independently.

Schedule = **exponentially-deepening round-robin over TIME**. Every
strategy is **strictly time-bound, never depth/iteration-limited** — each
gets a wall-clock slice and runs as many iterations as fit. Round 1 gives
every strategy 1× the base time slice, round 2 gives 2×, round 3 4×,
doubling each round until the total `optimize_search_seconds` is spent
(`strat1*1, strat2*1, strat3*1 ; repeat *2 ; repeat *4 ; …`). A strategy's
best(k) from a round **seeds its next, deeper round** (warm-start,
persisted via the §21 checkpoint), so deeper rounds refine rather than
restart; a strategy that goes 2 rounds with no best(k) gain is dropped and
its time reallocated to the rest.

Selection = **always take the global best by the §25 multi-objective**,
lexicographic **faster → lowest peak memory → smaller size** (cpu measured
by JIT when the TU is runnable). For a library/non-runnable TU the "faster"
axis has no measurement, so selection falls back to **lowest peak memory →
smaller size**.

Composition = **beam search over oracle-gated pipelines** (decided). Build
the result as a sequential pipeline — apply strategy A's best, then run B
on the result, etc., oracle-gating each step so it stays sound — but keep
the top-**M** partial pipelines at each step (beam), trying each strategy's
best(k) as the next stage. This compounds transforms (Convert→bit-flag→
inline) while exploring orderings/combinations, pruned to M. It's a strict
superset: `M=1` = greedy sequential; `M→∞` over all orderings =
oracle-gated cross-product. Chosen over pure cross-product (2^strats ×
orderings, budget spent on bookkeeping) and over pure greedy (phase-order
blind). **M is adaptive to observed synergy**: start `M=1` (greedy) and
widen the beam only when the round's 2nd-best partial pipeline would have
overtaken the winner under a different continuation — i.e. only when
ordering/composition demonstrably matters — so beam width is spent exactly
where compounding pays, not blindly with the budget.

**Save-checkpoint-and-stop interrupt.** The `-O4+` search must stop within
~1-2 s of a kill signal without waiting on any in-flight work, so recovery
never depends on doing work at stop time:
- **Signal = SIGTERM (primary) + SIGINT + SIGHUP.** SIGTERM is what the OS
  / systemd / `kill` send for graceful shutdown (a grace window precedes
  the uncatchable SIGKILL); SIGINT is Ctrl-C, SIGHUP is terminal loss.
  SIGKILL/SIGSTOP can't be caught — which is *why* recovery leans on §21's
  incremental checkpoint, not on the handler running.
- **Async-signal-safe handler.** It only sets `volatile sig_atomic_t stop
  = 1`; all filesystem work stays in the main loop (no FS/malloc in the
  handler).
- **Abandon the in-flight eval — don't finish it.** A single candidate
  eval can be arbitrarily long (a deep-round JIT run), so on `stop` the
  search drops the running eval (its result isn't needed — best-so-far is
  already on disk from §21's incremental writes) and returns within the
  bound. A watchdog kills/abandons the eval subprocess/JIT so it can't
  stall the exit.
- **Per-candidate eval timeout — subprocess watchdog.** Each eval runs in
  a **forked subprocess**; the parent enforces the time bound by killing it
  (POSIX `fork` + `waitpid` + `SIGKILL`; Windows `CreateProcess` +
  `TerminateProcess`). This is the most portable, robust isolation — a
  hung, crashing, or runaway eval can never stall or take down the parent,
  and killing a subprocess is universally reliable, unlike in-process
  `alarm`/thread-cancellation (fragile, non-portable, unsafe mid-JIT). An
  overrunning candidate is killed and abandoned as no-improvement. This is
  also what gives the stop path its teeth: on `stop`, kill the eval
  subprocess and return immediately — never wait on it.
That bound is compile-time only; the embedded runtime JIT (§26) is
unbounded.

Per-strategy search shape = **macro binary narrow, then micro exhaustive
sweep** (decided). Within its `[MIN,MAX)` range a strategy first probes
lo/mid/hi and keeps the better half, recursing until the **whole remaining
range can be swept within the strategy's current time slice** (adaptive
micro-threshold — fast evals sweep a wide range, slow evals narrow further
first), then exhaustively sweeps it. The macro phase finds the promising
coarse region in ~log2 probes; the micro sweep finds the local optimum
inside it. `MIN..MAX` is seeded from the `-g` hot-value cache when present
(§25) so ranges track values that actually run. State (phase / macro
bounds / micro cursor) lives in the §21 checkpoint, so this is fully
resumable and interruptible.

This is the harness §22/§24/§25 plug into: §24 (hot-slice) becomes the
per-round time allocator across strategies × functions; §25 is the scoring
oracle it calls. Builds on §22 + §25 + §21.

Decided 2026-07-10: lexicographic **faster → memory → size** (non-runnable:
memory → size); strategies **strictly time-bound**, not depth-limited;
best-carry `k = 4` (default); rounds double the time slice until the budget
is spent, early-dropping a strategy after 2 no-gain rounds; interruptible
via the save-checkpoint-and-stop signal. **Strategy API = static vtable in
a registry** `{name, [MIN,MAX), decode(seed)→config, apply(config,ast)→ast',
probe(range,budget)→best(k)}` — `apply` (pure) composes in the beam, `probe`
drives the search, scheduler owns control flow. **Base time-slice
adaptive**: `first_eval_time × factor`, so the round-1 ×1 slice sizes to the
workload's per-eval cost. **Micro-threshold adaptive** (sweep when the range
fits the slice, above). Starting constants: base-slice `factor = 8` (round 1
fits ~8 evals/strategy; rounds 2/3 → 16/32). **Strategy order within a
round: cheapest-eval first** (quick wins seed the beam), then by recent
best-gain so productive strategies get their slice while budget is fresh.

## 32. Value-reference node — feasibility study RESOLVED (2026-07-10)

**Verdict: no new node kind is needed.** The frontier's single "missing
AST value-reference node" blocker (OPTIMIZE.md Frontier; §27/§28 deferral)
conflates **four distinct mechanisms**, and the study (three horizontal
slices over the node/arena machinery, the replay/emitter contract, and the
blocked consumers, plus an end-to-end vertical trace) separates them:

1. **Retag-to-literal (analysis only).** A proven-constant use rewrites in
   place to `AST_Literal` via the existing `ast_ident_setlit`
   (src/mccast.c:3478) — no runtime value is ever materialized. Covers the
   **SCCP value-lattice half** entirely: extend `ast_cprop_block`
   (src/mccast.c:3849) from per-block to structured-join dataflow — fork
   the per-offset lattice (`ast_cprop_koff/ktt/kval`) at `AST_If` op 0
   arms (fork = memcpy, meet = intersect-equal-values), pre-kill
   loop-written offsets via `ast_licm_written` (src/mccast.c:4337) for op
   2..5 instead of a fixpoint, bail on any label-def region
   (`ast_sccp_has_label`, the established soundness device). Also covers
   **§27 interchange legality** (conservative dependence test from
   per-offset reaching-defs + affine-IV recognition, bail-to-"no") and the
   **§27 rewrite** (pure structural swap of the nested op 2..5 subtrees)
   and most of **§28** (rule engine over the existing retag primitives).
   The "§27/§28 blocked on the node" framing is over-conservative for
   these — only their value-materializing sub-cases inherit mechanism 3.
2. **Existing-named-local reuse (landed; extend across joins).** The
   CSE/LICM trick (`ast_cse_setref`, src/mccast.c:4281). Carrying the
   availability table into dominated child regions instead of clearing it
   at every If/Invoke/loop boundary (`ast_cse_block`, src/mccast.c:4702 —
   the same move `ast_licm_at_loop` already makes across the back-edge)
   yields dominator-based available-expressions — the majority of
   practical CSE/GVN. Never merges two values, so no φ.
3. **Fresh persistent temp — feasible with zero new node kinds.** A pass
   builds ordinary `AST_Store`/`AST_Ref` nodes (the exact `ast_tco_run`
   shape, src/mccast.c:4182-4192: `op=VT_LOCAL|VT_LVAL`, `ival=offset`,
   `sym=0`, value wrapped in `AST_Convert`) whose offset is a **fresh
   negative frame slot carved directly from the `saved_loc` floor** —
   `loc = (loc - size) & -align`, bypassing `ast_alloc_loc` entirely, the
   proven promote-fix mechanism (`ast_promo_entry_init`,
   src/mccast.c:2328). This sidesteps the `ast_locrec` positional-pool
   desync completely. Verified end-to-end: `gfunc_epilog` frame patching
   includes carved slots on every backend (x86_64-gen.c:1560,
   arm64-gen.c:1625, riscv64-gen.c:848); inline-graft bias + `frame_size`
   capture (src/mccast.c:2024, 2402-2412) already accommodate synthetic
   offsets; the reemit sweep (src/mccast.c:5107-5119) picks them up
   provided they are strictly negative; PE frame-bottom hazard avoided by
   construction (rbp-relative, above the outgoing-call area). Arch-neutral
   (only the register-promotion bonus is x86_64-gated). Unblocks
   **post-join PRE**, **IV strength reduction** (init/incr insertion
   points confirmed: dedicated incr BasicBlock in op 3/5 captures,
   src/mccast.c:1111, replay 2777-2817), and **fresh-temp CSE/LICM**.
4. **True φ-SSA node.** Genuinely required only for cross-join
   value-number merges (arms compute the same expression from *different*
   operand values, merged result reused). Smallest payoff, largest cost —
   **rejected**; passes bail on this case (the
   `ast_licm_operands_ok`-style guard already refuses it).

**Obstacles for mechanism 3** (from the vertical trace), one fatal unless
guarded:
- **B (fatal without guard):** promotion picks up the synthetic temp (a
  scalar local whose address is never taken, so never poisoned —
  `ast_plan_promotion` src/mccast.c:2153-2293) and `ast_promo_entry_init`
  entry-seeds its register from the **uninitialized** slot. Guard: add
  temp offsets to the promotion poison list (first), or prove
  store-dominates-all-refs (later, makes temps register-promotable).
- **A:** no carve hook exists between `loc = saved_loc`
  (src/mccast.c:4963) and `ast_promo_entry_init` (4977) — one must be
  added; temp carve must run **before** the promote save-slot carve. Bake
  offsets from `saved_loc` at transform time (`loc == saved_loc` there,
  a property of pass-1 faithfulness), reserve `loc` at the hook.
- **C:** stored values must be materialized (wrap in `AST_Convert` like
  TCO) — a bare `VT_CMP`/logical RHS is position-sensitive at `vstore`
  (the FIX.md `vcheck_cmp` lesson class).
- **D:** a temp `Store` must dominate every `Ref` (no conditional def
  with post-join reads); also the trigger for B.

**Per-consumer minimal mechanism:**

| Consumer | Mechanism | Coverage |
|---|---|---|
| SCCP value-lattice | 1 (analysis only) | ~full, over int non-escaping scalar locals |
| GVN dominator core | 2 (carry table across joins) | majority of real CSE |
| GVN post-join PRE | 3 (hoisted temp; memory is the merge) | adds partial redundancy |
| GVN value merge | 4 — rejected | bail |
| IV strength reduction | 3 | full over affine IVs in op 2..5 loops; pow-of-2 already backend-native (§iter-23 negative result) |
| §27 legality + rewrite | 1 + structural edit | conservative interchange; SSA only tightens the test |
| §28 rule engine | 1 (+3 for value-materializing rules) | algebraic/structural rules φ-free |

**Build order (decided recommendation):**
1. **§32a — SCCP value-lattice, analysis-only** (cheapest, no temp infra;
   pairs with the shipped `ast_sccp_run` branch half).
2. **§32b — cross-join CSE extension** (no temp infra; extends the landed
   named-local trick through dominated regions).
3. **§32c — synthetic-temp infrastructure** (carve hook + promo-poison
   guard + `AST_Convert` wrap + store-dominance rule), then consumers in
   order: fresh-temp LICM (smallest step from landed code) → post-join
   PRE → IV strength reduction.
4. **φ-SSA node: rejected** — revisit only if the bail-rate of §32b/§32c
   measures as significant on real corpora.

**§32a scoping findings (2026-07-10, from reading the live machinery —
implementation not yet started):**

- **Op maps (authoritative, from the capture hooks).** `AST_If` op: 0 =
  if/else (children cond, then-bb, else-bb), 2 = `while`
  (`ast_hook_while_begin`, mccast.c:1059), 3 = `for` with cond (:1154),
  4 = `do`-`while` (:1090), 5 = `for` **without** cond (:1154) **and
  ternary** (`ast_hook_ternary_begin`, :889), 6 = `switch` (:1246).
  `AST_Jump` op: 0 = break, 1 = continue (:1142), 4 = label def (:4275,
  what `ast_sccp_has_label` tests), 5 = goto **and** the TCO-built
  internal jumps (:4265).
- **The op-5 overload is a classifier hazard.** The recurring "loops are
  the op 2..5 forms" phrasing (§24/§27/§30) is wrong at the edges: an
  op-5 `AST_If` is *either* a `for(;;)` loop *or* a ternary — indeed
  `ast_cprop_safe` whitelists If op 5 as a pure *expression* form
  (mccast.c:3839-3842). Any structured-dataflow or loop-nest pass must
  disambiguate by structure (loop forms are built through the `ast_cf_*`
  stack with body/incr BasicBlocks; ternaries through `ast_tern`),
  never by op alone.
- **Lattice machinery facts.** `ast_cprop_*` is a 3-parallel-array map
  (`AST_CPROP_MAX` 128) with swap-delete `ast_cprop_kill` — any meet loop
  must not advance past a killed slot. `gen` is guarded by
  `ast_cprop_escapes` (whole-arena address-taken scan), so tracked
  offsets are provably call-clobber-free — the flat pass's reset on
  calls is conservatism, not a soundness need. `ast_licm_written`
  counts *any* `AST_Unary` over a Ref as a write (INC/DEC + the ADDR/
  MEMBER forms) — free conservative pre-kill for loops.
- **Fork/meet design (validated against the code, ready to build).**
  Snapshot = `{koff, ktt, kval, kn}` copy (~2 KB); If op 0: rewrite cond
  with the pre-branch lattice (sound: cond evaluates before either arm's
  stores), snapshot IN, walk then-arm, snapshot THEN-OUT, restore IN,
  walk else-arm, meet THEN-OUT (an absent else = meet with IN). Loops:
  pre-kill every entry `ast_licm_written` in the whole loop subtree, then
  walk each child BasicBlock with a fresh copy of the invariant lattice —
  sound per-iteration: invariants are never written, and body-local gens
  re-establish in program order every iteration — and continue after the
  loop with the invariants only (a `while`/`for` may run zero times).
  Bail to join-∅ (and skip descent) when an arm subtree contains any
  `AST_Jump`; unvisited BasicBlocks then get today's flat per-block scan
  via a visited bitmap, so coverage is strictly ≥ the current pass.
  Statement walk otherwise mirrors `ast_cprop_block` (Store/Return/
  reset-on-unknown), plus threading the lattice through nested plain
  BasicBlock statements.

**§32a LANDED (2026-07-10) — structured-join const-prop
(`MCC_AST_CPROP_JOIN`, default off).** Implemented exactly per the scoping
findings above: `ast_cprop_stmts` threads the lattice recursively from the
root block; If op 0 rewrites the cond with the pre-branch lattice, forks
(snapshot IN), walks both arms, and meets THEN-OUT against ELSE-OUT
(swap-delete-aware intersect); loop ops 2..4 pre-kill every
`ast_licm_written` entry then walk each child BasicBlock with a fresh copy
of the invariant lattice (per-iteration soundness argument in the findings)
and rewrite non-BB cond/incr expressions with invariants only; op 5 is
*not* treated as a loop (the ternary overload) and falls to the
conservative reset; any `AST_Jump` inside an If arm bails to join-∅ with
no descent, and a visited bitmap hands every undescended BasicBlock to
the old flat per-block scan, so coverage is strictly ≥ the flat pass.
Return-bearing arms conservatively meet as ∅ (fall-through tracking is a
listed refinement). Validated: full ctest 1862 green with the gate off
AND forced on corpus-wide (`MCC_AST_CPROP_JOIN=1 ctest`), 3-stage
self-host fixpoint byte-identical in both modes (the ON fixpoint = mcc
compiling itself three generations with the pass active), on/off outputs
agree on the new `ast-cprop-join` exec test (`tests/cpropjoin.sh`:
same-const joins, differing joins must-not-fold, loop invariants,
loop-written must-not-fold, nested if-in-loop, break/opaque path,
escaped locals, do-while, all × `-O1..-O3` × gate on/off against the
`-O0` reference). Remaining §32a refinements: fall-through-aware meets,
op-5 `for(;;)` classification via the `ast_cf_*` structure, switch (op 6)
arms, and §31 strategy registration.

**§32b LANDED (2026-07-10) — cross-join CSE/LICM
(`MCC_AST_CSE_JOIN`, default off).** The same structured walk applied to
the availability table (`ast_cse_stmts`): If op 0 arms inherit the
dominating table (the §32 mechanism-2 win) and the join keeps exactly the
entries that survived both arms (node-identity meet — arm-generated
entries never cross the join, so no φ); loops run `ast_licm_at_loop` with
the *incoming* table first (richer table → more hoists), then descend
child BasicBlocks with only the entries that are unwritten
(`ast_licm_written`) *and* operand-stable (`ast_licm_operands_ok`) in the
loop; op 5 keeps the flat `ast_licm_at_loop` + reset (the ternary
overload); any `AST_Jump` in an arm bails to reset with the flat-scan
fallback via the visited bitmap. Store/Return/Invoke statement handling
is byte-for-byte the flat pass's. Validated: ctest 1862 with both join
gates forced on corpus-wide, fixpoint byte-identical with both on and
both off, and the `ast-cprop-join` test extended to sweep the 2×2 gate
matrix at `-O1..-O3` (same-expr joins fold, operand-killed exprs must
not, loop-invariant exprs reuse across and inside the loop). Remaining
§32b refinements: availability across `Invoke` statements (entries are
provably call-clobber-free — the reset is legacy conservatism), and §31
strategy registration.

Sub-decisions settled: promotion guard = poison-list first, dominance
proof later; each pass env-gated per the existing pattern and registered
as a §31 strategy so the search permutes it. Gates (the FIX.md lesson —
pass-2 replay is never byte-verified and the last three regressions were
invisible to the default suite): full ctest + fixpoint + whole-corpus
mcctest + c-torture `--ast` columns + native arm64 spot-check for every
increment. Doc correction folded in: OPTIMIZE.md Frontier and the §27/§28
deferral texts should be re-pointed at this decomposition (§27/§28 are
unblocked-for-analysis now; only their value-materializing halves wait on
§32c). Builds on the landed pass suite + §22/§31 for search integration.

## 33. Function-call-window optimization — macro/micro slices (large, correctness-sensitive, INVESTIGATIVE)

**The gap.** Optimization today stops at the function boundary in two
distinct ways, and neither the landed §32a/§32b join dataflow nor Tier-4
virtual inlining crosses it. A "call window" is the straight-line region
that spans the caller ops feeding a call site and the callee's own body
ops. Two slices of that window are unoptimized:

- **Macro slice** — the coarse dataflow *across* the call/graft boundary:
  a value or available expression live before a call cannot be reused
  after it (un-inlined case) or inside the grafted body (inlined case).
- **Micro slice** — the few ops immediately adjacent to the seam: the
  caller's last pre-call ops and the callee's first body ops. The
  motivating case is "two ops before a call fuse with the first two ops
  inside the callee once it is inlined" — currently impossible.

**Why both fail today (traced, authoritative):**
1. `AST_Invoke` flushes both dataflow tables. `ast_cprop_stmts` falls to
   `else { ast_cprop_kn = 0; }` on a call (src/mccast.c ~4079); `ast_cse_stmts`
   substitutes into the args then `ast_cse_n = 0` (~4950). Availability
   never survives a call — even though `ast_cprop_escapes` already proves
   the tracked offsets are call-clobber-free (the §32b "legacy
   conservatism" note, TODO.md:1298, and §32a finding, :1233-1235).
2. The **merged post-graft window is never re-analyzed.** The two-pass
   faithfulness gate runs the AST transforms (`ast_cprop_run`/`ast_cse_run`
   et al.) once over the *caller's own tree in pass 1*, then grafts in
   pass-2 replay by walking the callee's **separate, independently
   optimized arena** (`ast_replay_bb(e->ast, …)`, src/mccast.c:1967).
   Caller facts and callee facts are computed in two disjoint arenas that
   never meet; the spliced result is emitted, not re-optimized.
3. Non-constant arguments are **spilled at the seam.** The graft
   materializes each non-const arg into a biased frame slot via
   `vstore` (src/mccast.c:1926-1937); the callee body then re-loads it.
   The caller's live value is forced through memory, so it cannot fuse
   with the callee's first op. Only *constant* args cross, via the
   `ast_argsub_val`/`ast_argsub_off` substitution (1910-1925, replayed at
   the `AST_Ref` case 2402-2409) and the resulting dead-branch prune
   (2825-2834). That const channel is the one working cross-window path
   and the template for the rest.

**Relationship to §32.** This is the cross-*call* extension §32
explicitly scoped out: §32 mechanism 2 carries availability across
If/loop joins *within one function* but keeps the Invoke/graft boundary
(TODO.md:1139, 1298); §32 mechanism 4 (φ-SSA for true value merges) stays
**rejected** and nothing here revives it — every item below is φ-free,
using memory (the biased slot / the graft return slot, the same "memory
is the merge" device as §32 mechanism 3) wherever two paths join. §33 is
sequenced *after* §32c because the fresh-temp carve is a shared
dependency of §33c.

**Decomposition (investigative sub-items):**

- **§33a — Cross-`Invoke` availability (macro, un-inlined calls).** Drop
  the `AST_Invoke` table reset in `ast_cprop_stmts`/`ast_cse_stmts`,
  keeping only entries proven call-clobber-free. Investigate: (i) the
  survivor set is exactly the non-escaping scalar locals `ast_cprop_escapes`
  already whitelists — a call cannot alias a local whose address was never
  taken; (ii) caller-saved registers are irrelevant because these entries
  are *memory offsets*, re-loaded on next use, not values pinned in regs;
  (iii) interaction with Tier-3 register promotion (`ast_plan_promotion`)
  — a promoted local held in a caller-saved reg across a call is a
  separate hazard already handled by `ast_no_callful_promo`
  (src/mccast.c:4931), so the memory-offset survivors are orthogonal.
  This is the smallest, already-named increment (the "remaining §32b
  refinement") and the correct first step. No temp infra, φ-free.

- **§33b — Unified post-graft window dataflow (macro, the core gap).**
  The merged caller+callee window must exist in *one* arena for any
  cross-boundary analysis to see it. Investigate two structurings:
  (A) **splice-then-reanalyze** — the graft copies the callee subtree
  into the caller arena (frame-biased, as the replay already biases live)
  so a single §32a/§32b join pass over the merged tree treats the callee
  body as ordinary dominated BasicBlocks; the seam becomes a plain
  store-then-block, and §33a's cross-Invoke rule is then unnecessary for
  inlined calls because there is no residual Invoke; (B) **two-pass
  hand-off** — keep the separate arenas but thread the caller's exit
  lattice/availability into `ast_inline_graft` as the callee replay's
  entry facts. (A) is more powerful (enables §33c/§33d natively) but
  fights the faithfulness-gate architecture: the transforms currently run
  in pass 1 *before* graft, and pass-2 replay is never byte-verified
  (the FIX.md lesson, TODO.md:1304-1306) — so a post-graft pass needs its
  own soundness gate. Deliverable of this item is the decision between
  (A) and (B) plus the arena/gate design, not code.

- **§33c — Argument de-spill / caller-value forwarding (micro, the
  motivating case).** Forward a caller's live single-use value directly
  into the callee's first param use instead of round-tripping through the
  biased slot — the non-const generalization of the const `ast_argsub`
  channel. Investigate: (i) legality predicate = the param is **read
  once, before any store to it, and its forwarded value's operands are
  unclobbered up to that read** (a read-once/def-dominance analysis over
  the callee subtree, the same shape as `ast_licm_operands_ok`); (ii)
  when legal, register the caller SValue in an extended `ast_argsub_val`
  keyed by param offset and *substitute the live value* at the callee's
  `AST_Ref`, eliding the `vstore`/reload pair — this is exactly what makes
  "two caller ops fuse with the callee's first two ops" happen; (iii)
  multi-read or post-store params fall back to today's spill (strictly no
  regression). Depends on §33b(A) or the seam being visible; depends on
  §32c only if a materialized temp is needed for the multi-read case.

- **§33d — Seam peephole window (micro).** Scope the long-open McKeeman
  peephole item (OPTIMIZE.md Frontier: "peephole window over emitted code
  … adjacent redundant load/store elision in the vstack emitter") to the
  graft seam specifically: a store-to-slot immediately followed by a
  load-from-the-same-slot that straddle the inline boundary is the
  residue §33c cannot forward (multi-read params, address-exposed slots).
  Investigate the stated blocker — "emitter byte-identity risk" — i.e.
  whether a bounded 2-3 op window elision can be proven to preserve the
  pass-1 faithfulness contract, or whether it must run only in the
  unverified pass-2 replay under a differential exec gate. This is the
  narrowest, most correctness-sensitive slice; sequence it last.

- **§33e — Window-level scoring & cache key (governance).** §18/§21 cache
  keys are per-function intention hashes (`ast_intention_hash`), and a
  grafted window has no key of its own — the caller's key already covers
  the graft because the callee tree is folded in before hashing? Verify:
  `ast_intention_hash` runs in `ast_func_end` pre-pass over the caller
  arena *before* graft, so it does **not** currently include the callee
  body — a window transform would need either a window-level key or an
  accepted cache miss on the first graft. Investigate §31 strategy
  registration for the window passes and §25 scoring of the size/JIT delta
  a de-spill produces (typically a net op reduction — cheap to score).

**Per-slice minimal mechanism (proposed):**

| Slice | Item | Mechanism | φ? |
|---|---|---|---|
| Macro, un-inlined | §33a | drop Invoke reset for escape-proven entries | no |
| Macro, inlined | §33b | splice callee into caller arena, reanalyze | no (memory-merge) |
| Micro, seam value | §33c | forward live single-use arg, elide spill | no |
| Micro, seam residue | §33d | bounded load/store elision at the seam | no |
| Governance | §33e | window key + §31/§25 integration | n/a |

**Build order (proposed, cheapest-first):** §33a (no infra, already
named) → §33b **decision** (A vs B; the pivot everything else hangs on) →
§33c (the motivating win, needs §33b's seam + optionally §32c) → §33d
(narrowest, highest risk) → §33e folded in alongside. Open question to
resolve first in §33b: whether splice-then-reanalyze can be made
faithfulness-safe, since it is the difference between a genuinely
re-optimized merged window and a hand-off approximation.

**Gates (same standard as §32b — pass-2 replay is never byte-verified,
so the default suite is blind to graft regressions):** full ctest +
3-stage self-host fixpoint byte-identical (gate off AND forced on) +
whole-corpus mcctest + c-torture `--ast` columns + native arm64
spot-check, per increment. Every item env-gated on its own flag
(`MCC_AST_CALL_WINDOW` family) defaulting off, per the established
pattern. Builds on §32a/§32b (join dataflow), §32c (fresh temp, for
§33c's multi-read fallback), the Tier-4 graft path (src/mccast.c:1857-1997),
and §18/§21/§25/§31 for search + scoring integration.

# Doc-audit gap backlog (added 2026-07-10)

Items surfaced by a full `*.md` audit (OPTIMIZE.md / FIX.md / STATUS.md /
MCC.md / EXCESS.md) that were **tracked nowhere** in the numbered ladder or
the arm64 queue. NOTE: the conformance / ABI / CST / promotion-quality
backlog (A1 spill-slot sharing, `p->m`/XMM8-15 promote, arm64/riscv64
promotion, Tier-4 default policy, conformance §6-A/§6-B, CST symref-shadow /
`H_e` / slice-G, i386 TLS, skipped-test ungating audit, CMake normalization)
already lives in **MCC.md §10 + EXCESS.md** — that is the intended home for
it and it is deliberately **not** duplicated here. Only genuinely-untracked
items are added below.

## 34. BUG: arm64 `load()` r-flag mask rejects §30's bit-flag SValue (correctness, latent)

- [x] On arm64, setting `MCC_AST_INLINE_NODES` (any value) trips
      `arm64-gen.c:650 load(1,(32,400,0))` (FIX.md:234-237): §30's bit-flag
      transform (`ast_bf_build`) produces an SValue whose r-field carries a
      flag not in the arm64 `load()` accepted mask — the "arm64 load()
      r-flag mask" class. Masked today only because CI never sets that env,
      so the bench/search never exercises it — but it **blocks the
      `MCC_AST_INLINE_NODES` search dimension (§23) on arm64 entirely** and
      will resurface the moment §31 registers bitflag×inline on that target.

**LANDED (2026-07-10).** Root cause (traced): the stray r-flag is
`VT_MUSTCAST` (0x400/0x0C00, "char/short in an int register needs a cast").
arm64 `load()` (`src/arch/arm64/arm64-gen.c:494`) stripped
`VT_BOUNDED|VT_NONCONST|VT_NONLVAL` from its `svr` location classifier but
**not** `VT_MUSTCAST`, then used exact-equality case tests (`svr == …`), so a
value with a pending cast matched no case and hit `assert(0)` — where
x86_64's `load()` never does, because it bit-tests (`fr & VT_VALMASK`,
`fr & VT_LVAL`) and inherently ignores `VT_MUSTCAST`. Fix mirrors x86_64:
add `VT_MUSTCAST` to the strip mask, routing a register-source-with-pending-
cast to the reg-to-reg move case (`svr < VT_CONST`, :602); the cast is
applied by the caller (`gv`/`gen_cast`) exactly as on x86_64. **Provably
regression-free:** any SValue that already matched a case had no
`VT_MUSTCAST` set (or it would already assert), so masking it is a no-op for
every working path — the change can only convert an assert into correct
routing. Gates: full `cmake-cross` ctest **1862/1862** (27/27 arm64 cells),
`fixpoint-invariant` byte-identical (stage2==3==4). The exact self-host
assert needs the CI arm64-sysroot self-compile to reproduce (layout-specific,
FIX.md:225-229) — recommend the native arm64 spot-check
(`MCC_AST_BITFLAG=1 MCC_AST_INLINE_NODES=8`, bitflag corpus) confirm it there.

## 35. Sethi–Ullman evaluation-order numbering (large, codegen-order, byte-identity risk)

- [x] OPTIMIZE.md Tier-2 catalog (:81-82) + Frontier (:651): evaluation-order
      / register-pressure numbering in the replay emitter — reorder
      commutative operand emission to cut register pressure. Untracked by
      §17-§33. Codegen-order change ⇒ the byte-identity gate is the hard part
      (any reorder perturbs `-O0..-O3` bytes unless gated behind a default-off
      pass like the rest of the ladder). Same landing shape as the other AST
      passes: a default-off `MCC_AST_*` gate, exec-golden + fixpoint (off AND
      forced-on) + native arm64 spot-check. Feeds §31 as a strategy.

**LANDED (2026-07-10, first increment) — `MCC_AST_SETHI` (default off).**
`ast_sethi_run` (src/mccast.c) walks the captured tree; for a commutative
binary node (`+ * & | ^`) whose two operands are both `ast_cse_regpure`
(side-effect-free), it puts the higher Sethi–Ullman-numbered operand first
(emitted child-order = evaluation order in the replay), so the operand that
needs more registers is evaluated first. Commuting a side-effect-free pair
is value-preserving for **every** operand type (IEEE `+`/`*` commute
bit-exactly — commutativity, not associativity; `&|^` are integer-only), so
no dataflow proof and no result-type restriction are needed. **One hazard
found and guarded (the FIX.md/§30 `VT_CMP` lesson):** an operand whose root
is a comparison/logical op (`ast_sethi_cmp_root`) leaves a `VT_CMP` the
parent must consume immediately — reordering it clobbers the flags (this is
exactly why `ast_bf_build` fixes its `bit & guard` order); SETHI skips the
swap when either operand is comparison/logical-rooted. Wired into the replay
driver mirroring the `bitflag` gate (`sethis`/`do_sethi`). Effect confirmed
real: `.text` 81→73 B on nested arithmetic; fires 17× compiling the mcc
self-source at `-O1`. Gates: `cmake-cross` ctest **1862/1862** default (off
path inert) AND forced-on (`MCC_AST_SETHI=1 ctest`), `fixpoint-invariant`
byte-identical (stage2==3==4) both off and forced-on, `bitflag_transform`
green under SETHI (the guard), output matches gcc/`-O0`. Remaining: register
it as a §31 search strategy; the current cut only reorders top-level
commutative pairs where the right operand is strictly deeper — an n-ary
reassociation-aware ordering is a later extension (reassociation itself is
not commutative-safe, so stays out). Native arm64 spot-check recommended
(codegen-order change; x86_64 fixpoint is the primary gate).

## 36. Chaitin–Briggs graph-coloring register allocation (largest, codegen — run LAST)

- [~] OPTIMIZE.md Tier-3 catalog (:90-92) + Frontier (:651), self-described as
      "largest item, last." Replace the pinned-register promotion heuristic
      (`ast_plan_promotion`, `ast_promo_*`, src/mccast.c) with a real
      interference-graph coloring allocator. Untracked by §17-§33 — the single
      largest algorithm OPTIMIZE.md names with no ladder home. Prerequisite it
      subsumes: the A1 backward-liveness spill-slot-sharing item already in
      MCC.md §10/EXCESS (coloring gives interval sharing for free). Enormous,
      correctness-critical (the promotion machinery is the source of the FIX.md
      reemit/promote regressions); lands default-off, fixpoint-gated (off AND
      forced-on) + per-target byte-identical objects + native arm64/riscv64.

  **FIRST INCREMENT LANDED (2026-07-10): the coloring allocator itself,
  default-off.** `ast_color_graph(n, adj, cost, k, color)` (src/mccast.c, in the
  arch-neutral public region so `asttool` can unit-test it) is a full
  **Chaitin–Briggs** allocator: build degrees → *simplify* (push nodes with
  degree < k) → *potential-spill* (push the lowest-`cost` node when none is
  trivially colorable) → *optimistic select* (pop, assign the lowest color free
  among colored neighbors, leave `-1` if none). Wired into `ast_plan_promotion`
  behind **`MCC_AST_COLOR` (default off)**: it builds a per-register-class
  interference graph over the promotion candidates and colors it to choose which
  are promoted, replacing the greedy top-k-by-weight loop. Current interference
  model = **complete graph per class** (every promoted local is pinned for the
  whole function, so all same-class candidates conflict), so this increment
  lands the *algorithm + integration + tests*; live-range interference (the
  register-*sharing* win, which needs the `ast_promo_entry_init` emission to
  load at range boundaries) is the **next increment**.
  Validation: unit test `ast/color` (K4 needs 4 colors; K4/k=3 spills the
  lowest-cost node; independent set → 1 color; path → 2 colors; proper-coloring
  invariant checked). Default (gate off): `-O0` 3-stage fixpoint stage2==3==4
  byte-identical, full ctest **1880/1880**, release/cross/multisource/sanitize
  build clean. Forced-on (`MCC_AST_COLOR=1 -O2`): objects deterministic and
  **correct across the whole exec corpus (229/229 programs match gate-off
  behavior, 0 divergences)**. Remaining for §36: live-range interference +
  range-based emission (register sharing), then folding in spill-slot sharing
  and finally *replacing* (not just informing) the promotion assignment, each
  fixpoint-gated + native arm64/riscv64.

  **SECOND INCREMENT LANDED (2026-07-10): live-range interference + register
  sharing (sound, straight-line-restricted).** The coloring now *shares* a
  register between promotion candidates whose live ranges are disjoint. Because
  sound sharing under arbitrary control flow needs real CFG liveness (naive
  node-index intervals are unsound across loop back-edges), this increment
  restricts sharing to **straight-line functions** (no `AST_If`/`AST_Jump`),
  where node-index order == execution order and interval overlap is exact.
  Mechanics: (1) per candidate, compute `[firstref, lastref]` over the AST
  linearization + whether the first reference is a *definition* (an `AST_Store`
  target); a candidate whose first ref is a *read* is live-in (parameter) so its
  interval is pinned to start at entry (pos 0). (2) Interference = interval
  *overlap*; two candidates share a color only if one's refs entirely precede
  the other's. (3) Emission: `ast_promo_entry_init` entry-loads only the first
  candidate per physical register (the earliest-range one — a live-in param if
  present); later sharers get their value from their own defining write, so no
  reload machinery is needed. Branchy functions fall back to the increment-1
  complete-graph (no sharing). Fixed en route: `ast_color_graph` now always
  initializes `color[]` before the `k<=0` early return (the XMM class is `k=0`
  when the function has a call), and `ast_promo_{off,typ,reg}` were resized to
  `AST_PROMO_MAX*8` since sharing promotes more candidates than physical
  registers. Validation: gate-off `-O0` fixpoint stage2==3==4 byte-identical +
  full ctest **1880/1880** + release/cross/multisource/sanitize clean;
  forced-on (`MCC_AST_COLOR=1 -O2`) **229/229 exec corpus correct, 0
  divergences, 0 compiler crashes**, and register sharing genuinely fires
  **105× across the corpus**. Remaining for §36: CFG-liveness so sharing works
  through branches/loops (lifts the straight-line restriction), spill-slot
  sharing, and finally *replacing* the promotion heuristic outright — each
  fixpoint-gated + native arm64/riscv64.

## 37. Bench-statistics roadmap — Bayesian + ANOVA inference (medium, tools-only)

`tools/bench.c` ships Welch's t-test + Cohen's d + percentile-bootstrap 95%
CI. OPTIMIZE.md's statistical taxonomy lists three **planned** methods with
no tracker — tools-only, cannot touch the compiler / fixpoint:

- [x] **Credible interval** (OPTIMIZE.md:150-155,162): Bayesian counterpart to
      the bootstrap CI over the per-cell wall-time samples.
- [x] **Bayes factor** (OPTIMIZE.md:123-128,150-155): the Bayesian
      significance verdict alongside the Welch `*`/`ns` column, for
      "faster vs reference" decisions the t-test only frequentist-tests.
- [x] **ANOVA** (OPTIMIZE.md:160): one-way ANOVA to compare >2 `-O` configs
      at once instead of the current pairwise-vs-reference Welch tests.
- [x] Gate: `tools/bench.c` unit/self-check only (no compiler surface, no
      fixpoint/ctest-codegen impact); additive columns, existing output
      unchanged unless a new `--stats=bayes|anova` flag is passed.

**LANDED (2026-07-10).** `tools/bench.c` gains three stat functions behind
a new `--stats` flag (default off, existing report byte-unchanged without
it): `cred_interval` (95% t-based credible interval under a Jeffreys prior,
reusing `t_crit_05`), `bayes_factor` (BF10 via the Wagenmakers 2007 BIC
approximation from a pooled two-sample t, labeled strong/subst./weak H0/H1),
and `anova_oneway` (one-way F across the mcc `-O` configs, exact upper-tail
p-value via a Numerical-Recipes regularized incomplete beta
`bt_betai`/`bt_betacf`/`bt_gammln`). `write_stats` prints a per-workload
block: per-cell CrI, per-`-O`-group Bayes factor vs same-config reference,
and the cross-config ANOVA. Verified: math unit-checked against known values
(`betai(0.5,0.5,0.5)=0.5`, `betai(3,1,0.5)=0.125`, ANOVA `F(2,6)=3.0
p=0.125`, CrI/BF sign behavior); compiles clean (0 warnings from the new
code); end-to-end run confirms `--stats` adds only the block and the default
output is structurally identical without it. Compiler/fixpoint untouched
(tool-only; `tools/bench.c` is not in the default build or ctest codegen
path). Naming note: the TODO proposed `--stats=bayes|anova`; shipped as a
single `--stats` toggle that emits all three (simpler; all are cheap).

## 38. Live CI-validation open items (blocked on non-x86_64-Linux hardware)

Open validations from FIX.md that need a runner this repo's local x86_64
Linux box does not have — parked here so they are tracked, not lost:

- [x] **macos-x86_64 `[-O3]` rows n/a** — RESOLVED (2026-07-10, via Rosetta 2
      on this arm64 box; it WAS the arch-independent reemit/promote bug, now
      fixed by `ast_fn_faithful`). The x86_64-macOS mcc (built from HEAD) run
      under Rosetta: `-O0..-O3` compile+execute correctly, and the `-O3`
      3-stage self-host reaches a byte-identical fixpoint (`75bc72196818` ×3) —
      the exact thing that was "n/a." No Darwin-x86 CI runner needed after all;
      Rosetta is the runner.
- [~] **CI re-checks pending after three landed FIX.md fixes**: the
      `vcheck_cmp`-before-`gfunc_call` guard and the `ast_fn_faithful` reemit
      gate are now re-verified on **macos-x86_64 (via Rosetta)** and
      **macos-arm64 (native)** here — `-O3` self-host + exec green on both. The
      promote frame-slot change is x86_64-only (compiled out on arm64) and its
      macos-x86_64 codegen is exercised by the Rosetta x64 build. Remaining:
      **msvc-arm64** (needs a Windows runner) and the ELF `-O2/-O3` linux CI
      cells (linux runner) — genuinely off-box.
- [ ] **static-glibc self-host gap** (README.md:12 / MCC.md:25): static-glibc
      via mcc hits a `__pthread_initialize_minimal` gap (musl is the
      fully-static path). Decide finish-or-document-as-permanent.

## 39. Doc-staleness reconciliation (docs-only, no code)

Known-stale prose that later work already contradicts — TODO §32 itself
(:1427-1429) asks for the first of these:

- [x] OPTIMIZE.md **Frontier** (:636-660) + Scoreboard **"Open / blocked"**
      (:34-40): both predate the iter-25/27 CSE/LICM landings and TODO §32's
      "no new node kind" resolution; re-point them at the §32 decomposition.
      (Done 2026-07-10: dated SUPERSEDED note on Frontier + staleness
      correction on the Scoreboard para; historical prose retained.)
- [x] MCC.md §10 (:242-243): the `-O1` soundness backlog + wiring rows lag
      EXCESS.md (§19 CLEARED, §20 UNPARKED 2026-07-09) — reconcile to EXCESS.
      (Done: both rows reconciled to CLEARED/UNPARKED, EXCESS cross-refs added.)
- [x] STATUS.md §25 row: still says "JIT cpu tier pending" though N4 landed
      `MCC_AST_JITSCORE` (2026-07-10) — update to match. (Done: row now
      **functional**, cpu+RSS tier noted landed.)
- [x] OPTIMIZE.md Tier-1 note (:51-55): the sin/cos/exp "stays open until a
      determinism story exists" text is stale — landed as `-ffold-math`
      (iterations 31/33/35-38); mark it done. (Done: clause updated to
      LANDED behind `-ffold-math`.)

# 40. TDD execution plan & gap closure (added 2026-07-10)

Consolidated from a full re-read of TODO.md + STATUS.md + OPTIMIZE.md + MCC.md
+ FIX.md + EXCESS.md + README.md + CONFIG.md + BUILD.md. Every open item is
bucketed by whether it can be *implemented and validated in this environment*
(native arm64-darwin, Apple M1 Pro; no x86_64-Linux/msvc/riscv64/Darwin-x86
runner). The standing landing gate for any compiler change is unchanged:
default-off `MCC_AST_*` env, full ctest green, 3-stage self-host fixpoint
byte-identical (gate off AND forced-on), native arm64 spot-check, per
increment.

## 40.1 Untracked gaps now registered as items

- [x] **§6-A conformance gaps 1-4 were ALREADY FIXED — MCC.md prose is
      stale** (verified 2026-07-10 by re-checking code + regression tests, not
      just the doc audit): gap 1 (bare-`inline` external def) fixed by
      `3ca81969` — mcc now link-errors a bare-inline-only call exactly like
      cc/clang, tested by `cli/c99_inline_no_extern_def` +
      `c99_inline_emission_matrix`; gap 2 (K&R implicit-int) fixed by
      `c1777f26`, tested by `cli/knr_implicit_int_param`/`implicit_int_diag`/
      `wimplicit_int_flag`; gap 3 (`va_start` last-arg) tested green by
      `cli/va_start_last_param_clean`; gap 4 (`<threads.h>` shadow) resolved by
      the `include_next` shim (`a90afe80`), tested by `exec/c11_threads`. My
      earlier §40.1 registration was a doc-audit false-positive. ACTION: flip
      MCC.md:148-151 prose to "landed"; no code.
- [x] **NEW gap — gnu89 plain-`inline` diverges from clang → FIXED** (found
      while verifying gap 1; untracked): under `-fgnu89-inline`, `inline int
      f(){...}` + a call link-errored in mcc but LINKS in clang/gcc (gnu89
      plain inline provides the out-of-line definition). RESOLVED as FIX: the
      decl-time rewrite at `mccgen.c` now strips `VT_INLINE` from a bare inline
      function def when `gnu89_inline` is set (gate: `(type.t & (VT_INLINE |
      VT_STATIC | VT_EXTERN)) == VT_INLINE`), so it emits the external def and
      links, matching gcc AND clang (verified via truth-table). Fully gated on
      `gnu89_inline` (default C99 path byte-identical — proven by object sha on
      a mixed static/extern/plain-inline sample: `7519bff1…` old==new — and by
      `fixpoint-invariant` staying green). `gnu89_inline` is off by default and
      unused in self-host, and mcc's own source has no bare `inline` funcs, so
      self-host is untouched. Tests: `cli/gnu89_plain_inline_emits_def` (nm →
      `T f`), `cli/gnu89_plain_inline_links_and_runs` (rc=42), gate
      `cli/c99_plain_inline_default_link_error` (default still link-errors).
      The converse gnu89 `extern inline` behavior (mcc emits a static copy and
      links; clang link-errors at `-O0`) is left AS-IS and now recorded as an
      intentional `[DIFF]` in MCC.md §6-A, pinned by
      `cli/gnu89_extern_inline_static_copy_diff`. Full ctest 1861/1865 (only
      the 4 known `bound_global` artifacts fail); fixpoint-invariant green.
- [x] **STATUS.md rung table is stale past §32** (Agent-A gap 1/2): it stopped
      at §32 and framed §32c as "fully scoped," omitting §33/§35/§36/§37/§38/
      §39. Done: rung table extended (§32c, §25) + scope note added. NOTE the
      "-O3 determinism blocker" I first added here is RETRACTED — `-O3`
      self-host is deterministic and N2 box-3 is DONE.

## 40.2 Bucket A — implementable + validatable now (TDD, this box)

Build order, cheapest-first; each lands green before the next starts (all
touch `src/mccast.c`, so serialize to avoid worktree merge conflicts):

1. [x] **§33a — cross-`Invoke` availability** (`MCC_AST_CALL_WINDOW`,
      default off) — LANDED. In `ast_cprop_stmts`/`ast_cse_stmts` (join paths),
      an entry now survives an `AST_Invoke` iff its local is non-escaping
      (`ast_cprop_escapes`) AND un-written by the call's args
      (`ast_licm_written`; CSE also requires `ast_licm_operands_ok`); else the
      table still fully resets (byte-identical when off). Validated: gate-off
      `-O0..-O3` hash-identical + full ctest 1860/1860 green; gate-on it fires
      (0→1 cprop fold, 0→1 CSE fold) with output byte-matching the -O0
      reference; address-taken / arg-mutated locals correctly NOT folded.
      Requires the pass's own `MCC_AST_CPROP_JOIN`/`CSE_JOIN` gate too (only
      the `*_stmts` join functions are wired; `*_block` is a possible
      follow-up).
2. [~] **§30 additional encodings** (extend `ast_bf_*`, same
      `MCC_AST_BITFLAG` search dim). **Biased ranges — LANDED**: a cluster of
      same-key `==` constants fitting any 64-wide window `[base, base+63]`
      (base picked as the signed-64 min) now folds by biasing `k'=(unsigned)
      key-base` before the shift-mask; `base==0` is byte-identical to the old
      encoding (proven by object sha), span>63 / unsigned-wraparound windows
      conservatively fall back. Validated: gate-off byte-identity + full ctest
      1860/1860; fires on a `{100,105,120,150,163}` cluster with the edge-key
      checksum matching the transform-off reference at -O1/-O2/-O3.
      **`&&`-of-`!=` complement — LANDED**: a same-key `key != C1 && … &&
      key != Cn` chain (n-ary `TOK_LAND` of `TOK_NE`) now folds to `member ^ 1`
      (the negation of the membership test), reusing `ast_bf_window`/
      `keyexpr`/`build` via a parametrized `ast_bf_cmpconst`/`ast_bf_cond_
      parse_op` + new `ast_bf_try_land`. `member ∈ {0,1}` so `^1` is exact
      logical negation; out-of-window/negative keys give `guard=0 → member=0 →
      result=1`, correct (not in the set → all `!=` true). Validated: existing
      `==`/`||` fold byte-unchanged (object sha), gate-off `-O0..-O3`
      byte-identical, full ctest green, fires on a `k!=100&&…&&k!=163` cluster
      with the edge-key checksum matching the transform-off reference.
      **Value-table dispatch — INVESTIGATED, reclassified out of `ast_bf`**
      (2026-07-10): a true rodata lookup table is **blocked** — the AST layer
      (`src/mccast.h:7-25`) has no array/global/static-data node kind and no
      pass ever emits initialized data (`ast_fconst_push_ref` only *replays*
      backend constant-pool indices captured by the front end; `ast_bf_build`
      only builds `Literal`/`Binary`/`Convert` arithmetic), and synthesizing a
      table Sym+initializer that survives the two-pass byte-identity
      reset-and-re-emit is invasive new infrastructure. The arithmetic-only
      *packed-immediate* fallback is expressible but requires collapsing a
      whole if-chain into a synthesized `Store` with default-encoding — a new
      statement-level mutation class (every existing bitflag transform only
      rewrites expression→expression) for a narrow value/range slice; declined
      as not-clean on a self-hosting compiler. If wanted, it is its own
      **data-emission-infrastructure project** (a table-symbol+initializer
      emitter wired into the replay/rewrite lifecycle), not an `ast_bf` fold —
      moved to Bucket B. **If-chain fall-through `!=` form — LANDED**: a nested
      `if(k!=C1){if(k!=C2){…body}}` chain (`AST_If` op-0, no-else, single-child
      wrapper blocks) captures as the statement dual of the `&&` form and now
      folds via new `ast_bf_try_ifne` (registered after `ast_bf_try_if`,
      reusing `ast_bf_cond_parse_op(TOK_LAND,TOK_NE)`/`window`/`build` → `member
      ^ 1`), with conservative no-else/`nchild==1`/label guards; it poisons the
      LAND node so there's no double-fold with `ast_bf_try_land`. Validated:
      gate-off byte-identical, existing encodings byte-unchanged, fires + edge-
      key checksum matches the transform-off reference (+ permanent
      `cli/bitflag_ifne` test). **§30 encodings are now COMPLETE** for the
      arithmetic model (==/||, biased-range, `&&`-of-`!=`, If-chain `!=`); only
      the value-table needs the separate Bucket-B data-emission project.
3. [~] **§32a refinements** — **op-5 `for(;;)` classification LANDED**: the
      `MCC_AST_CPROP_JOIN` structured descent now treats op-5 infinite loops
      with the same invariant-preserving meet as op-2..4 loops (`ast_cprop_
      stmts`, `<=4`→`<=5`) — a `for(;;)` exits only via `break`, so entry
      facts not written anywhere in the loop are valid at every body point and
      break target. Validated: gate-off `-O0..-O3` byte-identical + full ctest
      1860/1860; gate-on fires (0→1 cprop fold on a `for(;;){…break;}`
      invariant) with output matching the -O0 reference; cpropjoin.sh OK.
      **switch(op 6) invariant classification LANDED**: op-6 switch has the
      same [controlling-expr(Ref/Literal), body-BB] child shape as loops, so
      the same `<=5`→`<=6` extension gives it the invariant-preserving descent
      — a local written nowhere in the switch survives to every case path /
      break target (fall-through is irrelevant since only whole-switch
      invariants are kept). Validated: gate-off byte-identical (180 objects, 0
      diffs) + full ctest 1860/1860 + fixpoint + cpropjoin.sh OK; fires (0→1
      fold on a switch-invariant const), gate-on output matches the -O0 ref,
      and a switch that writes the local in a case is correctly NOT folded.
      **switch per-arm VALUE meet — LANDED** (sound subset): `ast_cprop_switch_
      meet` meets the per-arm exit lattices (generalizing the op-0 two-arm meet
      to N arms) so a value set identically in every arm survives — but ONLY
      when the switch is provably safe: has a `default` (total), zero
      fall-through between arms, no labels, no mid-arm break/continue/return,
      no nested loop/switch in an arm (empty/trailing segments fold in the
      entry state); any violation falls back to the keep-invariant descent.
      Validated: gate-off byte-identical, gate-ON full ctest zero new failures
      (incl. self-host exec-replay-promote), cpropjoin.sh OK, 10 positive
      shapes fire + 6 negatives (fall-through/no-default/diff-values/mid-return/
      mid-break/loop-in-arm) correctly NOT folded + soundness edges safe,
      adversarial matches clang. **§32a is now essentially complete** for the
      structured-join lattice (op-0 meet, op-2..5 loop invariants, op-6 switch
      invariants + sound value meet); the only residue is cross-loop-iteration
      value merging (needs widening/fixpoint dataflow — Bucket-B).
4. [x] **§35 — Sethi–Ullman evaluation-order numbering** — LANDED as
      `MCC_AST_SETHI` (commit `f45a6ec5`, first increment): reorders
      commutative operand emission behind a default-off gate; byte-identity
      preserved when off. (Follow-ups: wider operand shapes, feed §31 as a
      strategy.)

Remaining Bucket-A (open, serialized on `src/mccast.c`): **§32a refinements**
(fall-through-aware meets, op-5 `for(;;)` classification, switch(op 6) arms in
the `MCC_AST_CPROP_JOIN` lattice) and **§32c value-materializing fresh-temp
LICM** (now unblocked — the `-O3` self-host fixpoint gate is available). Both
follow the same default-off + fixpoint-off/on + native-arm64 + full-ctest gate
proven by §33a/§30; the `&&`-of-`!=` and value-table §30 encodings also remain.

## 40.3 Bucket B — milestone-scale (plan only; do NOT attempt piecemeal)

Each is multi-day, correctness-critical, and would jeopardize the green/
fixpoint invariant if rushed. Sequenced, not started:
- ~~**-O3 self-host determinism** (blocks N2 box-3)~~ — RETRACTED: `-O3`
  self-host IS deterministic and reaches a byte-identical fixpoint (N2 box-3
  now DONE); the earlier "non-determinism" was a harness bug (embedded output
  basename). §32c's fixpoint gate is therefore available; §32c's remaining
  work is the value-materializing pass itself, not a determinism prerequisite.
- **§32c value-materializing fresh-temp LICM** — **FIRST INCREMENT LANDED**
  (arm64-first, default-off `MCC_AST_LICM_TEMP`): hoists one loop-invariant
  pure integer `Binary` (operands invariant via `ast_licm_operands_ok`, pure
  via `ast_cse_regpure` so no div/mod trap, used ≥2× in the loop, not already
  in a local) into a fresh strictly-negative carved temp — obstacle A (carve
  reserved after `loc=saved_loc`, before `ast_promo_entry_init`), C
  (`AST_Convert`-wrapped store, comparisons excluded), D (preheader store in
  the loop's parent BB dominates all uses) all handled. Validated native
  arm64: gate-off `-O0..-O3` byte-identical + full ctest 1862 + gate-ON
  corpus-wide ctest green + gate-ON `-O2` 3-stage self-host fixpoint converges
  + 23 adversarial loop shapes match `-O0`/clang. **x86_64 obstacle-B
  (promotion poison) validation — DONE (2026-07-10, via Rosetta)**: the
  x86_64-macOS mcc (built from HEAD, run under Rosetta 2 on this arm64 box)
  exercises promotion (x86_64-only) alongside the fresh temp. With
  `MCC_AST_LICM_TEMP=1` at `-O1/-O2/-O3` (promotion ACTIVE) a promotion-eligible
  + fresh-temp TU (nested loops, multiple invariants) matches the `-O0`
  reference AND system clang exactly (`832675297231051594`), and the gate-ON
  `-O2` 3-stage x86_64 self-host fixpoint converges — so the poison guard works
  (promotion never seeds a register from the uninitialized temp slot). §32c's
  first increment is now validated on BOTH arm64 and x86_64; the gate stays
  default-off. **Remaining**: post-join PRE → IV strength reduction (the further
  §32c consumers).
- **§36 Chaitin–Briggs graph-coloring regalloc** — largest, replaces
  `ast_plan_promotion`; run last.
- **§26 `--embed-jit` runtime engine**, **§33b/c/d/e**, **§27/§28**,
  **§22 true per-function re-emit** (blocked on the AST-clone/destructive-
  mutation issue).

## 40.4 Bucket C — blocked on hardware not present here

Cannot be validated on arm64-darwin; defer to CI / a matching runner:
- **§38** macos-x86_64 `-O3` retest, msvc/arm64 CI re-checks, static-glibc
  self-host gap (needs Darwin-x86 / msvc / glibc runners).
- **N2 box-4 riscv64**, **EXCESS** i386 TLS `R_386_TLS_GD/LDM`, arm64/riscv64
  Tier-3 register-model extension + qemu validation, i386-fastcall-abi.

## 40.5 Method

Divide-and-conquer via subagents in isolated git worktrees, one per Bucket-A
item, serialized on `src/mccast.c`. Each agent must return its diff, the full
ctest summary, gate-off byte-identity evidence, and gate-on fire+correctness
evidence; the main loop verifies green (ctest + fixpoint off/on + arm64
spot-check) before committing to `main`. Docs-only items (40.1 STATUS refresh,
Bucket-C tracking) land directly. Bucket-B stays documented, not coded.

# 41. Ungate all AST/optimization paths by default (default-ON sweep, correctness-critical)

Requested 2026-07-10. Today every AST/optimizer pass is opt-in behind a
default-off `MCC_AST_*` gate (or `-O4+` search) so the default `-O0..-O3`
output stays byte-identical to the reference and the fixpoint holds. This item
flips that: make the AST replay + optimization code paths **ON by default**
and prove nothing regresses.

BLAST-RADIUS EVIDENCE (2026-07-10, native arm64): this is a **re-baseline
program, not a flip.** Forcing a single gate on changes `-O2` object bytes for
any TU where the pass fires — measured on 3 sample TUs: `CPROP_JOIN`, `BITFLAG`,
`SETHI` each change bytes where they fire (e.g. n1_join, n3_window), `CSE_JOIN`/
`CALL_WINDOW` unchanged only because they didn't fire on those samples. So
default-ON necessarily perturbs the byte-identity goldens (`dash-s-bytes`,
`--ast` columns, per-target objects) and the 3-stage fixpoint's own bytes — all
of which must be intentionally re-baselined per target, pass by pass, not
"fixed." The fixpoint still has to *converge* (stage2==3==4) under the new
defaults; it just converges to different bytes than today. That re-baseline +
per-target/per-runner reconvergence is the actual work here.

- [x] **Enumerate every gate.** Done 2026-07-10 from `ast_configure`
      (`src/mccast.c:651-689`). `ast_env_gate("NAME", D)` = env value if set,
      else default `D`. **Correction to this item's premise: the core passes
      are already default-ON at their `-O` level — §41 is really about the six
      search-only passes.** Full map (default when `MCC_AST_*` unset):

      | pass (env) | default | ON at |
      |---|---|---|
      | `ast_replay_env` (base replay/reemit) | `optimize>=1` (no env) | **-O1+** |
      | `MCC_AST_TEMPLATES` | `optimize>=1` | **-O1+** |
      | `MCC_AST_PROMOTE` | `x86_64 && optimize>=2` | **-O2+** (x86_64) |
      | `MCC_AST_INLINE` | `optimize>=3 && !size` | **-O3** (non-`-Os`) |
      | `MCC_AST_SETHI` | `0` | off (search/-O4+ only) |
      | `MCC_AST_BITFLAG` | `0` | off (search) |
      | `MCC_AST_CPROP_JOIN` | `0` | off (search) |
      | `MCC_AST_CSE_JOIN` | `0` | off (search) |
      | `MCC_AST_CALL_WINDOW` | `0` | off (search) |
      | `MCC_AST_COST` | `0` | off (analysis-only) |
      | `MCC_AST_NO_CALLFUL` | `0` | off |

      Numeric knobs (not on/off): `INLINE_LIMIT`/`PROMOTE_LIMIT`/`OPT_LIMIT`
      (default -1 = unlimited), `INLINE_NODES` (64), `GRAFT` (2048),
      `BITFLAG` min-threshold (5). `PERFN`/`FN_CONFIG`/`JITSCORE`/`HASH_OUT`/
      `REPLAY_DUMP` are search/tooling plumbing, not codegen passes. So the
      remaining default-off *codegen* passes to consider for §41 are exactly:
      **SETHI, BITFLAG, CPROP_JOIN, CSE_JOIN, CALL_WINDOW** (COST is analysis).
- [ ] **Flip defaults to ON**, pass by pass (not all at once), each behind the
      same landing gate it has today so a regression is bisectable: after each
      flip, the pass runs by default at its target `-O` level. NOT flipped:
      changing the default `-O2` codegen for all users is a user-facing,
      hard-to-reverse decision, held for explicit sign-off + the cross-target
      CI re-baseline below (not an autonomous flip).
- [~] **Regression matrix per flip:** full `ctest` green on every preset that
      builds here (native `macos`; plus any cross/qemu presets that run), the
      3-stage self-host **fixpoint byte-identical**, and `-O4` search still
      converges. Critically, verify **lower levels (`-O0..-O3`) still pass all
      tests** — ungating changes their output, so the exec-golden / `--ast`
      columns / dash-s-bytes goldens must be re-baselined intentionally and
      re-verified, not silently broken. **CORRECTNESS PRECONDITION MET
      (2026-07-10, x86_64):** each of the five remaining default-off codegen
      passes — SETHI, BITFLAG, CPROP_JOIN, CSE_JOIN, CALL_WINDOW — forced ON
      via `MCC_AST_*=1` at `-O2` over the *entire* `src/mcc.c` self-source:
      **3-stage fixpoint CONVERGES (stage2==3==4) AND the self-built mcc is
      correct** (compiles+runs a probe to the right answer), for all five
      independently. So none carries a self-host miscompile hazard — the flip
      is a **byte-golden re-baseline**, not a correctness fix. What remains:
      regenerate the x86_64 `dash-s-bytes`/`--ast`/exec goldens for the flipped
      default, then the cross-target sweep below. (This is the safe evidence an
      autonomous pass can produce without changing user-facing defaults.)
- [ ] **Cross-target / runner sweep:** the byte-identity goldens are
      per-target; a default-ON pass must be validated on each
      target×preset×runner the CI matrix covers (x86_64/arm64/riscv64 ×
      elf/macho/pe × qemu/native), not just the local box — items that can't
      run here (Bucket C, §40.4) are gated on CI.
- [ ] **Decision record:** for any pass that cannot be safely defaulted-on
      (byte-identity or correctness cost), document why and leave it gated.
      The deliverable is a validated default-on set + a residue list, not a
      blind flip.

Gate: this is the inverse of the whole ladder's "default-off" invariant, so it
is correctness-critical — treat each flip as its own increment with the full
fixpoint + ctest + native-arm64 + CI-matrix gate.

# 42. OPT.md — plain-English walkthrough of the `-O4` pipeline (docs)

Requested 2026-07-10. Write a new `OPT.md` that explains, step by step in plain
English, how `-O4` works end to end, for a reader who does not know the code:

- [x] The driver path: what `-O<N>` for `N>=4` triggers (`mcc_superopt_search`
      / `mcc_superopt_perfn`), the ~N-second budget, SIGTERM save-and-stop.
- [x] The search space: gate × budget (nodes × grafts × bitflag-threshold) ×
      opt-limit dimensions, the 3-strategy portfolio, greedy composition,
      doubling time slices.
- [x] Scoring: `.text`-size objective vs the `MCC_AST_JITSCORE` cpu+RSS tier
      (best-of-K `-run` timing), non-runnable → size fallback.
- [x] The AST-replay optimizer the search permutes: replay/reemit, the passes
      (const-prop, CSE/LICM, join dataflow §32a/b, promotion, inlining,
      bit-flag §30, Sethi–Ullman §35), and the faithfulness gate.
- [x] Persistence: the per-user cache (`so-*.ck` / `pf-*.ck` / `mcchv-*`),
      resumable warm-start, flock + keep-best + durable writes; `--clear-cache`.
- [x] How it stays safe: default-off gates, byte-identity, 3-stage fixpoint.
      Cross-link STATUS.md (state) / OPTIMIZE.md (history) / this file (design).

Docs-only; no code, no fixpoint impact. Should read as a narrative, not a spec
dump — pull the authoritative facts from STATUS.md + §18–§40 here.
