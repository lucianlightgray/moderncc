


#include <stdio.h>

int main(void)
{
    unsigned long q;


    unsigned long full = 0;
    asm("movl $0x11223344, %k0" : "=r"(full));

    asm("movb $0xFF, %b0" : "+r"(full));
    printf("k+b: %lx\n", full & 0xffffffffUL);


    unsigned long w = 0x12340000;
    asm("movw $0xABCD, %w0" : "+r"(w));
    printf("w: %lx\n", w);


    asm("movq $0x123456789A, %q0" : "=r"(q));
    printf("q: %lx\n", q);
    return 0;
}
