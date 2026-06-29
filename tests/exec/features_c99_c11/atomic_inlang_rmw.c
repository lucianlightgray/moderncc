/* 6.5.16.2/6.5.2.4: a read-modify-write directly on an _Atomic object
   (a++, --a, a+=x, a-=x, a&=x, a|=x, a^=x) must be a single atomic operation.
   This checks functional results (single-threaded, deterministic) and real
   indivisibility under contention (multiple threads incrementing a shared
   counter — a non-atomic ++ would lose updates and yield < expected). */
#include <stdio.h>
#include <pthread.h>

_Atomic int g;
_Atomic unsigned char uc;
_Atomic long long ll;
_Atomic long counter;

#define NTHREAD 8
#define NSTEP   100000

_Atomic int casbox;        /* exercises the CAS-loop path under contention */

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < NSTEP; i++) {
        counter++;          /* direct atomic increment under contention */
        casbox *= 1;        /* identity via the CAS retry loop (must not corrupt) */
    }
    return 0;
}

int main(void)
{
    int ok = 1;

    /* functional, single-threaded: results must match plain integer math. */
    g = 10;
    int a = g++;            /* post: a=10, g=11 */
    int b = ++g;            /* pre:  g=12, b=12 */
    g += 5;                 /* 17 */
    g -= 2;                 /* 15 */
    g &= 0xfe;              /* 14 */
    g |= 1;                 /* 15 */
    g ^= 4;                 /* 11 */
    ok &= (a == 10 && b == 12 && g == 11);

    uc = 200;
    uc += 100;              /* wraps to 44 in 8 bits */
    ok &= ((unsigned char)uc == 44);

    ll = 1;
    ll += 0x100000000LL;    /* 64-bit atomic add */
    ok &= (ll == 0x100000001LL);

    /* atomic pointer RMW: scaled by pointee size like ordinary pointer math. */
    static int vec[20];
    _Atomic(int *) p = vec;
    p++;                    /* &vec[1] */
    p += 3;                 /* &vec[4] */
    int *q = p--;           /* post: q=&vec[4], p=&vec[3] */
    p -= 2;                 /* &vec[1] */
    ok &= (p - vec == 1 && q - vec == 4);

    /* CAS-loop path: integer * / % << >> have no direct fetch helper. */
    _Atomic int c = 7;
    c *= 3;                 /* 21 */
    c %= 5;                 /* 1  */
    c <<= 4;               /* 16 */
    ok &= (c == 16);
    _Atomic unsigned cu = 0xF0;
    cu >>= 2;              /* 60 */
    cu /= 3;               /* 20 */
    ok &= (cu == 20);

    /* float atomics also go through the CAS loop (desired passed as int bits). */
    _Atomic double ad2 = 2.0;
    ad2 *= 2.5;            /* 5  */
    ad2 += 1.0;            /* 6  */
    ad2 /= 2.0;           /* 3  */
    double adp = ++ad2;   /* 4  */
    ok &= (ad2 == 4.0 && adp == 4.0);

    /* contended: 8 threads * 100000 increments == 800000 iff atomic; the
       identity CAS multiply must leave casbox == its initial value. */
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
