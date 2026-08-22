extern int printf(const char *, ...);

/* memset loop-idiom (T-win-50053): a counted unit-stride store of a
 * byte-replicable constant to an array -> memset under -floop-idiom, a scalar
 * loop under -fno-loop-idiom. Covers: const trip (a/b/c), non-zero byte-fills
 * (d/e), a NON-byte-replicable value that must stay a scalar loop (nope), and a
 * RUNTIME trip bound driven by a volatile so the runtime `(size_t)n*elemsize`
 * length path fires (r). The reads after each loop keep the stores live. */
volatile unsigned g_n = 16;

int main(void)
{
	int a[32];
	short b[20];
	long c[12];
	unsigned char d[24];
	int e[8];
	int nope[8];
	int r[16];
	unsigned n = g_n;
	unsigned i;
	int j;
	for (j = 0; j < 32; j++) a[j] = 0;
	for (j = 0; j < 20; j++) b[j] = 0;
	for (j = 0; j < 12; j++) c[j] = 0;
	for (j = 0; j < 24; j++) d[j] = 0x41;
	for (j = 0; j < 8; j++) e[j] = -1;
	for (j = 0; j < 8; j++) nope[j] = 0x141;
	for (i = 0; i < n; i++) r[i] = 0;
	printf("%d %d %d %d %d %d %d %d %d\n", a[0], (int)b[19], (int)c[11],
				 (int)d[0], (int)d[23], e[7], nope[0], nope[7], r[0]);
	return 0;
}
