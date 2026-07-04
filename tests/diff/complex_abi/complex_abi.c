#include "harness.h"

typedef float _Complex cf;
typedef double _Complex cd;
typedef long double _Complex cl;

static cf mulf(cf a, cf b) {
    return a * b;
}
static cd muld(cd a, cd b) {
    return a * b;
}
static cl mull(cl a, cl b) {
    return a * b;
}

static cd mix(int a, cd z, int b) {
    return (a + b) + z;
}

int run(void) {

    cd a = 1.0 + 2.0i, b = 3.0 + 4.0i;
    cd s = a + b;
    if (__real__ s != 4.0 || __imag__ s != 6.0)
        return 1;
    cd d = b - a;
    if (__real__ d != 2.0 || __imag__ d != 2.0)
        return 2;
    cd p = muld(a, b);
    if (__real__ p != -5.0 || __imag__ p != 10.0)
        return 3;
    cd q = p / b;
    if (__real__ q != 1.0 || __imag__ q != 2.0)
        return 4;
    if (!(a == (1.0 + 2.0i)))
        return 5;
    if (a != (1.0 + 2.0i))
        return 6;
    cd r = a + 10.0;
    if (__real__ r != 11.0 || __imag__ r != 2.0)
        return 7;
    cd m = mix(3, a, 4);
    if (__real__ m != 8.0 || __imag__ m != 2.0)
        return 8;

    cf fa = 1.0f + 2.0fi, fb = 3.0f + 4.0fi;
    cf fs = fa + fb;
    if (__real__ fs != 4.0f || __imag__ fs != 6.0f)
        return 10;
    cf fp = mulf(fa, fb);
    if (__real__ fp != -5.0f || __imag__ fp != 10.0f)
        return 11;
    cf fr = fa * 2.0f;
    if (__real__ fr != 2.0f || __imag__ fr != 4.0f)
        return 12;

    cl la;
    __real__ la = 5;
    __imag__ la = 6;
    cl lb;
    __real__ lb = 2;
    __imag__ lb = 3;
    cl ls = la + lb;
    if (__real__ ls != 7.0L || __imag__ ls != 9.0L)
        return 13;
    cl lp = mull(la, lb);
    if (__real__ lp != -8.0L || __imag__ lp != 27.0L)
        return 14;
    cl lq = lp / lb;
    if (__real__ lq != 5.0L || __imag__ lq != 6.0L)
        return 15;
    cl lr = la + 3.0L;
    if (__real__ lr != 8.0L || __imag__ lr != 6.0L)
        return 16;

    cd cv = (cd)7.0;
    if (__real__ cv != 7.0 || __imag__ cv != 0.0)
        return 17;
    double re = (double)b;
    if (re != 3.0)
        return 18;
    cf narrowed = (cf)a;
    if (__real__ narrowed != 1.0f || __imag__ narrowed != 2.0f)
        return 19;

    cd ca = 1.0 + 1.0i;
    ca += 2.0 + 3.0i;
    if (__real__ ca != 3.0 || __imag__ ca != 4.0)
        return 20;
    cd cm = 1.0 + 1.0i;
    cm *= 2.0;
    if (__real__ cm != 2.0 || __imag__ cm != 2.0)
        return 21;

    return 0;
}

TEST_MAIN()
