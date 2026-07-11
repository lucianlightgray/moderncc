# tests/fuzz/corpus — graduated miscompile repros

Every `.c` here is a self-contained program that once exposed an **mcc**
divergence from the `gcc == clang` consensus on x86_64-Linux, auto-reduced by
`tests/fuzz/runner.c` (`--reduce`) and kept as a permanent regression lock.

The corpus-replay ctest (`fuzz/corpus`) rebuilds each file with mcc at
`-O0..-O3` and asserts it now matches the reference consensus. A file that
starts to diverge again fails the build.

## File conventions

- `repro_seed<N>.c` — a reduced miscompile repro. The leading comment records
  the generator seed and the `-O` level / `MCC_AST_*` gate the auto-triage
  attributed it to.
- `accept_<name>.c` — a program where mcc **intentionally** differs from
  gcc/clang (implementation-defined or a documented mcc extension). These are
  baselines, not bugs; replay reports them `INFO` and never fails on them.

## Graduating a new repro

1. A campaign run with `--corpus tests/fuzz/corpus` drops a reduced
   `repro_seed<N>.c` here automatically on a confirmed, UB-free divergence.
2. Inspect it. If it is a genuine mcc bug, keep the filename; the replay test
   then guards the fix.
3. If on review it is an accepted `[DIFF]` from gcc/clang (impl-defined), rename
   it `accept_<name>.c` and capture the rationale in `../NOTES.md`.
