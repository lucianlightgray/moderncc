#ifndef _POPCNTINTRIN_H_INCLUDED
#define _POPCNTINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "popcntintrin.h is only supported on x86 targets"
#endif

#define __MCC_POPCNT_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_POPCNT_INLINE int _mm_popcnt_u32(unsigned int __x)
{
	return __builtin_popcount(__x);
}

#ifdef __x86_64__
__MCC_POPCNT_INLINE long long _mm_popcnt_u64(unsigned long long __x)
{
	return __builtin_popcountll(__x);
}
#endif

#endif
