#include <stdio.h>
/* auto-reduced miscompile repro: seed=10 attributed=-O2 (default gates) */
union U { unsigned long w; unsigned int h[2]; };
static union U u = {0x1122334455667788UL};
static unsigned long v0 = 14480184UL;
static unsigned long v1 = 2496765UL;
static unsigned long v2 = 15680252UL;
static unsigned long v3 = 12382106UL;
static unsigned long v4 = 15827531UL;

static unsigned long f0(unsigned long a, unsigned long b, int depth) {
	unsigned long acc = a ^ (b * 3UL);
	for (unsigned long i = 0; i < 6UL; i++) {
		acc += b;
	}
	acc += u.h[a & 1UL] ^ u.w;
	return acc;
}

static unsigned long f1(unsigned long a, unsigned long b, int depth) {
	unsigned long acc = a ^ (b * 3UL);
	if (depth < 5) acc += f0((~((a) % ((v1 & 0xffffUL) + 1UL))), 31U, depth + 1);
	return acc;
}

static unsigned long f2(unsigned long a, unsigned long b, int depth) {
	unsigned long acc = a ^ (b * 3UL);
	acc += ((2329724861UL - (136U == v4 ? b : v0)) > ((((v2) / ((a & 0xffffUL) + 1UL))) / ((v2 & 0xffffUL) + 1UL)) ? (~106U) : 93U);
	for (unsigned long i = 0; i < 4UL; i++) {
		v4 = ((unsigned long)((((b ^ 173U)) % (((v1 + v3) & 0xffffUL) + 1UL))) << ((v4) & 63UL));
	}
	v4 = (((unsigned long)((unsigned)((a >= 1122586491UL ? a : b)) % 9U)) + (~b));
	for (unsigned long i = 0; i < 3UL; i++) {
		if (((unsigned long)((unsigned)(v0) % 3597U)) & 1UL) {
			acc += b;
		} else {
			if (depth < 5) acc += f0(2198119948UL, ((a != a ? 172U : 16U) * v3), depth + 1);
		}
	}
	acc += u.h[a & 1UL] ^ u.w;
	return acc;
}

int main(void) {
	unsigned long acc = 0xcafef00dUL;
	acc = acc * 1000003UL + f1(acc, 7804387UL, 0);
	acc = acc * 1000003UL + f2(acc, 7022025UL, 0);
	acc = acc * 1000003UL + f0(acc, 1786403UL, 0);
	return (int)(acc & 0x7fUL);
}
