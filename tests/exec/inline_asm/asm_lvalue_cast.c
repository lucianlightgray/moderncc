extern int printf(const char *, ...);

int main(void) {
	unsigned x = 5, r = 0;
	__asm__("movl %1, %0" : "=r"((unsigned)(r)) : "r"((unsigned)(x)));
	printf(r == 5 ? "OK\n" : "FAIL\n");
	return 0;
}
