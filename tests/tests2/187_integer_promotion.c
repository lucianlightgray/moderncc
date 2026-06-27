/* C99 6.3.1.1p2: the integer promotions. In an arithmetic context an operand of
   a type with rank lower than int (char, short, bit-fields, _Bool) is converted
   to int (or unsigned int if int cannot hold all its values). This is why the
   result of arithmetic on chars/shorts has type int, why ~ and << on a small
   unsigned operate at int width, and why a difference of two unsigned chars can
   be negative. */
#include <stdio.h>

int main(void)
{
    /* unsigned char operands promote to int, so the difference can be negative */
    unsigned char a = 1, b = 5;
    printf("diff: %d\n", a - b);            /* -4, not a huge unsigned */

    /* char arithmetic happens at int width */
    signed char c = 100, d = 100;
    printf("add: %d\n", c + d);             /* 200 (fits in int, not in char) */

    /* ~ on a small unsigned promotes to int first: ~(unsigned char)0 is -1,
       not 255, because the operand becomes int 0 then ~ gives -1 */
    unsigned char z = 0;
    printf("compl: %d\n", ~z);              /* -1 */

    /* << promotes the left operand to int; (unsigned char)1 << 9 = 512 fits */
    unsigned char one = 1;
    printf("shift: %d\n", one << 9);        /* 512 (would be 0 at 8-bit width) */

    /* a short multiplied stays correct because both promote to int */
    short s = 300;
    printf("mul: %d\n", s * s);             /* 90000 (overflows 16 bits, fits int) */

    /* the promoted result type is int: sizeof confirms it */
    printf("rank: %d\n", (int)(sizeof(a + b) == sizeof(int)));   /* 1 */

    /* a plain char compared after promotion */
    char ch = 'A';
    printf("char: %d\n", ch + 1);           /* 66 */
    return 0;
}
