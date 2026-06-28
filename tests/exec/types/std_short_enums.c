#include <stdio.h>

/* Compiled with: -std=c11 -fshort-enums (see exec/goldens.h flags field).
   Exercises the promoted CLI flags: -std= must set __STDC_VERSION__, and
   -fshort-enums must shrink an enum to its minimal underlying type. */

enum Small { A, B, C };

int main(void)
{
    printf("%ld\n", (long)__STDC_VERSION__);
    printf("%zu\n", sizeof(enum Small));
    return 0;
}
