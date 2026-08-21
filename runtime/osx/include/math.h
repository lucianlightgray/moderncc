#ifndef _MCC_OSX_MATH_H
#define _MCC_OSX_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HUGE_VAL __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()
#define HUGE_VALL __builtin_huge_vall()
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")

#define FP_NAN 1
#define FP_INFINITE 2
#define FP_ZERO 3
#define FP_NORMAL 4
#define FP_SUBNORMAL 5

#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define isfinite(x) __builtin_isfinite(x)
#define signbit(x) __builtin_signbit(x)

double acos(double);
double asin(double);
double atan(double);
double atan2(double, double);
double cos(double);
double sin(double);
double tan(double);
double cosh(double);
double sinh(double);
double tanh(double);
double exp(double);
double frexp(double, int *);
double ldexp(double, int);
double log(double);
double log2(double);
double log10(double);
double modf(double, double *);
double pow(double, double);
double sqrt(double);
double cbrt(double);
double hypot(double, double);
double ceil(double);
double fabs(double);
double floor(double);
double fmod(double, double);
double round(double);
double trunc(double);
double copysign(double, double);
double nan(const char *);
double fmin(double, double);
double fmax(double, double);
double fma(double, double, double);

float acosf(float);
float asinf(float);
float atanf(float);
float atan2f(float, float);
float cosf(float);
float sinf(float);
float tanf(float);
float expf(float);
float logf(float);
float log2f(float);
float log10f(float);
float powf(float, float);
float sqrtf(float);
float ceilf(float);
float fabsf(float);
float floorf(float);
float fmodf(float, float);
float roundf(float);
float truncf(float);
float copysignf(float, float);
float fminf(float, float);
float fmaxf(float, float);
float fmaf(float, float, float);

long double sqrtl(long double);
long double fabsl(long double);
long double powl(long double, long double);
long double floorl(long double);
long double ceill(long double);
long double fmodl(long double, long double);
long double acosl(long double);
long double asinl(long double);
long double atanl(long double);
long double atan2l(long double, long double);
long double cosl(long double);
long double sinl(long double);
long double tanl(long double);
long double coshl(long double);
long double sinhl(long double);
long double tanhl(long double);
long double expl(long double);
long double logl(long double);
long double log2l(long double);
long double log10l(long double);
long double modfl(long double, long double *);
long double cbrtl(long double);
long double hypotl(long double, long double);
long double roundl(long double);
long double truncl(long double);
long double copysignl(long double, long double);
long double fminl(long double, long double);
long double fmaxl(long double, long double);
long double fmal(long double, long double, long double);

#if __FLT_EVAL_METHOD__ == 0 || __FLT_EVAL_METHOD__ == -1 || __FLT_EVAL_METHOD__ == 16
typedef float float_t;
typedef double double_t;
#elif __FLT_EVAL_METHOD__ == 1
typedef double float_t;
typedef double double_t;
#elif __FLT_EVAL_METHOD__ == 2
typedef long double float_t;
typedef long double double_t;
#else
typedef float float_t;
typedef double double_t;
#endif

#ifdef __cplusplus
}
#endif

#endif
