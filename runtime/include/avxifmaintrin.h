#ifndef _AVXIFMAINTRIN_H_INCLUDED
#define _AVXIFMAINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "avxifmaintrin.h is only supported on x86 targets"
#endif

#include <avx2intrin.h>

#define __MCC_IFMA_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

#define __MCC_IFMA_M52 0x000fffffffffffffull
#define __MCC_IFMA_M26 0x0000000003ffffffull

__MCC_IFMA_INLINE unsigned long long __mcc_ifma_lo(unsigned long long __b,
																									 unsigned long long __c)
{
	unsigned long long __bl = __b & __MCC_IFMA_M26, __bh = (__b & __MCC_IFMA_M52) >> 26;
	unsigned long long __cl = __c & __MCC_IFMA_M26, __ch = (__c & __MCC_IFMA_M52) >> 26;
	unsigned long long __mid = __bh * __cl + __bl * __ch;
	unsigned long long __low = __bl * __cl + ((__mid & __MCC_IFMA_M26) << 26);
	return __low & __MCC_IFMA_M52;
}

__MCC_IFMA_INLINE unsigned long long __mcc_ifma_hi(unsigned long long __b,
																									 unsigned long long __c)
{
	unsigned long long __bl = __b & __MCC_IFMA_M26, __bh = (__b & __MCC_IFMA_M52) >> 26;
	unsigned long long __cl = __c & __MCC_IFMA_M26, __ch = (__c & __MCC_IFMA_M52) >> 26;
	unsigned long long __mid = __bh * __cl + __bl * __ch;
	unsigned long long __low = __bl * __cl + ((__mid & __MCC_IFMA_M26) << 26);
	return (__bh * __ch + (__mid >> 26) + (__low >> 52)) & __MCC_IFMA_M52;
}

__MCC_IFMA_INLINE __m128i _mm_madd52lo_epu64(__m128i __a, __m128i __b, __m128i __c)
{
	__v2du __x = (__v2du)__a, __y = (__v2du)__b, __z = (__v2du)__c;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__x[__i] += __mcc_ifma_lo(__y[__i], __z[__i]);
	return (__m128i)__x;
}

__MCC_IFMA_INLINE __m128i _mm_madd52hi_epu64(__m128i __a, __m128i __b, __m128i __c)
{
	__v2du __x = (__v2du)__a, __y = (__v2du)__b, __z = (__v2du)__c;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__x[__i] += __mcc_ifma_hi(__y[__i], __z[__i]);
	return (__m128i)__x;
}

__MCC_IFMA_INLINE __m256i _mm256_madd52lo_epu64(__m256i __a, __m256i __b, __m256i __c)
{
	__v4du __x = (__v4du)__a, __y = (__v4du)__b, __z = (__v4du)__c;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__x[__i] += __mcc_ifma_lo(__y[__i], __z[__i]);
	return (__m256i)__x;
}

__MCC_IFMA_INLINE __m256i _mm256_madd52hi_epu64(__m256i __a, __m256i __b, __m256i __c)
{
	__v4du __x = (__v4du)__a, __y = (__v4du)__b, __z = (__v4du)__c;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__x[__i] += __mcc_ifma_hi(__y[__i], __z[__i]);
	return (__m256i)__x;
}

#endif
