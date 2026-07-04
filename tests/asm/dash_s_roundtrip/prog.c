extern int printf(const char *, ...);

struct pt {
	int x, y;
};
static int g_counter = 41;
static const char *msg = "rt";

static int fact(int n) {
	return n <= 1 ? 1 : n * fact(n - 1);
}

static long sumarr(const int *a, int n) {
	long s = 0;
	for (int i = 0; i < n; i++)
		s += a[i] << 1;
	return s;
}

static int classify(int x) {
	switch (x) {
	case 0:
		return 100;
	case 3:
		return 300;
	case 9:
		return 900;
	default:
		return x * 2 - 1;
	}
}

static unsigned bits(unsigned v) {
	return (v & 0xff) | (v >> 4) ^ (v << 3);
}

static int add1(int x) {
	return x + 1;
}
static int neg(int x) {
	return -x;
}

static double g_scale = 2.5;
static float g_bias = 0.75f;

static double dpoly(double x, double y) {
	double q = x * y + x / y - (x - y);
	return q * g_scale;
}

static float fpoly(float x, float y) {
	float q = x * y - x / y + (x + y);
	return q - g_bias;
}

static double weight(int i, long long n, float f) {
	double d = i;
	d += (double)n;
	d *= (double)f;
	return d;
}

static int quantize(double d, float f) {
	int a = (int)d;
	long long b = (long long)d;
	int c = (int)f;
	float ff = (float)d;
	return a + (int)b + c + (int)ff;
}

struct dp {
	double a, b;
};

static struct dp mkdp(double x) {
	struct dp r;
	r.a = x + 0.5;
	r.b = x * 2.0;
	return r;
}

static double dpdot(double w, struct dp p) {
	return w * p.a - p.b / w;
}

static double dfrac(double d) {
	return d - (int)d;
}
static float ffrac(float f) {
	return f - (int)f;
}

static int fdcmp(double a, double b, float c, float d) {
	int n = 0;
	if (a < b)
		n |= 1;
	if (a >= b)
		n |= 2;
	if (a == b)
		n |= 4;
	if (a != b)
		n |= 8;
	if (c > d)
		n |= 16;
	if (c <= d)
		n |= 32;
	if (c == d)
		n |= 64;
	return n;
}

int main(void) {
	int a[6] = {3, 1, 4, 1, 5, 9};
	struct pt p = {7, 35};
	int (*fp[2])(int) = {add1, neg};
	int acc = g_counter + p.x + p.y;

	for (int i = 0; i < 2; i++)
		acc = fp[i](acc);

	double dv = dpoly(3.5, 1.25);
	float fv = fpoly(2.0f, 0.5f);
	double wv = weight(3, 1000000007LL, 1.5f);
	double pv = dpdot(2.0, mkdp(1.5));

	printf("%s %d %ld %d %d %u %d %.6f %.6f %.6f %.6f %.6f %.6f %d %d %d\n",
		   msg, fact(6), sumarr(a, 6), classify(9), classify(5),
		   bits(0x1234u), acc,
		   dv, (double)fv, wv, pv, dfrac(dv), (double)ffrac(fv),
		   quantize(dv, fv), fdcmp(dv, wv, fv, 0.5f), fdcmp(1.5, 1.5, 2.0f, 2.0f));
	return 0;
}
