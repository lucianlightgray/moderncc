# TODO.md — settled gate work, in order of complexity

The decided, no-further-discussion items migrated out of
[GATED.md](GATED.md) §7 (audit of 2026-07-09, commit `eb1fd4ef`; decision
rounds 1-2 of 2026-07-09 folded in). Ordered simplest → most complex; do them
top-down. Still open elsewhere: DEGLOBAL.md's stage-2 spelling (decided at
Stage-1 fallout), and the deferred/closed GATED.md items (format arc,
TargetDesc) which are **not** work.

Standing gates for every item: `ctest` green; items touching `src/` also need
the 3-stage byte-identical self-host fixpoint; items touching backend or
format files add the local matrix / a qemu spot-check as noted.

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
      [SPLIT.md](SPLIT.md) §4-6: 13 headers created, 13 `.c` skeletons
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

- [ ] Add `host_cache_dir(char *buf, int size)` to `mcchost.*` that
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
- [ ] Compute a stable **intention hash** over the captured AST tree
      (kinds + ops + type/sym/const payloads in canonical child order —
      the replay-relevant fields, *not* addresses or capture-order slot
      ids) so it is reproducible across runs and processes. This is the
      cache key; a matching hash means "same intention, reuse the prior
      optimizer result", a miss means "new intention, search fresh".
- [ ] Cache entry per key = the previously JIT-optimized version of that
      function (the relocatable/position-independent code bytes the
      hypervisor produced) **plus the last iteration attempt's optimizer
      inputs** (the search state / permutation weights / gate settings
      that produced it) so the next run warm-starts from where the last
      search left off rather than re-deriving it. File is keyed under the
      cache dir by `MCC_VERSION` + target triple + intention hash so a
      compiler-version or target bump can't load a stale/foreign blob.
- [ ] The hypervisor (`tools/mcchv.c`) reads the cache on start (seed the
      pattern permutation and JIT kernel from the stored entry when the
      intention hash matches) and writes back the improved version at the
      end; a cold miss reproduces today's from-scratch behavior exactly.
      Ship a cli/unit test: resolve dir, round-trip an entry, prove a hash
      match warm-starts and a hash mismatch (edited intention) misses, and
      skip cleanly where no writable home exists.
