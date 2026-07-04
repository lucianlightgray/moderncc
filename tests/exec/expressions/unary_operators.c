#include <stdio.h>

int main(void) {
    int x = 42;
    int *p = &x;
    *p = *p + 1;
    printf("indir: %d\n", x);
    printf("roundtrip: %d\n", *&x);

    int n = 7;
    printf("neg: %d %d\n", -n, +n);

    printf("not: %d %d %d\n", !0, !5, !!5);

    unsigned u = 0u;
    printf("compl: %u\n", ~u);
    unsigned m = 0x0Fu;
    printf("mask: %u\n", ~m & 0xFFu);

    int a[4] = {10, 20, 30, 40};
    int *q = &a[1];
    printf("addr: %d %d\n", *q, *(q + 1));

    printf("order: %d\n", &a[0] < &a[3]);
    return 0;
}
