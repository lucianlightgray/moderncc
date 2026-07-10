extern int printf(const char *, ...);

static long g;

static int id_int(int i) {
	g += 1;
	return (int)i;
}

static int widen_narrow(int i) {
	return (int)(long)i;
}

static int triple(int i) {
	return (int)(long)(int)i;
}

static unsigned uwiden_narrow(unsigned u) {
	return (unsigned)(unsigned long)u;
}

static long real_widen(int i) {
	return (long)i;
}

static int real_narrow(long l) {
	return (int)l;
}

static int short_roundtrip(short s) {
	return (int)(long)(short)s;
}

static int mixed(int a, int b) {
	long wa = (long)a;
	int na = (int)wa;
	unsigned ub = (unsigned)b;
	unsigned nub = (unsigned)(unsigned long)ub;
	return na + (int)nub;
}

int main(void) {
	unsigned long long chk = 0;
	chk = chk * 31 + id_int(7);
	chk = chk * 31 + id_int(-3);
	chk = chk * 31 + widen_narrow(11);
	chk = chk * 31 + widen_narrow(-9);
	chk = chk * 31 + triple(123);
	chk = chk * 31 + (int)uwiden_narrow(4000000000u);
	chk = chk * 31 + (int)real_widen(1000000);
	chk = chk * 31 + real_narrow(0x1122334455667788L);
	chk = chk * 31 + short_roundtrip(-1234);
	chk = chk * 31 + mixed(50, -8);
	chk = chk * 31 + real_narrow(-1L);
	printf("chk=%llu g=%ld\n", chk, g);
	printf("OK\n");
	return 0;
}
