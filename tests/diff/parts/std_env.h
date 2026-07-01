/* Standard-library environment for standalone parts-suite wrappers.
   Real system headers (no mcclib.h), so wrappers compile cleanly under gcc,
   clang and mcc alike. */
#ifndef PARTS_STD_ENV_H
#define PARTS_STD_ENV_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <float.h>
#include <iso646.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <setjmp.h>
#include <signal.h>
#include <stdalign.h>
#include <stdnoreturn.h>
#include <time.h>
#include <complex.h>
/* Apple's <complex.h> defines the C11 CMPLX macros only for __clang__ (its
   comment blames the gcc-4.2 frontend), so the parts-suite's gcc reference on
   macOS lacks them. Modern gcc implements __builtin_complex; provide the
   macros where the system header omits them so all three compilers agree. */
#ifndef CMPLX
#define CMPLX(r, i)  __builtin_complex((double)(r), (double)(i))
#endif
#ifndef CMPLXF
#define CMPLXF(r, i) __builtin_complex((float)(r), (float)(i))
#endif
#ifndef CMPLXL
#define CMPLXL(r, i) __builtin_complex((long double)(r), (long double)(i))
#endif
#include <fenv.h>
#include <math.h>
#include <wchar.h>
#include <wctype.h>
/* Apple ships no system <uchar.h>. mcc bundles its own, so __has_include alone
   would be true under mcc but false under the gcc/clang references, desyncing
   the parts-suite 3-way diff. Gate on the *system* having it (all three agree
   per-platform) and expose PARTS_STD_HAVE_UCHAR to guard the C11 uchar unit.
   uchar stays fully covered on Linux (native + qemu). */
#if __has_include(<uchar.h>) && !defined(__APPLE__)
#include <uchar.h>
#define PARTS_STD_HAVE_UCHAR 1
#endif
#include <tgmath.h>
#endif
