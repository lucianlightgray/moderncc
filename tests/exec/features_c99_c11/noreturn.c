#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#ifndef _WIN32
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

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
_Noreturn _Noreturn static void diverge_dup(void) {
	longjmp(jb, 3);
}
static inline _Noreturn void diverge_inline(void) {
	longjmp(jb, 11);
}
_Noreturn static int diverge_int(void) {
	longjmp(jb, 5);
}
__attribute__((noreturn)) static void diverge_attr(void) {
	longjmp(jb, 8);
}
static int drive(void (*fp)(void)) {
	volatile int reached = 0;
	if (setjmp(jb) == 0) {
		fp();
		reached = 1;
	}
	return !reached;
}

static int drive_int(int (*fp)(void)) {
	volatile int reached = 0;
	if (setjmp(jb) == 0) {
		(void)fp();
		reached = 1;
	}
	return !reached;
}

#ifndef _WIN32
_Noreturn static void call_exit(void) {
	exit(42);
}
_Noreturn static void call_abort(void) {
	abort();
}

static int child_exit_code(void (*fp)(void)) {
	pid_t p = fork();
	if (p == 0) {
		fp();
		_exit(0);
	}
	int st = 0;
	while (waitpid(p, &st, 0) < 0)
		;
	if (WIFEXITED(st))
		return WEXITSTATUS(st);
	return -1;
}

static int child_signal(void (*fp)(void)) {
	pid_t p = fork();
	if (p == 0) {
		fp();
		_exit(0);
	}
	int st = 0;
	while (waitpid(p, &st, 0) < 0)
		;
	if (WIFSIGNALED(st))
		return WTERMSIG(st);
	return -1;
}
#endif

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

	ok &= drive(diverge_dup);
	ok &= drive(diverge_inline);
	ok &= drive(diverge_attr);
	ok &= drive_int(diverge_int);

	{
		void (*fp)(void) = diverge_macro;
		ok &= drive(fp);
	}

#ifndef _WIN32
	ok &= (child_exit_code(call_exit) == 42);
	ok &= (child_signal(call_abort) == SIGABRT);
#endif

	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
