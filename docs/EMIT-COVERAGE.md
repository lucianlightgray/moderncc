# Emit coverage, `-O0` vs `-O1` — measured 2026-08-11

A fresh inventory of what each layer **actually does**, taken by instrumenting the four layers
at the point where each makes its own decision, and running the result over a corpus. Nothing
here is carried over from another census; where a number disagrees with one, that is a finding
rather than a copy.

## How it was taken

`src/mccinv.h` is an env-gated counter set (`MCC_INV=1`), wired at four sites chosen because
the numbers are *local* there:

| counter | site | what it means |
| --- | --- | --- |
| `aot.fn`, `aot.bytes` | `gen_function()` completion, `src/mccgen.c` | a body finished codegen; `ind - func_ind` is what it emitted |
| `rir.body`, `rir.rec` | `rir_hook_body_begin()`, `src/mccrir.c` | RIR saw a body / decided to record it |
| `ast.body`, `ast.arena`, `ast.faithful`, `ast.parser_bytes`, `ast.replay_bytes` | `ast_func_end()` at the faithfulness decision, `src/mccast.c` | the AST layer reached its verdict, and both byte lengths are in scope |
| `jit.baked` | the embed stash, `src/mccast.c` | a body was accepted for JIT baking |

**Two properties of the instrument, both checked rather than assumed.** It is
**non-perturbing** — an object compiled with `MCC_INV=1` is byte-identical to one compiled
without, verified with `cmp`; if it were not, every number below would be measuring the
instrument. And it is inert when off: 40 compiles in 0.08 s, and `ast/o0-baseline` (which
compares object sha256 across twelve targets) and the whole smoke suite pass unchanged.

Corpus: the first **300** `gcc.c-torture/execute` programs; **284** compile on every arm, so
all four arms have the same denominator.

## The catalog

| arm | `rir.body` | `rir.rec` | `ast.body` | `ast.faithful` | `jit.baked` | `aot.fn` | `aot.bytes` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `-O0` | 1932 | **0** | **0** | **0** | 0 | 1921 | 317,694 |
| `-O1` | 1933 | 1930 | 1894 | 1885 | 0 | 1922 | 316,619 |
| `-O0 --embed-jit` | 1932 | 1929 | 1894 | 1883 | 587 | 1921 | 321,216 |
| `-O1 --embed-jit` | 1933 | 1930 | 1894 | 1885 | 589 | 1922 | 320,153 |

### 1. At `-O0` the RIR and AST layers are not merely idle, they are absent

RIR is offered **1932 bodies and records 0**. No `ast.*` counter increments at all — the AST
layer never reaches its verdict, so there is no arena, no replay and no faithfulness question.
`-O0` emit coverage is **exactly one layer: AOT**.

This is the single largest structural difference between the two levels, and it is a cliff
rather than a slope: the second level does not record *more*, it is the first to record
anything.

### 2. `--embed-jit` — not the `-O` level — is what turns the pipeline on at `-O0`

`-O0 --embed-jit` records **1929 of 1932** bodies and builds **1894** arenas, i.e. it reaches
the same coverage as `-O1` while the optimizer level is still zero. Read against row 1, the
gate on the whole RIR/AST pipeline at `-O0` is the *JIT request*, not the optimization request.

`-O0 --embed-jit` and `-O1 --embed-jit` are within 2 bodies of each other on every counter.

### 3. Everything recorded does not reach the AST verdict, and the gap is stable

`rir.rec` 1930 → `ast.body` 1894 loses **36 bodies** at `-O1`, and the same 1894 appears on all
three pipeline arms. `aot.fn` is 1922, so **28 bodies are emitted by AOT but never reach the
AST decision point**. Both gaps are reproducible across arms, which makes them structural
rather than incidental.

### 4. Replay reproduces the parser almost exactly, and `--embed-jit` at `-O0` is where it does not

| arm | parser bytes | replay bytes | delta |
| --- | ---: | ---: | ---: |
| `-O1` | 267,575 | 267,566 | **−9** |
| `-O1 --embed-jit` | 267,575 | 267,566 | **−9** |
| `-O0 --embed-jit` | 267,971 | 267,983 | **+12** |

At `-O1` the replay is 9 bytes shorter across ~268 KB. At `-O0 --embed-jit` it is 12 bytes
*longer*, and faithful drops 1885 → 1883. So the two bodies that lose faithfulness under
`-O0 --embed-jit` are the ones to look at if that arm is ever made to matter — the pipeline is
otherwise identical to `-O1`.

### 5. The finding that most contradicts the level's name: `-O1` usually emits the same bytes as `-O0`

Compiling each program at both levels and comparing objects:

    identical = 185      differ = 99      (of 284)

**Two thirds of programs emit byte-identical code at `-O1` and `-O0`**, despite `-O1` running
the entire pipeline row 1 shows is absent — 1930 recorded, 1894 arenas, 1885 faithful. Total
emitted size moves by **−1,075 bytes, −0.34%**.

So the pipeline being *active* and the pipeline *changing the output* are separated by a wide
margin here: 100% of bodies go through it, ~35% of programs come out different.

### 6. `--embed-jit` costs emitted size at both levels

`aot.bytes` rises +3,522 at `-O0` and +3,534 at `-O1` (≈ +1.1%) with 587–589 bodies baked —
the dispatch and blob material. Worth stating because the baking is otherwise invisible in
these counters: `jit.baked` counts acceptances, not the bytes they cost.

## What this inventory does not establish

- **Why** the 36 recorded-but-not-verdicted and 28 emitted-but-not-verdicted bodies drop out.
  The counters localise the gap; they do not name a reason.
- Whether the 99 programs that differ at `-O1` are *better*. Size moved −0.34% overall; that is
  not a performance claim.
- Anything about `-O2`+ or `-O13`. Only the two levels asked about were measured.
