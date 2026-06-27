/* C99 6.4.5: string literals -- adjacent-literal concatenation, escape
   sequences inside strings, embedded NUL, and the array nature of a literal. */
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* adjacent string literals are concatenated at translation phase 6 */
    const char *s = "abc" "def" "ghi";
    printf("concat: %s len=%d\n", s, (int)strlen(s));

    /* concatenation across line breaks */
    const char *multi =
        "line one is here "
        "and continues here";
    printf("multi: %s\n", multi);

    /* escape sequences inside a string literal */
    printf("escapes: [%s]\n", "tab\tend\\backslash\"quote");

    /* octal and hex escapes inside strings resolve to bytes */
    printf("bytes: %d %d\n", "\101\102"[0], "\x43\x44"[1]);  /* 'A', 'D' */

    /* an embedded NUL: strlen stops at it, but sizeof counts the whole array */
    char buf[] = "ab\0cd";
    printf("embedded: strlen=%d sizeof=%d byte4=%d\n",
           (int)strlen(buf), (int)sizeof(buf), buf[3]);      /* 2, 6, 'c' */

    /* a string literal is an array: indexable, and sizeof includes the NUL */
    printf("array: %c %c sizeof=%d\n",
           "hello"[0], "hello"[4], (int)sizeof("hello"));    /* h o 6 */
    return 0;
}
