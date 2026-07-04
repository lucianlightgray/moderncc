#include <stdio.h>
#include <string.h>

typedef struct {
    int arr[4];
    char tag;
} SA;
typedef struct {
    int a;
    long double ld;
    int b;
} SLD;
typedef struct {
    int a, b, c, d, e;
} S5;
struct Anon {
    int top;
    union {
        int u;
        float f;
    };
    struct {
        short lo, hi;
    };
};

static SA mutSA(SA s) {
    for (int i = 0; i < 4; i++)
        s.arr[i] *= 2;
    s.tag++;
    return s;
}
static int sld_chk(SLD s) {
    return s.a + (int)s.ld + s.b;
}
static S5 chain(S5 s) {
    S5 t;
    t = s;
    t.a += t.e;
    return t;
}

int main(void) {

    SA a = {.arr = {1, 2, 3, 4}, .tag = 'X'};
    a = mutSA(mutSA(a));
    printf("SA %d %d %d %d %d\n", a.arr[0], a.arr[1], a.arr[2], a.arr[3], a.tag);

    SLD s = {.a = 7, .ld = 2.5L, .b = 9};
    printf("SLD %d\n", sld_chk(s));

    struct Anon an;
    memset(&an, 0, sizeof an);
    an.top = 1;
    an.u = 42;
    an.lo = 5;
    an.hi = 6;
    printf("Anon %d %d %d %d\n", an.top, an.u, an.lo, an.hi);

    int idx[8] = {[2] = 20, [5] = 50, [2] = 22};
    printf("idx %d %d %d %d\n", idx[0], idx[2], idx[5], idx[7]);

    S5 q = {1, 2, 3, 4, 5};
    q = chain(chain(q));
    printf("S5 %d %d %d %d %d\n", q.a, q.b, q.c, q.d, q.e);

    SA b = a, c;
    c = b;
    printf("copy %d %d\n", c.arr[3], c.tag);
    return 0;
}
