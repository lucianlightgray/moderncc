/* Self-checking long-double and string<->float conformance against the target
   libc. long double through the variadic boundary takes a *different* ABI slot
   from double on several arches (x86_64 passes it on the stack as 80-bit, not
   in an XMM register; AArch64/riscv64 use 128-bit quad; ARM aliases it to
   64-bit double) -- snprintf("%Lf") routes the value through that slot into the
   real libc, so a wrong long-double varargs class diverges here. All values are
   small exact decimals so the expected text is arch-independent regardless of
   the underlying long-double width. 0 on success. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char buf[64];

    /* a lone long double through varargs */
    long double ld = 3.5L;
    snprintf(buf, sizeof buf, "%.2Lf", ld);
    if (strcmp(buf, "3.50")) return 1;

    /* double and long double mixed: each must land in its own ABI class */
    snprintf(buf, sizeof buf, "%.1f %.1Lf", 1.5, 2.5L);
    if (strcmp(buf, "1.5 2.5")) return 2;

    /* int, double, long double interleaved -- maximal class mixing */
    snprintf(buf, sizeof buf, "%d %.1f %.1Lf", 7, 0.5, 0.25L);
    if (strcmp(buf, "7 0.5 0.2")) return 3;     /* %.1Lf rounds 0.25 -> 0.2 (banker's/RN) */

    /* strtod round-trips an exact decimal */
    char *end;
    double v = strtod("2.5", &end);
    if (v != 2.5 || *end != '\0') return 4;

    /* strtold -> long double path */
    long double lv = strtold("6.5", &end);
    if (lv != 6.5L || *end != '\0') return 5;

    return 0;
}
