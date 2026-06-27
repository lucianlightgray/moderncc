/* C99 6.5.2.4 (postfix ++/--) and 6.5.3.1 (prefix ++/--). Postfix yields the
   old value and then increments/decrements; prefix updates first and yields the
   new value. Each full expression below has at most one modification of a given
   object between sequence points (no unspecified/undefined ordering). */
#include <stdio.h>

int main(void)
{
    int i = 5;
    printf("post: %d\n", i++);   /* prints 5 */
    printf("after: %d\n", i);    /* 6 */
    printf("pre: %d\n", ++i);    /* 7 */
    printf("after: %d\n", i);    /* 7 */

    int j = 5;
    printf("postdec: %d\n", j--); /* 5 */
    printf("predec: %d\n", --j);  /* 3 */

    /* ++/-- through a pointer walks an array */
    int a[5] = { 0, 1, 2, 3, 4 };
    int *p = a;
    int s = 0;
    while (p < a + 5)
        s += *p++;                /* read *p, then advance p */
    printf("walk: %d\n", s);      /* 10 */

    /* pre-increment a pointer then dereference */
    int *q = a;
    printf("preptr: %d\n", *++q); /* a[1] = 1 */

    /* ++ on a value used to index, sequenced via separate statements */
    int k = 0;
    int picked = a[k++];          /* a[0], k becomes 1 */
    printf("index: picked=%d k=%d\n", picked, k);   /* 0 1 */
    return 0;
}
