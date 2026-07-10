extern int printf(const char *, ...);

static long g;

static int sfx1(void) {
	g += 1;
	return 1;
}

static int sfx0(void) {
	g += 1;
	return 0;
}

static int ext(int x) {
	g += 1;
	return x;
}

static volatile int vz;

static int if1_then(void) {
	int r = 1;
	if (1)
		r = 5;
	return r;
}

static int if0_then(void) {
	int r = 1;
	if (0)
		r = 5;
	return r;
}

static int if1_else(void) {
	int r = 0;
	if (1)
		r = 7;
	else
		r = 9;
	return r;
}

static int if0_else(void) {
	int r = 0;
	if (0)
		r = 7;
	else
		r = 9;
	return r;
}

static int if_const_expr(void) {
	int r = 0;
	if (2 + 3)
		r = 4;
	return r;
}

static int if_const_zero_expr(void) {
	int r = 0;
	if (2 - 2)
		r = 4;
	else
		r = 8;
	return r;
}

static int if_mul_zero(void) {
	int r = 0;
	if (3 * 0)
		r = 4;
	else
		r = 6;
	return r;
}

static int if_cprop_zero(void) {
	int c = 0;
	int r = 1;
	if (c)
		r = 5;
	return r;
}

static int if_cprop_nonzero(void) {
	int c = 5;
	int r = 1;
	if (c)
		r = 9;
	else
		r = 2;
	return r;
}

static int if_cprop_chain(void) {
	int a = 0;
	int b = a;
	int r = 3;
	if (b)
		r = 4;
	else
		r = 11;
	return r;
}

static int if_dead_effect(void) {
	if (0)
		g += 100;
	g += 2;
	return 0;
}

static int if_taken_effect(void) {
	if (1)
		g += 4;
	else
		g += 100;
	return 0;
}

static int if_else_dead_effect(void) {
	if (1)
		g += 8;
	else
		g += 100;
	g += 1;
	return 0;
}

static int if_nested(void) {
	int r = 0;
	if (1) {
		if (0)
			r = 1;
		else
			r = 2;
	} else {
		r = 3;
	}
	return r;
}

static int if_nested_dead(void) {
	int r = 0;
	if (0) {
		if (1)
			r = 1;
		g += 100;
	} else {
		r = 5;
	}
	return r;
}

static int if_both_return(void) {
	if (1)
		return 21;
	else
		return 99;
}

static int if0_both_return(void) {
	if (0)
		return 99;
	else
		return 22;
}

static int if_sizeof(void) {
	int r = 0;
	if (sizeof(int))
		r = 4;
	return r;
}

static int if_char_lit(void) {
	int r = 0;
	if ('a')
		r = 3;
	return r;
}

static long if_long_const(void) {
	long r = 0;
	if (1234567890123L)
		r = 7;
	return r;
}

static int if_bool_const(void) {
	int r = 0;
	if ((_Bool)0)
		r = 4;
	else
		r = 6;
	return r;
}

static int if_empty_then(void) {
	int r = 3;
	if (1) {
	} else {
		r = 9;
	}
	return r;
}

static int if_char_cprop(void) {
	char c = 0;
	int r = 1;
	if (c)
		r = 5;
	return r;
}

static int nf_side_cond_true(void) {
	int r = 0;
	if (sfx1())
		r = 5;
	else
		r = 6;
	return r;
}

static int nf_side_cond_false(void) {
	int r = 0;
	if (sfx0())
		r = 5;
	else
		r = 6;
	return r;
}

static int nf_param_cond(int x) {
	int r = 0;
	if (x)
		r = 5;
	else
		r = 6;
	return r;
}

static int nf_volatile_cond(void) {
	int r = 0;
	if (vz)
		r = 5;
	else
		r = 6;
	return r;
}

static int nf_call_in_cond(void) {
	int r = 0;
	if (ext(0))
		r = 5;
	else
		r = 6;
	return r;
}

static int nf_reassign_cond(void) {
	int c = 0;
	c = ext(1);
	int r = 0;
	if (c)
		r = 5;
	else
		r = 6;
	return r;
}

static int nf_label_in_dead(void) {
	int r = 1;
	if (0) {
	L:
		r = 9;
	}
	if (r == 7)
		goto L;
	return r;
}

static int mix_cprop_then_dead(void) {
	int c = 0;
	int r = 2;
	if (c) {
		g += 100;
		r = 5;
	}
	g += 3;
	return r;
}

int main(void) {
	unsigned long long chk = 0;
	vz = 0;
	chk = chk * 31 + if1_then();
	chk = chk * 31 + if0_then();
	chk = chk * 31 + if1_else();
	chk = chk * 31 + if0_else();
	chk = chk * 31 + if_const_expr();
	chk = chk * 31 + if_const_zero_expr();
	chk = chk * 31 + if_mul_zero();
	chk = chk * 31 + if_cprop_zero();
	chk = chk * 31 + if_cprop_nonzero();
	chk = chk * 31 + if_cprop_chain();
	chk = chk * 31 + if_dead_effect();
	chk = chk * 31 + if_taken_effect();
	chk = chk * 31 + if_else_dead_effect();
	chk = chk * 31 + if_nested();
	chk = chk * 31 + if_nested_dead();
	chk = chk * 31 + if_both_return();
	chk = chk * 31 + if0_both_return();
	chk = chk * 31 + if_sizeof();
	chk = chk * 31 + if_char_lit();
	chk = chk * 31 + (int)if_long_const();
	chk = chk * 31 + if_bool_const();
	chk = chk * 31 + if_empty_then();
	chk = chk * 31 + if_char_cprop();
	chk = chk * 31 + nf_side_cond_true();
	chk = chk * 31 + nf_side_cond_false();
	chk = chk * 31 + nf_param_cond(1);
	chk = chk * 31 + nf_param_cond(0);
	chk = chk * 31 + nf_volatile_cond();
	chk = chk * 31 + nf_call_in_cond();
	chk = chk * 31 + nf_reassign_cond();
	chk = chk * 31 + nf_label_in_dead();
	chk = chk * 31 + mix_cprop_then_dead();
	printf("chk=%llu g=%ld\n", chk, g);
	printf("OK\n");
	return 0;
}
