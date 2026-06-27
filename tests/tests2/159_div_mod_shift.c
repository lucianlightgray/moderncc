/* C99 6.5.5 multiplicative (/ and %) and 6.5.7 shift operators.
   C99 fixed integer division to truncate toward zero, so a/b discards the
   fractional part and a%b takes the sign of the dividend, with the identity
   (a/b)*b + a%b == a holding for all representable operands. Shifts and
   unsigned wraparound are also exercised. No undefined behavior is asserted
   (no overflow, no shift past width, no negative shift counts). */
#include <stdio.h>

int main(void)
{
    /* truncation toward zero and the division/remainder identity */
    int pairs[][2] = { {7,2}, {-7,2}, {7,-2}, {-7,-2}, {1,3}, {-1,3} };
    for (int i = 0; i < (int)(sizeof pairs / sizeof pairs[0]); i++) {
        int a = pairs[i][0], b = pairs[i][1];
        int q = a / b, r = a % b;
        printf("%d/%d=%d %d%%%d=%d ident=%d\n",
               a, b, q, a, b, r, (q * b + r) == a);
    }

    /* unsigned arithmetic wraps modulo 2^N (well-defined) */
    unsigned u = 0u;
    u = u - 1u;                 /* UINT_MAX */
    printf("wrap: %u\n", u + 1u);

    /* shifts on unsigned values */
    unsigned x = 1u;
    printf("shl: %u\n", x << 4);         /* 16 */
    printf("shr: %u\n", 0xF0u >> 4);     /* 15 */

    /* right shift of a non-negative signed value is well-defined */
    int s = 256;
    printf("sshr: %d\n", s >> 3);        /* 32 */

    /* shift result type follows the left operand (promoted) */
    printf("wide: %llu\n", (unsigned long long)1 << 40);
    return 0;
}
