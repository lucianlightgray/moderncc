static int add(int a, int b) {
	return a + b;
}
static int scale(int x, int k) {
	return x * k;
}
static int madd(int a, int b) {
	int p = a * b;
	int q = p + a;
	return q - 1;
}
static int clamp(int x, int lo, int hi) {
	int r = x;
	if (x < lo)
		r = lo;
	if (x > hi)
		r = hi;
	return r;
}
static int sgn(int x) {
	if (x < 0)
		return -1;
	if (x > 0)
		return 1;
	return 0;
}
static double area(double w, double h) {
	return w * h;
}
static int quad(int x) {
	return add(x, x) * 2;
}
static int pick(int k) {
	switch (k) {
	case 1:
		return 3;
	case 2:
		return 5;
	default:
		return 0;
	}
}
static int firsthit(const int *a, int n) {
	for (int i = 0; i < n; i++)
		if (a[i])
			return i;
	return n;
}
struct Pair {
	int a, b;
};
static struct Pair mkpair(int a, int b) {
	struct Pair p;
	p.a = a;
	p.b = b;
	return p;
}
static int sumpt(struct Pair p) {
	return p.a + p.b;
}
struct Big {
	long a, b, c, d;
};
static long sumbig(struct Big b) {
	return b.a + b.b + b.c + b.d;
}
static struct Pair addpt(struct Pair p, struct Pair q) {
	struct Pair r;
	r.a = p.a + q.a;
	r.b = p.b + q.b;
	return r;
}
static int use_sumpt(int x, int y) {
	struct Pair p = {x, y};
	return sumpt(p);
}
static int use_sumbig(long a, long b, long c, long d) {
	struct Big s = {a, b, c, d};
	return (int)sumbig(s);
}
static int use_addpt(int ax, int ay, int bx, int by) {
	struct Pair p = {ax, ay}, q = {bx, by}, r = addpt(p, q);
	return r.a + r.b;
}
static int gsum(int n) {
	int s = 0, i = 0;
loop:
	if (i < n) {
		s += i;
		i++;
		goto loop;
	}
	return s;
}
static int fwd_callee(int x);
static int fwd_sum(int n) {
	int s = 0;
	for (int i = 0; i < n; i++)
		s += fwd_callee(i);
	return s;
}
static int fwd_callee(int x) {
	return x + 1;
}

struct FwdBox {
	int v;
};
static int fwd_dbl(int x);
static int fwd_boxed(int n) {
	struct FwdBox b;
	const char *m = "fwdmsg";
	b.v = 0;
	for (int i = 0; i < n; i++)
		b.v += fwd_dbl(i);
	return b.v + (m[0] == 'f');
}
static int fwd_dbl(int x) {
	return x * 2;
}

int main(void) {
	int r = add(3, 4);
	int s = scale(5, 6);
	int t = add(scale(2, 3), r);
	int u = madd(2, 3);
	int c = clamp(99, 0, 5);
	int g = sgn(-4) + sgn(0) + sgn(9);
	int d = (int)area(2.5, 4.0);
	int q = quad(2);
	int arr2[3] = {0, 0, 7};
	int p = pick(2) + firsthit(arr2, 3);
	struct Pair pr = mkpair(2, 3);
	int m = pr.a + pr.b;
	int gs = gsum(5);
	int (*fp)(int) = fwd_sum;
	int fw = fp(4);
	int (*fb)(int) = fwd_boxed;
	int bx = fb(4);
	int sp = use_sumpt(3, 4);
	int sb = use_sumbig(1, 2, 3, 4);
	int ap = use_addpt(3, 4, 10, 20);
	return r + s + t + u + c + g + d + q + p + m + gs + fw + bx + sp + sb + ap - 137;
}
