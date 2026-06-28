#ifndef _TCC_COMPLEX_H
#define _TCC_COMPLEX_H


#define complex         _Complex
#define _Complex_I      ((__extension__ (double _Complex){ 0.0, 1.0 }))
#define I               _Complex_I

#define _Imaginary_I    _Complex_I

#define creal(z)        (__real__ (double _Complex)(z))
#define crealf(z)       (__real__ (float _Complex)(z))
#define creall(z)       (__real__ (long double _Complex)(z))
#define cimag(z)        (__imag__ (double _Complex)(z))
#define cimagf(z)       (__imag__ (float _Complex)(z))
#define cimagl(z)       (__imag__ (long double _Complex)(z))

double cabs(double _Complex);
float cabsf(float _Complex);
long double cabsl(long double _Complex);
double carg(double _Complex);
float cargf(float _Complex);
long double cargl(long double _Complex);
double _Complex conj(double _Complex);
float _Complex conjf(float _Complex);
long double _Complex conjl(long double _Complex);
double _Complex cproj(double _Complex);
float _Complex cprojf(float _Complex);
long double _Complex cprojl(long double _Complex);

double _Complex cexp(double _Complex);
float _Complex cexpf(float _Complex);
long double _Complex cexpl(long double _Complex);
double _Complex clog(double _Complex);
float _Complex clogf(float _Complex);
long double _Complex clogl(long double _Complex);
double _Complex cpow(double _Complex, double _Complex);
float _Complex cpowf(float _Complex, float _Complex);
long double _Complex cpowl(long double _Complex, long double _Complex);
double _Complex csqrt(double _Complex);
float _Complex csqrtf(float _Complex);
long double _Complex csqrtl(long double _Complex);

double _Complex csin(double _Complex);
double _Complex ccos(double _Complex);
double _Complex ctan(double _Complex);
double _Complex casin(double _Complex);
double _Complex cacos(double _Complex);
double _Complex catan(double _Complex);
double _Complex csinh(double _Complex);
double _Complex ccosh(double _Complex);
double _Complex ctanh(double _Complex);

#endif
