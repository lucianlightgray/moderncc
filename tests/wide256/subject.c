#include <stdio.h>
#include <string.h>
#include "corpus.h"

#define W256C(h3, h2, h1, h0)                                                  \
	(((__int256)(unsigned long long)(h3) << 192) |                               \
	 ((__int256)(unsigned long long)(h2) << 128) |                               \
	 ((__int256)(unsigned long long)(h1) << 64) |                                \
	 ((__int256)(unsigned long long)(h0)))

#define K0 W256C(0x0ull, 0x0ull, 0x0ull, 0x0ull)
#define K1 W256C(0x0ull, 0x0ull, 0x0ull, 0x1ull)
#define K2                                                                     \
	W256C(0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull,   \
				0xffffffffffffffffull)
#define K3 W256C(0x8000000000000000ull, 0x0ull, 0x0ull, 0x0ull)
#define K4                                                                     \
	W256C(0x7fffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull,   \
				0xffffffffffffffffull)
#define K5 W256C(0x0ull, 0x0ull, 0x1ull, 0x0ull)
#define K6 W256C(0x0ull, 0x0ull, 0x0ull, 0xffffffffffffffffull)
#define K7 W256C(0x0ull, 0x1ull, 0x0ull, 0x0ull)
#define K8 W256C(0x0ull, 0x0ull, 0xffffffffffffffffull, 0xffffffffffffffffull)
#define K9 W256C(0x1ull, 0x0ull, 0x0ull, 0x0ull)
#define K10                                                                    \
	W256C(0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull,   \
				0xfffffffffffffffdull)
#define K11                                                                    \
	W256C(0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull,   \
				0xc4653601a52a5d1full)
#define K12                                                                    \
	W256C(0x123456789abcdef0ull, 0x00ff00ff00ff00ffull, 0xfedcba9876543210ull,   \
				0x0123456789abcdefull)
#define K13 W256C(0x0ull, 0xffffffffffffffffull, 0x0ull, 0xdeadbeefcafebabeull)
#define K14 W256C(0x0ull, 0x0ull, 0x0ull, 0xffffffffull)
#define K15 W256C(0x0ull, 0x0ull, 0x0ull, 0x100000000ull)
#define K16                                                                    \
	W256C(0x8000000000000000ull, 0x7fffffffffffffffull, 0x8000000000000000ull,   \
				0x7fffffffffffffffull)
#define K17 W256C(0x0ull, 0x0ull, 0x0ull, 0x7ull)
#ifdef MCC_W256_MUTATE
#undef K3
#define K3 (W256C(0x8000000000000000ull, 0x0ull, 0x0ull, 0x0ull) ^ (__int256)1)
#endif
#define KK(i) K##i
#define KU(i) ((unsigned __int256)KK(i))

#define FOLD_ADD(i, j) KK(i) + KK(j),
#define FOLD_SUB(i, j) KK(i) - KK(j),
#define FOLD_MUL(i, j) KK(i) * KK(j),
#define FOLD_AND(i, j) KK(i) & KK(j),
#define FOLD_OR(i, j) KK(i) | KK(j),
#define FOLD_XOR(i, j) KK(i) ^ KK(j),
#define FOLD_SDIV(i, j) KK(i) / KK(j),
#define FOLD_SMOD(i, j) KK(i) % KK(j),
#define FOLD_UDIV(i, j) KU(i) / KU(j),
#define FOLD_UMOD(i, j) KU(i) % KU(j),
#define FOLD_NEG(i, j) -KK(i),
#define FOLD_NOT(i, j) ~KK(i),
#define FOLD_SHL_ROW(i, j)                                                     \
	{KK(i) << 0,   KK(i) << 1,   KK(i) << 31,  KK(i) << 32,  KK(i) << 63,        \
	 KK(i) << 64,  KK(i) << 65,  KK(i) << 127, KK(i) << 128, KK(i) << 191,       \
	 KK(i) << 192, KK(i) << 255, KK(i) << 256, KK(i) << -5},
#define FOLD_SAR_ROW(i, j)                                                     \
	{KK(i) >> 0,   KK(i) >> 1,   KK(i) >> 31,  KK(i) >> 32,  KK(i) >> 63,        \
	 KK(i) >> 64,  KK(i) >> 65,  KK(i) >> 127, KK(i) >> 128, KK(i) >> 191,       \
	 KK(i) >> 192, KK(i) >> 255, KK(i) >> 256, KK(i) >> -5},
#define FOLD_SHR_ROW(i, j)                                                     \
	{KU(i) >> 0,   KU(i) >> 1,   KU(i) >> 31,  KU(i) >> 32,  KU(i) >> 63,        \
	 KU(i) >> 64,  KU(i) >> 65,  KU(i) >> 127, KU(i) >> 128, KU(i) >> 191,       \
	 KU(i) >> 192, KU(i) >> 255, KU(i) >> 256, KU(i) >> -5},
#define FOLD_CMP(i, j)                                                         \
	{KK(i) == KK(j), KK(i) != KK(j), KK(i) < KK(j),  KK(i) <= KK(j),             \
	 KK(i) > KK(j),  KK(i) >= KK(j), KU(i) < KU(j),  KU(i) <= KU(j),             \
	 KU(i) > KU(j),  KU(i) >= KU(j)},

static const __int256 kadd[] = {W256_FOLD_LIST(FOLD_ADD)};
static const __int256 ksub[] = {W256_FOLD_LIST(FOLD_SUB)};
static const __int256 kmul[] = {W256_FOLD_LIST(FOLD_MUL)};
static const __int256 kand[] = {W256_FOLD_LIST(FOLD_AND)};
static const __int256 kor[] = {W256_FOLD_LIST(FOLD_OR)};
static const __int256 kxor[] = {W256_FOLD_LIST(FOLD_XOR)};
static const __int256 ksdiv[] = {W256_FOLD_LIST(FOLD_SDIV)};
static const __int256 ksmod[] = {W256_FOLD_LIST(FOLD_SMOD)};
static const unsigned __int256 kudiv[] = {W256_FOLD_LIST(FOLD_UDIV)};
static const unsigned __int256 kumod[] = {W256_FOLD_LIST(FOLD_UMOD)};
static const __int256 kneg[] = {W256_FOLD_LIST(FOLD_NEG)};
static const __int256 knot[] = {W256_FOLD_LIST(FOLD_NOT)};
static const __int256 kshl[][W256_NSHIFT] = {W256_FOLD_LIST(FOLD_SHL_ROW)};
static const __int256 ksar[][W256_NSHIFT] = {W256_FOLD_LIST(FOLD_SAR_ROW)};
static const unsigned __int256 kshr[][W256_NSHIFT] = {
		W256_FOLD_LIST(FOLD_SHR_ROW)};
static const int kcmp[][10] = {W256_FOLD_LIST(FOLD_CMP)};

static const int kfold_pairs[][2] = {
#define FOLD_PAIR(i, j) {i, j},
		W256_FOLD_LIST(FOLD_PAIR)};

static void emit(const char *tag, int i, int j, const void *p) {
	unsigned long long w[4];
	memcpy(w, p, 32);
	printf("%s %d %d %016llx%016llx%016llx%016llx\n", tag, i, j, w[3], w[2], w[1],
				 w[0]);
}

static void emit_i(const char *tag, int i, int j, long long v) {
	printf("%s %d %d %lld\n", tag, i, j, v);
}

static __int256 load(int i) {
	__int256 v;
	unsigned long long w[4];
	memcpy(w, w256_oper[i], 32);
#ifdef MCC_W256_MUTATE
	if (i == 3)
		w[0] ^= 1ull;
#endif
	memcpy(&v, w, 32);
	return v;
}

static unsigned __int256 loadu(int i) {
	unsigned __int256 v;
	unsigned long long w[4];
	memcpy(w, w256_oper[i], 32);
#ifdef MCC_W256_MUTATE
	if (i == 3)
		w[0] ^= 1ull;
#endif
	memcpy(&v, w, 32);
	return v;
}

int main(void) {
	int i, j;
	__int256 a, b, r;
	unsigned __int256 ua, ub, ur;

	for (i = 0; i < W256_NOPER; i++) {
		for (j = 0; j < W256_NOPER; j++) {
			a = load(i);
			b = load(j);
			ua = loadu(i);
			ub = loadu(j);

			r = a + b;
			emit("add", i, j, &r);
			r = a - b;
			emit("sub", i, j, &r);
			r = a * b;
			emit("mul", i, j, &r);
			r = a & b;
			emit("and", i, j, &r);
			r = a | b;
			emit("or", i, j, &r);
			r = a ^ b;
			emit("xor", i, j, &r);
			r = a / b;
			emit("sdiv", i, j, &r);
			r = a % b;
			emit("smod", i, j, &r);
			ur = ua / ub;
			emit("udiv", i, j, &ur);
			ur = ua % ub;
			emit("umod", i, j, &ur);

			emit_i("seq", i, j, a == b);
			emit_i("sne", i, j, a != b);
			emit_i("slt", i, j, a < b);
			emit_i("sle", i, j, a <= b);
			emit_i("sgt", i, j, a > b);
			emit_i("sge", i, j, a >= b);
			emit_i("ult", i, j, ua < ub);
			emit_i("ule", i, j, ua <= ub);
			emit_i("ugt", i, j, ua > ub);
			emit_i("uge", i, j, ua >= ub);
		}
	}

	for (i = 0; i < W256_NOPER; i++) {
		a = load(i);
		ua = loadu(i);
		for (j = 0; j < W256_NSHIFT; j++) {
			long long n = w256_shift[j];
			r = a << n;
			emit("shl", i, j, &r);
			r = a >> n;
			emit("sar", i, j, &r);
			ur = ua >> n;
			emit("shr", i, j, &ur);
			ur = ua << n;
			emit("ushl", i, j, &ur);
		}
	}

	for (i = 0; i < W256_NOPER; i++) {
		a = load(i);
		ua = loadu(i);
		emit_i("tosc", i, 0, (long long)(signed char)a);
		emit_i("touc", i, 0, (long long)(unsigned char)a);
		emit_i("tos", i, 0, (long long)(short)a);
		emit_i("tous", i, 0, (long long)(unsigned short)a);
		emit_i("toi", i, 0, (long long)(int)a);
		emit_i("toui", i, 0, (long long)(unsigned int)a);
		emit_i("toll", i, 0, (long long)a);
		emit_i("toull", i, 0, (long long)(unsigned long long)a);
		emit_i("tob", i, 0, (long long)(int)(_Bool)a);
		emit_i("tobu", i, 0, (long long)(int)(_Bool)ua);
		r = (__int256)(signed char)a;
		emit("fromsc", i, 0, &r);
		r = (__int256)(unsigned char)a;
		emit("fromuc", i, 0, &r);
		r = (__int256)(short)a;
		emit("froms", i, 0, &r);
		r = (__int256)(int)a;
		emit("fromi", i, 0, &r);
		r = (__int256)(unsigned int)a;
		emit("fromui", i, 0, &r);
		r = (__int256)(long long)a;
		emit("fromll", i, 0, &r);
		r = (__int256)(unsigned long long)a;
		emit("fromull", i, 0, &r);
		ur = (unsigned __int256)a;
		emit("tou", i, 0, &ur);
		r = (__int256)ua;
		emit("tos256", i, 0, &r);
		r = -a;
		emit("neg", i, 0, &r);
		r = ~a;
		emit("not", i, 0, &r);
	}

	for (i = 0; i < W256_NFOLD; i++) {
		static const char *const cmpn[10] = {"ceq",  "cne",  "clt",  "cle",
																				 "cgt",  "cge",  "cult", "cule",
																				 "cugt", "cuge"};
		emit("cadd", i, 0, &kadd[i]);
		emit("csub", i, 0, &ksub[i]);
		emit("cmul", i, 0, &kmul[i]);
		emit("cand", i, 0, &kand[i]);
		emit("cor", i, 0, &kor[i]);
		emit("cxor", i, 0, &kxor[i]);
		emit("csdiv", i, 0, &ksdiv[i]);
		emit("csmod", i, 0, &ksmod[i]);
		emit("cudiv", i, 0, &kudiv[i]);
		emit("cumod", i, 0, &kumod[i]);
		emit("cneg", i, 0, &kneg[i]);
		emit("cnot", i, 0, &knot[i]);
		for (j = 0; j < W256_NSHIFT; j++) {
			emit("cshl", i, j, &kshl[i][j]);
			emit("csar", i, j, &ksar[i][j]);
			emit("cshr", i, j, &kshr[i][j]);
		}
		for (j = 0; j < 10; j++)
			emit_i(cmpn[j], i, 0, kcmp[i][j]);
	}
	(void)kfold_pairs;
	return 0;
}
