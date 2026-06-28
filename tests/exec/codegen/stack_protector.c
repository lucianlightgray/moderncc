#include <stdio.h>

/* Compiled with -fstack-protector-all (see exec/goldens.h). Verifies that the
   x86_64 canary prolog/epilog produce correct, working code for a function
   with a local buffer (a stack smash would abort via __stack_chk_fail). */

static int checksum(const int *a, int n)
{
    int buf[8];
    int i, s = 0;
    for (i = 0; i < n && i < 8; i++)
        buf[i] = a[i] * 2;
    for (i = 0; i < n && i < 8; i++)
        s += buf[i];
    return s;
}

int main(void)
{
    int a[5] = { 1, 2, 3, 4, 5 };
    printf("%d\n", checksum(a, 5));
    return 0;
}
