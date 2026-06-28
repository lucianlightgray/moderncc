#if __aarch64__
#include <stdlib.h>
char **_environ;
wchar_t **_wenviron;
int __argc;
char **__argv;
wchar_t **__wargv;
#endif

#if __aarch64__ || __x86_64__
void __faststorefence(void)
{
#if __aarch64__
    __asm__("dmb ish");
#elif __x86_64__
    __asm__("lock; orl $0,(%%rsp)" ::: "memory");
#endif
}
#endif
