/* Fixture for slice/cref-oracle. Every function here is written so that mcc's
 * slice recogniser accepts its body as an expression slice: locals only, no
 * calls, no loads through pointers, integer types throughout. The expected
 * values were produced by the oracle, not by hand, so a wrong constant cannot
 * quietly become the thing under test.
 *
 * Exit status is the verdict: 0 passes, the failing check number otherwise. */

static int sdiv(int a, int b) {
	int q = a / b;
	int r = a % b;
	return q * 3 + r;
}

static unsigned int udiv(unsigned int a, unsigned int b) {
	unsigned int q = a / b;
	unsigned int r = a % b;
	return q + r * 3u;
}

int main(void) {
	if ((int)(sdiv(-100, 7)) != -44)
		return 1;
	if ((int)(sdiv(100, -7)) != -40)
		return 2;
	if ((int)(sdiv(0, 3)) != 0)
		return 3;
	if ((unsigned int)(udiv(4000000000u, 7u)) != 571428580u)
		return 4;
	if ((unsigned int)(udiv(1u, 4000000000u)) != 3u)
		return 5;
	if ((unsigned int)(udiv(123456789u, 1000u)) != 125823u)
		return 6;
	return 0;
}
