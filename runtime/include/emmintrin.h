#ifndef _EMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "emmintrin.h is only supported on x86 targets"
#endif

#include <xmmintrin.h>

typedef double __m128d __attribute__((__vector_size__(16), __aligned__(16)));
typedef long long __m128i __attribute__((__vector_size__(16), __aligned__(16)));
typedef double __m128d_u __attribute__((__vector_size__(16), __aligned__(1)));
typedef long long __m128i_u __attribute__((__vector_size__(16), __aligned__(1)));

#define __MCC_SSE2_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

struct __mcc_loadu_si128 {
	__v2di __v;
} __attribute__((__packed__, __may_alias__));

struct __mcc_loadu_pd {
	__v2df __v;
} __attribute__((__packed__, __may_alias__));

struct __mcc_loadu_f64 {
	double __v;
} __attribute__((__packed__, __may_alias__));

struct __mcc_loadu_i32 {
	int __v;
} __attribute__((__packed__, __may_alias__));

__MCC_SSE2_INLINE double __mcc_sqrtsd(double __x)
{
	double __r;
	__asm__("sqrtsd %1, %0" : "=x"(__r) : "x"(__x));
	return __r;
}

__MCC_SSE2_INLINE double __mcc_rint(double __x)
{
	double __m = 4503599627370496.0;
	if (!(__x > -__m && __x < __m))
		return __x;
	if (__x < 0.0)
		return -((-__x + __m) - __m);
	return (__x + __m) - __m;
}

__MCC_SSE2_INLINE int __mcc_cvt_f64_i32(double __x)
{
	double __r = __mcc_rint(__x);
	if (!(__r >= -2147483648.0 && __r <= 2147483647.0))
		return (-2147483647 - 1);
	return (int)__r;
}

__MCC_SSE2_INLINE int __mcc_cvtt_f64_i32(double __x)
{
	if (!(__x > -2147483649.0 && __x < 2147483648.0))
		return (-2147483647 - 1);
	return (int)__x;
}

__MCC_SSE2_INLINE long long __mcc_cvt_f64_i64(double __x)
{
	double __r = __mcc_rint(__x);
	if (!(__r >= -9223372036854775808.0 && __r < 9223372036854775808.0))
		return (-9223372036854775807LL - 1);
	return (long long)__r;
}

__MCC_SSE2_INLINE long long __mcc_cvtt_f64_i64(double __x)
{
	if (!(__x >= -9223372036854775808.0 && __x < 9223372036854775808.0))
		return (-9223372036854775807LL - 1);
	return (long long)__x;
}

__MCC_SSE2_INLINE __m128d _mm_setzero_pd(void)
{
	return (__m128d)(__v2df){0.0, 0.0};
}

__MCC_SSE2_INLINE __m128d _mm_set_sd(double __w)
{
	return (__m128d)(__v2df){__w, 0.0};
}

__MCC_SSE2_INLINE __m128d _mm_set1_pd(double __w)
{
	return (__m128d)(__v2df){__w, __w};
}

__MCC_SSE2_INLINE __m128d _mm_set_pd1(double __w)
{
	return _mm_set1_pd(__w);
}

__MCC_SSE2_INLINE __m128d _mm_set_pd(double __x, double __w)
{
	return (__m128d)(__v2df){__w, __x};
}

__MCC_SSE2_INLINE __m128d _mm_setr_pd(double __w, double __x)
{
	return (__m128d)(__v2df){__w, __x};
}

__MCC_SSE2_INLINE __m128d _mm_undefined_pd(void)
{
	return (__m128d)(__v2df){0.0, 0.0};
}

__MCC_SSE2_INLINE __m128i _mm_undefined_si128(void)
{
	return (__m128i)(__v2di){0, 0};
}

__MCC_SSE2_INLINE __m128d _mm_load_pd(const double *__p)
{
	return *(const __m128d *)__p;
}

__MCC_SSE2_INLINE __m128d _mm_loadu_pd(const double *__p)
{
	return (__m128d)((const struct __mcc_loadu_pd *)__p)->__v;
}

__MCC_SSE2_INLINE __m128d _mm_load_sd(const double *__p)
{
	return (__m128d)(__v2df){((const struct __mcc_loadu_f64 *)__p)->__v, 0.0};
}

__MCC_SSE2_INLINE __m128d _mm_load1_pd(const double *__p)
{
	double __v = ((const struct __mcc_loadu_f64 *)__p)->__v;
	return (__m128d)(__v2df){__v, __v};
}

__MCC_SSE2_INLINE __m128d _mm_load_pd1(const double *__p)
{
	return _mm_load1_pd(__p);
}

__MCC_SSE2_INLINE __m128d _mm_loadr_pd(const double *__p)
{
	__v2df __v = (__v2df)*(const __m128d *)__p;
	return (__m128d)__builtin_shufflevector(__v, __v, 1, 0);
}

__MCC_SSE2_INLINE __m128d _mm_loadh_pd(__m128d __a, const double *__p)
{
	__v2df __r = (__v2df)__a;
	__r[1] = ((const struct __mcc_loadu_f64 *)__p)->__v;
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_loadl_pd(__m128d __a, const double *__p)
{
	__v2df __r = (__v2df)__a;
	__r[0] = ((const struct __mcc_loadu_f64 *)__p)->__v;
	return (__m128d)__r;
}

__MCC_SSE2_INLINE void _mm_store_pd(double *__p, __m128d __a)
{
	*(__m128d *)__p = __a;
}

__MCC_SSE2_INLINE void _mm_storeu_pd(double *__p, __m128d __a)
{
	((struct __mcc_loadu_pd *)__p)->__v = (__v2df)__a;
}

__MCC_SSE2_INLINE void _mm_store_sd(double *__p, __m128d __a)
{
	((struct __mcc_loadu_f64 *)__p)->__v = ((__v2df)__a)[0];
}

__MCC_SSE2_INLINE void _mm_store1_pd(double *__p, __m128d __a)
{
	__v2df __v = (__v2df)__a;
	*(__m128d *)__p = (__m128d)__builtin_shufflevector(__v, __v, 0, 0);
}

__MCC_SSE2_INLINE void _mm_store_pd1(double *__p, __m128d __a)
{
	_mm_store1_pd(__p, __a);
}

__MCC_SSE2_INLINE void _mm_storer_pd(double *__p, __m128d __a)
{
	__v2df __v = (__v2df)__a;
	*(__m128d *)__p = (__m128d)__builtin_shufflevector(__v, __v, 1, 0);
}

__MCC_SSE2_INLINE void _mm_storeh_pd(double *__p, __m128d __a)
{
	((struct __mcc_loadu_f64 *)__p)->__v = ((__v2df)__a)[1];
}

__MCC_SSE2_INLINE void _mm_storel_pd(double *__p, __m128d __a)
{
	((struct __mcc_loadu_f64 *)__p)->__v = ((__v2df)__a)[0];
}

__MCC_SSE2_INLINE void _mm_stream_pd(double *__p, __m128d __a)
{
	*(__m128d *)__p = __a;
}

__MCC_SSE2_INLINE __m128d _mm_add_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2df)__a + (__v2df)__b);
}

__MCC_SSE2_INLINE __m128d _mm_add_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)__a;
	__r[0] = __r[0] + ((__v2df)__b)[0];
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_sub_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2df)__a - (__v2df)__b);
}

__MCC_SSE2_INLINE __m128d _mm_sub_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)__a;
	__r[0] = __r[0] - ((__v2df)__b)[0];
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_mul_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2df)__a * (__v2df)__b);
}

__MCC_SSE2_INLINE __m128d _mm_mul_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)__a;
	__r[0] = __r[0] * ((__v2df)__b)[0];
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_div_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2df)__a / (__v2df)__b);
}

__MCC_SSE2_INLINE __m128d _mm_div_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)__a;
	__r[0] = __r[0] / ((__v2df)__b)[0];
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_sqrt_pd(__m128d __a)
{
	__v2df __x = (__v2df)__a, __r;
	__r[0] = __mcc_sqrtsd(__x[0]);
	__r[1] = __mcc_sqrtsd(__x[1]);
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_sqrt_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)__a;
	__r[0] = __mcc_sqrtsd(((__v2df)__b)[0]);
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_min_pd(__m128d __a, __m128d __b)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __r;
	__r[0] = __x[0] < __y[0] ? __x[0] : __y[0];
	__r[1] = __x[1] < __y[1] ? __x[1] : __y[1];
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_min_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)__a;
	double __y = ((__v2df)__b)[0];
	__r[0] = __r[0] < __y ? __r[0] : __y;
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_max_pd(__m128d __a, __m128d __b)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __r;
	__r[0] = __x[0] > __y[0] ? __x[0] : __y[0];
	__r[1] = __x[1] > __y[1] ? __x[1] : __y[1];
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_max_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)__a;
	double __y = ((__v2df)__b)[0];
	__r[0] = __r[0] > __y ? __r[0] : __y;
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_and_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2du)__a & (__v2du)__b);
}

__MCC_SSE2_INLINE __m128d _mm_andnot_pd(__m128d __a, __m128d __b)
{
	return (__m128d)(~(__v2du)__a & (__v2du)__b);
}

__MCC_SSE2_INLINE __m128d _mm_or_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2du)__a | (__v2du)__b);
}

__MCC_SSE2_INLINE __m128d _mm_xor_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2du)__a ^ (__v2du)__b);
}

__MCC_SSE2_INLINE __m128d _mm_cmpeq_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2di)((__v2df)__a == (__v2df)__b));
}

__MCC_SSE2_INLINE __m128d _mm_cmplt_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2di)((__v2df)__a < (__v2df)__b));
}

__MCC_SSE2_INLINE __m128d _mm_cmple_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2di)((__v2df)__a <= (__v2df)__b));
}

__MCC_SSE2_INLINE __m128d _mm_cmpgt_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2di)((__v2df)__a > (__v2df)__b));
}

__MCC_SSE2_INLINE __m128d _mm_cmpge_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2di)((__v2df)__a >= (__v2df)__b));
}

__MCC_SSE2_INLINE __m128d _mm_cmpneq_pd(__m128d __a, __m128d __b)
{
	return (__m128d)((__v2di)((__v2df)__a != (__v2df)__b));
}

__MCC_SSE2_INLINE __m128d _mm_cmpnlt_pd(__m128d __a, __m128d __b)
{
	return (__m128d)(~(__v2di)((__v2df)__a < (__v2df)__b));
}

__MCC_SSE2_INLINE __m128d _mm_cmpnle_pd(__m128d __a, __m128d __b)
{
	return (__m128d)(~(__v2di)((__v2df)__a <= (__v2df)__b));
}

__MCC_SSE2_INLINE __m128d _mm_cmpngt_pd(__m128d __a, __m128d __b)
{
	return (__m128d)(~(__v2di)((__v2df)__a > (__v2df)__b));
}

__MCC_SSE2_INLINE __m128d _mm_cmpnge_pd(__m128d __a, __m128d __b)
{
	return (__m128d)(~(__v2di)((__v2df)__a >= (__v2df)__b));
}

__MCC_SSE2_INLINE __m128d _mm_cmpord_pd(__m128d __a, __m128d __b)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b;
	return (__m128d)((__v2di)(__x == __x) & (__v2di)(__y == __y));
}

__MCC_SSE2_INLINE __m128d _mm_cmpunord_pd(__m128d __a, __m128d __b)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b;
	return (__m128d)((__v2di)(__x != __x) | (__v2di)(__y != __y));
}

__MCC_SSE2_INLINE __m128d __mcc_setlow_pd(__m128d __a, long long __m)
{
	union {
		long long __i;
		double __f;
	} __c;
	__v2df __r = (__v2df)__a;
	__c.__i = __m;
	__r[0] = __c.__f;
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_cmpeq_sd(__m128d __a, __m128d __b)
{
	return __mcc_setlow_pd(__a, ((__v2df)__a)[0] == ((__v2df)__b)[0] ? -1 : 0);
}

__MCC_SSE2_INLINE __m128d _mm_cmplt_sd(__m128d __a, __m128d __b)
{
	return __mcc_setlow_pd(__a, ((__v2df)__a)[0] < ((__v2df)__b)[0] ? -1 : 0);
}

__MCC_SSE2_INLINE __m128d _mm_cmple_sd(__m128d __a, __m128d __b)
{
	return __mcc_setlow_pd(__a, ((__v2df)__a)[0] <= ((__v2df)__b)[0] ? -1 : 0);
}

__MCC_SSE2_INLINE __m128d _mm_cmpneq_sd(__m128d __a, __m128d __b)
{
	return __mcc_setlow_pd(__a, ((__v2df)__a)[0] != ((__v2df)__b)[0] ? -1 : 0);
}

__MCC_SSE2_INLINE __m128d _mm_cmpnlt_sd(__m128d __a, __m128d __b)
{
	return __mcc_setlow_pd(__a, ((__v2df)__a)[0] < ((__v2df)__b)[0] ? 0 : -1);
}

__MCC_SSE2_INLINE __m128d _mm_cmpnle_sd(__m128d __a, __m128d __b)
{
	return __mcc_setlow_pd(__a, ((__v2df)__a)[0] <= ((__v2df)__b)[0] ? 0 : -1);
}

__MCC_SSE2_INLINE __m128d _mm_cmpord_sd(__m128d __a, __m128d __b)
{
	double __x = ((__v2df)__a)[0], __y = ((__v2df)__b)[0];
	return __mcc_setlow_pd(__a, (__x == __x && __y == __y) ? -1 : 0);
}

__MCC_SSE2_INLINE __m128d _mm_cmpunord_sd(__m128d __a, __m128d __b)
{
	double __x = ((__v2df)__a)[0], __y = ((__v2df)__b)[0];
	return __mcc_setlow_pd(__a, (__x != __x || __y != __y) ? -1 : 0);
}

__MCC_SSE2_INLINE __m128d _mm_cmpgt_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)_mm_cmplt_sd(__b, __a);
	return (__m128d)__builtin_shufflevector(__r, (__v2df)__a, 0, 3);
}

__MCC_SSE2_INLINE __m128d _mm_cmpge_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)_mm_cmple_sd(__b, __a);
	return (__m128d)__builtin_shufflevector(__r, (__v2df)__a, 0, 3);
}

__MCC_SSE2_INLINE __m128d _mm_cmpngt_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)_mm_cmpnlt_sd(__b, __a);
	return (__m128d)__builtin_shufflevector(__r, (__v2df)__a, 0, 3);
}

__MCC_SSE2_INLINE __m128d _mm_cmpnge_sd(__m128d __a, __m128d __b)
{
	__v2df __r = (__v2df)_mm_cmpnle_sd(__b, __a);
	return (__m128d)__builtin_shufflevector(__r, (__v2df)__a, 0, 3);
}

__MCC_SSE2_INLINE int _mm_comieq_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] == ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_comilt_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] < ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_comile_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] <= ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_comigt_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] > ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_comige_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] >= ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_comineq_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] != ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_ucomieq_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] == ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_ucomilt_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] < ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_ucomile_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] <= ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_ucomigt_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] > ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_ucomige_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] >= ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE int _mm_ucomineq_sd(__m128d __a, __m128d __b)
{
	return ((__v2df)__a)[0] != ((__v2df)__b)[0];
}

__MCC_SSE2_INLINE __m128 _mm_cvtpd_ps(__m128d __a)
{
	__v2df __x = (__v2df)__a;
	__v4sf __r;
	__r[0] = (float)__x[0];
	__r[1] = (float)__x[1];
	__r[2] = 0.0f;
	__r[3] = 0.0f;
	return (__m128)__r;
}

__MCC_SSE2_INLINE __m128d _mm_cvtps_pd(__m128 __a)
{
	__v4sf __x = (__v4sf)__a;
	__v2df __r;
	__r[0] = (double)__x[0];
	__r[1] = (double)__x[1];
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_cvtepi32_pd(__m128i __a)
{
	__v4si __x = (__v4si)__a;
	__v2df __r;
	__r[0] = (double)__x[0];
	__r[1] = (double)__x[1];
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128 _mm_cvtepi32_ps(__m128i __a)
{
	__v4si __x = (__v4si)__a;
	__v4sf __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (float)__x[__i];
	return (__m128)__r;
}

__MCC_SSE2_INLINE __m128i _mm_cvtpd_epi32(__m128d __a)
{
	__v2df __x = (__v2df)__a;
	__v4si __r;
	__r[0] = __mcc_cvt_f64_i32(__x[0]);
	__r[1] = __mcc_cvt_f64_i32(__x[1]);
	__r[2] = 0;
	__r[3] = 0;
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_cvttpd_epi32(__m128d __a)
{
	__v2df __x = (__v2df)__a;
	__v4si __r;
	__r[0] = __mcc_cvtt_f64_i32(__x[0]);
	__r[1] = __mcc_cvtt_f64_i32(__x[1]);
	__r[2] = 0;
	__r[3] = 0;
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_cvtps_epi32(__m128 __a)
{
	__v4sf __x = (__v4sf)__a;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_cvt_f32_i32(__x[__i]);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_cvttps_epi32(__m128 __a)
{
	__v4sf __x = (__v4sf)__a;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_cvtt_f32_i32(__x[__i]);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE int _mm_cvtsd_si32(__m128d __a)
{
	return __mcc_cvt_f64_i32(((__v2df)__a)[0]);
}

__MCC_SSE2_INLINE int _mm_cvttsd_si32(__m128d __a)
{
	return __mcc_cvtt_f64_i32(((__v2df)__a)[0]);
}

#ifdef __x86_64__
__MCC_SSE2_INLINE long long _mm_cvtsd_si64(__m128d __a)
{
	return __mcc_cvt_f64_i64(((__v2df)__a)[0]);
}

__MCC_SSE2_INLINE long long _mm_cvtsd_si64x(__m128d __a)
{
	return _mm_cvtsd_si64(__a);
}

__MCC_SSE2_INLINE long long _mm_cvttsd_si64(__m128d __a)
{
	return __mcc_cvtt_f64_i64(((__v2df)__a)[0]);
}

__MCC_SSE2_INLINE long long _mm_cvttsd_si64x(__m128d __a)
{
	return _mm_cvttsd_si64(__a);
}
#endif

__MCC_SSE2_INLINE __m128 _mm_cvtsd_ss(__m128 __a, __m128d __b)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = (float)((__v2df)__b)[0];
	return (__m128)__r;
}

__MCC_SSE2_INLINE __m128d _mm_cvtss_sd(__m128d __a, __m128 __b)
{
	__v2df __r = (__v2df)__a;
	__r[0] = (double)((__v4sf)__b)[0];
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_cvtsi32_sd(__m128d __a, int __b)
{
	__v2df __r = (__v2df)__a;
	__r[0] = (double)__b;
	return (__m128d)__r;
}

#ifdef __x86_64__
__MCC_SSE2_INLINE __m128d _mm_cvtsi64_sd(__m128d __a, long long __b)
{
	__v2df __r = (__v2df)__a;
	__r[0] = (double)__b;
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_cvtsi64x_sd(__m128d __a, long long __b)
{
	return _mm_cvtsi64_sd(__a, __b);
}
#endif

__MCC_SSE2_INLINE double _mm_cvtsd_f64(__m128d __a)
{
	return ((__v2df)__a)[0];
}

__MCC_SSE2_INLINE __m64 _mm_cvtpd_pi32(__m128d __a)
{
	__v2df __x = (__v2df)__a;
	__v2si __r;
	__r[0] = __mcc_cvt_f64_i32(__x[0]);
	__r[1] = __mcc_cvt_f64_i32(__x[1]);
	return (__m64)__r;
}

__MCC_SSE2_INLINE __m64 _mm_cvttpd_pi32(__m128d __a)
{
	__v2df __x = (__v2df)__a;
	__v2si __r;
	__r[0] = __mcc_cvtt_f64_i32(__x[0]);
	__r[1] = __mcc_cvtt_f64_i32(__x[1]);
	return (__m64)__r;
}

__MCC_SSE2_INLINE __m128d _mm_cvtpi32_pd(__m64 __a)
{
	__v2si __x = (__v2si)__a;
	__v2df __r;
	__r[0] = (double)__x[0];
	__r[1] = (double)__x[1];
	return (__m128d)__r;
}

__MCC_SSE2_INLINE __m128d _mm_unpackhi_pd(__m128d __a, __m128d __b)
{
	return (__m128d)__builtin_shufflevector((__v2df)__a, (__v2df)__b, 1, 3);
}

__MCC_SSE2_INLINE __m128d _mm_unpacklo_pd(__m128d __a, __m128d __b)
{
	return (__m128d)__builtin_shufflevector((__v2df)__a, (__v2df)__b, 0, 2);
}

__MCC_SSE2_INLINE __m128d _mm_move_sd(__m128d __a, __m128d __b)
{
	return (__m128d)__builtin_shufflevector((__v2df)__a, (__v2df)__b, 2, 1);
}

__MCC_SSE2_INLINE int _mm_movemask_pd(__m128d __a)
{
	__v2du __x = (__v2du)__a;
	return (int)(((__x[0] >> 63) & 1) | (((__x[1] >> 63) & 1) << 1));
}

#define _mm_shuffle_pd(a, b, mask)                                            \
	((__m128d)__builtin_shufflevector((__v2df)(__m128d)(a), (__v2df)(__m128d)(b), \
			(int)((mask) & 1), (int)((((mask) >> 1) & 1) + 2)))

__MCC_SSE2_INLINE __m128d _mm_castps_pd(__m128 __a)
{
	return (__m128d)__a;
}

__MCC_SSE2_INLINE __m128i _mm_castps_si128(__m128 __a)
{
	return (__m128i)__a;
}

__MCC_SSE2_INLINE __m128 _mm_castpd_ps(__m128d __a)
{
	return (__m128)__a;
}

__MCC_SSE2_INLINE __m128i _mm_castpd_si128(__m128d __a)
{
	return (__m128i)__a;
}

__MCC_SSE2_INLINE __m128 _mm_castsi128_ps(__m128i __a)
{
	return (__m128)__a;
}

__MCC_SSE2_INLINE __m128d _mm_castsi128_pd(__m128i __a)
{
	return (__m128d)__a;
}

__MCC_SSE2_INLINE __m128i _mm_setzero_si128(void)
{
	return (__m128i)(__v2di){0, 0};
}

__MCC_SSE2_INLINE __m128i _mm_set_epi64x(long long __q1, long long __q0)
{
	return (__m128i)(__v2di){__q0, __q1};
}

__MCC_SSE2_INLINE __m128i _mm_set_epi64(__m64 __q1, __m64 __q0)
{
	return (__m128i)(__v2di){_mm_cvtm64_si64(__q0), _mm_cvtm64_si64(__q1)};
}

__MCC_SSE2_INLINE __m128i _mm_set_epi32(int __i3, int __i2, int __i1, int __i0)
{
	return (__m128i)(__v4si){__i0, __i1, __i2, __i3};
}

__MCC_SSE2_INLINE __m128i _mm_set_epi16(short __w7, short __w6, short __w5,
		short __w4, short __w3, short __w2, short __w1, short __w0)
{
	return (__m128i)(__v8hi){__w0, __w1, __w2, __w3, __w4, __w5, __w6, __w7};
}

__MCC_SSE2_INLINE __m128i _mm_set_epi8(char __b15, char __b14, char __b13,
		char __b12, char __b11, char __b10, char __b9, char __b8, char __b7,
		char __b6, char __b5, char __b4, char __b3, char __b2, char __b1, char __b0)
{
	return (__m128i)(__v16qi){__b0, __b1, __b2, __b3, __b4, __b5, __b6, __b7,
		__b8, __b9, __b10, __b11, __b12, __b13, __b14, __b15};
}

__MCC_SSE2_INLINE __m128i _mm_set1_epi64x(long long __q)
{
	return (__m128i)(__v2di){__q, __q};
}

__MCC_SSE2_INLINE __m128i _mm_set1_epi64(__m64 __q)
{
	long long __v = _mm_cvtm64_si64(__q);
	return (__m128i)(__v2di){__v, __v};
}

__MCC_SSE2_INLINE __m128i _mm_set1_epi32(int __i)
{
	return (__m128i)(__v4si){__i, __i, __i, __i};
}

__MCC_SSE2_INLINE __m128i _mm_set1_epi16(short __w)
{
	return (__m128i)(__v8hi){__w, __w, __w, __w, __w, __w, __w, __w};
}

__MCC_SSE2_INLINE __m128i _mm_set1_epi8(char __b)
{
	return (__m128i)(__v16qi){__b, __b, __b, __b, __b, __b, __b, __b,
		__b, __b, __b, __b, __b, __b, __b, __b};
}

__MCC_SSE2_INLINE __m128i _mm_setr_epi64(__m64 __q0, __m64 __q1)
{
	return (__m128i)(__v2di){_mm_cvtm64_si64(__q0), _mm_cvtm64_si64(__q1)};
}

__MCC_SSE2_INLINE __m128i _mm_setr_epi32(int __i0, int __i1, int __i2, int __i3)
{
	return (__m128i)(__v4si){__i0, __i1, __i2, __i3};
}

__MCC_SSE2_INLINE __m128i _mm_setr_epi16(short __w0, short __w1, short __w2,
		short __w3, short __w4, short __w5, short __w6, short __w7)
{
	return (__m128i)(__v8hi){__w0, __w1, __w2, __w3, __w4, __w5, __w6, __w7};
}

__MCC_SSE2_INLINE __m128i _mm_setr_epi8(char __b0, char __b1, char __b2,
		char __b3, char __b4, char __b5, char __b6, char __b7, char __b8,
		char __b9, char __b10, char __b11, char __b12, char __b13, char __b14,
		char __b15)
{
	return (__m128i)(__v16qi){__b0, __b1, __b2, __b3, __b4, __b5, __b6, __b7,
		__b8, __b9, __b10, __b11, __b12, __b13, __b14, __b15};
}

__MCC_SSE2_INLINE __m128i _mm_load_si128(const __m128i *__p)
{
	return *__p;
}

__MCC_SSE2_INLINE __m128i _mm_loadu_si128(const __m128i *__p)
{
	return (__m128i)((const struct __mcc_loadu_si128 *)__p)->__v;
}

__MCC_SSE2_INLINE __m128i _mm_loadl_epi64(const __m128i *__p)
{
	return (__m128i)(__v2di){((const struct __mcc_loadu_si64 *)__p)->__v, 0};
}

__MCC_SSE2_INLINE __m128i _mm_loadu_si64(const void *__p)
{
	return (__m128i)(__v2di){((const struct __mcc_loadu_si64 *)__p)->__v, 0};
}

__MCC_SSE2_INLINE __m128i _mm_loadu_si32(const void *__p)
{
	return (__m128i)(__v4si){((const struct __mcc_loadu_i32 *)__p)->__v, 0, 0, 0};
}

__MCC_SSE2_INLINE void _mm_store_si128(__m128i *__p, __m128i __b)
{
	*__p = __b;
}

__MCC_SSE2_INLINE void _mm_storeu_si128(__m128i *__p, __m128i __b)
{
	((struct __mcc_loadu_si128 *)__p)->__v = (__v2di)__b;
}

__MCC_SSE2_INLINE void _mm_storel_epi64(__m128i *__p, __m128i __b)
{
	((struct __mcc_loadu_si64 *)__p)->__v = ((__v2di)__b)[0];
}

__MCC_SSE2_INLINE void _mm_storeu_si64(void *__p, __m128i __b)
{
	((struct __mcc_loadu_si64 *)__p)->__v = ((__v2di)__b)[0];
}

__MCC_SSE2_INLINE void _mm_storeu_si32(void *__p, __m128i __b)
{
	((struct __mcc_loadu_i32 *)__p)->__v = ((__v4si)__b)[0];
}

__MCC_SSE2_INLINE void _mm_stream_si128(__m128i *__p, __m128i __a)
{
	*__p = __a;
}

__MCC_SSE2_INLINE void _mm_stream_si32(int *__p, int __a)
{
	*__p = __a;
}

#ifdef __x86_64__
__MCC_SSE2_INLINE void _mm_stream_si64(long long *__p, long long __a)
{
	*__p = __a;
}
#endif

__MCC_SSE2_INLINE int _mm_cvtsi128_si32(__m128i __a)
{
	return ((__v4si)__a)[0];
}

__MCC_SSE2_INLINE __m128i _mm_cvtsi32_si128(int __a)
{
	return (__m128i)(__v4si){__a, 0, 0, 0};
}

#ifdef __x86_64__
__MCC_SSE2_INLINE long long _mm_cvtsi128_si64(__m128i __a)
{
	return ((__v2di)__a)[0];
}

__MCC_SSE2_INLINE long long _mm_cvtsi128_si64x(__m128i __a)
{
	return _mm_cvtsi128_si64(__a);
}

__MCC_SSE2_INLINE __m128i _mm_cvtsi64_si128(long long __a)
{
	return (__m128i)(__v2di){__a, 0};
}

__MCC_SSE2_INLINE __m128i _mm_cvtsi64x_si128(long long __a)
{
	return _mm_cvtsi64_si128(__a);
}
#endif

__MCC_SSE2_INLINE __m64 _mm_movepi64_pi64(__m128i __a)
{
	return _mm_cvtsi64_m64(((__v2di)__a)[0]);
}

__MCC_SSE2_INLINE __m128i _mm_movpi64_epi64(__m64 __a)
{
	return (__m128i)(__v2di){_mm_cvtm64_si64(__a), 0};
}

__MCC_SSE2_INLINE __m128i _mm_move_epi64(__m128i __a)
{
	return (__m128i)(__v2di){((__v2di)__a)[0], 0};
}

__MCC_SSE2_INLINE __m128i _mm_add_epi8(__m128i __a, __m128i __b)
{
	return (__m128i)((__v16qu)__a + (__v16qu)__b);
}

__MCC_SSE2_INLINE __m128i _mm_add_epi16(__m128i __a, __m128i __b)
{
	return (__m128i)((__v8hu)__a + (__v8hu)__b);
}

__MCC_SSE2_INLINE __m128i _mm_add_epi32(__m128i __a, __m128i __b)
{
	return (__m128i)((__v4su)__a + (__v4su)__b);
}

__MCC_SSE2_INLINE __m128i _mm_add_epi64(__m128i __a, __m128i __b)
{
	return (__m128i)((__v2du)__a + (__v2du)__b);
}

__MCC_SSE2_INLINE __m128i _mm_sub_epi8(__m128i __a, __m128i __b)
{
	return (__m128i)((__v16qu)__a - (__v16qu)__b);
}

__MCC_SSE2_INLINE __m128i _mm_sub_epi16(__m128i __a, __m128i __b)
{
	return (__m128i)((__v8hu)__a - (__v8hu)__b);
}

__MCC_SSE2_INLINE __m128i _mm_sub_epi32(__m128i __a, __m128i __b)
{
	return (__m128i)((__v4su)__a - (__v4su)__b);
}

__MCC_SSE2_INLINE __m128i _mm_sub_epi64(__m128i __a, __m128i __b)
{
	return (__m128i)((__v2du)__a - (__v2du)__b);
}

__MCC_SSE2_INLINE __m128i _mm_adds_epi8(__m128i __a, __m128i __b)
{
	__v16qs __x = (__v16qs)__a, __y = (__v16qs)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (signed char)__mcc_sat_sb((int)__x[__i] + (int)__y[__i]);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_adds_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (short)__mcc_sat_sw((int)__x[__i] + (int)__y[__i]);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_adds_epu8(__m128i __a, __m128i __b)
{
	__v16qu __x = (__v16qu)__a, __y = (__v16qu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (unsigned char)__mcc_sat_ub((int)__x[__i] + (int)__y[__i]);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_adds_epu16(__m128i __a, __m128i __b)
{
	__v8hu __x = (__v8hu)__a, __y = (__v8hu)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (unsigned short)__mcc_sat_uw((int)__x[__i] + (int)__y[__i]);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_subs_epi8(__m128i __a, __m128i __b)
{
	__v16qs __x = (__v16qs)__a, __y = (__v16qs)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (signed char)__mcc_sat_sb((int)__x[__i] - (int)__y[__i]);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_subs_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (short)__mcc_sat_sw((int)__x[__i] - (int)__y[__i]);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_subs_epu8(__m128i __a, __m128i __b)
{
	__v16qu __x = (__v16qu)__a, __y = (__v16qu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (unsigned char)__mcc_sat_ub((int)__x[__i] - (int)__y[__i]);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_subs_epu16(__m128i __a, __m128i __b)
{
	__v8hu __x = (__v8hu)__a, __y = (__v8hu)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (unsigned short)__mcc_sat_uw((int)__x[__i] - (int)__y[__i]);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_madd_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		unsigned int __lo = (unsigned int)((int)__x[2 * __i] * (int)__y[2 * __i]);
		unsigned int __hi = (unsigned int)((int)__x[2 * __i + 1] * (int)__y[2 * __i + 1]);
		__r[__i] = (int)(__lo + __hi);
	}
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_mulhi_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (short)(((int)__x[__i] * (int)__y[__i]) >> 16);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_mulhi_epu16(__m128i __a, __m128i __b)
{
	__v8hu __x = (__v8hu)__a, __y = (__v8hu)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (unsigned short)(((unsigned int)__x[__i] * (unsigned int)__y[__i]) >> 16);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_mullo_epi16(__m128i __a, __m128i __b)
{
	return (__m128i)((__v8hu)__a * (__v8hu)__b);
}

__MCC_SSE2_INLINE __m64 _mm_mul_su32(__m64 __a, __m64 __b)
{
	unsigned long long __x = (unsigned long long)((__v2su)__a)[0];
	unsigned long long __y = (unsigned long long)((__v2su)__b)[0];
	return (__m64)(__v1du){__x * __y};
}

__MCC_SSE2_INLINE __m128i _mm_mul_epu32(__m128i __a, __m128i __b)
{
	__v4su __x = (__v4su)__a, __y = (__v4su)__b;
	__v2du __r;
	__r[0] = (unsigned long long)__x[0] * (unsigned long long)__y[0];
	__r[1] = (unsigned long long)__x[2] * (unsigned long long)__y[2];
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_sad_epu8(__m128i __a, __m128i __b)
{
	__v16qu __x = (__v16qu)__a, __y = (__v16qu)__b;
	__v2du __r;
	int __i, __k;
	for (__k = 0; __k < 2; __k++) {
		unsigned int __s = 0;
		for (__i = 0; __i < 8; __i++) {
			unsigned int __p = __x[__k * 8 + __i], __q = __y[__k * 8 + __i];
			__s += __p > __q ? __p - __q : __q - __p;
		}
		__r[__k] = __s;
	}
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_avg_epu8(__m128i __a, __m128i __b)
{
	__v16qu __x = (__v16qu)__a, __y = (__v16qu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (unsigned char)(((unsigned int)__x[__i] + (unsigned int)__y[__i] + 1) >> 1);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_avg_epu16(__m128i __a, __m128i __b)
{
	__v8hu __x = (__v8hu)__a, __y = (__v8hu)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (unsigned short)(((unsigned int)__x[__i] + (unsigned int)__y[__i] + 1) >> 1);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_max_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_max_epu8(__m128i __a, __m128i __b)
{
	__v16qu __x = (__v16qu)__a, __y = (__v16qu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_min_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_min_epu8(__m128i __a, __m128i __b)
{
	__v16qu __x = (__v16qu)__a, __y = (__v16qu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_and_si128(__m128i __a, __m128i __b)
{
	return (__m128i)((__v2du)__a & (__v2du)__b);
}

__MCC_SSE2_INLINE __m128i _mm_andnot_si128(__m128i __a, __m128i __b)
{
	return (__m128i)(~(__v2du)__a & (__v2du)__b);
}

__MCC_SSE2_INLINE __m128i _mm_or_si128(__m128i __a, __m128i __b)
{
	return (__m128i)((__v2du)__a | (__v2du)__b);
}

__MCC_SSE2_INLINE __m128i _mm_xor_si128(__m128i __a, __m128i __b)
{
	return (__m128i)((__v2du)__a ^ (__v2du)__b);
}

__MCC_SSE2_INLINE __m128i _mm_cmpeq_epi8(__m128i __a, __m128i __b)
{
	return (__m128i)((__v16qi)__a == (__v16qi)__b);
}

__MCC_SSE2_INLINE __m128i _mm_cmpeq_epi16(__m128i __a, __m128i __b)
{
	return (__m128i)((__v8hi)__a == (__v8hi)__b);
}

__MCC_SSE2_INLINE __m128i _mm_cmpeq_epi32(__m128i __a, __m128i __b)
{
	return (__m128i)((__v4si)__a == (__v4si)__b);
}

__MCC_SSE2_INLINE __m128i _mm_cmpgt_epi8(__m128i __a, __m128i __b)
{
	return (__m128i)((__v16qs)__a > (__v16qs)__b);
}

__MCC_SSE2_INLINE __m128i _mm_cmpgt_epi16(__m128i __a, __m128i __b)
{
	return (__m128i)((__v8hi)__a > (__v8hi)__b);
}

__MCC_SSE2_INLINE __m128i _mm_cmpgt_epi32(__m128i __a, __m128i __b)
{
	return (__m128i)((__v4si)__a > (__v4si)__b);
}

__MCC_SSE2_INLINE __m128i _mm_cmplt_epi8(__m128i __a, __m128i __b)
{
	return _mm_cmpgt_epi8(__b, __a);
}

__MCC_SSE2_INLINE __m128i _mm_cmplt_epi16(__m128i __a, __m128i __b)
{
	return _mm_cmpgt_epi16(__b, __a);
}

__MCC_SSE2_INLINE __m128i _mm_cmplt_epi32(__m128i __a, __m128i __b)
{
	return _mm_cmpgt_epi32(__b, __a);
}

__MCC_SSE2_INLINE __m128i _mm_sll_epi16(__m128i __a, __m128i __count)
{
	unsigned long long __c = (unsigned long long)((__v2du)__count)[0];
	__v8hu __x = (__v8hu)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __c > 15 ? 0 : (unsigned short)(__x[__i] << __c);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_slli_epi16(__m128i __a, int __count)
{
	return _mm_sll_epi16(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__count, 0});
}

__MCC_SSE2_INLINE __m128i _mm_sll_epi32(__m128i __a, __m128i __count)
{
	unsigned long long __c = (unsigned long long)((__v2du)__count)[0];
	__v4su __x = (__v4su)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __c > 31 ? 0 : (unsigned int)(__x[__i] << __c);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_slli_epi32(__m128i __a, int __count)
{
	return _mm_sll_epi32(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__count, 0});
}

__MCC_SSE2_INLINE __m128i _mm_sll_epi64(__m128i __a, __m128i __count)
{
	unsigned long long __c = (unsigned long long)((__v2du)__count)[0];
	__v2du __x = (__v2du)__a, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __c > 63 ? 0 : __x[__i] << __c;
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_slli_epi64(__m128i __a, int __count)
{
	return _mm_sll_epi64(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__count, 0});
}

__MCC_SSE2_INLINE __m128i _mm_srl_epi16(__m128i __a, __m128i __count)
{
	unsigned long long __c = (unsigned long long)((__v2du)__count)[0];
	__v8hu __x = (__v8hu)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __c > 15 ? 0 : (unsigned short)(__x[__i] >> __c);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_srli_epi16(__m128i __a, int __count)
{
	return _mm_srl_epi16(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__count, 0});
}

__MCC_SSE2_INLINE __m128i _mm_srl_epi32(__m128i __a, __m128i __count)
{
	unsigned long long __c = (unsigned long long)((__v2du)__count)[0];
	__v4su __x = (__v4su)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __c > 31 ? 0 : (unsigned int)(__x[__i] >> __c);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_srli_epi32(__m128i __a, int __count)
{
	return _mm_srl_epi32(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__count, 0});
}

__MCC_SSE2_INLINE __m128i _mm_srl_epi64(__m128i __a, __m128i __count)
{
	unsigned long long __c = (unsigned long long)((__v2du)__count)[0];
	__v2du __x = (__v2du)__a, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __c > 63 ? 0 : __x[__i] >> __c;
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_srli_epi64(__m128i __a, int __count)
{
	return _mm_srl_epi64(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__count, 0});
}

__MCC_SSE2_INLINE __m128i _mm_sra_epi16(__m128i __a, __m128i __count)
{
	unsigned long long __c = (unsigned long long)((__v2du)__count)[0];
	__v8hi __x = (__v8hi)__a, __r;
	int __i, __s = __c > 15 ? 15 : (int)__c;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (short)((int)__x[__i] >> __s);
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_srai_epi16(__m128i __a, int __count)
{
	return _mm_sra_epi16(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__count, 0});
}

__MCC_SSE2_INLINE __m128i _mm_sra_epi32(__m128i __a, __m128i __count)
{
	unsigned long long __c = (unsigned long long)((__v2du)__count)[0];
	__v4si __x = (__v4si)__a, __r;
	int __i, __s = __c > 31 ? 31 : (int)__c;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] >> __s;
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_srai_epi32(__m128i __a, int __count)
{
	return _mm_sra_epi32(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__count, 0});
}

__MCC_SSE2_INLINE __m128i _mm_slli_si128(__m128i __a, int __imm)
{
	__v16qu __x = (__v16qu)__a, __r;
	int __i, __n = (int)((unsigned int)__imm & 0xff);
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (__n > __i || __n > 15) ? 0 : __x[__i - __n];
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_srli_si128(__m128i __a, int __imm)
{
	__v16qu __x = (__v16qu)__a, __r;
	int __i, __n = (int)((unsigned int)__imm & 0xff);
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (__n > 15 || __i + __n > 15) ? 0 : __x[__i + __n];
	return (__m128i)__r;
}

#define _mm_bslli_si128(a, imm) _mm_slli_si128((a), (imm))
#define _mm_bsrli_si128(a, imm) _mm_srli_si128((a), (imm))

__MCC_SSE2_INLINE __m128i _mm_packs_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b;
	__v16qs __r;
	int __i;
	for (__i = 0; __i < 8; __i++) {
		__r[__i] = (signed char)__mcc_sat_sb((int)__x[__i]);
		__r[__i + 8] = (signed char)__mcc_sat_sb((int)__y[__i]);
	}
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_packs_epi32(__m128i __a, __m128i __b)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b;
	__v8hi __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		__r[__i] = (short)__mcc_sat_sw(__x[__i]);
		__r[__i + 4] = (short)__mcc_sat_sw(__y[__i]);
	}
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_packus_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b;
	__v16qu __r;
	int __i;
	for (__i = 0; __i < 8; __i++) {
		__r[__i] = (unsigned char)__mcc_sat_ub((int)__x[__i]);
		__r[__i + 8] = (unsigned char)__mcc_sat_ub((int)__y[__i]);
	}
	return (__m128i)__r;
}

__MCC_SSE2_INLINE __m128i _mm_unpackhi_epi8(__m128i __a, __m128i __b)
{
	return (__m128i)__builtin_shufflevector((__v16qi)__a, (__v16qi)__b,
			8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31);
}

__MCC_SSE2_INLINE __m128i _mm_unpackhi_epi16(__m128i __a, __m128i __b)
{
	return (__m128i)__builtin_shufflevector((__v8hi)__a, (__v8hi)__b,
			4, 12, 5, 13, 6, 14, 7, 15);
}

__MCC_SSE2_INLINE __m128i _mm_unpackhi_epi32(__m128i __a, __m128i __b)
{
	return (__m128i)__builtin_shufflevector((__v4si)__a, (__v4si)__b, 2, 6, 3, 7);
}

__MCC_SSE2_INLINE __m128i _mm_unpackhi_epi64(__m128i __a, __m128i __b)
{
	return (__m128i)__builtin_shufflevector((__v2di)__a, (__v2di)__b, 1, 3);
}

__MCC_SSE2_INLINE __m128i _mm_unpacklo_epi8(__m128i __a, __m128i __b)
{
	return (__m128i)__builtin_shufflevector((__v16qi)__a, (__v16qi)__b,
			0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23);
}

__MCC_SSE2_INLINE __m128i _mm_unpacklo_epi16(__m128i __a, __m128i __b)
{
	return (__m128i)__builtin_shufflevector((__v8hi)__a, (__v8hi)__b,
			0, 8, 1, 9, 2, 10, 3, 11);
}

__MCC_SSE2_INLINE __m128i _mm_unpacklo_epi32(__m128i __a, __m128i __b)
{
	return (__m128i)__builtin_shufflevector((__v4si)__a, (__v4si)__b, 0, 4, 1, 5);
}

__MCC_SSE2_INLINE __m128i _mm_unpacklo_epi64(__m128i __a, __m128i __b)
{
	return (__m128i)__builtin_shufflevector((__v2di)__a, (__v2di)__b, 0, 2);
}

__MCC_SSE2_INLINE int _mm_movemask_epi8(__m128i __a)
{
	__v16qu __x = (__v16qu)__a;
	int __i, __r = 0;
	for (__i = 0; __i < 16; __i++)
		__r |= (int)((__x[__i] >> 7) & 1) << __i;
	return __r;
}

#define _mm_shuffle_epi32(a, imm)                                             \
	((__m128i)__builtin_shufflevector((__v4si)(__m128i)(a), (__v4si)(__m128i)(a), \
			(int)((imm) & 3), (int)(((imm) >> 2) & 3), (int)(((imm) >> 4) & 3),      \
			(int)(((imm) >> 6) & 3)))

#define _mm_shufflelo_epi16(a, imm)                                           \
	((__m128i)__builtin_shufflevector((__v8hi)(__m128i)(a), (__v8hi)(__m128i)(a), \
			(int)((imm) & 3), (int)(((imm) >> 2) & 3), (int)(((imm) >> 4) & 3),      \
			(int)(((imm) >> 6) & 3), 4, 5, 6, 7))

#define _mm_shufflehi_epi16(a, imm)                                           \
	((__m128i)__builtin_shufflevector((__v8hi)(__m128i)(a), (__v8hi)(__m128i)(a), \
			0, 1, 2, 3, (int)(((imm) & 3) + 4), (int)((((imm) >> 2) & 3) + 4),       \
			(int)((((imm) >> 4) & 3) + 4), (int)((((imm) >> 6) & 3) + 4)))

#define _mm_extract_epi16(a, imm) \
	((int)(unsigned short)((__v8hu)(__m128i)(a))[(imm) & 7])

__MCC_SSE2_INLINE __m128i __mcc_insert_epi16(__m128i __a, int __b, int __imm)
{
	__v8hu __r = (__v8hu)__a;
	__r[__imm & 7] = (unsigned short)__b;
	return (__m128i)__r;
}

#define _mm_insert_epi16(a, b, imm) __mcc_insert_epi16((a), (b), (imm))

__MCC_SSE2_INLINE void _mm_maskmoveu_si128(__m128i __d, __m128i __n, char *__p)
{
	__v16qu __data = (__v16qu)__d, __mask = (__v16qu)__n;
	int __i;
	for (__i = 0; __i < 16; __i++)
		if (__mask[__i] & 0x80)
			__p[__i] = (char)__data[__i];
}

__MCC_SSE2_INLINE void _mm_clflush(const void *__p)
{
	__asm__ volatile("clflush %0" : : "m"(*(const char *)__p));
}

__MCC_SSE2_INLINE void _mm_lfence(void)
{
	__asm__ volatile("lfence" : : : "memory");
}

__MCC_SSE2_INLINE void _mm_mfence(void)
{
	__asm__ volatile("mfence" : : : "memory");
}

#endif
