#ifndef _BMI2INTRIN_H_INCLUDED
#define _BMI2INTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "bmi2intrin.h is only supported on x86 targets"
#endif

#define __MCC_BMI2_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_BMI2_INLINE unsigned int _bzhi_u32(unsigned int __x, unsigned int __y)
{
	unsigned int __n = __y & 0xffu;
	if (__n > 31u)
		return __x;
	return __x & ((1u << __n) - 1u);
}

__MCC_BMI2_INLINE unsigned int _pdep_u32(unsigned int __x, unsigned int __m)
{
	unsigned int __r = 0u, __b = 1u;
	while (__m) {
		unsigned int __l = __m & (unsigned int)(-(int)__m);
		if (__x & __b)
			__r |= __l;
		__m ^= __l;
		__b <<= 1;
	}
	return __r;
}

__MCC_BMI2_INLINE unsigned int _pext_u32(unsigned int __x, unsigned int __m)
{
	unsigned int __r = 0u, __b = 1u;
	while (__m) {
		unsigned int __l = __m & (unsigned int)(-(int)__m);
		if (__x & __l)
			__r |= __b;
		__m ^= __l;
		__b <<= 1;
	}
	return __r;
}

#ifndef __x86_64__
__MCC_BMI2_INLINE unsigned int _mulx_u32(unsigned int __x, unsigned int __y,
																				 unsigned int *__p)
{
	unsigned long long __r = (unsigned long long)__x * (unsigned long long)__y;
	*__p = (unsigned int)(__r >> 32);
	return (unsigned int)__r;
}
#endif

#ifdef __x86_64__
__MCC_BMI2_INLINE unsigned long long _bzhi_u64(unsigned long long __x,
																							unsigned long long __y)
{
	unsigned long long __n = __y & 0xffull;
	if (__n > 63ull)
		return __x;
	return __x & ((1ull << __n) - 1ull);
}

__MCC_BMI2_INLINE unsigned long long _pdep_u64(unsigned long long __x,
																							 unsigned long long __m)
{
	unsigned long long __r = 0ull, __b = 1ull;
	while (__m) {
		unsigned long long __l = __m & (unsigned long long)(-(long long)__m);
		if (__x & __b)
			__r |= __l;
		__m ^= __l;
		__b <<= 1;
	}
	return __r;
}

__MCC_BMI2_INLINE unsigned long long _pext_u64(unsigned long long __x,
																							 unsigned long long __m)
{
	unsigned long long __r = 0ull, __b = 1ull;
	while (__m) {
		unsigned long long __l = __m & (unsigned long long)(-(long long)__m);
		if (__x & __l)
			__r |= __b;
		__m ^= __l;
		__b <<= 1;
	}
	return __r;
}

#ifdef __SIZEOF_INT128__
__MCC_BMI2_INLINE unsigned long long _mulx_u64(unsigned long long __x,
																							unsigned long long __y,
																							unsigned long long *__p)
{
	unsigned __int128 __r = (unsigned __int128)__x * (unsigned __int128)__y;
	*__p = (unsigned long long)(__r >> 64);
	return (unsigned long long)__r;
}
#endif
#endif

#endif
