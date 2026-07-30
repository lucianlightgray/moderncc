#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define ODD 0x9E3779B97F4A7C15ULL
#define MAXT 256

static unsigned long long rev64(unsigned long long v) {
    v = ((v >> 1) & 0x5555555555555555ULL) | ((v & 0x5555555555555555ULL) << 1);
    v = ((v >> 2) & 0x3333333333333333ULL) | ((v & 0x3333333333333333ULL) << 2);
    v = ((v >> 4) & 0x0F0F0F0F0F0F0F0FULL) | ((v & 0x0F0F0F0F0F0F0F0FULL) << 4);
    v = ((v >> 8) & 0x00FF00FF00FF00FFULL) | ((v & 0x00FF00FF00FF00FFULL) << 8);
    v = ((v >> 16) & 0x0000FFFF0000FFFFULL) | ((v & 0x0000FFFF0000FFFFULL) << 16);
    v = (v >> 32) | (v << 32);
    return v;
}

static int pop64(unsigned long long v) {
    v = v - ((v >> 1) & 0x5555555555555555ULL);
    v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
    v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (int)((v * 0x0101010101010101ULL) >> 56);
}

struct job {
    unsigned long long lo, hi;
    unsigned long long n, xsum, asum, psum;
    unsigned long long bad_inv, bad_pop;
    unsigned long long first_bad;
};

static void *work(void *p) {
    struct job *j = (struct job *)p;
    unsigned long long i;
    unsigned long long xs = 0, as = 0, ps = 0;
    unsigned long long n = 0, bi = 0, bp = 0, fb = 0;

    for (i = j->lo; i < j->hi; i++) {
        unsigned long long x = i * ODD;
        unsigned long long r = rev64(x);
        if (rev64(r) != x) { if (!bi) fb = x; bi++; }
        if (pop64(r) != pop64(x)) { if (!bp) fb = x; bp++; }
        xs ^= r;
        as += r;
        ps += (unsigned long long)pop64(r);
        n++;
    }
    j->n = n; j->xsum = xs; j->asum = as; j->psum = ps;
    j->bad_inv = bi; j->bad_pop = bp; j->first_bad = fb;
    return 0;
}

int main(int argc, char **argv) {
    pthread_t th[MAXT];
    struct job jb[MAXT];
    int nt = (argc > 1) ? atoi(argv[1]) : 32;
    unsigned long long total = (argc > 2)
        ? strtoull(argv[2], 0, 0) : (1ULL << 32);
    unsigned long long n = 0, bi = 0, bp = 0, fb = 0;
    unsigned long long xsum = 0, asum = 0, psum = 0;
    int lat_ok = 1;
    int i;

    if (nt < 1) nt = 1;
    if (nt > MAXT) nt = MAXT;

    for (i = 0; i < 64; i++) {
        unsigned long long one = 1ULL << i;
        if (rev64(one) != (1ULL << (63 - i))) lat_ok = 0;
        if (rev64(~one) != ~(1ULL << (63 - i))) lat_ok = 0;
    }
    if (rev64(0ULL) != 0ULL) lat_ok = 0;
    if (rev64(~0ULL) != ~0ULL) lat_ok = 0;

    for (i = 0; i < nt; i++) {
        jb[i].lo = total * (unsigned long long)i / (unsigned long long)nt;
        jb[i].hi = total * (unsigned long long)(i + 1) / (unsigned long long)nt;
        jb[i].n = jb[i].xsum = jb[i].asum = jb[i].psum = 0;
        jb[i].bad_inv = jb[i].bad_pop = jb[i].first_bad = 0;
        if (pthread_create(&th[i], 0, work, &jb[i]) != 0) {
            printf("pthread_create failed at %d\n", i);
            return 1;
        }
    }
    for (i = 0; i < nt; i++) pthread_join(th[i], 0);

    for (i = 0; i < nt; i++) {
        n += jb[i].n;
        bi += jb[i].bad_inv;
        bp += jb[i].bad_pop;
        if (!fb && jb[i].first_bad) fb = jb[i].first_bad;
        xsum ^= jb[i].xsum;
        asum += jb[i].asum;
        psum += jb[i].psum;
    }

    printf("threads=%d total=%llu swept=%llu\n", nt, total, n);
    printf("lattice_ok=%d bad_involution=%llu bad_popcount=%llu first_bad=%llx\n",
           lat_ok, bi, bp, fb);
    printf("xsum=%016llx asum=%016llx psum=%llu\n", xsum, asum, psum);
    return (n == total && bi == 0 && bp == 0 && lat_ok) ? 0 : 1;
}
