/* Fixture for slice/cref-oracle. Every function here is written so that mcc's
 * slice recogniser accepts its body as an expression slice: locals only, no
 * calls, no loads through pointers, integer types throughout. The expected
 * values were produced by the oracle, not by hand, so a wrong constant cannot
 * quietly become the thing under test.
 *
 * Exit status is the verdict: 0 passes, the failing check number otherwise. */

static int pick(int a, int b, int c) {
	int t = a > b ? a - b : b - a;
	int u = c ? t * 2 : t + 1;
	return (u < 100 ? u : 100) + (a ? 1 : 0);
}

static long long nest(long long a, long long b) {
	long long t = a < 0 ? -a : a;
	long long u = b < 0 ? -b : b;
	return t > u ? t - u : u - t;
}

int main(void) {
	if ((int)(pick(5, 2, 1)) != 7)
		return 1;
	if ((int)(pick(-5, 2, 0)) != 9)
		return 2;
	if ((int)(pick(0, 0, 0)) != 1)
		return 3;
	if ((long long)(nest(-9000000000LL, 5LL)) != 8999999995LL)
		return 4;
	if ((long long)(nest(3LL, -3LL)) != 0LL)
		return 5;
	if ((long long)(nest(0LL, 0LL)) != 0LL)
		return 6;
	return 0;
}
