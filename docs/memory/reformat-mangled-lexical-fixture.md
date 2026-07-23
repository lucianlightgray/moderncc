---
name: reformat-mangled-lexical-fixture
description: CI conformance-native failures traced to a reformatter mangling the token-sensitive lexical.c fixture
metadata: 
  node_type: memory
  type: project
  originSessionId: ce6d4875-df6d-410d-9b11-49fa4899a0e6
---

The whole-tree reformat commit `6eda0601` ("Reformat to 2-space indent; keep
load-bearing fixtures verbatim") silently corrupted `tests/qemu/conformance/lexical.c`,
which is a deliberately token-adjacency-sensitive fixture (universal character
names + digraphs). The reformatter:
- split the UCN `café` into `caf\ u00e9` → `error: stray '\' in program`
- split the digraphs `<%10, 20, 30%>` and `arr<:0:>`/`arr<:2:>` across lines with
  spaces, destroying the `<%` `%>` `<:` `:>` tokens.

Because this is a **frontend (lexer) compile error**, it is target-independent, so
it broke *every* CI harness that compiles the conformance corpus and nothing else:
`pe-native-conformance` (Windows), `macho-conformance-native`/`macho-structural`
(macOS), and `qemu-<arch>-<libc>-exec` (all qemu arches). ~15 CI jobs red, one root
cause. Fix: restore lexical.c verbatim from `git show 6eda0601^:...` (fixed on main
2026-07-07).

**Why:** these conformance harnesses all glob `tests/qemu/conformance/*.c` and run
each program (rc==0 = pass); a single un-compilable corpus file fails all of them.
The author's local `ctest` was green because Linux native ctest does NOT run the
pe/macho/qemu native-exec harnesses — those live only in CI (Windows/macOS/docker).

**How to apply:** when many native-exec conformance jobs fail across unrelated
platforms at once, suspect a **shared corpus file**, not per-target codegen. Reproduce
fast locally with `ctest --test-dir cmake-debug -R pe-native-conformance
--output-on-failure` (prints per-file PASS/FAIL). Never run a bulk formatter over
`tests/qemu/conformance/lexical.c` (or other token-sensitive fixtures) — it must stay
verbatim. See [[moderncc-windows-build]].
