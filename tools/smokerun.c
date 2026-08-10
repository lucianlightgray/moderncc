#include "toolsupport.h"
#include "../src/mccopt.h"

#include <time.h>

#ifndef _WIN32
#include <sys/wait.h>
#include <signal.h>
#endif

#define SMK_MAXLEVEL MCC_OPT_SEARCH_LEVEL
#define SMK_MAXCAT 4096
#define SMK_MAXBANK 4096

static const char *g_mcc;
static const char *g_srcdir;
static const char *g_work;
static int g_verbose;
static unsigned g_budget_ms = 1000;
static unsigned g_deadline_ms = 6000;
static long g_min_cases;
static int g_poison;
static int g_rebank;
static int g_rebank_req;
static int g_require_device;
static int g_skip;
static const char *g_extra_flags = "";

static long g_checks_total;
static long g_cases_total;
static int g_fail;
static int g_levels_run;

typedef struct
{
	char key[192];
	long n;
} Cat;

static Cat g_cat[SMK_MAXCAT];
static int g_ncat;
static int g_cat_full;

#define SMK_MAXROW 512

typedef struct
{
	char name[128];
	unsigned mask;
} RowLev;

static RowLev g_rowlev[SMK_MAXROW];
static int g_nrowlev;

static Cat g_bank[SMK_MAXBANK];
static int g_nbank;
static int g_have_bank;

static void note(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	fflush(stdout);
}

static void bad(const char *fmt, ...)
{
	va_list ap;
	g_fail++;
	fputs("FAIL ", stdout);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	fputc('\n', stdout);
	fflush(stdout);
}

static void cat_add(const char *fmt, ...)
{
	char k[192];
	va_list ap;
	int i;
	va_start(ap, fmt);
	vsnprintf(k, sizeof k, fmt, ap);
	va_end(ap);
	for (i = 0; i < g_ncat; i++)
		if (!strcmp(g_cat[i].key, k)) {
			g_cat[i].n++;
			return;
		}
	if (g_ncat >= SMK_MAXCAT) {
		g_cat_full = 1;
		return;
	}
	snprintf(g_cat[g_ncat].key, sizeof g_cat[g_ncat].key, "%s", k);
	g_cat[g_ncat].n = 1;
	g_ncat++;
}

static void cat_addn(long n, const char *fmt, ...)
{
	char k[192];
	va_list ap;
	int i;
	if (n <= 0)
		return;
	va_start(ap, fmt);
	vsnprintf(k, sizeof k, fmt, ap);
	va_end(ap);
	for (i = 0; i < g_ncat; i++)
		if (!strcmp(g_cat[i].key, k)) {
			g_cat[i].n += n;
			return;
		}
	if (g_ncat >= SMK_MAXCAT) {
		g_cat_full = 1;
		return;
	}
	snprintf(g_cat[g_ncat].key, sizeof g_cat[g_ncat].key, "%s", k);
	g_cat[g_ncat].n = n;
	g_ncat++;
}

static void rowlev_note(const char *name, int level)
{
	int i;
	for (i = 0; i < g_nrowlev; i++)
		if (!strcmp(g_rowlev[i].name, name)) {
			g_rowlev[i].mask |= 1u << level;
			return;
		}
	if (g_nrowlev >= SMK_MAXROW)
		return;
	snprintf(g_rowlev[g_nrowlev].name, sizeof g_rowlev[g_nrowlev].name, "%s",
					 name);
	g_rowlev[g_nrowlev].mask = 1u << level;
	g_nrowlev++;
}

static void scan_failrows(const char *text, int level)
{
	const char *p = text;
	while ((p = strstr(p, "FAIL ")) != NULL) {
		char name[128];
		const char *q = p + 5;
		const char *eol = strchr(p, '\n');
		size_t n;
		q = strchr(q, ' ');
		if (!q || (eol && q > eol)) {
			p = eol ? eol + 1 : p + 5;
			continue;
		}
		q++;
		n = strcspn(q, " \n");
		if (n && n < sizeof name) {
			memcpy(name, q, n);
			name[n] = 0;
			rowlev_note(name, level);
		}
		p = eol ? eol + 1 : q + n;
	}
}

static void rowlev_report(int maxlevel)
{
	int i;
	unsigned all = maxlevel >= 31 ? 0xffffffffu : ((1u << (maxlevel + 1)) - 1u);
	for (i = 0; i < g_nrowlev; i++) {
		int lo = -1, hi = -1, k, gaps = 0, prev = -2;
		for (k = 0; k <= maxlevel; k++)
			if (g_rowlev[i].mask & (1u << k)) {
				if (lo < 0)
					lo = k;
				hi = k;
				if (prev >= 0 && k != prev + 1)
					gaps = 1;
				prev = k;
			}
		if (lo < 0)
			continue;
		note("LEVELRANGE %-32s O%d..O%d%s%s\n", g_rowlev[i].name, lo, hi,
				 gaps ? " (with gaps)" : "",
				 g_rowlev[i].mask == all ? " every-level" : "");
	}
}

static int cat_cmp(const void *a, const void *b)
{
	return strcmp(((const Cat *)a)->key, ((const Cat *)b)->key);
}

static unsigned now_ms(void)
{
	return host_clock_ms();
}

static int sm_system(const char *cmd)
{
	return system(cmd);
}

#define SMK_TRAP_EXIT 97

static int died_by_fpe(int st)
{
	if (st == -1)
		return 0;
#ifndef _WIN32
	if (WIFSIGNALED(st))
		return WTERMSIG(st) == SIGFPE;
	if (WIFEXITED(st))
		return WEXITSTATUS(st) == SMK_TRAP_EXIT ||
					 WEXITSTATUS(st) == 128 + SIGFPE;
	return 0;
#else
	return st == SMK_TRAP_EXIT || (st != 0 && st != 1 && st != 2);
#endif
}

static int exited_zero(int st)
{
#ifndef _WIN32
	return st != -1 && WIFEXITED(st) && WEXITSTATUS(st) == 0;
#else
	return st == 0;
#endif
}

static int exit_code(int st)
{
#ifndef _WIN32
	if (st != -1 && WIFEXITED(st))
		return WEXITSTATUS(st);
	return -1;
#else
	return st;
#endif
}

static char *slurp(const char *path)
{
	long n = 0;
	char *s = ts_read_file(path, &n);
	if (!s) {
		s = malloc(1);
		if (s)
			s[0] = 0;
	}
	return s;
}

static void set_census_env(int level, const char *tsv)
{
	char b[64];
	host_setenv("MCC_RIR_PROD", "2");
	host_setenv("MCC_RIR_PROD_OUT", tsv);
	host_setenv("MCC_JIT_VERBOSE", "1");
	host_setenv("MCC_RIR_ABORTWHY", "1");
	host_setenv("MCC_FORCE_REPLAY", "1");
	host_unsetenv("MCC_REPLAY_IR");
	snprintf(b, sizeof b, "%u", g_budget_ms);
	host_setenv("MCC_SEARCH_BUDGET_MS", b);
	(void)level;
}

static void clear_census_env(void)
{
	host_unsetenv("MCC_RIR_PROD");
	host_unsetenv("MCC_RIR_PROD_OUT");
	host_unsetenv("MCC_JIT_VERBOSE");
	host_unsetenv("MCC_RIR_ABORTWHY");
	host_unsetenv("MCC_AST_EVAL_LADDER");
	host_unsetenv("MCC_AST_EVAL_LADDER_CENSUS");
	host_unsetenv("MCC_AST_EVAL_LADDER_GPU");
	host_unsetenv("MCC_FORCE_REPLAY");
}

static const char *const kLadderReasons[] = {
		"no-static-type", "unsupported-node", "unsupported-op",
		"too-many-live-ins", "no-observed", "over-budget", "all-undefined", NULL};

static void scan_ladder(const char *text, int level)
{
	const char *p = text;
	while ((p = strstr(p, "] refuse ")) != NULL) {
		int i;
		const char *line = p;
		const char *eol = strchr(line, '\n');
		size_t len = eol ? (size_t)(eol - line) : strlen(line);
		for (i = 0; kLadderReasons[i]; i++) {
			char pat[64];
			const char *q;
			size_t off;
			snprintf(pat, sizeof pat, "%s=", kLadderReasons[i]);
			q = strstr(line, pat);
			if (!q)
				continue;
			off = (size_t)(q - line);
			if (off >= len)
				continue;
			cat_addn(strtol(q + strlen(pat), NULL, 10), "O%d slice-refused:%s", level,
							 kLadderReasons[i]);
		}
		p = eol ? eol : line + len;
	}
}

static int memmem_len(const char *hay, size_t n, const char *needle)
{
	size_t m = strlen(needle);
	size_t i;
	if (m > n)
		return 0;
	for (i = 0; i + m <= n; i++)
		if (!memcmp(hay + i, needle, m))
			return 1;
	return 0;
}

static void scan_jit(const char *text, int level)
{
	const char *p = text;
	while ((p = strstr(p, "refuse-to-JIT ")) != NULL) {
		const char *eol = strchr(p, '\n');
		size_t len = eol ? (size_t)(eol - p) : strlen(p);
		const char *why = "other";
		if (memmem_len(p, len, "VLA"))
			why = "vla";
		else if (memmem_len(p, len, "GP-int set"))
			why = "signature";
		cat_add("O%d jit-not-baked:%s", level, why);
		p = eol ? eol : p + len;
	}
}

static void scan_rir(const char *text, int level)
{
	const char *p = text;
	while (p && *p) {
		const char *eol = strchr(p, '\n');
		size_t len = eol ? (size_t)(eol - p) : strlen(p);
		char line[1024];
		char *f[8];
		int nf = 0;
		char *s;
		if (len && len < sizeof line && p[0] != '[') {
			memcpy(line, p, len);
			line[len] = 0;
			s = line;
			while (nf < 8) {
				f[nf++] = s;
				s = strchr(s, '\t');
				if (!s)
					break;
				*s++ = 0;
			}
			if (nf >= 5) {
				if (!strcmp(f[0], "fallback"))
					cat_add("O%d replay-fallback:%s", level, f[4][0] ? f[4] : "?");
				else if (!strcmp(f[0], "nomodel"))
					cat_add("O%d replay-nomodel:%s", level, f[3][0] ? f[3] : "?");
			}
		}
		if (!eol)
			break;
		p = eol + 1;
	}
}

static int parse_summary(const char *out, long *checks, long *sweep,
												 long *msweep, long *fchecks, long *failures,
												 char *digest, size_t dn)
{
	const char *p = strstr(out, "smoke: checks=");
	if (!p)
		return 0;
	return sscanf(p,
								"smoke: checks=%ld sweep=%ld msweep=%ld fchecks=%ld "
								"failures=%ld digest=%31s",
								checks, sweep, msweep, fchecks, failures, digest) == 6 &&
				 dn > 16;
}

static int compile_subject(int level, const char *exe, const char *log,
													 const char *tsv, unsigned *ms, int gpu, int ladder,
													 int jit)
{
	char cmd[4096];
	unsigned t0, t1;
	int st;
	set_census_env(level, tsv);
	if (ladder) {
		host_setenv("MCC_AST_EVAL_LADDER", "1");
		host_setenv("MCC_AST_EVAL_LADDER_CENSUS", "1");
	}
	if (gpu) {
		host_setenv("MCC_AST_EVAL_LADDER", "1");
		host_setenv("MCC_AST_EVAL_LADDER_CENSUS", "1");
		host_setenv("MCC_AST_EVAL_LADDER_GPU", "1");
	}
	remove(tsv);
	snprintf(cmd, sizeof cmd,
					 "\"%s\" -w -O%d %s %s -fno-diagnostics-show-caret "
					 "\"-I%s/tests/smoke\" \"%s/tests/smoke/subject.c\" -o \"%s\" "
					 "> \"%s\" 2>&1",
					 g_mcc, level, jit ? "--embed-jit" : "", g_extra_flags, g_srcdir,
					 g_srcdir, exe, log);
	t0 = now_ms();
	st = sm_system(cmd);
	t1 = now_ms();
	*ms = t1 - t0;
	clear_census_env();
	return exited_zero(st);
}

static int run_subject(const char *exe, const char *args, const char *out)
{
	char cmd[4096];
	snprintf(cmd, sizeof cmd, "\"%s\" %s > \"%s\" 2>&1", exe, args, out);
	return sm_system(cmd);
}

static int run_subject_nocore(const char *exe, const char *args,
															const char *out)
{
	char cmd[4096];
#ifndef _WIN32
	snprintf(cmd, sizeof cmd,
					 "ulimit -c 0 2>/dev/null; \"%s\" %s > \"%s\" 2>&1", exe, args, out);
#else
	snprintf(cmd, sizeof cmd, "\"%s\" %s > \"%s\" 2>&1", exe, args, out);
#endif
	return sm_system(cmd);
}

static void bank_load(const char *path)
{
	char *txt = ts_read_file(path, NULL);
	char *p;
	if (!txt)
		return;
	g_have_bank = 1;
	p = txt;
	while (*p) {
		char *eol = strchr(p, '\n');
		if (eol)
			*eol = 0;
		if (p[0] && p[0] != '#') {
			char *tab = strrchr(p, ' ');
			if (tab && g_nbank < SMK_MAXBANK) {
				*tab = 0;
				snprintf(g_bank[g_nbank].key, sizeof g_bank[g_nbank].key, "%s", p);
				g_bank[g_nbank].n = strtol(tab + 1, NULL, 10);
				g_nbank++;
			}
		}
		if (!eol)
			break;
		p = eol + 1;
	}
	free(txt);
}

static long bank_get(const char *key, int *found)
{
	int i;
	for (i = 0; i < g_nbank; i++)
		if (!strcmp(g_bank[i].key, key)) {
			*found = 1;
			return g_bank[i].n;
		}
	*found = 0;
	return 0;
}

static const char *g_scope = "";
static const char *g_owned[8];
static int g_nowned;

static int scope_match(const char *key, const char *scope)
{
	const char *sp = strchr(key, ' ');
	if (!sp)
		return 0;
	return strncmp(sp + 1, scope, strlen(scope)) == 0;
}

static int in_scope(const char *key)
{
	return scope_match(key, g_scope);
}

static int owned(const char *key)
{
	int i;
	for (i = 0; i < g_nowned; i++)
		if (scope_match(key, g_owned[i]))
			return 1;
	return 0;
}

static void own(const char *scope)
{
	if (g_nowned < 8)
		g_owned[g_nowned++] = scope;
	g_scope = scope;
}

static void bank_write(const char *path)
{
	FILE *f;
	int i, n = 0;
	for (i = 0; i < g_nbank; i++)
		if (!owned(g_bank[i].key) && g_ncat < SMK_MAXCAT) {
			snprintf(g_cat[g_ncat].key, sizeof g_cat[g_ncat].key, "%s",
							 g_bank[i].key);
			g_cat[g_ncat].n = g_bank[i].n;
			g_ncat++;
		}
	qsort(g_cat, (size_t)g_ncat, sizeof g_cat[0], cat_cmp);
	f = fopen(path, "w");
	if (!f) {
		bad("cannot write bank %s", path);
		return;
	}
	fprintf(f, "# regenerate: ctest -R smoke/rebank\n");
	for (i = 0; i < g_ncat; i++, n++)
		fprintf(f, "%s %ld\n", g_cat[i].key, g_cat[i].n);
	fclose(f);
	note("smokerun: rebanked %d categories into %s\n", n, path);
}

static void ratchet(const char *path)
{
	int i, worse = 0, better = 0, newcat = 0, nown = 0;
	qsort(g_cat, (size_t)g_ncat, sizeof g_cat[0], cat_cmp);
	if (g_rebank) {
		bank_write(path);
		return;
	}
	if (!g_have_bank) {
		if (g_rebank_req)
			return;
		bad("no bail bank at %s; a census with nothing to compare against ratchets "
				"nothing -- run with --rebank once and commit the file",
				path);
		return;
	}
	for (i = 0; i < g_ncat; i++) {
		int found = 0;
		long was;
		if (!in_scope(g_cat[i].key))
			continue;
		nown++;
		was = bank_get(g_cat[i].key, &found);
		if (!found) {
			newcat++;
			bad("new bail category %s = %ld; it was not in the bank, so it is a "
					"regression until banked deliberately",
					g_cat[i].key, g_cat[i].n);
		} else if (g_cat[i].n > was) {
			worse++;
			bad("bail ratchet %s rose %ld -> %ld", g_cat[i].key, was, g_cat[i].n);
		} else if (g_cat[i].n < was) {
			better++;
			note("IMPROVED %s fell %ld -> %ld\n", g_cat[i].key, was, g_cat[i].n);
		}
	}
	for (i = 0; i < g_nbank; i++) {
		int j, seen = 0;
		if (!in_scope(g_bank[i].key))
			continue;
		for (j = 0; j < g_ncat; j++)
			if (!strcmp(g_cat[j].key, g_bank[i].key))
				seen = 1;
		if (!seen && g_bank[i].n > 0) {
			better++;
			note("IMPROVED %s fell %ld -> 0\n", g_bank[i].key, g_bank[i].n);
		}
	}
	note("smokerun: bail census scope='%s' %d categories (%d worse, %d better, "
			 "%d new)\n",
			 g_scope, nown, worse, better, newcat);
	if (g_cat_full)
		bad("bail census overflowed %d categories, so the census silently stopped "
				"counting; raise SMK_MAXCAT",
				SMK_MAXCAT);
	if (nown == 0)
		bad("bail census scope '%s' produced no categories at all, so the ratchet "
				"measured nothing",
				g_scope);
	if (better && !worse && !newcat)
		note("smokerun: re-bank with: smokerun --rebank (then commit %s)\n", path);
}

static void run_traps(const char *exe, int level)
{
	char out[1024], cmd[256], names[8192];
	int n = 0, i;
	char *txt, *p;
	snprintf(out, sizeof out, "%s/traps.txt", g_work);
	run_subject(exe, "--trapnames", out);
	txt = slurp(out);
	snprintf(names, sizeof names, "%s", txt);
	free(txt);
	for (p = names; *p; p++)
		if (*p == '\n')
			n++;
	if (n <= 0) {
		bad("O%d subject listed %d trap rows; the idiv fault case would not be "
				"exercised at all",
				level, n);
		return;
	}
	p = names;
	for (i = 0; i < n; i++) {
		int st;
		char *eol = strchr(p, '\n');
		if (eol)
			*eol = 0;
		snprintf(cmd, sizeof cmd, "--trap %d", i);
		st = run_subject_nocore(exe, cmd, out);
		g_checks_total++;
		g_cases_total++;
		if (!died_by_fpe(st)) {
			char *o = slurp(out);
			bad("O%d trap row %s did not fault (status=%d): %s", level, p,
					exit_code(st), o);
			free(o);
		}
		if (!eol)
			break;
		p = eol + 1;
	}
	if (g_verbose)
		note("  O%-2d traps=%d all faulted\n", level, n);
}

static int level_pass(int level, const char *want_digest, char *got_digest,
											size_t gdn)
{
	char exe[1024], log[1024], tsv[1024], out[1024], args[64];
	unsigned ms = 0;
	long checks = 0, sweep = 0, msweep = 0, fchecks = 0, failures = 0;
	char *ltxt, *otxt;
	int st, ok = 1;

	ts_path(exe, sizeof exe, g_work, "subject-O%d.exe", level);
	ts_path(log, sizeof log, g_work, "compile-O%d.log", level);
	ts_path(tsv, sizeof tsv, g_work, "rir-O%d.tsv", level);
	ts_path(out, sizeof out, g_work, "run-O%d.txt", level);

	if (!compile_subject(level, exe, log, tsv, &ms, 0, 0, 0)) {
		char *l = slurp(log);
		bad("O%d compile failed:\n%s", level, l);
		free(l);
		return 0;
	}
	if (ms > g_deadline_ms) {
		bad("O%d compile took %u ms, past the %u ms deadline (search budget %u ms); "
				"a search that blows its cap is a failure, not a truncation",
				level, ms, g_deadline_ms, g_budget_ms);
		ok = 0;
	}

	ltxt = slurp(log);
	if (ltxt[0] && strstr(ltxt, "error"))
		bad("O%d compiler emitted an error:\n%s", level, ltxt);
	scan_jit(ltxt, level);
	free(ltxt);

	ltxt = slurp(tsv);
	scan_rir(ltxt, level);
	free(ltxt);

	snprintf(args, sizeof args, "%s", g_poison ? "--poison" : "");
	st = run_subject(exe, args, out);
	otxt = slurp(out);
	if (!parse_summary(otxt, &checks, &sweep, &msweep, &fchecks, &failures,
										 got_digest, gdn)) {
		bad("O%d subject printed no summary line:\n%s", level, otxt);
		free(otxt);
		return 0;
	}
	if (failures || !exited_zero(st)) {
		bad("O%d subject reported %ld failures (exit %d):\n%s", level, failures,
				exit_code(st), otxt);
		ok = 0;
	}
	scan_failrows(otxt, level);
	free(otxt);

	if (checks <= 0 || sweep <= 0 || msweep <= 0) {
		bad("O%d subject executed checks=%ld sweep=%ld msweep=%ld; a run that "
				"compared nothing must not report success",
				level, checks, sweep, msweep);
		ok = 0;
	}
	if (want_digest && strcmp(want_digest, got_digest)) {
		bad("O%d sweep digest %s differs from -O0's %s; the optimizer changed a "
				"value",
				level, got_digest, want_digest);
		ok = 0;
	}

	g_checks_total += checks;
	g_cases_total += checks + sweep + msweep + fchecks;
	g_levels_run++;
	note("  O%-2d %5u ms  checks=%ld sweep=%ld msweep=%ld fsweep=%ld digest=%s\n",
			 level, ms, checks, sweep, msweep, fchecks, got_digest);
	run_traps(exe, level);
	return ok;
}

static void jit_census(int level)
{
	char exe[1024], log[1024], tsv[1024];
	unsigned ms = 0;
	char *txt;
	ts_path(exe, sizeof exe, g_work, "subject-jit.exe");
	ts_path(log, sizeof log, g_work, "compile-jit.log");
	ts_path(tsv, sizeof tsv, g_work, "rir-jit.tsv");
	if (!compile_subject(level, exe, log, tsv, &ms, 0, 0, 1)) {
		char *l = slurp(log);
		bad("jit census: the -O%d --embed-jit compile failed:\n%s", level, l);
		free(l);
		return;
	}
	txt = slurp(log);
	if (!strstr(txt, "refuse-to-JIT") && !strstr(txt, "no functions were JIT-baked"))
		bad("jit census: -O%d --embed-jit reported neither a refusal nor an "
				"empty bake, so the jit-not-baked ratchet would measure nothing",
				level);
	scan_jit(txt, level);
	free(txt);
	note("  jit census at -O%d in %u ms\n", level, ms);
}

static void slice_census(int level)
{
	char exe[1024], log[1024], tsv[1024];
	unsigned ms = 0;
	char *txt;
	ts_path(exe, sizeof exe, g_work, "subject-slice.exe");
	ts_path(log, sizeof log, g_work, "compile-slice.log");
	ts_path(tsv, sizeof tsv, g_work, "rir-slice.tsv");
	if (!compile_subject(level, exe, log, tsv, &ms, 0, 1, 0)) {
		char *l = slurp(log);
		bad("slice census: the -O%d ladder compile failed:\n%s", level, l);
		free(l);
		return;
	}
	txt = slurp(log);
	if (!strstr(txt, "] refuse "))
		bad("slice census: the -O%d ladder emitted no refusal histogram, so the "
				"slice-refused ratchet would measure nothing",
				level);
	scan_ladder(txt, level);
	free(txt);
	note("  slice census at -O%d in %u ms\n", level, ms);
}

static void wrapv_pass(int level, const char *flag, const char *tag,
											 const char *want_digest)
{
	char exe[1024], log[1024], tsv[1024], out[1024], dg[32] = "";
	unsigned ms = 0;
	long checks = 0, sweep = 0, msweep = 0, fchecks = 0, failures = 0;
	char *txt;
	int st;
	ts_path(exe, sizeof exe, g_work, "subject-%s.exe", tag);
	ts_path(log, sizeof log, g_work, "compile-%s.log", tag);
	ts_path(tsv, sizeof tsv, g_work, "rir-%s.tsv", tag);
	ts_path(out, sizeof out, g_work, "run-%s.txt", tag);
	g_extra_flags = flag;
	if (!compile_subject(level, exe, log, tsv, &ms, 0, 0, 0)) {
		char *l = slurp(log);
		bad("%s: the -O%d %s compile failed:\n%s", tag, level, flag, l);
		free(l);
		g_extra_flags = "";
		return;
	}
	g_extra_flags = "";
	st = run_subject(exe, "", out);
	txt = slurp(out);
	if (!parse_summary(txt, &checks, &sweep, &msweep, &fchecks, &failures, dg,
										 sizeof dg)) {
		bad("%s: the %s run printed no summary:\n%s", tag, flag, txt);
		free(txt);
		return;
	}
	if (failures || !exited_zero(st))
		bad("%s: %s reported %ld failures:\n%s", tag, flag, failures, txt);
	free(txt);
	if (want_digest && !strcmp(want_digest, dg))
		cat_add("wrapv wrapv-inert:digest-identical-under-%s", flag + 1);
	else if (want_digest)
		note("  %s changed the digest to %s (default %s)\n", flag, dg, want_digest);
	g_checks_total += checks;
	g_cases_total += checks + sweep + msweep + fchecks;
	note("  %-10s %5u ms  checks=%ld digest=%s\n", flag, ms, checks, dg);
}

static int device_probe(char *devname, size_t dn, long *dispatches)
{
	char exe[1024], log[1024], tsv[1024], out[1024];
	char dg[32] = "", dn2[32] = "";
	unsigned ms = 0;
	char *txt;
	const char *p;
	int available = 0;
	long checks = 0, sweep = 0, msweep = 0, fchecks = 0, failures = 0;

	ts_path(exe, sizeof exe, g_work, "subject-devcpu.exe");
	ts_path(log, sizeof log, g_work, "compile-devcpu.log");
	ts_path(tsv, sizeof tsv, g_work, "rir-devcpu.tsv");
	ts_path(out, sizeof out, g_work, "run-devcpu.txt");
	snprintf(devname, dn, "%s", "?");
	*dispatches = 0;
	if (!compile_subject(9, exe, log, tsv, &ms, 0, 0, 0)) {
		bad("device arm: the plain -O9 reference compile failed");
		return -1;
	}
	run_subject(exe, g_poison ? "--poison" : "", out);
	txt = slurp(out);
	if (!parse_summary(txt, &checks, &sweep, &msweep, &fchecks, &failures, dg,
										 sizeof dg)) {
		bad("device arm: the CPU reference run printed no summary");
		free(txt);
		return -1;
	}
	free(txt);

	ts_path(exe, sizeof exe, g_work, "subject-dev.exe");
	ts_path(log, sizeof log, g_work, "compile-dev.log");
	ts_path(tsv, sizeof tsv, g_work, "rir-dev.tsv");
	ts_path(out, sizeof out, g_work, "run-dev.txt");
	if (!compile_subject(9, exe, log, tsv, &ms, 1, 0, 0)) {
		char *l = slurp(log);
		bad("device arm: the compile failed under MCC_AST_EVAL_LADDER_GPU:\n%s", l);
		free(l);
		return -1;
	}
	txt = slurp(log);
	p = strstr(txt, "[ladder-gpu] tried=");
	if (!p) {
		free(txt);
		return 0;
	}
	sscanf(p, "[ladder-gpu] tried=%*d available=%d", &available);
	{
		const char *d = strstr(p, "device=");
		if (d) {
			size_t i = 0;
			d += 7;
			while (i + 1 < dn && d[i] && d[i] != ' ' && d[i] != '\n') {
				devname[i] = d[i];
				i++;
			}
			devname[i] = 0;
		}
	}
	{
		const char *d = strstr(p, "dispatches=");
		if (d)
			*dispatches = strtol(d + 11, NULL, 10);
	}
	scan_ladder(txt, 9);
	free(txt);
	if (!available) {
		cat_add("dev device-refused:unavailable");
		return 0;
	}
	if (*dispatches == 0)
		cat_add("dev device-refused:no-dispatch");

	{
		long c2 = 0, s2 = 0, m2 = 0, f2 = 0, x2 = 0;
		int st = run_subject(exe, g_poison ? "--poison" : "", out);
		txt = slurp(out);
		if (!parse_summary(txt, &c2, &s2, &m2, &f2, &x2, dn2, sizeof dn2)) {
			bad("device arm: the GPU-arm run printed no summary:\n%s", txt);
			free(txt);
			return available;
		}
		if (x2 || !exited_zero(st))
			bad("device arm: the GPU-arm binary reported %ld failures:\n%s", x2, txt);
		free(txt);
		if (strcmp(dg, dn2))
			bad("device arm: digest %s under the device oracle differs from the CPU "
					"oracle's %s; the device changed a value",
					dn2, dg);
		g_checks_total += checks + c2;
		g_cases_total += checks + sweep + msweep + c2 + s2 + m2;
		note("  device oracle: cpu-digest=%s gpu-digest=%s cases=%ld\n", dg, dn2,
				 checks + sweep + msweep + c2 + s2 + m2);
	}
	return available;
}

static int ref_build(const char *cc, const char *tag, const char *out)
{
	char cmd[4096], exe[1024], log[1024];
	int st;
	ts_path(exe, sizeof exe, g_work, "subject-%s.exe", tag);
	ts_path(log, sizeof log, g_work, "build-%s.log", tag);
	snprintf(cmd, sizeof cmd,
					 "\"%s\" -w -O1 -DSM_REF_BUILD=1 \"-I%s/tests/smoke\" "
					 "\"%s/tests/smoke/subject.c\" -o \"%s\" > \"%s\" 2>&1",
					 cc, g_srcdir, g_srcdir, exe, log);
	st = sm_system(cmd);
	if (!exited_zero(st))
		return 0;
	st = run_subject(exe, "--dump", out);
	return exited_zero(st);
}

static char *field(char *line, int idx)
{
	int i = 0;
	char *p = line;
	while (i < idx) {
		p = strchr(p, ' ');
		if (!p)
			return NULL;
		while (*p == ' ')
			p++;
		i++;
	}
	return p;
}

static int same_word(const char *a, const char *b)
{
	while (*a && *a != ' ' && *a != '\n' && *b && *b != ' ' && *b != '\n') {
		if (*a != *b)
			return 0;
		a++;
		b++;
	}
	return (!*a || *a == ' ' || *a == '\n') && (!*b || *b == ' ' || *b == '\n');
}

static void divergence(void)
{
	char mout[1024], gout[1024], cout[1024], exe[1024], log[1024], tsv[1024];
	char *m, *g, *c;
	unsigned ms = 0;
	const char *gcc = getenv("MCC_SMOKE_GCC");
	const char *clang = getenv("MCC_SMOKE_CLANG");
	int have_g, have_c, n3 = 0, ndis = 0, nloud = 0, nrows = 0;
	char *pm;

	if (!gcc || !gcc[0])
		gcc = "gcc-15";
	if (!clang || !clang[0])
		clang = "clang-22";

	ts_path(mout, sizeof mout, g_work, "dump-mcc.txt");
	ts_path(gout, sizeof gout, g_work, "dump-gcc.txt");
	ts_path(cout, sizeof cout, g_work, "dump-clang.txt");
	ts_path(exe, sizeof exe, g_work, "subject-dump.exe");
	ts_path(log, sizeof log, g_work, "compile-dump.log");
	ts_path(tsv, sizeof tsv, g_work, "rir-dump.tsv");

	if (!compile_subject(1, exe, log, tsv, &ms, 0, 0, 0)) {
		bad("divergence: mcc could not build the subject");
		return;
	}
	if (!exited_zero(run_subject(exe, "--dump", mout))) {
		bad("divergence: the mcc subject dump failed");
		return;
	}
	have_g = ref_build(gcc, "gcc", gout);
	have_c = ref_build(clang, "clang", cout);
	if (!have_g && !have_c) {
		fprintf(stderr,
						"SKIP: smokerun found no working reference compiler (tried %s and "
						"%s); the UB divergence report needs one, and the goldens stand "
						"on their own without it\n",
						gcc, clang);
		g_skip = 1;
		return;
	}
	m = slurp(mout);
	g = have_g ? slurp(gout) : NULL;
	c = have_c ? slurp(cout) : NULL;

	note("smokerun: UB divergence vs %s%s%s\n", have_g ? gcc : "",
			 have_g && have_c ? " and " : "", have_c ? clang : "");
	pm = m;
	while (pm && *pm) {
		char *eol = strchr(pm, '\n');
		char name[192];
		char *nm;
		size_t nlen;
		if (eol)
			*eol = 0;
		nm = field(pm, 1);
		if (!nm)
			goto next;
		nlen = strcspn(nm, " ");
		if (nlen >= sizeof name)
			goto next;
		memcpy(name, nm, nlen);
		name[nlen] = 0;
		nrows++;
		{
			const char *gl = NULL, *cl = NULL;
			char pat[200];
			snprintf(pat, sizeof pat, "%c %s ", pm[0], name);
			if (g)
				gl = strstr(g, pat);
			if (c)
				cl = strstr(c, pat);
			if (!gl && !cl)
				goto next;
			n3++;
			{
				static const int cols[3] = {3, 4, 0};
				static const char *const colname[3] = {"fold", "run", ""};
				int ci;
				for (ci = 0; cols[ci]; ci++) {
					const char *mv = field(pm, cols[ci]);
					const char *gv = gl ? field((char *)gl, cols[ci]) : NULL;
					const char *cv = cl ? field((char *)cl, cols[ci]) : NULL;
					int dg, dc;
					if (!mv)
						continue;
					dg = gv && !same_word(mv, gv);
					dc = cv && !same_word(mv, cv);
					if (!dg && !dc)
						continue;
					ndis++;
					if (dg && dc) {
						nloud++;
						cat_add("div diverge-both:%s.%s", name, colname[ci]);
						note("DIVERGE-BOTH %s.%s mcc=%.16s gcc=%.16s clang=%.16s\n", name,
								 colname[ci], mv, gv ? gv : "-", cv ? cv : "-");
					} else {
						cat_add("div diverge-one:%s.%s", name, colname[ci]);
						note("DIVERGE      %s.%s mcc=%.16s gcc=%.16s clang=%.16s\n", name,
								 colname[ci], mv, gv ? gv : "=", cv ? cv : "=");
					}
				}
			}
		}
	next:
		if (!eol)
			break;
		pm = eol + 1;
	}
	note("smokerun: divergence rows=%d comparable=%d differing=%d "
			 "mcc-differs-from-both=%d\n",
			 nrows, n3, ndis, nloud);
	if (nloud)
		note("smokerun: %d case(s) where mcc differs from BOTH references -- these "
				 "are defects to triage, not consensus to bank\n",
				 nloud);
	if (n3 == 0)
		bad("divergence: nothing was comparable, so the report proves nothing");
	free(m);
	free(g);
	free(c);
}

static void usage(void)
{
	fprintf(stderr,
					"usage: smokerun --mcc PATH --srcdir DIR --work DIR [--min-cases N]\n"
					"                [--max-level N] [--budget-ms N] [--deadline-ms N]\n"
					"                [--bank FILE] [--rebank] [--known-positive]\n"
					"                [--divergence] [--device] [--require-device] [-v]\n");
}

int main(int argc, char **argv)
{
	char bankpath[1024] = "";
	int maxlevel = SMK_MAXLEVEL;
	int i, do_div = 0, do_dev = 0, known_pos = 0, do_slice = 0;
	char d0[32] = "", dn[32] = "";

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--mcc") && i + 1 < argc)
			g_mcc = argv[++i];
		else if (!strcmp(argv[i], "--srcdir") && i + 1 < argc)
			g_srcdir = argv[++i];
		else if (!strcmp(argv[i], "--work") && i + 1 < argc)
			g_work = argv[++i];
		else if (!strcmp(argv[i], "--bank") && i + 1 < argc)
			snprintf(bankpath, sizeof bankpath, "%s", argv[++i]);
		else if (!strcmp(argv[i], "--min-cases") && i + 1 < argc)
			g_min_cases = strtol(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--max-level") && i + 1 < argc)
			maxlevel = (int)strtol(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--budget-ms") && i + 1 < argc)
			g_budget_ms = (unsigned)strtoul(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--deadline-ms") && i + 1 < argc)
			g_deadline_ms = (unsigned)strtoul(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--rebank"))
			g_rebank = g_rebank_req = 1;
		else if (!strcmp(argv[i], "--known-positive"))
			known_pos = 1;
		else if (!strcmp(argv[i], "--divergence"))
			do_div = 1;
		else if (!strcmp(argv[i], "--slice-census"))
			do_slice = 1;
		else if (!strcmp(argv[i], "--device"))
			do_dev = 1;
		else if (!strcmp(argv[i], "--require-device"))
			g_require_device = do_dev = 1;
		else if (!strcmp(argv[i], "-v"))
			g_verbose = 1;
		else {
			fprintf(stderr, "smokerun: unknown argument '%s'\n", argv[i]);
			usage();
			return 2;
		}
	}
	if (!g_mcc || !g_srcdir || !g_work) {
		usage();
		return 2;
	}
	host_mkdirs(g_work);
	if (!bankpath[0])
		snprintf(bankpath, sizeof bankpath, "%s/tests/smoke/bails.txt", g_srcdir);

	if (maxlevel < 0 || maxlevel > SMK_MAXLEVEL)
		maxlevel = SMK_MAXLEVEL;

	if (do_div) {
		bank_load(bankpath);
		divergence();
		if (g_skip)
			return TS_SKIP_CODE;
		own("diverge-");
		ratchet(bankpath);
		return g_fail ? 1 : 0;
	}

	if (do_slice) {
		bank_load(bankpath);
		slice_census(9);
		own("slice-");
		ratchet(bankpath);
		return g_fail ? 1 : 0;
	}

	if (do_dev) {
		char name[128];
		g_poison = known_pos;
		long disp = 0;
		int avail = device_probe(name, sizeof name, &disp);
		if (avail < 0)
			return 1;
		if (!avail) {
			if (g_require_device) {
				fprintf(stderr,
								"smokerun: --require-device but the ladder reports "
								"available=0 (device=%s)\n",
								name);
				return 1;
			}
			fprintf(stderr,
							"SKIP: smokerun device arm found no usable compute device "
							"(ladder-gpu available=0, device=%s); install a Vulkan ICD, or "
							"Metal on Darwin\n",
							name);
			return TS_SKIP_CODE;
		}
		bank_load(bankpath);
		note("smokerun: device=%s dispatches=%ld value-cases=%ld\n", name, disp,
				 g_cases_total);
		if (disp == 0) {
			bad("device arm: available=1 but zero dispatches, so the device half "
					"never ran and an agreeing verdict proves nothing");
			return 1;
		}
		if (g_cases_total < g_min_cases) {
			fprintf(stderr,
							"smokerun: device arm examined %ld value cases, below the "
							"--min-cases %ld floor\n",
							g_cases_total, g_min_cases);
			return 1;
		}
		if (known_pos) {
			if (!g_fail) {
				fprintf(stderr, "smokerun: --known-positive poisoned an expectation "
												"and the device arm still passed; it is blind\n");
				return 1;
			}
			note("smokerun: device known-positive fired %d failure(s), as it must\n",
					 g_fail);
			return 0;
		}
		return g_fail ? 1 : 0;
	}

	g_poison = known_pos;
	bank_load(bankpath);

	note("smokerun: mcc=%s levels=0..%d search-budget=%ums deadline=%ums\n", g_mcc,
			 maxlevel, g_budget_ms, g_deadline_ms);

	for (i = 0; i <= maxlevel; i++) {
		level_pass(i, i == 0 ? NULL : d0, dn, sizeof dn);
		if (i == 0)
			snprintf(d0, sizeof d0, "%s", dn);
	}

	wrapv_pass(2, "-fwrapv", "wrapv", d0);
	wrapv_pass(2, "-fno-wrapv", "nowrapv", d0);
	rowlev_report(maxlevel);

	if (!known_pos) {
		int rb = g_rebank;
		g_rebank = 0;
		own("replay-");
		ratchet(bankpath);
		jit_census(2);
		own("jit-");
		ratchet(bankpath);
		own("wrapv-");
		ratchet(bankpath);
		if (rb) {
			g_rebank = 1;
			bank_write(bankpath);
		}
	}

	note("smokerun: levels=%d checks=%ld value-cases=%ld failures=%d\n",
			 g_levels_run, g_checks_total, g_cases_total, g_fail);

	if (g_levels_run == 0) {
		fprintf(stderr, "smokerun: zero levels ran; a suite that executed nothing "
										"must not report success\n");
		return 1;
	}
	if (g_cases_total < g_min_cases) {
		fprintf(stderr,
						"smokerun: examined %ld value cases, below the --min-cases %ld "
						"floor -- an empty or truncated run must not read as a pass\n",
						g_cases_total, g_min_cases);
		return 1;
	}
	if (known_pos) {
		if (!g_fail) {
			fprintf(stderr, "smokerun: --known-positive poisoned an expectation and "
											"every level still passed; the suite is blind\n");
			return 1;
		}
		note("smokerun: known-positive fired %d failure(s), as it must\n", g_fail);
		return 0;
	}
	return g_fail ? 1 : 0;
}
