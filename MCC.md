# MCC — noun index

Consolidated reference for ModernCC, organized as a **noun index**. Each entry is
a domain noun with its who/what/where/when/why/how packed inline (only the facets
the docs answer). Nouns are grouped under a **parent noun**; children are ordered
by **how often the parent references them** (the `≈N` is that reference count).
The exhaustive tail — every CMake node, CST slice, fixture, and known-gap test —
lives in [EXCESS.md](EXCESS.md); getting-started material stays in
[README.md](README.md). Nouns still missing a 5W+H facet are listed under
[Verification](#verification--nouns-with-thin-5wh).

Legend: **W**hat · **Whr** where · **Why** · **H**ow · **Whn** when · **Who**.

---

## 1. mcc — the compiler (root noun)

**mcc (ModernCC)** — W: extremely small, fast, portable one-pass compile-and-link
C11 compiler. Whr: `src/`. Why: fast single-pass compilation, in-memory exec,
embeddability, tiny footprint. H: single-source amalgamation, integrated
assembler, near-`-O0` deterministic host-independent backend. Who: TinyCC
(Bellard) lineage, LGPL-2.1; self-hosts + cross-compiles. Children:

- **targets/arches** (≈12) — W: x86_64 · i386 · arm · arm64 · riscv64. H: cross via `mcc-<arch>` (`MCC_ENABLE_CROSS`). Why: x86_64 is where Tier-3 promotion is validated; i386 has the fastcall ordering limit; riscv64/arm carry per-target `long double` widths.
- **libcs** (≈9) — W: glibc · musl · msvcrt · libSystem. H: glibc/musl via `--sysroot`; PE links legacy `msvcrt.dll`. Why: musl enables fully-static self-host; static-glibc via mcc hits a `__pthread_initialize_minimal` gap.
- **object formats** (≈8) — W: ELF · PE/COFF · Mach-O. Whr: `src/objfmt`. Why: ELF `longjmp` doesn't unwind (mingw SEH bug is PE-only); PE MSVC-ABI makes `long double`==`double`; Mach-O errors external `__thread`.
- **modes** (≈7) — W: compile+link · `-c` · `-S` (asm listing via built-in disassembler) · `-run` (in-memory JIT, no `a.out`) · `libmcc` C API.
- **build variants** (≈7) — W: `mcc` (default single-source), `mcc-static`, `mcc-dynamic`, `-musl` siblings, cross `mcc-<arch>`. H: suffix `mcc[-<arch>][-static|-dynamic][-musl]`. → §3.
- **libmcc** (≈12) — W: embeddable library (`libmcc.so`/`-static.a`/`-dynamic.so`). Whr: `include/libmcc.h`. Why: the embed API + what `mcc-dynamic` links. Who: exercised by `tests/embed`.
- **integrated assembler / `MCC_CONFIG_ASM`** (≈8, default ON) — W: inline asm, `asm goto`, `.s` files, `.cfi_*`, scalar SSE. Why: a **modelled ISA subset** — unmodelled mnemonics hard-error by name; disabling forces `MCC_MCCRT_USE_HOSTCC`. → §6.
- **bounds checker (`-b`) / backtraces (`-bt`)** (≈4) — W: optional safety. Whn: `MCC_CONFIG_DIAG_RT=bounds` in Debug builds only (one off|backtrace|bounds ladder; the bcheck-without-backtrace state is unrepresentable). Why: bcheck unsupported on PE (faults in msvcrt).
- **CST / AST subsystems** (≈4 each, default ON) — W: side-channel IRs; `-O0` byte-identical either way. → §8, §9.
- **machofat** (≈3) — W: self-contained universal/fat Mach-O combiner + ad-hoc codesign (no lipo). Whn: built only on Darwin; 2-slice case shells to `xcrun --show-sdk-path`. Who: `macho-universal` test.

## 2. Build system

**CMakePresets.json** (≈8) — W: single source of truth for every build. Whr: repo
root. Why: CI workflows + docker runners invoke presets, not raw `-D`. H: names
encode `platform[-compiler][-axis]`, each builds into `cmake-<preset>/`, Ninja
except msvc. Children (presets, by reference frequency):

- **`dist-*`** (≈14) — W: release-artifact presets (`dist-linux-gcc/-clang`, `-macos`, `-mingw`, `-msvc`). H: all permutations (Release, tests OFF, MCC_AMALGAMATED OFF, both libs, all cross). Whr: shared `dist.yml`.
- **`msvc`** (≈11) — W: MSVC CI cell. H: Release, `TOOLCHAIN_PROFILE=msvc`, default VS generator (no fixed `-G`), diff3 refs from `$env{}`.
- **`qemu` / `qemu-<arch>`** (≈11) — W: qemu-user cross-conformance. H: Debug, cross+`QEMU_TESTS` ON, `glibc;musl`; one CI job per host×arch×libc via `ci qemu`. → §4.
- **`sanitize` / `sanitize-msvc`** (≈10) — W: sanitized builds. H: GCC/Clang→ASan+UBSan, MSVC→ASan, mingw→trap-UBSan; alignment excluded. Why: feed `sanitize-smoke`.
- **`linux-gcc*`** (≈10) — W: the `linux` docker CI cells (`-cross/-musl/-release/-static/-multisource/-asm-off/-predefs-off/-pie/-dwarf/-sanitize/-diagnostics`). H: pinned `CMAKE_C_COMPILER` + one axis each.
- **`release`** (≈6) — W: the only Release+musl test run. H: musl ON, strip ON, bcheck/backtrace OFF.
- **`matrix`** (≈6) — W: the superbuild, build-only. H: `gcc;clang` × `native;cross`, each cell ctests during build.
- **`macos` / `macos-cross`** (≈5) — W: macos CI cell. H: `CC=$env{CC}`, `MCC_DARWIN_HOST=ON`, arm64 native + x86_64 Rosetta. Why: enables kernel-fused apple-libc suite.
- **`debug`** (≈5) — W: baseline interactive Debug. H: musl OFF, bcheck+backtrace ON, strip OFF.
- **`mingw`** (≈4) — W: build-only mingw cell (winlibs GCC `16.1.0-ucrt`).
- **`cross` / `diagnostics` / `cst` / `ast` / `local-ci`** (≈3 each) — W: alias/named-scenario dev presets (`cross`=all cross compilers; `diagnostics`=`ALL_DIAGNOSTICS`; `cst`=`MCC_CONFIG_LSP=OFF`; `ast`=`MCC_CONFIG_OPTIMIZER=OFF`; `local-ci`=reproduces the CI matrix).

**CMake node catalog** — W: the 55 `mcc_config_node`s influencing a build. Whr:
`CMakeLists.txt` §1z + loose `option()`s. Full table in EXCESS. Highest-reference children:

- **`MCC_SINGLE_SOURCE`** (≈18, ON) — W: amalgamate libmcc into one TU. Why: gates whether libmcc's `ST_FUNC` helpers have external linkage (needed by `mcc-dynamic`).
- **`MCC_BUILD_STATIC_LIB`** (≈12, OFF) — W: `libmcc-static.a` vs shared `libmcc.so`.
- **`MCC_BUILD_STATIC_EXE`** (≈11, OFF; forced OFF macOS) — W: build `mcc-static`; `MCC_AMALGAMATED` decides self-contained vs linking `libmcc.a`.
- **`MCC_CONFIG_ASM`** (≈8, ON), **`MCC_BUILD_SANITIZE`** (≈8, OFF), **`MCC_ENABLE_CROSS`** (≈8, OFF) — see §1/§2.
- **`MCC_BUILD_MUSL`** (≈7, OFF), **`MCC_MCCRT_USE_HOSTCC`** (≈6, auto), **`MCC_TOOLCHAIN_PROFILE`** (≈6, auto — seeds defaults, doesn't switch compilers), **`MCC_CONFIG_DIAG_RT`** (≈6, Debug), **`MCC_BUILD_DYNAMIC_EXE`** (≈6, ON) — see EXCESS for the rest.
- **`MCC_CONFIG_LSP`/`MCC_CONFIG_OPTIMIZER`** (≈4 each, ON) — W: enable `--lsp` capture / the `-O1+` optimizer by building the side-channel subsystems (CST/AST); codegen byte-identical either way.

**`ci` tool** (≈13) — W: the C tool (`tools/ci.c`) every workflow job drives its
preset through. H: subcommands `run-preset`/`qemu`/`dist`/`plan`/`parity`/`pkg`/
`bench-summary`; shell-agnostic. Whr: holds the preset ledger (`PS_*`/`PLAN_*`).
Children: **`ci plan`** (≈5, generates CI matrices via `fromJSON`), **`ci parity`**
(≈4, ctest `preset-parity-invariant` — fails on any preset not in both workflow +
ledger), **`ci.yml`/`dist.yml`/`release.yml`** (the workflows; `dist.yml` is the
one reusable pipeline).

**ckconfig** (≈6) — W: config-drift checker (`tools/ckconfig.c`, ctest
`config-drift-invariant`). Why: prevents emitter/default/reader rot. H: fails on
DRIFT(a) read-with-no-provider / DRIFT(b) emitted-but-unread. Children:
**`MCC_CONFIG_STATIC`** (≈6, the one `MCC_BUILD_*`-driven macro the code reads —
static `-run` symbol table, Linux-only), the read-flag set (`ASM AUTO_MCCDIR
BACKTRACE BCHECK … SYSINCLUDEPATHS`), and allowlisted asymmetries
(`UCLIBC` dead, `BACKTRACE_ONLY`/`ELFINTERP_ARMHF`/`TOOLHOST` header-only).
Analogue: **`hostgate.c`** (`MCC_HOST_*`).

## 3. Executable & library shapes

Parent: the **suffix convention** `mcc[-<arch>][-static|-dynamic][-musl]`
(arch first for cross, then link shape, `-musl` last). Children by reference frequency:

- **`mcc-dynamic`** (≈14) — W: non-amalgamated driver TU (`mcctools.c`) linked against primary libmcc. Whn: `BUILD_DYNAMIC_EXE` **and** `MCC_AMALGAMATED=OFF`; skipped under default ON. Why: its `mcctools.c` needs libmcc-internal `ST_FUNC` helpers (`pstrcpy`, …) that only a multi-TU libmcc exports.
- **`mcc`** (≈12) — W: canonical installed binary the suite drives. H: single-source, self-contained, dynamic libc, resolves `-run` symbols via `dlsym`. Whn: built always.
- **`libmcc`** (≈12) — → §1.
- **`mcc-static`** (≈9) — W: fully static (`-static`). Whn: `BUILD_STATIC_EXE`; forced OFF macOS (no static libc). Why: resolves `-run` symbols from a built-in table (`MCC_CONFIG_STATIC`).
- **cross compilers `mcc-<arch>`** (≈9) — W: self-contained host binaries for 11 foreign targets. Whn: `ENABLE_CROSS`. H: take `-static`+`-musl`, never `-dynamic` (no per-arch libmcc); each emits `<arch>-libmccrt.a`.
- **`-musl` siblings** (≈8) — Whn: `BUILD_MUSL`, via explicit `-musl` presets.
- **`mccrt` / `libmccrt.a`** (≈7) — W: the runtime archive. H: built by mcc or host CC (`MCC_MCCRT_USE_HOSTCC`); bakeable into `mcc` via `MCC_EMBED_MCCRT` (streamed through a temp fd, breaks the build cycle).
- **`mccbench` / `bench`** (≈4) — W: races mcc vs host compilers over 3 workloads → `dist/bench-<plat>.txt` (time/CPU/RSS/obj-size/fns-per-sec). Whn: `MCC_BENCH` (CI on).
- **`package-dist` / `ci pkg`** (≈5) — W: writes `mcc-`/`libmcc-`/`mcc-cross-`/`bundle-`/`checksums-` archives into `dist/` (`.tar.xz`/`.zip`).

## 4. Test suite (ctest)

**ctest suite** (≈10+) — W: CTest organized by mechanism, **one test per case**.
H: `ctest -j` fans across cores; inapplicable cases **Skip with a reason**
(`SKIP_RETURN_CODE 77`), never silently omit. Whr: `tests/`. ~400+ case files.
Children by reference frequency:

- **`tests/exec/`** (≈9) — W: main behavioral/codegen driver, 241 `.c`, 17 topic dirs. H: golden stdout/exit in `goldens.h`, run by `runner.c`. Key subdir **`features_c99_c11/`** (46, the C99/C11-coverage target).
- **`tests/qemu/`** (≈8) — W: cross-target runtime on 5 arches (~42): `conformance/` (16) + `apple-libc/` (26) + `docker/` + `macho/`. → qemu matrix below.
- **`tests/diff/`** (≈7) — W: 3-way golden-diff vs gcc/clang/tcc: `full_language.c` + 33 clause runners `parts/run_s6_2…s7_28` + Annexes + `complex_abi/`.
- **`tests/diff3/`** (≈4) — W: granular 3-way differential vs gcc **and** clang over the exec corpus (distinct from tcc-based `diff/`).
- **`tests/cli/`** (≈4) — W: driver/CLI behavior (16, `cases.h`): flags, TLS, suffixes, `readelf`/`nm` structural checks; also the negative-diagnostic tier.
- **`tests/ast/`** (≈3), **`preprocess/`** (≈3), **`cst/`** (≈2), **`sanitize/`** (≈2, `sanitize-smoke`), **`diagnostics/`** (≈2), **`embed`/`behavior`/`asm`/`tls`/`static`** — see EXCESS.
- **`full_language.c` / `mcctest`** (≈6) — W: 416-line differential mega-TU selecting compiler-specific code via `CC_NAME`. Whn: `-O0`-only (its self-referential `__bug_table` inline-asm miscompiles under any optimizer).

**qemu cross matrix** (≈6) — W: every arch × {glibc, musl} (5×2, all green). H:
fetch a Gentoo stage3 rootfs, cross-compile `tests/qemu/conformance/`, run under
`qemu-<arch> -L`; built twice (default + `-fPIC -pie`); programs self-check.
Whn: opt-in (`MCC_QEMU_TESTS`), offline-by-default. Children: **Gentoo stage3
rootfs** (≈5, vendored `vendor/gentoo-stage3-<arch>-<libc>/`), **Docker runner**
(≈3, off-Linux hosts), **`qemu-arm64-osx`** (≈2, arm64-Darwin codegen).

**CI labels** (≈5) — W: stable host-capability selectors. Who: **`native`** (every
non-qemu test; the `_test-native` base runs `ctest -L native` on *every* host, no
per-OS exclude lists — capability decided by configure probes, missing ones
skip-stub/self-skip exit 77), **`macho`** (≈5, Mach-O; some need Darwin/darling),
**`wine`** (≈3, PE under wine, now self-located via `setup-wine`), **`qemu`** (≈3,
the only non-`native` label; `qemu-arm64-osx` carries `qemu;macho`). H: `ctest -L
native -N` lists the native set. Whr: `.github/workflows/ci.yml` on every push.

**Reference compilers** — **GCC** (≈10, `~/Projects/gcc` @ `31d967232a9`;
`gcc.dg/c99-*`×135, `c11-*`×126, `atomic/`×48, torture; ~70% diagnostic), **Clang**
(≈8, `~/Projects/llvm-project` @ `0f1f456263b5`; one file per WG14 paper `nNNNN.c`),
**tcc** (≈2, third reference in `tests/diff/`). Version-of-record: gcc 15.3,
clang 22.

## 5. Conformance

**C9911 ledger** (≈10+) — W: clause-by-clause source of truth for C99 (N1256) +
C11 (N1570); every normative requirement a paraphrased checkbox. Whr: was the
~6400-row `docs/C9911.md`; the consolidation kept the **52 mcc-specific
divergences + deliberate DIFFs** (below + §5 gaps) and dropped the 2,875
conforming per-clause rows as extrapolatable ("conforms except the listed
divergences"). Why: authoritative gap ledger. Who: mcc / gcc 15.3 / clang 22.
Children:

- **requirement checkboxes** (≈8) — W: **3,837 rows**; 2,875 `mcc:✓`; 111 non-`✓` (88 `~`, 23 `✗`); **52 mcc-specific divergences** from the gcc==clang consensus (45 `~`, 7 `✗`).
- **status tags** (≈9) — W: `✓` conforms · `✗` diverges · `~` partial/optional; `(spec; not separately testable)` for pure-definition/UB rows.
- **gcc==clang consensus** (≈6) — Why: the baseline that separates real mcc gaps from consensus omissions.
- **deliberate DIFFs** (≈8) — W: `[DIFF]` conformant differences kept on purpose. Who: `__STDC_VERSION__==199901L` while shipping C11 headers; invalid-token-paste warn-continue (`pp_invalid_paste`); assignment-to-const warns (gcc/clang error); comma-in-ICE / `_Noreturn`-on-object diagnosed only `-pedantic`. Why: a diagnostic of any severity satisfies "shall … a diagnostic".
- **default C11 standard** (≈2) — W: `cversion=201112`, set in `mcc_new()` (`src/libmcc.c:911`); `-std=c99/c17/c23` select others.

**Conformance gaps** — **§6-A** (was ≈5; 1-4 now RESOLVED, 2026-07-10): (1)
plain `inline` no-extern-def now provides no external definition and
link-errors like gcc/clang (`3ca81969`, tested `cli/c99_inline_*`); (2) K&R
implicit-`int` params rejected under C99+ (`c1777f26`); (3) `va_start` last-arg
check green (`cli/va_start_last_param_clean`); (4) `<threads.h>` precedence
fixed via the `include_next` shim (`a90afe80`). Remaining §6-A residue: gnu89
plain-`inline` link behavior diverges from clang (TODO §40.1 NEW gap).
**§6-B coverage gaps** (≈8, mcc passes but under-tests): FAM (mcc ~1 vs gcc 13),
`_Noreturn` (1 vs 5), `_Alignas`/`_Alignof`, VLA-jump diagnostics, UCN breadth,
FP eval-method / Annex-F wide returns, `_Complex` Annex-G, negative-test tier.
**§6-C consensus omissions** (≈3, conforming — don't chase): `_Imaginary`, Annex K,
inexact-hex-float diag, incomplete-`_Generic`-assoc. **diff3 differential** (≈4):
four expected macOS residuals, none an mcc defect. **severity tags** (≈6):
`[BUG]`/`[FEATURE]`/`[DIAG]`/`[OPT]`/`[TASK]`/`[LIMITATION]`/`[DIFF]`.

## 6. Platform ABI & runtime

Parent: per-target byte-for-byte native-ABI matching. Children by reference frequency:

- **arm64 inline-assembler subset** (≈6) — Whr: `src/arch/arm64/arm64-asm.c`. W: models what C inline asm needs (add/sub/logical/shift/mul/mov*/mrs·msr/ldr·str/branches/adrp/barriers/ret). Why: errors by exact name on `cmp`/`csel`/`udiv`/`madd`/`fadd`/`neg`/`ldxr` etc.
- **`long double` ABI** (≈5) — W: 80-bit x87 (x86 ELF), 128-bit quad (arm64/riscv64 Linux), **64-bit==double** (Apple arm64, all PE via `MCC_USING_DOUBLE_FOR_LDOUBLE`). Whn: Apple case pinned by `cli/apple_arm64_long_double_is_double`. Why: the Apple/MSVC ABI, not an mcc limitation.
- **win32 runtime shims** (≈5) — Whr: `runtime/win32/include`. W: intentional POSIX subsets — non-recursive pthread mutexes, no key destructors/cancellation, minimal `<sched.h>`, `<fenv.h>` default-rounding-only on non-x86/arm64.
- **i386 `__fastcall`/`__thiscall` ordering** (≈5) — Whr: `src/arch/i386/i386-gen.c`. W: a register arg after a stack-spilled arg errors (accepted limitation). Whn: `i386-fastcall-abi`; register-then-stack fully supported.
- **Mach-O TLS limitation** (≈4) — Whr: `src/objfmt/mccmacho.c`. W: local `__thread` works (TLV descriptor); cross-module `extern __thread` hard-errors. Whn: pinned by `cli/macho_extern_tls_unsupported`.
- **arm (32-bit) inline-asm subset** (≈3) — Whr: `src/arch/arm/arm-asm.c`. W: 64-bit `long long` GPR-pair operands hard-error; use two 32-bit operands.
- **`<math.h>` host-provided policy** (≈2) — W: no freestanding `math.h`; decls + `libm` from host libc. Who: a freestanding non-glibc host must supply its own.
- **mingw-w64 SEH `gen_function` miscompile** (≈3) — W: fixed ~30% `RtlVirtualUnwind` fault when an `error1()` longjmp unwound through the AST-trap frame at `-O2/-O3`. H: build just the setjmp-holding frame `-O0` on mingw-gcc (now `ast_func_end` in `mccast.c`).
- **runtime-library ABI notes** (≈5) — W: fenv/errno/pthread shims; builtin-name conflicts (`__atomic_*`/`bswap` via asm `.set` — clang refuses C defs, Mach-O rejects non-weak alias); mcc-only cache-flush builtins (`armflush.c`/`lib-riscv.c`).

## 7. Profiling

**profiling method** (≈8) — W: where the mcc front end spends time. H: `hyperfine`
on a kernel-tuned quiet system (ASLR/turbo off, `performance` governor, pinned to
core 2); every figure reproducible from its command. Whr: method + results folded
into [EXCESS.md](EXCESS.md) §Profiling (was `docs/PROFILING.md` + NOTES.md). Whn:
2026-07-06, Gentoo x86-64 / gcc 15.3.0 / clang 22.1.8. Children:

- **the nine-build spread** (≈11) — W: `mcc-self` (baseline) / `mcc-gcc` / `mcc-clang` / `mcc-musl` / `mcc-static`, all `-O0 -g`, + gcc/clang debug+release. Why: mcc ignores `-O` so all mcc rows emit byte-identical code — only run-speed differs; `mcc-self` is the fastest glibc mcc (280 ms), inverting the old bootstrap cost.
- **`src/mcc.c` amalgamation** (≈12) — W: ~108k-line whole-compiler TU. Whn: `mcc-self` 280 ms `-c` vs gcc-O2 16.1 s (**57×**), clang-O2 13.4 s (48×).
- **`cst_mix64` / CST hash-consing hotspot** (≈9) — W: the multiply-xor hash finalizer; ~30% self-time in `-c` (now #1, ahead of the lexer). H: win = fewer calls (coarser granularity), not a cheaper mix.
- **`next_nomacro` / lexer core** (≈8) — Whr: `src/mccpp.c`. W: #1 in `-E` (~21%); pushed to 6% in `-c` by CST.
- **peak RSS** (≈5) — Whn: mcc ~29 MB vs gcc-O2 329 MB (11×), clang-O2 242 MB.
- **`full_language.c` fixture** (≈5) — Why: `-O0`-only, too small to separate lexer signal from noise (lexer experiments belong on `-E src/mcc.c`).
- **`TOK_HASH_SIZE` optimization** (≈3) — Whn: 16384→65536, 2026-07-05, 1.03–1.06× faster `-E`, +384 KB fixed. **idea #1 TokenSym cache** (≈3, applied); **SWAR idea #2** (≈2, rejected — below noise / runtime `set_idnum()` toggles).
- **`mcc_p` / gprof build** (≈3) — W: `-pg -static` target from `MCC_BUILD_PROFILE`; Linux/GNU-Clang only (fatal on Darwin). **perf path** (≈3, Linux-only).

## 8. CST subsystem

**CST database** — W: byte-faithful **concrete** syntax tree side-channel. Whr:
`src/mcccst.{c,h}`, `tools/csttool.c`, `tests/cst/`. Why: substrate for
`-g`/LSP/optimization; pure reflection round-trips byte-identical. Who: complete,
gated `MCC_CONFIG_LSP` (default ON). Children by reference frequency:

- **`H_s` structural / `H_t` trivia hash** (≈20) — W: two-channel 128-bit Merkle hash. H: `H_s` position-independent (trivia excluded), `H_t` layout; format-only edits localize without dirtying `H_s`; incremental rehash O(depth).
- **`src/mcccst.{c,h}`** (≈15) — W: single-file implementation (per-slice split not taken); un-poisons malloc/free for self-containment.
- **SourceFile template / content-addressed store** (≈14) — H: `cst_store_intern` dedups by `H_s(body)`; `cst_render(template,binding)`. Why: two `#include`s of one header collapse to one template id.
- **CST hooks** (≈14) — H: `CST_OPEN/MARK/CLOSE/LEAF` macros (`((void)0)` when off) + `cst_hook_token/def/use/wrap`. Why: pure side-effect — the compiler never reads the CST.
- **node kinds / schema** (≈13) — W: SoA nodes — structural, PP-concrete (MacroInvocation/IncludeDirective/PPConditional), leaf Token; `Error`/`Missing` reserved for LSP.
- **slice records (S0, B–J)** (≈12) — W: the vertical-slice completion log (store/hashing/geometry/serialization/byte-offset/owned-source/hooks/symbol-refs/macro-fidelity), all done.
- **offset→node index / `cst_base`** (≈10) — W: rebuildable accelerator + byte-offset (`src/mcc.h:464`); `cst_node_at` binary search. Why: LSP hits it per keystroke.
- **symbol refs (slice I)** (≈8) — W: use→def ids; v1 last-declaration-wins (shadowing mis-resolves, pinned by `cst/symref-shadow`).
- **macro fidelity (slice J)** (≈8) — W: written-source macro-use capture (`cst_hook_wrap`); round-trip byte-identical is load-bearing.
- **`H_e` epoch hash** (≈7) — W: invertible slot-keyed edit hash — **designed, not built**; `slot_key` currently repurposed for branch tags.
- **codegen-identity invariant** (≈5) — Why: CST never feeds a codegen decision; CST-on vs -off object output byte-identical (§8.5).

## 9. AST intention-IR

**AST intention IR** — W: an *intention* IR (desugared, type-resolved,
post-preprocessor) alongside the CST; 15 node kinds. Whr: `src/mccast.{c,h}`, gated
`MCC_CONFIG_OPTIMIZER` (ON); the hook/replay half lives under `#ifdef _MCC_H` and reads
`mccgen.c` statics (`vstack` et al.), so multi-TU builds `#include "mccast.c"` at
the end of `mccgen.c` while the standalone `mccast.c` TU compiles empty — only
`tools/asttool.c` (no `MCC_AMALGAMATED` define) compiles the arena half freestanding. Why: portable/optimizable/inlinable layer feeding an
experimental `-O1`. H: built as a pure side-channel via parser hooks with **zero
CST dependency**; `-O0` never reads it. Founding reframe (§18): the backend is
feature-complete C11, so replay never fakes anything — every gap is a **query
gap**, not a codegen gap. Children:

- **replay driver** (≈20, `ast_replay_body`→`_bb`→`_value`) — W: a second driver walking the optimized AST and calling the **same vstack ops** the parser does. Why: reach `-O0` parity (byte-identical), then beat it via promote/inline. Whn: Tier 1/2 complete, arch-independent.
- **vstack API / emitter** (≈18, `vpush*`/`gen_op`/`gv`/`gsym`/`gjmp`) — W: the shared emit core, no parser coupling. Why: `-O1` adds a driver over it, **no `gen_op` surgery**.
- **byte-verify safety net** (≈9) — W: re-emitted body compared to parser `-O0` bytes; any mismatch restores parser emission. Why: correctness never depends on modeling every op.
- **node kinds** (≈12) — W: TranslationUnit, BasicBlock, If/Jump/Return, Ref/Literal/Load/Store/Unary/Binary/Convert/Invoke/InitList, Poison. Why: `Ref` yields an *address*; every read `Load`, write `Store`; `Label`/`Block`/`Decl` dissolve; types live in `CType`/`Sym`.
- **Tier 3 register promotion** (≈8+, `MCC_AST_PROMOTE`) — W: mem2reg of address-not-taken locals — the real `-O1` payoff, first opt that beats `-O0`. H: pinned register seeded from the stack slot at entry; call-free R10/R9/R8, call-ful RBX/R12–R15, floats XMM6/7. Whr: x86_64-only. Poisons: `&`-taken, `p->m` bases, aggregate ranges, `++`/`--`, bitfields, `volatile`, inline asm. → EXCESS for pins/backlog.
- **Tier 4 virtual inline** (≈8+, `MCC_AST_INLINE`) — W: graft a within-TU `static` callee in place of the boundary `Call` (`ast_inline_graft`, `hi`-based frame bias). H: deletes the ABI (params materialized into biased slots), returns coalesce via a memory phi, cycle guard depth 8, per-site const-arg specialization + dead-branch elim (§19.3). Whn: broadly complete, arch-independent (arm64-verified); **not** auto-enabled by `-O1`.
- **exec-replay / -tmpl / -promote columns** (≈8) — W: whole-corpus CTest columns re-running `tests/exec` under replay (+templates, +promote), output-compared to `-O0`. Why: the driver's gate + feature checklist.
- **const-fold template** (≈5–7, `MCC_AST_TEMPLATES`) — W: first optimization template; byte-neutral, also the vehicle for §19.3 const-prop.
- **replay pass suite** (`MCC_AST_TEMPLATES` umbrella) — W: the sibling passes run in `ast_func_end` after the faithfulness gate: branch-fold, ident/redundant-cast elimination, per-block const-prop, per-block CSE + LICM (named-local reuse), dead-store elim, SCCP branch half, empty/identical-arm join fold, bit-flag encode, tail-call→back-edge (TCO). Whr: `src/mccast.c`; history in OPTIMIZE.md, ladder status in STATUS.md.
- **structured-join dataflow (§32a/b)** (`MCC_AST_CPROP_JOIN` / `MCC_AST_CSE_JOIN`, default off) — W: the const lattice and the CSE availability table thread through structured control flow (fork/meet at `AST_If` joins, invariant-set loop descent, flat-scan fallback via a visited bitmap) instead of resetting at every boundary. H: retag-to-literal + named-local reuse only — no new node kind (TODO §32 verdict). Gate: corpus ctest with the gates forced on + dual-mode self-host fixpoint.
- **bit-flag conditional encoder** (`MCC_AST_BITFLAG=N`, default off; searched over {0,3,5,9} at `-O4+`) — W: same-key `==`-cluster dispatch (`x==a||x==b||…` and else-if chains) re-encoded as a branchless mask test `(int)((MASK >> (k&63)) & 1) & (k<64)`. H: reversed operand order — replay never runs `vcheck_cmp`, so the comparison must be consumed immediately (FIX.md bug class).
- **Sethi–Ullman operand ordering (§35)** (`MCC_AST_SETHI`, default off) — W: for a commutative binary (`+ * & | ^`) with two side-effect-free operands, evaluate the higher-register-pressure operand first (emitted child-order = replay evaluation order). H: value-preserving for any type (IEEE `+`/`*` commute bit-exactly; `&|^` integer-only) — but skips operands whose root is a comparison/logical op, since reordering a `VT_CMP` producer clobbers the flags (the same `vcheck_cmp` bug class). Effect: `.text` 81→73 B on nested arithmetic; fixpoint byte-identical off and forced-on.
- **superoptimizer + caches (`-O<N>`, N≥4)** — W: a compile-time strategy-portfolio search over the pass/budget space, scored by `.text` size, resumable via durable flock'd checkpoints under the per-user cache dir, interruptible (SIGTERM save-and-stop, subprocess watchdog). H: per-function tier (`MCC_AST_PERFN`) keyed by the alpha-renamed **AST intention hash** (`ast_intention_hash`, also public as `mcc_intention_hash`/`mcc_cache_dir` in libmcc); `--clear-cache`, `--embed-jit` manifest (`--jit-functions`/`--jit-max-duration`). Whr: `src/mcc.c` + `tools/mcchv.c`; the single status snapshot is **STATUS.md**.
- **opt gating** (≈5–7) — Who: the replay stack is driven by the runtime `-O` level, batched by measured compile cost: `-O0` (the default) leaves the backend byte-untouched; **`-O1`** = replay + const-fold templates (+13–22% compile cycles, templates ≈ free); **`-O2`/`-Os`** = + Tier-3 promotion (x86_64 only; +5–7.5%); **`-O3`/`-Ofast`** = + Tier-4 virtual inline (+1–4%; excluded from `-Os` — grafting grows code). The former `MCC_AST_REPLAY` master env gate is gone; the sub-gates `MCC_AST_TEMPLATES`/`_PROMOTE`/`_INLINE` remain env overrides on top of the `-O` defaults (`0` closes, non-`0` opens), `_REPLAY_DUMP`/`_NO_CALLFUL` stay opt-in. Later opt-in knobs (search/debug, catalog in STATUS.md): `MCC_AST_INLINE_LIMIT`/`_INLINE_NODES`/`_GRAFT`, `_PROMOTE_LIMIT`/`_OPT_LIMIT`, `_FN_CONFIG`/`_PERFN`, `_COST`, `_BITFLAG`, `_SETHI`, `_CPROP_JOIN`/`_CSE_JOIN`, `_HASH_OUT` (internal driver↔worker channel). The default-on flip was reverted after `mcctest` exposed Tier-3/Tier-4 unsoundness on the PE target and arm64.
- **correctness gates** (≈10) — Who: exec-golden differential, **two-pass faithfulness gate** (pass 1 byte-verifies vs `-O0`, only-if-faithful pass 2 transforms), the **GCC c-torture differential AST gate (A4/§C1)** (≈8, `mccharness gcctestsuite --ast`, pass-at-`-O0`-fail-under-column=regression), `ast/replay-*` fixtures, full `ctest` (1862 on x86_64 as of 2026-07-10; arm64 runs one fewer).

## 10. Backlog / open work

Parent: the live task tracker (former TODO.md). Children by reference frequency:

- **`-O1` transform soundness backlog — CLEARED 2026-07-09** (see EXCESS.md, §19) — W (historical): 14 promote + 4 inline + 3 replay `KNOWNGAP`s. All 21 now fixed and empty (`GCCTS_AST_KNOWN_{REPLAY,PROMOTE,INLINE}` = `{0}` in `tools/mccharness.c`); the "drive to zero before A1" gate is met.
- **`-O1` wiring — UNPARKED 2026-07-09 (§20)** (see EXCESS.md) — `mcc -O1` engages replay + Tier-3 promote and **now self-hosts** (the `-O1`-built compiler reproduces the `-O0`-built one; inline also self-hosts under the governor). Former park reasons (hard-error recovery, inline governor) resolved; `mcc-self -O1` no longer `n/a`.
- **A1 — backward-liveness spill-slot sharing** (≈1–5) — W: share a pin across disjoint live ranges (a register allocator over the AST CFG). Why blocked: promotion entry-seeds every pin — A1 needs per-live-range seeding, a rework of the emit model.
- **CMake normalization** (≈3) — W: minimize gating (autodetect), offload to `tools/`, fold `.cmake`. Whn: assessed 2026-07-07 — largely in place; remainder is open-ended polish with CI-breakage risk.
- **skipped-test ungating audit (per triple)** (≈2 each) — W: re-evaluate each `mcc_skip_test` when a host can run a triple. Who: x86_64-linux full; i386-linux blocked (no 32-bit sysroot); aarch64/armv7 partial (qemu is x86-TSO — can't validate weak-memory atomics); x86_64/i386-windows available; arm64-windows blocked (no native ref cc); x86_64/arm64-Darwin audited.
- **i386 TLS residual** (≈2) — W: `R_386_TLS_GD/LDM` paths (`i386-link.c`) need an i386 cross + 32-bit sysroot. Whn: still open (x86_64 covered by `tls-models`).
- **C99/C11 test-coverage backlog** (≈2) — W: negative/diagnostic tier (gcc's files are ~70% `dg-error`; seed `c99-typespec-1.c` = 1055 dg-error) + UCN-in-identifier breadth.
- **decided-with-revisit-trigger backlog** — Who: store factoring (shared render engine), `Bind`-marker (reopens if CST can't answer a `-g`/LSP query), `k` depth, size-gated outline, template DSL, per-function `-O1`, PP-as-executable-C (parked).
- **ACHTUNG DO-NOT-DO list** (≈1 each) — Who: human-dimension-aware diagnostics; full `-g`/gdb; time-budgeted `-O1..100`; CST-snapshot hot-reload. Why: explicitly fenced off.

---

## Verification — nouns with thin 5W/H

The index-agents flagged these nouns as succinctly named but missing a facet a
reader would expect. Listed so gaps stay visible (fold detail from EXCESS/source
when needed):

- **Poison** (AST node kind) — no Why/How/Whn: the docs never say when it's emitted or how it lowers.
- **`-g` / LSP / debug info**, **LTO / cross-TU**, **`-O2`/`-O3` SSA drivers**, **hot-reload snapshots**, **jump-table/algebraic templates** — Long-horizon design-only; What-name present, no How/Whn.
- **`TranslationUnit` node**, **`H_e` epoch hash**, **CST slice-G multi-file include stitching** — reserved/designed-not-built; no How/owning consumer.
- **spill-slot sharing (A1)**, **inline governor** — What+Why, but no Whr (symbol/file), sizing metric, or Whn.
- **`pr119002`** (call-free int promote gap), **`usad-run`/`ssad-run`** (inline holes) — named with no root cause. **"Real semantic/diagnostic gaps" TODO bucket** — declared header, empty body.
- **PP-as-executable-C (JIT)**, **`k`/size-gated-outline/template-DSL/per-function-`-O1`** revisit triggers — one-line Why, no Whr/Whn.
- **riscv64** — thin Who/Whn beyond `long double` + cache-flush builtin. **`-run` / embeddable library** — listed as unique capabilities, implementation not described. **MSVC arm64 TLS miscompile** — root cause noted, no Who/Whn-fixed.
- **tcc** — the one reference compiler with no version/provenance (gcc/clang have SHAs).
- **Runtime-path knobs, `MCC_BUILD_STRIP`, `MCC_CONFIG_NEW_DTAGS`, `MCC_RUN_MMAP_EXEC`, `MCC_AUTO_MCCDIR`** — What/How given, no Why/Whn to override the default (full catalog in EXCESS).
</content>
