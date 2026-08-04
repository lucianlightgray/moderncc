#ifndef _FMAINTRIN_H_INCLUDED
#define _FMAINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "fmaintrin.h is only supported on x86 targets"
#endif

#include <avxintrin.h>

#define __MCC_FMA_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_FMA_INLINE float __mcc_fma_ss(float __a, float __b, float __c, int __na, int __nc)
{
	union {
		float __f;
		unsigned int __u;
	} __v;
	if (__a != __a || __b != __b || __c != __c) {
		__v.__f = (__a != __a) ? __a : ((__b != __b) ? __b : __c);
		__v.__u |= 0x00400000u;
		return __v.__f;
	}
	__v.__f = __builtin_fmaf(__na ? -__a : __a, __b, __nc ? -__c : __c);
	if (__v.__f != __v.__f)
		__v.__u = 0xffc00000u;
	return __v.__f;
}

__MCC_FMA_INLINE double __mcc_fma_sd(double __a, double __b, double __c, int __na,
																					 int __nc)
{
	union {
		double __d;
		unsigned long long __u;
	} __v;
	if (__a != __a || __b != __b || __c != __c) {
		__v.__d = (__a != __a) ? __a : ((__b != __b) ? __b : __c);
		__v.__u |= 0x0008000000000000ull;
		return __v.__d;
	}
	__v.__d = __builtin_fma(__na ? -__a : __a, __b, __nc ? -__c : __c);
	if (__v.__d != __v.__d)
		__v.__u = 0xfff8000000000000ull;
	return __v.__d;
}

__MCC_FMA_INLINE __m128d _mm_fmadd_pd(__m128d __a, __m128d __b, __m128d __c)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __z = (__v2df)__c, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 0, 0);
	return (__m128d)__r;
}

__MCC_FMA_INLINE __m128d _mm_fmsub_pd(__m128d __a, __m128d __b, __m128d __c)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __z = (__v2df)__c, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 0, 1);
	return (__m128d)__r;
}

__MCC_FMA_INLINE __m128d _mm_fnmadd_pd(__m128d __a, __m128d __b, __m128d __c)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __z = (__v2df)__c, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 1, 0);
	return (__m128d)__r;
}

__MCC_FMA_INLINE __m128d _mm_fnmsub_pd(__m128d __a, __m128d __b, __m128d __c)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __z = (__v2df)__c, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 1, 1);
	return (__m128d)__r;
}

__MCC_FMA_INLINE __m128d _mm_fmaddsub_pd(__m128d __a, __m128d __b, __m128d __c)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __z = (__v2df)__c, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 0, !(__i & 1));
	return (__m128d)__r;
}

__MCC_FMA_INLINE __m128d _mm_fmsubadd_pd(__m128d __a, __m128d __b, __m128d __c)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __z = (__v2df)__c, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 0, (__i & 1));
	return (__m128d)__r;
}

__MCC_FMA_INLINE __m128 _mm_fmadd_ps(__m128 __a, __m128 __b, __m128 __c)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __z = (__v4sf)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 0, 0);
	return (__m128)__r;
}

__MCC_FMA_INLINE __m128 _mm_fmsub_ps(__m128 __a, __m128 __b, __m128 __c)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __z = (__v4sf)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 0, 1);
	return (__m128)__r;
}

__MCC_FMA_INLINE __m128 _mm_fnmadd_ps(__m128 __a, __m128 __b, __m128 __c)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __z = (__v4sf)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 1, 0);
	return (__m128)__r;
}

__MCC_FMA_INLINE __m128 _mm_fnmsub_ps(__m128 __a, __m128 __b, __m128 __c)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __z = (__v4sf)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 1, 1);
	return (__m128)__r;
}

__MCC_FMA_INLINE __m128 _mm_fmaddsub_ps(__m128 __a, __m128 __b, __m128 __c)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __z = (__v4sf)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 0, !(__i & 1));
	return (__m128)__r;
}

__MCC_FMA_INLINE __m128 _mm_fmsubadd_ps(__m128 __a, __m128 __b, __m128 __c)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __z = (__v4sf)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 0, (__i & 1));
	return (__m128)__r;
}

__MCC_FMA_INLINE __m256d _mm256_fmadd_pd(__m256d __a, __m256d __b, __m256d __c)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __z = (__v4df)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 0, 0);
	return (__m256d)__r;
}

__MCC_FMA_INLINE __m256d _mm256_fmsub_pd(__m256d __a, __m256d __b, __m256d __c)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __z = (__v4df)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 0, 1);
	return (__m256d)__r;
}

__MCC_FMA_INLINE __m256d _mm256_fnmadd_pd(__m256d __a, __m256d __b, __m256d __c)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __z = (__v4df)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 1, 0);
	return (__m256d)__r;
}

__MCC_FMA_INLINE __m256d _mm256_fnmsub_pd(__m256d __a, __m256d __b, __m256d __c)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __z = (__v4df)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 1, 1);
	return (__m256d)__r;
}

__MCC_FMA_INLINE __m256d _mm256_fmaddsub_pd(__m256d __a, __m256d __b, __m256d __c)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __z = (__v4df)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 0, !(__i & 1));
	return (__m256d)__r;
}

__MCC_FMA_INLINE __m256d _mm256_fmsubadd_pd(__m256d __a, __m256d __b, __m256d __c)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __z = (__v4df)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_fma_sd(__x[__i], __y[__i], __z[__i], 0, (__i & 1));
	return (__m256d)__r;
}

__MCC_FMA_INLINE __m256 _mm256_fmadd_ps(__m256 __a, __m256 __b, __m256 __c)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __z = (__v8sf)__c, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 0, 0);
	return (__m256)__r;
}

__MCC_FMA_INLINE __m256 _mm256_fmsub_ps(__m256 __a, __m256 __b, __m256 __c)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __z = (__v8sf)__c, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 0, 1);
	return (__m256)__r;
}

__MCC_FMA_INLINE __m256 _mm256_fnmadd_ps(__m256 __a, __m256 __b, __m256 __c)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __z = (__v8sf)__c, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 1, 0);
	return (__m256)__r;
}

__MCC_FMA_INLINE __m256 _mm256_fnmsub_ps(__m256 __a, __m256 __b, __m256 __c)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __z = (__v8sf)__c, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 1, 1);
	return (__m256)__r;
}

__MCC_FMA_INLINE __m256 _mm256_fmaddsub_ps(__m256 __a, __m256 __b, __m256 __c)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __z = (__v8sf)__c, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 0, !(__i & 1));
	return (__m256)__r;
}

__MCC_FMA_INLINE __m256 _mm256_fmsubadd_ps(__m256 __a, __m256 __b, __m256 __c)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __z = (__v8sf)__c, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_fma_ss(__x[__i], __y[__i], __z[__i], 0, (__i & 1));
	return (__m256)__r;
}

__MCC_FMA_INLINE __m128d _mm_fmadd_sd(__m128d __a, __m128d __b, __m128d __c)
{
	__v2df __r = (__v2df)__a;
	__r[0] = __mcc_fma_sd(__r[0], ((__v2df)__b)[0], ((__v2df)__c)[0], 0, 0);
	return (__m128d)__r;
}

__MCC_FMA_INLINE __m128d _mm_fmsub_sd(__m128d __a, __m128d __b, __m128d __c)
{
	__v2df __r = (__v2df)__a;
	__r[0] = __mcc_fma_sd(__r[0], ((__v2df)__b)[0], ((__v2df)__c)[0], 0, 1);
	return (__m128d)__r;
}

__MCC_FMA_INLINE __m128d _mm_fnmadd_sd(__m128d __a, __m128d __b, __m128d __c)
{
	__v2df __r = (__v2df)__a;
	__r[0] = __mcc_fma_sd(__r[0], ((__v2df)__b)[0], ((__v2df)__c)[0], 1, 0);
	return (__m128d)__r;
}

__MCC_FMA_INLINE __m128d _mm_fnmsub_sd(__m128d __a, __m128d __b, __m128d __c)
{
	__v2df __r = (__v2df)__a;
	__r[0] = __mcc_fma_sd(__r[0], ((__v2df)__b)[0], ((__v2df)__c)[0], 1, 1);
	return (__m128d)__r;
}

__MCC_FMA_INLINE __m128 _mm_fmadd_ss(__m128 __a, __m128 __b, __m128 __c)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __mcc_fma_ss(__r[0], ((__v4sf)__b)[0], ((__v4sf)__c)[0], 0, 0);
	return (__m128)__r;
}

__MCC_FMA_INLINE __m128 _mm_fmsub_ss(__m128 __a, __m128 __b, __m128 __c)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __mcc_fma_ss(__r[0], ((__v4sf)__b)[0], ((__v4sf)__c)[0], 0, 1);
	return (__m128)__r;
}

__MCC_FMA_INLINE __m128 _mm_fnmadd_ss(__m128 __a, __m128 __b, __m128 __c)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __mcc_fma_ss(__r[0], ((__v4sf)__b)[0], ((__v4sf)__c)[0], 1, 0);
	return (__m128)__r;
}

__MCC_FMA_INLINE __m128 _mm_fnmsub_ss(__m128 __a, __m128 __b, __m128 __c)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __mcc_fma_ss(__r[0], ((__v4sf)__b)[0], ((__v4sf)__c)[0], 1, 1);
	return (__m128)__r;
}

#endif
