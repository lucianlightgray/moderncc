#ifndef _MCC_UCHAR_H
#define _MCC_UCHAR_H

/* C11 7.28 <uchar.h>.
   Hosted: pull the system <uchar.h> (its exact mbstate_t + mb conversion
   runtime). Freestanding (-nostdinc), or when the bundled copy shadows the
   system one on the include_next search path, fall back to the C11 contents
   below. The fallback is guarded by glibc's own _UCHAR_H sentinel: if the
   system header was actually included it defines _UCHAR_H (and the types), so
   the fallback is skipped. char16_t/char32_t come from the compiler's
   __CHAR16_TYPE__/__CHAR32_TYPE__ predefs; the mb conversion functions are
   libc (declared here, resolved at link only if used). */
#if defined __has_include_next && __has_include_next(<uchar.h>)
# include_next <uchar.h>
#endif

#ifndef _UCHAR_H
#define _UCHAR_H 1

#include <stddef.h>             /* size_t */

#ifndef __cplusplus
typedef __CHAR16_TYPE__ char16_t;
typedef __CHAR32_TYPE__ char32_t;
#endif

#ifndef __mbstate_t_defined
#define __mbstate_t_defined 1
typedef struct { unsigned __count; unsigned __value; } mbstate_t;
#endif

size_t mbrtoc16(char16_t *restrict, const char *restrict, size_t, mbstate_t *restrict);
size_t c16rtomb(char *restrict, char16_t, mbstate_t *restrict);
size_t mbrtoc32(char32_t *restrict, const char *restrict, size_t, mbstate_t *restrict);
size_t c32rtomb(char *restrict, char32_t, mbstate_t *restrict);

#endif /* _UCHAR_H */
#endif /* _MCC_UCHAR_H */
