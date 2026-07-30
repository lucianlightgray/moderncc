#include <stdio.h>

union fb { float f; unsigned int b; };

int main(void) {
    unsigned long long n = 0;
    unsigned long long n_zero = 0, n_sub = 0, n_norm = 0, n_inf = 0, n_nan = 0;
    unsigned long long n_snan = 0, n_selfne = 0, n_rt_bad = 0;
    unsigned long long chk = 1469598103934665603ULL;
    unsigned int first_rt_bad = 0, first_rt_got = 0;
    unsigned int i;

    i = 0;
    do {
        union fb x, y;
        double d;
        unsigned int e, m;

        x.b = i;
        e = (i >> 23) & 0xFFu;
        m = i & 0x7FFFFFu;

        if (e == 0u && m == 0u) n_zero++;
        else if (e == 0u) n_sub++;
        else if (e == 0xFFu && m == 0u) n_inf++;
        else if (e == 0xFFu) {
            n_nan++;
            if (((m >> 22) & 1u) == 0u) n_snan++;
        } else n_norm++;

        if (!(x.f == x.f)) n_selfne++;

        d = (double)x.f;
        y.f = (float)d;
        if (y.b != x.b) {
            if (n_rt_bad == 0) { first_rt_bad = x.b; first_rt_got = y.b; }
            n_rt_bad++;
        }

        chk = (chk ^ x.b) * 1099511628211ULL;
        n++;
    } while (++i != 0);

    printf("n=%llu\n", n);
    printf("zero=%llu sub=%llu norm=%llu inf=%llu nan=%llu (snan=%llu)\n",
           n_zero, n_sub, n_norm, n_inf, n_nan, n_snan);
    printf("sum=%llu\n", n_zero + n_sub + n_norm + n_inf + n_nan);
    printf("self_ne=%llu\n", n_selfne);
    printf("roundtrip_mismatch=%llu first=%08x got=%08x\n",
           n_rt_bad, first_rt_bad, first_rt_got);
    printf("chk=%llu\n", chk);
    return 0;
}
