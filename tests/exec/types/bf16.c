#include <stdio.h>
#include <string.h>

typedef unsigned short u16;
typedef unsigned int u32;

static u16 hb(__bf16 h) {
	u16 r;
	memcpy(&r, &h, 2);
	return r;
}

static __bf16 mkh(u16 b) {
	__bf16 h;
	memcpy(&h, &b, 2);
	return h;
}

static float mkf(u32 b) {
	float f;
	memcpy(&f, &b, 4);
	return f;
}

static __bf16 addbf(__bf16 x, __bf16 y) {
	return x + y;
}

static __bf16 mulbf(__bf16 x, __bf16 y) {
	return x * y;
}

struct SBI {
	__bf16 a;
	int tag;
};

int main(void) {
	int fail = 0;

	if (sizeof(__bf16) != 2)
		fail = 1;

	/* encode: pi rounds to 0x4049 (round to nearest even) */
	if (hb((__bf16)3.14159f) != 0x4049)
		fail = 1;

	/* decode is exact: bf16 is the high half of the float */
	if ((float)mkh(0x4049) != 3.140625f)
		fail = 1;

	/* round-to-nearest-even on a tie: 0x3f808000 -> 0x3f80 (down to even) */
	if (hb((__bf16)mkf(0x3f808000u)) != 0x3f80)
		fail = 1;
	/* 0x3f818000 -> 0x3f82 (up to even) */
	if (hb((__bf16)mkf(0x3f818000u)) != 0x3f82)
		fail = 1;

	/* inf, -inf, nan survive; max float rounds to inf */
	if (hb((__bf16)mkf(0x7f800000u)) != 0x7f80)
		fail = 1;
	if (hb((__bf16)mkf(0xff800000u)) != 0xff80)
		fail = 1;
	if (hb((__bf16)mkf(0x7fc00000u)) != 0x7fc0)
		fail = 1;
	if (hb((__bf16)mkf(0x7f7fffffu)) != 0x7f80)
		fail = 1;

	/* arithmetic promotes to float and rounds back: exact-in-bf16 cases */
	if ((float)((__bf16)1.5f + (__bf16)0.25f) != 1.75f)
		fail = 1;
	if ((float)mulbf((__bf16)1.5f, (__bf16)2.0f) != 3.0f)
		fail = 1;

	/* call ABI: pass two, return one */
	if ((float)addbf((__bf16)2.0f, (__bf16)5.0f) != 7.0f)
		fail = 1;

	/* negation is a sign-bit flip */
	if (hb(-(__bf16)3.14159f) != (u16)(0x4049 ^ 0x8000))
		fail = 1;

	/* comparison via float promotion */
	if (!((__bf16)1.0f < (__bf16)2.0f))
		fail = 1;

	/* integer round trip */
	if ((int)(__bf16)42 != 42)
		fail = 1;

	/* storage in aggregates */
	__bf16 arr[3];
	arr[0] = (__bf16)1.0f;
	arr[1] = (__bf16)-2.5f;
	arr[2] = (__bf16)100.0f;
	struct SBI s;
	s.a = arr[1];
	s.tag = 7;
	if ((float)s.a != -2.5f || s.tag != 7)
		fail = 1;

	/* static initializer */
	static __bf16 g = (__bf16)1.0f;
	if (hb(g) != 0x3f80)
		fail = 1;

	if (!fail)
		printf("OK\n");
	return fail;
}
