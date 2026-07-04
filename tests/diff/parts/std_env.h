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

#ifndef CMPLX
#define CMPLX(r, i) __builtin_complex((double)(r), (double)(i))
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

#if __has_include(<uchar.h>) && !defined(__APPLE__)
#include <uchar.h>
#define PARTS_STD_HAVE_UCHAR 1
#endif
#include <tgmath.h>
#endif
