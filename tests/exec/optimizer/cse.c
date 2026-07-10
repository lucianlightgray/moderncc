extern int printf(const char *, ...);

static long g;

static int sfx(int x) {
	g += x;
	return x + 1;
}

static void wr(int *p, int v) {
	g += 1;
	*p = v;
}

/* Eliminable: repeated pure subexpression in one basic block. */

static int c_basic(int a, int b, int c) {
	int t = a * b + c;
	int u = a * b + c;
	return t + u;
}

static int c_nested(int a, int b) {
	int t = (a + b) * (a + b);
	int u = (a + b) * (a + b);
	return t + u;
}

static int c_partial(int a, int b, int c) {
	int t = a * b;
	int u = a * b + c;
	return t + u;
}

static int c_reuse3(int a, int b) {
	int p = a ^ b;
	int q = a ^ b;
	int r = a ^ b;
	return p + q + r;
}

static int c_shift(int a, int b) {
	int t = (a << 3) - b;
	int u = (a << 3) - b;
	return t + u;
}

static int c_cmp(int a, int b) {
	int t = (a * b) > 100;
	int u = (a * b) > 100;
	return t + u;
}

static int c_conv(int a, int b) {
	long t = (long)a * (long)b + a;
	long u = (long)a * (long)b + a;
	return (int)(t + u);
}

static unsigned c_uint(unsigned a, unsigned b) {
	unsigned t = a * b + 1u;
	unsigned u = a * b + 1u;
	return t + u;
}

static long c_long(long a, long b) {
	long t = a * b - a;
	long u = a * b - a;
	return t + u;
}

static int c_use_in_call(int a, int b) {
	int t = a * b + 7;
	int r = sfx(a * b + 7);
	return t + r;
}

static int c_chain(int a, int b, int c) {
	int t = a * b + c;
	int u = a * b + c;
	int w = a * b + c;
	return t + u + w;
}

/* Must-NOT eliminate: intervening store / call / branch / volatile / alias. */

static int nf_call_between(int a, int b) {
	int t = a * b;
	int r = sfx(1);
	int u = a * b;
	return t + u + r;
}

static int nf_store_operand(int a, int b) {
	int t = a * b;
	a = 5;
	int u = a * b;
	return t + u;
}

static int nf_store_target(int a, int b) {
	int t = a * b;
	t = 9;
	int u = a * b;
	return t + u;
}

static int nf_branch(int a, int b) {
	int t = a * b;
	if (g > 1000000000L)
		return t;
	int u = a * b;
	return t + u;
}

static volatile int vs;

static int nf_volatile(int a, int b) {
	int t = a * b + vs;
	int u = a * b + vs;
	return t + u;
}

static int nf_self_read(int a, int b) {
	int x = a + b;
	x = x * 2;
	int y = x * 2;
	return x + y;
}

static int nf_addr_operand(int a, int b) {
	int *p = &a;
	int t = a * b;
	wr(p, 5);
	int u = a * b;
	return t + u;
}

static int nf_store_ptr(int a, int b) {
	int s = 0;
	int t = a * b;
	wr(&s, 7);
	int u = a * b;
	return t + u + s;
}

int main(void) {
	unsigned long long chk = 0;
	chk = chk * 31 + c_basic(3, 4, 5);
	chk = chk * 31 + c_nested(2, 3);
	chk = chk * 31 + c_partial(3, 4, 5);
	chk = chk * 31 + c_reuse3(3, 4);
	chk = chk * 31 + c_shift(3, 4);
	chk = chk * 31 + c_cmp(30, 40);
	chk = chk * 31 + c_conv(3, 4);
	chk = chk * 31 + (int)c_uint(3, 4);
	chk = chk * 31 + (int)c_long(3, 4);
	chk = chk * 31 + c_use_in_call(3, 4);
	chk = chk * 31 + c_chain(3, 4, 5);
	chk = chk * 31 + nf_call_between(3, 4);
	chk = chk * 31 + nf_store_operand(3, 4);
	chk = chk * 31 + nf_store_target(3, 4);
	chk = chk * 31 + nf_branch(3, 4);
	chk = chk * 31 + nf_volatile(3, 4);
	chk = chk * 31 + nf_self_read(3, 4);
	chk = chk * 31 + nf_addr_operand(3, 4);
	chk = chk * 31 + nf_store_ptr(3, 4);
	printf("chk=%llu g=%ld\n", chk, g);
	printf("OK\n");
	return 0;
}
