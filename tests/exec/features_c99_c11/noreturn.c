/* _Noreturn functions (C11 §6.7.4). Valid runtime uses: a function marked
 * _Noreturn / noreturn that genuinely does not return (here via longjmp).
 * Exercises both the _Noreturn keyword and the <stdnoreturn.h> `noreturn`
 * macro, and the two placements (before/after the return type). Mirrors the
 * valid half of gcc c11-noreturn-2.c. Prints OK. */
#include <stdio.h>
#include <setjmp.h>
#include <stdnoreturn.h>

static jmp_buf jb;

_Noreturn static void diverge_kw(int v) { longjmp(jb, v); }
noreturn static void diverge_macro(void) { longjmp(jb, 99); }
static noreturn void diverge_post(void) { longjmp(jb, 7); }

int main(void) {
	int ok = 1;
	volatile int reached;

	reached = 0;
	if (setjmp(jb) == 0) {
		diverge_kw(1);
		reached = 1; /* unreachable */
	}
	ok &= !reached;

	reached = 0;
	if (setjmp(jb) == 0) {
		diverge_macro();
		reached = 1; /* unreachable */
	}
	ok &= !reached;

	reached = 0;
	if (setjmp(jb) == 0) {
		diverge_post();
		reached = 1; /* unreachable */
	}
	ok &= !reached;

	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
