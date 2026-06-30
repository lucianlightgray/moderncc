
#include <stdio.h>
int main(void)
{
    printf("%d\n", '\e');
    printf("%d\n", L'A');

    printf("%zu %d\n", sizeof("\U0001F600"), (unsigned char)"\U0001F600"[0]);
    return 0;
}
