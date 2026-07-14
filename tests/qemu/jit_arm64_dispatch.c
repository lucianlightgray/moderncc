/* §26 2B — arm64 JIT dispatch tail: validates that a hand-emitted AArch64
   pointer-swap dispatch works end-to-end (the mechanism the mode-6 slot +
   stubs will use once ported into libmcc). Freestanding so it runs under
   `qemu-aarch64` with no arm64 libc. It exercises:
     - an RWX code page emitted at runtime,
     - the dispatch prologue `ldr x16, [pc+8]; br x16` reading a data slot,
     - the AArch64 instruction-cache maintenance an arm64 JIT MUST do after
       writing code (`dc cvau; dsb ish; ic ivau; dsb ish; isb`) — the reason
       the tail is a hand-emitted per-arch primitive, not shared with x86,
     - a pointer-swap that atomically redirects the slot to a new target.
   Exit code 0 = PASS. Validated encodings (llvm-mc):
     ldr x16, #8  -> 0x58000050    br x16 -> 0xd61f0200 */
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
	unsigned int i0 = 0x58000050u; /* ldr x16, #8 */
	unsigned int i1 = 0xd61f0200u; /* br x16      */
	void *t1 = (void *)target1, *t2 = (void *)target2;
	int (*disp)(int) = (int (*)(int))b;
	int r1, r2;
	if ((long)b < 0 && (long)b > -4096)
		ex(10);
	__builtin_memcpy(b + 0, &i0, 4);
	__builtin_memcpy(b + 4, &i1, 4);
	__builtin_memcpy(b + 8, &t1, 8); /* the swappable dispatch slot */
	iflush(b, 16);
	r1 = disp(5); /* -> target1(5) = 116 */
	__builtin_memcpy(b + 8, &t2, 8); /* pointer-swap the slot */
	r2 = disp(5); /* -> target2(5) = 227 */
	ex((r1 == 116 && r2 == 227) ? 0 : (100 + (r1 != 116) + ((r2 != 227) << 1)));
}
