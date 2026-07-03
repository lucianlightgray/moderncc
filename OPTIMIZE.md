# OPTIMIZE — DRY / helper-extraction findings

Duplication and helper-extraction candidates for the mcc codebase, mined
mechanically from a statement-pattern analysis of the C we own (`src/`, `tools/`,
`runtime/`). Every item below is grounded in a query you can re-run.

- **How this was produced:** `analysis/build.py` tokenizes the tree, splits every
  function into linear *statement units*, normalizes them (locals → `V1,V2…`,
  literals → placeholders, **call names & struct fields kept**), and mines
  contiguous groups that recur. Two databases are kept for exploration:
  `analysis/statements.db` (per-function patterns) and `analysis/inlined.db`
  (call-inlined streams + cross-function patterns). See `analysis/README.md`.
- **Corpus** (re-analyzed at commit `06dbb3c7`, "Port PLAN Part 0 tool suite to
  Windows"): 77 files · 1,927 functions · 38,108 statements → 10,817 recurring
  per-function patterns (n = 3…12) and 30,209 cross-boundary patterns.
- **Reproduce any finding:** `python3 analysis/query.py {candidates,sweep,xfunc,show,xshow,recursion}`.
- **Caveat:** the parser is a pragmatic heuristic, not a full C frontend. Ranking
  is trustworthy; **confirm each site before refactoring.** Normalization is
  structural, so "same pattern" can mean "same shape, different names" — read the
  cited code. This caveat proved decisive for Priority 1 (see status).

## Implementation status (2026-07-03)

Implemented and verified the **real** DRY wins; the Priority-1 arch findings were
investigated and turned out to be **normalization artifacts**, not duplication.

| item | outcome | verification |
|------|---------|--------------|
| **P2** `mcc_grow_capacity` | ✅ **done** — one doubling-growth helper backs `cstr_realloc` + `section_realloc` | native + onesource-off ctest 46/46 |
| **P3a** unified argv builder | ✅ **done** — `Argv`+`ts_arg/ts_args/ts_argz` in `toolsupport`; `A/Z`, `arg/args/argz` are one-line wrappers; `ci.c` uses it directly | ctest 46/46 |
| **P3b** `ts_path` join | ✅ **done** — 123 `snprintf("%s/…", dir)` sites now call `ts_path` | ctest 46/46 |
| **P4** asm helper | ◐ **partial** — the extractable part is only `skip_constraint_modifiers` (4 identical copies → one `ST_FUNC` in `mccasm.c`); the full-function/loop extraction is **infeasible** (see P4) | native + cross(4 arches) + onesource-off 46/46 |
| **P1a–e** arch tables | ❌ **not real duplication** — do not refactor (see P1) | verified by inspection |

**Key correction:** the analysis ranks by *normalized* pattern frequency, which
renames enum constants to `V#`. That makes every same-shaped `switch` (e.g.
`case X: return N;` / `case X: y = Z; break;`) collide, inflating the Priority-1
counts. On inspection the arch "duplication" is textual shape over
**semantically arch-specific** code (different constraint letters, registers,
encodings per backend) — real logic duplication there is minimal.

### Changes since the previous analysis (`8eb4cb01` → `06dbb3c7`)
The pull was the Windows (mingw/MSVC) host port of the PLAN Part-0 tool suite.
- **P3a got worse:** `tools/build.c` was refactored to use a new `arg()`/`argz()`
  argv builder — a *third/fourth* copy of an idiom already in `mccharness.c`,
  `ci.c`, `defcheck.c`. Promoted to the top of P3 below.
- **P1, P2, P4 unchanged:** the port touched `src/mcchost.c`, `tools/build.c`,
  `tools/toolhost.h`, `CMakeLists.txt` — not `src/arch/*` or the realloc helpers,
  so the arch tables, `grow_capacity`, and the constraint loop are unaffected.
- **`src/mcchost.c` Win32 additions:** platform-branchy host code (pipe reader
  thread, `host_sys_info`, ~10 `CloseHandle`/`GetLastError` sites). Inherently
  `_WIN32`-guarded host-axis code; no large extractable block — **not** a new
  finding, noted for completeness.

---

## Priority 1 — Assembler / codegen backends ❌ NOT REAL DUPLICATION

> **Reversed after inspection.** These were ranked highest by *normalized* pattern
> frequency, but the normalizer renames enum constants and locals to `V#`, so
> every same-shaped `switch` collides. Reading the actual source shows these are
> distinct per-arch logic that merely shares a `case`-ladder *shape*. Converting
> them to tables would be a pure style change to codegen — real miscompile risk,
> zero deduplication. **Left as-is.** Kept below with the evidence, since "why we
> did NOT do this" is itself a finding.

### 1a. Condition-code → encoding table (arm) — *single function, already DRY*
`mapcc` (`src/arch/arm/arm-gen.c:418`) is **one** switch; the inline sites at
`arm-gen.c:647,1430` already **call** `mapcc` (they don't re-spell the table).
The "4 functions" from `query.py` was the normalizer matching `mapcc` against
unrelated `case V1: return #;` ladders (e.g. riscv64 `asm_parse_csrvar`). No copy
to remove. A switch→table rewrite here is style, not DRY.

### 1b. Barrel-shifter modes (arm) — *ladder exists once*
`ENCODE_BARREL_SHIFTER_MODE_ASR` appears at exactly three lines in
`src/arch/arm/arm-asm.c`: the `#define` (`:414`) and two uses (`:527`, `:820`).
The full `asr/lsr/ror` ladder is written **once** (~`:522`). "Count 39 in 10
functions" was `case V1: V2 = V3; break;` matching every similar switch in the
assemblers — not a repeated barrel-shifter ladder.

### 1c–1e. arm64 emission / riscv64 mem-access / i386 decode ladders — *shape only*
`arm64_gen_opil`, `asm_mem_access_opcode`, and i386-dis `decode` each contain a
long `case`/`else-if` ladder, but the **values differ every case** (distinct
opcodes, immediates, mnemonics). Normalization erases those to `#`/`V#`, so they
read as one pattern. They are genuine per-instruction encoding logic; a table
would just relocate the same data with added indirection. Not pursued.

---

## Priority 2 — The capacity-doubling growth idiom ✅ DONE

**Implemented:** added `ST_FUNC unsigned long mcc_grow_capacity(cur, need, min_cap)`
in `src/libmcc.c` (declared `src/mcc.h`); `cstr_realloc` and `section_realloc` now
call it (section keeps its own `memset`). Verified: native + onesource-off ctest
46/46 — every compile exercises both buffers.

The single best *cross-subsystem* finding — surfaced only by inlined mining
(`query.py xfunc --min-span 3`, patterns x#28198 & x#26848). Two independent
dynamic buffers grow their storage with the **same doubling loop**:

`src/mccpp.c:288` `cstr_realloc(CString*)`:
```c
int size = cstr->size_allocated;
if (size < 8) size = 8;
while (size < new_size) size = size * 2;
cstr->data = mcc_realloc(cstr->data, size);
```
`src/objfmt/mccelf.c:285` `section_realloc(Section*)`:
```c
unsigned long size = sec->data_allocated;
if (size == 0) size = 1;
while (size < new_size) size = size * 2;
data = mcc_realloc(sec->data, size);
memset(data + sec->data_allocated, 0, size - sec->data_allocated);
```
- **Same idiom, not byte-identical:** the floor differs (8 vs 1) and
  `section_realloc` zero-fills the grown region.
- **Fix:** `size_t mcc_grow_capacity(size_t cur, size_t need, size_t floor)`
  returning the doubled size; both callers use it, `section_realloc` keeps its
  own `memset`. Low risk, exercised by the whole ctest suite (every compile grows
  both buffers).

---

## Priority 3 — `tools/` build/test-harness boilerplate

High raw counts but concentrated in the in-tree tools (recently written). Lower
architectural stakes than the compiler, but cheap wins.

### 3a. `argv` builder — unified ✅ DONE
Was the same `v->a[v->n++] = s` one-liner in four tools. **Implemented:** one
`Argv` type + `ts_arg` / `ts_args` / `ts_argz` (with an overflow guard) now live
in `tools/toolsupport.{c,h}`. `mccharness.c`'s `A`/`Z` and `build.c`'s
`arg`/`args`/`argz` are one-line wrappers over them (call sites untouched);
`ci.c`'s two inline `a[n++]` argvs now use `Argv` directly. `defcheck.c`'s
`syms_add` is a *different* structure (an owning, doubling-grown `strdup` set) and
was correctly left alone. Verified ctest 46/46.

### 3b. Path building — `ts_path` ✅ DONE
**Implemented:** `int ts_path(char *dst, size_t n, const char *dir, const char *fmt, …)`
in `toolsupport`; the 123 genuine `snprintf(dst, sizeof dst, "%s/…", dir)` joins
across `mccharness.c` (109), `build.c` (14), `dashsbytes.c` (3), `ci.c` (1) now
call it. Compiler-*flag* snprintfs (`-I%s`, `-B%s`, `-L%s/…`) are **not** path
joins and were left. Verified ctest 46/46.

### 3c. CLI option triples — `opt(argc, argv, "--flag", NULL)` ×46 — *left as-is*
`opt` (`tools/mccharness.c:61`) is already a helper; the "duplication" is a block
of consecutive `const char *x = opt(argc, argv, "--x", NULL);` config lines with
no shared logic to factor. Clearer left explicit. **Not changed.**

---

## Priority 4 — asm constraint code (4 arch backends) ◐ PARTIAL

The n=12 pattern flagged an "identical" 12-statement constraint-resolution loop
in all 4 `asm_compute_constraints` (`i386/arm/arm64/riscv64-asm.c`). On close
inspection the extraction is mostly **infeasible**, with one genuine win:

- **The loop is not truly shareable.** Its `else` branch calls
  `constraint_priority(str)`, which is `static inline` and **genuinely different
  per arch** (i386 handles `'a'/'b'/'c'/'d'/'S'/'D'/'A'`; arm handles
  `'l'/'I'/'J'/'K'/'L'`, different register ranges). The loop text is identical
  but its behavior is arch-parametrized, so hoisting it would need a
  function-pointer callback for marginal benefit — **not done.**
- **The rest of `asm_compute_constraints` diverges too** — the `switch(c)`
  register-allocation body is arch-specific (different constraint letters, reg
  files, `regs_allocated[]` reservations). Not a clone.
- **✅ The one real shared helper: `skip_constraint_modifiers`.** A 5-line pure
  string scanner, byte-identical (bar whitespace) in all 4 backends.
  **Implemented:** moved to `src/mccasm.c` as `ST_FUNC` (declared `mcc.h`, beside
  the already-shared `find_constraint`), and the 4 static copies deleted.
  Verified: native + **cross build of all 4 arch backends** + onesource-off, 46/46.

---

## Context — recursion map (not a defect; informs any inlining/analysis work)

From `query.py recursion` (`analysis/inlined.db`): 848 call-cycles were detected
and bounded when building the inlined streams. The dominant recursive cores are
exactly the expected ones and should be treated as intentional:

| cycle | hits | note |
|-------|------|------|
| `next` / `next_nomacro` | 380+ | recursive-descent lexer/preprocessor |
| `type_size` | 194 | recursive type-size (nested aggregates) |
| `type_to_str`, `compare_types`, `is_compatible_types` | 48/23/… | recursive type machinery |
| `host_dir_walk`, `ts_fnmatch` | 52/8 | tree walk / glob in `tools/` |

These are the reason cross-function inlined patterns bottom out where they do;
they are not extraction candidates.

---

## Suggested order of work

**Done, in this order:** `mcc_grow_capacity` (P2) → unified argv builder +
`ts_path` (P3a/P3b) → `skip_constraint_modifiers` shared (P4 partial). Each was
built and tested (native ctest 46/46; the asm change additionally cross-built
across all 4 arch backends and the onesource-off/separate-TU config).

**Deliberately not done:** the Priority-1 arch "tables" (P1a–e) and the
`asm_compute_constraints` loop hoist (P4) — investigation showed these are
normalized-shape collisions or arch-specific logic, not real duplication (see
each section). Refactoring them would add risk to codegen with no dedup benefit.

**Net effect:** ~5 duplicated helper implementations collapsed to one each
(`grow_capacity`, the argv builder, `skip_constraint_modifiers`) plus 123
path-join sites routed through `ts_path` — all verified. The lesson for future
runs: rank by *normalized* frequency to find candidates, but always read the
source before refactoring — structural shape ≠ logic duplication. Re-run
`python3 analysis/build.py` to see the collapsed counts.
