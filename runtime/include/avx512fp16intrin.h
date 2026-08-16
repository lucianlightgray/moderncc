#ifndef _AVX512FP16INTRIN_H_INCLUDED
#define _AVX512FP16INTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "avx512fp16intrin.h is only supported on x86 targets"
#endif

/* T-lin-10006: the half-precision (_Float16) AVX-512-FP16 vector type spellings.
 * mcc allows _Float16 as a vector element (see apply_attr_vector_size) and lowers
 * these generically, so 128/256-bit forms run and match gcc + clang byte-for-byte
 * without the AVX512-FP16 ISA (tests/exec/types/fp16_vector.c). The 512-bit form
 * parses; its native ABI is only exercisable on AVX-512 hardware. */
typedef _Float16 __m128h __attribute__((__vector_size__(16), __aligned__(16)));
typedef _Float16 __m256h __attribute__((__vector_size__(32), __aligned__(32)));
typedef _Float16 __m512h __attribute__((__vector_size__(64), __aligned__(64)));
typedef _Float16 __m128h_u __attribute__((__vector_size__(16), __aligned__(1)));
typedef _Float16 __m256h_u __attribute__((__vector_size__(32), __aligned__(1)));
typedef _Float16 __m512h_u __attribute__((__vector_size__(64), __aligned__(1)));

typedef _Float16 __v8hf __attribute__((__vector_size__(16)));
typedef _Float16 __v16hf __attribute__((__vector_size__(32)));
typedef _Float16 __v32hf __attribute__((__vector_size__(64)));

#endif /* _AVX512FP16INTRIN_H_INCLUDED */
