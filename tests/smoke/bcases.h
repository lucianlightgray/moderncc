#ifndef MCC_SMOKE_BCASES_H
#define MCC_SMOKE_BCASES_H

#include <stdio.h>

#include "smoke.h"

#define SM_BFOPS(X, A) \
	X(A, LOAD) X(A, ADD1) X(A, SUB1) X(A, COM) X(A, SHL1) X(A, SHR1) \
	X(A, MUL3) X(A, NEG) X(A, GTM1) X(A, LTZ) X(A, GEZ) X(A, EQM1) \
	X(A, SELF) X(A, TERNI) X(A, PREINC) X(A, ADDASG)

#define SM_BFE_LOAD(E, f, x) E(f)
#define SM_BFE_ADD1(E, f, x) E((f) + 1)
#define SM_BFE_SUB1(E, f, x) E((f) - 1)
#define SM_BFE_COM(E, f, x) E(~(f))
#define SM_BFE_SHL1(E, f, x) E((f) << 1)
#define SM_BFE_SHR1(E, f, x) E((f) >> 1)
#define SM_BFE_MUL3(E, f, x) E((f) * 3)
#define SM_BFE_NEG(E, f, x) E(-(f))
#define SM_BFE_GTM1(E, f, x) SM_ENC_INT((f) > -1)
#define SM_BFE_LTZ(E, f, x) SM_ENC_INT((f) < 0)
#define SM_BFE_GEZ(E, f, x) SM_ENC_INT((f) >= 0)
#define SM_BFE_EQM1(E, f, x) SM_ENC_INT((f) == -1)
#define SM_BFE_SELF(E, f, x) E((f) ? (f) + 1 : (f) - 1)
#define SM_BFE_TERNI(E, f, x) E((f) > 0 ? (f) : 0)
#define SM_BFE_PREINC(E, f, x) (++(f), E(f))
#define SM_BFE_ADDASG(E, f, x) ((f) += 1, E(f))

enum {
#define SM_BFO_ROW(a, op) SM_BFO_##op,
	SM_BFOPS(SM_BFO_ROW, _)
#undef SM_BFO_ROW
			SM_BFO_COUNT
};

#define SM_BF_TYPES(X) \
	X(BFS, int, 1) X(BFS, int, 2) X(BFS, int, 3) X(BFS, int, 4) \
	X(BFS, int, 7) X(BFS, int, 8) X(BFS, int, 9) X(BFS, int, 15) \
	X(BFS, int, 16) X(BFS, int, 17) X(BFS, int, 31) X(BFS, int, 32) \
	X(BFU, unsigned int, 1) X(BFU, unsigned int, 2) X(BFU, unsigned int, 3) \
	X(BFU, unsigned int, 4) X(BFU, unsigned int, 7) X(BFU, unsigned int, 8) \
	X(BFU, unsigned int, 9) X(BFU, unsigned int, 15) X(BFU, unsigned int, 16) \
	X(BFU, unsigned int, 17) X(BFU, unsigned int, 31) X(BFU, unsigned int, 32) \
	X(BFSL, long long, 8) X(BFSL, long long, 16) X(BFSL, long long, 31) \
	X(BFSL, long long, 32) X(BFSL, long long, 33) X(BFSL, long long, 39) \
	X(BFSL, long long, 40) X(BFSL, long long, 41) X(BFSL, long long, 47) \
	X(BFSL, long long, 48) X(BFSL, long long, 63) X(BFSL, long long, 64) \
	X(BFUL, unsigned long long, 8) X(BFUL, unsigned long long, 16) \
	X(BFUL, unsigned long long, 31) X(BFUL, unsigned long long, 32) \
	X(BFUL, unsigned long long, 33) X(BFUL, unsigned long long, 39) \
	X(BFUL, unsigned long long, 40) X(BFUL, unsigned long long, 41) \
	X(BFUL, unsigned long long, 47) X(BFUL, unsigned long long, 48) \
	X(BFUL, unsigned long long, 63) X(BFUL, unsigned long long, 64)

#define SM_BFSTRUCT(TAG, T, W) \
	typedef struct { T f : W; } SmBf_##TAG##_##W;
SM_BF_TYPES(SM_BFSTRUCT)
#undef SM_BFSTRUCT

enum {
#define SM_BFT_ROW(TAG, T, W) SM_BFT_##TAG##_##W,
	SM_BF_TYPES(SM_BFT_ROW)
#undef SM_BFT_ROW
			SM_BFT_COUNT
};

#define SM_BFKEY(t, op) ((t)*SM_BFO_COUNT + (op))

#define SM_BFENC_BFS SM_ENC_SLL
#define SM_BFENC_BFU SM_ENC_ULL
#define SM_BFENC_BFSL SM_ENC_SLL
#define SM_BFENC_BFUL SM_ENC_ULL

typedef struct
{
	const char *name;
	unsigned short t;
	unsigned short op;
	SmBits init;
	SmBits want;
} SmBfRow;

#define SM_BF(nm, TAG, W, OP, INIT, WANT) \
	{ nm, SM_BFT_##TAG##_##W, SM_BFO_##OP, (SmBits)(INIT), (SmBits)(WANT) },

static const SmBfRow smb_rows[] = {

		SM_BF("bf.u9.gt.m1", BFU, 9, GTM1, 0x1ffu, SM_ENC_INT(1))
		SM_BF("bf.u9.ltz", BFU, 9, LTZ, 0x1ffu, SM_ENC_INT(0))
		SM_BF("bf.u9.gez", BFU, 9, GEZ, 0x1ffu, SM_ENC_INT(1))
		SM_BF("bf.u9.eqm1", BFU, 9, EQM1, 0x1ffu, SM_ENC_INT(0))
		SM_BF("bf.u9.load", BFU, 9, LOAD, 0x1ffu, SM_ENC_ULL(511))
		SM_BF("bf.u31.gt.m1", BFU, 31, GTM1, 0x7fffffffu, SM_ENC_INT(1))
		SM_BF("bf.u31.load", BFU, 31, LOAD, 0x7fffffffu, SM_ENC_ULL(0x7fffffffu))
		SM_BF("bf.u32.gt.m1", BFU, 32, GTM1, 0xffffffffu, SM_ENC_INT(0))
		SM_BF("bf.u32.load", BFU, 32, LOAD, 0xffffffffu, SM_ENC_ULL(0xffffffffu))
		SM_BF("bf.u32.ltz", BFU, 32, LTZ, 0xffffffffu, SM_ENC_INT(0))
		SM_BF("bf.u1.gt.m1", BFU, 1, GTM1, 1u, SM_ENC_INT(1))
		SM_BF("bf.u16.gt.m1", BFU, 16, GTM1, 0xffffu, SM_ENC_INT(1))
		SM_BF("bf.u8.gt.m1", BFU, 8, GTM1, 0xffu, SM_ENC_INT(1))

		SM_BF("bf.s3.m4.load", BFS, 3, LOAD, 4, SM_ENC_SLL(-4))
		SM_BF("bf.s3.m4.ltz", BFS, 3, LTZ, 4, SM_ENC_INT(1))
		SM_BF("bf.s3.m4.gtm1", BFS, 3, GTM1, 4, SM_ENC_INT(0))
		SM_BF("bf.s3.m1.eqm1", BFS, 3, EQM1, 7, SM_ENC_INT(1))
		SM_BF("bf.s3.m4.neg", BFS, 3, NEG, 4, SM_ENC_SLL(4))
		SM_BF("bf.s3.m4.add1", BFS, 3, ADD1, 4, SM_ENC_SLL(-3))
		SM_BF("bf.s3.max.add1", BFS, 3, ADD1, 3, SM_ENC_SLL(4))
		SM_BF("bf.s8.min.load", BFS, 8, LOAD, 0x80, SM_ENC_SLL(-128))
		SM_BF("bf.s8.min.neg", BFS, 8, NEG, 0x80, SM_ENC_SLL(128))
		SM_BF("bf.s16.min.load", BFS, 16, LOAD, 0x8000, SM_ENC_SLL(-32768))
		SM_BF("bf.s31.min.load", BFS, 31, LOAD, 0x40000000, SM_ENC_SLL(-1073741824))
		SM_BF("bf.s32.min.load", BFS, 32, LOAD, 0x80000000u, SM_ENC_SLL(-2147483648LL))
		SM_BF("bf.s32.min.gtm1", BFS, 32, GTM1, 0x80000000u, SM_ENC_INT(0))
		SM_BF("bf.s1.m1.load", BFS, 1, LOAD, 1, SM_ENC_SLL(-1))
		SM_BF("bf.s1.m1.eqm1", BFS, 1, EQM1, 1, SM_ENC_INT(1))

		SM_BF("bf.ul40.max.load", BFUL, 40, LOAD, 0xffffffffffull,
					SM_ENC_ULL(0xffffffffffull))
		SM_BF("bf.ul40.max.add1", BFUL, 40, ADD1, 0xffffffffffull, SM_ENC_ULL(0))
		SM_BF("bf.ul40.max.com", BFUL, 40, COM, 0xffffffffffull, SM_ENC_ULL(0))
		SM_BF("bf.ul40.max.shl1", BFUL, 40, SHL1, 0xffffffffffull,
					SM_ENC_ULL(0xfffffffffeull))
		SM_BF("bf.ul40.max.shr1", BFUL, 40, SHR1, 0xffffffffffull,
					SM_ENC_ULL(0x7fffffffffull))
		SM_BF("bf.ul40.max.gtm1", BFUL, 40, GTM1, 0xffffffffffull, SM_ENC_INT(0))
		SM_BF("bf.ul40.max.eqm1", BFUL, 40, EQM1, 0xffffffffffull, SM_ENC_INT(1))
		SM_BF("bf.ul33.max.eqm1", BFUL, 33, EQM1, 0x1ffffffffull, SM_ENC_INT(1))
		SM_BF("bf.ul63.max.eqm1", BFUL, 63, EQM1, 0x7fffffffffffffffull, SM_ENC_INT(1))
		SM_BF("bf.ul64.max.eqm1", BFUL, 64, EQM1, 0xffffffffffffffffull, SM_ENC_INT(1))
		SM_BF("bf.ul40.max.ltz", BFUL, 40, LTZ, 0xffffffffffull, SM_ENC_INT(0))
		SM_BF("bf.ul33.max.add1", BFUL, 33, ADD1, 0x1ffffffffull, SM_ENC_ULL(0))
		SM_BF("bf.ul33.max.com", BFUL, 33, COM, 0x1ffffffffull, SM_ENC_ULL(0))
		SM_BF("bf.ul48.max.add1", BFUL, 48, ADD1, 0xffffffffffffull, SM_ENC_ULL(0))
		SM_BF("bf.ul63.max.add1", BFUL, 63, ADD1, 0x7fffffffffffffffull,
					SM_ENC_ULL(0))
		SM_BF("bf.ul64.max.add1", BFUL, 64, ADD1, 0xffffffffffffffffull,
					SM_ENC_ULL(0))
		SM_BF("bf.ul64.max.com", BFUL, 64, COM, 0xffffffffffffffffull,
					SM_ENC_ULL(0))

		SM_BF("bf.sl40.min.load", BFSL, 40, LOAD, 0x8000000000ull,
					SM_ENC_SLL(-549755813888LL))
		SM_BF("bf.sl40.min.neg", BFSL, 40, NEG, 0x8000000000ull,
					SM_ENC_SLL(-549755813888LL))
		SM_BF("bf.sl40.m1.load", BFSL, 40, LOAD, 0xffffffffffull, SM_ENC_SLL(-1))
		SM_BF("bf.sl40.m1.gtm1", BFSL, 40, GTM1, 0xffffffffffull, SM_ENC_INT(0))
		SM_BF("bf.sl40.m1.add1", BFSL, 40, ADD1, 0xffffffffffull, SM_ENC_SLL(0))
		SM_BF("bf.sl33.m1.load", BFSL, 33, LOAD, 0x1ffffffffull, SM_ENC_SLL(-1))
		SM_BF("bf.sl63.min.load", BFSL, 63, LOAD, 0x4000000000000000ull,
					SM_ENC_SLL(-4611686018427387904LL))
		SM_BF("bf.sl64.min.load", BFSL, 64, LOAD, 0x8000000000000000ull,
					SM_ENC_SLL(LLONG_MIN))
		SM_BF("bf.sl64.min.neg", BFSL, 64, NEG, 0x8000000000000000ull,
					SM_ENC_SLL(LLONG_MIN))

		SM_BF("bf.s3.terni.neg", BFS, 3, TERNI, 4, SM_ENC_SLL(0))
		SM_BF("bf.s3.terni.pos", BFS, 3, TERNI, 3, SM_ENC_SLL(3))
		SM_BF("bf.u9.self", BFU, 9, SELF, 0x1ffu, SM_ENC_ULL(512))
		SM_BF("bf.s3.preinc.max", BFS, 3, PREINC, 3, SM_ENC_SLL(-4))
		SM_BF("bf.u9.preinc.max", BFU, 9, PREINC, 0x1ffu, SM_ENC_ULL(0))
		SM_BF("bf.ul40.addasg.max", BFUL, 40, ADDASG, 0xffffffffffull,
					SM_ENC_ULL(0))
};

#define SM_BFARM(TAG, T, W, OP) \
	case SM_BFKEY(SM_BFT_##TAG##_##W, SM_BFO_##OP): { \
		volatile SmBf_##TAG##_##W s; \
		s.f = (T)(init); \
		return SM_BFE_##OP(SM_BFENC_##TAG, s.f, 0); \
	}

#define SM_BFARM_ONE(TAG, T, W) \
	SM_BFARM(TAG, T, W, LOAD) \
	SM_BFARM(TAG, T, W, ADD1) \
	SM_BFARM(TAG, T, W, SUB1) \
	SM_BFARM(TAG, T, W, COM) \
	SM_BFARM(TAG, T, W, SHL1) \
	SM_BFARM(TAG, T, W, SHR1) \
	SM_BFARM(TAG, T, W, MUL3) \
	SM_BFARM(TAG, T, W, NEG) \
	SM_BFARM(TAG, T, W, GTM1) \
	SM_BFARM(TAG, T, W, LTZ) \
	SM_BFARM(TAG, T, W, GEZ) \
	SM_BFARM(TAG, T, W, EQM1) \
	SM_BFARM(TAG, T, W, SELF) \
	SM_BFARM(TAG, T, W, TERNI) \
	SM_BFARM(TAG, T, W, PREINC) \
	SM_BFARM(TAG, T, W, ADDASG)

static SmBits smb_run(int t, int op, SmBits init)
{
	switch (SM_BFKEY(t, op)) {
		SM_BF_TYPES(SM_BFARM_ONE)
	}
	return ~(SmBits)0;
}

static const char *const smb_type_name[] = {
#define SM_BFN(TAG, T, W) #TAG "_" #W,
		SM_BF_TYPES(SM_BFN)
#undef SM_BFN
};

static const char *const smb_op_name[] = {
#define SM_BFON(a, op) #op,
		SM_BFOPS(SM_BFON, _)
#undef SM_BFON
};

#define SMB_ROWS_N ((int)(sizeof smb_rows / sizeof smb_rows[0]))

static const SmBits smb_corpus[] = {
		0ull, 1ull, 2ull, 3ull, 4ull, 7ull, 0x7full, 0x80ull, 0xffull,
		0x7fffull, 0x8000ull, 0xffffull, 0x7fffffffull, 0x80000000ull,
		0xffffffffull, 0x1ffffffffull, 0x7fffffffffull, 0x8000000000ull,
		0xffffffffffull, 0x7fffffffffffffffull, 0x8000000000000000ull,
		0xffffffffffffffffull, 0xaaaaaaaaaaaaaaaaull, 0x5555555555555555ull};

#define SMB_CORPUS_N ((int)(sizeof smb_corpus / sizeof smb_corpus[0]))

static long smb_rows_run(long *checks, long *failures, long *reported,
												 int poison)
{
	long n = 0;
	int i;
	for (i = 0; i < SMB_ROWS_N; i++) {
		const SmBfRow *r = &smb_rows[i];
		SmBits want = r->want;
		SmBits got;
		if (poison && i == 0)
			want ^= 1ull;
		got = smb_run(r->t, r->op, r->init);
		(*checks)++;
		n++;
		if (got != want) {
			(*failures)++;
			if ((*reported)++ < 40)
				printf("FAIL bfrun %s got=%016llx want=%016llx\n", r->name,
							 (unsigned long long)got, (unsigned long long)want);
		}
	}
	return n;
}

static long smb_sweep(SmBits *digest)
{
	int t, op, i;
	long n = 0;
	for (t = 0; t < SM_BFT_COUNT; t++)
		for (op = 0; op < SM_BFO_COUNT; op++)
			for (i = 0; i < SMB_CORPUS_N; i++) {
				SmBits r = smb_run(t, op, smb_corpus[i]);
				*digest = (*digest ^ (SmBits)SM_BFKEY(t, op)) * 1099511628211ull;
				*digest = (*digest ^ smb_corpus[i]) * 1099511628211ull;
				*digest = (*digest ^ r) * 1099511628211ull;
				n++;
			}
	return n;
}

static void smb_row_dump(void)
{
	int i, t, op;
	for (i = 0; i < SMB_ROWS_N; i++) {
		const SmBfRow *r = &smb_rows[i];
		printf("B %s 0 %016llx %016llx %016llx\n", r->name,
					 (unsigned long long)r->want,
					 (unsigned long long)smb_run(r->t, r->op, r->init),
					 (unsigned long long)r->want);
	}
	for (t = 0; t < SM_BFT_COUNT; t++)
		for (op = 0; op < SM_BFO_COUNT; op++) {
			SmBits h = 14695981039346656037ull;
			for (i = 0; i < SMB_CORPUS_N; i++)
				h = (h ^ smb_run(t, op, smb_corpus[i])) * 1099511628211ull;
			printf("B bfsweep.%s.%s 0 %016llx %016llx %016llx\n", smb_type_name[t],
						 smb_op_name[op], (unsigned long long)h, (unsigned long long)h,
						 (unsigned long long)h);
		}
}

static int smb_rows_count(void)
{
	return SMB_ROWS_N;
}

#endif
