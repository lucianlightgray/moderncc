#ifndef _AVXINTRIN_H_INCLUDED
#define _AVXINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "avxintrin.h is only supported on x86 targets"
#endif

#include <smmintrin.h>

typedef float __m256 __attribute__((__vector_size__(32), __aligned__(32)));
typedef double __m256d __attribute__((__vector_size__(32), __aligned__(32)));
typedef long long __m256i __attribute__((__vector_size__(32), __aligned__(32)));
typedef float __m256_u __attribute__((__vector_size__(32), __aligned__(1)));
typedef double __m256d_u __attribute__((__vector_size__(32), __aligned__(1)));
typedef long long __m256i_u __attribute__((__vector_size__(32), __aligned__(1)));

typedef float __v8sf __attribute__((__vector_size__(32)));
typedef double __v4df __attribute__((__vector_size__(32)));
typedef int __v8si __attribute__((__vector_size__(32)));
typedef unsigned int __v8su __attribute__((__vector_size__(32)));
typedef short __v16hi __attribute__((__vector_size__(32)));
typedef unsigned short __v16hu __attribute__((__vector_size__(32)));
typedef char __v32qi __attribute__((__vector_size__(32)));
typedef signed char __v32qs __attribute__((__vector_size__(32)));
typedef unsigned char __v32qu __attribute__((__vector_size__(32)));
typedef long long __v4di __attribute__((__vector_size__(32)));
typedef unsigned long long __v4du __attribute__((__vector_size__(32)));

#define __MCC_AVX_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

struct __mcc_loadu_si256 {
	__v4di __v;
} __attribute__((__packed__, __may_alias__));

struct __mcc_loadu_pd256 {
	__v4df __v;
} __attribute__((__packed__, __may_alias__));

struct __mcc_loadu_ps256 {
	__v8sf __v;
} __attribute__((__packed__, __may_alias__));

#define _CMP_EQ_OQ 0x00
#define _CMP_LT_OS 0x01
#define _CMP_LE_OS 0x02
#define _CMP_UNORD_Q 0x03
#define _CMP_NEQ_UQ 0x04
#define _CMP_NLT_US 0x05
#define _CMP_NLE_US 0x06
#define _CMP_ORD_Q 0x07
#define _CMP_EQ_UQ 0x08
#define _CMP_NGE_US 0x09
#define _CMP_NGT_US 0x0a
#define _CMP_FALSE_OQ 0x0b
#define _CMP_NEQ_OQ 0x0c
#define _CMP_GE_OS 0x0d
#define _CMP_GT_OS 0x0e
#define _CMP_TRUE_UQ 0x0f
#define _CMP_EQ_OS 0x10
#define _CMP_LT_OQ 0x11
#define _CMP_LE_OQ 0x12
#define _CMP_UNORD_S 0x13
#define _CMP_NEQ_US 0x14
#define _CMP_NLT_UQ 0x15
#define _CMP_NLE_UQ 0x16
#define _CMP_ORD_S 0x17
#define _CMP_EQ_US 0x18
#define _CMP_NGE_UQ 0x19
#define _CMP_NGT_UQ 0x1a
#define _CMP_FALSE_OS 0x1b
#define _CMP_NEQ_OS 0x1c
#define _CMP_GE_OQ 0x1d
#define _CMP_GT_OQ 0x1e
#define _CMP_TRUE_US 0x1f

__MCC_AVX_INLINE int __mcc_cmp_f64(double __x, double __y, int __p)
{
	switch (__p & 15) {
	case 0:
		return __x == __y;
	case 1:
		return __x < __y;
	case 2:
		return __x <= __y;
	case 3:
		return !(__x == __x) || !(__y == __y);
	case 4:
		return !(__x == __y);
	case 5:
		return !(__x < __y);
	case 6:
		return !(__x <= __y);
	case 7:
		return (__x == __x) && (__y == __y);
	case 8:
		return (__x == __y) || !(__x == __x) || !(__y == __y);
	case 9:
		return !(__x >= __y);
	case 10:
		return !(__x > __y);
	case 11:
		return 0;
	case 12:
		return (__x != __y) && (__x == __x) && (__y == __y);
	case 13:
		return __x >= __y;
	case 14:
		return __x > __y;
	default:
		return 1;
	}
}

__MCC_AVX_INLINE int __mcc_cmp_f32(float __x, float __y, int __p)
{
	switch (__p & 15) {
	case 0:
		return __x == __y;
	case 1:
		return __x < __y;
	case 2:
		return __x <= __y;
	case 3:
		return !(__x == __x) || !(__y == __y);
	case 4:
		return !(__x == __y);
	case 5:
		return !(__x < __y);
	case 6:
		return !(__x <= __y);
	case 7:
		return (__x == __x) && (__y == __y);
	case 8:
		return (__x == __y) || !(__x == __x) || !(__y == __y);
	case 9:
		return !(__x >= __y);
	case 10:
		return !(__x > __y);
	case 11:
		return 0;
	case 12:
		return (__x != __y) && (__x == __x) && (__y == __y);
	case 13:
		return __x >= __y;
	case 14:
		return __x > __y;
	default:
		return 1;
	}
}

__MCC_AVX_INLINE __m256d _mm256_setzero_pd(void)
{
	return (__m256d)(__v4df){0.0, 0.0, 0.0, 0.0};
}

__MCC_AVX_INLINE __m256 _mm256_setzero_ps(void)
{
	return (__m256)(__v8sf){0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
}

__MCC_AVX_INLINE __m256i _mm256_setzero_si256(void)
{
	return (__m256i)(__v4di){0, 0, 0, 0};
}

__MCC_AVX_INLINE __m256d _mm256_set_pd(double __a, double __b, double __c, double __d)
{
	return (__m256d)(__v4df){__d, __c, __b, __a};
}

__MCC_AVX_INLINE __m256 _mm256_set_ps(float __a, float __b, float __c, float __d, float __e,
				      float __f, float __g, float __h)
{
	return (__m256)(__v8sf){__h, __g, __f, __e, __d, __c, __b, __a};
}

__MCC_AVX_INLINE __m256i _mm256_set_epi64x(long long __a, long long __b, long long __c,
					   long long __d)
{
	return (__m256i)(__v4di){__d, __c, __b, __a};
}

__MCC_AVX_INLINE __m256i _mm256_set_epi32(int __a, int __b, int __c, int __d, int __e, int __f,
					  int __g, int __h)
{
	return (__m256i)(__v8si){__h, __g, __f, __e, __d, __c, __b, __a};
}

__MCC_AVX_INLINE __m256i _mm256_set_epi16(short __e15, short __e14, short __e13, short __e12,
					  short __e11, short __e10, short __e09, short __e08,
					  short __e07, short __e06, short __e05, short __e04,
					  short __e03, short __e02, short __e01, short __e00)
{
	return (__m256i)(__v16hi){__e00, __e01, __e02, __e03, __e04, __e05, __e06, __e07,
				  __e08, __e09, __e10, __e11, __e12, __e13, __e14, __e15};
}

__MCC_AVX_INLINE __m256i
_mm256_set_epi8(char __e31, char __e30, char __e29, char __e28, char __e27, char __e26, char __e25,
		char __e24, char __e23, char __e22, char __e21, char __e20, char __e19, char __e18,
		char __e17, char __e16, char __e15, char __e14, char __e13, char __e12, char __e11,
		char __e10, char __e09, char __e08, char __e07, char __e06, char __e05, char __e04,
		char __e03, char __e02, char __e01, char __e00)
{
	return (__m256i)(__v32qi){__e00, __e01, __e02, __e03, __e04, __e05, __e06, __e07,
				  __e08, __e09, __e10, __e11, __e12, __e13, __e14, __e15,
				  __e16, __e17, __e18, __e19, __e20, __e21, __e22, __e23,
				  __e24, __e25, __e26, __e27, __e28, __e29, __e30, __e31};
}

__MCC_AVX_INLINE __m256d _mm256_setr_pd(double __a, double __b, double __c, double __d)
{
	return (__m256d)(__v4df){__a, __b, __c, __d};
}

__MCC_AVX_INLINE __m256 _mm256_setr_ps(float __a, float __b, float __c, float __d, float __e,
				       float __f, float __g, float __h)
{
	return (__m256)(__v8sf){__a, __b, __c, __d, __e, __f, __g, __h};
}

__MCC_AVX_INLINE __m256i _mm256_setr_epi64x(long long __a, long long __b, long long __c,
					    long long __d)
{
	return (__m256i)(__v4di){__a, __b, __c, __d};
}

__MCC_AVX_INLINE __m256i _mm256_setr_epi32(int __a, int __b, int __c, int __d, int __e, int __f,
					   int __g, int __h)
{
	return (__m256i)(__v8si){__a, __b, __c, __d, __e, __f, __g, __h};
}

__MCC_AVX_INLINE __m256i _mm256_setr_epi16(short __e00, short __e01, short __e02, short __e03,
					   short __e04, short __e05, short __e06, short __e07,
					   short __e08, short __e09, short __e10, short __e11,
					   short __e12, short __e13, short __e14, short __e15)
{
	return (__m256i)(__v16hi){__e00, __e01, __e02, __e03, __e04, __e05, __e06, __e07,
				  __e08, __e09, __e10, __e11, __e12, __e13, __e14, __e15};
}

__MCC_AVX_INLINE __m256i
_mm256_setr_epi8(char __e00, char __e01, char __e02, char __e03, char __e04, char __e05, char __e06,
		 char __e07, char __e08, char __e09, char __e10, char __e11, char __e12, char __e13,
		 char __e14, char __e15, char __e16, char __e17, char __e18, char __e19, char __e20,
		 char __e21, char __e22, char __e23, char __e24, char __e25, char __e26, char __e27,
		 char __e28, char __e29, char __e30, char __e31)
{
	return (__m256i)(__v32qi){__e00, __e01, __e02, __e03, __e04, __e05, __e06, __e07,
				  __e08, __e09, __e10, __e11, __e12, __e13, __e14, __e15,
				  __e16, __e17, __e18, __e19, __e20, __e21, __e22, __e23,
				  __e24, __e25, __e26, __e27, __e28, __e29, __e30, __e31};
}

__MCC_AVX_INLINE __m256d _mm256_set1_pd(double __a)
{
	return (__m256d)(__v4df){__a, __a, __a, __a};
}

__MCC_AVX_INLINE __m256 _mm256_set1_ps(float __a)
{
	return (__m256)(__v8sf){__a, __a, __a, __a, __a, __a, __a, __a};
}

__MCC_AVX_INLINE __m256i _mm256_set1_epi64x(long long __a)
{
	return (__m256i)(__v4di){__a, __a, __a, __a};
}

__MCC_AVX_INLINE __m256i _mm256_set1_epi32(int __a)
{
	return (__m256i)(__v8si){__a, __a, __a, __a, __a, __a, __a, __a};
}

__MCC_AVX_INLINE __m256i _mm256_set1_epi16(short __a)
{
	return (__m256i)(__v16hi){__a, __a, __a, __a, __a, __a, __a, __a,
				  __a, __a, __a, __a, __a, __a, __a, __a};
}

__MCC_AVX_INLINE __m256i _mm256_set1_epi8(char __a)
{
	return (__m256i)(__v32qi){__a, __a, __a, __a, __a, __a, __a, __a, __a, __a, __a,
				  __a, __a, __a, __a, __a, __a, __a, __a, __a, __a, __a,
				  __a, __a, __a, __a, __a, __a, __a, __a, __a, __a};
}

__MCC_AVX_INLINE __m256 _mm256_castpd_ps(__m256d __a)
{
	return (__m256)__a;
}

__MCC_AVX_INLINE __m256i _mm256_castpd_si256(__m256d __a)
{
	return (__m256i)__a;
}

__MCC_AVX_INLINE __m256d _mm256_castps_pd(__m256 __a)
{
	return (__m256d)__a;
}

__MCC_AVX_INLINE __m256i _mm256_castps_si256(__m256 __a)
{
	return (__m256i)__a;
}

__MCC_AVX_INLINE __m256 _mm256_castsi256_ps(__m256i __a)
{
	return (__m256)__a;
}

__MCC_AVX_INLINE __m256d _mm256_castsi256_pd(__m256i __a)
{
	return (__m256d)__a;
}

__MCC_AVX_INLINE __m128d _mm256_castpd256_pd128(__m256d __a)
{
	return (__m128d)__builtin_shufflevector((__v4df)__a, (__v4df)__a, 0, 1);
}

__MCC_AVX_INLINE __m128 _mm256_castps256_ps128(__m256 __a)
{
	return (__m128)__builtin_shufflevector((__v8sf)__a, (__v8sf)__a, 0, 1, 2, 3);
}

__MCC_AVX_INLINE __m128i _mm256_castsi256_si128(__m256i __a)
{
	return (__m128i)__builtin_shufflevector((__v4di)__a, (__v4di)__a, 0, 1);
}

__MCC_AVX_INLINE __m256d _mm256_castpd128_pd256(__m128d __a)
{
	return (__m256d)__builtin_shufflevector((__v2df)__a, (__v2df)__a, 0, 1, -1, -1);
}

__MCC_AVX_INLINE __m256 _mm256_castps128_ps256(__m128 __a)
{
	return (__m256)__builtin_shufflevector((__v4sf)__a, (__v4sf)__a, 0, 1, 2, 3, -1, -1, -1,
					       -1);
}

__MCC_AVX_INLINE __m256i _mm256_castsi128_si256(__m128i __a)
{
	return (__m256i)__builtin_shufflevector((__v2di)__a, (__v2di)__a, 0, 1, -1, -1);
}

__MCC_AVX_INLINE __m256d _mm256_zextpd128_pd256(__m128d __a)
{
	return (__m256d)__builtin_shufflevector((__v2df)__a, (__v2df)_mm_setzero_pd(), 0, 1, 2, 3);
}

__MCC_AVX_INLINE __m256 _mm256_zextps128_ps256(__m128 __a)
{
	return (__m256)__builtin_shufflevector((__v4sf)__a, (__v4sf)_mm_setzero_ps(), 0, 1, 2, 3, 4,
					       5, 6, 7);
}

__MCC_AVX_INLINE __m256i _mm256_zextsi128_si256(__m128i __a)
{
	return (__m256i)__builtin_shufflevector((__v2di)__a, (__v2di)_mm_setzero_si128(), 0, 1, 2,
						3);
}

__MCC_AVX_INLINE __m256d _mm256_add_pd(__m256d __a, __m256d __b)
{
	return (__m256d)((__v4df)__a + (__v4df)__b);
}

__MCC_AVX_INLINE __m256 _mm256_add_ps(__m256 __a, __m256 __b)
{
	return (__m256)((__v8sf)__a + (__v8sf)__b);
}

__MCC_AVX_INLINE __m256d _mm256_sub_pd(__m256d __a, __m256d __b)
{
	return (__m256d)((__v4df)__a - (__v4df)__b);
}

__MCC_AVX_INLINE __m256 _mm256_sub_ps(__m256 __a, __m256 __b)
{
	return (__m256)((__v8sf)__a - (__v8sf)__b);
}

__MCC_AVX_INLINE __m256d _mm256_mul_pd(__m256d __a, __m256d __b)
{
	return (__m256d)((__v4df)__a * (__v4df)__b);
}

__MCC_AVX_INLINE __m256 _mm256_mul_ps(__m256 __a, __m256 __b)
{
	return (__m256)((__v8sf)__a * (__v8sf)__b);
}

__MCC_AVX_INLINE __m256d _mm256_div_pd(__m256d __a, __m256d __b)
{
	return (__m256d)((__v4df)__a / (__v4df)__b);
}

__MCC_AVX_INLINE __m256 _mm256_div_ps(__m256 __a, __m256 __b)
{
	return (__m256)((__v8sf)__a / (__v8sf)__b);
}

__MCC_AVX_INLINE __m256d _mm256_addsub_pd(__m256d __a, __m256d __b)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (__i & 1) ? __x[__i] + __y[__i] : __x[__i] - __y[__i];
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m256 _mm256_addsub_ps(__m256 __a, __m256 __b)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (__i & 1) ? __x[__i] + __y[__i] : __x[__i] - __y[__i];
	return (__m256)__r;
}

__MCC_AVX_INLINE __m256d _mm256_hadd_pd(__m256d __a, __m256d __b)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __r;
	__r[0] = __x[0] + __x[1];
	__r[1] = __y[0] + __y[1];
	__r[2] = __x[2] + __x[3];
	__r[3] = __y[2] + __y[3];
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m256d _mm256_hsub_pd(__m256d __a, __m256d __b)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __r;
	__r[0] = __x[0] - __x[1];
	__r[1] = __y[0] - __y[1];
	__r[2] = __x[2] - __x[3];
	__r[3] = __y[2] - __y[3];
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m256 _mm256_hadd_ps(__m256 __a, __m256 __b)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __r;
	__r[0] = __x[0] + __x[1];
	__r[1] = __x[2] + __x[3];
	__r[2] = __y[0] + __y[1];
	__r[3] = __y[2] + __y[3];
	__r[4] = __x[4] + __x[5];
	__r[5] = __x[6] + __x[7];
	__r[6] = __y[4] + __y[5];
	__r[7] = __y[6] + __y[7];
	return (__m256)__r;
}

__MCC_AVX_INLINE __m256 _mm256_hsub_ps(__m256 __a, __m256 __b)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __r;
	__r[0] = __x[0] - __x[1];
	__r[1] = __x[2] - __x[3];
	__r[2] = __y[0] - __y[1];
	__r[3] = __y[2] - __y[3];
	__r[4] = __x[4] - __x[5];
	__r[5] = __x[6] - __x[7];
	__r[6] = __y[4] - __y[5];
	__r[7] = __y[6] - __y[7];
	return (__m256)__r;
}

__MCC_AVX_INLINE __m256d _mm256_max_pd(__m256d __a, __m256d __b)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m256d _mm256_min_pd(__m256d __a, __m256d __b)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m256 _mm256_max_ps(__m256 __a, __m256 __b)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] > __y[__i] ? __x[__i] : __y[__i];
	return (__m256)__r;
}

__MCC_AVX_INLINE __m256 _mm256_min_ps(__m256 __a, __m256 __b)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __x[__i] < __y[__i] ? __x[__i] : __y[__i];
	return (__m256)__r;
}

__MCC_AVX_INLINE __m256d _mm256_sqrt_pd(__m256d __a)
{
	__v4df __x = (__v4df)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_sqrtsd(__x[__i]);
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m256 _mm256_sqrt_ps(__m256 __a)
{
	__v8sf __x = (__v8sf)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_sqrtss(__x[__i]);
	return (__m256)__r;
}

__MCC_AVX_INLINE __m256 _mm256_rsqrt_ps(__m256 __a)
{
	__v8sf __x = (__v8sf)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_rsqrtss(__x[__i]);
	return (__m256)__r;
}

__MCC_AVX_INLINE __m256 _mm256_rcp_ps(__m256 __a)
{
	__v8sf __x = (__v8sf)__a, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_rcpss(__x[__i]);
	return (__m256)__r;
}

__MCC_AVX_INLINE __m256d _mm256_and_pd(__m256d __a, __m256d __b)
{
	return (__m256d)((__v4du)__a & (__v4du)__b);
}

__MCC_AVX_INLINE __m256 _mm256_and_ps(__m256 __a, __m256 __b)
{
	return (__m256)((__v8su)__a & (__v8su)__b);
}

__MCC_AVX_INLINE __m256d _mm256_andnot_pd(__m256d __a, __m256d __b)
{
	return (__m256d)(~(__v4du)__a & (__v4du)__b);
}

__MCC_AVX_INLINE __m256 _mm256_andnot_ps(__m256 __a, __m256 __b)
{
	return (__m256)(~(__v8su)__a & (__v8su)__b);
}

__MCC_AVX_INLINE __m256d _mm256_or_pd(__m256d __a, __m256d __b)
{
	return (__m256d)((__v4du)__a | (__v4du)__b);
}

__MCC_AVX_INLINE __m256 _mm256_or_ps(__m256 __a, __m256 __b)
{
	return (__m256)((__v8su)__a | (__v8su)__b);
}

__MCC_AVX_INLINE __m256d _mm256_xor_pd(__m256d __a, __m256d __b)
{
	return (__m256d)((__v4du)__a ^ (__v4du)__b);
}

__MCC_AVX_INLINE __m256 _mm256_xor_ps(__m256 __a, __m256 __b)
{
	return (__m256)((__v8su)__a ^ (__v8su)__b);
}

__MCC_AVX_INLINE __m256i _mm256_and_si256(__m256i __a, __m256i __b)
{
	return (__m256i)((__v4du)__a & (__v4du)__b);
}

__MCC_AVX_INLINE __m256i _mm256_andnot_si256(__m256i __a, __m256i __b)
{
	return (__m256i)(~(__v4du)__a & (__v4du)__b);
}

__MCC_AVX_INLINE __m256i _mm256_or_si256(__m256i __a, __m256i __b)
{
	return (__m256i)((__v4du)__a | (__v4du)__b);
}

__MCC_AVX_INLINE __m256i _mm256_xor_si256(__m256i __a, __m256i __b)
{
	return (__m256i)((__v4du)__a ^ (__v4du)__b);
}

__MCC_AVX_INLINE __m256d __mcc_cmp_pd256(__m256d __a, __m256d __b, int __p)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b;
	__v4di __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_cmp_f64(__x[__i], __y[__i], __p) ? -1LL : 0LL;
	return (__m256d)__r;
}

#define _mm256_cmp_pd(a, b, p) __mcc_cmp_pd256((a), (b), (int)(p))

__MCC_AVX_INLINE __m256 __mcc_cmp_ps256(__m256 __a, __m256 __b, int __p)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b;
	__v8si __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_cmp_f32(__x[__i], __y[__i], __p) ? -1 : 0;
	return (__m256)__r;
}

#define _mm256_cmp_ps(a, b, p) __mcc_cmp_ps256((a), (b), (int)(p))

__MCC_AVX_INLINE __m128d __mcc_cmp_pd128(__m128d __a, __m128d __b, int __p)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b;
	__v2di __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __mcc_cmp_f64(__x[__i], __y[__i], __p) ? -1LL : 0LL;
	return (__m128d)__r;
}

#define _mm_cmp_pd(a, b, p) __mcc_cmp_pd128((a), (b), (int)(p))

__MCC_AVX_INLINE __m128 __mcc_cmp_ps128(__m128 __a, __m128 __b, int __p)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_cmp_f32(__x[__i], __y[__i], __p) ? -1 : 0;
	return (__m128)__r;
}

#define _mm_cmp_ps(a, b, p) __mcc_cmp_ps128((a), (b), (int)(p))

__MCC_AVX_INLINE __m128d __mcc_cmp_sd(__m128d __a, __m128d __b, int __p)
{
	__v2df __x = (__v2df)__a, __y = (__v2df)__b;
	__v2di __t = (__v2di)__a;
	__t[0] = __mcc_cmp_f64(__x[0], __y[0], __p) ? -1LL : 0LL;
	return (__m128d)__t;
}

#define _mm_cmp_sd(a, b, p) __mcc_cmp_sd((a), (b), (int)(p))

__MCC_AVX_INLINE __m128 __mcc_cmp_ss(__m128 __a, __m128 __b, int __p)
{
	__v4sf __x = (__v4sf)__a, __y = (__v4sf)__b;
	__v4si __t = (__v4si)__a;
	__t[0] = __mcc_cmp_f32(__x[0], __y[0], __p) ? -1 : 0;
	return (__m128)__t;
}

#define _mm_cmp_ss(a, b, p) __mcc_cmp_ss((a), (b), (int)(p))

__MCC_AVX_INLINE __m256d _mm256_cvtepi32_pd(__m128i __a)
{
	__v4si __x = (__v4si)__a;
	__v4df __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (double)__x[__i];
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m256 _mm256_cvtepi32_ps(__m256i __a)
{
	__v8si __x = (__v8si)__a;
	__v8sf __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (float)__x[__i];
	return (__m256)__r;
}

__MCC_AVX_INLINE __m128 _mm256_cvtpd_ps(__m256d __a)
{
	__v4df __x = (__v4df)__a;
	__v4sf __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (float)__x[__i];
	return (__m128)__r;
}

__MCC_AVX_INLINE __m256d _mm256_cvtps_pd(__m128 __a)
{
	__v4sf __x = (__v4sf)__a;
	__v4df __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (double)__x[__i];
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m128i _mm256_cvtpd_epi32(__m256d __a)
{
	__v4df __x = (__v4df)__a;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_cvt_f64_i32(__x[__i]);
	return (__m128i)__r;
}

__MCC_AVX_INLINE __m128i _mm256_cvttpd_epi32(__m256d __a)
{
	__v4df __x = (__v4df)__a;
	__v4si __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_cvtt_f64_i32(__x[__i]);
	return (__m128i)__r;
}

__MCC_AVX_INLINE __m256i _mm256_cvtps_epi32(__m256 __a)
{
	__v8sf __x = (__v8sf)__a;
	__v8si __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_cvt_f32_i32(__x[__i]);
	return (__m256i)__r;
}

__MCC_AVX_INLINE __m256i _mm256_cvttps_epi32(__m256 __a)
{
	__v8sf __x = (__v8sf)__a;
	__v8si __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_cvtt_f32_i32(__x[__i]);
	return (__m256i)__r;
}

__MCC_AVX_INLINE double _mm256_cvtsd_f64(__m256d __a)
{
	return ((__v4df)__a)[0];
}

__MCC_AVX_INLINE float _mm256_cvtss_f32(__m256 __a)
{
	return ((__v8sf)__a)[0];
}

__MCC_AVX_INLINE int _mm256_cvtsi256_si32(__m256i __a)
{
	return ((__v8si)__a)[0];
}

__MCC_AVX_INLINE __m256d __mcc_round_pd256(__m256d __a, int __imm)
{
	__v4df __x = (__v4df)__a, __r;
	int __m = __mcc_rnd_mode(__imm), __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __mcc_rnd_f64(__x[__i], __m);
	return (__m256d)__r;
}

#define _mm256_round_pd(a, imm) __mcc_round_pd256((a), (int)(imm))

__MCC_AVX_INLINE __m256 __mcc_round_ps256(__m256 __a, int __imm)
{
	__v8sf __x = (__v8sf)__a, __r;
	int __m = __mcc_rnd_mode(__imm), __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __mcc_rnd_f32(__x[__i], __m);
	return (__m256)__r;
}

#define _mm256_round_ps(a, imm) __mcc_round_ps256((a), (int)(imm))

#define _mm256_ceil_pd(V) _mm256_round_pd((V), _MM_FROUND_CEIL)
#define _mm256_floor_pd(V) _mm256_round_pd((V), _MM_FROUND_FLOOR)
#define _mm256_ceil_ps(V) _mm256_round_ps((V), _MM_FROUND_CEIL)
#define _mm256_floor_ps(V) _mm256_round_ps((V), _MM_FROUND_FLOOR)

__MCC_AVX_INLINE __m256d __mcc_blend_pd256(__m256d __a, __m256d __b, int __imm)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (__imm >> __i) & 1 ? __y[__i] : __x[__i];
	return (__m256d)__r;
}

#define _mm256_blend_pd(a, b, imm) __mcc_blend_pd256((a), (b), (int)(imm))

__MCC_AVX_INLINE __m256 __mcc_blend_ps256(__m256 __a, __m256 __b, int __imm)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (__imm >> __i) & 1 ? __y[__i] : __x[__i];
	return (__m256)__r;
}

#define _mm256_blend_ps(a, b, imm) __mcc_blend_ps256((a), (b), (int)(imm))

__MCC_AVX_INLINE __m256d _mm256_blendv_pd(__m256d __a, __m256d __b, __m256d __m)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __r;
	__v4di __k = (__v4di)__m;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __k[__i] < 0 ? __y[__i] : __x[__i];
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m256 _mm256_blendv_ps(__m256 __a, __m256 __b, __m256 __m)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __r;
	__v8si __k = (__v8si)__m;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __k[__i] < 0 ? __y[__i] : __x[__i];
	return (__m256)__r;
}

__MCC_AVX_INLINE __m256 __mcc_dp_ps256(__m256 __a, __m256 __b, int __imm)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++) {
		float __t[4], __s;
		for (__i = 0; __i < 4; __i++)
			__t[__i] = (__imm >> (4 + __i)) & 1 ? __x[4 * __l + __i] * __y[4 * __l + __i]
							    : 0.0f;
		__s = (__t[0] + __t[1]) + (__t[2] + __t[3]);
		for (__i = 0; __i < 4; __i++)
			__r[4 * __l + __i] = (__imm >> __i) & 1 ? __s : 0.0f;
	}
	return (__m256)__r;
}

#define _mm256_dp_ps(a, b, imm) __mcc_dp_ps256((a), (b), (int)(imm))

__MCC_AVX_INLINE __m256d __mcc_shuffle_pd256(__m256d __a, __m256d __b, int __imm)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __r;
	__r[0] = __x[(__imm >> 0) & 1];
	__r[1] = __y[(__imm >> 1) & 1];
	__r[2] = __x[2 + ((__imm >> 2) & 1)];
	__r[3] = __y[2 + ((__imm >> 3) & 1)];
	return (__m256d)__r;
}

#define _mm256_shuffle_pd(a, b, imm) __mcc_shuffle_pd256((a), (b), (int)(imm))

__MCC_AVX_INLINE __m256 __mcc_shuffle_ps256(__m256 __a, __m256 __b, int __imm)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __r;
	int __l;
	for (__l = 0; __l < 2; __l++) {
		__r[4 * __l + 0] = __x[4 * __l + ((__imm >> 0) & 3)];
		__r[4 * __l + 1] = __x[4 * __l + ((__imm >> 2) & 3)];
		__r[4 * __l + 2] = __y[4 * __l + ((__imm >> 4) & 3)];
		__r[4 * __l + 3] = __y[4 * __l + ((__imm >> 6) & 3)];
	}
	return (__m256)__r;
}

#define _mm256_shuffle_ps(a, b, imm) __mcc_shuffle_ps256((a), (b), (int)(imm))

__MCC_AVX_INLINE __m256d __mcc_permute_pd256(__m256d __a, int __imm)
{
	__v4df __x = (__v4df)__a, __r;
	__r[0] = __x[(__imm >> 0) & 1];
	__r[1] = __x[(__imm >> 1) & 1];
	__r[2] = __x[2 + ((__imm >> 2) & 1)];
	__r[3] = __x[2 + ((__imm >> 3) & 1)];
	return (__m256d)__r;
}

#define _mm256_permute_pd(a, imm) __mcc_permute_pd256((a), (int)(imm))

__MCC_AVX_INLINE __m128d __mcc_permute_pd128(__m128d __a, int __imm)
{
	__v2df __x = (__v2df)__a, __r;
	__r[0] = __x[(__imm >> 0) & 1];
	__r[1] = __x[(__imm >> 1) & 1];
	return (__m128d)__r;
}

#define _mm_permute_pd(a, imm) __mcc_permute_pd128((a), (int)(imm))

__MCC_AVX_INLINE __m256 __mcc_permute_ps256(__m256 __a, int __imm)
{
	__v8sf __x = (__v8sf)__a, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++)
			__r[4 * __l + __i] = __x[4 * __l + ((__imm >> (2 * __i)) & 3)];
	return (__m256)__r;
}

#define _mm256_permute_ps(a, imm) __mcc_permute_ps256((a), (int)(imm))

__MCC_AVX_INLINE __m128 __mcc_permute_ps128(__m128 __a, int __imm)
{
	__v4sf __x = (__v4sf)__a, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[(__imm >> (2 * __i)) & 3];
	return (__m128)__r;
}

#define _mm_permute_ps(a, imm) __mcc_permute_ps128((a), (int)(imm))

__MCC_AVX_INLINE __m256d _mm256_permutevar_pd(__m256d __a, __m256i __c)
{
	__v4df __x = (__v4df)__a, __r;
	__v4di __k = (__v4di)__c;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 2; __i++)
			__r[2 * __l + __i] =
				__x[2 * __l + (int)((__k[2 * __l + __i] >> 1) & 1)];
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m128d _mm_permutevar_pd(__m128d __a, __m128i __c)
{
	__v2df __x = (__v2df)__a, __r;
	__v2di __k = (__v2di)__c;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __x[(int)((__k[__i] >> 1) & 1)];
	return (__m128d)__r;
}

__MCC_AVX_INLINE __m256 _mm256_permutevar_ps(__m256 __a, __m256i __c)
{
	__v8sf __x = (__v8sf)__a, __r;
	__v8si __k = (__v8si)__c;
	int __l, __i;
	for (__l = 0; __l < 2; __l++)
		for (__i = 0; __i < 4; __i++)
			__r[4 * __l + __i] = __x[4 * __l + (__k[4 * __l + __i] & 3)];
	return (__m256)__r;
}

__MCC_AVX_INLINE __m128 _mm_permutevar_ps(__m128 __a, __m128i __c)
{
	__v4sf __x = (__v4sf)__a, __r;
	__v4si __k = (__v4si)__c;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__k[__i] & 3];
	return (__m128)__r;
}

__MCC_AVX_INLINE __m256d __mcc_permute2f128_pd(__m256d __a, __m256d __b, int __imm)
{
	__v4df __x = (__v4df)__a, __y = (__v4df)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++) {
		int __c = (__imm >> (4 * __l)) & 0xf;
		for (__i = 0; __i < 2; __i++) {
			if (__c & 8)
				__r[2 * __l + __i] = 0.0;
			else if (__c & 2)
				__r[2 * __l + __i] = __y[2 * (__c & 1) + __i];
			else
				__r[2 * __l + __i] = __x[2 * (__c & 1) + __i];
		}
	}
	return (__m256d)__r;
}

#define _mm256_permute2f128_pd(a, b, imm) __mcc_permute2f128_pd((a), (b), (int)(imm))

__MCC_AVX_INLINE __m256 __mcc_permute2f128_ps(__m256 __a, __m256 __b, int __imm)
{
	__v8sf __x = (__v8sf)__a, __y = (__v8sf)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++) {
		int __c = (__imm >> (4 * __l)) & 0xf;
		for (__i = 0; __i < 4; __i++) {
			if (__c & 8)
				__r[4 * __l + __i] = 0.0f;
			else if (__c & 2)
				__r[4 * __l + __i] = __y[4 * (__c & 1) + __i];
			else
				__r[4 * __l + __i] = __x[4 * (__c & 1) + __i];
		}
	}
	return (__m256)__r;
}

#define _mm256_permute2f128_ps(a, b, imm) __mcc_permute2f128_ps((a), (b), (int)(imm))

__MCC_AVX_INLINE __m256i __mcc_permute2f128_si256(__m256i __a, __m256i __b, int __imm)
{
	__v4di __x = (__v4di)__a, __y = (__v4di)__b, __r;
	int __l, __i;
	for (__l = 0; __l < 2; __l++) {
		int __c = (__imm >> (4 * __l)) & 0xf;
		for (__i = 0; __i < 2; __i++) {
			if (__c & 8)
				__r[2 * __l + __i] = 0;
			else if (__c & 2)
				__r[2 * __l + __i] = __y[2 * (__c & 1) + __i];
			else
				__r[2 * __l + __i] = __x[2 * (__c & 1) + __i];
		}
	}
	return (__m256i)__r;
}

#define _mm256_permute2f128_si256(a, b, imm) __mcc_permute2f128_si256((a), (b), (int)(imm))

__MCC_AVX_INLINE __m128d __mcc_extractf128_pd(__m256d __a, int __imm)
{
	__v4df __x = (__v4df)__a;
	__v2df __r;
	int __o = (__imm & 1) * 2, __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __x[__o + __i];
	return (__m128d)__r;
}

#define _mm256_extractf128_pd(a, imm) __mcc_extractf128_pd((a), (int)(imm))

__MCC_AVX_INLINE __m128 __mcc_extractf128_ps(__m256 __a, int __imm)
{
	__v8sf __x = (__v8sf)__a;
	__v4sf __r;
	int __o = (__imm & 1) * 4, __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __x[__o + __i];
	return (__m128)__r;
}

#define _mm256_extractf128_ps(a, imm) __mcc_extractf128_ps((a), (int)(imm))

__MCC_AVX_INLINE __m128i __mcc_extractf128_si256(__m256i __a, int __imm)
{
	__v4di __x = (__v4di)__a;
	__v2di __r;
	int __o = (__imm & 1) * 2, __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __x[__o + __i];
	return (__m128i)__r;
}

#define _mm256_extractf128_si256(a, imm) __mcc_extractf128_si256((a), (int)(imm))

__MCC_AVX_INLINE __m256d __mcc_insertf128_pd(__m256d __a, __m128d __b, int __imm)
{
	__v4df __r = (__v4df)__a;
	__v2df __y = (__v2df)__b;
	int __o = (__imm & 1) * 2, __i;
	for (__i = 0; __i < 2; __i++)
		__r[__o + __i] = __y[__i];
	return (__m256d)__r;
}

#define _mm256_insertf128_pd(a, b, imm) __mcc_insertf128_pd((a), (b), (int)(imm))

__MCC_AVX_INLINE __m256 __mcc_insertf128_ps(__m256 __a, __m128 __b, int __imm)
{
	__v8sf __r = (__v8sf)__a;
	__v4sf __y = (__v4sf)__b;
	int __o = (__imm & 1) * 4, __i;
	for (__i = 0; __i < 4; __i++)
		__r[__o + __i] = __y[__i];
	return (__m256)__r;
}

#define _mm256_insertf128_ps(a, b, imm) __mcc_insertf128_ps((a), (b), (int)(imm))

__MCC_AVX_INLINE __m256i __mcc_insertf128_si256(__m256i __a, __m128i __b, int __imm)
{
	__v4di __r = (__v4di)__a;
	__v2di __y = (__v2di)__b;
	int __o = (__imm & 1) * 2, __i;
	for (__i = 0; __i < 2; __i++)
		__r[__o + __i] = __y[__i];
	return (__m256i)__r;
}

#define _mm256_insertf128_si256(a, b, imm) __mcc_insertf128_si256((a), (b), (int)(imm))

#define _mm256_extract_epi8(a, imm) ((int)(unsigned char)((__v32qu)(__m256i)(a))[(imm) & 31])
#define _mm256_extract_epi16(a, imm) ((int)(unsigned short)((__v16hu)(__m256i)(a))[(imm) & 15])
#define _mm256_extract_epi32(a, imm) ((int)((__v8si)(__m256i)(a))[(imm) & 7])

#ifdef __x86_64__
#define _mm256_extract_epi64(a, imm) ((long long)((__v4di)(__m256i)(a))[(imm) & 3])
#endif

__MCC_AVX_INLINE __m256i __mcc_insert_epi8_256(__m256i __a, int __b, int __imm)
{
	__v32qu __r = (__v32qu)__a;
	__r[__imm & 31] = (unsigned char)__b;
	return (__m256i)__r;
}

#define _mm256_insert_epi8(a, b, imm) __mcc_insert_epi8_256((a), (int)(b), (int)(imm))

__MCC_AVX_INLINE __m256i __mcc_insert_epi16_256(__m256i __a, int __b, int __imm)
{
	__v16hu __r = (__v16hu)__a;
	__r[__imm & 15] = (unsigned short)__b;
	return (__m256i)__r;
}

#define _mm256_insert_epi16(a, b, imm) __mcc_insert_epi16_256((a), (int)(b), (int)(imm))

__MCC_AVX_INLINE __m256i __mcc_insert_epi32_256(__m256i __a, int __b, int __imm)
{
	__v8si __r = (__v8si)__a;
	__r[__imm & 7] = __b;
	return (__m256i)__r;
}

#define _mm256_insert_epi32(a, b, imm) __mcc_insert_epi32_256((a), (int)(b), (int)(imm))

#ifdef __x86_64__
__MCC_AVX_INLINE __m256i __mcc_insert_epi64_256(__m256i __a, long long __b, int __imm)
{
	__v4di __r = (__v4di)__a;
	__r[__imm & 3] = __b;
	return (__m256i)__r;
}

#define _mm256_insert_epi64(a, b, imm) __mcc_insert_epi64_256((a), (long long)(b), (int)(imm))
#endif

__MCC_AVX_INLINE __m256d _mm256_unpackhi_pd(__m256d __a, __m256d __b)
{
	return (__m256d)__builtin_shufflevector((__v4df)__a, (__v4df)__b, 1, 5, 3, 7);
}

__MCC_AVX_INLINE __m256d _mm256_unpacklo_pd(__m256d __a, __m256d __b)
{
	return (__m256d)__builtin_shufflevector((__v4df)__a, (__v4df)__b, 0, 4, 2, 6);
}

__MCC_AVX_INLINE __m256 _mm256_unpackhi_ps(__m256 __a, __m256 __b)
{
	return (__m256)__builtin_shufflevector((__v8sf)__a, (__v8sf)__b, 2, 10, 3, 11, 6, 14, 7,
					       15);
}

__MCC_AVX_INLINE __m256 _mm256_unpacklo_ps(__m256 __a, __m256 __b)
{
	return (__m256)__builtin_shufflevector((__v8sf)__a, (__v8sf)__b, 0, 8, 1, 9, 4, 12, 5, 13);
}

__MCC_AVX_INLINE __m256 _mm256_movehdup_ps(__m256 __a)
{
	return (__m256)__builtin_shufflevector((__v8sf)__a, (__v8sf)__a, 1, 1, 3, 3, 5, 5, 7, 7);
}

__MCC_AVX_INLINE __m256 _mm256_moveldup_ps(__m256 __a)
{
	return (__m256)__builtin_shufflevector((__v8sf)__a, (__v8sf)__a, 0, 0, 2, 2, 4, 4, 6, 6);
}

__MCC_AVX_INLINE __m256d _mm256_movedup_pd(__m256d __a)
{
	return (__m256d)__builtin_shufflevector((__v4df)__a, (__v4df)__a, 0, 0, 2, 2);
}

__MCC_AVX_INLINE int _mm256_movemask_pd(__m256d __a)
{
	__v4di __x = (__v4di)__a;
	int __r = 0, __i;
	for (__i = 0; __i < 4; __i++)
		if (__x[__i] < 0)
			__r |= 1 << __i;
	return __r;
}

__MCC_AVX_INLINE int _mm256_movemask_ps(__m256 __a)
{
	__v8si __x = (__v8si)__a;
	int __r = 0, __i;
	for (__i = 0; __i < 8; __i++)
		if (__x[__i] < 0)
			__r |= 1 << __i;
	return __r;
}

__MCC_AVX_INLINE int _mm256_testz_si256(__m256i __a, __m256i __b)
{
	__v4du __x = (__v4du)__a, __y = (__v4du)__b;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__x[__i] & __y[__i])
			return 0;
	return 1;
}

__MCC_AVX_INLINE int _mm256_testc_si256(__m256i __a, __m256i __b)
{
	__v4du __x = (__v4du)__a, __y = (__v4du)__b;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (~__x[__i] & __y[__i])
			return 0;
	return 1;
}

__MCC_AVX_INLINE int _mm256_testnzc_si256(__m256i __a, __m256i __b)
{
	return !_mm256_testz_si256(__a, __b) && !_mm256_testc_si256(__a, __b);
}

__MCC_AVX_INLINE int _mm256_testz_pd(__m256d __a, __m256d __b)
{
	__v4di __x = (__v4di)__a, __y = (__v4di)__b;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if ((__x[__i] & __y[__i]) < 0)
			return 0;
	return 1;
}

__MCC_AVX_INLINE int _mm256_testc_pd(__m256d __a, __m256d __b)
{
	__v4di __x = (__v4di)__a, __y = (__v4di)__b;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if ((~__x[__i] & __y[__i]) < 0)
			return 0;
	return 1;
}

__MCC_AVX_INLINE int _mm256_testnzc_pd(__m256d __a, __m256d __b)
{
	return !_mm256_testz_pd(__a, __b) && !_mm256_testc_pd(__a, __b);
}

__MCC_AVX_INLINE int _mm256_testz_ps(__m256 __a, __m256 __b)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b;
	int __i;
	for (__i = 0; __i < 8; __i++)
		if ((__x[__i] & __y[__i]) < 0)
			return 0;
	return 1;
}

__MCC_AVX_INLINE int _mm256_testc_ps(__m256 __a, __m256 __b)
{
	__v8si __x = (__v8si)__a, __y = (__v8si)__b;
	int __i;
	for (__i = 0; __i < 8; __i++)
		if ((~__x[__i] & __y[__i]) < 0)
			return 0;
	return 1;
}

__MCC_AVX_INLINE int _mm256_testnzc_ps(__m256 __a, __m256 __b)
{
	return !_mm256_testz_ps(__a, __b) && !_mm256_testc_ps(__a, __b);
}

__MCC_AVX_INLINE int _mm_testz_pd(__m128d __a, __m128d __b)
{
	__v2di __x = (__v2di)__a, __y = (__v2di)__b;
	int __i;
	for (__i = 0; __i < 2; __i++)
		if ((__x[__i] & __y[__i]) < 0)
			return 0;
	return 1;
}

__MCC_AVX_INLINE int _mm_testc_pd(__m128d __a, __m128d __b)
{
	__v2di __x = (__v2di)__a, __y = (__v2di)__b;
	int __i;
	for (__i = 0; __i < 2; __i++)
		if ((~__x[__i] & __y[__i]) < 0)
			return 0;
	return 1;
}

__MCC_AVX_INLINE int _mm_testnzc_pd(__m128d __a, __m128d __b)
{
	return !_mm_testz_pd(__a, __b) && !_mm_testc_pd(__a, __b);
}

__MCC_AVX_INLINE int _mm_testz_ps(__m128 __a, __m128 __b)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if ((__x[__i] & __y[__i]) < 0)
			return 0;
	return 1;
}

__MCC_AVX_INLINE int _mm_testc_ps(__m128 __a, __m128 __b)
{
	__v4si __x = (__v4si)__a, __y = (__v4si)__b;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if ((~__x[__i] & __y[__i]) < 0)
			return 0;
	return 1;
}

__MCC_AVX_INLINE int _mm_testnzc_ps(__m128 __a, __m128 __b)
{
	return !_mm_testz_ps(__a, __b) && !_mm_testc_ps(__a, __b);
}

__MCC_AVX_INLINE __m256d _mm256_load_pd(double const *__p)
{
	return *(__m256d *)__p;
}

__MCC_AVX_INLINE __m256 _mm256_load_ps(float const *__p)
{
	return *(__m256 *)__p;
}

__MCC_AVX_INLINE __m256i _mm256_load_si256(__m256i const *__p)
{
	return *__p;
}

__MCC_AVX_INLINE __m256d _mm256_loadu_pd(double const *__p)
{
	return (__m256d)((const struct __mcc_loadu_pd256 *)__p)->__v;
}

__MCC_AVX_INLINE __m256 _mm256_loadu_ps(float const *__p)
{
	return (__m256)((const struct __mcc_loadu_ps256 *)__p)->__v;
}

__MCC_AVX_INLINE __m256i _mm256_loadu_si256(__m256i_u const *__p)
{
	return (__m256i)((const struct __mcc_loadu_si256 *)__p)->__v;
}

__MCC_AVX_INLINE __m256i _mm256_lddqu_si256(__m256i const *__p)
{
	return (__m256i)((const struct __mcc_loadu_si256 *)__p)->__v;
}

__MCC_AVX_INLINE void _mm256_store_pd(double *__p, __m256d __a)
{
	*(__m256d *)__p = __a;
}

__MCC_AVX_INLINE void _mm256_store_ps(float *__p, __m256 __a)
{
	*(__m256 *)__p = __a;
}

__MCC_AVX_INLINE void _mm256_store_si256(__m256i *__p, __m256i __a)
{
	*__p = __a;
}

__MCC_AVX_INLINE void _mm256_storeu_pd(double *__p, __m256d __a)
{
	((struct __mcc_loadu_pd256 *)__p)->__v = (__v4df)__a;
}

__MCC_AVX_INLINE void _mm256_storeu_ps(float *__p, __m256 __a)
{
	((struct __mcc_loadu_ps256 *)__p)->__v = (__v8sf)__a;
}

__MCC_AVX_INLINE void _mm256_storeu_si256(__m256i_u *__p, __m256i __a)
{
	((struct __mcc_loadu_si256 *)__p)->__v = (__v4di)__a;
}

__MCC_AVX_INLINE void _mm256_stream_pd(double *__p, __m256d __a)
{
	*(__m256d *)__p = __a;
}

__MCC_AVX_INLINE void _mm256_stream_ps(float *__p, __m256 __a)
{
	*(__m256 *)__p = __a;
}

__MCC_AVX_INLINE void _mm256_stream_si256(__m256i *__p, __m256i __a)
{
	*__p = __a;
}

__MCC_AVX_INLINE __m256d _mm256_loadu2_m128d(double const *__hi, double const *__lo)
{
	__v4df __r;
	__v2df __l = (__v2df)_mm_loadu_pd(__lo), __h = (__v2df)_mm_loadu_pd(__hi);
	__r[0] = __l[0];
	__r[1] = __l[1];
	__r[2] = __h[0];
	__r[3] = __h[1];
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m256 _mm256_loadu2_m128(float const *__hi, float const *__lo)
{
	__v8sf __r;
	__v4sf __l = (__v4sf)_mm_loadu_ps(__lo), __h = (__v4sf)_mm_loadu_ps(__hi);
	int __i;
	for (__i = 0; __i < 4; __i++) {
		__r[__i] = __l[__i];
		__r[__i + 4] = __h[__i];
	}
	return (__m256)__r;
}

__MCC_AVX_INLINE __m256i _mm256_loadu2_m128i(__m128i_u const *__hi, __m128i_u const *__lo)
{
	__v4di __r;
	__v2di __l = (__v2di)_mm_loadu_si128((const __m128i *)__lo);
	__v2di __h = (__v2di)_mm_loadu_si128((const __m128i *)__hi);
	__r[0] = __l[0];
	__r[1] = __l[1];
	__r[2] = __h[0];
	__r[3] = __h[1];
	return (__m256i)__r;
}

__MCC_AVX_INLINE void _mm256_storeu2_m128d(double *__hi, double *__lo, __m256d __a)
{
	_mm_storeu_pd(__lo, _mm256_castpd256_pd128(__a));
	_mm_storeu_pd(__hi, _mm256_extractf128_pd(__a, 1));
}

__MCC_AVX_INLINE void _mm256_storeu2_m128(float *__hi, float *__lo, __m256 __a)
{
	_mm_storeu_ps(__lo, _mm256_castps256_ps128(__a));
	_mm_storeu_ps(__hi, _mm256_extractf128_ps(__a, 1));
}

__MCC_AVX_INLINE void _mm256_storeu2_m128i(__m128i_u *__hi, __m128i_u *__lo, __m256i __a)
{
	_mm_storeu_si128((__m128i *)__lo, _mm256_castsi256_si128(__a));
	_mm_storeu_si128((__m128i *)__hi, _mm256_extractf128_si256(__a, 1));
}

__MCC_AVX_INLINE __m256d _mm256_maskload_pd(double const *__p, __m256i __m)
{
	__v4di __k = (__v4di)__m;
	__v4df __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __k[__i] < 0 ? ((const struct __mcc_loadu_f64 *)(__p + __i))->__v : 0.0;
	return (__m256d)__r;
}

__MCC_AVX_INLINE __m128d _mm_maskload_pd(double const *__p, __m128i __m)
{
	__v2di __k = (__v2di)__m;
	__v2df __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __k[__i] < 0 ? ((const struct __mcc_loadu_f64 *)(__p + __i))->__v : 0.0;
	return (__m128d)__r;
}

__MCC_AVX_INLINE __m256 _mm256_maskload_ps(float const *__p, __m256i __m)
{
	__v8si __k = (__v8si)__m;
	__v8sf __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = __k[__i] < 0 ? ((const struct __mcc_loadu_f32 *)(__p + __i))->__v : 0.0f;
	return (__m256)__r;
}

__MCC_AVX_INLINE __m128 _mm_maskload_ps(float const *__p, __m128i __m)
{
	__v4si __k = (__v4si)__m;
	__v4sf __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __k[__i] < 0 ? ((const struct __mcc_loadu_f32 *)(__p + __i))->__v : 0.0f;
	return (__m128)__r;
}

__MCC_AVX_INLINE void _mm256_maskstore_pd(double *__p, __m256i __m, __m256d __a)
{
	__v4di __k = (__v4di)__m;
	__v4df __x = (__v4df)__a;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__k[__i] < 0)
			((struct __mcc_loadu_f64 *)(__p + __i))->__v = __x[__i];
}

__MCC_AVX_INLINE void _mm_maskstore_pd(double *__p, __m128i __m, __m128d __a)
{
	__v2di __k = (__v2di)__m;
	__v2df __x = (__v2df)__a;
	int __i;
	for (__i = 0; __i < 2; __i++)
		if (__k[__i] < 0)
			((struct __mcc_loadu_f64 *)(__p + __i))->__v = __x[__i];
}

__MCC_AVX_INLINE void _mm256_maskstore_ps(float *__p, __m256i __m, __m256 __a)
{
	__v8si __k = (__v8si)__m;
	__v8sf __x = (__v8sf)__a;
	int __i;
	for (__i = 0; __i < 8; __i++)
		if (__k[__i] < 0)
			((struct __mcc_loadu_f32 *)(__p + __i))->__v = __x[__i];
}

__MCC_AVX_INLINE void _mm_maskstore_ps(float *__p, __m128i __m, __m128 __a)
{
	__v4si __k = (__v4si)__m;
	__v4sf __x = (__v4sf)__a;
	int __i;
	for (__i = 0; __i < 4; __i++)
		if (__k[__i] < 0)
			((struct __mcc_loadu_f32 *)(__p + __i))->__v = __x[__i];
}

__MCC_AVX_INLINE __m128 _mm_broadcast_ss(float const *__p)
{
	float __v = ((const struct __mcc_loadu_f32 *)__p)->__v;
	return (__m128)(__v4sf){__v, __v, __v, __v};
}

__MCC_AVX_INLINE __m256 _mm256_broadcast_ss(float const *__p)
{
	float __v = ((const struct __mcc_loadu_f32 *)__p)->__v;
	return (__m256)(__v8sf){__v, __v, __v, __v, __v, __v, __v, __v};
}

__MCC_AVX_INLINE __m256d _mm256_broadcast_sd(double const *__p)
{
	double __v = ((const struct __mcc_loadu_f64 *)__p)->__v;
	return (__m256d)(__v4df){__v, __v, __v, __v};
}

__MCC_AVX_INLINE __m256 _mm256_broadcast_ps(__m128 const *__p)
{
	__v4sf __v = (__v4sf)((const struct __mcc_loadu_ps *)__p)->__v;
	return (__m256)__builtin_shufflevector(__v, __v, 0, 1, 2, 3, 0, 1, 2, 3);
}

__MCC_AVX_INLINE __m256d _mm256_broadcast_pd(__m128d const *__p)
{
	__v2df __v = (__v2df)((const struct __mcc_loadu_pd *)__p)->__v;
	return (__m256d)__builtin_shufflevector(__v, __v, 0, 1, 0, 1);
}

__MCC_AVX_INLINE __m256 _mm256_set_m128(__m128 __hi, __m128 __lo)
{
	return (__m256)__builtin_shufflevector((__v4sf)__lo, (__v4sf)__hi, 0, 1, 2, 3, 4, 5, 6, 7);
}

__MCC_AVX_INLINE __m256d _mm256_set_m128d(__m128d __hi, __m128d __lo)
{
	return (__m256d)__builtin_shufflevector((__v2df)__lo, (__v2df)__hi, 0, 1, 2, 3);
}

__MCC_AVX_INLINE __m256i _mm256_set_m128i(__m128i __hi, __m128i __lo)
{
	return (__m256i)__builtin_shufflevector((__v2di)__lo, (__v2di)__hi, 0, 1, 2, 3);
}

__MCC_AVX_INLINE __m256 _mm256_setr_m128(__m128 __lo, __m128 __hi)
{
	return _mm256_set_m128(__hi, __lo);
}

__MCC_AVX_INLINE __m256d _mm256_setr_m128d(__m128d __lo, __m128d __hi)
{
	return _mm256_set_m128d(__hi, __lo);
}

__MCC_AVX_INLINE __m256i _mm256_setr_m128i(__m128i __lo, __m128i __hi)
{
	return _mm256_set_m128i(__hi, __lo);
}

__MCC_AVX_INLINE __m256d _mm256_undefined_pd(void)
{
	return _mm256_setzero_pd();
}

__MCC_AVX_INLINE __m256 _mm256_undefined_ps(void)
{
	return _mm256_setzero_ps();
}

__MCC_AVX_INLINE __m256i _mm256_undefined_si256(void)
{
	return _mm256_setzero_si256();
}

__MCC_AVX_INLINE void _mm256_zeroall(void)
{
}

__MCC_AVX_INLINE void _mm256_zeroupper(void)
{
}

#endif
