static jmp_buf s7_13_jb0;
static jmp_buf s7_13_jb42;
static volatile sig_atomic_t s7_13_sigflag = 0;
static void s7_13_handler(int sig) {
	(void)sig;
	s7_13_sigflag = 1;
}
static _Alignas(32) unsigned char s7_13_buf[32];
static _Alignas(double) unsigned char s7_13_da[1];

void s7_13_setjmp_signal_align(void) {

	volatile int v = 0;
	int r0 = setjmp(s7_13_jb0);
	if (r0 == 0) {
		v = 5;
		longjmp(s7_13_jb0, 0);
	}
	printf("s7_13 setjmp val0 -> r=%d v=%d\n", r0, (int)v);

	int r42 = setjmp(s7_13_jb42);
	if (r42 == 0)
		longjmp(s7_13_jb42, 42);
	printf("s7_13 setjmp val42 -> r=%d\n", r42);

#if !defined(_WIN32)
	void (*prev)(int) = signal(SIGUSR1, s7_13_handler);
	s7_13_sigflag = 0;
	int rc = raise(SIGUSR1);
	printf("s7_13 signal flag=%d rc0=%d\n", (int)s7_13_sigflag, rc == 0);
	void (*prev2)(int) = signal(SIGUSR1, prev);
	printf("s7_13 signal prev_returned=%d\n", prev2 == s7_13_handler);
#endif

	printf("s7_13 stdalign defs=%d\n",
		   __alignas_is_defined == 1 && __alignof_is_defined == 1);

	printf("s7_13 _Alignof int matches=%d\n",
		   _Alignof(int) == __alignof__(int));
	printf("s7_13 _Alignof array=elem=%d\n",
		   _Alignof(double[10]) == _Alignof(double));

	printf("s7_13 _Alignas typename=ice=%d\n",
		   _Alignof(s7_13_da) == _Alignof(double));

	printf("s7_13 _Alignas32 aligned=%d\n",
		   ((uintptr_t)(void *)s7_13_buf) % 32u == 0);
}
