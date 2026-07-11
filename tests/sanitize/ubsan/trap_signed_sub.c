extern int printf(const char *, ...);

int main(int argc, char **argv) {
	volatile int a = -2000000000;
	volatile int b = 2000000000;
	volatile int c = a - b;
	printf("no-trap: %d\n", c);
	return 0;
}
