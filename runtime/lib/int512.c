/* 8-limb (512-bit) instantiation of the wide-integer kernel, for C23
 * _BitInt(257..512).  Identical to int256.c but compiled at 8 limbs; the
 * kernel (wide256_arith.h) is fully limb-count-parameterized, and its
 * mcc_w256_* inlines are static, so these __mcc_i512_* symbols are disjoint
 * from int256.c's __mcc_i256_*.  __int256/gmp-diff is untouched. */
#define MCC_W256_LIMBS 8
#include "../../src/wide256_arith.h"

typedef mcc_w256_limb w256[MCC_W256_LIMBS];

void __mcc_i512_add(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_add(r, a, b);
}

void __mcc_i512_sub(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_sub(r, a, b);
}

void __mcc_i512_mul(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	w256 t;
	mcc_w256_mul(t, a, b);
	mcc_w256_copy(r, t);
}

void __mcc_i512_and(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_and(r, a, b);
}

void __mcc_i512_or(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_or(r, a, b);
}

void __mcc_i512_xor(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_xor(r, a, b);
}

void __mcc_i512_shl(mcc_w256_limb *r, const mcc_w256_limb *a, long long n) {
	if (n < 0 || n >= 64 * MCC_W256_LIMBS)
		mcc_w256_zero(r);
	else
		mcc_w256_shl(r, a, (unsigned int)n);
}

void __mcc_i512_shr(mcc_w256_limb *r, const mcc_w256_limb *a, long long n) {
	if (n < 0 || n >= 64 * MCC_W256_LIMBS)
		mcc_w256_zero(r);
	else
		mcc_w256_shr(r, a, (unsigned int)n);
}

void __mcc_i512_sar(mcc_w256_limb *r, const mcc_w256_limb *a, long long n) {
	if (n < 0 || n >= 64 * MCC_W256_LIMBS)
		mcc_w256_sar(r, a, 64 * MCC_W256_LIMBS);
	else
		mcc_w256_sar(r, a, (unsigned int)n);
}

void __mcc_i512_udiv(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	w256 q, rem;
	mcc_w256_udivmod(q, rem, a, b);
	mcc_w256_copy(r, q);
}

void __mcc_i512_umod(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	w256 q, rem;
	mcc_w256_udivmod(q, rem, a, b);
	mcc_w256_copy(r, rem);
}

void __mcc_i512_sdiv(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	w256 q, rem;
	mcc_w256_sdivmod(q, rem, a, b);
	mcc_w256_copy(r, q);
}

void __mcc_i512_smod(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	w256 q, rem;
	mcc_w256_sdivmod(q, rem, a, b);
	mcc_w256_copy(r, rem);
}

int __mcc_i512_ucmp(const mcc_w256_limb *a, const mcc_w256_limb *b) {
	return mcc_w256_ucmp(a, b);
}

int __mcc_i512_scmp(const mcc_w256_limb *a, const mcc_w256_limb *b) {
	return mcc_w256_scmp(a, b);
}

void __mcc_i512_from_i64(mcc_w256_limb *r, long long v) {
	mcc_w256_from_i64(r, (mcc_w256_limb)v);
}

void __mcc_i512_from_u64(mcc_w256_limb *r, unsigned long long v) {
	mcc_w256_from_u64(r, v);
}

int __mcc_i512_nonzero(const mcc_w256_limb *a) {
	return !mcc_w256_is_zero(a);
}

void __mcc_i512_not(mcc_w256_limb *r, const mcc_w256_limb *a) {
	mcc_w256_not(r, a);
}

void __mcc_i512_neg(mcc_w256_limb *r, const mcc_w256_limb *a) {
	w256 t;
	mcc_w256_neg(t, a);
	mcc_w256_copy(r, t);
}

/* Correctly-rounded (round to nearest, ties to even) magnitude -> double,
 * keeping `mant` significant bits.  Returns a value with at most `mant`
 * significant bits, so (float) of the mant=24 result is exact (no double
 * rounding).  `a` is treated as an unsigned 256-bit magnitude. */
static double w256_round_to_double(const mcc_w256_limb *a, int mant) {
	int i, L, shift, round_bit, sticky;
	mcc_w256_limb head, shifted[MCC_W256_LIMBS], back[MCC_W256_LIMBS];
	double d, scale;

	L = 0;
	for (i = MCC_W256_LIMBS - 1; i >= 0; i--) {
		if (a[i]) {
			mcc_w256_limb v = a[i];
			L = i * 64;
			while (v) { L++; v >>= 1; }
			break;
		}
	}
	if (L == 0)
		return 0.0;
	if (L <= mant)
		return (double)a[0];			/* fits exactly (L <= mant < 64) */
	shift = L - (mant + 1);				/* keep the top mant+1 bits */
	mcc_w256_shr(shifted, a, (unsigned int)shift);
	head = shifted[0];					/* mant+1 bits, fits in 64 */
	mcc_w256_shl(back, shifted, (unsigned int)shift);
	sticky = 0;
	for (i = 0; i < MCC_W256_LIMBS; i++)
		if (back[i] != a[i]) { sticky = 1; break; }
	round_bit = (int)(head & 1);
	head >>= 1;							/* mant-bit significand */
	if (round_bit && (sticky || (head & 1)))
		head++;							/* ties to even; may reach 2^mant */
	d = (double)head;
	scale = 1.0;
	for (i = 0; i < shift + 1; i++)
		scale *= 2.0;					/* exact power of two */
	return d * scale;
}

double __mcc_i512_to_f64(const mcc_w256_limb *a, int is_unsigned) {
	if (!is_unsigned && mcc_w256_sign(a)) {
		w256 t;
		mcc_w256_neg(t, a);
		return -w256_round_to_double(t, 53);
	}
	return w256_round_to_double(a, 53);
}

float __mcc_i512_to_f32(const mcc_w256_limb *a, int is_unsigned) {
	if (!is_unsigned && mcc_w256_sign(a)) {
		w256 t;
		mcc_w256_neg(t, a);
		return (float)-w256_round_to_double(t, 24);
	}
	return (float)w256_round_to_double(a, 24);
}

/* Truncate a non-negative double toward zero into a 256-bit magnitude. */
static void w256_from_double_mag(mcc_w256_limb *r, double x) {
	union { double d; unsigned long long u; } b;
	int exp, e;
	unsigned long long mant;

	mcc_w256_zero(r);
	if (!(x >= 1.0))
		return;							/* |x| < 1 or NaN -> 0 */
	b.d = x;
	exp = (int)((b.u >> 52) & 0x7FF) - 1023;
	mant = (b.u & 0xFFFFFFFFFFFFFULL) | (1ULL << 52);	/* 53-bit significand */
	e = exp - 52;						/* value = mant * 2^e */
	if (exp >= 512) {					/* out of range (UB in C) -> saturate */
		mcc_w256_not(r, r);
		return;
	}
	r[0] = mant;
	if (e > 0)
		mcc_w256_shl(r, r, (unsigned int)e);
	else if (e < 0)
		mcc_w256_shr(r, r, (unsigned int)(-e));
}

void __mcc_i512_from_f64(mcc_w256_limb *r, double x, int is_unsigned) {
	if (x < 0.0) {
		if (is_unsigned)
			{ mcc_w256_zero(r); return; }	/* UB; match gcc's 0 for negatives */
		w256_from_double_mag(r, -x);
		mcc_w256_neg(r, r);
		return;
	}
	w256_from_double_mag(r, x);
}

void __mcc_i512_from_f32(mcc_w256_limb *r, float x, int is_unsigned) {
	__mcc_i512_from_f64(r, (double)x, is_unsigned);
}
