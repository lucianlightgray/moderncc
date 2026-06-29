#ifndef _COMPAT_STDLIB_H
#define _COMPAT_STDLIB_H
#include <sys/types.h>
#include <limits.h>             /* Darwin pulls INT_MAX in transitively here */
#ifndef NULL
#define NULL ((void *)0)
#endif
#endif
