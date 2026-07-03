/*
 *  toolhost.h - the single amalgamation unit for the tool host layer
 *
 *  Exactly ONE tool translation unit (tools/toolsupport.c) includes this
 *  header; it pulls in the compiler's host-abstraction layer (src/mcchost.c)
 *  with EXTERNAL linkage so the spawn/path/filesystem primitives are defined
 *  once and linked into every tool.  Other tools include only toolsupport.h
 *  (declarations) and link against toolsupport.o.  The amalgamation mirrors
 *  runtime/lib/bt-exe.c: set the mode, supply a minimal libc prelude, then
 *  #include the .c.
 *
 *  Keeping the host branches (POSIX vs Windows) solely in mcchost.{h,c} means
 *  the host-gate invariant holds for tools too: no raw _WIN32/__APPLE__/...
 *  tests belong in tools/, only host_* calls and the MCC_HOST_* predicates.
 */
#ifndef MCC_TOOLHOST_H
#define MCC_TOOLHOST_H

#define CONFIG_MCC_TOOLHOST 1
/* external linkage: host_* defined once here, referenced from other tool TUs */
#ifndef ONE_SOURCE
# define ONE_SOURCE 0
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

/* the compiler's memory helpers map to plain libc for standalone tools */
#define mcc_malloc     malloc
#define mcc_mallocz(n) calloc(1, (size_t)(n))
#define mcc_realloc    realloc
#define mcc_free       free

/* two compiler helpers mcchost.c references (its Windows branches): the
   compiler TU gets them from mcc.h/libmcc.c; tools define them here once. */
#include "../src/mcchost.h"
static inline int toup(int c)
{
    return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}
static inline char *mcc_basename(const char *name)
{
    char *p = (char *)strchr(name, 0);
    while (p > name && !HOST_IS_DIRSEP(p[-1]))
        --p;
    return p;
}

#include "../src/mcchost.c"

#endif /* MCC_TOOLHOST_H */
