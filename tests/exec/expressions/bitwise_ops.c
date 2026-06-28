



#include <stdio.h>

int main(void)
{
    unsigned x = 0x00FFu, y = 0x0F0Fu;

    printf("and: %u\n", x & y);
    printf("or:  %u\n", x | y);
    printf("xor: %u\n", x ^ y);


    unsigned f = 0;
    f |= (1u << 3);
    f |= (1u << 5);
    printf("set: %u\n", f);
    f &= ~(1u << 3);
    printf("clear: %u\n", f);
    f ^= (1u << 5);
    printf("toggle: %u\n", f);


    unsigned a = 0xABCDu, b = 0x1234u;
    printf("xinv: %d\n", ((a ^ b) ^ b) == a);


    unsigned mask = 0xFFFFu;
    printf("demorgan: %d\n",
           (~(a & b) & mask) == ((~a | ~b) & mask));


    printf("lowbyte: %u\n", 0x1234u & 0xFFu);
    return 0;
}
