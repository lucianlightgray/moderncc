

#include <stdio.h>

int main(void)
{
    char c = (char)0xFF;
    int promoted = c;
    printf("%d %d\n", promoted, c < 0);
    return 0;
}
