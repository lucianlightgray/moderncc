#include "../../src/wide256_arith.h"

typedef mcc_w256_limb w256[MCC_W256_LIMBS];

void __mcc_i256_add(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_add(r, a, b);
}

void __mcc_i256_sub(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_sub(r, a, b);
}

void __mcc_i256_mul(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	w256 t;
	mcc_w256_mul(t, a, b);
	mcc_w256_copy(r, t);
}

void __mcc_i256_and(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_and(r, a, b);
}

void __mcc_i256_or(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_or(r, a, b);
}

void __mcc_i256_xor(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_xor(r, a, b);
}

void __mcc_i256_shl(mcc_w256_limb *r, const mcc_w256_limb *a, long long n) {
	if (n < 0 || n >= 64 * MCC_W256_LIMBS)
		mcc_w256_zero(r);
	else
		mcc_w256_shl(r, a, (unsigned int)n);
}

void __mcc_i256_shr(mcc_w256_limb *r, const mcc_w256_limb *a, long long n) {
	if (n < 0 || n >= 64 * MCC_W256_LIMBS)
		mcc_w256_zero(r);
	else
		mcc_w256_shr(r, a, (unsigned int)n);
}

void __mcc_i256_sar(mcc_w256_limb *r, const mcc_w256_limb *a, long long n) {
	if (n < 0 || n >= 64 * MCC_W256_LIMBS)
		mcc_w256_sar(r, a, 64 * MCC_W256_LIMBS);
	else
		mcc_w256_sar(r, a, (unsigned int)n);
}

void __mcc_i256_udiv(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	w256 q, rem;
	mcc_w256_udivmod(q, rem, a, b);
	mcc_w256_copy(r, q);
}

void __mcc_i256_umod(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	w256 q, rem;
	mcc_w256_udivmod(q, rem, a, b);
	mcc_w256_copy(r, rem);
}

void __mcc_i256_sdiv(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	w256 q, rem;
	mcc_w256_sdivmod(q, rem, a, b);
	mcc_w256_copy(r, q);
}

void __mcc_i256_smod(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	w256 q, rem;
	mcc_w256_sdivmod(q, rem, a, b);
	mcc_w256_copy(r, rem);
}

int __mcc_i256_ucmp(const mcc_w256_limb *a, const mcc_w256_limb *b) {
	return mcc_w256_ucmp(a, b);
}

int __mcc_i256_scmp(const mcc_w256_limb *a, const mcc_w256_limb *b) {
	return mcc_w256_scmp(a, b);
}

void __mcc_i256_from_i64(mcc_w256_limb *r, long long v) {
	mcc_w256_from_i64(r, (mcc_w256_limb)v);
}

void __mcc_i256_from_u64(mcc_w256_limb *r, unsigned long long v) {
	mcc_w256_from_u64(r, v);
}

int __mcc_i256_nonzero(const mcc_w256_limb *a) {
	return !mcc_w256_is_zero(a);
}

void __mcc_i256_not(mcc_w256_limb *r, const mcc_w256_limb *a) {
	mcc_w256_not(r, a);
}

void __mcc_i256_neg(mcc_w256_limb *r, const mcc_w256_limb *a) {
	w256 t;
	mcc_w256_neg(t, a);
	mcc_w256_copy(r, t);
}
