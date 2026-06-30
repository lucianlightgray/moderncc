








extern int printf(const char *, ...);

static int isnan_(double x) { return x != x; }
static int isinf_(double x) { return !isnan_(x) && ((x - x) != (x - x)); }

int main(void)
{
    int ok = 1;
    double inf = 1.0;
    inf = inf / (inf - 1.0);


    double _Complex a = {1, 2}, b = {3, 4}, z;
    z = a * b;
    if (!(__real__ z == -5 && __imag__ z == 10)) ok = 0;


    double _Complex c = {11, -2};
    z = c / b;
    if (!(__real__ z == 1 && __imag__ z == -2)) ok = 0;


    double _Complex one = {1, 0}, infc = {inf, inf};
    z = one / infc;
    if (!(__real__ z == 0 && __imag__ z == 0)) ok = 0;


    double _Complex inf0 = {inf, 0};
    z = a * inf0;
    if (!isinf_(__real__ z)) ok = 0;


    double _Complex zero = {0, 0};
    z = one / zero;
    if (!isinf_(__real__ z)) ok = 0;


    float _Complex fa = {1, 2}, fb = {3, 4}, fz;
    fz = fa * fb;
    if (!((float)__real__ fz == -5 && (float)__imag__ fz == 10)) ok = 0;

    long double _Complex la = {1, 2}, lb = {3, 4}, lz;
    lz = la * lb;
    if (!((long double)__real__ lz == -5 && (long double)__imag__ lz == 10)) ok = 0;

    printf(ok ? "OK\n" : "FAIL\n");
    return 0;
}
