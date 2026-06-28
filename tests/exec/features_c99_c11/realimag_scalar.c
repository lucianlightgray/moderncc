/* GNU __real__/__imag__ applied to a real (non-_Complex) scalar:
   __real x == x, __imag x == 0 of x's type. */
#include <stdio.h>

int main(void)
{
    double r = 3.5;
    int i = 42;
    printf("%g %g\n", __real__ r, __imag__ r);
    printf("%d %d\n", __real__ i, __imag__ i);
    return 0;
}
