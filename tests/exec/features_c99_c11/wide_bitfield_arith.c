#include <stdio.h>

struct s {
	unsigned long long u33 : 33;
	unsigned long long u40 : 40;
	unsigned long long u41 : 41;
};

struct foo {
	unsigned long long b : 40;
};

static struct s a = {0x100000, 0x100000, 0x100000};
static struct s b = {0x100000000ULL, 0x100000000ULL, 0x100000000ULL};

int main(void) {
	int fail = 0;

	if (a.u33 * a.u33 != 0 || a.u40 * a.u40 != 0 || a.u33 * a.u40 != 0)
		fail = 1;
	if (a.u33 * a.u41 != 0x10000000000ULL || a.u41 * a.u33 != 0x10000000000ULL)
		fail = 1;
	if (a.u41 * a.u41 != 0x10000000000ULL)
		fail = 1;

	if (b.u33 + b.u33 != 0)
		fail = 1;
	if (b.u33 + b.u40 != 0x200000000ULL || b.u40 + b.u33 != 0x200000000ULL)
		fail = 1;

	struct foo x;
	x.b = 0x0100;
	if ((x.b << 32) != 0)
		fail = 1;
	x.b = 0x0100000001ULL;
	if (((x.b << 8) + (x.b >> 32)) != 0x101ULL)
		fail = 1;
	x.b = 0x0100000000ULL;
	if (((x.b << 8) + (x.b >> 32)) != 0x1ULL)
		fail = 1;

	puts(fail ? "FAIL" : "OK");
	return fail;
}
