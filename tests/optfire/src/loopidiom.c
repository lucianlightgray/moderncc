extern int printf(const char *, ...);

/* memset loop-idiom (T-win-50053): a counted unit-stride store of 0 to a local
 * array -> memset under -floop-idiom, a scalar loop under -fno-loop-idiom. The
 * reads after the zeroing keep the store live so the idiom actually fires. */
int main(void)
{
	int a[32];
	short b[20];
	long c[12];
	int i;
	for (i = 0; i < 32; i++) a[i] = 0;
	for (i = 0; i < 20; i++) b[i] = 0;
	for (i = 0; i < 12; i++) c[i] = 0;
	printf("%d %d %d %d %d %d\n", a[0], a[31], (int)b[0], (int)b[19],
				 (int)c[0], (int)c[11]);
	return 0;
}
