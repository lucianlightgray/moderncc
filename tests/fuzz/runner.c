#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../support/hostcompat.h"
#include "gen.h"

#ifdef _WIN32
#define EXE_SFX ".exe"
#ifndef WIFEXITED
#define WIFEXITED(s) 1
#define WEXITSTATUS(s) (s)
#endif
#else
#include <sys/wait.h>
#define EXE_SFX ".out"
#endif

#define MCC_SKIP_RC 77
#define RUN_TIMEOUT 20

#define RUN_SYSTEM(c) HC_SYSTEM_SH(c)
#define RUN_POPEN(c) HC_POPEN_SH(c)

static int verbose;

static int timed_out(int raw) {
	return WIFEXITED(raw) && (WEXITSTATUS(raw) == 124 || WEXITSTATUS(raw) == 137);
}

static int crashed(int raw) {
	if (timed_out(raw))
		return 0;
#ifdef WIFSIGNALED
	if (WIFSIGNALED(raw))
		return 1;
#endif
	return WIFEXITED(raw) && WEXITSTATUS(raw) >= 128;
}

static char *slurp(FILE *f) {
	size_t cap = 4096, len = 0;
	char *b = malloc(cap);
	size_t n;
	while ((n = fread(b + len, 1, cap - len, f)) > 0) {
		len += n;
		if (len == cap) {
			cap *= 2;
			b = realloc(b, cap);
		}
	}
	b[len] = 0;
	return b;
}

static char *cap_cmd(const char *cmd, int *status) {
	FILE *f = RUN_POPEN(cmd);
	if (!f) {
		if (status)
			*status = -1;
		return strdup("");
	}
	char *o = slurp(f);
	int rc = pclose(f);
	if (status)
		*status = rc;
	return o;
}

static void timeout_wrap(const char *cmd, char *out, size_t n) {
	static int probed, have_timeout, have_gtimeout;
	if (!probed) {
		have_timeout = RUN_SYSTEM("command -v timeout  >/dev/null 2>&1") == 0;
		have_gtimeout = RUN_SYSTEM("command -v gtimeout >/dev/null 2>&1") == 0;
		probed = 1;
	}
	if (have_timeout)
		snprintf(out, n, "timeout %d %s", RUN_TIMEOUT, cmd);
	else if (have_gtimeout)
		snprintf(out, n, "gtimeout %d %s", RUN_TIMEOUT, cmd);
	else
		snprintf(out, n,
				 "{ %s & __p=$!; ( sleep %d; kill -9 $__p ) >/dev/null 2>&1 & __w=$!; "
				 "wait $__p; __r=$?; kill $__w 2>/dev/null; exit $__r; }",
				 cmd, RUN_TIMEOUT);
}

enum { RES_BUILDFAIL = 0, RES_OK = 1, RES_INCONCLUSIVE = 2 };

typedef struct {
	int kind;
	char *out;
	int exit_code;
} runres;

static void runres_free(runres *r) {
	free(r->out);
	r->out = NULL;
}

static int runres_eq(const runres *a, const runres *b) {
	return a->exit_code == b->exit_code && !strcmp(a->out, b->out);
}

static runres build_run(const char *cc, const char *mcc, const char *bdir,
						const char *idir, const char *env, const char *opt,
						const char *work, const char *label, const char *src) {
	runres res = {RES_INCONCLUSIVE, strdup(""), 0};
	char exe[2048], cmd[8192], guarded[8448];
	snprintf(exe, sizeof exe, "%s/%s%s", work, label, EXE_SFX);
	remove(exe);
	if (cc)
		snprintf(cmd, sizeof cmd, "\"%s\" -w %s \"%s\" -o \"%s\" -lm >/dev/null 2>&1",
				 cc, opt, src, exe);
	else
		snprintf(cmd, sizeof cmd,
				 "%s \"%s\" \"-B%s\" \"-I%s\" %s \"%s\" -o \"%s\" >/dev/null 2>&1",
				 env ? env : "", mcc, bdir, idir, opt, src, exe);
	timeout_wrap(cmd, guarded, sizeof guarded);
	int brc = RUN_SYSTEM(guarded);
	if (timed_out(brc)) {
		res.kind = RES_INCONCLUSIVE;
		return res;
	}
	if (brc != 0) {
		res.kind = RES_BUILDFAIL;
		return res;
	}
	char prog[8192], grun[8448], run[12288];
	int rrc;
	snprintf(prog, sizeof prog, "\"%s\"", exe);
	timeout_wrap(prog, grun, sizeof grun);
	snprintf(run, sizeof run, "cd \"%s\" && %s", work, grun);
	free(res.out);
	res.out = cap_cmd(run, &rrc);
	if (timed_out(rrc) || crashed(rrc)) {
		res.kind = RES_INCONCLUSIVE;
		return res;
	}
	res.exit_code = WIFEXITED(rrc) ? WEXITSTATUS(rrc) : -1;
	res.kind = RES_OK;
	return res;
}

static int has_ub(const char *gcc, const char *work, const char *src) {
	char exe[2048], cmd[8192], guarded[8448];
	snprintf(exe, sizeof exe, "%s/ubchk%s", work, EXE_SFX);
	remove(exe);
	snprintf(cmd, sizeof cmd,
			 "\"%s\" -w -O1 -fsanitize=undefined,address "
			 "-fno-sanitize-recover=all \"%s\" -o \"%s\" -lm >/dev/null 2>&1",
			 gcc, src, exe);
	timeout_wrap(cmd, guarded, sizeof guarded);
	if (RUN_SYSTEM(guarded) != 0)
		return 0;
	char prog[8192], grun[8448], run[12288];
	int rrc;
	snprintf(prog, sizeof prog, "\"%s\" 2>&1", exe);
	timeout_wrap(prog, grun, sizeof grun);
	snprintf(run, sizeof run, "cd \"%s\" && %s", work, grun);
	char *o = cap_cmd(run, &rrc);
	int ub = crashed(rrc) || strstr(o, "runtime error") != NULL ||
			 strstr(o, "AddressSanitizer") != NULL ||
			 strstr(o, "UndefinedBehaviorSanitizer") != NULL;
	free(o);
	return ub;
}

static const char *const OPTS[] = {"-O0", "-O1", "-O2", "-O3"};
#define NOPTS ((int)(sizeof OPTS / sizeof *OPTS))

typedef struct {
	const char *name;
	const char *env;
} gate_t;

static const gate_t GATES[] = {
	{"CPROP_JOIN", "MCC_AST_CPROP_JOIN=1"},
	{"CSE_JOIN", "MCC_AST_CSE_JOIN=1"},
	{"CALL_WINDOW", "MCC_AST_CALL_WINDOW=1"},
	{"BITFLAG", "MCC_AST_BITFLAG=1"},
	{"SETHI", "MCC_AST_SETHI=1"},
	{"PROMOTE", "MCC_AST_PROMOTE=1"},
	{"INLINE", "MCC_AST_INLINE=1"},
};
#define NGATES ((int)(sizeof GATES / sizeof *GATES))

static int consensus(const char *gcc, const char *clang, const char *bdir,
					 const char *idir, const char *work, const char *src,
					 runres *cons) {
	runres g = build_run(gcc, NULL, bdir, idir, NULL, "-O2", work, "ref_gcc", src);
	runres c = build_run(clang, NULL, bdir, idir, NULL, "-O2", work, "ref_clang", src);
	int ok = g.kind == RES_OK && c.kind == RES_OK && runres_eq(&g, &c);
	if (ok) {
		*cons = g;
		runres_free(&c);
		return 1;
	}
	runres_free(&g);
	runres_free(&c);
	return 0;
}

static int mcc_diverges(const char *mcc, const char *bdir, const char *idir,
						const char *work, const char *src, const runres *cons,
						const char *env, const char *opt) {
	runres m = build_run(mcc, NULL, bdir, idir, env, opt, work, "mcc", src);
	int div = m.kind == RES_BUILDFAIL || m.kind == RES_INCONCLUSIVE ||
			  !runres_eq(&m, cons);
	runres_free(&m);
	return div;
}

typedef struct {
	int found;
	char opt[8];
	char gate[32];
} attribution;

static attribution triage(const char *mcc, const char *gcc, const char *clang,
						  const char *bdir, const char *idir, const char *work,
						  const char *src) {
	attribution a;
	a.found = 0;
	a.opt[0] = a.gate[0] = 0;
	runres cons;
	if (!consensus(gcc, clang, bdir, idir, work, src, &cons))
		return a;
	for (int i = 0; i < NOPTS; i++) {
		if (mcc_diverges(mcc, bdir, idir, work, src, &cons, NULL, OPTS[i])) {
			a.found = 1;
			snprintf(a.opt, sizeof a.opt, "%s", OPTS[i]);
			snprintf(a.gate, sizeof a.gate, "(default gates)");
			runres_free(&cons);
			return a;
		}
	}
	for (int g = 0; g < NGATES; g++) {
		for (int i = 0; i < NOPTS; i++) {
			if (mcc_diverges(mcc, bdir, idir, work, src, &cons, GATES[g].env,
							 OPTS[i])) {
				a.found = 1;
				snprintf(a.opt, sizeof a.opt, "%s", OPTS[i]);
				snprintf(a.gate, sizeof a.gate, "%s", GATES[g].name);
				runres_free(&cons);
				return a;
			}
		}
	}
	runres_free(&cons);
	return a;
}

typedef struct {
	char **line;
	int n;
} doc;

static doc doc_read(const char *path) {
	doc d = {NULL, 0};
	FILE *f = fopen(path, "rb");
	if (!f)
		return d;
	char *all = slurp(f);
	fclose(f);
	int cap = 64;
	d.line = malloc(sizeof(char *) * cap);
	char *p = all;
	while (*p) {
		char *nl = strchr(p, '\n');
		size_t len = nl ? (size_t)(nl - p) + 1 : strlen(p);
		char *s = malloc(len + 1);
		memcpy(s, p, len);
		s[len] = 0;
		if (d.n == cap) {
			cap *= 2;
			d.line = realloc(d.line, sizeof(char *) * cap);
		}
		d.line[d.n++] = s;
		if (!nl)
			break;
		p = nl + 1;
	}
	free(all);
	return d;
}

static void doc_write(const doc *d, const char *path, const char *keep) {
	FILE *f = fopen(path, "wb");
	for (int i = 0; i < d->n; i++)
		if (keep[i])
			fputs(d->line[i], f);
	fclose(f);
}

static int interesting(const char *mcc, const char *gcc, const char *clang,
					   const char *bdir, const char *idir, const char *work,
					   const char *cand) {
	runres cons;
	if (!consensus(gcc, clang, bdir, idir, work, cand, &cons))
		return 0;
	if (has_ub(gcc, work, cand)) {
		runres_free(&cons);
		return 0;
	}
	int div = 0;
	for (int i = 0; i < NOPTS && !div; i++)
		div = mcc_diverges(mcc, bdir, idir, work, cand, &cons, NULL, OPTS[i]);
	for (int g = 0; g < NGATES && !div; g++)
		for (int i = 0; i < NOPTS && !div; i++)
			div = mcc_diverges(mcc, bdir, idir, work, cand, &cons, GATES[g].env,
							   OPTS[i]);
	runres_free(&cons);
	return div;
}

static void reduce(const char *mcc, const char *gcc, const char *clang,
				   const char *bdir, const char *idir, const char *work,
				   const char *src, const char *outpath) {
	doc d = doc_read(src);
	if (!d.n)
		return;
	char *keep = malloc(d.n);
	memset(keep, 1, d.n);
	char cand[2048];
	snprintf(cand, sizeof cand, "%s/reduce_cand.c", work);
	int changed = 1, chunk = d.n;
	while (changed || chunk > 1) {
		changed = 0;
		for (int start = 0; start < d.n; start += chunk) {
			int any = 0;
			for (int i = start; i < start + chunk && i < d.n; i++)
				if (keep[i]) {
					keep[i] = 0;
					any = 1;
				}
			if (!any)
				continue;
			doc_write(&d, cand, keep);
			if (interesting(mcc, gcc, clang, bdir, idir, work, cand))
				changed = 1;
			else
				for (int i = start; i < start + chunk && i < d.n; i++)
					keep[i] = 1;
		}
		if (!changed && chunk > 1)
			chunk = (chunk + 1) / 2;
	}
	doc_write(&d, outpath, keep);
	for (int i = 0; i < d.n; i++)
		free(d.line[i]);
	free(d.line);
	free(keep);
}

static int ends_with(const char *s, const char *sfx) {
	size_t ls = strlen(s), lf = strlen(sfx);
	return ls >= lf && !strcmp(s + ls - lf, sfx);
}

static int replay_corpus(const char *mcc, const char *gcc, const char *clang,
						 const char *bdir, const char *idir, const char *work,
						 const char *dir) {
	char cmd[4096];
	snprintf(cmd, sizeof cmd, "ls \"%s\"/*.c 2>/dev/null", dir);
	int st;
	char *listing = cap_cmd(cmd, &st);
	int total = 0, fail = 0, skip = 0;
	for (char *ln = strtok(listing, "\n"); ln; ln = strtok(NULL, "\n")) {
		if (!*ln)
			continue;
		char base[512];
		const char *slash = strrchr(ln, '/');
		snprintf(base, sizeof base, "%s", slash ? slash + 1 : ln);
		if (!strncmp(base, "accept_", 7)) {
			printf("corpus: INFO  %-40s -- accepted [DIFF] baseline, not a bug\n", base);
			skip++;
			continue;
		}
		total++;
		runres cons;
		if (!consensus(gcc, clang, bdir, idir, work, ln, &cons)) {
			printf("corpus: SKIP  %-40s -- references disagree / cannot build\n", base);
			skip++;
			continue;
		}
		int div = 0;
		char which[8] = "";
		for (int i = 0; i < NOPTS; i++)
			if (mcc_diverges(mcc, bdir, idir, work, ln, &cons, NULL, OPTS[i])) {
				div = 1;
				snprintf(which, sizeof which, "%s", OPTS[i]);
				break;
			}
		runres_free(&cons);
		if (div) {
			printf("corpus: FAIL  %-40s -- mcc regressed at %s\n", base, which);
			fail++;
		} else {
			printf("corpus: ok    %-40s\n", base);
		}
	}
	free(listing);
	printf("corpus: %d checked, %d regressed, %d skipped/accepted\n", total, fail, skip);
	if (fail)
		return 1;
	return 0;
}

static void save_repro(const char *reduced, const char *corpus, unsigned long seed,
					   const attribution *a) {
	char dst[2048];
	snprintf(dst, sizeof dst, "%s/repro_seed%lu.c", corpus, seed);
	FILE *in = fopen(reduced, "rb");
	if (!in)
		return;
	char *body = slurp(in);
	fclose(in);
	FILE *out = fopen(dst, "wb");
	if (out) {
		fprintf(out, "/* auto-reduced miscompile repro: seed=%lu attributed=%s %s */\n",
				seed, a->found ? a->opt : "?", a->found ? a->gate : "");
		fputs(body, out);
		fclose(out);
		printf("  saved repro -> %s\n", dst);
	}
	free(body);
}

static void usage(const char *p) {
	fprintf(stderr,
			"usage: %s <mcc> <bdir> <idir> <work> <gcc> <clang> [opts]\n"
			"  --seed N        base seed (default env MCC_FUZZ_SEED or 1)\n"
			"  --count N       programs to try (default env MCC_FUZZ_COUNT or 20)\n"
			"  --corpus DIR    dir for saved repros / replay\n"
			"  --replay        regression-lock: replay every .c in --corpus\n"
			"  --gen SEED      emit one generated program to stdout and exit\n"
			"  --reduce FILE   reduce FILE to a minimal repro (needs --corpus)\n"
			"  --gates         also sweep MCC_AST_* gate flags per program\n"
			"  -v              verbose\n",
			p);
}

int main(int argc, char **argv) {
	if (argc < 7) {
		usage(argv[0]);
		return 2;
	}
	const char *mcc = argv[1], *bdir = argv[2], *idir = argv[3], *work = argv[4],
			   *gcc = argv[5], *clang = argv[6];
	unsigned long seed = strtoul(hc_envv("MCC_FUZZ_SEED", "1"), NULL, 10);
	long count = strtol(hc_envv("MCC_FUZZ_COUNT", "20"), NULL, 10);
	const char *corpus = NULL, *reduce_in = NULL;
	int do_replay = 0, do_gates = 0;
	long gen_seed = -1;
	for (int i = 7; i < argc; i++) {
		if (!strcmp(argv[i], "--seed") && i + 1 < argc)
			seed = strtoul(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--count") && i + 1 < argc)
			count = strtol(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--corpus") && i + 1 < argc)
			corpus = argv[++i];
		else if (!strcmp(argv[i], "--replay"))
			do_replay = 1;
		else if (!strcmp(argv[i], "--gates"))
			do_gates = 1;
		else if (!strcmp(argv[i], "--gen") && i + 1 < argc)
			gen_seed = strtol(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--reduce") && i + 1 < argc)
			reduce_in = argv[++i];
		else if (!strcmp(argv[i], "-v"))
			verbose = 1;
	}
	if (hc_envv("MCC_FUZZ_GATES", "")[0])
		do_gates = 1;

	if (gen_seed >= 0) {
		fuzz_emit((unsigned long)gen_seed, stdout);
		return 0;
	}
	(void)ends_with;

	hc_set_workdir(work);
	HC_MKDIR(work);

	if (do_replay) {
		if (!corpus) {
			fprintf(stderr, "--replay needs --corpus DIR\n");
			return 2;
		}
		return replay_corpus(mcc, gcc, clang, bdir, idir, work, corpus);
	}

	if (reduce_in) {
		if (!corpus) {
			fprintf(stderr, "--reduce needs --corpus DIR\n");
			return 2;
		}
		char red[2048];
		snprintf(red, sizeof red, "%s/reduced.c", work);
		reduce(mcc, gcc, clang, bdir, idir, work, reduce_in, red);
		attribution a = triage(mcc, gcc, clang, bdir, idir, work, red);
		save_repro(red, corpus, seed, &a);
		return 0;
	}

	char src[2048];
	snprintf(src, sizeof src, "%s/prog.c", work);
	int pass = 0, drop = 0, fail = 0;
	for (long i = 0; i < count; i++) {
		unsigned long s = seed + (unsigned long)i;
		FILE *f = fopen(src, "wb");
		if (!f) {
			fprintf(stderr, "cannot write %s\n", src);
			return 2;
		}
		fuzz_emit(s, f);
		fclose(f);

		runres cons;
		if (!consensus(gcc, clang, bdir, idir, work, src, &cons)) {
			drop++;
			if (verbose)
				printf("drop  seed=%lu -- references disagree / cannot build (latent UB)\n", s);
			continue;
		}
		int diverged = 0;
		char confenv[64] = "", confopt[8] = "";
		for (int oi = 0; oi < NOPTS && !diverged; oi++)
			if (mcc_diverges(mcc, bdir, idir, work, src, &cons, NULL, OPTS[oi])) {
				diverged = 1;
				snprintf(confopt, sizeof confopt, "%s", OPTS[oi]);
			}
		if (!diverged && do_gates)
			for (int g = 0; g < NGATES && !diverged; g++)
				for (int oi = 0; oi < NOPTS && !diverged; oi++)
					if (mcc_diverges(mcc, bdir, idir, work, src, &cons,
									 GATES[g].env, OPTS[oi])) {
						diverged = 1;
						snprintf(confenv, sizeof confenv, "%s", GATES[g].env);
						snprintf(confopt, sizeof confopt, "%s", OPTS[oi]);
					}
		runres_free(&cons);

		if (!diverged) {
			pass++;
			continue;
		}
		if (has_ub(gcc, work, src)) {
			drop++;
			if (verbose)
				printf("drop  seed=%lu -- divergence but program has UB\n", s);
			continue;
		}
		fail++;
		printf("FAIL  seed=%lu -- mcc diverges from gcc==clang at %s %s\n", s,
			   confopt, confenv);
		if (corpus) {
			HC_MKDIR(corpus);
			char red[2048];
			snprintf(red, sizeof red, "%s/reduced_%lu.c", work, s);
			reduce(mcc, gcc, clang, bdir, idir, work, src, red);
			attribution a = triage(mcc, gcc, clang, bdir, idir, work, red);
			printf("  attribution: %s %s\n", a.found ? a.opt : "?",
				   a.found ? a.gate : "");
			save_repro(red, corpus, s, &a);
		}
	}
	printf("fuzz: %d agree, %d miscompile, %d dropped(UB/impl-def) over seeds %lu..%lu\n",
		   pass, fail, drop, seed, seed + (unsigned long)count - 1);
	if (fail)
		return 1;
	if (pass == 0)
		return MCC_SKIP_RC;
	return 0;
}
