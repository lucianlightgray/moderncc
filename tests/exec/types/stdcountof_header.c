extern int printf(const char *, ...);

#include <stdcountof.h>

static int a[3];
static int b[countof a];
static char c[countof(a)];
static int d[2][5];

#define str(x) #x
#define xstr(x) str(x)

int main(void) {
	int ok = 1;

#ifndef countof
	ok = 0;
#endif
	if (countof a != 3)
		ok = 0;
	if (countof b != 3)
		ok = 0;
	if (sizeof c != 3)
		ok = 0;
	if (countof d != 2)
		ok = 0;
	if (countof d[0] != 5)
		ok = 0;
	if (__builtin_strcmp(xstr(countof), "_Countof") != 0)
		ok = 0;
	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
