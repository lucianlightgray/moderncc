#include <stdio.h>

int main(void) {

    unsigned int a, b, c, d;
    asm volatile("cpuid"
                 : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                 : "a"(0));

    printf("cpuid: %d\n", (b | c | d) != 0);

    unsigned long long pair;
    asm("movl $0xEFEF, %%eax\n\t"
        "movl $0xABAB, %%edx"
        : "=A"(pair));
    printf("A: %llx\n", pair);

    long keep = 0x1234;
    asm volatile("xorq %%rbx, %%rbx\n\t"
                 "movq $0, %%r12"
                 : : : "rbx", "r12");
    printf("keep: %lx\n", keep);

    register long r asm("r15") = 0x5678;
    asm volatile("" : "+r"(r));
    printf("hello\n");
    printf("reg: %lx\n", r);
    return 0;
}
