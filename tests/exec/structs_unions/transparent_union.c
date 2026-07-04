#include <stdio.h>

struct a {
    int x;
};
struct b {
    int y;
};

typedef union {
    struct a *pa;
    struct b *pb;
    int *pi;
} __attribute__((transparent_union)) U;

static int first_int(U u) {
    return *u.pi;
}

int main(void) {
    struct a A = {11};
    struct b B = {22};
    int i = 33;
    U var;
    var.pa = &A;

    printf("%d\n", first_int(&A));
    printf("%d\n", first_int(&B));
    printf("%d\n", first_int(&i));
    printf("%d\n", first_int(var));
    printf("%d\n", first_int((U){.pb = &B}));
    return 0;
}
