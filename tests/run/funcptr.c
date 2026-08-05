#include <stdint.h>
#include <stdio.h>

typedef int32_t (*fn)(int32_t);

static int32_t f1(int32_t x) { return x + 1; }
static int32_t f2(int32_t x) { return x * 2; }
static int32_t f3(int32_t x) { return x - 3; }

static fn tbl[3] = {f1, f2, f3};

extern int32_t aux_apply(fn f, int32_t x);
extern fn aux_pick(int i);
extern int32_t aux_chain(fn f, fn g, int32_t x);

int32_t exported_double(int32_t x) { return x + x; }

int main(void) {
	int i;
	int32_t acc = 0;
	fn local = f3;
	for (i = 0; i < 3; i++)
		acc += tbl[i](10);
	printf("tbl=%d\n", acc);
	printf("apply=%d\n", aux_apply(f2, 21));
	printf("pick=%d\n", aux_pick(1)(9));
	printf("same=%d\n", aux_pick(0) == exported_double);
	printf("chain=%d\n", aux_chain(f1, f2, 20));
	printf("local=%d\n", local(50));
	printf("eq=%d\n", tbl[0] == f1);
	printf("ne=%d\n", tbl[0] == f2);
	return 0;
}
