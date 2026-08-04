#ifndef _LZCNTINTRIN_H_INCLUDED
#define _LZCNTINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "lzcntintrin.h is only supported on x86 targets"
#endif

#define __MCC_LZCNT_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_LZCNT_INLINE unsigned short __lzcnt16(unsigned short __x)
{
	return __x ? (unsigned short)(__builtin_clz((unsigned int)__x) - 16)
						 : (unsigned short)16;
}

__MCC_LZCNT_INLINE unsigned int __lzcnt32(unsigned int __x)
{
	return __x ? (unsigned int)__builtin_clz(__x) : 32u;
}

#define __lzcnt(x) __lzcnt32(x)
#define _lzcnt_u32(x) __lzcnt32(x)

#ifdef __x86_64__
__MCC_LZCNT_INLINE unsigned long long __lzcnt64(unsigned long long __x)
{
	return __x ? (unsigned long long)__builtin_clzll(__x) : 64ull;
}

#define _lzcnt_u64(x) __lzcnt64(x)
#endif

#endif
