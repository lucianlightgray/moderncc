int printf(const char *, ...);

int arrindex = 4;
int getintcnt = 0;

int getint(int v)
{
	getintcnt++;
	return v + 1;
}

int bump_glob(int v)
{
	return arrindex++ + v;
}

int main(void)
{
	int a, b, c, d;
	a = getint(3);
	b = getint(3);
	c = bump_glob(0);
	d = bump_glob(0);
	printf("a=%d b=%d cnt=%d c=%d d=%d idx=%d\n", a, b, getintcnt, c, d, arrindex);
	return 0;
}
