#ifndef _MCC_OSX_FENV_H
#define _MCC_OSX_FENV_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__arm64__) || defined(__aarch64__)

typedef struct {
	unsigned long long __fpsr;
	unsigned long long __fpcr;
} fenv_t;

#define FE_INVALID 0x0001
#define FE_DIVBYZERO 0x0002
#define FE_OVERFLOW 0x0004
#define FE_UNDERFLOW 0x0008
#define FE_INEXACT 0x0010
#define FE_FLUSHTOZERO 0x0080
#define FE_ALL_EXCEPT 0x009f

#define FE_TONEAREST 0x00000000
#define FE_UPWARD 0x00400000
#define FE_DOWNWARD 0x00800000
#define FE_TOWARDZERO 0x00C00000

#elif defined(__i386__) || defined(__x86_64__)

typedef struct {
	unsigned short __control;
	unsigned short __status;
	unsigned int __mxcsr;
	char __reserved[8];
} fenv_t;

#define FE_INVALID 0x0001
#define FE_DENORMALOPERAND 0x0002
#define FE_DIVBYZERO 0x0004
#define FE_OVERFLOW 0x0008
#define FE_UNDERFLOW 0x0010
#define FE_INEXACT 0x0020
#define FE_ALL_EXCEPT 0x003f

#define FE_TONEAREST 0x0000
#define FE_DOWNWARD 0x0400
#define FE_UPWARD 0x0800
#define FE_TOWARDZERO 0x0c00

#else
#error Undefined platform for fenv
#endif

typedef unsigned short fexcept_t;

extern const fenv_t _FE_DFL_ENV;
#define FE_DFL_ENV &_FE_DFL_ENV

int feclearexcept(int);
int fegetexceptflag(fexcept_t *, int);
int feraiseexcept(int);
int fesetexceptflag(const fexcept_t *, int);
int fetestexcept(int);
int fegetround(void);
int fesetround(int);
int fegetenv(fenv_t *);
int feholdexcept(fenv_t *);
int fesetenv(const fenv_t *);
int feupdateenv(const fenv_t *);

#ifdef __cplusplus
}
#endif

#endif
