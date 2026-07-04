#include <stdio.h>

int main(void) {

    int v = (1, 2, 3);
    printf("value: %d\n", v);

    int a = 0;
    int b = (a = 5, a + 1);
    printf("seq: a=%d b=%d\n", a, b);

    int n = 0, log = 0;
    n = (n++, n++, n++, n);
    (log = log * 10 + 1, log = log * 10 + 2, log = log * 10 + 3);
    printf("count: n=%d order=%d\n", n, log);

    int sum = 0;
    for (int i = 0, j = 10; i < j; i++, j--)
        sum += i + j;
    printf("forsum: %d\n", sum);

    printf("nested: %d\n", (10, 20) + (1, 2));
    return 0;
}
