# The emit map — mcc compiling itself and `full_language.c`, measured 2026-08-11

A map of what actually runs when mcc compiles a body: which layers fire, how many machine-code
emit calls each one costs, and where bodies fall out between layers. Taken on the two targets
asked for — mcc's own source (`src/mcc.c`, the unity TU) and `tests/diff/full_language.c` — and
not on a corpus, so every number here is attributable to one of two named inputs.

This is the companion to [`docs/EMIT-COVERAGE.md`](EMIT-COVERAGE.md), which counted *how many*
bodies each layer handled. That document closes by saying it cannot say **why** the
recorded-but-not-verdicted bodies drop out. This one names them.

## How it was taken

Three instruments, all already in the tree except the third:

| instrument | gate | what it gives |
| --- | --- | --- |
| `MCC_TRACE` (`src/mcclog.h`) | build `-DMCC_CONFIG_TRACE=ON`, run `MCC_LOG=128` | an ordered per-basic-block event stream, ~14,900 call sites |
| `MCC_INV` (`src/mccinv.h`) | run `MCC_INV=1` | the four per-layer decision counters, validated non-perturbing |
| `tools/emit-map.py` | — | segments the trace by compiled body and reconciles it against `MCC_INV` |

**`src/mccrir.c` had to be instrumented first.** `tools/tracegate.c` only polices files that
already contain the literal `MCC_TRACE(`, and `mccrir.c` contained none — so the layer that makes
the record/skip/discard decision emitted **zero** trace events and was invisible to the tracer by
construction. 637 sites were added (247 function entries, 390 braced branches), inline after each
`{` so that **the file's line count is unchanged at 6,492** and existing anchors do not drift.
`trace-gate-invariant` passes over the result.

**Non-perturbing, checked rather than assumed.** Objects emitted by the instrumented trace build
are byte-identical (`cmp`) to those from the pre-change compiler, for both targets at both `-O0`
and `-O1`. The default build is unaffected in principle as well as in fact: without
`MCC_CONFIG_TRACE` every added site preprocesses to `((void)0)`. 72 of 72 relevant regression
cells pass, including `selfhost-smoke`, `selfhost-fixpoint`, `ast/o0-baseline`, `rir-parity` and
`mcctest` (the `full_language.c` differential oracle).

`tools/emit-map.py` resolves its file:line anchors **by symbol** at run time — `docs/TODO.md`
records that hardcoded anchors in this tree drifted 1000–1900 lines across merges.

## 1. The map

`full_language.c` at `-O1`, **18,152,102 trace events**, by source file:

| share | events | file | layer |
| ---: | ---: | --- | --- |
| 29.01% | 5,265,155 | `mccast.c` | AST |
| 22.27% | 4,043,349 | `mccpp.c` | preprocessor |
| 21.41% | 3,886,998 | `mccgen.c` | AOT codegen |
| **9.99%** | **1,813,472** | **`mccrir.c`** | **RIR — previously 0.00%** |
| 7.15% | 1,298,754 | `x86_64-gen.c` | AOT codegen (arch) |
| 5.23% | 950,190 | `mccircap.c` | JRN / `ir_cap` |
| 1.55% | 280,500 | `mcccst.c` | CST |
| 1.52% | 276,384 | `libmcc.c` | driver |
| 1.17% | 211,476 | `mccelf.c` | object writer |
| <0.4% | 125,807 | `i386-asm.c`, `mcchost.c`, `wide256_slice.h`, `mccdbg.c`, `mccasm.c`, … | rest |

**A tenth of the compile was unmapped.** RIR's own hot spots — `rir_stamp_sv` 272,948,
`rir_resolve` 241,496, `rir_stamp_flt_fold` 118,122, `rir_build` 99,665, `rir_to_arena` 91,554 —
were not visible to any instrument in the tree before this.

"JRN" is `ir_cap` (`src/mccircap.c`): `jrn_*`/`JrnOp`/`JOP_*` were renamed to
`ir_cap_*`/`IrCapOp`/`IR_OP_*`, per `docs/ARCHIVED.md`. It has no named hooks — it is spliced into
codegen by **61 `#define <primitive> ir_cap_<primitive>` macro shims** (`src/mccgen.c`), which is
why it never appears as a call in the codegen source text.

For scale, the self-host stream measured **212,695,219 events across 17 files** *before* RIR was
instrumented.

## 2. The catalog

Six arms, two targets. `bodies` is `rir.body`; `verdict` is `ast.body`.

| arm | bodies | `rir.rec` | verdict | faithful | `jit.baked` | `g()` bytes | surviving | **amp** |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `FL -O0` | 303 | **0** | **0** | 0 | 0 | 115,965 | 115,934 | **1.0003** |
| `FL -O1` | 305 | 305 | 301 | 291 | 0 | 295,810 | 115,781 | **2.5549** |
| `FL -O0 --embed-jit` | 303 | 303 | 299 | 285 | 41 | 226,381 | 116,180 | **1.9485** |
| `self -O0` | 3165 | **0** | **0** | 0 | 0 | 1,662,057 | 1,662,057 | **1.0000** |
| `self -O1` | 3167 | 3167 | 3165 | 3095 | 0 | 4,489,590 | 1,655,937 | **2.7112** |
| `self -O0 --embed-jit` | 3165 | 3165 | 3163 | 3055 | 1550 | 3,267,318 | 1,671,357 | **1.9549** |

The `-O0` rows reproduce `EMIT-COVERAGE.md`'s central finding on both named targets: at `-O0` the
RIR and AST layers are **absent, not idle** — 0 of 3165 bodies recorded self-host, and not one
`ast.*` counter increments.

## 3. Every machine-code byte is emitted 1, 2 or ~2.7 times, and the `-O` level picks which

This is the finding the map exists to produce. `g()` is the single byte-emit primitive for
x86_64 — `o()`, `gen_le16/32/64` and every instruction encoder bottom out in it — so counting its
calls, minus the ones that early-return on `nocode_wanted`, counts **every machine-code byte the
compiler writes**, including the ones it later throws away. Against `aot.bytes`, which counts only
the bytes that survive into the object:

| arm | amplification | what the extra passes are |
| --- | ---: | --- |
| `self -O0` | **1.0000** | nothing. 1,662,057 bytes written, 1,662,057 survive — an exact identity |
| `self -O0 --embed-jit` | 1.9549 | parser pass + **exactly one** replay pass (`replay_per_verdict` = **1.0000**) |
| `self -O1` | 2.7112 | parser pass + **1.62** replay passes/body — 1964 of 3165 bodies replay more than once |

At `-O0` the compiler writes each byte once and keeps it. Turning the pipeline on doubles that:
the parser emits a body, the AST layer rewinds `ind` to the body's start and replays it, and the
loser is discarded. `-O1` then adds a *third* partial pass — and the arm that isolates it is
`-O0 --embed-jit`, which runs the whole record/replay pipeline at **exactly one replay per body**
and zero multi-replay bodies. So the extra passes at `-O1` are the **optimizer trying more than
one candidate**, not the record/replay machinery.

Two consequences worth stating plainly:

- **Any instrument that counts emit *calls* over-reports surviving bytes by up to 2.7×.** The two
  numbers are not interchangeable, and the ratio is a function of the `-O` level.
- `FL -O0` is 1.0003, not 1.0000 — **31 bytes**, 0.027%, written through `g()` but falling outside
  the `func_ind..ind` window that `aot.bytes` measures. Self-host at `-O0` has **no** such
  residual, so it is specific to this input rather than inherent. Candidate sources, none of them
  confirmed here: alignment padding, which `gen_function()` emits *before* it samples `func_ind`;
  the `diag_only` path in `gen_inline_functions()`, which calls `gen_function()` for diagnostics
  and then rewinds `data_offset`; and top-level `asm`. Attributing the 31 bytes is left open.

## 4. The recorded-but-not-verdicted gap, named

`EMIT-COVERAGE.md` reports 36 bodies recorded by RIR that never reach `ast_func_end`'s verdict,
and says the counters "localise the gap; they do not name a reason". Both drop-out arms are
already braced branches in the instrumented `mccast.c`, so the trace names them without any new
code:

| arm | gap | replay aborted | never replayed | **unexplained** |
| --- | ---: | ---: | ---: | ---: |
| `FL -O1` | 4 | 3 | 1 | **0** |
| `FL -O0 --embed-jit` | 4 | 3 | 1 | **0** |
| `self -O1` | 2 | 0 | 2 | **0** |
| `self -O0 --embed-jit` | 2 | 0 | 2 | **0** |

**The gap closes exactly, on all four pipeline arms.** Two distinct mechanisms, not one:

1. **Replay aborted** — `ast_func_end` installs its own `error_jmp_buf` around the replay; a
   diagnostic raised during replay longjmps into the `} else {` arm. The `mcc_inv_add` calls sit
   *inside* the `setjmp`-success arm, so such a body is recorded, is replayed, does produce a
   `rir_prod_note("fallback")` — and is invisible to every counter. 3 of 4 on `full_language.c`.
2. **Never replayed** — `ast_replay_ok()` is false (the arena has no first child), so the body
   never reaches `ast_replay_body` at all. Both self-host drop-outs are this, and both take the
   `rir_prod_why_set("replayok")` path. On `full_language.c` the single case is `const_func`,
   which takes the same branch with `ast_rir_used` false.

The mechanisms are *target-dependent*: `full_language.c` loses bodies mostly to replay aborts,
self-host only to empty arenas. A corpus average would have hidden that.

## 5. Faithfulness is much worse on these targets than on the torture corpus

`EMIT-COVERAGE.md` measures 9 unfaithful of 1894 verdicted bodies at `-O1` — 0.47%. On the two
targets here:

| arm | verdicted | faithful | **unfaithful** |
| --- | ---: | ---: | ---: |
| `FL -O1` | 301 | 291 | **10 — 3.32%** |
| `FL -O0 --embed-jit` | 299 | 285 | **14 — 4.68%** |
| `self -O1` | 3165 | 3095 | **70 — 2.21%** |
| `self -O0 --embed-jit` | 3163 | 3055 | **108 — 3.41%** |

Between 4.7× and 10× the corpus rate. These are not contradictory measurements — they are
different inputs — but they do mean **the corpus rate is not a safe estimate for real code**;
mcc's own source replays unfaithfully 2.21% of the time. Note also that on both targets
`--embed-jit` at `-O0` is *less* faithful than `-O1` (4.68% vs 3.32%, 3.41% vs 2.21%), matching
`EMIT-COVERAGE.md`'s observation that the `-O0 --embed-jit` arm is where replay diverges.

## 6. `--embed-jit`, not the `-O` level, turns the pipeline on

Reproduced on both named targets: `-O0 --embed-jit` records 3165 of 3165 bodies self-host and
builds 3163 arenas — the same coverage as `-O1` with the optimizer still at zero — and bakes
**1550 bodies**, 49% of those verdicted. On `full_language.c` it bakes 41 of 299, 14%.

## What this does not establish

- **Why** the 3 `full_language.c` bodies abort during replay. The map names the arm they exit
  through, not the diagnostic that unwinds them.
- Whether the discarded pass is ever the *better* code. Amplification says each byte is written up
  to 2.7 times; it does not say the survivor was the right choice.
- Anything about non-x86_64. `g()` is the sole byte primitive on x86_64, but arm, arm64 and
  riscv64 each have an `o()` that writes the cursor directly and bypasses it, so the amplification
  method as written does not carry to those targets unmodified.
- Anything about `-O2`+ or `-O13`, or about the assembler, linker-synthesized PLT/veneer bytes and
  JIT stubs, which reach the section through `section_ptr_add` rather than `g()`.

## Reproducing

    cmake -S . -B cmake-trace -G Ninja -DCMAKE_BUILD_TYPE=Release -DMCC_CONFIG_TRACE=ON
    cmake --build cmake-trace --target mcc
    python3 tools/emit-map.py cmake-trace --target selfhost --opt=-O1
    python3 tools/emit-map.py cmake-trace --target full_language.c --opt=-O1 --per-body /tmp/fl.tsv

Banked as the `census`-labelled cells `emit-map-full-language` and `emit-map-selfhost`
(`tests/emitmap/bank.json`, armed by `MCC_EMIT_MAP=1`). The bank holds ratios and percentages, not
totals. On a build without `MCC_CONFIG_TRACE` the cells return 77 rather than a green zero.
