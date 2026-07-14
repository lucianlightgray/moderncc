/* §26 2B — arm64 JIT KGC differential-verify stub, all-double (SSE-class) tail:
   validates the hand-emitted AArch64 FP known-good-cache stub the libmcc
   `mccjit_make_kgc_stub_fp` port will use (the K4A scalar all-double marshalling
   path, the 4th and last data-path stub mechanism). Freestanding so it runs
   under `qemu-aarch64` with no arm64 libc. For a 2-`double` signature it:
     - spills the FP arg registers (d0-d1, the AArch64 analogue of the x86
       `movsd` xmm0-7 spill) as doubles into an argv frame,
     - calls the C verifier `double (kgc, variant, baseline, argv, nargs,
       *flagged)` with the GP arg regs (x0-x5) — the double argv is passed by
       POINTER (x3), not in FP regs, exactly as `mccjit_kgc_calln_fp` expects,
     - the verifier runs both variant + baseline, compares RAW return bits, sets
       *flagged on a mismatch, and returns the baseline (deopt) double,
     - the stub returns leaving d0 untouched, so the verifier's double return
       propagates to the caller (the x86 stub relies on the same xmm0 pass-through).
   Exit 0 = PASS. Encodings (llvm-mc -triple=aarch64): str d0,[sp] ->0xfd0003e0
     str d1,[sp,#8] ->0xfd0007e1  mov w4,#2 ->0x52800044  blr x16 ->0xd63f0200 */
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

static double variant_good(double a, double b) { return a * 2.0 + b; }
static double baseline_fp(double a, double b) { return a * 2.0 + b; }
static double variant_bad(double a, double b) { return a * 2.0 + b + 1.0; }

static double verifyfn_fp(void *kgc, void *variant, void *baseline, double *argv,
													unsigned nargs, int *flagged) {
	double (*vf)(double, double) = (double (*)(double, double))variant;
	double (*bf)(double, double) = (double (*)(double, double))baseline;
	double vv, bv;
	unsigned long vb, bb;
	(void)kgc;
	(void)nargs;
	vv = vf(argv[0], argv[1]);
	bv = bf(argv[0], argv[1]);
	__builtin_memcpy(&vb, &vv, 8);
	__builtin_memcpy(&bb, &bv, 8);
	if (vb != bb) {
		*flagged = 1;
		return bv;
	}
	return vv;
}

static const unsigned int stub_code[16] = {
		0xa9bf7bfdu, 0xd100c3ffu, 0xfd0003e0u, 0xfd0007e1u, 0x58000180u,
		0x580001a1u, 0x580001c2u, 0x910003e3u, 0x52800044u, 0x580001a5u,
		0x580001d0u, 0xd63f0200u, 0x9100c3ffu, 0xa8c17bfdu, 0xd65f03c0u,
		0xd503201fu};

static void emit(unsigned char *b, void *kgc, void *variant, void *baseline,
								 void *flag, void *verify) {
	__builtin_memcpy(b + 0, stub_code, sizeof stub_code);
	__builtin_memcpy(b + 64, &kgc, 8);
	__builtin_memcpy(b + 72, &variant, 8);
	__builtin_memcpy(b + 80, &baseline, 8);
	__builtin_memcpy(b + 88, &flag, 8);
	__builtin_memcpy(b + 96, &verify, 8);
	iflush(b, 104);
}

void _start(void) {
	unsigned char *b = (unsigned char *)sysc6(222, 0, 4096, 7, 0x22, -1, 0);
	int dummy_kgc = 0;
	int flag = 0;
	double (*stub)(double, double) = (double (*)(double, double))b;
	double r;
	if ((long)b < 0 && (long)b > -4096)
		ex(10);

	emit(b, &dummy_kgc, (void *)variant_good, (void *)baseline_fp, &flag,
			 (void *)verifyfn_fp);
	r = stub(1.5, 2.25);
	if (r != 5.25)
		ex(20);
	if (flag != 0)
		ex(21);

	flag = 0;
	emit(b, &dummy_kgc, (void *)variant_bad, (void *)baseline_fp, &flag,
			 (void *)verifyfn_fp);
	r = stub(1.5, 2.25);
	if (r != 5.25)
		ex(30);
	if (flag != 1)
		ex(31);

	ex(0);
}
