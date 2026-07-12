extern int printf(const char *, ...);

static unsigned udiv(unsigned x, unsigned y) {
	return (unsigned)((unsigned long long)x / (unsigned long long)y);
}

static unsigned umod(unsigned x, unsigned y) {
	return (unsigned)((unsigned long long)x % (unsigned long long)y);
}

static int shl3(int x) {
	return (int)((long long)x << 3);
}

static unsigned shl_uc(unsigned x, int c) {
	return (unsigned)((unsigned long long)x << c);
}

static unsigned div_high(unsigned long long a, unsigned b) {
	return (unsigned)(a / b);
}

static int shl_wide(long long a, int c) {
	return (int)(a << c);
}

int main(void) {
	volatile unsigned vx = 4000000000u, vy = 7u;
	volatile int vs = -1234567;
	volatile unsigned long long va = 0x100000000ULL;
	volatile long long vw = 1;
	unsigned long long chk = 0;
	chk = chk * 31 + udiv(vx, vy);
	chk = chk * 31 + umod(vx, vy);
	chk = chk * 31 + udiv(0xFFFFFFFFu, 6u);
	chk = chk * 31 + umod(0xFFFFFFFFu, 6u);
	chk = chk * 31 + (unsigned)shl3(vs);
	chk = chk * 31 + shl_uc(vx, 5);
	chk = chk * 31 + shl_uc(vx, 31);
	chk = chk * 31 + div_high(va, 3u);
	chk = chk * 31 + (unsigned)shl_wide(vw, 40);
	chk = chk * 31 + (unsigned)shl_wide(vw, 3);
	printf("chk=%llu\n", chk);
	printf("OK\n");
	return 0;
}
