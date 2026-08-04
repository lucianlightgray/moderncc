#ifndef _TMMINTRIN_H_INCLUDED
#define _TMMINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "tmmintrin.h is only supported on x86 targets"
#endif

#include <pmmintrin.h>

#define __MCC_SSSE3_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_SSSE3_INLINE __m128i _mm_abs_epi8(__m128i __a)
{
	__v16qs __x = (__v16qs)__a, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (signed char)(__x[__i] < 0 ? (int)(0u - (unsigned int)(int)__x[__i])
						     : (int)__x[__i]);
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_abs_epi16(__m128i __a)
{
	__v8hi __x = (__v8hi)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (short)(__x[__i] < 0 ? (int)(0u - (unsigned int)(int)__x[__i])
						: (int)__x[__i]);
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_abs_epi32(__m128i __a)
{
	__v4si __x = (__v4si)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (int)(__x[__i] < 0 ? 0u - (unsigned int)__x[__i] : (unsigned int)__x[__i]);
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_abs_pi8(__m64 __a)
{
	__v8qs __x = (__v8qs)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (signed char)(__x[__i] < 0 ? (int)(0u - (unsigned int)(int)__x[__i])
						     : (int)__x[__i]);
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_abs_pi16(__m64 __a)
{
	__v4hi __x = (__v4hi)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (short)(__x[__i] < 0 ? (int)(0u - (unsigned int)(int)__x[__i])
						: (int)__x[__i]);
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_abs_pi32(__m64 __a)
{
	__v2si __x = (__v2si)__a, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = (int)(__x[__i] < 0 ? 0u - (unsigned int)__x[__i] : (unsigned int)__x[__i]);
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_hadd_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		__r[__i] = (short)((unsigned short)__x[2 * __i] + (unsigned short)__x[2 * __i + 1]);
		__r[__i + 4] =
			(short)((unsigned short)__y[2 * __i] + (unsigned short)__y[2 * __i + 1]);
	}
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_hadd_epi32(__m128i __a, __m128i __b)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b, __r;
	int __i;
	for (__i = 0; __i < 2; __i++) {
		__r[__i] = (int)((unsigned int)__x[2 * __i] + (unsigned int)__x[2 * __i + 1]);
		__r[__i + 2] = (int)((unsigned int)__y[2 * __i] + (unsigned int)__y[2 * __i + 1]);
	}
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_hadds_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		__r[__i] = (short)__mcc_sat_sw((int)__x[2 * __i] + (int)__x[2 * __i + 1]);
		__r[__i + 4] = (short)__mcc_sat_sw((int)__y[2 * __i] + (int)__y[2 * __i + 1]);
	}
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_hsub_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		__r[__i] = (short)((unsigned short)__x[2 * __i] - (unsigned short)__x[2 * __i + 1]);
		__r[__i + 4] =
			(short)((unsigned short)__y[2 * __i] - (unsigned short)__y[2 * __i + 1]);
	}
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_hsub_epi32(__m128i __a, __m128i __b)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b, __r;
	int __i;
	for (__i = 0; __i < 2; __i++) {
		__r[__i] = (int)((unsigned int)__x[2 * __i] - (unsigned int)__x[2 * __i + 1]);
		__r[__i + 2] = (int)((unsigned int)__y[2 * __i] - (unsigned int)__y[2 * __i + 1]);
	}
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_hsubs_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		__r[__i] = (short)__mcc_sat_sw((int)__x[2 * __i] - (int)__x[2 * __i + 1]);
		__r[__i + 4] = (short)__mcc_sat_sw((int)__y[2 * __i] - (int)__y[2 * __i + 1]);
	}
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_hadd_pi16(__m64 __a, __m64 __b)
{
	__v4hi __x = (__v4hi)__a, __y = (__v4hi)__b, __r;
	int __i;
	for (__i = 0; __i < 2; __i++) {
		__r[__i] = (short)((unsigned short)__x[2 * __i] + (unsigned short)__x[2 * __i + 1]);
		__r[__i + 2] =
			(short)((unsigned short)__y[2 * __i] + (unsigned short)__y[2 * __i + 1]);
	}
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_hadd_pi32(__m64 __a, __m64 __b)
{
	__v2si __x = (__v2si)__a, __y = (__v2si)__b, __r;
	__r[0] = (int)((unsigned int)__x[0] + (unsigned int)__x[1]);
	__r[1] = (int)((unsigned int)__y[0] + (unsigned int)__y[1]);
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_hadds_pi16(__m64 __a, __m64 __b)
{
	__v4hi __x = (__v4hi)__a, __y = (__v4hi)__b, __r;
	int __i;
	for (__i = 0; __i < 2; __i++) {
		__r[__i] = (short)__mcc_sat_sw((int)__x[2 * __i] + (int)__x[2 * __i + 1]);
		__r[__i + 2] = (short)__mcc_sat_sw((int)__y[2 * __i] + (int)__y[2 * __i + 1]);
	}
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_hsub_pi16(__m64 __a, __m64 __b)
{
	__v4hi __x = (__v4hi)__a, __y = (__v4hi)__b, __r;
	int __i;
	for (__i = 0; __i < 2; __i++) {
		__r[__i] = (short)((unsigned short)__x[2 * __i] - (unsigned short)__x[2 * __i + 1]);
		__r[__i + 2] =
			(short)((unsigned short)__y[2 * __i] - (unsigned short)__y[2 * __i + 1]);
	}
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_hsub_pi32(__m64 __a, __m64 __b)
{
	__v2si __x = (__v2si)__a, __y = (__v2si)__b, __r;
	__r[0] = (int)((unsigned int)__x[0] - (unsigned int)__x[1]);
	__r[1] = (int)((unsigned int)__y[0] - (unsigned int)__y[1]);
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_hsubs_pi16(__m64 __a, __m64 __b)
{
	__v4hi __x = (__v4hi)__a, __y = (__v4hi)__b, __r;
	int __i;
	for (__i = 0; __i < 2; __i++) {
		__r[__i] = (short)__mcc_sat_sw((int)__x[2 * __i] - (int)__x[2 * __i + 1]);
		__r[__i + 2] = (short)__mcc_sat_sw((int)__y[2 * __i] - (int)__y[2 * __i + 1]);
	}
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_maddubs_epi16(__m128i __a, __m128i __b)
{
	__v16qu __x = (__v16qu)__a;
	__v16qs __y = (__v16qs)__b;
	__v8hi __r;
	int __i;
	for (__i = 0; __i < 8; __i++) {
		int __lo = (int)__x[2 * __i] * (int)__y[2 * __i];
		int __hi = (int)__x[2 * __i + 1] * (int)__y[2 * __i + 1];
		__r[__i] = (short)__mcc_sat_sw(__lo + __hi);
	}
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_maddubs_pi16(__m64 __a, __m64 __b)
{
	__v8qu __x = (__v8qu)__a;
	__v8qs __y = (__v8qs)__b;
	__v4hi __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		int __lo = (int)__x[2 * __i] * (int)__y[2 * __i];
		int __hi = (int)__x[2 * __i + 1] * (int)__y[2 * __i + 1];
		__r[__i] = (short)__mcc_sat_sw(__lo + __hi);
	}
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_mulhrs_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (short)(((((int)__x[__i] * (int)__y[__i]) >> 14) + 1) >> 1);
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_mulhrs_pi16(__m64 __a, __m64 __b)
{
	__v4hi __x = (__v4hi)__a, __y = (__v4hi)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (short)(((((int)__x[__i] * (int)__y[__i]) >> 14) + 1) >> 1);
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_shuffle_epi8(__m128i __a, __m128i __b)
{
	__v16qu __x = (__v16qu)__a, __y = (__v16qu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (__y[__i] & 0x80) ? 0 : __x[__y[__i] & 15];
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_shuffle_pi8(__m64 __a, __m64 __b)
{
	__v8qu __x = (__v8qu)__a, __y = (__v8qu)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (__y[__i] & 0x80) ? 0 : __x[__y[__i] & 7];
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_sign_epi8(__m128i __a, __m128i __b)
{
	__v16qs __x = (__v16qs)__a, __y = (__v16qs)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++) {
		if (__y[__i] < 0)
			__r[__i] = (signed char)(0u - (unsigned int)(int)__x[__i]);
		else if (__y[__i] == 0)
			__r[__i] = 0;
		else
			__r[__i] = __x[__i];
	}
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_sign_epi16(__m128i __a, __m128i __b)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++) {
		if (__y[__i] < 0)
			__r[__i] = (short)(0u - (unsigned int)(int)__x[__i]);
		else if (__y[__i] == 0)
			__r[__i] = 0;
		else
			__r[__i] = __x[__i];
	}
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m128i _mm_sign_epi32(__m128i __a, __m128i __b)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		if (__y[__i] < 0)
			__r[__i] = (int)(0u - (unsigned int)__x[__i]);
		else if (__y[__i] == 0)
			__r[__i] = 0;
		else
			__r[__i] = __x[__i];
	}
	return (__m128i)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_sign_pi8(__m64 __a, __m64 __b)
{
	__v8qs __x = (__v8qs)__a, __y = (__v8qs)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++) {
		if (__y[__i] < 0)
			__r[__i] = (signed char)(0u - (unsigned int)(int)__x[__i]);
		else if (__y[__i] == 0)
			__r[__i] = 0;
		else
			__r[__i] = __x[__i];
	}
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_sign_pi16(__m64 __a, __m64 __b)
{
	__v4hi __x = (__v4hi)__a, __y = (__v4hi)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		if (__y[__i] < 0)
			__r[__i] = (short)(0u - (unsigned int)(int)__x[__i]);
		else if (__y[__i] == 0)
			__r[__i] = 0;
		else
			__r[__i] = __x[__i];
	}
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m64 _mm_sign_pi32(__m64 __a, __m64 __b)
{
	__v2si __x = (__v2si)__a, __y = (__v2si)__b, __r;
	int __i;
	for (__i = 0; __i < 2; __i++) {
		if (__y[__i] < 0)
			__r[__i] = (int)(0u - (unsigned int)__x[__i]);
		else if (__y[__i] == 0)
			__r[__i] = 0;
		else
			__r[__i] = __x[__i];
	}
	return (__m64)__r;
}

__MCC_SSSE3_INLINE __m128i __mcc_alignr_epi8(__m128i __a, __m128i __b, int __n)
{
	__v16qu __x = (__v16qu)__a, __y = (__v16qu)__b, __r;
	int __i;
	if (__n > 31)
		return (__m128i)(__v2di){0, 0};
	for (__i = 0; __i < 16; __i++) {
		int __j = __i + __n;
		__r[__i] = __j < 16 ? __y[__j] : (__j < 32 ? __x[__j - 16] : 0);
	}
	return (__m128i)__r;
}

#define _mm_alignr_epi8(a, b, n) __mcc_alignr_epi8((a), (b), (int)(n))

__MCC_SSSE3_INLINE __m64 __mcc_alignr_pi8(__m64 __a, __m64 __b, int __n)
{
	__v8qu __x = (__v8qu)__a, __y = (__v8qu)__b, __r;
	int __i;
	if (__n > 15)
		return (__m64)(__v1di){0};
	for (__i = 0; __i < 8; __i++) {
		int __j = __i + __n;
		__r[__i] = __j < 8 ? __y[__j] : (__j < 16 ? __x[__j - 8] : 0);
	}
	return (__m64)__r;
}

#define _mm_alignr_pi8(a, b, n) __mcc_alignr_pi8((a), (b), (int)(n))

#endif
