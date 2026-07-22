#if defined __arm__

#ifdef __MCC__

unsigned _mccsyscall(unsigned syscall_nr, ...);

__asm__(
		".global _mccsyscall\n"
		"_mccsyscall:\n"
		"push    {r7, lr}\n\t"
		"mov     r7, r0\n\t"
		"mov     r0, r1\n\t"
		"mov     r1, r2\n\t"
		"mov     r2, r3\n\t"
		"svc     #0\n\t"
		"pop     {r7, pc}");

#if defined(__thumb__) || defined(__ARM_EABI__)
#define __NR_SYSCALL_BASE 0x0
#else
#define __NR_SYSCALL_BASE 0x900000
#endif
#define __ARM_NR_BASE (__NR_SYSCALL_BASE + 0x0f0000)
#define __ARM_NR_cacheflush (__ARM_NR_BASE + 2)

#define syscall _mccsyscall

#else

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

#endif

void __clear_cache(void *beginning, void *end) {
	syscall(__ARM_NR_cacheflush, beginning, end, 0);
}

#elif defined __aarch64__
void __clear_cache(void *beg, void *end) {

#ifdef __MCC__
	__arm64_clear_cache(beg, end);
#elif defined(__APPLE__)
	/* Apple clang lowers this to sys_icache_invalidate (not a call to
	   __clear_cache), so there is no self-recursion here. */
	__builtin___clear_cache(beg, end);
#else
	/* Do NOT implement this with __builtin___clear_cache on ELF AArch64: gcc and
	   clang lower that builtin to a call to __clear_cache -- i.e. to this very
	   function -- so it self-recurses to a stack overflow. That detonates in the
	   --embed-jit runtime the first time the JIT flushes the I-cache after
	   writing code. Emit the architectural clean+invalidate sequence inline,
	   reading the D/I line sizes from CTR_EL0 (same shape as libgcc/compiler-rt
	   and mcc's own __arm64_clear_cache). */
	unsigned long ctr, addr, dline, iline;
	__asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
	dline = 4UL << ((ctr >> 16) & 0xf);
	iline = 4UL << (ctr & 0xf);
	for (addr = (unsigned long)beg & ~(dline - 1);
			 addr < (unsigned long)end; addr += dline)
		__asm__ volatile("dc cvau, %0" : : "r"(addr) : "memory");
	__asm__ volatile("dsb ish" : : : "memory");
	for (addr = (unsigned long)beg & ~(iline - 1);
			 addr < (unsigned long)end; addr += iline)
		__asm__ volatile("ic ivau, %0" : : "r"(addr) : "memory");
	__asm__ volatile("dsb ish\n\tisb" : : : "memory");
#endif
}

#endif
