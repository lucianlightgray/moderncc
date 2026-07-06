# PLAN — CST Database (Concrete Syntax Tree) for mcc

Status: **design frozen, pre-implementation.** This plan implements two TODO items:

- CST Database for Debugging, LSP, and Optimization data/layers.
- CST Database uses hierarchical incremental hashes to enable bidirectional
  lookups starting from any character index in any file.

The plan is deliberately staged so a **TDD source-of-truth harness runs before
any consumer is built**, proving the CST is a pure reflection of the source
while the single-pass compiler remains the oracle.

---

## 0. North-star invariants (must hold at every commit)

1. **Byte-identical codegen.** With the CST feature compiled out *or* in, `mcc`
   emits identical object/asm/exe bytes. The CST is a pure side-effect; it never
   feeds back into a codegen decision. This is a *tested* invariant, not a hope.
2. **Zero-cost when off.** Gated by a single CMake config node → PP define. When
   off, every hook compiles to nothing (no calls, no fields, no size cost).
3. **Total self-containment.** The CST shares **no memory** with the compiler:
   its own arena allocator, its own copy of each file's source bytes, its own
   interned strings, and all cross-references expressed as **CST-internal node
   ids** — never `Sym*`, never pointers into `BufferedFile.buffer`, never a
   section pointer. The live compiler is an *oracle consulted during
   construction*, never a backing store.
4. **Pure reflection.** The CST round-trips to byte-identical source, and every
   datum in it is derivable from the source text alone. This is the milestone-1
   test surface.
5. **No new external dependencies.** Everything hand-rolled; consistent with
   mcc's self-hosting / no-dep posture.

---

## 1. Frozen design decisions

| Axis | Decision |
|---|---|
| Representation | **Flat data-oriented arrays** (SoA): `kind[]`, `parent[]`, `first_child[]`, `next_sib[]`, `width[]`, `struct_hash[]`, `trivia_hash[]`, `sym_ref[]`, reserved `slot_key[]`, leaf payload. Index-addressed; mmap-serializable 1:1. This SoA *is* the green layer; a red cursor layers on top later without reshaping. Children are **linked** (`first_child`/`next_sib`), **not** contiguous ranges, so a 5B splice is a pointer patch — no array shift. |
| Node identity | **Tagged 64-bit ids `(u32 file : u32 local)`.** SoA arrays are indexed by the bare `u32` local id; every reference that crosses a file boundary (symbol `use→def`, `IncludeDirective` target, future 4C dedup entry) stores the full 64-bit id. Reserves multi-file + dedup room now so id width never has to migrate. |
| Construction | **Side-recording** from the existing recursive descent in `mccgen.c`; oracle-only (see invariant 3). |
| Positioning | **Relative widths** (green-node style); absolute char offset never stored, never hashed. A derived **`offset→node` index is built in M2 (mandatory, not on-demand)** — LSP fires offset→node on every hover/keystroke; the index is a rebuildable accelerator over the canonical widths, patched at the 5B sweep. |
| Hashing | **Non-crypto 128-bit Merkle**, two channels: position-independent **structural** hash (drives identity/reuse/dedup/TDD) + separate **trivia/layout** hash (comment/whitespace edits). `4B` rolling hash and `4C` hash-consing are *designed-for, deferred*. |
| Incrementality | **v1 = batch rebuild**, but data structures are 5B-ready: batch rebuild is literally the incremental path with the *splice* step replaced by full rebuild. |
| Persistence | **v1 = mmap-able arena snapshot (6A) behind a versioned header** (magic + format-version + endianness + section table) from the first on-disk artifact — never a raw dump. Serialize-to a content-addressed store (6B, keyed by our structural hashes) is designed-for, deferred. Embedded KV (6C) rejected (dependency conflict); its query needs are met by hand-rolled in-memory indices. |
| Symbols | **8A now, as node-ids:** during build we consult the live `Sym` to learn a use's def-site, but store only `use-node → def-node` ids. **8C (lazy LSP resolution) next**, querying those ids. |
| Includes | **Per-file subtrees stitched by id.** Each file is its own `TranslationUnit` over its own owned buffer; `IncludeDirective` holds a cross-file node-id to the included file's root. This *is* the LSP document model and lets a single edited header reparse without rebuilding every includer. |
| Trivia | **9A + structured trivia pieces:** trivia is `(kind, rel-span)[]` attached to leaf tokens; excluded from the structural hash. `Comment` node kind **reserved** for later promotion to 9B. |
| Resilience | Error-recovery deferred to the LSP stage; `Error` and `Missing` node kinds **reserved** now so adding recovery never reshapes the node format. |

### Consumer order (each is a later milestone)
1. **TDD "pure reflection" harness** (M1–M3) — no consumer; proves fidelity.
2. **`-g` debugging** — span↔PC, reuse `mccdbg.c`.
3. **LSP** — goto/hover/semantic-tokens/diagnostics; brings 8C + resilience.
4. **Optimization layer** — attach dataflow facts to nodes.

---

## 2. Node schema (reserved kinds & layout)

Node kinds are a flat enum, grouped. Reserve now even if unused until a later
milestone:

- **Structural:** `TranslationUnit`, `Declaration`, `FunctionDef`, `Declarator`,
  `ParamList`, `StructOrUnion`, `Enum`, `TypeName`, `Initializer`,
  `CompoundStmt`, `If`/`While`/`For`/`Do`/`Switch`/`Return`/`Goto`/`Label`/
  `ExprStmt`, `Binary`, `Unary`, `Call`, `Member`, `Index`, `Cast`, `Cond`,
  `Comma`, `Paren`, `Primary` (ident/const/string/char).
- **Preprocessor (concrete):** `MacroInvocation`, `IncludeDirective`,
  `PPDirective` (generic `#…`), `PPConditional`.
- **Leaves:** `Token` (carries token-kind + owned byte span + trivia pieces).
- **Reserved for later milestones:** `Comment`, `Error`, `Missing`.

Per-node columns (SoA, one array each): `kind`, `parent`, `first_child`,
`next_sib` (children **linked**, not contiguous — splice-friendly), `width`
(bytes this node spans, **relative** — sum of children + own leaf bytes +
attached trivia), `struct_hash` (u128), `trivia_hash` (u128), `sym_ref` (tagged
64-bit `(file,local)` node-id of def-site, or `NONE`), reserved `slot_key`
(order-maintenance key for the deferred 5B epoch hash — unused/zero in v1, §3.1),
plus a leaf side-table for `Token` (token-kind, byte-span into owned source,
trivia-piece list).

**Node ids:** SoA arrays are indexed by a bare `u32` **local** id; any id that
crosses a file boundary (sym refs, include targets, future dedup entries) is the
full 64-bit `(u32 file : u32 local)`. High bits reserved now so multi-file and
4C hash-consing never force an id-width migration.

Absolute offset of a node = prefix-sum of preceding siblings' widths + parent's
absolute offset; never stored. Served by the **mandatory `offset→node` index
built in M2** (a rebuildable accelerator over the canonical widths), not an
on-demand walk — LSP queries this on every keystroke.

---

## 3. Hashing specification

Governing rule: **no absolute position feeds any hash.** This is what makes
subtree reuse, cross-file dedup, and stable TDD fingerprints possible.

- **Structural hash** `H_s(node)`:
  - Leaf: `H_s = mix(salt(token_kind), bytes(leaf_span))` — trivia excluded.
  - Internal: `h = salt(kind, child_count); for c in children: h = mix(h, H_s(c))`.
  - Order- and kind-sensitive; child-count in the salt disambiguates
    `a+b` vs `a+(b)`.
  - 128-bit, non-crypto (two 64-bit lanes / 128-bit finalizer). Self-contained
    implementation in `mcccst.c` — no dependency.
- **Trivia hash** `H_t(node)`: folds comment/whitespace bytes + widths, so
  comment- or format-only edits are detectable and localized without dirtying
  `H_s`.
- **Incremental update:** because `H_s` is a pure function of children, an edit
  rehashes only nodes on the **root path** whose child-hash multiset changed —
  O(depth). This *is* the "hierarchical incremental hash."

Deferred (reserve, don't build): **4B rolling hash** (lexer-level dirty-window
finder for 5B relex — orthogonal to the Merkle tree tool); **4C hash-consing**
(a `H_s → node` table to physically dedup identical subtrees; big win for 6B).
Node-id allocation must leave room for a dedup table later.

### 3.1 Incremental rehash: invertible epoch hash + tombstone sweep (5B-era)

The canonical Merkle `H_s` is deliberately non-invertible, so when one child
changes its parent must **re-scan all siblings** — O(fanout) per level. In C the
trees are *wide and shallow* (`TranslationUnit`, long `CompoundStmt`, big
`InitializerList` have huge fanout; nesting is shallow), so fanout, not depth, is
the dominant rehash cost. The incremental layer attacks it with a **dual-hash**,
mirroring the 4B/4A (rolling/Merkle) split:

- **Canonical `H_s`** — unchanged. The identity for TDD fingerprints and 6B
  content-addressing. Recomputed **only at the sweep**, and only over the
  *touched frontier* (O(touched) — paid at snapshot time anyway), never
  whole-tree. `H_s`-recompute must therefore be frontier-scoped, not assume the
  whole tree.
- **Invertible "epoch" hash `H_e`** — the fast incremental dirty-tracker. Combine
  step is a slot-keyed group operation:

  ```
  H_e(parent) = Σ_i  M(key_i) · H_e(child_i)   (mod 2^128),   M(key) odd
  ```

  This is subtractable in O(1) per level: replacing child `i` is
  `H_e += M(key_i)·(new − old)`; a removed child becomes a **Null tombstone**
  with `H_e = 0`, and since `M(key)·0 = 0` its term drops out *exactly*. Leaves
  keep the strong non-linear mix; only the combine is linear (Bellare–Micciancio
  incremental hashing). During an editing burst, edits do O(1) algebraic patches
  + drop tombstones into a dirty-set; **no ancestor walk at edit time**. An
  explicit later **sweep** compacts tombstones, renormalizes slot keys, and
  reconciles `H_s` over the touched frontier. N clustered edits then cost
  amortized O(depth + N) instead of O(N·depth).

  Net effect: this **changes the cost model, not the O(depth) bound** — a caller
  needing a *fresh* root hash mid-burst still pays the chain — but per-level cost
  drops O(fanout)→O(1) (the win that matters for C) and edits amortize across the
  batch. `H_s` stays the canonical identity; `H_e` is used only to localize the
  5B change-frontier and answer "did this subtree change".

  **Four caveats (design around, don't relitigate):**
  1. *Order-collisions* — must slot-bind via `M(key_i)`; never a bare sum
     (`a−b` would collide with `b−a`). Slot keys are structural ordinals, **not**
     byte offsets, so the "no absolute position in the hash" rule still holds.
  2. *Insert/delete renumbers slots* → all following `M(key_i)` terms change →
     O(fanout) again at that node. Use **stable keys** (fractional /
     order-maintenance indices) so inserts don't renumber siblings; keys drift
     and are renormalized *by the sweep*.
  3. *Weaker collision resistance* — linear combiners are fine for our own
     (non-adversarial) change-detection but riskier for 6B / cross-machine
     dedup. This is exactly why `H_e` never replaces `H_s`; `H_s` remains the
     content address.
  4. *Tombstone/width discipline* — Null must be the true group identity
     (contributes 0) **and** zero-width until swept, or offset arithmetic drifts.

  **Scope:** purely 5B-era; v1 (5A batch) has no live edits to optimize. Design-
  for-now = keep the combine behind one function, reserve an optional per-node
  slot-key field, and keep `H_s`-recompute frontier-scoped — so `H_e` +
  tombstone-sweep drop in at 5B without reshaping nodes.

---

## 4. Memory & preprocessor model (the subtle parts)

- **CST owns the source.** On first read of each file, the CST copies its bytes
  into an owned per-file buffer. All leaf spans reference *that* copy, so the CST
  survives `BufferedFile` recycling and serializes without fixups. This owned
  buffer *is* the LSP document model, and each file's subtree is a separate
  `TranslationUnit` stitched to its includers by an `IncludeDirective` cross-file
  id (§1) — so one edited header reparses without rebuilding every includer.
- **Owned arena.** All node arrays + string pool live in a `CstArena` with a
  bump allocator, independent lifetime, `free`/`snapshot`/`load` as a unit.
- **Symbol refs are node-ids.** At a use site, the live `Sym` tells us the
  def-site's source location → we map that to the def's CST node-id and store
  *that*. No `Sym*` ever enters CST memory. 8C later resolves lazily over these.
- **Preprocessor / macros.** The parser sees post-expansion tokens
  (`next()`, `mccpp.c:3874`), but a "pure reflection" CST must capture *written*
  source. Resolution: capture leaf spans at the **lexer/source boundary** where
  real file bytes are addressable; represent a macro use as a
  `MacroInvocation` node whose **span covers the macro-use text** (round-trip /
  LSP) and whose **children hold the expansion** (needed later by `-g` / opt).
  **Phasing (highest-risk item, §11):** M1 handles only the expansion-transparent
  subset; full `MacroInvocation` fidelity is a **dedicated milestone (Mμ) between
  M3 and M4**, grown under the round-trip corpus while the single-pass compiler
  is still the oracle — before symbols build on top.

---

## 5. Byte-offset facility (prerequisite)

`BufferedFile` tracks `line_num` only. Add a monotonic **byte cursor** updated
alongside `line_num` in the lexer's advance path (`mccpp.c` `handle_eob`/inbuf
and the newline bumps around `mccpp.c:800–1000`). **Decision: a monotonic
counter (one increment per consumed byte), not `buf_ptr - buffer` arithmetic** —
the counter stays correct across `handle_eob` chunk refills and macro pushback,
where the pointer-difference form silently drifts; the per-byte increment cost is
negligible. Exposed only when the CST define is on (zero-cost otherwise). This
gives every recorded leaf an exact `(file, byte-offset, length)`.

---

## 6. Integration points (hooks)

Recording is driven from the existing recursive descent. Hook macros expand to
nothing when the feature is off. Primary sites in `mccgen.c`:

- `decl()` (`10074`), `external decl` path — `Declaration` / `FunctionDef`.
- `struct_decl()` (`4248`), `post_type()` (`5348`), `type_decl()` (`5655`) —
  type structure.
- `block()` (`8402`), `gexpr_decl()` (`8337`), `lblock()` (`8321`) — statements.
- expression descent (`unary`/`expr`/`gexpr` family) — expression nodes.
- token consumption (`next`/`skip`) — leaf capture with owned span + trivia.

Pattern: a small explicit **node stack** in `mcccst.c`; each grammar function
brackets its work with `cst_open(kind)` / `cst_close()`, and leaves are pushed
at token consumption. Because the existing descent already mirrors the grammar,
the CST shape falls out of the call structure with no grammar duplication. In
debug builds, `cst_open`/`cst_close` assert stack balance (each open closed
exactly once; stack empty at end of `decl()`), catching hook-coverage drift
directly rather than only via the span-coverage test (§8.2).

---

## 7. CMake gating

Add a config node next to the diagnostics ones (`CMakeLists.txt:1087` pattern):

```cmake
mcc_config_node(MCC_CST TYPE BOOL DEFAULT OFF GROUP "Experimental"
    HELP "Build the CST database subsystem (side-recorded concrete syntax tree; \
LSP/-g/opt substrate). Off by default; codegen is byte-identical either way.")
```

- When `ON`: `target_compile_definitions(... PRIVATE CONFIG_MCC_CST=1)` and add
  `src/mcccst.c` to the sources.
- Add a `cst` preset to `CMakePresets.json` (mirrors the `diagnostics` preset)
  so CI can build/run the CST TDD suite.
- Source guards: `#if CONFIG_MCC_CST` around every hook and all new files.

---

## 8. TDD "pure reflection" harness (the M1 deliverable)

New tool `tools/csttool` (or a `mccharness cst` subcommand) + tests under
`tests/cst/`, registered via `add_test` in the root `CMakeLists.txt` (same shape
as `exec/` and `parts/`, around `CMakeLists.txt:2872/2987`). The **primary
oracle test is round-trip identity**, plus structural and hash assertions:

1. **Round-trip:** serialize CST → source bytes; assert byte-identical to input.
   The single strongest "pure reflection" proof. Run over the entire existing
   `tests/exec` and `tests/tests2` corpus (thousands of real C files).
2. **Span coverage:** child spans tile the parent with no gaps/overlaps; sum of
   widths == parent width; leaves + trivia cover every source byte exactly once.
3. **Bidirectional lookup:** for random byte indices, `offset → node → span`
   maps back onto the same offset; def↔use node-ids round-trip.
4. **Hash invariance:** structural hash is unchanged by whitespace/comment-only
   edits; changes iff token structure changes. Identical subtrees share a hash.
5. **Codegen-identity gate:** build `mcc` with and without `CONFIG_MCC_CST`;
   assert identical output over the corpus (protects invariant 1).
6. **Snapshot round-trip:** dump arena (6A, versioned header) → reload → identical
   tree + hashes; a version/endianness mismatch is rejected cleanly, not misread.

---

## 9. Milestones & task breakdown

**M0 — Scaffolding (no behavior).**
- `MCC_CST` config node + preset; `CONFIG_MCC_CST` guards; empty
  `src/mcccst.{c,h}`; `CstArena` + bump allocator; hook macros that expand to
  nothing. Prove zero-cost-off: byte-identical `mcc` with the flag on (hooks
  still no-op) and off.

**M1 — Leaf capture + owned source + round-trip.**
- Byte-offset facility (§5). Own each file's bytes. Capture `Token` leaves with
  owned spans + trivia pieces. Minimal tree (flat list under `TranslationUnit`).
- Deliver the round-trip test (§8.1) and span-coverage (§8.2). This is the first
  green source-of-truth signal.

**M2 — Structure + relative widths + bidirectional lookup.**
- Open/close nodes across the `mccgen.c` grammar (§6). Relative widths; tagged
  `(file,local)` node-ids; the **mandatory `offset→node` index** (§2, not an
  on-demand walk). Debug-build hook-balance asserts (§6). Deliver §8.3.

**M3 — Hashing + snapshot.**
- 128-bit structural + trivia channels (§3); O(depth) rehash kept
  **frontier-scoped** (§3.1); reserve the per-node `slot_key` column (unused in
  v1). Arena snapshot (6A) save/load behind a **versioned header** (§1
  Persistence). Deliver §8.4 and §8.6. **End of M3 = "CST complete" for
  source-of-truth purposes**; single-pass oracle can now be cross-checked.

**Mμ — Macro fidelity (highest-risk, §4/§11).**
- Promote the expansion-transparent subset to full `MacroInvocation` nodes
  (use-text span + expansion children). Grow under the round-trip corpus with the
  single-pass compiler still the oracle, *before* symbols build on top.

**M4 — Symbol refs (8A as node-ids).**
- Consult live `Sym` at build to record `use→def` node-ids. Def↔use round-trip
  test.

**M5+ — Consumers (separate future plans):** `-g` (span↔PC via `mccdbg.c`) →
LSP (8C lazy resolution + `Error`/`Missing` resilience + incremental 5B splice +
6B content-addressed store) → optimization layer.

---

## 10. Deferred / reserved (do not build yet, but don't preclude)

- **5B incremental splice** — enabled by relative widths + structural hash;
  batch rebuild is the same code minus the splice. Needs: re-lex-window finder
  (4B rolling hash), parser error-recovery (LSP-era), `Error`/`Missing` nodes.
- **6B content-addressed store** — serialize nodes keyed by structural hash;
  cross-version/file dedup; backbone for hot-reload / "reconciled CST snapshots"
  (see TODO "Later" notes). Keep as archival format layered over 6A working image.
- **4C hash-consing** — physical subtree dedup table keyed by `H_s`.
- **Invertible epoch hash `H_e` + tombstone sweep** (§3.1) — O(1)-per-level
  incremental rehash for 5B live edits; dual-hash alongside canonical `H_s`.
  Reserve the per-node slot-key field and keep `H_s`-recompute frontier-scoped
  now; build at 5B.
- **9B trivia-as-nodes** — promote reserved `Comment` kind if opt/LSP needs it.
- **6C embedded KV** — rejected (dependency); LSP queries synthesized from
  hand-rolled indices (`offset→node`, `name→def`, `def→uses`) over the flat
  arrays, durable via 6B.

---

## 11. Risk register

- **Macro-fidelity vs. pure reflection** (§4) — highest-uncertainty item; grow
  under the round-trip test rather than up-front.
- **Hook coverage drift** — a grammar path without a hook silently drops a
  subtree; span-coverage test (§8.2) catches gaps/overlaps as failures.
- **Zero-cost-off regressions** — guard with the codegen-identity gate (§8.5) in
  CI on every commit.
- **Width/offset arithmetic bugs** — the tiling invariant (§8.2) is the tripwire.
