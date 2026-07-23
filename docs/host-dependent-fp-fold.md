---
name: host-dependent-fp-fold
description: mcc constant-fold of negative-float->unsigned was UB → host-arch-dependent i386 codegen (d3 diff)
metadata: 
  node_type: memory
  type: project
  originSessionId: 9bd9e324-1267-4171-9bfa-1e5adcd69786
---

The i386-codegen-diff-docker **d3** divergence (mcc=187 on x86_64 CI hosts vs 188 on an arm64 dev box) was NOT an i386 codegen bug — it was mcc emitting **different code depending on the machine that built mcc**. `gen_cvt` in mccgen.c folded a float constant to unsigned via `(uint64_t)vtop->c.ld`; converting a *negative* long double to unsigned is UB, so `(unsigned)(-1.5f)` folded to -1 on x86_64 hosts (gcc AND clang, via cvttsd2si) but 0 on arm64 hosts → an extra `dec eax` in the x86_64-built compiler. Fixed 2026-07-20 (commit 5e9aad13): fold the negative case through int64 (`(uint64_t)(int64_t)ld`) — defined, matches mcc's own runtime, byte-identical on x86_64 hosts (no CI golden shift). The d3 test also used that UB expr; changed to `(unsigned)(int)(-1.5f)` (commit 8e2abb72).

Gotchas for debugging this class: (1) a codegen diff that reproduces on CI but not locally may be **host-compiler/arch-dependent**, not target-dependent — build mcc the same way CI does (I used Docker amd64 + gcc) and byte-diff the `.text`. (2) linux/386 exec here is qemu-i386 (Docker), FP-accurate enough for this. (3) The differential fuzzer excludes UB via `has_ub`; hand-written differential tests must do the same — never require an exact cross-compiler match on a UB expression. Related: [[i386-sse-fp-model]], [[docker-amd64-repro]], [[docker-i386-exec]].
