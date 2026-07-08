# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (migrated to docs/NOTES.md).

---

# AST — coverage widening (docs/AST.md §16 Mid)

The AST intention-IR **first phase (A1–A7) is complete and green** — intention-IR
library, vstack-replay driver, three-layer differential gate, and the const-fold
template. The completion record is in docs/NOTES.md ("Completed work — AST
intention-IR (first phase: replay driver + first template)"); the design is in
docs/AST.md. `-O0` codegen is byte-identical with `CONFIG_AST` on; the driver runs
only under `MCC_AST_REPLAY`, and the byte-verify net keeps every unmodeled construct
on correct `-O0` fallback — so widening coverage cannot break output correctness,
only add (or fail to add) replayed functions.

**Query-first framing (docs/AST.md §18).** The vstack/ABI backend is **feature-complete
C11 — the exec suite proves it.** The parser is one driver demonstrating the opcodes
span the language; the replay driver is a *second* driver of the *same* ops. It never
fakes or reimplements anything (no faux-stack, no abstract machine) — reaching `-O0`
parity is "use the tools it already has." **Every remaining coverage gap is therefore an
AST-*query* gap, not a codegen gap:** the op exists; the driver lacks the query to drive
it. Gaps are tiered by query cost (§18.2) — the three open items below are all Tier 2
(a local structural walk). **The actionable checklist — every query mapped to the exact
`src/mccgen.c` symbol that answers it today vs. the specific TODO — is docs/AST.md §18.5
(the single source of truth for "what's left").** In brief: three Tier-2 hooks close
`-O0` parity (VLA scope-edge capture, `I`-unit rodata-const reuse, nested landor chain);
Tier 3 (liveness→`gv` steering) is the first real `-O1` win; Tier 4 (inline + guards) is
the minimize-invoke payoff. **No item on the list is a new machine op.**

- [x] **Widen replay coverage to `-O0` parity (§16 Mid) — DONE 2026-07-08.** All actionable
  §18.5 Tier-2 query gaps are closed; the replay driver reproduces `-O0` for the checklist.
  ~436 function instances across the exec corpus now replay byte-identically (up from ~283).
  The residual long tail is diverse per-construct fallbacks (atomics, `__builtin_*` families,
  inline asm, `_Complex` specials, and the deliberately-deferred short-circuit-as-call-argument
  case — commit f50807dc, whose failure mode is a hard arm64/macos crash byte-verify can't
  catch, so it stays bailed) — each an isolated capture gap, not a blocker, and all fall back
  correctly. Detail of the 19+ milestones + the three Tier-2 closures below.
- [~] **This session (2026-07-08) landed 19 milestones**
  covering every major target and nearly the whole tail: floats/double, call-result stores,
  scalar struct member access (`.`/`->`), struct copy/deref, `switch` dispatch, named
  `goto`/labels, struct-return callees (`return s`), by-value struct args (`f(s)`), bit-field
  member access, struct-return callers **all ABI forms** (register-return, sret hidden-pointer,
  arch-transfer), `f().x` (member of an rvalue struct), **`_Complex` arithmetic +
  `__real__`/`__imag__` + casts**, and **short-circuit results used as values** (`int r=a&&b`,
  `(a&&b)+1`) — plus two latent correctness bugs fixed (vpop call double-emit; float const-pool
  duplication) and a switch-replay segfault guarded. Key enabler: an **ordinal frame-slot table**
  (`ast_alloc_loc`/`ast_locrec`, the `ast_fconst` pattern) wrapping the struct-result and
  complex temps so replay reserves the same offsets — `-O0` stays byte-identical (the record is
  passive). Fixtures under `ast/replay-*` (16 REPLAYED + fallback + template gates).
  **All three Tier-2 query gaps this list once carried are now LANDED (2026-07-08)** —
  nested short-circuit, `_Complex` construction, and VLA/`alloca` (details below). **Query-first
  framing (docs/AST.md §18) held: the vstack/ABI backend is feature-complete C11 — `-O0`
  already compiled all three correctly — so none was a codegen gap; each was a driver-side
  query/capture gap over ops that already exist.** Two latent pre-existing bugs surfaced and
  were fixed on the way (a NULL-`sym` global-condition crash; an unreset `ast_last_return`).
  **VLA/`alloca` LANDED 2026-07-08.** The `gen_vla_sp_save`/`gen_vla_alloc`/`gen_vla_sp_restore`
  ops exist (`-O0` emits them); the missing pieces were pure capture/query, all local walks
  (docs/AST.md §4/§18.3): (1) the runtime **size computation** (`n*elemsize` → the length slot)
  is already an ordinary captured Store; (2) the machine-tier **alloc sequence** is captured as
  one coarse `Unary(AST_OP_VLA)` effect carrying the VLA type + the `addr`/`locorig` frame-slot
  offsets (relocation-free, rbp/rsp-relative, so replay re-issues it with the captured immediates
  and does **not** decrement `loc` — the frame size stays parse-final); (3) the paired **LIFO SP
  restore** at the scope edge — the *lexical-scope-edge query* — is captured by hooking
  `vla_restore`, annotating the `Return` node when it fires during a `return`'s `leave_scope`
  (emitted between the cast and `gfunc_return`) or emitting an `AST_OP_VLA_RESTORE` BB effect at
  a nested block's `}`. 1-D, N-D, char, multiple-same-scope, and nested-block VLAs all replay
  byte-identically (fixture `ast/replay-vla`). PE / bcheck `use_call` alloc forms desync (fall
  back). No new machine op — exactly the §18 prediction.
  **`_Complex` construction `re + im*I` LANDED 2026-07-08.** The imaginary-*literal* form
  `r + 2.0i` already replayed via a coarse `Unary(AST_OP_IMAG)` node (fixture
  `ast/replay-complex_imag`); the `__builtin_complex`-based `I` unit now replays too
  (fixture `ast/replay-complex_ctor`). Two mechanisms: (1) `ast_hook_builtin_complex_begin/
  end` bracket `__builtin_complex(re,im)` — the two scalar-const arg pushes + the
  init_putv/section_add would desync the mirror, so they are suppressed (`ast_in_op`) and the
  rodata-const *result* is captured as a single Ref leaf (its anon Sym persists across
  discard/replay, so re-pushing it re-creates the same reference — no ordinal table needed).
  (2) The float→double `_Complex` **widening cast** (`gen_complex_cast`, const path) and the
  scalar float pool now share one **ordinal rodata-symbol-reuse** helper trio
  (`ast_fconst_reuse`/`_record`/`_push_ref`, generalized from the inline `gv` float-pool
  code): the cast materializes a fresh rodata const on every emit pass, so replay recorded
  the build-time ELF symbol index and reuses it, keeping the relocation byte-identical. The
  earlier "link-errored (unresolved anon symbol)" note was the pre-generalization diagnosis;
  the reuse mechanism resolves it.
  **Nested short-circuit operands LANDED 2026-07-08** (`(a&&b)||c`): the inner
  Binary(&&/||) is already a captured node, so the outer chain just accepts it as a child
  and replay's `AST_Binary` case recurses (the inner short-circuit renders its own gvtst
  chain into a VT_CMP the outer chain then gvtst's — the parser's exact sequence). No
  "nested landor-chain query" was needed after all; the earlier "segfaults on deeper
  nesting (grep)" was **not** the nested operand — it was a **latent pre-existing bug** the
  bail happened to mask: a *bare global* used directly as an `if`/`while`/`for`/`do`
  condition (`if (g)`) was captured with `sym==0` (the eager push-hook fires inside
  `vsetc`, before `vpushsym` sets `->sym`) and never re-finalized at the condition site, so
  replay reconstructed a NULL `sym` and faulted in `gvtst`'s `load`. Fixed by finalizing the
  condition leaf (`ast_finalize_leaf(ast_vs[0], vtop)`) in the four condition hooks; a
  nested *ternary* operand (register value, not a VT_CMP chain) still bails. Fixture
  `ast/replay-short_circuit` now covers nested operands, nested short-circuit as a branch
  condition, and bare-global conditions.
  **Variadic struct returns now replay** (`ast/replay-struct_ret_variadic`) — the struct-return
  ABI is variadic-independent, so no special handling was needed. Detail per item below:
  - ~~struct-return callers~~ **LANDED 2026-07-08** (register-return form) — `struct r = f()`
    replays. The post-call register→temp reconstruction is reproduced in the Invoke replay,
    and the result temp uses an ordinal frame-slot table (`ast_alloc_loc`/`ast_locrec`, the
    `ast_fconst` pattern) so its offset matches the parse-build; `-O0` stays byte-identical
    (the record is passive). Fixture `ast/replay-struct_ret_caller`. **The sret
    hidden-pointer form (large structs, ret_nregs==0) now replays too** (fixture
    `ast/replay-struct_ret_sret`): `ast_alloc_loc` wraps *both* the register-return temp and
    the sret temp, so replay reserves the same ordinal slot and re-pushes the captured result.
    Took three attempts — the frame-slot `loc` reuse must be coordinated across both sites, and
    the bug lived outside the byte-verified body (epilog/temp reservation), so the test suite
    (not byte-verify) caught the regressions. **The arch-transfer form (ret_nregs<0, mixed
    INT+SSE structs) now replays too** — same sret temp slot + `arch_transfer_ret_regs`; only
    **variadic struct returns** still bail. `f().x` (a struct-call result used directly as a
    member base) also replays — `ast_hook_member_end` threads the base's non-lvalue bit
    (VT_NONLVAL) through to replay. **Struct-return callers are now complete (all ABI forms).**
  - ~~bit-field member Store~~ **LANDED 2026-07-08** — the read-modify-write mask/shift
    (`adjust_bf`/`load_packed_bf` in `gv`, the mask/shift in `vstore`) runs inside the
    suppressed `gv`/`vstore`, so bit-field member access + `Store` + arithmetic all replay
    (member_end/vstore/genop now admit VT_BITFIELD lvalues/operands; feared crash never
    materialized — the shift/mask is fully suppressed). Signed + unsigned. Fixture
    `ast/replay-bitfield`.
  - **`_Complex` arithmetic + `__real__`/`__imag__` LANDED 2026-07-08** — a `_Complex` operand
    routes to `gen_complex_op` inside the suppressed `gen_op`, its result temp is an ordinal
    frame slot (`cplx_local` wraps `ast_alloc_loc`), and `__real__`/`__imag__` (`complex_part`)
    is captured via the same coarse member hook as `.`/`->`. Complex `+`/`-`/`*` and part
    extraction replay. **Complex casts** (real→complex, complex→complex) also replay now —
    `gen_complex_cast` runs suppressed under the Convert node (same pattern). Fixture
    `ast/replay-complex_arith`. Still falls back: **`_Complex` construction** (`re + im*I`, the
    imaginary-unit `I` constant / const-fold rodata) — a Tier-2 **rodata-const-symbol-reuse
    query** gap (docs/AST.md §18.3), not a codegen gap: reuse the `ast_fconst` ordinal-symbol
    pattern rather than capturing the rodata const as a bare Ref leaf.
  - **VLA/`alloca`** — **not a codegen gap** (docs/AST.md §18.3): the machine-tier
    `StackAlloc`/`StackSave`/`StackRestore` ops already exist and `-O0` emits them
    correctly. The gap is the **lexical-scope-edge query** — the driver needs to know
    *where* (which scope boundaries) to emit the paired save/restore for correct LIFO
    unwind. The scope nesting is in the CST/AST; the query is a local walk (§4/§18.2 Tier 2).
  - **short-circuit sub-cases LANDED 2026-07-08** — `int r = a&&b` (decl-init), `(a&&b)+1`
    (VT_CMP used arithmetically), and short-circuit in a ternary all replay: the VT_CMP→0/1
    materialization (setcc, in `vcheck_cmp`) is now suppressed during capture, so the
    `Binary(&&/||)` node stays and replay re-materializes it when the consuming op runs.
    Fixture `ast/replay-short_circuit`. Still falls back: **nested VT_CMP operands**
    (`(a&&b)||c`, bail in `ast_hook_landor_operand`) — a Tier-2 **nested-landor-chain query**
    gap (docs/AST.md §18.3): the driver needs the chain structure, not the flat gvtst
    reproduction (the flat model segfaults on deep nesting).

  _Baseline (predates this session's widening):_ ≥119/238 exec golden source files replay
  ≥1 function. Measured outcome buckets across the exec corpus (per function):
  ~283 replay, ~200 bail (unsupported construct), ~116 desync (mirror lost sync), ~89
  skip (struct/float/aggregate return via `ast_bad_type`), ~67 unfaithful (byte
  mismatch), ~39 empty. Landed: `for(;;)` loops (`If` op==5; `ast/replay-for_infinite`).
  **Landed 2026-07-08 — `float`/`double` (§A3 floats).** `ast_bad_type` now allows plain
  `float`/`double` (still bails `long double`/`_Complex`-pair `VT_QFLOAT`/struct/bitfield):
  fp arithmetic (`gen_opif`), int↔fp + fp-resize casts (`gen_cast`), fp comparisons, float
  params/returns, and float local-store chains all replay. The one blocker — a `float`/
  `double` constant is materialized by `gv()` into a fresh rodata slot + anon symbol, so
  replaying `gv` made a *second* slot and the reloc diverged → the parse-build now **records
  each const-pool symbol and replay reuses them ordinally** (`ast_fconst`/`ast_replaying`),
  keeping the relocation byte-identical. Fixture `ast/replay-float_ops`. Next targets by bail
  volume: none of the big buckets remain. **Landed 2026-07-08 — `goto`/labels.** A `label:`
  is a `Jump` op==4 marker (ival=token), a `goto` a `Jump` op==5 (both effects in their BB).
  Replay keeps a per-function label table (token → {jind, jnext}) reproducing the parser's
  forward-chain (`gjmp` onto jnext) / backward-jump (`gjmp_addr(jind)`) / definition-backpatch
  (`gsym`). Modeled for plain named gotos; VLA/cleanup-scope/computed-goto bail (their
  `pending_gotos`/`try_call_cleanup_goto` machinery is unmodeled). Forward + backward, out of
  nested loops, over blocks, multiple labels, and goto-from-a-switch-case all replay; fixture
  `ast/replay-goto_dispatch` (safety-net fixture now `ld_fallback`, `long double`).
  **Landed 2026-07-08 — `switch` dispatch.** A switch is an `If(op==6)` [value, bodyBB];
  `case`/`default` labels are `Jump` markers (op==2 [ival=v1,fbits=v2] / op==3) inside the
  body. Capture: `ast_hook_switch_begin` (before `sw->sv=*vtop--`, so the value leaf is
  finalized against the right vtop) + `_case`/`_default` + `_body_end` (suppresses the
  dispatch epilogue's vstack ops via `ast_in_call`) + `_end`. Replay rebuilds a `switch_t`,
  emits the value, jump-over, replays the body (markers record `cr->ind`/`def_sym`), then
  `case_sort` + `gcase` reproduce the binary-search dispatch (with the global `cur_switch`
  set so `case_cmp` sees signedness). **Safety:** the controlling value must be a reloadable
  leaf (Ref/Literal) — a computed value lives in a register the body clobbers, and since a
  fatal replay error is NOT caught by byte-verify (unlike a byte mismatch), computed-value
  switches bail (this fixed a segfault on grep's `switch(tolower(...))`). Fall-through,
  break, ranges (`case a ... b`), unsigned, and nested if/while/switch in cases all replay;
  fixture `ast/replay-switch_dispatch`. **Landed 2026-07-08 — scalar struct member access (`.`/`->`).**
  Member access does uncaptured in-place `vtop->type` retypes (to `char*` for the byte-offset
  add, back to the member type), so the fine-grained op tree can't be replayed (the offset
  would scale by `sizeof(base)`). Folded into one coarse `Unary(AST_OP_MEMBER[_ARROW])` node
  (ival=offset, type=member): `ast_hook_member_begin`/`_end` suppress the internal
  indir/gaddrof/vpushi/gen_op and replay reproduces the parser's exact sequence. `vpush` now
  admits struct/union **lvalue** leaves (a reconstructable frame/global address; bit-field/
  `long double`/`_Complex` still out), and `call_begin` guards by-value struct args so they
  fall back. Reads + writes, local `.` and pointer `->`, and struct-by-value *params* replay;
  fixture `ast/replay-struct_member`. **Landed 2026-07-08 — struct copy + deref.** Struct→struct
  `vstore` now records a `Store` (the aggregate memmove/`gen_struct_copy` runs with its internal
  ops suppressed by `ast_in_op`); replay reproduces the copy. The `indir` guard now allows
  deref-to-struct (a reconstructable lvalue, not a register value), so `*a = *b`, `q = *b`, and
  `(*p).x` replay. Fixture `ast/replay-struct_copy`. Still falling back (future work): struct
  **returns** (sret), by-value struct **args** (call-site guarded), bit-field member `Store`
  (shift/mask desugar), and the `InitList` node. Each construct lands with its `ast/replay-*`
  fixture and the whole-corpus `exec-replay` / `exec-replay-tmpl` columns staying green.
  _Node-level gap ledger (docs/AST.md §A3, feature-complete = the 15 kinds):_ the unbuilt
  kinds are **`InitList`** (aggregate/compound-literal init still reaches codegen as scalar
  `Store`s or a `memset`/`memcpy` `Invoke` — the §7 blob/`memset` grouping node does not
  exist yet) and TU-level **`TranslationUnit`** (per-fn arena only; needed for LTO/static
  localization, Long). `Store` lacks the bitfield shift/mask desugar + aggregate copy;
  `Invoke` lacks sret / by-value-struct args. **Landed 2026-07-08 — call-result stores:**
  `T x = f();` / `x = f();` (both initializer and assignment, `int`/`double` alike) now replay.
  Root cause was a latent double-emit bug: a `Store` leaves the RHS value on the mirror as the
  assignment's result, and `ast_hook_vpop` re-added that `Invoke`/`Unary` as a *bare* BB effect
  → replay emitted the call twice. Fixed by only re-adding when the node is still unparented
  (`ast_parent==AST_NONE`); a value already consumed by a `Store` is parented to it. Fixture
  `ast/replay-call_store`. `long double`/`_Complex` still desync. _Note the two orderings differ:_ by **bail volume**
  `switch`/`goto` lead, but by **combined payoff** floats/wider types led (now landed).
- **Reprioritized (2026-07-08, docs/AST.md §A1):** liveness-steered **register promotion**
  (mem2reg of address-not-taken locals) is the real -O1 payoff → moved to **early Mid**,
  right after the zero-template invariant is green. The broader template library beyond
  const-fold (algebraic, dead-branch, jump-table) is **demoted to Long** (structural
  rewrites are the smaller win). Long-horizon items (virtual always-inline over the shared
  store, cross-TU LTO, `-g` from provenance, hot-reload snapshots, **separate `-O2`/`-O3`
  replay drivers / SSA**) stay design in docs/AST.md §16 Mid/Long.

### AST — beyond `-O0`: the next milestones (design in docs/AST.md §16 Mid/Long)

With `-O0` replay parity complete, these are where `-O1` beats `-O0` — both purely
because the replay driver holds the whole per-function AST as a queryable object the
streaming parser never had. Neither is a new machine op (docs/AST.md §18.2).

- [~] **Tier 3 — liveness-steered register promotion (the real `-O1` payoff).** **LANDED
  2026-07-08 (incl. control flow)** — the first optimization that deliberately beats `-O0`:
  an address-not-taken integer local is kept in a pinned register (**R10/R9/R8**, x86_64) with
  **zero stack load/store traffic**. Opt-in `MCC_AST_PROMOTE`; a promoted function's bytes
  diverge from `-O0` by construction, so **byte-verify is bypassed for it and the exec-golden
  differential is the gate** — a whole-corpus `exec-replay-promote` column (281 programs, all
  green) plus fixture `ast/replay-promote` (`[ast-promote] N <fn>` proves it fired). A read
  pushes the value register-resident (gv copies it into a temp when consumed; the pin stays
  intact for later reads); a write forces the value into the pin; **the pin is seeded from the
  local's stack slot at function entry** (`ast_promo_entry_init`), which makes promotion valid
  across **arbitrary control flow (loops/if)** and for parameters / read-before-write locals —
  the register mirrors what `-O0` would read from the never-again-written slot. **Applies to:**
  any function with integer (`int`/`long`) **or pointer** locals — **call-free** uses caller-saved
  **R10/R9/R8** (no save/restore), **call-ful** uses callee-saved **RBX/R12–R15** (pushed at entry,
  popped at the return funnel; see the call-ful sub-item below, ON by default since 2026-07-08). A
  promoted pointer's *value* lives in the register; a deref `*p`/`p[i]` is an `AST_Load` that indirs
  the register into a memory base (needing high-register SIB/REX.B addressing in `load`/`store` —
  landed). **Poisoned:** floats/structs, `&`-taken / member-base locals (incl. `p->m`) and whole
  local array/struct slot ranges, `++`/`--` (lvalue) targets, and any function using **inline asm**
  (clobbers). Additive — no `gen_op` surgery.
  - [x] **Two-pass soundness gate — LANDED 2026-07-08.** Promotion now replays WITHOUT it and
    byte-verifies against `-O0` first (pass 1); only if faithful does it re-replay WITH promotion
    (pass 2), kept unconditionally. This closes a real (if narrow) unsoundness: forcing
    `faithful=1` for a promoted fn is wrong — byte-verify can't tell "diverged by promotion" from
    "diverged by a replay bug", so it kept broken replays. The pass-2 idempotency bug that stalled
    the first attempt was **`nocode_wanted`** (pass 1 ending in a `return` leaves it `>0`, so pass
    2 emitted an empty body → zeroed struct returns) — **not** GOT/`gen_gotpcrel` (that earlier
    diagnosis was wrong; `*ABS*` in `if.c` was a symptom of that function's *unfaithful* replay,
    which the gate now correctly excludes). Pass 2 also resets `ind`/`rsym`/body-relocs/`loc`/
    `anon_sym`/`ast_fconst_i`/`ast_locrec_i`/`nocode_wanted`. Whole `tests/exec` corpus green,
    ctest 1768/1768 (in both call-free and call-ful modes).
  - [x] **Broaden to call-ful functions via callee-saved RBX/R12–R15 — LANDED 2026-07-08, ON by
    default (the big win).** A call-free fn's caller-saved pins die across calls; a callee-saved reg
    (mcc never allocates RBX/R12–R15; the ABI has the callee preserve them) survives, so the fn
    push/pops them at entry / at the single return funnel with an even-count alignment pad. Promotion
    (`MCC_AST_PROMOTE`) now promotes across calls by default; `MCC_AST_NO_CALLFUL` is an escape hatch
    that restricts it to the call-free pool. **Whole `tests/exec` corpus green in both modes; ctest
    1768/1768.** A 5-pin stress fn (`[ast-promote] 5`) round-trips across a loop + calls, so the
    R14/R15 push/pop/`load` REX encoding is correct. Three bugs closed to get here, all *faithful*
    call-ful miscompiles the two-pass byte-verify could not catch (promoted bytes diverge from `-O0`
    by construction):
    - **Aggregate-element aliasing** (`unary_operators`, `type_coercion` structs): a constant-index
      **array element** / **struct member** is captured as a plain `int` local Ref at the member's
      frame offset and promoted — but the aggregate is also read from **memory** when its address is
      formed (`int *q=&a[1]`, `a+1` is pointer arithmetic with **no `AST_OP_ADDR` node** to catch).
      Fixed by poisoning the whole `[base, base+sizeof]` slot range of any local array/struct Ref.
    - **Widening sign-extension** (`conversions_semantics`): `long lo = sh;` (short→long) is a memory
      store that applies the implicit assign-cast to the slot's width; the register write skipped it,
      leaving a 32-bit `mov %eax,%ebx` (zero-extended) instead of 64-bit sign-extended. Fixed by
      `gen_cast`-ing the value to the target local's `CType` (type **and** ref, so enums don't crash)
      inside `ast_promo_write`.
    - **Recycled Sym pointer across the two passes** (`type_coercion`): the AST captures rodata
      leaves (string literals) as raw `Sym*`; such a ref sym can already sit on `sym_free_first`
      (freed after the parse-build used it), and replay's own `sym_push` (float/`_Complex` const-pool
      reuse in `ast_fconst_push_ref`) recycled that slot — harmless for one byte-verified pass, but
      pass 2 re-read the corrupted pointer and emitted a relocation against the wrong symbol (a
      format string resolved to a float const → empty `printf`). Fixed by hiding `sym_free_first`
      (set to NULL) for the duration of replay so every replay allocation is fresh, restoring it
      after both passes. VLA+call-ful still bails (rsp race).
  - [x] **Promote pointer locals — LANDED 2026-07-08.** A pointer's value (address) is promoted
    like an int; `*p`/`p[i]` derefs the register (an `AST_Load`, not an lvalue use of `p`). Needed a
    real backend fix: `gen_modrm_impl` now emits the SIB byte for an r12 base (low bits `100`) and a
    forced disp8 for r13 (`101`), and `store()`'s 32-bit/byte paths take REX.B from the destination
    base — the normal allocator never bases off r12-r15, so this was an unexercised encoding gap;
    byte-identical for every register it does use (ctest 1768/1768, exec-replay byte-verify green).
    A pointer used as a `p->m` base stays **poisoned** (a deref `*p`/`p[i]` is fine): the member
    lowering folds the byte-offset with `gen_op('+', const)` = `add $off, %base` in place, which
    corrupts the pin (verified: average.c `s->average`/`s->count++`). Displacement folding is NOT a
    drop-in fix — `gen_modrm_impl`'s plain-register-base path deliberately ignores a nonzero `c`
    (`-O0` pre-adds member offsets into the base and leaves stale addend metadata; emitting it as a
    disp breaks `-O0`, verified). A real fix needs a distinct "register base + live displacement"
    addressing form. Deferred — marginal coverage for real backend surgery.
  - [x] **Spill weighting (access-frequency × loop-depth) — LANDED 2026-07-08.** When candidates
    exceed the pin pool, `ast_plan_promotion` promotes the highest-weighted locals first (selection-
    sort) rather than first-seen — the scarce pins go to the hottest slots. The weight is a tree
    walk (`ast_promo_weigh`): each reference contributes `2^loop-depth`, where a loop is an `If`
    node with `op==2`, so an inner-loop local (verified: nested-loop `x,y,z,w` win R12–R15) outranks
    an outer-loop or straight-line one. Any valid subset is correct, so exec output is unchanged.
  - [x] **Promote float/double locals — LANDED 2026-07-08.** A `float`/`double` local pins into
    **XMM6/XMM7** (`reg_classes` = `RC_XMM6`/`RC_XMM7` with no `RC_FLOAT`, so `get_reg` never
    allocates them — free to pin, no backend extension needed). All XMM are caller-saved on SysV, so
    float promotion is **call-free only** (`xmm_max = has_call ? 0 : 2`); no save/restore. Two
    independent pools (GP + XMM) in one weight-ordered pass, so a function mixes int/pointer (GP) and
    float (XMM) pins. The existing `ast_promo_write`/`entry_init` already dispatch on
    `reg_classes[reg]`, so `gv(RC_XMM6/7)` materializes/seeds the pin; a read copies it to a scratch
    XMM (XMM6/7 aren't in `RC_FLOAT`). Verified: `s += a[i]`/`t *= 1.5` stay in `addsd`/`mulsd` on
    XMM6/7; `float` and `double`; call-ful correctly excludes floats. Corpus green, ctest 1768/1768.
  - [ ] share one spill slot across disjoint live ranges (needs a real backward-liveness pass);
    other arches (GP/XMM pools are x86_64-specific).
- [ ] **Tier 4 — virtual always-inline over the shared store (the minimize-invoke payoff).**
  Inline internal calls instead of emitting a boundary `Call`, using the CST's content-
  addressed store/binding/render engine (docs/AST.md §9). Cycle detection via the instance
  hash; guard queries (`setjmp`/signal/VLA regions → non-inlinable-across, §18.4). Requires
  defer-to-TU (§13) and store factoring (the first virtual-inline render is the validated
  second user of the shared engine).
- [ ] **Long horizon (design only):** the broader template library (algebraic, dead-branch,
  jump-table), the time-budgeted engine (§12/§221), dependency-ordered `-O1` compile, cross-TU
  LTO, `-g` from provenance, hot-reload snapshots, and separate `-O2`/`-O3` (SSA) drivers.

## AST — decided-with-revisit-trigger backlog (docs/AST.md "Revisit triggers")

Each is a closed decision; the item is the named condition that would reopen it.

- [ ] **Verify the CST answers every `-g` and LSP question without friction/contention**
  (does the CST's lexical-scope spans + source ranges cover debugger scope queries, hover
  types, go-to-def, live ranges) — if a gap surfaces, that reopens the dissolved `Bind`
  marker (docs/AST.md §B1). Until then `Bind` stays fully dissolved into liveness.
- [ ] **Acceptance bar (§C1):** the `-O1` replay driver is "done" only when every
  `tests/exec` golden is green under the `-O1-replay` column **and** GCC's own test suite
  passes under both `mcc -O0` and `mcc -O1`. Wire up the GCC-suite run as an AST gate.
- [ ] **`k` value:** raise the always-inline depth `k` above the `k=1`/widen-on-back-edge
  default only under `-O2`/`-O3` or an explicit size budget (`k≈log_b(budget)`).
- [ ] **Size-gated outline:** land as a later binding-graph template (swap an inline binding
  for a `Call` to a materialized standalone when rendered-size × site-count > budget);
  v1 stays strict always-inline.
- [ ] **Store factoring:** pull `store`/`binding`/`render` into the structure-agnostic
  engine at the *first virtual-inline render* — the shared-storage mechanism for the
  `CONFIG_CST || CONFIG_AST` overlap; neither subsystem ever depends on the other.
- [ ] **Template DSL / data-driven registry:** revisit a declarative pattern DSL past ~30
  templates; keep the function-pointer registry interface uniform so it can go data-driven.
- [ ] **Per-function `-O1` mode:** consider only if `-O1`'s multi-pass (defer-to-TU) compile
  latency becomes a complaint.
- [ ] **PP-as-executable-C (JIT):** parked; promoted-not-necessitated by the
  include-permutation analysis.

---

# Now

- [~] Normalize as much of the CMake code as possible: 1) minimize gating instead preferring autodetecting the existence of tools and enabling as many tests/targets/configs as are available on the host, 2) reduce CMake usage by relying on `tools` where advantageous, 3) fold in separate .cmake files into CMakeLists.txt.
  _Assessment (2026-07-07):_ **(3) largely moot** — there are no external `.cmake`
  *modules* to fold: `CMakeLists.txt` is already monolithic, the only `include()`s
  are the optional `config-extra.cmake` + standard CMake modules (ExternalProject,
  GNUInstallDirs, …), and the `run_*_fetch.cmake` helpers are already generated
  inline via `file(WRITE …)`; the `tests/*.cmake` are `-P` driver scripts that run
  in a fresh process and *cannot* be folded. **(1) substantially in place** — 25
  `find_program()` autodetections drive the toolchain/reference-cc/emulator
  discovery, and `ci local`/the presets enable what the host supports. **(2)** the
  `tools/` family (`build.c`, `ckbuildmd.c`, `hostgate.c`, and this session's
  `ckconfig.c`) already offloads config-emission + invariant checks from CMake.
  What remains is an open-ended "as much as possible" polish with real
  CI-breakage risk across the ~35 presets/platforms not testable from one Linux
  host — pursue incrementally with a specific, verifiable target, not as a sweep.
- [x] **Regenerate the Windows "all green" counts + add a divergence check
  (both hosts now cite the same per-case basis).**
  The Linux side was regenerated earlier (2026-07-07: `debug` = 1447 registered /
  1279 run / 168 env-gated skips). **Windows now regenerated (2026-07-08,
  main@ab7f5ee9) from an actual `ctest` run on this host** — every native preset
  registers **~1696** per-case tests, **0 fail**: `debug`/`ast`/`cst` =
  1515 run / 181 skip (1696), `cross` = 1517 / 179, `release` = 1494 / 202,
  `diagnostics` = 1516 / 181 (1697, +`sanitize-smoke`), `sanitize` = 1516 / 181
  (1697), `sanitize-msvc` = 1495 / 200 (1695), `msvc` (cl-built) = 1515 / 179
  (1694). The prior 812/810 figures predated the `CONFIG_AST`
  `exec-replay`/`exec-replay-tmpl` columns and the 1415 basis predated
  `exec-replay-promote` (the whole-corpus Tier-3 promotion column `d6740df9` added,
  the ~281-case jump); the regenerated Windows basis now matches the Linux one
  (per-case, replay columns included). `msvc` runs the same 1515 cases as `debug`
  (all pass); it registers two fewer only because the two macOS-only `macho-*`
  cross cases aren't emitted under the VS generator.
  `docs/NOTES.md` "Windows status" cites these figures. _Divergence check —
  decided NOT to add the strict count-checker_ (the `tools/ckbuildmd.c`-style grep
  of NOTES.md vs `ctest -N`): the registered total is documented to **track
  upstream test additions**, so a hard drift-fail would break CI on every
  legitimate new test; the real divergence risk was the two hosts citing different
  *bases*, which the regeneration resolves.
- [~] **Validate the remaining i386 TLS large-address pattern assumptions
  (x86_64 32[S] done; i386 TLS residual needs i386 cross + sysroot).**
  x86_64 GD/LD/IE/LE is covered by the `tls-models` ctest (`tests/tls/`, links
  gcc/clang objects in all four models, dynamic + static) — that push fixed real bugs
  (TLSGD→LE used only the symbol's own section size for the TP offset; static GD/LD
  links failed on `__tls_get_addr`). The `R_X86_64_32[S] out of range` check
  (`x86_64-link.c`) now has a positive test — `cli/x86_64_reloc_32s_range`
  (`cpu=x86_64,os=linux,asm`) forces a >2 GB layout with two ~1.5 GB `.bss` arrays and
  an absolute `movl $b+…` past +2 GB, asserting the diagnostic fires. **STILL OPEN:**
  the i386 `R_386_TLS_GD/LDM` pattern paths (`i386-link.c`) need an i386 cross build +
  32-bit sysroot to exercise (not available in this env). → Add the i386 TLS gate under
  the cross preset when an i386 runtime is available.

- [x] **Investigate the macOS CI benchmark reporting gcc "n/a" for the
  full-language config — DONE 2026-07-08.** Two independent root causes, both
  hit only on macOS/arm64 and both now fixed: **(1) the Apple-clang-`gcc`-shim
  distinction.** `tools/bench.c` detected `/usr/bin/gcc` (the Apple clang shim)
  as its "gcc" column and fed it `-DCC_NAME=CC_gcc`; `full_language.c` guards its
  K&R `old_style_f((void*)1,…)` path with `#if CC_NAME == CC_clang`, so
  Apple-clang-told-it-is-gcc took the non-clang path and modern clang rejects the
  `int`-conversion as a hard *error* → n/a. Fixed by routing the bench "gcc"
  column through the genuine-GNU-gcc resolver `ts_resolve_reference_cc()` (skips
  clang/Apple-LLVM, requires "gcc"/"GCC"/"Free Software"; candidate list extended
  to `gcc-16…gcc-10`), mirroring CMake's `mcc_find_gnu_gcc()` — so `-DCC_NAME=CC_gcc`
  now goes to the real Homebrew gcc-16. **(2) a `full_language` feature genuine
  gcc-on-Darwin can't compile.** gcc-16 then ICE'd (`assemble_alias`, varasm.cc)
  on the `__attribute__((weak, alias(…)))` block in `tests/diff/parts/legacy_builtins.h`
  — alias is unsupported on Mach-O (that is *why* the block already excluded
  `__clang__`). Fixed by also excluding `__APPLE__` (defined by gcc, clang, and mcc
  on Darwin, but not mingw/Linux) on both the alias def and the paired
  `some_lib_func` print, keeping all Darwin compilers on the same output path
  (`some_lib_func=444`) — output-neutral for the mcctest differential. The
  full-language gcc column now reports a real number (obj size ≈ clang's).

---

# Skipped-test ungating audit (per triple)

For each triple below: **when the current host has enough access to build/run
*all* of that triple's tests**, rigorously re-evaluate every test it currently
`mcc_skip_test`s or self-skips (rc 77). For each skip, decide whether a
*legitimate* subset can actually run on this platform and ungate that subset
(narrow the gate, split the case, or drop the guard); leave genuinely
unsupportable cases skipped with the reason kept current. Don't ungate blind —
verify the ungated part passes on the real target, not just that it compiles.

Access status is from this host (x86_64 Linux; qemu-aarch64/arm/i386, wine,
mingw x86_64 + i686 present; no macOS SDK/osxcross, no i386-Linux sysroot, no
arm64 mingw). Re-probe before acting — availability changes per CI runner.

- [ ] **x86_64-linux (native).** _Access: full now._ Audit the host-native skips
  first — these have no emulation excuse: `cli-suite` native-only structural
  readelf/nm, `static-glibc-run` (no static glibc), `parts-suite`/`diff3-suite`
  (needs both gcc + clang), `mcctest` (GCC-compatible ref cc). Confirm each guard
  still reflects a real host gap vs a stale assumption.
- [ ] **i386-linux.** _Access: blocked — qemu-i386 present but no 32-bit Linux
  sysroot._ Ties into the open i386-TLS item in "Now". Ungate once a 32-bit
  sysroot is on the runner; until then the skips are legitimate.
- [ ] **aarch64-linux.** _Access: partial — qemu-aarch64 present, but qemu models
  x86-TSO so weak-memory atomics/bounds tests can't be faithfully validated
  (see [[arm64-native-ci-failures]])._ Ungate only the memory-model-independent
  subset under qemu; leave the ordering-sensitive cases for a native arm64 runner.
- [ ] **armv7-linux.** _Access: partial — qemu-arm present, same x86-TSO caveat._
  Same split as aarch64: ungate memory-model-independent tests under qemu, defer
  ordering-sensitive ones to native hardware.
- [ ] **x86_64-windows (mingw + wine).** _Access: available — x86_64-w64-mingw32
  + wine present._ Re-evaluate the `pe-wine-conformance` / `mcctest` PE skips: how
  much of the full_language differential runs under wine byte-identically, and
  which cases are genuinely wine-limited vs conservatively gated.
- [ ] **i386-windows (mingw + wine).** _Access: available — i686-w64-mingw32 +
  wine present._ Same audit as x86_64-windows for the 32-bit PE path.
- [ ] **arm64-windows.** _Access: blocked — no arm64 gcc/mingw reference; the only
  reference is emulated x86_64 mingw, which can't be byte-identical to native
  arm64 mcc (see the `mcctest` skip reason)._ Leave `mcctest`/`mcctest-bcheck`
  skipped until a native arm64-Windows reference cc exists; codegen stays covered
  by exec/* goldens + pe-native-conformance.
- [x] **x86_64-Darwin — AUDITED 2026-07-08 on a native arm64 macOS host (SDK +
  full cross toolchain present).** Re-evaluated every rc-77 skip on the `macos`
  and `macos-cross` presets; all remaining Darwin skips are legitimate, none
  spurious: the **2-slice universal / `macho-structural` / `dash-s-bytes-*`**
  differential skips are cross-toolchain-gated and **do run** under `macos-cross`
  (which builds `mcc-x86_64-osx` etc.); the loader-based **`macho-codegen-run` /
  `macho-image-run` / `macho-apple-libc`** self-skip because their oracle
  (`tests/qemu/macho/loader.c`) is Linux-only (`<linux/seccomp.h>`, `REG_RAX`,
  `uc_mcontext.gregs`) — their native equivalent, **`macho-conformance-native`**
  (real libSystem exec) + `macho-structural` + `macho-universal`, already runs on
  the Mac; the rest are arch- (x86/i386/riscv/win32) or config- (bcheck/static-glibc)
  gated. **Fixed a real red surfaced by the audit:** the just-landed Tier-3
  `ast/replay-promote` fixture was registered unconditionally but promotion v1 is
  x86_64-only (pins R11/R10/R9/R8, `#if …MCC_TARGET_X86_64`), so it failed on
  arm64 — now gated to `MCC_CPU==x86_64` with a self-skip (coverage on other
  targets stays via the output-identical `exec-replay-promote` corpus).
- [x] **arm64-Darwin — AUDITED 2026-07-08 (native arm64 macOS runner).** Same
  skip audit as x86_64-Darwin (all legitimate); the `ast/replay-promote`
  arch-gate fix applies here too. **External `__thread` Mach-O limitation
  revisited on the real runner:** the hard-error (`Mach-O: external thread-local
  '<x>' is unsupported`, `src/objfmt/mccmacho.c`) still fires correctly at link
  time for a cross-module `extern __thread` reference; locally-defined `__thread`
  works (TLV descriptors). Lifting it needs TLV *import* descriptors + GOT-indirect
  loads in both arm64/x86_64 codegen and the Mach-O writer — a non-trivial change
  to a currently-green subsystem, and the repo gates it behind "a real
  cross-module-TLS-on-Darwin need appears" (docs/NOTES.md), which has not. Decision
  stands; the limitation is now **pinned** by a negative test
  (`cli/macho_extern_tls_unsupported`, `os=Darwin`) so it cannot silently regress.

## macOS CI observations (from the 2026-07-08 `macos-15-arm64` CI run)

The GitHub macOS runners are now **Apple Silicon (`macos-15-arm64`)**. Native coverage
is healthy — `macho-conformance-native`, `macho-stack-protector`, and `macho-universal`
(the `machofat` fat-binary tool, shelling to `xcrun --show-sdk-path`) all **pass**
natively on arm64. Remaining gaps to close:

- [x] **`ast/replay-promote` failed on every non-x86_64 target** (arm64 Linux + macOS
  arm64): the fixture asserted `PROMOTES=calc`, but Tier-3 register promotion is
  x86_64-only, so the assertion can never hold elsewhere. **Fixed 2026-07-08** — the
  fixture is now registered only when `MCC_CPU STREQUAL "x86_64"` (the whole-corpus
  `exec-replay-promote` column still runs everywhere as a no-op). _This was a
  self-inflicted regression from the Tier-3 v1 commit._
- [ ] **The four x86_64-targeting `macho-{structural,codegen-run,image-run,apple-libc}`
  drivers self-skip on arm64 macOS** (`_have_osx_cross` is false: the `macos` preset
  builds no cross compilers, and `host_is_x86_64()` is false). On an arm64 host these
  would need the **`x86_64-osx` cross** (they'd run x86_64 Mach-O under Rosetta 2) — so
  either build it on the arm64 macOS runner (via the `macos-cross` preset) or add
  **arm64-native** structural/codegen/image/apple-libc drivers so arm64 macOS gets that
  coverage directly. (Native arm64 Mach-O is already exercised by
  `macho-conformance-native`.)
- [ ] **`exec/backtrace` (+ its `exec-replay`/`-tmpl`/`-promote` and `diff3` columns)
  skips on macOS.** The golden output is ELF/Linux-specific (exact BCHECK/backtrace
  formatting + addresses); decide whether to validate a Darwin-specific formatted
  backtrace/bcheck golden or keep it documented as ELF-only.

## Tier-3 register-promotion correctness (x86_64)

- [x] **Global-access miscompile fixed + promotion broadened to control flow (2026-07-08).**
  The v1 pin pool `{R11,R10,R9,R8}` was unsafe: **R11** backs `load`'s scratch and the
  GOTPCREL/TLS addressing, so `int g; int f(int v){int x=v+1; g=x; return x+40;}` returned
  16, not 42. Fixed by **excluding R11** — the pool is now `{R10,R9,R8}`, all provably safe
  in a *call-free* function (R10 is used nowhere in the x86_64 backend; R8/R9 only in the
  call-argument register arrays). Same change let the **control-flow extension** land: the
  pinned register is seeded from the local's stack slot at function entry (`ast_promo_entry_
  init`), so promotion is valid across arbitrary control flow (loops/if) and for parameters/
  read-before-write locals — the single-BB and def-before-use restrictions are gone. New
  poisons: **inline asm** (clobbers) and **`++`/`--`** (lvalue) targets. Coverage rose from 3
  to **89** promoted functions across the exec corpus (all green under `exec-replay-promote`);
  fixture `ast/replay-promote` now exercises a loop. Follow-on (more registers, needs
  prologue/epilogue save/restore): pin **callee-saved** RBX/R12–R15 too.

---

# C99/C11 test-coverage backlog (from docs/TESTS.md)

Each item ports/mirrors a specific gcc/clang conformance test into an mcc test —
runtime cases go in `tests/exec/features_c99_c11/`, diagnostics/negatives in
`tests/diff/parts/` (or a new reject corpus). Reference paths are relative to
`~/Projects/gcc/gcc/testsuite` (gcc) and `~/Projects/llvm-project/clang/test/C`
(clang). Context + gap matrix: docs/TESTS.md §5–§6. Landed C99/C11 additions are
recorded in docs/NOTES.md ("Landed: C99/C11 test-coverage additions & fixes").

## Real semantic/diagnostic gaps — fix mcc, then add the test


## Coverage-depth gaps — mcc passes but under-tests vs gcc/clang; add tests

- [~] **UCN-in-identifier breadth.** Basic runtime is covered by
  `tests/exec/lexical/ucn_identifiers.c` (`\u`/`\U`, raw UTF-8, raw≡escaped); the
  **invalid-UCN rejection** cases (§6.4.3) are now pinned by
  `cli/c11_ucn_basic_latin_reject` (`A` in an identifier) and
  `cli/c11_ucn_surrogate_reject` (`\uD800`), both matching gcc/clang rc=1.
  *Remaining = the smaller tail: UCN in different token positions and
  normalization/UAX#31 breadth.* _Ref:_ gcc `gcc.dg/ucnid-*.c`; clang
  `C99/n717.c` (UCN grammar), `C11/n1518.c` (UAX#31).
- [~] **Negative/diagnostic test tier.** _Established_ in `tests/cli/cases.h`
  (grep-the-message pattern): `c99_fam_not_last`, `c11_alignas_underalign`,
  `c99_vla_goto_into_scope`, `c99_vla_switch_into_scope`, `c11_noreturn_returns`,
  plus this session's `c99_kr_implicit_int`, `c99_inline_no_extern_def`,
  `c11_ucn_basic_latin_reject`, `c11_ucn_surrogate_reject`, and
  `c11_signed_unsigned_reject` (type-specifier `signed`+`unsigned`; the "too many
  basic types" excess is already in `errors_and_warnings.c`). *Remaining
  (continuous): broaden toward the ~70% of gcc's C99/C11 files that are `dg-error`
  negatives* — the highest-volume seed is gcc `gcc.dg/c99-typespec-1.c` (1055
  dg-error over every type-specifier combo), plus `c11-align-3.c` and the
  `c99-flex-array-*` / `c11-*` negative files.

---

ACHTUNG!!! DO NOT DO!!! WARNING!!!

* Use only human friendly warnings/errors, backed by tests that check formatted output against terminal dimensions/configuration
* Implement/finish `-g` debugging/debugger and flesh out gdb/etc test cases, check against gcc and clang sources of truth
* Optimization -O1...100 levels measured in max seconds to spend optimizing?
* Hot reload by saving/loading CST snapshots on the fly and on run with --hotreload arg
* Run hot-reloads from reconciled CST snapshots

ACHTUNG!!! DO NOT DO!!! WARNING!!!

---
