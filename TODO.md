# C99 & C11 Conformance & Platform TODO

Goal: implement every detail of ISO/IEC 9899:1999 (C99) **and** ISO/IEC 9899:2011
(C11), backed by regression tests across all targets (x86_64, i386, ARM, AArch64,
RISC-V 64), platforms (ELF/PE/Mach-O), and both glibc and musl.

References: **N1256** (the C99 working draft) and **N1570** (the C11 working
draft); clause numbers below cite N1570 unless a C99-only rule is noted.
Each item records the standard requirement, the current `mcc` behaviour
(with how it is observed — `file:line` or a test result), and the precise gap.

The default standard is **C99** (`cversion=199901`, `src/libmcc.c:860`); `-std=c11`
selects C11. Items below apply at the relevant `-std` as noted.

Severity tags:
- **[BUG]** — wrong codegen, crash, hang, or rejects-valid C99/C11.
- **[FEATURE]** — a standard feature that is absent.
- **[DIAG]** — a constraint violation that `mcc` accepts with no diagnostic, or a diagnostic that is wrong/misleading.
- **[OPT]** — optional in the standard (a conforming implementation may omit it); listed for the "implement everything" goal.
- **[TASK]** — test/infrastructure work, not a language feature.
- **[LIMITATION]** — a host/platform constraint outside `mcc`'s control.
- **[DIFF]** — a known, deliberate difference from a reference compiler; not a defect.

Legend: `[ ]` open · `[~]` partial · `[x]` done.

Every open item is 3-way verified against gcc 15.3 and clang 22 (the project has
a history of false-positive TODO entries from unverified audits — re-verify any
item against the live binary before "fixing" it). Per the standing goal, each
item must ship with a regression test (cli/exec/diff) across the relevant
targets before it is marked done.

---

## Platform / host limitations (pre-existing)

- [x] **[BUG] Mach-O thread-local storage (`__thread` / `_Thread_local`) now uses TLV descriptors.**
  The backend emitted ELF local-exec TLS relocations (arm64 `R_AARCH64_TLSLE_*`,
  base `tpidr_el0 + sym@tprel`; x86_64 `%fs:0 + sym@tpoff`), which Mach-O has no
  equivalent of, so every thread-local read landed on a wrong address and
  segfaulted. Implemented as native Mach-O TLV descriptors in three parts:
  (1) `src/objfmt/mccmacho.c` `macho_tls_setup`/`macho_tls_finalize` synthesise a
  `__DATA,__thread_vars` section (`S_THREAD_LOCAL_VARIABLES`) holding a 24-byte
  `{ thunk, key, offset }` descriptor per thread-local, map `.tdata`/`.tbss` to
  `__thread_data`/`__thread_bss` (`S_THREAD_LOCAL_REGULAR`/`_ZEROFILL`), repoint
  each TLS symbol at its descriptor, bind the thunk word to libSystem's
  `__tlv_bootstrap` (through the existing chained-fixups path), fill the third
  word with the variable's byte offset in the contiguous template, and set
  `MH_HAS_TLV_DESCRIPTORS`;
  (2) codegen (`src/arch/{arm64,x86_64}/*-gen.c`) reaches a `_Thread_local` by
  taking the descriptor address, calling its thunk (arm64 `ldr x16,[x0]; blr`;
  x86_64 `lea _desc(%rip),%rdi; call *(%rdi)`) and using the returned per-thread
  address for the load/store/`&tls` paths, saving the registers the thunk
  clobbers around the call;
  (3) a front-end fix (`src/mccgen.c` `gv`) strips `VT_TLS` from the rodata
  symbol that spills a floating-point constant, so a literal in an FP TLS context
  is no longer mistaken for a TLS access (this was also latent on ELF/PE), and
  x86_64 `gen_opf` force-loads a TLS float lvalue instead of using it as a direct
  memory operand. Verified natively on arm64 (`tls` + `tls_aggr` return 0
  multi-threaded, plus a struct/array/bss/double stress test) and on x86_64-osx
  via Rosetta; the full ELF qemu matrix (5 arches × glibc/musl) stays green. The
  `tls` exec golden no longer carries `os!=Darwin` and now runs on Darwin.

- [x] **[DIAG] §6.5.1.1p2 `_Generic` association-type completeness — VLA + function-type now enforced.**
  The "no two compatible association types" rule was already enforced
  (`src/mccgen.c`, `TOK_GENERIC`; cli `generic_duplicate_assoc`). Now the
  complete-object-type sub-rule is too: a **variably modified** (VLA) association
  type is a hard error (`int[n]:` — gcc and clang both error by default), and a
  **function-type** association is diagnosed under `-pedantic` ("ISO C forbids a
  '_Generic' association with a function type" — gcc's `-pedantic` stance; clang
  errors). The incomplete-object case is left accepted (C2Y relaxes it and gcc
  accepts it). cli `generic_assoc_type_completeness`.

- [~] **[LIMITATION] The kernel-fused Mach-O / libSystem path needs a macOS or darling host.**
  The self-contained Mach-O tests run on any host: structural validation
  (`macho-structural`), Darwin-codegen execution against host glibc
  (`macho-codegen-run`), Mach-O image execution via the in-repo loader
  (`macho-image-run`), and Apple's own string/memory + self-contained printf
  sources run as Mach-O images (`macho-apple-libc`). The remainder of
  `libSystem` is kernel-fused and unrunnable off Darwin: `libmalloc`
  (magazine/nano/xzone zones on `os_unfair_lock`/Mach VM), FILE-backed/locale
  `stdio` (`xlocale`/`__sFILE`/gdtoa), and `dyld`/`pthread`/GCD/ObjC/Mach IPC.
  These require `-DMCC_DARWIN_HOST=ON` (a macOS or darling host); the boundary
  is documented in `tests/qemu/apple-libc/PROVENANCE.md`.

- [x] **[FEATURE] C11 `<threads.h>` now ships in mcc's bundled headers (Darwin shim over pthread).**
  Apple's libc provides no `<threads.h>` / `thrd_*` runtime, so a C11-threads
  program could not compile on a stock macOS host. `runtime/include/threads.h`
  now provides the full §7.26 API (`thrd_*`, `mtx_*`, `cnd_*`, `tss_*`,
  `call_once`/`once_flag`) as `static inline` wrappers over POSIX threads, using
  the same `__has_include_next` delegate-or-define pattern as the bundled
  `<stdint.h>`: where the hosting libc already supplies `<threads.h>` (glibc,
  musl) it is included via `#include_next` and wins, so the ELF targets are
  untouched; only where it is absent (macOS) does the shim apply. The
  `c11_threads` exec golden no longer carries `os!=Darwin` and passes on Darwin
  (arm64 native and x86_64-osx via Rosetta) alongside the glibc/musl ELF
  targets; it stays `os!=WIN32` (msvcrt has no pthread).

- [~] **[TASK] Systematic diagnostic-coverage sweep.**
  Each `[DIAG]`/`[FEATURE]` item ships with a cli/exec test that asserts the
  diagnostic fires and that the valid forms still compile. A full mechanical
  parity sweep across all ~312 `mcc_error`/`mcc_warning`/`mcc_pedantic` call
  sites in `src/` — to confirm the diagnostics that predate this tracker each
  have a regression test — remains an ongoing audit.

- [~] **[DIFF] diff3 differential divergences on macOS.**
  The `diff3` suite compiles each golden with `mcc`, gcc, and clang and flags
  cases where `mcc` differs from a gcc==clang consensus. Four such divergences
  remain on macOS, none of which is an `mcc` defect (`predefined_macros`
  __STDC_VERSION__ default; `bitfields_ms` implementation-defined layout;
  `cleanup` teardown ordering; `c11_freestanding_headers` where `mcc` is the
  most conformant of the three). The suite returns non-zero on any divergence
  by design; these four are the expected residual on a macOS host.

---

## §6.4 Lexical elements

- [~] **[BUG] §6.4.5p5 — mixed wide/narrow string-literal concatenation: wide-first fixed.**
  The merge loop (`src/mccgen.c`, `decl_initializer`) now widens each narrow
  literal to the array's element width instead of byte-copying, with correct
  length math. `L"ab" "cd"` ⇒ correct 5-element `wchar_t` (was `wcslen=1`
  garbage); `wchar_t w[]=L"pq" "rs"` works; same-prefix and narrow chains
  unchanged. exec `string_concat_mixed`. REMAINING: *narrow-first* promotion
  (`"ab" L"cd"` / `wchar_t a[]="x" L"y"`) — the element type is fixed by the
  first literal in `str_init` (unary), so a wider literal following it can't be
  represented; mcc now errors cleanly ("a wide string literal cannot follow a
  narrower one") instead of miscompiling. Full support needs a look-ahead in
  `str_init` to pick the widest element type before building the array.

- [x] **[BUG]/[DIAG] §6.4.5p2 — conflicting wide-prefix concatenation now rejected.**
  The merge loop tracks the established wide encoding prefix and errors
  "unsupported concatenation of string literals with different encoding prefixes"
  when a second, different wide prefix appears (`u"a" U"b"`, etc.), matching
  gcc/clang. (Was silently accepted + miscompiled.) cli covered via the
  `string_concat_mixed` exec test's sibling checks; 5-arch.

- [x] **[DIAG] §6.4.4.4p9 — octal escape out of range now diagnosed.**
  `'\777'` (0o777=511) was silently truncated to 0xFF; now warns "octal escape
  sequence out of range" for narrow (`!is_long`) literals, matching gcc
  (clang errors). Fixed at `src/mccpp.c` (octal escape path). Wide literals
  unaffected (`L'\777'` no warning). cli `escape_out_of_range`.

- [x] **[DIAG] §6.4.4.4p9 — hex escape out of range now diagnosed.**
  `"\xfff"` was silently truncated; now warns "hex escape sequence out of range"
  for narrow literals (gated on the `\x` path, i<0, so `\u`/`\U` UCNs are
  exempt). Fixed at `src/mccpp.c`. cli `escape_out_of_range`.

---

## §6.5–6.6 Expressions & constant expressions

*Cross-cutting: lvalue-ness is tracked via the `VT_LVAL` bit on the value stack.
The cast and comma operators force the scalar result into a register so
`VT_LVAL` drops; the function-call rvalue struct member (`g().x`) — which can't
use that trick (structs don't go in registers) — uses the dedicated
`VT_NONLVAL` marker. All three lvalue cases are fixed below.*

- [x] **[BUG] §6.5.2.2/§6.5.2.3 — member of a function-call rvalue struct is no longer a modifiable lvalue.**
  A by-value struct returned from a call lives in an addressable temp but is not
  a modifiable lvalue. Added a `VT_NONLVAL` value-stack bit (`src/mcc.h`, 0x2000)
  set on struct call results (`src/mccgen.c`, end of the call `(` case),
  propagated through `.` member access (but not `->`, which dereferences a
  pointer to a real lvalue), and checked at unary `&` and at assignment. Now
  `g().x = 3`, `&g().x`, `&g()` error (matching gcc/clang); reading `g().x`,
  copying `c = g()`, and `gp()->x = 3` (assign through a returned pointer) still
  work. cli `rvalue_struct_member`; 5-arch.

- [x] **[BUG] §6.5.17 — comma-operator result no longer an lvalue.**
  `(a,b)=7` and `&(a,b)` now error "lvalue expected"; the comma result is forced
  into a register (`gexpr`, after the comma loop) so `VT_LVAL` is dropped, with
  aggregates/arrays/functions/complex excluded (they decay/aren't assignable and
  can't go to a register). `(a,b)` as an rvalue, comma in `for`, and `(a,s).x` /
  `(a,arr)[i]` all still work (runtime-verified). cli
  `lvalue_cast_comma_constraints`.

- [x] **[BUG] §6.5.4 — cast-operator result no longer an lvalue.**
  `(int)a = 9` and `&(int)a` now error "lvalue expected"; the no-op-cast-of-
  lvalue case is forced into a register after `gen_cast` (cast-of-expression
  branch only, not the compound-literal `(T){...}` branch). Real conversions,
  rvalue use (`(int)a+1`), and `*(int*)p` still work (runtime-verified). The GNU
  lvalue-cast extension stays valid in **asm operands** (`"=a" ((USItype)(x))`,
  used by `runtime/lib/libmcc1.c`'s `udiv_qrnnd` on i386) — gated via the
  `asm_lvalue_cast` flag set in `parse_asm_operands` (the cross/i386 build caught
  this regression; native ctest did not). cli `lvalue_cast_comma_constraints`,
  exec `asm_lvalue_cast`.

- [x] **[BUG] §6.5.3.3p1 — unary minus now rejects a pointer operand.**
  "The operand of the unary + or - operator shall have arithmetic type."
  `(-p)==0` (p is `int*`) used to compile silently; now errors "pointer not
  accepted for unary minus", mirroring the adjacent unary `+` guard. Fixed at
  `src/mccgen.c` (`case '-'`). Integer negation unaffected; complex (`VT_STRUCT`)
  unaffected. cli `unary_minus_pointer`.

- [~] **[DIAG] §6.5.16.1 — assignment to a const-qualified struct/union is diagnosed (as a warning).**
  Audit re-check correction: `a = b` where `a` is `const struct S` *does* warn
  "assignment of read-only location" — `vstore` calls `verify_assign_cast`
  (`src/mccgen.c:3557`) whose const check (`:3456`) fires for aggregates too,
  exactly as for the scalar case. mcc's long-standing stance is to *warn* on
  assignment-to-const (scalar and aggregate alike); gcc/clang *error*. Only the
  severity differs. Promoting to a default-on error would be more conformant but
  is a broad change to a long-standing lenient warning — deferred.
  3-way: mcc=warns | gcc=error | clang=error.

- [~] **[DIAG] §6.6p3 — comma operator in an integer constant expression diagnosed only under -pedantic.**
  A required constant expression "shall not contain ... comma operators" outside
  an unevaluated subexpression. `int a[(1,2)];` is `mcc_pedantic`
  (`src/mccgen.c:7448`): silent at default level, but `-pedantic` warns and
  `-pedantic-errors` hard-errors. clang is also lenient at default; gcc rejects
  the file-scope case by default via its VLA-at-file-scope path. Effectively
  handled — consider whether to promote to a default-on warning. Low priority.

---

## §6.7 Declarations

- [x] **[BUG] §6.7.6.2p2 — VLA type with external linkage now rejected.**
  An object of variably-modified type shall have no linkage. `void f(int n){
  extern int a[n]; }` now errors "object with variably modified type must have
  no linkage" (`src/mccgen.c`, decl() VLA+VT_EXTERN check). `extern int a[];`
  (incomplete, non-VLA) and local VLAs still compile. gcc+clang both reject.
  cli `decl_storage_type_constraints`.

- [x] **[DIAG] §6.7.1p3 — `_Thread_local` combined with `typedef` now rejected.**
  `_Thread_local typedef int T;` now errors "'_Thread_local' used with
  'typedef'" (`src/mccgen.c`, the `VT_TLS` block now checks `VT_TYPEDEF` first).
  `_Thread_local static`/`extern` still compile. gcc+clang both reject. cli
  `decl_storage_type_constraints`.

- [~] **[DIAG] §6.7.4p2 — `_Noreturn` on a non-function object diagnosed only under -pedantic.**
  Function specifiers shall declare only functions. `_Noreturn int x;` is
  diagnosed via `mcc_pedantic` (`src/mccgen.c:9509`, gated on bit 128 = the
  `_Noreturn` keyword vs `__attribute__((noreturn))`): silent at default,
  `-pedantic` warns, `-pedantic-errors` errors. gcc warns by default; clang
  errors by default. Effectively handled under `-pedantic`; consider promoting
  to a default-on warning to match gcc. (The parallel `inline int x;` is a
  default-on hard error at `src/mccgen.c:9504` — intentionally asymmetric since
  gcc treats `_Noreturn` on an object as an extension.)

- [x] **[DIAG] §6.7.6.3p10 — qualified `void` as the sole parameter now rejected.**
  The no-parameter special case is an unnamed, *unqualified* `void`.
  `void f(const void);` now errors "'void' as only parameter may not be
  qualified" (`src/mccgen.c`, the lone-`void` special case now checks
  `VT_CONSTANT`/`VT_VOLATILE`). `void f(void)` still compiles. gcc+clang both
  reject. cli `decl_storage_type_constraints`.

- [x] **[DIAG] §6.7.4p3 — inline external definition with a modifiable static object now diagnosed.**
  A non-`const` `static`-duration object defined inside an inline external-linkage
  function (`inline int counter(void){ static int n; return ++n; }`) now warns
  "'n' is static but declared in inline function 'counter' which is not static",
  matching gcc/clang exactly. `gen_function` (`src/mccgen.c`) sets
  `cur_func_inline_extern` when the function is `inline` and not `static`
  (`extern inline` has had `VT_INLINE` stripped, so it is excluded); the local
  static-object decl site checks it. A `const` static local, a `static` (internal-
  linkage) function, and an `extern inline` function are all silent. cli
  `inline_extern_static_object`. (The internal-linkage *reference* sub-case — only
  gcc, not clang, warns — is left out to stay within the 3-way-agreed set.)

- [x] **[DIAG] §6.7.1 / §6.2.2p7 — `static` declaration following `extern` now an error.**
  `extern int x; static int x;` now errors "static declaration of 'x' follows
  non-static declaration" (`src/mccgen.c`, `patch_type` — directional check on
  the `VT_STATIC` mismatch). The well-defined reverse (`static int y; extern int
  y;`) stays silent. Matches gcc/clang. cli `linkage_static_after_extern`.

---

## §6.2–6.3 / §6.8–6.9 Conversions, statements, external definitions

*Cross-cutting: `_Complex` is an anonymous struct. The implicit store/init paths
(`vstore`, `verify_assign_cast`) used to fall into a raw `VT_STRUCT` byte copy;
they now route complex-involved stores through `gen_complex_cast` (fixed below).
The one remaining complex gap is constant-folding (static initializers) — see
`__builtin_complex` in the infrastructure section.*

- [x] **[BUG] §6.3.1.6 — implicit complex→complex conversion across element types fixed.**
  `double _Complex w = f;` (f a `float _Complex`) now converts each part instead
  of raw-copying the source bytes. `vstore` (`src/mccgen.c`) now `gen_cast`s the
  source to the destination complex type when the two complex element types
  differ — **gated to differing elements only** (a same-element complex store is
  a correct raw copy *and* is what `cplx_materialize` uses internally, so
  matching it would recurse into `gen_complex_cast` → vstack overflow). Runtime-
  verified vs gcc (float→double/long double, init+assign). exec
  `c11_complex_convert` (extended).

- [x] **[BUG] §6.3.1.7 — implicit complex→integer / complex→_Bool conversion fixed.**
  `int n = z;`, `n = z;`, `_Bool b = z;` now convert via the real part.
  `verify_assign_cast` integer case now accepts a complex source
  (`src/mccgen.c`); `vstore` `gen_cast`s a complex source to a real scalar
  destination before the store (drops the imaginary part) instead of raw-copying
  the struct. Runtime-verified vs gcc (`n=3`, `b=1`); return/arg paths (already
  via `gen_assign_cast`) unaffected. exec `c11_complex_convert` (extended).

- [x] **[BUG] §6.9.2/§6.6 — static complex initializer: both `CMPLX` and `a+b*I` forms now work.**
  `static double _Complex g = CMPLX(3.0,4.0);` and `static double _Complex g =
  3.0 + 4.0*I;` both now produce a constant. Complex `+`/`-`/`*`/`/` and
  real↔complex / cross-element conversions are constant-folded in `gen_complex_op`
  / `gen_complex_cast` (see the `__builtin_complex` FEATURE item below for the
  mechanism). 3-way: mcc/gcc/clang all accept, `creal`/`cimag` byte-match.

- [x] **[DIAG] §6.8.6.1 — jump into the scope of a variably-modified non-array object now diagnosed.**
  A goto/switch into the scope of a pointer-to-VLA (`int (*p)[n]`) — variably
  modified but not itself a VLA array, so it allocates nothing — now errors "goto
  jumps into the scope of a variably modified declaration", matching gcc/clang.
  A new `type_is_vm()` helper (`src/mccgen.c`) detects a VLA anywhere in a type's
  pointer/array derivation; `decl_initializer_alloc` registers the jump-diag scope
  (`vla_open_birth`/`vla_diag`) for the VM-but-not-VLA case without allocating
  (the VLA-array case still registers + allocates as before). cli
  `jump_into_vla_scope` (extended with the pointer-to-VLA case).

- [x] **[DIAG] §6.8.1 — label immediately followed by a declaration now diagnosed under `-pedantic`.**
  `L: int x;` (and `case`/`default:` followed by a declaration) is a constraint
  violation pre-C23. A new `tok_starts_declspec()` predicate (declaration-specifier
  keywords + typedef-name lookup, `src/mccgen.c`) is checked at `block_after_label`
  (covering named, `case`, and `default` labels) and emits `mcc_pedantic` "a label
  can only be part of a statement and a declaration is not a statement", matching
  gcc/clang `-Wfree-labels`. Gated on `cversion < 202311` so it is silent in C23
  (where it is legal); a label-then-statement stays clean. cli
  `label_then_declaration`.

---

## §6.10 / §5 Preprocessor & environment

- [x] **[BUG] §6.10.3.3 — token paste forming `//` / `/*` now diagnosed (was a hang).**
  `C(/,/)` used to spin forever (>5GB): re-lexing the pasted `//` ran
  `parse_line_comment` off the synthetic `:paste:` buffer (no terminating
  newline), so `buf_ptr` never reached the NUL. Now `macro_twosharps`
  (`src/mccpp.c`) scans the accumulated pasted spelling for a comment introducer
  (`/` followed by `/` or `*`, anywhere — catches `x//` from `x ## / ## /`) and
  hard-errors "pasting formed '%s', an invalid preprocessing token", matching
  gcc/clang (rc=1). Valid pastes — incl. the `/=` operator (`/` ## `=`) — are
  unaffected. cli `paste_comment_introducer` (with a `timeout` hang-guard).

- [x] **[FEATURE] §5.2.1.1 / §5.1.1.2 — trigraphs now replaced in strict ISO `-std` mode.**
  The `-std` parser (`src/libmcc.c`) records a `strict_iso` flag for `c`/
  `iso9899:` prefixes (not `gnu`) and sets `s->trigraphs` accordingly:
  `-std=c89/c99/c11/c17` process trigraphs (gcc/clang do), `-std=gnu*` and the
  default leave them off, and C23 (`-std=c23`) keeps them off (removed there).
  `-trigraphs`/`-ftrigraphs` still force them on. Verified 3-way on all 5 arches.
  cli `trigraphs_strict_std`.

- [x] **[DIAG] §6.10.8p2 — `#define`/`#undef` of a builtin predefined macro now diagnosed.**
  The magic builtin tokens (`__LINE__`/`__FILE__`/`__DATE__`/`__TIME__`/
  `__COUNTER__`) bypassed the regular-table redefinition warning. Added
  `is_predef_macro()` (`src/mccpp.c`) checked in `parse_define` ("%s redefined")
  and the `#undef` handler ("undefining %s"); ordinary macros stay clean under
  `-Werror`. Matches gcc/clang (`-Wbuiltin-macro-redefined`). cli
  `builtin_macro_redefine`. (Also retargeted the `pp_defined_operator` torture
  test's section 5 from redefining `__LINE__` to a `PASTE` macro — it no longer
  asserts a constraint violation works silently, while keeping the `# ## #`→`##`
  paste coverage.)

- [x] **[DIAG] §6.10.3p4 — variadic macro invoked with no arguments for `...` now diagnosed.**
  `V("hi")` for `#define V(fmt,...)` now warns under `-pedantic` / errors under
  `-pedantic-errors` ("ISO C does not permit a variadic macro to be invoked with
  no argument for the '...'"), at the empty-variadic arg-gathering site
  (`src/mccpp.c`, `macro_subst_tok`). Silent at default, silent with an argument.
  Matches gcc/clang (-pedantic). cli `va_args_empty_pedantic`.

- [~] **[DIFF] §6.10.3.3p3 — invalid token paste is warn-and-continue (deliberate leniency).**
  `C(+,-)` / `C(*,/)` emit a warning and continue (rc=0, output `+ -` / `* /`).
  The result is UB (no diagnostic strictly mandated); mcc deliberately recovers
  rather than aborting, and this is locked in by the `pp_invalid_paste` exec
  golden. gcc/clang hard-error; this is an intentional, tested difference, kept.
  (The genuinely-broken comment-introducer case `//`/`/*` IS a hard error now —
  see §6.10.3.3 above.) 3-way: mcc=warn(rc=0) | gcc=error | clang=error.

- [x] **[DIAG] §6.10.3.2p2 — stringizing an argument ending in a stray `\` now drops it and warns.**
  `#define S(x) #x` / `S(a\)` previously stringized to the invalid `"a\"` (the
  trailing lone backslash escaped the closing quote → "unknown escape sequence"
  at compile). The stringize builder (`macro_arg_subst`, `src/mccpp.c`) now counts
  the trailing backslash run before appending the closing quote; an odd run means
  the last `\` is dangling, so it is dropped with "invalid string literal,
  ignoring final '\'" — yielding the valid `"a"`, matching gcc exactly. An even
  backslash run (a real escaped `\\`) and normal arguments stringize unchanged.
  cli `stringize_trailing_backslash`.

---

## §7 Library — freestanding headers, builtins, atomics, complex

*Cross-cutting: `__builtin_complex` is now implemented (runtime + constant-into-
rodata), so `CMPLX` and same-element static `_Complex_I` work. The remaining
complex-constant gap is folding complex *arithmetic* (`a+b*I`) and cross-element
complex→complex *constant conversion* — see §6.9.2 and §7.3.1 above.*

- [x] **[FEATURE] §7.3.1p2 — `_Complex_I` / `I` is a constant of the right type (incl. cross-element).**
  `complex.h` defines `_Complex_I` as `__builtin_complex(0.0f, 1.0f)` — type
  `float _Complex` (matching the standard) and a genuine constant expression.
  `static float _Complex si = _Complex_I;` works (0,1); runtime `1.0 + 2.0*I`
  works. The former cross-element sliver (`static double _Complex = _Complex_I`,
  float→double complex constant) **now folds correctly** via the complex
  constant-conversion path in `gen_complex_cast` (see the `__builtin_complex`
  FEATURE item). exec `c11_complex_convert` + `c11_complex_const_fold`.

- [x] **[FEATURE] §7.3.9.3 — `CMPLX`/`CMPLXF`/`CMPLXL` macros.**
  Added to `runtime/include/complex.h` via the new `__builtin_complex`
  (`src/mccgen.c`, `TOK_builtin_complex`). Works at runtime AND as a constant
  expression in static/file-scope initializers: when both parts are constants,
  `__builtin_complex` emits the complex into a rodata anonymous object
  (`section_add`/`vpush_ref`/`init_putv`, like a struct compound literal) so it
  is a load-time constant. Runtime-verified vs gcc on all 5 arches; static +
  runtime + global covered by exec `c11_complex_convert`.

- [x] **[BUG] §7.17.7 — atomic load/store/exchange/CAS on aggregate `_Atomic` types now work.**
  mcc's `gen_atomic_{load,store}_aggregate` lowering emits the size-generic
  `__atomic_load`/`store`/`exchange`/`compare_exchange(size, mem, …, order)`
  calls; the runtime now provides them (`runtime/lib/atomic.c`) as portable-C
  libatomic-compatible functions backed by an address-indexed spinlock pool
  (the `__atomic_*_n` byte-lock lowers to the size-1 ops already in the file).
  `_Atomic struct{int x,y,z;}` load/store/exchange/compare_exchange link and run,
  matching gcc on all 5 arches. exec `atomic_aggregate`; the lowering's symbol
  emission is also covered by cli `atomic_aggregate_load_generic`.

- [x] **[DIAG] §7.1.4p1 — `creal`/`cimag` family now have callable functions.**
  `complex.h` now declares `double creal(double _Complex);` etc. *before* the
  function-like macros (so the prototypes aren't macro-expanded). `(creal)(z)`
  and `&creal` now work (resolve against libm with `-lm`); the fast `creal(z)`
  macro path is unchanged. Verified 3-way (`3.0 3.0 3.0`). cli
  `complex_creal_function`.

- [x] **[FEATURE] §7.28 — `<uchar.h>` now bundled.**
  Added `runtime/include/uchar.h` (`#include_next` then a fallback guarded by
  glibc's `_UCHAR_H` sentinel + `__mbstate_t_defined`): `char16_t`/`char32_t`
  from the `__CHAR16/32_TYPE__` predefs, a minimal `mbstate_t`, and the
  `mbrtoc16`/`c16rtomb`/`mbrtoc32`/`c32rtomb` libc prototypes. Works hosted and
  freestanding (`-nostdinc`), and coexists with `<wchar.h>` (no `mbstate_t`
  clash). cli `uchar_header`. (The bundled copy shadowing the system one on the
  `include_next` path is why the `_UCHAR_H` sentinel — not bare `include_next` —
  is required.)

- [x] **[DIAG] §7.17.8 — `atomic_flag_*` now typed `volatile atomic_flag *`.**
  `runtime/include/stdatomic.h` declarations changed from `void *object` to
  `volatile atomic_flag *object` (matching the standard / gcc). A non-pointer
  argument is now diagnosed; `&atomic_flag` usage compiles clean under `-Werror`.
  cli `atomic_flag_type`.

---

## Sweep 2 — additional gaps (fresh 5-agent aggressive pass, 2026-06-29)

A second clause-by-clause sweep (Annex F/G FP, deep §7 library, §6.3/6.5/6.6
expressions, §6.7/6.2 declarations, §6.10/§5 preprocessor), each item 3-way
verified vs gcc 15.3 / clang 22 and confirmed *not* already tracked above. The
bulk of each area matched the references; these are the residual divergences.

### §6.4 / §6.10 lexical & preprocessor

- [x] **[BUG] §5.1.1.2 — `mcc -E` no longer hangs on an identifier followed by `\`+whitespace+newline.**
  The `parse_ident_ucn` loop (`src/mccpp.c`) re-read a stray `\` forever
  (`handle_stray` under `-E`/`ACCEPT_STRAYS` re-presents the `\` without
  consuming it; `-c` errored instead, so only `-E` hung). Now: if `PEEKC` yields
  `\` again (a stray, not a consumed splice), the identifier ends (`break`) and
  the `\` is tokenized separately — the loop always makes progress. In-identifier
  line splices (`ab\<NL>cd`→`abcd`) and UCN identifiers still work. cli
  `ident_backslash_no_hang` (with a `timeout` hang-guard). (Full gcc-style
  splice of `\`+ws+NL — emit `ab` + warning instead of a stray `\` — is the
  separate item below.)

- [~] **[BUG] §5.1.1.2 / §6.10 — `\`+whitespace+newline not spliced (now terminates, still not gcc-compat).**
  The hang above is fixed; the residual is behavioral: `-c` hard-errors "stray
  '\'" and `-E` emits a stray `\` token, whereas gcc/clang treat `\`+ws+NL as a
  line-continuation splice with a `-Wbackslash-newline-escape` warning. Making
  `handle_stray` consume `\`+whitespace+newline as a warned splice would match
  the references. Low priority (extension; mcc's strict-ISO rejection is
  defensible).
  3-way: mcc=stray-`\`/no-splice | gcc/clang=splice+warn.

- [x] **[DIAG] §6.4.4.4p10 — multi-character character constant now warns by default.**
  The existing `'ab'` warning (`src/mccpp.c`) was gated on `-Wall`; now it is a
  default-on `mcc_warning` (matching gcc/clang `-Wmultichar`), still suppressible
  with `-w`. Single-char constants stay clean. cli `multichar_warning` (now at
  default level). Also covers the `#if 'ab'` case.

- [x] **[DIAG] §6.10.1 — integer overflow in a `#if` controlling expression now diagnosed.**
  `#if 9223372036854775807 + 1 < 0` (and the `*` form) now emit `mcc_warning`
  "integer overflow in preprocessor expression", matching gcc/clang. In the
  constant folder `gen_opic` (`src/mccgen.c`), when `pp_expr` is active and the
  operands are signed 64-bit, a new `pp_signed_ovf()` helper detects `+`/`-`/`*`
  overflow before the wrap. `pp_expr` is temporarily zeroed across the warning so
  `error1()`'s pp-expression message-rewrite (and its token-stream consumption)
  doesn't fire. Unsigned wraparound and a sum that stays within 64-bit (e.g.
  `INT_MAX+1`, evaluated in intmax_t) stay silent; div-by-zero still hard-errors.
  cli `pp_if_integer_overflow`.

- [x] **[DIAG] §6.10.4p3 — `#line` number out of range now diagnosed; no more negative wrap.**
  The `#line` digit sequence is now accumulated in 64-bit (`src/mccpp.c`, the
  `_line_num` loop): a value of `0` or `> 2147483647` violates the §6.10.4p3
  constraint and emits "line number out of range" under `-pedantic`
  (`-pedantic-errors` → error), matching gcc/clang. The carried `__LINE__` is
  clamped to `INT_MAX` instead of wrapping to `-2147483648` even without
  `-pedantic`. A valid line number stays clean under `-pedantic-errors`. cli
  `line_number_out_of_range`.

### §6.3 / §6.5 / §6.6 conversions & expressions

- [x] **[DIAG] §6.5.16.1/§6.3.2.3p2 — nested-pointer qualifier laundering now diagnosed.**
  `const int **q = p;` (p is `int**`) now warns "assignment from incompatible
  pointer type" — closing the silent const-laundering hole. In `verify_assign_cast`'s
  pointer-descent loop (`src/mccgen.c`), beyond the immediately-pointed-to level
  the destination adding a qualifier the source lacks now sets a `deepqual` flag
  that marks the types incompatible (the "left has all qualifiers" relaxation is
  correctly applied only at level 0). Top-level add (`int*→const int*`) stays
  valid; top-level discard (`const int*→int*`) still warns "discards qualifiers".
  Matches gcc/clang (which both reject `int**→const int**`). The existing
  `errors_and_warnings` test had a silently-accepted laundering case (line 107)
  now correctly flagged. cli `nested_pointer_qualifier_launder`.

- [x] **[OPT] §6.5.6 — `void *` / function-pointer arithmetic now diagnosed under `-pedantic`.**
  `gen_op` (`src/mccgen.c`, the `VT_PTR` arithmetic branch) now emits
  `mcc_pedantic` "ISO C forbids arithmetic on a pointer to 'void' or to a
  function" for `+`/`-`/`++`/`--` and pointer-pointer subtraction when the
  target type is `void` or a function. Object/`char` pointer arithmetic stays
  clean. Matches gcc/clang `-Wpointer-arith`. cli `void_fn_pointer_arith`.

- [x] **[OPT] §6.3.2.3 — function-pointer ↔ `void *` conversion now diagnosed under `-pedantic`.**
  `verify_assign_cast` (`src/mccgen.c`, the `void*`-vs-incompatible branch) now
  emits `mcc_pedantic` "ISO C forbids conversion between a function pointer and
  'void *'" when one pointer targets a function and the other is `void`.
  `void*`↔object* and an explicit `(void*)fp` cast stay valid. Matches gcc/clang
  `-pedantic`. cli `fn_pointer_void_conversion`.

- [x] **[OPT] §6.6p6 — float-derived (non-ICE) array size now diagnosed at file scope.**
  `int a[(int)(1.0+2.0)];` / `int a[(1.0<2.0)?4:2];` now warn "ISO C forbids an
  array size that is not an integer constant expression" under `-pedantic` at file
  scope. `gen_opif` (`src/mccgen.c`) sets a new `ice_float_op` flag whenever a
  floating arithmetic/comparison is folded in a `CONST_WANTED` context; the
  array-size site resets it before `expr_const()` and warns if it folded to a
  constant via a float op. A floating constant as the *immediate* operand of a
  cast goes through `gen_cast` not `gen_opif`, so `(int)3.0` and `(int)1.5+(int)2.5`
  stay valid ICEs (silent even under `-pedantic-errors`). Block-scope arrays fold
  silently — VLAs are allowed there, matching gcc/clang exactly. cli
  `float_derived_array_size`.

### §6.7 / §6.2 / §6.9 declarations

- [x] **[DIAG] §6.7.1p2 — `auto`/`register` now enter the multiple-storage-class check.**
  The `auto`/`register` cases (`src/mccgen.c`) now error "multiple storage
  classes" when another storage class (extern/static/typedef or a prior
  auto/register) is present, and the `storage:` label also rejects a prior
  auto/register. `static auto`, `register static`, `auto auto`, etc. now error;
  a single `register` param, block-scope `auto`, and static/extern/typedef alone
  still compile. gcc+clang both reject. cli `storage_class_exclusivity`; 5-arch.

- [x] **[DIAG] §6.7.6.3p7 — `int a[static]` (no size operand) now rejected.**
  The array-parameter `[` parser (`src/mccgen.c`, `post_type`) tracks a
  `saw_static` flag and errors "'static' may not be used without an array size"
  when `static` is followed by `]` with no size. `[static N]`, `[const N]`, `[]`
  still compile. gcc+clang both reject. cli `array_static_param`; 5-arch.

- [x] **[DIAG] §6.7p3 — C99 typedef redefinition (same type) now diagnosed under `-pedantic`.**
  The same-type typedef-redefinition path (`src/mccgen.c`) now emits
  `mcc_pedantic` "redefinition of typedef is a C11 feature" when
  `cversion < 201112` (C99/C90). Silent in C11; incompatible-type redefinition
  is still an error in all modes. cli `typedef_redefinition_c99`.

- [x] **[DIAG] §6.7.2.1 — struct with only unnamed bit-fields now diagnosed under `-pedantic`.**
  `struct S{int:4;};` / `union U{int:4;};` now emit `mcc_pedantic` "ISO C forbids
  a struct/union with no named members", matching gcc/clang `-Wpedantic`. The
  existing empty-struct check (`src/mccgen.c`, after the member loop) was broadened
  from `!type->ref->next` to `!c`, where `c` is set for a named member, an
  anonymous struct/union member, or a named flexible array member — so the valid
  forms (`struct C{int x;int:4;}`, anon members, FAMs) stay silent. cli
  `empty_struct_pedantic` (now also covers the unnamed-bit-field cases).

- [x] **[DIAG] §6.7.2.1 — empty struct/union now diagnosed under `-pedantic`.**
  `struct S{};` / `union U{};` (no members) now emit `mcc_pedantic` "ISO C
  forbids a struct/union with no named members" (`src/mccgen.c`, after the
  member loop, when `type->ref->next` is empty). A named member or an anonymous
  struct/union member keeps it valid. cli `empty_struct_pedantic`. (The
  unnamed-bit-field-only case `struct S{int:4;}` pushes a member so it is not
  caught here — that finding stays open below.)

- [x] **[DIAG] §6.7.2.1 — anonymous struct/union members in C99 now diagnosed under `-pedantic`.**
  `struct S { struct { int x; }; };` now emits `mcc_pedantic` "anonymous
  structs/unions are a C11 feature" when `cversion < 201112` (`src/mccgen.c`
  struct_decl, the `>= SYM_FIRST_ANOM` no-declarator branch). Silent in C11, and
  suppressed in system headers (mcc's own `__va_list_tag` no longer trips it) via
  the system-header-suppression infrastructure above. cli
  `anon_member_c99_and_sysheader`.

- [x] **[DIAG] §6.9p1 — stray `;` at file scope now diagnosed under `-pedantic`.**
  A lone `;` at file scope (`l == VT_CONST` in `decl()`, `src/mccgen.c`) now emits
  `mcc_pedantic` "ISO C does not allow an empty declaration". A `;` inside a
  function stays a valid null statement (no warning). Matches gcc/clang
  `-Wpedantic`. cli `empty_declaration_pedantic`.

- [~] **[DIFF] §6.7.2.1 — no-declarator tagged struct *member*: mcc matches gcc `-fms-extensions` (silent).**
  `struct S { struct T { int x; }; };` — re-verified 3-way: gcc *default* (no
  `-fms-extensions`) warns "declaration does not declare anything"; but gcc
  **`-fms-extensions` is silent even under `-pedantic`**, and clang
  `-fms-extensions -pedantic` instead warns "anonymous structs are a Microsoft
  extension". mcc enables MS extensions by default (so the nested tag becomes an
  anonymous member), which matches gcc `-fms-extensions` exactly — silent. The two
  references disagree once MS extensions are on, and mcc tracks gcc; adopting
  clang's MS-extension warning would diverge from gcc. Left as a defensible DIFF.
  (`src/mccgen.c` already warns when `ms_extensions == 0`.)

- [x] **[DIAG] §6.7.9p14 — wrong-element-type string-literal initializer now gives a clear message.**
  `int a[4] = "abc";` / `char a[]=L"abc"` now error "cannot initialize array of
  'T' from a string literal of a different character type" instead of the old
  "integer from pointer" + "',' expected" cascade. A new `else if` in the array
  branch of `decl_initializer` (`src/mccgen.c`) catches a string-literal token
  whose element type doesn't match the (scalar) array element — gated on
  `no_oblock` and `!(t1->t & VT_ARRAY)` so braced (`char a[4]={"ab"}`) and nested
  sub-array (`char m[][3]={"ab","cd"}`) string inits still recurse and stay valid.
  cli `string_init_element_mismatch`.

- [~] **[DIFF] §6.8.6.4p1 — `return <expr>;` in `void` / `return;` in non-`void`: CONFORMANT (diagnosed as a default-on warning).**
  §6.8.6.4 is a *constraint*, which requires only that a conforming implementation
  issue **a diagnostic** — it does not mandate an error. mcc emits a default-on
  warning ("void function returns a value" / "'return' with no value") and
  upgrades it to an error under `-Werror`, so the standard's requirement is met.
  This is mcc's deliberate, **consistent** lenient-warning stance for constraint
  violations — verified identical to the const-assignment §6.5.16.1 case (both
  warn by default, both become errors under `-Werror`). gcc/clang choosing to make
  it a hard error by default is their policy, not a standard requirement. Left as a
  conformant DIFF (forcing an error would diverge from mcc's coherent
  permissive-by-default / `-Werror`-enforces philosophy for no conformance gain).

- [ ] **[DIFF] §7.16.1.4 — `va_start` on a `register`-qualified last parameter not diagnosed (fragmented, no mandated diag).**
  `int f(register int n, ...){ va_start(a,n); }` is silent; both refs warn
  (`-Wvarargs`, default-on). The standard mandates **no** diagnostic (it is UB),
  and mcc's `va_start` is implemented differently per target (`runtime/include/
  mccdefs.h`): the x86_64 SysV macro reads `__builtin_frame_address(0)` and never
  references `last` at all (so register-ness is invisible); i386/arm/generic
  expand to `&(last)`, which *does* already trip mcc's "address of register
  variable requested" diagnostic; arm64/riscv64/x86_64-PE use a builtin token and
  are silent. A uniform `-Wvarargs` would need per-target plumbing for a
  non-mandated courtesy warning — low value. Left open.

### §7 library / floating-point builtins

- [~] **[FEATURE] §F/§7.12 — GCC/Clang floating-point builtins (constants + classification done).**
  Added to `runtime/include/mccdefs.h` as constant-foldable macros (`#ifndef`-
  guarded so the BSD/Apple defines win where present): `__builtin_inf`/`inff`/
  `infl`, `huge_val`/`valf`/`vall`, `nan`/`nanf`/`nanl`/`nans*` (inf = folded
  overflow product, nan = `0.0/0.0`), and the `isnan`/`isinf`/`isfinite`/
  `isunordered`/`isgreater`/`isgreaterequal`/`isless`/`islessequal`/`islessgreater`
  classification + comparison builtins. Direct use compiles, links, and folds in
  a constant expression (`static int a=__builtin_isnan(0.0/0.0)`), matching gcc on
  all 5 arches. exec `fp_builtins`. Also added `fabs`/`fabsf`/`fabsl`,
  `signbit`/`signbitf`/`signbitl` (incl. `-0.0` detection via `1/x`), and
  `copysign`/`copysignf`/`copysignl` as constant-foldable macros — verified 3-way.
  `fpclassify`/`isnormal`: **work correctly via glibc + `-lm`** (verified:
  `fpclassify` returns `FP_ZERO`/`FP_NORMAL`/`FP_INFINITE`/`FP_NAN`/`FP_SUBNORMAL`
  and `isnormal` is correct for normal/zero/subnormal/inf). They route to glibc's
  `__fpclassify` function (not the gcc builtin) because mcc defines no `__GNUC__`,
  so they need `-lm` where gcc's builtin does not — a usability DIFF, not a
  correctness gap. They cannot be replaced by clean constant-foldable macros: the
  subnormal test needs the *per-type* smallest-normal threshold, which a
  type-agnostic macro can't know (gcc uses a type-aware builtin). The macro
  builtins above also multi-evaluate their argument (intrinsics don't). The two
  DIFFs below (NAN sign, signbit return) come from glibc's *own* fallback macros
  via the same no-`__GNUC__` routing — both verified conforming.

- [~] **[DIFF] §7.12/F.2.1 — `NAN` yields a *negative* NaN: CONFORMANT (sign unspecified); root cause = mcc doesn't define `__GNUC__`.**
  glibc's `NAN`/math macros pick their implementation by `__GNUC_PREREQ`; mcc
  defines **no `__GNUC__`** (a deliberate, conservative tcc-lineage choice — not
  claiming GCC compatibility avoids pulling in gcc-only header code paths), so
  glibc falls to its non-builtin fallback (`(0.0f/0.0f)` → sign-bit-set NaN).
  Annex F leaves the NaN sign **unspecified**, so `-nan` is fully conforming; it
  merely differs cosmetically from gcc/clang's `+nan`. The only "fix" is to make
  mcc advertise `__GNUC__ >= 6` so glibc routes to `__builtin_nan` — a broad,
  risky GCC-compat claim that changes behavior across *every* system header, taken
  purely to match an unspecified sign. Not worth it; left as a conformant DIFF.
  3-way: mcc=`-nan` | gcc/clang=`nan` (all conforming).

- [~] **[DIFF] §7.12.3.6 — `signbit(-1.0)` returns 128: CONFORMANT (nonzero iff negative); same `__GNUC__` root cause.**
  §7.12.3.6 requires only a **nonzero** value when the sign is negative — `128`
  is nonzero, and verified mcc+glibc gives nonzero iff negative / zero iff
  non-negative, so it is **conforming**. As above, glibc routes `signbit` to its
  extern `__signbit` (returns the 0x80 sign byte) rather than `__builtin_signbit`
  because mcc defines no `__GNUC__`; mcc's own `__builtin_signbit(-1.0)` correctly
  returns `1` (used directly, off the glibc path). Matching gcc/clang's exact `1`
  would again require the risky blanket `__GNUC__` claim for no conformance gain.
  Left as a conformant DIFF. 3-way: mcc=128 | gcc/clang=1 (all conforming).

- [x] **[DIAG] §7.3.1p2 — `<complex.h>` no longer defines `_Imaginary_I`.**
  Removed the `#define _Imaginary_I _Complex_I` line from
  `runtime/include/complex.h` — the macro is defined iff the implementation
  supports imaginary types (mcc does not), matching gcc/clang. Verified
  `#ifdef _Imaginary_I` is now false; complex `I` arithmetic still works (covered
  by `c11_complex_*` exec tests).

### infrastructure (sweep 2)

- [x] **[FEATURE] §6.10.8.1/§5.1.2.1 — `-ffreestanding` now sets `__STDC_HOSTED__` to 0.**
  Added a `freestanding` state field (`src/mcc.h`) wired via `options_f`
  (`-ffreestanding` sets it, `-fhosted` clears it); `__STDC_HOSTED__` is now
  `(nostdlib || freestanding) ? 0 : 1` (`src/mccpp.c`). `-ffreestanding`→0,
  `-fhosted`/default→1, matching gcc/clang. cli `freestanding_hosted_macro`.
  (A distinct freestanding-vs-hosted *header set* is not implemented — the
  bundled freestanding headers already work via `-nostdinc`.)
  3-way: mcc=0 | gcc/clang=0.

---

## Cross-cutting infrastructure

- [x] **[TASK] System-header warning suppression.**
  Added a `system_header` bit to `BufferedFile` (`src/mcc.h`), set for the predef
  `<command line>` buffer (which embeds `mccdefs.h`: va_list, `__int128`, the
  `__builtin` macros) and for files resolved via a `-isystem`/default system path
  — and propagated to headers included from a system header. The central warning
  emitter (`src/libmcc.c`) now suppresses `ERROR_WARN` (incl. `-pedantic`)
  originating in a system context, *before* the `-Werror` upgrade, as gcc/clang
  do; errors are never suppressed. Verified: `-isystem` header warnings are
  silenced while the same construct in user code (`-I` or the main file) still
  warns; the va_list anon union no longer breaks `-std=c99 -pedantic`. This makes
  `-pedantic` usable against real libc headers and unblocked the anon-member
  diagnostic below. cli `anon_member_c99_and_sysheader`.

- [x] **[TASK] `-pedantic` / `-pedantic-errors` flag.** Already implemented
  (`src/libmcc.c:1676-1677,1930-1935` → `warn_pedantic`/`pedantic_errors`;
  `mcc_pedantic` at `src/mccgen.c:275`). 11 `mcc_pedantic` call sites fire under
  the flag (verified: `_Noreturn`-on-object and comma-in-constant-expr warn
  under `-pedantic`, hard-error under `-pedantic-errors`). Remaining work is
  adding the two *missing* pedantic diagnostics noted above
  (empty-`__VA_ARGS__` §6.10.3p4, label-then-declaration §6.8.1).

- [x] **[FEATURE] `__builtin_complex` + complex constant-folding — complete.**
  `__builtin_complex(re, im)` is implemented (`src/mccgen.c`, `TOK_builtin_complex`)
  for both the **runtime** case (`cplx_local`) and the **constant** case (rodata
  anonymous object via `section_add`/`vpush_ref`/`init_putv`). Wired to `CMPLX`
  and `_Complex_I`. **Now also folds complex constant *arithmetic* and conversions
  in static initializers** (§6.6/§6.7.9): `gen_complex_op` and `gen_complex_cast`
  recognise compile-time-constant operands — a real scalar constant, or a
  float/double complex rodata constant read back via the source section
  (`mcc_state->sections[elfsym(sym)->st_shndx]->data`, the same path the static-init
  memmove uses) — compute `+`/`-`/`*`/`/` (and real↔complex / cross-element complex
  conversion) with the scalar folder, and emit the result as a rodata complex
  constant. So `double _Complex a = 1.0 + 2.0*I;`, `(2.0+1.0*I)*(1.0+1.0*I)`, the
  float `_Complex_I` widened to double, division, and nesting all work at file
  scope, **byte-matching gcc** (`creal`/`cimag` verified). Limited to float/double
  elements (long double's in-memory layout isn't portable to read back when cross
  compiling — it stays the clean runtime path); the Annex-G robust runtime helper
  is unchanged for non-constant `*`/`/`. exec `c11_complex_const_fold`; runtime-
  verified on i386/arm/arm64/riscv64 via qemu + x86_64 native; self-host fixpoint
  preserved.

- [x] **[BUG] Self-host byte-reproducibility 2-cycle — FIXED (live `#ifndef __builtin_*` predef guards).**
  The 3-stage ELF self-host had been landing in a **2-cycle** (stage2 ≠ stage3,
  stage2 == stage4) instead of a byte-identical fixpoint. Root cause (diagnosed by
  diffing the stage outputs to `.data.ro`, then to the embedded predef text): the
  c2str-generated `mccdefs_.h` kept the `#ifndef __builtin_inff` / `#endif` guards
  (and the `inf`/`nan`/`huge_val` family) as **live** preprocessor directives —
  c2str emits column-0 lines as live directives and indented lines as injected
  string fragments. So during mcc's *self*-compilation those guards were evaluated
  against mcc's own predef macro state, and since the guarded text both *defines*
  the tested macro and *becomes* mcc's next-generation predefs, it self-oscillated:
  a generation that baked in `#define __builtin_inff` injected it as a predef, so
  the `#ifndef` skipped the fragment in the next generation, which then didn't
  inject it, so the one after re-included it — a stable 2-cycle (496 B = the 12
  `inf`/`nan` lines). The guards were also **broken for their stated purpose**
  ("BSD/Apple defines win where present") because those defines are injected
  *target-time* strings while the guard ran at *mcc-compile-time*. Fix: **indent**
  the guards in `runtime/include/mccdefs.h` so c2str emits them as injected
  string content — now evaluated in the target program (correct phase, matching
  the BSD/Apple defines) and no longer live during self-compilation. Result:
  **stage2 == stage3 == stage4 byte-identical (484713 B)**; self-hosted compiler
  passes 16/16 `tests/qemu/conformance`. See [[mcc-self-host-bootstrap]].

- [ ] **[TASK] Regression tests + cross-target/libc coverage for every item above.**
  Per the standing goal, each fix ships with a cli/exec/diff regression test and
  is verified across x86_64/i386/ARM/AArch64/RISC-V 64, ELF/PE/Mach-O, and
  glibc+musl (qemu matrix where runtime execution is needed). Codegen/conversion
  fixes (the complex and lvalue clusters especially) must be runtime-tested, not
  compile-tested — the project's recurring lesson is that a relaxation turning a
  compile error into a silent miscompile is a regression.

---

## Landed (this audit cycle)

- [x] §6.5.3.3p1 unary `-` rejects a pointer operand — cli `unary_minus_pointer`.
- [x] §6.4.4.4p9 octal + hex escape-out-of-range warnings (narrow literals) — cli `escape_out_of_range`.
- [x] §6.7.6.3p10 qualified lone `void` parameter rejected — cli `decl_storage_type_constraints`.
- [x] §6.7.1p3 `_Thread_local` + `typedef` rejected — cli `decl_storage_type_constraints`.
- [x] §6.7.6.2p2 `extern`/linkage on a VLA type rejected — cli `decl_storage_type_constraints`.
- [x] §6.10.8p2 `#define`/`#undef` of builtin predefined macros diagnosed — cli `builtin_macro_redefine`.
- [x] §6.5.4 cast result is not an lvalue (`(int)a=9`, `&(int)a` rejected) — cli `lvalue_cast_comma_constraints`.
- [x] §6.5.17 comma result is not an lvalue (`(a,b)=7`, `&(a,b)` rejected) — cli `lvalue_cast_comma_constraints`.
- [x] §6.10.3.3 token paste forming `//`/`/*` diagnosed instead of hanging — cli `paste_comment_introducer`.
- [x] §7.1.4p1 `creal`/`cimag` callable functions added (`(creal)(z)`, `&creal`) — cli `complex_creal_function`.
- [x] §7.28 `<uchar.h>` bundled (hosted + freestanding) — cli `uchar_header`.
- [x] §6.3.1.6 implicit complex→complex (differing element) conversion — exec `c11_complex_convert`.
- [x] §6.3.1.7 implicit complex→integer/_Bool conversion — exec `c11_complex_convert`.
- [x] §6.5.4 asm-operand GNU lvalue-cast carve-out preserved (regression guard) — exec `asm_lvalue_cast`.
- [x] §7.3.9.3 `__builtin_complex` + `CMPLX`/`CMPLXF`/`CMPLXL` (runtime + static constant) — exec `c11_complex_convert`.
- [x] §7.3.1 `_Complex_I` is a constant of type `float _Complex` (same-element + cross-element static folding + runtime) — exec `c11_complex_const_fold`.
- [x] §5.2.1.1 trigraphs processed in strict ISO `-std=cNN` mode (off for gnu/default/C23) — cli `trigraphs_strict_std`.
- [x] §6.10.3p4 empty-`__VA_ARGS__` variadic invocation diagnosed under -pedantic — cli `va_args_empty_pedantic`.
- [x] §7.17.8 `atomic_flag_*` typed `volatile atomic_flag *` (diagnoses non-pointer args) — cli `atomic_flag_type`.
- [x] §6.2.2p7 `static` after non-static `extern` is an error (reverse stays silent) — cli `linkage_static_after_extern`.
- [~] §6.4.5p5 mixed wide/narrow string concat: wide-first widening fixed; narrow-first promotion errors cleanly — exec `string_concat_mixed`.
- [x] §6.4.5p2 conflicting wide-prefix string concatenation rejected — exec `string_concat_mixed`.
