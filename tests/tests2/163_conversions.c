/* C99 6.3: conversions. Covers integer promotions, signed/unsigned conversion
   by modular reduction (6.3.1.3), truncation to a narrower unsigned type, the
   usual arithmetic conversions (6.3.1.8), and float->int truncation toward
   zero (6.3.1.4). Only well-defined cases are used: conversions to unsigned
   types are defined for all values, and the float->int casts stay in range. */
#include <stdio.h>

int main(void)
{
    /* signed -> unsigned is reduction modulo 2^N (well-defined) */
    int neg = -1;
    unsigned u = (unsigned)neg;
    printf("s2u: %u\n", u);                 /* UINT_MAX */

    /* narrowing to unsigned char keeps the low 8 bits */
    int big = 0x1234;
    unsigned char uc = (unsigned char)big;
    printf("narrow: %u\n", (unsigned)uc);   /* 0x34 = 52 */

    /* usual arithmetic conversions: int + unsigned -> unsigned */
    printf("uac: %d\n", (int)((-1 + 1u) == 0u));

    /* a small unsigned operand promotes to int, so the result can be negative */
    unsigned char a = 1, b = 2;
    printf("promote: %d\n", a - b);         /* -1, not a huge unsigned */

    /* float -> integer truncates toward zero (drops the fraction) */
    printf("ftrunc: %d %d\n", (int)3.9, (int)-3.9);   /* 3 -3 */

    /* int -> double is exact for small magnitudes; round-trips back */
    int n = 1000003;
    printf("rt: %d\n", (int)((double)n) == n);

    /* widening preserves value and sign for signed types */
    short sh = -12345;
    long  lo = sh;
    printf("widen: %ld\n", lo);

    /* converting a nonzero value to _Bool-like via != yields 0/1 */
    printf("bool: %d %d\n", !!42, !!0);
    return 0;
}
