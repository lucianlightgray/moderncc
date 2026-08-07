#include <stdio.h>
#include <string.h>

double fabs(double);
float fabsf(float);

static unsigned long long bits_d(double x)
{
	unsigned long long u;
	memcpy(&u, &x, 8);
	return u;
}

static unsigned bits_f(float x)
{
	unsigned u;
	memcpy(&u, &x, 4);
	return u;
}

static double from_d(unsigned long long u)
{
	double x;
	memcpy(&x, &u, 8);
	return x;
}

static float from_f(unsigned u)
{
	float x;
	memcpy(&x, &u, 4);
	return x;
}

static double bi_d(double x) { return __builtin_fabs(x); }
static float bi_f(float x) { return __builtin_fabsf(x); }
static double call_d(double x) { return fabs(x); }
static float call_f(float x) { return fabsf(x); }

static const unsigned long long dcase[] = {
		0x0000000000000000ULL, 0x8000000000000000ULL,
		0x3ff0000000000000ULL, 0xbff0000000000000ULL,
		0x7ff0000000000000ULL, 0xfff0000000000000ULL,
		0x7ff8000000000000ULL, 0xfff8000000000000ULL,
		0x7ff800000dedbeefULL, 0xfff800000dedbeefULL,
		0x000fffffffffffffULL, 0x800fffffffffffffULL,
		0x0000000000000001ULL, 0x8000000000000001ULL,
};

static const unsigned fcase[] = {
		0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u,
		0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00000u,
		0x7fc0deadu, 0xffc0deadu, 0x007fffffu, 0x807fffffu,
		0x00000001u, 0x80000001u,
};

int main(void)
{
	int ok = 1;
	unsigned i;

	for (i = 0; i < sizeof dcase / sizeof *dcase; i++) {
		unsigned long long in = dcase[i], want = in & 0x7fffffffffffffffULL;
		if (bits_d(bi_d(from_d(in))) != want)
			ok = 0;
		if (bits_d(call_d(from_d(in))) != want)
			ok = 0;
	}
	for (i = 0; i < sizeof fcase / sizeof *fcase; i++) {
		unsigned in = fcase[i], want = in & 0x7fffffffu;
		if (bits_f(bi_f(from_f(in))) != want)
			ok = 0;
		if (bits_f(call_f(from_f(in))) != want)
			ok = 0;
	}

	if (bits_d(__builtin_fabs(-0.0)) != 0)
		ok = 0;
	if (bits_f(__builtin_fabsf(-0.0f)) != 0)
		ok = 0;
	if (__builtin_fabs(-2.5) != 2.5 || __builtin_fabs(2.5) != 2.5)
		ok = 0;
	if (__builtin_fabsf(-3.5f) != 3.5f)
		ok = 0;
	if (__builtin_fabs(-3) != 3.0)
		ok = 0;
	if (__builtin_fabsl(-4.5L) != 4.5L)
		ok = 0;

	{
		static const double k = __builtin_fabs(-6.25);
		static const float kf = __builtin_fabsf(-1.5f);
		if (k != 6.25 || kf != 1.5f)
			ok = 0;
	}

	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
