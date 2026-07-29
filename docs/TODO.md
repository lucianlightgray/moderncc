# TODO

## How to process
- **P0 first: full JIT/AOT parity across every arch and triple (section below). Work it ahead of the FP/compute and ungate campaigns unless something is actively broken.**
- Mark an item in progress, commit+push to main, then start it.
- Implement gated behind a new env (default OFF ⇒ byte-identical); validate the gated-ON path to the M8 bar; independently re-verify the pass fires + correctness vs gcc; commit; prune the item.
- Completed items are pruned entirely; detail lives in git history.
- M8 bar: ctest byte-identity · `-O6` differential vs gcc/clang · 3-stage self-host fixpoint · **`MCC_JIT=1` ≡ `MCC_JIT=0` on every triple that has a JIT (P0)** · UBSan/ASan · cross-arch (i386/arm32/riscv64/arm64) · differential fuzz (x86_64 + native arm64) · shadow-IV zero-divergence. The differential fuzzer runs on native x86_64 AND arm64 (`ubuntu-24.04-arm`); only the shadow-IV oracle stays x86-only.
- Cross-arch checks use `cmake-cross/mcc-i386` and `cmake-cross/mcc-arm64` (the `cmake-qemu-*` builds emit native x86_64 and lack the optimizer).
- Always enable TRACE while working: configure with `-DMCC_CONFIG_TRACE=ON` (defines `MCC_CONFIG_TRACE=1`, activating the `MCC_TRACE(...)` branch markers) and run with `-v128` (the `MCC_LOG_TRACE` bit, `1<<7`). `MCC_TRACE` writes a `[TRACE] file:line func:` line to **stderr** per branch — capture with `2>trace.log`. **SCOPE IT** (`MCC_TRACE_FILE`/`MCC_TRACE_FUNC` substring comma-lists, `MCC_TRACE_SKIP` exact names): an unscoped trace of a two-line program is 127,871 lines of which 80% is `mccpp.c` preprocessor churn, and `MCC_TRACE_FILE=mccast` cuts that to 2,467. To find where two configurations diverge, do NOT hand-patch a temporary `fprintf` into a suspect guard — run `tools/tracediff.sh <mcc> <src> <sideA> <sideB>`, which diffs the two traces and prints the first divergence with the deciding values attached (each side takes env assignments and/or mcc flags, so both `MCC_AST_FOO=0 MCC_AST_FOO=1` and `-O1 -O3` work). See the tracing section below. The logging layer (`src/mcclog.h`) is `MCCState`-free: it reads a free-standing `mcc_log_verbose` global (declared in `mcclog.h`, defined in `mcchost.c`, mirrored from the active state in `mcc_enter_state`/`mcc_exit_state`); explicit-source variants are `mcc_log_enabled_v`/`mcc_logf_v`/`MCC_TRACE_V`/`MCC_DEBUG_V`. Standalone non-amalgamated compiles of `mccast.c`/`mccstats.c` (e.g. `asttool`) can trace if built with `MCC_CONFIG_TRACE=1` and `mcc_log_verbose` set directly (those targets don't parse `-v`).
- Drive each change with grep: grep the token/env/function/`ST_FUNC` name across `src/` → edit → rebuild → run `-v128 2>trace.log` and grep the trace (plus ctest/differential logs) to confirm the intended branch fired and byte-identity/gcc-parity held. Preserve the `MCC_TRACE("br\n")`/`MCC_TRACE("enter\n")` markers on new branches so `tracegate` stays satisfied.
- Build/test loop: `cmake --build cmake-debug --target mcc -j` (the configured dir; ninja incremental is seconds). AST-internal unit selftests live in `tools/asttool.c` (`#include "mccast.c"` directly, so it sees statics + needs VT_* `#ifndef` fallbacks; framework `CHECK(cond,"msg")`, suites dispatched by `argv[1]`). Build `--target asttool`, run `./cmake-debug/asttool <suite>` (no arg = all). Register a new suite in BOTH the `tools/asttool.c` main dispatch AND the `foreach(_an ...)` list in `CMakeLists.txt` (~3292), reconfigure, then `ctest --test-dir cmake-debug -R "^ast/"`. The `mcc_build` fixture (~66s) rebuilds mcc first. Self-host compile: extract `-D`/`-I` from `compile_commands.json` for `src/mcc.c` and pass as argv (mcc's `@file` mangles quoted `-D` path macros).
- Fast `MCC_AST_*` gate validation (skips the cmake/JIT build): in a `debian:bookworm-slim` container (`apt-get install gcc gcc-multilib gcc-aarch64-linux-gnu gcc-riscv64-linux-gnu qemu-user`), build a per-target mcc as a host tool straight from the amalgamation — `gcc -w -DMCC_CONFIG_OPTIMIZER=1 -DMCC_TARGET_<T> -Isrc -Iinclude -Isrc/formats -Isrc/objfmt -Isrc/arch/<a> -O0 -o mcc-<T> src/mcc.c` (T ∈ X86_64/ARM64/RISCV64/I386; entry `src/mcc.c`→`libmcc.c`). Run mcc with `-B/src -I/src/runtime/include` (its own headers live in `runtime/include`, not `include/`) and freestanding test programs (`extern int printf(...)`, no system headers — avoids glibc multiarch errors). Isolate codegen with `mcc -c foo.c -o foo.o` then link/run via the matching cross gcc (`aarch64/riscv64-linux-gnu-gcc`, `gcc -m32` for i386, native for x86_64) under `qemu-<arch> -L /usr/<triple>` (i386/x86_64 run native; mcc's own linker fails in slim images, no `crt1.o`). Byte-identity = gate-off object vs no-env object (`md5sum`); fire = gate-on ≠ gate-off; correctness = diff vs the same-arch gcc reference. On a Windows/MSYS host every `docker run`/`exec` with a `-v` mount needs `MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'`; use a persistent `docker run -d … sleep N` + `docker exec` to build once and iterate. Full AOT==JIT / arm64-native needs the cmake debug preset with the embed-jit blob (heavy).

## P0 — FULL JIT/AOT PARITY ACROSS EVERY ARCH AND TRIPLE (urgent)
**This is the top priority; work it before anything else in this file, including the FP/compute and ungate campaigns.** The parity property is: for every supported triple, a JIT-capable `mcc` runs on that triple, JIT-compiles itself, and produces output byte-identical to the same compile with the JIT off — `MCC_JIT=1` ≡ `MCC_JIT=0`. Where that does not hold, the JIT is a second, unverified code path: today's optimizer gates are validated against AOT only, so any divergence means a whole class of miscompiles nothing in the suite can see. It also blocks unrelated work — `AOT==JIT-on-arm64` is already listed as a gating condition for the `MCC_AST_OPASSIGN` and `MCC_AST_PROMO_ARROW` flips, and will gate the 2026-07-26 gates too.

**Current parity matrix (consolidated from the detailed items below — those remain the source of truth for each blocker):**

| triple | JIT self-host ≡ AOT | blocker |
|---|---|---|
| x86_64-linux | **YES** | — (reference) |
| arm64-linux | **YES** | — (reference; JIT≡AOT self-host **PROVEN ON NATIVE Apple-M1 arm64 silicon, no emulator, 2026-07-27** — a stage-2 self-JITing `mcc` hot-swapped 52 of its own KGC-verified functions under `MCC_JIT=1` yet emitted an object byte-identical to `MCC_JIT=0`, md5 `f6c25ff9…`; see step 2) |
| i386-linux | **AOT self-host VERIFIED** + **`-run` JIT parity now NON-TRIVIALLY VERIFIED 2026-07-27** | 3-stage AOT fixpoint o1==o2==o3 byte-identical at 1543403 B. JIT: native 32-bit `mcc32` on x86_64 hardware (no qemu), `-O2 -run`, `MCC_JIT=1` output IDENTICAL to `MCC_JIT=0` on 4 program shapes (int kernel, double kernel, snprintf/string, 64-bit-math-on-32-bit-`long`) with **`swapped=2..3` real dispatch installs each** — this supersedes the old `swapped=0` reading that made the equality near-trivial. Also 60-seed differential fuzz vs `gcc -m32`+`clang -m32` with the full `--gates` sweep: 0 miscompiles. **Still NOT proven: self-host UNDER the JIT.** The JIT is mode-6 (`-run`) only, so a `-c` self-compile reports `recompiles=0` and cannot exercise it; proving that needs mcc32 running itself via `-run`. |
| riscv64-linux | **`-run` + JIT parity VERIFIED** under qemu-riscv64 on an arm64 (weak-memory) host, 2026-07-27 | The recorded rc=139 SEGV was a **qemu-on-x86-TSO artifact + a `-static`-without-`-DMCC_CONFIG_STATIC` build error**, NOT a `-run` bug: on arm64-hosted qemu, riscv64 `-run` runs `hi`/`hot` correct + byte-identical under `MCC_JIT=0`≡`MCC_JIT=1` (dynamic build, and static+`-DMCC_CONFIG_STATIC -lm`, and a self-hosted embed-JIT mcc1). Regression-locked by the `run-parity-riscv64` ctest. Not yet native riscv64 silicon; full self-compile byte-identity gate still TODO but no longer blocked. |
| arm-linux | **`-run` + JIT parity VERIFIED** (static AND dynamic host) under qemu-arm on an arm64 host, 2026-07-27 | CONFIG_STATIC armv7 mcc `-run`s `hi`→`sum=10` + `hot`→`acc=2075865568` byte-identical `MCC_JIT=0`≡`=1`. The DYNAMIC-host `R_ARM_CALL` (reloc 28) out-of-±32MB error is now **FIXED** (`arm_veneer_memory_calls`, the arm peer of `arm64_veneer_memory_calls`: emits a `.mcc.veneer` long-branch stub for far external calls in `-run`) — dynamic-host `-run` now works too. Regression-locked by the `run-parity-arm` ctest. Not native arm silicon (qemu). |
| x86_64-win32 | **YES on llvm-mingw** (2026-07-27); CI winlibs-gcc still blocked | `mcc-jit` self-compiles `src/mcc.c` to a **byte-identical** object under `MCC_JIT=1` ≡ `MCC_JIT=0` (md5 `d3c01df6…`, no crash) on the mstorsjo llvm-mingw host — parity PROVEN there. The CI `0xC0000005` is confirmed **winlibs-gcc-toolchain-specific** (P0 step 5, needs a CI trace), not a universal x86_64-win32 JIT bug. |
| i386-win32 | **YES** — AOT self-host byte-identical; runtime-JIT MCC_JIT=1≡MCC_JIT=0 byte-identical (`-c` + `-run`); `MCC_JIT_I386_STUBS` **flipped default-ON** 2026-07-27 (P0 step 4 done) | soak green locally on WoW64 (`tools/i386win32-soak.sh`): AOT self-host + 36/37 selftests + differential-fuzz vs gcc+clang (0 divergences, generator now i386-`long`-safe). Follow-up (non-blocking): per-arch baked `MCC_EMBED_JIT_BLOB` (standalone `--embed-jit` exe) |
| arm64-win32 | **NO** | needs arm64-Windows HW (SEH/icache/frameless-leaf; qemu+wine mask via x86-TSO) |
| x86_64-osx | **NO** | reader reads PLAIN Mach-O relocatables (2026-07-27) **and ARCHIVE MEMBERS (2026-07-28** — BSD `#1/` names + little-endian `__.SYMDEF` index; `macho-archive` ctest proves byte-identity vs a bare-object link and selective pull). Remaining blocker is **relocations** (`reloff`/`nreloc` are read but ignored), plus validation against real `clang -c` objects and a real `libmcc_jitengine.a`. See P0 step 1 |
| arm64-osx | **NO** | same Mach-O archive/relocation gap as x86_64-osx; runtime JIT unavailable in every local build |

**Ordered plan — each step unblocks the next, and the first two are the widest wins:**
1. **Mach-O relocatable `.o` READER — plain objects LANDED 2026-07-27; ARCHIVES LANDED 2026-07-28 (on Linux, via `llvm-ar`); RELOCATIONS are the remaining blocker and are reserved for macOS.** `macho_object_type` + `macho_load_object_file` (`mccmacho.c`, modelled on `coff_load_object_file`) parse `MH_OBJECT`: `LC_SEGMENT_64` sections are mapped to mcc's ELF-internal names (`__TEXT,__text` → `.text`, `__data` → `.data`, `__bss` → `.bss`, `__const`/`__cstring` → `.rodata`, else `.<seg><sect>`) and merged like the COFF path; `LC_SYMTAB`/`nlist_64` symbols are registered via `set_elf_sym` with `N_UNDF`→`SHN_UNDEF`, `N_ABS`→`SHN_ABS`, `N_SECT`→ the merged section at the right offset. Hooked into the `default:` case of the MACHO branch of the `libmcc.c` loader switch, replacing `unrecognized file type`.

   Validated end-to-end on Linux against `tests/macho/make_fixture.py`: `mcc-x86_64-osx -nostdlib usefx.o fixture.o prov.o` links, and `llvm-objdump --macho -d` on the result shows `callq _mcc_fixture_defined` resolving to the fixture's `movl $0x2a, %eax ; retq` placed at `0x100001012` — i.e. the section data and the symbol both came out of the Mach-O object. Full ctest 6522/6522. Note symbol names are kept underscore-prefixed: a MACHO-target mcc uses `_name` internally (it emits references to `_mcc_fixture_defined`), so the reader must NOT strip the leading `_` the way the i386-COFF path does.

   **NOT done, and to be finished ON macOS:**
   - **Archives — DONE 2026-07-28, ON LINUX. This item was wrongly reserved for macOS.** The reasoning was that GNU `ar` cannot index Mach-O members, so a Linux-built archive is invalid by construction — true of GNU `ar`, but `llvm-ar` writes a correct BSD armap and `llvm-nm --print-armap` reads it, and both are ordinary LLVM tools. The whole path is therefore testable here against `tests/macho/make_fixture.py`.

     The earlier attempt's hooks were in the RIGHT PLACES and still pulled nothing, because two format details were missing and both are silent failures:
     - **BSD extended names.** `llvm-ar` writes `#1/<len>` headers where the member NAME occupies the first `<len>` bytes of the member DATA. `read_ar_header` had no `#1/` case, so the computed data offset pointed at the filename instead of the Mach-O header — every probe saw garbage magic and declined. Fixed in `read_ar_header`, which now consumes the name, shortens `ar_size` by it, and returns the larger header length.
     - **A BSD symbol index.** `mcc_load_archive`'s a la carte branch recognized only the SysV names (`/`, `/SYM64/`). A `__.SYMDEF` archive matched neither, so the loop walked every member and loaded NOTHING, silently and successfully. Added `mcc_load_alacarte_bsd` — note the BSD index is **little-endian** where the SysV one is big-endian, and its layout is a byte-count + `{str_offset, member_offset}` ranlib array + string table, not a parallel offset/name pair.

     Validated four ways by the new `macho-archive` ctest cell: the link succeeds; it is **byte-identical to linking the same member as a bare object**; the pulled member's actual body (`movl $0x2a, %eax ; retq`) is in the output per `llvm-objdump --macho -d`; and an archive whose symbols are unreferenced pulls **nothing**, so a la carte is genuinely selective rather than load-everything. The cell also asserts the armap is non-empty first, so it cannot pass vacuously.

     Still macOS-only: exercising this against a REAL `libmcc_jitengine.a` and real `clang -c` objects. The fixture is synthetic and relocation-free, so **relocations remain the blocker below** — archive membership is solved, real-object content is not.
   - **Relocations — x86_64 DONE 2026-07-28, ON LINUX; arm64 still open.** Also wrongly reserved for macOS: `clang -target x86_64-apple-macos11 -c` runs on Linux and emits genuine Mach-O objects with genuine `X86_64_RELOC_*` entries, which is all the loader consumes.

     **This was a SILENT WRONG ANSWER, not a missing feature.** `reloff`/`nreloc` were read and ignored, so linking against a relocated Mach-O object SUCCEEDED and left the placeholder displacement in place — `e8 00 00 00 00`, a call to the next instruction. Nothing in the suite failed; the binary was simply broken. `macho_load_relocs` now handles `UNSIGNED`/`SIGNED`/`SIGNED_1/2/4`/`BRANCH`, both extern and section-relative, and **hard-errors on anything else** (scattered, unsupported widths, unknown types) rather than skipping it. arm64 relocations hard-error by name until they can be validated on arm64-osx — refusing is correct where guessing produces a working-looking binary.

     Two conversions the format forces: Mach-O is REL-style (addend lives in the instruction field) while mcc's internal form is RELA, so each addend is read back out of the section data; and Mach-O pcrel is measured from the END of the relocated field where ELF measures from its start, hence `-4` plus the extra `-1/-2/-4` that `SIGNED_1/2/4` name.

     Guarded by the `macho-reloc` ctest cell, which asserts clang actually emitted BRANCH/SIGNED relocations first (so it cannot pass vacuously), then checks RESOLVED TARGETS rather than link success: the call must disassemble to `_other` by name, no `e8 00 00 00 00` placeholder may survive, and the rip-relative data reference must ARITHMETICALLY compute to the address `llvm-nm` reports for `_msg` (verified at `0x100008000`).

     **arm64 DONE 2026-07-28 too** (`macho-reloc-arm64` cell), via `clang -target arm64-apple-macos11` + `mcc-arm64-osx`. `BRANCH26` → `R_AARCH64_CALL26`, `PAGE21` → `R_AARCH64_ADR_PREL_PG_HI21`, `PAGEOFF12` → **one of five** ELF relocations chosen by decoding the instruction (`ADD_ABS_LO12_NC` for an ADD; `LDST8/16/32/64/128_ABS_LO12_NC` for a load/store, size from the instruction's size/opc fields). `ARM64_RELOC_ADDEND` is handled as the pseudo-entry it is — it carries a 24-bit addend in `r_symbolnum` for the entry that FOLLOWS it, because an arm64 instruction has nowhere to put one.

     One trap worth keeping: for `msg[5]` clang does NOT emit `ARM64_RELOC_ADDEND` — it folds the `+5` into the `ldrsb` immediate. A LO12 relocation that OVERWRITES that field therefore silently reads `msg[0]`. mcc's LO12 handlers add into the existing immediate, so this works, and the cell pins it by asserting the offset is non-zero AND that `adrp`+`ldrsb` arithmetically computes `_msg+5` (measured `0x100004005`). A cell that only checked "the link succeeded" or "the offset resolved" would pass while reading the wrong byte.

     **BUG FOUND AND FIXED 2026-07-28 by combining the two cells.** mcc's relocation appliers ADD into the existing field (`add32le`/`add64le`, REL semantics inherited from tcc) rather than overwriting it, so passing the field's existing value as the RELA addend counts it TWICE. The addend must be **0** (plus only the pcrel bias). This was invisible to both narrower cells — `macho-archive` uses the relocation-free fixture, `macho-reloc` used a bare object with a ZERO addend (`msg[0]`) — and with `tag[3]` the reference resolved to `_tag+6`. The lesson generalises: a relocation test whose addend is zero cannot distinguish "addend applied once" from "applied twice", and a wrong addend still disassembles to a plausible instruction, so assertions must be arithmetic against `llvm-nm`.

     New `macho-archive-reloc` cell covers the gap: a real 4-member `llvm-ar` archive of clang objects, transitive pull `a → b → c` (b and c reachable only through a), non-zero addends of two different widths in two different members (`tag[3]`, `tbl[2]`), and a member referencing a symbol that exists nowhere — so an unconditional pull fails the link outright rather than passing quietly.

     Remaining: scattered relocations, `GOT`/`TLV`/`SUBTRACTOR` (all hard-error by name rather than being skipped), and validation against a real `libmcc_jitengine.a` — that last one genuinely wants macOS, because building the JIT engine for an apple triple needs the macOS SDK headers, which is a real gate rather than a tooling assumption.
   - **Real-world corpus.** The fixture is minimal and synthetic. Validate against actual `clang -c` output and a real `.a` on macOS; that is also the only place `--embed-jit` can be exercised for the osx triples.

2. **riscv64 + arm-linux JIT run/verify — DONE** (whole ELF family: arm64 proven on native silicon, riscv64 + armv7 verified under emulation on an arm64 host). Detail in git history.
3. **JIT/AOT arena-normalization alignment — DONE 2026-07-27.** AOT- and JIT-shaped arenas now agree on `ast_intention_hash` for the same function, so a slice ident proven by one path is findable by the other. Two distinct causes, both fixed: the two hashes were being taken at different pipeline stages (parse-end vs post-optimizer), and the blob round-trip dropped symbol identity for plain locals while the hash still folded it. Syms now serialize as position+type only (`mccjit_sym_positional`), and both hashes mirror that rule (`ast_ih_sym_dropped`). Note for future readers: `ast_ih_sym_dropped` is PERMANENT, not scaffolding — the AOT arena holds live `Sym *` pointers that the blob deliberately omits, so the hash must describe the serializable identity. Detail and the retired false leads (hash-time Convert-folding; 'the serializer is at fault') are in git history around `7335b252`.
4. **i386 stub-tail flip — DONE 2026-07-27.** `MCC_JIT_I386_STUBS` is default-ON and now has native-ELF backing: i386-linux AOT 3-stage fixpoint byte-identical, 60-seed differential fuzz green with the full gate sweep, and `-run` `MCC_JIT=1`==`MCC_JIT=0` with 2-3 real dispatch swaps per program across 4 shapes. Reproduce: build `mcc32` with `gcc -m32 -w -DMCC_CONFIG_OPTIMIZER=1 -DMCC_TARGET_I386 -DMCC_EMBED_JIT=1 -Isrc -Iinclude -Isrc/formats -Isrc/objfmt -Isrc/arch/i386 -O0 -o mcc32 src/mcc.c`. For `-run` you need a PRIVATE `-B` stage: `cmake-cross` cannot serve i386 because **`runmain.o` is looked up by a FIXED name** and the one there is x86_64 (`invalid object file`). Copy `cmake-cross/include/mccdefs.h` + `cmake-cross/i386-libmccrt.a` (also as `libmccrt.a`) into a stage dir and build `runmain.o` there with mcc32 itself from `runtime/lib/runmain.c`. Fuzz refs need `-m32` wrapper scripts. **Follow-up DONE 2026-07-27 — a multi-arch `-B` dir now serves `-run` on every target, no per-arch stage.** Two separate causes, and the lookup code was innocent of both: (1) `mcc_add_support` was ALREADY generic (`<arch>[-<os>]-<filename>`), but the cross build never STAGED a runmain — `runmain` was appended to `_objs` only under `if(native)`, so `i386-runmain.o` simply did not exist. Fixed in CMakeLists; all 13 arch/OS variants now stage beside the plain one. (2) With that in place the failure moved to `libmccrt.a`, because the `-run` path never used the arch-aware lookup at all: the ELF `MCC_OUTPUT_MEMORY` branch called `mcc_add_dll(s1, MCC_MCCRT, 0)` on the PLAIN name. Absence is silent there by design, but a WRONG-ARCH archive is not — the loader raises `mcc_error_noabort` from inside, which no caller flag suppresses, and `nb_errors != 0` then fails the link. Added `mcc_add_support_opt()`, which loads only what the read-only arch probe confirms (plain, then the arch-tagged name) and otherwise stays quiet, and pointed the MEMORY branch at it. Verified: i386 `-O2 -run` against `-B$PWD/cmake-cross` with no private staging, `MCC_JIT=1` output identical to `MCC_JIT=0` on all 4 shapes with `swapped=2..3`. Full ctest 7121/7121.
5. **PE x86_64 runtime-JIT crash** — needs a CI stack trace; the icache hypothesis is already disproven, so do not re-try it.
6. **arm64-Windows** — genuinely HW-gated. Keep it explicitly skip-marked so it never false-greens, and do not let it hold up 1–5.

**Standing rule while this is P0:** any new optimizer gate must state its JIT status. The 2026-07-26 gates (`MCC_AST_CHAINSTORE`, `MCC_AST_PROMO_INCDEC`, `MCC_AST_IVSR_PTR`, `MCC_AST_REGDISP`, `-fc99-inline-body`) were validated AOT-only across x86_64/riscv64/arm64 and have **no JIT validation at all** — that gap is now part of their flip criteria.

### CAMPAIGN IN PROGRESS (2026-07-27) — feature-complete AOT/JIT parity that ungates every Windows/PE/x86 path
Goal: bring the **x86_64-win32 / i386-win32** rows of the parity matrix to the same "feature-complete, at-parity, ungated" state the ELF reference arches already hold — a JIT-capable `mcc.exe` that self-hosts under `MCC_JIT=1` byte-identical to `MCC_JIT=0`, and a `--embed-jit` standalone-exe path that works with the native COFF toolchain. This is the local, non-HW-gated portion of P0 steps 4–6; **arm64-win32 stays explicitly HW-gated** (needs real arm64-Windows silicon for SEH/icache/frameless-leaf — do not false-green it). Concrete workstream, tractable → blocked, each cross-referenced to its detailed item below:

**Reality check (2026-07-27, verified on this Windows host — mingw `cmake-build-debug` + MSVC `cmake-msvc`):** most of the infra the items below described as "to do" ALREADY LANDED and is validated locally. **All 48 `jit/selftest-*` pass on mingw-Windows x86_64**, including the ones the stale text says skip. Do NOT re-implement these — verify and prune:
- **COFF `.o`/`.a` reader (`coff_load_object_file`) is complete** and default-on for all PE targets; the CMake `--embed-jit` blob guard (~2009/2655) is already ungated on WIN32 with mingw runtime-lib discovery. The blob embeds + links on MSVC.
- **Win64 `mccjit_make_kgc_stub_mixed` is implemented** (`mccjit_embed.c`, `#if MCC_HOST_WIN32`, "Win64 positional mixed stub") and `jit/selftest-mixed` RUNS (no skip) on x86_64-win32. The `return NULL` at ~4813 is the generic non-x86/arm fallback, not WIN32. Item is DONE.
- **The Win32 `MCC_EMBED_JIT` runtime is ported** — `mccjit_win32.h` shims `mmap`/`VirtualProtect`/`FlushInstructionCache`/SRWLock/CondVar/threads; `host_runmem_*` all have WIN32 paths; fork/pthread_atfork are correctly no-op'd (no Windows equivalent needed). Item is DONE.

1. **[soak now runs locally — 2026-07-27] i386-PE stub-tail flip (`MCC_JIT_I386_STUBS`).** No longer CI-gated: installed winlibs i686 GCC (real libgcc) at `C:/Users/llg/opt/mingw32`, built the i386-win32 cross compiler via the `cross` preset, and ran the soak on WoW64. AOT 3-stage self-host is byte-identical and 30/37 stub-tail selftests pass gate-on. It is NOT the "mechanical" flip this line assumed — see P0 step 4 for the concrete remaining checklist (runtime staging for the `-run` selftests, `mixed`/`stage2`, differential-fuzz). Do not flip yet.
2. **[needs CI trace, NOT local] PE x86_64 runtime-JIT self-host `0xC0000005`.** P0 step 5; icache hypothesis already disproven — do not re-try it, get a CI stack trace at the fault. Keep skip-marked until then.
3. **[HW-gated] arm64-win32.** Keep every arm64-Windows JIT path explicitly skip-marked (`SKIP_RETURN_CODE 77`) so it never false-greens; logic is wine-validated but the fault subset needs native silicon.

Rule for this campaign: default-OFF ⇒ byte-identical (M8 bar), validate the gated-ON path with local MSVC (`cmake-msvc`) + mingw (llvm-ucrt) builds, prune each item on completion and update the parity matrix row. Honesty rule: an item that is HW-/CI-/toolchain-gated is NOT "implementation complete" — it is marked blocked with its exact blocker, never quietly closed.

**CONSOLIDATION (2026-07-27):** with all five source changes in (SECREL native-TLS COFF reading, à la carte pull-once, `__ImageBase` synthesis, compiler-rt embed fallback, `K32GetProcessMemoryInfo` def) plus the CMake self-host enablement, the **full local ctest suite is 100% GREEN — 6424/6424, 0 failures** on llvm-mingw x86_64 (the core PE linker path is exercised by every test that links the CRT). x86_64-win32 `--embed-jit` + self-host bake + `MCC_JIT=1`≡`MCC_JIT=0` parity are done, wired, and regression-clean. Regression lock-in **DONE**: `embed-jit-smoke` ctest (`tools/embed-jit-smoke.py`, gated `WIN32 AND NOT MSVC`, SKIP-77 on no-blob/missing-runtime-lib, FAIL only on wrong output or a non-lib bake failure; JIT-OFF only so it never touches the winlibs `0xC0000005`). Passes locally on llvm-mingw. So the entire x86_64-win32 (mingw) `--embed-jit` path is implemented, proven, wired, and CI-guarded. **All local-tractable campaign items are complete;** what remains is strictly gated: i386 (no i386 toolchain here), MSVC embed (ucrt/msvcrt CRT-model conflict), P0 step 5 (winlibs-specific CI crash — needs a CI trace), arm64-win32 (HW).

## `-march` (default `native`) + move every ISA-dependent optimizer behind it
**Blocking caveat RESOLVED 2026-07-27 by the second option: `ROUND_INLINE` is out of the `o4` blanket.** It was `ast_env_gate("MCC_AST_ROUND_INLINE", o4)` while its own comment said "Default OFF because roundsd is SSE4.1" — the code contradicted the doc. Verified before and after on a `floor`/`ceil`/`trunc` source: `-O4` emitted **3 `roundsd`/`roundss` and zero libm relocs**, i.e. `-O4` output genuinely required an SSE4.1 CPU with nothing recording it; now `-O2`/`-O3`/`-O4` all emit 0 `roundsd` and keep the 3 libm calls, and `MCC_AST_ROUND_INLINE=1` still inlines on demand. Full suite 7214/7214. The principle worth keeping when `-march` lands: **`-O4` means "run every optimizer", not "raise the required ISA"** — an optimizer that changes the ISA floor does not belong in a blanket keyed on optimization effort. Restore it to `o4 || (level >= x86-64-v2)` once the level exists. Checked at the same time: no `vfmadd`/`vfmsub` is emitted at `-O4`, so `MCC_AST_FMA_INLINE` is not doing the same thing on x86 (it is arm64/riscv64-only, as this section already states).

**Motivation.** *(Historical: the `-O4`/SSE4.1 leak described here was FIXED 2026-07-27 — see the resolved caveat above. Kept because it is the clearest statement of why the axis is needed.)* `-O4` runs every implemented optimizer, which included `MCC_AST_ROUND_INLINE`, and `roundsd` is SSE4.1, not mcc's SSE2 baseline — so `-O4` output required an SSE4.1 CPU with nothing in the compiler recording that fact. The root cause is that mcc has no way to express a target ISA level: the optimizer gate *is* the ISA switch (`mccast.c` says so outright — "the user opts in for an SSE4.1 target (like gcc's `-msse4.1`)"). Every ISA-dependent optimizer either stays off and loses performance, or goes on and silently narrows the set of CPUs the output runs on. `-march` is the missing axis.

**Current state.** `-march=` is already **accepted and discarded** — `case MCC_OPTION_m` (`libmcc.c`) matches `arch=`/`tune=`/`cpu=`/`cmodel=`/`fpmath=` and `break`s, so `mcc -march=x86-64-v3` is silently a no-op today. `-m32`/`-m64` are the only `-m` forms with meaning. On ARM the ISA level exists but is **compile-time only**: `MCC_CONFIG_CPUVER` (CMake `MCC_CPUVER`, `CMakeLists.txt`) baked per build and read at `arm-link.c` (`blx_avail = CPUVER >= 5`) and `arm-gen.c` (`#if CPUVER >= 7`), alongside `MCC_ARM_EABI/_VFP/_HARDFLOAT/_IDIV`. Nothing is queryable per invocation.

**Plan.**
1. **Feature mask on `MCCState`**, not a string. Parse `-march=`/`-mcpu=`/`-mtune=` into a bitmask + a single predicate (`mcc_isa_has(s1, MCC_ISA_SSE41)`). Keep the existing string forms accepted so no command line regresses.
2. **`-march=native` is the default**, resolved by host detection: `CPUID` leaf 1/7 on x86, `getauxval(AT_HWCAP/HWCAP2)` on arm64/armv7, `AT_HWCAP` + `/proc/cpuinfo` on riscv64, with a documented fallback to the triple baseline when detection fails. **Cross-compilation must NOT default to native** — when the target triple differs from the host, default to that triple's baseline (`x86-64`, `armv7-a`, `armv8-a`, `rv64gc`, `i686`) or native detection will bake host-only instructions into cross output.
3. **Named levels**, matching gcc/clang so muscle memory transfers: `x86-64` (SSE2, today's baseline), `x86-64-v2` (SSE4.2/POPCNT), `x86-64-v3` (AVX2/FMA/BMI), `x86-64-v4` (AVX512); `armv7-a[+idiv][+vfp][+neon]`, `armv8-a[+simd]`; `rv64gc` and friends. `-march=<level>` must be *reproducible*: same level ⇒ byte-identical output on any host.
4. **Re-gate the ISA-dependent optimizers** from "opt-in env knob" to "on when the ISA allows", i.e. default becomes `o4 || (level >= N && mcc_isa_has(...))` and the `MCC_AST_*` env var stays as a manual override:
   - `MCC_AST_ROUND_INLINE` — `roundsd`, needs **SSE4.1** (`x86-64-v2`). The one actually breaking `-O4` portability today.
   - `MCC_AST_FMA_INLINE` — x86 needs **FMA3** (`x86-64-v3`); arm64 `FMADD` and riscv64 `fmadd.d/.s` are baseline for those triples, so they should turn on with the triple, not wait for a flag.
   - `MCC_AST_COPYSIGN_INLINE` — SSE2 on x86 (baseline ⇒ can just be on); riscv64 `fsgnj` needs F/D.
   - `MCC_AST_MATH_INLINE` — `sqrtsd`/`FABS` are baseline everywhere it is implemented; the non-x86_64 `#else` default of 0 (`mccast.c`) is a golden-regen debt, not an ISA limit — retire it rather than march-gate it.
   - `MCC_AST_DIVMAGIC` on armv7 — the magic-multiply choice depends on whether `idiv` exists (`MCC_ARM_IDIV`); becomes `armv7-a+idiv`.
   - arm `blx` (`arm-link.c`) and the `CPUVER >= 7` paths (`arm-gen.c`) — migrate off the compile-time `#if` onto the runtime mask so one binary can target several ARM levels.
   - **Do NOT march-gate `MCC_AST_XMM_HI` or `MCC_AST_REGDISP`.** XMM8-15 and register-displacement addressing are x86-64 *baseline*; they are default-off for validation reasons, not ISA reasons. Gating them behind `-march` would be wrong and would hide the real (soak) blocker.
5. **Predefined macros must follow `-march`** (`mccpp.c`, same place `__OPTIMIZE__` is set): `__SSE4_1__`, `__AVX2__`, `__FMA__`, `__ARM_NEON`, `__ARM_FEATURE_IDIV`, `__riscv_flen`, … Otherwise system headers and libc `ifunc`/inline-asm paths disagree with what mcc actually emits — a silent-miscompile class, not a cosmetic gap.
6. **Introspection**: report the resolved level and feature set (extend `-print-search-dirs`, or a `-print-isa`), so CI and bug reports can state the ISA a build targeted.

**Testing / M8 impact — read before starting.** `-march=native` makes output **host-dependent**, which collides head-on with the two invariants this project leans on: golden byte-identity and the 3-stage self-host fixpoint (they would diverge between an AVX-512 CI runner and an SSE2 one). Therefore: every golden, differential and fixpoint test must pin an explicit `-march=<baseline>` rather than inherit native; the differential fuzz should additionally run per level (`x86-64`, `-v2`, `-v3`) since each level is a distinct codegen path; and `-march=native` needs its own smoke test asserting only that it runs, never byte-identity. Add a `ckconfig`-style guard so a new ISA-dependent optimizer cannot land without declaring its required level.

## Auto-detect `-B`/`-I` dirs when absent from argv — DESIGN BLOCKED on the self-host byte-identity invariant
Wanted: `mcc` should find its own `include`/`runtime`/`win32` dirs when `-B` and `-I` are not given, so a compiler built into an arbitrary directory works out of the box. This is a real papercut — a hand-built or scratch-dir compiler currently reports the failure as something unrelated: `include file 'stddef.h' not found` from the system `stdio.h`, `mccdefs.h not found`, or `_runmain not defined`.

**Attempted 2026-07-27 and reverted twice; the obvious implementation cannot work.** `mcc_auto_mccdir` (`libmcc.c`) probes `<exe>`, `<exe>/..`, `<exe>/../lib/mcc`, then `MCC_CONFIG_MCCDIR`, then `/usr/{local/,}lib/mcc`, using `<base>/include/mccdefs.h` as the marker. Adding a walk-up-from-**cwd** fallback does fix the papercut (verified: a `gcc -m32`-built i386 mcc in a scratch dir compiles with no `-B` and no `-I`, auto-resolving `install: <checkout>/runtime`), and it is byte-identical for any compiler that already resolves. **But it breaks `selfhost-fixpoint` / `selfhost-fixpoint-gates`.** `tools/selfhost-fixpoint.py` passes NO `-B`: stage0 sits in `cmake-debug` and resolves to `cmake-debug/include`, while stages 1-3 are built into a temp dir and previously resolved nowhere — with the fallback they resolve to `<checkout>/runtime` instead. Two different header sets across stages ⇒ `o1 != o2` ⇒ the fixpoint reports "nondeterministic codegen". Gating the fallback on `-B` being absent AND the base being unusable does NOT help: that is exactly the stage1-3 situation. Confirmed by attribution — upstream HEAD passes 2/2, the change fails, revert restores green.

The invariant is the blocker, not the probe: resolving *unresolvable* compilers to some directory necessarily makes them disagree with compilers that resolve elsewhere, and the self-host gate exists to catch precisely that. A workable design has to make the fallback produce the SAME header set the build tree would, rather than "whatever checkout is above the cwd" — e.g. record the producing build dir at compile time and prefer it, or have the fixpoint pass `-B` explicitly so stages are pinned. Note `-B` now ACCUMULATES (upstream 0134d15e), so a future attempt should also check it does not silently add a second base. Also note `runtime/win32/include` has no `mccdefs.h` of its own — the win32 headers LAYER over `runtime/include` (CMakeLists copies both into one install dir), so a single `{B}` cannot express the source-tree win32 layout; that half needs two search prefixes, which `-B` accumulation now makes possible.

## KGC verification refuses almost everything — blocked (found 2026-07-26 by the per-gate sweep)
**KGC verification refuses almost everything — ROOT-CAUSED 2026-07-26, fix landed default-OFF behind `MCC_JIT_PURITY_NOESCAPE`.** `refused-unverified` is not a verification failure: verification never runs. `mccjit_last_kgc_ok = scalar_ok && purity != AST_PURITY_IMPURE` (`mccjit_embed.c`), and `ast_fn_purity` (`mccast.c`) returns IMPURE on the FIRST `AST_Store` or `AST_Invoke` in the arena — so **any local assignment at all** (`int s = 0;`, any accumulator, any `x += y`) or any call disqualifies the whole function. `baseline=(nil)` in the log is a symptom, not a cause: the baseline recompile sits inside that gate and never executes. `probe(7)=-1` is diagnostic only and never affects routing (it also *executes user code with a fabricated argument* under `MCC_JIT_VERBOSE`, which is its own hazard). Other refusal reasons, in rank order after stores/calls: `nparam == 0` (so every `f(void)`, including `main`), `void` return, `float` (only `VT_DOUBLE` counts as FP — `float` is neither GP nor FP), arity > 6, variadic, struct-by-value, bitfields, any `VT_VOLATILE` node.
  `ast_fn_purity_noescape` (`mccast.c`) already implements the right analysis — a store to a non-escaping local is not observable — and its own comment says `ast_fn_purity` "gates KGC memoization and must stay conservative", i.e. the split was always intended. It is now computed into `mccjit_last_purity_ne` and selects the KGC gate under `MCC_JIT_PURITY_NOESCAPE=1`; `memoize_ok` deliberately stays on the strict `ast_fn_purity` (memoizing a TIER1 function that reads globals would serve stale results). Opt-in doubles the verified fraction on a scalar hot loop (2 -> 4 swapped of 13).
  **It is default-OFF because raising the verified fraction ALONE makes the JIT worse, in two independent ways — both measured, and both must be fixed before this can flip:**
  1. **The KGC verify stub itself costs ~9x per call on small hot functions, and only amortizes via memoization — which requires TIER0.** Measured on a scalar hot loop: `MCC_JIT_PURITY_NOESCAPE=1` gives 2.42s; the same run plus `MCC_JIT_NO_KGC=1` (variant installed unverified) gives **0.27s**, vs 0.19s for `MCC_JIT=0`. So the variant is fine — the verification is the cost. Newly-admitted functions are TIER1 (any load), `memoize_ok` requires strict TIER0, so they double-call baseline+variant and hash a tuple on **every** call, forever. Until verification amortizes, widening admission is a straight loss. The fix is NOT "make the variant different": tried routing the sync path through `mcc_jit_recompile_blob_gated` with the intent's baked warm mask (`warm=0xfffffff83f`, so genuinely gated) and the slowdown was unchanged at 2.48s — **and it broke `regression/o4-aot-jit`**, because `mccjit_recompile_common` only fires the backend-submitted AST override under `!mccjit_recompile_use_gates`, i.e. the gated recompile and `MCC_JIT_SUBMIT_AOT` are mutually exclusive by construction. Reverted. Real options: graduate to direct dispatch after N consecutive verified matches (bounded verification — unsound in the limit, but the design already accepts that stance via poisoning); or find a sound memoization key for TIER1; or keep admission TIER0-only.
  **Host parity is now GATED, 2026-07-27 — `jit/run-parity-host`.** The existing `run-parity-<arch>` cells cover only
  the CROSS triples and skip without cross tooling, so on an ordinary host machine nothing checked
  `MCC_JIT=1` == `MCC_JIT=0` at all, despite it being a P0 bar item. The new cell runs each program under `MCC_JIT=0`,
  `MCC_JIT=1`, and `MCC_JIT=1` with WIDENED admission (`PURITY_NOESCAPE`+`LAZY`+`SEARCH`) and requires all three to
  agree — the widened leg being the point, since default admission installs nothing and comparing it to no-JIT is
  nearly vacuous. **Measured while building it: the default gate installs 0 variants on every program; the widened gate
  installs 1 each for `int_mod` and `fp_div_accum`, and 0 for `float_narrow` (a `float` signature is in neither the GP
  nor the FP verified set).** So the cell asserts a corpus-wide install count >= 1; the negative control (dropping
  `MCC_JIT_LAZY`) drives it to 0 and the cell fails.

  On these four shapes the widened gate does NOT break parity — all three legs agree byte-for-byte. That does not
  refute item 2 below, whose reproducer differed; it says the break is shape-dependent, and there is now a gate that
  will catch it if it reappears on this corpus.

  **Trap worth recording, because it nearly shipped: the first version of the non-vacuity guard counted the token
  `verified`, which also matches the REFUSAL message "signature not in the verified GP-int set".** It reported 3
  "verifications" for a run that verified nothing, and would have passed with the JIT fully refusing. The real signal
  is `mccjit-lazy[install]`. Same substring class as `grep faithful` matching `unfaithful`.

  2. **Wider admission exposes near-match acceptance, which breaks `MCC_JIT=1` == `MCC_JIT=0`.** With `MCC_JIT_LAZY=1 MCC_JIT_SEARCH=1` the widened gate changed program output (FP accumulation diverged in the 4th significant digit); conservative gate matched exactly. `MCC_JIT_NEARMATCH` is default-ON and by design KEEPS a variant that mismatches the baseline on a small input set, so admitting more functions admits more divergence. That is a direct P0 parity violation and is the harder of the two.

## Finding WHERE two configurations diverge — use the trace, stop hand-patching printfs (built 2026-07-28)

Every recorder diagnosis in this file was reached the same slow way: hand-patch a temporary `fprintf` into whichever
guard looked suspicious, rebuild, measure, revert, repeat. The instrumentation was thrown away each time, so the next
question started from zero. That was unnecessary — `MCC_CONFIG_TRACE` builds already carry **11,476 trace sites**
(every function entry and every branch, enforced by the `trace-gate-invariant` cell). The information was always
there; what was missing was a way to read it.

**Three changes make it usable, and `tools/tracediff.sh` drives them:**

1. **Scoping** (`mcclog.h`): `MCC_TRACE_FILE` / `MCC_TRACE_FUNC` (substring, comma-list) and `MCC_TRACE_SKIP` (exact
   function names). This matters more than it sounds — on a two-line test program an unscoped `-v128` trace is
   **127,871 lines, 8.4 MB, of which 105,331 (80%) are `mccpp.c` preprocessor churn** that never contributes to a
   codegen divergence. `MCC_TRACE_FILE=mccast` cuts it to 2,467.
2. **Values** (`MCC_TRACE_IF` in `mcclog.h`, used at `ast_hook_vpush`/`vstore`/`vdup`/`cmp_invert`): those four
   verdict-producing hooks now print `r=%#x t=%#x vn=%d rel=%d` instead of a bare `enter`, so the diff shows WHICH
   VALUE decided, not merely which line ran. `MCC_TRACE_IF` evaluates its arguments only when the TRACE bit is
   actually set — a plain `MCC_TRACE` would dereference `vtop` on every hook call in a trace build.
3. **Desync state** (`AST_SET_DESYNC`): the macro recorded only `__LINE__`. It now also emits
   `DESYNC vn=%d inop=%d incall=%d bail=%d`, so a rejected body says why rather than just where.
4. **`MCC_LOG=<mask>` — the LINK PHASE was completely untraceable and nobody had noticed.** `mcc_log_verbose` is
   mirrored from the state in `mcc_enter_state` and **zeroed again in `mcc_exit_state`**, and linking runs outside
   that window. So `-v128` covered parsing and codegen but produced ZERO output from `mccelf.c` (754 trace sites) and
   `mccmacho.c` (319) — 1,073 sites that could never fire, in exactly the object/archive/relocation code where
   format bugs live. `MCC_LOG` is an env-set verbosity FLOOR that `mcc_log_enabled` ORs in, so it survives every
   state transition and covers the whole process. Found while tracing why a Mach-O archive member was not pulled:
   the answer was invisible until the floor existed. Default 0, so nothing changes unless it is set.

**Worked example — this is the exact bug that motivated the tool.** The `MCC_AST_OPASSIGN` staging bug above cost a
hand-instrumented guard, a rebuild and a revert to find. With the tool it is one command:

    tools/tracediff.sh ./cmake-debug/mcc repro.c MCC_AST_OPASSIGN=0 MCC_AST_OPASSIGN=1

    == trace sizes: A=2467  B=4495  (scope: file=mccast)
    == FIRST DIVERGENCE (this is the actionable line)
      src/mccast.c:2528 ast_hook_vdup: enter r=0x100 t=0x9 vn=1 rel=1
     -src/mccast.c:2532 ast_hook_vdup: br
     +src/mccast.c:6569 ast_expr_pure: enter

`r=0x100` is `VT_LVAL`, `t=0x9` is `VT_DOUBLE`, and the two configs split on whether `ast_hook_vdup` bails or runs the
purity check — which is the entire answer, with the deciding values attached.

**Reach for this whenever the question is "why is this body accepted here and rejected there".** It answers
config-vs-config directly (gate on/off, `-O2` vs `-O3`, arch vs arch via `TD_OPT`/extra flags). It does NOT answer
"why is this one run wrong" — there is no second trace to diff against.

Guarded by the `ast/tracediff` ctest cell, which asserts all three properties independently: that scoping still
narrows the trace, that the hooks still print `r=`/`t=`, and that `AST_SET_DESYNC` still dumps state. Each can rot
silently and each would quietly return the workflow to hand-patching. `trace-gate-invariant` (`tools/tracegate.c`)
accepts `MCC_TRACE_IF("enter ...")` as a valid function opener alongside `MCC_TRACE("enter\n")`.

## `unfaithful` is now the LARGEST bucket and was never broken down — first characterisation 2026-07-28

**RE-MEASURED 2026-07-28 after the session's five fidelity fixes — 294 diffs, and the shape of the bucket is not
what a modelling fix can reach.** `MCC_AST_VERIFY_DIFF` over mcc's own TU at `-O2`:

| split | count |
|---|---|
| replay and baseline are the SAME LENGTH | 152 |
| — of those, the diff window is a strict PERMUTATION of the same bytes | **48** |
| replay LONGER than baseline | 67 |
| replay SHORTER than baseline | 75 |

Sampled by hand, the same-length cases are evaluation-ORDER and REGISTER-CHOICE differences, not missing model
detail. `elfsym` does the same `48 8b 00` load three instructions earlier in the baseline than in the replay;
`value64` computes the identical mask-and-subtract through `%rdx`/`%rcx` where the baseline used `%rcx`/`%rdx`;
`host_runmem_free` reorders a `31 c0` zeroing. All three are 197/412/279 bytes on BOTH sides.

**The one fully reduced case is the opposite of a defect: the replay is BETTER.** `(int)(long)h` on a pointer
PARAMETER — `host_file_unlock`'s `int fd = (int)(intptr_t)h - 1` — has the baseline emit a 64-bit load plus the
`shl $32; shr $32` truncation pair, while the replay emits a plain 32-bit load, 9 bytes shorter and semantically
identical:

    base: 48 8b 45 f8  48 c1 e0 20  48 c1 e8 20  83 e8 01  89 45 f4
    repl: 8b 45 f8                               83 e8 01  89 45 f4

Minimal repro: `int f(void *h) { int fd = (int)(long)h - 1; return use(fd); }` is unfaithful, while the same shape
from a `long` parameter (`(int)x`) is faithful — the replay's trace shows both `AST_Convert` nodes present and
applied in order (`CVT t=0x5 -> 0x804`, then `0x804 -> 0x3`), so the model is complete; the two sides simply take
different `gen_cast` paths from the same input. That makes it a CODEGEN lead rather than a recorder one: the parser
is emitting a register truncation where a narrower load would do.

Consequence for anyone planning work here: the `unfaithful` bucket is mostly byte-identity being stricter than it
needs to be, so it will NOT move the way the `desync` bucket did (251 -> 51 with five targeted fixes). Closing a
meaningful part of it needs either a semantic-equivalence check to replace byte equality — which is exactly the
guard that has caught every miscompile in this file, so it is not a trade to make casually — or a codegen change
that makes the PARSER emit what the replay already emits.


At `-O2` on mcc's own TU the split is **1438 faithful / 207 unfaithful / 206 desync** of 1851. Every entry in the
desync table below has a named hook and a count; `unfaithful` had neither, despite being bigger than any single
desync site. It is now instrumented permanently: the faithfulness comparison emits
`UNFAITHFUL <fn> newlen= oldlen= firstdiff= relnew= relold=` under `MCC_LOG=128 MCC_TRACE_FILE=mccast`, so the four
independent arms of that comparison (length, bytes, relocation count, relocation content) stop collapsing into one
word. Measured over 205 captured events:

| failing arm | count |
|---|---:|
| replay emits a DIFFERENT LENGTH | 108 |
| same length, bytes differ | 97 |
| relocation count differs | **0** |
| relocation content differs | **0** |

**Relocations are never the cause.** That is the `MCC_AST_RELOC_EQUIV` flip carrying its full weight — worth knowing
before anyone spends effort there again.

Of the 108 length-differs: **43 replay LONGER, 65 SHORTER, mean |delta| 13.7 bytes**, and the distribution is
dominated by small values — `-5` (12), `+5` (10), `-14` (7), `+2` (6). Of the 97 same-length cases the first differing
byte sits at a **median 54% into the body** (min 4%, max 99%), i.e. replay tracks the parser for half the function and
then diverges; it is not failing at the prologue.

**The ±5 class is dead code after a `noreturn` call, and it shares a root cause with the 92-event
`ast_hook_call_begin`/`nocode_wanted` desync site.** Three-line reproducer:

    extern void bail(const char *) __attribute__((noreturn));
    extern void warn(const char *);
    void a(int c, const char *m) { if (c) bail(m); else warn(m); }  /* UNFAITHFUL, newlen 41 vs 36 */
    void b(int c, const char *m) { if (c) warn(m); else warn(m); }  /* faithful */

On x86_64 five bytes is exactly `jmp rel32`/`call rel32`, and the natural reading is that the parser suppresses the
jump over the else-branch as dead code while replay emits it.

**CORRECTION 2026-07-28 — the `nocode_wanted` MECHANISM is NOT established, only the `noreturn` TRIGGER is.** Two
things went wrong in the first write-up and both are worth recording:

- The original control was confounded. `a` called TWO DIFFERENT functions while `b` called the SAME one twice, so it
  varied `noreturn` AND callee-distinctness together. Re-run with a proper third case:

      void a_noret(int c, const char *m) { if (c) bail(m);  else warn(m); }   /* UNFAITHFUL +5 */
      void a_two  (int c, const char *m) { if (c) note(m);  else warn(m); }   /* faithful    */
      void a_same (int c, const char *m) { if (c) warn(m);  else warn(m); }   /* faithful    */

  `a_two` isolates callee-distinctness and is faithful, so **`noreturn` really is the trigger** — that part survives.

- An attempt to ATTRIBUTE the bucket to `nocode_wanted` failed and was reverted rather than shipped. A per-function
  flag ORed in wherever `nocode_wanted` was observed — first at `ast_hook_vpush` and `ast_hook_call_begin`, then also
  at `ast_hook_stmt`/`ast_hook_if_else`/`ast_hook_if_end`/`ast_hook_if_gvtst_done` — reported **0 for `a_noret`
  itself**, the case already proven positive. It also reported 0 for all 205 TU events, which is therefore
  MEANINGLESS: an instrument that misses a known positive cannot support a negative conclusion. Either
  `nocode_wanted` is already clear by the time any recorder hook runs, or it is not the mechanism at all.

**RESOLVED 2026-07-28 by dumping the bytes** (`MCC_AST_UNFAITHFUL_DUMP=1`, which prints both sequences at the
comparison). The mechanism originally described was RIGHT; only the detector was wrong. For `a_noret`:

    parser: 8b 45 f8  83 f8 00  0f 84 0c 00 00 00  48 8b 45 f0 48 89 c7  e8 ........              48 8b 45 f0 ...
    replay: 8b 45 f8  83 f8 00  0f 84 11 00 00 00  48 8b 45 f0 48 89 c7  e8 ........  e9 0c 00 00 00  48 8b 45 f0 ...
                                        ^^ +5                                        ^^^^^^^^^^^^^^ the extra 5 bytes

The extra five bytes are exactly **`e9 0c 00 00 00` — a `jmp rel32`**, the jump over the else-branch, and the `je`
displacement widens from `0x0c` to `0x11` as a consequence. Source chain, all confirmed: a call to a
`func_noreturn` symbol hits `if (s->f.func_noreturn) ... CODE_OFF();` in `mccgen.c` (`gen_function`'s call path),
`CODE_OFF()` sets `nocode_wanted`, and the if/else statement code's `gjmp` (`x86_64-gen.c`, `gjmp2(0xe9, t)`)
therefore emits nothing. Replay does not carry that dead-region state, so it emits the jump.

**Why the first flag-sampling detector read 0 on a known positive:** `CODE_OFF()` fires at the CALL SITE in
`mccgen.c`, and the suppressed `gjmp` is emitted by the STATEMENT code with no recorder hook in between — so sampling
`nocode_wanted` at `ast_hook_vpush`/`call_begin`/`stmt`/`if_else`/`if_end` observes it at none of those points.
Sampling at **`ast_hook_call_end()`**, which sits directly after that `CODE_OFF()`, DOES report 1 for `a_noret`. That
detector is validated (positive on `a_noret`, negative on `a_two`/`a_same`) and is kept.

**ATTRIBUTION MEASURED — and it is 0. `noreturn` explains NONE of mcc's real unfaithful bucket.** With the validated
detector, **0 of 205** unfaithful functions on mcc's own TU show `nocode_wanted` at all: 0 of the 108 length-differs
and 0 of the 97 byte-differs. This RETRACTS the speculation in the first write-up that `nocode_wanted` might cover
"roughly 300 of the 413 non-faithful functions" — it covers the 92-event desync site and, on this TU, nothing in the
unfaithful bucket.

**The recorder's dead-branch handling is NOT broadly broken; only the noreturn-call path is.** Measured:

| shape | verdict |
|---|---|
| `if (c) bail(m); else warn(m);` (noreturn call) | **unfaithful, +5** |
| `if (c) return; else warn(m);` | faithful |
| `if (c) return v; else v++;` | faithful |
| `if (c) goto e; else warn(m);` | faithful |
| `if (c) warn(m); else warn(m);` | faithful |

So `return`/`goto`-terminated branches already replay correctly. Do not generalise the noreturn finding to
"terminating branches" — that was tried and refuted.

**The real ±5 is the SAME construct — and that makes the "0 of 205" figure self-contradictory.** Widening the dump
(`MCC_AST_UNFAITHFUL_DUMP=<bytes>`, now a byte count rather than a flag) and aligning the two sequences shows
`mcc_pedantic` diverging at absolute offset **480**, where replay emits **`e9 63 00 00 00`** — a `jmp rel32`, exactly
the construct the synthetic `a_noret` case produces. The earlier `je` displacement difference (`0x51` vs `0x56`) is
just its consequence.

And `mcc_pedantic`'s source is precisely the noreturn shape:

    if (mcc_state->pedantic_errors) mcc_error("%s", msg);
    else                            mcc_warning_c(warn_pedantic)("%s", msg);

`mcc_error` is `#define mcc_error MCC_SET_STATE(_mcc_error)` and `_mcc_error` is declared
`PUB_FUNC NORETURN void _mcc_error(...)`. So this call site SHOULD have set `func_noreturn` → `CODE_OFF()` →
`nocode_wanted`, and the validated detector SHOULD have reported 1 for it. It reported 0.

**RESOLVED — and the "0 of 205" was MY ANALYSIS SCRIPT, not the compiler.** The `awk` that produced it read the
nocode field as `$7`, which is `relold=`; the field is `$8`. `split("relold=792", n, "=")` never yields `1`, so every
function classified as nocode=0. The very run that reported "0 of 205" contains the line

    UNFAITHFUL mcc_pedantic newlen=584 oldlen=579 firstdiff=395 relnew=792 relold=792 nocode=1

i.e. the contradiction was visible in the output being summarised. Two commits reported that 0 before it was caught.

**Correct attribution, `$8`:**

| | nocode SEEN | nocode NOT seen |
|---|---:|---:|
| length-differs | **19** | 89 |
| bytes-differ | **5** | 92 |

**24 of 205 (12%)** of the unfaithful bucket involves `nocode_wanted`. Not 0, and not the "roughly 300 of 413" the
first write-up speculated. Corroborating evidence that the detector was always working: instrumenting the
`CODE_OFF()` site itself shows it firing **402 times** over the TU, and `ast_hook_call_end` observes
`nocode_wanted == 0x20000000` (`CODE_OFF_BIT`) **569 times** — 360 in bodies that end `desync`, 209 with the recorder
still live. So noreturn call sites are recognised and the flag is seen; most simply land in `desync` or stay
`faithful` rather than in this bucket.

**The 89 non-nocode length-differs are NOT one phenomenon.** Delta profile: `-5` (11), `-14` (7), `+2` (6), `-2` (5),
`-19` (5), `-6` (4), `-3` (4), `-28` (3). Dumping two of the eleven `-5` cases (where replay is SHORTER, the opposite
direction from the noreturn `+5`) already shows two different causes:

- **`cst_hook_begin`** — the parser emits five bytes that replay does not: **`b8 00 00 00 00`**, i.e. `mov eax, 0`.
  That is not a jump at all; it is a constant materialised into `eax` that the replay's model drops. Worth chasing
  first, because "the model folds a constant the parser kept" is a modelling difference rather than a control-flow
  one and is likely to be systematic.
- **`end_macro`** — within the dumped window only the `je` displacement differs (`0x54` vs `0x4f`); the actual
  five-byte difference is further along, the same shape as `mcc_pedantic`.

So do not look for a single fix here. Use `MCC_AST_UNFAITHFUL_DUMP=<bytes>` per function; the window centres on
`firstdiff`, and for these functions `firstdiff` (133–255) is well past the prologue, so a small window shows only
the consequence (a shifted branch displacement) rather than the cause.

### CHAINED ASSIGNMENT loses its RHS materialisation in replay — ALREADY DOCUMENTED IN `ast_hook_vstore`

Chasing the `cst_hook_begin` `-5` case above led straight to its source line, `cst_lcount = cst_scount = cst_sstop = 0;`
— a chained assignment. Minimal reproducer and verdicts:

    static int a, b, c;
    void f1 (void)  { a = 0; }            /* faithful    */
    void f2 (void)  { a = b = 0; }        /* UNFAITHFUL  */
    void f3 (void)  { a = b = c = 0; }    /* UNFAITHFUL  */
    void f3v(int v) { a = b = c = v; }    /* UNFAITHFUL  */

A single store is fine; **any chain of two or more is not**, and it is not sensitive to whether the RHS is a constant
(`MCC_AST_CHAINSTORE=0` changes nothing, so this is not that gate). The bytes for `f2`:

    parser (17): b8 00 00 00 00   89 05 ....   89 05 ....    mov eax,0 ; mov [b],eax ; mov [a],eax
    replay (12):                  89 05 ....   89 05 ....                mov [b],eax ; mov [a],eax

**Replay emits both stores but never materialises the value into the register.** The replayed body would store
whatever happened to be in `eax`. It is correctly rejected — the always-on comparison catches it, which is exactly
why replay bugs cost coverage rather than correctness — but the model is losing the RHS value production, not merely
ordering it differently.

**This was NOT a new discovery — the cause is already stated in the `MCC_AST_CHAINSTORE` comment in
`ast_hook_vstore`**, which says the gate "does NOT make the chained-assignment idiom faithful; that has a separate
cause (the parser materialises the value once and chains two vstores, the replay emits two independent stores)."
That is the same root cause, written down before this investigation started. What is added here is only the
evidence: the two-line reproducer, the verdict table showing a chain of TWO already suffices and that it is
RHS-independent, and the byte-level confirmation that the missing bytes are precisely the value materialisation.

That makes twice in this session that the answer was already in a code comment — the other being
`MCC_AST_OPASSIGN`, whose header comment even named nbody's `advance()` as the victim. **Read the gate comment for
the construct before investigating it.**

This is the `AST_StoreVal` / assignment-as-value area (F3a): the chain's inner store is consumed as a VALUE by the
outer store, and that value is what goes missing. Anyone fixing it should re-check `assign_value_effects.c`, which
already pins evaluation counts for `a = b = f(v)` and would catch a fix that duplicates the RHS instead of dropping
it.

**Mechanism notes gathered 2026-07-28 — read these before attempting a fix; the accounting does NOT close yet.**
- `AST_FB_STORE_VALUE_LIVE` is set by `ast_finalize_storevals` ONLY when the marker is the LEFTMOST leaf of the
  statement immediately following the store. In `a = b = 0` the marker is the outer store's VALUE (second child)
  while the leftmost leaf is the lvalue `a`, so the flag is never set for the plain chained idiom, and replay's
  `AST_StoreVal` case takes its fallback branch.
- That fallback re-emits the inner store's RHS, which would make replay LONGER. The observed replay is SHORTER — 12
  bytes against 17, missing exactly one `mov eax,0` — so the fallback is evidently not what runs either.
- `ast_hook_vstore` resolves a marker back to the inner store's RHS at RECORD time (the `mkr` block), so the outer
  store's value child ends up being the same `Literal` node the inner store already owns — a node with two parents,
  since `ast_add_child` reparents without unlinking. Which child chain replay walks decides whether it is emitted
  once, twice or never.
- **ANSWERED 2026-07-28: replay visits the shared node TWICE.** An `RV n=… kind=… parent=…` trace in
  `ast_replay_value` gives, for `a = b = 0`:

      RV n=2 kind=Ref     parent=4      b
      RV n=3 kind=Literal parent=6      0
      RV n=1 kind=Ref     parent=6      a
      RV n=3 kind=Literal parent=6      0   <- the SAME node again

  So the `Literal` really is reached from both stores' child chains, and its `parent` records only the outer store.
  Two visits ought to make replay LONGER, yet replay is 12 bytes against the parser's 17 — resolved below.
- **ACCOUNTING CLOSED 2026-07-28.** Adding `ind` (code offset) and vstack depth to the same trace:

      RV n=2 Ref     ind=11 vtop=-1     b        emits nothing
      RV n=3 Literal ind=11 vtop= 0     0        emits nothing
                                                 ... store -> ind 17   (6 bytes)
      RV n=1 Ref     ind=17 vtop=-1     a        emits nothing
      RV n=3 Literal ind=17 vtop= 0     0        emits nothing
                                                 ... store -> ind 23   (6 bytes)

  Body = 12 bytes, and **neither `Literal` visit emits anything**. Each store emits the 6-byte `mov %eax,disp32`
  REGISTER form, so replay stores from `eax` without ever loading it — the replayed body would write whatever
  happened to be in the register, which is what "loses the RHS materialisation" means concretely.

  **Root cause: `ast_finalize_leaf(value, vtop)` finalises the chained store's value leaf AFTER the parser has
  already materialised it, so the leaf records "this value lives in register `eax`"** — true at record time, never
  reproduced at replay. The single-store control (`a = 0`) is faithful because there the parser's materialisation
  and the leaf agree. The existing `MCC_AST_CHAINSTORE` comment says the same thing from the other direction: "The
  copy is finalized with vtop, i.e. it records 'the value is in the register the inner vstore left it in'."

  So the fix is to record the chained value as the RHS EXPRESSION rather than as the register it happened to land
  in. That is a model-shape change in the exact place `emit-at-marker` miscompiled, so it needs
  `assign_value_effects.c` plus the full bar — but the mechanism is no longer guesswork.

- **The recorded leaf forms, measured 2026-07-28 (`LEAF n= r= t= ival=` trace in the `AST_Literal`/`AST_Ref` replay
  case), pin it exactly:**

  | case | value leaf `r` | `t` |
  |---|---|---|
  | `a = 0` (faithful) | `0x30` = `VT_CONST` | `0x3` = `VT_INT` |
  | `a = b = 0` (unfaithful) | `0` = **register `eax`** | `0x3003` — **the LVALUE's type** |

  The chained value leaf is recorded as register-resident AND carries the lvalue's type rather than the constant's,
  so it was finalised against the wrong `SValue` altogether — not merely against a materialised copy of the right
  one.

- **ATTEMPTED AND REJECTED: skipping `ast_finalize_leaf(value, vtop)` when the value came from the marker
  (`if (!mkr)`).** No effect — all four reproducer verdicts unchanged. Node 3 already carries `r=0` before the outer
  store runs, so the damage is done by the INNER store's own finalize. Reverted; nothing kept.

- **THE CONTROL THAT EXPLAINS IT — `b = 0; a = 0;` as two statements is FAITHFUL**, and the parser emits the SAME
  `mov $0x0,%eax` materialisation there. So "the parser materialised it, the recorder truthfully wrote that down"
  is NOT the explanation; that reading was about to be adopted and the control refutes it. Side by side:

  | | leaves | `r` | `t` |
  |---|---|---|---|
  | `b = 0; a = 0;` (faithful) | **two distinct** Literals | `0x30` `VT_CONST` | `0x3` `VT_INT` |
  | `a = b = 0` (unfaithful) | **one shared** Literal, visited twice | `0` register | `0x3003` the lvalue's |

  `b = 0, a = 0` (comma) is faithful too.

  **Why they differ:** in the two-statement form the inner value is DISCARDED, so nothing forces materialisation
  before `ast_hook_vstore` runs — the leaf is still `VT_CONST` when finalised, and the parser's `mov $0,%eax` is
  emitted afterwards by `vstore`'s own codegen. In the chained form the value is NEEDED by the outer assignment, so
  `gv()` materialises it into a register BEFORE the hook, and the finalize then captures the register instead of
  the constant.

  The obvious reading is that the recorder should capture the leaf's form BEFORE materialisation. **That was tried
  and it is NOT sufficient — see below. This needs a model-shape change after all.**

- **THE OVERWRITE, CAUGHT IN THE ACT.** A `FIN n= k= had_op= <- sv_r=` trace in `ast_finalize_leaf`:

      chained    FIN n=3 k=Literal had_op=0x30 <- sv_r=0x30      (inner store: constant kept)
                 FIN n=3 k=Literal had_op=0x30 <- sv_r=0         (outer store: CONSTANT OVERWRITTEN BY REGISTER)
      separate   FIN n=2 k=Literal had_op=0x30 <- sv_r=0x30
                 FIN n=6 k=Literal had_op=0x30 <- sv_r=0x30      (both stay constant)

  So the leaf DOES hold `VT_CONST` and the outer store's finalize is what replaces it with register 0. (This also
  corrects the note above that blamed the inner store's finalize — the inner one is fine; it is the second visit,
  from the outer store, that overwrites.)

- **ATTEMPTED AND REJECTED (2nd attempt): preserving the constant form** — skip the overwrite in
  `ast_finalize_leaf` when the node already holds a non-symbolic `VT_CONST` and the incoming `sv->r` is a register.
  All four reproducer verdicts unchanged. The reason is now clear and it is not a bug in the guard: the parser
  materialises the value and emits the 6-byte register store `mov %eax,disp32`, so a replay that keeps the constant
  emits the 10-byte immediate form `movl $0,disp32` instead. **Both directions are unfaithful** — keeping the
  register loses the materialisation, keeping the constant changes the store encoding.

  **Conclusion: the model cannot express this with a single leaf.** It has to represent BOTH "the value is this
  constant" AND "materialise it into a register here, once, before the first store" — which is the model-shape
  change `emit-at-marker` attempted and miscompiled at `-O2`/`-O3`. Two independent narrow fixes have now been
  tried and rejected on evidence, so do not attempt a third narrow one; the next attempt should be the shape change,
  with `assign_value_effects.c` and the full bar.

**Do NOT fix this by making the marker emit at its use site.** That was tried earlier in this session (F3a,
"emit-at-marker"): correct at `-O0`/`-O1`, MISCOMPILED at `-O2`/`-O3`, and caught only because
`assign_value_effects.c` counts evaluations rather than checksumming results.

Contrast with the sign extension fixed in `b0fb11d5`, where the parse-vs-replay trace closed cleanly and the fix was
a parser-side synth-suspend bracket with no change to model shape. Here it does not close, so the same confidence is
not available.

**MEASURED 2026-07-28: 45 of 205 (22%).** A `chain=` field now rides the `UNFAITHFUL` trace line, set in
`ast_hook_vstore` whenever the store's value is an `AST_StoreVal` marker or already has a parent — i.e. the
syntactic chain, independent of whether `MCC_AST_CHAINSTORE` is on. Validated before use on both the synthetic
reproducer (`f2`/`f3`/`f3v` → `chain=1`) AND a member of the real population (`cst_hook_begin` → `chain=1`), and the
summarising script was checked against a raw line first (`chain` is `$9`) — the two failures that produced three
retracted numbers earlier in this section.

| | count | share of 205 |
|---|---:|---:|
| chained assignment present | **45** | 22% |
| `nocode_wanted` present | 24 | 12% |
| both | 6 | |
| **neither** | **142** | **69%** |

**The two causes are cleanly separated by symptom: all 45 chained cases are LENGTH-differs, and ZERO of the 97
byte-differs are chained.** That follows from the mechanism — a dropped value materialisation removes bytes, it does
not alter bytes in place — and it is a useful filter: when triaging a same-length divergence, chained assignment is
already excluded.

Where that leaves the two halves:
- **108 length-differs**: 45 chained + 19 nocode (6 overlapping) → 50 unattributed, now partly characterised
  (2026-07-28, same validated decoded-instruction tooling, zero `(bad)` decodes):
  - replay is SHORTER in 37 of the 50 and longer in 13.
  - **24 of 50 (48%) have a delta consisting ENTIRELY of mov-family instructions** (`mov`/`movq`/`movl`/`movzbl`/
    `movslq`), i.e. spill and reload traffic — the same register-allocation family as the 90-of-92 byte-differs,
    except that here the differing allocation changes the instruction COUNT rather than just the encoding. Parser-only
    instructions across the group are dominated by `movq K(R),R` (29), `mov K(R),R` (21), `mov R,K(R)` (21),
    `movq R,K(R)` (16), against far fewer replay-only ones — so the parser generally spills more.
  - The remaining 25 have non-mov differences and are the genuinely unexplained remainder. Their non-mov delta
    mnemonics: `xorb` 16, `movabs` 6, `add` 6, `cmp` 5, `call` 5, then `shl`/`shr`/`xor`/`jmp`/`lea` in ones and
    twos. `xorb` is NOT an anomaly despite the count: it is spread 1–2 per function across `foldm_hypot`,
    `foldm_atan`, `foldm_tgamma` and `ast_fc_gp`, i.e. math-folding paths, not a single construct. (Checked
    precisely because 16 looked concentrated.)
  - **Four functions have a `call` in the delta, which is the only remaining place in the bucket where the two sides
    might do genuinely different work.** `host_runmem_alloc`, `mcc_define_symbol` and `mcc_preprocess` have replay
    emitting an EXTRA call; `store_packed_bf` has the parser emitting TWO extra calls that replay does not.

    **The call deltas are REAL, confirmed independently by relocation counts.** The `relnew`/`relold` byte counts
    differ by exact multiples of 24 — the ELF64 `Rela` size — and the multiples match the disassembly exactly:

    | function | reloc bytes | Δ | calls |
    |---|---|---:|---|
    | `host_runmem_alloc` | 624 vs 600 | +24 | 1 extra in replay |
    | `mcc_define_symbol` | 432 vs 408 | +24 | 1 extra in replay |
    | `mcc_preprocess` | 2736 vs 2664 | +72 | 3 extra in replay |
    | `store_packed_bf` | 1176 vs 1224 | −48 | 2 extra in parser |

    Two independent signals (decoded instructions and relocation bytes) agreeing means one side genuinely emits
    calls the other does not.

    **INLINE-vs-CALL was the obvious explanation and it is REFUTED:** `MCC_AST_INLINE=0` leaves all of them
    `unfaithful`, so the parser is not simply inlining a helper that replay calls. (That gate is one inlining
    mechanism, not all of them — `foldm_*` math folding and `-fc99-inline-body` were not separately excluded.)

    **TARGETS RESOLVED 2026-07-28** by dumping the relocation symbol names at the comparison
    (`[unfaithful-rel]`, same `MCC_AST_UNFAITHFUL_DUMP` gate):

    | function | difference |
    |---|---|
    | `host_runmem_alloc` | replay-only call to **`host_pagesize`** |
    | `mcc_define_symbol` | replay-only **`strchr`** |
    | `mcc_preprocess` | replay-only **`get_tok_str`** (plus `tok`/`tokc` data references) |
    | `store_packed_bf` | parser-only **`vdup`, `gv_dup`** |

    **RESOLVED 2026-07-28 — all three are the DOCUMENTED call-argument limitation of `AST_StoreVal`, not a new
    defect.** They are assignment-used-as-a-value in an argument position (`mcchost.c:1557` is
    `ptr = mcc_malloc(size += host_pagesize());`), and replay re-evaluates the RHS including the call. Reproducer
    with relocation symbols, which show the doubled call directly:

        void opassign_arg (void) { use(s += g()); }   UNFAITHFUL   parser: g s s use   replay: g s s g s use
        void assign_arg   (void) { use(s = g());  }   UNFAITHFUL   parser: g s use     replay: g s g use
        void opassign_stmt(void) { s += g(); }        faithful
        void plain_arg    (void) { use(g()); }        faithful

    Assignment as a STATEMENT is faithful; assignment as a VALUE in an argument is not. That is exactly what
    `ast_finalize_storevals`'s leftmost-leaf guard is documented to do — its own comment says "A call argument fails
    this (gfunc_call pushes around it) and keeps the old, unfaithful behaviour" — and the `AST_StoreVal` replay case
    says the fallback is "the pre-F3a double evaluation, which is merely unfaithful". So the behaviour is known,
    deliberate, and safe: the body is rejected, and `assign_value_effects.c` separately pins that the PARSER
    evaluates these exactly once.

    This is the same assignment-as-value family as the chained-store gap above, and any fix would be the same
    model-shape change.

    **`store_packed_bf` — RESOLVED 2026-07-28. A ternary whose VALUE IS DISCARDED records NOTHING.** Its source
    contains `c ? vdup() : gv_dup();` as a statement, and the recorder models no nodes for that at all, so replay
    emits neither call. Reproducer:

        void t_void   (int c) { c ? a()  : b();  }   /* verdict: EMPTY    */
        void t_intdisc(int c) { c ? ai() : bi(); }   /* verdict: EMPTY    */
        int  t_int    (int c) { return c ? ai() : bi(); }  /* faithful   */
        void t_if     (int c) { if (c) a(); else b(); }    /* faithful   */

    A ternary whose value is USED is faithful, and the equivalent `if`/`else` is faithful; only the discarded-value
    form is lost. When it is the whole body the verdict is `empty`; when it sits inside a larger function — as in
    `store_packed_bf` — the body comes out `unfaithful` with the arms' side effects missing from the replay.

    **No correctness risk today**, and this was checked rather than assumed: `empty` is produced in the ELSE branch
    of `if (ast_fn_faithful)`, so an empty body is not faithful and every consumer is gated off it, exactly like
    `desync`. There are also ZERO `empty` verdicts across the TU's 1851 functions, so the construct never forms a
    whole body in mcc's own source — it only degrades the functions that contain it.

    That leaves 3 of the 4 call-deltas (`host_runmem_alloc`, `mcc_define_symbol`, `mcc_preprocess`), all of the
    replay-recomputes-a-value kind.

    Caveat on reproducing this: `ast_relsym_name` carries its own `MCC_TRACE`, which interleaves into the middle of
    the dump line. Use `MCC_TRACE_SKIP=ast_relsym_name`.

  Taken with the byte-differ result, the picture across the whole bucket is consistent: most of what the
  faithfulness check rejects is allocation and scheduling difference rather than wrong modelling. Chained assignment
  and the lost sign extension remain the only two places where replay would compute something different.
- **97 byte-differs**: 5 nocode, 0 chained → 92 unattributed, the largest unexplained group. **First look at two of
  them, 2026-07-28: these appear to be ORDER differences, not modelling errors.**

  `mcc_mulhs64` (85 bytes, `firstdiff=3`) — identical instruction count, same operations, different operand/register
  ordering in call-argument setup:

      parser: mov rax,[rbp-8] ; mov rcx,[rbp-16] ; mov rsi,rcx     ; mov rdi,rax ; call
      replay: mov rax,[rbp-16]; mov rsi,rax      ; mov rax,[rbp-8] ; mov rdi,rax ; call

  and further on a `sub` where the parser accumulates into `rcx` while replay accumulates into `rax`
  (`48 29 c1` vs `48 29 c8`), again same length.

  `host_nproc` — BASIC-BLOCK ordering: the parser emits `jle +8 ; mov eax,[rbp-8] ; jmp ; mov eax,1`, replay emits
  the two arms in the opposite order with correspondingly different displacements.

  **If this generalises, the 92 are not model bugs at all** — the model is right and replay is emitting a valid
  alternative encoding, which the byte-identity check rejects because it is stricter than semantic equivalence. That
  reframes the work: the fix would be making replay reproduce the parser's ORDERING deterministically (argument
  evaluation order, which register receives an accumulate, which arm is laid out first), not repairing what the
  model records. It also means this group is unlikely to be hiding a correctness bug, unlike a dropped value
  materialisation.

  **CLASSIFIED 2026-07-28 by DECODED instruction multiset: 90 of 92 are reorder or register-allocation only.**

  | | count |
  |---|---:|
  | same instruction multiset (pure reorder) | **40** |
  | differ, but identical once register NAMES are canonicalised | **50** |
  | genuinely different instructions | **2** |

  So the group is what the two hand-read cases suggested: the model is right and replay emits the same computation,
  differing only in instruction ORDER and REGISTER CHOICE. Byte-identity rejects it because it is stricter than
  semantic equivalence. Fixing it means making replay reproduce the parser's scheduling and allocation, not
  repairing what the model records — and this group is very unlikely to hide a correctness bug.

  **Three artifacts had to be cleared to get this number, all of which produced confident wrong answers first:**
  1. `llvm-objdump` has no `-b binary` (that is GNU `objdump`). Every invocation errored, stdout was empty, both
     sides compared as empty lists, and the classifier reported **92 of 92 identical**. Caught only by a positive
     control — `cst_hook_begin` provably has an extra instruction and was reported SAME.
  2. With GNU `objdump` it reported 32/60, but the dump window started at `firstdiff - 8`, which is not an
     instruction boundary, so the disassembly was misaligned — visible as `(bad)` opcodes and nonsense operands like
     `add %ah,K(%rax,%rcx,2)`. The dump now starts at offset 0 whenever the requested window covers the body; the
     re-run has **zero `(bad)` decodes**.
  3. Comparing raw BYTE multisets (an earlier attempt) cannot separate these at all: a register swap plus the
     displacement changes that follow a reordered block move the byte multiset 13–14%, which is why
     `mcc_mulhs64` and `host_nproc` — both hand-verified reorders — landed in that attempt's "structurally
     different" bucket.

  The final classifier is validated in both directions: zero `(bad)` decodes, and `cst_hook_begin`/`end_macro`/
  `mcc_pedantic` all still report DIFFERENT after register canonicalisation, with `cst_hook_begin`'s parser-only
  instruction coming out as `mov $K,R` — the `mov eax,0` identified by hand.

  **THE 2 GENUINELY-DIFFERENT ONES ARE A REAL SEMANTIC BUG — a lost sign extension.** `imm_ext` and `modrm` (both in
  `x86_64-dis.c`) differ identically: parser emits `movslq`, replay emits `movzbl`. Both contain
  `v = (signed char)get8(d);` — a narrowing cast to a SIGNED narrow type stored into a wider variable. Minimal
  reproducer and verdicts:

      long long s_char (void) { long long v; v = (signed char)g8();   return v; }  /* UNFAITHFUL */
      long long s_short(void) { long long v; v = (short)g8();         return v; }  /* UNFAITHFUL */
      long long u_char (void) { long long v; v = (unsigned char)g8(); return v; }  /* faithful   */
      long long plain  (void) { long long v; v = g8();                return v; }  /* faithful   */

      parser: call ; movzbl %al,%eax ; movsbl %al,%eax ; movslq %eax,%rax ; mov %rax,-8(%rbp)
      replay: call ; movzbl %al,%eax ; movsbl %al,%eax ; movzbl %al,%eax  ; mov %rax,-8(%rbp)

  **Replay re-does the byte ZERO-extend instead of the int→`long long` SIGN-extend**, so for any negative value the
  replayed body computes a different result. It is harmless today only because the always-on comparison rejects it —
  but that makes it a landmine: anyone who "fixes" `imm_ext`/`modrm` into faithfulness without fixing the extension
  ships a miscompile. Signed narrow casts (`signed char`, `short`) are affected; unsigned ones are not.

  This is the ONLY correctness-relevant defect found anywhere in the 205-function unfaithful bucket, and it is worth
  fixing on its own merits rather than for the 2 functions of coverage it buys.

  **It is NOT x86_64-specific — it is in the shared model/replay path.** Same reproducer across the cross compilers:

  | target | `(signed char)` | `(unsigned char)` |
  |---|---|---|
  | x86_64 | unfaithful | faithful |
  | arm64 | unfaithful | faithful |
  | riscv64 | unfaithful | faithful |
  | i386 | unfaithful | faithful |
  | arm (32-bit) | **faithful** | faithful |

  Four of five targets reproduce it, so look in `ast_hook_convert` / the `AST_Convert` replay case, not in a backend.
  arm32 is the outlier and is worth a glance when fixing — either its `gen_cast` happens to emit the same sequence
  for both source types, which would mask the same modelling error, or it genuinely records the chain correctly.

  **PINPOINTED 2026-07-28: the second Convert node carries the WRONG TARGET TYPE.** A `CVT from t=%#x -> t=%#x`
  trace in the `AST_Convert` replay case (kept, `MCC_TRACE_IF`-gated) gives, with `0x31` = `unsigned char`
  (`VT_BYTE|VT_UNSIGNED|VT_DEFSIGN`) and `0x21` = `signed char`:

  | | conversion 1 | conversion 2 |
  |---|---|---|
  | `s_char` | `0x31 → 0x21` (correct: the cast) | `0x21 → 0x31` — **should be `long long`** |
  | `u_char` | `0x31 → 0x31` | `0x31 → 0x31` |

  So the widening to `long long` is never represented; the second recorded Convert instead targets `unsigned char`,
  the SOURCE expression's type. For `u_char` that is a no-op and coincidentally byte-identical, which is why the
  unsigned form is faithful — **the same defect is present there, merely invisible.**

  **FIXED 2026-07-28. The suspect was confirmed and the fix is one synth-suspend bracket.** Marking calls that
  originate inside `force_charshort_cast` and tracing every `ast_hook_convert` showed the parse recording
  `0x21 → 0x4` (signed char → `long long`) directly, while replay produced `0x21 → 0x31` then `0x31 → 0x4` — the
  extra step coming from a Convert node recorded by `force_charshort_cast`'s OWN internal `gen_cast` (the line with
  `charshort=1`). That inner cast is an implementation detail of the outer `gen_cast`, which replay re-triggers by
  itself, so recording it as a user-visible Convert corrupts the chain.

  Fix: bracket the inner `gen_cast_s` with `ast_hook_synth_begin()`/`ast_hook_synth_end()` — the same idiom already
  used for the 32-bit `gen_cast` path, the `sizeof` operand and static-storage initialisers, and the one that works
  because it suspends at the PARSER where no model state is owed.

  Result: `s_char`, `s_short`, `u_char` and the no-cast control are all now faithful, and TU fidelity goes
  **1438 → 1443 faithful, 207 → 204 unfaithful** (desync unchanged at 206). Correctness verified by EXECUTION, not
  just by the verdict flipping: `(signed char)0x80 → -128`, `(signed char)0xff → -1`, `(signed char)0x7f → 127`,
  `(short)` sign extension, and the unsigned control, all correct at `-O0/-O1/-O2/-O3` and matching gcc `-O2`. That
  matters because these bodies now REACH the optimizer, where a wrong model miscompiles instead of merely being
  rejected.

  Bar: host ctest 7281/7281, cross ctest 7440/7440, self-host fixpoint `s3 == s4` byte-identical.

  `MCC_AST_REPLAY_DUMP=1` does not help here — it only fires for bodies that came out faithful.

  The rest is NOT classified, and the attempted threshold failed a check that should be recorded. Bucketing the
  remaining 62 by "≤10% of bytes differ" (46) versus ">10%" (16) does NOT separate reorder from structurally
  different: `mcc_mulhs64` and `host_nproc` — the two cases READ BY HAND above and confirmed to be an operand-order
  and a block-order difference — score 14% and 13%, landing in the supposedly "structurally different" group. A
  register swap (`48 29 c1` → `48 29 c8`) and the displacement changes that follow a reordered block move the
  multiset far more than the ordering itself does, so the metric measures the wrong thing at that resolution.

  Do not treat the 16 as a shortlist of real bugs; that reading was checked against two known-good cases and failed.
  A useful classifier needs to compare DECODED instructions (opcode + operand roles) rather than raw bytes, or to
  canonicalise displacements before comparing. Until then: 30 confirmed pure reorders, 62 unclassified, and the
  two-case hypothesis that the group is dominated by ordering stands unrefuted but unmeasured.

**Standing caution, earned three times in this section.** A `0` from an instrument is not a finding until the
instrument has been shown to produce a `1` on a known positive DRAWN FROM THE SAME POPULATION, and until the
summarising script has been checked against a raw line of its own input. The first hook-sampling detector genuinely
was broken (it read 0 on `a_noret` itself); the `ast_hook_call_end` one was fine and was wrongly blamed for two
commits because the awk above sat on top of it. Print one raw record next to the summary before believing either.

Fixing this means the recorder must model "this branch ends unreachable" so replay suppresses the same jump. That is
the same dead-region state the 92-event `ast_hook_call_begin`/`nocode_wanted` desync site gives up on, so one model
would serve both.

**NOT established, and do not assume it:** what SHARE of the real 108 this accounts for. 32 of 108 have a delta that
is a multiple of 5, which is suggestive but not proof — a crude source scan for noreturn calls in the named functions
(`end_macro`, `skip_to_eol`, `tok_print`, `mccgen_finish`) came back empty, and that scan was too loose to trust in
either direction. Someone should attribute the remaining 76 non-multiple-of-5 deltas before treating `nocode_wanted`
as the single explanation for this bucket.

The payoff if it does hold: `nocode_wanted` would account for the 92-event desync site AND a large part of the
207-function unfaithful bucket, making it by far the highest-value remaining modelling target — roughly 300 of the
413 non-faithful functions rather than the 92 the desync table shows on its own.

## NEXT WORKSTREAM — user-prioritized 2026-07-28, in this order

**STATUS after the 2026-07-28 session: D1a DONE. B1a two-of-three DONE. A2a narrowed to one target. A1a not started.**

| item | state | what landed |
|---|---|---|
| D1a gate staging audit | **DONE** | 3 gates re-staged: `OPASSIGN`→`-O2` (`6acb9e69`), `CHAINSTORE`→`-O2` (`e81035e5`), `PROMO_ARROW`+`PROMO_INCDEC`→`-O2/-O3` (`e052542a`) |
| B1a assignment-as-value | **2 of 3** | statement chain fixed as a side effect of D1a; discarded ternary fixed (`8e867f40`); **call-argument path open, experiment specified** |
| A2a vstack SYNC | **60 -> 4** | site proven to be a DETECTOR (`026233f8`); unaccounted pops fixed in `switch` (`1939ba28`), `if`/`while`/`do`/`for` (`0c4f6b33`), short-circuit attributed to its own hook (`d6aa9fcd`), atomics bailed + verdict ordering fixed; the 4 left are a second `switch`-path pop, see below |
| A1a `nocode_wanted` | not started | 3 approaches ruled out; hardest item in the file |

Measured effect of the above at `-O2`: nbody 0.49s → 0.36s, spectral-norm 0.55s → 0.26s, matmul 2.21s → 2.07s.
Recorder fidelity 75.3% → 78.1%, and one real correctness defect (a lost sign extension) found and fixed
(`b0fb11d5`).

### AGREED PLAN — user-chosen 2026-07-28 (second round), in this order

**Primary, all four, in order:**
1. **A1a — fix the `vpop` imbalance** (SYNC Group A, 12 functions). The only item anywhere in this file with a
   fully SOLVED mechanism awaiting a fix: inside a `&&`/`||` region `ast_hook_vpop` early-returns without
   `ast_vn--` while codegen pops the vstack. See the A2a section for the verbatim trace.
2. **A2a — chase the `call_end` lead** (SYNC Group B, 31 functions). `ast_hook_call_end` immediately precedes 13 of
   the 31; consecutive pushes account for 9 and a prior guard-decline for 8. Narrow to a mechanism BEFORE any fix.
3. **B1a — widen the leftmost-leaf guard for call arguments.** Evidence complete; the parser keeps the store's value
   live in `%rax` across argument setup, so the guard's stated reason does not hold for this shape. Gate on
   `assign_value_effects.c` + full bar — this is where `emit-at-marker` miscompiled.
4. **C1a — design a fourth `nocode_wanted` approach** (92, largest desync site). Three are ruled out; this is the
   hardest item in the file and is deliberately last.

**Anomalies, all three chosen:**
- **D1a — explain the 1856 → 1841 verdict-count drop** under `PROMO_ARROW`/`PROMO_INCDEC`. The faithful RATIO held
  and `mcc` exits 0 with no errors, so this is not the SIGSEGV truncation recorded elsewhere. Blocks trusting any
  future count change here.
- **D1b — `store_packed_bf`.** Still `unfaithful` after `8e867f40` removed its suspected cause (the discarded-value
  ternary). Now unattributed.
- **D1c — the 26 non-mov length-differs.** Last unexplained group in the unfaithful bucket. Delta mnemonics `xorb`
  16, `movabs` 6, `add` 6, `cmp` 5, spread across `foldm_*` math folding rather than one construct.

**Housekeeping, two chosen:**
- **E1a — prune this file** (past 3400 lines; its own rule says completed items are pruned with detail left to git
  history).
- **E1b — re-measure the `-O0`/`-O1`/`-O2`/`-Os`/`-O3` curve.** Doubly stale: taken at 59.5% fidelity, and three
  gates have been re-staged since.

**NOT chosen this round:** E1c (Mach-O scattered/GOT/TLV relocations) — they hard-error by name, which is safe, and
real-object validation stays macOS-gated.

### OPEN TASKS carried out of that session, highest value first

1. **A2a — find the stale-value producer.** All 43 SYNC failures carry exactly one extra modelled value and are
   detected at the next callee push, so there is ONE producer to find, not 43 gaps. Bisect the 31 failures outside
   any ternary/short-circuit region. Five shapes are already eliminated by experiment (see the A2a section) — do not
   re-test them.
2. **B1a — widen the leftmost-leaf guard for call arguments.** The parser demonstrably keeps the store's value live
   in a register across argument setup (`mov %eax,s(%rip)` then `mov %rax,%rdi`), so the guard's stated reason does
   not hold for this shape. Gate on `assign_value_effects.c` + full bar; this is where `emit-at-marker` miscompiled.
3. **Explain the verdict-count change.** Enabling `PROMO_ARROW`/`PROMO_INCDEC` moved the count 1856 → 1841 while the
   faithful RATIO held. `mcc` exits 0 with no errors, so this is not the SIGSEGV truncation this file records
   elsewhere. Unknown why promotion staging changes which functions reach the verify print. **Understand it before
   reading any future count change here as meaningful.**
4. **`store_packed_bf` is still `unfaithful`.** Its `c ? vdup() : gv_dup()` is now recorded after `8e867f40`, so the
   discarded-ternary hole is not its cause. Unattributed.
5. **The 26 non-mov length-differs.** Delta mnemonics `xorb` 16, `movabs` 6, `add` 6, `cmp` 5 — the `xorb` count is
   spread across `foldm_*` math folding, not one construct. Only remaining unexplained group in the unfaithful bucket.
6. **A1a `nocode_wanted` (92).** Unchanged; the design problem stands.
7. **E1b — prune this file.** Now 3376 lines. Its own "How to process" rule says completed items are pruned entirely
   with detail left to git history; this session added several large resolved blocks that qualify.
8. **D1b — re-measure the `-O0`/`-O1`/`-O2`/`-Os`/`-O3` curve.** Still taken at 59.5% fidelity and now cited in
   conclusions while fidelity is 78.1% AND three gates have been re-staged since. The old numbers are now doubly stale.
9. **Mach-O remainder.** Scattered / GOT / TLV relocations still hard-error by name (safe, incomplete). Validation
   against a real `libmcc_jitengine.a` needs macOS SDK headers — a genuine gate, unlike the `llvm-ar`/`clang -target`
   assumptions that turned out to be wrong.
10. **`MCC_AST_CYCLE` stays at `-O3`** — measured, no fidelity and no runtime effect at `-O2`. Recorded so the audit
    is not repeated.

### MEASUREMENT DISCIPLINE — earned the hard way in that session, apply before believing any number

- **A `0` from an instrument is not a finding** until it has produced a `1` on a known positive DRAWN FROM THE SAME
  POPULATION. Two detectors in this file reported clean zeroes while being blind.
- **Check the summarising script against a raw line of its own input.** An `awk` reading `$7` instead of `$8`
  produced "0 of 205" and cost two commits before the raw line was read.
- **Anchor grep patterns.** Three separate false signals came from substring matches: `faithful` inside
  `unfaithful`, `verified` inside a refusal message, and `SYNC` inside `DESYNC`. Each made a hard problem briefly
  look easy — that is the tell.
- **`rc=$?` after a pipeline is the LAST command's status, not the compiler's.** A truncated verdict histogram with
  a bogus `rc=0` reads exactly like a fidelity collapse.
- **Read the gate's own comment before investigating it.** Four times the answer was already written there —
  `MCC_AST_OPASSIGN` even named nbody's `advance()` as the victim.

### Original ordering and rationale

Chosen from the decision table at the end of the 2026-07-28 session. Work them in order; each has its evidence
already gathered and cited, so none of them starts from measurement.

**Standing method for ALL of these: use `MCC_TRACE` to TEST THEORIES AND PROVE IMPLEMENTATION DIRECTIONS.** Do not
reason about what the recorder or replay "should" be doing and then patch — instrument, run, read. This session
produced two landed fixes and four rejected ones, and in every single case the trace decided it. The tooling is
`tools/tracediff.sh`, `MCC_LOG=128`, `MCC_TRACE_FILE`/`MCC_TRACE_FUNC`/`MCC_TRACE_SKIP`, `MCC_AST_UNFAITHFUL_DUMP`,
and the `LEAF`/`RV`/`CVT`/`FIN`/`[unfaithful-rel]` traces already in place.

### 1. D1a — DONE 2026-07-28. No gate remains at `optimize >= 3`.

Two gates were staged there. `MCC_AST_CYCLE` measured no fidelity or runtime effect at `-O2` and stays at `-O3`;
`MCC_AST_CHAINSTORE` moved spectral-norm 0.55s -> 0.35s and was flipped to `>= 2` (`e81035e5`). The adjacent
mis-staging — `MCC_AST_PROMO_ARROW`/`MCC_AST_PROMO_INCDEC` keyed to `optimize_size` ALONE, so on at `-Os` and off
at `-O2`/`-O3` — was flipped the same way (`e052542a`): nbody 0.40 -> 0.36s, spectral-norm 0.28 -> 0.26s, matmul
2.21 -> 2.07s, output identical to gates-off. Detail in git history.

**The D1a anomaly — the verdict COUNT dropping 1856 -> 1841 with the promotion gates on — NO LONGER REPRODUCES,
checked 2026-07-28.** With `MCC_AST_PROMO_ARROW=0 MCC_AST_PROMO_INCDEC=0` versus the defaults, mcc's own TU emits
**2127 verify lines both ways, the same function NAMES, and the identical verdict split** (1731 faithful / 296
unfaithful / 51 desync / 49 bail). The check is not vacuous: the two objects DIFFER, so the gates are active. Item
closed. (The measurement trap it came with is still worth keeping: `rc=$?` after a pipeline reports `grep`'s
status, not `mcc`'s, and the original pass was misread because of exactly that.)

### 2. B1a — DONE 2026-07-28. All three shapes resolved.

Assignment-as-value had three failing shapes and each has its own commit: the plain statement chain
(`a = b = 0`) fell out of staging `MCC_AST_CHAINSTORE` at `-O2`; the discarded-value ternary (`c ? a() : b();`,
verdict `empty`) was one missing node kind in `ast_hook_vpop`, which attaches a discarded value to the basic block
only for `AST_Invoke`/`AST_Unary` and so never attached the `AST_If` a ternary pushes (`8e867f40`); and the
call-argument path (`use(s = g())`) landed as `MCC_AST_STOREVAL_CALL` (`62bfb7b7`, see the section below).

Two alternatives are NOT viable and should not be re-tried: emitting nothing at the marker leaves `gfunc_call`
short an operand (its own comment says so), and re-emitting the RHS duplicates the call — measured, it is how the
shape became `unfaithful` in the first place. Reloading from the lvalue is semantically right but emits a load
where the parser reuses a register, trading one unfaithful form for another.

This is still the region where `emit-at-marker` was correct at `-O0`/`-O1` and MISCOMPILED at `-O2`/`-O3`, caught
only by `assign_value_effects.c` counting evaluations. Gate any further change here on that file plus the full bar.

### 3. A2a — DONE 2026-07-28. The SYNC site went 60 -> 4, and the 4 left are cosmetic.

The site is `ast_hook_vpush`'s `ast_vn != rel - 1` guard. Every failure was uniform — the recorder holding exactly
ONE more modelled value than the codegen vstack, never a capacity problem (`rel > AST_VS_MAX` was 0 of 43, so do
NOT raise `AST_VS_MAX`), and the value being pushed when it fires is overwhelmingly a CALLEE symbol. **The site is a
DETECTOR, not the culprit**: the drift happens earlier and the next push is merely the first thing to check the
invariant.

Four unaccounted-pop classes were found and fixed, each with its own commit — `switch` (`1939ba28`),
`if`/`while`/`do`/`for` (`0c4f6b33`), the short-circuit region (`d6aa9fcd`), and atomic lowering plus the
verdict-ordering bug that hid it (`5c75bf00`). The general rule they all violated:

**A hook that models a codegen pop must account for it on EVERY exit, bail included.** `ast_bail` does not stop the
recorder's stack bookkeeping — it only marks the function un-replayable — so an early return that skips the
decrement silently converts a clean `bail` into a `desync`, attributed to whatever pushes next.

Empirically NOT the cause, so do not re-test: early `return` in a branch, a `noreturn` call in a branch, a noreturn
call followed by more statements, deref-after-check, `c ? g() : 0`, and discarded `(void)x` casts.

**METHODOLOGY, learned the hard way: do NOT byte-compare two self-compiled `src/mcc.c` objects across a
compiler-source edit.** `src/mcc.c` is the amalgamation and INCLUDES `mccast.c`, so editing the recorder also edits
the translation unit being compiled — two variables, not one. A never-called dummy function added to `mccast.c`
reproduces the effect by itself. Compile a FIXED corpus with both binaries instead, and put both binaries in the
same directory: `-B` alone does not equalise them, because auto-mccdir also probes the exe directory and a binary
sitting elsewhere picks up different `threads.h`/`stdatomic.h`.

**The 4 remaining SYNC events** (`cst_hook_end`, `ast_eval_slice_wtype`/`_rec`/`_kind_ok`) all report `bail`, so
they cost nothing; they are a second unaccounted pop on the `switch` path. Repro: the same
ternary-over-recursive-call body is faithful under `if` and bare, and desyncs under `switch`.


**ROOT-CAUSED 2026-07-28, and the `ast_in_call` framing above was the symptom, not the cause.** The pops belong to
`expr_landor`, which consumes each operand it is done with — `t = gvtst(i, t)` when the operand is dynamic, `vpop()`
when it folded — and `ast_hook_landor_operand` sets `ast_in_call = 1` precisely so that consumption is not
double-counted. That accounting is complete on the NON-const path (`ast_vn--` per operand). It is incomplete on the
CONST path, whose comment states the assumption outright: "the parser folded the whole expression to that constant
and never evaluated the right-hand side". That is true only for the SHORT-CIRCUITING constant (`0 && X`, `1 || X`,
i.e. `c != i`). For the IDENTITY constant (`1 && X`, `0 || X`, `c == i`) the parser drops the constant and evaluates
every remaining operand normally — so each middle operand IS consumed, and the const path decrements for none of
them. Minimal repro set, all at `-O2`:

| source | before | after |
|---|---|---|
| `1 && x && y` | `desync:2322` (SYNC, misattributed) | `desync` at the landor hook |
| `0 \|\| x \|\| y` | `desync:2322` | `desync` at the landor hook |
| `x && y && z` | faithful | faithful |
| `1 && x` | unfaithful | unfaithful |

**Two fixes were tried and REJECTED — record them so they are not retried:**
- *Decrement the middle operand and continue* (a `dropnext` flag consumed by `ast_hook_landor_next`). Counts then
  balance and the function passes `ast_replay_ok`, so replay RUNS on a model that is missing an operand — immediate
  SEGV in `ast_replay_bb` -> `gfunc_return` -> `gvtst` on a garbage jump chain. A count fix is not a model fix: the
  operand has to become a child of the region's Binary, not vanish.
- *Desync the whole identity-constant case at the first operand.* Costs **17 previously-faithful functions**
  (1643 -> 1626) — the 2-operand shape (`1 && x`) models correctly today and must keep working.

**What landed instead** (`ast_lor_consumed[]`): desync at the SECOND consumption inside a const region, which is
exactly the first moment a middle operand is dropped. Faithful stays 1643, unfaithful 228, desync 248 — totals
unchanged, because these functions were already desyncing; only the attribution moves. SYNC-site desyncs
**23 -> 10**, with the 13 landing on the landor hook. Corpus 774/774 byte-identical, full ctest 7281/7281.

**The remaining real fix** is to model the identity constant as an ABSENT operand: build the region's `AST_Binary`
as the non-const path does, drop the constant's node, and add each subsequent operand as a child.

**TRIED 2026-07-28 AND IT IS A NET LOSS — but the attempt maps the remaining work exactly, so start from here.**
Building the Binary and dropping the constant DOES fix every isolated shape: `1 && x`, `1 && x && y`,
`0 || x || y` and `1 && x && y && z` all go from desync/unfaithful to **faithful**, and the 1-child case needs no
special handling (the LAND replay loop `for k: replay(child); if (k+1<nc) gvtst` then `gvtst_set` emits exactly the
parser's `x != 0` for one child). On mcc's own TU, though: **faithful 1731 -> 1716, desync 51 -> 77.** The regions
in real code are not the isolated shapes.

The 36 new desyncs split into three sub-cases, all in the operand/end hooks, and two are still open:
- **A later IDENTITY constant** (`x && 1`): the parser drops it (`vpop`), so the model must too — `ast_vn--`, no
  child. Implemented in the attempt, works.
- **A later SHORT-CIRCUITING constant** (`x && 0`, 16 events): the parser folds the WHOLE expression from that
  point (`nocode_wanted++, f = 1`), so the region stops being a Binary and becomes a constant. The model has
  already built the Binary and has no way back. Open.
- **A materialized ending** (20 events): `expr_landor` finishes with `if (cc || f) { vpop(); vpushi(i ^ f); }`, and
  `ast_hook_landor_end` desyncs on `materialized` by design. Those pushes happen while `ast_in_call` is still 1, so
  they are invisible to the model and the end hook would have to synthesize the Literal itself. Open.

Note the two open cases are the same shape from opposite ends: a `&&`/`||` region whose value is a CONSTANT the
parser folds, which the model represents as a Binary. The obvious conclusion is to decide the region's kind at the
END (Binary vs Literal) rather than at the first operand.

**THAT WAS TRIED TOO, 2026-07-28, and it is worth +2. Reverted.** The deferred form is genuinely simpler — no
`ast_lor_const`/`ast_lor_consumed` bookkeeping at all: each operand either contributes a node (`c < 0`) or is
dropped (`c >= 0`, the parser pops its slot), the children are buffered per region, and `ast_hook_landor_end`
builds an `AST_Binary` from them when the region is not materialized and finalizes a `Literal` straight off `vtop`
when it is. Every isolated shape becomes faithful, including `1 && x`, which no other attempt fixed.

On mcc's TU it is **1731 -> 1733 faithful**, and 15 NEW desyncs appear inside the deferred operand path itself
(`ast_vn < 1`, and the existing ternary-`AST_If` operand check). Individual functions move both ways —
`host_icache_flush`, `host_fault_regs` and `indir` gain, `pp_builtin_func` and `block` lose. So the region's KIND
was never the blocker: what remains is per-operand model state that the region hooks cannot see. Anyone picking
this up should stop optimising the region representation and go look at why an operand arrives with `ast_vn < 1`.

**Group B (31) — all resolved by the four unaccounted-pop fixes above.** Two method notes survive it and are worth
more than the narrative:

- **Anchor trace greps.** `grep -o 'SYNC .*'` also matches the tail of the `DESYNC ...` state lines; anchor with
  `\bSYNC vn=`. Third time in this file a substring match produced a false signal (`faithful`/`unfaithful`,
  `verified` in a refusal message, now this).
- **Respect function boundaries when correlating trace events.** A line-window correlation over a 1.4M-line trace
  produced a clean-looking "18 of them show two consecutive `vpop` early-returns" split that was pure artifact —
  re-running with the window reset at each `ast_func_begin` gave in-function pop counts spread from 0 to 9. The
  `call_end`-precedes-13 and `incall=0`-is-18 splits were both WITHDRAWN on that basis.


### The `desync` bucket after the 2026-07-28 session — 251 -> 51

`MCC_AST_NOCODE_CALL` took 88, `MCC_AST_INDIRECT_CALL` 36, `MCC_AST_LANDOR_INVERT` 29, and the four
unaccounted-pop fixes the rest. What is left, largest first:

| count | site | what it is |
|---|---|---|
| 18 | `ast_hook_vstore` | a store inside a ternary or short-circuit region — **one approach tried and RULED OUT, see below** |
| 13 | `ast_hook_landor_next` | the identity-constant `&&`/`||` chain's second consumption (see A2a Group A above) |
| 6 | `ast_hook_vstore` shape | bad type / `ast_vn` mismatch at the store |
| 4 | vpush SYNC | the residual `switch`-path pop, all four already reported as `bail` |
| 10 | assorted | one or two each |

**`MCC_AST_INDIRECT_CALL` — LANDED and flipped default-on 2026-07-28.** The site that held 38 of the desyncs was
NOT the `&&`/`||`-argument check next to it (that one is innocent — `g(a && b)` is faithful); it was
`ast_kind(ast_vs[ast_vn - need]) != AST_Ref`, i.e. **the CALLEE not being a plain Ref**. Reduced to four shapes:
`fp(...)` and `gfp(...)` through a function-pointer VARIABLE were already faithful, while `s->enc(...)`,
`tab[c].enc(...)` and `((T)p)(...)` all desynced.

Admitting `AST_Unary`/`AST_Convert` callees alone was worth **nothing** — 16 functions moved from `desync` to
`unfaithful` and the faithful count did not rise. `MCC_AST_VERIFY_DIFF` on the smallest repro said why in one line:

    [ast-diff] call_member: baseline 32 B, replay 37 B, first diff @ +22
      base @+14: … 48 89 c7 4c 8b 5d e0 4d 8b 1b 41 ff d3
      repl @+14: … 48 89 c7 b8 00 00 00 00 4c 8b 5d e0 4d 8b 1b 41 ff d3

The replay emits `b8 00 00 00 00` — the varargs AL setup `gfunc_call` adds when
`vtop->type.ref->f.func_type != FUNC_NEW`. The parser normalizes a function-POINTER callee to the function type
(`vtop->type = *pointed_type(&vtop->type)`) before the hook runs, so a `Ref` callee is recorded already normalized;
a `Unary` callee is recorded by the member hook BEFORE that, keeping the pointer type, whose `ref` is the pointer's
Sym rather than the function's. Replaying the same normalization after evaluating child 0 fixes it: faithful
1701 -> 1713, desync 116 -> 80.

Widening further to `AST_Binary` callees (`table[i](…)` with a local array) adds nothing — measured, no change —
so the relaxation stops at Unary/Convert. `((T)p)(…)` stays `unfaithful`, not desync.

Validation: gate-off 777/777 corpus objects byte-identical at `-O0`/`-O2`/`-O3`; gate-on determinism (3 runs +
`setarch -R` = one md5), zero non-printable symbol names, self-host fixpoint 5/5 native and byte-identical arm64 +
riscv64 cross fixpoints under qemu, all 53 `jit/*` cells, 100-seed differential fuzz vs gcc + clang with `--gates`
and 0 miscompiles. After the flip: full ctest **7327/7327**, ratchet baseline 173 -> 169, and
`optfire/default-indirect_call` locks the default-on state. `tests/exec/functions_abi/indirect_call_shapes.c` is
the new corpus cell for the three shapes — the old `func_pointers.c` did not cover them, which is why the corpus
fire rate was 0 before it was added.

**Store inside a region (18) — ONE APPROACH RULED OUT 2026-07-28. Do not retry it without fixing the passes.**
`ast_hook_vstore` desyncs when `ast_tern_top > 0 || ast_lor_top > 0`. Reduced: `c ? (y = 1) : (y = 2)`,
`a && (x = b)`, `a || (x = b)` and `a && (*p = 3)` desync, while `y = c ? x : x + 1` and `x = a ? b : 0` are
faithful — it is a store INSIDE the region, not a store OF one.

The reason the hook refuses is real: it ends by `ast_add_child(ast_cur, ast_cur_bb, st)`, i.e. a store is always a
BB STATEMENT, and a statement in the enclosing block would execute unconditionally at replay. The natural fix is to
keep the `AST_Store` as the region operand's VALUE instead (no BB child, the store node itself left on `ast_vs`)
and teach `ast_replay_value` a `case AST_Store` that replays lhs, rhs, `vstore()` and leaves the value — which is
exactly what the parser does for an assignment used as a value.

That works as far as the recorder is concerned: faithful 1731 -> 1738, desync 51 -> 33, and a new runtime cell
(`tests/exec/optimizer/region_store.c`, kept — it counts side effects through `a && (*out = side(7))` and checks
the store does NOT happen on the short-circuited path) matches gcc at `-O0`/`-O1`/`-O2`/`-O3` with the gate both
off and on. Gate-off stayed 786/786 byte-identical, determinism and symtab were clean, and the `-O2` self-host
fixpoint passed.

**It fails at `-O3` and `-Os`: stage 2 of the fixpoint — the mcc-BUILT mcc compiling mcc.c — dies with SIGABRT, and
a hand-run stage 2 hangs instead of finishing.** So the gate builds a broken compiler. Turning off `CYCLE`,
`INLINE` and `INLINE_PASS` individually does not fix it, so it is not one obvious pass.

The lesson generalises past this gate: **the faithfulness byte-compare does NOT protect a new node kind in value
position.** It validates the model against the baseline BEFORE the passes transform it; the passes then run on a
shape they were never written for and their output is what gets emitted. `AST_Store` had only ever appeared as a
BB statement, so every pass that walks values assumes it cannot see one. A retry has to audit the passes for
`AST_Store` in value position first — the same hazard that killed the fourth `nocode_wanted` approach, one level up.

**`MCC_AST_LANDOR_INVERT` — LANDED and flipped default-on 2026-07-28.** `!(a && b)` and `!(a || b)` desynced at
`ast_hook_cmp_invert`: `gen_test_zero(TOK_EQ)` inverts a `VT_CMP` in place by swapping `jtrue`/`jfalse` and flipping
`cmp_op`, and the hook models that as a token flip — which only works when the model's top node is a comparison
`AST_Binary`. For a short-circuit it is a `TOK_LAND`/`TOK_LOR` node, so the hook desynced. Reduced: `!(a && b)`,
`!(a || b)` and `if (!(a && b))` desync; `!(a < b)`, `!f(a)`, `!!a` and `t = a && b; !t` are all faithful, so it is
specifically NOT applied DIRECTLY to a short-circuit result.

De Morgan is not needed. The replay of a `LAND`/`LOR` node already mirrors `expr_landor` exactly — evaluate each
operand, `gvtst(i, t)` between them, `gvtst_set(i, t)` at the end — leaving the same `VT_CMP` the parser leaves. So
the inversion is recorded as one bit on the node (`AST_FB_LANDOR_INVERT`, XORed so `!!` cancels) and replayed as
the same three lines `gen_test_zero` runs. faithful 1713 -> **1731**, desync 80 -> 51.

Validated: gate-off 783/783 corpus objects byte-identical at `-O0`/`-O2`/`-O3`; gate-on determinism (3 runs +
`setarch -R` = one md5), zero non-printable symbol names, self-host fixpoint + all `jit/*` cells 57/57. After the
flip: full ctest **7350/7350**, 100-seed differential fuzz vs gcc + clang with `--gates` and 0 miscompiles,
byte-identical arm64 and riscv64 cross self-host fixpoints under qemu, ratchet baseline 169 -> 168, and
`optfire/default-landor_invert` locks the default. `tests/exec/optimizer/logical_not_shortcircuit.c` is the new
corpus cell: it counts side effects through `!(side(a) && side(b))` and enumerates the truth table for `!(a && b)`,
`!(a || b)`, `!(a && b && c)`, `!!(a && b)` and `if (!(a && b))`, matching gcc at `-O0`/`-O1`/`-O2`/`-O3` with the
gate both off and on — an inverted jump sense is exactly the bug that byte-identity alone would not catch.

**THE RESIDUE IS 10, AND SIX OF THEM ARE ONE CLASS — measured 2026-07-28 after the three fixes above:**

| function | delta | value at the failing push |
|---|---|---|
| `mccjit_qsbr_quiescent`, `mccjit_qsbr_min_local`, `mccjit_qsbr_retire`, `mccjit_selftest_pool`, `mccjit_patch_swap_store`, `mcc_jit_publish` | **3 or 4** | `r=0x30` constant |
| `cst_hook_end`, `ast_eval_slice_wtype`, `ast_eval_slice_rec`, `ast_eval_slice_kind_ok` | 1 | mixed |

The six large-delta ones were all **atomic lowering**. `parse_atomic` (`mccgen.c`) builds a libcall by hand —
`vpush_helper_func`, `vrott(7)`, `gfunc_call(6)`, `gen_test_zero`, `gvtst` — and none of that traffic is modelled,
so the drift is 3-4 values rather than the 1 every other class produces. **FIXED 2026-07-28: `parse_atomic` now
calls `ast_hook_bail()`.**

**That exposed a verdict-ordering bug worth more than the atomics themselves.** Bailing alone changed nothing,
because `ast_bail` does not stop the recorder — the model kept drifting and still desynced, and the verdict ladder
checks `desync` BEFORE `bail`. So a function that deliberately refused to model a construct was reported as an
accidental drift, at whatever unrelated push first noticed. `AST_SET_DESYNC` now records that a bail came first, and
the verdict reports `bail` in that case. On mcc's own TU: **desync 248 -> 204, bail 3 -> 47, faithful unchanged at
1643, unfaithful unchanged at 228** — 44 functions were being counted as drifts when they had been deliberately
refused. The SYNC site itself falls **10 -> 4**.

**The ratchet had to be strengthened in the same commit, or the reclassification would be a free win.**
`verify_ratchet.cmake` counted `desync|unfaithful|stackresidue` as gaps and ignored `bail`, so relabelling would
have shrunk the gap set without improving coverage. `bail` now counts — a bailed function is excluded from AST
optimization exactly like a desynced one. Baseline 167 -> 172 (26 bail entries added, 21 desync entries relabelled;
the net +5 is functions that were bailing all along and had been invisible to the ratchet).

The four remaining SYNC events are `cst_hook_end` and `ast_eval_slice_wtype`/`_rec`/`_kind_ok`. Minimal repro,
found by reducing the real function rather than guessing:

    int a1(int n) {                       /* desync */      int a3(int n) {                  /* faithful */
      switch (k(n)) {                                         if (k(n) == 1) {
      case 1: { int wt = a1(c(n, 1));                           int wt = a3(c(n, 1));
                return wt ? wt : a1(c(n, 2)); }                 return wt ? wt : a3(c(n, 2));
      default: return 0; } }                                  }
                                                              return 0; }

The same body is faithful under `if` and bare, and desyncs under `switch` — so it is a SECOND unaccounted pop on the
switch path, distinct from the `sw->sv = *vtop--` one already fixed. The trace shows it inside the ternary:
`ast_hook_ternary_begin` early-returns (the function is already bailed), the `gvtst` push lands, and the true arm's
push arrives with `rel` one lower than the model. Since these four now report `bail` rather than `desync` they are
no longer misattributed, but the pop is still unaccounted.

### 4. A1a — model dead regions for `nocode_wanted` (98, the largest desync site)
Three approaches were already ruled out and recorded: a flat gate; hooking the transitions across ~30 irregular
mutation sites; and a region suspend, which fails because it leaves the model BEHIND the vstack. A fourth approach
has to mirror stack effects while recording nothing AND survive a self-host generation.

**A FOURTH IS NOW RULED OUT TOO, 2026-07-28, and its failure signature is the most useful thing in this section.**
The obvious relaxation is to delete the guard: `ast_hook_call_begin` desyncs on `nocode_wanted`, so let it model the
call and rely on the replay byte-comparison to reject anything wrong. The reasoning looked sound — a dead call
emits nothing, and replay re-enters the dead state on its own because the constructs that CAUSE it (`return`,
constant conditions) are themselves modelled and replayed. That much is even true: dead code WITHOUT a call is
already faithful today (`if (0) x++;`, `return x; x++;`, `if (0) { int z = x; return z; }` all pass), because
replay re-executes the `Return` and mcc's own `CODE_OFF` turns emission off again.

Measured with the guard gated off (`MCC_AST_NOCODE_CALL=1`), the headline looked excellent:

| | faithful | desync | unfaithful | bail |
|---|---|---|---|---|
| gate off | 1643 | 204 | 228 | 47 |
| gate on | **1699** | 116 | 258 | 49 |

**+56 faithful, and it is wrong.** Three things fail, in increasing order of severity:
1. **Spurious symbol references.** `if (0) f(x);` gate-on emits an UND `f` in the symtab that gate-off does not.
   A call that never executes now creates a link-time dependency.
2. **Garbage symbol names on a large TU.** mcc's own object gains symbols whose names are raw pointer bytes
   (`<\x953f5be0>` and friends in `readelf -sW`). `.text`, `.data`, `.rodata` and `.bss` are all byte-identical —
   the corruption is confined to the symbol table.
3. **Run-to-run nondeterminism.** Three runs of the SAME binary on the SAME input produce three different objects,
   and `setarch -R` produces a fourth. That is a read of uninitialized memory, not a modelling difference.

The 3-stage self-host fixpoint catches all of it: `o1 != o2` at every level (`-O2`, `-O3`, `-Os`, and the gate
sweep). Worth noting for future gate work — the corpus did NOT catch it. 774/774 objects stayed byte-identical
gate-off, the gate fired on only 12 of 516 corpus compiles, and the small dead-call repro has a clean symtab. Only
mcc's own TU is big enough to surface it.

**Why it happens, and what it implies for the fifth attempt.** The guard is not conservatism about EMISSION, it is
protection against reading the recorder's own stack while that stack is known to be drifting. In a dead region the
model's `ast_vn` no longer tracks the vstack — that drift is exactly what the guard reports — so
`ast_vs[ast_vn - need + i]` in `ast_hook_call_begin` indexes slots that were never written for this call, and the
`sym` field of an unwritten arena slot is uninitialized memory. Hence pointer-shaped names and ASLR sensitivity.
So the fifth approach cannot start by trusting `ast_vs` inside a dead region: it has to make the model's stack
correct there first (mirror the pushes and pops, record no nodes), which is the "mirror stack effects while
recording nothing" requirement stated above — now with a concrete reason rather than an aesthetic one.

**THE FIFTH APPROACH LANDED 2026-07-28 behind `MCC_AST_NOCODE_CALL` (default OFF), and it is exactly that.** A dead
call is now MIRRORED rather than modelled: `ast_hook_call_begin` verifies the model is still in sync
(`ast_vn == rel`, enough operands), drops the callee and the arguments (`ast_vn -= need`), and records NOTHING —
no `AST_Invoke`, no `ast_finalize_leaf`, so no read of `ast_vs` contents and no symbol. `ast_hook_call_end` pushes a
neutral placeholder (`AST_Literal 0`, no sym) to stand for the result the parser pushed, and
`ast_hook_call_effect_end` handles the discarded-statement ending with no placeholder. The dead statement simply is
not in the model, which is the correct model: the baseline emitted nothing for it, so replay emitting nothing
matches byte-for-byte, and replay does not need to re-enter the dead state at all.

Why this is sound where the fourth was not: every failure there came from READING the recorder's stack while it was
drifting. Mirroring never reads it — it only decrements — and it refuses (desyncs) if the stack is already out of
sync, so an unwritten arena slot can never be reached.

| | faithful | desync | unfaithful | bail |
|---|---|---|---|---|
| gate off | 1643 | 204 | 228 | 47 |
| gate on | **1700** | 116 | 258 | 49 |

All seven dead-call shapes that used to desync are faithful gate-on — `if (0) f(x)`, `return x; f(x)`,
`0 && f(x)`, `1 ? x : f(x)`, `while (0) f(x)`, unreachable tails after an if/else, and a dead statement between two
`case` labels. Validation, including the three checks that killed the fourth approach:
- gate-off 774/774 corpus objects byte-identical at `-O0`/`-O2`/`-O3`, full ctest 7281/7281;
- **deterministic**: three runs plus `setarch -R` produce ONE md5 on mcc's own TU (the fourth approach produced four);
- **no garbage symbols**: zero non-printable symbol names, and the small repro's symtab is identical gate-on vs
  gate-off (the fourth added a spurious UND `f`);
- **self-host fixpoint green at every level** — `-O2`, `-O3`, `-Os` and the gate sweep (the fourth failed all four);
- gate-on full ctest 7280/7281, the one failure being `ast-verify-ratchet` reporting the gap-set shrink against its
  gate-off baseline.

Differential fuzz gate-on: **120 seeds vs gcc + clang with the full `--gates` sweep, 0 miscompiles**
(`fuzz_runner --seed 4242 --count 120 --gates`).

**FLIPPED DEFAULT-ON at `-O2`/`-O4` 2026-07-28, after the cross-arch and JIT legs came back clean:**
- **arm64 and riscv64 3-stage cross self-host fixpoints**, `o1 == o2 == o3` byte-identical with stages 2 and 3
  executed as real arm64/riscv64 code under qemu (`tools/selfhost-cross-native.sh`), plus each one's
  compile-and-run sanity against the host cc. Run twice: with the knob forced on, and again after the flip with no
  knob at all.
- **i386 and arm**: the seven dead-call shapes compiled by `cmake-cross/mcc-<arch>`, linked against the vendored
  stage3 sysroot and run under `qemu-i386`/`qemu-arm` — output matches gcc gate-off AND gate-on. The gate provably
  FIRES on all four cross targets (the objects differ), so none of these legs is vacuous.
- **JIT**: all 53 `jit/*` cells pass with the gate on.
- Full ctest after the flip: **7304/7304**. Differential fuzz with the new defaults: **150 seeds vs gcc + clang with
  `--gates`, 0 miscompiles**. Ratchet baseline 172 -> 170.
- `optfire/default-nocode_call` locks the default-on state the same way the other flipped gates are locked
  (`tests/optfire/src/nocode_call.c` + a `defstate.txt` row), so a regression that silently turns it back off fails
  a test rather than quietly costing 57 functions. Note the corpus is a WEAK gate for this class — it caught none of the
fourth approach's failures — so any flip must lean on the self-host fixpoint and determinism checks instead.

### Alongside
- **E1b — prune this file.** It is 3062 lines and its own "How to process" rule says completed items are pruned
  entirely with detail left to git history. The 2026-07-27 and 2026-07-28 sessions both added large resolved
  blocks that now qualify.
- ~~**D1b — re-measure the `-O0`/`-O1`/`-O2`/`-Os`/`-O3` curve.**~~ **DONE 2026-07-28**, on mcc's own TU
  (2127 verified functions):

  | level | faithful | desync | unfaithful | bail | % faithful |
  |---|---|---|---|---|---|
  | `-O0` | — | — | — | — | recorder inactive (zero verify lines) |
  | `-O1` | 1589 | 276 | 215 | 47 | **74.7%** |
  | `-O2` | 1713 | 80 | 285 | 49 | **80.5%** |
  | `-Os` | 1699 | 116 | 261 | 49 | 80.0% |
  | `-O3` | 1713 | 80 | 285 | 49 | 80.5% |
  | `-O4` | 3376 | 232 | 494 | 152 | 79.4% (every function is verified TWICE — the search runs two passes, so the
  counts double; the RATIO is the only comparable figure) |

  The `-O1` deficit is not a modelling difference: it is the gates staged at `-O2` (`NOCODE_CALL`,
  `INDIRECT_CALL`, `CHAINSTORE`, `OPASSIGN`, …). `-Os` trails `-O2` by 14 because `INDIRECT_CALL` keys on
  `optimize >= 2` and `-Os` does not set it.

## AST recorder fidelity — INDEX (findings live in the sections named; this is a map)

**CURRENT STATE, measured 2026-07-28 on mcc's own amalgamated TU at `-O2` (2127 functions, compile `rc=0` so the
counts are not truncated): 1731 faithful / 51 desync / 296 unfaithful / 49 bail — `81.4%` FAITHFUL.** That session
moved it 1643 -> 1731 with four unaccounted-pop fixes and three new default-on gates (`MCC_AST_NOCODE_CALL`,
`MCC_AST_INDIRECT_CALL`, `MCC_AST_LANDOR_INVERT`); `desync` fell 251 -> 51. The verdict ladder now reports a
deliberate `bail` ahead of a later accidental `desync`, so the desync bucket is drifts only, and `unfaithful` is
decisively the largest remaining bucket.
The 2026-07-27 session ended at 1392 faithful (75.3%) of 1849; before that it started at 1101 (59.5%) with 60 bails. `faithful` is the figure that matters: it gates the optimizer
passes, `ast_search_*`, the ROI scorer, `ast_inline_retain`, `ast_reemit_retain` and JIT dispatch. `desync`,
`unfaithful` and `bail` are all equally excluded, so moving a function BETWEEN them is not progress and not a
regression.

Four fixes landed this session, each through the full bar (host + cross ctest, 3-stage self-host fixpoint,
`ast/treecheck`, 400-seed gate-swept differential fuzz, all four qemu triples, both side-effect guards):

| fix | gain |
|---|---|
| bare `return;` modelled (was `ast_bail`) | **+217** faithful, bail 60 -> 1 |
| `sizeof`/`_Alignof` operand hidden from the recorder | +33 |
| constant `&&`/`||` folded | +20 |
| static-storage initializers hidden from the recorder | +16 |
| F3a assignment-as-value (`AST_StoreVal`) | +10 |
| constant `?:` folded | +5 |

Two more landed 2026-07-28, same bar:

| fix | gain |
|---|---|
| `MCC_AST_OPASSIGN` staged at `-O2` instead of `-O3` (`b0fb11d5` predecessor `6acb9e69`) | **+44** faithful, and nbody `-O2` 0.49s -> 0.41s, matching `-O3` |
| `force_charshort_cast`'s internal cast hidden from the recorder (`b0fb11d5`) | +5, and it fixed a REAL lost sign extension |

**The `unfaithful` bucket is now fully triaged — see the section below.** Of 205 measured events: 45 chained
assignment, 24 `nocode_wanted`, 90 reorder/register-allocation, 24 spill/regalloc, 2 lost sign extension (FIXED),
and the remainder characterised. **Exactly one correctness-relevant defect was found in the whole bucket and it is
fixed**; everything else is either a documented modelling limitation or semantically-equivalent emission that
byte-identity rejects for being stricter than it needs to be.

**The remaining compile-time parser contexts were SWEPT 2026-07-27 and are all already faithful, so the pattern
below has no further easy applications:** local `enum` declarations (including `Y = X + 1`), local arrays with
constant sizes, bitfield widths, `_Static_assert` in a function body, `switch` `case` label expressions (including
`case 2+3`), `_Alignas` on a local, local `typedef`s, and compound literals. Only `sizeof`/`_Alignof` operands and
static-storage initializers needed the bracket — do not re-sweep these.

**The pattern that generalises: suspend where the PARSER knows it emits nothing, not where the recorder notices
trouble.** Two of the six (`sizeof`, static initializers) are the same three-line `ast_hook_synth_begin`/`_end`
bracket around a parser call whose walk produces no code. The equivalent recorder-side suspends all FAILED — a
region suspend inside the recorder leaves the model behind the vstack instead of ahead (see the `nocode_wanted`
attempts), because the values are real even when the code is not.

**206 desyncs, re-measured 2026-07-28 on the current tree.** Counts are UNCHANGED from the 2026-07-27 measurement
for every site except the value-model guard; only the line numbers moved. **Identify sites by HOOK NAME, not by line
number** — every fix shifts them, and the numbers below are already the third set this file has carried.

| hook | check | count |
|---|---|---:|
| `ast_hook_call_begin` | `nocode_wanted` | **92** |
| `ast_hook_vpush` | vstack SYNC (`ast_vn != rel - 1`) | 43 |
| `ast_hook_cmp_invert` | `&&`/`\|\|` reached the comparison inverter — see below, NOT a missing case | 24 |
| `ast_hook_vstore` | a store inside a ternary/`&&`/`\|\|` region — correct, see below | 17 |
| `ast_hook_call_begin` | callee is not an `AST_Ref` — relaxing it buys nothing, see below | 15 |
| `ast_hook_vstore` | (second store site) | 6 |
| `ast_hook_vpush` | value-model guard | ~~119~~ → **4** (2026-07-28: 68 of these were `MCC_AST_OPASSIGN` staged at `-O3`; flipped to `-O2`, see the nbody section) |
| `ast_hook_landor_operand` | | 2 + 1 |
| `ast_hook_member_end` | | 1 |

**Every site above already has a recorded verdict, and they are all "ruled out" or "needs a design change", not
"unexplored":** `nocode_wanted` has three approaches ruled out (flat gate, transition hooking across ~30 irregular
mutation sites, region suspend — a region suspend leaves the model BEHIND the vstack because values are real even
when code is not); `cmp_invert` is a correct guard that needs De Morgan and would miscompile if the switch cases
were simply added; the ternary/landor store site is a correct guard needing per-arm basic blocks; the callee-kind
guard was measured to buy nothing when relaxed. So the desync half is not a triage problem the way `unfaithful` was
— it is a set of known design decisions, and the next person should pick one and design it rather than measure it
again.

**`ast_hook_call_begin`'s callee-kind guard is conservative but relaxing it BUYS NOTHING (checked 2026-07-27).**
It fires when the callee is a COMPUTED expression rather than a plain `AST_Ref`: `s.m(1)` (struct member),
`t[i](1)` (array of function pointers) and `(c ? h : fp)(1)` (ternary) all desync, while a direct call and a plain
function-pointer variable — global or local — are faithful. Widening the accepted kinds to `Load`/`Unary`/`Binary`/`If`
does let those bodies through, but they land in **`unfaithful`**, not `faithful`: replay does not reproduce the
parser's bytes for a computed callee. Since `unfaithful` and `desync` are equally excluded, that is a wash, and the
extra permissiveness only loses the guard's clarity. Reverted. Recovering these 14 needs the replay side to emit a
computed callee the way the parser does (evaluation order of callee versus arguments is the likely culprit), not a
looser recorder check.

**`ast_hook_vstore`'s ternary/landor guard is also CORRECT, and closing it is a structural change (checked
2026-07-27).** Every conditional-store shape reaches it — `x ? (v=1) : (v=2)`, `x && (v=1)`, `x || (v=1)`, and the
compound form `x ? (v+=1) : (v-=1)` — while the same store outside the region is faithful. The reason is that
`ast_hook_ternary_begin` does NOT switch `ast_cur_bb` the way the loop hooks do (`ast_hook_while_begin` sets
`ast_cur_bb = body`): a ternary's arms are expressions whose values go to `ast_vs`, so a Store recorded inside one
would be appended to the ENCLOSING basic block and replayed UNCONDITIONALLY. Refusing is the right answer. Closing it
needs the arms to own their own basic blocks — the same structural change the `while ((h = f()))` case needs, and it
lands on every pass that walks `AST_If`.

**`ast_hook_cmp_invert`'s `default:` arm looks like a trivial "add the missing cases" fix and is NOT — adding them
would MISCOMPILE (checked 2026-07-27).** Instrumenting it: all 24 events are `op = 0x90` (`TOK_LAND`, 17) or `0x91`
(`TOK_LOR`, 7), on an `AST_Binary` in every case — i.e. `!(a && b)`. The hook models inversion as `op ^ 1`, which is
exactly right for the comparison pairs it lists (`ULT`/`UGE`, `EQ`/`NE`, …) but for the logical operators maps
`&&` -> `||` while leaving both operands untouched. De Morgan requires negating the operands too, so a bare op-flip
would record a model that means something different from the code. **The `default:` arm is doing the correct thing;
closing this needs `!(a && b)` modelled as `!a || !b`, which rewrites the operand subtrees, not a switch case.**

Note what is ABSENT: the constant-condition sites for `?:` and `&&`/`||` (previously 45 + 6) are gone entirely —
those two fixes removed the whole category rather than reducing it. The `ast_hook_vstore` row is newly visible for
the same reason: it fires for a store INSIDE a ternary/landor region, which previously desynced earlier at the
condition.

**Measure with the full TU, not `tests/exec`** — the corpus is simpler code and reads several points higher.

**COUNTING CORRECTION 2026-07-27 — earlier `faithful` figures in these sections were INFLATED.** They were taken with `grep -c 'faithful\t'`, which also matches **un**faithful. Corrected, anchored counts over mcc's own TU at `-O2` (1736 functions):

| config | desync | unfaithful | faithful | delta |
|---|---:|---:|---:|---:|
| baseline | 642 | 139 | 904 | — |
| `MCC_AST_MEMBER_AGG=1` | 504 | 169 | **1010** | +106 |
| `MCC_AST_MEMBER_CONST=1` | 591 | 143 | 950 | +46 |
| `MCC_AST_CMP_INVERT=1` | 650 | 122 | 913 | +9 |

The 48%-unoptimized headline is unaffected (832 of 1736 non-faithful). But **`CMP_INVERT` was recorded as a net coverage LOSS of 8 functions, justified as 'correctness costs a little coverage'. That was wrong — it is a net GAIN of 9.** The correctness argument for it stands on its own; the trade-off framing does not, and is retracted. `MEMBER_AGG` is +106 not +136, `MEMBER_CONST` +46 not +51 (its desync reduction of 51 was correct — that number was measured a different way).

**SUPERSEDED 2026-07-27 by the F1 flips — the ceiling is now 41%, not 48%.** Re-measured on mcc's own TU at `-O2`
with `MEMBER_AGG`/`MEMBER_CONST`/`CMP_INVERT` all default-on, against the same compiler with all three forced off:

| | faithful | desync | unfaithful | bail | non-faithful |
|---|---:|---:|---:|---:|---:|
| all three OFF (old) | 896 (48.5%) | 746 | 148 | 57 | **952 (51.5%)** |
| all three ON (now) | **1090 (59.0%)** | 540 | 157 | 60 | **758 (41.0%)** |

**+194 functions faithful, +10.5 percentage points**, out of 1848. Desync falls by 206; the +9 unfaithful and +3 bail
are the known `MEMBER_CONST` residue (F3) and cost nothing, since unfaithful and desync are equally excluded. Since an
optimizer pass cannot run in a function the recorder did not model faithfully, this bounds every other optimization
number in this file — so figures below that were derived against the 48% ceiling are now conservative.

Original entry, kept because the itemised causes below are still measured against it. Established 2026-07-27 while
building `optfire`. **48% of functions get no AST optimization on x86_64** — 832 of 1736
non-faithful over mcc's own TU at `-O2` (642 desync, 139 unfaithful, 904 faithful, plus bail/empty). An optimizer pass
cannot run in a function the recorder did not model faithfully, so this bounds every other optimization number in this
file. Counts here are the anchored ones from the correction table above; do not mix them with the pre-correction
figures still quoted in some sub-entries below.

Causes, largest first (`AST_SET_DESYNC()` stores `__LINE__`, so a `desync:N` must be read against the source the
measuring compiler was BUILT from — a stale cross compiler reports meaningless lines):

| share | site | cause | where written up |
|---|---|---|---|
| 287 (34%) | member access | 214 nested-struct (all LVALUE — `MCC_AST_MEMBER_AGG` recovers them), 73 `const` (`MCC_AST_MEMBER_CONST` recovers 51) | Ungate campaign |
| 213 (26%) | vstack depth | 100% the `ast_vn != rel - 1` SYNC arm; capacity never fires | see below — measured, no shortcut |
| 83 (13%) | value model | register-resident operand; the 32-bit `int`<->`long long` case | Cross-arch parity |
| 32 (4%) | `&&`/`\|\|` | **100% a CONSTANT first operand** (`c >= 0`) — measured | see F4 |

Landed against this, all default OFF (deltas per the corrected table above): **`MCC_AST_MEMBER_AGG` +106 faithful, the largest single win**; `MCC_AST_MEMBER_CONST` +46; `MCC_AST_CMP_INVERT` +9 (it also fixes `!!` modelling, closing a path where 8 functions were optimized on an inverted-condition model; on arm64 it desyncs rather than mismodels). **None is flipped on — see the follow-ups section for what each still needs.**

**`MCC_AST_MEMBER_AGG` detail.** The VALUE-model guard already permits an aggregate LVALUE (`agg_lval`), but the MEMBER guard rejected any `VT_STRUCT`-typed member outright through `ast_bad_type`. Measured: **all 214 struct-member rejections in mcc's own TU are `nonlval=0`**, i.e. lvalues — a nested `a.b.c` path whose intermediate is never loaded as a value, exactly the shape the value site allows. Granting the same escape: **desync 642 -> 504, faithful 904 -> 1010 (+106, +12%)**, with unfaithful 139 -> 169. Those +30 are caught and excluded from optimization, so there is no miscompile path. **Characterised 2026-07-27, and the conclusion is that they do NOT block the flip.** Every transition under the gate is OUT of `desync`: 106 -> `faithful`, 30 -> `unfaithful`, 2 -> `bail`, and **zero functions regress from `faithful`**. Since `unfaithful` and `desync` are equally excluded from optimization (both are `!ast_fn_faithful`), those 30 are exactly as optimized as before — the gate is strictly better, not a trade. Their diffs are heterogeneous, unlike the single-shape `const` residue: sampling 8, four replay at identical length (`ast_fn_inlinable` 630/630, `ast_tco_run` 3607/3607, `compare_types` 564/564, `gcase_jumptable` 1913/1913) and four differ in both directions (`decl` 6697/6758, `block_cleanup` 354/361, `expr_cond` 1362/1357, `cplx_extract_const` 949/943); none is reloc-only. So there is no single root cause to chase, and chasing them is optional polish rather than a precondition. **Real flip criteria: cross-arch validation (arm64 especially, given `CMP_INVERT` behaved differently there) and regenerating the ratchet baseline.** Full suite 7228/7228 with the gate OFF; with it ON the only failure is `ast-verify-ratchet` reporting **769 gaps against a 776 baseline** — it fails because the gap set IMPROVED and wants regenerating. **That instruction is now superseded**: an earlier revision said not to regenerate the baseline until the 30 were understood. They are understood — they never regress from `faithful`, so regenerating is gated on the FLIP, not on them. See follow-up F1.

**Two method rules this cost real time to learn — apply them before believing any measurement here:**
1. **Check the compile exit status before reading a counter or comparing objects.** `--stats` still prints its panel
   after a failed compile (counters read 0), and `cmp` of two objects that were never written reports "differ". That
   combination produced a confident, entirely false "the cross compilers run no optimizer" reading.
2. **Prefer the `--stats` counter to an object diff when asking whether a pass fired.** An object diff conflates "did
   not fire" with "fired and emitted identical bytes" — it produced a false "17 gates are broken on i386" and a false
   "28 gates are inert". And grep the counter anchored (`\b<name> +[0-9]+`); the panel prints many.

**The 26% vstack site: MEASURED 2026-07-27 — it is 100% the SYNC arm, 0% capacity. Do NOT raise `AST_VS_MAX`.** `ast_hook_vpush` desyncs on `ast_vn != rel - 1 || rel > AST_VS_MAX`:
- `ast_vn != rel - 1` is a REAL desync — the recorder's node count has lost sync with the codegen vstack.
- `rel > AST_VS_MAX` is a CAPACITY limit, not a modelling failure. `AST_VS_MAX` is **64** and `ast_vs` is a fixed `AstLocal[64]`, so a sufficiently deep expression is refused for want of table space. Raising it costs 4 bytes per entry.
Instrumenting both guard sites to report which arm fired, over mcc's own TU at `-O2`: **214 events, all `SYNC`, none `CAPACITY`** — and all from the first site (`ast_hook_vpush`), none from the second. So the 64-entry `ast_vs` table is never the limiter and enlarging it recovers nothing; the whole 26% is the recorder genuinely losing sync with the codegen vstack. That is real modelling work, not a constant bump. This is exactly why the note said to measure before touching the constant — the cheap-win reading was wrong.

Also unexplained: **riscv64 shows ~30x the `unfaithful` rate of every other target** (92 vs 3 over the freestanding
`optfire` corpus), and it is relocation divergence with byte-identical code. Cross-arch parity section; reserved for the
arm64 machine.

## `-O4` is a SIZE level, not a speed level — measured 2026-07-27 (corrected)
Full `vendor/plb` sweep (gcc / clang / mcc x `-O0..-O3`,`-Os`, plus `mcc -O4`), 5 kernels, min-of-3, every cell's stdout
checked against a `gcc -O2` reference: **0 correctness mismatches anywhere**.

**CORRECTION — an earlier revision of this entry called the `-O4` result a "correctness-of-optimization bug". It is
not.** The out-of-process superopt is **scored by emitted SIZE** (`SoPfCkpt.best_size` in `mcc.c`), so producing smaller
and slower code is it working, not failing. Text sizes confirm it, `-O4` plain versus `-O4` with the driver disabled:

| kernel | `-O3` | `-O4` plain | `-O4` `SEARCH_WORKER=1` |
|---|---:|---:|---:|
| nbody | 3543 | **3583** | 3945 |
| spectral | 2722 | **2494** | 2769 |
| matmul | 2542 | **2276** | 2972 |

`MCC_SEARCH_WORKER=1` at `mcc.c` DISABLES that driver (`mcc_superopt_search` is gated on
`!mcc_env_on("MCC_SEARCH_WORKER")`), which is why it is both faster to compile and larger. The runtime cost of the size
win, isolated one variable at a time on `nbody`: plain `-O4` compiles in 8.44 s and runs in 0.28 s; with the driver
disabled, 4.29 s and 0.18 s. `MCC_JIT=1` and `MCC_AST_SEARCH=1` change neither number.

**What IS worth acting on:**
1. **The semantics are surprising and undocumented.** `-O4` is presented as "run every implemented optimizer", and `-Os`
   already exists for size, so a user reasonably expects `-O4` to be the fastest code. It is the smallest. Say so in the
   help text, or make the scoring axis selectable.
2. **The runtime-scoring mode is reachable but does not change the winner (investigated 2026-07-27).** It is enabled
   by **`MCC_AST_JITSCORE=1`**, not `MCC_JIT` — that is why the sweep never engaged it; I was setting an unrelated
   variable. With it on and a COLD cache, `-O4` on `nbody` produces a **byte-identical binary** (same md5, same text
   3636, same 0.29 s runtime) as size scoring, at the same compile time. So on this kernel the search converges on the
   same configuration either way and the scoring axis is not the lever it appears to be. Before building on it, find a
   kernel where the two scorings actually diverge; if none exists, the mode is decorative.
3. **The superopt cache makes `-O4` 2x SLOWER for a byte-identical result — characterised 2026-07-27.** With 1112
   accumulated `so-*` entries, `-O4` on `nbody` compiles in **8.4 s**; against a clean `XDG_CACHE_HOME`, **4.0 s**.
   Stable across repeats in both states, and **both produce the SAME binary** (md5 `c860fa1313`, text 3636, 0.28 s).
   So the extra 4.4 s buys nothing at all.
   It is NOT a directory scan — the lookup is keyed (`so-<key>.ck` via `so_ckpt_path`). The mechanism is stranger:
   `MCC_AST_SEARCH_VERBOSE=1` shows the clean run emitting **16 `[search] store` records, all `COMPLETE`** (the
   per-function AST search runs and finishes for 16 functions), while the warm run emits **ZERO search lines** — the
   per-function search is entirely short-circuited by memo hits, and it still takes twice as long. So the extra time is
   NOT search work; it is the out-of-process superopt driver spending a time budget the memo hits freed up, and
   arriving at the identical answer.
   Two things to fix, in order: (a) the driver should stop when the in-process search reports every function COMPLETE,
   rather than burning its remaining budget on a decided outcome; (b) a memo hit should make `-O4` FASTER, which is
   the whole point of the checkpoint.
   **This also invalidates a class of numbers: every `-O4` compile-time figure in this file is cache-state-dependent
   and probably overstated**, including the `tools/bench.c` 420x claim. Re-measure against an isolated
   `XDG_CACHE_HOME` before quoting any of them.
3. **8.4 s per compile for ~1-10% size** is the trade `tools/bench.c` already reports as 420x compile time for ~8%
   smaller output. Consistent; no new information, but it belongs next to these numbers.

**REQUESTED 2026-07-27 — `-O4`+ must be a strict SUPERSET of `-O3`, never a regression from it.** Today the `-O4`
search is free to select a gate configuration that turns OFF optimizations `-O3` applies by default, so `-O4` can and
does emit worse code than `-O3`. Evidence from the sweep above, same box, same kernel: **`mcc -O3` runs `nbody` in
0.246 s and `mcc -O4` in 0.285 s** — `-O4` is 16% SLOWER than the level below it, after spending 4-8 s searching. The
mechanism is already documented elsewhere in this file: at `-O4` the `--stats` STRATEGY counters read all-zero because
the search picks a config that disables the strategies for small functions (the `divmagic` case is the worked example —
`MCC_AST_SEARCH=1` keeps `idiv`, `MCC_AST_SEARCH=0` emits the magic-multiply).

**PART 1 IMPLEMENTED 2026-07-27 as `MCC_AST_SEARCH_FLOOR` (default OFF) — and it does NOT fix the regression.**
The floor is the gate mask captured at the end of `ast_configure` (at `-O4` that IS the `-O3` default set, since `-O4`
sets `optimize=3`), OR-ed into every `ast_search_gates_set()`, so the search can only ADD. It demonstrably takes
effect — nbody text goes 3636 -> 3748 with it on — but **runtime stays 0.28 s against `-O3`'s 0.24 s**. Full suite
7228/7228 with it off.
So the `-O4` slowdown is NOT the search subtracting `-O3` gates, which was the hypothesis this item was written on.
Also falsified: the superopt's inline-limit axis — forcing `MCC_AST_INLINE_LIMIT=160` at `-O4` changes neither
runtime nor text.
**CAUSE IDENTIFIED 2026-07-27 — the out-of-process driver DISCARDS a better in-process result.** Clean-cache,
min-of-5, nbody:

| config | runtime | text |
|---|---:|---:|
| `-O3` | 0.24 s | 3596 |
| `-O4`, driver ON (default) | **0.28 s** | 3636 |
| `-O4`, driver OFF (`MCC_SEARCH_WORKER=1`) | **0.17 s** | 3766 |

The in-process per-function search alone produces the FASTEST code of the three — **29% faster than `-O3`** — for 4.7%
more text. The out-of-process driver then replaces that result with its own size-optimal pick: **3.5% smaller and 65%
slower**. So `-O4` already contains a better answer than it ships; the driver throws it away.
That reframes the fix. Flooring gates (part 1 above) cannot help, because nothing is being subtracted — a whole
better configuration is being overridden.
**Widened to 4 kernels 2026-07-27, and one case is a STRICT regression that needs no axis debate:**

| kernel | `-O4` vs `-O3` time | `-O4` vs `-O3` text |
|---|---:|---:|
| nbody | **+17.7%** | **+1.1% LARGER** |
| spectral | +40.0% | -8.2% |
| matmul | +1.1% | -10.3% |
| nsieve | +8.2% | -1.4% |

`-O4` is slower than `-O3` on **all four**. On three it at least buys size, which is a defensible (if surprising)
trade. **On nbody it is slower AND larger — worse on the driver's OWN scoring axis.** That is not a design tradeoff,
it is the driver shipping a result it should have rejected.
**DIAGNOSED 2026-07-27 — the driver's SCORING PROXY is miscalibrated. Four hypotheses falsified first; recording them
so nobody re-runs the same experiments.**
- *Not* gate subtraction at the AST-search layer. `MCC_AST_SEARCH_FLOOR` takes effect (text 3636 -> 3748) and does not
  recover the runtime. It also targets the wrong layer: the driver sets gates via ENV in child processes, which
  `ast_configure` reads, so a floor applied inside `ast_search_gates_set` never sees them.
- *Not* the inline-limit axis. `MCC_AST_INLINE_LIMIT=160` changes neither runtime nor text.
- *Not* an unreachable baseline. Reading `so_setenv_cfg` end to end: `gate == SO_GATE_DEFAULT` RESTORES the user env,
  so the plain baseline IS expressible and IS what `MCC_SO_DEFAULT_SEED` evaluates first. (This retracts the previous
  entry's inference that the driver's space could not contain `-O3`.)
- *Not* the searched gate set. Forcing `MCC_AST_TEMPLATES=1 MCC_AST_PROMOTE=1 MCC_AST_INLINE=1` at `-O4` gives
  0.284 s / 3632 — indistinguishable from plain `-O4` — because the driver re-decides the config inside its children
  regardless.

**What it is:** `so_eval` scores a candidate as `text + spills * so_spill_w` with `SO_SPILL_W_DEFAULT = 48`. That is a
SPEED proxy, not a size metric — and it is choosing wrong. The driver's children all run with `MCC_SEARCH_WORKER=1`
(set by `so_setenv_cfg`), i.e. the in-process search, which on its own produces the FASTEST code measured here
(0.17 s, 29% better than `-O3`). The driver then evaluates candidates on the proxy and ships one that is **65% slower**
than the baseline it could have kept, and on nbody LARGER as well.

**Fix (1) "keep the baseline" WAS ATTEMPTED 2026-07-27 IN THE OBVIOUS FORM AND IT IS WRONG — do not redo it.**
The obvious reading is "seed the search with the baseline so nothing worse can be shipped", i.e. flip
`MCC_SO_DEFAULT_SEED` default-on (`best_gate = SO_GATE_DEFAULT` unconditionally at `mcc_superopt_search`’s seed). One line. It makes
nbody **WORSE: 3636 -> 3998 `.text`**, reproducibly and deterministically (3 clean runs each, checkpoint cache
`~/.cache/mcc/so-*.ck` cleared between). Reverted.

Why, and this is the load-bearing detail: **`SO_GATE_DEFAULT` is not a cheap candidate.** It RESTORES the user
environment, so evaluating it at `-O4` re-runs the full in-process AST search for that one evaluation, whereas an
ordinary gate word `g` is a cheap fixed configuration. The seed eval therefore burns the whole `budget_ms` before the
gate loop at `mcc_superopt_search`’s gate loop executes even once, and the driver ships the unimproved baseline. Seeding with the baseline
does not *guarantee* the baseline is a floor — it *replaces the search with* the baseline.

**What the same experiment established, which is more useful than the fix that failed:**
- **The user's baseline is genuinely never a candidate by default.** `best_gate` initialises to `0` in `mcc_superopt_search`, and
  the gate loop enumerates `g` over `[0, SO_GATE_SPACE)`, which cannot contain `SO_GATE_DEFAULT` (`0xFFFFFFFF`). So
  the earlier note "the plain baseline IS expressible and IS what `MCC_SO_DEFAULT_SEED` evaluates first" is right
  about *expressible* and wrong about *evaluated* — nothing evaluates it unless that env is set.
- **`-O4`'s own default config is far worse than `-O3`'s, and that is the real gap.** Measured on nbody:
  `MCC_SEARCH_WORKER=1 -O4` (driver off, `-O4` default gates, in-process search) = **3998**, versus `-O3` = **3596**.
  The driver then searches 3998 down to 3636 — so the driver IS improving on its own baseline by 9%; it just starts
  from a config that is 11% worse than `-O3`. `-O4` ending up larger than `-O3` is therefore NOT the driver shipping
  a result it should have rejected (this retracts that framing above); it is the `-O4` default gate set being worse
  than the `-O3` one, which is exactly what Wanted item 1 (the floor) describes.
- Current `-O3` vs `-O4` `.text` across the kernels, with the driver as-is: nbody 3596/3636 (**larger**), nsieve
  1816/1790, mandelbrot 2153/2094, matmul 2595/2329. So nbody is the ONLY kernel where `-O4` loses on size, and the
  other three are 1-10% wins.

**So the correct shape of the floor is: compile once with the `-O3` configuration, keep that object, and ship a
superopt candidate only if it beats it.** That costs one extra ordinary compile (cheap, deterministic) rather than one
extra full-search evaluation (which is what made the seeding attempt fail). The `-O3` config is not expressible as a
gate word today, so this needs a real baseline slot in the driver, not an env flip.
Remaining, unchanged: (2) recalibrate or justify `so_spill_w = 48`, which
appears to be a guess; (3) only then revisit whether `-O4` should optimise speed or size. Note `so_jitscore` already
exists to score by actual measured runtime (`so_run_score`) and is inert in practice — that is the honest metric this
proxy is standing in for.
Note the floor costs size for no measured runtime gain here (3748 vs 3636), so adopting it is a decision about
GUARANTEES — '`-O4` is never weaker than `-O3`' — not a measured win. Keep it off until the real cause is found, or
it will be credited with a fix it did not make.

Wanted:
1. **Floor the search at the `-O3` default set — LANDED 2026-07-27 for the axis that actually mattered, `PROMOTE`.**
   The principle is that the search may only ADD gates, never subtract from the default set. Applied to the one axis
   the measurement indicted: `so_setenv_cfg` used to force `MCC_AST_PROMOTE=0` whenever bit 1 of the gate word was
   clear, so a SIZE-scored search could switch promotion off — and promotion is precisely the transform that trades
   size (prologue saves) for speed (fewer spills). It now only ever sets the gate ON; when the bit is clear it calls
   the new `so_unsetenv_axis`, restoring the compiler's own default (or the user's pin) instead of forcing `0`.
   `MCC_SO_PROMOTE_FLOOR=0` restores the old behaviour and is byte-identical on `.text` for all four kernels.

   **The measurement that settles it — the `-O4` default config was the FASTEST thing on the bench and the driver was
   throwing it away.** nbody: `-O3` 3596 B / 0.23 s; `-O4` as shipped 3636 B / 0.27 s; `-O4` with promotion pinned on
   3998 B / **0.17 s**. So the driver's size-scored search was selecting a candidate **59% slower** than the baseline
   it started from, and the whole `-O4`-is-slower-than-`-O3` finding reduces to this one axis.

   Result across the kernels (best-of-5, `.text`):

   | kernel | `-O3` | `-O4` after | `-O4` before |
   |---|---:|---:|---:|
   | nbody | 0.23 s / 3596 | **0.17 s** / 3998 | 0.27 s / 3636 |
   | matmul | 0.65 s / 2595 | **0.52 s** / 3025 | 0.65 s / 2329 |
   | mandelbrot | 0.48 s / 2153 | 0.47 s / 2192 | 0.48 s / 2094 |
   | nsieve | 0.14 s / 1816 | 0.14 s / 1895 | 0.14 s / 1790 |

   **`-O4` is now never slower than `-O3` on any kernel, and 20-26% faster on two** — the property this item asked
   for. The honest cost: `-O4` is now LARGER than `-O3` on all four (it buys speed with prologue saves), where before
   it was smaller on three. That is the right trade for a level whose contract is "optimize hardest", but it does mean
   the size/speed question below is no longer hypothetical — `-O4` has now definitively picked speed.

   **The floor work is COMPLETE as of 2026-07-27, and my earlier "still open: the same floor for the other 61
   `o4`-defaulted gates" was a CATEGORY ERROR.** The superopt driver can only subtract a gate it actually sets, and it
   sets exactly **12** axes — `TEMPLATES`, `PROMOTE`, `INLINE`, `NO_CALLFUL`, `INLINE_LIMIT`, `INLINE_NODES`, `GRAFT`,
   `BITFLAG`, `CPROP_JOIN`, `CSE_JOIN`, `PROMOTE_LIMIT`, `OPT_LIMIT` (the `so_axes[]` table; the only other env it
   touches is `MCC_SEARCH_WORKER`, `MCC_AST_SPILL_OUT` and `MCC_AST_FN_CONFIG`). The other ~50 `o4` gates are never
   written by the driver at all, so a child process gets them from `ast_configure` at its `-O` level — **they are
   already floored by construction and there is nothing to fix.**

   Of the six axes the search CAN switch off, only `PROMOTE` moves runtime. Measured at `-O4`, best-of-5, each axis
   pinned versus plain `-O4` (nbody 0.17 s / 3998 B, matmul 0.52 s / 3025 B):

   | pinned | nbody | matmul |
   |---|---|---|
   | `TEMPLATES=1` | 0.17 s / 3854 B | 0.52 s / 3025 B |
   | `INLINE=1` | 0.18 s / 3998 B | 0.51 s / 3025 B |
   | `CPROP_JOIN=1` | 0.17 s / 3998 B | 0.53 s / 3025 B |
   | `CSE_JOIN=1` | 0.17 s / 3998 B | 0.53 s / 3025 B |
   | `NO_CALLFUL=0` | 0.18 s / 3998 B | 0.52 s / 3025 B |

   All within noise, against `PROMOTE`'s 59% swing. So no further floor is justified by measurement.

   **THE BIG ONE, measured 2026-07-27: `-O4` does not SEARCH on real inputs — it evaluates ONE configuration.**
   `-v64` makes the driver report its own eval count, and the budget is `optimize_search_seconds = <level>` seconds
   (libmcc.c), i.e. **4 seconds for `-O4`** — while a single evaluation of nbody takes **8.2 seconds**. The
   `while (host_clock_ms() - start < budget_ms)` loop therefore never runs a single iteration:

   | input | evals | wall |
   |---|---:|---|
   | nbody | **1** | 8240 ms |
   | nsieve | **1** | 8171 ms |
   | matmul | **2** | 11424 ms |
   | `tests/superopt/src/spillheavy.c` | 7 | 4813 ms |

   Only the small fixture gets a real search. **This reframes everything above, including my own `PROMOTE` fix**: the
   floor helped not because the search found something better, but because *the single configuration the driver
   actually evaluates* stopped having promotion switched off. Gate 0 with `PROMOTE` forced off was 3636 B / 0.27 s;
   with the floor restoring the default it is 3998 B / 0.17 s — the whole 59% win, from one evaluation. Raising the
   budget does not help on nbody: `-O10` gives 3 evals and `-O30` gives 7, and `best gate` stays 0 with `.text` 3998
   throughout.

   **RETRACTION — I previously wrote here that `TEMPLATES=1` gives nbody 3854 B at equal speed and "the size-scored
   search does NOT find it". That claim is withdrawn: the driver is behaving correctly.** Instrumenting the gate loop
   shows gate 1 (which is exactly `TEMPLATES=1`) scoring **4198, identical to gate 0**, so within the driver's own
   candidate space it is not smaller and nothing is being wrongly rejected.

   **The discrepancy that left open is now RESOLVED 2026-07-27, and the cause is a different bug: the IN-PROCESS AST
   search is NON-MONOTONIC in its budget.** Instrumenting the SEED evaluation (not just the gate loop) showed the same
   pinned environment scoring 4054 at `-O4` but 4198 at `-O30`, which is impossible if the pin is the only variable —
   and it isn't: the child inherits the `-O` level too, so its own in-process search budget scales with it. Measured
   directly with the driver out of the picture (`MCC_SEARCH_WORKER=1 MCC_AST_TEMPLATES=1`, nbody):

   | child level | `.text` | best-of-3 |
   |---|---:|---:|
   | `-O4` | **3854** | 0.17 s |
   | `-O5` | 3998 | 0.18 s |
   | `-O6` / `-O8` / `-O10` / `-O30` | 3998 | 0.17 s |

   **One extra second of search costs 144 bytes at identical runtime**, and the step is between `-O4` and `-O5`. With
   `TEMPLATES=0` every level gives 3998, so this surfaces only when the smaller configuration is reachable at all. It
   is a size regression, not a speed one (0.17 s throughout) — but it makes "a higher `-O` is at least as good" false,
   the same guarantee the `PROMOTE` floor above exists to protect.

   **Diagnosis, and it corrects my own first wording of this item.** I wrote that the in-process search "should never
   replace a candidate with a larger one on its own scoring axis". That is not what is happening, because **bytes are
   not its axis**: `ast_search_score_one` scores `ast_cost_score(trial)`, a static AST cost model, and its keep-rule
   is already strict-less on that. So this is an OBJECTIVE MISMATCH, not a broken accept-rule — with more budget the
   search reaches configurations that genuinely score better on the cost model while emitting more bytes.

   Evidence, nbody with `TEMPLATES=1` pinned, driver out of the picture:

   | scoring | `-O4` | `-O5` | `-O8` |
   |---|---:|---:|---:|
   | `ast_cost_score` (default) | **3854** | 3998 | 3998 |
   | `MCC_AST_SEARCH_EMITSIZE=1` | 3998 | 3998 | 3998 |

   So scoring by real emitted size IS monotonic here — but it never finds the 3854 configuration the cost model
   reaches at `-O4`, so it is not a drop-in fix either. Both halves need explaining before this is closed: why the cost
   model prefers a larger emit at higher budgets, and why the emit-size objective cannot reach 3854 at all. Note the
   driver's accept logic is NOT at fault and was already checked (gate 1 scores 4198, identical to gate 0).

   Secondary, unchanged: the earlier per-gate ablation over all 62 `o4` gates (`MCC_SEARCH_WORKER=1`, each forced off)
   found 20 that shrink `.text`, but 10 land on the SAME 3854 B, so they are perturbing which configuration the
   in-process AST search settles on rather than each costing size independently. Do not read that table as 20 separate
   regressions, and note it measures the in-process search, not the driver.
2a. **Two of the superopt's three search axes were DEAD — fixed 2026-07-27.** Found while measuring item 2. Each
   round is supposed to explore gate, then budget, then limit, each bounded by `slice = base_ms << round`. But
   `base_ms = seed_eval_ms * SO_SLICE_FACTOR(8)`, and at `-O4` the whole run's budget is only 4 s, so the FIRST
   slice already exceeds the entire budget and the gate loop consumes it — the budget and limit loops are then
   skipped by their own `host_clock_ms() - start < budget_ms` guard. **Proof from the banked checkpoint after 8
   compiles of the same input: `claim_gate=576` but `budget_cursor=0` and `limit_cursor=0`.** Neither axis had ever
   been touched. `slice` is now capped at a third of the REMAINING budget, so every axis gets a turn; the same
   8-compile sequence now reaches `budget_cursor=15, limit_cursor=5` (the limit axis fully exhausted).
   **Stated honestly: this bought NOTHING measurable.** `.text` and best-of-5 runtime are unchanged on nbody
   (3998 / 0.17 s), matmul (3025 / 0.52 s) and mandelbrot (2192 / 0.47-0.48 s), and unchanged on the
   `superopt/promote-floor` fixture (1681). It is landed as a defect fix — the search now explores the space it
   documents — not as a performance win, and it should not be credited with one later.

2b. **Bank the `-O3` baseline in the checkpoint.** The cached record should carry the static `-O3` configuration, not
   only the searched delta, so a later compile that hits the memo immediately gets the full static optimization set
   without re-deriving it. This is also the fix for the finding above, where a memo hit currently short-circuits the
   per-function search and the driver then burns its freed budget to arrive at the identical binary — with a banked
   floor there is a concrete result to hand back instead.
   **Quantified 2026-07-27, and it is worse than "burns its freed budget": a warm compile costs exactly as much as a
   cold one.** Eight successive `-O4` compiles of the same unchanged input: 4.70 / 4.69 / 4.70 / 4.70 / 4.70 / 4.70 /
   4.78 / 4.74 s, with `.text` settling at 1681 from the second run onward. So from run 2 every compile spends the
   full 4 s search budget to reproduce a byte-identical binary. Convergence IS reachable — the loop breaks once
   `claim_gate >= SO_GATE_SPACE` (2576) and both cursors are exhausted — but at ~72 gates banked per run that is
   roughly 35 compiles away, so in practice no build ever gets there. The cheap win available today is an
   early-out on a fully-exhausted checkpoint; the fuller fix is this item.
3. **Regression-guard it — LANDED 2026-07-27 as `superopt/promote-floor`.** A timing assertion would be flaky in CI,
   so the cell locks the FLOOR instead of the runtime, which is deterministic and is the property that actually broke:
   `-O4` output must be byte-identical on `.text` to `-O4` with `MCC_AST_PROMOTE=1` pinned. If the size-scored search
   ever subtracts promotion again, the two diverge and the cell fails. **Non-vacuous, verified both ways:** it passes
   with the floor and fails with `MCC_SO_PROMOTE_FLOOR=0` (free 1230 B vs pinned 1696 B on the fixture). Runs against
   `tests/superopt/src/spillheavy.c` — a self-contained FP/struct kernel with the nbody `advance()` shape (5 bodies,
   pairwise loop, 6 live doubles per iteration) so the cell does not depend on `vendor/plb` being checked out. Both
   legs run with a private `HOME` because `-O4` caches its search result per input in the user cache dir, and a stale
   `so-*.ck` checkpoint would make the assertion vacuous.

Note this interacts with the size/speed question above: if `-O4` remains size-scored, the floor should be stated in the
scoring axis actually in use (never LARGER than `-O3`), and the same 16% runtime regression should be re-examined once
the axis is settled.

**RE-TESTED 2026-07-27 after fidelity went 59.5% -> 75.3% faithful: the level curve did NOT move, and the reason is
now pinned to a SPECIFIC site rather than the ceiling in general.** Best-of-5, same kernels:

| kernel | `-O0` | `-O1` | `-O2` | `-Os` | `-O3` |
|---|---:|---:|---:|---:|---:|
| nbody | 0.29 | 0.29 | 0.29 | 0.29 | **0.24** |
| nsieve | 0.16 | 0.16 | 0.14 | 0.14 | 0.14 |
| mandelbrot | 0.49 | 0.49 | 0.49 | 0.48 | 0.48 |
| matmul | 0.67 | 0.68 | 0.68 | 0.67 | 0.66 |

Essentially identical to the original measurement, so +291 faithful functions bought nothing here. **That does NOT
refute the ceiling explanation below — the control says the opposite.** Per-function verdicts on nbody show
**`advance` — the hot function — is `desync` at `ast_hook_vpush`'s VALUE-MODEL guard**, along with `scale_bodies`,
with `offset_momentum` `unfaithful`. The kernels are 67-78% faithful overall, but the functions that matter are still
excluded, so the optimizer never sees the loop that dominates the runtime.

**RESOLVED 2026-07-28 — and the recorded diagnosis above was WRONG.** That group was characterised here as
register-held lvalues needing pointer provenance modelled, i.e. "not straightforwardly modellable at all". It is
nothing of the kind. Instrumenting the guard to print its inputs showed `advance` failing with
`vmask=0 lval=1 bt=VT_DOUBLE` — a `double` lvalue addressed through a register — and the minimal reproducer is two
lines:

    struct B { double x, y, vx, vy; };
    void f(struct B *p, double d) { p->vx -= d; }   /* desync at -O2, FAITHFUL at -O3 */

Plain member read (`return p->x`) and plain member write (`p->x = 1.0`) are both faithful. Only COMPOUND assignment
through a pointer desyncs — and `MCC_AST_OPASSIGN` already models it exactly. The gate was simply default-on at
`-O3` and not at `-O2` (`ast_env_gate("MCC_AST_OPASSIGN", o4 || s1->optimize >= 3)`). Discriminating controls:
`*p -= d` desyncs, but `g -= d` on a static global is faithful.

**LANDED: the default is now `>= 2`.** Measured effects, all on the same tree:

| measurement | before | after |
|---|---|---|
| mcc's own TU at `-O2`, faithful | 1392 / 1849 (75.3%) | **1436 / 1849 (77.7%)** |
| value-model guard events on that TU | 72 | **4** |
| exec-corpus ratchet gap set | 759 | **752** (7 now faithful, none regressed) |
| nbody `-O2` best-of-5 | 0.49s | **0.41s** |

68 of the 72 events were this one gate. And nbody at `-O2` is now **0.41s — exactly its `-O3` number**, with
byte-identical program output: `MCC_AST_OPASSIGN` was the ENTIRE `-O2` → `-O3` gap on that kernel. So the flat
`-O2` curve recorded below is, for nbody, not a fidelity ceiling at all but a gate staged one level too high.

The lesson worth keeping: before concluding that a desync group needs new modelling, check whether an EXISTING gate
already models it and is merely off at that level. The instrumentation that settled this printed the guard's actual
inputs (`vmask`/`lval`/`bt`) — the earlier characterisation was inferred from the guard's source condition instead,
and inferred wrong.

Bar run for the flip: host ctest 7276/7276, cross ctest 7435/7435, self-host fixpoint `s3 == s4`, and both
`runtime-bench-check` and `runtime-bench-gatewin` pass (`MCC_AST_OPASSIGN`/nbody is one of the two `GATE_WINS`; the
guard sets the env explicitly, so a default change does not disturb it).

Residual, NOT fixed here: `tests/ast/verify-baseline/x86_64-win32.txt` is stale in the SAFE direction. Measured with
`cmake-cross/mcc-x86_64-win32`: **69 gaps now against 209 in the baseline, every drift entry "now FAITHFUL", zero new
gaps.** Most of that predates this flip (it is the whole 2026-07-27 fidelity session landing). It was NOT regenerated
because that baseline's documented producer is the mingw build, and the `mingw` preset is CI-only — it fetches a
winlibs GCC and its inner build fails here with "No CMAKE_ASM_COMPILER could be found" even though
`x86_64-w64-mingw32-gcc` works standalone. Regenerating it from the Linux-hosted cross would bank a baseline from an
unvalidated producer. Whoever runs the mingw build should regenerate it; the cell can only fail as "regenerate to
bank the win".

**Original finding, still true as stated: mcc `-O1`/`-O2`/`-Os` are indistinguishable from `-O0`** on
these kernels (nbody 0.298 / 0.303 / 0.293 versus 0.299), while gcc and clang gain roughly 2x from `-O0` to `-O1`.
`-O3` is the first level that moves (0.246). That is consistent with the 48% recorder-fidelity ceiling above — a pass
that cannot run in half the functions cannot show up in a benchmark — and is the most direct evidence yet that fidelity
work is worth more than new passes.

## Follow-ups / due diligence from the 2026-07-27 optimizer-fidelity session
Every item below came out of building `optfire` and following what it found. Nothing here is speculative — each is a
loose end left by a landed change, with the specific evidence that would close it.

**F1 — DONE 2026-07-27: all three recorder gates are FLIPPED DEFAULT-ON.** `MCC_AST_MEMBER_AGG`,
`MCC_AST_MEMBER_CONST` and `MCC_AST_CMP_INVERT` each shipped as a SEPARATE commit so a regression stays attributable
(their effects overlap — all three touch recorder fidelity — and a combined flip would only make bisection harder).
Each flip was: change the default, regenerate `tests/ast/verify-baseline/x86_64-linux.txt`, then run the full bar.
Per-flip results:

| flip | gate | ratchet | fixpoint size | fuzz (400 seeds, `--gates`) |
|---|---|---|---:|---|
| 1 | `MEMBER_AGG` | 776 -> 769 gaps, regenerated | 5492351 | 397 agree, 0 miscompile |
| 2 | `MEMBER_CONST` | 769 -> 768 gaps, regenerated | 5494575 | 397 agree, 0 miscompile |
| 3 | `CMP_INVERT` | unchanged, no regen needed | 5494783 | 397 agree, 0 miscompile |

Every flip held host ctest 7229/7229, cross ctest 7388/7388, a byte-identical 3-stage self-host fixpoint
(o1==o2==o3), and all four qemu gates 10/10. The ratchet failing on flips 1 and 2 is the intended signal, not a
regression — it fails when the gap set IMPROVES, and regenerating banks the win. Flip 3 needed no regeneration
because `CMP_INVERT`'s effect does not show on the exec corpus gap set.

**The `CMP_INVERT` trade-off was taken consciously, as this entry required.** On arm64 it deliberately DESYNCS rather
than models (F2), which the full-TU measurement quantifies: 21 functions move from `unfaithful` to `desync`
(unfaithful 162 -> 141, desync 777 -> 798, faithful unchanged at 894). Both verdicts are equally excluded from
optimization, so arm64 coverage is UNCHANGED — the flip removes a wrong model at no cost there, and gains +9 on
x86_64/i386/riscv64. F2 remains open and would convert those 21 into real coverage.

Original evidence for the flip decision, kept as the record of what the bar required. Evidence, all 2026-07-27:
- **+106 faithful on x86_64, +87 on i386** over mcc's own TU, and **zero functions regress from `faithful`** — every
  transition is out of `desync` (106 -> faithful, 30 -> unfaithful, 2 -> bail), and `unfaithful`/`desync` are equally
  excluded from optimization, so the 30 are no worse off than before.
- **3-stage self-host fixpoint holds byte-identically with the gate ON** (o1==o2==o3 at 5492335).
- **Differential fuzz, 400 seeds with the full `--gates` sweep, gate ON: 396 agree, 0 miscompile, 0 buildfail.**
- **`MCC_JIT=1` == `MCC_JIT=0`** on 4 shapes with `swapped=3` real dispatch installs (non-vacuous).
- **Byte-identical with the gate OFF**; full ctest 7229/7229.
The ONLY failing cell with it on is `ast-verify-ratchet`, reporting **769 gaps against a 776 baseline** — it fails
because the gap set IMPROVED. So flipping is: set the default, regenerate `tests/ast/verify-baseline/<cpu>-<os>.txt`,
done. Not yet covered by the bar: arm64/riscv64 (tooling-blocked, see (a)) and `-O6` differential.
**`MCC_AST_MEMBER_CONST` and `MCC_AST_CMP_INVERT` now have the same evidence (2026-07-27) — all three meet the bar.**
Both hold the **3-stage self-host fixpoint byte-identically with the gate ON**, and both pass a **300-seed
differential fuzz with the full `--gates` sweep: 298 agree, 0 miscompile, 0 buildfail** each. Combined with their
coverage (`MEMBER_CONST` +46 x86_64 / +38 i386, `CMP_INVERT` +9 / +9), JIT parity, and byte-identity when off, the
flip criteria are met for all three.
**Order to flip, if flipping:** `MEMBER_AGG` first (largest, cleanest — zero regressions from `faithful`), then
`MEMBER_CONST`, then `CMP_INVERT`. Each flip is: change the default, regenerate
`tests/ast/verify-baseline/<cpu>-<os>.txt`, confirm the ratchet goes green. Flip them SEPARATELY so a regression is
attributable — their effects overlap (all three touch recorder fidelity) and a combined flip would make bisection
harder for no gain.
`CMP_INVERT` carries one extra caveat the other two do not: on arm64 it deliberately DESYNCS rather than models (F2),
so flipping it slightly reduces arm64 coverage in exchange for removing a wrong model. That is the right trade but it
should be a conscious one. `MCC_AST_MEMBER_AGG` (+106 faithful), `MCC_AST_MEMBER_CONST`
(+46) and `MCC_AST_CMP_INVERT` (+9) are all default-OFF and byte-identical off. Each needs, before flipping:
(a) cross-arch validation — **DONE on ALL FOUR TARGETS 2026-07-27; the recorded blocker was wrong.** This entry said
arm64/riscv64 "cannot compile a large TU on an x86_64 box (`mcchost.c: field not found: pc`, host-shaped
signal-context fields), so they need a real cross sysroot or a native run." They do need a cross sysroot — and one is
already vendored. Pointing each cross compiler at its own musl stage3 headers compiles the whole amalgamation on this
box: `mcc-<cpu> -B cmake-cross --sysroot vendor/gentoo-stage3-<cpu>-musl -isystem $ROOT/usr/include` plus the `-D`/`-I`
set lifted from `compile_commands.json` (drop `MCC_TARGET_*`, `MCC_GITHASH` — it contains a space and splits into two
argv entries — and the other arch's `-I`). arm64 5730847 B, riscv64 7703633 B, both rc=0. Faithful delta on the full
`src/mcc.c` TU:

| gate | x86_64 | i386 | arm64 | riscv64 |
|---|---:|---:|---:|---:|
| `MEMBER_AGG` | +106 | +87 | +97 | +97 |
| `MEMBER_CONST` | +46 | +38 | +78 | +83 |
| `CMP_INVERT` | +9 | +9 | +0 | +9 |

So none of the three is x86_64-specific, and `MEMBER_CONST` is roughly **twice as valuable on arm64/riscv64** as on
x86_64. `CMP_INVERT`'s arm64 `+0` is the documented by-construction case (F2): it moves 21 functions from
`unfaithful` to `desync`, and since both are excluded from optimization the coverage is unchanged while a wrong model
is removed. **Do NOT use the exec corpus as the cross-arch oracle for these gates** — it yields +1/+0/-2, far too
small to distinguish anything, for the same reason the `optfire` corpus is useless here: neither contains enough
nested-struct member access. The large TU is the only oracle that separates them.
Superseded note, kept for the method: **i386 DONE 2026-07-27, arm64/riscv64 were open.** On a real TU (`src/mcc.c` via a
native 32-bit `mcc32`, rc=0) all three gates reproduce the x86_64 shape: `MEMBER_AGG` desync 715 -> 601 and faithful
**811 -> 898 (+87** vs +106 on x86_64), `MEMBER_CONST` +38 (vs +46), `CMP_INVERT` +9 (vs +9). So the gates are not
x86_64-specific. Two caveats: the freestanding `optfire` corpus shows NO difference on ANY target for `MEMBER_AGG`
(those cases contain no nested-struct member access, so it cannot exercise the gate — do not use it as the cross-arch
oracle), and arm64/riscv64 cannot compile a large TU on an x86_64 box (`mcchost.c: field not found: pc`, host-shaped
signal-context fields), so they need a real cross sysroot or a native run. `CMP_INVERT` on arm64 is known BY
CONSTRUCTION — it desyncs rather than mismodels there (F2);
(b) `ast-verify-ratchet` baseline regenerated, which currently FAILS gate-on only because the gap set IMPROVED
(769 vs 776 for `MEMBER_AGG`, 775 vs 776 for `MEMBER_CONST`);
(c) ~~a JIT status statement~~ **DONE 2026-07-27.** All four new gates (`MEMBER_AGG`, `MEMBER_CONST`, `CMP_INVERT`,
`SEARCH_FLOOR`) hold **`MCC_JIT=1` == `MCC_JIT=0`** across 4 program shapes (int kernel, double kernel,
snprintf/string, 64-bit math) — 16 of 16 identical. **Non-trivially**: `--stats` reports `recompiles=11 swapped=3
refused=5` with each gate on, i.e. the JIT really installed 3 dispatch variants rather than the equality being vacuous,
which is the trap the i386 row records.
Scope of the claim, stated so it is not over-read: x86_64 only, `-O2`, and the JIT counters are IDENTICAL with each
gate on or off (11/3/5 throughout). That is expected — these gates change the RECORDER's fidelity while the JIT
recompiles from the intent blob — but it does mean the test shows 'gate-on does not break JIT parity', not 'the gate
changes JIT-compiled code'. Cross-arch JIT validation is still part of (a).
They are independent and can flip separately; `MEMBER_AGG` is the largest and the cleanest (zero regressions from
`faithful`).

**F2 — arm64 `CMP_INVERT`: DIAGNOSED 2026-07-27, and the previous framing was BACKWARDS.** This entry said
"flipping the op would model it WRONGLY, and a wrong model that happens to replay identically is the latent
miscompile path this hook exists to close." That is not what is happening. Measured by building an arm64 compiler with
the desync arm removed and dumping the diff on `int f(int a){ return !!a; }`:

    base: ... e0 17 9f 1a  00 00 00 52     cset w0, eq ; eor w0, w0, #1   (20 B)
    repl: ... e0 07 9f 1a                  cset w0, ne                    (16 B)

The two are **semantically identical** — `!(a==0)` and `a!=0` — and the REPLAY is the better code. So the `^1` model
is CORRECT on arm64; what diverges is that the arm64 BASELINE emits a redundant instruction. x86_64 emits the optimal
form for the same sources (`setne`, and `setge` for `!(a<b)`), which is exactly why the hook replays faithfully there.
arm64 emits `cset eq; eor #1` and `cset lt; eor #1`. This is a **missed arm64 optimization**, present at `-O0` and
`-O2` alike, not a modelling hazard.

Mechanism, pinned to the line: `gen_opi`/`gen_opl` (arm64-gen.c) call `arm64_gen_opil(op, …)`, which **emits the
`cmp` and the `cset` EAGERLY**, and only then `arm64_vset_VT_CMP(op)` marks the value `VT_CMP` with the sentinel
`vset_VT_CMP(0x80)`, stashing the result register in `cmp_r`. Because the `cset`'s condition field is already in the
instruction stream, a later `gen_test_zero` inversion cannot rewrite it — `vtop->cmp_op ^= 1` only flips the sentinel's
low bit, and `arm64_load_cmp` materialises that bit as `vpushi(1); arm64_gen_opil('^', 0)`. x86_64 keeps `VT_CMP`
LAZY (the condition lives in `cmp_op` until a `setcc` is emitted at materialisation), so inverting is free there.

Two candidate fixes, neither attempted — the gate is sound without this and it is real backend work:
1. **Make arm64's `VT_CMP` lazy** like x86_64: defer the `cset` to materialisation so the condition field is still
   editable. Correct and removes the redundant instruction everywhere, but it restructures the arm64 compare path.
2. **Peephole the inversion**: in `arm64_load_cmp`, patch the already-emitted `cset`'s condition field instead of
   emitting `eor`, when that `cset` is still the last instruction. Much smaller, but only valid when nothing was
   emitted in between, so it needs a guard.
Either one makes baseline == replay AND drops an instruction on every `!` of a comparison. Worth doing on value
grounds now that `CMP_INVERT` is default-on: it would convert arm64's 21 `unfaithful`->`desync` functions into real
coverage (see F1 flip 3).

**F3 — `MCC_AST_MEMBER_CONST`'s 4-function residue. RE-MEASURED 2026-07-27 with all three gates now default-on, and
the "benign, same length" characterisation is HALF WRONG — do not act on it.** The residue is still exactly the four
recorded functions (confirmed by differencing the unfaithful set with the gate on vs off: 157 vs 153), but they are
two different phenomena, not one:

| function | baseline | replay | what it is |
|---|---:|---:|---|
| `set_flag` | 912 B | 912 B | operand-order swap, as described |
| `ast_hash_of` | 859 B | 859 B | operand-order swap, as described |
| `ast_sid_node` | 988 B | **979 B** | 9 B SHORTER — not operand order |
| `so_ckpt_write` | 1181 B | **1228 B** | 47 B LONGER, and a CALL moves |

**`so_ckpt_write` is not benign.** Disassembled from the `MCC_AST_VERIFY_DIFF` dump at +171:

    base:  mov [rbp-0xaa8],rax ; cmp rax,0 ; je +0x2dc ; lea … (error block)
    repl:  mov [rbp-0xaa8],rax ; lea rax,[rip+0] ; mov rsi,rax ; mov rax,[rbp-0x8]
           mov rdi,rax ; call … ; cmp rax,0 ; je +0x303

The replay emits a two-argument `call` and then compares ITS return value, where the baseline compares a value it
already holds and branches without calling. That is a semantic difference, not a scheduling one — the replayed body
would perform a call the parser's body does not make at that point.

**Consequence for how this item gets closed: F3 is NOT "optional polish" and must NOT be closed by relaxing the
faithfulness check** (e.g. by teaching it to tolerate operand-order or length differences). The check is doing exactly
its job here — it is rejecting a replay that is not equivalent. Two of the four may well be benign reorderings worth
canonicalising; the `so_ckpt_write` case needs the MODEL corrected, and until it is understood the other two should
not be waved through by a blanket tolerance that would also admit this one. Verification status, stated so it is not
over-read: `so_ckpt_write` verified by disassembly, the other three by length and byte-pattern only.

**F3a — NEW 2026-07-27: assignment-in-condition makes the AST replay DUPLICATE the call. Found while diagnosing F3;
it is a general modelling bug, not a `MEMBER_CONST` artifact.** `if ((h = call(...)))` replays as "store the value,
then call again and test THAT" instead of "store the value, then test the stored value". Minimal reproducer, which
needs none of the F1 gates:

    int calls;
    static void *stub(const char *a, const char *b) { (void)a; (void)b; calls++; return 0; }
    static int sink(void *h) { return h != 0; }
    static int f(const char *p) { void *h; if ((h = stub(p, "rb"))) return sink(h); return 0; }

`f` is `unfaithful`, baseline 58 B vs replay 80 B:

    base:  mov [rbp-0x10],rax ; cmp rax,0 ; je ; mov rax,[rbp-0x10] ; mov rdi,rax ; call sink
    repl:  mov [rbp-0x10],rax ; lea rax,[rip+0] ; mov rsi,rax ; mov rax,[rbp-0x8] ; mov rdi,rax
           call stub ; cmp rax,0 ; je ; mov rax,[rbp-0x10] ; mov rdi,rax ; call sink

The replay re-derives the condition from the RHS SUBTREE rather than from the assigned value, so the call is emitted
twice — `calls` would be 2 instead of 1. **No miscompile ships**: the faithfulness check rejects the body and the
parser's code is used, verified at runtime (`calls=1`, matching gcc). The cost is coverage, and it is not niche —
mcc's own source has **26 `if ((x = …))` and 18 `while ((x = …))` sites**, and the idiom is ubiquitous in C generally
(`if ((f = fopen(…)))`). This is also the true cause of one of F3's four functions: `so_ckpt_write`'s
`if ((f = host_fopen(path, "rb")))`.

**MECHANISM CONFIRMED 2026-07-27 with a purpose-built diagnostic — it is a DANGLING REPARENT, and the tree becomes a
DAG.** `ast_add_child` does not copy or detach; it OVERWRITES `a->parent[child] = parent` while the previous parent's
`first_child`/`next_sib` links still point at the node. mcc already ships a flag to observe this — `MCC_REPARENT_DBG`,
which warns whenever a child that already has a parent is re-parented. On the reproducer below it fires exactly once
and names the culprit:

    [reparent] child=7(k11) from 8 to 9
    [ast-verify] unfaithful   ?   f

`k11` is `AST_Invoke` — the CALL node itself. It is re-parented from the Store (8) to the consuming node (9), but the
Store's child list still references it, so the node is reachable from BOTH parents. Replay walks children via
`first_child`/`next_sib`, hits it under each, and emits the call twice. **So the duplication is not "the residual is
the RHS subtree" as I first put it — it is that the residual node is silently STOLEN from the Store while the Store
keeps a dangling link to it.** That also explains why the sibling case behaves differently: the chained-assignment
path in `ast_hook_vstore` DOES guard against this (`chained = … && ast_parent(ast_cur, value) != AST_NONE` then
`ast_dup_sub`), but the guard is conditioned on `ast_chainstore_env` and only covers store-into-store, not
store-into-condition.

**OPEN LEAD, worth checking before the fix — do any FAITHFUL functions carry a stolen node?** It matters because
`faithful` is what gates the optimizer passes: if a stolen-node DAG only ever occurs in `unfaithful` bodies it is
inert (nothing optimizes them), but if a faithful body contains one then passes are transforming an invalid model and
re-emitting from it, which is the one failure mode that is NOT caught by the byte check. Counting `MCC_REPARENT_DBG`
events over mcc's own TU at `-O2`: **408 events total, and 69 of them are in functions whose final verdict is
`faithful`** (58 unfaithful, 23 desync across six sites, 1 bail).

That count was only a LEAD — `ast_add_child` also fires for legitimate restructuring — so it was settled properly
with the checker it called for. **SETTLED 2026-07-27: the DAG hazard is INERT. Zero violations in any FAITHFUL body.**

`MCC_AST_TREECHK` (mccast.c, opt-in, O(nodes)) verifies the arena really is a tree: no node sits in two child chains,
and every node's `parent[]` agrees with the chain it was found in. Results — **187 violations over mcc's own TU (79 of
them genuine two-chain DAGs, across 58 functions) and 44 over the exec corpus, ALL of them in bodies the recorder had
already REJECTED**, at `-O2`/`-O3`/`-Os` alike. So no optimizer pass has ever been handed one of these models, and the
one failure mode the always-on byte check cannot catch does not occur in practice.

**Locked as `ast/treecheck`** (`tests/ast/treecheck.sh`, exec corpus at `-O2`/`-O3`/`-Os`). Non-vacuous: forcing every
body to report FAITHFUL makes it fail with 44 violations. The checker also gives F3a a precise one-line diagnostic —
on the reproducer it prints `node 7(k11) is in TWO child chains: 8 and 9`, naming the stolen `AST_Invoke`.

Consequence for F3a: it is now confirmed to be **purely a coverage item with no latent correctness exposure** in
current output. Combined with the always-on byte check above, a wrong attempt at it can lose optimization but cannot
miscompile — the remaining caution is only about keeping the MODEL semantically right.

**Tried and REJECTED 2026-07-27 — making `ast_add_child` UNLINK the child from its old parent's chain before
re-parenting. It SEGFAULTS the compiler.** This is the obvious general fix (a node should only ever be in one chain,
and it would repair every dangling case at the source), and it is worth recording that it does not work, because it
looks correct: for a legitimate re-parent the old owner no longer holds the node, so the unlink is a no-op, and
`ast/treecheck` had already shown every dangling case lands in a rejected body. Implemented with a bounded
`ast_unlink_child` helper (fix `first_child`/`next_sib`/`last_child`/`nchild`, then re-parent).

Result: `mcc` builds and the small tests pass — `exec/*` was 300/300 and the plain TU compile returned 0 — but
compiling mcc's own amalgamation under `MCC_AST_VERIFY=1` **crashes with SIGSEGV after 39 of 1848 functions**. So
something in the recorder/replay path depends on the child remaining threaded into its old parent's chain after a
re-parent; the current non-unlinking behaviour is LOAD-BEARING, not merely sloppy. Reverted.

**Then the local fix was ATTEMPTED 2026-07-27. It WORKS for the basic case and CRASHES on one shape; reverted, with
the design validated and the blocker named.** Shape implemented: a new leaf kind `AST_StoreVal` handed back as the
assignment's residual instead of the RHS subtree (it references its Store by INDEX in `ival`, never by parent, so the
model stays a tree and the RHS keeps exactly one owner); a finalize pass `ast_finalize_storevals` that sets a new
`AST_FB_STORE_VALUE_LIVE` fbit on the Store only when the marker actually acquired a parent; replay of a value-live
Store skipping its `vpop()` so the value stays on the vstack exactly as the parser leaves it; and `ast_replay_value`
treating the marker as a no-op.

**It demonstrably works — `if ((h = stub(p,"rb"))) return sink(h);` goes from `unfaithful` to `FAITHFUL`.** That is
the coverage this item exists to recover, so the approach is sound. Isolated shapes all compile clean:
if-condition, while-condition, chained `a = b = f()`, short-circuit `&&`, nested `(b = f()) + 1`, ternary, and
`return`.

**SECOND ATTEMPT 2026-07-27 got it WORKING and still reverted it — the blocker is now a GATE INTERACTION, not a
crash.** The segfault below was diagnosed and fixed: `ast_replay_value` for the marker emitted NOTHING
unconditionally, so when the guard declined to make a store value-live the consumer was handed a missing operand and
`gfunc_call` read past the vstack (`bt` bottoms out in `gfunc_call`). Correct fallback: if the store is not
value-live, re-emit its RHS — that reproduces the pre-F3a double evaluation, so the body lands in `unfaithful` as
before, which is safe. With that, plus a tight guard requiring the marker to be a DIRECT child of the statement
immediately following the store:

- **0 crashes over all 258 exec files** (was 1)
- the canonical case is fixed: `if ((h = stub(p,"rb"))) return sink(h);` is **FAITHFUL**
- `ast-verify-ratchet` recorded the win it was built to record: **`if_cond` moved out of the gap set, 776 -> 775**
- mcc's own TU: **faithful 1091 -> 1094, unfaithful 158 -> 156**
- self-host fixpoint byte-identical, `ast/treecheck` clean at `-O2`/`-O3`/`-Os`, 400-seed gate-swept fuzz 397 agree
  0 miscompile, all four qemu triples 14/14

**THIRD ATTEMPT: LANDED 2026-07-27.** The CHAINSTORE interaction below is resolved — when the outer store's value
is a marker, `ast_hook_vstore` resolves it back to the inner store's RHS *before* the chained test, so
`ast_parent(value) != AST_NONE` sees what it always saw and the gate fires exactly as before. `optfire/chainstore`
and `runtime-bench-gatewin` both green again.

Final shape, all three pieces needed: (1) the residual is an `AST_StoreVal` marker referencing its Store by INDEX, so
the RHS keeps one owner and the model stays a tree; (2) `ast_finalize_storevals` sets `AST_FB_STORE_VALUE_LIVE` only
when the marker is a DIRECT child of the statement immediately following the store — anything nested (notably a call
argument, where `gfunc_call` pushes the remaining args in between) keeps the old behaviour; (3) replay of a
value-live Store skips its `vpop()`, and a marker whose store is NOT value-live re-emits the RHS rather than emitting
nothing, which is what previously segfaulted `gfunc_call`.

Result: **`if ((h = f(...)))` is now FAITHFUL**, mcc's own TU goes faithful 1091 -> 1094 and unfaithful 158 -> 156,
and `ast-verify-ratchet` banked the win it was built for (`if_cond` left the gap set, 776 -> 775). Validated on the
full bar: host ctest 7253/7253, cross 7412/7412, self-host fixpoint byte-identical (5508863), `ast/treecheck` clean at
`-O2`/`-O3`/`-Os`, 400-seed gate-swept fuzz 397 agree 0 miscompile, all four qemu triples 14/14, and 0 crashes over
all 258 exec files.

**Still open — and SURVEYED 2026-07-27 so the next attempt does not start blind.** The remaining 7 ratchet entries
are NOT one problem. Instrumenting `ast_finalize_storevals` to report, per marker, whether the guard accepted it:

| shape | markers made | guard | verdict | blocker |
|---|---:|---|---|---|
| `if ((h = f()))` | 2 | accepted | **faithful** | — done |
| `while ((h = f()))` | 1 | **ACCEPTED** | unfaithful | back-edge — and the obvious fix MISCOMPILES, see below |
| `a = (b = f()) + 1` | 1 | rejected | unfaithful | correctly rejected — see below |
| `(a = f()) ? … : …` | 1 | accepted | **faithful** | done (leftmost-leaf guard) |
| `return (a = f()) + a` | 1 | accepted | **faithful** | done (leftmost-leaf guard) |
| `f((a = g2(v)) + 0)` | 1 | rejected | unfaithful | `gfunc_call` vstack juggling |
| `a = b = f()` | **0** | n/a | unfaithful | marker is resolved away by the CHAINSTORE path |
| `(x = f()) && (y = g2())` | **0** | n/a | `desync` (store-in-ternary/landor) | desyncs before any marker is made |

**The most valuable one, `while ((c = getchar()) != EOF)`, PASSES the guard and is still unfaithful** — so its
blocker is structural, not the vstack invariant the guard exists to protect. The store is recorded as a statement in
the enclosing BB, but a `while` re-evaluates its condition on every iteration, so the loop's back edge has to
re-execute the store; replay emits it once, ahead of the loop. Fixing it means recording the store INSIDE the loop's
condition region, which is a recorder-structure change, not a guard tweak. **Do not try to widen the guard for this
one — the guard is already letting it through.**

Two of the eight never reach the marker at all: `a = b = f()` has its marker resolved back to the inner RHS by the
CHAINSTORE path (deliberate — see the fix above), and the short-circuit case desyncs at `ast_hook_vstore`’s ternary/landor-region guard before any
store is modelled.

**TRIED AND REVERTED 2026-07-27 — "emit the store AT its marker, skip it in the BB". It fixes `while` and it
MISCOMPILES. This is the single most important negative result in this item; do not retry it.** The idea is clean:
instead of leaving the value live, skip the Store where it sits in the basic block and emit it in full at the marker.
That reproduces the parser for `if`, and for a loop it puts the store INSIDE the condition where the back edge
re-executes it — no change to the loop node's arity, no unlink, no preamble child. It works, on the face of it:
`while ((h = f(n)))` becomes **faithful**, along with `if`/ternary/`return`, and mcc's own TU jumps
**faithful 1101 -> 1111, unfaithful 149 -> 139** with zero crashes over the exec corpus.

**But `tests/exec/optimizer/assign_value_effects.c` returns 1 at `-O2` and `-O3` while being correct at
`-O0`/`-O1`.** That is a real miscompile, and it is exactly the failure mode recorded above under "replay bugs are
safe, model bugs are not": the body now PASSES the byte check, so the optimizer passes run on it — and the model is a
lie. The Store node still sits at its original position in the basic block while its effect actually happens inside
the following statement, so any pass reasoning about statement order (DSE, CSE, const-prop) draws the wrong
conclusion. The always-on byte comparison cannot catch this, because it only ever validates the UNOPTIMIZED replay.

The landed value-live approach does not have this problem precisely because it leaves the Store where it is and only
skips the `vpop()` — the node's position still tells the truth about when the store happens. **So `while` cannot be
fixed by relocating the emission; it needs the store to genuinely live inside the loop's condition region in the
MODEL** (a preamble child on the loop node, which changes its arity and therefore every loop pass — interchange,
fusion, tile, LICM). That is the real cost of this one, and it is why it stays open.

**GUARD WIDENED 2026-07-27 from "direct child" to "LEFTMOST LEAF", which is the real invariant.** The value must
still be on TOP of the vstack when the marker is reached; that holds whenever the marker is the first thing the
following statement evaluates, not merely when it is a direct child. Implemented by walking up from the marker and
requiring first-child at every step (and refusing to cross an `AST_Invoke`, since `gfunc_call` pushes around its
arguments). Gains `(a = f()) ? … : …` and `return (a = f()) + a`. mcc's own TU: **faithful 1094 -> 1101, unfaithful
156 -> 149**; cumulative over the whole F3a item, **1091 -> 1101 faithful, 158 -> 149 unfaithful**.

`a = (b = f()) + 1` stays rejected and that is CORRECT, not a gap to close: the enclosing statement is
`Store(a, Binary(marker, 1))`, whose first child is the lvalue `a`. The parser pushes that lvalue onto the vstack
before evaluating the right-hand side, so by the time the marker is reached the pending value is no longer on top.
The first-child walk rejects it for exactly that reason.

Validated: host ctest 7253/7253, cross 7412/7412, self-host fixpoint byte-identical (5510079), `ast/treecheck` clean
at `-O2`/`-O3`/`-Os`, 400-seed gate-swept fuzz 397 agree 0 miscompile, qemu 14/14 on all four triples, 0 crashes over
all 258 exec files, and `optfire/chainstore` + `runtime-bench-gatewin` still green.

**The reverted second attempt's failure, kept because the trap is general: it silently disabled
`MCC_AST_CHAINSTORE`.** For `a = b = c` the OUTER store now
receives the marker as its value, and CHAINSTORE's detection is
`chained = ast_chainstore_env && value != AST_NONE && ast_parent(ast_cur, value) != AST_NONE` — a freshly created
marker has no parent yet, so `chained` is never true and the gate stops firing entirely. Caught by two independent
cells: `optfire/chainstore` ("objects are byte-identical") and `runtime-bench-gatewin` ("win collapsed to +0.4% over
14 runs, need >= 8%"). **+3 faithful does not buy the loss of a gate with a measured ~8% benchmark win**, so this
needs the interaction resolved first — the marker must resolve back to the inner store's RHS for CHAINSTORE's
parent test, or CHAINSTORE must learn the marker form.

Worth noting the two cells that caught it are exactly the kind this session has been adding, and neither the fidelity
counters, the fixpoint, the fuzz, nor the tree checker noticed — a gate silently ceasing to fire is invisible to all
of them.

**The FIRST attempt's blocker, kept for the record: an assignment inside a CALL ARGUMENT segfaulted the compiler** — `return f((a = g2(v)) + 0) + a;`
reproduces it alone. 1 crash in 258 exec files (only `assign_value_effects.c`, which covers that shape deliberately).
The value is consumed from inside `gfunc_call`'s own vstack juggling, so the store's live value is not where replay
assumes. **An adjacency guard did NOT fix it** — requiring the marker's owning statement to be the store's immediate
next sibling in the same basic block still crashes, so the interaction is subtler than "the vstack entry went stale"
and needs to be understood before this is retried. Do not simply re-apply the patch with a wider guard.

**Consequence for how F3a must be fixed: it has to be LOCAL to the vstore/consumer path, not a change to
`ast_add_child`'s contract.** Whatever takes ownership of the residual has to do so knowingly at the point where the
value is consumed, leaving every other re-parent in the compiler exactly as it is today.

**Process note — this nearly produced a false measurement, for the second time in this session.** The first fidelity
run after the change printed `33 faithful / 5 desync / 1 bail` and an object that existed, which reads like a
catastrophic fidelity regression. It was a SIGSEGV: the driver had already written a partial object, and the verdict
histogram was simply truncated at the crash. Only checking `rc` (139) revealed it. **A shrinking verdict histogram
means "measurement aborted", not "fidelity collapsed" — check the exit status.**

**Also tried and rejected 2026-07-27: making `ast_add_child` DESYNC on a dangling re-parent** (detecting that the child is
still threaded into its old parent's chain, which is cheap and bounded by that parent's fan-out) so the recorder never
builds a DAG at all, instead of building one and relying on it being rejected downstream. Two reasons it was dropped,
in order of weight: (1) **it buys no coverage** — it converts `unfaithful` into `desync`, and both are equally
excluded, so the only gain is a tidier invariant that `ast/treecheck` already proves is harmless; (2) `ast_add_child`
sits near the top of mccast.c, well before `ast_capture`, `ast_desync` and the `AST_SET_DESYNC` macro exist, so it
needs forward declarations threaded back through the file — real churn in the arena's core mutator for a cosmetic
result. **If someone wants this anyway, note the trap that caught me**: mccast.c is `#include`d into the amalgamated
`src/mcc.c`, so a compile error there makes any fidelity measurement taken with the *previous* binary silently bogus
(the TU being measured fails to compile, and the verdict histogram just gets shorter — 685 functions instead of 1848).
Check the compile succeeded before reading the counts.

**ROOT CAUSE, localised to one line — `ast_hook_vstore` (`ast_hook_vstore`’s residual assignment):**

    ast_add_child(ast_cur, st, lval);
    ast_add_child(ast_cur, st, value);
    ast_add_child(ast_cur, ast_cur_bb, st);
    ast_vs[ast_vn - 2] = value;   /* <-- residual value of the assignment IS the RHS subtree */
    ast_vn--;

An assignment leaves the RHS EXPRESSION TREE as its residual model value, so any later consumer of `(h = expr)`
replays that whole subtree — re-emitting its call. The parser does the opposite: it materialises the value once and
keeps it live, and the condition tests what is already there.

**Do not "fix" this by pointing the residual at the lval.** The replay would then emit a LOAD from the assigned
location that the parser never emits, so the body stays unfaithful — the coverage is not recovered, and every
currently-faithful `a = pure_expr` consumer would change bytes too. The residual needs to be a node kind meaning "the
value this Store already produced", which replay reuses instead of re-evaluating.

**And that node cannot simply POINT AT the Store — the model is required to be a TREE, which is the structural reason
this is a refactor rather than a local edit (established 2026-07-27).** The arena has no DAG support: both places that
could otherwise share a node take a deep copy specifically to avoid a second parent — `ast_hook_vpush` for the
compound-assign vdup ("Duplicate the top ast_vs AST node via a deep copy so the model stays a tree") and
`ast_hook_vstore` itself ("Give this store its own deep copy … so the model stays a tree"), both via `ast_dup_sub`.
A `StoreValue` node referencing the Store would create exactly the second parent those copies exist to prevent, and
copying the Store instead reintroduces the duplicate evaluation this item is about.

So the principled fix is to model assignment as an EXPRESSION node — which is what it is in C — rather than as a
statement plus a residual, and have the statement form be "expression, value discarded". That is a real refactor of
the store path, not a patch, and it was NOT attempted here. A cheaper variant worth evaluating first: leave the value
on the vstack at replay (emit `vstore()` without the `vpop()`) and mark the consumer as "already on the stack", but
that only works while the Store is the immediately-preceding replayed statement, so it needs a verified ordering
invariant and a safe bail-out when a pass has moved things — otherwise a stale vstack entry corrupts everything
downstream.

The same root cause is already noted three lines above in that function's own comment for a sibling case: *"this does
NOT make the chained-assignment idiom faithful; that has a separate cause (the parser materialises the value once and
chains two vstores, the replay emits two independent stores)"*. Chained assignment `a = b = c` and
assignment-in-condition are the SAME defect seen twice — both are "the value a Store produced" being re-derived rather
than reused. Fixing the residual properly should close both.

**IMPORTANT CORRECTION to my own repeated caution on this item (2026-07-27).** I wrote several times that F3a "has a
correctness-shaped failure mode — a wrong fix here duplicates or drops a call". **That is wrong for the REPLAY side,
and the distinction matters for how this and every similar modelling item should be approached.** The faithfulness
check is ALWAYS ON, not gated behind `MCC_AST_VERIFY`: `ast_replay_body()` emits the replayed body and it is then
compared byte-for-byte AND relocation-for-relocation against the parser's own output —

    faithful = new_len == body_len &&
               memcmp(cur_text_section->data + ast_body_ind_sv, orig, body_len) == 0 &&
               new_rel - ast_reloc0_sv == rel_len &&
               (rel_len == 0 || ast_reloc_range_equiv(...));

— and EVERY consumer is gated on that flag: the optimizer passes, `ast_search_select`/`ast_search_axis_pick`, the ROI
scorer, `ast_inline_retain`, `ast_reemit_retain` and JIT dispatch. The whole replay runs under a `setjmp` error sink
as well. **So a replay that does not reproduce the parser's bytes exactly is never used for anything: a replay bug
degrades to lost coverage, not a miscompile.** That is a much cheaper experiment than this file has been assuming,
and it is why the existing gates (`optfire`, the ratchet, the exec corpus) are the right validation for replay work.

**Where the real danger is, stated precisely so this correction is not over-read:** a change that makes the MODEL
semantically wrong while the unoptimized replay still happens to be byte-identical IS dangerous, because the optimizer
passes then transform a wrong tree and re-emit from it — and that output is not compared against anything. So:
**replay bugs are safe, model bugs are not.** A fix to F3a changes what the model records, so it still needs the exec
corpus and the differential fuzz — but the byte check will catch the ordinary failure of simply getting the emission
wrong, which is the failure this item was most likely to hit.

**GENERALISED 2026-07-27 into `tests/exec/optimizer/side_effect_order.c`, because the class of bug it catches is not
specific to F3a.** A MODEL bug is invisible to every other gate in this repo: the replay is byte-compared against the
parser before it is used, so a bad replay only costs optimization — but once a body is ACCEPTED the passes transform
the model and re-emit, and *that* output is compared against nothing. Such a bug therefore miscompiles only at `-O2`
and above. It happened for real (see "emit-at-marker" above, which was correct at `-O0`/`-O1` and wrong at `-O2`), and
the only reason it was caught is that the effects were COUNTED — **the repo's other optimizer goldens fold results
into a checksum, which a reordered or dropped side effect can leave unchanged.**

So the new file pins side-effect ORDER and COUNT across every construct the recorder models with a dedicated hook:
ternary arms, `&&`/`||` short-circuit (including that the right operand is not evaluated at all), comma sequencing,
compound assignment, pre/post increment, nested calls and call arguments. Each effect appends a tag to a log and
`main` compares the log to an exact string, so ordering is pinned rather than summed. Golden `effects ok`. Agrees with
gcc at `-O0/-O1/-O2/-O3/-Os` and with mcc at those plus `-O4`; 23 ctest cells. **Mutation-tested against both bug
classes it targets**: breaking `&&`'s short-circuit exits 3, and reordering the operands of a nested call expression
exits 10.

**The guard for the F3a fix LANDED 2026-07-27, before the fix itself: `tests/exec/optimizer/assign_value_effects.c`.**
Ten shapes of assignment-used-as-a-value (`if`/`while` condition, chained `a = b = f()`, both arms of a `&&`, nested
`(b = f()) + 1`, as a call argument, in a ternary, in a `return`), each COUNTING its own side effects so an extra or
missing evaluation changes stdout instead of hiding in a checksum. Golden `calls=14 calls2=2`. It also pins the
short-circuit case where the right operand must NOT be evaluated at all. Agrees with gcc at `-O0/-O1/-O2/-O3/-Os` and
with mcc at those plus `-O4`; 23 ctest cells. **Mutation-tested against the exact defect**: rewriting one case so the
RHS is evaluated twice makes the program exit 2 and the cell go red.

**Bonus, and it is the useful part: `ast-verify-ratchet` now carries a NAMED INVENTORY of F3a's footprint.** Adding
the file grew the gap set 768 -> 776, and all 8 are it: `if_cond`, `while_cond`, `chained`, `nested_assign`,
`assign_in_arg`, `assign_in_return`, `assign_in_ternary` (unfaithful) and `shortcircuit_both` (desync). Baseline
regenerated to bank them. **So the ratchet is now the progress meter for this item** — a correct fix makes those 8
entries disappear, and the ratchet fails until the baseline is regenerated to record the win. Note `chained` being in
that list is the direct confirmation that `a = b = c` is the same defect, which the `ast_hook_vstore` comment had
called "a separate cause".

**F4 — The `&&`/`||` desync site: MEASURED 2026-07-27, and it is one cause, not four.** Instrumenting the guard in
`ast_hook_landor_operand` over mcc's own TU: **32 events, ALL `c >= 0`** — the first operand of the `&&`/`||` folded
to a compile-time constant. Zero from `ast_in_call`, `ast_in_op`, `ast_lor_top >= 16` or `ast_vn < 1`, so three of the
four disjuncts never fire here and only the constant one matters.
Why it desyncs is defensible: with a known condition the front end short-circuits and one arm may never be emitted,
so the recorder cannot model what codegen did not produce. But it is also the case where the whole expression is
trivially foldable — the result is either the constant or the other operand — so **modelling it as that, instead of
desyncing, looks like the cheapest remaining fidelity win** at 4% of desyncs and a single well-defined shape.
**Precondition VERIFIED 2026-07-27 — modelling is safe.** `int cf(int a){return 0 && side(a);}` desyncs at the guard
and emits `mov $0x0,%eax` with **zero relocations to `side`**, so the un-emitted arm really is absent rather than
branch-eliminated later. `int dyn(int a,int b){return a && b;}` is already `faithful` and is the control.
**Attempted test-first and STOPPED at red, deliberately.** A `cli/landor_const_faithful` cell was written asserting
`cf`/`ct` become faithful with `dyn` unchanged; it goes red as expected (`desync` (landor-constant)). It was REVERTED rather than
left failing or satisfied with a guess, because tracing the hook shows the protocol is subtler than the guard reads:
**both operand calls fire even for a constant condition**, and `c` means different things across them —
`cf` gets `op=144 c=0 first=1 vn=1` then `op=144 c=1 first=0 vn=1`, while dynamic `dyn` gets `c=-1` on both. So `c` is
not simply 'the constant value of operand 1', and the second call's `c=1` needs explaining before any modelling is
written. Implementing against a misread of that protocol is exactly how the `!!` inversion, the const-member
relaxation and the search floor each went wrong earlier in this file.
**PROTOCOL DECODED 2026-07-27 — and the fix is bigger than this guard.** `expr_landor` (`mccgen.c`) is:
```c
int i = op == TOK_LAND, c;              /* i = 1 for &&, 0 for || */
c = f ? i : condition_3way();           /* -1 = dynamic, else the constant */
if (c < 0) save_regs(1), cc = 0;
else if (c != i) nocode_wanted++, f = 1;   /* this operand short-circuits */
```
So `c` is NOT 'the constant value of operand 1' on later calls — once `f` latches, `c = i` for every subsequent
operand. That explains the traced `cf` sequence (`c=0 first=1` then `c=1 first=0`) and `ct` (`c=1` then `c=0`): the
second value is the latch, not a constant. The decisive test is `c >= 0 && c != i` on the FIRST call.
**The blocker is dead-code hook traffic, not the guard.** When an operand short-circuits, `nocode_wanted++` and the
remaining arm is still PARSED — so it keeps calling the recorder's hooks while emitting nothing. The recorder has
exactly ONE `nocode_wanted` check today (`mccast.c`, the call hook) and it DESYNCS rather than ignoring, so a dead arm
containing a call desyncs anyway and a dead arm without one pushes phantom nodes onto `ast_vs`. Modelling the landor
constant therefore requires the recorder to suppress hook EFFECTS under `nocode_wanted` generally — a cross-cutting
change to every modelling hook, not a local edit here.
**The obvious implementation of that was TRIED 2026-07-27 and makes things WORSE — do not repeat it.** Adding
`|| (ast_nocode_gate && nocode_wanted)` to the 8 shared hook guards
(`!ast_capture || ast_desync || ast_in_op || ast_in_call`) and measuring over mcc's own TU:

| gate | desync | faithful | stackresidue |
|---|---:|---:|---:|
| off | 642 | 905 | 0 |
| on | **732** (+90) | **837** (-68) | **5** (new) |

It is a net loss, and the new `stackresidue` verdicts say why: the hooks come in PAIRS (a push must be matched by its
pop, an op-begin by its op-end), and a flat entry-guard suppresses whichever side of a pair happens to run while
`nocode_wanted` is set, leaving the model unbalanced. Suppression has to be balanced — enter dead code once, ignore
everything until the matching exit, and restore the recorder's stack depth — not decided per hook call.
So the correct shape is a dead-code DEPTH tracked by the recorder (mirroring `nocode_wanted`'s own nesting), with the
hooks consulting it only where suppression keeps the push/pop invariant. That is a real design task, not a guard
edit. Reverted; the tree is unchanged.

**F5 — The vstack SYNC site is real modelling work, with no shortcut. Now the LARGEST remaining cause by a wide
margin.** Measured 100% the `ast_vn != rel - 1` arm, 0% capacity, so raising `AST_VS_MAX` recovers nothing.

**CHARACTERISED 2026-07-27, and the direction is the opposite of what "SYNC mismatch" suggests.** Instrumenting the
site (`ast_hook_vpush`, the FIRST of the two `ast_vn != rel - 1` arms — the other is at ~3178, so patch by index, not
by text) and dumping every event over mcc's own TU gives 261 events, one per desyncing function, and they are
strikingly uniform:

| measurement | result |
|---|---|
| `rel > AST_VS_MAX` (capacity) | **0 of 261** — re-confirms raising the cap is worthless |
| `delta = rel - 1 - ast_vn` | **-1 for ALL 261** |
| `vtop` class at the failure | 202 `VT_CONST\|VT_SYM` (156 rvalue, 46 lvalue), 44 `VT_LOCAL` lvalue, 15 plain `VT_CONST` |

`delta == -1` means `ast_vn == rel`, where the hook requires `ast_vn == rel - 1`. **So the MODEL has one node too
many, not one too few.** The recorder is not failing to model something that was pushed — something CONSUMED a vstack
entry without the corresponding `ast_vs` node being dropped, and the mismatch is only noticed at the next push. That
inverts the search: do not go looking for an unmodelled push, look for a pop/consume path that does not decrement
`ast_vn`.

The `vtop` column says where to look first: in 202 of 261 cases the value being pushed when the discrepancy surfaces
is a SYMBOL address (`VT_CONST|VT_SYM` — a global, a string literal, or a function being called), which is what makes
this the dominant site in a TU full of calls and globals. Note it is the NEXT push that reports, so the symbol is a
witness, not the culprit.

**Every event is a distinct function** (261 events, 261 functions), so this is one systematic gap rather than a few
functions failing repeatedly.

**RE-MEASURED 2026-07-27 after the five fidelity fixes — the signature is UNCHANGED and the stale node is now
IDENTIFIED.** The site is down to 64 events, but every one still has `delta = -1` (model one node too many) and
`cap = 0`, so nothing about the earlier diagnosis moved. Dumping the model stack at the failure shows the model holds
exactly ONE stale node in 61 of 64 cases (`vn=1 rel=1`), and its kind is:

| leftover node | count |
|---|---:|
| `AST_Convert` | **22** |
| `AST_Invoke` | 13 |
| `AST_Literal` | 12 |
| `AST_Binary` | 6 |
| `AST_Unary` | 5 |
| `AST_Load` | 2 |
| `AST_StoreVal` | 1 |

**FOUND AND FIXED 2026-07-27 — the `Convert` leftover is the function-scope STATIC WITH AN INITIALIZER, which unifies
this site with the second F5 reproducer recorded above.** Tracing which functions produce a `Convert` leftover named
`ast_slice_multi_on`, `switch_jt_env`, `mcc_stats_enable` and friends — all the env-gate idiom
`static int on = -1; if (on < 0) { … } return on;`. Confirmed directly: `int f(void){ static int s = 5; return s; }`
desyncs with `leftover-kind=10` (`AST_Convert`), while the same static WITHOUT an initializer is faithful, and so are
the `?:`-only and `&&`-only variants once the static is removed.

Cause: a static's initializer is compile-time data written into a section, so the parser emits no code — but the value
it walks goes through `gen_cast`, and `ast_hook_convert` puts a `Convert` on the model stack that nothing ever pops.
The model then runs one ahead and the next real push reports the mismatch. Fix is the same parser-side synth-suspend
that worked for `sizeof`: bracket `decl_initializer_alloc` in `decl()` when the storage is `VT_CONST` and there is an
initializer. **Autos are deliberately NOT bracketed** — their initializer emits real code that must be modelled, and
`int v = x + 1;` / `int t[3] = {1,2,3};` stay faithful either way.

Gain: **+16 faithful (1376 -> 1392), desync 287 -> 268**, self-host fixpoint byte-identical (5525167), host ctest
7276/7276, cross 7435/7435, `ast/treecheck` clean at `-O2`/`-O3`/`-Os`, 400-seed gate-swept fuzz 397 agree 0
miscompile, qemu 14/14 on all four triples, 0 exec-corpus crashes, both side-effect guards green. Ratchet 763 -> 759.

Superseded lead: **find the path that consumes a vstack entry holding a CONVERSION result without dropping its
`ast_vs` node** — that is a third of the site, and `ast_hook_convert` replaces the top of `ast_vs` with
a `Convert`, so a consumer that pops the vstack without a matching hook leaves exactly this. The `Invoke` (13) and
`Literal` (12) groups are the same bug seen through other producers. Context is mostly clean (`in_op=0 in_call=0
tern=0 lor=0` in 52 of 64), so this is not an in-flight-expression artifact; 12 events are inside a short-circuit
region (`lor=1`) and may share a cause with the landor work above.

**MINIMAL REPRODUCER for one path into this site, bisected 2026-07-27 out of `tests/exec/lexical/U32_string.c`
(15 lines, the smallest corpus file that hits it):**

    typedef __CHAR32_TYPE__ c32;
    extern int printf(const char *, ...);
    int main(void){ const c32 *p = U"xy"; int ok = p[0]==120 && p[1]==121; printf("%d",ok); return 0; }

That is `desync` (`ast_hook_vpush` SYNC). The discriminating axes, each verified by flipping exactly one thing:

| variant | verdict |
|---|---|
| `const c32 *p = U"xy"`, two derefs in `&&` | **`desync` (`ast_hook_vpush` SYNC)** |
| same with `c16`/`u"xy"` | **`desync` (`ast_hook_vpush` SYNC)** |
| same with three derefs | **`desync` (`ast_hook_vpush` SYNC)** |
| plain `const char *p = "xy"` | faithful |
| `const int *p = (const int *)"xyzw"` | faithful |
| `static const c32 q[]=U"xy"; p = q` (named global, not a literal) | faithful |
| local array `c32 s[]=U"ABC"` instead of a pointer | faithful |
| `||` instead of `&&` | faithful |
| two derefs NOT in a short-circuit (`p[0]+p[1]`) | faithful |

So it is not element size (the `int*` cast is 4-byte and faithful), and not symbol-vs-literal alone (the named wide
global is faithful). It is specifically **a pointer initialised from a WIDE string literal, dereferenced at least
twice inside a short-circuit `&&`**. That combination is what leaves the extra `ast_vs` node.

**Scope: this is ONE path into `ast_hook_vpush`’s vstack-SYNC arm, and it is now MEASURED to cover none of the TU's 261.** I fixed the shape
and re-counted, which is exactly what this note said to do — see below.

**CAUSE FOUND for the reproducer, and the naive fix is NET-NEGATIVE. Reverted; read this before retrying.**
A `gdb` breakpoint on the desync gives the culprit immediately:

    ast_hook_vpush  <-  vsetc  <-  vpush64  <-  vpushi(121)
                    <-  decl_initializer  (its string-element loop)
                    <-  decl_initializer_alloc  <-  unary()  (the `str_init` path)

A wide string literal in expression position is materialised as an ANONYMOUS object, and `decl_initializer` walks it
element by element as `vpushi(ch); init_putv(...)`. That pair is net-zero on the vstack, but the recorder models each
`vpushi` as an expression value and nothing drops the node — so the model runs one entry ahead and the NEXT real push
reports the mismatch. That is precisely the "consume without an `ast_vn` decrement" direction the delta measurement
predicted, and the same class as the `gaddrof`/`gen_cast` synth-suspend fixes.

Bracketing that pair with `ast_hook_synth_begin`/`_end` **does fix the reproducer** (`main` goes desync -> faithful)
and clears 3 of the exec corpus's 44 site-2302 desyncs. **But it also LOSES 4 faithful functions**, corpus faithful
2410 -> 2406, so it is a net regression and was reverted. The bracket is too broad: `decl_initializer` runs for every
initializer, including real declarations where the recorder legitimately models the initial values. **A retry must
narrow the suspend to the anonymous-object-in-expression-position case (the `str_init` path in `unary()`), not the
shared `decl_initializer` element loop.**

And the headline: **on mcc's own TU this changed nothing at all** — faithful stayed 1101, `ast_hook_vpush`’s vstack-SYNC arm stayed 261. mcc's
own source contains no wide string literals, so this path accounts for **0 of the 261** TU events.

**SECOND REPRODUCER, bisected FROM mcc's own TU 2026-07-27, and it is a ONE-LINER:**

    int f(void){ static int s = 5; return s; }        /* `desync` (`ast_hook_vpush` SYNC) */
    int f(void){ static int s;     s = 1; return s; } /* FAITHFUL   */

**A function-scope `static` WITH AN INITIALIZER desyncs; the same static without one does not, and a file-scope
static does not.** Verified across variants: initializer `0` / `5` / any value all desync, read-only use desyncs,
two statics desync, and the ubiquitous guard idiom `static int on = -1; if (on < 0) on = 1;` desyncs. Bisected down
from `mcc_stats_enable` (mccstats.c), one of the smallest desyncing functions in the TU.

That matters because **mcc's own source is saturated with this idiom** — every cached-env gate is
`static int on = -1;` — and two spot-checked examples (`ast_hook_cmp_invert`, `mcc_stats_enable`) are both in the
261. It is also the SAME family as the wide-string case above: a static's initializer goes through
`decl_initializer`, whose value pushes the recorder models as expression values.

`gdb` shows the witness differs from the culprit here, exactly as the delta analysis predicted: the reported push is
the innocent `return s;` (`unary()`’s value push -> `vset`), while the extra node was left behind earlier during
the declaration. Do not "fix" the site the backtrace names.

**Coverage MEASURED 2026-07-27 with a proper brace-matching locator (256 of 261 definitions found, vs 43 by the
earlier crude grep): the static-initializer shape is only 19 of them, ~7%. It is NOT the main cause.**

**THIRD AND DOMINANT REPRODUCER — a `void` function with a bare early `return;`. Two lines:**

    static int a, b;
    void f(void){ if (!a) return; if (b) b = 1; }      /* `desync` (`ast_hook_vpush` SYNC) */

Verified by flipping one thing at a time:

| variant | verdict |
|---|---|
| `void f(){ if(!a) return; if(b) b=1; }` | **`desync` (`ast_hook_vpush` SYNC)** |
| `void f(){ if(!a) return; if(b>0) b--; }` | **`desync` (`ast_hook_vpush` SYNC)** |
| same logic NESTED, no early return (`if(a){ if(b>0) b--; }`) | faithful |
| `int f(){ if(!a) return 0; if(b>0) b--; return 1; }` — returns a VALUE | faithful |
| `void f(){ if(!a) return; b=1; }` — no second `if` | bail (not desync) |
| one `if` alone, decrement alone, local instead of global | faithful |

So it needs: **void return type + a bare `return;` + at least one more `if` after it.** Neither the trace macros nor
`__func__` are involved — I tested both and they are faithful, so the `MCC_TRACE` in every mcc function is a red
herring.

**This is the shape that explains F5.** Of the 256 located functions, **218 (85%) are void-returning and contain a
bare `return;`** — against 19 with a static initializer. Site 2302 is 48% of all desyncs, so this single shape is on
the order of 40% of every desync in the TU, and it is the guard-clause idiom that `mccast.c` itself is built out of
(`void ast_hook_X(void) { if (!ast_active) return; … }`).

**MECHANISM RESOLVED 2026-07-27, and it REFRAMES F5 ENTIRELY: `ast_hook_vpush`’s vstack-SYNC arm is a SYMPTOM, not the bug. A bare
`return;` is simply not modelled, and makes the function unoptimizable however it is labelled.**

`ast_hook_return(has_val)` does, for `!has_val`, exactly this:

    if (!has_val || ast_ret_val == AST_NONE) { ast_bail = 1; return; }

— no `AST_Return` node is built at all. `gdb` at the desync confirms the rest: `ast_bail` is **already 1** when the
2302 check trips, `ast_vn == 1` against `rel == 1`, and the leftover node is the enclosing `if`'s condition, which the
control-flow hooks stop maintaining once bailed. So the sequence is: bare `return;` bails -> the `if` hooks stop
balancing `ast_vn` -> the next push notices and calls it a desync.

The label depends only on what follows the return, and BOTH outcomes are fatal:

| shape | verdict |
|---|---|
| `void f(){ if(!a) return; }` | bail |
| `void f(){ if(!a) return; b=1; }` | bail |
| `void f(){ if(!a) return; if(b) b=1; }` | **`desync` (`ast_hook_vpush` SYNC)** |
| `void f(){ b=1; return; }` — even a TRAILING bare return | bail |
| `void f(){ if(!a) b=2; if(b) b=1; }` — no return at all | **faithful** |
| `int f(){ if(!a) return 0; … return 2; }` — value return | **faithful** |

**So "fix the 2302 desync" is the wrong goal — relabelling it `bail` would gain nothing, since `faithful` gates every
consumer and both verdicts fail it.** The actual work is to MODEL a valueless return: build an `AST_Return` with no
value child and teach replay to emit the jump-to-epilogue for it, including its interaction with `ast_hook_return_jmp`
and the `ret_jumps` bookkeeping. That is a new node shape, not a bookkeeping repair, which is why nothing here has
dented it.

Payoff if done: 218 of the 256 located site-2302 functions are void-with-bare-`return;`, `ast_hook_vpush`’s vstack-SYNC arm is 48% of all
desyncs, and the 60 `bail` verdicts are largely this too — so this one construct plausibly gates ~40% of every
non-faithful function in mcc's own TU. It is the single highest-value item in the fidelity section by a wide margin.

**LANDED 2026-07-27 — bare `return;` is now MODELLED. faithful 1101 -> 1318 (+217), bail 60 -> 1.** The largest
single fidelity win in this file. Four small edits, no new node kind:
1. `ast_hook_return` builds an `AST_Return` with NO value child instead of setting `ast_bail`; only a return that HAD
   a value the recorder failed to model still bails.
2-4. The three passes that consumed a Return's value unguarded now tolerate the missing child, each keeping its
   state-kill: `ast_cprop_block` (nothing to rewrite, still clears the known-value set) and the two `ast_cse_subst`
   sites (nothing to substitute, still clears the available-expression set). Replay needed NO change — it already
   guarded `v != AST_NONE` and emitted only the epilogue jump.

Measured on mcc's own TU at `-O2`: **faithful 1101 -> 1318, desync 540 -> 356, bail 60 -> 1, unfaithful 149 -> 175**
(the +26 unfaithful are newly-modelled bodies that now replay and differ — they were previously bailed, so this is not
a regression: both verdicts are equally excluded). **The recorder-fidelity ceiling drops from 41% to 28.7%.**

Validated on the full bar: host ctest 7276/7276, cross 7435/7435, self-host fixpoint byte-identical (5516303),
`ast/treecheck` clean at `-O2`/`-O3`/`-Os`, 400-seed gate-swept differential fuzz 397 agree 0 miscompile, qemu 14/14
on all four triples, 0 compile crashes over the exec corpus, and both side-effect guards
(`assign_value_effects`, `side_effect_order`) green at `-O0/-O1/-O2/-O3/-Os` — the check that matters most here, since
this is exactly the change class that miscompiled silently earlier in the session.

**The audit that made it safe, kept because the method generalises:** only 3 of the 14 `AST_Return` sites were
unguarded. `case AST_Return` in replay, the second cprop site, `ast_inline`'s and the graft path all already tested
`AST_NONE` or `ast_nchild == 1`. Enumerating the sites first — rather than null-guarding reflexively — is what kept
this to four edits.

Superseded first attempt: **the recorder half is a ONE-LINE change and it WORKS; the blocker was that the PASSES
assume a Return always has a value child.**

**Replay already supports a valueless Return** (mccast.c `case AST_Return`): it guards `v != AST_NONE` around the
value emission and otherwise emits only the epilogue jump, in both the graft and normal paths. Nothing there needs
writing. The recorder change is just to stop bailing:

    if (has_val && ast_ret_val == AST_NONE) { ast_bail = 1; return; }   /* was: !has_val || ... */
    AstLocal ret = ast_node(ast_cur, AST_Return);
    if (has_val) ast_add_child(ast_cur, ret, ast_ret_val);

With that, every probe above flips to **faithful** — `if(!a) return;` alone, with a following statement, with a
following `if`, and a trailing bare `return;` — and the exec corpus builds with **0 crashes**, both side-effect guards
pass (`calls=14 calls2=2`, `effects ok`).

**Then mcc's own TU SEGFAULTS.** `ast_cprop_safe(a, n=0xffffffff)` -> `ast_type_t` on `AST_NONE`, from
`ast_cprop_block` (`ast_cprop_block`) doing `ast_cprop_safe(a, ast_first_child(a, s))` for `k == AST_Return` with no
child. So the passes, not the replay, are what assume the child exists. Note the exec corpus did NOT catch this —
only the full amalgamation did, which is worth knowing before trusting a green corpus run on any Return-shape change.

**Remaining work is a per-pass audit of the 14 `AST_Return` sites in mccast.c.** Unguarded child accesses seen at
the cprop and two CSE sites; the `ast_nchild(ca, n) == 1` guard already does it correctly (`ast_nchild(ca, n) == 1`). This is
NOT a mechanical null-guard: each pass needs a decision about what a valueless return MEANS to it (cprop must kill its
known-values set but has nothing to rewrite; DSE must treat it as a barrier; TCO/inline must not treat it as a
value-producing return). A wrong guard here is the dangerous class — it passes the byte check and then miscompiles
under `-O2` — so each site wants the side-effect-counting guards plus the fuzz, not just a fidelity count.

**Histogram re-measured 2026-07-27 after this session's changes** (three recorder gates flipped default-on, F3a
landed, `RELOC_EQUIV` flipped), because the old one predates all of them and its shares are no longer right. mcc's own
TU at `-O2`, 1849 functions: **1101 faithful / 540 desync / 149 unfaithful / 60 bail / 1 empty**. The desyncs by site:

| site | count | share of desyncs | what it is |
|---|---:|---:|---|
| `ast_hook_vpush`’s vstack-SYNC arm | **261** | **48%** | the F5 vstack SYNC arm (`ast_vn != rel - 1`) |
| `ast_hook_vpush`’s value-model guard | 106 | 20% | `ast_hook_vpush` value-model guard (not const/sym/local, or bad type) |
| `ast_hook_call_begin`’s `nocode_wanted` guard | 78 | 14% | `nocode_wanted` |
| `ast_hook_landor_operand`’s constant-first-operand guard | 39 | 7% | `&&`/`||` first operand (F4) |
| `ast_hook_cmp_invert`’s unmodelled-op arm | 20 | 4% | unmodelled op (`default:` arm) |

**RE-MEASURED after the bare-`return;` fix landed (2026-07-27). The table above is superseded — `ast_hook_vpush`’s vstack-SYNC arm collapsed
and the ranking changed.** Totals are now **1318 faithful / 356 desync / 175 unfaithful / 1 bail / 1 empty**:

| site | count | share | what it is |
|---|---:|---:|---|
| `ast_hook_vpush`’s value-model guard | **112** | **31%** | `ast_hook_vpush` value-model guard — now the leader |
| `ast_hook_call_begin`’s `nocode_wanted` guard | 87 | 24% | `nocode_wanted` |
| `ast_hook_vpush`’s vstack-SYNC arm | 48 | 13% | the old F5 site, **down from 261** |
| `ast_hook_landor_operand`’s constant-first-operand guard | 45 | 13% | `&&`/`||` first operand (F4) |
| `ast_hook_cmp_invert`’s unmodelled-op arm | 22 | 6% | unmodelled op (`default:` arm) |

**Site 2315 characterised the same way (instrument, dump every event, cross-tabulate). 112 events, TWO clean groups:**

| group | count | detail |
|---|---:|---|
| register-held **LVALUE** (`VT_VALMASK` is a hard register, `VT_LVAL` set, no sym) | **61 (54%)** | 51 in reg 0, 10 in reg 1 |
| **struct**-typed value that is not an aggregate lvalue | **51 (46%)** | 48 `VT_STRUCT`, 3 `VT_LDOUBLE` |

The first group is a dereference through a pointer already materialised in a register: the model only knows
`is_const` / `is_sym` / `is_local` / `is_llocal_lval`, and "lvalue whose address is live in a register" is none of
them. **Note that one is not straightforwardly modellable at all** — the register is a register-allocation artifact,
so a model that names it would not survive replay under a different allocation; it needs the POINTER's provenance
modelled instead. The second is the aggregate-rvalue case the `agg_lval` escape deliberately does not cover. Neither
is a bookkeeping slip — both need a new modellable value form, so this is genuinely the same class of work the
bare-return fix turned out to be, not a guard tweak.

**Site 3256 (`nocode_wanted`, 87 events, 24%) enumerated 2026-07-27 — it is DEAD-OR-UNEVALUATED CODE CONTAINING A
CALL, and it fans out across three different desync sites:**

| shape | verdict |
|---|---|
| `return (int)sizeof(h(1));` — unevaluated operand | **`desync` (`nocode_wanted`)** |
| `return 1; return h(2);` — dead after return | **`desync` (`nocode_wanted`)** |
| `if (0) h(1);` | **`desync` (`nocode_wanted`)** |
| `goto e; h(1); e: …` — dead after goto | **`desync` (`nocode_wanted`)** |
| `return 0 && h(1);` | `desync` (landor-constant) |
| `return 1 \|\| h(1);` | `desync` (landor-constant) |
| `return 1 ? a : h(1);` — dead ternary arm | `desync` (ternary-constant) |
| `return h(1);` — control | **faithful** |

Sites 3256 + 2589 + 2533 total 87 + 45 + 6 = **138, 39% of all desyncs**, and they share the THEME that the parser
knows the code is dead or the condition is constant while the recorder cannot model it. **They do NOT share a
mechanism, and treating them as one problem would be a mistake — I checked the conditions rather than inferring
them:**

- **3256** is `ast_hook_call_begin` testing the `nocode_wanted` FLAG. Fix shape: suspend recording across the whole
  no-code region.
  **REMOVING THE DESYNC OUTRIGHT was tried 2026-07-27. It reaches 74.9% faithful and FAILS THE SELF-HOST FIXPOINT.
  Do not retry the one-line removal.** The lead was solid: dead code containing NO call is already faithful —
  `if (0) b = 1;`, `return a; b = 1;` and `sizeof(a+b)` all pass — so the `nocode_wanted` guard looked gratuitous.
  Deleting it gives:

  | | result |
  |---|---|
  | mcc's own TU | faithful **1343 -> 1385**, desync 327 -> 261 |
  | `if (0) h(1);`, `return a; h(1);`, `1 ? a : h(1)` | desync -> **faithful** |
  | `sizeof(h(1))`, `0 && h(1)` | desync -> unfaithful (safe, still excluded) |
  | exec corpus | 0 crashes |
  | `assign_value_effects` / `side_effect_order` | green at `-O0/-O2/-O3/-Os` |
  | `ast/treecheck` | clean at `-O2/-O3/-Os` |
  | 400-seed gate-swept differential fuzz | **397 agree, 0 miscompile** |
  | **3-stage self-host fixpoint** | **FAIL — `o1=5523199` vs `o2=o3=5523119`** |

  `o1 != o2` means a compiler built BY the patched compiler emits different code than the patched compiler does:
  the recorder is admitting a body whose optimized form is not stable across a self-host generation. **Every other
  gate passed** — the fuzz, both side-effect guards, the tree checker and the whole exec corpus — so this is the
  clearest example in the file of why the fixpoint is not redundant with them. Intermediate step, also recorded so it
  is not re-derived: merely *skipping* the desync (returning without modelling) is worse still, moving the failure to
  the value-model guard, because the hook must MIRROR the call's stack effect (consume `nb_args + 1`, push a result)
  rather than do nothing.

  So the remaining constraint set for this item is now: mirror stack effects while recording nothing (a suspend that
  hides both is wrong, see below), and whatever is admitted must survive a self-host generation — which the plain
  Invoke node for a dead call does not.

  **RESOLVED 2026-07-27 by dumping the model AT the faithfulness check (before any pass runs) — and it overturns the
  `Poison` hypothesis below, which is retained only so the wrong lead is not re-followed.** The recorder records dead
  code as an ORDINARY constant-condition `If`, with the dead statement fully present inside it, and the body is
  `faithful`:

      [rec-model] f (faithful=1)
      BasicBlock
        If
          Literal 0
          BasicBlock
            Store
              Ref
              Literal 1
        Store …

  So nothing special happens in the recorder at all. Replay emits that `If`, and **codegen's own constant folding
  makes the parser and the replay both emit nothing for the branch** — that is why dead code without a call is
  faithful. The `Poison` seen in `MCC_AST_REPLAY_DUMP` is produced by SCCP AFTER the faithfulness check, and is
  irrelevant to this item.

  **Consequence: "have the call hook leave a Poison" is the WRONG plan.** For `if (0) h(1);` the recorder should just
  record the `Invoke` inside the constant `If` like any other statement — which is exactly what deleting the
  `nocode_wanted` guard does, and that case does go faithful. The cases that then become `unfaithful` (`sizeof(h(1))`,
  `0 && h(1)`) are the ones NOT wrapped in a constant `If`, so codegen has nothing to fold and replay emits a call the
  parser never emitted.

  **The `sizeof` half of that subset is now FIXED (2026-07-27), and the synth-suspend idiom DOES work here — because
  it is applied at the PARSER, not in the recorder.** `sizeof`/`_Alignof` has the clean paired boundary the general
  `nocode_wanted` problem lacks: `unary()` parses the operand with `expr_type(&type, unary)` and then pushes the size
  constant, so bracketing just that call with `ast_hook_synth_begin`/`_end` hides the unevaluated walk and leaves the
  constant modelled normally. That is the distinction from the recorder-side region suspend that failed earlier: here
  the operand's net vstack effect really IS zero, so there is nothing left behind to mirror.

  Gain: **+33 faithful (1343 -> 1376), desync 327 -> 287** — larger than the handful of `sizeof(call)` shapes
  suggests, because mcc's own source uses `sizeof` heavily. `sizeof(h(1))`, `sizeof(a+b)`, `sizeof(int)`,
  `_Alignof(int)` and `sizeof(a) + a` are all faithful, live-call control unchanged. Validated with the
  **self-host fixpoint byte-identical (5523423)** — the gate that killed the `nocode_wanted` removal — plus host
  ctest 7276/7276, cross 7435/7435, `ast/treecheck` clean at `-O2`/`-O3`/`-Os`, 400-seed gate-swept fuzz 397 agree
  0 miscompile, qemu 14/14 on all four triples, 0 exec-corpus crashes, both side-effect guards green. Ratchet
  765 -> 763.

  **What REMAINS is the short-circuit RHS (`0 && h(1)`)**, which has no equivalent parser-side boundary — the RHS is
  parsed inline by `expr_landor` rather than through a single bracketable call.

  Superseded framing: **that, not the `if (0)` case, is what needs a mechanism** — and it is also where the
  self-host fixpoint failure must be coming from, since the `if (0)` form is structurally identical to code that is
  already faithful today.

  Superseded lead, kept so it is not re-tried: **the likely mechanism to reuse is `AST_Poison`.** `MCC_AST_REPLAY_DUMP=1`
  on `void f(void){ if (0) b = 1; a = 2; }` shows the dead statement represented as a single `Poison` node:

      BasicBlock          (dead)              BasicBlock            (live `if (1)`)
        Poison                                  BasicBlock
        Store                                     BasicBlock
          Ref                                       Store …
          Literal 2                             Store …

  `ast_replay_bb` has NO `case AST_Poison`, so it falls through `default: break;` and emits nothing — which is
  exactly the "mirror the stack effect, emit nothing" behaviour a dead call needs. So the experiment is: have
  `ast_hook_call_begin` under `nocode_wanted` consume `nb_args + 1` operands and leave a `Poison` as the call's
  result, instead of desyncing or building an `Invoke`.

  **Open question, and my first two attempts to answer it were BOTH invalid — do not repeat them.** It is not yet
  known whether that `Poison` is produced by the RECORDER or by a later pass. The recorder region (mccast.c below
  the recorder hooks, above the optimizer passes) contains no `AST_Poison` at all, which suggests a pass, but neither control disabled the transformation:
  `MCC_AST_OPT_LIMIT=0` and `MCC_AST_SCCP=0` both still print `[ast-sccp] 1 f` and still show the `Poison`, and `-O0`
  produces no dump at all because replay does not run there. A valid isolation needs a build with the pass actually
  removed, or a dump taken at the faithfulness check rather than after optimization. This matters because if a PASS
  makes the Poison, then the recorded model still contains the dead statement and the faithfulness check is passing
  for some other reason — which would change the whole plan.

  **A TARGETED suspend was tried 2026-07-27 and does NOT work — recording it because the boundaries looked ideal.**
  After the constant-ternary fix landed, the recorder knows exactly where a folded ternary's untaken arm starts and
  ends (`_branch` / `_branch_done` for the non-taken index), which is precisely the paired enter/leave the general
  `nocode_wanted` problem lacks. Suspending recording across that arm (`ast_in_op++` / `--`) nevertheless REGRESSES
  the already-working case: `1 ? a : b` goes faithful -> desync. **Reason, and it generalises to any suspend-based
  attempt: the untaken arm's VALUE is real even though its CODE is not.** The parser still pushes a vstack entry for
  that arm — the landed fix accounts for it with an `ast_vn--` — so suppressing the recorder leaves the model one
  BEHIND the vstack instead of one ahead. A no-code region cannot be made invisible to the model; the model has to
  keep mirroring the stack effects while recording no nodes. That is a different mechanism from
  `ast_hook_synth_begin`/`_end`, which suppresses both. Note the Tests/infra section records that a FLAT `nocode_wanted` gate was tried and made fidelity
  WORSE (desync +90, faithful -68, 5 new `stackresidue`) because the hooks come in pairs; a working version needs
  paired enter/leave in the shape of `ast_hook_synth_begin`/`_end`, not a per-hook test. Hooking the transitions is
  harder than it looks — `nocode_wanted` is mutated at ~30 sites in mccgen.c in four different shapes
  (`++`, `--`, save/restore, and a bare `= 0` that breaks nesting outright).
- **2589** (`ast_hook_landor_operand`) and **2533** (`ast_hook_ternary_begin`) test `c >= 0`, i.e. the CONDITION
  folded to a compile-time constant, which is a different predicate entirely. That matches F4's own measurement
  ("32 events, ALL `c >= 0`"). Fix shape here is to MODEL a constant-condition `&&`/`||`/`?:` — record the arm that
  is actually taken and drop the other — which is a modelling addition, not a suspend.

  **Design input measured 2026-07-27, and it is the fact that makes this tractable: the hook sequence is IDENTICAL
  for a constant and a non-constant condition.** Instrumenting all four ternary hooks:

      x ? 1 : 2      begin c=-1 -> branch 0 -> done 0 -> branch 1 -> done 1 -> end     FAITHFUL
      1 ? a : h(1)   begin c=1  -> branch 0 -> done 0 -> branch 1 -> done 1 -> end     desync at begin

  Both arms' `branch`/`done` hooks fire either way, so the recorder is NOT missing callbacks in the constant case —
  it simply refuses at `begin`. A pass-through can therefore be built inside the existing state machine (remember
  "constant, taken arm = `c ? 0 : 1`" at `begin`, let the taken arm's `done` supply the result value, discard the
  other) without inventing new hook points.

  **The open `ast_vn` question was then ANSWERED by an observation build (let `begin` proceed on a constant condition
  purely to trace, then revert). Three results, and they change the plan:**

  1. **The sequence is BALANCED.** With a call-free untaken arm, `1 ? a : b` runs
     `begin(vn=1) -> branch0(0) -> done0(1) -> branch1(0) -> done1(1) -> end(vn=0)` with `desync=0` throughout, and
     `ast_vn` returns to 0. Both arms DO push a value. So the state machine needs no new hook points and no
     rebalancing — exactly the enabling fact hoped for.
  2. **Removing the desync alone is NOT enough: the verdict becomes `unfaithful`.** The recorder still builds an
     `AST_If` with both arms while the parser emitted only the taken one, so replay diverges. The fix must therefore
     SUPPRESS the If node and pass the taken arm's value straight through — confirming the shape above rather than
     just removing the guard.
  **IMPLEMENTED AND LANDED 2026-07-27 for the ternary half.** `ast_hook_ternary_begin` no longer desyncs on a
  constant condition: it drops the condition node, records which arm survives, and builds NO `AST_If`.
  `ast_hook_ternary_branch_done` mirrors the parser's decrement for BOTH arms and stashes the taken arm's node;
  `ast_hook_ternary_end` re-pushes that node as the result. All the constant shapes go faithful —
  `1 ? a : b`, `0 ? a : b`, and nested `(1 ? a : b) + (0 ? b : a)` — with the non-constant `c ? a : b` unchanged.

  **The stash/re-push is required and the obvious "retain in place" is WRONG — worth recording, because it fails
  asymmetrically and would look like a working fix.** Tracing `vn`/`rel` through both polarities:

      1 ? a : b   done0 vn=1 rel=1 | branch1 vn=1 rel=0  <-- parser POPPED, model did not
      0 ? a : b   done0 vn=1 rel=1 | branch1 vn=0 rel=0      (fine by accident)

  Both arms occupy the SAME vstack slot: the parser pops it between them and re-pushes. So retaining the taken arm's
  value in place only works when the taken arm is the LAST one (`c == 0`); for `c != 0` it desyncs at the next push.
  Mirroring the decrement for both arms and re-pushing the stashed node at `_end` works for both.

  Gain is **+5 faithful (1318 -> 1323), desync 356 -> 351** — small, and exactly as predicted by point 3 below: most
  constant conditions in this TU have a CALL in the untaken arm and hit `nocode_wanted` before the ternary hooks
  matter. The remaining value here is gated on the `nocode_wanted` work, not on more ternary modelling.

  **The `&&`/`||` half MEASURED 2026-07-27, and it is a SIMPLER shape than the ternary — all 45 events are one case.**
  Instrumenting both `c >= 0` arms of `ast_hook_landor_operand` over mcc's own TU:

  | | count |
  |---|---:|
  | `first` operand constant | **45 of 45** |
  | a LATER operand constant | **0** |
  | `c == 0` with `op == TOK_LAND` (`0 && X`) | 26 |
  | `c == 1` with `op == TOK_LOR` (`1 \|\| X`) | 19 |

  So every single case is the SHORT-CIRCUIT-TO-CONSTANT form: the first operand already decides the result, the RHS is
  not evaluated at all, and the whole expression folds to the literal `0` (for `&&`) or `1` (for `||`). No case needs
  the general "constant at operand N" machinery, and none needs the taken-arm stash the ternary required — the result
  is a Literal, not an operand's node. That makes this the easiest remaining modelling item in the section.

  **IMPLEMENTED AND LANDED 2026-07-27.** The observation build answered the open questions: only ONE
  `ast_hook_landor_operand` fires (none for the skipped RHS), `landor_end` DOES arrive with `materialized=1`, and
  `vn`/`rel` are 1/1 throughout. `ast_hook_landor_operand` now recognises a constant first operand, records the level
  as folded, and builds no `AST_Binary`; `ast_hook_landor_end` pops the level without pushing.

  **The non-obvious part, found only by backtracing the first failed attempt: the model must DROP the constant's own
  node.** Keeping it as the result desyncs immediately, because the parser DISCARDS the first operand's vstack slot
  and pushes the folded constant in its place (`expr_landor` -> `vset`, `expr_landor`). Dropping it lets that
  replacement push be modelled as an ordinary `Literal`, which is both simpler and what actually happens. So the two
  constant-condition fixes need OPPOSITE handling — the ternary stashes and re-pushes the taken arm's node, the
  landor drops its node and lets the parser's replacement be re-modelled. Do not generalise one to the other.

  Gain: **+20 faithful (1323 -> 1343), desync 351 -> 327**. `(0 && a) + (1 || a)` and the nested/chained non-constant
  forms are all faithful. `0 && h(1)` still desyncs — at the `nocode_wanted` site (`ast_hook_call_begin`), confirming
  the ordering recorded above: the call-containing subset is gated on that item, not on this one.
  Validated: host ctest 7276/7276, cross 7435/7435, fixpoint byte-identical (5522511), `ast/treecheck` clean at
  `-O2`/`-O3`/`-Os`, 400-seed gate-swept fuzz 397 agree 0 miscompile, qemu 14/14 on all four triples, 0 exec-corpus
  crashes, both side-effect guards green. Ratchet 773 -> 765 gaps, regenerated.
  Validated: host ctest 7276/7276, cross 7435/7435, fixpoint byte-identical (5519967), `ast/treecheck` clean at
  `-O2`/`-O3`/`-Os`, 400-seed gate-swept fuzz 397 agree 0 miscompile, qemu 14/14 on all four triples, 0 exec-corpus
  crashes, both side-effect guards green.

  3. **If the untaken arm contains a CALL, `nocode_wanted` desyncs first** — `1 ? a : h(1)` shows `desync=1` already
     set by the time arm 1's `done` fires, at `ast_hook_call_begin`’s `nocode_wanted` guard (`ast_hook_call_begin`). **So the two items are ordered for
     that subset**: a constant ternary whose untaken arm is simple can be fixed by the pass-through alone, but one
     containing a call needs the `nocode_wanted` suspend as a prerequisite. That is a conditional dependency, not an
     absolute one — do not sequence the whole 51 events behind the 87.

So this is two pieces of work, roughly 87 and 51 events, not one of 138.

So F5 alone is now **48% of all desyncs and 14% of every function in the TU** — it was described as "the
second-largest cause" against an older mix, and it is now comfortably first. Anyone picking up fidelity work should
start here rather than at F4 (7%) or the `nocode_wanted` site (14%), and note that a flat `nocode_wanted` gate was
already tried and made fidelity WORSE (see the Tests/infra section) — the hooks come in pairs.

**F6 — riscv64's 30x `unfaithful` rate: ROOT-CAUSED AND FIXED 2026-07-27 by flipping `MCC_AST_RELOC_EQUIV`
default-on.** The mitigation already existed in-tree and was default-OFF; this entry's claim that F6 was "the only one
with no landed mitigation" was wrong.

**The recorded hypothesis was also wrong, and the correction is the useful part.** It blamed the `AUIPC`+`JALR`
(`R_RISCV_CALL`/`R_RISCV_RELAX`) pair emitted for an *external call*, inferred from the fact that 72 of 92 were `main`.
Reduction disproves it directly: `int h(int); void foo(int a){ h(a); }` — an external call — is **faithful**, while
`const char *p; void foo(void){ p = "x"; }` — no call at all — is **unfaithful**. The real trigger is **any reference
to a symbol's address**: string literal, static array, or global variable. `main` correlated only because `main`
usually contains a `printf` string literal. **Lesson: 72-of-92-are-`main` was a correlation that named the wrong
cause; the one-line reduction that separates "call" from "symbol reference" settles it in a minute.**

The divergence is genuinely not a code difference — `MCC_AST_VERIFY_DIFF` reports `baseline 20 B, replay 20 B (code
identical)` with the same bytes — it is that replay re-emits the same local symbol at a **different symbol index**.
`MCC_AST_RELOC_EQUIV` judges the relocation range structurally (equal offset, type, addend; symbols compared through
`ast_reloc_sym_equiv`) instead of by raw `memcmp`. That comparison is sound rather than merely lenient: it requires
BOTH symbols to be `STB_LOCAL` with identical `st_info`/`st_other`/`st_shndx`/`st_value`/`st_size` **and an identical
name**, so it tolerates a re-indexed identical local symbol and nothing else.

Effect on the exec corpus at `-O2`, and the profile is what makes the flip safe: **riscv64 192 faithful/105 unfaithful
-> 294/3**, which is exactly arm64's own profile (294/3). **Provably a no-op on every other target** — x86_64
2385/39, arm64 294/3, i386 295/7, byte-for-byte identical with the gate on and off, and the self-host fixpoint size is
unchanged at 5489695 B. So it buys ~102 additional riscv64 functions for the AST optimizer and changes nothing
elsewhere. `optfire` riscv64 rises **31/46 -> 36/46**: the loop transforms (`interchange`/`fusion`/`tile`),
`math_inline` and `argfwd` all start firing.

Validated: the gate is demonstrably live on riscv64 (**30 of 63 objects change** with it on, so the runs below are not
vacuous); **224 exec programs built with `mcc-riscv64` + the musl stage3 sysroot and run under `qemu-riscv64` produce
byte-identical output gate-on vs gate-off, 0 regressions**, 214 matching host gcc (the 10 that do not, do not under
either setting); host ctest 7229/7229; cross ctest 7343/7343; self-host fixpoint byte-identical; and all four qemu
gates (`riscv64`/`arm64`/`arm`/`i386`) 10/10.

**Residual riscv64 non-firing after the flip was 10 cases; all 10 are now RESOLVED 2026-07-27, and measuring each
before guarding it was the right call — 5 of the 10 turned out NOT to be gaps.**
- *`bfold_sqrt`/`bfold_sign`/`bfold_round`/`bfold_minmax` (4) — the reassoc situation exactly.* The `bfold` counter
  goes 0 -> 4/4/5/4 on riscv64, **identical to x86_64**; only the object-level effect is invisible. Moved to
  `cdelta.txt`, where they pass on every target. Had these been arch-guarded on the object diff alone, the suite would
  now be asserting that builtin folding does not happen on riscv64, which is false.
- *`promote` (1) — not a gap either.* riscv64's caller-saved promotion pool is deliberately empty
  (`AST_PROMO_CALLER_N 0`; pinning a0-a7 makes `get_reg` return -1 and `freg()` assert — the comment at the riscv64 promotion-pool block
  documents the crash), but it HAS an 11-register callee-saved pool reached through `MCC_AST_PROMO_LEAF_CALLEE`,
  which is default-on at `-O4` anyway. Adding that to the case's extra-env field makes it fire on riscv64 and changes
  nothing on x86_64/arm64, which fire either way.
- *`promo_arrow`/`promo_incdec`/`spill_share`/`color`/`opassign` (5) — genuinely undemonstrable there, guarded.*
  These need a CALLER-saved pool to produce an object-level effect, and riscv64's is empty by design. `arch.txt`
  narrowed from `x86_64,arm64,riscv64` to `x86_64,arm64`, with the file recording that this guards the CASE, not the
  gate — the same treatment `opassign` already had for i386.

**Method rule this section keeps re-proving, now three times (reassoc, bfold, promote): an object diff conflates "the
pass did not fire" with "the pass fired and the bytes happened not to change". Measure the `--stats` counter first;
reach for an arch guard only after the counter also reads zero.**

**F7 — `optfire` cross-triple: arm64 WIRED 2026-07-27; i386 and riscv64 need triage.** Measured every differ case
through each cross compiler with `OPTFIRE_NORUN=1`:

| target | cells | status |
|---|---:|---|
| **arm64** | **50** | wired, all passing — `regdisp` is arch-guarded to x86_64 |
| **i386** | **44** | wired, all passing — the 6 promotion/`opassign` cases are arch-guarded |
| **riscv64** | **45** | wired, all passing — 5 caller-saved-pool cases arch-guarded |

**All three cross triples are now WIRED: 140 cross cells, 100% passing** (`optfire-arm64` 50, `optfire-i386` 44,
`optfire-riscv64` 45), alongside 106 host cells. Registration is one loop over `arm64;i386;riscv64` handling both
`differ` and `cdelta` manifests.

arm64 is therefore wired: 50 `optfire-arm64/<case>` cells, all passing. They register only when the `mcc-arm64` target
exists, so the default `MCC_ENABLE_CROSS=OFF` build is untouched (7229/7229 unchanged) and they appear in a
cross-enabled configure. `arch.txt` is honoured against the CROSS target's cpu rather than the host's, which is what
keeps `regdisp` out.
**i386 TRIAGED 2026-07-27; still not wired, and the split is the point:**
- *Promotion family (6) — BY CONSTRUCTION, guarded.* The entire promotion machinery is inside
  `#if MCC_CONFIG_OPTIMIZER && (X86_64 || ARM64 || RISCV64)`, so i386 has NO promotion code and these cannot fire
  there. `promote`/`promo_arrow`/`promo_incdec`/`spill_share`/`color` are now `arch.txt`-guarded to those three CPUs,
  matching the `#if` exactly. Note riscv64 IS compiled in but has no leaf (caller-saved) pool by design, so some of
  these still will not fire there — a CASE-shape issue for when riscv64 is wired, not grounds to drop it from the list.
- *Reassoc family (4) — RESOLVED 2026-07-27 by a new `cdelta` optfire mode, not by reworking the cases.* The pass
  fires identically on both targets (`reassoc` counter 8/8, 3/3, 3/3, 4/4 x86_64 vs i386); only the object-level
  effect is invisible on i386. So the defect was in the MEASUREMENT, not the cases: an object diff asks "did the bytes
  change", which is a proxy for "did the pass fire" that this target breaks. **`cdelta` asks the real question** — it
  compiles with the gate forced to 0 and to 1 and requires the named `--stats` counter to be 0 and then nonzero. That
  is strictly stronger than differ mode (a byte change could come from anywhere; a counter delta is attributable to
  the pass) and it is portable, because the counter does not depend on whether the transform survives to the emitted
  bytes on a given target. All four sub-knobs moved from `differs_sub.txt` to `cdelta.txt` and now PASS on i386
  (`reassoc 0 -> 8/3/3/4`), which is where differ mode could not reach them. Built test-first per the TDD request:
  the manifest and CMake wiring went in first and the cells failed `unknown mode: cdelta`, then the mode was
  implemented. Negative control confirms it bites — forcing `MCC_AST_REASSOC=0` globally makes the cell FAIL with
  `pass DID NOT FIRE`. **Generalisable: prefer `cdelta` to `differ` for any gate whose transform is real but whose
  byte-level effect is target-dependent.** An arch guard would have been the wrong fix here — it would have recorded
  the pass as absent on i386 when it demonstrably fires.
- *Narrow family (6) incl. `vlat` — was a REAL gap; ROOT-CAUSED AND FIXED 2026-07-27.* It was two independent
  problems wearing one symptom, and the recorded diagnosis had them merged:
  1. **The cases were written with `long`, which is 64-bit on x86_64 but 32-bit on i386**, so there was no 64->32
     narrowing to find there *at all*. Those functions were **fully faithful** on i386 — no desync — the pass simply
     had nothing to do. Fixed by rewriting `narrow_fix`/`narrow_class0..3` in `long long`, which is 64-bit on every
     target mcc supports, so the same source poses the same question everywhere. **`vlat` already used `long long`**,
     which is why it was the one member of the family that failed for the *other* reason.
  2. **A genuine compiler defect, found by backtracing the desync rather than reasoning about it.** Every 64->32
     narrowing on a 32-bit target desynced the whole function, so the AST optimizer silently abandoned it. Cause:
     `gen_cast` (mccgen.c, `#if MCC_PTR_SIZE == 4`) splits a 64-bit value into 32-bit halves via `lexpand`/`lbuild`,
     and `lexpand`'s `vdup` ran through `ast_hook_vpush`, which saw a bare VT_LLONG register value it cannot model
     (`r=0 tt=0x4 in_op=0`) and set desync. This is the same class as the `gaddrof` materialisation bug and takes the
     same fix: bracket the halves-splitting with `ast_hook_synth_begin`/`_end` so the recorder does not see it. **It
     is safe precisely because the cast IS already modelled** — `ast_hook_convert` records an `AST_Convert` node at
     `gen_cast` entry, so replay re-emits the conversion; the `lexpand` is emission detail, not semantics.
  Effect: on i386 `vlat` goes from 3 desync + 1 faithful to **4 faithful**, and `narrow` 0 -> 3, matching x86_64
  exactly. **Method note worth keeping: the desync line number alone was misleading.** `ast_hook_vpush` bails early on
  `ast_in_op`, so the site "could not" fire inside an op — and it did not; it fired from `gen_cast`, which is not an
  op. A one-line instrumented print plus a `gdb` breakpoint gave the caller chain in minutes
  (`gen_cast -> lexpand -> vdup -> vpushv -> ast_hook_vpush`); reading the code around the reported line did not.
- *`opassign` — TRIAGED 2026-07-27: promotion-MEDIATED, guarded.* Its consumers sit OUTSIDE the promotion `#if`, so
  the gate itself is not arch-scoped — but the case's observable effect is. Measured on x86_64: with
  `MCC_AST_PROMOTE=0` forced, toggling `MCC_AST_OPASSIGN` produces a **byte-identical object**, while with promotion
  on it differs. `opassign` works by flipping functions from desynced to faithful so promotion can act on them, so on
  i386 — no promotion machinery at all — the case cannot demonstrate the gate however correct the gate is. Guarded to
  `x86_64,arm64,riscv64`, and the `arch.txt` comment records that this guards the CASE, not the gate.
**i386 is now WIRED: 45 `optfire-i386/*` cells, all passing** (was 39/45 when first wired, which is how the two
narrow problems above were isolated — the cells were added FIRST and left red, then diagnosed). The cross-target
registration is now a loop over `arm64;i386` handling both `differ` and `cdelta` manifests, so adding riscv64 is a
one-word change once its non-firing set is triaged.

**Validation for the `gen_cast` fix (it changes real 32-bit codegen, so object self-identity is not sufficient):**
x86_64 provably unaffected — **0 of 750 objects differ** across the exec corpus at -O0/-O2/-O3 between a pre-fix and
post-fix compiler (arm64/riscv64 are unaffected by construction, the change is inside `#if MCC_PTR_SIZE == 4`).
The 32-bit targets DO change, and narrowly: **i386 3 of 244 objects, arm 2 of 239**. Correctness on those: a native
i386 differential against `gcc -m32` over **227 runnable exec programs at -O0 and -O2 — 0 regressions**
(i386 ELF runs directly on x86_64, so this needs neither qemu nor docker, which matters because docker is not
available here). Plus `qemu-arm` 10/10, `qemu-i386` 10/10, host ctest 7229/7229, cross ctest 7343/7343, and the
3-stage self-host fixpoint byte-identical (o1==o2==o3, 5489695 B).

**Process lesson, and it is a REPEAT of one already recorded in this file — I nearly published the wrong conclusion
again.** The first x86_64 byte-identity check reported "DIFFERS at -O0/-O2/-O3". It was comparing objects that were
never written: the self-compile was missing `-B`, failed with `include file 'stdarg.h' not found`, and `cmp` on two
nonexistent files reports "differ". The measurement above only became meaningful after asserting `-s` on both outputs
first. **Check the compile's exit status (or the file's existence) before reading a counter or comparing objects** —
this is the third time that exact trap has produced a false finding here.
riscv64 is untriaged beyond the promotion note.

**F8 — Re-earn the gate-swept fuzz coverage.** The first VALID gate-swept soak ran 2026-07-27 (600 seeds, ~31k
configurations, 0 miscompiles). Everything before it swept nothing, because `GATES[g].env` was passed into a parameter
the reference branch of `build_run` never read. So: (a) the ungate campaign's soak evidence has to be rebuilt on top of
this baseline, not appended to it; (b) the same soak still needs running on arm64-native; (c) seed count should scale
up — a 3000-seed run was started and stopped as lower-value than the queued work, not because of any failure.

**F9 — `optfire/default-argfwd` is a deliberate TRIPWIRE.** It asserts `MCC_AST_ARGFWD` is default-OFF, which is
currently true only because the pass is structurally unreachable (`do_inline`'s `!ast_inline_pass_env` guard, with
`INLINE_PASS` default-on from `-O2`). If that guard is ever fixed, the cell FAILS — that is intended, and the fix is to
flip the expectation to `on`, not to delete the cell.

**F10 — Extend the self-host fixpoint beyond one axis.** `--opt=<level>` landed and `-O3`/`-Os` cells now pass
byte-identically, closing the hole where `-O2` never exercised the `-O3` defaults (`CYCLE`, `OPASSIGN`, `CHAINSTORE`,
`INLINE`) or the `-Os` ones (`PROMO_ARROW`, `PROMO_INCDEC`, `REGDISP`).

**Re-confirmed 2026-07-27 after the F1 flips, the `PROMOTE` floor and the slice cap — all four levels still hold
byte-identically:** `-O1` 6166623, `-O2` 5497999, `-O3` 5528255, `-Os` 5486367 (each `o1 == o2 == o3`).

**The `MCC_JIT=1` axis named here is VACUOUS — do not implement it.** `MCC_JIT` is not a compile-time switch: it is
read by the boot ctor embedded into a JIT-enabled OUTPUT binary (the embedded JIT boot ctor), i.e. at runtime of the produced
program, and only when that program was built with `--embed-jit`. Verified directly: compiling the same source with
`MCC_JIT=0` and `MCC_JIT=1` produces **byte-identical output** (4327 B both), and the self-host fixpoint under
`MCC_JIT=1` reports the identical 5497999 as without it. So running the fixpoint under that env asserts nothing new.
The meaningful JIT axis already exists separately in the M8 bar as the runtime equivalence `MCC_JIT=1` == `MCC_JIT=0`
on a JIT-embedded binary — which is a different test and is not a fixpoint concern.
Still genuinely not covered: level x gate combinations.

## Infrastructure parity, CI economics, and a looser slice cache (user-prioritized 2026-07-27)
Sits ABOVE the non-P0 campaigns: items 1-3 are CI/build hygiene that every later change rides on, and they can proceed in parallel with P0. Items 4-5 are feature work.

1. **Bring CI jobs, CMake targets and matrix runners to parity.**
   - **(a) Naming conventions.** The same concept is spelled differently at each layer — cmake target `mcc_static` vs output name `mcc-static`, cross archives `<arch>-libmccrt.a` vs the plain `libmccrt.a`, workflow job ids in `matrix.yml` vs `ci.yml`, and ctest cell names (`selfhost-arm64-native`, `qemu-arm64-glibc-exec`, `exec-gatesoff/*`) that follow three different schemes. Pick ONE scheme (suggest `<area>-<triple>-<variant>`) and apply it across workflows, targets and tests, so a failure name tells you the cell without a lookup.
   - **(b) PP / CMake public-private config split.** `mcc_config_node()` nodes and bare `option()` calls disagree about what is ADVANCED, what carries a GROUP, and which become `-D` on the compiler TU. `tools/ckconfig.c` already detects `MCC_CONFIG_*` drift between CMake and code — it has DRIFT(a) "code reads it, CMake never mentions it" and DRIFT(b) "CMake emits -D, code never reads it (dead)". **DONE 2026-07-27 — the rule was chosen and the check landed as DRIFT(c).**
Picked option (iii), an allowlist of legitimately-derived defines, because it matches the file's existing
`ALLOW_EXTERN`/`ALLOW_DEAD` shape and needed no renaming of the 56 existing nodes. The rule is two-part: a define
counts as declared if an `mcc_config_node()` names it EXACTLY, or if the node is named `MCC_X` and the emitted define
is `MCC_CONFIG_X` (the documented derived form — `MCC_CPUVER` -> `-DMCC_CONFIG_CPUVER`). Measured before writing it,
which is what made the allowlist small: of 26 emitted defines, 14 match a node exactly and 5 match the derived form,
leaving **7** to justify by hand — `MCC_CONFIG_MUSL`/`_UCLIBC` (fanned out from the `MCC_CONFIG_LIBC` node),
`_MCCDIR`/`_CROSSPREFIX` (derived from prefix/libdir), `_DWARF_VERSION` (from the `MCC_CONFIG_DWARF` node's value),
and `_MACHO_CHAINED_FIXUPS`/`_RUN_DUALMAP` (platform-pinned constants, not knobs). Each carries its reason in the
table, so an undeclared knob is now a reviewed entry rather than an oversight.

Enforced by the existing `config-drift-invariant` cell. **Mutation-tested both ways**: dropping `MCC_CONFIG_MUSL` from
the allowlist reports `DRIFT(c)`, and adding a bogus `-DMCC_CONFIG_BOGUS_KNOB` to CMake reports it too (alongside the
pre-existing `DRIFT(b)` for being unread). The entry below is kept because its reasoning is what selected the rule.

Superseded assessment: **Blocked on a decision, not on code (assessed 2026-07-27).** The obvious third check — "every knob that becomes a `-D` must be declared via `mcc_config_node()`" — cannot be written mechanically today because node name and define name are not the same namespace: there are **56 `mcc_config_node()` declarations but only 18 carry the `MCC_CONFIG_` prefix**, and several defines are DERIVED from a differently-named node (`MCC_CPUVER` the node becomes `-DMCC_CONFIG_CPUVER`; `MCC_CONFIG_LIBC` fans out to `MCC_CONFIG_MUSL`/`_UCLIBC`). A name-equality check would false-positive on every derived define, and a CI gate that cries wolf is worse than no gate. So this needs the intended rule stated first — pick one: (i) node name and define name must match exactly, and derived defines get their own nodes; (ii) an explicit declared mapping (node → emitted defines) that `ckconfig` reads; or (iii) an allowlist of legitimately-derived defines, in the style of the existing `ALLOW_EXTERN`/`ALLOW_DEAD` tables. Once the rule exists the check itself is ~30 lines in the file's existing shape.
   - **(c) Optimization levels 0-4 in the matrix.** *(exec corpus half DONE 2026-07-27: it now runs at `-O0` (runner default), `-O1`, `-O2`, `-O3`, `-Os` and `-O4` — `exec-O3/*` and `exec-Os/*` added, 299 cells each, 299/299 green in <9 s apiece. Cross-arch half STARTED: `mccharness qemurun` had NO `-O` flag at all, so every qemu triple's conformance suite only ever ran at `-O0` — the optimizer was untested through that path. Added `--opt` (default empty ⇒ unchanged) and a `qemu-<arch>-<libc>-O2` cell per triple. **Remaining question ANSWERED 2026-07-27: `-O2`, `-O3` and `-Os` cells per triple are all LANDED and all
meaningful.** 14 cells per preset, 100% passing on i386/arm/arm64/riscv64.

**RETRACTION — an earlier revision of this entry claimed `-O3` was byte-identical to `-O2` and `-Os` byte-identical to
`-O0` on this corpus ("0 of 30"), concluded the cells were worthless, and reverted them. That measurement was WRONG.**
Re-measured by direct per-file `cmp` instead of the `join`-based comparison used the first time, on the SAME binaries:
on i386, 17 of 34 artifacts differ between `-Os` and `-O0` and 6 differ between `-O3` and `-O2`; on riscv64, 20 of 36
and 4. Only 4 and 2 of those respectively come from the new files below — so **13 pre-existing artifacts already
differed at `-Os` and 4 at `-O3`**, and the cells were never vacuous. The differing pre-existing programs are named
(i386 `-Os`: control, floats_libc, integers, libc, tls, varargs_fp, vla; `-O3`: integers, vla; riscv64 `-O3`: vla), so
this is checkable rather than a bare count. **Method lesson: `join` silently produces a wrong answer if either input
is not sorted in its collating order — for a "did these bytes change" question, loop and `cmp` instead.** The single
spot-check that seeded the wrong conclusion (`libc_struct.pic` identical at `-Os`) was itself correct; one file was
just not representative.

**Corpus widened anyway, and it was worth doing on its own merits.** `tests/qemu/conformance` was 16 files / 708 lines
of ABI-and-libc shapes (aggregates, varargs, tls, complex, vla) with little that `-O3`/`-Os` act on. Added
`opt_loops.c` (LICM, IVSR, nested loops for interchange/tiling, fusion, plus zero-trip cases) and `opt_structs.c`
(`->` chains for REGDISP/PROMO_ARROW, compound assign and inc/dec through `->` for OPASSIGN/PROMO_INCDEC, store chains
for CHAINSTORE, plus copy-isolation and zero-trip write-back checks). Effect: `-Os` divergence 13 -> 17 artifacts and
`-O3` 4 -> 6 on i386. **Both files self-validate** — every result is computed once in an optimizable form and once
through `volatile`, so there are no hardcoded expected values to rot, and integer-only so i386's x87 excess precision
under `-fexcess-precision=fast` cannot cause a spurious failure. Verified rc=0 under gcc at `-O0/-O1/-O2/-O3/-Os` and
under mcc at those plus `-O4` and `-fPIC -pie`.
`-O1` is still skipped as a subset of `-O2`, and `-O4` still needs its own budget (out-of-process superopt, ~20 s per
compile).)* Cells today run at a single default level, so `-O0`/`-O3`/`-O4` codegen is barely covered per triple. Add the level as a matrix axis. Note `-O4` runs the out-of-process superopt (~20 s per compile measured), so it needs its own budget rather than being folded into the default cells.

2. **From-scratch CTest review — maximise tests per triple, and split bundled tests.** Only three ctest labels exist (`native`, `macho`, `wine`), so per-triple selection is coarse and most cells are gated by `if(UNIX)`-style conditionals rather than a label. Audit every `add_test` for: (i) whether it could run on MORE triples than it currently does (many are native-only by habit, not necessity); (ii) whether it bundles several independent assertions that would yield more coverage as separate tests — the per-golden `exec/<name>` split is the model to copy. ~~Concrete known gap: the exec corpus never runs at `-O3` with DEFAULT gates.~~ **CLOSED 2026-07-27** — `exec-O3/*` (299 cells) now runs the corpus at `-O3` with defaults; 299/299 green in 8.7 s. **SECOND, BIGGER GAP CLOSED 2026-07-27: `optfire/*` (38 cells) — the suite now asserts optimizer passes actually FIRE.** Until now every `exec-*` family (298 goldens x 21 variants) asserted CORRECTNESS ONLY, so a pass stubbed to `return 0` passed all of them — `exec-tile/loop_tile` stays green if `ast_tile_run()` does nothing — and eighteen gates had no effect assertion anywhere. `tests/optfire/` adds two modes: **counter** (compile with `--stats=4`, require the pass's transform counter, which is its `_run()` return value, to be nonzero) covering the 21 named strategy passes, all of which fire at `-O2`; and **differ** (toggle the gate, require the objects to differ) covering 17 gates that have no counter. Both also require the optimized program's stdout to equal the `-O0` build's, so a pass that fires but MISCOMPILES fails too — `-O0` is the oracle, no goldens needed. Validated in the direction that matters, that they can FAIL: forcing `MCC_AST_{DIVMAGIC,SELECT,RANGE,PRE}=0` makes each cell report `DID NOT FIRE`. Full suite 7159/7159. Two trigger conditions worth keeping: `ltemp` needs the loop-invariant subexpression to appear at least TWICE, and **no AST gate can change codegen unless the enclosing function replays `faithful`** — `MCC_AST_VERIFY=1` prints that verdict and is the fastest way to distinguish 'pass declined' from 'function was never eligible'; `CHAINSTORE`/`OPASSIGN` work precisely BY flipping functions to faithful, which is why their obvious test shapes come out byte-identical. **Extended to 106 cells 2026-07-27** (adding 12 `defstate` cells that assert a gate's DEFAULT state, which differ mode cannot — see the Ungate campaign for the `ARGFWD` finding that produced)

   *(earlier revision said 93; that count predates the defstate mode)* **Extended to 93 cells** (21 counter + 21 level-map + 51 differ), covering the strategy passes, the pipeline/structural gates, and the sub-knob families (`IDENT_*`, `REASSOC_*`, `BFOLD_*`, `NARROW_*`, `SCCP_FIX`, `CSE_COMM`, `DSE_CALL`, `TCO_PTR`, `SETHI_LEAF/NARY`). Every gate attempted reached DIFFER; none was abandoned. **MUTATION-TESTED, which is the evidence that matters:** stubbing `ast_tile_run`/`ast_fusion_run`/`ast_interchange_run` to `return 0` makes the corresponding `optfire` cell FAIL — and, in the same build, **`exec-tile/*` still reports 299/299 PASS**. That is the audit's claim confirmed empirically rather than argued: the golden corpus is blind to a pass that does nothing. Findings from the case work, worth keeping: (1) the constant-operand identities (`x+0`, `x*1`, `x<<0`, `x&-1`) are folded by `gen_opic` BEFORE AST recording, so `ident` never sees them — only same-operand forms (`x-x`, `x^x`) and unsigned-vs-0 relationals reach the AST; (2) `MCC_AST_IDENT_SHIFT`'s only rule is shift-by-literal-0, which cannot exist at record time — the literal has to be manufactured by `cprop`, which runs AFTER `ident`, so it fires only on a second pipeline cycle and therefore needs `-O3` (`MCC_AST_CYCLE`); (3) `narrow_fix`/`sccp_fix` are pinned to `-O2` because at `-O3` the cycle reaches the same fixpoint anyway and toggling the sub-knob changes nothing. **Arch guard landed 2026-07-27** — `tests/optfire/arch.txt` (`<name>|<cpu>[,<cpu>]`) registers a case only when `MCC_CPU` matches, so a gate that is compile-time inert on a target cannot report a spurious DID-NOT-FIRE. `regdisp` is listed x86_64-only (`ast_regdisp_env` is hard-0 elsewhere). Verified both ways: 93 cells on x86_64, and forcing the requirement to arm64 prints `optfire: skipping regdisp (needs CPU arm64, have x86_64)` and drops to 92. **Cross-triple: harness support LANDED, the blocker is the case SOURCES (2026-07-27).** `OPTFIRE_NORUN=1` asserts only that the pass fired and skips the `-O0` oracle and the program runs — the cross-compiler mode, since a cross mcc cannot link a runnable host binary. `OPTFIRE_MCCFLAGS` passes `-B`/`-I` through. Demonstrated against `cmake-cross/mcc-i386`: both counter and differ PASS, and forcing the gate off still FAILs, so the assertion is live and not vacuous. **All 72 case sources are now FREESTANDING (2026-07-27)** — `#include <stdio.h>` replaced by `extern int printf(const char *, ...);`, so a cross mcc can compile every case without target headers. That removes the last blocker on the compile-side half of cross enablement; what remains is wiring per-target cells and judging the x86-shaped cases. **The conversion exposed a test that was passing for the wrong reason, which is the more valuable result.** `optfire/ident` went red immediately: its `ident` count of 1 came from glibc's own inline functions in `stdio.h` (`vprintf`/`getchar`/`fgetc_unlocked` compile as faithful functions and one of them folded), NOT from the case's own identities — which never reach the AST at all, because `x+0`/`x*1`/`x&-1` are folded by `gen_opic` before AST recording. Rewritten around double-convert identities `(int)(long)x`: now 2 folds from the case's own code, 0 with the `IDENT_*` gates forced off, 0 at `-O0`. Its level row also had to be corrected from `-Os:off` to `-Os:on` — the old `off` was an artifact of the header-derived fold, not a property of the pass. **Lesson worth generalising: any case that includes a system header may be measuring the header, not itself.** Freestanding sources make the counter attributable to the case. **Retraction — do not repeat this mistake:** an earlier measurement here concluded the cross compilers ran no optimizer and that `--stats` counters were broken on them. Both were artifacts of those failed `stdio.h` compiles: `--stats` still prints its panel when the compile fails, so the counters read 0, and a `cmp` of two objects that were never written reports 'differ'. Measured properly with a freestanding source, `mcc-i386` reports `divmagic=2`, exactly like the native compiler. **Always check the compile's exit status before reading a counter or comparing objects.** Also still open per-case: several cases are x86-shaped (`select` expects cmov-style if-conversion, `sethi_*` counts x86 register pressure) and may legitimately not fire elsewhere — a per-case judgement, not a bulk enablement.

**RETRACTED 2026-07-27 — `MCC_AST_INLINE` is NOT dead. The finding below was wrong; kept because the reasoning trap is worth seeing.** I concluded it from `do_inline`'s `!ast_inline_pass_env` guard alone, and the capture-time graft genuinely cannot fire while INLINE_PASS is on. But `ast_inline_env` has OTHER consumers — notably `ast_reemit_retain`, which retains function bodies for re-emit independently of `do_inline`, and the post-capture inliner consumes those. Measured at `-O3` on `libmcc.c`: default gives **34 grafts / .text 926489**, `MCC_AST_INLINE=0` gives **0 grafts / .text 913547**. The gate is load-bearing; turning it off removes all inlining. (Aside: inlining costs 1.4% size here, which is the expected size/speed trade, not a defect.) An attempted 'honest default' of `>= 3 && !optimize_size && !ast_inline_pass_env` was reverted — it is NOT byte-identical, at any of -O1/-O2/-O3/-Os, for exactly this reason. **Lesson: grep every consumer of a gate variable before calling it dead; one guarded call site does not make the gate inert.** `MCC_AST_ARGFWD` (single use, inside the graft path) and `MCC_AST_PERFN_INPROC` (`do_inline`-guarded) ARE inert as recorded — re-checked across ALL of `src/` after the retraction, not just `mccast.c`: each has exactly one consumer (its sole consumer in `mccast.c` and its sole consumer in `ast_func_end`), so there is no second path the way `ast_inline_env` had one. Original, incorrect finding follows.
**FINDING 2026-07-27 (from writing `optfire`): `MCC_AST_INLINE`'s default-on condition is DEAD — decide whether that is intended.** Its default is `s1->optimize >= 3 && !s1->optimize_size` (`mccast.c`), but the capture-time graft is guarded by `int do_inline = faithful && !do_tco && !ast_inline_pass_env && ...`, and `MCC_AST_INLINE_PASS` defaults on from `-O2` (`o4 || s1->optimize >= 2`). So at every level where INLINE turns itself on (-O3, -O4) INLINE_PASS is on too and the graft is unreachable. Measured with `MCC_AST_REPLAY_DUMP=1` at `-O3`: **0 `[ast-inline] grafted` by default, 9 with `MCC_AST_INLINE_PASS=0`.** The two inliners are alternatives by design — `ast/replay-inline` deliberately runs `-O1` with `MCC_AST_INLINE=1` (INLINE_PASS is off below -O2 there), and an earlier attempt this session to couple them broke that test and was reverted — so the mechanism is fine. What is wrong is the DEFAULT: it advertises a pass that can never run. Pick one: (i) drop the default to 0 and treat INLINE as explicitly opt-in for the `-O1` virtual-inline path, (ii) make it `>= 3 && !optimize_size && !inline_pass` so the condition states the real requirement, or (iii) leave it and document the subsumption at the gate. `optfire/inline_capture` pins the behaviour either way by forcing `MCC_AST_INLINE_PASS=0`. Same guard makes `MCC_AST_PERFN_INPROC` unreachable by default, but that gate is default-OFF so it advertises nothing.

3. **Cut CI wall-clock (currently ~18 min).** **Data gathered 2026-07-27 — read this before proposing a split.** `cmake-debug/Testing/Temporary/CTestCostData.txt` already carries per-test timings, so no instrumented run is needed. Distribution over 7122 tests, 1896 s serial, mean 0.266 s:

   | band | tests | serial | share |
   |---|---:|---:|---:|
   | >= 2 s | 197 | 715 s | 37.7% |
   | 0.5-2 s | 660 | 965 s | **50.9%** |
   | < 0.5 s | 6265 | 216 s | 11.4% |

   **There is no dominant long tail**: the slowest 25 tests are only 12.4% of wall, the slowest 100 are 26.2%, the slowest 200 are 38.0%. So a base/compound/complex split that moves a handful of obvious hogs (the `*-docker` cells, `fuzz/matrix-*`, `ast/magic`, `cross-factory`) buys ~12%, not a halving — the mass is in the 0.5-2 s middle band, which is 661 ordinary tests. Any tiering has to move that band to matter, which is a much bigger and riskier reorganisation than the item first assumed.

   **Also check the premise before reorganising anything.** Locally the whole suite is ~165 s wall at `-j12` while `native` is ~2235 s*proc — i.e. it is parallel-bound, not serial-bound. A 2-4 core CI runner would spend ~8-16 min on the same suite, which plausibly IS most of the 18 min, but the CI job also installs a toolchain and builds before testing (`.github/workflows/ci.yml`: install toolchain → run preset → pack). **Measure the split between install / build / test on a real CI run first** — if build or toolchain install is a large share, test tiering is the wrong lever and more runner cores or better caching is the right one.

   ORIGINAL PROPOSAL:  Proposal to evaluate: tier the suite into **base / compound / complex** — base gates every push, compound on PR, complex on merge/nightly. Gather the data before splitting: per-test wall-clock (the suite is 6522 tests; `native` alone is ~2200 s*proc, so it is already parallel-bound, not serial-bound). Look first at the biggest single consumers (self-host fixpoints, qemu cells, the `-O4` search cells) since tiering only pays if the long tail moves out of the push path.

4. **~~Add an `mcc -O4` row to the bench report.~~ DONE 2026-07-27.** `tools/bench.c` now sweeps `{default,-O1,-O2,-O3,-O4}`; `struct compiler` gained a `reps` override and the `-O4` entry pins it to 1 repeat because each compile runs the out-of-process superopt. First numbers on `portable-corpus` make the previously-invisible trade explicit: `-O3` 0.020 s CPU / 10190 funcs-per-s / 102.5 KB obj vs `-O4` **8.384 s / 26 funcs-per-s / 94.1 KB** — roughly **420x the compile time for ~8% smaller output**. ~~Follow-up: the `vs-ref` column compares against gcc at the same index, which is meaningless for `-O4`.~~ **DONE 2026-07-27** — `vs_ref()` now prints `n/a (superopt search, not a compile)` for `-O4`, and the legend says why: gcc/clang accept `-O4` but treat it as `-O3`, so the row was comparing mcc's out-of-process search against an ordinary compile. MSVC still has no `-O4` equivalent (its opts list carries a matching `NULL`), which is correct and needs nothing. ORIGINAL ITEM:  `tools/bench.c` sweeps `{default, -O1, -O2, -O3}` (`gccopts[]`, with the MSVC list separate) and never reports `-O4`, so the superoptimizer's actual cost/benefit is invisible in the published numbers. Add it. Design around the cost: `-O4` is ~20 s per compile, so it needs a reduced repeat count or a dedicated row rather than joining the main sweep at `--repeats 5`.

5. **Make the JIT/AOT slice cache cross-platform and LOOSE rather than rigid.** **First slice landed 2026-07-27: the slice STORE is no longer partitioned per triple.** `ast_search_key_salt` folded version + triplet into the cache filename, so every target kept a private `sl-<key>.ck` and re-derived the same slices. Split into `ast_search_key_salt_ex(h, per_triple)`: the search memo and its consumers keep the triplet (conservative — their hits are not intersection-filtered), while the slice store and `jit_graduated_find` use `ast_slice_key_salt`, which folds VERSION ONLY (the version still guards record FORMAT). Safety is not an assumption — it is the `warm = cached & ast_search_searchable(base)` intersection on the consume path: a foreign entry can only ever select a SUBSET of gates the current target already permits, never enable one it does not, so a wrong hit costs a suboptimal-but-legal gate subset. Verified: default-off byte-identical at -O0/-O2/-O3, ast+slice selftests 75/75, full suite 7121/7121. **NOW DIRECTLY OBSERVED 2026-07-27 — cross-triple filename sharing is CONFIRMED at runtime.** With `XDG_CACHE_HOME` pointed at a scratch dir and `MCC_AST_SLICE=1`, an x86_64 `mcc` and a native 32-bit `mcc32` compiling the SAME program both wrote **`sl-a70e6ef1ff6af646.ck` — byte-identical filename across triples**; pointed at one SHARED dir they use a single file. **CORRECTION (an earlier revision of this entry was wrong):** it claimed the store only opens when a slice GRADUATES and therefore needs `MCC_AST_SLICE=1` + `MCC_JIT=1` + `-run` together. It does not. `MCC_AST_SLICE=1` alone is sufficient — a plain `-c` compile with the JIT OFF writes the identical 832 B file, verified across `MCC_JIT=1 -run` / `MCC_JIT=0 -run` / `MCC_JIT=0 -c`. The writer is the atexit static flush (`ast_slice_flush_atexit` -> `ast_slice_disk_commit`), not `ast_slice_graduate*`. **Consequence to keep in mind for every measurement below:** all records observed so far carry `proven=0`, i.e. they are STATIC observations, and the JIT-proven graduation path has NOT actually been exercised yet. Identity results are unaffected (an ident is an ident), but any claim about PROVEN records remains unmeasured. **One gotcha that cost a false negative:** the salt folds `MCC_VERSION_STR`, so the two binaries must be the SAME version or filenames differ for reasons unrelated to the triple — a hand-built `gcc -m32 ... src/mcc.c` defaults to a different `MCC_VERSION` than the cmake build, so pass `-DMCC_VERSION=<value from compile_commands.json>`. `MCC_SLICE_DUMP=<file>` dumps every committed record as `ident g=<gates> size proven`.

**Width-normalization DONE 2026-07-27 (`MCC_AST_SLICE_WIDTHNORM`, default OFF) — cross-triple slice identity is 100% on the measured case.** Two normalizations, both under the one gate, each found by measurement rather than inspection: (1) `ast_sid_type_norm()` strips `VT_LONG` (0x0800), a spelling tag rather than a width — i386 `long` is `VT_INT|VT_LONG` (4 B), x86_64 `long` is `VT_LLONG|VT_LONG` (8 B). (2) `ast_sid_ival_norm()` folds a `VT_PTR`-typed node's `ival` as (quotient, remainder) over `MCC_PTR_SIZE`, because that ival is a byte offset ALREADY scaled by the target pointer size — the same address computation literally reads `iv=8,16` on x86_64 and `iv=4,8` on i386, with every other word in the node identical. Folding quotient and remainder separately loses nothing: an unscaled value still separates. **Measured** (matched sources, `int` on x86_64 / `long` on i386, 26 idents committed per side, via `MCC_SLICE_DUMP`): **gate off 6 shared / 20 i386-only; gate on 26 shared / 0 i386-only.** **Negative controls confirm it is not just collapsing everything** — (a) two DIFFERENT programs on the same arch share only 12 of 26/17 idents; (b) an `int` kernel vs a `long` kernel on the SAME x86_64 target share only 14 of 26, i.e. genuine 4-byte vs 8-byte operations stay distinct. The gate aligns targets that SPELL a width differently; it does not merge different widths. **Correction to the previous entry:** it claimed the two `AST_Convert` residuals were legitimately-different casts and that the ceiling was ~92%. That was wrong. Those Converts had a `VT_PTR` root — they were pointer conversions carrying scaled offsets, not the `(long)s` cast — and normalization (2) resolves them correctly. The real `(long)s` width difference is what negative control (b) still separates. Default OFF because a changed slice ident changes which gate config `ast_slice_consume` warm-starts, which can change codegen; full ctest 7121/7121 with it off. **POTENTIAL-optimization storage: MECHANISM LANDED but UNEXERCISED — read this before trusting it** (`MCC_AST_SLICE_MULTI`, default OFF). Off, the store keeps ONE record per ident: `ast_slice_merge_one` collapses competing configs (proven beats static, then cheaper size) and the consumer masks whatever single config it is handed — a fixed decision, exactly what this item says it should not be. On, records are keyed by (ident, gates) so several CANDIDATES coexist, and `ast_slice_probe_table_ex` takes the target's `searchable` mask and prefers the candidate retaining the most gates AFTER masking, instead of degrading an arbitrary one. The consume call site now computes `searchable` before the probe rather than after, so legality informs the choice. Byte-identical off: asttool 747/747, ctest 7121/7121. **NOT DEMONSTRATED TO DO ANYTHING, and do not record it as working.** Every configuration tried produced exactly ONE config per ident, so multi mode had nothing to distinguish: same-arch repeat, cross-triple x86_64-vs-i386, forced gate envs (`MCC_AST_INLINE=0 MCC_AST_CSE_JOIN=0 MCC_AST_PRE=0`), and `-O4` search on both targets ALL gave a 0-byte store delta. The reason is visible in the dump: all 26 records carry the SAME mask `g=000000fffffff83f` and `proven=0`. So the candidate set is degenerate on these workloads, and the open question is no longer 'implement multi-candidate storage' but **'what workload ever produces two different gate configs for one slice ident?'** — most likely one that actually reaches the JIT-proven graduation path, which per the correction above has never been exercised. **PARTIALLY ANSWERED 2026-07-27 — the proven path is DARK CODE in every configuration tried, and that is the real blocker.** Traced the graduation trigger: `ast_slice_graduate_arena` is called only from `mccjit_graduate_slices_blob`, which fires only under `gs_bench_won && best` inside `mccjit_lazy_search`, which is reached only via `mccjit_lazy_entry` when `MCC_JIT_SEARCH` is on. Setting **`MCC_JIT_SEARCH=1 MCC_JIT_BENCH=1 MCC_JIT_SEARCH_MS=400`** together with `MCC_AST_SLICE=1 MCC_JIT=1` on a hot `-O2 -run` still produced **26 records, all `proven=0`, one gate mask** — and `--stats` shows NO gsearch line at all, i.e. `mcc_stats_jit_gsearch` never ran, so `mccjit_lazy_search` was never entered. Meanwhile the same run reports `recompiles=11 swapped=3`, so the JIT IS working — it just recompiles through a route that never graduates. **Two consequences worth acting on before any more slice-cache work.** (1) `MCC_AST_SLICE_MULTI` cannot be justified or refuted until something reaches this path, so leave it default-OFF and do not build on it. (2) More importantly, the `proven` concept is itself UNTESTED against real data: `ast_slice_merge_one`'s 'proven adopts outright, static cannot override proven' rule and the probe's 'proven outranks static' ranking have only ever been exercised by asttool unit tables, never by a record the JIT actually wrote. **REACHED AND DIAGNOSED 2026-07-27 — the blocker is `admits=0`, not a missing switch.** `mccjit_lazy_entry` hangs off the LAZY counter-stub tiering path, not the eager recompile route, and needs **`MCC_JIT_LAZY=1`** plus a threshold low enough to actually tier (`MCC_JIT_HOT_THRESHOLD=5`; the default 1000 is never reached by mcc's own functions during one small compile). Full repro: `MCC_AST_SLICE=1 MCC_JIT=1 MCC_JIT_LAZY=1 MCC_JIT_SEARCH=1 MCC_JIT_BENCH=1 MCC_JIT_SEARCH_MS=200 MCC_JIT_HOT_THRESHOLD=5 mcc -O2 --stats -run <hot.c>` — verbose then shows `install`/`promote`/`cold` events and `--stats` gains the gate-search line. **With that, the search RUNS and still cannot graduate: `gate-search: 3 runs 639 cands admits=0 budget-hit=0 best=0x00000000`.** `gs_bench_won` is set only inside `if (admit)`, and `best` stays NULL, so `mccjit_graduate_slices_blob` is unreachable while the admit rate is zero. Records stay `proven=0` and the store keeps one gate mask, which is why `MCC_AST_SLICE_MULTI` has nothing to distinguish. **CONFIRMED as the KGC defect, and then refined — graduation needs TWO things, not one (2026-07-27).** First half confirmed by bisect: the 639 candidates are not being REJECTED at the admit test, they fail to BUILD — `mccjit_lazy_build_masked` returns NULL and `if (!cand) continue` skips them. KGC verification is the cause: `MCC_JIT_NO_KGC=0` gives `3 runs 639 cands admits=0`, while `MCC_JIT_NO_KGC=1` gives **`1 runs 4 cands admits=1 best=0xfffffff83f`**. Note this is NOT `mccjit_bench_admit` — with `MCC_JIT_BENCH=0` (so `admit = improved`) the count is still 0, so the bench is innocent. **Second half, and the reason bypassing KGC still does not graduate:** with `MCC_JIT_NO_KGC=1 MCC_JIT_BENCH=1` the store STILL holds 26 `proven=0` records and no `mccjit-graduate` line appears. That single admit is the WARM-START seed taken before the vocab loop (`mccjit_intent_peek_warm_gates` -> build -> `gs_admits++`), which deliberately does NOT set `gs_bench_won` — the code requires a genuine bench win over an existing incumbent. The vocab loop admitted nothing that beat the AOT-selected warm mask. **RESOLVED 2026-07-27 — (a) and (b) are COUPLED, and the coupling is a structural deadlock. The previous advice here (check (b) first) is retired: (b) cannot be tested independently.** (b) alone is achievable and was demonstrated: the earlier `admits=1` was a BUDGET artifact, not a ceiling — raising `MCC_JIT_SEARCH_MS` from 200 to 3000 gives **`1 runs 4 cands admits=4 best=0x0003c00e`** on a.c/b.c/c.c alike, and that winning mask DIFFERS from the warm-start `0xfffffff83f`, so vocab variants genuinely do beat the AOT config. **But it still graduates nothing**, because `gs_bench_won` also requires `r` (routed), and `*routed = 1` is set at exactly ONE place: when `mccjit_make_kgc_stub_*` successfully builds a stub. So `routed == 1` if and only if a KGC stub exists. Therefore: **KGC on -> build returns NULL -> `admits=0`; KGC off (`MCC_JIT_NO_KGC=1`) -> builds succeed but `routed=0` -> `gs_bench_won` never set.** The escape hatch that buys admissions is the same switch that destroys the routing the graduation condition tests for. **Consequence: `MCC_JIT_NO_KGC=1` can NEVER produce a proven record, by construction — do not use it to try.** **PINPOINTED 2026-07-27 to one line.** `mccjit_lazy_build_masked` (`mccjit_embed.c`) ends:
```c
if (!entry && no_kgc)
	entry = variant ? mccjit_make_trampoline(variant) : NULL;
return entry;
```
The unrouted-trampoline fallback is gated on `no_kgc`. So with KGC ON and `mccjit_last_kgc_ok` false — which is nearly every function, since `ast_fn_purity` returns IMPURE on the first store or call — `entry` stays NULL and the candidate is DROPPED, which is the `admits=0` reading. With KGC OFF the same line installs a trampoline, which is why builds then succeed but `routed` stays 0.
**This is by DESIGN, not a bug**: an unverifiable function should not be silently JIT-swapped. So there is no clever way to break the deadlock at this line — installing the trampoline under KGC-on would be exactly the unsound behaviour KGC exists to prevent. The only sound route is the blocked item above: widen what CAN be verified (`MCC_JIT_PURITY_NOESCAPE`, already implemented, default-OFF behind two measured obstacles — the ~9x verify-stub cost and near-match parity divergence). Everything downstream — proven records, `MCC_AST_SLICE_MULTI`, benchmarking the eligible set — waits on those two, and no amount of work on the slice cache moves them. The only path to a proven slice record is fixing KGC verification itself so builds succeed WITH routing, i.e. the blocked item above and its two measured obstacles. Everything downstream — proven records, the `proven`-outranks-`static` ranking, `MCC_AST_SLICE_MULTI` — is gated behind that single fix and cannot be exercised or validated before it. ORIGINAL:  The disk key currently folds **version + triplet** into the salt, so nothing is ever shared across targets — every triple re-derives the same optimization. Wanted: a slice that is a *pure optimization equivalence* should hit the same entry regardless of target width (a 32-bit `long` and a 64-bit `int` produce the same runtime values for most such slices, and should match). Design direction: the identity is a normalized-kernel-hash, so width-dependent nodes need normalizing out of the hash (or the entry needs a width-tolerance predicate), and entries should store **POTENTIAL** optimizations re-validated at use rather than fixed decisions. This is sound to make loose: a wrong hit costs a suboptimal pass order, never a miscompile — the existing per-reuse `ast_slice_equiv`/certifiable check still gates codegen. Interacts with the arena-normalization item in P0 step 3, which is the other half of "the caches must agree too".

## AOT must cache ALL ELIGIBLE optimizations, not just the one it chose (requested 2026-07-27)
**Requirement.** An AOT compile should persist every optimization that was *eligible* for each slice, so the runtime JIT
can benchmark the full strategy space at runtime instead of inheriting one AOT decision. Today it persists a decision.

**What the code does now.** `AstSliceMemo.gates` is documented in `mccast.c` as "enclosing function's CHOSEN gate
config" — one applied mask per slice ident. `ast_slice_memo_put` overwrites it when a cheaper observation arrives, and
`ast_slice_merge_one` collapses competitors on commit (proven beats static, then smaller size). So the on-disk record is
a fixed decision by construction, which is exactly what item 5 above says it should not be.

**What already exists and should be reused, not rebuilt:**
- `ast_search_searchable(base)` already computes the ELIGIBLE set for the current target — the gates that are legal to
  vary here. That is the value this item wants persisted; it is currently computed, used to mask a consume, and thrown
  away.
- `MCC_AST_SLICE_MULTI` (default OFF) already lets several `(ident, gates)` candidate records coexist and already makes
  the consumer prefer the candidate retaining the most gates after masking. It is implemented and **unexercised**
  precisely because AOT only ever emits one config per ident — this item is the missing producer for it.
- The record format is version-salted (`ast_slice_key_salt` folds `MCC_VERSION_STR`), so adding an `eligible` field is a
  format change that old caches will correctly miss rather than misread.

**Work.**
1. ~~Persist the eligible mask per slice record.~~ **DONE 2026-07-27.** `AstSliceMemo.eligible` is set from
   `ast_search_searchable()` and stamped on every observation; `gates` still carries the chosen config for warm-start.
   Record widened 4 -> 5 words, so `AST_SLICE_REC_MAGIC` bumped `'SL'` -> `'SM'` and old files SKIP rather than
   misparse at the wrong stride. Verified with gates forced off: `g=000000fffff9f83f e=000000fffffff83f`.
2. ~~Expose the candidate space on the consume path.~~ **DONE 2026-07-27.** `ast_slice_probe_table_cand()` reports the
   winning record's chosen config and eligible space separately; existing callers and the 4-arg wrapper are unchanged.
   Written test-first (spec failed on the missing symbol, then 4 checks pinned the contract).
   Locked by `cli/slice_eligible_set`, which was verified to FAIL against a compiler that records the chosen mask
   instead of the eligible one. Two earlier versions of that cell were vacuous — see its comment; `MCC_SLICE_DUMP`
   appends, so a stale dump file makes `NR==1` read someone else's record.
3. **REMAINING — and it is the blocked half.** Have the JIT enumerate candidates from
   `eligible & ast_search_searchable(base)` at runtime rather than replaying `chosen`, and record the winner as
   `proven`. Also turns `MCC_AST_SLICE_MULTI` from unexercised into load-bearing. Gated on the KGC/`routed` deadlock
   below: nothing benchmarks anything until that clears. The AOT side is complete, so when it clears there is a real
   candidate space waiting rather than a single decision.

**Known downstream blocker — read before assuming this delivers benchmarking.** The JIT's benchmark/graduation path is
currently UNREACHABLE (see item 5): `gs_bench_won` requires both a KGC-routed build and a vocab candidate beating the
warm-start incumbent, and KGC-on makes the build return NULL while KGC-off clears `routed`. So a richer cache will not
be benchmarked by anything until that deadlock is fixed. The AOT half is still worth landing first — it is the
prerequisite, it is independently testable (dump the records and assert the eligible set is present and correct), and
it is what makes the JIT half meaningful when the deadlock clears.

## Divorce optimization from FUNCTIONS — any AST sub-slice should be promotable
**The goal.** Today AOT, JIT and the AST layer are all tightly coupled to *functions* as the unit of optimization. They should not be. **Any sub-slice of AST nodes can, should and will be eligible for optimization promotion**, identified by what it consumes and produces — not by which human-named function happens to contain it. A user program built with the embedded JIT should ship its **whole AST**, not an AST artificially chopped into per-function blobs, so the runtime can slice and splice at **any** call site — including `main` itself during JIT boot.

**What is function-coupled today** (all of this has to give):
- `MccjitIntent` (`mccjit_internal.h`) carries `fn_name` and `nparam`: the shipped blob IS a per-function unit, serialized one function at a time.
- Dispatch slots are emitted as `__mccjit_slot_<fn>` symbols (`mccast.c`) and resolved BY NAME, so the runtime's addressing unit is a named function.
- JIT eligibility is a *signature* predicate — non-varargs, 1..6 params, scalar/`double` params and return. A slice with no signature at all cannot be expressed, and `main` is excluded twice over (zero params, and it runs once so tiering never fires).
- KGC verification is built per function signature (`_n`/`_fp`/`_mixed` stub builders keyed on GP/FP arg classes), so the verify story is signature-shaped too.

**What already points the right way** — do not rebuild these:
- Slice identity is *already* specified as the **normalized-kernel-hash ALONE, context-free** (Slice cache §), explicitly so a slice can be reused across functions and TUs. That is the hash the user asks for; it just is not the unit the JIT ships or dispatches on yet.
- `ast_slice_ident_hash` / `ast_slice_window_scan` / `ast_slice_locate` / `ast_slice_splice` / `ast_slice_promote_static` already exist as node-level primitives.
- **Whole-vs-sub-slice is now expressible in the shipped format without a fake root — LANDED 2026-07-27.** The dead `AST_TranslationUnit` kind (enum value 0; never created by the compiler — the real arena root is `AST_BasicBlock` and `ast_root()` is positional index 0; its only users were two synthetic-container sites in `tools/asttool.c`) is REMOVED, and the whole-function-vs-promoted-sub-slice distinction it nominally implied now lives as one byte of intent-header metadata: `MccjitIntent.unit_kind` ∈ {`MCCJIT_UNIT_WHOLE`, `MCCJIT_UNIT_KERNEL`} (`mccjit_internal.h`), serialized in the header right after `warm_gates` (`mccjit_intent.c`, both `_serialize`/`_deserialize`; `mccjit_intent_peek_warm_gates` stops before it, unchanged), `MCCJIT_INTENT_FORMAT` bumped 8→9. Today it is DERIVED (`sym ? WHOLE : KERNEL` — a kernel slice has no owning `Sym`, hence no signature trailer); when item 1 below actually serializes owner-less kernels it becomes the load-bearing discriminator that lets a shipped blob declare it is a sub-slice rather than a function. **Critically, `unit_kind` is header-only and is NEVER folded into any hash:** `ast_slice_ident_hash`→`ast_sid_node` (mccast.c) hashes only the subtree's own structure (kind, op, type_t, positionally-interned sym/local-offset, fbits, nchild, recursively over children) — no parent, no index, no unit metadata — so an inline slice identical to a whole-function body hashes EQUAL regardless of which it is, which is exactly the context-free identity item 2 requires. Enum renumbering (`AST_BasicBlock` is now kind 0) is byte-identity-safe: `AstKind` is an internal optimizer tag never emitted into object code, so codegen is unchanged; the only serialized effect is the `kind` u16 in intent blobs (covered by the FORMAT bump) and absolute hash values (covered by the version+triplet cache salt, so old `sl-<salt>.ck` caches simply miss). Validated: clean build; asttool 66/66 suites; JIT round-trip selftests 7/7 (`leaf-int`/`reemit-gates`/`stage2`/`kgc`/`strlit`/`struct` — these serialize→deserialize→replay→codegen through the FORMAT-9 header). NOT yet validated (do before relying on it under load): a full 3-stage self-host fixpoint and the exec/fuzz corpus; the change is argued byte-identical rather than measured so, because no codegen path reads `AstKind` as an integer.

**The work.**
1. **Ship the whole AST.** Replace per-function intent serialization with one arena per module (or per TU), so the runtime holds the real graph rather than N disconnected function ASTs. Keep a function index as a *view* over it, not as the storage unit.
2. **Ins/outs hash.** Define slice identity purely from the boundary: the set of values entering the sub-slice and the set leaving it (types/widths normalized — see item 5 on width-normalization, which this shares), plus the internal node shape. No `fn_name`, no parameter ordinal, no source location. Two textually different functions containing the same kernel must hash equal.
3. **Optimization Reconciler.** The piece that does not exist yet: after a sub-slice is optimized or hot-patched, something must **re-wire the ins/outs** so entry and exit stay coherent with the surrounding graph — register/stack locations, live ranges, and the splice-point contract on both sides. `ast_slice_splice` is node-identity-stable in-arena today, but it does not reconcile a *differently-shaped* replacement's boundary; that is the gap. It needs to handle: a slice whose optimized form needs fewer/more live-ins, one that sinks a computation past the exit, and one spliced at a call site where the caller's frame is already laid out.
4. **Dispatch by identity, not name.** Slots keyed by slice ident rather than `__mccjit_slot_<fn>`, so a promoted slice inside `main` — or inside a function the eligibility predicate rejects today — is addressable.

**Why this is the shape of the payoff.** The measured KGC-refusal blocker (§ above) is largely a *function-granularity* artifact: whole functions are refused because they contain a store, a call, or a signature the verifier cannot express, even when the hot kernel inside them is trivially verifiable. Sub-slice promotion sidesteps that — verify and promote the kernel, leave the rest of the function alone. Sequence it after the arena-normalization work (P0 step 3), which is the prerequisite for JIT-shaped and AOT-shaped arenas agreeing on any identity at all.

## Slice cache — rolling-window JIT↔AOT content-addressed optimization
The optimizer is being refactored from whole-function to a growing leaf-first **rolling window of AST slices** addressed by a **context-free normalized identity**, cached on disk as the shared substrate between the runtime JIT and pure AOT. Goal: cross-function/incremental memoization, sharper `-O4+` search, and **AOT `-O4+` emitting slice optimizations a prior JIT run benchmark-proved — even with the JIT compiled out**. All gated behind `MCC_AST_SLICE` (default OFF ⇒ byte-identical); codegen may change only when a warm cache steers it (validated by exec/diff parity, not byte-identity). Load-bearing decision: **identity = normalized-kernel-hash ALONE (context-free)** — forced by the JIT→AOT reuse requirement (a fresh AOT compile must hit a slice a different function/TU optimized under the JIT, and identity must survive inlining and other layers on the same nodes); the dependency path lives in a *separate* locator, never in the identity. Safe because a gate-config cache value is correct under any collision (worst case suboptimal passes, never miscompile); per-reuse `ast_slice_equiv`/`certifiable` soundness is only needed once actual variants are spliced. Primitives landed: `ast_slice_ident_hash`, `ast_slice_window_scan`, the disk store (`host_cache_dir()/sl-<salt>.ck`, salt=version+triplet, flock+tmp+atomic-rename merge), `ast_slice_consume` warm-start, JIT graduation (`AstSliceMemo.proven`), and the hot-patch trio `ast_slice_splice` (node-identity-stable in-arena replace) / `ast_slice_locate` (identity→all sites) / `ast_slice_promote_static`.

Observe on a build: slice cache lives at `host_cache_dir()/sl-<salt>.ck` (on macOS `~/Library/Caches/mcc/...`); set `MCC_AST_SLICE=1` (default off ⇒ byte-identical); the warm-start trace line is `ast_slice_consume: slice warm-start` (mccast.c). Search engine is `combo_run` (mcccombo.h), permutation axis via `MCC_AST_SEARCH_ORDERED=1`.

Remaining:
- **JIT/AOT arena-normalization alignment — P0 step 3.** The JIT recompiles from a differently-normalized arena than AOT, so JIT-written PROVEN idents don't reliably match AOT-shaped idents ⇒ cross-mode proven hits aren't guaranteed in practice. Align identity across the two arenas. This is the same gap that makes the graduation→PROVEN mechanism only unit-proven, not demonstrable end-to-end.
- **Productive isolated per-slice optimizer** whose result the from-scratch downstream re-emit wouldn't independently rediscover (per-slice gate search / strength-reduction under a gate the downstream mask omits). Today folding a captured slice finds nothing new, so the promotion gate correctly REJECTs and the KEEP→splice branch is unit-proven, not live-e2e.
- **Per-slice `mccjit_bench_pair` micro-bench tuples** (the runtime arm; today per-slice uses the static gate, whole-function bench stays in `mccjit_lazy_search`).
- **Hierarchical-hash upstream propagation.** Chain a slice's hash into enclosing scopes/callers/KGC slots so re-optimizing one propagates up and re-patches every dependent; `ast_slice_locate` is the identity lookup those consume.
- **Wire the splice primitive into the AOT in-place emit path.** Blocked on emit-cursor desync: `ast_func_end`'s replay re-emits from three whole-function ordered cursors (`ast_locrec`/`ast_fconst`/reloc) recorded at capture; splicing a differently-shaped subtree before that replay desyncs them. AOT currently keeps only the function-level gate warm-start.

## JIT runtime
- Build a bounded UB-sound compile-time loop interpreter in `ast_eval_slice`; fold no-escape-pure slices to constants into the submitted arena.
- Split the KGC key into (code-hash, data-hash); track poison per (code+data); feed poison as a search input.
- K-patch near-match acceptance is DONE and default-on (disable with `MCC_JIT_NEARMATCH=0`): a KGC-verified variant that mismatches the baseline is KEPT (not poisoned) IFF the mismatch set fits a 64-entry jump table AND it benchmarks faster; mismatching inputs are served from a per-KGC correction table. Follow-ups: emit an actual jump-table stub for accepted closed-domain variants (skip the calln dispatch); persist the correction table; a faithful mixed bench (per-arg GP/FP class vector) so the `calln_mixed_i/_d` paths can accept instead of only record+patch; fold accept/reject into the {good,bad,unknown} classified set and the (code,data) key split.
- **OPEN blocker — arm64-Windows `jit/selftest-nearmatch` SEGFAULT.** Deterministic SegFault ~1.2–1.4s in on the arm64-Windows cells ONLY (`msvc`/`sanitize-msvc`, both shards); x86_64 msvc/mingw/sanitize + all Linux (incl. arm64) green. Ruled out: the thread pool (single-threaded bench still faults), the `mccjit_invoke` signature cast, and local reproduction (this host's mstorsjo llvm-mingw ships no `libclang_rt.asan`). Known: the 4000 warmup `mccjit_kgc_call1` calls succeed; the fault appears once a scenario runs the accept/reject bench, which invokes the test's `mcc_relocate`'d functions ~100k× tight. Prime suspect: running those relocated functions under the heavy bench on arm64 where the W^X dual-map port is incomplete, or a latent memory bug exposed by the arm64 layout. Interim: the ctest is skipped on arm64-Windows (`CMakeLists.txt` when `WIN32 AND MCC_CPU STREQUAL "arm64"`, `SKIP_RETURN_CODE 77`; `tests/embed/jit_selftest_nearmatch.c` self-skips under `_WIN32 && (_M_ARM64||__aarch64__)`). This is a workaround — root-cause on arm64-Windows HW under windbg/gdb (faulting addr+stack, per-scenario `fprintf` checkpoints, native-arm64 ASAN), then remove the skip. The feature itself is proven on x86_64 + native arm64-Linux.
- Cross-code data-shape reuse of KGC entries: once the key is split, add a secondary index over the *data-shape* alone (a canonicalized descriptor — element dtype/width, layout/stride/alignment, value-range/known-bits, ABI class, loop trip-shape — independent of code-hash). On a shape hit for *different* code whose own specialization is missing or cold, speculatively try the shape-matched cached codegen as an extra candidate. JIT path: benchmark the borrowed candidate against the code's own baseline, keep only if it wins, poison per (code-hash, shape) if it loses or is unsound. AOT path: gate acceptance on the deterministic cost/emit-size model + static-range soundness. Hard parts: the shape-fit predicate + canonicalization, a soundness guard (verify or poison, never silently miscompile), bounding the extra lookups/benches. Fold shape-poison into the {good,bad,unknown} classified set.
- Switch-table cover strategy row (dense→jump-table, sparse→perfect-hash).
- Unify KGC+poison into one {good,bad,unknown} LFU-bounded classified set.
- Persist poison to the mmap'd cache under the opt-in persistent-cache flag.
- Parser-less re-emit-only engine slice + `-ffunction-sections`/`--gc-sections` for the ~800 KB Tier-B embed; reconcile CMake `libmcc-static.a` so plain `-lmcc` prefers it.
- General value-range narrowing fold (needs the VLat consumer).
- Small struct-by-value marshalling; promote `classify_x86_64_arg` (x86_64-gen.c) to `ST_FUNC`.
- Wire QSBR reclamation into the swap path + quiescent points (function-entry + loop back-edges).
- nop-padded patchable-prologue in-place code-patch + int3/trap + dual-map page-flip patch rows.
- Non-ABI register calling-convention kernel with boundary ABI harness; inline-vs-shim search axis; float + memory-boundary slices.
- **Cross-triple JIT self-host fanout — P0, see the parity matrix at the top.** The bootstrap itself is DONE on both ELF reference arches (x86_64 + arm64: a `--embed-jit`-baked `mcc-jit` self-compiles `src/mcc.c` under `MCC_JIT=1`, recompiling+hot-swapping its own functions, to an object byte-identical to the `MCC_JIT=0` AOT compile). Build a JIT-capable `mcc` for every target triple T (runs on T and JITs itself on T) via a per-triple 5-stage chain (the native path collapses stages 1+4 into the existing `mcc`): (1) `mcc-T` stage-0 host tool emitting T, built with the optimizer (`MCC_CROSS_OPTIMIZER`); (2) `mcc-T` self-compiles `src/libmcc.c` → `libmcc_jitengine-T.a` (the baked engine must itself be T code); (3) `bin2c` → `mccjit_blob-T.c`; (4) rebuild `mcc-T` + that blob (`MCC_EMBED_JIT_BLOB`); (5) `mcc-T --embed-jit` self-compiles `src/mcc.c` → `mcc-T-jit`. ELF (i386/x86_64/arm/arm64/riscv64) genuinely bootstrap (build here; run/verify native + x86_64/arm64/i386 via Docker, arm/riscv64 build-only). macOS-osx (x86_64/arm64) is blocked — Mach-O `--embed-jit` can't resolve `_mccjit_boot_swap` (separate bake bug, tied to the Mach-O `.o` reader/writer under macOS §). **PE x86_64-win32 now BOOTSTRAPS (2026-07-27):** `mcc_add_jit_selfhost()` is enabled on WIN32 (CMake `mcc-jit-selfhost` target, opt-in `MCC_BUILD_JIT_SELFHOST`), bakes a working `mcc-jit.exe`, and self-compiles byte-identical under `MCC_JIT=1` ≡ `MCC_JIT=0` on llvm-mingw. i386-PE is toolchain-gated (no i386 libgcc on this host); arm64-PE is HW-gated; the winlibs-gcc RUNTIME-JIT `0xC0000005` (P0 step 5) is a separate toolchain-specific blocker. Wire the remaining PE triples per-triple in the cross loop reusing `mcc_add_jit_selfhost()`; gate macOS with a STATUS naming its blocker rather than shipping broken targets.
- **arm64-osx runtime JIT is unavailable in every local build — P0 step 1 (the Mach-O reader unblocks both osx triples).** Baked `--embed-jit <src> -o out` errors `unresolved reference to '_mccjit_set_search_budget'`/`_mccjit_boot_swap` — those symbols are registered only for the in-process path via `mcc_add_symbol` (src/mccjit_embed.c), not in any `arm64-osx-libmccrt.a`, so `--embed-jit` to a file output is broken here. In-process `-run --jit` doesn't engage the JIT on ordinary code (0 `mccjit_*` calls in a `-v128` trace); `--embed-jit` on a trivial main "succeeds" only because it bakes nothing. So no with-JIT-vs-without-JIT baked-binary comparison is possible on Apple Silicon. Same root as the JIT/AOT arena-normalization gap. **Precise defect (traced 2026-07-26): the fix is NOT another `mcc_add_symbol` — that only binds a host function pointer, meaningful only for `MCC_OUTPUT_MEMORY`; file output is supposed to resolve `mccjit_boot_swap`/`mccjit_set_search_budget` by LINKING `libmcc_jitengine.a` (as ELF/PE already do — the file-output path emits a `mcc_compile_string` boot stub at mccjit_embed.c that expects the bodies at link time), and the real blocker is that mcc's own linker has no Mach-O object reader.** `mcc_object_type` (mccelf.c) sniffs only ELF/ar magic — a Mach-O `.o`/archive member returns type 0 and is rejected, so `_mccjit_boot_swap` can't be pulled from the Mach-O `libmcc_jitengine.a`. Fix: add an `MH_MAGIC_64`/fat branch to `mcc_object_type` returning `AFF_BINTYPE_REL`; add `macho_load_object_file` to mccmacho.c (which now also has `macho_object_type`/`macho_load_object_file` for PLAIN relocatables) as the peer of `coff_load_object_file` (mccpe.c); dispatch it from libmcc.c + the archive loop mccelf.c/3516 — the COFF path is the exact template. Minimal failing unit: feed one Mach-O `.a` member to mcc as a link input (rejected independent of `--embed-jit`). The JIT/AOT arena-normalization alignment (Slice cache §) is a separate follow-up needed to make graduated→PROVEN reuse demonstrable end-to-end once the link works.
- Flip `MCC_AST_JIT_EVAL_GATE` default-on after 3 clean self-host + fuzz soaks; extend the oracle to statement-level control flow.
- AOT-static sink scorer: deterministic cost/emit-size model + static-analysis ranges (AstVLat) + gain-ordered time-budgeted scheduling.
- Data→code substitution via a synthetic `.init_array` ctor.
- Fix the `ast_reemit` segfault when recompiling a pointer-RETURNING function (pre-existing, string-independent).
- Structural fix for `mccjit_bench_pair`: paired per-round ratio + median to cancel common-mode contention (env-independent) — currently a bench tie-flake; changes production promotion semantics + other benches ⇒ needs its own soak.
- **JIT handle-table complexity + data reduction, TDD-first.** Go through the `MccjitHandles` intern and the serialized handle-table/type-record format (`mccjit_intent.c`: `mccjit_handles_intern`, `mccjit_emit_type_record`, the serialize/deserialize handle loops) and drive every reduction as a **test-first proof**: for each candidate technique write the failing/asserting test BEFORE the change — a round-trip fidelity proof (serialize → deserialize → recompile/replay yields the bit-identical result and the same rebuilt `Sym`/`CType` graph) AND a measured reduction assertion (blob bytes strictly drop, or intern cost drops from O(n²)). The harness already exists to hang these on: the `mccjit-selftest-*` battery (`mccjit_embed.c`) round-trips intents and already prints `serialized … = N bytes`, and `tools/asttool.c` unit-tests the AST primitives — turn those byte prints into hard assertions. Enumerate ALL techniques and keep/drop each by its proven delta; the ones already visible in the code:
  - **Drop `handle_raw` from the wire.** The serializer writes the writer-process `Sym*` bits as a u64 per handle (`mccjit_put_u64(buf, handles.raw[k])`), the reader stores it into `out->handle_raw`, and **nothing ever reads it back** (verified: no downstream read of `it->handle_raw` anywhere in `src/`/`tools/`/`tests/`) — nodes reference handles by 1-based id and rebuild via `mccjit_build_rec`, never by raw pointer. 8 dead bytes per handle + a dead allocation on read. Prove the round-trip is identical with the field removed.
  - **Elide `token_v` when a name is present.** On read, `handle_token_v[i]` is overwritten by `tok_alloc(name)->tok` whenever `nm && nm[0]` (deserialize loop) — so the serialized `token_v` is only consulted in the no-name case. Presence-flag it (write it only when there is no name).
  - **De-quadratic the intern.** `mccjit_handles_intern` linear-scans all existing entries on every call, and the node-emit pass re-calls it per node after the collect pass already interned everything — O(n²) in handles×nodes. Do it (all reductions are valid); a pointer→id index makes the node pass a lookup, not a re-scan. Rather than pre-judge payoff, instrument it into the ever-fires ledger (ROI §): report the distribution of handle-table sizes and scan depth across the self-compile + exec corpus, so "collapses to O(1) here but the table was tiny" is distinguished from "the scan was hot" by measurement, not by a guess up front.
  - **Presence-flag / varint the always-8-byte per-node payload.** Each node unconditionally serializes `ival`, `fbits`, `cst` as u64 and `op`/`type_t` as u32 even when zero (the common case for most kinds). A per-node presence bitmask + LEB128 shrinks the node stream, which dominates blob size.
  - **Structurally dedup type records + varint ids/counts.** Distinct `Sym*` with identical role+layout intern to separate handles today (pointer-identity by design); a structural-hash secondary key can collapse them — but ONLY behind a proof that the reader's rebuilt graph is unchanged (identity-vs-structural is the sharp edge). Handle ids/counts are u32 over tiny tables ⇒ LEB128.
  Constraints every proof rides on: any wire change bumps `MCCJIT_INTENT_FORMAT` (currently 8) and stale blobs must fail closed — the magic/format/salt check already enforces this, and the slice-cache salt embeds the version so on-disk caches invalidate cleanly. Round-trip fidelity is the non-negotiable invariant; a size win that perturbs the rebuilt graph is a reject, not a win.

### Multi-threaded optimizer (defer)
The concurrent gate-search scorer is thread-safe and opt-in (`MCC_AST_SEARCH_PTHREADS`): the 64-global scored-path surface is `_Thread_local` via `MCC_OPT_TLS`, TSan-clean (down from ~47k races), byte-identical across runs. `mcc -run`/JIT executes TLS correctly (macOS via a synthesized tlv descriptor + `pthread_key`-backed thunk; Linux via a `static __thread` slab with the LE relocation retargeted). The invariant that must survive any threading: **parallel score → deterministic select (lowest cost, ties→lowest index) → serial emit** on the untouched captured tree; emit stays single-threaded, so byte-golden output is unchanged regardless of scheduling. The dominant race surface is the epoch-keyed analysis arenas (`ast_hash_*`, `ast_du_*`, `ast_memo_*`), not the statically-obvious `ast_cur`/gate flags. Remaining:
- Route the workers through the shared `mccjit_pool` instead of per-search `pthread_create` (avoids thread-churn per function; note `_Thread_local` arenas leak at thread exit, so the pool must reuse a fixed worker set or add explicit per-thread teardown).
- Make the threaded path the default only after Linux + x86_64 CI validates the LE-slab TLS path and the x86_64 trampoline (build-clean but not runtime-exercised on arm64 macOS).
- Wire the capture→score→select funnel stats through the joined workers.
- Portable alternative to `_Thread_local` if TLS cost bites: move the epoch-keyed caches *into* the `AstArena` struct (`a->hash`/`a->du`/`a->memo`) so each trial clone owns its cache — no TLS, portable to every target.

## JIT arm64
- **arm64 mode-6 dispatch for object output — DONE 2026-07-26.** (1) HARDENED the slot dispatch: the address-of-slot load changed from GOT-based (`R_AARCH64_ADR_GOT_PAGE`+`R_AARCH64_LD64_GOT_LO12_NC`, brittle for a local data symbol under external/non-PIC link and the source of the in-memory-re-emit corruption) to the self-contained PC-relative `adrp x16,slot (ADR_PREL_PG_HI21) + add x16,x16,#:lo12:slot (ADD_ABS_LO12_NC) + ldr x16,[x16] + br x16` idiom (same 16-byte footprint, so `aot_base+16` unchanged; `R_AARCH64_ABS64` slot-init kept, becomes RELATIVE under PIE). (2) Enabled mode-6 for OBJECT OUTPUT behind the existing `MCC_JIT_SUBMIT_AOT` env (default unset ⇒ byte-identical plain `-c`), with the `__mccjit_slot_<fn>` slot emitted as a named global so it survives external link; `mccjit_embed_note`/`ast_jit_submit_aot` stay embed-only. (3) DROPPED the `!ast_search_env` term from the arm64 outer guard — the PCREL hardening removes the re-emit corruption it guarded against. Validated native aarch64 (`cmake-arm64docker` + amalgamation): encodings assembler-verified; default `-c` byte-identical to pre-change HEAD (md5) incl. the 2.37 MB self-compiled amalgamation, no dispatch stub without the env; `MCC_JIT_SUBMIT_AOT=1 -c` linked with gcc `-no-pie` AND `-pie` runs correct vs gcc across a 15-program battery (recursion/mutual-recursion/loops/multi-arg); `MCC_JIT=1 -O4 -run` now emits `mccjit-boot` mode-6 lines (0 before the guard drop) and is correct 15/15; `jit/selftest-*` 48/48; all 5 arches build; host arm64-macOS cmake links. FOLLOW-UP (opt-in edge only): the object-output slot is a GLOBAL symbol, so two TUs each with a same-named `static` function would collide at external link — make the slot `STB_LOCAL` (or per-TU-unique) for statics, or resolve slots by section+offset instead of name.
- **arm64 in-place trampoline patch row — DONE 2026-07-26.** Added the arm64 `"inplace-tramp"` row to the hot-patch strategy registry `mccjit_patch_reg[]` (mccjit_embed.c), the arm64 peer of the x86_64 `movabs rax,imm; jmp rax` in-place patcher. `make` emits `movz/movk x16,#target (×4); br x16` on an RW page then flips it RX; `swap` genuinely rewrites the 4 MOVZ/MOVK instruction immediates IN PLACE (mprotect RW → rewrite → mprotect RX, whose RX transition runs `host_icache_flush`) — a true code patch, distinct from `ptr-swap-slot`'s data-slot store. All new code is under `#if defined(MCCJIT_ARM64)` so x86_64/i386 objects are byte-identical. Encodings assembler-verified (`objdump`). Validated native aarch64 (`cmake-arm64docker`): `jit/selftest-patch` now prints `inplace-tramp dispatch=11 redirect=22 OK` and benchmarks 1.65 ns/call (beating ptr-swap-slot's 1.67 by dropping the load indirection); full `jit/selftest-*` 48/48 green; all 5 arches build; host arm64-macOS `cmake` links. NOTE: the 4-instruction MOVZ/MOVK rewrite is non-atomic (correct under the between-calls / swap-under-quiescence model the selftest uses; the atomic single-store redirect remains `ptr-swap-slot`).
- **arm64 W^X port — DONE 2026-07-26 (via mmap-RW→mprotect-RX, not the dual-map/split-page design originally sketched).** Every arm64 executable-memory allocation in `mccjit_embed.c` writes to a `PROT_READ|PROT_WRITE` page, then flips it to RX with `host_runmem_protect(HOST_PROT_RX)` — so a page is never simultaneously writable and executable (genuine W^X). Verified there is NO arm64-reachable `PROT_EXEC|PROT_WRITE` mmap left: `mccjit_probe_exec_mem` (already RW→RX), `mccjit_make_counter_stub`, `mccjit_make_kgc_stub_n`/`_fp`/`_mixed`, and the AAPCS64 `mccjit_mixed_thunk_build` all use the flip; the remaining RWX `mmap`s (~3524/5836/5854/6730/6747) are all inside `#if MCCJIT_X64`/`#elif MCCJIT_I386` guards. `_make_trampoline` is a no-op on arm64 (the `#else` returns `variant`), and arm64 swaps variants by POINTER (QSBR-style), never patching a live page in place — so mprotect-once-RX is correct here and the dual-map's live-write capability is unneeded (the `host_runmem_alloc` MAP_SHARED dual map remains available for the main `mccrun` codegen path and for any future in-place-patch feature; see the trampoline patch-row item). **Validated on NATIVE arm64 (not qemu): all 48 `jit/selftest-*` ctest cases pass in `cmake-arm64docker`, including the previously-"zero qemu coverage, highest-risk" `selftest-mixed` (the `mccjit_make_kgc_stub_mixed` + AAPCS64 forwarding thunk), plus `-kgc`/`-nearmatch`/`-fparg`/`-patch`/`-qsbr`; and a live `MCC_JIT=1 -run` hot loop boots a variant (`mccjit-boot[sync] ... route=direct`) and returns correct results.**
- **arm64 `--embed-jit` bake follow-ups (surfaced 2026-07-27 proving the native self-host JIT≡AOT gate — see P0 step 2).** Baking a self-JITing arm64 `mcc` from a stock debian gcc needed two host-toolchain work-arounds; each is a real, small mcc gap worth closing so the bake works with a stock local-exec engine:
  - **(a) Teach mcc's arm64 linker `R_AARCH64_TLSLE_ADD_TPREL_LO12_NC` (551) — DONE 2026-07-27.** gcc compiles the engine's `_Thread_local` optimizer arenas to that local-exec TLS reloc (the default ADD form is `HI12` + `LO12_NC`); `arm64-link.c` handled the checked `_LO12` and the TLSDESC relocs but not gcc's `_NC` variant, so linking a stock local-exec engine failed `Unknown relocation type for got: 551` unless it was forced `-fPIC -mtls-dialect=desc`. Fix (`arm64-link.c` `code_reloc`/`gotplt_entry_type`/apply-switch, + the disasm sizing in `arm64-dis.c`): treat `_NC` identically to `_LO12` — the apply path only masks the low 12 bits (`imm & 0xfff`), which already IS the checked variant's value, so no separate overflow check is needed. Purely additive: mcc's own codegen emits `_LO12` (550), never 551, so mcc output is byte-identical by construction. Verified native arm64: a gcc `-O2` `_Thread_local` object (relocs `HI12` + `LO12_NC`) now links via mcc and runs matching the all-gcc reference (`tls 105 112`); pre-fix the same link errored `Unknown relocation type for got: 551`. All-mcc TLS + `-run` + all-5-arch build unaffected. **So the `-mtls-dialect=desc` embed-bake workaround is no longer needed** (only the `-mno-outline-atomics` one — follow-up (b) — remains).
  - **(b) ELF-Linux `MCC_EMBED_JIT_GCC_LIBDIR` bake — DONE 2026-07-27.** The engine blob is linked LAST, after all cmd-line libs, so an engine built by a stock gcc with default `-moutline-atomics` couldn't resolve libgcc helpers (`__aarch64_ldadd8_acq_rel`, `__divtf3`, …) → the bake needed `-mno-outline-atomics`. Added the ELF peer of the WIN32 mingw libgcc bake: CMake discovers the host libgcc dir (`gcc -print-libgcc-file-name`) and bakes `MCC_EMBED_JIT_GCC_LIBDIR`; `mcc_add_jit_engine_embedded` (libmcc.c) adds it to the search path and links libgcc (or disk-probed compiler-rt builtins for a clang host) AFTER the blob. Default-safe (inert unless baked); WIN32 branch untouched. Verified native arm64: an engine-with-default-outline-atomics bake now links a 1.1 MB standalone exe (`acc=998508278240`, `MCC_JIT=0`≡`=1`); WITHOUT the define the same bake errors `unresolved reference to '__aarch64_ldadd8_acq_rel'`. So the `-mno-outline-atomics` workaround is retired; with (a) also done, the native arm64 `--embed-jit` self-host bake needs NO workarounds.

## Windows / PE (non-JIT)
Follow-ups from the 2026-07-26 optimizer work, which was developed and validated entirely on Linux/x86_64 plus qemu cross runs. None of it is flipped on, so none of this is shipping-critical — but each item is a place a Windows CI cell will go red or, worse, silently skip.
- **Regenerate `tests/ast/verify-baseline/x86_64-win32.txt` for `tests/exec/statements/chained_assign.c` — DONE 2026-07-27 (commit 62a52a26).** Reproduced the mingw/x86_64 `ast-verify-ratchet` failure locally (220 gaps vs 214) and regenerated the win32 baseline on a mingw/x86_64 build; the diff is exactly the 6 `chained_assign.c` `unfaithful` entries (the win32 gap set differs from linux's 9). Ratchet passes locally (220==220).
- **Run the new gates and gate COMBINATIONS on Windows.** The 2026-07-26 gates (`MCC_AST_CHAINSTORE`, `MCC_AST_PROMO_INCDEC`, `MCC_AST_IVSR_PTR`, `MCC_AST_REGDISP`) and the new ctest variants `exec-ivsrptr/*` and `exec-chainstore/*` have never run on a PE target. The Win64 ABI differs where it matters most for two of them: a different callee-saved register set and the 32-byte shadow space, and `PROMO_INCDEC` edits the promotion pools while `CHAINSTORE` interacts with them (their combination miscompiled on Linux and needed a dedicated fix). Treat a green single-gate run as insufficient — the combination is what caught the Linux bug.
  - **UPDATE 2026-07-27 — `exec-chainstore/*` now runs on the msvc/arm64 CI cell and caught TWO real arm64-only miscompiles** under `-O2 CHAINSTORE=1 PROMO_INCDEC=1 OPASSIGN=1` (x86_64 msvc + mingw pass; reproduced on arm64-linux in Docker/qemu and with an arm64 cross-compiler, so they are arm64-codegen bugs, not win32-ABI-specific):
    1. **`chained_assign`/`chain_reassign` — FIXED (commit ee7a719a).** `a = b = i` with `i` a PROMO_INCDEC-promoted counter: the inner store read `i` straight from its promoted reg (`stur w_i,[b]`) and never materialised it into the register the outer store reads, so the promoted outer target got a stale reg (added `1+i` per iter → 210 instead of 380; x86_64 routes it through the accumulator and was fine). Fix extends the existing chained-store promotion guard (`ast_plan_promotion`) to also decline promoting the chained store's VALUE SOURCE, not just the inner target — under `ast_chainstore_env`, so the default path is byte-identical. Verified: arm64 cross-compiler asm now loads `i` from memory (`2i`); x86_64 rebuilt → 380, no regression. Native-arm64 RUNTIME confirmation DONE 2026-07-27 (below).
    2. **`select_branchless` — FIXED (commit 526f13ea, `gen_cmov` must clobber rf not rt), verified 2026-07-27.** Was an arm64 hang: nested fixed-bound loops promoted counters to callee-saved regs (`x19/x20/x21`) across the `imin()`/`imax()` calls and one got clobbered → infinite loop.
  - **arm64 verification 2026-07-27 (closes the pending runtime confirmation).** Ran under `qemu-aarch64` with the vendored `gentoo-stage3-arm64-glibc` sysroot, using the `cmake-qemu-arm64` cross compiler (optimizer confirmed compiled in — `-O0` vs `-O3` objects differ — since a cross build without it makes these gates silently no-op and any pass meaningless). `select_branchless`: rc=0 `OK` at `-O2` with `CHAINSTORE=1 PROMO_INCDEC=1 OPASSIGN=1` forced AND at `-O4` where all three are default-on — no hang (a 45s hard timeout was in place; the old failure was a 300s stall). `chained_assign`: prints `reassign 380`, not the buggy 210, and the whole line matches the gcc reference (`expr 230 / mixed 162.000000 / reassign 380`). Faithful `qemu-arm64-{glibc,musl}-exec` cell: 5/5. Gates are read at COMPILE time by `ast_configure`, so they must be set on the mcc invocation, not on the qemu run.
- ~~**Verify `-fc99-inline-body`'s weak out-of-line body on PE.**~~ — **DONE 2026-07-27** (commit "portable two-TU -fc99-inline-body weak-body gate"). Confirmed on Windows/PE: two TUs both taking `&plain_inline` link (weak collapse) and print `9 17 35`; WITHOUT the flag the link fails `unresolved reference to add3`, so the flag is load-bearing. Promoted to the portable `c99-inline-body-2tu` ctest (self-checks the values AND that both TUs' `&add3` resolve to the one collapsed weak copy; runs on every platform via the mccexe harness + emulator; added a reusable `--cflags` option to the mccexe suite). The Mach-O peer item is still open.
- ~~**Make `tools/runtime-bench.py` skip cleanly on Windows rather than fail.**~~ — **VERIFIED 2026-07-27, no change needed.** `runtime-bench-check` (the `--check-only` output-vs-reference gate, registered unconditionally) RUNS AND PASSES on Windows/PE (14.5s locally) — the `perf` insn columns degrade cleanly (Linux-only, absent → skipped) and the correctness comparison still runs. The wall-clock `runtime-bench-gatewin` is `mcc_skip_test`-marked off WIN32 (CMakeLists ~5115). No hard failure.
- ~~**`selfhost-cross-native.sh` / `selfhost-riscv64-docker.sh` are POSIX-only.**~~ — **VERIFIED 2026-07-27, no change needed.** Both `selfhost-riscv64-docker` and `selfhost-arm64-native` are registered inside `if(UNIX)` (CMakeLists ~5126), so they are **not added at all on WIN32** — they cannot error there. The cross self-host gate is simply unavailable on Windows (by construction), as intended.

## JIT Windows / PE
- ~~**P0 step 4 — i386-PE stub-tail (`MCC_JIT_I386_STUBS`).**~~ **FLIPPED default-ON 2026-07-27** (`mccjit_i386_stubs_enabled` returns 1 when the env is unset; `MCC_JIT_I386_STUBS=0` restores the historical bail-to-NULL). i386-gated so x86_64/arm64/riscv64 are byte-identical (native re-verified: 48/48 jit selftests + 377/377 exec/compile/fixpoint green). Verified post-flip: default (env unset) runs the stub tail and the `mixed` skip-gated selftest now PASSES (was SKIP), `=0` restores SKIP, and runtime-JIT parity MCC_JIT=1≡MCC_JIT=0 still byte-identical at the default. The full soak that justified the flip (`tools/i386win32-soak.sh`, runs locally on WoW64 — no docker/qemu):
  - **Toolchain:** installed winlibs `i686-w64-mingw32` GCC 16.1.0 (ucrt, real libgcc — `___chkstk_ms`/`___udivdi3`) at `C:/Users/llg/opt/mingw32`, resolving the "no i386 libgcc" blocker. Built the i386-win32 cross compiler + runtime via the `cross` preset (`cmake-cross/mcc-i386-win32.exe` + `i386-win32-libmccrt.a`) and self-compiled `src/mcc.c` to a real i386 `mcc.exe`.
  - **AOT self-host:** 3-stage FIXPOINT byte-identical `o1==o2==o3` (host-cross stage1 == i386-native stage2 == stage3) on WoW64.
  - **Runtime-JIT parity (the exact flip property):** `MCC_JIT=1` with `MCC_JIT_I386_STUBS=1` ≡ `MCC_JIT=0` **byte-identical**, both a direct `-c` compile (hot-threshold=1 forcing promotion) AND the stronger `-run src/mcc.c` in-memory self-host (the JIT-optimized i386 mcc compiles a workload byte-identical to AOT).
  - **Stub-tail selftests:** **36/37 pass with `MCC_JIT_I386_STUBS=1`** (1 by-design skip: `evalgate` is x86_64-only). All THREE stub types the flip un-gates are exercised faithful — `kgc` (KGC verify-stub; baseline/variant stub addresses differ gate-on vs -off, proving the hand-emitted i386 stub is really invoked), `fparg` (`fp-stub-swapped=1`), and `mixed` (`stub-exec faithful`/`divergent`/`FP-ret` all OK). Confirmed real, not an mcc-i386-codegen artifact: a reference-winlibs-gcc-built harness gives identical results.
  - **Differential vs gcc+clang** (both available for i386: winlibs gcc + llvm clang, all emitting i386 PE): mcc agrees with the gcc/clang consensus on every well-defined program. The only divergences are the x86_64-tuned generator emitting `<< (x & 63)` shift masks that are UB on i386's 32-bit `long` — present at mcc `-O0` (AOT, not stub-related) and exactly what the real fuzzer's UBSan gate excludes.
  - **Fixes landed for the soak:** (a) i386 MEMORY recompile now resolves codegen-emitted runtime helpers (`__fixdfdi` etc.) from libmccrt — `#if MCCJIT_I386` `js->nostdlib=0` in `mccjit_recompile_common`, byte-identical on x86_64/arm64/riscv64 by construction and only reached with the gate on; this fixed `mixed` and `stage2` (their earlier "baseline recompile NULL"/callee-unbound were runtime-not-found, not stub bugs, since the cross compiler bakes a nonexistent `MCC_CONFIG_MCCDIR`). (b) multiple-`-B` accumulation (see the `-B` item) so the runtime can be staged.
  **Differential-fuzz — now i386-clean and wired into the soak (2026-07-27).** The generator (`tests/fuzz/gen.h`) previously emitted `<< (x & 63)` shift masks assuming 64-bit `long`, which is UB on i386's 32-bit `long` and caused false divergences (the earlier seed-19 mismatch reproduced at mcc `-O0` AOT — i.e. UB, not a stub bug). Fixed: the mask is now `& (8UL*sizeof(unsigned long)-1UL)`, computed at runtime from the target's actual `long` width — `& 63` on LP64 (x86_64-Linux unchanged, byte-identical), `& 31` on ILP32 (i386/Windows), well-defined and identical across mcc and every reference compiler on the same ISA. `tools/i386win32-soak.sh` now runs a differential-fuzz phase (mcc-i386 vs a gcc-O0/gcc-O2 consensus + a distinct i686 clang when present); with the fix, 0 divergences and 0 UB-skips across the seed range (was ~40% inconclusive before). Still open (separate toolchain item, not the stub flip): the per-arch baked `MCC_EMBED_JIT_BLOB` for i386-PE (standalone `--embed-jit` exe with the engine baked).
- **PE runtime-JIT self-host x86_64 residual crash — P0 step 5.** (`mcc.exe --jit -O4 -run src/mcc.c` faults `0xC0000005`) — OPEN. The icache hypothesis is disproven (adding `host_icache_flush` on Windows/x64 + flushing the published page before the release store didn't fix it — the hardening is KEPT because it's correct per MSDN for generated code and arm64-Windows needs it, but it isn't the root cause). Not locally reproducible with the exact CI toolchain (winlibs gcc 16.1 ucrt x86_64) even under forced raw-variant swapping. The fault is almost certainly in the x86_64-only swapped-variant path (i386 never swaps a variant; x86_64 swaps many): the KGC-stub ABI (`mccjit_make_kgc_stub_n`/`_fp`/`_mixed` register marshalling) or a benchmark/search-worker timing/ASLR interaction only the CI CPUs hit. The gate is re-skipped on all Windows (`tools/selfhost-jit.py` self-skip + CMake `NOT WIN32`). Next: get a CI stack trace (temporarily un-skip + `MCC_JIT_VERBOSE`/`MCC_JIT_PERF_MAP` on a branch, or a crash-dump upload), then disasm the swapped variant/stub at the fault. **NEW EVIDENCE 2026-07-27 — the crash is toolchain-specific, NOT universal.** On the **mstorsjo llvm-mingw** host a `--embed-jit`-baked `mcc-jit.exe` (full `src/mcc.c` self-host bake, 5.67 MB) self-compiles the whole amalgamation under `MCC_JIT=1` **crash-free** and byte-identical to `MCC_JIT=0` (2472357-byte object, md5 `d3c01df6…`) — so the x86_64-win32 runtime-JIT swapped-variant path is CORRECT on llvm-mingw. That strongly localises the `0xC0000005` to the **winlibs-gcc-16.1-ucrt** toolchain specifically (CRT/TLS/ASLR layout, or a winlibs-gcc codegen quirk in the baked engine), narrowing the CI-trace hunt: diff the winlibs vs llvm-mingw baked engine at the swap site.
- arm64-PE runtime-JIT native-fault subset — needs arm64-Windows HW: `RtlAddFunctionTable`/SEH unwind walking JIT frames, `IC IVAU`/`DSB`/`ISB` icache coherence, and the frameless-leaf return corruption itself (all spin-not-fault under qemu/wine x86-TSO). Codegen/logic is validated under wine (`tools/arm64pe-wine-docker.sh`, debian-bookworm wine-8.0 arm64) — the mode-6 slot-swap + AAPCS64 mixed-arg thunk run correct on the native arm64-PE loader.
- MSVC-arm64 JIT-exec miscompile (wild-jump) — needs arm64-Windows HW (same x86-TSO masking).
- ~~Port `mccjit_make_kgc_stub_mixed` to the Win64 positional ABI~~ — **DONE** (`mccjit_embed.c` `#if MCC_HOST_WIN32`; `jit/selftest-mixed` runs+passes on x86_64-win32, verified 2026-07-27; the `return NULL` at ~4813 is the generic non-x86/arm fallback, not WIN32).
- ~~Enable the `--embed-jit` standalone-exe embed-blob on WIN32~~ — **COFF reader + CMake blob ungating DONE**; the ELF-only-linker claim is stale (`coff_load_object_file` reads COFF `.o`/`.a`). The real residual blocker was native-TLS SECREL relocs in the engine object — **FIXED 2026-07-27** (see campaign item 1). Full end-to-end now only needs the host CC's runtime libs to resolve (CI winlibs gcc: yes; mstorsjo-llvm: no libgcc); the à la carte loader non-termination that also blocked this is FIXED (see the `mcc_load_alacarte` note below).
- ~~Port a real Windows JIT runtime for `MCC_EMBED_JIT`~~ — **DONE** (`mccjit_win32.h` shims mmap/VirtualProtect/FlushInstructionCache/SRWLock/threads; `host_runmem_*` WIN32 paths; fork/atfork correctly no-op). All 48 `jit/selftest-*` green on mingw-Windows x86_64, verified 2026-07-27.
- ~~**Multi-instance `-B` support.**~~ **DONE 2026-07-27.** mcc's `-B` was single-instance / last-wins (`mcc_set_lib_path` *replaces* `mcc_lib_path`), silently dropping earlier `-B` — yet `tools/selfhost-jit.py`, `tests/ci/regression_o4_aot_jit.sh`, and the CMake JIT self-host (`CMakeLists.txt`/2805) all pass `-B<src> -B<build>` expecting both searched. gcc AND clang both accumulate (verified `-B/a -B/b -print-search-dirs` lists both). Fixed in `libmcc.c` `MCC_OPTION_B`: the newest `-B` stays the `{B}` template base (so last-wins output is byte-identical), and each previous base is demoted to explicit search prefixes — `<dir>` + `<dir>/lib` into library_paths (covering ELF `{B}` and PE `{B}/lib`) and `<dir>/include` (+ `/winapi`) into sysinclude_paths. Additive for single-`-B` (untouched). Validated: 605 (cli/compile/exec) + 305 (jit/embed/selfhost/abitest/diff) + 68 (fixpoint/self) native ctests green; multi-`-B` split-staging (runtime in one dir, win32 CRT in another) links+runs on i386. Note the `jit_selftest_*` harness mains still keep only the last `-B`, but the soak stages the runtime under one `{B}/lib` so that path isn't needed.
- Standalone `--embed-jit` blob for i386-PE / arm64-PE: the compiler machinery + COFF engine-symbol resolution landed (i386 embed-slot codegen, i386-PE COFF `_`-undecoration, cross-embed link stubs, short-import COFF reader for MSVC/LLVM/SDK libs). What remains is toolchain-gated, not a code bug: the per-arch baked `MCC_EMBED_JIT_BLOB` needs the mcc engine archive compiled for i386/arm64, and this host's mingw is x86_64-only (no i386 libgcc, no arm64 compiler), so a full end-to-end cross `--embed-jit` exe can't be produced here.

## AOT foundations
### FP/compute codegen gap
mcc is ~2–3× slower than gcc/clang on compute-bound loops (nbody 3.2×, spectral 3×, mandelbrot 2.7×, fannkuch 2.2×; competitive only on memory/malloc-bound code). `-O60` and `-O60 --embed-jit` don't close it (JIT never amortizes). Root-caused by disasm of nbody `advance()` (mcc 184 insns vs gcc 104) to three backend gaps. `advance()` is now replayable (see `MCC_AST_OPASSIGN` under Tests/infra) so all three are directly actionable on it:

**⚠️ MEASURED 2026-07-26 (native x86_64 docker, nbody 20M steps, best-of-5; container timing noise ~±13% so trust the DETERMINISTIC `advance`-loop stack-memop count over wall-clock for small deltas) — the bottleneck is FP REGISTER SPILLING.** gcc-O2 **2038ms / 0 stack memops** in `advance`; mcc-O4 baseline **4402ms / 88 memops** (2.16×; mcc spills every FP value every iteration, gcc keeps them in registers). Per-gate:
  - **`MATH_INLINE_PREPASS + PROMO_LEAF_CALLEE` (+ OPASSIGN for replayability) is a REAL WIN: 3797ms / 60 memops (−14% wall-clock, −32% spills), correct.** Inlining sqrt makes `advance` a leaf, so the GP callee-saved promotion pool kicks in and holds the pointers/indices. (A prior loop called this "neutral" — that was timing noise; the deterministic memop drop 88→60 is the real signal. These two gates are the priority for the default-on flip after the M8 soak.)
  - `MCC_AST_IVSR_PTR` (pointer LSR) alone is wall-clock-neutral — it drops the index `imul`s (12→1) but the imuls were never the bottleneck. **`MATH_PREP + IVSR_PTR` REGRESSES ~27% (memops 88→202)**: once sqrt is inlined IVSR_PTR fires more and materializes the strength-reduced pointers into `ltemp` slots **poisoned from promotion** (mccast.c) ⇒ they spill to MEMORY. Same trap as FP-pool/DIVMAGIC-b: **AST materialization to an unpromotable `ltemp` is net-negative in spill-heavy loops.** `IVSR_PTR` stays default-off and must NOT flip alongside `MATH_PREP` until its temps are register-promotable.
  - **Widening the leaf FP promo pool (xmm 2→6, `MCC_AST_PROMO_LEAF_XMM`) — DONE, reduces spills 60→49.** The earlier "get_reg exhaustion" hypothesis was WRONG: root-caused (via nbody disasm) to a promotion-correctness bug in the DESTRUCTIVE in-place FP ops — `sqrtsd r,r` / `roundsd r,r` clobber their source, and when the source is a promoted/pinned reg still live (`d2 * sqrt(d2)` needs d2 after sqrt(d2)) it computes garbage (energy 7.6 vs 303503). Fixed by emitting into a fresh scratch reg when the source is pinned (see the x86_64 gen_sqrt/gen_round fix; byte-identical by default, gen_fabs's spill/AND/reload path was already safe). With that fix the FP-pool is correct across nbody + a 30-config high-FP-pressure/nested fuzz vs gcc at O0/O2/O4, byte-identical gate-off. `get_reg` never actually returned −1 (xmm0/1 stay spillable) — no exhaustion.
  So the path is: (1) DONE — MATH_PREP+PROMO_LEAF (88→60, −14%); (2) DONE — gen_sqrt/round promoted-reg fix + LEAF_XMM pool (60→49); (3) NEXT — the residual 49 spills are the **8-xmm register-count wall**, NOT a search/promotion-tuning issue: measured 2026-07-26 that advance spills are identical at -O2/-O3/-O4 (all = 49, with all 8 of xmm0–7 in use at every level), so the -O4 search does NOT drop promotion — promotion is already maxed at the 8 modeled xmm and advance simply has >8 live FP values. Closing it requires MORE FP registers: extend the x86_64 backend to model xmm8–15 (the real lever). That needs a REX prefix (REX.R for dst≥8, REX.B for src/rm≥8) threaded through EVERY SSE emit site in x86_64-gen.c (load/store/mov/mulsd/addsd/…/gen_sqrt/gen_round/gen_copysign) plus reg_classes/MCC_NB_REGS/REG_VALUE — pervasive but mechanical; gcc uses all 16 xmm to hit 0 spills. Then auto-vectorization (below). **Scoped 2026-07-26 — two keys make this SAFE-by-construction:** (1) the existing `orex(ll, r, r2, b)` helper emits a REX byte only when `REX_BASE(r|r2)` is set, and xmm0–7 are ids 16–23 (`REX_BASE`=0), so **threading `orex` through the SSE sites is byte-identical for all current xmm0–7 codegen** — REX bytes appear only for xmm8–15 (ids 24–31, `REX_BASE`=1). (2) To keep it byte-identical by DEFAULT, xmm8–15 must NOT be handed out as general FP scratch: give them a distinct reg class (e.g. `MCC_RC_FLOAT_HI`) so `get_reg(MCC_RC_FLOAT)` still only returns xmm0–7, and update the "is this an FP reg" checks (load/store/is_float) to accept `RC_FLOAT|RC_FLOAT_HI`; xmm8–15 are then reachable ONLY by pinning them via the gated `MCC_AST_PROMO_LEAF_XMM` pool (widen it to include the xmm8–15 ids). Validate: byte-identical default + exhaustive FP fuzz (every SSE op — mul/add/sub/div/sqrt/round/cvt/mov/min/max — with xmm8–15 operands) vs gcc, since any un-threaded site silently mis-encodes an xmm8–15 value as xmm0–7. A focused implementation loop. **IN PROGRESS 2026-07-26 — a PREREQUISITE was found that the "mechanical" framing above missed: the x86_64 register-id space is FULL.** `REX_BASE`/`REG_VALUE` force xmm8–15 onto ids 24–31 (bit3 set, low3 = xmm number; the only other candidates are 8–15 = r8–r15 and 40–47 = out of range), but id 24 is already `MCC_TREG_ST0`, and there is no free slot to move it to: on x86_64 `MCC_TREG_MEM = 0x20` is OR'ed into integer reg ids so 0x20–0x2f is the indirect-through-reg range and `VT_CONST` starts at 0x30, leaving exactly 32 id slots (0–31) for 16 GP + 16 XMM + 1 ST0 = 33. Every one of ids 0–15 encodes a real GP register in modrm (incl. rsp id 4, used at x86_64-gen.c, and rbx/r12–r15 which the promotion pool pins), and relocating ST0 into the 0–15 numeric range would break the many `r < MCC_TREG_XMM0` "is an integer register" tests. **Plan (Option A, chosen): widen `VT_VALMASK` 0x3f→0x7f** (bits 0x40/0x80 are unused in the `r` field — `VT_LVAL` is 0x100 — and `VT_CONST`/`VT_LLOCAL`/`VT_LOCAL`/`VT_CMP`/`VT_JMP`/`VT_JMPI` stay at 0x30–0x35, so arm64 (NB_REGS 38) / riscv64 (30) / arm / i386 are unaffected), **move x86_64 `MCC_TREG_MEM` 0x20→0x40** (i386 keeps 0x20; its NB_REGS is 5), which frees ids 0x20–0x2f: put xmm8–15 at 24–31 and `MCC_TREG_ST0` at 32, `MCC_NB_REGS` 25→33. One semantic fixup this forces: `x86_64-gen.c` `indirect = (r & VT_VALMASK) >= MCC_TREG_MEM` is a threshold test that only ever means "does r carry the MEM bit" at that point (VT_CONST/VT_LOCAL are handled in earlier branches), so it must become the value-agnostic `(r & MCC_TREG_MEM)`; audit ``'s rsp use and every other `>= MCC_TREG_MEM` / `& MCC_TREG_MEM` site the same way. **Step 1 (id-space only) DONE 2026-07-26** — `VT_VALMASK` 0x3f→0x7f, x86_64 `MCC_TREG_MEM` 0x20→0x40, `MCC_TREG_XMM8..15` = 24..31, `MCC_TREG_ST0` 24→32, `MCC_NB_REGS` 25→33 (reg_classes gains 8 zero entries so xmm8–15 are still un-allocatable), plus the two forced fixups: `orex` must NOT zero a MEM-flagged operand (`>= VT_CONST` is now true for 0x40|reg, which would have dropped REX.B ⇒ miscompile) so its guard gained `&& !(r & MCC_TREG_MEM)`, and the `indirect` threshold became `(r & MCC_TREG_MEM) != 0`. Validated: 486/486 object comparisons byte-identical over `tests/exec`+`tests/behavior` at -O0/-O2, the 1.9 MB self-compiled amalgamation byte-identical at -O0/-O1/-O2/-O3, x87 long-double (the moved ST0 id) matches gcc at -O0..-O3, all 5 arches build. **Step 2 (make xmm8–15 reachable via the promotion pool) DONE 2026-07-26** — `reg_classes[24..31]` gain unique `MCC_RC_XMM8..15` bits and NO `MCC_RC_FLOAT` (mirroring how xmm6/7 are already excluded from the scratch class), so `get_reg(MCC_RC_FLOAT)` still only returns xmm0–7 and default codegen is untouched; the `MCC_AST_PROMO_LEAF_XMM` leaf pool grew 6→14 entries with xmm8–15 first (they never contend with scratch), then the pre-existing xmm6,7,5,4,3,2. A new `sse_rex(reg, rm)` helper emits the REX byte between the mandatory SSE prefix and the `0f` escape; it was needed at exactly the sites that can see a pinned high register: `load()`'s reg-reg and ST0 moves (the FLOAT/DOUBLE memory path just needed its premature `r = REG_VALUE(r)` truncation dropped so the existing `orex` sees the full id), `gen_opf`'s pinned-source unpin `movsd`, and the `ucomis*`/`addsd`-family reg-reg forms (the memory forms already went through `orex`). `gen_sqrt`/`gen_round`/`gen_copysign` need nothing — they `gv(MCC_RC_FLOAT)` first, which by construction normalizes into xmm0–7. Validated: gate-off byte-identical (492 exec+behavior objects at -O0/-O2 + the self-compiled amalgamation at -O0/-O2); gate-on a 12-live-double leaf matches gcc at -O0/-O1/-O2/-O4 with xmm8–15 in use, and a 40-seed randomized FP fuzz matches gcc 80/80 at -O0/-O2 and 8/8 at -O4. **Step 3 — NEGATIVE RESULT, the scoping above was wrong about where the win comes from:** widening the *promotion pool* does NOT reduce nbody `advance()` spills at all (18 stack memops with the pool off, with the old xmm2–7 pool, and with the new xmm8–15 pool; zero xmm8–15 uses in `advance`). Promotion only pins whole *locals*, and `advance`'s FP pressure lives in expression *temporaries* allocated by `get_reg(MCC_RC_FLOAT)` — so the spills are the backend register allocator running out of the 8 modeled scratch xmm, exactly as measured, and the pool-only reachability that makes step 2 safe-by-construction is also what makes it useless for this workload. Closing the gap needs xmm8–15 in `MCC_RC_FLOAT` itself (a new gate; `reg_classes` is `const` per-arch so the gate has to either drop the `const` and patch the 8 entries at init or teach `get_reg` to treat the HI class as FLOAT when set), and that exposes EVERY SSE emit site to a high register — so the full `orex`/`sse_rex` thread the original scoping described is still required: `gen_sqrt`/`gen_round`/`gen_copysign`/the `gen_cvt_*` conversions/the x87↔SSE ST0 paths/`gfunc_call` arg setup, plus the `MCC_RC_FLOAT`-is-xmm0–7 assumptions asserted in comments at those sites. Validate that step with an exhaustive per-SSE-op fuzz (mul/add/sub/div/sqrt/round/cvt/mov/min/max with xmm8–15 operands) since any un-threaded site silently mis-encodes a high register as its low twin — the exact failure this step already hit once (`gen_opf`'s unpin move read xmm15 as xmm7). **Step 3 DONE 2026-07-26** — REX threaded through the remaining SSE sites (`gen_sqrt`, `gen_round`, `gen_copysign`, `gen_cvt_itof`/`_ftof`/`_ftoi`, the `gfunc_call` movq-to-stack; `gen_cvt_ftoi`'s `orex` also had its rm/reg operands swapped — harmless while both were <8, wrong the moment the xmm source can be high), and `MCC_AST_XMM_HI` (default OFF below `-O4`) ORs `MCC_RC_FLOAT` into `reg_classes[xmm8..15]` at env-init so `get_reg` allocates all 16. `reg_classes` lost its `const` on all five backends to allow the patch. Gate-off byte-identical (496 exec+behavior objects at -O0/-O2 + the self-compiled amalgamation, all 5 arches build); gate-on a 45-seed fuzz covering mul/add/sub/div/sqrt/fabs/floor/ceil/trunc/rint/nearbyint/int↔fp/long-double round-trips matches gcc 90/90 at -O0/-O2.

**Where the FP work actually has to go next (measured 2026-07-26, supersedes the "8-xmm register-count wall" diagnosis above).** With the gates on, `advance()` at **-O2 has 7 stack memops** (FP promotion live, 29 xmm8–15 references) but at **-O4 has 18** (zero promotion, zero high registers) for a nearly identical instruction count (238 vs 232). Same shape on a 12-live-double leaf: 13 memops at -O2 vs 50 at -O4. So the -O4 search DROPS promotion — the static `ast_cost_score` (`nodes×(maxdepth+1)×(calls+1)`) can't see spills, rates the two orders as equivalent, and picks the un-promoted one. Adding FP registers does not fix that; `MCC_AST_XMM_HI` changes *which* registers are used at -O2/-O3 but not *how many* memops there are. This is the same "eligible-but-search-doesn't-select" mechanism already named under **Strategy scheduler** and the ROI item ("static cost benefit leaves strength-reduction-class passes at benefit 0") — the FP/compute gap is now a scheduler problem, not a backend-encoding one. The register work above stays useful (it is the prerequisite that makes >8 pinned FP values *possible*), but the next measurable win is making the -O4 search preserve promotion.

**A pre-existing miscompile this campaign uncovered — FIXED.** `gen_cvt_ftof` converts float↔double IN PLACE (`unpcklps r,r; cvtps2pd r,r`), so when the source is a pinned/promoted register still live the conversion destroys it — the same destructive-op-on-a-promoted-reg class already fixed for `gen_sqrt`/`gen_round`, which `gen_cvt_ftof` was simply missed by. Reachable on HEAD with `MCC_AST_PROMO_LEAF_XMM=1` alone at -O2/-O3 (an 11-line leaf: 13 float locals, one `(float)((double)a*3.0 - (double)b)` in a loop, returned 4537.88867 instead of -16417.1387). Fixed by a shared `sse_unpin_src()` that moves the source to a fresh scratch first. **Process lesson: the `MCC_AST_PROMO_LEAF_XMM` soak recorded above as clean ("correct across nbody + a 30-config high-FP-pressure/nested fuzz") did not include float↔double conversions — any future destructive-op audit must enumerate every in-place SSE form (`unpcklps`/`cvtps2pd`/`cvtpd2ps` as well as `sqrtsd`/`roundsd`), not just the math builtins.** LSR (`IVSR_PTR`) still needs register-promotable temps before it helps. Follow-ups: the M8 soak + default-on flip for `MCC_AST_PROMO_LEAF_XMM`. (The destructive-op-clobber fix has now been applied DEFENSIVELY to arm64 gen_fabs/gen_sqrt/gen_round and riscv64 gen_fabs/gen_sqrt too — byte-identical by default there since their FP promotion doesn't yet trigger it, but it hardens them for when a wider FP pool lands.)

- **`MCC_AST_IVSR_PTR` legality review DONE 2026-07-26 — both miscompiles have ONE root cause, now fixed.** The second bug (a row pointer taken inside the loop, `int *r = g2[i]; s += r[i] + r[0];`, reading out of bounds at -O2) is not a separate mechanism after all. **`ast_licm_operands_ok` delegates "is this a local?" to `ast_cprop_is_local`, which is a CONST-PROPAGATION helper and therefore also demands an INTEGER type (`ast_ident_intt`). A POINTER-typed local base is silently accepted as loop-invariant even when the loop body assigns it every iteration** — instrumentation confirmed the base Ref had `op=0x132` (VT_LOCAL|VT_LVAL, no VT_SYM) and `ast_licm_written(loop, -24) == 1`, while `ast_cprop_is_local` returned 0 purely on the type test. The `a[i][i]` case is the same hole reached differently: the pass's own materialised pointer is also a pointer-typed local written in the increment block. Fix: `ast_ivsr_ptr_base_varies` asks the question directly — any `AST_Ref` to a local (VT_LOCAL|VT_LVAL, no VT_SYM, ANY type) that `ast_licm_written` reports written in the loop makes the base varying, plus the IV itself — replacing the two point guards with one rule. **Audited 2026-07-26 whether the same hole reaches the other `ast_licm_operands_ok` callers, including the default-ON `MCC_AST_LICM_TEMP` — it does NOT, and here is why, so nobody re-runs this.** Every other caller pairs the check with `ast_cse_regpure`, and `ast_cse_regpure_compute` rejects an `AST_Ref` to a local unless `ast_ident_intt(t)` — i.e. a pointer- or float-typed local operand is already excluded there. Confirmed at each site: `ast_ltemp_scan` (LICM_TEMP) and the mul-based `ast_ivsr_cofactor` both require regpure on the candidate; the PRE site tests `ast_cse_regpure(a, e)` immediately above; and `ast_licm_at_loop` consumes only CSE-table entries, whose insertion is itself gated on `ast_cse_regpure(a, val)`. **`MCC_AST_IVSR_PTR` was the sole caller that deliberately drops regpure** (its own comment says so — a pointer-index address is not register-pure), which is exactly why it alone was exposed. Empirically confirmed with a pointer-arithmetic battery (row pointers from a 2-D array, `p - q` differences, a pointer advanced in the body) matching gcc at -O1/-O2/-O4 under LICM_TEMP alone, LICM_TEMP+IVSR+PRE, and LICM_TEMP+IVSR_PTR+OPASSIGN. **`ast_licm_operands_ok` has nonetheless been hardened** to recognise a local by its Ref form rather than through `ast_cprop_is_local`, so the next caller that drops regpure does not inherit the trap — verified byte-identical (506 exec+behavior objects at -O0/-O2 and the self-compiled amalgamation at -O0/-O2/-O3), which is the expected result given the above and is the evidence that the audit's reasoning is right.
- **`exec-ivsrptr` corpus variant ADDED 2026-07-26** (mirrors `exec-vlat`/`exec-narrowfix`): the whole exec corpus at -O2 with `MCC_AST_OPASSIGN=1;MCC_AST_IVSR_PTR=1`, 298 cases. **Verified it is real coverage, not a no-op — reverting the guard makes `exec-ivsrptr/array_2d_iv` fail and restoring it makes it pass.** This is the coverage whose absence let both bugs sit undetected; IVSR_PTR's own validation was a fixed 6-pattern differential that never crossed the corpus.
- **Regression coverage added 2026-07-26:** `tests/exec/pointers_arrays/array_2d_iv.c` (+ `goldens.h` entry) covers same-IV multi-dimensional indexing — `a[i][i]`, compound-assign `a[i][j] += `, anti-diagonal `a[i][7-i]`, fixed-row/fixed-column, 3-D `a[i][i][i]`, a function-local 2-D array, a reverse-counting diagonal, and the row-pointer walk above. The `ast-verify-ratchet` baseline gained exactly 4 entries for it (3 are the glibc `<stdio.h>` inline family every corpus file pulls in, 1 is the compound-assign loop desyncing without OPASSIGN) — regenerated for `x86_64-linux` only; **the `x86_64-win32` baseline will need the same regen on a mingw host.** Still to add once the second bug is fixed: an `exec-ivsrptr` corpus variant forcing `MCC_AST_OPASSIGN=1;MCC_AST_IVSR_PTR=1` at -O2 (mirroring `exec-vlat`/`exec-narrowfix`), which is the coverage whose absence let both bugs sit undetected — it is deliberately NOT added yet because the open bug would make it red.
- **`MCC_AST_IVSR_PTR` miscompiled `a[i][i]` — FIXED 2026-07-26.** First recorded here as an "OPASSIGN + IVSR_PTR interaction"; that was wrong. **IVSR_PTR miscompiles on its own** — the earlier repro only needed `MCC_AST_OPASSIGN=1` because its compound-assign write loop is what made `main` `ast_replay_ok`, so without it the optimizer never ran and the bug stayed hidden. Reduced repro is a plain read loop, no compound assignment anywhere: after `c[i][j] = i + j`, the loop `for (i…) s = s + c[i][i]` returns exactly half the right answer, because **`c[i][i]` reads `c[i][0]`**. Rows are correct; only the diagonal is wrong, which is why the write loop looked innocent. Root cause: the pass runs to fixpoint, so once the inner `&c + i` has been strength-reduced to a pointer `p`, the OUTER add reappears on the next cycle as `p + i` — structurally a perfect "loop-invariant base + IV" match, except `p` is itself an induction pointer. Hoisting that to the preheader and stepping it gives the row start. `ast_licm_operands_ok` cannot catch it: it asks whether the base's memory operands are WRITTEN inside the loop, and both the IV and `p` are locals updated in the increment block, so the base reads as invariant. Fix: `ast_ivsr_ptr_cofactor` now rejects a base that references the IV local **or any `ast_ltemp` slot this pass already materialised**. Validated: still fires where intended (nbody `advance` index `imul` 3 → 1; a 7-field struct battery 12 → 0), byte-identical gate-off (self-compiled amalgamation at -O0/-O2/-O3), ctest 5591/5591, asttool 747/0, all 5 arches build, a 200-run randomized differential with both gates on is 0-fail, and all 5 runtime-bench kernels verify against gcc with the gates on. **Process note: the first "failure" seen while validating the fix was my own test** — it called a mutating `upd(v,64)` alongside two other calls in one `printf` argument list, where evaluation order is unspecified, so gcc and mcc legitimately disagreed. That is the second time this exact trap has appeared here; sequence mutating calls into separate statements before believing a differential.
- **No auto-vectorization — but it is NOT the main matmul gap. Decomposed 2026-07-26, correcting the claim added with the runtime-bench harness.** On `tests/runtime/matmul.c` (600x8, this host): gcc -O2 vectorized **280 ms**, gcc -O2 `-fno-tree-vectorize` **530 ms**, mcc -O2 **2650 ms**. So of the ~9.5x gap, vectorization is only **1.9x** and mcc-vs-SCALAR-gcc is **5.0x** — the larger factor by far. Disassembly of the inner loop says the same thing: mcc 72 stack memops vs gcc **0** (both vectorized and not), 247 insns vs 159 scalar, 9 `imul` vs 2. So matmul is primarily more evidence for the register-allocation/spill item above, not for vectorization, and the note added earlier calling it "the sharpest evidence for the auto-vectorization item" was wrong. **No existing gate closes it**: PROMOTE, OPASSIGN, PROMO_ARROW, SO_SPILL_SCORE+DEFAULT_SEED all land within noise of 2650 ms (2530-2840), and the spill count does not drop (72 -> 94 with OPASSIGN, 138 with IVSR_PTR). gcc packs x/y/z FP into SIMD (packed `mulpd/addpd/movupd/unpcklpd`, 2 doubles/op); mcc emits 0 packed (all scalar `mulsd/addsd`). Add a vectorization pass (pack adjacent independent FP ops / short vectorizable loops into `xmm` packed ops) — the single biggest FP win, ~2× on the vectorizable parts.
- **No hot-loop register allocation / strength reduction** — the dominant cost (184 vs 104 insns). Sub-gaps in rough impact order:
  - **Fold the field offset into the load's addressing mode — FIRST SLICE LANDED 2026-07-26 behind `MCC_AST_REGDISP` (default OFF below `-O4`/`-Os`), x86_64 member derefs only.** The dedicated SValue form the failed prototype lacked is `VT_REGDISP = 0x0080` — free because `VT_VALMASK` was widened to 0x7f and `VT_LVAL` is 0x100 — marking "register base + real displacement" so it is distinguishable from a plain register rvalue. The member deref sets `c.i = cumofs` + the marker instead of emitting `add`; `gaddrof` materialises the add when it clears `VT_LVAL`; x86_64 `gen_modrm_impl` treats the marker like `MCC_TREG_MEM`; `save_reg_upstack` drops it when spilling to `VT_LLOCAL`. **Two hazards found by measurement, both confirming the "c.i is overloaded/stale" warning below:** (1) the base's `c.i` is NOT zero after `gaddrof` — it still holds the pointer local's frame offset, so the fold must ASSIGN `cumofs`, never add to it (`p->b` at +4 emitted `-0x4`, exactly `-8 + 4`); (2) an array-typed member never gets `VT_LVAL`, so its address flows on as an rvalue and the displacement gets double-counted — array members are excluded for now. Validated: gate OFF byte-identical (496 objects at -O0/-O2), ctest 5589/5589, all 5 arches build; gate ON a 60-seed randomized struct differential (mixed field types, nested struct, pointer member, address-of, write and loop paths) matches gcc 120/120 at -O0/-O2 AND is self-identical to gate-off output 120/120. Effect: nbody `advance` add-imm 18→3, a struct battery 32→23. **Process note: the first gcc-oracle run showed 60/60 failures that were NOT the gate** — gate-off, gate-on and HEAD all agreed, and the generator was at fault (it called `rd()` and a mutating `loop()` in one `printf`, whose argument order is unspecified). Always check the gate-off baseline against the same oracle before attributing a differential failure. **Replay fold added 2026-07-26 (mccast.c) — it is NOT optional.** The replay always re-emitted the `add`, so with the gate on the parser and replay bodies disagree and the function is rejected as unfaithful, silently losing ALL AST optimization: on the mcc amalgamation `MCC_AST_VERIFY=1` counts **136 unfaithful gate-off, 362 gate-on without the replay fold, 159 with it**. **The residual replay-fidelity gap is CLOSED 2026-07-26** — the mcc amalgamation at -O2 now reports **159 unfaithful gate-off and 159 gate-on, identical sets** (was 136/362-without-replay-fold/159-with-it, then +23..26 gate-on regressions). **The earlier diagnosis in this entry was wrong** and is kept only as a process lesson: it read the `tok_str_free_str` dump as "the parser folded and the replay did not" and blamed a `VT_CONST|VT_SYM` base taking a different branch. Measuring the constant settles it — the replay's EXTRA `add` is always exactly `cumofs * sizeof(*member)` (0x89e30→0x44f180 in `tok_str_free_str`, 0x10→0x80 in a 3-line repro), i.e. a *pointer-scaled* re-add of the same offset, so the add is emitted TWICE on replay, not skipped. Real cause: `gaddrof` materialises the folded displacement with `vpushi(c.i); gen_op('+')`, and during CAPTURE those calls run through the recorder hooks, so the parser records a spurious `Binary('+', Addr(Member), Literal(cumofs))` on top of the `Member` node that already carries `ival = cumofs`. The replay then re-derives the add from the `Member` node AND replays the recorded `Binary` — and because the replayed child's type is the member's own type (not the `char_pointer_type` the parser had temporarily installed), the second add is scaled by the element size. Fix: new `ast_hook_synth_begin`/`_end` (mccast.c, `ast_in_op++/--`, the same suspend idiom `ast_hook_imag_begin/end` uses) bracket the materialisation block in `gaddrof` so the synthetic add is invisible to the recorder; the vstack is net-neutral across the block, so `ast_vn`/`rel` stay in sync. The failing class is exactly address-of-a-member (`&p->f`, `*(T*)&p->f`); plain member read/write was already faithful. Validated: gate-off byte-identical (504 exec+behavior objects at -O0/-O2 and the self-compiled amalgamation at -O0/-O1/-O2/-O3, pre-fix binary vs post-fix binary on identical sources), ctest 5589/5589, asttool 747/0, all 5 arches build, and an 80-seed randomized struct differential (global/local/pointer bases, array members, nested struct, pointer-to-struct member, address-of read and write paths) matches gcc 240/240 at -O0/-O2/-O4 with the gate on. **Gate-on spill bug FIXED 2026-07-26 — `MCC_AST_REGDISP=1 ctest -R '^exec'` is now 4737/4737** (it was 64 failures over 4 programs x 16 configs: `bitfields`, `bitfields_ms`, `average`, `cleanup`; all pre-dated the replay-fidelity fix above and reproduce on the pre-fix binary). Single root cause, and it is the one hazard the original scoping did not enumerate: **`save_reg_upstack` (mccgen.c) silently dropped the folded displacement.** It spills the base register to a temp slot, then retags every vstack entry using that register as `VT_LLOCAL | c.i = slot` — and `VT_REGDISP` was in the cleared mask while `c.i` (which held `cumofs`) was overwritten with the slot offset, so the member offset vanished and the reload addressed the struct base. `s->c++`, `++s->c` and `s->c += 1` all broke; `s->c = s->c + 1` did not, because only the compound forms keep the folded lvalue live across a spill. Instrumented count on the mcc amalgamation at -O2: **943 spills lost a displacement**. Fix: a pre-pass in `save_reg_upstack` spills folded entries first, grouped by distinct displacement, each group getting its own temp slot that holds the already-added address — `gen_reg_addi(r, d)`; `store(r, slot)`; retag; `gen_reg_addi(r, -d)`. The restore is emitted only when the register still has a vstack user (or is in `ast_pinned_regs`) after retagging, which in practice it never does, so the steady-state cost is one LEA per spilled entry. `gen_reg_addi` is a new x86_64 primitive (`lea d(%r),%r` — **LEA, not ADD, so the flags survive**; a spill can land between a compare and its consumer). The grouping loop handles several distinct displacements on one base by adjusting and restoring the register per group; measured on the amalgamation that case never arises (943/943 spills had exactly one distinct displacement and all users were folded entries), so that arm is correctness insurance, not a measured path. Also note `p->r2 == r` entries are matched only by the generic path, which would still drop a displacement — unreachable today because a folded entry is a pointer-sized lvalue with no `r2`. **Size effect on the mcc amalgamation at -O2: `.text` gate-on 1789315 B vs gate-off 1791087 B (−1772 B), add-immediates 6235 vs 15188 (−59%).** Without the dead-restore skip the same build was 1792190 B, i.e. *larger* than gate-off — the spill LEAs eat most of the fold's win, so any future widening of this gate should re-measure `.text` rather than assume the add-count drop is the whole story. `ast_regdisp_env` is now `#ifdef MCC_TARGET_X86_64`-guarded (previously any arch could set the env and get `VT_REGDISP` with no backend recognition); the per-arch ports must lift that guard together with their own `gen_reg_addi` and `load`/`store`/`gaddrof` recognition. Validated: gate-off byte-identical (504 exec+behavior objects at -O0/-O2 and the self-compiled amalgamation at -O0/-O1/-O2/-O3, pre-slice binary vs post-slice binary on identical sources), ctest 5589/5589 default, exec 4737/4737 with the gate on, replay fidelity 159 unfaithful gate-off = 159 gate-on (identical sets), asttool 747/0, all 5 arches build, and two randomized differentials vs gcc with the gate on: 120-seed struct battery 240/240 at -O0/-O2 and a 150-seed register-pressure battery (3-6 live struct pointers, compound assign through `->`, nested/indirect members, FP+GP mixed) 315/315 at -O0/-O2 plus -O4 on every 10th seed. Process lesson repeated from the slice above: the gate's original validation was a randomized struct differential plus object self-identity and caught none of this — **`MCC_AST_REGDISP=1 ctest -R '^exec'` is the cheap gate that does.** **Gate-on self-host fixpoint + shape matrix DONE 2026-07-26.** `tools/selfhost-fixpoint.py cmake-debug MCC_AST_REGDISP=1` is byte-identical (o1 == o2 == o3, 5417567 B) — a mcc built by a REGDISP-enabled mcc reproduces itself — and so is the same run with REGDISP added to the 12 default-on gates (`PROMOTE/COLOR/LICM_TEMP/IVSR/PRE/REASSOC/SETHI_LEAF/NARROW_ELIM/ARGFWD/SPILL_SHARE/VLAT/DIVMAGIC`). A single-TU shape matrix (nested `->` chains through a cyclic list, all five bitfield widths incl. a 40-bit `long` field, address-of chains taken to `int*`/`long*`/`double*`/`struct*`/array-element/array-of-struct, array members, union punning, struct-by-value in both directions for the small/MEMORY/HFA classes, GP and FP varargs, `const`/`volatile` qualified paths, and a stack-local copy of the whole nest) matches gcc at -O0/-O1/-O2/-O3/-O4 with the gate on, and the gate demonstrably fires on it (add-immediates 81 -> 43, objects differ from gate-off). **Payoff measured on nbody (5M steps, interleaved best-of-7, correct output both ways, with `OPASSIGN`+`MATH_INLINE_PREPASS`+`PROMO_LEAF_CALLEE`+`PROMO_ARROW` on): 530 ms -> 500 ms (-6%), `advance()` 190 -> 181 instructions, add-immediates 21 -> 4, stack memops unchanged at 53.** So the fold is a real but modest win: it removes the address arithmetic it targets, and it does NOT touch the spill traffic that the FP/compute item identifies as the actual bottleneck (gcc -O2 is 260 ms here, so this moves mcc from 2.08x to 1.92x). Size on the mcc amalgamation is -1772 B .text. Judge further investment against that: the remaining ports are per-arch mechanical work for a ~6% single-workload gain. Remaining: port `load`/`store`/`gaddrof` recognition (and a per-arch `gen_reg_addi`, plus lifting the `#ifdef MCC_TARGET_X86_64` on `ast_regdisp_env`) to arm64/riscv64/i386/arm; fold array indexing (`a[i].f` and the array-member case above); then the read/write/&/nested/bitfield/byval/vararg matrix + self-host fixpoint + golden regen + M8 soak before flipping.
  - **(original scoping, still accurate for the un-ported arches)** mcc materializes an `add` for EVERY register-base + constant-offset deref at ALL -O levels (`mov p,%rax; add $off,%rax; mov (%rax)`, ~3 insns/field) instead of `mov off(%p),…` (1 insn), because the parser's member/array deref (`mccgen.c`: `gaddrof(); vpushi(cumofs); gen_op('+')`, mirrored in the replay ~4871) materializes the add for a register base instead of keeping the constant as an lvalue displacement. `load()`/`store()` already emit `[reg+disp]` for read/write, so the fold is feasible — BUT the mcc/TCC SValue model assumes a register RVALUE has `c.i==0`, and a `load()`-side `fc!=0` heuristic is a VERIFIED-WRONG dead end (a register rvalue's `c.i` is overloaded/stale — the `v != r` reg-move path drops it on purpose — so keying on `fc!=0` corrupts unrelated register values; prototyped and it miscompiled even gate-off). The real requirement is a DEDICATED SValue form/marker for "register base + real displacement" distinct from a plain register value, recognized by `load`/`store`/`gaddrof`/`gv`; then fold in the deref/lvalue context only (`gaddrof` materializes `add c.i,reg` when it clears `VT_LVAL` on a register base with `c.i!=0`), parser + replay folding identically under one gate. Cross-cutting (every struct/array access, every arch) ⇒ gate default-OFF byte-identical, validate read/write/&/nested/array/bitfield/byval/vararg + self-host fixpoint + fuzz on x86_64, then port `load`/`store`/`gaddrof` per-arch, golden regen + M8 soak before flipping.
  - **Expand the leaf promotion pool to callee-saved registers — GP DONE** (gated `MCC_AST_PROMO_LEAF_CALLEE`, default OFF below `-O4` ⇒ byte-identical). A leaf (no calls) previously got only the tiny caller-saved GP pool (3 on x86_64), which is the regalloc cliff a function falls off the moment its last call — e.g. `sqrt` — is inlined away (it drops from the callful callee-saved pool to the caller-saved leaf pool). Now, with the gate on, `ast_plan_promotion` builds a combined leaf GP pool (caller-saved first so the graph colorer prefers the no-save regs, then callee-saved), and `ast_promo_entry_init`/`_exit_restore` save/restore each promoted **callee-saved** reg's incoming value per-reg (the save condition changed from the blanket `if (ast_promo_callful)` to per-reg `ast_promo_reg_is_callee`, which is provably byte-identical for the two pre-existing paths: a callful fn promotes only into callee-saved regs ⇒ saves all as before; a leaf-gate-off promotes only into caller-saved ⇒ saves none as before). Validated (docker + qemu): with the gate on, a register-hungry leaf now uses callee regs (x86_64 rbx/r12-r15, riscv64 s-regs — hotleaf ref count 49/90; arm64 overflows less, its caller pool is already 7) and the prologue emits the `rbx/r12-r15 → stack` saves; a multi-leaf differential (caller holds many values live across each call, stressing callee-saved preservation) bit-matches gcc at O0/O2/O4 on x86_64/arm64/riscv64; default-off byte-identical on all three; asttool 20/0; all 5 arches build. Remaining: flipping default-on after the golden-regen + AOT==JIT-on-arm64 + native-arm64 3-stage self-host soak. The hot pointers `p=&b[i]`/`q=&b[j]` promote via `MCC_AST_PROMO_ARROW`; with LEAF_CALLEE they can now all sit in registers in a sqrt-inlined leaf.
    - **FLOAT promotion pool — investigated 2026-07-26, two dead ends, do NOT redo without the infra work:** (1) *Widening the caller-saved leaf FP pool* (x86_64 xmm6,7 → xmm2..7) is a NO-OP win: prototyped behind a gate, it's correct + byte-identical-off, but mcc's backend register allocator already keeps a leaf's FP values in xmm0–7 on its own — a single-loop AND a two-loop (cross-block) FP-heavy leaf both emit ZERO FP stack spills/reloads with the pool at 2 OR 6, so the extra pinned promo regs buy nothing (FP promotion is redundant with the backend's own FP regalloc, unlike GP where promotion pins across the whole function). Reverted; don't re-add. (2) *The callee-saved FP pool* (arm64 v8–v15 / riscv64 fs0–fs11) is INFRA-BLOCKED: both backends model only 8 FP registers, all caller-saved (`reg_classes` has just `MCC_RC_F(0)..F(7)`, and arm64/riscv64 `fltr`/`freg` assert `r <= F(7)`), so there is no register-id for v8+/fs0+ at all. It needs extending the whole FP register file (the `MCC_TREG_F` range + `reg_classes` + `fltr`/`freg` + prolog/epilog ABI save-restore + get_reg's scratch discipline), i.e. the real "PR-3" lift, not a pool-table edit. x86_64 SysV genuinely has no callee-saved xmm, so it's out regardless.
  - **Loop strength reduction — pointer-index READ loops DONE** (gated `MCC_AST_IVSR_PTR`, default OFF below `-O4` ⇒ byte-identical). Root cause found 2026-07-26: the pre-existing `ivsr` pass only matches an **explicit** `Binary('*', iv, C)`, but `a[i]` lowers to `Binary('+', ptr, iv)` where the element-size scaling is IMPLICIT in the pointer-add's codegen (no AST mul node exists), so the mul-ivsr scan never finds a target — that's why LSR "didn't fire" on any array-index loop, not just advance(). New `ast_ivsr_ptr_run` (mirrors the ivsr materialize/init/increment/subst machinery) matches the loop-invariant-base pointer-add `base + i`, materializes a pointer `p = base + i` before the loop, advances `p += stride` each iteration (the pointer-add scales by the element size), and substitutes. Uses `ast_licm_operands_ok` + `ast_expr_pure` for the base (NOT `ast_cse_regpure`, which rejects a pointer param's memory-load Ref). Validated (docker + qemu): fires on x86_64 + arm64, eliminating the per-iteration index `imul` for non-power-of-2 element sizes (sum-over-`struct{double×7}` 3 imuls→0, **nested loops both levels 2→0**, two-array 1→0; power-of-2 strides correctly untouched — SIB/shift already handles them); a 6-pattern differential (multi-field, non-pow2, stride-2, nested, two-array, reverse) + a randomized struct-array battery bit-match gcc at O0/O2/O4; byte-identical gate-off on x86_64/arm64/riscv64; asttool 20/0; all 5 arches build. **Follow-up (a) STORE-target / compound-assign loops DONE** (2026-07-26): a dedicated `ast_ivsr_ptr_subst` (no lvalue guard, unlike `ast_licm_subst`) now replaces the pointer-add everywhere — a `base+i` is ALWAYS an rvalue (a computed address), so substituting it inside a store target's address (e.g. advance's `p[i].vx -= …`) is safe. The function must first be REPLAYABLE, which for compound-assign-through-pointer needs `MCC_AST_OPASSIGN=1` (else it's not optimized at all); with `MCC_AST_OPASSIGN=1 MCC_AST_IVSR_PTR=1` a write/compound-assign loop now fires (x86_64 upd-loop imul 6→1, arm64 mul 8→3) and bit-matches gcc at O0/O2/O4; reads still fire without OPASSIGN; byte-identical gate-off; asttool 20/0; all 5 arches build. This gets advance's position-update loop (`p[i].x += p[i].vx*dt`, no `continue`) to fire. **Follow-up (b) `continue`/`break`/labelled loops DONE** (2026-07-26): the bail narrowed from `ast_sccp_has_label(a, n)` (whole loop) to `ast_sccp_has_label(a, body)` (body only). Root cause: `AST_Jump` op-4 is a label DEFINITION, op-5 a goto; a `continue`/`break` compiles to a goto (op-5) in the body targeting the loop's own increment/exit label (op-4), which lives in incrbb / after the loop — NOT in the body. So the whole-loop scan was flagging the loop's own compiler-generated increment label. The real hazard is a label DEFINITION in the body (an external goto-into-loop target that would bypass the `p` init) — checking only the body allows continue/break/goto-OUT while still bailing on that. Safe because the increment does `p += stride` in lockstep with `i++`, so p == base+i at every body use regardless of intra-loop control flow. Validated: nbody advance's INNER force loop (`if(i==j)continue`) now fires (x86_64 index-imul 12→1, arm64 24→13 — residual are FP muls), and a 40-config randomized nbody fuzz + goto-out-of-loop + a body-label case (goto-into-body, which correctly does NOT fire) bit-match gcc at O0/O2/O4 on x86_64+arm64; byte-identical gate-off; asttool 20/0; all 5 arches build. So with `MCC_AST_OPASSIGN=1 MCC_AST_IVSR_PTR=1` the pointer-index LSR now fires on the FULL nbody advance() (both the continue force loop and the position-update loop). **Remaining:** (c) riscv64 IVSR_PTR doesn't fire — **ROOT-CAUSED AND FIXED 2026-07-26 behind `MCC_AST_RELOC_EQUIV` (default OFF => byte-identical).** The re-diagnosis recorded here was right: a riscv64 `double`-array loop records but is `unfaithful` with "code identical — relocation/length divergence", because the FP constant is loaded PC-relative via an `R_RISCV_PCREL_HI20` + `R_RISCV_PCREL_LO12_I` pair whose LO12 references a fresh anonymous label put at the AUIPC site (riscv64-gen.c `load_symofs`). What the note did not say, and what matters for the fix: the two labels are not merely different indices — `put_extern_sym2` names them from `get_tok_str(0)`, so **both carry the literal placeholder name `"<no name>"` written to strtab twice at different offsets**, i.e. `st_name` differs even though the name TEXT is equal. So the fix compares the name text, not `st_name`. `ast_reloc_range_equiv` (mccast.c) replaces the raw `memcmp` of the body's relocation range in the faithfulness verdict: memcmp stays the fast path, and only under the gate does it fall back to a structural compare — equal `r_offset`, equal `ELFW(R_TYPE)`, equal addend, and symbol indices accepted as equivalent when both entries are STB_LOCAL with identical name text, `st_info`, `st_other`, `st_shndx`, `st_value` and `st_size` (two indistinguishable local symbol-table entries are interchangeable as relocation targets). **Effect on the riscv64 amalgamation at -O2: faithful 11 -> 102, unfaithful 109 -> 18** — this is the item's real payoff, unblocking the AST optimizer for riscv64 FP-constant functions generally, not just IVSR_PTR; with the gate on, `MCC_AST_IVSR_PTR=1` now fires on a riscv64 `double`-array loop. Gated because it is NOT byte-identical on riscv64 by construction: 91 more functions become optimizable, so default riscv64 codegen would change. x86_64 is unaffected either way (the memcmp fast path already matches — unfaithful stays 159 gate-on and gate-off, and the -O2 amalgamation object is identical with the gate set). Validated: gate-off byte-identical on x86_64 (self-compiled amalgamation at -O0/-O2/-O3 vs the pre-change binary) and on riscv64 (object identical to the pre-change compiler); ctest 5589/5589; asttool 747/0; all 5 arches build; riscv64 differentials under qemu vs `riscv64-linux-gnu-gcc` with the gate on — a 40-seed FP-constant battery 120/120 at -O0/-O2/-O4 and an 80-run struct + register-pressure battery 160/160 at -O0/-O2, gate-off baseline 0 failures in both. **Oracle gotcha, cost an hour: riscv64 gcc contracts `a*b+c` into `fmadd` by default, so the reference must be built with `-ffp-contract=off`** — without it 36/80 runs "fail" on last-digit float differences AND the gate-off baseline fails identically, which is the tell. (Also unrelated: mcc riscv64 has no `__builtin_va_list`, so vararg test programs don't compile there at all — gate-independent.) Remaining before flipping `MCC_AST_RELOC_EQUIV` on: the riscv64 M8 soak the arch shares (its Tier-3 self-host gap is the real blocker), and the residual-unfaithful triage, now ANSWERED. **Correction to the numbers first recorded here: 11 -> 102 was measured over a TRUNCATED TU** — the riscv64 compile was aborting partway (see the `gen_negf` crash under Other AOT). On the full 2038-function amalgamation at -O2 the gate is worth **64 -> 979 faithful** (unfaithful 1067 -> 152, desync 854 and bail 53 unchanged). Of the 152 residual, **137 are also unfaithful on x86_64** (the shared arch-neutral recorder-bail set already tracked under Tests/infra) and **15 are riscv64-only**: `asm_global_instr`, `asm_int_expr`, `ast_constparam_fold`, `ast_data_reemit`, `ast_ivsr_ptr_run`, `ast_ivsr_run`, `ast_jit_fold_consts`, `ast_memo_pack`, `ast_slice_disk_path`, `cstr_u8cat`, `emit_sizes`, `mccjit_recompile_profiled`, `mccjit_selftest_stage2`, `mccjit_selftest_strlit`, `mccjit_selftest_struct`. So yes, a second riscv64-specific cause remains, but it is ~1% of the TU rather than the blocker it looked like — characterising those 15 is the follow-up. (d) flip default-on after the golden-regen + M8 soak.
  - `MCC_AST_PROMO_ARROW` (default OFF below `-O4`/`-Os`) skips the promotion poison for `AST_OP_MEMBER_ARROW` — a `->` only reads the pointer value + derefs to other memory, so the pointer slot doesn't escape; without it the hot pointers `p`/`q` (highest promotion weight) were poisoned by the member-access poison loop (mccast.c, which poisons a local for any unary with a local-Ref child). Flip default-on after the cross-arch + fuzz soak — cross-arch DONE (bit-exact vs gcc on x86_64/arm64/riscv64/i386) and the seed fuzz campaign DONE (see the replay-fidelity item); remaining shared blockers: AOT==JIT-on-arm64 + native-arm64 3-stage self-host.
- **Math builtins.** `fabs`/`fabsf`→sign-clear and `sqrt`/`sqrtf`→bare `sqrtsd`/`sqrtss` (when the arg is provably non-negative via the conservative `ast_expr_nonneg`) inline to hardware at -O2+ on x86_64 (`MCC_AST_MATH_INLINE`, default-on from `-O1` (was ≥O2 before the -O curation); libcall restored at O0; a false nonneg only drops errno on a domain error, never corrupts the value). The inline previously did NOT fire in complex nested-loop functions (nbody `advance()` kept `call sqrt`, and the -O4 search's winning order even REGRESSED a standalone `sqrt(x*x)` to a libcall that -O2/-O3 inline) — fixed behind `MCC_AST_MATH_INLINE_PREPASS` (default ON from `-O1`): (a) `ast_math_inline_run` applies the fabs/sqrt rewrites UNCONDITIONALLY as a pre-pass before the -O≥4 search, since they're always a strict improvement independent of emit-size scoring; (b) `ast_local_nonneg` proves a Ref-to-local non-negative when the local's address is never taken and it has exactly one defining Store whose value is nonneg (covers `advance()`'s `sqrt(d2)` where `d2 = dx*dx+dy*dy+dz*dz`). `floor`/`ceil`/`trunc` → single `roundsd`/`roundss` DONE behind `MCC_AST_ROUND_INLINE` (default OFF, opt-in because roundsd is SSE4.1 not the SSE2 baseline — like gcc's -msse4.1): `gen_round(mode)` emits `66 0F 3A 0B/0A /r ib` with imm floor=0x9/ceil=0xA/trunc=0xB (bit3 suppresses the precision exception to match libm); rewrite in `ast_bfold_run` (fires -O1/-O2) + `ast_math_inline_run` prepass (so -O4 doesn't drop it). Validated x86_64: bit-exact vs gcc -msse4.1 at O0/O2/O4 over an edge sweep + 20k random bit-patterns (denormals/NaN/inf); byte-identical gate-off; asttool 747/0; self-compile clean. **arm64 port DONE** — `gen_fabs`/`gen_sqrt`/`gen_round` added to arm64-gen.c using native single-instruction FP (FABS `0x1E20C000`, FSQRT `0x1E21C000`, FRINTM/P/Z `0x1E254000`/`0x1E24C000`/`0x1E25C000`, mirroring gen_opf's FNEG); the rewrite/replay `#ifdef`s now include `MCC_TARGET_ARM64`. arm64 is **default-OFF** (`ast_math_inline_env` defaults 0 on non-x86_64 pending the arm64 golden-regen; opt-in `MCC_AST_MATH_INLINE=1`), so default arm64 codegen is byte-identical. Validated via qemu-aarch64: with the gates on, `sq(x)=sqrt(x*x)` emits `fsqrt d0,d0` and fabs/floor/ceil/trunc emit FABS/FRINTM/FRINTP/FRINTZ (gates-off = `bl` libcalls); a 20 000 random-bit-pattern fuzz (denormals/NaN/inf, double+float) over fabs/floor/ceil/trunc/sqrt bit-matches aarch64-gcc at O0/O2/O4; default-off byte-identical; x86_64 unaffected (re-verified); asttool 747/0; arm64/x86_64/riscv64 all build. **riscv64 fabs/sqrt port DONE** — `gen_fabs` (fsgnjx.d/.s = `fabs.d/.s`, `ER(0x53,2,r,r,r,0x11/0x10)`) and `gen_sqrt` (`fsqrt.d/.s`, `ER(0x53,7,r,r,0,0x2D/0x2C)`) added to riscv64-gen.c; the fabs/sqrt rewrite+replay `#ifdef`s now include `MCC_TARGET_RISCV64`. floor/ceil/trunc are NOT ported to riscv64 (RV64 baseline has no round-to-integral insn — gcc keeps the libcall too; the round rewrite/replay stay `#if X86_64||ARM64`, and `gen_round` isn't referenced on riscv64). Default-OFF like arm64 (opt-in `MCC_AST_MATH_INLINE=1`). Validated via qemu-riscv64: `sq(x)=sqrt(x*x)` emits `fsqrt.d`, fabs emits `fabs.d/.s` (gate-off = libcall); a 20 000 random-bit-pattern fuzz (fabs/sqrt, double+float) bit-matches riscv64-gcc at O0/O2/O4; default-off byte-identical; x86_64 + arm64 re-verified unaffected by the `#ifdef` split; all 5 arches build; asttool 747/0. **`-fno-math-errno` DONE** — added the `-f[no-]math-errno` flag (`MCCState.no_math_errno`, FlagDef in libmcc.c; also `MCC_AST_NO_MATH_ERRNO=1`). When set, the sqrt rewrite drops the errno-EDOM guard so sqrt of ANY sign inlines to hardware (sqrtsd/fsqrt returns the same NaN libm would; only errno is skipped) — matches gcc `-fno-math-errno`. No new gen (reuses `gen_sqrt` on all arches; arch-neutral). Validated x86_64: default (errno-preserving) matches gcc default at O0/O2/O4 and is byte-identical (no-flag == `-fmath-errno`); `-fno-math-errno` inlines unknown-sign sqrt at -O2 (matches gcc `-fno-math-errno`) and at -O4 with `MCC_AST_MATH_INLINE_PREPASS=1`; asttool 747/0; self-compile clean. This supersedes the common case of the "possibly-negative sqrt" item; the errno-PRESERVING inline (sqrt of unknown sign that STILL sets errno, via a NaN-check diamond) remains a lower-priority follow-up. **copysign x86_64 + riscv64 + arm64 DONE** (gated `MCC_AST_COPYSIGN_INLINE`, default OFF on ALL arches — opt-in, byte-identical default even on x86_64 where fabs/sqrt math-inline is otherwise default-on). New `AST_OP_COPYSIGN` binary op + rewrite/replay. riscv64 `gen_copysign` = `fsgnj.d/.s` (1 insn, `ER(0x53,0,x,x,y,0x11/0x10)`); x86_64 `gen_copysign` = SSE mask, no rodata (`pcmpeqd` all-ones → `psllq 63`/`pslld 31` sign mask; `andpd` y→sign(y), `andnpd` →|x|, `orpd`). **arm64 `gen_copysign` = GP round-trip** (no single insn): `fmov` x/y into two GP scratch (`fmov Xd,Dn`=`0x9E660000`, 32-bit `fmov Wd,Sn`=`0x1E260000`), `and` the sign off x (`and Xd,Xn,#0x7fff..`=`0x9240F800`; 32-bit `#0x7fffffff`=`0x12007800`), keep the sign of y (`and #0x8000..`=`0x92410000`; 32-bit `#0x80000000`=`0x12010000`), `orr` them, `fmov` back (`fmov Dd,Xn`=`0x9E670000`, `fmov Sd,Wn`=`0x1E270000`); two distinct GP scratch via the `ast_pinned_regs` pin-dance (get_reg doesn't mark used). Validated (docker + qemu-riscv64/aarch64): default-off byte-identical on all three (copysign stays libcall); with `MCC_AST_COPYSIGN_INLINE=1` a 20 000 random-bit-pattern fuzz (NaN/inf/sign combos, double+float) bit-matches gcc/riscv64-gcc/aarch64-gcc at O0/O2/O4 (x86_64 emits andpd/andnpd/orpd, riscv64 `fsgnj.d`, arm64 fmov/and/orr); all 5 arches build; asttool 20/0; self-compile clean. Remaining copysign: i386 only (x87 has no single copysign; GP round-trip needs a memory store/reload — deferred like i386 floor/ceil/trunc); the errno-preserving NaN-check-diamond sqrt (default-mode inline of unknown-sign sqrt that still sets errno; `-fno-math-errno` already covers the don't-care case). **Errno-diamond investigated 2026-07-26 — real but deferred, know the shape before starting:** gcc's default-mode inline is `sqrtsd x→r; ucomisd r,r; jp .Lcall; (use r); .Lcall: call sqrt` — a lazy value-producing diamond where ONE arm has the errno side effect (so it can't be a `gen_select_branch`-style select of precomputed values, and setting errno directly needs `__errno_location()`/TLS anyway ⇒ a call is unavoidable). It can't be a clean AST rewrite either: the AST optimizer is a CFG (AST_If/Jump/BasicBlock), with NO expression-level ternary node, so a synthesized value-merge across a branch is exactly the hard non-SSA merge problem. So it must be a raw per-arch `gen_sqrt_errno` helper interleaving `gtst`/`gjmp`/`gsym` + `gfunc_call` + a `move_reg` merge (template: mccgen.c `gen_select_branch`), keeping x live across the call — high miscompile risk, only docker-fuzz-validatable (no local M8/self-host), so it needs a focused loop with heavy errno+value fuzz, not a drive-by. Its practical value is also narrower than it looks: the hot sqrt cases (distances/norms) already inline via the `ast_expr_nonneg` proof, so the diamond only helps genuinely-unknown-sign sqrt under default `-fmath-errno`. **i386 (x87) fabs/sqrt port DONE** — `gen_fabs` (x87 `fabs`, `o(0xe1d9)`) and `gen_sqrt` (x87 `fsqrt`, `o(0xfad9)`) on st0, mirroring the `fchs` negate; the fabs/sqrt rewrite+replay+prepass `#ifdef`s now include `MCC_TARGET_I386`. No floor/ceil/trunc (x87 frndint needs rounding-control juggling — gcc keeps the libcall) and no copysign on i386. Default-OFF (opt-in `MCC_AST_MATH_INLINE=1`). Validated via `-m32`: `ff`→`fabs`, `sq(x)=sqrt(x*x)`→`fsqrt` (no libcall); a 20 000 random-bit-pattern fuzz (fabs/sqrt, double+float) matches gcc `-m32` at EACH -O level (O0 libcall both, O2/O4 x87 inline both — mcc==gcc per level; x87 excess-precision is conformant on i386, FLT_EVAL_METHOD=2); default-off byte-identical; all 4 arches build; asttool 747/0; x86_64/riscv64 re-verified unaffected. So math-inline fabs/sqrt now covers ALL FOUR optimizer arches (x86_64/arm64/riscv64/i386). Remaining i386: floor/ceil/trunc (x87 rounding-control) + copysign. **`round`/`roundf` arm64 DONE** — inlined via `FRINTA` (round-to-nearest, ties AWAY = `0x1E264000`, added as `gen_round` mode 3), an EXACT match for C `round()` that also doesn't raise inexact; new `AST_OP_ROUND` unary (bid 8), rewrite+prepass+replay all `#if MCC_TARGET_ARM64`-only and gated behind the existing `MCC_AST_ROUND_INLINE` (default OFF). x86 stays a libcall (roundsd is ties-to-EVEN, can't do C round's ties-away — so round is arm64-only, NOT in the floor/ceil/trunc x86_64||arm64 block). Validated via qemu-aarch64: with the gate on, `round`/`roundf` emit `frinta d0,d0`/`frinta s0,s0` (gate-off = `bl round`), a 20 000 random-bit-pattern fuzz + a halfway/ties-away/±0/inf/large edge sweep bit-match aarch64-gcc at O0/O2/O4, the -O4 prepass keeps it (7 frinta), default-off byte-identical; x86_64 round confirmed still a libcall + byte-identical gate on/off; all 5 arches build; asttool 20/0. flip arm64/riscv64 `MCC_AST_MATH_INLINE` default-on after each arch's golden-regen + soak; then flip `MCC_AST_MATH_INLINE_PREPASS` + `MCC_AST_ROUND_INLINE` after the cross-arch differential + AOT==JIT-on-arm64 + fuzz soak. **`fmin`/`fmax` arm64 DONE** — inlined via `FMINNM`/`FMAXNM` (opcode 0110/0111 = `0x1E206800`/`0x1E207800`, `gen_fminmax(is_max)` mirroring gen_opf's FP 2-source path), the IEEE-754 minNum/maxNum forms that return the numeric operand vs a quiet NaN and treat −0<+0 — an EXACT match for C fmin/fmax; new `AST_OP_FMIN`/`AST_OP_FMAX` binary ops (bid 6/7), rewrite+replay `#if MCC_TARGET_ARM64`-only, gated behind new `MCC_AST_MINMAX_INLINE` (default OFF; fminnm is ARMv8 baseline so no ISA-extension reason, off only for the byte-identical policy). x86 stays a libcall (`minsd`/`maxsd` propagate NaN and mishandle ±0). Validated via qemu-aarch64: with the gate on, fmin/fmax emit `fminnm`/`fmaxnm` d/s (gate-off = `bl fmin`), a 20 000 random-bit-pattern fuzz + a full NaN/±0/±inf/ordered edge cross-product bit-match the TRUE libm (gcc `-O2 -fno-builtin-fmin…` reference) AND gcc -O2 at O0/O2/O4, double+float; default-off byte-identical; x86_64 fmin confirmed still a libcall + byte-identical gate on/off; all 5 arches build; asttool 20/0. `round` inlines ONLY on arm64 (FRINTA) — x86 `roundsd` round-to-even ≠ half-away-from-zero. **`rint`/`nearbyint` x86_64 + arm64 DONE** — round to integral using the DYNAMIC rounding mode (a two-arch win, unlike round/fmin/fmax): x86 `roundsd`/`ss` imm `0x4` (rint, RAISES inexact) / `0xc` (nearbyint, suppresses) via `gen_round` modes 4/5; arm64 `FRINTX` (`0x1E274000`, rint) / `FRINTI` (`0x1E27C000`, nearbyint) as `gen_round` modes 4/5. New `AST_OP_RINT`/`AST_OP_NEARBYINT` unary (bid 9/10, added to the bfold table); rewrite+prepass+replay `#if X86_64||ARM64`; reuses the `MCC_AST_ROUND_INLINE` gate (default OFF). Constant args are NOT const-folded (they depend on the dynamic mode — `ast_bfold_eval_d/f` return 0 for id 9/10), so a literal stays a libcall; only runtime args inline. Validated (docker native x86_64 + qemu-aarch64): with the gate on, emit `roundsd`(imm 0x4/0xc)/`roundss` and `frintx`/`frinti` (gate-off = `bl rint`), a 20 000 random-bit-pattern fuzz + a ties/±0/inf/large edge sweep bit-match the TRUE libm (gcc `-O2 -fno-builtin-rint…`) at O0/O2/O4, double+float; default-off byte-identical on x86_64/arm64/riscv64; riscv64/i386 keep the libcall; asttool 20/0; all 5 arches build. So the rounding family now covers: floor/ceil/trunc (x86_64+arm64), round (arm64), rint/nearbyint (x86_64+arm64); riscv64 has none (no round-to-integral baseline insn). **`fma`/`fmaf` arm64 + riscv64 DONE** — fused multiply-add with a SINGLE rounding (faster AND more accurate than `x*y+z`), a baseline instruction on both: arm64 `FMADD Dd,Dn,Dm,Da` (FP 3-source, `0x1F400000` double / `0x1F000000` single | Rm<<16|Ra<<10|Rn<<5|Rd), riscv64 `fmadd.d/.s` (R4-type via `ER(0x43,rm=7,rd,rs1,rs2,func7=(rs3<<2)|funct2)`, dynamic rounding). It's the first 3-arg builtin: new `AST_OP_FMA` (bid 11 in the bfold table), the rewrite intercepts bid 11 BEFORE the 2-slot `ab[]` const-fold machinery (which would overflow at nargs 3) and never const-folds it (correctly-rounded fma is host-mode-dependent). `gen_fma` forces all three operands into FP regs with a `gv; vrott(3)` ×3 loop (each protected on the vstack while the next is loaded — no explicit pinning), emits into a fresh reg, pops to one value. Rewrite+replay `#if ARM64||RISCV64`; new `MCC_AST_FMA_INLINE` gate (default OFF). x86 needs FMA3 (not baseline) so it stays a libcall. Validated (docker + qemu-aarch64/riscv64): with the gate on, emit `fmadd`/`fmadd.d`/`.s` (gate-off = libcall — clean toggle: arm64 2 fmadd→0, riscv64 fmadd.d+.s→2 jal); a 20 000 random-bit-pattern fuzz + a 2744-combo edge cross-product (cancellation, NaN/inf/±0, single-vs-double-rounding-differ cases) bit-match the TRUE libm (gcc `-O2 -fno-builtin-fma`) at O0/O2/O4, double+float; default-off byte-identical; x86_64 fma confirmed still a libcall; all 5 arches build; asttool 20/0. Note: inlining sqrt alone doesn't move the nbody wall-clock — the dominant cost is the hot-loop regalloc/LSR above; this removes the per-iteration call+spill, a prerequisite.

The "eligible-but-search-doesn't-select" mechanism (a strategy is faithful + gated on but the search's winning order drops it) is the same reason both the math-inline and the IV/loop passes don't fire on `advance()` — a unified fix guaranteeing key always-beneficial strategies run addresses them together.

**arm64-native follow-ups (measured 2026-07-26, Apple Silicon / Apple clang 21 reference — the measurements above are x86_64/gcc-docker; these are the native-arm64 gaps + cross-cutting items surfaced re-profiling the in-tree vendor/plb kernels).** Native min-of-5 vs Apple clang -O2: nbody/2.c **3.3×** (mcc 4.03s / clang 1.22s), spectral-norm/3.c **4.0×** (25.2 / 6.37), nsieve/1.c **2.1×** (2.85 / 1.37); mcc -O0/-O1/-O2/-O3 are within noise of each other on all three (AST passes give ~0 hot-loop delta — the gap is the backend). Disasm of nbody `advance()` on arm64: **221 ldr / 58 str vs clang 29 / 9**, and `bl _sqrt` per inner iteration (clang uses `fsqrt`). `-O45` size-mode and `MCC_AST_JITSCORE=1 -O45` runtime-mode both recover ~0% over -O3 (45s search, ~1100 whole-program recompiles), confirming the gate search can't reach the backend. Each item gated default-OFF ⇒ byte-identical per How-to-process:
- **Flip arm64 `MCC_AST_MATH_INLINE` default-on.** `ast_math_inline_env` is `optimize>=2` on x86_64 (mccast.c) but hard-`0` on every other arch (mccast.c), so out-of-box arm64 emits `bl _sqrt`/`bl _fabs` — the reason native arm64 shows the libcall above. The lowering already exists (arm64 `gen_sqrt`=FSQRT arm64-gen.c, `gen_fabs`=FABS :2289; replay `#if …||ARM64` at mccast.c); errno safety is the shared `ast_no_math_errno || ast_expr_nonneg(arg)` guard (mccast.c). Flip = merge the :1950 branch into the x86_64 case; pair with defaulting `MCC_AST_MATH_INLINE_PREPASS` on (or confirm the -O4 fullset cycle re-runs bfold) so the -O≥4 winning order doesn't revert sqrt to a libcall (x86_64 hazard mccast.c). Shares the arm64 golden-regen + native-arm64 differential/self-host soak with the `opt_promote` flip (Ungate §). **INVESTIGATED 2026-07-26 — answer: yes, the existing leaf path suffices, and the item's premise needs one correction.** Measured on an arm64-emitting mcc, disassembling `advance()` (deterministic counts, not wall-clock; an nbody written for this measurement, so compare rows against each other, NOT against the Apple-Silicon vendor/plb figures above):

| gates | insns | stack memops | of which FP | `bl` | `fsqrt` |
|---|---|---|---|---|---|
| arm64 defaults | 210 | 82 | 34 | 1 | 0 |
| `MATH_INLINE`+`PREPASS` | 210 | 82 | 34 | 1 | 0 |
| + `OPASSIGN` | 210 | 82 | 34 | **0** | **1** |
| + `PROMOTE` | 199 | 67 | 19 | 0 | 1 |
| + `PROMO_ARROW` | **182** | **47** | 19 | 0 | 1 |
| + `LEAF_XMM` / `LEAF_CALLEE` | 182 | 47 | 19 | 0 | 1 |

**(1) The correction: flipping `MCC_AST_MATH_INLINE` default-on will NOT remove the `bl _sqrt` from `advance()`.** `MATH_INLINE`+`PREPASS` alone changes nothing there — the libcall only goes away once `MCC_AST_OPASSIGN=1` is also on, because `advance()` compound-assigns through a pointer (`bi->vx -= …`) and is not `ast_replay_ok` without it, so the optimizer never sees the function at all. This is the same replayability precondition already documented for x86_64; the item above attributed the native-arm64 libcall to the `mccast.c` hard-0 alone, and that is only half of it. The gate itself is fine: on a replayable leaf (`double simple(double x){return sqrt(x*x);}`) `MCC_AST_MATH_INLINE=1` ALONE emits `fsqrt` and drops the `bl`, so the flip is still worth doing — it just fixes ordinary functions, not this one.
**(2) Once sqrt is inlined, `advance()` does become a leaf (`bl`=0) and the EXISTING promotion machinery fires on arm64** — `PROMOTE` takes stack memops 82→67 and FP memops 34→19, and `PROMO_ARROW` takes it to 47 (insns 210→182). So **arm64 does not need the non-leaf FP promotion / per-call live-range-splitting work below to capture this**; that item stays valuable for call-containing loops but is not the blocker here. `LEAF_XMM` and `LEAF_CALLEE` add exactly nothing on arm64 for this function, consistent with the "arm64 overflows less, its caller pool is already 7" note — do not spend effort widening arm64 leaf pools.
**Where the arm64 effort should go, in order:** flip `MATH_INLINE` (+`PREPASS`) default-on for the ordinary-function win; get `OPASSIGN` flipped (its blockers are AOT==JIT-on-arm64 + native-arm64 3-stage self-host, already listed under Tests/infra) since it gates the whole hot-loop story on arm64 exactly as on x86_64; then `PROMOTE`+`PROMO_ARROW` for arm64. Only after that does non-leaf FP promotion become the next lever. Correctness spot-check for the whole set: every row above matches the `aarch64-gcc -O2 -ffp-contract=off` reference bit-for-bit under qemu-aarch64, and a 182-run randomized differential (FP-constant, struct, register-pressure, math-builtin and float-negate batteries) at -O0/-O2 with the full set on is 0-fail with a 0-fail gate-off baseline.
- **`MCC_AST_IVSR_PTR` is a LARGE matmul win once the counters are promoted — measured 2026-07-26 with the new instructions-retired column, and it overturns the "IVSR_PTR is net-negative" claim recorded elsewhere in this file.** Baseline `MCC_AST_OPASSIGN=1 MCC_AST_PROMO_INCDEC=1`, adding `MCC_AST_IVSR_PTR=1`: **matmul −22.8% instructions retired and −16.1% wall-clock** (60.6G → 46.8G insns); the other four kernels are flat to noise (nbody +3.5% time but −0.1% insns, nsieve −15.1% time but +1.5% insns — both layout). matmul goes from 10.25x to **~9.1x** vs gcc.
  **Why the earlier verdict was wrong is a methodology lesson worth keeping.** The recorded evidence for "IVSR_PTR makes it worse" was a STATIC count — emitted memops 21 → 47 on a matmul kernel — and that static count still reproduces exactly (8 → 41 memops, 76 → 95 instructions with PROMO_INCDEC on). But static counts weight every instruction equally, and strength reduction deliberately moves address arithmetic OUT of the inner loop into the preheader: the emitted function grows while the hot path shrinks. Dynamic instructions retired tells the opposite and correct story. **Any loop-transform gate judged on static emitted-size or memop counts has been judged on the wrong metric** — that includes the `MCC_AST_SEARCH_EMITSIZE` scorer and the `IVSR_PTR`+`MATH_PREP` "REGRESSES 27% (memops 88→202)" note under FP/compute — **re-measured 2026-07-26 and it does NOT reproduce as a regression.** On the in-tree `vendor/plb` nbody, with `OPASSIGN`+`PROMO_INCDEC` as the baseline, adding `MATH_INLINE_PREPASS`+`IVSR_PTR` moves nbody **−0.1% instructions retired** (+2.0% wall-clock with no instruction change, i.e. layout) while matmul gains −22.8%. The static signature that produced the original verdict DOES still reproduce — `advance()` memops 51 → 63, instructions 184 → 189 — which is the same "strength reduction grows the function and shrinks the hot path" effect described above, read as a regression. **So the recorded blocker "`IVSR_PTR` stays default-off and must NOT flip alongside `MATH_PREP` until its temps are register-promotable" is not supported by dynamic measurement** and should be re-decided. Two caveats on scope: this is the plb nbody, not the hand-written one the original note used, and the FP-pool poison it describes (`ltemp` slots excluded from promotion) is real — it just does not cost what was claimed. **Incidental but useful: on x86_64 the plb nbody already has `call=0` in `advance()` at plain -O2**, i.e. sqrt is inlined by the default-on `MCC_AST_MATH_INLINE`, so `MATH_INLINE_PREPASS` is a measured no-op there (identical memops and instruction counts, +0.0% insns on all five kernels) — the prepass matters for the functions the -O4 search would otherwise revert, not for reaching `advance()` on this target.
  Two conditions were both needed and neither held when the old note was written: the two IVSR_PTR miscompiles had to be fixed (`a[i][i]` and the pointer-base bug, both fixed 2026-07-26), and the loop counters had to be register-promoted (`MCC_AST_PROMO_INCDEC`, same day) so the strength-reduced pointers are not competing with memory-resident IVs. Validated for this combination: all 5 runtime-bench kernels verify their output vs gcc, the exec corpus passes 298/298 with `OPASSIGN`+`PROMO_INCDEC`+`IVSR_PTR` forced on, and a 40-seed randomized loop differential is 80/80 with a 0-failure gate-off baseline.
- **THE headline codegen gap, diagnosed 2026-07-26: mcc keeps loop induction variables in MEMORY, in essentially every loop.** Simplest possible case, `int sum(int n){int i,s=0;for(i=0;i<n;i++)s+=i*3;return s;}` at -O2: **mcc 11 frame accesses, gcc 0.** Same for a float accumulation loop (mcc 8, gcc 0) and for the matmul kernel (mcc 21-24 GP memops with FP traffic at zero). **MEASUREMENT CORRECTION 2026-07-26: the gcc figures previously quoted beside that ("gcc 0 memops with 31 instructions") were INVALID and are withdrawn.** Those came from an isolated kernel file whose `static` arrays are never initialised, so gcc proved the whole product zero and folded it — the disassembly is `pxor %xmm0,%xmm0; addsd (%rax),%xmm0` with **no multiply at all** (`mulsd` count 0). Any isolated-kernel comparison against gcc must initialise the arrays or gcc is not compiling the same program. The mcc-side observations are unaffected (they are mcc-vs-mcc), and the valid comparison on the real benchmark, where `main()` initialises, is: **mcc 334 instructions / 115 frame accesses vs gcc-scalar 159 / 0, both with 3 `mulsd`** — same arithmetic, roughly double the instructions and all of the memory traffic. Every counted loop reloads and stores its counter across the back edge. This — not FP promotion, not vectorization — is the common factor behind the runtime-bench ratios (nsieve 1.5x, nbody 2.5x, mandelbrot 2.9x, spectral 4.5x, matmul 9.7x).
  **Traced end-to-end 2026-07-26; the increment poison is LOAD-BEARING, not merely conservative.** The planner's poison loop (the one `MCC_AST_PROMO_ARROW` already carves `->` out of) rejects a local for ANY `AST_Unary` over a local Ref, and `i++` is such a unary (`TOK_INC` = 0x82), so every loop counter poisons itself with its own increment — confirmed by dumping the candidate table: in the matmul kernel `i`/`j`/`k` are the three highest-weight candidates (6/5/6) all carrying `poison=1 by=unary-op=130`, and in `sum()` the counter is the one poisoned candidate. **But removing the poison does not help, and that is the informative part.** With the carve-out the planner does select the counter (`DECIDE sum faithful=1 promote=1 promo_n=3`, colour-assigned regs 10/9/8) — and the EMITTED CODE IS UNCHANGED: the counter is still `mov -0xc(%rbp),%eax; add $1,%eax; mov %eax,-0xc(%rbp)` across the back edge, frame accesses stay at 11, and `fsum` actually regresses 8 → 11. So the emit path silently ignores a promotion plan that covers an incremented local. Promotion itself is working — `MCC_AST_PROMOTE=0` vs `=1` is 14 → 11 on `sum` and 11 → 8 on `fsum` — it just cannot honour the counter. **Conclusion: the poison encodes a real limitation in the `inc()`/promoted-local interaction, so the fix is in the increment emit path (teach `inc()` / the TOK_INC replay to operate on a promoted register), NOT in the planner.**
  **LANDED 2026-07-26 behind `MCC_AST_PROMO_INCDEC` (default OFF below `-O4`/`-Os` ⇒ byte-identical), for the STATEMENT form only.** `i++;` as a statement has an unused result, so no old-value copy is needed and the update is just `reg ± 1` — and that is exactly the loop-increment case. `ast_replay_bb` now handles a statement-level `TOK_INC`/`TOK_DEC` whose child is a promoted Ref by pushing the register, applying the op and calling the existing `ast_promo_write`, mirroring the promoted-local store path right above it; the planner's poison is lifted for that same shape only (parent is an `AST_BasicBlock`). **Value-producing `i++` inside an expression still needs the old value preserved across the update, which this path does not do, so it stays poisoned** — that is the remaining half of the item.
  Structural effect is large and deterministic: `sum()` 11 → **8** frame accesses, `fsum()` 8 → **4**, the matmul kernel 21 → **8** (gcc 0). Wall-clock is a different story and is recorded honestly — measured with `tools/runtime-bench.py --runs 7`, attributing against `MCC_AST_OPASSIGN=1` as the baseline so the gate's own contribution is isolated: **nbody −5.4%, matmul −8.2%, mandelbrot −0.7%, but nsieve +6.0% and spectral +1.7% REGRESS**. An earlier 3-run read showed nsieve at −17.9%, which more runs showed to be noise — treat any single short run of this harness as unusable for deltas under ~10%. So this is a real structural win that does not yet translate into a uniform speed win, and **the nsieve regression was investigated 2026-07-26 and is NOT a codegen regression** — the guess recorded here (extra pressure on the small caller-saved leaf pool) was wrong. `nsieve()` is callful, so promotion uses the callee-saved pool; with the gate it saves 4 registers instead of 2 (frame `0x40`→`0x50`) and grows 83→89 instructions, all of it prologue/epilogue on a function called 3 times. **Both hot loops get strictly better**: the outer `for (i = 2; i < m; ++i)` and the inner `for (j = i<<1; j < m; j += i)` each replace a frame load of the counter with a register move, at identical instruction count, and total frame accesses drop 21→16. `perf stat` settles it: **instructions retired 4,649,365,262 vs 4,649,365,298** (the 36-instruction delta is exactly the extra saves on 3 calls) and branch-misses equal, yet **cycles 1.881e9 vs 2.040e9, +8.5%**. Same instruction stream, same branch behaviour, more cycles ⇒ a front-end/code-layout effect, not worse code. Consistent with the delta shrinking as the problem grows (+14.3% at `nsieve 11`, +7.9% at `12`, +1.9% at `13`). Note neither mcc NOR gcc emits loop-head alignment padding here (0 alignment nops in both), so this is layout luck rather than a missing `.p2align` pass, and chasing it further needs a layout-aware experiment (shift the function and re-measure), not a codegen change. **Practical consequence: the wall-clock table above understates the gate — nbody −5.4% and matmul −8.2% are real work reductions, while nsieve's +6% and spectral's +1.7% are not attributable to worse output.** **Shared soak progressed 2026-07-26 for the whole loop-gate set (`MCC_AST_OPASSIGN=1 MCC_AST_PROMO_INCDEC=1 MCC_AST_IVSR_PTR=1`), which is the combination the measurements above recommend:** the x86_64 3-stage self-host reaches a byte-identical fixpoint (o1 == o2 == o3, 5439919 B) with those three gates, and again with the 12 default-on gates added on top — the same size both times, so the loop gates do not perturb the self-host image. The exec corpus passes 298/298 with them forced on, all 5 runtime-bench kernels verify their output vs gcc, a 40-seed randomized loop differential is 80/80, and the four compute kernels match gcc at **-O0/-O1/-O2/-O3/-O4/-O6** (24/24), which covers the M8 bar's `-O6` differential leg. **Cross-arch leg partly closed 2026-07-26 WITHOUT docker.** The docker daemon was down (the riscv64 self-host script correctly skipped 77 rather than false-greening), but the cross differential does not actually need it: **mcc can link its own cross output against the in-tree gentoo stage3 sysroots and run it under the host qemu.** Recipe, worth reusing — build a host tool emitting the target (`gcc -DMCC_CONFIG_OPTIMIZER=1 -DMCC_TARGET_<T> …  src/mcc.c`), then compile+link with
  `-B$PWD/cmake-cross --sysroot=vendor/gentoo-stage3-<arch>-glibc -I$PWD/runtime/include -Ivendor/gentoo-stage3-<arch>-glibc/usr/include`
  and run under `qemu-<arch> -L vendor/gentoo-stage3-<arch>-glibc`. Three things have to line up and each fails loudly if missed: `-B` must point at `cmake-cross` (that is where the per-arch `riscv64-libmccrt.a` / `arm64-libmccrt.a` live — with `-B$PWD` mcc reports `file 'libmccrt.a' not found`); `--sysroot` is required or mcc picks up the HOST `/usr/lib/crt1.o` and reports `invalid object file`; and `runtime/include` must precede the sysroot includes. With that, the 40-seed randomized loop/increment differential runs **80/80 on riscv64 and 80/80 on arm64** under `OPASSIGN`+`PROMO_INCDEC`+`IVSR_PTR`, each with a 0-failure gate-off baseline — so the gate set is now differential-clean on three architectures. **The riscv64 3-stage self-host now runs WITHOUT docker too** — `tools/selfhost-riscv64-native.sh` does the same gate using mcc's own linker, the vendored sysroot and host qemu, and `selfhost-riscv64-docker.sh` `exec`s into it whenever `qemu-riscv64` + a host cc + the sysroot + `cmake-cross/riscv64-libmccrt.a` are all present (docker remains the fallback). **Byte-identical fixpoint confirmed with no docker: o1 == o2 == o3 at 2667809 B stock and 2674385 B under `OPASSIGN`+`PROMO_INCDEC`+`IVSR_PTR`**, plus the sanity program matching the host cc at -O0/-O1/-O2. One extra prerequisite the docker path never needed: the runtime archive must be staged as plain `libmccrt.a` in a scratch `-B` dir, because mcc looks for `MCC_CONFIG_CROSSPREFIX "libmccrt.a"` and a stage-built mcc has an EMPTY prefix — pointing `-B` straight at `cmake-cross` makes it pick the host x86_64 archive and fail with `invalid object file` plus unresolved `__clear_cache`/`__floatunsitf`. Verified the new script skips 77 on a missing prerequisite and exits 1 on injected object drift, so it cannot false-green. **arm64 3-stage self-host now runs too, same recipe** — the script was generalised to `tools/selfhost-cross-native.sh <arch>` (riscv64|arm64; `selfhost-riscv64-docker.sh` dispatches into it, and a new ctest `selfhost-arm64-native` covers arm64). **arm64 reaches a byte-identical fixpoint with no docker: o1 == o2 == o3 at 2361279 B stock and 2366607 B under `OPASSIGN`+`PROMO_INCDEC`+`IVSR_PTR`**, sanity program matching the host cc at -O0/-O1/-O2. So the loop-gate set now has a 3-stage self-host fixpoint on **x86_64, riscv64 AND arm64**, plus differentials on all three, all runnable on a plain dev box. Unsupported arches skip 77 rather than failing. Still outstanding for the flip: the arm64-NATIVE CI cells (qemu is x86-TSO, so it cannot stand in for the memory-model-sensitive checks) and the layout-sensitivity judgement call above. Plus the layout-sensitivity judgement call; **an instructions-retired column was added to `tools/runtime-bench.py` 2026-07-26** so this question is answerable from the harness rather than a manual `perf` run. It reports instructions retired per kernel plus a per-config delta beside the time delta, and it reproduces the manual finding automatically: with `MCC_AST_OPASSIGN=1` as the baseline, `MCC_AST_PROMO_INCDEC=1` gives **nbody −4.5% time / −1.5% insns, matmul −8.6% / −2.8%** (real work reductions) against **nsieve +7.2% time / +0.0% insns** and spectral −0.7% / −0.0% (layout). The rule is printed under the table: *a time delta with no instruction delta is code layout, not codegen — judge flips on the insn column*. `perf` is probed for BOTH presence and permission (`perf_event_paranoid` can refuse to count even when the binary exists), the column is simply omitted when it cannot count, `--no-perf` forces it off, and `--check-only` (the CI path) never invokes perf at all. Validated: gate-off byte-identical (506 exec+behavior objects at -O0/-O2 and the self-compiled amalgamation at -O0/-O2/-O3), ctest 5905/5905, asttool 747/0, all 5 arches build, the exec corpus passes with the gate on alone AND with `+OPASSIGN` (298 each), all 5 runtime-bench kernels verify their output under both gate configs, and a 60-seed randomized loop/increment differential (up/down counters, `while(i<n) s+=i++` and `++i`, nested loops, `continue`/`break`, an extra `i++` inside the body, array and FP accumulation) matches gcc 240/240 at -O0/-O2 with a 0-failure gate-off baseline.
  **Process note, third occurrence this session: the first version of that differential reported 20 failures that were entirely my generator's fault** — it called several mutating functions in one `printf` argument list, where evaluation order is unspecified, so gcc and mcc legitimately disagreed. The tell was failures at -O0, where promotion does not run, and the gate-off baseline failing identically. Sequence mutating calls into separate statements, and always run the gate-off baseline through the same oracle before believing a differential. Removing the poison without that is strictly harmful: it produces plans the backend drops, and costs code quality. A `MCC_AST_PROMO_INCDEC` prototype was built, measured, and REVERTED on this evidence.
  Reproducer needs no gates: `int sum(int n){int i,s=0;for(i=0;i<n;i++)s+=i*3;return s;}` at -O2 — mcc 11 frame accesses, gcc 0. Fixing the increment path is worth more than every other open codegen item in this file combined; the runtime-bench ratios (nsieve 1.5x … matmul 9.7x) all sit on top of it.
- **Non-leaf FP register promotion (the deferred PR-3 callee-saved FP pool). MEASURED 2026-07-26: this is NOT what the matmul gap needs — that gap is INTEGER induction-variable allocation.** Test: the identical matmul kernel compiled twice in one file, once as a leaf and once with a trailing `sink()` call to force `has_call`. **Both emit exactly 24 stack memops**, so the `xmm_max = has_call ? 0 : AST_PROMO_XMM_N` cliff (mccast.c) does not fire here at all. The earlier 72-vs-30 reading that suggested it did was an artifact — `main` also holds the init loops, so the two were never comparable. Disassembly settles it: **FP (xmm) stack traffic in the hot kernel is ZERO**; all 24 memops are GP. The inner loop reloads and stores `i`/`j`/`k` from the frame every iteration (`mov -0x10(%rbp),%eax; add $1,%eax; mov %eax,-0x10(%rbp)`) and recomputes the full 2-D address each time (`movslq; shl $9; lea; add; movslq; shl $3; add`), where gcc's scalar `-fno-tree-vectorize` output has **0** stack memops and 31 instructions vs mcc's 79. Two separate causes, in order of size:
  (a) **The kernel `desync`s by default** (`desync`) — the recorder declines it, so NO AST pass runs, including promotion. `MCC_AST_OPASSIGN=1` makes it `faithful`, which is another instance of the already-recorded fact that OPASSIGN gates the whole hot-loop story; without it, measuring any other gate on a compound-assign kernel measures nothing.
  (b) **Even faithful, the integer IVs are still not register-allocated**: 24 → 21 memops with OPASSIGN, and `PROMOTE` (already default-on at -O2 on x86_64) adds nothing. Three int locals in a leaf ought to fit the 3-register caller-saved pool, so why `ast_plan_promotion` declines them is the open question and the actual lever for matmul. `IVSR_PTR` makes it worse (21 → 47), the same "materialisation into an unpromotable `ltemp` is net-negative" trap already recorded for `IVSR_PTR`+`MATH_PREP`.
  So the FP-pool work below stays justified for FP-pressure kernels like nbody, but it cannot move matmul, and the ~5.0x mcc-vs-scalar-gcc factor measured there is an integer-regalloc story. Original framing kept below. Re-confirmed the residual cliff is FP-specific: `ast_plan_promotion` sets `xmm_max = has_call ? 0 : AST_PROMO_XMM_N` (mccast.c) — any function with a call gets ZERO FP promotion, so a double live across `sqrt` spills. GP non-leaf promotion already works (callee-saved pool + per-reg save/restore `ast_promo_entry_init`/`_exit_restore` mccast.c, on by default). Two paths in order: (1) the already-listed callee-saved FP pool (arm64 v8–v15 / riscv64 fs0–fs11) — add synthetic FP-saved reg-ids mirroring `MCC_TREG_SAVED` (arm64-gen.h, mapped in `intr()` arm64-gen.c) with `reg_classes[]=0` so only promotion reaches them, point the `has_call` FP pool at them, extend `ast_promo_reg_is_callee` (mccast.c); x86_64 SysV has no callee-saved xmm ⇒ needs the caller-save-around-call path instead. (2) NEW generalization — **per-call live-range splitting**: `has_call` is function-global today, so a caller-saved reg is unusable even for an FP value that never spans a call. Split each candidate's interval at `AST_Invoke` boundaries (the `lo[]/hi[]` interval + disjoint-interference + `ast_color_graph` machinery already exists at mccast.c) so non-call-spanning sub-ranges use caller-saved FP at no save cost and only call-spanning sub-ranges need callee-saved/around-call spill. Validate via `MCC_AST_SPILL_OUT`/objdump memop count (mccast.c) on `advance()`; the xmm8–15 RA prerequisite is DONE.
- **Loop-invariant FP-constant / global-address hoisting — two new gates.** nbody reloads the constant `1.0` through the GOT every inner iteration: arm64 `arm64_sym` unconditionally emits `adrp :got:_L.N ; ldr` on ELF (arm64-gen.c) and there is no `FMOV #imm` path, though AArch64 VFPExpandImm encodes 1.0/0.5/2.0. (a) **`MCC_AST_FMOV_IMM` — DONE 2026-07-26 (default OFF below `-O4` ⇒ byte-identical).** One correction to the scoping: the hook cannot go in arm64 `load()`, because by then the literal is already a rodata SYMBOL and its value is gone. The only place the FP constant still exists as a value is `gv()` (mccgen.c), immediately before the `section_add(rodata_section)` materialisation — that is where `arm64_fmov_imm()` now runs. Second gotcha: the obvious guard `rc & MCC_RC_FLOAT` never fires for a returned constant, because `rc` is `MCC_RC_FRET` = `MCC_RC_F(0)` which does NOT carry the `MCC_RC_FLOAT` bit; test `reg_classes[get_reg(rc)] & MCC_RC_FLOAT` instead. `arm64_fmov_imm(r, is_dbl, bits)` (arm64-gen.c) emits `0x1E201000 | ftype<<22 | imm8<<13 | Rd` and returns 0 for anything VFPExpandImm cannot represent — the `exp < -3 || exp > 4` test rejects inf/NaN, zero and every denormal without special cases, so the caller falls through to rodata rather than mis-encoding. Encodings verified against `llvm-mc` for 1.0/-1.0/0.5/2.0/31.0/-4.75 (double) and 1.0/-0.125 (single). **Measured on the cited kernel `vendor/plb/bench/algorithm/nbody/2.c`: `advance()` goes adrp 1 → 0, ldr-literal 25 → 24, insns 219 → 217** — i.e. the per-iteration GOT round-trip for the `1.0` in `inv_distance = 1.0 / sqrt(...)` is gone, which is exactly what this item predicted. On a synthetic invariant-literal loop (`1.0`, `2.0`, `0.5`) the effect is larger: adrp 4 → 1, ldr-literal 6 → 3, insns 60 → 54; a loop whose literals are NOT encodable (pi, 1e300) is untouched, confirming the fall-through. It is a pure size win too (a 13-constant TU 4486 → 3750 B), so it is a reasonable -Os/all-levels candidate as noted. Validated: gate-off byte-identical (x86_64 self-compiled amalgamation at -O0/-O2/-O3; the change is `#if MCC_CONFIG_OPTIMIZER && defined(MCC_TARGET_ARM64)` and gated); ctest 5590/5590; asttool 747/0; all 5 arches build; under qemu-aarch64 vs `aarch64-gcc`, an exhaustive constant battery — **all 256 VFPExpandImm-encodable doubles and floats reconstructed as ±(1+m/16)·2ⁿ, plus ±0, denormals, ±inf, pi, 1e±300** — bit-matches at -O0/-O1/-O2/-O4 gate-on and gate-off, plus a 208-run randomized differential (0 fail, 0 gate-off baseline fail, gate changed output on 80 objects) and the plb nbody kernel itself matching gcc with the gate on alone and combined with the MATH_INLINE/OPASSIGN/PROMOTE/ARROW set. Remaining: the arm64 golden-regen + soak before flipping default-on, shared with the other arm64 flips. (b) `MCC_AST_LICM_FP` — the synthetic-preheader hoist `ast_ltemp_scan`/`_materialize` (mccast.c/11015) is integer-only (`ast_ident_intt` excludes VT_FLOAT/DOUBLE, mccast.c); drop that for FP and admit a loop-invariant `_L.N` load, reusing the existing preheader splice `ast_ltemp_insert_before` (mccast.c) for the non-encodable-constant / invariant-global-address cases fmov-imm can't cover; gate on `ast_cost_spill`'s FP budget (mccast.c) to respect pressure. Secondary: ELF-always-GOT for local statics (arm64_sym:434) could use PC-relative ADR+ADD like the PE path (:429) — a separate, broader decision shrinking every rodata access.
- **Give the -O4 superopt search a register-promotion actuator.** `MCC_SO_SPILL_SCORE` prices spill quality into the objective (mcc.c) but is a passive MEASUREMENT — nothing in the search changes allocation, so the promotion knobs it wants to steer aren't reachable (this is why `MCC_AST_JITSCORE=1 -O45` recovers ~0%). Add the promotion controls as `so_axes[]` entries (mcc.c) — `MCC_AST_PROMO_LEAF_CALLEE`/`_LEAF_XMM` + a new bounded `MCC_AST_PROMO_POOL` pool-size — decoded from the budget radix in `so_setenv_cfg` (mcc.c; bump `SO_BUDGET_SPACE` :335), with an `ast_env_int` reader clamping the `ast_promo_*` arrays (mccast.c); per-function via a spare bit in `ast_fncfg_parse` (mccast.c) + a 4th `cfgs[]` value in `mcc_superopt_perfn` (mcc.c). CRITICAL: a bad pool/leaf choice is a MISCOMPILE, not a size regression, so the axis must select only among ABI-validated pool sizes AND `so_eval` must gate acceptance on an exec-diff vs the seed before a promotion config becomes `best` (mcc.c). Re-sweep `MCC_SO_SPILL_SCORE` jointly once the actuator lands (the 48 plateau was measured against the current fixed pools). NB the slice cache's context-free identity (Slice cache §) sits awkwardly with a function-scoped promotion decision (prologue/epilogue is global) — resolve whether promotion is a slice gate-config value or must live in the locator.
- **Auto-vectorization — implementation plan for the "No auto-vectorization" gap above (now purely greenfield; the xmm8–15 RA prerequisite is DONE).** Packed SIMD lives ONLY in the integrated assembler/disassembler tables (`OPT_MMXSSE` x86_64-asm.h; `*_Q_SIMD`/ld1/st1 arm64-tok.h) — codegen emits 0 packed ops and there is no `VT_VECTOR`/`vector_size`/intrinsic path. So the first step is BACKEND, not the AST packer: add packed emission to `gen_opf`/load/store (x86 `mulpd/addpd/movupd/unpcklpd`; arm64 `fmul/fadd .2d`, `ld1`/`st1`, `zip1/2`) + a vector reg class + a vector AST node the re-emit path lowers; THEN a straight-line SLP packer as a new AST strategy (`AST_SG_SLP` at a free gate bit 42–47; 5-edit registration recipe at mccast.c), reusing def-use (`ast_du_*` mccast.c) + escape (`ast_cprop_escapes`) analyses plus a new isomorphism/pack-group analysis. Validation is exec/diff parity (byte-identity vs gcc impossible once packed). Decide: real `VT_VECTOR` (also unlocks intrinsics) vs an emit-only pack-group annotation; x86-first vs arm64-in-v1; `movupd` (skip alignment analysis) for v1.

### Strategy scheduler — ROI/time-weighted ordering
The optimizer runs strategies round-robin to fixpoint (`ast_run_strat_cycle`, ≤8 cycles) but the -O4 order/gate search scores candidates by a static metric only: `ast_cost_score` = `nodes×(maxdepth+1)×(calls+1)` (mccast.c), or emitted byte size with `MCC_AST_SEARCH_EMITSIZE`. It does not benchmark real speed/memory. Landed: `MCC_AST_SEARCH_FULLSET` (default ON — after the emit-size combo search picks its winning order, append every remaining gated strategy so each ticks ≥1× within budget); `MCC_AST_ROI` (default ON at -O≥4, deterministic) + `ast_search_roi_order` (orders the round-robin by ROI = benefit/apply-cost, where benefit = static `ast_cost_score` delta and apply-cost = transform-count delta on a pristine clone, both pure functions of the cloned AST). ROI scores its benefit with `ast_cost_score` unconditionally — never the emit-size probe (`ast_search_emit_size` replays to MEMORY and perturbs the shared emit cursors, so `MCC_AST_ROI=1 MCC_AST_SEARCH_EMITSIZE=1` miscompiles). Phase-2 remaining:
- **Ever-fires ledger: refine ROI to report whether a technique EVER finds a promoted match / optimization — never to dismiss one a priori.** Principle: every optimization technique is valid; the question ROI must answer is not "is this worth trying?" (always try it — `MCC_AST_SEARCH_FULLSET` already ticks every gate ≥1×) but "did it EVER produce a match?" Today ROI collapses a strategy to a scalar benefit/apply-cost and a static-cost benefit of 0 silently sinks a pass to the bottom of the round-robin, where a technique that would fire on some input looks identical to one that fires nowhere. Add a per-strategy / per-gate existence ledger orthogonal to the magnitude score: for each technique, record whether it EVER changed the AST (won a candidate / found a promoted slice match / folded / re-laid-out) at three scopes — this function, this TU, and the whole self-compile + exec corpus — and surface **never-fired** techniques as an explicit report bucket (not silent absence, not a pruned gate). A never-firing technique is then a KNOWN, named gap to refine — better query/reconciliation logic, a wider search vocabulary, a missing enabling gate (the OPASSIGN-gates-the-whole-hot-loop pattern already recorded above is exactly this: a pass measured "inert" only because an upstream gate left every candidate `desync`) — rather than evidence the technique is worthless. This is the reporting substrate the handle-reduction TODO (JIT runtime §), the scalarize↔aggregate axis (search-vocabulary §), and the micro-optimization long tail all need: distinguish "fired but small here" from "never fired anywhere," because only the second is a reason to change course, and even then toward fixing the enabler, not deleting the technique.
- The static cost benefit leaves strength-reduction-class passes (`ivsr`) at benefit 0 (they cut runtime, not static cost) so they sink — fold in a measured runtime signal (reuse the out-of-process superopt's `so_jitscore` whole-program wall-clock) so real speed drives ROI. **Partly addressed 2026-07-26 by `MCC_AST_COST_SPILL` (default OFF):** `ast_cost_score` now optionally adds a per-loop register-pressure term — distinct scalar locals referenced in the loop subtree plus a Sethi–Ullman register-need estimate of its expression trees, split GP/FP, penalised by the excess over the per-target budget (`AST_PROMO_CALLEE_N` GP / 8 FP) and weighted `4^(loopdepth-1)`. Still a pure function of the arena, as the ROI scorer requires. It separates cases the old metric rated equal (nbody `advance` 1332→1812, a 12-live-double leaf 434→770, a pressure-free `main` unchanged). **What it does NOT fix, and why — the -O4 promotion loss is decided somewhere else entirely.** At -O4 the driver hands the whole compile to the OUT-OF-PROCESS superopt (`mcc_superopt_search`, mcc.c), which spawns worker `mcc` processes and scores each candidate by **`.text` byte size** (`so_textsize`) — `ast_cost_score` is never consulted for that decision. Worse, `so_setenv_cfg` (mcc.c) *unconditionally* `setenv`s the axes it searches, including `MCC_AST_PROMOTE` from bit 1 of the gate word, so (a) promotion is chosen purely on emitted size, which promotion tends to *increase* (prologue saves) while it *decreases* spills, and (b) an explicit user `MCC_AST_PROMOTE=1` is silently overwritten at -O4. Two follow-ups, in order: **(1) DONE 2026-07-26** — `so_setenv_cfg` now calls `so_axes_snapshot()` once before its first override and routes all 12 axes through `so_setenv_axis`, which keeps any value the user pinned in the environment (the `mcc_superopt_perfn` `MCC_AST_TEMPLATES` override goes through the same helper). Measured on nbody `advance()`: `MCC_AST_PROMOTE=1` at -O4 used to be ignored (18 stack memops), it is now honoured (7, matching -O2). Unpinned -O4 output is unchanged (byte-identical -O4 objects for 4 sources), 496 exec+behavior objects at -O0/-O2 and the self-compiled amalgamation stay byte-identical, pinned -O4 output matches gcc on 6 fuzz seeds + the FP repros, all 5 arches build. Note a pinned axis still consumes search evaluations on now-identical candidates — de-duplicating the gate enumeration against pinned axes is a cheap follow-up. **(2) ATTEMPTED AND REVERTED 2026-07-26 — read this before retrying, three metric proxies were tried and all three are UNSOUND.** The plumbing worked end-to-end (worker writes a count to `MCC_AST_SPILL_OUT`, `so_eval` scores `text + W*count`; commit `4fd6a445`, reverted in `dedeaf26` — reinstate the plumbing from there, it is ~40 lines and is not the problem). What failed is the *counter*:
  - **`save_reg`'s spill path** reports **0** on nbody `advance()` in BOTH promote-on and promote-off builds. What differs between them is ordinary local memory traffic, not register-pressure spills through `save_reg`.
  - **A `gv()` counter** (increment when a `VT_LOCAL`/`VT_LLOCAL` lvalue must be materialised) gives the right answer on a small TU (143 vs 156 refs, matching objdump's 41 vs 54 stack memops) but the **WRONG SIGN** on a TU with cold code: append 40 trivial cold functions to `advance` and it reports promote-on as *worse* (1305 vs 1074) while objdump shows promote-on is much *better* (**391 vs 640** whole-object stack memops, a 39% reduction).
  - **Counting at each backend's `load()`/`store()` entry** (a `mcc_stackref_note(sv->r)` in all five arches) inverts identically — 1305 vs 1074 again.
  The root cause of the inversion is **multi-emit**: a function body is emitted more than once (the parser walk, then one or more AST replays), and enabling promotion changes *how many* functions become `ast_replay_ok` and therefore how many bodies get emitted twice — so any counter incremented inside `load`/`store`/`gv` measures emit *volume*, not final code. (Excluding `ast_search_emit_size`'s trial replays by save/restoring the counter around it changes nothing at -O2, since the search only runs at -O4.) Secondary cause: `load()` on a `VT_LOCAL` that is not `VT_LVAL` emits an `lea`, not a stack access, so the proxy overcounts ~3.3× even in the single-emit case.
  **(2) LANDED on the second attempt 2026-07-26 behind `MCC_SO_SPILL_SCORE` (default OFF below `-O2`)** — `mcc_stackref_note` (called from every backend's `load`/`store`) counts into a per-function bucket that RESETS whenever `ind` rewinds below the last position it saw; every body re-emit begins by rewinding to `ast_body_ind_sv`, so the bucket always holds the most recent emit, and `ast_func_end` commits the surviving bucket to the total. That one rule fixes all three failures above without enumerating the replay sites. The counter now tracks objdump on BOTH TUs — nb2 39/52 vs objdump 41/54, dilute 229/518 vs objdump 391/640, same ordering (the old proxy said 1305/1074, inverted). End to end promotion now wins on merit: nbody `advance` 18→7 stack memops, whole object 54→41. **The budget limit this exposed is now fixed too, by a separate gate.** The diluted TU did not flip at plain -O4 (only at `-O25`), and the root cause turned out to be structural rather than a budget shortfall: the search's gate-0 baseline forces `MCC_AST_PROMOTE=0`/`TEMPLATES=0`/`INLINE=0`, so **-O4 starts from a configuration strictly worse than what plain -O2 emits** and only recovers if it reaches the promoting gate within budget. `MCC_SO_DEFAULT_SEED` (default OFF) fixes that: a `SO_GATE_DEFAULT` sentinel makes `so_setenv_cfg` *unset* every axis instead of forcing it, so the incumbent is the compiler's own defaults and the search can only improve on it (user-pinned axes still win). The two gates fix genuinely different failures and are complementary — on `advance()`: the **diluted TU** flips with the seed alone (15→7; the search is budget-starved, so the defaults incumbent survives) but NOT with the spill score alone; the **small TU** does NOT flip with the seed alone (18; the search has budget and picks the 941-byte non-promoting candidate over the 981-byte promoting one) and needs the spill score. With both on, each TU matches its -O2 reference exactly. **Weight sweep DONE 2026-07-26 — 48 is validated, and the plateau is wide.** The weight is now tunable without a rebuild (`MCC_SO_SPILL_SCORE=N` sets it; `=1` keeps the default 48; `0`/unset = off). The repo had no runtime benchmarks (`tests/bench` is a *compile-speed* corpus and its one program is malloc-bound), so the sweep used four compute-bound programs built for it — nbody, spectral-norm, mandelbrot, matmul — at -O4 with `MCC_SO_DEFAULT_SEED=1`, best-of-5 wall clock, output checked against the gcc reference every run:

| W | nbody | spectral | mandel | matmul |
|---|---|---|---|---|
| off | 1100 | 690 | 820 | 2110 |
| 0 (=off) | 1090 | 690 | 830 | 2110 |
| 12 | **870** | 690 | 820 | 2110 |
| 24 | 880 | 690 | 820 | 2100 |
| 48 | **870** | 690 | 830 | 2120 |
| 96 | 880 | 690 | 820 | 2100 |
| 192 | 880 | 680 | 820 | 2100 |
| 384 | 890 | 690 | 820 | 2120 |

Readings: (a) nbody gains **~20%** and every weight from 12 to 384 captures the whole win — the metric is a step function here, not a tuning knob, so 48 is safely mid-plateau; (b) the other three are flat to noise, i.e. **no benchmark regressed at any weight**, which is the property a default-on flip needs; (c) `W=0` reproducing the off numbers confirms the gate plumbing. For scale: gcc -O2 is nbody 360 / spectral 170 / mandel 290 / matmul 230 ms, so this closes nbody from ~3.0× to **2.4×** gcc and leaves the other three untouched — consistent with the FP/compute item's claim that the remaining gap is vectorization plus the field-offset addressing fold, not spills. Remaining before the flip: the sweep is single-host x86_64 and four programs; it wants the multi-arch golden-regen + seeds soak every other flip shares. Validated: default OFF ⇒ 498 objects at -O0/-O2 and -O4 objects on 3 sources byte-identical to HEAD, ctest 5589/5589, gate-on -O4 matches gcc on 6 fuzz seeds + the FP repros, all 5 arches build.

  **Historical — what a correct attempt needed** (kept because the failure modes recur): per-function accounting where only the FINAL committed emit counts — reset a per-function bucket at the start of each body emit and commit it once the winning body is in place (the natural hook is `ast_func_end`, which is where the final `ast_replay_body` lands; note there are several replay sites — mccast.c, ~15804, ~16139, ~16377). The counted event should be the actual rbp-relative operand emission (x86_64 `gen_modrm_impl`'s `VT_LOCAL` branch and its per-arch twins), not a `load`/`store` call. Validate any candidate metric by *correlating it against objdump's whole-object stack-memop count across BOTH a small hot TU and a cold-diluted one* — that pair is what catches the sign inversion, and neither the small TU alone nor `.text` size alone will. Until this lands, `-O4` still defaults to the smaller-`.text`, more-spills variant and the only way to get promotion at -O4 is to pin `MCC_AST_PROMOTE=1` explicitly (which follow-up (1) now honours). `so_jitscore`'s real wall-clock path (only active when the compile links an exe) remains the more faithful oracle when available.
- Get `ivsr`/`licm`/promote to actually FIRE on `advance()`. The hot fn is now reached by the scheduler (it appears in the ROI dump under `MCC_AST_OPASSIGN=1`, which makes it `ast_replay_ok` — previously it bailed in `ast_replay_ok` before the diff/search ever ran, so it kept un-optimized baseline code), but the loop passes still don't fire on it — the same "eligible-but-search-doesn't-select" mechanism as the math-inline.
- Wire ROI into the default -O4 path (currently needs `MCC_AST_SEARCH=1`, which the default out-of-process superopt doesn't set on its workers); flip on after the plb soak + an arm64 AOT/JIT-diff re-run.
- **Replace the ROI/search round-robin clock with an extremely-high-precision timer.** The per-function search budget/window is driven by `ast_now_ms()` = `(unsigned)((unsigned long long)clock() * 1000 / CLOCKS_PER_SEC)` (mccast.c, feeding `ast_search_start_ms`/`ast_search_budget_ms`/the `AST_SEARCH_WIN` duration window). `clock()` reports CPU time but its underlying resolution is coarse (`times()`/tick-quantised, ~1–10 ms on many hosts), so short per-candidate durations round to 0 ms and the stop-point lands on a different candidate from run to run — the search becomes wall-clock-nondeterministic. Observed concretely at -O4: whether a dense switch gets the jump-table dispatch (`switch_jt_dense`) flips between otherwise-identical runs, because the budget expires at a different point in the round-robin. Swap `ast_now_ms` for a monotonic high-res source — `clock_gettime(CLOCK_PROCESS_CPUTIME_ID)` (nanoseconds, CPU-time to keep the single-threaded-search semantics) with a `CLOCK_MONOTONIC` fallback — returning a wider `uint64_t` ns (widen `ast_search_*_ms` accordingly). **Constraint that killed the obvious fix before:** the asttool unit harness `#include`s mccast.c WITHOUT the mcchost timer (see the `ast_now_ms` block comment), so the new source must be self-contained or `#ifdef`-guarded with a `clock()` fallback — it can't just call into mcchost. Also keep the determinism guarantee the block comment at ~1850 relies on (the AOT==JIT invariant that a *count*-driven, not wall-clock-driven, order preserves — `clock()` timing already broke it once, commit `9c3d3930`): a finer timer reduces but does not remove timing-driven divergence, so pair it with, or gate it behind, the count-based ordering rather than treating precision alone as the fix.

### Other AOT
- **Plain C99 `inline` (non-`static`, non-`extern`) failed to link — FIXED 2026-07-26 behind `-fc99-inline-body` (default OFF ⇒ byte-identical).** `mcc -O2 vendor/plb/bench/algorithm/spectral-norm/3.c` gave `unresolved reference to 'A'` at every -O level. Per C99 6.7.4 a plain-`inline` definition supplies an inline definition with NO external definition, and mcc supplied neither: the body is parked in `inline_fns`, the emit decision `emit = !(t&VT_INLINE) || ((t&VT_STATIC)&&sym->c)` (mccgen.c) is 0, and the existing `diag_only` branch generates the body only to rewind the section and restore the ElfSym. Fix: when the gate is on and a referenced plain-inline symbol would have gone down `diag_only`, clear `VT_INLINE`, set `sym->a.weak`, and emit for real. **Clearing `VT_INLINE` is as load-bearing as setting `a.weak`** — `put_extern_sym2` maps `VT_STATIC|VT_INLINE` to `STB_LOCAL` and would otherwise bury the body where no other TU can reach it. Verified `readelf` reports `FUNC WEAK` and the kernel now links and prints gcc's `1.274224116`. **Weak rather than gnu89's strong definition is measurably the right call**: a two-TU program where both TUs include the same plain-inline header and take its address links cleanly and runs correctly under `-fc99-inline-body`, while the same program is a hard multiple-definition link error under `gcc -fgnu89-inline` AND under `mcc -fgnu89-inline` (`'addup' defined twice`). Note gcc *rejects* that program by default (`undefined reference to 'addup'`), so gcc is not a usable oracle for the multi-TU case — the expected values were checked by hand. Validated: gate-off byte-identical (504 exec+behavior objects at -O0/-O2 and the self-compiled amalgamation at -O0/-O2/-O3); ctest 5591/5591; asttool 747/0; all 5 arches build. `spectral-norm/3.c` is now the runtime-bench harness's 5th kernel (the harness gained per-kernel mcc flags for it) and measures **4.49x vs gcc -O2** on x86_64. Follow-ups: decide whether to adopt this as the DEFAULT (it makes mcc accept programs gcc rejects, which is a deliberate superset — the alternative is the strict C99 diagnostic); and the inliner question below.
- **`inline` functions are now inlined under `-fc99-inline-body` — FIXED 2026-07-26, and the fix is two lines, not the eager-capture project this item feared.** Measured payoff on plb spectral-norm: `A()` goes from 2 call relocs per hot loop to **0**, and the kernel drops **7.29x → 5.76x** instructions vs gcc (a 21% reduction), output matching gcc exactly. The route is not eager capture at all: `-fc99-inline-body` now strips `VT_INLINE` at DECLARATION time (mccgen.c, beside the existing `gnu89_inline` branch it mirrors) instead of parking the body in `inline_fns`, so the body is generated in place, `ast_inline_capture` has it in the pool when callers compile, and it is inlinable like any other function; `ad.a.weak = 1` keeps the out-of-line copy mergeable across TUs, which is the property the end-of-TU path provided. The second line relaxes `ast_fn_inlinable`'s `VT_STATIC` requirement for exactly this case (`c99_inline_body && sym->a.weak`) — C99 6.7.4p7 leaves it unspecified whether a call uses the inline definition or the external one, so using it is what the standard intends, and the weak body covers address-taken and un-inlined uses. **Note that same relaxation was tried alone earlier and measured INERT — it only matters once the body is captured, which is why the two changes are inseparable.** Validated: flag-off byte-identical (506 exec+behavior objects at -O0/-O2 and the self-compiled amalgamation at -O0/-O2/-O3), ctest 5906/5906, asttool 747/0, all 5 arches build, byte-identical self-host fixpoint, all runtime-bench kernels verify, the two-TU weak-merge case still links and runs correctly (`FUNC WEAK`, 9/17/35), and an inline battery (integer, FP, ternary, address-taken) matches a `static inline` gcc oracle at -O0/-O1/-O2/-O3 with the address-taken use correctly keeping one out-of-line call. Historical note follows.
- **(historical) `inline` functions were NEVER AST-inlined, on any arch, at any -O — measured 2026-07-26. The `VT_STATIC` gate in `ast_fn_inlinable` is NOT the reason, so the follow-up first filed here ("relax the VT_STATIC requirement") was mis-scoped; that relaxation was implemented, measured to change nothing, and REVERTED rather than shipped as a widened soundness condition with no benefit.** Minimal repro, same file, same -O: `static int sq(int x){return x*x;}` called twice is inlined (0 call relocs at -O2 and -O3), while `static inline int sq(...)` — one keyword different — is not (2 relocs). Plain `inline` behaves identically. The cause is capture ORDER, not eligibility: an `inline`-keyword body is parked in `inline_fns` and only generated in the loop at the end of `mcc_compile`, long after its callers are compiled, so `ast_inline_capture` has nothing in the pool when the call site is recorded. Relaxing `ast_fn_inlinable` cannot help a body that does not exist yet. Fixing it means capturing inline bodies eagerly (parse/record the parked body before its first caller, or defer callers) — a real change to compilation order, not a predicate edit. **Feasibility checked 2026-07-26: the parking site is contained** (mccgen.c `decl()`, `if (sym->type.t & VT_INLINE)` → `skip_or_save_block(&fn->func_str)`) **but capturing there means running `begin_macro`/`next`/`gen_function` re-entrantly in the middle of `decl()`**, where the end-of-TU loop that does the same thing today runs safely from `mcc_compile` at top level. That re-entrancy (parser `tok`/`file`/macro-stack state) is the risk, and it is why this is a focused project with a heavy fuzz+self-host soak rather than a drive-by; and it is what would let `-fc99-inline-body`'s weak body become the FALLBACK it is meant to be rather than the only path. Worth doing: it is the difference between a call per element and none in every C99-inline-style hot loop. **A second, independent blocker on the spectral kernel:** `mult_Av` and `mult_Atv` — the two functions that call `A(i,j)` — are `unfaithful`, so no AST optimization runs on them at all regardless of the inliner (this is the shared replay-fidelity gap set under Tests/infra, not a new defect). Both must be fixed before spectral-norm's 4.49x moves. Process note: the first measurement of this used `grep -c 'R_X86_64_PLT32\s+A$'`, which silently reports 0 because objdump prints the addend (`A-0x4`) — the anchored pattern made "not inlined" look like "inlined". Count call relocs with the addend included.
- **mcc's linker reports a duplicate definition but exits 0** (found 2026-07-26 while testing the above): `mcc a.o b.o -o x` with `addup` defined strongly in both prints `error: 'addup' defined twice` and still writes the output binary with exit status 0. A build script keying off the exit code sees success. Audit the linker's error paths for the same pattern — this is the false-green class the skip-audit item under Tests/infra targets.
- **riscv64 `-O1+` crashed on ANY float negation — FIXED 2026-07-26.** `double f(double d){return -d;}` aborted the compiler with `riscv64-gen.c: store: Assertion 'sv->r & VT_LVAL' failed` at -O1/-O2/-O4; -O0 was fine, and a build without `MCC_CONFIG_OPTIMIZER` was fine at every level, so it was the AST replay, not riscv64 codegen. Pre-existing (reproduced at 6f03a991). Cause: riscv64 is the only supported target on the `#else` arm of `gen_negf` (mccgen.c) — x86_64/i386/arm64 alias it to `gen_opf`, arm uses `0 - x` — and that arm has no hardware negate, so it flips the sign bit in memory via `save_reg(gv(...)); vdup(); incr_bf_adr(size-1); vdup(); vpushi(0x80); gen_op('^'); vstore()`. Those calls are backend-synthesized but still run through the recorder hooks, so the AST captured the byte-address sign-flip as though the program had written it; the replay re-emitted it against a value that was no longer an lvalue. **Same class as the `gaddrof` synthetic-add bug fixed earlier the same day (REGDISP §) — backend-synthesized vstack operations leaking into the recorder. Any future codegen that fabricates vstack ops must either suppress capture (`ast_hook_synth_begin`/`_end`) or model the whole operation as one node.**  Fix here is the conservative one: `ast_hook_bail()` at the top of that `gen_negf`, so a float-negating function keeps its byte-faithful baseline and merely forgoes AST optimization. **Effect: the riscv64 cross-compiler now compiles the entire mcc amalgamation at -O2 (8.39 MB, 2038 functions) where it previously aborted** — a prerequisite for the riscv64 Tier-3 self-host item below. x86_64/i386/arm64/arm are untouched by construction (different `gen_negf`), confirmed byte-identical at -O0/-O2/-O3 on the self-compiled amalgamation. Validated: ctest 5589/5589, asttool 747/0, all 5 arches build, and under qemu vs `riscv64-linux-gnu-gcc -ffp-contract=off` a dedicated negate battery (scalar/nested/loop/compound, double+float, -0.0 and 1e308 edges) matches at -O0/-O1/-O2/-O4 plus a 120-program randomized differential 240/240 at -O0/-O2 with 0 gate-off baseline failures. Follow-up: model the negate as a single unary AST node so those functions become optimizable again instead of bailing (they were never optimized before — they crashed — so the bail is not a regression).
- Gate landscape (for "enable all gates by default"): most optimization transforms already default ON at -O2+. The default-OFF gates fall in three classes — (i) diagnostic dumps/reports (`*_DUMP`, `*_REPORT`, `DATA_REEMIT`, `ROI_DUMP` — must stay off; debug noise, not codegen); (ii) search-mode toggles (`SEARCH`, `SEARCH_ORDER(ED)`, `ROI`, `SLICE`, `COST`, `PERFN_INPROC`, `SEARCH_THREADS/PTHREADS` — several mutually exclusive); (iii) unproven loop transforms (`INTERCHANGE`, `FUSION`, `TILE` — off pending correctness proof). So the safe maximal reading of "enable every gate" is "enable the proven optimization transforms," most already on at -O2.
- `MCC_AST_DIVMAGIC` item (b): optimal single-multiply form for the signed / `a==1` cases (needs real temp-materialization; the C path still uses the dup-based 2× mul-high).
- Fix the emit-time value-axis framework full-state save/restore (promotion-plan arrays + `nocode_wanted` + register-allocator/`vtop` state); then enable the inline + promote search axes.
- Memo unification: migrate to `ComboMemo` + disk backing with a bulk-value compression mode; subsume the out-of-process superopt.
- PR-C loop-IV monotonicity widening into `ast_vlat_context_at`, op-3/op-5 for-loops only; held until x86 fuzz soaks clean.
- Predicate-vector 4th side-car index; `context_in`/`context_out` memo-key consumer; descendant-indexed (DFS enter/exit) def/use extension for subtree-scoped write queries.
- Flip `MCC_AST_VLAT` default-on; signed `/ %` (INT_MIN/−1 trap), `<<` value-count.
- Flip `MCC_AST_NARROW_ELIM` default-on: flow-sensitive facts + globals.
- Flip `MCC_AST_ARGFWD` default-on; widen past single-use. (Note: ARGFWD only fires via the replay-time inline-graft path behind `ast_inline_env` (default `optimize>=3`), so flipping it has NO effect at plain -O2 — only under graft-inline (-O3/`MCC_AST_INLINE`).)
- Flip `MCC_AST_SPILL_SHARE` default-on; general per-value spill slots; riscv64 fixpoint.
- Flip arm64 `opt_promote` default-on after the tens-of-thousands-of-seeds soak; PR-3 callee-saved float pool v8–v15.
- riscv64 register promotion is DONE (GP `s1..s11` + float, qemu differential 0-fail); remaining: the callee-saved float pool `fs0-fs11` (mirroring arm64's deferred `v8-v15`). Default-on flip shares the global promotion soak.
- Promotion arch scope (decision): arm (armv7) is register-adequate (`r4-r11`/`d8-d15`) but has a large hidden-libcall surface (`__aeabi_idiv` — no baseline hw divide, 64-bit `long long`, soft-float) ⇒ the callful-forcing hazard analysis must be far broader than arm64's single long-double case; defer behind a libcall-hazard audit. i386 is out of scope — `MCC_NB_REGS 5` models only eax/ecx/edx + ~3 callee-saved GP, no callee-saved SSE; marginal payoff at the highest miscompile risk.
- Replace the `ast_plan_promotion` heuristic with the coloring allocator; fixpoint-gated + native arm64/riscv64.
- Large direct-copy struct-by-value inline capture: the Tier-4 inline now grafts the indirect (hidden-pointer, >16B) large-struct-by-value class on all arches. arm + i386 remain a SEPARATE gap — their >16B struct-by-value is a *direct* stack copy (VT_LOCAL, like x86_64 SysV MEMORY), NOT the hidden-pointer class, so it doesn't go through this path and stays retained. Needs a distinct "large direct-copy arg" capture model.
- MCC_CPU-gated `ast/replay-inline` / `-inline-spec` fixtures aren't widened for riscv64/i386/arm (dormant — no riscv64/i386/arm-host CI build, and `inline.c` main desyncs on riscv64); coverage is the docker ctests. Widening any MCC_CPU gate needs a host-arch CI build to exercise it (currently only x86_64 + arm64 hosts run).
- **riscv64 Tier-3 self-host — CLOSED 2026-07-26.** The 3-stage fixpoint holds byte-identically: **o1 == o2 == o3 (2664481 B)**, where stage0 is a gcc-built host tool emitting riscv64, stage1 is that tool compiling `src/mcc.c`, and **stages 2 and 3 are the mcc-built riscv64 mcc running under qemu-riscv64** — so this is a real cross-arch gate, not a host-arch one. The `gen_negf` crash above was the blocker; once the amalgamation compiled, the fixpoint held first try. Harness: `tools/selfhost-riscv64-docker.sh` (debian:bookworm-slim + gcc-riscv64-linux-gnu + qemu-user, ~30 s), ctest `selfhost-riscv64-docker`, `SKIP_RETURN_CODE 77` when docker is absent. Extra args are passed as env to every stage so gate sets can be soaked; `MCC_AST_RELOC_EQUIV=1` also reaches a byte-identical fixpoint (2712641 B). The script also requires mcc1 to build a *running* program matching gcc at -O0/-O1/-O2, because a fixpoint alone proves stability, not correctness. **Two traps this hit while being written, both worth knowing before touching it:** (a) the riscv64 sysroot includes must come AFTER the project ones or the system `elf.h` shadows `src/formats/elf.h` and the build dies on `R_RISCV_SET_ULEB128`; (b) a crashed stage leaves the previous run's object in place, so a naive compare reports FIXPOINT off stale files — the first version of this run did exactly that and reported a green 12-gate fixpoint that had actually core-dumped. Every stage now removes its output first and aborts if a stage produced nothing. **The test is registered OUTSIDE `if(TARGET mcc-riscv64)`** (the block holding `riscv64-promote-docker` etc., which is not configured in the plain debug build) because the script builds its own stage0 from source in-container; gating it there would have silently dropped the only cross-arch self-host gate. Remaining for full riscv64 Tier-4 parity is the separate list under Cross-arch parity (replay-inline ungate, JIT stub tail, ASan enhancements).
- **Investigate `AST_Poison` prevalence + reconciliation.** `AST_Poison` is a tombstone, not a construct: the columnar arena (`struct AstArena`) has no free list, so any pass that deletes a node re-kinds the slot to `AST_Poison` and `ast_clear_children`s it rather than removing it. Five planters today, all in `mccast.c`: narrow-lift (`ast_narrow_make` ~7250, poisons the wrapper after hoisting `inner`), DSE (~8070, redundant local store), SCCP branch-fold (~8123, dead `If` when the taken arm is empty), jump-thread/if-simplify (`ast_jt_*` ~8359, both arms empty), and the generic `ast_bf_drop` (~9329, bitfield lowering). Every downstream pass then pays to walk-and-skip these dead slots, and each one is serialized as a live node (a `kind` u16 + full payload row) in `mccjit_intent_serialize`'s node stream and re-materialized on deserialize — so tombstone density inflates both per-pass walk cost and blob size. Measure it first: instrument for post-pass Poison count per function / per TU (and as a fraction of live nodes) across the amalgamation self-compile + the exec corpus, and bucket by planter so the dominant source is named the way the desync gap set is. Then determine how many can/should be eliminated by (a) passes that rewrite-in-place instead of tombstoning where the slot could be reused, (b) an arena compaction/reconciliation step (renumber live nodes, drop Poison, remap `parent`/`first_child`/`last_child`/`next_sib`/child-index columns — the same index-remap `ast_slice_extract` already does) run before serialize and/or between pass phases, and (c) skip-index or generation-tagged querying so walkers don't re-scan tombstones. Open question the measurement settles: whether Poison is rare enough to leave alone (tombstoning is cheap and node-index-stable, which several passes rely on mid-transform) or common enough that a reconcile pass pays for itself. Node-index stability is the constraint — any compaction must run at a point where no live cursor/`AstLocal` is held across it.
- **riscv64 `MCC_AST_PROMOTE=1` aborted the compiler — FIXED 2026-07-26.** `freg: Assertion 'r >= 8 && r < 16'` turned out to be `freg(-1)`: a `get_reg` failure sentinel reaching `load()`/`store()` as a register id. Backtrace `ast_replay_bb -> gfunc_return -> vstore -> gfunc_call -> gv(MCC_RC_R(2)) -> load(r=-1)`. Cause: the riscv64 caller-saved promotion pool was `{2,3,4,5,6,7}` = **a2-a7, the ABI argument registers**, and the FP pool `{10,11,12,13}` = **fa2-fa5, likewise argument registers**. A function is only leaf as far as the AST can see — a struct copy or struct return lowers to a hidden `memcpy`, and `gfunc_call` materialises each argument with `gv(MCC_RC_R(n))`, a class containing exactly ONE register. Promotion pins it, `get_reg` has nothing to return, and -1 propagates. The size pattern is the tell: `struct M{int a;}` (4 B), `{int a,b,c;}` (12 B) and `{char a[24];}` crash while `{int a,b;}` (8 B) and `{long a,b;}` (16 B) do not — only the former need the memcpy. Fix: riscv64 has NO leaf pool (`AST_PROMO_CALLER_N`/`AST_PROMO_XMM_N` = 0), because the only caller-saved registers mcc models on this arch ARE the argument registers; leaves promote into the callee-saved pool via `MCC_AST_PROMO_LEAF_CALLEE`, whose per-reg `ast_promo_reg_is_callee` save/restore already covers a leaf holding s-registers. Restoring a real leaf pool needs register ids for t0-t6, and an FP pool needs fs0-fs11 (the deferred PR-3 callee-saved float pool) — neither is modelled. **This contradicts the "riscv64 register promotion is DONE (GP `s1..s11` + float, qemu differential 0-fail)" note below: that soak never exercised promotion over a TU containing a struct return.** Validated: byte-identical by default on every arch (promotion is opt-in off x86_64, and the change is inside `#if defined(MCC_TARGET_RISCV64)`) — x86_64 self-compiled amalgamation identical at -O0/-O2/-O3; ctest 5590/5590; asttool 747/0; all 5 arches build; the riscv64 amalgamation now compiles under `MCC_AST_PROMOTE=1` alone, with `+LEAF_CALLEE`, and with the full 12-gate set; **`tools/selfhost-riscv64-docker.sh` reaches a byte-identical 3-stage fixpoint under all three** (2679281 B promote, 2679281 B 12-gate — the set that previously core-dumped — and 2749921 B with LEAF_CALLEE+RELOC_EQUIV); and a 364-run riscv64 differential vs `riscv64-linux-gnu-gcc` (FP-constant, struct, register-pressure and a dedicated struct-return battery covering the 4/8/12/16/24-byte and 2-double return classes) at -O0/-O2 under both promote configurations, 0 failures, 0 gate-off baseline failures.

## Missing backend intrinsic lowering — 21 helper calls where gcc AND clang emit inline code (found 2026-07-28 building GCC with mcc)

Building the GCC 17 trunk tree with a self-hosted `mcc -O3` as CC exposed the complete set. One probe TU (`extern void *alloca()`, the whole `__builtin_{clz,ctz,ffs,clrsb,popcount,parity,bswap}*` family, `__atomic_{load,store,exchange,compare_exchange,fetch_add}_n`, `__sync_lock_test_and_set`) compiled at `-O2` leaves these undefined symbols:

| compiler | undefined after `-O2 -c` |
|---|---|
| `gcc` | `__popcountdi2` (one, and it is a real libgcc symbol) |
| `clang` | none |
| `gcc`/`clang` with `-march=native` | none |
| **`mcc`** | **21** — `alloca`, `__builtin_{bswap16,bswap32,bswap64,clrsb,clz,clzl,clzll,ctz,ctzl,ctzll,ffs,parity,popcount,popcountll}`, `__atomic_{load_4,store_4,exchange_4,compare_exchange_4,fetch_add_4,fetch_add_8}` |

Two reasons this is worth more than the usual code-quality argument. First, most of those names resolve only in `libmccrt.a`, so an mcc-compiled `.o` handed to a FOREIGN driver is unresolvable (**the atomics are the exception — they already are the libatomic ABI and link fine with `-latomic`; see the INVESTIGATE section below for the per-family breakdown**) — building GCC needed eight `ar x libmccrt.a` members (`alloca builtin atomic stdatomic mccrt complex float128 va_list`, checked collision-free) put on `LDFLAGS` as plain objects, because `g++` links the mcc-built C libraries there and never adds mcc's runtime. `__va_arg` was in that set until 2026-07-28 (77735a9f made it a `static __inline` in the predefs); the rest are the residue. Second, `__builtin_clz` and friends are not names any other runtime provides, so the dependency is invisible to anyone reading the object for portability.

The acceptance bar for each item below is the M8 bar plus: gate-off byte-identity, the probe TU losing that symbol, and a differential run of the affected builtin against BOTH gcc and clang including the UB-adjacent edge cases (`clz(0)`/`ctz(0)` are undefined — match what the hardware instruction actually leaves behind rather than inventing a value, and keep the constant-folding path's answer consistent with the runtime path's).

### ~~`alloca` — the inline path already exists, the call just does not use it~~ — DONE 2026-07-28
`gen_alloca_inline` in `gfunc_call` (`x86_64-gen.c`) takes `alloca(n)` before the call is built: round the request
up to 16, `sub %rax,%rsp`, `mov %rsp,%rax`. DEFAULT-ON; `MCC_ALLOCA_INLINE=0` opts out.

Both traps this section recorded turned out to be answerable rather than blocking:
- **Lifetime.** The block must live to the function epilogue, so — unlike a VLA — it is deliberately NOT registered
  in `cur_scope->vla`, and nothing reclaims it early; `leave` restores `rsp` from `rbp`. The corpus cell pins this
  with a block that allocates inside a nested scope and reads the memory after the scope closes.
- **`alloca` in an argument list.** `f(alloca(n))` works because mcc evaluates every argument expression onto the
  vstack BEFORE `gfunc_call` emits any stack traffic, and because locals and spills are `rbp`-relative, so moving
  `rsp` underneath them is safe. Covered directly by the cell.

It declines when bounds checking is on — that path needs the real call so `__bound_alloca_nr` can record the block —
and the mccrt `alloca` stub stays for PE/bcheck and older objects, exactly as this section asked.

Validated: gate-off byte-identical over the corpus at `-O0`/`-O2`; the cell (argument-list use, a loop, nested
scopes, use-after-scope, and a 4 KB block) matches gcc at `-O0`/`-O1`/`-O2`/`-O3` with the gate both ways; full
ctest 7483/7483 in both states; self-host fixpoint byte-identical; 60-seed differential fuzz vs gcc + clang with
`--gates`, 0 miscompiles.

Flip checks beyond the gated ones: the bounds-checked path really does keep the call (with `-b` the object carries
`__bound_alloca`, verified rather than assumed — an earlier check used a flag mcc ignores and was vacuous), the
four cross targets still emit `alloca` since this is x86_64-only, and the corpus cell runs correctly under
`qemu-i386` and `qemu-arm`. Its `alloca` declaration uses `__SIZE_TYPE__` rather than `unsigned long`, which is
what makes it portable to the 32-bit targets at all — with `unsigned long` it is a redefinition conflict there.

**With this the section's probe is 21 -> 0. mcc's object for that TU now has no undefined helper at all, where gcc
still needs `__popcountdi2`.**

### Bit builtins — `clz`/`ctz`/`ffs`/`clrsb`/`popcount`/`parity`/`bswap`

**`bswap` is DONE 2026-07-28 on x86_64 and DEFAULT-ON (`MCC_BSWAP_INLINE=0` opts out).** It is the one member of this
family that needs NO `-march` work: `bswap` is 486-baseline and the 16-bit form is `rol $8`, so there is no ISA
floor to raise. `gen_bswap(size)` (`x86_64-gen.c`) emits `rol $0x8,%ax` for 2 bytes and `0f c8+r` — with REX.W for
8 — for 4 and 8, and `unary()` calls it instead of `vpush_helper_func` when the gate is on. The probe criterion
this section states is met: the three `__builtin_bswap{16,32,64}` UND symbols disappear from the object, leaving
only `printf`.

Only x86_64 inlines it: i386, arm, arm64 and riscv64 still emit the helper call (verified — their objects keep the
three UND symbols), so this does not change any cross target and the arm64 cross self-host fixpoint is unchanged
and byte-identical. Validated before the flip with gate-off 789/789 corpus objects byte-identical at
`-O0`/`-O2`/`-O3`, and after it with full ctest 7394/7394. A new corpus cell
(`tests/exec/codegen/bswap_inline.c`) checks the fixed edges (0, all-ones, `0x00ff` at each width) plus 200
xorshift values per width AND asserts the round trip is the identity, matching gcc at `-O0`/`-O1`/`-O2`/`-O3` with
the gate both off and on; full ctest 7394/7394 both ways.

**`clz`/`ctz` are DONE 2026-07-28 too, DEFAULT-ON (`MCC_BITSCAN_INLINE=0` opts out), and they were NOT blocked on
`-march` either.** The blocking claim below is about `lzcnt`/`tzcnt`/`popcnt`; `BSR`/`BSF` are 386-baseline and give
both operations directly — `ctz` is `BSF`, and `clz` is `BSR` followed by `xor $31` (or `$63`), because
`31 - idx == idx ^ 31` for a 5-bit index. Both are undefined at zero, which is exactly what `__builtin_clz`/`ctz`
document, so the sequence needs no zero guard and raises no ISA floor. `gen_bitscan(ctz, size)` in `x86_64-gen.c`.

Validated: gate-off 792/792 corpus objects byte-identical at `-O0`/`-O2`/`-O3`; a new self-checking corpus cell
(`tests/exec/codegen/bitscan_inline.c`) walks every single-bit position at both widths and 300 xorshift values
against shift-loop reference implementations — so it fails on a wrong answer even where the golden output would
not change — matching gcc at `-O0`/`-O1`/`-O2`/`-O3` in both gate states; full ctest 7416/7416 both ways; the four
`__builtin_{clz,ctz,clzll,ctzll}` UND symbols disappear, mcc's own object now has **zero** `clz`/`ctz`/`bswap`
helper references, and both the native and arm64 cross self-host fixpoints are byte-identical.

**`ffs` is DONE 2026-07-28 as well**, on the same `MCC_BITSCAN_INLINE` gate: `BSF`, then `CMOVZ` a `-1` scratch in
for the zero case, then increment. Two encoding details worth keeping — the `mov $-1` and the `CMOV` run 32-bit on
purpose (the answer is at most 64, and REX.W on `B8+r` makes it a 10-byte `movabs`), and the scratch comes from
`get_reg(MCC_RC_INT)` so it cannot collide with the operand.

**The probe from the head of this section is now 21 -> 10.** Remaining: the five `__atomic_*_4` calls, `alloca`,
`__builtin_popcount`/`popcountll` (genuinely SSE4.2), `__builtin_parity` and `__builtin_clrsb`. `parity` and
`clrsb` are baseline-implementable but multi-instruction (xor-fold, and `clz(x ^ (x >> W-1)) - 1` with a zero
case), so they are worth less than the atomics, which are both the biggest remaining group AND the one where the
inline form is a large speed win rather than a code-size one.

**`__atomic_fetch_add`/`fetch_sub` are DONE 2026-07-28, DEFAULT-ON (`MCC_ATOMIC_INLINE=0` opts out), sizes 4 and 8.**
`lock xadd` leaves the PREVIOUS contents in the value register, which is exactly fetch semantics, and LOCK already
implies seq_cst on x86 so the memory-order argument needs no extra fence. `fetch_sub` is the same instruction with
the operand negated first. Sizes 1 and 2 stay on the helper: the byte form needs a REX prefix just to name
`sil`/`dil`, which is not worth a special case. `gen_atomic_xadd(size)` in `x86_64-gen.c`, hooked in `parse_atomic`
just before the `snprintf`-built helper call.

Verified as a THREADED test, not only single-threaded arithmetic — `tests/exec/codegen/atomic_fetch_inline.c` runs
4 threads x 50000 interleaved add/sub pairs plus a per-thread add, and the total is deterministic (28 and 400000)
only if every operation is atomic. It matches gcc with the gate on and off, at `-O0` and `-O2`. Gate-off is
byte-identical over the corpus and the whole suite is 7439/7439 in both states; `__atomic_fetch_add_4` and
`_8` disappear from the probe object.

**`__atomic_load_n`, `__atomic_store_n` and `__atomic_exchange_n` are DONE 2026-07-28 on the same gate**, sizes 4
and 8. Load is a plain `mov` (x86 loads are already acquire, and seq_cst needs nothing more on the load side);
store and exchange are both `XCHG r, m`, which is implicitly LOCKed — that is the trap this section already
recorded, and it is why the store does NOT lower to a bare `mov`. mcc's emitted form matches gcc's instruction for
instruction (`xchg %ecx,(%rax)` / `xchg %rcx,(%rax)`).

**A guard the first cut got wrong, worth keeping:** the size test alone is not enough. `_Atomic struct {char a,b,c,d;}`
is size 4 and `_Atomic float` is size 4, and both were taken by the inline path, which then ran `gv2(MCC_RC_INT, …)`
on a struct or an xmm value — `test_atomic_store_struct` printed `4 0 0 0` instead of `1 2 3 4`. The path now also
requires the atom's base type to be an integer, pointer or bool. Floats and structs keep the helper and are
byte-for-byte unchanged.

**`popcount` and `parity` are DONE 2026-07-28, DEFAULT-ON (`MCC_POPCOUNT_INLINE=0` opts out) — and NOT as x86_64
machine code.** They are lowered in the PARSER as an ordinary expression through `gv_dup`/`vpushi`/`gen_op`: the
standard SWAR fold (`x -= (x>>1)&0x55..`, `x = (x&0x33..) + ((x>>2)&0x33..)`, `x = (x + (x>>4)) & 0x0f..`,
`(x * 0x0101..) >> (W-8)`), with parity taking `& 1` of the result. That is what gcc emits with `-mno-popcnt`, it
needs no new encodings, and — the reason it is worth more than an x86 emitter — **it removes the helper on EVERY
target**: arm64, riscv64, i386 and arm objects lose the four UND symbols too, and the i386 and arm builds run the
new corpus cell correctly under qemu.

So the SSE4.2 `popcnt` note below is about the one-instruction form; the portable lowering does not need `-march`
at all and can ship before it.

Flip validation: full ctest 7461/7461, native self-host fixpoint byte-identical, all 53 `jit/*` cells, an 80-seed
differential fuzz vs gcc + clang with `--gates` and 0 miscompiles, and the arm64 AND riscv64 cross self-host
fixpoints byte-identical — the cross legs matter here in a way they did not for the x86_64-only gates, because this
lowering changes every target's code.

**`clrsb` is DONE 2026-07-28 on the same `MCC_POPCOUNT_INLINE` gate, portable, using the recipe the popcount work
produced.** It is `clz(x ^ (x >> (W-1))) - 1`, and the awkward part was that portable `clz` was itself missing —
popcount supplies it: smear the value down (`x |= x>>1; x |= x>>2; … x |= x>>16`, plus `>>32` at 64) and then
`clz(x) = W - popcount(x)`. Substituting gives a branch-free `clrsb` with NO special case:

    clrsb(x) = (W - 1) - popcount(smear(x ^ (x >> (W-1))))

and it is right at the boundaries by construction — `x = 0` and `x = -1` both smear to 0, giving `W-1`, which is
what both should return. Verified against a bit-walking reference over every single-bit position (positive and
negated) and 200 xorshift values at both widths.

**`__atomic_compare_exchange` is DONE 2026-07-28** — `gen_atomic_cmpxchg(size)`, sizes 4 and 8, on the same
default-on gate. Emitted shape:

    mov  (%rsi),%eax          ; expected
    lock cmpxchg %ecx,(%rdi)  ; desired, ptr
    sete %dl                  ; capture ZF FIRST
    je   1f
    mov  %eax,(%rsi)          ; failure write-back
    1: movzbl %dl,%edx

Three things had to be right, and two of them were wrong on the first attempt:
- **`vrott` vs `vrotb`.** `vrott(3)` sends the TOP to the bottom (`[a,b,c] -> [c,a,b]`); bringing the pointer up
  from `vtop[-2]` needs `vrotb(3)`. The first cut read the register of the wrong operand.
- **Everything must be pinned to distinct registers.** `get_reg(MCC_RC_INT)` handed back `%rax` for the `sete`
  scratch — the one register CMPXCHG uses implicitly — so the scratch clobbered the expected value and the failure
  write-back stored the boolean. Operands are now pinned with `gv(MCC_RC_RCX)`, `gv(MCC_RC_RSI)`, `gv(MCC_RC_RDI)`
  and the scratch is `%rdx` after `save_reg`, in the style of `gen_opi`'s divide path.
- **ZF must be captured before the branch**, since the failure path's `mov` comes after it.

The test is a SPINLOCK, not arithmetic: 4 threads take a CAS lock 20000 times each and add to a shared counter, so
the total (240000) is only reachable if the CAS provides real mutual exclusion; the single-threaded block pins the
`expected` write-back on a FAILED exchange, which is the part a naive lowering drops.

**The probe from the head of this section is now 21 -> 1, and the only entry left is `alloca`** (its own subsection
above). For reference, gcc on the same TU still needs `__popcountdi2`.

**Flip validation (2026-07-28), for the whole `MCC_ATOMIC_INLINE` set:** full ctest 7439/7439, native self-host
fixpoint byte-identical, all 53 `jit/*` cells, an 80-seed differential fuzz vs gcc + clang with `--gates` and 0
miscompiles, and the arm64 cross self-host fixpoint byte-identical. Cross targets are untouched by construction and
by check — i386, arm, arm64 and riscv64 objects still carry the `__atomic_fetch_*` UND symbols.

`__atomic_compare_exchange` is the one left, and it is the hardest of the group because `CMPXCHG` pins the expected
value in `%rax` and reports through ZF. The shape to copy is `gen_opi`'s divide path, which already does exactly
this kind of pinning: `gv2(MCC_RC_RAX, MCC_RC_RCX)` plus `save_reg(MCC_TREG_RDX)`. The sequence is: load `*expected`
into `%rax`, `lock cmpxchg desired, (ptr)`, `sete` the bool result, and on failure store `%rax` back through the
expected pointer — that write-back is the part a naive lowering forgets, and it is observable, so any test must
check `expected` after a FAILED exchange, not just the return value.

Encodings for the REST of the atomics, all lock-free for sizes 1-8 on x86_64: `__atomic_load_n` at any ordering
up to seq_cst is a plain `mov` (x86 loads are acquire); `__atomic_store_n` at seq_cst must be `xchg` (implicitly
locked) or `mov`+`mfence`, NOT a bare `mov`; `__atomic_exchange_n` is `xchg`; `__atomic_fetch_add` is
`lock xadd`; `__atomic_compare_exchange_n` is `lock cmpxchg` with the expected value in `%rax` and the result
taken from ZF.
The fallback is arch-generic, not a per-backend omission: mccgen.c constant-folds when the argument is `VT_CONST` (`fold_bit_builtin`) and otherwise does `vpush_helper_func(btok)` + `gfunc_call(1)` at mccgen.c:9404 (the clz/ctz/ffs/clrsb/popcount/parity family) and :9438 (the bswap family), calling into `runtime/lib/builtin.c`. That means ALL FIVE ARCHES pay a call for `x86 bsr/bsf/popcnt/bswap`, `arm rbit/clz/rev`, `arm64 clz/rbit/cnt/rev`, `riscv64 Zbb clz/ctz/cpop/rev8`. Suggested order — `bswap` first (unconditional single instruction everywhere, no UB corner, smallest blast radius), then `clz`/`ctz` (x86 `bsr`/`bsf` need the zero-input contract pinned down), then `ffs`/`clrsb`/`parity` (compositions of the former), and `popcount` last because it is the one case where gcc itself still calls a libgcc helper (`__popcountdi2`) without the ISA bit.
**This is ISA-conditional and therefore blocked on, or at least coupled to, the `-march` section above** (`popcnt` needs SSE4.2/ABM, `lzcnt`/`tzcnt` need BMI1, riscv64 needs Zbb). Baseline x86_64 still gets `bsr`/`bsf`/`bswap` unconditionally, so the `-march` dependency only gates the last increment, not the whole item. Note `gcc -march=native` inlines every one of the 21, which is the real target state.

### Lock-free atomics — `__atomic_load_N`/`store_N`/`exchange_N`/`compare_exchange_N`/`fetch_OP_N`
mcc builds the libcall name as a string and calls it (`snprintf(buf, ..., "__atomic_compare_exchange_%d", size)` + `vpush_helper_func(tok_alloc_const(buf))`, mccgen.c ~6656; `atomic_store_needs_libcall` and the `base = "__atomic_fetch_add"` table at ~6524 are the same shape). For sizes ≤ pointer-size on x86_64 every one of these is a single instruction — plain `mov` for relaxed/acquire/release load and store, `xchg` for exchange, `lock cmpxchg` for CAS, `lock xadd` for fetch_add — and gcc/clang emit exactly that. The x86_64 backend has no `gen_atomic` entry point yet, so this needs one plus the memory-order argument actually being consulted (mcc currently hardcodes `vpushi(0x5)` for both success and failure orders at the CAS site, which is `seq_cst` — correct but pessimistic).
Two traps worth writing down before starting: `__atomic_store_n` at `seq_cst` needs `xchg` or a trailing `mfence`, not a bare `mov`; and the sub-word (`_1`/`_2`) cases still need the helpers for the non-lock-free paths, so this is a fast-path addition, not a replacement. `__sync_lock_test_and_set` lowers to `__atomic_exchange_4` today — that is how libbacktrace's `mmap.c` pulled the symbol into the GCC build.
Related gotcha already paid for once: `__atomic_exchange_N` lives in `stdatomic.o`, NOT `atomic.o`, inside `libmccrt.a`.

### ~~`-Wl,--version-script` is accepted but does not restrict exports~~ — DONE 2026-07-28
`77735a9f` stopped `-Wl,-version-script` being a hard error; the file was recorded in `s->version_script` and never
read. It is now applied: `export_global_syms` (`mccelf.c`) consults the script and skips any DEFINED global that the
script does not list. Scope is the common subset — a `local: *;` wildcard plus an explicit `global:` list — and the
filter only engages when that wildcard is present, so a script without one keeps today's export-everything
behaviour rather than silently hiding more than GNU ld would.

Verified against GNU ld end to end, not just by symbol count: with `{ global: public_fn; local: *; };` mcc's `.so`
exports `public_fn` alone (gcc's exports the same one), a program calling `public_fn` links and runs, and one
calling `private_fn` fails with `undefined reference` exactly as it does against the gcc-built library. Without the
script both symbols are still exported. `cli/version_script_hides` locks both halves in one case.

## INVESTIGATE: make mcc objects linkable without libmccrt — the helper names are mcc-private (raised 2026-07-28)

Sibling of the intrinsic-lowering item above, and worth doing *even if* every intrinsic eventually gets inlined, because the residue (soft-float, `__mcc_*` overflow builtins, `_Complex`, `float128`, bcheck, tcov) will always need SOME runtime. The question here is not "how do we emit fewer calls" but "when mcc emits a call into its own runtime, how does a FOREIGN linker resolve it". Today the answer is: it does not, and the GCC build had to `ar x libmccrt.a` eight members onto `LDFLAGS` by hand.

**RE-MEASURED 2026-07-28, after the intrinsic section closed at 21 -> 0 — two of the three families are GONE.**
Compiling `tests/exec` + `tests/behavior` + mcc's own TU and collecting every undefined symbol, the residue that a
stock toolchain cannot resolve is now exactly the `__mcc_*` family, 15 names:

| group | names |
|---|---|
| overflow builtins | `__mcc_addo_{uc,s,i,u,ll,ull,ti}`, `__mcc_subo_{uc,i}`, `__mcc_mulo_{i,u,ll,ull,ti}` |
| complex arithmetic | `__mcc_cmul`, `__mcc_cmulf`, `__mcc_cmull`, `__mcc_cdiv` |
| misc | `__mcc_signbit` |

Everything else that survives IS a libgcc name and resolves against a stock toolchain: `__multi3`, `__udivti3`,
`__divti3`, `__ashlti3`, `__ashrti3`, `__lshrti3` and the `__fix*`/`__float*` conversions. The bit builtins and
`alloca` no longer appear at all, and the atomics are inlined rather than called.

So the item is no longer "three families" — it is one family, and the biggest group is now handled the same way the
atomics were.

**`__builtin_add_overflow`/`sub_overflow`/`mul_overflow` are inlined 2026-07-28, DEFAULT-ON
(`MCC_OVERFLOW_INLINE=0` opts out), widths 4 and 8, signed and unsigned.** They reach codegen as CALLS to the mccdefs `_Generic` dispatch helpers
(`__mcc_addo_i` and friends), not as builtin tokens, so the hook intercepts the call in `gfunc_call` the way the
`alloca` one does, reads the width and signedness off the RESULT POINTER's pointee type, and emits
`add`/`sub` + `seto` (signed, OF) or `setb` (unsigned, CF) + the store. Widths 1 and 2 keep the helper.

One contract detail that cost a debugging round and is worth stating: `gfunc_call` must pop the callee AND every
argument and leave the result where the CALLER's `vsetc(&ret.type, ret.r, …)` expects it — `%rax` for an
int-returning helper — not push a value of its own. Pushing one gives `internal compiler error: vstack leak`.

Multiply needed one extra shape: signed is `imul r, r/m`, which sets OF like `add` does, but unsigned has no
two-operand form — it is the one-operand `mul r/m` with the multiplicand pinned in `%rax` and the high half landing
in `%rdx`, so that path pins `a -> %rax` and moves the flag scratch to `%rsi` (it cannot be `%rdx`). The store has
to happen BEFORE the `movzx` writes the boolean into `%rax`, since for unsigned mul the product IS in `%rax`.

**`__mcc_signbit`/`signbitf` are inlined 2026-07-28, DEFAULT-ON (`MCC_SIGNBIT_INLINE=0` opts out).** `MOVMSKPS`/
`MOVMSKPD` put lane 0's sign bit in bit 0 of a GP register, so `__builtin_signbit` is that plus one `and $1`; both
are SSE2, so no ISA floor moves. x87 `long double` has no equivalent and keeps `__mcc_signbitl`.

Worth knowing before writing a test for it: **gcc's `__builtin_signbit` returns the raw sign BIT, not 1** — on a
negative float it yields `-2147483648`. That is conformant (the documented contract is "nonzero"), and mcc returns
0/1 both in the helper and inline, so a differential test must compare `!!signbit`, not the value. A first cut
compared against 1 and "failed" under gcc at both `-O0` and `-O2` while mcc passed, which reads like an mcc bug
until you print the number.

**CORRECTED MEASUREMENT 2026-07-28 (a stale count was reported twice in this section — this is the real one).**
Over `tests/exec` + `tests/behavior` + mcc's own TU the non-libgcc residue is **19 names**, not the 6 an earlier
pass claimed. The claim went stale when the operand-widening fix landed: widening forced the inline hook back to
width 8, so every narrow overflow helper returned to the object. What is actually left:

| group | names | why |
|---|---|---|
| overflow, widths 1/2/4 | `__mcc_addo_{sc,uc,s,us,i,u}`, `__mcc_subo_{sc,uc,i,u}`, `__mcc_mulo_{sc,i,u}` | operands are 64-bit now, so the inline needs a truncate-and-compare tail — attempted twice, reverted twice |
| overflow, 128-bit | `__mcc_addo_ti`, `__mcc_mulo_ti` | needs `add`/`adc` + `seto` on a register pair |
| complex | `__mcc_cmul`, `__mcc_cmulf`, `__mcc_cmull`, `__mcc_cdiv` | call-shape change, see below |

Everything else that survives IS a libgcc name and resolves against a stock toolchain: `__multi3`, `__udivti3`,
`__divti3`, `__ashlti3`, `__ashrti3`, `__lshrti3` and the `__fix*`/`__float*` conversions. The bit builtins,
`alloca`, `signbit` and the atomics no longer appear at all — those are the durable wins, and
`cli/intrinsics_no_helper_calls` keeps them that way.

**The complex helpers cannot be fixed by a RENAME — checked 2026-07-28, and this kills the cheapest option for
them.** mcc's are
`void __mcc_cmul(T *res, T a, T b, T c, T d)` — result through an out-pointer — while libgcc's are
`_Complex double __muldc3(double a, double b, double c, double d)`, returning the pair by value (SSE,SSE under
SysV, so `xmm0`/`xmm1`; the `long double` forms `__mulxc3`/`__divxc3` return in `st(0)`/`st(1)`). Adopting the
libgcc ABI therefore means changing the CALL SITE to consume a two-register return, not just the symbol name. mcc
already has the machinery — `gfunc_call`'s `ret_nregs` path handles multi-register returns — so this is a contained
change, but it is a call-shape change and needs its own differential test against gcc for the NaN/infinity fixups
those helpers exist to implement.

Flip validation: full ctest 7505/7505, self-host fixpoint byte-identical, all 53 `jit/*` cells, a 60-seed
differential fuzz vs gcc + clang with `--gates` and 0 miscompiles, and the arm64 cross sanity build. This is
x86_64-only, and the check confirms it: i386, arm, arm64 and riscv64 objects still carry
`__mcc_addo_i`/`subo_i`/`addo_u`/`subo_u`/`addo_ll`/`addo_ull`.

**The narrow widths landed and were then withdrawn 2026-07-28** — see the wrong-answer bug below, which made a
same-width inline unsound for them. Residue is **6 names**: `__mcc_addo_ti`, `__mcc_mulo_ti` (128-bit) and the four
complex helpers.

**A pre-existing WRONG-ANSWER bug surfaced while testing them, and it is now FIXED.** mcc's
`__builtin_add_overflow` is a `_Generic` dispatch in `mccdefs.h`, and the helper prototypes took the RESULT type
for the operands — so the operands were converted before the check and
`__builtin_add_overflow(-300, -300, &signed_char)` truncated both to -44 and reported NO overflow where gcc reports
overflow. It was not confined to exotic cases: `__builtin_add_overflow(4294967296LL, 0LL, &int_result)` was wrong
the same way.

The fix is one line of prototype per width: the narrow variants now take `long long`/`unsigned long long` operands
(`__MCC_OV_DECL_W`), and the helper bodies, which already computed in a wide type, simply stop re-truncating.
Verified against gcc on a 12-case matrix crossing operand and result widths and signedness — all 12 now agree,
where 2 disagreed before, plus the 4 extra cases that the matrix originally missed.

Consequence for the inline path: with wide operands and a narrow result, a single `add` + `seto` is no longer the
whole answer (the flag describes the WIDE add, and the fit still has to be checked), so the inline hook is now
limited to width 8, where operand and result widths coincide.

**Re-adding widths 1/2/4 was ATTEMPTED TWICE 2026-07-28 and reverted both times — do not start it from the recipe
alone.** The shape is: do the arithmetic at 64 bits (the operands ARE 64-bit now), capture `seto`/`setb` for the
wide op, `movsx`/`movzx` the low bytes back into a scratch, `cmp` against the wide result, `setne`, OR the two
flags, then store the low bytes. Both attempts were caught by the same 12-case matrix against gcc rather than by
the suite:
- **First attempt** emitted the arithmetic at the RESULT width (byte/word/dword `add`), which re-introduced exactly
  the truncation bug the prototype fix had just removed — the two originally-failing matrix cases came back.
- **Second attempt** fixed that to a 64-bit `add` but the truncate-compare tail then reported no overflow for every
  narrow case, taking the matrix from 2 failures to 9. The `setne`/`or` sequence or its register choices are wrong;
  it was not debugged further.

The value at stake is one call per narrow-result overflow check, and the width-8 case — which the common
`long`/`long long` idiom uses — is already inlined. Anyone picking this up should build the matrix FIRST and run it
after every emit change, because the suite passes in both broken states.

**Recipe for the narrow widths, kept because it is what made them mechanical.** Byte and word add/sub are the same
two-operand shape with different opcodes — byte `02 /r` (add) and `2a /r` (sub), word the 4-byte opcodes behind a
`0x66` prefix — and the stores are `88 /r` and `66 89 /r`. The one trap is that the operands are pinned to
`%rcx`/`%rsi`/`%rdi`, and **the byte forms of `sil`/`dil` require a REX prefix that `orex` will not emit on its
own** (it only fires for regs >= 8), so size 1 has to force `0x40 | REX_BASE(rm) | (REX_BASE(reg) << 2)`. Byte
MULTIPLY is different again — there is no two-operand `imul r8`, only the one-operand `F6 /5` form through
`%al`/`%ax` — so mul should stay at widths 4 and 8 unless someone wants that path too.

**A residual `alloca` was found and fixed while measuring this**: `__builtin_alloca` is declared in `mccdefs.h` as
`__builtin_alloca` with an `__asm__("alloca")` rename, so the inline hook's `sym->v != TOK_alloca` test missed it
and `tests/exec/features_c99_c11/builtins.c` still emitted the call. The hook now also matches `sym->asm_label`.

**First measurement (2026-07-28, superseded by the above but kept for the naming analysis) — the three families
were NOT equally bad:**

| family | name mcc emits | resolvable by a stock toolchain? |
|---|---|---|
| atomics | `__atomic_load_4`, `__atomic_store_4`, `__atomic_exchange_4`, `__atomic_compare_exchange_4`, `__atomic_fetch_add_{4,8}` | **YES — these already ARE the libatomic ABI.** `g++ mcc-built.o -latomic` links and runs, output identical to the gcc-built reference. Verified 2026-07-28. Nothing to rename; the gap is that mcc's driver knows to pull them from libmccrt and never tells anyone else `-latomic` would do |
| bit builtins | `__builtin_clz`, `__builtin_popcountll`, `__builtin_bswap32`, … (14 names) | **NO — these names exist nowhere but `libmccrt.a`.** libgcc's equivalents are `__clzdi2`/`__ctzdi2`/`__ffsdi2`/`__paritydi2`/`__popcountdi2`/`__clrsbdi2`/`__bswap{si,di}2` |
| `alloca` | `alloca` | **NO, and no rename can fix it** — glibc exports no `alloca` symbol at all (`nm -D libc.so.6` → 0 hits); it is a compiler builtin everywhere. Inline it or emit it into the object |

**Sharp edge found while checking the rename option:** on x86_64, libgcc ships only the **DI/TI** widths — `__clzdi2`, `__ctzdi2`, `__popcountdi2`, `__paritydi2`, `__ffsdi2`, `__clrsbdi2` (+ `ti2`) — and of the SI variants only `__bswapsi2`. There is no `__clzsi2`/`__popcountsi2`/`__ctzsi2` to link against, because gcc always inlines the 32-bit cases on this target. So a straight rename to the libgcc ABI fixes the 64-bit helpers on any GNU toolchain and leaves the 32-bit ones exactly as unresolvable as they are now. Check per target before assuming otherwise — arm/riscv libgcc do ship SI variants.

**Options to investigate, roughly cheapest-first. Not ranked yet; that is the point of the item.**
- **Say `-latomic` out loud.** Zero-risk, and it removes a whole third of the problem for anyone linking mcc objects with gcc/clang. Options: mention it in `mcc -hh`, or have `-print-file-name=libatomic.so` work (the `-print-*` options landed 77735a9f), or emit nothing and just document it.
- **Adopt the libgcc ABI names for the widths libgcc actually has.** Emit `__clzdi2` instead of `__builtin_clzll` and keep an mcc-side alias so existing objects still link. Must check semantics match, not just names: libgcc's `__clzsi2`/`__clzdi2` are undefined at 0 and return `int`, while `runtime/lib/builtin.c` may define the zero case — a silent behaviour change at the boundary is worse than an unresolved symbol. Also decide what happens to the SI widths that libgcc does not carry.
- **Emit the helper INTO the object when referenced**, as a local or weak definition, so the `.o` is self-contained. There is a working precedent: `__va_arg` became a `static __inline` in the injected predefs (77735a9f) and the symbol vanished from every object with zero codegen risk. That trick generalises to anything expressible in C — the whole bit-builtin family qualifies. It does NOT cover `alloca` (needs the caller's frame) and it costs per-object size, so measure the bloat on a real corpus before committing. Alternative shape: a real COMDAT/weak group so the linker dedups copies.
- **Ship `libmccrt` where a foreign driver can find it** — a `libmccrt.so`, and/or install the `.a` into a system library path, and/or a gcc-style spec/`.deplibs` hint. Note GNU ld ignores `.deplibs`, so that sub-option is probably dead on arrival; confirm before spending time on it.
- **Split the archive.** The GCC build needed exactly 8 of 12 members and had to check them for symbol collisions by hand. A curated `libmccrt-compat.a` (or a documented member list) that is safe to put on any link line would have turned a 40-minute detour into one flag. Cheapest real win after `-latomic`.
- **Inline the intrinsic** — see the section above. Makes the question moot per-builtin but never for the whole runtime.

**Evaluation criteria for whichever wins:** self-host stays byte-identical; no new symbol collisions against libc/libgcc/libstdc++ — **audited 2026-07-28 and currently clean**: across all 8 extracted members the only exported names are `__*` and the `atomic_*` C11 entry points, plus `alloca` itself. An earlier draft of this item claimed `runtime/lib/alloca.c` leaks a global `p3`, and a second draft blamed `builtin.o`'s `table_*`; **both were wrong** — `nm` reports `t p3` and `r table_1_32`, all local. Re-run the audit if members are added; works under gcc, clang, lld and mold, not just GNU ld; unchanged for `-run`/JIT, which resolves against the embedded runtime and does not care; and a PE/Mach-O answer, since `libmccrt.a` is equally private there.

**Reproducer for whatever lands:** compile the 21-symbol probe TU from the section above with mcc, link the object with `g++` and with `clang++` and nothing else, and require it to resolve.

## `-static-libgcc` / `-static-libstdc++` are hard errors — gcc AND clang accept both (found 2026-07-28)

```
gcc   -static-libgcc     OK          clang -static-libgcc     OK
gcc   -static-libstdc++  OK          clang -static-libstdc++  OK (warns "argument unused during compilation")
mcc   -static-libgcc     mcc: error: invalid option -- '-static-libgcc'
mcc   -static-libstdc++  mcc: error: invalid option -- '-static-libstdc++'
```

This is what made the GCC build unbuildable before anything was even compiled. GCC's top-level `configure` defaults `--with-static-standard-libraries=yes`, which puts `-static-libstdc++ -static-libgcc` into `LDFLAGS` for the host stage; every subdirectory `configure` then failed its very first link probe with the maximally unhelpful **"C compiler cannot create executables"**, and the real message was only visible in `libdecnumber/config.log`. Worked around with `--without-static-standard-libraries`; not fixed.

The fix is a two-line addition to the `MCC_OPTION_ignored` group in `mcc_options[]` (`libmcc.c`, alongside `-pipe`/`-C`/`--param`/`-traditional`), and accept-and-ignore is arguably the *semantically correct* answer rather than a stub: mcc's runtime is `libmccrt.a` and is already statically linked (or embedded via `MCC_EMBED_MCCRT`), so "link libgcc statically" is a request mcc trivially already satisfies. Decide whether `-static-libstdc++` should warn like clang does — mcc has no C++ mode at all, so an unused-argument warning is defensible, but silence matches gcc and is the safer default for configure probes that treat any stderr output as a failure signal.

While here, sweep for the rest of the same class. `-V` and `-qversion` were checked during the same session and mcc's rejection MATCHES gcc and clang (both error too), so those are correct as-is and should not be "fixed".

## Make "mcc builds GCC" a repeatable gate, not a one-off (raised 2026-07-28)

The GCC 17.0.0 trunk tree now builds to completion with a self-hosted `mcc -O3` as CC, `--enable-languages=c,c++`. This is the largest third-party C corpus mcc has been through and it found 7 real divergences (77735a9f) that the entire 7281-test ctest suite did not. Worth keeping.

**Recipe, so it does not have to be rediscovered:**
- Compiler: `--preset release -DMCC_EMBED_JIT=OFF -DMCC_CONFIG_JIT=OFF -DMCC_BUILD_TESTS=OFF`, then a 3-stage self-host at `-O3` keeping the stage-2 binary. Put the binary INSIDE the build dir so `<exe>/include` auto-mccdir resolves.
- `CC=<mcc-o3> CXX=g++ CFLAGS=-O2 CXXFLAGS=-O2 CC_FOR_BUILD=<mcc-o3>`, `--disable-bootstrap --disable-multilib --disable-werror --disable-nls --without-static-standard-libraries`.
- `LDFLAGS` = the 8 `ar x libmccrt.a` members as plain object paths (see the INVESTIGATE section). Plain `.o` files link regardless of position; a static archive in `LDFLAGS` sits before the objects and resolves nothing.
- `--disable-bootstrap` is load-bearing: a bootstrap would rebuild stages 2/3 with the just-built `xgcc` and stop testing mcc after stage 1.

**Measured 2026-07-28, 32-core host, `make -j32`:** wall **7:04.88**, CPU 3953.5 s user + 287.5 s sys (998%), peak RSS **1.52 GiB** for the largest single process (`genautomata`) and **11.43 GiB** summed across the process tree at its peak (69 concurrent processes, 0.5 s sampling). Compile-command split: **492 TUs through mcc** (libiberty, the C half of libcpp, libdecnumber, libbacktrace, zlib, fixincludes, lto-plugin), 814 through `g++` (GCC proper is C++), 1421 through the freshly built `xgcc` (libgcc, libstdc++, the other target libraries). So mcc compiles ~18% of the compile commands — a gate should not be read as "mcc built GCC" without that qualifier. Note `/usr/bin/time -v` alone reports only the largest single child and understates a `-j32` build by ~7.5x — sample the tree if the number is meant to size a machine.

**What a gate would need to decide:** where the GCC tree comes from (it is ~1.5 GB of source and the build is another 1.7 GB, so not a checkout-per-CI-run); whether to gate on the full `c,c++` build or a cheaper subset like `all-gcc`; and whether the two workarounds above are permanent (they are both tracked as their own items — `-static-libgcc` acceptance and the libmccrt linkability question — so a green gate should eventually need NEITHER flag).

**Verification beyond exit status:** the resulting `xgcc`/`xg++` were run, not just built — a C varargs program (GP/SSE/overflow/large-struct) and a C++20 `<vector>`/`<algorithm>`/`<iostream>` program both compile and produce correct output. Any gate should keep that step; "the build finished" is much weaker than "the compiler it produced works".

## `__int128` PHASE 2 — replace the two-half helper calls with native emitters, TDD against gcc/clang (raised 2026-07-28)

Phase 1 (in progress) makes `__int128` real by representing it as two 64-bit halves and lowering every
operation to a libgcc-ABI runtime call in `runtime/lib/int128.c` — `__multi3`, `__udivti3`, `__ashlti3`,
`__clzti2` and friends, written as portable bitwise C over an upper/lower pair. That is the reference
semantics: obviously correct, linkable by any toolchain, and slow. **Phase 2 is to emit native x86_64
code for the operations where a call is absurd, one operation at a time, each one proven equivalent
before it is switched on.**

**Why a call is the right Phase-1 answer and still the wrong long-term one.** `a + b` on a 128-bit
value is `add`/`adc` — two instructions. Going through `__addti3`-style call overhead for that is
roughly two orders of magnitude off. Multiply is one `mulq` plus two `imulq` and an `add`. Shifts are
`shld`/`shrd` plus a branch on `count >= 64`. Only divide and modulo genuinely deserve to stay calls,
which is exactly what gcc does — gcc inlines everything else and calls `__udivti3`/`__divti3`.

**METHOD — stub first, prove, then switch. Do not write the emitter and then test it.**
For each operation:
1. Add a native emitter as a STUB that is compiled but not reachable, behind a per-operation env gate
   (`MCC_I128_NATIVE_ADD=1` and so on, defaulting OFF ⇒ byte-identical output). This is the same
   fallthrough-gate discipline the AST work uses, and it makes bisection trivial.
2. Write the differential test BEFORE the emitter body: a generator that emits C using the operation
   over a corpus of values, compiled three ways — mcc gate-off (helper call), mcc gate-on (native), and
   gcc plus clang as the oracle — with all four required to produce identical output. Values must
   include zero, ±1, `INT128_MIN`, both halves nonzero, high-half-only, and shift counts 0/1/63/64/65/127.
3. Only once the gate-on path matches on the full corpus does the gate flip default-ON, and the helper
   stays as the fallback for every other target.

**Ordered by payoff:**
- `add`/`sub`/`neg` — `add`+`adc`, `sub`+`sbb`. Two instructions, trivially provable, biggest ratio win.
- Comparisons — `cmp` high, branch, `cmp` low. Also feeds `switch` and conditionals.
- Bitwise `and`/`or`/`xor`/`not` — two independent 64-bit ops, no carry, the easiest of the lot.
- Shifts — `shld`/`shrd` plus the `count >= 64` case. The count-64 boundary is where implementations
  reliably go wrong (x86 shift counts are masked to 6 bits), so test 63/64/65 explicitly.
- `clz`/`ctz`/`popcount` — `bsr`/`bsf` on the appropriate half with a branch. Note these are ALSO in the
  wider "mcc's builtins are runtime calls" item elsewhere in this file; do them together.
- Multiply — `mulq` for the low 64×64→128 product, two `imulq` for the cross terms.
- Divide/modulo — LEAVE AS CALLS. gcc does. Not worth inline expansion.

**Prerequisite, and the reason this is Phase 2 rather than Phase 1:** these emitters need a 128-bit
value to live in a REGISTER PAIR, and mcc's register allocator is single-register-per-value. Phase 1
sidesteps this entirely by keeping the value in memory and passing it per the SysV ABI. Whether Phase 2
needs a real register-pair concept in `SValue`, or can get away with an
allocate-two-adjacent-registers convention, is the first thing to establish — and it is the item most
likely to change the plan, so establish it before writing any emitter.

**Acceptance for the whole phase:** every gate default-ON, byte-identical self-host fixpoint, ctest
green, and the differential corpus passing against BOTH gcc and clang. Any operation that cannot be
made to match stays a helper call — a slower correct answer beats a faster wrong one, and the Phase 1
implementation is always there as the fallback.

## Ungate campaign (flip every default-off feature on)
Endgame: once each gate's M8 soak is clean, flip it default-on and regenerate goldens. The proven optimizer passes are already flipped default-on at -O2 (`PROMOTE`/`COLOR`/`LICM_TEMP`/`IVSR`/`PRE`/`REASSOC`/`SETHI_LEAF`/`NARROW_ELIM`/`SPILL_SHARE`/`VLAT`/`DIVMAGIC` — **11 of these VERIFIED 2026-07-27 by `optfire/default-*`, which compiles with no env versus the gate forced to 0 and requires the objects to differ**). **`ARGFWD` was on that list and does NOT belong: its gate defaults on (`o4 || s1->optimize >= 2`) but the PASS is inert.** Toggling `MCC_AST_ARGFWD` at `-O2`/`-O3` changes nothing; it only becomes live with `MCC_AST_INLINE_PASS=0`, because argument forwarding lives in the replay-time graft path that `do_inline`'s `!ast_inline_pass_env` guard disables — and `INLINE_PASS` is default-on from `-O2`. **Second gate found in this class**, alongside `MCC_AST_PERFN_INPROC` (which is default-OFF, so it advertises nothing). NOT `MCC_AST_INLINE` — that one was retracted, see the `-march` section: it has a second consumer (`ast_reemit_retain`) and is load-bearing. The per-gate cost/benefit sweep in this section will have measured `ARGFWD` as worthless for the same reason its own caveat gives for `MATH_INLINE`/`ROUND_INLINE`/`COPYSIGN_INLINE`. `optfire/default-argfwd` asserts `off` as a TRIPWIRE: fix the graft-path guard and that cell fails, prompting the flip to `on`. **Swept for further instances 2026-07-27 — none found.** Method that works: a gate is in this class only if it has a case that PROVABLY exercises it (a passing `optfire` differ cell, i.e. forcing it changes the object) AND that same case shows no change when the gate is left at its default. Across all 51 differ cases only 4 flagged — `CHAINSTORE`, `OPASSIGN`, `PROMO_ARROW`, `PROMO_INCDEC` — and all 4 are false positives from testing at `-O2` when their declared defaults are `optimize >= 3` and `optimize_size`; each is confirmed default-ON at its real level (`-O3`, `-O3`, `-Os`, `-Os`). So `ARGFWD` is the only advertised-but-unreachable gate among those with exercising cases, alongside `MCC_AST_INLINE` and `PERFN_INPROC` which are known and share the same `!ast_inline_pass_env` guard. **A cruder sweep does NOT work and was discarded**: toggling each gate over `libmcc.c`+`mccgen.c` reported 28 gates as inert, but that conflates 'structurally unreachable' with 'no opportunity in those two files' — `TILE`, `FUSION`, `CSE_COMM` and others appear inert there while `optfire` proves they fire on suitable code. Object-diff sweeps need a case known to exercise the gate, or they measure the corpus rather than the compiler, and `MCC_CROSS_OPTIMIZER` defaults ON so cross triples carry the AST optimizer + embed-jit (the per-triple reemit/JIT parity axes are now real, not latent — watch cross-cell wall-clock, ~+46% per cross compiler). Remaining:
- The per-gate multi-arch golden-regen + tens-of-thousands-of-seeds differential soak on the x86_64/arm64-native CI cells, shared by every landed flip. **FIRST VALID GATE-SWEPT SOAK RAN 2026-07-27: 600 seeds, `596 agree, 0 miscompile, 4 dropped(UB), 0 mcc-buildfail` (seeds 100000-100599, x86_64, gcc+clang consensus).** With `--gates` each agreeing program is additionally rebuilt across the runner's 12 `MCC_AST_*` gates x 4 `-O` levels, so this is ~31k compile+run configurations, not 600. **Every earlier `--gates` run in this repo swept NOTHING** — `GATES[g].env` was passed into a parameter the reference-compiler branch of `build_run` never read (fixed 2026-07-27), so any prior claim of gate-sweep fuzz coverage is vacuous and this is the baseline to build on. Remaining for the flips: the same on arm64-native, and scaling the seed count up. Evidence so far: native x86_64 all-gates run 5501/5501 and arm64-macOS 5514/5514; a per-gate x86_64 mcc-vs-gcc differential soak found 0 miscompiles with all 12 gates individually confirmed firing; the 3-stage self-host byte-identical fixpoint (o1==o2==o3) holds with all 12 gates forced on AND per-gate individually (`selfhost-fixpoint-gates` ctest, native-ELF-gated). This is broad but NOT yet the per-gate seed soak the flips ultimately require.
- REASSOC follow-up: resolve pass-order confluence so the reassociated form is canonical (a determinism/optimality issue, not correctness — REASSOC holds the byte-identical self-host fixpoint).
- `MCC_AST_INTERCHANGE`/`_FUSION`/`_TILE` are **already default-on at `-O2`+** (`mccast.c` `ast_configure`); the residual is the soak evidence, not the flip.
- Wire `MCC_AST_COST` as the search/budget scorer, then flip it on (the search machinery it scores is itself still off).
- Flip the rest of the search family (`_EMITSIZE`/`_EMITISO`/`_INLINE`/`_THREADS`/`_ORDERED`/`_ORDER`) on (needs the emit-isolation prerequisite). `MCC_AST_SEARCH` itself is flipped default-on at -O≥4 ONLY (scoped to when the user asked for a search budget; at -O0..3 `optimize_search_seconds==0` ⇒ the search never runs, byte-identical). The per-function search is now resumable/continuable (CONTINUE from a budget-truncated memo hit skipping already-tried ordinals; a COMPLETE latch in the spare `order_n` word makes complete functions replay their winner, so repeated builds converge; candidate cap `AST_SEARCH_CAND_MAX=64` to match the 64-bit `tried`/`skip` bitmask). Observe with `MCC_AST_SEARCH_VERBOSE=1`. Remaining: the multi-arch golden-regen + seeds soak, a self-host byte-fixpoint under search-on across arches, confirm the emit-isolation prerequisite holds under that soak, then decide whether to flip the rest of the family and whether to lower the default from -O≥4 toward -O2.
- Flip `MCC_AST_PERFN_INPROC` on (depends on the same per-function search path).
- `MCC_AST_NO_CALLFUL` — leave OFF (a superopt search-axis constraint, not a straight win; only meaningful once the search family is on).
- **Flip evidence — 2026-07-26 per-gate cost/benefit sweep (x86_64).** Method: 60 gates measured two ways — isolated (floor + one gate) and leave-one-out (all-on minus one gate) — compile CPU as min-of-9 on the `src/mcc.c` self-host TU (noise floor ~±0.5%, 9-rep spread median 1.22%/p90 2.06%), runtime as min-of-3 over nbody/nsieve/mandelbrot/matmul/spectral with every output diffed against `gcc -O2`. 100 static + 95 runtime + 61 leave-one-out configs: **zero miscompiles and zero output mismatches** apart from the `INLINE_PASS` defect above. Load-bearing result: **isolated measurement badly understates value — no single gate beats −2% on nbody, yet all-on is −38%**, so the flip criteria should use leave-one-out marginals, not per-gate isolation. Largest marginals (runtime lost when the gate is removed from all-on): `PROMOTE` +21.2%, `OPASSIGN` +10.2%, `CHAINSTORE` +8.2%, `INLINE_PASS` +7.4%, `CSE_COMM` +4.9%, `NARROW_ELIM` +4.4%, `BFOLD_SQRT` +4.3%, `MATH_INLINE` +4.0%, `RANGE` +4.0%. This is direct support for the `MCC_AST_OPASSIGN`/`MCC_AST_CHAINSTORE` flips — **both landed default-on at `-O3`** and both are worth ~8–10% runtime, while costing 3.8%/1.8% compile time. Several high-value gates are compile-time NEGATIVE (they pay for themselves by shrinking the AST for later passes): `RANGE` −43.7ms, `SETHI_NARY` −39.3ms, `PRE` −32.2ms, `CSE_COMM` −31.5ms, `CALL_WINDOW` −25.3ms on a ~1022ms baseline. Worst ratio measured: `MCC_AST_CYCLE` at +89.1ms compile (8.7%) for +1.8% runtime. `MCC_MERGE_STRINGS` is the standout size lever: −12299 bytes `.text` for +45ms. Caveat: `MATH_INLINE`/`ROUND_INLINE`/`COPYSIGN_INLINE` only fire with `TEMPLATES` on (the rewrites live in `ast_bfold_run`), so any sweep that gates them independently measures them as inert.
- The runtime JIT is already default-on. `MCC_AST_JIT_SPLICE` stays OFF — it's a splice-validation primitive that REPLACES the optimizer emit path and DISABLES mode-6 KGC dispatch (`mccast.c` `!ast_jit_splice_env`); flipping it would remove the production JIT, not ungate it.

## Cross-arch parity (raise every arch to the x86_64 Tier-4 reference)
**MEASURED 2026-07-27, then CORRECTED — the real i386 gap is the NARROW family, not 17 gates.** Running the whole `optfire` differ suite against `cmake-cross/mcc-i386` (`OPTFIRE_NORUN=1 OPTFIRE_MCCFLAGS=-B$PWD/cmake-cross`) gives PASS 34 / FAIL 17 on OBJECT DIFFERENCE. But object-difference is the weaker signal, and checking the `--stats` counters splits those 17 into three very different groups:
- **reassociation (4) — NOT a gap. Retracted, and RESOLVED 2026-07-27 via `cdelta` mode (see the i386 triage above).** `reassoc` fires IDENTICALLY on both targets: `reassoc_assoc` 8/8, `reassoc_shlshr` 3/3, `reassoc_shrshl` 3/3, `reassoc_muldist` 4/4 (x86_64/i386). The transform happens; toggling the sub-knob just does not change the emitted i386 bytes. These cases need arch guards or reworking, and are NOT a parity defect. My first write-up of this section claimed they did not fire — that was read off the object diff alone, and a grep bug (matching `narrow 0` earlier in the panel than `reassoc`) appeared to confirm it.
- **narrow / value-range (6) — ROOT-CAUSED 2026-07-27, and it is NOT a narrow defect.** The real fault is in the **i386 AST recorder**: any function mixing 32-bit and 64-bit integers desyncs, so no AST pass can run in it at all. Isolated: on i386 `(long long)a * (long long)b`, `(int)(w + 3)`, the round trip of both, and `(long long)a + (long long)b` ALL report **`desync`**, while the same four are `faithful` on x86_64. Functions using `long long` UNIFORMLY (params and return all 64-bit — `add64`/`mul64`/`shift64`/`cmp64`) are faithful on i386, so it is the int<->long long CONVERSION that desyncs, not 64-bit arithmetic. One desync code (2169) for every shape suggests a single site. Consequence well beyond narrow: **every AST optimization is silently disabled for any i386 function that converts between int and long long.** That is why `narrow` reads 0 there — it never gets the chance to run. Note the earlier line in this section saying replay fidelity was ruled out: that check used the optfire narrow cases, which are written with `long`, and i386 `long` is 32 bits, so those functions never mix widths and are genuinely faithful. The ruling-out was true of those sources and false as a general claim. Reproduce: `MCC_AST_VERIFY=1 cmake-cross/mcc-i386 -B$PWD/cmake-cross -O2 -c <case>.c -o /dev/null`. **SITE LOCATED 2026-07-27.** `desync:<N>` is a LINE NUMBER (`AST_SET_DESYNC()` stores `__LINE__`), so it must be read against the source the compiler was BUILT from — `cmake-cross/mcc-i386` is stale and reports 2169, which lands in `ast_configure` and is meaningless; a compiler built from current source reports **2260**. That is the value-model guard `if ((ast_bad_type(tt) && !agg_lval) || (!is_const && !is_sym && !is_local && !(agg_lval && is_llocal_lval)))`. Instrumented to see which clause fires rather than inferring: **`badtype=0`, and `const/sym/local/llocal/agg` are ALL 0, with `r=0x00000000` and `btype` 3 (`VT_INT`) or 4 (`VT_LLONG`).** `r=0` is neither `VT_CONST` (0x30), `VT_LOCAL` (0x32) nor `VT_LLOCAL` (0x31) — it is register 0, i.e. the value is REGISTER-RESIDENT. So on i386 an int<->long long conversion materialises its result into a register, and the recorder's value model only covers const/sym/local operands, so it desyncs. Both directions trip it (btype 3 and 4 both appear). **Fix is substantial, not a patch:** it means extending the value model to represent register-resident (and on 32-bit, register-PAIR) values, which is why x86_64 is unaffected — a `long long` fits one register there and the conversion does not force this shape. Scope it before starting.
**BLAST RADIUS: it is a 32-BIT TARGET CLASS defect, not an i386 one.** Same 5-function probe across every cross compiler: `x86_64`, `arm64`, `riscv64` = **0/5 desync**; `i386` and `arm` = **5/5 desync**. Exactly the split the register-pair diagnosis predicts.
**IMPACT, MEASURED — and smaller than the synthetic probe implies, so do not oversell it.** Compiling mcc's own amalgamated TU at `-O2`: **x86_64 641 desync of 1735 functions (36%), i386 714 of 1708 (41%)**. So mixed-width conversions cost roughly **5 percentage points, about 73 functions**, on real code — total for each affected function, but not the wholesale loss the 5/5 synthetic result suggests. **Worth its own item: 48% of functions get NO AST optimization on x86_64, and the causes are now itemised.** Histogram over mcc's own amalgamated TU at `-O2` (1735 functions, `MCC_AST_VERIFY=1`): **833 non-faithful — 641 desync, 139 unfaithful, 50 bail, 3 empty.** The desyncs concentrate in four sites, so this is not a long tail:
| count | line | guard |
|---:|---:|---|
| 287 | 2969 | member access with `qual`/`bcheck`, or `ast_bad_type(mtype)` and not a bitfield |
| 213 | 2247 | vstack depth mismatch: `ast_vn != rel - 1 \|\| rel > AST_VS_MAX` |
| 83 | 2260 | the value-model guard — register-resident operand (the 32-bit int<->long long case above) |
| 32 | 2489 | `&&`/`\|\|` operand: non-const, or inside a call/op, or `ast_lor_top >= 16` |
So **the single biggest cause (287, 34% of desyncs) is member access**, and the second (213, 26%) is a vstack-depth invariant — together 60%, both far larger than the register-resident case (83, 13%) that the 32-bit item above is about. Any effort to raise optimization coverage should start at the member guard in `ast_hook_member_begin`, not at the width bug. (Sites in this file are named by HOOK rather than by line: `AST_SET_DESYNC()` records `__LINE__`, so any absolute number is only valid against the exact source the measuring compiler was built from, and every fix shifts them.)
**The member guard in `ast_hook_member_begin` broken down 2026-07-27 (instrumented, all 287 events accounted for):**
- **214 (75%) — the member's own type is `VT_STRUCT`**, i.e. a nested-struct member access; `ast_bad_type()` rejects `VT_STRUCT` outright. At 214 of 641 this is **33% of every desync in the TU — the single largest identified cause of lost AST optimization in the compiler.**
- **73 (25%) — `qual != 0`, always `0x100` = `VT_CONSTANT`**, i.e. the member (or the path to it) is `const`-qualified. Member base types here are ordinary (`VT_PTR` 35, `VT_INT` 24, `VT_LLONG` 8, `VT_BYTE` 4, `VT_SHORT` 2), so nothing about the value is unmodellable — it is the qualifier alone that rejects. `bcheck` never fires (0 events).
**The `const` group is now addressed: `MCC_AST_MEMBER_CONST`, default OFF (landed 2026-07-27).** Dropping a lone `VT_CONSTANT` qualifier recovers **51 of the 73**: desync 641 -> 590 over mcc's own TU. **It is NOT purely conservative, which is why it is opt-in** — the same run moves `unfaithful` 139 -> 143, i.e. 4 functions then replay to DIFFERENT bytes. That is a genuine model gap, not a safe relaxation, so the guard existed for a reason. It is still a net win because unfaithful functions are excluded from optimization anyway: +47 optimizable functions with no path to a miscompile. Validated: full suite 7214/7214 with the gate OFF (byte-identical) and, with it ON, the only failure is `ast-verify-ratchet` reporting **775 gaps vs 776 baseline — it fails because the gap set IMPROVED** and wants the baseline regenerated to bank the win. **The 4 regressors are identified 2026-07-27 and share ONE shape: member access through a POINTER-TO-CONST aggregate.** They are `ast_hash_of` and `ast_sid_node` (`const AstArena *a` -> `a->kind[n]` etc.), `set_flag` (`const FlagDef *flags` -> `p->name`, `p->flags`) and `so_ckpt_write` (`const SoCkpt *nw`); all four move `desync -> unfaithful`, none from any other verdict. Reproduce the list by diffing per-function verdicts between gate-off and gate-on runs of `MCC_AST_VERIFY=1` over `src/mcc.c`. So the residue is not random: dropping the qualifier lets the recorder MODEL a `const`-pointer member load whose replay then emits different bytes — the load itself is presumably codegen'd differently when the base is const-qualified. That is the thing to understand before the default can flip; **that hypothesis is now TESTED and FALSE.** Restricting the relaxation to non-arrow (`.`) access does avoid all 4 regressors — unfaithful stays at 139 — but recovers only **2** functions instead of 50 (faithful 1043 -> 1045 versus 1043 -> 1093 for the unrestricted gate). Nearly every const-member win comes from ARROW access, i.e. the same shape as the regressors, so there is no cheap subset to carve out and the variant was reverted rather than shipped for +2. The path forward is therefore to FIX the 4, not to avoid them: understand why a modelled `const`-pointer member load replays to different bytes. **Narrowed 2026-07-27, NOT yet closed.** Instrumented the site to print the enclosing function: all 73 `qual` events are `qual=0x100` (`VT_CONSTANT`) on **arrow** access, and the four regressors are `ast_hash_of` (member base `VT_PTR`), `ast_sid_node` (`VT_PTR`), `set_flag` (`VT_SHORT`) and `so_ckpt_write` (`VT_INT`). **Three minimal reproducers FAILED — do not retry these:** (1) `const struct R *p` with `int` members -> all faithful, no `qual`; (2) a `const int` FIELD inside the struct -> faithful; (3) `const struct Ar *a` with POINTER members (`a->kind[i]`), the closest match to `ast_hash_of` -> still faithful, no `qual`. A nested-aggregate member does desync but at the OTHER site (the struct-typed group), not this one. So the qualifier is not coming from 'pointer to const struct' in the obvious way, and the trigger is still unidentified. **Reproduction puzzle CLOSED 2026-07-27 — the qualifier was never the missing ingredient.** Named the members at the call site: the `ast_hash_of`/`ast_sid_node` events are exactly the `AstArena` pointer fields (`kind`, `op`, `ival`, `sym`, `first_child`, `next_sib`, `nchild`, `type_t`, `type_ref`, `fbits`), i.e. precisely the shape reproducer (3) modelled. The reason that reproducer looked like a miss: **it DID raise `qualifiers` (3 events) and still verified faithful.** `qual` alone does not desync — the site is guarded by `if (!ast_member_cap) return;` above it, so the access must ALSO have been captured (`ast_capture && !ast_desync && !ast_in_op && !ast_in_call && ast_vn >= 1 && ast_vn == rel`). All three attempts raised the qualifier in non-capturable contexts. Note the scale gap this explains: 458 `qualifiers` events across the TU, only 73 of which reach the desync. A minimal reproducer therefore needs the const member access in a CAPTURABLE position — not inside a call argument or a nested operation. **ANSWERED 2026-07-27 — the replay is SEMANTICALLY WRONG, not merely reordered, so the guard is justified.** Use the existing facility rather than adding one: `MCC_AST_VERIFY_DIFF=<substr>` dumps a byte diff for any non-faithful function (`MCC_AST_MEMBER_CONST=1 MCC_AST_VERIFY=1 MCC_AST_VERIFY_DIFF=set_flag mcc ... -c src/libmcc.c`). For `set_flag`: baseline and replay are both 426 B, first diff at +291. Two differences:
1. *Operand evaluation order swaps* — baseline `mov rax,[rbp-8]; mov rcx,[rbp-0x30]; movzx ecx,[rcx]; add rax,rcx`, replay `mov rax,[rbp-0x30]; movzx eax,[rax]; mov rcx,[rbp-8]; add rcx,rax`. Equivalent, just a different schedule.
2. **An INVERTED CONDITION** — after identical `and edx,2` / `cmp edx,0`, baseline emits `0f 95 c2` (`SETNE`) and the replay emits `0f 94 c2` (`SETE`). That is not a scheduling artifact; the modelled form computes the opposite boolean for `if (0 == (p->flags & WD_ALL))`.
So relaxing the qualifier makes the recorder model these accesses INCORRECTLY. No miscompile can escape today — `unfaithful` functions are excluded from optimization, which is exactly the backstop working — but this reframes the gate: the +47 recovered functions are genuinely faithful and fine, while the 4 are a real modelling bug that must be fixed, not merely tolerated. **Do not flip the default and do not regenerate the ratchet baseline until the SETE/SETNE inversion is understood** — banking the ratchet would hide it.
**Investigation state 2026-07-27, two dead ends recorded so they are not retried:**
- *Synthesising the shape does not work.* A FOURTH minimal reproducer failed: `const struct F *f` with the member access as the left operand of `&&` in a capturable position (`if ((f->flags & 2) && other(k))`), which is the exact construct `set_flag` inverts. Faithful under both gate settings. Four attempts have now missed; stop building candidate structs.
- *`MCC_AST_REPLAY_DUMP` cannot see it.* The dump is emitted only for functions that end FAITHFUL, so the recorded AST for `set_flag` (unfaithful under the gate) never prints — 22758 dump lines, zero mentions of it. Inspecting the modelled AST needs the dump moved before the faithfulness check, or a separate always-dump env.
**MINIMAL REPRODUCER FOUND 2026-07-27 by reducing the real function — synthesising never would have.** ~30 lines, and it tracks the real build exactly: `MCC_AST_MEMBER_CONST=0` -> `desync`, `=1` -> `unfaithful`. Ingredients: the REAL `FlagDef` layout `{uint16_t offset; uint16_t flags; const char *name;}` — note `uint16_t`, which is why every hand-written `int`-member attempt missed (the probe had already said `mbtype=2`/`VT_SHORT` and I did not act on it) — a `const FlagDef *p` walked in a `for` loop, `if (0 == (p->flags & WD_ALL)) continue;`, and the `*f = (*f & mask) | (value ^ !!(p->flags & FD_INVERT));` store. Stub `strstart` returning 0 is enough. **ISOLATED 2026-07-27 to a FOUR-LINE function: the bug is `!!` (double negation) on a captured const-qualified member load.**
```c
typedef struct FlagDef { uint16_t offset; uint16_t flags; const char *name; } FlagDef;
static int dneg (const FlagDef *p){ return !!(p->flags & 2); }   /* MCC_AST_MEMBER_CONST=1 -> unfaithful */
static int sneg (const FlagDef *p){ return  !(p->flags & 2); }   /* faithful */
static int plain(const FlagDef *p){ return  (p->flags & 2) != 0; } /* faithful */
```
Single `!` and the explicit `!= 0` both model correctly; only `!!` breaks. The 30-line `set_flag` reduction reproduces the real build's diff BYTE-FOR-BYTE (426 B, first diff @ +291), and the inverted region decodes to exactly this expression: `mov rdx,[rbp-0x30]; add rdx,2; movzx edx,word[rdx]; and edx,2; cmp edx,0;` then baseline `0f 95 c2` (`SETNE`, i.e. `x != 0`) versus replay `0f 94 c2` (`SETE`, i.e. `!x`). **The model emits ONE negation where the source has two.**
Runtime behaviour is unaffected in both gate settings (`1 0 1`), because unfaithful functions are excluded from optimization — the backstop working as designed, which is why this was never a live miscompile.
**ROOT CAUSE FOUND 2026-07-27, and a fix ATTEMPTED AND WITHDRAWN — read before retrying.**
*Cause:* `gen_test_zero(TOK_EQ)` has two paths. When `vtop->r != VT_CMP` it does `vpushi(0); gen_op(op)`, which the recorder sees. When `vtop->r == VT_CMP` it inverts in place — swap `jtrue`/`jfalse`, `vtop->cmp_op ^= 1` — emitting no code and calling NO `gen_op`, so `ast_hook_genop` never fires. For `!!x` the first `!` takes the emitting path and IS modelled; the second takes the in-place path and is NOT. Hence one negation in the model where the source has two.
*Fix:* **LANDED 2026-07-27 as `MCC_AST_CMP_INVERT`, default OFF.** `ast_hook_cmp_invert()` is called from that branch and flips the modelled node's op with `^1` (comparison tokens pair correctly: `ULT`/`UGE` 0x92/0x93, `EQ`/`NE` 0x94/0x95, `ULE`/`UGT` 0x96/0x97, `LT`/`GE` 0x9c/0x9d, `LE`/`GT` 0x9e/0x9f), desyncing when the top node is not a Binary comparison. Isolated case fixed: `dneg` goes unfaithful -> faithful, and in the 30-line `set_flag` reduction the SETE/SETNE inversion disappears (both sides emit `0f 95 c2`), leaving only benign operand reordering. **Measured over mcc's own TU: unfaithful 139 -> 122, desync 642 -> 650, faithful 1043 -> 1035.** Full suite 7214/7214 with the gate OFF *and* ON.
*Correction to the previous entry here:* it recorded three failures (`regression/o4-aot-jit`, `selfhost-jit`, `cross-factory`) that 'persisted with the hook gated OFF' and called them unexplained. They are explained and were my own bug: the call site lacked `#if MCC_CONFIG_OPTIMIZER`, and `cross-factory` builds a compiler WITHOUT the optimizer, so it failed with `implicit declaration of function 'ast_hook_cmp_invert'` — a compile error, which is precisely why gating could not neutralise it. Every other hook call in `mccgen.c` carries that guard. With it added, all three pass both ways.
*Why the net faithful count DROPS and that is the right trade:* the 8 functions that lose `faithful` had a model with an INVERTED CONDITION and were being optimized on it. They replayed byte-identically so nothing escaped, but an optimizer acting on that AST is a latent miscompile path — the gate closes it and they now desync honestly. So this is a correctness fix that costs a little coverage, not a coverage regression. **Second target CONFIRMED 2026-07-27 (i386):** same shape as x86_64 — unfaithful **131 -> 115**, desync **715 -> 722**, faithful **941 -> 934** over mcc's own TU, compile rc=0. So the fix behaves identically where it can be measured, and the desync fallback's blast radius is proportionate on a second target rather than x86_64-specific.
**arm64 and riscv64 remain UNMEASURED, and the numbers a naive run prints are junk** — a host-tool compiler for those targets cannot compile `src/mcc.c` on an x86_64 box: it dies at `mcchost.c: error: field not found: pc` (signal-context fields are host-header shaped), rc=1, after verifying only ~80 functions. An earlier run of this comparison printed plausible-looking deltas for both; they were from failed compiles. Measuring them needs a real cross sysroot or a native run. **arm64 MEASURED 2026-07-27 — the fix does NOT work there, so the default must stay OFF.** Measuring it needed a different route than `src/mcc.c` (which cannot be compiled for arm64 on an x86_64 host): rebuild `cmake-cross` with the change and run the `!!` reproducer through `cmake-cross/mcc-arm64`. Result on `dneg`:
| target | gate off | gate on |
|---|---|---|
| x86_64 | unfaithful | **faithful** |
| i386 | unfaithful | **faithful** |
| riscv64 | unfaithful | **faithful** |
| **arm64** | unfaithful | **unfaithful** |
The arm64 diff is a different shape from the x86 one: baseline **32 B vs replay 28 B** — a LENGTH difference, not a condition swap — first diff at +25, with `1a9f17e0` vs `1a9f07e0` (`CSINC`, differing condition field) and an extra baseline instruction the replay omits. arm64 materialises `!!x` through conditional-select rather than a compare-and-setcc pair, so flipping the modelled comparison op does not reproduce it. The `^1` token flip is the right model for the x86-shaped path only. **arm64 now HANDLED 2026-07-27 by the desync route** (the second of the two options). Under `#ifdef MCC_TARGET_ARM64` the hook sets desync instead of flipping the op, because a token flip models arm64's CSINC form WRONGLY and a wrong model that happens to replay identically is exactly the latent miscompile path this hook exists to close. Verified on the `!!` reproducer with the gate ON: x86_64 `faithful`, i386 `faithful`, riscv64 `faithful`, arm64 `desync` — honest rather than unfaithful-by-luck. arm64 loses nothing it had, since `!!` was already mismodelled there before this hook existed.
**Remaining before the default can flip:** model the arm64 CSINC form so that target gains the fix too (optional — the gate is sound without it), and regenerate the ratchet baseline.
**The 4 `MCC_AST_MEMBER_CONST` regressors are NOT fixed by `MCC_AST_CMP_INVERT`** — with both gates on, the 30-line `set_flag` reduction is still `unfaithful`. The inversion is gone (that was the correctness half); what remains is a pure OPERAND-ORDER divergence, same length 426 B, first diff @ +291. Source `f = (unsigned char *)s + p->offset;`: baseline loads `s` then `p->offset` (`mov rax,[rbp-8]; mov rcx,[rbp-0x30]; movzx ecx,word[rcx]; add rax,rcx`), replay loads `p->offset` then `s` (`mov rax,[rbp-0x30]; movzx eax,word[rax]; mov rcx,[rbp-8]; add rcx,rax`). So the recorded `Binary '+'` has its operands swapped relative to the order codegen evaluated them.
**Ruled out: Sethi-Ullman.** The reorder persists with `MCC_AST_SETHI=0 MCC_AST_SETHI_LEAF=0 MCC_AST_SETHI_NARY=0` all set, individually and together — so it is not a reordering pass, it is how the member-capture path records the enclosing binary's children (`ast_hook_member_end` replaces the top `ast_vs` entry; something there leaves the operands transposed).
Note this residue is BENIGN, unlike the SETE/SETNE one: both operands are pure loads and `+` is commutative, so the replay computes the same value. Fixing it buys coverage, not correctness — which is a different priority from the inversion work above.
**riscv64 has ~30x the unfaithful rate of the other targets — CHARACTERISED 2026-07-27; the code is IDENTICAL, the RELOCATIONS are not.** Over the 72-file freestanding `optfire` corpus all four targets compile cleanly with desync 3-6 and faithful ~164, but unfaithful is **3 (x86_64), 3 (i386), 3 (arm64), 92 (riscv64)**. Unrelated to `MCC_AST_CMP_INVERT` (identical with it off and on).
What the divergence actually is: `MCC_AST_VERIFY_DIFF` reports **`code identical — relocation/length divergence`** for **27 of 30 sampled**, e.g. `main` in `abs.c` is 84 B on both sides with matching bytes. So `ast_replay_body` reproduces riscv64 CODE exactly and the faithfulness check fails on its relocation clause (`new_rel - ast_reloc0_sv == rel_len && ast_reloc_range_equiv(...)`), not on `memcmp`.
The distribution fits: **72 of the 92 are `main`**, and every corpus `main` calls `printf` — i.e. it is functions with an EXTERNAL CALL. riscv64 lowers a call as an `AUIPC`+`JALR` pair carrying `R_RISCV_CALL` (usually plus `R_RISCV_RELAX`), so the natural hypothesis is that replay emits a different relocation COUNT for the pair. **Unverified** — confirming it means dumping the reloc records on both sides, which nothing currently does.
Consequence if true: every riscv64 function containing an external call is excluded from AST optimization, which would be a far larger coverage loss on that target than any gate discussed above. Per the standing instruction, riscv64 work is reserved for the arm64 machine — this is recorded as an observation, not started.
The struct group (214, 75% of member desyncs) remains the bigger prize and needs the value model to represent aggregate members.
- **promotion / register (6)** — `promote`, `promo_arrow`, `promo_incdec`, `spill_share`, `color`, `opassign`. No `--stats` counter exists for these, so only the object diff is available. `opt_promote` is set only inside `#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)`, and forcing `MCC_AST_PROMOTE=1` (which overrides the default) still changes nothing, so the backend support looks genuinely absent rather than defaulted off — probably by design, but unconfirmed.
Control: 34 gates including `divmagic`, `select`, `tile`, `fusion` DO fire on i386, so nothing here is a blanket 'cross has no optimizer' effect (an earlier reading to that effect was a failed-compile artifact, retracted).
**METHOD NOTE, twice-learned: prefer the `--stats` counter over an object diff when asking whether a pass fired.** An object diff conflates 'did not fire' with 'fired but emitted the same bytes', and it silently reports 'differ' when both compiles failed. Check the compile's exit status, and anchor counter greps with `\b<name> +[0-9]+` — the panel prints many counters and an unanchored pattern matches the wrong one.
Next step: trace why `narrow` declines on i386.

Each arch should match x86_64 for self-host, promotion, cmov/csel, div-magic, JIT stub tail, ASan native-shadow, stack-protector, UBSan trap, TLS GD/LDM, Tier-4 replay-inline, over-align. Per-arch remaining:
- **riscv64** — raise to Tier-4: self-host, replay-inline ungate, JIT stub tail, ASan native-shadow READ/WRITE access-type + region-relative locator, Sv39/bottom-up-mmap shadow-layout robustness, UBSan/ASan `ebreak` SIGTRAP coexistence. (Promotion, stack-protector canary, and codegen-time PIC/PIE TLS local-exec are DONE; **`-run` + JIT `MCC_JIT=0`≡`=1` parity now VERIFIED under qemu-riscv64 on an arm64 host, 2026-07-27, see P0 step 2** — was previously build-only; callee-saved float pool deferred as arm64.)
- **i386** — JIT stub tail; runtime-test the implemented PIC/PIE ASan stack path.
- **arm (armv7)** — **`-run` + JIT `MCC_JIT=0`≡`=1` parity VERIFIED, static AND dynamic host (qemu-arm on an arm64 host, 2026-07-27, see P0 step 2)** — `hi`/`hot` correct + byte-identical. The DYNAMIC-host far-external-call gap (`R_ARM_CALL` reloc 28 >±32MB) is FIXED via `arm_veneer_memory_calls` (arm peer of the arm64 `-run` veneer); regression-locked by `run-parity-arm`. AOT-to-file link is separately rough (mcc's arm `.o` stamps EABI-attr v0 → GNU ld rejects; static link hits GOT reloc 107 + `__ctzdi2`; no arm32 sysroot vendored). Then: self-host, JIT stub tail. Full `-pie` (ET_DYN) ASan is blocked by the shared mccasan PIE-startup bug.
- **arm64** — mode-6 object-output dispatch, in-place patch row. (`opt_promote` default-on at -O2 DONE 2026-07-26: parity with x86_64, `ast_configure` now enables it for `MCC_TARGET_ARM64`; validated 604/604 native-aarch64 differential vs gcc at -O0/-O1/-O2/-O4, byte-deterministic on the self-compiled 2.37 MB amalgamation, host arm64-macOS ctest exec/behavior/ast/selfhost/diff 363/363. Residual: full CI seed soak; a true 3-stage self-host binary fixpoint is blocked by the pre-existing gate-independent arm64 TLS-LE reloc-overflow in mcc's own linker, not by this flip.)
- **PE targets** — full UBSan handler ABI (minimal-runtime only today; needs `.rodata` descriptor tables); the i386/arm64-PE embed-jit per-arch baked blob (toolchain-gated); the arm64-PE JIT native-fault class (HW-gated).
- **True `-shared`/`.so` dynamic-TLS on arm/arm64/riscv64** — errors on all three. riscv64's `riscv64-link.c` GD→LE relaxation is a deliberately-incomplete stub (rewrites auipc→lui but never neutralizes `call __tls_get_addr`); global-dynamic dynamic-TLS output for shared libs stays open. (Local-exec / non-shared PIC/PIE TLS is DONE on every arch.)
- **Weak-memory-model validator for aarch64/armv7** (qemu is x86-TSO). The native-arm64 differential-fuzz band is DONE and live (portable gcc+clang majority-vote oracle on `ubuntu-24.04-arm` per-push). Remaining: re-land the nightly `campaign` soak by editing `matrix.yml` directly (it's now the hand-edited source of truth — the `ci emit` generator was removed; `fuzz-nightly.yml` already runs the campaign via `ci fuzz`); an armv7 cell; a dedicated concurrency/litmus validator.

**Hard ceiling — cannot open without arm64-Windows silicon** (qemu/wine mask via x86-TSO): the arm64-Windows MSVC JIT-exec miscompile; the arm64-PE JIT native-fault subset (SEH/icache/frameless-leaf, codegen already wine-validated); the aarch64/armv7 weak-memory validator. Keep these explicitly `experimental`/skip-marked so they never false-green.

**Cross-cutting enabler:** `.rodata` data-emission unblocks the full `__ubsan_handle_*` ABI (Sanitizers §) + value-table dispatch (Const-data §) at once.

## Const-data
- Size-changing datacomp: `.init_array` decompress ctor (all 5 arches) + `__mcc_decompress` runtime.
- TLS `tdata`→`tbss` + asan/bcheck zero-init `.bss` cases.
- Value-table `switch` dispatch (needs `.rodata` data-emission).
- **Dense-switch jump table — DONE 2026-07-26** (x86_64, non-PIC, gated `MCC_SWITCH_JUMPTABLE` default OFF below `-O4` ⇒ byte-identical). `switch_jt_dense`/`gcase_jumptable` (mccgen.c) replace `gcase`'s O(log n) compare tree with an O(1) indexed jump for dense integer case sets (`n>=4`, `span=hi-lo+1<=4096`, `>=50%` density, not `long long`, not PIC): emits `idx=val-lo; if ((unsigned)idx > span-1) goto default; jmp *tab[idx]` (`ff 24 c5 <disp32>` + `R_X86_64_32S` to a `(span)`-entry `.rodata` pointer table; gaps → a default trampoline). **AST-replay interaction** solved via `ast_hook_bail` (mccast.c) rather than the specced tab_sym-on-node reuse: the baseline codegen emits the table during recording and bails the recorder for that function, so the byte-faithful baseline is kept without a faithfulness/reloc mismatch — this covers the -O2 hot path too (the function just isn't further AST-optimized). A subtlety fixed: the dispatch is emitted after the body's terminating jump so `nocode_wanted` is set, which makes `put_extern_sym` a no-op for the per-case position symbols the table relocs need — cleared around symbol creation. Validated: 40 randomized non-reducible dense switches + hand patterns (gaps/ranges/negative-lo/wide) bit-match gcc at O0/O2/O4, byte-identical gate-off vs HEAD baseline (mcc.c self-host TU + corpus), PIC bails to `gcase`, all 5 arches build (x86_64-only `#if`), asttool 20/0. **PIC support DONE 2026-07-26** — under `-fPIC`/`-fPIE`, `gcase_jumptable` emits a table of 32-bit SELF-RELATIVE offsets (`case - table_base`) instead of absolute 8-byte pointers, dispatched via `mov idx32,idx32; lea tab(%rip),base; movslq 0(base,idx,4),tmp; add base,tmp; jmp *tmp`. The offsets are position-independent so the table stays truly read-only (the case-vs-table difference is a link-time constant within the image — no dynamic relocation); each entry is an `R_X86_64_PC32` to the case symbol with addend = the entry's own offset within the table (`S + A - P = case + i*4 - (tab_base + i*4) = case - table_base`). `switch_jt_dense` no longer bails on `mcc_state->pic`. Validated (docker native x86_64): PIC differential (40 randomized dense switches, gaps/ranges/negative-lo/out-of-range, linked `gcc -pie`) bit-matches gcc-pie at O0/O2/O4 (589 PC32 table relocs fire); non-PIC path byte-identical to HEAD (pure-additive refactor) + still matches gcc `-no-pie`; default gate-off byte-identical; all 5 arches build; asttool 20/0. **arm64 port DONE 2026-07-26** — `switch_jt_env`/`switch_jt_dense`/`gcase_jumptable` are now `#if X86_64||ARM64`; arm64 always uses a 32-bit self-relative offset table (adrp/add is PC-relative, so no separate PIC path). Dispatch: `mov Widx,Widx` (zero-extend); `adrp base,tab` (`R_AARCH64_ADR_PREL_PG_HI21`) + `add base,base,:lo12:tab` (`R_AARCH64_ADD_ABS_LO12_NC`); `ldrsw idx,[base,idx,lsl 2]`; `add base,base,idx`; `br base` (encodings verified with aarch64-as); the shared range-check + `ast_hook_bail` + table-reloc loop are reused, with entries as `R_AARCH64_PREL32` (case − table_base, addend = entry offset). `intr()` forward-declared for mccgen.c (arm64-gen.c is included later in the amalgamation). Validated (docker + qemu-aarch64): a 40-switch randomized differential (gaps/ranges/negative-lo/out-of-range) + hand patterns bit-match aarch64-gcc at O0/O2/O4 (548 PREL32 table relocs fire); arm64 default gate-off byte-identical; x86_64 non-PIC + PIC gate-on byte-identical to HEAD (pure-additive restructure, 6/6); all 5 arches build; asttool 20/0. **riscv64 port DONE 2026-07-26** — now `#if X86_64||ARM64||RISCV64`; riscv64 also uses a 32-bit self-relative offset table (auipc/addi is PC-relative). Dispatch: `slli/srli idx,idx,32` (zext.w); `auipc base,%pcrel_hi(tab)` (`R_RISCV_PCREL_HI20`) + `addi base,base,%pcrel_lo(label)` (`R_RISCV_PCREL_LO12_I` to a `put_extern_sym` label at the auipc site, mirroring `load_symofs`); `slli idx,idx,2`; `add idx,base,idx`; `lw idx,0(idx)` (sign-extends); `add base,base,idx`; `jr base` (encodings verified with riscv64-as). Table entries express the cross-section difference `case − table_base` as a `R_RISCV_ADD32(case)` + `R_RISCV_SUB32(tab_sym)` pair at the same offset (RISC-V's convention for a data symbol difference — verified with riscv64-as). `ireg()` forward-declared for mccgen.c (riscv64-gen.c is later in the amalgamation). Because the jump table bails the AST recorder, riscv64's replay-fidelity gaps (the PCREL label-index divergence) are irrelevant here — no replay happens. Validated (docker + qemu-riscv64): a 40-switch randomized differential + hand patterns bit-match riscv64-gcc at O0/O2/O4 (507 ADD32 table relocs fire); riscv64 default gate-off byte-identical; x86_64 non-PIC + PIC byte-identical to HEAD (6/6, pure-additive `#elif`); all 5 arches build; asttool 20/0. So the jump table now covers 3 of 5 arches (x86_64/arm64/riscv64). **i386 port DONE 2026-07-26** — now `#if X86_64||ARM64||RISCV64||I386`; i386 uses an absolute 4-byte pointer table (`elt=MCC_PTR_SIZE`), non-PIC only (`switch_jt_dense` bails to `gcase` when `mcc_state->pic`, since i386 has no PC-relative addressing for a self-relative table). Dispatch: `jmp *tab(,idx,4)` (`ff 24 <SIB>`, ModRM 0x24 /4+SIB, SIB scale4/index=idx/no-base → `disp32` = `R_386_32` REL to the table symbol); the idx reg is already a full 32-bit register constrained to [0,span-1] by the range check, so no zero-extension is needed (unlike x86_64/arm64/riscv64). Table entries are absolute 4-byte pointers (`R_386_32`, REL with implicit-0 addend from the zero-filled rodata — `section_realloc` memsets new bytes). `greloc` (i386 REL, no addend arg) is used instead of `greloca`. Validated (docker mccx64, `-m32`): a 100-seed randomized dense-switch differential (gaps/ranges/negative-lo/out-of-range, 40% range-cases) bit-matches gcc `-m32` at O0/O2/O4 (fired on 90/100; the other 10 bailed on the <4-case / <50%-density thresholds, still correct); dispatch confirmed `ff 24 85 <disp32>` = `jmp *tab(,%eax,4)`; default gate-off byte-identical (md5); PIC bails to `gcase` (0 jump tables, runtime MATCH under `-fPIC`); `long long` + sparse switches bail; x86_64 gate-on byte-identical to HEAD + still matches gcc `-no-pie`; all 5 arches build; asttool 747/0. So the jump table now covers 4 of 5 arches (x86_64/arm64/riscv64/i386). Follow-ups (deferred): arm (armv7) port; i386 PIC (needs the `call/pop`-thunk PC base for a self-relative table); store tab_sym on the AST switch node so replay REUSES it and the jump-table function stays AST-optimizable instead of bailing; also unblocks `__ubsan_handle_*` descriptor tables.

## CST
- slice-G multi-file `#include` stitching.
- `-g` from CST provenance (stands up the debugger + gdb suite); stop discarding the arena on the `--lsp`+`-g` path.
- Design `--hotreload` from reconciled CST snapshots.
- Revisit the Bind-marker (does CST supersede it?).
- Emit `CST_Error`/`CST_Missing` (error-recovery CST).

## Sanitizers
- Honor auto over-alignment under `-fsanitize=address` / `-b`.
- **Full `__ubsan_handle_*` ABI.** Minimal-runtime trap + `-recover` is landed on all 5 ELF arches (gated OFF) + PE (i386/arm): per-arch emitters swap the trap for a `call`/`bl` to `__ubsan_handle_<kind>_minimal` (add/sub/mul/divrem_overflow, shift_out_of_bounds, type_mismatch_v1); the handlers (runtime/lib/mccubsan.c, auto-linked) log to stderr, return, and print an atexit summary (`MCC_UBSAN_EXITCODE=N`/`UBSAN_OPTIONS=exitcode=N` forces exit status on recovered UB). Per-check recover sets parse a comma list into `MCC_SANR_*`. Remaining: upgrade minimal→full ABI — emit `struct {SourceLocation, TypeDescriptor}` tables + operand values so diagnostics name file:line and values (large, needs `.rodata` descriptor emission).
- **Native-shadow ASan report enhancements.** riscv64 heap+stack+global redzones are DONE (qemu-riscv64 proven; Sv48-class shadow reserving the x86_64 `[OFF,2^44+OFF)` span). Remaining: access-type READ/WRITE + region-relative locator in reports; Sv39/39-bit-VA/bottom-up-mmap shadow-layout robustness. Port these enhancements to x86_64 (3rd saved reg, qemu-amd64 validation) then propagate to riscv64.
- Fix the same-granule "…to the left" locator underflow in the shared `asan_locate` (affects all PE arches + ELF); msvcrt-internal CRT allocs aren't redzoned.
- Confirm the ShadowGap contiguous-mmap fix on real x86_64 CI — not locally reproducible (qemu can't run the `-fasan-shadow` runtime).
- Decide compiler-rt-interop vs `libmccsan`.
- Explore `-fsanitize-coverage`, `-fsanitize=cfi`, `_FORTIFY_SOURCE`, freestanding/KASAN-style runtime sanitizer.

## Tests / infra
- **Coverage holes in the -O sweep** (open): `exec-gatesoff/*` runs the corpus at `-O3` with `OPASSIGN`/`CHAINSTORE` forced **OFF**, and `exec-O3/*` now runs it at `-O3` with **defaults** (added 2026-07-27, 299/299). Both directions are covered. (An earlier note here claimed 4 pre-existing `-O3` failures — `atomic_misc`, `run_atexit`, `errors_and_warnings`, `nodata_wanted`. That was **wrong**: it came from invoking `exec_runner` by hand without the `_exec_env` the ctest cells supply. All four pass in the real cells.) ~~Separately, `tools/selfhost-fixpoint.py` hardcodes `-O2`, so the fixpoint gate never exercises a `-O3` default either.~~ **CLOSED 2026-07-27** — the script takes `--opt=<level>` (default `-O2`, so nothing regresses) and two new cells `selfhost-fixpoint-O3` and `selfhost-fixpoint-Os` cover the levels that were blind: `-O3` turns on `CYCLE`, `OPASSIGN`, `CHAINSTORE` and `INLINE`, and `-Os` turns on `PROMO_ARROW`, `PROMO_INCDEC` and `REGDISP`, none of which the `-O2` gate ever saw. **Both hold byte-identically**: `-O3` o1==o2==o3 at 5512623 B, `-Os` at 5477327 B. Each cell adds ~11 s.
- **Runtime (generated-code speed) benchmark harness — LANDED 2026-07-26.** `tools/runtime-bench.py` + ctest `runtime-bench-check`. It builds each kernel with mcc and with a reference compiler, runs min-of-N, and **verifies mcc's output against the reference on every run** — a fast result that miscompiles fails instead of scoring well. CI runs `--check-only` (one run, correctness only): wall-clock on a shared runner is noise, so timing is advisory and never a pass/fail condition. `--gates "K=V …"` is repeatable, so a flip decision is one command — the first config is the baseline and later ones print as a delta, with numbered columns and a legend (gate strings are far too long to be headers). Skips 77 without a reference compiler or kernels.
  **Superseded by the instruction-ratio baseline below — wall-clock ratios conflate real work with layout and cache effects, so prioritise on `insn/ref`.**
  **Cross-arch validation of the day's gate set on the REAL kernels, 2026-07-26** (previously only the randomized fuzz corpus was cross-checked): compiled with `CHAINSTORE`+`OPASSIGN`+`PROMO_INCDEC`+`IVSR_PTR` for riscv64 and arm64 via the vendored stage3 sysroots and run under qemu, **nbody, nsieve, mandelbrot and matmul all match the gcc reference on BOTH arches**, and spectral matches on both with `-fc99-inline-body` — with `A-calls = 0`, i.e. **the plain-C99-inline inlining fix works on riscv64 and arm64 too, not just x86_64**. Combined with the byte-identical 3-stage self-host fixpoints on all three arches, the gate set is now validated on real programs cross-arch rather than only on generated ones.
  **CONSOLIDATED 2026-07-26 — effect of the whole day's gate set vs stock defaults** (`MCC_AST_CHAINSTORE=1 MCC_AST_OPASSIGN=1 MCC_AST_PROMO_INCDEC=1 MCC_AST_IVSR_PTR=1`, `--runs 5`, spectral also gets `-fc99-inline-body`):

  | kernel | insns vs defaults | time vs defaults | resulting insn/ref |
  |---|---|---|---|
  | matmul | **−24.9%** | −20.9% | 10.22x → ~7.7x |
  | spectral | **−23.8%** | −32.1% | 7.41x → ~5.6x |
  | nbody | −6.9% | −19.0% | 2.46x → ~2.29x |
  | mandelbrot | −0.1% | −0.7% | 2.05x |
  | nsieve | **+1.5%** | −9.1% | 3.04x → ~3.09x |

  Two honest readings: **nsieve is a small real instruction REGRESSION** (+1.5%) that the wall-clock column hides behind a −9.1% that is layout noise — it is memory-bound (3.04x instructions but only 1.6x time), so it should not drive decisions either way; and mandelbrot is untouched, consistent with its residue being cache/layout rather than emitted work. None of these gates is flipped — they remain default-OFF pending the shared soak and the layout-sensitivity call.
  **x86_64 INSTRUCTION baseline captured 2026-07-26** (`--gates "MCC_AST_OPASSIGN=1 MCC_AST_PROMO_INCDEC=1 MCC_AST_IVSR_PTR=1"`, reference gcc -O2 `-ffp-contract=off`, `insn/ref` = mcc instructions retired / gcc's):

  | kernel | time vs ref | **insn/ref** | mcc insns |
  |---|---|---|---|
  | nbody | 1.96x | **2.29x** | 14.61G |
  | nsieve | 1.44x | **3.09x** | 4.72G |
  | mandelbrot | 2.84x | **2.05x** | 7.27G |
  | matmul | 8.17x | **7.67x** | 46.78G |
  | spectral | 4.34x | **7.41x** | 22.68G |

  Readings that change priorities: (a) **matmul and spectral genuinely execute ~7.5x the instructions gcc does** — that is a real work gap, not layout, and they are where codegen effort pays; (b) **nsieve executes 3.09x the instructions but runs only 1.44x slower**, i.e. it is memory-bound and extra instructions hide in stalls, so optimising it is worth less than the time ratio suggests; (c) mandelbrot is the opposite (2.05x insns but 2.84x time), so its remaining gap is cache/layout rather than emitted work. Note the reference is gcc WITH vectorization, and each packed op does 2 doubles, so of matmul's 7.67x roughly 1.9x is SIMD and the rest (~4x) is scalar inefficiency — vectorization alone would not close it.
  Older wall-clock-only baseline, kept for the timing figures: (this host, gcc -O2 -ffp-contract=off reference, min-of-3):

  | kernel | ref ms | mcc ms | ratio |
  |---|---|---|---|
  | nbody (plb 2.c, 10M) | 500 | 1252 | 2.50x |
  | nsieve (plb 1.c, 12) | 351 | 526 | 1.50x |
  | mandelbrot (3000) | 467 | 1319 | 2.82x |
  | matmul (600x8) | 290 | 2861 | **9.86x** |

  **matmul at ~10x is a new and much larger gap than the 2-3x recorded elsewhere in this file** — gcc vectorises the inner loop and mcc emits scalar code, so it is the sharpest available evidence for the auto-vectorization item, and the natural kernel to track it with. As a self-check the harness reproduces the known `OPASSIGN`+`MATH_INLINE_PREPASS`+`PROMO_LEAF_CALLEE` win independently: nbody **-14.5%** (the FP/compute item above recorded -14% from an ad-hoc measurement), with the other three flat to noise.
  Kernels are the in-tree plb C kernels mcc can build (`nbody/2.c`, `nsieve/1.c`) plus `tests/runtime/``mandelbrot.c` and `matmul.c` (committed, so the ad-hoc programs the spill-weight sweep used are no longer unreproducible). Sizes are tuned so each takes ~0.3-0.5 s under gcc — at the sizes first tried everything ran in 6-57 ms and process startup dominated. **Excluded kernels, with reasons, so they are not re-investigated:** `spectral-norm/3.c` (plain C99 `inline`, unresolved `A` — the AOT item), `nbody/5.c` (x86 SIMD builtins + vector types, no `VT_VECTOR` in mcc), `binarytrees/2.c` (libapr), `mandelbrot/1-ffi.c` (libcrypto MD5). Remaining: run it on the arm64-native CI cell so a flip that helps one arch cannot silently regress the other — that is the cross-arch half of this item and still needs CI wiring; and record a baseline JSON to diff against once the numbers are trusted. Note the earlier Apple-Silicon figures (nbody 3.3x / spectral 4.0x / nsieve 2.1x vs Apple clang) are a different host AND a different reference compiler, so compare them only as trends.
- **`MCC_AST_CHAINSTORE=1` + `MCC_AST_PROMO_INCDEC=1` miscompiled at -O2 — FOUND AND FIXED 2026-07-26 the same day.** Each gate alone was correct, so only the combination failed. Minimal repro (no call, no FP needed — three variants all fail):

```c
static double v[64];
static double mv(int n){int j;double sum;
  for(sum=j=0;j<n;j++)sum+=v[j]/(j+1);return sum;}
```

**The error is a CONSTANT +512 offset in every variant measured** (21.837641318 → 533.837641318, 274.975563615 → 786.975563615, 413568 → 414080), i.e. the chained initialisation leaves the OUTER target holding a wrong starting value rather than corrupting the loop. That points straight at the interaction: `CHAINSTORE` makes the function faithful so the optimizer now runs on it, `PROMO_INCDEC` then promotes the counter `j` into a register, and the chained store's value — which is shared with `j`'s store — ends up referring to the promoted register that the loop subsequently increments, instead of the constant. **Confirmed from the disassembly and fixed.** Without promotion the inner store emits `mov $0x0,%eax; mov %eax,-0xc(%rbp)` so the outer `cvtsi2sd %eax` is correct; with promotion it emits `mov $0x0,%r9d` straight into the promoted register and `%eax` is never written, so the outer store converts a stale register. The copy is finalized with `vtop`, i.e. it records "the value is in the register the inner vstore left it in" — true only if the inner store actually materialises it there, which a promoted target does not. Fix: tag the adopting store in `ast_hook_vstore` and have `ast_plan_promotion` decline to promote the INNER target of a tagged pair. Cost is one un-promoted counter in chained-init loops; correctness is restored with the faithfulness win intact (`f1`-`f4` all faithful, spectral still −3.2% instructions). **Note what caught it and what did not:** the exec corpus passes 298/298 with each gate and with the whole loop-gate set, all five runtime-bench kernels verify, and three self-host fixpoints are byte-identical — it only surfaced from running a REAL kernel variant (spectral with `A` made `static`) through the instruction-ratio comparison. Gate-combination coverage over the corpus was the missing test, and it now exists as a habit: the exec corpus was re-run under `CHAINSTORE`, `PROMO_INCDEC`, both together, and both plus `OPASSIGN`+`IVSR_PTR` (298/298 each), alongside byte-identical 3-stage self-host fixpoints on x86_64/riscv64/arm64 with the pair, a 40-seed differential 80/80, all runtime-bench kernels verifying under two combinations, ctest 5906/5906 and asttool 747/0. **Run gate COMBINATIONS, not just each gate alone — every single-gate check passed while the pair miscompiled.** That is now enforced: `exec-chainstore/<case>` runs the whole exec corpus at -O2 under `MCC_AST_CHAINSTORE=1;MCC_AST_PROMO_INCDEC=1;MCC_AST_OPASSIGN=1` (298 cases), mirroring `exec-ivsrptr`. **The variant alone was NOT enough and that is worth knowing**: with the fix reverted the corpus still passed 298/298, because nothing in it wrote a chained-init reduction loop. So `tests/exec/statements/chained_assign.c` was added (+ goldens entry) covering the reduction form plb spectral-norm uses, an int-only chain, a four-deep `a = b = i = j = 0`, a chain used as a value inside an expression, a mixed int/float/double chain, and repeated re-assignment — verified to FAIL without the fix (`reduce 98.371945452` vs `34.371945452`) and pass with it. A combination variant is only as good as the shapes the corpus contains.
- **Chained assignment `a = b = 0` was recorded UNFAITHFULLY — FIXED 2026-07-26 behind `MCC_AST_CHAINSTORE` (default OFF below `-O3` ⇒ byte-identical). The cause was one line of ORDERING, after four attempts down blind alleys.** Four-line repro, all four functions differing only in how the two variables are initialised:

```c
double f1(int n){int j;double sum;for(sum=j=0;j<n;j++)sum+=v[j];return sum;}  /* unfaithful */
double f2(int n){int j;double sum;for(sum=0,j=0;j<n;j++)sum+=v[j];return sum;} /* faithful   */
double f3(int n){int j;double sum=0;for(j=0;j<n;j++)sum+=v[j];return sum;}     /* faithful   */
int    f4(int n){int j,k;for(k=j=0;j<n;j++)k+=j;return k;}                     /* unfaithful */
```

`f4` is int-only and still fails, so **the defect is the chained assignment itself, not the int→double conversion**. `MCC_AST_VERIFY_DIFF` localises it precisely: baseline 190 B vs replay 185 B, and the missing 5 bytes are exactly a `b8 00 00 00 00` (`mov $0x0,%eax`) that the baseline emits before storing to the inner target and the replay does not — the recorder models `b = 0` and `a = b` without reproducing the shared materialisation of the constant into a register.
  **Why it matters more than it looks: `vendor/plb/spectral-norm/3.c` writes its inner loop as `for (sum = j = 0; j < n; j++)`, so `mult_Av` and `mult_Atv` — the only hot functions — are `unfaithful` and NO AST pass runs on them at all.** That is why spectral is immune to every gate: `PROMO_INCDEC`, `IVSR_PTR`, `PROMO_LEAF_CALLEE`, `PROMO_LEAF_XMM` and -O3 inlining each measured **+0.0% instructions** on it, and `A()` is still called per element (2 call relocs) even when rewritten as `static`. Fixing this one recorder shape unblocks the second-worst kernel in the suite (7.41x instructions, 4.3x time) — and unlike the deferred `?:`/`||` store clusters it is a small, self-contained shape with a four-line reproducer.
  **A SEPARATE, real defect was found while attempting this and is fixed behind `MCC_AST_CHAINSTORE` (default OFF below `-O3` ⇒ byte-identical): chained assignment builds a NON-TREE AST.** An assignment yields its value, so in `a = b = v` the inner store's value node is left as the expression result and the outer store adopts it — and `ast_add_child` REPARENTS, leaving the node in the inner store's child list while its parent points at the outer one. Measured with a print in `ast_add_child` on unmodified mcc: `k = j = 0` logs one reparent, the equivalent `k = 0, j = 0` logs none, and **compiling mcc's own amalgamation logs 314**. The fix gives the adopting store its own deep copy, exactly as `ast_hook_vpush` already does for the compound-assign vdup; only a value that already has a parent is copied, which is why it is byte-identical (506 exec+behavior objects at -O0/-O2, the self-compiled amalgamation, and a byte-identical 3-stage self-host fixpoint), with ctest 5906/5906, exec corpus 298/298 gate-on, asttool 747/0 and all arches building. It removes a latent hazard for anything that assumes a tree (`ast_dup_sub`, the identity hashes, the slice work).
  **It does NOT fix the faithfulness problem, and the intermediate result that suggested it did was an artifact.** Copying on EVERY store (rather than only on re-adoption) made the four-line reproducer report faithful — but it also took the amalgamation from 159 unfaithful to 595, i.e. it perturbed node numbering rather than fixing anything. With the correct targeted copy the reproducer is unchanged, so the chained-assignment unfaithfulness has the separate cause diagnosed below and that item stays open.
  **Attempted 2026-07-26 and REVERTED — the shape of the fix is now known, and it is bigger than it looks.** The faithful model is clear: the parser handles `a = (b = v)` by materialising `v` ONCE and leaving it on the vstack for the outer store, so the AST must let a Store be an EXPRESSION — record the inner assignment's result as the Store node itself, flag it so `ast_replay_bb` skips it as a statement, and give `ast_replay_value` an `AST_Store` case that emits lval/value/`vstore()` WITHOUT the trailing `vpop`. That reproduces the parser's emission order exactly. Implemented behind `MCC_AST_CHAINSTORE` and it **segfaults the compiler immediately**: putting a Store node on `ast_vs` breaks every consumer that assumes an expression operand is a typed value node (`ast_finalize_leaf`, `ast_ident_etype`, the type queries) — a Store carries no expression type at all. **Second attempt the same day narrowed it further and also failed, so record what the blocker actually is.** The first theory (untyped Store node breaking `ast_finalize_leaf`) is WRONG — that function already returns early for any kind other than `AST_Literal`/`AST_Ref`. The second theory was right about a real hazard but did not fix it: `ast_add_child` sets `next_sib[child] = AST_NONE`, so nesting a store that was already appended to the basic block TRUNCATES the BB's statement list at that point; the append must be deferred until the statement ends (keep the newest store pending, let an enclosing store replace it, flush the survivor in `ast_hook_vstore_end` when `ast_in_op` hits 0). With that deferral implemented it STILL segfaults, and gdb names the real failure: **infinite recursion in `ast_ih_node` (mccast.c), the identity-hash walker, revisiting the same node forever — i.e. the construction produces a CYCLIC AST**, not a tree. **Attempt three instrumented `ast_add_child` and the cycle is now fully explained — the remaining problem is a single design question, not a mystery.** Dumping every insertion for `int f(int n){int j,k;for(k=j=0;j<n;j++)k+=j;return k;}` shows: the inner store (node 4) IS appended to the basic block, then reparented into the outer store (node 5), and appending 5 to the BB then sets `next_sib[4] = 5` while 4 is already a CHILD of 5 — walking the BB's children goes 4 → 5 → 4 forever, which is the `ast_ih_node` stack overflow. **The deferral fired at the wrong moment: `ast_in_op == 0` in `ast_hook_vstore_end` is NOT a statement boundary**, because in `a = (b = v)` the inner `vstore()` completes entirely before the outer one starts, so the pending store is flushed to the BB just in time to be corrupted.
  **What attempt four needs is a correct flush point, and the constraint is sharp.** Flushing in the next `ast_hook_vstore` (append the pending store only if the new store is NOT consuming it) handles the nesting, but something must still flush the last one, and `ast_hook_stmt` — the only statement hook — fires at statement START. Deferring to the next statement is WRONG for a condition expression: `if (a = b = v)` would flush into whatever `ast_cur_bb` has become (the then-branch), putting the store in the wrong block. So the flush needs either a real end-of-expression-statement hook or an `ast_cur_bb`-aware flush that runs before any control-flow hook switches blocks. That is the whole remaining problem; the recording, nesting and replay pieces all work. Reproduce the trace with a `getenv` print at the top of `ast_add_child` — it names the corrupting insertion in one run.
  **Attempt five found it, and NONE of the replay machinery from attempts one to four was needed.** `ast_hook_vstore` finalizes the value node with `ast_finalize_leaf(ast_vs[ast_vn-1], vtop)` — and at the OUTER store of `a = b = v` that node is SHARED with the inner store, so finalizing it overwrites the SValue the inner store recorded (a constant) with the state `vtop` has now (the register the inner `vstore` left the value in). The inner store's value then claims to be register-resident, its replay skips the `mov $0x0,%eax`, and both stores read a stale register — exactly the 5-byte diff seen from attempt one onward. **Fix: take the deep copy BEFORE `ast_finalize_leaf` and finalize only the copy**, so the inner store keeps what it recorded. That is the whole change; the tree-invariant repair above and the faithfulness fix are the same two lines.
  Measured: the four-line reproducer is fully faithful (`f1`/`f4` flip), the mcc amalgamation goes **159 → 155 unfaithful**, and `mult_Av`/`mult_Atv` in plb spectral-norm become faithful — which finally moves the kernel every other gate left at exactly +0.0%: **spectral −3.2% instructions retired, −4.5% wall-clock**. Validated: gate-off byte-identical (self-compiled amalgamation at -O0/-O2/-O3 vs the pre-change binary, 506 exec+behavior objects), ctest 5906/5906, exec corpus 298/298 gate-on, asttool 747/0, all 5 arches build, all runtime-bench kernels verify vs gcc, a 40-seed randomized differential 80/80, and byte-identical 3-stage self-host fixpoints on **x86_64, riscv64 AND arm64** with the gate on.
- **String-literal `L.N`/anon-symbol object-layout sensitivity** (3 exec files excluded from object-diff). String literals are named `L.<v − SYM_FIRST_ANOM>` (mccpp.c) off the global `anon_sym` counter, which is *shared* with anonymous structs/unions/compound-literals and struct tags, so a literal's symbol name shifts with unrelated preceding anon consumers and with any pass that allocates anon symbols in a different order/count (e.g. `"hello"` is `L.2` alone but `L.5` when a `struct{union{int a;int b;};}` precedes it). Same-source output is byte-deterministic; the sensitivity is to code structure / opt-order / cross-compiler naming. Fix (deferred, large): name rodata literals from a literal-local stable key (content hash or per-`.rodata` emission index) independent of `anon_sym`, then re-include the 3 files — renames every rodata-literal symbol ⇒ full object-golden regen + M8 soak.
- **Promote/inline replay-fidelity gap set** (763 baseline gaps: 721 desync / 33 unfaithful / 9 stackresidue). Root-caused to a handful of AST-recorder bail sites in `mccast.c`, each an unmodeled construct that diverges the recorder's value-stack/AST model from real codegen. Bail sites: ~2409 `ast_hook_vstore` on a store inside a ternary/`||` branch (the largest named cluster — the glibc `<stdio.h>` inline family `putchar_unlocked`/`putc_unlocked`/`fputc_unlocked`, since `__putc_unlocked_body` expands to `cond ? (*p++ = c) : __overflow(...)`); ~1452 `ast_hook_vpush` value-stack-depth mismatch (a downstream detector — some earlier unmodeled push/pop already diverged the model); ~2135 member/bitfield aggregate store; ~2294 `ast_hook_call_begin` nested in-call. The ~1993 case (`ast_hook_vpush` declining a register-resident lvalue `r=VT_LVAL|reg`, produced when a **compound assignment `op=` through a pointer to a struct member** `vdup`s the member lval to reload it — the one that rejected nbody's hot `advance()` and blocked the whole FP/compute-loop + ROI work) is now MODELED behind `MCC_AST_OPASSIGN` (default OFF below `-O3` ⇒ byte-identical): a new `ast_hook_vdup` (armed from `expr_eq`'s compound-assign path just before `vdup()` when the top lval is pure) duplicates the top node so the recorded shape is the ordinary `Store(lval, Binary(op, lval_copy, rhs))` tagged `AST_OP_OPASSIGN`, and the statement-context replay re-emits the byte-faithful single-address-computation `vdup` form (replay also structurally requires `value == Binary(op, X==lval, rhs)` + pure lval, else it falls back to the always-correct baseline). This ungates `advance()` and the `p->field op= …` class into the replay/optimizer path (a REPLAYABILITY unlock only — the loop passes still don't fire on it, see the FP/compute item). Cross-arch differential DONE for `MCC_AST_OPASSIGN` + `MCC_AST_PROMO_ARROW` (both arch-neutral): bit-matches gcc at -O0/-O2/-O4 on x86_64/arm64/riscv64/i386 (nbody + 20k-op randomized compound-assign fuzz + a struct/linked-list/array battery); gates OFF byte-identical, gates FIRE on all four; arm32 not covered (mcc emits `EABI version 0` vs the gnueabihf toolchain's version 5 — needs the soft-float/static-link harness). Seed fuzz campaign DONE 2026-07-25 (docker/x86_64): a UB-free integer generator (random compound-assign-through-pointer / member+array access / two promoted pointers, per seed) run over **30 000 distinct programs** — gates FIRE on 100%, and mcc-gates-ON ≡ mcc-gates-OFF at runtime on **all 30 000 (zero divergence)** — plus a 3 000-seed mcc-ON-vs-gcc pass (0 mismatch) for absolute correctness at scale, and ~842 seeds at -O4 (the search path) also clean. Remaining before flipping `MCC_AST_OPASSIGN` on: AOT==JIT-on-arm64 (needs the JIT engine, qemu can't), and a native-arm64 3-stage self-host. Still-deferred sibling bails (model conditional stores under `?:`/`||` — the 2409 cluster, correct store placement under the conditional is the hard part — plus the constructs feeding 1452 and the ~2135/~2294 aggregate/nested-call bails): large, risky, the AST substrate is desync-fragile.
- Model the AST recorder's `desync` bucket shapes (struct-by-value ABI, bitfields, VLAs, C11 atomics, `__attribute__((cleanup))`, `_Complex`, `_Noreturn`, inline asm) — the recorder declines these, each an unmodeled construct that can silently miscompile once the fidelity net is retired.
- Regenerate the `ast-verify-ratchet` baselines for the non-win32/linux targets — they SKIP until regenned.
- **Skip-audit + max-coverage provisioning** (anti-false-green). Many suites `mcc_skip_test`/`SKIP_RETURN_CODE 77` for *installable or configurable* reasons rather than genuine host incapability, and those skips are silent — a suite can no-op while ctest/CI stays green. Build a per-host capability audit: enumerate every skip gate, classify each as legitimately HW/arch-gated vs provisionable-on-this-host, and document the exact install/config to un-skip each — diff3 needs 2 *distinct* reference compilers (`MCC_DIFF3_GCC`/`_CLANG` or `MCC_DIFF3_EXTRA_REFS`); the structural cli/diff3 cases need binutils `nm`/`readelf`; diff3/preprocess on Windows need a POSIX shell (`MCC_TEST_SH`); i386-fastcall/`-fPIC`-TLS need `linux/386` docker; cross-arch exec needs qemu-user + sysroots. Then add a ctest/CI assertion that each expected suite actually *ran* ≥N tests on platforms that support it (parse the ctest-junit run-vs-skipped counts), failing the cell on a silent no-op. First increment landed for diff3 (`ci junit-assert <xml> --expect diff3/=1`); generalize across preprocess/cli-structural/i386-fastcall/cross-exec.
- Audit `mcc_skip_test` per-triple ungating (i386-linux, aarch64/armv7-linux).
- i386-fastcall off-i386: optional leftover — teach the native `suite_i386fastcall` (tools/mccharness.c) a Docker execution backend so the mingw/`-m32` harness path works off-i386; confirm CI actually provides `linux/386` docker.
- Run the `arm-inline`/`riscv64-inline`/`riscv64-promote` docker differentials natively on arm64 too (parameterize the build-stage platform like `extlink-docker.sh` does with `_extlink_host_plat`, since the qemu-user target stage is host-arch-independent). Currently arm64 hosts get zero coverage of these three (only x86_64 does); on arm64-macOS Docker-Desktop/Rosetta they double-emulate and false-fail — skip them locally with `ctest -E 'riscv64-(inline|promote)-docker'`.
- **Link-time/ABI mixed-object fuzzer.** The fixed-corpus mcc↔gcc ABI differential is landed (`abidiff-docker.sh` crosses mcc/gcc object mixes over struct-by-value/HFA/vararg/`_Complex`/TLS/weak/alias cases on arm64/riscv64/armv7/amd64; `extlink-docker.sh` links a single mcc `.o` with GNU ld and checks distinct DWARF low_pc). Remaining: the *fuzzed* version (current guards are fixed-corpus); a native (non-docker) Linux-CI variant so the guard runs in the normal build; `long double` + returned-HFA edge cases.
- Coverage-guided generation (gcov/Intel-PT into `tests/fuzz/gen.h`).
- EMI mutation (Orion/Athena/Hermes) targeting optimizer miscompiles.
- DWARF: variable locations under heavier `-O1+` register promotion, lexical-block scoping; riscv64/armv7 aren't gdb-tested here (qemu-user has no ptrace) but share the line-table fixes.
- armv7 `.ARM.exidx` emission path IF mcc-C ever needs to be unwindable-through in a C++/`-funwind-tables` context (matches gcc today — a feature gap only if that use case arises).
- Provide a runnable test for the kernel-fused libSystem path (`MCC_DARWIN_HOST=ON`) — nothing exercises it.
- Promote the `mingw i686` CI cell off `continue-on-error`/`matrix.experimental` once i386-Windows codegen is proven.
- Confirm the `-fexcess-precision=standard` i386 WIN32 refflags fix (CMakeLists.txt) on the `windows-2025-vs2026` UCRT CI image — speculative, only reproduces there.
- Validate stack auto over-alignment on i386-PE and arm64-PE (the test stays x86_64-gated).
- Normalize CMake incrementally (autodetect + fold `.cmake` files, verifiable target).
- Cut CI wall-clock: gate `bench`, shard macOS ctest, prune matrix re-runs.

## macOS (Mach-O / Darwin)
- **No macOS recorder-fidelity baseline exists.** `tests/ast/verify-baseline/` holds only `x86_64-linux.txt` and `x86_64-win32.txt`, so `ast-verify-ratchet` cannot ratchet on Darwin — the gap set there is unmeasured, which matters now that the 2026-07-26 work moved the linux count (159 → 155 unfaithful with `MCC_AST_CHAINSTORE`) and added two exec files. Generate an `arm64-osx` (and/or `x86_64-osx`) baseline with `tests/ast/verify_ratchet.cmake -DREGEN=1` on a Mac and wire the key, or record explicitly why Darwin is excluded so the silence is deliberate rather than accidental.
- **Validate the 2026-07-26 gates on arm64-macOS.** `MCC_AST_CHAINSTORE`, `MCC_AST_PROMO_INCDEC`, `MCC_AST_IVSR_PTR` and the `exec-ivsrptr`/`exec-chainstore` combination variants have run on linux-x86_64 natively and on riscv64/arm64 under qemu, never on Darwin. arm64-macOS is the one place the arm64 backend runs NATIVELY (qemu is x86-TSO), so it is also the only place their interaction with `opt_promote` and the arm64 promotion pools gets a real memory model. The kernels are cheap to check: nbody, nsieve, mandelbrot, matmul and spectral all matched the gcc reference cross-arch, so a Darwin mismatch would be a genuine finding.
- **Verify `-fc99-inline-body` on Mach-O.** Same two-TU test as the PE item: the flag emits a weak out-of-line body so several TUs collapse onto one copy, and `src/objfmt/mccmacho.c` has `N_WEAK_DEF`/`EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION` handling, but the path has never been exercised. Also confirm the inlining half works there — on Linux the flag takes plb spectral-norm's `A()` from 2 calls per element to 0 and the kernel from 7.29x to 5.76x gcc, and the same was confirmed on riscv64 and arm64 under qemu.
- **`tools/runtime-bench.py` on macOS: reference compiler resolves to clang (fine), but `perf` does not exist**, so the `insn/ref` and `insns` columns silently vanish and only wall-clock remains — which is exactly the measurement that misled repeatedly on Linux (nsieve reads −9.1% on time while being +1.5% on instructions). Either add a Darwin instruction-count path (`dtrace`, `/usr/bin/time -l`, or Instruments) or make the harness say out loud that it is timing-only on this platform, so a Mac reading is not mistaken for a Linux-grade one.
- **No cross-arch self-host gate on Darwin.** `tools/selfhost-cross-native.sh` needs qemu-user plus the vendored gentoo stage3 sysroots and will skip 77 on macOS. Since arm64-macOS runs the arm64 backend natively, the equivalent there is a NATIVE 3-stage self-host rather than a cross one — worth wiring, because it would be the only non-qemu arm64 self-host in the tree.
- **P0 STEP 1 — highest-leverage item in this file.** Build a real Mach-O relocatable `.o` reader/writer — `mcc_object_type` only sniffs ELF/ar today; unblocks bare-dylib frameworks, host-CC embed, `ld` interop, AND the macOS JIT self-host (Mach-O `--embed-jit` can't resolve `_mccjit_boot_swap`).
- Add a built-in ad-hoc codesigner — currently shells out to `/usr/bin/codesign`.
- Wire `machofat` into `dist-macos` packaging.

## Recurring codegen review checks (latent-bug-class guards)
- **Never forward-declare another translation unit's `static` helper.** `mccgen.c` declared arm64 `intr`/riscv64 `ireg` as `static ...; /* defined later in the same TU */`, which holds only in the amalgamated build; the multisource cells (`cmake-linux-gcc-multisource`) compile `mccgen.c` separately and `libmcc.so` got an undefined `intr`, breaking every exe that links it. Cross-file helpers must be `ST_FUNC` (= `static` when `MCC_AMALGAMATED`, external otherwise) and declared that way at the use site. Local amalgamated builds and `cmake-debug` cannot catch this — reproduce with `gcc -c -DMCC_AMALGAMATED=0 ... src/mccgen.c` and check `nm -u` for the symbol.
  **Checked 2026-07-27: the `linux-gcc-multisource` PRESET runs locally and is currently clean, so this does not need CI to exercise** — `cmake --preset linux-gcc-multisource && cmake --build --preset linux-gcc-multisource` builds 120 objects with `-DMCC_AMALGAMATED=0` (so `ST_FUNC` really is `extern` there), `nm -u libmcc.so` shows only libc imports, and its ctest is **7276/7276**. Worth running before any change that moves helpers between files; it is not expensive.
  **CAVEAT, stated because the claim above it is stronger than what I could verify: I could NOT construct a mutation that this build catches and `cmake-debug` misses.** Declaring a nonexistent helper `static` fails in BOTH builds (compile error either way, so it proves nothing about multisource). Declaring a real cross-file static (`ast_treechk_on`, defined `static` in mccast.c) and calling it under a runtime-false guard was accepted by BOTH — amalgamated rc=0 as expected, but multisource rc=0 too, with mccgen.c genuinely recompiled at `MCC_AMALGAMATED=0`. So the guard's effectiveness against the exact recorded shape is UNVERIFIED here; the historical failure was a link error on `libmcc.so`, which suggests the trigger needs the symbol to be reached by a live call rather than a dead branch. Do not assume this preset is a sufficient gate for the bug class until a mutation reproduces it.
- On any new front-end `r`-field flag, extend the `sv->r & ~(...)` mask in `arm64-gen.c` `load()` and the twin in `arm64-asm.c` `arm64_memory_needs_address_reg` — else arm64 `assert(0)` slips past x86/qemu CI.
- On any new `ast_func_end` dispatcher work, keep the raw-x86 emission and its x86-only static helpers inside the `#if MCC_TARGET_I386||MCC_TARGET_X86_64` guard.
- Audit new pseudo-instruction cases in the arch-asm files for reads of `imm`/`op.e` without a preceding explicit `e.v` assignment (brace-init `Operand` union ⇒ host-compiler-dependent garbage).
- The arm64 runtime mode-6 JIT dispatch slot used to carry an extra `!ast_search_env` guard term (the search re-emits each candidate to MEMORY and the old GOT-based dispatch slot corrupted the fn symbol there). That guard is GONE as of 2026-07-26: the slot's address-of load was hardened from GOT (`ADR_GOT_PAGE`/`LD64_GOT_LO12_NC`) to self-contained PC-relative `adrp/add` (`ADR_PREL_PG_HI21`/`ADD_ABS_LO12_NC`), which is re-emit-safe, so `-O4 -run` now gets BOTH search-opt AND mode-6 JIT submit/override. If you touch the arm64 dispatch slot, keep the address-of-slot computation PC-relative and self-contained (no GOT reloc against the local slot symbol) or the search re-emit corruption returns.

## Search vocabulary / strategy long tail
- **Variable re-layout axis: scalarize ↔ aggregate as a search dimension.** A new pair of inverse transforms over local storage, exposed to the JIT/AOT search as a layout axis rather than a fixed lowering. **Scalarize (split / SROA):** a compound local (struct/array — an `AST_Ref` with `op` VT_LOCAL over an aggregate `type_t`/`type_ref`, plus its `Load`/`Store`/member offsets) whose fields are accessed independently is replaced by N independent scalar locals, each separately register-promotable, DSE-able and const-foldable. **Aggregate (pack / fuse):** the inverse — several scalar locals with correlated liveness/access are coalesced into one compound slot so they load/spill/move as a unit (adjacency, one spill, SIMD-lane packing). The point is not one canonical choice but that BOTH directions become moves the search can try: each re-layout changes the slice's live-in/live-out signature, so it multiplies the space the engine already explores — permutations (orderings) × combinations (which locals group) × slice windows — and each variant is a distinct slice identity with its own register/spill outcome to benchmark. Hooks into existing machinery: the live-in boundary is `ast_slice_live_ins`; identity is `ast_slice_ident_hash` (which already positionally interns local-Ref offsets, so a re-layout that changes the sharing pattern already hashes differently); slot allocation is `ast_alloc_loc`/`ast_alloc_temp_loc`. **This is the concrete transform that most needs the Divorce §'s two missing pieces:** item 2's ins/outs hash (so a re-laid-out slice is recognized as the same kernel with a different boundary) and item 3's **Optimization Reconciler** (re-wiring entry/exit when the optimized form needs fewer/more live-ins is EXACTLY what changing a local's layout does). Correctness gates before any of it fires: no address-taken/escape on a scalarized aggregate, no aliasing or type-punning across fused/split members, and live-range compatibility for a pack (two locals simultaneously live can't share one non-overlapping slot). Gate default OFF ⇒ byte-identical; codegen moves only when a warm slice-cache value steers it, validated by exec/diff parity like the rest of `MCC_AST_SLICE`.
- Loop-nest precision: symbolic bounds, fewer non-affine bail-outs, asttool dep suite (blocked by `MCC_INTERNAL`).
- True 2-D loop tiling (strip both loops).
- Range/known-bits lattice remaining.
- Adaptive beam width; per-function scoping.
- Hot-slice budget allocation (needs emit isolation).
- `-g` hot-value cache.
- Post-graft window dataflow decision (splice-then-reanalyze vs two-pass).
- Seam peephole window; window-level cache key; scoring of the de-spill delta.
- Speculative arm insertion (revisit only with the 3-stage self-host gate).
- Widening/fixpoint dataflow for cross-iteration value merging.
- `switch`-arm detection form.
- Inline budgets as a search value-axis (needs emit isolation); more param shapes; inline cross-TU static callees; heuristic non-static inlining.
- Arena-mutating pass-subset re-emit axis; promotion re-emit axis (needs emit isolation).
- Rewrite-rule IR; instruction-level superoptimization.
- V-strategy variations (bfold/ident/narrow/cprop/cse/licm/dse/sccp/jt/bf/sethi/tco) — widen search vocabulary.
- Build the epoch hash (invertible slot-keyed edit patch).
- `.rodata` data-emission project (prerequisite for value-table dispatch).
- Design cross-TU LTO; separate `-O2`/`-O3` SSA drivers; time-budgeted engine; dependency-ordered `-O1`; broader template library.
- Formula-family unification; graduate the disk search-memo into compiled-in strategies.
- FLOAT: emit-size scoring under the tick scheduler + JIT-runtime scoring; C11-thread pool with per-context state (needs emit isolation).
- Revisit: `k` always-inline depth policy; size-gated outline; store factoring; template DSL past ~30 templates; per-function `-O1`; PP-as-executable-C JIT; human-friendly diagnostics vs terminal geometry.
