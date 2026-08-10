extern int printf(const char *, ...);

__attribute__((weak)) static int weak_helper(int n) {
	int a = n, i;
	for (i = 0; i < 5; i++)
		a = a * 3 + i;
	return a & 0x7ff;
}

int hot(int n) {
	int s = 0, i;
	for (i = 0; i < n; i++)
		s += weak_helper(i);
	return s;
}

int main(void) {
	printf("weak_static %d\n", hot(120));
	return 0;
}
