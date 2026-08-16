#include <stdio.h>
static int arith(int n) {
	static void *const tab[] = { &&L0, &&L1, &&L2, &&L3 };
	void *const *p = tab + (n & 3);
	int r = 0;
	goto **p;
L0: r = 10; goto done;
L1: r = 20; goto done;
L2: r = 30; goto done;
L3: r = 40; goto done;
done:
	return r;
}
int main(void) {
	int fail = 0;
	if (arith(0) != 10 || arith(1) != 20 || arith(2) != 30 || arith(3) != 40) fail = 1;
	if (arith(5) != 20 || arith(6) != 30) fail = 1;
	puts(fail ? "FAIL" : "OK");
	return fail;
}
