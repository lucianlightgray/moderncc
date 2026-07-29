extern int printf(const char *, ...);

static unsigned long long seed = 99194853094755497ull;

static unsigned long long xs(void)
{
	seed ^= seed << 13;
	seed ^= seed >> 7;
	seed ^= seed << 17;
	return seed;
}

int main(void)
{
	union {
		double d;
		unsigned long long u;
	} du;
	union {
		float f;
		unsigned u;
	} fu;
	double inf;
	int i;

	for (i = 0; i < 2000; i++) {
		unsigned long long v = xs();

		du.u = v;
		fu.u = (unsigned)v;
		if (!!__builtin_signbit(du.d) != (int)(v >> 63))
			return 1;
		if (!!__builtin_signbit(fu.f) != (int)((unsigned)v >> 31))
			return 2;
	}
	if (!!__builtin_signbit(-0.0) != 1 || !!__builtin_signbit(0.0) != 0)
		return 3;
	if (!!__builtin_signbit(-0.0f) != 1 || !!__builtin_signbit(0.0f) != 0)
		return 4;
	inf = 1.0 / 0.0;
	if (!!__builtin_signbit(-inf) != 1 || !!__builtin_signbit(inf) != 0)
		return 5;
	if (!!__builtin_signbit(-1.5L) != 1 || !!__builtin_signbit(1.5L) != 0)
		return 6;
	printf("OK\n");
	return 0;
}
