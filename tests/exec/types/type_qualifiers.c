#include <stdio.h>

int main(void) {

    const int ci = 42;
    printf("const: %d\n", ci);

    int a[3] = {1, 2, 3};
    const int *pc = a;
    pc++;
    printf("ptr_to_const: %d\n", *pc);

    int x = 10;
    int *const cp = &x;
    *cp = 20;
    printf("const_ptr: %d\n", x);

    const int *const cpc = &ci;
    printf("cpc: %d\n", *cpc);

    const int tbl[4] = {5, 10, 15, 20};
    int sum = 0;
    for (int i = 0; i < 4; i++)
        sum += tbl[i];
    printf("const_arr: %d\n", sum);

    volatile int v = 0;
    v = v + 1;
    v = v + 1;
    printf("volatile: %d\n", v);

    const int *r = &ci;
    printf("read_const: %d\n", *r);
    return 0;
}
