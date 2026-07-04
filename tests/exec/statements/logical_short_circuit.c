#include <stdio.h>

int calls;
int note(int ret) {
    calls++;
    return ret;
}

int main(void) {

    printf("vals: %d %d %d %d\n", 1 && 2, 0 && 2, 0 || 5, 0 || 0);

    calls = 0;
    int r1 = (0 && note(1));
    printf("and_sc: r=%d calls=%d\n", r1, calls);

    calls = 0;
    int r2 = (1 || note(1));
    printf("or_sc: r=%d calls=%d\n", r2, calls);

    calls = 0;
    int r3 = (1 && note(7));
    printf("and_eval: r=%d calls=%d\n", r3, calls);

    int *p = NULL;
    int a[1] = {42};
    p = a;
    int guarded = (p != NULL && *p == 42);
    printf("guard: %d\n", guarded);

    printf("prec: %d\n", 0 || 1 && 0 || 1);
    return 0;
}
