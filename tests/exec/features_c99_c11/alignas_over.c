#include <stdio.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

alignas(64) static char over64;
static alignas(32) int over32;
static alignas(alignof(max_align_t)) double md;

_Alignas(0) static int noeffect;
_Alignas(double) static char cd;
static alignas(16) long double sld;

struct S {
	char c;
	alignas(16) int a;
};

struct T {
	char c;
	_Alignas(double) char d;
	int e;
};

union U {
	char c;
	double d;
	long long l;
};

struct AElem {
	alignas(16) int v[2];
};

static struct AElem elems[4];

static int arr_sized[_Alignof(double) > 0 ? 3 : -1];

_Static_assert(_Alignof(int) >= 1, "int alignment");
_Static_assert(_Alignof(long double) >= _Alignof(char), "long double alignment");
_Static_assert(alignof(struct S) >= 16, "struct S over-aligned");

static int is_pow2(size_t x) {
	return x != 0 && (x & (x - 1)) == 0;
}

int main(void) {
	int ok = 1;

	ok &= (alignof(char) == 1);
	ok &= is_pow2(alignof(short));
	ok &= is_pow2(alignof(int));
	ok &= is_pow2(alignof(long));
	ok &= is_pow2(alignof(long long));
	ok &= is_pow2(alignof(float));
	ok &= is_pow2(alignof(double));
	ok &= is_pow2(alignof(long double));
	ok &= is_pow2(alignof(void *));
	ok &= is_pow2(alignof(struct S));
	ok &= is_pow2(alignof(union U));

	ok &= (alignof(max_align_t) >= alignof(long double));
	ok &= (alignof(max_align_t) >= alignof(double));

	ok &= (alignof(int[10]) == alignof(int));
	ok &= (alignof(double[4]) == alignof(double));
	ok &= (alignof(union U) >= alignof(double));

	ok &= (((uintptr_t)&over64 % 64) == 0);
	ok &= (((uintptr_t)&over32 % 32) == 0);
	ok &= (((uintptr_t)&md % alignof(max_align_t)) == 0);
	ok &= (((uintptr_t)&sld % 16) == 0);

	ok &= (alignof(noeffect) == alignof(int));
	ok &= (alignof(cd) == alignof(double));

	ok &= (alignof(struct S) >= 16);
	ok &= (alignof(struct S) >= alignof(int));
	ok &= ((offsetof(struct S, a) % 16) == 0);
	ok &= (alignof(struct T) >= alignof(double));
	ok &= ((offsetof(struct T, d) % alignof(double)) == 0);

	ok &= (_Alignof(int) == alignof(int));
	ok &= (_Alignof(double) == alignof(double));

	alignas(16) int loc16;
	alignas(16) char locbuf[40];
	static alignas(64) int locstatic;
	ok &= (((uintptr_t)&loc16 % 16) == 0);
	ok &= (((uintptr_t)locbuf % 16) == 0);
	ok &= (((uintptr_t)&locstatic % 64) == 0);

	ok &= (alignof(struct AElem) >= 16);
	for (int i = 0; i < 4; i++)
		ok &= (((uintptr_t)&elems[i].v[0] % 16) == 0);
	ok &= (((uintptr_t)&elems[2] % 16) == 0);
	ok &= ((sizeof elems[0]) % 16 == 0);

	ok &= ((int)(sizeof arr_sized / sizeof arr_sized[0]) == 3);

	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
