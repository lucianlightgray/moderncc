/* C99 6.5.10 (&), 6.5.11 (^), 6.5.12 (|): bitwise operators on integer operands.
   Tested on unsigned values where every bit pattern is well-defined, covering
   masking, setting/clearing/toggling bits, the XOR self-inverse property, and
   De Morgan's identity over a fixed width. */
#include <stdio.h>

int main(void)
{
    unsigned x = 0x00FFu, y = 0x0F0Fu;

    printf("and: %u\n", x & y);     /* 0x000F = 15 */
    printf("or:  %u\n", x | y);     /* 0x0FFF = 4095 */
    printf("xor: %u\n", x ^ y);     /* 0x0FF0 = 4080 */

    /* set, clear, toggle a specific bit */
    unsigned f = 0;
    f |= (1u << 3);                 /* set bit 3 */
    f |= (1u << 5);                 /* set bit 5 */
    printf("set: %u\n", f);         /* 0x28 = 40 */
    f &= ~(1u << 3);                /* clear bit 3 */
    printf("clear: %u\n", f);       /* 0x20 = 32 */
    f ^= (1u << 5);                 /* toggle bit 5 off */
    printf("toggle: %u\n", f);      /* 0 */

    /* XOR is its own inverse: (a ^ b) ^ b == a */
    unsigned a = 0xABCDu, b = 0x1234u;
    printf("xinv: %d\n", ((a ^ b) ^ b) == a);   /* 1 */

    /* De Morgan over a 16-bit mask: ~(a & b) == (~a | ~b) */
    unsigned mask = 0xFFFFu;
    printf("demorgan: %d\n",
           (~(a & b) & mask) == ((~a | ~b) & mask));   /* 1 */

    /* masking the low byte */
    printf("lowbyte: %u\n", 0x1234u & 0xFFu);   /* 0x34 = 52 */
    return 0;
}
