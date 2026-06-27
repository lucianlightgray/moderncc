/* C99 6.7.8: initialization of aggregates. A partial initializer list zero-fills
   the remaining elements/members (6.7.8p21); nested braces initialize nested
   aggregates; a char array may be initialized from a string literal (with the
   terminating '\0' when it fits); designated initializers set specific elements
   and members, and later initializers override earlier ones for the same
   subobject. */
#include <stdio.h>

struct point { int x, y, z; };

int main(void)
{
    /* partial init: the rest is zero-filled */
    int a[5] = { 1, 2 };
    printf("partial: %d %d %d %d %d\n", a[0], a[1], a[2], a[3], a[4]);

    /* an all-zero aggregate via the {0} idiom */
    struct point p = { 0 };
    printf("zero: %d %d %d\n", p.x, p.y, p.z);

    /* nested aggregate initialization */
    struct point line[2] = { { 1, 2, 3 }, { 4, 5, 6 } };
    printf("nested: %d %d\n", line[0].y, line[1].z);    /* 2 6 */

    /* char array from a string literal: includes the trailing '\0' */
    char s[6] = "hi";
    printf("str: [%s] len-region=%d c2=%d\n", s, (int)sizeof s, s[2]);  /* hi 6 0 */

    /* designated initializers, including sparse and out-of-order */
    int d[8] = { [2] = 20, [5] = 50, [0] = 0 };
    printf("desig: %d %d %d %d\n", d[0], d[2], d[5], d[7]);   /* 0 20 50 0 */

    /* member designators on a struct */
    struct point m = { .z = 9, .x = 1 };       /* y defaults to 0 */
    printf("memdesig: %d %d %d\n", m.x, m.y, m.z);   /* 1 0 9 */

    /* a later initializer overrides an earlier one for the same element */
    int o[3] = { [0] = 1, [1] = 2, [0] = 7 };
    printf("override: %d %d\n", o[0], o[1]);   /* 7 2 */
    return 0;
}
