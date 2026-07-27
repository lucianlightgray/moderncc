extern int printf(const char *, ...);

long ga = 100000L;
int gb = 37;

int main(void)
{
	long a = ga;
	int b = gb;
	int r = (int)(a + b);
	int s = (int)(a * b);
	int t = (int)(b - a);
	printf("%d %d %d\n", r, s, t);
	return 0;
}
