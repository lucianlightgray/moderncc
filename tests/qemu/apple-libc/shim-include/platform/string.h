/* Compat shim for Apple libplatform's <platform/string.h>. Forces the portable
   C bodies (all _PLATFORM_OPTIMIZED_* = 0, exactly as the upstream header
   defaults off the optimized targets) and emits the public names
   (VARIANT_STATIC). No libplatform source body is modified. */
#ifndef _COMPAT_PLATFORM_STRING_H
#define _COMPAT_PLATFORM_STRING_H
#include <sys/types.h>          /* size_t */
#ifndef NULL
#define NULL ((void *)0)
#endif
#ifndef __restrict
#define __restrict
#endif
#ifndef VARIANT_STATIC
#define VARIANT_STATIC 1        /* emit the public strlen/strcmp/... wrappers */
#endif
#define _PLATFORM_OPTIMIZED_BZERO 0
#define _PLATFORM_OPTIMIZED_MEMCCPY 0
#define _PLATFORM_OPTIMIZED_MEMCHR 0
#define _PLATFORM_OPTIMIZED_MEMCMP 0
#define _PLATFORM_OPTIMIZED_MEMCMP_ZERO_ALIGNED8 0
#define _PLATFORM_OPTIMIZED_MEMMOVE 0
#define _PLATFORM_OPTIMIZED_MEMSET 0
#define _PLATFORM_OPTIMIZED_MEMSET_PATTERN4 0
#define _PLATFORM_OPTIMIZED_MEMSET_PATTERN8 0
#define _PLATFORM_OPTIMIZED_MEMSET_PATTERN16 0
#define _PLATFORM_OPTIMIZED_STRCHR 0
#define _PLATFORM_OPTIMIZED_STRCMP 0
#define _PLATFORM_OPTIMIZED_STRCPY 0
#define _PLATFORM_OPTIMIZED_STRLCAT 0
#define _PLATFORM_OPTIMIZED_STRLCPY 0
#define _PLATFORM_OPTIMIZED_STRLEN 0
#define _PLATFORM_OPTIMIZED_STRNCMP 0
#define _PLATFORM_OPTIMIZED_STRNCPY 0
#define _PLATFORM_OPTIMIZED_STRNLEN 0
#define _PLATFORM_OPTIMIZED_STRSTR 0
/* internal prototypes referenced across the libplatform TUs */
void  *_platform_memmove(void *, const void *, size_t);
void  *_platform_memset(void *, int, size_t);
void   _platform_memset_pattern4(void *, const void *, size_t);
void   _platform_memset_pattern8(void *, const void *, size_t);
void   _platform_memset_pattern16(void *, const void *, size_t);
void  *_platform_memchr(const void *, int, size_t);
int    _platform_memcmp(const void *, const void *, size_t);
void  *_platform_memccpy(void *, const void *, int, size_t);
void   _platform_bzero(void *, size_t);
size_t _platform_strlen(const char *);
size_t _platform_strnlen(const char *, size_t);
int    _platform_strcmp(const char *, const char *);
int    _platform_strncmp(const char *, const char *, size_t);
char  *_platform_strcpy(char *, const char *);
char  *_platform_strncpy(char *, const char *, size_t);
size_t _platform_strlcpy(char *, const char *, size_t);
size_t _platform_strlcat(char *, const char *, size_t);
char  *_platform_strchr(const char *, int);
char  *_platform_strstr(const char *, const char *);
/* public names string_io.c calls (resolved at link to the real libplatform
   code). strchr MUST be prototyped -- it returns a pointer, and an implicit
   `int` return makes the compiler truncate it to 32 bits. */
size_t strlen(const char *);
int    strncmp(const char *, const char *, size_t);
char  *strchr(const char *, int);
void  *memmove(void *, const void *, size_t);
void  *memcpy(void *, const void *, size_t);
#endif
