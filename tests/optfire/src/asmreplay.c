#include <stdio.h>

#ifndef ASMREPLAY_KP
#define ASMREPLAY_KP 0
#endif

#if defined(__x86_64__) || defined(__i386__)

extern int replay_sym_a;
extern int replay_sym_b[];

static int defines_a_text_symbol(int n) {
	int r = n + 1;
	__asm__ volatile("jmp .+6\n"
									 "replay_sym_a: .long 0\n");
	r = r * 2;
	return r;
}

static int defines_a_symbol_in_another_section(int n) {
	int r = n + 3;
	__asm__ volatile(".pushsection .replay_tbl, \"a\"\n"
									 ".globl replay_sym_b\n"
									 "replay_sym_b:\n"
									 ".long 11\n"
									 ".popsection\n");
	r = r * 2;
	return r;
}

static int plain_after_the_asm(int n) {
	int a = n + 1;
	int b = a * 2;
	int c = a * 2;
	return b + c;
}

int main(void) {
	int bad = 0;
	int v;

	v = defines_a_text_symbol(1);
	printf("text_symbol=%d\n", v);
	if (v != 4)
		bad++;

	v = defines_a_symbol_in_another_section(1);
	printf("other_section=%d\n", v);
	if (v != 8)
		bad++;

	v = replay_sym_b[0];
	printf("table=%d\n", v);
	if (v != 11)
		bad++;

	v = plain_after_the_asm(1);
	printf("after=%d\n", v);
	if (v != 8)
		bad++;

	if (ASMREPLAY_KP)
		printf("known-positive skew\n");

	printf("bad=%d\n", bad);
	return bad ? 3 : 0;
}

#else

int main(void) {
	printf("skip: not an x86 target\n");
	return 77;
}

#endif
