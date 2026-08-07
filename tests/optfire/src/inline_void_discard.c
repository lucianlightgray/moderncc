extern int printf(const char *, ...);

static int trace[8];
static int trace_n;

static int note(int tag, int v)
{
	if (trace_n < 8)
		trace[trace_n++] = tag * 100 + v;
	return v * 3 + tag;
}

static int accum(int a, int b)
{
	return note(1, a) + note(2, b);
}

static int step(int seed)
{
	int r = seed;
	int i;

	for (i = 0; i < 3; i++) {
		(void)note(3, r);
		r = accum(r, i) & 0xff;
	}
	return r;
}

int main(void)
{
	int slot;
	int i;

	(void)note(4, 7);
	slot = step(5);
	(void)accum(slot, 1);
	for (i = 0; i < trace_n; i++)
		printf("t%d=%d\n", i, trace[i]);
	printf("slot=%d n=%d\n", slot, trace_n);
	return 0;
}
