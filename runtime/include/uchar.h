#ifndef _MCC_UCHAR_H
#define _MCC_UCHAR_H

#if defined __has_include_next && __has_include_next(<uchar.h>)
# include_next <uchar.h>
#endif

#ifndef _UCHAR_H
#define _UCHAR_H 1

#include <stddef.h>

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

#endif
#endif
