#include <stdio.h>

#define N (2 * 3)

enum { E = (1 << 4) - 1 };

struct flags {
    unsigned a : (1 + 1);
    unsigned b : 4;
};

static int table[N + 1] = {[N] = 100};

int main(void) {

    printf("dim: %d\n", (int)(sizeof table / sizeof table[0]));
    printf("init: %d %d\n", table[0], table[6]);

    printf("enum: %d\n", E);

    int hits = 0;
    for (int i = 0; i < 20; i++) {
        switch (i) {
        case N:
            hits += 1;
            break;
        case 2 * N:
            hits += 10;
            break;
        case E:
            hits += 100;
            break;
        default:
            break;
        }
    }
    printf("cases: %d\n", hits);

    struct flags fl;
    fl.a = 3;
    fl.b = 15;
    printf("bits: %u %u\n", fl.a, fl.b);

    int local[N];
    printf("local: %d\n", (int)(sizeof local / sizeof local[0]));
    return 0;
}
