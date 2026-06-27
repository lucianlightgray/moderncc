/* C99 6.7.5: declarators. The relative binding of *, [], and () determines
   whether a declarator names an array of pointers, a pointer to an array, a
   pointer to a function, etc. This test builds each shape and uses it, relying
   on the standard rule that [] and () bind tighter than a leading *. */
#include <stdio.h>

static int g(int x) { return x + 1; }

int main(void)
{
    /* array of 3 pointers to int (suffix [] binds before the *) */
    int x = 1, y = 2, z = 3;
    int *ap[3] = { &x, &y, &z };
    printf("arr_of_ptr: %d %d %d\n", *ap[0], *ap[1], *ap[2]);   /* 1 2 3 */

    /* pointer to an array of 3 ints (parentheses force the * to bind first) */
    int grid[2][3] = { { 10, 20, 30 }, { 40, 50, 60 } };
    int (*rp)[3] = grid;            /* points at the first row */
    printf("ptr_to_arr: %d %d\n", rp[1][2], (*rp)[0]);          /* 60 10 */

    /* pointer to function returning int */
    int (*fp)(int) = g;
    printf("ptr_to_func: %d\n", fp(41));                        /* 42 */

    /* array of 2 pointers to function */
    int (*fa[2])(int) = { g, g };
    printf("arr_of_func: %d\n", fa[0](10) + fa[1](20));         /* 11 + 21 = 32 */

    /* pointer to pointer to int */
    int v = 7;
    int *p = &v;
    int **pp = &p;
    **pp = 70;
    printf("ptr_ptr: %d\n", v);                                 /* 70 */

    /* a const pointer to a pointer-to-array still indexes through the chain */
    int (*const crp)[3] = grid;
    printf("const_chain: %d\n", crp[0][1]);                     /* 20 */
    return 0;
}
