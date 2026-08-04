#ifndef _AVX2INTRIN_H_INCLUDED
#define _AVX2INTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "avx2intrin.h is only supported on x86 targets"
#endif

#include <avxintrin.h>

#define __MCC_AVX2_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_AVX2_INLINE __m256i _mm256_add_epi8(__m256i __a, __m256i __b)
{
	return (__m256i)((__v32qu)__a + (__v32qu)__b);
}

__MCC_AVX2_INLINE __m256i _mm256_add_epi16(__m256i __a, __m256i __b)
{
	return (__m256i)((__v16hu)__a + (__v16hu)__b);
}

__MCC_AVX2_INLINE __m256i _mm256_add_epi32(__m256i __a, __m256i __b)
{
	return (__m256i)((__v8su)__a + (__v8su)__b);
}

__MCC_AVX2_INLINE __m256i _mm256_add_epi64(__m256i __a, __m256i __b)
{
	return (__m256i)((__v4du)__a + (__v4du)__b);
}

__MCC_AVX2_INLINE __m256i _mm256_sub_epi8(__m256i __a, __m256i __b)
{
	return (__m256i)((__v32qu)__a - (__v32qu)__b);
}

__MCC_AVX2_INLINE __m256i _mm256_sub_epi16(__m256i __a, __m256i __b)
{
	return (__m256i)((__v16hu)__a - (__v16hu)__b);
}

__MCC_AVX2_INLINE __m256i _mm256_sub_epi32(__m256i __a, __m256i __b)
{
	return (__m256i)((__v8su)__a - (__v8su)__b);
}

__MCC_AVX2_INLINE __m256i _mm256_sub_epi64(__m256i __a, __m256i __b)
{
	return (__m256i)((__v4du)__a - (__v4du)__b);
}

__MCC_AVX2_INLINE __m256i _mm256_adds_epi8(__m256i __a, __m256i __b)
{
	__v32qs __x = (__v32qs)__a, __y = (__v32qs)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = (signed char)__mcc_sat_sb((int)__x[__i] + (int)__y[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_adds_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (short)__mcc_sat_sw((int)__x[__i] + (int)__y[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_adds_epu8(__m256i __a, __m256i __b)
{
	__v32qu __x = (__v32qu)__a, __y = (__v32qu)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = (unsigned char)__mcc_sat_ub((int)__x[__i] + (int)__y[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_adds_epu16(__m256i __a, __m256i __b)
{
	__v16hu __x = (__v16hu)__a, __y = (__v16hu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (unsigned short)__mcc_sat_uw((int)__x[__i] + (int)__y[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_subs_epi8(__m256i __a, __m256i __b)
{
	__v32qs __x = (__v32qs)__a, __y = (__v32qs)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = (signed char)__mcc_sat_sb((int)__x[__i] - (int)__y[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_subs_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (short)__mcc_sat_sw((int)__x[__i] - (int)__y[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_subs_epu8(__m256i __a, __m256i __b)
{
	__v32qu __x = (__v32qu)__a, __y = (__v32qu)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = (unsigned char)__mcc_sat_ub((int)__x[__i] - (int)__y[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_subs_epu16(__m256i __a, __m256i __b)
{
	__v16hu __x = (__v16hu)__a, __y = (__v16hu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (unsigned short)__mcc_sat_uw((int)__x[__i] - (int)__y[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_avg_epu8(__m256i __a, __m256i __b)
{
	__v32qu __x = (__v32qu)__a, __y = (__v32qu)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = (unsigned char)(((unsigned int)__x[__i] + __y[__i] + 1) >> 1);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_avg_epu16(__m256i __a, __m256i __b)
{
	__v16hu __x = (__v16hu)__a, __y = (__v16hu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (unsigned short)(((unsigned int)__x[__i] + __y[__i] + 1) >> 1);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_abs_epi8(__m256i __a)
{
	__v32qs __x = (__v32qs)__a, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = (signed char)(__x[__i] < 0 ? (int)(0u - (unsigned int)(int)__x[__i])
						     : (int)__x[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_abs_epi16(__m256i __a)
{
	__v16hi __x = (__v16hi)__a, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (short)(__x[__i] < 0 ? (int)(0u - (unsigned int)(int)__x[__i])
						: (int)__x[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_abs_epi32(__m256i __a)
{
	__v8si __x = (__v8si)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (int)(__x[__i] < 0 ? 0u - (unsigned int)__x[__i] : (unsigned int)__x[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_max_epi8(__m256i __a, __m256i __b)
{
	__v32qs __x = (__v32qs)__a, __y = (__v32qs)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_min_epi8(__m256i __a, __m256i __b)
{
	__v32qs __x = (__v32qs)__a, __y = (__v32qs)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_max_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_min_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_max_epi32(__m256i __a, __m256i __b)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_min_epi32(__m256i __a, __m256i __b)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_max_epu8(__m256i __a, __m256i __b)
{
	__v32qu __x = (__v32qu)__a, __y = (__v32qu)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_min_epu8(__m256i __a, __m256i __b)
{
	__v32qu __x = (__v32qu)__a, __y = (__v32qu)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_max_epu16(__m256i __a, __m256i __b)
{
	__v16hu __x = (__v16hu)__a, __y = (__v16hu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_min_epu16(__m256i __a, __m256i __b)
{
	__v16hu __x = (__v16hu)__a, __y = (__v16hu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_max_epu32(__m256i __a, __m256i __b)
{
	__v8su __x = (__v8su)__a, __y = (__v8su)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_min_epu32(__m256i __a, __m256i __b)
{
	__v8su __x = (__v8su)__a, __y = (__v8su)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cmpeq_epi8(__m256i __a, __m256i __b)
{
	__v32qs __x = (__v32qs)__a, __y = (__v32qs)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = __x[__i] == __y[__i] ? -1 : 0;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cmpeq_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i] == __y[__i] ? -1 : 0;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cmpeq_epi32(__m256i __a, __m256i __b)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] == __y[__i] ? -1 : 0;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cmpeq_epi64(__m256i __a, __m256i __b)
{
	__v4di __x = (__v4di)__a, __y = (__v4di)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] == __y[__i] ? -1LL : 0LL;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cmpgt_epi8(__m256i __a, __m256i __b)
{
	__v32qs __x = (__v32qs)__a, __y = (__v32qs)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = __x[__i] > __y[__i] ? -1 : 0;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cmpgt_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i] > __y[__i] ? -1 : 0;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cmpgt_epi32(__m256i __a, __m256i __b)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] > __y[__i] ? -1 : 0;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cmpgt_epi64(__m256i __a, __m256i __b)
{
	__v4di __x = (__v4di)__a, __y = (__v4di)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] > __y[__i] ? -1LL : 0LL;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_mullo_epi16(__m256i __a, __m256i __b)
{
	__v16hu __x = (__v16hu)__a, __y = (__v16hu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (unsigned short)(__x[__i] * __y[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_mullo_epi32(__m256i __a, __m256i __b)
{
	__v8su __x = (__v8su)__a, __y = (__v8su)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] * __y[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_mulhi_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (short)(((int)__x[__i] * (int)__y[__i]) >> 16);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_mulhi_epu16(__m256i __a, __m256i __b)
{
	__v16hu __x = (__v16hu)__a, __y = (__v16hu)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (unsigned short)(((unsigned int)__x[__i] * (unsigned int)__y[__i]) >> 16);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_mulhrs_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (short)(((((int)__x[__i] * (int)__y[__i]) >> 14) + 1) >> 1);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_mul_epi32(__m256i __a, __m256i __b)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b;
	__v4di __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (long long)__x[2 * __i] * (long long)__y[2 * __i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_mul_epu32(__m256i __a, __m256i __b)
{
	__v8su __x = (__v8su)__a, __y = (__v8su)__b;
	__v4du __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (unsigned long long)__x[2 * __i] * (unsigned long long)__y[2 * __i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_madd_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b;
	__v8si __r;
	int __i;
	for (__i = 0; __i < 8; __i++) {
		unsigned int __lo = (unsigned int)((int)__x[2 * __i] * (int)__y[2 * __i]);
		unsigned int __hi = (unsigned int)((int)__x[2 * __i + 1] * (int)__y[2 * __i + 1]);
		__r[__i] = (int)(__lo + __hi);
	}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_maddubs_epi16(__m256i __a, __m256i __b)
{
	__v32qu __x = (__v32qu)__a;
	__v32qs __y = (__v32qs)__b;
	__v16hi __r;
	int __i;
	for (__i = 0; __i < 16; __i++) {
		int __lo = (int)__x[2 * __i] * (int)__y[2 * __i];
		int __hi = (int)__x[2 * __i + 1] * (int)__y[2 * __i + 1];
		__r[__i] = (short)__mcc_sat_sw(__lo + __hi);
	}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_sad_epu8(__m256i __a, __m256i __b)
{
	__v32qu __x = (__v32qu)__a, __y = (__v32qu)__b;
	__v4du __r;
	int __k, __j;
	for (__k = 0; __k < 4; __k++) {
		unsigned int __s = 0;
		for (__j = 0; __j < 8; __j++) {
			int __d = (int)__x[8 * __k + __j] - (int)__y[8 * __k + __j];
			__s += (unsigned int)(__d < 0 ? -__d : __d);
		}
		__r[__k] = __s;
	}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_hadd_epi16(__m256i __a, __m256i __b)
{
	__v16hu __x = (__v16hu)__a, __y = (__v16hu)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++) {
			__r[8 * __l + __i] =
				(unsigned short)(__x[8 * __l + 2 * __i] + __x[8 * __l + 2 * __i + 1]);
			__r[8 * __l + 4 + __i] =
				(unsigned short)(__y[8 * __l + 2 * __i] + __y[8 * __l + 2 * __i + 1]);
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_hsub_epi16(__m256i __a, __m256i __b)
{
	__v16hu __x = (__v16hu)__a, __y = (__v16hu)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++) {
			__r[8 * __l + __i] =
				(unsigned short)(__x[8 * __l + 2 * __i] - __x[8 * __l + 2 * __i + 1]);
			__r[8 * __l + 4 + __i] =
				(unsigned short)(__y[8 * __l + 2 * __i] - __y[8 * __l + 2 * __i + 1]);
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_hadds_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++) {
			__r[8 * __l + __i] = (short)__mcc_sat_sw(
				(int)__x[8 * __l + 2 * __i] + (int)__x[8 * __l + 2 * __i + 1]);
			__r[8 * __l + 4 + __i] = (short)__mcc_sat_sw(
				(int)__y[8 * __l + 2 * __i] + (int)__y[8 * __l + 2 * __i + 1]);
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_hsubs_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++) {
			__r[8 * __l + __i] = (short)__mcc_sat_sw(
				(int)__x[8 * __l + 2 * __i] - (int)__x[8 * __l + 2 * __i + 1]);
			__r[8 * __l + 4 + __i] = (short)__mcc_sat_sw(
				(int)__y[8 * __l + 2 * __i] - (int)__y[8 * __l + 2 * __i + 1]);
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_hadd_epi32(__m256i __a, __m256i __b)
{
	__v8su __x = (__v8su)__a, __y = (__v8su)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 2; __i++) {
			__r[4 * __l + __i] = __x[4 * __l + 2 * __i] + __x[4 * __l + 2 * __i + 1];
			__r[4 * __l + 2 + __i] = __y[4 * __l + 2 * __i] + __y[4 * __l + 2 * __i + 1];
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_hsub_epi32(__m256i __a, __m256i __b)
{
	__v8su __x = (__v8su)__a, __y = (__v8su)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 2; __i++) {
			__r[4 * __l + __i] = __x[4 * __l + 2 * __i] - __x[4 * __l + 2 * __i + 1];
			__r[4 * __l + 2 + __i] = __y[4 * __l + 2 * __i] - __y[4 * __l + 2 * __i + 1];
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_sign_epi8(__m256i __a, __m256i __b)
{
	__v32qs __x = (__v32qs)__a, __y = (__v32qs)__b, __r;
	int __i;
	for (__i = 0; __i < 32; __i++) {
		if (__y[__i] < 0)
			__r[__i] = (signed char)(0u - (unsigned int)(int)__x[__i]);
		else if (__y[__i] == 0)
			__r[__i] = 0;
		else
			__r[__i] = __x[__i];
	}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_sign_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++) {
		if (__y[__i] < 0)
			__r[__i] = (short)(0u - (unsigned int)(int)__x[__i]);
		else if (__y[__i] == 0)
			__r[__i] = 0;
		else
			__r[__i] = __x[__i];
	}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_sign_epi32(__m256i __a, __m256i __b)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++) {
		if (__y[__i] < 0)
			__r[__i] = (int)(0u - (unsigned int)__x[__i]);
		else if (__y[__i] == 0)
			__r[__i] = 0;
		else
			__r[__i] = __x[__i];
	}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_packs_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b;
	__v32qs __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 8; __i++) {
			__r[16 * __l + __i] = (signed char)__mcc_sat_sb((int)__x[8 * __l + __i]);
			__r[16 * __l + 8 + __i] = (signed char)__mcc_sat_sb((int)__y[8 * __l + __i]);
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_packs_epi32(__m256i __a, __m256i __b)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b;
	__v16hi __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++) {
			__r[8 * __l + __i] = (short)__mcc_sat_sw(__x[4 * __l + __i]);
			__r[8 * __l + 4 + __i] = (short)__mcc_sat_sw(__y[4 * __l + __i]);
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_packus_epi16(__m256i __a, __m256i __b)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b;
	__v32qu __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 8; __i++) {
			__r[16 * __l + __i] = (unsigned char)__mcc_sat_ub((int)__x[8 * __l + __i]);
			__r[16 * __l + 8 + __i] =
				(unsigned char)__mcc_sat_ub((int)__y[8 * __l + __i]);
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_packus_epi32(__m256i __a, __m256i __b)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b;
	__v16hu __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++) {
			__r[8 * __l + __i] = (unsigned short)__mcc_sat_uw(__x[4 * __l + __i]);
			__r[8 * __l + 4 + __i] = (unsigned short)__mcc_sat_uw(__y[4 * __l + __i]);
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_unpackhi_epi8(__m256i __a, __m256i __b)
{
	return (__m256i)__builtin_shufflevector(
		(__v32qi)__a, (__v32qi)__b, 8, 40, 9, 41, 10, 42, 11, 43, 12, 44, 13, 45, 14, 46, 15,
		47, 24, 56, 25, 57, 26, 58, 27, 59, 28, 60, 29, 61, 30, 62, 31, 63);
}

__MCC_AVX2_INLINE __m256i _mm256_unpacklo_epi8(__m256i __a, __m256i __b)
{
	return (__m256i)__builtin_shufflevector(
		(__v32qi)__a, (__v32qi)__b, 0, 32, 1, 33, 2, 34, 3, 35, 4, 36, 5, 37, 6, 38, 7, 39,
		16, 48, 17, 49, 18, 50, 19, 51, 20, 52, 21, 53, 22, 54, 23, 55);
}

__MCC_AVX2_INLINE __m256i _mm256_unpackhi_epi16(__m256i __a, __m256i __b)
{
	return (__m256i)__builtin_shufflevector((__v16hi)__a, (__v16hi)__b, 4, 20, 5, 21, 6, 22, 7,
						23, 12, 28, 13, 29, 14, 30, 15, 31);
}

__MCC_AVX2_INLINE __m256i _mm256_unpacklo_epi16(__m256i __a, __m256i __b)
{
	return (__m256i)__builtin_shufflevector((__v16hi)__a, (__v16hi)__b, 0, 16, 1, 17, 2, 18, 3,
						19, 8, 24, 9, 25, 10, 26, 11, 27);
}

__MCC_AVX2_INLINE __m256i _mm256_unpackhi_epi32(__m256i __a, __m256i __b)
{
	return (__m256i)__builtin_shufflevector((__v8si)__a, (__v8si)__b, 2, 10, 3, 11, 6, 14, 7,
						15);
}

__MCC_AVX2_INLINE __m256i _mm256_unpacklo_epi32(__m256i __a, __m256i __b)
{
	return (__m256i)__builtin_shufflevector((__v8si)__a, (__v8si)__b, 0, 8, 1, 9, 4, 12, 5, 13);
}

__MCC_AVX2_INLINE __m256i _mm256_unpackhi_epi64(__m256i __a, __m256i __b)
{
	return (__m256i)__builtin_shufflevector((__v4di)__a, (__v4di)__b, 1, 5, 3, 7);
}

__MCC_AVX2_INLINE __m256i _mm256_unpacklo_epi64(__m256i __a, __m256i __b)
{
	return (__m256i)__builtin_shufflevector((__v4di)__a, (__v4di)__b, 0, 4, 2, 6);
}

__MCC_AVX2_INLINE __m256i _mm256_shuffle_epi8(__m256i __a, __m256i __b)
{
	__v32qu __x = (__v32qu)__a, __y = (__v32qu)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 16; __i++) {
			unsigned char __k = __y[16 * __l + __i];
			__r[16 * __l + __i] = (__k & 0x80) ? 0 : __x[16 * __l + (__k & 15)];
		}
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i __mcc_shuffle_epi32_256(__m256i __a, int __imm)
{
	__v8si __x = (__v8si)__a, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++)
			__r[4 * __l + __i] = __x[4 * __l + ((__imm >> (2 * __i)) & 3)];
	return (__m256i)__r;
}

#define _mm256_shuffle_epi32(a, imm) __mcc_shuffle_epi32_256((a), (int)(imm))

__MCC_AVX2_INLINE __m256i __mcc_shufflehi_epi16_256(__m256i __a, int __imm)
{
	__v16hi __x = (__v16hi)__a, __r = __x;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++)
			__r[8 * __l + 4 + __i] = __x[8 * __l + 4 + ((__imm >> (2 * __i)) & 3)];
	return (__m256i)__r;
}

#define _mm256_shufflehi_epi16(a, imm) __mcc_shufflehi_epi16_256((a), (int)(imm))

__MCC_AVX2_INLINE __m256i __mcc_shufflelo_epi16_256(__m256i __a, int __imm)
{
	__v16hi __x = (__v16hi)__a, __r = __x;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++)
			__r[8 * __l + __i] = __x[8 * __l + ((__imm >> (2 * __i)) & 3)];
	return (__m256i)__r;
}

#define _mm256_shufflelo_epi16(a, imm) __mcc_shufflelo_epi16_256((a), (int)(imm))

__MCC_AVX2_INLINE __m256i __mcc_alignr_epi8_256(__m256i __a, __m256i __b, int __n)
{
	__v32qu __x = (__v32qu)__a, __y = (__v32qu)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 16; __i++) {
			int __j = __i + __n;
			__r[16 * __l + __i] =
				__n > 31 ? 0
					 : (__j < 16 ? __y[16 * __l + __j]
						     : (__j < 32 ? __x[16 * __l + __j - 16] : 0));
		}
	return (__m256i)__r;
}

#define _mm256_alignr_epi8(a, b, n) __mcc_alignr_epi8_256((a), (b), (int)(n))

__MCC_AVX2_INLINE __m256i __mcc_blend_epi16_256(__m256i __a, __m256i __b, int __imm)
{
	__v16hi __x = (__v16hi)__a, __y = (__v16hi)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 8; __i++)
			__r[8 * __l + __i] = (__imm >> __i) & 1 ? __y[8 * __l + __i]
							       : __x[8 * __l + __i];
	return (__m256i)__r;
}

#define _mm256_blend_epi16(a, b, imm) __mcc_blend_epi16_256((a), (b), (int)(imm))

__MCC_AVX2_INLINE __m256i __mcc_blend_epi32_256(__m256i __a, __m256i __b, int __imm)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (__imm >> __i) & 1 ? __y[__i] : __x[__i];
	return (__m256i)__r;
}

#define _mm256_blend_epi32(a, b, imm) __mcc_blend_epi32_256((a), (b), (int)(imm))

__MCC_AVX2_INLINE __m128i __mcc_blend_epi32_128(__m128i __a, __m128i __b, int __imm)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (__imm >> __i) & 1 ? __y[__i] : __x[__i];
	return (__m128i)__r;
}

#define _mm_blend_epi32(a, b, imm) __mcc_blend_epi32_128((a), (b), (int)(imm))

__MCC_AVX2_INLINE __m256i _mm256_blendv_epi8(__m256i __a, __m256i __b, __m256i __m)
{
	__v32qs __x = (__v32qs)__a, __y = (__v32qs)__b, __k = (__v32qs)__m, __r;
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = __k[__i] < 0 ? __y[__i] : __x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE int _mm256_movemask_epi8(__m256i __a)
{
	__v32qs __x = (__v32qs)__a;
	int __r = 0, __i;
	for (__i = 0; __i < 32; __i++)
		if (__x[__i] < 0)
			__r |= 1 << __i;
	return __r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepi8_epi16(__m128i __a)
{
	__v16qs __x = (__v16qs)__a;
	__v16hi __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepi8_epi32(__m128i __a)
{
	__v16qs __x = (__v16qs)__a;
	__v8si __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepi8_epi64(__m128i __a)
{
	__v16qs __x = (__v16qs)__a;
	__v4di __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepi16_epi32(__m128i __a)
{
	__v8hi __x = (__v8hi)__a;
	__v8si __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepi16_epi64(__m128i __a)
{
	__v8hi __x = (__v8hi)__a;
	__v4di __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepi32_epi64(__m128i __a)
{
	__v4si __x = (__v4si)__a;
	__v4di __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepu8_epi16(__m128i __a)
{
	__v16qu __x = (__v16qu)__a;
	__v16hi __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (short)__x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepu8_epi32(__m128i __a)
{
	__v16qu __x = (__v16qu)__a;
	__v8si __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (int)__x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepu8_epi64(__m128i __a)
{
	__v16qu __x = (__v16qu)__a;
	__v4di __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (long long)__x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepu16_epi32(__m128i __a)
{
	__v8hu __x = (__v8hu)__a;
	__v8si __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (int)__x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepu16_epi64(__m128i __a)
{
	__v8hu __x = (__v8hu)__a;
	__v4di __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (long long)__x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_cvtepu32_epi64(__m128i __a)
{
	__v4su __x = (__v4su)__a;
	__v4di __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (long long)__x[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_sll_epi16(__m256i __a, __m128i __c)
{
	unsigned long long __n = (unsigned long long)((__v2du)__c)[0];
	__v16hu __x = (__v16hu)__a, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __n > 15 ? 0 : (unsigned short)(__x[__i] << __n);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_sll_epi32(__m256i __a, __m128i __c)
{
	unsigned long long __n = (unsigned long long)((__v2du)__c)[0];
	__v8su __x = (__v8su)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __n > 31 ? 0 : __x[__i] << __n;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_sll_epi64(__m256i __a, __m128i __c)
{
	unsigned long long __n = (unsigned long long)((__v2du)__c)[0];
	__v4du __x = (__v4du)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __n > 63 ? 0 : __x[__i] << __n;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_srl_epi16(__m256i __a, __m128i __c)
{
	unsigned long long __n = (unsigned long long)((__v2du)__c)[0];
	__v16hu __x = (__v16hu)__a, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __n > 15 ? 0 : (unsigned short)(__x[__i] >> __n);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_srl_epi32(__m256i __a, __m128i __c)
{
	unsigned long long __n = (unsigned long long)((__v2du)__c)[0];
	__v8su __x = (__v8su)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __n > 31 ? 0 : __x[__i] >> __n;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_srl_epi64(__m256i __a, __m128i __c)
{
	unsigned long long __n = (unsigned long long)((__v2du)__c)[0];
	__v4du __x = (__v4du)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __n > 63 ? 0 : __x[__i] >> __n;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_sra_epi16(__m256i __a, __m128i __c)
{
	unsigned long long __n = (unsigned long long)((__v2du)__c)[0];
	__v16hi __x = (__v16hi)__a, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = (short)(__x[__i] >> (__n > 15 ? 15 : __n));
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_sra_epi32(__m256i __a, __m128i __c)
{
	unsigned long long __n = (unsigned long long)((__v2du)__c)[0];
	__v8si __x = (__v8si)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] >> (__n > 31 ? 31 : __n);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_slli_epi16(__m256i __a, int __n)
{
	return _mm256_sll_epi16(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__n, 0});
}

__MCC_AVX2_INLINE __m256i _mm256_slli_epi32(__m256i __a, int __n)
{
	return _mm256_sll_epi32(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__n, 0});
}

__MCC_AVX2_INLINE __m256i _mm256_slli_epi64(__m256i __a, int __n)
{
	return _mm256_sll_epi64(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__n, 0});
}

__MCC_AVX2_INLINE __m256i _mm256_srli_epi16(__m256i __a, int __n)
{
	return _mm256_srl_epi16(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__n, 0});
}

__MCC_AVX2_INLINE __m256i _mm256_srli_epi32(__m256i __a, int __n)
{
	return _mm256_srl_epi32(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__n, 0});
}

__MCC_AVX2_INLINE __m256i _mm256_srli_epi64(__m256i __a, int __n)
{
	return _mm256_srl_epi64(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__n, 0});
}

__MCC_AVX2_INLINE __m256i _mm256_srai_epi16(__m256i __a, int __n)
{
	return _mm256_sra_epi16(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__n, 0});
}

__MCC_AVX2_INLINE __m256i _mm256_srai_epi32(__m256i __a, int __n)
{
	return _mm256_sra_epi32(__a, (__m128i)(__v2du){(unsigned long long)(unsigned int)__n, 0});
}

__MCC_AVX2_INLINE __m256i _mm256_slli_si256(__m256i __a, int __imm)
{
	__v32qu __x = (__v32qu)__a, __r;
	int __n = (int)((unsigned int)__imm & 0xff), __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 16; __i++)
			__r[16 * __l + __i] =
				(__n > __i || __n > 15) ? 0 : __x[16 * __l + __i - __n];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_srli_si256(__m256i __a, int __imm)
{
	__v32qu __x = (__v32qu)__a, __r;
	int __n = (int)((unsigned int)__imm & 0xff), __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 16; __i++)
			__r[16 * __l + __i] =
				(__i + __n > 15) ? 0 : __x[16 * __l + __i + __n];
	return (__m256i)__r;
}

#define _mm256_bslli_epi128(a, imm) _mm256_slli_si256((a), (imm))
#define _mm256_bsrli_epi128(a, imm) _mm256_srli_si256((a), (imm))

__MCC_AVX2_INLINE __m256i _mm256_sllv_epi32(__m256i __a, __m256i __c)
{
	__v8su __x = (__v8su)__a, __k = (__v8su)__c, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __k[__i] > 31 ? 0 : __x[__i] << __k[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_srlv_epi32(__m256i __a, __m256i __c)
{
	__v8su __x = (__v8su)__a, __k = (__v8su)__c, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __k[__i] > 31 ? 0 : __x[__i] >> __k[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_srav_epi32(__m256i __a, __m256i __c)
{
	__v8si __x = (__v8si)__a, __r;
	__v8su __k = (__v8su)__c;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] >> (__k[__i] > 31 ? 31 : __k[__i]);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_sllv_epi64(__m256i __a, __m256i __c)
{
	__v4du __x = (__v4du)__a, __k = (__v4du)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __k[__i] > 63 ? 0 : __x[__i] << __k[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_srlv_epi64(__m256i __a, __m256i __c)
{
	__v4du __x = (__v4du)__a, __k = (__v4du)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __k[__i] > 63 ? 0 : __x[__i] >> __k[__i];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_sllv_epi32(__m128i __a, __m128i __c)
{
	__v4su __x = (__v4su)__a, __k = (__v4su)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __k[__i] > 31 ? 0 : __x[__i] << __k[__i];
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_srlv_epi32(__m128i __a, __m128i __c)
{
	__v4su __x = (__v4su)__a, __k = (__v4su)__c, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __k[__i] > 31 ? 0 : __x[__i] >> __k[__i];
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_srav_epi32(__m128i __a, __m128i __c)
{
	__v4si __x = (__v4si)__a, __r;
	__v4su __k = (__v4su)__c;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] >> (__k[__i] > 31 ? 31 : __k[__i]);
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_sllv_epi64(__m128i __a, __m128i __c)
{
	__v2du __x = (__v2du)__a, __k = (__v2du)__c, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __k[__i] > 63 ? 0 : __x[__i] << __k[__i];
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_srlv_epi64(__m128i __a, __m128i __c)
{
	__v2du __x = (__v2du)__a, __k = (__v2du)__c, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __k[__i] > 63 ? 0 : __x[__i] >> __k[__i];
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m256i __mcc_permute4x64_epi64(__m256i __a, int __imm)
{
	__v4di __x = (__v4di)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[(__imm >> (2 * __i)) & 3];
	return (__m256i)__r;
}

#define _mm256_permute4x64_epi64(a, imm) __mcc_permute4x64_epi64((a), (int)(imm))

__MCC_AVX2_INLINE __m256d __mcc_permute4x64_pd(__m256d __a, int __imm)
{
	__v4df __x = (__v4df)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[(__imm >> (2 * __i)) & 3];
	return (__m256d)__r;
}

#define _mm256_permute4x64_pd(a, imm) __mcc_permute4x64_pd((a), (int)(imm))

__MCC_AVX2_INLINE __m256i _mm256_permutevar8x32_epi32(__m256i __a, __m256i __c)
{
	__v8si __x = (__v8si)__a, __k = (__v8si)__c, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__k[__i] & 7];
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256 _mm256_permutevar8x32_ps(__m256 __a, __m256i __c)
{
	__v8sf __x = (__v8sf)__a, __r;
	__v8si __k = (__v8si)__c;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__k[__i] & 7];
	return (__m256)__r;
}

#define _mm256_permute2x128_si256(a, b, imm) _mm256_permute2f128_si256((a), (b), (imm))

__MCC_AVX2_INLINE __m128i __mcc_extracti128_si256(__m256i __a, int __imm)
{
	__v4di __x = (__v4di)__a;
	__v2di __r;
	int __o = (__imm & 1) * 2, __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __x[__o + __i];
	return (__m128i)__r;
}

#define _mm256_extracti128_si256(a, imm) __mcc_extracti128_si256((a), (int)(imm))

__MCC_AVX2_INLINE __m256i __mcc_inserti128_si256(__m256i __a, __m128i __b, int __imm)
{
	__v4di __r = (__v4di)__a;
	__v2di __y = (__v2di)__b;
	int __o = (__imm & 1) * 2, __i;
	for (__i = 0; __i < 2; __i++)
		__r[__o + __i] = __y[__i];
	return (__m256i)__r;
}

#define _mm256_inserti128_si256(a, b, imm) __mcc_inserti128_si256((a), (b), (int)(imm))

__MCC_AVX2_INLINE __m256i __mcc_mpsadbw_epu8_256(__m256i __a, __m256i __b, int __imm)
{
	__v32qu __x = (__v32qu)__a, __y = (__v32qu)__b;
	__v16hu __r;
	int __l, __i, __j;
	for (__l = 0; __l < 2; __l++) {
		int __c = (__imm >> (3 * __l)) & 7;
		int __oa = 16 * __l + ((__c >> 2) & 1) * 4, __ob = 16 * __l + (__c & 3) * 4;
		for (__i = 0; __i < 8; __i++) {
			int __s = 0;
			for (__j = 0; __j < 4; __j++) {
				int __d = (int)__x[__oa + __i + __j] - (int)__y[__ob + __j];
				__s += __d < 0 ? -__d : __d;
			}
			__r[8 * __l + __i] = (unsigned short)__s;
		}
	}
	return (__m256i)__r;
}

#define _mm256_mpsadbw_epu8(a, b, imm) __mcc_mpsadbw_epu8_256((a), (b), (int)(imm))

__MCC_AVX2_INLINE __m256i _mm256_broadcastsi128_si256(__m128i __a)
{
	return (__m256i)__builtin_shufflevector((__v2di)__a, (__v2di)__a, 0, 1, 0, 1);
}

#define _mm_broadcastsi128_si256(a) _mm256_broadcastsi128_si256(a)

__MCC_AVX2_INLINE __m256i _mm256_broadcastb_epi8(__m128i __a)
{
	__v32qi __r;
	char __v = ((__v16qi)__a)[0];
	int __i;
	for (__i = 0; __i < 32; __i++)
		__r[__i] = __v;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_broadcastw_epi16(__m128i __a)
{
	__v16hi __r;
	short __v = ((__v8hi)__a)[0];
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __v;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_broadcastd_epi32(__m128i __a)
{
	__v8si __r;
	int __v = ((__v4si)__a)[0], __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __v;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_broadcastq_epi64(__m128i __a)
{
	__v4di __r;
	long long __v = ((__v2di)__a)[0];
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __v;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256 _mm256_broadcastss_ps(__m128 __a)
{
	__v8sf __r;
	float __v = ((__v4sf)__a)[0];
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __v;
	return (__m256)__r;
}

__MCC_AVX2_INLINE __m256d _mm256_broadcastsd_pd(__m128d __a)
{
	__v4df __r;
	double __v = ((__v2df)__a)[0];
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __v;
	return (__m256d)__r;
}

__MCC_AVX2_INLINE __m128i _mm_broadcastb_epi8(__m128i __a)
{
	__v16qi __r;
	char __v = ((__v16qi)__a)[0];
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __v;
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_broadcastw_epi16(__m128i __a)
{
	__v8hi __r;
	short __v = ((__v8hi)__a)[0];
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __v;
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_broadcastd_epi32(__m128i __a)
{
	__v4si __r;
	int __v = ((__v4si)__a)[0], __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __v;
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_broadcastq_epi64(__m128i __a)
{
	__v2di __r;
	long long __v = ((__v2di)__a)[0];
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __v;
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128 _mm_broadcastss_ps(__m128 __a)
{
	__v4sf __r;
	float __v = ((__v4sf)__a)[0];
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __v;
	return (__m128)__r;
}

__MCC_AVX2_INLINE __m128d _mm_broadcastsd_pd(__m128d __a)
{
	__v2df __r;
	double __v = ((__v2df)__a)[0];
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __v;
	return (__m128d)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_stream_load_si256(__m256i const *__p)
{
	return *(__m256i *)__p;
}

__MCC_AVX2_INLINE __m256i _mm256_maskload_epi32(int const *__p, __m256i __m)
{
	__v8si __k = (__v8si)__m, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __k[__i] < 0 ? ((const struct __mcc_loadu_i32 *)(__p + __i))->__v : 0;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_maskload_epi32(int const *__p, __m128i __m)
{
	__v4si __k = (__v4si)__m, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __k[__i] < 0 ? ((const struct __mcc_loadu_i32 *)(__p + __i))->__v : 0;
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_maskload_epi64(long long const *__p, __m256i __m)
{
	__v4di __k = (__v4di)__m, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __k[__i] < 0 ? ((const struct __mcc_loadu_si64 *)(__p + __i))->__v : 0;
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_maskload_epi64(long long const *__p, __m128i __m)
{
	__v2di __k = (__v2di)__m, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __k[__i] < 0 ? ((const struct __mcc_loadu_si64 *)(__p + __i))->__v : 0;
	return (__m128i)__r;
}

__MCC_AVX2_INLINE void _mm256_maskstore_epi32(int *__p, __m256i __m, __m256i __a)
{
	__v8si __k = (__v8si)__m, __x = (__v8si)__a;
	int __i;
	for (__i = 0; __i < 8; __i++)
		if (__k[__i] < 0)
			((struct __mcc_loadu_i32 *)(__p + __i))->__v = __x[__i];
}

__MCC_AVX2_INLINE void _mm_maskstore_epi32(int *__p, __m128i __m, __m128i __a)
{
	__v4si __k = (__v4si)__m, __x = (__v4si)__a;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__k[__i] < 0)
			((struct __mcc_loadu_i32 *)(__p + __i))->__v = __x[__i];
}

__MCC_AVX2_INLINE void _mm256_maskstore_epi64(long long *__p, __m256i __m, __m256i __a)
{
	__v4di __k = (__v4di)__m, __x = (__v4di)__a;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__k[__i] < 0)
			((struct __mcc_loadu_si64 *)(__p + __i))->__v = __x[__i];
}

__MCC_AVX2_INLINE void _mm_maskstore_epi64(long long *__p, __m128i __m, __m128i __a)
{
	__v2di __k = (__v2di)__m, __x = (__v2di)__a;
	int __i;
	for (__i = 0; __i < 2; __i++)
		if (__k[__i] < 0)
			((struct __mcc_loadu_si64 *)(__p + __i))->__v = __x[__i];
}

__MCC_AVX2_INLINE const char *__mcc_gaddr(const void *__b, long long __i, int __s)
{
	return (const char *)__b + __i * (long long)__s;
}

__MCC_AVX2_INLINE float __mcc_gld_ss(const float *__b, long long __i, int __s)
{
	return ((const struct __mcc_loadu_f32 *)__mcc_gaddr(__b, __i, __s))->__v;
}

__MCC_AVX2_INLINE double __mcc_gld_sd(const double *__b, long long __i, int __s)
{
	return ((const struct __mcc_loadu_f64 *)__mcc_gaddr(__b, __i, __s))->__v;
}

__MCC_AVX2_INLINE int __mcc_gld_si(const int *__b, long long __i, int __s)
{
	return ((const struct __mcc_loadu_i32 *)__mcc_gaddr(__b, __i, __s))->__v;
}

__MCC_AVX2_INLINE long long __mcc_gld_sq(const long long *__b, long long __i, int __s)
{
	return ((const struct __mcc_loadu_si64 *)__mcc_gaddr(__b, __i, __s))->__v;
}

__MCC_AVX2_INLINE __m128d _mm_mask_i32gather_pd(__m128d __src, const double *__base,
																								__m128i __index, __m128d __mask,
																								const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v2di __mk = (__v2di)__mask;
	__v2df __r = (__v2df)__src;
	int __i;
	for (__i = 0; __i < 2; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_sd(__base, (long long)__ix[__i], __scale);
	return (__m128d)__r;
}

__MCC_AVX2_INLINE __m128d _mm_i32gather_pd(const double *__base, __m128i __index,
																					 const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v2df __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_gld_sd(__base, (long long)__ix[__i], __scale);
	return (__m128d)__r;
}

__MCC_AVX2_INLINE __m256d _mm256_mask_i32gather_pd(__m256d __src, const double *__base,
																									 __m128i __index, __m256d __mask,
																									 const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v4di __mk = (__v4di)__mask;
	__v4df __r = (__v4df)__src;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_sd(__base, (long long)__ix[__i], __scale);
	return (__m256d)__r;
}

__MCC_AVX2_INLINE __m256d _mm256_i32gather_pd(const double *__base, __m128i __index,
																							const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v4df __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_gld_sd(__base, (long long)__ix[__i], __scale);
	return (__m256d)__r;
}

__MCC_AVX2_INLINE __m128d _mm_mask_i64gather_pd(__m128d __src, const double *__base,
																								__m128i __index, __m128d __mask,
																								const int __scale)
{
	__v2di __ix = (__v2di)__index;
	__v2di __mk = (__v2di)__mask;
	__v2df __r = (__v2df)__src;
	int __i;
	for (__i = 0; __i < 2; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_sd(__base, __ix[__i], __scale);
	return (__m128d)__r;
}

__MCC_AVX2_INLINE __m128d _mm_i64gather_pd(const double *__base, __m128i __index,
																					 const int __scale)
{
	__v2di __ix = (__v2di)__index;
	__v2df __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_gld_sd(__base, __ix[__i], __scale);
	return (__m128d)__r;
}

__MCC_AVX2_INLINE __m256d _mm256_mask_i64gather_pd(__m256d __src, const double *__base,
																									 __m256i __index, __m256d __mask,
																									 const int __scale)
{
	__v4di __ix = (__v4di)__index;
	__v4di __mk = (__v4di)__mask;
	__v4df __r = (__v4df)__src;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_sd(__base, __ix[__i], __scale);
	return (__m256d)__r;
}

__MCC_AVX2_INLINE __m256d _mm256_i64gather_pd(const double *__base, __m256i __index,
																							const int __scale)
{
	__v4di __ix = (__v4di)__index;
	__v4df __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_gld_sd(__base, __ix[__i], __scale);
	return (__m256d)__r;
}

__MCC_AVX2_INLINE __m128 _mm_mask_i32gather_ps(__m128 __src, const float *__base,
																							 __m128i __index, __m128 __mask,
																							 const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v4si __mk = (__v4si)__mask;
	__v4sf __r = (__v4sf)__src;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_ss(__base, (long long)__ix[__i], __scale);
	return (__m128)__r;
}

__MCC_AVX2_INLINE __m128 _mm_i32gather_ps(const float *__base, __m128i __index,
																					const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v4sf __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_gld_ss(__base, (long long)__ix[__i], __scale);
	return (__m128)__r;
}

__MCC_AVX2_INLINE __m256 _mm256_mask_i32gather_ps(__m256 __src, const float *__base,
																									__m256i __index, __m256 __mask,
																									const int __scale)
{
	__v8si __ix = (__v8si)__index;
	__v8si __mk = (__v8si)__mask;
	__v8sf __r = (__v8sf)__src;
	int __i;
	for (__i = 0; __i < 8; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_ss(__base, (long long)__ix[__i], __scale);
	return (__m256)__r;
}

__MCC_AVX2_INLINE __m256 _mm256_i32gather_ps(const float *__base, __m256i __index,
																						 const int __scale)
{
	__v8si __ix = (__v8si)__index;
	__v8sf __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_gld_ss(__base, (long long)__ix[__i], __scale);
	return (__m256)__r;
}

__MCC_AVX2_INLINE __m128 _mm_mask_i64gather_ps(__m128 __src, const float *__base,
																							 __m128i __index, __m128 __mask,
																							 const int __scale)
{
	__v2di __ix = (__v2di)__index;
	__v4si __mk = (__v4si)__mask;
	__v4sf __s = (__v4sf)__src, __r = (__v4sf){0, 0, 0, 0};
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mk[__i] < 0 ? __mcc_gld_ss(__base, __ix[__i], __scale) : __s[__i];
	return (__m128)__r;
}

__MCC_AVX2_INLINE __m128 _mm_i64gather_ps(const float *__base, __m128i __index,
																					const int __scale)
{
	__v2di __ix = (__v2di)__index;
	__v4sf __r = (__v4sf){0, 0, 0, 0};
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_gld_ss(__base, __ix[__i], __scale);
	return (__m128)__r;
}

__MCC_AVX2_INLINE __m128 _mm256_mask_i64gather_ps(__m128 __src, const float *__base,
																									__m256i __index, __m128 __mask,
																									const int __scale)
{
	__v4di __ix = (__v4di)__index;
	__v4si __mk = (__v4si)__mask;
	__v4sf __r = (__v4sf)__src;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_ss(__base, __ix[__i], __scale);
	return (__m128)__r;
}

__MCC_AVX2_INLINE __m128 _mm256_i64gather_ps(const float *__base, __m256i __index,
																						 const int __scale)
{
	__v4di __ix = (__v4di)__index;
	__v4sf __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_gld_ss(__base, __ix[__i], __scale);
	return (__m128)__r;
}

__MCC_AVX2_INLINE __m128i _mm_mask_i32gather_epi64(__m128i __src, const long long *__base,
																									 __m128i __index, __m128i __mask,
																									 const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v2di __mk = (__v2di)__mask;
	__v2di __r = (__v2di)__src;
	int __i;
	for (__i = 0; __i < 2; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_sq(__base, (long long)__ix[__i], __scale);
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_i32gather_epi64(const long long *__base, __m128i __index,
																							const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v2di __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_gld_sq(__base, (long long)__ix[__i], __scale);
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_mask_i32gather_epi64(__m256i __src, const long long *__base,
																											__m128i __index, __m256i __mask,
																											const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v4di __mk = (__v4di)__mask;
	__v4di __r = (__v4di)__src;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_sq(__base, (long long)__ix[__i], __scale);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_i32gather_epi64(const long long *__base, __m128i __index,
																								 const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v4di __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_gld_sq(__base, (long long)__ix[__i], __scale);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_mask_i64gather_epi64(__m128i __src, const long long *__base,
																									 __m128i __index, __m128i __mask,
																									 const int __scale)
{
	__v2di __ix = (__v2di)__index;
	__v2di __mk = (__v2di)__mask;
	__v2di __r = (__v2di)__src;
	int __i;
	for (__i = 0; __i < 2; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_sq(__base, __ix[__i], __scale);
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_i64gather_epi64(const long long *__base, __m128i __index,
																							const int __scale)
{
	__v2di __ix = (__v2di)__index;
	__v2di __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_gld_sq(__base, __ix[__i], __scale);
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_mask_i64gather_epi64(__m256i __src, const long long *__base,
																											__m256i __index, __m256i __mask,
																											const int __scale)
{
	__v4di __ix = (__v4di)__index;
	__v4di __mk = (__v4di)__mask;
	__v4di __r = (__v4di)__src;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_sq(__base, __ix[__i], __scale);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_i64gather_epi64(const long long *__base, __m256i __index,
																								 const int __scale)
{
	__v4di __ix = (__v4di)__index;
	__v4di __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_gld_sq(__base, __ix[__i], __scale);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_mask_i32gather_epi32(__m128i __src, const int *__base,
																									 __m128i __index, __m128i __mask,
																									 const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v4si __mk = (__v4si)__mask;
	__v4si __r = (__v4si)__src;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_si(__base, (long long)__ix[__i], __scale);
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_i32gather_epi32(const int *__base, __m128i __index,
																							const int __scale)
{
	__v4si __ix = (__v4si)__index;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_gld_si(__base, (long long)__ix[__i], __scale);
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_mask_i32gather_epi32(__m256i __src, const int *__base,
																											__m256i __index, __m256i __mask,
																											const int __scale)
{
	__v8si __ix = (__v8si)__index;
	__v8si __mk = (__v8si)__mask;
	__v8si __r = (__v8si)__src;
	int __i;
	for (__i = 0; __i < 8; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_si(__base, (long long)__ix[__i], __scale);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m256i _mm256_i32gather_epi32(const int *__base, __m256i __index,
																								 const int __scale)
{
	__v8si __ix = (__v8si)__index;
	__v8si __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_gld_si(__base, (long long)__ix[__i], __scale);
	return (__m256i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_mask_i64gather_epi32(__m128i __src, const int *__base,
																									 __m128i __index, __m128i __mask,
																									 const int __scale)
{
	__v2di __ix = (__v2di)__index;
	__v4si __mk = (__v4si)__mask;
	__v4si __s = (__v4si)__src, __r = (__v4si){0, 0, 0, 0};
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mk[__i] < 0 ? __mcc_gld_si(__base, __ix[__i], __scale) : __s[__i];
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm_i64gather_epi32(const int *__base, __m128i __index,
																							const int __scale)
{
	__v2di __ix = (__v2di)__index;
	__v4si __r = (__v4si){0, 0, 0, 0};
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_gld_si(__base, __ix[__i], __scale);
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm256_mask_i64gather_epi32(__m128i __src, const int *__base,
																											__m256i __index, __m128i __mask,
																											const int __scale)
{
	__v4di __ix = (__v4di)__index;
	__v4si __mk = (__v4si)__mask;
	__v4si __r = (__v4si)__src;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__mk[__i] < 0)
			__r[__i] = __mcc_gld_si(__base, __ix[__i], __scale);
	return (__m128i)__r;
}

__MCC_AVX2_INLINE __m128i _mm256_i64gather_epi32(const int *__base, __m256i __index,
																								 const int __scale)
{
	__v4di __ix = (__v4di)__index;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_gld_si(__base, __ix[__i], __scale);
	return (__m128i)__r;
}

#endif
