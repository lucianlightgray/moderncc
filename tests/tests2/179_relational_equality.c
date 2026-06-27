/* C99 6.5.8 (relational < <= > >=) and 6.5.9 (equality == !=). Each operator
   yields an int that is 0 or 1. Covers integer and floating comparisons, the
   usual arithmetic conversions in mixed comparisons, pointer comparisons within
   one array object, and equality against the null pointer. */
#include <stdio.h>

int main(void)
{
    /* relational results are int 0/1 */
    printf("rel: %d %d %d %d\n", 3 < 5, 5 <= 5, 7 > 9, 9 >= 9);   /* 1 1 0 1 */
    printf("eq: %d %d\n", 4 == 4, 4 != 4);                        /* 1 0 */

    /* floating-point comparisons */
    printf("flt: %d %d\n", 1.5 < 2.0, 2.0 <= 1.5);               /* 1 0 */

    /* mixed int/unsigned comparison after the usual arithmetic conversions:
       a negative int converts to a large unsigned, so -1 > 0u */
    printf("mixed: %d\n", (-1 > 0u));                            /* 1 */

    /* relational operators are left-associative and lower precedence than
       arithmetic: 1 + 1 < 3 parses as (1 + 1) < 3 */
    printf("prec: %d\n", 1 + 1 < 3);                            /* 1 */

    /* pointer comparisons within a single array are well-defined */
    int a[4];
    int *p = &a[1], *q = &a[3];
    printf("ptr: %d %d %d\n", p < q, p == p, p != q);           /* 1 1 1 */

    /* equality with a null pointer */
    int *np = NULL;
    printf("null: %d %d\n", np == NULL, p != NULL);             /* 1 1 */

    /* equality of values reached different ways */
    printf("paths: %d\n", (2 + 3) == (10 - 5));                 /* 1 */
    return 0;
}
