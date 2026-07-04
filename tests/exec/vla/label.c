#include <stdio.h>

void f1(int argc) {
    char test[argc];
    if (0)
    label:
        printf("boom!\n");
    if (argc-- == 0)
        return;
    goto label;
}

void f2(void) {
    goto start;
    {
        int a[1 && 1];
        int b[1 || 1];
        int c[1 ? 1 : 1];
    start:
        a[0] = 0;
        b[0] = 0;
        c[0] = 0;
    }
}

void f3(void) {
    printf("%d\n", 0 ? printf("x1\n") : 11);
    printf("%d\n", 1 ? 12 : printf("x2\n"));
    printf("%d\n", 0 && printf("x3\n"));
    printf("%d\n", 1 || printf("x4\n"));
}

int main() {
    f1(2);
    f2();
    f3();

    return 0;
}
