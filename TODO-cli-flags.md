# TODO — promote gcc/clang-style features to command-line arguments

Goal: make mcc honor the gcc/clang command-line flags that are currently either
baked in at build time (`CONFIG_*` / `#ifdef`), half-wired, or silently ignored —
plus add the common gcc/clang flags mcc lacks entirely.

Derived from the build-time-vs-runtime audit. Anchors are `file:line` at time of
writing; re-grep before editing.

---

## STATUS (implemented)

Done and tested (native 18/18 + regression test `tests2/197_cli_std_flags.c`):
- **T1.1** `-std=` full set (no `gnu_ext` toggle — it also gates `__asm__` etc.
  that system headers need; only `cversion`/`__STDC_VERSION__` is set).
- **T1.2** `-b`/`-bt` always parse; graceful error when not built in.
- **T2.1** `-O0..9`/`-Os`/`-Oz`/`-Og`/`-Ofast` + `__OPTIMIZE_SIZE__`.
- **T1.4** `-pie`/`-no-pie` toggle ET_DYN/ET_EXEC output.
- **T1.3** `--sysroot=`/`-isysroot` (runtime `{R}` expansion).
- **N.3** ignored flags already report under `-Wunsupported`.
- **NW.1-5** real `-fshort-enums`; `-fwrapv`/`-fno-builtin`/`-fomit-frame-pointer`/
  `-ffunction-sections`/`-fdata-sections` accepted.
- **NW.6** `-fvisibility=` (+ `SymAttr.visibility_set` so explicit `default` wins).
- **T3.2** `-Wl,--disable-new-dtags`.
- **NW.8/9** `-march=`/`-mtune=`/`-mcpu=`/`-mcmodel=`/`-mfpmath=` accepted.

- **NW.7** `-fstack-protector[-strong|-all]` — **implemented for x86_64 ELF**:
  prolog stashes the `%fs:0x28` guard in a frame slot, epilog xors it back
  (via `%rcx`, since `%rax`/`%rdx` hold the return value) and calls
  `__stack_chk_fail` on mismatch. Tested end-to-end (real `*** stack smashing
  detected ***`) + a clean-run golden. `-fno-stack-protector` disables; on
  non-x86_64/Mach-O it reports under `-Wunsupported`. The `-strong`/`-all`
  variants currently all behave as `-all` (every function protected); the
  array-only heuristic is a future refinement.

Partial / deferred (CLI surface done; deep codegen intentionally not shipped):
- **N.2** `-s` — reports under `-Wunsupported`; real symtab stripping deferred
  (truncating `.symtab` corrupted the image; needs proper section removal).
- **T1.5** `-fPIC`/`-fPIE`/`-fpic`/`-fpie`/`-fno-pic`/`-fno-pie` accepted; x86_64
  is position-independent already (`-fPIC -pie` runs as a PIE). The i386/arm
  `#if defined CONFIG_MCC_PIC` codegen→runtime conversion is the remaining deep
  work, deliberately not attempted: it's **untestable in this environment** (no
  i386 multilib/libc, so i386 PIC binaries can't be run/verified) and the
  refactor would risk regressing the i386 backend with no test coverage.

---

## Common implementation pattern (read first)

Adding/upgrading a CLI flag almost always touches the same five places:

1. **Option table** — `src/libmcc.c` `mcc_options[]` (~line 1560): add
   `{ "name", MCC_OPTION_xxx, <flags> }`. New enum value in `MCC_OPTION_*`
   (~line 1500). Use `MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP` for `-On`-style
   attached args, `MCC_OPTION_HAS_ARG` for separated args.
2. **State field** — `src/mcc.h` `MCCState` flag block (~lines 670–722) for a new
   `unsigned char`; or reuse an existing field.
3. **Handler** — the big `switch` in `mcc_parse_args` (`src/libmcc.c` ~1860–2100):
   `case MCC_OPTION_xxx:` setting the state.
4. **Boolean `-f`/`-m` flags** — instead of 1–3, just add a row to `options_f[]` /
   `options_m[]` (`offsetof(MCCState, field)`, optional `FD_INVERT`); the generic
   `set_flag` path handles `no-` prefixing for free.
5. **Help + tests** — `src/mcc.c` `help[]` / `help2[]`; add a `tests2/` case (run
   or `dt` mode) and, for diagnostics, an entry in `60_errors_and_warnings.c`.

No test currently snapshots `--help`, so help text is free to edit.

---

## Tier 1 — build-time-baked features that gcc/clang expose at runtime

### [ ] T1.1 `-std=` full standard set  *(start here — cheapest, self-contained)*
gcc/clang accept `c89/c90/c99/c11/c17/c18/c23` + `gnu89/gnu99/gnu11/gnu17/gnu23`.
mcc only recognizes `c11`/`gnu11` (`libmcc.c:1924`); everything else is a **silent
no-op**, leaving `cversion` at the C99 default (`libmcc.c:855`).

- [ ] Replace the `case MCC_OPTION_std` body with a table mapping each standard
      token → `cversion` value (`199409`, `199901`, `201112`, `201710`, `202311`).
- [ ] Set `s->gnu_ext` from the `gnu*` vs `c*` prefix (drives GNU extensions).
- [ ] Warn on an unrecognized `-std=` value (gcc errors; a warning is acceptable).
- [ ] Audit `cversion` consumers so newer standards actually gate features
      (`__STDC_VERSION__`, `_Static_assert`, etc.).
- [ ] Tests: `tests2/` run-mode checks of `__STDC_VERSION__` for each `-std=`.

### [ ] T1.2 `-b` / `-bt` always parseable (graceful unsupported error)
Whether these options even **exist** is gated by `CONFIG_MCC_BCHECK` /
`CONFIG_MCC_BACKTRACE` at build (`libmcc.c:1571–1575`, handlers `1867–1877`).
gcc/clang always parse `-fsanitize=…` and error clearly if unsupported.

- [ ] Move `{ "b", … }` / `{ "bt", … }` out of the `#ifdef` so they always parse.
- [ ] In the handler, if the corresponding runtime isn't compiled in, call
      `mcc_error("bounds checker not built into this mcc")` instead of falling
      through to "unknown option".
- [ ] (Optional, larger) Always compile the bcheck/backtrace runtime into
      `libmcc1` so the flags always *work*, not just parse.
- [ ] Tests: `dt`-mode error-message case for the unsupported path.

### [ ] T1.3 `--sysroot=` / `-isysroot`
`CONFIG_SYSROOT` is concatenated into the search-path **macros at compile time**
(`mcc.h:238/248/258`, `mccelf.c:3642`); there is **no runtime override**.

- [ ] Add `MCCState.sysroot` (char*) + options `--sysroot` and `-isysroot`
      (`MCC_OPTION_HAS_ARG`).
- [ ] In `mcc_set_output_type` / path-init, prefer `s->sysroot` over the
      compile-time `CONFIG_SYSROOT` when expanding include/lib/crt paths.
- [ ] Make the `{B}`/`{R}`/triplet path expansion consult the runtime sysroot.
- [ ] Tests: `-print-search-dirs` with/without `--sysroot=/tmp/x` shows the swap.

### [ ] T1.4 `-pie` / `-no-pie` (output-type half)
`-pie` is in the **ignored** list (`libmcc.c:1632`); PIE-by-default is forced by
`CONFIG_MCC_PIE` in `mcc_set_output_type` (`libmcc.c:929`). No way to toggle at runtime.

- [ ] Add real `MCC_OPTION_pie` / `MCC_OPTION_no_pie`; set/clear `MCC_OUTPUT_DYN`
      on the EXE output type.
- [ ] Make `CONFIG_MCC_PIE` only the *default*, overridable by `-pie`/`-no-pie`.
- [ ] Remove `-pie` from the ignored list.
- [ ] Tests: `readelf -h` of `-pie` vs `-no-pie` output (ET_DYN vs ET_EXEC).

### [ ] T1.5 `-fPIC` / `-fPIE` / `-fpic` / `-fpie` / `-fno-pic` (codegen — **large**)
The hard one. PIC codegen is hardwired by `#if defined CONFIG_MCC_PIC` across the
backends (`i386-gen.c` ~14 sites, `arm-gen.c:497`, `i386-link.c:17`). True runtime
`-fPIC` means turning those compile-time branches into runtime decisions.

- [ ] Add `MCCState.pic` (0/1/2 for none/pic/PIC) + `-f[no-]pic` / `-f[no-]PIE`
      rows (these can't be plain `options_f[]` rows since they affect codegen).
- [ ] i386 backend: convert each `#if defined CONFIG_MCC_PIC` to test `s->pic`
      at emit time (GOT/PLT sequences, `i386-gen.c:57/142/202/253/309/377/431/…`).
- [ ] arm backend: same for `arm-gen.c:497`.
- [ ] Linker: `i386-link.c:17` relocation handling under runtime `pic`.
- [ ] Keep `CONFIG_MCC_PIC` as the compiled-in *default* for `s->pic`.
- [ ] Tests: compile a GOT-referencing TU `-fPIC` vs `-fno-pic`, diff relocs;
      run a `-fPIC -pie` exe under qemu (ties into the cross harness).
- [ ] Note: x86_64 is largely RIP-relative already, so this is mostly i386/arm.

---

## Tier 2 — runtime flags that exist but are half-wired

### [ ] T2.1 `-O` level recognition
`s->optimize` is parsed (`libmcc.c:2062`) but only read at `mccpp.c:3357` to define
`__OPTIMIZE__`. `-Os/-Og/-Ofast` fall through `isnum()` → silently `optimize=1`.

- [ ] Recognize `-Os`, `-Og`, `-Ofast`, `-O` (=`-O1`) explicitly; map to a sane
      `optimize` value; define `__OPTIMIZE_SIZE__` for `-Os`.
- [ ] Document in `-hh` that `-O` only sets predefined macros (no optimization
      passes), to set expectations.

---

## Tier 3 — build-time defaults that could gain a runtime override (low value)

### [ ] T3.1 DWARF default version override
`CONFIG_DWARF_VERSION` sets the default (`mcc.h:1734`); `-gdwarf-<n>` already
overrides (`libmcc.c:1888`). Marginal — gcc has no "default version" knob. Skip
unless wanted.

### [ ] T3.2 `--disable-new-dtags` runtime parity
`CONFIG_NEW_DTAGS` is the build default; `-Wl,--enable-new-dtags` already works
(`libmcc.c:1407`). Add `-Wl,--disable-new-dtags` for symmetry.

### [ ] T3.3 libc flavor / `--target=`
`CONFIG_MCC_MUSL` / `CONFIG_MCC_UCLIBC` are build-time; clang folds these into
`--target=<triple>`. A real `--target=` is large (drives arch+os+libc+paths) —
track as its own epic, overlaps with the cross work.

---

## Notable — flags gcc/clang honor but mcc silently ignores
(`{ "x", 0, 0 }` rows, `libmcc.c:1628–1635`)

### [ ] N.1 `-pie` → see T1.4 (stop ignoring it).
### [ ] N.2 `-s` (strip symbols) — honor by skipping the symtab/strtab emission, or
      document as unsupported instead of silently accepting.
### [ ] N.3 Decide policy for `-pedantic`, `-pipe`, `--param`, `-arch`, `-traditional`:
      keep ignoring (current), but make sure `-Wunsupported` reports them.

---

## New work — common gcc/clang flags mcc lacks entirely

These are *new* behavior, not promotions. Group by effort.

Boolean `-f` flags (add an `options_f[]` row + honor the field):
- [ ] NW.1 `-fwrapv` — defined signed overflow (codegen: suppress UB assumptions).
- [ ] NW.2 `-fno-builtin` — stop recognizing builtin functions.
- [ ] NW.3 `-fshort-enums` — pack enums to smallest type (type layout).
- [ ] NW.4 `-fomit-frame-pointer` / `-fno-omit-frame-pointer` (codegen).
- [ ] NW.5 `-ffunction-sections` / `-fdata-sections` (section naming in objfmt).

Larger / valued:
- [ ] NW.6 `-fvisibility=default|hidden|protected` — default symbol visibility
      (set `STV_*` on emitted symbols; `mccelf.c`).
- [ ] NW.7 `-fstack-protector[-strong|-all]` / `-fno-stack-protector` — canary
      prologue/epilogue + `__stack_chk_*` refs; runtime support in `libmcc1`.
- [ ] NW.8 `-march=` / `-mtune=` / `-mcpu=` — at minimum parse + define the arch
      feature macros (`__SSE2__`, `__ARM_FEATURE_*`); full codegen tuning is large.
- [ ] NW.9 `-mcmodel=small|medium|large` — code-model-aware reloc/codegen (x86_64).
- [ ] NW.10 `-static-pie` — static + PIE output (depends on T1.4/T1.5).

---

## Cross-cutting (do alongside each task)

- [ ] Update `src/mcc.c` `help[]` / `help2[]` for every newly honored flag.
- [ ] Add `tests2/` coverage (run-mode for behavior, `dt`-mode for diagnostics);
      register goldens in `tests/tests2_data.h`.
- [ ] For anything affecting codegen/output, add a cross check via the freestanding
      `tests/complex-cross/` qemu harness where applicable.
- [ ] Keep `CONFIG_*` build macros as the compiled-in *defaults* once a feature is
      promoted (don't delete them — they become the fallback default for the new
      runtime field).
- [ ] `grep` the `-W[no-]unsupported` path so newly-parsed-but-unimplemented flags
      report consistently.

## Suggested order
T1.1 → T1.2 → T2.1 → T1.4 → T1.3 → N.2/N.3 → NW.1–NW.5 (easy `-f` batch) →
T1.5 (PIC codegen, large) → NW.6/NW.7 → NW.8/NW.9/T3.3 (`--target=` epic).
