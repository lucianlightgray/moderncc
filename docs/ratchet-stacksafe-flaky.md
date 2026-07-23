---
name: ratchet-stacksafe-flaky
description: ast-verify-ratchet is TWO-SIDED (a baseline gap going faithful is a fatal drift too); deferring an x86_64-linux baseline regen turns CI red
metadata: 
  node_type: memory
  type: project
  originSessionId: 1fcb9812-2fe8-4001-8922-abde5b52b1fd
---

`ast-verify-ratchet` (tests/ast/verify_ratchet.cmake) sweeps the exec-golden
corpus under MCC_AST_VERIFY=1 and diffs the desync/unfaithful/stackresidue gap
set vs a checked-in per-target baseline (verify-baseline/x86_64-linux.txt etc).
It fails on drift in **either** direction: a new gap (regression) AND a baseline
gap that has become faithful ("regenerate to bank the win"). Do not assume "gaps
only shrink so it stays ratchet-safe" — a shrink is a fatal drift too.

CI #1082/#1086 went red on every x86_64 job from ONE such shrink:
`bounds/stack_safe.c:main` went desync→faithful (762 vs 763). It was NOT flaky —
it's the deterministic effect of commit b100da94 ("bracket ftoi/itof libcalls so
the recorder stays synced", src/mccgen.c gen_cast) which makes main's
`(unsigned long long)a / 1.0` libcall-cast faithful on x86_64 (32/32 consistent
faithful across CI jobs). b100da94 regenerated the x86_64-**win32** baseline but
**deferred** x86_64-linux (the win32 build can't emit that key), and the deferral
tripped the next x86_64-linux run.

**Fix (commit b4b1c256):** drop the one now-faithful entry from x86_64-linux.txt
(the deferred regen). `types/conversion.c:main` stays desync on linux (unlike
win32). This superseded a wrong first attempt (bb1b044e) that added a `_flaky`
allow-list to verify_ratchet.cmake on a mis-diagnosis of "nondeterministic" —
reverted, since it would mask a real future regression of that function.

**Lesson:** when a recorder-fidelity fix shrinks a gap set, regenerate EVERY
affected per-target baseline in the same change (or on a runner that can emit that
target's key), never defer one — the ratchet is two-sided. To regen x86_64-linux
you need a real x86_64-linux optimizer+recorder build; a `-DMCC_CONFIG_OPTIMIZER=1`
amalgamation build alone emits NO [ast-verify] lines (recorder inactive), so a
Docker-amd64 regen from this arm64 host is not a faithful stand-in — prefer editing
the known single-entry delta that CI reports. Related class:
[[emit-size-measurement-state-restore]], [[ast-replay-symdbg-debugging]].
