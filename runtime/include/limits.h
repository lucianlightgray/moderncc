#ifndef _MCC_LIMITS_H
#define _MCC_LIMITS_H

#ifndef _GCC_LIMITS_H_
#define _GCC_LIMITS_H_ 1
#endif
#if defined __has_include_next && __has_include_next(<limits.h>)
# include_next <limits.h>
#endif

#ifndef CHAR_BIT
#define CHAR_BIT __CHAR_BIT__
#endif
#ifndef MB_LEN_MAX
#define MB_LEN_MAX 16
#endif

#ifndef SCHAR_MIN
#define SCHAR_MIN (-128)
#endif
#ifndef SCHAR_MAX
#define SCHAR_MAX 127
#endif
#ifndef UCHAR_MAX
#define UCHAR_MAX 255
#endif

#ifndef CHAR_MIN
# if ('\xff' > 0)
#  define CHAR_MIN 0
# else
#  define CHAR_MIN (-128)
# endif
#endif
#ifndef CHAR_MAX
# if ('\xff' > 0)
#  define CHAR_MAX 255
# else
#  define CHAR_MAX 127
# endif
#endif

#ifndef SHRT_MIN
#define SHRT_MIN (-32768)
#endif
#ifndef SHRT_MAX
#define SHRT_MAX 32767
#endif
#ifndef USHRT_MAX
#define USHRT_MAX 65535
#endif

#ifndef INT_MIN
#define INT_MIN (-__INT_MAX__ - 1)
#endif
#ifndef INT_MAX
#define INT_MAX __INT_MAX__
#endif
#ifndef UINT_MAX
#define UINT_MAX (__INT_MAX__ * 2U + 1U)
#endif

#ifndef LONG_MIN
#define LONG_MIN (-__LONG_MAX__ - 1L)
#endif
#ifndef LONG_MAX
#define LONG_MAX __LONG_MAX__
#endif
#ifndef ULONG_MAX
#define ULONG_MAX (__LONG_MAX__ * 2UL + 1UL)
#endif

#ifndef LLONG_MIN
#define LLONG_MIN (-__LONG_LONG_MAX__ - 1LL)
#endif
#ifndef LLONG_MAX
#define LLONG_MAX __LONG_LONG_MAX__
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX (__LONG_LONG_MAX__ * 2ULL + 1ULL)
#endif

#endif
