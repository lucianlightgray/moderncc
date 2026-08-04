#ifndef _PMMINTRIN_H_INCLUDED
#define _PMMINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "pmmintrin.h is only supported on x86 targets"
#endif

#include <emmintrin.h>

#define __MCC_SSE3_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

#define _MM_DENORMALS_ZERO_ON 0x0040
#define _MM_DENORMALS_ZERO_OFF 0x0000
#define _MM_DENORMALS_ZERO_MASK 0x0040

#define _MM_GET_DENORMALS_ZERO_MODE() (_mm_getcsr() & _MM_DENORMALS_ZERO_MASK)
#define _MM_SET_DENORMALS_ZERO_MODE(mode) \
	_mm_setcsr((_mm_getcsr() & ~_MM_DENORMALS_ZERO_MASK) | (mode))

__MCC_SSE3_INLINE __m128 _mm_addsub_ps(__m128 __a, __m128 __b)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __r;
	__r[0] = __x[0] - __y[0];
	__r[1] = __x[1] + __y[1];
	__r[2] = __x[2] - __y[2];
	__r[3] = __x[3] + __y[3];
	return (__m128)__r;
}

__MCC_SSE3_INLINE __m128d _mm_addsub_pd(__m128d __a, __m128d __b)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __r;
	__r[0] = __x[0] - __y[0];
	__r[1] = __x[1] + __y[1];
	return (__m128d)__r;
}

__MCC_SSE3_INLINE __m128 _mm_hadd_ps(__m128 __a, __m128 __b)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __r;
	__r[0] = __x[0] + __x[1];
	__r[1] = __x[2] + __x[3];
	__r[2] = __y[0] + __y[1];
	__r[3] = __y[2] + __y[3];
	return (__m128)__r;
}

__MCC_SSE3_INLINE __m128 _mm_hsub_ps(__m128 __a, __m128 __b)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b, __r;
	__r[0] = __x[0] - __x[1];
	__r[1] = __x[2] - __x[3];
	__r[2] = __y[0] - __y[1];
	__r[3] = __y[2] - __y[3];
	return (__m128)__r;
}

__MCC_SSE3_INLINE __m128d _mm_hadd_pd(__m128d __a, __m128d __b)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __r;
	__r[0] = __x[0] + __x[1];
	__r[1] = __y[0] + __y[1];
	return (__m128d)__r;
}

__MCC_SSE3_INLINE __m128d _mm_hsub_pd(__m128d __a, __m128d __b)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b, __r;
	__r[0] = __x[0] - __x[1];
	__r[1] = __y[0] - __y[1];
	return (__m128d)__r;
}

__MCC_SSE3_INLINE __m128 _mm_movehdup_ps(__m128 __a)
{
	return (__m128)__builtin_shufflevector((__v4sf)__a, (__v4sf)__a, 1, 1, 3, 3);
}

__MCC_SSE3_INLINE __m128 _mm_moveldup_ps(__m128 __a)
{
	return (__m128)__builtin_shufflevector((__v4sf)__a, (__v4sf)__a, 0, 0, 2, 2);
}

__MCC_SSE3_INLINE __m128d _mm_movedup_pd(__m128d __a)
{
	return (__m128d)__builtin_shufflevector((__v2df)__a, (__v2df)__a, 0, 0);
}

__MCC_SSE3_INLINE __m128d _mm_loaddup_pd(double const *__p)
{
	double __v = ((const struct __mcc_loadu_f64 *)__p)->__v;
	return (__m128d)(__v2df){__v, __v};
}

__MCC_SSE3_INLINE __m128i _mm_lddqu_si128(__m128i const *__p)
{
	return (__m128i)((const struct __mcc_loadu_si128 *)__p)->__v;
}

__MCC_SSE3_INLINE void _mm_monitor(void const *__p, unsigned int __e, unsigned int __h)
{
	__asm__ volatile("monitor" : : "a"(__p), "c"(__e), "d"(__h));
}

__MCC_SSE3_INLINE void _mm_mwait(unsigned int __e, unsigned int __h)
{
	__asm__ volatile("mwait" : : "a"(__e), "c"(__h));
}

#endif
