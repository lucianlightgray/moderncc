extern int printf(const char *, ...);

int main(int argc, char **argv) {
	volatile int x = 1;
	volatile int n = -1;
	volatile int y = x << n;
	printf("no-trap: %d\n", y);
	return 0;
}
