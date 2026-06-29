#ifndef _COMPAT_STRING_H
#define _COMPAT_STRING_H
#include <sys/types.h>
#ifndef NULL
#define NULL ((void *)0)
#endif
/* substrate (provided by the test's freestanding wrap) */
size_t strlen(const char *);
int    strncmp(const char *, const char *, size_t);
int    memcmp(const void *, const void *, size_t);
void  *memchr(const void *, int, size_t);
/* implemented by the vendored Apple sources under test */
size_t strcspn(const char *, const char *);
char  *strpbrk(const char *, const char *);
char  *strsep(char **, const char *);
void  *memmem(const void *, size_t, const void *, size_t);
char  *strchrnul(const char *, int);
char  *strnstr(const char *, const char *, size_t);
#endif
