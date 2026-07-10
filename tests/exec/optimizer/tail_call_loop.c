extern int printf(const char *, ...);

static long g;

static long fact(long n, long acc) {
	g += 1;
	if (n <= 1)
		return acc;
	return fact(n - 1, acc * n);
}

static long suma(int n, long acc) {
	g += 1;
	if (n == 0)
		return acc;
	return suma(n - 1, acc + n);
}

static unsigned usum(unsigned n, unsigned acc) {
	g += 1;
	if (n == 0u)
		return acc;
	return usum(n - 1u, acc + n);
}

static int csum(char n, int acc) {
	g += 1;
	if (n == 0)
		return acc;
	return csum((char)(n - 1), acc + n);
}

static int ssum(short n, int acc) {
	g += 1;
	if (n == 0)
		return acc;
	return ssum((short)(n - 1), acc + n);
}

static long deep(long n, long acc) {
	g += 1;
	if (n == 0)
		return acc;
	return deep(n - 1, acc + 1);
}

static int inif(int n, int acc) {
	g += 1;
	if (n != 0)
		return inif(n - 1, acc + n);
	return acc;
}

static int inloop(int n, int acc) {
	while (1) {
		g += 1;
		if (n == 0)
			return acc;
		return inloop(n - 1, acc + 3);
	}
}

static int swap2(int a, int b, int depth) {
	g += 1;
	if (depth == 0)
		return a * 1000 + b;
	return swap2(b, a, depth - 1);
}

static int rot3(int a, int b, int c, int depth) {
	g += 1;
	if (depth == 0)
		return a * 100 + b * 10 + c;
	return rot3(b, c, a, depth - 1);
}

static int reorder(int x, int y) {
	g += 1;
	if (x <= 0)
		return y;
	return reorder(x - 1, y + x);
}

static long nontail(long n) {
	g += 1;
	if (n == 0)
		return 0;
	return 1 + nontail(n - 1);
}

static long mixed(long n, long acc) {
	g += 1;
	if (n < 0)
		return acc * 2;
	if (n == 0)
		return acc;
	return mixed(n - 1, acc + n);
}

int main(void) {
	unsigned long long chk = 0;
	chk = chk * 31 + (unsigned long long)fact(10, 1);
	chk = chk * 31 + (unsigned long long)suma(100, 0);
	chk = chk * 31 + (unsigned long long)usum(50u, 0u);
	chk = chk * 31 + (unsigned long long)csum(20, 0);
	chk = chk * 31 + (unsigned long long)ssum(300, 0);
	chk = chk * 31 + (unsigned long long)deep(10000, 0);
	chk = chk * 31 + (unsigned long long)inif(40, 0);
	chk = chk * 31 + (unsigned long long)inloop(25, 0);
	chk = chk * 31 + (unsigned long long)swap2(7, 3, 4);
	chk = chk * 31 + (unsigned long long)swap2(7, 3, 5);
	chk = chk * 31 + (unsigned long long)rot3(1, 2, 3, 3);
	chk = chk * 31 + (unsigned long long)rot3(1, 2, 3, 4);
	chk = chk * 31 + (unsigned long long)reorder(9, 0);
	chk = chk * 31 + (unsigned long long)nontail(50);
	chk = chk * 31 + (unsigned long long)mixed(30, 0);
	chk = chk * 31 + (unsigned long long)mixed(-5, 7);
	printf("chk=%llu g=%ld\n", chk, g);
	printf("OK\n");
	return 0;
}
