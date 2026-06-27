/* C99 6.5.4: the cast operator performs an explicit conversion to the named
   type. Covers arithmetic narrowing/widening, float<->int truncation, the
   `(void)` cast that discards a value, and an integer<->pointer<->integer
   round-trip through a sufficiently wide integer type (uintptr-like via
   unsigned long, kept in range). Only well-defined casts are used. */
#include <stdio.h>

int main(void)
{
    /* float -> int truncates toward zero; int -> double widens exactly */
    printf("trunc: %d %d\n", (int)3.99, (int)-3.99);     /* 3 -3 */
    printf("widen: %d\n", (int)((double)7 == 7.0));      /* 1 */

    /* narrowing to unsigned char keeps the low 8 bits */
    int big = 0x4321;
    printf("narrow: %u\n", (unsigned)(unsigned char)big);   /* 0x21 = 33 */

    /* the (void) cast explicitly discards a value (common to silence "unused") */
    int q = 5;
    (void)q;
    (void)(q + 1);
    printf("voidcast: %d\n", q);                          /* 5 */

    /* casting changes how the same arithmetic is performed: integer vs double
       division */
    printf("intdiv: %d\n", 7 / 2);                        /* 3 */
    printf("fltdiv: %d\n", (int)(((double)7 / 2) * 10));  /* 35 */

    /* a cast in the middle of an expression applies before the surrounding op */
    double avg = (double)(3 + 4) / 2;                    /* 3.5 */
    printf("avg: %d\n", (int)(avg * 2));                 /* 7 */

    /* pointer round-trip through an integer wide enough to hold it */
    int obj = 1234;
    int *p = &obj;
    unsigned long bits = (unsigned long)(void *)p;
    int *p2 = (int *)(void *)bits;
    printf("ptrrt: %d\n", *p2);                          /* 1234 */
    return 0;
}
