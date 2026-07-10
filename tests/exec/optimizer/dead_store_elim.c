extern int printf(const char *, ...);

static long g;

static int bump(int x) {
	g += x;
	return x + 1;
}

static void bumpv(void) {
	g += 1;
}

static void wr(int *p, int v) {
	g += 1;
	*p = v;
}

static int d_two(void) {
	int a = 1;
	a = 2;
	return a;
}

static int d_three(void) {
	int a = 1;
	a = 2;
	a = 3;
	return a;
}

static int d_arith_rhs(void) {
	int a = 4 * 5 + 3;
	a = 10;
	return a;
}

static int d_other_between(void) {
	int a = 1;
	int b = 7;
	a = 2;
	return a + b;
}

static int d_copy_overwritten(void) {
	int a = 5;
	int t = a;
	t = 9;
	return t + a;
}

static int d_addr_of_other(void) {
	int a = 1;
	int b = 2;
	int *p = &b;
	a = 9;
	return a + *p;
}

static int d_chain_diff(void) {
	int a = 1;
	int b = 2;
	a = 3;
	b = 4;
	a = 5;
	b = 6;
	return a + b;
}

static long d_long(void) {
	long a = 1000000L;
	a = 2000000L;
	return a;
}

static unsigned d_uint(void) {
	unsigned a = 0xFFu;
	a = 7u;
	return a;
}

static int d_short(void) {
	short a = 100;
	a = 200;
	return a;
}

static int d_char(void) {
	char a = 65;
	a = 70;
	return a;
}

static int d_bool(void) {
	_Bool a = 0;
	a = 1;
	return a;
}

static int d_pure_binary(void) {
	int k = 3;
	int a = k + 1;
	a = k * 2;
	return a;
}

static int d_convert_rhs(void) {
	long w = 40;
	int a = (int)w;
	a = 12;
	return a;
}

static int d_ternary_rhs(void) {
	int c = 1;
	int a = c ? 5 : 6;
	a = 8;
	return a;
}

static int d_many_locals(void) {
	int a = 1, b = 2, c = 3;
	a = 10;
	b = 20;
	c = 30;
	return a + b + c;
}

static int d_reassign_twice_use(void) {
	int a = 1;
	a = 2;
	a = 3;
	return a + 100;
}

static int nf_rhs_call(void) {
	int a = bump(3);
	a = 5;
	return a;
}

static int nf_rhs_call2(void) {
	int a = 1;
	int b = bump(4);
	b = 9;
	return a + b;
}

static int cp_read_between(void) {
	int a = 1;
	int b = a;
	a = 2;
	return a + b;
}

static int cp_self_use(void) {
	int a = 5;
	a = a + 1;
	return a;
}

static volatile int vsink;

static int nf_volatile(void) {
	volatile int a = 1;
	a = 2;
	return a + vsink;
}

static int nf_addr_taken(void) {
	int a = 1;
	int *p = &a;
	a = 2;
	return *p;
}

static int nf_call_stmt_between(void) {
	int a = 1;
	bumpv();
	a = 2;
	return a;
}

static int nf_store_through_ptr(void) {
	int a = 1;
	int b = 0;
	wr(&b, 7);
	a = 2;
	return a + b;
}

static int nf_branch_between(void) {
	int a = 1;
	if (g > 100000000L)
		return a;
	a = 2;
	return a;
}

static int nf_inc_rhs(void) {
	int k = 4;
	int a = k++;
	a = 9;
	return a + k;
}

static int cp_read_then_dead(void) {
	int a = 5;
	int u = a;
	a = 6;
	int b = a;
	b = 7;
	return u + a + b;
}

static int nf_label_between(void) {
	int a = 1;
	int i = 0;
again:
	a = 2;
	if (i < 1) {
		i = i + 1;
		g += 0;
		goto again;
	}
	return a;
}

static int mix_pending_survives_block(void) {
	int a = 1;
	if (g > 100000000L)
		a = 3;
	return a;
}

static int mix_dead_then_call_keeps(void) {
	int a = 1;
	a = 2;
	int b = bump(0);
	return a + b;
}

static int mix_double_dead(void) {
	int a = 1;
	int b = 2;
	a = 3;
	a = 4;
	b = 5;
	b = 6;
	return a + b;
}

static int mix_dead_copy_and_const(void) {
	int a = 10;
	int t = a;
	t = 3;
	a = 4;
	return t + a;
}

static int mix_scoped(void) {
	int r = 0;
	{
		int a = 1;
		a = 5;
		r += a;
	}
	{
		int a = 2;
		a = 6;
		r += a;
	}
	return r;
}

static int mix_wide_narrow(void) {
	long a = 7;
	a = 8;
	int b = 3;
	b = 4;
	return (int)a + b;
}

static int mix_pure_conv_chain(void) {
	int a = 1;
	a = 2;
	long w = a;
	int c = (int)w;
	c = 9;
	return c;
}

int main(void) {
	unsigned long long chk = 0;
	chk = chk * 31 + d_two();
	chk = chk * 31 + d_three();
	chk = chk * 31 + d_arith_rhs();
	chk = chk * 31 + d_other_between();
	chk = chk * 31 + d_copy_overwritten();
	chk = chk * 31 + d_addr_of_other();
	chk = chk * 31 + d_chain_diff();
	chk = chk * 31 + (int)d_long();
	chk = chk * 31 + (int)d_uint();
	chk = chk * 31 + d_short();
	chk = chk * 31 + d_char();
	chk = chk * 31 + d_bool();
	chk = chk * 31 + d_pure_binary();
	chk = chk * 31 + d_convert_rhs();
	chk = chk * 31 + d_ternary_rhs();
	chk = chk * 31 + d_many_locals();
	chk = chk * 31 + d_reassign_twice_use();
	chk = chk * 31 + nf_rhs_call();
	chk = chk * 31 + nf_rhs_call2();
	chk = chk * 31 + cp_read_between();
	chk = chk * 31 + cp_self_use();
	chk = chk * 31 + nf_volatile();
	chk = chk * 31 + nf_addr_taken();
	chk = chk * 31 + nf_call_stmt_between();
	chk = chk * 31 + nf_store_through_ptr();
	chk = chk * 31 + nf_branch_between();
	chk = chk * 31 + nf_inc_rhs();
	chk = chk * 31 + cp_read_then_dead();
	chk = chk * 31 + nf_label_between();
	chk = chk * 31 + mix_pending_survives_block();
	chk = chk * 31 + mix_dead_then_call_keeps();
	chk = chk * 31 + mix_double_dead();
	chk = chk * 31 + mix_dead_copy_and_const();
	chk = chk * 31 + mix_scoped();
	chk = chk * 31 + mix_wide_narrow();
	chk = chk * 31 + mix_pure_conv_chain();
	printf("chk=%llu g=%ld\n", chk, g);
	printf("OK\n");
	return 0;
}
