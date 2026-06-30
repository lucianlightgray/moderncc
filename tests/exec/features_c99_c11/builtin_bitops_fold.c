




extern int printf(const char *, ...);


int fold_clz[__builtin_clz(1u) == 31 ? 1 : -1];
int fold_ctz[__builtin_ctz(8u) == 3 ? 1 : -1];
int fold_pop[__builtin_popcount(0xffu) == 8 ? 1 : -1];
int fold_ffs0[__builtin_ffs(0) == 0 ? 1 : -1];
int fold_ffs[__builtin_ffs(16) == 5 ? 1 : -1];
int fold_par[__builtin_parity(7u) == 1 ? 1 : -1];
int fold_clrsb[__builtin_clrsb(1) == 30 ? 1 : -1];
int fold_clrsb0[__builtin_clrsb(0) == 31 ? 1 : -1];
int fold_clzll[__builtin_clzll(1ull) == 63 ? 1 : -1];
int fold_ctzll[__builtin_ctzll(0x100ull) == 8 ? 1 : -1];

int main(void)
{
    volatile unsigned v = 0xF0u;
    volatile int s = -16;
    volatile unsigned long long L = 0x8000ull;

    int ok = __builtin_clz(v) == 24
          && __builtin_ctz(v) == 4
          && __builtin_popcount(v) == 4
          && __builtin_ffs(s) == 5
          && __builtin_parity(v) == 0
          && __builtin_clrsb(s) == 27
          && __builtin_clzll(L) == 48
          && __builtin_ctzll(L) == 15
          && __builtin_popcountll(L) == 1;

    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
