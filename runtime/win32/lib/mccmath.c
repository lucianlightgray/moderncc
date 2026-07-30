/* i386-win32 single-precision float math fallback.
 *
 * The 32-bit msvcrt.dll (SysWOW64) does NOT export the single-precision float
 * math family (sqrtf/expf/powf/hypotf/...); the 64-bit msvcrt.dll does. mcc's
 * msvcrt import def is shared across win32 arches, so on i386 an undefined
 * `sqrtf` would otherwise be bound to a msvcrt export that does not exist, and
 * the resulting PE fails to LOAD with STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139)
 * before main runs.
 *
 * gcc/mingw avoid this because their i686 libmsvcrt.a omits these symbols and
 * libmingwex.a supplies them statically. This file is mcc's libmingwex peer:
 * thin wrappers over the double-precision msvcrt entries (which ARE exported on
 * 32-bit), pulled from libmccrt.a on demand only when the float symbol is still
 * undefined after the command line — so a program that links a real libm (e.g.
 * winlibs -lm on the mingw CI cell) uses that instead, no duplicate symbol.
 *
 * Guarded to i386: x86_64/arm64 win32 keep binding these to their own CRT and
 * this object is empty for them (never archived-in). Double-rounding through
 * (float)f((double)x) matches a true single-precision result for these entries.
 */
#if defined(__i386__)

extern double __cdecl sqrt(double);
extern double __cdecl exp(double);
extern double __cdecl log(double);
extern double __cdecl log10(double);
extern double __cdecl pow(double, double);
extern double __cdecl sin(double);
extern double __cdecl cos(double);
extern double __cdecl tan(double);
extern double __cdecl asin(double);
extern double __cdecl acos(double);
extern double __cdecl atan(double);
extern double __cdecl atan2(double, double);
extern double __cdecl sinh(double);
extern double __cdecl cosh(double);
extern double __cdecl tanh(double);
extern double __cdecl floor(double);
extern double __cdecl ceil(double);
extern double __cdecl fmod(double, double);
extern double __cdecl fabs(double);
extern double __cdecl _hypot(double, double);

float __cdecl sqrtf(float x) { return (float)sqrt((double)x); }
float __cdecl expf(float x) { return (float)exp((double)x); }
float __cdecl logf(float x) { return (float)log((double)x); }
float __cdecl log10f(float x) { return (float)log10((double)x); }
float __cdecl powf(float x, float y) { return (float)pow((double)x, (double)y); }
float __cdecl sinf(float x) { return (float)sin((double)x); }
float __cdecl cosf(float x) { return (float)cos((double)x); }
float __cdecl tanf(float x) { return (float)tan((double)x); }
float __cdecl asinf(float x) { return (float)asin((double)x); }
float __cdecl acosf(float x) { return (float)acos((double)x); }
float __cdecl atanf(float x) { return (float)atan((double)x); }
float __cdecl atan2f(float x, float y) { return (float)atan2((double)x, (double)y); }
float __cdecl sinhf(float x) { return (float)sinh((double)x); }
float __cdecl coshf(float x) { return (float)cosh((double)x); }
float __cdecl tanhf(float x) { return (float)tanh((double)x); }
float __cdecl floorf(float x) { return (float)floor((double)x); }
float __cdecl ceilf(float x) { return (float)ceil((double)x); }
float __cdecl fmodf(float x, float y) { return (float)fmod((double)x, (double)y); }
float __cdecl fabsf(float x) { return (float)fabs((double)x); }
float __cdecl hypotf(float x, float y) { return (float)_hypot((double)x, (double)y); }
float __cdecl _hypotf(float x, float y) { return (float)_hypot((double)x, (double)y); }

#endif /* __i386__ */
