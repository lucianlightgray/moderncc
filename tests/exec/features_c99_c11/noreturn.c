#include <stdio.h>
#include <setjmp.h>
#include <stdnoreturn.h>

static jmp_buf jb;

_Noreturn static void diverge_kw(int v) {
	longjmp(jb, v);
}
noreturn static void diverge_macro(void) {
	longjmp(jb, 99);
}
static noreturn void diverge_post(void) {
	longjmp(jb, 7);
}

int main(void) {
	int ok = 1;
	volatile int reached;

	reached = 0;
	if (setjmp(jb) == 0) {
		diverge_kw(1);
		reached = 1;
	}
	ok &= !reached;

	reached = 0;
	if (setjmp(jb) == 0) {
		diverge_macro();
		reached = 1;
	}
	ok &= !reached;

	reached = 0;
	if (setjmp(jb) == 0) {
		diverge_post();
		reached = 1;
	}
	ok &= !reached;

	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
