extern int printf(const char *, ...);

int gx = 5;

int main(void)
{
	int flag = 0;
	int r = gx;
	if (flag)
		r = r * 100;
	else
		r = r + 1;
	if (!flag)
		r += 7;
	printf("%d\n", r);
	return 0;
}
