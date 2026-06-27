/* C99 6.10.2: source file inclusion. A `#include "name"` searches in an
   implementation-defined manner that, for tcc/gcc/clang, includes the directory
   of the including file -- so a sibling header resolves. An include guard makes
   a repeated include a no-op (the macros/declarations are not redefined). */
#include <stdio.h>
#include "inc188.h"
#include "inc188.h"      /* second include is a no-op thanks to the guard */

/* the guard macro is visible here, proving the header was processed */
#ifndef INC188_H
#error "header guard macro not defined after include"
#endif

int main(void)
{
    /* a macro defined in the header */
    printf("answer: %d\n", INC188_ANSWER);          /* 42 */

    /* a function defined in the header */
    printf("triple: %d\n", inc188_triple(14));      /* 42 */

    /* combine the two to show both came from the header */
    printf("combined: %d\n", inc188_triple(INC188_ANSWER) - 84);  /* 42 */
    return 0;
}
