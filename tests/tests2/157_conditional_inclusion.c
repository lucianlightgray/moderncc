/* C99 6.10.1: conditional inclusion -- #if/#elif/#else, the defined operator,
   #ifdef/#ifndef, arithmetic and comparison in controlling expressions, macro
   expansion within them, and the rule that an unknown identifier counts as 0. */
#include <stdio.h>

#define LEVEL 3
#define FEATURE_A 1
/* FEATURE_B intentionally undefined */

int main(void)
{
    /* arithmetic and comparison are evaluated by the preprocessor */
#if 2 + 3 * 4 == 14
    printf("arith: yes\n");
#else
    printf("arith: no\n");
#endif

    /* macros expand inside the controlling expression */
#if LEVEL > 2 && LEVEL <= 5
    printf("level: %d in range\n", LEVEL);
#endif

    /* defined operator, both spellings, with && / ! */
#if defined(FEATURE_A) && !defined(FEATURE_B)
    printf("features: A only\n");
#endif

    /* an identifier that is not a macro evaluates to 0 in #if */
#if UNDEFINED_THING
    printf("undef: nonzero\n");
#else
    printf("undef: zero\n");
#endif

    /* #elif chain picks exactly one branch */
#if LEVEL == 1
    printf("branch: one\n");
#elif LEVEL == 2
    printf("branch: two\n");
#elif LEVEL == 3
    printf("branch: three\n");
#else
    printf("branch: other\n");
#endif

    /* nested conditionals and #ifdef/#ifndef */
#ifdef FEATURE_A
# ifndef FEATURE_B
    printf("nested: A set, B unset\n");
# endif
#endif
    return 0;
}
