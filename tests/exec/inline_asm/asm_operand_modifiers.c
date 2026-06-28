/* x86 inline-asm operand size modifiers: %b (8-bit), %w (16-bit),
   %k (32-bit), %q (64-bit) select sub-register names of one operand.
   Requires the integrated assembler; x86 only. */
#include <stdio.h>

int main(void)
{
    unsigned long q;

    /* Load 0x4142 into the 16-bit name, leave rest as set. */
    unsigned long full = 0;
    asm("movl $0x11223344, %k0" : "=r"(full));
    /* %b0 = low byte: overwrite it with 0xFF. */
    asm("movb $0xFF, %b0" : "+r"(full));
    printf("k+b: %lx\n", full & 0xffffffffUL);

    /* %w0 writes the 16-bit subregister only. */
    unsigned long w = 0x12340000;
    asm("movw $0xABCD, %w0" : "+r"(w));
    printf("w: %lx\n", w);

    /* %q0 is the full 64-bit register. */
    asm("movq $0x123456789A, %q0" : "=r"(q));
    printf("q: %lx\n", q);
    return 0;
}
