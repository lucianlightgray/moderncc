#include <complex.h>
extern int printf(const char *, ...);

double _Complex c_a = 1.0 + 2.0 * I;

double _Complex c_m = (2.0 + 1.0 * I) * (1.0 + 1.0 * I);

double _Complex c_s = (5.0 + 6.0 * I) - (1.0 + 2.0 * I);

double _Complex c_d = (1.0 + 3.0 * I) / (1.0 + 1.0 * I);

float _Complex c_f = 1.0f + 2.0f * I;

double _Complex c_r = 3.0;

double _Complex c_n = (1.0 + 1.0 * I) * (1.0 + 1.0 * I) + 1.0;

static int eq(double _Complex z, double re, double im) {
    return __real__ z == re && __imag__ z == im;
}

int main(void) {
    int ok = 1;
    if (!eq(c_a, 1, 2))
        ok = 0;
    if (!eq(c_m, 1, 3))
        ok = 0;
    if (!eq(c_s, 4, 4))
        ok = 0;
    if (!eq(c_d, 2, 1))
        ok = 0;
    if (!((double)__real__ c_f == 1 && (double)__imag__ c_f == 2))
        ok = 0;
    if (!eq(c_r, 3, 0))
        ok = 0;
    if (!eq(c_n, 1, 2))
        ok = 0;
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
