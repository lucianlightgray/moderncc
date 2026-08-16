extern int ext_a(int);
extern int ext_b(int, int);
extern void ext_v(int);
int use_ext(int x, int y) {
	int a = ext_a(x);
	int b = ext_b(x, y);
	if (a < b) {
		ext_v(a);
		return ext_a(b);
	}
	return ext_b(b, a);
}
int ext_chain(int x) { return ext_a(ext_b(x, ext_a(x))); }
