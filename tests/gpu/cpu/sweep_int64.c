#include <stdio.h>

int main(void) {
    unsigned long long chk = 1469598103934665603ULL;
    unsigned long long n_lat = 0, n_probe = 0;
    unsigned long long min_u = ~0ULL, max_u = 0;
    long long min_s = 9223372036854775807LL, max_s = -9223372036854775807LL - 1;
    unsigned int bits_seen_one = 0, bits_seen_zero = 0;
    int i;
    unsigned int k;

    for (i = 0; i < 64; i++) {
        unsigned long long one = 1ULL << i;
        unsigned long long zero = ~one;
        unsigned long long v[2];
        int j;
        v[0] = one;
        v[1] = zero;
        for (j = 0; j < 2; j++) {
            unsigned long long u = v[j];
            long long s = (long long)u;
            if (u < min_u) min_u = u;
            if (u > max_u) max_u = u;
            if (s < min_s) min_s = s;
            if (s > max_s) max_s = s;
            chk = (chk ^ u) * 1099511628211ULL;
            n_lat++;
        }
        if ((one >> i) & 1ULL) bits_seen_one++;
        if (((zero >> i) & 1ULL) == 0ULL) bits_seen_zero++;
    }

    k = 0;
    do {
        unsigned long long u = (unsigned long long)k * 4294967297ULL;
        long long s = (long long)u;
        if (u < min_u) min_u = u;
        if (u > max_u) max_u = u;
        if (s < min_s) min_s = s;
        if (s > max_s) max_s = s;
        chk = (chk ^ u) * 1099511628211ULL;
        n_probe++;
    } while (++k != 0);

    printf("lattice: n=%llu bits_one=%u bits_zero=%u\n",
           n_lat, bits_seen_one, bits_seen_zero);
    printf("probe:   n=%llu\n", n_probe);
    printf("u64: min=%llu max=%llu\n", min_u, max_u);
    printf("s64: min=%lld max=%lld\n", min_s, max_s);
    printf("chk=%llu\n", chk);
    return 0;
}
