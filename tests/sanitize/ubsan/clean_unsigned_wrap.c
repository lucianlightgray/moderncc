extern int printf(const char *, ...);

int main(int argc, char **argv) {
	volatile unsigned a = 4000000000u;
	volatile unsigned b = 4000000000u;
	volatile unsigned add = a + b;
	volatile unsigned mul = a * b;
	volatile unsigned n = 20;
	volatile unsigned shl = a << n;
	printf("%u %u %u\n", add, mul, shl);
	return 0;
}
