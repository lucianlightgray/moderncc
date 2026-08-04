#ifndef _ADXINTRIN_H_INCLUDED
#define _ADXINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "adxintrin.h is only supported on x86 targets"
#endif

#define __MCC_ADX_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_ADX_INLINE unsigned char _addcarry_u32(unsigned char __c, unsigned int __x,
																						 unsigned int __y, unsigned int *__p)
{
	return __builtin_ia32_addcarryx_u32(__c, __x, __y, __p);
}

__MCC_ADX_INLINE unsigned char _addcarryx_u32(unsigned char __c, unsigned int __x,
																							unsigned int __y, unsigned int *__p)
{
	return __builtin_ia32_addcarryx_u32(__c, __x, __y, __p);
}

__MCC_ADX_INLINE unsigned char _subborrow_u32(unsigned char __c, unsigned int __x,
																							unsigned int __y, unsigned int *__p)
{
	return __builtin_ia32_sbb_u32(__c, __x, __y, __p);
}

#ifdef __x86_64__
__MCC_ADX_INLINE unsigned char _addcarry_u64(unsigned char __c, unsigned long long __x,
																						 unsigned long long __y,
																						 unsigned long long *__p)
{
	return __builtin_ia32_addcarryx_u64(__c, __x, __y, __p);
}

__MCC_ADX_INLINE unsigned char _addcarryx_u64(unsigned char __c, unsigned long long __x,
																							unsigned long long __y,
																							unsigned long long *__p)
{
	return __builtin_ia32_addcarryx_u64(__c, __x, __y, __p);
}

__MCC_ADX_INLINE unsigned char _subborrow_u64(unsigned char __c, unsigned long long __x,
																							unsigned long long __y,
																							unsigned long long *__p)
{
	return __builtin_ia32_sbb_u64(__c, __x, __y, __p);
}
#endif

#endif
