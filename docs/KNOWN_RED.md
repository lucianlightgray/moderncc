# KNOWN_RED — quarantined pre-existing test failures

Single source of truth for tests that are **chronically red for a known reason**
and must not be counted as a regression at VERIFY time (`INSTRUCTIONS.md` §8).
Replaces the scattered per-harness `KNOWN_RED=` strings (`tools/run-opt.sh`,
`tools/run-tier.sh`) and the in-file `XFAIL:` tags (`tools/xsuite.py`) — every
harness should read THIS file.

**Anti-vacuity rule (mandatory, ported from `tools/run-opt.sh`):** a quarantined
cell that **PASSES** fails the suite ("listed KNOWN_RED but passed — delist it").
The registry can never rot into permanent amnesty: the moment a red goes green,
this file must be updated or the build breaks. A `code` change that *fixes* a
quarantined cell removes its row in the same commit.

**Scope tokens:** `ALL` (every platform) · `linux` `macos` `windows` · an arch
(`x86_64` `arm64` `riscv64` `i386`) · or a compound `os:arch`. A cell red on only
some platforms lists exactly those.

## Format

```
<cell-id> | <harness> | <scope> | <reason> | REF <DETAILS-anchor> | since <YYYY-MM-DD>
```

`<harness>` ∈ `ctest | run-opt | run-tier | xsuite`. `<cell-id>` is the ctest
test name, the run-opt `cell:-Olevel` key, etc. Every row needs a `REF` to a
`DETAILS.md#anchor` explaining the root cause and the plan (or why it is WONTFIX).

## Registry

```
builtin_expect_is_code_neutral | ctest | arm64 | arm64-LP64 branch-consumed long-cast QoI: __builtin_expect object-size bloat vs plain if; gated cpu!=arm64 by T-lin-10453 | REF DETAILS.md#t-lin-10453 | since 2026-08-20
```

_(Seed row: the exemplar the process review named. Sessions add rows here instead
of re-deriving pre-existing reds by worktree-bisect; each addition cites the
DETAILS anchor that proved it pre-existing on a pristine worktree.)_
