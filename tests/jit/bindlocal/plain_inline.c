extern int printf(const char *, ...);

inline int inline_helper(int n) {
	int a = n, i;
	for (i = 0; i < 5; i++)
		a = a * 5 + 2;
	return a & 0x7ff;
}

int hot(int n) {
	int s = 0, i;
	for (i = 0; i < n; i++)
		s += inline_helper(i);
	return s;
}

int main(void) {
	printf("plain_inline %d\n", hot(100));
	return 0;
}
