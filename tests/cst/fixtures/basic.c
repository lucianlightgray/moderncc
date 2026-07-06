/* basic.c — exercises comments, operators, and declarations for the CST
 * round-trip. Freestanding: no includes, so it compiles with a bare -c. */

// a line comment with odd chars: /* not nested */ and a tab	here
int   global_counter = 0;          /* trailing block comment */

struct Point { int x, y; };

static int add(int a, int b) {
    return a + b;   // returns the sum
}

int compute(int n) {
    int acc = 0;
    for (int i = 0; i < n; ++i) {
        acc += add(i, i * 2) - (i % 3 ? 1 : 0);
    }
    return acc;
}

int main(void) {
    struct Point p = { .x = 1, .y = 2 };
    return compute(p.x + p.y);
}
