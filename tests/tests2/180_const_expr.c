/* C99 6.6: constant expressions. Integer constant expressions are evaluated at
   translation time and may appear where the language requires a constant: array
   dimensions, case labels, enumerator values, bit-field widths, and static
   initializers. This test confirms such expressions compute the expected values
   and are usable in each of those contexts. */
#include <stdio.h>

#define N (2 * 3)              /* macro folds into a constant */

enum { E = (1 << 4) - 1 };     /* enumerator from a constant expression: 15 */

struct flags { unsigned a : (1 + 1); unsigned b : 4; };   /* bit-field widths */

/* file-scope array sized by a constant expression; static init is constant */
static int table[N + 1] = { [N] = 100 };

int main(void)
{
    /* array dimension was a constant expression */
    printf("dim: %d\n", (int)(sizeof table / sizeof table[0]));   /* 7 */
    printf("init: %d %d\n", table[0], table[6]);                  /* 0 100 */

    /* enumerator constant */
    printf("enum: %d\n", E);                                      /* 15 */

    /* case labels must be integer constant expressions */
    int hits = 0;
    for (int i = 0; i < 20; i++) {
        switch (i) {
        case N:          hits += 1; break;   /* 6 */
        case 2 * N:      hits += 10; break;  /* 12 */
        case E:          hits += 100; break; /* 15 */
        default: break;
        }
    }
    printf("cases: %d\n", hits);             /* 111 */

    /* bit-field widths from constant expressions hold their max values */
    struct flags fl;
    fl.a = 3;          /* fits in 2 bits */
    fl.b = 15;         /* fits in 4 bits */
    printf("bits: %u %u\n", fl.a, fl.b);     /* 3 15 */

    /* a local array sized by a constant expression */
    int local[N];
    printf("local: %d\n", (int)(sizeof local / sizeof local[0]));  /* 6 */
    return 0;
}
