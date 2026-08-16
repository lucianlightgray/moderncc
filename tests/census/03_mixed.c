static int loc(int x) { return x + 7; }
static int loc2(int x) { return x ^ 3; }
extern int rem(int);
extern int rem2(int, int);
int mixed(int x) { return loc(x) + rem(x); }
int mixed_branch(int x, int y) {
	int a = loc(x);
	int b = rem(y);
	if (a > b)
		return loc2(a) + rem2(b, a);
	return loc(b) - rem(a);
}
