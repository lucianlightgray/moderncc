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

## Staged approach (each stage independently gated by a JIT differential)

1. **Retain the baseline + a per-function variant table.** Stop freeing `orig`/`orig_rel`
   (or keep the AST); register each JIT-eligible function in a runtime table
   {name, baseline ptr, optimized ptr, guard}.
2. **Entry-guarded dispatcher.** Emit JIT'd functions as a small stub: check the guard,
   `jmp`/call the current best variant pointer (baseline initially). No call-site change.
3. **Runtime recompile.** Reuse the codegen + `mcc_relocate`-style pass to compile the
   optimized AST variant into fresh executable memory; atomically publish its pointer.
4. **Guard + deopt.** Bound the variant to a live-in domain; the guard checks it at entry;
   on failure the dispatcher falls back to the retained baseline.
5. **Trigger + budget.** Wire `--jit-functions` (which functions get a dispatcher) and
   `--jit-max-duration` (recompile budget); choose the trigger (startup vs profile).
6. **Soundness.** The variant comes from the already-faithful AOT search; add the runtime
   differential guard, and (later) the `eval_slice` exhaustive checker over the domain.

## Decisions to settle (see the table in the commit message / below)

The open ambiguities are D1–D8 below; the recommended path is the lowest-risk one that
reuses `-run` and defers `eval_slice`/threads.
