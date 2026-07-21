#include <stdio.h>
/* auto-reduced miscompile repro: seed=6 attributed=?  */
static unsigned long v0 = 10741356UL;
static unsigned long v1 = 3294721UL;
static unsigned long v2 = 15593137UL;

static unsigned long f0(unsigned long a, unsigned long b, int depth) {
	unsigned long acc = a ^ (b * 3UL);
	for (unsigned long i = 0; i < 4UL; i++) {
		v1 = (v1 - ((unsigned long)((long)(v2 & a) >> 1)));
	}
	return acc;
}

static unsigned long f1(unsigned long a, unsigned long b, int depth) {
	unsigned long acc = a ^ (b * 3UL);
	for (unsigned long i = 0; i < 3UL; i++) {
		acc ^= v1;
	}
	v2 = ((unsigned long)((long)((v1 != 285182792UL ? v1 : v1) | ((b) / ((a & 0xffffUL) + 1UL))) >> 1));
	v0 = v1;
	for (unsigned long i = 0; i < 2UL; i++) {
		v2 = (v1 <= 33U ? b : 2477454392UL);
	}
	if (depth < 5) acc += f0(((unsigned long)((unsigned)((v0 >= 3415042698UL ? 142U : b)) % 255U)), ((unsigned long)((long)(b & 14U) >> 1)), depth + 1);
	return acc;
}

int main(void) {
	unsigned long acc = 0xcafef00dUL;
	acc = acc * 1000003UL + f1(acc, 2397687UL, 0);
	acc = acc * 1000003UL + f0(acc, 8407757UL, 0);
	acc = acc * 1000003UL + f1(acc, 5232052UL, 0);
	acc = acc * 1000003UL + f0(acc, 8480541UL, 0);
	printf("%lu\n", acc);
	return (int)(acc & 0x7fUL);
}
