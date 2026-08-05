extern int printf(const char *, ...);

__attribute__((deprecated)) static int old_fn(void) {
	return 1;
}

__attribute__((deprecated("use new_x"))) static int old_x = 2;

static int fresh_fn(void) {
	return 4;
}

int main(void) {
	int ok = 1;

	if (old_fn() != 1)
		ok = 0;
	if (old_x != 2)
		ok = 0;
	if (fresh_fn() != 4)
		ok = 0;
	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
