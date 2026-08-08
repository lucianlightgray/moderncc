# Device libc — census, feasibility, and a result that redirects the phase

> Measured 2026-08-08 against real recorded arenas. The headline was independently
> re-measured on a separate 5-file sample before being written down, because it
> contradicts the strategy it was commissioned to plan.

## The headline: a device libc is a latency lever, not a coverage lever

A device libc unblocks **4.4–4.7% of Invoke-blocked blocks**. Not 40%. The mass of
`AST_Invoke` is not libc at all — it is the compiler calling its own tiny accessors.

| corpus | Invoke-blocked blocks | unblocked by a *complete* device libc |
| --- | ---: | ---: |
| `tests/exec`, 60 files | 454 | **3 (0.66%)** |
| compiler, 15 sources | 16,537 | **734 (4.44%)** |
| compiler, 5 sources (independent re-measure) | 6,442 | **303 (4.70%)** |

That is the ceiling with *every* libc name on the device, not a first tranche.

Against the alternatives, over the same 16,537 blocks:

| capability | blocks unblocked | share |
| --- | ---: | ---: |
| device libc alone | 734 | 4.44% |
| **D4b (internal calls on device) alone** | **12,901** | **78.01%** |
| D4b + device libc | 14,672 | 88.72% |
| + D1d posted output | 15,464 | 93.51% |
| libc + posting, **no** D4b | 1,188 | 7.18% |

**There is no ordering in which building the libc first buys time on the call boundary**,
because the blocks it unlocks are almost disjoint from the mass, and the mass is
user-internal calls. Top blocking callees on the compiler's own source: `_mcc_error`,
`ast_child`, `ast_kind`, `next`, `ast_op`, `_mcc_warning`, `get_tok_str`.

**This does not contradict D2b, and both numbers are true.** `docs/PLAN.md` scores D2b at
77.8% of *dynamic crossings* removed by four functions; this is 4.4% of *static blocks*
unblocked. `memcpy`/`strcmp`/`memset`/`strlen` are called constantly from a small number
of sites, so they dominate crossings and barely move block coverage. The census confirms
D2b's own function ranking on static data too — `memcpy` 520 blocks, `strcmp` 518,
`memset` 434, `strlen` 185 — it just shows those functions buy far less *coverage* than
they buy *crossing reduction*. **State D2b as a latency lever and the two agree.**

It also strengthens D4b's justification rather than weakening it: the old argument was
"87.65% of invokes are internal", a property of invokes. The new one is "78.0% of
Invoke-blocked blocks need nothing but internal calls", a property of blocks — strictly
stronger, because it accounts for co-occurrence that the per-invoke figure ignores.

## Corpus choice is load-bearing

`tests/exec` is print-and-assert code: `printf` alone is 35% of its Invoke nodes. Its libc
ceiling is 3 blocks. The compiler's own source gives 734. **A and B disagree by 47×**, and
B is the one that matters. Any future measurement of this phase must use the compiler.

Raw Invoke class share, compiler corpus: **user-internal 90.0%**, pure `str*`/`mem*` 4.2%,
host I/O 3.6%, `snprintf` family 0.7%, allocator 0.4%, indirect 0.1%. The 90.0%
independently corroborates the plan's measured 87.65%.

## Feasibility buckets

**(a) Pure computation, buildable today — effectively empty.** Every `mem*`/`str*`
function takes a pointer, so none qualify. What is left is `abs`/`toupper`/`isdigit`-class
register-only work, whose measured value is zero — the host compiler inlines it and it
never crosses. **The device libc has no risk-free warm-up: its first real function
requires B1.**

**(b) Needs the B1 address space** — `memcpy`, `memset`, `memmove`, `memcmp`, `strlen`,
`strcmp`, `strncmp`, `strchr`, `strcpy`, `atoi`, `strtol`, `snprintf`. Under B1b a pointer
*is* a byte offset into the device buffer, so each becomes a bounded byte loop. This is
623 of the 734-block ceiling.

**(c) Needs a real allocator** — `malloc`, `calloc`, `realloc`, `free`. A bump pointer
covers `malloc`/`calloc`; **`free` (150 nodes) forces a real free list** and `realloc`
forces per-block size metadata. Combined marginal gain **+50 blocks (0.30%)** — the worst
value-per-risk item in the census. Do it last, if at all.

**(d) Host I/O, never a device function** — `printf`, `fprintf`, `fopen`, `read`, `write`,
`getenv`, `exit`, plus `setjmp`/`longjmp` and `pthread_*`.

**`printf` splits, and the split is worth more than the libc.** Its return is discarded at
**100% of measured sites** (80/80 compiler, 452/452 tests/exec) — exactly D1d's
asymmetric-posting precondition. So the *formatting* half is bucket (b) (it is `snprintf`
into a device buffer) and only byte emission is (d), posted to a host ring. Measured
effect on `tests/exec`: coverage 39.2% → **85.5%**.

## Two test-integrity hazards, found before any code was written

**1. The CPU reference has no `AST_Invoke` case at all.** `ast_eval_slice`
(`src/ast_eval_slice.h:440`) falls through for every invoke and returns 0 = undefined. So
a differential over any invoke-bearing arena is **vacuous, not merely weak** — it compares
nothing. The reference implementation must land in the *same commit* as the first emitter.

**2. `--mutate` would be blind to `memcpy`.** The mutation operator perturbs the **returned
value** (`tools/spvgate.c:654`, `:156`, applied at `:985`/`:1256`). But `memcpy` discards
its return at 462/462 sites and `memset` at 343/343. **A return-value mutation cannot be
observed by a memcpy test**, so the known-positive would pass while proving nothing. The
operator must move to the written memory — perturb one byte of the destination region.

This is the **fifth** instance of the "a cell that cannot fail" class in one session, after
`spvgate` reporting OK for a case that lowered nothing, the GPU differentials before a
mutation switch existed, `rir/drop-ratchet` reading the wrong report file, and
`gpu/ladder-gpu-parity` ignoring its subprocess exit status.

A third, smaller gap: `MAX_LIVE` is 4 (`tools/spvgate.c:112`) and `collect_lives` tracks
only scalar local `Ref`s. A `memcpy` differential's input and output are *frame regions*,
not four scalars, so the harness needs a frame-buffer comparison mode. That is the largest
single work item in the first slice — larger than the emitter.

## ABI

Recognition reuses `ast_slc_callee_sym` (`src/mccast.c:13628`) verbatim — do not write a
second callee resolver. Two guards, both load-bearing:

- Fire only when `ast_slc_invclass` (`src/mccast.c:13652`) returns **1 = opaque**. Return
  2 or 3 means the TU defines a body for that name, and **a user function called `memcpy`
  must go to the call boundary or the compiler miscompiles a legal program.**
- Require the Sym to have no body in this TU, for the same reason at a different
  granularity.

Arguments are children 1..n; pointers lower to a `uint` byte offset (B1b, no new concept).
**Return plumbing is free for the first tranche**: `memcpy`, `memset`, `memmove` and `free`
discard their result at 100% of sites, so the emitter yields a void value and the frame
emitter's existing statement handling absorbs it.

**One B1 invariant to write down before any allocator exists: offset 0 must be reserved as
NULL.** A bump allocator handing out offset 0 returns a pointer equal to NULL, and
`malloc`'s result is used at 66/66 sites, so the first thing the program does is null-check
it.

Memory the callee writes goes straight into the B1 buffer — host-visible coherent at
command-buffer granularity, no copy-back. That is also the hazard, and it is J3a′'s
argument again: a fault part-way through a device `memcpy` leaves a destination no correct
execution produces, so **device libc inherits whole-run abandon-and-restart and cannot use
region-granularity fallback.**

`memcpy`'s `restrict` contract is not checkable on device. Emit a forward byte loop for
`memcpy`, an explicit direction test for `memmove`, and do not replicate any host libc's
vectorised overlap behaviour — matching glibc's accidental semantics is not a goal and
chasing it would make the differential unstable.

## Sequencing, if the phase proceeds

**First: `memcpy`.** Highest marginal unblock in both real corpora (+249 compiler, +27),
fixed arity 3 at 462/462 sites so no varargs, return discarded at 462/462 so no return
plumbing, needs B1 and nothing else. It is the only candidate whose risk is purely "does
the byte loop over B1 offsets work", with no second unknown bundled in.

Then `memset` (+31, near-zero incremental risk), then `strcmp` + `strlen` together (they
introduce the return path and share a scan loop), then `memcmp` (+22, free once `strcmp`
exists). **`snprintf` is #2 by gain (+168) and last by sequencing** — it bundles varargs, a
`%` engine and 64-bit division, and putting it early means a failing differential cannot
tell you which of the three broke. **The allocator is last.**

## Residual after D4b + libc + posting

1,073 blocks on the compiler corpus: `getenv` 238, `fclose` 125, **indirect 102**, `close`
82, `__assert_fail` 75, `fopen` 67, `fread` 48. Almost all real syscalls — the B3b
population. Indirect calls are the only residual class that is neither I/O nor a known
syscall, and they have no device answer anywhere in the current plan.

## Caveats

- Two compiler sources exited nonzero under the dump flags, so their arenas are partial.
- The dump covers only bodies the RIR replay path modelled — the same "modelled
  population" caveat `tools/slice-census.py` carries.
- "D4b alone" treats every callee modelled anywhere in the dump as internal; across 13 TUs
  that slightly overstates internal-ness versus a strict per-TU D4b. Flagged as an
  estimate; the 90.0% raw user-internal share is the tighter cross-check.
