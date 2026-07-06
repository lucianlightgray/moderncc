# IMPLEMENTATION — CST Database, sliced for parallel build

Companion to `PLAN.md`. `PLAN.md` freezes *what* and *why* and lays the work out
as horizontal milestones (M0→M4). This document re-cuts the same work into
**vertical slices**: self-contained units, each with a published interface, that
can be built and tested **in isolation against stubs** — then **woven together**
in a small number of explicit integration passes.

Nothing here changes a decision in `PLAN.md`; it is a build order. Every slice
cites the `PLAN.md` section that governs it. The invariants in `PLAN.md §0` bind
every slice and every weave.

---

## 0. The slicing principle

The CST splits cleanly along one seam:

- **Pure slices** — no dependency on the compiler. They operate on synthetic
  in-memory trees and byte strings. They can be written, unit-tested to green,
  and frozen *before mcc is touched at all*. (Node store, hashing, geometry,
  serialization, reflection.)
- **Compiler-side slices** — live in `mccpp.c` / `mccgen.c`, depend on mcc
  internals but **not on each other's CST logic**. (Byte cursor, owned source,
  recording hooks, symbol consult, macro capture.)
- **Weaves** — the only steps that require two live subsystems at once. Kept few
  and explicit; each ends on a named invariant gate.

The enabling trick: **every pure slice is exercised by a synthetic-tree builder
in the harness, not by real compiler output.** A hand-built 20-node tree with
known widths and bytes is enough to prove hashing, geometry, indexing, and
serialization correct. Real source only enters at the first weave — by which
point the libraries it flows into are already trusted.

The seam that makes this possible is the header contract in §3: pure slices
publish functions; compiler-side slices call only the hook macros. Neither side
reaches across.

---

## 1. Slice catalog

| Slice | Name | Kind | Depends on | `PLAN.md` | Built/tested against |
|---|---|---|---|---|---|
| **S0** | Gating & harness skeleton | build | — | §7, §8 | on/off byte-identity |
| **B** | Node store core (arena, SoA, ids) | pure | S0 | §1, §2 | synthetic tree builder |
| **C** | Hashing library | pure | S0 | §3, §3.1 | golden vectors + property tests |
| **D** | Geometry & offset→node index | pure | B | §1, §2, §5 | synthetic trees |
| **E** | Serialization (snapshot + reflect) | pure | B (+G stub) | §1, §8.1, §8.6 | synthetic arenas |
| **F** | Byte-offset facility | compiler | — | §5 | known char positions |
| **G** | Owned source & trivia | compiler | B, F | §4, §1 (Trivia) | file → owned buffer |
| **H** | Recording hooks (the weave) | weave | B, D, F, G | §6 | live grammar |
| **I** | Symbol refs | compiler | H, B | §1 (Symbols), §4 | def↔use round-trip |
| **J** | Macro fidelity (Mμ) | compiler | H, F | §4, §11 | round-trip corpus |

`kind = pure` slices have **zero** `#include` of compiler headers and can compile
into the harness alone.

---

## 2. Per-slice detail

### S0 — Gating & harness skeleton
**Files:** `CMakeLists.txt` (config node + `add_test`), `CMakePresets.json`,
`src/mcccst.{c,h}` (empty but present), `tools/csttool/` skeleton,
`tests/cst/`.
**Delivers:** `MCC_CST` config node (`PLAN §7`), `CONFIG_MCC_CST` guards, the
`cst` preset, and the **codegen-identity gate** (`§8.5`) wired into CI so it runs
from commit one — before there is anything to break, so it can only ever catch a
regression. `csttool` links `mcccst.c` and the synthetic-tree builder.
**Done when:** `mcc` builds byte-identical with the flag on (hooks no-op) and off
(`§0.1`, `§0.2`), and an empty `tests/cst` target is registered and green.

### B — Node store core *(pure)*
**Files:** `src/mcccst.c` (arena + SoA), `src/mcccst.h` (id + store API).
**Delivers:** `CstArena` bump allocator; the SoA columns of `PLAN §2` including
the reserved `slot_key` column; the tagged-id scheme (`u32 local` array index,
64-bit `(file,local)` for anything crossing a file); `cst_node_open/close`,
`cst_leaf`, `append_child`, and column accessors. Linked children
(`first_child`/`next_sib`) per `§1` — no contiguous ranges.
**Isolation test:** harness builds synthetic trees directly through the API;
asserts parent/child/sibling topology, id encode/decode round-trip, arena
free/reset, and reserved columns default to zero.

### C — Hashing library *(pure)*
**Files:** `src/mcccst_hash.c` (or a section of `mcccst.c`).
**Delivers:** the 128-bit non-crypto hash (two 64-bit lanes + finalizer, `§3`);
`cst_hash_leaf` (kind-salt + leaf bytes, trivia excluded), `cst_hash_internal`
(`salt(kind, child_count)` then Merkle-fold over child hashes), `cst_hash_eq`,
and the **frontier-scoped** `cst_rehash_frontier` (`§3.1`). The epoch-hash `H_e`
is **not** built — only its seam is reserved: the combine stays behind one
function and the `slot_key` column (owned by B) is left untouched.
**Isolation test:** golden vectors for stability; property tests — trivia/format
change leaves `H_s` fixed; `a+b` ≠ `a+(b)` (child-count salt); identical subtrees
hash equal (`§8.4`). All on synthetic byte strings and child-hash arrays; no tree
required.

### D — Geometry & offset→node index *(pure)*
**Files:** `src/mcccst_geom.c`.
**Delivers:** relative-width finalization on `cst_node_close`; `cst_abs_offset`
(prefix-sum walk); and the **mandatory** `offset→node` index (`§1` Positioning,
`§2`) — built once, queried O(log n) or better, a rebuildable accelerator over
the canonical widths.
**Isolation test:** synthetic trees with known widths; the tiling invariant
(`§8.2` — child widths sum to parent, no gaps/overlaps); random abs-offset →
`cst_node_at` → span → offset round-trip (`§8.3`).

### E — Serialization *(pure)*
**Files:** `src/mcccst_io.c`.
**Delivers:** the **versioned snapshot** (magic + format-version + endianness +
section table, `§1` Persistence) `save`/`load`; and `cst_reflect` — the CST→source
emitter for the round-trip oracle (`§8.1`). `reflect` consumes owned-source spans;
until G exists it runs against a **stub buffer** the harness fills.
**Isolation test:** synthetic arena → save → load → identical columns + hashes;
deliberate version/endianness mismatch is rejected cleanly (`§8.6`);
`reflect(build_from(bytes)) == bytes` for hand-built trees over a stub buffer.

### F — Byte-offset facility *(compiler-side, isolated)*
**Files:** `src/mccpp.c` (advance path near `handle_eob`, `mccpp.c:800–1000`).
**Delivers:** the monotonic **byte cursor** (`§5`) — one increment per consumed
byte, robust across chunk refills and macro pushback; exposed only under
`CONFIG_MCC_CST`.
**Isolation test:** a debug probe that, for a fixture file, asserts the cursor at
each token boundary equals the known character position — across an `#include`, a
multi-chunk file, and a macro expansion. **No CST needed** to validate this.

### G — Owned source & trivia *(compiler-side)*
**Files:** `src/mcccst.c` (owned buffers), leaf-trivia classifier.
**Delivers:** per-file byte copy into arena + string pool (`§4`), the LSP
document model; trivia captured as `(kind, rel-span)[]` on leaves, excluded from
`H_s` (`§1` Trivia); per-file subtree ownership so `IncludeDirective` can stitch
by cross-file id (`§1` Includes).
**Isolation test:** feed a file's bytes → owned buffer is byte-identical and
outlives a simulated `BufferedFile` recycle; trivia classification of a
whitespace/comment fixture yields the expected piece list.

### H — Recording hooks *(first weave — see §5)*
**Files:** `src/mccgen.c` hook sites (`§6`: `decl` `10074`, `struct_decl` `4248`,
`post_type` `5348`, `type_decl` `5655`, `block` `8402`, `gexpr_decl` `8337`,
`lblock` `8321`, the `unary`/`expr`/`gexpr` family, `next`/`skip`).
**Delivers:** the `cst_open`/`cst_close` node stack and the hook macros that
bracket each grammar function; leaf capture at token consumption; **debug-build
balance asserts** (`§6`). Connects B+D+F+G to the live descent.
**Test:** this is where real source first flows — see Weave-1 gate (§5).

### I — Symbol refs
**Files:** `src/mccgen.c` use/def sites; `src/mcccst.c` `name→def`/`def→uses`.
**Delivers:** at a use site, consult the live `Sym` for its def location → map to
the def's CST node-id → store the tagged `(file,local)` `sym_ref` (`§1` Symbols,
`§4`). No `Sym*` ever enters CST memory.
**Test:** def↔use node-ids round-trip over the corpus (`§8.3`).

### J — Macro fidelity *(Mμ)*
**Files:** `src/mccpp.c` / `mccgen.c` PP boundary.
**Delivers:** `MacroInvocation` nodes — span covers the written use-text,
children hold the expansion (`§4`). Grown incrementally under the round-trip
corpus, the highest-risk item (`§11`), while the single-pass compiler is still
the oracle.
**Test:** round-trip stays byte-identical as macro cases are switched from the
transparent subset to full fidelity.

---

## 3. The seam (interface contract)

Pure slices publish these; compiler-side slices call **only** the hook macros.
This is the whole contract that lets the two sides be built apart. Sketch — exact
signatures firm up in S0/B, but the shape is fixed:

```c
/* ---- ids (B) ---- */
typedef uint32_t CstLocal;                 /* SoA array index, per file      */
typedef uint64_t CstId;                    /* (file<<32)|local, cross-file   */
#define CST_NONE ((CstLocal)0xffffffffu)

/* ---- store (B) ---- */
typedef struct CstArena CstArena;
CstLocal cst_node_open (CstArena*, uint16_t kind);   /* push build stack     */
void     cst_node_close(CstArena*, CstLocal);        /* pop; finalize width  */
CstLocal cst_leaf      (CstArena*, uint16_t tok, uint32_t off, uint32_t len);
uint16_t cst_kind (const CstArena*, CstLocal);
uint32_t cst_width(const CstArena*, CstLocal);       /* relative             */

/* ---- hashing (C) ---- */
typedef struct { uint64_t lo, hi; } CstHash;
CstHash cst_hash_leaf    (uint16_t tok, const uint8_t* b, uint32_t n);
CstHash cst_hash_internal(uint16_t kind, const CstHash* child, uint32_t n);
void    cst_rehash_frontier(CstArena*, const CstLocal* touched, uint32_t n);

/* ---- geometry (D) ---- */
uint32_t cst_abs_offset(const CstArena*, CstLocal);
void     cst_index_build(CstArena*);
CstLocal cst_node_at    (const CstArena*, uint32_t abs_off);

/* ---- serialization (E) ---- */
int       cst_snapshot_save(const CstArena*, const char* path);
CstArena *cst_snapshot_load(const char* path);       /* NULL on version skew */
size_t    cst_reflect(const CstArena*, CstLocal root, uint8_t* out, size_t cap);

/* ---- owned source (G) ---- */
uint32_t  cst_own_file(CstArena*, const char* name, const uint8_t*, size_t);

/* ---- hooks (H) — the ONLY surface the compiler sees ---- */
#if CONFIG_MCC_CST
#  define CST_OPEN(k)   cst_hook_open(k)
#  define CST_CLOSE()   cst_hook_close()
#  define CST_LEAF(tk)  cst_hook_leaf(tk)          /* pulls cursor from F     */
#else
#  define CST_OPEN(k)   ((void)0)
#  define CST_CLOSE()   ((void)0)
#  define CST_LEAF(tk)  ((void)0)
#endif
```

Rule: `mccgen.c`/`mccpp.c` reference **only** `CST_*` macros and the byte cursor.
All structure, hashing, geometry, and IO stay inside `mcccst*.c` behind the
functions above. This keeps `§0.2` (zero-cost off) mechanical and lets any pure
slice be swapped or rebuilt without touching the compiler.

---

## 4. Dependency graph & parallel lanes

```
                 S0  (build gating + harness + codegen-identity gate)
                  │
      ┌───────────┼───────────────┬───────────────┐
      │           │               │               │
      B ──────────┤               C (pure,        F (compiler,
   (pure store)   │             independent)     independent)
      │           │
      ├── D (geometry, needs B)
      ├── E (serialization, needs B; reflect on G-stub)
      └── G (owned source, needs B + F)
                  │
                  ▼
      ══════════ WEAVE 1: H (hooks) ══════════  → real CST from real source
                  │
      ══════════ WEAVE 2: hash + snapshot online ══════════
                  │
      ══════════ WEAVE 3: I (symbols) + J (macros) ══════════
                  │
      ══════════ FINAL: corpus + hardening ══════════
```

**Three lanes run fully in parallel after S0:**
- *Pure lane:* B → {C, D, E}. One or two people; no compiler knowledge needed.
- *Cursor lane:* F, then G. Compiler-side, no CST algorithm knowledge needed.
- *C* is independent of everything but S0 and can be first-done.

The lanes only meet at Weave 1.

---

## 5. Weaving passes

Each weave is small, ordered, and ends on a gate from `PLAN §8`. A weave does not
start until its input slices are individually green.

### Weave 1 — Structure online *(H connects the libraries to the grammar)*
Wire the `CST_*` hooks through the `mccgen.c` sites (`§6`), pulling spans from F,
ownership from G, storing into B, finalizing widths via D.
**Gates:** round-trip identity (`§8.1`) and span-coverage / tiling (`§8.2`) over a
starter subset, then the **full `tests/exec` + `tests2` corpus**. Debug
hook-balance asserts (`§6`) active. This is the first green "pure reflection"
signal — `PLAN` M1+M2 realized together, because B/D/G were already trusted.

### Weave 2 — Hash & snapshot online
Attach C at `cst_node_close` (bottom-up `H_s`/`H_t`), and E at end-of-parse.
**Gates:** hash invariance (`§8.4`) and snapshot round-trip incl. version/endian
rejection (`§8.6`). End of Weave 2 = **"CST complete" for source-of-truth**
(`PLAN` M3).

### Weave 3 — Symbols & macros
Land I (def↔use ids, gate `§8.3`) and J (Mμ macro fidelity) — in that order, so
symbols resolve over a stable tree before macro nodes reshape spans. J grows
case-by-case under the still-green round-trip corpus.

### Final — corpus & hardening
Full-corpus run of every gate; re-confirm `§0.1`/`§0.2` via the codegen-identity
gate from S0; `§11` risk items (macro fidelity, hook drift, zero-cost-off,
width arithmetic) each pinned to their tripwire test.

---

## 6. Test strategy per slice

| Slice | How it's proven **without** its neighbors |
|---|---|
| S0 | flag on/off byte-identity; empty test target registers |
| B | synthetic-tree builder asserts topology + id round-trip |
| C | golden hash vectors + invariance properties on raw byte strings |
| D | tiling invariant + random offset round-trip on hand-built trees |
| E | arena save/load equality; version-skew rejection; reflect over stub buffer |
| F | cursor == known char position across include/chunk/macro fixtures |
| G | owned buffer byte-identical + survives recycle; trivia piece list |
| H | first real-source round-trip + span-coverage (Weave-1 gate) |
| I | def↔use id round-trip over corpus |
| J | round-trip stays byte-identical as macro cases go full-fidelity |

The synthetic-tree builder in `tools/csttool` is itself a deliverable of S0/B: it
is the substrate that makes B–E provable in isolation.

---

## 7. Mapping back to `PLAN.md` milestones

| `PLAN` milestone | Realized by |
|---|---|
| M0 scaffolding | **S0** |
| M1 leaf + owned source + round-trip | **F + G**, then **Weave 1** |
| M2 structure + widths + lookup | **B + D**, landed at **Weave 1** |
| M3 hashing + snapshot | **C + E**, landed at **Weave 2** |
| Mμ macro fidelity | **J** (Weave 3) |
| M4 symbol refs | **I** (Weave 3) |

The milestones remain the *contractual* checkpoints and their gates are
unchanged; the slices are how the work is parallelized to reach them. Where the
milestone view builds strictly bottom-to-top, the slice view builds the pure
libraries and the compiler cursor **concurrently** and pays the integration cost
in three named weaves instead of spreading it across every milestone.
