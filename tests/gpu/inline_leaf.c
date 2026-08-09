static int mix3(int a, int b) { return a * 3 + b; }

static int clamp_lo(int v) { return v < 0 ? 0 : v; }

int slice_inline_leaf(int x, int y) {
	int p;
	int q;
	int r;
	p = mix3(x, y);
	q = mix3(y, x) + 7;
	r = clamp_lo(p - q);
	return p + q + r;
}

int slice_inline_discard(int x) {
	int p;
	p = x + 1;
	mix3(x, x);
	p = p * 2;
	return p;
}
