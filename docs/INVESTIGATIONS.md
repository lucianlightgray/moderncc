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

## Round 3 (2026-08-17) — concurrency, memory/lifetime

### <a id="objreader-bounds"></a>Object-file readers: unchecked bounds on untrusted input — SECURITY
The linker's object/archive/dylib readers trust attacker-controllable counts/offsets:
- COFF reloc offset unchecked → **OOB heap write**: `roff = rl->VirtualAddress + smap[i+1].offset` flows into `coff_map_reloc`'s `memcpy(fld,&v,4)` with no bound vs section size (`src/objfmt/mccpe.c:2616-2621`); the Mach-O reader guards the identical op (`mccmacho.c:2781`), COFF does not.
- Archive symbol count `nsyms` from the `/`/`SYM64/` member walked with no bound vs member size → **OOB read** (`src/objfmt/mccelf.c:3815-3833`); BSD variant validates, GNU/PE does not.
- Mach-O dylib exports: `iextdef`/`nextdef` index `symtab[]`/`strtab` unbounded; `nsyms==0` derefs NULL (`mccmacho.c:3296-3308`).
- DLL export name count DWORD→int truncation under-allocates, then reads `namep[i]` (`mccpe.c:1717-1732`).
- Unbounded Mach-O load-command walk in the dll path (`cmdsize==0`→infinite loop, `mccmacho.c:3290`); silent short-read leaves uninitialized parse buffers (`mccelf.c:3370`).
`[NEW]` cluster; distinct from the reloc-*type* task T-mac-30019. Reachable by feeding a crafted `.o`/`.a`/dylib to the linker. **TASK: T-mac-30023.**

### <a id="tls-runslab-bounds"></a>TLS run-slab bounds guard vs. copy mismatch (`-run`)
`src/mccrun.c:471` guards `total` (Σ section `data_offset`) against `mcc_jit_tls_slab[MCC_JIT_TLS_MAX]`, but the seed copy uses `seed_len = max(sh_addr-base + data_offset)` (`:540-543,613-616`); inter-section alignment padding makes `seed_len > total`, so the guard passes while `memcpy(slab, seed, seed_len)` (`:499,643`, per worker-thread reseed) writes past the slab. Trigger: `-run` on Linux a program with ≥2 non-NOBITS TLS sections + alignment gaps. Latent (usual case is single `.tdata`). `[NEW]`, distinct from KNOWN tls_threads tpoff red. **TASK: T-mac-30024.**

### <a id="libmcc-reentrancy"></a>Multithreaded libmcc / JIT reentrancy
Default opt-search is fork-isolated (safe); the racy opt-search-pthreads path is dev-gated off (`[KNOWN]` DETAILS:19797, "60/60 SIGSEGV"). Under-documented gap: the per-`MccjitCounterState` lock (`src/mccjit_embed.c:1191`) does NOT cover the process-global JIT scratch it relies on (`mccjit_last_*`, `:361-367`) — two user threads promoting different slots via the sync fallback (`:1665-1670`) race them. `MCC_GPU_LOCK` compiles to `((void)0)` on Windows (`src/mccgpu.c:35`, `[KNOWN]` L7/DETAILS:35049) — GPU state unsynchronized there. Diag ring buffer publishes index before filling slot (`:266-268`, dev-gated `MCC_JIT_CRASH_DIAG`, cosmetic). Reentrancy hazard for a multithreaded libmcc consumer. `[NEW]`/`[KNOWN]`-mix. **TASK: T-mac-30025.**

### <a id="foldeval-diag-divergence"></a>`ast_fold_eval` diverges from the gen evaluator on diagnostics too — same root as T-mac-30014
The AST template folder silently folds cases the authoritative gen path diagnoses:
- **Signed integer overflow** in constant `+`/`-`/`*`: `ast_fold_eval` computes raw `l1+l2` etc. (`src/mccast.c:6976-6987`) and rewrites to a `Literal`; gen emits `mcc_pedantic("integer overflow in constant expression")` (`src/mccgen.c:3701`), fatal under `-pedantic-errors` (`:553`). Same source: rejected on gen path, silently wrapped on fold path.
- **Shift count negative or ≥ width**: `ast_fold_eval` just masks `l2 & 31/63` (`mccast.c:7000-7005`); gen warns (`mccgen.c:3669-3671`). Diagnostic lost when the fold fires first; value also diverges under the T-mac-30014 gating (`-fno-replay-fallback`/-O1+).
`[NEW]`, same root function/masking as **T-mac-30014** — the unsigned-div value bug is one instance of a general "ast_fold_eval must match gen on value AND diagnostic" problem; fix together. **TASK: T-mac-30026.**

### <a id="pp-directive-guards"></a>Preprocessor directive operand-guard holes
- `#undef` accepts a non-identifier or empty name with no diagnostic (`src/mccpp.c:2765` guards only `defined`/`__VA_ARGS__`); `#define` (`:2320`) rejects both `< TOK_IDENT` and those names.
- `#ifdef`/`#ifndef`/`#elifdef`/`#elifndef` accept `defined` and `__VA_ARGS__` as operand (`:2826,:2882` guard only `< TOK_IDENT`) — silently evaluated as "not defined".
- `__VA_OPT__` in an object-like/non-variadic macro is only a warning, and that path then skips the structural `(`/`##`/unterminated checks (`:2408-2416`) yet still appends the token → bogus definition; every other `__VA_OPT__` misuse is fatal.
- Token-paste producing >1 pp-token uses `mcc_error_noabort` then proceeds to emit each wrong sub-token (`:4946`), vs fatal for the comment-start case (`:4933`).
`[NEW]` cluster of complementary guard holes / severity inconsistencies. **TASK: T-mac-30026** (diagnostic-consistency; adjacent to T-mac-30022).

### <a id="embed-jit-nobake-notice"></a>`--embed-jit` no-bake notice bypasses the warning machinery — [KNOWN T-lin-10028]
`src/mccjit_embed.c:2189` hand-prints via raw `fprintf`, so it is immune to `-w` AND cannot be promoted by `-Werror`; the engine-less binary still exits 0. `[KNOWN]`.

### Reentry-state — verified ROBUST (negative result)
The error-handling audit confirmed `stk_data_floor`/`error_jmp_buf` are saved/restored around every temporary raise, `error1` unwinds to the floor, and `preprocess_start` re-initializes include/ifdef/pack stacks at the next TU — so a top-level error longjmp does not corrupt the next compilation. The multithreaded JIT-scratch race (`#libmcc-reentrancy`) remains valid; single-threaded reentry is clean.

---

## Round 4 (2026-08-17) — driver/CLI, linker resolution, cross-target/ABI

### <a id="driver-cli-semantics"></a>Driver / CLI option semantics
- `-imacros <file>` is a byte-for-byte alias of `-include` (`src/libmcc.c:3070`) — leaks the file's declarations/code into every TU instead of absorbing only its macros; wrong, silent, undocumented.
- `-fstack-protector` help (`src/mcc.c:150`) understates targets — code enables canaries on arm64-ELF/i386/arm/riscv64 too (`libmcc.c:3143`); also omits `-fstack-protector-strong`.
- Plain `char` is unsigned on ALL targets incl. x86_64/i386 (`libmcc.c:1226`, defines `__CHAR_UNSIGNED__`) — deviates from the SysV/GCC signed-char norm; undocumented ABI/behavior surprise.
- Mach-O linker options `-flat_namespace`/`-two_levelnamespace`/`-undefined <treatment>` accepted and silently ignored (`libmcc.c:3430-3435`) — `-undefined dynamic_lookup` is a no-op.
- Parsed-but-undocumented: `--jit-threads`, `-print-isa`, `-iquote`, `-idirafter`, `-imacros`.
`[NEW]`. **TASK: T-mac-30027.** (KNOWN: `--jit-threads` override + `--jit-conservative` help = T-lin-10393.)

### <a id="linker-resolution"></a>Linker resolution / archive semantics divergences
`mccelf.c` is the shared resolution core; divergences are in the format wrappers:
- Mach-O silently ignores `-e`/`--entry` — hardcodes `main` for LC_MAIN (`mccmacho.c:2551`); ELF/PE honor `elf_entryname`. Custom entry dropped without warning.
- Mach-O undefined-symbol diagnosis stricter than ELF/PE: errors any non-weak undef not in dynsymtab regardless of reloc reference (`mccmacho.c:981`), vs the shared `relocate_syms` erroring only reloc-referenced undefs.
- Library search order deviates from GNU ld: pattern-outer/path-inner (`libmcc.c:1854`) → `libfoo.so` searched in all `-L` dirs before `libfoo.a` in any; a `.a` in an earlier dir loses to a `.so` in a later dir.
- No `SHF_MERGE`/`SHF_STRINGS` dedup in any format (size only).
- `.bss`==`SHN_COMMON` conflation in `set_elf_sym` (`mccelf.c:647`) — a genuine `.bss` global treated as tentative, silently overridable by a later strong def.
`[NEW]`. **TASK: T-mac-30028.** (KNOWN: PE-only alacarte precedence = T-win-50021; COFF weak-external aux/COMDAT selection = DETAILS:7683.)

### <a id="longdouble-cross-target"></a>arm64/PE `long double` ABI + host-vs-target + cross-target predefines
- **arm64 `long double` is 16 bytes on every OS** (`src/arch/arm64/arm64-gen.h:27`, no macho/PE guard) — wrong for Apple & Windows arm64 where it is 8 (`==double`). `type_size`/`__SIZEOF_LONG_DOUBLE__` report 16; ABI-mismatched vs system libc (`%Lf`, `strtold`, struct layout). Self-consistent within mcc code (why the mac-arm64 suite stays green). `[NEW, HIGH]`. Same family as `[KNOWN]` x86_64-PE `long double`=16 (T-lin-10394/T-win-50003).
- Host-vs-target: `ast_sv_hi()` (`mccast.c:2735`) extracts a *target* `VT_LDOUBLE` constant's hi word via *host* `sizeof(long double)`/`LDBL_MANT_DIG` → returns 0 on an 8-byte-ld host targeting 80-bit. Long-double constant repack (`mccgen.c:16833-16863`) has an unhandled host×target `#else` that silently emits an uninitialized constant.
- `__SIZEOF_WINT_T__`==4 (`mccpp.c:5612`) contradicts `__WINT_TYPE__`==`unsigned short` (2) on PE.
- Frozen/missing arch feature predefines: arm hardcodes `__ARM_ARCH_4__`, arm64 emits no `__ARM_*`, i386 gets no SSE macros (`mccpp.c:5530-5563`) → feature-test `#if` silently takes the wrong path.
- Non-Darwin→macho cross build omits macOS SDK search paths (`libmcc.c:1327`), silent.
`[NEW]`. **TASK: T-mac-30029.**

---

## Round 5 (2026-08-17) — self-host determinism, standard headers, numeric lexing

### <a id="longdouble-selfhost-determinism"></a>Long-double self-host determinism hole
mcc const-folds `long double` using HOST long-double arithmetic (`gen_opif`, `src/mccgen.c:4019`), and the self-host determinism design explicitly exempts long double from bit-exactness (DETAILS:34715 row E3: "long double escapes … claim it doesn't affect selfhost-fixpoint is probably false"). Compounding: mcc's OWN preprocessor `parse_number` (`src/mccpp.c:3379,3507`) runs every float literal in any source — incl. mcc's ~100K lines — through 80-bit long-double scaling constants (tightest self-reference loop). `LDOUBLE_WORDS` (`src/mcc.h:230`) derives from host `sizeof(long double)`, so on Apple-arm64 (where T-mac-30029 mis-sizes ld to 16) a self-built mcc drifts payload width stage-0→stage-1. No gate sees a STABLE divergence: `selfhost-fixpoint.py` compares stage2==stage3 (same compiler); `selfhost-output-parity.py` runs only `-O2`/same-host; `selftest.c`/`combo_selftest.c` exercise only integer/pointer/codec code (vacuous w.r.t. float). `[NEW]` self-host framing of `[KNOWN]` T-mac-30029/T-lin-10394. **TASK: T-mac-30030.**

### <a id="header-conformance"></a>Bundled standard-header conformance
`runtime/include` headers hardcode values contradicting the compiler's per-target predefines:
- `stdint.h:100` `WCHAR_MIN`/`WCHAR_MAX` hardcoded signed-32 (`INT32_MIN/MAX`) — wrong on ARM/aarch64 Linux (`wchar_t` unsigned, `__WCHAR_MAX__ 0xffffffffU`) and Windows (16-bit). Fix like GCC: `#define WCHAR_MAX __WCHAR_MAX__`.
- `mccdefs.h:236` `__WCHAR_MAX__`/`MIN__` signed-32 on Windows though Windows `wchar_t` is `unsigned short`; the `#if __linux__ && (arm||aarch64)` guard drops Windows into the wrong else-branch.
- `stddef.h:34` `unreachable()` defined as `((void)0)` instead of `__builtin_unreachable()` (the builtin exists, `mccgen.c:13288`) — silently defeats the C23 contract.
- `limits.h`/`stdint.h` missing ALL C23 `*_WIDTH` macros + `BOOL_MAX` + `__STDC_VERSION_*_H__`; `uchar.h` missing `char8_t`/`mbrtoc8`/`c8rtomb` (though `__CHAR8_TYPE__` is predefined).
- `stdint.h:106` `WINT_MAX` 32-bit vs `__WINT_TYPE__ unsigned short` on PE (KNOWN-adjacent T-mac-30029); `intmax_t` typedef `long long` vs `__INTMAX_TYPE__ long` on LP64; `float.h:28` `FLT_EVAL_METHOD` hardcoded 2 for i386 vs `__FLT_EVAL_METHOD__ 0` under SSE2; `threads.h:10` unconditionally includes `<pthread.h>` (fails on PE); `stdatomic.h:197` fences/flags declared `extern` not mapped to builtins (link-fail risk).
`[NEW]`. **TASK: T-mac-30031.**

### <a id="numeric-literal-lexing"></a>Numeric / float literal lexing
- **`wb`/`uwb` `_BitInt` literal truncated to 128 bits** (`src/mccpp.c:3727-3729`): stores only `tokc.q.lo/hi`, zeroes `q.w2/w3`, never writes `q.w4-w7` — but `__BITINT_MAXWIDTH__`=512 (`:5619`), cap allows N≤512 (`:3722`), `_BitInt(256)` is tested. Any `_BitInt` literal >2^128 gets a silently truncated value; codegen reads stale `q.w4-w7` from the reused global `tokc` for widths >256. The sibling `i256` path (`:3765`) stores all four words. `[NEW, HIGH]` — timely after the wideint-unify merge.
- **Triple width drift**: predefine 512, `wb` accumulator `w[8]`=256-bit (`:3684`), store=128 → a 257–512-bit literal overflows the accumulator and is rejected with a misleading "exceeds 512-bit maximum" (`:3722`); representable 512-bit literals can't be formed. `[NEW]`.
- `__bf16` has no literal suffix and no constant path (`mccgen.c:5337` errors "not a load-time constant") despite `VT_BF16`/`bf16_round` existing — asymmetric with `_Float16`. `[NEW]`.
- Decimal literal in [2^63,2^64) made `unsigned long long` + spurious overflow warning (`:3780`), against C rank rules; value >ULLONG wraps mod 2^64 with only a warning (`:3618`). `[NEW, LOW]`.
- long-double/`_Float16` literals stored/rounded via host `long double`/`strtold` (`:3529,3587`) — `[KNOWN]` T-mac-30029 / self-host family (`#longdouble-selfhost-determinism`).
`[NEW]`. **TASK: T-mac-30032.** (Verified correct: C23 digit separators, `0b`/`0o`, leading-zero octal incl. rejecting `089`, suffix sniffing, hex/oct/bin type assignment.)

---

## Round 6 (2026-08-17) — assembler/inline-asm, disassembler round-trip, debug-info, type/ABI, CST/AST/RIR replay, diagnostics

Six parallel subsystem audits (mac-arm64). Every finding cites `file:line`; all deduped against rounds 1-5 + `DETAILS.md`/`TODO.md`/`ARCHIVED.md`. Tasks minted: **T-mac-30033..30038**.

### <a id="r6-inline-asm-regnum"></a>Assembler / inline-asm
- **[HIGH] arm64 inline-asm register numbering collides with codegen numbering → miscompile.** The asm constraint layer uses PHYSICAL numbering (x0..x30 = 0..30, SP = 31; `src/arch/arm64/arm64-asm.c:29-60,231-232,2191-2196`) but `asm_gen_code` calls `load(op->reg,…)`/`store(op->reg,…)` with NO translation (`arm64-asm.c:2099,2122`), while codegen numbering (`src/arch/arm64/arm64-gen.h:6-10`) has `R30 = 19` and `F(x) = x+20` (v0-v7 = indices 20-27). They coincide only for x0-x18; at 19+ they diverge — physical x19 → codegen LR, physical x20-x27 → codegen v0-v7. `register long v asm("x20")` stores an unrelated FP reg; pushing the `'r'` allocator past x18 targets/corrupts LR. Exactly the latent bug `DETAILS.md:14699` predicted after the riscv64 fix; arm32 is protected by `arm_asm_treg()` (`src/arch/arm/arm-asm.c:2465-2475`), arm64 has no shim. Also `reg_saved[]={19..30}` (`:2056-2058`) is physical but gated on the codegen-indexed `regs_allocated[op->reg]`, spuriously save/restoring x20-x27 on float operands.
- **[MED-HIGH] `.section name,"flags",@type` drops the type and force-sets `SHF_ALLOC`.** `asm_parse_directive` starts `flags=SHF_ALLOC` (`src/mccasm.c:806`), parses only `w`/`x`, and parses-then-discards the `@type`/`%type` operand (`:830-835`); `find_section` always makes `SHT_PROGBITS|SHF_ALLOC`. So `.section .tb,"aw",@nobits` → PROGBITS (skip/zero allocate real file bytes, loses BSS/TLS-BSS), and every `.section` gets `SHF_ALLOC` forced on (non-alloc metadata/debug sections wrongly become loadable). Silent.
- **[MED] arm64 cannot clobber v8-v31; models only 8 SIMD regs for asm.** `ARM64_FREG_BASE/LAST=20/27`, `arm64_parse_regvar` only knows v0-v7 (`src/arch/arm64/arm64-asm.c:62-63,233-242`); `asm(:::"v8")` → hard "invalid clobber register" though v8-v15 are callee-saved; the `'x'`/`'y'` constraint distinction collapses to the same 8-reg file.
- **[MED] x86_64 `'A'` constraint reserves only RAX, not RDX:RAX** (`src/arch/i386/i386-asm.c:1280-1285`, `is_llong=0`); an `"=A"` on a 128-bit value leaves RDX unclaimed. (i386 branch is correct.)
- **TASK: T-mac-30033.**

### <a id="r6-disasm-roundtrip"></a>Disassembler ↔ encoder round-trip
- **[HIGH] x86_64 segment-override prefix (`%fs`/`%gs`) parsed then dropped.** `decode()` records `d->seg` but `modrm()` never uses it (`src/arch/x86_64/x86_64-dis.c:284-289` vs `:94-183`); i386 threads seg correctly (`src/arch/i386/i386-dis.c:87,110,146,152`). Every `__thread` access emits `%fs:`-relative; disasm drops it → the `-S`/reassembled form is an absolute load from address 0 (crash), semantics changed. Repro: `__thread int tv; int gt(void){return tv;}` `-S`.
- **[HIGH] Undecoded opcodes under-consume the ModRM byte → instruction-stream desync.** `bsf`/`bsr` (`0F BC/BD`, emitted for `__builtin_ctz/clz/ffs`) hit the `raw0f` `.byte` fallback that returns a length covering only `0F`+opcode, not ModRM/SIB/disp/imm (`src/arch/x86_64/x86_64-dis.c:1181-1184`; same flaw in the 1-byte default `:814-816` and both i386 fallbacks `789-791,897-899`). The next insn starts mid-stream; the epilogue vanishes and `-S` no longer reassembles. Repro: `int f(unsigned x){return __builtin_ctz(x);}` `-S`.
- **[MED] x86_64 `movd`/`movq` register operand decoded 16-bit under the mandatory `0x66`.** `vsize()` treats the mandatory `0x66` as an operand-size override (`:204-210`) → GPR printed 16-bit (`:1070-1086`): `movd %eax,%xmm0` → `movd %ax,%xmm0`. Latent for C codegen (memory-operand/REX.W forms only), real for the inline-assembler path + disassembling arbitrary objects.
- **[LOW-MED] arm64 HFA-passing `LD1`/`ST1` not decoded.** The `LD1 0x4c40…` fails the mask test at `src/arch/arm64/arm64-dis.c:872` → bare `.long`; `ST1` matches but prints `{…}` losing the reg-list/element size. Round-trip preserved (raw word) but disasm uninformative for any HFA-by-value call.
- **TASK: T-mac-30034.**

### <a id="r6-debug-info"></a>Debug-info emission vs codegen
- **[HIGH] arm64 synthetic FDE hardcodes a fixed prologue shape → wrong CFA for variadic / many-arg functions.** `mcc_debug_frame_end` (`src/mccdbg.c:1591-1606`) assumes the sp-sub lands at insn ~4, but the real prologue inserts up to 9 variable register-save insns (`src/arch/arm64/arm64-gen.c:1564-1614`); unlike x86_64 (which rebases the CFA onto fp via `DW_CFA_def_cfa_register`, `mccdbg.c:1574`) the arm64 CIE keeps the CFA sp-relative (`:1445-1453`) and only adjusts `def_cfa_offset`, so `def_cfa_offset 224+diff` attaches ~28-36 bytes too early → CFA overshoots by `diff` across the save window → corrupted backtraces / broken table unwind for any variadic fn. riscv64 shares the anti-pattern (`:1607-1633`).
- **[MED] Flexible/zero-length array emits a bogus 2^64 `DW_AT_upper_bound`.** `dwarf_uleb128(…, t->type.ref->c - 1)` with `c == 0/-1` → `-1` sign-extends to `0xFFFF…FF` (`src/mccdbg.c:2714`; abbrev `:150-152`). gcc/clang omit the bound; consumers see ~2^64 elements.
- **[MED] enum DIE hardcodes `byte_size=4` and an `int` base**, contradicting `enum E:long long` (8), value-widened enums (8), and `-fshort-enums`/packed (1/2) (`src/mccdbg.c:2608,2619` vs `src/mccgen.c:7398-7437`). Debugger reads the wrong byte count; consumer layouts wrong.
- **[MED] bit-field members always use `DW_AT_data_bit_offset` (a DWARF-4 attribute) even in a DWARF-2 CU** (`src/mccdbg.c:184-190,2574-2579`; CU version from `s1->dwarf` can be 2 — the Mach-O default). Non-conforming; strict v2/v3 consumers can't decode any bit-field. (Adjacent to but distinct from the T-mac-30015 Mach-O-weak-`-g` note.)
- **TASK: T-mac-30035.**

### <a id="r6-type-abi-half"></a>Type-system / ABI
- **[HIGH] `_Float16`/`__bf16` passed/classified in INTEGER registers on x86_64 & arm64, violating both psABIs and internally inconsistent with how they are RETURNED.** `is_float_abi()` excludes the half types (`src/mccgen.c:669-670`) though `is_float()` includes them, so args go via a GPR (x86_64 `classify_x86_64_inner` → integer mode, `src/arch/x86_64/x86_64-gen.c:1146-1148`; arm64 skips the FP-reg rule → x0, `src/arch/arm64/arm64-gen.c:1119,1125`, and half-HFAs not recognized at `:979`), while scalar returns use xmm0/v0 (`is_float()` true, `src/mccgen.c:15036`). Any call to/from a gcc/clang half-taking/returning function miscompiles, and a wrongly-GPR half shifts EVERY following integer arg. Root cause is the `is_float`/`is_float_abi` split (`src/mccgen.c:669-670`) — reconcile the two predicates, don't patch per-arch.
- **[MED] x86_64 over-aligned ≤16B struct over-counts registers (NO_CLASS not modeled).** `classify_x86_64_arg` derives reg_count from mode+size/8 (`src/arch/x86_64/x86_64-gen.c:1304-1330`) and `classify_x86_64_inner` merges fields only, never per-eightbyte NO_CLASS. `struct{_Alignas(16) int x;}` (size 16, 2nd eightbyte all padding) → mcc rdi:rsi / rax:rdx, gcc rdi / rax only → ABI break + shifts following args. `struct{_Alignas(16) double d;}` hits it on the SSE side. arm64 is correct here (ceil(size/8), no NO_CLASS concept).
- **TASK: T-mac-30036.**

### <a id="r6-rir-replay"></a>CST/AST/RIR capture-and-replay
- **[MED-HIGH] Census replay fallback restores `.text`/`.rela.text` but never rewinds `.data`/`.rodata`.** `ast_func_end` rewinds only text+rela (`src/mccast.c:20869-20872`), the fallback restores only those (`:21704-21712`); trial-appended data/rodata constants (e.g. a `double` in the const pool) are neither compared nor rewound — the exact stale-section shape the T-win-50003 Bucket B fix zero-fills in the OTHER trial path (`ast_search_emit_size`, `src/mccast.c:19617-19661`), but here in the DEFAULT `-O1+` census path. Benign free-space on ELF/Mach; PE import-table-corruption class (0xC0000005 pre-main) on PE — an uncovered latent-corruption / cross-platform surface.
- **[MED] `ast_reemit` frame-floor scan counts only `VT_LOCAL`, misses `VT_LLOCAL` → under-sized frame.** `src/mccast.c:21876` gates on VT_LOCAL only; the census scan covers both (`:6931-6932`). A forward-inlined re-emit whose only deep local is `VT_LLOCAL` gets too-high a floor → replay-allocated temporaries overlap it → silent stack corruption. No faithfulness fallback on this path; `-O1+` only.
- **[MED] `ast_fold_rec` result-type promotion diverges from `gen`'s usual-arithmetic-conversions** (new fold-vs-gen family member, distinct from the div-value and diagnostic ones). Sub-int operands are never promoted to `int` (`src/mccast.c:7042-7054` keeps the narrow type, may OR in `VT_UNSIGNED`), and `:7069` stamps x's type-ref even in the branch that switched `tt` to y's type. The value matches (`value64` is 32-bit for non-LLONG) but the node TYPE differs → a signedness-sensitive parent op selects different codegen; ships under `-fno-replay-fallback`. Reachable only after a prior template pass synthesizes a `Binary(lit,lit)` from sub-int operands.
- **TASK: T-mac-30037.**

### <a id="r6-diagnostics-pedantic"></a>Diagnostics — pedantic / implicit-int severity
- **`-pedantic-errors` fires HARD errors inside system headers on most preprocessor pedantic paths** — the warning form is dropped there by `error1`'s system-header suppression (`src/libmcc.c:667-673`), but the error form is not, and the pp sites guard `!pp_in_system_header()` only sporadically. Unguarded: `src/mccpp.c:2979,3053,3190,3228,3510,3553,3751,4152,5295`; guarded siblings `:1441,2454,4299,4613,4618`; the `mcc_pedantic()` helper self-guards (`src/mccgen.c:547-552`). So the same construct in a `<system header>` is silent under `-pedantic` but fatal under `-pedantic-errors` — site-dependent, not rule-dependent; makes `-pedantic-errors` unusable against real SDK/glibc headers.
- **`warn_pedantic`-only diagnostics that never honor `pedantic_errors`** — e.g. tentative-array "assumed one element" (`src/mccgen.c:18803-18806`) stays a warning under `-pedantic-errors` while its siblings promote; the flag is silently partial.
- **`-pedantic-errors -Wno-pedantic` honored on some paths, ignored on others.** `-Wno-pedantic` clears only `warn_pedantic`, not `pedantic_errors` (`src/libmcc.c:2403,2793-2794`); `warn_pedantic`-gated sites go silent (`mcc_pedantic` early-return `src/mccgen.c:545`; `src/mccpp.c:3481,3508,3551,3749`) while `pedantic_errors`-only sites still hard-error a disabled diagnostic (`src/mccpp.c:1441`, `src/mccgen.c:4326,5850`).
- **C99 implicit-int diagnosed 3 different ways, and `-Wno-implicit-int` DOWNGRADES the error instead of suppressing.** `src/mccgen.c:18657` hard-errors (old-style extern param) but `:18443,:18526` stay warnings forever; and `:18657` is gated on `(warn_implicit_int & WARN_ON)` so `-Wno-implicit-int` takes the else branch (`:18660`) and turns the hard error into a warning — the opposite of `-Wno-`.
- **TASK: T-mac-30038.**


## Pending taskification (captured, not yet minted)

Lower-severity or newly-arrived items awaiting a `TODO.md` task in a later loop iteration: codegen arm64 `va_arg` assert (`#codegen-arm64-vaarg-assert`); type/C23 bit-field gaps (`#type-c23-gaps`); runtime/intrinsics (`#runtime-intrinsics`); Metal-vs-Vulkan f64 (`#gpu-metal-vs-vulkan-f64`); JIT RWX/`MAP_JIT` (`#jit-rwx-mapjit`); debug-info asymmetries beyond Mach-O unwind (`#debug-info-asymmetry`). The JIT lazy-vs-sync root cause (`#jit-lazy-vs-sync-kgc`) should be attached to existing **T-lin-10029**, not minted separately.
