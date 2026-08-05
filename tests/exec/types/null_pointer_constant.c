extern int printf(const char *, ...);
#include <stddef.h>

static void take(void *p, int *q) {
	(void)p;
	(void)q;
}

int main(void) {
	int ok = 1;
	void *a = (char)0;
	void *b = (short)0;
	void *c = (long)0;
	int *d = (_Bool)0;
	void *e = '\0';
	void *f = (char)0 ? (void *)0 : (char)0;

	take((char)0, (short)0);
	if (a || b || c || d || e || f)
		ok = 0;
	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
