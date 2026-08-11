extern int printf(const char *, ...);
extern double fabs(double);
extern float fabsf(float);

static volatile double smp_zero = 0.0;
static volatile double smp_one = 1.0;

static int smp_lt(double x)
{
	return fabs(x) < 0.0;
}

static int smp_ltf(float x)
{
	return fabsf(x) < 0.0f;
}

static int smp_ge(double x)
{
	return fabs(x) >= 0.0;
}

static int smp_ltneg(double x)
{
	return fabs(x) < -0.0;
}

int main(void)
{
	double nan = smp_zero / smp_zero;
	double inf = smp_one / smp_zero;
	double neg = -3.5;
	double nzero = -0.0;

	printf("fabscmp %d %d %d %d %d %d %d %d %d %d %d\n", smp_lt(1.0),
				 smp_lt(neg), smp_lt(nan), smp_lt(inf), smp_lt(-inf), smp_lt(nzero),
				 smp_ltf(-2.5f), smp_ltf((float)nan), smp_ge(1.0), smp_ge(nan),
				 smp_ltneg(nan));
	return 0;
}
