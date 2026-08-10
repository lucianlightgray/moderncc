#include <stdio.h>

static int hits;

void plt_target(void);

#define CLOBBERS \
	"rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory", "cc"

static void call_plain(void) { __asm__ volatile("call plt_target" ::: CLOBBERS); }

static void call_plt(void) { __asm__ volatile("call plt_target@PLT" ::: CLOBBERS); }

int main(void) {
	call_plain();
	printf("plain: %d\n", hits);
	call_plt();
	printf("plt: %d\n", hits);
	call_plt();
	call_plain();
	printf("both: %d\n", hits);
	return 0;
}

void plt_target(void) { hits += 7; }
