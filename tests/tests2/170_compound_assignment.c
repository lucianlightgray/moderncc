


#include <stdio.h>

int side_calls = 0;
int idx = 0;
int *select(int *base) { side_calls++; return &base[idx]; }

int main(void)
{
    int v = 10;
    v += 5;  printf("add: %d\n", v);
    v -= 3;  printf("sub: %d\n", v);
    v *= 4;  printf("mul: %d\n", v);
    v /= 5;  printf("div: %d\n", v);
    v %= 4;  printf("mod: %d\n", v);

    unsigned b = 0x0Fu;
    b <<= 4;  printf("shl: %u\n", b);
    b >>= 2;  printf("shr: %u\n", b);
    b &= 0x30u; printf("and: %u\n", b);
    b |= 0x05u; printf("or: %u\n", b);
    b ^= 0xFFu; printf("xor: %u\n", b);


    int arr[3] = { 100, 200, 300 };
    idx = 1;
    *select(arr) += 7;
    printf("once: calls=%d val=%d\n", side_calls, arr[1]);
    return 0;
}
