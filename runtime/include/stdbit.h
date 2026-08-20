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
 * SDK. Both the type-generic macros and the C23 per-type named entry points
 * (stdc_<op>_uc/_us/_ui/_ul/_ull) are provided below. */

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

#define stdc_leading_zeros_uc(x) __builtin_stdc_leading_zeros((unsigned char)(x))
#define stdc_leading_zeros_us(x) __builtin_stdc_leading_zeros((unsigned short)(x))
#define stdc_leading_zeros_ui(x) __builtin_stdc_leading_zeros((unsigned int)(x))
#define stdc_leading_zeros_ul(x) __builtin_stdc_leading_zeros((unsigned long)(x))
#define stdc_leading_zeros_ull(x) __builtin_stdc_leading_zeros((unsigned long long)(x))

#define stdc_leading_ones_uc(x) __builtin_stdc_leading_ones((unsigned char)(x))
#define stdc_leading_ones_us(x) __builtin_stdc_leading_ones((unsigned short)(x))
#define stdc_leading_ones_ui(x) __builtin_stdc_leading_ones((unsigned int)(x))
#define stdc_leading_ones_ul(x) __builtin_stdc_leading_ones((unsigned long)(x))
#define stdc_leading_ones_ull(x) __builtin_stdc_leading_ones((unsigned long long)(x))

#define stdc_trailing_zeros_uc(x) __builtin_stdc_trailing_zeros((unsigned char)(x))
#define stdc_trailing_zeros_us(x) __builtin_stdc_trailing_zeros((unsigned short)(x))
#define stdc_trailing_zeros_ui(x) __builtin_stdc_trailing_zeros((unsigned int)(x))
#define stdc_trailing_zeros_ul(x) __builtin_stdc_trailing_zeros((unsigned long)(x))
#define stdc_trailing_zeros_ull(x) __builtin_stdc_trailing_zeros((unsigned long long)(x))

#define stdc_trailing_ones_uc(x) __builtin_stdc_trailing_ones((unsigned char)(x))
#define stdc_trailing_ones_us(x) __builtin_stdc_trailing_ones((unsigned short)(x))
#define stdc_trailing_ones_ui(x) __builtin_stdc_trailing_ones((unsigned int)(x))
#define stdc_trailing_ones_ul(x) __builtin_stdc_trailing_ones((unsigned long)(x))
#define stdc_trailing_ones_ull(x) __builtin_stdc_trailing_ones((unsigned long long)(x))

#define stdc_first_leading_zero_uc(x) __builtin_stdc_first_leading_zero((unsigned char)(x))
#define stdc_first_leading_zero_us(x) __builtin_stdc_first_leading_zero((unsigned short)(x))
#define stdc_first_leading_zero_ui(x) __builtin_stdc_first_leading_zero((unsigned int)(x))
#define stdc_first_leading_zero_ul(x) __builtin_stdc_first_leading_zero((unsigned long)(x))
#define stdc_first_leading_zero_ull(x) __builtin_stdc_first_leading_zero((unsigned long long)(x))

#define stdc_first_leading_one_uc(x) __builtin_stdc_first_leading_one((unsigned char)(x))
#define stdc_first_leading_one_us(x) __builtin_stdc_first_leading_one((unsigned short)(x))
#define stdc_first_leading_one_ui(x) __builtin_stdc_first_leading_one((unsigned int)(x))
#define stdc_first_leading_one_ul(x) __builtin_stdc_first_leading_one((unsigned long)(x))
#define stdc_first_leading_one_ull(x) __builtin_stdc_first_leading_one((unsigned long long)(x))

#define stdc_first_trailing_zero_uc(x) __builtin_stdc_first_trailing_zero((unsigned char)(x))
#define stdc_first_trailing_zero_us(x) __builtin_stdc_first_trailing_zero((unsigned short)(x))
#define stdc_first_trailing_zero_ui(x) __builtin_stdc_first_trailing_zero((unsigned int)(x))
#define stdc_first_trailing_zero_ul(x) __builtin_stdc_first_trailing_zero((unsigned long)(x))
#define stdc_first_trailing_zero_ull(x) __builtin_stdc_first_trailing_zero((unsigned long long)(x))

#define stdc_first_trailing_one_uc(x) __builtin_stdc_first_trailing_one((unsigned char)(x))
#define stdc_first_trailing_one_us(x) __builtin_stdc_first_trailing_one((unsigned short)(x))
#define stdc_first_trailing_one_ui(x) __builtin_stdc_first_trailing_one((unsigned int)(x))
#define stdc_first_trailing_one_ul(x) __builtin_stdc_first_trailing_one((unsigned long)(x))
#define stdc_first_trailing_one_ull(x) __builtin_stdc_first_trailing_one((unsigned long long)(x))

#define stdc_count_zeros_uc(x) __builtin_stdc_count_zeros((unsigned char)(x))
#define stdc_count_zeros_us(x) __builtin_stdc_count_zeros((unsigned short)(x))
#define stdc_count_zeros_ui(x) __builtin_stdc_count_zeros((unsigned int)(x))
#define stdc_count_zeros_ul(x) __builtin_stdc_count_zeros((unsigned long)(x))
#define stdc_count_zeros_ull(x) __builtin_stdc_count_zeros((unsigned long long)(x))

#define stdc_count_ones_uc(x) __builtin_stdc_count_ones((unsigned char)(x))
#define stdc_count_ones_us(x) __builtin_stdc_count_ones((unsigned short)(x))
#define stdc_count_ones_ui(x) __builtin_stdc_count_ones((unsigned int)(x))
#define stdc_count_ones_ul(x) __builtin_stdc_count_ones((unsigned long)(x))
#define stdc_count_ones_ull(x) __builtin_stdc_count_ones((unsigned long long)(x))

#define stdc_has_single_bit_uc(x) __builtin_stdc_has_single_bit((unsigned char)(x))
#define stdc_has_single_bit_us(x) __builtin_stdc_has_single_bit((unsigned short)(x))
#define stdc_has_single_bit_ui(x) __builtin_stdc_has_single_bit((unsigned int)(x))
#define stdc_has_single_bit_ul(x) __builtin_stdc_has_single_bit((unsigned long)(x))
#define stdc_has_single_bit_ull(x) __builtin_stdc_has_single_bit((unsigned long long)(x))

#define stdc_bit_width_uc(x) __builtin_stdc_bit_width((unsigned char)(x))
#define stdc_bit_width_us(x) __builtin_stdc_bit_width((unsigned short)(x))
#define stdc_bit_width_ui(x) __builtin_stdc_bit_width((unsigned int)(x))
#define stdc_bit_width_ul(x) __builtin_stdc_bit_width((unsigned long)(x))
#define stdc_bit_width_ull(x) __builtin_stdc_bit_width((unsigned long long)(x))

#define stdc_bit_floor_uc(x) __builtin_stdc_bit_floor((unsigned char)(x))
#define stdc_bit_floor_us(x) __builtin_stdc_bit_floor((unsigned short)(x))
#define stdc_bit_floor_ui(x) __builtin_stdc_bit_floor((unsigned int)(x))
#define stdc_bit_floor_ul(x) __builtin_stdc_bit_floor((unsigned long)(x))
#define stdc_bit_floor_ull(x) __builtin_stdc_bit_floor((unsigned long long)(x))

#define stdc_bit_ceil_uc(x) __builtin_stdc_bit_ceil((unsigned char)(x))
#define stdc_bit_ceil_us(x) __builtin_stdc_bit_ceil((unsigned short)(x))
#define stdc_bit_ceil_ui(x) __builtin_stdc_bit_ceil((unsigned int)(x))
#define stdc_bit_ceil_ul(x) __builtin_stdc_bit_ceil((unsigned long)(x))
#define stdc_bit_ceil_ull(x) __builtin_stdc_bit_ceil((unsigned long long)(x))

#endif
