#include "toolsupport.h"
#include "../src/mccopt.h"
#include "../src/mcchost.h"

#include <time.h>

#if !MCC_HOST_WIN32
#include <sys/wait.h>
#include <signal.h>
#endif

#define SMK_MAXCAT 4096
#define SMK_MAXBANK 4096

#if defined __x86_64__ || defined _M_X64
#define SMK_TARGET_ARCH "x86_64"
#elif defined __aarch64__ || defined _M_ARM64
#define SMK_TARGET_ARCH "arm64"
#elif defined __i386__ || defined _M_IX86
#define SMK_TARGET_ARCH "i386"
#elif defined __arm__ || defined _M_ARM
#define SMK_TARGET_ARCH "arm"
#elif defined __riscv
#define SMK_TARGET_ARCH "riscv64"
#else
#define SMK_TARGET_ARCH "unknown"
#endif

#if MCC_HOST_WIN32
#define SMK_TARGET_OS "windows"
#elif MCC_HOST_DARWIN
#define SMK_TARGET_OS "macos"
#elif MCC_HOST_LINUX
#define SMK_TARGET_OS "linux"
#else
#define SMK_TARGET_OS "unix"
#endif

#define SMK_TARGET_KEY SMK_TARGET_ARCH "-" SMK_TARGET_OS

static int smk_maxlevel(void)
{
	static const int dflt[] = {
#define MCC_OPT_ROW(id, name, d) (d),
			MCC_OPT_LIST(MCC_OPT_ROW)
#undef MCC_OPT_ROW
	};
	int i, top = 0;
	for (i = 0; i < MCC_OPT_COUNT; i++) {
		int d = dflt[i];
		if (MCC_OPTD_IS_DEV(d))
			continue;
		if (!MCC_OPTD_IS_LEVEL(d))
			continue;
		if (MCC_OPTD_LEVEL_OF(d) > top)
			top = MCC_OPTD_LEVEL_OF(d);
	}
	return top;
}

static const char *g_mcc;
static const char *g_srcdir;
static const char *g_work;
static int g_verbose;
static unsigned g_budget_ms = 1000;
static unsigned g_deadline_ms = 6000;
static long g_min_cases;
static long g_min_passes;
static long g_min_strats;
#define SMK_MAXSTRAT 64

static int g_strat_total;
static int g_strat_fired;
static int g_poison;
static int g_rebank;
static int g_rebank_req;
static int g_require_device;
static int g_skip;
static int g_force_replay = 1;
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

#define SMK_FORK_TRIES 6
#define SMK_FORK_BACKOFF_MS 120

static unsigned sm_fork_retries;

static int sm_system(const char *cmd)
{
	int st = -1;
	int i;
#if MCC_HOST_WIN32
	size_t n = strlen(cmd);
	char *w = malloc(n + 3);

	if (w) {
		w[0] = '"';
		memcpy(w + 1, cmd, n);
		w[n + 1] = '"';
		w[n + 2] = 0;
		cmd = w;
	}
#endif

	for (i = 0; i < SMK_FORK_TRIES; i++) {
		st = system(cmd);
		if (st != -1)
			break;
		sm_fork_retries++;
		if (i + 1 < SMK_FORK_TRIES) {
			unsigned t0 = now_ms();
			while (now_ms() - t0 < SMK_FORK_BACKOFF_MS * (unsigned)(i + 1))
				;
		}
	}
	if (st == -1)
		fprintf(stderr, "smoke: fork failed %d times for: %s\n", SMK_FORK_TRIES,
						cmd);
#if MCC_HOST_WIN32
	free(w);
#endif
	return st;
}

#define SMK_TRAP_EXIT 97

static int died_by_fpe(int st)
{
	if (st == -1)
		return 0;
#if !MCC_HOST_WIN32
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
#if !MCC_HOST_WIN32
	return st != -1 && WIFEXITED(st) && WEXITSTATUS(st) == 0;
#else
	return st == 0;
#endif
}

static int exit_code(int st)
{
#if !MCC_HOST_WIN32
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
	} else {
		long r = 0, w = 0;
		while (r < n) {
			if (s[r] == '\r' && r + 1 < n && s[r + 1] == '\n')
				r++;
			s[w++] = s[r++];
		}
		s[w] = 0;
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
	if (g_force_replay)
		host_setenv("MCC_FORCE_REPLAY", "1");
	else
		host_unsetenv("MCC_FORCE_REPLAY");
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
	host_unsetenv("MCC_STATS");
}

static const char *const kLadderReasons[] = {
		"no-static-type", "unsupported-node", "unsupported-op",
		"too-many-live-ins", "no-observed", "over-budget", "all-undefined", NULL};

static void scan_ladder_tagged(const char *text, int level, const char *tag)
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
			cat_addn(strtol(q + strlen(pat), NULL, 10), "O%d %sslice-refused:%s", level,
							 tag, kLadderReasons[i]);
		}
		p = eol ? eol : line + len;
	}
}

static void scan_ladder(const char *text, int level)
{
	scan_ladder_tagged(text, level, "");
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

static const char *g_src = "subject.c";

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
					 "\"-I%s/tests/smoke\" \"%s/tests/smoke/%s\" -lm -o \"%s\" "
					 "> \"%s\" 2>&1",
					 g_mcc, level, jit ? "--embed-jit" : "", g_extra_flags, g_srcdir,
					 g_srcdir, g_src, exe, log);
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

static int run_subject_err(const char *exe, const char *args, const char *out,
													 const char *err)
{
	char cmd[4096];
	snprintf(cmd, sizeof cmd, "\"%s\" %s > \"%s\" 2> \"%s\"", exe, args, out, err);
	return sm_system(cmd);
}

static int run_subject_nocore(const char *exe, const char *args,
															const char *out)
{
	char cmd[4096];
#if !MCC_HOST_WIN32
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
	fprintf(f, "# regenerated by smokerun --rebank; the hand-written header "
						 "that triages every row below is NOT generated and must be "
						 "restored by hand\n");
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
		bad("O%d subject listed %d trap rows; the divide-by-zero and MIN/-1 cases "
				"would not be exercised at all",
				level, n);
		return;
	}
	p = names;
	for (i = 0; i < n; i++) {
		int st;
		char *eol = strchr(p, '\n');
		char *want = strchr(p, '\t');
		if (eol)
			*eol = 0;
		if (!want) {
			bad("O%d trap row %d ('%s') carries no expectation; --trapnames must "
					"emit 'name<TAB>fault' or 'name<TAB><hex>'",
					level, i, p);
			if (!eol)
				break;
			p = eol + 1;
			continue;
		}
		*want++ = 0;
		snprintf(cmd, sizeof cmd, "--trap %d", i);
		st = run_subject_nocore(exe, cmd, out);
		g_checks_total++;
		g_cases_total++;
		if (!strcmp(want, "fault")) {
			if (!died_by_fpe(st)) {
				char *o = slurp(out);
				bad("O%d trap row %s did not fault (status=%d): %s", level, p,
						exit_code(st), o);
				free(o);
			}
		} else {
			char *o = slurp(out);
			char wnt[128];
			snprintf(wnt, sizeof wnt, "NOTRAP %s %s\n", p, want);
			if (died_by_fpe(st) || exit_code(st) != 0 || strcmp(o, wnt))
				bad("O%d trap row %s: this target does not fault on the divide, so "
						"the row must return %s; got status=%d output '%s'",
						level, p, want, exit_code(st), o);
			free(o);
		}
		if (!eol)
			break;
		p = eol + 1;
	}
	if (g_verbose)
		note("  O%-2d traps=%d checked\n", level, n);
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

static void strat_census(int level)
{
	char exe[1024], log[1024], tsv[1024];
	unsigned ms = 0;
	char *txt, *p, *q;
	int total = 0, fired = 0;
	ts_path(exe, sizeof exe, g_work, "subject-strat.exe");
	ts_path(log, sizeof log, g_work, "compile-strat.log");
	ts_path(tsv, sizeof tsv, g_work, "rir-strat.tsv");
	host_setenv("MCC_STATS", "4");
	if (!compile_subject(level, exe, log, tsv, &ms, 0, 0, 0)) {
		char *l = slurp(log);
		bad("strategy census: the -O%d compile failed:\n%s", level, l);
		free(l);
		return;
	}
	txt = slurp(log);
	p = strstr(txt, "[strategy] ");
	if (!p) {
		bad("strategy census: the -O%d compile emitted no [strategy] record, so "
				"the --min-strats floor would measure nothing",
				level);
		free(txt);
		return;
	}
	q = strchr(p, '\n');
	if (q)
		*q = '\0';
	p += strlen("[strategy] ");
	while (*p) {
		char *eq;
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		eq = strchr(p, '=');
		if (!eq)
			break;
		if (strncmp(p, "calls=", 6) != 0) {
			total++;
			if (strtoul(eq + 1, NULL, 10) > 0)
				fired++;
		}
		p = eq + 1;
		while (*p && *p != ' ')
			p++;
	}
	g_strat_total = total;
	g_strat_fired = fired;
	free(txt);
	note("  strategy census at -O%d: %d of %d fired, in %u ms\n", level, fired,
			 total, ms);
}

/* The -O13 tier prints one [strategy] panel per search phase and the last one
   reads all zeros, so a census that takes the first or the last record is
   measuring a phase rather than the compile.  Take the per-column max.  Bank
   the strategies that stayed dark, not the fire counts: a fire count must not
   fall, which is the wrong direction for a monotone-decreasing bank, whereas a
   strategy going dark is a new category (hard fail) and one lighting up is a
   row that fell to zero (IMPROVED).  ratchet() needs no change for either. */
static void strat_dark_census(void)
{
	char exe[1024], log[1024], tsv[1024];
	unsigned ms = 0;
	char *txt, *p;
	char names[SMK_MAXSTRAT][32];
	long best[SMK_MAXSTRAT];
	int n = 0, i, recs = 0, dark = 0;

	ts_path(exe, sizeof exe, g_work, "subject-o13.exe");
	ts_path(log, sizeof log, g_work, "compile-o13.log");
	ts_path(tsv, sizeof tsv, g_work, "rir-o13.tsv");
	host_setenv("MCC_STATS", "4");
	if (!compile_subject(MCC_OPT_SEARCH_LEVEL, exe, log, tsv, &ms, 0, 0, 0)) {
		char *l = slurp(log);
		bad("-O%d strategy census: the compile failed:\n%s", MCC_OPT_SEARCH_LEVEL,
				l);
		free(l);
		return;
	}
	txt = slurp(log);
	p = txt;
	while ((p = strstr(p, "[strategy] ")) != NULL) {
		char line[2048];
		char *eol, *t;
		size_t len;
		p += strlen("[strategy] ");
		eol = strchr(p, '\n');
		len = eol ? (size_t)(eol - p) : strlen(p);
		if (len >= sizeof line)
			len = sizeof line - 1;
		memcpy(line, p, len);
		line[len] = '\0';
		recs++;
		for (t = strtok(line, " "); t; t = strtok(NULL, " ")) {
			char *eq = strchr(t, '=');
			long v;
			if (!eq)
				continue;
			*eq = '\0';
			v = strtol(eq + 1, NULL, 10);
			if (!strcmp(t, "calls"))
				continue;
			for (i = 0; i < n; i++)
				if (!strcmp(names[i], t))
					break;
			if (i == n) {
				if (n >= SMK_MAXSTRAT) {
					bad("-O%d strategy census: more than %d strategies in the panel, so "
							"the census silently stopped counting",
							MCC_OPT_SEARCH_LEVEL, SMK_MAXSTRAT);
					free(txt);
					return;
				}
				snprintf(names[n], sizeof names[n], "%s", t);
				best[n] = 0;
				n++;
			}
			if (v > best[i])
				best[i] = v;
		}
		p = eol ? eol : p + len;
	}
	if (!recs) {
		bad("-O%d strategy census: the compile emitted no [strategy] record, so "
				"the dark-strategy bank would measure nothing",
				MCC_OPT_SEARCH_LEVEL);
		free(txt);
		return;
	}
	for (i = 0; i < n; i++)
		if (best[i] == 0) {
			dark++;
			cat_add("O%d strat-dark:%s", MCC_OPT_SEARCH_LEVEL, names[i]);
		}
	note("  strategy census at -O%d: %d of %d fired, %d dark, over %d panel(s), "
			 "in %u ms\n",
			 MCC_OPT_SEARCH_LEVEL, n - dark, n, dark, recs, ms);
	if (n == 0)
		bad("-O%d strategy census: the panel named no strategies at all",
				MCC_OPT_SEARCH_LEVEL);
	free(txt);
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
	if (!compile_subject(smk_maxlevel(), exe, log, tsv, &ms, 0, 0, 0)) {
		bad("device arm: the plain -O%d reference compile failed", smk_maxlevel());
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
	if (!compile_subject(smk_maxlevel(), exe, log, tsv, &ms, 1, 0, 0)) {
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
	scan_ladder_tagged(txt, 9, "dev-");
	free(txt);
	if (!available) {
		cat_add("dev dev-refused:unavailable");
		return 0;
	}
	if (*dispatches == 0)
		cat_add("dev dev-refused:no-dispatch");

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

#define SMK_REF_FLAGS "-O1 -ffp-contract=off -DSM_REF_BUILD=1"

static int ref_build(const char *cc, const char *tag, const char *out,
										 const char *src, const char *flags, const char *args)
{
	char cmd[4096], exe[1024], log[1024];
	int st;
	ts_path(exe, sizeof exe, g_work, "subject-%s.exe", tag);
	ts_path(log, sizeof log, g_work, "build-%s.log", tag);
	snprintf(cmd, sizeof cmd,
					 "\"%s\" -w %s \"-I%s/tests/smoke\" "
					 "\"%s/tests/smoke/%s\" -o \"%s\" > \"%s\" 2>&1",
					 cc, flags, g_srcdir, g_srcdir, src, exe, log);
	st = sm_system(cmd);
	if (!exited_zero(st))
		return 0;
	st = run_subject(exe, args, out);
	return exited_zero(st);
}

static const char *ref_gcc(void)
{
	const char *cc = getenv("MCC_SMOKE_GCC");
	return cc && cc[0] ? cc : "gcc-15";
}

static const char *ref_clang(void)
{
	const char *cc = getenv("MCC_SMOKE_CLANG");
	return cc && cc[0] ? cc : "clang-22";
}

static int ref_is_clang(const char *cc)
{
	char cmd[2048], out[1024];
	char *txt;
	int r;
	ts_path(out, sizeof out, g_work, "reffamily.txt");
	snprintf(cmd, sizeof cmd, "\"%s\" -dM -E -x c %s > \"%s\" 2>&1", cc,
					 MCC_HOST_WIN32 ? "NUL" : "/dev/null", out);
	if (!exited_zero(sm_system(cmd)))
		return -1;
	txt = slurp(out);
	r = strstr(txt, "__clang__") ? 1 : (strstr(txt, "__GNUC__") ? 0 : -1);
	free(txt);
	return r;
}

typedef struct
{
	const char *name;
	const char *file;
	const char *flags;
	const char *want;
	const char *oracle;
	const char *mark;
	int lo;
	int hi;
	const char *nomark;
} Pass;

static const Pass g_pass[] = {
		{"wrapv", NULL, "-fwrapv", NULL, NULL, NULL, 2, 2},
		{"nowrapv", NULL, "-fno-wrapv", NULL, NULL, NULL, 2, 2},
		{"asmreplay", "pass-asmreplay.c", "", "asmreplay 0 42 8\n", "-O0", NULL, 0,
		 -1, "assembler label 'smp_asm_label' already defined"},
		{"c90tag", "pass-c90tag.c", "-std=iso9899:1990", "c90tag 32 16 24 1\n",
		 "-O2 -std=iso9899:1990", NULL, 0, -1},
		{"absshadow", "pass-absshadow.c", "-fno-builtin",
		 "absshadow 2147483647 9223372036854775807 1 2147483647 "
		 "9223372036854775807\n",
		 "-O2 -fno-builtin", NULL, 0, -1},
		{"cplxcond", "pass-cplxcond.c", "",
		 "cplxcond 1 2 5 6 0 7 3.0 4.0 9.0 8.0 5\n", "-O2", NULL, 0, -1},
		{"msstruct", "pass-msstruct.c", "",
#if MCC_HOST_WIN32
		 /* PE default is ms_bitfields (T-win-50015): the one unmarked struct
		  * smp_plain {char;int:3;char} lays out MS-style = 12, not GCC's 4. */
		 "msstruct 20 2 8 3 8 4 8 12 5\n",
#else
		 "msstruct 20 2 8 3 8 4 8 4 5\n",
#endif
		 "-O2", NULL, 0, -1},
		{"fabscmp", "pass-fabscmp.c", "",
		 "fabscmp 0 0 0 0 0 0 0 0 1 0 0\n", "-O2", NULL, 0, -1},
		{"taut", "pass-taut.c", "",
		 "taut 111 000 111 000 111 111 100\n", "-O2", NULL, 0, -1},
		{"deadcase", "pass-deadcase.c", "",
		 "deadcase 1 0 3 4 5 1 11 6 8 0\n", "-O2", NULL, 0, -1},
		{"narrowelim", "pass-narrowelim.c", "",
		 "narrowelim -199 -199 -101 -7 0 -993 -7000 99 -199 -101 0 -993 -199 -993 "
		 "-199 -7\n",
		 "-O2", NULL, 0, -1},
		{"tautconv", "pass-tautconv.c", "",
		 "tautconv 111110101111110011111110111101101111011011110110"
		 "11111010111111101111111011111110 1111\n",
		 "-O2", NULL, 0, -1},
};

#define SMK_NPASS ((int)(sizeof g_pass / sizeof g_pass[0]))

static long g_pass_checks;
static long g_pass_rowchecks[SMK_NPASS];
static int g_pass_rows;
static int g_pass_fails;

static const char *pass_want(const Pass *p)
{
	static char poisoned[512];
	if (!g_poison || !p->want)
		return p->want;
	snprintf(poisoned, sizeof poisoned, "%s-poisoned\n", p->want);
	return poisoned;
}

static void pass_subject(const Pass *p, int level, const char *want_digest,
												 int idx)
{
	char exe[1024], log[1024], tsv[1024], out[1024], dg[32] = "";
	unsigned ms = 0;
	long checks = 0, sweep = 0, msweep = 0, fchecks = 0, failures = 0;
	char *txt;
	int st;

	ts_path(exe, sizeof exe, g_work, "subject-%s.exe", p->name);
	ts_path(log, sizeof log, g_work, "compile-%s.log", p->name);
	ts_path(tsv, sizeof tsv, g_work, "rir-%s.tsv", p->name);
	ts_path(out, sizeof out, g_work, "run-%s.txt", p->name);
	g_extra_flags = p->flags;
	g_pass_checks++;
	g_pass_rowchecks[idx]++;
	if (!compile_subject(level, exe, log, tsv, &ms, 0, 0, 0)) {
		char *l = slurp(log);
		bad("%s: the -O%d %s compile failed:\n%s", p->name, level, p->flags, l);
		free(l);
		g_extra_flags = "";
		rowlev_note(p->name, level);
		g_pass_fails++;
		return;
	}
	g_extra_flags = "";
	st = run_subject(exe, "", out);
	txt = slurp(out);
	g_pass_checks++;
	g_pass_rowchecks[idx]++;
	if (!parse_summary(txt, &checks, &sweep, &msweep, &fchecks, &failures, dg,
										 sizeof dg)) {
		bad("%s: the %s run printed no summary:\n%s", p->name, p->flags, txt);
		free(txt);
		rowlev_note(p->name, level);
		g_pass_fails++;
		return;
	}
	if (failures || !exited_zero(st)) {
		bad("%s: %s reported %ld failures:\n%s", p->name, p->flags, failures, txt);
		rowlev_note(p->name, level);
		g_pass_fails++;
	}
	free(txt);
	if (want_digest && !strcmp(want_digest, dg))
		cat_add("wrapv wrapv-inert:digest-identical-under-%s", p->flags + 1);
	else if (want_digest)
		note("  %s changed the digest to %s (default %s)\n", p->flags, dg,
				 want_digest);
	g_checks_total += checks;
	g_cases_total += checks + sweep + msweep + fchecks;
	note("  %-10s %5u ms  checks=%ld digest=%s\n", p->flags, ms, checks, dg);
}

static int pass_expect(const Pass *p, const char *who, const char *got,
											 int level, int idx)
{
	const char *want = pass_want(p);
	g_pass_checks++;
	g_pass_rowchecks[idx]++;
	if (!strcmp(want, got))
		return 1;
	bad("%s: %s printed \"%s\" but the pinned answer is \"%s\"", p->name, who,
			got, want);
	if (level >= 0)
		rowlev_note(p->name, level);
	g_pass_fails++;
	return 0;
}

static void pass_fixture_level(const Pass *p, int level, int idx)
{
	char exe[1024], log[1024], tsv[1024], out[1024];
	unsigned ms = 0;
	char *txt;
	int st;

	ts_path(exe, sizeof exe, g_work, "pass-%s-O%d.exe", p->name, level);
	ts_path(log, sizeof log, g_work, "pass-%s-O%d.log", p->name, level);
	ts_path(tsv, sizeof tsv, g_work, "pass-%s-O%d.tsv", p->name, level);
	ts_path(out, sizeof out, g_work, "pass-%s-O%d.txt", p->name, level);

	g_src = p->file;
	g_extra_flags = p->flags;
	g_pass_checks++;
	g_pass_rowchecks[idx]++;
	st = compile_subject(level, exe, log, tsv, &ms, 0, 0, 0);
	g_src = "subject.c";
	g_extra_flags = "";
	if (!st) {
		char *l = slurp(log);
		bad("%s: the -O%d %s compile of %s failed; the parser did not survive the "
				"pass:\n%s",
				p->name, level, p->flags, p->file, l);
		free(l);
		rowlev_note(p->name, level);
		g_pass_fails++;
		return;
	}
	txt = slurp(log);
	g_pass_checks++;
	g_pass_rowchecks[idx]++;
	if (p->nomark && strstr(txt, p->nomark)) {
		bad("%s: the -O%d compile of %s printed \"%s\", which this row exists to "
				"assert can no longer happen:\n%s",
				p->name, level, p->file, p->nomark, txt);
		rowlev_note(p->name, level);
		g_pass_fails++;
	}
	if (p->mark) {
		if (!strstr(txt, p->mark)) {
			bad("%s: the -O%d compile of %s never printed \"%s\", so the path the "
					"pass exists to cover was not taken:\n%s",
					p->name, level, p->file, p->mark, txt);
			rowlev_note(p->name, level);
			g_pass_fails++;
		}
	} else if (!p->nomark && txt[0] && strstr(txt, "error")) {
		bad("%s: the -O%d compile of %s emitted an error:\n%s", p->name, level,
				p->file, txt);
		rowlev_note(p->name, level);
		g_pass_fails++;
	}
	free(txt);

	st = run_subject(exe, "", out);
	txt = slurp(out);
	if (!exited_zero(st)) {
		bad("%s: the -O%d binary exited %d:\n%s", p->name, level, exit_code(st),
				txt);
		rowlev_note(p->name, level);
		g_pass_fails++;
	}
	pass_expect(p, "mcc", txt, level, idx);
	if (g_verbose)
		note("  %-10s O%-2d %5u ms  %s", p->name, level, ms, txt);
	free(txt);
}

static void pass_oracle(const Pass *p, int idx)
{
	char out[1024], tag[160];
	const char *cc[2];
	int i, any = 0;

	if (!p->oracle)
		return;
	cc[0] = ref_gcc();
	cc[1] = ref_clang();
	{
		int fg = ref_is_clang(cc[0]), fc = ref_is_clang(cc[1]);
		if (fg >= 0 && fg == fc)
			cc[1] = NULL;
	}
	for (i = 0; i < 2; i++) {
		char *txt;
		if (!cc[i])
			continue;
		snprintf(tag, sizeof tag, "%s-ref%d", p->name, i);
		ts_path(out, sizeof out, g_work, "pass-%s.txt", tag);
		if (!ref_build(cc[i], tag, out, p->file, p->oracle, ""))
			continue;
		any = 1;
		txt = slurp(out);
		pass_expect(p, cc[i], txt, -1, idx);
		free(txt);
	}
	if (!any)
		note("  %-10s no reference compiler could adjudicate (%s, %s); the pinned "
				 "answer stands on mcc alone\n",
				 p->name, cc[0], cc[1] ? cc[1] : "same family as the first");
	else
		note("  %-10s oracle-adjudicated by %s (%s)\n", p->name, p->oracle,
				 cc[1] ? "two independent references"
							 : "one reference; the pair is one implementation family");
}

static void passes_run(int maxlevel, const char *want_digest)
{
	int i;
	for (i = 0; i < SMK_NPASS; i++) {
		const Pass *p = &g_pass[i];
		int lo = p->lo, hi = p->hi < 0 ? maxlevel : p->hi, lv;
		if (hi > maxlevel)
			hi = maxlevel;
		if (lo > hi)
			lo = hi;
		for (lv = lo; lv <= hi; lv++) {
			if (p->want)
				pass_fixture_level(p, lv, i);
			else
				pass_subject(p, lv, want_digest, i);
		}
		if (p->want && !g_poison)
			pass_oracle(p, i);
		g_pass_rows++;
		if (g_pass_rowchecks[i] <= 0)
			bad("pass '%s' executed zero checks, so a green run proves nothing about "
					"it; a pass that compiles nothing must not read as a pass",
					p->name);
	}
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

static char *points_of(const char *exe, const char *cat, const char *tag)
{
	char out[1024], args[256];
	snprintf(args, sizeof args, "--points %s", cat);
	ts_path(out, sizeof out, g_work, "points-%s.txt", tag);
	if (!exited_zero(run_subject(exe, args, out)))
		return NULL;
	return slurp(out);
}

static const char *line_at(const char *txt, long n)
{
	long i = 0;
	if (!txt)
		return NULL;
	while (*txt && i < n) {
		const char *e = strchr(txt, '\n');
		if (!e)
			return NULL;
		txt = e + 1;
		i++;
	}
	return *txt ? txt : NULL;
}

static int line_eq(const char *a, const char *b)
{
	if (!a || !b)
		return 0;
	while (*a && *a != '\n' && *b && *b != '\n') {
		if (*a != *b)
			return 0;
		a++;
		b++;
	}
	return (!*a || *a == '\n') && (!*b || *b == '\n');
}

/* A verdict class computed on a category digest says "at least one point in
 * this category behaved like this".  refs-disagree then clears every other
 * point in the category, including points where the references DO agree and
 * mcc differs -- which is the exact defect this class was added to fix, one
 * level down.  This re-runs the three subjects over the category's own points
 * and reports how many of them the digest verdict is hiding. */
static void decompose(const char *mexe, const char *gexe, const char *cexe,
											const char *cat)
{
	char *pm = points_of(mexe, cat, "mcc");
	char *pg = gexe ? points_of(gexe, cat, "gcc") : NULL;
	char *pc = cexe ? points_of(cexe, cat, "clang") : NULL;
	long i, agree_mcc_differs = 0, n = 0;
	if (!pm || !pg || !pc) {
		if (strstr(cat, "sweep."))
			bad("divergence: %s is a sweep with no --points mode, so its digest "
					"verdict cannot be decomposed and may be clearing points it does not "
					"describe",
					cat);
		else
			note("smokerun: %s is a single named case, not a sweep, so its digest "
					 "is its only point and the verdict is already exact\n",
					 cat);
		free(pm);
		free(pg);
		free(pc);
		return;
	}
	for (i = 0;; i++) {
		const char *lm = line_at(pm, i), *lg = line_at(pg, i), *lc = line_at(pc, i);
		if (!lm || !lg || !lc)
			break;
		n++;
		if (line_eq(lg, lc) && !line_eq(lm, lg))
			agree_mcc_differs++;
	}
	if (n == 0)
		bad("divergence: %s decomposed to zero points, so the decomposition "
				"measured nothing",
				cat);
	else if (agree_mcc_differs) {
		cat_addn(agree_mcc_differs, "div diverge-masked:%s", cat);
		note("MASKED       %s %ld of %ld points have the references AGREEING and "
				 "mcc differing, which the category's refs-disagree verdict clears\n",
				 cat, agree_mcc_differs, n);
	}
	free(pm);
	free(pg);
	free(pc);
}

static void divergence(void)
{
	char mout[1024], gout[1024], cout[1024], exe[1024], log[1024], tsv[1024];
	char *m, *g, *c;
	unsigned ms = 0;
	const char *gcc = ref_gcc();
	const char *clang = ref_clang();
	int have_g, have_c, n3 = 0, ndis = 0, nloud = 0, nrows = 0, nrefdis = 0;
	char *pm;
	char rdcat[32][192];
	int nrdcat = 0;

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
	{
		int fg = ref_is_clang(gcc), fc = ref_is_clang(clang);
		if (fg >= 0 && fg == fc) {
			note("smokerun: %s and %s are the same implementation family (%s), so "
					 "they cannot adjudicate each other -- dropping the second and "
					 "reporting every difference as diverge-one. A diverge-BOTH verdict "
					 "needs two independent references\n",
					 gcc, clang, fg ? "clang" : "gnu");
			clang = NULL;
		}
	}
	have_g = ref_build(gcc, "gcc", gout, "subject.c", SMK_REF_FLAGS, "--dump");
	have_c = clang &&
					 ref_build(clang, "clang", cout, "subject.c", SMK_REF_FLAGS, "--dump");
	if (!have_g && !have_c) {
		fprintf(stderr,
						"SKIP: smokerun found no working reference compiler (tried %s and "
						"%s); the UB divergence report needs one, and the goldens stand "
						"on their own without it\n",
						gcc, clang);
		g_skip = 1;
		return;
	}
	if (g_have_bank && (!have_g || !have_c)) {
		int i;
		for (i = 0; i < g_nbank; i++) {
			if (strstr(g_bank[i].key, "diverge-both:") ||
					strstr(g_bank[i].key, "diverge-refs:")) {
				fprintf(stderr,
								"SKIP: only one working reference (%s), but this key's bank "
								"holds diverge-both/diverge-refs rows taken under TWO "
								"independent references; a one-reference run collapses every "
								"verdict to diverge-one and grading it against a two-reference "
								"bank misreads hundreds of banked rows as new categories. "
								"Provision the second reference named in the bank header.\n",
								have_g ? "gcc" : "clang");
				g_skip = 1;
				return;
			}
		}
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
					if (dg && dc && !same_word(gv, cv)) {
						nrefdis++;
						cat_add("div diverge-refs:%s.%s", name, colname[ci]);
						{
							int q, seen = 0;
							for (q = 0; q < nrdcat; q++)
								if (!strcmp(rdcat[q], name))
									seen = 1;
							if (!seen && nrdcat < 32)
								snprintf(rdcat[nrdcat++], sizeof rdcat[0], "%s", name);
						}
						note("REFS-DISAGREE %s.%s mcc=%.32s gcc=%.32s clang=%.32s\n", name,
								 colname[ci], mv, gv, cv);
					} else if (dg && dc) {
						nloud++;
						cat_add("div diverge-both:%s.%s", name, colname[ci]);
						note("DIVERGE-BOTH %s.%s mcc=%.32s gcc=%.32s clang=%.32s\n", name,
								 colname[ci], mv, gv ? gv : "-", cv ? cv : "-");
					} else {
						cat_add("div diverge-one:%s.%s", name, colname[ci]);
						note("DIVERGE      %s.%s mcc=%.32s gcc=%.32s clang=%.32s\n", name,
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
	if (nrdcat && have_g && have_c) {
		char gexe[1024], cexe[1024];
		int q;
		ts_path(gexe, sizeof gexe, g_work, "subject-gcc.exe");
		ts_path(cexe, sizeof cexe, g_work, "subject-clang.exe");
		for (q = 0; q < nrdcat; q++)
			decompose(exe, gexe, cexe, rdcat[q]);
	}
	note("smokerun: divergence rows=%d comparable=%d differing=%d "
			 "mcc-differs-from-both=%d refs-disagree=%d\n",
			 nrows, n3, ndis, nloud, nrefdis);
	if (nrefdis)
		note("smokerun: %d case(s) where the two references disagree with each "
				 "other, so mcc matching neither is UB or implementation-defined, not a "
				 "defect -- pin mcc's answer and record the disagreement\n",
				 nrefdis);
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

typedef struct
{
	const char *name;
	int level;
	int replay;
	int ladder;
	int gpu;
	int jit;
	int optional;
	const char *what;
} Engine;

static const Engine g_engine[] = {
		{"ast", 0, 0, 0, 0, 0, 0,
		 "the AST constant evaluator, with no RIR replay under it"},
		{"rir", 0, 1, 0, 0, 0, 0,
		 "the RIR build-and-replay evaluator at the same -O0 the ast arm uses"},
		{"rir-o4", 4, 0, 0, 0, 0, 0,
		 "the RIR evaluator as -O4 reaches it, through the whole optimizer"},
		{"slice", 4, 0, 1, 0, 0, 0,
		 "the slice ladder, compiling and executing slices on the CPU"},
		{"gpu", 4, 0, 1, 1, 0, 1,
		 "the slice ladder, dispatching those slices to the device"},
		{"jit", 4, 0, 0, 0, 1, 0,
		 "the embedded JIT, re-baking the subject's own functions at run time"},
		{"jit-o1", 1, 0, 0, 0, 1, 0,
		 "the embedded JIT at -O1, the lowest level at which its engine boots"},
		{"jit-o2", 2, 0, 0, 0, 1, 0,
		 "the embedded JIT at -O2, the level the JIT cells default to"},
		{"jit-o3", 3, 0, 0, 0, 1, 0,
		 "the embedded JIT at -O3, one rung below the consolidated level"},
};

#define SMK_NENGINE ((int)(sizeof g_engine / sizeof g_engine[0]))

static int smk_nengine_req(void)
{
	int i, n = 0;
	for (i = 0; i < SMK_NENGINE; i++)
		if (!g_engine[i].optional)
			n++;
	return n;
}

static int g_eng_ran;
static int g_eng_ran_req;
static int g_eng_ran_opt;
static const char *g_eng_drop;
static long g_eng_rowcmp;
static int g_eng_kp_fired;
static int g_eng_kp_want;

static long tsv_used_rows(const char *path)
{
	char *txt = slurp(path);
	char *p = txt;
	long n = 0;
	while (p && *p) {
		char *eol = strchr(p, '\n');
		if (!strncmp(p, "used\t", 5))
			n++;
		if (!eol)
			break;
		p = eol + 1;
	}
	free(txt);
	return n;
}

static int eng_gpu_ready(const char *log, char *dev, size_t dn, long *disp)
{
	char *txt = slurp(log);
	const char *p = strstr(txt, "[ladder-gpu] tried=");
	int avail = 0;
	snprintf(dev, dn, "%s", "?");
	*disp = 0;
	if (!p) {
		free(txt);
		return 0;
	}
	sscanf(p, "[ladder-gpu] tried=%*d available=%d", &avail);
	{
		const char *d = strstr(p, "device=");
		const char *e = d ? strstr(d, " rungs=") : NULL;
		if (d && e) {
			size_t n = (size_t)(e - (d + 7));
			if (n >= dn)
				n = dn - 1;
			memcpy(dev, d + 7, n);
			dev[n] = 0;
		}
	}
	{
		const char *d = strstr(p, "dispatches=");
		if (d)
			*disp = strtol(d + 11, NULL, 10);
	}
	free(txt);
	return avail;
}

static int eng_prove(const Engine *e, const char *log, const char *tsv)
{
	char *txt = slurp(log);
	long used = tsv_used_rows(tsv);
	int ok = 1;

	if (!e->ladder) {
		int want_replay = e->replay || e->jit || e->level >= 1;
		if (want_replay && used <= 0) {
			bad("engine %s: the RIR census recorded no replayed evaluation at all, so "
					"this arm never reached %s and its agreement with the baseline is "
					"agreement with the baseline against itself",
					e->name, e->what);
			ok = 0;
		}
		if (!want_replay && used > 0) {
			bad("engine %s: the RIR census recorded %ld replayed evaluations, but this "
					"arm exists to measure %s; with replay leaking in it is a second copy "
					"of the rir arm and the AST evaluator stays unmeasured",
					e->name, used, e->what);
			ok = 0;
		}
	}
	if (e->ladder) {
		const char *p = strstr(txt, "[ladder-self] pairs=");
		long pairs = 0, cert = 0, differ = -1;
		if (!p) {
			bad("engine %s: the compile emitted no [ladder-self] census, so %s never "
					"ran and every row below was compared against the AST evaluator by "
					"the AST evaluator",
					e->name, e->what);
			ok = 0;
		} else {
			sscanf(p, "[ladder-self] pairs=%ld certified=%ld differ=%ld", &pairs, &cert,
						 &differ);
			if (cert <= 0) {
				bad("engine %s: the ladder certified 0 of %ld pairs, so no slice was "
						"executed and the arm measured nothing",
						e->name, pairs);
				ok = 0;
			}
			if (differ != 0) {
				bad("engine %s: the ladder disagreed with the AST evaluator on %ld of "
						"%ld self pairs; a slice that computes a different value from the "
						"tree it was cut from is a miscompile in waiting",
						e->name, differ, pairs);
				ok = 0;
			}
		}
	}
	free(txt);
	return ok;
}

static long eng_dump_diff(const char *ename, const char *bpath,
													const char *gpath, long *rows)
{
	char *b = slurp(bpath), *g = slurp(gpath);
	char *pb = b, *pg = g;
	long n = 0, r = 0, shown = 0;

	*rows = 0;
	while (*pb && *pg) {
		char *eb = strchr(pb, '\n'), *eg = strchr(pg, '\n');
		size_t lb = eb ? (size_t)(eb - pb) : strlen(pb);
		size_t lg = eg ? (size_t)(eg - pg) : strlen(pg);
		r++;
		if (lb != lg || memcmp(pb, pg, lb)) {
			n++;
			if (shown < 12) {
				shown++;
				bad("engine %s: dumped row %ld differs from the ast baseline\n"
						"       ast %.*s\n  %10s %.*s",
						ename, r, (int)lb, pb, ename, (int)lg, pg);
			}
		}
		if (!eb || !eg)
			break;
		pb = eb + 1;
		pg = eg + 1;
	}
	if (*pb || *pg) {
		n++;
		bad("engine %s: dumped %s rows than the ast baseline after %ld matching "
				"rows; a truncated dump compares only the prefix it managed to print",
				ename, *pg ? "more" : "fewer", r);
	}
	*rows = r;
	free(b);
	free(g);
	return n;
}

static void eng_no_baseline(void)
{
	bad("engine ast is the baseline every other engine is compared against, and "
			"it did not produce one; the arm stops here rather than reporting "
			"agreement against an empty file");
}

static void engines_run(int known_pos)
{
	char bdump[1024] = "", bdigest[32] = "";
	long brows = 0;
	int i;

	note("smokerun: engine parity, %d engines, ast at -O0 is the baseline\n",
			 SMK_NENGINE);

	for (i = 0; i < SMK_NENGINE; i++) {
		const Engine *e = &g_engine[i];
		char exe[1024], log[1024], tsv[1024], dump[1024], out[1024], err[1024];
		char dg[32] = "";
		long checks = 0, sweep = 0, msweep = 0, fchecks = 0, failures = 0;
		long ndiff = 0, rows = 0;
		unsigned ms = 0;
		char *txt;
		int st;

		if (g_eng_drop && !strcmp(g_eng_drop, e->name)) {
			note("  %-7s DROPPED by --engines-drop\n", e->name);
			continue;
		}

		ts_path(exe, sizeof exe, g_work, "eng-%s.exe", e->name);
		ts_path(log, sizeof log, g_work, "eng-%s.log", e->name);
		ts_path(tsv, sizeof tsv, g_work, "eng-%s.tsv", e->name);
		ts_path(dump, sizeof dump, g_work, "eng-%s.dump", e->name);
		ts_path(out, sizeof out, g_work, "eng-%s.out", e->name);
		ts_path(err, sizeof err, g_work, "eng-%s.err", e->name);

		g_force_replay = e->replay;
		st = compile_subject(e->level, exe, log, tsv, &ms, e->gpu, e->ladder,
												 e->jit);
		g_force_replay = 1;
		if (!st) {
			char *l = slurp(log);
			bad("engine %s: the -O%d compile failed, so %s is unmeasured:\n%s", e->name,
					e->level, e->what, l);
			free(l);
			if (i == 0) {
				eng_no_baseline();
				return;
			}
			continue;
		}

		if (e->gpu) {
			char dev[128];
			long disp = 0;
			int avail = eng_gpu_ready(log, dev, sizeof dev, &disp);
			if (!avail || disp <= 0) {
				if (g_require_device) {
					bad("engine %s: --require-device, but the ladder reports available=%d "
							"dispatches=%ld (device=%s)",
							e->name, avail, disp, dev);
					continue;
				}
				note("  %-7s SKIP no usable device (available=%d dispatches=%ld "
						 "device=%s)\n",
						 e->name, avail, disp, dev);
				continue;
			}
			note("  %-7s device=%s dispatches=%ld\n", e->name, dev, disp);
		}

		if (!eng_prove(e, log, tsv)) {
			if (i == 0) {
				eng_no_baseline();
				return;
			}
			continue;
		}

		if (e->jit) {
			/* Force the subject to boot its embedded JIT regardless of the
			   build's MCC_CONFIG_JIT default: the jit-parity arm must actually
			   run the JIT (its whole point is to catch "an AOT run wearing the
			   jit arm's name").  A build configured MCC_CONFIG_JIT=OFF bakes
			   def_on=0 into the subject, so without MCC_JIT=1 the boot-swap
			   silently no-ops and this arm degrades to AOT (the win-PE failure
			   mode; the engine itself is fine).  MCC_JIT=1 overrides def_on. */
			host_setenv("MCC_JIT", "1");
			host_setenv("MCC_JIT_VERBOSE", "1");
			host_setenv("MCC_JIT_HOT_CALLS", "1");
		}
		run_subject_err(exe, known_pos && i ? "--dump --poison" : "--dump", dump,
										err);
		st = run_subject_err(exe, "", out, err);
		if (e->jit) {
			host_unsetenv("MCC_JIT");
			host_unsetenv("MCC_JIT_VERBOSE");
			host_unsetenv("MCC_JIT_HOT_CALLS");
			txt = slurp(err);
			if (!strstr(txt, "swapped")) {
				bad("engine %s: the --embed-jit binary ran without swapping a single "
						"function to JIT-baked code, so it is an AOT run wearing the jit "
						"arm's name and %s stayed unmeasured",
						e->name, e->what);
				free(txt);
				continue;
			}
			free(txt);
		}

		txt = slurp(out);
		if (!parse_summary(txt, &checks, &sweep, &msweep, &fchecks, &failures, dg,
											 sizeof dg)) {
			bad("engine %s: the run printed no summary line:\n%s", e->name, txt);
			free(txt);
			if (i == 0) {
				eng_no_baseline();
				return;
			}
			continue;
		}
		if (failures || !exited_zero(st)) {
			bad("engine %s: the run reported %ld failures (exit %d):\n%s", e->name,
					failures, exit_code(st), txt);
		}
		free(txt);

		g_eng_ran++;
		if (e->optional)
			g_eng_ran_opt++;
		else
			g_eng_ran_req++;
		g_checks_total += checks;
		g_cases_total += checks + sweep + msweep + fchecks;

		if (i == 0) {
			snprintf(bdump, sizeof bdump, "%s", dump);
			snprintf(bdigest, sizeof bdigest, "%s", dg);
			{
				char *bt = slurp(dump);
				char *p = bt;
				while (*p) {
					if (*p == '\n')
						brows++;
					p++;
				}
				free(bt);
			}
			if (brows <= 0) {
				bad("engine %s: the baseline dumped %ld rows, so every engine below "
						"would be compared against nothing",
						e->name, brows);
				return;
			}
			note("  %-7s -O%d %5u ms  baseline rows=%ld digest=%s\n", e->name, e->level,
					 ms, brows, dg);
			continue;
		}

		ndiff = eng_dump_diff(e->name, bdump, dump, &rows);
		g_eng_rowcmp += rows;
		if (rows < brows)
			bad("engine %s: only %ld of the baseline's %ld rows were comparable",
					e->name, rows, brows);
		if (strcmp(bdigest, dg))
			bad("engine %s: sweep digest %s differs from the ast baseline's %s; %s "
					"computes a different value somewhere the row dump does not reach",
					e->name, dg, bdigest, e->what);
		if (known_pos) {
			g_eng_kp_want++;
			if (ndiff > 0)
				g_eng_kp_fired++;
			else
				bad("engine %s: --known-positive poisoned its dump and the row "
						"comparison still reported agreement; this engine's rows are not "
						"being compared at all",
						e->name);
		}
		note("  %-7s -O%d %5u ms  rows=%ld differing=%ld digest=%s\n", e->name,
				 e->level, ms, rows, ndiff, dg);
	}
}

static void usage(void)
{
	fprintf(stderr,
					"usage: smokerun --mcc PATH --srcdir DIR --work DIR [--min-cases N]\n"
					"                [--min-passes N] [--min-strats N] [--max-level N]\n"
					"                [--strat-dark]\n"
					"                [--budget-ms N]\n"
					"                [--deadline-ms N]\n"
					"                [--bank FILE] [--rebank] [--known-positive]\n"
					"                [--divergence] [--device] [--require-device]\n"
					"                [--engines] [--min-engines N] [--engines-drop NAME]\n"
					"                [-v]\n");
}

int main(int argc, char **argv)
{
	char bankpath[1024] = "";
	int maxlevel = smk_maxlevel();
	int i, do_div = 0, do_dev = 0, known_pos = 0, do_slice = 0, do_eng = 0;
	int do_dark = 0;
	long min_engines = 0;
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
		else if (!strcmp(argv[i], "--min-passes") && i + 1 < argc)
			g_min_passes = strtol(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--min-strats") && i + 1 < argc)
			g_min_strats = strtol(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--max-level") && i + 1 < argc)
			maxlevel = (int)strtol(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--strat-dark"))
			do_dark = 1;
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
		else if (!strcmp(argv[i], "--engines"))
			do_eng = 1;
		else if (!strcmp(argv[i], "--min-engines") && i + 1 < argc)
			min_engines = strtol(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--engines-drop") && i + 1 < argc)
			g_eng_drop = argv[++i];
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
	if (!bankpath[0]) {
		char *probe;
		snprintf(bankpath, sizeof bankpath, "%s/tests/smoke/bails-%s.txt", g_srcdir,
						 SMK_TARGET_KEY);
		probe = ts_read_file(bankpath, NULL);
		if (probe)
			free(probe);
		else
			snprintf(bankpath, sizeof bankpath, "%s/tests/smoke/bails.txt", g_srcdir);
	}
	note("smokerun: bank %s\n", bankpath);

	if (maxlevel < 0 || maxlevel > smk_maxlevel()) {
		if (maxlevel == MCC_OPT_SEARCH_LEVEL) {
			fprintf(stderr,
							"smokerun: --max-level %d is the search tier, not a rung of the "
							"-O0..-O%d ladder, and -O5..-O%d are a hard error -- the level "
							"sweep cannot walk to it. Use --strat-dark, which compiles the "
							"subject once at -O%d and banks the strategies that stay dark\n",
							MCC_OPT_SEARCH_LEVEL, smk_maxlevel(), MCC_OPT_SEARCH_LEVEL - 1,
							MCC_OPT_SEARCH_LEVEL);
			return 1;
		}
		if (maxlevel > smk_maxlevel())
			fprintf(stderr,
							"smokerun: --max-level %d clamped to %d, the top of the ladder\n",
							maxlevel, smk_maxlevel());
		maxlevel = smk_maxlevel();
	}

	if (do_div) {
		bank_load(bankpath);
		divergence();
		if (g_skip)
			return TS_SKIP_CODE;
		own("diverge-");
		ratchet(bankpath);
		return g_fail ? 1 : 0;
	}

	if (do_eng) {
		engines_run(known_pos);
		note("smokerun: engines ran=%d of %d (required %d of %d, optional %d), row "
				 "comparisons=%ld, value-cases=%ld, failures=%d\n",
				 g_eng_ran, SMK_NENGINE, g_eng_ran_req, smk_nengine_req(), g_eng_ran_opt,
				 g_eng_rowcmp, g_cases_total, g_fail);
		if (g_eng_ran_req < min_engines) {
			fprintf(stderr,
							"smokerun: %d required engine(s) ran, below the --min-engines %ld "
							"floor -- an arm that silently stops reaching an engine reads "
							"exactly like one that agrees with it. The floor counts only "
							"engines that cannot skip on their own (%d of %d registered); the "
							"device engine is reported separately as optional=%d, because a "
							"floor over the total lets a present device mask a missing "
							"non-device engine\n",
							g_eng_ran_req, min_engines, smk_nengine_req(), SMK_NENGINE,
							g_eng_ran_opt);
			return 1;
		}
		if (g_eng_rowcmp <= 0) {
			fprintf(stderr, "smokerun: the engine arm compared zero rows, so agreement "
											"between engines was never established\n");
			return 1;
		}
		if (known_pos) {
			if (!g_eng_kp_want) {
				fprintf(stderr, "smokerun: --known-positive had no non-baseline engine "
												"to poison, so it proved nothing\n");
				return 1;
			}
			if (g_eng_kp_fired < g_eng_kp_want) {
				fprintf(stderr,
								"smokerun: --known-positive poisoned %d engine dump(s) and only "
								"%d reported a differing row; the row comparison is blind\n",
								g_eng_kp_want, g_eng_kp_fired);
				return 1;
			}
			note("smokerun: engine known-positive fired on %d of %d compared engines, "
					 "as it must\n",
					 g_eng_kp_fired, g_eng_kp_want);
			return 0;
		}
		if (g_cases_total < g_min_cases) {
			fprintf(stderr,
							"smokerun: the engine arm examined %ld value cases, below the "
							"--min-cases %ld floor\n",
							g_cases_total, g_min_cases);
			return 1;
		}
		return g_fail ? 1 : 0;
	}

	if (do_dark) {
		bank_load(bankpath);
		strat_dark_census();
		own("strat-dark");
		ratchet(bankpath);
		return g_fail ? 1 : 0;
	}

	if (do_slice) {
		bank_load(bankpath);
		slice_census(smk_maxlevel());
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
		own("dev-");
		ratchet(bankpath);
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

	passes_run(maxlevel, d0);
	rowlev_report(maxlevel);
	if (g_min_strats > 0)
		strat_census(maxlevel);

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
	note("smokerun: passes=%d/%d pass-checks=%ld pass-failures=%d\n", g_pass_rows,
			 SMK_NPASS, g_pass_checks, g_pass_fails);

	if (sm_fork_retries)
		note("smokerun: %u fork retries under host load; results are unaffected "
				 "but the box was contended\n",
				 sm_fork_retries);

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
	if (g_pass_rows < SMK_NPASS) {
		fprintf(stderr,
						"smokerun: %d of %d compile passes ran; a pass that never "
						"executed must not read as a pass\n",
						g_pass_rows, SMK_NPASS);
		return 1;
	}
	if (g_pass_checks < g_min_passes) {
		fprintf(stderr,
						"smokerun: compile passes executed %ld checks, below the "
						"--min-passes %ld floor -- a silently-empty pass must not read as "
						"a pass\n",
						g_pass_checks, g_min_passes);
		return 1;
	}
	if (g_min_strats > 0 && !known_pos) {
		if (g_strat_total < g_min_strats) {
			fprintf(stderr,
							"smokerun: the [strategy] record names %d strategies, below the "
							"--min-strats %ld floor -- the table shrank and the coverage "
							"claim shrank silently with it\n",
							g_strat_total, g_min_strats);
			return 1;
		}
		if (g_strat_fired < g_min_strats) {
			fprintf(stderr,
							"smokerun: %d of %d strategies fired at -O%d, below the "
							"--min-strats %ld floor -- a subject that stops reaching a "
							"strategy must not read as coverage\n",
							g_strat_fired, g_strat_total, maxlevel, g_min_strats);
			return 1;
		}
		note("smokerun: %d of %d strategies fired, at or above the --min-strats "
				 "%ld floor\n",
				 g_strat_fired, g_strat_total, g_min_strats);
	}

	if (known_pos) {
		int nfix = 0, i;
		for (i = 0; i < SMK_NPASS; i++)
			if (g_pass[i].want)
				nfix++;
		if (!g_fail) {
			fprintf(stderr, "smokerun: --known-positive poisoned an expectation and "
											"every level still passed; the suite is blind\n");
			return 1;
		}
		if (g_pass_fails < nfix) {
			fprintf(stderr,
							"smokerun: --known-positive poisoned every fixture pass but only "
							"%d of %d reported a failure; the pass machinery is blind\n",
							g_pass_fails, nfix);
			return 1;
		}
		note("smokerun: known-positive fired %d failure(s) including %d from the "
				 "compile passes, as it must\n",
				 g_fail, g_pass_fails);
		return 0;
	}
	return g_fail ? 1 : 0;
}
