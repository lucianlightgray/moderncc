/* C99 6.5.17: the comma operator evaluates its left operand as a void
   expression (there is a sequence point after it), discards the result, then
   evaluates the right operand, whose value and type are the result of the
   whole expression. */
#include <stdio.h>

int main(void)
{
    /* value of a comma expression is the right operand */
    int v = (1, 2, 3);
    printf("value: %d\n", v);

    /* left operand's side effects happen before the right operand is used */
    int a = 0;
    int b = (a = 5, a + 1);
    printf("seq: a=%d b=%d\n", a, b);

    /* multiple comma-separated side effects, left to right */
    int n = 0, log = 0;
    n = (n++, n++, n++, n);          /* n becomes 3 */
    (log = log * 10 + 1, log = log * 10 + 2, log = log * 10 + 3);
    printf("count: n=%d order=%d\n", n, log);

    /* comma in a for-loop's clauses (the canonical multi-variable idiom) */
    int sum = 0;
    for (int i = 0, j = 10; i < j; i++, j--)
        sum += i + j;
    printf("forsum: %d\n", sum);    /* i+j == 10 each step, 5 steps -> 50 */

    /* the result is an rvalue usable in a larger expression */
    printf("nested: %d\n", (10, 20) + (1, 2));   /* 20 + 2 */
    return 0;
}
