extern int printf(const char *, ...);

static long random(void) {
	long a = 1, i;
	for (i = 0; i < 7; i++)
		a = a * 3 + i;
	if (a > 1000)
		a -= 1000;
	if (a < 0)
		a = -a;
	return a;
}

static int mix(int n) { return (int)(random() & 0xff) + n; }

int hot(int n) {
	int s = 0, i;
	for (i = 0; i < n; i++)
		s += mix(i);
	return s;
}

int main(void) {
	printf("collide_libc %d\n", hot(200));
	return 0;
}
