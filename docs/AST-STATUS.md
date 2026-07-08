# AST implementation status

Status of the `docs/AST.md` plan — the two-tier CST/AST intention IR and its `-O1`
replay driver. This is the *implementation* companion to `AST.md` (the design) and
`TODO.md` (the live task tracker). Last updated at commit `874953f3`; `ctest` 1769/1769.

All work is **x86_64**, gated under `CONFIG_AST`, and opt-in per feature via environment
variables (below). The default compiler (`-O0`, no env) is byte-untouched by every layer
here — verified empirically (see **Correctness gates**).

---

## 1. Summary

| Tier | What it is | Status |
|---|---|---|
| **Tier 1/2** | AST-replay driver over the vstack API; `-O0` replay parity | ✅ complete |
| **Tier 3** | Register promotion (mem2reg of address-not-taken locals) — the real `-O1` payoff | ✅ comprehensive (x86_64) |
| **Tier 4** | Virtual always-inline over the retained-AST store | 🟡 functional for a broad class; breadth remaining |
| Long-horizon | Template library, time-budgeted engine, LTO, `-g`, `-O2`/`-O3` | ⬜ design only |

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

## 5. Tier 4 — virtual always-inline 🟡 (functional; breadth remaining)

`MCC_AST_INLINE=1`. Inline a within-TU call in place of the boundary `Call`, replaying the
callee's body into the caller. Built in slices, all landed this session.

### What works

A **`static`, non-variadic, leaf** (calls nothing itself), VLA-free, size-bounded function
whose body ends in **one tail `return EXPR;`** — with **local declarations** and **internal
control flow (if/else, loops)** — is inlined into a **later** caller (defined-before-use).

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
  fresh code offsets (`gind`), so no label scoping is needed while the return is the single
  tail. Nested inlines compose (arg expressions graft recursively).
- **Pass structure:** grafting is a faithfulness-gated **pass-2** transformation, taking
  **precedence over promotion** for a function that has both (grafting removes the calls the
  promotion planner keyed its pool choice on).
- **Excluded:** pointer-to-VLA params (`int m[n][n]`) — indexing needs a runtime element
  size the frame bias can't relocate.

**Gates:** exec-golden (an inlined caller's bytes diverge from `-O0`) + the `ast/replay-inline`
fixture (asserts `add`/`scale`/`madd` (multi-statement) / `clamp` (two-if) graft).

### Remaining Tier-4 breadth (TODO.md "Slice 2 breadth")

- **Early / multiple returns** — needs an inline-end label and return-value coalescing
  (phi-like: each return path lands the value in one register at the join).
- **`goto` / `switch`** — these touch the shared label/switch replay state, so need scoping.
- **Non-scalar params/return** — struct-by-value, float params.
- **Forward-declared / later-defined callees** — needs true **defer-to-TU** (§13): hold ASTs
  until the TU closes, then lower in dependency order (leaves-first). Today only
  defined-before-use inlines, since the callee must be retained at the call site.
- **Cycle detection** — instance-hash ∈ ancestor stack (recursion guard).
- **Guard queries** — `setjmp`/signal/VLA regions → non-inlinable-across (§18.4).
- **Combine inline + promotion** in one pass 2 (currently inline takes precedence).

---

## 6. What remains beyond Tier 3/4

- **Spill-slot sharing across disjoint live ranges** — promotion currently pins each local
  for the whole function; sharing a pin between two non-overlapping locals needs a real
  backward-liveness pass + interval coloring (essentially a register allocator).
- **Other architectures (arm64/riscv64)** — the promo push/pop/write/seed encodings and the
  pin pools are x86_64-specific. Unlike x86_64 (where RBX/R12–R15 and XMM6/7 were already
  modeled as class-0/reserved registers), arm64's backend (`NB_REGS=28`) does not expose
  x19–x28 at all, so it needs a backend register-model extension per target + qemu
  validation. The arch-agnostic analysis (`ast_plan_promotion`, weighting, poison, the
  two-pass gate, the shared `get_reg` pin-skip) is reused as-is.
- **Acceptance bar §C1** (`AST.md`): wire the GCC test suite as an AST gate under both
  `mcc -O0` and the `-O1-replay` column (the exec-golden column is already green).

---

## 7. Correctness gates

- **`exec-replay` / `exec-replay-promote` corpus columns** — every `tests/exec` golden run
  under replay (and promotion) with output compared to `-O0`.
- **Byte-identity** — the default replay column byte-verifies against `-O0`; the backend
  addressing change was additionally diffed `.o`-for-`.o` across 55 files against the prior
  backend (0 differences).
- **`ast/replay-*` fixtures** — `replay-promote` and `replay-inline` assert the specific
  transformation fired (`[ast-promote] N fn` / `[ast-inline] grafted fn`) and the program
  still returns 42.
- **`ctest`** — 1769/1769 with all features off by default (each is opt-in), so `-O0` and the
  byte-verified replay column are unaffected.

Methodology held across the whole effort: land a change, run the full corpus + `ctest`, fix
the surfaced edge case, and commit only when green — reverting anything demonstrated
incorrect (e.g. two `p->m` promotion attempts were reverted rather than shipped).

---

## 8. Key source locations (`src/mccgen.c` unless noted)

| Area | Symbols |
|---|---|
| Replay driver | `ast_replay_body`, `ast_replay_bb`, `ast_replay_value` |
| Promotion plan | `ast_plan_promotion`, `ast_promo_weigh` (loop-depth weight), poison loops |
| Promotion emit | `ast_promo_write`, `ast_promo_entry_init`, `ast_promo_push`/`pop`, pools `ast_promo_caller`/`callee`/`xmm` |
| Two-pass gate | `gen_function`'s replay block (pass 1 byte-verify → pass 2 transform) |
| Inline analysis | `ast_fn_inlinable`, `ast_inline_capture`, `ast_inline_graftable`, `ast_inline_retain`/`lookup`, `ast_inline_pool` |
| Inline emit | `ast_inline_graft`, `ast_in_graft`/`ast_inline_bias`/`ast_graft_rt`, `AST_Return`/`AST_Invoke` cases |
| Backend addressing | `gen_modrm_impl`, `store()` — `src/arch/x86_64/x86_64-gen.c` |
