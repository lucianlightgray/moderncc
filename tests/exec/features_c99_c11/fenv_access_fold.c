





#include <stdio.h>
#include <fenv.h>

#pragma STDC FENV_ACCESS ON

int main(void)
{
    fesetround(FE_DOWNWARD);
    volatile double a = 1.0, b = 5.0;
    double runtime = a / b;
    double literal = 1.0 / 5.0;
    printf("%s\n", runtime == literal ? "OK" : "FAIL");
    return 0;
}
