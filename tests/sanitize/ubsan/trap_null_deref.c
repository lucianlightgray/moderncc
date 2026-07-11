extern int printf(const char *, ...);

int main(int argc, char **argv) {
	volatile int *p = 0;
	int v = *p;
	printf("no-trap: %d\n", v);
	return 0;
}
