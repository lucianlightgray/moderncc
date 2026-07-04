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
    __arm64_clear_cache(beg, end);
}

#endif
