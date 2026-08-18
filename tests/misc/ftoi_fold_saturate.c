static int gi_big = (int)1e30;
static int gi_neg = (int)-1e30;
static unsigned gu_m1 = (unsigned)-1.0;
static unsigned gu_big = (unsigned)1e30;
static unsigned gu_neg = (unsigned)-1e30;
static long long gll_big = (long long)1e30;
static long long gll_neg = (long long)-1e30;
static unsigned long long gull_big = (unsigned long long)1e30;
static unsigned long long gull_m1 = (unsigned long long)-1.0;

int main(void) {
	volatile double big = 1e30, neg = -1e30, m1 = -1.0;

	int ri_big = (int)big;
	int ri_neg = (int)neg;
	unsigned ru_m1 = (unsigned)m1;
	unsigned ru_big = (unsigned)big;
	unsigned ru_neg = (unsigned)neg;
	long long rll_big = (long long)big;
	long long rll_neg = (long long)neg;
	unsigned long long rull_big = (unsigned long long)big;
	unsigned long long rull_m1 = (unsigned long long)m1;

	if (gi_big != 2147483647) return 1;
	if (gi_big != ri_big) return 2;
	if (gi_neg != -2147483647 - 1) return 3;
	if (gi_neg != ri_neg) return 4;
	if (gu_m1 != 0u) return 5;
	if (gu_m1 != ru_m1) return 6;
	if (gu_big != 4294967295u) return 7;
	if (gu_big != ru_big) return 8;
	if (gu_neg != 0u) return 9;
	if (gu_neg != ru_neg) return 10;
	if (gll_big != 9223372036854775807LL) return 11;
	if (gll_big != rll_big) return 12;
	if (gll_neg != -9223372036854775807LL - 1) return 13;
	if (gll_neg != rll_neg) return 14;
	if (gull_big != 18446744073709551615ULL) return 15;
	if (gull_big != rull_big) return 16;
	if (gull_m1 != 0ull) return 17;
	if (gull_m1 != rull_m1) return 18;

	return 0;
}
