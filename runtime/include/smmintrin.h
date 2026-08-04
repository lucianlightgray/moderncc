#ifndef _SMMINTRIN_H_INCLUDED
#define _SMMINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "smmintrin.h is only supported on x86 targets"
#endif

#include <tmmintrin.h>

#define __MCC_SSE4_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

#define _MM_FROUND_TO_NEAREST_INT 0x00
#define _MM_FROUND_TO_NEG_INF 0x01
#define _MM_FROUND_TO_POS_INF 0x02
#define _MM_FROUND_TO_ZERO 0x03
#define _MM_FROUND_CUR_DIRECTION 0x04

#define _MM_FROUND_RAISE_EXC 0x00
#define _MM_FROUND_NO_EXC 0x08

#define _MM_FROUND_NINT (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_FLOOR (_MM_FROUND_TO_NEG_INF | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_CEIL (_MM_FROUND_TO_POS_INF | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_TRUNC (_MM_FROUND_TO_ZERO | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_RINT (_MM_FROUND_CUR_DIRECTION | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_NEARBYINT (_MM_FROUND_CUR_DIRECTION | _MM_FROUND_NO_EXC)

__MCC_SSE4_INLINE double __mcc_rnd_f64(double __x, int __m)
{
	double __t, __d;
	if (!(__x > -4503599627370496.0 && __x < 4503599627370496.0))
		return __x;
	if (__x == 0.0)
		return __x;
	__t = (double)(long long)__x;
	__d = __x - __t;
	if (__m == 1) {
		if (__d < 0.0)
			__t = __t - 1.0;
	} else if (__m == 2) {
		if (__d > 0.0)
			__t = __t + 1.0;
	} else if (__m == 0) {
		if (__d > 0.5 || (__d == 0.5 && ((long long)__t & 1)))
			__t = __t + 1.0;
		else if (__d < -0.5 || (__d == -0.5 && ((long long)__t & 1)))
			__t = __t - 1.0;
	}
	if (__t == 0.0 && __x < 0.0)
		return -0.0;
	return __t;
}

__MCC_SSE4_INLINE float __mcc_rnd_f32(float __x, int __m)
{
	float __t, __d;
	if (!(__x > -8388608.0f && __x < 8388608.0f))
		return __x;
	if (__x == 0.0f)
		return __x;
	__t = (float)(long long)__x;
	__d = __x - __t;
	if (__m == 1) {
		if (__d < 0.0f)
			__t = __t - 1.0f;
	} else if (__m == 2) {
		if (__d > 0.0f)
			__t = __t + 1.0f;
	} else if (__m == 0) {
		if (__d > 0.5f || (__d == 0.5f && ((long long)__t & 1)))
			__t = __t + 1.0f;
		else if (__d < -0.5f || (__d == -0.5f && ((long long)__t & 1)))
			__t = __t - 1.0f;
	}
	if (__t == 0.0f && __x < 0.0f)
		return -0.0f;
	return __t;
}

__MCC_SSE4_INLINE int __mcc_rnd_mode(int __imm)
{
	if (__imm & 4)
		return (int)((_mm_getcsr() >> 13) & 3);
	return __imm & 3;
}

__MCC_SSE4_INLINE __m128 _mm_round_ps(__m128 __a, const int __imm)
{
	__v4sf __x = (__v4sf)__a, __r;
	int __m = __mcc_rnd_mode(__imm), __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_rnd_f32(__x[__i], __m);
	return (__m128)__r;
}

__MCC_SSE4_INLINE __m128d _mm_round_pd(__m128d __a, const int __imm)
{
	__v2df __x = (__v2df)__a, __r;
	int __m = __mcc_rnd_mode(__imm), __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_rnd_f64(__x[__i], __m);
	return (__m128d)__r;
}

__MCC_SSE4_INLINE __m128 _mm_round_ss(__m128 __a, __m128 __b, const int __imm)
{
	__v4sf __r = (__v4sf)__a;
	__r[0] = __mcc_rnd_f32(((__v4sf)__b)[0], __mcc_rnd_mode(__imm));
	return (__m128)__r;
}

__MCC_SSE4_INLINE __m128d _mm_round_sd(__m128d __a, __m128d __b, const int __imm)
{
	__v2df __r = (__v2df)__a;
	__r[0] = __mcc_rnd_f64(((__v2df)__b)[0], __mcc_rnd_mode(__imm));
	return (__m128d)__r;
}

#define _mm_ceil_ps(V) _mm_round_ps((V), _MM_FROUND_CEIL)
#define _mm_ceil_pd(V) _mm_round_pd((V), _MM_FROUND_CEIL)
#define _mm_ceil_ss(D, V) _mm_round_ss((D), (V), _MM_FROUND_CEIL)
#define _mm_ceil_sd(D, V) _mm_round_sd((D), (V), _MM_FROUND_CEIL)

#define _mm_floor_ps(V) _mm_round_ps((V), _MM_FROUND_FLOOR)
#define _mm_floor_pd(V) _mm_round_pd((V), _MM_FROUND_FLOOR)
#define _mm_floor_ss(D, V) _mm_round_ss((D), (V), _MM_FROUND_FLOOR)
#define _mm_floor_sd(D, V) _mm_round_sd((D), (V), _MM_FROUND_FLOOR)

__MCC_SSE4_INLINE __m128i __mcc_blend_epi16(__m128i __a, __m128i __b, int __imm)
{
	__v8hi __x = (__v8hi)__a, __y = (__v8hi)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (__imm >> __i) & 1 ? __y[__i] : __x[__i];
	return (__m128i)__r;
}

#define _mm_blend_epi16(a, b, imm) __mcc_blend_epi16((a), (b), (int)(imm))

__MCC_SSE4_INLINE __m128 __mcc_blend_ps(__m128 __a, __m128 __b, int __imm)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (__imm >> __i) & 1 ? __y[__i] : __x[__i];
	return (__m128)__r;
}

#define _mm_blend_ps(a, b, imm) __mcc_blend_ps((a), (b), (int)(imm))

__MCC_SSE4_INLINE __m128d __mcc_blend_pd(__m128d __a, __m128d __b, int __imm)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = (__imm >> __i) & 1 ? __y[__i] : __x[__i];
	return (__m128d)__r;
}

#define _mm_blend_pd(a, b, imm) __mcc_blend_pd((a), (b), (int)(imm))

__MCC_SSE4_INLINE __m128i _mm_blendv_epi8(__m128i __a, __m128i __b, __m128i __m)
{
	__v16qs __x = (__v16qs)__a, __y = (__v16qs)__b, __k = (__v16qs)__m, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __k[__i] < 0 ? __y[__i] : __x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128 _mm_blendv_ps(__m128 __a, __m128 __b, __m128 __m)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __r;
	__v4si __k = (__v4si)__m;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __k[__i] < 0 ? __y[__i] : __x[__i];
	return (__m128)__r;
}

__MCC_SSE4_INLINE __m128d _mm_blendv_pd(__m128d __a, __m128d __b, __m128d __m)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __r;
	__v2di __k = (__v2di)__m;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __k[__i] < 0 ? __y[__i] : __x[__i];
	return (__m128d)__r;
}

__MCC_SSE4_INLINE __m128i _mm_max_epi8(__m128i __a, __m128i __b)
{
	__v16qs __x = (__v16qs)__a, __y = (__v16qs)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_min_epi8(__m128i __a, __m128i __b)
{
	__v16qs __x = (__v16qs)__a, __y = (__v16qs)__b, __r;
	int __i;
	for (__i = 0; __i < 16; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_max_epu16(__m128i __a, __m128i __b)
{
	__v8hu __x = (__v8hu)__a, __y = (__v8hu)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_min_epu16(__m128i __a, __m128i __b)
{
	__v8hu __x = (__v8hu)__a, __y = (__v8hu)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_max_epi32(__m128i __a, __m128i __b)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_min_epi32(__m128i __a, __m128i __b)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_max_epu32(__m128i __a, __m128i __b)
{
	__v4su __x = (__v4su)__a, __y = (__v4su)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_min_epu32(__m128i __a, __m128i __b)
{
	__v4su __x = (__v4su)__a, __y = (__v4su)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_mullo_epi32(__m128i __a, __m128i __b)
{
	__v4su __x = (__v4su)__a, __y = (__v4su)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] * __y[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_mul_epi32(__m128i __a, __m128i __b)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b;
	__v2di __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = (long long)__x[2 * __i] * (long long)__y[2 * __i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_packus_epi32(__m128i __a, __m128i __b)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b;
	__v8hu __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		__r[__i] = (unsigned short)__mcc_sat_uw(__x[__i]);
		__r[__i + 4] = (unsigned short)__mcc_sat_uw(__y[__i]);
	}
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cmpeq_epi64(__m128i __a, __m128i __b)
{
	__v2di __x = (__v2di)__a, __y = (__v2di)__b, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __x[__i] == __y[__i] ? -1LL : 0LL;
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cmpgt_epi64(__m128i __a, __m128i __b)
{
	__v2di __x = (__v2di)__a, __y = (__v2di)__b, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __x[__i] > __y[__i] ? -1LL : 0LL;
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepi8_epi16(__m128i __a)
{
	__v16qs __x = (__v16qs)__a;
	__v8hi __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepi8_epi32(__m128i __a)
{
	__v16qs __x = (__v16qs)__a;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepi8_epi64(__m128i __a)
{
	__v16qs __x = (__v16qs)__a;
	__v2di __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepi16_epi32(__m128i __a)
{
	__v8hi __x = (__v8hi)__a;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepi16_epi64(__m128i __a)
{
	__v8hi __x = (__v8hi)__a;
	__v2di __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepi32_epi64(__m128i __a)
{
	__v4si __x = (__v4si)__a;
	__v2di __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepu8_epi16(__m128i __a)
{
	__v16qu __x = (__v16qu)__a;
	__v8hi __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (short)__x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepu8_epi32(__m128i __a)
{
	__v16qu __x = (__v16qu)__a;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (int)__x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepu8_epi64(__m128i __a)
{
	__v16qu __x = (__v16qu)__a;
	__v2di __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = (long long)__x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepu16_epi32(__m128i __a)
{
	__v8hu __x = (__v8hu)__a;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (int)__x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepu16_epi64(__m128i __a)
{
	__v8hu __x = (__v8hu)__a;
	__v2di __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = (long long)__x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i _mm_cvtepu32_epi64(__m128i __a)
{
	__v4su __x = (__v4su)__a;
	__v2di __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = (long long)__x[__i];
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128 __mcc_dp_ps(__m128 __a, __m128 __b, int __imm)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __t, __r;
	float __s;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__t[__i] = (__imm >> (4 + __i)) & 1 ? __x[__i] * __y[__i] : 0.0f;
	__s = (__t[0] + __t[1]) + (__t[2] + __t[3]);
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (__imm >> __i) & 1 ? __s : 0.0f;
	return (__m128)__r;
}

#define _mm_dp_ps(a, b, imm) __mcc_dp_ps((a), (b), (int)(imm))

__MCC_SSE4_INLINE __m128d __mcc_dp_pd(__m128d __a, __m128d __b, int __imm)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __t, __r;
	double __s;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__t[__i] = (__imm >> (4 + __i)) & 1 ? __x[__i] * __y[__i] : 0.0;
	__s = __t[0] + __t[1];
	for (__i = 0; __i < 2; __i++)
		__r[__i] = (__imm >> __i) & 1 ? __s : 0.0;
	return (__m128d)__r;
}

#define _mm_dp_pd(a, b, imm) __mcc_dp_pd((a), (b), (int)(imm))

#define _mm_extract_epi8(a, imm) ((int)(unsigned char)((__v16qu)(__m128i)(a))[(imm) & 15])
#define _mm_extract_epi32(a, imm) ((int)((__v4si)(__m128i)(a))[(imm) & 3])
#define _mm_extract_ps(a, imm) ((int)((__v4si)(__m128)(a))[(imm) & 3])

#define _MM_EXTRACT_FLOAT(D, S, N) (__extension__({ (D) = ((__v4sf)(__m128)(S))[(N)]; }))

#ifdef __x86_64__
#define _mm_extract_epi64(a, imm) ((long long)((__v2di)(__m128i)(a))[(imm) & 1])
#endif

__MCC_SSE4_INLINE __m128i __mcc_insert_epi8(__m128i __a, int __b, int __imm)
{
	__v16qu __r = (__v16qu)__a;
	__r[__imm & 15] = (unsigned char)__b;
	return (__m128i)__r;
}

#define _mm_insert_epi8(a, b, imm) __mcc_insert_epi8((a), (int)(b), (int)(imm))

__MCC_SSE4_INLINE __m128i __mcc_insert_epi32(__m128i __a, int __b, int __imm)
{
	__v4si __r = (__v4si)__a;
	__r[__imm & 3] = __b;
	return (__m128i)__r;
}

#define _mm_insert_epi32(a, b, imm) __mcc_insert_epi32((a), (int)(b), (int)(imm))

#ifdef __x86_64__
__MCC_SSE4_INLINE __m128i __mcc_insert_epi64(__m128i __a, long long __b, int __imm)
{
	__v2di __r = (__v2di)__a;
	__r[__imm & 1] = __b;
	return (__m128i)__r;
}

#define _mm_insert_epi64(a, b, imm) __mcc_insert_epi64((a), (long long)(b), (int)(imm))
#endif

__MCC_SSE4_INLINE __m128 __mcc_insert_ps(__m128 __a, __m128 __b, int __imm)
{
	__v4sf __r = (__v4sf)__a, __y = (__v4sf)__b;
	int __i;
	__r[(__imm >> 4) & 3] = __y[(__imm >> 6) & 3];
	for (__i = 0; __i < 4; __i++)
		if ((__imm >> __i) & 1)
			__r[__i] = 0.0f;
	return (__m128)__r;
}

#define _mm_insert_ps(a, b, imm) __mcc_insert_ps((a), (b), (int)(imm))

__MCC_SSE4_INLINE int _mm_testz_si128(__m128i __a, __m128i __b)
{
	__v2du __x = (__v2du)__a, __y = (__v2du)__b;
	return ((__x[0] & __y[0]) | (__x[1] & __y[1])) == 0;
}

__MCC_SSE4_INLINE int _mm_testc_si128(__m128i __a, __m128i __b)
{
	__v2du __x = (__v2du)__a, __y = (__v2du)__b;
	return ((~__x[0] & __y[0]) | (~__x[1] & __y[1])) == 0;
}

__MCC_SSE4_INLINE int _mm_testnzc_si128(__m128i __a, __m128i __b)
{
	return !_mm_testz_si128(__a, __b) && !_mm_testc_si128(__a, __b);
}

#define _mm_test_all_zeros(M, V) _mm_testz_si128((M), (V))
#define _mm_test_all_ones(V) _mm_testc_si128((V), _mm_cmpeq_epi32((V), (V)))
#define _mm_test_mix_ones_zeros(M, V) _mm_testnzc_si128((M), (V))

__MCC_SSE4_INLINE __m128i _mm_minpos_epu16(__m128i __a)
{
	__v8hu __x = (__v8hu)__a, __r;
	unsigned short __m = __x[0];
	int __k = 0, __i;
	for (__i = 1; __i < 8; __i++)
		if (__x[__i] < __m) {
			__m = __x[__i];
			__k = __i;
		}
	for (__i = 0; __i < 8; __i++)
		__r[__i] = 0;
	__r[0] = __m;
	__r[1] = (unsigned short)__k;
	return (__m128i)__r;
}

__MCC_SSE4_INLINE __m128i __mcc_mpsadbw_epu8(__m128i __a, __m128i __b, int __imm)
{
	__v16qu __x = (__v16qu)__a, __y = (__v16qu)__b;
	__v8hu __r;
	int __oa = ((__imm >> 2) & 1) * 4, __ob = (__imm & 3) * 4;
	int __i, __j;
	for (__i = 0; __i < 8; __i++) {
		int __s = 0;
		for (__j = 0; __j < 4; __j++) {
			int __d = (int)__x[__oa + __i + __j] - (int)__y[__ob + __j];
			__s += __d < 0 ? -__d : __d;
		}
		__r[__i] = (unsigned short)__s;
	}
	return (__m128i)__r;
}

#define _mm_mpsadbw_epu8(a, b, imm) __mcc_mpsadbw_epu8((a), (b), (int)(imm))

__MCC_SSE4_INLINE __m128i _mm_stream_load_si128(__m128i *__p)
{
	return *__p;
}

__MCC_SSE4_INLINE unsigned int __mcc_crc32_step(unsigned int __c, unsigned char __v)
{
	int __i;
	__c ^= __v;
	for (__i = 0; __i < 8; __i++)
		__c = (__c >> 1) ^ (0x82f63b78u & (0u - (__c & 1u)));
	return __c;
}

__MCC_SSE4_INLINE unsigned int _mm_crc32_u8(unsigned int __c, unsigned char __v)
{
	return __mcc_crc32_step(__c, __v);
}

__MCC_SSE4_INLINE unsigned int _mm_crc32_u16(unsigned int __c, unsigned short __v)
{
	__c = __mcc_crc32_step(__c, (unsigned char)__v);
	return __mcc_crc32_step(__c, (unsigned char)(__v >> 8));
}

__MCC_SSE4_INLINE unsigned int _mm_crc32_u32(unsigned int __c, unsigned int __v)
{
	int __i;
	for (__i = 0; __i < 4; __i++)
		__c = __mcc_crc32_step(__c, (unsigned char)(__v >> (8 * __i)));
	return __c;
}

#ifdef __x86_64__
__MCC_SSE4_INLINE unsigned long long _mm_crc32_u64(unsigned long long __c, unsigned long long __v)
{
	unsigned int __t = (unsigned int)__c;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__t = __mcc_crc32_step(__t, (unsigned char)(__v >> (8 * __i)));
	return (unsigned long long)__t;
}
#endif

#define _SIDD_UBYTE_OPS 0x00
#define _SIDD_UWORD_OPS 0x01
#define _SIDD_SBYTE_OPS 0x02
#define _SIDD_SWORD_OPS 0x03

#define _SIDD_CMP_EQUAL_ANY 0x00
#define _SIDD_CMP_RANGES 0x04
#define _SIDD_CMP_EQUAL_EACH 0x08
#define _SIDD_CMP_EQUAL_ORDERED 0x0c

#define _SIDD_POSITIVE_POLARITY 0x00
#define _SIDD_NEGATIVE_POLARITY 0x10
#define _SIDD_MASKED_POSITIVE_POLARITY 0x20
#define _SIDD_MASKED_NEGATIVE_POLARITY 0x30

#define _SIDD_LEAST_SIGNIFICANT 0x00
#define _SIDD_MOST_SIGNIFICANT 0x40

#define _SIDD_BIT_MASK 0x00
#define _SIDD_UNIT_MASK 0x40

__MCC_SSE4_INLINE int __mcc_pcmpstr_elem(__m128i __v, int __i, int __imm)
{
	if (__imm & 1) {
		if (__imm & 2)
			return (int)((__v8hi)__v)[__i];
		return (int)((__v8hu)__v)[__i];
	}
	if (__imm & 2)
		return (int)((__v16qs)__v)[__i];
	return (int)((__v16qu)__v)[__i];
}

__MCC_SSE4_INLINE int __mcc_pcmpstr_implicit_len(__m128i __v, int __imm)
{
	int __n = (__imm & 1) ? 8 : 16, __i;
	for (__i = 0; __i < __n; __i++)
		if (__mcc_pcmpstr_elem(__v, __i, __imm & ~2) == 0)
			return __i;
	return __n;
}

__MCC_SSE4_INLINE int __mcc_pcmpstr_clamp_len(int __l, int __imm)
{
	int __n = (__imm & 1) ? 8 : 16;
	if (__l < 0)
		__l = -__l;
	return __l > __n ? __n : __l;
}

__MCC_SSE4_INLINE int __mcc_pcmpstr_bool(__m128i __a, __m128i __b, int __i, int __j, int __la,
					 int __lb, int __imm)
{
	int __agg = (__imm >> 2) & 3;
	int __av = __i < __la, __bv = __j < __lb;
	if (!__av || !__bv) {
		if (__agg == 2)
			return (!__av && !__bv) ? 1 : 0;
		if (__agg == 3)
			return !__av ? 1 : 0;
		return 0;
	}
	{
		int __x = __mcc_pcmpstr_elem(__a, __i, __imm);
		int __y = __mcc_pcmpstr_elem(__b, __j, __imm);
		if (__agg == 1)
			return (__i & 1) ? (__y <= __x) : (__y >= __x);
		return __x == __y;
	}
}

__MCC_SSE4_INLINE int __mcc_pcmpstr_res(__m128i __a, __m128i __b, int __la, int __lb, int __imm)
{
	int __n = (__imm & 1) ? 8 : 16;
	int __agg = (__imm >> 2) & 3;
	int __pol = (__imm >> 4) & 3;
	int __res = 0, __i, __j;
	if (__agg == 0) {
		for (__j = 0; __j < __n; __j++)
			for (__i = 0; __i < __n; __i++)
				if (__mcc_pcmpstr_bool(__a, __b, __i, __j, __la, __lb, __imm))
					__res |= 1 << __j;
	} else if (__agg == 1) {
		for (__j = 0; __j < __n; __j++)
			for (__i = 0; __i < __n; __i += 2)
				if (__mcc_pcmpstr_bool(__a, __b, __i, __j, __la, __lb, __imm) &&
				    __mcc_pcmpstr_bool(__a, __b, __i + 1, __j, __la, __lb, __imm))
					__res |= 1 << __j;
	} else if (__agg == 2) {
		for (__j = 0; __j < __n; __j++)
			if (__mcc_pcmpstr_bool(__a, __b, __j, __j, __la, __lb, __imm))
				__res |= 1 << __j;
	} else {
		for (__j = 0; __j < __n; __j++) {
			int __k = 1;
			for (__i = 0; __i < __n - __j; __i++)
				if (!__mcc_pcmpstr_bool(__a, __b, __i, __i + __j, __la, __lb,
							__imm)) {
					__k = 0;
					break;
				}
			if (__k)
				__res |= 1 << __j;
		}
	}
	if (__pol == 1)
		__res ^= (1 << __n) - 1;
	else if (__pol == 3)
		__res ^= (1 << __lb) - 1;
	return __res & ((1 << __n) - 1);
}

__MCC_SSE4_INLINE int __mcc_pcmpstr_index(int __res, int __imm)
{
	int __n = (__imm & 1) ? 8 : 16, __i;
	if (__imm & 0x40) {
		for (__i = __n - 1; __i >= 0; __i--)
			if ((__res >> __i) & 1)
				return __i;
		return __n;
	}
	for (__i = 0; __i < __n; __i++)
		if ((__res >> __i) & 1)
			return __i;
	return __n;
}

__MCC_SSE4_INLINE __m128i __mcc_pcmpstr_mask(int __res, int __imm)
{
	int __n = (__imm & 1) ? 8 : 16, __i;
	if (!(__imm & 0x40)) {
		__v2du __r = {0, 0};
		__r[0] = (unsigned long long)(unsigned int)__res;
		return (__m128i)__r;
	}
	if (__imm & 1) {
		__v8hi __r;
		for (__i = 0; __i < 8; __i++)
			__r[__i] = (short)(((__res >> __i) & 1) ? -1 : 0);
		return (__m128i)__r;
	}
	{
		__v16qs __r;
		for (__i = 0; __i < 16; __i++)
			__r[__i] = (signed char)(((__res >> __i) & 1) ? -1 : 0);
		return (__m128i)__r;
	}
}

#define __mcc_istr_res(a, b, imm)                                                \
	__mcc_pcmpstr_res((a), (b), __mcc_pcmpstr_implicit_len((a), (imm)),      \
			  __mcc_pcmpstr_implicit_len((b), (imm)), (imm))
#define __mcc_estr_res(a, la, b, lb, imm)                                        \
	__mcc_pcmpstr_res((a), (b), __mcc_pcmpstr_clamp_len((la), (imm)),        \
			  __mcc_pcmpstr_clamp_len((lb), (imm)), (imm))

#define _mm_cmpistri(a, b, imm) __mcc_pcmpstr_index(__mcc_istr_res((a), (b), (imm)), (imm))
#define _mm_cmpistrm(a, b, imm) __mcc_pcmpstr_mask(__mcc_istr_res((a), (b), (imm)), (imm))
#define _mm_cmpistrc(a, b, imm) (__mcc_istr_res((a), (b), (imm)) != 0)
#define _mm_cmpistro(a, b, imm) (__mcc_istr_res((a), (b), (imm)) & 1)
#define _mm_cmpistrs(a, b, imm) \
	(__mcc_pcmpstr_implicit_len((a), (imm)) < ((imm) & 1 ? 8 : 16))
#define _mm_cmpistrz(a, b, imm) \
	(__mcc_pcmpstr_implicit_len((b), (imm)) < ((imm) & 1 ? 8 : 16))
#define _mm_cmpistra(a, b, imm) \
	(!_mm_cmpistrz((a), (b), (imm)) && __mcc_istr_res((a), (b), (imm)) == 0)

#define _mm_cmpestri(a, la, b, lb, imm) \
	__mcc_pcmpstr_index(__mcc_estr_res((a), (la), (b), (lb), (imm)), (imm))
#define _mm_cmpestrm(a, la, b, lb, imm) \
	__mcc_pcmpstr_mask(__mcc_estr_res((a), (la), (b), (lb), (imm)), (imm))
#define _mm_cmpestrc(a, la, b, lb, imm) (__mcc_estr_res((a), (la), (b), (lb), (imm)) != 0)
#define _mm_cmpestro(a, la, b, lb, imm) (__mcc_estr_res((a), (la), (b), (lb), (imm)) & 1)
#define _mm_cmpestrs(a, la, b, lb, imm) \
	(__mcc_pcmpstr_clamp_len((la), (imm)) < ((imm) & 1 ? 8 : 16))
#define _mm_cmpestrz(a, la, b, lb, imm) \
	(__mcc_pcmpstr_clamp_len((lb), (imm)) < ((imm) & 1 ? 8 : 16))
#define _mm_cmpestra(a, la, b, lb, imm)                    \
	(!_mm_cmpestrz((a), (la), (b), (lb), (imm)) &&     \
	 __mcc_estr_res((a), (la), (b), (lb), (imm)) == 0)

#endif
