extern int printf(const char *, ...);

int gv = 1000;

int main(void)
{
	int a = gv;
	int c = (2 + 3) * 4;
	int d = a + 0;
	int e = a * 1;
	int q = a / 7;
	printf("%d %d %d %d\n", c, d + e, q, a - a);
	return 0;
}
