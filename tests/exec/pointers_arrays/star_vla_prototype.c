extern int printf(const char *, ...);

void inner_star(int n, double x[3][*]);
void outer_star(int n, double x[*][3]);
void both_star(int n, double x[*][*]);
void unsized_outer(int a[][*]);
void ptr_to_star(char (*a)[4][*]);

void inner_star(int n, double x[3][n]) {
	x[2][n - 1] = 1.5;
}

void outer_star(int n, double x[n][3]) {
	x[n - 1][2] = 2.5;
}

void both_star(int n, double x[n][n]) {
	x[n - 1][n - 1] = 3.5;
}

void unsized_outer(int a[][4]) {
	a[1][3] = 7;
}

void ptr_to_star(char (*a)[4][4]) {
	(*a)[3][3] = 'z';
}

int main(void) {
	int ok = 1;
	double a[3][5];
	double b[4][3];
	double c[2][2];
	int d[2][4];
	char e[4][4];

	inner_star(5, a);
	outer_star(4, b);
	both_star(2, c);
	unsized_outer(d);
	ptr_to_star(&e);

	if (a[2][4] != 1.5)
		ok = 0;
	if (b[3][2] != 2.5)
		ok = 0;
	if (c[1][1] != 3.5)
		ok = 0;
	if (d[1][3] != 7)
		ok = 0;
	if (e[3][3] != 'z')
		ok = 0;
	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
