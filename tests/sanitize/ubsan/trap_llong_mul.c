extern int printf(const char *, ...);

int main(int argc, char **argv) {
	volatile long long a = 4000000000ll;
	volatile long long b = 4000000000ll;
	volatile long long c = a * b;
	printf("no-trap: %lld\n", c);
	return 0;
}
