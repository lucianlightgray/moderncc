/* Fixture for slice/cref-oracle. Every function here is written so that mcc's
 * slice recogniser accepts its body as an expression slice: locals only, no
 * calls, no loads through pointers, integer types throughout. The expected
 * values were produced by the oracle, not by hand, so a wrong constant cannot
 * quietly become the thing under test.
 *
 * Exit status is the verdict: 0 passes, the failing check number otherwise. */

static int both(int a, int b, int c) {
	int t = a && b;
	int u = a || c;
	int v = (a && b) || (c && a);
	return t * 4 + u * 2 + v;
}

static int notted(int a, int b) {
	int t = !a;
	int u = ~b;
	int v = !(a && b);
	return t + u + v;
}

int main(void) {
	if ((int)(both(1, 0, 1)) != 3)
		return 1;
	if ((int)(both(0, 1, 1)) != 2)
		return 2;
	if ((int)(both(3, 4, 5)) != 7)
		return 3;
	if ((int)(notted(0, 0)) != 1)
		return 4;
	if ((int)(notted(1, -1)) != 0)
		return 5;
	if ((int)(notted(7, 255)) != -256)
		return 6;
	return 0;
}
