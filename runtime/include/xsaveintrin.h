#ifndef _XSAVEINTRIN_H_INCLUDED
#define _XSAVEINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "xsaveintrin.h is only supported on x86 targets"
#endif

#define __MCC_XSAVE_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_XSAVE_INLINE void _xsave(void *__p, long long __m)
{
	unsigned int __lo = (unsigned int)__m, __hi = (unsigned int)((unsigned long long)__m >> 32);
	__asm__ __volatile__("xsave %0" : "=m"(*(char *)__p) : "a"(__lo), "d"(__hi) : "memory");
}

__MCC_XSAVE_INLINE void _xrstor(void *__p, long long __m)
{
	unsigned int __lo = (unsigned int)__m, __hi = (unsigned int)((unsigned long long)__m >> 32);
	__asm__ __volatile__("xrstor %0" : : "m"(*(const char *)__p), "a"(__lo), "d"(__hi) : "memory");
}

__MCC_XSAVE_INLINE void _xsetbv(unsigned int __a, long long __v)
{
	unsigned int __lo = (unsigned int)__v, __hi = (unsigned int)((unsigned long long)__v >> 32);
	__asm__ __volatile__("xsetbv" : : "c"(__a), "a"(__lo), "d"(__hi));
}

__MCC_XSAVE_INLINE long long _xgetbv(unsigned int __a)
{
	unsigned int __lo, __hi;
	__asm__ __volatile__("xgetbv" : "=a"(__lo), "=d"(__hi) : "c"(__a));
	return (long long)(((unsigned long long)__hi << 32) | __lo);
}

#ifdef __x86_64__
__MCC_XSAVE_INLINE void _xsave64(void *__p, long long __m)
{
	unsigned int __lo = (unsigned int)__m, __hi = (unsigned int)((unsigned long long)__m >> 32);
	__asm__ __volatile__("xsave64 %0" : "=m"(*(char *)__p) : "a"(__lo), "d"(__hi) : "memory");
}

__MCC_XSAVE_INLINE void _xrstor64(void *__p, long long __m)
{
	unsigned int __lo = (unsigned int)__m, __hi = (unsigned int)((unsigned long long)__m >> 32);
	__asm__ __volatile__("xrstor64 %0" : : "m"(*(const char *)__p), "a"(__lo), "d"(__hi) : "memory");
}
#endif

#endif
