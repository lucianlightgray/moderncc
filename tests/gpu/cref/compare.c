/* Fixture for slice/cref-oracle. Every function here is written so that mcc's
 * slice recogniser accepts its body as an expression slice: locals only, no
 * calls, no loads through pointers, integer types throughout. The expected
 * values were produced by the oracle, not by hand, so a wrong constant cannot
 * quietly become the thing under test.
 *
 * Exit status is the verdict: 0 passes, the failing check number otherwise. */

static int cmps(int a, int b) {
	int lt = a < b;
	int ge = a >= b;
	int eq = a == b;
	return lt * 4 + ge * 2 + eq;
}

static int cmpu(unsigned int a, unsigned int b) {
	int lt = a < b;
	int gt = a > b;
	int ne = a != b;
	return lt * 4 + gt * 2 + ne;
}

int main(void) {
	if ((int)(cmps(-3, 4)) != 4)
		return 1;
	if ((int)(cmps(4, 4)) != 3)
		return 2;
	if ((int)(cmps(9, -9)) != 2)
		return 3;
	if ((int)(cmpu(1u, 4000000000u)) != 5)
		return 4;
	if ((int)(cmpu(4000000000u, 1u)) != 3)
		return 5;
	if ((int)(cmpu(7u, 7u)) != 0)
		return 6;
	return 0;
}
