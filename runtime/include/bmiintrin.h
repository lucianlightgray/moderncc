#ifndef _BMIINTRIN_H_INCLUDED
#define _BMIINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "bmiintrin.h is only supported on x86 targets"
#endif

#define __MCC_BMI_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_BMI_INLINE unsigned short __tzcnt_u16(unsigned short __x)
{
	return __x ? (unsigned short)__builtin_ctz((unsigned int)__x) : (unsigned short)16;
}

__MCC_BMI_INLINE unsigned int __andn_u32(unsigned int __x, unsigned int __y)
{
	return ~__x & __y;
}

__MCC_BMI_INLINE unsigned int __bextr_u32(unsigned int __x, unsigned int __y)
{
	unsigned int __s = __y & 0xffu, __l = (__y >> 8) & 0xffu;
	if (__s > 31u)
		return 0u;
	__x >>= __s;
	if (__l > 31u)
		return __x;
	return __x & ((1u << __l) - 1u);
}

__MCC_BMI_INLINE unsigned int _bextr_u32(unsigned int __x, unsigned int __s,
																				 unsigned int __l)
{
	return __bextr_u32(__x, (__s & 0xffu) | ((__l & 0xffu) << 8));
}

__MCC_BMI_INLINE unsigned int __blsi_u32(unsigned int __x)
{
	return __x & (unsigned int)(-(int)__x);
}

__MCC_BMI_INLINE unsigned int __blsmsk_u32(unsigned int __x)
{
	return __x ^ (__x - 1u);
}

__MCC_BMI_INLINE unsigned int __blsr_u32(unsigned int __x)
{
	return __x & (__x - 1u);
}

__MCC_BMI_INLINE unsigned int __tzcnt_u32(unsigned int __x)
{
	return __x ? (unsigned int)__builtin_ctz(__x) : 32u;
}

#define _tzcnt_u16(x) __tzcnt_u16(x)
#define _andn_u32(x, y) __andn_u32((x), (y))
#define _blsi_u32(x) __blsi_u32(x)
#define _blsmsk_u32(x) __blsmsk_u32(x)
#define _blsr_u32(x) __blsr_u32(x)
#define _tzcnt_u32(x) __tzcnt_u32(x)

#ifdef __x86_64__
__MCC_BMI_INLINE unsigned long long __andn_u64(unsigned long long __x,
																							 unsigned long long __y)
{
	return ~__x & __y;
}

__MCC_BMI_INLINE unsigned long long __bextr_u64(unsigned long long __x,
																								unsigned long long __y)
{
	unsigned long long __s = __y & 0xffull, __l = (__y >> 8) & 0xffull;
	if (__s > 63ull)
		return 0ull;
	__x >>= __s;
	if (__l > 63ull)
		return __x;
	return __x & ((1ull << __l) - 1ull);
}

__MCC_BMI_INLINE unsigned long long _bextr_u64(unsigned long long __x,
																							 unsigned int __s, unsigned int __l)
{
	return __bextr_u64(__x, (__s & 0xffu) | ((unsigned long long)(__l & 0xffu) << 8));
}

__MCC_BMI_INLINE unsigned long long __blsi_u64(unsigned long long __x)
{
	return __x & (unsigned long long)(-(long long)__x);
}

__MCC_BMI_INLINE unsigned long long __blsmsk_u64(unsigned long long __x)
{
	return __x ^ (__x - 1ull);
}

__MCC_BMI_INLINE unsigned long long __blsr_u64(unsigned long long __x)
{
	return __x & (__x - 1ull);
}

__MCC_BMI_INLINE unsigned long long __tzcnt_u64(unsigned long long __x)
{
	return __x ? (unsigned long long)__builtin_ctzll(__x) : 64ull;
}

#define _andn_u64(x, y) __andn_u64((x), (y))
#define _blsi_u64(x) __blsi_u64(x)
#define _blsmsk_u64(x) __blsmsk_u64(x)
#define _blsr_u64(x) __blsr_u64(x)
#define _tzcnt_u64(x) __tzcnt_u64(x)
#endif

#endif
