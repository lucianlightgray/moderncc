# INVESTIGATIONS

Findings from codebase gap/conflict audits (parallel subagent sweeps). Each entry: a discovered gap, contradiction, or conflicting implementation, with file:line, a `[NEW]`/`[KNOWN]` label (KNOWN = already tracked under a `T-id`), and a `TASK:` pointer when a `TODO.md` exploration task has been minted. This file is the durable evidence log; `TODO.md` carries the one-line actionable state. Append-only in spirit; deepen entries as investigation continues.

Method: `docs/GOAL.md`-style parallel audits — one subagent per subsystem, each grepping for stub/bailout/asymmetry markers, reading to confirm, and cross-referencing `TODO.md`/`DETAILS.md` to separate NEW from KNOWN. Findings here are subagent-reported with file:line; only those marked **VERIFIED** were independently reproduced.

---

## Round 1 (2026-08-17) — core subsystems

### <a id="unsigned-const-fold-sdiv"></a>Unsigned `/`,`%` const-fold uses signed `gen_opic_sdiv` — **VERIFIED, FILED**
`ast_fold_eval` (`src/mccast.c:6988-6999`) folds unsigned division/modulo with signed magnitude division, disagreeing with the authoritative `gen_opic` (`src/mccgen.c:3740`). Deterministic wrong-code reproduced under `-fno-replay-fallback` at `-O1+` (masked by replay-fallback in default builds). Trigger: children-first fold of a unary-minus operand. `[NEW]` → already migrated to a full task. **TASK: T-mac-30014** (filed 2026-08-17, see `DETAILS.md#t-mac-30014-unsigned-const-fold-uses-signed-sdiv`).

### <a id="gpu-multidevice-teardown"></a>GPU multi-device teardown / lifetime cluster
The T-win-50022 multi-device "create-all" loop landed without its per-device teardown half (`DETAILS.md:47175` records it as held). `mcc_gpu_quiesce` (`src/mccgpu.c:2941`) is single-slot yet destroys the process-wide `VkInstance` (`:2961`) under still-live logical devices — the T-lin-10033 lifetime-bug class in multi-device form. `stranded` is a global counter (`:50-56`) with slot-local teardown (`:3077`) → one slot's fence timeout leaks all devices; `mcc_gpu_reopen` (`:2978`) is also slot-local. Fires only when `ndev>1`. `[NEW]`, root cause shared with `[KNOWN]` T-lin-10033/T-win-50022. **TASK: T-mac-30021.**

### <a id="spirv-f64-execmode"></a>SPIR-V f64 never pins SignedZero/Inf/NaN-preserve
Only `LocalSize` execution mode is emitted (`src/mccgpu.h:2496`); add/sub/mul run at hardware default though the RTX device advertises `shaderSignedZeroInfNanPreserveFloat64` (`DETAILS.md:31172`). NaN payload/sign and signed-zero results diverge from the CPU reference. Distinct from `[KNOWN]` unpinnable-denormal (T-lin-10061); this is pinnable-but-unpinned. `[NEW]`. Grouped under GPU-numeric investigation (**TASK: T-mac-30021**, numeric sub-item).

### <a id="gpu-metal-vs-vulkan-f64"></a>Metal (software f64) vs Vulkan (native f64) divergence
Metal uses full software IEEE-754 emulation (`src/mccgpu.h:1656`) and sets `mcc_gpu.f64=1` unconditionally (`src/mccgpu.c:523`) with no capability behind it; Vulkan gates on `shaderFloat64`. Same source → backend-specific f64 (esp. denormals). `[NEW]`.

### <a id="jit-lazy-vs-sync-kgc"></a>Lazy JIT counts "KGC-refused / kept-AOT" as "build failed" — root cause of T-lin-10029
`mccjit_lazy_build_masked` (`src/mccjit_embed.c:1119-1167`): when KGC verification refuses, `entry` stays NULL and the only trampoline fallback (`:1164`) fires only when KGC is *disabled*; lazy callers (`:1615`,`:1693`) then count `failed++` and "give up". The sync path (`:1035`) treats the identical KGC-refusal as normal kept-AOT. Same condition, opposite verdicts. `[NEW root-cause]` for `[KNOWN]` **T-lin-10029** — should be attached there, not a new task. Also: a usable variant is silently discarded when the KGC stub/baseline alloc fails (`:1149`,`:1156`).

### <a id="jit-rwx-mapjit"></a>x86_64 JIT pages RWX-forever; no `MAP_JIT`
x86_64 JIT primitives `mmap(...PROT_EXEC...)` and never re-protect (`src/mccjit_embed.c:937,1711,4212,6041`); arm64 uses W^X via `host_runmem_protect` but neither uses `MAP_JIT`/`pthread_jit_write_protect_np` (`src/mcchost.c:1624`). W^X hole on x86_64; x86_64-macOS embed-JIT under hardened runtime would fail (and is unmeasured — T-lin-10030 mac lane is arm64-only). `[NEW]`. Plus `mccjit_dispatch_entry` has no arm64 arm (`:6039`, returns fallback) and `mccjit_set_always_gpu` is one-way with no GPU-exists check (`:2108`, the T-lin-10082 seam).

### <a id="pp-float16-in-if"></a>Preprocessor: `_Float16` constants not rejected in `#if`
The pp-expr float guard `tok>=TOK_STR && tok<=TOK_CLDOUBLE` (`src/mccpp.c:2204`) misses `TOK_CFLOAT16=0xd5` (`src/mcc.h:1326`), so `#if 1.0f16` flows an illegal float operand into the integer-constant-expression evaluator. `[NEW]`. **TASK: T-mac-30022** (preprocessor `#if`/builtin semantics).

### <a id="pp-target-builtins-zero"></a>Preprocessor: 8 `__has_*`/`__is_target_*` builtins silently evaluate to 0
`pp_builtin_func` recognizes `__has_cpp_attribute`, `__has_warning`, `__is_target_arch/os/vendor/environment`, etc. (`src/mccpp.c:1993`), but only 5 are registered as real macros (`:5765`); the other 8 hit the fallback (`:2249`) returning 0 with no diagnostic. `#if __is_target_arch(x86_64)` is always false. `[NEW]`. **TASK: T-mac-30022.** Also: `#embed limit()/offset()` take only a single integer token, not a constant-expression (`:1708`).

### <a id="rir-vacuous-floors"></a>RIR: vacuous floors / ungated validity signals
`tools/rir-coverage.py`: banked lowerable sub-metric floors default to `0.0` so `measured+tol>=0.0` cannot fail (`:1950,1967`) — inconsistent with the arena floors' `is None`→skip. `control_neutral` is computed/printed but never gated (`:1221,1567`), so a non-neutral reference makes every "benign" verdict meaningless yet passing. Divergent bodies whose baseline `-run` exits nonzero are dropped as "unrunnable" and never miscompile-checked (`:1320`) — but many `tests/exec` use nonzero exit as their pass signal. `[NEW]`, same class as `[KNOWN]` T-lin-10043/10054. Folded into gate-vacuity **TASK: T-mac-30020.**

### <a id="codegen-arm64-vaarg-assert"></a>Codegen: live `assert(0)` in arm64 ELF `va_arg` for 16-byte/16-align non-HFA aggregate
`src/arch/arm64/arm64-gen.c:1721` — a conforming over-aligned small struct through `...` aborts the compiler (debug) or runs two untested trailing instructions (NDEBUG); the sibling stack-cursor block at `:1739` treats the same case as normal. `[NEW]`. (Minor; captured, not yet taskified.)

---

## Round 2 (2026-08-17) — object formats, debug, optimizer, runtime, types, gates

### <a id="macho-no-unwind"></a>Mach-O emits NO unwind/CFI information at all — CRITICAL
`src/objfmt/mccelf.c:90-92` forces `unwind_tables=0` for every non-ELF format; `mcc_eh_frame_start` (`src/mccdbg.c:1402`) then early-returns. PE substitutes `.pdata`/`.xdata`, but `mccmacho.c` has zero `__eh_frame`/`__compact_unwind` path (the `sk_uw_info` slot at `:1420` is `{0}`, never populated). macOS table-based unwinding (C++ EH, `_Unwind_*`, `cleanup`, crash reporters) has no data. Undocumented — the DETAILS parity matrix is ELF-vs-PE only. `[NEW]`. **TASK: T-mac-30015.**

### <a id="atomic-32bit-align"></a>`_Atomic` scalars under-aligned on i386/arm32 — 32-bit ABI break
`_Atomic` applied as a pure qualifier (`src/mccgen.c:9103`) with no alignment bump; `type_size` gives `long long`/`double` 4-byte align on i386 (`!MCC_TARGET_PE`) and arm (`!MCC_ARM_EABI`) (`:5711`). `_Alignof(_Atomic long long)`==4 vs gcc's 8; `struct{char c; _Atomic long long x;}` places `x` at offset 4 not 8 — silent interop miscompile vs any 32-bit system header, plus a tearing hazard. x86_64 unaffected. `[NEW]`, trivially confirmable via `_Alignof`. **TASK: T-mac-30016.**

### <a id="riscv64-backend-gaps"></a>riscv64 backend is second-class — inline-asm + reloc gaps
Inline-asm memory operands emit no base-register addressing (`src/arch/riscv64/riscv64-asm.c:2111`): `asm(::"m"(local))` expands to `lw rd, a0` instead of `lw rd, 0(a0)` — malformed. `constraint_priority` advertises `A`/`S` it can't satisfy (`:2255`, hard-fail mid-alloc). `code_reloc` returns -1 for compressed relocs its own `relocate()` handles (`riscv64-link.c:5`). `[NEW]` cluster. **TASK: T-mac-30017.** (Related: arm `K`/`L` dead in `arm-asm.c:2693`; arm32 no VFP constraint class.)

### <a id="optsearch-determinism"></a>Optimizer search: nondeterminism cluster
pthreads search pool races non-TLS emit globals (`ind/loc/rsym` `src/mccgen.c:245`; counters `mccast.c:18470`) — dev-gated off (`OPT_SEARCH_PTHREADS`). Memo/disk key (`ast_search_key_salt:17978`) omits result-changing flags (emit-size vs cost, walk order, predict) → a cache entry from one config served under another emits different objects. Disk-eviction comparator (`:18170`) returns 0 on ties → non-stable `qsort` → libc-dependent survivors. `[NEW]` mechanisms inside `[KNOWN]` T-lin-10045 (disk-cache determinism). Plus a degenerate slice "search" that can only pick greedy-largest (`:19083`). **TASK: T-mac-30018.**

### <a id="coff-reloc-emit"></a>COFF/reloc emit format-disagreement bugs
COFF object emit collapses `STB_WEAK`→strong `EXTERNAL` (`src/objfmt/mccpe.c:2259`; `WEAK_EXTERNAL` honored on read, never emitted, no COMDAT) → multiple-definition errors, lost overrides. i386 `DIR32NB` (RVA) decoded as absolute `R_386_32` (`:2020`). arm64 COFF `BRANCH26` CALL/JUMP gated on **host** not target (`:2035`) → a `B` cross-read on a non-Win host rewritten as `BL`, clobbering X30. `[NEW]`, adjacent to `[KNOWN]` T-win-50005/50006. **TASK: T-mac-30019.**

### <a id="debug-info-asymmetry"></a>Debug-info asymmetries (DWARF version, CodeView)
DWARF default version differs by platform: Mach-O=2, ELF/PE=5 (`src/mcc.h:2206`) → weaker `-g` on macOS. CodeView describes locals only FP-relative (`src/mccdbg.c:534`), no register defrange — optimized locals get no location. `-gcodeview` emits both CodeView AND DWARF into one object (`src/libmcc.c:2916`). `[NEW]`. (Grouped with Mach-O unwind investigation, **TASK: T-mac-30015** debug-info sub-items.)

### <a id="gate-vacuity"></a>Gate infrastructure vacuity beyond RIR
`tools/bitint-diff.py` — the per-target `_BitInt` ABI differential gate — has no floor on lines-compared (`:105`) AND is wired into nothing (no CMakeLists/ctest/must-run); it would catch the `_Atomic`/`_BitInt` ABI divergences but never runs. `defcheck` `def-verify` passes on an empty `.def` glob (`tools/defcheck.c:90`). `asm_reloc_suffix.cmake:81` jmp-equivalence tooth has no non-empty floor. `opt_determinism_mutate.cmake:21` swallows a `77` skip as a detection. `[NEW]`, same class as `[KNOWN]` T-lin-10043/10054/10048. **TASK: T-mac-30020.**

### <a id="type-c23-gaps"></a>Type-system / C23 gaps
`_BitInt(N>64)` and `__int128` bit-fields rejected (`src/mccgen.c:7521`) — C23 mandates the former, gcc/clang accept the latter. x86_64 SysV forces MEMORY class for any struct with a packed/unaligned field (`x86_64-gen.c:1298`) — may disagree with gcc's eightbyte classification at a boundary. `[NEW]`. (Captured; taskify next iteration or fold into ABI-conformance work.)

### <a id="runtime-intrinsics"></a>Runtime / intrinsics gaps
No `arm_neon.h`/`arm_acle.h` (full x86 SIMD suite present) — portable NEON code fails on arm64 (`runtime/include/`). half↔double conversion always double-rounds through float (`src/mccgen.c:5130`; no `__extendhfdf2`/`__truncdfhf2`). `stdckdint.h` `__int128` accumulator overflows for 128-bit operand types (`:51`). `int512.c` (24 helpers) orphaned on main; dead `__mcc_i256_neg/not/nonzero`. `[NEW]`. (Captured; taskify next iteration.)

---

## Pending taskification (captured, not yet minted)

Lower-severity or newly-arrived items awaiting a `TODO.md` task in a later loop iteration: codegen arm64 `va_arg` assert (`#codegen-arm64-vaarg-assert`); type/C23 bit-field gaps (`#type-c23-gaps`); runtime/intrinsics (`#runtime-intrinsics`); Metal-vs-Vulkan f64 (`#gpu-metal-vs-vulkan-f64`); JIT RWX/`MAP_JIT` (`#jit-rwx-mapjit`); debug-info asymmetries beyond Mach-O unwind (`#debug-info-asymmetry`). The JIT lazy-vs-sync root cause (`#jit-lazy-vs-sync-kgc`) should be attached to existing **T-lin-10029**, not minted separately.
