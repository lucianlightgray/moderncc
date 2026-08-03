#ifndef _MCC_STDINT_H
#define _MCC_STDINT_H

#if defined __has_include_next && __has_include_next(<stdint.h>)
#include_next <stdint.h>
#endif

#ifndef INT8_MAX

typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef __INT64_TYPE__ int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __INT64_TYPE__ uint64_t;

typedef int8_t int_least8_t;
typedef int16_t int_least16_t;
typedef int32_t int_least32_t;
typedef int64_t int_least64_t;
typedef uint8_t uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

typedef int8_t int_fast8_t;
typedef int16_t int_fast16_t;
typedef int32_t int_fast32_t;
typedef int64_t int_fast64_t;
typedef uint8_t uint_fast8_t;
typedef uint16_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef long long intmax_t;
typedef unsigned long long uintmax_t;

#define INT8_MIN (-128)
#define INT16_MIN (-32768)
#define INT32_MIN (-2147483647 - 1)
#define INT64_MIN (-__LONG_LONG_MAX__ - 1)
#define INT8_MAX 127
#define INT16_MAX 32767
#define INT32_MAX 2147483647
#define INT64_MAX __LONG_LONG_MAX__
#define UINT8_MAX 255
#define UINT16_MAX 65535
#define UINT32_MAX 4294967295U
#define UINT64_MAX (__LONG_LONG_MAX__ * 2ULL + 1ULL)

#define INT_LEAST8_MIN INT8_MIN
#define INT_LEAST16_MIN INT16_MIN
#define INT_LEAST32_MIN INT32_MIN
#define INT_LEAST64_MIN INT64_MIN
#define INT_LEAST8_MAX INT8_MAX
#define INT_LEAST16_MAX INT16_MAX
#define INT_LEAST32_MAX INT32_MAX
#define INT_LEAST64_MAX INT64_MAX
#define UINT_LEAST8_MAX UINT8_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#define UINT_LEAST64_MAX UINT64_MAX

#define INT_FAST8_MIN INT8_MIN
#define INT_FAST16_MIN INT16_MIN
#define INT_FAST32_MIN INT32_MIN
#define INT_FAST64_MIN INT64_MIN
#define INT_FAST8_MAX INT8_MAX
#define INT_FAST16_MAX INT16_MAX
#define INT_FAST32_MAX INT32_MAX
#define INT_FAST64_MAX INT64_MAX
#define UINT_FAST8_MAX UINT8_MAX
#define UINT_FAST16_MAX UINT16_MAX
#define UINT_FAST32_MAX UINT32_MAX
#define UINT_FAST64_MAX UINT64_MAX

#if __SIZEOF_POINTER__ == 8
#define INTPTR_MIN INT64_MIN
#define INTPTR_MAX INT64_MAX
#define UINTPTR_MAX UINT64_MAX
#else
#define INTPTR_MIN INT32_MIN
#define INTPTR_MAX INT32_MAX
#define UINTPTR_MAX UINT32_MAX
#endif

#define INTMAX_MIN INT64_MIN
#define INTMAX_MAX INT64_MAX
#define UINTMAX_MAX UINT64_MAX

#define PTRDIFF_MIN INTPTR_MIN
#define PTRDIFF_MAX INTPTR_MAX
#define SIZE_MAX UINTPTR_MAX
#define SIG_ATOMIC_MIN INT32_MIN
#define SIG_ATOMIC_MAX INT32_MAX
#ifndef WCHAR_MIN
#define WCHAR_MIN INT32_MIN
#endif
#ifndef WCHAR_MAX
#define WCHAR_MAX INT32_MAX
#endif
#ifndef WINT_MIN
#define WINT_MIN 0
#endif
#ifndef WINT_MAX
#define WINT_MAX UINT32_MAX
#endif

#define INT8_C(x) x
#define INT16_C(x) x
#define INT32_C(x) x
#define UINT8_C(x) x
#define UINT16_C(x) x
#define UINT32_C(x) x##U
#if __SIZEOF_LONG__ == 8 && defined __linux__
#define INT64_C(x) x##L
#define UINT64_C(x) x##UL
#else
#define INT64_C(x) x##LL
#define UINT64_C(x) x##ULL
#endif
#define INTMAX_C(x) x##LL
#define UINTMAX_C(x) x##ULL

#endif
#endif
