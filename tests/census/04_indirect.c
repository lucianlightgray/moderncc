static int f1(int x) { return x + 1; }
static int f2(int x) { return x * 2; }
static int f3(int x) { return x - 5; }
int dispatch(int sel, int x) {
	int (*fp)(int) = sel > 0 ? f1 : (sel < 0 ? f2 : f3);
	return fp(x);
}
int table(int i, int x) {
	static int (*const t[3])(int) = { f1, f2, f3 };
	return t[i % 3](x) + t[(i + 1) % 3](x);
}
int via_arg(int (*g)(int), int x) { return g(x) + g(x + 1); }
