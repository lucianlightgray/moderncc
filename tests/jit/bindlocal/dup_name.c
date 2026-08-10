extern int printf(const char *, ...);
extern int other_hot(int n);

static int dup_helper(int n) {
	int a = n, i;
	for (i = 0; i < 4; i++)
		a = a * 7 + 1;
	return a & 0x3ff;
}

int hot(int n) {
	int s = 0, i;
	for (i = 0; i < n; i++)
		s += dup_helper(i);
	return s;
}

int main(void) {
	printf("dup_name %d %d\n", hot(90), other_hot(90));
	return 0;
}
