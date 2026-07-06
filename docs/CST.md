# CST.md — Post-completion decisions & next-phase direction

Companion to [PLAN.md](PLAN.md) / [IMPLEMENTATION.md](IMPLEMENTATION.md) /
[NOTES.md](NOTES.md). The CST database is **functionally complete** (all slices
S0/B–J + weaves + FINAL landed, `MCC_CST` ON by default, codegen byte-identical
on/off — see NOTES.md § Completed work). This file records the **chosen
direction for the residual "Now" items** and the next phase, captured
interactively on 2026-07-06. It changes no frozen PLAN.md invariant (§0 still
binds); it decides *how far* to push concrete-tree fidelity and *what comes
next*.

Legend: `[ ]` open · `[~]` in progress · `[x]` done.

---

## Implementation status (2026-07-06)

Progress against the sequenced plan below (tracker: [docs/TODO.md](TODO.md)):

- [x] **D4 scaffolding** — coverage gates (`cst/kinds-*`) + the headline template
  gate (`cst/template`); driver `tests/cst/kinds.cmake`.
- [x] **D1a** Expression fill-in — `Unary`/`Cast`/`Paren`/`Primary`.
- [x] **D1b** Declaration structure (D2 range-wrap) — `Declaration`/`FunctionDef`/
  `ParamList`/`Enum`/`TypeName`/`Initializer`/`Label`.
- [x] **D1c** PP-concrete — `IncludeDirective`/`PPDirective`/`PPConditional`
  (per directive line; gated on the main captured file). Surfaced + fixed a
  partial-overlap round-trip hazard in `cst_nest_specs`.
- [x] **D1d** `Comment` promotion — line/inline/block `CST_Comment` leaf nodes,
  excluded from `H_s`, folded into `H_t` (§8.4 held).
- [x] **D3+D5** — the template/binding/render model (content-addressed store
  with pure-`H_s` hash-consing + **debug collision tripwire**, recursive
  per-instance bindings, the `render(template, binding)` fold with a threaded
  environment, and `cst_render_identity` as the round-trip oracle). The
  **headline recursive re-include branch-selection gate** (`cst/template`)
  passes all five assertions. **Live capture wired** (`cst/incstore`): every
  real `#include` interns its file as a hash-consed **full-concrete**
  `SourceFile` template (`cst_build_sourcefile`: every line a leaf, each
  `#if/#else/#endif` a `PPConditional` with tagged branch bodies incl. dead
  branches) and binds the `IncludeDirective` node to it (dedup across nested +
  repeated includes; every template `render_identity`-round-trips).
- [x] **FINAL** — every gate re-run over the corpus; §0.1/§0.2 re-confirmed
  (CST-on 828/828, CST-off 811/811, codegen byte-identical on/off).

---

## Decision summary (2026-07-06)

| # | Axis | Decision |
|---|---|---|
| **D1** | Reserved node kinds | **Produce all remaining reserved kinds** — Declaration structure + Expression fill-in + PP-concrete — **and additionally promote `Comment`** (line / inline / block) from reserved to produced (PLAN §10 "9B trivia-as-nodes"). |
| **D2** | Slice-H bracketing | **Retroactive range-wrap** — reuse `cst_hook_open_at` + `cst_nest_specs`, the technique already proven on the expression cascade. No grammar edits, exit-count-agnostic, lowest codegen-identity risk. |
| **D3** | Slice G (includes) | **Implement now as a `SourceFile` static template + per-instance binding.** The file's concrete tree (all `#if`/`#else` branches captured) is a **static template** keyed by **pure `H_s(body)`** and hash-consed once; per-include variation lives in a small **binding**; output = `render(template, binding)`. Pulls PLAN's deferred **4C** (hash-consing) + **6B** (content-addressed store) forward. |
| **D4** | Next phase | **TDD-first gap closure** — before any consumer (`-g`/LSP/opt), write **recursive** tests that expose gaps in the *current* implementation and drive fixes red→green. Consumer selection deferred until the tree is test-proven complete. |
| **D5** | Template renderer | **Build the full `render(template, binding)` renderer now** (not a deferred seam), proven by the recursive **re-include branch** gate: `#include "file.h"` → `#define FEATURE_FLAG_TOGGLE` → `#include "file.h"` selects the `#else` subtree, from the *same* deduped template. |

---

## D1 — Finish every reserved node kind (+ Comment)

Target: zero reserved-but-unproduced kinds. Grouped by the build approach each
needs.

### D1a — Expression fill-in *(cheapest, do first)*
`Unary`, `Cast`, `Paren`, `Primary` (ident/const/string/char).
- **Approach:** extend the existing retroactive range-wrap already producing
  `Binary`/`Cond`/`Comma`/`Member`/`Index`/`Call`. Same `cst_hook_open_at` +
  `cst_nest_specs` machinery; only new open-specs at the `unary()` /
  primary-expression / cast sites.
- **Note:** low *incremental* value (the `Token` leaf already carries the datum,
  and the hash child-count salt already distinguishes `a+b` from `a+(b)`), but
  cheap and it makes the tree fully explicit for consumers that walk kinds
  rather than leaves. `Paren` in particular makes parenthesization first-class
  instead of hash-implicit.

### D1b — Declaration structure *(via D2 approach)*
`Declaration`, `FunctionDef`, `ParamList`, `Enum`, `TypeName`, `Initializer`,
`Label`.
- **Approach:** retroactive range-wrap (D2). Mark decl start at the external-decl
  loop / `post_type()` entry, segment items at their end, wrap by leaf-range
  containment — no goto-epilogue, no caller-wrap, no hot-path grammar edits.
- **Value:** high — `FunctionDef`/`Declaration` grouping = LSP document-symbols +
  folding ranges and `-g` function spans; `ParamList` = signature help; `Enum` /
  `TypeName` / `Initializer` complete the declaration subtree.
- `Label` currently falls through as a plain statement leaf (`block()` produces
  `Goto` but not `Label`); add the `Label` open at the label site.

### D1c — PP-concrete *(slice-J-style boundary capture)*
`IncludeDirective`, `PPDirective` (generic `#…`), `PPConditional`
(`#if`/`#ifdef`/`#else`/`#endif`).
- **Approach:** capture at the preprocessor boundary the same way slice J wrapped
  `MacroInvocation` (`cst_hook_wrap` at the PP site), since the post-expansion
  parser never sees these lines. Span covers the written directive text.
- `IncludeDirective` couples to **D3** — its cross-file node-id targets the
  included file's `SourceFile` node.
- **Value:** high for LSP (include-navigation, directive semantics) and for
  round-trip fidelity of directive lines.

### D1d — Comment promotion (9B) *(new, beyond the original 14)*
Promote the reserved `Comment` kind to produced, for **line** (`//…`), **inline**
(`/*…*/` mid-line), and **block** (`/*…*/` spanning) comments.
- **Approach:** today trivia is lumped as one leading-whitespace piece per leaf
  and excluded from `H_s` (slice G notes). Promotion = emit `Comment` nodes from
  the trivia classifier and fold their bytes into `H_t` (trivia channel) only —
  **`Comment` must stay out of `H_s`** so the §8.4 hash-invariance gate (comment
  edits don't dirty the structural hash) still holds. This is the one D1 item
  that touches the whitespace/comment-invariance invariant, so it needs its own
  gate (see D4).
- **Value:** doc-comment extraction (LSP hover), comment-preserving refactors.

**Ordering within D1:** D1a → D1b → D1c → D1d (cheap-and-safe → structural →
boundary → trivia-sensitive), each behind its own round-trip + tiling gate so a
regression is localized to the slice that caused it.

---

## D2 — Slice-H approach: retroactive range-wrap

Chosen over goto-epilogue / caller-wrap / leave-as-is. `decl()`@10074 has 4
returns and `post_type()` has loops/continues — the reasons grouping was
originally deferred. Range-wrap sidesteps all of it:

- Record a **mark** (`cst_mark()` = current leaf index) at decl/item start.
- Let the existing descent run and emit its flat leaves + inner specs unchanged.
- In `cst_hook_end`, **wrap retroactively** by leaf-range containment
  (`cst_hook_open_at` + `cst_nest_specs`) — exactly the mechanism that resolved
  the expression cascade's left-recursion cleanly.
- **No** edits to the multi-exit control flow, so the **codegen-identity gate
  (§8.5) stays mechanically green** — the decisive advantage over the
  goto-epilogue refactor, which touches hot compiler paths.
- Crash-hardening already learned in H applies: **drop empty-range specs**
  (macro-expanded / zero-width items) rather than wrapping them.

---

## D3 — Slice G as a content-addressed `SourceFile` node

**Chosen design (your reframing of G):** instead of plain per-file subtree
ownership, model each file as a first-class **`SourceFile` node** that carries a
**snapshot/hash of the preprocessor context** it was included under, so the
file's body subtree can leverage the hierarchical/incremental hashes and
**identical include instances dedup** instead of duplicating chunks in the tree.

### The template / binding split (finalized)

DQ-1/DQ-2 (the old `H_ctx`-fragmentation and context-builtin worries) are
**dissolved** by separating a static template from a per-instance binding. The
key realisation: a file's *bytes* are fixed, so its structure is fixed — context
never belongs in the dedup key, it belongs in a binding applied at render time.
We do **not** assume "identical bytes = identical output"; we assert "identical
bytes = identical **template**, and output = `render(template, binding)`."

- **`SourceFile` node = a static template.** Promotes/repurposes the per-file
  `TranslationUnit` of PLAN §1 Includes; owns the file's byte copy and roots its
  concrete tree. **Full-concrete (Table A = A): it captures *all* `#if`/`#else`
  branches as concrete nodes under `PPConditional`** — dead branches included.
  This is the correct CST (concrete = written source, best for LSP) *and* what
  makes branch selection a render-time binding instead of a structural fork.
- **Dedup key = pure `H_s(body)`.** Position-independent (PLAN §3), so it is
  stable across every include site. **`H_ctx` is gone from the key.** Two
  `#include`s of the same header — *regardless of context, include guards, or
  which `#if` branch is live* — share one physical subtree (referenced by
  `(file,local)` id). This is PLAN §10 **4C hash-consing**, now unconditional.
- **Per-instance binding (Table B) = the holes.** A small side-table per include
  instance holding only the genuinely per-instance values:
  - `#if` **branch selectors** — the controlling-macro truth values that pick the
    live branch of each captured `PPConditional`;
  - `__COUNTER__` slot values and `__INCLUDE_LEVEL__`;
  - function-like **macro arguments** supplied at the call site;
  - the `__FILE__` include-path (edge case: differs only across distinct `-I`
    resolutions of the same file).
  - **Not** in the binding (static → stay in the template): `__LINE__`
    (line-within-file, fixed per token position), `__DATE__`, `__TIME__`.
- **`render(template, binding)` = the output**, built **now** (D5, Table C = B) —
  not a deferred seam. Renders the live-selected token stream. Round-trip (§8.1)
  is `render` with the **identity binding** (emit the written source, all
  branches), which is already the current behaviour, so the round-trip oracle and
  the expansion renderer share one code path.
- **Render is a fold with a threaded environment.** Including a file mutates
  global PP state (its `#define`s affect later files), so `render` walks the
  include tree threading a PP environment: each `SourceFile` consumes the inbound
  environment (which fixes its branch selectors) and emits the outbound one to
  its successors. Full-concrete capture keeps this clean — the environment
  selects among branches that are *all* physically present in the shared template.
- **`IncludeDirective` (D1c)** holds the cross-file node-id of the target
  `SourceFile` template + a reference to the include-site binding — PLAN §1's
  stitch-by-id, now content-addressed.
- **Backbone for 6B / hot-reload:** the `H_s(body)` store is exactly the
  content-addressed snapshot store PLAN §10 6B reserved, and the substrate for
  the TODO "Later" hot-reload / reconciled-CST-snapshot notes. This decision
  promotes 4C + 6B from *deferred* to *now* — a deliberate scope increase
  recorded here, not a silent drift.

### The invariant guard (kept from old DQ-3)

Dedup must never change round-trip (§8.1) or tiling (§8.2): a shared subtree must
`render` to the byte-identical text of *each* include site and tile each parent.
Relative widths (PLAN §2) make a shared subtree position-independent by
construction, but this is the highest-risk new invariant, so it gets an explicit
recursive gate — see D4/D5.

---

## D4 — TDD-first gap closure (before any consumer)

No consumer (`-g` / LSP / optimization, PLAN §9 M5+) starts until the current
implementation is **test-proven** for the gaps we already know about and any the
tests surface. Build the tests first, watch them fail, then close.

### Headline gate — recursive re-include branch selection (D3 + D5)

The **primary** new gate, written first and red until D3/D5 land. It proves the
template is deduped *and* the binding correctly selects a different branch:

```c
/* file.h */
#ifdef FEATURE_FLAG_TOGGLE
  int chosen = 1;      /* #if branch  */
#else
  int chosen = 0;      /* #else branch */
#endif
```
```c
/* driver */
#include "file.h"            /* instance 1: FEATURE_FLAG_TOGGLE undefined → #else */
#define FEATURE_FLAG_TOGGLE
#include "file.h"            /* instance 2: defined            → #if   */
```

Assertions (all must hold simultaneously):
1. **One template, deduped:** both includes reference the **same** `SourceFile`
   node id — a single `H_s(body)` — even though they render differently.
2. **Both branches present in the template:** the `#if` *and* `#else` bodies are
   captured as concrete children of the `PPConditional` (full-concrete, Table A).
   Assert the `#else` subtree was hashed (has a stable `H_s`) even on instance 2
   where it is *not* rendered, and the `#if` subtree likewise on instance 1.
3. **Binding selects correctly:** instance 1's binding renders the `#else`
   (`chosen = 0`); instance 2's renders the `#if` (`chosen = 1`).
4. **Render == compiler output:** each rendered instance is byte-identical to
   what the single-pass compiler actually preprocessed at that site (the oracle).
5. **Recursive:** the same holds when `file.h` itself `#include`s a nested header
   with its own toggled `#if` — the fold threads the environment through the
   nesting and each level's shared template renders its own branch.

### Other known gaps to write failing tests for
- Every D1 kind: a fixture asserting the kind is **produced** (not just that
  round-trip holds) — a coverage assertion, since round-trip alone passed even
  while these kinds were absent.
- **Comment (D1d):** comment-only edit leaves `H_s` fixed **and** now yields a
  `Comment` node whose bytes are in `H_t`; a code edit changes `H_s`.
- **Dedup across contexts (D3):** same header included twice under *differing*
  context ⇒ still **one** physical subtree (assert shared id), rendered two ways
  — the whole point of pure-`H_s` keying vs. the old `H_ctx` fragmentation.
- **Static builtins stay in the template:** a header using `__LINE__` renders the
  same line numbers at both include sites (static per position) while
  `__COUNTER__`/`__INCLUDE_LEVEL__` differ per instance via the binding.
- **Slice-I scope gap (already documented):** last-declaration-wins mis-resolves
  shadowed names across scopes (NOTES.md slice I) — a failing `sym_ref` test on a
  shadowing fixture pins whether we fix it now or keep the documented limitation.
- **Slice-J v1 imprecisions (already documented):** function-like invocation may
  drop the trailing `)`; object-like macros inside another macro's args stay
  plain — failing tests decide fix-vs-keep per NOTES.md slice J.

### Method
Mirror the existing `tests/cst/{store,hash,geom,serial,sym,symref,macro}` shape
+ the `cst_validate` corpus gate. Each new gate registered via `add_test`; each
D1/D3 slice lands only when its own gate goes green **and** the full corpus
round-trip (§8.1) / tiling (§8.2) / offset→node (§8.3) / hash-invariance (§8.4) /
snapshot (§8.6) / **codegen-identity (§8.5)** stay green.

---

## Sequenced plan

1. **D4 scaffolding** — write the failing gates for every gap above (red),
   **headline recursive re-include branch gate first**.
2. **D1a** Expression fill-in (`Unary`/`Cast`/`Paren`/`Primary`) → green its gate.
3. **D1b** Declaration structure via **D2** range-wrap → green.
4. **D1c** PP-concrete (`IncludeDirective`/`PPDirective`/`PPConditional`),
   **full-concrete** — capture *all* `#if`/`#else` branches as concrete nodes →
   green. Prerequisite for D3's branch-selection binding.
5. **D3 template + D5 renderer together** — `SourceFile` static template,
   pure-`H_s(body)` hash-consing, per-instance binding, and the full
   `render(template, binding)` fold with threaded environment. Land the invariant
   guard + the **headline recursive re-include branch gate** green (the `#else`
   subtree is hashed on the deduped template and rendered on the toggled
   instance). Round-trip = render-with-identity-binding shares the path.
6. **D1d** `Comment` promotion (`H_t`-only) → green, with §8.4 held.
7. **FINAL** re-run every gate over the full corpus; re-confirm §0.1/§0.2 via the
   codegen-identity gate; then (separately) revisit the consumer choice
   (Table 4) now that the tree is test-proven complete.

Each step is independently gated and independently revertible; none edits the
multi-exit grammar control flow (D2), so the codegen-identity invariant is never
at risk from a structural change.
