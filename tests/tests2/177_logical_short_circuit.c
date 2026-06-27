/* C99 6.5.13 (&&) and 6.5.14 (||): logical operators yield int 0 or 1 and
   guarantee left-to-right evaluation with short-circuiting -- the right operand
   is not evaluated when the result is already determined, and there is a
   sequence point after the left operand. */
#include <stdio.h>

int calls;
int note(int ret) { calls++; return ret; }

int main(void)
{
    /* result type is int, value is 0 or 1 */
    printf("vals: %d %d %d %d\n", 1 && 2, 0 && 2, 0 || 5, 0 || 0);  /* 1 0 1 0 */

    /* && short-circuits: false left operand skips the right */
    calls = 0;
    int r1 = (0 && note(1));
    printf("and_sc: r=%d calls=%d\n", r1, calls);   /* 0 0 */

    /* || short-circuits: true left operand skips the right */
    calls = 0;
    int r2 = (1 || note(1));
    printf("or_sc: r=%d calls=%d\n", r2, calls);     /* 1 0 */

    /* when not short-circuited, the right operand IS evaluated */
    calls = 0;
    int r3 = (1 && note(7));
    printf("and_eval: r=%d calls=%d\n", r3, calls);  /* 1 1 (note returns 7 -> !=0 -> 1) */

    /* sequence point lets the left operand guard the right (no UB) */
    int *p = NULL;
    int a[1] = { 42 };
    p = a;
    int guarded = (p != NULL && *p == 42);
    printf("guard: %d\n", guarded);                  /* 1 */

    /* chained, mixed precedence: && binds tighter than || */
    printf("prec: %d\n", 0 || 1 && 0 || 1);          /* (0)||((1&&0))||1 = 1 */
    return 0;
}
