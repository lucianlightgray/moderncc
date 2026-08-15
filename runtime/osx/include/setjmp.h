#ifndef _MCC_OSX_SETJMP_H
#define _MCC_OSX_SETJMP_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__x86_64__)
#define _JBLEN ((9 * 2) + 3 + 16)
#elif defined(__i386__)
#define _JBLEN (18)
#elif defined(__arm64__) || defined(__aarch64__)
#define _JBLEN ((14 + 8 + 2) * 2)
#elif defined(__arm__)
#define _JBLEN (10 + 16 + 2)
#else
#error Undefined platform for setjmp
#endif

typedef int jmp_buf[_JBLEN];
typedef int sigjmp_buf[_JBLEN + 1];

int setjmp(jmp_buf);
void longjmp(jmp_buf, int);
int _setjmp(jmp_buf);
void _longjmp(jmp_buf, int);
int sigsetjmp(sigjmp_buf, int);
void siglongjmp(sigjmp_buf, int);

#ifdef __cplusplus
}
#endif

#endif
