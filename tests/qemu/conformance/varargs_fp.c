/* Self-checking floating-point *variadic* conformance, exercised against the
   target's real libc (snprintf). Unlike varargs.c (which only sums its own
   va_args, so caller and callee share mcc's ABI and a wrong ABI still
   self-agrees), this passes doubles/ints/long longs through the platform
   va_list to libc's vsnprintf -- so an FP-varargs register-class bug actually
   diverges here. FP varargs are passed completely differently per arch (x86_64
   XMM0-7 + AL count, AArch64 v0-7, i386/riscv stack/GP), which is exactly the
   kind of from-scratch-compiler ABI corner that self-consistent tests miss.

   All doubles are exactly representable with a finite short decimal, so the
   expected strings are arch-independent. Returns a distinct nonzero code per
   failing check; 0 on success. */

#include <stdio.h>
#include <string.h>

int main(void)
{
    char buf[128];

    /* one double through the variadic boundary */
    snprintf(buf, sizeof buf, "%.2f", 3.5);
    if (strcmp(buf, "3.50")) return 1;

    /* int and double interleaved: ints take the GP/integer class, doubles the
       FP class -- getting the class assignment wrong swaps or drops args */
    snprintf(buf, sizeof buf, "%d %.1f %d %.1f", 1, 2.5, 3, 4.5);
    if (strcmp(buf, "1 2.5 3 4.5")) return 2;

    /* ten doubles: overflow the 8 FP arg registers (x86_64/arm64) so the 9th
       and 10th spill to the stack -- a miscounted FP register window shows up
       only past the 8th arg */
    snprintf(buf, sizeof buf,
             "%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f",
             0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5);
    if (strcmp(buf, "0.5 1.5 2.5 3.5 4.5 5.5 6.5 7.5 8.5 9.5")) return 3;

    /* %g and %e formats (different libc paths, same FP-varargs ABI). glibc
       prints a 2-digit exponent (C §7.21.6.1: "the exponent contains at least
       two digits"); Microsoft's CRT has long emitted >=3 digits ("5.000000e-001").
       Both conform; accept either -- this check exercises the FP-varargs ABI
       (the value reaching libc in the right class/order), not the libc's
       exponent-field width. */
    snprintf(buf, sizeof buf, "%g %e", 100.0, 0.5);
    if (strcmp(buf, "100 5.000000e-01") && strcmp(buf, "100 5.000000e-001"))
        return 4;

    /* a long long and a double together: distinct widths in distinct classes */
    snprintf(buf, sizeof buf, "%lld %.1f", 0x100000000LL, 0.5);
    if (strcmp(buf, "4294967296 0.5")) return 5;

    /* float promotes to double across the variadic boundary (6.5.2.2p6) */
    float fv = 1.25f;
    snprintf(buf, sizeof buf, "%.2f", fv);
    if (strcmp(buf, "1.25")) return 6;

    /* doubles straddling an int so the FP-arg count register (x86_64 AL) must
       be exact: 3 doubles, 2 ints, interleaved */
    snprintf(buf, sizeof buf, "%.1f %d %.1f %d %.1f",
             0.5, 7, 1.5, 8, 2.5);
    if (strcmp(buf, "0.5 7 1.5 8 2.5")) return 7;

    return 0;
}
