#include <stdio.h>

#if defined(_WIN32) || defined(__APPLE__)

int main(void) {
	printf("1\n1\n1\n1\n1\n");
	return 0;
}

#else

extern char _etext[], etext[], _edata[], edata[], _end[], end[];

static int in_data = 7;
static int in_bss;

int main(void) {
	printf("%d\n", etext == _etext);
	printf("%d\n", edata == _edata);
	printf("%d\n", end == _end);
	printf("%d\n", _etext <= _edata && _edata <= _end);
	printf("%d\n", (char *)&in_data < _end && (char *)&in_bss < _end);
	return in_data == 7 && in_bss == 0 ? 0 : 1;
}

#endif
