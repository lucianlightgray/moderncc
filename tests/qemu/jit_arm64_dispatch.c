











static long sysc6(long n, long a, long b, long c, long d, long e, long f) {
	register long x8 asm("x8") = n;
	register long x0 asm("x0") = a, x1 asm("x1") = b, x2 asm("x2") = c;
	register long x3 asm("x3") = d, x4 asm("x4") = e, x5 asm("x5") = f;
	asm volatile("svc 0"
							 : "+r"(x0)
							 : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
							 : "memory");
	return x0;
}

static void ex(int c) { sysc6(93, c, 0, 0, 0, 0, 0); }

static void iflush(void *p, unsigned long n) {
	unsigned long a = (unsigned long)p, end = a + n, x;
	for (x = a & ~63UL; x < end; x += 64)
		asm volatile("dc cvau, %0" ::"r"(x) : "memory");
	asm volatile("dsb ish" ::: "memory");
	for (x = a & ~63UL; x < end; x += 64)
		asm volatile("ic ivau, %0" ::"r"(x) : "memory");
	asm volatile("dsb ish; isb" ::: "memory");
}

static int target1(int x) { return x + 111; }
static int target2(int x) { return x + 222; }

void _start(void) {
	unsigned char *b = (unsigned char *)sysc6(222, 0, 4096, 7, 0x22, -1, 0);
	unsigned int i0 = 0x58000050u;
	unsigned int i1 = 0xd61f0200u;
	void *t1 = (void *)target1, *t2 = (void *)target2;
	int (*disp)(int) = (int (*)(int))b;
	int r1, r2;
	if ((long)b < 0 && (long)b > -4096)
		ex(10);
	__builtin_memcpy(b + 0, &i0, 4);
	__builtin_memcpy(b + 4, &i1, 4);
	__builtin_memcpy(b + 8, &t1, 8);
	iflush(b, 16);
	r1 = disp(5);
	__builtin_memcpy(b + 8, &t2, 8);
	r2 = disp(5);
	ex((r1 == 116 && r2 == 227) ? 0 : (100 + (r1 != 116) + ((r2 != 227) << 1)));
}
