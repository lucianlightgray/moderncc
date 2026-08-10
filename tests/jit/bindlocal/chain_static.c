extern int printf(const char *, ...);

static int leaf(int n) {
	int a = n, i;
	for (i = 0; i < 4; i++)
		a = a * 2 + i;
	return a & 0x3ff;
}

static int middle(int n) {
	int a = leaf(n), i;
	for (i = 0; i < 3; i++)
		a = a + leaf(i);
	return a;
}

static inline int shim(int n) {
	int a = middle(n), i;
	for (i = 0; i < 2; i++)
		a = a - leaf(i);
	return a;
}

int hot(int n) {
	int s = 0, i;
	for (i = 0; i < n; i++)
		s += shim(i) + middle(i);
	return s;
}

int main(void) {
	printf("chain_static %d\n", hot(150));
	return 0;
}
