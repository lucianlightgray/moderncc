#ifndef _XMMINTRIN_H_INCLUDED
#define _XMMINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "xmmintrin.h is only supported on x86 targets"
#endif

#include <mmintrin.h>
#include <mm_malloc.h>

typedef float __m128 __attribute__((__vector_size__(16), __aligned__(16)));
typedef float __m128_u __attribute__((__vector_size__(16), __aligned__(1)));

typedef float __v4sf __attribute__((__vector_size__(16)));
typedef int __v4si __attribute__((__vector_size__(16)));
typedef unsigned int __v4su __attribute__((__vector_size__(16)));
typedef short __v8hi __attribute__((__vector_size__(16)));
typedef unsigned short __v8hu __attribute__((__vector_size__(16)));
typedef char __v16qi __attribute__((__vector_size__(16)));
typedef signed char __v16qs __attribute__((__vector_size__(16)));
typedef unsigned char __v16qu __attribute__((__vector_size__(16)));
typedef long long __v2di __attribute__((__vector_size__(16)));
typedef unsigned long long __v2du __attribute__((__vector_size__(16)));
typedef double __v2df __attribute__((__vector_size__(16)));

#define __MCC_SSE_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

struct __mcc_loadu_ps {
	__v4sf __v;
} __attribute__((__packed__, __may_alias__));

struct __mcc_loadu_si64 {
	long long __v;
} __attribute__((__packed__, __may_alias__));

struct __mcc_loadu_f32 {
	float __v;
} __attribute__((__packed__, __may_alias__));

#define _MM_SHUFFLE(fp3, fp2, fp1, fp0) \
	(((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | (fp0))

#define _MM_EXCEPT_INVALID 0x0001
#define _MM_EXCEPT_DENORM 0x0002
#define _MM_EXCEPT_DIV_ZERO 0x0004
#define _MM_EXCEPT_OVERFLOW 0x0008
#define _MM_EXCEPT_UNDERFLOW 0x0010
#define _MM_EXCEPT_INEXACT 0x0020
#define _MM_EXCEPT_MASK 0x003f

#define _MM_MASK_INVALID 0x0080
#define _MM_MASK_DENORM 0x0100
#define _MM_MASK_DIV_ZERO 0x0200
#define _MM_MASK_OVERFLOW 0x0400
#define _MM_MASK_UNDERFLOW 0x0800
#define _MM_MASK_INEXACT 0x1000
#define _MM_MASK_MASK 0x1f80

#define _MM_ROUND_NEAREST 0x0000
#define _MM_ROUND_DOWN 0x2000
#define _MM_ROUND_UP 0x4000
#define _MM_ROUND_TOWARD_ZERO 0x6000
#define _MM_ROUND_MASK 0x6000

#define _MM_FLUSH_ZERO_MASK 0x8000
#define _MM_FLUSH_ZERO_ON 0x8000
#define _MM_FLUSH_ZERO_OFF 0x0000

#define _MM_HINT_ET0 7
#define _MM_HINT_ET1 6
#define _MM_HINT_T0 3
#define _MM_HINT_T1 2
#define _MM_HINT_T2 1
#define _MM_HINT_NTA 0

__MCC_SSE_INLINE float __mcc_sqrtss(float __x)
{
	float __r;
	__asm__("sqrtss %1, %0" : "=x"(__r) : "x"(__x));
	return __r;
}

__MCC_SSE_INLINE float __mcc_rcpss(float __x)
{
	float __r;
	__asm__("rcpss %1, %0" : "=x"(__r) : "x"(__x));
	return __r;
}

__MCC_SSE_INLINE float __mcc_rsqrtss(float __x)
{
	float __r;
	__asm__("rsqrtss %1, %0" : "=x"(__r) : "x"(__x));
	return __r;
}

__MCC_SSE_INLINE float __mcc_rintf(float __x)
{
	float __m = 8388608.0f;
	if (!(__x > -__m && __x < __m))
		return __x;
	if (__x < 0.0f)
		return -((-__x + __m) - __m);
	return (__x + __m) - __m;
}

__MCC_SSE_INLINE int __mcc_cvt_f32_i32(float __x)
{
	float __r = __mcc_rintf(__x);
	if (!(__r >= -2147483648.0f && __r < 2147483648.0f))
		return (-2147483647 - 1);
	return (int)__r;
}

__MCC_SSE_INLINE int __mcc_cvtt_f32_i32(float __x)
{
	if (!(__x >= -2147483648.0f && __x < 2147483648.0f))
		return (-2147483647 - 1);
	return (int)__x;
}

__MCC_SSE_INLINE long long __mcc_cvt_f32_i64(float __x)
{
	float __r = __mcc_rintf(__x);
	if (!(__r >= -9223372036854775808.0f && __r < 9223372036854775808.0f))
		return (-9223372036854775807LL - 1);
	return (long long)__r;
}

__MCC_SSE_INLINE long long __mcc_cvtt_f32_i64(float __x)
{
	if (!(__x >= -9223372036854775808.0f && __x < 9223372036854775808.0f))
		return (-9223372036854775807LL - 1);
	return (long long)__x;
}

__MCC_SSE_INLINE __m128 _mm_setzero_ps(void)
{
	return (__m128)(__v4sf){0.0f, 0.0f, 0.0f, 0.0f};
}

__MCC_SSE_INLINE __m128 _mm_undefined_ps(void)
{
	return (__m128)(__v4sf){0.0f, 0.0f, 0.0f, 0.0f};
}

__MCC_SSE_INLINE __m128 _mm_set_ss(float __w)
{
	return (__m128)(__v4sf){__w, 0.0f, 0.0f, 0.0f};
}

__MCC_SSE_INLINE __m128 _mm_set1_ps(float __w)
{
	return (__m128)(__v4sf){__w, __w, __w, __w};
}

__MCC_SSE_INLINE __m128 _mm_set_ps1(float __w)
{
	return _mm_set1_ps(__w);
}

__MCC_SSE_INLINE __m128 _mm_set_ps(float __z, float __y, float __x, float __w)
{
	return (__m128)(__v4sf){__w, __x, __y, __z};
}

__MCC_SSE_INLINE __m128 _mm_setr_ps(float __z, float __y, float __x, float __w)
{
	return (__m128)(__v4sf){__z, __y, __x, __w};
}

__MCC_SSE_INLINE __m128 _mm_load_ss(const float *__p)
{
	return (__m128)(__v4sf){((const struct __mcc_loadu_f32 *)__p)->__v, 0.0f, 0.0f, 0.0f};
}

__MCC_SSE_INLINE __m128 _mm_load1_ps(const float *__p)
{
	float __v = ((const struct __mcc_loadu_f32 *)__p)->__v;
	return (__m128)(__v4sf){__v, __v, __v, __v};
}

__MCC_SSE_INLINE __m128 _mm_load_ps1(const float *__p)
{
	return _mm_load1_ps(__p);
}

__MCC_SSE_INLINE __m128 _mm_load_ps(const float *__p)
{
	return *(const __m128 *)__p;
}

__MCC_SSE_INLINE __m128 _mm_loadu_ps(const float *__p)
{
	return (__m128)((const struct __mcc_loadu_ps *)__p)->__v;
}

__MCC_SSE_INLINE __m128 _mm_loadr_ps(const float *__p)
{
	__v4sf __v = (__v4sf)*(const __m128 *)__p;
	return (__m128)__builtin_shufflevector(__v, __v, 3, 2, 1, 0);
}

__MCC_SSE_INLINE void _mm_store_ss(float *__p, __m128 __a)
{
	((struct __mcc_loadu_f32 *)__p)->__v = ((__v4sf)__a)[0];
}

__MCC_SSE_INLINE void _mm_store_ps(float *__p, __m128 __a)
{
	*(__m128 *)__p = __a;
}

__MCC_SSE_INLINE void _mm_storeu_ps(float *__p, __m128 __a)
{
	((struct __mcc_loadu_ps *)__p)->__v = (__v4sf)__a;
}

__MCC_SSE_INLINE void _mm_store1_ps(float *__p, __m128 __a)
{
	__v4sf __v = (__v4sf)__a;
	*(__m128 *)__p = (__m128)__builtin_shufflevector(__v, __v, 0, 0, 0, 0);
}

__MCC_SSE_INLINE void _mm_store_ps1(float *__p, __m128 __a)
{
	_mm_store1_ps(__p, __a);
}

__MCC_SSE_INLINE void _mm_storer_ps(float *__p, __m128 __a)
{
	__v4sf __v = (__v4sf)__a;
	*(__m128 *)__p = (__m128)__builtin_shufflevector(__v, __v, 3, 2, 1, 0);
}

__MCC_SSE_INLINE void _mm_stream_ps(float *__p, __m128 __a)
{
	*(__m128 *)__p = __a;
}

__MCC_SSE_INLINE __m128 _mm_add_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4sf)__a + (__v4sf)__b);
}

__MCC_SSE_INLINE __m128 _mm_add_ss(__m128 __a, __m128 __b)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __r[0] + ((__v4sf)__b)[0];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_sub_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4sf)__a - (__v4sf)__b);
}

__MCC_SSE_INLINE __m128 _mm_sub_ss(__m128 __a, __m128 __b)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __r[0] - ((__v4sf)__b)[0];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_mul_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4sf)__a * (__v4sf)__b);
}

__MCC_SSE_INLINE __m128 _mm_mul_ss(__m128 __a, __m128 __b)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __r[0] * ((__v4sf)__b)[0];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_div_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4sf)__a / (__v4sf)__b);
}

__MCC_SSE_INLINE __m128 _mm_div_ss(__m128 __a, __m128 __b)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __r[0] / ((__v4sf)__b)[0];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_sqrt_ps(__m128 __a)
{
	__v4sf __x = (__v4sf)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_sqrtss(__x[__i]);
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_sqrt_ss(__m128 __a)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __mcc_sqrtss(__r[0]);
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_rcp_ps(__m128 __a)
{
	__v4sf __x = (__v4sf)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_rcpss(__x[__i]);
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_rcp_ss(__m128 __a)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __mcc_rcpss(__r[0]);
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_rsqrt_ps(__m128 __a)
{
	__v4sf __x = (__v4sf)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_rsqrtss(__x[__i]);
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_rsqrt_ss(__m128 __a)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __mcc_rsqrtss(__r[0]);
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_min_ps(__m128 __a, __m128 __b)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_min_ss(__m128 __a, __m128 __b)
{
	__v4sf __r = (__v4sf)__a;
	float __y = ((__v4sf)__b)[0];
	__r[0] = __r[0] < __y ? __r[0] : __y;
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_max_ps(__m128 __a, __m128 __b)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_max_ss(__m128 __a, __m128 __b)
{
	__v4sf __r = (__v4sf)__a;
	float __y = ((__v4sf)__b)[0];
	__r[0] = __r[0] > __y ? __r[0] : __y;
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_and_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4su)__a & (__v4su)__b);
}

__MCC_SSE_INLINE __m128 _mm_andnot_ps(__m128 __a, __m128 __b)
{
	return (__m128)(~(__v4su)__a & (__v4su)__b);
}

__MCC_SSE_INLINE __m128 _mm_or_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4su)__a | (__v4su)__b);
}

__MCC_SSE_INLINE __m128 _mm_xor_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4su)__a ^ (__v4su)__b);
}

__MCC_SSE_INLINE __m128 _mm_cmpeq_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4si)((__v4sf)__a == (__v4sf)__b));
}

__MCC_SSE_INLINE __m128 _mm_cmplt_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4si)((__v4sf)__a < (__v4sf)__b));
}

__MCC_SSE_INLINE __m128 _mm_cmple_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4si)((__v4sf)__a <= (__v4sf)__b));
}

__MCC_SSE_INLINE __m128 _mm_cmpgt_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4si)((__v4sf)__a > (__v4sf)__b));
}

__MCC_SSE_INLINE __m128 _mm_cmpge_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4si)((__v4sf)__a >= (__v4sf)__b));
}

__MCC_SSE_INLINE __m128 _mm_cmpneq_ps(__m128 __a, __m128 __b)
{
	return (__m128)((__v4si)((__v4sf)__a != (__v4sf)__b));
}

__MCC_SSE_INLINE __m128 _mm_cmpnlt_ps(__m128 __a, __m128 __b)
{
	return (__m128)(~(__v4si)((__v4sf)__a < (__v4sf)__b));
}

__MCC_SSE_INLINE __m128 _mm_cmpnle_ps(__m128 __a, __m128 __b)
{
	return (__m128)(~(__v4si)((__v4sf)__a <= (__v4sf)__b));
}

__MCC_SSE_INLINE __m128 _mm_cmpngt_ps(__m128 __a, __m128 __b)
{
	return (__m128)(~(__v4si)((__v4sf)__a > (__v4sf)__b));
}

__MCC_SSE_INLINE __m128 _mm_cmpnge_ps(__m128 __a, __m128 __b)
{
	return (__m128)(~(__v4si)((__v4sf)__a >= (__v4sf)__b));
}

__MCC_SSE_INLINE __m128 _mm_cmpord_ps(__m128 __a, __m128 __b)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b;
	return (__m128)((__v4si)(__x == __x) & (__v4si)(__y == __y));
}

__MCC_SSE_INLINE __m128 _mm_cmpunord_ps(__m128 __a, __m128 __b)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b;
	return (__m128)((__v4si)(__x != __x) | (__v4si)(__y != __y));
}

__MCC_SSE_INLINE __m128 __mcc_setlow_ps(__m128 __a, int __m)
{
	union {
		int __i;
		float __f;
	} __c;
	__v4sf __r = (__v4sf)__a;
	__c.__i = __m;
	__r[0] = __c.__f;
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_cmpeq_ss(__m128 __a, __m128 __b)
{
	return __mcc_setlow_ps(__a, ((__v4sf)__a)[0] == ((__v4sf)__b)[0] ? -1 : 0);
}

__MCC_SSE_INLINE __m128 _mm_cmplt_ss(__m128 __a, __m128 __b)
{
	return __mcc_setlow_ps(__a, ((__v4sf)__a)[0] < ((__v4sf)__b)[0] ? -1 : 0);
}

__MCC_SSE_INLINE __m128 _mm_cmple_ss(__m128 __a, __m128 __b)
{
	return __mcc_setlow_ps(__a, ((__v4sf)__a)[0] <= ((__v4sf)__b)[0] ? -1 : 0);
}

__MCC_SSE_INLINE __m128 _mm_cmpneq_ss(__m128 __a, __m128 __b)
{
	return __mcc_setlow_ps(__a, ((__v4sf)__a)[0] != ((__v4sf)__b)[0] ? -1 : 0);
}

__MCC_SSE_INLINE __m128 _mm_cmpnlt_ss(__m128 __a, __m128 __b)
{
	return __mcc_setlow_ps(__a, ((__v4sf)__a)[0] < ((__v4sf)__b)[0] ? 0 : -1);
}

__MCC_SSE_INLINE __m128 _mm_cmpnle_ss(__m128 __a, __m128 __b)
{
	return __mcc_setlow_ps(__a, ((__v4sf)__a)[0] <= ((__v4sf)__b)[0] ? 0 : -1);
}

__MCC_SSE_INLINE __m128 _mm_cmpord_ss(__m128 __a, __m128 __b)
{
	float __x = ((__v4sf)__a)[0], __y = ((__v4sf)__b)[0];
	return __mcc_setlow_ps(__a, (__x == __x && __y == __y) ? -1 : 0);
}

__MCC_SSE_INLINE __m128 _mm_cmpunord_ss(__m128 __a, __m128 __b)
{
	float __x = ((__v4sf)__a)[0], __y = ((__v4sf)__b)[0];
	return __mcc_setlow_ps(__a, (__x != __x || __y != __y) ? -1 : 0);
}

__MCC_SSE_INLINE __m128 _mm_cmpgt_ss(__m128 __a, __m128 __b)
{
	__v4sf __r = (__v4sf)_mm_cmplt_ss(__b, __a);
	__v4sf __x = (__v4sf)__a;
	return (__m128)__builtin_shufflevector(__r, __x, 0, 5, 6, 7);
}

__MCC_SSE_INLINE __m128 _mm_cmpge_ss(__m128 __a, __m128 __b)
{
	__v4sf __r = (__v4sf)_mm_cmple_ss(__b, __a);
	__v4sf __x = (__v4sf)__a;
	return (__m128)__builtin_shufflevector(__r, __x, 0, 5, 6, 7);
}

__MCC_SSE_INLINE __m128 _mm_cmpngt_ss(__m128 __a, __m128 __b)
{
	__v4sf __r = (__v4sf)_mm_cmpnlt_ss(__b, __a);
	__v4sf __x = (__v4sf)__a;
	return (__m128)__builtin_shufflevector(__r, __x, 0, 5, 6, 7);
}

__MCC_SSE_INLINE __m128 _mm_cmpnge_ss(__m128 __a, __m128 __b)
{
	__v4sf __r = (__v4sf)_mm_cmpnle_ss(__b, __a);
	__v4sf __x = (__v4sf)__a;
	return (__m128)__builtin_shufflevector(__r, __x, 0, 5, 6, 7);
}

__MCC_SSE_INLINE int _mm_comieq_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] == ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_comilt_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] < ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_comile_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] <= ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_comigt_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] > ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_comige_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] >= ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_comineq_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] != ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_ucomieq_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] == ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_ucomilt_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] < ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_ucomile_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] <= ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_ucomigt_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] > ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_ucomige_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] >= ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_ucomineq_ss(__m128 __a, __m128 __b)
{
	return ((__v4sf)__a)[0] != ((__v4sf)__b)[0];
}

__MCC_SSE_INLINE int _mm_cvtss_si32(__m128 __a)
{
	return __mcc_cvt_f32_i32(((__v4sf)__a)[0]);
}

__MCC_SSE_INLINE int _mm_cvt_ss2si(__m128 __a)
{
	return _mm_cvtss_si32(__a);
}

__MCC_SSE_INLINE int _mm_cvttss_si32(__m128 __a)
{
	return __mcc_cvtt_f32_i32(((__v4sf)__a)[0]);
}

__MCC_SSE_INLINE int _mm_cvtt_ss2si(__m128 __a)
{
	return _mm_cvttss_si32(__a);
}

#ifdef __x86_64__
__MCC_SSE_INLINE long long _mm_cvtss_si64(__m128 __a)
{
	return __mcc_cvt_f32_i64(((__v4sf)__a)[0]);
}

__MCC_SSE_INLINE long long _mm_cvttss_si64(__m128 __a)
{
	return __mcc_cvtt_f32_i64(((__v4sf)__a)[0]);
}
#endif

__MCC_SSE_INLINE __m128 _mm_cvtsi32_ss(__m128 __a, int __b)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = (float)__b;
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_cvt_si2ss(__m128 __a, int __b)
{
	return _mm_cvtsi32_ss(__a, __b);
}

#ifdef __x86_64__
__MCC_SSE_INLINE __m128 _mm_cvtsi64_ss(__m128 __a, long long __b)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = (float)__b;
	return (__m128)__r;
}
#endif

__MCC_SSE_INLINE float _mm_cvtss_f32(__m128 __a)
{
	return ((__v4sf)__a)[0];
}

__MCC_SSE_INLINE __m64 _mm_cvtps_pi32(__m128 __a)
{
	__v4sf __x = (__v4sf)__a;
	__v2si __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_cvt_f32_i32(__x[__i]);
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _mm_cvt_ps2pi(__m128 __a)
{
	return _mm_cvtps_pi32(__a);
}

__MCC_SSE_INLINE __m64 _mm_cvttps_pi32(__m128 __a)
{
	__v4sf __x = (__v4sf)__a;
	__v2si __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_cvtt_f32_i32(__x[__i]);
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _mm_cvtt_ps2pi(__m128 __a)
{
	return _mm_cvttps_pi32(__a);
}

__MCC_SSE_INLINE __m128 _mm_cvtpi32_ps(__m128 __a, __m64 __b)
{
	__v4sf __r = (__v4sf)__a;
	__v2si __x = (__v2si)__b;
	__r[0] = (float)__x[0];
	__r[1] = (float)__x[1];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_cvt_pi2ps(__m128 __a, __m64 __b)
{
	return _mm_cvtpi32_ps(__a, __b);
}

__MCC_SSE_INLINE __m128 _mm_cvtpi16_ps(__m64 __a)
{
	__v4hi __x = (__v4hi)__a;
	__v4sf __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (float)__x[__i];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_cvtpu16_ps(__m64 __a)
{
	__v4hu __x = (__v4hu)__a;
	__v4sf __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (float)__x[__i];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_cvtpi8_ps(__m64 __a)
{
	__v8qs __x = (__v8qs)__a;
	__v4sf __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (float)__x[__i];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_cvtpu8_ps(__m64 __a)
{
	__v8qu __x = (__v8qu)__a;
	__v4sf __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (float)__x[__i];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_cvtpi32x2_ps(__m64 __a, __m64 __b)
{
	__v2si __x = (__v2si)__a, __y = (__v2si)__b;
	__v4sf __r;
	__r[0] = (float)__x[0];
	__r[1] = (float)__x[1];
	__r[2] = (float)__y[0];
	__r[3] = (float)__y[1];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m64 _mm_cvtps_pi16(__m128 __a)
{
	__v4sf __x = (__v4sf)__a;
	__v4hi __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (short)__mcc_sat_sw(__mcc_cvt_f32_i32(__x[__i]));
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _mm_cvtps_pi8(__m128 __a)
{
	__v4sf __x = (__v4sf)__a;
	__v8qs __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		__r[__i] = (signed char)__mcc_sat_sb(__mcc_sat_sw(__mcc_cvt_f32_i32(__x[__i])));
		__r[__i + 4] = 0;
	}
	return (__m64)__r;
}

__MCC_SSE_INLINE __m128 _mm_unpackhi_ps(__m128 __a, __m128 __b)
{
	return (__m128)__builtin_shufflevector((__v4sf)__a, (__v4sf)__b, 2, 6, 3, 7);
}

__MCC_SSE_INLINE __m128 _mm_unpacklo_ps(__m128 __a, __m128 __b)
{
	return (__m128)__builtin_shufflevector((__v4sf)__a, (__v4sf)__b, 0, 4, 1, 5);
}

__MCC_SSE_INLINE __m128 _mm_movehl_ps(__m128 __a, __m128 __b)
{
	return (__m128)__builtin_shufflevector((__v4sf)__a, (__v4sf)__b, 6, 7, 2, 3);
}

__MCC_SSE_INLINE __m128 _mm_movelh_ps(__m128 __a, __m128 __b)
{
	return (__m128)__builtin_shufflevector((__v4sf)__a, (__v4sf)__b, 0, 1, 4, 5);
}

__MCC_SSE_INLINE __m128 _mm_move_ss(__m128 __a, __m128 __b)
{
	return (__m128)__builtin_shufflevector((__v4sf)__a, (__v4sf)__b, 4, 1, 2, 3);
}

__MCC_SSE_INLINE __m128 _mm_loadh_pi(__m128 __a, const __m64 *__p)
{
	__v4sf __r = (__v4sf)__a;
	union {
		long long __i;
		float __f[2];
	} __c;
	__c.__i = ((const struct __mcc_loadu_si64 *)__p)->__v;
	__r[2] = __c.__f[0];
	__r[3] = __c.__f[1];
	return (__m128)__r;
}

__MCC_SSE_INLINE __m128 _mm_loadl_pi(__m128 __a, const __m64 *__p)
{
	__v4sf __r = (__v4sf)__a;
	union {
		long long __i;
		float __f[2];
	} __c;
	__c.__i = ((const struct __mcc_loadu_si64 *)__p)->__v;
	__r[0] = __c.__f[0];
	__r[1] = __c.__f[1];
	return (__m128)__r;
}

__MCC_SSE_INLINE void _mm_storeh_pi(__m64 *__p, __m128 __a)
{
	union {
		long long __i;
		float __f[2];
	} __c;
	__c.__f[0] = ((__v4sf)__a)[2];
	__c.__f[1] = ((__v4sf)__a)[3];
	((struct __mcc_loadu_si64 *)__p)->__v = __c.__i;
}

__MCC_SSE_INLINE void _mm_storel_pi(__m64 *__p, __m128 __a)
{
	union {
		long long __i;
		float __f[2];
	} __c;
	__c.__f[0] = ((__v4sf)__a)[0];
	__c.__f[1] = ((__v4sf)__a)[1];
	((struct __mcc_loadu_si64 *)__p)->__v = __c.__i;
}

__MCC_SSE_INLINE int _mm_movemask_ps(__m128 __a)
{
	__v4su __x = (__v4su)__a;
	return (int)(((__x[0] >> 31) & 1) | (((__x[1] >> 31) & 1) << 1) |
			(((__x[2] >> 31) & 1) << 2) | (((__x[3] >> 31) & 1) << 3));
}

#define _mm_shuffle_ps(a, b, mask)                                          \
	((__m128)__builtin_shufflevector((__v4sf)(__m128)(a), (__v4sf)(__m128)(b), \
			(int)((mask) & 3), (int)(((mask) >> 2) & 3),                          \
			(int)((((mask) >> 4) & 3) + 4), (int)((((mask) >> 6) & 3) + 4)))

#define _mm_shuffle_pi16(a, mask)                                            \
	((__m64)__builtin_shufflevector((__v4hi)(__m64)(a), (__v4hi)(__m64)(a),     \
			(int)((mask) & 3), (int)(((mask) >> 2) & 3),                           \
			(int)(((mask) >> 4) & 3), (int)(((mask) >> 6) & 3)))

#define _m_pshufw(a, mask) _mm_shuffle_pi16((a), (mask))

#define _mm_extract_pi16(a, n) ((int)(unsigned short)((__v4hu)(__m64)(a))[(n) & 3])
#define _m_pextrw(a, n) _mm_extract_pi16((a), (n))

__MCC_SSE_INLINE __m64 __mcc_insert_pi16(__m64 __a, int __d, int __n)
{
	__v4hu __r = (__v4hu)__a;
	__r[__n & 3] = (unsigned short)__d;
	return (__m64)__r;
}

#define _mm_insert_pi16(a, d, n) __mcc_insert_pi16((a), (d), (n))
#define _m_pinsrw(a, d, n) _mm_insert_pi16((a), (d), (n))

__MCC_SSE_INLINE __m64 _mm_max_pi16(__m64 __a, __m64 __b)
{
	__v4hi __x = (__v4hi)__a, __y = (__v4hi)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _m_pmaxsw(__m64 __a, __m64 __b)
{
	return _mm_max_pi16(__a, __b);
}

__MCC_SSE_INLINE __m64 _mm_max_pu8(__m64 __a, __m64 __b)
{
	__v8qu __x = (__v8qu)__a, __y = (__v8qu)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _m_pmaxub(__m64 __a, __m64 __b)
{
	return _mm_max_pu8(__a, __b);
}

__MCC_SSE_INLINE __m64 _mm_min_pi16(__m64 __a, __m64 __b)
{
	__v4hi __x = (__v4hi)__a, __y = (__v4hi)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _m_pminsw(__m64 __a, __m64 __b)
{
	return _mm_min_pi16(__a, __b);
}

__MCC_SSE_INLINE __m64 _mm_min_pu8(__m64 __a, __m64 __b)
{
	__v8qu __x = (__v8qu)__a, __y = (__v8qu)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _m_pminub(__m64 __a, __m64 __b)
{
	return _mm_min_pu8(__a, __b);
}

__MCC_SSE_INLINE int _mm_movemask_pi8(__m64 __a)
{
	__v8qu __x = (__v8qu)__a;
	int __i, __r = 0;
	for (__i = 0; __i < 8; __i++)
		__r |= (int)((__x[__i] >> 7) & 1) << __i;
	return __r;
}

__MCC_SSE_INLINE int _m_pmovmskb(__m64 __a)
{
	return _mm_movemask_pi8(__a);
}

__MCC_SSE_INLINE __m64 _mm_mulhi_pu16(__m64 __a, __m64 __b)
{
	__v4hu __x = (__v4hu)__a, __y = (__v4hu)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (unsigned short)(((unsigned int)__x[__i] * (unsigned int)__y[__i]) >> 16);
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _m_pmulhuw(__m64 __a, __m64 __b)
{
	return _mm_mulhi_pu16(__a, __b);
}

__MCC_SSE_INLINE __m64 _mm_avg_pu8(__m64 __a, __m64 __b)
{
	__v8qu __x = (__v8qu)__a, __y = (__v8qu)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (unsigned char)(((unsigned int)__x[__i] + (unsigned int)__y[__i] + 1) >> 1);
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _m_pavgb(__m64 __a, __m64 __b)
{
	return _mm_avg_pu8(__a, __b);
}

__MCC_SSE_INLINE __m64 _mm_avg_pu16(__m64 __a, __m64 __b)
{
	__v4hu __x = (__v4hu)__a, __y = (__v4hu)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (unsigned short)(((unsigned int)__x[__i] + (unsigned int)__y[__i] + 1) >> 1);
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _m_pavgw(__m64 __a, __m64 __b)
{
	return _mm_avg_pu16(__a, __b);
}

__MCC_SSE_INLINE __m64 _mm_sad_pu8(__m64 __a, __m64 __b)
{
	__v8qu __x = (__v8qu)__a, __y = (__v8qu)__b;
	__v4hu __r;
	unsigned int __s = 0;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__s += __x[__i] > __y[__i] ? (unsigned int)(__x[__i] - __y[__i])
															 : (unsigned int)(__y[__i] - __x[__i]);
	__r[0] = (unsigned short)__s;
	__r[1] = 0;
	__r[2] = 0;
	__r[3] = 0;
	return (__m64)__r;
}

__MCC_SSE_INLINE __m64 _m_psadbw(__m64 __a, __m64 __b)
{
	return _mm_sad_pu8(__a, __b);
}

__MCC_SSE_INLINE void _mm_maskmove_si64(__m64 __d, __m64 __n, char *__p)
{
	__v8qu __data = (__v8qu)__d, __mask = (__v8qu)__n;
	int __i;
	for (__i = 0; __i < 8; __i++)
		if (__mask[__i] & 0x80)
			__p[__i] = (char)__data[__i];
}

__MCC_SSE_INLINE void _m_maskmovq(__m64 __d, __m64 __n, char *__p)
{
	_mm_maskmove_si64(__d, __n, __p);
}

__MCC_SSE_INLINE void _mm_stream_pi(__m64 *__p, __m64 __a)
{
	*__p = __a;
}

__MCC_SSE_INLINE unsigned int _mm_getcsr(void)
{
	unsigned int __r;
	__asm__ volatile("stmxcsr %0" : "=m"(__r));
	return __r;
}

__MCC_SSE_INLINE void _mm_setcsr(unsigned int __i)
{
	__asm__ volatile("ldmxcsr %0" : : "m"(__i));
}

#define _MM_GET_EXCEPTION_STATE() (_mm_getcsr() & _MM_EXCEPT_MASK)
#define _MM_SET_EXCEPTION_STATE(__mask) \
	_mm_setcsr((_mm_getcsr() & ~_MM_EXCEPT_MASK) | (__mask))
#define _MM_GET_EXCEPTION_MASK() (_mm_getcsr() & _MM_MASK_MASK)
#define _MM_SET_EXCEPTION_MASK(__mask) \
	_mm_setcsr((_mm_getcsr() & ~_MM_MASK_MASK) | (__mask))
#define _MM_GET_ROUNDING_MODE() (_mm_getcsr() & _MM_ROUND_MASK)
#define _MM_SET_ROUNDING_MODE(__mode) \
	_mm_setcsr((_mm_getcsr() & ~_MM_ROUND_MASK) | (__mode))
#define _MM_GET_FLUSH_ZERO_MODE() (_mm_getcsr() & _MM_FLUSH_ZERO_MASK)
#define _MM_SET_FLUSH_ZERO_MODE(__mode) \
	_mm_setcsr((_mm_getcsr() & ~_MM_FLUSH_ZERO_MASK) | (__mode))

__MCC_SSE_INLINE void _mm_sfence(void)
{
	__asm__ volatile("sfence" : : : "memory");
}

__MCC_SSE_INLINE void _mm_pause(void)
{
	__asm__ volatile("pause" : : : "memory");
}

#define _mm_prefetch(P, I) \
	__builtin_prefetch((P), (((I) & 0x4) >> 2), ((I) & 0x3))

#define _MM_TRANSPOSE4_PS(row0, row1, row2, row3)                    \
	do {                                                               \
		__m128 __t0 = _mm_unpacklo_ps((row0), (row1));                   \
		__m128 __t1 = _mm_unpacklo_ps((row2), (row3));                   \
		__m128 __t2 = _mm_unpackhi_ps((row0), (row1));                   \
		__m128 __t3 = _mm_unpackhi_ps((row2), (row3));                   \
		(row0) = _mm_movelh_ps(__t0, __t1);                              \
		(row1) = _mm_movehl_ps(__t1, __t0);                              \
		(row2) = _mm_movelh_ps(__t2, __t3);                              \
		(row3) = _mm_movehl_ps(__t3, __t2);                              \
	} while (0)

#define _MM_ALIGN16 __attribute__((__aligned__(16)))

#endif
