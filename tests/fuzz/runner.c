#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
#define MAX_REFS 16

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

typedef struct {
	const char *label[MAX_REFS], *path[MAX_REFS];
	int n;
} Refs;

static int same_cc(const char *a, const char *b) {
	char cmd[4096];
	snprintf(cmd, sizeof cmd, "\"%s\" --version 2>/dev/null", a);
	char *va = cap_cmd(cmd, NULL);
	snprintf(cmd, sizeof cmd, "\"%s\" --version 2>/dev/null", b);
	char *vb = cap_cmd(cmd, NULL);
	int same = va[0] && !strcmp(va, vb);
	free(va);
	free(vb);
	return same;
}

static void refs_dedup(Refs *r) {
	Refs o = {{0}, {0}, 0};
	for (int i = 0; i < r->n; i++) {
		int dup = 0;
		for (int j = 0; j < o.n; j++)
			if (same_cc(r->path[i], o.path[j])) {
				dup = 1;
				break;
			}
		if (!dup) {
			o.label[o.n] = r->label[i];
			o.path[o.n] = r->path[i];
			o.n++;
		}
	}
	*r = o;
}

static int refs_add(Refs *r, const char *label, const char *path) {
	if (r->n >= MAX_REFS)
		return 0;
	r->label[r->n] = label;
	r->path[r->n] = path;
	r->n++;
	return 1;
}

static int runres_eq(const runres *a, const runres *b) {
	return a->exit_code == b->exit_code && !strcmp(a->out, b->out);
}

static int runres_majority(const runres *r, const int *ok, int n, int *wc) {
	int best = -1, bestc = 0, nok = 0;
	for (int i = 0; i < n; i++)
		if (ok[i])
			nok++;
	for (int a = 0; a < n; a++) {
		if (!ok[a])
			continue;
		int c = 0;
		for (int b = 0; b < n; b++)
			if (ok[b] && runres_eq(&r[a], &r[b]))
				c++;
		if (c > bestc) {
			bestc = c;
			best = a;
		}
	}
	if (wc)
		*wc = bestc;
	return (nok >= 2 && bestc * 2 > nok) ? best : -1;
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
	{"COLOR", "MCC_AST_COLOR=1"},
	{"INLINE", "MCC_AST_INLINE=1"},
	{"LICM_TEMP", "MCC_AST_LICM_TEMP=1"},
	{"IVSR", "MCC_AST_IVSR=1"},
	{"PRE", "MCC_AST_PRE=1"},
	/* Opt-in -O4 search knobs (normally reached only via the gate search). Forcing
	 * them on directly lets the differential fuzzer stress their codegen against
	 * gcc/clang across random programs — V-narrow(a) narrow-to-fixpoint and
	 * V-sethi(a) leaf-aware Sethi-Ullman. */
	{"NARROW_FIX", "MCC_AST_NARROW_FIX=1"},
	{"SETHI_LEAF", "MCC_AST_SETHI_LEAF=1"},
	{"SCCP_FIX", "MCC_AST_SCCP_FIX=1"},
	{"DSE_CALL", "MCC_AST_DSE_CALL=1"},
	{"TCO_PTR", "MCC_AST_TCO_PTR=1"},
	{"CSE_COMM", "MCC_AST_CSE_COMM=1"},
	{"RANGE", "MCC_AST_RANGE=1"},
	{"DIVMAGIC", "MCC_AST_DIVMAGIC=1"},
	{"ABS", "MCC_AST_ABS=1"},
	{"REASSOC", "MCC_AST_REASSOC=1"},
	{"INTERCHANGE", "MCC_AST_INTERCHANGE=1"},
	{"CSE_WINDOW", "MCC_AST_CSE_WINDOW=256"},
	{"CPROP_WINDOW", "MCC_AST_CPROP_WINDOW=512"},
	{"INLINE_DEEP", "MCC_AST_INLINE=1 MCC_AST_INLINE_DEPTH=16"},
	{"TCO_MAXP", "MCC_AST_TCO_MAXP=32"},
};
#define NGATES ((int)(sizeof GATES / sizeof *GATES))

static int consensus(const Refs *refs, const char *bdir, const char *idir,
					 const char *work, const char *src, runres *cons) {
	runres r[MAX_REFS];
	int ok[MAX_REFS];
	for (int i = 0; i < refs->n; i++) {
		char lbl[64];
		snprintf(lbl, sizeof lbl, "ref_%s", refs->label[i]);
		r[i] = build_run(refs->path[i], NULL, bdir, idir, NULL, "-O2", work, lbl, src);
		ok[i] = r[i].kind == RES_OK;
	}
	int wc = 0, win = runres_majority(r, ok, refs->n, &wc);
	int ret = 0;
	if (win >= 0) {
		*cons = r[win];
		r[win].out = NULL;
		ret = 1;
	}
	for (int i = 0; i < refs->n; i++)
		runres_free(&r[i]);
	return ret;
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

static attribution triage(const char *mcc, const Refs *refs,
						  const char *bdir, const char *idir, const char *work,
						  const char *src) {
	attribution a;
	a.found = 0;
	a.opt[0] = a.gate[0] = 0;
	runres cons;
	if (!consensus(refs, bdir, idir, work, src, &cons))
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

static int interesting(const char *mcc, const Refs *refs,
					   const char *bdir, const char *idir, const char *work,
					   const char *cand) {
	runres cons;
	if (!consensus(refs, bdir, idir, work, cand, &cons))
		return 0;
	if (refs->n && has_ub(refs->path[0], work, cand)) {
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

static void reduce(const char *mcc, const Refs *refs,
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
			if (interesting(mcc, refs, bdir, idir, work, cand))
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

static int replay_corpus(const char *mcc, const Refs *refs,
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
		if (!consensus(refs, bdir, idir, work, ln, &cons)) {
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

static char *popen_line(const char *cmd) {
	FILE *f = HC_POPEN_SH(cmd);
	char *o, *nl;
	if (!f)
		return NULL;
	o = slurp(f);
	pclose(f);
	nl = strchr(o, '\n');
	if (nl)
		*nl = 0;
	return o;
}

typedef struct {
	char *key;
	long hits;
	unsigned long first_seed;
	long first_round;
	long last_epoch;
} ScoreClass;

static const char *scoreboard_path(const char *corpus, char *buf, size_t n) {
	const char *env = hc_envv("MCC_FUZZ_SCOREBOARD", "");
	if (env[0])
		return env;
	snprintf(buf, n, "%s/scoreboard.tsv", corpus);
	return buf;
}

static ScoreClass *score_find(ScoreClass *sb, int n, const char *key) {
	int i;
	for (i = 0; i < n; i++)
		if (!strcmp(sb[i].key, key))
			return &sb[i];
	return NULL;
}

static int score_load(const char *path, ScoreClass **out) {
	FILE *f = fopen(path, "rb");
	ScoreClass *sb = NULL;
	char line[8192];
	int n = 0;
	*out = NULL;
	if (!f)
		return 0;
	while (fgets(line, sizeof line, f)) {
		char *nl = strchr(line, '\n'), *tab;
		long hits = 0, fr = 0, le = 0;
		unsigned long fs = 0;
		if (nl)
			*nl = 0;
		if (line[0] == '#' || line[0] == 0)
			continue;
		tab = strchr(line, '\t');
		if (!tab)
			continue;
		*tab++ = 0;
		sscanf(tab, "%ld\t%lu\t%ld\t%ld", &hits, &fs, &fr, &le);
		sb = realloc(sb, sizeof *sb * (n + 1));
		sb[n].key = strdup(line);
		sb[n].hits = hits;
		sb[n].first_seed = fs;
		sb[n].first_round = fr;
		sb[n].last_epoch = le;
		n++;
	}
	fclose(f);
	*out = sb;
	return n;
}

static void score_save(const char *path, ScoreClass *sb, int n) {
	char tmp[4096];
	FILE *f;
	int i;
	snprintf(tmp, sizeof tmp, "%s.tmp", path);
	f = fopen(tmp, "wb");
	if (!f)
		return;
	fprintf(f, "# mcc fuzz miscompile-class scoreboard\n");
	fprintf(f, "# class\thits\tfirst_seed\tfirst_round\tlast_epoch\n");
	for (i = 0; i < n; i++)
		fprintf(f, "%s\t%ld\t%lu\t%ld\t%ld\n", sb[i].key, sb[i].hits,
				sb[i].first_seed, sb[i].first_round, sb[i].last_epoch);
	fclose(f);
	remove(path);
	rename(tmp, path);
}

/* Nightly differential-fuzz campaign: loop the batch runner over fresh seed
   batches until the wall-clock budget is spent or K consecutive batches surface
   no new miscompile *class* (attribution = the -O level + MCC_AST_* gate the
   runner blames). Found repros are reduced+saved into the corpus by the batch
   runner; this driver dedups their attribution classes and enforces the stop
   rule -- exiting nonzero exactly when a new class was found. */
static int campaign_main(const char *self, int argc, char **argv) {
	Refs refs = {{0}, {0}, 0};
	const char *pos[16];
	int npos = 0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--ref") && i + 2 < argc) {
			refs_add(&refs, argv[i + 1], argv[i + 2]);
			i += 2;
		} else if (npos < 16) {
			pos[npos++] = argv[i];
		}
	}
	refs_dedup(&refs);
	if (npos < 5 || refs.n < 2) {
		fprintf(stderr,
				"usage: %s campaign <mcc> <bdir> <idir> <corpus> <work>"
				" [budget_secs] [batch] [stop_k] --ref <label> <path> [--ref ...]\n",
				self);
		return 2;
	}
	{
		const char *mcc = pos[0], *bdir = pos[1], *idir = pos[2], *corpus = pos[3],
				   *work = pos[4];
		long budget = npos > 5 ? strtol(pos[5], NULL, 10) : 3600;
		long batch = npos > 6 ? strtol(pos[6], NULL, 10) : 50;
		long stop_k = npos > 7 ? strtol(pos[7], NULL, 10) : 20;
		char refargs[4096] = "";
		for (int i = 0; i < refs.n; i++) {
			char one[1024];
			snprintf(one, sizeof one, " --ref \"%s\" \"%s\"", refs.label[i], refs.path[i]);
			strncat(refargs, one, sizeof refargs - strlen(refargs) - 1);
		}
		char sbbuf[4096];
		const char *sbpath;
		ScoreClass *sb = NULL;
		int nsb, known_at_start;
		time_t start = time(NULL);
		unsigned long seed = strtoul(hc_envv("MCC_FUZZ_SEED", "1000"), NULL, 10);
		long empty_streak = 0, total_fail = 0, round = 0, run_new = 0;

		HC_MKDIR(corpus);
		HC_MKDIR(work);
		sbpath = scoreboard_path(corpus, sbbuf, sizeof sbbuf);
		nsb = score_load(sbpath, &sb);
		known_at_start = nsb;
		printf("campaign: scoreboard %s (%d known class%s)\n", sbpath, nsb,
			   nsb == 1 ? "" : "es");

		for (;;) {
			long elapsed = (long)(time(NULL) - start), nnew = 0;
			char logp[2048], rdir[2048], cmd[8192];
			int rc, exitc;
			FILE *lf;

			if (elapsed >= budget) {
				printf("campaign: budget %lds reached\n", budget);
				break;
			}
			if (empty_streak >= stop_k) {
				printf("campaign: %ld consecutive batches with no new class -> converged\n",
					   stop_k);
				break;
			}
			round++;
			snprintf(rdir, sizeof rdir, "%s/r%ld", work, round);
			snprintf(logp, sizeof logp, "%s/round-%ld.log", work, round);
			snprintf(cmd, sizeof cmd,
					 "\"%s\" \"%s\" \"%s\" \"%s\" \"%s\" --seed %lu "
					 "--count %ld --gates --corpus \"%s\"%s >\"%s\" 2>&1",
					 self, mcc, bdir, idir, rdir, seed, batch, corpus, refargs, logp);
			rc = HC_SYSTEM_SH(cmd);
			exitc = WIFEXITED(rc) ? WEXITSTATUS(rc) : rc;

			lf = fopen(logp, "rb");
			if (lf) {
				char line[8192];
				while (fgets(line, sizeof line, lf)) {
					char *nl = strchr(line, '\n');
					const char *key;
					ScoreClass *sc;
					if (nl)
						*nl = 0;
					if (strncmp(line, "  attribution: ", 15))
						continue;
					key = line + 15;
					total_fail++;
					sc = score_find(sb, nsb, key);
					if (!sc) {
						sb = realloc(sb, sizeof *sb * (nsb + 1));
						sc = &sb[nsb++];
						sc->key = strdup(key);
						sc->hits = 0;
						sc->first_seed = seed;
						sc->first_round = round;
						nnew++;
						run_new++;
						printf("campaign: NEW miscompile class [%s] at round %ld (seed base %lu)\n",
							   key, round, seed);
					}
					sc->hits++;
					sc->last_epoch = (long)time(NULL);
				}
				fclose(lf);
			}
			empty_streak = nnew > 0 ? 0 : empty_streak + 1;
			score_save(sbpath, sb, nsb);
			printf("campaign: round %ld seeds %lu..%lu rc=%d new=%ld streak=%ld elapsed=%lds\n",
				   round, seed, seed + (unsigned long)batch - 1, exitc, nnew, empty_streak,
				   elapsed);
			seed += (unsigned long)batch;
		}
		score_save(sbpath, sb, nsb);
		printf("campaign: done rounds=%ld miscompiles=%ld new-classes=%ld total-classes=%d (was %d)\n",
			   round, total_fail, run_new, nsb, known_at_start);
		{
			int i, ret = run_new > 0 ? 1 : 0;
			for (i = 0; i < nsb; i++)
				free(sb[i].key);
			free(sb);
			return ret;
		}
	}
}

/* git-bisect predicate for a saved repro: rebuild the preset at the current
   commit, then replay the repro through the freshly built runner. Prints the
   bisect protocol codes (0=good, 1=bad, 125=skip). */
static int bisect_main(int argc, char **argv) {
	const char *repro = argc > 1 ? argv[1] : NULL;
	const char *preset = argc > 2 ? argv[2] : "debug";
	char *root, *gcc, *clang, *work;
	char cmd[8192], bdir[4096], one[4096];
	int rc, exitc;
	FILE *tf;

	if (!repro || !(tf = fopen(repro, "rb"))) {
		fprintf(stderr, "git-bisect predicate for a tests/fuzz repro (0=good,1=bad,125=skip)\n");
		fprintf(stderr, "usage: git bisect run <fuzz_runner> bisect <repro.c> [preset]\n");
		return 125;
	}
	fclose(tf);

	root = popen_line("git rev-parse --show-toplevel");
	if (!root || !*root) {
		free(root);
		return 125;
	}
	snprintf(cmd, sizeof cmd, "cmake --build --preset \"%s\" -j >/dev/null 2>&1", preset);
	if (HC_SYSTEM_SH(cmd) != 0) {
		free(root);
		return 125;
	}
	gcc = popen_line("command -v gcc");
	clang = popen_line("command -v clang");
	work = popen_line("mktemp -d");
	if (!gcc || !*gcc || !clang || !*clang || !work || !*work) {
		free(root);
		free(gcc);
		free(clang);
		free(work);
		return 125;
	}
	snprintf(one, sizeof one, "%s/corpus", work);
	HC_MKDIR(one);
	snprintf(cmd, sizeof cmd, "cp \"%s\" \"%s\"/", repro, one);
	rc = HC_SYSTEM_SH(cmd);
	snprintf(bdir, sizeof bdir, "%s/cmake-%s", root, preset);
	snprintf(cmd, sizeof cmd,
			 "\"%s/fuzz_runner\" \"%s/mcc\" \"%s\" \"%s/runtime/include\" \"%s\" "
			 "--corpus \"%s\" --replay --ref gcc \"%s\" --ref clang \"%s\"",
			 bdir, bdir, bdir, root, work, one, gcc, clang);
	rc = HC_SYSTEM_SH(cmd);
	exitc = WIFEXITED(rc) ? WEXITSTATUS(rc) : rc;
	snprintf(cmd, sizeof cmd, "rm -rf \"%s\"", work);
	if (HC_SYSTEM_SH(cmd)) {
		/* best-effort cleanup */
	}
	free(root);
	free(gcc);
	free(clang);
	free(work);
	return exitc;
}

static void usage(const char *p) {
	fprintf(stderr,
			"usage: %s <mcc> <bdir> <idir> <work> --ref <label> <path> [--ref ...] [opts]\n"
			"       %s campaign <mcc> <bdir> <idir> <corpus> <work> [budget] [batch] [stop_k] --ref <label> <path> [--ref ...]\n"
			"       %s bisect <repro.c> [preset]\n"
			"  --ref LABEL P   a reference compiler (>=2 distinct needed for the majority consensus)\n"
			"  --seed N        base seed (default env MCC_FUZZ_SEED or 1)\n"
			"  --count N       programs to try (default env MCC_FUZZ_COUNT or 20)\n"
			"  --corpus DIR    dir for saved repros / replay\n"
			"  --replay        regression-lock: replay every .c in --corpus\n"
			"  --gen SEED      emit one generated program to stdout and exit\n"
			"  --reduce FILE   reduce FILE to a minimal repro (needs --corpus)\n"
			"  --gates         also sweep MCC_AST_* gate flags per program\n"
			"  -v              verbose\n",
			p, p, p);
}

int main(int argc, char **argv) {
	if (argc >= 2 && !strcmp(argv[1], "campaign"))
		return campaign_main(argv[0], argc - 1, argv + 1);
	if (argc >= 2 && !strcmp(argv[1], "bisect"))
		return bisect_main(argc - 1, argv + 1);
	if (argc < 5) {
		usage(argv[0]);
		return 2;
	}
	const char *mcc = argv[1], *bdir = argv[2], *idir = argv[3], *work = argv[4];
	unsigned long seed = strtoul(hc_envv("MCC_FUZZ_SEED", "1"), NULL, 10);
	long count = strtol(hc_envv("MCC_FUZZ_COUNT", "20"), NULL, 10);
	const char *corpus = NULL, *reduce_in = NULL;
	int do_replay = 0, do_gates = 0;
	long gen_seed = -1;
	Refs refs = {{0}, {0}, 0};
	for (int i = 5; i < argc; i++) {
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
		else if (!strcmp(argv[i], "--ref") && i + 2 < argc) {
			refs_add(&refs, argv[i + 1], argv[i + 2]);
			i += 2;
		} else if (!strcmp(argv[i], "-v"))
			verbose = 1;
	}
	if (hc_envv("MCC_FUZZ_GATES", "")[0])
		do_gates = 1;
	refs_dedup(&refs);

	if (gen_seed >= 0) {
		fuzz_emit((unsigned long)gen_seed, stdout);
		return 0;
	}
	(void)ends_with;

	hc_set_workdir(work);
	HC_MKDIR(work);

	if (refs.n < 2) {
		fprintf(stderr,
				"fuzz: need >=2 distinct reference compilers (--ref <label> <path>); have %d\n",
				refs.n);
		return MCC_SKIP_RC;
	}

	if (do_replay) {
		if (!corpus) {
			fprintf(stderr, "--replay needs --corpus DIR\n");
			return 2;
		}
		return replay_corpus(mcc, &refs, bdir, idir, work, corpus);
	}

	if (reduce_in) {
		if (!corpus) {
			fprintf(stderr, "--reduce needs --corpus DIR\n");
			return 2;
		}
		char red[2048];
		snprintf(red, sizeof red, "%s/reduced.c", work);
		reduce(mcc, &refs, bdir, idir, work, reduce_in, red);
		attribution a = triage(mcc, &refs, bdir, idir, work, red);
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
		if (!consensus(&refs, bdir, idir, work, src, &cons)) {
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
		if (has_ub(refs.path[0], work, src)) {
			drop++;
			if (verbose)
				printf("drop  seed=%lu -- divergence but program has UB\n", s);
			continue;
		}
		fail++;
		printf("FAIL  seed=%lu -- mcc diverges from the %d-ref consensus at %s %s\n", s,
			   refs.n, confopt, confenv);
		if (corpus) {
			HC_MKDIR(corpus);
			char red[2048];
			snprintf(red, sizeof red, "%s/reduced_%lu.c", work, s);
			reduce(mcc, &refs, bdir, idir, work, src, red);
			attribution a = triage(mcc, &refs, bdir, idir, work, red);
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
