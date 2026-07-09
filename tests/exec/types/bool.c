#include <stdio.h>
#include <stdbool.h>

struct flags {
	bool a : 1, b : 1, c : 1;
};

int main(void) {

	bool b1 = 5;
	bool b2 = -3;
	bool b3 = 0;
	bool b4 = 256;
	printf("convert: %d %d %d %d\n", b1, b2, b3, b4);

	bool fb = 0.1;
	bool fz = 0.0;
	int dummy;
	bool pb = (&dummy != 0);
	bool pz = ((void *)0 != 0);
	printf("float/ptr: %d %d %d %d\n", fb, fz, pb, pz);

	printf("macros: %d %d sum=%d\n", true, false, true + true + true);

	const char *names[2] = {"no", "yes"};
	printf("index: %s %s\n", names[(bool)0], names[(bool)42]);

	struct flags f = {.a = 7, .b = 0, .c = 1};
	printf("size=%d bitfields: %d %d %d\n",
				 (int)sizeof(bool), f.a, f.b, f.c);

	bool t = false;
	t++;
	t++;
	printf("incr: %d\n", t);
	return 0;
}
