#include <stdio.h>

int main(void) {
    unsigned long long chk_u = 1469598103934665603ULL;
    unsigned long long chk_s = 1469598103934665603ULL;
    unsigned long long n_u = 0, n_s = 0;
    unsigned int min_u = 0xFFFFFFFFu, max_u = 0;
    int min_s = 2147483647, max_s = -2147483647 - 1;
    unsigned int i;

    i = 0;
    do {
        unsigned int u = i;
        if (u < min_u) min_u = u;
        if (u > max_u) max_u = u;
        chk_u = (chk_u ^ u) * 1099511628211ULL;
        n_u++;
    } while (++i != 0);

    i = 0;
    do {
        int s = (int)i;
        if (s < min_s) min_s = s;
        if (s > max_s) max_s = s;
        chk_s = (chk_s ^ (unsigned int)s) * 1099511628211ULL;
        n_s++;
    } while (++i != 0);

    printf("unsigned: n=%llu min=%u max=%u chk=%llu\n", n_u, min_u, max_u, chk_u);
    printf("signed:   n=%llu min=%d max=%d chk=%llu\n", n_s, min_s, max_s, chk_s);
    return 0;
}
