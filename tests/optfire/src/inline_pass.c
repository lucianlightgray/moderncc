extern int printf(const char *, ...);

static int addk(int v, int k)
{
	return v + k;
}

static int mixk(int v)
{
	return addk(v, 7) ^ addk(v, 3);
}

static int driver(int seed)
{
	int r = seed;
	int i;

	for (i = 0; i < 5; i++)
		r = mixk(r) & 0xffff;
	return r;
}

int main(void)
{
	printf("inline_pass=%d\n", driver(13));
	return 0;
}
