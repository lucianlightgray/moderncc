#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "cases.h"
#include "bcases.h"
#include "fcases.h"
#include "scases.h"

static const char *const sm_type_name[] = {
#define SM_TN(tag, cty, w) #tag,
		SM_ITYPES(SM_TN)
#undef SM_TN
};

static const int sm_type_width[] = {
#define SM_TW(tag, cty, w) w,
		SM_ITYPES(SM_TW)
#undef SM_TW
};

static const char *const sm_op_name[] = {
#define SM_ON(a, op) #op,
		SM_OPS(SM_ON, _)
#undef SM_ON
};

static const char *const sm_sop_name[] = {
#define SM_SN(a, op) #op,
		SM_SOPS(SM_SN, _)
#undef SM_SN
};

static const char *const sm_pair_name[] = {
#define SM_PN(tag, l, r, e) #tag,
		SM_MIXPAIRS(SM_PN)
#undef SM_PN
};

static const char *const sm_mixop_name[] = {
#define SM_MN(a, op) #op,
		SM_MIXOPS(SM_MN, _)
#undef SM_MN
};

static const SmBits sm_corpus[] = {
		0ull, 1ull, 2ull, 3ull, 7ull, 0x7full, 0x80ull, 0xffull,
		0x7fffull, 0x8000ull, 0xffffull, 0x5555ull,
		0x7ffffffeull, 0x7fffffffull, 0x80000000ull, 0x80000001ull,
		0xfffffffeull, 0xffffffffull, 0xaaaaaaaaull,
		0x7ffffffffffffffeull, 0x7fffffffffffffffull,
		0x8000000000000000ull, 0x8000000000000001ull, 0xffffffffffffffffull};

#define SM_CORPUS_N ((int)(sizeof sm_corpus / sizeof sm_corpus[0]))

static long g_checks;
static long g_failures;
static long g_reported;
static int g_verbose;
static int g_poison;

static void sm_fail(const char *what, const char *name, SmBits got, SmBits want)
{
	g_failures++;
	if (g_reported < 40) {
		g_reported++;
		printf("FAIL %s %s got=%016llx want=%016llx\n", what, name,
					 (unsigned long long)got, (unsigned long long)want);
	}
}

#define SM_ARM(TY, OP) \
	case SM_KEY(SM_T_##TY, SM_O_##OP): { \
		volatile SM_CTY_##TY va = (SM_CTY_##TY)(a); \
		volatile SM_CTY_##TY vb = (SM_CTY_##TY)(b); \
		volatile SM_CTY_##TY vc = (SM_CTY_##TY)(c); \
		return SM_EXPR_##OP(SM_CTY_##TY, SM_ENC_##TY, va, vb, vc); \
	}

#define SM_ARMS_TY(tag, cty, w) SM_OPS(SM_ARM, tag)

static SmBits sm_run(int tag, int op, SmBits a, SmBits b, SmBits c)
{
	switch (SM_KEY(tag, op)) {
		SM_ITYPES(SM_ARMS_TY)
	}
	return ~(SmBits)0;
}

#define SM_SARM(TY, OP) \
	case SM_SKEY(SM_T_##TY, SM_S_##OP): { \
		volatile SM_CTY_##TY v = (SM_CTY_##TY)(a); \
		return SM_SEXPR_##OP(SM_CTY_##TY, SM_ENC_##TY, v, b); \
	}

#define SM_SARMS_TY(tag, cty, w) SM_SOPS(SM_SARM, tag)

static SmBits sm_srun(int tag, int op, SmBits a, SmBits b)
{
	switch (SM_SKEY(tag, op)) {
		SM_ITYPES(SM_SARMS_TY)
	}
	return ~(SmBits)0;
}

#define SM_MARM(PAIR, OP) \
	case SM_MKEY(SM_P_##PAIR, SM_M_##OP): { \
		volatile SM_LTY_##PAIR va = (SM_LTY_##PAIR)(a); \
		volatile SM_RTY_##PAIR vb = (SM_RTY_##PAIR)(b); \
		return SM_MEXPR_##OP(SM_LTY_##PAIR, SM_RTY_##PAIR, SM_MENC_##PAIR, va, vb); \
	}

#define SM_MARMS_P(tag, l, r, e) SM_MIXOPS(SM_MARM, tag)

static SmBits sm_mrun(int pair, int op, SmBits a, SmBits b)
{
	switch (SM_MKEY(pair, op)) {
		SM_MIXPAIRS(SM_MARMS_P)
	}
	return ~(SmBits)0;
}

static SmBits sm_mask(int tag, SmBits v)
{
	int w = sm_type_width[tag];
	if (w >= 64)
		return v;
	return v & ((1ull << w) - 1ull);
}

static int sm_zero_divisor(int tag, int op, SmBits b)
{
	if (!SM_OP_NEEDS_NZ_B(op))
		return 0;
	return sm_mask(tag, b) == 0ull;
}

static int sm_idiv_trap(int tag, int op, SmBits a, SmBits b)
{
	int w;
	if (!SM_OP_NEEDS_NZ_B(op))
		return 0;
	if (tag != SM_T_SI && tag != SM_T_SL && tag != SM_T_SLL)
		return 0;
	w = sm_type_width[tag];
	if (sm_mask(tag, b) != sm_mask(tag, (SmBits)-1))
		return 0;
	return sm_mask(tag, a) == (w >= 64 ? 0x8000000000000000ull : (1ull << (w - 1)));
}

static int sm_mix_zero_divisor(int op, SmBits b)
{
	(void)op;
	(void)b;
	return 0;
}

static SmBits g_digest;
static SmBits g_dpart[7];

static void sm_mix_digest(SmBits v)
{
	g_digest = (g_digest ^ v) * 1099511628211ull;
}

static void sm_check(const char *what, const char *name, SmBits got, SmBits want)
{
	g_checks++;
	if (got != want)
		sm_fail(what, name, got, want);
}

static void sm_rows_run(void)
{
	int i;
	for (i = 0; i < sm_rows_count; i++) {
		const SmRow *r = &sm_rows[i];
		SmBits want = r->want;
		if (g_poison && i == 0)
			want ^= 1ull;
		if (r->foldable)
			sm_check("fold", r->name, r->fold, want);
		sm_check("run", r->name, sm_run(r->tag, r->op, r->a, r->b, r->c), want);
	}
	for (i = 0; i < sm_mix_rows_count; i++) {
		const SmMixRow *r = &sm_mix_rows[i];
		SmBits want = r->want;
		if (r->foldable)
			sm_check("mixfold", r->name, r->fold, want);
		sm_check("mixrun", r->name, sm_mrun(r->pair, r->op, r->a, r->b), want);
	}
}

static void sm_init_rows_run(void)
{
	int i;
	for (i = 0; i < sm_init_rows_count; i++) {
		const SmIRow *r = &sm_init_rows[i];
		sm_check("init", r->name, (SmBits)(long long)r->base[r->idx], r->want);
	}
}

#if defined __GNUC__ || defined __MCC__
#define SM_HAVE_CGOTO 1
#else
#define SM_HAVE_CGOTO 0
#endif

#if SM_HAVE_CGOTO
static SmBits sm_cgoto(int n)
{
	static const void *const tab[] = {&&L0, &&L1, &&L2, &&L3};
	volatile SmBits acc = 0;
	volatile int k = n & 3;
	goto *tab[k];
L0:
	acc += 1;
	goto Lend;
L1:
	acc += 10;
	goto Lend;
L2:
	acc += 100;
	goto Lend;
L3:
	acc += 1000;
Lend:
	return acc;
}

static void sm_cgoto_run(void)
{
	static const SmBits want[4] = {1, 10, 100, 1000};
	int i;
	for (i = 0; i < 8; i++)
		sm_check("cgoto", "cgoto.dispatch", sm_cgoto(i), want[i & 3]);
}
#else
static void sm_cgoto_run(void) {}
#endif

static long sm_sweep(void)
{
	int tag, op, i, j;
	long n = 0;
	for (tag = 0; tag < SM_T_COUNT; tag++)
		for (op = 0; op < SM_O_COUNT; op++)
			for (i = 0; i < SM_CORPUS_N; i++)
				for (j = 0; j < SM_CORPUS_N; j++) {
					SmBits a = sm_corpus[i], b = sm_corpus[j], r;
					if (sm_zero_divisor(tag, op, b) || sm_idiv_trap(tag, op, a, b))
						continue;
					r = sm_run(tag, op, a, b, sm_corpus[(i + j + 1) % SM_CORPUS_N]);
					sm_mix_digest((SmBits)SM_KEY(tag, op));
					sm_mix_digest(a);
					sm_mix_digest(b);
					sm_mix_digest(r);
					n++;
				}
	return n;
}

static long sm_ssweep(void)
{
	int tag, op, i, j;
	long n = 0;
	for (tag = 0; tag < SM_T_COUNT; tag++)
		for (op = 0; op < SM_S_COUNT; op++)
			for (i = 0; i < SM_CORPUS_N; i++)
				for (j = 0; j < SM_CORPUS_N; j++) {
					SmBits a = sm_corpus[i], b = sm_corpus[j], r;
					r = sm_srun(tag, op, a, b);
					sm_mix_digest((SmBits)SM_SKEY(tag, op) ^ 0x517cc1b727220a95ull);
					sm_mix_digest(a);
					sm_mix_digest(b);
					sm_mix_digest(r);
					n++;
				}
	return n;
}

static long sm_msweep(void)
{
	int p, op, i, j;
	long n = 0;
	for (p = 0; p < SM_P_COUNT; p++)
		for (op = 0; op < SM_M_COUNT; op++)
			for (i = 0; i < SM_CORPUS_N; i++)
				for (j = 0; j < SM_CORPUS_N; j++) {
					SmBits a = sm_corpus[i], b = sm_corpus[j], r;
					if (sm_mix_zero_divisor(op, b))
						continue;
					r = sm_mrun(p, op, a, b);
					sm_mix_digest((SmBits)SM_MKEY(p, op) ^ 0x9e3779b97f4a7c15ull);
					sm_mix_digest(a);
					sm_mix_digest(b);
					sm_mix_digest(r);
					n++;
				}
	return n;
}

static void sm_digest_dump(void)
{
	int tag, op, i, j, p;
	for (tag = 0; tag < SM_T_COUNT; tag++)
		for (op = 0; op < SM_O_COUNT; op++) {
			SmBits h = 14695981039346656037ull;
			long n = 0;
			for (i = 0; i < SM_CORPUS_N; i++)
				for (j = 0; j < SM_CORPUS_N; j++) {
					SmBits a = sm_corpus[i], b = sm_corpus[j], r;
					if (sm_zero_divisor(tag, op, b) || sm_idiv_trap(tag, op, a, b))
						continue;
					r = sm_run(tag, op, a, b, sm_corpus[(i + j + 1) % SM_CORPUS_N]);
					h = (h ^ r) * 1099511628211ull;
					n++;
				}
			printf("D int %s %s %ld %016llx\n", sm_type_name[tag], sm_op_name[op], n,
						 (unsigned long long)h);
		}
	for (p = 0; p < SM_P_COUNT; p++)
		for (op = 0; op < SM_M_COUNT; op++) {
			SmBits h = 14695981039346656037ull;
			long n = 0;
			for (i = 0; i < SM_CORPUS_N; i++)
				for (j = 0; j < SM_CORPUS_N; j++) {
					SmBits r = sm_mrun(p, op, sm_corpus[i], sm_corpus[j]);
					h = (h ^ r) * 1099511628211ull;
					n++;
				}
			printf("D mix %s %s %ld %016llx\n", sm_pair_name[p], sm_mixop_name[op], n,
						 (unsigned long long)h);
		}
}

static void sm_row_dump(void)
{
	int i;
	for (i = 0; i < sm_rows_count; i++) {
		const SmRow *r = &sm_rows[i];
		SmBits got = sm_run(r->tag, r->op, r->a, r->b, r->c);
		if (g_poison && i == 0)
			got ^= 1ull;
		printf("R %s %d %016llx %016llx %016llx\n", r->name, (int)r->foldable,
					 (unsigned long long)r->fold, (unsigned long long)got,
					 (unsigned long long)r->want);
	}
	for (i = 0; i < sm_init_rows_count; i++) {
		const SmIRow *r = &sm_init_rows[i];
		printf("I %s 0 %016llx %016llx %016llx\n", r->name,
					 (unsigned long long)r->want,
					 (unsigned long long)(long long)r->base[r->idx],
					 (unsigned long long)r->want);
	}
	for (i = 0; i < sm_mix_rows_count; i++) {
		const SmMixRow *r = &sm_mix_rows[i];
		printf("M %s %d %016llx %016llx %016llx\n", r->name, (int)r->foldable,
					 (unsigned long long)r->fold,
					 (unsigned long long)sm_mrun(r->pair, r->op, r->a, r->b),
					 (unsigned long long)r->want);
	}
	for (i = 0; i < SM_T_COUNT; i++) {
		int op, j, k;
		for (op = 0; op < SM_S_COUNT; op++) {
			SmBits h = 14695981039346656037ull;
			for (j = 0; j < SM_CORPUS_N; j++)
				for (k = 0; k < SM_CORPUS_N; k++)
					h = (h ^ sm_srun(i, op, sm_corpus[j], sm_corpus[k])) *
							1099511628211ull;
			printf("S ssweep.%s.%s 0 %016llx %016llx %016llx\n", sm_type_name[i],
						 sm_sop_name[op], (unsigned long long)h, (unsigned long long)h,
						 (unsigned long long)h);
		}
	}
	smf_row_dump();
	smb_row_dump();
}

#define SM_TRAP_EXIT 97

static void sm_on_fpe(int sig)
{
	(void)sig;
	_Exit(SM_TRAP_EXIT);
}

static int sm_trap_fire(int idx)
{
	const SmTrapRow *r;
	SmBits v;
	if (idx < 0 || idx >= sm_trap_rows_count)
		return 2;
	r = &sm_trap_rows[idx];
	signal(SIGFPE, sm_on_fpe);
	v = sm_run(r->tag, r->op, r->a, r->b, r->c);
	printf("NOTRAP %s %016llx\n", r->name, (unsigned long long)v);
	return 0;
}

int main(int argc, char **argv)
{
	long nsweep = 0, nmsweep = 0, nf = 0;
	int i, mode_digest = 0, mode_rows = 0, trap = -1;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--digest"))
			mode_digest = 1;
		else if (!strcmp(argv[i], "--dump"))
			mode_rows = 1;
		else if (!strcmp(argv[i], "--verbose"))
			g_verbose = 1;
		else if (!strcmp(argv[i], "--poison"))
			g_poison = 1;
		else if (!strcmp(argv[i], "--trap") && i + 1 < argc)
			trap = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--trapnames")) {
			int k;
			for (k = 0; k < sm_trap_rows_count; k++)
				if (SM_DIV_TRAPS)
					printf("%s\tfault\n", sm_trap_rows[k].name);
				else
					printf("%s\t%016llx\n", sm_trap_rows[k].name,
								 (unsigned long long)sm_trap_rows[k].nofault);
			return 0;
		} else {
			fprintf(stderr, "smoke subject: unknown argument '%s'\n", argv[i]);
			return 2;
		}
	}

	if (trap >= 0)
		return sm_trap_fire(trap);

	if (mode_rows) {
		sm_row_dump();
		return 0;
	}

	g_digest = 14695981039346656037ull;
	sm_rows_run();
	sm_init_rows_run();
	sm_cgoto_run();
	nf = smf_rows_run(&g_checks, &g_failures, &g_reported, g_poison);
	nf += smb_rows_run(&g_checks, &g_failures, &g_reported, g_poison);
	nsweep = sm_sweep();
	g_dpart[0] = g_digest;
	nsweep += sm_ssweep();
	g_dpart[1] = g_digest;
	nsweep += smb_sweep(&g_digest);
	g_dpart[2] = g_digest;
	nf += smf_sweep(&g_digest);
	g_dpart[3] = g_digest;
	nf += smc_sweep(&g_digest);
	g_dpart[4] = g_digest;
	nsweep += sms_sweep(&g_digest);
	g_dpart[5] = g_digest;
	nmsweep = sm_msweep();
	g_dpart[6] = g_digest;

	if (mode_digest) {
		sm_digest_dump();
		smf_digest_dump();
	}

	printf("smoke: checks=%ld sweep=%ld msweep=%ld fchecks=%ld failures=%ld "
				 "digest=%016llx\n",
				 g_checks, nsweep, nmsweep, nf, g_failures,
				 (unsigned long long)g_digest);
	if (g_verbose)
		printf("smoke: parts=%016llx %016llx %016llx %016llx %016llx %016llx "
					 "%016llx\n",
					 (unsigned long long)g_dpart[0], (unsigned long long)g_dpart[1],
					 (unsigned long long)g_dpart[2], (unsigned long long)g_dpart[3],
					 (unsigned long long)g_dpart[4], (unsigned long long)g_dpart[5],
					 (unsigned long long)g_dpart[6]);
	if (g_verbose)
		printf("smoke: types=%d ops=%d pairs=%d mixops=%d corpus=%d rows=%d "
					 "mixrows=%d traprows=%d frows=%d\n",
					 SM_T_COUNT, SM_O_COUNT, SM_P_COUNT, SM_M_COUNT, SM_CORPUS_N,
					 sm_rows_count, sm_mix_rows_count, sm_trap_rows_count,
					 smf_rows_count() + smb_rows_count());
	return g_failures ? 1 : 0;
}
