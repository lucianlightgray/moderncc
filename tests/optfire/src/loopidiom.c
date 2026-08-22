extern int printf(const char *, ...);

/* loop-idiom (T-win-50053): counted unit-stride stores -> memset/memcpy under
 * -floop-idiom, scalar loops under -fno-loop-idiom. Covers: const-trip memset
 * (a/b/c), non-zero byte-fills (d/e), a NON-byte-replicable value that must stay
 * a scalar loop (nope), a RUNTIME trip bound via a volatile (r), and a memcpy
 * between two DISTINCT arrays (dst<-cs, sound: distinct objects never overlap).
 * The reads after each loop keep the stores live. */
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
	int cs[10];
	int dst[10];
	int f[16];
	int fs[16], fd[16];
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
	for (j = 0; j < 10; j++) cs[j] = j * 2 + 1;
	for (j = 0; j < 10; j++) dst[j] = cs[j];
	for (j = 0; j < 16; j++) f[j] = 5;
	for (j = 6; j < 16; j++) f[j] = 0;
	for (j = 0; j < 16; j++) fs[j] = j + 100;
	for (j = 0; j < 16; j++) fd[j] = 7;
	for (j = 5; j < 16; j++) fd[j] = fs[j];
	printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", a[0], (int)b[19], (int)c[11],
				 (int)d[0], (int)d[23], e[7], nope[0], nope[7], r[0], dst[0], dst[9], f[0], f[6], f[15], fd[0], fd[5], fd[15]);
	return 0;
}
