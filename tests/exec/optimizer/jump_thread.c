extern int printf(const char *, ...);

static long g;

static int bump(void) {
	g += 1;
	return 1;
}

static volatile int vflag = 1;

static int fa_then_only(int x) {
	if (x)
		;
	return 7;
}

static int fa_both_empty(int a, int b) {
	if (a < b) {
	} else {
	}
	return 8;
}

static int fa_arith_cond(int a, int b) {
	if (a + b)
		;
	return 9;
}

static int fa_bitand_cond(int a) {
	if (a & 1) {
	} else {
	}
	return 10;
}

static int fa_cmp_else_empty(int a, int b) {
	if (a == b)
		;
	else
		;
	return 11;
}

static int fa_load_cond(int *p) {
	if (*p) {
	}
	return 12;
}

static int fa_nested(int a, int b) {
	if (a) {
		if (b) {
		} else {
		}
	}
	return 13;
}

static int fb_return_const(int c) {
	if (c)
		return 5;
	else
		return 5;
}

static int fb_return_expr(int a, int b) {
	if (a < b)
		return a + b;
	else
		return a + b;
}

static int fb_assign_same(int c) {
	int x;
	if (c)
		x = 7;
	else
		x = 7;
	return x;
}

static int fb_side_effect_once(int c) {
	if (c)
		g += 10;
	else
		g += 10;
	return 14;
}

static int fb_call_once(int c) {
	int r;
	if (c)
		r = bump();
	else
		r = bump();
	return r + 20;
}

static int fb_impure_cond_kept(void) {
	int x;
	if (bump())
		x = 5;
	else
		x = 5;
	return x;
}

static int nf_impure_empty(void) {
	if (bump())
		;
	return 15;
}

static int nf_diff_return(int c) {
	if (c)
		return 1;
	else
		return 2;
}

static int nf_diff_assign(int c) {
	int x;
	if (c)
		x = 1;
	else
		x = 2;
	return x;
}

static int nf_one_arm(int c) {
	int x = 0;
	if (c)
		x = 3;
	else
		;
	return x;
}

static int nf_then_body(int c) {
	int x = 0;
	if (c)
		x = 4;
	return x;
}

static int nf_volatile_cond(void) {
	if (vflag)
		;
	return 16;
}

static int nf_diff_side_effect(int c) {
	if (c)
		g += 1;
	else
		g += 2;
	return 17;
}

int main(void) {
	unsigned long long chk = 0;
	chk = chk * 31 + fa_then_only(1);
	chk = chk * 31 + fa_then_only(0);
	chk = chk * 31 + fa_both_empty(3, 4);
	chk = chk * 31 + fa_arith_cond(2, -2);
	chk = chk * 31 + fa_bitand_cond(7);
	chk = chk * 31 + fa_cmp_else_empty(5, 5);
	int local = 1;
	chk = chk * 31 + fa_load_cond(&local);
	chk = chk * 31 + fa_nested(1, 0);
	chk = chk * 31 + fb_return_const(1);
	chk = chk * 31 + fb_return_const(0);
	chk = chk * 31 + fb_return_expr(2, 9);
	chk = chk * 31 + fb_assign_same(1);
	chk = chk * 31 + fb_assign_same(0);
	chk = chk * 31 + fb_side_effect_once(1);
	chk = chk * 31 + fb_side_effect_once(0);
	chk = chk * 31 + fb_call_once(1);
	chk = chk * 31 + fb_call_once(0);
	chk = chk * 31 + fb_impure_cond_kept();
	chk = chk * 31 + nf_impure_empty();
	chk = chk * 31 + nf_diff_return(1);
	chk = chk * 31 + nf_diff_return(0);
	chk = chk * 31 + nf_diff_assign(1);
	chk = chk * 31 + nf_diff_assign(0);
	chk = chk * 31 + nf_one_arm(1);
	chk = chk * 31 + nf_one_arm(0);
	chk = chk * 31 + nf_then_body(1);
	chk = chk * 31 + nf_then_body(0);
	chk = chk * 31 + nf_volatile_cond();
	chk = chk * 31 + nf_diff_side_effect(1);
	chk = chk * 31 + nf_diff_side_effect(0);
	printf("chk=%llu g=%ld\n", chk, g);
	printf("OK\n");
	return 0;
}
