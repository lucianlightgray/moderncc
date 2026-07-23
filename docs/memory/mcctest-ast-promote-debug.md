---
name: mcctest-ast-promote-debug
description: "What mcctest tests, how AST-opt env gates work, and how to bisect a promote codegen bug"
metadata: 
  node_type: memory
  type: project
  originSessionId: 35124f51-27e7-4a2f-962e-39e727706c02
---

`mcctest` (ctest) is the whole-corpus **cc-vs-mcc self-host differential**:
`mccharness mcctest` compiles `tests/diff/full_language.c` with both a
reference gcc and mcc, runs both, and byte-compares stdout. It's the broadest
codegen gate; a mismatch = mcc miscompiled *something* in that ~1500-line
aggregate. It runs mcc at the **default, no-env** invocation.

The AST optimization stack (replay / const-fold templates / Tier-3 register
promotion / Tier-4 inline) is controlled by env gates read in
`mccgen_compile` via `ast_env_gate(name, dflt)`: **unset → dflt, "0" → off,
any other value → on**. As of the 2026-07-09 revert these default **off**
(opt-in); `-O1` re-engages replay + Tier-3 promotion (x86_64 only). So you can
toggle the whole stack per-invocation **without rebuilding mcc**:
`MCC_AST_REPLAY/PROMOTE/INLINE/TEMPLATES=0/1`.

Key trap: the per-golden `exec-replay-promote/*` ctest columns pin
`MCC_AST_PROMOTE=1` explicitly and can be all-green while `mcctest` fails —
an **aggregate-only** promote bug (a function only present in full_language.c)
is invisible to the golden columns. A failing `mcctest` with green
exec-replay columns ⇒ suspect a promote/inline soundness gap on a
full_language-only function.

**Bisecting which function's promotion is unsound** (this caught the gate-flip
regression, commit 3c884cc7): add a temporary per-function index gate in the
`do_promote` path of `mccgen.c` — a static counter incremented per promoted
function, gated by `MCC_AST_PROMO_LO/HI/SKIP` env — then binary-search which
index's promotion flips the test from pass→crash. Disassemble with
**`vendor/llvm-clang/.../bin/llvm-objdump.exe`**, which reads both mcc's ELF
relocatable objects (`-d`, `-r`, `--syms`, `--disassemble-symbols=fn`) *and*
the final PE.

Non-obvious PE fact this exposed: promoting a function that grows its body by
N bytes can **deterministically crash a *different*, earlier-running function**
— the corruption is at **link time** (mcc's internal PE linker is sensitive to
the resulting `.text` layout), not runtime. The crash-site relocatable code was
byte-identical; only the later function's size changed. Same promotion on
Linux/ELF (system `ld`) linked fine — which is why the flip's author saw
linux-gcc x86_64 green while PE + arm64 regressed. See [[moderncc-windows-build]].
