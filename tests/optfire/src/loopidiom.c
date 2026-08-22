extern int printf(const char *, ...);

/* memset loop-idiom (T-win-50053): a counted unit-stride store of a
 * byte-replicable constant to a local array -> memset under -floop-idiom, a
 * scalar loop under -fno-loop-idiom. The reads after keep the stores live so the
 * idiom actually fires. Slice 2 covers non-zero byte-replicable fills; the
 * `nope` loop (0x141 into int, NOT byte-replicable) must stay a scalar loop. */
int main(void)
{
	int a[32];
	short b[20];
	long c[12];
	unsigned char d[24];
	int e[8];
	int nope[8];
	int i;
	for (i = 0; i < 32; i++) a[i] = 0;
	for (i = 0; i < 20; i++) b[i] = 0;
	for (i = 0; i < 12; i++) c[i] = 0;
	for (i = 0; i < 24; i++) d[i] = 0x41;
	for (i = 0; i < 8; i++) e[i] = -1;
	for (i = 0; i < 8; i++) nope[i] = 0x141;
	printf("%d %d %d %d %d %d %d %d\n", a[0], (int)b[19], (int)c[11],
				 (int)d[0], (int)d[23], e[7], nope[0], nope[7]);
	return 0;
}
