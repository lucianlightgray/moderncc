# Upstream-bug investigation (companion to `TODO.md`)

The `TODO.md` checklist was the historical TinyCC upstream bug list. On
2026-06-27 every concretely-testable item was reproduced against the built
compiler (`build/tcc -B<builddir>`) and cross-checked against `gcc -std=gnu99`.
**Most no longer reproduce** — they were fixed somewhere on the way to 0.9.28rc,
and have been removed from `TODO.md`. This file keeps the record of what is
*still actionable*: finishing `_Complex` on every CPU target.

Re-run a native reproducer with:
`build/tcc -B<builddir> -run repro.c`   (compile-only: drop `-run`, add `-c`)

---

## §A. `_Complex` across all CPU targets — the active work

`_Complex T` is modelled as a cached, marked anonymous
`struct { T __real, __imag; }` (`SymAttr.is_complex`), so pass/return/copy/
sizeof/ABI come from the struct machinery. Implemented in `tccgen.c`:
`is_complex_type`, `complex_part`, `cplx_local/_push_part/_store_part/
_materialize`, `gen_complex_op` (`+ - * /`, `== !=`), `gen_complex_cast`, the
`i`/`j` imaginary suffix (`parse_number`), and a bundled `include/complex.h`.
All complex-specific code is gated on `is_complex`, so non-complex builds (the
x86_64 self-host) are untouched.

This was only ever *executed* on x86_64. The active work runs it on
i386/arm/arm64/riscv64 under qemu-user, regression-locks the passing parts, and
fixes the rest. Plan, run-target table, and TDD discipline live in `TODO.md`.

### A1. qemu-user freestanding harness (built + proven this session)
- `tcc -static -nostdlib`, own `_start`, exit code = result (0 pass / N fail).
  No target libc/sysroot needed.
- `cexit.h` per-arch `sys_exit(int)`; `cmem.h` freestanding `memmove/memcpy/
  memset` (+ `__aeabi_*` on arm) because every non-x86_64 target lowers the
  `_Complex` struct copy to a `memmove` call.
- Harness wrinkles discovered & solved: tcc's arm64 integrated assembler has no
  `svc` mnemonic and arm rejects pinning `r7`, so the syscall opcode is emitted
  as a raw `.int` word; tcc's `.word` is 2 bytes (not 4) — use `.int`. arm uses
  the OABI `swi #0x900001` form so the syscall number is in the instruction.

### A2. Results from running the harness (2026-06-27)
Value-based suite (`tests/complex-cross/complex_test.c`), after the R1 fix and
linking the per-target `libtcc1.a`:

| case | x86_64 | i386 | arm | arm64 | riscv64 |
|---|---|---|---|---|---|
| `float`/`double`/`long double` `+ - * /`, `== !=`, pass/return, mixed args, promotion, casts, `+=`/`*=` | ✅ | ✅ | ✅ | ✅ | ✅ |

**R1 — FIXED.** `long double _Complex` arithmetic returned NaN on x86_64
(reproducible natively, even `z + (1.0L+1.0Li)` with no call). Root cause:
`cplx_push_part` loaded each part with the hardcoded `gv(RC_FLOAT)`, forcing an
80-bit x87 `long double` into an SSE register (x86_64) — garbage. gv() only
auto-remaps `RC_FLOAT`→correct class for riscv64 (the "mega hack" at
`tccgen.c` ~1908), not x86_64. Fix: `gv(RC_TYPE(vtop->type.t))`, which returns
`RC_ST0` (x86_64 ldouble) / `RC_INT` (riscv64 soft quad) / `RC_FLOAT`. Self-host
185/185 preserved.

**R2 — RESOLVED (harness/link).** `long double _Complex` on arm64/riscv64 needs
128-bit soft-float quad helpers (`__addtf3`,`__subtf3`,`__multf3`,`__divtf3`,
`__netf2`,…). They are already defined in `lib/lib-arm64.c` / `lib/lib-riscv.c`,
so the freestanding harness just links `<tgt>-libtcc1.a`.

**R3 — FIXED: complex *return* ABI now matches gcc.** Found by the tcc↔gcc
conformance test (§A3). The struct model returned a complex in memory, but the
platform ABI returns a *small* complex in registers, so a tcc↔gcc call by value
crashed. Two cells diverged; both fixed:

| cell | gcc returns in | was | now |
|---|---|---|---|
| i386 `float _Complex` | `edx:eax` (real=eax, imag=edx) | memory (sret) | ✅ edx:eax |
| i386 `double`/`long double _Complex` | memory (`ret $4`) | memory | ✅ memory |
| x86_64 `float`/`double _Complex` | `xmm0`(:`xmm1`) | same (SSE) | ✅ SSE |
| x86_64 `long double _Complex` | `st0:st1` | memory (sret) | ✅ st0:st1 |

Confirmed by disassembly: `gcc -m32` emits `flds;…;movl %eax;movl %edx` for
`float _Complex` (registers), but `ret $4` memory return for `struct{float,
float}`; `gcc` x86_64 emits `fld…faddp…fxch;ret` (st0:st1) for `long double
_Complex`. So the design note "`_Complex T` has the same ABI as `struct{T,T}`"
is **false** for these two cells (it was only ever checked for cfloat/cdouble
*sizes*, not the small-complex register-return special cases).

Fixes (the design note "`_Complex T` has the same ABI as `struct{T,T}`" is
**false** for the two register-return cells — only ever checked for cfloat/
cdouble *sizes*):
- **i386** (`i386-gen.c`): `sysv_struct_ret_in_regs()` (shared by `gfunc_sret`
  and `gfunc_prolog`) returns an `is_complex` float in `edx:eax`; the prolog no
  longer reserves the hidden struct-return pointer for it — that reservation
  shifted register args by one slot and was the actual crash; the return-value
  marshalling was already correct.
- **x86_64** (`x86_64-gen.c`, SysV side only — PE long double is 64-bit):
  `x86_64_complex_ldouble()` gates it; `gfunc_sret` returns `-1` and a new
  `arch_transfer_ret_regs()` moves the pair between st0:st1 and memory with raw
  `fldt`/`fstpt` (store() for ldouble is deliberately non-popping); the prolog
  skips the hidden pointer. The two `arch_transfer_ret_regs` call sites in
  `tccgen.c` are enabled for x86_64 alongside riscv64.

Time-sink gotcha: `x86_64-gen.c` has **two** copies of each ABI function, under
`#ifdef TCC_TARGET_PE` vs `#else` (SysV). The SysV copies are live on Linux;
editing the PE copy compiles cleanly and does nothing. Verified: non-complex
`struct{long double,long double}` still returns in memory, plain `long double`
unaffected, self-host 185/185, conformance green for every i386+x86_64 cell.

### A3. Cross-ABI conformance (where an oracle exists)
Value-based asserts catch self-inconsistent codegen but not a self-consistent
deviation from the *platform* ABI. For i386 and x86_64 the host can build the
oracle: link a tcc-compiled caller against a `gcc -m32` / `gcc` callee that
takes/returns `_Complex` (the `tests/i386-fastcall-abi.sh` pattern). No cross
`gcc` for arm/arm64/riscv64 on this host, so those rely on value asserts +
documented-ABI inspection.

---

## §B. Open but deferred (design notes / gated subsystems)
- Portability assumptions (`int` 32-bit; `int` where target `size_t` fits; host
  fp for target fp) — architectural, not point fixes.
- Static-linking caveat (glibc needs `libc.so`) — upstream/glibc.
- Bound-checking edge cases — gated by `TCC_CONFIG_BCHECK`, hard to reproduce on
  a modern host; defer unless bcheck work is requested.
- libtcc reentrancy, leak-after-longjmp, interactive debugger, PowerPC /
  portable-bytecode backends — large separate projects.

---

## §F. TDD self-check of the §A plan (done before coding)
Checked the plan in `TODO.md` against test-driven-development principles:

1. **Red before green — satisfied.** R1 and R2 are concrete failing tests that
   exist *now*, before any fix. Step 1 also writes the currently-green
   float/double tests first, so the regression net predates codegen edits.
2. **One behavior per test, small steps — satisfied.** Each assertion returns a
   distinct exit code identifying which property failed; fixes proceed
   x86_64 → i386 → arm/arm64/riscv64, smallest change first.
3. **Fast feedback — satisfied.** R1 reproduces natively (no qemu) for the
   tightest loop; qemu targets run only after the native loop is green.
4. **Trustworthy oracle — satisfied.** Asserts are value-based (math is known),
   so they don't depend on tcc being right elsewhere; gcc is used as an
   independent ABI oracle exactly where the host can produce it (i386/x86_64).
5. **No regressions — satisfied.** The full 5-target suite *and* the x86_64
   self-host suite (185/185) run after every change; all complex code stays
   `is_complex`-gated so non-complex paths can't regress.
6. **Refactor only on green — satisfied.** Harness/codegen cleanup is step-last
   and only with everything green.

Risk noted: value asserts can't see a self-consistent platform-ABI deviation on
arm/arm64/riscv64 (no cross-gcc oracle). Mitigation = §A3 inspection against the
documented per-arch ABI for the return-by-value and arg-interleaving cases.
