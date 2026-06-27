# Upstream-bug investigation (companion to `TODO`)

The `TODO` file is the historical TinyCC upstream bug list. On 2026-06-27 every
concretely-testable item was reproduced against the built compiler
(`build/tcc -Iinclude -Bbuild`) and cross-checked against `gcc -std=gnu99`.
Most items **no longer reproduce** — they were fixed somewhere on the way to
0.9.28rc. This file records the verification, isolates the items that *still*
reproduce, and scaffolds the work needed to close them.

Re-run any reproducer with:
`build/tcc -Iinclude -Bbuild -run repro.c`   (compile-only: drop `-run`, add `-c`)

---

## A. Verified ALREADY FIXED (reproducer passes, matches gcc)

| TODO line | Reproducer result |
|---|---|
| invalid cast in comparison `if (v == (int8_t)v)` | `v=5`→eq, `v=300`→ne — correct (matches gcc) |
| fix multiple unions init | `union U a[2]={{.i=1},{.c='A'}}` → `1 65` (matches gcc) |
| struct/union/enum in nested scopes | redeclaring `struct S` in two sibling blocks works (matches gcc) |
| `__STDC_IEC_559__` static `0.0/0.0` init | `static float x=0.0f/0.0f` → NaN at compile time (matches gcc) |
| `? x, y : z` in unsized var init | `int a[]={c?1,2:3}` → `2` — comma parsed as operator (matches gcc) |
| typeof with arrays | `typeof(a) b` preserves `int[10]`, `sizeof` 40/40 (matches gcc -std=gnu99) |
| functions→function-pointers in params | `int apply(func f,…)` and `int g(int f(int),…)` both decay & call fine |
| `__builtin_expect()` (listed "missing") | works, incl. `likely()`/`!!(x)` kernel idiom |
| `atexit` (listed "missing") | registered handler runs after `main` |
| varargs / `<stdarg.h>` | `va_start/va_arg/va_end` sum works |
| postfix compound literals (`20010124-1.c`) | `((struct S){.a=3,.b=4}).b` → `4` |
| section/`__attribute__((aligned(64)))` | address is 64-aligned at runtime |

These should be moved to the "Fixed" bucket in `TODO` (done — see the checklist),
not re-investigated.

---

## B. Verified STILL OPEN (reproduces today)

### B1. `__attribute__((transparent_union))` — ✅ DONE (2026-06-27)
Implemented: `SymAttr.transp_union` bit (tcc.h), `TOK_TRANSPARENT_UNION1/2`
(tcctok.h), parsed in `parse_attribute`, stored on the union Sym in
`struct_layout` (union-only, warns on non-union), and consumed in
`gfunc_param_typed` via the new `transparent_union_member()` helper: a function
argument assignable to any union member is passed as that member's scalar type
(same calling convention as the union for pointer members). 3-way regression
test `tests/tests2/190_transparent_union.c`. Output matches gcc & clang.
Scope: function arguments (the socket.h need). Direct assignment / return of a
transparent union are not specially handled (gcc restricts the semantics to
parameters anyway).

Original reproducer (now passes):
```c
typedef union { int *ip; long *lp; }
  __attribute__((__transparent_union__)) tu;
void f(tu u){ /* ... */ }
int main(){ int x=7; f(&x); }   /* passing int* where union expected */
```
→ `error: cannot convert 'int *' to 'union <anonymous>'`

Investigation start points:
- attribute parsing: `tccgen.c` `parse_attribute()` — add a `transparent_union`
  flag bit on the union's `AttributeDef`/`Sym`.
- assignment/argument compatibility: `gen_assign_cast()` / `type_to_str` path in
  `tccgen.c`. When the target is a transparent union, an rvalue assignable to
  *any* member must be accepted and wrapped as that member.
- Scope check: gcc only honors it for function parameters/returns; matching that
  exactly avoids opening a hole in ordinary union assignment.

### B2. `_Complex` / C99 complex types — LARGE (concrete blockers identified)
```c
#include <complex.h>
double complex z = 1.0 + 2.0*I;   /* error: _Complex is not yet supported */
```
Rejected in `parse_btype` (tccgen.c, `case TOK_COMPLEX`). Implementing it is a
large feature, and the type-system entry cost alone is real:
- `VT_BTYPE` is a 4-bit field (`0x000f`, tcc.h:1058). Values 0-11/13/14 are
  taken; only **12 and 15 are free** — but `_Complex` needs THREE base types
  (complex float/double/long double). Two free slots < three.
- There is **no spare modifier bit** in the type range (0x0010-0x0800 are all in
  use: UNSIGNED/DEFSIGN/ARRAY/BITFIELD/CONSTANT/VOLATILE/VLA/LONG) to tag
  "complex" onto the existing VT_FLOAT/VT_DOUBLE/VT_LDOUBLE btypes.
So representation requires repurposing scarce bits (e.g. a QFLOAT/QLONG-style
ABI-only pair, or dropping complex-long-double), *before* the pervasive work:
every `gen_op` case (+ - * / with the complex multiply/divide expansions),
`type_size`/alignment, two-component load/store, real<->complex and
complex<->complex casts, `__real__`/`__imag__` operators, the x86-64 SysV
pass/return ABI (e.g. `_Complex double` returned in xmm0:xmm1), and `<complex.h>`
+ the `I` macro. No safe partial gate: once `parse_btype` accepts `_Complex`,
every downstream path must handle it or miscompile. Out of scope for an
incremental fix; keep as a tracked, scoped feature.

---

## C. Open but ARCH-SPECIFIC (not reproducible on x86_64 host)

On this host `gcc -m32` works (32-bit multilib present), so i386 code can be
built AND run — no emulator needed. Build the i386 backend with
`cmake -S . -B build-i386 -DTCC_ENABLE_CROSS=ON && cmake --build build-i386
--target i386-tcc`.

- **i386 fastcall mostly wrong** — ✅ FIXED. The register-arg calling convention
  in `i386-gen.c` (`gfunc_call` + `gfunc_prolog`) blindly put the first two stack
  dwords in ecx/edx (the code even said `XXX: incorrect for struct/floats`). Now a
  shared classifier `fastcall_arg_class()` implements gcc/MSVC rules: only a
  leading run of integral/pointer args <=4 bytes goes in ecx/edx; `long long`
  spills to the stack and exhausts the remaining register budget; float/double/
  struct stay on the stack without consuming a slot. The one corner that the
  push-then-pop call sequence can't express (a float/struct arg *before* an
  integer register arg, where gcc still uses a register) is rejected with a clear
  error instead of corrupting the stack. Verified 3-way (tcc->gcc, gcc->tcc,
  tcc->tcc) by `tests/i386-fastcall-abi.sh`; cdecl/normal calls unaffected; the
  x86_64 self-host build is untouched (i386-gen.c isn't part of it).
- **FPU st(0) left unclean** — STILL OPEN. x87 stack discipline in i386 codegen;
  breaks interop with optimized gcc/msvc code that assumes a clean x87 stack.
  Now reproducible here via `gcc -m32` + the i386 cross tcc, but a separate fix.

## D. NOT actionable as bug-fixes (design notes / deliberate non-goals)

- Portability assumptions (`int` is 32-bit / `sizeof(int)==4`; `int` used where
  target `size_t` fits; host fp arithmetic for target fp) — architectural, not a
  point fix.
- Static-linking caveats (glibc needs `libc.so`) — upstream/glibc, not tcc.
- Bound-checking edge cases (RedHat 7.3 exit, setjmp, `&` on locals, float/
  longlong/struct copy) — old, gated by `TCC_CONFIG_BCHECK`; hard to even
  reproduce on a modern host; defer unless bcheck work is specifically requested.
- **disable-asm option** — ✅ DONE. New `TCC_CONFIG_ASM` (default ON). When OFF,
  config.h emits `#define CONFIG_TCC_ASM 0`, reconciled in tcc.h to
  `TCC_DISABLE_ASM`, which suppresses the per-arch `#define CONFIG_TCC_ASM` and
  guards the `i386-asm.c` body. The integrated-assembler entry points (inline asm
  `asm_instr`, global asm `asm_global_instr`, `.s`/`.S` via `tcc_assemble`) are
  `#ifdef CONFIG_TCC_ASM`-guarded to a clear error. IMPORTANT: asm *labels*
  (`decl __asm__("name")` symbol renaming) are NOT disabled — they only parse a
  string (`parse_asm_str`, in tccgen.c) and are required by the predefined
  `__builtin_memcpy`/`malloc`/`alloca` aliases in tccdefs.h. CMake forces
  `TCC_LIBTCC1_USEGCC` when asm is off (a no-asm tcc can't assemble `lib/*.S`) and
  drops the 6 asm-dependent tests. Verified: default 179/179, asm-off 173/173.
  (The sibling **disable-bcheck** already existed: `TCC_CONFIG_BCHECK`.)
- Reentrancy of libtcc, leak-after-longjmp, interactive debugger, PowerPC/
  portable-bytecode backends — large, separate projects, not point fixes.

## E. Ambiguous wording — clarify before touching

- **"fix static functions declared inside block"** — the obvious reading
  (`static int f(void);` at block scope) is a *constraint violation* that tcc
  correctly rejects, exactly like gcc. Real intent unknown (possibly GNU nested
  functions, or a forward-declared file-scope static used early). Needs an
  upstream-history dig before any change.
- **"fix function pointer type display"** — assigning to a `int(*)(int,double)`
  now yields a sensible diagnostic. Likely already adequate; need the original
  bad-output example to know what "display" was wrong.

---

## Status of next actions
1. **transparent_union (B1)** — ✅ DONE (implemented + 3-way test 190).
2. **E items** — ✅ RESOLVED: fn-pointer type display already renders correctly;
   preprocessor redefinition matches gcc; `void(__attribute__()* )()` parses;
   "static fn in block" standard form correctly rejected (GNU nested functions
   are a separate large extension, deliberately unsupported).
3. **B2 (_Complex)** — OPEN, LARGE. Genuinely out of scope for an incremental
   pass: needs a complex btype/representation, codegen for every operator + cast
   + `__real__`/`__imag__`, ABI classification in ALL backends (x86_64/i386/arm/
   arm64/riscv64), and `<complex.h>`. The current clean "not yet supported" error
   is the correct conservative state — a partial impl that miscompiles complex
   arithmetic in a self-hosting compiler would be worse. Track as a feature.
4. **C (i386 fastcall / x87 st(0))** — OPEN, arch-specific. Not reproducible on
   an x86_64 host; needs an i386 build + emulator + a gcc/msvc-interop harness to
   even observe the bug. Track with the right target set up.
5. **D** — closed (design notes / deliberate non-goals).

## Genuinely out of scope for a single session (large/new subsystems)
_Complex (B2), i386 fastcall + x87 cleanliness (C), PowerPC backend, portable
bytecode interpreter, interactive debugger, full libtcc reentrancy, C++ support,
VLA-vs-signals redesign. These are multi-day–multi-week features or need
hardware/emulator harnesses; they are tracked, not implementable ad hoc.
