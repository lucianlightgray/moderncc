extern int printf(const char *, ...);

static volatile unsigned smp_uv[3] = {198u, 7u, 100u};
static volatile int smp_sv[3] = {198, 7, 100};

static long long smp_k1(void) { return ~(unsigned char)198u; }
static long long smp_k2(void) { return ~(unsigned short)198u; }
static long long smp_k3(void) { return ~(signed char)100u; }
static long long smp_k4(void) { return -(unsigned short)7u; }
static long long smp_k5(void) { return (unsigned char)7u < -1; }
static long long smp_k6(void) { return (unsigned char)7u - 1000; }
static long long smp_k7(void) { return (unsigned short)7u * -1000; }
static long long smp_k8(void) { return (unsigned char)198u >> 1; }

static long long smp_c1(void) { return ~(unsigned char)198; }
static long long smp_c2(void) { return ~(signed char)100; }
static long long smp_c3(void) { return (unsigned char)7 < -1; }
static long long smp_c4(void) { return (unsigned char)7 - 1000; }

static long long smp_v1(void) { return ~(unsigned char)smp_uv[0]; }
static long long smp_v2(void) { return (unsigned char)smp_uv[1] - 1000; }
static long long smp_v3(void) { return ~(unsigned char)smp_sv[0]; }
static long long smp_v4(void) { return -(unsigned short)smp_uv[1]; }

int main(void)
{
	printf("narrowelim %lld %lld %lld %lld %lld %lld %lld %lld", smp_k1(),
				 smp_k2(), smp_k3(), smp_k4(), smp_k5(), smp_k6(), smp_k7(),
				 smp_k8());
	printf(" %lld %lld %lld %lld", smp_c1(), smp_c2(), smp_c3(), smp_c4());
	printf(" %lld %lld %lld %lld\n", smp_v1(), smp_v2(), smp_v3(), smp_v4());
	return 0;
}
