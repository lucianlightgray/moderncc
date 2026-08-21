#if __aarch64__ || __x86_64__
#include <stdlib.h>
#include <stdio.h>
char **_environ;
wchar_t **_wenviron;
int __argc;
char **__argv;
wchar_t **__wargv;

int __cdecl _crt_atexit(void(__cdecl *)(void));
int __cdecl atexit(void(__cdecl *__func)(void)) {
	return _crt_atexit(__func);
}

void __cdecl _assert(const char *_msg, const char *_file, unsigned _line) {
	fprintf(stderr, "Assertion failed: %s, file %s, line %u\n", _msg, _file, _line);
	fflush(stderr);
	abort();
}
#endif

#if __aarch64__ || __x86_64__
void __faststorefence(void) {
#if __aarch64__
	__asm__("dmb ish");
#elif __x86_64__
	__asm__("lock; orl $0,(%%rsp)" ::: "memory");
#endif
}
#endif
