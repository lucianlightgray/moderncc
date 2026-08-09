/* Fixture for slice/cref-oracle. Every function here is written so that mcc's
 * slice recogniser accepts its body as an expression slice: locals only, no
 * calls, no loads through pointers, integer types throughout. The expected
 * values were produced by the oracle, not by hand, so a wrong constant cannot
 * quietly become the thing under test.
 *
 * Exit status is the verdict: 0 passes, the failing check number otherwise. */

static int chars(signed char a, signed char b) {
	int t = a + b;
	int u = a * b;
	return t + u - (a - b);
}

static int shorts(short a, short b) {
	int t = a * 3 + b;
	int u = (short)(t + a);
	return t - u;
}

static int converts(long long a) {
	int t = (int)a;
	short u = (short)t;
	signed char v = (signed char)u;
	return t + u + v;
}

int main(void) {
	if ((int)(chars(100, -100)) != -10200)
		return 1;
	if ((int)(chars(-128, 127)) != -16002)
		return 2;
	if ((int)(chars(0, 0)) != 0)
		return 3;
	if ((int)(shorts(30000, -30000)) != 35536)
		return 4;
	if ((int)(shorts(-1, 1)) != 1)
		return 5;
	if ((int)(converts(1234567890123LL)) != 1912277345)
		return 6;
	if ((int)(converts(-1LL)) != -3)
		return 7;
	return 0;
}
