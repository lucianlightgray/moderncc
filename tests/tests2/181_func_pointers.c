/* C99 6.3.2.1 (a function designator decays to a pointer to function) and
   6.5.2.2 (function calls). A function name used other than as the operand of &
   or sizeof becomes a pointer; a call may be written through that pointer with
   or without an explicit * or &; pointers to functions can be stored, passed,
   and selected at runtime. */
#include <stdio.h>

static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }

/* higher-order function: takes a function pointer parameter */
static int apply(int (*f)(int, int), int x, int y) { return f(x, y); }

int main(void)
{
    int (*fp)(int, int) = add;     /* function designator decays to pointer */

    /* all three call spellings are equivalent */
    printf("calls: %d %d %d\n", fp(2, 3), (*fp)(2, 3), (&add)(2, 3));  /* 5 5 5 */

    /* &func and plain func yield the same pointer value */
    printf("same: %d\n", (add == &add));   /* 1 */

    /* pass a function pointer to a higher-order function */
    printf("apply: %d %d\n", apply(add, 10, 4), apply(sub, 10, 4));    /* 14 6 */

    /* select a function at runtime from a table */
    int (*table[2])(int, int) = { add, sub };
    int total = 0;
    for (int i = 0; i < 2; i++)
        total += table[i](20, 5);          /* 25 + 15 */
    printf("table: %d\n", total);          /* 40 */

    /* reassign the pointer and call again */
    fp = sub;
    printf("rebind: %d\n", fp(9, 4));      /* 5 */
    return 0;
}
