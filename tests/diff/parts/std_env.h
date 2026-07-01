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
#include <fenv.h>
#include <math.h>
#include <wchar.h>
#include <wctype.h>
#include <uchar.h>
#include <tgmath.h>
#endif
