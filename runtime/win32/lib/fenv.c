#define __FE_ROUND_MASK 0x0c00

#if defined(__x86_64__) || defined(__i386__)

int fesetround(int __round)
{
	unsigned short __cw;
	unsigned int __mxcsr;

	if (__round & ~__FE_ROUND_MASK)
		return -1;
	__asm__ __volatile__("fnstcw %0" : "=m"(__cw));
	__cw = (unsigned short)((__cw & ~__FE_ROUND_MASK) | __round);
	__asm__ __volatile__("fldcw %0" : : "m"(__cw));
	__asm__ __volatile__("stmxcsr %0" : "=m"(__mxcsr));
	__mxcsr = (__mxcsr & ~(__FE_ROUND_MASK << 3)) | ((unsigned)__round << 3);
	__asm__ __volatile__("ldmxcsr %0" : : "m"(__mxcsr));
	return 0;
}

int fegetround(void)
{
	unsigned int __mxcsr;

	__asm__ __volatile__("stmxcsr %0" : "=m"(__mxcsr));
	return (int)((__mxcsr >> 3) & __FE_ROUND_MASK);
}

#elif defined(__aarch64__)

int fesetround(int __round)
{
	unsigned long __fpcr;
	unsigned __rm;
	static const unsigned char __fe_to_fpcr[4] = {0, 2, 1, 3};

	if (__round & ~__FE_ROUND_MASK)
		return -1;
	__rm = __fe_to_fpcr[(__round >> 10) & 3];
	__asm__ __volatile__("mrs %0, fpcr" : "=r"(__fpcr));
	__fpcr = (__fpcr & ~(3UL << 22)) | ((unsigned long)__rm << 22);
	__asm__ __volatile__("msr fpcr, %0" : : "r"(__fpcr));
	return 0;
}

int fegetround(void)
{
	unsigned long __fpcr;
	static const int __fpcr_to_fe[4] = {0x000, 0x800, 0x400, 0xc00};

	__asm__ __volatile__("mrs %0, fpcr" : "=r"(__fpcr));
	return __fpcr_to_fe[(__fpcr >> 22) & 3];
}

#else

int fesetround(int __round) { return __round == 0 ? 0 : -1; }
int fegetround(void) { return 0; }

#endif
