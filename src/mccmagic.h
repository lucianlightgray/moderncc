#ifndef MCC_MAGIC_H
#define MCC_MAGIC_H

/*
 * Magic-number constants for strength-reducing integer division by a constant
 * (Granlund-Montgomery / Hacker's Delight, ch. 10). A later AST transform will fold
 * `x / C` and `x % C` into a high-multiply + shift (+ correction) sequence, which is
 * ~5 cycles vs the ~20-40 of a hardware divide. This header holds ONLY the pure
 * arithmetic that computes the magic (M, shift, add-flag) and a reference applier used
 * to prove it — no AST or codegen dependency, and dependency-free (only <stdint.h>), so
 * tools/asttool.c can exhaustively self-test the algorithm before it is wired anywhere
 * (the same "selftest before wire" discipline as mccgate.h / mcccombo.h). Getting the
 * magic wrong silently miscompiles arithmetic, so the algorithm is validated exhaustively
 * (all-ish dividends x a large divisor range vs native `/`) before it can be trusted.
 */

#include <stdint.h>

/* Unsigned 32-bit: q = n / d == (n's high-mul by M) adjusted, per mcc_divu_apply. */
typedef struct {
	uint32_t M; /* magic multiplier */
	int a;      /* add-indicator: needs the overflow-avoiding correction sequence */
	int s;      /* post-shift */
} MccMagicU;

static MccMagicU mcc_magicu(uint32_t d) {
	int p;
	uint32_t nc, delta, q1, r1, q2, r2;
	MccMagicU mag;
	mag.a = 0;
	nc = 0xFFFFFFFFu - (0u - d) % d; /* -1 - (-d) mod d */
	p = 31;
	q1 = 0x80000000u / nc;
	r1 = 0x80000000u - q1 * nc;
	q2 = 0x7FFFFFFFu / d;
	r2 = 0x7FFFFFFFu - q2 * d;
	do {
		p++;
		if (r1 >= nc - r1) {
			q1 = 2 * q1 + 1;
			r1 = 2 * r1 - nc;
		} else {
			q1 = 2 * q1;
			r1 = 2 * r1;
		}
		if (r2 + 1 >= d - r2) {
			if (q2 >= 0x7FFFFFFFu)
				mag.a = 1;
			q2 = 2 * q2 + 1;
			r2 = 2 * r2 + 1 - d;
		} else {
			if (q2 >= 0x80000000u)
				mag.a = 1;
			q2 = 2 * q2;
			r2 = 2 * r2 + 1;
		}
		delta = d - 1 - r2;
	} while (p < 64 && (q1 < delta || (q1 == delta && r1 == 0)));
	mag.M = q2 + 1;
	mag.s = p - 32;
	return mag;
}

/* Reference application of an unsigned magic (mirrors the sequence the AST fold emits). */
static uint32_t mcc_divu_apply(uint32_t n, MccMagicU mag) {
	uint32_t q = (uint32_t)(((uint64_t)mag.M * n) >> 32); /* MULUH(M, n) */
	if (mag.a) {
		uint32_t t = ((n - q) >> 1) + q; /* avoids the 33-bit intermediate overflow */
		return t >> (mag.s - 1);
	}
	return q >> mag.s;
}

/* Signed 32-bit: q = n / d (C truncation toward zero), per mcc_divs_apply. */
typedef struct {
	int32_t M; /* magic multiplier (may be negative) */
	int s;     /* post-shift */
} MccMagicS;

static MccMagicS mcc_magics(int32_t d) {
	int p;
	uint32_t ad, anc, delta, q1, r1, q2, r2, t;
	const uint32_t two31 = 0x80000000u;
	MccMagicS mag;
	ad = (d < 0) ? (0u - (uint32_t)d) : (uint32_t)d; /* |d| */
	t = two31 + ((uint32_t)d >> 31);
	anc = t - 1 - t % ad; /* |nc| */
	p = 31;
	q1 = two31 / anc;
	r1 = two31 - q1 * anc;
	q2 = two31 / ad;
	r2 = two31 - q2 * ad;
	do {
		p++;
		q1 = 2 * q1;
		r1 = 2 * r1;
		if (r1 >= anc) {
			q1++;
			r1 -= anc;
		}
		q2 = 2 * q2;
		r2 = 2 * r2;
		if (r2 >= ad) {
			q2++;
			r2 -= ad;
		}
		delta = ad - r2;
	} while (q1 < delta || (q1 == delta && r1 == 0));
	mag.M = (int32_t)(q2 + 1);
	if (d < 0)
		mag.M = -mag.M;
	mag.s = p - 32;
	return mag;
}

/* Reference application of a signed magic (mirrors the sequence the AST fold emits). */
static int32_t mcc_divs_apply(int32_t n, int32_t d, MccMagicS mag) {
	int32_t q = (int32_t)(((int64_t)mag.M * n) >> 32); /* MULSH(M, n) */
	if (d > 0 && mag.M < 0)
		q += n;
	else if (d < 0 && mag.M > 0)
		q -= n;
	q >>= mag.s;
	q += (int32_t)((uint32_t)q >> 31); /* add the sign bit */
	return q;
}

#endif /* MCC_MAGIC_H */
