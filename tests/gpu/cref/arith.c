/* Fixture for slice/cref-oracle. Every function here is written so that mcc's
 * slice recogniser accepts its body as an expression slice: locals only, no
 * calls, no loads through pointers, integer types throughout. The expected
 * values were produced by the oracle, not by hand, so a wrong constant cannot
 * quietly become the thing under test.
 *
 * Exit status is the verdict: 0 passes, the failing check number otherwise. */

static int mix(int a, int b, int c) {
	int t = a * b + c - (a ^ b);
	int u = t & 0xff;
	int v = (t | c) - (u * 3);
	return v + t - u;
}

static long long wide(long long a, long long b) {
	long long t = a * 3 + b;
	long long u = (t ^ b) - (a & b);
	return t + u - (a | b);
}

int main(void) {
	if ((int)(mix(3, 5, 7)) != -25)
		return 1;
	if ((int)(mix(-4, 9, 2)) != -976)
		return 2;
	if ((int)(mix(0, 0, 0)) != 0)
		return 3;
	if ((long long)(wide(1000000LL, 7LL)) != 5000000LL)
		return 4;
	if ((long long)(wide(-5LL, 11LL)) != -19LL)
		return 5;
	if ((long long)(wide(0LL, 0LL)) != 0LL)
		return 6;
	return 0;
}
