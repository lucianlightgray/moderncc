extern int printf(const char *, ...);

static int sink[8];

int main(void)
{
	long s = 0;
	int i;

	for (i = 0; i < 8; i++) {
		sink[i] = i * i - i + 3;
		s += sink[i];
	}

	int t = 100;
	int j;
	for (j = 1; j < 5; j++)
		t -= j * 2;

	printf("unroll=%ld,%d\n", s, t);
	return 0;
}
