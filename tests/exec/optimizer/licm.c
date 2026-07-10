extern int printf(const char *, ...);
static int g;
static int fx(int x) { g++; return x; }

static int inv_while(int a, int b, int n) {
	int foo = a * b;
	int s = 0, i = 0;
	while (i < n) { s += a * b + i; i++; }
	return s + foo;
}
static int inv_for(int a, int b, int n) {
	int foo = a + b;
	int s = 0;
	for (int i = 0; i < n; i++) s += a + b - i;
	return s + foo;
}
static int inv_do(int a, int b, int n) {
	int foo = a ^ b;
	int s = 0, i = 0;
	do { s += a ^ b; i++; } while (i < n);
	return s + foo;
}
static int inv_nested(int a, int b, int n) {
	int foo = a * b;
	int s = 0;
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++)
			s += a * b;
	return s + foo;
}
static int nf_operand_mut(int a, int n) {
	int foo = a * 3;
	int s = 0;
	for (int i = 0; i < n; i++) { a = a + 1; s += a * 3; }
	return s + foo;
}
static int nf_foo_reassign(int a, int b, int n) {
	int foo = a * b;
	int s = 0;
	for (int i = 0; i < n; i++) { foo = i; s += a * b; }
	return s + foo;
}
static int nf_compound_mut(int a, int n) {
	int foo = a * 5;
	int s = 0;
	for (int i = 0; i < n; i++) { a += 2; s += a * 5; }
	return s + foo;
}
static int nf_call_operand(int a, int n) {
	int foo = a * 7;
	int s = 0;
	for (int i = 0; i < n; i++) s += fx(a) * 7;
	return s + foo;
}

int main(void) {
	unsigned long long chk = 0;
	chk = chk * 31 + (unsigned)inv_while(3, 4, 5);
	chk = chk * 31 + (unsigned)inv_for(2, 6, 4);
	chk = chk * 31 + (unsigned)inv_do(5, 3, 3);
	chk = chk * 31 + (unsigned)inv_nested(2, 3, 4);
	chk = chk * 31 + (unsigned)nf_operand_mut(2, 4);
	chk = chk * 31 + (unsigned)nf_foo_reassign(3, 2, 3);
	chk = chk * 31 + (unsigned)nf_compound_mut(1, 3);
	chk = chk * 31 + (unsigned)nf_call_operand(2, 5);
	printf("chk=%llu g=%d\n", chk, g);
	printf("OK\n");
	return 0;
}
