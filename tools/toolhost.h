#ifndef MCC_TOOLHOST_H
#define MCC_TOOLHOST_H

#define MCC_CONFIG_TOOLHOST 1

#ifndef MCC_AMALGAMATED
#define MCC_AMALGAMATED 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>

#define mcc_malloc malloc
#define mcc_mallocz(n) calloc(1, (size_t)(n))
#define mcc_realloc realloc
#define mcc_free free

#include "../src/mcchost.h"

static inline int toup(int c) {
	return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

static inline char *mcc_basename(const char *name) {
	char *p = (char *)strchr(name, 0);
	while (p > name && !HOST_IS_DIRSEP(p[-1]))
		--p;
	return p;
}

#include "../src/mcchost.c"

#endif
