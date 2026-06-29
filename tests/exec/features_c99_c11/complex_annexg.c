/* C11 Annex G.5.1/G.5.2: complex * and / use the inf/nan-correct algorithm
   (gcc/clang default), not the naive (ac-bd)+(ad+bc)i / naive divide that is
   only valid under CX_LIMITED_RANGE.  The naive forms produce NaN where the
   robust ones recover a finite/infinite value; this test pins the cases where
   they differ, plus finite sanity.  See runtime/lib/complex.c.

   Build NOTE: the codegen routes * and / to __mcc_c{mul,div}{,f,l}; the test
   harness links runtime/lib/complex.o (part of libmcc1). */

extern int printf(const char *, ...);

static int isnan_(double x) { return x != x; }
static int isinf_(double x) { return !isnan_(x) && ((x - x) != (x - x)); }

int main(void)
{
    int ok = 1;
    double inf = 1.0;
    inf = inf / (inf - 1.0);                 /* +inf without a literal 0-divide */

    /* finite sanity: (1+2i)*(3+4i) = -5+10i */
    double _Complex a = {1, 2}, b = {3, 4}, z;
    z = a * b;
    if (!(__real__ z == -5 && __imag__ z == 10)) ok = 0;

    /* (11-2i)/(3+4i) = 1-2i */
    double _Complex c = {11, -2};
    z = c / b;
    if (!(__real__ z == 1 && __imag__ z == -2)) ok = 0;

    /* Annex G divide: 1/(inf+inf*i) -> 0 (naive would be NaN). */
    double _Complex one = {1, 0}, infc = {inf, inf};
    z = one / infc;
    if (!(__real__ z == 0 && __imag__ z == 0)) ok = 0;

    /* Annex G multiply: (1+2i)*(inf+0i) keeps an infinite real part. */
    double _Complex inf0 = {inf, 0};
    z = a * inf0;
    if (!isinf_(__real__ z)) ok = 0;

    /* divide by zero -> infinite real part (not all-NaN). */
    double _Complex zero = {0, 0};
    z = one / zero;
    if (!isinf_(__real__ z)) ok = 0;

    /* float and long double precisions go through the same routing. */
    float _Complex fa = {1, 2}, fb = {3, 4}, fz;
    fz = fa * fb;
    if (!((float)__real__ fz == -5 && (float)__imag__ fz == 10)) ok = 0;

    long double _Complex la = {1, 2}, lb = {3, 4}, lz;
    lz = la * lb;
    if (!((long double)__real__ lz == -5 && (long double)__imag__ lz == 10)) ok = 0;

    printf(ok ? "OK\n" : "FAIL\n");
    return 0;
}
