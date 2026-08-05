# TODO

## The configuration surface moved: read this before running any recipe below

**Every `MCC_AST_*` and `MCC_RIR_*` gate is now a `-f` flag.** 113 of them, generated
from one list in `src/mccopt.h`, which also generates the driver's option table so the
two cannot drift. `-fno-<name>` turns a knob off, `-f<name>` on.

This matters for reading the rest of this file, because **a retired variable in the
environment is ignored silently** — no warning, no error, exit 0. A recipe that still
spells one will run, print a plausible board, and measure nothing. That failure mode
cost three separate defects during the conversion: the superopt search explored a
space where nothing changed and passed; `runtime-bench`'s ratchet read its win as
+0.0% instead of erroring; `optfire`'s extra-gate column stopped firing. The runnable
recipes in this file were repointed; prose that merely *names* a retired variable was
left as history.

Translation for the ones this file uses most:

| was | now |
| --- | --- |
| `MCC_RIR_NOFB=1` | `-fno-replay-fallback` |
| `MCC_AST_PROMOTE=0` | `-fno-promote-locals` |
| `MCC_AST_TEMPLATES=0` | `-fno-reemit-templates` |
| `MCC_AST_INLINE=0` | `-fno-inline` |
| `MCC_AST_SEARCH=1` | `-fopt-search` |
| `MCC_AST_NARROW_FIX=1` | `-fnarrow-fix` |
| `MCC_AST_BITFLAG=1` | `-ftree-switch-conversion` |

**Still environment variables, deliberately.** The numeric tuning knobs
(`MCC_AST_CSE_WINDOW`, `MCC_AST_TILE_SIZE`, `MCC_AST_INLINE_DEPTH`, …) and the RIR
measurement handles this file's censuses are built on — `MCC_REPLAY_IR`,
`MCC_RIR_PROD`, `MCC_RIR_FORCE`, `MCC_FORCE_REPLAY` — are unchanged, so every
`[rir-prod]` / `[rir-total]` recipe still works verbatim. So are the `MCC_JIT_*`
knobs: the embed JIT runs inside programs mcc *compiled*, where a compiler flag
cannot reach it, which is the same reason `OMP_NUM_THREADS` is an environment
variable.

**`-O` is a ladder now, not a dial.** 1–3 are the settled levels; **4–12 are one
in-development optimizer each**, cumulative; **13 and up run the strategy search with
the level as its budget in seconds**. Anything in this file that says `-O4` meaning
"search for 4s" now means "`-O3` plus one x86-only pass" — the search entry is
`-O13`. `MCC_OPT_SEARCH_LEVEL` in `mccopt.h` is the single definition, and
`ast_opt_defaults` refuses to build if a knob is ever placed at or past it.

A pass that is merely unfinished belongs on the ladder; one that is **known to
miscompile does not**. `-fjit-splice` was off-by-default, exercised by nothing, and
segfaults a threads program; putting it on rung 12 made every level above it emit
wrong code, and the suite caught it. It is back to opt-in.

**Compile-time is down to four switches** outside target/host: `MCC_DIAG`
(instruments mcc as it runs — allocator/Sym/optrace, plus the warning set and
`mcc_s`/`mcc_c`), `MCC_DEV` (differential self-checking that aborts on divergence —
the AST side-car coherence oracle, and the JIT fault injectors, which therefore
cannot be switched on in a shipped compiler), `MCC_CONFIG_TRACE` (`MCC_TRACE` alone,
~12,600 call sites), and `MCC_EMBED_JIT`. Deleted outright, now always compiled in:
`MCC_CONFIG_OPTIMIZER`, `MCC_CONFIG_DIAG_RT`, `MCC_CONFIG_LSP`, `MCC_CONFIG_ASM`,
`MCC_REPLAY_IR` (the compile-time half), `MCC_IR_CAPTURE`, `MCC_PROFILE` with
`mcc_p`. Their capabilities are reachable at runtime instead: `-O0`, `-b`/`-bt`/
`-fsanitize=bounds`, `--lsp`, `-fasm`/`-fno-asm`.

**Coverage owed.** `tests/optfire/flagsweep.sh` checks that every flag in the table
accepts both spellings (113/113) and that turning each on and off still computes the
reference answer on twelve exec goldens. The exec half is **not yet wired as ctest
cells** — that is the next job, and it matters: 43 of these flags are referenced
nowhere in `tests/`, `tools/` or `CMakeLists.txt`, which is exactly the population
`-fjit-splice` came from. The first version of that harness used two synthetic
programs and *passed* `-fjit-splice`; it now uses real goldens covering threads and
atomics, and fails it. A sweep that misses the bug you already have is worse than no
sweep, because it reads as coverage.

## Where things stand — 2026-08-04, `66df3c7e`

| Gate | State |
|---|---|
| board, gcc suite | `FAIL 449` / `FAILEXE 122` |
| board, llvm suite | `FAIL 226` |
| `ctest` | **8150/8157 — 7 deliberate reds**, all `run-tier/<triple>`, all `-run` TLS |
| `-O3` fixpoint | byte-identical, **3068543** in the primary checkout |
| cross / qemu / wine | 90/90 |
| tracegate, schemagate | OK |

**The 8145/8145 baseline is retired.** `run-tier` added twelve cells and seven of them
are red on purpose, against two real pre-existing `-run` TLS defects. Do not "fix" them
by weakening the corpus. Any *eighth* failure is a regression.

**Fixpoint sizes do not compare across trees.** A git worktree's longer path inflates
embedded strings: the same commit measures 3068543 in `/home/llg/Projects/mcc` and
3076911 under `.claude/worktrees/…`. The gate is `o1 == o2 == o3` **within one tree**.
An absolute size quoted from a worktree cost one agent a false discrepancy hunt.

### Landed today

`72fedcf1` four semantic gaps (board 471 → 449) · `7a31e90b` uninitialised `is_label`
· `d50c480d` bitfield side-car · `7bded795` `VT_BTYPE` widened to 5 bits ·
`2217535a` riscv64 `-run` veneer, arm64 `svc`, arm asm register numbering ·
`66df3c7e` the `run-tier` tier · plus the qemu-user bootstrap that made
`run-parity-arm`/`-arm64`/`-riscv64` execute at all.

### Open, roughly by value

1. **`-run` TLS, two defects** — the seven red cells. `tls_threads` fails on *every*
   runnable triple including x86_64: a `pthread_create`d thread reads 0 from every
   initialised `__thread` variable. `tls` additionally fails on `i386`, `arm`,
   `riscv64` and both PE targets, losing `.tdata` initialisers in the main thread.
   Neither is a JIT-tier bug; AOT is correct everywhere. Details below.
2. **`__bf16`** — the type word is no longer the blocker. Slots 16-31 are free. What
   remains is the encode/decode helper and per-backend ABI. **Do not alias it onto
   `_Float16`**: they differ only in the helper, so an alias miscompiles silently.
3. **Width-64 bitfields are never marked `VT_BITFIELD`** (`mccgen.c:6559`), the source
   of all ten gcc/clang bitfield deviations. The six-bit ceiling that forced it is gone;
   lifting it is now a layout change that needs its own differential.
4. **riscv64 aborts on every mixed int/float two-register struct return** —
   `arch_transfer_ret_regs` asserts `vtop->r == (VT_LOCAL | VT_LVAL)`, failing 7 of 37
   shapes. A hard abort, not a miscompile.
5. **arm64 `store()` does not strip `VT_MUSTCAST`** while its own `load()` does — the
   same shape as the mask bug that cost a cross-tier failure, one mask narrower.
6. **C23 structural tag compatibility** (8 files) — a subsystem, not a patch; a
   half-implementation hands the wrong `Sym` to later member lookups.
7. Smaller and precisely located: `register` array decay accepted where gcc/clang
   reject · `const`-qualified *parameter* assignment accepted (not complex-specific) ·
   `_Atomic _Complex ++` unsupported · one-argument `va_start` on i386, arm and
   x86_64-PE · the seven costed cluster root causes in the table further down.
8. **Unproven rather than open**: the Darwin branch of `run-tier.sh` has never
   executed — treat its first run on a Mac as untested. And `arm-win32`,
   `arm64-win32` and `arm-wince` have no runner on any host available here (wine
   emulates x86 PE only; qemu-user cannot load PE), so their `-run` paths are
   untested by anything and will stay that way without Windows-on-ARM hardware.

## `_Float16` landed on all five backends; `__bf16` is refused, and cannot be added without a type-word change

`_Float16` (IEEE binary16) is implemented and verified on x86_64, i386, arm, arm64
and riscv64. `__bf16` is **parsed and refused** with
`'__bf16' is not supported on this target`. That split is not a scoping preference,
it is forced by the type word, and the next person to pick this up needs the reason
before they try again.

**`VT_BTYPE` is 4 bits and slot 15 was the only one free** (`src/mcc.h:1081-1097`);
`_Float16` took it. A second base type needs a discriminator bit that survives
`t & VT_TYPE`, and there is none: `VT_TYPE` is `0x00060F7F` and every bit in it is
allocated. The three candidates all fail for the same reason — they are *stripped*
by existing normalizers, so `__bf16` and `_Float16` would silently compare equal and
share IR nodes: `VT_DEFSIGN` is cleared at `src/mccrir.c:1156` (the RIR type
normalizer) and at `src/mccgen.c:4120-4121` (type compatibility); `VT_LONG` is
cleared at `src/mccrir.c:1156` too; the `VT_STRUCT_SHIFT` region is masked out of
`VT_TYPE` by construction. `VT_UNSIGNED` is the only bit never stripped from a user
type (the two `~VT_UNSIGNED` sites, `src/mccgen.c:4359,4379`, act on a freshly
pushed `size_t` in pointer arithmetic, never a user type) — so it was the one viable
overload, at the cost of auditing its 264 read sites.

**That decision is now moot: `VT_BTYPE` has been widened to 5 bits** — see
"`VT_BTYPE` widened to 5 bits — step 2" at the end of this file. Slots 16-31 are
free, `VT_TYPE` is `0x000c1eff`, and no flag needs overloading. The remaining work
for `__bf16` is the encode/decode helper and the per-backend ABI, not the type word.
Do not add `__bf16` by aliasing it onto `_Float16` — bf16 and binary16 differ only in
the encode/decode helper, so an alias miscompiles silently rather than loudly, which
is the one outcome worth avoiding.

### What the representation is, so it is not re-derived

- `VT_FLOAT16 = 15`; `is_float()` is **true** for it, but `MCC_RC_TYPE`/`R_RET`
  place it in **integer** registers (`src/mccgen.c`). riscv64 already did exactly
  this for `long double`, so the "float type in a GP register" shape was not new.
- Backends therefore need `is_float_abi()` — `is_float() && bt != VT_FLOAT16` — at
  every site that picks a *register class*. Missing one is loud, not silent: i386
  pushed 12 bytes as an x87 long double (`i386-gen.c:597`) and segfaulted; arm64 and
  riscv64 tripped `assert(0)` in `load()`. All are patched.
- Arithmetic never reaches a backend as `VT_FLOAT16`: `gen_op` computes in `float`
  and casts the result back (`src/mccgen.c`), and unary `-` does the same. This is
  safe for `+ - * /` because binary32 has ≥ 2·11+2 bits, so the double rounding is
  exact — the same reason gcc does it.
- **A `_Float16` constant holds its raw 16-bit encoding in `c.i`, not a value in
  `c.f`.** This bit is easy to get wrong and fails quietly: with the value in `c.f`,
  `(_Float16)3` is `0x40400000`, whose low 16 bits are zero, so every constant
  materialized through an integer-register immediate read back as `0.0`.
- Conversions are lowered once, generically, in `gen_cast` — `__mcc_extendhfsf2` /
  `__mcc_truncsfhf2` in `runtime/lib/float16.c`. The names are deliberately **not**
  libgcc's `__extendhfsf2`/`__truncsfhf2`: libgcc passes `_Float16` in SSE registers
  and mcc passes it in integer registers, so sharing the name would be a silent ABI
  mismatch when mcc objects link against libgcc.

### The one divergence from gcc, and why it is not a bug

Bit-exact against gcc over 3,495 comparisons (every conversion direction, ±0, ±inf,
qNaN/sNaN, subnormals at both ends, binary16 max/min normal, round-to-even and
round-to-odd ties, overflow-to-inf, underflow-to-zero and to subnormal, all of
`+ - * /` and all six comparisons, struct/array layout, by-value and varargs
passing). The residual is **NaN ⊗ NaN arithmetic only**: when *both* operands are
NaN, gcc propagates the second, mcc the first. IEEE 754 leaves which NaN propagates
unspecified. Plain `float` NaN arithmetic is byte-identical between the two, so this
is operand ordering in the promoted sequence, not a conversion defect.

Verified semantics were taken from gcc 15.3, not from memory — and one brief
assumption did not survive: **gcc 15 permits `__bf16` arithmetic** (result type
`__bf16`, `bf16`/`BF16` suffixes accepted), so the "storage and conversion only"
restriction is a gcc-13-era rule. `__bf16` also **rounds** float→bf16 to
nearest-even; it does not truncate. `_Float16` is *not* default-argument-promoted in
varargs, on any `-std=`.

### Post-landing fix: the promoter did not know binary16 arithmetic is a call

The Windows stage2 cells (dynamic/pe/sanitize) failed every `exec-*/float16` at
`-O2/-O3/-Os`: `ast_node_libcall` had no `VT_FLOAT16` arm, so the promotion
planner classified a function whose only calls were `__mcc_extendhfsf2` /
`__mcc_truncsfhf2` as call-free and handed promoted locals the **caller-saved**
pool. On win64 that is a guaranteed clobber, not a latent one — `gfunc_call`
stages arguments through r10/r11 before moving them into rcx/rdx, so a loop
counter promoted into `r10d` dies at the first helper call in the body. Fixed in
`ast_node_libcall` (binary ops by *operand* type — comparisons have `int` result
type — conversions with binary16 on exactly one side, and FNEG). Two things worth
knowing before touching this again: **Linux passed the same suites by luck**
(SysV marshalling never touches r10 and the mcc-compiled helpers happen to
preserve it), so a green Linux column is not evidence the promoter models calls
correctly; and `ast_subtree_has_call` (the `ast_promo_store_late` gate) still
counts only `AST_Invoke`, not libcall-lowered nodes — benign today because the
callee-saved pool survives calls, but it is the same mismodel one layer down.

## URGENT — gates that are red or unmeasurable right now

Re-verified mechanically against `da3a461b` on 2026-08-03 in a fresh `cmake-verify`
(`-DMCC_ENABLE_CROSS=ON -DCMAKE_C_FLAGS=-DMCC_REPLAY_IR_C2=1`), which found three
gates this file recorded as green and were not. **Two of the three closed in
`bc85ce70`**; what is left is item 2, which is a reading error and not a red gate.

1. ~~**`tools/o0_ab.sh` fails on all twelve keys and has since `8ab42063`.**~~
   **Closed in `bc85ce70`.** It reported *"an -O0 object moved"* for roughly 250
   objects per key, 6,016 diff lines — very nearly the whole corpus, on every key.
   The cause was never a phase of the cut: `8ab42063` adds **161 lines to
   `runtime/include/mccdefs.h`**, which every compile includes, so `-O0` output
   shifted tree-wide by construction. The shift was **checked rather than assumed**
   before rebanking: building the compiler at `8ab42063^` and diffing objects over
   the corpus, `.text`, `.rodata` and `.data` are byte-identical and the only
   difference is the *names* of local labels in the symbol table and relocations —
   `L.6` becomes `L.9`, because the added declarations consume three anonymous-symbol
   ids. No `-O0` codegen moved, which is what the gate exists to prove. Both boards
   re-taken with the documented recipe on a cross-enabled Debug build,
   `SOURCE_DATE_EPOCH` pinned by the tool itself; only the twelve `.obj.txt` files
   moved, every `.rir.txt` and `.gated.rir.txt` is byte-identical, so no counter
   changed. `C2_NO_EXTRA=1 O0_AB_CHECK=1` over all twelve keys exits 0 and
   `arm-win32 == arm-wince` still holds on counters and object sha256 both.
2. **The `C2_CORPUS=all` board is the bar, and the number this file quotes
   everywhere is not it.** Still open, because it is a documentation defect and not a
   measurement one. See **Scoreboard** — the low number this file quotes is the
   `C2_NO_EXTRA=1` figure, i.e. measured with `full_language.c` *excluded*, while the
   surrounding prose says `all` is the bar. As re-measured at `bc85ce70`: `all` is
   **201** and `C2_NO_EXTRA=1` is **111**, with `full_language.c` costing **90**.
   Anywhere this file still says 194/104, it is quoting the `da3a461b` reading of the
   same pair.
3. ~~**`tests/ast/rir_c2.cmake`'s `BANKFN` floor is one body low on every key.**~~
   **Closed in `bc85ce70`.** The corpus gained a body, so `fn` read exactly +1
   against every banked figure. Measured rather than assumed — a
   `MCC_REPLAY_IR_C2=1` build reads `fn=1150` against the banked 1149 on x86_64, the
   +1 the notes predicted — so all twelve moved up by one, and all 16 `rir-c2` cells
   pass against the tightened floors. The floor only catches population *loss*, so
   this restored its meaning rather than fixing a red cell.
4. **Four things this file and `tools/o0_ab.sh` still describe as live are not**,
   found by grepping the tree at `bc85ce70` rather than reading the prose. None is a
   red gate; each will mislead the next person who acts on it.
   - **`MCC_AST_INT128` no longer exists in `src/` at all** — zero occurrences
     tree-wide outside this file. It died with the recorder in P5. Five paragraphs
     below (**Keep the measurement honest**, the P5 inventory, and the two
     forced-`-O0` notes) still weigh whether to keep it in an env list. All
     historical; the only live forced-`-O0` gating is `MCC_RIR_FORCE` /
     `MCC_FORCE_REPLAY` at **`src/mccast.c:1923-1925`**, which is one predicate with
     two spellings.
   - **`o0_ab.sh:9` still cites `src/mccast.c:2035`** for that gate. The line-reference
     table below has the correct `:1923`; the script's header does not.
   - **The "38 gates" figure is stale in both scripts' comments, and the whole
     derivation is now vacuous.** It greps for
     `ast_env_gate("MCC_AST_*", o4 || s1->optimize >= 1)`, and `ast_env_gate` no
     longer exists — the gates are `-f` flags resolved from `mccopt.h`. The scripts
     abort at zero derived gates, so `C2_FORCE=1` will fail loudly rather than
     measure the wrong thing; they need repointing at the flag table before the
     next forced-`-O0` board.
   - **There is no `-O0` cell for `rir_c2.cmake`.** The twelve `ast/rir-parity-*-O0`
     cells enforce the replay bar at `-O0` and `rir_c2.cmake` banks the C2 gap at
     `-O1`/`-O2`/`-O3`, so the forced-`-O0` C2 gap is reachable *only* by hand
     through `tools/c2_sweep.sh` and is banked only as prose in **Scoreboard**.
     `tools/o0_ab.sh` is likewise not a ctest cell. Now that a twelve-key sweep is
     45 seconds rather than the twelve minutes this file assumed, the argument that
     kept these out of ctest no longer holds — reconsider it.
   - **Both tree-versus-arena instruments are dead, and this file still opens its
     debugging recipe with one of them.** `RIRC2TREE` was deleted in `ebda09c2` —
     `:353` documents it as live and `:607` records its removal, and the two
     contradict outright; zero occurrences in `src/` or `tools/`. `MCC_REPLAY_IR=6`'s
     `[rir-diff]` half is worse, because it *looks* alive: it is guarded on
     `ast_replay_ok(ast_cur)` (`src/mccrir.c:4559`), and since P5 `ast_func_begin`
     sets `ast_cur = ast_arena_new()` (`src/mccast.c:12911`) — a fresh *empty* arena
     with no recorder to fill it — so the comparison never runs and prints nothing.
     Verified by running it: `[rir-dump]` fires, `TREE:` sections and `[rir-diff]`
     lines are **zero**. `:291`'s *"a structural `[rir-dump]` diff against the tree
     found nearly everything that closed"* describes a capability the tree no longer
     has. **Diagnosis is one-sided from here**: the arena dump, `RIRDUMP`'s op
     window, the `[ent]`/`[stmt]`/`[vst]`/`[gop]`/`[arg]` streams, and byte windows.
     Restoring a control leg means re-adding the ternary at `src/mccrir.c:4852`
     *plus* a leg marker in `[rir-c2part]`, or `:353`'s silent-fallback caveat
     returns with it.
5. **The class E scope in `:399` is wrong, and it is wrong in the optimistic
   direction.** That line says `fuzz/runner.c::triage`/`::interesting`/`::main` are
   *"27 on the nine non-x86_64 keys only — the x86_64 keys closed with the store-chain
   fix."* Measured at `bc85ce70`: the x86_64 close holds **only at `-O1`**. The same
   three bodies, with the same signature, diverge at `-O1` on **arm64, riscv64, arm
   and i386**, and at forced `-O0` on **x86_64**. One defect, roughly 15 divergences
   at `-O1` plus 3 at `-O0` — not a three-body `-O0` curiosity. Root cause is
   established (see **Still open**); the arena node is not yet pinned.

6. ~~**`stage2 / linux / predefs-off` failed two `parts/` cells on CI**~~ —
   **closed 2026-08-04, but note the first fix did not close it.** Two sessions
   landed on this concurrently. `43243c67` read the failure as a *spelling*
   problem — gcc 12 and earlier reject `-std=gnu23` and want `-std=gnu2x` — and
   added `ref_std_flag()`, an acceptance probe. That diagnosis does not survive
   the log: the runner is **gcc 13.2.0** (`4:13.2.0-7ubuntu1`), the log contains
   **zero** flag rejections, and the gcc leg failed *inside the source* at
   `s7_13.h:40:34`. A flag that is rejected does not get you a semantic error on
   line 40.

   What actually happens is that gcc 13 **accepts** `-std=gnu23` and then
   reports `__STDC_VERSION__` as **202000L**, where clang and mcc report
   202311L. The suite's whole premise is a 3-way stdout identity, so it was
   comparing three compilers answering two different language questions. The
   error proves the version window on its own, with no version lore needed:
   `<stdalign.h>` drops `__alignas_is_defined` once `__STDC_VERSION__ > 201710L`
   and `s7_13.h` required it whenever `__STDC_VERSION__ < 202311L`, so reaching
   that error at all pins the reference between the two. `run_s6_10_4` prints
   `__STDC_VERSION__` outright, which is the same fact seen from the other side.

   Because an acceptance probe asks whether the flag *parses*, not which
   language it selects, `ref_std_flag()` picks `gnu23` on this very runner and
   both cells still fail. Measured head-to-head against a gcc-13 stand-in that
   reproduces the CI error byte for byte (`s7_13.h:40:34`), over the full suite
   on identical inputs and the *unmodified* test files: `43243c67` **31/33**,
   failing exactly `run_s7_13` and `run_s6_10_4` with the CI symptoms; the
   replacement **33/33**.

   The parts suite now resolves the std by **agreement** instead: probe gcc,
   clang and mcc for `__STDC_VERSION__` and take the newest of
   `gnu23`/`gnu2x`/`gnu17`/`gnu11` all three report identically, printing what
   it resolved. `gnu2x` is kept in the list because `43243c67`'s point about
   pre-13 gcc is independently true — a rejected probe just loses the round.
   C23 coverage is retained wherever the reference toolchain implements it
   (gcc 16: resolves `gnu23`, 33/33) and stepped down only where it does not
   (gcc-13 stand-in: `gnu23` and `gnu2x` both disagree at 202000L, resolves
   `gnu17`, 33/33 — and that holds against the *unmodified* test files, so the
   harness fix alone is sufficient; the `s7_13.h` guard below is belt and
   braces).
   `ref_std_flag()` is left in place for `suite_mcctest`, which is a 2-way
   comparison rather than a 3-way identity.

   `s7_13.h` additionally now keys on `#ifdef __alignas_is_defined` rather than
   a version threshold, which is what it meant to ask; the old guard misfires on
   any compiler reporting a C2x-era version, independent of this suite.

   It was also never `predefs-off`-specific, as originally banked: at
   `3486e3a4` (run 30915974711) **every** linux stage2 job failed the same two
   cells, 8120/8122. The one green linux job, `asm-off`, is green only because
   the parts suite is registered under `MCC_CONFIG_ASM` and skipped there. All
   five macOS jobs passed. The original entry also blamed `1f8f7e36` (xsuite's
   std forwarding), which the parts suite does not use.

7. ~~**`selfhost-fixpoint` is not stable at the default `-O`, on macOS arm64 and
   on Windows.**~~ **Diagnosed and closed on the Windows host, 2026-08-04.**
   The instability was real, bimodal (every bad run produced the *same*
   alternate object, ±192 bytes), and reproducible on Windows only with
   *concurrent mcc compiles* (3 parallel fixpoint chains, ~1-5 in 30) — never
   serially, never under plain CPU load. The divergence was always one body
   (`mccstats_spark`) losing its `% 40` divmagic lowering to a plain `idiv`,
   and a `-DMCC_CONFIG_TRACE=1` twin caught the branch:
   `ast_divmagic_lowered` returned *"already lowered"* off a **stale
   `ast_divmagic_base` watermark keyed on the arena's address**. Every body's
   arena is a fresh `ast_arena_new()`; when the heap recycles a freed arena's
   address — allocator-timing-dependent, which is what made it look like a
   host quirk — `ast_divmagic_run`'s `base_arena != a` identity check cannot
   see the reuse and the new body inherits the old watermark. The fix is the
   invalidation hook the other five pointer-keyed caches already had in
   `ast_arena_free`: `ast_divmagic_invalidate` beside `ast_du_invalidate` and
   friends. Verified on the Windows host: 36/36 identical objects under the
   reproducing stress, 12/12 clean chains over the gates knob set, `-Os` and
   `-O3`, 3-way concurrent plus burn. The macOS arm64 `o1=3396582 o2=3396230
   o3=3396582` reading at `3486e3a4` has the same signature and the mechanism
   is OS-agnostic (any malloc recycles) — re-run that cell on the macOS host
   at this fix before carrying it forward as a separate defect.

8. **`mcctest`/`mcctest-bcheck` fail against Apple clang 21 as the reference.**
   `tests/diff/parts/legacy_numeric.h:7:23` is `static double nan2 = 0.0 / 0.0;`
   and the *reference* leg rejects it: `error: cannot compile this constant
   l-value expression yet`. Verified pre-existing by running the pristine
   `HEAD:tools/mccharness.c` — identical failure, so it is not fallout from the
   parts-suite fix. CI does not see it (its macOS images carry an older clang),
   so this is a local-toolchain gap, not a tree red. `suite_mcctest` still
   hardcodes `-std=gnu23` at `tools/mccharness.c:467`; if this needs closing,
   the capability probe added for the parts suite is the obvious lever.

### New urgent items — Windows and macOS, which this host cannot reach

Everything below was **attempted locally and could not be measured**, not skipped.
This host runs x86_64 Linux with docker, wine, and `qemu-{i386,arm,aarch64,riscv64}`.
Verified reachable here: the four ELF cross keys execute under `qemu-user` and both
x86 PE keys execute under `wine` (a `printf` binary from `mcc-x86_64-win32` and
`mcc-i386-win32` both run clean). That leaves six of the twelve keys and every
host-native self-host unmeasurable on this machine.

| # | what | why it needs the other host |
| --- | --- | --- |
| W1 | **`arm64-osx` and `x86_64-osx` runtime A/B and self-host** — **the A/B half is done** | Mach-O will not execute here at all. **Closed on 2026-08-03 for the differential**: the macOS arm64 host ran `tools/c2_equiv.sh` on both keys at forced `-O0`/`-O1`/`-O2`/`-O3`, eight cells, all `differential: NONE`, and the whole byte gap on both keys measures `[rir-prod] fallback` per body — see the macOS item under **For the Windows and macOS hosts**. **Still open on that host: the full `ctest` suite and `o0_ab.sh`,** which are the larger half and which this file has still never had a Mach-O reading of. |
| W2 | **`arm64-win32`, `arm-win32`, `arm-wince` execution** | wine on an x86_64 host runs x86 PE only; ARM and ARM64 PE do not load. Three keys, 47 divergences between them, never executed. |
| W3 | **`selfhost-fixpoint-O3` on macOS** | Recorded as *"green again by side effect, and the defect underneath it was never found… treat this as latent, not fixed."* It is green here. The failure was macOS-only, so this host cannot tell whether it is fixed or dormant. Re-run with the `MCC_AST_INLINE_LIMIT` bisect against a fresh probe, per the recipe already in this file. |
| W4 | **Windows host stage2 (`pe`, `sanitize`, `dynamic`)** | The three reds at the P5 merge were closed by raising `SizeOfStackReserve` to 8MB, which is present at `src/objfmt/mccpe.c:738` and verified by reading. The *stack-overflow behaviour it fixes* needs a real 1MB-default PE process to confirm; wine's stack handling is not the same test. |
| W5 | **mcc cannot self-host on Windows arm64** | Still open, still unreachable. Stage1 mcc takes an access violation (`0xC0000005`) on `lib/atomic.c`, `lib/alloca.S`, `lib/alloca-bt.S`, `lib/builtin.c`. Host ABI — varargs, `alloca`, stack probe — and it needs a Windows arm64 machine. |
| W6 | **Re-bank `verify-baseline` — now moot, record the closure** | The prediction that `x86_64-darwin.txt` and `x86_64-win32.txt` needed re-banking on their own hosts is **dead**: P5 deleted all four files and `tests/ast/verify-baseline/` no longer exists. Nothing to do; do not carry it forward. |
| W7 | **The external suites cannot be run here at all** | `cmake-release` does not exist and no gcc/llvm test tree is vendored, so nothing under **External suites** or **Vector types** could be checked on this host. **Partly answered upstream**: `214ed40f` re-ran the whole tree and reports 82.6/82.4 over 20,513 tests — see **The eight-cluster sweep**. The gap that remains is that this host still cannot reproduce it. |
| W8 | **`selfhost-jit` faults `0xC0000374` (STATUS_HEAP_CORRUPTION) on x86_64 Windows** | Live. `mcc --jit -O13 -run src/mcc.c` corrupts the heap during the in-memory recompile of its own source; basic JIT `-run` works and the same cell is green under `linux-gcc`/`linux-clang` in WSL, so this is Windows-PE-JIT-specific and in the same swapped-variant/KGC-stub residual family as the `0xC0000005` skip. `tools/selfhost-jit.py` now SKIPs `0xC0000374` alongside `0xC0000005`; the heap-corruption defect itself is still open and needs a `cdb`/PageHeap run to name the faulting allocation. |

**W4 and W5 above are live work on the Windows host — see the section below.** The
preset sweep already closed the 8MB host-exe stack reserve and the `_WIN32_WINNT`
0x0600 floor; `stage2` is listed there as not started, which is exactly W4. Do not
open a second front on either.

## Open — the Windows-host preset sweep, interrupted by a reboot (2026-08-03)

A full presets-x-tests sweep on the Windows x86_64 host (scoop clang MSVC-ABI as default CC), four host-build defects fixed and pushed: the `_WIN32_WINNT` 0x0600 floor (SRWLocks), the 8MB host-exe stack reserve (mcc-ejboot 0xC00000FD), the diagnostics `mcc_p` skip under MSVC-ABI clang, and gcc discovery preferring vendored winlibs over CLion's bundle (whose clang-built libpthread.a references `__intrinsic_setjmpex` its GNU runtime never defines — probe-linked now, do not re-diagnose).

State when the machine went down: `cross`/`debug`/`release`/`sanitize`/`cst` green at 8160-8184 each, on the pre-rebase base. Still to run, all on the pushed HEAD:

- `matrix` — clean rebuild was in flight when the reboot killed it; `rm -rf cmake-matrix` and rerun (config log should print the winlibs gcc, not CLion's).
- `diagnostics` — rerun with the mcc_p skip; everything cascaded from that one link before.
- ~~`msvc`~~ — **green on 2026-08-04** at `583acfc3` + the shadow-iv bash fix: full
  suite passes with only the expected skips (no cross sysroots, no docker, macho).
  Two Windows-host defects closed on the way: `#embed`'s `S_ISREG` (UCRT has no
  such macro; stage1 windows-x86_64-msvc died at link on CI) and the
  `cross/shadow-iv-*` cells invoking bare `bash`, which resolves to System32's
  WSL relay on a real Windows host — the one script test in the tree not going
  through `sh`. CMake now resolves a real bash (beside the trusted `sh`, or by
  asking that sh via cygpath — a scoop shim dir holds no bash.exe) and prepends
  its directory to the test PATH, since bare bash.exe does not bring its own
  usr/bin along.
- `sanitize-msvc` (VS 18 is installed), `mingw`, `stage2` (stage1 = the release mcc), `dist-mingw`, `dist-msvc` — not started.
- Re-verify one full suite at the final HEAD: the green five above tested the tree before ~30 upstream commits landed mid-batch.

Cut the AST recorder and the operation journal out and leave Replay_IR as the compiler's only intermediate representation. **Cut to Replay_IR** at the end of this file is the staged plan; P0 through P4 have landed and `MCC_RIR_PROD` now defaults to **1**, so Replay_IR is the production arena — read P4 before touching the arena, it is where the three wrong-code classes are written down. **`MCC_RIR_ONLY` now defaults to 1 as well**: the recorder's per-body decline verdict was the arena's admission gate, and with it bypassed the arena is adopted on its own pre-flight alone. That widened the optimized population by 6.3% of bodies; the eight defects the widening exposed are closed, the three `optfire` cells that died with the gates they measure are deleted, and the tree is green at **8252/8252**. **The next step is the deletion itself** — read P5, which is now a pure deletion that moves nothing by construction. Everything above P5 is the C2 work, which the plan no longer blocks on — the per-body fallback decision means a body Replay_IR cannot re-emit keeps the parser's bytes rather than blocking the deletion.

Two bars, both required. **Replay** (`rir_verify`) replays a captured body against the parser's own bytes. **C2** re-emits from the reconstructed arena and compares — the harder bar, and the one still open. Replay is at `faithful + empty == fn` on all twelve target keys at `-O0`/`-O1`/`-O2`/`-O3`, gated by the 48 `ast/rir-parity-*` cells.

**The corpus is now all of `tests/`, not `tests/exec`.** 657 files against 277, and about twice the bodies per key — 1980–2521 against 1146–1318. `tests/exec` was never a wrong measurement, but it was a narrow one: it reads a 14-body gap where the whole tree reads 148, and it says `c2bytes=0` everywhere where the whole tree finds byte divergences on ten of twelve keys. `C2_CORPUS=exec` still produces the historical board for continuity; `C2_CORPUS=all` is the default and is the bar.

## The stage2 `diagnostics` cell no longer runs in CI (2026-08-05)

`diagnostics` (`-DMCC_DIAG=ON`) is now listed in `FEAT_CI_SKIP` in `tools/ci.c`, so
`ci plan --job stage2` and `--job stage2-nightly` emit its cells with a `skip` reason
instead of a build, and `ci local` (the all-features stage2 sweep) counts it as a skip.
Both workflows already honour `matrix.skip`, so the cells still appear in the run with
one `SKIP diagnostics: …` line each rather than vanishing. Reason banked in the plan
output: *local instrumentation axis; the CI gate covers the same code through the other
feature cells.*

What is **not** skipped, and is how the axis is still exercised:

- `mcc-ci stage2 diagnostics --mcc …` invoked by name — unchanged, no skip path.
- The `diagnostics` and `linux-gcc-diagnostics` presets — unchanged; both are already
  in `PS_EXEMPT`, so the preset-coverage gate stays quiet either way.
- `ckconfig`/`MCC_DIAG` itself in `CMakeLists.txt:1207` — untouched.

`ci plan --job stage2-gate` now hard-fails if a `FEAT_CI_SKIP` feature is ever added to
`GATE_CELLS`, so the two tables cannot contradict each other silently. To put the cell
back, delete its `FEAT_CI_SKIP` row — nothing else was changed.

Standing risk of the skip: `MCC_DIAG` is the everything-on warning/debug build, and
`525235f8` records it catching two real defects rather than noise. Nothing in CI reads
those warnings now, so a diag-only regression will only surface on a local run.

## Handoff — state at this checkpoint

**Re-measured at `da3a461b`, 2026-08-03: `ctest -j 8` registers 8254 cells and reads
8253 passed, 1 failed.** The one failure is `selfhost-output-parity-O3`, and it
**passes standalone in 15.4 s** — it is the `-j 8` contention artifact this file
already documents for the `selfhost-*` family, not a regression. Treat the tree as
**8254/8254**. The earlier figures in this section (8252, 8232, 8231) are all
superseded; the count rose because the two feature commits of 2026-08-03 added
cells, not because anything was un-deleted.

### Where the cut stands

| phase | state |
| --- | --- |
| P0 harness, P1 journal verify-half cut, P2 decouple + admission predicate, P3 capture rebrand | landed |
| P4 cutover | landed; **`MCC_RIR_PROD` defaults to 1**, arena drives **91.5%** of bodies (`used=2011 fallback=11 skip=176` of 2198, x86_64 `-O2`, whole corpus) |
| P5 switch | **landed. `MCC_RIR_ONLY` defaults to 1**; the three `optfire` cells that died with it are deleted and the suite is 8252/8252 |
| P5 deletion | **done, and re-verified mechanically.** All 33 recorder symbols read 0 occurrences under `src/`: `ast_hook_`, `ast_bail`, `ast_desync`, `ast_vn`, `ast_try_active`, `ast_gap_note`, `AST_SET_BAIL`/`_DESYNC`, `ast_capture`, `ast_in_op`, `ast_in_call`, `ast_ret_val`, `ast_last_return`, `ast_saw_nocode`/`_chain`, the four `*_pending`, `ast_cf_top`/`_if`/`_savebb`, `ast_vs`, `ast_ret_bad`, `ast_bad_vtype`, `ast_wide_vtype`, `ast_verify_env`, `MCC_AST_INT128`, `MCC_AST_LDOUBLE`, `MCC_RIR_ONLY`, `ast_rir_seam`. All 12 named recorder-shape gates read 0 in `src/` **and** in `tests/`. All six keepers are present. **Four pieces of residue survive — see "Deletion residue" below.** |
| `fix-imaginary` | branch ready, merges clean, **unblocked** — `tests/ast/verify-baseline/` and `verify_ratchet.cmake` are confirmed absent from the tree |
| P6 split + `ast_*` → `ir_*` | not started. **Its stated precondition is false** — see P6. |

### Deletion residue — four things P5 was supposed to take and did not

Found by re-grepping the inventory at `da3a461b`. None is load-bearing; all four are
one-line removals that belong in the P6 diff at the latest.

- **`MCC_AST_VERIFY_DIFF` and its whole facility survive.** The inventory listed
  `ast_verify_env`, `MCC_AST_VERIFY` and `MCC_AST_VERIFY_OUT` and those three are
  gone, but `ast_verify_diff` (`src/mccast.c:1311`, read at `:1927`),
  `ast_verify_diff_match` (`:12866`), `ast_verify_dump_diff` (`:12879`) and the call
  site at `:15122` are all live, as is `ast_treechk`/`ast_treechk_on`/`MCC_AST_TREECHK`
  (`:5125`–`:5143`, called at `:15118`). The `ast/treecheck` **cell** is gone; the
  machinery it drove is not. Decide whether the diff-dump is worth keeping as a
  Replay_IR instrument, or delete it — but do not leave it undecided.
- **`ast_jit_guard_env` (`src/mccast.c:1382`) is still there.** This file's own P5
  inventory names it and calls it *"already dead today, declared and never assigned"*.
  It is still declared, still never assigned, still never read.
- **`ast_rir_arena` is now a dead local.** `src/mccast.c:14948` declares it and
  `:14955` assigns it; nothing reads it. It is the stump of the `do_promote`
  carve-out, and that carve-out really is gone from `do_promote` (`:15213`) as
  claimed — this is just what it left behind.
- **`tools/tracediff.sh` survives** although the deletion note says `ast/treecheck`
  and `ast/tracediff` went *"with their two scripts"*. The two **cells** are gone;
  one script is not, and this file still recommends it as an instrument under
  **Open work raised after the cutover**. Keep it and fix the note, or delete both.

Also: **`tools/gate-ledger.sh` still runs the recorder half.** It drives
`MCC_AST_HASH_OUT` at `:34` and prints *"changes the recorded AST"* at `:77`. The P5
inventory lists `tools/gate-ledger.sh`'s recorder half as dying with the recorder.
It did not.

### Next steps, in order

1. **Land `fix-imaginary`** — `ast-verify-ratchet-{O1,O2,O3}` are deleted, which were the only three cells blocking it. It merges clean and needs no rebase. **This is unblocked now.**
2. **P6**: split `mccast.c` and rename `ast_*` → `ir_*`. **Its precondition is false** — the `ir_`/`IR_` namespace holds 835 occurrences under `src/` (`ir_cap_*` 478, `IR_OP_*` 280, `IR_CAP_*` 77), all of them P3's own rebrand. Settle the target spelling first; see P6.
3. Then the items under **Open work raised after the cutover** — the poison-over-deletion change, the `ast_cycle_env` convergence, the pointer test, glibc headers, and `MCC_TRACE` on `full_language.c`.

### What the deletion did

**Done, and gated.** `-1,618` lines of `src/mccast.c` (76 hook definitions plus their 64 forward declarations), all **124** call sites in `src/mccgen.c` with the 26 `#if MCC_CONFIG_OPTIMIZER` blocks they left empty, all **76** declarations in `src/mccast.h`, and **12 of the 14** recorder-shape gates, which measurement confirmed had exactly two references each — a declaration and an assignment, no reader. **The six keepers are kept**: `ast_label_id`, `ast_label_forget`, `ast_sv_hi`, `ast_cmp_invert_late`, `ast_bad_type`, `ast_func_has_asm`. Verified mechanically — the set of top-level definitions lost is *exactly* the `ast_hook_*` set, nothing else.

Twenty cells died with it and are deleted: `ast-verify-ratchet-{O1,O2,O3}` with `tests/ast/verify_ratchet.cmake` and all four `tests/ast/verify-baseline/` files; the **fourteen** `ast/rir-c3-*` cells with `tests/ast/rir_c3.cmake` and the `rir-c3` custom target — P4 predicted these die with the recorder and they did, all fourteen, in one run; `ast/treecheck` and `ast/tracediff` with their two scripts; and `optfire/default-nocode_call`, a fifteenth recorder-shape gate cell the inventory had not listed. `MCC_AST_INT128=1` is out of `rir_parity.cmake`'s forced-`-O0` env, exactly as **Keep the measurement honest** said to do it — *when* the gate dies, not before. 8252 → **8232** registered cells, and `ctest -j 8` is **8232 of 8232, zero failures**.

Gates met: the twelve-key object A/B reads **19,425 compared, 12 differ** — the single `-O3` `scopes.c` widening described above and nothing else, unchanged before and after the gate removal, which is what proves those 12 gates were readerless. `C2_NO_EXTRA=1 O0_AB_CHECK=1 tools/o0_ab.sh` passes against the bank on all twelve keys with `arm-win32 == arm-wince`. `tracegate`/`schemagate`/`targetgate` clean.

**The state went too, and the recorder is now gone entirely.** `ast_bail`, `ast_desync`, `AST_SET_BAIL`/`AST_SET_DESYNC`, `ast_gap_note` and the whole gap-note block, the shadow vstack (`ast_vs`, `ast_vn`, `ast_cf_if`/`ast_cf_savebb`/`ast_cf_top`), `ast_capture`, `ast_in_op` (with its ten increment/decrement sites in `src/mccgen.c` and the lone `ast_bail = 1` in `va_arg`), `ast_in_call`, the ternary and land/or stacks, `ast_ret_val`, `ast_last_return`, `ast_saw_nocode`/`ast_saw_chain`, `ast_call_pending`/`ast_inc_pending`/`ast_vdup_pending`/`ast_opassign_store_pending` — all deleted. `ast_replay_ok` is now literally "the arena has a body". `ast_try_active` is gone and `rir_try_active`, which `src/mccrir.c` has owned since P2, gates `ast_func_begin` and `ast_func_end` directly; `ast_ret_bad`, `ast_bad_vtype` and `ast_wide_vtype` went with it, and with them the last two recorder gates, `MCC_AST_INT128` and `MCC_AST_LDOUBLE`. `ast_verify_env`, `MCC_AST_VERIFY`, `MCC_AST_VERIFY_OUT` and both `[ast-verify]` verdict blocks are gone. `tests/fuzz/runner.c`'s `GATES[]` lost its eight dead rows and the `RECORDER_2026_07_31` combination that consisted entirely of them; every `MCC_AST_*` the fuzzer still names is verified to exist in `src/mccast.c`.

**`MCC_RIR_ONLY` is gone and `MCC_RIR_PROD` is now report-only.** `ast_func_end` adopts the arena unconditionally: `rir_prod_env` is `ast_replay_env && !rir_env`, with no gate and no `ast_verify_env` term. `MCC_RIR_PROD` survives *only* as the verbosity level the measurement recipes in this file are written against — unset or `0` is silent, `2` still prints `[rir-prod] <verdict>` per body and the `[rir-prod-total]` atexit line. **It can no longer turn production off**, which is the point: there is nothing left to fall back to.

**Two more cells died with the off-switch, and the inventory had no row for them.** `ast/rir-inert` and `ast/journal-inert-build`, with `tests/ast/journal_inert.cmake` and the `mcc_nojrn` target they existed to drive. Both proved the capture substrate is *byte-inert* by compiling the corpus twice under `MCC_RIR_PROD=0` and comparing. That property is now vacuous and the cells are unsatisfiable by construction: with capture as the sole producer, a compiler built `MCC_IR_CAPTURE=0` has no arena at all, so it keeps the parser's bytes while `mcc` re-emits from the arena — the two legs *must* differ. P4 saw the shape of this coming ("with the cutover on, `mcc` and `mcc_nojrn` legitimately differ because one has a different producer") and re-pointed the cell rather than retiring it; the off-switch's removal is what finally made it unanswerable. 8232 → **8230** cells; upstream has since added one back, so the tree reads 8231.

### The `MCC_RIR_ONLY` flip, as measured

One line: `ast_env_gate("MCC_RIR_ONLY", 0)` → `1` at `src/mccast.c:2040`. Note the switch lives in **`mccast.c`**, not `mccrir.c` as an earlier revision of this file guessed.

- **Object A/B with the switch off is byte-identical**: `cmake-p5base` (pre-flip, nothing set) against the flipped binary under `MCC_RIR_ONLY=0`, over `find tests -name '*.c'` × twelve keys × `-O1`/`-O2`/`-O3`, `SOURCE_DATE_EPOCH` pinned — **19,425 of 19,425 identical, 0 differ**, with `arm-win32 ≡ arm-wince` on every row.
- **`ctest` before the flip: 8255/8255.** After the flip: 8251/8255, and the four are the three `optfire` cells plus the root-owned-leftovers artifact below.
- **The three `optfire` cells failed for exactly the recorded reason**, read off the cell output rather than predicted: `optfire/opassign` and `optfire-arm64/opassign` both report *"pass DID NOT FIRE (MCC_AST_OPASSIGN=0 and MCC_AST_OPASSIGN=1 objects are byte-identical at -O2)"*, and `optfire/default-landor_invert` reports *"MCC_AST_LANDOR_INVERT is default-off at -O2, expected default-on"*. Both gates' only readers are inside the hook block (`src/mccast.c:2524` in `ast_hook_cmp_invert`, `:2653` in `ast_hook_vdup`), so with the arena as sole producer they correctly stop changing output. The cells are deleted — one line each from `tests/optfire/differs.txt` and `defstate.txt`, plus their two `arch.txt` scoping entries. 8255 → 8252 registered cells, and 8252/8252 green.
- **The flip was verified by counters, not by one object.** `MCC_RIR_PROD=2` on `tests/exec/programs/grep.c` reads `used=14 fallback=0 skip=0` with the switch defaulted on against `used=10 fallback=0 skip=4` under `MCC_RIR_ONLY=0` — the four bodies the recorder declines are exactly the widening.
- Four side configurations green in distinct build dirs; `tracegate`/`schemagate`/`targetgate` clean; `src/mccrir.c` still has no `MCC_TRACE`.

**A trap this flip re-paid for, and the file already warned about it.** `riscv64-promote-docker` leaves **root-owned** files under `<build>/riscv64-promote-docker/corpus/` on every *successful* run, so the *next* run fails in 0.00 s with a permission error. It failed that way three times here and each time read as a real regression. `sudo rm -rf` the directory and re-run the cell alone before believing it; it passed in 33 s every time from clean. The failure is a property of the previous run, not of any change.

### Measurement setup, and the traps that cost time

- Build for measurement: `cmake -S . -B bc2 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMCC_ENABLE_CROSS=ON -DCMAKE_C_FLAGS=-DMCC_REPLAY_IR_C2=1`.
- **`vendor/` is gitignored but a *symlink* named `vendor` is not matched by `/vendor/`.** A git worktree has no sysroots, so four ELF cross keys compile ~77 files of 276 and still print a green board. Symlink it, verify `ls vendor/gentoo-stage3-riscv64-glibc/usr/include`, and **never `git add -A`**. The same applies to a `cmake-cross` symlink, which is what makes the docker-gated cells actually run instead of SKIP.
- **A long checkout path fails 22 `exec*/bound_global`** — `bt_info.file` is `char[100]`. Not a regression. Configure through a short symlink (`/tmp/mccw`) to avoid it.
- **Root-owned leftovers under `<build>/riscv64-promote-docker/`** fail that cell in 0.00 s with a permission error and read as a real failure. Clear them before believing it.
- ~~**`superopt/promote-floor` is load-sensitive and will read as a regression.**~~ **Fixed** — the test is `RUN_SERIAL` now, so it no longer needs re-running by hand. The underlying cause is worth keeping in mind: `-O4` is the *wall-clock*-budgeted search (`host_clock_ms() - start < budget_ms`), so a loaded machine explores fewer candidates and can miss the promoted variant. Any future test that compares two `-O4` outputs has the same exposure, and `RUN_SERIAL` narrows the window rather than closing it — the search is genuinely nondeterministic under load.
- **Do not run a twelve-key sweep and `ctest -j 8` concurrently** — the contention alone fails several `selfhost-*` cells.
- **Pin `SOURCE_DATE_EPOCH`** for any object A/B, or a file differs from its own second compile.
- **`ast_env_int` returns its default for values `<= 0`**, so `MCC_RIR_PROD=0` means *on*; unset it instead. `ast_env_gate` handles `0` correctly.
- **`cp -a` of a build dir is not a reference build** — `CTestTestfile.cmake` bakes the binary dir absolutely and the copy silently runs the original binaries.
- Verifying a switch by compiling one file and diffing the object proves nothing: only 69 of 562 files differ at `-O2` on x86_64. Use a file already known to differ, or the `[rir-prod-total]` counters at `MCC_RIR_PROD=2`.

### The Windows stage2 reds after the merge, closed

The three failure groups on every Windows stage2 job (`pe`, `sanitize`, `dynamic`) at the P5 merge were two defects, both closed:

- **`selfhost-*` (all eight) and `o4-aot-jit` Part 1: `STATUS_STACK_OVERFLOW` (0xC00000FD) in any mcc-linked mcc compiling `src/mcc.c` at `-O1`+.** The recursion is `ast_replay_bb`'s else-arm — one frame per `else if` link — and it predates P5; what P5 changed is that the arena now admits the wide population, so the long option-chain bodies replay. The amplifier is self-hosted codegen: mcc gives `ast_replay_bb` a **27,664-byte frame** (`mov $0x6c10,%eax; call __chkstk`), so ~37 levels exhaust the **1MB** default PE stack reserve. gcc-linked stage-zeros survive on mingw's 2MB reserve and small frames, which is why the tree read green everywhere but a stage2. The fix raises `pe_template`'s `SizeOfStackReserve` to **0x00800000** for all PE targets — the arm64 arm of that `#if` had already chosen 8MB, so this deletes the `#if` rather than adding one. Reserve is address space, not commit; `-stack=` still overrides. Debugging note that cost an hour: both vendored and scoop gdb **crash (0xC0000409) loading mcc's PE DWARF** — `objcopy --strip-debug` on a `-g` build leaves the COFF symtab, gdb then runs and backtraces against it, and `addr2line` on the un-stripped twin names the statics.
- **`exec-replay-promote/c11_complex_convert`: the fconst reuse list, third act** — see the new kind-tag bullet under **What closed**. Win32-only because the win64 ABI passes 16-byte `_Complex` by reference, which is the shape that materializes the constant temp.

### The three expected `optfire` failures

`optfire/opassign`, `optfire-arm64/opassign`, `optfire/default-landor_invert`. `optfire` probes *which* optimizations fire; both gates are recorder-shape gates with no reader outside the hook block, so they correctly stop changing output. **They are resolved by the deletion, not by a code change** — do not attempt to fix them.

### Method that worked, for whoever picks this up

Every phase was gated on a number measured before and after, and the phases that moved nothing said so with a byte-identical object A/B over the whole corpus × twelve keys × three `-O` levels. Three separate agents reported a defect as "pre-existing" that measurement showed was theirs, and two predictions written into this file were falsified by the next measurement. **Re-measure rather than carrying a row forward** — this file says so in several places and it earned every one of them.

## Scoreboard

Per key at HEAD, produced by `tools/c2_sweep.sh <builddir> <key> <opt>` against an mcc built `-DMCC_REPLAY_IR_C2=1` under `-DMCC_ENABLE_CROSS=ON`, run in place. **One twelve-key `all` corpus takes about 45 seconds**, roughly 3.6s per key — measured at `bc85ce70`, and the "about 12 minutes" this file used to say was wrong by an order of magnitude and was the reason several rows here were copied forward instead of re-run. Re-measure rather than copying a row forward; it is nearly free.

Read `ok=` on every row before believing it. `rir_report` is an **atexit** handler, so a file that fails to compile still prints `[rir-total]` and still contributes bodies — counting `[rir-total]` lines is not an honesty check. The sweep counts only files that exited 0 and prints that count.

`C2_CORPUS=all`, 657 files, `-O1`. `gap` is `c2try - c2ok`. **The `arenahasheq` column is frozen at the reading below and cannot be re-measured** — P4 deleted the counter and the `tools/c2_sweep.sh` column with it, deliberately, because it scored the arena against the recorder's tree rather than against the parser. It is kept here only so the classes it named still resolve:

| key | c2ok/c2try | gap | classes | arenahasheq (retired) | ok |
| --- | --- | --- | --- | --- | --- |
| x86_64 | 1944/1948 | 4 | 4 len | 1414/1988 | 522 |
| x86_64-win32 | 2306/2317 | 11 | 2 bytes + 7 len + 2 err | 1616/2364 | 527 |
| arm64 | 2114/2124 | 10 | 1 byte + 8 len + 1 invalid | 1506/2164 | 543 |
| i386 | 1937/1947 | 10 | 10 len | 1384/1987 | 519 |
| arm | 2052/2064 | 12 | 2 bytes + 9 len + 1 invalid | 1442/2104 | 541 |
| arm64-win32 | 2331/2346 | 15 | 2 bytes + 11 len + 2 err | 1637/2392 | 522 |
| x86_64-osx | 2449/2462 | 13 | 3 bytes + 8 len + 2 err | 1656/2508 | 571 |
| i386-win32 | 2310/2327 | 17 | 2 bytes + 13 len + 2 err | 1593/2378 | 526 |
| arm-win32 / arm-wince | 2270/2287 each | 17 | 3 bytes + 12 len + 2 err | 1575/2334 | 519 |
| riscv64 | 2035/2048 | 13 | 2 bytes + 9 len + 2 invalid, plus 7 skip | 1463/2094 | 539 |
| arm64-osx | 2458/2477 | 19 | 4 bytes + 13 len + 2 err | 1679/2521 | 566 |

**The table above predates P4 and its `ok` column no longer matches the tree** (it reads 522 files on x86_64 where the same sweep now reads 562), so do not diff a fresh row against it.

### The `all` board is 201. The 111 is the board with `full_language.c` taken out.

**Re-measured at `bc85ce70`, 2026-08-03, `C2_CORPUS=all`, `-O1`, twelve keys, on the
660-file corpus**, in a `cmake-verify` rebuilt at that commit. Run twice, once as the
default and once with `C2_NO_EXTRA=1`, which is the only difference between the two
columns. The `da3a461b` reading of the same pair — **194 / 104**, the numbers most of
this file still quotes — is kept in the row below each figure:

| key | `all` (the bar) | `all` with `C2_NO_EXTRA=1` | `full_language.c` costs | `c2try` |
| --- | --- | --- | --- | --- |
| x86_64 | 7 (was 7) | 7 (was 7) | — (does not enter) | 2134 |
| i386 | 11 (was 11) | 11 (was 11) | — (does not enter) | 2133 |
| x86_64-win32 | **12** (was 11) | 5 (was 4) | 7 | 2327 |
| x86_64-osx | **12** (was 12) | 4 (was 4) | 8 | 2179 |
| arm64-win32 | **16** (was 15) | 8 (was 7) | 8 | 2356 |
| i386-win32 | **17** (was 16) | 9 (was 8) | 8 | 2337 |
| arm-win32 / arm-wince | **17** each (was 16) | 9 each (was 8) | 8 | 2297 |
| arm64-osx | **19** (was 18) | 10 (was 9) | 9 | 2429 |
| arm64 | **20** (was 20) | 11 (was 11) | 9 | 2445 |
| arm | **22** (was 21) | 13 (was 12) | 9 | 2383 |
| riscv64 | **31** (was 31) | 15 (was 15) | 16 | 2378 |
| **total** | **201** (was 194) | **111** (was 104) | **90** (unmoved) | |

**The `full_language.c` costs column is unmoved row for row**, which localises the
+7: it is entirely outside that file, spread one body each across seven keys, with
x86_64, i386, x86_64-osx, arm64 and riscv64 unmoved on both columns.

The `C2_NO_EXTRA=1` column reproduces this file's historical board **row for row,
exactly**, which is what identifies it: every "104" in this document was taken with
`full_language.c` excluded, while the prose around it says *"`C2_CORPUS=all` is the
default and is the bar."* Both statements cannot be true. **194 is the bar**; 104 is
the bar minus the one file this document elsewhere calls *"the second front and the
only file that reaches classes `tests/exec` does not."*

Three consequences worth acting on:

- **`full_language.c` now enters the census on ten keys, not seven.** It compiles
  clean at `-O0`, `-O1`, `-O2` **and** `-O3` on all twelve keys — 48 of 48 rc=0,
  measured directly. The "seven keys" figure and the note that it *"aborts the C2
  probe on several keys"* were taken on a compiler that **segfaulted** on it at
  `-O2`/`-O3`; that crash is fixed and the file's census moved with it.
- **The `c2err=2` column is `full_language.c`, not a PE/Mach-O property.** It reads
  exactly 2 on the ten keys where the file enters and **0** on x86_64 and i386 where
  it does not. Earlier revisions read it as a per-target constant because those were
  the only keys the file entered on.
- **It still drops out on x86_64 and i386, and the reason is a known open defect.**
  Under `MCC_REPLAY_IR=5` both keys fail with `tests/diff/parts/legacy_meta.h:376:
  error: ';' expected (got '0')` — which is *verbatim* the `AST_OP_ASM` replay defect
  written up as P4 defect 4. The pre-flight refuses an `AST_OP_ASM` arena in
  **production**, so production is safe; the **verify** path still replays it and
  still corrupts the tokenizer stack. That defect is not closed, it is only
  contained, and it is what keeps two keys' worth of coverage off the board.

For continuity, the older `C2_NO_EXTRA=1` before → after readings at the
lost-intermediate commit, which the column above still matches:

| key | before | after | c2try |
| --- | --- | --- | --- |
| x86_64 | 7 | 7 | 2125 |
| x86_64-osx | 4 | 4 | 1873 |
| x86_64-win32 | 4 | 4 | 2058 |
| i386 | 13 | **11** | 2124 |
| i386-win32 | 9 | **8** | 2076 |
| arm | 13 | **12** | 2102 |
| arm-win32 / arm-wince | 9 each | **8** each | 2053 |
| arm64 | 11 | 11 | 2162 |
| arm64-osx | 9 | 9 | 2146 |
| arm64-win32 | 7 | 7 | 2108 |
| riscv64 | 15 | 15 | 2096 (plus 7 skip, 1 invalid) — 2086 before the `ASM_REGVAR_ASMREG` fix brought a 552nd file in |

110 → 104. `faithful` is identical on all twelve keys either side, which is the check that matters: a capture-stream perturbation shows up there first.

`C2_CORPUS=exec` for continuity with every earlier board in this file, `-O1`, gap only: x86_64 **0**, x86_64-osx **0**, x86_64-win32 **0**, i386 **0**, arm64 1, arm64-osx 1, arm64-win32 1, i386-win32 1, arm 1, arm-win32 1, arm-wince 1, riscv64 3 (plus 2 skip). Ten over twelve keys, down from 16 when the lost-intermediate class closed.

**Re-measured at `da3a461b`: the `exec` board is unmoved, row for row, all twelve
keys, gap still 10, `arm-win32 == arm-wince`.** `fn` is exactly **+1 on every key**
against the banks in **Keep the measurement honest** (x86_64 1150 v 1149, x86_64-osx
1143 v 1142, x86_64-win32 1238 v 1237, i386 1153 v 1152, i386-win32 1258 v 1257, arm
1131 v 1130, arm-win32/wince 1236 v 1235, arm64 1200 v 1199, arm64-osx 1225 v 1224,
arm64-win32 1298 v 1297, riscv64 1157 v 1156) — one body arriving uniformly, which is
the corpus growing, not a population drift. **The `BANKFN` floors were rebanked to
those figures in `bc85ce70`**; `CMakeLists.txt` now carries them and all 16 `rir-c2`
cells pass against the tightened floors.

**Forced `-O0` is 189 against the `-O1` board's 201, and tracks it to within three bodies on every key.** Re-measured at `bc85ce70` alongside the `-O1` board above, same build, same 660-file corpus (`C2_FORCE=1 C2_CORPUS=all … -O0`, 28 gates forced):

| key | `-O0` | `-O1` | Δ | bytes / len / invalid / err |
| --- | --- | --- | --- | --- |
| x86_64 | 8 | 7 | +1 | 1 / 7 / 0 / 0 |
| i386 | 9 | 11 | −2 | 1 / 8 / 0 / 0 |
| x86_64-osx | 10 | 12 | −3 | 2 / 6 / 0 / 2 |
| x86_64-win32 | 15 | 12 | +3 | 2 / 10 / 1 / 2 |
| arm64-osx | 16 | 19 | −3 | 4 / 9 / 1 / 2 |
| arm64-win32 | 16 | 16 | 0 | 2 / 11 / 1 / 2 |
| arm64 | 17 | 20 | −3 | 3 / 12 / 0 / 2 |
| i386-win32 | 17 | 17 | 0 | 3 / 11 / 1 / 2 |
| arm-win32 / arm-wince | 17 each | 17 each | 0 | 3 / 11 / 1 / 2 |
| arm | 19 | 22 | −3 | 4 / 12 / 1 / 2 |
| riscv64 | 28 | 31 | −3 | 12 / 14 / 2 / 0 (+8 skip) |
| **total** | **189** | **201** | **−12** | 40 / 122 / 9 / 18 |

**Read the two rows only as a pair taken from one build.** The previous reading of this line — x86_64 7, x86_64-osx 18, x86_64-win32 15, arm64 10, arm64-osx 21, arm64-win32 16, i386 9, i386-win32 18, arm 11, arm-win32 18, arm-wince 18, riscv64 13, total 174 — was taken against a *different* tree from the `-O1` board printed beside it, and the pair does not satisfy the three-body claim it was making: it reads arm 11 against an `-O1` of 21, arm64 10 against 20, riscv64 13 against 31. The claim is true of the `bc85ce70` pair above and was not true as previously printed. Any future `-O0` row belongs in the same commit as its `-O1` row.

**The `-O0`→`-O1` movement is almost entirely the `bytes` class.** Same-length/different-bytes goes 40 → 51 across the twelve keys while `len` is flat at 122 → 123, and `invalid` (9) and `err` (18) are identical at both levels. So the twelve bodies `-O1` loses against `-O0` are all same-length divergences. `-O2` and `-O3` were byte-identical to `-O1` on every counter when last measured on five keys. C2 replays the arena's own emission and the optimizer passes are a separate question (C3), so this is what one would expect; it means the completion bar's "at every `-O`" is close to one measurement, but the `-O0` column is cheap — the whole twelve-key sweep is under a minute on this host, not the twelve minutes this file estimates elsewhere — and it is NOT identical, so print it.

`arm-win32` and `arm-wince` share a define set and must read identically. Any sweep where those two rows differ has a harness bug, not a codegen one — the cheapest available check that a run measured what it thinks.

## Finishing RIR: the fallback census, and the five defects that block 100%

**The completion bar is `fallback == 0` with the goldens green.** Byte identity is not
the obligation; semantic correctness is. This section is the measured route to that
bar, and it replaces guessing at the C2 gap with a list of five named defects.

### CLOSED: six replay-fidelity gates were gated on the optimization level, so `-O0` replayed wrong

**The defect.** `MCC_AST_CMP_MAT`, `MCC_AST_INDIRECT_CALL`, `MCC_AST_LOOPCOND_STORE`,
`MCC_AST_STOREVAL_CALL`, `MCC_AST_STOREVAL_CALLUP` and `MCC_AST_WHILE_COMMA` all
defaulted to `o4 || s1->optimize >= 1`. None of the six is an optimization — each
reproduces a shape the parser emits at *every* level, and `31d1cd52` says so of the
first one in its subject line ("mirrors `vsetc`'s pending-comparison materialization
in replay"). They carry the `optimize >= 1` default only because that is where the
staged flip of `e9ca26c3` left them. The effect was that the replay was **wrong at
`-O0`**, not merely less optimized.

All six are unconditional now. They kept `-fno-` spellings for bisection —
`-fno-replay-cmp-materialize`, `-fno-replay-indirect-call`,
`-fno-replay-loopcond-store`, `-fno-replay-while-comma`, `-fno-storeval-call`,
`-fno-storeval-callup` — which is how the defect was proved in the first place.

**Measured at `d4bf0d27` on the arm64 host, fresh `cmake-verify`, whole corpus**
(`find tests -name '*.c'`, 569 of 662 files compile with `-I runtime/include`),
`MCC_RIR_FORCE=1 MCC_RIR_PROD=2`, 2263 bodies:

| level | `used` | `fallback` | `nomodel` |
| --- | --- | --- | --- |
| forced `-O0`, before | 2111 | **64** | 88 |
| forced `-O0`, after | 2155 | **20** | 88 |
| `-O1` (unmoved; the six were already on there) | 2168 | 7 | 88 |

The `nomodel` 88 are identical at every level and are the pre-flight, not the byte
gate: `asm` 41, `noops` 39, `regdangle` 4, `bail` 3, `invalid` 1.

**The goldens, which is the semantic bar.** `ctest -R '^exec/'` is the `-O0` family
(`MCC_TEST_OPT` default):

| leg | before | after |
| --- | --- | --- |
| baseline | green | green |
| `MCC_RIR_FORCE=1` (arena adopted, byte gate on) | green | green |
| `MCC_RIR_FORCE=1 -fno-replay-fallback` | **11 of 318 fail** | **green, 319/319** |
| `MCC_TEST_OPT=-O1 -fno-replay-fallback` | green | green |

The eleven were `builtin_overflow`, `random_stuff`, `precedence`, `bool`, `func_name`,
`signbit_inline`, `overflow_narrow`, `overflow_inline`, `floating_point`, `grep`,
`weak_undef`.

**The six are the minimal set, found by delta-debugging and not by reading names.**
All 28 derived gates on is green; greedily dropping one at a time and re-running the
last cells to fail leaves exactly these six, and the full 318 pass on the six alone.
`CMP_MAT` alone takes 11 failures to 2; `INDIRECT_CALL` takes those 2 to 1; `grep`
needs the remaining four together, which is why no single-gate sweep found it.

**The shape, in five lines** — a comparison-valued argument that is not the last
argument was overwritten by a later sibling:

```c
int a = 12, b = 34;
printf("%d, %d\n", a == a, a == b);   /* parser: "1, 0"   arena was: "0, 0" */
```

Narrowed by probe at `-O0` under `-fno-replay-fallback`, parser → arena, before the fix:

- `a == b, a + b` → `0 46` became `46 46`
- `a == b, 7` → `0 7` became `7 7` — a *constant* second argument clobbers it, so it
  was never register pressure
- `!a, a + b` → same; comparison **last**, or routed through a variable first, was fine

`ast_replay_value`'s `AST_Ref` push is a raw `vpushv`, and `vcheck_cmp()` in front of
it was what the gate controlled. With it off, two `VT_CMP` entries sat on the vstack at
once — a state the parser cannot reach, because `vsetc` flushes the pending comparison
before every push. Both arguments then materialised from the same flags.
`func_name`'s `strcmp(__func__, "main") == 0, __func__[0]` printed `match=109`, the
second argument's `'m'`, and `bool.c`'s bitfield row was the same defect with the
`bool` normalisation playing the part of the comparison.

**Why no board caught it, and this is the part worth remembering.** The documented
forced-`-O0` recipe is `C2_FORCE=1`, which forces `MCC_FORCE_REPLAY` **plus every
`optimize >= 1` gate** — that is, it forces the six gates whose absence is the defect.
So the recipe this file insists on ("Read no `-O0` differential that was taken without
it") is precisely the recipe that hides this class. The defect is only visible with
production forced and the gates left at their `-O0` defaults, which is the
configuration a real `-O0` adoption would actually ship. Any future claim that a
gate is "for optimization" needs to be checked against its use sites rather than its
default: all six read only in the replay and StoreVal reconstruction paths
(`src/mccast.c:4380`, `:4779`, `:4998`, `:5016`, `:5591-5640`).

**No-regression evidence.** Objects from the pre-fix and post-fix compilers over the
whole corpus, `SOURCE_DATE_EPOCH` pinned, at `-O0`/`-O1`/`-O2`/`-O3`: **569 identical,
0 differ on every level, 2,276 objects in total.** Expected — at `-O1`+ the six were
already on, and at `-O0` nothing adopts the arena unless forced.

**One derivation died with it, and three scripts still depend on it.**
`tools/c2_sweep.sh`, `tools/c2_equiv.sh` and `tools/o0_ab.sh` build their
forced-`-O0` env by grepping for
`ast_env_gate("MCC_AST_*", o4 || s1->optimize >= 1)`. That function no longer
exists, so the grep yields zero and all three abort — loudly, which is the one
mercy here. **They must be repointed at `src/mccopt.h`** (every row whose class is
`MCC_OPTD_LEVEL(n)` with `n <= 3`, spelled `-f<name>`) before the next forced-`-O0`
board is taken. Until then there is no C2 byte board.

**What is left at forced `-O0`: 20 fallbacks against `-O1`'s 7.** That residue is the
next measurement, and it is now a *smaller* problem than the `-O1` census was two
sections ago rather than a different one.

### The remaining fallbacks are NOT benign: `ptr_unlink` segfaults under the arena

Ten-line reproducer, no builtin, no optimizer dependence:

```c
static void ptr_unlink(void *list, void *e, unsigned next) {
	void **pp, **nn, *p;
	for (pp = list; !!(p = *pp); pp = nn) {
		nn = (void *)((char *)p + next);
		if (p == e) { *pp = *nn; break; }
	}
}
```

Parser: prints the right answer. `-fno-replay-fallback`: **SIGSEGV**. This is the shape that
takes down the JIT self-host, and it is a real miscompile rather than a byte
difference — which settles the question of whether the six/thirty-nine byte-divergent
bodies are safe to ship. **They are not.**

`RIRPRODDUMP` shows why. The arena is

```
If
  Binary op#149 (!=)
    StoreVal            <- refers to a Store that is NOT in the tree as a statement
    Literal 0
```

The `p = *pp` `Store` never appears as a statement anywhere, so nothing performs it.
Both `StoreVal` paths only produce a *value*: one reuses a live register, the other
re-derives the value expression. The condition therefore tested the right number while
`p` stayed uninitialised and the body dereferenced garbage.

**ROOT CAUSE CONFIRMED BY MEASUREMENT, and the obvious fix is a banked negative.**

The `[stmt]`/`[ent]` traces settle where the store goes:

```
[ent]  6 RBEGIN for      [stmt] node=5 kind=If      <- the loop node enters the root block
[ent] 11 OP vstore       [stmt] node=9 kind=Store   <- `p = *pp` enters the root block, AFTER it
[ent] 17 RBEGIN cond                                <- the cond region opens only here
```

**A `for`'s condition expression is captured between the `RIR_R_FOR` rbegin and the
`RIR_R_COND` rbegin.** `rir_stmt` therefore drops any store in it into the *enclosing*
block, which already holds this loop's `If`, so it replays **after** the loop and the
body runs with `p` unassigned. That is the segfault.

`rir_docond`/`rir_dheld` exist to hold exactly such stores, but `rir_docond` is armed in
one place only (`src/mccrir.c:3843`): at a `RIR_R_BODY` rend inside a `RIR_R_DO`. That
is do-while, where the condition *follows* the body — and it is why the do-while comma
case was the only member of this family ever fixed.

**Arming at the `RIR_R_COND` rbegin does nothing** — measured; the store is already
statement'd by then. **Arming at the `RIR_R_FOR` rbegin works and costs too much**:
`ptr_unlink` becomes correct *and* `used`, but `mcc.c` at `-O1` goes **39 -> 68**
fallbacks and `tests/` goes **6 -> 9**. The flag parks every store captured before the
cond region, not just the one feeding the condition, so every `for` whose condition
touches memory gets a prefix the parser never emitted. **Reverted; do not re-try it
unnarrowed.**

**The discriminator data, which is the useful artifact.** Arming `rir_docond` at the
`RIR_R_FOR` rbegin and diffing the `-O1` fallback list of `src/mcc.c` before and after:

*Fixed by arming (5)* — these are the shape that needs it:
`ptr_unlink`, `decl`, `block_cleanup`, `mcc_split_path`, `move_ref_to_global`

*Broken by arming (34)* — these must NOT get it:
`asm_expr_logic`, `asm_expr_prod`, `asm_expr_sum`, `constraint_priority`,
`cstr_vprintf`, `embed_parse_name`, `expr_landor`, `host_find_tool`, `host_spawn_ex`,
`host_spawn_run`, `host_spawn_timeout`, `ld_next`, `macro_twosharps`,
`mcc_add_linker_symbols`, `mcc_get_debug_info`, `mcc_get_dwarf_info`,
`mcc_load_archive`, `mcc_load_ldscript`, `mcc_preprocess`, `mcc_support_arch_match`,
`mccpp_new`, `next_nomacro`, `parse_comment`, `parse_define`, `parse_escape_string`,
`parse_line_comment`, `parse_number`, `parse_pp_string`, `peek_file`,
`preprocess_skip`, `struct_decl`, `struct_layout`, `subst_asm_operands`, `tok_alloc`

Net -29. **Read those 39 functions against each other before designing the
predicate** — the distinguishing shape is in there, and it is cheaper to find by
reading five that want the treatment against thirty-four that do not than by
reasoning about regions.

**The mechanism, so the narrow trigger can be designed rather than guessed.** The
`IR_OP_VSTORE` handler (`src/mccrir.c:2333`) branches on exactly these flags:

```c
if (rir_lorn || rir_ternn || rir_docond) {
    ast_add_child(n, t); ast_add_child(n, v);
    rir_push(n);          /* push the Store as a VALUE */
    break;
}
...
rir_stmt(n);              /* else: statement it, and synthesise a StoreVal for its value */
```

Pushed as a value, the condition consumes the `Store` node itself and replaying the
condition performs the store — which is why arming `rir_docond` fixes `ptr_unlink` and
even makes it `used`. Statement'd, the condition instead gets a `StoreVal`, and every
`StoreVal` path produces only a value, so the assignment is lost.

So the trigger does not need to move nodes after the fact (there is no unlink helper,
and adding one would be invasive). It needs to decide **at the vstore** whether this
store's value is what the enclosing loop's condition will consume. Region membership
alone is too coarse — that is the measured -29.

What is landed and inert, waiting for that trigger: `rir_cf_cond` parks held
condition stores for `RIR_R_WHILE`/`RIR_R_FOR` as well as `RIR_R_DO`, and the loop rend
appends the prefix for a `for` as child 3 (it required `nchild == 2`, true for while/do
and false for `for`, so the `for` replay arm's `nchild >= 4` hook could never fire).
Both replay arms already run a condition prefix. **The missing piece is a trigger that
holds only the store whose value the condition consumes** — not a region-wide flag.

### The real scope: 39 fallbacks in `src/mcc.c` at `-O1`, not 6

**Every census in this file before this point was corpus-scoped.** `tests/` is not the
workload that matters; `src/mcc.c` is the largest and most demanding input the compiler
sees, and it reads:

| input | `-O1` | `-O2` | `-O4` |
| --- | --- | --- | --- |
| `src/mcc.c` | **used 1121, fallback 39** | 1118 / 42 | 1091 / 68 |
| `tests/` (660 files) | used 2169, fallback 6 | — | — |

Classified by failing term at `-O1`: **25 `len`, 13 `bytes`, 1 error-path** (`error1`,
which N10 says not to touch). Two small examples diffed against the parser —
`so_filesize` and `ptr_unlink` — show the same shape: **the arena omits a spill the
parser emits** (`mov %rax,-0x30(%rbp)`). That is the "scheduling and allocation, not
model shape" class this file already says to *expect last*, and it is the majority of
what remains.

### `fallback == 0` is NOT one flag away — several bodies genuinely miscompile

`-fno-replay-fallback` zeroes the census and passes all 317 exec goldens at `-O1`, `-O2` and
`-O3`. It nonetheless breaks `selfhost-jit`, `selfhost-arm64-native`,
`selfhost-riscv64-docker`, `cross/no-compiler-abort-x86_64-win32` and two `exec-search`
cells. `selfhost-jit` dies as **`mcc: memory full`** with peak RSS **1,235 MB against a
184 MB baseline** — a 6.7x blowup, and a `realloc` returning NULL in `mcc_realloc`.

**`MCC_RIR_NOFB_SKIP=<names>`** is the bisection handle for this: a comma-separated
list of bodies that keep falling back even with the no-fallback path on, so the culprit
can be found without rebuilding. Bisecting the 68 `-O4` fallbacks of `mcc.c`:

- skipping all 68 -> passes
- skipping the first 44 -> passes; the first 43 -> fails
- skipping **only** `mcc_split_path` -> still fails

So this is **not one bad body**. At least a substantial fraction of the 68 emit wrong
code, and the byte-`faithful` gate has been the only thing standing between them and
the shipped compiler. The 6.7x memory blowup is consistent with a miscompiled allocator
— `host_runmem_alloc` and `cleanup_sections` are both on the fallback list — rather
than with a leak.

**The methodological lesson, which cost a wrong turn:** the 317 exec goldens are not
sufficient validation for a codegen-affecting change. They passed at every `-O` while
self-hosting broke. Run the full suite, and prefer `mcc.c` over `tests/` when measuring
the census.

**`-fno-replay-fallback` drives the `-O1` census to `used 2175, fallback 0`** and the exec
goldens are clean under it at `-O1`, `-O2` **and** `-O3` — zero regressions at every
level, after the five fixes below. So the remaining six byte-divergent bodies are
safe to ship; the byte gate is the only thing still refusing them.

**Defaulting it on does not work yet, and the reason is worth knowing.** With the
default flipped, `ctest` loses `selfhost-jit`, `selfhost-arm64-native`,
`selfhost-riscv64-docker`, `cross/no-compiler-abort-x86_64-win32` and two
`exec-search` cells. `selfhost-jit` fails as **`mcc: memory full`** — a `realloc`
returning NULL in `mcc_realloc` (`src/libmcc.c:172`), i.e. genuine exhaustion, not a
miscompile. Neither `-fno-inline` nor `-fno-reemit-templates` changes it, so it is
not the inline-retention path that first suggests itself.

**The methodological lesson, which cost a wrong turn:** the 317 exec goldens are *not*
sufficient validation for a change of this class. They passed at every `-O` while
selfhost broke. **Run the full suite before believing any codegen-affecting change** —
self-hosting is the harder bar, and it is the one that caught this.

So the remaining work for `fallback == 0` is a **memory** problem in the JIT self-host
under always-keep, not a correctness one. Start by measuring where the growth is:
every body now retains an arena that previously would have been discarded on the
fallback path, and the JIT's inner compile is where that first bites.

### OPEN: `void_expr` is a PROMOTION defect, not an arena one

`tests/exec/statements/void_expr.c::main` falls back at `-O2`/`-O3` (not `-O1`) and
miscompiles when forced through. **`-fno-promote-locals` fixes it outright** — that is
the whole diagnosis, and it clears the arena, the replay and the landor flags
(`MCC_RIR_NOMAT` and `MCC_RIR_NOINV` both leave it unmoved).

The source is a `for` loop whose body holds a **discarded short-circuit with a call in
its right operand**:

```c
for (; i < 3; ++i) {
	printf("%d\n", i);
	(void)(i || (f(i), ++count));
}
```

Promoted, `i` and `count` land in `%ebx`/`%r12d` and the loop comes out structurally
broken — `jge` to the immediately following instruction instead of the exit, the
increment block unreachable, the `||`'s short-circuit branch landing *inside* the
`f(i)` call it should skip, and no back-edge, so the body runs once and falls into the
epilogue. Output is `0 f(0)` instead of `0 f(0) 1 2 count 1`.

**Two things in `ast_plan_promotion` (`src/mccast.c:3428`) that fit the symptom:**

- **Short-circuits are invisible to it.** Its scan classifies control flow by looking
  for `AST_If` with `so == 2|3|4|5`. A landor is an `AST_Binary` (`op#145`/`op#146`),
  so a body whose only extra branching comes from `&&`/`||` is planned as if it were
  straight-line.
- **`has_loop` and `has_goto` are assigned and never read** — the planner gathers those
  facts in the same scan and discards them. Only `has_call` is used (`:3459`,
  `:3600-3604`, to pick the callee-saved pool). Whatever they were meant to gate is
  missing.

**Before changing the planner, read N15**: making `save_reg_upstack` skip pinned
registers was *"the tempting one-liner, and it is semantically right"* and moved 36
objects at `-O2`/`-O3`. Promotion changes are measured on the twelve-key board, not
reasoned about. The cheap correct-first step is to teach the scan that a landor is a
branch and see what the board says.

**This is the third latent wrong-code bug the fallback was masking** — promotion runs
regardless of `faithful`, so its output was simply discarded whenever it broke a body.
Same pattern as the fold and the fconst desync below.

### CLOSED: `ast_fold_rec` folded at the LEFT operand's type instead of the common type

**Found by watchpoint, after eight theories from source reading had all failed.** The
method that worked: `gdb --batch`, break at a probe, resolve the node index from the
arena's parallel arrays, `watch ast_cur->kind[$vc]`, continue. It named the writer in
one run — `ast_set_kind` ← `ast_fold_rec` (`src/mccast.c:5499`) ← `ast_run_templates`
← `ast_func_end:15040`.

The defect:

```c
AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
int tt = ast_type_t(a, x);                        /* LEFT operand's type only */
uint64_t r = ast_fold_eval(op, tt, l1, l2, &ok);  /* folds at that width */
```

tcc lowers unary minus as `vpushi(0); gen_op('-')`, so the left operand of a negated
64-bit constant is an **`int` zero** and the fold ran at 32 bits.
`0 - 9223372036854775807LL` came out **1**; `-9223372036854775807LL - 1` came out
**0** — both the low 32 bits of the right answer. Fixed by applying the integer half
of the usual arithmetic conversions before folding, with shifts excluded because their
result type is the promoted *left* operand's, not the common one.

**Measured effect, `-O1` over the whole corpus:**

| | before | after |
| --- | --- | --- |
| `used` | 2158 | **2168** |
| `fallback` | 17 | **7** |
| failing terms | `len 13, bytes 3, relcontent 1` | `len 5, bytes 1, relcontent 1` |
| golden regressions under `-fno-replay-fallback` | 5 | **1** (`c11_complex_convert`) |

One type bug accounted for **ten of the seventeen fallbacks and four of the five
miscompilations**. Note what had been protecting the tree: the fold was already
corrupting arenas in production, and the byte-`faithful` compare caught every instance
and fell back to the parser. **The fallback path was masking a live wrong-code bug** —
the strongest argument yet for driving the census to zero rather than living with it.

**Remaining:** `fallback` 7 (`len 5, bytes 1, relcontent 1`) and one golden,
`c11_complex_convert`. Method note stands: when source reading stalls, put a watchpoint
on the arena slot and let it name the writer.

### The census, measured at `-O1` over all 660 corpus files on x86_64

`rir_prod_take` now records *which* of its conditions refused a body
(`rir_prod_why`, `src/mccrir.c`), and `[rir-prod-why]` prints the histogram at exit.
Run it with `MCC_RIR_PROD=2`.

| verdict | n | meaning |
| --- | --- | --- |
| `used` | **2158** | arena shipped |
| `nomodel` | **57** | the pre-flight refused the body |
| `fallback` | **17** | arena accepted, then the replay's bytes differed |

`nomodel` breaks down as **`noops` 39, `asm` 10, `regdangle` 5, `bail` 3**. The
`noops` bodies have no captured ops at all and are the cheap half; `asm` is the known
`AST_OP_ASM` pre-flight refusal; `regdangle` and `bail` are the two deliberate M6
refusals.

### The `fallback` 17 are a BYTE gate, and removing it costs exactly five goldens

`faithful` in `ast_func_end` (`src/mccast.c:15065`) is a pure `memcmp` of the replay
against the parser's bytes, and `rir_prod_note(faithful ? "used" : "fallback")`
(`:15680`) is driven entirely by it. The optimizer is gated on it too —
`ast_run_strat_seq` opens `if (faithful && ast_strategies[si].gate())`
(`src/mccast.c:13044`) — so an unfaithful body is neither trusted nor optimized.

`-fno-replay-fallback` (`ast_rir_nofb_env`, off by default) accepts a byte-divergent replay
into production so the bar can be measured rather than argued about. **With it on,
`fallback` goes 17 → 0 and `used` 2158 → 2175.**

**It separates two questions that `faithful` had conflated, and that separation is the
point.** `faithful` asks *"did the replay reproduce the parser byte for byte,
relocations included"*, and that is the right gate for the **optimizer**
(`ast_run_strat_seq`, `src/mccast.c:13044`) — passes must not run over a body whose
replay was never validated. It is far too strong a question for the **keep/restore**
decision, which only needs *"is this body safe to ship"*. So the switch computes
`keep = faithful || (ast_rir_nofb_env && ast_replay_completed)` and uses `keep` only
for `rir_prod_note` and the restore, leaving `faithful` — and therefore the optimizer
gate — untouched. `ast_replay_completed` is set only where the replay ran to
completion, because the error path clears `faithful` for reasons that have nothing to
do with the byte compare and a body that longjmp'd out must never be kept.

**Forcing `faithful` itself instead is a trap, and it was measured.** Doing that keeps
the body *and* turns the optimizer loose on it in the same move, so the two effects
cannot be told apart. Under the correct separation the golden regressions go **4 → 5**
(`c11_complex_convert` joins), which says a pass had been *masking* one defect in the
un-optimized replay. **Five is the honest count**; the four was an artifact of letting
passes run.

**And the goldens say that is not yet safe.** Same build, same corpus, x86_64:

| | passed | failed |
| --- | --- | --- |
| baseline | 297 | 8 |
| `-fno-replay-fallback` | 293 | 13 |

Identical at `-O1`, `-O2` and `-O3`. The eight baseline failures are the known
batch-invocation artifacts that pass under ctest isolation; the delta is **four
regressions**, and nothing is fixed by the change:

- `atomic_inlang_rmw`
- `atomic_ptr`
- `builtin_overflow`
- `overflow_inline`

*(An earlier revision listed a fifth, `line`. That was a parsing artifact — `awk
'{print $2}'` over every line beginning `FAIL` picks words out of the mismatch
**output** as well as out of `FAIL  <name>` headers. Match `^FAIL  ` with the two
spaces. There is no golden called `line`.)*

**Worked example, `overflow_inline`.** `MCC_RIR_PROD=2` shows its `main` is the single
fallback body in the file (`used=7 fallback=1`). Forced into production it exits **5**
instead of printing `OK`, and `return 5` is
`!__builtin_add_overflow(9223372036854775807LL, 1LL, &l1) || l1 != -9223372036854775807LL - 1`
— so the arena mismodels a 64-bit signed overflow builtin. **That is wrong code, and
the byte-faithful gate is what has been preventing it**, which settles the question of
whether these 17 are benign shape differences: at least four are not. A minimal
reproducer of just that `__builtin_add_overflow` call agrees with gcc under both
settings, so this is N20 for the fourth time — work from `overflow_inline.c::main`,
not from a model of it.

**This is the single most useful thing measured about RIR so far**, because it turns
the completion bar into a finite list. The byte-faithful gate is **load-bearing for
correctness**, not merely conservative — at least four of the 17 byte-divergent bodies
are genuinely wrong when used, and `overflow_inline` above is proof rather than
inference. Two of the four names are atomics and two are overflow builtins, which
suggests the arena mismodels a lowering shared by those rather than four unrelated
defects — check `atomic_ptr` and `builtin_overflow` against the same
`__builtin_*_overflow` path before assuming otherwise.

### `overflow_inline` diagnosed: `AST_FB_LANDOR_MATERIAL` set on a runtime `||`

**Seven-line reproducer**, reduced from the real body by truncating `main` and
balancing braces — not by building a model up, which failed four times:

```c
extern int printf(const char *, ...);
int main(void) {
	long long a;
	int c1 = (!__builtin_add_overflow(9223372036854775807LL, 1LL, &a) || a != -9223372036854775807LL - 1);
	printf("c1=%d\n", c1);   /* want 0; -fno-replay-fallback prints 1 */
	return 0;
}
```

Narrowed by substitution: `!builtin` alone is correct, the builtin alone is correct,
`!builtin && cmp` is correct, and `builtin == 0 || cmp` fails identically — **so it is
not the `!`, it is the `||`**. `a` holds the right value in every case, so the store
is fine; only the OR's *result* is wrong, and it comes out **constant 1** (a second
case whose answer should be 1 also reads 1). Disabling passes does not change it, so
this is the replay and not a pass misreading a flag.

**Mechanism.** `ast_replay_value`'s materialisation arm (`src/mccast.c:4155-4163`)
mirrors tcc's `expr_landor` exactly — evaluate operands with `gvtst`, then
`vpushi(i ^ f); gsym(t);`. tcc reaches that only when the expression folds to a
constant (`cc || f`); when any operand needs runtime evaluation it takes `gvtst_set`
instead. Both operands here are runtime, so the parser used `gvtst_set` — but
`RIR_R_LANDOR`'s rend set `AST_FB_LANDOR_MATERIAL` from `ro->rval & 1` and stashed
`(ro->rval >> 2) & 1` as the node's `ival` (`src/mccrir.c:3712-3718`), so replay
pushed that bit as a constant and bound the short-circuit label after it. Both paths
then converge on one constant, which is exactly the observed behaviour.

**REFUTED — the materialisation arm is not the culprit.** The confirmation this
paragraph originally demanded was run instead of assumed, and it killed the
hypothesis. `-fno-replay-materialize` (`ast_rir_nomat_env`, diagnostic, off by default) makes
`ast_replay_value` ignore `AST_FB_LANDOR_MATERIAL` and always take the `gvtst_set`
path. With `-fno-replay-fallback -fno-replay-materialize` the reproducer **still prints `c1=1`**. So
the constant does not come from that arm, and everything above about `rval & 1`,
`ival`, and `expr_landor`'s `cc || f` condition is a plausible story that measurement
does not support. **Do not act on it.**

Bank the shape of the mistake, because it is the third time in this file: a mechanism
was reasoned out from source reading, matched the symptom exactly, and was wrong. The
`MCC_RIR_NOMAT` switch is kept precisely because it turns that class of hypothesis
into a one-command test.

**What survives** is the reproducer and the narrowing, which are measurements rather
than inference: it is the `||` and not the `!`; the stored value is correct so only
the OR's result is wrong; it reads constant 1; passes are not involved; and the
materialisation arm is now excluded. The remaining candidates are the `gvtst_set`
path and its `AST_FB_LANDOR_INVERT` handling (`src/mccast.c:4170-4176`), the
`Binary op#148` node the `!` lowered to, and the `Invoke` operand itself — the
builtin's result feeding a short-circuit is a shape `[arg]`/`[gop]` have not been
pointed at yet.

**`AST_FB_LANDOR_INVERT` is refuted as well.** `-fno-replay-landor-invert` gates the `INVERT`
swap at `src/mccast.c:4180`; the reproducer is unmoved with it on, alone or together
with `-fno-replay-materialize`. So `docs/TODO.md:491`'s long-standing suspicion — *"the same
shape as the `!cmp` defect and still live"* — is **not** what miscompiles this body,
whatever else may be true of it.

**Neither landor flag is the cause. Two hypotheses, both killed by measurement rather
than landed as wrong fixes.** That is the point of keeping `MCC_RIR_NOMAT` and
`MCC_RIR_NOINV`: each turns a source-reading hypothesis into one command.

### LOCALISED TO ~90 LINES: the tree is correct leaving `rir_prod_take` and folded to `Literal 0` before `ast_replay_body`

**Two dumps, one compile, same body** — `RIRPRODDUMP=<funcname>` prints both
(`[rir-proddump]` from inside `rir_prod_take` just before it hands the arena over,
`[ast-predump]` from `src/mccast.c` immediately before `ast_replay_body(ast_cur)`):

```
[rir-proddump] main:            [ast-predump] main:
  Store                           Store
    Ref #0                          Ref #0
    Binary -                        Literal 0        <- the whole subtree, folded to 0
      Binary -
        Literal 0
        Literal 9223372036854775807
      Literal 1
```

**The arena leaves `rir_prod_take` correct and arrives at the replay folded to `0`.**
The fold is a 32-bit truncation: `INT64_MIN`'s low half is `0`. That is the entire
`overflow_inline` miscompilation, and it happens in the window between those two
points — roughly `src/mccast.c:14944-15076`, about ninety lines.

**And nothing in that window visibly transforms the arena.** The only call taking
`ast_cur` is `ast_intention_hash(ast_cur, AST_NONE)` (`:14981`), which takes a
`const AstArena *`. The rest is `ast_arena_free` of the *previous* `ast_cur`, the
`ast_cur = ast_rir_prod` handover, per-function gate overrides from `ast_fncfg`, and
the `orig`/`ind` save. So either something in there mutates through a path that does
not name `ast_cur`, or the two dumps are not printing the same object.

**The bisect is started and the window is down to ~50 lines.** Three probes are in the
tree, all gated on the same `RIRPRODDUMP=<funcname>` and all off by default:
`[rir-proddump]` (inside `rir_prod_take`), `[ast-handover]` (before
`ast_intention_hash`), `[ast-mid]` (before `int keep_baseline = 0;`) and
`[ast-predump]` (before `ast_replay_body`). One compile prints all four, exactly once
each — verified, so this is a single pass and not two invocations being compared:

```
[rir-proddump]  Binary -      correct
[ast-handover]  Binary -      correct
[ast-mid]       Binary -      correct
[ast-predump]   Literal 0     folded
```

So the fold happens between `int keep_baseline = 0;` (`src/mccast.c:15018`) and
`ast_replay_body(ast_cur)`. **Every line in that window has been read and none of it
transforms the arena** — it is the `orig`/`orig_rel` capture, `ind = ast_body_ind_sv`,
`rsym = 0`, `nocode_wanted = 0`, the `sym_free_first`/`loc`/`anon_sym` saves, sixteen
`int … = 0` pass counters, the `setjmp` prologue, `ast_promo_n = 0`,
`ast_pinned_regs = 0`, and `rir_prod_replay_begin()`. The only calls are `mcc_malloc`,
`memcpy`, `setjmp` and `rir_prod_replay_begin`.

**The bisect was continued and the window is now ~40 lines, with the arena's identity
proven constant across it.** A fifth probe, `[ast-injmp]`, sits just inside the
`setjmp` block after `error_set_jmp_enabled = 1`:

```
[ast-mid]    arena=0x56177f7577a0   Binary -     correct
[ast-injmp]  arena=0x56177f7577a0   Literal 0    folded
```

**Same arena pointer, same root, different contents.** So this is not a handover to a
different object and not a transform — something writes into the arena's node storage.
The window is `int keep_baseline = 0;` (`src/mccast.c:15018`) to that probe, and it
contains **no calls but `mcc_malloc`, `memcpy` and `setjmp`**, plus scalar
initialisation.

Two candidates checked and eliminated: `body_len=46 rel_len=48 reloc1=48 reloc0=0`, so
neither length underflows and neither `memcpy` overruns its freshly allocated buffer;
and the handover block above is correct — `ast_arena_free(ast_cur)` frees the *previous*
arena before `ast_cur = ast_rir_prod` takes the production one.

**Heap corruption is refuted too — seventh theory down.** An
`-fsanitize=address,undefined` build of mcc **reproduces the fold** (verified: its
`[ast-mid]` reads `Binary -` and its `[ast-injmp]` reads `Literal 0`) and reports
**nothing**. So the write is in bounds and legitimate as far as the allocator is
concerned.

**And it is not an artifact of the probes.** With the three earlier dumps disabled and
only `[ast-injmp]` firing, it still reads `Literal 0`.

**What the measurements jointly say**, and this is the tightest statement available:
across `src/mccast.c:15018` → the `[ast-injmp]` probe, the arena has the **same
pointer, the same root, and the same node count (16)**, while the `Store`'s value child
changes from a `Binary -` tree to `Literal 0`. Same count with different contents means
a node was **rewritten in place** — which is exactly what constant folding does — in a
window whose only calls are `mcc_malloc`, `memcpy` and `setjmp`.

**Where that leaves it.** Every explanation reachable by reading that window has been
eliminated by measurement: no transform call, no length underflow, no buffer overrun,
no use-after-free, no sanitizer finding, no probe artifact, identity constant
throughout. Something rewrites an arena node from code that does not appear in the
window — a callback, a macro expanding to more than it looks, an `#if`-selected body
that differs from what the plain reading suggests, or a second thread. **Check what
those four lines actually expand to** (`mcc_malloc`, `memcpy` and the `setjmp` macro
are all candidates for being something other than they appear) before assuming the
source reads the way it looks.

The probes to continue with are in the tree, all under `RIRPRODDUMP=<funcname>`:
`[rir-proddump]`, `[ast-handover]`, `[ast-mid]`, `[ast-injmp]`, `[ast-predump]`.

**Confirmed against the unfiltered dumps** — an earlier `awk` filter was a plausible
confound and is not one. Full output, same run:

```
[ast-mid]   arena=0x…7a0 root=0 count=16     [ast-injmp] arena=0x…7a0 root=0 count=16
BasicBlock                                    BasicBlock
  Store                                         Store
    Ref #0                                        Ref #0
    Binary -                                      Literal 0
      Binary -                                  Invoke #0        <- identical below
        Literal 0                                 …
        Literal 9223372036854775807
      Literal 1
  Invoke #0        <- identical below
```

Only the `Store`'s value child changes; the `Invoke` and `Return` subtrees are
byte-identical, and `count` stays 16 because the folded result **reuses a node** rather
than shrinking the arena. So the rewrite is narrow and targeted, not a wholesale
rebuild — which argues for a real fold somewhere rather than a stray write.

**The macro-expansion lead is dead too.** Preprocessing with the build's own command
line (`ninja -t commands …`, strip `-MD/-MT/-MF`, swap `-c` for `-E`) shows the window
is **literally as it reads**: `mcc_malloc` is *not* expanded — the
`mcc_malloc_debug` macro at `src/mcc.h:1316` is inactive in this configuration —
`setjmp` becomes plain `_setjmp`, and the only calls between `[ast-mid]` and
`[ast-injmp]` are two `mcc_malloc`, two `memcpy` and `_setjmp`.

**So every mechanism reachable from this window has been eliminated by measurement**:
no transform call, no length underflow, no buffer overrun, no use-after-free, no
sanitizer finding, no probe artifact, no filter artifact, no hidden macro, and the
arena's pointer, root and node count are all constant across it. The observation is
stable and reproduces on both a normal and a sanitizer build.

**Eight theories have now been refuted here, all of them reached by reading source.**
Whoever picks this up should treat that as the main datum and change method rather
than generate a ninth: single-step the window in a debugger with a **watchpoint on the
`Store` node's child slot** in the arena's node arrays. That names the writer directly
and is the one approach not yet tried. `ast_dump`'s output pins which node index to
watch, and `RIRPRODDUMP` gives the exact body.

### Earlier framing, superseded: the production arena is correct but the replay does not walk it

**`RIRPRODDUMP=<funcname>` is new and is the only window onto the arena production
actually ships** (`rir_prod_take`, `src/mccrir.c`). `[rir-dump]` is gated at
`rir_env >= 6` and any `rir_env >= 1` turns production off, so that dump can never
show this object. Built precisely to test the previous section's claim — and it
**refuted** it.

For the two-line reproducer, production's own arena is right:

```
Store
  Ref #0
  Binary -
    Binary -
      Literal 0
      Literal 9223372036854775807
    Literal 1
```

The 64-bit literal is present and exact. **So the two modes do not build different
arenas, and the "MCC_REPLAY_IR changes the arena" conclusion below is wrong** — kept
only so the instrument that killed it is on record.

**What is wrong is the replay of that tree.** With
`MCC_TRACE_FILE=mccast MCC_TRACE_FUNC=ast_replay_value -v128` on the production path,
the whole store replays as a **single** leaf:

```
LEAF n=6 r=0x132 t=0x4 ival=-8      <- the store target
LEAF n=5 r=0x30  t=0x3 ival=0       <- and that is all
```

Three `Literal` nodes are in the tree; **one** is visited, the `Literal 0`, typed
`VT_INT`. The `Binary -` subtree is never walked, which is why 2 bytes are emitted
where C2 emits 10. So the defect is not the arena and not the constant's capture — it
is that **something between `rir_prod_take` and the emission replaces or bypasses the
`Binary` subtree with its left-most leaf**.

`ast_replay_body` runs `ast_finalize_storevals` and `ast_finalize_chainstores` before
`ast_replay_bb` (`src/mccast.c:5418`), and both rewrite `Store` nodes. **They are the
first place to look**, with the caveat that C2 calls the same two finalizers and does
*not* miscompile — so whatever differs is in the state those finalizers read, not in
the finalizers being called at all.

### Superseded and REFUTED: "setting `MCC_REPLAY_IR` changes the arena"

**`MCC_REPLAY_IR` is not side-effect-free with respect to the arena, so the C2 board
does not necessarily measure the arena production uses.** This is the finding that
explains every contradiction below, and it should be checked before any further work
on the fallback list.

`[optrace]` labels both replay legs `C2` (`rir_prod_replay_begin` sets `rir_c2_active`
too), so the same body can be captured in each mode and diffed. For the two-line
reproducer:

```
C2 leg  (MCC_REPLAY_IR=5):   vswap ×4, gv@11, load@11, store@21
production (unset):                     gv@11, load@11, store@13
```

Production is missing the four leading `vswap`s **and** its `load` emits **2 bytes**
where C2's emits **10** — ten bytes being a `movabs $imm64`, two being a truncated
constant. That is the `INT64_MIN` → `0` miscompilation, seen at the instruction level.

**The two runs are not replaying the same thing.** In the C2 run `rir_env >= 1`, so
`rir_verify()` replays the captured op stream *before* the arena is built; in the
production run `rir_env == 0` and verify never runs. `rir_verify` and `rir_build`
share capture state (`rir_ops`, the vstack snapshots, `rir_sh`), so if verify's replay
perturbs any of it, `rir_to_arena` produces a **different arena** in the two modes.
The op-stream diff above is direct evidence that it does.

**Consequences, in order of importance:**

1. **`c2ok` does not certify the arena production ships.** `overflow_inline.c::main`
   reads `c2ok=1` and miscompiles in production; those are not contradictory once the
   arenas are known to differ. Every use of the C2 board as evidence about production
   behaviour needs re-reading in that light — including this file's own framing of the
   gap as "how much still falls back".
2. **It explains the five refuted hypotheses below.** Each looked for a defect in the
   arena or the replay, using C2 measurements to characterise a body whose production
   arena was a different object. That is why five mechanisms each fit the symptom and
   each failed under test.
3. **It is testable directly**: dump the arena in both modes and compare. That needs a
   dump reachable with `rir_env == 0`, which does not exist today — `[rir-dump]` is
   gated at `rir_env >= 6`. **Adding a production-side arena dump is the first thing to
   build**, because without it no statement about production's arena can be checked.

### Superseded: production's replay prologue does not mirror C2's

`faithful` is a four-term conjunction and now records **which** term failed
(`rir_unfaithful_why`, printed as the fifth field of `[rir-prod]`). Split across all 17
fallbacks at `-O1` over the corpus:

| failing term | n | meaning |
| --- | --- | --- |
| `len` | **13** | the replay emitted a *different amount* of code |
| `bytes` | 3 | same length, different bytes |
| `relcontent` | 1 | code identical, relocations differ |

So the relocation theory is **wrong** — 16 of 17 are genuine code differences. Bodies:
`coherency_test`, `test16`, `test17`, `s7_6_inttypes_test`, `s_stddef_stdint` and nine
`main`s on `len`; `s7_22_intarith_test`, `s7_9_iso646_test`, `s7_9_limits_test` on
`bytes`; one `main` on `relcontent`.

**And here is the contradiction that names the defect.** `overflow_inline.c::main`
fails on **`len`** — production's replay emits a different *length* — while C2's replay
of the **same arena** is byte-identical (`c2ok=1`). Both call `ast_replay_body` on the
same nodes, and `faithful` is computed *before* any pass runs (`src/mccast.c:15065`,
passes at `:15158`), so the optimizer is not involved. The only thing that differs is
**the prologue**:

- C2's, `src/mccrir.c:4785-4822`, resets `ind`, the reloc offsets, `nocode_wanted`,
  `cg_func_alloca`, `nb_stk_data`, `arr_temp_local_vars`, the whole `vstack`, `loc`,
  `anon_sym`, `ast_pinned_regs`, the break/continue/switch symbols, `sym_free_first`
  and the label allocator.
- `rir_prod_replay_begin`, `src/mccrir.c:4498-4515`, resets the record indices,
  `ast_fconst_i`, `ast_locrec_i`, `ast_replaying`, `ir_cap_replaying`,
  `rir_c2_active`, the label allocator, the break/continue/switch symbols,
  `ast_temp_frontier` and `loc` — and **not** `anon_sym`, `ast_pinned_regs`,
  `sym_free_first`, `nb_stk_data`, `nocode_wanted` or `cg_func_alloca`.

**`docs/TODO.md`'s own rule says exactly what that costs**: *"Keep the C2 harness
mirroring the tree's replay prologue exactly… Leftover allocator state reads as a
codegen difference; one omission, a dirty vstack, costs 194 bodies."* The rule has
always been written as an obligation on C2. **The measurement says the omission is on
the production side**, which is why C2 is byte-exact where production is not.

**Most of that list is a false alarm — production resets them at the CALL SITE, not in
`rir_prod_replay_begin`.** `src/mccast.c:15014-15033` already does `ind =
ast_body_ind_sv`, `nocode_wanted = 0`, `sym_free_first = NULL`, and saves `loc`/
`anon_sym`; `:15070-15072` adds `ast_promo_n = 0` and `ast_pinned_regs = 0`. Compare
the *combined* prologue, not `rir_prod_replay_begin` alone, or you will chase resets
that are already there.

**The one real difference was tried and does nothing.** C2 clears
`mcc_state->cg_func_alloca` (`src/mccrir.c:4825`) and production does not — and
`test16`/`test17`, the two `alloca` bodies, are among the `len` failures, so it looked
compelling. Adding it to `rir_prod_replay_begin` leaves the census **bit-for-bit
identical**: still `used 2158, nomodel 57, fallback 17`, still `len 13, bytes 3,
relcontent 1`. Reverted rather than landed, because an inert change is noise. **Do not
re-try it.**

So the prologue-asymmetry theory is **refuted** along with the four before it. What
still stands, and is not explained by any of them, is the bare contradiction: for
`overflow_inline.c::main`, C2's replay of the arena is byte-identical while
production's differs in *length*, with no pass involved and the prologues now known to
be equivalent on every field anyone has checked. Whatever is left is something the
production path does around `ast_replay_body` that C2 does not, and it is not state
initialisation. Instrument the two emissions directly — `[optrace]` labels the C2 leg
`C2`, so capture both streams for one body and diff by `ind=` — rather than reasoning
about which globals differ.

### Superseded reading: the arena is byte-exact on the reproducer

The two-line reproducer below compiles to a body that reports **both** of these:

```
[rir-c2part] main ok=1          <- arena re-emits BYTE-IDENTICALLY to the parser
[rir-prod]   fallback  main     <- production rejects the same body
```

C2 replays the arena with **no passes** and matches the parser exactly, so **the arena's
reconstruction of this body is correct**. Whatever makes production reject it is not the
code bytes. `faithful` (`src/mccast.c:15065`) is a conjunction, and only its first two
terms are the `memcmp`:

```c
faithful = new_len == body_len && memcmp(...) == 0 &&
           new_rel - ast_reloc0_sv == rel_len &&
           (rel_len == 0 || ast_reloc_range_equiv(rsec2->data + ast_reloc0_sv, orig_rel, rel_len));
```

so a **relocation** mismatch fails it just as surely as a byte one, and this body's bytes
are provably fine. **Split the 17 by which term fails before treating any of them as
arena defects** — the census currently conflates "the arena emitted different code" with
"the relocations did not line up", and at least one body is the second kind.

That also relocates the wrong code. `-fno-replay-fallback` does two things, not one: it keeps
the replayed body *and* it makes `faithful` true, which is what
`ast_run_strat_seq` gates the **optimizer** on (`src/mccast.c:13044`). So forcing this
body through runs passes over it for the first time. The miscompilation below may
therefore be a **pass** defect exposed by the switch rather than an arena defect —
which is the opposite of what the rest of this section assumed. An earlier
`MCC_AST_OPT_LIMIT=0` probe appeared to rule passes out, but `docs/TODO.md`'s N17
warns that gate name produces no measurable change and must not be trusted; confirm
with `MCC_AST_REPLAY_DUMP=1` and the `[ast-*]` counters instead.

### The reproducer: a folded negative 64-bit constant replays as `Literal 0` typed `VT_INT`

**Two lines, no builtin, no `||`, no call:**

```c
long long a = -9223372036854775807LL - 1;   /* arena: 0   gcc: -9223372036854775808 */
long long e = 0 - 9223372036854775807LL;    /* arena: 1   gcc: -9223372036854775807 */
```

against `9223372036854775807LL - 1` and `4294967296LL + 1`, which are both **correct**.
The failing pair are exactly the ones whose result is negative and whose folded value's
low 32 bits are degenerate: `INT64_MIN` is `0x8000000000000000`, low half `0`, and
`0 - 0x7FFFFFFFFFFFFFFF` wraps to `1` in 32 bits. Both observed values are the 32-bit
truncation of the right answer.

**The `LEAF` trace names it.** With `MCC_CONFIG_TRACE=ON`,
`MCC_TRACE_FILE=mccast MCC_TRACE_FUNC=ast_replay_value -v128`, the first line replays as

```
LEAF n=5 r=0x30 t=0x3 ival=0
```

`t=0x3` is `VT_INT` and `ival=0`, where the node must be `VT_LLONG` holding
`INT64_MIN`. The arena's own storage is not the problem — `ival` is a `uint64_t *`
(`src/mccast.c:99`) and `rir_leaf_slot` copies the whole `sv->c.i` and `sv->type.t`
(`src/mccrir.c:1153-1166`). **So the wrong value is already in the SValue the leaf is
captured from, or the capture takes the wrong leaf.**

The likeliest shape, and the thing to check first: tcc lowers unary minus as
`vpushi(0); vswap(); gen_op('-')`. The `vpushi(0)` is an `int` zero. The parser then
*folds* the pair into one `VT_LLONG` constant, but the arena has already recorded a
`Literal 0` of type `VT_INT` for the pushed zero. If the fold collapses the parser's
vstack without the arena's shadow stack replacing that leaf with the folded value, the
arena keeps the `int 0` and the big operand is gone — which is precisely `a=0`. Note
the earlier `[rir-dump]` of a *comparison* context preserved the unfolded
`Binary -(Binary -(0, INT64_MAX), 1)` tree, so the two paths behave differently and
that difference is the lead.

**This is a real wrong-code defect independent of everything else in this section** —
no builtin, no short-circuit, no call. It explains `overflow_inline` (whose failing
check compares against `-9223372036854775807LL - 1`) and it is the first thing to fix.
Check whether `builtin_overflow`, `atomic_ptr` and `atomic_inlang_rmw` share it before
treating them as separate defects.

**Where the earlier search stood, superseded by the above.** `&&` is correct and `||` is not,
and both go through the same arm — the only difference is `i = (bop == TOK_LAND)`
feeding `gvtst`/`gvtst_set`. Since the materialisation and invert paths are both
excluded, the remaining candidates are the `gvtst_set(0, t)` path itself for `||`,
and the operand shape: the arena rebuilds `!builtin` as a real
`Binary ==(Invoke, Literal 0)` node, where the parser may have realised the `!` by
manipulating jump flags without ever emitting a comparison. A reconstructed comparison
feeding a short-circuit is a different thing from a flag feeding one, and that
asymmetry is the next thing to test — `builtin == 0 || cmp` fails identically, so the
explicit form is no safer than the `!`.

**The route to the bar, in order:**

1. Fix the five above. Reproduce each with
   `-fno-replay-fallback MCC_TEST_OPT=-O1 <builddir>/exec_runner … --only <name>`, and
   diff its output against the baseline run. These are real wrong-code defects and
   the goldens name them precisely — no oracle, no byte board, no bisect needed.
2. Then `noops` 39 — confirm an empty arena is legitimate for a body with no captured
   ops (prologue and epilogue are emitted outside the body), in which case these are
   a naming problem rather than a gap and the census should count them separately.
3. Then `asm` 10, which is P4 defect 4 and the one genuinely hard item.
4. `regdangle` 5 and `bail` 3 are deliberate M6 refusals; each needs its shape
   modelled rather than refused, and `docs/TODO.md:760` already argues modelling
   beats refusing where both were tried.
5. Only when `fallback == 0` **and** the goldens are green on every executable key
   does the fallback path come out.

**Do not turn `MCC_RIR_NOFB` on by default until step 1 is done** — it is a
measurement switch, and with five known regressions behind it, shipping it would be
shipping wrong code.

## What the C2 gap actually measures — read this before treating a divergence as a bug

**The C2 gap is a migration-completeness number, not a defect count.** This was got
wrong once in this file, at length, and the correction is worth stating first because
every "still open" entry below reads differently under it.

**Correctness is measured by the goldens, and that bar is already green.** 8255 ctest
cells, of which the `exec` families check the same `tests/exec/goldens.h` expected
output at `-O0` (`exec/`, `MCC_TEST_OPT` default, `tests/runner.c:574`), `-O1`, `-O3`
and `-Os`, all passing. `-O0` is independently corroborated against gcc and clang.
That is the oracle, it covers unit through integration, and nothing below changes it.

**A C2 divergence is not a golden failure, and usually cannot become one.** C2 is a
*verification* leg: it re-emits from the reconstructed arena into the text section,
compares, and then restores the parser's bytes (`src/mccrir.c:5017`). Its output is
discarded by construction. What ships is decided separately by `rir_prod_take`'s
pre-flight, which **refuses** shapes it cannot model and falls back to the parser's
bytes for that body.

So the gap splits in two, and only one half is a correctness question at all:

- **Inert** — byte-divergent *and* refused by the pre-flight. The parser's bytes
  ship, the divergence never reaches a binary, and the number is telling you how much
  still falls back rather than how much is wrong. Verified per body with
  `MCC_RIR_PROD=2`: `coherency_test`, `bounds_stress::test16`/`::test17`,
  `rev64_mt::main` and `s7_9_iso646_test` all read `[rir-prod] fallback`.
- **Live** — byte-divergent *and* used in production. The arena's bytes ship and
  differ from the parser's, so correctness there rests on the two being equivalent.
  `tests/fuzz/runner.c` is this case: `[rir-prod-total] used=49 fallback=0`, with
  `triage`/`interesting`/`main` C2-divergent and shipping anyway.

**Only the live half needs a semantic argument.** The inert half needs a *model*
improvement to shrink the fallback list, which is the deletion's completeness goal,
not a bug hunt. Any entry below that reads like a wrong-code claim must first be
checked with `MCC_RIR_PROD=2`; if it says `fallback`, the divergence is inert and the
urgency is wrong.

### The counters, and what they should be

`[rir-total]` prints `c2equiv=` and `c2unproven=`, partitioning `c2bytes + c2len`,
fed by `rir_c2_equiv_proven` (`src/mccrir.c:4423`) reading a `RIREQUIV` name list.
`tools/c2_sweep.sh` carries it as `equiv=N/M`. **As shipped it defaults to proving
nothing, so every key reads `equiv=0/N`, and in that state `c2unproven` merely
restates `bytes+len`.**

**The useful redefinition, which needs no oracle: `c2unproven` should be
byte-divergent AND live.** That is the set where correctness actually depends on
equivalence, it is derivable from the pre-flight verdict the compiler already
computes, and it turns the column into a number with a meaning instead of a
placeholder. The obstacle is scheduling, not information: `rir_prod_env =
ast_replay_env && !rir_env`, so during a `MCC_REPLAY_IR=5` run production is off and
`rir_prod_take` returns NULL — the C2 leg cannot read the verdict it needs. Factoring
the pre-flight predicate out of `rir_prod_take` so both legs can call it is the whole
change. Until that lands, treat `equiv=0/N` as unpopulated rather than as a finding.

**No in-compiler prover, and the reason is not effort.** `mcc_disasm_insn`
(`src/mcc.h:1719`) returns instruction text and boundaries, not a read/write set per
instruction. The only check implementable on that interface — a permutation over
instruction encodings — calls two reordered memory operations equivalent when they
are not. Making it sound needs per-ISA effect models for six architectures. **Do not
implement a byte-level or encoding-level prover.** If one appears in a diff, it is
unsound unless it carries those models.

### How to localise a failure to the arena: differential execution

`rir_prod_env = ast_replay_env && !rir_env` (`src/mccrir.c:521`), with no gate term.
That single line is the whole instrument:

- **`MCC_REPLAY_IR` unset, `-O1`+** — production is on and the **arena's** emission
  ships.
- **`MCC_REPLAY_IR=1`, `-O1`+** — verify mode, production off, the **parser's** bytes
  ship.

So the same source, built twice with one env var flipped, produces two binaries that
differ exactly where the arena and the parser differ. Run both, compare observable
behaviour, and a divergence names the arena as the culprit. It is
architecture-independent, needs no effect model, and is indifferent to looping and
nesting because it observes the whole program.

**Keep its status straight.** This localises; it does not certify. The exec suites
already run with production on, so the arena's emission is validated by execution for
every body those tests reach — that validation comes from the goldens, and it is
present whether or not anyone ever runs the flip. A clean differential adds
reassurance, not a completion claim. Its real value is the failing case: when a
golden goes red, one run says arena or optimizer instead of a bisect.

### Measured at `9d588502`: clean on all five executable keys, at every `-O`

**`tools/c2_equiv.sh <builddir> [key|all] [opt] [outdir]`** is the harness. It needs
no new comparison logic — `tests/runner.c` already compares program stdout against a
golden (`texts_equal`, `tests/runner.c:743`) — so a key's leg is *"did any golden
change verdict"*, and the script is the env plumbing plus two refusals.

**Fifteen cells, five keys x `-O1`/`-O2`/`-O3`. As first measured — before the
`nb_seqp` fix below, which is what the one differential turned out to be:**

| key | passing goldens (arena/parser) | differential |
| --- | --- | --- |
| x86_64 | 299 / 298 | `struct_init` only |
| arm64 | 250 / 249 | `struct_init` only |
| riscv64 | 247 / 246 | `struct_init` only |
| i386 | 246 / 245 | `struct_init` only |
| arm | 241 / 240 | `struct_init` only |

Identical at all three `-O` levels, and `arena-only` is **empty everywhere** — no
golden ever fails under the arena that passes under the parser.

**The one differential was a real defect in the replay harness, and it is fixed.**
`struct_init` failed in the parser leg because that leg emitted a diagnostic the
arena leg did not — `struct_init.c:407: warning: operation on 'i' may be undefined`,
where line 407 is `tst_bf arr[] = {{1, 2, 3}};`: no `i`, no sequence point. **The
replay prologue already sets `mcc_state->warn_none = 1`, so the warning was not
raised during replay — its *events* outlived it.** `seqp_record_sv`
(`src/mccgen.c:371`) appends to `mcc_state->nb_seqp` on every store, replay re-runs
the same stores, and the next `seqp_flush()` at a statement boundary in the parser —
with warnings back on — saw the parser's write plus the replay's and counted two
writes to one object. The line number was wrong for the same reason: it was wherever
the parser had reached by then. Fixed by saving and restoring `nb_seqp` and
`seqp_overflow` across the replay, alongside the state the prologue already handles.
**Exactly the leftover-state class "Keep the measurement honest" warns about**, and
the second instance of it — one omission there once cost 194 bodies.

**After the fix all fifteen cells read `differential: NONE`** and both legs report
equal pass counts (299/299, 250/250, 247/247, 246/246, 241/241). The byte board is
identical on every counter either side of the fix, which is what a diagnostic-only
change must be.

So across every key whose output this host can execute, **the arena's emission and
the parser's bytes are behaviourally identical at every optimisation level**, against
a byte board reading a gap of 201. That difference — 201 counted, none observable —
is the whole argument for the second column.

**Three things this result is not.** It does not attribute per body: it proves
*programs* behave the same, not which bodies ran, and a body on an untaken path is
not witnessed. It covers the `exec` corpus only, not the 660-file `all` corpus the
byte board uses. And eight goldens (`atomic_misc`, `bound_global`, `bound_test_b`,
`builtins`, `errors_and_warnings`, `grep`, `nodata_wanted`, `run_atexit`) fail in
**both** legs under a single batch invocation while passing under ctest's per-test
isolation, so they cancel out of the differential but were never compared.

**`MCC_REPLAY_IR=1` was not diagnostic-transparent; that was a defect in the control
leg itself and is now closed** (the `nb_seqp` leak above). Recorded because the shape
recurs: a divergence whose only observable is a diagnostic, same family as the
`combine_types` "pointer type mismatch in comparison" case. The temptation when one
appears is to filter diagnostics out of the compared text — **do not**, because the
compared text is the only thing that surfaced this one. Fix the leak instead.

**Anything the replay prologue does not save is a candidate for the same bug.** The
prologue resets `vstack`/`vtop`, `loc`, `anon_sym`, `ast_pinned_regs`, the
break/continue/switch symbols, `sym_free_first`, `ast_fconst_i`, `ast_locrec_i`,
`nb_stk_data`, the label allocator, and now `nb_seqp`/`seqp_overflow`. Two more
diagnostic-side pieces of state are mutated by `seqp_record_sv` and are **not** saved:
`obj->a.inited`, which drives `"'%s' is used uninitialized"`, and `obj->a.addrtaken`.
Replay setting `inited` on a `Sym` can only *suppress* a later warning, which is why
no golden caught it — but it is the same leak with a quieter symptom.

### The cross-key trap, paid for once already

**Do not read a cross-key differential without reading its pass count first.** The
four ELF keys were run under `qemu-user` and all four reported *"differential:
NONE"* — which was **worthless**: `0 passed, 252 failed (compile), 65 skipped` on
i386, and the same shape on arm, arm64 and riscv64. Nothing compiled, so nothing was
compared, and a green-looking row came out of an empty population. This is the trap
`tools/o0_ab.sh:174` guards for object banks and it has now been hit once on the
execution side too.

**All three causes are now handled inside `tools/c2_equiv.sh`**, and the refusal is
built in: it prints `UNMEASURABLE` and exits non-zero for any key whose pass count is
zero, rather than a differential. Keep that refusal. The causes, recorded because
each one produced a plausible-looking wrong answer:

- **The sysroot flags.** The runner takes `MCC_TEST_SYSROOT` (`tests/runner.c:578`)
  and builds `--sysroot`, `-isystem`, and the four `-L` paths itself
  (`tests/runner.c:710-713`), including the `usr/lib64`/`lib64` pair `arm64` and
  `riscv64` need or `crt1.o` is not found. Not passing it fails every compile.
- **`qemu-user` needs `-L <sysroot>` too, not just mcc.** Without it the emulator
  resolves the interpreter against the **host** root, so an i386 binary loads the
  host's x86_64 libc and dies with *"CPU ISA level is lower than required"* — which
  reads as a codegen failure and is not one. The symptom after fixing only the
  compiler flags is worse than the one before: compiles succeed, every program
  produces empty output, and every golden reports `(mismatch)` rather than
  `(compile)`.
- **`MCC_TEST_RUNEMU`, not `MCC_TEST_EMU`.** `MCC_TEST_EMU` prefixes the **compiler**
  invocation (`tests/runner.c:684`, `:690`, `:696`, `:703`, `:716`); `MCC_TEST_RUNEMU`
  prefixes the **produced executable** and is what sets `cross` (`:577-579`, and
  `cross ? "" : emu` at `:716`, `cross ? runemu : emu` at `:726`). Setting the former
  for a cross key runs `qemu-i386` on `mcc-i386` — a host x86_64 binary that merely
  *targets* i386 — and yields *"Invalid ELF image for this architecture"*.

### What to build, in order

1. ~~**`tools/c2_equiv.sh`**~~ — **done**, and clean on all five keys it can reach.
   ~~Still to add: the two x86 PE keys~~ — **the PE arm landed and was measured
   *natively* on the Windows x86_64 host (2026-08-03, `ad0dc1e0`), which is a
   stronger reading than the planned `wine` leg**; see **the Windows measurement
   below**. The differential now covers seven of twelve keys. A `wine` leg on the
   Linux host would only re-measure the same two keys under emulation — redundant
   now, do not build it. The remaining five are Mach-O and ARM/ARM64 PE, W1/W2,
   and stay byte-only.
2. **A per-body execution tap**, so a passing program can be attributed to the bodies
   it actually ran. Without it the harness proves "this program behaves the same",
   which is not the same claim as "this body is equivalent" — a body on an untaken
   path proves nothing. This is the load-bearing half and the reason step 1 alone is
   not enough. **See the section below for why the obvious implementations do not
   work and what does.**
3. **Feed the attributed body names to `RIREQUIV`** and re-measure. `c2unproven` is
   then the real number: byte-divergent *and* unwitnessed by execution.
4. **A purpose-built control-flow corpus**, because the claim is "regardless of
   looping/nesting/complexity" and coverage is what makes that true. The existing
   corpus was not written to stress the arena. Needed shapes, each with observable
   output and each currently under-represented: nested loops with `break`/`continue`
   to outer levels, `switch` inside a loop with fallthrough, `goto` webs and computed
   `goto`, deeply nested ternaries, long `&&`/`||` chains as loop and ternary
   conditions, statement expressions in argument position, VLA and `alloca` inside
   loops, compound literals in expressions, chained assignment across struct members,
   recursion, and `setjmp`/`longjmp`. Four of the five open classes below are exactly
   these shapes, which is the tell that the corpus gap and the defect list are the
   same list.

### A per-body execution oracle is NOT needed. Do not build one.

**This was proposed in an earlier revision of this section and the reasoning was
circular.** The argument was: the whole-program differential proves "this program
behaves the same", not "this body is equivalent", because a body on an untaken path
is unwitnessed — therefore build a tap that attributes execution per body.

The flaw is that an unwitnessed body is a **test-coverage** gap, and the answer to a
coverage gap is a test, not an oracle. Detection is already covered: 8255 goldens
check expected output at four `-O` levels, so an arena miscompile in anything a test
touches fails a golden, and `tools/c2_equiv.sh` then localises it to the arena or the
optimizer in one run. Nothing about that chain needs to know *which* body ran; when a
golden fails you bisect. Building an observer to answer a question the suite already
answers is machinery for its own sake.

**What replaces it, if you want per-body confidence:** a test, not an oracle. Give
the pre-flight a `RIRONLY=<funcname>` filter (`rir_prod_take`, `src/mccrir.c:4424` —
the M6 refusal site, which cannot move a C2 counter by construction), compile a
golden with the arena enabled for exactly one live divergent body and the parser's
bytes everywhere else, and run it. Golden output means that body's emission is
correct **in isolation**, which is stronger than the whole-program run because it
cannot be masked by another body compensating. Cost is one compile-and-run per live
divergent body, seconds at 3.6s a compile, and it works identically on keys that
cannot execute here because the verdict is the golden, not a trace. This is worth
doing only for the **live** half of the gap; the inert half ships nothing.

### Historical note: why `MCC_TRACE` and `fprintf` were never the mechanism

**Both instrument the wrong process.** `MCC_TRACE`, and any `fprintf(stderr, "%s:%d",
__FILE__, __LINE__, ...)` added to `src/`, are compiled into **mcc**. They report
which *compiler* functions ran while compiling. The question the tap has to answer is
which *compiled body* ran when the **test program** was executed, minutes later, in a
different process — often under `qemu` or `wine`. No amount of instrumentation in the
compiler can observe that, because the compiler is not running at the time. This is
the same category error as trying to prove program equivalence by diffing `-O0`
against `-O1` compiler traces.

To observe the test program from inside, mcc would have to **emit** the tap into the
code it generates — a function-entry counter per body. That is a codegen change, and
it moves every byte in the corpus, so it cannot run in the same build the byte board
measures. It is not disqualified by that (the differential compares two legs and
identical instrumentation cancels), but it is a large change to gain something two
cheaper routes already give.

Emitting a tap into generated code would work but is a codegen change that moves
every byte in the corpus, and per the section above it is not needed anyway.

### `-O0` versus `-O1+` IS the correctness oracle. The `MCC_REPLAY_IR` flip is the localiser.

These answer different questions and an earlier revision of this file conflated them,
objecting to the first because *"at `-O0` no arena is built"*. That objection is
technically true and beside the point:

- **`-O0` vs `-O1+` against the goldens is the correctness bar.** `-O0` is the
  reference — corroborated by gcc and clang — and the goldens encode the expected
  output, so every `-O` level is checked against the same ground truth. A divergence
  anywhere in that matrix is a real bug in whatever the higher level added, arena and
  optimizer together. This already runs as the `exec/`, `exec-O1/`, `exec-O3/` and
  `exec-Os/` families and is green. **It is the primary bar and it does not need
  Replay_IR to be mentioned at all.**
- **The `MCC_REPLAY_IR` flip is what you reach for when that bar goes red**, because
  it holds `-O` constant and changes only whose bytes ship (`rir_prod_env =
  ast_replay_env && !rir_env`). It answers "arena or optimizer", which the `-O0`
  comparison cannot. That is `tools/c2_equiv.sh`, and it is a debugging saving, not a
  completion criterion.

Fifteen clean `c2_equiv` cells are therefore reassurance rather than proof of
anything the goldens had not already established.

### The JIT is the natural next consumer

`mccjit_recompile_common` (`src/mccjit_embed.c:571`) drives runtime recompilation and
its promotion path runs the same arena, so the same flip applies unchanged — run a
JIT workload twice with `MCC_REPLAY_IR` flipped and compare observable behaviour. It
reaches what the static corpus cannot: bodies only ever recompiled, and the promotion
decisions themselves. Two cautions. `embed_jit` is a term in `ast_replay_env`
(`src/mccast.c:1923`), so the JIT arms the arena independently of `-O` and the flip
may not partition the same way — check that first. And P4 defect 3 was a JIT-seam
defect (`7133295b`, the runtime recompile lacking an error `jmp` context), so the seam
has form.

**The JIT extension is real and is the natural next consumer.** `mccjit_recompile_common`
(`src/mccjit_embed.c:571`) drives runtime recompilation, and its promotion path runs
the same arena. The same differential applies unchanged — run a JIT workload twice
with `MCC_REPLAY_IR` flipped and compare observable behaviour — and it reaches
something the static corpus cannot: bodies that are only ever recompiled, and the
promotion decisions themselves. Two cautions before building it. `embed_jit` is a
term in `ast_replay_env` (`src/mccast.c:1923`), so the JIT arms the arena
independently of `-O` and the flip may not partition the same way; check that first.
And P4 defect 3 was a JIT-seam defect (`7133295b`, the runtime recompile lacking an
error `jmp` context), so the seam has form.

### For the Windows and macOS hosts: run the goldens on the five keys that have never executed them

**The point is that five of twelve keys have never had their goldens executed at
all** — not that a "semantic bar" needs validating. `x86_64-osx`, `arm64-osx`,
`arm64-win32`, `arm-win32` and `arm-wince` are compiled and byte-compared and nothing
more, so the 8255-cell correctness oracle that covers the other seven has simply never
run on them. That is the gap, and `ctest` closes most of it; the `c2_equiv` run is the
cheaper follow-up that says *arena or optimizer* if a cell goes red.

**Down from seven, then from five to three**: the Windows host measured
`x86_64-win32` and `i386-win32` natively on 2026-08-03 and both are clean at every
`-O` (see item 2 below); the **macOS arm64 host measured both Mach-O keys the same
day** and both are clean at every `-O` *including a non-vacuous forced `-O0`* (see
the macOS item below). Their byte gaps — 12 and 14 on PE, 13 and 20 on Mach-O —
turned out to be the same benign class as the five Linux keys, which is exactly the
outcome the inert/live split predicts and a useful confirmation that a byte gap is
not a defect count. **The three that remain are `arm64-win32` 16, `arm-win32` 17 and
`arm-wince` 17, all Windows-on-ARM (W2/W5)** — **listed as census, not as
suspicion**.

Priority order is `ctest` first, `c2_equiv` second. An earlier revision had this
backwards, on the since-corrected assumption that a byte gap was evidence of a defect
needing semantic adjudication. It is not — see the inert/live split at the head of
this section — so the byte gaps on those keys (`x86_64-osx` 12, `arm64-osx` 19,
`arm64-win32` 16, `arm-win32` 17, `arm-wince` 17) are **not** the reason to run
anything. The reason is that the goldens have never executed there.

**macOS (both hosts, or one Apple-silicon machine for both keys).**

1. `cmake -S . -B bc2 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMCC_ENABLE_CROSS=ON -DCMAKE_C_FLAGS=-DMCC_REPLAY_IR_C2=1 && ninja -C bc2`
2. `ctest --test-dir bc2 -j 8` — a full suite on a Mach-O host, which this file has
   never had a clean reading of. Expect `selfhost-fixpoint-O3` to need attention: W3
   records it as *"green again by side effect, and the defect underneath it was never
   found"*, and macOS is the only host that can tell whether it is fixed or dormant.
3. ~~`tools/c2_equiv.sh bc2 all -O1` and again at `-O2`/`-O3`. **The script needs a
   Mach-O arm first.**~~ — **done and measured, 2026-08-03 on the macOS 26.5.2
   arm64 host** (`cmake-c2all`: Debug, `-DMCC_ENABLE_CROSS=ON`,
   `-DCMAKE_C_FLAGS=-DMCC_REPLAY_IR_C2=1`, rebuilt at `ad0dc1e0`; rows re-verified
   against the script as rebased onto `68025a58`). **Eight cells, two keys x
   forced `-O0`/`-O1`/`-O2`/`-O3`, every one `differential: NONE`:**

   | key | passing goldens (arena/parser) | differential | uncompared (fail both legs) |
   | --- | --- | --- | --- |
   | arm64-osx (native) | 295 / 295 (of 317, 22 skip) | NONE at all four `-O` | none — zero batch failures |
   | x86_64-osx (Rosetta) | 251 / 251 (of 317, 65 skip) | NONE at all four `-O` | `c11_freestanding_headers` |

   `arena-only` is empty on all eight. Four findings the next reader needs:
   - **The prescribed flags above were wrong, the same way the Windows ones were.**
     `-B runtime -I runtime/include` is `tools/c2_sweep.sh`'s compile line, not the
     runner's: `tests/runner.c` takes a *bdir* and emits a single `-B`, and what
     works is `-B <builddir>` — the Mach-O runtime pieces are already there as
     `<builddir>/arm64-osx-libmccrt.a` and `x86_64-osx-libmccrt.a`, so no synthetic
     `-B` dir is needed the way `i386-win32` needed one.
   - **`x86_64-osx` needs the Rosetta prefix and the SDK sysroot *together*, and
     neither works without the other.** A cross Mach-O link needs the SDK's
     `libSystem`, and the runner only emits `-L<sys>/usr/lib` when it thinks it is
     cross — which it decides from `MCC_TEST_RUNEMU` alone (`tests/runner.c:578`).
     So the *run* prefix is also what turns the *link* flag on. Without the pair,
     every compile fails with `library 'c' not found` /
     `unresolved reference to '_memset'`, which reads as a codegen failure and is
     not one. `MCC_TEST_SYSROOT` is `xcrun --show-sdk-path`; `MCC_TEST_RUNEMU` is
     `arch -x86_64`. Both are in the script.
   - **`MCC_TEST_OS=Darwin` and `MCC_TEST_CPU=arm64|x86_64` are required**, or
     `req_met` (`tests/runner.c:32-33`) runs the ELF-only goldens and reports them
     as mismatches. This is why `x86_64-osx` skips 65 against `arm64-osx`'s 22.
   - **The `-O0` row here is NOT the vacuous control the Windows `-O0` row was**,
     because `tools/c2_equiv.sh` now has a `C2_FORCE=1` arm — see below.
4. `C2_NO_EXTRA=1 O0_AB_CHECK=1 tools/o0_ab.sh bc2 all` — the `-O0` object bank was
   taken on Linux; confirm it reproduces. **Not run yet.**

**`C2_FORCE=1` makes the `-O0` differential real, and it is a different thing from
`C2_FORCE` in `tools/c2_sweep.sh`.** The differential turns on *production*, and
production is `rir_prod_env = ast_replay_env && !rir_env` with `ast_replay_env =
optimize >= 1 || embed_jit || MCC_FORCE_REPLAY || MCC_RIR_FORCE`
(`src/mccast.c:1923-1925`). Measured on this build at `-O0` on
`tests/exec/programs/grep.c`: `[rir-prod-total] used=0` plain, `used=9 fallback=4`
with `MCC_FORCE_REPLAY=1` plus the 28 derived gates. So a plain `-O0` row ships the
parser's bytes in *both* legs and proves plumbing only — exactly what the Windows
`-O0` row turned out to be. With the force it is a real population: over the whole
660-file corpus, `arm64-osx` reads `used=2108 fallback=65 skip=88` and `x86_64-osx`
`used=2104 fallback=77 skip=56`. **Read no `-O0` differential that was taken without
it.**

Two corrections that fall out of this, both measured rather than reasoned:

- **`tools/c2_sweep.sh:40-42`'s stated reason for `C2_FORCE` is wrong.** It says *"a
  plain `-O0` run journals nothing at all and would read as a perfect board over an
  empty population."* It does not. Capture is armed by `rir_env || rir_prod_env`
  (`src/mccrir.c:535`), and the sweep runs under `MCC_REPLAY_IR=5`, so `rir_env`
  alone gives the byte board a **full** `-O0` population and `MCC_FORCE_REPLAY` is
  inert there — the 28 gates are the entire effect. Measured on `arm64-osx`, `all`
  corpus: plain `-O0` reads `c2ok=2389/2487`, a gap of **98** (`bytes=8 len=87`),
  against forced `-O0`'s **17** and `-O1`'s **20**. Forcing is still the right thing
  to do; the comment's reason for it is not, and a reader who trusts it will
  misread a `-O0` sweep row as empty when it is merely bad.
- **The same `rir_env` term, not `embed_jit`, is the simpler explanation of the
  Windows `-O0` observation** that *"capture and the verify replay run even at
  `-O0`"*. It reproduces here: `MCC_REPLAY_IR=1` at `-O0` fills the rir.log (14
  `faithful` on `grep.c`) on a build where `embed_jit` is not in play. Both
  statements can be true; `rir_env || rir_prod_env` is the one that always holds.

**The inert/live split on the two Mach-O keys — first measurement, and the gap is
100% inert.** This is the `MCC_RIR_PROD=2` reading the report list below asks for and
that nobody had taken on a Mach-O key. Aggregate over the 660-file corpus at `-O1`:

| key | used | fallback | skip | adopt rate |
| --- | --- | --- | --- | --- |
| arm64-osx | 2153 | 20 | 88 | 95.2% of 2261 bodies |
| x86_64-osx | 2164 | 17 | 56 | 96.7% of 2237 bodies |

**Per body it is unanimous: every C2-divergent body on both keys reads
`[rir-prod] fallback`.** Checked one at a time, not inferred from the totals —
`bounds_stress::test16`/`::test17`, `run_coherency::coherency_test`,
`run_s7_28::s7_28_wconv`, `run_s7_9::s7_9_iso646_test`, `ternary_op::tst_yarpgen`,
`fuzz/runner::triage`/`::interesting`/`::main`, `rev64_mt::main`,
`apple-libc/strpbrk::strpbrk`, and the six inside `full_language.c`
(`char_short_test`, `struct_assign_test`, `longlong_test`, `statement_expr_test`,
`s7_9_iso646_test`, `coherency_test`). **The live half is empty on both Mach-O
keys**, so their whole byte gap is fallback census and carries no correctness weight
— which is what the clean differential independently says, by a different route.

Note this contradicts nothing but does narrow `:399`'s class E: `fuzz/runner.c`'s
three bodies are recorded there as *live* (`used=49 fallback=0`) on the keys measured
on Linux. On both Mach-O keys the same three bodies fall back. Whether that is a
per-key pre-flight difference or corpus drift since that reading is not settled here.

**The Mach-O byte board, taken on a Mach-O host for the first time** (same build,
`C2_CORPUS=all`, 660 files):

| key | `-O1` = `-O2` = `-O3` | forced `-O0` | plain `-O0` | c2try |
| --- | --- | --- | --- | --- |
| arm64-osx | **20** (bytes 4 / len 13 / invalid 1 / err 2) | **20** (identical on every counter) | 98 (bytes 8 / len 87) | 2487 |
| x86_64-osx | **13** (bytes 3 / len 8 / err 2) | **16** (bytes 3 / len 11 / err 2) | — | 2472 |

Against this file's Linux-measured `all` board (`arm64-osx` 19, `x86_64-osx` 12) both
are **+1**, which is corpus growth and not a host difference — the same uniform +1
the `fn` banks took. `-O2` and `-O3` are byte-identical to `-O1` on every counter on
both keys, as everywhere else. `arm64-osx`'s forced `-O0` being identical to its
`-O1` on all four sub-counters is worth one re-measurement by the next reader before
it is trusted: `x86_64-osx` moves (8 → 11 `len`), so the equality is a property of
the key, not of the harness.

**Still not run on this host**: step 2 (`ctest` on a Mach-O host, which this file has
never had a reading of — including the `selfhost-fixpoint-O3` W3 question) and step 4
(`o0_ab.sh`). Those remain the larger deliverable; the differential above is the
cheaper follow-up, taken first only because it was what was asked for.

**Windows x86_64.**

1. Same two builds, then `ctest`. The preset sweep at the top of this file lists
   `matrix`, `diagnostics`, `msvc`, `sanitize-msvc`, `mingw`, `stage2`, `dist-mingw`
   and `dist-msvc` as unrun — `stage2` is W4 and is the one that needs a real 1MB-default
   PE process to confirm the 8MB `SizeOfStackReserve` fix at `src/objfmt/mccpe.c:738`.
2. ~~`tools/c2_equiv.sh bc2 all` once it has a **PE arm**~~ — **done and measured,
   2026-08-03 at `ad0dc1e0`, on the Windows x86_64 host** (`cmake-c2`: Debug,
   `-DMCC_ENABLE_CROSS=ON`, `-DCMAKE_C_FLAGS=-DMCC_REPLAY_IR_C2=1 -DRIR_DBG_OPTRACE=1`,
   winlibs gcc, rebuilt at that commit). **Eight cells, two keys x
   `-O0`/`-O1`/`-O2`/`-O3`, every one `differential: NONE`:**

   | key | passing goldens (arena/parser) | differential | uncompared (fail both legs) |
   | --- | --- | --- | --- |
   | x86_64-win32 | 293 / 293 (of 317, 24 skip) | NONE at all four `-O` | none — zero batch failures |
   | i386-win32 | 288 / 288 (of 317, 25 skip) | NONE at all four `-O` | `atomic_misc`, `errors_and_warnings`, `nodata_wanted`, `run_atexit` |

   So the two x86 PE keys' byte gaps are the **same benign class as the five Linux
   keys** — byte-divergent, behaviourally unobservable on the exec corpus.
   `arena-only` is empty everywhere. The four i386-win32 both-leg failures are
   identical mismatches in both legs (arch-specific expectation drift, not arena),
   cancel out of the differential, and were never compared — same caveat class as
   the eight both-leg goldens on Linux. Three findings the next reader needs:
   - **The prescription above was wrong about the flags.** The runner passes a
     *single* `-B`, so `-B runtime/win32 -B runtime` cannot be handed to it. What
     works: `x86_64-win32` is exactly the ctest-green native configuration
     (`mcc` + `-B <builddir>`); for `i386-win32` the script now *assembles* a
     synthetic `-B` dir — the archive sits at `<builddir>/i386-win32-libmccrt.a`,
     the crt objects in `<builddir>/lib-i386-win32/`, the `.def` import stubs only
     in `runtime/win32/lib/`, and headers in `<builddir>/include` — because the
     cross mcc's baked `MCC_CONFIG_MCCDIR` points at the *install* prefix
     (`dist/lib/mcc/win32`), which need not exist. All inside `tools/c2_equiv.sh`.
   - **Two Git-Bash traps handled in the script**: `MCC_TEST_OS=WIN32` must be set
     or the goldens' `expect_win32` variants are not selected; and env-var *values*
     are not MSYS-path-converted (argv is), so `MCC_REPLAY_IR_OUT` needs `cygpath -m`
     or the parser leg's `[rir-*]` diagnostics leak into the captured output and
     every golden mismatches for a harness reason.
   - **The `-O0` row is a vacuous control on this build, for a subtler reason than
     "no arena at -O0".** This mcc embeds the JIT, and `embed_jit` is a term in
     `ast_replay_env` (`src/mccast.c:1923`) — so capture *and* the verify replay run
     even at `-O0` (`rfaithful` fires, the rir.log fills). But production still does
     not adopt: `MCC_RIR_PROD=2` reads `used=0` at `-O0` against `used=1` at `-O1`
     on the same file, so both `-O0` legs ship the parser's bytes and the row proves
     plumbing only. This is the `embed_jit` partition check the JIT paragraph above
     said to run first — **the flip still partitions correctly at `-O1`+ on an
     embed-jit build; at `-O0` it partitions nothing.**
3. `arm64-win32`, `arm-win32` and `arm-wince` still need a **Windows-on-ARM** machine;
   they are W2 and W5 and remain byte-only until one exists. `arm-win32` and
   `arm-wince` must read identically on every counter — a differential where they
   disagree is a harness bug, not a codegen one.

**What to report back, in this order:**

- **The `ctest` result.** This is the actual deliverable — the first execution of the
  correctness oracle on those keys. A failing cell there is a real bug regardless of
  what any byte board says.
- **`MCC_RIR_PROD=2` totals per key**, i.e. `used=` versus `fallback=`. That splits
  each key's byte gap into the inert half (falls back, ships nothing) and the live
  half (arena bytes ship). Only the live half can matter for correctness, and nobody
  has ever measured that split on a Mach-O or ARM-PE key.
- The `c2_equiv` `differential:` line per key per `-O`, and any key printing
  `UNMEASURABLE` — the script refusing an empty population, which means host plumbing
  is wrong rather than the compiler. A non-empty `arena-only:[...]` is a genuine arena
  defect; that column is empty on all fifteen cells measured here.

### What replaces what — the corrected hierarchy

An earlier revision proposed `c2unproven == 0` as a replacement completion bar. It is
not a bar at all in its shipped form, and the hierarchy is simpler than that revision
made it:

1. **The goldens are the correctness bar.** 8255 cells, expected output checked at
   four `-O` levels, `-O0` corroborated against gcc and clang. Green. Nothing on this
   page outranks it, and no byte number can contradict it.
2. **`gap == 0` is the deletion-completeness bar.** It measures how many bodies still
   fall back to the parser rather than how many are wrong. Driving it to zero is what
   lets the fallback path be removed; leaving it non-zero costs coverage, not
   correctness.
3. **`tools/c2_equiv.sh` is a localiser.** When (1) goes red it says arena or
   optimizer in one run. It is not a bar and a clean run is not a completion claim.

**The byte board is not retired by any of this.** Five keys cannot execute here and
have no other instrument, and this file's own history holds wrong-code classes the
byte compare could not see (**The class that only Replay_IR can reach**) — so the
byte column stays. What changes is how a non-zero gap should be *read*: as a fallback
census first, and as a defect candidate only for bodies the pre-flight actually
accepts.

## Close the C2 gap

**The one rule this repo has paid for six times: fix at the USE site, never at the capture site.** A change where a primitive *consumes* an operand — the wrap `JOP_GENOP` performs, the `JOP_GV` wrap, the `JOP_VSTORE` and `JOP_STORE` arms, the argcast loop — is bound to the right node by the op's own recorded snapshot, and every such change has landed. A change where a cast or a lowering *happens* — a mark at `gen_cast`, a region bracketing `gen_negf` — perturbs the very stream `rir_verify` replays, and every such change failed. Read the banked negatives before designing another attempt.

Instruments, in the order they pay: `RIRDUMP=1` for ops either side of the blamed index with byte windows; `-DRIR_DBG_OPTRACE=1` plus `RIRDBG=<funcname>` for the `[ent]` entry stream (which prints each entry's `nocode` — that is how the dead-code class was found), `[stmt]` naming which entry pushed each BasicBlock statement, `[vst]`/`[gop]` for the vstore and genop admission inputs; `MCC_REPLAY_IR=6` for `[rir-dump]`/`[rir-diff]` node pairs against the tree. A structural `[rir-dump]` diff against the tree found nearly everything that closed.

`RIRC2TREE=1` drives C2 from the tree's arena instead. A body that diverges identically in both legs is a **shared replay** defect in `ast_replay_bb`/`ast_replay_value`, not a model defect; it still blocks the bar, but it is different work and it benefits both models. Caveat: `RIRC2TREE` falls back to the RIR arena when `ast_replay_ok(ast_cur)` is false, so re-confirm any individual body before attacking it as a replay defect.

### What closed, and the shape that worked

- **The inline-asm operand class.** `asm_instr` evaluates the operands, then `save_regs(0)`, then two `asm_gen_code` calls; only the last three were journalled, and `JOP_ASMGEN` replays off the parser's own vstack snapshot so its bytes were always right. The operand evaluation — pointer gv, deref, spills — was orphaned shadow-stack nodes nothing emitted. A `RIR_M_ASMOPS` mark taken immediately before `save_regs(0)` collects them into one `AST_OP_ASMOPS` statement whose replay calls `save_regs` itself. Closed `fancy_copy`, `fancy_copy2`, `sigaddset1`, `sigdelset1`, `memcpy1`, `memcpy2`, `mconstraint_test`, `other_constraints_test`.
- **The dead-code jump class.** `rir_op_effect` drops ops under `nocode_wanted`, but a break's arena `Jump` comes from a mark and a switch's dispatch jump comes from replaying the `AST_If` — neither is an op. `CODE_OFF_BIT` lives outside `RIR_NOEVAL_MASK`, so the mark filter let both through. Closed `optimize_out_test`.
- **A narrowing cast on an lvalue, at the `gv`.** `(int)zeros_c[1]` where `zeros_c` is `long` makes the parser's gv load 32 bits; the arena's untyped Load still named the long, so replay loaded 64, added 64 and truncated after. The gv's snapshot is where the narrowed type survives. Same wrap as the line below, widened to `VT_INT`.
- **`!cmp` left the arena's operator meaning the opposite of what the source wrote — wrong code, found by the fuzzer.** `if (!(a > a)) arr[5] = a;` dropped the store at `-O1`/`-O2`/`-O3` and `if (!(a == a))` executed one that must not run; `MCC_RIR_PROD=0` was correct, so this was live only because P4 flipped the default. `!` on a comparison emits nothing — the parser swaps `jtrue`/`jfalse` and flips `cmp_op` — and `RIR_M_CMPINV`'s `inflags` arm modelled that as `AST_FB_CMP_INVERT_LATE` **instead of** flipping the recorded token, which the cmp-invert commit chose deliberately: on arm64 a comparison materialises through CSINC, so the token flip replays `cset eq`+`cbnz` where the parser wrote `cset ne`+`cbz` — same length, same meaning, different bytes. Correct for emission, and it left the node saying `>` where the program says `<=`. `ast_ident_node`'s self-comparison fold reads that operator as a value, folded to 0, and `ast_sccp_scan` discarded the live arm. **The fix separates the two questions instead of choosing between them**: the operator always moves to the inverted one, and the flag now says only that replay must re-run the comparison the other way round and swap the flags, which `ast_replay_value` does by emitting `gen_op(bop ^ 1)` under the flag. Bytes are unchanged by construction — the same two acts in the same order — and every pass reading the operator sees the program's own sense. Both capture sites move together, `ast_hook_cmp_invert` and the mark. Closes seeds 240739, 240981 and 241595 of the nightly campaign at all four `-O` levels on x86_64; 257/257 `ast/*`, `ast-*` and `optfire/*` on arm64. `tests/fuzz/corpus/repro_cmp_invert_self.c` carries all three directions including the float case the flag was written for.
- **`gv` on a sub-int lvalue.** `gv` on a char or short lvalue emits the widening load itself, and both the placement (inside a ternary arm) and the signedness (`(unsigned char)` leaves no op) were missing from the arena. Wrapping the operand in a `Convert` at the `JOP_GV` site closed `mt_workload.c::main`, `rev64_mt.c::main` on x86_64 and `corpus.c::str_hash`.
- **A label's identity in the arena.** `rir_hook_label` and `rir_hook_goto` were handed the label's *token*, and two `__label__ l1` in different statement-expression scopes are the same token, so the arena's labels aliased where the parser's `label_find` scoping keeps them apart -- `ast_rp_label_floor` scopes only the inline-graft path. A monotonic id per label Sym, forgotten when `label_pop` frees it, fixes the identity; a bare Sym pointer does not, because `label_pop` frees at every scope exit and the allocator recycles. Two call sites needed reading rather than replacing: the goto arm reassigns `s` to a per-goto cleanup thunk before the hook runs, and `rir_hook_cleanup_thunk` records the same label from `g->cleanup_label->v`, which has to move to the id too or the thunk and the goto stop agreeing.
- **A discarded call's statement position.** A call whose value is discarded becomes an arena statement only when the shadow stack drops it, and inside a statement expression the parser's `vpop` lands AFTER the following goto's and label's marks. `rir_flush_effect_top` takes an unparented effectful `AST_Invoke` off the shadow stack at those two marks; it must BLANK the entry, because `rir_drop` re-`rir_stmt`s any effectful node it pops without checking for a parent and a node added to one BasicBlock twice reads as "nchild disagrees with sibling chain". Neither this nor the label id closes anything alone; together they closed `local_label_test`.
- **A compound assignment as a store-chain source.** `ast_finalize_chainstores` declined if either store was an `AST_OP_OPASSIGN`; only the second has to be a plain assignment. Shared-replay, so it moved both models. Closed `fuzz_next` and 39 divergences in `tests/fuzz/runner.c` on the x86_64 keys.
- The **code-free cast on a 32-bit target**, the same class **at a call argument**, and riscv64's **`gen_negf` spill** — all documented at length in earlier revisions of this file, all closed by widening the wrap a primitive already performs on its own operands.
- **The lost-intermediate class, width half: the 64-bit cast lowering on a 32-bit target.** `(long long)(int)(v >> 3)` with `v` a `unsigned long long` lowers, on every `MCC_PTR_SIZE == 4` key, entirely inside the `RIR_R_SYNTH` region `gen_cast` opens at `src/mccgen.c:4547` — the `(int)` is `lexpand(); vpop();` and the `(long long)` is `gv_dup(); vpushi(31); gen_op(TOK_SAR); lbuild()`. Neither leaves a conversion op, so the arena stored the raw 64-bit `Binary >>` and replay skipped the sign-extend-from-32. The fix is the **region's own value**: `rir_hook_castsynth_end(type, ds, ss)` closes that region with `rir_rend_to_val(RIR_R_SYNTH, type->t)` whenever the block actually did the 64-bit work (`ds == 8`, or `ss == 8 && ds >= 4`), and `rir_region`'s `RIR_R_SYNTH` rend wraps the shadow top in a `Convert` of that type. Closed `overflow_narrow.c::main` on all five 32-bit keys and `integer_promotion.c::main` on i386 — six divergences on both corpora, none anywhere else, `faithful` unmoved on all twelve keys.
- **The lost-intermediate class, const-fold half: a code-free float cast over an operand the parser already folded.** `(double)(3 + 4) / 2` folds to `3.5` in the parser with no op at all; the arena kept `Binary /` over an int `Binary +` and an int `Literal 2`. The `JOP_GENOP` operand wrap already had the right snapshot — it was skipping every float destination outright (`is_float(st)`). Narrowed that skip to "float destination **and** the operand is not a constant subtree the parser folded", which is exactly the code-free case: a non-constant int operand of a float op always leaves a `cvt_itof`, and a genuinely-float operand already carries a float type so the existing `rir_child_has_type` guard stops the wrap. **This closes P4's wrong-code defect 2** — `tests/exec/expressions/cast_operator.c` prints `avg: 7` under `MCC_RIR_PROD=1` where it printed `6`. It moves **no** C2 counter on either corpus, which is the point: the byte compare validates un-optimized emission and cannot see `ast_run_templates` folding `7 / 2` as an integer divide off a type the arena never had.

- **The op-assign vdup, second half: a dereferenced lvalue target, not just a member.** `*(unsigned *)(data + r) += a - b` (`ptr_longlong_arith32.c::main`) reached the arena as `Store(Load(Convert(Binary + data r)), Binary +(Ref data, ...))` where the tree has the target's own `Load(Convert(Binary + …))` in the RHS's left slot. `rir_reconcile_sv` had refilled the vdup slot from the SValue, which by then named `data`, so replay dereferenced `data` and the promoted register held 4. The `IR_OP_VPUSHV` `RIR_M_OPASSIGN` arm was already the right site — it just would not fire, because `rir_lvalue_shape` accepted only `AST_Unary MEMBER`/`MEMBER_ARROW`. It now also accepts an `AST_Load` over a pure address, and `rir_addr_pure` learned `AST_Binary` over `+ - * & | ^ << >> >>>` (no divide, which can trap). The bitfield/array/VLA exclusion the earlier half paid for moved to the top of the function so it covers both shapes. **Closes `ptr_longlong_arith32` in all 13 optimizer suites and `array_2d_iv` in `exec-ivsrptr`** (`g2[i][j] += i + j` is the same shape). Both `exec` and `all` C2 boards are byte-identical on all twelve keys — the class is invisible to the byte compare, exactly like the const-fold half.
- **The fconst reuse list, third act: a recorded constant whose replay never asks.** `gen_complex_cast`'s const path materialises the 16-byte complex into rodata and records it (`src/mccgen.c:6351`), but the arena captures the *result* as a `VT_CONST|VT_SYM` ref, so the promote re-emission pushes the ref and never re-runs the cast — the entry sits unconsumed at the head and every scalar reuse after it is off by one. On x86_64-win32, `check_arg(4.0, …)` then loads `im` from the first half of the complex literal (4.0 for 0.0) and `re` from the wrong slot entirely; checks 11–22 of `c11_complex_convert` all fail, promote-only. The RIR-side consumer survives the same stream because `rir_hook_fconst_reuse` resyncs **by position**; the ast-side list is sequential and cannot. The fix tags each entry with the site that recorded it — scalar `gv` (`:2437`) or complex cast — and the sequential consumer skips a kind mismatch at the head: record order is emission order, so a mismatched head entry is one whose consumer already passed or will never ask. The discriminator is exact: complex is `VT_STRUCT` + `ref->a.is_complex` and never `is_float`, so the two sites cannot see each other's constants.
- **A dead-code float constant desynchronised the whole reuse list.** `ast_fconst_record` appended `vtop->sym->c` unconditionally, but a constant materialised under `NODATA_WANTED` gets `size = 0`, so `put_extern_sym` assigns it nothing and `c` is recorded as **0**. `ast_fconst_reuse` treats a returned 0 as "no reuse", so such an entry can never be consumed — it only shifts every later index by one. In `flt_eval_method.c::main` the parser records 30 entries of which 4 are these zeros; the replay makes 26 calls and never asks for the dead ones, so from index 23 on every reuse is off by two and `ast_fconst_push_ref` stamps the *caller's* type onto the *previous* constant's symbol. The visible damage was a `movd` (4 bytes) against the 8-byte `+inf` double, i.e. `isnanf(0.0f)`, and `ok` went to 0. `ast_fconst_record` now skips `c == 0`. This is a **shared-replay** defect, not a model defect: it fixes the recorder too, and `ast-verify-ratchet-{O1,O2,O3}` went 136 gaps to 135 with `features_c99_c11/flt_eval_method.c main unfaithful` becoming faithful — `tests/ast/verify-baseline/x86_64-linux.txt` re-banked. **Closes `flt_eval_method` in `exec-replay-promote`.**

### The class that only Replay_IR can reach

**A body the recorder declines is a body whose shared-replay defects have never been executed.** `flt_eval_method.c::main` was `[ast-verify] unfaithful` on the tree, so the parser's bytes were restored and `AST_PF_EMIT` never ran — the `ast_fconst[]` desync was there the whole time and cost nothing. Replay_IR is `rfaithful` on the same body, so promotion runs, the arena is re-emitted, and the latent bug becomes wrong code. Expect more of these as Replay_IR's faithful population widens past the recorder's: the tell is that the failing gate is a *pass* gate (`-fpromote-locals` here) and that the same body reads `unfaithful`/`desync` in `tests/ast/verify-baseline/`. Check that file before blaming the arena. Two consequences worth acting on: the C2 byte compare can never see this class, and the fix belongs in the shared replay where it pays twice.

**Two verify baselines carry an unmeasured prediction.** `tests/ast/verify-baseline/x86_64-darwin.txt` and `x86_64-win32.txt` both list `features_c99_c11/flt_eval_method.c main unfaithful`, the same verdict `x86_64-linux.txt` carried. The `c == 0` skip is target- and host-independent, so both should now read faithful and both need re-banking on their own hosts — they are left alone here because a cross build's gap set does not reproduce a host-native one (measured: `mcc-x86_64-win32` against the mingw-hosted `x86_64-win32.txt` drifts on dozens of unrelated bodies, because the header resolution differs). `arm64-darwin.txt` lists the body as `desync`, a different verdict, and is expected to be unmoved.

### Banked negatives — do not re-pay for these

- **Marking code-free `gen_cast` calls**, three variants: at every such call wrapping the shadow-stack top → `c2ok 531/1146`, `c2invalid=517`, two core dumps; narrowed to a const-only `AST_Binary` outside every region → `c2err=3`, `faithful` down one; bound to the operand the marker's own vstack snapshot names → `faithful` holds but `c2err` 0 to 1-3 on every key and neither target body moves. Stamping the bound node's type instead of wrapping is worse again.
- **Bracketing `gen_negf` as `RIR_R_FNEG`** on the `RIR_R_INC` pattern **aborts the compiler**. Region markers are part of the captured stream.
- **Relaxing the `!lv` guard in `JOP_STORE` generally**: -30 `c2ok` on every key. The float restriction is what makes it safe.
- **Dropping the argcast wrap's `AST_Convert` skip** without the wider test **segfaults** every PE key.
- **Widening `rir_child_width_differs` itself** to the effective width: same `c2ok`, minus 14 to 20 `arenahasheq` on every key.
- **Retyping the cast chain at its conversion op** for `overflow_narrow.c::main`, two variants, both no-op. The `(int)` narrow in `(long long)(int)(v>>3)` emits no op, so there is no op to attach to. The conversion-op route is dead for the whole lost-intermediate class; **the route that worked was the region's own value**, not a field on an op — see the two lost-intermediate entries under "What closed". A region's `rval` is invisible to `rir_verify`, because `rir_run` skips every entry whose `tag != RIR_T_OP`, so putting new data on a region marker that already exists is a USE-site change and not a capture-site one. Adding a *new* region still is not; that negative stands below.
- **Closing the `RIR_R_SYNTH` cast region at its `rbegin` instead of its `rend`**: -14 bodies on every 32-bit key (`narrow_ranged.c` ×10, `popcount_inline.c` ×2, `signbit_inline.c`, `bitfields.c`, `overflow_inline.c`, `struct_packed_indirect.c`), for +5 — i386 gap 2 to 16. At the `rbegin` the recorded vstack still reads the 64-bit *source*, so the `Convert` is correct but premature: `rir_stamp_sv`'s widen loop then sees a node typed 4 bytes against a snapshot that says 8 and wraps it in a *second* `Convert` back to the source width, and replay materialises a high half the parser never emitted (`b9 00 00 00 00` after the call in `narrow_ranged.c::udiv`). `faithful` held in both variants, which is the tell that the failure is reconstruction and not capture. Any future region-value wrap belongs at the end of the region, where the snapshot has already moved.
- **Cloning the op-assign target at the vdup for a bitfield lvalue**: -2 `c2ok` on x86_64 (`bitfields.c::main`, `bitfields_ms.c::main`), a `c2len` divergence both times. The `IR_OP_VPUSHV` clone that fixed `tal_realloc_impl` has to exclude `VT_BITFIELD` (and `VT_ARRAY`/`VT_VLA`) targets and impure address subtrees; with that exclusion the twelve-key board is unmoved on both corpora. **Re-measured while closing `bitfields_ms`, and the cost is still exactly 2** — x86_64 `exec` 1113/1113 → 1111/1113. The byte windows say why, and they say the clone is the wrong shape rather than a near miss. The parser evaluates the bitfield lvalue's address **once** (`mov -0x10(%rbp),%rax`), lets the vdup'd copy spill to a temp inside `gv`'s `save_regs`, and reloads that temp for the read-modify-write; with the clone the trial re-materialises the address into a second register (`mov -0x10(%rbp),%rcx`) and evaluates the *value* first, so the whole sequence is transposed at identical length. Modelling this needs the parser's own spill placement, not a subtree copy. The runtime defect the exclusion left behind is closed by refusing the shape in `rir_prod_take` instead — production-only, so it cannot cost a `c2ok` at all.
- **Dropping every mark recorded with any `nocode` bit set**: -25 bodies. A label, case or default recorded with `CODE_OFF` set is usually the very thing that turns code back on. Narrowing that to `RIR_M_JUMP` only is still -13, because `gjmp_acs` sets `CODE_OFF` on the way out and so every break looks unreachable by the time the hook runs; the hook has to be handed `nocode_wanted` as it stood BEFORE the `gjmp`.
- **Touching `error1`'s opening `mcc_exit_state(s1)`** (`src/libmcc.c:651`), either variant, **deadlocks the JIT self-host**. It is the release half of the `mcc_enter_state` that `MCC_SET_STATE` (`src/mcc.h:2128`) wraps every `mcc_warning`/`mcc_error` in, and `mcc_compile_sem` is a binary semaphore: deleting it strands the take, and adding a matching `mcc_enter_state(s1)` before each of the three returns doubles it. The regime, not the diagnostic, is what was wrong — see P4 defect 3.
- **Widening `ast_sccp_has_label` itself to count `case`/`default` markers**, rather than adding a second predicate for the one site that deletes a subtree. Six call sites, and only `ast_sccp_scan`'s dead arm needs it: `ast_cprop_switch_meet` is handed the switch node whose body always holds case markers, so the widened predicate reads true there on every switch in the tree and disables switch cprop outright; `ast_cprop_stmt`'s loop guard, `ast_licm_at_loop`, `ast_ltemp_run` and `ast_ivsr_run` would each go conservative on any loop containing a `case`. `ast_jt`'s identical-arm merge is the one other site with the same hazard and does not need it: two arms both carrying the same `case` value, or both carrying `default`, is not legal C, so `ast_ident_same` can never hold over a real one.
- **Reading `AST_FB_NOCODE` on every statement kind** rather than only `AST_If`: -25 bodies. Other kinds pack raw values into `fbits` — `ASMGEN` its vstack window, `MEMBER` its `VT_NONLVAL` — and alias any new flag bit. Any future `AST_FB_` read in `ast_replay_bb`'s statement loop must be kind-guarded for the same reason.

### Still open, largest first

Counts are divergences over the twelve keys on the `all` corpus. The per-body
diagnoses below marked *"re-measured at `bc85ce70`"* were taken on x86_64 at forced
`-O0` with `cmake-verify` rebuilt at that commit; the older counts beside them are
from `da3a461b` and have not been re-taken per body.

**`riscv64-promote-docker` is not idempotent and will fail your second `ctest`.** The
cell passes on a clean directory and then leaves `corpus/{a,b}.c` owned by **root**
(the container writes as root; the cleanup runs as you), so the *next* run dies with
`rm: cannot remove ...: Permission denied` before compiling anything. It is not a
codegen failure and not caused by whatever you just changed. Clear it with
`sudo rm -rf <builddir>/riscv64-promote-docker` and it passes again. Fixing it
properly means the cell chowning or removing its workdir from inside the container.

**Tree state at `bd9026df`+1, so that the next person is not surprised by a
dirty tree**: the `c2equiv`/`c2unproven` seam in `src/mccrir.c` and its column in
`tools/c2_sweep.sh`; this file; and a staged `git rm -r` of `b/`, a 126 MB build
directory committed by accident in `bc85ce70` — `.gitignore` carries only
`/cmake-*/`, so **`b/` and `bc2/` are both still unignored** and the next `git add -A`
re-adds them. Not yet run for the seam: the full `ctest` (only the 237 `ast`/`optfire`
cells, all passing). Nothing to rebank — the seam moved no `BANKGAP`, and the
twelve-key byte board is identical on every pre-existing counter either side of it.

- `bounds_stress.c::test16` and `::test17`, **24**, both on all twelve keys: `strcpy(q = alloca(strlen(demo) + 1), demo)` inside a call argument. The trial re-evaluates `strlen(demo) + 1` and calls `alloca` a second time instead of reusing the value. Nested call in an argument, so the same family as `struct_assign_test`. **INERT — both read `[rir-prod] fallback`**, so the double-`alloca` never ships; an earlier revision called it *"likely not equivalent"*, which was true of the re-emission and irrelevant to the binary. Largest model gap on the board, still worth closing for deletion completeness, but not a wrong-code item.
- `fuzz/runner.c::triage`, `::interesting`, `::main` — **LIVE, and the only live class on this list. `[rir-prod-total] used=49 fallback=0`**: the pre-flight accepts all 49 bodies, so the arena's bytes — including the extra dereference — actually ship at `-O1`+. This is the one entry where correctness rests on the arena and the parser being equivalent rather than on the fallback, which makes it the highest-priority item here even though its byte count is not the largest. The extra `mov (%rcx),%rcx` loads the same pointer the parser loads later, so it is very probably equivalent, and the goldens are green — but "probably" is doing work no other entry on this list needs. **The scope on this line was also wrong; see URGENT item 5.** Open at `-O1` on **arm64, riscv64, arm and i386** and at forced `-O0` on **x86_64**; the store-chain fix closed x86_64 at `-O1` only. **Root-caused at `bc85ce70`, x86_64 `-O0`:** all three insert the *identical* three bytes, `48 8b 09` (`mov (%rcx),%rcx`) — the same "extra dereference the parser did not make" seen as `ldr x0,[x0]` on arm64-osx, so it is one defect on every key. The source shape is `GATES[g].env` (a member at offset 8 of an array element) passed as a call argument. **The parser defers the load to argument-marshalling time**: it computes *all* argument addresses first, then loads each at push time inside `gfunc_call`, in reverse order — `add $0x8,%rcx` … `mov (%rdx),%rdx; push %rdx; mov (%rcx),%rcx; push %rcx`. Replay emits the `AST_Load` eagerly at expression position instead. `AST_Load` replays as `ast_replay_value(child); indir();` (`src/mccast.c:4319-4322`) and `indir()` emits a load **only when its operand arrives `VT_LVAL`**, so the address subtree is reaching replay as an lvalue where the parser had a plain rvalue. **The arena builds one `Load` too many, and the earlier entries here were taken in
the wrong configuration.** `runner.c` is **`c2ok` at `-O1` on x86_64** — the
store-chain fix closed it there — and diverges only at **forced `-O0`** on that key
(and at `-O1` on arm64/riscv64/arm/i386). Two prior revisions of this entry dumped the
arena and the optrace at `-O1` on x86_64, i.e. of the **passing** body, and drew
conclusions from it. Everything they concluded about `MEMBER(Load(...))` being the
defect shape was describing the *correct* shape. Re-take any measurement here with
`MCC_FORCE_REPLAY=1` + the 28 gates at `-O0`, or on a cross key.

At the configuration that actually diverges, `[rir-dump]` reads:

```
Load                          <- EXTRA; absent from the passing -O1 shape
  Convert t=5
    Unary op#262145           <- AST_OP_MEMBER
      Load
        Binary +
          Ref GATES
          Ref index
```

against the neighbouring `OPTS[i]` argument, which is the plain `Convert(Load(Binary +))`.

**The mechanism follows directly.** `AST_Load` replays as `ast_replay_value(child);
indir();` (`src/mccast.c:4319-4322`), and `indir()` emits a load only when its operand
already carries `VT_LVAL`. `AST_OP_MEMBER`'s arm sets exactly that at
`src/mccast.c:4241` (`vtop->r |= VT_LVAL | fbits`). So the outer `Load` finds an
lvalue, calls `gv(MCC_RC_INT)`, and emits the 3-byte `mov (%rcx),%rcx`. The parser's
corresponding `indir()` ran on a computed address that was *not* yet an lvalue and
emitted nothing, deferring the load to `gfunc_call`'s push.

**Both earlier suspects are dead and stay dead** — `gen_cast` no-ops on same type
(`if (sbt != dbt)`, `src/mccgen.c:4378`), and every `Convert` in the body reads
`fb=0`, so `AST_FB_CONVERT_GV` never fires. The `Convert` is not involved.

**What is not yet decided is the fix site**, and it is a genuine three-way choice:
whether the arena should not build the outer `Load` at all (construction, i.e. the
`RIR_M_LOAD` mark firing once more at `-O0` than the shape needs); whether
`AST_OP_MEMBER` should not set `VT_LVAL` when its result feeds a `Load`; or whether
the `Load` replay should skip `indir()` on an already-lvalue operand. The first is a
capture-site change and the banked negatives are unanimous about those. **Measure any
of them on the twelve-key board before and after, and remember this class is LIVE.**

The older framing of the open question, kept because it is still the second half:
**which arena node makes the address lvalue** — and that decides between the argcast use site (`src/mccrir.c:2435`, legal) and the `RIR_M_LOAD` mark (`src/mccrir.c:3073`, a capture site, the category that produced two core dumps and a compiler abort in the banked negatives). `[arg]` reports the two operands as `cur=Load … curt=0 st=5`, an **untyped** Load — the case `:413`'s banked negative N13 says the argcast wrap was special-cased for, and widening that wrap cost −16 `c2ok` on every key. **Two synthetic reducers were written and neither reproduces** (`ok=1` both times), which is banked negative N20 exactly: work from the real body, not a model of it. Instrument `runner.c::interesting` directly with `RIRDBG` and read the arena node for that argument.
- `statement_expr_test`, **7**, is what is left of the statement-expression class after `local_label_test` closed. It is now a `c2bytes` divergence at identical length -- pure ordering, not a missing or extra instruction. The two defects that made up the class are described under "what closed"; whatever is left is a third.
- `full_language.c`, **42** over its seven keys, six bodies: `struct_assign_test` (call), `statement_expr_test` (jmp), `s7_9_iso646_test` (store, and a BYTE divergence not a length one), `local_label_test` (call), `coherency_test` (genop), `char_short_test` (cvt_csti). Of these, `struct_assign_test` and `char_short_test` are confirmed RIR-model defects (the tree leg gets them right or differs); the rest reproduce identically in the tree leg and are shared-replay work. `longlong_test` adds 5 more on the keys where it diverges.
- `rev64_mt.c::main`, **12**, still open on all keys — a different and much larger divergence from the ternary one that closed in the same file's sibling. Re-measured at `bc85ce70`, x86_64 forced `-O0`: want **2044** got **2006**. The divergence is `add $0x18,%rdx` in the parser against `add $0x28,%rdx` in the trial — **a different member offset** — followed by `xor %eax,%eax; mov %rax,(%rdx); mov %rax,(%rcx)`. Source is the chained `jb[i].n = jb[i].xsum = jb[i].asum = jb[i].psum = 0;`. **INERT — `[rir-prod] fallback`**, so those bytes never ship and the "is it equivalent" question an earlier revision raised is moot for correctness. It is a model gap: the arena cannot re-emit this shape, so the body cannot stop falling back.
- `coherency_test` — **INERT. `[rir-prod] fallback`, so the parser's bytes ship and
  nothing below reaches a binary.** An earlier revision called this *"likely a real
  bug"* on the strength of the byte evidence alone; that was wrong, and it is the
  reason this file now opens with the inert/live split. Checked directly: mcc prints
  `coh compound-literal-sum: 41` at `-O0`, `-O1`, `-O2` and `-O3`, byte-identical to
  gcc. The finding below is a **model** gap — a body the arena cannot re-emit and
  therefore cannot stop falling back — not a defect. Its priority is deletion
  completeness, not correctness. The re-emission is missing content: Re-measured at `bc85ce70`, x86_64 forced `-O0`: want **930** got **806**, and the trial's last bytes are `48 8b 65 e8` (`mov -0x18(%rbp),%rsp`, the VLA stack restore) immediately after the previous `call`. The final statement — `printf("coh compound-literal-sum: %d\n", (int)(vla[1] + (int[]){10, 20, 30}[2] + COH_C))` — is absent from the re-emission entirely. **Capture is complete**: the `[rir-op]` stream carries the three `vstore`s that materialise the compound literal (ops 472/479/486, parser offsets advancing `909→920→931→942`), the `genop` chain at 493-498 and the `call` at 501. So the loss is in the **arena reconstruction in `rir_build`**, not in `ast_replay_bb`/`ast_replay_value` — compensating in the replay would be patching downstream of where the content goes missing. `:401` classifies this body as shared-replay on the strength of `70c7d6f7`; that measurement is from the deleted tree leg and **the arena-truncation evidence above points the other way**. Note also that compound literals have **no** RIR prior art anywhere in this file, and every place a VLA or array shape has met the arena the landed decision was to *exclude* it, never to model it — so there is no banked negative to steer by here, in either direction.
- `ternary_op.c::tst_yarpgen`, **7**: a `JOP_STORE` register spill the parser makes mid-expression and the trial does not. Scheduling and allocation, not model shape — a single yarpgen expression with enough live values to spill, and the trial reaching that point with fewer live registers is the symptom. Expect it last.
- `s7_9_iso646_test`, **12** including `run_s7_9.c`: a pure ORDERING difference at identical length. The parser stores then loads the next condition; the trial hoists the load before the store and picks a different register for it. Same family as `AST_FB_STORE_ADDR_LATE`.
- `zero_bss.c::main` **5**, `smoke.c::main` **5**, `struct_ret_variadic.c::mkv` **3**, `varargs.c::mix` **3**, `strpbrk.c::strpbrk` **2**, `struct_packed_indirect.c::main` **1**, `integer_promotion.c::main` **1** (riscv64 only), `run_s7_28.c::s7_28_wconv` **1**, `run_s_annFGK.c::s_annFGK_annex_test` **1**. `overflow_narrow.c::main` is closed.
- The **lost-intermediate** class is closed for every body it was blamed for. `overflow_narrow.c::main` and i386's `integer_promotion.c::main` closed with the `RIR_R_SYNTH` region value; `cast_operator.c::main` closed with the genop float wrap; both are written up under "What closed". `grep.c::pmatch` does not diverge on any key at HEAD and should not be carried forward as a member. What is left of `integer_promotion.c::main` is riscv64 only and is the `VT_QLONG` representation item, not the width class — the `(unsigned)` widen on riscv64 takes the `trunc = 32` leg with no synth region to hang a value on, and widening the operand wrap for it is not the fix.
- riscv64's 7 `c2skip` and 2 `c2invalid` on the `all` corpus, and its one remaining compiler abort (down from four; the other three were the `asm_parse_regvar` numbering bug).
- **The lost-intermediate class over a pointer**, found by the JIT seam and costing **zero** C2 counters, which is why it had never been seen. `for_each_elem` (`src/mcc.h:1662`) compares `elem` against `(type *)(sec->data + sec->data_offset)`; the cast from `unsigned char *` is code-free, the arena keeps the `unsigned char *`, and replay compares two pointers of different pointee type. Bytes are identical — both operands are the same width and the bodies are `c2ok` — so the only observable is that `combine_types` emits a "pointer type mismatch in comparison" warning the parser did not. It is benign *today* only because nothing downstream reads the pointee; a `+`/`-` or a `[]` on such an operand would scale by the wrong size. The same shape as the two closed lost-intermediate halves, and the same rule applies: the route is a region's own value or an existing primitive's operand wrap, never a mark at the `gen_cast`.

## Raise arena fidelity

- Raise arena fidelity. **The `arenahasheq` counter no longer exists** — P4 retired it; the last reading was 1680 of 2521 on arm64-osx and the Scoreboard's column is frozen at that sweep. Re-instrument before quoting a number, and prefer C2, which scores against the parser. Measure as `arenahasheq` and as diffs-per-body falling, not as first-divergence classes disappearing — fixing divergence #1 only exposes #2. Classify by **structural** dump diff, not by node index: `ast_intention_hash` is a structural walk that hashes neither `type_ref` nor `ival` on a `Ref`, so the large Convert-versus-Ref index-transposition class and every `ref`-only diff are not fidelity defects and cost nothing to ignore. Live classes: field-only, extra-Convert-over-Binary, missing-Convert-over-Literal, tree-extra-Store. The last two are understood and not cheap — the tree builds cast chains where one push record recovers only one link, and `tree-extra-Store` is dead code under `nocode_wanted` that the tree records and the op filter drops.
- The extra-Convert-over-Binary class is **measured, not guessed**, and the measurement says do not touch it. Gating the whole argcast wrap on `RIR_M_ARGCAST`'s per-argument fired flag reads **+25 arenahasheq and -16 c2ok on every key**: the wraps the tree does not make are load-bearing for emission. The flag is therefore consulted only for the untyped-Load case it was added for. Any future attempt to close the class has to explain that trade first.
- Land explicit operand binding as a side-table keyed by op index, not a node field and not positional binding.
- Widen C3 pass equivalence past the field-identical population — the same work as raising arena fidelity, since the paired population *is* the field-identical population. Add the optimized-versus-optimized byte tap by replicating C2's whole save/restore prologue just before the `orig`/`orig_rel` frees in `ast_func_end`, where `ind` is the end of the optimized body: a different point with different live state. Report the coverage difference separately rather than as a mismatch, because the tree runs passes only where `ast_replay_ok` holds while Replay_IR accepts every body.

## Finish the capture path

- Phase F: un-embed `RirOp` from `JrnOp`. This is a design change, not a lift — Replay_IR must own an op stream with its own vstack buffer (the shape `rir_mvs`/`mvs_off`/`mvs_n` already has for marks) instead of reading `jrn_ops[]` and indexing `jrn_vs[]`. `jrn_snap_vstack` falls out of the same change: it has no `mccgen.c` call site to hang on, so it cannot be lifted independently.
- Discharge F against the same obligation every prior slice met: byte-identical `MCC_REPLAY_IR=5` sweep log and identical object sha256 at `-O0`, `-O1`, `-O2`, `-O3` and forced `-O0`. That compares capture against parser, tree and journal at once and is strictly stronger than cross-checking two capture paths against each other. Replace site by site; do not run both paths side by side.

## The TinyCC upstream sync

`tinycc/mob` is fetched as the `tinycc` remote. The merge base is `a338258d` (2026-06-13); `main` descends from tcc linearly and has never carried a merge commit from it.

**A plain `git merge tinycc/mob` is unusable and always will be.** Every file is renamed and relocated (`tccgen.c` → `src/mccgen.c`, `tccelf.c` → `src/objfmt/mccelf.c`, `x86_64-gen.c` → `src/arch/x86_64/x86_64-gen.c`) and every identifier with them, so git's rename detection finds nothing: the trial merge produced **31 modify/delete conflicts** and would have resurrected `tccgen.c` alongside `src/mccgen.c`. Sync by porting semantics per commit, never by merging.

**Synced at `2be0218b` (2026-07-30), 17 commits.** What they were and what happened to each:

| upstream | disposition |
| --- | --- |
| `85ba3ae8` `'n'` operand modifier not negating | **ported.** moderncc had the identical bug — `val = -val` computed then `(int)sv->c.i` printed — in all four sites (`i386-asm.c`, `arm-asm.c`, `riscv64-asm.c` ×2, the last also needing `val == 0` for `'z'`) |
| `2be0218b` arm64 addend on LLP64 | **ported.** `arm64_sym`'s `unsigned long addend` → `addr_t`; `unsigned long` is 32-bit on LLP64 so large addends truncated |
| `31020bf9` quoted `LIBRARY` in `.def` | **ported.** New `get_libname`, heap `dllname`, `.dll` default extension |
| `384614a9` + `43158eaa` ELF boundary symbols | **ported.** `linker_sym` bit on `struct sym_attr`, `set_linker_sym`/`provide_linker_sym`/`update_linker_sym`/`finalize_linker_symbols`, called after `layout_sections` so the final section order decides `_etext`/`_edata`/`_end`. Verified against gcc: all six names agree, and a user definition of `end` correctly wins (PROVIDE semantics) |
| `5673055a` `b895aa7e` `04206d29` `97a6d8c7` `38059770` TLS | **already present, and moderncc is ahead.** See below |
| `a667b53f` `ff85981b` `d9d02c56` `add111e6` win32 self-host with tcc 0.9.27 | not applicable — moderncc does not bootstrap from tcc 0.9.27 |
| `4ed845d0` Makefile doc targets, `.github/workflows` | not applicable — CMake, and a different CI |
| `da58264a` tests2 fixtures | moderncc has its own test tree; the boundary-symbol behaviour is covered by the new `exec/programs/linker_symbols.c` golden, which self-gates so ELF, PE and Mach-O all print the same thing |
| `7f7845cd` review some recent changes | one applicable hunk, deliberately **not** taken — see below |

**The TLS group was already reconciled, item by item, and mostly by moderncc arriving first.** Checked individually rather than assumed: riscv64 carries the addend on *both* halves of the TPREL pair, byte-identical to upstream's final form; `mcc_elf_end_file` already keeps undefined TLS symbols typed `STT_TLS`; i386 local-exec already emits `fc` rather than `0` (different encoding, same effect); PE TLS already exists on i386 and arm64 (`gen_pe_tls_base`, `pe_tls_index_sym`), which is what `38059770` adds upstream. x86_64 float and `long double` TLS were verified behaviourally against gcc. And moderncc is **ahead** on two counts upstream has no equivalent for: riscv64 general-dynamic TLS (`riscv64_tls_gd_a0`) and arm64 dynamic TLS in a shared object (`9f76b124`).

What is left of the TLS commits is *refactor* — "remove stupid copy&paste code", "integrate tls into gen_modrm()", "remove stupid loops" — with no behavioural delta. Upstream moved TLS **into** `gen_modrm`; moderncc handles it at the `load`/`store` sites. Porting that reshaping into backends carrying Replay_IR capture hooks would perturb the captured stream, move objects, invalidate the byte-identity gates, and buy nothing. Not done, and it should not be done later either.

**One real upstream fix was deliberately declined.** `7f7845cd` adds `vpop(), vpushi(0)` to `gen_cast`'s `dbt_bt == VT_VOID` arm, "do not confuse backends with VT_VOID in registers". That is a capture-site change in the single most banked-dangerous function in this tree, and moderncc shows no symptom: a void-cast battery (`(void)(a+b)`, `(void)g()`, `(void)dg()`, `(void)(a?g():b)`, `(void)(void)a`, comma-expression) matches gcc at `-O0`/`-O1`/`-O2`/`-O3` and compiles clean on eleven of the twelve keys (`arm-wince` was not run; it shares a define set with `arm-win32` and must read identically). **Re-measure before taking it**; if a symptom ever appears, the fix belongs at the use site.

**A pre-existing gap this sync found, not caused by it**: a static i386 link of a TLS program cannot resolve `R_386_TLS_GOTIE`. Filed under **Open work raised after the cutover**.

*Gate as met: `ctest -j 8` **8254 of 8254**, zero failures; twelve-key object A/B against the pre-port build **19,557 of 19,557 byte-identical, 0 differ** — the port touches only link-time and asm-operand paths, so no codegen moved; `tools/o0_ab.sh` re-banked because the corpus gained one file, and the rebank is **+12 lines, -0** across the twelve object banks, i.e. exactly one new sha256 per key with every pre-existing hash unchanged; `tracegate`/`schemagate`/`targetgate` clean; four side configurations green.*

## Keep the measurement honest

- Bank the corpus census against header resolution, never against a remembered number. `MCC_CONFIG_AUTO_MCCDIR` resolves mcc's own freestanding headers from **argv[0]'s directory**, so a compiler built into a scratch dir with no sibling `include/` silently loses `stdarg.h`/`stdbool.h`/`stdatomic.h`/`threads.h`; a build omitting `-DMCC_CONFIG_MCCDIR` finds no system headers at all and reads 476; a glibc sysroot resolves `<threads.h>` to glibc's rather than mcc's shim and costs 25 functions. Every one of these leaves `rc`, the file count and the failing-file list unchanged — the census is the only thing that moves, which is what makes it a trap.
- **Done** (`tools:`): `c2_sweep.sh` no longer selects `$BUILD/mcc` for a key the host is not. The native compiler carries no sysroot flags, but which key it IS depends on the host — x86_64 on a Linux host, arm64-osx on a mac — so asking for key `x86_64` on a mac silently measured `arm64-osx` with `rc` 0 and a plausible row. It now prefers an explicit `mcc-<key>` whenever one was built.
- The `all` corpus compiles 519–571 of 657 files per key. The rest are `dg-error` cases, host-specific `darwin/`, arch-specific `arch/` and files that need a driver; they are excluded by the `rc` check, not by a list, so a file that starts compiling joins the census automatically. `full_language.c` needs `-I <repo root> -DCC_NAME=CC_gcc` and enters only on the 7 keys where the C2 probe's own error does not abort the compile — `extra=` on the row says whether it did.
- Keep the C2 harness mirroring the tree's replay prologue exactly across `vstack`/`vtop`, `loc`, `anon_sym`, `ast_pinned_regs`, `ast_rp_bsym`, `ast_rp_csym`, `ast_rp_switch`, `ast_temp_frontier`, `ast_rp_nlabel`, `ast_fconst_i`, `ast_locrec_i`, `sym_free_first`, `ast_rp_asmops`. Leftover allocator state reads as a codegen difference; one omission, a dirty vstack, costs 194 bodies.
- Keep the `-O0` cells honest. `ast_replay_env` needs `optimize >= 1`, so `-O0` without `FORCE` journals nothing and the cell exits 1 on "0 bodies journalled" rather than 77. **`FORCE` is now two explicit env vars, `MCC_RIR_FORCE=1 MCC_AST_INT128=1`**, the first read at `src/mccast.c:2035`, and `SRCDIR` is gone from `rir_parity.cmake` and from all twelve callers; the old form derived the 38 `o4 || optimize >= 1` gate names by regex over `${SRCDIR}/*.c` and would hard-fail the moment one of those gates was deleted. Forced `-O0` legitimately reads 3 bodies fewer than `-O1` on x86_64, arm, arm64, arm64-osx, i386 and riscv64, 2 fewer on x86_64-osx, and 0 fewer on the five PE keys — `grep.c::tolower`, `c11_threads.c::thrd_equal`, `arm64.c::putchar`, all `static inline` shims whose out-of-line copy `-O1` emits and `-O0` does not. That is an emit difference, not a census loss. `tools/c2_sweep.sh`'s own `C2_FORCE=1` still derives the 38 gate names by regex over `src/*.c` and refuses to run if it finds none; that is now the only derivation left in the tree, and it should follow `rir_parity.cmake` to `MCC_RIR_FORCE` rather than be the last thing coupled to gate spellings the recorder's deletion will change.
- **`MCC_AST_INT128=1` is in that env for a measured reason: without it forced `-O0` loses 2 bodies, on x86_64 and x86_64-osx only.** Measured either side of the cut on the whole twelve-key matrix: x86_64 `1146 → 1144` against an `-O1` of 1149, x86_64-osx `1140 → 1138` against 1142, and every other key identical either way (arm64 1186 both ways; the five PE keys unmoved at their `-O1` figure). Both bodies are `tests/exec/types/int128.c`, whose two out-of-line `__int128` bodies exist at `-O0` only when that gate is on — the derived list forced it and `MCC_RIR_FORCE` alone does not. Bisected to that one gate of the 38: `MCC_AST_INT128=1` alone takes that file from `fn=4` to `fn=6`, `-freemit-templates` moves nothing. Forced `-O0` is the coverage baseline every deletion phase is diffed against, and its worth is that it covers the population `-O1` covers minus only the three shims above; narrowing it by 2 on the two keys where C2 already reads 100% would remove coverage exactly where a P1/P4 regression would be most legible. The gate name is a coupling, but a cheap and self-healing one — when the gate dies with the recorder those bodies are captured unconditionally and the env entry becomes a no-op, so **delete the line then, and do not re-derive a gate list either way**.
- The C2 gap is ratcheted per key by `tests/ast/rir_c2.cmake` (`ast/rir-c2-{O1,O2,O3}` and eleven `ast/rir-c2-<triple>`), a banked gap and not a 100% bar. Each cell banks `BANKGAP` (`c2try - c2ok`), `BANKSKIP` and `BANKFN`; the `BANKFN` floor is the honesty half, since a gap ratchet over an unpinned population passes by measuring fewer bodies — with `vendor/` absent the ELF keys read `ok=77 fn=476` and a gap of 1, which is the header-resolution trap above wearing a green tick. The cell SKIPs unless the probe reports `c2try>0`, so a stock build without `-DMCC_REPLAY_IR_C2=1` cannot pass it vacuously, and it counts only files that exited 0. **The cells ratchet the `exec` corpus, not `all`** — deliberately, because a ctest cell has to be seconds and one twelve-key `all` sweep is twelve minutes. `all` is the bar and the sweep measures it; `exec` is the regression tripwire and ctest measures it. Do not conflate the two numbers. Banks as re-measured by rerunning each cell after the lost-intermediate class closed: x86_64 `0/1149`, x86_64-osx `0/1142`, x86_64-win32 `0/1237`, i386 `0/1152`, arm64 `1/1199`, arm64-osx `1/1224`, arm64-win32 `1/1297`, i386-win32 `1/1257`, arm `1/1130`, arm-win32 and arm-wince `1/1235`, riscv64 `3/1156` plus 2 `c2skip` — ten over the twelve keys, down from sixteen. Rebank by rerunning the cell, never by editing the number to match a memory. A gap *below* the bank only prints a STATUS line, so a closed body sits unratcheted until someone rebanks; treat that line as a to-do, not as noise.
- **A new hook call compiles on the host and still breaks every stage2 CI job.** The amalgamated, optimizer-on build hides two whole classes of missing declaration. `mccast.h` declares its hooks behind `MCC_CONFIG_OPTIMIZER && defined(MCC_INTERNAL)`, which is why every `ast_hook_*` call in `mccgen.c` sits inside `#if MCC_CONFIG_OPTIMIZER` — the moment mcc compiles mcc without `-DMCC_CONFIG_OPTIMIZER=1` (selfhost-jit, cross-factory, o4-aot-jit) an unguarded call is an implicit declaration, which mcc treats as a hard error. And with `MCC_SINGLE_SOURCE=OFF` each `src/*.c` is its own translation unit, so a file that only saw `mccrir.h` through `mccgen.c` no longer does. Before pushing a new hook call, build all four: `-DMCC_SINGLE_SOURCE=OFF`, `-DMCC_CONFIG_OPTIMIZER=OFF`, `-DMCC_REPLAY_IR=OFF`, `-DMCC_CONFIG_ASM=OFF`. Each takes about a minute and all four together would have caught both defects. **P3 moved 813 lines into a new translation unit (`src/mccircap.c`) and all four were verified green on it, in isolation** — a shared build directory silently carries each `-D` into the next configure, so use a distinct one per configuration or the matrix tests the union rather than the cases.
- Do not run the twelve-key sweep and `ctest -j8` at the same time. Seven `selfhost-fixpoint` cells fail under that contention and pass individually — a false red that costs a bisect.
- A **native x86_64** reading, taken in an emulated `linux/amd64` container, is NOT the cross x86_64 row: `tests/exec` reads `c2ok=1114/1115` with one byte divergence where the cross build reads a clean `1120/1120`. The populations differ — 269 files against 265, 1151 bodies against 1156 — because a native build resolves the container's glibc headers and the cross build resolves the Gentoo stage3 sysroot, which is exactly the census trap the first item in this section describes. Do not read the difference as a codegen difference until the file set is pinned. The same run left a `qemu-x86_64` core dump on one `tests/exec/statements` file; that is emulation noise, not an mcc crash — the native arm64 container compiles the same file fine.
- Extend the native-versus-cross cross-check to riscv64 and i386. arm64 has it, re-measured at HEAD in a `linux/arm64` container against `tests/exec`: native reads `c2ok=1179/1180`, cross reads `1169/1170` — the same gap of 1 over a population 10 bodies larger, because a native build resolves glibc's headers directly instead of through the stage3 sysroot. On the `all` corpus the two are **not** comparable and must not be put in one table: the native build compiles 556 files against the cross build's 543, `full_language.c` among them, so the native row carries divergences the cross row never had a chance to see. Pin the file set before comparing anything wider than `exec`. Neither i386 nor riscv64 can run natively on this host; both need silicon or a full-system emulator, not `qemu-user`.
- **Ten of twelve keys are confirmed host-invariant off macOS** — a second host stack reproduces the board. On a Windows x86_64 host, mcc built with the vendored winlibs gcc and `-DMCC_REPLAY_IR_C2=1` reproduced all five PE-key rows byte-exact, and the `arm-win32` ≡ `arm-wince` identity gate held natively. A WSL Ubuntu clone with the five ELF cross compilers and the four Gentoo stage3 glibc sysroots reproduced x86_64/i386/arm/arm64/riscv64 exactly on the same host. This is host-invariance, which is weaker than and distinct from the on-target-native check the line above wants.
- The PE `tgmath.h` shadowing is **not real** — settled on the macOS host, do not reorder any header lookup.
- Keep the fifteen files the repo-wide comment strip deliberately skipped, since in each the comment is the test payload and not prose. The ten `tests/diagnostics/dg-error/*.c` carry the expected diagnostic in a `/* dg-error: ... */` marker that `run_dgerror.cmake` greps out and hard-fails on when it is absent; `tests/exec/preprocessor/comment.c` is the comment-lexing permutation corpus; `tests/cst/kinds/comment.c` and `tests/cst/hashinv/spaced.c` exercise CST comment promotion and the H_s/H_t comment-invariance split; `tests/preprocess/asm/gas_comments.S` is the GAS comment corpus; and `tests/exec/programs/grep.c` is greped as its own input by the `{SELF}` golden, which expects the trailing vim modeline in the output. The strip also preserved line numbering in `tests/**` C sources because `tests/exec/goldens.h` pins diagnostics as `file.c:line:`.
- Keep `src/mccrir.c` free of any `MCC_TRACE(` call; `tools/tracegate.c` checks a file only if it contains one, so the first call pulls that file's ~250 unannotated branches into the checked set at once. Run `./cmake-c2all/tracegate src` and `./cmake-c2all/schemagate src` before every push.

## The imaginary-literal fix, held on a branch

`fix-imaginary` (`a3c51e8d`) closes the imaginary-literal defect this file has carried as open: the lexer took `i`/`I`/`j`/`J` only *after* the float suffix, so `1.0iF` — exactly what glibc's `complex.h` writes — did not lex; and the order that did lex did not fold, so `double _Complex z = 1.0i;` at file scope reported "initializer element is not constant". Both halves are fixed, folding in `gen_imaginary_complex` so the parser and the recorder stay on one path, and it drags out a latent bug in `mk_complex_type`, which maps every non-float base to the `long double _Complex` cache slot — an integer imaginary literal took whichever complex type happened to be cached there. Twenty suffix spellings, file and block scope, runtime values byte-identical to gcc.

**It is not on `main`, and the reason is worth more than the fix.** The fold makes complex constants rodata references, and the AST recorder cannot model that: `ast/replay-complex_imag` stops replaying `mk` at all, and the recorder's fidelity ratchet gains three gaps — `c11_imaginary_suffix.c::local_scope`, `::main`, and `feature_macros.c::main`, all `desync` or `unfaithful`.

**Replay_IR is `rfaithful` on every one of them, before and after, and the fold takes `mk` from 20 ops to 6.** So this is not a regression in the compiler; it is a front-end improvement that the recorder alone cannot follow, measured on the one body where the two models can be compared directly.

Landing it then would have meant re-banking a fidelity ratchet to accept a recorder regression, on a subsystem P5 deletes outright.

**Re-measured after P4 flipped the default, and the cost has already shrunk from four cells to three.** `ast/replay-complex_imag` now **passes**: it failed only because the recorder was the producer, and Replay_IR was `rfaithful` on `mk` all along. Merged onto the flipped `main` the branch costs exactly the three `ast-verify-ratchet-{O1,O2,O3}` cells and nothing else — the other 22 failures in that run are the long-checkout-path `bound_global` artifact, not the fix. Those three cells measure the **recorder's** fidelity and P5 deletes them outright, so **land `fix-imaginary` the moment P5 lands**; it merges clean and needs no rebase.

Two further findings from it, neither a complex defect, both filed here so they are not re-diagnosed as one. mcc advertises `__GNUC__ 4` / `__GNUC_MINOR__ 2`; glibc gates `CMPLX` on `__GNUC_PREREQ (4, 7)`, so `CMPLX(5.0, 6.0)` parses as a call to an implicitly-declared function and reports "initializer element is not constant" for an entirely different reason. And `bits/floatn.h` leaves `__HAVE_FLOAT128 0` against an unconditional `__HAVE_FLOAT64X 1`, which `tgmath.h` `#error`s on. One version-advertisement question underlies both. With the fix, the complex family against **glibc's own** `complex.h` goes from 2/8 files to 6/8 on the five ELF keys, and those two are the remainder.

## Open work raised after the cutover

- **Use `AST_Poison` for dead arms instead of deleting them.** The `dead_code.c::main` defect was `ast_sccp_scan` folding `if (const)` by *discarding* the dead arm, guarded by an enumeration — `ast_sccp_has_label` counted only `AST_Jump op == 4` (a label definition) and missed `op == 2` (case) and `op == 3` (default), so the fold deleted case entries whose bodies were live. The fix added a second memoized predicate beside it, which closes the bug but leaves the shape: **a deletion guarded by an enumeration of everything that must not be deleted, which is only ever as complete as its last audit.** Poisoning the subtree instead keeps the node, so every existing scan — labels, cases, defaults, and whatever is added next — still sees it, and correctness stops depending on the guard being exhaustive. `AST_Poison` already exists, five passes already produce it (`ast_narrow_make`, `ast_dse_kill`, `ast_sccp`, `ast_jt_run`, `ast_bf_drop`) and `rir_op_effect` emits it, so the vocabulary is there. Before doing it, confirm two things: that replay emits nothing for a poisoned subtree, and that every pass treats `AST_Poison` as opaque rather than walking into it. If both hold, the two `has_label`/`has_case` predicates and their negative can go with the change.
- **`array_in_struct_init.c` is `ast_cycle_env`, not an `-O3` anomaly — but the item is now UNMEASURABLE AS WRITTEN.** The recipe is a prod-on/prod-off A/B, and **production can no longer be turned off**: `rir_prod_env = ast_replay_env && !rir_env` (`src/mccrir.c:521`) has no gate term, and `MCC_RIR_PROD` now only sets `rir_prod_gate`, the verbosity level (`src/mccrir.c:5082`). The original reading was: on arm, prod-on/prod-off agree at `-O2` and differ at `-O3`; `MCC_AST_CYCLE=0` at `-O3` makes them agree. What is still measurable is the cycle gate alone, and **at `da3a461b` it moves nothing** — `MCC_AST_CYCLE=1` versus `=0` on that file at `-O2` and at `-O3` on arm is byte-identical at both levels. So either the convergence defect closed with something else, or it needs a different instrument than the one recorded. `ast_cycle_env` is `optimize >= 3` (`src/mccast.c:1942`, not `:2060`) and re-runs the strategy cycle to a fixpoint. **Re-derive an instrument before re-opening this**; do not special-case the file.
- **Instrument a test for the lost-intermediate over a pointer.** `for_each_elem` (`src/mcc.h:1662`) expands to `elem < (type *)(sec->data + sec->data_offset)`; the `(ElfW_Rel *)` on an `unsigned char *` emits no op, so the arena keeps `unsigned char *` and replay compares mismatched pointer types. It costs **zero C2 counters**, which is exactly why it went unseen for so long, and it is benign only because nothing downstream reads the pointee — a `+`, `-` or `[]` on such an operand would scale by the wrong size. There is no test for it. Write one that *does* scale: a code-free pointer cast whose result is then indexed or offset, so the wrong pointee size becomes a wrong address rather than a wrong type. It should fail today.
- **Finish real glibc header support.** mcc advertises `__GNUC__ 4` / `__GNUC_MINOR__ 2`. glibc gates `CMPLX` on `__GNUC_PREREQ (4, 7)`, so `CMPLX(5.0, 6.0)` parses as a call to an implicitly-declared function; and `bits/floatn.h` leaves `__HAVE_FLOAT128 0` against an unconditional `__HAVE_FLOAT64X 1`, which `tgmath.h:66` `#error`s on. With the imaginary-literal fix the complex family goes from 2/8 to 6/8 files against glibc's own headers on the five ELF keys, and those two are the remainder. One version-advertisement question underlies both — decide what mcc should claim and what it must then implement, rather than raising the number and finding out. The sweep's `-I runtime/include`-first ordering is a *preference* once this lands, not a requirement.
- **`MCC_TRACE` `full_language.c` to isolate its remaining defects — and it is now worth 90 divergences, not a footnote.** It is the second front and the only file that reaches classes `tests/exec` does not. **Re-measured at `da3a461b`, the premises of this item have changed and the item got bigger:** it compiles clean at all four `-O` levels on all twelve keys (48/48 rc=0), enters the C2 census on **ten** keys rather than seven, and contributes **90 of the 194-gap `all` board** — 7 to 9 divergences per key, 16 on riscv64. The clause *"fails to parse under `MCC_REPLAY_IR=1` with an armed recorder"* is dead; there is no recorder. What replaces it is sharper: on **x86_64 and i386** the file still fails outright under `MCC_REPLAY_IR=5` with `tests/diff/parts/legacy_meta.h:376: error: ';' expected (got '0')`, which is the `AST_OP_ASM` re-assembly defect (P4 defect 4) reproducing verbatim in the verify path. **Close that first** — it is one named defect standing between two keys and their coverage — then trace the rest. Build with `-DMCC_CONFIG_TRACE=ON` and use `tools/tracediff.sh` (which still exists, despite the deletion note saying otherwise) to diff a passing key's trace against a failing one.
- **Audit the rest of the arena for fields that do not mean what they say.** The `!cmp` defect was not that a pass was wrong; it was that the arena carried an operator whose sense lived in a *separate flag*, so a pass reading the operator alone was reading a half-truth. `AST_FB_LANDOR_INVERT` is the same shape and is still live — `ast_replay_value` applies it, and nothing else consults it. The general rule the two suggest: **a flag may say how replay reaches a value; it may never be the only place the value's meaning is written.** `AST_FB_CMP_INVERT_LATE` now obeys that. Walk the remaining `AST_FB_*` bits and the `ast_op` conventions and decide, one at a time, which side of the line each is on. The cheap standing check is the runtime A/B (`-O1` with and without `MCC_RIR_PROD=1`, stdout and exit code) — it is what the fuzzer effectively ran.
- **`selfhost-fixpoint-O3` is green again, by side effect, and the defect underneath it was never found.** The gate failed on every macOS stage2 cell from the P5 merge; it passes at `c850da2c` and after, in both the Release and the local-ci configurations. **Bisected**: `b4dd48a7` (docs only) fails, `c850da2c` — `feat(gen): __attribute__((vector_size))` — passes. Nothing in that commit touches the inliner, promotion or the narrow pass, so treat this as **latent, not fixed**.

  What the failure was, all measured, in case it returns:
  - **The probe.** `void be32(unsigned char *p, unsigned v) { p[0]=v>>24, p[1]=v>>16, p[2]=v>>8, p[3]=v; }` at `-O3`. `ldrb w1, [x29, …]` is the agreeing answer; the miscompiled compiler emitted `ldr`. Semantically a no-op — the following `strb` discards the difference — which is why only the fixpoint gate could see it.
  - **One TU.** A per-file `-O2`/`-O3` mix over the 22 multi-TU sources, delta-debugged to **`mccgen.c` alone at `-O3`**. It `#include`s `mccast.c` at line 15054, so that TU holds the narrow pass; the result is self-consistent.
  - **Two gates of fourteen.** With `mccgen.c` at `-O3`, `-fno-inline` **or** `-fno-promote-locals` made it agree; `CYCLE`, `TEMPLATES`, `COLOR`, `PRE`, `IVSR`, `LICM_TEMP`, `NARROW`, `CSE_JOIN`, `CPROP_JOIN`, `SETHI`, `REASSOC`, `DIVMAGIC`, `RANGE` and `SCCP_FIX` did not. Inliner **crossed with** register promotion, and `INLINE` is `optimize >= 3` — which is the whole of the `-O3`-ness.
  - **One graft.** `MCC_AST_INLINE_LIMIT` binary-searched to **#55** (54 agreed, 55 diverged): `ast_vlat_context_at` grafted into `ast_narrow_elim_srcrange`, at its first `return ast_vlat_context_at(a, c, out);`.
  - **The symptom.** Printing the decision from `ast_narrow_elim_fits`: the limit-54 compiler read `srcrange=1 ctx={lo=0 hi=4294967295 tt=51 st=1}`, the limit-55 one read `srcrange=0`. **The grafted call returned 0 where it had to return 1**, so `ctx` was never written and the caller read an uninitialised `AstVLat` — which is exactly why a clang-built and an mcc-built compiler disagreed at all, and why the value it settled on was build-dependent.

  **Why "latent" and not "closed".** The graft itself still happens at HEAD — instrumenting the two graft sites shows `ast_vlat_context_at into ast_narrow_elim_srcrange` at #58 and #64, the numbering having shifted because `c850da2c` added bodies ahead of it — and an `MCC_AST_INLINE_LIMIT` sweep over 0..79 (68 grafts total) finds no prefix that reproduces the probe. So the miscompile either went away for a reason nobody wrote down, or it survives and no longer changes *this* body's outcome. **Re-expose it by bisecting `MCC_AST_INLINE_LIMIT` against a fresh probe, not by re-deriving the gate list.**

  **Where to look if it returns.** `ast_inline_graft` calls `save_regs(0)` immediately before `ast_replay_bb(e->ast, …)` (mccast.c ~2754) — the same shape as the banked *"a promoted store target loses its assignment"* defect below, whose negative applies here too: **do not fix it by making `save_reg_upstack` skip pinned registers.** `ast_vlat_context_at` returns 0 at exactly three points, and `el` comes from a struct-returning call, so the hidden return slot under the graft's frame `bias` is the first thing to check. **Synthetic reductions do not reproduce**: four attempts — out-pointer callee, struct-returning inner call, a non-inlinable inner call, added frame pressure — agreed at every `-O` level. Work from two real binaries, not from a model of them.

- **mcc cannot self-host on Windows arm64.** Every `stage2` cell on `windows-arm64-msvc` and `windows-arm64-mingw` dies the same way: the stage1 mcc, itself built by the host `cl`/`gcc`, takes an access violation (`code=3221225477`) compiling `lib/atomic.c`, `lib/alloca.S`, `lib/alloca-bt.S` and `lib/builtin.c` — the runtime library, four objects, before anything of the compiler proper is reached. Not a target-codegen defect: a cross build from macOS produces `arm64-win32-libmccrt.a` from those same sources without complaint, so the crash is in mcc **running** on Windows arm64, not in what it emits for it. That leaves the host ABI — varargs, `alloca`, the stack probe — as the place to look, and it needs a Windows arm64 machine to look on. `windows-arm64-mingw` carries `experimental` and does not red the workflow; `windows-arm64-msvc` does not and does.
- **A static i386 link of a TLS program cannot resolve `R_386_TLS_GOTIE`.** `gotplt_entry_type` has no case for reloc **16**, so `build_got_entries` (`src/objfmt/mccelf.c:1288`, erroring at `:1306`) reports *"Unknown relocation type for got: 16"* — **510 times** for one small `__thread` test — and produces no binary. **Only `-static` is affected**: the same source, same compiler, links and runs clean dynamically, and x86_64 is unaffected either way. Found while porting the TinyCC TLS commits and **it is not a regression** — a build predating that work fails identically, and upstream's commits do not fix it, so this is moderncc's own gap and not something a future sync will bring in. The reloc is glibc's *"GOT entry for static TLS block"*, which is exactly the case a static link has to handle. Repro: `mcc-i386 --sysroot=vendor/gentoo-stage3-i386-glibc -static` over any file with a `__thread` variable.
- **The `-O3` graft path lost its admission predicate and nothing replaced it.** Before P5, `ast_fn_inlinable` and `ast_reemit_retain` both opened with `... || ast_bail || ast_desync` — the recorder's per-body decline verdict. The deletion removed those flags, so the two predicates now read `if (!ast_inline_env && !ast_inline_pass_env)` (`src/mccast.c:2449`) and `if (!ast_inline_env || ast_reemit_poison || ast_reemit_n >= AST_INLINE_MAX)` (`:2828`): the forward inliner is gated on the *pass* being on and on nothing about the *body*. That widening is real and measured — it is the one file the deletion moved, `tests/exec/statements/scopes.c` at `-O3` on all twelve keys — and it was verified correct on that body. The open question is whether it is correct in general or merely correct so far. **Decide deliberately**: either the graft path needs a Replay_IR-owned predicate of its own (the arena has `rir_prod_take`'s pre-flight and `ast_replay_ok` to build one from), or it genuinely needs none and that should be written down with the argument. Do not leave it decided by deletion. The instrument is the runtime A/B at `-O3`, not the byte compare, since a graft that changes a pass's input is invisible to it.
- **The one upstream fix the sync declined, and the condition for taking it.** `7f7845cd` adds `vpop(), vpushi(0)` to `gen_cast`'s `dbt_bt == VT_VOID` arm — *"do not confuse backends with VT_VOID in registers"*. It was not taken: it is a **capture-site** change in the function this file's first rule names, and moderncc shows no symptom. What was tried, so it is not re-tried blindly: a void-cast battery — `(void)(a+b)`, `(void)g()`, `(void)dg()` (float), `(void)(a?g():b)`, `(void)(long long)a`, `(void)(void)a`, and a comma expression — matches gcc at `-O0`/`-O1`/`-O2`/`-O3` and compiles clean on eleven of the twelve keys. **Take it only against a reproduction**, and if one appears, prefer the use site: the banked negatives for `gen_cast` are unanimous.
- **Three of the nightly campaign's seven repros are not miscompiles and never were.** Seeds 240466, 240914 and 241631 are syntactically invalid C — a stray `}` whose `for (...) {` is gone — which is why `triage` could attribute none of them and why the campaign's "2 new classes" over-counts. The reducer bug that produced them is fixed (`reduce` restored the whole chunk on a rejected step, reviving lines an *earlier accepted* step had removed, so `keep[]` stopped describing any candidate that had been tested), and `reduce` now refuses to bank a final candidate that does not reproduce. Delete those three from any list of open defects; the real content of that campaign is the `!cmp` class, which seeds 240739, 240981 and 241595 all reduce to.

## Cut to Replay_IR

### The finding that sets the order

**The recorder's tree is the production optimizer at `-O1` and above, not a side-car.** `ast_replay_env = optimize >= 1 || embed_jit || MCC_FORCE_REPLAY` (`src/mccast.c:2035`) arms the whole recorder through `ast_try_active` (`src/mccast.c:16016`); `ast_replay_body(ast_cur)` then **overwrites** the parser's bytes at `src/mccast.c:18186`, and the parser's copy is restored only on `!faithful` (`src/mccast.c:18804`). The passes mutate `ast_cur` in place (`ast_run_strat_cycle`, `src/mccast.c:18306`) and `AST_PF_EMIT` re-emits it (`src/mccast.c:18420`). `ast_cur` is also the only source of JIT blobs and retain pools — six call sites, `src/mccast.c:18045, 18486, 18526, 18572, 18910, 18912-18913`.

So "delete `mccast.c`" would delete the optimizer and the JIT. The correct statement of the work is narrower and harder: **the recorder is a redundant second producer of the same arena, and Replay_IR takes its place.** The arena, the replay engine, the ~10,800 lines of passes and the slice/JIT infrastructure all survive; what dies is the 1,716-line hook recorder (`src/mccast.c:2311-4026`, 143 definitions, 126 call sites in `src/mccgen.c`, 78 declarations in `src/mccast.h`), the journal's verify half (~550 of `src/mccast.c:14495-15971`), and the measurement scaffolding that exists only to compare the two producers.

**`-O0` is the A/B precisely because the recorder is absent there.** At `-O0` no arena is allocated and the parser's bytes ship untouched, so every phase below must leave `-O0` objects byte-identical — a cut that moves an `-O0` byte has touched something it had no business touching. `MCC_FORCE_REPLAY` at `-O0` arms Replay_IR over every body independently of the recorder, which is what makes it a coverage proof rather than a comparison.

### Decisions taken

- **Cutover: per-body fallback.** Production uses `rir_arena` when that body's C2 check passes and keeps the parser's bytes when it does not. This is not new machinery — the `-O1` path already is *emit from the arena → compare against the parser → restore on mismatch*, and per-body fallback is that same shape with `rir_arena` substituted for `ast_cur`. It converts the remaining 18-body C2 gap from a precondition for deleting 4,500 lines into an optimization gap on 18 bodies per key. Close them because they are worth closing, not because the deletion waits on them.
- **Scope: refactor the optimizer and the JIT onto Replay_IR**, rather than leaving them mechanically re-producered. The pass drivers, the retain pools, the `-O3` forward-inline re-emit and the JIT blob producers each get rewired to the Replay_IR arena as a first-class source.
- **Renames: full split and `ast_*` → `ir_*`.** `src/mccast.c` splits into arena+replay, passes, and slice/search units, and the prefix goes with it. ~~The `ir_`/`IR_` namespace is verified empty across `src/`, `tools/` and `include/`.~~ **Superseded — P3's rebrand filled it; 835 occurrences under `src/` at `da3a461b`. See P6.** Size, re-counted at `da3a461b`: **12,163 `ast_`/`AST_` tokens over 1,231 distinct identifiers**, across `mccast.c` (9,065), `mccrir.c` (1,255), `tools/asttool.c` (629), `ast_eval_slice.h` (224), `mccjit_embed.c` (196), `mccast.h` (117), `mccforecast.h` (104), `mccgen.c` (65), `mccgate.h` (60), `mccjit_intent.c` (36) and the arch backends (11).

**The split must extend the include chain, not create link units.** `mccast.c` and `mccrir.c` are `#include`d back to back into one translation unit (`src/libmcc.c:11-12`; `src/mccgen.c:15229-15235`), and `mccrir.c` reads `mccast.c`'s file statics — `jrn_ops`, `jrn_vs`, `ast_cur`, `ast_base_depth`, `ast_body_ind_sv`. The ordering is load-bearing for the journal's macro trick as well: `src/mccgen.c:143-234` `#define`s 61 codegen primitives to `jrn_` twins and `src/mccgen.c:15155-15234` `#undef`s all 61 *before* the includes, which is the only reason `JRN_W0(load, …)` can define `jrn_load` whose body calls `(load)(…)`.

### The gate at each phase

1. `-O0` objects byte-identical across the corpus, before and after.
2. `MCC_RIR_FORCE=1` at `-O0`: `faithful + empty == fn` on all twelve keys.
3. `C2_NO_EXTRA=1 tools/c2_sweep.sh` counters unmoved — for a re-homing or a rename, *unmoved by one* is the whole proof. Only P4 is allowed to move them.

### P0 — harness first, nothing deleted

- **Done**: `rir_parity.cmake`'s derived forced-`-O0` gate list is now an explicit `MCC_RIR_FORCE=1 MCC_AST_INT128=1`, the first read at `src/mccast.c:2035` beside `MCC_FORCE_REPLAY`, which still works and is still the name the `-O0` A/B in this plan is written against. `SRCDIR` is gone from the script and from all twelve cells. The board is unchanged from the derived form on every key; `MCC_AST_INT128` is the one gate of the 38 that was worth bodies, and **Keep the measurement honest** records why it is set explicitly and when to delete it.
- **Done**: `tests/ast/rir_c2.cmake` is a banked-gap ratchet over `MCC_REPLAY_IR=5`, wired as `ast/rir-c2-{O1,O2,O3}` plus eleven cross cells, banked at the 18-body gap and SKIPping a build without `-DMCC_REPLAY_IR_C2=1`. Numbers and the population floor are in **Keep the measurement honest**. The 100% bar belongs to the bodies, not to the deletion.
- **Done**: `tools/o0_ab.sh` is the `-O0` A/B itself — per-key object sha256 over the corpus plus the forced-`-O0` coverage board, banked under `tests/ast/o0-baseline/` and re-checked with `O0_AB_CHECK=1`. It refuses to measure a key whose sysroot is absent, because the first run without one read 76-79 objects of 276 on four keys and still printed a green board on every row.
- Add the `-O0` object-identity cell. `tests/ast/journal_inert.cmake` already supports `-DRIR=1` (`:21-26`) and is the right driver.

### P1 — free cuts — **Done**

Nothing here was in the `-O1` path and nothing here was read by Replay_IR. Landed as `p1-free-cuts`, **-859 lines of `src/mccast.c`**, with the twelve-key C2 board and the `-O0` corpus byte-identical on both sides — measured, not inherited.

- The journal's verify/report half is gone: `JRN_FIX_LIST`/`JFIX_*`, `jrn_sv_eq`, `jrn_prefix_eq`, `jrn_classify`, `jrn_deep_reg_live`, `jrn_run`, `jrn_emit_line`, `jrn_blame`, `jrn_site`, `jrn_verify`, `jrn_configure`, the whole `jrn_oracle_*` family, `jrn_report`, the `jrn_tot_*`/`jrn_fixhist`/`jrn_ophist`/`jrn_fixpair`/`jrn_regdiff_n` counters, `jrn_fix_name`, and the now-dead `#else` inert stubs. `MCC_JOURNAL`, `MCC_JOURNAL_OUT` and `MCC_JOURNAL_ORACLE` no longer exist as environment gates; `MCC_JOURNAL_HOOKS` does, and still names the capture substrate.
- The record substrate is untouched. `jrn_issue` is verbatim minus its one write into the verify half (`jrn_regdiff_n++`, which took `int got` with it); `jrn_reset` lost only its two verdict lines. `jrn_gap`, `jrn_bad`, `jrn_active`, `jrn_depth`, `jrn_snap_vstack`, `jrn_begin`/`jrn_end`, `jrn_new_op`, `jrn_raw_add`, `jrn_op_name`, `jrn_replaying`, `jrn_fconst_take`/`jrn_fconst_note`, `jrn_pred`, `JrnOp`, `JRN_OP_LIST` and every `JRN_W*`/`JRN_R*`/`JRN_WSV` wrapper stay. `struct JrnSub` and the sub-op table died with `jrn_oracle_ops`, which collapsed `jrn_begin`/`jrn_end`'s nesting branches to a plain depth counter.
- The gate in `ast_func_begin` is now `if (rir_env && ast_try_active)`; capture (`jrn_active = 1/0`) still drives Replay_IR. `jrn_started` is gone with `jrn_verify`, and the `jrn_oracle_want` disjunct at the verdict site reduced to `if (ast_verify_env)` — which made `ast_declined` and `ast_replay_tried` dead locals, so they went too.
- `ast_hook_stmt` (an empty switch) and `ast_hook_data` are deleted with their two `mccgen.c` call sites and their `mccast.h` declarations. `ast_hook_data` took its exclusively-dead diagnostic family with it — `ast_data_zero_check`, `ast_data_estimate`, `ast_data_roundtrips`, `ast_data_reemit`, `ast_data_reemit_selftest`, `AstDataRec`/`ast_data_recs`, the three `ast_data_pipe_*` buffers, the five `ast_data_*` counters and the `MCC_AST_DATA_REPORT`/`MCC_AST_DATA_REEMIT` gates. The real `.data`→`.bss` and string-merge transforms are the parser's, at `src/mccgen.c:14383/14401/14418`, and are untouched; so are `ast_data_all_zero` and `ast_strpool_find_or_add`, which they call.
- Tests, build and tools: `ast-journal-parity-{O1,O2,O3}`, the 30 `tests/ast/journal-baseline/` files, `verify_ratchet.cmake`'s entire JOURNAL mode, `tools/jrn_sweep.sh`, the 130 `journal-sweep-*`/`journal-native-*`/`journal-regen-*` targets and `journal_sweep.cmake`/`journal_native.cmake`/`journal_report.cmake` are all gone. `verify_ratchet.cmake` keeps only the recorder's `[ast-verify]` ratchet, which dies in P5.
- `tests/ast/journal_inert.cmake` survives with one mode instead of three. `ast/journal-inert` (the `MCC_JOURNAL=1` runtime cell) is deleted — `ast/rir-inert` already covers that ground — and the driver's gate is now unconditionally `MCC_REPLAY_IR=1`/`[rir-verify]`. `mcc_nojrn` and `ast/journal-inert-build` are kept and re-pointed onto that gate: proving the capture substrate is byte-inert still matters for Replay_IR, and it is the same proof.
- The `raw=`/`rawb=` breadth bucket was **retired, not ported**. That discharges the twelve outstanding `.depth.txt` rebanks: the files are deleted rather than regenerated in twelve environments.

### P2 — decouple Replay_IR from recorder state

Replay_IR reads five recorder-owned globals; all must move before the recorder can die.

**Done (state ownership).** `src/mccrir.c` now owns `rir_base_depth`, `rir_body_ind_sv` and `rir_reloc0_sv`, all three sampled in `rir_hook_body_begin`, and no longer names `ast_base_depth`, `ast_body_ind_sv`, `ast_reloc0_sv` or `ast_try_active`. `src/mccast.c` was not touched; its four globals are still live for the recorder and for the journal's verify half, and they die with those. Twelve-key C2 board and the `-O0` corpus objects are byte-for-byte unmoved; the 50 `ast/rir-parity-*` cells pass.

What the transfer turned out to rest on:

- **The sample point is exact, not approximate.** `rir_hook_body_begin()` (`src/mccgen.c:14588`) and `ast_func_begin()` (`:14589`) are adjacent statements after `gfunc_prolog`/`mcc_debug_prolog_epilog`/`func_vla_arg`, so `ind`, `cur_text_section->reloc->data_offset` and `(int)(vtop - vstack + 1)` are identical at both. `rir_hook_body_begin` has exactly one call site.
- **The recorder assigned `ast_base_depth` only inside `if (ast_try_active)`; Replay_IR assigns unconditionally.** Not a behaviour change: every `rir_base_depth` read is behind `rir_started`, and `rir_started` is set only under `ast_try_active` (`src/mccast.c:16060`), so the stale-value case was never observable.
- **The cleanup rebase is a save/restore, and it is also unobservable today.** Confirmed: `ast_hook_cleanup_call_begin` early-returns unless `ast_vn == 0`, so `ast_cleanup_bias` collapses to `(vtop - vstack + 1) - ast_base_depth` and the pair is exactly *set to current depth / put it back*. `rir_hook_cleanup_call_begin/end` (`src/mccgen.c:12243`/`:12258`) mirror that, including the `bias < 0` no-op leg. It cannot move a counter, because **every reader of `rir_base_depth` is replay-time**: `rir_op_effect`, `rir_mark_apply` and `rir_region` are reached only from `rir_to_arena`/`rir_run`, which run only from `rir_verify`, which runs once at `ast_func_end` — long after every cleanup pair has closed. The sibling exists so the invariant survives the recorder's deletion, not because it is load-bearing now. `tests/exec/features_c99_c11/cleanup.c` does exercise the hook.
- **`ast_try_active` at the `arenahasheq` gate was dead weight.** `rir_verify` is called only under `if (rir_started)`, and `rir_started` implies `ast_try_active`, so the term was always true there. It is now `rir_started`, which is the honest predicate for that site and is Replay_IR's own.

**Done — the `ast_ret_bad` widening.** `src/mccrir.c` owns `rir_try_active = rir_env && !debug_modes && !cur_func_inline_extern`, sampled in `rir_hook_body_begin`; the arming block in `ast_func_begin` is now `if (rir_try_active)` and its redundant inner `if (rir_env)` is gone. `-O0` objects byte-identical on all twelve keys; all 153 `^ast` cells pass.

**The `-O1` boards did not move — by one line, not by luck.** `ast_ret_bad` is `ast_bad_vtype(func_vt.t) && (func_vt.t & VT_BTYPE) != VT_STRUCT`, and `ast_bad_vtype` is `ast_bad_type && !ast_wide_vtype` (`src/mccast.c:2411-2432`), where `ast_wide_vtype` returns `ast_ldouble_env` for `VT_LDOUBLE` and `ast_int128_env` for `VT_INT128`/`VT_QLONG`. Both of those gates default on at `optimize >= 1`, so at `-O1` the predicate already reads false for exactly the two type classes it names — the term was only ever costing bodies where those gates are off. Twelve-key `exec` and `all` boards at `-O1` are byte-identical before and after, per-file logs included.

**Where it does pay: forced `-O0`.** `MCC_REPLAY_IR=1 MCC_FORCE_REPLAY=1` at `-O0` gains 2 bodies on x86_64 (`fn` 1144 → 1146, `faithful` 1109 → 1111) and 2 on x86_64-osx (1138 → 1140, 1103 → 1105); the other ten keys are unmoved, and `empty` stays 35 with `unfaithful`/`diverge`/`rewind`/`error` all 0 everywhere. Both bodies are `tests/exec/types/int128.c::addv` and `::mix`, the `__int128`-returning pair, and both verify `rfaithful` — 2 admitted, 2 faithful, 0 new failures. `tests/ast/o0-baseline/{board.txt,x86_64.rir.txt,x86_64-osx.rir.txt}` re-banked; `board.gated.txt` and the `.gated.rir.txt` files did not move.

**So `MCC_AST_INT128=1` in the forced-`-O0` env is now a no-op for Replay_IR coverage** — the gated board already read 1146/1140 and the ungated board now reads the same. It still gates the recorder's own tree, which the `[ast-verify]` ratchet measures, so leave the line where **Keep the measurement honest** put it and delete it with the recorder, not before.

**Superseded — the `ast_ret_bad` widening, on its own commit.** Deliberately *not* in the above, because it changes the measured population and a pure ownership transfer must not. The admission predicate Replay_IR should have is `rir_env && !debug_modes && !cur_func_inline_extern`, **dropping `!ast_ret_bad`** — an AST *value-model* limit (`long double`, `__int128`, `_Complex` returns) that Replay_IR does not share and is currently paying for. Where it goes: today Replay_IR has no admission predicate of its own at all — it is armed entirely by `src/mccast.c:16060`, `if ((jrn_env || rir_env) && ast_try_active) { ... if (rir_env) { rir_reset(); rir_active = 1; rir_started = 1; } }`, which is why `ast_try_active` is the de facto gate. The predicate belongs in `rir_hook_body_begin` as a `rir_try_active` that `rir_reset`/`rir_active`/`rir_started` key off, which also lets the arming block at `:16060` shrink to the recorder's half when the recorder dies. Expect the population to widen and new failures to surface; that is coverage arriving. Measure it at `-O0` under `MCC_RIR_FORCE=1` first, then take a fresh twelve-key board — the C2 row moves by construction, so the before/after board is the deliverable, not a regression report.

- Re-home the reverse edges: `rir_dbg_on` and its two taps out of `src/mccast.c:14805/14944`; `ast_alloc_loc`/`ast_alloc_temp_loc`/`ast_fconst_*` into neutral frame and constant services with Replay_IR and the recorder as observers rather than a priority chain buried in `ast_fconst_reuse` (`src/mccast.c:2279`); `ast_sym_defer` → `sym_defer` (Replay_IR needs it too — `rir_snap_types` exists because `Sym*` are unstable).
- Model to copy: `rir_hook_slot_record/replay` (`src/mccgen.c:7679/7684`) is already a clean parser↔Replay_IR pair that never touches `mccast.c`.

*Gate: C2 counters unmoved on every key, except for the measured `ast_ret_bad` widening.*

### P3 — rebrand the capture substrate

**Done.** `jrn_*` → `ir_cap_*`, `JrnOp` → `IrCapOp`, `JOP_*` → `IR_OP_*`, `JRN_OP_LIST`/`_ENUM`/`_NAME` → `IR_OP_*`, `JRN_W0`/`W1`/`W2`/`R1`/`R2`/`WSV`/`REC` → `IR_CAP_*`, `MCC_JOURNAL_HOOKS` → `MCC_IR_CAPTURE`, `MCC_JRN_HAVE_*` → `MCC_IR_HAVE_*`, `MCC_JRN_VA_START_VOID` → `MCC_IR_VA_START_VOID`. Nothing named `jrn`/`JRN`/`JOP` survives under `src/`, `tools/` or `include/`. Line-for-line: `mcc.h` 11/11, `mccasm.c` 6/6, `mccrir.c` 135/135, `mccgen.c` 213/209 (the four extra lines are the new include).

**The substrate is `src/mccircap.c`**, 813 lines, joined to the include chain between `mccast.c` and `mccrir.c` in both places that build the translation unit — `src/libmcc.c` and the `!MCC_AMALGAMATED` block at the foot of `src/mccgen.c`. It is not a link unit and must never become one. The move is exact: the file is `src/mccast.c:14416-15224` verbatim under the rename, `#pragma push_macro("gjmp")`/`pop_macro` still bracketing the whole region, wrapped in `mccrir.c`'s own `#if MCC_CONFIG_OPTIMIZER && (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))`.

What the split rested on:

- **`tools/asttool.c` `#include`s `mccast.c` and is unaffected**, because everything from `src/mccast.c:1289`'s `#ifdef MCC_INTERNAL` down is already invisible to it — `MCC_INTERNAL` comes from `mcc.h:4` and `asttool` never includes `mcc.h`. Had the substrate been outside that guard, `asttool` would have needed the new unit too.
- **Four references run backwards into the substrate** and now need declarations at `src/mccast.c:2272-2277`: `ir_cap_reset`, `ir_cap_gap`, `ir_cap_active`. `ir_cap_fconst_take`/`ir_cap_fconst_note` already had them. `ir_cap_active`, `ir_cap_raw` and `ir_cap_vs` are declared as tentative definitions in both files, which is one object in one translation unit, and is the idiom `src/mccast.c:4068` was already using.
- **`tools/targetgate.c`'s `ALLOWED[]` had to learn the new name** — the substrate carries two `MCC_TARGET_I386`/`MCC_TARGET_X86_64` conditionals. `tools/schemagate.c` did not: `IR_OP_` does not intersect the `AST_OP_`/`AST_FB_`/`RIR_R_`/`RIR_M_` define spaces, and its counts are unmoved at 38/21/27/28/18. **Re-measured at `da3a461b`: 38/21/27/28/19** — the region rend-value term count moved by one, which is the `RIR_R_SYNTH` region value the lost-intermediate fix added. `schemagate` is clean; the figure in this line is simply one revision old.

*Gate held: the twelve-key `C2_CORPUS=exec` `-O1` board is identical row for row before and after (`arm-win32` and `arm-wince` agreeing, as they must); `C2_NO_EXTRA=1 O0_AB_CHECK=1 tools/o0_ab.sh` passes against the bank unchanged; a direct object-sha256 A/B against the merge base over 656 corpus files × `-O1`/`-O2`/`-O3` × twelve keys is byte-identical on all 23,616 cells; 153 of 153 `^ast` cells pass; `tracegate` and `schemagate` clean.*

### P4 — the cutover

The only phase with real risk, and the only one allowed to move a counter. **It is built, measured, and shipped OFF by default.** `MCC_RIR_PROD=1` turns it on; unset, nothing about the compiler changes. The reason is below, and it is the finding of the phase, not a failure of nerve: three separate classes of **wrong code** come out of running the existing passes over the Replay_IR arena, and the arena's own lossiness is the cause of all three.

**Landed and unconditional.**

- `rir_prod_take()` (`src/mccrir.c`) builds the arena for the body just parsed, runs the pre-flight — non-empty op stream, no `ir_cap_bad`/`rir_unbal`/`rir_ovf`, no `rir_arena_mismatch`, `ast_validate`, `rir_emit_safe`, no `AST_OP_ASM` node — and **detaches** `rir_arena` (sets it to NULL so the next body allocates a fresh one) so the caller owns it. `rir_prod_replay_begin`/`_end` set up and tear down exactly the C2 prologue (`rir_c2_active`, `ir_cap_replaying`, `ast_replaying=0`, the four `rir_*rec_i` cursors, `loc = rir_body_loc_sv`, `ast_rp_*`, `ast_temp_frontier`) around the one replay that does the byte compare.
- `ast_func_end` substitutes the arena: `ast_cur = rir_prod_take()` and the tree arena is freed. **Every consumer moves with it for free**, because they all read `ast_cur` — `ast_run_strat_cycle`, the loop transforms, `AST_PF_EMIT`, `ast_plan_promotion`, `ast_inline_retain`/`ast_reemit_retain` (and therefore the `-O3` forward-inline re-emit through `ast_reemit`), `ast_baseline_retain`, `mccjit_embed_note`, `mccjit_embed_stash_leaf`. There is no separate rewiring step; item 2 of the plan collapses into item 1.
- Per-body fallback is the existing machinery, exactly as predicted: the first `ast_replay_body(ast_cur)` is compared byte-for-byte against the parser, and `!faithful` restores the parser's bytes at the site that already existed. The population is unchanged — the swap is gated on `ast_replay_ok(tree)` so no body is *newly* attempted.
- `MCC_RIR_PROD` gates it. `0`/unset is off; `1` is on; `2` also prints `[rir-prod] <verdict>\t<file>\t<func>` per body plus a `[rir-prod-total]` atexit line, into `MCC_RIR_PROD_OUT` if set. Verdicts: `used`, `fallback` (adopted, bytes diverged), `nomodel` (pre-flight refused), `noreplay` (`ast_replay_ok` false). The fifth verdict `nojit` is **gone** — defect 3 below closed and the seam carve-out with it. **Note `ast_env_int` returns the default for a value `<= 0`**, so the gate is `ast_env_gate(...) ? ast_env_int(...) : 0` — reading it with `ast_env_int` alone makes `MCC_RIR_PROD=0` mean *on*, which cost half a day.
- Production is off whenever `MCC_REPLAY_IR` or `MCC_AST_VERIFY` is set. Both are measurement modes for one of the two producers and must keep measuring that producer; `ast-verify-ratchet-*` and the whole `MCC_REPLAY_IR=5` sweep therefore read exactly what they read before.
- **Deleted**: the `RIRC2TREE` control leg and the four tree-versus-tree counters `arenacmp`, `arenacounteq`, `arenahasheq`, `treenodes` (and `rir_body_hasheq`, `rir_treekindhist`, the `heq=` field of `[rir-c2part]`, and the `arenahasheq` column of `tools/c2_sweep.sh`). **`arenahasheq` is retired deliberately** — it measured fidelity against the thing being deleted; C2 measures against the parser and is the metric that survives. The **Scoreboard** table's `arenahasheq` column is dead and must not be re-derived. The C3 *pair* probe (`pair=`/`samefolds=`/`samehash=`/`pairfired=`) survives, because the 14 `ast/rir-c3-*` cells are a live gate over it; it is now gated inline on `rir_env >= 6 && ast_replay_ok(ast_cur) && intention hashes equal` instead of on the deleted counter, and it dies with the recorder in P5.
- `rir_emit_safe` and its `rir_unsafe`/`rir_bb_slot` helpers left `#if MCC_REPLAY_IR_C2` — production needs them. This is the "amalgamated build hides a missing declaration" class again: it compiled everywhere `-DMCC_REPLAY_IR_C2=1` was set and broke `selfhost-*` and `target-link-gate`, which build the five target sets without it.
- `tests/ast/journal_inert.cmake` runs both legs under `MCC_RIR_PROD=0`. The cell proves the **capture substrate** is byte-inert; with the cutover on, `mcc` and `mcc_nojrn` legitimately differ because one has a different *producer*, not a side-car. Re-pointing it keeps the property it was built to prove.

**The gate, measured.** With `MCC_RIR_PROD` unset:

- `C2_NO_EXTRA=1 O0_AB_CHECK=1 tools/o0_ab.sh bc2 all` passes on all twelve keys, object sha256 and forced-`-O0` counters unmoved against the bank.
- Direct object-sha256 A/B against a clean `origin/main` build over `find tests -name '*.c'` × twelve keys × `-O1`/`-O2`/`-O3`: **19,686 of 19,686 objects byte-identical, 0 differ, 0 build failures.**
- 153 of 153 `^ast` cells pass, and the full `ctest -j 8` failure set is byte-for-byte the same list the same binary produces with `MCC_RIR_PROD=0` — 22 cells, every one of them `exec*/bound_global`, which is the long-checkout-path artifact recorded below and not a change. A pristine `origin/main` build configured under a short path is 8,254 of 8,254 green.
- `tracegate`, `schemagate`, `targetgate` clean; `src/mccrir.c` still contains no `MCC_TRACE`. All four side configurations (`MCC_SINGLE_SOURCE=OFF`, `MCC_CONFIG_OPTIMIZER=OFF`, `MCC_REPLAY_IR=OFF`, `MCC_CONFIG_ASM=OFF`) build green in distinct build directories.

**What it costs when it is on** (`MCC_RIR_PROD=1`, same corpus, same A/B). Files whose object moves, of files that compiled:

| key | -O1 | -O2 | -O3 |
| --- | --- | --- | --- |
| x86_64 | 31/565 | 367/564 | 367/564 |
| x86_64-osx | 26/524 | 337/523 | 337/523 |
| x86_64-win32 | 28/527 | 278/526 | 278/526 |
| i386 | 34/560 | 48/559 | 48/559 |
| i386-win32 | 30/526 | 42/525 | 42/525 |
| arm | 32/554 | 44/553 | 45/553 |
| arm-win32 / arm-wince | 28/519 each | 38/518 each | 39/518 each |
| arm64 | 28/556 | 290/555 | 290/555 |
| arm64-osx | 30/558 | 292/557 | 292/557 |
| arm64-win32 | 24/522 | 276/521 | 276/521 |
| riscv64 | 75/552 | 76/551 | 76/551 |

The `-O2`/`-O3` explosion on the x86_64 and arm64 keys was **one cause**: `opt_promote = optimize >= 2` there, and register promotion was disabled for Replay_IR-sourced bodies, so nearly every file lost it. **That carve-out is gone** (defect 1 below) and the two keys re-measured on the `all` corpus read x86_64 `-O2` 69/562 and arm64 `-O2` 46/555; the table's `-O2`/`-O3` columns are P4's pre-fix reading everywhere else. The `-O1` column and the i386/arm/riscv64 columns are the arena's own effect. riscv64's 75 is the extra `ident` firings the arena's spare `Convert` nodes admit; on `tests/exec/preprocessor/hashdefine.c::main` the RIR arena fires `[ast-ident] 3` where the tree fires none, and the body gets **longer**.

**Per-body verdicts at `-O1`** (`MCC_RIR_PROD=2`, all 657 files, twelve keys):

| key | bodies | used | fallback | nomodel | noreplay |
| --- | --- | --- | --- | --- | --- |
| x86_64 | 2478 | 2221 | 22 | 72 | 163 |
| x86_64-osx | 2215 | 1991 | 18 | 70 | 136 |
| x86_64-win32 | 2364 | 2108 | 23 | 77 | 156 |
| i386 | 2465 | 2199 | 28 | 67 | 171 |
| i386-win32 | 2378 | 2155 | 23 | 71 | 129 |
| arm | 2418 | 2183 | 27 | 43 | 165 |
| arm-win32 / arm-wince | 2334 each | 2139 | 21 | 45 | 129 |
| arm64 | 2479 | 2173 | 25 | 81 | 200 |
| arm64-osx | 2463 | 2162 | 23 | 81 | 197 |
| arm64-win32 | 2392 | 2137 | 19 | 85 | 151 |
| riscv64 | 2409 | 2116 | 21 | 73 | 199 |

`noreplay` is the recorder's own refusal and is not a cost — those bodies were never optimized. `nomodel` is the pre-flight; `fallback` is the byte compare. **The banked fallback list**, 37 distinct bodies, all twelve keys unless noted: `bounds_stress.c::test16`/`::test17`, `rev64_mt.c::main`, `sweep_int64.c::main`, `overflow_inline.c::main`, and from `full_language.c` `struct_assign_test`, `s_stddef_stdint`, `s7_9_iso646_test`, `macro_test`, `char_short_test`, plus (11 keys) `ldfcast`, `ffcast`, `dfcast`, `bfa2`, `bfa3`; (9) `builtin_overflow.c::main`, `full_language.c::longlong_test`; (8) `fuzz/runner.c::triage`/`::main`/`::interesting`; (7) `full_language.c::s7_6_inttypes_test`/`::s7_22_intarith_test`; (6) `run_s_stddef.c::s_stddef_stdint`, `run_s7_9.c::s7_9_iso646_test`, `run_s7_6.c::s7_6_inttypes_test`, `run_s7_22.c::s7_22_intarith_test`; (5) `overflow_narrow.c::main`; (2) `struct_ret_variadic.c::mkv`; (1) `struct_packed_indirect.c::main`, the five `alloca_inline.c` bodies, `run_s7_28.c::s7_28_wconv`, `full_language.c::callsave_test`/`::alloca_test`. That list is the **C2 gap wearing production clothes** and matches the Scoreboard's classes body for body.

**Why it is off — three wrong-code classes, each reproduced and each rooted in arena lossiness.**

1. **Register promotion is unsound on the arena, twice — CLOSED.** Both halves are fixed and `do_promote`'s `&& !ast_rir_arena` carve-out is **gone**; the conservative escape rule was never needed and was not written. What the two halves turned out to be:

   - **The local-array extent.** `tests/ast/replay/promote.c` exited 35 instead of 42: `int arr[4]` passed to `sumptr(arr, 4)` reached the arena as a `Ref` of type `int *`, so `ast_plan_promotion`'s **fourth** loop — the `VT_ARRAY`/`VT_STRUCT` pass at `src/mccast.c:5138`, not the escape loop at `:5117`, which reads `sz` from the pointee and gets 4 in *both* legs — skipped it entirely and never poisoned `[base, base+16)`. **P4's note that `sv->sym` is NULL for a local was wrong**: `unary` sets `vtop->sym = s` for locals too (`src/mccgen.c:11030`) and the sym survives the decay, so `sv->sym->type` still reads `VT_PTR|VT_ARRAY` with the element `Sym` — the extent was in the SValue all along. `rir_leaf_slot`'s array-decay rescue now takes the `VT_LOCAL` case as well as `VT_CONST|VT_SYM`, guarded on `sv->c.i == sv->sym->c` so it only fires where the SValue still names the symbol's own slot, and on `!VT_VLA`.
   - **The op-assign's vdup.** `src/mccpp.c::tal_realloc_impl` was miscompiled because the arena's `al->p += adj_size` read `Convert t=5 (Ref r=0x100)` where the tree reads `Unary MEMBER_ARROW (Ref al)` — `r=0x100` is `VT_LVAL` over **register 0**. The vdup at `src/mccgen.c:11771` pushes a vstack slot Replay_IR does not model, so `rir_reconcile_sv` refilled it from the SValue, which by then held the target's address in a physical register. Replay reproduced the parser's bytes from it (the body was `c2ok`, never a fallback), but promotion reallocated `%rax` and left the base stale. Fixed in a new `IR_OP_VPUSHV` arm of `rir_op_effect`: when the immediately preceding mark was `RIR_M_OPASSIGN`, the depth matches, the target SValue is a register lvalue and the shadow top is an unparented pure `MEMBER`/`MEMBER_ARROW` subtree, push `ast_dup_sub` of it — exactly what `ast_hook_vpush` does for the recorder at `src/mccast.c:2322`. **Bitfield targets must be excluded**: without that the arena for `bitfields.c::main` and `bitfields_ms.c::main` re-emits at a different length, which is the whole cost the rule had.

   Bisected with `MCC_AST_PROMOTE_LIMIT` rather than `MCC_AST_OPT_LIMIT` — it isolates the promotion decision directly, and `tal_realloc_impl` is the **75th promoted** body of `src/mcc.c` at `-O2` (P4's 76th *optimized* body, same function). With both fixes `selfhost-smoke` and `selfhost-smoke-gates` pass under `MCC_RIR_PROD=1` with promotion on, and the twelve-key `all` board is **byte-identical row for row** with the arena change on and off.
2. **The const-fold templates were unsound on the arena — FIXED.** `tests/exec/expressions/cast_operator.c` printed `avg: 6` for `(int)(((double)(3 + 4) / 2) * 2)`, which is 7: the arena lost the code-free `(double)` on a constant and `ast_run_templates` folded `7 / 2` as an integer. It now prints 7 under `MCC_RIR_PROD=1`, closed by the genop float wrap under "What closed". Two things it taught, both still true. **The bug needed two statements to show**: the minimal `double avg = (double)(3+4)/2;` alone reproduced nothing — it takes a *sibling* integer `7 / 2` in the same body for the templates pass to match the folded shape, which is why a one-line repro read green. And **the byte compare cannot see this class at all**: `cast_operator.c::main` was `c2ok` before and after, because C2 validates un-optimized emission and the damage is a later pass reading a type. Every pass that reads a type off the arena stays suspect; the only test that caught this one is a golden that happened to exist.
3. **The JIT seam — CLOSED, and it was never about the arena's shape.** `selfhost-jit` (`--jit -O4 -run src/mcc.c`) segfaulted with the arena adopted, `MCC_AST_OPT_LIMIT=0` did not help, and P4 carved the swap out whenever `ast_jit_env`, `ast_jit_splice_env`, `ast_jit_dispatch_env`, `ast_jit_fns_n`, `ast_search_env`, `ast_roi_env`, `ast_slice_env`, `embed_jit` or `MCC_OUTPUT_MEMORY` was in play (verdict `nojit`). **The carve-out is gone and the cell passes.** The whole `ast_rir_seam` predicate is deleted; there is no JIT-shaped exception left in `ast_func_end`.

   What the seam actually was, in three links:

   - **The arena drops the code-free pointer cast in `for_each_elem`.** `src/mcc.h:1662` expands to `elem < (type *)(sec->data + sec->data_offset)`; the `(ElfW_Rel *)` on an `unsigned char *` emits no op, so the arena kept the raw `unsigned char *` and replay compared `struct <anonymous> *` against `unsigned char *`. `combine_types` therefore calls `mcc_warning("pointer type mismatch in comparison")` — a diagnostic the tree's arena never produced. It moves **no bytes** (both sides are pointers of the same width, and `objfmt/mccelf.c::update_relocs`, `::cfi_fde_shndx` are `c2ok` either way); it only makes a diagnostic *possible*. This is the lost-intermediate class over a pointer and is still open — see "Still open".
   - **`error1` releases the compile state before it returns.** `src/libmcc.c:651` opens with `mcc_exit_state(s1)`, paired with the `mcc_enter_state(s1)` that `#define MCC_SET_STATE(fn) (mcc_enter_state(s1), fn)` (`src/mcc.h:2128`) wraps every `mcc_warning`/`mcc_error` in. Both halves are **no-ops whenever `s1->error_set_jmp_enabled` is 1**, which is every ordinary compile (`mcc_compile` sets it at `src/libmcc.c:825`). That is the only regime the pairing was ever exercised in: from a `USING_GLOBALS` file such as `mccgen.c` the macro does *not* enter, so with the flag clear the pairing is one-sided.
   - **The runtime JIT recompile is the one compile in mcc that runs with the flag clear.** `mccjit_recompile_common` (`src/mccjit_embed.c:594`) does `mcc_enter_state(js)` on a fresh `js` and then runs arbitrary codegen. So the warning above really did set `mcc_state = NULL` and post `mcc_compile_sem`, and `gen_op` dereferenced `mcc_state` four lines later (`src/mccgen.c:4060`, `warn_extra_ptr_zero_cmp`) — SIGSEGV. `mccjit_embed.c` being arena-generic was true and was never the question.

   **The fix is the JIT seam adopting the protocol every other compile already uses**: `mccjit_recompile_common` now brackets the re-emit in `js->error_set_jmp_enabled = 1` plus a `setjmp(js->error_jmp_buf)`, exactly as `mcc_compile` does, with `stk_data_floor` saved and restored around it. Inside that bracket `mcc_enter_state`/`mcc_exit_state` are the no-ops they are during any other compile, so a diagnostic can no longer tear the state down; and a *hard* error in a JIT recompile now unwinds to the setjmp and abandons that recompile (`entry` stays NULL, the caller keeps the original code) instead of `exit(1)`-ing the user's program. `ast_reemit` additionally saves/sets/restores `mcc_state->warn_none` around its `ast_replay_body`, mirroring what `ast_func_end` already does for the tree's replay — re-emitting a body the parser has already diagnosed must not diagnose it a second time.

   **Two things this cost, banked.** Removing `error1`'s opening `mcc_exit_state(s1)` outright *looks* correct (with the flag set it is dead; with the flag clear it strands the state) and **deadlocks**: it is the release half of the `MCC_SET_STATE` pair, and `mcc_compile_sem` is a binary semaphore. Adding a matching `mcc_enter_state(s1)` before each of `error1`'s three returns deadlocks too, for the mirror reason. Do not touch `error1`; fix the caller's regime.
4. Minor, and fixed rather than carved out: **replaying an `AST_OP_ASM` is destructive.** `full_language.c::get_asm_string` re-assembles `asm volatile(... "some_symbol: .long 0" ...)`, the assembler errors on the duplicate label, the `longjmp` out of `mcc_assemble_internal` leaves the tokenizer stack pushed on the asm buffer, and the *parser* then reads `.long 0` and reports `';' expected (got '0')` at the closing brace. The tree never hit it because the recorder drops that body's asm entirely. The pre-flight refuses any arena containing an `AST_OP_ASM`; a narrower rule needs to know whether the asm text defines a symbol or emits into a section, and re-emitting either is not idempotent for the tree's replay either.

**Next step, in the order that de-risks.** None of this needs new machinery — the switch exists and the measurement is a shell loop away.

- **Done — the lost-intermediate class.** Both halves closed, written up under "What closed": the region-value route, not the field-on-an-op route the earlier revision of this file predicted. Defect 2 above is gone; defect 1 is now entirely the array-extent item below. The default still does not flip, because defects 1 and 3 stand. The cheap standing check for defect 2's family is a **runtime** A/B, not a byte one — compile and run every `tests/exec` file at `-O1` with and without `MCC_RIR_PROD=1` and diff stdout and exit code. 252 of 276 build standalone and all 252 now agree; before the fix `cast_operator.c` was the one that did not, and C2 called it clean.
- **Done**: the local-array extent and the op-assign vdup, and with them `do_promote`'s `!ast_rir_arena`. It did come from the SValue after all — see defect 1 above.
- **Done, and it was worth the whole `-O2` column.** Object-move A/B against the same binary with the switch off, `all` corpus minus `full_language.c`: x86_64 `-O2` **367/564 → 69/562**, arm64 `-O2` **290/555 → 46/555**, x86_64 `-O1` 31/565 → 30/562 (unmoved; the population differs by the dropped file). The remaining `-O2` moves are the arena's own effect, not the missing pass. The other nine keys' `-O2`/`-O3` columns are still at P4's reading and want re-measuring.
- **Done — the JIT seam, defect 3.** `selfhost-jit` passes under `MCC_RIR_PROD=1` with the `ast_rir_seam` carve-out deleted; the diagnosis and the two banked negatives are under defect 3 above. **All three of P4's blockers are closed.**
- `tests/exec/optimizer/*` and the `optfire*` cells are the sensitive instrument for "a pass fired differently". **All pass under `MCC_RIR_PROD=1` at HEAD** (the count is **243** at `da3a461b` — 105 `optfire/`, 49 `optfire-arm64/`, 45 `optfire-riscv64/`, 44 `optfire-i386/`; the "26" this line used to carry predates the per-arch scoping), `chainstore` included; the list of movers this line used to carry is closed. They are the right regression suite for each of the steps above; do not rebank them, use them.

**Two measurement traps banked while doing this.**

- `ast_env_int(name, dflt)` returns `dflt` when the value parses `<= 0`, so no `MCC_*` gate read through it can be turned *off* from the environment. `ast_env_gate` is the one that honours `0`.
- A `cp -a` of a CMake build directory is **not** a reference build: `CTestTestfile.cmake` has the original binary directory baked in absolutely, so `ctest --test-dir <copy>` silently runs the *original* build's binaries. A pristine reference has to be configured from its own source tree (`git archive origin/main | tar -x -C …`). Comparing against the copy read 119 "pre-existing" failures that did not exist. And when comparing, prefer the same binary with the feature switched off — it is exact, and it is one run.
- `exec*/bound_global` fails for every configuration in a worktree whose absolute path is long: `bt_info.file` is `char[100]` (`src/mccrun.c:783`) and the truncated path prints as `bound_globa:7`. It is a property of the checkout path, not of any change.

*Gate as met: `-O0` unchanged; at `-O1`/`-O2`/`-O3` every one of 19,686 objects is byte-identical to the pre-cutover build with the switch off, and the per-key fallback list above is what the switch costs when it is on.*

**Still owed by P4, deliberately not attempted.** Flipping `MCC_REPLAY_IR_C2` on per target as each key reaches 100% on the `all` corpus (no key is there). Ungating `MCC_REPLAY_IR` — dropping the CMake option, removing the `REPLAY_IR` row from `GATES[]` in `tests/fuzz/runner.c`, making capture unconditional while verify stays gated (`rir_verify` costs +7.7% wall clock, 4.30 s against 4.63 s over 266 corpus compiles at `-O1`), and turning `rir_parity.cmake`/`rir_c3.cmake`'s 77 skip paths into hard failures. Both of those belong after the default flips, not before: ungating capture for every user build is only worth its cost once the arena is the producer.

### The object-move columns, all twelve keys

Same binary, `MCC_RIR_PROD` on versus off, `all` corpus minus `full_language.c`, `SOURCE_DATE_EPOCH` pinned. This is what the switch currently costs in changed code, and it is the column the array-extent work moved most (x86_64 `-O2` was 367 before promotion came back).

| key | `-O2` | `-O3` | population |
| --- | --- | --- | --- |
| arm-win32 / arm-wince | 32 each | 33 each | 518 |
| i386-win32 | 36 | 36 | 525 |
| arm | 38 | 39 | 553 |
| arm64-win32 | 39 | 39 | 521 |
| i386 | 42 | 42 | 559 |
| arm64 | 46 | 46 | 555 |
| arm64-osx | 47 | 47 | 557 |
| x86_64-win32 | 50 | 50 | 526 |
| x86_64-osx | 56 | 56 | 523 |
| x86_64 | 69 | 69 | 562 |
| riscv64 | 76 | 76 | 551 |

`arm-win32` and `arm-wince` agree at both levels, as they must.

**`-O2` and `-O3` are identical on nine of twelve keys, and differ by exactly one body on the three arm keys** — `tests/exec/pointers_arrays/array_in_struct_init.c`, which moves at `-O3` and not at `-O2`. The file's earlier claim that `-O2`/`-O3` are byte-identical on every counter was measured on five keys and is very nearly right; this is the one exception, and it is worth one look before the switch flips.

**Two traps in taking this measurement, both of which produced a wrong answer first.** Without `SOURCE_DATE_EPOCH` pinned, an embedded timestamp makes a file differ between its own two compiles, which inflated the arm `-O3` count from 39 to 40 and invented a second `-O3`-only body. And a per-file enumeration must skip a file the *off* leg fails to compile, or a pre-existing failure reads as a move.

**Done — a pre-existing riscv64 crash, and it was a units mismatch, not a float-register gap.** `tests/diff/complex_abi/complex_abi.c` aborted at **every** `-O` with `freg: Assertion 'r >= 8 && r < 16' failed`, switch off, and nothing measured it because the `rc` check excludes non-compiling files. It is not a complex defect at all — `harness.h` alone reproduces it, and the four-line minimum is `void f(int c){ register long a0 __asm__("a0") = c; __asm__ volatile("ecall" ::"r"(a0)); }`. A plain `long a0 = c;` with the same `"r"` constraint is fine.

`asm_parse_regvar` was corrected to return a **codegen** index, which is right for `VT_VALMASK`. But `asm-constraints.inc.c` then assigned `op->vt->sym->r & VT_VALMASK` straight into `op->reg`, which riscv64's constraint solver and `asm_gen_code`'s `mcc_ireg`/`mcc_freg` read as a **machine** encoding — `load(r=-10)` reaching `freg`. The shared prologue has always worked on x86_64 and i386 because there the two numberings coincide (0-7 = eax-edi); on riscv64 codegen 0-7 are a0-a7 but machine 10-17, and codegen 8-15 are fa0-fa7 against machine 42-49.

Fixed with an arch hook, `ASM_REGVAR_ASMREG`, identity by default so no other backend changes, and riscv64 mapping the two ranges and returning -1 for anything else — which reads as "no register asked for" and lets the allocator choose, the same philosophy the `asm_parse_regvar` split used. Verified by decoding the emitted bytes rather than by the absence of a crash: `ld a0,-32(s0)` / `ld a7,-40(s0)` / `ecall`, so the binding is honoured. riscv64 now compiles **552** files of the `all` corpus instead of 551. **Any other backend whose codegen and asm register numberings differ has the same latent bug**; arm and arm64 are worth a look.

### The `MCC_RIR_PROD=1` census — the remaining work list

Measured on a **short checkout path**, which matters: the 22 `exec*/bound_global` failures both P4 and the array-extent work reported as pre-existing are an artifact of a long worktree path (`bt_info.file` is `char[100]`). On `/home/llg/Projects/moderncc` the suite is **8255/8255 with the switch off**, so the prod-on comparison is exact and noise-free.

The census read **36 failures of 8255** and named five distinct compiler defects plus their selfhost consequences. **All six defects are now closed** — a sixth turned up that the census had no row for.

| body | suites | closed by |
| --- | --- | --- |
| `ptr_longlong_arith32` | 13 | the op-assign vdup taking an `AST_Load` over a pure address — modelled, not refused |
| `array_2d_iv` | 1 | the same one line; it was never a separate defect |
| `flt_eval_method` | 1 | the `c == 0` skip in `ast_fconst_record` — a shared-replay defect, not a model one |
| `bitfields_ms` | 11 | refusing a parented register-lvalue `Ref` in `rir_prod_take` — a **refusal**, not a model fix |
| `chainstore` | 2 | the arena honouring `ast_chainstore_env` |
| `selfhost-output-parity-O2/O3` | 2 | the do-while comma condition — the sixth defect |
**Deleting the JIT seam's carve-out widened the population.** The four `exec-search*` suites set `-fopt-search` and were excluded from the arena swap outright, so closing defect 3 exposed the same bodies there too — +12 cells at the time, every one of them a body already named, none new. Coverage arriving, not regression; they close with their bodies.


**Three defects, all invisible to C2 and all found by the runtime A/B.**

- **The dangling register lvalue.** `s->a += 6` on a bitfield leaves the arena's op-assign vdup slot as `Ref r=<reg>|VT_LVAL` — *dereference whatever is in that machine register* — because the `IR_OP_VPUSHV` clone excludes `VT_BITFIELD` targets. The register's producer is a **sibling's emission**, a dataflow edge no pass can see, so `ast_plan_promotion` promoted the pointer out of its frame slot, deleted the defining `mov -0x10(%rbp),%rax` and left the dependent `mov (%rax),%eax` reading whatever was there. It is **not MS-specific** — plain `bitfields.c` has the identical shape and escapes only because the recorder declines that body, so with the tree as producer it is never optimized at all. A 15-line repro is `struct S { unsigned x:12, a:4; } _s, *s = &_s; s->a += 6; ++s->a;`. `rir_prod_take` now refuses any arena holding a parented register-lvalue `Ref`, which is production-only (`rir_prod_take` returns NULL under `rir_env`) and therefore **cannot move a C2 counter by construction**; the body keeps the parser's bytes, which is what the tree does with it anyway. Cost over the 2198 bodies of the `all` corpus: `used` 2013 → 2004, `nomodel` 49 → 62. **This is a refusal, not a model fix** — it is also what makes `ptr_longlong_arith32` and `array_2d_iv` stop failing, and their underlying arena defect is untouched. Modelling it means making the clone byte-faithful for a bitfield target; see the banked negative.
- **The chainstore gate.** `rir_op_effect`'s `IR_OP_VSTORE` arm set the `chained` fbit unconditionally where `ast_hook_vstore` sets it only under `ast_chainstore_env`, so `ast_finalize_chainstores` fired even with `MCC_AST_CHAINSTORE=0` and the gate-off and gate-on objects came out identical — which is exactly what `optfire`'s `differ` mode calls "pass DID NOT FIRE". x86_64 and arm64 hid it because `ast_plan_promotion` has its own `ast_chainstore_env` block; i386 has no promotion machinery at all and riscv64's does not fire on that case, which is why only those two cells failed. With the gate honoured all four objects (prod × gate) match the prod-off pair byte-for-byte.
- **The do-while comma condition.** `while (c = (unsigned char)*p, c != '\0')` reached the arena as `If op=4 nc=2`, with the condition buried as the last statement of the held-store BasicBlock. The tree has `nc=3` — body, condition, prologue — and `ast_replay_bb`'s `op == 4` arm reads slot 1 as the condition and slot 2 as the prologue. `rir_if_safe`'s case 4 did not catch it because both slots were BasicBlocks and it only checks slot 0. It re-emitted the parser's bytes exactly and mispromoted by **one instruction** (`mov %ecx,%r15d` for `mov %eax,%r15d`). The body is `libmcc.c::dynarray_split`, and `#pragma comment(option, "-mms-bitfields")` in `bitfields.c` is the only test in the tree that reaches it — which is why the stage-1 mcc failed on exactly `bitfields.c` and `bitfields_ms.c` and nothing else. `rir_cf_cond` now parks the held stores in `rir_cfpfx`, which the `RIR_R_DO` rend already appends as child 2.
**A prediction was right for the wrong reason.** The census expected the seven `selfhost-*` cells to clear with the five bodies. They did — but `output-parity-O2/O3` were a **sixth** defect with no row, a stable self-compile miscompile that only that cell can see, because `fixpoint` compares stage2 against stage3 and a stable miscompile survives it. Treat `selfhost-output-parity` as its own row in any future census.

The instrument for this list is the runtime A/B, not the byte compare: compile and run every `tests/exec` file at `-O1` with and without the switch and diff stdout and exit code. C2 calls every one of these bodies clean, because it validates un-optimized emission. **Run it under the failing cell's own gates** — the three modelled bodies all read SAME at a plain `-O1`; `ptr_longlong_arith32` needs `-O2`, `array_2d_iv` needs `-O2 MCC_AST_OPASSIGN=1 MCC_AST_IVSR_PTR=1`, and `flt_eval_method` needs `-O1 -fno-reemit-templates -fpromote-locals` (neither gate alone reproduces it). The gate sets are on the suite's `set_tests_properties` line in `CMakeLists.txt`.

**P5 readiness, measured rather than assumed.** The 44 `ast/replay-*` cells — the fixture suite the plan says survives the recorder and becomes Replay_IR's regression suite — pass **100% with `MCC_RIR_PROD=1` as well as with it off**. They assert `[ast-replay]`/`[ast-promote]`/`[ast-inline]` markers, which are producer-agnostic, so that inheritance is confirmed and not merely hoped for. Whatever else P5 has to do, it does not have to rewrite those 44 cells. (**44 confirmed by `ctest -N` at `da3a461b`**; this line read 45 and the P5 inventory read 44 — the inventory was right.)

### P4 is complete — Replay_IR is the production arena by default

`MCC_RIR_PROD` defaults to **1**. `MCC_RIR_PROD=0` still turns it off, and that escape hatch should live until P5 lands.

What the flip rests on, all measured on a short checkout path with nothing else running:

- **Full `ctest` 8255/8255 with the default on**, and 8255/8255 with it off. The census that read 36 failures reads none.
- **The arena drives 91.5% of bodies** — over the whole `all` corpus at `-O2` on x86_64, `used=2011 fallback=11 skip=176` of 2198. The other 8.5% keep the parser's bytes, which is the per-body fallback working as designed, not a defect.
- `used` is 2011 and not the 2004 the bitfield refusal measured alone: the op-assign modelling recovered seven bodies the refusal would have declined. **The two fixes compose better than either half**, which is the argument for landing modelling rather than refusals wherever it is affordable.
- **C2 boards unmoved**: `exec` gap 10 over twelve keys, `all` gap 104. C2 runs under `MCC_REPLAY_IR=5`, which disables production, so this is the control.
- **`-O0` A/B unmoved** on all twelve keys, objects and counters, `arm-win32 == arm-wince`. The recorder never ran at `-O0` and still does not.
- `tracegate`/`schemagate`/`targetgate` clean; four side configurations green in distinct build dirs.

**A weak check worth not repeating.** "Compile one file with and without the switch and diff the object" proves nothing — only 69 of 562 files differ at `-O2` on x86_64, so most files are identical either way and read as "the flip did not take effect". Verify against a file already known to differ, or against the `[rir-prod-total]` counters at `MCC_RIR_PROD=2`.

### P5 — delete the recorder — blocked, and on exactly one thing

**The premise "nothing consumes the recorder's tree" is false, and the counter-example is one line.** `ast_replay_ok(ast_cur)` at `src/mccast.c:16553` is evaluated on the **tree**, and it is the arena's admission gate: `rir_prod_take()`'s arena is adopted only for a body the *recorder* also came out replayable on, and is otherwise freed with the verdict `noreplay`. `ast_replay_ok` is `!ast_bail && !ast_desync && ast_vn == 0 && ast_cf_top == 0 &&` root-has-a-child, and the first four terms are recorder state. With the recorder gone the predicate collapses to "the arena has a body", which is true of every arena the pre-flight admitted — so **the deletion widens the optimized population by every body the recorder declines, and those bodies have never been through a single pass.**

There is a second consumer, and it turns out to be free: when the pre-flight refuses (`nomodel`), `ast_cur` stays the tree and **the tree is optimized and re-emitted**. That path is not a fallback to the parser's bytes, contrary to how P4 reads.

**Measured at HEAD on x86_64 over `find tests -name '*.c'`, 657 files.** Per-body verdicts at `-O1` (`MCC_RIR_PROD=2`), 2503 bodies: `used` 2244, `fallback` 22, `nomodel` 80 (3.2%), `noreplay` **157 (6.3%)**. The two halves of the deletion were then simulated separately against the same binary with `SOURCE_DATE_EPOCH` pinned — files whose object moves, of files that compiled:

| what the deletion changes | `-O1` | `-O2` |
| --- | --- | --- |
| widen — adopt the arena on its own pre-flight, ignoring the recorder's verdict | **36/562** | **67/562** |
| narrow — drop the tree as the `nomodel` producer | **0/565** | **0/564** |
| both, i.e. the P5 end state | **36/563** | **66/561** |

**The narrow half is free; the widen half is the whole cost.** So exactly one property of the recorder is load-bearing — its per-body decline verdict, which keeps 6.3% of bodies out of the optimizer entirely. Everything else P5 lists really is dead.

**The three hangs are closed, and they were three unrelated defects, none of them a Replay_IR modelling error.** The whole corpus now compiles under `MCC_RIR_ONLY=1` on all twelve keys at `-O1` and `-O2`, 657 files, no hang and no crash; the three files run five times each at both levels on every key with zero failures. What they turned out to be:

- **The range-designator element type — a dangling stack `Sym`.** `decl_designator` (`src/mccgen.c:13391`) builds a stack-local `Sym aref` for `[a ... b] = x`, points a stack-local `CType` at it, pushes that on the vstack and calls `ast_hook_bail()`, which is why the recorder declines the body. Replay_IR had no sibling, so the arena stored `&aref` as its `type_ref` and replay dereferenced a dead frame — ASan names it in one run as a stack-use-after-return in `is_complex_type` from `vstore` in `ast_replay_bb`. **`rir_snap_types` does not save it**: its guard is `if (!rir_env) return;`, so the snapshot table is inert in production. Turning that guard into `rir_env || rir_prod_env` does fix the two files, and **it is a banked negative** — `rir_xt[]` is reset per body by `rir_reset` while an arena retained for inlining outlives it, and `ast/replay-inline` goes 42 → 33. The landed fix is the recorder's own shape: `rir_hook_bail()` sets a flag that `rir_prod_take` refuses on, which is production-only and cannot move a C2 counter. Closes `struct_init.c` and `range_designator_copy.c`. Modelling this properly needs snapped `Sym`s with the arena's lifetime, not a per-body table.
- **`cleanup.c` was not a defect at all — it was `O(n²)`.** `main` expands `INCR_GI7` to **65536** cleanup blocks, which is a legitimate 524,519-node arena with ~49,000 distinct local slots. `ast_du_find` was a linear probe over a fixed 2048-entry table, and on overflow `ast_du_state` went `-1` and *every* escape query fell back to `ast_cprop_escapes_scan`, a full arena walk — so `cprop`, `cse` and `dse` each became quadratic in node count. Every stack sample during the hang is in that scan. The table now grows and is hashed; the file compiles in 0.6 s. The results are identical either way by construction — that is what `MCC_CONFIG_AST_SHADOW`'s `ast_du_diverge` asserts — so this moves no byte anywhere.
- **`MCC_AST_OPT_LIMIT=0` was a false lead on all three.** It "fixes" `struct_init.c` and `range_designator_copy.c` only because with no pass firing there is no `AST_PF_EMIT` and the parser's bytes ship; the dangling type is read by the *first* `ast_replay_body`. On `cleanup.c` it does not help at all, which the earlier reading did not check. Per-gate `MCC_AST_*=0` tables were also useless, for the recorded reason.

**Two more files, and the sweep is the only thing that found them.** The three were what x86_64 showed; `tests/exec/programs/grep.c::compile` and `tests/exec/features_c99_c11/atomic_counter.c::atomic_counter_test` abort at `-O2` on the **three arm64 keys only**, in `store`'s trailing `assert(0)` (`src/arch/arm64/arm64-gen.c`). A promoted local used as a store target is a bare register SValue, and arm64's `store` has no register-destination case — x86_64 and i386 have carried one all along and arm64's own `load` already had it. Added, and both files verified to produce byte-identical stdout under `qemu-aarch64` with the switch off and on. **Sweep every key before believing a fix**: two of the three original files are clean on arm64 at both levels and would have read as "already fixed" there.

**And a sixth, which only `ctest` found: a promoted store target loses its assignment.** `MCC_RIR_ONLY=1` newly admits `mccelf.c::mcc_write_elf_file`, whose `if (fd < 0 || (f = fdopen(fd, "wb")) == NULL)` is a `Store` *used as a value*, so it replays through `ast_replay_value`, which pushed the target and then the value and called `vstore`. With `f` promoted, the target is a bare register; `save_regs(0)` inside the call spills it, `save_reg_upstack` rewrites the vstack entry to the temp it just stored to, and the assignment lands in the temp while the promoted register keeps its stale value. The self-compiled stage-1 mcc then got a garbage `FILE*` and spun — the visible symptom was `selfhost-output-parity` hanging on `tests/exec/arch/riscv_asm.c`, 1,600 bodies away from the cause. `ast_replay_bb`'s *statement* arm has always avoided this by evaluating the value and then calling `ast_promo_write`; the two `ast_replay_value` arms now do the same, restricted to a value subtree containing a call — the only thing that can spill a pinned register, since `get_reg` and its `save_found` fallback both skip them. A 30-line repro is in the commit. **Banked negative: do not fix it by making `save_reg_upstack` skip pinned registers.** That is the tempting one-liner and it is semantically right, but it also deletes every *harmless* spill of a promoted register and moves **36 objects at `-O2`/`-O3`** with the switch unset. The narrow fix moves zero. And x86_64's `store` did not mask the register indices in its register-to-register modrm byte, so a destination of `r12`-`r15` encoded as a different register pair — unreachable while every such target was being spilled, fixed alongside.

**Done — what `MCC_RIR_ONLY=1` cost, and it was 3 of 8255 `ctest` cells, all the same non-defect.**

- **3 × `optfire`** — `optfire/opassign`, `optfire-arm64/opassign`, `optfire/default-landor_invert`. These were not defects: `MCC_AST_OPASSIGN` and `MCC_AST_LANDOR_INVERT` are both on the list of 14 recorder-shape gates with no reader outside the hook block, and with the arena as the sole producer they stop changing the output, which is exactly what `optfire`'s `differ` mode reports as "pass DID NOT FIRE". **The three cells are deleted**, one line each from `tests/optfire/differs.txt` and `tests/optfire/defstate.txt` plus their two `tests/optfire/arch.txt` scoping entries. The two *gates* are still there and die with the recorder.

**Done — `dead_code.c::main`, and it was never a Replay_IR defect at all.** The 19 `exec*/dead_code` cells were reading a **pass** bug that the recorder's own decline verdict had been hiding since the pass was written. `ast_sccp_scan` folds `if (const)` by keeping the taken arm and discarding the dead one, guarded by `ast_sccp_has_label(dead)` — and `ast_sccp_has_label_compute` counts only `AST_Jump` with `op == 4`, a *label* definition. `op == 2` is a **case** marker and `op == 3` is **default**, and a `case` inside a dead arm is exactly the thing that turns `nocode_wanted` back off (`gind()` clears it), so the code after it is live. `switch (i) { if (0) { ... case 41: printf("caseok"); } }` therefore lost its arm, its case entry and its body.

Two things it teaches. **The producer was irrelevant**: a 25-line reduction of the two switches reproduces identically under `MCC_RIR_PROD=0`, i.e. with the recorder's own tree as the arena. `dead_code.c::main` escaped only because the recorder declines *that* body, which is the "class that only Replay_IR can reach" one more time — a latent shared defect that costs nothing until the population widens. And **the earlier reading that "the first replay is faithful and the re-emit is not" was right about the symptom and wrong about the cause**: the re-emit is not idempotent *because the arena it re-emits is not the arena the first replay saw*, the pass having deleted the case markers in between. `MCC_AST_SCCP=0` did not name it because there is no such gate — SCCP is a strategy in `ast_run_strat_cycle`, and the only `MCC_AST_*` that turns it off is `-fno-reemit-templates`, which turns everything off. Confirm a pass with `MCC_AST_REPLAY_DUMP=1` and read the `[ast-*]` counters; do not trust a gate name that produces no change in them.

The fix is a second memoized predicate, `ast_sccp_has_case`, consulted beside `ast_sccp_has_label` at the one site that *deletes* a subtree. **It must not widen `ast_sccp_has_label` itself**: five of that predicate's six call sites are loop- and switch-shaped conservatism guards, and one of them (`ast_cprop_switch_meet`) is handed the switch node itself, whose body always holds case markers — widening there disables switch cprop outright. `ast_sccp_has_case` also stops at a nested `AST_If op=6`, because an inner switch's cases are internal to it and deleting the pair together is sound. Cost with the switch unset: **zero objects** over 19,425, so no body in the corpus but this one has the shape.

**`MCC_RIR_ONLY` is the switch, and it now defaults to 1** (`src/mccast.c:2040` — the switch is in `mccast.c`, not `mccrir.c`). On, `ast_func_end` adopts `rir_prod_take()`'s arena on the pre-flight alone and keeps the parser's bytes when the pre-flight refuses — exactly the state the deletion produces, measurable while the recorder is still there to be compared against. It is the same shape `MCC_RIR_PROD` had for P4, and it exists so that P5 does not have to be a 3,000-line deletion and a behaviour change in one commit. `MCC_RIR_ONLY=0` still turns it off and that escape hatch should live until the deletion lands. `MCC_RIR_ONLY` is inert under `MCC_REPLAY_IR` and `MCC_AST_VERIFY`, because `rir_prod_env` already is.

**The order P5 now has to run in.**

1. **Done** — the three hangs, plus the two arm64 aborts and the self-host miscompile the wider sweeps turned up. The whole corpus compiles under the switch on all twelve keys.
2. **Done** — `dead_code.c::main` (the SCCP case-marker bug above) and the riscv64 self-host abort (the indeterminate spill `r2` below). `MCC_RIR_ONLY=1 ctest -j 8` is **8252 of 8255**, and the three are the `optfire` cells that die with the gates they measure.
3. Justify the moves the switch makes, the way P4 justified its own — the instrument is the runtime A/B (compile and run every `tests/exec` file, diff stdout and exit code), not the byte compare, because the byte compare cannot see a pass reading a type off the arena. The runtime A/B itself is **252 of 252 identical at `-O1` and at `-O2`**, switch on against switch off, stdout and exit code both. The board it has to justify, same binary switch on versus off, `all` corpus minus `full_language.c`, `SOURCE_DATE_EPOCH` pinned, as `-O1` / `-O2` over the population:

| key | `-O1` | `-O2` | population |
| --- | --- | --- | --- |
| x86_64-win32 | 25 | 51 | 526 |
| arm-win32 / arm-wince | 27 each | 33 each | 518 |
| i386-win32 | 28 | 34 | 525 |
| x86_64-osx | 30 | 53 | 523 |
| arm | 35 | 41 | 553 |
| x86_64 | 36 | 67 | 564 |
| i386 | 36 | 42 | 559 |
| arm64-win32 | 36 | 65 | 521 |
| arm64-osx | 45 | 76 | 557 |
| arm64 | 47 | 79 | 555 |
| riscv64 | 83 | 85 | 552 |

x86_64 reads 36 and 67 against the 36/562 and 66/561 P5 measured by simulating the widen half alone, so the switch is doing exactly and only what that simulation said it would. `arm-win32` and `arm-wince` agree at both levels, as they must.
4. **Done** — `MCC_RIR_ONLY` is on by default and the three `optfire` cells are deleted. The switch-off object A/B is **19,425 of 19,425 byte-identical over twelve keys × three `-O` levels**, and `ctest` is 8252/8252. The measurements are written up under **The `MCC_RIR_ONLY` flip, as measured** in the Handoff.
5. *Then* delete. **"With the switch already on, the deletion moves nothing by construction" was a prediction, and the measurement falsified it** — see the item below. It moves exactly one file.

**The deletion has a second reader of the recorder's decline verdict, and `MCC_RIR_ONLY` never bypassed it.** With the 1,618 lines of hooks removed, the twelve-key object A/B against the same corpus reads **19,425 compared, 12 differ** — one file, `tests/exec/statements/scopes.c`, on all twelve keys and at **`-O3` only**. The reference leg is exact: the pre-deletion binary under `MCC_RIR_ONLY=1`, which is the flipped state by construction.

The cause is not the arena. `MCC_RIR_PROD=2` reads `used=15 fallback=0 skip=0` on that file in both legs, so adoption is identical. The pass counters are what move: `[ast-inline]` goes **2 → 6**, and the extra lines are `grafted f6`, `specialized f6 (1 const arg)` and `re-emitted main_6 (forward inline)`. **`ast_fn_inlinable` (`src/mccast.c:2516`) and `ast_reemit_retain` (`:2895`) both open with `... || ast_bail || ast_desync`** — the recorder's own per-body decline state. `MCC_RIR_ONLY` bypasses that verdict at the *adoption* site (`ast_replay_ok`) and nowhere else, so the `-O3` forward-inliner kept consulting it right up to the deletion. With the recorder gone those two flags are permanently 0 and the graft path widens.

This is the same finding P5 already made — the decline verdict was load-bearing — with a **second** consumer the inventory missed, and it is intrinsic to the deletion rather than something to preserve: `ast_bail`/`ast_desync` are recorder state and there is nothing left to consult. The widened graft is correct on the one body that moves: `scopes.c` compiled `-O3` and run produces **byte-identical stdout and exit code** either side. Justify any further widening the same way — the runtime A/B, not the byte compare, because a pass reading a type off the arena is invisible to the byte compare.

**Done — the riscv64 self-host abort, and `r=50` is `VT_LOCAL`, not an asm register.** `selfhost-riscv64-docker` aborted under the switch at `freg: Assertion 'r >= 8 && r < 16' failed`, from `store(r=50)` under `ir_cap_store`. 50 also happens to be what `asm_compute_constraints`' `'f'` case allocates last, which makes this look like the `ASM_REGVAR_ASMREG` units bug wearing a different hat. It is not: `VT_LOCAL` is `0x32` and `VT_VALMASK` is `0x7f`, so `store(p->r & VT_VALMASK, &sv)` in `save_reg_upstack` (`src/mccgen.c:1921`) prints exactly 50 when `p` is an ordinary memory lvalue. **Read the constant before believing a register number.**

Why that entry was reached at all is the real defect, and it is the *second* half of a trap `rir_op_effect`'s `IR_OP_STORE` arm already had a comment about. `save_reg_upstack` builds its spill target as a bare stack `SValue` and sets only `type.t`, `r`, `c.i` and `sym` — `type.ref`, `c.q.hi` and **`r2`** are whatever the frame held. `ir_cap_store` snapshots that struct verbatim, and `rir_leaf_slot` records `sv->r2` into the arena's `wide_r2` for any value below `VT_CONST`, so replay pushed a `VT_LVAL|VT_LOCAL` leaf carrying `r2 = 0`. `save_reg_upstack`'s own `p->r2 == r` arm then matched it while saving register 0 and tried to spill a memory lvalue. `type.ref` had already been sanitized at this site (it is interned through `rir_ptr_sym`); `r2` now is too, at the same place and for the same reason. `ast_sv_hi` was never exposed — it returns 0 unless the SValue is a pure constant, which is the shape of guard `r2` was missing. **The recorder has the identical unguarded clamp in `ast_finalize_leaf` and is safe only because it is fed real vstack entries, which `vsetc` initialises.** Cost: **zero** C2 counters on either corpus and zero objects with the switch unset.

*Gate as met for the switch itself, all with `MCC_RIR_ONLY` unset and re-measured after the two fixes above: object sha256 A/B against a clean `origin/main` build over `find tests -name '*.c'` × twelve keys × `-O1`/`-O2`/`-O3` is **19,425 of 19,425 identical, 0 differ**, with exit-code parity on every one of the 7,884 file×key pairs so nothing dropped out of the population silently; `C2_NO_EXTRA=1 O0_AB_CHECK=1 tools/o0_ab.sh bc2 all` passes on all twelve keys with `arm-win32 == arm-wince`; the `C2_CORPUS=exec` board reads gap **10** and `C2_CORPUS=all` reads **104**, both unmoved row for row; a full `ctest -j 8` on a short checkout path is **8255 of 8255 with no failures**; `tracegate`/`schemagate`/`targetgate` clean and `src/mccrir.c` still has no `MCC_TRACE`; the four side configurations build green in distinct directories. Under the switch, the whole corpus compiles on twelve keys at `-O1` and `-O2` with no hang, no crash and no new failure, and `selfhost-riscv64-docker` passes.*

**`selfhost-riscv64-docker` does not need docker and should not be run through it.** The cell's script takes a native `qemu-riscv64` path first and only falls back to a container, and that path needs `$root/cmake-cross/riscv64-libmccrt.a` — a plain `bc2` built `-DMCC_ENABLE_CROSS=ON` has every cross runtime in it, so a `cmake-cross` symlink pointing at the build directory is enough to turn the cell from SKIP into a real run. A worktree without one reports SKIP and the cell is invisible; that is how this abort survived a full `ctest`. `selfhost-arm64-native` is gated the same way. Like `vendor`, a `cmake-cross` **symlink** is not matched by `.gitignore`'s `/cmake-cross/`, so never `git add -A`.

**The inventory, verified against the tree at this commit** (the line numbers in earlier revisions of this section had all moved): the hook bodies are `src/mccast.c:2318-4023`, 1,706 lines; 76 declarations in `src/mccast.h` and 124 call sites in `src/mccgen.c`; the shadow vstack (`ast_vs`, `ast_cf_if`/`ast_cf_savebb`/`ast_cf_top`, `ast_vn`); `ast_bail`/`ast_desync`/`AST_SET_BAIL`/`AST_SET_DESYNC`/`ast_gap_note`/`ast_replay_ok`/`ast_try_active`; the recorder-shape `MCC_AST_*` gates in `ast_configure` (14 have no reader outside the hook block: `CALL_NORETURN`, `CLEANUP_RET`, `CONVERT_GV`, `FNEG`, `INDIRECT_LOAD`, `INT128`, `LANDOR_FOLD`, `LANDOR_INVERT`, `LDOUBLE`, `NOCODE_CALL`, `OPASSIGN`, `REGPAIR`, `TERNARY_DISCARD`, `VOIDRET_EXPR` — and `ast_jit_guard_env` at `src/mccast.c:1384` is already dead today, declared and never assigned); the 4 `tests/ast/verify-baseline/` files; `ast-verify-ratchet-{O1,O2,O3}`, `ast/treecheck`, `ast/tracediff`, `tools/gate-ledger.sh`'s recorder half; `MCC_AST_INT128=1` in `rir_parity.cmake`'s forced-`-O0` env; and `MCC_RIR_PROD` and `MCC_RIR_ONLY` themselves.

**Six things inside the hook block are not the recorder's and must stay**: `ast_label_id` and `ast_label_forget` (`src/mccgen.c` calls both, and `rir_hook_goto`/`rir_hook_label`/`rir_hook_cleanup_thunk` are the reason the id exists at all), `ast_sv_hi` and `ast_cmp_invert_late` (three and one call sites in `src/mccrir.c`), `ast_bad_type` (`src/mccast.c:7037`, in the passes), and `ast_func_has_asm` (`src/mccrir.c:2541`). `ast_bad_vtype`/`ast_wide_vtype` survive only as long as `ast_try_active`'s `ast_ret_bad` term does.

**The 44 `ast/replay-*` cells survive and become Replay_IR's regression suite for free** — they assert `[ast-replay]`/`[ast-promote]`/`[ast-inline]` markers, which are producer-agnostic. So do the 20 `asttool` suites, which never touch the recorder. (**Both counts confirmed at `da3a461b`: 44 and 20. The `asttool` cells register as `ast/<name>` — `arena clone wide validate dump cfg template intention color forecast gatemap magic vlat combo_walk slice_ident slice_window slice_persist slice_graduate slice_splice slice_locate` — so grepping the cell list for "asttool" finds none.**)

**The compile-time dividend cannot be measured until the deletion happens.** `MCC_RIR_ONLY=1` still builds the shadow tree; it only stops reading it. Whatever the recorder costs per compile is still there, and the first honest reading of it is the deletion's own before/after.

### P6 — split and rename

**The `ir_`/`IR_` namespace is NOT empty, and this is P6's stated precondition.**
Both the Handoff's next-steps list and **Decisions taken** assert it is "verified
empty". It was, before P3 — and P3's own rebrand is what filled it. Measured at
`da3a461b` under `src/`: **835 occurrences — `ir_cap_*` 478, `IR_OP_*` 280,
`IR_CAP_*` 77.** A blanket `ast_*` → `ir_*` will collide with the capture substrate
P3 created. Decide the target spelling before the diff opens; `ir_cap_` is already
taken by capture, so the arena/replay side needs its own prefix or the two have to be
re-partitioned deliberately.

**The line ranges below are all stale** — they describe a 19,082-line `mccast.c` and
the file is now **15,910** lines, P5 having removed 1,618 and the passes having moved
with them. Re-derive every boundary against the current file before splitting;
none of the six numbers in the next sentence survives.

`mccast.c` splits along the boundaries the map already found: arena + replay + hashing (`85-1117`, `5640-7082`, `14413-14494`), the passes (`7083-14412`), slice/search/JIT infrastructure (`16076-18053`), and the drivers. Then `ast_*` → `ir_*`, `AST_*` → `IR_*` across the seven files and `tools/asttool.c`. Rename the things that were never AST while the diff is already open: `ast_data_all_zero`, `ast_strpool_find_or_add`, `ast_pinned_regs`, `ast_alloc_loc`, and the six `ast_*_env` codegen gates are all genuine compiler machinery carrying the prefix by accident. `tools/targetgate.c:3-7` whitelists `src/mccast.c` by name and needs the new unit names. Run `./cmake-debug/tracegate src` and `./cmake-debug/schemagate src` before every push, and keep the new units free of any `MCC_TRACE(` call for the reason recorded above.

## External suites: the gcc and llvm C tests over a self-hosted `-O3` mcc

The compiler under test is `mcc` self-hosted at `-O3` — `tools/selfhost-o3.py cmake-release cmake-release/mcc-o3 -O3` compiles `src/mcc.c` with the release `mcc` and links with `mcc` itself (its runtime supplies the x87 long-double helpers GNU ld cannot resolve). `tools/selfhost-fixpoint.py cmake-release --opt=-O3` is clean at `64c39bdf`: `o1 == o2 == o3`, **2,910,735 bytes**, byte-identical across all three stages, so the `-O3` self-host is a fixpoint and not merely a build that happened to link. (The 2,921,935 this file used to quote was `ab430dfd`'s; the size moves with `src/`.)

`tools/xsuite.py` runs the two external trees and `tools/xsuite-report.py` reads its `results.jsonl`. The harness honors each suite's own directives rather than compiling everything blind: DejaGnu `dg-do`/`dg-error`/`dg-options`/`dg-require-effective-target`/target selectors on the gcc side, lit `RUN:`/`REQUIRES:`/`UNSUPPORTED:`/`-verify` on the llvm side. A test whose directives ask for something this host or this compiler cannot express — another architecture's intrinsics, `-mavx512f`, OpenMP, LTO, a `%clang_cc1 -triple aarch64` — is **skipped with the reason recorded**, never scored as a failure. Note the invocation: `--opt=-O0`, not `--opt -O0`; argparse reads the latter as a missing argument and the docstring at `tools/xsuite.py:12` still shows the form that fails.

**Re-measured at `64c39bdf` on 2026-08-03.** 47,715 `.c` files in, 27,202 skipped by directive, **20,513 tests run at each of `-O0` and `-O3`**.

| suite | tests | `-O0` pass | `-O3` pass | skipped |
|---|---:|---:|---:|---:|
| `gcc.c-torture/compile` | 1,834 | 95.9% | 95.8% | 182 |
| `gcc.c-torture/execute` | 1,853 | 88.8% | 87.8% | 64 |
| `gcc.dg` | 11,769 | 83.8% | 83.8% | 7,145 |
| `c-c++-common` | 1,232 | 76.6% | 76.6% | 2,174 |
| `gcc.target/{i386,x86_64}` | 803 | 70.9% | 70.9% | 9,018 |
| `gcc.misc-tests` | 32 | 81.2% | 81.2% | 44 |
| `gcc.c-torture/unsorted` | 1 | 0.0% | 0.0% | 0 |
| `clang/test` | 2,972 | 71.3% | 71.3% | 7,725 |
| `compiler-rt/test` | 17 | 47.1% | 47.1% | 827 |
| **total** | **20,513** | **82.6%** | **82.4%** | **27,202** |

**The rise from 75.5% is denominator, not compiler.** This board and the one it replaces disagree by 2,960 tests moved from the run column to the skip column, and the movement is entirely in `gcc.dg` (+1,627), `c-c++-common` (+1,072) and `compiler-rt` (+261) — precisely the three suites carrying `DRIVER_DIRS` subtrees (`/vect/`, `/analyzer/`, `/gomp/`, `/lto/`, `/orc/`, …), which `gcc_plan`/`lit_plan` now skip whole. `gcc.target`, `clang/test` and the three torture suites have no such subtree and their skip counts are unchanged to the test. Checked rather than assumed: both external trees are untouched since **gcc `9d8f85ca333` (Jul 28)** and **llvm `0f1f456263b5` (Jul 3)**, both older than either run, the total is 47,715 files either way, and the skip decision is a pure function of `(path, text)` — no host probe, no compiler invocation. So the committed harness cannot produce 23,473, and the 24,242/23,473 pair quoted in `ab430dfd`'s message and previously here came from a pre-commit draft without the whole-directory skip. **Absolute passes fell** (`gcc.dg` ≈10,248 — back-computed from the old board's rounded 76.5% — → 9,868); it is the failures that were skipped away. Treat 82.6% as a new baseline, not as progress over 75.5%.

Failure buckets after the same re-measure: 1,658 `parse`, 593 implicit-declaration, 544 unresolvable GNU/clang headers, 266 nested functions, 105 inline-asm, 8 `vector_size`, 6 attribute, 2 gnu-builtin — and **1,647 in `other`**, still the only bucket that is a work list. The 952 `vector_size` this file used to report is now 8 for the same reason as above: `/vect/` is skipped whole rather than scored.

### The `-O3` column is where the defects are

**32 tests pass at `-O0` and fail at `-O3`; 1 goes the other way.** The list is `cmake-release/xsuite/o3-only-failures.txt`. Of the five previously confirmed by hand, **three now pass at both levels**:

1. `gcc.c-torture/execute/{20000412-2,conversion}.c` — **still wrong code at `-O3`**: each compiles clean, aborts at runtime, and runs to 0 at `-O0`. These two are the whole runtime-visible optimizer-defect list in 20,513 tests.
2. `gcc.c-torture/execute/medce-1.c` — **now PASS at both levels**; the third miscompile is gone.
3. `gcc.c-torture/execute/pr68506.c` — the **`vstack leak (-1)` ICE is gone**; PASS at both levels.
4. `gcc.c-torture/compile/930503-2.c` — the **`-O3` segfault is gone**; PASS at both levels.

Two more shapes in that list are optimizer *quality*, not correctness, and should not be filed as bugs: the `link_error0` idiom (`pure-1.c`, `compare-3.c`, `ieee/{compare-fp-3,fp-cmp-6,fp-cmp-9}.c`, `20020720-1.c`, `20041114-1.c` — all reporting `unresolved reference`) fails only because `-O3`'s inlining leaves a call `-O0`'s folding had already removed, and the `builtin-convert-*`/`builtin-cproj-2`/`builtin-{c,wc}type-*` rows want `__builtin_` forms mcc does not carry. `compile/{20000922-1,pr27528}.c` are the `X` asm constraint, unsupported at `-O3`'s register pressure only.

Unexamined and new to this board, all `-O3`-only `FAILEXE`: `execute/{20000715-1,990208-1,bcp-1,builtin-constant,pr85582-2}.c`, the four `{,v}{f,}printf-chk-1.c`, and `gcc.dg/{c99-func-3,pr38615,torture/pr117811,torture/pr54877}.c`. `bcp-1.c` and `builtin-constant.c` test `__builtin_constant_p` folding and are likely quality rather than correctness; the rest are unclassified.

### Open items, in the order they are worth paying for

1. The two remaining `-O3` wrong-code aborts (`20000412-2.c`, `conversion.c`). Bisect each against the pass set the way P4's classes were bisected — they are the only runtime-visible optimizer defects in 20,513 tests.
2. Classify the eleven unexamined `-O3`-only `FAILEXE` above. Any that abort rather than mis-fold join item 1; the `__builtin_constant_p` pair probably does not.
3. **153 `FAILEXE` at `-O0`** — programs that build and then abort with the optimizer off (167 at `-O3`). This is a baseline-codegen list, not an optimizer list, and it is the larger of the two. `cmake-release/xsuite/failexe.txt`.
4. **1,013 `XPASS`** — tests carrying `dg-error`/`expected-error` that mcc accepted silently (530 `gcc.dg`, 263 `clang/test`, 181 `c-c++-common`, 34 `gcc.target`, 5 torture). Each is a missing diagnostic. Low severity individually; as a set it is the honest measure of how much of C's constraint checking mcc does not do.
5. **4 ICEs**, down from the previous board's set and now a different set: `gcc.c-torture/compile/limits-exprparen.c`, `gcc.dg/pr121081.c`, `gcc.dg/strub-internal-pr113394.c`, `clang/test/Sema/attr-nonblocking-constraints.c`. Both columns, so none is optimizer-specific. `cmake-release/xsuite/ice.txt`.
6. `gcc.dg/pr97459-{2,4,5,6}.c` hang at runtime at both opt levels (15s timeout) — unchanged. `gcc.dg/O16384.c` is now skipped by directive rather than timing out.
7. The `DRIVER_DIRS` skip is doing more work than the board admits. 4,341 `gcc.dg` files, 1,720 `c-c++-common` and 554 `compiler-rt` are skipped on their *directory* alone; roughly 2,960 of those would otherwise be scored. `/vect/` and `/analyzer/` are genuinely out of scope, but the list should be re-derived per-directory rather than left as one blanket, or the board keeps flattering itself.

**Two harness caveats, so the board is not read as stronger than it is.** A test expected to be rejected is scored `PASS` when mcc exits nonzero *for any reason*, so a file rejected over an unsupported extension rather than the intended constraint violation still counts. And `dg-output` text, `FileCheck` patterns and dump-scan `dg-final` are not verified at all — a `dg-do run` test is scored on its exit status alone. Both make the pass column optimistic; neither affects the `-O3`-only delta, which compares mcc against itself.

### Known regression from the sNaN fix — `-FLT_SNAN` no longer folds

`clang/test/C/C2y/n3364.c:25`, `float f3 = -FLT_SNAN;`, now fails with
*"initializer element is not constant"*. It passed before the `__builtin_nans` fix.

Cause, understood and deliberately not reverted: `__builtin_nans` used to be `(0.0/0.0)`,
which folded, so `-` on it folded too. That spelling was wrong three ways at once — quiet
instead of signalling, **wrong sign bit**, payload ignored — and the replacement builds the
sNaN as a **union compound literal** so it stays a compile-time constant. `FLT_SNAN` alone
folds fine; what does not fold is **unary `-` on a compound-literal-punned float constant**
in a static initializer.

This is the same mechanism that broke `cli/builtin_signbit_no_trap` during that work, where
it was closed by backing the `__builtin_nan` half out. The `nans` half keeps the
compound-literal form because it is the only way to spell a signalling NaN as a constant.

**Do not fix this by reverting the header.** The trade is one test against
`nans` going from 135/135 rows wrong versus gcc to 0/135. The fix belongs in
`src/mccgen.c`: fold unary `-`/`+` on a constant float compound literal in a
constant-expression context. Negating an sNaN must not quiet it — check bits, not printed
values (gcc: `7fa00000` for `FLT_SNAN`, `ffa00000` for `-FLT_SNAN`).

### The 136 `FAILEXE`, clustered and checked against real compilers

Replaying every one through `gcc -O0` and `clang -O0` with the harness's own flags:
**94 of 136 (69%) are not clean mcc defects** — 30 fail under gcc itself (13 `hardbool`,
4 `guality` needing gdb), 14 the installed gcc cannot even build (`counted_by` postdates
it), 38 pass only under gcc-specific `-O2` object-size folding that clang also rejects.
**42 have both references passing** and are the genuine candidate list.

| n | cluster | |
|---:|---|---|
| 29 | `__builtin_object_size` / dynamic | 23 need gcc's `-O2` folding — feature gap, not wrong code |
| 24 | singletons | |
| 18 | `counted_by` / flexible arrays | 14 unbuildable by the installed gcc |
| 15 | IPA alias/modref | needs `-O2` IPA |
| 13 | `hardbool` | gcc extension; gcc itself fails these here |
| 11 | VLA | 8 genuine — see below |
| 9 | complex signed zero | **fixed** |
| 5 | `scalar_storage_order` | |
| 4 | `guality` | needs gdb |

**Open lead — the VLA cluster (8 genuine).** `expr_type()` at `src/mccgen.c:8441` does an
unconditional `nocode_wanted++` around the operand of `sizeof`/`typeof`. C99 6.5.3.4p2
requires a **VLA-typed operand to be evaluated**. Two consequences, both reproduced: side
effects are dropped (`++i` never runs), and the VLA size slot is never written, so
`vpush_type_size` reads zero — `sizeof(typeof(*(++i,(char(*)[i])a)))` gives `i=0 j=0`
where gcc gives `i=1 j=1`.
Deliberately **not** attempted: the fix needs the operand evaluated-then-discarded *only
when the type turns out to be a VLA*, which is not knowable until after parsing, and the
cluster is three sub-causes rather than one (size-expression evaluation; `alloca` versus
VLA scope in `vla-24`; statement-expression VLAs in `vla-stexp-*`). That is a refactor
with real regression risk against 8,122 passing tests, and trading ctest passes for
FAILEXE passes would be a net loss.

### Needs `src/`: two builtin-fidelity gaps that cannot be closed from a header

Both were implemented, measured to work, and then **backed out** because a header-only
form breaks file-scope uses. Recording them so the next attempt starts from the answer.

**`__builtin_nan` discards its payload.** `src/mccgen.c:11577` does `vtop--` and always
emits `0x7ff8000000000000`. The payload-correct version works as a macro but regresses
`gcc.c-torture/execute/ieee/builtin-nan-1.c`, which writes
`double n1 = __builtin_nan("0x1");` at **file scope** — a macro cannot make a payloaded
NaN a constant expression, because the payload is a string literal and the preprocessor
cannot turn `"0x1"` into `1`. It passed before only because *both* sides ignored the
payload. There is no header-only escape: mcc's `__builtin_choose_expr` **parses the
unselected arm**, so the statement-expression trick dies with *"statement expression
outside of function"*.
Fix: parse the payload in the `TOK_builtin_nan{,f,l}` case and fold to a constant, then
add `TOK_builtin_nans{,f,l}` beside it and delete both the macro and the runtime helpers.

**The float-classification macros evaluate their argument many times.** gcc evaluates
once; mcc re-expands. Measured side-effect counts — `isnan` 2, `isinf` 2, `fabs` 2,
`abs` 2, `copysign` 3, `isfinite` 4, `isunordered` 4, `isgreater` 6, `isnormal` 7,
**`fpclassify` 12**. So `__builtin_isnan(*p++)` increments twice and
`__builtin_fpclassify(…, f())` calls `f` twelve times. The
`__builtin_choose_expr(__builtin_constant_p(x), …, ({ typeof(x) v = (x); … }))` form takes
the counts to gcc's exactly (audit 22 → 12 differences) but breaks every file-scope use
for the same parse-the-unselected-arm reason.
Fix: either stop `__builtin_choose_expr` parsing the discarded arm, or make these real
compiler builtins.

Also absent entirely: **`__builtin_issignaling`** — not a token, not a macro. Now
implementable since sNaN works, but it moves no gate: all nine `builtin-issignaling-*`
tests are SKIP under `dg-require-effective-target`, not failures.

### Correction to the `ieee/` figures this file quoted

The recorded "PASS 251, FAILEXE 0, FAILCOMPILE 61" is wrong and was not reproducible.
Re-measured against a baseline `mcc-o3` built from stashed sources with the identical
command: **PASS 263, FAILEXE 0, FAILCOMPILE 13, SKIP 9**. The 61 figure counted the nine
`builtin-issignaling-*` variants as FAILCOMPILE when `tools/xsuite.py` actually SKIPs them
on `dg-require-effective-target` — including `builtin-issignaling-1.c`, skipped for
`dg-add-options(ieee)` rather than for the missing builtin. The 13 real FAILCOMPILE are
`compare-fp-3.c` and `fp-cmp-{6,7,9}.c`.

### Open — 32-byte vector types are laid out at 16-byte alignment

`mk_vector_type` (`src/mccgen.c:6532`) caps vector alignment at `MCC_MAX_ALIGN`, 16 on
x86-64. gcc and clang both lay out a bare `__vector_size__(32)` type at **32**:

```c
typedef float v8f __attribute__((__vector_size__(32)));   /* no __aligned__ */
struct H { char c; v8f v; };
/* gcc and clang: sizeof 64, offsetof(v) 32   |   mcc: sizeof 48, offsetof(v) 16 */
```

Independent of argument classification, in a different file from the ABI fix, and only
affects 32-byte vectors written *without* an explicit `__aligned__`. Confirmed present on
unmodified `main`. With these shapes excluded the ABI differential is fully green; with
them included, the only remaining diffs in a 31-shape run are the `sizeof`/`_Alignof`
lines themselves — every computed value still matches gcc. Fixing it changes struct layout
for a whole type class, a wider ABI decision than the defect it was found beside.

### Closed — the vector-float NaN "operand ordering" lead was wrong

Recorded earlier as a pre-existing defect: plain `__v4sf + __v4sf` diverging from gcc when
both operands are NaN. **There is nothing to fix, because gcc contradicts itself.** Over
9,408 bit-pattern cases (14 × 14 patterns × `+ - * /` × every lane of `__v4sf`/`__v8sf`):

| reference | mcc agreement |
|---|---|
| gcc `-O0` | 8032/9408 |
| **gcc `-O1`** | **9408/9408** |
| gcc `-O2` | 8032/9408 |
| **clang `-O0` / `-O2`** | **9408/9408** |

gcc-O0 versus gcc-O1 differ on 1,376 of 9,408 for identical source. The cause is which
operand gcc's unoptimised expansion leaves in the destination register — `-O0` emits
`movaps VA,%xmm1; movaps VB,%xmm0; addps %xmm1,%xmm0` (destination holds VB), `-O1`+ emits
`movaps VA,%xmm0; addps VB,%xmm0` (destination holds VA). It is not a lowering rule and it
is not vector-specific: scalar `float + float` on two globals diverges identically, which
contradicts the original report's "scalar never diverges" — those cases used register
operands. The reported "~30 of 100" is exactly 1376/4800, i.e. a comparison against gcc
`-O0` specifically. mcc already matches gcc once optimising and clang everywhere; changing
it would break both, on a payload IEEE 754 leaves unspecified.

### Local test tiers: what runs, and the one host change that would add more

docker, wine and qemu-user are all available on this host, and the tiers are green:

| tier | result |
|---|---|
| `cross` + `qemu` + `wine` (cmake-cross) | **90/90** — 79 run, 11 need `MCC_REPLAY_IR_C2=1` |
| full cross-build `ctest` | **8,255/8,255** |
| `docker` | **26/26** (3 arm64 cells skipped, see below) |

This matters because a large share of this session's work was ABI and codegen —
x86-64 stack-argument alignment, SSEUP vector classification, mixed INTEGER+SSE struct
returns — validated only on x86-64 Linux. The cross build exercises arm, arm64, riscv64,
i386, arm-win32, arm-wince, i386-win32, arm64-win32 and arm64-osx, and qemu-user actually
*runs* the results. **Run `cmake --preset cross` and the `cross|qemu|wine` tier before
pushing anything touching `src/arch/`, ABI classification or codegen.**

**Fixed while enabling this:** `riscv64-promote-docker` bind-mounts the build directory
and runs as root, so it left root-owned files behind and its own `rm -rf "$WORK"` then
failed with EPERM — it passed once and failed on every subsequent run. The container now
chowns the work tree back to the invoking uid/gid on exit, and `dg_reset_work` in
`tools/dockergate.sh` clears a directory an older run already poisoned, via a throwaway
container when the host `rm` cannot.

**Open, needs a host change rather than a repo change.** The three arm64 docker cells skip
because this host's `qemu-aarch64` binfmt handler is registered with flags `OC` and not
**`F`**. Without `F` the kernel resolves the interpreter inside the *container's* mount
namespace, where `/usr/bin/qemu-aarch64` does not exist, so any arm64 container dies with
`exec ...: no such file or directory`. One command fixes it, and it changes host state,
not the repo:

```
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

Worth doing: `abi-diff-arm64-docker` is an ABI differential against a real arm64
toolchain, which is precisely the class of test that caught the 32-byte vector defect on
x86-64.

### Board state at `29e0167b` — the goal metric

Goal: every gcc test clang can run should work under mcc. That is exactly the gcc-suite
`FAIL` + `FAILEXE` set, since `REFFAIL` is by definition what clang cannot run either.

**21,586 run at `-O0`** (up from 19,571 once the ISA corpus stopped being skipped):
PASS **18,838**, REFFAIL 1,075, FAIL 920, XPASS 389, XPASS_REFOK 232, FAILEXE 132.
Raw 87.3%, honest **92.9%**.

**Goal metric: 807 remaining** — 675 FAIL + 132 FAILEXE, by suite:
`gcc.dg` 458, `gcc.target` 288, `c-c++-common` 29, `c-torture/compile` 16,
`c-torture/execute` 15, `gcc.misc-tests` 1.

**One fix moved 219 tests.** Unskipping the ISA corpus exposed 227 `FAILEXE` in
`gcc.target`, almost all AVX runtime tests computing wrong answers. They shared a single
root cause — stack-argument offsets rounded against the wrong base — and after that fix
`gcc.target` FAILEXE is **8**. That is the strongest argument in this file for clustering
before patching: 227 symptoms, one defect.

What is left in `gcc.target`'s 280 FAIL is mostly *feature*, not defect: 140 missing
intrinsics (AVX-512 and friends), 34 unknown types parsed as implicit int, 52 parse
errors on types mcc lacks, 12 x87 asm tied-operand constraints.

### The board was hiding ~1,900 gcc.target tests that clang can run

`tools/xsuite.py` skipped **every** `-m*` dg-option via `BAD_OPT_RE`, and `tok_true()`
returned false for every effective-target starting with `sse` or `avx`. So the entire ISA
corpus was dropped before mcc was ever invoked — 4,031 `avx*`, 175 `sse4`, 29 `sse3`,
19 `ssse3`, and **347 `-msse2` tests for an ISA mcc has always shipped**.

This was found the hard way: an agent was dispatched to add SSE3–SSE4.2 on the premise
that `gcc.target`'s 112 failures were dominated by missing intrinsics. It measured first
and found SSE3 through SSE4.2 accounted for **zero** board failures — the real ceiling for
that task was ~7 tests. The premise was wrong because the instrument was lying.

`MCC_ISA_OPT_RE` now forwards `-m<isa>` for the ISAs mcc supports, and `MCC_ISA_ET` marks
their effective-targets true. `gcc.target` alone goes **803 run → 2,709 run**, 1,906 newly
runnable, PASS 614 → **2,065**.

**The failure count went up, and that is the honest outcome:** FAIL 112 → 280,
FAILEXE **3 → 227**. Those were always failing; they were simply never run. The 227 are
dominated by AVX *runtime* tests (`avx-set-v8sf`, `avx-vperm2f128-256`,
`avx-vinsertf128-256`, `avx-vroundpd-256`) — they compile and compute wrong answers, which
points at the confirmed 32-byte vector by-value defect rather than at 227 separate bugs.
Of the 280 FAIL, 140 are further missing intrinsics and 34 are unknown types parsed as
implicit int.

Still skipped and correctly so: AVX-512, `__bf16`, and every non-x86 target. (`_Float16`
itself is no longer skipped — see the top of this file.)

### The work list, as of the current `main` (clang-oracled)

19,571 run at `-O0`: **16,961 PASS, 999 REFFAIL, 862 FAIL, 384 XPASS, 229 XPASS_REFOK,
136 FAILEXE, 0 ICE, 0 TIMEOUT.** Raw 86.7%, **honest 92.5%** once the 1,228
clang-confirmed invalid expectations leave the denominator.

The **862 genuine FAIL** (clang compiles these; mcc does not) cluster as:

| n | signature | what it actually is |
|---:|---|---|
| 116 | `'X' expected (got 'X')` | many causes; needs sampling, not one fix |
| 101 | implicit declaration | the remaining `__builtin_*` tail |
| 90 | unresolved reference | link-stage, distinct from the `link_error` idiom that `REFFAIL` already absorbs |
| 43 | `type defaults to 'int'` | **10 are C23 `auto` type inference** (§6.7.9) — `auto i = 1;` is rejected outright; the other 33 are implicit-int diagnostic tests |
| 39 | `struct or union expected` | **the nested member designator bug**, identified early in this file and never fixed: `{.a.a=1, .a.b=2}` fails while either half alone works |
| 55 | `redefinition` + `incompatible types for redefinition` | declaration merging / C23 tag compatibility |
| 20 | `invalid array size` | the mixed bucket recorded earlier — packed bitfields, `__LINE__` across a continuation, array-size ceiling |
| 19 | `initializer element is not constant` | the incomplete constant-expression evaluator |

Four of those clusters — nested designator (39), declaration merging (55), array size (20),
constant folding (19) — are **bugs recorded in this file at the very start of the sweep and
still open**. Together 133 tests, and they are defects rather than features, so they should
outrank feature work.

By suite the 862 split `gcc.dg` 415, `llvm:clang` 259, `gcc.target` 113, `c-c++-common` 38,
`c-torture` 34.

**A caveat on the oracle's own fidelity.** clang's default is `gnu17`; mcc's is now `gnu23`.
For a test carrying no explicit `-std`, the two are being asked different questions, so
`REFFAIL` can undercount. I checked the specific case that looked most likely to be
affected — implicit int — and it is not: clang 22 rejects it at its default too. But the
general mismatch stands and the fix is to forward mcc's default `-std` to the reference
when the test does not specify one.

### The board now asks a reference compiler whether a test is a valid expectation

`tools/xsuite.py --ref clang` re-runs **the harness's own command line** with a reference
compiler whenever mcc fails to compile, or accepts something the test says must be
rejected. Two new statuses fall out:

- **`REFFAIL`** — mcc rejected it and so did clang. Not an mcc defect.
- **`XPASS_REFOK`** — mcc accepted a should-be-rejected test and clang accepts it too, so
  the `dg-error` is conditional and does not apply on this host. Not a missing diagnostic.
- **`ref=badflag`** — clang rejected the *flags*, not the source; counted as inconclusive
  and never as evidence either way. Without this guard every gcc-only flag would
  masquerade as "clang rejects this too" and silently inflate the exoneration count — the
  same trap that contaminated the earlier `-ansi` diagnostics analysis.

Measured over the full board: **999 `REFFAIL` and 229 `XPASS_REFOK` at `-O0`** (1,219 at
`-O3`), plus 93 inconclusive. So **1,228 of the rows previously counted against mcc are
not mcc defects** — 53% of the `FAIL` column and 37% of the `XPASS` column.

| | raw | with invalid expectations removed |
|---|---|---|
| `-O0` | 86.6% (16947/19571) | **92.4%** (16947/18343) |
| `-O3` | 86.5% (16920/19571) | **92.2%** (16920/18352) |

Real remaining mcc gaps at `-O0`: **878 `FAIL`, 382 `XPASS`, 136 `FAILEXE`, 0 ICE, 0
TIMEOUT.**

**Validate a `REFFAIL` the way the harness runs it, not the way that seems obvious.** Two
of four spot-checked files compiled fine under a hand-written `clang -c`, which looked
like the oracle over-counting. It was not: both are `dg-do link` tests, and the harness
*links* them. `gcc.dg/builtins-18.c` is the `link_error` idiom — clang at `-O0` fails it
with `undefined reference to link_error`, exactly as mcc does. The `-c` spot-check was
testing something the harness never does. A pleasing side effect: the whole `link_error0`
family, hand-classified earlier in this file as "optimizer quality, not correctness", is
now classified automatically.

Caveat on the oracle's authority: clang and gcc genuinely disagree — clang errors
unconditionally where gcc keys severity to the standard — so `REFFAIL` means "clang also
rejects this", not "no compiler accepts this". For gcc-dialect tests `--ref gcc` is the
better question and is a one-word change.

### CLOSED — it was never a `copysign` bug: the recorder loses in-place float folds

`gcc.c-torture/execute/ieee/copysign2.c` now passes at `-O0/-O1/-O2/-O3`.

**Both leads in this file were wrong, and the sweep is why we know.** `MCC_AST_PROMOTE`
does *not* flip it — the promotion lead was a false trail. A full sweep of **all 202
`MCC_*` gates** found exactly one fine-grained gate that changes the bit pattern:
`MCC_AST_IDENT_CONV`. And `ident-convert` is an integer-only identity, so it is not the
cause either — it is the *trigger*. It is simply the only transform that fires in the
failing function, which makes `do_ident` non-zero, which makes mcc re-emit the body from
the recorded AST instead of keeping the parser's bytes. **The wrong code was in the
recording; any optimization firing would expose it.** That is why "which gate fixes it"
was the wrong question and "what does the gate cause mcc to *do* differently" was the
right one.

Two independent holes, both the same underlying mistake — *the parser folds a float
constant in place on the value stack and the recorder never refreshes the AST node it
already materialised for that slot*:

1. `__builtin_copysign(x, y)` expands through `-__builtin_fabs(x)`. That unary minus on a
   float goes from `unary()` (`src/mccgen.c:11849`) **straight to `gen_opif(TOK_NEG)`**,
   which folds in place (`f1 = -f1`, `:3618`) and calls no backend `gen_*`, so
   `src/mccircap.c` captures nothing. Every *other* float fold reaches `gen_opif` via
   `gen_op`, which is captured as `IR_OP_GENOP`; `TOK_NEG` from `unary()` was the sole
   uncaptured entry. `RIRPRODDUMP` on the unfixed build shows the ternary recorded as
   `If(signbit(Y[2]), Literal +1.0, Literal +1.0)` — both arms positive.
2. `rir_prov_ok` (`src/mccrir.c:1146`) re-typed float constants, guarding on
   `rir_pvc[slot].i == sv->c.i`. For `long double`, `c.i` is only the **mantissa** — the
   sign lives in `c.q.hi` — so `+0.0` and `-0.0L` compared equal and `-0.0L` came back
   positive.

Fixed by `rir_stamp_flt_fold()`, which replaces a shadow node with a literal carrying the
`SValue`'s actual bits (wide half included) when the parser says the slot is a folded
float constant, plus one line refusing provenance re-typing for float constants. Sound
because replaying `Binary -(lit, lit)` re-folds to the same constant — the emitted bytes
are unchanged in the sound cases and only corrected in the unsound one, which the
byte-identical fixpoint confirms. `gen_opif` bails to `general_case` for `x/0` and
non-finite operands, so those never enter the rewrite.

**This is a general recorder defect, not a `copysign` one** — any `-` applied to a folded
float constant was affected. `copysign2.c` is merely the test that reaches it.

Differential vs gcc, 3,193 bit-pattern comparisons per level: before **132/139/139/139**
differing at `-O0/-O1/-O2/-O3`; after **132/132/132/132**. The optimization-level delta is
gone. The residual 132 are identical at `-O0`, unaffected by this change, and **not a
defect in `copysign`**: `mccdefs.h` defines `__builtin_nans(s)` as `(0.0/0.0)`, so
signalling-NaN *inputs* are quiet NaNs under mcc (`7ff8…` vs gcc's `7ff4…`); the sign
handling matches in every one of them. That is an input-fidelity gap in `__builtin_nans`
worth its own entry.

Whole `gcc.c-torture/execute/ieee/` × 4 levels: PASS 248 → **251**, FAILEXE 3 → **0**,
FAILCOMPILE 61 unchanged.

`gcc.c-torture/execute/ieee/copysign2.c` aborts at `-O1`, `-O2` and `-O3`; passes at
`-O0`; gcc passes at every level.

**It is not a regression from today's work** — check the board history before assuming so:
it was `FAILEXE` at *both* `-O0` and `-O3` on the original board and on board2. The
`__builtin_fabs`/`copysign` fix closed the `-O0` half, which is what made the surviving
`-O1`+ half visible as an "`-O3`-only" row for the first time. Exposure, not breakage.

Narrowed as far as is useful:
- Fails for **`float` and `double` at indices 2 and 4** — `copysign(-1.0, y)` and
  `copysign(-0.0, y)` with `y = -2.0` from a static array — and **not** for `long double`.
- The test compares with `memcmp`, so it is a **bit-level** difference; `%g` printing of
  the same values matches gcc, which is why a naive repro looks clean.
- The same eight operations written inline in `main` are **bit-identical to gcc** at
  `-O1`. The failure needs the calls to sit in a separate function (the test's
  `TEST(TYPE, EXT)` macro generates `void testf/test/testl(void)`), which points at a
  promotion or inlining interaction rather than at the `copysign` lowering itself.
Next step: bisect with the `MCC_AST_*` gates the way the earlier cluster was, starting
with the `MCC_AST_PROMOTE` family — three of the fixed miscompiles lived there and all
involved non-int-sized data.

### Landed — the 16-byte vector ABI now matches SysV, and GNU builtins

**Vector ABI.** `classify_x86_64_arg` had no **SSEUP** class, so mcc passed a 16-byte
vector as two separate SSE eightbytes (xmm0 *and* xmm1) where SysV puts all 128 bits in
xmm0 — mcc and gcc objects could not exchange vectors at all. Now classified SSEUP with
`x86_64_vec16_move()` emitting `movups` (alignment-free, so no frame-slot constraint),
wired through `gfunc_call`, `gfunc_prolog`, returns via the existing `gfunc_sret`/
`arch_transfer_ret_regs` escape hatch, and `va_arg`. Cross-compiler differential, 162
cases, gcc as ground truth: **gcc→mcc 43/162 → 162/162, mcc→gcc 48/162 → 162/162**, also
green at `-O2`/`-O3`, under the self-hosted compiler, and through the JIT at `-O4`.
Two findings beyond the brief: the **8-byte** vector paths were *already* wrong (integer
8-byte vectors went to GPRs instead of xmm), so "keep them working as they are" was not
achievable and they were made SysV-correct instead; and gcc *and* clang both pass
`double __attribute__((vector_size(8)))` in **memory** (V1DF is absent from gcc's vector
mode list), replicated deliberately. Residual risk is deeply nested or exotic aggregates —
the implementation is a targeted predicate, not a full eightbyte classifier with SSEUP
post-merge.

**GNU builtins.** The `implicit declaration` bucket was re-derived rather than trusted:
**282 files, not the 205 this file recorded**, and 278 of them are missing exactly one
name, so per-builtin flip counts equal file counts. `__builtin_*_overflow_p` was *already*
present and `__builtin_stdc_*`/`clzg` were 1-3 files each, not ~15. Added the width-generic
family (`clzg`/`ctzg`/`clrsbg`/`ffsg`/`popcountg`/`parityg`/`bswapg`, `bitreverse*`, all 16
`__builtin_stdc_*`, the 18 fixed-width overflow forms), `__builtin___clear_cache`,
`alloca_with_align`, 39 libm declarations with non-uniform signatures, and scalar
`int x = {};` plus `[[...]]` after a function declarator. All built on 128-bit helpers
taking `(value, precision)` as **function arguments**, so each operand is evaluated exactly
once rather than via statement-expression macros. **38 flipped FAIL→PASS.** Of 20 losses,
17 are PASS→XPASS — tests expecting rejection that mcc used to satisfy by failing to
*parse* the construct; it now accepts the construct but lacks gcc's appertainment
diagnostics, which is the cost of having the feature at all.

Deliberately skipped, each for a stated reason: `clear_padding` (needs full layout
traversal), `va_arg_pack` (needs inlining mcc lacks), `has_attribute` (a partial answer is
a *wrong* answer), `setjmp`/`longjmp` (frame semantics), `cpu_supports` (needs CPUID),
`nullptr` and `constexpr` — faking `nullptr` as `((void*)0)` would silently pass tests
that should fail on the `nullptr_t` type distinction, and approximating `constexpr` as
`static const` would silently turn `int a[n]` into a VLA.

Two bugs the differential testing caught *before* shipping, both worth the pattern:
`__builtin_clzg(x, y)` must evaluate **both** arguments even when `x != 0`
(`gcc.dg/torture/pr122188.c` exists for exactly this), and an early 64-bit-only
implementation silently miscomputed for `unsigned __int128` because a `-dM -E` probe
missed that `__SIZEOF_INT128__` is set inside `mccdefs.h`, which that dump does not show.

**`__has_builtin` always returned 0** — `runtime/include/mccdefs.h:317` defined it as the
literal `0`, so it expanded before the preprocessor logic could ever see it. Removed;
`expr_preprocess` now answers it, scanning the argument with `next_nomacro` (gcc and clang
do not macro-expand it either) and reporting 1 for a DEF'd token or a defined
`__builtin_*` macro.

**Measure this under `-c`, not `-E`.** A large block of `runtime/include/mccdefs.h` sits
behind `#ifndef __MCC_PP__` starting at `:324`, so preprocess-only mode does not see the
macro-defined builtins at all and every one of them reads as absent. An earlier round of
this work was tested with `-E` and wrongly concluded the macro case was still broken. Under
`-c`, verified against gcc: `__builtin_clz`, `__builtin_expect`, `__builtin_clzg`,
`__builtin_stdc_bit_width` and a bogus name all agree.

Three residual differences, all understood:
- `__builtin_bitreverse32` — mcc says 1, **gcc says 0**. mcc genuinely has it now and gcc
  does not; mcc is right.
- `__builtin_clear_padding`, `printf` — mcc says 0, gcc says 1. Honest: mcc implements
  neither, and the `printf` gap is the same one behind the four `*printf-chk` failures.
- `__builtin_memcpy` — mcc says 0 but **does compile and run it**, via a generic
  `__builtin_`-prefix-stripping fallback rather than a token or macro, which
  `pp_has_builtin_arg` cannot see. This is the one real under-report; closing it means
  hooking that fallback in `src/mccgen.c`.
Under-reporting is the safe direction: a false 0 makes a caller take its portable
fallback, where a false 1 makes it emit a call that fails.

### Landed — D1: the default is now `gnu23`

`src/libmcc.c:1161` is `202311` with `std_strict_ansi` still 0, so the dialect is `gnu23`
and `__STDC_VERSION__` is `202311L`. **Full external suite: PASS 16,942 → 16,999 (+57),
FAIL 2,395 → 2,333.** The "58 tests" this file estimated was optimistic-by-luck; +57 net
is the measured figure.

Both blockers this file recorded were real, but the second was **not** one of the 184
failures: this host has `/usr/include/threads.h`, so `__has_include_next` wins and mcc's
fallback body is dead code. It was reproduced only by forcing the fallback path. Fixed
anyway — the `once_flag` typedef now sits behind an inner `#ifndef ONCE_FLAG_INIT`
re-tested *after* `<stdlib.h>` is pulled in, and `call_once` is defined only when mcc
supplied the typedef.

Blocker 1 was fixed by making `bool`/`true`/`false`/`static_assert` genuine keywords
rather than object-like macros — verified against gcc 15.3 first, which treats them as
keywords in every respect (`#if true` → 1, `#ifdef true` → *not defined*,
`defined(true)` → 0, `STR(true)` → `"true"`, `#undef true` → silent no-op). mcc's `-E`
output for the probe is now character-identical to `gcc -std=gnu23`. The same mechanism
also added the three C23 keyword spellings `alignas`/`alignof`/`thread_local`, which the
flip would otherwise have regressed.

**ctest 184 → 47 → 0**, and the 47 were only **4 distinct root causes** (43 were
pipeline-variant duplicates): 2 stale test expectations, 1 harness bug
(`tools/mccharness.c` pinned the gcc/clang reference to `-std=gnu11` while mcc ran at its
default, so the differential was comparing unlike with unlike), and 1 genuine mcc bug —
`stdalign.h` still defined `__alignas_is_defined`/`__alignof_is_defined`, which C23
removed. A fifth genuine bug surfaced separately: `float.h` was missing C23's
`FLT_SNAN`/`DBL_SNAN`/`LDBL_SNAN`/`INFINITY`/`NAN`.

**All 8 external-suite regressions were triaged to zero genuine ones.** Four became
`XPASS` because gcc *also* accepts them under `gnu23` — mcc's old `PASS` was accidental,
"correctly rejecting" for the wrong reason, since xsuite derives `expect=reject` from
`dg-message` even where gcc only warns. `enum-mode-2.c` now *agrees* with gcc, which also
rejects `enum { false, true }` under C23. Two are load flakes whose emitted executables are
byte-identical between the two compilers, and `20050527-1.c` is a pre-existing
nondeterministic VLA-bound segfault, also byte-identical before and after.

Merging it required care: the `errors_and_warnings` golden was edited by *both* this agent
and the D2/D4a severity work, and the D1 agent's base predated D2/D4a — so taking its
golden wholesale would have silently reverted the severity changes. Resolved by keeping the
D2/D4a golden and applying only the one genuine C23 delta (`test_abstract_decls` loses its
`identifier expected`, because unnamed parameters are legal in C23), then re-running to
confirm.

Still missing after the flip, and *not* made worse by it: `nullptr` and `constexpr`, both
needing parser work in `src/mccgen.c`. Their absence is a clean diagnostic rather than
silent misbehaviour, and faking `nullptr` as `((void*)0)` was deliberately rejected — it
would silently pass tests that should fail on the type distinction.

### Closed — `--suite-default-flags` is a near-no-op, and the version that said otherwise was wrong

The harness's `--suite-default-flags` (adding `gcc.dg`'s real `dg.exp` defaults
`-ansi -pedantic-errors`, and `-Wc++-compat` for `c-c++-common`) was left off and
unmeasured because mcc rejected `-ansi`. With `-ansi` landed it could finally be run, and
the first run showed **PASS dropping by 1,432** on `gcc.dg` top level, apparently
revealing a huge C90-conformance gap: 303 *"'X' is a C99 feature"*, 189 *"ISO C90 does not
support"*, 108 mixed declarations, 97 loop-initial declarations, 59 VLAs, 49 compound
literals.

**All of it was a harness bug.** In GCC's DejaGnu, `dg-options` **replaces**
`DEFAULT_CFLAGS`; it does not append to it. `suite_default_flags` applied the defaults to
every top-level file regardless, so 1,531 of the 1,540 regressions (**99%**) were tests
carrying their own `dg-options` that gcc would never have compiled in strict C90 at all.
Fixed by making `suite_default_flags` return nothing when the file contains a
`dg-options`/`dg-additional-options` directive. Re-measured: **+2 PASS, XPASS unchanged**.
So the default board was already right, the 83 apparent XPASS→PASS "gains" were artifacts
too, and this flag is not worth enabling by default.

The lesson generalises to the rest of this file: a flag change that moves a thousand rows
is far more likely to be a harness defect than a compiler discovery, and the cheap check
is to ask what fraction of the moved tests share a single structural property.

### CLOSED — all nine known wrong-code defects

`conversion.c` closed last, and with it the recorder blind spot. `ctest` **8,099 of
8,099**, `-O3` self-host byte-identical fixpoint, `tracegate`/`schemagate` OK.

**The suggested predicate was insufficient and the suggested guard was wrong** — both
worth recording, because this file proposed both.
- Firing `RIR_M_CONVERT` only when the top bit is set still left **36 of 988** differential
  cases failing: narrowing constant casts are the same hole in another guise. The landed
  test is `pre != post` (the fold actually changed bits), which subsumes it *and* is
  narrower in practice — `unsigned i = 0;` has `pre == post` and does not fire, which is
  exactly what regressed the earlier attempt.
- The proposed defensive guard — force `rir_arena_mismatch` when a reconcile sees a
  signedness mismatch — was **measured and rejected**. It cannot see this defect class at
  all (`rir_stamp_sv` bails at `is_float(ct) != is_float(v->type.t)` before reaching any
  signedness comparison), and where it *can* fire it is mostly wrong: **436 hits across 85
  functions** in a single `-O3` self-compile, ~3.7% of all optimized bodies, essentially
  all legitimate. Guard *placement* mattered more than the guard: an initial
  `cwant == rir_shn` check hit 48 of 53 marks because the shadow stack legitimately lags
  the vstack, costing 27 bodies their arena.
- The non-constant arm of the hole (`ds == ss && ds >= 4`) was tested directly with 720
  non-constant cases at every width and both directions: **0 mismatches on baseline and
  fixed**. Not currently live — luck rather than design, but the proposed guard is not the
  instrument that would change that.

**`ast_fconst_reuse` is no longer value-blind.** It now stores a 36-byte key of the
*emitted* bytes (built with the same `write32le`/`write64le`/`write_ldouble` primitives
`init_putv` uses, so `long double` padding never enters it) and refuses reuse on mismatch.
Both call sites are keyed, including the `cplx=1` site a previous agent could not verify —
`cplx_extract_const` fills `re->type`/`im->type` itself, so the source base type is
available without reading `vtop->type.ref`. By construction it can only produce false
*mismatches*, never false matches. Measured: **0 stale rejections** across a full
`-O1/-O2/-O3` self-compile and 4,388 compiles over `tests/` plus all of
`gcc.c-torture/execute`; **234 legitimate reuses still hit**, so the guard is live and
rejects nothing real; and forcing every comparison to fail produced identical correct
output, confirming the fallback is safe. Arena census went `used=2312` → `used=2315` —
three bodies *gained* an arena, none lost.

### Historical — the cluster as first found

**All fixed and merged**, `ctest` 8,099 of 8,099, `-O3` self-host a byte-identical
fixpoint. `pr54877`, `pr117811`, `pr85582-2`, `20000715-1`, `20000412-2` and `pr38615`
all PASS at `-O0` and `-O3`. `conversion.c` remains — its fix needs the recorder work
described below and is in flight.

Two root causes were **not** what this file predicted, and both are worth remembering:

**The float `--` bug was not a sign error.** Nothing negated the constant. `ast_fconst_reuse`
(`src/mccast.c:2166`) is a **positional** replay cache: the record pass appends every float
constant it materializes and, on replay, `gv()` hands back the next recorded entry *in
order, without ever comparing values*. The record pass runs `inc()` (`mccgen.c:5099`),
which pushes `c - TOK_MID` = −1 and interns **−1.0**; the promoted fast path pushed +1
with a compensating `'-'`, its constant was silently replaced by the recorded −1.0, and
the sign applied twice. Proven by pushing `7` and observing the emitted operand was still
−1.0 with no new `.rodata` entry. **That cache is still value-blind** — any future rewrite
that changes which float constants replay emits, or in what order, will silently
substitute the wrong one.

**The aggregate-promotion hypothesis was right but incomplete.** The `AST_OP_ADDR` node's
pointee is literally `VT_VOID` (size 1) — *and* the ADDR node records the converted callee
parameter type while the child `AST_Ref` has already decayed to a plain pointer, so
**neither** carries the object size. `void*`/`char*` parameters fail; `int(*)[4]`, `int*`
and `const S*` all pass.

The `MCC_AST_PROMOTE` leaf test now consults `ast_node_libcall()`, an explicit enumeration
of backend lowerings (`__int128`/`VT_QLONG` mul/div/mod/shift, `long double` arithmetic,
the int↔float conversion helpers, `_Atomic`, non-inlined `__builtin_bswap`/`signbit`/
`ffs`/`clz`, struct stores without `MCC_TARGET_NATIVE_STRUCT_COPY`) rather than
special-casing `__int128` — verified targeted, since a genuine leaf still uses
`%r8/%r9/%r10`.

### Also closed — the optimizer's exponential blowup

`ast_divmagic_*_spow2` tripled its operand subtree per level via three `ast_dup_sub` deep
copies, so a chained `% 2` grew as **3^n**. `n=8` took 5.2s and emitted 296,598 bytes
where gcc emits 1,392 flat. Now **0.003s and 1,606 bytes**, within 18% of gcc and
*smaller* than gcc on `960302-1.c` itself. Separately, `ast_memo_sync` memset **all five**
memo tables on every epoch change — and `epoch++` fires on essentially every mutation, so
each query inside a mutate-then-query pass cost O(nodes)×5; now per-node epoch stamping,
O(1) per query. Measured independently on the *unfixed* exponential AST, object sizes
bit-for-bit unchanged, which is the clean proof the two defects were independent.
The `ast_dup_sub` audit found the reported case was **not the worst**: `int / 7` grew
×3.99 per operator (901,382 bytes at n=7). i386 already had the right answer —
`ast_divmagic_materialize` binds `x` to a stack temp; the other targets took the
`ast_dup_sub` branch. The landed fix is a *refusal*, not the single-evaluation lowering;
making `ast_divmagic_materialize` target-independent is the principled follow-up, deferred
because `ast_ident_pure` admits non-volatile loads, so hoisting the operand's store can
move a load across a guarding branch.

### Historical — the miscompile inventory as first found

**`double d = 10; d--;` yields 11 at `-O2` and `-O3`.** Correct at `-O0`/`-O1`, correct
under gcc. Reproduced by hand in the merged tree. Floating-point decrement is being
compiled as increment: the promoted inc/dec fast path at `src/mccast.c:4764-4784` pushes
constant `1` and varies the opcode (`gen_op(TOK_INC ? '+' : '-')`), but the constant is
materialized as `-1.0` while the subtract opcode is kept, so the sign is applied twice
(`subsd` of `-1.0`). Gate `MCC_AST_PROMO_INCDEC=0` masks it. The convention used
elsewhere in this compiler (`src/mccgen.c:5078`, `:5099`) is to push `c - TOK_MID` and
always `gen_op('+')`.

Seven confirmed wrong-code defects at `-O2`+, each isolated to a single env gate by
bisection and confirmed against generated assembly. Two verified by hand in the merged
tree (marked ✓). **`-O2` is not trustworthy until these close**, which is why feature
work is queued behind them.

| Defect | Gate | Site | Repro |
|---|---|---|---|
| ✓ FP `--` compiles as `++` | `MCC_AST_PROMO_INCDEC` | `mccast.c:4764-4784` | `double d=10; d--;` → 11 |
| ✓ cprop survives a side-effecting `if` condition | `MCC_AST_CPROP_JOIN` | `mccast.c:7488-7492` | `int y=2; if(y++!=2){} if(y!=3) BUG` |
| Promotion ignores backend-synthesized libcalls | `MCC_AST_PROMOTE` | `mccast.c:3435-3438`, `:3601-3602` | `__int128 x<<(y&5)` → 4 not 12 |
| Promotion of address-taken aggregate | `MCC_AST_PROMOTE` | `mccast.c:3495-3520` | `pr117811.c`; two elements share `%ebx`, never stored |
| `__func__` merged with an identical string literal | `MCC_MERGE_STRINGS` | `mccgen.c:15410-15419` | `"main" == __func__` true; C99 6.4.2.2 requires distinct objects |
| Tail call reuses a frame whose local's address escapes | `MCC_AST_TCO_PTR` | `mccast.c:2060` (gate; transform site not located) | `pr38615.c` |
| `20000412-2.c`, `conversion.c` | not yet isolated | — | the two original `-O3` aborts |

**Two more, root-caused after the table above was written.**

**`conversion.c` — the replay recorder is blind to no-code casts.** `(double)(int)~((~0U)>>1)`
prints `2147483648.0` at `-O1`+ and `-2147483648.0` at `-O0` and under gcc. `gen_cast`
(`src/mccgen.c:4425-4487`) folds an integer cast of a constant by rewriting `vtop->type`
and emitting **no code** (`done:` at `:4670`). The replay-IR recorder builds the
optimizer's arena from the captured *codegen op stream*, so a cast that emits nothing is
invisible: `rir_stamp_sv` (`src/mccrir.c:1794`) only inserts an `AST_Convert` when a type
*shrinks* (`:1860`), and its reconcile points fire only at emitted ops. The replayed tree
therefore carries `Binary ^` typed *unsigned*, and the later `(double)` takes the unsigned
branch at `mccgen.c:4438` instead of the signed one at `:4441`. Confirmed under gdb; both
constants are in `.rodata` with the wrong one selected. The faithfulness check compares
*code bytes*, which are identical, so the semantically-wrong body is accepted — that is
why nothing caught it. `MCC_REPLAY_IR=1` masks the bug entirely
(`rir_prod_env = ast_replay_env && !rir_env`, `src/mccrir.c:534`), which matters for
future bisects. **The general hole outlives any point fix**: *any* no-code type change in
`gen_cast` is invisible to the recorder, including the `ds == ss && ds >= 4` path at
`mccgen.c:4573`.

**Invalid TCO is wider than the `MCC_AST_TCO_PTR` gate suggests.** `ast_tco_run`
(`src/mccast.c:7949`) checks escapes on **parameter slots only** (`:7979-7981`), never on
other locals whose address reaches the callee. After frame reuse the caller and callee
share one object. `int f(int a,int *y){int x=a; if(!a) return *y; return f(a-1,&x);}`
returns 0 at `-O2` and 1 everywhere else; the `G=&x` variant is wrong even with
`MCC_AST_TCO_PTR=0`, so the gate does not bound the defect.

Two of these share the `MCC_AST_PROMOTE` family but are distinct defects. The cprop one
is the broadest in blast radius — *any* `if` whose condition mutates a local corrupts the
lattice at `-O2`+ — and the fix pattern already exists in the non-join sibling
`ast_cprop_block`, which does an unconditional `ast_cprop_kn = 0` at `mccast.c:7282`.

Also classified, and **not** bugs: `990208-1.c` (inliner is expression-body-only,
`mccast.c:8539-8568`), `bcp-1.c` and `builtin-constant.c` (`__builtin_constant_p` is
resolved at *parse* time from value-stack flags at `mccgen.c:11277`, so it can never
answer 1 for a constant-propagated local — always standard-conforming, just weak), and
the four `*printf-chk` tests (mcc has no `__*_chk` builtin knowledge at all; note gcc
itself also fails `vfprintf-chk-1` on this machine, so that one is not a valid
expectation here).

### Board after the first six fixes

19,571 tests/column, **`-O0` 85.8%, `-O3` 85.7%, ICE 0, TIMEOUT 0, XPASS 618** (from
4 ICE / 4 TIMEOUT / 1,013 XPASS). The ICE and timeout columns are now empty for the
first time. Remember the rate is not comparable to the 82.6% board — see the denominator
note above.

### Decisions taken 2026-08-03 — direction for the fix program

The owner's standing principle for all of it: **mimic gcc and clang as closely as
possible; where they disagree or the case is ambiguous, fall back to sane and safe
behaviour and record the choice.** Apply this rather than inventing mcc-specific
semantics.

| | Decision | Where |
|---|---|---|
| D1 | **Default `-std` becomes `gnu23`**, from C11. mcc already implements C23 unnamed parameters, `(...)`-only prototypes and 1-arg `static_assert` — verified by hand, they are gated off, not missing. 58 tests, one line. | `src/libmcc.c:1161` |
| D2 | **Conversion diagnostics become errors** — `incompatible-pointer-types`, `int-conversion`, `implicit-int`, matching gcc 14+/clang 16+ and C23. Measured self-host-safe; gcc `-Werror=` over all 87 TUs gives zero errors. 29 tests. | `src/mccgen.c:4797`, `src/libmcc.c:1153-1180` |
| D3 | **`warn_unsupported` defaults on** — unknown attributes, options and linker options warn, as gcc's `-Wattributes` does. The bit must be **split first**: it also gates four ignored-assembler-directive warnings in `src/mccasm.c`. 39 tests. | `src/libmcc.c:1153-1180`, `:3266` |
| D4 | **Relax where mcc is stricter than gcc** — excess initializers and `return;` in a non-void function are hard errors in mcc and warnings in gcc/clang. 28 tests. | `src/mccgen.c:14312`, `:14322`, `src/libmcc.c:1168` |
| D6 | **Implement the three dropped attributes properly** rather than diagnosing them: `scalar_storage_order` (real byte-swapping), `mode(byte)`/`packed` on enums (enum sizing), and `ms_abi` (the Microsoft calling convention). Today all three are parsed and silently ignored, so mcc emits wrong data and wrong calls with no diagnostic at any warning level — verified against gcc both ways. This is the most dangerous item on the board because mcc's objects are meant to link against gcc's. | attribute handling `src/mccgen.c:5402`, struct layout, x86_64 call lowering |
| D5 | **All four feature areas are in scope**, sequenced by cost: GNU builtins long tail (205 tests, ~100 names, mechanical), C23 small spellings (74 — `nullptr` 21, `[[attr]]` after declarator 16, `alignas`/`alignof`/`thread_local` 13, scalar empty init 13, `constexpr` 11), x86 intrinsic headers (103), fixed-point `_Fract`/`_Accum` (66). | mixed |

**Sequencing constraint, and why the program is not all parallel.** Nearly every item
above lands in `src/mccgen.c` (16k lines) or `src/libmcc.c`, so the work serialises on
those two files rather than on ideas. Wave 1 (in flight): the 128-bit division
slowdown in `runtime/lib/int128.c`, the four crash bugs in `src/mccgen.c`, `-ansi` plus
unknown-option prefix matching in `src/libmcc.c`, harness accuracy in `tools/xsuite.py`,
and the x86 intrinsic headers (mostly new files under `include/`). D1-D4 and D6 and the
remaining features queue behind whichever wave-1 agent owns their file.

### Landed in the working tree 2026-08-03, gated together

`ctest` **8,098 of 8,098** (8,097 baseline plus the new `#embed` regression test), `-O3`
self-host clean, `tracegate` and `schemagate` both OK. Nothing committed.

1. **`#embed` device files and unbounded reads** — see the section below.
2. **128-bit division was ~190x slower than gcc's.** `runtime/lib/int128.c` ran an
   unconditional 128-iteration bit-serial loop for every divide, with no fast paths, and
   all of `__udivti3`/`__umodti3`/`__divti3`/`__modti3`/`__udivmodti4` funnel through it.
   Added the compiler-rt prologue: both-halves-64-bit → one hardware divide;
   `d.high == 0` → Knuth Algorithm D on 32-bit limbs (this is the path the failing tests
   hit — 2^127-scale dividends with small divisors, so the naive "both fit in 64 bits"
   check is *not* sufficient); genuinely-128-bit divisors keep the original loop.
   Divide-by-zero deliberately still falls through to the old loop, because the old code
   returns `q = ~0, r = n` rather than trapping and a hardware `/0` in the fast path
   would have turned that into SIGFPE. Verified by 2,479,138 differential checks against
   host libgcc, **0 failures before and after**, including `INT128_MIN / -1`.
   **`gcc.dg/pr97459-{2,4,5,6}.c`: 1.6-3.3s → 0.07-0.16s**, so the four runtime timeouts
   on the board are gone. Microbenchmark 286ms → 14.8ms; the gap to gcc closed from 151x
   to 7.8x, the residue being call overhead rather than the algorithm.
3. **Five preprocessor defects**, all measured: digraphs were folded to their primaries
   at lex time so spelling was unrecoverable and they were wrongly accepted in strict
   C90 (now carried as distinct `TOK_DIG_*` tokens, converted only when not preprocessing);
   `#` stringification octal-escaped control characters and failed to escape `"` and `\`;
   pending whitespace was lost across a nested macro expansion; pp-number swallowed
   `p+`/`p-` in strict C90; and multi-character constants accumulated *signed* bytes, so
   `'\234b'` read `-25502` instead of `40034`. **`gcc.dg/cpp` +7 with zero regressions**
   across all 541 runnable tests in that suite.

4. **Four compiler crashes.** Unbounded parser recursion through `unary()` was a plain
   SIGSEGV on deeply nested parens (`limits-exprparen.c`); the body is now
   `unary_nested()` behind a counting wrapper, so the "decrement on every exit path"
   requirement holds structurally rather than by inspection — the body has an early
   `return` plus dozens of `break`s. Because `mcc_error` longjmps rather than truly not
   returning, the depth also resets per translation unit. **2,047 nested parens accepted,
   2,048 a clean diagnostic** (measured). Plus: calling a function with an incomplete
   return type was silent and segfaulted (no sret pointer); pointer *subtraction* on an
   empty aggregate divided by `sizeof == 0` and raised SIGFPE (`p + 1` still works, as in
   gcc); and `va_start` in a fixed-argument function was silent and segfaulted.
   **The `va_start` plan in this file was wrong** — un-`#ifdef`-ing
   `check_va_start_last_param` would have been a no-op twice over: on x86_64 SysV
   `__builtin_va_start` is a *macro* in `runtime/include/mccdefs.h:499`, not a builtin, so
   no `case` is ever reached; and the check guards on `cur_func_last_param`, which is only
   set when `func_var` is true, so it structurally cannot fire in a fixed-arg function.
   Fixed by routing x86_64 SysV through mccgen like the other three targets, which also
   revived the previously-dead `-Wvarargs` on this target.
5. **Driver options.** `-ansi` is now accepted as `-std=c90` — verified byte-identical
   predefines to `-std=c90`, and it does *not* imply `-pedantic`, matching gcc. This
   matters far beyond the flag: `gcc.dg/dg.exp:25` puts `-ansi` on the default command
   line, so mcc was failing files before reading them. Unknown options now warn instead of
   being swallowed: `-veryodd` previously **enabled verbose mode** via prefix matching.
   The `warn_unsupported` bit was **split** rather than flipped — a new
   `warn_unsupported_option` defaults on, while the old bit still gates unknown attributes
   and the four ignored-directive warnings in `src/mccasm.c`, which is the decision this
   file records as needing to be made separately.
6. **Harness accuracy** (`tools/xsuite.py`), with three of the briefed counts corrected on
   measurement: `dg-message` affects **281 files, not ~10** (mcc was scored XPASS for
   correctly exiting 0); `gcc.dg/special/` must **not** be directory-skipped because
   `special.exp` globs `*[1-9].c` and 4 of the files are real passing tests, so only the 5
   auxiliary halves are skipped; and `dg.exp`'s defaults apply to `gcc.dg` **top level
   only** (5,910 files), not the subtree. Board **20,513 @ 82.6% → 19,551 @ 85.8%**, of
   which **+2.07 pts is pure denominator and only +1.16 pts is real** — absolute PASS
   *fell* by 323, because 776 of the 1,924 skipped artifact rows were passing. `llvm:clang`
   moved *down* 0.1 pts, which is the evidence this was not number-pumping. ICEs 4 → 1.

7. **Runtime math and the SysV varargs ABI.** `__builtin_fabs` was
   `(x) <= 0 ? -x : x` — false for NaN, so the sign survived, and `copysign` was built on
   it and inverted with it; now uses the native `__builtin_signbit`. Complex division was
   unscaled Smith, annihilating the `a*d` term when `d/c` underflows; now a line-for-line
   port of libgcc's Baudin-Smith prologue. `__builtin_{add,sub,mul}_overflow` coerced both
   operands to the *result* type's signedness, so the stored value was always right and
   the **flag was wrong**; now sign+magnitude in each operand's own type with the result
   computed in infinite precision. And `classify_x86_64_va_arg`
   (`arch/x86_64/x86_64-gen.c:1295`) never consulted `x86_64_mixed_class`, so a mixed
   INTEGER+SSE eightbyte was copied wholesale from the GP save area **and desynchronized
   the rest of the `va_list`**, corrupting every later `va_arg` in the call.
   A **second, caller-side** bug was found while fixing it: `gfunc_call`'s mixed-class path
   at `:1711` built an `SValue` with `c.i + 8`, but `gen_modrm_impl` ignores the
   displacement for a plain register-indirect lvalue, so *both* eightbytes loaded from
   offset 0 whenever the struct came from a pointer or a spilled lvalue.
   Differential vs gcc 15.3: fabs/copysign 14 wrong → **0** of 838; overflow builtins
   (exhaustive 256×256 × 4 signedness × 3 ops × 8 result types) all 5 digests wrong →
   **0** of 6,373,015; varargs mixed-class 11 wrong → **0**, including cross-ABI
   gcc↔mcc caller/callee combinations. Complex division 14,464 wrong → 604, and those 604
   are **not a defect**: this machine's libgcc built `__divdc3` with FMA, and compiling the
   same ported source with `gcc -mfma -ffp-contract=fast` gives 0 of 38,416 — identical
   algorithm, different fused rounding. Named external tests **70 → 78 passing**.
   Still open, deliberately not attempted: `pr92904.c`'s last 2 checks are a *different*
   defect — 32-byte-aligned aggregates in the varargs overflow area, isolated by a split
   build to mcc's **caller** side (`gfunc_call` handles only `align == 16`); fixing it
   needs dynamic `rsp` over-alignment, a call-sequence change. And `__int128` result types
   still route to `runtime/lib/int128.c`'s helpers, which retain the old operand-signedness
   coercion.

**Correction to the `-O0 FAILEXE` figure this file quotes.** The "153 at `-O0`" is not
153 `-O0` defects. `tools/xsuite.py:292` builds `[mcc] + [opt] + flags`, so a test's own
`dg-options "-O2"` lands *after* the harness `-O0` and wins — **76 of the 153 ran at their
own level**. Re-run with `--force-opt`, 151 do still fail at true `-O0`. But compiling all
153 with real gcc 15.3 at `-O0` gives **68 pass, 71 fail, 14 unbuildable**, so a gcc-`-O0`
failure means the test needs an optimizer, not that mcc is wrong. **68 is the lower bound
on genuine defects, not 153.** The largest single lever in the set is
`__builtin_object_size` (51 tests, stubbed to `-1`/`0` at `src/mccgen.c:11386-11393`) —
but gcc itself only passes 4 of those 51 at `-O0`, so a front-end-only implementation is
worth ~5 tests and the rest need the optimizer.

### Closed — `#embed` read the whole file, and trusted `lseek` to say how big it was

`clang/test/Preprocessor/embed-reject-device-files-lin.c` was an `XPASS`: mcc accepted
`#embed "/dev/urandom"`, `/dev/random`, `/dev/zero` and `/dev/null` and built empty
arrays, where clang rejects all four. It never hung *here* — `lseek(SEEK_END)` returns
0 on all four devices on this kernel, so mcc read nothing — but the acceptance was one
kernel's `lseek` away from an unbounded read, and the same function had a second,
reachable memory-exhaustion path.

`embed_read_file` (`src/mccpp.c:1700`) took the size from `lseek(SEEK_END)` and did a
single `mcc_malloc(size)` plus one `read()`. Measured with a 64 TiB sparse file: mcc
attempts the whole allocation, dying in `mcc: memory full` under an `RLIMIT_AS` cap and
faulting in pages until RAM is gone without one. It also read the *entire* file before
applying `limit(N)`, so `#embed "big.bin" limit(8)` paid for all of `big.bin`.

Now: `fstat` replaces `lseek`, non-regular files are refused with clang's own wording
(`device files are not yet supported by '#embed' directive`), the read is incremental
and partial-read-safe, and the resolver stops searching the embed path once a candidate
is rejected rather than falling through to *not found*. `embed_want` passes
`offset + limit` down so only the bytes actually used are allocated — the 64 TiB file
with `limit(8)` now compiles instantly. Anything over **1 GiB** (`EMBED_MAX_SIZE`) is a
named diagnostic instead of an allocation; that ceiling is a policy choice, not a
measurement, and is one constant if it is wrong.

`__has_embed` deliberately did **not** become fatal: clang reports a device as *found*,
so `has_embed_test` returns found for a non-regular file without reading it. Verified
against clang both ways.

Gates: `ctest` 8,096 of 8,096 with the `-O3` self-host re-taken and clean; all 94
`#embed` tests across both external trees re-run, **exactly one status change** and it
is the intended `XPASS → PASS`. Regression test at
`tests/diagnostics/dg-error/embed_device_file_lin.c`; the `dg-error` glob now skips
`*_lin` cases off POSIX hosts, since that tier runs mcc on the host and `/dev` is a
host path.

Three `#embed` `XPASS`es remain, all diagnostics rather than memory: `gcc.dg/cpp/embed-2.c`
(`#embed` before C23 should be a pedantic error under `-std=c17 -pedantic-errors`) and
`embed-{6,7}.c` (under `-fpreprocessed` gcc requires the `gnu::base64` parameter).

### Phase 1 — the `-O0` defects the sweep found, and what closed

The board above reads `-O0` and `-O3` as the harness ran them, but a test's own
`dg-options "-O2"` overrides the column: 111 of the 142 baseline failures are
genuine `-O0` defects, the rest only fail with the optimizer the test asked for.
`tools/xsuite.py --force-opt` strips the test's `-O` and re-runs at the column's
level; that is the list phase 1 works from.

**Closed, with `ctest` at 8096 of 8096 and the `-O3` self-host fixpoint still byte-identical:**

1. `__attribute__((aligned(N)))` on a **member** was replacing the natural alignment instead of only raising it. GCC lowers alignment only when `packed` is also present. `gcc.dg/align-1.c`, `c-c++-common/attr-aligned-1.c`, and `gcc.dg/bf-ms-layout-2.c`.
2. The same attribute on a **typedef** *may* lower alignment — the kernel's `unaligned_u64` idiom — and the naive form of fix 1 breaks it. The two cases are now distinguished by a new `SymAttr.type_aligned` bit set in `sym_to_attr` and cleared by an explicit declarator attribute. `tests/diff/parts/legacy_expr.h`'s `aligntest9` is the regression that catches this.
3. `__alignof__` applied to a **typedef name** dropped the typedef's alignment entirely (`__alignof__(T)` read 8 where the type was `aligned(16)`). The type-name branch of `unary` now carries the parsed alignment out through `gen_sizeof_parsed_align`.
4. A union whose only member is a zero-width bitfield took that member's alignment; GCC gives it 1. `gcc.dg/empty1.c`.
5. Eight missing `__SIZEOF_*` predefines — `SHORT`, `FLOAT`, `DOUBLE`, `LONG_DOUBLE`, `SIZE_T`, `PTRDIFF_T`, `WCHAR_T`, `WINT_T`. Tests gate whole bodies on these, so the absence read as a silent pass-with-nothing-executed rather than an error. `gcc.dg/torture/pr58416.c`.
6. `__INCLUDE_LEVEL__` was undefined. `gcc.dg/cpp/strify2.c`.
7. `//` is not a comment in strict ISO C89/C90 — `1 //**/ 2` is `1/2` there, and mcc read it as `1`. Gated on `std_strict_ansi`, so `-std=gnu89` keeps the extension. `gcc.dg/cpp/pr61854-1.c`, `-5.c`.

**Converted from silent wrong code to a diagnostic:** a struct or union member
with a variable-length array type. mcc accepted `struct S { int a[n]; }`, computed
a *static* size for it (7 read as 8, and `{int a; char b[n]; int c;}` read as 16
with `c` overlapping `b`), and the resulting binaries segfaulted or corrupted the
frame. Eleven tests were failing this way — `20040423-1`, `20041218-2`, `pr41935`,
`pr82210`, `gnu23-empty-init-1`, `packed-vla`, `vla-stexp-*`, `typename-vla-*`,
`pr99122-2`. They now fail to compile with a named error instead of building a
wrong program. `tests/cli/cases.h`'s `pedantic_diagnostics` case banked the old
accept-and-miscompile behaviour and moves with it.

**The feature that replaces that diagnostic** is runtime-sized structs, and it is
the largest single item the sweep found: the member's size, every offset after it,
the struct's own size and any copy of it all become runtime expressions. The
machinery for the scalar case already exists — `vpush_type_size` reads a VLA's
size out of a frame slot (`type->ref->c`) — so the shape of the work is to give a
struct carrying a VLA member the same treatment, plus `alloca` at declaration and
runtime offsets in member access. Nineteen tests ride on it, `pr51990`/`pr99122-1`
among them, and it is phase 2's first entry.

**Still open at `-O0`, largest clusters first:** `hardbool` (14, a GCC 14 type
attribute), preprocessor conformance (~10 — digraph spelling must survive
stringification, C90 pp-number rules, comments inside skipped groups),
`__builtin_object_size` and its dynamic form (7), `scalar_storage_order` (4),
`_Complex` division checks (5), and the varargs ABI for over-aligned and
`__int128` arguments (`pr92904`, `stdarg-3`).

### Phase 3 — the `-O3` column, and the four defects behind it

The `-O3`-only list was 46 tests. Five were confirmed by hand as wrong code or
a compiler crash; the causes turned out to be four, and closing them took the
list to **30**, the `-O3` ICE count from 10 to 5, and the runtime-abort count in
`c-torture/execute` from 30 to 23. Re-measured over the whole tree from this
commit: **20,513 tests per column, 76.4% at `-O0` and 76.3% at `-O3`** — the two
columns are now within 28 tests of each other, against 39 before.

1. **`ast_sym_defer` chained deferred syms through `sym->next`** — the same
   field that holds a struct's member list. Deferring a function-local struct
   type turned its field chain into the deferred list, and
   `x86_64_has_unaligned_field` then walked it until the stack ran out. The
   trigger is ordinary: any function-local aggregate passed by value to a call,
   because the replay runs at `ast_func_end`, after the body's scope has been
   popped. `930503-2`, `930530-1`, `pr26213`, and `pr68506`'s `vstack leak` ICE
   and `medce-1`'s wrong code fall out of it too. The deferred set is a plain
   array now and leaves `sym->next` alone. This one was live at every `-O` level
   and on both arenas — `MCC_RIR_PROD=0` did not move it.
2. **`AST_FB_CMP_INVERT_LATE` was applied only to a live comparison.** When both
   operands fold to constants there are no flags to swap and the inversion was
   dropped, so `!(f < 0)` with `f` known zero emitted **0**. `origin/main`'s own
   fix for this class (run the opposite operator, then swap) is the right shape
   and is what arm64's instruction pairing needs, but it leaves the folded case
   open; the merge keeps both halves. `pr110954-1`.
3. **`value64` truncates to 32 bits for every type narrower than a long long.**
   That is what the parser's folding wants, and it makes the function useless as
   a *does this constant fit in `signed char`* test — which is exactly what the
   narrowing pass asked it. It answered yes for -1634678893 and eliminated the
   `(signed char)` conversion. `ast_ii_cval_fits` does the real
   width-and-signedness check; both narrow paths use it. `pr70941`, `pr81814`,
   `20001009-1`.
4. **A call whose target lost its function type** dereferenced a NULL ref in
   `gfunc_call`. The replay already runs under an error sink with a byte-compare
   fallback, so raising the error instead lands in the designed path and keeps
   the parser's bytes. `Wsequence-point-2`.

**Reverted, and worth recording as a trap**: answering `__builtin_object_size`
subobject queries from the argument's declared array type. It matched gcc on the
plain cases and closed `pr101836_1`, but a trailing array's answer depends on
whether the member is flexible, not on its declared bound — the whole-tree
re-run turned up five tests that went from passing to aborting against the one
gained. `-1` is a permitted answer and a safe one. A real implementation needs
the flex-array rules.

**Still open at `-O3`**, from the re-run: `20000412-2` and `conversion` (both
`va_arg` of a variably-sized type), `990208-1` (a label address inside a
function the inliner takes), `pr85582-2`, `20000715-1`, `pr54877`, and the
`link_error` idiom in `bcp-1`/`pure-1`/`compare-3`, which is optimizer quality
rather than a defect — those tests require a fold `-O0` already performs and
`-O3`'s inlining undoes.

## Vector types

`__attribute__((vector_size(N)))` is implemented, on the shape `_Complex`
already established: an anonymous struct of N/sizeof(elem) fields carrying
`SymAttr.is_vector`, with the struct's alignment set to the vector's size.
Everything the aggregate machinery already does — `sizeof`, `_Alignof`, brace
initialization, assignment, parameter passing, return, compound literals —
comes for free and needed no new code. The operations are lowered element-wise
against a stack temp, which is correct but scalar: there is no SIMD codegen
behind this, and the ABI is the one mcc gives a struct of that size, not the
SysV vector classification, so a vector crossing a translation-unit boundary to
gcc-compiled code is not yet ABI-compatible.

What is covered: element-wise `+ - * / % & | ^ << >>` and unary `- ~`; the six
comparisons, which yield a signed-integer vector of the same width with -1 for
true as GCC does; scalar broadcast on either side; subscripting as both rvalue
and assignment target; casts between vectors of the same size and between a
vector and a same-size scalar; `__builtin_shuffle` with a one- or two-vector
source and a *runtime* mask; `__builtin_shufflevector` with constant indices;
and `__builtin_convertvector`.

Two type-system decisions worth recording. Vector types are **interned** by
(element type, count) in `gen_vector_type_cache` — without that, the type minted
for a comparison result is a distinct anonymous struct from the operand's and
every `v4si e = a == b;` fails as an incompatible initializer. And
`compare_types` treats two vectors of the same size and element count as
compatible, which is GCC's `-flax-vector-conversions` rather than its default;
the strict rule needs the element type carried in the comparison result, which
is the natural next step.

**The board**: of the 914 runs (457 tests × two columns) that the sweep had
blocked on `vector_size`, **777 pass**. The remaining 134 are a long tail —
30 parse shapes, 16 conversions, 16 other builtins (`__builtin_reduce_*`, the
`ia32` intrinsics), 10 that assign to a read-only location, 8 `_Complex`
element types, and 4 `__mode__(vector_size)`.

Two guards came out of this work rather than the feature: `||`/`&&`/`?:` on a
vector reached `gvtst` and crashed on a bogus jump chain, and now says *used
vector type where a scalar is required* as GCC does; and a vector whose element
count exceeds 1024 is refused rather than materialized, because the struct
representation makes `vector_size(1 << 29)` into 134 million field syms
(`gcc.dg/pr69973.c` hung the compiler before the cap).

### The baseline after vectors, and the computed-goto regression the re-run caught

Re-measured over the whole tree with the self-hosted `-O3` compiler:
**20,513 tests per column, 78.3% at `-O0` and 78.1% at `-O3`**, up from 76.4/76.3
before vector types. 778 runs moved from failing to passing; the `vector-ext`
failure bucket went from 914 to 8.

31 runs moved the other way and they split three ways. Twenty are **`XPASS`** —
tests that assert GCC rejects an invalid vector conversion, which mcc now
accepts because `compare_types` takes the `-flax-vector-conversions` rule. That
is the documented cost of the lax rule and it is a missing diagnostic, not wrong
code. Four are **timeouts under load** that pass when run alone. The remaining
seven were a real regression, and the re-run is the only reason it was found.

**Taking a label's address disabled nothing, so register promotion ran on the
body and the label definitions vanished from the output.** `goto *tab[n]` then
jumped into code that was never emitted, and the program hung. It reproduces in
nine lines:

```c
int f(int n) {
  static const void *tab[] = {&&a, &&b};
  int r = 0;
  goto *tab[n];
a: r = 10; goto done;
b: r = 20; goto done;
done: return r;
}
```

`-O0` and `-O1` print `10 20`; `-O2` and `-O3` hang. Bisected to the P5 merge —
`1d7963ac` passes, `1dce997a` does not — and it is not the deletion itself but
what the deletion exposed: with the recorder gone, nothing declines a body that
takes a label's address, and `ast_plan_promotion`'s guard only knew about
`ast_func_has_asm`. `ast_func_has_labeladdr` is set where `&&label` is parsed and
joins that guard. `920302-1`, `20040302-1`, `20050527-1` are closed; `990208-1`
still fails, and that one is the inliner rather than promotion — it is a label
address inside a function the inliner takes, which is the pre-existing entry in
the `-O3` list.

The lesson worth keeping: the P5 board was green on `ctest` 8073 of 8073 through
the whole merge. The external suites are what read the difference, and only
because the sweep was re-run over the whole tree rather than over the tests that
had failed before.

## The eight-cluster sweep over gcc and clang

The external board, re-measured over the whole tree from this commit:
**20,513 tests per column, 82.6% at `-O0` and 82.4% at `-O3`**, from 78.3/78.1.
**1,965 runs moved from failing to passing.** Against that, 198 rows moved to
`XPASS` — tests asserting a diagnostic mcc now doesn't emit, a consequence of
the lax vector-conversion rule and the loosened function-compatibility check —
and exactly one file regressed, `builtin-issignaling-1.c`, which now reaches the
link and stops on a builtin mcc does not have.

The work was split into eight disjoint clusters, each taken by an agent in its
own worktree with the same two hard gates (`ctest` clean, `-O3` self-host
fixpoint byte-identical) and a standing rule that a test is never weakened and
the harness is never edited.

| cluster | gained | the lever |
|---|---:|---|
| parse / C23 syntax | 382 | `[[...]]` attribute-specifier-sequences; ~225 files were blocked on a *missing predefine*, not on syntax |
| missing `__builtin_*` | 299 | `runtime/include/mccdefs.h` already maps builtins onto libc symbols by asm label — most were simply never added to that table |
| `_Complex` | 47 | integer complex, and GCC's overflow-avoiding wide/ratio division, which it applies to integer complex too |
| semantic rejections | 49 | `__extension__` was parsed and then ignored, so it never suppressed anything |
| `#embed` | 20 | full C23 form including `limit`/`prefix`/`suffix`/`if_empty` and `__has_embed` |
| VLA struct members | 42 | see below |
| missing diagnostics | 18 | `is_compatible_func` never looked at the other side's parameter list |
| inline asm | ~35 | flag-output operands (`=@ccCOND`), the missing constraint letters, `xgetbv` |
| headers | 4 | 202 of the remaining 249 want another architecture's intrinsics |

**Three findings worth keeping.**

*The VLA-member miscompile was not in the new code.* The first version of
runtime-sized structs passed its own 79-file cluster at `-O0` and miscompiled at
`-O1`: after a loop wrote through the VLA member, reading the member at offset 0
returned the array's first four bytes. The cause was a **pre-existing** REGDISP
fast path in member access that defers a field's constant offset to be folded
into a later addressing mode, guarded on *the field is not an array*. No
ordinary member carries `VT_VLA`, so that guard had never seen one; the deferred
offset was dropped rather than folded. It was reverted, diagnosed properly, and
re-landed. The lesson is the testing one: validating a codegen feature at `-O0`
alone is not validation.

*Three regressions only the combined tree could show.* Each agent's branch was
green on its own and two of them still broke things together — a
function-compatibility rewrite that was too strict in two directions, an
`__extension__` unget that left `decl()` looking at the wrong token, and a
`__BITINT_MAXWIDTH__` predefine advertising a `_BitInt` mcc cannot parse.
Per-cluster gates do not compose; only the whole-tree re-run reads them.

*The harness understates mcc.* Three agents independently found that
`tools/xsuite.py`'s `KEEP_OPT_RE` drops flags the tests depend on — `-Werror`,
`-idirafter`, `--embed-dir` — and never expands `${srcdir}`. Whole families
(`gcc.dg/spellcheck-options-*`, the `Werror-*` set) are unpassable for that
reason alone, and several `#embed` tests fail only because the flag never
arrives. That is a measurement bug in our tooling, not a gap in the compiler,
and it is the next thing to fix before the board is quoted again.

## Verification pass — `da3a461b`, 2026-08-03

**Read this first: the tree moved 31 commits under this pass.** Everything below was
measured against `da3a461b`; `origin/main` reached `38d3f650` while it was running,
bringing VLA struct members, C23 attributes, `#embed`, integer `_Complex` and the
eight-cluster sweep above — **+850 lines of `src/mccgen.c`** and another **+193 of
`runtime/include/mccdefs.h`**. So:

- **The structural findings hold** — `src/mccast.c` and `src/mccrir.c` were not
  touched by any of the 31 commits, so the deletion inventory, the deletion residue,
  the `ir_`/`IR_` namespace count and every `mccast.c`/`mccrir.c` line number below
  are still exact. The `mcc.h`, `mccgen.c` and `libmcc.c` references have been
  re-resolved against the merged tree.
- **The methodological findings hold** — the low board figure is still the
  `C2_NO_EXTRA=1` board and not the `all` board, whatever the numbers now are. The
  `-O0` bank *was* stale and doubly so, since `mccdefs.h` grew a second time; it was
  re-taken in `bc85ce70` and URGENT item 1 records how the shift was proved cosmetic
  first.
- **Every number is provisional**: `ctest` 8254, the `fn` counts and the `c2err`
  attribution were all taken before the merge. **Re-run before quoting any of them** —
  which is what this file has said all along. The 194/104 boards have since been
  re-run at `bc85ce70` and read **201/111**; see **Scoreboard**.

Every mechanically checkable statement in this file was re-run against the tree at
`da3a461b` in a fresh build (`cmake -S . -B cmake-verify -G Ninja
-DCMAKE_BUILD_TYPE=Debug -DMCC_ENABLE_CROSS=ON -DCMAKE_C_FLAGS=-DMCC_REPLAY_IR_C2=1`)
on the short path `/home/llg/Projects/moderncc`, with `vendor/` present and all five
ELF sysroots resolving. What follows is the result, so the next person diffs against
a measurement and not a memory.

### Line references — 28 of 30 were stale, here are the current ones

The deletion moved 1,618 lines out of `mccast.c` and the feature commits moved more.
Only `src/libmcc.c:651` and `:825` still point where this file says they do.

| this file says | actually at | what it is |
| --- | --- | --- |
| `mccast.c:2035` | **`mccast.c:1923`** | `ast_replay_env` (and `MCC_RIR_FORCE` at `:1925`) |
| `mccast.c:2040` | **gone** | `MCC_RIR_ONLY` — deleted, the switch no longer exists |
| `mccast.c:2060` | **`mccast.c:1942`** | `ast_cycle_env` |
| `mccast.c:2279` | **`mccast.c:2139`** | `ast_fconst_reuse` |
| `mccast.c:2449` | **`mccast.c:2449`** ✓ | `ast_fn_inlinable` — correct |
| `mccast.c:2828` | **`mccast.c:2828`** ✓ | `ast_reemit_retain` — correct |
| `mccast.c:2516` / `:2895` | **`:2449` / `:2828`** | duplicate references to the same two functions, both one revision old |
| `mccast.c:5117` / `:5138` | **`mccast.c:3413`** | `ast_plan_promotion` and its loops |
| `mccast.c:7037` | **`mccast.c:1892`** | `ast_bad_type` |
| `mccast.c:16060` | **`mccast.c:12932`** | the Replay_IR arming block (`rir_reset()`) |
| `mccast.c:16553` | **`mccast.c:12805`** | `ast_replay_ok` |
| `mcc.h:1662` | **`mcc.h:1686`** | `for_each_elem` (was 1669 pre-merge) |
| `mcc.h:2128` | **`mcc.h:2147`** | `MCC_SET_STATE` (was 2130 pre-merge) |
| `mccrun.c:783` | **`mccrun.c:783`** ✓ | `char file[100]` — correct |
| `mccgen.c:1921` | **`mccgen.c:1846`** | `save_reg_upstack` |
| `mccgen.c:4060` | **`mccgen.c:4127`** | `warn_extra_ptr_zero_cmp` |
| `mccgen.c:4547` | **`mccgen.c:426`** | `gen_cast` |
| `mccgen.c:7679` | **`mccgen.c:8636`** | `rir_hook_slot_record` |
| `mccgen.c:11030` / `:11771` | re-derive | `unary`'s `vtop->sym = s`; the op-assign vdup |
| `mccgen.c:12243` | **`mccgen.c:13233`** | `rir_hook_cleanup_call_begin` |
| `mccgen.c:13391` | **`mccgen.c:14237`** | `decl_designator` |
| `mccgen.c:14588` | **`mccgen.c:15581`** | `rir_hook_body_begin` call site |
| `mccjit_embed.c:594` | **`mccjit_embed.c:571`** | `mccjit_recompile_common` |
| `mccast.c` "19,082 lines" | **15,910** | unmoved by the merge; `mccgen.c` is now **16,244**, `mccrir.c` 5,092, `mcc.h` 2,154 |

### Verified true — do not re-check these

- **The P5 deletion is complete.** All 33 recorder symbols and all 12 named
  recorder-shape gates read 0 under `src/`, and the gates read 0 under `tests/` too.
  All six keepers are present. Four residue items survive and are listed under
  **Deletion residue**.
- **Every file this document says was deleted is gone**: `verify_ratchet.cmake`,
  `rir_c3.cmake`, `journal_inert.cmake`, `jrn_sweep.sh`, `journal_{sweep,native,report}.cmake`,
  and the `verify-baseline/` and `journal-baseline/` directories.
- **Cell counts**: 48 `ast/rir-parity-*`, 14 `ast/rir-c2-*`, **0** `ast/rir-c3-*`, 0
  `ast-verify-ratchet-*`, 0 `ast/treecheck`, 0 `ast/tracediff`, 0 `ast/rir-inert`, 0
  `ast/journal-*`, 44 `ast/replay-*`, 20 `asttool`, 20 `selfhost-*`.
- **All 26 symbols this file claims exist, exist**: `ast_func_has_labeladdr`,
  `ast_sccp_has_case`, `ASM_REGVAR_ASMREG`, `AST_Poison` (5 producers in `mccast.c`
  at `:6785 :7607 :7672 :7894 :8863`, plus `mccrir.c:2180`), `rir_flush_effect_top`,
  `rir_lvalue_shape`, `rir_addr_pure`, `rir_hook_castsynth_end`, `RIR_R_SYNTH`,
  `RIR_M_ASMOPS`, `AST_OP_ASMOPS`, `rir_prod_take`, `rir_try_active`, `ast_ii_cval_fits`,
  `gen_vector_type_cache`, `rir_cfpfx`, `ast_promo_write`, and the rest.
- **The two `-O3` graft predicates read exactly as described** — `ast_fn_inlinable`
  opens `if (!ast_inline_env && !ast_inline_pass_env)`, `ast_reemit_retain` opens
  `if (!ast_inline_env || ast_reemit_poison || ast_reemit_n >= AST_INLINE_MAX)`.
  The widening is real and still undecided.
- **`tracegate`, `schemagate`, `targetgate` all clean**; `src/mccrir.c` contains
  **0** `MCC_TRACE(` calls.
- **All four side configurations build green** in distinct directories:
  `MCC_SINGLE_SOURCE=OFF`, `MCC_CONFIG_OPTIMIZER=OFF`, `MCC_REPLAY_IR=OFF`,
  `MCC_CONFIG_ASM=OFF`.
- **`MCC_AST_INT128=1` is out of `rir_parity.cmake`**, exactly as **Keep the
  measurement honest** said to do it, and the forced-`-O0` board is unmoved: all
  twelve keys read `empty=35 unfaithful=0 diverge=0 rewind=0 error=0 unbal=0 ovf=0`.
- **`C2_FORCE`'s gate derivation survives the deletion** — it now derives **28**
  `optimize >= 1` gates where it once derived 38, and does not hard-fail. Forced
  `-O0` on x86_64/`exec` reads `c2ok=1111/1111`, gap 0.
- **Production cannot be turned off.** `rir_prod_env = ast_replay_env && !rir_env`
  (`mccrir.c:521`), no gate term. `MCC_RIR_PROD` sets only `rir_prod_gate`
  (`mccrir.c:5082`). Any recipe in this file phrased as a prod-on/prod-off A/B is
  dead as written.
- **Two regressions this file records as closed are genuinely closed**, re-run
  directly: the computed-goto/label-address case prints `10 20` at all four `-O`
  levels, and `dead_code.c` produces identical output at all four.
- **Vector types work** at all four `-O` levels (`v4si` arithmetic, `==` yielding
  -1, `sizeof` 16), and the scalar-context guard fires with *"used vector type where
  a scalar is required"*.
- **The i386 static-TLS defect is still open and still reproduces**: `-static` over
  a `__thread` variable prints *"Unknown relocation type for got: 16"*; the same
  source links clean dynamically.
- **`SizeOfStackReserve` is 8MB for all PE targets** (`src/objfmt/mccpe.c:738`).

### What this host could and could not execute

Verified reachable and used: `qemu-i386`, `qemu-arm`, `qemu-aarch64`, `qemu-riscv64`
all run mcc-built cross binaries (each prints `hello 42`, rc=0) — note arm64 and
riscv64 need `-L<sysroot>/usr/lib64 -L<sysroot>/lib64` on the mcc link line or
`crt1.o` is not found, which reads as a compiler failure and is not one. `wine` runs
both x86 PE keys. `docker` is available.

Unreachable here, and the reason each one is a real gap rather than an omission:
Mach-O does not execute on Linux, so `x86_64-osx` and `arm64-osx` are byte-compared
only; `wine` on an x86_64 host does not load ARM or ARM64 PE, so `arm-win32`,
`arm-wince` and `arm64-win32` are byte-compared only; and no self-host on a Windows
or macOS *host* can be attempted at all. Those are items **W1**–**W5** at the top of
this file.

---

## Always-keep (`-fno-replay-fallback`): 31 failing cells → 10

Driving toward "-O1 never falls back" the useful measurement is not the census but
`-fno-replay-fallback ctest`, which keeps the replay unconditionally and so *executes*
every body the fallback would have hidden. The default path is 8230/8230 throughout
everything below; these numbers are the always-keep run only.

Start of this pass: 31 failures. Now 10. The 31 were never 31 distinct bugs — 21 of
them were one golden replicated across every exec variant.

### Closed

- **`gjmp_append` / promotion of a loop condition that assigns** — `while ((n2 =
  read32le(p = ...)))` came out at `-O4` with five variables promoted to registers
  and wrong control flow. Found by bisecting `mcc.c`'s `-O4` fallbacks with
  `MCC_RIR_NOFB_SKIP`: skipping only `gjmp_append` made `selfhost-jit` pass, and
  `-fno-promote-locals` made it pass with nothing skipped — which named the *pass*,
  not the arena. `ast_plan_promotion` now declines a body when an `AST_If` with a
  loop op has a `Store`/`StoreVal` anywhere in its condition, alongside the existing
  landor decline. `selfhost-jit` had previously died as "memory full" (that was
  `ptr_unlink`'s infinite loop) and then as SIGSEGV. Census: `-O2` 37→35, `-O4`
  63→61, `-O1` unchanged at 34.
- **`-freverse-funcargs` (21 cells)** — all 21 `errors_and_warnings` cells were the
  single golden `test_reverse_funcargs`. The other 91 macros in that file agree on
  diagnostics *exactly*, so this was purely evaluation order:

      printf(" %d %d %d\n", printf("1"), printf("22"), printf("333"));
      parser  333221 1 2 3
      replay  122333 1 2 3

  The parser implements right-to-left by saving each argument's tokens, replaying
  them backwards, then `vrev()`ing the stack — so what reaches the arena is the
  post-vrev *source* order and a child-order replay evaluates left to right.
  Faithful modelling needs an order flag on the `Invoke` node and a schema revision.
  The option is off by default and never appears in the census, so `rir_prod_take`
  now refuses with a new `revargs` reason. Correctness first.

### Open: `exec-gatesoff/assign_value_effects` — a real arena bug

`gatesoff` is `-O3` with `MCC_AST_CHAINSTORE=0`. Exit 8 is the `chained(5)` check:
`a = b = f(v)` must call `f` **once**, and the arena calls it twice. The dump is
unambiguous — two `Invoke` nodes:

    BasicBlock
      Store  Ref b   Invoke f(v)
      Store  Ref a   Invoke f(v)      <-- duplicate
      Return Binary + (Ref a) (Ref b)

`-fno-promote-locals` does **not** change it, so this is the arena, not a pass. This
is a wrong-code defect that fallback has been masking: at default settings the body
simply fails `faithful` and ships parser bytes.

Two fixes tried and both **refuted, with costs**:

- **N22 — read the target back.** In the chained branch of the `IR_OP_VSTORE`
  handler, when the inner store's value `rir_effectful()`, emit `Load(dup(target))`
  instead of `ast_dup_sub(value)`. Fixes the test, but the parser keeps that value
  in a *register* rather than reloading, so the bytes stop matching: census `-O1`
  34→**40**, and the default suite broke — **15 failures**. Reverted. A reload is
  also wrong outright for a narrow or volatile target.
- **N23 — bail on an effectful chained store.** Setting `rir_prod_bail` in the same
  branch changes nothing: `chained` still reports `[rir-prod] used ... len`. The
  branch is not the site that duplicates. `rir_prod_bail` resets at function begin
  (`src/mccrir.c:558`), which is before capture, so the reset is not the cause. The
  duplication therefore comes from the shadow-stack reconstruction that rebuilds the
  value top (`rir_stamp_call_top` / the `AST_Load` construction near
  `src/mccrir.c:1946`), **not** from the chained detection at `src/mccrir.c:2309`.
  That is where the next attempt should start, and it should begin by dumping the
  capture stream rather than by reading the handler — N20 applies.

### Open: the rest

- 7 selfhost cells — `selfhost-fixpoint`, `-O1`, `-O3`, `-Os`, `-gates`, and
  `selfhost-output-parity-O2`/`-O3`. Not yet diagnosed. Fixpoint failing while the
  default suite is green means the *second* generation diverges, so the right first
  measurement is which stage-2 object differs, not which test fails.
- `selfhost-arm64-native` — not yet diagnosed.
- `cross/no-compiler-abort-x86_64-win32` — banked already, do not touch.

### Method note

The census and the always-keep suite answer different questions and the census is
the weaker one. `mcc.c -O1` sat at 34 before and after the `revargs` fix because a
refusal is a *skip*, not a fallback; meanwhile 21 executing cells went from wrong to
right. When the goal is "the replay is correct", count failing cells under
`-fno-replay-fallback`. When the goal is "the replay is byte-faithful", count the census.
Do not report one as though it were the other.

---

## The chained store is the dominant remaining class

`mcc.c -O1` sits at 34 fallbacks: 21 `len`, 12 `bytes`, 1 `error1` (banked, do not
touch). Sampling the byte diffs rather than the counts, the largest identifiable group
is one C shape — the chained assignment.

    cleanup_symbols   s->data_offset = s->link->data_offset = s->hash->data_offset = 0;
    cleanup_sections  s->data = mcc_realloc(s->data, s->data_allocated = s->data_offset);
    chained (test)    a = b = f(v);

The parser computes the addresses first and then stores; the replay interleaves
address computation with the stores. `cleanup_symbols` at `+36`:

    parser  load s1 ; load [s1+0x60] ; load s1 ; load [s1+0x70] ; xor ; store ; store
    replay  load s1 ; load [+0x60] ; load s1 ; load [+0x70] ; xor ; store ; store
            (same instructions, different registers and a different store order)

**The class splits, and the halves need different treatment:**

- **Pure value** (`= 0`, a constant, a non-volatile load) — the reorder is *benign*.
  Three independent addresses all set to zero end up zero whatever the order. This
  costs faithfulness only, and is a candidate for the semantic-equivalence verdict
  rather than for a byte fix.
- **Effectful value** (a call) — the duplication is a **wrong-code bug**. `a = b =
  f(v)` captures two `Invoke` nodes and calls `f` twice. This is the open
  `assign_value_effects` failure and it is not a faithfulness question at all.

Do not attack these together. The second is a correctness defect and should be fixed
or refused on its own; the first is a byte-equivalence question and may not be worth
fixing at all if the equivalence verdict can carry it.

### Reading `firstdiff` correctly

`firstdiff` frequently points at a **jump displacement**, not at the defect.
`builtin_libm_find` reports `@18`, where the only difference is `0f 8d 4b` against
`0f 8d 4e` — a forward branch encoding a target three bytes further out. The real
divergence is downstream and the early offset is an echo of it. Always read the whole
window (`MCC_AST_UNFAITHFUL_DUMP=<n>` with a large `n`) before believing the offset.

### Instrumentation that works, and one that does not

- `MCC_LOG=0xff MCC_AST_UNFAITHFUL_DUMP=<window>` prints `[unfaithful] <fn> @<off>
  parser:` / `replay:` byte rows and `[unfaithful-rel]` symbol rows. This is the
  useful one.
- `RIRPRODDUMP=<fn>` prints the arena at five stages, and `[ast-postreplay]` gives
  `newlen`/`bodylen` directly.
- The `MCC_TRACE_IF("UNFAITHFUL ...")` line next to that dump does **not** reach
  stderr under `MCC_LOG=0xff`. Do not spend time trying to grep for it; use the
  `[unfaithful]` rows instead.

### The chained-store nesting knob: three variants, all measured

`ast_detach_last_child` makes the nested shape `Store(a, Store(b, v))` reachable, so
the outer store can consume the inner one instead of copying its value subtree. What
gets nested is a tunable, and the three settings tried do **not** trade off the way
they look like they should:

| variant | `mcc.c -O1` | suite |
|---|---|---|
| effectful value only — **shipped** | 34 | clean |
| nest everything | **32** | `chained_assign` mixed + 2 `optfire/chainstore` fail |
| nest everything, `Convert`-wrapped when types differ | **79** | 4 fail |
| nest when base types agree, or effectful | 35 | clean |

The `32` is **not** two real wins. The gain comes precisely from nesting stores whose
value converts — `a = b = expr` has `b`'s type — and those are the same bodies that
break: `chained_assign`'s mixed case prints `165.018532` for `162.000000`. Restricting
to equal types, either on the full type word or on `VT_BTYPE` alone, gives 34 and 35
respectively, which proves the improvement lived entirely in the wrong half. Wrapping
the nested store in a `Convert` to the inner store's type is much worse than either,
so the replay's store-as-value arm is not simply missing a cast.

**Do not re-tune this knob without first answering what the replay's store-as-value
arm yields for a converting store.** That is the actual unknown, and every variant
above is a guess about it. Measure it directly.

### Correction to N23

The earlier note said bailing in the chained branch "changed nothing, which locates the
duplication elsewhere". The second half was wrong. The branch *is* taken -- confirmed
by instrumenting it, `kind=13` (`AST_StoreVal`), `nchild=0`. `rir_prod_bail` simply
cannot be set from there: `rir_prod_take` tests it **before** calling `rir_build()`,
and `IR_OP_VSTORE` runs during the build, so the flag is read a function too late. Any
refusal decided inside a handler needs a counter checked *after* the build, next to
the existing `mismatch` test.

Refusing would not have served the goal in any case. A refusal makes the body a
`skip` rather than a `fallback`, so the census improves while the object still ships
parser bytes. Driving the count down with refusals is gaming the metric; only a
faithful arena is real progress.

### Correcting the class sizes: chained stores are 2 bodies, not the dominant class

An earlier section called the chained store "the dominant remaining class". That was
wrong, and the measurement that disproves it was already in hand: nesting *everything*
moved `mcc.c -O1` from 34 to **32**. Two bodies. The other 32 have unrelated causes.
Sampling three diffs and generalising from them was the mistake -- exactly the habit
N20 warns about.

A cheap mechanical split of the 33 dumped diffs (64-byte window around the first
difference, comparing the byte *multiset*):

- **7 are pure reorders** -- identical bytes, different order:
  `_mcc_backtrace`, `bind_exe_dynsyms`, `case_adjacent`, `export_global_syms`,
  `put_elf_sym`, `relocate_syms`, `rt_find_state`. All are `bytes`-class. These are
  the natural constituency for the semantic-equivalence verdict rather than for byte
  fixes: if the equivalence proof can carry a reorder, they stop falling back
  *legitimately* rather than by refusal.
- **26 have genuinely different bytes** and need individual diagnosis.

The window is 64 bytes, so treat the 7 as a lead rather than a proof -- a reorder that
spills past the window will not be caught, and a coincidental multiset match is
possible.

### Honest scope estimate

Reaching zero fallback at `-O1` is not one fix or a few. It is on the order of 26
separate byte-level diagnoses plus an equivalence path for the 7 reorders, each
needing the dump-read-fix-measure loop that has taken roughly one working session per
*class* so far. Nothing found this session suggests a single common root cause behind
the 26; the four defects fixed this session were four unrelated mechanisms
(promotion of assigning loop conditions, argument evaluation order, non-store
condition effects, chained-store duplication).

The per-body work is parallelisable -- each body is independent, and the
instrumentation to triage one is now a single command:

    MCC_LOG=0xff MCC_AST_UNFAITHFUL_DUMP=64 mcc -w -O1 ... -c src/mcc.c 2>&1 \
      | grep '^\[unfaithful\] <fn> '

### format_func_spec: a doubled load, and why the obvious fold is not the fix

`format_func_spec` is a clean single-cause `len` case worth finishing. Its
`strcmp(name, tbl[i].n)` replays as

    parser  lea tbl ; add rcx ; mov rcx,[rcx]
    replay  lea tbl ; add rcx ; mov rcx,[rcx] ; mov rcx,[rcx]

three bytes longer, which is exactly the `0f 83 a7` -> `0f 83 aa` displacement seen at
`@33`. The arena subtree is

    Load ( Convert ( Unary op#262145 ( Load ( Binary + tbl idx ) ) ) )

**`op#262145` is `AST_OP_MEMBER` (0x40001), not `AST_OP_ADDR` (0x40000).** Reading it
as an address-of and folding `&*y -> y` is wrong, and that misreading cost two
experiments here. The real shape is a member access whose base is already a load, so
the fold has to account for the member offset rather than cancel a dereference.

Two negatives worth keeping:

- **N24 — fold at the `IR_OP_ADDR` capture site.** `mcc.c -O1` fallback 34 -> 29, but
  `skip` 10 -> **64** and `used` 1126 -> 1077: 54 bodies came out `invalid`. Folding
  during capture disturbs the shadow stack. Detaching the inner node first does not
  help -- the count is identical -- so it is the stack bookkeeping, not re-parenting.
- **N25 — fold as a post-build rewrite.** Inert, because it tested for
  `AST_OP_ADDR` and the node is `AST_OP_MEMBER`. The approach is still the right one:
  a post-build pass leaves capture alone and so avoids N24 entirely. `ast_replace_child`
  was written for it and is worth re-adding when the pass is written correctly.

The general lesson for this file: a post-build arena rewrite is the safe place for
canonicalisation. Capture-site edits have now failed this way twice.

### N26 — the arena is cumulative, which invalidates whole-arena rewrite passes

Both fold attempts above assumed `rir_arena` holds one function. It does not.
Instrumenting a pass placed after `rir_build()` in `rir_prod_take` and printing
`ast_count(rir_arena)` per call gives

    host_runmem_dual  22     merge_funcattr 121    merge_attr 116
    bf_operand_bits   791    type_size     1320    cplx_push_cst 445

-- monotonically growing, then resetting. A pass written as
`for (n = 0; n < ast_count(rir_arena); n++)` therefore walks **previously built
bodies as well as the current one**, and rewriting there corrupts arenas already
handed out. Anything of this shape must be bounded to the current body's node range,
or run from the per-body entry point rather than over the whole arena.

The same instrumentation turned up a second thing worth knowing: that pass ran only
**10 times** across a `mcc.c` compile whose census reports 1126 `used` + 34 `fallback`
+ 10 `skip`. So the overwhelming majority of bodies do **not** reach `rir_prod_take`'s
post-build point. Before writing any pass there, find out which entry point the other
1160 bodies actually take -- placing work in `rir_prod_take` and measuring no change
proves nothing about the work, only about the placement.

That is the concrete blocker for the `format_func_spec` fix, and it is a plumbing
question with a definite answer, not another guess.

### N26 is WRONG — retracted in full

The preceding N26 entry claimed two things and **both are false**. Retained here only
so the retraction travels with the claim.

1. *"`rir_prod_take` is not the common path -- a pass placed after `rir_build()` ran
   only 10 times."* **False.** Counting entries directly gives **1172 calls and 1172
   builds** over a `mcc.c` compile. The placement was on the common path the whole
   time. The pass ran 10 times because it was wrapped in
   `mcc_env_on("MCC_RIR_FOLD_MEMLOAD")`, and that gate -- not the placement -- is what
   suppressed it. Whatever `mcc_env_on` does with a repeatedly-read variable, do not
   use it to gate a per-body pass without checking it fires per body.
2. *"The arena is cumulative, so a whole-arena loop rewrites bodies already handed
   out."* **False.** The `ast_count` sequence 22, 121, **116**, 791, 1320, **445** is
   non-monotonic, which means it resets per body. I read a rising prefix and inferred
   accumulation.

Running the `Member(Load(y)) -> Member(y)` fold **unconditionally**, which is the
experiment those two errors had prevented, leaves the census at **34** and leaves
`format_func_spec` at `newlen=220 bodylen=217` -- the same +3. So the fold does not
apply to that body, and the doubled `mov rcx,[rcx]` has some other origin than the
`Member` node's child being a `Load`. The subtree is

    Load ( Convert ( Unary MEMBER ( Load ( Binary + tbl idx ) ) ) )

and the next step is to establish **which** of the two `Load`s the parser does not
emit, by reading the replay of this exact body instruction by instruction rather than
by pattern-matching the tree shape. Three attempts have now been made against a guess
about the shape; none survived contact.

`ast_replace_child` was written twice for this and reverted twice. It is a correct
helper -- position-preserving child swap, returns 0 without side effects when `old` is
not a child -- and is worth re-adding once there is a fold that actually earns it.

## The real placement bug, and the fold it unblocked

Everything above about "the fold does nothing" was measuring a pass that never ran.
**`rir_to_arena()` -- which creates the arena -- is called at `src/mccrir.c:4647`, well
after `rir_build()`.** A pass inserted next to `rir_build()` sees `rir_arena == NULL`
and returns immediately. The ten bodies that did fire were leftovers from calls that
returned early before the `rir_arena = NULL` at the end of `rir_prod_take`.

Moved after `rir_to_arena()`, the same pass runs on all **1172** bodies. That single
line of placement is what three earlier "inert" results were actually reporting.

### Byte attribution: use it, it works

`RVATTR=<fn>` -- a wrapper around `ast_replay_value` recording `ind` before and after
each node -- gave the answer for `format_func_spec` in one run:

    n=33 Binary +   +17   address computation
    n=34 Load       +0    indir() on an address, emits nothing
    n=35 MEMBER     +0    lvalue, emits nothing
    n=37 Load       +3    indir() on an lvalue MATERIALISES the pointer
    n=39 Invoke     +38   15 bytes of marshalling and a second load

So the outer `Load` is redundant: `AST_OP_MEMBER` already leaves `VT_LVAL` set, and
the use loads once by itself. Stop pattern-matching tree shapes; attribute the bytes.

### N27 — the Load-over-MEMBER fold: correct for pointers, and worthless there

Turning that `Load` into a same-type `Convert` (mutate in place -- **detaching orphans
it with `nc=0` and `rir_emit_safe` walks unreachable nodes too, which is the entire
"unsafe" cluster**) gives, with the default suite **clean at 8253/8253**:

| | `-O1` | `-O2` | `-O4` | used | always-keep |
|---|---|---|---|---|---|
| baseline | 34 | 35 | 61 | 1126 | 9 |
| fold, unguarded | **29** | **30** | **56** | **1131** | **46** |
| fold, pointer-valued only | 34 | 35 | 61 | 1126 | 9 |

The unguarded fold is a genuine five-body win *and* miscompiles: the 46 are 21
`union_byval` + 21 `transparent_union`, which pass their arguments as addresses
instead of values -- `TU 11 22 12 77` becomes `TU -1283239216 ...`. Guarding on
`VT_ARRAY` or on `VT_STRUCT` does **not** catch it; the member's own type there is
scalar. Guarding on a pointer-valued result does, and gives back exactly zero.

**The gain lives entirely in the half that is wrong** -- the same shape as the
chained-store knob. Not landed. The remaining question is what distinguishes a
transparent-union argument from `tbl[i].n` at this point in the arena; answer that and
the five bodies are real.

### N28 — the type-preserving discriminator for N27 does not exist

The obvious split for N27 looked exact: fold the `Load` over `AST_OP_MEMBER` only when
it does not change the type, on the reasoning that

    format_func_spec   tbl[i].n    char * member read as char *   redundant
    transparent_union  *u.pi       int * member read as int       real dereference

Measured, that guard gives `mcc.c -O1` **34** -- the baseline -- with always-keep back
to 9. So **all five bodies N27 gained were type-changing loads**, the same class as
`*u.pi`. There is no safe subset here: every fold that helps is one that removes a
dereference something else needs.

That closes the N27 line as stated. Three discriminators have now been tried against
it -- `VT_ARRAY`, `VT_STRUCT`, type-equality -- and the pointer-result one; each either
misses the union breakage or erases the entire gain. The next attempt should stop
looking for a predicate over the *node* and instead ask why five bodies contain a
`Load` over a `MEMBER` that the parser does not emit at all. That is a capture
question, not a folding question.

---

## W6 — i386 C2 gap 6, opened by the AST_FB_LOAD_LVAL fix (needs a Windows/i386 pass)

`ast/rir-c2-i386` and `ast/rir-c2-i386-win32` report

    rir_c2: -O1 srcs=279 ok=269 notok=10 fn=1166 faithful=1131 c2ok=1125/1131
            gap=6 (bytes=0 len=6 err=0 invalid=0)

against a banked 0. Every other cell of the 8255-cell suite passes. This was landed
deliberately: the change it comes with closes a wrong-code bug that made a
self-hosted compiler segfault, and the i386 gap is a byte-faithfulness ratchet, not a
behaviour failure. Trading a live miscompile for six unfaithful i386 bodies is the
right way round, but the six should be closed rather than re-banked if that is
possible.

**What to look at.** The fix skips a redundant `indir()` in `ast_replay_value`'s
`AST_Load` arm when three things hold: the capture recorded that the parser was *not*
on an lvalue (`AST_FB_LOAD_LVAL` clear), the replay's `vtop` *is* an lvalue, and the
load sits over a member access (`ast_load_over_member`). All six i386 bodies fail on
length, so the skip is firing there where the i386 parser did emit the load.

**Three narrowings already tried, all rejected — do not repeat them:**

| narrowing | i386 C2 | `mcc.c -O1` | selfhost-fixpoint |
|---|---|---|---|
| add an `AST_FB_LOAD_SEEN` "we actually looked" bit | still 6 | 37 | SIGSEGV again |
| restrict `ast_load_over_member` to plain `AST_OP_MEMBER` | **0, passes** | 37 | SIGSEGV again |
| compare the Load's type to its child's | n/a | 36 | SIGSEGV again |

The second is the informative one: **the entire win lives in the `AST_OP_MEMBER_ARROW`
path, and that is also exactly what i386 objects to.** So the question to answer is why
`->` through a member differs between the x86_64 and i386 back ends at this point --
most likely the i386 `AST_OP_MEMBER_ARROW` arm leaves `vtop->r` in a different state
than x86_64's, so the `!(vtop->r & VT_LVAL)` test does not mean the same thing on both.
Compare the two `AST_OP_MEMBER_ARROW` replays under `RVATTR` on one of the six bodies
before changing anything.

`tests/ast/rir_c2.cmake` reports counts only; naming the six needs instrumentation in
the C2 leg, which does not exist yet.

## The 119 `gcc.dg` FAILEXE: 74% are not ours, and none of the rest is an optimizer bug

Re-measured at `d4bf0d27` with `tools/xsuite.py --opt=-O0 --ref clang`: **120**
`gcc.dg` `FAILEXE`, not 119. Every one of them was then re-run through the *same*
harness with `--mcc /usr/bin/gcc` and `--mcc clang` so the flags and the timeout are
identical rather than reconstructed by hand.

| verdict | count | reading |
|---|---:|---|
| gcc **and** clang pass at `-O0` | 30 | ours |
| gcc passes, clang does not | 48 | out of scope: the goal metric is *"every gcc test clang can run"* |
| neither reference passes | 42 | not a compiler gap at all |

Taking "clang passes" as the bar — which is what the goal sentence says — the
in-scope set is **31 of 120 (26%)**: the 30 above plus `pointer-counted-by-8.c`,
where clang passes and gcc does not. The other 89 are noise in this block and should
stop being counted against mcc. The largest single family in the 48 is
`builtin-object-size-*`/`builtin-dynamic-object-size-*`, which clang cannot do at
`-O0` at all.

**The 31, clustered.** Each was also run at `-O0/-O1/-O2/-O3`:

| cluster | n | root cause |
|---|---:|---|
| VLA-typed operand of `sizeof`/`typeof` is not evaluated | 8 | `expr_type()` at **`src/mccgen.c:8609`** |
| `__builtin_object_size` precision | 8 | `builtin-{,dynamic-}object-size-{9,11,12}`, `pr39343`, `pointer-counted-by-8` |
| `__builtin_constant_p` true only after IPA | 4 | `ipa/inline-8`, `ipa/pr92497-1`, `tree-ssa/modref-{2,5}` |
| `ms_struct` bitfield layout | 2 | `bf-ms-layout-{2,5}` |
| gnu23 tag-based aliasing exploited by the optimizer | 2 | `gnu23-tag-alias-{2,5}` |
| singletons | 7 | `c11-uni-string-1` (`u8`/`u`/`U` as macro names), `c90-scope-1`, `cpp/embed-12`, `fwrapv-2`, `gnu99-init-1` (GNU range designators), `vla-24`, `torture/20240517-1` |

**The finding that matters: 30 of the 31 fail identically at `-O0`, `-O1`, `-O2` and
`-O3`.** (The exception is `torture/20240517-1`, which *passes* at `-O2`/`-O3` and
wants `-fmerge-all-constants` at lower levels.) So not one of them is a wrong-code
defect introduced by the AST/RIR optimizer; they are missing front-end and codegen
capability. Anyone taking this block should expect to work in `src/mccgen.c`, and
`src/mccast.c`/`src/mccrir.c` will not help.

**The VLA cluster is one line of ownership away.** `expr_type()` does an
unconditional `nocode_wanted++` around the operand, but C99 6.5.3.4p2 requires a
VLA-typed operand to be *evaluated*. Confirmed with a two-line repro: `sizeof
(typeof (*(++i, (char (*)[i])a)))` leaves `i == 0` under mcc and `i == 1` under gcc.
It pays `vla-{14,15,16}`, `vla-stexp-{4,6,9}`, `typename-vla-1` and `pr114831-2`
(`typeof((n++,a))`), i.e. 8 of the 31, and it is the single highest-yield change in
the whole block. `vla-24` is *not* in it: there the VLA scope exit restores the stack
pointer and takes an `alloca` from the same scope with it.

**Both known flakes re-measured** on the tip: `20050527-1.c` is 40/40 clean now and
`flex-array-counted-by-pr121000.c` is **13/40** — still nondeterministic, so any
board that shows either of them moving is showing a coin flip. `20050527-1.c` carries
no `dg-options`, so it runs at `-O0`, where the AST optimizer is off entirely; it can
never be evidence about an optimizer change.

## The `link_error` block: what each of the 56 actually wants

85 files reach the linker with an unresolved `link_error`. 29 are `REFFAIL` (clang
misses them too) and are out of scope, leaving **56**. Classified by reading every
one of them, the capability needed is:

| capability | n |
|---|---:|
| alias analysis (TBAA, restrict, points-to, distinct mallocs) | 10 |
| ranges through arithmetic (`>>`, `/`, `*`, narrowing casts, bitfield precision) | 7 |
| folding comparisons of address expressions | 6 |
| plain copy/constant propagation across a merge | 6 |
| `__builtin_{pow,exp,copysign,fmod}` constant folding | 4 |
| ranges from constant aggregate initializers, unknown index | 4 |
| unsigned/signed wrap reasoning | 4 |
| range narrowed by a dominating condition | 4 |
| loop unrolling / store motion / IV range | 4 |
| known-bits (alignment, low-zero-bit) propagation | 2 |
| interprocedural (indirect inlining, callee return range) | 2 |
| **constant-index read from a `static const` aggregate** | **1** |
| proving a loop never exits | 1 |
| `__attribute__((nonnull))` as a range source | 1 |

The last-but-two line is now closed; see below. Two notes for whoever takes the rest:

*The address-comparison cluster is in `gen_opic`, not in the AST.* `src/mccgen.c:3586`
already folds `sym+a <op> sym+b` for `-`/`==`/`!=` when both sides are
`VT_CONST|VT_SYM` with the *same* symbol, and `:3596` folds two *different* named
symbols under `CONST_WANTED`. What is missing is (a) the same treatment when both
sides are `VT_LOCAL` with no symbol -- which is what `pr15791-{1,2}`, `pr19807-1` and
`pr27150-1` all are, since their arrays are on the stack -- and (b) an arm for a
symbol address compared against literal 0, which is `pr15347`. Neither belongs in
`mccast.c`. **(a) has a real hazard**: `loc` is restored at block exit, so two locals
in disjoint sibling scopes can share a frame offset, and "same offset" would then
wrongly mean "same object". Any implementation must prove that cannot reach the fold.

*`fwrapv-2` wants `(2*x)/2 -> x`.* Legal only because signed overflow is undefined,
so it must be predicated on `-fwrapv` being off. One test; weigh it against the fact
that this is the exact identity the wrong-fold audit calls out as an example.

## Landed: constant-index reads from `static const` aggregates (`ast_cload_run`)

`src/mccast.c`, strategy index 21, gate `MCC_AST_CLOAD` (default `-O1`+ and `-O4`).
Rewrites an arena node that reads a `static const` object at a link-time-constant
address into an `AST_Literal`. Three node shapes fold -- an `AST_Load` over a
constant address, an `AST_Ref` that is itself the lvalue, and an `AST_OP_MEMBER` over
either -- because folding only the first would leave `cars[1].tire_pressure[2]`
constant while `cars[1].speed` stayed a load.

**It reuses `gen.c` rather than reimplementing it.** `mccast.c` is `#include`d into
the same TU, so `const_lval_bytes` (`src/mccgen.c:3355`: `SHT_NOBITS`, bounds, and
the relocation-overlap scan) and `fold_const_lval_at` (`:3393`: sign/zero extension)
are directly callable. They were already correct; they were simply unreachable from a
function body, because `fold_const_lval` gates them on `global_expr && CONST_WANTED`.
What was missing was only the walk from an arena subtree back to `sym + byte offset`,
which is `ast_cload_addr`/`ast_cload_lval`.

**Two things about the arena that cost most of the debugging time and are worth
writing down.**

1. *The index operand of `AST_Binary '+'` is in element units, not bytes.* The IR
   capture suppresses nesting, so `gen_op`'s own `vpush_type_size(); gen_op('*')` for
   pointer arithmetic never reaches the arena. The stride is implied entirely by the
   pointee type, which is why the two walkers thread a `CType` alongside the offset.
   `AST_Load` carries no type of its own; the loaded type *is* the pointee the child
   resolved to, which also makes `*(const unsigned char *)&ia[0]` read as a byte
   because the `AST_Convert` updates it.
2. *A decayed array `Ref` has no `VT_LVAL`.* `gaddrof` cleared it, so it is a bare
   `VT_CONST|VT_SYM` value and never reaches the lvalue walker. That needed its own
   arm.

**Two guards are deliberately stricter than `gen.c`'s.** `const_lval_bytes` waives its
`SHF_WRITE` check for anonymous symbols, which is sound where it is used -- a constant
expression is evaluated before any store can run -- and is *not* sound inside a
function body, where a file-scope compound literal in `.data` may already have been
written; so a non-writable section is required outright. And the fold only fires where
the parent consumes the node as a plain rvalue (`ast_cload_rvalue_use`), because
`AST_OP_ADDR`, `AST_OP_MEMBER` and the inc/dec path all need the lvalue itself.

**The architectural constraint this ran into, for the next person.** The first attempt
folded at replay time, inside `ast_replay_value`'s `AST_Load` arm, and moved nothing.
`ast_func_end` replays the arena *once as a fidelity check* and computes `faithful` by
comparing the emitted bytes and relocations against the parser's; a replay that
optimizes is unfaithful and the whole body falls back. Any pass that changes code must
be an arena rewrite in the post-fidelity strategy phase, and its hit count must be
wired into the `do_*` disjunctions at `src/mccast.c:16135`/`:16158` and into
`AST_PF_EMIT`'s `ast_fconst_i` -- **a strategy that is not in those lists runs and is
then thrown away**, which is exactly what happened. This is also why
`ast_run_templates` may run *before* the fidelity replay: `Literal op Literal` was
already folded by `gen_opic` during parsing, so it is byte-neutral.

**Yield: one test**, `gcc.dg/tree-ssa/pr14841.c`, `FAIL -> PASS`. The whole-tree sweep
moves 18,837 -> 18,840 `PASS` with **zero** regressions; the other two rows are the
`flex-array-counted-by-pr121000` and `20050527-1` flakes documented above.
`gcc.c-torture/execute` at `-O0/-O1/-O2/-O3` is byte-for-byte the same board before and
after (0 cells moved). `ctest` 8122/8122, `selfhost-fixpoint -O3` byte-identical,
`tracegate`/`schemagate` OK. `-O0` output cannot move: the gate is `optimize >= 1` and
so is the `sg_templates` strategy gate. Compiling `src/mcc.c` at `-O3` the fold fires
**7 times in 6 bodies**, so `selfhost-fixpoint` is a real exercise of it and not a
vacuous pass. A 26-case adversarial file (`INT_MIN`/`INT_MAX`, `UINT_MAX`, signed and
unsigned `char`/`short`/`long long`, `_Bool`, mixed-member structs, 2-D arrays,
string-literal indexing, `&ia[2]-&ia[0]`, a runtime index that must still load, and
`*(const unsigned char *)&ia[0]`) agrees with gcc at all four levels.

One test is a thin return for the machinery, and that is the honest headline. The
reason to keep it is that it is the *prerequisite* for the four-test
`CONST-AGG-RANGE` row above and it is the piece with no soundness argument left to
make.

**Next step on this line, precisely.** `vrp-from-cst-agg-{3,4,7}` need the same walk
with a *non-literal* index: enumerate `base + i*stride` for `0 <= i < count` (out of
bounds is UB, so the union over declared elements is legal), cap the enumeration, and
take min/max of the bytes. The hard half is not that; it is delivering the range to
the comparison. In all three tests the value is stored into a local first, and
`ast_vlat_use_of` (`src/mccast.c:9732`) refuses any local that is not
`ast_local_is_readonly`. Either relax it to "written exactly once, from an expression
with a known range" -- in which case `ast_vlat_recompute` must be extended in lockstep
or the `MCC_CONFIG_AST_SHADOW` build aborts -- or add an expression-level
`ast_expr_range()` independent of the lattice. Note also that `ast_range_run` rewrites
`v <= 2 || v > 11` into `(unsigned)(v-3) > 8` before any of this is asked, so the fold
must either happen inside the earlier `ast_ident_run` fixpoint or see through
`Convert`+`-`.
---

## W7 — riscv64 local-exec TLS in a PIE — CLOSED

`qemu-riscv64` failed 8 of 23 cells, all of them the same two conformance cases
(`tls [pic]`, `tls_aggr [pic]`) refusing with

    mcc: error: local-exec TLS in a shared object is not valid;
                dynamic TLS is not implemented for riscv64

The message was describing a situation that was not occurring. The `[pic]` leg builds
`-fPIC -pie` (`tools/mccharness.c:2270`), a position-independent **executable**, and
`mcc_set_output_type` ORs `MCC_OUTPUT_DYN` -- which *is* `MCC_OUTPUT_DLL` -- into an
EXE whenever `-pie` is on. So a PIE carries **both** bits.

That made the two halves disagree:

- `riscv64-gen.c` gates general-dynamic on `pic && DLL && !EXE`, so for a PIE it
  correctly declined GD and emitted local-exec, which is valid there -- a PIE's TLS
  block sits at a fixed offset from the thread pointer, and gcc uses local-exec too.
- `riscv64-link.c:353` then refused that local-exec on the DLL bit **alone**, reading
  a PIE as a shared object.

So this was never a missing feature. `riscv64_tls_gd_a0` exists and the non-PIC legs
always passed; the linker check was simply broader than the codegen condition that
produces the relocation. Fixed by testing the same condition in both places: refuse
only a real shared library, `DLL && !EXE`.

Verified with qemu-user, not by inspection: both cases build **and run** clean under
`qemu-riscv64 -L <sysroot>`, and the preset went 15/23 to **23 of 23**. The docker
tier is 26 of 26.

`arm-link.c:427` and `arm64-link.c:430` carry the identical over-broad check. Neither
is reachable today because their codegen emits TLSDESC under `pic`, so TPREL never
arrives -- left alone deliberately rather than changed blind, but they are the same
latent bug if either target ever emits local-exec under `-pie`.


---

## The 26-preset Linux matrix: all green, and what it took

Run clean at `63d1d1fe` — configure, build and test from scratch per preset, nothing
editing the tree concurrently. **26 of 26 pass.**

    debug  release  sanitize  diagnostics  cst  cross
    linux-gcc  -cross  -musl  -release  -static  -multisource
    linux-gcc-asm-off  -predefs-off  -pie  -dwarf  -diagnostics  -sanitize
    linux-clang  -cross  -release
    qemu-x86_64  qemu-i386  qemu-arm  qemu-arm64  qemu-riscv64

Six were red at the start of the run. All six are closed:

| preset(s) | was | cause |
|---|---|---|
| `release`, `linux-gcc-pie` | `superopt/promote-floor` | wall-clock search under `-j32`; now `RUN_SERIAL` |
| `linux-clang` ×3 | `mcctest`, `mcctest-bcheck` | clang was acting as the "GCC-compatible reference"; now prefers gcc on PATH on Linux |
| `qemu-i386` | `flt_eval_method` | `__FLT_EVAL_METHOD__` was 0 while `FLT_EVAL_METHOD` was 2 |
| `qemu-riscv64` | 8 cells | W6/W7 — the PIE local-exec TLS refusal |

macOS and MSVC presets are excluded: they cannot run on a Linux host. The docker tier
is separately green at 26 of 26, and includes `selfhost-riscv64-docker`.

**A methodology note that cost real time.** The first attempt at this matrix ran while
I was still editing the tree, and it configures and builds from the live source. It
reported `cross` failing 8216 of 8232, `linux-gcc` as BUILD_FAIL, and `cst` at 10
failures. **Every one of those was my own mid-edit state, not a defect** — all green on
the clean re-run. If a sweep like this is worth starting, it is worth freezing the tree
first; otherwise its output is indistinguishable from a real regression and someone
will chase it.

### Census at the close

`mcc.c -O1`: **used 1181, fallback 24, skip 11** (`-O2` 25, `-O4` 54), down from 34-36
over the session with **seven** wrong-code defects closed underneath it: promotion of
assigning loop conditions, `-freverse-funcargs` argument order, non-store
loop-condition effects, the chained-store duplication, and the three cast-erasure bugs
above (`put_stabs`, `next`, `const_ref_data`/`format_str_literal`).

The remaining 24, all of `src/mcc.c` at `-O1` -- 12 `len`, 11 `bytes`, 1 error path:

    parse_include  embed_params_init  pragma_parse  pragma_operator  gen_complex_op
    unary_nested   case_adjacent      gen_function  mcc_debug_new    mcc_debug_frame_end
    mcc_eh_frame_hdr  put_elf_sym     relocate_syms bind_exe_dynsyms export_global_syms
    cleanup_symbols   cleanup_sections rt_find_state _mcc_backtrace  rt_fault
    decode  error1  so_filesize  asm_expr_cmp

**Eight of those 24 are the proven-equivalent set above** (`put_elf_sym`,
`relocate_syms`, `bind_exe_dynsyms`, `export_global_syms`, `rt_find_state`,
`_mcc_backtrace`, `case_adjacent`, `so_filesize`), and `error1` is banked N10 "do not
touch". So the genuinely open population is **15**, not 24. Disassemble before judging
any of them: `RVATTR` attributes emitted bytes to arena nodes, and the instruction
diff is what exposed all three miscompiles above -- the byte census called two of them
zero-delta.

---

## Three miscompiles the census could not distinguish from reordering

Found by disassembling the fallback bodies and diffing the *instructions* rather than
the bytes (`docs/fallbacks-O1.md`, regenerate per the note below). All three were
sitting in the census looking innocuous; two had a **zero** length delta, which is the
least suspicious row shape there is.

| body | verdict | the difference | consequence |
|---|---|---|---|
| `put_stabs` | `bytes`, delta 0 | `sub rax,0xc` → `sub rax,0x1` | pointer 11 bytes off the intended stab record |
| `next` | `bytes`, delta 0 | `ja` → `jg` | every token below `0xa8` treated as a digraph |
| `const_ref_data`, `format_str_literal` | `len`, +1 | `mov ecx,[rcx]` → `mov rcx,[rcx]` | 8-byte read of a value cast to `unsigned` |

**One root cause behind all three: a cast that emits no instruction leaves no trace in
the arena.** `gen_cast` returns without emitting for a pointer→pointer cast, for a
signedness-only cast on a value already in a register, and for an lvalue narrowing on
the `ALLOW_SUBTYPE_ACCESS` path. No instruction means no IR op, no hook, and no
`AST_Convert` node — so the replay re-derives the operation from operand types that no
longer carry the cast, and picks a different element size, signedness, or width.

`src/mccrir.c` already reconciles arena types against the parser's captured `SValue`
in three places. Each had a gap on exactly one side:

- the `GENOP` pointer repair read `ast_type_t(cur)` directly, and a `Binary` from a
  previous `GENOP` carries type 0, so `ptr-arith → cast → ptr-arith` chains bailed out;
- the genop signedness reconcile had `unsigned → signed` but no mirror;
- `rir_stamp_sv` reconciled widening (`cs >= vs2`) but discarded narrowing.

**None of the three had a use-site fix.** The information is physically absent from the
arena by the time the replay runs, which is worth remembering the next time the "fix at
the USE site" rule is applied — it holds only where the arena still carries what is
needed.

Measured on the combined tree: `mcc.c -O1` **31 → 24** fallbacks (`-O2` 32 → 25, `-O4`
60 → 54), `used` 1174 → 1181. ctest 8255 with only the two banked W6 i386 C2 cells,
docker tier 26 of 26, all four side configurations build.

### Eight bodies proven equivalent, not fixed

Audited separately and all safe: `put_elf_sym`, `relocate_syms`, `bind_exe_dynsyms`,
`export_global_syms`, `rt_find_state`, `_mcc_backtrace`, `case_adjacent`, `so_filesize`.
Seven are one transposed pair of *adjacent* loads with no store, call or barrier
between them — two loads with no intervening write read the same values in either
order, so aliasing never arises; the hoisted one is always a `[rbp-disp8]` frame slot
that cannot fault. `so_filesize` is different: a then/else block layout swap in a
ternary, both arms 55 bytes landing on the same join point.

These are the natural first entries for `RIREQUIV` (`rir_c2_equiv_proven`,
`src/mccrir.c`), whose comment already says the proof must come from outside the
compiler. Not wired up yet.

### Regenerating docs/fallbacks-O1.md

Not tracked -- it is a snapshot that goes stale the moment the census moves. Rebuild by
dumping full bodies (`MCC_LOG=0xff MCC_AST_UNFAITHFUL_DUMP=20000`), disassembling both
legs with `objdump -D -b binary -m i386:x86-64 -M intel --no-show-raw-insn`, and diffing
with **addresses dropped and branch targets rewritten as signed displacements** --
without those two normalisations a body that shifts by a few bytes reads as wholly
rewritten (`parse_include` showed 256 changed lines instead of 42).


---

## Running agents in parallel over one working tree

Three agents were dispatched at once to investigate separate divergences. All three
needed to edit `src/mccrir.c`, and they shared the checkout. What happened:

- Two hit large, disjoint, random full-suite failures -- one saw 14 cells, another 13 --
  caused by `cmake-verify/mcc` being relinked underneath a running `ctest`. Every one
  passed on rerun.
- Each attributed the others' edits to "another session" and reported a census that
  silently included work it had not done. One reported `used=1181 fallback=24` for a
  change worth `31 -> 30` on its own.
- Both worked around it unprompted, one with a throwaway worktree and one with a
  `git stash`/pop cycle. Nothing was lost, but only because they noticed.

Nothing was wrong with the findings; the *numbers* were unreliable, and the combined
tree had to be rebuilt and re-measured from scratch before anything could be believed.

**Give parallel agents isolated worktrees whenever they may touch the same files**, or
serialise them. This is the same live-tree confound that invalidated the first
26-preset matrix run earlier in the same session -- a sweep that reads and builds from
a tree someone else is writing cannot distinguish its own noise from a regression.

---

## Four semantic gaps closed — `72fedcf1`, 2026-08-04

Board: gcc `FAIL 471 -> 449`, `FAILEXE 123 -> 122`; llvm `FAIL 231 -> 226`.
27 `FAIL -> PASS`, zero `PASS -> FAIL`. Gates re-run after rebase onto `0db99c4c`:
ctest 8145/8145, `-O3` fixpoint byte-identical (o1=o2=o3=3059711), tracegate and
schemagate OK, cross/qemu/wine 90/90.

Three of the four handed-down diagnoses held. Two corrections worth recording:

- **C23 does not mean "further arguments are permitted".** The standard's text allows
  them; clang 22 *rejects* `va_start(ap, a, b)`. mcc matches clang and keeps rejecting.
- **Pre-increment on `_Complex` already worked**, and was already bit-exact against gcc.
  Only *post*-increment was broken -- `gv_dup()` on a 16-byte aggregate. Half of a
  diagnosis being right is the normal case; probe both halves before costing the work.

Cluster counts measured on `err_raw` came out `9 / 10 / 15 / 8`, not the `9 / 8 / 8 / 8`
carried forward from the earlier census. The census had been counting normalised `err`.

### The bug that only showed up two backends away

Making post-increment correct meant marking `cplx_local` results `VT_NONLVAL`. That
propagated into `arm64-gen.c` `store()`, which masked only `VT_BOUNDED|VT_REGDISP`
before its *exact* `svr ==` comparisons -- while its own `load()` (line 580) and
`arm64-asm.c` (line 899) also mask `VT_NONCONST|VT_NONLVAL`. Any `VT_NONLVAL` reaching
`store()` fell through to `assert(0)`. The cross tier sat at 88/90 until the mask was
aligned. No other backend is affected; they all use `& VT_VALMASK`.

A latent inconsistency between two functions in one file, invisible until an unrelated
frontend change started producing the value that distinguished them. `ctest` on x86-64
was green throughout.

### `register` arrays: still wrong, deliberately

`register int a[10]; g(a);` -- gcc and clang reject the decay, mcc accepts. Unchanged
by this work because the fix belongs at the array-to-pointer decay site, not at `&`.

Also still open and *not* complex-specific: `const`-qualified **parameter** assignment
(`double f(const double z){ z++; }`) is accepted; the scalar case fails identically, so
this is a general qualifier-on-parameter gap, not a complex one.

`_Atomic _Complex ++` gives a clean "not supported"; gcc and clang accept it.

### One-argument `va_start`: three targets get a diagnostic, not support

Supported on x86_64 SysV, arm64 and riscv64 -- their `gen_va_start` already ignored
`last`. i386, arm and x86_64-PE compute the `va_list` from `&last` and would need
per-function first-vararg-offset tracking in the prologue; they emit a clean
unsupported diagnostic instead of a wrong answer.

i386 is genuinely small: record `addr` after the parameter loop in `gfunc_prolog` as a
`func_va_list_ofs`, then mirror riscv64's `vset(&char_pointer_type, VT_LOCAL, ofs)`.
arm is fiddly -- the first-vararg slot depends on EABI alignment and the `func_var`
core-register spill. **No test on the board exercises either.**

### Structural tag compatibility (8 files) — assessed, not attempted

`compare_types:4165` does `return (type1->ref == type2->ref)` for `VT_STRUCT`: pure
`Sym` pointer identity. C23 structural compatibility needs a recursive member walk with
cycle detection for self-referential tags, composite-type construction so the *result*
carries the union of both member lists' completeness, and a decision about where the
composite `Sym` lives and how it interacts with `patch_type`, `_Generic` and typedef
redefinition. That is a subsystem, not a patch.

It must not be half-done. A structural comparison that stops short of building the
composite would accept the redefinition and then hand the wrong `Sym` to later member
lookups -- the silent-wrong-answer mode, not a loud one.

### Remaining cluster root causes, costed

| Cluster | Files | Cost | Blocker |
|---|---|---|---|
| block-scope `const` scalars never folded | 3 | medium | folded initializer must be stashed on the `Sym`, plus an address-taken bail-out |
| `&&label - &&label` | 3 | **large** | needs symbol-difference relocations in the data path, absent tree-wide |
| file-scope array declarators ICE | 4 | medium | `post_type:8594` `!local_stack` guard must become "size is needed as a value" |
| `fold_const_lval` gated on `global_expr` | 2 | 1 line | widens folding into every constant context; wants its own full sweep |
| vector casts never constant | 1 | medium | |
| `_Complex long double`, complex `==`/`!=` folding | 3 | small | |
| block-scope prototype not dropped by file-scope unprototyped redeclaration | 1 | medium | new `Sym` bit plus a downgrade branch in `patch_type` |

### Process note, again

`runtime/include/` is copied into the build tree by a configure-time `file(COPY ...)` at
`CMakeLists.txt:2931`. Editing a runtime header requires re-running `cmake -S . -B
cmake-release`; `cmake --build` alone silently uses the stale copy. This has now cost
three separate test cycles in this session.

---

## The bitfield side-car — `d50c480d`, 2026-08-04

`CType` is now `{ int t; unsigned char bp, bs; struct Sym *ref; }`. Still **16 bytes**
on LP64, `ref` still at offset 8 — the two fields live in padding the ABI was already
inserting. `BIT_POS`/`BIT_SIZE` were **deleted** from `src/mcc.h` rather than redefined,
so every consumer became a compile error instead of a silent wrong read.

Layout-neutral by construction *and* by measurement: 7,970 differential cases
byte-identical before and after, object output byte-identical on **all eight backends**
over a 1,205-struct corpus, **zero board movement across 47,715 tests**.

### Three premises in the brief were wrong

1. **Bits 20-31 are not all bitfield data.** `VT_UNION`=1, `VT_ENUM`=2, `VT_ENUM_VAL`=3,
   `VT_ASM`=4, `VT_ASM_FUNC`=5, `VT_BT_ARRAY`=6 are a **tag enumeration** sharing bits
   20-22, told apart from bitfield data only by `VT_BITFIELD`. This frees bits **23-31**,
   not 20-31 — and it is why widening `VT_BTYPE` is not a `#define` renumber (below).
2. **The arena is one of six wire formats** carrying a type word, not one: also the
   `MccjitTypeRec`/`MccjitIntent` on-disk intent blob, `RirMark` packing, the
   `rir_castgv_t/ref` statics, `AstInlineFn::param_typ[]`, and the `rir_xt_t[]` snapshot
   cache. `MCCJIT_INTENT_FORMAT` bumped 11→12; the salt is version-string-only and would
   **not** have invalidated stale blobs on its own.
3. **Width-64 bitfields are deliberately not marked `VT_BITFIELD`** (`mccgen.c:6559`)
   because six bits cannot hold 64. That single special case is the source of **all ten**
   pre-existing gcc/clang deviations. `unsigned char bs` removes the ceiling; lifting the
   case is left to a change allowed to move layout.

### The copy-by-`t`-alone hazard was ~100 sites, not 21

- ~100 sites do `CType x; x.t = …; x.ref = …;`, leaving `bp`/`bs` as **stack garbage**,
  not zero. `gfunc_sret` in five backends writes `ret->t`/`ret->ref` without clearing.
- **`sym_push` (`mccgen.c:1354`) copied only `.t` and `.ref`** — dropped every bitfield
  width on the floor. First bug found: `sizeof` went to 0, 7,357/7,970 differential
  failures.
- ~32 sites **reconstruct a `CType` from the arena**. Missing these broke
  `ast/replay-bitfield`: the reemit path rebuilt bitfield types with pos/size zeroed, so
  replayed codegen stopped matching the parser's and every bitfield body silently fell
  back to the slow path.

The fix is an invariant rather than ~100 patches: **`bp`/`bs` are zero unless
`VT_BITFIELD` is set**, enforced at every writer. All 21 readers are already
`VT_BITFIELD`-guarded, so the indeterminate bytes are unobservable. No "non-zero size"
assertion — `int : 0;` is legitimately `VT_BITFIELD` with size 0.

### The fixpoint gate paid for itself

A first attempt added `bp`/`bs` to five identity caches, which read those indeterminate
bytes. `o1 != o2`, 3043679 against 3043583 — a gcc-built and an mcc-built compiler
disagreeing about their own output, which is the uninitialized-memory signature and
nothing else. Four of the five compared **pointed-to** types, which are never bitfields,
and were reverted; only the Sym-keyed `rir_xt_bp/bs` survived.

`ctest` was green for the broken version. Byte-identical self-host is the gate that sees
this class.

### Step 2 (widen `VT_BTYPE` to 5 bits) — worked out, deliberately not landed

Blocked by premise (1): freeing only bits 23-31 means `VT_CONSTEXPR` lands on bit 20 and
**collides with the tag enumeration**, so `VT_STRUCT_SHIFT` must move 20→21 and
`VT_STRUCT_MASK` be re-derived. That is a structural change to a region the brief
described as free, and it wants its own sweep rather than a tail-end append.

Staged and unapplied at `scratchpad/step2.py`: `VT_BTYPE` → `0x001f`, all 16 flags up one
bit, `VT_STRUCT_SHIFT` 20→21, `VT_STRUCT_MASK` → `((1U << 11) - 1) << 21 | VT_BITFIELD`,
`MCCJIT_INTENT_FORMAT` 12→13. Base-type enum **values** are untouched, so the ~130
`(t & VT_BTYPE) == VT_X` tests need no edits. Bits 24-31 remain free afterwards.

`src/mccast.c` carries an `#ifndef VT_BITFIELD` fallback block because `tools/asttool.c`
`#include`s it without `mcc.h`. **It already holds stale `VT_BTYPE 0x000f` and
`VT_LONG 0x0800` and must be updated by step 2.**

### The gate to keep

The **8-backend object comparison** (arm, arm64, riscv64, i386, x86_64, arm64-win32,
i386-win32, x86_64-osx, byte-identical over the struct corpus) is the one worth
carrying forward. It reaches arm and arm64, which the qemu cells cannot — those two are
unbuildable because mcc's own assembler rejects `svc`, which is unrelated and still open.
It should be the **primary** step-2 gate: a renumber must be emission-identical
everywhere, and this says so cheaply where ctest is blind.

### Separate pre-existing bug, landed first as `7a31e90b`

`parse_asm_operands` never initialized `op->is_label`, while the label list a few lines
below memsets its own entries. `find_constraint` maps an out-of-range `%lN` onto the
k-th `+` operand, so `subst_asm_operands` called `get_tok_str()` on uninitialized stack.
The compiler at `40b0432a` **dumps core** on
`llvm-project/clang/test/CodeGen/asm-goto.c`. Nothing to do with bitfields; it surfaced
only because the side-car board run flagged the one status change.

Note a hand-written reduction of that test does *not* reproduce — mcc rejects the
`:: label` spelling with a syntax error before reaching the substitution. The crash
needs the real file's two `+` operands.

## `VT_BTYPE` widened to 5 bits — step 2

`VT_BTYPE` is `0x001f`, all 16 flags moved up one bit, `VT_STRUCT_SHIFT` 20→21,
`VT_STRUCT_MASK` re-derived as `((1U << 11) - 1) << 21 | VT_BITFIELD`,
`MCCJIT_INTENT_FORMAT` 12→13. Base-type enum **values** are untouched, so the ~130
`(t & VT_BTYPE) == VT_X` tests needed no edits — verified by grep, not assumed.
Slots 16-31 are now free for `__bf16`; **no new type was added.**

### The final map, checked mechanically

Bits 0-4 `VT_BTYPE`; 5-20 the sixteen flags in their old order (`VT_UNSIGNED` 5 …
`VT_CONSTEXPR` 20); 21-23 the tag enumeration; 24-31 free. `VT_STRUCT_MASK`
`0xffe00100`, `VT_STORAGE` `0x0013e000`, `VT_TYPE` `0x000c1eff`.

`VT_TYPE` keeps exactly the nine flags it kept before (`VT_UNSIGNED`, `VT_DEFSIGN`,
`VT_ARRAY`, `VT_CONSTANT`, `VT_VOLATILE`, `VT_VLA`, `VT_LONG`, `VT_ATOMIC_BIT`,
`VT_NULLPTR`) — so its users' assumptions still hold.

The no-overlap check is a script, not an eyeball: it compiles a probe against the
real `mcc.h` once per target and asserts `VT_BTYPE` is a contiguous low mask, every
base-type value fits it, the sixteen flags are sixteen distinct single bits disjoint
from it, the tag region is disjoint from both, and `VT_STRUCT_MASK` covers the tag
region plus `VT_BITFIELD` and nothing else. It reports zero errors on the new map
and also passes on the **old** map, which is what makes it trustworthy.

### The `(1U << (6 + 6))` in `VT_STRUCT_MASK` was a live trap

It is a leftover from when bitfield pos/size lived at bits 20-31. At shift 21 the
12-bit mask evaluates to `0x1FFE00000`, which **truncates to `0xFFE00000` in a
32-bit `int`** — silently dropping the top bit and aliasing struct/enum ids. The
width must go to 11 in the same edit as the shift; they are not separable.

### `riscv64` packed the base type into a hardcoded 4-bit field

This is the one place a renumber broke real codegen, and it is invisible to a green
x86-64 ctest. `reg_pass_rec` packs `fieldofs[k] = (ofs << 4) | btype`, and
`gfunc_call` re-packs both into one `int info[]`: btype#1 at bits 12-15, btype#2 at
16-19, offset at 20+. With `VT_BTYPE` at `0x1f` the decoders `(ii >> 12) & VT_BTYPE`
and `(ii >> 16) & VT_BTYPE` reach one bit into the *neighbouring field* — wrong
immediately, for existing base types, not only for future ones.

Fixed by widening the packing to 5 bits throughout (`<< 4`/`>> 4` → `<< 5`/`>> 5`,
btype#2 to bit 17, offset to bit 22) and making `info[]` `long long`, because at
shift 22 an 11-bit offset would overrun bit 31. The alternative — pinning those
sites to a literal `0xf` — was rejected: it re-creates the same landmine exactly
where the next base type will step.

`x86_64`, `arm`, `arm64` and `i386` have no equivalent packing; riscv64 is the sole
occurrence.

### `mccast.c`'s fallback block now fails loudly

`tools/asttool.c` `#include`s `mccast.c` **without** `mcc.h`, so the `#ifndef`
fallbacks are live in that build and dead in the compiler build. They were *not*
stale at `ff517a43` — all six matched `mcc.h` exactly — but nothing enforced that,
and a renumber that missed them would give two builds with different type encodings
and **no compile error**. Each fallback now carries an `#else` arm asserting the
literal against the real macro (`typedef char ..._check[X == lit ? 1 : -1]`, the
idiom already used at `mccast.c:2369`). Verified by deliberately corrupting one:
the compiler build fails with `size of array is negative`.

Note `VT_VALMASK`/`VT_LOCAL`/`VT_SYM` in that block are the **`SValue.r` namespace**
and must not move; only `VT_BITFIELD`, `VT_LONG`, `VT_BTYPE` did.

### Wire formats: only the JIT intent blob needed action

Of the six, five carry the type word at full 32-bit width with `bp`/`bs` in separate
fields and never outlive the process — the AST arena (`int32_t type_t`),
`rir_castgv_*`, `AstInlineFn::param_typ[]`, `rir_xt_t[]`, and `RirMark` (whose one
bit-pack, `rir_mark_vla`, gives the type word the entire low 32 bits of a 64-bit
slot). The three `uint32_t` AST hash folds do not truncate and nothing persists a
hash against a golden constant.

The intent blob is the exception and it is genuinely cross-run: `--embed-jit` bakes
it into `.data` with a constructor that re-deserializes it at program start. Its
`salt` is written but **never compared** — confirmed, the only salt equality check
in the tree is the KGC header — so `MCCJIT_INTENT_FORMAT` is the sole invalidation
lever. Verified end to end: a blob extracted from a binary built at `ff517a43`
carries `format=12`, the new compiler stamps `13`, and the new reader returns
`deserialize_rc=-1` / `peek_warm=0` on the old one. Rejected, not misread.

### Gates

Build clean (0 warnings). **8-backend object comparison: 768 cases
(8 targets × {-O0,-O2} × 48 files), byte-identical**, corpus extended past structs
to every base type, `_Float16`, `__int128`, `_Complex`, `long double`, `_Bool`,
vectors, VLAs, function pointers, atomics, `constexpr`/`nullptr`, and the whole tag
enumeration including inline asm. `ctest` 8145/8145; cross `ctest -L
"cross|qemu|wine"` 90/90; `tracegate`/`schemagate` clean; fixpoint byte-identical at
o1=o2=o3=**3076911** (baseline 3076623 — a stable new size, as expected when the
compiler's own source changes). Board: 47,715 tests, **one** status change,
`gcc.dg/flex-array-counted-by-pr121000.c` FAILEXE→PASS — and that object is
**byte-identical** between the two compilers while the single binary passes 17/30
and segfaults 13/30, so it is the known flake and not movement.

The object comparison is only worth what it exercises: introducing a deliberate
one-bit error in the riscv64 decoder changes **10** corpus files, so the
byte-identical result is a real result and not a vacuous one.

### Two pre-existing bugs found, neither touched

1. **`riscv64` aborts on every mixed int/float two-register struct.**
   `arch_transfer_ret_regs` asserts `vtop->r == (VT_LOCAL | VT_LVAL)` and it fails
   for `{int,float}`, `{float,int}`, `{double,int}`, `{int,double}`, `{long,float}`,
   `{float,long}` and nested `{int}{double}` — 7 of 37 shapes, at `-O0` and `-O2`.
   **Identical at `ff517a43`**, so it predates the renumber. The other 30 shapes are
   byte-identical. Worth its own task: this is the `prc[1] != prc[2]` return path,
   and an assert means every such struct return is a hard abort, not a miscompile.
2. **`arm64` `store()` still does not strip `VT_MUSTCAST`.** `load()`
   (`arm64-gen.c:580`) masks `~(VT_BOUNDED|VT_NONCONST|VT_NONLVAL|VT_MUSTCAST|VT_REGDISP)`
   before its `svr ==` comparisons; `store()` (`:782`) masks the same set **minus
   `VT_MUSTCAST`**, so a value carrying it takes the fallback path in `store` while
   `load` matches. This is the same class as the bug that cost a cross-tier failure,
   one mask narrower. `SValue.r` is a different namespace from the type word and was
   not renumbered, so this is unrelated to step 2 — but it is still wrong.

---

## qemu-user actually reaches arm — the "assembler rejects `svc`" claim was half wrong

Two cells that had **never run** now run and pass, in about a second each:

| Cell | Was | Why it skipped | Now |
|---|---|---|---|
| `run-parity-arm` | Skipped | wanted `arm-linux-gnueabihf-gcc` and `/usr/arm-linux-gnueabihf` | **Passed** |
| `run-parity-arm64` | Skipped | gated on the *host* being aarch64 | **Passed** |

Both were skipping while `vendor/gentoo-stage3-$arch-glibc`, `cmake-cross/mcc-$arch` and
`qemu-arm`/`qemu-aarch64` were all present. `tools/selfhost-run-parity.sh` now bootstraps
the target-hosted mcc with mcc itself when the preferred cross compiler is missing
(`4e0f...`). arm produces `acc=2075865568`, the 32-bit-`long` value only an arm host
gives — the cell is genuinely exercising arm, not silently falling back to x86.

`tools/qemu-selfhost.sh arm -O1` also passes end to end: **s2 == s3 fixpoint at 798931
bytes**, s3 executes. The arm sysroot has been in `vendor/` the whole time.

### The claim that was wrong

Recorded earlier as "arm and arm64 are unbuildable — mcc's own assembler rejects `svc`".
Tested directly:

- **arm: `svc` assembles fine.** `__asm__ volatile ("mov r7, #1\n\tmov r0, #42\n\tsvc #0")`
  compiles with `mcc-arm -nostdlib -static` and exits 42 under `qemu-arm`. What actually
  fails is **explicit register variables** — `register long r7 __asm__("r7")` gives
  `compiler error! register 7 is no int register` from `arm-gen.c:267`. A different bug
  in a different place, and one that does not block freestanding arm testing at all,
  because a plain asm string works.
- **arm64: `svc` genuinely is not implemented** — `ARM64 instruction 'svc' not
  implemented`. That half of the claim holds.

The lesson is the session's recurring one: the *symptom* was "freestanding test won't
build on arm", and the first-guess cause survived into the record without being probed
on its own.

### riscv64 `-run` segfaults on any input — new, and previously invisible

`run-parity-riscv64` has always skipped for want of `riscv64-linux-gnu-gcc`. With the same
vendored-sysroot bootstrap, the riscv64-hosted mcc **works**: `mcc -v` runs, and it
compiles and links `hi.c` into a binary that prints `sum=10` under `qemu-riscv64`.

But `-run` segfaults. On `int main(void){return 3;}`. Under **both** `MCC_JIT=0` and
`MCC_JIT=1`, so it is not the JIT tier — it is the in-memory execution path itself.
Exit 139. arm on the identical input returns 3.

The fallback is deliberately **withheld** for riscv64 so the tree does not go red on a
pre-existing bug, and the skip message names the reason instead of blaming a missing
compiler. Flip it on in the same change that fixes `-run`.

### Three open bugs this exposed

1. **riscv64 `-run` segfaults** on any input, both JIT tiers. Compile-and-link is fine.
2. **arm64 assembler has no `svc`** — blocks freestanding syscall tests on arm64.
3. **arm rejects explicit register variables** in inline asm (`arm-gen.c:267`), so the
   ordinary `register long r7 __asm__("r7")` syscall-wrapper idiom does not compile.

---

## All three landed — and two of them were the same shape of bug

### 1. riscv64 `-run`: a ±2 GB call with no range check and no veneer

`-run` maps the program at whatever `host_runmem_alloc` returns (0x6e9000 in the repro)
while the libc it calls lives wherever the loader put it (0x7fb854bf5aa8). `R_RISCV_CALL`
/ `R_RISCV_CALL_PLT` is `auipc`+`jalr`, a **signed 32-bit** PC-relative pair, and
`riscv64-link.c` wrote it with **no range check at all**. 256 GB of displacement was
silently truncated and the program jumped into nothing.

`build_got_entries` (`mccelf.c:1316`) refuses to create a PLT entry when
`output_type == MCC_OUTPUT_MEMORY`, so there is nothing to bounce off: in `-run` every
external call must reach the real `dlsym` address directly. arm and arm64 already solve
this with `arm_veneer_memory_calls` / `arm64_veneer_memory_calls`, which rewrite every
call against an `SHN_UNDEF`/`SHN_ABS` symbol to point at a synthesized `.mcc.veneer`
entry that loads a full-width address and jumps. riscv64 had no equivalent — the whole
mechanism was arm-only, and nobody noticed because riscv64 `-run` had never run.

`riscv64_veneer_memory_calls` now does the same with `auipc t1,0 / ld t1,16(t1) /
jr t1 / nop / .quad target(R_RISCV_64)` (encodings checked against `llvm-mc`), and
`R_RISCV_CALL` gained the range check it never had, so the next time this breaks it says
so instead of segfaulting. `int main(void){return 3;}` returns 3; `hi`/`hot` match at
`acc=998508278240` under both JIT tiers. The `need_fallback` withhold in
`tools/selfhost-run-parity.sh` is gone and **`run-parity-riscv64` Passes**.

The x86_64 arm of the same switch does have the check (`x86_64-link.c:218`) and errors
out. Only riscv64 was silent.

The signature does not move across the `VT_BTYPE` widening (`7bded795`). Re-checked by
keeping the new range check and stashing only the `mccrun.c` call site: same symbol,
same displacement — `'fflush' is 140160382986144 bytes away (val=7f79a2457aa8,
addr=6e9308)`. Nothing here touches the riscv64 struct packing that commit rewrote.

### 2. arm64 `svc` — the tokens existed, the encoder did not

`DEF_ASM(svc/hvc/smc/brk/hlt)` were already in `arm64-tok.h:454`; `asm_opcode` simply had
no case for them, so they fell to the `not implemented` default. Added `asm_exception` +
`gen_exception` next to `asm_barrier` (the same operand-parsing shape) and the five
`ARM64_*` base words. All twelve forms tested disassemble byte-identically to `llvm-mc`;
`svc` exits 42 under `qemu-aarch64` and `brk #1` raises SIGTRAP.

### 3. arm register variables — a machine/treg numbering conflation, not a missing class

The diagnosis in the heading above was half right. arm's asm layer is numbered in
**machine encoding** throughout — `subst_asm_operand` prints `TOK_ASM_r0 + reg`,
`asm_clobber` and `regs_allocated[13]` index by machine number, the `'r'` scan walks
`0..8`. But `asm_gen_code` fed those numbers straight into `load()`/`store()`, which take
**codegen tregs**, and arm only has tregs for r0-r3, r12, sp and lr. So the two
numberings agreed by accident for r0-r3 and diverged for everything else.

That makes the reported symptom the *narrow* case. The wider one: **six `"r"` operands
already failed on plain arm** — no register variables involved — because the allocator
handed out machine r4 and up:

```c
__asm__ volatile ("add %0, %1, %2\n\t..." : "=&r"(o)
                  : "r"(a), "r"(b), "r"(c), "r"(d), "r"(e), "r"(f));
```
gave `compiler error! register 5 is no int register`. And machine r4 was *worse* than an
error: `intr(4)` is `MCC_TREG_R12`, so `load(4, …)` silently loaded into **r12** while
`%4` printed `r4`.

The fix keeps the asm layer machine-numbered (everything else already assumed that) and
translates at the one boundary that needs tregs: `arm_asm_load`/`arm_asm_store` use the
treg directly when one exists and otherwise move through a scratch treg picked from
`{ip, r3, r2, r1, r0}` avoiding the operand registers. `*pout_reg` — used as an address
base, so it must be a real treg — is now chosen from `{r0..r3, r12}` rather than `0..8`.
`arm_parse_regvar` and `mccgen.c:16169` were left alone: r7 already passed the
`< MCC_NB_REGS` guard, so nothing upstream of `asm_gen_code` was ever wrong.

### Freestanding syscall matrix, both idioms, all under qemu

| target | plain asm string | `register long x __asm__("…")` |
|---|---|---|
| arm | exit 42 | exit 42 (was `register 7 is no int register`) |
| arm64 | exit 42 (was `'svc' not implemented`) | exit 42 |
| riscv64 | exit 42 | exit 42 |
| i386 | exit 42 | exit 42 |

riscv64 and i386 needed no work: riscv64 already has the `ASM_REGVAR_ASMREG` hook
(`rv_regvar_asmreg`) that arm was missing the equivalent of, and i386's tregs are its
machine numbers.

### Sweeping the exec corpus through riscv64 `-run` leaves exactly one real defect

`hi`/`hot` are two programs. Running all 278 `tests/exec/**.c` through riscv64 `-run` and
diffing against the same source under the native `-run` gives **269 identical, 9 different**
— and re-running those 9 through riscv64 *compile-and-link* sorts them completely:

| case | verdict |
|---|---|
| `al_ax_extend`, `asm_constraints_x86`, `asm_goto`, `asm_lvalue_cast`, `asm_operand_modifiers`, `int128` | fail the same way at compile-and-link — x86 mnemonics, x86 constraints, `__int128`. Not `-run`. |
| `arch/riscv_asm.c` | native prints `SKIP`, riscv64 actually runs it. Correct. |
| `types/char_signedness.c` | `255 0` vs `-1 1` — riscv64's `char` is unsigned. Correct. |
| `features_c99_c11/tls.c` | **the one real one.** Links and runs correctly as a binary (`42 0 42 0 100 200 42 0`); under `-run` every value is `0`. |

So the veneer closes the whole relocation class, and TLS is the single remaining `-run`
defect on riscv64 — with a corpus test that already catches it.

### The one thing left open: `-run` TLS is broken on **x86_64 too**, not just riscv64

Chasing that last corpus difference produced the more interesting result. It is not an
arm/riscv64 gap — the **host** target has it. `tests/exec/features_c99_c11/tls.c`, same
compiler, same source, two invocations:

```
$ mcc -o tls tls.c && ./tls        $ mcc -run tls.c
42                                 42
0                                  0
42                                 0        <-- diverges here
0                                  42
100                                0
200                                rc=1
42
0
rc=0
```

So `-run` gets thread-local storage wrong on x86_64, and riscv64 (all zeros) and arm are
worse rather than uniquely broken. `host_run_tls_slab_tpoff` (`mcchost.c:1449`) only reads
the real thread pointer on `__x86_64__` and `__aarch64__`, and of the four backends only
x86_64 and arm64 consult `s1->run_tls_active` / `run_tls_slab_tpoff` in their TPREL
relocations at all — but consulting it is evidently not sufficient either.

Left alone deliberately: it is pre-existing, it is not the segfault, and it is not
riscv64-specific, so folding it into this change would have hidden it. What makes it
worth its own task is that **no gate catches it**. `run-parity` runs exactly two programs
(`hi`, `hot`), neither uses TLS; `tls.c` is in the exec corpus but the corpus is run
compile-and-link, where it passes. A `-run` leg for `tls.c` — on the host, before any
cross target — is the first step, and it should go red immediately.

(arm additionally prints `fsum=0.000` under `-run` where riscv64 correctly gets `10.000`,
so arm `-run` has a soft-float problem on top of the TLS one. Same test, same command,
unrelated to everything above.)

## `run-tier/<triple>` — `-run` on all twelve triples, and the TLS red it was built to find

`tools/run-tier.sh <triple> <bdir> <xdir>` bootstraps a **`<triple>`-hosted** mcc and runs
the fourteen-program corpus in `tests/run/` through it under both `MCC_JIT=0` and
`MCC_JIT=1`, asserting `JIT=0 == JIT=1 == expected`. Registered as `run-tier/<triple>` for
every `MCC_X` triple. Emulated cells are labelled `qemu`/`wine`/`macho`, never `cross`, so
they land in the `ctest -LE native` set the stage2 emulate job runs.

| Cell | Runner | 12 non-TLS | `tls` (single-threaded) | `tls_threads` |
|---|---|---|---|---|
| `x86_64` | native | pass | pass | **FAIL** |
| `arm64` | qemu-aarch64 | pass | pass | **FAIL** |
| `i386` `arm` `riscv64` | qemu-user | pass | **FAIL** | **FAIL** |
| `x86_64-win32` `i386-win32` | wine64 / wine | pass | **FAIL** | **FAIL** |
| `x86_64-osx` `arm64-osx` | Darwin only | skip on Linux | | |
| `arm64-win32` `arm-win32` `arm-wince` | none anywhere | `mcc_skip_test` | | |

**riscv64 passes all twelve non-TLS programs** after the veneer fix above — relocations
across translation units, function pointers, a 1 MB `.bss`, `setjmp`/`longjmp`, structs by
value at every size that straddles the register/memory boundary, and mixed int/double
varargs. arm passes `fp_double` and `fp_longdouble` too; the `fsum=0.000` soft-float
symptom recorded above does **not** reproduce through a hardfloat-built arm mcc.

### The TLS red, refined into two distinct defects

The corpus splits `-run` TLS into a single-threaded case and a threaded one, and they fail
on different sets of targets — so they are two bugs, not one.

- **`tls_threads` fails everywhere, including x86_64 and arm64.** The main thread is right;
  a thread created with `pthread_create` reads **0** from every initialized `__thread`
  variable (`child_init=0` where the linked binary prints `42`). Writes inside the child
  work, and the main thread's values survive the join. So the run TLS slab exists but is
  never seeded for threads other than the one that called `mcc_run`.
- **`tls` — no threads at all — additionally fails on `i386`, `arm`, `riscv64` and both PE
  targets.** There the `.tdata` initializers are lost in the *main* thread:
  `init=0 static=0 aux=0 auxpriv=1` where the linked binary gives `42 9 7 4`. Zero-init
  TLS, writes and reads-back are all correct. This matches `host_run_tls_slab_tpoff`
  (`mcchost.c:1449`) only reading the real thread pointer on `__x86_64__`/`__aarch64__`.

Both are pre-existing, both are identical under `MCC_JIT=0` and `MCC_JIT=1` — so neither is
a JIT-tier bug — and the AOT compile-and-link path is correct on every one of these targets.
The cells are **deliberately red**; fix `-run` TLS, do not weaken the corpus.

### What a PE-hosted mcc needed, since nothing had ever built one

`mcc-x86_64-win32` compiles `src/mcc.c` into a working `mcc.exe` in under a second, and
`wine64 mcc.exe -run prog.c a b` works, argv and all. `pthread_create` works under wine too.
Three things were not obvious:

- **`-I runtime/win32/include/winapi` on top of `-I runtime/win32/include`.**
  `src/mcchost.h` includes `<windows.h>`, which lives only in the `winapi/` subdirectory.
- **Every `src/arch/*` directory on the include path**, not just the target's:
  `mcctok.h` includes `i386-tok.h` unconditionally.
- **`-L$B` as well as `-B$B`.** `mcc_add_support("runmain.o")` resolves through
  `mcc_add_dll` → `s->library_paths` (`src/libmcc.c:1615`), which `-B` does not populate.
  Without it the run still *executes* and prints the right answer, but mcc reports
  `file 'runmain.o' not found` and exits **5** — correct stdout behind a red exit code,
  the shape that hides in a test which only diffs output.

The rest is `suite_pewine`'s recipe (`tools/mccharness.c:2318`): `runtime/win32/lib/*.def`
plus `cmake-cross/lib-<t>/*.o` plus `<t>-libmccrt.a` staged into the `-B` directory.

### Two traps the corpus is built around

- **`long` is 32-bit on `arm` and `i386`.** The corpus uses `<stdint.h>` fixed-width types
  throughout and never prints a `long`, a pointer or a `size_t`, so one `.expected` file per
  program is correct on all twelve triples — no per-arch constant bank. `long double` is
  exercised as an argument and return type, but every value is exactly representable in
  `double` and printed as one, so the 80/128/64-bit split never reaches the golden.
- **Wine writes CRLF.** The driver strips `\r` before comparing. Without that every PE cell
  fails with a `want` and a `got` that render identically in the log.

The driver also asserts the `mcc -v` target string (`(AArch64 Linux)`, `(i386 Windows)`, …)
before running anything, so a runner silently falling back to the host fails loudly instead
of passing a cell it never tested.

### Open

- `x86_64-osx` / `arm64-osx` skip on Linux and the Darwin branch of `run-tier.sh` has
  **never executed**. It mirrors the `tools/c2_equiv.sh:61-81` host gate and the Mach-O
  bootstrap shape; treat its first run on a Mac as unproven.
- The `x86_64` cell uses the build directory's own `mcc` rather than re-bootstrapping one.

## The stage2 red after the config refactor: five failures, four causes

The a55c0a07/965a516e push turned every stage2 job red. All five failing tests trace to
two consequences of the refactor plus two harness typos, fixed as follows.

### Flags stranded in environment position (three harness sites)

`MCC_AST_*=1 mcc` recipes were converted to `-f` flags mechanically, and three landed
where the environment variable used to sit — *before* the executable — so `sh`/`env`
tried to exec `-fopt-slice` as a program: `cli/slice_eligible_set`, `cli/per_fn_config`
(`tests/cli/cases.h`), and `superopt/promote-floor` (`${pin:+-fpromote-locals}` before
`"$MCC"`, which reported as "-O13 (pinned) build failed"). The flags moved onto mcc's
argv. A retired env var fails silently, but a flag in env position fails loudly —
grep any future conversion for `-f[a-z-]* [A-Z_]*=` and `\${[a-z]*:+-f` before pushing.

### `MCC_DIAG` now reaches the main binary, so error-path leaks became test output

The old `MCC_ALL_DIAGNOSTICS` put `MEM_DEBUG`/`SYM_DEBUG` only on `mcc_s`; `MCC_DIAG=1`
goes into `_mccdefs` and instruments the `mcc` every test runs. 279 diagnostics-job
tests failed, every one on the same shape: a test that *expects* compile errors got
`mcc: mem_leak= N bytes` appended, because `mcc_error` longjmps past cleanup by design
(`assoc_types` in `_Generic`, per-`Sym` allocations under `MCC_DIAG`'s individual
`sym_malloc`, tal chunks, ...). **Policy, not fix**: `mcc_leakcheck_quiet` is set when
any compile error is diagnosed and both leak reporters stay silent for that window
(`libmcc.c` `mcc_memcheck`, `mccpp.c` `tal_delete`); clean compiles keep full leak
checking, which is where a regression would matter. The suppressed error-path leaks
are real but bounded (the longjmp path frees `stk_data` slots only); auditing them
one by one is open, low priority.

Four shapes the error window did NOT cover, found on the next run. Two were real
defects the instrumentation caught, not report noise:

- **`-fmacro-eval` freed across allocators.** `host_spawn_ex` hands captured stdout
  to `pp_macro_eval`, which frees it with `mcc_free`; the POSIX `host_slurp_fd` sat
  in the libc `push_macro` region and allocated with plain `malloc` (the Windows
  pipe reader already used `mcc_malloc`). Under MCC_DIAG that was an instant
  "mcc_free check failed" `exit(1)` (`cli/macro_eval_recursive`); on any build it
  was a cross-allocator free. The slurp goes through the mcc allocator now.
- **`ir_cap_teardown` missed `ir_cap_raw`.** The teardown that exists precisely to
  stop capture-cache leak reports freed `ops`/`vs`/`fc` but not the raw byte
  buffer — 4096 bytes at `mccircap.c` on `exec-search*/led`, `grep`,
  `translation_limits`, `switch_semantics`.

- **`mcc_run_tls_seed` really leaked.** The thread-seed snapshot from `26881521` is
  module-level (so `mcc_run_thr_start` can reach it) and nothing state-owned freed
  it — every `-run` of a TLS-using program reported 8 bytes at `mccrun.c` under
  MCC_DIAG (`exec/builtins`, `bound_global`, `bound_test_b`; runtime bound errors
  are `rt_printf`, not `mcc_error`, so no error taint). `mcc_run_free` now frees it.
- **Embed-JIT recompiles make the window meaningless.** Recompile states nest inside
  the running state and their intent/KGC registries are retained for the process
  lifetime; `exec-zerobss/run_atexit` dumped 538,880 "leaked" bytes and then a
  *negative* `mem_leak=` from the overlap. `mccjit_recompile_common` now taints the
  window like a compile error does. A per-state (rather than process-window) leak
  accounting would measure through JIT activity honestly; open, low priority.

### Self-hosting under `-run` and the TLS slab arithmetic

`selfhost-jit` died with `mccrun: TLS size 65976 exceeds -run slab`. The arithmetic:
a `-run` guest's whole TLS must fit the host's 64K `mcc_jit_tls_slab`, and a guest
*mcc* carries its own equally-sized slab **plus, now, the optimizer's ~440 bytes of
per-worker `MCC_OPT_TLS` gates** — before the refactor the inner mcc was compiled
without `MCC_CONFIG_OPTIMIZER`, so its TLS was the slab alone, 65536 exactly, and the
`>` check passed on the knife's edge. No fixed slab size can host a same-sized guest
with any extra TLS, so `MCC_JIT_TLS_MAX` is now `#ifndef`-overridable and the two
self-host harnesses (`tools/selfhost-jit.py`, `tools/i386jit-selfhost-docker.sh`)
compile the *inner* mcc with `-DMCC_JIT_TLS_MAX=4096` — the inner never hosts a `-run`
guest of its own, so its slab only has to exist. Any new harness that `-run`s
`src/mcc.c` needs the same define or it will fail the same way.

### Not mine: `tls_threads` and the i386 run slab

`run-tier/x86_64`'s last red program was fixed upstream in `26881521` (seed the slab
in program-created threads; wire `R_386_TLS_LE` to `run_tls_active`) — 14/14 both
JIT tiers. arm, riscv64 and the PE triples still lack run-mode TLS.

## CLOSED: arm64-Windows dist self-host -- three chained PE/import defects

Verified green on `windows-11-arm` (run 31007869579): stage2 build rc=0, full
self-hosted toolchain installed, and the CI-produced `mcc.exe` imports no `_getpid`
with all four TLS-directory fields relocated. The dispatch-only `arm64-crash-debug.yml`
has been deleted; no `dist.yml` change was needed (every fix is in mcc's own source, and
the transient "Access is denied" scanner flake did not recur on the clean runs).

Nightly Dist (30986437707) went red on `windows-arm64-msvc` and `windows-arm64-mingw`,
green on Aug 2 (30738246110, head `89a9103d`). stage1 (msvc/mingw-built) mcc is fine
and builds all of stage2 including `mcc.exe`; the **stage2 mcc -- mcc's own arm64-PE
output -- failed on every invocation**, instantly and input-independently, so it is a
startup/image defect, not a codegen-of-a-construct defect. The config refactor is what
exposed it: it compiled the optimizer's `_Thread_local` gates into every build, giving
the stage2 mcc a PE **TLS directory** for the first time (the green-era arm64 mcc had
none, which is why it loaded).

**How this was debugged from an x86_64 host** (reusable -- there IS arm64 coverage now):
- `.github/workflows/arm64-crash-debug.yml` is a **dispatch-only** workflow on
  `windows-11-arm`: it rebuilds the dist, lets stage2 fail, and captures diagnostics
  (procdump `-e 1 -f` first-chance dump; cdb `.exr/.cxr/k`; `Get-WinEvent` on the
  CodeIntegrity/AppLocker/Defender logs; `FLG_SHOW_LDR_SNAPS` loader snaps). It uploads
  `cmake-stage2-release/mcc.exe` as artifact `arm64-crash-dump`. **Delete this workflow
  once the cell is green.** `gh run download <id> -n arm64-crash-dump` then read the PE
  offline with `llvm-readobj --coff-basereloc/--coff-tls-directory/--coff-imports`.
- **wine-arm64 under emulated docker runs arm64-PE mcc** (`docker run --platform
  linux/arm64 debian:bookworm-slim`, `apt-get install wine`): `mcc.exe -v` works, so it
  is a live loader. BUT every wine leg is too permissive to reproduce these bugs -- it
  ran all three broken images. Use it to confirm a *fix* loads, never to reproduce a
  failure. (This corrects `run-tier.sh`'s "no Windows-on-ARM emulator" note; the wine
  legs still fail there on the separate PE `-run` TLS defect.)
- A cross `mcc-arm64-win32` builds on this x86_64 host: `cmake -B <d> -DMCC_TARGET_ARCH=arm64`
  then compile `src/mcc.c` with `-DMCC_TARGET_ARM64=1 -DMCC_TARGET_PE=1 -DMCC_EMBED_JIT=1
  -DMCC_JIT_DEFAULT=1` and the full `-I src/arch/*` set. That image is what to inspect.

**FIXED -- defect 1, the crash (`55e3e561` + `fde63525`).** `pe_set_tls` writes four
absolute VAs into `IMAGE_TLS_DIRECTORY` (Start/EndAddressOfRawData, AddressOfIndex,
AddressOfCallBacks) at header-write time, and nothing base-relocated them. On an
always-ASLR host `ntdll!LdrpAllocateTlsEntry` dereferenced preferred-base addresses and
faulted before `main` -- the 0xC0000005 (cdb confirmed the faulting frame and that
`x9` held the unrelocated `AddressOfIndex`). x86_64 Windows hid it by granting mcc its
preferred base. First fix emitted a *separate* `.reloc` block for the directory; but it
lives in a `.data` page that already had a block, and a **duplicate page RVA** makes the
arm64 loader reject the image with `ERROR_BAD_EXE_FORMAT` ("%1 is not a valid Win32
application" -- that was the second symptom). `fde63525` instead registers the four
fields as ordinary `REL_TYPE_DIRECT` relocs in `pe_add_tls`, so `pe_build_reloc` merges
them into the existing page block. Verified offline: 25 blocks, zero duplicate pages, all
four fields at `tlsdir+0/8/16/24` relocated; and an x86_64 build force-loaded via
`LoadLibraryEx` relocates off its preferred base and still compiles.

**FIXED -- defect 2, the import (`f0eca60a`).** With the TLS fix the image LOADED and
then failed at static import resolution: `0xC0000139` STATUS_ENTRYPOINT_NOT_FOUND on
`-c`, `0xC0000135` on `-v`. The `FLG_SHOW_LDR_SNAPS` run (31007294903) named it exactly:
`LdrpReportError: Locating export "_getpid" for DLL "...mcc.exe" failed`. **arm64
`msvcrt.dll` does not export `_getpid`**; a static import of it makes the whole image
unloadable (LdrpSnapModule raises before `main`). The only Win32 use was the JIT
perf-map filename in `mccjit_perf_map_path` (`src/mccjit_embed.c`); switched to
`GetCurrentProcessId` (a kernel32 export on every arch, already imported). The
`mccstats.c` `getpid()` calls are `MCC_HOST_POSIX`-gated and never reach the PE.
Verified the emitted arm64 image no longer imports `_getpid` and still loads under
wine-arm64.

**Status:** all three fixed and confirmed green on real hardware (run 31007869579,
stage2 rc=0). The debug workflow is deleted. If a fresh-binary "Access is denied" flake
recurs on that runner (it did once, transiently, 370ms after the link -- realtime
scanner, not the image), add `Add-MpPreference -ExclusionPath $env:GITHUB_WORKSPACE` to
the `dist.yml` windows job; it was not needed on any clean run.

General lesson for arm64-PE: the arm64 CRT is leaner than x64's. Any msvcrt import mcc
emits must exist there; prefer a kernel32 equivalent (`GetCurrentProcessId`,
`GetSystemTimeAsFileTime`, ...) over an msvcrt CRT helper on the Win32 path. The import
emitter is `pe_build_imports` in `src/objfmt/mccpe.c`; runtime import decls live under
`runtime/win32`. The `FLG_SHOW_LDR_SNAPS` + cdb recipe in `arm64-crash-debug.yml` names
any future missing export directly.

Coverage note: the Dist nightly and that debug workflow are the only arm64-Windows
signals; `run-tier`'s `arm64-win32` row is `mcc_skip_test` (no in-tree runner). Wiring
the wine-arm64-under-docker path into a real cell would give a cheap always-on gate for
loader/format bugs (defects 1 and 3) -- it would NOT have caught defect 2, since wine's
msvcrt exports `_getpid`.

## Feature parity measured against the other compiler: 47 board failures closed

The brief was "run the C tests in `~/Projects/gcc` and `~/Projects/llvm`, use clang as
the oracle for the gcc suite and gcc as the oracle for the llvm suite, and make mcc
behave the way the other compiler does." That framing is what makes the boards
actionable, because it splits the failure column into work and noise:

| status | meaning | in scope? |
|---|---|---|
| `FAIL` | mcc rejects, the oracle accepts | **yes** |
| `XPASS` | mcc accepts, the oracle rejects (missing diagnostic) | **yes** |
| `REFFAIL` | both reject -- the test wants the *other* compiler's extension | no |
| `XPASS_REFOK` | both accept -- the test's `dg-error` is compiler-specific | no |
| `REFSKIP` | the oracle could not be run at all (see below) | no |

Measured at `dca5a85f` with `tools/xsuite.py --opt=-O0`, one run per suite so each gets
its own `--ref`:

```
tools/xsuite.py --mcc cmake-release/mcc --out <d> --gcc  ~/Projects/gcc  --opt=-O0 --ref clang
tools/xsuite.py --mcc cmake-release/mcc --out <d> --llvm ~/Projects/llvm --opt=-O0 --ref gcc
```

| board | before | after | delta |
|---|---:|---:|---:|
| gcc suite `FAIL` | 450 | 378 | **-72** (44 fixed, 28 reclassified) |
| gcc suite `XPASS` | 282 | 259 | -23 (reclassified) |
| gcc suite `PASS` | 17,188 | 17,229 | +41 |
| llvm suite `FAIL` | 109 | 82 | **-27** (3 fixed, 24 reclassified) |
| llvm suite `PASS` | 1,969 | 1,971 | +2 |

`ctest` 8318/8318 with six new exec goldens, all six `diff3`-clean against gcc 15 and
clang 22.

### The harness was counting 79 tests the oracle could not judge

`ref_verdict` already returned `"badflag"` when the reference compiler rejected the
*flags* rather than the code -- and the caller then ignored it, leaving the row scored
as an mcc failure. 79 rows were in that state: 52 in the gcc board, 27 in the llvm
board. The largest family is clang's `_Accum`/`_Fract` fixed-point tests, which pass
`-ffixed-point`; **gcc has no fixed-point support on x86_64 at all**, so those 17 rows
were never a parity gap. They are now `REFSKIP` and are excluded from the denominator
the same way a directive skip is. Any board that shows a big one-day drop in `FAIL`
should be checked against this: 51 of the 99 rows this change moved were bookkeeping,
not compiler work, and the table above keeps the two apart deliberately.

### What was actually fixed

Each of these is accepted by both gcc and clang and was rejected by mcc.

1. **C23 digit separators** (`1'000`, `0x1'23`, `314'159e-0'5f`). The pp-number scanner
   absorbs `'` between digits and diagnoses the three invalid placements the standard
   names (`adjacent`, `after base indicator`, `outside digit sequence`). The subtle
   part: a separator must stop `e`/`E` from starting an exponent, because the C23
   grammar consumes the `e` in `0x0'e` via the *separator* production, not the
   exponent one. So `0x0'e-0xe` is a subtraction while `0x1e-1` is still an invalid
   number -- gcc and clang draw the line in exactly that place, and the first
   implementation of this got it wrong in both directions.
2. **C2Y `0o`/`0O` octal constants**, pedwarned below C2Y. Binary constants stopped
   pedwarning at C23 and later, where they are standard; `cli/wpedantic_alias` used
   `0b101` to test the `-Wpedantic` aliases and moved to `0o5`.
3. **`[*]` in a non-outermost array declarator of a prototype** (`double x[3][*]`) is a
   VLA of unspecified size, not an array of incomplete element type. `star_dim` also
   suppresses the "need explicit inner array size in VLAs" error, which only applies to
   real definitions.
4. **File-scope tentative definition of an incomplete struct/union**. `struct s0 x;`
   before `struct s0 { ... };` is legal when the tag is completed later in the TU. The
   decision is deferred to end-of-TU next to the existing tentative-array pass
   (`finalize_tentative_arrays`, flag `a.tentative_incomplete`), and still errors
   `storage size of 'x' isn't known` when the tag never completes.
5. **Flexible array member in a union** -- the GCC extension, pedwarned.
6. **C23 tag redefinition** (`struct q { int x; };` twice in one scope). The
   redefinition parses into a *fresh anonymous tag* and is then compared field by
   field against the original; on success the original `Sym` is what the type keeps, so
   no later member lookup can be handed the wrong one -- which is the failure mode the
   earlier assessment of this feature warned about. Enum redefinitions compare
   name/value pairs **order-insensitively** (C23 requires a one-to-one correspondence,
   not the same order: `enum X { E = 1, F = 2 }` and `enum X { F = 2, E = 1 }` are
   compatible and gcc accepts both). During the redefinition body the original tag is
   marked in-progress (`c = -2`), which is what makes a *nested* redefinition
   (`struct bar { struct bar { ... } *n; }`) still an error, as gcc requires.
7. **Copying a struct that has a `const` member.** The read-only check lived in
   `verify_assign_cast`, which every implicit conversion goes through, so passing such
   a struct by value or returning one was rejected. It moved to the two places C
   actually constrains -- the assignment operators (`expr_eq` after `test_lvalue`) and
   `++`/`--` (`inc`) -- plus the atomic RMW/store lowering, which had depended on the
   old placement. `__func__` had to become `const char[]` at the same time; it was
   lowered as a plain string literal, and `__func__[0] = 'a'` is a constraint violation
   that mcc had been catching only by accident.
8. **Complex arithmetic no longer propagates operand qualifiers.** `gen_complex_op`
   copied the whole `CType` of whichever operand was complex, so `a[0] = b[0] + b[1]`
   with `b` a `static const double _Complex[]` reported the *destination* as read-only.
9. **C23 array-qualifier rules.** An array type carries its element's qualifiers
   (`type_quals_deep`), so `1 ? (const int(*)[1]) : (void*)` composes to `const void *`,
   and `const int (*p)[3] = x;` from `int (*x)[3]` is a legal assignment rather than a
   deep-qualifier violation. Gated on `cversion >= 202311`.
10. **`= {}` on a VLA** (C23 empty initializer): allowed at any std with the existing
    C23 pedwarn below C23, lowered as a `memset` of the runtime size right after the
    `gen_vla_alloc`.

### Two things found and deliberately not fixed

**A tag declared in a parameter list is a different type inside the body.**
`void g(struct bar { int c; } *B) { struct bar t = *B; }` gives
`cannot convert 'struct bar' to 'struct bar'`. `move_ref_to_global` moves the tag `Sym`
to the global stack for lifetime and `sym_copy`s it back into the local stack for name
lookup, so the body's lookup and the parameter's type end up on two different `Sym`s,
and `compare_types` compares struct types by `ref` identity. This is inherited from
tcc's scoping model, it is not new, and it is the last thing standing between mcc and
`gcc.dg/struct-alias-2.c`. Any fix has to preserve both properties the copy exists for:
the tag must outlive the function, and its *name* must not leak past it.

**gcc rejects a self-referential struct redefinition that clang accepts.**
`struct self { void (*p)(const struct self *); };` twice: clang 22 and mcc accept,
gcc 15 says `redefinition of struct or union 'struct self'`. `gcc.dg/pr124303.c` is
gcc's own test for accepting it, so this is a gcc version skew, not a rule. The
`c23_tag_redefinition` golden avoids the shape so the `diff3` consensus stays clean.

### Where the remaining 460 are

Top clusters across both boards after this work, by error text:

| n | cluster | note |
|---:|---|---|
| 67 | `unresolved reference to 'link_error'`/`link_failure`/`foo` | the optimizer block already costed above |
| 34 | `'__bf16' is not supported on this target` | 24 of them are the `x86_64/abi/bf16` ABI suite |
| 24 | `';' expected (got '__m512'/'__m128h'/'__m256h')` | AVX-512 intrinsic headers |
| 13 | `type defaults to 'int' in declaration` | mostly `__float128` and `__seg_fs` reaching the declarator as an unknown type |
| 10 | `initializer element is not constant` | static-initializer folding (`&&label` differences, statement expressions) |
| 10 | `invalid array size` | `ms_struct` layout and `(int)(-5.0/3.0)`-style constant folding |
| 10 | `constant expression expected` | same family |
| ~10 | `#pragma pack(show)`, `ms_struct`, mac68k alignment | pragma surface |
| 6 | `identifier expected` | `_BitInt(N)` |

### Landed after that table: `deprecated` and `unavailable`

`__attribute__((deprecated))` / `[[deprecated]]` and `__attribute__((unavailable))`
were parsed and thrown away. They now set `a.deprecated` / `a.unavailable` and
diagnose at the *use* site in `unary()`'s identifier path -- a warning under the new
`-Wdeprecated-declarations` (on by default, as in gcc) and a hard error for
`unavailable`. Worth `attr-unavailable-{1,4}` and three `XPASS` rows, and it is the
kind of gap that costs nothing in the board and a lot in real builds.

What is *not* done, and why the remaining `attr-unavailable`/`c23-attr-deprecated`
rows still fail: those want the attribute on a **type** (`struct {...}
__attribute__((unavailable)) x;` then `typeof(x) y;`) and want a diagnostic when the
attribute appears in a position where it is *ignored* (`int [[deprecated]] var;`).
Both need the attribute to live on the `CType`, not just the `Sym`.

`nodiscard` was deliberately left alone. Warning on a discarded result needs the
call's SValue to carry the attribute to the point where the statement drops it, and
every cheap way to fake that (a global set at the call, cleared "when consumed")
mis-fires on `f(get())` and `get() + 1`. It is two `XPASS` rows; it is not worth a
heuristic that warns on correct code.

### One attempt reverted: unevaluated VM type names at file scope

`int b = sizeof (int (*)[a]);` is accepted by gcc and clang (the size is not
evaluated, so a variably-modified *type name* is fine at file scope) and mcc rejects
it with `constant expression expected`. The obvious fix -- a new `TYPE_TNAME`
declarator bit that survives the `TYPE_ABSTRACT` strip in `type_decl_1`, letting
`post_type` take a non-constant bound at file scope and build a VLA with no size slot
-- makes `vla-21.c` pass and **segfaults on `gcc.dg/pr88701.c`**, which puts a
compound literal of pointer-to-array type in a parameter's array bound. It was
reverted rather than landed with a crash. Anyone retrying it needs a sentinel for
"VLA with no size slot" that `vpush_type_size` rejects loudly: `ref->c` is a *negative*
frame offset for real VLAs, so `c < 0` cannot be that sentinel, and `c == 0` (which is
what the reverted patch used) is not enough on its own -- pr88701 crashes before it.
