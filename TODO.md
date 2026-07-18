# TODO

## Working conventions

Mark an item in progress, then commit and push to main, before starting work on
it. Completed items are pruned once verified; the detail lives in git history.

## Skip-gated: runtime-JIT backend-override differential guards

`regression/o4-aot-jit` + `regression/jit-submit-aot-diff` (and 6 `jit/selftest-*`
on MSVC-arm64) are skip-gated in CMakeLists to x86_64-non-MSVC, the only cells
where the runtime `-run` JIT override path (`mcc_jit_submit_ast` /
`MCC_JIT_SUBMIT_AOT`) both fires and is correct. Two real bugs hide behind the
gate and must be root-caused (repros run the differential scripts directly):

- **arm64 runtime-JIT frameless-leaf return corruption.** ROOT-CAUSED, fix
  pending (not a search/gate/DIVMAGIC bug; plain `MCC_JIT=1` reproduces). On
  arm64-**Linux** a hot function that is a *frameless leaf* — no callee-saved
  spill, no stack locals, no loop — comes back miscompiled: the caller's control
  flow breaks (its post-return code, e.g. `printf`, re-executes every iteration)
  and the accumulator is garbage. Minimal repro:
  `int f(int x){return x+1;} … for(i…)s+=f(i); printf("%ld",s);` →
  `MCC_JIT=1 MCC_JIT_HOT_THRESHOLD=50 mcc -O4 -run` emits one garbage line per
  iteration; `MCC_JIT=0 mcc -O2 -run` is correct. Adding any frame (a `volatile`
  local, an array, or a loop) masks it — which is why `gcd`/p3-popcount (loops)
  survive and only straight-line KGC-verified `poly` fails.
  Mechanism (verified by disasm): the arm64 mode-6 dispatch prepends a **fixed
  224-byte callee frame** at the entry (`stp x29,x30,[sp,#-224]!; mov x29,sp;
  spill args`) then tail-`br`s through `*slot` (src/mccast.c ~13504, `body+16`).
  The teardown `mov sp,x29; ldp x29,x30,[sp],#224` lives ONLY in the AOT baseline
  body, so any swapped-in target leaks that frame → caller resumes with corrupt
  sp/x29. (x86's frame is a single `push rbp` its stubs undo with `leave`;
  arm64's is a big spill frame with no generic undo.)
  DEAD END — do not retry: adding the leave to the arm64 KGC stubs + framing
  `mccjit_dispatch_entry` (reflog `7d592c01`, dropped) fixes the KGC path and all
  49/45 jit selftests + both regression scripts, but BREAKS `jit/selftest-slice-
  live`: slot targets are a MIX — mode-6 stubs (KGC, AOT body) AND self-contained
  ones (direct/trampoline variants, STEP-3 slice kernels via `route=direct`) —
  and are also called frameless directly by `mccjit_bench_admit` + the verbose
  probe. No per-stub leave satisfies all entry paths.
  CORRECT FIX: emit the arm64 dispatch stub BEFORE the prologue so `*slot`
  initially points at the full function (prologue included) and every swapped-in
  target is entered CLEAN (no leave anywhere) — use `arm64_func_start_offset` as
  the splice base instead of `ast_body_ind_sv`, `body_sym` → start+16, rebase the
  prologue's relocs through `ast_baseline_splice`. Overlaps the STEP-3 slice
  promote seam. Repro env: arm64-Linux Docker (native on Apple Silicon),
  `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja mcc`, then
  `sh tests/ci/regression_jit_submit_aot_diff.sh build/mcc`; the minimal
  `int f(int x){return x+1;}` loop is the fastest check. (arm64-macOS can't repro
  — `-run` JIT off under Apple W^X; validate there via `ctest -R jit/`.)
- **MSVC-arm64 JIT-exec miscompile.** The 6 selftests (reemit-gates, fold-consts,
  search-live{,-kgc,-struct,-purity}) JIT-compile a fn and call it; on the
  arm64-Windows runner the called code faults/miscompiles. Same family as the
  d6755098 skip-gate (unreproducible off real arm64-Windows HW; wine masks it).

## Runtime JIT: `--jit` / `MCC_JIT` / `MCC_CONFIG_JIT`

mcc activates the runtime JIT through a layered model: the `--jit`/`--no-jit`
CLI flags, a runtime `MCC_JIT` env var (0/1) read by the baked `.init_array`
constructor, and the CMake `MCC_CONFIG_JIT` build default (ON) → `MCC_JIT_DEFAULT`.
`--embed-jit` bakes the machinery into output. Precedence is env > flag (for
`-run`) > CMake default; `MCC_JIT=0` runs pure AOT.

JIT eligibility covers int/ptr-argument functions, string-using functions
(`MCCJIT_ROLE_DATA` captures anonymous rodata string literals as raw bytes and
rematerializes them in `mccjit_build_rec`, guarded to in-bounds rodata elfsyms
under `MCCJIT_DATA_MAX` with no relocations in range), pointer-returning
functions (the intent serializes the return pointee via `MccjitIntent.ret_type_ref`),
and enum-typed functions (`mccjit_strip_enum` recompiles enums as their integer
base). Switch functions and any unsupported shape stay on the AOT baseline.
Internal JIT-helper compiles set `mccjit_internal_compile`, so they bypass the
process-global embed registry. Runtime JIT runs on ELF, Mach-O, and PE/Windows
targets (`ctest -R jit/` green on each).

## Windows JIT-embed

Windows runtime JIT works: `ctest -R jit/` = 32/32 on the x86_64 mingw host.
`src/mccjit_win32.h` shims the POSIX layer (VirtualAlloc exec pages ·
CreateFileMapping KGC store · SRWLOCK + CONDITION_VARIABLE + InitOnce +
`_beginthreadex` · QPC clock · Interlocked atomics); the x86_64 stubs carry
Microsoft-x64-ABI branches; the `-run`/`--embed-jit` pipeline fires on PE.
`mccjit_make_kgc_stub_mixed` returns NULL on WIN32 (mixed int+FP → AOT baseline,
`jit/selftest-mixed` skips). The KGC verify-stub/dispatch tail is x86_64/arm64-only;
i686 promotion-dependent selftests skip via `MCCJIT_HAVE_STUB_TAIL` (a real i386
KGC/FP(x87)/mixed stub tail is the future work to make i386 JIT-promote).

The prior Windows/PE CI failures are resolved (detail in git history): the
`exec-replay/{run_atexit,errors_and_warnings}` mismatch was cross-CRT stdout
buffering — a winlibs/msvc mcc links UCRT while `-dt -run` snippets use msvcrt, so
snippet stdout flushed only at process exit (reordered); fixed by `fflush(0)` in
`runmain.c`'s `_runmain`/`exit`. The i686 `jit/selftest-*` failures skip on arches
without the stub tail. The ckconfig i686 `.rsrc` was a transient host-binutils
manifest quirk that no longer reproduces (toolchain-side, not mcc).

### Windows embed-blob (`--embed-jit` standalone exe)

`--embed-jit` now **links** the JIT engine end-to-end on Windows (exe ~3 KB →
~1 MB); the one remaining blocker is that it doesn't yet **run** (below). Landed
(detail in git history):
- **COFF/PE object + archive reader** — `coff_load_object_file`/`coff_object_type`/
  `coff_map_reloc` in `src/objfmt/mccpe.c` (sections/symbols/relocs → internal ELF;
  `.text$mn`→`.text` merge; COMDAT/link-once dedup), wired into `mcc_load_archive`
  (whole-archive + à-la-carte) and the bare-object dispatch, `#ifdef MCC_TARGET_PE`.
  `libmcc_jitengine` ungated on WIN32. Validated: bare object / archive / whole-
  archive links run correct; `ctest -R jit/` = 32/32, no regressions.
- **Function-selection** — `--embed-jit` at `-O0` now arms the AST recorder so the
  mode-6 stash fires (`ast_replay_env |= embed_jit`, `src/mccast.c`); previously the
  engine was never stashed/linked (silently a non-embed exe, on Linux too).
- **Portable temp file** — `host_temp_file` (`mcchost.c`, GetTempPath+O_BINARY on
  WIN32) replaces the hardcoded `/tmp` mkstemp.
- **mingw runtime deps** — after the engine link, the toolchain support archives
  (`libmingwex`/`libgcc`/`libucrt`/`libmsvcrt`/`libkernel32`) are added alacarte;
  lib dirs baked from `gcc -print-*` (`MCC_EMBED_JIT_{MINGW,GCC}_LIBDIR`).
- **kernel32.def** — added `InitializeSRWLock` + the SRW/InitOnce exports the JIT
  win32 shim uses.

- [~] **IN PROGRESS: RUNTIME: the embedded engine crashes at startup — mcc has no PE
  import-LIBRARY support.** The `--embed-jit` exe links but SIGSEGVs before `main`
  (both `MCC_JIT=0`/`=1`). Root-caused via gdb: a `memset(buf,0,4)` call goes to
  the import thunk `jmp *[IAT]`, but the IAT slot still holds the
  `IMAGE_IMPORT_BY_NAME` string pointer (the "memset" name, 16 bytes away) — the
  Windows loader **never bound the import**, so the thunk jumps into the name
  string. Deeper cause: mcc (like TinyCC) resolves DLL imports **only from `.def`
  files** (`pe_load_def`→`pe_putimport`); it has no import-library handling. The
  mingw import archives (`libmsvcrt.a`/`libucrt.a`/`libkernel32.a`) satisfy the
  engine's libc refs with import members that mcc mishandles: **short-import**
  members (`IMPORT_OBJECT_HEADER`, sig `00 00 FF FF`) are skipped by
  `coff_object_type`, and **long-import** members (regular COFF with `.idata$`
  sections) are loaded as inert data — neither assembles a bound PE import
  directory, so the IAT is never filled. (This is why the engine links but its
  imports are dead.) There is also a **UCRT-vs-msvcrt** dimension: the winlibs
  engine is UCRT-compiled, so `__imp___acrt_iob_func` et al. are not in mcc's
  msvcrt `.def` and carry an ABI mismatch.
  **Fix path (a real linker feature):** teach mcc to consume PE import libraries —
  parse short-import members (`IMPORT_OBJECT_HEADER` → `mcc_add_dllref` +
  `pe_putimport`, mirroring `pe_load_def`) and recognize long-import `.idata$`
  members (extract name/DLL, register via `pe_putimport` instead of loading the
  `.idata`), plus a UCRT export set. Non-COFF-reloc (reloc types verified:
  ADDR32NB is `.pdata`-only, SECREL is `.debug`-only).
  *Separate minor follow-up:* ADDR32NB is mapped to absolute `R_X86_64_32` (should
  be RVA); harmless today (`.pdata`-only) but wrong.

  **Implementation guide (everything mapped this session):**
  - *Repro/build (native Windows host).* winlibs x86_64 UCRT at
    `vendor/winlibs-mingw-w64-16.1.0-ucrt-x86_64/mingw64/bin` (prepend to PATH in
    PowerShell so gcc finds `as`; Git Bash mangles `C:/…` PATH). Build dir
    `cmake-winlibs` (configured `-DCMAKE_C_COMPILER=<winlibs gcc>`). Repro:
    `mcc --embed-jit hello.c -o h.exe` (hello with an eligible fn like
    `int busy(int)`), then `MCC_JIT=0 ./h.exe` → SIGSEGV. gdb: `jmp *[IAT]` thunk,
    IAT holds the `IMAGE_IMPORT_BY_NAME` string ptr. (i686 repro: `cmake-i686`,
    `-DMCC_TARGET_ARCH=i386`.)
  - *Pattern to mirror.* `pe_load_def` (`mccpe.c:1741`): `dllindex =
    mcc_add_dllref(s1, dllname, 0)->index;` then `pe_putimport(s1, dllindex, name,
    ord);` per export. mcc's `__imp_` handling already exists (`mccpe.c:1385`
    strips `__imp_`/`_imp__`), so one `pe_putimport(name)` serves both the thunk
    (`name`) and IAT (`__imp_name`) references.
  - *Short-import member* = 20-byte `IMPORT_OBJECT_HEADER`: `WORD Sig1(=0),
    Sig2(=0xFFFF), Version, Machine; DWORD TimeDateStamp, SizeOfData; WORD
    Ordinal/Hint, Type` (Type: bits 0-1 = CODE/DATA/CONST, bits 2-4 = NameType),
    then `name\0` then `dllname\0` (within `SizeOfData`). Add a
    `coff_load_short_import(s1,fd,off)` in `mccpe.c` + detect (`Sig1==0 &&
    Sig2==0xFFFF`) in `mcc_load_archive`'s member dispatch (`mccelf.c` — the same
    two spots as the COFF-object dispatch: whole-archive loop + `mcc_load_alacarte`
    member pull). NameType may prefix-strip (`?`/`@`/`_`) — handle IMPORT_OBJECT_NAME
    (as-is) first; ordinal-only imports are rare here.
  - *Long-import member* = regular COFF object (machine `64 86…`, so it currently
    loads via `coff_load_object_file`) carrying `.text` thunk + `.idata$2/$4/$5/$6`.
    Detect these (member defines `X` + `__imp_X` and has `.idata$` sections) and
    route to `pe_putimport` instead of loading the `.idata` as data. The DLL name is
    in `.idata$7` (or derivable). *Note the archives are MIXED:* `libmsvcrt.a`/
    `libucrt.a` also hold real code members (e.g. `lib64_libucrt_extra_a-*.o` math
    fns) that must still load normally — dispatch on member shape, not archive.
    `nm libmsvcrt.a` shows `T memset` (thunk) + `I __imp_memset` per import.
  - *UCRT export set.* `__imp___acrt_iob_func`/`__imp__open`/`_read`/`fstat64i32`/
    `strtoll`/`strtoull`/`__p__environ` are UCRT (not in mcc's `msvcrt.def`).
    Once import-lib parsing works they come from `libucrt.a` with the right DLL
    (`api-ms-win-crt-*` / `ucrtbase.dll`); no separate def needed. Beware UCRT vs
    msvcrt `FILE`/stdio ABI if any FILE-typed import is exercised.
  - *After it works:* re-check whether the `kernel32.def` SRW additions and the
    `mcc_add_jit_engine_embedded` static-lib list are still all needed (imports may
    then resolve straight from `libkernel32.a`). Validate: `mcc --embed-jit hello.c`
    runs correct under `MCC_JIT=0` and `=1` (self-recompile), and `ctest -R jit/`
    = 32/32 on winlibs.

## Bring Windows AOT + JIT to full parity with the Linux implementation

The Linux (ELF) target is the reference implementation for both the AOT backend
search and the runtime/embedded JIT. Windows (PE) trails it in several concrete
places that this item tracks to closure. Parity = a Windows PE build passes the
same JIT/AOT-exercising suites the Linux build does, with no target-specific
skip-gates beyond genuine OS/hardware limits.

Known gaps (each links to its detail above / in git history):

- **`--embed-jit` standalone exe does not run on PE.** Linux `--embed-jit` bakes
  the engine and self-recompiles; PE links it but SIGSEGVs at startup for lack of
  PE import-library support (the IN-PROGRESS item under "Windows embed-blob"). This
  is the first parity blocker — until the embedded engine runs under `MCC_JIT=0`
  and `=1`, the file-embed JIT is Linux-only.
- **i386-PE JIT promotion is inert.** The KGC verify-stub / dispatch tail is
  x86_64/arm64-only, so i686 promotion-dependent selftests skip
  (`MCCJIT_HAVE_STUB_TAIL`). Linux i386 has the same gap, but bringing a real i386
  KGC/FP(x87)/mixed stub tail online is required for full-arch parity.
- **`mccjit_make_kgc_stub_mixed` returns NULL on WIN32.** Mixed int+FP signatures
  fall back to the AOT baseline (`jit/selftest-mixed` skips) because the Win64 ABI
  is positional; Linux SysV handles it. Needs a per-arg class vector for Win64.
- **arm64-PE runtime-JIT bugs are skip-gated.** The frameless-leaf return
  corruption + the MSVC-arm64 JIT-exec miscompile (both in the skip-gate section
  at the top of this file) keep `-run`/`MCC_JIT` promotion off on arm64-Windows.
- **The `-run` backend-override differential path** (`regression/o4-aot-jit`,
  `regression/jit-submit-aot-diff`) is skip-gated to x86_64-non-MSVC; PE parity
  needs those green on the PE cells once the runtime-JIT bugs above are fixed.

Definition of done: on x86_64-PE, the same `ctest -R jit/` + `regression/*` +
`exec-replay/*` set runs (not skips) and matches the Linux goldens; the remaining
skips are arm64-HW / i386-x87 items with a filed sub-task, not silent gaps.

## qemu-amd64 emulation noise (not compiler defects)

Under an amd64 Ubuntu container on qemu/Apple Silicon, a recurring set of ctest
failures are environment/emulation artifacts — they fail identically on clean
HEAD and/or pass when run serially in isolation:

- `macho-*` (Mach-O output cannot run on Linux),
- `asan_shadow_native_*`, `cli/bcheck_exe_static_bounds` (ASan/bounds runtime under emulation),
- `config-defines` (host triplet/OS-release specific), `run_atexit`,
  `exec-search*/errors_and_warnings` (diagnostic-text/atexit env deltas),
- `git-stamp` (fails when the working tree is dirty),
- sporadic `function_pointer`/`func_pointers`/`func_arg_struct_compare`/`complex`
  (qemu parallelism flakes; pass in isolation).

A real regression shows up as a *different* failing test. Validate JIT/codegen
work with `ctest -R jit/` + `ctest -R ast/` and per-test serial reruns, not the
full parallel sweep.

## FIXED: AST-replay frame desync overlaps stack slots (self-host DIVMAGIC crash)

FIXED 2026-07-17 (see NOTES.md for rationale). Replay-time scratch allocations
(the register-promotion save area + `get_temp_local_var` backend temps) now use a
dedicated replay frontier `ast_temp_frontier` seeded below `ast_locrec_min` (the
lowest recorded AST-local offset), so they can never overlap a replayed AST slot.
New `ast_alloc_temp_loc` (mccast.c) serves both; `ast_promo_entry_init` and
`get_temp_local_var` call it; the frontier reset + `ast_loc_low` seed were moved
before `ast_promo_entry_init` in the replay macro. Validated: repro no longer
SEGVs, self-host search evaluates ~26k like gcc-mcc, `fixpoint-invariant`
byte-identical, `ast/`+`jit/`+`fixpoint` ctests 98/98 green.

### Original root cause (kept for reference)
FULLY ROOT-CAUSED 2026-07-17. NOT a JIT bug (reproduces with `MCC_JIT=0`). When an
*mcc-compiled* mcc runs the in-process `MCC_AST_SEARCH` gate search over a heavy TU
(`src/mcc.c`), a stack-slot overlap in the DIVMAGIC helpers corrupts a pointer's high
dword and SIGSEGVs (crash frame is `ast_node` deref of a bit-34-corrupt `AstArena*`,
`0x7d4170` -> `0x4007d4170`). gcc-built mcc emits the same buggy output but only crashes
when the buggy function actually runs (the search exercises it); gcc laid out gcc-mcc's
own frames fine, so gcc-mcc "works" while its OUTPUT is buggy.

Root cause (store-level trace confirmed, 269 overlaps, ALL in `ast_divmagic_try`,
`_signed`, `_u64`, `_s64`): the AST-replay optimizer records stack-slot offsets
(`ast_alloc_loc`, mccast.c:1132) during the PRE-transform codegen pass and stores them
IN the AST nodes (must stay stable). It then TRANSFORMS the AST (DIVMAGIC -> 64-bit
magic multiply `MULHU`, needing an extra 8-byte temp). On REPLAY (re-emit of the
transformed AST via `ast_reemit`), `ast_alloc_loc` replays the recorded pre-transform
offsets, but the transformed code allocates DIFFERENT backend temps via
`get_temp_local_var` (mccgen.c:1577), which is NOT recorded/replayed. Both allocators
share the global `loc` frontier — `ast_alloc_loc` rewinds it to recorded absolute
offsets, `get_temp_local_var` decrements it freshly — so they DESYNC: a fresh 4-byte
temp lands at `-124`, overlapping the recorded 8-byte `uint64_t` slot at `-128`. A
4-byte store then clobbers the 8-byte value's high dword. Trace proof: record makes one
4-byte temp at `-132`; replay makes an 8-byte temp at `-136` + a 4-byte at `-140`
(different count/size) -> `[STORE-OVERLAP] [-128,+8) vs [-124,+4)`.

Rebuildable repro (no JIT, no embed):
  DEFS='-DMCC_CONFIG_OPTIMIZER=1 -DMCC_CONFIG_PREDEFS=1 -DMCC_CONFIG_DIAG_RT=2 \
    -DMCC_TARGET_X86_64=1 -DMCC_CONFIG_TRIPLET="x86_64-pc-linux-gnu" \
    -DMCC_CONFIG_AUTO_MCCDIR=1 -DMCC_CONFIG_LSP=1 -DMCC_JIT_DEFAULT=1'
  cmake-embedjit/mcc -O2 $DEFS <full src/formats/objfmt/arch includes> src/mcc.c -o self
  MCC_JIT=0 MCC_AST_SEARCH=1 MCC_SEARCH_WORKER=1 self -O8 <includes> -c src/mcc.c -o /dev/null
  -> SIGSEGV in the search. Store-overlap detector (temporary): log VT_LOCAL stores in
  `store()` (x86_64-gen.c:480) per funcname, flag two different-offset stores whose byte
  ranges overlap; build with $DEFS (NOT plain -c: the overlap needs the optimizer/replay).

### FIX TASKS (in order)
- [x] T1. During replay, allocate backend temps (`get_temp_local_var`) BELOW the recorded
      frame low-water (`ast_loc_low`) so they can never overlap a recorded `ast_alloc_loc`
      AST slot. AST-slot offsets must stay stable (AST nodes encode them); backend temps
      are scratch and may move. Decouple the temp frontier from the `ast_alloc_loc`-rewound
      `loc` (e.g. a separate `ast_temp_loc` frontier used during `ast_replaying`, seeded to
      `ast_loc_low`, monotonically decreasing). Touches: `ast_alloc_loc` (mccast.c:1132),
      `get_temp_local_var` (mccgen.c:1577/1600), and the ~5 `loc = ast_alloc_loc(...)` call
      sites (mccgen.c:4783,9545,9667,10209; mccast.c:4301).
- [x] T2. The replay emits its own `gfunc_prolog`/epilog, so the reserved frame grows to
      include the scratch region — verify the epilog frame-size patch uses the post-replay
      `ast_loc_low` (relates to the earlier `ast_loc_low` clamp fix, mcc-inline-c-o1).
- [x] T3. VALIDATE: repro no longer SEGVs AND self-host search evaluates like gcc-mcc
      (~28k candidates); `fixpoint-invariant` byte-identical; exec differential
      (`ctest -R exec`) unchanged; `ctest -R ast/` + `ctest -R jit/` green; re-run the
      store-overlap detector -> 0 overlaps in the divmagic functions.

### Dead ends (do not repeat)
- Layout-luck: `-O2` self-host SEGVs, `-O1`/`-O0` "pass" but are still corrupt. NOT a clean bisect.
- Disabling every `-O2` AST pass + `MCC_AST_TEMPLATES` does NOT remove it -> it's the replay
  frame machinery, not any single strategy pass.
- Clamping `get_temp_local_var` to 8-byte size/align: changed the binary, did NOT fix -> the
  slots are individually disjoint; it's a cross-allocator record/replay desync, not undersize.
- Allocation-level overlap detectors on plain `-c` (no $DEFS): 0 hits -> MUST use $DEFS.
- ASan: only benign arena leaks (wrong-width access of valid memory, not OOB/UAF). valgrind
  dead on this host (SIGILL in `_dl_start`). `-g` hides it (disables AST-replay).
- The `rbx=0x20c49ba5` (÷1000 magic) at the fault is a red herring (`ast_now_ms`), not the cause.

## Refactor: always-on JIT + runtime perm x combo x strategy search to jit_max_duration

Decisions (settled): D1 = Both (compile-time search picks the AOT baseline the JIT
deopts to + bakes the engine; runtime JIT continues searching). D2 = timeout is the
existing `--jit-max-duration` (`s->jit_max_duration`, default 600s); `-O4`/`-O<sec>`
stays the compile-time search budget. No new `--jit-timeout` flag.

### STATUS (present state, 2026-07-18)

The core refactor is DELIVERED. What ships today:
- Runtime perm x combo x strategy search on hot functions, threaded (async
  `mccjit_pool`), live incremental hot-swap per improving variant, bounded by
  `jit_max_duration` — opt-in `MCC_JIT_SEARCH`; default path fixpoint byte-identical.
  (STEP 0 enabler + the ENABLER/LIVE block under STEP 2.)
- Backend override API `mcc_jit_submit_ast(Sym*, AstArena*, gate_mask, flags)`
  (STEP 1 + STEP 2 (iv)): the backend hands its live compiled AST to the engine; the
  runtime JIT recompiles/hot-swaps FROM that AST, override winning per-symbol over
  the shipped `MccjitIntent`. Opt-in `MCC_JIT_SUBMIT_AOT` (default OFF -> dormant ->
  fixpoint byte-identical). Wired at `ast_jit_submit_aot` (mccast.c, in
  `ast_func_end` at the `mccjit_embed_note` site).
- Proof + hardening: `regression/o4-aot-jit` (Part 1 AOT 4s search + Part 2
  backend-AST override fires, output == JIT-off reference) and
  `regression/jit-submit-aot-diff` (3 real programs x {JIT=0, JIT, JIT+submit} all
  agree, override actually fires). `jit/selftest-submit` end-to-end (1005 -> override
  15 -> clear -> 1005). 52/52 jit/embed/regression + fixpoint byte-identical.

CORRECTION to the north-star sketch below (line ~333, "without going through
serialization"): a live-AST hand-off is NOT serialization-free. A raw `AstArena`
clone is not portable across `MCCState` contexts — functions reference their param
syms / callees / globals as live `Sym*` POINTERS that are dead by runtime-recompile
time, so reemitting a clone segfaults. The `Sym*`->name-handle remap that makes an
arena portable IS `mccjit_intent_serialize/_deserialize`. So the override table
stores a SERIALIZED BLOB (serialized at submit time while the pointers are live). A
truly serialization-free seam would need recompiling in the origin `MCCState` or
replicating the remap on the live arena — deferred; serialize-on-submit is the sound
path and reuses the ship-to-disk machinery. See NOTES.md "Backend override API".

REMAINING (each needs a decision or a host mcc lacks — none are mechanical loop work):
- STEP 2 (i)/(ii): fold / JIT-execute pure slices INTO the submitted arena so the
  override delivers an OPTIMIZED AST, not an equal copy. The ESCAPE-ANALYSIS half is
  DONE (b5fff356): `ast_fn_purity_noescape` (mccast.h) treats a store to a
  non-escaping local scalar as pure where `ast_fn_purity` returns IMPURE — additive,
  `ast_fn_purity` untouched, default byte-identical; jit/selftest-noescape. REMAINING
  (the real subproject): a bounded, UB-sound compile-time loop interpreter that
  CONSUMES the analysis to fold a no-escape-pure slice (incl. a loop) to a constant
  and splice it into the submitted arena before `ast_jit_submit_aot` — `ast_eval_slice`
  evaluates no loops yet. Today the submitted arena == the faithful arena (seam
  proven; the fold is the refinement).
- STEP 2 (iii): dispatch the AOT eval/JIT-exec jobs on `mccjit_pool`. The heavy work
  (recompile from the override AST) is ALREADY threaded via the runtime promote seam;
  only the (nonexistent yet) AOT eval jobs of (i)/(ii) would need pool dispatch.
- STEP 3: live partial-slice hot path DELIVERED 2026-07-18 (opt-in
  MCC_JIT_SLICE_SEARCH; jit/selftest-slice-live). RESIDUAL: true in-arena
  sub-region splice for slice ⊊ body (C2b reconcile) — needs a node-identity-
  stable arena-splice primitive; deferred as lowest-ROI. See the STEP 3 block below.
- Windows PE import-library RUNTIME crash (see "Windows embed-blob" above): needs a
  native Windows host + a PE import-library loader. Cannot be built/validated on Linux.

### Target architecture (north star — 2026-07-17)

Three interacting roles for the one embedded engine:

- **(A) AOT `-O4`+ uses the JIT as a compile-time evaluator.** The backend does not
  just statically search gate permutations — it drives the embedded engine over the
  target program's AST guided by *const evaluators* (`ast_eval_slice.h`:
  `ast_eval_binop`/`ast_eval_slice`, UB-soundness-gated by `MCC_AST_JIT_EVAL_GATE`,
  refusals counted by `ast_jit_eval_refused_count`) to fold/specialize pure slices
  and feed concrete facts back into codegen. Foundation exists (eval-slice + the
  M5c slice kit `ast_slice_extract`/`_certifiable`/`_equiv`/`_live_ins`); using it to
  *guide AOT backend compilation* at `-O4`+ is NEW.
- **(B) Runtime JIT cold path = deserialize the shipped AST.** The baked program
  ships its `MccjitIntent` AST; the engine deserializes and re-emits it
  (`mccjit_recompile_common` -> `ast_reemit`). This is today's behavior and the
  cold/deopt baseline. Mostly settled.
- **(C) Runtime JIT hot path = permute, hot-swap a partial slice, reconcile.** As the
  engine finds an optimization permutation it hot-swaps *a partial slice* of the AST
  (not necessarily the whole function; `ast_slice_extract` kernel), then the
  surrounding slices are reconciled/revised to accommodate it — ideally each slice
  running its own permuted optimization strategy. Phase 3 below currently plans
  WHOLE-FUNCTION swap; partial-slice swap + neighbour reconciliation is NEW and the
  main open design (see "Open decisions" table). `mccjit_ast_spec_fold` (param->const
  partial fold) is the closest existing primitive.

### Open decisions — guide with cell refs like A1a (author will pick)

Group A — AOT `-O4`+ JIT-evaluator-guided backend:
  A1 what the engine computes at AOT time:
     a) run `ast_eval_slice` const-evaluators to fold/specialize pure slices (cheap, sound-gated)
     b) JIT-EXECUTE pure slices of the target to get concrete constants (compile-time function exec)
     c) both — eval-slice for cheap cases, JIT-execute for expensive pure slices
  A2 how it is invoked:
     a) in-process during the -O4 search (AOT compiler links the engine, calls it)
     b) the out-of-process fork-pool workers each call the engine
     c) a dedicated const-eval pass gated on -O4+ (separate from the gate search)
  A3 soundness gate:
     a) reuse the existing MCC_AST_JIT_EVAL_GATE UB oracle (refuse unsound)
     b) purity-only: restrict to AST_PURITY_TIER0 slices
     c) both + a value memo keyed by slice hash

Group B — runtime cold baseline:
  B1 baseline source:
     a) keep deserialize-shipped-AST as-is (settled)
     b) also ship the AOT search-winner gate config so cold start = best AOT baseline
  B2 when the hot path is disabled (jit off / infeasible / over budget):
     a) run the shipped AOT baseline (settled)
     b) run the AOT search-winner variant baked as a second baseline

Group C — runtime hot path (permute + partial-slice hot-swap + reconcile):
  C1 swap granularity:
     a) whole function (current Phase 3 plan; simplest)
     b) partial AST slice via ast_slice_extract kernel, rest untouched
     c) adaptive: slice a hot sub-region if the profile localizes it, else whole fn
  C2 reconcile the surrounding slices after a slice is optimized:
     a) re-run ast_run_strat_cycle on the boundary neighbours (re-optimize edges)
     b) ast_slice_equiv-verify the boundary + splice with fixups only (no re-opt)
     c) full-function re-emit but pin the optimized slice
  C3 strategy permutation scope:
     a) each slice searches its own gate/strategy permutation (independent)
     b) one global permutation per function
     c) slice-local search seeded from the function's current best
  C4 correctness net for a swapped slice:
     a) KGC differential verify per slice (existing net)
     b) ast_slice_equiv structural check + KGC
     c) deopt-to-baseline on any mismatch (never keep an unverified slice)

### SETTLED direction (2026-07-17, author-chosen)

Picked cells: A1c, A2a, A3a · B1a/B2a (baseline stays) · C1c, C2b, C3c, C4b.
Two overriding requirements on top:

- **THREADED.** All AOT const-eval / JIT-execute and all runtime permute+recompile
  work runs on worker THREADS (the existing `mccjit_pool` pthread pool:
  `mccjit_pool_start`/`_enqueue`/`mccjit_job_run_lazy`, mccjit_embed.c:711/741/758),
  never blocking the driver's main compile-or-run thread. A2a "in-process during the
  -O4 search" therefore means: the AOT `-O4` search dispatches eval/JIT-exec jobs to
  the pool and joins, not a synchronous inline call and not the out-of-process fork
  pool. The runtime hot path is already async on this pool; the AOT side reuses it.

- **BACKEND OVERRIDE API — backend hands a dynamic AST to the engine.** Today the
  engine's cold path deserializes the compiled-in `MccjitIntent` shipped with the
  program (`mccjit_recompile_common` -> `mccjit_intent_deserialize` -> `it.arena`).
  Add an API so the AOT backend can OVERRIDE that and submit a live, dynamically
  compiled `AstArena` (e.g. the const-eval-refined / partially-specialized AST it
  built at -O4) directly to the engine for a symbol. The engine then optimizes /
  hot-swaps from the backend's AST instead of the baked intent.
  Sketch (names provisional):
    `int mcc_jit_submit_ast(Sym *sym, AstArena *ast, unsigned gate_mask, int flags);`
      - enqueues a pool job that runs the C-hot-path (slice -> permute -> reconcile ->
        KGC/slice_equiv verify -> pointer-swap via `mcc_jit_publish`) on `ast`;
      - PRECEDENCE: a backend-submitted AST for a sym wins over the shipped
        `MccjitIntent` for that sym; symbols the backend does not submit keep the
        deserialize-shipped path (B1a). Store submissions in a per-sym override table
        checked at the top of `mccjit_recompile_common`.
    `void mcc_jit_engine_config(...);` — backend registers the const-evaluator/eval
      hooks (ast_eval_slice + `MCC_AST_JIT_EVAL_GATE` oracle, A3a) the engine may call.
  This is the seam that lets pillar A (AOT eval) feed pillar C (runtime hot path)
  without going through serialization: the backend's dynamic AST is the engine's
  working AST. Serialization stays only for the ship-to-disk cold baseline (B).

Implementation ordering (revised): (0) land the arg-spill fix — DONE. (1) the
backend-override API + per-sym override table + pool-threaded submit, on the
existing whole-function recompile (proves the seam end to end, KGC-verified).
(2) AOT `-O4` dispatches eval/JIT-exec jobs to the pool and feeds results into the
submitted AST (pillar A). (3) partial-slice granularity + neighbour reconciliation
(C1c/C2b/C3c) on top of the working whole-function seam. Each step gated by
fixpoint-invariant byte-identical + exec differential + KGC.

### Implementation guide (finalized 2026-07-17 — anchors, data, integration)

Anchors below are post-arg-spill-fix (commit a7c38f1a) line numbers.

DATA / SEAMS
- `MccjitIntent` (mccjit_internal.h:59-77): the deserialized shipped AST unit.
  `.arena` = the AST; plus salt, `handle_*`, `recs`, `fn_name`, `ret_type_t/ref`,
  `func_type`, `nparam`, `param_type_t/off/name`. This is pillar-B cold data.
- `MccjitCounterState` (mccjit_embed.c:539-554): per-slot runtime hot state —
  `slot,blob,len,baseline,threshold,count,promoted,building` + KGC profile
  (`argseen,nsample,argmin[],argmax[],sample[][]`, `lock`). The hot path + the
  profile that localizes a hot slice (C1c) both live here.
- `MccjitSwapJob` (mccjit_embed.c:636-646): pool work item.
- Recompile entry: `mcc_jit_recompile_blob(buf,len)` (mccjit_embed.c:284) ->
  `mccjit_recompile_common` (161) -> `mccjit_intent_deserialize` (192) ->
  `ast_reemit_extern(sym, it.arena)` (mccjit_embed.c:71 decl; mccast.c:13892 def).
- `ast_reemit` (mccast.c:13794) is a PURE REPLAY — it does NOT run
  `ast_run_strat_cycle` (mccast.c:11596), so varying `ast_*_env` gates alone changes
  nothing. Optimized variants must transform the arena first.
- Transform-and-score primitive already exists: `ast_search_score_one`
  (mccast.c:12497) = clone pristine arena -> `ast_search_gates_set(mask)` (12274) ->
  `ast_run_strat_cycle` -> cost. The searchable `AST_SG_*` vocabulary is built in
  `ast_search_select` at mccast.c:12844 (`combo_run` enumerates it).
- Slice kit (M5c): `ast_slice_extract` (mccast.c:169), `ast_slice_certifiable`
  (12095), `ast_slice_equiv` (12105), `ast_slice_live_ins`, `ast_slice_copy_into`.
- Const-evaluator: `src/ast_eval_slice.h` (`ast_eval_binop`, `ast_eval_slice`,
  UB-soundness gated by `MCC_AST_JIT_EVAL_GATE`, refusals via
  `ast_jit_eval_refused_count`). Partial param->const fold: `mccjit_ast_spec_fold`
  (mccast.c:13896).
- Thread pool: `mccjit_pool_start` (mccjit_embed.c:711), `mccjit_pool_enqueue`
  (741), `mccjit_job_run_lazy` (758), promote seam `mccjit_counter_tick` (790).
- Fitness / swap / budget: `mccjit_bench_admit` (560), `mcc_jit_publish` (73/488),
  `jit_max_duration` currently a skip-if-over guard (mccjit_embed.c:432,466; passed
  from `s1->jit_max_duration` at 1345) — REPURPOSE as the CLOCK_MONOTONIC search
  budget for the pool loop (NOT `ast_now_ms`'s `clock()`).

STEP 0 — always-on JIT. `-run`/MEMORY JIT is already default-on (`MCC_JIT_DEFAULT=1`;
`mccjit-boot` fires with no `MCC_JIT`; `MCC_JIT=0` disables). Remaining: file-embed
slice (`s->embed_jit` 0->1 at libmcc.c:962 + `ast_jit_env` at mccast.c:1347) links the
~800KB engine into every EXE/.o and historically broke ~25 tests (KGC variants
segfault on real programs) — gate behind the full validation sweep; keep `MCC_JIT=0`
/`--no-jit`/`--no-embed-jit`. The in-process `MCC_AST_SEARCH` self-host SEGV that
blocked Phase 2 is FIXED (a7c38f1a), so the in-process search path is usable now.

STEP 1 [DONE — backend override seam: ast_reemit_with_gates (42fded0e) + mcc_jit_submit_ast/per-sym table/recompile integration (597afebb) + validation jit/selftest-reemit-gates (c6e9f952) + jit/selftest-submit; default-off, fixpoint byte-identical. Same-process (-run/MEMORY); file outputs ship via serialization. Immediate pool-triggered swap-on-submit DEFERRED as non-primary: for the AOT-backend flow the compile-time submit precedes the runtime .init_array install, so the override applies on the natural hot-recompile — only a runtime-resubmit case needs immediate swap.] — backend override API + per-sym override table + threaded submit (on the
existing whole-function recompile; proves the seam end to end).
  1a. New `ast_reemit_with_gates(Sym *sym, AstArena *arena, unsigned gate_mask)` in
      mccast.c: clone `arena`, `ast_search_gates_set(gate_mask)`, `ast_run_strat_cycle`,
      then emit to `cur_text_section` like `ast_reemit` (factor the shared tail out of
      `ast_search_score_one`, but emit instead of score). Exported via mccast.h.
  2a. Per-sym override table in mccjit_embed.c: `struct { int64_t tokv; AstArena *ast;
      unsigned gate_mask; int flags; }` array + rwlock. Public:
      `int mcc_jit_submit_ast(Sym *sym, AstArena *ast, unsigned gate_mask, int flags);`
      dedup-insert (key = sym token v) then `mccjit_pool_enqueue` a job carrying the
      slot for `sym`. `void mcc_jit_engine_config(...)` registers the eval hooks
      (ast_eval_slice + MCC_AST_JIT_EVAL_GATE oracle, A3a) the engine may call.
  3a. In `mccjit_recompile_common` before the deserialize at :192, look up the sym in
      the override table; on hit use the submitted `ast` (skip deserialize) and route
      through `ast_reemit_with_gates(sym, ast, gate_mask)`; on miss keep the shipped
      `MccjitIntent` path (B1a). PRECEDENCE: override > shipped intent, per sym.
  4a. Threading: the submit job runs on the pool (mirror `mccjit_job_run_lazy`),
      verifies via `mccjit_bench_admit` + KGC, swaps via `mcc_jit_publish`. Never
      blocks the driver. Default OFF (no submissions) => `fixpoint-invariant`
      byte-identical unchanged. Add `jit/selftest-submit-ast` (submit an AST, assert
      the swap + a correct result).

STEP 2 [in progress: ast_jit_const_fn DONE + ast_jit_fold_consts DONE (walks arena, rewrites free-var-free int Binary/Unary nodes to VT_CONST literals; Convert excluded=void-eval trap; validated end-to-end by jit/selftest-fold-consts forcing it through every recompile incl spec-fold, all selftests correct; note: compiler pre-folds plain source so it fires only post-transform e.g. after spec-fold). (iv) DONE — AOT->engine submit seam: ast_jit_submit_aot (mccast.c, at the mccjit_embed_note site in ast_func_end) hands the live per-function arena to the engine via mcc_jit_submit_ast when MCC_JIT_SUBMIT_AOT is set (default OFF -> fixpoint byte-identical, dormant). CRITICAL FINDING: a raw AstArena clone is NOT portable across MCCState contexts — every function references its PARAM syms (and any callees/globals) as live Sym* POINTERS, dead by runtime-recompile time; a naive reemit of a cloned arena segfaults (validated: full-mask reemit of glibc stdio inline stubs crashed). The Sym*->name-handle remap that makes an arena portable is EXACTLY what mccjit_intent_serialize/_deserialize already do. So the override table now stores a SERIALIZED BLOB (mcc_jit_submit_ast serializes at submit time, when the Sym*s are live; mccjit_override_put stores the blob; mccjit_recompile_common re-deserializes the override blob in place of the shipped intent when it.anchor_sym_v matches, override wins per B1a). Zero-serialization live-AST hand-off would require recompiling in the ORIGIN state or replicating the remap — deferred; serialize-on-submit is the sound path and reuses the ship-to-disk machinery. Validated: jit/selftest-submit (baseline f(5)=1005 -> submit x*3 override -> f(5)=15 -> clear -> 1005) + regression/o4-aot-jit Part 2 (busy submitted + mccjit-override[busy] fires + correct 731672571124) + fixpoint byte-identical + 52/52 jit/embed/regression. NOTE for the AOT-eval pillar: because serialize needs live Sym*s, the arena MUST be submitted during compilation (ast_func_end), not reconstructed later. REMAINING: (i) an AOT -O4 pass that const-folds pure slices INTO the submitted arena before submit (today the submitted arena == the faithful arena, so the override is behaviorally equal to the shipped path — the seam is proven, the refinement is the fold); (ii) JIT-execute non-trivial pure slices — the ESCAPE-ANALYSIS half is now DONE (ast_fn_purity_noescape, b5fff356: non-escaping local stores no longer force IMPURE, so loop accumulators certify; jit/selftest-noescape); what remains is the bounded UB-sound loop interpreter that consumes it (ast_eval_slice evaluates no loops yet); (iii) pool dispatch (threaded A2a).] — AOT `-O4`+ JIT-evaluator-guided backend, threaded (pillar A, A1c/A2a/A3a).
  In the `-O4` search, for each certifiable pure slice (`ast_slice_certifiable`)
  dispatch a POOL job that either runs `ast_eval_slice` (cheap, A1c) or JIT-EXECUTES
  the slice for a concrete constant (expensive, A1c), UB-gated by
  MCC_AST_JIT_EVAL_GATE (A3a). Fold results into the working arena
  (mccjit_ast_spec_fold-style), then hand the refined arena to the engine via
  `mcc_jit_submit_ast` so ONE dynamic AST drives both the AOT baseline and the runtime
  hot path (no serialization on this seam; serialization stays only for the
  ship-to-disk cold baseline). Reuse `mccjit_pool` for the eval jobs.

ENABLER DONE (runtime search foundation): mcc_jit_recompile_blob_gated(blob,len,gate_mask) recompiles applying a gate mask via ast_reemit_with_gates (globals mccjit_recompile_use_gates/gate_mask; recompiles are serialized by the global compiler state so no lock needed). jit/selftest-gated proves variant diversity: f recompiled under masks {0,1,0x20001,all-48-bits} all correct. This is what the runtime perm x combo x strategy loop calls per candidate. LOOP STRUCTURE DONE: mccjit_search_masks(blob,len,masks[],nmask,budget_s,&best_mask,&tried) — budget-bounded (CLOCK_MONOTONIC) gate-mask enumeration via mcc_jit_recompile_blob_gated; jit/selftest-search proves it (128 enumerated with no budget, ~13 under a 3ms budget, all winners correct). Selection is first-valid placeholder (emit-size blocked by the text_section state-macro; bench needs a live st). LIVE (opt-in MCC_JIT_SEARCH): mccjit_lazy_search(st,&routed) wired into BOTH promote seams (sync mccjit_counter_tick + async mccjit_job_run_lazy). Per hot function it recompiles a gate-mask vocabulary via mccjit_lazy_build_masked (variant gated, KGC baseline UNGATED/trusted), mccjit_bench_admit picks the fastest, bounded by MCC_JIT_SEARCH_MS (default 50ms) wall-clock. jit/selftest-search-live (all selftests under MCC_JIT_SEARCH=1, hot-threshold 2) promote correct KGC-verified functions. Default path (no env) = mccjit_lazy_build_masked(...,use_gates=0) identical -> fixpoint byte-identical. THE JIT NOW RUNS THE PERM x COMBO x STRATEGY SEARCH ON HOT FUNCTIONS AT RUNTIME. Broadly validated: jit/selftest-search-live-{kgc,struct,ptrret,purity,pool,lazy} all promote correct KGC-verified variants under the live search (7 shapes: int/kgc/struct-ret/ptr-ret/purity/async-pool/lazy). Vocab DONE: ast_jit_search_vocab (mccast.c) builds a principled candidate set from the real AST_SG_* gates (0, an -O2-ish base, and base + each of DIVMAGIC/ABS/REASSOC-family/NARROW-fix-family/SETHILEAF/LTEMP-IVSR-PRE); mccjit_lazy_search uses it. Live-search selftests promote correct with it; fixpoint byte-identical. Budget-from-jit_max_duration DONE (D2): the generated .init_array ctor bakes mccjit_set_search_budget(jit_max_duration) (sync+async) into a runtime global; mccjit_lazy_search uses it (MCC_JIT_SEARCH_MS env overrides for tests, else jit_max_duration, else 50ms). 86/86 incl embed + fixpoint byte-identical. Vocab widened to single-gate NEIGHBOURS of the -O2 base (~22 masks: 0, base, base^each of 20 searchable gates) so the search explores a real local neighbourhood; async/sync budget split: the async pool-thread search uses the full baked jit_max_duration (safe, non-blocking, background), the rare SYNC fallback is capped at 0.1s so it can't hang the hot path. Live-search + pool selftests promote correct; 86/86 incl embed + fixpoint byte-identical. HONEST SCOPE: the ~22-mask pass is ~20ms, so today jit_max_duration is a SAFETY CAP, not the driver — the search finishes on vocab-exhaustion first. COMBINATORIAL VOCAB + LIVE INCREMENTAL HOT-SWAP DONE: ast_jit_search_vocab now emits singles AND pairs of gate toggles (~212 masks, capped 256); the async pool-thread search publishes each improving variant to the slot AS IT FINDS IT (mcc_jit_publish per bench-admit win) so the running program hot-swaps to better code DURING the search, all bounded by jit_max_duration. 86/86 incl embed + fixpoint byte-identical. This is the practical realization of 'keep optimizing at runtime until timeout' on the background thread. Not done: true cross-pass accumulation across SEPARATE hot promotions (relax the promote-freeze so a function re-searches over multiple hot episodes) — marginal given a single async pass already sweeps the combinatorial space bounded by the budget. (c) Step 3 partial-slice reconcile remains.

STEP 3 [LIVE WIRING DELIVERED 2026-07-18 — the slice pillar now fires at the promote seam. Both promote seams route through mccjit_lazy_entry (mccjit_embed.c); opt-in MCC_JIT_SLICE_SEARCH (async-pool path only) engages mccjit_slice_search: deserialize the shipped intent -> ast_slice_search localizes+certifies the return slice (gated to AST_PURITY_TIER0 so the kernel reemit is the whole function by construction) -> ast_slice_wrap_kernel -> mccjit_reemit_arena_blob reemits the fn from that kernel -> DIFFERENTIAL VERIFY against a faithful whole-fn reemit of the same intent over every captured live-in tuple (mismatch -> discard/deopt) -> publish via mccjit_make_trampoline (leading-`leave`, slot-enterable exactly like the whole-fn search winner). Correctness net is the faithful-vs-kernel differential, NOT a direct baseline call (the shipped baseline is a dispatch-only KGC stub; direct-calling it corrupts the stack — see mcc-jit-kgc-stub-dispatch-only). Validated: jit/selftest-slice-live (TIER0 int fn promotes a verified slice-kernel, correct over 6 live-ins, stable) + 54/54 jit/embed/regression + 62/62 ast/fixpoint incl fixpoint byte-identical (default-off => path dormant). The machinery selftests (extract/oracle/kernel/reemit/install/search) still pass.
  RESIDUAL (deferred, honest scope): true in-arena sub-region splice for slice ⊊ body (C2b reconcile) — replace a hot sub-slice while KEEPING the surrounding impure ops. Needs a node-identity-stable arena-splice primitive (no public splice-back exists; ast_slice_copy_into is static + append-only) and neighbour reconciliation. Today the slice path engages only when the certifiable slice spans the whole function (TIER0), where kernel-reemit == the function; genuine partial replacement of a hot sub-region within an impure function is the remaining lowest-ROI piece over the working whole-function live search.]

STEP 3 — partial-slice hot-swap + reconcile (pillar C, C1c/C2b/C3c/C4b) on the
working whole-function seam.
  C1c adaptive: use the `MccjitCounterState` profile (sample/argmin/argmax) to localize
      a hot sub-region; `ast_slice_extract` that kernel; else whole function.
  C3c strategy: slice-local `combo_run` over the searchable set (mccast.c:12844),
      seeded from the function's current-best gate mask.
  C2b reconcile: after optimizing the slice, `ast_slice_equiv`-verify the boundary and
      splice with fixups (`ast_slice_copy_into` + `ast_slice_live_ins`) — no neighbour
      re-opt.
  C4b correctness: `ast_slice_equiv` structural check + KGC differential per swapped
      slice; deopt-to-baseline (`mcc_jit_publish(slot, baseline)`) on any mismatch.
  Budget: `jit_max_duration` on CLOCK_MONOTONIC bounds the pool search loop. Stats:
  candidates-evaluated / best-kept counters in mccstats.c mirroring
  `mcc_stats_search_end`, surfaced under `--stats=2`.

Every step gated by the validation sweep below. Serialization is retained ONLY for
the ship-to-disk cold baseline (pillar B); the AOT->runtime hot seam is live AST.

Validation gate for any phase: `fixpoint-invariant` byte-identical, exec
differential JIT-on == `MCC_JIT=0`, cross-arch goldens, no crashes on real
programs (the historical default-on failure mode).
