extern int printf(const char *, ...);

int gx = -37;

int main(void)
{
	int x = gx;
	int a = x < 0 ? -x : x;
	printf("%d\n", a);
	return 0;
}
