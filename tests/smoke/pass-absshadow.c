extern int printf(const char *, ...);

#define SMP_INT_MIN (-2147483647 - 1)
#define SMP_INT_MAX 2147483647
#define SMP_LONG_MIN (-9223372036854775807L - 1)
#define SMP_LONG_MAX 9223372036854775807L
#define SMP_LLONG_MIN (-9223372036854775807LL - 1)
#define SMP_LLONG_MAX 9223372036854775807LL

int abs(int v) { return v == SMP_INT_MIN ? SMP_INT_MAX : (v < 0 ? -v : v); }

long labs(long v)
{
	if (sizeof(long) == 8)
		return v == SMP_LONG_MIN ? SMP_LONG_MAX : (v < 0 ? -v : v);
	return v == (long)SMP_INT_MIN ? (long)SMP_INT_MAX : (v < 0 ? -v : v);
}

long long llabs(long long v)
{
	return v == SMP_LLONG_MIN ? SMP_LLONG_MAX : (v < 0 ? -v : v);
}

int main(void)
{
	volatile int i = SMP_INT_MIN;
	volatile long l = sizeof(long) == 8 ? SMP_LONG_MIN : (long)SMP_INT_MIN;
	volatile long long ll = SMP_LLONG_MIN;
	long lwant = sizeof(long) == 8 ? SMP_LONG_MAX : (long)SMP_INT_MAX;
	int ci = abs(SMP_INT_MIN);
	long long cll = llabs(SMP_LLONG_MIN);

	printf("absshadow %d %lld %d %d %lld\n", abs(i), llabs(ll),
				 labs(l) == lwant, ci, cll);
	return 0;
}
