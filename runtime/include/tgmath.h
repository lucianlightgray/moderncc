#ifndef _TGMATH_H
#define _TGMATH_H

#include <math.h>
#include <complex.h>
#include <float.h>

#ifndef __cplusplus

#if LDBL_MANT_DIG == DBL_MANT_DIG
/* On ABIs where long double has the same type as double (e.g. MSVC/PE, and
   AArch64 Mach-O), an explicit `long double` / `long double _Complex` arm
   duplicates the double arm and makes _Generic ill-formed ("two compatible
   types"). Fold those into the double/default selection. */
#define __tgmath_real(x, F) \
  _Generic ((x), float: F##f, default: F)(x)
#define __tgmath_real_2_1(x, y, F) \
  _Generic ((x), float: F##f, default: F)(x, y)
#define __tgmath_real_2(x, y, F) \
  _Generic ((x)+(y), float: F##f, default: F)(x, y)
#define __tgmath_real_3_2(x, y, z, F) \
  _Generic ((x)+(y), float: F##f, default: F)(x, y, z)
#define __tgmath_real_3(x, y, z, F) \
  _Generic ((x)+(y)+(z), float: F##f, default: F)(x, y, z)
#define __tgmath_rc(x, F, C) _Generic ((x),                     \
    float _Complex: C##f, double _Complex: C,                   \
    float: F##f, default: F)(x)
#define __tgmath_rc2(x, y, F, C) _Generic ((x)+(y),             \
    float _Complex: C##f, double _Complex: C,                   \
    float: F##f, default: F)(x, y)
#define __tgmath_cx(x, C) _Generic ((x),                        \
    float _Complex: C##f, default: C)(x)
#else
#define __tgmath_real(x, F) \
  _Generic ((x), float: F##f, long double: F##l, default: F)(x)
#define __tgmath_real_2_1(x, y, F) \
  _Generic ((x), float: F##f, long double: F##l, default: F)(x, y)
#define __tgmath_real_2(x, y, F) \
  _Generic ((x)+(y), float: F##f, long double: F##l, default: F)(x, y)
#define __tgmath_real_3_2(x, y, z, F) \
  _Generic ((x)+(y), float: F##f, long double: F##l, default: F)(x, y, z)
#define __tgmath_real_3(x, y, z, F) \
  _Generic ((x)+(y)+(z), float: F##f, long double: F##l, default: F)(x, y, z)

/* 7.25: functions with both real and complex variants dispatch to the
   c-prefixed function when any argument has complex type. */
#define __tgmath_rc(x, F, C) _Generic ((x),                     \
    float _Complex: C##f, double _Complex: C, long double _Complex: C##l, \
    float: F##f, long double: F##l, default: F)(x)
#define __tgmath_rc2(x, y, F, C) _Generic ((x)+(y),             \
    float _Complex: C##f, double _Complex: C, long double _Complex: C##l, \
    float: F##f, long double: F##l, default: F)(x, y)
/* 7.25p7: type-generic forms of the complex-only functions. */
#define __tgmath_cx(x, C) _Generic ((x),                        \
    float _Complex: C##f, long double _Complex: C##l, default: C)(x)
#endif

#define acos(z)          __tgmath_rc(z, acos, cacos)
#define asin(z)          __tgmath_rc(z, asin, casin)
#define atan(z)          __tgmath_rc(z, atan, catan)
#define acosh(z)         __tgmath_rc(z, acosh, cacosh)
#define asinh(z)         __tgmath_rc(z, asinh, casinh)
#define atanh(z)         __tgmath_rc(z, atanh, catanh)
#define cos(z)           __tgmath_rc(z, cos, ccos)
#define sin(z)           __tgmath_rc(z, sin, csin)
#define tan(z)           __tgmath_rc(z, tan, ctan)
#define cosh(z)          __tgmath_rc(z, cosh, ccosh)
#define sinh(z)          __tgmath_rc(z, sinh, csinh)
#define tanh(z)          __tgmath_rc(z, tanh, ctanh)
#define exp(z)           __tgmath_rc(z, exp, cexp)
#define log(z)           __tgmath_rc(z, log, clog)
#define pow(z1,z2)       __tgmath_rc2(z1, z2, pow, cpow)
#define sqrt(z)          __tgmath_rc(z, sqrt, csqrt)
#define fabs(z)          __tgmath_rc(z, fabs, cabs)

#define carg(z)          __tgmath_cx(z, carg)
#define conj(z)          __tgmath_cx(z, conj)
#define cproj(z)         __tgmath_cx(z, cproj)
/* creal/cimag are provided generically by <complex.h>. */

#define atan2(x,y)       __tgmath_real_2(x, y, atan2)
#define cbrt(x)          __tgmath_real(x, cbrt)
#define ceil(x)          __tgmath_real(x, ceil)
#define copysign(x,y)    __tgmath_real_2(x, y, copysign)
#define erf(x)           __tgmath_real(x, erf)
#define erfc(x)          __tgmath_real(x, erfc)
#define exp2(x)          __tgmath_real(x, exp2)
#define expm1(x)         __tgmath_real(x, expm1)
#define fdim(x,y)        __tgmath_real_2(x, y, fdim)
#define floor(x)         __tgmath_real(x, floor)
#define fma(x,y,z)       __tgmath_real_3(x, y, z, fma)
#define fmax(x,y)        __tgmath_real_2(x, y, fmax)
#define fmin(x,y)        __tgmath_real_2(x, y, fmin)
#define fmod(x,y)        __tgmath_real_2(x, y, fmod)
#define frexp(x,y)       __tgmath_real_2_1(x, y, frexp)
#define hypot(x,y)       __tgmath_real_2(x, y, hypot)
#define ilogb(x)         __tgmath_real(x, ilogb)
#define ldexp(x,y)       __tgmath_real_2_1(x, y, ldexp)
#define lgamma(x)        __tgmath_real(x, lgamma)
#define llrint(x)        __tgmath_real(x, llrint)
#define llround(x)       __tgmath_real(x, llround)
#define log10(x)         __tgmath_real(x, log10)
#define log1p(x)         __tgmath_real(x, log1p)
#define log2(x)          __tgmath_real(x, log2)
#define logb(x)          __tgmath_real(x, logb)
#define lrint(x)         __tgmath_real(x, lrint)
#define lround(x)        __tgmath_real(x, lround)
#define nearbyint(x)     __tgmath_real(x, nearbyint)
#define nextafter(x,y)   __tgmath_real_2(x, y, nextafter)
#define nexttoward(x,y)  __tgmath_real_2(x, y, nexttoward)
#define remainder(x,y)   __tgmath_real_2(x, y, remainder)
#define remquo(x,y,z)    __tgmath_real_3_2(x, y, z, remquo)
#define rint(x)          __tgmath_real(x, rint)
#define round(x)         __tgmath_real(x, round)
#define scalbln(x,y)     __tgmath_real_2_1(x, y, scalbln)
#define scalbn(x,y)      __tgmath_real_2_1(x, y, scalbn)
#define tgamma(x)        __tgmath_real(x, tgamma)
#define trunc(x)         __tgmath_real(x, trunc)

#endif
#endif
