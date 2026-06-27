/* C99 6.4.4.4: character constants and escape sequences. Values are ASCII on
   all three toolchains for this host. Stay in the 0..127 range to avoid the
   implementation-defined signedness of plain char. */
#include <stdio.h>

int main(void)
{
    /* simple escape sequences (6.4.4.4 table) */
    printf("simple: %d %d %d %d %d %d %d\n",
           '\a', '\b', '\f', '\n', '\r', '\t', '\v');
    printf("punct: %d %d %d %d\n", '\\', '\'', '\"', '\?');
    printf("nul: %d\n", '\0');

    /* octal escapes: \ooo, up to three octal digits */
    printf("octal: %d %d %d\n", '\101', '\0', '\177');   /* 'A', 0, 0x7f */

    /* hex escapes: \xhh... (consumes all following hex digits) */
    printf("hex: %d %d\n", '\x41', '\x7e');               /* 'A', '~' */

    /* escapes embedded mid-token still resolve to their numeric value */
    char nl = '\n';
    printf("isnl: %d\n", nl == 10);

    /* an escape used as an integer in arithmetic */
    printf("arith: %d\n", '\x10' + '\020');               /* 16 + 16 */

    /* character constant has type int in C */
    printf("type: %d\n", (int)sizeof('A'));
    return 0;
}
