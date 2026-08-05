extern int printf(const char *, ...);

static int sum_vla(int n) {
	volatile int vla[n] = {};
	int i, s = 0;

	for (i = 0; i < n; i++)
		s += vla[i];
	return s;
}

static int sum_vla2(int n) {
	int vla[n][3] = {};
	int i, j, s = 0;

	for (i = 0; i < n; i++)
		for (j = 0; j < 3; j++)
			s += vla[i][j];
	vla[n - 1][2] = 7;
	return s + vla[n - 1][2];
}

int main(void) {
	int ok = 1;

	if (sum_vla(1) != 0)
		ok = 0;
	if (sum_vla(64) != 0)
		ok = 0;
	if (sum_vla2(5) != 7)
		ok = 0;
	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
