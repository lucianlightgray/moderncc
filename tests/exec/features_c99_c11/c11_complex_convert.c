/* 6.3.1.6/6.3.1.7/6.7.9: real->complex and complex->complex conversions in
   assignment, return, argument passing, and block-scope initialization. */

extern int printf(const char *, ...);

double _Complex g_real = 4.0;          /* static/global init from real */
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

    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
