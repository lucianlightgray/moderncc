/* C99 6.5.15: the conditional operator ?:. The first operand is evaluated as a
   scalar; exactly one of the second/third operands is evaluated (the other has
   no side effects); there is a sequence point after the first operand; and the
   result type is the common type of the second and third operands (e.g. int+
   double -> double). */
#include <stdio.h>

int calls;
int note(int v) { calls++; return v; }

int main(void)
{
    /* basic selection */
    printf("sel: %d %d\n", (1 ? 10 : 20), (0 ? 10 : 20));   /* 10 20 */

    /* only the taken branch is evaluated (the other side effect never runs) */
    calls = 0;
    int a = (1 ? note(7) : note(9));
    printf("true_branch: a=%d calls=%d\n", a, calls);        /* 7 1 */
    calls = 0;
    int b = (0 ? note(7) : note(9));
    printf("false_branch: b=%d calls=%d\n", b, calls);       /* 9 1 */

    /* common type: int and double yield double for both branch results */
    int cond = 0;
    double r = cond ? 1 : 2.5;        /* result type double regardless of cond */
    printf("commontype: %d\n", (int)(r * 2));               /* 5 */

    /* nested/chained ?: associates right-to-left */
    for (int x = -1; x <= 1; x++) {
        const char *s = x < 0 ? "neg" : x == 0 ? "zero" : "pos";
        printf("sign(%d)=%s\n", x, s);
    }

    /* ?: as an lvalue-producing form is not allowed in C, but its value can be
       assigned; pick a value used in further arithmetic */
    int m = 3 > 2 ? 100 : 0;
    printf("use: %d\n", m + 1);                              /* 101 */
    return 0;
}
