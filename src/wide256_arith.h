#ifndef MCC_W256_LIMBS
#define MCC_W256_LIMBS 4
#endif

typedef unsigned long long mcc_w256_limb;

static inline void mcc_w256_copy(mcc_w256_limb *r, const mcc_w256_limb *a) {
	int i;
	for (i = 0; i < MCC_W256_LIMBS; i++)
		r[i] = a[i];
}

static inline void mcc_w256_zero(mcc_w256_limb *r) {
	int i;
	for (i = 0; i < MCC_W256_LIMBS; i++)
		r[i] = 0;
}

static inline int mcc_w256_is_zero(const mcc_w256_limb *a) {
	int i;
	for (i = 0; i < MCC_W256_LIMBS; i++)
		if (a[i])
			return 0;
	return 1;
}

static inline int mcc_w256_sign(const mcc_w256_limb *a) {
	return (int)(a[MCC_W256_LIMBS - 1] >> 63);
}

static inline int mcc_w256_bit(const mcc_w256_limb *a, int n) {
	if (n < 0 || n >= 64 * MCC_W256_LIMBS)
		return 0;
	return (int)((a[n >> 6] >> (n & 63)) & 1ull);
}

static inline void mcc_w256_setbit(mcc_w256_limb *r, int n) {
	if (n < 0 || n >= 64 * MCC_W256_LIMBS)
		return;
	r[n >> 6] |= 1ull << (n & 63);
}

static inline void mcc_w256_and(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	int i;
	for (i = 0; i < MCC_W256_LIMBS; i++)
		r[i] = a[i] & b[i];
}

static inline void mcc_w256_or(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	int i;
	for (i = 0; i < MCC_W256_LIMBS; i++)
		r[i] = a[i] | b[i];
}

static inline void mcc_w256_xor(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	int i;
	for (i = 0; i < MCC_W256_LIMBS; i++)
		r[i] = a[i] ^ b[i];
}

static inline void mcc_w256_not(mcc_w256_limb *r, const mcc_w256_limb *a) {
	int i;
	for (i = 0; i < MCC_W256_LIMBS; i++)
		r[i] = ~a[i];
}

static inline void mcc_w256_add(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_limb carry = 0;
	int i;
	for (i = 0; i < MCC_W256_LIMBS; i++) {
		mcc_w256_limb s = a[i] + b[i];
		mcc_w256_limb c1 = s < a[i];
		mcc_w256_limb t = s + carry;
		mcc_w256_limb c2 = t < s;
		r[i] = t;
		carry = c1 | c2;
	}
}

static inline void mcc_w256_sub(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_limb borrow = 0;
	int i;
	for (i = 0; i < MCC_W256_LIMBS; i++) {
		mcc_w256_limb d = a[i] - b[i];
		mcc_w256_limb b1 = a[i] < b[i];
		mcc_w256_limb t = d - borrow;
		mcc_w256_limb b2 = d < borrow;
		r[i] = t;
		borrow = b1 | b2;
	}
}

static inline void mcc_w256_neg(mcc_w256_limb *r, const mcc_w256_limb *a) {
	mcc_w256_limb z[MCC_W256_LIMBS];
	mcc_w256_zero(z);
	mcc_w256_sub(r, z, a);
}

static inline void mcc_w256_mul(mcc_w256_limb *r, const mcc_w256_limb *a, const mcc_w256_limb *b) {
	unsigned int ah[2 * MCC_W256_LIMBS], bh[2 * MCC_W256_LIMBS];
	mcc_w256_limb acc[2 * MCC_W256_LIMBS];
	mcc_w256_limb carry;
	int i, j, n = 2 * MCC_W256_LIMBS;

	for (i = 0; i < MCC_W256_LIMBS; i++) {
		ah[2 * i] = (unsigned int)(a[i] & 0xffffffffu);
		ah[2 * i + 1] = (unsigned int)(a[i] >> 32);
		bh[2 * i] = (unsigned int)(b[i] & 0xffffffffu);
		bh[2 * i + 1] = (unsigned int)(b[i] >> 32);
	}
	for (i = 0; i < n; i++)
		acc[i] = 0;
	for (i = 0; i < n; i++) {
		carry = 0;
		for (j = 0; i + j < n; j++) {
			mcc_w256_limb cur = acc[i + j] + (mcc_w256_limb)ah[i] * bh[j] + carry;
			acc[i + j] = cur & 0xffffffffu;
			carry = cur >> 32;
		}
	}
	for (i = 0; i < MCC_W256_LIMBS; i++)
		r[i] = (acc[2 * i] & 0xffffffffu) | (acc[2 * i + 1] << 32);
}

static inline void mcc_w256_shl(mcc_w256_limb *r, const mcc_w256_limb *a, unsigned int n) {
	int i;
	unsigned int words, bits;
	if (n >= 64u * MCC_W256_LIMBS) {
		mcc_w256_zero(r);
		return;
	}
	words = n >> 6;
	bits = n & 63;
	for (i = MCC_W256_LIMBS - 1; i >= 0; i--) {
		mcc_w256_limb v = 0;
		if ((unsigned int)i >= words) {
			v = a[i - words] << bits;
			if (bits && (unsigned int)i > words)
				v |= a[i - words - 1] >> (64 - bits);
		}
		r[i] = v;
	}
}

static inline void mcc_w256_shr(mcc_w256_limb *r, const mcc_w256_limb *a, unsigned int n) {
	int i;
	unsigned int words, bits;
	if (n >= 64u * MCC_W256_LIMBS) {
		mcc_w256_zero(r);
		return;
	}
	words = n >> 6;
	bits = n & 63;
	for (i = 0; i < MCC_W256_LIMBS; i++) {
		mcc_w256_limb v = 0;
		if ((unsigned int)i + words < MCC_W256_LIMBS) {
			v = a[i + words] >> bits;
			if (bits && (unsigned int)i + words + 1 < MCC_W256_LIMBS)
				v |= a[i + words + 1] << (64 - bits);
		}
		r[i] = v;
	}
}

static inline void mcc_w256_sar(mcc_w256_limb *r, const mcc_w256_limb *a, unsigned int n) {
	mcc_w256_limb fill = mcc_w256_sign(a) ? ~(mcc_w256_limb)0 : 0;
	int i;
	unsigned int words, bits;
	if (n >= 64u * MCC_W256_LIMBS) {
		for (i = 0; i < MCC_W256_LIMBS; i++)
			r[i] = fill;
		return;
	}
	words = n >> 6;
	bits = n & 63;
	for (i = 0; i < MCC_W256_LIMBS; i++) {
		mcc_w256_limb hi = (unsigned int)i + words + 1 < MCC_W256_LIMBS
													 ? a[i + words + 1]
													 : fill;
		mcc_w256_limb lo = (unsigned int)i + words < MCC_W256_LIMBS ? a[i + words] : fill;
		mcc_w256_limb v = lo >> bits;
		if (bits)
			v |= hi << (64 - bits);
		r[i] = v;
	}
}

static inline int mcc_w256_ucmp(const mcc_w256_limb *a, const mcc_w256_limb *b) {
	int i;
	for (i = MCC_W256_LIMBS - 1; i >= 0; i--) {
		if (a[i] != b[i])
			return a[i] < b[i] ? -1 : 1;
	}
	return 0;
}

static inline int mcc_w256_scmp(const mcc_w256_limb *a, const mcc_w256_limb *b) {
	int sa = mcc_w256_sign(a), sb = mcc_w256_sign(b);
	if (sa != sb)
		return sa ? -1 : 1;
	return mcc_w256_ucmp(a, b);
}

static inline void mcc_w256_udivmod(mcc_w256_limb *q, mcc_w256_limb *rem,
														 const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_limb r[MCC_W256_LIMBS], t[MCC_W256_LIMBS];
	int i;

	mcc_w256_zero(q);
	mcc_w256_zero(r);
	if (mcc_w256_is_zero(b)) {
		for (i = 0; i < MCC_W256_LIMBS; i++)
			q[i] = ~(mcc_w256_limb)0;
		mcc_w256_copy(rem, a);
		return;
	}
	for (i = 64 * MCC_W256_LIMBS - 1; i >= 0; i--) {
		mcc_w256_shl(t, r, 1);
		mcc_w256_copy(r, t);
		r[0] |= (mcc_w256_limb)mcc_w256_bit(a, i);
		if (mcc_w256_ucmp(r, b) >= 0) {
			mcc_w256_sub(t, r, b);
			mcc_w256_copy(r, t);
			mcc_w256_setbit(q, i);
		}
	}
	mcc_w256_copy(rem, r);
}

static inline void mcc_w256_sdivmod(mcc_w256_limb *q, mcc_w256_limb *rem,
														 const mcc_w256_limb *a, const mcc_w256_limb *b) {
	mcc_w256_limb ua[MCC_W256_LIMBS], ub[MCC_W256_LIMBS];
	mcc_w256_limb uq[MCC_W256_LIMBS], ur[MCC_W256_LIMBS];
	int na = mcc_w256_sign(a), nb = mcc_w256_sign(b);

	if (mcc_w256_is_zero(b)) {
		int i;
		for (i = 0; i < MCC_W256_LIMBS; i++)
			q[i] = ~(mcc_w256_limb)0;
		mcc_w256_copy(rem, a);
		return;
	}
	if (na)
		mcc_w256_neg(ua, a);
	else
		mcc_w256_copy(ua, a);
	if (nb)
		mcc_w256_neg(ub, b);
	else
		mcc_w256_copy(ub, b);
	mcc_w256_udivmod(uq, ur, ua, ub);
	if (na != nb)
		mcc_w256_neg(q, uq);
	else
		mcc_w256_copy(q, uq);
	if (na)
		mcc_w256_neg(rem, ur);
	else
		mcc_w256_copy(rem, ur);
}

static inline void mcc_w256_from_u64(mcc_w256_limb *r, mcc_w256_limb v) {
	int i;
	r[0] = v;
	for (i = 1; i < MCC_W256_LIMBS; i++)
		r[i] = 0;
}

static inline void mcc_w256_from_i64(mcc_w256_limb *r, mcc_w256_limb v) {
	mcc_w256_limb fill = (v >> 63) ? ~(mcc_w256_limb)0 : 0;
	int i;
	r[0] = v;
	for (i = 1; i < MCC_W256_LIMBS; i++)
		r[i] = fill;
}
