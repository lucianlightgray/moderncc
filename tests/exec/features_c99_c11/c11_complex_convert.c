/* 6.3.1.6/6.3.1.7/6.7.9: real->complex and complex->complex conversions in
   assignment, return, argument passing, and block-scope initialization. */

#include <complex.h>                   /* CMPLX / CMPLXF */
extern int printf(const char *, ...);

double _Complex g_real = 4.0;          /* static/global init from real */
double _Complex g_cmplx = CMPLX(5.0, 6.0);     /* 7.3.9.3: static CMPLX constant */
float _Complex  g_cI = _Complex_I;             /* 7.3.1: static _Complex_I constant */
static float _Complex g_int = 6;       /* static/global init from int */

static double _Complex ret_real(void) { return 7.0; }      /* return real->complex */

static int check_arg(double _Complex z, double re, double im)
{
    return __real__ z == re && __imag__ z == im;
}

int main(void)
{
    int ok = 1;

    if (!(__real__ g_real == 4 && __imag__ g_real == 0)) ok = 0;  /* global real */
    if (!(__real__ g_cmplx == 5 && __imag__ g_cmplx == 6)) ok = 0; /* static CMPLX */
    if (!((double)__real__ g_cI == 0 && (double)__imag__ g_cI == 1)) ok = 0; /* static I */
    if (!((double)__real__ g_int == 6 && (double)__imag__ g_int == 0)) ok = 0; /* global int */

    double _Complex z = 3.0;        /* init from real */
    if (!(__real__ z == 3 && __imag__ z == 0)) ok = 0;

    double _Complex zi = 5;         /* init from int */
    if (!(__real__ zi == 5 && __imag__ zi == 0)) ok = 0;

    z = 9;                          /* assign from int */
    if (!(__real__ z == 9 && __imag__ z == 0)) ok = 0;

    double r = 2.5;
    z = r;                          /* assign from real var */
    if (!(__real__ z == 2.5 && __imag__ z == 0)) ok = 0;

    double _Complex w = {1.0, 2.0};
    z = w;                          /* complex -> complex */
    if (!(__real__ z == 1 && __imag__ z == 2)) ok = 0;

    if (!(__real__ ret_real() == 7 && __imag__ ret_real() == 0)) ok = 0;  /* return */

    if (!check_arg(4.0, 4.0, 0.0)) ok = 0;     /* real argument -> complex param */
    if (!check_arg(w, 1.0, 2.0)) ok = 0;       /* complex argument */

    float _Complex f = (float _Complex)w;      /* complex -> complex precision */
    if (!((double)__real__ f == 1 && (double)__imag__ f == 2)) ok = 0;

    /* 6.3.1.6: implicit complex->complex across element types (not a cast). */
    float _Complex fc = {1.0f, 2.0f};
    double _Complex wd = fc;                    /* init float -> double complex */
    if (!(__real__ wd == 1 && __imag__ wd == 2)) ok = 0;
    long double _Complex wl = fc;               /* init float -> long double */
    if (!((double)__real__ wl == 1 && (double)__imag__ wl == 2)) ok = 0;
    double _Complex wd2; wd2 = fc;              /* assign float -> double complex */
    if (!(__real__ wd2 == 1 && __imag__ wd2 == 2)) ok = 0;

    /* 6.3.1.7: implicit complex->integer / _Bool drops the imaginary part. */
    double _Complex zz = {3.0, 4.0};
    int n = zz;                                 /* init complex -> int */
    if (n != 3) ok = 0;
    n = 0; n = zz;                              /* assign complex -> int */
    if (n != 3) ok = 0;
    _Bool bb = zz;                              /* complex -> _Bool (nonzero) */
    if (bb != 1) ok = 0;

    /* 7.3.9.3: __builtin_complex / CMPLX construct a complex from two parts. */
    double _Complex bz = __builtin_complex(7.0, 8.0);
    if (!(__real__ bz == 7 && __imag__ bz == 8)) ok = 0;
    double _Complex cz = CMPLX(9.0, 10.0);
    if (!(__real__ cz == 9 && __imag__ cz == 10)) ok = 0;
    float _Complex cf = CMPLXF(1.5f, 2.5f);
    if (!((double)__real__ cf == 1.5 && (double)__imag__ cf == 2.5)) ok = 0;

    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
