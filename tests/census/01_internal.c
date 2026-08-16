static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }
static int sub(int a, int b) { return a - b; }
int combo(int x, int y) {
	int s = add(x, y);
	int p = mul(x, y);
	if (s > p)
		return add(s, sub(p, x));
	return mul(sub(s, y), p);
}
int chain(int x) { return add(mul(x, x), add(x, sub(x, 1))); }
int ladder(int x, int y, int z) {
	int a = add(x, y);
	int b = mul(a, z);
	int c = sub(b, a);
	return add(add(a, b), c);
}
