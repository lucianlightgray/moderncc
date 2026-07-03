# OPTIMIZE — DRY / helper-extraction findings

Duplication and helper-extraction candidates for the mcc codebase, mined
mechanically from a statement-pattern analysis of the C we own (`src/`, `tools/`,
`runtime/`) and cross-checked with a raw-body hash/similarity pass. Every item is
grounded in a query or grep you can re-run.

- **How this was produced:** `python3 analysis/build.py` tokenizes the tree,
  splits every function into linear *statement units*, normalizes them (locals →
  `V1,V2…`, literals → placeholders, **call names & struct fields kept**), and
  mines contiguous groups that recur. Two databases are kept for exploration:
  `analysis/statements.db` (per-function patterns) and `analysis/inlined.db`
  (call-inlined streams + cross-function patterns). See `analysis/README.md`.
  Findings below were then confirmed with a raw-text function-body hash
  (`/tmp/dupfn.py`-style: exact-identical bodies across files) and a token-multiset
  Jaccard near-dup pass — the ranking tool over-counts (it renames enum constants,
  so same-shaped `switch`es collide), so **every item here was read in source.**
- **Corpus** (re-analyzed at commit `22f4049c`): 77 files · 1,929 functions ·
  38,114 statements → 10,817 recurring per-function patterns (n = 3…12) and
  31,380 cross-boundary patterns.
- **Reproduce:** `python3 analysis/query.py {candidates,sweep,xfunc,show,xshow}`;
  cross-file dups: the SQL at the bottom of this file.
- **Caveat:** the parser is a pragmatic heuristic, not a full C frontend. Ranking
  is trustworthy for *where to look*; **confirm each site before refactoring.**

## Relationship to prior work

The previous OPTIMIZE pass (`45583894`, "DRY: collapse duplicated helpers") is
**done and committed**: `mcc_grow_capacity`, the unified `toolsupport` argv builder
(`ts_arg`/`ts_args`/`ts_argz`) + `ts_path`, and hoisting `skip_constraint_modifiers`
into `mccasm.c`. Those no longer appear as live candidates. It also correctly
**rejected** the Priority-1 arch "tables" as normalization artifacts — that verdict
still holds and is not re-litigated here.

This is a **fresh pass** over the current tree. The remaining real duplication
falls into two buckets: **(A)** one cross-subsystem redundancy that lives in a
single binary (LEB128), and **(B)** parallel per-arch source copies of the
backend emit primitives. Bucket B needs a framing note:

> **Only one arch backend is compiled per build.** `CMakeLists.txt:1758` globs
> `src/arch/${MCC_CPU}/*.c` (plus `i386-asm.c` for x86_64). So the five copies of
> `g()` never coexist in one binary — they are **alternatives**, not linked
> redundancy. Merging them is a **source-maintenance** win (one source of truth so
> a fix lands everywhere), not a code-size win, and carries **zero runtime risk**
> for the byte-identical ones. Weigh it as maintainability, not performance.

---

## Priority 1 — LEB128 encode/size helpers (cross-subsystem, same binary) ⭐

The single best finding this pass, and the only one that is genuine duplication
*within one linked binary* (the DWARF debug-info writer and the Mach-O writer are
both compiled into every build). Same shape as the prior `mcc_grow_capacity` win.

**`uleb128_size` is byte-identical in two files:**
- `src/mccdbg.c:686` `dwarf_uleb128_size(unsigned long long)`
- `src/objfmt/mccmacho.c:505` `uleb128_size(unsigned long long)`
```c
int size = 0;
do { value >>= 7; size++; } while (value != 0);
return size;
```

**The uleb128 *encoders* are the same continuation-byte loop over different sinks:**
- `src/mccdbg.c:713` `dwarf_uleb128(Section*, …)` → emits via `dwarf_data1`
- `src/objfmt/mccmacho.c:516` `write_uleb128(Section*, …)` → emits via `section_ptr_add`

There is also a matching **decoder** `dwarf_read_uleb128` used in `src/mccrun.c:829`,
and the signed variants `dwarf_sleb128`/`dwarf_sleb128_size` (`mccdbg.c:698,723`).

- **Fix:** add a small LEB128 primitive set (e.g. `mcc_uleb128_size` /
  `mcc_sleb128_size`, and an encoder that takes a `Section*`) in a shared unit
  (`libmcc.c` or a new `mccleb.c`), declared in `mcc.h`. `dwarf_*` and the Mach-O
  writer become thin callers. The size helpers are a trivially safe first step
  (pure functions, identical bodies); the encoders differ only in which 1-byte
  emit they call, so parametrize on the sink or keep the encoder writing to the
  passed `Section*`.
- **Risk:** low. Exercised by every `-g` compile (DWARF) and every Mach-O link;
  `ctest` covers both. Start with `*_size` (pure) to de-risk.

---

## Priority 2 — `g()` is byte-identical across all five backends ⭐

The one clean, unambiguous cross-arch collapse. `g(int c)` — "append one byte to
the current text section, growing it" — is **character-for-character identical** in
all five backends (verified by raw-body hash; `ngram #5990`, and the Jaccard pass
reports 1.00 for every pair):

- `src/arch/arm/arm-asm.c:126`
- `src/arch/arm64/arm64-asm.c:112`
- `src/arch/i386/i386-gen.c:77`
- `src/arch/riscv64/riscv64-asm.c:88`
- `src/arch/x86_64/x86_64-gen.c:137`
```c
ST_FUNC void g(int c) {
    int ind1;
    if (nocode_wanted) return;
    ind1 = ind + 1;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind] = c;
    ind = ind1;
}
```
It reads and writes only shared globals (`ind`, `cur_text_section`,
`nocode_wanted`) — nothing arch-specific.

- **Fix:** define `g` once in a backend-common source file compiled for every arch
  (e.g. `src/arch/common-emit.c`, or hoist into an existing always-linked TU),
  declared `ST_FUNC` in `mcc.h`; delete the five copies. Because only one arch
  compiles per build there is no ODR conflict either way.
- **Risk:** minimal — the bodies are identical, so the merged result is a no-op
  change per arch, and `g` is on the hot path of *every* emitted instruction, so
  any regression fails the whole suite immediately.

---

## Priority 3 — The rest of the backend emit-primitive family (partial overlap)

Beyond `g`, a cluster of tiny emit/util primitives is copied across backends with
**partial** identity. Unlike `g`, these have 2–3 distinct variants, so this is not
a blind merge — an arch-common file would host the shared variant and the outliers
keep their own. Ordered by cleanliness; all are the "one arch per build" framing.

| helper | copies | identical groups | note |
|--------|--------|------------------|------|
| `oad` | 2 | **1** — `i386-gen.c:121`, `x86_64-gen.c:211` | identical; x86-family only |
| `gen_le16` / `gen_le32` | 5 | 3 — {i386,x86_64}, {arm,riscv64}, {arm64} | all little-endian byte writers; textual/format drift, semantically one function |
| `o(unsigned int)` | 5 | 3 — {i386,x86_64}, {arm64,riscv64}, {arm} | **do not blind-merge:** variable-length (x86) vs fixed 4-byte (arm64/riscv) emit are genuinely different |
| `gjmp_append(int,int)` | 4 | 2 — {arm64,riscv64,x86_64} identical, arm differs | 3-way identical group is mergeable |
| `asm_clobber` | 4 | 3 — {arm,riscv64} identical, i386 & arm64 differ | modest |
| `pe_tls_index_sym` | 3 | 3 identical bodies — `i386-gen.c:235`, `x86_64-gen.c:306`, `arm64-gen.c:499` (named `arm64_pe_tls_index_sym`) | Win32-only path; identical |

- **Fix:** create `src/arch/common-emit.c` (or a shared header of `static inline`s)
  for the byte-serialization writers (`gen_le16/32/64`) and the identical helpers
  (`oad`, the 3-way `gjmp_append`, `pe_tls_index_sym`). Leave `o` per-arch — its
  variants encode different instruction-word models.
- **Risk:** low per-helper, but higher *coordination* cost than P2 (must confirm
  each arch maps to the shared variant). Recommend doing P2 first, then folding in
  only the fully-identical members here (`oad`, `pe_tls_index_sym`, the 3-way
  `gjmp_append`). Treat `gen_le*` and `asm_clobber` as optional cleanup.

---

## Priority 4 — `gen_bounds_epilog` shared prologue (small, guarded)

`gen_bounds_epilog` (arm/arm64/riscv64/x86_64 `-gen.c`) opens with an identical
~10-line prologue (`ngram #4808`):
```c
int offset_modified = func_bound_offset != lbounds_section->data_offset;
if (!offset_modified && !func_bound_add_epilog) return;
bounds_ptr = section_ptr_add(lbounds_section, sizeof(addr_t));
*bounds_ptr = 0;
sym_data = get_sym_ref(&char_pointer_type, lbounds_section,
                       func_bound_offset, PTR_SIZE);
```
after which each arch emits its own bounds-check call sequence. Only the prologue
is shareable, and the whole thing is behind `CONFIG_BOUNDS_CHECK`. Extract the
prologue into a helper returning `sym_data` (or `NULL` for the early-out) if
touching this area anyway; not worth a standalone change.

---

## Investigated and deliberately NOT pursued

- **i386-dis ↔ x86_64-dis disassembler:** only **2% of body bytes** are identical
  (7 tiny helpers: `get8/16/32`, `peek`, `imm`, `P`, `mcc_disasm_insn`). The bulk
  — `decode`, `modrm`, `alu_rm`, `gpr`, `sfx`, `vsize` — genuinely differs (16/32
  vs 64-bit decoding). The shared helpers are too small to be worth a shared TU.
- **`asm_compute_constraints`** (arm/arm64/i386 ~180–230 lines each): the
  **prologue** (operand init + reference resolution + priority) is near-identical,
  but the body is a large **per-arch constraint-letter `switch`** (different
  register classes, immediate predicates, memory forms per ISA) — real logic, not
  duplication. `skip_constraint_modifiers` was already hoisted (`45583894`). The
  prologue *could* be extracted but is entangled with arch locals; low value,
  matches the prior "leave the bulk as-is" call.
- **runtime `run_ctors`/`run_dtors`** (`runtime/lib/runmain.c` vs
  `runtime/win32/lib/crtinit.c`): platform-split copies with **different types**
  (`char**` vs `_TCHAR**`) that never link together. Intentional; leave as-is.
- **arch `case`-ladder "tables"** (`gen_opil`, `gen_opf`, `asm_mem_access_opcode`,
  i386 `decode`, barrel-shifter/condition-code ladders): top-ranked by the miner
  but **normalization artifacts** — same `switch` *shape*, arch-specific values.
  Confirmed by the prior pass; still not real duplication.

---

## Reproduce the cross-file dup query

```sql
-- analysis/statements.db : n-grams (n>=4) spanning >=3 distinct src/ files
SELECT g.id, g.n, g.count, COUNT(DISTINCT f.id) AS files
FROM ngram g
JOIN ngram_occ o ON o.ngram_id = g.id
JOIN func fn ON fn.id = o.func_id
JOIN file f ON f.id = fn.file_id
WHERE g.n >= 4 AND f.dir = 'src'
GROUP BY g.id HAVING files >= 3
ORDER BY files DESC, g.n DESC, g.count DESC;
```
Exact-identical bodies: hash `cparse.raw_text(fn['tokens'])` per function across
`src/**/*.c` and group; near-dups: token-multiset Jaccard ≥ 0.88.
