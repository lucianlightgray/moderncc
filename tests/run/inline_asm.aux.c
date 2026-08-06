#if defined(__x86_64__) || defined(__i386__)
#define MCC_RUN_ASM 1
#endif

int aux_asm_addv(int x, int y) {
#ifdef MCC_RUN_ASM
	int r;
	__asm__("addl %2,%0" : "=r"(r) : "0"(x), "r"(y));
	return r;
#else
	return x + y;
#endif
}

int aux_asm_has_asm(void) {
#ifdef MCC_RUN_ASM
	return 1;
#else
	return 0;
#endif
}
