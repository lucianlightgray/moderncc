/* Lexer edge cases: GNU \e escape, wide char L'x', 8-digit \U astral UCN. */
#include <stdio.h>
int main(void)
{
    printf("%d\n", '\e');                 /* GNU escape for ESC (27) */
    printf("%d\n", L'A');                 /* wide character constant  */
    /* U+1F600 encodes to 4 UTF-8 bytes (F0 9F 98 80) + NUL = 5 */
    printf("%zu %d\n", sizeof("\U0001F600"), (unsigned char)"\U0001F600"[0]);
    return 0;
}
