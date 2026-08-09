int a[256], b[256], c[256];
int m[32][32];
int gsum;

int dp_plain(int n) {
	for (int i = 0; i < n; i++)
		a[i] = b[i] * 2;
	return a[0];
}

int dp_fwd(int n) {
	for (int i = 1; i < n; i++)
		a[i] = a[i - 1] + 1;
	return a[0];
}

int dp_bwd(int n) {
	for (int i = 0; i < n; i++)
		a[i] = a[i + 1];
	return a[0];
}

int dp_reduce(int n) {
	int s = 0;
	for (int i = 0; i < n; i++)
		s += a[i];
	return s;
}

int dp_alias(int *p, int *q, int n) {
	for (int i = 0; i < n; i++)
		p[i] = q[i] + 1;
	return p[0];
}

int dp_nest_outer_carried(int n) {
	for (int i = 1; i < n; i++)
		for (int j = 0; j < n; j++)
			m[i][j] = m[i - 1][j] + 1;
	return m[0][0];
}

int dp_nest_inner_carried(int n) {
	for (int i = 0; i < n; i++)
		for (int j = 1; j < n; j++)
			m[i][j] = m[i][j - 1] + 1;
	return m[0][0];
}

int dp_nest_both(int n) {
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++)
			m[i][j] = i + j;
	return m[0][0];
}

int dp_priv_temp(int n) {
	for (int i = 0; i < n; i++) {
		int t = b[i] * 3;
		a[i] = t + 1;
	}
	return a[0];
}

int dp_cond_scalar(int n) {
	int t = 0;
	for (int i = 0; i < n; i++) {
		if (b[i] > 0)
			t = b[i];
		a[i] = t;
	}
	return a[0];
}

int dp_global_acc(int n) {
	for (int i = 0; i < n; i++)
		gsum += b[i];
	return gsum;
}

extern int dp_opaque(int);

int dp_call(int n) {
	for (int i = 0; i < n; i++)
		a[i] = dp_opaque(b[i]);
	return a[0];
}

int dp_break(int n) {
	for (int i = 0; i < n; i++) {
		if (b[i] < 0)
			break;
		a[i] = b[i];
	}
	return a[0];
}

int dp_ptr_bump(int *p, int n) {
	for (int i = 0; i < n; i++) {
		*p = i;
		p++;
	}
	return 0;
}

int dp_while(int n) {
	int i = 0;
	while (i < n) {
		c[i] = b[i];
		i++;
	}
	return c[0];
}

int dp_do(int n) {
	int i = 0;
	do {
		c[i] = b[i];
		i++;
	} while (i < n);
	return c[0];
}

int dp_stride2(int n) {
	for (int i = 0; i < n; i += 2)
		a[i] = b[i] + c[i];
	return a[0];
}

int dp_dist8(int n) {
	for (int i = 8; i < n; i++)
		a[i] = a[i - 8] + 1;
	return a[0];
}

int main(void) {
	int r = dp_plain(4) + dp_fwd(4) + dp_bwd(4) + dp_reduce(4);
	r += dp_alias(a, b, 4) + dp_nest_outer_carried(4);
	r += dp_nest_inner_carried(4) + dp_nest_both(4);
	r += dp_priv_temp(4) + dp_cond_scalar(4) + dp_global_acc(4);
	r += dp_break(4) + dp_ptr_bump(a, 4) + dp_while(4) + dp_do(4);
	r += dp_stride2(4) + dp_dist8(16);
	return r != r;
}
