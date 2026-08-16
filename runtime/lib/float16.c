#ifdef __MCC__
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#else
#include <stdint.h>
#endif

typedef union {
	uint32_t x;
	float f;
} u32_t;

static uint32_t f16_bits_to_f32_bits(uint16_t h) {
	uint32_t sign = (uint32_t)(h & 0x8000) << 16;
	uint32_t exp = (h >> 10) & 0x1f;
	uint32_t mant = h & 0x3ff;
	int e = -1;
	if (exp == 0) {
		if (mant == 0)
			return sign;
		do {
			mant <<= 1;
			e++;
		} while (!(mant & 0x400));
		mant &= 0x3ff;
		return sign | ((uint32_t)(127 - 15 - e) << 23) | (mant << 13);
	}
	if (exp == 0x1f) {
		if (mant == 0)
			return sign | 0x7f800000u;
		return sign | 0x7f800000u | 0x400000u | (mant << 13);
	}
	return sign | ((exp + 127 - 15) << 23) | (mant << 13);
}

static uint16_t f32_bits_to_f16_bits(uint32_t x) {
	uint32_t sign = (x >> 16) & 0x8000;
	uint32_t exp = (x >> 23) & 0xff;
	uint32_t mant = x & 0x7fffff;
	uint32_t m, rem, half;
	int e, shift;
	if (exp == 0xff) {
		if (mant == 0)
			return (uint16_t)(sign | 0x7c00);
		return (uint16_t)(sign | 0x7c00 | ((mant >> 13) | 0x200));
	}
	e = (int)exp - 127 + 15;
	if (e >= 0x1f)
		return (uint16_t)(sign | 0x7c00);
	if (e <= 0) {
		if (e < -10)
			return (uint16_t)sign;
		mant |= 0x800000;
		shift = 14 - e;
		m = mant >> shift;
		rem = mant & ((1u << shift) - 1);
		half = 1u << (shift - 1);
		if (rem > half || (rem == half && (m & 1)))
			m++;
		return (uint16_t)(sign | m);
	}
	m = mant >> 13;
	rem = mant & 0x1fff;
	if (rem > 0x1000 || (rem == 0x1000 && (m & 1))) {
		m++;
		if (m == 0x400) {
			m = 0;
			e++;
			if (e >= 0x1f)
				return (uint16_t)(sign | 0x7c00);
		}
	}
	return (uint16_t)(sign | ((uint32_t)e << 10) | m);
}

float __mcc_extendhfsf2(uint16_t a) {
	u32_t u;
	u.x = f16_bits_to_f32_bits(a);
	return u.f;
}

uint16_t __mcc_truncsfhf2(float a) {
	u32_t u;
	u.f = a;
	return f32_bits_to_f16_bits(u.x);
}

static uint16_t f32_bits_to_bf16_bits(uint32_t x) {
	uint32_t exp = (x >> 23) & 0xff;
	uint32_t mant = x & 0x7fffff;
	uint32_t lsb, bias;
	if (exp == 0xff) {
		if (mant == 0)
			return (uint16_t)(x >> 16);
		return (uint16_t)((x >> 16) | 0x40);
	}
	lsb = (x >> 16) & 1;
	bias = 0x7fff + lsb;
	x += bias;
	return (uint16_t)(x >> 16);
}

float __mcc_extendbfsf2(uint16_t a) {
	u32_t u;
	u.x = (uint32_t)a << 16;
	return u.f;
}

uint16_t __mcc_truncsfbf2(float a) {
	u32_t u;
	u.f = a;
	return f32_bits_to_bf16_bits(u.x);
}
