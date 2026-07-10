# EXCESS — crucial detail dropped from MCC.md

Consolidating `docs/` into the succinct [MCC.md](MCC.md) would lose the specifics
below. This file preserves that residual, grouped by the former source doc, so it
can be reviewed and either folded back, kept, or discarded. Nothing here is
getting-started material (that stays in [README.md](README.md)); it is reference
detail, deliberate-behavior records, measured numbers, and open work.

---

## Platform ABI & runtime notes (was NOTES.md)

- **`long double` follows each target's platform ABI, byte-for-byte with the
  native compiler:** x86_64/i386 ELF = 80-bit x87 extended; arm64/riscv64 Linux
  ELF = 128-bit IEEE quad; **arm64 Darwin = 64-bit (== `double`)**; **any
  PE/Windows (MSVC ABI) = 64-bit (== `double`)**. On Apple-arm64 and all PE,
  `sizeof(long double)==8` (`MCC_USING_DOUBLE_FOR_LDOUBLE`) — the Apple/MSVC ABI,
  not an mcc limitation. Pinned by `cli/apple_arm64_long_double_is_double`.
- **`<math.h>` is host-provided** — mcc ships no freestanding `math.h`; math
  links against the host/target libm. A freestanding non-glibc host must supply
  its own.
- **Mach-O TLS:** a `__thread` *defined* in the module works (TLV descriptor);
  referencing an `extern __thread` defined in another module hard-errors
  (`Mach-O: external thread-local '<x>' is unsupported`) — intentional. Pinned by
  `cli/macho_extern_tls_unsupported`. Lifting it needs TLV import descriptors +
  GOT-indirect loads in codegen and the Mach-O writer; gated behind a real need.
- **win32 runtime shims are intentional subsets** (`runtime/win32/include`):
  `<pthread.h>` mutexes non-recursive (`RECURSIVE` accepted, behaves as plain
  SRWLOCK), no thread-key destructors, no cancellation; `<sched.h>` minimal;
  `<fenv.h>` has no rounding-control-register access on non-x86/arm64 PE.
- **Inline-assembler = modelled subset, not full ISA.** Unmodelled mnemonics
  hard-error by name (`"ARM64 instruction '<m>' not implemented"`).
  - arm64 supported: `add(s)/sub(s)`, `and(s)/orr/eor`, `asr/lsl/lsr/ror`, `mul`,
    `mov/movk/movn/movz`, `mrs/msr`, `ldr(b/h)/ldp/str(b/h)/stp`,
    `b/b.<cond>/bl/blr/br/cbz/cbnz`, `adrp`, `isb/dsb/dmb`, `ret`, `nop`.
    NOT modelled: `cmp/cmn/tst`, `csel/cset`, `udiv/sdiv`, `madd/msub`, FP/SIMD
    (`fadd/fmov/…`), `neg/mvn`, atomics (`ldxr/stxr`).
  - arm (32-bit): 64-bit (`long long`) inline-asm operands unmodelled (GPR-pair
    case hard-errors) — use two 32-bit operands or a memory round-trip.
- **i386 `__fastcall`/`__thiscall`:** a register-eligible integer arg following a
  stack-spilled arg is an accepted limitation (errors "fastcall with a
  non-register argument before an integer register argument is not supported").
  Straight-ahead register-then-stack ordering is fully supported
  (`i386-fastcall-abi`).
- **mingw-w64 SEH miscompile of `gen_function` (fixed):** a nested `setjmp` (the
  AST-replay error trap) made `RtlVirtualUnwind` fault ~30% on mingw-gcc `-O2/-O3`
  when an `error1()` longjmp unwound through its frame. Fix: build just the
  setjmp-holding frame unoptimized on mingw-gcc hosts
  (`__attribute__((optimize("O0")))`, gated `MCC_HOST_WIN32 && __GNUC__ &&
  !__clang__`). Since the AST migration into `mccast.c`, that frame is
  `ast_func_end` (the replay driver), not `gen_function`.

## Toolchain / capability comparison (was NOTES.md)

- Format support (`Y`/`~`/`-`) — mcc alone does multi-target-from-one-build and
  ELF+PE+Mach-O; only mcc has `-run`, an embeddable compiler library, integrated
  assembler + single-pass/fast-compile + tiny (~1 MB) footprint. mcc has **no
  optimizing codegen** (the one column where gcc/clang/mingw/msvc lead). riscv64
  and ELF/Mach-O output: mingw/msvc `-`.
- libc coverage: glibc/musl ELF full; msvcrt PE (wine + native); libSystem
  Mach-O (qemu + native macOS).
- MSVC "breaks C99/C11 standards; quirky"; MSVC has no inline `asm` on x64.

## Build config — full CMake node catalog (was BUILD.md)

The complete 55-node catalog (types/defaults/gates/validation) is large; the
high-value specifics the summary omits:

- **Executable shapes:** `mcc` (always, self-contained single-source, libc-only);
  `mcc-static` (`MCC_BUILD_STATIC_EXE`, `MCC_SINGLE_SOURCE` decides self-contained
  vs libmcc.a); `mcc-dynamic` (`MCC_BUILD_DYNAMIC_EXE` **and**
  `MCC_SINGLE_SOURCE=OFF` — non-amalgamated driver linking primary libmcc; skipped
  under default MCC_AMALGAMATED=ON, the recurring `pstrcpy` link reason). Each with
  a `-musl` sibling. Cross compilers (`MCC_ENABLE_CROSS`) build the same shape for
  11 targets, host binaries, `-static` but never `-dynamic`.
- **Libraries:** default `libmcc.so`; `MCC_BUILD_STATIC_LIB` → `libmcc-static.a`;
  +`MCC_BUILD_DYNAMIC_LIB` → both.
- **Feature toggles baked as `CONFIG_*`:** `MCC_CONFIG_MINGW`, `BACKTRACE`,
  `BCHECK` (needs BACKTRACE), `ASM`, `PREDEFS`, `PIE`/`PIC` (ELF),
  `RUN_MMAP_EXEC` (W^X kernels), `NEW_DTAGS` (ELF), `AUTO_MCCDIR`, `LIBC`,
  `DWARF`, `SEMLOCK`, `LSP`, `OPTIMIZER`, `NEW_MACHO`/`CODESIGN` (Darwin).
- **Instrumentation:** `MCC_BUILD_SANITIZE` (GCC/Clang→ASan+UBSan;
  MSVC→`/fsanitize=address`; mingw/PE→trap-mode UBSan; alignment excluded — mcc's
  one intentional unaligned idiom), `MCC_BUILD_PROFILE` (`mcc_p`, `-pg -static`,
  fatal on Darwin), `MCC_BUILD_COVERAGE` (`mcc_c`).
- **`MCC_EMBED_MCCRT`** (default ON, forced OFF on WIN32): bakes `libmccrt.a`
  into the `mcc` binary; forces host-CC-built mccrt to break the build cycle.
- **Cross-value validation** (`mcc_validate_config`, several fatal unless
  `MCC_CONFIG_AUTOCORRECT`): PROFILE needs GCC/Clang host + not Darwin;
  cross-compiling with empty `MCC_EMULATOR` fatal; BCHECK needs BACKTRACE;
  SEMLOCK must be numeric; toolchain-profile entry must be in
  {auto,gcc,clang,mcc,msvc,mingw}; ELF-only knobs on WIN32/Darwin warn as inert.
- **dist presets** produce all permutations (Release, tests OFF,
  MCC_AMALGAMATED=OFF, both libs, all cross compilers) and package `mcc-`,
  `libmcc-`, `mcc-cross-`, `bundle-`, `checksums-` archives via `ci pkg` into
  `dist/`. `dist-msvc` skips per-cross `-static` (`-static` is a GNU-ld flag).
- **Benchmark** (`MCC_BENCH`, CI-on): `mccbench` races mcc vs host compilers over
  3 workloads, writes `dist/bench-<plat>.txt` (time/CPU/RSS/obj-size/fns-per-sec).

## Code-facing config surface (was CONFIG.md)

- Naming transform: `MCC_CONFIG_<T>`↔`CONFIG_MCC_<T>` (swap); `MCC_<T>`→
  `CONFIG_MCC_<T>` (prefix). Booleans emit `=0/=1`; strings via `mcc_def_str`.
- Read flags: `ASM AUTO_MCCDIR BACKTRACE BCHECK CPUVER CROSSPREFIX CRTPREFIX CST
  ELFINTERP LIBPATHS MUSL PIC PIE PREDEFS SEMLOCK SWITCHES SYSINCLUDEPATHS`.
- Intentional asymmetries `ckconfig` allowlists: `MCC_CONFIG_UCLIBC` (emitted,
  dead — legacy provenance), `BACKTRACE_ONLY` (read, no node),
  `ELFINTERP_ARMHF`/`TOOLHOST` (header `#define`s), `MCC_CONFIG_STATIC`
  (per-target via `MCC_BUILD_STATIC_EXE`, Linux-only, read in
  `src/mcchost.*`/`libmcc.c` for the static `-run` symbol table).
- `ckconfig` (ctest `config-drift-invariant`) fails on DRIFT(a) read-with-no-
  provider and DRIFT(b) emitted-but-unread. Analogous host-macro rule:
  `tools/hostgate.c` / `MCC_HOST_*`.

## Profiling — method + measured results (was PROFILING.md + NOTES.md)

- **Method (reproducible):** workloads `tests/diff/full_language.c` (small,
  `-O0`-only — its `__bug_table` inline-asm miscompiles under any optimizer) and
  `src/mcc.c` (~100k-line amalgamation, carries release columns). Nine builds
  (gcc/clang debug+release, five `mcc` all `-O0 -g`: mcc-gcc/-clang/-self/-musl/
  -static). Host of record: Gentoo x86-64, kernel 6.18, 32 cores, gcc 15.3.0,
  clang 22.1.8; pinned to core 2, ASLR/turbo off, `performance` governor;
  `hyperfine` (don't use `-N` on the amalgamation — shell-escaped defines break).
- **`mcc.c` `-c` compile:** mcc-self 280 ms vs gcc-O2 16.1 s (57×), clang-O2
  13.4 s (48×); vs `-O0` refs still 4–8×. Peak RSS: mcc ~29 MB vs gcc-O2 329 MB
  (11×), clang-O2 242 MB.
- **Self-host cost inverts at `-O0`:** mcc-self (280 ms) is the *fastest* glibc
  mcc, beating gcc-`-O0`-built (290) and clang-`-O0`-built (369). The old "1.7×
  bootstrap cost" was a RelWithDebInfo artifact. All mcc builds emit
  byte-identical code (deterministic, host-cc-independent).
- **Where time goes (`perf`):** compile `-c` is CST-hash-consing-bound
  (`cst_mix64` + `cst_hash_bytes` ~30% self-time, called once per interned node —
  the win is *fewer* calls); preprocess `-E` is lexer-bound (`next_nomacro`+`next`
  ~21%). Applied lexer win: `TOK_HASH_SIZE` 16384→65536 (1.03–1.06× faster `-E`,
  +384 KB fixed table). One kept micro-opt (cache interned `TokenSym`); three
  SWAR/char-class ideas rejected as below-noise or unfit. Lexer experiments
  belong on `-E src/mcc.c`, not the 416-line TU.

## Conformance — deliberate differences & gaps (was C9911.md / NOTES.md / TESTS.md)

- **C9911.md** is the clause-by-clause ledger (3,837 paraphrased-requirement
  checkboxes citing N1256/N1570, each tagged mcc/gcc/clang `✓`/`~`/`✗`). It is a
  *specification map*, not a work tracker. If regenerated, refresh against the
  gcc/clang clones. Consider keeping it verbatim — it is not compressible without
  losing the per-clause citations.
- **`__STDC_VERSION__` default:** mcc advertises C99 (`199901L`) while shipping
  C11 freestanding headers/atomics/threads/`_Noreturn` — by-design (gcc 15
  defaults C23, clang 22 C17).
- **Deliberate DIFFs (conformant, kept):** assignment-to-const warns (gcc/clang
  error); comma in ICE / `_Noreturn` on an object diagnosed only under
  `-pedantic`; invalid token-paste `C(+,-)` is warn-and-continue rc=0 (locked by
  `pp_invalid_paste`); four macOS `diff3` divergences are expected residual
  (predefined-macros default, MS bitfield layout, cleanup teardown order,
  freestanding-headers where mcc is most conformant).
- **Real mcc-specific gaps (diverges from gcc==clang consensus; in TODO):**
  1. §6.7.4p6 plain `inline` with no external def emits a global → an `-O0` call
     *links* (rc 0) where gcc/clang fail (widest correctness impact).
  2. §6.9.1p6 old-style K&R identifier-list params accept implicit-`int` with only
     a warning (diagnostic-only).
  3. §7.16.1.4p3 `va_start` 2nd arg not last named param silently accepted on
     x86_64-SysV/i386 (fires on arm64/riscv64/PE) — needs a SysV `gen_va_start`
     rework.
  4. §7.26.1 bundled `include/threads.h` resolves ahead of the system header
     (shadowing; usage still works).
  - *Corrected/stale:* implicit function declaration now errors by default
     (matches gcc) — the ledger row was stale; re-sweep.
- **Consensus omissions (all three lack; conforming — do not chase):**
  `_Imaginary` type family (Annex G), Annex K bounds-checking, inexact-hex-float
  diagnostic, incomplete-`_Generic`-association diagnostic.
- **Test-coverage-depth gaps (mcc passes but under-tests vs gcc/clang):** flexible
  array members (gcc 13 vs mcc ~1), `_Noreturn` (1 vs 5), `_Alignas`/`_Alignof`,
  VLA goto/switch-into-scope diagnostics, UCN-in-identifier breadth, FP
  eval-method / Annex-F wide returns, `_Complex` Annex-G edge cases, a systematic
  negative/`dg-error` tier (gcc's C99/C11 files are ~70% diagnostic).
- **TESTS.md** cross-indexes gcc (`gcc.dg/c99-*`×135, `c11-*`×126, `atomic/`×48,
  torture) and clang (`test/C/C99`×19, `/C11`×16, one-file-per-WG14-paper) trees
  against mcc's suites; it is the narrative summary of the C9911 ledger.

## AST intention-IR — design (was AST.md) + status (was AST-STATUS.md) + backlog (TODO.md)

- **Design principle:** two tiers split by machine-dependence — Intention (15
  node kinds: TranslationUnit, BasicBlock, If/Jump/Return, Ref/Literal/Load/
  Store/Unary/Binary/Convert/Invoke/InitList, Poison) is portable; Machine tier
  (addressing modes, register/stack placement, C ABI, StackAlloc) is realized at
  lowering. `Label`/`Block`/`Decl` dissolve; types/symbols live in `CType`/`Sym`.
- **Memory model:** `Ref` yields an *address*; every read is `Load`, every write
  `Store`; no hidden lvalue→rvalue. Storage class is a backend decision subject to
  source constraints (`volatile`/`_Atomic`/`register`/address-taken). VLA/`alloca`
  is the one lifetime≠liveness exception (LIFO SP save/restore at scope edges).
- **Two drivers, one emitter:** the vstack API (`vpush*`/`gen_op`/`gv`/`gsym`/
  `gjmp`) is the shared emit core. `-O0` = the streaming parser drives it
  (untouched, byte-identical). `-O1` = a second driver walks the optimized AST and
  calls the *same* ops. Founding reframe (§18): the backend is feature-complete
  C11 (exec suite proves it), so replay never fakes anything — every gap is an
  AST-**query** gap, not a codegen gap, tiered by query cost (Tier 1 O(1) field
  reads → Tier 4 whole-program/fixpoint).
- **Opt gating (`-O0` byte-untouched):** the replay stack is driven by the
  runtime `-O` level, batched by measured compile cost — `-O0` (the default)
  disables it; `-O1` = replay + const-fold templates (+13–22% compile cycles,
  templates ≈ free); `-O2`/`-Os` = + Tier-3 promotion (x86_64 only, +5–7.5%);
  `-O3`/`-Ofast` = + Tier-4 virtual inline (+1–4%, excluded from `-Os`). The
  former `MCC_AST_REPLAY` env master gate is gone. `MCC_AST_TEMPLATES`,
  `MCC_AST_PROMOTE` and `MCC_AST_INLINE` remain env overrides on top of the
  `-O` defaults (non-`0` opens, `0` closes); `MCC_AST_REPLAY_DUMP` and
  `MCC_AST_NO_CALLFUL` stay opt-in. The default-on
  flip was reverted: the full-corpus `mcctest` differential exposed
  Tier-3/Tier-4 soundness gaps on the PE target and arm64 that the per-golden
  columns miss. Faithfulness gate: pass 1 replays with no
  transform and **byte-verifies vs `-O0`**; only if faithful does pass 2 apply the
  transform, then gated by the **exec-golden differential**; unfaithful → `-O0`
  fallback.
- **Tier 1/2 (`-O0` replay parity, ✅ arch-independent):** covers essentially all
  of C — floats, call-result stores, struct member/copy/deref/`f().x`/bitfields,
  switch, goto/labels, all struct-return ABI forms + by-value struct args, full
  `_Complex`, short-circuit-as-value, VLA/`alloca`. Key mechanisms: ordinal
  frame-slot reuse (`ast_alloc_loc`/`ast_locrec`, `ast_fconst`), suppress-and-fold
  coarse nodes. Two latent bugs fixed en route (call double-emit; float const-pool
  dup).
- **Tier 3 register promotion (✅ x86_64):** address-not-taken int/long/pointer/
  float locals pinned to registers, seeded from the stack slot at entry (valid
  across control flow/params). Call-free uses caller-saved R10/R9/R8; call-ful
  uses callee-saved RBX/R12–R15 (push at entry / pop at return funnel); floats use
  XMM6/XMM7 (call-free only). Weighted selection `2^loop-depth`. Poisons:
  `&`-taken, member/`p->m` bases, aggregate slot ranges, `++`/`--`, bitfields,
  `volatile`/`_Atomic`, structs, inline asm; VLA+call-ful bails. Required a
  byte-neutral backend fix (`gen_modrm_impl`/`store()` SIB/REX.B for r12–r15
  bases, diffed `.o`-for-`.o` across 55 files). Two documented not-done cases:
  `p->m` bases (needs "register base + live displacement" addressing), >2 float
  pins (needs XMM8–15).
- **Tier 4 virtual always-inline (🟢 arch-independent, arm64-verified):** inline a
  within-TU `static` non-variadic VLA-free size-bounded callee (any intra-function
  control flow, multiple/early returns, int/ptr/float/double + by-value struct
  params §19.2, struct/scalar returns, its own calls, forward callers via
  defer-to-TU). Returns coalesce via a memory phi (result slot). Cycle guard
  depth 8. `hi`-based frame bias unifies x86_64 (negative params, byte-identical)
  and arm64 (positive params). Per-site specialization §19.3 (constant-arg
  const-prop + dead-branch elim under `MCC_AST_TEMPLATES`). Composes with Tier-3.
  Excluded: `_Complex`/`long double`/bitfield params, pointer-to-VLA params,
  `setjmp`-calling callees (guard query), block-scoped-Sym re-emit (poisoned →
  real forward call).
- **A4 acceptance gate (✅ wired 2026-07-09):** GCC c-torture (~3766 tests) as a
  **differential** AST gate (`mccharness gcctestsuite --ast <replay|promote|
  inline|inline-tmpl>`): pass-at-`-O0`-but-fail-under-column = regression. Surfaced
  and fixed 10 pre-existing replay regressions (computed-callee bail,
  frame-depth-faithfulness); 3 documented-known (`pr51581-1/2` pp-const-expr state
  corruption, `20070919-1` cyclic VLA-in-struct crash).
- **`-O1` wiring (UNPARKED 2026-07-09, §20):** `mcc -O1` engages replay + Tier-3
  promotion (both call-free and call-ful pools; Tier-4 inline stays behind
  `MCC_AST_INLINE`). Both former blockers are resolved: (1) the pp-const-expr
  corruption was a stale muted sequence-point warning firing inside a later `#if`
  (fixed by `seqp_reset()` on replay restore); (2) the inline governor is the
  per-function `AST_GRAFT_BUDGET`. `mcc -O1` now self-hosts and the `-O1`-built
  compiler's `-O0` output is byte-identical to the `-O0`-built compiler's.
  Whether `-O1` should also enable Tier-4 inline by default is the one open
  policy call (inline is correct + self-hosts under the governor, but the
  compile-time/size trade-off past the governor cap is unmeasured).

## AST — open backlog / revisit-triggers (was TODO.md)

- **`-O1` transform soundness backlog — CLEARED 2026-07-09.** All 21 KNOWNGAPs
  (14 promote + 4 inline + 3 replay) are fixed and `GCCTS_AST_KNOWN_{REPLAY,
  PROMOTE,INLINE}` are all **empty**; every gcc-torture AST column reports 0
  regressions with no baseline. Root causes were five x86_64 high-register
  (r8–r15/pin) encoding bugs in `store`/`load`/`gen_opf` (missing/misplaced REX,
  in-place SSE-dest pin clobber, setcc REX.R) invisible at `-O0`; two promotion
  analysis holes (address-escape range poisoning; VLA functions no longer
  promote); the inline graft not spilling caller regs before the callee body
  (`save_regs(0)`) + honoring `__attribute__((noinline))` + only retaining
  byte-faithful callees as graft candidates; and for replay, a stale
  sequence-point warning firing inside a `#if` (fixed by `seqp_reset()` on
  restore) + a self-cycle guard in `aggr_has_const_member`. A `gen_cast`
  double-emit (internal truncation shifts captured as AST nodes) was also fixed
  via `ast_in_op` suppression, and a recycled-`Sym` class of bugs (garbage
  symtab names breaking the `-O1` self-LINK) closed by deferring `sym_free`
  across the capture→replay window.
- **`-O1` self-hosts (2026-07-09).** `mcc -O1` (replay + Tier-3 promote, both
  call-free and call-ful pools; inline excluded by design) compiles + links
  itself, and the `-O1`-built compiler's `-O0` output is byte-identical to the
  `-O0`-built compiler's. Replay + promote + **inline** combined also self-hosts
  byte-identically, gated by: a per-function `AST_GRAFT_BUDGET` (2048 nodes)
  governor, and `ast_no_callful_promo` — Tier-4 inline and call-ful Tier-3
  promotion both claim the callee-saved bank (RBX/R12–R15), so a function that
  will inline-graft restricts promotion to the call-free pool (the one real
  promote×inline interaction bug; found fast via the env-flag fallthrough gates
  rather than byte-diffing). `MCC_AST_INLINE_LIMIT=N` caps grafts to the first N
  (bisection aid).
- **A1 — backward-liveness spill-slot sharing (the remaining ratified roadmap
  item; a quality enhancement, not a correctness gap — `-O1` is complete and
  self-hosting without it):** pin sharing across disjoint live ranges needs a real
  backward-liveness pass + interval coloring. Blocker: promotion currently
  entry-seeds every pin (mirrors `-O0` for read-before-write/params/loop-carried),
  which conflicts with sharing — A1 must replace entry-seeding with per-live-range
  seeding (a rework of the emit model, not just added analysis).
- **Tier-4 remainder:** `ast/replay-inline-spec` is already registered on arm64
  (the `x86_64 OR arm64` branch; CI arm64 runners exercise it); only riscv64/other
  arches stay gated until per-arch verified.
- **Promotion on arm64/riscv64:** needs a backend register-model extension
  (arm64 `MCC_NB_REGS=28` doesn't expose x19–x28) + qemu validation; the arch-agnostic
  analysis is reused.
- **Decided-with-revisit-trigger backlog:** verify the CST answers every `-g`/LSP
  query (else reopen the dissolved `Bind` marker); `k` value (raise always-inline
  depth only under `-O2/-O3`/budget); size-gated outline; store factoring (first
  virtual-inline render is the shared engine's 2nd user); template DSL past ~30
  templates; per-function `-O1` mode; PP-as-executable-C JIT (parked).
- **Long horizon (design only):** broader template library (algebraic,
  dead-branch, jump-table), time-budgeted engine, dependency-ordered `-O1`,
  cross-TU LTO, `-g` from provenance, hot-reload snapshots, separate `-O2`/`-O3`
  (SSA) drivers.

## CST database — design record (was NOTES.md §"CST database")

- Companion side-channel to the AST (`src/mcccst.{c,h}`): byte-faithful
  *concrete* syntax tree, hash-consed / content-addressed (`store`/`binding`/
  `render` engine, `H_s` structural hash), snapshot-serializable
  (`cst_snapshot`). Built via `ast_hook_*`-style vstack hooks, CST-independent of
  the AST (each subsystem functions with the other off; shared storage only when
  both on, gated `MCC_CONFIG_LSP || MCC_CONFIG_OPTIMIZER`). Substrate for `-g`/LSP (lexical
  scope spans + source ranges) and the AST's shared inline/template engine.
  Frozen spec + completed vertical slices are in the former NOTES.md (§0–§11 +
  "Completed work — CST database", D1–D5). Its per-node hash-consing is the #1
  compile hot spot (see Profiling).

## Open non-AST TODO (was TODO.md "Now" + audits)

- **CMake normalization ("as much as possible"):** minimize gating (prefer
  autodetect + enable-what-the-host-supports; 25 `find_program`s already),
  reduce CMake by offloading to `tools/`, fold `.cmake` files in. Assessment:
  folding largely moot (already monolithic); pursue incrementally with a
  verifiable target, not a sweep (CI-breakage risk across ~35 presets/platforms).
- **i386 TLS residual:** x86_64 GD/LD/IE/LE covered (`tls-models` ctest);
  `R_X86_64_32[S]` range covered (`cli/x86_64_reloc_32s_range`). Still open: i386
  `R_386_TLS_GD/LDM` paths need an i386 cross build + 32-bit sysroot.
- **Skipped-test ungating audit (per triple):** re-evaluate each `mcc_skip_test`
  when a host can build/run that triple. Status: x86_64-linux full;
  i386-linux blocked (no 32-bit sysroot); aarch64/armv7-linux partial (qemu is
  x86-TSO — can't validate weak-memory atomics; ungate only memory-model-
  independent subset); x86_64/i386-windows available (mingw+wine); arm64-windows
  blocked (no native arm64 ref cc); x86_64/arm64-Darwin audited 2026-07-08 (all
  remaining skips legitimate).
- **Divergence check** for the NOTES.md ctest counts: **decided NOT** to add a
  strict count-checker — the registered total is documented to track upstream test
  additions, so a hard drift-fail would break CI on every new test.

## "ACHTUNG — DO NOT DO" list (verbatim from TODO.md tail)

Explicitly-parked ideas, kept as a record of decisions *not* taken:

- Only human-friendly warnings/errors, backed by tests checking formatted output
  against terminal dimensions/configuration.
- Implement/finish `-g` debugging/debugger + gdb test cases vs gcc/clang.
- Optimization `-O1..100` levels measured in max-seconds-to-spend-optimizing.
- Hot reload via saving/loading CST snapshots on the fly and on run
  (`--hotreload`); run hot-reloads from reconciled CST snapshots.

## Detailed enumerations — the full tail MCC.md defers here

Exhaustive lists that `MCC.md` names but does not spell out, kept so no
`docs/*.md` datum is lost.

### Full CMake node catalog (was BUILD.md §1–§13, 55 nodes)

- **Standard CMake values:** `CMAKE_BUILD_TYPE` (Debug/Release/RelWithDebInfo/MinSizeRel, forwarded to matrix cells), `CMAKE_C_COMPILER` (the real compiler switch — profile only seeds defaults), `CMAKE_CROSSCOMPILING_EMULATOR`→`MCC_EMULATOR` (needed to run foreign mcc), `CMAKE_TOOLCHAIN_FILE`, `CMAKE_INSTALL_PREFIX` (defaults to `MCC_DIST_DIR`; must be set at configure time — mcc runtime dir bakes an absolute path), `CMAKE_OSX_DEPLOYMENT_TARGET` (auto-pinned for Homebrew gcc), `CMAKE_EXPORT_COMPILE_COMMANDS` (ON, before `project()`).
- **Build-target knobs:** `MCC_TOOLCHAIN_PROFILE` (auto/gcc/clang/mcc/msvc/mingw; list→superbuild), `MCC_ENABLE_CROSS`, `MCC_BUILD_STATIC_LIB`, `MCC_BUILD_STATIC_EXE`, `MCC_BUILD_DYNAMIC_LIB`, `MCC_BUILD_DYNAMIC_EXE` (ON, gate MCC_AMALGAMATED=OFF), `MCC_BUILD_MUSL`, `MCC_BUILD_STRIP`, `MCC_ENABLE_RPATH` (ON, !static-lib), `MCC_SINGLE_SOURCE` (ON), `MCC_BUILD_TESTS` (ON), `MCC_BENCH`, `MCC_MCCRT_USE_HOSTCC` (auto-forced when no emulator/asm-off/embed), `MCC_EMBED_MCCRT` (ON ELF/Mach-O, OFF WIN32), `MCC_CONFIG_AUTOCORRECT`, `MCC_MINGW_SOURCE` (winlibs/multilib).
- **Diagnostics/instrumentation:** `MCC_ALL_DIAGNOSTICS`, `MCC_BUILD_SANITIZE`, `MCC_BUILD_PROFILE` (mcc_p, `-pg -static`, fatal Darwin/MSVC), `MCC_BUILD_COVERAGE` (mcc_c).
- **Feature toggles (→ CONFIG_*):** `MCC_CONFIG_MINGW`, `_BACKTRACE`, `_BCHECK` (needs BACKTRACE), `_ASM`, `_PREDEFS`, `_PIE`/`_PIC` (ELF), `MCC_RUN_MMAP_EXEC` (W^X kernels), `_NEW_DTAGS` (ELF, DT_RUNPATH vs DT_RPATH), `MCC_AUTO_MCCDIR`, `_LIBC` (uClibc/musl/''), `_DWARF` (0/2/3/4/5/''), `_SEMLOCK` (numeric, fatal else), `_LSP`, `_OPTIMIZER`, `_NEW_MACHO`/`_CODESIGN` (Darwin).
- **Runtime path overrides** (STRING '', mirror `configure --…`): `MCC_SYSROOT`, `_TRIPLET`, `_SYSINCLUDEPATHS`, `_LIBPATHS`, `_CRTPREFIX`, `_ELFINTERP`, `_SWITCHES`, `_OS_RELEASE`, `_INSTALL_MCCDIR`.
- **Extra build flags:** `MCC_EXTRA_CFLAGS`/`_LDFLAGS`/`_LIBS`.
- **ARM ABI (gate MCC_CPU==arm):** `MCC_ARM_EABI`/`_VFP`/`_HARDFLOAT`/`_IDIV` (from `__ARM_FEATURE_IDIV`), `MCC_CPUVER` (from `__ARM_ARCH`).
- **Superbuild:** `MCC_TARGETS` (native/cross/custom), `_SUPERBUILD_TEST`/`_SEQUENTIAL`/`_CHILD`, `_MSVC_GENERATOR`/`_MSVC_PLATFORM` (x64), `MCC_TARGET_ARGS_<name>`, `MCC_CC_<tc>`, `MCC_GNU_GCC`.
- **Vendor/downloads:** `MCC_VENDOR_DIR` (input root, vendor-first), `MCC_DIST_DIR` (output root, docker-mounted /dist), `MCC_MINGW_DIR`/`_WINLIBS_VER` (`16.1.0-ucrt`)/`_X86_64_URL`+`_SHA256`/`_I686_URL`+`_SHA256`/`_MULTILIB_*`, `MCC_CLANG_DIR`/`_URL`/`_SHA256`/`_SUBDIR`, `MCC_CONFIG_EXTRA` (config-extra.cmake). Vendor subdirs: `winlibs-mingw-w64-*`, `mingw-w64-multilib`, `llvm-clang`, `musl-src`, `musl-sysroot`, `gnu-gcc`, `gentoo-stage3-<arch>-<libc>`.
- **Test-suite knobs:** `MCC_REF_CC`, `MCC_DIFF3_GCC`/`_CLANG`, `MCC_GCCTESTSUITE_PATH`, `MCC_ARM_CROSS_COMPILE`, `MCC_DARWIN_HOST`, `MCC_CROSS_DIR`, `MCC_QEMU_TESTS`/`_MIRROR`/`_ARCHS`/`_LIBCS`/`_DLDIR`/`_DOCKER_ARCHS`/`_LIBCS`. (The former `MCC_WINE` knob was **removed** — wine is now self-located; `setup-wine` installs it.)
- **`clang-toolchain` vendored fetch:** `MCC_CLANG_URL`/`_SHA256`/`_SUBDIR` carry pinned defaults for Windows x86_64, Linux x86_64/arm64, and macOS arm64; `cmake --build <bld> --target clang-toolchain` fetches a SHA256-verified LLVM into `vendor/llvm-clang/` (auto-wired next reconfigure), and `ci local` runs it automatically when it probes no system clang — supplying the 2nd reference for the 3-way diff3/preprocess suites on hosts with no system clang.
- **Derived (assert, don't set):** `MCC_CPU`, `MCC_TARGETOS`, `MCC_CC_NAME`, `MCC_EMULATOR`, `cross_compiling`, `arm_abi`; validator `mcc_validate_config()`.
- **Cross compiler targets (11):** `mcc-i386`, `-x86_64`, `-arm`, `-arm64`, `-riscv64` (Linux); `-i386-win32`, `-x86_64-win32`, `-arm64-win32`, `-arm-wince` (WIN32); `-x86_64-osx`, `-arm64-osx` (Darwin).

### Full ctest suite / fixture inventory (was TESTS.md §1, README, NOTES coverage matrix)

- Suites & counts: `exec` (241, 17 dirs: features_c99_c11 46, types 30, statements 25, structs_unions 19, preprocessor 15, lexical 16, pointers_arrays 15, expressions 14, functions_abi 14, vla 5), `diff` (~35), `diff3` (corpus), `ast` (41), `preprocess` (~38), `qemu` (~42: conformance 16 + apple-libc 26), `cli` (16), `cst` (24), `sanitize` (1), `diagnostics` (1), `tls`/`static` (2), `embed`/`behavior`/`asm`/`ci`/`support`/`bench` (~30). Total ≈400+.
- Per-toolchain P/S/— matrix (Win mingw / Win gcc / Win msvc / Lin gcc / Lin clang / mac clang) for: `exec/*`, `mcctest`, `mcctest-bcheck` (S on Win/PE), `sanitize-smoke`, `preprocess/*`, `diff3/*`, `parts/*` (S on Win), `cli/*`, `libtest`/`-extra`/`-mt`/`abitest-cc`, `hello-run`/`-exe`/`vla_test-run`, `compile.*`, `asm-c-connect-test` (S mac), `dash-s-roundtrip` (S mac), `asm-gas-directives` (S all — missing `sgdtq`/`sidtq`/`swapgs`), `i386-fastcall-abi` (needs i386 cross + ELF-32 ref), `compile.win32.*`/`pe-native-conformance`, `pe-wine-conformance` (label wine), `macho-*` (8 drivers, label macho), qemu matrix (label qemu).
- macho drivers (8): `macho-structural`, `-codegen-run`, `-image-run`, `-apple-libc`, `-conformance-native`, `-stack-protector`, `-universal` (machofat), `-libsystem-kernel-fused` (needs MCC_DARWIN_HOST); `qemu-arm64-osx` (labels qemu;macho).
- **`native` label (test scheduling):** every non-qemu test additionally carries the `native` label (applied in one pass at end of configure); the single `_test-native` preset base runs `ctest -L native` on **every** host — no per-OS exclude lists. What executes is decided at configure time by host probes (wine, osx cross, Darwin host, …): a missing capability registers a skip-stub or self-skips (exit 77), so the native suite is always safe to run in full. `ctest -L native -N` lists exactly what the current tree considers native. `qemu` is the **only** non-`native` label; `qemu-arm64-osx` carries `qemu;macho` and stays out of the native suite.
- In-tree tool ctests: `host-gate-invariant`, `git-stamp`, `def-verify`, `build-md-nodes`, `config-defines`, `host-detect`, `cross-factory`, `ci-matrix`, `ci-pkg-smoke`, `qemu-fetch-parse`, `config-drift-invariant`, `preset-parity-invariant`, `tls-models`, `cli/x86_64_reloc_32s_range`.

### Full CST slice ledger (was NOTES.md §CST database)

Slices S0 (gating), B (SoA CstArena store: `kind[]/parent[]/first_child[]/next_sib[]/width[]/slot_key[]` + Token side-table), C (H_s/H_t hashing), D (geometry; D1d comments/trivia, D3/D5 SourceFile-template store dedup), E (mmap snapshot: versioned header magic+format+endian+section-table, `cst_reflect` DFS), F (byte-offset `cst_base`, `cst_node_at` binary search), G (owned-source; slice-G multi-file include stitching is the one `[ ]` open item — main file only), H (hooks), I (symbol-refs use→def, v1 last-declaration-wins), J (macro-fidelity `cst_hook_wrap`, `mccpp.c:4113`). Invariants: codegen-identity (§8.5, CST-on 830/830 vs -off 811/811 byte-identical objects), tiling/span-coverage (§8.2), round-trip (§8.1), offset→node (§8.3), corpus gate `cst_validate` over 308–312 files. Designed-not-built: `H_e` epoch hash (invertible slot-keyed O(1) edit patch; `slot_key` currently repurposed for `cst_mark_branch` PPConditional tags, `mcccst.c:512` — a future H_e must reconcile the dual use). Harness `tools/csttool.c` (links nothing from the compiler, §0.3).

### Full AST fixture + KNOWNGAP list (was AST-STATUS §5–§8, TODO §C1)

- `ast/replay-*` fixtures: `promote` (call-free/call-ful/pointer/float), `inline` (add/scale/madd/clamp/sgn/area/quad/pick/firsthit/mkpair/gsum/sumpt/sumbig>16B/addpt + fwd_sum/fwd_boxed defer-to-TU), `inline-spec` (choose/clampk/mul/addk specialize, x86_64-gated), `vla`, `complex_ctor`/`_imag`/`_arith`, `short_circuit`, `goto_dispatch`, `switch_dispatch`, `struct_ret_caller`/`_sret`/`_variadic`, `struct_member`, `struct_copy`, `bitfield`, `float_ops`, `call_store`, `ld_fallback`.
- Promote/Inline/Replay KNOWNGAP baselines: **all empty as of 2026-07-09** (the 14+4+3 listed historically are all fixed; `GCCTS_AST_KNOWN_{REPLAY,PROMOTE,INLINE}` in `tools/mccharness.c` are `{0}`). Historical list, for provenance: promote (14) call-free FLOAT (7) `941021-1`,`postmod-1`,`990829-1`,`920929-1`,`pr36343`,`pr28982a`,`pr15262`; call-ful GP (6) `20080519-1`,`20170111-1`,`20020402-3`,`loop-8`,`20000722-1`,`pr28982b`; call-free int (1) `pr119002`; inline (4) `usad-run`,`pr45070`,`ssad-run`,`pr41750`; replay (3) `pr51581-1/2`,`20070919-1`.
- Key source symbols (`src/mccgen.c` unless noted): replay `ast_replay_body`/`_bb`/`_value`; promotion `ast_plan_promotion`/`ast_promo_weigh`/`ast_promo_write`/`_entry_init`/`_push`/`_pop`, pools `ast_promo_caller`/`callee`/`xmm`; inline `ast_fn_inlinable`/`ast_inline_capture`/`_graftable`/`_retain`/`_lookup`/`_pool`/`ast_local_is_readonly`/`ast_inline_graft`/`ast_in_graft`/`ast_inline_bias`/`ast_graft_rt`/`ast_argsub_*`; backend addressing `gen_modrm_impl`/`store()` (`src/arch/x86_64/x86_64-gen.c`); gate `suite_gcctestsuite`/`gccts_ast_skiplisted` (`tools/mccharness.c`).

### Internal symbol & hook inventory (was AST.md / NOTES.md / CONFIG.md)

- **AST capture hooks** (`src/mccgen.c`, fire from vstack positions, early-return on `!ast_active`): `ast_hook_leaf`, `ast_hook_stmt`, `ast_hook_vpush`/`_vpop`, `ast_hook_genop`, `ast_hook_convert`, `ast_hook_return`, `ast_hook_call_begin`/`_call_effect_end`, `ast_hook_member_begin`/`_end`, `ast_hook_label`/`_goto`, `ast_hook_switch_begin`/`_case`/`_default`/`_body_end`/`_end`, `ast_hook_landor_operand`, `ast_hook_vla_alloc_begin`/`_end`/`_vla_restore`, `ast_hook_builtin_complex_begin`/`_end`.
- **AST internals:** `ast_active` (build gate), `ast_finalize_leaf` (snapshots vtop CType/Sym), `ast_desync`/`ast_bail`/`ast_bad_type` (fallback triggers), `ast_capture`/`ast_clear_children`, `ast_in_op`/`ast_in_call` (suppress-and-fold), `ast_fconst`/`_i`/`_reuse`/`_record`/`_push_ref` (const-pool ordinal reuse), `ast_alloc_loc`/`ast_locrec`/`_i` (frame-slot reuse), `ast_replaying`, `ast_error_sink`/`stk_data_floor` (error trap), `ast_fold_eval`/`_rec` (const-fold), `ast_dump`, `ast_inline_cap_off`/`_depth`/`_stack`/`_bias`/`_ret_sym`/`_pool`, `ast_reemit`/`_retain`/`_forward_inlines`/`_poison`, `ast_pin_rodata_syms`/`ast_pin_type`, `ast_rp_switch`/`ast_rp_label_get`, `ast_sym`.
- **CST internals:** `cst_open`/`_close`/`_mark`/`_leaf` (`CST_*` macros), `cst_hook_token`/`_def`/`_use`/`_wrap`/`_include`, `cst_store_intern`, `cst_render`, `cst_reflect`, `cst_node_at`, `cst_base` (`src/mcc.h:464`), `cst_mix64`/`cst_hash_bytes`, `cst_mark_branch` (`mcccst.c:512`), `cst_validate`, `cst_rehash_dirty`, `cst_snapshot`, `slot_key`, `H_s`/`H_t`/`H_e`.
- **Additional knobs not in MCC.md:** `MCC_TEST_SH` (POSIX sh dir for cli/diff3 helpers), `MCC_TEST_RUNEMU`/`MCC_TEST_SYSROOT` (test emulator/sysroot), `MCC_LOCAL_CI_AS_TEST` (local-ci preset), `MCC_PROFILE` (defined by `mcc_p`; neutralizes `static`/`inline` at `src/mcc.h:205`), `MCC_TARGET_IS_HOST`/`MCC_TARGET_X86_64` (derived/target macros). Header include-guards (`MCC_COMPLEX_H`, `MCC_LIMITS_H`, `MCC_STDINT_H`, `MCC_CST_STORE`/`_SNAPSHOT`) are incidental and carry no config semantics.

## Migrated code-comment rationale (was NOTES.md §"Migrated from code comments")

Two batches (2026-07-06, 2026-07-09) of technical rationale stripped from source
comments per the "no code comments" project rule and relocated to NOTES.md. This
is per-function/per-subsystem design reasoning (backend, objfmt, arch quirks). It
is voluminous and tightly coupled to specific `src/` symbols — **review before
discarding**; it is the "why" behind non-obvious code that the code itself no
longer carries.
</content>
