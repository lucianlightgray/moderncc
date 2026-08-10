int printf(const char *, ...);

int bar(const char **x)
{
	return *(*x)++;
}

int main(void)
{
	const char *p = "abcdef";
	int c0, c1, c2;
	c0 = bar(&p);
	c1 = bar(&p);
	c2 = bar(&p);
	printf("c=%c%c%c rest=%s\n", c0, c1, c2, p);
	return 0;
}
