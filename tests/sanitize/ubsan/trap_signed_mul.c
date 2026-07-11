extern int printf(const char *, ...);

int main(int argc, char **argv) {
	volatile int a = 100000;
	volatile int b = 100000;
	volatile int c = a * b;
	printf("no-trap: %d\n", c);
	return 0;
}
