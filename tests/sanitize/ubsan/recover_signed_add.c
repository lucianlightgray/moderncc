extern int printf(const char *, ...);

int main(int argc, char **argv) {
	volatile int a = 2000000000;
	volatile int b = 2000000000;
	int c = a + b;
	(void)argc;
	(void)argv;
	(void)c;
	printf("survived\n");
	return 0;
}
