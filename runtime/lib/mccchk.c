extern void abort(void) __attribute__((noreturn));

void __chk_fail(void) {
	abort();
}
