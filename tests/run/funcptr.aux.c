#include <stdint.h>

typedef int32_t (*fn)(int32_t);

extern int32_t exported_double(int32_t x);

static int32_t aux_neg(int32_t x) { return -x; }

static fn atbl[2];

int32_t aux_apply(fn f, int32_t x) { return f(x); }

fn aux_pick(int i) {
	atbl[0] = exported_double;
	atbl[1] = aux_neg;
	return atbl[i & 1];
}

int32_t aux_chain(fn f, fn g, int32_t x) { return g(f(x)); }
