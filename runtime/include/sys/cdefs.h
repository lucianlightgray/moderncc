/* NOTE: intentionally no `#ifndef _MCC_SYS_CDEFS_H` include guard.
   mcc ships this file in more than one search directory (the -B build/include
   tree and the -I runtime/include tree), so a copy is often reached twice while
   resolving a single <sys/cdefs.h>.  An include guard would make the second
   copy a no-op and swallow its `#include_next`, so the real platform header
   below would never be reached.  glibc's own <sys/cdefs.h> is idempotent (it
   has its own guard), and the trailing `#undef` is harmless to repeat. */

/* Pull in the platform's real <sys/cdefs.h> (glibc, etc.). */
#if defined __has_include_next && __has_include_next(<sys/cdefs.h>)
# include_next <sys/cdefs.h>
#endif

/* glibc's <sys/cdefs.h> does `#define __attribute__(xyz)` (nullifies it) unless
   the compiler advertises itself as GCC/clang.  mcc understands __attribute__
   and must not pretend to be gcc (that pulls in gcc-only header paths it can't
   parse), so instead we simply undo the nullification here: user attributes
   (packed, mode, transparent_union, weak, alias, ...) then survive any system
   header include, while glibc's own declarations -- which are written to work
   fine without their attributes -- are unaffected. */
#ifdef __attribute__
# undef __attribute__
#endif
