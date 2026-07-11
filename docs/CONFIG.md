# CONFIG.md — code-facing configuration macros

The subset of build configuration that reaches the compiler source as `-D`
defines, the naming standard those macros follow, and the machine checks that
keep CMake and the C side in lockstep. The build-side catalog (every CMake
cache value, presets, matrices) is [BUILD.md](BUILD.md); this file covers what
the *source* reads.

## Naming standard

Settled by the gate-macro rename sweep; the drift checker
enforces the prefixes:

- **`MCC_CONFIG_<WHAT_IT_DOES>`** — public, user/build-facing, settable via a
  CMake node or a `-D` define (`MCC_CONFIG_OPTIMIZER`, `MCC_CONFIG_DIAG_RT`,
  `MCC_CONFIG_TRIPLET`, …). Names state what the feature does as implemented —
  plain English, no history, no relative terms.
- **`MCC_<WHAT_IT_IS>`** — private/derived/structural constants
  (`MCC_TARGET_X86_64`, `MCC_AMALGAMATED`, `MCC_PTR_SIZE`, `MCC_VERSION`).
  Nothing gate-like ships without one of the two prefixes, and no unprefixed
  name crosses a file boundary.
- **No derived negatives / shadows** — one positive macro per feature; no
  `*_ONLY`/`*_OFF` inverses.
- **One test idiom** — boolean config macros are tested with `#if`, never
  mixed `#ifdef`/`#if defined(X) && X`.

## Emission flow

`mcc_config_node()` declarations in `CMakeLists.txt` (section 1z) define the
cache values; the emission block converts the code-facing ones into compile
definitions and writes the exact list to
`<build>/config-defines.txt` — the ledger the drift checker audits. A typical
debug-preset ledger:

```
MCC_TARGET_X86_64=1
MCC_CONFIG_DIAG_RT=2
MCC_CONFIG_LSP=1
MCC_CONFIG_OPTIMIZER=1
MCC_CONFIG_TRIPLET="x86_64-pc-linux-gnu"
MCC_CONFIG_OS_RELEASE="…"
MCC_CONFIG_MCCDIR="…"
```

Level-valued knobs emit numeric macros so `#if` can compare them
(`MCC_CONFIG_DIAG_RT`: 0 = off, 1 = backtrace, 2 = bounds — the CMake side is
the `off;backtrace;bounds` STRING dropdown). The `bounds` level (2) also
compiles in mcc's own `-fsanitize=address`/`-fsanitize=bounds` support: those
flags share the memory/bounds checker, so `libmcc.c` gates them behind
`#if MCC_CONFIG_DIAG_RT >= 2` and errors ("needs the memory/bounds checker,
which was not compiled in") when built below that level. Path/string defaults
that used to
be baked as `-D` strings live as data rows in `src/mccdefaults.h` instead
(see BUILD.md §6); only force-overrides still arrive as defines.

## Drift checking (`config-drift-invariant`)

`tools/ckconfig.c` scans the source for every `MCC_CONFIG_*` reference and
diffs it against the emitted ledger, failing the `config-drift-invariant`
ctest on either direction of drift:

- **emitted-but-unread** — a define the build sets but no source line tests
  (a retired or misspelled macro). Curated exceptions: `ALLOW_DEAD`
  (`MCC_CONFIG_UCLIBC`, `MCC_CONFIG_AUTOCORRECT`, `MCC_CONFIG_LIBC` — build-side
  selectors with no direct source read).
- **read-but-never-emittable** — a macro the source tests but no CMake node
  can produce. Curated exceptions: `ALLOW_EXTERN`
  (`MCC_CONFIG_BACKTRACE_ONLY`, `MCC_CONFIG_MCCBOOT` — structural/bootstrap
  defines set outside the node system).

Sibling invariants from the same tool family: `host-gate-invariant`
(`tools/hostgate.c` — raw host-OS macros allowed only in `mcchost.*`),
`target-gate-invariant` (`MCC_TARGET_*` conditionals frozen to `src/arch/` +
the audited consumer list), `dead-gate-invariant` (no new `#if 0` / bare
`#if 1`), `idiom-gate-invariant` (the canonical `#if` test form),
`preset-parity-invariant`, and `retired-macro-invariant` (zero hits for
`CONFIG_MCC_*`, bare `CONFIG_[A-Z]`, and retired names).

## Adding a flag

1. Declare the `mcc_config_node()` in CMakeLists (comment-free; the node table
   is the documentation source — regenerate via `mcc_generate_node_doc()`).
2. Emit it in the emission block under its `MCC_CONFIG_*` name; presets and
   the `tools/ci.c` ledger stay in lockstep in the same commit.
3. Read it in source with the canonical idiom (`#if MCC_CONFIG_X` for
   booleans, numeric compare for levels).
4. `ctest -R config-drift-invariant` (plus `preset-parity-invariant`) must
   stay green; document the knob's build-side row in BUILD.md §5.
