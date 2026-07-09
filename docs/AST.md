# AST — an intention IR alongside the CST

> **Implementation status:** see **[docs/AST-STATUS.md](AST-STATUS.md)** for the current
> tier-by-tier state (Tier 1/2 `-O0` parity ✅, Tier 3 register promotion ✅, Tier 4
> virtual-inline 🟡), env flags, gates, and remaining work. This file is the *design*.

Status: **first phase complete** (§16 Short + §17 replay-driver bring-up). The
intention-IR library (`src/mccast.{c,h}`), the vstack-replay driver (in
`src/mccgen.c`, gated by `MCC_AST_REPLAY`), the golden-TDD differential gates
(`tests/ast/`, `exec-replay/*`), and the first optimization template —
**const-fold** (`MCC_AST_TEMPLATES`, `exec-replay-tmpl/*`) — are all in and green.
The completion record (A1–A7) is in docs/NOTES.md ("Completed work — AST
intention-IR"). **§16 Mid coverage-widening is substantially complete (2026-07-08,
19 milestones — see docs/NOTES.md "AST replay coverage widening" + docs/TODO.md).**
The replay driver now covers essentially all of C: floats/double, call-result
stores, struct member access (`.`/`->`) + copy/deref + `f().x` + bit-fields,
`switch`, named `goto`/labels, **all struct-return ABI forms** (register / sret
hidden-pointer / arch-transfer / variadic) and by-value struct args, the full
`_Complex` surface (arithmetic, `__real__`/`__imag__`, casts, imaginary literals),
and short-circuit results used as values. Two latent correctness bugs were fixed
on the way (call double-emit; float const-pool duplication) and a switch-replay
segfault guarded. **All three of the once-"Remaining" Tier-2 query gaps are now
CLOSED (2026-07-08), completing `-O0` replay parity for the §18.5 checklist:**
nested short-circuit operands (`(a&&b)||c` — the inner Binary is already a captured
node; the outer chain just accepts it and replay's `AST_Binary` recurses — no query
needed, and the "grep segfault" was an unrelated latent NULL-`sym` bug in bare-global
`if`/loop conditions, now fixed); the `__builtin_complex`-based `I` unit (`r + i*I` —
`ast_hook_builtin_complex_begin/end` capture the rodata-const result as a Ref leaf, and
the float→double `_Complex` widening cast reuses its rodata symbol ordinally via a
generalized `ast_fconst_reuse/_record/_push_ref` trio); and VLA/`alloca` (the coarse
`Unary(AST_OP_VLA)` alloc effect + the LIFO SP-restore lexical-scope-edge query as a
Return annotation / `AST_OP_VLA_RESTORE` BB effect — fixing another latent bug, an
unreset `ast_last_return`). **The founding reframe (§18) held exactly: the vstack/ABI
backend is feature-complete C11 — the exec suite proves it — so the replay driver never
faked/reimplemented anything; every gap was a driver-side query/capture over ops that
already exist.** The remainder below (mid/long horizons: liveness-steered register
promotion [the next real `-O1` win, Tier 3], virtual always-inline [Tier 4], more
templates, LTO, `-g`, hot-reload) is **plan / design** (curate freely).

### Key mechanisms landed in the Mid coverage widening (2026-07-08)

- **Ordinal frame-slot reuse** (`ast_alloc_loc`/`ast_locrec`, the `ast_fconst`
  pattern): a codegen path that allocates an anonymous frame slot with a direct
  `loc = (loc-size)&-align` (struct-return result temps, sret hidden-pointer temp,
  the `_Complex` result temp `cplx_local`) would land at a *different* offset on
  replay (source locals are fixed in their Syms, not re-allocated). The parse-build
  records each slot offset in emission order; replay reuses them ordinally. `-O0`
  and the parse-build stay byte-identical (the record is passive; reuse fires only
  under `ast_replaying`). This was the enabler for all struct-return callers and
  `_Complex` — and its absence caused a real frame-layout regression (caught by the
  exec suite, *not* byte-verify, since the epilog/temp reservation lives outside the
  verified body).
- **Suppress-and-fold**: an operation whose internal ops fire capture hooks and
  desync the mirror is bracketed with `ast_in_op`/`ast_in_call` suppression and
  captured as one coarse node whose replay reproduces the exact sequence. Used for
  member access (`Unary(AST_OP_MEMBER)`), the `VT_CMP`→0/1 materialization in
  `vcheck_cmp` (short-circuit-as-value), `gen_complex_cast`, and imaginary literals
  (`Unary(AST_OP_IMAG)`).
- **Struct-return caller reconstruction**: the Invoke replay recomputes the return
  descriptor from the result type (`gfunc_sret` + `PUT_R_RET`) and reproduces the
  parser's post-call handling — register→temp reconstruction (`ret_nregs>0`),
  re-push of the sret result temp (`==0`), or `+ arch_transfer_ret_regs` (`<0`). Companion to the CST subsystem
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
operators remain as tree nodes. Each hoisted temp is an **anonymous per-temp `Sym`,
register-preferred** (address never taken → the allocator keeps it in a register, liveness
scoped): the easy-to-lower form that is also the fast one, no shared-slot bookkeeping (§C2). `!x` → `Binary(==, x, 0)`; all 0/1-producing ops are
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
| Construction path | **parallel parse-emission via vstack hooks** · lower from the CST | **Ratified from the shipped first phase** (§17): `ast_hook_*` fire from the same vstack positions as the CST hooks and capture typed values directly, with **zero CST dependency**. Overlapping hook *sites* compile under `CONFIG_CST \|\| CONFIG_AST`; each subsystem functions entirely without the other (A4). Lowering-from-CST is *rejected* — it would couple AST to CST, violating the independence rule. |
| AST↔CST independence (A4) | shared impl · **two independent impls, one storage when both on** | AST and CST are **separate implementations**: `CONFIG_AST` builds and replays with `CONFIG_CST` **off**, and vice-versa. When *both* are enabled they share one storage/hook-firing surface (gated by the `CONFIG_CST \|\| CONFIG_AST` OR) to avoid duplicating captured provenance — but neither ever *depends* on the other. |
| Typing | re-derive · **reuse parse-time types** | the `ast_hook_*` sites snapshot the `CType`/`Sym` the vstack already computed at that parse moment (`ast_finalize_leaf`); re-deriving would duplicate the semantic engine |
| Build timing | eager side-channel · **lazy, only when -O1 asks** | the AST's only consumer is -O1 codegen; building at -O0 is pure overhead (this *refines* the intro's "side-channel" wording) |
| -O0 cost | always build · **-O1-gated (`ast_active`)** | -O0 pays nothing — the hooks early-return on `!ast_active` |

**Net: parser-hook → AST (CST-independent), built lazily at -O1.** Types are captured
inline at the ~11 vstack sites that already hold a resolved `vtop` (no standalone typing
pass, no CST→AST lowering). The one residual is confirming each capture site sees a valid
`vtop` at finalize — structurally guaranteed (types resolve bottom-up on the vstack exactly
as expressions complete), retired the first time a full function captures.

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
| Acceptance bar (§C1) | exec goldens only · **exec goldens + full GCC test suite, both `-O0` and `-O1`** | the replay driver is "done" when every `tests/exec` golden is green under the `-O1-replay` column **and** GCC's own test suite passes under both `mcc -O0` and `mcc -O1`. `-O2`/`-O3` get their own separate replay drivers later (§16 Long) |
| Harness | new · **reuse qemu-user / exec suite** | the 5-target exec harnesses already in the repo |

## 16. Phasing / prerequisites / overhead

| Horizon | Milestone | Prereq / overhead | Codegen risk |
|---|---|---|---|
| **Short** | typed-CST annotations + CST→AST lowering for exprs & straight-line code; AST-dump + differential-exec gate | annotation hooks; no templates/inlining | none (AST unread by codegen) |
| **Mid** | flat-CFG construction (control flow); the **AST-replay driver** over the existing vstack API (§10 — *not* a refactor of `gen_op`); -O1 replays AST→vstack with zero templates (byte-identical to -O0); **then liveness-steered register promotion** (mem2reg of address-not-taken locals — the real -O1 payoff per §A1, promoted from Long); const-fold template (proves the template mechanism); virtual-inline via the shared store | the replay driver + CFG→control-emission reconstruction; defer-to-TU | gated by the zero-template invariant |
| **Long** | the broader template library (algebraic, dead-branch, jump-table — demoted from Mid per §A1: rewrites are the *smaller* payoff); time-budgeted engine (§221); dependency-ordered -O1 compile; cross-TU LTO; hot-reload snapshots; `-g` from provenance; **separate `-O2`/`-O3` replay drivers (SSA)** | LTO plumbing; snapshot reconciliation | mature |

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

- **D-b (control emission)** — *implemented on demand.* Two mechanisms, by structure:
  - **Structured constructs (shipped):** `if`/`else`/`while`/`do`/`for`/`for(;;)`/ternary
    replay by **reproducing the parser's exact `gind`/`gvtst`/`gjmp`/`gsym` pattern** per
    construct (`ast_replay_bb`), so no backpatch table is needed — the byte-verify net
    confirms fidelity. `break`/`continue` chain onto the loop's `bsym`/`csym` exactly as
    the parser does.
  - **Unstructured remainder (`goto`/labels, `switch`):** the flat-CFG model — **RPO block
    layout + `int bb_addr[nbb]` + per-BB pending-jump fixups**. `Jump` to the next block in
    layout is fall-through; else `gjmp` recorded against the target BB. `If`→`gvtst` (thread
    its returned jump-list into the fixup list) + `gjmp` the other edge. Back-edges target
    already-emitted blocks → `gjmp_addr(bb_addr[i])` directly. **No loop-recovery pass** —
    the flat CFG already carries every edge. `switch` reconstructs the parser's sorted
    `gcase` binary-search tree emitted *after* the fall-through body. This is the one
    concrete "known unknown" of the phase; retired by the first `statements/` golden that
    confirms `gvtst`'s jump-list threads cleanly into the fixup list.

  **Byte-tripwire scope (§A3):** byte-identity holds *iff* replay issues the same `gen_op`
  order as the parser — precisely **single-BB functions, zero templates, zero placement
  steering** (predicate `nbb==1`). Auto-enabled there, auto-disabled otherwise. Any control
  flow may reorder block layout (benign, but not byte-equal), so it is a debug aid, never a
  gate.
- **D-c (AST storage)** — *minimal upfront, grows only when a test forces it.* Plain
  per-function arena (build → replay → free); no hash-consing until virtual-inline (Mid)
  needs it. TDD prevents over-building.
- **D-d (first template)** — *sequenced after* the whole corpus is green under
  zero-template replay (templates are only safe once re-emission is trusted). Then
  **const-fold first** (tree-scope, trivial input≡output test), each subsequent template
  landing with the corpus re-run + its own §15 per-template differential test.

---

## 18. The AST query surface — the only real remaining problem

**Founding reframe (2026-07-08).** The backend — the vstack API (`vpush*`/`gen_op`/
`gv`/`gsym`/`gjmp`) plus every piece of ABI machinery below it (indirect
`CallTransfer`, sret, varargs marshalling, VLA SP moves, soft-float, `_Complex`) — is
**feature-complete C11, and the exec suite is the proof.** The recursive-descent
parser is not "the `-O0` implementation of C11"; it is merely **one driver that
demonstrates the opcodes already span the language.** There is nothing in C11 the
opcodes can't already express, because `-O0` already emits all of it correctly across
thousands of passing tests.

Therefore the replay driver **never fakes, reimplements, or invents machinery.** It
*calls the same ops*. Reaching feature-parity with `-O0` = **"use the tools it already
has."** Every downstream feature question (recursion, varargs, function pointers,
`setjmp`/`longjmp`, VLA) is already answered by the backend; the driver only needs to
know *when* and *with what* to call each op. **The entire difficulty is the AST query
surface**: can the driver — holding the whole AST — answer, easily, the questions the
streaming parser answered implicitly from its live context?

### 18.1 Dissolution — the "new machinery" mirage

Anything that *looked* like it needed a new abstract machine dissolves into "the op
exists; the driver needs one query." The naïve one-`main`-with-a-faux-stack flattening
is an *existence proof that inline-everything is sound*, **not** an implementation
strategy — physically flattening would force `switch`-dispatch even where a call is
statically a `Jump`, re-introducing the very overhead inlining removes.

| Looked like "reimplement…" | The op already exists (proven by `-O0`) | The *only* new thing is the query |
|---|---|---|
| non-tail/tree recursion via a faux-stack | a non-inlined recursive call **is** the existing internal/boundary `Call` on the **real** machine stack | *"inline this instance, or emit the `Call`?"* (inlinability + hash-termination) |
| `va_start`/`va_arg` on a faux-stack | `-O0` already marshals varargs — **replay already ships variadic struct-return calls** | *(none special — the descriptor is recomputed from the result type)* |
| function-pointer `switch`-dispatch / computed goto | the existing **indirect `CallTransfer`**; identity is real because escaped fns are materialized standalones (§9) | *"is this `Invoke` indirect?"* — an O(1) node field |
| `setjmp`/`longjmp` against a faux-stack | runtime calls + a real frame; `-O0` compiles them | *"does this region contain `setjmp` → forbid inlining across it"* — a **guard** query (§18.4) |
| VLA/`alloca` SP reimplementation | `-O0` already emits the SP moves (`StackAlloc`/`StackSave`/`StackRestore` exist) | *"where are the lexical scope edges, for LIFO save/restore placement?"* — a **scope** query |

Restated: **every C11 hazard that seemed to need new machine machinery is actually an
inlinability/placement *guard query* over ops that already exist.** The reframe doesn't
lose the C11 subtlety — it *relocates* it from the machine tier (hard) to the query
tier (a predicate on the AST). "Minimize-invoke" needs no floor theorem: parity =
reproduce the parser's op selection; boundary calls stay `Call` exactly as `-O0` does
(that's just linkage). `-O1` then beats `-O0` in exactly **two** ways, both purely
because the driver holds the whole AST as a *queryable object* the streaming parser
never had: **(1)** inline an internal call instead of `Call` (Tier 4 query); **(2)**
steer placement / promote address-not-taken locals to registers (Tier 3 query).

### 18.2 The surface, tiered by query cost

The right axis is **cost to answer**, because "answer the questions *easily*" is the
whole game. Tier 1 is free; Tier 4 is where the engineering lives.

**Tier 1 — O(1) node-field reads** (free; mostly shipped)

| Driver decision | Query | Answer lives in | Parity / beyond | Status |
|---|---|---|---|---|
| operand width/sign for `Binary`/`Convert`/`Load`/`Store` | access `CType` | node field (§11 capture) | parity | ✅ |
| no-op reinterpret vs real `Convert` | `is_noop(src,dst)` | predicate on node | parity | ✅ |
| literal encoding (int/float/`_Complex`) | `Literal` | node field | parity | ✅ (float-pool bug fixed) |
| address/loc of a named object | `Ref → Sym.loc` | Sym | parity | ✅ |
| anon temp slot (sret ptr, struct-ret temp, `cplx_local`) | ordinal loc record | `ast_locrec` | parity | ✅ (the Mid enabler) |
| sret / return-descriptor form | recompute from result type | `gfunc_sret`+`PUT_R_RET` | parity | ✅ |
| direct vs indirect call | is the `Invoke` target a value? | node field | parity | ✅ |
| must-be-memory (`&`-taken/`volatile`/`register`) | constraint flag | Sym/type (§4) | parity **+ enables promotion** | ✅ |

**Tier 2 — local structural walk** (cheap; on-demand — **all three open items live here**)

| Driver decision | Query | Mechanism (shipped) | Status |
|---|---|---|---|
| structured branch target (`if`/`while`/`do`/`for`/ternary) | reproduce parser `gind`/`gvtst`/`gjmp`/`gsym` | per-construct pattern in `ast_replay_bb` | ✅ |
| `goto`/label target + back-edge | per-fn label table (token → `{jind,jnext}`) | `ast_hook_label`/`ast_hook_goto` | ✅ |
| switch cascade + default | rebuild `switch_t`, `case_sort`+`gcase` | `ast_hook_switch_*` | ✅ |
| *(planned generalization)* flat-CFG target | `bb_addr[BB]` + RPO layout + fixup | §17 D-b — not yet built | ⬜ |
| **VLA save/restore placement** | **lexical scope edges (LIFO nesting)** | CST/AST scope walk | 🔧 **open — pure query gap** |
| **rodata const-symbol reuse (`I`-unit)** | **look up existing rodata symbol** | const-pool table | 🔧 **open — pure query gap** |
| **nested short-circuit** | **nested landor-chain shape** | replace the flat model | 🔧 **open — model gap** |

**Tier 3 — whole-function analysis** (the register-promotion payoff — beyond `-O0`, early Mid)

| Driver decision | Query | Analysis |
|---|---|---|
| promote local to register | live range (birth = first def, death = last use) | backward liveness |
| share one spill slot across disjoint lifetimes | non-overlapping live ranges | liveness interference |
| keep-in-reg across a span / spill weight | access freq × loop depth | liveness + loop nesting |

**Tier 4 — whole-program / fixpoint** (the inline payoff + its correctness guards — Mid)

| Driver decision | Query | Analysis |
|---|---|---|
| inline vs emit `Call` | callee internal & inlinable? | linkage (O(1)) + **address-escape (whole-program)** + size |
| does this inline instance terminate? | instance-hash ∈ ancestor stack | §9 structural hash |
| per-site specialization | constant-arg branch select | §9 binding state |

### 18.3 Every open item was a Tier-2 query gap, not a codegen gap — all now CLOSED (2026-07-08)

The three "Remaining" replay items were **op-exists / query-missing**, not feature gaps —
the backend already compiled all three at `-O0`. All three are now landed, exactly as the
reframe predicted (no new machine op):

- **VLA/`alloca`** ✅ — the SP-move ops exist; the driver added the **lexical-scope-edge
  query**: a coarse `Unary(AST_OP_VLA)` alloc effect (captured immediates, no `loc`
  decrement at replay) plus the paired SP restore emitted at the scope edge — a `Return`
  annotation when it fires in a `return`'s `leave_scope`, else an `AST_OP_VLA_RESTORE` BB
  effect at a nested block's `}`. Fixture `replay-vla`.
- **`__builtin_complex` `I`-unit** ✅ — `ast_hook_builtin_complex_begin/end` capture the
  rodata-const result as a Ref leaf (the anon Sym persists), and the float→double
  `_Complex` widening cast reuses its rodata symbol ordinally via the generalized
  `ast_fconst_reuse/_record/_push_ref` trio. Fixture `replay-complex_ctor`.
- **nested short-circuit** (`(a&&b)||c`) ✅ — needed **no** new "landor-chain query": the
  inner `Binary(&&/||)` is already a captured child, so `ast_hook_landor_operand` just
  accepts it and replay's `AST_Binary` case recurses. The "grep segfault" was an *unrelated*
  latent NULL-`sym` bug in bare-global `if`/loop conditions (the eager push-hook captures
  a global leaf before `vpushsym` sets `->sym`; the condition site never re-finalized it),
  now fixed by finalizing the condition leaf in the four condition hooks. Fixture
  `replay-short_circuit`.

### 18.4 Guard queries — the Tier-4 correctness backstops

Inlining is what turns C11 hazards into query obligations. These predicates keep
inline-heavy *sound*, not merely fast — each is a flag propagated up the binding graph:

| Guard query | Hazard it prevents | Effect on the driver |
|---|---|---|
| region contains `setjmp` / installs a signal handler? | inlining would change the frame `longjmp` / the handler refers to | mark the region **non-inlinable across** |
| body allocates a VLA? | inlining into a loop changes the SP-unwind scope | inhibit inline, or emit a fresh scope pair |
| callee address-taken / external linkage? | identity + ABI must be observable | keep a materialized standalone (§9); boundary `Call` |

The reframe therefore *relocates* the classic C11 flatten hazards from "machinery we'd
have to build" to "predicates the driver evaluates" — and the machine ops they guard are
already proven correct by the exec suite.

### 18.5 Implementation checklist — query → concrete mechanism

The actionable artifact: every query from §18.2–18.4 mapped to the **exact symbol in
`src/mccgen.c` that answers it today**, or the specific TODO if it's a gap. Legend:
✅ shipped · 🔧 open (answer-source exists, not yet driven) · ⬜ not built (beyond `-O0`,
Mid). This is the single source of truth for "what's left."

**Tier 1 — O(1) node-field reads (all ✅)**

| Query | Answered by | Fixture |
|---|---|---|
| operand type/width/sign | `ast_finalize_leaf()` snapshots the `SValue` `CType`/`Sym` onto the leaf; replay reads it | (all) |
| no-op vs real `Convert` | `ast_hook_convert` + node `op` | `replay-float_ops` |
| fp/complex literal encoding | `ast_fconst[]` const-pool symbols, ordinal reuse via `ast_fconst_i` under `ast_replaying` | `replay-float_ops` |
| address of a named object | `ast_sym(a,n)` → reconstructed into `sv.sym` in `ast_replay_value()` | (all) |
| anon temp slot (sret/struct-ret/`cplx_local`) | `ast_alloc_loc()` + `ast_locrec[]`, ordinal cursor `ast_locrec_i` | `replay-struct_ret_*` |
| sret / return-descriptor form | recomputed from result type (`gfunc_sret`+`PUT_R_RET`) in the Invoke replay | `replay-struct_ret_sret` |
| direct vs indirect call | `ast_hook_call_begin`/`_end` node | `replay-call_store` |
| must-be-memory (`&`/`volatile`/`register`) | `VT_*` flags on the captured type; bail gate `ast_bad_type()` | — |

**Tier 2 — local structural walk (all ✅ as of 2026-07-08 — `-O0` parity complete)**

| Query | Answered by | Fixture |
|---|---|---|
| structured branch target | `ast_replay_bb()` reproduces the parser's `gind`/`gvtst`/`gjmp`/`gsym` per construct | `replay-*` (loops/if) |
| `goto`/label target + back-edge | per-fn label table (token→`{jind,jnext}`), `ast_hook_label`/`ast_hook_goto` | `replay-goto_dispatch` |
| switch dispatch | `ast_hook_switch_*` → rebuild `switch_t`, `case_sort`+`gcase` | `replay-switch_dispatch` |
| ✅ **VLA scope edges** | coarse `Unary(AST_OP_VLA)` alloc effect (`ast_hook_vla_alloc_begin/end`) + LIFO SP restore as a `Return` annotation / `AST_OP_VLA_RESTORE` BB effect (`ast_hook_vla_restore`) — captured immediates, no `loc` decrement at replay | `replay-vla` |
| ✅ **rodata const-symbol reuse (`I`-unit)** | `ast_hook_builtin_complex_begin/end` capture the rodata-const result as a Ref leaf; the widening cast reuses its symbol ordinally via `ast_fconst_reuse/_record/_push_ref` | `replay-complex_ctor` |
| ✅ **nested landor** | no new node needed — the inner `Binary(&&/||)` is already a captured child; `ast_hook_landor_operand` accepts it and replay's `AST_Binary` recurses. (The "grep segfault" was an unrelated NULL-`sym` bug in bare-global conditions, fixed.) | `replay-short_circuit` |

**Tier 3 — whole-function liveness (beyond `-O0` — the register-promotion payoff; v1 ✅ 2026-07-08)**

| Query | Status |
|---|---|
| promote local to register | ✅ **landed incl. control flow, calls, pointers, and floats** (opt-in `MCC_AST_PROMOTE`, x86_64): an address-not-taken **integer, pointer, or float/double** local is pinned to a register (GP for int/ptr; **XMM6/7** for float/double, call-free only since XMM are caller-saved) with zero stack traffic — reads push it register-resident (gv copies on use), writes force it into the pin (converting to the local's type, so a widening store sign-extends), and the pin is **seeded from the local's slot at entry** so it is valid across loops/if and for params. A promoted pointer's value lives in the register; `*p`/`p[i]` is an `AST_Load` that indirs it into a memory base (needed a backend fix: SIB/REX.B addressing for an r12–r15 base in `load`/`store`, an unexercised encoding gap since the allocator never bases off those). A **call-free** function uses caller-saved **R10/R9/R8** (R10 is used nowhere in the backend, R8/R9 only for call args; R11 excluded — it backs `load`/GOTPCREL); a **call-ful** function uses callee-saved **RBX/R12–R15** (pushed at entry with an even-count alignment pad, popped at the single return funnel) so the pin survives calls. Poisons: floats/structs, `&`/member-base (incl. `p->m`) and whole local array/struct slot ranges, `++`/`--`, inline asm; VLA+call-ful bails (rsp race). Byte-verify bypassed (bytes diverge from `-O0`) so the two-pass faithfulness gate + whole-corpus **`exec-replay-promote`** column + `ast/replay-promote` fixture (asserts a call-free, a call-ful, and a pointer fn promote) are the gate. The first opt that beats `-O0`. `MCC_AST_NO_CALLFUL` restricts to the call-free pool. |
| ⬜ broaden | share one spill slot across disjoint live ranges; a real backward-liveness pass for finer live-range reuse; other arches (the GP/XMM pin pools are x86_64-specific). |

**Tier 4 — whole-program / fixpoint (⬜ beyond `-O0`, Mid — the inline payoff + guards)**

| Query | TODO |
|---|---|
| inline vs `Call` | inlinability query: linkage (O(1)) + address-escape (whole-program) + size |
| inline instance terminates? | instance-hash ∈ ancestor stack (§9 structural hash) |
| per-site specialization | constant-arg branch select (§9 binding state) |
| guard: `setjmp`/signal/VLA region | propagate a **non-inlinable-across** flag up the binding graph (§18.4) |

**Net remaining work:** the three Tier-2 hooks are all ✅ (2026-07-08) — **`-O0` replay
parity is complete** for the checklist — and **Tier 3 register promotion is landed**
(the first opt that beats `-O0`: address-not-taken integer locals kept in pinned registers
across control flow AND calls — call-free via caller-saved R10/R9/R8, call-ful via callee-saved
RBX/R12–R15 — gated by the `exec-replay-promote` corpus column). What remains is **breadth, still beyond
`-O0`**: a real backward-liveness/def-use pass for finer live-range reuse (share a slot across
disjoint ranges, spill-weight by loop depth, promote floats via an XMM pool), and Tier 4
(inline + guards, the "minimize-invoke" payoff). Nothing here is a new machine op — every
remaining ⬜ is a query or a driver-steering step over ops the exec suite already proves.

---

## 19. Completing Tier-4 — struct-by-value params & per-site specialization (A2 design pass)

**Design pass, 2026-07-08 (pre-implementation).** Tier 4 (§9) is broadly functional
(AST-STATUS §5): a `static`, non-variadic, VLA-free callee with control flow, multiple
returns, scalar/pointer/float params, and its own calls grafts into a caller, forward
callers included (defer-to-TU). The two named remainders are **struct-by-value params**
and **per-site specialization**. This section designs both before any code lands.
Roadmap ratified with the user: **A2 (this section) → A4 (GCC-suite acceptance gate) →
A1 (backward-liveness / spill-slot sharing)** — finish the 90%-built inline breadth,
then lock correctness against an external suite, then invest in the liveness pass that
compounds under everything.

### 19.1 Diagnosis — why the plain `vstore` bind miscompiles

Grafting today binds each arg with a uniform frame bias (`ast_inline_graft`):
`ast_replay_value(arg)` → `vstore` into `param_off[i] + ast_inline_bias`, and every
`VT_LOCAL` reference in the replayed body is shifted by the same `ast_inline_bias`
(`ast_replay_value`, `sv.c.i += ast_inline_bias`). This is correct **iff every captured
param offset is a negative rbp-relative local slot** — which holds for the scalar params
grafting currently accepts, because `gfunc_prolog` spills register-passed scalars to
fresh negative `loc` slots.

It breaks the moment a param's storage is **not** a negative local. `gfunc_prolog`'s
`param_addr` (captured verbatim as `ast_inline_cap_off[n] = ls->c`) has two disjoint
meanings, chosen by `classify_x86_64_arg`:

| Param storage class | `param_addr` sign | Reproduced by uniform bias? |
|---|---|---|
| register-class (int/sse, fits remaining regs) — spilled to a fresh local | **negative** (`loc -= …`) | **yes** — inside `[loc-frame_size, loc]`, shifts cleanly |
| memory-class (>16B struct, x87, or the 7th+ overflow arg) — read in place from the incoming-args area | **positive** (`addr`, from `PTR_SIZE*2` up) | **no** — a positive offset + a negative bias lands in the *caller's own locals*, and it was never reserved by `loc -= frame_size` |

So the failure is **not struct-specific** — it is **stack-passed-param-specific**. A
`transparent_union` / >16-byte struct is simply the *common* way to hit a positive
`param_addr`; a function with a 7th+ scalar argument hits the identical latent hazard
(currently unexercised because graftable callees are small). The register-class ≤16-byte
struct case the past experiment saw "work" is exactly the sub-case whose `param_addr`
was already negative.

| Failure mode | Root cause | Fix owned by |
|---|---|---|
| >16-byte struct param corrupts caller locals | positive `param_addr` biased as if negative | §19.2 per-param remap |
| ≤16-byte per-member (`transparent_union`) struct wrong | plain `vstore` copies bytes but arg type ≠ slot type (implicit-arg-conversion the ABI would have done) | §19.2 materialize-then-copy |
| 7th+ scalar arg (latent) | same positive-offset hazard | §19.2 (same fix, for free) |

### 19.2 The bind — materialize into a fresh local, remap per-param

The unifying move: **inlining dissolves the ABI, so no param should ever be read from
the incoming-args area.** Give *every* param a fresh negative **materialization slot** in
the caller's frame, copy the (type-correct) argument value into it, and map the callee's
references to that slot — replacing the single uniform bias with a per-param offset map
for the param region (locals keep the uniform bias).

| Decision | Options | Choice |
|---|---|---|
| Offset relocation | uniform `ast_inline_bias` (today) · **per-param remap over a uniform local bias** | per-param remap: memory-class (positive) params get a fresh negative slot; register-class params keep the bias path; a small `param_remap[]` consulted in `ast_replay_value` before the uniform shift |
| Struct copy | element-wise · **`vstore` block-copy of a VT_STRUCT lvalue** | `vstore` already lowers a struct assignment to the backend's block move (memcpy/inline copy); the arg is materialized as an addressable lvalue first |
| Arg-type correctness (`transparent_union`, promotions) | trust the recorded slot type · **cast the arg to the param's captured `CType` before copy** | reuse the captured `param_typ`/`param_ref`; a `gen_assign_cast`-equivalent makes the union/implicit-conversion match what the boundary `Call` would have produced |
| Class query | hard-code x86_64 · **`classify_x86_64_arg` at capture, recorded as a class bit** | arch-agnostic: capture records `is_stack_passed` per param (from the same classify the prolog used); the graft branches on the bit, not on the arch |
| Frame reservation | reserve locals only (today) · **reserve locals + N materialization slots** | `loc -= frame_size` still covers register-class params; the graft additionally allocates one aligned slot per memory-class param and records it in `param_remap[]` |
| Capture gate | exclude structs (today) · **admit struct/union scalable params; keep excluding pointer-to-VLA** | `ast_inline_capture` drops its `bt != VT_STRUCT` early-return for by-value struct/union; pointer-to-VLA and unnamed params stay excluded (unchanged) |

Net mechanism (all in `ast_inline_graft` / `ast_inline_capture` / the `ast_replay_value`
`VT_LOCAL` relocation — **no backend change**, the block move and classify already exist
and are exec-proven):

1. **Capture** (`ast_inline_capture`): for each param record `param_off`, `param_typ`,
   `param_ref`, **and** a `param_stack` bit = `classify_x86_64_arg(type,…) ==
   x86_64_mode_memory` (or overflow). Admit VT_STRUCT/VT_UNION.
2. **Bind** (`ast_inline_graft`): materialize each arg as an addressable lvalue; for a
   `param_stack` param, allocate a fresh aligned negative slot `s`, set `param_remap[i]=s`;
   else `param_remap[i] = param_off[i] + bias` (the existing path). Cast the arg to the
   captured param `CType`, then `vstore` (scalar store or struct block-copy) into
   `param_remap[i]`.
3. **Replay**: in `ast_replay_value`, a `VT_LOCAL` reference whose offset falls in the
   callee's param region resolves through `param_remap[]`; every other local keeps the
   uniform `ast_inline_bias`. (The param region is contiguous and known from capture, so
   the test is a range check, not a per-node table walk.)

This is the "ABI-aware bind" AST-STATUS §5 names, reframed: not *reproducing* the ABI but
*deleting* it — the one query it needs (`is this param stack-passed?`) is answered by the
same `classify_x86_64_arg` the prolog already calls.

### 19.3 Per-site specialization — constant-arg branch select

Second remainder: a graft is shape-identical at every call site (AST-STATUS §5); a
constant argument is stored to a slot and re-loaded, not folded into the body. §9's design
is **binding: constant-arg branch select** — per-site dead-branch elimination falling out
of the binding, for free.

| Decision | Options | Choice |
|---|---|---|
| Specialization vehicle | new specializer · **the existing template engine (`MCC_AST_TEMPLATES`, const-fold)** | reuse §12: bind a constant arg as a `Literal` value (not a slot Store+Load), so the const-fold template already folds it through the grafted body and dead-branches collapse — no new pass |
| v1 scope | full k-CFA per-site clones · **const-arg fold only (no clone), templates-gated** | a constant arg substitutes its `Literal` at the param's `Load` sites during replay; the graft stays one shape but the *values* specialize per site. Full binding-keyed clones (distinct rendered bodies per constant tuple) defer to the §9 store-factoring milestone |
| Interaction with the bind | independent · **layered: §19.2 binds non-constant args, §19.3 substitutes constant ones** | a call with mixed args stores the variable ones (§19.2) and substitutes the constant ones (§19.3); the two are orthogonal per-param choices |
| Gate | trust · **exec-golden + a fixture asserting a dead branch vanished** | a `ast/replay-inline-spec` fixture: a callee `if (flag) A else B` grafted at a `flag=1` site emits only `A` (byte-count / `[ast-inline]` dump check) |

The v1 keeps grafting single-shape (cheap) while getting the constant-prop win by routing
constants through the *already-built* const-fold template — the §9 promise ("per-site
specialization falls out for free — it's branch-select") realized without the store-factor
work, which stays deferred to its validated second user (§ store-factoring revisit).

### 19.4 Validation & sequencing

- **Gates (unchanged shape):** two-pass faithfulness (pass 1 byte-verify vs `-O0`, pass 2
  the transform under the exec-golden differential) + the whole-corpus `MCC_AST_INLINE`
  column (byte/exit-identical to `-O0` across the ~218 programs) + `ast/replay-inline`
  extended with a struct-by-value graft and `ast/replay-inline-spec` for the constant fold.
- **Then A4:** wire the GCC test suite as an AST gate under both `mcc -O0` and the
  `-O1-replay`/inline columns (§C1 acceptance bar) — the external suite is expected to
  surface exactly the stack-passed-param and per-member-classify edge cases §19.2 targets,
  which is why it sequences *after* A2 lands but *before* the deeper A1 investment.
- **Then A1:** the real backward-liveness pass (spill-slot sharing across disjoint live
  ranges, spill-weighting) — the register-allocator-grade work every later tier compounds
  on (AST-STATUS §6).

### 19.5 Open sub-choices for ratification

Three knobs are genuine forks I want confirmed before implementing §19.2/§19.3:

| Knob | Options | My lean |
|---|---|---|
| Param-region resolution in `ast_replay_value` | range-check the offset · a per-graft `param_remap[]` indexed lookup | range-check (contiguous region, O(1), no table walk) |
| Constant-arg v1 aggressiveness | fold-only, single-shape graft · full per-site binding-keyed clones now | fold-only (defers store-factoring to its validated second user) |
| Struct-return × struct-param overlap | land struct-param bind independently · co-verify with the existing struct-*return* coalesce in one fixture | co-verify (a callee taking *and* returning a struct exercises both memory paths at once) |

### 19.6 Landed (2026-07-08/09)

Both §19.2 and §19.3 shipped; all three §19.5 leans were taken as written.

- **§19.2 struct-by-value params** — the bind *deletes* the ABI: every param is materialized
  into a fresh caller-frame slot. Register-class (negative `param_addr`) keeps the uniform
  bias; memory/stack-passed (positive `param_addr`) gets a fresh slot + a range-checked
  per-param remap. Records `param_size` + a `param_stack` bit (= `param_addr >= 0`). Co-verified
  a struct-param-AND-return callee (`addpt`). One latent bug fixed en route: the unfaithful-
  replay fallback left `loc` shallower than -O0, mis-sizing the frame → now `loc = saved_loc`.
- **§19.3 per-site specialization** — a constant integer arg to a **read-only** param is
  constant-propagated (substituted at Ref sites, `ast_argsub_*`), not slot-stored. `gen_op`
  folds the constant arithmetic and, **inside a graft (pass 2)**, a constant-folded `if`
  condition **selects its taken branch and drops the dead block entirely** — this exceeds the
  original "const-fold template makes dead-branch elim fall out" framing (that only removes the
  conditional *jump*; the block-drop is a graft-only branch-select in `ast_replay_bb`, safe
  because pass 2 is exec-gated not byte-verified). Single-shape (no clone). Templates-gated.

Gates: exec corpus 269/269 -O0-identical across INLINE / INLINE+TEMPLATES / TEMPLATES (and the
sanitized mcc); ctest 1770/1770; ASan/UBSan-clean. Fixtures `ast/replay-inline` (struct params)
and `ast/replay-inline-spec` (specialization).

---

## 20. `-O1` flag → AST pipeline, and the benchmark/CI `-O1` columns (2026-07-08, PARKED)

This section records the `-O1` wiring work started 2026-07-08 and **parked mid-way** with a
concrete finding, so it can be resumed. Tracked in docs/TODO.md ("`-O1` flag → AST pipeline").

### 20.1 What landed

- **`mcc -O1` now engages the AST optimizer** (realizing §10's "`-O1` = a second driver over
  the same vstack ops"). In `gen_function`'s env setup, `s1->optimize >= 1` sets
  `ast_replay_env = 1` (arch-general; the replay driver is faithfulness-gated and falls back to
  `-O0` per function) and, on x86_64, `ast_promote_env = 1` (Tier-3 register promotion — the
  documented "first opt that beats `-O0`"). The `MCC_AST_*` env vars still force any layer on
  independently, on any arch. `-O0` is untouched (all of this is behind the replay path).
- **Tier-4 inline is deliberately NOT auto-enabled by `-O1`.** It is broadly functional (§ Tier-4
  / AST-STATUS §5) but not robust on large TUs — self-compiling `mcc` under `MCC_AST_INLINE` blows
  up combinatorially (grafting/defer-to-TU on an 800-function TU). It stays behind `MCC_AST_INLINE`
  until a size/complexity governor lands.
- **Benchmark `-O1` columns** (`tools/bench.c`): every detected compiler is measured twice — at its
  default level and at its first optimization level — interleaved per compiler for direct
  comparison. Flags: `mcc`/`gcc`/`clang`/`mingw` → `-O1`; MSVC/CL → `/O1` (the size-optimizing
  level, the literal "1"). A `struct compiler.opt` field carries the pre-spelled flag; the sysinfo
  compiler list de-dups the `-O1` twins.
- **CI needs no YAML change:** every benchmark step already `cat`s `dist/bench-*.txt` into the run
  summary, so the new `-O1` rows surface automatically across the dist-linux/macos/msvc/mingw/
  dist-windows jobs.
- **Graceful-fallback net, extended to hard errors.** The replay re-emission in `gen_function` is
  wrapped in a **nested `error_jmp_buf` trap**: an `mcc_error` raised by an imperfectly-captured
  construct mid-replay is caught (via a silent `ast_error_sink` diagnostic hook), and the parser's
  already-saved `-O0` bytes are restored — the same fallback a byte-mismatch takes, so `-O1` need
  not have modeled every op. The trap restores `nb_errors`, `error_func`, `vtop`, `loc`, and the
  `stk_data` cleanup stack via a new **`stk_data_floor`** (so `mcc_error`'s longjmp cleanup frees
  only what replay itself pushed, never the outer compile's live entries).

Gate: `ctest` 1769/1769; the AST replay/promote/inline columns are unchanged (the trap never fires
on the faithful corpus); `-O0` byte-identical.

### 20.2 The parked finding — full mid-replay recovery needs error-model surgery

The nested trap makes `-O1` compile the corpus and `full_language.c` (per-function graceful
fallback), but **not `mcc` self-compile**: after a caught replay-error, some codegen/pp-expr global
state stays inconsistent, and a later `#if defined(CONFIG_AST) && CONFIG_AST` mis-evaluates —
`error: bad preprocessor expression: #if 0 && 0`. Root cause: mcc's error model is **abort-and-
clean-up**, not nested recovery — the preprocessor's `#if` constant-expression evaluator shares the
vstack / jump-list / codegen machinery, so unwinding a longjmp out of arbitrary replay depth leaves
more than `stk_data` inconsistent (jump lists, `ind`, `nocode_wanted`, macro/token state).

`stk_data_floor` fixed one corruption source but is **not sufficient**. Full, safe mid-replay
recovery requires a broader save/restore of every replay-touched global (an explicit "codegen
checkpoint/restore"), which is the "error-model surgery" §18 anticipated — a real, self-contained
task, not a quick patch. **Until it lands, `mcc -O1` can fail a heavy-replay-error TU that `-O0`
compiles** (the benchmark honestly shows `mcc-self -O1 = n/a`); `-O1` is therefore **experimental**.

### 20.3 Resume plan

1. **Codegen checkpoint/restore** around the replay attempt: snapshot the full set of replay-mutated
   globals (jump lists / `rsym`, `ind`, `nocode_wanted`, `vtop`/`vstack` depth, macro & token state,
   `stk_data`) and restore them wholesale on the trap — turning the fragile partial unwind into a
   clean one. Then self-compile `-O1` should fall back cleanly and `mcc-self -O1` becomes a real row.
2. **Inline governor** (Tier-4): size/complexity cap + termination budget, then re-enable inline
   under `-O1`.
3. Fold in §19 (struct-by-value params + per-site specialization) once inline is `-O1`-safe.

---

## Decisions — final (ready for implementation)

### Ratified this pass (2026-07-08)

- **§11 construction** — the AST builds from vstack/parser hooks (`ast_hook_*`),
  **CST-independent**; overlapping hook/storage surface is gated `CONFIG_CST || CONFIG_AST`
  so each subsystem functions entirely without the other (A4). Lowering-from-CST is dropped.
- **§A1 sequencing** — liveness-steered **register promotion is the real -O1 payoff** →
  moved to **early Mid**; the template library beyond const-fold is **demoted to Long**
  (structural rewrites are the smaller win; const-fold only proves the mechanism).
- **§A2 control emission** — structured constructs replay by parser-pattern reproduction
  (shipped); the `goto`/`switch` remainder uses RPO layout + `bb_addr[]` fixups.
- **§A3 byte-tripwire** — scope is exactly `nbb==1` (+ zero templates/steering); a debug
  aid, never a gate.
- **§C1 acceptance bar** — exec goldens **+ the full GCC test suite** green under both
  `mcc -O0` and `mcc -O1`; `-O2`/`-O3` get **separate** replay drivers later.
- **§C2 hoisted temps** — anonymous per-temp `Sym`, register-preferred (easy *and* fast).
- **§18 query-first reframe** — the backend is **feature-complete C11** (the exec suite
  proves it); the parser is one driver demonstrating the opcodes span the language. The
  replay driver **never fakes/reimplements** — it "uses the tools it already has" to reach
  `-O0` parity, and beats `-O0` only via two AST-only queries (inline, register-promote).
  Every open item (VLA, `I`-unit, nested short-circuit) is a **Tier-2 query gap, not a
  codegen gap.** C11 flatten hazards (`setjmp`/VLA/signals) relocate to **guard queries**
  (§18.4), not new machinery. This supersedes any "faux-stack / reimplement / irreducible
  invoke floor" framing from earlier passes.

### Decided (2026-07-07 design pass)

- **Construction (§11)** — *superseded by the 2026-07-08 ratification above.* The original
  plan (eager `cst_hook_type` capture + lazy CST→AST lowering) is dropped: the shipped build
  captures types via `ast_hook_*` directly off the vstack (CST-independent), and the AST is
  built parallel-with-parse, still lazily gated by `ast_active` so `-O0` pays nothing. The
  "no standalone typing pass" and "-O0 pays nothing" conclusions carry over unchanged.
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

### Revisit triggers (backlog — tracked in docs/TODO.md)

Each is a **closed** decision paired with the named condition that would reopen it:

- **`Bind` marker** — stays dissolved into liveness (15 kinds). Reopen only if CST lexical
  scope spans prove insufficient for a `-g`/LSP scope query → *TODO: verify the CST answers
  every `-g`/LSP question without friction.*
- **`k` value** — default **`k=1`, widen-on-back-edge** (the instance hash detects the
  back-edge precisely; `k` only bounds permutation breadth). Raise `k` only under
  `-O2`/`-O3` or an explicit size budget (`k≈log_b(budget)`). *(Mid — inlining.)*
- **Outline threshold** — v1 **strict always-inline** (outline only to break recursion /
  escaped identities). Size-gated outline arrives as a *later binding-graph template*, not
  a v1 knob. *(Mid.)*
- **Store factoring** — plain per-fn arena now. Factor `store`/`binding`/`render` into the
  structure-agnostic engine at the **first virtual-inline render** (the validated second
  user — not speculative); this is also the shared-storage mechanism for A4. *(Mid.)*
- **Template representation (§12)** — **AST-fragment patterns + hash-index + hand-C guards**;
  revisit a declarative DSL past ~30 templates; data-file registry deferred (uniform
  function-pointer interface from day one). *(After the corpus is green under zero-template
  replay.)*
- **Defer-to-TU (§13)** — accept `-O1` is multi-pass (price of whole-TU always-inline). Add
  a per-function `-O1` mode only if compile latency becomes a complaint.
- **PP-as-executable-C (JIT)** — parked; promoted-not-necessitated by the
  include-permutation analysis.

### Residual ledger (implementation-validation only — no design ambiguity remains)

| Residual | Retired by |
|---|---|
| §A2 `gvtst` jump-list threads into the BB fixup list | first `goto`/`switch` `statements/` golden green |
| §11 `vtop` valid at every capture site | first full-function capture (structurally guaranteed) |
| §A3 tripwire predicate holds | `nbb==1` goldens byte-match under zero templates |
| §16 Mid coverage widening | the per-construct gap list in docs/TODO.md, one `ast/replay-*` fixture at a time |
