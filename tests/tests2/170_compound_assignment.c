/* C99 6.5.16.2: compound assignment. `E1 op= E2` behaves like `E1 = E1 op E2`
   except the lvalue E1 is evaluated only once. Covers each arithmetic and
   bitwise compound operator and the single-evaluation guarantee. */
#include <stdio.h>

int side_calls = 0;
int idx = 0;
int *select(int *base) { side_calls++; return &base[idx]; }

int main(void)
{
    int v = 10;
    v += 5;  printf("add: %d\n", v);    /* 15 */
    v -= 3;  printf("sub: %d\n", v);    /* 12 */
    v *= 4;  printf("mul: %d\n", v);    /* 48 */
    v /= 5;  printf("div: %d\n", v);    /* 9 */
    v %= 4;  printf("mod: %d\n", v);    /* 1 */

    unsigned b = 0x0Fu;
    b <<= 4;  printf("shl: %u\n", b);   /* 0xF0 = 240 */
    b >>= 2;  printf("shr: %u\n", b);   /* 0x3C = 60 */
    b &= 0x30u; printf("and: %u\n", b); /* 0x30 = 48 */
    b |= 0x05u; printf("or: %u\n", b);  /* 0x35 = 53 */
    b ^= 0xFFu; printf("xor: %u\n", b); /* 0xCA = 202 */

    /* single-evaluation: the lvalue expression (a function call here) runs once */
    int arr[3] = { 100, 200, 300 };
    idx = 1;
    *select(arr) += 7;
    printf("once: calls=%d val=%d\n", side_calls, arr[1]);  /* 1 207 */
    return 0;
}
