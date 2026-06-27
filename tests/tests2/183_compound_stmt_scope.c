/* C99 6.8.2 compound statements and 6.2.1 block scope. A name declared in an
   inner block shadows an outer declaration for the extent of that block and the
   outer object is unaffected; each iteration of a loop body that declares an
   object gets a fresh object; and a block-scope declaration's lifetime ends at
   the closing brace. */
#include <stdio.h>

int main(void)
{
    int x = 1;
    printf("outer: %d\n", x);          /* 1 */
    {
        int x = 2;                     /* shadows the outer x */
        printf("inner: %d\n", x);      /* 2 */
        {
            int x = 3;                 /* shadows again */
            printf("inner2: %d\n", x); /* 3 */
        }
        printf("back2: %d\n", x);      /* 2 */
    }
    printf("back1: %d\n", x);          /* outer x untouched: 1 */

    /* a for-loop control variable has block scope limited to the loop */
    int sum = 0;
    for (int i = 0; i < 4; i++)
        sum += i;
    int i = 99;                        /* a different i; the loop's i is gone */
    printf("loopscope: sum=%d i=%d\n", sum, i);   /* 6 99 */

    /* each block entry reinitializes a non-static local */
    int totals = 0;
    for (int k = 0; k < 3; k++) {
        int fresh = 10;                /* re-created each iteration */
        fresh += k;
        totals += fresh;               /* 10 + 11 + 12 */
    }
    printf("fresh: %d\n", totals);     /* 33 */

    /* inner block can introduce a name of a different type than an outer one */
    long x_long;                       /* distinct name, no conflict with int x */
    x_long = (long)x + 41;
    printf("mixed: %ld\n", x_long);    /* 42 */
    return 0;
}
