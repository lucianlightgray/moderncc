extern int printf(const char *, ...);

static int calls;

static int side(int v)
{
	calls++;
	return v;
}

static int and_store(int a, int *out)
{
	*out = 0;
	return a && (*out = side(7));
}

static int or_store(int a, int *out)
{
	*out = 0;
	return a || (*out = side(9));
}

static int tern_store(int c, int *out)
{
	*out = 0;
	c ? (*out = side(1)) : (*out = side(2));
	return *out;
}

static int chain(int a, int b, int *out)
{
	*out = 0;
	return a && b && (*out = side(3));
}

int main(void)
{
	int v, r;

	calls = 0;
	r = and_store(0, &v);
	printf("and0 r=%d v=%d calls=%d\n", r, v, calls);
	calls = 0;
	r = and_store(1, &v);
	printf("and1 r=%d v=%d calls=%d\n", r, v, calls);

	calls = 0;
	r = or_store(1, &v);
	printf("or1 r=%d v=%d calls=%d\n", r, v, calls);
	calls = 0;
	r = or_store(0, &v);
	printf("or0 r=%d v=%d calls=%d\n", r, v, calls);

	calls = 0;
	r = tern_store(1, &v);
	printf("tern1 r=%d v=%d calls=%d\n", r, v, calls);
	calls = 0;
	r = tern_store(0, &v);
	printf("tern0 r=%d v=%d calls=%d\n", r, v, calls);

	calls = 0;
	r = chain(1, 0, &v);
	printf("chain10 r=%d v=%d calls=%d\n", r, v, calls);
	calls = 0;
	r = chain(1, 1, &v);
	printf("chain11 r=%d v=%d calls=%d\n", r, v, calls);
	return 0;
}
