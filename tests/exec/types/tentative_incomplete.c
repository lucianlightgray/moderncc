extern int printf(const char *, ...);

struct s0;
struct s0 x;
struct s0 y;

struct s0 *addr_of_y(void) {
	return &y;
}

struct s0 {
	int a;
	int b;
};

union u0;
union u0 u;

union u0 {
	int a;
	double d;
};

int main(void) {
	int ok = 1;

	x.a = 3;
	x.b = 4;
	if (x.a + x.b != 7)
		ok = 0;
	if (addr_of_y() != &y)
		ok = 0;
	if (sizeof x != 2 * sizeof(int))
		ok = 0;
	if (sizeof u != sizeof(double))
		ok = 0;
	if (y.a != 0 || y.b != 0)
		ok = 0;
	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
