int printf(const char *, ...);

int i;

signed char foo(signed char val)
{
	i++;
	if (i > 1)
		return -1;
	else
		return val;
}

int main(void)
{
	int a, b;
	i = 0;
	a = foo(10);
	i += 2;
	b = foo(11);
	printf("a=%d b=%d i=%d\n", a, b, i);
	return 0;
}
