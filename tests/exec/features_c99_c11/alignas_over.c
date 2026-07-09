#include <stdio.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

alignas(64) static char over64;
static alignas(32) int over32;
static alignas(alignof(max_align_t)) double md;

struct S {
	char c;
	alignas(16) int a;
};

int main(void) {
	int ok = 1;
	ok &= (alignof(max_align_t) >= alignof(long double));
	ok &= (((uintptr_t)&over64 % 64) == 0);
	ok &= (((uintptr_t)&over32 % 32) == 0);
	ok &= (((uintptr_t)&md % alignof(max_align_t)) == 0);
	ok &= (alignof(struct S) >= 16);
	ok &= ((offsetof(struct S, a) % 16) == 0);
	ok &= (_Alignof(int) == alignof(int));
	ok &= (_Alignof(double) == alignof(double));
	ok &= (alignof(struct S) >= alignof(int));
	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
