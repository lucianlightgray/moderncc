/* x86 fixed-register and pair constraints + clobbers + explicit register
   variables. Requires the integrated assembler; x86_64 only. */
#include <stdio.h>

int main(void)
{
    /* "=a"/"=b"/"=c"/"=d" fixed-register outputs, "a" fixed-register input. */
    unsigned int a, b, c, d;
    asm volatile("cpuid"
                 : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                 : "a"(0));
    /* leaf 0: EBX/EDX/ECX spell the 12-byte vendor string; just sanity-check
       that the registers came back non-trivially populated. */
    printf("cpuid: %d\n", (b | c | d) != 0);

    /* "A" constraint: on x86_64 this is a single 64-bit register (rax),
       matching gcc/clang -- not the i386 edx:eax pair.  eax was loaded
       with 0xEFEF (movl zero-extends into rax), so the result is 0xefef. */
    unsigned long long pair;
    asm("movl $0xEFEF, %%eax\n\t"
        "movl $0xABAB, %%edx"
        : "=A"(pair));
    printf("A: %llx\n", pair);

    /* Specific-register clobbers must force live values to be preserved. */
    long keep = 0x1234;
    asm volatile("xorq %%rbx, %%rbx\n\t"
                 "movq $0, %%r12"
                 : : : "rbx", "r12");
    printf("keep: %lx\n", keep);

    /* Explicit register variable bound to r15, survives across a call. */
    register long r asm("r15") = 0x5678;
    asm volatile("" : "+r"(r));
    printf("hello\n");
    printf("reg: %lx\n", r);
    return 0;
}
