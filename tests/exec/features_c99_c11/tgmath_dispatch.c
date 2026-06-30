
#include <tgmath.h>

extern int printf(const char *, ...);

int main(void)
{
    int ok = 1;


    if (sqrt(4.0) != 2.0) ok = 0;
    if (sqrt(4.0f) != 2.0f) ok = 0;
    if (ceil(2.3) != 3.0) ok = 0;
    if (pow(2.0, 3.0) != 8.0) ok = 0;


    double _Complex z = 3.0 + 4.0 * I;
    if (fabs(z) != 5.0) ok = 0;
    double _Complex e = exp(0.0 * I);
    if (__real__ e != 1.0 || __imag__ e != 0.0) ok = 0;
    if (creal(z) != 3.0 || cimag(z) != 4.0) ok = 0;

    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
