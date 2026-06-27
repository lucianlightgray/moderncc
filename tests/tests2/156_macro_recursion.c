/* C99 6.10.3.4: macro rescanning and the "no recursive expansion" rule -- a
   macro name found while rescanning its own replacement is left untouched
   ("painted blue"), so expansion always terminates. */
#include <stdio.h>

/* object-like chain: each rescan expands the next, terminating at a literal */
#define A   B
#define B   C
#define C   17

/* self-referential object-like macro: the inner name is not re-expanded, so it
   resolves to the like-named variable */
int selfref = 5;
#define selfref (selfref + 100)

/* rescanning expands a macro produced by another macro's expansion */
#define DOUBLE(x)  ((x) + (x))
#define APPLY(f, v) f(v)

/* a function-like macro name not followed by '(' is left alone; with '(' (even
   after whitespace) it expands */
#define INC(x) ((x) + 1)

int main(void)
{
    printf("chain: %d\n", A);                 /* A->B->C->17 */
    printf("selfref: %d\n", selfref);         /* (selfref + 100) -> 5+100 */
    printf("rescan: %d\n", APPLY(DOUBLE, 9)); /* DOUBLE(9) -> 18 */
    printf("space_paren: %d\n", INC (41));    /* expands across the space */

    /* nested expansion of the same function-like macro on distinct invocations
       (not recursion -- separate, fully-expandable calls) */
    printf("nested: %d\n", DOUBLE(DOUBLE(3)));/* ((3+3)+(3+3)) = 12 */
    return 0;
}
