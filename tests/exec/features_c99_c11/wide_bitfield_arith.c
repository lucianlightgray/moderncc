#include <stdio.h>

struct s {
	unsigned long long u33 : 33;
	unsigned long long u40 : 40;
	unsigned long long u41 : 41;
};

struct foo {
	unsigned long long b : 40;
};

static struct s a = {0x100000, 0x100000, 0x100000};
static struct s b = {0x100000000ULL, 0x100000000ULL, 0x100000000ULL};

int main(void) {
	int fail = 0;

	if (a.u33 * a.u33 != 0 || a.u40 * a.u40 != 0 || a.u33 * a.u40 != 0)
		fail = 1;
	if (a.u33 * a.u41 != 0x10000000000ULL || a.u41 * a.u33 != 0x10000000000ULL)
		fail = 1;
	if (a.u41 * a.u41 != 0x10000000000ULL)
		fail = 1;

	if (b.u33 + b.u33 != 0)
		fail = 1;
	if (b.u33 + b.u40 != 0x200000000ULL || b.u40 + b.u33 != 0x200000000ULL)
		fail = 1;

	struct foo x;
	x.b = 0x0100;
	if ((x.b << 32) != 0)
		fail = 1;
	x.b = 0x0100000001ULL;
	if (((x.b << 8) + (x.b >> 32)) != 0x101ULL)
		fail = 1;
	x.b = 0x0100000000ULL;
	if (((x.b << 8) + (x.b >> 32)) != 0x1ULL)
		fail = 1;

	/* T-lin-10078: `/` and `%` on over-wide bit-fields reduce each OPERAND to N
	   bits before the op (they are non-modular, unlike + - * <<), matching gcc's
	   precision-N model.  The `/ -1` shape is discriminating: unreduced, -1 is
	   2^64-1 and the quotient collapses to 0; reduced to N bits it is 2^N-1 and
	   the answer is 1.  Both the constant-folded and runtime paths are checked.
	   wants verified against gcc-16. */
	{
		struct { unsigned long long f : 33; } cu = {(1ULL << 33) - 1};
		volatile struct { unsigned long long f : 33; } vu = {(1ULL << 33) - 1};
		if (cu.f / -1 != 1 || cu.f % -1 != 0)
			fail = 1;
		if (vu.f / -1 != 1 || vu.f % -1 != 0)
			fail = 1;
	}
	{
		volatile struct { unsigned long long f : 40; } u40 = {(1ULL << 40) - 1};
		volatile struct { unsigned long long f : 63; } u63 = {(1ULL << 63) - 1};
		volatile struct { unsigned long long f : 64; } u64 = {~0ULL};
		if (u40.f / -1 != 1 || u40.f % -1 != 0)
			fail = 1;
		if (u63.f / -1 != 1 || u64.f / -1 != 1)
			fail = 1;
	}
	{
		signed long long init = (1LL << 32) - 1; /* max positive 33-bit signed */
		struct { signed long long g : 33; } cs = {init};
		volatile struct { signed long long g : 33; } vs = {init};
		if (cs.g / -1 != -4294967295LL || cs.g % -1 != 0)
			fail = 1;
		if (vs.g / -1 != -4294967295LL || vs.g % -1 != 0)
			fail = 1;
	}

	puts(fail ? "FAIL" : "OK");
	return fail;
}
