static int isnan_(double x) {
    return x != x;
}
static int isinf_(double x) {
    return !isnan_(x) && ((x - x) != (x - x));
}

int main(void) {
    volatile double z = 0.0;
    double inf = 1.0 / z;

    double _Complex a = {1, 2}, b = {3, 4}, r;
    r = a * b;
    if (__real__ r != -5 || __imag__ r != 10)
        return 1;

    double _Complex c = {11, -2};
    r = c / b;
    if (__real__ r != 1 || __imag__ r != -2)
        return 2;

    double _Complex one = {1, 0}, infc = {inf, inf};
    r = one / infc;
    if (__real__ r != 0 || __imag__ r != 0)
        return 3;

    double _Complex inf0 = {inf, 0};
    r = a * inf0;
    if (!isinf_(__real__ r))
        return 4;

    double _Complex zero = {0, 0};
    r = one / zero;
    if (!isinf_(__real__ r))
        return 5;

    float _Complex fa = {1, 2}, fb = {3, 4}, fr;
    fr = fa * fb;
    if ((float)__real__ fr != -5 || (float)__imag__ fr != 10)
        return 6;

    long double _Complex la = {1, 2}, lb = {3, 4}, lr;
    lr = la * lb;
    if ((long double)__real__ lr != -5 || (long double)__imag__ lr != 10)
        return 7;

    return 0;
}
