/* Self-checking C11 _Complex conformance (Annex G.5.1/G.5.2 multiply/divide).
   Freestanding: no printf / no <complex.h> / no <math.h> — only exact compares
   and arithmetic-built inf/nan tests, so it runs on every cross target under
   qemu.  Exercises the runtime __mcc_c{mul,div}{,f,l} helpers (libmcc1) that
   mcc lowers complex * and / to; returns a distinct nonzero code per failure. */

static int isnan_(double x) { return x != x; }
static int isinf_(double x) { return !isnan_(x) && ((x - x) != (x - x)); }

int main(void)
{
    volatile double z = 0.0;
    double inf = 1.0 / z;                 /* +inf at runtime (no folding) */

    /* finite multiply: (1+2i)*(3+4i) = -5+10i (exact in binary fp) */
    double _Complex a = {1, 2}, b = {3, 4}, r;
    r = a * b;
    if (__real__ r != -5 || __imag__ r != 10) return 1;

    /* finite divide: (11-2i)/(3+4i) = 1-2i (exact) */
    double _Complex c = {11, -2};
    r = c / b;
    if (__real__ r != 1 || __imag__ r != -2) return 2;

    /* Annex G divide: 1/(inf+inf*i) -> 0 (the naive form yields NaN) */
    double _Complex one = {1, 0}, infc = {inf, inf};
    r = one / infc;
    if (__real__ r != 0 || __imag__ r != 0) return 3;

    /* Annex G multiply: (1+2i)*(inf+0i) keeps an infinite real part */
    double _Complex inf0 = {inf, 0};
    r = a * inf0;
    if (!isinf_(__real__ r)) return 4;

    /* divide by zero -> infinite real part (not all-NaN) */
    double _Complex zero = {0, 0};
    r = one / zero;
    if (!isinf_(__real__ r)) return 5;

    /* float precision goes through __mcc_cmulf */
    float _Complex fa = {1, 2}, fb = {3, 4}, fr;
    fr = fa * fb;
    if ((float)__real__ fr != -5 || (float)__imag__ fr != 10) return 6;

    /* long double precision goes through __mcc_cmull (80-bit on x86, etc.) */
    long double _Complex la = {1, 2}, lb = {3, 4}, lr;
    lr = la * lb;
    if ((long double)__real__ lr != -5 || (long double)__imag__ lr != 10) return 7;

    return 0;
}
