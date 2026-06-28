/* Self-checking variadic-argument conformance test (integer and double
   promotions through the per-arch va_list ABI). */

#include <stdarg.h>

static long isum(int n, ...)
{
    va_list ap;
    long s = 0;
    int i;
    va_start(ap, n);
    for (i = 0; i < n; i++)
        s += va_arg(ap, int);
    va_end(ap);
    return s;
}

static double dsum(int n, ...)
{
    va_list ap;
    double s = 0;
    int i;
    va_start(ap, n);
    for (i = 0; i < n; i++)
        s += va_arg(ap, double);
    va_end(ap);
    return s;
}

static long long mix(int n, ...)
{
    va_list ap;
    long long s = 0;
    int i;
    va_start(ap, n);
    for (i = 0; i < n; i++)
        s += va_arg(ap, long long);
    va_end(ap);
    return s;
}

int main(void)
{
    if (isum(5, 1, 2, 3, 4, 5) != 15) return 1;
    if (dsum(3, 1.5, 2.5, 3.0) != 7.0) return 2;
    if (mix(2, 0x100000000LL, 0x200000000LL) != 0x300000000LL) return 3;
    return 0;
}
