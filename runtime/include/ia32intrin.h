#ifndef _IA32INTRIN_H_INCLUDED
#define _IA32INTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "ia32intrin.h is only supported on x86 targets"
#endif

#define __MCC_GPR_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_GPR_INLINE int __bsfd(int __x)
{
	return __builtin_ctz((unsigned int)__x);
}

__MCC_GPR_INLINE int __bsrd(int __x)
{
	return 31 - __builtin_clz((unsigned int)__x);
}

__MCC_GPR_INLINE int __bswapd(int __x)
{
	return (int)__builtin_bswap32((unsigned int)__x);
}

__MCC_GPR_INLINE int __popcntd(unsigned int __x)
{
	return __builtin_popcount(__x);
}

#ifdef __x86_64__
__MCC_GPR_INLINE int __bsfq(long long __x)
{
	return __builtin_ctzll((unsigned long long)__x);
}

__MCC_GPR_INLINE int __bsrq(long long __x)
{
	return 63 - __builtin_clzll((unsigned long long)__x);
}

__MCC_GPR_INLINE long long __bswapq(long long __x)
{
	return (long long)__builtin_bswap64((unsigned long long)__x);
}

__MCC_GPR_INLINE int __popcntq(unsigned long long __x)
{
	return __builtin_popcountll(__x);
}
#endif

__MCC_GPR_INLINE unsigned char __rolb(unsigned char __x, int __c)
{
	__c &= 7;
	return (unsigned char)((__x << __c) | (__x >> (-__c & 7)));
}

__MCC_GPR_INLINE unsigned char __rorb(unsigned char __x, int __c)
{
	__c &= 7;
	return (unsigned char)((__x >> __c) | (__x << (-__c & 7)));
}

__MCC_GPR_INLINE unsigned short __rolw(unsigned short __x, int __c)
{
	__c &= 15;
	return (unsigned short)((__x << __c) | (__x >> (-__c & 15)));
}

__MCC_GPR_INLINE unsigned short __rorw(unsigned short __x, int __c)
{
	__c &= 15;
	return (unsigned short)((__x >> __c) | (__x << (-__c & 15)));
}

__MCC_GPR_INLINE unsigned int __rold(unsigned int __x, int __c)
{
	__c &= 31;
	return (__x << __c) | (__x >> (-__c & 31));
}

__MCC_GPR_INLINE unsigned int __rord(unsigned int __x, int __c)
{
	__c &= 31;
	return (__x >> __c) | (__x << (-__c & 31));
}

#ifdef __x86_64__
__MCC_GPR_INLINE unsigned long long __rolq(unsigned long long __x, int __c)
{
	__c &= 63;
	return (__x << __c) | (__x >> (-__c & 63));
}

__MCC_GPR_INLINE unsigned long long __rorq(unsigned long long __x, int __c)
{
	__c &= 63;
	return (__x >> __c) | (__x << (-__c & 63));
}
#endif

#define _bit_scan_forward(a) __bsfd(a)
#define _bit_scan_reverse(a) __bsrd(a)
#define _bswap(a) __bswapd(a)
#define _popcnt32(a) __popcntd(a)
#define _rdpmc(a) __rdpmc(a)
#define _rdtscp(a) __rdtscp(a)
#define _rdtsc() __rdtsc()
#define _rotl(a, b) __rold((a), (b))
#define _rotr(a, b) __rord((a), (b))
#define _rotwl(a, b) __rolw((a), (b))
#define _rotwr(a, b) __rorw((a), (b))

#ifdef __x86_64__
#define _bswap64(a) __bswapq(a)
#define _popcnt64(a) __popcntq(a)
#endif

#ifdef __LP64__
#define _lrotl(a, b) __rolq((a), (b))
#define _lrotr(a, b) __rorq((a), (b))
#else
#define _lrotl(a, b) __rold((a), (b))
#define _lrotr(a, b) __rord((a), (b))
#endif

__MCC_GPR_INLINE unsigned long long __rdtsc(void)
{
	unsigned int __lo, __hi;
	__asm__ volatile("rdtsc" : "=a"(__lo), "=d"(__hi));
	return ((unsigned long long)__hi << 32) | __lo;
}

__MCC_GPR_INLINE unsigned long long __rdtscp(unsigned int *__aux)
{
	unsigned int __lo, __hi, __a;
	__asm__ volatile(".byte 0x0f, 0x01, 0xf9"
									 : "=a"(__lo), "=d"(__hi), "=c"(__a));
	*__aux = __a;
	return ((unsigned long long)__hi << 32) | __lo;
}

__MCC_GPR_INLINE unsigned long long __rdpmc(int __s)
{
	unsigned int __lo, __hi;
	__asm__ volatile("rdpmc" : "=a"(__lo), "=d"(__hi) : "c"(__s));
	return ((unsigned long long)__hi << 32) | __lo;
}

#ifdef __x86_64__
#define __readeflags()                                       \
	(__extension__({                                           \
		unsigned long long __mcc_ef;                             \
		__asm__ volatile("pushfq; popq %0" : "=r"(__mcc_ef));     \
		__mcc_ef;                                                \
	}))

#define __writeeflags(x)                                     \
	(__extension__({                                           \
		unsigned long long __mcc_ef = (x);                       \
		__asm__ volatile("pushq %0; popfq" : : "r"(__mcc_ef) : "cc"); \
	}))
#else
#define __readeflags()                                       \
	(__extension__({                                           \
		unsigned int __mcc_ef;                                   \
		__asm__ volatile("pushfl; popl %0" : "=r"(__mcc_ef));     \
		__mcc_ef;                                                \
	}))

#define __writeeflags(x)                                     \
	(__extension__({                                           \
		unsigned int __mcc_ef = (x);                             \
		__asm__ volatile("pushl %0; popfl" : : "r"(__mcc_ef) : "cc"); \
	}))
#endif

__MCC_GPR_INLINE void __pause(void)
{
	__asm__ volatile("pause" : : : "memory");
}

__MCC_GPR_INLINE void _wbinvd(void)
{
	__asm__ volatile("wbinvd" : : : "memory");
}

#endif
