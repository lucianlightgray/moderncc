/* __builtin_setjmp: the two things a library setjmp gets wrong without help
 * from the compiler. Both were live defects, found by the GCC torture corpus
 * (pr60003.c, pr84521.c) on the first full-suite run after the builtin landed.
 * That corpus is optional here -- it is a symlink to an upstream gcc checkout
 * and the cell skips without one -- so the coverage lives in-tree as well.
 *
 * MCC-ONLY. gcc's __builtin_setjmp has restricted semantics that need compiler
 * support to use safely from C, and the same program does not build or does not
 * behave under it; the shared oracle for this file is _setjmp, asserted
 * alongside in builtin_mcc_ext.c.
 */

extern int printf(const char *, ...);

static int fails;
#define CK(cond)                            \
	do {                                      \
		if (!(cond)) {                          \
			printf("FAIL line %d\n", __LINE__);   \
			fails++;                              \
		}                                       \
	} while (0)

#if defined __MCC__ && !defined _WIN32
#define SJ_PROBE 1

/* ---- 1. a local assigned between the setjmp and the longjmp -------------
 * The assignment happens after the setjmp returns 0. If the local is living in
 * a callee-saved register, longjmp restores that register to its setjmp-time
 * value and the assignment is lost -- the local reads back 0 and this returns
 * 0 instead of x. Six of them, because the promotion pass ranks candidates and
 * one is easy to miss.
 */
static void *nl_buf[5];
static int nl_depth;

__attribute__((noinline)) static void nl_jump(void) {
	__builtin_longjmp(nl_buf, 1);
}

/* Several frames deep, so the longjmp unwinds more than its own caller and the
 * intervening frames' callee-saved saves are abandoned. */
__attribute__((noinline)) static void nl_deep(int n) {
	if (n == 0)
		nl_jump();
	nl_deep(n - 1);
	nl_depth++; /* never reached; keeps the call from being a tail call */
}

__attribute__((noinline)) static int nl_locals(int x) {
	int a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
	if (__builtin_setjmp(nl_buf) == 0) {
		a = 1;
		b = 2;
		c = 3;
		d = 4;
		e = 5;
		f = 6;
		nl_deep(4);
		return -1; /* unreachable */
	}
	if (a != 1 || b != 2 || c != 3 || d != 4 || e != 5 || f != 6)
		return 0;
	return x;
}

/* ---- 2. an UNINITIALIZED local buffer -----------------------------------
 * GCC's buffer is a bare `void *buf[5]`, and every program that uses it
 * declares one as an ordinary local. Nothing may read it before setjmp writes
 * it: an implementation that stashes state in buf[0] and tests it for zero
 * reads stack garbage and stores through it.
 *
 * The dirty_stack() call first fills the frame this buffer will occupy with
 * non-zero bytes, so "uninitialized" here is not quietly zero.
 */
__attribute__((noinline)) static void dirty_stack(void) {
	volatile long junk[64];
	int i;
	for (i = 0; i < 64; i++)
		junk[i] = 0x5a5a5a5a5a5a5a5aL;
}

__attribute__((noinline)) static void local_jump(void *p) {
	__builtin_longjmp(p, 1);
}

__attribute__((noinline)) static int nl_local_buf(void) {
	void *buf[5]; /* deliberately uninitialized */
	volatile int reached = 0;
	if (!__builtin_setjmp(buf)) {
		reached = 1;
		local_jump(buf);
		return -1; /* unreachable */
	}
	return reached == 1 ? 7 : 0;
}

/* Two distinct live buffers, to catch a slot table that hands both the same
 * save area or that keys on something the two share. */
static void *buf_a[5], *buf_b[5];
__attribute__((noinline)) static int two_buffers(void) {
	int ra, rb;
	ra = __builtin_setjmp(buf_a);
	if (ra == 0) {
		rb = __builtin_setjmp(buf_b);
		if (rb == 0)
			__builtin_longjmp(buf_b, 1);
		__builtin_longjmp(buf_a, 1);
	}
	return ra;
}
#endif

int main(void) {
#ifdef SJ_PROBE
	CK(nl_locals(42) == 42);
	/* Run it twice: the second setjmp on the same buffer must find its own
	 * save area rather than allocate a second one and leak. */
	CK(nl_locals(42) == 42);
	CK(nl_local_buf() == 7);
	CK(nl_local_buf() == 7);
	CK(two_buffers() == 1);
#endif
	printf(fails ? "FAIL\n" : "OK\n");
	return fails ? 1 : 0;
}
