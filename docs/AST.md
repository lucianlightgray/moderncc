# AST — an intention IR alongside the CST

Status: **plan / design** (curate freely). Companion to the CST subsystem
(`src/mcccst.{c,h}`). Where the **CST** is byte-faithful *concrete* syntax, the
**AST** is *intention*: desugared, type-resolved, post-preprocessor. Guarded by
CMake `CONFIG_AST` (ON by default), built as a pure side-channel like the CST —
the compiler never *has* to read it (`-O0` stays a one-pass parse+compile+link
flow); `-O1+` consumes it.

---

## 1. Founding principle: two tiers, split by "does it depend on the machine?"

The IR is **not single-altitude**. Everything the target dictates lives *below*
the intention layer.

| Tier | Contains | Target-specific? |
|---|---|---|
| **Intention (the AST proper)** | 15 node kinds below; typed, flat control, tree expressions | no — portable, optimizable, inlinable |
| **Machine / lowering** | addressing modes, register/stack placement, the C ABI (`ArgOut`/`CallTransfer`/`ResultIn`/sret), `StackAlloc` | yes — realized at lowering; emitted inline at `-O0`, after optimization at `-O1` |

Addressing modes, storage class, and the C ABI are the three big things that stay
*below*. This is the "separate codegen/asm/machine/host unless desired" goal.

---

## 2. Node inventory — 15 intention kinds

```
Structural  TranslationUnit  BasicBlock
Terminators If  Jump  Return
Values      Ref  Literal  Load  Store  Unary  Binary  Convert  Invoke  InitList
Recovery    Poison
```

Types/symbols are **not nodes** — they live in the `CType`/`Sym` side-table.
`Label`, lexical `Block`, and `Decl` **dissolve** (see below).

### The compound-node framework

Every candidate for a "compound" node resolves one of three ways:

| Outcome | Trigger | Exemplar |
|---|---|---|
| **Decompose** | fuses orthogonal intentions / smuggles a machine concern | `FetchOffset` → `Load`/`Store` + pointer-`Binary` |
| **Triage / evict** | some permutations are *other* operations (or *not* operations) | `Cast` → `Convert` + `Binary(!=0)` + discard; `Decl` → `Sym` + liveness + `Store`/blob/BB |
| **Keep (+ cheap predicate)** | genuinely one intention *and* the group enables a lowering the parts would force you to rediscover | `InitList` (blob/`memset` coalescing); no-op `Convert` via `is_noop()`; `neg`/`bitnot` in `Unary` |

Guiding rule: **fold a permutation out when it's secretly a *different* operation**
(`!x` is a compare, bool-ify is a compare, decay is an address); **keep it when it's
a genuine primitive** (`neg`/`bitnot` are single-operand machine ops).

---

## 3. Memory model — the spine

**`Ref` yields the *address* of a named object. Every read is an explicit `Load`;
every write is a `Store`. No hidden lvalue-to-rvalue conversion.** Address math is
plain pointer `Binary`; addressing modes (`base+idx*scale+disp`) are recovered by
the backend (a machine concern). At `-O0`, `Load` of a register-resident local
lowers to no memory op — the backend elides it.

| Source | AST |
|---|---|
| `x` (read) | `Load(Ref x)` |
| `x = e` | `Store(Ref x, e)` |
| `a[i]` | `Load(a + i*scale)` |
| `a.b.c` | `Load(&a + const)` |
| `*p` | `Load(p)` |
| `&a[i]` | `a + i*scale` (a pointer value) |
| `x += e` | `Store(Ref x, Load(Ref x) + e)` |
| `i++` (post) | `t = Load(Ref i); Store(Ref i, t+1); value = t` |
| `s.bf = v` (bitfield) | `Store(A, (Load(A) & ~m) | ((v<<b) & m))` — desugared shift/mask |

`Load`/`Store` carry the access `CType` (width, `volatile`/`_Atomic`/`restrict`,
alias/TBAA info). No `FetchOffset`, no lvalue "modes", no bitfield node.

---

## 4. Storage & liveness — the backend places, subject to constraints

**Storage class is a backend decision, subject to source-mandated constraints.**
The AST expresses values, types, and lifetimes; the backend assigns
register/stack/section.

- **Lifetime rides on liveness** — a value's birth is its first def, death its
  last use, computed by backward dataflow. No positioned "declaration" node.
- **Placement** (register vs stack slot vs `.data`/`.bss`) is the allocator's, by
  spill weights (access frequency / loop depth). Spill slots get fixed
  compile-time offsets and disjoint lifetimes share them — **no LIFO hazard.**
- **Constraints carried on the value/type:** `volatile`, `_Atomic`, `register`
  (no-address), **address-taken** (`&` seen → must be memory-backed).
- **Duration & linkage are *not* placement** — they're semantics (static vs
  automatic changes program meaning) and link-time facts. They ride on the `Sym`,
  and are largely encoded *structurally* (side-table Sym = static/external;
  in-stream = automatic). The binding never names a storage class.
- **"heap" is not a declaration concept** — heap objects are anonymous, born from
  `Invoke(malloc)`.

### VLA / `alloca` — the one exception to "lifetime = liveness"

Runtime-*sized* allocations can't be fixed frame slots; they move SP at runtime and
must unwind **LIFO** (which liveness order does *not* guarantee — an outer VLA
last-used before an inner one would corrupt the stack). So a VLA is a genuine
runtime op at the machine tier: `StackAlloc`(size)→pointer, paired
`StackSave`/`StackRestore` emitted by the front-end at lexical scope edges (it knows
the nesting; the CST has it). Ordinary values are unaffected.

---

## 5. Control flow — flat CFG

`BasicBlock` = an ordered list of **control-free tree-expression ops** (`Store`,
`Invoke`, or an effectful expr), terminated by **exactly one** of `If` / `Jump` /
`Return`. A function is a graph of BasicBlocks. `Label` is subsumed by BasicBlock
identity.

- **`If(cond, L_true, L_false)`** — conditional branch, **two explicit targets** (no
  implicit fall-through). Flag `switchable` marks a switch cascade.
- **`Jump(target)`** — unconditional (goto/break/continue/back-edge/merge).
- **`Return(value?)`** — the **callee-side dual of `Invoke`** and the template's
  *continuation-hole*. Under always-inline it **folds**: each `Return(v)` at a
  spliced site becomes `Store(result_slot, v); Jump(continuation)` (void → just the
  `Jump`); multiple returns share one continuation. It **survives only at a
  non-inlined standalone** (external/address-taken fn, recursion bottom-out), where
  it can't be a `Jump` — the target is the **runtime return address**, not a static
  block. There it lowers to the ABI epilogue (frame teardown, `StackRestore`,
  callee-saves) + `ret`. The return address is machine-tier state (pushed by
  `CallTransfer`, consumed by `ret`); the AST never names it, just as `Invoke` never
  names it.

Everything desugars here: `while`/`for`/`do`/`switch` → `If`+`Jump`; `switch` = a
`switchable`-`If` cascade testing one value (jump-table is an `-O1` template;
fall-through is free; `default` = the final false-edge). **Control-flow operators
`&&`/`||`/`?:`/`,` lower to CFG** (hoisted out, leaving temps) — only control-free
operators remain as tree nodes. `!x` → `Binary(==, x, 0)`; all 0/1-producing ops are
comparisons in one family. Op order within a block encodes C sequence points.

---

## 6. `Cast` → `Convert` (triage)

`Convert` = genuine value conversions only (integer/fp resize, int↔fp, `_Complex`
build/extract, and zero-cost reinterprets distinguished by a cheap `is_noop(src,dst)`
predicate). The impostors are evicted:

- `(_Bool)x` → `Binary(!=, x, 0)` (a comparison; folds into `If` uniformly).
- `(void)x` → discard by position (no node).
- array/function decay → a no-op `Convert` of the address.
- qualification / enum↔int → no-op `Convert` or nothing.

Usual-arithmetic-conversions become explicit `Convert` nodes, so every `Binary` sees
same-typed operands.

---

## 7. `Decl` dissolves

`Decl` was ~90% a compile-time binding with a small runtime tail. Evict by
executability:

| Permutation | Footprint | Goes to |
|---|---|---|
| typedef, tag, `extern`, prototype | none | `Sym` side-table only |
| static / global var | link-time data | `Sym` + data blob (+ relocations) |
| local var | value + init code | liveness + hoisted `Store`/`InitList` |
| parameter | ABI live-in | liveness (init = ABI arg N) |
| function definition | code | `Sym` + BasicBlock graph |

The `Sym` (carrying its CST source range) is the anchor `-g`/diagnostics/CST-recon
want. `static` (internal linkage) may be anchored just above its topmost usage block
(function-local case; file-scope statics used by several functions stay
module-level). `TranslationUnit` = ordered `Sym`s defined by data blobs or BB graphs.

`InitList` is **kept** (grouping enables blob/`memset` coalescing): sparse
(zero implicit), lowering does constant-region coalescing, static blobs carry
relocations, string-init is element-wise, delivered brace-normalized.

---

## 8. Calls — abstract `Invoke` (intention) → ABI at lowering

At parse time you can't know inlinability or address-escape, so the intention tier
keeps **one abstract `Invoke`** (callee + typed arg *values* + result; direct/opaque;
variadic). Argument conversions (default promotions, param coercions) are `Convert`
nodes; a by-value struct arg is an aggregate `Load` (the copy is intention, the
placement is ABI).

At lowering, `Invoke` resolves by **boundary**, not resolution:

- **internal + inlinable** → inlined (vanishes into the callee's blocks).
- **internal + not inlinable** (recursion, cold/huge) → internal transfer, **private
  convention** (you own both sides).
- **boundary** (external linkage / address-taken / extern / function-pointer /
  varargs-to-spec) → the C-ABI **`Call`** decomposition: `ArgOut`(→ABI slot) ×N +
  sret hidden-pointer + `CallTransfer`(direct rel32 | indirect GOT/PLT) +
  `ResultIn`. **`Call` exists only as the ABI boundary mechanism.**

`extern` is *resolved* yet still a boundary `Call` — resolution gates only the
transfer form, boundary gates whether the ABI exists. Inline-expanded builtins
(`alloca`, `stacksave`, `__builtin_expect`) are **ops, not calls**; only libcall
builtins (`memcpy`, soft-float) are `Call`s.

---

## 9. Strict always-inline — virtual, over a shared template store

Inlining is **virtual** (template + binding + lazy render), not physical
duplication — reusing the CST's content-addressed `store`/`binding`/`render` engine
(ideally factored into a structure-agnostic library shared by both).

| CST recursive include | AST always-inline |
|---|---|
| SourceFile template, hash-consed by `H_s` | function-body template, hash-consed by `H_s` |
| include site → template id | call site → template id |
| binding: `#if` branch select | binding: constant-arg branch select (per-site dead-branch elim) |
| binding: child include | binding: child inline |
| self-include, depth-gated → 1 template | recursive fn → 1 template, cyclic binding |
| `cst_render` → bytes | `ast_render` → specialized CFG |

Effects: one template + N cheap bindings for N call sites; **per-site specialization
/ constant-prop falls out for free** (it's branch-select).

### Cycle detection = the instance hash

**Instance hash = `H_s` (structural) ⊕ context hash (binding state)** — identical to
the CST's `(template, binding)` identity. The `(template × structural-context)` state
space is **finite**, so by pigeonhole the instance hash *must* repeat → the unroller
**always terminates** (compile-time termination is guaranteed even when the *program*
doesn't halt — the non-termination is faithfully preserved as a loop/recursion).
**Hash structure, never concrete values.**

Algorithm — keep the current unroll **path** (ancestor instance hashes):

- hash ∈ ancestor stack → **back-edge**. *Tail position* (empty continuation) →
  register-reusing `Jump` (frame reused, no enter/exit = tail-call-to-loop); the
  base-case `If` is the loop exit. *Non-tail* (e.g. tree recursion `fib`) → runtime
  transfer (needs a stack for the continuation).
- hash ∈ global emitted set but not an ancestor → **shareable code** → outline (call).
- else → emit fresh.

### Totality

- **Preprocessor cannot infinitely unroll** — macro expansion is total (C11
  §6.10.3.4p2, "blue paint"); `#include` is depth-capped (§5.2.4.1 → diagnostic).
  A recursive `#include` doesn't loop but can *permute* into `O(b^d)` distinct
  contexts; the instance hash dedups repeats and the depth cap bounds the rest.
  This **promotes** the parked PP-JIT idea (execute → one deterministic trace vs
  enumerate the permutation space) but does not necessitate it.
- **C functions** can express non-termination; structural hashing still halts the
  *compiler* and emits a faithful loop/recursion.

### Escaped identities & outlining

An address-taken / external-linkage function needs a **materialized C-ABI
standalone** in addition to its inlined renderings. Physical rendering at N sites
duplicates *machine code* (representation is deduped, code is not) — the size-relief
valve is to **outline** (materialize + call), i.e. `Call` reused internally.

### Depth `k` — resource governor, not correctness

The instance hash guarantees termination; `k` only caps combinatorial blowup
(permutation explosion / over-eager specialization). No statistical law; set from a
code-size budget: worst case `O(b^k)`, so **`k ≈ log_b(budget)`** (binary recursion,
~256× budget → `k=8`, where GCC's `max-inline-recursive-depth` landed). Context
sensitivity: `k=1` (k-CFA sweet spot). **Recommended: conservatively low (1–2)** —
`k` fires only on pathology.

---

## 10. `-O0` vs `-O1` — one emitter, two drivers

**The "shared emit core" is not something carved out of `gen_op`; it is the vstack
API that already exists.** `vpush*` / `gen_op` / `gv` / `gsym` / `gjmp` is already a
clean stack-machine interface with **no parser coupling** (`gen_op` operates purely
on the vstack — it never touches `tok`/`next()`/parser state). Today that API has
exactly one driver: the recursive-descent parser, streaming, calling the ops as it
goes. `-O1` just **adds a second driver** that walks the optimized AST and calls the
*same* ops.

- **`-O0`** — parser → vstack ops → bytes. Skips AST build; one-pass; byte-identical
  to today; real calls, greedy storage decided by code-flow, deliberately. *Untouched.*
- **`-O1+`** — parser → AST-builder (vstack emit no-op'd) → template rewrites &
  always-inline render to fixpoint → **AST-replay walker** → *same* vstack ops → bytes.
  The replay walker holds the **whole** optimized AST, so unlike the streaming parser
  it can act with foreknowledge.

The AST is arguably a *more* natural driver than the parser: the parser reconstructs
the expression tree implicitly via recursion; the AST holds it explicitly. This is why
§15's oracle is sharp — **`-O1`-with-zero-templates must reconstruct the same vstack
op-sequence the parser would emit**, so the bytes match `-O0`. Faithful re-emission is
a directly testable invariant.

### The one boundary: rewrites are free, placement is not

Replay hands you two *different* classes of optimization; only one comes for free.

| Class | Lives | Free via replay? |
|---|---|---|
| **AST rewrites** — const-fold, algebraic, dead-branch, jump-table, inline/specialize (all of §12) | in the AST, *above* the emitter | **Yes.** The rewriter changes the tree; replay emits the better tree. |
| **Placement** — register promotion, spill weights, static localization (§4) | *inside* `gv`/the allocator, *below* the AST | **No.** Replaying through the greedy `-O0` allocator yields `-O0`'s greedy placement. |

Placement is the only part not carried by the tree. But the replay walker is the
*right* place to fix it — better than the parser ever could be — because it sees the
whole AST: run backward liveness first, then **steer** the emitter (keep this value in
a register across this span, spill that one). That changes *how you drive* the emitter,
not the emitter's guts — still no surgery on `gen_op`. Hence the phasing:

- **First** — replay driver + the zero-template invariant (proves faithful
  re-emission against `-O0`). Structural rewrites then land on top, each free.
- **Later** — teach the replay driver to consult liveness and steer placement. The
  only part that touches allocation, and it is **additive** (a smarter driver), not a
  rewrite of the emit core.

---

## 11. Building the AST — typed-CST → AST, lowered lazily

The CST solved single-pass construction via deferred leaf-capture + retroactive
wrapping. The AST needs *typed values + basic blocks*, so it is **lowered from the
CST**, not emitted in parallel with the parser.

| Decision | Options | Choice |
|---|---|---|
| Construction path | parallel parse-emission · **lower from the CST** | post-D1–D5 the CST already has `If`/`While`/`Binary`/`Call`… nodes; lowering from it leaves the gnarly parser untouched and realizes "reduce the CST to intention" |
| Typing | re-derive · **reuse parse-time types** ("typed CST") | annotate CST expr/decl nodes with the `CType`/`Sym` the vstack already computes; re-deriving would duplicate the semantic engine |
| Build timing | eager side-channel · **lazy, only when -O1 asks** | the AST's only consumer is -O1 codegen; building at -O0 is pure overhead (this *refines* the intro's "side-channel" wording) |
| -O0 annotation cost | always annotate · **-O1-gated** | -O0 pays nothing |

**Net: typed-CST → AST, built lazily at -O1.** New work = typed-CST annotation hooks
(mirror the existing `cst_hook_*`). Complexity flag: the CST→AST pass must resolve
every node to `CType`/`Sym`; the reuse path avoids re-implementing typing but couples
parse to annotation.

## 12. The optimization-template engine ("-O1 waits for a pattern")

A library of **templates** = `pattern → rewrite (+ guard)`. -O1 holds codegen and runs
templates until a budget or fixpoint, then lowers.

| Decision | Options | Choice |
|---|---|---|
| What a template is | one engine, three pattern *scopes*: tree-expr (peephole), CFG (jump-table, dead-block), binding-graph (inline, specialize) | uniform engine, scoped patterns |
| Pattern representation | hand-C matchers · declarative DSL · **patterns as AST fragments with metavariables** | AST-fragments; a pattern's structural hash **indexes candidate sites in the hash-cons store** |
| Find sites | bottom-up · **worklist + hash-indexed candidates** | mirror `cst_rehash_dirty`'s dirty frontier — a changed node re-hashes → re-checked against templates whose root-hash matches |
| Termination | prove confluence · **cost-monotone + budget backstop** | each template must not raise a cost metric; budget is the hard stop (same philosophy as depth-`k`) |
| Time budget (TODO §221) | per-expr · **per-function** · per-TU | per-function; run in priority order until the seconds are spent, then emit; `-O1..N` = escalating budgets |
| Provenance | learned · data-file · **compiled-in registry, data-driven-ready** | uniform interface so templates are reorderable/schedulable at runtime |
| vs. inlining | phase-ordered · **interleaved via the worklist** | a render dirties nodes → triggers templates → may enable more inlining, to fixpoint/budget |

Key reuse: **the content-addressed store that dedups inline templates also indexes
optimization patterns** — matching becomes a hash lookup.

## 13. -O1 deferral / trigger ("wait before compiling/linking")

| Decision | Options | Choice |
|---|---|---|
| Granularity | per-function, held after build | per-function |
| "Wait" boundary | compile when built · **defer to the inline-closure (the TU)** | always-inline needs callees rendered before callers optimize → -O1 is necessarily **multi-pass** (fine — -O0 is the one-pass path) |
| Compile trigger | — | **budget exhausted OR fixpoint reached OR must-emit (end of closure)** — never waits forever |
| Lowering order | source · **dependency (leaves first)** | callees optimized before callers inline them |
| Cross-TU | inline now · **within-TU only; cross-TU = LTO later** | boundary calls to other TUs stay `Call`; the inline closure = the TU |
| -O0 | — | skips all of this: parse → shared vstack primitives, real calls, greedy storage |

## 14. AST↔CST linkage & persistence

| Decision | Options | Choice |
|---|---|---|
| Provenance | none · **each AST node carries its origin CST node id** | free (AST is lowered from CST); unlocks `-g` ranges, diagnostics, hot-reload reconciliation |
| Serialization | new format · **reuse `cst_snapshot`** | the template/binding store is already snapshot-shaped |
| Hot-reload (TODO) | now · **later; store serializable from day one** | later |
| `-g` / LSP | AST only · **AST + CST provenance** | AST = intention (live ranges, types); CST = concrete spans/scopes |

## 15. Validation & correctness gates

| Decision | Options | Choice |
|---|---|---|
| Oracle | byte round-trip (CST-style) · **differential behavioral equivalence, operationalized as the existing exec-golden corpus** (§17) | AST is desugared → no byte round-trip; run the program and compare output. *No new oracle:* re-run the ~240 3-way-validated `tests/exec` goldens as an `-O1-replay` runner column (the CST-on/off pattern) |
| The invariant | — | **-O1-with-zero-templates ⇒ same observable result as -O0** (proves lowering faithful before any template lands). Brought up **one golden at a time** (§17) — the corpus is the replay driver's feature checklist |
| Template safety | trust · **per-template semantic differential test** | each rewrite proven input ≡ output |
| Harness | new · **reuse qemu-user / exec suite** | the 5-target exec harnesses already in the repo |

## 16. Phasing / prerequisites / overhead

| Horizon | Milestone | Prereq / overhead | Codegen risk |
|---|---|---|---|
| **Short** | typed-CST annotations + CST→AST lowering for exprs & straight-line code; AST-dump + differential-exec gate | annotation hooks; no templates/inlining | none (AST unread by codegen) |
| **Mid** | flat-CFG construction (control flow); add the **AST-replay driver** over the existing vstack API (§10 — *not* a refactor of `gen_op`); -O1 replays AST→vstack with zero templates (byte-identical to -O0); virtual-inline via the CST store; first templates (tree-const-fold, algebraic, dead-branch, jump-table) | the replay driver + CFG→control-emission reconstruction; defer-to-TU | gated by the zero-template invariant |
| **Long** | liveness-steered placement in the replay driver (register promotion / static localization — the §10 "Later" phase); time-budgeted engine (§221); dependency-ordered -O1 compile; cross-TU LTO; hot-reload snapshots; `-g` from provenance; SSA (`-O2+`) | LTO plumbing; snapshot reconciliation | mature |

## 17. Replay driver — golden-TDD bring-up (First phase)

The replay driver (§10) is built **test-first against the goldens, not against a
byte-identity spec.** `tests/exec` is ~240 `mcc_golden_t` rows (`src` + `expect`
stdout, plus a `flags` column), already 3-way-validated (gcc/clang/tcc) and run
cross-target under qemu. The gate is a second **`-O1-replay` runner column** over the
*same* table asserting the *same* `expect` — mechanically the CST-on/CST-off pattern.
Green = faithful re-emission.

**Why golden-behavioral, not byte-identical:** byte-identity is over-strict — it would
flag benign block-layout / branch-encoding differences the moment the first template
runs, so it must be abandoned at Mid anyway. The golden gate **survives templates**
(a rewrite that changes bytes but not output stays green). Its only cost — it catches
just the divergence the inputs exercise — is small given a large, cross-validated,
cross-target corpus, and byte-identity's extra strictness was mostly illusory. Byte
match is kept only as an *optional debug tripwire* on the straight-line subset where it
naturally holds; never a gate.

**The corpus is the curriculum.** Bring the driver up one failing golden at a time; each
green case is one feature checked off. Natural order by what each category forces:

| Order | Category (count) | Forces into the replay driver |
|---|---|---|
| 1 | `expressions` (14) | `Literal`, `Binary`, `Convert` — tree-expr replay |
| 2 | `types` (30) | usual-arithmetic `Convert`, width/sign |
| 3 | `pointers_arrays` (15) · `structs_unions` (19) | `Ref`/`Load`/`Store`, pointer-`Binary` offsets, `InitList` |
| 4 | `statements` (25) | **the CFG milestone (D-b)** — `If`/`Jump`/`Return`, loops, `switch` cascade |
| 5 | `functions_abi` (14) | `Invoke`, `Return`, ABI boundary `Call` |
| 6 | `features_c99_c11` (44) · `programs` (12) · remainder | the long tail |

**This reshapes §17's sub-decisions:**

- **D-b (control emission)** — *implemented on demand.* Not designed up front: the
  first `statements/` golden that fails specifies exactly the `If`/`Jump` shape needed.
  Direct CFG emission (each BB a label; `If`→`gtst`+`gjmp`; `Jump`→`gjmp`; backpatch by
  BB id) — no loop-recovery pass, consistent with the flat-CFG model.
- **D-c (AST storage)** — *minimal upfront, grows only when a test forces it.* Plain
  per-function arena (build → replay → free); no hash-consing until virtual-inline (Mid)
  needs it. TDD prevents over-building.
- **D-d (first template)** — *sequenced after* the whole corpus is green under
  zero-template replay (templates are only safe once re-emission is trusted). Then
  **const-fold first** (tree-scope, trivial input≡output test), each subsequent template
  landing with the corpus re-run + its own §15 per-template differential test.

---

## Open decisions & questions (DECIDE)

### Decided (2026-07-07 design pass)

- **Construction (§11)** — *eager* type capture (`cst_hook_type` off the existing
  vstack path; a no-op pointer store at `-O0`, so `-O0` pays nothing) + *lazy* AST
  lowering (only when `-O1` asks). Splits "when types are captured" (eager, at the
  parse moment that already holds them) from "when the AST is built" (lazy) — removes
  the coupling objection. No standalone typing pass.
- **Emit-core is not a refactor (§10/§16)** — the vstack API (`vpush*`/`gen_op`/`gv`/
  `gsym`/`gjmp`) is already the parser-decoupled seam (`gen_op` never touches `tok`/
  parser state — verified). `-O1` = a second **AST-replay driver** over that same API.
  Structural rewrites come free; liveness-steered placement is an additive "Later"
  driver upgrade, not surgery on `gen_op`.
- **Replay gate is golden-TDD, not byte-identity (§17)** — gate on the existing ~240
  `tests/exec` goldens re-run as an `-O1-replay` runner column (same `expect`), brought
  up one test at a time. Byte-identity dropped (over-strict; dies at Mid when templates
  change bytes) — kept only as an optional debug tripwire on the straight-line subset.
- **D-b/D-c/D-d (§17)** — control emission implemented on-demand (direct CFG emit,
  BB=label); AST storage a minimal per-fn arena (hash-cons deferred to Mid); first
  template = const-fold, sequenced *after* the corpus is green under zero templates.

### Open decisions

- **`Bind` marker**: fully dissolved into liveness (15 kinds) — confirmed. Revisit
  only if `-g` scope quality needs a positioned marker beyond the CST.
- **`k` value** & generalization strategy: depth-`k` unroll then widen, vs widen at
  first back-edge. Default `k=1–2`. *(Mid — inlining.)*
- **Outline threshold**: strict always-inline (accept bloat, outline only to break
  recursion) vs size-gated outline of hot-but-huge templates. *(Mid — inlining.)*
- **PP-as-executable-C (JIT)**: parked; promoted (not necessitated) by the
  include-permutation analysis.
- **Store factoring**: generalize `cst_store`/`binding`/`render` into a
  structure-agnostic engine shared by CST (bytes) and AST (CFG nodes). Lean: don't
  factor speculatively — pull it out when virtual-inline becomes the second real user.
  *(Mid.)*
- **Template representation (§12)**: patterns-as-AST-fragments + hash-index (rec) vs
  a declarative DSL; when/whether to make the registry data-driven. *(Deferred until
  the corpus is green under zero-template replay — the next real design conversation.)*
- **Defer-to-TU (§13)**: accept that `-O1` is multi-pass (departs from streaming) — the
  price of whole-TU always-inline; revisit if per-function compile is wanted.

### Open questions / ambiguities (unresolved, surfaced but not yet decided)

- **`-O1` payoff source (sequencing fork)**: does `-O1`'s value come mostly from
  structural rewrites (then the §10 "Later" liveness-steered placement can stay Long) or
  are register-allocation gains wanted early (then placement-steering moves up in
  priority)? Undecided — changes only *sequencing*, not architecture.
- **CFG → control-emission reconstruction (§16 Mid risk)**: how hard is reconstructing
  the parser's `gsym`/`gjmp` backpatching from a flat CFG in the replay driver? Believed
  tractable via direct BB-labelled emission (§17 D-b), but the exact `switch`/loop
  back-edge and label-fixup mechanics are unproven until the `statements/` goldens are
  attempted. The one concrete "known unknown" of the First phase.
- **Byte-tripwire scope (§17)**: byte-identity is dropped as a *gate* but kept as an
  optional debug aid "on the straight-line subset where it naturally holds" — the exact
  boundary of that subset (which nodes still emit byte-identically vs. legitimately
  diverge) is unspecified until replay is built.
- **`cst_hook_type` annotation surface (§11)**: which CST nodes must carry `CType`/`Sym`
  for lowering to resolve every node, and whether the existing `cst_hook_*` set covers
  them or new hook points are needed — a coupling-cost unknown until the CST→AST pass is
  attempted.
