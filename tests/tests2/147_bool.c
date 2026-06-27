/* C99 _Bool / <stdbool.h>: the boolean type's defining property is that any
   conversion to it yields 0 or 1, regardless of the source value. */
#include <stdio.h>
#include <stdbool.h>

struct flags { bool a : 1, b : 1, c : 1; };

int main(void)
{
    /* every nonzero scalar converts to exactly 1 */
    bool b1 = 5;
    bool b2 = -3;
    bool b3 = 0;
    bool b4 = 256;          /* low byte is 0, but bool is still 1 */
    printf("convert: %d %d %d %d\n", b1, b2, b3, b4);

    /* float and pointer conversions follow the same rule */
    bool fb = 0.1;
    bool fz = 0.0;
    int dummy;
    bool pb = (&dummy != 0);
    bool pz = ((void *)0 != 0);
    printf("float/ptr: %d %d %d %d\n", fb, fz, pb, pz);

    /* true/false macros and arithmetic in an int context */
    printf("macros: %d %d sum=%d\n", true, false, true + true + true);

    /* bool used as an array index and in conditionals */
    const char *names[2] = { "no", "yes" };
    printf("index: %s %s\n", names[(bool)0], names[(bool)42]);

    /* sizeof(bool) is 1; a bool bitfield still stores only 0/1 */
    struct flags f = { .a = 7, .b = 0, .c = 1 };
    printf("size=%d bitfields: %d %d %d\n",
           (int)sizeof(bool), f.a, f.b, f.c);

    /* ++ on a bool saturates at 1 (it is just `b = b + 1` clamped by the
       conversion back to _Bool) -- C99 deprecated this but it is well defined */
    bool t = false;
    t++;
    t++;
    printf("incr: %d\n", t);
    return 0;
}
