#include <stdio.h>
#include <stddef.h>

int main(void) {
    int a[6] = {0, 10, 20, 30, 40, 50};

    printf("commute: %d %d %d\n", a[2], *(a + 2), 2 [a]);

    int *p = a + 1;
    int *q = a + 5;
    printf("diff: %ld\n", (long)(q - p));
    printf("elem: %d\n", p[2]);

    int sum = 0;
    for (int *it = a; it != a + 6; ++it)
        sum += *it;
    printf("sum: %d\n", sum);

    int m[2][3] = {{1, 2, 3}, {4, 5, 6}};
    printf("twod: %d %d\n", m[1][2], *(*(m + 1) + 2));

    printf("rowsz: %d\n", (int)(sizeof m[0] / sizeof m[0][0]));

    int *mid = &a[3];
    printf("around: %d %d %d\n", mid[-1], mid[0], mid[1]);
    return 0;
}
