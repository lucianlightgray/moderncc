/* §26 2B — arm64 JIT counter/profiling-stub tail: validates that a hand-emitted
   AArch64 hot-counter stub works end-to-end (the mechanism the libmcc
   `mccjit_make_counter_stub` port will use, the piece after the dispatch tail in
   `jit_arm64_dispatch.c`). Freestanding so it runs under `qemu-aarch64` with no
   arm64 libc. It exercises the full x86 counter-stub semantics on AArch64:
     - spill the 6 GP arg registers (x0-x5) + LR to a frame,
     - call the C tick `long *(state, regs)` with x0=state, x1=&spilled regs,
       in the same `regs[MCCJIT_KGC_MAXARG-1-i] == param i` layout the profiler
       (`mccjit_counter_capture`) reads (regs[0]=x5 … regs[5]=x0),
     - restore x0-x5 so the returned target sees the ORIGINAL args,
     - tail `br` to the tick-returned target (its `ret` uses the restored LR, so
       control returns to the stub's caller — the arm64 analogue of x86 `jmp rax`).
   Exit code 0 = PASS. Validated encodings (llvm-mc -triple=aarch64):
     sub sp,sp,#64 ->0xd10103ff  ldr x0,#56 ->0x580001c0  mov x1,sp ->0x910003e1
     ldr x16,#56 ->0x580001d0    blr x16 ->0xd63f0200      mov x16,x0 ->0xaa0003f0
     add sp,sp,#64 ->0x910103ff  br x16 ->0xd61f0200 */
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

struct cstate {
	long captured[6];
	void *target;
};

static long addup(long a, long b, long c, long d, long e, long f) {
	return a * 1 + b * 10 + c * 100 + d * 1000 + e * 10000 + f * 100000;
}

static void *tickfn(struct cstate *st, long *regs) {
	int i;
	for (i = 0; i < 6; i++)
		st->captured[i] = regs[i];
	return st->target;
}

static const unsigned int stub_code[22] = {
		0xd10103ffu, 0xf90017e0u, 0xf90013e1u, 0xf9000fe2u, 0xf9000be3u,
		0xf90007e4u, 0xf90003e5u, 0xf9001bfeu, 0x580001c0u, 0x910003e1u,
		0x580001d0u, 0xd63f0200u, 0xaa0003f0u, 0xf9401bfeu, 0xf94017e0u,
		0xf94013e1u, 0xf9400fe2u, 0xf9400be3u, 0xf94007e4u, 0xf94003e5u,
		0x910103ffu, 0xd61f0200u};

void _start(void) {
	unsigned char *b = (unsigned char *)sysc6(222, 0, 4096, 7, 0x22, -1, 0);
	struct cstate st;
	void *sp = (void *)&st;
	void *tp = (void *)tickfn;
	long (*stub)(long, long, long, long, long, long);
	long r;
	int i;
	if ((long)b < 0 && (long)b > -4096)
		ex(10);
	st.target = (void *)addup;
	for (i = 0; i < 6; i++)
		st.captured[i] = -1;
	__builtin_memcpy(b + 0, stub_code, sizeof stub_code);
	__builtin_memcpy(b + 88, &sp, 8);
	__builtin_memcpy(b + 96, &tp, 8);
	iflush(b, 104);
	stub = (long (*)(long, long, long, long, long, long))b;
	r = stub(1, 2, 3, 4, 5, 6);
	if (r != 654321)
		ex(20);
	if (st.captured[0] != 6 || st.captured[1] != 5 || st.captured[2] != 4 ||
			st.captured[3] != 3 || st.captured[4] != 2 || st.captured[5] != 1)
		ex(30);
	ex(0);
}
