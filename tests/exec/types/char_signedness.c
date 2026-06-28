/* Plain-char signedness is controlled by -f(un)signed-char. This file is run
   twice with opposite flags; the (char)0xFF promotion reveals the setting. */
#include <stdio.h>

int main(void)
{
    char c = (char)0xFF;
    int promoted = c;                 /* sign- or zero-extended per setting */
    printf("%d %d\n", promoted, c < 0);
    return 0;
}
