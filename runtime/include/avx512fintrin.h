#ifndef _AVX512FINTRIN_H_INCLUDED
#define _AVX512FINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "avx512fintrin.h is only supported on x86 targets"
#endif

/* T-lin-10006: the AVX-512 512-bit vector type spellings. The element types
 * (float/double/long long) are ordinary vector elements, so these lower and run
 * generically; mcc does not require AVX-512 hardware to compute them (unlike gcc,
 * which emits native AVX-512 for them). */
typedef float __m512 __attribute__((__vector_size__(64), __aligned__(64)));
typedef double __m512d __attribute__((__vector_size__(64), __aligned__(64)));
typedef long long __m512i __attribute__((__vector_size__(64), __aligned__(64)));
typedef float __m512_u __attribute__((__vector_size__(64), __aligned__(1)));
typedef double __m512d_u __attribute__((__vector_size__(64), __aligned__(1)));
typedef long long __m512i_u __attribute__((__vector_size__(64), __aligned__(1)));

typedef float __v16sf __attribute__((__vector_size__(64)));
typedef double __v8df __attribute__((__vector_size__(64)));
typedef long long __v8di __attribute__((__vector_size__(64)));
typedef int __v16si __attribute__((__vector_size__(64)));
typedef short __v32hi __attribute__((__vector_size__(64)));
typedef char __v64qi __attribute__((__vector_size__(64)));

#endif /* _AVX512FINTRIN_H_INCLUDED */
