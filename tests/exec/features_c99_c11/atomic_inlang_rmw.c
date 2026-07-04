#include <stdio.h>
#include <pthread.h>

_Atomic int g;
_Atomic unsigned char uc;
_Atomic long long ll;
_Atomic long counter;

#define NTHREAD 8
#define NSTEP 100000

_Atomic int casbox;

static void *worker(void *arg) {
    (void)arg;
    for (int i = 0; i < NSTEP; i++) {
        counter++;
        casbox *= 1;
    }
    return 0;
}

int main(void) {
    int ok = 1;

    g = 10;
    int a = g++;
    int b = ++g;
    g += 5;
    g -= 2;
    g &= 0xfe;
    g |= 1;
    g ^= 4;
    ok &= (a == 10 && b == 12 && g == 11);

    uc = 200;
    uc += 100;
    ok &= ((unsigned char)uc == 44);

    ll = 1;
    ll += 0x100000000LL;
    ok &= (ll == 0x100000001LL);

    static int vec[20];
    _Atomic(int *) p = vec;
    p++;
    p += 3;
    int *q = p--;
    p -= 2;
    ok &= (p - vec == 1 && q - vec == 4);

    _Atomic int c = 7;
    c *= 3;
    c %= 5;
    c <<= 4;
    ok &= (c == 16);
    _Atomic unsigned cu = 0xF0;
    cu >>= 2;
    cu /= 3;
    ok &= (cu == 20);

    _Atomic double ad2 = 2.0;
    ad2 *= 2.5;
    ad2 += 1.0;
    ad2 /= 2.0;
    double adp = ++ad2;
    ok &= (ad2 == 4.0 && adp == 4.0);

    counter = 0;
    casbox = 12345;
    pthread_t t[NTHREAD];
    for (int i = 0; i < NTHREAD; i++)
        pthread_create(&t[i], 0, worker, 0);
    for (int i = 0; i < NTHREAD; i++)
        pthread_join(t[i], 0);
    ok &= ((long)counter == (long)NTHREAD * NSTEP);
    ok &= (casbox == 12345);

    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
