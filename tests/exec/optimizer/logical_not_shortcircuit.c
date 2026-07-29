extern int printf(const char *, ...);
static int calls;
static int side(int v) { calls++; return v; }
static int c1(int a, int b) { return !(a && b); }
static int c5(int a, int b) { return !(a || b); }
static int c8(int a, int b) { return !(side(a) && side(b)); }
static int c9(int a, int b, int c) { return !(a && b && c); }
static int c10(int a, int b) { return !!(a && b); }
static int c11(int a, int b) { if (!(a && b)) return 7; return 9; }
int main(void) {
	int i, j, s = 0;
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
			s = s * 3 + c1(i, j) + 2 * c5(i, j) + 4 * c9(i, j, 1) + 8 * c10(i, j) + c11(i, j);
	calls = 0;
	s += c8(0, 5) * 100 + calls * 1000;
	printf("s=%d calls=%d\n", s, calls);
	return 0;
}
