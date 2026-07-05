/*
 * fesetround / fegetround for the mcc WIN32/PE (msvcrt) target.
 *
 * msvcrt exports neither, so <fenv.h>'s rounding-mode control is otherwise
 * unresolved on PE (on ELF/Mach-O the system libm provides these, so this file
 * is compiled into libmccrt only for WIN32 targets).  The rounding-mode value
 * is the C99 FE_* encoding, which matches the x87 control-word field (bits
 * 10-11): FE_TONEAREST 0x000, FE_DOWNWARD 0x400, FE_UPWARD 0x800,
 * FE_TOWARDZERO 0xc00.
 */

#define __FE_ROUND_MASK 0x0c00

#if defined(__x86_64__) || defined(__i386__)

/*
 * x86: set both the x87 control word (bits 10-11) and the SSE MXCSR (bits
 * 13-14 == the x87 field << 3), since scalar FP on this target uses SSE while
 * long double / legacy paths use the x87 stack.  Read back from MXCSR.
 */
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

/*
 * arm64: rounding mode is FPCR bits 23:22 (00 RN, 01 RP, 10 RM, 11 RZ).  Map
 * to/from the FE_* field (value >> 10): 0 RN, 1 RM, 2 RP, 3 RZ.
 */
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

#else /* arm (wince) and any other PE arch without an inline-asm path here */

/* No control-register access implemented: accept the default only. */
int fesetround(int __round) { return __round == 0 ? 0 : -1; }
int fegetround(void) { return 0; }

#endif
