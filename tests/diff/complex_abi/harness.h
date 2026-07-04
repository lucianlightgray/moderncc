#ifndef COMPLEX_CROSS_HARNESS_H
#define COMPLEX_CROSS_HARNESS_H

#if defined(__x86_64__)
static void sys_exit(int c) {
    __asm__ volatile("syscall" ::"a"(60), "D"(c));
}
#elif defined(__i386__)
static void sys_exit(int c) {
    __asm__ volatile("int $0x80" ::"a"(1), "b"(c));
}
#elif defined(__aarch64__)

static void sys_exit(int c) {
    register long x0 __asm__("x0") = c;
    register long x8 __asm__("x8") = 93;
    __asm__ volatile(".int 0xD4000001" ::"r"(x0), "r"(x8) : "memory");
}
#elif defined(__arm__)

static void sys_exit(int c) {
    register long r0 __asm__("r0") = c;
    __asm__ volatile(".int 0xEF900001" ::"r"(r0) : "memory");
}
#elif defined(__riscv)
static void sys_exit(int c) {
    register long a0 __asm__("a0") = c;
    register long a7 __asm__("a7") = 93;
    __asm__ volatile("ecall" ::"r"(a0), "r"(a7) : "memory");
}
#else
#error "unsupported target for the complex-cross harness"
#endif

void *memmove(void *d, const void *s, unsigned long n) {
    char *D = d;
    const char *S = s;
    if (D < S)
        while (n--)
            *D++ = *S++;
    else {
        D += n;
        S += n;
        while (n--)
            *--D = *--S;
    }
    return d;
}
void *memcpy(void *d, const void *s, unsigned long n) {
    return memmove(d, s, n);
}
void *memset(void *d, int c, unsigned long n) {
    char *D = d;
    while (n--)
        *D++ = (char)c;
    return d;
}
#ifdef __arm__
void *__aeabi_memmove8(void *d, const void *s, unsigned long n) {
    return memmove(d, s, n);
}
void *__aeabi_memmove4(void *d, const void *s, unsigned long n) {
    return memmove(d, s, n);
}
void *__aeabi_memcpy8(void *d, const void *s, unsigned long n) {
    return memmove(d, s, n);
}
void *__aeabi_memcpy4(void *d, const void *s, unsigned long n) {
    return memmove(d, s, n);
}
void *__aeabi_memcpy(void *d, const void *s, unsigned long n) {
    return memmove(d, s, n);
}
void __aeabi_memclr(void *d, unsigned long n) {
    memset(d, 0, n);
}
void __aeabi_memclr8(void *d, unsigned long n) {
    memset(d, 0, n);
}
void __aeabi_memclr4(void *d, unsigned long n) {
    memset(d, 0, n);
}
#endif

#define TEST_MAIN()      \
    void _start(void) {  \
        sys_exit(run()); \
    }

#endif
