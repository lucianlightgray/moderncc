#ifndef _AVXVNNIINTRIN_H_INCLUDED
#define _AVXVNNIINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "avxvnniintrin.h is only supported on x86 targets"
#endif

#include <avx2intrin.h>

#define __MCC_VNNI_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_VNNI_INLINE int __mcc_vnni_sat32(long long __v)
{
	if (__v > 2147483647LL)
		return 2147483647;
	if (__v < -2147483647LL - 1LL)
		return -2147483647 - 1;
	return (int)__v;
}

__MCC_VNNI_INLINE __m128i _mm_dpbusd_epi32(__m128i __s, __m128i __a, __m128i __b)
{
	__v16qu __x = (__v16qu)__a;
	__v16qs __y = (__v16qs)__b;
	__v4si __r = (__v4si)__s;
	int __j, __i;
	for (__j = 0; __j < 4; __j++) {
		long long __t = (long long)__r[__j];
		for (__i = 0; __i < 4; __i++)
			__t += (long long)(int)__x[4 * __j + __i] * (long long)(int)__y[4 * __j + __i];
		__r[__j] = (int)__t;
	}
	return (__m128i)__r;
}

__MCC_VNNI_INLINE __m128i _mm_dpbusds_epi32(__m128i __s, __m128i __a, __m128i __b)
{
	__v16qu __x = (__v16qu)__a;
	__v16qs __y = (__v16qs)__b;
	__v4si __r = (__v4si)__s;
	int __j, __i;
	for (__j = 0; __j < 4; __j++) {
		long long __t = (long long)__r[__j];
		for (__i = 0; __i < 4; __i++)
			__t += (long long)(int)__x[4 * __j + __i] * (long long)(int)__y[4 * __j + __i];
		__r[__j] = __mcc_vnni_sat32(__t);
	}
	return (__m128i)__r;
}

__MCC_VNNI_INLINE __m128i _mm_dpwssd_epi32(__m128i __s, __m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b;
	__v4si __r = (__v4si)__s;
	int __j;
	for (__j = 0; __j < 4; __j++)
		__r[__j] = (int)((long long)__r[__j]
										 + (long long)(int)__x[2 * __j] * (long long)(int)__y[2 * __j]
										 + (long long)(int)__x[2 * __j + 1] * (long long)(int)__y[2 * __j + 1]);
	return (__m128i)__r;
}

__MCC_VNNI_INLINE __m128i _mm_dpwssds_epi32(__m128i __s, __m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b;
	__v4si __r = (__v4si)__s;
	int __j;
	for (__j = 0; __j < 4; __j++)
		__r[__j] = __mcc_vnni_sat32((long long)__r[__j]
																+ (long long)(int)__x[2 * __j] * (long long)(int)__y[2 * __j]
																+ (long long)(int)__x[2 * __j + 1] * (long long)(int)__y[2 * __j + 1]);
	return (__m128i)__r;
}

__MCC_VNNI_INLINE __m256i _mm256_dpbusd_epi32(__m256i __s, __m256i __a, __m256i __b)
{
	__v32qu __x = (__v32qu)__a;
	__v32qs __y = (__v32qs)__b;
	__v8si __r = (__v8si)__s;
	int __j, __i;
	for (__j = 0; __j < 8; __j++) {
		long long __t = (long long)__r[__j];
		for (__i = 0; __i < 4; __i++)
			__t += (long long)(int)__x[4 * __j + __i] * (long long)(int)__y[4 * __j + __i];
		__r[__j] = (int)__t;
	}
	return (__m256i)__r;
}

__MCC_VNNI_INLINE __m256i _mm256_dpbusds_epi32(__m256i __s, __m256i __a, __m256i __b)
{
	__v32qu __x = (__v32qu)__a;
	__v32qs __y = (__v32qs)__b;
	__v8si __r = (__v8si)__s;
	int __j, __i;
	for (__j = 0; __j < 8; __j++) {
		long long __t = (long long)__r[__j];
		for (__i = 0; __i < 4; __i++)
			__t += (long long)(int)__x[4 * __j + __i] * (long long)(int)__y[4 * __j + __i];
		__r[__j] = __mcc_vnni_sat32(__t);
	}
	return (__m256i)__r;
}

__MCC_VNNI_INLINE __m256i _mm256_dpwssd_epi32(__m256i __s, __m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b;
	__v8si __r = (__v8si)__s;
	int __j;
	for (__j = 0; __j < 8; __j++) {
		long long __t = (long long)__r[__j]
										+ (long long)(int)__x[2 * __j] * (long long)(int)__y[2 * __j]
										+ (long long)(int)__x[2 * __j + 1] * (long long)(int)__y[2 * __j + 1];
		__r[__j] = (int)__t;
	}
	return (__m256i)__r;
}

__MCC_VNNI_INLINE __m256i _mm256_dpwssds_epi32(__m256i __s, __m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b;
	__v8si __r = (__v8si)__s;
	int __j;
	for (__j = 0; __j < 8; __j++) {
		long long __t = (long long)__r[__j]
										+ (long long)(int)__x[2 * __j] * (long long)(int)__y[2 * __j]
										+ (long long)(int)__x[2 * __j + 1] * (long long)(int)__y[2 * __j + 1];
		__r[__j] = __mcc_vnni_sat32(__t);
	}
	return (__m256i)__r;
}

#endif
