extern int printf(const char *, ...);

int ga = 6, gb = 7, gx = 8;

int main(void)
{
	int a = ga, b = gb;
	int t;
	t = 1;
	t = 2;
	t = gx + 3;
	int p = (a * b + 3);
	int q = (a * b + 3);
	int r = p + q + t + (a * b + 3);
	printf("%d\n", r);
	return 0;
}
