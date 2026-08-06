#include <stdio.h>

extern int aux_asm_addv(int x, int y);
extern int aux_asm_has_asm(void);

#if defined(__x86_64__) || defined(__i386__)
#define MCC_RUN_ASM 1
#endif

static int addv(int x, int y) {
#ifdef MCC_RUN_ASM
	int r;
	__asm__("addl %2,%0" : "=r"(r) : "0"(x), "r"(y));
	return r;
#else
	return x + y;
#endif
}

static int rwop(int x) {
#ifdef MCC_RUN_ASM
	__asm__("addl $7,%0" : "+r"(x));
	return x;
#else
	return x + 7;
#endif
}

static int memclob(int *p) {
#ifdef MCC_RUN_ASM
	__asm__ __volatile__("incl (%0)" ::"r"(p) : "memory");
	return *p;
#else
	*p += 1;
	return *p;
#endif
}

static int gotoish(int x) {
#ifdef MCC_RUN_ASM
	__asm__ goto("testl %0,%0; jz %l[zero]" ::"r"(x)::zero);
	return 1;
zero:
	return 0;
#else
	return x ? 1 : 0;
#endif
}

static int looped(int n) {
	int acc = 0;
	int v = 1;
	int i;
	for (i = 0; i < n; i++) {
#ifdef MCC_RUN_ASM
		__asm__("addl $2,%0" : "+r"(v));
#else
		v += 2;
#endif
		acc += v;
	}
	return acc;
}

static void fence(void) {
#ifdef MCC_RUN_ASM
	__asm__ __volatile__("" ::: "memory");
#endif
}

int main(void) {
	int cell = 41;
	fence();
	printf("addv=%d\n", addv(20, 22));
	printf("rwop=%d\n", rwop(35));
	printf("memclob=%d\n", memclob(&cell));
	printf("goto0=%d\n", gotoish(0));
	printf("goto1=%d\n", gotoish(9));
	printf("looped=%d\n", looped(3));
	printf("aux=%d\n", aux_asm_addv(19, 23));
	return aux_asm_has_asm() ? 0 : 0;
}
