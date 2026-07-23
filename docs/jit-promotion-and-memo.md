# JIT promotion benchmark & KGC memo internals

Rationale for the embed-JIT promotion/benchmark path and the KGC value memo,
kept here rather than inline (see `src/mccjit_embed.c`). Each section names the
function it documents.

## KGC value memo (compressed sorted array of observed values)

The KGC (known-good-config) memo is a lexicographically-sorted array of the
argument-value tuples a JIT'd variant has been verified against (variant output
== baseline output for that tuple).

### `mccjit_kgc_encode_compressed`
Serialize the sorted KGC memo as a compressed byte stream: per-column delta
against the previous tuple, zig-zag mapped, LEB128 varint packed. Sorted input
makes the leading-column deltas small and non-negative, so the array shrinks
hard (~8x on real runs). Returns a malloc'd buffer in `*out` (caller frees) and
its length in `*outlen`.

### `mccjit_kgc_decode_compressed`
Inverse of `mccjit_kgc_encode_compressed`: reconstruct the sorted tuple array.
Fills up to `maxtuples` rows into `dst` and returns the decoded tuple count, or
-1 on a malformed/oversized stream. Used by the round-trip self-check
(`MCC_JIT_KGC_SELFTEST`).

### `mccjit_kgc_flush_compressed`
Save the sorted memo as a compressed array and account it in stats. Always runs
when stats are active (feeds the JIT "memo" line); additionally writes the
compressed blob to `$MCC_JIT_KGC_SAVE/kgc-<salt>-<arity>-<seq>.z` when that
directory is set, so the observed value set persists across runs.

### KGC guard registry (`mccjit_kgc_register` / `mccjit_kgc_unregister` / `mccjit_kgc_flush_all`)
Persistent per-variant KGC guards live for the whole process and are never
closed, so their observed-value arrays are flushed at exit — ordered before the
`atexit`'d stats print, which is registered earlier at JIT boot. Closed KGCs
unregister themselves, so each array is flushed exactly once.

`mccjit_kgc_flush_all` flushes every still-live guard once. It is invoked both as
the stats pre-finish hook (`mcc_stats_set_flush_hook`, so the memo line is
populated before the block is painted) and via `atexit` (so a stats-off, save-on
run still persists) — the per-node `flushed` flag makes whichever fires second a
no-op.

## Promotion benchmark (interleaved race, multi-core consensus)

### `mccjit_bench_run_pair`
Benchmarks the candidate and the incumbent alongside each other: within every
rep the two functions are invoked back-to-back over the same tuple set and timed
separately (three `CLOCK_MONOTONIC` reads per rep, bookkeeping excluded from both
windows), so any CPU-frequency, cache, or scheduler drift over the run is shared
by both and cancels out of the ratio. Both accumulate the same deterministic
invocation count, so the work is reproducible. The DCE sink is accumulated into a
caller-supplied `sink_out` (per-sibling) rather than a shared global, so
concurrent siblings do not race on it.

### `mccjit_bench_sibling_run`
One benchmarking sibling: a best-of-rounds interleaved race of the two strategies
(candidate + incumbent), yielding this core's promote verdict.

### `mccjit_bench_pair` (K5/L4A/L5A promotion scorer)
The two strategies (candidate + incumbent) race as siblings under
`mccjit_bench_run_pair`; that race is run **redundantly on every available core**
(each core an independent best-of-rounds sibling over the same observed live-in
set) and the promote decision is the **majority consensus** of the per-core
verdicts, so one throttled or noisy core cannot flip the result. The incumbent
still wins ties (a non-majority — including an even split — stays put). Each core
runs the full deterministic invocation count, so the decision is reproducible;
the siblings run concurrently, so N-core redundancy costs ~one core's wall-time.

Core count comes from `MCC_JIT_BENCH_CORES`, else `sysconf(_SC_NPROCESSORS_ONLN)`,
capped at `MCCJIT_BENCH_MAXCORES` (16); `1` reproduces the previous single-core
behavior exactly. The benchmarked variants are KGC-routed (pure/deterministic —
the differential compares variant vs baseline), so concurrent invocation is safe;
the tuple set is a read-only stack snapshot.
