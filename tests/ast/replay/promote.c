int ident(int x);

static int calc(int n) {
	int a = n + 1;
	int b = a * 3;
	int c = b + a;
	a = c - b;
	return a + b + c - 6;
}

static int loopy(int start) {
	int s = 0;
	int v = start;
	for (int i = 0; i < 5; i++) {
		s = s + v;
		v = v + 2;
	}
	return s;
}

int ident(int x) {
	return x;
}

static int sumptr(int *p, int n) {
	int t = 0;
	for (int i = 0; i < n; i++)
		t += p[i];
	return t;
}

static int callful(int start) {
	int p = ident(start);
	int q = ident(start + 1);
	int r = ident(start + 2);
	int t = ident(p + q + r);
	return p + q + r + t;
}

static int fdot(double *v, int n) {
	double s = 0.0;
	for (int i = 0; i < n; i++)
		s += v[i];
	return (int)s;
}

int main(void) {
	int arr[4] = {1, 2, 3, 4};
	double dv[3] = {1.0, 2.0, 3.0};
	return calc(5) + loopy(4) + callful(1) + sumptr(arr, 4) + fdot(dv, 3) - 68;
}
