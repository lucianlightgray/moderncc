extern int printf(const char *, ...);

static int s_isnan = __builtin_isnan(0.0 / 0.0);
static int s_isinf = __builtin_isinf(__builtin_inf());

int main(void) {
    int ok = 1;
    double inf = __builtin_inf();
    float finf = __builtin_inff();
    double nan = __builtin_nan("");

    if (!(inf > 1.79e308))
        ok = 0;
    if (!((double)finf > 3.4e38))
        ok = 0;
    if (!(__builtin_huge_val() > 1.79e308))
        ok = 0;

    if (__builtin_isnan(nan) != 1)
        ok = 0;
    if (__builtin_isnan(1.0) != 0)
        ok = 0;
    if (__builtin_isinf(inf) != 1)
        ok = 0;
    if (__builtin_isinf(1.0) != 0)
        ok = 0;
    if (__builtin_isfinite(1.0) != 1)
        ok = 0;
    if (__builtin_isfinite(inf) != 0)
        ok = 0;

    if (__builtin_isgreater(2.0, 1.0) != 1)
        ok = 0;
    if (__builtin_isless(1.0, 2.0) != 1)
        ok = 0;
    if (__builtin_isunordered(nan, 1.0) != 1)
        ok = 0;
    if (__builtin_isunordered(1.0, 2.0) != 0)
        ok = 0;

    if (__builtin_isgreater(nan, 1.0) != 0)
        ok = 0;

    if (s_isnan != 1 || s_isinf != 1)
        ok = 0;

    if (!(__builtin_fabs(-3.5) == 3.5 && __builtin_fabs(3.5) == 3.5))
        ok = 0;
    if ((double)__builtin_fabsf(-2.0f) != 2.0)
        ok = 0;
    if ((double)__builtin_fabsl(-4.5L) != 4.5)
        ok = 0;
    if (__builtin_signbit(-1.0) != 1 || __builtin_signbit(1.0) != 0)
        ok = 0;
    if (__builtin_signbit(-0.0) != 1)
        ok = 0;
    if (!(__builtin_copysign(3.0, -1.0) == -3.0 && __builtin_copysign(3.0, 1.0) == 3.0))
        ok = 0;

    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
