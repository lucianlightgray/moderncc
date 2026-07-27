extern int printf(const char *, ...);

static int selfsub(int x, int y)
{
	int a = x - x;
	int b = (x + y) - (x + y);
	int c = (x * y) - (x * y);
	return a + b + c + x * y;
}

static long lselfsub(long v, long w)
{
	long a = v - v;
	long b = (v | w) - (v | w);
	return a + b + v * w;
}

int main(void)
{
	printf("ident_arith=%ld\n", (long)selfsub(3, 5) + lselfsub(9L, 4L));
	return 0;
}
