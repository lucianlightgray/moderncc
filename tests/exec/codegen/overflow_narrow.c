extern int printf(const char *, ...);
static unsigned long long seed = 1234567891011ull;
static unsigned long long xs(void){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
#define CHK(T, W, EXPR_OV, REF) do { \
	T r1, r2; long long w; unsigned long long uw; (void)w; (void)uw; \
	int ov = (EXPR_OV); int rf = (REF); \
	if (ov != rf || r1 != r2) { printf("bad %s a=%lld b=%lld ov=%d ref=%d r1=%lld r2=%lld\n", #T, (long long)a, (long long)b, ov, rf, (long long)r1, (long long)r2); return 1; } \
} while (0)
int main(void) {
	int i;
	for (i = 0; i < 4000; i++) {
		unsigned long long v = xs();
		long long a = (long long)(int)(v >> 3), b = (long long)(int)(v >> 35);
		if (i % 4 == 1) { a = (long long)(signed char)v; b = (long long)(signed char)(v >> 8); }
		if (i % 4 == 2) { a = (long long)(short)v; b = (long long)(short)(v >> 16); }
		if (i % 4 == 3) { a = (long long)v; b = (long long)(v >> 32); }
		{ signed char r1, r2; long long w = a + b; r2 = (signed char)w;
		  if (__builtin_add_overflow(a, b, &r1) != (w != (long long)r2) || r1 != r2) { printf("sc %d\n", i); return 1; } }
		{ short r1, r2; long long w = a - b; r2 = (short)w;
		  if (__builtin_sub_overflow(a, b, &r1) != (w != (long long)r2) || r1 != r2) { printf("sh %d\n", i); return 2; } }
		{ int r1, r2; long long w = a + b; r2 = (int)w;
		  if (__builtin_add_overflow(a, b, &r1) != (w != (long long)r2) || r1 != r2) { printf("i %d a=%lld b=%lld\n", i, a, b); return 3; } }
		{ unsigned r1, r2; unsigned long long ua = (unsigned long long)a, ub = (unsigned long long)b, uw = ua + ub; r2 = (unsigned)uw;
		  if (__builtin_add_overflow(ua, ub, &r1) != (uw != (unsigned long long)r2 || uw < ua) || r1 != r2) { printf("u %d\n", i); return 4; } }
		{ unsigned char r1, r2; unsigned long long ua = (unsigned long long)a, ub = (unsigned long long)b, uw = ua + ub; r2 = (unsigned char)uw;
		  if (__builtin_add_overflow(ua, ub, &r1) != (uw != (unsigned long long)r2 || uw < ua) || r1 != r2) { printf("uc %d\n", i); return 5; } }
	}
	printf("OK\n");
	return 0;
}
