extern int printf(const char *, ...);

int fold16[__builtin_bswap16(0x1234) == 0x3412 ? 1 : -1];
int fold32[__builtin_bswap32(0x12345678u) == 0x78563412u ? 1 : -1];
int fold64[__builtin_bswap64(0x0123456789abcdefull) == 0xefcdab8967452301ull ? 1 : -1];

int main(void) {
    volatile unsigned short av = 0x1234;
    volatile unsigned int bv = 0x12345678u;
    volatile unsigned long long cv = 0x0123456789abcdefull;
    unsigned short a = __builtin_bswap16(av);
    unsigned int b = __builtin_bswap32(bv);
    unsigned long long c = __builtin_bswap64(cv);

    int ok = a == 0x3412 && b == 0x78563412u && c == 0xefcdab8967452301ull;
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
