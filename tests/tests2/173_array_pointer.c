/* C99 6.5.2.1 array subscripting and 6.5.6 additive operators on pointers.
   a[i] is defined as *(a + i), so subscripting commutes (a[i] == i[a]); pointer
   + integer, pointer difference (ptrdiff_t), and multidimensional row pointers
   are all exercised. Only in-bounds pointers (and the one-past-the-end pointer)
   are formed. */
#include <stdio.h>
#include <stddef.h>

int main(void)
{
    int a[6] = { 0, 10, 20, 30, 40, 50 };

    /* subscripting is commutative because a[i] == *(a + i) == *(i + a) == i[a] */
    printf("commute: %d %d %d\n", a[2], *(a + 2), 2[a]);   /* 20 20 20 */

    /* pointer + integer and the difference of two pointers */
    int *p = a + 1;
    int *q = a + 5;
    printf("diff: %ld\n", (long)(q - p));      /* 4 */
    printf("elem: %d\n", p[2]);                /* a[3] = 30 */

    /* walking with pointer arithmetic; one-past-the-end is a valid pointer */
    int sum = 0;
    for (int *it = a; it != a + 6; ++it)
        sum += *it;
    printf("sum: %d\n", sum);                  /* 150 */

    /* 2D array: rows are arrays; a row decays to a pointer to its first element */
    int m[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
    printf("twod: %d %d\n", m[1][2], *(*(m + 1) + 2));   /* 6 6 */

    /* size of a row vs an element */
    printf("rowsz: %d\n", (int)(sizeof m[0] / sizeof m[0][0]));   /* 3 */

    /* indexing relative to an interior pointer, including negative offset */
    int *mid = &a[3];
    printf("around: %d %d %d\n", mid[-1], mid[0], mid[1]);   /* 20 30 40 */
    return 0;
}
