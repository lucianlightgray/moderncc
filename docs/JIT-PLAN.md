# Next milestone plan — runtime JIT + guarded deopt (§26)

The `docs/AST.md` staged rollout (steps 1–5) is complete: the side-car substrate, the
strategy engine (sole pipeline), and the live -O4+ search with a fork worker pool and a
warmed memo are all on `main`. This is the **next** milestone — the design's separate
"+ the JIT" work (AST.md "Runtime deoptimization (JIT, -O4+)"), which the rollout
explicitly deferred behind the §26 embedded recompiler.

## What already exists (reuse), what's missing (build)

Reusable, per the JIT-infra survey:
- **Compile-to-executable-memory + relocate** — `-run` mode (`mcc_run`, `mccrun.c:121`)
  builds code into RWX or W^X dual-mapped memory (`host_runmem_alloc`, `mcchost.c:1085`)
  and relocates it (`mcc_relocate`, `mccrun.c:80`). Host==target on ELF
  x86_64/arm64/riscv64/arm/i386, PE, macho.
- **Indirection** — GOT/PLT (`build_got_entries`/`put_got_entry`, `mccelf.c`); a per-
  function GOT-style slot is the natural hot-swap redirect.
- **Startup hook** — `.init_array` ctor emission (ELF + macho) for a startup pool.
- **Replayable AST** — `ast_cur` survives the function when `keep_inline`/`keep_reemit`
  (`mccast.c:7633`); the search already produces optimized variants from it.

Missing (the §26 gap):
- The **byte-faithful baseline** (`orig`/`orig_rel`) is `mcc_free`'d per function
  (`mccast.c:7630`) — must be **retained** to be the deopt fallback.
- Intra-module calls are **hard `E8 rel32`** (`x86_64-gen.c:588`) — no swappable slot;
  the JIT'd function must instead be an **entry dispatcher** so call sites need no change.
- `--jit-functions` / `--jit-max-duration` are **parsed but inert** (`libmcc.c:2148`);
  `--embed-jit` only prints a manifest and gates the *build-time* superopt.
- `--jit-threads` and C11 `<threads.h>` **do not exist** (concurrency is `fork`).
- The **`eval_slice`** value-level equivalence checker **does not exist**.

## Architecture: the JIT is mostly Strategy objects, not a separate subsystem

Per AST.md ("one code path and one memo; the opt level is a dial on it, not a fork in
it"), the JIT's compile-time pieces are `Strategy` objects in the same `ast_strategies[]`
table the search already consumes — scored by the same model. Only a thin runtime
remains. The split:

- **New strategies** (transforms over the AST, searched/scored like any pass):
  - `jit-dispatch` — rewrite a function to `{guard; call variant-ptr else baseline}`.
  - `jit-guard` — insert the live-in domain check at entry.
  - `jit-profile` — insert live-in range-capture instrumentation.
  - `jit-patchpoint` — emit a nop-padded patchable prologue (the code-patch hot-swap
    variant is a strategy, not a bespoke mechanism).
  - the optimized variant itself is just the existing fold strategies applied.
- **Thin runtime** (not expressible as a compile-time transform):
  - recompile = **re-invoke the strategy engine at runtime** (D2A) on a hot function;
  - hot-swap = one atomic pointer store;
  - trigger = `.init_array` ctor or a `jit-profile` counter threshold.
- **Orthogonal**: `eval_slice` is the per-strategy soundness gate, not a transform.

**Payoff:** a *first complete version needs no runtime recompiler* — `jit-dispatch` +
`jit-guard` can emit `{guard; AOT-optimized-variant else baseline}` entirely at compile
time, giving real guarded deopt validatable by a differential immediately. §26 then
shrinks from "the prerequisite" to a stage-2 driver that re-runs the embedded engine for
hotter variants + swaps the pointer.

## Staged approach (each stage independently gated by a JIT differential)

1. **Guarded deopt as pure strategies — NO runtime recompiler.** Add `jit-dispatch` and
   `jit-guard` to `ast_strategies[]`. Together they emit a function as
   `{guard; call AOT-optimized-variant else call baseline}`, where the optimized variant
   is produced by the existing fold strategies and the baseline is the retained faithful
   emit (stop freeing `orig`/`orig_rel`, or re-emit from the kept `ast_cur`). This is real
   guarded deopt, entirely at compile time, gated by a differential — and it ships without
   §26.
2. **Runtime recompile (§26 driver, thin).** A minimal embedded runtime **re-invokes the
   strategy engine** (D2A) on a hot function into fresh executable memory via the
   `mcc_relocate` path, and atomically publishes the pointer the dispatcher reads.
3. **Profiling + trigger.** Add the `jit-profile` strategy (live-in range capture); drive
   recompilation from a startup `.init_array` ctor and/or a hot counter; wire
   `--jit-functions` (which functions get a dispatcher) and `--jit-max-duration` (budget).
4. **Soundness hardening.** `eval_slice` exhaustive equivalence over the guarded domain as
   the per-strategy gate; static `context_in` domain to replace the observed range.

## Decisions

Settled with the user: **D1=B** (embedded), **D2=A** (recompile = re-invoke the engine),
**D3=A** (entry dispatcher; the code-patch variant D3B is itself the `jit-patchpoint`
strategy), **D4=A** (runtime-observed live-in range). Remaining: **D5** trigger (startup
ctor vs hot counter — the counter is `jit-profile`), **D6** `eval_slice` now vs
trust-AOT-gate + differential, **D7** platform (ELF x86_64 first), **D8** wire the inert
`--jit-*` flags.
