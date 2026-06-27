/* C99 6.7.3: type qualifiers const and volatile. A const-qualified object's
   value is fixed after initialization; the qualifier's placement distinguishes
   "pointer to const" from "const pointer"; a const object may still be read
   through normal access; and volatile forces each access to occur (used here
   only to confirm reads/writes still produce the obvious values). */
#include <stdio.h>

int main(void)
{
    /* a const object holds its initializer */
    const int ci = 42;
    printf("const: %d\n", ci);                 /* 42 */

    /* pointer to const: the pointer may move, the pointee may not be written */
    int a[3] = { 1, 2, 3 };
    const int *pc = a;
    pc++;                                       /* moving the pointer is fine */
    printf("ptr_to_const: %d\n", *pc);          /* a[1] = 2 */

    /* const pointer: the pointer is fixed, but the pointee can be written */
    int x = 10;
    int *const cp = &x;
    *cp = 20;                                   /* writing through it is fine */
    printf("const_ptr: %d\n", x);               /* 20 */

    /* const pointer to const: neither the pointer nor the pointee changes */
    const int *const cpc = &ci;
    printf("cpc: %d\n", *cpc);                  /* 42 */

    /* const-qualified array elements are read normally */
    const int tbl[4] = { 5, 10, 15, 20 };
    int sum = 0;
    for (int i = 0; i < 4; i++)
        sum += tbl[i];
    printf("const_arr: %d\n", sum);             /* 50 */

    /* volatile: every access is performed; the value behaves normally */
    volatile int v = 0;
    v = v + 1;
    v = v + 1;
    printf("volatile: %d\n", v);                /* 2 */

    /* a non-const pointer can point at const data only via const* (checked
       above); reading the const value through it gives the same result */
    const int *r = &ci;
    printf("read_const: %d\n", *r);             /* 42 */
    return 0;
}
