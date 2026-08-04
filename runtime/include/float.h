#ifndef _FLOAT_H_
#define _FLOAT_H_

#define FLT_RADIX 2

#define FLT_MANT_DIG 24
#define FLT_DIG 6
#define FLT_ROUNDS 1
#define FLT_EPSILON 1.19209290e-07F
#define FLT_MIN_EXP (-125)
#define FLT_MIN 1.17549435e-38F
#define FLT_MIN_10_EXP (-37)
#define FLT_MAX_EXP 128
#define FLT_MAX 3.40282347e+38F
#define FLT_MAX_10_EXP 38

#define DBL_MANT_DIG 53
#define DBL_DIG 15
#define DBL_EPSILON 2.2204460492503131e-16
#define DBL_MIN_EXP (-1021)
#define DBL_MIN 2.2250738585072014e-308
#define DBL_MIN_10_EXP (-307)
#define DBL_MAX_EXP 1024
#define DBL_MAX 1.7976931348623157e+308
#define DBL_MAX_10_EXP 308

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#if defined __i386__
#define FLT_EVAL_METHOD 2
#else
#define FLT_EVAL_METHOD 0
#endif
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define FLT_HAS_SUBNORM 1
#define DBL_HAS_SUBNORM 1
#define LDBL_HAS_SUBNORM 1

#define FLT_DECIMAL_DIG 9
#define DBL_DECIMAL_DIG 17

#define FLT_TRUE_MIN 1.40129846e-45F
#define DBL_TRUE_MIN 4.9406564584124654e-324
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define FLT_NORM_MAX FLT_MAX
#define DBL_NORM_MAX DBL_MAX
#define FLT_IS_IEC_60559 1
#define DBL_IS_IEC_60559 1
#define __STDC_VERSION_FLOAT_H__ 202311L
#undef INFINITY
#define INFINITY (__builtin_inff ())
#undef NAN
#define NAN (__builtin_nanf (""))
#undef FLT_SNAN
#define FLT_SNAN (__builtin_nansf (""))
#undef DBL_SNAN
#define DBL_SNAN (__builtin_nans (""))
#undef LDBL_SNAN
#define LDBL_SNAN (__builtin_nansl (""))
#endif

#if defined _WIN32 || (defined __APPLE__ && defined __aarch64__)
#define LDBL_MANT_DIG 53
#define LDBL_DIG 15
#define LDBL_EPSILON 2.2204460492503131e-16L
#define LDBL_MIN_EXP (-1021)
#define LDBL_MIN 2.2250738585072014e-308L
#define LDBL_MIN_10_EXP (-307)
#define LDBL_MAX_EXP 1024
#define LDBL_MAX 1.7976931348623157e+308L
#define LDBL_MAX_10_EXP 308
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define DECIMAL_DIG 17
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define LDBL_DECIMAL_DIG 17
#define LDBL_TRUE_MIN 4.9406564584124654e-324L
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define LDBL_NORM_MAX LDBL_MAX
#define LDBL_IS_IEC_60559 1
#endif

#elif defined __i386__ || defined __x86_64__

#define LDBL_MANT_DIG 64
#define LDBL_DIG 18
#define LDBL_EPSILON 1.08420217248550443401e-19L
#define LDBL_MIN_EXP (-16381)
#define LDBL_MIN 3.36210314311209350626e-4932L
#define LDBL_MIN_10_EXP (-4931)
#define LDBL_MAX_EXP 16384
#define LDBL_MAX 1.18973149535723176502e+4932L
#define LDBL_MAX_10_EXP 4932
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define DECIMAL_DIG 21
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define LDBL_DECIMAL_DIG 21
#define LDBL_TRUE_MIN 3.64519953188247460253e-4951L
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define LDBL_NORM_MAX LDBL_MAX
#define LDBL_IS_IEC_60559 1
#endif

#elif (defined __aarch64__ && !defined __APPLE__) || defined __riscv
#define LDBL_MANT_DIG 113
#define LDBL_DIG 33
#define LDBL_EPSILON 1.92592994438723585305597794258492732e-34L
#define LDBL_MIN_EXP (-16381)
#define LDBL_MIN 3.36210314311209350626267781732175260e-4932L
#define LDBL_MIN_10_EXP (-4931)
#define LDBL_MAX_EXP 16384
#define LDBL_MAX 1.18973149535723176508575932662800702e+4932L
#define LDBL_MAX_10_EXP 4932
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define DECIMAL_DIG 36
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define LDBL_DECIMAL_DIG 36
#define LDBL_TRUE_MIN 6.47517511943802511092443895822764655e-4966L
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define LDBL_NORM_MAX LDBL_MAX
#define LDBL_IS_IEC_60559 1
#endif

#else

#define LDBL_MANT_DIG 53
#define LDBL_DIG 15
#define LDBL_EPSILON 2.2204460492503131e-16L
#define LDBL_MIN_EXP (-1021)
#define LDBL_MIN 2.2250738585072014e-308L
#define LDBL_MIN_10_EXP (-307)
#define LDBL_MAX_EXP 1024
#define LDBL_MAX 1.7976931348623157e+308L
#define LDBL_MAX_10_EXP 308
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define DECIMAL_DIG 17
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define LDBL_DECIMAL_DIG 17
#define LDBL_TRUE_MIN 4.9406564584124654e-324L
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define LDBL_NORM_MAX LDBL_MAX
#define LDBL_IS_IEC_60559 1
#endif

#endif

#if (defined(__STDC_WANT_IEC_60559_BFP_EXT__) || defined(__STDC_WANT_IEC_60559_EXT__)) && defined(DECIMAL_DIG)
#define CR_DECIMAL_DIG (DECIMAL_DIG + 3)
#endif

#endif
