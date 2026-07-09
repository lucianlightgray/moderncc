# AST implementation status

Status of the `docs/AST.md` plan — the two-tier CST/AST intention IR and its `-O1`
replay driver. This is the *implementation* companion to `AST.md` (the design) and
`TODO.md` (the live task tracker). Last updated 2026-07-09 (merge `81fe657e`);
`ctest` 1770/1770 (x86_64), 1769/1769 (arm64).

Gated under `CONFIG_AST` and opt-in per feature via environment variables (below). Tier 1/2
replay and Tier-4 virtual-inline are **arch-independent** (Tier-4 is now verified on both
x86_64 and arm64); only **Tier-3 register promotion is x86_64-specific** (its pin pools and
push/pop encodings). The default compiler (`-O0`, no env) is byte-untouched by every layer
here — verified empirically (see **Correctness gates**).

---

## 1. Summary

| Tier | What it is | Status |
|---|---|---|
| **Tier 1/2** | AST-replay driver over the vstack API; `-O0` replay parity | ✅ complete |
| **Tier 3** | Register promotion (mem2reg of address-not-taken locals) — the real `-O1` payoff | ✅ comprehensive (x86_64) |
| **Tier 4** | Virtual always-inline over the retained-AST store | ✅ broadly complete — incl. defer-to-TU, rodata/struct/forward callers, **struct-by-value params (§19.2)**, and **per-site specialization + dead-branch elim (§19.3)** |
| **A4 gate** | GCC c-torture as a differential AST acceptance gate (§C1) | ✅ wired (replay/promote/inline columns); replay green, transform columns baselined |
| Long-horizon | Backward-liveness spill-slot sharing (A1), template library, time-budgeted engine, LTO, `-g`, `-O2`/`-O3` | ⬜ design only |

The replay driver reconstructs a function's codegen from its captured intention IR, and
beats `-O0` via two AST-only transformations — **register promotion** (Tier 3) and
**virtual inline** (Tier 4) — each applied as a *faithfulness-gated second pass*:

> Pass 1 replays with no transformation and **byte-verifies against `-O0`**. Only if
> faithful does Pass 2 re-replay *with* the transformation, whose divergent bytes are then
> gated by the **exec-golden differential** (run the program, compare output), never by
> byte-identity. An unfaithful capture falls back to the parser's `-O0` emission.

---

## 2. Environment flags

| Flag | Effect |
|---|---|
| `-O1` (or higher) | **Experimental (docs/AST.md §20).** Engages the AST optimizer: replay (all arches) + Tier-3 register promotion (x86_64). Tier-4 inline is *not* auto-enabled (needs a governor). Falls back to `-O0` per function; a nested error trap catches replay hard-errors, but recovery is incomplete on heavy-replay-error TUs (mcc self-compile). |
| `MCC_AST_REPLAY=1` | Enable the AST-replay driver (Tier 1/2). Byte-identical to `-O0`. |
| `MCC_AST_REPLAY_DUMP=1` | Dump replayed trees + `[ast-promote]`/`[ast-inline]` activity to stderr. |
| `MCC_AST_PROMOTE=1` | Tier-3 register promotion (needs replay). |
| `MCC_AST_NO_CALLFUL=1` | Restrict promotion to the call-free register pool (escape hatch). |
| `MCC_AST_INLINE=1` | Tier-4 virtual always-inline (needs replay). |
| `MCC_AST_TEMPLATES=1` | Run optimization templates (const-fold); byte-neutral to replay. |

---

## 3. Tier 1/2 — `-O0` replay parity ✅

The AST-replay driver (`ast_replay_body` → `ast_replay_bb` → `ast_replay_value`) re-emits
from the captured IR over the existing vstack primitives (`vpush*`/`gen_op`/`gv`/`gsym`/
`gjmp`) — **not** a refactor of `gen_op`. For a single-BB function it is **byte-identical**
to `-O0` (the `nbb==1` byte-tripwire, `AST.md` §A3); across control flow it is correct but
block layout may reorder, so byte-identity is a debug aid there, not a gate.

All Tier-2 query gaps are closed: nested short-circuit `&&`/`||`, `_Complex` construction,
and VLA/`alloca`. The whole-corpus `exec-replay` column is the gate.

---

## 4. Tier 3 — register promotion ✅ (comprehensive, x86_64)

`MCC_AST_PROMOTE=1`. An **address-not-taken** local is pinned to a register with zero stack
load/store traffic, **seeded from its stack slot at function entry** so the register mirrors
what `-O0` would read — valid across arbitrary control flow (loops/if) and for parameters or
read-before-write locals. The first optimization that deliberately beats `-O0`.

### Coverage

| Dimension | Support |
|---|---|
| **Scalar kinds** | `int`, `long`, **pointer** (value; `*p`/`p[i]` deref), **`float`/`double`** |
| **Call context** | **call-free** (caller-saved R10/R9/R8) and **call-ful** (callee-saved RBX/R12–R15, pushed at entry / popped at the single return funnel with even-count alignment pad) |
| **Float pins** | XMM6/XMM7 (`RC_XMM6`/`RC_XMM7`, never allocated → free to pin); call-free only (all XMM caller-saved) — no backend register-set extension needed |
| **Selection** | when candidates exceed the pin pool, promote the highest-weighted first: weight = `2^loop-depth` summed over references (a loop is an `If` node with `op==2`), so inner-loop locals win the pins |
| **Mixed pools** | one function mixes int/pointer (GP) and float (XMM) pins |

### Poisons (never promoted)

`&`-taken locals (`AST_OP_ADDR`), aggregate member-base (`.`) and `p->m` bases, whole local
array/struct slot ranges (constant-index elements alias the aggregate via pointer
arithmetic), `++`/`--` (lvalue) targets, bitfields, `volatile`/`_Atomic`, structs, and any
function using inline asm. `VLA + call-ful` bails (rsp race).

### Backend fix that this required (byte-neutral)

Pointer promotion exercised a latent gap: a memory operand based on **r12–r15** was
mis-encoded because the normal allocator never bases off those registers. Fixed in
`gen_modrm_impl` (SIB byte for an r12 base, forced disp8 for r13) and `store()` (REX.B from
the destination base for the 32-bit/byte paths). **Verified byte-identical to the old
backend across 55 corpus `.o` files.**

### Two documented not-done cases (blockers analyzed, in the code + TODO)

- **`p->m` pointer bases** — the member lowering folds the byte-offset with
  `gen_op('+', const)` = `add $off, %base` *in place*, which corrupts the pin. Folding the
  offset as an addressing displacement instead breaks `-O0` (gen_modrm's plain-register-base
  path deliberately ignores a nonzero `c`). Needs a distinct "register base + live
  displacement" addressing form.
- **More than 2 float pins** — would need a backend extension to XMM8–15 + their SSE REX
  encoding.

**Gates:** two-pass faithfulness + whole-corpus `exec-replay-promote` column + the
`ast/replay-promote` fixture (asserts a call-free, a call-ful, a pointer, and a float
function each promote).

---

## 5. Tier 4 — virtual always-inline 🟢 (broadly functional; narrow breadth remaining)

`MCC_AST_INLINE=1`. Inline a within-TU call in place of the boundary `Call`, replaying the
callee's body into the caller. Built in slices, all landed this session.

### What works

A **`static`, non-variadic**, VLA-free, size-bounded function — with **local declarations**, **any
intra-function control flow (if/else, loops, `switch`, `break`/`continue`, `goto`/named labels)**,
**one or more value returns including early returns inside branches**, **`int`/pointer/`float`/
`double` scalar params, by-value struct/union params (§19.2), and struct/scalar returns**, and
**its own calls** (leaf or not) — is
inlined into a caller, whether the callee is defined **before OR after** it: a caller-before-callee
site is handled by **defer-to-TU** (end-of-TU re-emission of the caller with the now-retained callee
inlined, its symbol repointed). The callee's control-flow replay state
(labels via a **label floor**, plus `switch`/`break`/`continue`) is isolated from the caller's. Returns coalesce **via memory** (each stores to a dedicated
result slot — struct-sized for a struct return — non-tail returns jump to a graft-local inline-end
join), so several grafts feeding one call don't fight over a return register. A **non-leaf**
callee's own calls graft recursively (a depth+stack **cycle guard**, max depth 8, stops
direct/mutual recursion — the recursive call stays real) or emit a real call. Composes with
Tier-3 register promotion in one pass 2. String-literal / anon-rodata refs are handled by
**pinning** the captured Syms (`ast_pin_rodata_syms` + transitive `ast_pin_type` for aggregate/ptr
type refs), so both a string-referencing **callee** (graft) and a string/struct-referencing
**forward caller** (defer-to-TU re-emit) work. **Excluded** (fall back to a real call):
`_Complex`/`long double`/bitfield params, pointer-to-VLA params, and
`setjmp`-calling callees (guard query). Re-emission additionally refuses a function that captures a
**block-scoped** symbolic/type ref (an inner `extern`/function redeclaration) — that Sym is freed at
block close and dangles by end-of-TU, so `ast_reemit_poison` forces a real forward call (immediate
replay/grafting are unaffected — they run while the Sym is live).

- **Retention** (`ast_inline_retain`, keyed by function Sym; `ast_inline_lookup`): the
  within-TU inline closure held in memory. Non-graftable candidates are retained-only.
- **Metadata capture** (`ast_inline_capture`, right after `gfunc_prolog`): param frame
  offsets in ABI order, each param's `type.t` **and** `type.ref` (enum/pointer), and the
  frame size.
- **Grafting** (`ast_inline_graft`, at a caller's `AST_Invoke`): reserve fresh caller stack
  (`loc -= callee_frame`, kept so `gfunc_epilog` sizes the frame from the lowered `loc`);
  store each arg to its **relocated** param slot (`param_off + bias`); replay the whole body
  via `ast_replay_bb` under **`ast_inline_bias`** (added to every `VT_LOCAL` ref offset in
  `ast_replay_value`); `ast_in_graft` makes the tail `Return` leave its value on vtop cast to
  the call's result type instead of emitting the epilogue transfer. Internal branches use
  fresh code offsets (`gind`). Returns coalesce via memory: each stores its return-cast value
  to a per-graft result slot; non-tail returns (`op==1`) jump to the inline-end join
  (`ast_inline_ret_sym`), and the slot is pushed as an lvalue after the join. Nested inlines
  compose (arg expressions graft recursively). The **bias shifts below the callee's positive
  param extent** (`bias = (loc - hi) & -16`, `hi` = highest `param_off + size`): x86_64 spills
  register params to negative loc slots (`hi == 0`, `bias == loc`, byte-identical), but arm64
  spills them ABOVE the frame pointer, so without the shift a low positive param offset would
  land on the caller's saved fp/lr under the negative bias. A **struct return via an sret
  hidden pointer** (arm64's `ret_nregs==0` path for ≤16-byte structs) carries an extra
  `Invoke` child at index 1 that grafting skips (the return coalesces into the result slot, so
  the sret buffer is bypassed).
- **Struct-by-value params (§19.2):** the same `hi`-bias handles them — the ABI is *deleted*,
  not reproduced. A by-value struct arg is materialized into its biased slot with a `vstore`
  block-copy; the body reads members at `member_off + bias`. No ABI classification is needed:
  register-class and memory-class (>16-byte) structs, and arm64-positive params, all land in
  the reserved frame the `hi`-bias carved out. (This replaced an initial per-param-remap
  prototype — the uniform `hi`-bias subsumes it.)
- **Per-site specialization (§19.3, `MCC_AST_TEMPLATES`):** a **constant integer arg** bound to
  a **read-only param** (`ast_local_is_readonly`: never a `Store` child0, never under a `Unary`)
  is constant-propagated — its `Literal` substitutes at the param's Ref sites (`ast_argsub_*`,
  consulted in `ast_replay_value`) instead of a slot store+reload, so `gen_op` folds the
  arithmetic and — inside a graft (pass 2) — a condition folding to a compile-time constant
  **selects its taken branch and drops the dead block entirely**. Single-shape graft (no clone).
- **Pass structure:** grafting is a faithfulness-gated **pass-2** transformation, taking
  **precedence over promotion** for a function that has both (grafting removes the calls the
  promotion planner keyed its pool choice on).
- **Excluded:** pointer-to-VLA params (`int m[n][n]`) — indexing needs a runtime element
  size the frame bias can't relocate; `_Complex`/`long double`/bitfield params.

**Composes with Tier-3 promotion:** pass 2 runs both when applicable — a function inlines its
calls and promotes its own locals (the `setjmp` guard excludes `setjmp`-calling callees, and
inline+promote are exec-verified together).

**Gates:** exec-golden (an inlined caller's bytes diverge from `-O0`) + the `ast/replay-inline`
fixture (asserts `add`/`scale`/`madd`/`clamp`/`sgn`/`area`/`quad`/`pick`/`firsthit`/`mkpair`/`gsum`/
`sumpt`/`sumbig`(>16B)/`addpt` graft and `fwd_sum`/`fwd_boxed` defer-to-TU re-emit) + the
`ast/replay-inline-spec` fixture (§19.3, asserts `choose`/`clampk`/`mul`/`addk` **specialize**) +
a whole-exec-corpus differential (`-O0` vs `MCC_AST_INLINE` byte/exit-identical). `ast/replay-inline`
is registered on **x86_64 AND arm64** (`MCC_CPU STREQUAL x86_64 OR arm64`); `ast/replay-inline-spec`
is x86_64-gated (the dead-branch selection is verified on x86_64 so far).

### Remaining Tier-4 breadth (TODO.md "Slice 2 remainder")

- **Struct-by-value params — ✅ DONE (§19.2)** via the `hi`-bias + `vstore` block-copy (see the
  grafting note); `_Complex`/`long double`/bitfield params still fall back.
- **Per-site specialization — ✅ DONE (§19.3)** — constant-arg const-prop + dead-branch elim
  under `MCC_AST_TEMPLATES` (see the grafting note). Full binding-keyed per-site *clones* (distinct
  rendered bodies) stay deferred to the §9 store-factoring milestone.
- **arm64 — ✅ DONE for `ast/replay-inline`** (positive-param `hi`-bias + sret hidden-child skip).
  Remaining: un-gate `ast/replay-inline-spec` on arm64; riscv64 and other native arches stay
  gated until verified there.

---

## 6. What remains beyond Tier 3/4

- **A1 — spill-slot sharing across disjoint live ranges (the last ratified roadmap item).**
  Promotion currently pins each local for the whole function, so once the pin pool is full
  (5 GP + 2 XMM) the remaining candidates spill. Sharing a pin between two locals whose live
  ranges don't overlap needs a real **backward-liveness pass** + interval coloring
  (essentially a register allocator over the AST CFG). Purely additive to coverage — a valid
  coloring never changes observable behavior, so it stays behind the same two-pass gate.
  **Key non-obvious design point (blocks a naive implementation):** promotion **seeds every
  pin from its stack slot at function entry** (`ast_promo_entry_init`) so the register mirrors
  what `-O0` reads for read-before-write locals, params, and loop-carried values. This
  *conflicts* with sharing — two locals can't both be entry-seeded into one register. A1 must
  therefore replace entry-seeding with **per-live-range seeding** (seed a shared local from its
  slot at its range *start*, not at entry), which is a rework of the promotion emit model, not
  just an added analysis. Sequencing note: the `-O1` transform soundness backlog (below) should
  be driven down first — a register allocator built on a promotion pass that already miscompiles
  14 gcc-torture cases inherits and compounds those holes.
- **`-O1` transform soundness backlog (surfaced by the A4 gate).** The gcc c-torture
  differential baselined **14 promote + 4 inline + 3 replay** `KNOWNGAP`s (all behind the
  experimental `MCC_AST_PROMOTE`/`MCC_AST_INLINE` flags, not default `-O0`). These diverge from
  `-O0` by construction, so byte-verify can't catch them — the gate is their only net. Notable:
  a promoted **float pin (XMM6/7) clobbered by `gen_opf` operating in place** (root-caused;
  needs real float-promotion register discipline — the naive "copy the pin read to a scratch"
  regressed the corpus), and the 2 `pr51581` replay gaps = the parked §20 pp-const-expr
  state-corruption. Each fix shrinks the corresponding `GCCTS_AST_KNOWN_*` list in
  `tools/mccharness.c`.
- **Other architectures (arm64/riscv64) for promotion** — the promo push/pop/write/seed
  encodings and pin pools are x86_64-specific. Unlike x86_64 (where RBX/R12–R15 and XMM6/7 were
  already modeled as class-0/reserved registers), arm64's backend (`NB_REGS=28`) does not
  expose x19–x28, so it needs a backend register-model extension per target + qemu validation.
  The arch-agnostic analysis (`ast_plan_promotion`, weighting, poison, the two-pass gate, the
  shared `get_reg` pin-skip) is reused as-is. (Tier-4 *inline* is already arch-independent and
  arm64-verified.)
- **Acceptance bar §C1 — ✅ gate WIRED (2026-07-09).** The GCC c-torture suite runs as a
  differential AST gate (`mccharness gcctestsuite --ast <replay|promote|inline|inline-tmpl>` +
  `gcctestsuite-ast-*` cmake targets): each test at `-O0` baseline AND under the AST column, a
  test passing at `-O0` but failing under the column = REGRESSION. It surfaced (and fixed) 7
  pre-existing replay miscompiles the exec corpus never caught; the residual is the backlog
  above. "Done" for full §C1 = drive that backlog to zero.

---

## 7. Correctness gates

- **`exec-replay` / `exec-replay-promote` / `exec-replay-tmpl` corpus columns** — every
  `tests/exec` golden run under replay (and promotion / templates) with output compared to `-O0`.
- **GCC c-torture differential AST gate (§C1)** — `mccharness gcctestsuite --ast <column>` runs
  the ~3766-test suite at `-O0` AND under the AST column; regression = passes at `-O0`, fails
  under the column. Replay column green (0 regressions); promote/inline columns baselined
  (`GCCTS_AST_KNOWN_*`). Manual targets `gcctestsuite-ast-{replay,promote,inline}` (need
  `MCC_GCCTESTSUITE_PATH`).
- **Byte-identity** — the default replay column byte-verifies against `-O0`; the backend
  addressing change was additionally diffed `.o`-for-`.o` across 55 files against the prior
  backend (0 differences).
- **`ast/replay-*` fixtures** — `replay-promote`, `replay-inline`, and `replay-inline-spec`
  assert the specific transformation fired (`[ast-promote] N fn` / `[ast-inline] grafted fn` /
  `[ast-inline] specialized fn`) and the program still returns 42.
- **`ctest`** — 1770/1770 (x86_64) with all features off by default (each is opt-in), so `-O0`
  and the byte-verified replay column are unaffected.

Methodology held across the whole effort: land a change, run the full corpus + `ctest`, fix
the surfaced edge case, and commit only when green — reverting anything demonstrated
incorrect (e.g. two `p->m` promotion attempts and the naive float-pin-scratch fix were reverted
rather than shipped).

---

## 8. Key source locations (`src/mccgen.c` unless noted)

| Area | Symbols |
|---|---|
| Replay driver | `ast_replay_body`, `ast_replay_bb`, `ast_replay_value` |
| Promotion plan | `ast_plan_promotion`, `ast_promo_weigh` (loop-depth weight), poison loops |
| Promotion emit | `ast_promo_write`, `ast_promo_entry_init`, `ast_promo_push`/`pop`, pools `ast_promo_caller`/`callee`/`xmm` |
| Two-pass gate | `gen_function`'s replay block (pass 1 byte-verify → pass 2 transform) |
| Inline analysis | `ast_fn_inlinable`, `ast_inline_capture`, `ast_inline_graftable`, `ast_inline_retain`/`lookup`, `ast_inline_pool`, `ast_local_is_readonly` (§19.3) |
| Inline emit | `ast_inline_graft` (`hi`-bias, hidden-sret, struct block-copy §19.2), `ast_in_graft`/`ast_inline_bias`/`ast_graft_rt`, `ast_argsub_*` (§19.3 substitution), `AST_Return`/`AST_Invoke`/`AST_If` cases |
| Computed-callee bail | `ast_hook_call_begin` (callee child0 must be `AST_Ref`) |
| GCC-torture AST gate | `suite_gcctestsuite` (`--ast` differential), `gccts_ast_skiplisted`, `GCCTS_AST_KNOWN_*` — `tools/mccharness.c` |
| Backend addressing | `gen_modrm_impl`, `store()` — `src/arch/x86_64/x86_64-gen.c` |
