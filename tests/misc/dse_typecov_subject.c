extern int printf(const char *, ...);

union U {
	int i;
	long l;
};

static long union_pun(void)
{
	union U u;
	u.l = 0x1122334455667788L;
	u.i = 5;
	return u.l;
}

static double dead_double(double x)
{
	double d;
	d = x + 1.0;
	d = x * 2.0;
	return d;
}

static void *dead_ptr(void *p, void *q)
{
	void *r;
	r = p;
	r = q;
	return r;
}

int main(void)
{
	printf("%lx %.1f %d\n", union_pun(), dead_double(3.0),
				 dead_ptr((void *)1, (void *)2) == (void *)2);
	return 0;
}
