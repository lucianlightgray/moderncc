extern int printf(const char *, ...);

static long fx;

static int sink(int x) {
	fx += x;
	return x + 1;
}

static void mutate(int *p) {
	fx += 1;
	*p = *p * 3 + 7;
}

static void store_through(int *p, int v) {
	fx += 1;
	*p = v;
}

static int f_simple(void) {
	int a = 5;
	return a;
}

static int f_copy(void) {
	int a = 3;
	int b = a;
	return b;
}

static int f_chain(void) {
	int a = 7;
	return a + a + a;
}

static int f_arith(void) {
	int a = 4;
	return a * 10 + a;
}

static int f_two_consts(void) {
	int a = 6;
	int b = 9;
	return a * b;
}

static int f_store_other_between(void) {
	int a = 2;
	int b = 100;
	int c = a + b;
	return c;
}

static int f_reassign(void) {
	int a = 1;
	a = 9;
	return a;
}

static int f_reassign_use(void) {
	int a = 1;
	a = 40;
	return a + 2;
}

static int f_ternary_val(void) {
	int a = 5;
	return a > 3 ? a : 0;
}

static int f_ternary_both(void) {
	int a = 8;
	int b = 3;
	return (a & 1) ? a : b;
}

static int f_landor(void) {
	int a = 1;
	int b = 0;
	return (a && b) || a;
}

static int f_shift(void) {
	int a = 3;
	return a << 4;
}

static int f_selfadd(void) {
	int a = 5;
	a = a + 1;
	return a;
}

static int f_cmp(void) {
	int a = 10;
	return a == 10;
}

static long f_long(void) {
	long a = 1000000L;
	return a + a;
}

static int f_short(void) {
	short a = 200;
	return a + 1;
}

static int f_char(void) {
	char a = 65;
	return a + 1;
}

static unsigned f_unsigned(void) {
	unsigned a = 0xF0u;
	return a | 0x0Fu;
}

static int f_bool(void) {
	_Bool a = 1;
	return a + a;
}

static int f_many(void) {
	int a = 1, b = 2, c = 3, d = 4, e = 5;
	return a + b * c - d + e;
}

static int f_scoped_shadow(void) {
	int r = 0;
	{
		int a = 11;
		r += a;
	}
	{
		int a = 22;
		r += a;
	}
	return r;
}

static int f_two_returns(void) {
	int a = 3;
	if (a > 100)
		return a;
	return a + a;
}

static int nf_call_between(void) {
	int a = 5;
	sink(1);
	return a;
}

static int nf_reassign_from_call(void) {
	int a = 5;
	a = sink(10);
	return a;
}

static int nf_volatile(void) {
	volatile int a = 5;
	return a;
}

static int nf_addr_mutated(void) {
	int a = 5;
	mutate(&a);
	return a;
}

static int nf_addr_taken_read(void) {
	int a = 12;
	int *p = &a;
	return *p + a;
}

static int nf_inc_stmt(void) {
	int a = 5;
	a++;
	return a;
}

static int nf_dec_stmt(void) {
	int a = 5;
	--a;
	return a;
}

static int nf_inc_in_expr(void) {
	int a = 5;
	int b = a++;
	return b + a;
}

static int nf_store_through_ptr(void) {
	int a = 5;
	int b = 0;
	store_through(&b, 99);
	return a + b;
}

static int nf_pointer_local(void) {
	int x = 3;
	int *p = &x;
	*p = 44;
	return x;
}

static int nf_reassigned_before_use(void) {
	int a = 5;
	a = sink(0) - 1;
	int c = a + 1;
	return c;
}

static int nf_float_local(void) {
	double a = 2.5;
	double b = a;
	return (int)((b + a) * 2.0);
}

static int nf_alias_write(void) {
	int a = 5;
	int *p = &a;
	sink(*p);
	*p = 17;
	return a;
}

static int nf_addr_then_const(void) {
	int a = 5;
	int *p = &a;
	*p = 8;
	int q = a;
	return q;
}

static int mix_reset(void) {
	int a = 5;
	int b = a;
	sink(b);
	int c = a;
	return b + c;
}

static int mix_cascade(void) {
	int a = 2;
	int b = a * 3;
	int c = b;
	return a + c;
}

static int mix_loop_body(void) {
	int total = 0;
	int i = 0;
	while (i < 4) {
		int k = 10;
		total += k;
		i = i + 1;
	}
	return total;
}

static int mix_if_arms(void) {
	int x = 3;
	int r;
	if (x > 0) {
		int a = 100;
		r = a + x;
	} else {
		int a = 200;
		r = a;
	}
	return r;
}

static int mix_negate(void) {
	int a = 9;
	int b = -a;
	return b;
}

static int mix_not(void) {
	int a = 0;
	int b = !a;
	return b;
}

static int mix_complement(void) {
	int a = 0;
	int b = ~a;
	return b;
}

static int mix_conv(void) {
	int a = 300;
	char c = (char)a;
	return c;
}

static int mix_used_twice_call(void) {
	int a = 7;
	int x = a;
	int y = sink(a);
	int z = a;
	return x + y + z;
}

int main(void) {
	unsigned long long chk = 0;
	chk = chk * 31 + f_simple();
	chk = chk * 31 + f_copy();
	chk = chk * 31 + f_chain();
	chk = chk * 31 + f_arith();
	chk = chk * 31 + f_two_consts();
	chk = chk * 31 + f_store_other_between();
	chk = chk * 31 + f_reassign();
	chk = chk * 31 + f_reassign_use();
	chk = chk * 31 + f_ternary_val();
	chk = chk * 31 + f_ternary_both();
	chk = chk * 31 + f_landor();
	chk = chk * 31 + f_shift();
	chk = chk * 31 + f_selfadd();
	chk = chk * 31 + f_cmp();
	chk = chk * 31 + f_long();
	chk = chk * 31 + f_short();
	chk = chk * 31 + f_char();
	chk = chk * 31 + (int)f_unsigned();
	chk = chk * 31 + f_bool();
	chk = chk * 31 + f_many();
	chk = chk * 31 + f_scoped_shadow();
	chk = chk * 31 + f_two_returns();
	chk = chk * 31 + nf_call_between();
	chk = chk * 31 + nf_reassign_from_call();
	chk = chk * 31 + nf_volatile();
	chk = chk * 31 + nf_addr_mutated();
	chk = chk * 31 + nf_addr_taken_read();
	chk = chk * 31 + nf_inc_stmt();
	chk = chk * 31 + nf_dec_stmt();
	chk = chk * 31 + nf_inc_in_expr();
	chk = chk * 31 + nf_store_through_ptr();
	chk = chk * 31 + nf_pointer_local();
	chk = chk * 31 + nf_reassigned_before_use();
	chk = chk * 31 + nf_float_local();
	chk = chk * 31 + nf_alias_write();
	chk = chk * 31 + nf_addr_then_const();
	chk = chk * 31 + mix_reset();
	chk = chk * 31 + mix_cascade();
	chk = chk * 31 + mix_loop_body();
	chk = chk * 31 + mix_if_arms();
	chk = chk * 31 + mix_negate();
	chk = chk * 31 + mix_not();
	chk = chk * 31 + mix_complement();
	chk = chk * 31 + mix_conv();
	chk = chk * 31 + mix_used_twice_call();
	printf("chk=%llu fx=%ld\n", chk, fx);
	printf("OK\n");
	return 0;
}
