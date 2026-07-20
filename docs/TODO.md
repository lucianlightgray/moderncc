# TODO

## How to process
- Mark an item in progress, commit+push to main, then start it.
- Implement gated behind a new env (default OFF ⇒ byte-identical); validate the gated-ON path to the M8 bar; independently re-verify the pass fires + correctness vs gcc; commit; prune the item.
- Completed items are pruned entirely; detail lives in git history.
- M8 bar: ctest byte-identity · `-O6` differential vs gcc/clang · 3-stage self-host fixpoint · UBSan/ASan · cross-arch (i386/arm32/riscv64/arm64) · differential fuzz (x86_64 + native arm64) · shadow-IV zero-divergence. The differential fuzzer now runs on native x86_64 AND arm64 (`ubuntu-24.04-arm`); only the shadow-IV oracle stays x86-only.
- Cross-arch checks: use `cmake-cross/mcc-i386` and `cmake-cross/mcc-arm64` (the `cmake-qemu-*` builds emit native x86_64 and lack the optimizer).
- Always enable TRACE while working: configure with `-DMCC_CONFIG_TRACE=ON` (defines `MCC_CONFIG_TRACE=1`, activating the `MCC_TRACE(...)` branch markers) and run the compiler with `-v128` (the `MCC_LOG_TRACE` bit, `1<<7`). `MCC_TRACE` writes to **stderr** (`mcc_trace_at`/`mcc_trace_at_v` in `src/mcclog.h`), so every branch emits a `[TRACE] file:line func:` line on stderr — capture it with `2>trace.log` (or pipe `2>&1 | grep`). Use these to follow the execution path and to confirm a new pass's branches actually fire (and that unintended ones don't). The logging layer is `MCCState`-free: it reads a free-standing `mcc_log_verbose` global (declared in `mcclog.h`, defined in `mcchost.c`, mirrored from the active state in `mcc_enter_state`/reset in `mcc_exit_state`); the explicit-source variants are `mcc_log_enabled_v`/`mcc_logf_v`/`MCC_TRACE_V`/`MCC_DEBUG_V`, which take a verbose byte. Standalone `MCC_INTERNAL`/non-amalgamated compiles of `mccast.c`/`mccstats.c` (e.g. `asttool`) `#include "mcclog.h"` in their `#ifndef MCC_TRACE` fallback and can trace — build that target with `MCC_CONFIG_TRACE=1` and set `mcc_log_verbose` (those targets don't parse `-v`).
- Drive each change with `grep` iteratively: grep the token/env/function/`ST_FUNC` name across `src/` to find every site → edit → rebuild → run with `-v128 2>trace.log` and grep `trace.log` (plus the ctest/differential logs) to validate the intended branch fired and byte-identity/gcc-parity held → repeat until the M8 bar is green. Preserve the `MCC_TRACE("br\n")`/`MCC_TRACE("enter\n")` markers on new branches so `tracegate` stays satisfied.

## JIT runtime (§26)
- Fix `MCC_AST_SEARCH=1 -O4` segfault on `src/mcc.c` (JIT-independent; scale/emit-size fault in the in-process combo search).
- Build a bounded UB-sound compile-time loop interpreter in `ast_eval_slice`; fold no-escape-pure slices to constants into the submitted arena.
- In-arena sub-region splice for slice ⊊ body (C2b partial-slice reconcile; needs a node-identity-stable splice primitive).
- Split the KGC key into (code-hash, data-hash); track poison per (code+data); feed poison as a search input (K2).
- Explore/investigate cross-code data-shape reuse of KGC entries (K10). Once the key is split (K2), add a secondary index over the *data-shape* alone — a canonicalized descriptor of the slice's operands (element dtype/width, layout/stride/alignment, value-range/known-bits, ABI class, loop trip-shape) that is independent of the code-hash. On a shape hit for *different* code whose own specialization is missing or cold, speculatively try the shape-matched cached codegen as an extra candidate for this unrelated code: the intuition is that a specialization proven good for one function's data may fit another function whose data "has the same shape," even though the source differs. JIT path: it's cheap to be wrong — benchmark the borrowed candidate against the code's own baseline (reuse the MCC_JIT_BENCH_ROUNDS min-converge harness), keep it only if it wins, and poison per (code-hash, shape) if it loses or is unsound so it isn't retried. AOT path: no runtime bench, so gate acceptance on the deterministic cost/emit-size model (the AOT-static sink scorer below) plus static-range soundness. Hard parts: the shape-fit predicate + canonicalization (when do two slices genuinely share a shape?), a soundness guard so a borrowed body is UB-sound for the new data (verify or poison, never silently miscompile), and bounding the extra lookups/benches so the speculation stays net-positive. Fold shape-poison into the unified {good,bad,unknown} classified set (L7A).
- Switch-table cover strategy row (dense→jump-table, sparse→perfect-hash; L6A).
- Unify KGC+poison into one {good,bad,unknown} LFU-bounded classified set (L7A).
- Persist poison to the mmap'd cache under the opt-in persistent-cache flag (J1A).
- Parser-less re-emit-only engine slice + `-ffunction-sections`/`--gc-sections` for the ~800 KB Tier-B embed; reconcile CMake `libmcc-static.a` so plain `-lmcc` prefers it (L9B).
- General value-range narrowing fold (J7A; needs the P1 VLat consumer).
- Small struct-by-value marshalling; promote `classify_x86_64_arg` (x86_64-gen.c) to `ST_FUNC` (K4A).
- Wire QSBR reclamation into the swap path + L2A quiescent points (function-entry + loop back-edges); un-defer J5 (K9).
- D3B nop-padded patchable-prologue in-place code-patch + int3/trap + dual-map page-flip patch rows (J10).
- Non-ABI register calling-convention kernel with boundary ABI harness (K7); inline-vs-shim search axis (K8); float + memory-boundary slices.
- Flip `MCC_AST_JIT_EVAL_GATE` default-on after 3 clean self-host + fuzz soaks; extend the oracle to statement-level control flow (7A/M8).
- AOT-static sink scorer: deterministic cost/emit-size model + static-analysis ranges (AstVLat) + gain-ordered time-budgeted scheduling.
- K6/L8A data→code substitution via a synthetic `.init_array` ctor.

## JIT arm64
- arm64 mode-6 dispatch for object output: harden the GOT/ABS64 slot so the function symbol survives external link, then drop the `!ast_search_env` guard.
- arm64 `mccjit_make_kgc_stub_mixed` (mixed GP+FP thunk: `movz/movk`→x0..x5 + `ldr d0..d7`).
- arm64 in-place trampoline patch row.

## JIT Windows / i386-PE
- i386-PE KGC/FP(x87)/mixed stub tail (i386 JIT promotion; also the Linux i386 gap).
  - DONE (gated behind MCC_JIT_I386_STUBS, default OFF; default build byte-identical —
    x86_64/arm64 libmcc.c.obj proven byte-identical vs pristine, debug + -O2):
    the three i386 cdecl stub builders mccjit_make_kgc_stub_n/_fp/_mixed, the per-signature
    i386 mixed cdecl reconstruction thunk (published via the i386-only mccjit_i386_active_thunk),
    the i386 counter stub, trampoline, and dispatch-entry (src/mccjit_embed.c). Validated on a
    native i386-PE build (mstorsjo clang -m32, MCC_TARGET_ARCH=i386): jit/selftest-pool PASSES
    gate-ON (counter stub + async promote + KGC dispatch stub + jmp*slot), fparg FP(x87)
    differential-verify sub-tests PASS, and all three stub bodies (n/fp/mixed, faithful +
    divergent + x87-FP-return) pass a standalone machine-code harness (8/8). Gate-OFF: all i386
    stub builders return NULL and the selftests SKIP exactly as before. x86_64/arm64 unaffected
    (47/47 JIT selftests green; tracegate green).
  - FIXED (63bc90e9): the native mingw/i686 CI cell was FAILING 4 JIT selftests gate-OFF —
    jit/selftest-{lazy,eligibility,liverun} and search-live-lazy (reuses jit_selftest_lazy). They
    were missing the `#if defined(MCCJIT_I386) if(!mccjit_stub_tail_active())` skip guard that
    pool/fparg already had, so they ran the full promotion path and hit promoted=NULL / no
    dispatch slot / FP-arg refuse. Now they SKIP→PASS gate-OFF (so the "SKIP exactly as before"
    claim above is finally true for every i386 selftest); guard is i386-only so x86_64/arm64 stay
    byte-identical. Verified native i686 winlibs GCC 16.1.0: all four skip gate-OFF, body still
    runs gate-ON.
  - REMAINING to un-gate (needs the M8 bar):
    (1) i386 AST-faithful-replay desync for mixed long+double signatures ([ast-verify] desync) blocks
        the -run baking that would stash such functions, so the mixed stub can't yet promote through
        the normal pipeline (front-end/AST-recorder gap, upstream of the stub tail);
    (2) the mode-6 entry dispatcher in src/mccast.c emits hardcoded x86-64 opcodes (REX/rbp) with no
        i386 variant, so end-to-end -run JIT baking does not fire on i386 (fparg end-to-end sub-test);
    (3) differential vs gcc/clang + self-host on i386, then flip the default.
- arm64-PE runtime-JIT (frameless-leaf return corruption + RtlAddFunctionTable/icache); the native-fault class needs arm64-Windows HW (see NOTE below).
- MSVC-arm64 JIT-exec miscompile; needs arm64-Windows HW for the wild-jump fault (see NOTE below).
- NOTE (2026-07-20): wine-arm64 in Docker now boots + runs mcc arm64-PE exes on this x86_64 host (the `wineboot c0000135` is only the non-fatal ARM32/WoW `rundll32` stub, not the native ARM64 loader). So **codegen/logic**-class arm64-PE bugs ARE debuggable without arm64-Windows HW: build the gcc-cross `mcc-arm64-win32`, stage the win32 sysroot, run its emitted PE under wine (recipe validated the arm64-PE over-align change — `alignas_over.exe` → OK rc=0, plus switch/import-thunk/LLP64 corpus). Only the **native-fault / weak-memory** class genuinely needs HW — `RtlAddFunctionTable` unwind, icache coherence, the frameless-leaf return corruption, and MSVC-arm64's wild-jump all *spin-not-fault* under qemu/wine (x86-TSO masks them). So the two items above may be partially reproducible under wine for the codegen portion; the fault portion is not.
- Standalone `--embed-jit` blob for i386-PE / arm64-PE.

## AOT foundations
- Close the riscv64 Tier-3 self-host gap (6A; makes the M8 cross-arch gate real).
- Fix the emit-time value-axis framework full-state save/restore (promotion-plan arrays + `nocode_wanted` + register-allocator/`vtop` state); then enable the inline + promote search axes (3A).
- Fix the x86_64 mul-high register-allocation self-host miscompile, then flip `MCC_AST_DIVMAGIC` default-on; i386 64-bit divmagic runtime helper (4B).
- Flip `MCC_AST_SELECT`/`MCC_AST_ABS` default-on after soak; i386/arm 32-bit cmov (4B).
- Memo unification: migrate to `ComboMemo` + disk backing with a bulk-value compression mode; subsume the out-of-process superopt (5A/M2/M3).
- PR-C loop-IV monotonicity widening into `ast_vlat_context_at`, op-3/op-5 for-loops only; held until x86 fuzz soaks clean (P1/§32a).
- Predicate-vector 4th side-car index (P1).
- `context_in`/`context_out` memo-key consumer (P1).
- Descendant-indexed (DFS enter/exit) def/use extension for subtree-scoped write queries (P1).
- Flip `MCC_AST_VLAT` default-on; signed `/ %` (INT_MIN/−1 trap), `<<` value-count (P1/§29).
- Flip `MCC_AST_NARROW_ELIM` default-on: flow-sensitive facts + globals (§29).
- Flip `MCC_AST_ARGFWD` default-on; widen past single-use (§33c).
- Flip `MCC_AST_SPILL_SHARE` default-on; general per-value spill slots; riscv64 fixpoint (§36).
- Flip arm64 `opt_promote` default-on after tens-of-thousands-of-seeds soak; PR-3 callee-saved float pool v8–v15.
- riscv64 register promotion + qemu validation.
- Replace `ast_plan_promotion` heuristic with §36 coloring; fixpoint-gated + native arm64/riscv64.
- Verify Tier-4 inline (`ast/replay-inline-spec`) on riscv64, then ungate.

## Ungate campaign (flip every default-off feature on)
Endgame: once each gate's M8 soak is clean, flip it default-on and regenerate goldens. Beyond the per-gate flips in AOT foundations:
- Flip `MCC_AST_LICM_TEMP`, `MCC_AST_IVSR`, `MCC_AST_PRE`, `MCC_AST_SETHI_LEAF` default-on after soak.
- Resolve order-non-confluence, then flip `MCC_AST_REASSOC` default-on.
- Flip §27 `MCC_AST_INTERCHANGE`/`_FUSION`/`_TILE` default-on after soak.
- Wire `MCC_AST_COST` as the search/budget scorer, then flip it on.
- Flip the search family (`MCC_AST_SEARCH`/`_EMITSIZE`/`_EMITISO`/`_INLINE`/`_THREADS`/`_ORDERED`/`_ORDER`) on (needs 3A + §22 emit isolation).
- Flip `MCC_AST_PERFN_INPROC` on.
- Flip the runtime `MCC_AST_JIT` global gate + `_JIT_SPLICE`/`_JIT_DISPATCH` default-on after the full §26 validation-bar soak.

## Cross-arch parity (raise every arch to the x86_64 Tier-4 reference)
Goal: each arch matches x86_64 for self-host, promotion, cmov/csel, div-magic, JIT stub tail, ASan native-shadow, stack-protector, UBSan trap, TLS GD/LDM, Tier-4 replay-inline, over-align. Per-arch rollup (detail in the JIT/Sanitizers/AOT sections):
- riscv64: raise to Tier-4 — self-host, promotion, replay-inline, cmov, JIT stub tail, ASan native-shadow, stack-redzone.
- i386: UBSan trap, ASan native-shadow, stack-protector, TLS GD/LDM, 32-bit cmov, 64-bit div-magic helper, JIT stub tail, Tier-4 replay-inline.
- arm (armv7): raise from Tier-2 — self-host, cmov, UBSan trap, ASan native-shadow, stack-protector, JIT stub tail, replay-inline.
- arm64: TLS GD/LDM (currently LE only), mode-6 object-output dispatch, kgc-mixed stub, in-place patch row, flip `opt_promote` on.
- PE targets: UBSan handler ABI, asan-shadow, standalone embed-jit blob, arm64-PE runtime JIT. (over-align on i386-PE/arm64-PE validated + ungated — no compiler gate existed; codegen matches ELF oracle byte-for-byte modulo LLP64 type sizes; i386-PE runs the alignas_over test natively; arm64-PE alignas_over runs OK under wine-arm64 in Docker.)
- Provide a weak-memory-model validator for aarch64/armv7 (qemu is x86-TSO). The linux-arm64 CI fuzz **band** is DONE and live: the differential fuzzer (portable gcc+clang majority-vote oracle, not the x86-only shadow-IV oracle) runs on native `ubuntu-24.04-arm` per-push — the CMakeLists guard (~3777) registers `fuzz/smoke`+`fuzz/matrix-*` on arm64 and the ci.yml `linux`/arm64 cell's ctest runs them (the fuzz-exclude at ci.yml ~128 is macOS-x86/Rosetta-only). NOTE: an extra nightly `campaign` soak was hand-added to matrix.yml and then dropped by commit `4abbede5` ("regenerate matrix.yml") — **matrix.yml is a GENERATED file**, so re-adding the soak must go through the generator source (`tools/ci.c` / the ci-emit templates), not a hand-edit. Still open: re-land the nightly soak via the generator; an armv7 cell; a dedicated concurrency/litmus validator (qemu x86-TSO can't catch weak-memory reordering).

## Const-data (P2)
- Size-changing datacomp: `.init_array` decompress ctor (all 5 arches) + `__mcc_decompress` runtime (M6).
- TLS `tdata`→`tbss` + asan/bcheck zero-init `.bss` cases (M6z).
- §30 value-table dispatch (needs `.rodata` data-emission).

## CST (1A)
- slice-G multi-file `#include` stitching.
- `-g` from CST provenance (stands up the debugger + gdb suite); stop discarding the arena on the `--lsp`+`-g` path.
- Design `--hotreload` from reconciled CST snapshots.
- Revisit the Bind-marker (does CST supersede it?).
- Emit `CST_Error`/`CST_Missing` (error-recovery CST).

## Sanitizers
- Honor auto over-alignment under `-fsanitize=address` / `-b`.
- DONE: Validate + ungate `alignas` over-align on i386-PE / arm64-PE. No compiler gate ever existed (`STACK_OVERALIGN_MAX` is defined for every arch; the `overalign_indirect` path in `mccgen.c` `decl_initializer_alloc` is arch-neutral and the win64 `rax`/shadow-space split is `#if PE && X86_64` only). i386-PE: `and esp,-N` masks ESP down after the 16-align `alloca` (no shadow space on Win32) — verified by disasm, runs the full `alignas_over.c` natively (OK), matches i686 clang/msvcrt. arm64-PE: inline `and sp,sp,-N` — the `.text` is byte-identical to the native-arm64 (ELF) oracle except the 3 expected LLP64-vs-LP64 words (`sizeof(long)` 4-vs-8, `str w`/`str x`). Ungating was test-only (`alignas_over.c` `#if` now enables the runtime block for `__i386__`/`__aarch64__` on `_WIN32`); default build stays byte-identical.
- UBSan `-recover` mode — **x86_64 landed (gated OFF), ELF+PE validated**. New `do_sanitize_recover` gate (mcc.h) is set by `-fsanitize-recover=undefined|all` / `-fno-sanitize-trap=undefined|all` and cleared by `-fno-sanitize-recover=…` / `-fsanitize-trap=…` (libmcc.c ~2533, last-flag-wins). When ON, x86_64-gen.c `gen_ubsan_trap_or_call` swaps the trapping `ud2` for a PLT32 `call __ubsan_handle_<kind>_minimal` (kinds: add_overflow / divrem_overflow / shift_out_of_bounds / type_mismatch_v1 for nullptr); the minimal-ABI handlers (runtime/lib/mccubsan.c, auto-linked via mcc_add_support in mccpe.c + mccelf.c) log to stderr and RETURN. Default build is byte-identical (verified: `-fsanitize=undefined` object cmp'd clean-vs-patched). Test: `ubsan/recover_signed_add` (MODE=recover in run_ubsan.cmake, -O0..-O3). Remaining: (1) arm64/riscv64 emit the call (currently x86_64-only, else a warning keeps trap); (2) per-check recover sets (`-fsanitize-recover=shift` only) — today it's all-or-nothing per TU; (3) upgrade minimal→full ABI (emit `struct {SourceLocation,TypeDescriptor}` tables + operand values so the diagnostic names file:line and values) — large, needs `.rodata` descriptor emission; (4) sub/mul currently reuse the add_overflow symbol (cc-only dispatch can't tell +/-/* apart at the check site) — thread the op through for precise naming; (5) an atexit summary + exit-nonzero-on-any-UB option (the `mcc_ubsan_seen` flag is stubbed).
- clang-compatible `__ubsan_handle_*` diagnostic ABI — minimal-runtime variants DONE (see above). Full-runtime ABI (Data-pointer + ValueHandle args, SourceLocation/TypeDescriptor tables) is the remaining increment.
- **ASan on PE (x86_64-PE / i386-PE)** — native-shadow ASan is currently ELF/Mach-O-Linux-only and BROKEN if forced on PE: `-fasan-shadow` compiles+links on PE and emits the inline shadow probe (`movsbl 0x7fff8000(rax)`), but (a) the `mccasan.o` runtime is excluded on WIN32 (CMakeLists ~2353) so no shadow is mapped and no SIGILL handler is installed, and (b) stack/global redzone emission (`gen_asan_stack_prolog/epilog`, `.asan_lstack`, `__asan_globals`) is `#ifndef MCC_TARGET_PE` in x86_64-gen.c/mccgen.c. Net: an instrumented deref reads unmapped 0x7fff8000 and **segfaults on the probe itself** (verified: OOB `a[6]=1` under `-fasan-shadow` → raw segfault, not an ASan report). PE port plan: (1) new `runtime/win32/lib/mccasan_win32.c` that reserves shadow via `VirtualAlloc(MEM_RESERVE)` + commits lazily, installs an `AddVectoredExceptionHandler` for `EXCEPTION_ILLEGAL_INSTRUCTION` (reading rax/rdx/rcx from the `CONTEXT`, mirroring the POSIX `on_sigill`), and hooks malloc/free via the msvcrt CRT (or `HeapAlloc`); (2) resolve the fixed-offset collision — the Linux 0x7fff8000 offset + high-shadow ranges overlap Win64 user address space, so pick a PE-specific offset and MEM_RESERVE the shadow range up-front; (3) replace the ELF section-bracket symbols `__start/__stop___asan_globals` with a PE-compatible grouped-section (`.asan$a`/`.asan$z` COFF `$`-ordered sections) or an explicit registration table; drop the `#ifndef MCC_TARGET_PE` guards on the stack/global emitters and adapt reloc types (PC32 in-TU); (4) COFF section-name length: `.asan_lstack`/`__asan_globals` exceed 8 chars → use `/n`-longname or short names. Until then, `-fasan-shadow` should hard-error on PE instead of silently miscompiling. (The shipped `-fsanitize=address` on PE goes through the separate bcheck runtime, which works.)
- Port native-shadow ASan to riscv64; 39-bit-VA/bottom-up-mmap shadow-layout robustness; access-type READ/WRITE + region-relative locator; riscv64 stack-redzone.
- Decide compiler-rt-interop vs `libmccsan`.
- Explore `-fsanitize-coverage`, `-fsanitize=cfi`, `_FORTIFY_SOURCE`, freestanding/KASAN-style runtime sanitizer.

## Tests / infra
- String-literal `L.N`/anon-symbol object-layout sensitivity (3 exec files excluded from object-diff) — ROOT-CAUSED: string literals are named `L.<v − SYM_FIRST_ANOM>` (`mccpp.c` ~691) off the global `anon_sym` counter, which is *shared* with anonymous structs/unions/compound-literals and struct tags (`mccgen.c` ~1229/4488/4704/4769). So a literal's symbol name shifts with unrelated *preceding* anon consumers and with any pass that allocates anon symbols in a different order/count — verified: `"hello"` is `L.2` compiled alone but `L.5` when a single `struct{union{int a;int b;};}` precedes it; same-source output is byte-deterministic, so the sensitivity is to code structure / opt-order / cross-compiler naming (gcc uses `.LC0`), not run-to-run nondeterminism. Fix (deferred, large): name rodata literals from a literal-local stable key (content hash or per-`.rodata` emission index) independent of `anon_sym`, then re-include the 3 files — but this renames every rodata-literal symbol ⇒ full object-golden regeneration + M8 soak.
- Implement i386 `-fPIC` TLS codegen (GD/LDM). Stopgap landed: `gen_gotpcrel` (i386-gen.c) now hard-errors on a TLS symbol instead of emitting the old `R_386_GOT32X` that made the linked PIE **segfault** — so no more silent miscompile (found via linux/386 Docker as the 32-bit sysroot; non-PIC `R_386_TLS_LE` path is fine and runs correctly). Remaining: emit real global-dynamic for globals and local-dynamic for statics under `-fPIC`, then drop the error. Implementation spec (mcc's own linker in i386-link.c already relaxes these *exact* GNU byte patterns to LE, so codegen just has to emit them): GD = `8d 04 1d <4B R_386_TLS_GD>  e8 <4B R_386_PLT32 to ___tls_get_addr>` (`lea 0(,%ebx,1),%eax; call ___tls_get_addr@plt`; the GD reloc sits at the lea's disp32, i.e. code start +3; linker's `expect[]` begins at that offset −3). LDM = `8d 83 <4B R_386_TLS_LDM>  e8 <4B R_386_PLT32>` (`lea 0(%ebx),%eax; call …`; LDM reloc at disp32 = start +2), then per-access `R_386_TLS_LDO_32` to add the var's dtp-offset. Both need `%ebx`=GOT base first (`get_pc_thunk(MCC_TREG_EBX,…)`). After `___tls_get_addr`, `%eax` holds the var address; the TLS-access sites in i386-gen.c (load LVAL ~186, addr ~249, lea ~338) then dereference/offset from it. Validate with the `i386-tls-docker` ctest (flip its `-fPIC` expectation from must-error to must-run once implemented).
- i386-fastcall-abi off-i386 residual: the ABI is now validated by the docker-gated `i386-fastcall-abi-docker` ctest (added inside the `TARGET mcc-i386` block; runs `tools/i386fastcall-docker.sh`, skips 77 when docker/mcc-i386 absent) — passes in `cmake-cross`. Optional leftover: teach the native `suite_i386fastcall` (tools/mccharness.c) a Docker execution backend so the mingw/`-m32` harness path also works off-i386, and confirm the project CI actually provides `linux/386` docker (else the new test just skips there).
- Audit `mcc_skip_test` per-triple ungating (i386-linux, aarch64/armv7-linux).
- diff3 differential silently skips its whole suite when auto-detect finds <2 *distinct* reference compilers — e.g. an mstorsjo-mingw build tree where `gcc.exe` == `clang.exe` (both LLVM), which the runner de-dups by `--version` to 1 ref. All presets now honor `MCC_DIFF3_GCC`/`MCC_DIFF3_CLANG` env vars (hoisted to `_base`), so exporting those two paths enables diff3 for any preset (empty env ⇒ auto-detect, unchanged); or `-DMCC_DIFF3_GCC=…`/`-DMCC_DIFF3_CLANG=…` per build-dir, or `-DMCC_DIFF3_EXTRA_REFS=label=path`. On this host GCC 15.2 (CLion mingw) + clang 22.1 (mstorsjo) makes 249 diff3 tests fire (100% pass). Further durable options: teach `mcc_find_gnu_gcc` to also probe the CLion mingw path, or add a native clang-toolchain target so a self-contained clang is always available as the 2nd family. (CLion default build dirs like `cmake-build-debug` don't use presets, so set the env vars in the IDE toolchain or a git-ignored `CMakeUserPresets.json`.)
- Skip-audit + max-coverage provisioning (anti-false-green): many suites `mcc_skip_test`/`SKIP_RETURN_CODE 77` for *installable or configurable* reasons rather than genuine host incapability, and those skips are **silent** — a suite can no-op while `ctest`/CI stays green (e.g. diff3 with <2 distinct refs; the Windows reference-install step is `continue-on-error` and `MCC_DIFF3_READY` never fails the job). Build a per-host capability audit: enumerate every skip gate, classify each as legitimately HW/arch-gated (arm64-Windows HW, native-only) vs. provisionable-on-this-host, and document the exact install/config to un-skip each — diff3 needs 2 *distinct* reference compilers (`MCC_DIFF3_GCC`/`_CLANG`, or `MCC_DIFF3_EXTRA_REFS`); the structural cli/diff3 cases need binutils `nm`/`readelf` (dir cache var, ~CMakeLists:3615); diff3/preprocess on Windows need a POSIX shell (`MCC_TEST_SH`); i386-fastcall/`-fPIC`-TLS need `linux/386` docker; cross-arch exec needs qemu-user + sysroots (and clears `MCC_EMULATOR` gating where native). Then add a ctest/CI assertion that each expected suite actually *ran* ≥N tests on platforms that support it (parse `ctest-junit.xml` run-vs-skipped counts), failing the cell on a silent no-op so the differential/M8 gates can't false-green.
- Normalize CMake incrementally (autodetect + fold `.cmake` files, verifiable target).
- Cut CI wall-clock: gate `bench`, shard macOS ctest, prune matrix re-runs, profile Windows jobs.
- Promote/inline replay-fidelity gap set (763 baseline gaps: 721 desync / 33 unfaithful / 9 stackresidue) — ROOT-CAUSED to a handful of AST-recorder bail sites in `mccast.c`, each an unmodeled construct that diverges the recorder's value-stack/AST model from real codegen. The largest *named* cluster — the glibc `<stdio.h>` inline family `putchar_unlocked`/`putc_unlocked`/`fputc_unlocked`, recurring across every `arch/*` baseline file — is `ast_hook_vstore` bailing at ~2409 on a store inside a ternary/`||` branch (`ast_tern_top > 0 || ast_lor_top > 0`), because `__putc_unlocked_body` expands to `cond ? (*p++ = c) : __overflow(...)` (verified: minimal `putchar_unlocked(65)` → `desync:2409`). Other hot bail sites: ~1452 (`ast_hook_vpush` value-stack-depth mismatch `ast_vn != rel-1` — a *downstream* detector; some earlier unmodeled push/pop already diverged the model), ~2135 (member/bitfield aggregate store), ~2294 (`ast_hook_call_begin` nested in-call). Fix (deferred, large, risky): model conditional stores under `?:`/`||` (the 2409 bail is a deliberate safety cutoff — correct store placement under the conditional is the hard part) plus the upstream constructs feeding 1452; needs the self-host + fuzz soak, and the AST substrate is desync-fragile (see arg-spill/frame-desync history).
- Link-time/ABI differential fuzzer (mix mcc `.o` with gcc `.o`).
- Coverage-guided generation (gcov/Intel-PT into `tests/fuzz/gen.h`).
- EMI mutation (Orion/Athena/Hermes) targeting optimizer miscompiles.
- Self-host differential (compile `src/mcc.c` mcc vs gcc, diff over corpus).

## Search vocabulary / strategy long tail
- §27 loop-nest precision: symbolic bounds, fewer non-affine bail-outs, asttool dep suite (blocked by `MCC_INTERNAL`).
- §27 true 2-D loop tiling (strip both loops).
- §29 range/known-bits lattice remaining (PR-3+).
- §31 adaptive beam width; per-function scoping.
- §24 hot-slice budget allocation (needs §22 emit isolation).
- §25 `-g` hot-value cache.
- §33b post-graft window dataflow decision (splice-then-reanalyze vs two-pass).
- §33d seam peephole window; §33e window-level cache key; §25 scoring of the de-spill delta.
- §32c speculative arm insertion (revisit only with 3-stage self-host gate).
- §32a widening/fixpoint dataflow for cross-iteration value merging.
- §30 `switch`-arm detection form.
- §23 inline budgets as a §22 search value-axis (needs 3A); more param shapes; inline cross-TU static callees; heuristic non-static inlining.
- §22 arena-mutating pass-subset re-emit axis; promotion re-emit axis (needs 3A).
- §28 rewrite-rule IR; §28 instruction-level superoptimization.
- V-strategy variations (bfold/ident/narrow/cprop/cse/licm/dse/sccp/jt/bf/sethi/tco) — widen search vocabulary; see git history for per-row holds.
- Build the `H_e` epoch hash (invertible slot-keyed edit patch).
- `.rodata` data-emission project (prerequisite for §30).
- Design cross-TU LTO; separate `-O2`/`-O3` SSA drivers; time-budgeted engine; dependency-ordered `-O1`; broader template library.
- M7 formula-family unification; M7b graduate the disk search-memo into compiled-in strategies.
- FLOAT: emit-size scoring under the tick scheduler + JIT-runtime scoring; C11-thread pool with per-context state (needs §22 emit isolation).
- Revisit: `k` always-inline depth policy; size-gated outline; store factoring; template DSL past ~30 templates; per-function `-O1`; PP-as-executable-C JIT; human-friendly diagnostics vs terminal geometry.
