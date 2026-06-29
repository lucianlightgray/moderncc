#include <complex.h>

extern int printf(const char *, ...);

/* 7.3.5/7.3.6: every trig & hyperbolic complex function must have f and l
   variants. Verify the declarations exist with the right types at compile
   time (no libm link needed). */
#define CHK_F(name) _Static_assert( \
    __builtin_types_compatible_p(__typeof__(name##f), \
        float _Complex(float _Complex)), #name "f decl")
#define CHK_L(name) _Static_assert( \
    __builtin_types_compatible_p(__typeof__(name##l), \
        long double _Complex(long double _Complex)), #name "l decl")
#define CHK(name) CHK_F(name); CHK_L(name)

CHK(csin);  CHK(ccos);  CHK(ctan);
CHK(casin); CHK(cacos); CHK(catan);
CHK(csinh); CHK(ccosh); CHK(ctanh);
CHK(casinh); CHK(cacosh); CHK(catanh);

int main(void)
{
    /* libm-free sanity: I and the real+complex add path */
    double _Complex z = 1.0 + 2.0 * I;
    int ok = (creal(z) == 1.0) && (cimag(z) == 2.0);
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
