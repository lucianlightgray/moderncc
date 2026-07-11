extern int printf(const char *, ...);

int main(int argc, char **argv) {
	volatile int a = 30000;
	volatile int b = 40000;
	volatile int add = a + b;
	volatile int sub = a - b;
	volatile int mul = a * b;
	volatile long long wide = (long long)a * b;
	volatile int sh = 1;
	volatile int n = 30;
	volatile int shl = sh << n;
	volatile int q = mul / a;
	volatile int r = mul % a;
	printf("%d %d %d %lld %d %d %d\n", add, sub, mul, wide, shl, q, r);
	return 0;
}
