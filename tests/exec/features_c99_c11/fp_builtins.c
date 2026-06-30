/* Annex F / 7.12: the gcc/clang floating-point builtins. mcc provides them as
   constant-foldable macros (runtime/include/mccdefs.h): the inf / huge_val / nan
   constants plus the isnan/isinf/isfinite/isunordered/isgreater/isless
   classification and comparison builtins. */
extern int printf(const char *, ...);

/* constant-foldable in a static initializer */
static int s_isnan = __builtin_isnan(0.0 / 0.0);
static int s_isinf = __builtin_isinf(__builtin_inf());

int main(void)
{
    int ok = 1;
    double inf = __builtin_inf();
    float  finf = __builtin_inff();
    double nan = __builtin_nan("");

    if (!(inf > 1.79e308)) ok = 0;
    if (!((double)finf > 3.4e38)) ok = 0;
    if (!(__builtin_huge_val() > 1.79e308)) ok = 0;

    if (__builtin_isnan(nan) != 1) ok = 0;
    if (__builtin_isnan(1.0) != 0) ok = 0;
    if (__builtin_isinf(inf) != 1) ok = 0;
    if (__builtin_isinf(1.0) != 0) ok = 0;
    if (__builtin_isfinite(1.0) != 1) ok = 0;
    if (__builtin_isfinite(inf) != 0) ok = 0;

    if (__builtin_isgreater(2.0, 1.0) != 1) ok = 0;
    if (__builtin_isless(1.0, 2.0) != 1) ok = 0;
    if (__builtin_isunordered(nan, 1.0) != 1) ok = 0;
    if (__builtin_isunordered(1.0, 2.0) != 0) ok = 0;
    /* a NaN operand makes the quiet comparisons false, never trap-equivalent */
    if (__builtin_isgreater(nan, 1.0) != 0) ok = 0;

    if (s_isnan != 1 || s_isinf != 1) ok = 0;   /* static constant-folding */

    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
