# TODO.md — settled gate work, in order of complexity

The decided, no-further-discussion items migrated out of
[GATED.md](GATED.md) §7 (audit of 2026-07-09, commit `eb1fd4ef`). Ordered
simplest → most complex; do them top-down. Items still under discussion
(the `s->objfmt` arc, format gate unification, feature-gate simplification,
enforcement tripwires, TargetDesc, DEGLOBAL.md scope) stay in GATED.md /
their plan docs and are **not** listed here.

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

- [ ] `mccgen.c:98` — collapse `precedence_parser` to always-on; delete the
      legacy `expr_prod`/`expr_sum` cascade under `#ifndef precedence_parser`
      (`mccgen.c:7944`) after confirming nothing else references it.
- [ ] `mccgen.c:9448` — `init_assert()` on/off pair: tie to `NDEBUG` instead of
      a hardcoded `#if 1 / #else` no-op define.
- [ ] `mccpe.c:555` — keep the live ELF→COFF export loop, delete the `#if 0`
      `.file`-aux variant at `mccpe.c:545`, drop the gate.
- [ ] `mccpe.c:1140` — keep the `.def`-export branch, delete the disabled trace
      variant at `mccpe.c:1107`, drop the gate.
- [ ] `mccmacho.c:2001` — chained-fixup pointer format choice
      (`DYLD_CHAINED_PTR_64` vs `_64_OFFSET`): keep both branches but select by
      a *named* constant, not a bare `#if 1`.

## 5. Purge the obvious dead `#if 0` blocks (small)

Delete outright (verify each still-referenced-nowhere while removing):

- [ ] `mcc.h:1847` — disabled duplicate tool prototypes.
- [ ] `i386-gen.c:759` — old `gjmp_cond_addr`, superseded by `gjmp_cond`.
- [ ] `mccpp.c:1059` — unused `tok_size()` helper.
- [ ] `mccpe.c:317` — unused `pe_sec_flags[]` table.
- [ ] `arm-gen.c:172` — doubly-nested FPA/OABI deprecation warnings.
- [ ] `mccgen.c:2286` — old EQ/NE shortcut, superseded by `gvtst_set`.
- [ ] `mccgen.c:10865` — stray `if(tok=='{') expect(";")` fragment.

(The dormant *trace* blocks — `mccgen.c:521,725`, `mccelf.c:655`,
`mccpe.c:1107,1354` — are handled by item 9, not deleted here.)

## 6. Sanitizer pairing for the memory debuggers (small, CMake)

- [ ] Auto-define `MEM_DEBUG` and `SYM_DEBUG` for the `mcc_s` sanitizer target
      (`CMakeLists.txt` ~:2237 area) so they stop being source hand-edits.
      Keep them off every other target — their presence changes allocator
      layout/locking. Gate: `sanitize` preset builds + sanitize-smoke.

## 7. Investigate, then resolve, the three non-obvious `#if 0` blocks (medium)

Finish-or-purge decisions; the investigation *is* the task:

- [ ] `mccpp.c:1302` — `TOK_GET` function-call variant vs the inline fast
      macro: confirm full equivalence, then delete the gate and the slow path.
- [ ] `mccelf.c:3587` — disabled `DT_RPATH`/`DT_NEEDED` processing loop: verify
      the live dll-ref resolution covers every case it handled; if yes delete,
      if no finish it (it predates the current loader logic).
- [ ] `mccrun.c:623,691,747` — the abandoned DWARF directory-table path in the
      `-run` backtrace line-info parser: completing it yields correct backtrace
      paths for sources compiled outside the cwd. Decide finish vs purge by
      writing the failing case first (a `-bt` test with `-I`-relative sources).

## 8. `CONFIG_RUN_MMAP_EXEC` → runtime probe (medium)

- [ ] In `host_runmem_alloc`/`host_runmem_protect` (`mcchost.c:952-983`):
      attempt plain `mmap(RWX)`/`mprotect(RWX)`; on `EACCES`/`EPERM` fall back
      to the file-backed dual-mapping path; probe once, cache the result.
      Remove the "rebuild with -DMCC_RUN_MMAP_EXEC" failmsg (`mcchost.h:252`);
      keep the CMake knob only as a force-override. Constraint: the fallback
      must be chosen before `mcc_relocate_ex`'s two-pass layout
      (`mccrun.c:265,296`) since `ptr_diff` affects addressing. Gate: `-run`
      tests on Linux + a forced-fallback run (`MCC_RUN_MMAP_EXEC=ON` build or a
      seccomp/SELinux sandbox if available).

## 9. Runtime `--debug=<cat>` diagnostics (medium-large)

Policy (final): every trace/dump *implementation* compiles always; activation
is runtime via a category bitmask over the existing `g_debug` int
(`-d<num>`, `libmcc.c:59,2146`).

- [ ] Define categories: `reloc, inc, pp, struct, tok, pe, ver, asm` (+ `sym`
      for the dormant symbol traces). Map `--debug=a,b` → `g_debug` bits;
      keep `-d<num>` as the raw form.
- [ ] Convert the eight macro families: `DEBUG_RELOC` (also drop the force-
      `#undef` at `mccelf.c:3`), `INC_DEBUG` (delete — fold unique messages
      into the existing `-vv` include tree at `mccpp.c:1495`), `PP_DEBUG`
      (keep the `PP_PRINT` wrapper; fast path pays one branch), `BF_DEBUG`,
      `PARSE_DEBUG`'s token-echo half (`mccpp.c:3515`; the `len=1` half stays
      compile-time), `PE_PRINT_SECTIONS`, `DEBUG_VERSION`, `ASM_DEBUG`'s trace
      half (its "bad op table" checks at `i386-asm.c:951`, `mccasm.c:1445-1491`
      become always-on cheap asserts).
- [ ] Convert the dormant `#if 0` traces worth keeping (`mccgen.c:521` pv/psyms
      dumps → `sym`, `mccelf.c:655`, `mccpe.c:1354`); delete the rest
      (`mccgen.c:725`, `mccpe.c:1107` — superseded by categories above).
- [ ] Document in `mcc -hh`; bench before/after on the mcc rows (expected ~0:
      all sites are cold or one-branch guarded).

## 10. Structural ungating where free (medium-large)

- [ ] Delete `NEED_RELOC_TYPE`/`NEED_BUILD_GOT`: compile `code_reloc`,
      `gotplt_entry_type`, `create_plt_entry`, `relocate_plt`,
      `build_got_entries` unconditionally in all five `*-link.c` + the
      `mccelf.c:1140` consumer; remove the definitions at `mcc.h:1664-1669`.
      Cost: a few KB of unused code in PE/Mach-O builds. riscv64-link.c already
      lives this way (it never had the `NEED_RELOC_TYPE` guard).
- [ ] Convert the three `#if SHT_RELX == SHT_RELA` compile branches
      (`mccelf.c:697,1014,1845`) to runtime `if` on a has-addend predicate —
      the fourth site (`mccelf.c:700`) already does exactly this. The
      `ElfW_Rel` *type* stays compile-time (no runtime type selection in C).
- [ ] Gate: full local matrix (PE + Mach-O cells are where unused-function
      fallout appears) + per-target `mcc -c` object comparison (must be
      byte-identical — no executable text changes).

## 11. Execute SPLIT.md (largest of the settled set)

- [ ] The `TARGET_DEFS_ONLY` → `<arch>-<part>.h` split, per
      [SPLIT.md](SPLIT.md) §4-6: 13 headers created, 13 `.c` skeletons
      removed, `mcc.h:217-239` ladder rewritten, macro gone from the tree.
      Gates as specified there (matrix, ctest incl. drift invariants,
      self-host fixpoint, per-target byte-identical objects).
      Sequencing: land before any DEGLOBAL.md stage begins — both touch every
      backend file.

## 12. Rename every PP gate macro to the naming standard (widest surface — run LAST)

Deliberately last: items 1-11 delete whole macro families outright
(`TARGET_DEFS_ONLY`, `USING_GLOBALS` via DEGLOBAL, the debug traces,
`NEED_*`, the `#if 0/1` toggles), so the rename surface shrinks with every
item above. Supersedes GATED.md item 9(a)'s narrower `CONFIG_AST` rename.

**The rules:**

- [ ] **Public** (user/build-facing, settable via CMake or `-D`):
      `MCC_CONFIG_<WHAT_IT_DOES>`. **Private** (derived, per-target constants,
      structural): `MCC_<WHAT_IT_IS>`. Nothing gate-like without one of the
      two prefixes.
- [ ] Names state **what the feature does as implemented** — plain English,
      not intent, not history, not relative terms. The repeat offenders:
      `CONFIG_NEW_MACHO` ("new" relative to what?) → e.g.
      `MCC_CONFIG_MACHO_CHAINED_FIXUPS` (that *is* what it selects);
      `ELF_OBJ_ONLY` (reads as "only ELF objects", means "this format emits
      ELF *only as* `-c` objects, no ELF executables") → dies into `s->objfmt`
      or renames to say the true condition; `CONFIG_RUN_MMAP_EXEC` (names the
      syscall, not the behavior) → `MCC_CONFIG_RUN_DUALMAP` or similar once
      item 8's probe lands; `MCC_IS_NATIVE` → `MCC_TARGET_IS_HOST` (that is
      the actual test, `mcc.h:63-75`); `PROMOTE_RET` → `MCC_RET_PROMOTES_INT`
      (small integer returns are promoted); `SINGLE_SOURCE` →
      `MCC_AMALGAMATED` (one TU is what it does).
- [ ] **No derived negatives**: `MCC_DISABLE_ASM` (the inverse shadow of
      `CONFIG_MCC_ASM`, `mcc.h:101-103`) is deleted, one positive macro
      remains. Same for any `*_ONLY`/`*_OFF` shadows found in the sweep.
- [ ] **One test idiom** per macro kind (boolean config: `#if`, never mixed
      `#ifdef`/`#if defined(X) && X` — folds in the idiom half of GATED.md
      item 9(a)).
- [ ] **Prefix flip**: today the C macros are `CONFIG_MCC_*` while the CMake
      options are already `MCC_CONFIG_*` (`MCC_CONFIG_ASM` option →
      `CONFIG_MCC_ASM` define) — the *CMake side wins*; the C side renames to
      match. Every `CONFIG_*` without the `MCC_` root renames too
      (`CONFIG_AST` → `MCC_CONFIG_AST`, `CONFIG_SYSROOT` →
      `MCC_CONFIG_SYSROOT`, `CONFIG_DWARF_VERSION`, `CONFIG_CODESIGN`,
      `CONFIG_MCCBOOT`, `CONFIG_TRIPLET`, `CONFIG_MCCDIR`,
      `CONFIG_OS_RELEASE`, …). Private per-target constants gain the `MCC_`
      prefix (`PTR_SIZE` → `MCC_PTR_SIZE`, `LDOUBLE_SIZE`, `NB_REGS`,
      `RC_*`/`TREG_*` stay as-is only if kept backend-private after SPLIT —
      decide per family during the sweep, but no unprefixed name may cross a
      file boundary).
- [ ] **CMake refactor where it contradicts**: `CMakeLists.txt` emission
      blocks (`:1638-1749` area) emit the new names; `mcc_config_node` names,
      `CMakePresets.json` cache vars, and the `tools/ci.c` preset ledger stay
      in lockstep (one atomic commit — no alias/transition period; the presets
      are the single source of truth). NOTE: CMakeLists.txt stays
      comment-free.
- [ ] **Tooling follows**: `tools/ckconfig.c` scanner prefix `CONFIG_MCC_` →
      `MCC_CONFIG_` — which automatically brings the formerly-unprefixed
      macros under drift audit (closing the ckconfig blind spot from GATED.md
      §3); `tools/hostgate.c` untouched (bans *system* macros, which don't
      rename); grep-gates in CI: zero hits for `CONFIG_MCC_`, bare
      `CONFIG_[A-Z]`, and the retired names.
- [ ] Gates: full local matrix + qemu spot-check, `config-drift-invariant` +
      `host-gate-invariant` + `preset-parity-invariant` ctests, 3-stage
      self-host fixpoint, and per-target `mcc -c` byte-identical objects
      (renames move no code). Docs sweep last: MCC.md/EXCESS.md/BUILD.md
      references update in the same commit.
