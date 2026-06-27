# TODO

The historical TinyCC upstream bug list was triaged on 2026-06-27 against the
built compiler and `gcc -std=gnu99`: every concretely-testable item either
already passes (removed from this file) or is a deliberate non-goal (listed at
the bottom). What remains as *active work* is finishing `_Complex` across every
supported CPU target, driven by a qemu-user TDD harness. See
`TODO-BUGS-INVESTIGATION.md` for the full triage record and reproducers.

---

## ACTIVE: finish `_Complex` on all supported CPU targets (TDD via qemu-user)

`_Complex` was implemented as a marked 2-member struct `{ T __real, __imag; }`
(`is_complex` on the struct Sym) so pass/return/copy/sizeof/ABI come from the
struct machinery. That was only ever *executed* on the x86_64 self-host. This
work-item runs the same code on i386/arm/arm64/riscv64 under qemu-user, locks
the passing behaviour into a regression suite, and fixes what's broken.

### Supported run targets (Linux ELF)
| target  | runner                | exit stub (freestanding harness)        |
|---------|-----------------------|-----------------------------------------|
| x86_64  | native                | `syscall` (rax=60)                       |
| i386    | native (multilib)     | `int $0x80` (eax=1)                      |
| arm     | `qemu-arm`            | OABI `swi #0x900001`, `.int` word, r0    |
| arm64   | `qemu-aarch64`        | `svc #0` via `.int 0xD4000001`, x0/x8    |
| riscv64 | `qemu-riscv64`        | `ecall`, a0/a7                           |

(`*-win32`, `*-osx`, `c67` are out of scope: no qemu-user path / DSP.)

### Harness (built and proven this session — see scratchpad)
- Tests are **freestanding**: `tcc -static -nostdlib`, own `_start`, and the
  process **exit code carries the result** (`0` = all pass, `N` = assertion N
  failed). No target libc / sysroot required.
- `cexit.h` — per-arch `sys_exit(int)` (table above), register-pinned so a
  *runtime-computed* code reaches the kernel. arm64/arm needed raw `.int`
  opcodes because tcc's integrated assembler lacks `svc` and rejects pinning
  `r7`; tcc's `.word` is 2 bytes, so opcodes use `.int` (4 bytes).
- `cmem.h` — freestanding `memmove/memcpy/memset` (+ `__aeabi_*` aliases on
  arm): every non-x86_64 target lowers the `_Complex` struct copy to a
  `memmove` call, which `-nostdlib` can't otherwise resolve.
- Assertions are **value-based** → ground-truth-free: the math is known
  (`(1+2i)(3+4i) = -5+10i`), so a wrong ABI shows up as a wrong number. Where a
  platform-ABI *conformance* check is wanted and ground truth exists (i386 +
  x86_64), additionally link a tcc-compiled caller against a `gcc`-compiled
  callee (the `tests/i386-fastcall-abi.sh` pattern).

### Known state from running the harness (2026-06-27)
- ✅ GREEN on all 5 targets (value-based qemu suite, `tests/complex-cross/`):
  `float`/`double`/`long double _Complex` `+ - * /`, `==`/`!=`, pass-by-value,
  return-by-value, mixed `int`+complex args, real↔complex promotion, casts,
  compound assignment. Pure-tcc `_Complex` is correct and self-consistent
  everywhere.
- ✅ **R1 FIXED** — `long double _Complex` arithmetic returned **NaN** on x86_64
  because `cplx_push_part` loaded a part with the hardcoded `gv(RC_FLOAT)` (SSE)
  instead of the base type's class. Fixed to `gv(RC_TYPE(vtop->type.t))`, which
  picks `RC_ST0` (x86_64 x87) / `RC_INT` (riscv64 soft quad) / `RC_FLOAT`.
  Self-host stayed 185/185.
- ✅ **R2 RESOLVED** — `long double _Complex` on arm64/riscv64 needs libtcc1's
  soft-float quad helpers (`__addtf3`,`__netf2`,…); they already exist in
  `lib/lib-arm64.c` / `lib/lib-riscv.c`, so the harness just links the per-target
  `<tgt>-libtcc1.a`. Green on all 5 targets.
- ✅ **R3 FIXED** — tcc's complex **return** ABI deviated from gcc in two cells
  (found by the tcc↔gcc conformance test, `tests/complex-cross/abi/`): the
  platform returns a small complex in registers, but tcc's struct model returned
  it in memory, crashing at a tcc↔gcc boundary. Both fixed and confirmed against
  gcc:
  - **i386 `float _Complex`** → `edx:eax` (real=eax, imag=edx). `i386-gen.c`
    `gfunc_sret` returns it as a 2-integer-register value, and the prolog no
    longer reserves the hidden struct-return pointer for it
    (`sysv_struct_ret_in_regs`).
  - **x86_64 `long double _Complex`** → `st0:st1`. There is no x87 register
    *pair* class, so `gfunc_sret` returns `-1` and a new `arch_transfer_ret_regs`
    moves the pair to/from memory with raw `fldt`/`fstpt`; the prolog skips the
    hidden pointer (`x86_64_complex_ldouble`). Both PE (64-bit long double) and
    non-complex `struct{long double,long double}` (still memory) are untouched.
  arm/arm64/riscv64 have no host cross-gcc oracle but class a complex like the
  struct (HFA / by-reference) and pass the value suite, so are believed
  conformant. The conformance test runs all i386+x86_64 cells as required PASS.

### Plan (Red → Green → Refactor; full suite after every step)
1. ✅ **DONE — harness + GREEN suite committed** (`tests/complex-cross/`:
   `harness.h`, `complex_test.c`, `run-complex-cross.sh`). Covers float/double/
   long double arithmetic, compare, pass/return, mixed args, promotion, casts,
   compound assignment. All 5 targets PASS. `is_complex` gate intact; self-host
   185/185.
2. ✅ **DONE — R1 fixed** (`gv(RC_TYPE(...))` in `cplx_push_part`).
3. ✅ **DONE — R2 resolved** (link `<tgt>-libtcc1.a`; helpers already present).
4. ✅ **DONE — ABI-corner coverage** folded into `complex_test.c` (return-by-
   value, `int,complex,double` interleaving, casts, `+=`/`*=`).
5. ✅ **DONE — cross-ABI conformance harness** (`tests/complex-cross/abi/` +
   `run-complex-abi.sh`): gcc oracle for i386 + x86_64. float/double conformance
   PASSES; it is what surfaced **R3** (the two long-tail return cells), now run
   as `XFAIL` until R3 is fixed.
6. ✅ **DONE — R3 fixed** on both backends (i386 `edx:eax`, x86_64 `st0:st1`).
   All i386+x86_64 conformance cells are now required PASS; self-host 185/185.
7. Keep `tests/tests2/196_complex.c` (native, printf) as the human-readable
   companion to the freestanding suite.

**Status: `_Complex` is complete on all supported CPU targets.** Value suite
green on all 5 (x86_64/i386/arm/arm64/riscv64); tcc↔gcc ABI conformance green on
both targets with a gcc oracle (i386/x86_64), every base type. No open items.

### TDD discipline (verified against the plan before coding — see investigation §F)
- A failing test exists *first* for every behavior changed (R1/R2 already red).
- Smallest change to green; never edit codegen with the suite red on a target
  that was green.
- After each change: run all 5 targets **and** the x86_64 self-host suite.
- Prefer value-based asserts (no oracle needed); use gcc only as the ABI oracle
  where the host can produce it (i386/x86_64).
- Refactor (harness/codegen cleanup) only with everything green.

---

## Deliberate non-goals / design notes (not bugs, won't fix here)
- Portability assumptions: `int` is 32-bit / `sizeof(int)==4`; `int` used where
  target `size_t` fits; host fp arithmetic used for target fp when cross-
  compiling. Architectural, not point fixes.
- `-static` caveat: glibc still needs `libc.so` even static (musl is fine).
  Upstream/glibc, not tcc.
- Bound-checking edge cases (RedHat 7.3 exit, setjmp, `&` on locals,
  float/longlong/struct copy) — gated by `TCC_CONFIG_BCHECK`, hard to even
  reproduce on a modern host; defer unless bcheck work is requested.
- Large separate projects: full libtcc reentrancy, leak-after-longjmp audit,
  interactive debugger, PowerPC / portable-bytecode backends, C++ support,
  VLA-vs-signals redesign.
- Optimization wishlist (anon-symbol handling, parse/alloc speed, VT_LOCAL+const,
  better local-variable handling) — not bugs.
- "static functions declared inside block": the standard reading is a
  constraint violation tcc correctly rejects (like gcc); GNU nested functions
  are a separate large extension, deliberately unsupported.
