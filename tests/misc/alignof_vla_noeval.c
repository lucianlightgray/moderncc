extern int printf(const char *, ...);

int main(void) {
	int n = 3;
	unsigned long a = _Alignof(int[n++]);
	int m = 5;
	unsigned long b = _Alignof(char[m++]);
	int ok = (a == _Alignof(int)) && (n == 3)
		&& (b == _Alignof(char)) && (m == 5);
	printf("%s\n", ok ? "OK" : "FAIL");
	return ok ? 0 : 1;
}
