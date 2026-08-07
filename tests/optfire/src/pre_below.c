extern int printf(const char *, ...);

int ga = 3, gb = 4, gx = 1;

int main(void)
{
	int a = ga, b = gb, x = gx;
	int t;
	if (x)
		t = a * b;
	else
		t = 1;
	t = t + a * b;
	printf("%d\n", t);
	return 0;
}
