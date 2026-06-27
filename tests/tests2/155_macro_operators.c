/* C99 6.10.3.2 (#) and 6.10.3.3 (##): stringizing and token pasting, including
   whitespace normalization, quote escaping, empty operands, and the two-level
   "expand then stringize" idiom. */
#include <stdio.h>
#include <string.h>

#define STR(x)      #x
#define XSTR(x)     STR(x)
#define CAT(a, b)   a ## b
#define VALUE       42

int main(void)
{
    /* # turns the argument's tokens into a string literal; interior whitespace
       collapses to a single space and leading/trailing space is removed */
    printf("s1: [%s]\n", STR(hello));
    printf("s2: [%s]\n", STR( a   +   b ));
    printf("s3: [%s]\n", STR(1+2));

    /* # escapes embedded " and \ so the result is a valid string literal */
    printf("s4: [%s]\n", STR("quoted\n"));

    /* # does NOT expand its operand; XSTR expands one level first */
    printf("noexpand: [%s]\n", STR(VALUE));
    printf("expand:   [%s]\n", XSTR(VALUE));

    /* ## pastes tokens: identifiers, and digits into a single number */
    int foobar = 7;
    printf("paste_id: %d\n", CAT(foo, bar));
    printf("paste_num: %d\n", CAT(1, 2) + CAT(3, 4));      /* 12 + 34 */

    /* ## with an empty operand yields just the other operand */
    printf("empty_l: %d\n", CAT(, 99));
    printf("empty_r: %d\n", CAT(99, ));

    /* ## forms the token VALUE, which is then a macro name expanded on rescan */
    printf("paste_then_expand: %d\n", CAT(VAL, UE));       /* -> VALUE -> 42 */
    return 0;
}
