




#include <stdio.h>

static void pr(const char *t, double _Complex z)
{
    printf("%s %g %g\n", t, __real__ z, __imag__ z);
}

static double _Complex cmul(double _Complex a, double _Complex b) { return a * b; }
static double mag2(double _Complex z) { return __real__ z * __real__ z + __imag__ z * __imag__ z; }

int main(void)
{
    double _Complex a, b;
    __real__ a = 1; __imag__ a = 2;
    __real__ b = 3; __imag__ b = 4;

    pr("add", a + b);
    pr("sub", a - b);
    pr("mul", a * b);
    pr("div", a / b);
    pr("radd", 10 + a);
    pr("rmul", a * 2);
    pr("neg", -a);
    pr("call", cmul(a, b));

    printf("eq %d %d\n", a == b, a == a);
    printf("ne %d\n", a != b);
    printf("mag2 %g\n", mag2(b));


    double _Complex c = a;
    __real__ c = __real__ c + 100;
    pr("copy", c);


    printf("c2r %g\n", (double)b);
    double _Complex d = (double _Complex)7.0;
    pr("r2c", d);
    float _Complex f = (float _Complex)a;
    printf("c2c %g %g\n", (double)__real__ f, (double)__imag__ f);


    float _Complex g, h;
    __real__ g = 1.5f; __imag__ g = 0.5f;
    __real__ h = 2.0f; __imag__ h = -1.0f;
    float _Complex s = g + h;
    printf("fadd %g %g\n", (double)__real__ s, (double)__imag__ s);


    double _Complex k = 3.0 + 4.0i;
    pr("isuf", k);
    pr("imul", 2.0i * 3.0i);
    return 0;
}
