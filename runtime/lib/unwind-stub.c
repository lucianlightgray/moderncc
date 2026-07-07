#ifndef __MCC__
#include <stdlib.h>
#include <unistd.h>
#endif

#ifdef __MCC__
extern void abort(void);
extern long write(int, const void *, unsigned long);
#endif

static void unwind_unsupported(const char *who) {
	static const char pre[] = "mcc: static runtime: unsupported unwinder call: ";
	write(2, pre, sizeof(pre) - 1);
	const char *p = who;
	while (*p)
		p++;
	write(2, who, (unsigned long)(p - who));
	write(2, "\n", 1);
	abort();
}

__attribute__((weak, noreturn)) void _Unwind_Resume(void *exc) {
	(void)exc;
	unwind_unsupported("_Unwind_Resume");
}

__attribute__((weak)) int _Unwind_ForcedUnwind(void *exc, void *stop, void *stop_arg) {
	(void)exc;
	(void)stop;
	(void)stop_arg;
	unwind_unsupported("_Unwind_ForcedUnwind");
	return 0;
}

__attribute__((weak)) void *_Unwind_GetCFA(void *ctx) {
	(void)ctx;
	unwind_unsupported("_Unwind_GetCFA");
	return 0;
}

__attribute__((weak)) int __gcc_personality_v0(int version, int actions, unsigned long long exc_class,
																							 void *exc_obj, void *ctx) {
	(void)version;
	(void)actions;
	(void)exc_class;
	(void)exc_obj;
	(void)ctx;
	unwind_unsupported("__gcc_personality_v0");
	return 0;
}
