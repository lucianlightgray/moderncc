/* §26 2B — arm64 JIT KGC differential-verify stub tail: validates that a
   hand-emitted AArch64 known-good-cache stub works end-to-end (the mechanism the
   libmcc `mccjit_make_kgc_stub_n` port will use, the last data-path stub before
   the interdependent libmcc port). Freestanding so it runs under `qemu-aarch64`
   with no arm64 libc. It exercises the x86 KGC-stub marshalling semantics on
   AArch64 for a mixed wide/narrow 3-arg signature (long, int, long):
     - gather the incoming arg registers into a FORWARD-order argv array on the
       frame (argv[i] == arg i, unlike the reversed counter-stub layout), with
       narrow (32-bit) args sign-extended (`sxtw`) — the arm64 analogue of the
       x86 `movsxd`; a zero-extend would corrupt a negative arg and fail the
       arithmetic assert below,
     - call the C verifier `long (kgc, variant, baseline, argv, nargs, *flagged)`
       (x0..x5) that runs both variant + baseline, compares, sets *flagged on a
       mismatch, and returns the baseline (deopt) result,
     - return the verifier's result to the caller.
   Validated here as a directly-callable unit; the mode-6 dispatch entry that
   jumps into it is validated separately by `jit_arm64_dispatch.c`. Exit 0 = PASS.
   Encodings (llvm-mc -triple=aarch64): stp x29,x30,[sp,#-16]! ->0xa9bf7bfd
     sxtw x8,w1 ->0x93407c28  mov x3,sp ->0x910003e3  mov w4,#3 ->0x52800064
     blr x16 ->0xd63f0200  ldp x29,x30,[sp],#16 ->0xa8c17bfd  ret ->0xd65f03c0 */
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

static long variant_good(long a, long b, long c) { return a + b * 2 + c * 3; }
static long baseline_fn(long a, long b, long c) { return a + b * 2 + c * 3; }
static long variant_bad(long a, long b, long c) { return a + b * 2 + c * 3 + 1; }

static long verifyfn(void *kgc, void *variant, void *baseline, long *argv,
										 unsigned nargs, int *flagged) {
	long (*vf)(long, long, long) = (long (*)(long, long, long))variant;
	long (*bf)(long, long, long) = (long (*)(long, long, long))baseline;
	long vv, bv;
	(void)kgc;
	(void)nargs;
	vv = vf(argv[0], argv[1], argv[2]);
	bv = bf(argv[0], argv[1], argv[2]);
	if (vv != bv) {
		*flagged = 1;
		return bv;
	}
	return vv;
}

static const unsigned int stub_code[18] = {
		0xa9bf7bfdu, 0xd100c3ffu, 0xf90003e0u, 0x93407c28u, 0xf90007e8u,
		0xf9000be2u, 0x58000180u, 0x580001a1u, 0x580001c2u, 0x910003e3u,
		0x52800064u, 0x580001a5u, 0x580001d0u, 0xd63f0200u, 0x9100c3ffu,
		0xa8c17bfdu, 0xd65f03c0u, 0xd503201fu};

static void emit(unsigned char *b, void *kgc, void *variant, void *baseline,
								 void *flag, void *verify) {
	__builtin_memcpy(b + 0, stub_code, sizeof stub_code);
	__builtin_memcpy(b + 72, &kgc, 8);
	__builtin_memcpy(b + 80, &variant, 8);
	__builtin_memcpy(b + 88, &baseline, 8);
	__builtin_memcpy(b + 96, &flag, 8);
	__builtin_memcpy(b + 104, &verify, 8);
	iflush(b, 112);
}

void _start(void) {
	unsigned char *b = (unsigned char *)sysc6(222, 0, 4096, 7, 0x22, -1, 0);
	int dummy_kgc = 0;
	int flag = 0;
	long (*stub)(long, int, long) = (long (*)(long, int, long))b;
	long r;
	if ((long)b < 0 && (long)b > -4096)
		ex(10);

	emit(b, &dummy_kgc, (void *)variant_good, (void *)baseline_fn, &flag,
			 (void *)verifyfn);
	r = stub(10, -5, 7);
	if (r != 21)
		ex(20);
	if (flag != 0)
		ex(21);

	flag = 0;
	emit(b, &dummy_kgc, (void *)variant_bad, (void *)baseline_fn, &flag,
			 (void *)verifyfn);
	r = stub(10, -5, 7);
	if (r != 21)
		ex(30);
	if (flag != 1)
		ex(31);

	ex(0);
}
