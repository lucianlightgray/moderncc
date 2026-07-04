void s_annFGK_annex_test(void) {
    double s_annFGK_inf = __builtin_inf();
    double s_annFGK_nan = __builtin_nan("");

    volatile double nz = -0.0;
    double pz = 0.0;
    printf("signbit(-0.0)=%d signbit(+0.0)=%d\n",
           !!__builtin_signbit(nz), !!__builtin_signbit(pz));
    printf("(-0.0)+0.0 signbit=%d\n", !!__builtin_signbit(nz + 0.0));
    printf("(-0.0)-0.0 signbit=%d\n", !!__builtin_signbit(nz - 0.0));
    printf("-(+0.0) signbit=%d\n", !!__builtin_signbit(-pz));
    printf("0.0-(-0.0) signbit=%d\n", !!__builtin_signbit(0.0 - nz));

    printf("isinf(inf)=%d isnan(nan)=%d isfinite(1.0)=%d\n",
           __builtin_isinf(s_annFGK_inf) > 0, !!__builtin_isnan(s_annFGK_nan),
           !!__builtin_isfinite(1.0));
    printf("isinf(-inf)signbit=%d isfinite(inf)=%d isnan(1.0)=%d\n",
           !!__builtin_signbit(-s_annFGK_inf), !!__builtin_isfinite(s_annFGK_inf),
           !!__builtin_isnan(1.0));

    volatile double vnan = s_annFGK_nan, v1 = 1.0;
    printf("nan==nan=%d nan!=nan=%d 1==1=%d\n",
           vnan == vnan, vnan != vnan, v1 == v1);

    volatile double vinf = s_annFGK_inf, vz = 0.0;
    printf("inf*0 isnan=%d 1/0 isinf=%d -1/0 signbit=%d\n",
           !!__builtin_isnan(vinf * vz), __builtin_isinf(v1 / vz) > 0,
           !!__builtin_signbit(-v1 / vz));
    printf("x/2==x*0.5=%d 1*x==x=%d x/1==x=%d\n",
           (7.0 / 2.0) == (7.0 * 0.5), (1.0 * 3.5) == 3.5, (3.5 / 1.0) == 3.5);

    double _Complex z;
    __real__ z = 3.0;
    __imag__ z = 4.0;
    printf("re=%d im=%d\n", (int)__real__ z, (int)__imag__ z);

    double _Complex a;
    __real__ a = 1.0;
    __imag__ a = 2.0;
    double _Complex b;
    __real__ b = 5.0;
    __imag__ b = 10.0;
    double _Complex s = a + b, df = b - a;
    printf("add re=%d im=%d sub re=%d im=%d\n",
           (int)__real__ s, (int)__imag__ s, (int)__real__ df, (int)__imag__ df);

    double _Complex p = a * b;
    printf("mul re=%d im=%d\n", (int)__real__ p, (int)__imag__ p);
    double _Complex qd = p / b;
    printf("div re=%d im=%d\n", (int)__real__ qd, (int)__imag__ qd);

    double _Complex inf0;
    __real__ inf0 = s_annFGK_inf;
    __imag__ inf0 = 0.0;
    double _Complex one_i;
    __real__ one_i = 1.0;
    __imag__ one_i = 1.0;
    double _Complex m = inf0 * one_i;
    printf("cmul_inf re_isinf=%d im_isinf=%d\n",
           __builtin_isinf(__real__ m) != 0, __builtin_isinf(__imag__ m) != 0);

    double _Complex infd;
    __real__ infd = s_annFGK_inf;
    __imag__ infd = s_annFGK_inf;
    double _Complex q2 = one_i / infd;
    printf("cdiv_infdiv re=%d im=%d\n", (int)__real__ q2, (int)__imag__ q2);
}
