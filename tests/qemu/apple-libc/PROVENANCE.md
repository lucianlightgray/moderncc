# Vendored Apple libc sources (real-target-libc Mach-O testing)

These are **verbatim** copies of Apple's open-source C library — the actual code
that ships inside macOS's `libSystem` — compiled by mcc for x86_64 Darwin,
linked into Mach-O images, and executed on a Linux host via
`tests/qemu/macho/loader.c` (`run_macho_apple_libc.cmake`, CMake test
`macho-apple-libc`). No function body is modified; `shim-include/` is only a
tiny freestanding compat layer so the sources build without a macOS SDK.

## `src/` — Apple `Libc`, `string/FreeBSD/`

(`apple-oss-distributions/Libc`, branch `main`)

| file        | upstream path              |
|-------------|----------------------------|
| strcspn.c   | string/FreeBSD/strcspn.c   |
| strpbrk.c   | string/FreeBSD/strpbrk.c   |
| strsep.c    | string/FreeBSD/strsep.c    |
| memmem.c    | string/FreeBSD/memmem.c    |
| strchrnul.c | string/FreeBSD/strchrnul.c |
| strnstr.c   | string/FreeBSD/strnstr.c   |

## `src-libplatform/` — Apple `libplatform`, `src/string/generic/`

(`apple-oss-distributions/libplatform`, branch `main`) — the **core string and
memory routines**: `strlen strcmp strncmp strcpy strncpy strlcpy strlcat strchr
strstr strnlen memmove memcpy memcmp memchr memccpy bzero memset
memset_pattern4/8/16`.

These are the functions that, on a shipping macOS, are selected as hand-tuned
x86_64 **assembly** (in `libplatform`) via the Darwin **commpage**. Crucially,
that assembly is only an *optimization variant*: Apple ships these **portable C**
implementations as the functional equivalent, gated by the `_PLATFORM_OPTIMIZED_*`
macros (which `<platform/string.h>` itself defaults to `0`). So Apple's genuine
core libc **does** run off-Darwin — it is exercised here as a Mach-O image, with
no hand-written stand-in: the `Libc` FreeBSD funcs resolve `strlen`/`memcmp`/
`memchr`/`strncmp` against these real `libplatform` implementations, so **every**
string/memory function under test is Apple's own source.

## `src-simple/` — Apple `libplatform`, `src/simple/string_io.c`

(`apple-oss-distributions/libplatform`, branch `main`) — Apple's **self-contained
formatted-output engine** `__simple_bprintf` / `_simple_vsnprintf` /
`_simple_snprintf`. Apple ships this precisely because it has **no FILE / locale /
malloc dependency** — low-level code (e.g. libmalloc's own error reporting) needs
to format strings before/without the full stdio + locale machinery. The
fixed-buffer entry `_simple_vsnprintf` writes into the caller's buffer via the
real engine and **never touches Mach VM**: the `vm_allocate` paths live only in
the growing-string functions (`_simple_salloc`/`_enlarge`/`_simple_sfree`) it does
not call. So Apple's genuine printf engine — `%d %i %u %o %x %X %p %c %s`, field
width, zero-pad, `l`/`ll` length modifiers — runs as a Mach-O image off-Darwin.
The unused Mach-VM/`write`/`errno` symbols it references on the grow path are
stubbed in the link (the kernel's role; never executed here).
`apple_simple_conf.c` checks the formatted output byte-for-byte.

The three self-checking programs (`apple_string_conf.c`, `apple_libplatform_conf.c`,
`apple_simple_conf.c`) exercise the full surface and must exit 0.

## `shim-include/` compat layer (no source bodies touched)

`__FBSDID`→nothing; `u_long`/`u_char`/`uintptr_t`/`size_t`; literal `LONG_BIT 64`
(x86_64 is LP64, and `strcspn.c` does `#if LONG_BIT == 64`); `UCHAR_MAX`; no-op
`libc_hooks_*`; FreeBSD `__weak_reference(sym,alias)` → a global asm `.set`
alias (Darwin C names gain a leading `_`); and `<platform/string.h>` with all
`_PLATFORM_OPTIMIZED_* 0` + `VARIANT_STATIC 1` so the portable C bodies compile
and export their public names.

## The remaining boundary (genuinely needs macOS/darling)

What is left of `libSystem` after the string/memory layer is *structurally* fused
to the Darwin kernel and cannot execute on a non-macOS host (this is why darling
exists and is a multi-year project):

* **`malloc`/`free`/`realloc`** (`libmalloc`) — verified **empirically**, not just
  by reading: `mcc -c src/malloc_common.c` (with the same shim that builds the
  portable sources) stops immediately at `internal.h:32` with a hard
  `#error "Must set OS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY..."`, and
  `internal.h` pulls **~34 Apple-private headers** that do not and cannot exist
  off-Darwin: `mach/mach.h` `mach/mach_vm.h` `mach/vm_reclaim_private.h` …,
  `os/lock_private.h` `os/tsd.h` `os/atomic_private.h` `os/once_private.h`,
  `mach-o/dyld_priv.h`, `corecrypto/ccsha2.h`, `machine/cpu_capabilities.h` (the
  commpage), `ptrauth.h`. The allocator *algorithm* (magazine/nano/xzone) is C,
  but it is fused to `os_unfair_lock` (kernel `__ulock`), Darwin TSD (`%gs`),
  `mach_vm_allocate` (MIG via `mach_msg`), dyld SPIs, and corecrypto. Compiling
  it = reconstructing Apple's entire private SDK + Mach + dyld — i.e. the darling
  project — not the trivial build glue the portable sources needed.
* **FILE-backed / locale `stdio`** (`vfprintf`/`vsnprintf` in Libc) — verified by
  reading `Libc/stdio/FreeBSD/vsnprintf.c`: it is bound to Apple's `xlocale`
  (`__current_locale`), the extensible-printf engine (`__v2printf`,
  `printf_comp_t`/`printf_domain_t`), and the `__sFILE`/`__sFILEX` internals, and
  pulls `gdtoa` for floating conversions. Running it would mean reconstructing
  large parts of Apple's private FILE/locale subsystem, not just build glue — so
  it is out of scope. (The locale-free `_simple` engine above IS exercised.)
* **`dyld`, `pthread`/GCD, Objective-C runtime, Mach IPC** — kernel- and
  dyld-shared-cache-bound by definition.

Verifying *those* requires a macOS or darling host. This suite therefore runs
Apple's genuine code for everything that is portable (the entire core
string/memory libc), and documents the kernel-fused remainder rather than
substituting a stand-in for it. The standard cross-compilation division still
applies: mcc (the compiler) emits the calls and the correct Mach-O container/ABI;
the hosted kernel-bound library is the platform's, exercised where the platform
is available (glibc/musl on ELF, msvcrt on PE).
