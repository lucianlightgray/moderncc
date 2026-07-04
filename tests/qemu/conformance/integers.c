int main(void) {
    unsigned u = 0xFFFFFFFFu;
    if (u + 1u != 0u)
        return 1;
    if ((int)u != -1)
        return 2;

    long long s = 1;
    s <<= 40;
    if (s != (1LL << 40))
        return 3;
    if (s >> 40 != 1)
        return 4;

    int a = -7, b = 3;
    if (a / b != -2)
        return 5;
    if (a % b != -1)
        return 6;

    unsigned long long big = 0x100000000ULL;
    if (big * 3ULL != 0x300000000ULL)
        return 7;

    int x = 0x12345678;
    if ((x & 0xFF) != 0x78)
        return 8;
    if ((x ^ x) != 0)
        return 9;
    if (~0 != -1)
        return 10;

    unsigned char c = 200;
    c += 100;
    if (c != 44)
        return 11;

    return 0;
}
