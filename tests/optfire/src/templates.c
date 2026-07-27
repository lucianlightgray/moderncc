extern int printf(const char *, ...);

static int folded(int seed)
{
	int a = 3 * 7;
	int b = a + 100 - 21;
	int c = b * 2;
	int d = c / 4;
	int e = (d + a) ^ 0;
	int r = seed;
	int i;

	for (i = 0; i < 6; i++) {
		r += e + d;
		r ^= (a << 2);
		r += b - c;
	}
	if (a == 21)
		r += 1000;
	else
		r -= 1000;
	return r & 0xffff;
}

int main(void)
{
	printf("templates=%d\n", folded(5));
	return 0;
}
