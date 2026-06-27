/* C99 6.5.3.4: the sizeof operator. Its result is a size_t. Except when the
   operand is a variable length array, the operand is NOT evaluated -- so side
   effects in a sizeof operand never happen. Only portable size relationships
   are asserted (exact widths are implementation-defined). */
#include <stdio.h>

int counter = 0;
int bump(void) { counter++; return 0; }

int main(void)
{
    /* sizeof(char) is exactly 1 by definition */
    printf("char1: %d\n", (int)(sizeof(char) == 1));

    /* the operand of sizeof is unevaluated: bump() never runs */
    size_t z = sizeof(bump());
    printf("noeval: counter=%d nonzero_size=%d\n", counter, (int)(z > 0));

    /* portable ordering of the standard integer widths */
    printf("order: %d\n",
           (int)(sizeof(char)  <= sizeof(short) &&
                 sizeof(short) <= sizeof(int)   &&
                 sizeof(int)   <= sizeof(long)  &&
                 sizeof(long)  <= sizeof(long long)));

    /* a string literal is an array of char including the terminating '\0' */
    printf("strlit: %d\n", (int)sizeof("hello"));   /* 6 */

    /* array size vs element size; decayed pointer differs from the array */
    int arr[10];
    printf("arr: %d elems=%d\n",
           (int)sizeof(arr), (int)(sizeof(arr) / sizeof(arr[0])));

    /* sizeof binds tighter than binary +, and applies to a parenthesized expr */
    int k = 1;
    printf("expr: %d\n", (int)(sizeof(k + 1) == sizeof(int)));

    /* sizeof of a type vs an expression of that type agree */
    double d = 0;
    printf("match: %d\n", (int)(sizeof d == sizeof(double)));
    return 0;
}
