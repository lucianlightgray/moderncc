/* Macro invocations must appear as CST_MacroInvocation nodes (slice J) while the
 * written source still round-trips byte-identically. */
#define SQUARE(x) ((x) * (x))
#define LIMIT 100
#define ADD(a, b) ((a) + (b))

int compute(int n) {
    int a = SQUARE(n);
    int cap = LIMIT;
    int b = ADD(a, cap);
    return a + b;
}
