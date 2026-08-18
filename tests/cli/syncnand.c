extern int printf(const char *, ...);

int main(void) {
	int x = 12;
	int fo = __sync_fetch_and_nand(&x, 10);

	long long q = 0x0F0F0F0F0F0F0F0FLL;
	long long expect_q = ~0x000F000F000F000FLL;
	long long nf = __sync_nand_and_fetch(&q, 0x00FF00FF00FF00FFLL);

	int ok = fo == 12 && x == -9 && nf == expect_q && q == expect_q;
	printf(ok ? "OK\n" : "FAIL fo=%d x=%d nf=%lld q=%lld\n", fo, x, nf, q);
	return ok ? 0 : 1;
}
