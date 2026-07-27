extern int printf(const char *, ...);

int gx = 8;

int main(void)
{
	int t;
	t = 1;
	t = 2;
	t = gx + 3;
	printf("%d\n", t);
	return 0;
}
