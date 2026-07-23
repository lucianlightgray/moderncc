---
name: dwarf-singleline-prologue-end-bug
description: FIXED 2026-07-23 (387c6da7) — single-line-function prologue-end row now materialized on x86 via DW_LNS_copy; dwarf-gdb-docker green
metadata: 
  node_type: memory
  type: project
  originSessionId: 73e0a28d-1f6f-4b96-97d6-ed965bf500d9
---

FIXED 2026-07-23 (commit `387c6da7`, on main). The `dwarf-gdb-docker` ctest (tools/dwarfgdb-docker.sh) had stayed red on **linux x86_64** cells: `break getval` (single-line function, decl+body on one line) landed in the *next* function `steps` with garbage args.

**Why the earlier be7f94fa "fix" (arm64-validated) did NOT fix x86:** be7f94fa added a `dwarf_line.prologue_end` flag so `mcc_debug_line` bypasses the same-line suppression once and re-enters with `len_line==0`. But re-entering is not enough — `mccdbg.c`'s else-branch only ever APPENDS a `.debug_line` row when `len_line!=0` OR the pc-only special opcode fits (`n = len_pc*DWARF_LINE_RANGE(14) + 18 <= 255`). x86 leaf prologues are 19 bytes → `n = 284 > 255` → it emitted `DW_LNS_advance_pc` (advances the address register but appends NO row). So the `prologue_end` flag *leaked* onto the next function's entry row, and gdb's break-skip overshot into `steps`. arm64's smaller prologue fit the special opcode (which DOES append a row), so a row got emitted there — masking the bug and giving the false "fixed on arm64" signal.

**The actual fix:** in the else-branch, track `row_emitted`; if neither path appended a row, emit `DW_LNS_copy` to materialize the post-prologue row at the advanced PC. Byte-identical for every multi-line function and the special-opcode-fit path (incl. arm64's single-line case) — the only new output is one `DW_LNS_copy` in the previously-broken x86 single-line case. Confirmed by raw-line diff: getval's `advance_pc 19` is now followed by `Copy`, and steps' entry row no longer inherits prologue_end.

**Lesson:** `DW_LNS_advance_pc` moves the address register but does NOT append a row to the line matrix — only special opcodes and `DW_LNS_copy` append. A pending `set_prologue_end` with no appended row leaks the flag to the next real row. Also: validating a `.debug_line` change on ONE arch is not enough when the encoding path depends on prologue size (special-opcode fit) — the arm64/x86 prologue-size difference gated the whole bug.

Validated in Docker (debian bookworm + gdb, `-DMCC_TARGET_X86_64=1`): `dwarf-gdb-docker` exits 0 — backtrace frames keep source lines, `break getval` stops IN getval (`a=11 b=20` at t.c:7), stepping reads `a=11 b=22 c=19`. Reproduce/validate on this Windows host via `MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' bash tools/dwarfgdb-docker.sh <workdir>` (Docker Linux containers work here). See [[x86-optimizer-differential-docker.md]] for the Docker cross-mcc pattern.
