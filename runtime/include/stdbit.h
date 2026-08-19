#ifndef _STDBIT_H
#define _STDBIT_H

#define __STDC_VERSION_STDBIT_H__ 202311L

/* T-mac-30078 / T-mac-30230: a thin C23 <stdbit.h> that maps the type-generic
 * stdc_* bit-manipulation macros onto mcc's existing __builtin_stdc_* builtins
 * (defined in runtime/include/mccdefs.h). Those builtins are already generic
 * over any unsigned integer type (unsigned char/short/int/long/long long, and
 * unsigned _BitInt) via __typeof__ + the compiler's precision intrinsics, so
 * the standard type-generic entry points are one-line delegations.
 *
 * This exceeds the local reference toolchains, which ship no <stdbit.h> on this
 * SDK. The per-type named functions (stdc_leading_zeros_ui, ...) are not yet
 * provided; use the type-generic macros below. */

/* Endianness (C23 7.18.2): __STDC_ENDIAN_NATIVE__ equals __STDC_ENDIAN_LITTLE__
 * or __STDC_ENDIAN_BIG__ on this target. */
#define __STDC_ENDIAN_LITTLE__ __ORDER_LITTLE_ENDIAN__
#define __STDC_ENDIAN_BIG__    __ORDER_BIG_ENDIAN__
#define __STDC_ENDIAN_NATIVE__ __BYTE_ORDER__

/* Type-generic bit utilities (C23 7.18.3 - 7.18.16). */
#define stdc_leading_zeros(x)       __builtin_stdc_leading_zeros(x)
#define stdc_leading_ones(x)        __builtin_stdc_leading_ones(x)
#define stdc_trailing_zeros(x)      __builtin_stdc_trailing_zeros(x)
#define stdc_trailing_ones(x)       __builtin_stdc_trailing_ones(x)
#define stdc_first_leading_zero(x)  __builtin_stdc_first_leading_zero(x)
#define stdc_first_leading_one(x)   __builtin_stdc_first_leading_one(x)
#define stdc_first_trailing_zero(x) __builtin_stdc_first_trailing_zero(x)
#define stdc_first_trailing_one(x)  __builtin_stdc_first_trailing_one(x)
#define stdc_count_zeros(x)         __builtin_stdc_count_zeros(x)
#define stdc_count_ones(x)          __builtin_stdc_count_ones(x)
#define stdc_has_single_bit(x)      __builtin_stdc_has_single_bit(x)
#define stdc_bit_width(x)           __builtin_stdc_bit_width(x)
#define stdc_bit_floor(x)           __builtin_stdc_bit_floor(x)
#define stdc_bit_ceil(x)            __builtin_stdc_bit_ceil(x)

#endif
