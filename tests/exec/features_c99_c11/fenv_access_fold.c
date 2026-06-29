/* C11 Annex F / 6.10.6 (10.3.4): under #pragma STDC FENV_ACCESS ON the compiler
   must not constant-fold rounding-sensitive FP arithmetic, so the runtime
   rounding mode governs (gcc/clang behaviour).  1.0/5.0 rounds differently to
   nearest vs downward, so a *folded* literal (round-to-nearest) would differ
   from the runtime divide under FE_DOWNWARD; with folding suppressed they are
   bit-identical. */
#include <stdio.h>
#include <fenv.h>

#pragma STDC FENV_ACCESS ON

int main(void)
{
    fesetround(FE_DOWNWARD);
    volatile double a = 1.0, b = 5.0;
    double runtime = a / b;        /* divides at runtime, rounds downward */
    double literal = 1.0 / 5.0;    /* must NOT be folded (round-to-nearest) */
    printf("%s\n", runtime == literal ? "OK" : "FAIL");
    return 0;
}
