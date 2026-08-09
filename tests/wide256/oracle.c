#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "corpus.h"

static mpz_t MOD, HALF;

static const int kfold_pairs[][2] = {
#define FOLD_PAIR(i, j) {i, j},
		W256_FOLD_LIST(FOLD_PAIR)};

static void load_u(mpz_t r, int i) {
	int k;
	mpz_t t;
	mpz_init(t);
	mpz_set_ui(r, 0);
	for (k = 3; k >= 0; k--) {
		mpz_mul_2exp(r, r, 32);
		mpz_set_ui(t, (unsigned long)(w256_oper[i][k] >> 32));
		mpz_add(r, r, t);
		mpz_mul_2exp(r, r, 32);
		mpz_set_ui(t, (unsigned long)(w256_oper[i][k] & 0xffffffffu));
		mpz_add(r, r, t);
	}
	mpz_clear(t);
}

static void wrap(mpz_t r) {
	mpz_fdiv_r(r, r, MOD);
}

static void to_signed(mpz_t r) {
	if (mpz_cmp(r, HALF) >= 0)
		mpz_sub(r, r, MOD);
}

static void emit(const char *tag, int i, int j, mpz_t v) {
	char buf[80];
	char pad[65];
	size_t n;
	mpz_t t;
	mpz_init_set(t, v);
	wrap(t);
	mpz_get_str(buf, 16, t);
	mpz_clear(t);
	n = strlen(buf);
	if (n > 64) {
		fprintf(stderr, "oracle: value too wide\n");
		exit(2);
	}
	memset(pad, '0', 64 - n);
	memcpy(pad + (64 - n), buf, n + 1);
	printf("%s %d %d %s\n", tag, i, j, pad);
}

static void emit_i(const char *tag, int i, int j, long long v) {
	printf("%s %d %d %lld\n", tag, i, j, v);
}

static long long low_bits_signed(mpz_t v, int bits) {
	mpz_t t;
	unsigned long long u;
	long long s;
	mpz_init_set(t, v);
	wrap(t);
	mpz_fdiv_r_2exp(t, t, bits);
	u = 0;
	{
		mpz_t hi, lo;
		mpz_init(hi);
		mpz_init(lo);
		mpz_fdiv_q_2exp(hi, t, 32);
		mpz_fdiv_r_2exp(lo, t, 32);
		u = ((unsigned long long)mpz_get_ui(hi) << 32) |
				(unsigned long long)mpz_get_ui(lo);
		mpz_clear(hi);
		mpz_clear(lo);
	}
	mpz_clear(t);
	if (bits < 64 && (u >> (bits - 1)) & 1ull)
		u |= ~0ull << bits;
	memcpy(&s, &u, sizeof s);
	return s;
}

static long long low_bits_unsigned(mpz_t v, int bits) {
	long long s = low_bits_signed(v, bits);
	unsigned long long u;
	memcpy(&u, &s, sizeof u);
	if (bits < 64)
		u &= (1ull << bits) - 1ull;
	memcpy(&s, &u, sizeof s);
	return s;
}

static void from_signed_ll(mpz_t r, long long v) {
	if (v < 0) {
		mpz_set_ui(r, (unsigned long)(-(v + 1)));
		mpz_add_ui(r, r, 1);
		mpz_neg(r, r);
	} else {
		mpz_set_ui(r, (unsigned long)v);
	}
	wrap(r);
}

static void from_unsigned_ll(mpz_t r, unsigned long long v) {
	mpz_set_ui(r, (unsigned long)(v >> 32));
	mpz_mul_2exp(r, r, 32);
	{
		mpz_t lo;
		mpz_init_set_ui(lo, (unsigned long)(v & 0xffffffffu));
		mpz_add(r, r, lo);
		mpz_clear(lo);
	}
	wrap(r);
}

static void divmod(mpz_t q, mpz_t rem, mpz_t a, mpz_t b, int is_signed) {
	mpz_t x, y;
	mpz_init_set(x, a);
	mpz_init_set(y, b);
	if (is_signed) {
		to_signed(x);
		to_signed(y);
	}
	if (mpz_sgn(y) == 0) {
		mpz_set_si(q, -1);
		mpz_set(rem, a);
	} else {
		mpz_tdiv_qr(q, rem, x, y);
	}
	wrap(q);
	wrap(rem);
	mpz_clear(x);
	mpz_clear(y);
}

static void shiftop(mpz_t r, mpz_t a, long long n, int arith, int left) {
	mpz_t x;
	mpz_init_set(x, a);
	if (arith)
		to_signed(x);
	if (n < 0 || n >= 256) {
		if (left)
			mpz_set_ui(r, 0);
		else if (arith)
			mpz_set_si(r, mpz_sgn(x) < 0 ? -1 : 0);
		else
			mpz_set_ui(r, 0);
	} else if (left) {
		mpz_mul_2exp(r, x, (unsigned long)n);
	} else {
		mpz_fdiv_q_2exp(r, x, (unsigned long)n);
	}
	wrap(r);
	mpz_clear(x);
}

static int cmp_s(mpz_t a, mpz_t b) {
	mpz_t x, y;
	int c;
	mpz_init_set(x, a);
	mpz_init_set(y, b);
	to_signed(x);
	to_signed(y);
	c = mpz_cmp(x, y);
	mpz_clear(x);
	mpz_clear(y);
	return c;
}

int main(void) {
	int i, j;
	mpz_t a, b, r, q, rem;

	mpz_init(MOD);
	mpz_init(HALF);
	mpz_ui_pow_ui(MOD, 2, 256);
	mpz_ui_pow_ui(HALF, 2, 255);
	mpz_init(a);
	mpz_init(b);
	mpz_init(r);
	mpz_init(q);
	mpz_init(rem);

	for (i = 0; i < W256_NOPER; i++) {
		for (j = 0; j < W256_NOPER; j++) {
			load_u(a, i);
			load_u(b, j);

			mpz_add(r, a, b);
			emit("add", i, j, r);
			mpz_sub(r, a, b);
			emit("sub", i, j, r);
			mpz_mul(r, a, b);
			emit("mul", i, j, r);
			mpz_and(r, a, b);
			emit("and", i, j, r);
			mpz_ior(r, a, b);
			emit("or", i, j, r);
			mpz_xor(r, a, b);
			emit("xor", i, j, r);
			divmod(q, rem, a, b, 1);
			emit("sdiv", i, j, q);
			emit("smod", i, j, rem);
			divmod(q, rem, a, b, 0);
			emit("udiv", i, j, q);
			emit("umod", i, j, rem);

			emit_i("seq", i, j, mpz_cmp(a, b) == 0);
			emit_i("sne", i, j, mpz_cmp(a, b) != 0);
			emit_i("slt", i, j, cmp_s(a, b) < 0);
			emit_i("sle", i, j, cmp_s(a, b) <= 0);
			emit_i("sgt", i, j, cmp_s(a, b) > 0);
			emit_i("sge", i, j, cmp_s(a, b) >= 0);
			emit_i("ult", i, j, mpz_cmp(a, b) < 0);
			emit_i("ule", i, j, mpz_cmp(a, b) <= 0);
			emit_i("ugt", i, j, mpz_cmp(a, b) > 0);
			emit_i("uge", i, j, mpz_cmp(a, b) >= 0);
		}
	}

	for (i = 0; i < W256_NOPER; i++) {
		load_u(a, i);
		for (j = 0; j < W256_NSHIFT; j++) {
			long long n = w256_shift[j];
			shiftop(r, a, n, 1, 1);
			emit("shl", i, j, r);
			shiftop(r, a, n, 1, 0);
			emit("sar", i, j, r);
			shiftop(r, a, n, 0, 0);
			emit("shr", i, j, r);
			shiftop(r, a, n, 0, 1);
			emit("ushl", i, j, r);
		}
	}

	for (i = 0; i < W256_NOPER; i++) {
		load_u(a, i);
		emit_i("tosc", i, 0, low_bits_signed(a, 8));
		emit_i("touc", i, 0, low_bits_unsigned(a, 8));
		emit_i("tos", i, 0, low_bits_signed(a, 16));
		emit_i("tous", i, 0, low_bits_unsigned(a, 16));
		emit_i("toi", i, 0, low_bits_signed(a, 32));
		emit_i("toui", i, 0, low_bits_unsigned(a, 32));
		emit_i("toll", i, 0, low_bits_signed(a, 64));
		emit_i("toull", i, 0, low_bits_signed(a, 64));
		emit_i("tob", i, 0, mpz_sgn(a) != 0);
		emit_i("tobu", i, 0, mpz_sgn(a) != 0);
		from_signed_ll(r, low_bits_signed(a, 8));
		emit("fromsc", i, 0, r);
		from_unsigned_ll(r, (unsigned long long)low_bits_unsigned(a, 8));
		emit("fromuc", i, 0, r);
		from_signed_ll(r, low_bits_signed(a, 16));
		emit("froms", i, 0, r);
		from_signed_ll(r, low_bits_signed(a, 32));
		emit("fromi", i, 0, r);
		from_unsigned_ll(r, (unsigned long long)low_bits_unsigned(a, 32));
		emit("fromui", i, 0, r);
		from_signed_ll(r, low_bits_signed(a, 64));
		emit("fromll", i, 0, r);
		{
			unsigned long long u;
			long long s = low_bits_signed(a, 64);
			memcpy(&u, &s, sizeof u);
			from_unsigned_ll(r, u);
			emit("fromull", i, 0, r);
		}
		emit("tou", i, 0, a);
		emit("tos256", i, 0, a);
		mpz_neg(r, a);
		emit("neg", i, 0, r);
		mpz_com(r, a);
		emit("not", i, 0, r);
	}

	for (i = 0; i < W256_NFOLD; i++) {
		static const char *const cmpn[10] = {"ceq",  "cne",  "clt",  "cle",
																				 "cgt",  "cge",  "cult", "cule",
																				 "cugt", "cuge"};
		int x = kfold_pairs[i][0], y = kfold_pairs[i][1];
		load_u(a, x);
		load_u(b, y);
		mpz_add(r, a, b);
		emit("cadd", i, 0, r);
		mpz_sub(r, a, b);
		emit("csub", i, 0, r);
		mpz_mul(r, a, b);
		emit("cmul", i, 0, r);
		mpz_and(r, a, b);
		emit("cand", i, 0, r);
		mpz_ior(r, a, b);
		emit("cor", i, 0, r);
		mpz_xor(r, a, b);
		emit("cxor", i, 0, r);
		divmod(q, rem, a, b, 1);
		emit("csdiv", i, 0, q);
		emit("csmod", i, 0, rem);
		divmod(q, rem, a, b, 0);
		emit("cudiv", i, 0, q);
		emit("cumod", i, 0, rem);
		mpz_neg(r, a);
		emit("cneg", i, 0, r);
		mpz_com(r, a);
		emit("cnot", i, 0, r);
		for (j = 0; j < W256_NSHIFT; j++) {
			long long n = w256_shift[j];
			shiftop(r, a, n, 1, 1);
			emit("cshl", i, j, r);
			shiftop(r, a, n, 1, 0);
			emit("csar", i, j, r);
			shiftop(r, a, n, 0, 0);
			emit("cshr", i, j, r);
		}
		emit_i(cmpn[0], i, 0, mpz_cmp(a, b) == 0);
		emit_i(cmpn[1], i, 0, mpz_cmp(a, b) != 0);
		emit_i(cmpn[2], i, 0, cmp_s(a, b) < 0);
		emit_i(cmpn[3], i, 0, cmp_s(a, b) <= 0);
		emit_i(cmpn[4], i, 0, cmp_s(a, b) > 0);
		emit_i(cmpn[5], i, 0, cmp_s(a, b) >= 0);
		emit_i(cmpn[6], i, 0, mpz_cmp(a, b) < 0);
		emit_i(cmpn[7], i, 0, mpz_cmp(a, b) <= 0);
		emit_i(cmpn[8], i, 0, mpz_cmp(a, b) > 0);
		emit_i(cmpn[9], i, 0, mpz_cmp(a, b) >= 0);
	}
	return 0;
}
