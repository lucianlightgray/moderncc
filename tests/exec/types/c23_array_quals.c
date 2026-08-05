extern int printf(const char *, ...);

int main(void) {
	int ok = 1;
	const int (*u)[1] = 0;
	void *v = 0;
	int a[3] = {1, 2, 3};
	int(*x)[3] = &a;
	const int(*p)[3] = x;

	if (_Generic(1 ? u : v, const void *: 1, void *: 0) != 1)
		ok = 0;
	if (_Generic(1 ? v : u, const void *: 1, void *: 0) != 1)
		ok = 0;
	if ((*p)[1] != 2)
		ok = 0;
	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
