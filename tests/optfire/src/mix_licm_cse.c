extern int printf(const char *, ...);

int ga = 6, gb = 7, gn = 15;

int main(void)
{
	int i = 0, s = 0;
	int a = ga, b = gb, n = gn;
	while (i < n) {
		s += (a + b);
		s ^= (a + b);
		s -= (a + b);
		i++;
	}
	int p = (a * b + 3);
	int q = (a * b + 3);
	printf("%d %d\n", s, p + q);
	return 0;
}
