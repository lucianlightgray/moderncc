/* 6.6/6.7.9: complex arithmetic in a static/file-scope initializer is a constant
   expression (gcc/clang accept it). mcc folds +,-,*,/ on complex/real constants
   into a rodata complex constant, so `a + b*I` and chained complex arithmetic
   work at file scope, not just at runtime. float and double elements covered. */

#include <complex.h>
extern int printf(const char *, ...);

/* the canonical a + b*I form (float `_Complex_I` widened to double) */
double _Complex c_a = 1.0 + 2.0 * I;            /* 1 + 2i */
/* complex * complex */
double _Complex c_m = (2.0 + 1.0 * I) * (1.0 + 1.0 * I);   /* 1 + 3i */
/* complex - complex */
double _Complex c_s = (5.0 + 6.0 * I) - (1.0 + 2.0 * I);   /* 4 + 4i */
/* complex / complex */
double _Complex c_d = (1.0 + 3.0 * I) / (1.0 + 1.0 * I);   /* 2 + 1i */
/* all-float element */
float _Complex  c_f = 1.0f + 2.0f * I;          /* 1 + 2i (float) */
/* real -> complex, and a folded scalar that stays real */
double _Complex c_r = 3.0;                       /* 3 + 0i */
/* nested: ((1+1i)*(1+1i)) + 1  = (0+2i)+1 = 1 + 2i */
double _Complex c_n = (1.0 + 1.0 * I) * (1.0 + 1.0 * I) + 1.0;

static int eq(double _Complex z, double re, double im)
{
    return __real__ z == re && __imag__ z == im;
}

int main(void)
{
    int ok = 1;
    if (!eq(c_a, 1, 2)) ok = 0;
    if (!eq(c_m, 1, 3)) ok = 0;
    if (!eq(c_s, 4, 4)) ok = 0;
    if (!eq(c_d, 2, 1)) ok = 0;
    if (!((double)__real__ c_f == 1 && (double)__imag__ c_f == 2)) ok = 0;
    if (!eq(c_r, 3, 0)) ok = 0;
    if (!eq(c_n, 1, 2)) ok = 0;
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
