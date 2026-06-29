/* GCC/Clang-compatible __builtin_{add,sub,mul}_overflow.
   Type-generic (dispatched on the result type via _Generic over per-type
   runtime helpers in runtime/lib/builtin.c). For same-type operands the result
   and overflow flag are bit-for-bit identical to gcc (verified differentially). */
extern int printf(const char *, ...);

static int fails;
#define CK(cond) do { if (!(cond)) { printf("FAIL line %d\n", __LINE__); fails++; } } while (0)

int main(void)
{
    int r;
    long long rl;
    unsigned ru;
    unsigned long long rull;
    short rs;
    unsigned char rc;

    /* int add/sub/mul: no overflow */
    CK(__builtin_add_overflow(2, 3, &r) == 0 && r == 5);
    CK(__builtin_sub_overflow(10, 3, &r) == 0 && r == 7);
    CK(__builtin_mul_overflow(1000, 1000, &r) == 0 && r == 1000000);

    /* int overflow */
    CK(__builtin_add_overflow(2000000000, 2000000000, &r) == 1);
    CK(__builtin_mul_overflow(100000, 100000, &r) == 1);
    CK(__builtin_sub_overflow(-2000000000, 2000000000, &r) == 1);

    /* signed int edge: INT_MIN * -1 overflows */
    CK(__builtin_mul_overflow(-2147483647 - 1, -1, &r) == 1);

    /* long long: no wider type; sign-trick add and guarded-division mul */
    CK(__builtin_add_overflow(100LL, 23LL, &rl) == 0 && rl == 123);
    CK(__builtin_add_overflow(9223372036854775807LL, 1LL, &rl) == 1);
    CK(__builtin_mul_overflow(4000000000LL, 4000000000LL, &rl) == 1);
    CK(__builtin_mul_overflow(-9223372036854775807LL - 1, -1LL, &rl) == 1);
    CK(__builtin_mul_overflow(3LL, 7LL, &rl) == 0 && rl == 21);

    /* unsigned wrap-around detection */
    CK(__builtin_add_overflow(0xFFFFFFFFu, 1u, &ru) == 1);
    CK(__builtin_mul_overflow(0x10000u, 0x10000u, &ru) == 1);
    CK(__builtin_add_overflow(0xFFFFFFFFFFFFFFFFull, 1ull, &rull) == 1);
    CK(__builtin_mul_overflow(10ull, 20ull, &rull) == 0 && rull == 200);

    /* narrow result types (short, unsigned char) */
    CK(__builtin_add_overflow(30000, 30000, &rs) == 1);
    CK(__builtin_add_overflow(100, 200, &rc) == 1);     /* > 255 */
    CK(__builtin_sub_overflow(5, 10, &rc) == 1);        /* < 0 unsigned */
    CK(__builtin_add_overflow(40, 50, &rc) == 0 && rc == 90);

    printf(fails ? "FAIL\n" : "OK\n");
    return fails ? 1 : 0;
}
