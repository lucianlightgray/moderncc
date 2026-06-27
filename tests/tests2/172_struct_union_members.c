/* C99 6.5.2.3 (member access via . and ->), 6.5.16.1 (struct assignment copies
   the whole object), and 6.7.2.1 unions (members overlay the same storage).
   Nested structures and pointer-to-member access are exercised. */
#include <stdio.h>

struct point { int x, y; };
struct rect  { struct point lo, hi; };

union bits {
    unsigned u;
    unsigned char b[4];     /* same storage as u */
};

int main(void)
{
    struct point a = { 3, 4 };
    struct point *pa = &a;

    /* . on an object, -> on a pointer */
    printf("dot: %d %d\n", a.x, a.y);          /* 3 4 */
    pa->x = 30;
    printf("arrow: %d %d\n", pa->x, a.y);      /* 30 4 */

    /* whole-struct assignment copies by value (independent objects) */
    struct point c = a;
    c.x = 99;
    printf("copy: a=%d c=%d\n", a.x, c.x);     /* 30 99 */

    /* nested member access */
    struct rect r = { { 0, 0 }, { 10, 20 } };
    r.hi.x += 5;
    printf("nested: %d %d %d %d\n", r.lo.x, r.lo.y, r.hi.x, r.hi.y);

    /* a struct passed by value to a function is copied; the caller is unchanged */
    struct point arg = { 1, 2 };
    struct point ret = (struct point){ arg.y, arg.x };  /* swap via compound lit */
    printf("swap: %d %d\n", ret.x, ret.y);     /* 2 1 */

    /* union members alias the same bytes; write u, read b (endian-agnostic sum) */
    union bits w;
    w.u = 0;
    w.b[0] = 1; w.b[1] = 2; w.b[2] = 3; w.b[3] = 4;
    printf("union: sum=%d\n", w.b[0] + w.b[1] + w.b[2] + w.b[3]);  /* 10 */
    return 0;
}
