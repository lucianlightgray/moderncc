/* C99 _Complex: declaration, __real__/__imag__ (read and write), whole-value
   copy, pass/return by value, the arithmetic operators with real<->complex
   promotion, ==/!=, unary negation, and casts. Uses the GNU __real__/__imag__
   operators (no <complex.h>/libm) so it is byte-identical under gcc, clang and
   tcc. 3-way verified. */
#include <stdio.h>

static void pr(const char *t, double _Complex z)
{
    printf("%s %g %g\n", t, __real__ z, __imag__ z);
}

static double _Complex cmul(double _Complex a, double _Complex b) { return a * b; }
static double mag2(double _Complex z) { return __real__ z * __real__ z + __imag__ z * __imag__ z; }

int main(void)
{
    double _Complex a, b;
    __real__ a = 1; __imag__ a = 2;
    __real__ b = 3; __imag__ b = 4;

    pr("add", a + b);          /* 4 6 */
    pr("sub", a - b);          /* -2 -2 */
    pr("mul", a * b);          /* -5 10 */
    pr("div", a / b);          /* 0.44 0.08 */
    pr("radd", 10 + a);        /* 11 2  (real promoted) */
    pr("rmul", a * 2);         /* 2 4 */
    pr("neg", -a);             /* -1 -2 */
    pr("call", cmul(a, b));    /* -5 10 (pass/return by value) */

    printf("eq %d %d\n", a == b, a == a);   /* 0 1 */
    printf("ne %d\n", a != b);              /* 1 */
    printf("mag2 %g\n", mag2(b));           /* 25 */

    /* copy and component mutation */
    double _Complex c = a;
    __real__ c = __real__ c + 100;
    pr("copy", c);             /* 101 2 */

    /* casts */
    printf("c2r %g\n", (double)b);              /* 3 (real part) */
    double _Complex d = (double _Complex)7.0;   /* real -> complex */
    pr("r2c", d);                                /* 7 0 */
    float _Complex f = (float _Complex)a;        /* complex double -> complex float */
    printf("c2c %g %g\n", (double)__real__ f, (double)__imag__ f); /* 1 2 */

    /* float complex arithmetic */
    float _Complex g, h;
    __real__ g = 1.5f; __imag__ g = 0.5f;
    __real__ h = 2.0f; __imag__ h = -1.0f;
    float _Complex s = g + h;
    printf("fadd %g %g\n", (double)__real__ s, (double)__imag__ s); /* 3.5 -0.5 */

    /* imaginary constant suffix (local scope) */
    double _Complex k = 3.0 + 4.0i;
    pr("isuf", k);                 /* 3 4 */
    pr("imul", 2.0i * 3.0i);       /* -6 0 */
    return 0;
}
