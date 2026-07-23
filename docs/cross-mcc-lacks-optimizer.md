---
name: cross-mcc-lacks-optimizer
description: "cmake-cross/mcc-i386 (and mcc-arm64) are built WITHOUT MCC_CONFIG_OPTIMIZER — AST reemit is compiled out, so cross-arch reemit validation is vacuous unless you build an optimizer-enabled cross mcc"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 78b73548-cb59-49b4-bd97-c4d2a130c1b4
---

`cmake-cross/mcc-i386` compiles `src/mcc.c` with only `-DMCC_TARGET_I386` and **NO
`MCC_CONFIG_OPTIMIZER`** (check its entry in `cmake-cross/compile_commands.json`). So the
whole AST reemit optimizer (`ast_func_begin`/reemit is `#if MCC_CONFIG_OPTIMIZER`) is
compiled OUT: divmagic, branchless select (`MCC_AST_SELECT`), promotion, replay-inline all
silently no-op. Symptoms of an optimizer-less cross mcc: `MCC_AST_VERIFY=1` prints nothing,
default-on select emits a branch (`jge`) not `cmov`, `MCC_AST_DIVMAGIC=1` still emits
`idiv`/`__udivdi3`.

CONSEQUENCE: validating any i386 (or arm64-cross) AST-reemit work against
`cmake-cross/mcc-<arch>` is VACUOUS — on==off trivially because the transform never fires.
I nearly reported a vacuous "i386 divmagic validated" this way. To actually test, build an
optimizer-enabled cross compiler by hand: take the `mcc-<arch>` compile command from
`cmake-cross/compile_commands.json`, drop `-c`/`-o …o`/`-M*`, add
`-DMCC_CONFIG_OPTIMIZER=1 -o mcc-<arch>-opt -lm`, run it (amalgamated `src/mcc.c` links to
one executable). Then compile test `.o`s with that, and link+run under Docker (i386 →
`linux/386`; supply mcc-only runtime helpers like `__mcc_umulh64` by compiling a small
helper `.c` with the container gcc). See [[docker-i386-exec]], [[cross-arch-exec-matrix]].

The standing TODO enabler "build the cross CMake targets with MCC_CONFIG_OPTIMIZER" is the
real gate on i386→Tier-4 parity being *tested in CI* rather than only latent.
