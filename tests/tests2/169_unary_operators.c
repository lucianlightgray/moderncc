/* C99 6.5.3: unary operators. The address-of (&) and indirection (*) operators
   are inverses; unary + / - / ~ / ! follow the integer promotions; ! yields an
   int 0 or 1; ~x == -x - 1 for signed two's-complement-free reasoning is
   avoided -- instead ~ is tested on unsigned where it is fully defined. */
#include <stdio.h>

int main(void)
{
    int x = 42;
    int *p = &x;            /* address-of */
    *p = *p + 1;            /* indirection as lvalue */
    printf("indir: %d\n", x);              /* 43 */
    printf("roundtrip: %d\n", *&x);        /* &x then * gives x back: 43 */

    /* unary minus and plus */
    int n = 7;
    printf("neg: %d %d\n", -n, +n);        /* -7 7 */

    /* logical negation yields int 0/1 and double-negation normalizes */
    printf("not: %d %d %d\n", !0, !5, !!5);   /* 1 0 1 */

    /* bitwise complement on unsigned is well-defined (modulo 2^N) */
    unsigned u = 0u;
    printf("compl: %u\n", ~u);             /* UINT_MAX */
    unsigned m = 0x0Fu;
    printf("mask: %u\n", ~m & 0xFFu);      /* 0xF0 = 240 */

    /* & on an array element and pointer arithmetic agree */
    int a[4] = { 10, 20, 30, 40 };
    int *q = &a[1];
    printf("addr: %d %d\n", *q, *(q + 1)); /* 20 30 */

    /* the result of & has pointer type; comparing addresses of distinct
       objects in one array is well-defined */
    printf("order: %d\n", &a[0] < &a[3]);  /* 1 */
    return 0;
}
