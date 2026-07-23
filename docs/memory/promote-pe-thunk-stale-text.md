---
name: promote-pe-thunk-stale-text
description: "Root cause of arm64-PE promote hangs — AST replay leaves stale .text past ind, corrupting PE import-thunk literals; + how to repro promotion in the cross build"
metadata: 
  node_type: memory
  type: project
  originSessionId: 6b4a761d-7c7e-4ec2-8704-662331a94a05
---

The "internal PE linker sensitive to .text layout" class (see
[[mcctest-ast-promote-debug]]) had a concrete root cause, fixed 2026-07-13
(commit 80ea9843, `fix(ast): zero the vacated .text tail...`):

`ast_func_end` replay emits each function twice — a baseline "record" pass
grows `cur_text_section` to `orig_ind`, then the kept optimized body is
re-emitted from `ast_body_ind_sv`. When promotion **shrinks** the body,
`ind` ends below `orig_ind` and the longer baseline's trailing bytes stay
resident. `section_add()` only zero-fills capacity it *grows*, so those
stale code bytes survive. At PE output, `pe_build_imports` appends arm64
import thunks (`ldr x16,#16; ldr x16,[x16]; br x16; nop; <8-byte IAT
literal>`); the literal is never written — it relies on an R_AARCH64_ABS64
**addend of 0** to receive the IAT VA. If a thunk lands in the stale tail,
`stale_code + IAT_addr` = garbage pointer → `br x16` jumps wild → spins
(wine) / faults (native) **before main**. Non-promote never rewinds-shorter,
so the slot is zero → PE-only + size-gated, ELF always fine (addends there
land on zeroed section too, but the failing target was the thunk).
Fix: after committing the faithful body, `if (faithful && ind < orig_ind)
memset(cur_text_section->data + ind, 0, orig_ind - ind);` — byte-neutral
when nothing shrinks.

Repro gotcha (cost me the most time): the `MCC_ENABLE_CROSS` compilers
(`mcc-arm64-win32`, `mcc-arm64`, ...) are built **without
MCC_CONFIG_OPTIMIZER** (only `${_cross_defs}`, not `_mccdefs`), so `-O1`
does nothing and MCC_AST_PROMOTE=1 is a silent no-op — O0==O1, promote==base
byte-identical. To exercise promotion locally, patch CMakeLists `_cross_defs`
(~line 2657) to append `MCC_CONFIG_OPTIMIZER=1`, reconfigure, rebuild the
target. (Repo commits c1b54817 / c9d1edb9 corroborate this and added
`tests/qemu/native-optcheck.sh` for the arm64-Linux native path.) Then:
build `mcc-arm64-win32` + `arm64-win32-mccrt`, stage a `-B` sysroot
(runtime/include + runtime/win32/include, `arm64-win32-libmccrt.a` under
BOTH `libmccrt.a` and the crossprefixed name), compile the golden, run the
PE under wine-arm64 (mccwine container). See [[arm64-windows-repro-and-atomics]].
