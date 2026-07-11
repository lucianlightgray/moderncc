#include "toolsupport.h"

#include <math.h>

#if MCC_HOST_POSIX
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#else
#include <windows.h>
#include <psapi.h>
#endif

#if MCC_HOST_DARWIN
#include <sys/sysctl.h>
#endif

#if defined(_M_X64) || defined(_M_IX86)
#include <intrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

#define REPEATS_DEFAULT 5
#define MAXREP 64
#define MAXCC 32
#define MAXWL 8

enum { STYLE_GCC,
			 STYLE_CL };

struct compiler {
	const char *key;
	char path[4096];
	int style;
	const char *ccmacro;
	char version[128];
	const char *opt;
};

struct workload {
	const char *key;
	char src[4096];
	const char *incs[16];
	const char *defs[16];
	int needs_ccmacro;
	int gnu_c;
	int funcs;
	int lines;
	int have_counts;
};

struct meas {
	int ok;
	unsigned wall_ms;
	long cpu_ms;
	long peak_kb;
	long long objsize;
};

struct cell {
	struct meas m;
	int nwall;
	int ncpu;
	double wall_s[MAXREP];
	double cpu_s[MAXREP];
};

static double vec_mean(const double *x, int n) {
	double s = 0;
	int i;
	for (i = 0; i < n; i++)
		s += x[i];
	return n ? s / n : 0;
}

static double vec_var(const double *x, int n) {
	double m = vec_mean(x, n), s = 0;
	int i;
	if (n < 2)
		return 0;
	for (i = 0; i < n; i++)
		s += (x[i] - m) * (x[i] - m);
	return s / (n - 1);
}

static double t_crit_05(double df) {
	static const double T[30] = {
			12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365, 2.306, 2.262, 2.228,
			2.201, 2.179, 2.160, 2.145, 2.131, 2.120, 2.110, 2.101, 2.093, 2.086,
			2.080, 2.074, 2.069, 2.064, 2.060, 2.056, 2.052, 2.048, 2.045, 2.042};
	int i = (int)df;
	if (i < 1)
		i = 1;
	if (i <= 30)
		return T[i - 1];
	if (i < 40)
		return 2.042;
	if (i < 60)
		return 2.021;
	if (i < 120)
		return 2.000;
	return 1.980;
}

static int welch_sig(const double *a, int na, const double *b, int nb,
										 int *sig) {
	double va, vb, sea, seb, se2, t, df;
	if (na < 2 || nb < 2)
		return 0;
	va = vec_var(a, na);
	vb = vec_var(b, nb);
	sea = va / na;
	seb = vb / nb;
	se2 = sea + seb;
	if (se2 <= 0) {
		*sig = vec_mean(a, na) != vec_mean(b, nb);
		return 1;
	}
	t = (vec_mean(a, na) - vec_mean(b, nb)) / sqrt(se2);
	df = se2 * se2 / (sea * sea / (na - 1) + seb * seb / (nb - 1));
	*sig = fabs(t) > t_crit_05(df);
	return 1;
}

#define BOOT_B 2000
#define BOOT_SEED 0x9e3779b9u

static uint32_t boot_xs32(uint32_t *s) {
	uint32_t x = *s;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*s = x;
	return x;
}

static int cmp_double(const void *pa, const void *pb) {
	double a = *(const double *)pa, b = *(const double *)pb;
	return a < b ? -1 : a > b ? 1 : 0;
}

static int boot_ci(const double *x, int n, double *lo, double *hi) {
	static double means[BOOT_B];
	uint32_t s = BOOT_SEED;
	int b, i;
	if (n < 2)
		return 0;
	for (b = 0; b < BOOT_B; b++) {
		double sum = 0;
		for (i = 0; i < n; i++)
			sum += x[boot_xs32(&s) % (uint32_t)n];
		means[b] = sum / n;
	}
	qsort(means, BOOT_B, sizeof *means, cmp_double);
	*lo = means[(int)(0.025 * BOOT_B)];
	*hi = means[(int)(0.975 * BOOT_B)];
	return 1;
}

static double cohens_d(const double *a, int na, const double *b, int nb,
											 const char **label) {
	double va = vec_var(a, na), vb = vec_var(b, nb);
	double sp2 = ((na - 1) * va + (nb - 1) * vb) / (na + nb - 2);
	double sp = sqrt(sp2);
	double d = sp > 0 ? (vec_mean(a, na) - vec_mean(b, nb)) / sp : 0;
	double ad = fabs(d);
	*label = ad < 0.2 ? "negligible" : ad < 0.5 ? "small"
										: ad < 0.8	? "medium"
																: "large";
	return d;
}

static int g_stats;

static int cred_interval(const double *x, int n, double *lo, double *hi) {
	double m, se;
	if (n < 2)
		return 0;
	m = vec_mean(x, n);
	se = sqrt(vec_var(x, n) / n);
	*lo = m - t_crit_05((double)(n - 1)) * se;
	*hi = m + t_crit_05((double)(n - 1)) * se;
	return 1;
}

static int bayes_factor(const double *a, int na, const double *b, int nb,
												double *bf10, const char **label) {
	double va, vb, sp2, sp, t, n, df, dbic;
	if (na < 2 || nb < 2)
		return 0;
	va = vec_var(a, na);
	vb = vec_var(b, nb);
	df = na + nb - 2;
	sp2 = ((na - 1) * va + (nb - 1) * vb) / df;
	sp = sqrt(sp2);
	if (sp <= 0)
		return 0;
	t = (vec_mean(a, na) - vec_mean(b, nb)) / (sp * sqrt(1.0 / na + 1.0 / nb));
	n = na + nb;
	dbic = n * log(1.0 + t * t / df) - log(n);
	*bf10 = exp(dbic / 2.0);
	*label = *bf10 < 1.0 / 10 ? "strong H0"
					: *bf10 < 1.0 / 3 ? "subst. H0"
					: *bf10 < 3			  ? "weak"
					: *bf10 < 10			? "subst. H1"
														: "strong H1";
	return 1;
}

static double bt_gammln(double xx) {
	static const double cof[6] = {76.18009172947146,	 -86.50532032941677,
																24.01409824083091,	 -1.231739572450155,
																0.1208650973866179e-2, -0.5395239384953e-5};
	double x = xx, y = xx, tmp, ser = 1.000000000190015;
	int j;
	tmp = x + 5.5;
	tmp -= (x + 0.5) * log(tmp);
	for (j = 0; j < 6; j++)
		ser += cof[j] / ++y;
	return -tmp + log(2.5066282746310005 * ser / x);
}

static double bt_betacf(double a, double b, double x) {
	double aa, c, d, del, h, qab = a + b, qap = a + 1, qam = a - 1;
	int m;
	c = 1;
	d = 1 - qab * x / qap;
	if (fabs(d) < 1e-30)
		d = 1e-30;
	d = 1 / d;
	h = d;
	for (m = 1; m <= 200; m++) {
		int m2 = 2 * m;
		aa = m * (b - m) * x / ((qam + m2) * (a + m2));
		d = 1 + aa * d;
		if (fabs(d) < 1e-30)
			d = 1e-30;
		c = 1 + aa / c;
		if (fabs(c) < 1e-30)
			c = 1e-30;
		d = 1 / d;
		h *= d * c;
		aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
		d = 1 + aa * d;
		if (fabs(d) < 1e-30)
			d = 1e-30;
		c = 1 + aa / c;
		if (fabs(c) < 1e-30)
			c = 1e-30;
		d = 1 / d;
		del = d * c;
		h *= del;
		if (fabs(del - 1) < 3e-7)
			break;
	}
	return h;
}

static double bt_betai(double a, double b, double x) {
	double bt;
	if (x <= 0)
		return 0;
	if (x >= 1)
		return 1;
	bt = exp(bt_gammln(a + b) - bt_gammln(a) - bt_gammln(b) + a * log(x) +
					 b * log(1 - x));
	if (x < (a + 1) / (a + b + 2))
		return bt * bt_betacf(a, b, x) / a;
	return 1 - bt * bt_betacf(b, a, 1 - x) / b;
}

static int anova_oneway(double *const *grp, const int *ng, int G, double *F,
												int *df1, int *df2, double *p) {
	double grand = 0, ssb = 0, ssw = 0;
	int g, i, N = 0, used = 0;
	for (g = 0; g < G; g++) {
		if (ng[g] < 2)
			continue;
		used++;
		N += ng[g];
		for (i = 0; i < ng[g]; i++)
			grand += grp[g][i];
	}
	if (used < 2 || N <= used)
		return 0;
	grand /= N;
	for (g = 0; g < G; g++) {
		double mg;
		if (ng[g] < 2)
			continue;
		mg = vec_mean(grp[g], ng[g]);
		ssb += ng[g] * (mg - grand) * (mg - grand);
		for (i = 0; i < ng[g]; i++)
			ssw += (grp[g][i] - mg) * (grp[g][i] - mg);
	}
	*df1 = used - 1;
	*df2 = N - used;
	if (ssw <= 0) {
		*F = ssb > 0 ? 1e30 : 0;
		*p = ssb > 0 ? 0 : 1;
		return 1;
	}
	*F = (ssb / *df1) / (ssw / *df2);
	*p = bt_betai(*df2 / 2.0, *df1 / 2.0, *df2 / (*df2 + *df1 * *F));
	return 1;
}

static void write_stats(FILE *f, const struct compiler *ccs, int nccs,
												struct cell *cells) {
	double *grp[MAXCC];
	int ng[MAXCC], G = 0, i, j;
	double F, p;
	int df1, df2;
	fprintf(f, "  stats (--stats): 95%% credible interval, BIC-approx Bayes"
						 " factor vs same-[O] reference\n");
	for (i = 0; i < nccs; i++) {
		double lo, hi, bf;
		const char *blab = "";
		char cri[40];
		if (!cells[i].m.ok || !cells[i].nwall)
			continue;
		if (cred_interval(cells[i].wall_s, cells[i].nwall, &lo, &hi))
			snprintf(cri, sizeof cri, "[%.1f,%.1f]", lo * 1000.0, hi * 1000.0);
		else
			snprintf(cri, sizeof cri, "%s", "n/a");
		fprintf(f, "    %-4s %-8s CrI(ms)=%-15s", ccs[i].opt ? ccs[i].opt : "def",
						ccs[i].key, cri);
		if (!strcmp(ccs[i].key, "mcc")) {
			const char *og = ccs[i].opt ? ccs[i].opt + 1 : "";
			for (j = i + 1; j < nccs; j++) {
				const char *jg = ccs[j].opt ? ccs[j].opt + 1 : "";
				if (strcmp(jg, og))
					break;
				if (!strcmp(ccs[j].key, "mcc") || !cells[j].m.ok || !cells[j].nwall)
					continue;
				if (bayes_factor(cells[i].wall_s, cells[i].nwall, cells[j].wall_s,
												 cells[j].nwall, &bf, &blab))
					fprintf(f, "  BF10=%.2g vs %s (%s)", bf, ccs[j].key, blab);
			}
		}
		fprintf(f, "\n");
		if (!strcmp(ccs[i].key, "mcc") && cells[i].nwall >= 2) {
			grp[G] = cells[i].wall_s;
			ng[G++] = cells[i].nwall;
		}
	}
	if (G >= 2 && anova_oneway(grp, ng, G, &F, &df1, &df2, &p))
		fprintf(f, "    ANOVA across %d mcc -O configs: F(%d,%d)=%.3f  p=%.4f  %s\n",
						G, df1, df2, F, p, p < 0.05 ? "*" : "ns");
}

static struct meas measure_once(const char *const *argv) {
	struct meas m;
	unsigned t0;
	m.ok = 0;
	m.wall_ms = 0;
	m.cpu_ms = -1;
	m.peak_kb = -1;
	m.objsize = -1;
	t0 = host_clock_ms();
#if MCC_HOST_POSIX
	{
		pid_t pid = fork();
		int st;
		struct rusage ru;
		if (pid < 0)
			return m;
		if (pid == 0) {
			int dn = open("/dev/null", O_WRONLY);
			if (dn >= 0) {
				dup2(dn, 1);
				dup2(dn, 2);
			}
			execvp(argv[0], (char *const *)argv);
			_exit(127);
		}
		memset(&ru, 0, sizeof ru);
		if (wait4(pid, &st, 0, &ru) < 0)
			return m;
		m.wall_ms = host_clock_ms() - t0;
		m.cpu_ms = (long)ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000 +
							 (long)ru.ru_stime.tv_sec * 1000 + ru.ru_stime.tv_usec / 1000;
		m.peak_kb = MCC_HOST_DARWIN ? ru.ru_maxrss / 1024 : ru.ru_maxrss;
		m.ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
	}
#else
	{
		char cmd[32768];
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		SECURITY_ATTRIBUTES sa;
		HANDLE nul;
		DWORD ec = 1;
		size_t off = 0;
		int i;
		for (i = 0; argv[i]; i++) {
			const char *a = argv[i];
			int q = (*a == 0) || strpbrk(a, " \t\"") != NULL;
			if (off + strlen(a) + 4 >= sizeof cmd)
				break;
			if (i)
				cmd[off++] = ' ';
			if (q)
				cmd[off++] = '"';
			for (; *a; a++) {
				if (*a == '"')
					cmd[off++] = '\\';
				cmd[off++] = *a;
			}
			if (q)
				cmd[off++] = '"';
		}
		cmd[off] = 0;
		memset(&sa, 0, sizeof sa);
		sa.nLength = sizeof sa;
		sa.bInheritHandle = TRUE;
		nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
											&sa, OPEN_EXISTING, 0, NULL);
		memset(&si, 0, sizeof si);
		si.cb = sizeof si;
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = INVALID_HANDLE_VALUE;
		si.hStdOutput = nul;
		si.hStdError = nul;
		memset(&pi, 0, sizeof pi);
		if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
			if (nul != INVALID_HANDLE_VALUE)
				CloseHandle(nul);
			return m;
		}
		WaitForSingleObject(pi.hProcess, INFINITE);
		m.wall_ms = host_clock_ms() - t0;
		GetExitCodeProcess(pi.hProcess, &ec);
		{
			FILETIME cre, ex, kt, ut;
			if (GetProcessTimes(pi.hProcess, &cre, &ex, &kt, &ut)) {
				unsigned long long k = ((unsigned long long)kt.dwHighDateTime << 32) |
															 kt.dwLowDateTime;
				unsigned long long u = ((unsigned long long)ut.dwHighDateTime << 32) |
															 ut.dwLowDateTime;
				m.cpu_ms = (long)((k + u) / 10000);
			}
		}
		{
			PROCESS_MEMORY_COUNTERS pmc;
			memset(&pmc, 0, sizeof pmc);
			if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof pmc))
				m.peak_kb = (long)(pmc.PeakWorkingSetSize / 1024);
		}
		if (nul != INVALID_HANDLE_VALUE)
			CloseHandle(nul);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		m.ok = (ec == 0);
	}
#endif
	return m;
}

static void build_cmd(Argv *v, const struct compiler *cc,
											const struct workload *wl, const char *obj) {
	int i;
	char buf[4160];
	ts_arg(v, cc->path);
	if (cc->style == STYLE_CL) {
		ts_arg(v, "/nologo");
		ts_arg(v, "/c");
		ts_arg(v, wl->src);
		snprintf(buf, sizeof buf, "/Fo%s", obj);
		ts_arg(v, strdup(buf));
		for (i = 0; wl->incs[i]; i++) {
			snprintf(buf, sizeof buf, "/I%s", wl->incs[i]);
			ts_arg(v, strdup(buf));
		}
		for (i = 0; wl->defs[i]; i++) {
			snprintf(buf, sizeof buf, "/D%s", wl->defs[i]);
			ts_arg(v, strdup(buf));
		}
		if (wl->needs_ccmacro && cc->ccmacro) {
			snprintf(buf, sizeof buf, "/DCC_NAME=%s", cc->ccmacro);
			ts_arg(v, strdup(buf));
		}
	} else {
		ts_arg(v, "-c");
		ts_arg(v, wl->src);
		ts_arg(v, "-o");
		ts_arg(v, obj);
		for (i = 0; wl->incs[i]; i++) {
			snprintf(buf, sizeof buf, "-I%s", wl->incs[i]);
			ts_arg(v, strdup(buf));
		}
		for (i = 0; wl->defs[i]; i++) {
			snprintf(buf, sizeof buf, "-D%s", wl->defs[i]);
			ts_arg(v, strdup(buf));
		}
		if (wl->needs_ccmacro && cc->ccmacro) {
			snprintf(buf, sizeof buf, "-DCC_NAME=%s", cc->ccmacro);
			ts_arg(v, strdup(buf));
		}
	}
	if (cc->opt)
		ts_arg(v, cc->opt);
}

static void bench_one(const struct compiler *cc, const struct workload *wl,
											int repeats, struct cell *c) {
	struct meas best;
	char obj[4096];
	int r;
	memset(c, 0, sizeof *c);
	best.ok = 0;
	best.wall_ms = 0;
	best.cpu_ms = -1;
	best.peak_kb = -1;
	best.objsize = -1;
	c->m = best;
	ts_path(obj, sizeof obj, ".", "mccbench-%s-%s.o", cc->key, wl->key);
	if (wl->needs_ccmacro && !cc->ccmacro)
		return;
	for (r = 0; r < repeats; r++) {
		Argv v = {{0}, 0};
		struct meas m;
		build_cmd(&v, cc, wl, obj);
		m = measure_once(ts_argz(&v));
		if (!m.ok) {
			c->m = m;
			c->nwall = 0;
			c->ncpu = 0;
			return;
		}
		c->wall_s[c->nwall++] = (double)m.wall_ms / 1000.0;
		if (m.cpu_ms >= 0)
			c->cpu_s[c->ncpu++] = (double)m.cpu_ms / 1000.0;
		if (!best.ok || m.wall_ms < best.wall_ms)
			best = m;
	}
	if (best.ok) {
		int isd;
		long long sz;
		if (host_stat(obj, &isd, &sz, NULL) == 0 && !isd)
			best.objsize = sz;
		remove(obj);
	}
	c->m = best;
}

static void count_with_mcc(const struct compiler *mcc, struct workload *wl) {
	Argv v = {{0}, 0};
	char obj[4096], *err = NULL;
	HostSpawnOpts o;
	const char *p;
	wl->have_counts = 0;
	ts_path(obj, sizeof obj, ".", "mccbench-count-%s.o", wl->key);
	build_cmd(&v, mcc, wl, obj);
	{
		Argv v2 = {{0}, 0};
		int i;
		ts_arg(&v2, v.a[0]);
		ts_arg(&v2, "-bench");
		for (i = 1; i < v.n; i++)
			ts_arg(&v2, v.a[i]);
		v = v2;
	}
	memset(&o, 0, sizeof o);
	o.stderr_buf = &err;
	if (host_spawn_ex(ts_argz(&v), &o) == 0 && err) {
		if ((p = strstr(err, " functions"))) {
			const char *q = p;
			while (q > err && q[-1] != ',')
				q--;
			wl->funcs = atoi(q);
			wl->have_counts = 1;
		}
		if ((p = strstr(err, " lines"))) {
			const char *q = p;
			while (q > err && q[-1] != ',')
				q--;
			wl->lines = atoi(q);
		}
	}
	free(err);
	remove(obj);
}

static void fmt_stat(char *b, int n, double v, int have) {
	if (!have)
		snprintf(b, n, "%s", "n/a");
	else
		snprintf(b, n, "%.3f", v);
}

static void vs_ref(char *b, int n, const struct compiler *ccs, int nccs,
									 struct cell *cells, int i) {
	const char *og = ccs[i].opt ? ccs[i].opt + 1 : "";
	double wmean = vec_mean(cells[i].wall_s, cells[i].nwall);
	int j;
	b[0] = 0;
	if (strcmp(ccs[i].key, "mcc") || !cells[i].m.ok || !cells[i].nwall)
		return;
	for (j = i + 1; j < nccs; j++) {
		const char *jg = ccs[j].opt ? ccs[j].opt + 1 : "";
		struct cell *rc = &cells[j];
		const char *dlab = "";
		double rmean, d;
		int sig;
		if (strcmp(jg, og))
			return;
		if (!strcmp(ccs[j].key, "mcc"))
			continue;
		if (!rc->m.ok || !rc->nwall || (rmean = vec_mean(rc->wall_s, rc->nwall)) <= 0)
			return;
		d = cohens_d(cells[i].wall_s, cells[i].nwall, rc->wall_s, rc->nwall, &dlab);
		snprintf(b, n, "%+.1f%% vs %s%s  d=%+.2f %s",
						 (wmean - rmean) * 100.0 / rmean, ccs[j].key,
						 welch_sig(cells[i].wall_s, cells[i].nwall, rc->wall_s, rc->nwall,
											 &sig)
								 ? (sig ? " *" : " ns")
								 : "",
						 (cells[i].nwall > 1 && rc->nwall > 1) ? d : 0.0, dlab);
		return;
	}
}

static void write_table(FILE *f, const struct compiler *ccs, int nccs,
												struct workload *wl, int repeats) {
	int i;
	char cpu[16], csd[16], wall[16], wsd[16], ci[32], vs[96];
	struct cell *cells = calloc((size_t)nccs, sizeof *cells);
	if (!cells)
		return;
	for (i = 0; i < nccs; i++)
		if (!(wl->gnu_c && ccs[i].style == STYLE_CL))
			bench_one(&ccs[i], wl, repeats, &cells[i]);
	if (wl->have_counts)
		fprintf(f, "\nWorkload: %s  (%d lines, %d functions, n=%d runs)\n",
						wl->key, wl->lines, wl->funcs, repeats);
	else
		fprintf(f, "\nWorkload: %s  (n=%d runs)\n", wl->key, repeats);
	fprintf(f, "  %-8s %8s %7s %10s %8s %9s %8s %7s %-17s  %s\n",
					"compiler", "cpu(s)", "sd", "funcs/s", "obj(KB)", "peak(MB)",
					"wall(s)", "sd", "95%CI(ms)", "vs-ref");
	for (i = 0; i < nccs; i++) {
		struct cell *c = &cells[i];
		const char *pg = i ? (ccs[i - 1].opt ? ccs[i - 1].opt + 1 : "") : NULL;
		const char *og = ccs[i].opt ? ccs[i].opt + 1 : "";
		double wmean;
		if (!pg || strcmp(pg, og))
			fprintf(f, "\n  [%s]\n", ccs[i].opt ? ccs[i].opt : "default");
		if (wl->gnu_c && ccs[i].style == STYLE_CL) {
			fprintf(f, "  %-8s skipped (GNU-C workload)\n", ccs[i].key);
			continue;
		}
		if (!c->m.ok) {
			fprintf(f, "  %-8s %8s %7s %10s %8s %9s %8s %7s %-17s\n",
							ccs[i].key, "n/a", "", "n/a", "n/a", "n/a", "n/a", "", "n/a");
			continue;
		}
		wmean = vec_mean(c->wall_s, c->nwall);
		fmt_stat(cpu, sizeof cpu, vec_mean(c->cpu_s, c->ncpu), c->ncpu > 0);
		fmt_stat(csd, sizeof csd, sqrt(vec_var(c->cpu_s, c->ncpu)), c->ncpu > 1);
		fmt_stat(wall, sizeof wall, wmean, c->nwall > 0);
		fmt_stat(wsd, sizeof wsd, sqrt(vec_var(c->wall_s, c->nwall)),
						 c->nwall > 1);
		{
			double lo, hi;
			if (boot_ci(c->wall_s, c->nwall, &lo, &hi))
				snprintf(ci, sizeof ci, "[%.1f,%.1f]", lo * 1000.0, hi * 1000.0);
			else
				snprintf(ci, sizeof ci, "%s", "n/a");
		}
		vs_ref(vs, sizeof vs, ccs, nccs, cells, i);
		fprintf(f, "  %-8s %8s %7s ", ccs[i].key, cpu, csd);
		if (wl->have_counts && wl->funcs && wmean > 0)
			fprintf(f, "%10.0f ", (double)wl->funcs / wmean);
		else
			fprintf(f, "%10s ", "n/a");
		if (c->m.objsize >= 0)
			fprintf(f, "%8.1f ", (double)c->m.objsize / 1024.0);
		else
			fprintf(f, "%8s ", "n/a");
		if (c->m.peak_kb >= 0)
			fprintf(f, "%9.1f ", (double)c->m.peak_kb / 1024.0);
		else
			fprintf(f, "%9s ", "n/a");
		fprintf(f, "%8s %7s %-17s  %s\n", wall, wsd, ci, vs);
	}
	if (g_stats)
		write_stats(f, ccs, nccs, cells);
	free(cells);
}

static const char *find_lim(const char *p, const char *lim, const char *needle) {
	const char *r = strstr(p, needle);
	return (r && r < lim) ? r : NULL;
}

static void attr(char *dst, int n, const char *p, const char *lim,
								 const char *key) {
	const char *a = find_lim(p, lim, key);
	if (a) {
		a += strlen(key);
		snprintf(dst, n, "%.*s", (int)(strcspn(a, "\"")), a);
	}
}

static void write_tests(FILE *f, const char *junit) {
	char *x = ts_read_file(junit, NULL), *p;
	int total = 0, fail = 0, skip = 0;
	if (!x)
		return;
	fprintf(f, "\nTest results (%s)\n", junit);
	fprintf(f, "  %-7s %-48s %8s\n", "status", "name", "time(s)");
	for (p = x; (p = strstr(p, "<testcase"));) {
		const char *gt = strchr(p, '>');
		const char *tagend, *extent;
		char name[256] = "?", tm[32] = "?", st[32] = "";
		int failed, skipped;
		if (!gt)
			break;
		tagend = gt + 1;
		if (gt > p && gt[-1] == '/') {
			extent = tagend;
		} else {
			const char *c = strstr(tagend, "</testcase>");
			extent = c ? c + 11 : x + strlen(x);
		}
		attr(name, sizeof name, p, gt, "name=\"");
		attr(tm, sizeof tm, p, gt, "time=\"");
		attr(st, sizeof st, p, gt, "status=\"");
		skipped = find_lim(tagend, extent, "<skipped") != NULL ||
							!strcmp(st, "notrun");
		failed = find_lim(tagend, extent, "<failure") != NULL ||
						 !strcmp(st, "fail");
		total++;
		if (skipped)
			skip++;
		else if (failed)
			fail++;
		fprintf(f, "  %-7s %-48.48s %8s\n",
						skipped ? "SKIP" : failed ? "FAIL"
																			: "PASS",
						name, tm);
		p = (char *)extent;
	}
	fprintf(f, "\n  totals: %d tests, %d passed, %d failed, %d skipped\n",
					total, total - fail - skip, fail, skip);
	free(x);
}

struct hostinfo {
	char cpu_model[256];
	int log_cores;
	int phys_cores;
	double cpu_mhz;
	long long mem_kb;
	char virt[96];
};

static int hypervisor_present(void) {
#if defined(_M_X64) || defined(_M_IX86)
	int r[4];
	__cpuid(r, 1);
	return (r[2] & (1 << 31)) ? 1 : 0;
#elif defined(__x86_64__) || defined(__i386__)
	unsigned a, b, c, d;
	if (!__get_cpuid(1, &a, &b, &c, &d))
		return -1;
	return (c & (1u << 31)) ? 1 : 0;
#else
	return -1;
#endif
}

#if MCC_HOST_LINUX
static char *read_all(const char *path) {
	FILE *fp = fopen(path, "rb");
	char *buf = NULL;
	size_t cap = 0, len = 0, n;
	if (!fp)
		return NULL;
	do {
		if (len + 4096 + 1 > cap) {
			char *nb = realloc(buf, cap = len + 8192);
			if (!nb) {
				free(buf);
				fclose(fp);
				return NULL;
			}
			buf = nb;
		}
		n = fread(buf + len, 1, 4096, fp);
		len += n;
	} while (n == 4096);
	fclose(fp);
	buf[len] = 0;
	return buf;
}

static int proc_field(const char *text, const char *key, char *out, int n) {
	size_t kl = strlen(key);
	const char *p = text;
	while (p && *p) {
		if (!strncmp(p, key, kl)) {
			const char *c = strchr(p, ':');
			if (c) {
				const char *e;
				for (c++; *c == ' ' || *c == '\t'; c++)
					;
				e = strchr(c, '\n');
				if (!e)
					e = c + strlen(c);
				snprintf(out, n, "%.*s", (int)(e - c), c);
				return 1;
			}
		}
		p = strchr(p, '\n');
		if (p)
			p++;
	}
	return 0;
}

static int file_has(const char *path, const char *needle, char *found, int fn) {
	char *t = read_all(path);
	int hit = 0;
	if (t && (!needle || strstr(t, needle))) {
		hit = 1;
		if (found) {
			int i = 0;
			while (t[i] && t[i] != '\n' && i < fn - 1)
				found[i] = t[i], i++;
			found[i] = 0;
		}
	}
	free(t);
	return hit;
}

static int decode_arm_midr(const char *ci, char *out, int n) {
	static const struct
	{
		unsigned impl, part;
		const char *name;
	} P[] = {
			{0x41, 0xd03, "ARM Cortex-A53"}, {0x41, 0xd05, "ARM Cortex-A55"}, {0x41, 0xd07, "ARM Cortex-A57"}, {0x41, 0xd08, "ARM Cortex-A72"}, {0x41, 0xd09, "ARM Cortex-A73"}, {0x41, 0xd0a, "ARM Cortex-A75"}, {0x41, 0xd0b, "ARM Cortex-A76"}, {0x41, 0xd0c, "ARM Neoverse-N1"}, {0x41, 0xd40, "ARM Neoverse-V1"}, {0x41, 0xd49, "ARM Neoverse-N2"}, {0x41, 0xd4f, "ARM Neoverse-V2"}, {0x41, 0xd46, "ARM Cortex-A510"}, {0x41, 0xd47, "ARM Cortex-A710"}, {0x41, 0xd4d, "ARM Cortex-A715"}, {0xc0, 0xac3, "Ampere-1"}, {0xc0, 0xac4, "Ampere-1a"}, {0x48, 0xd01, "HiSilicon TSV110"}, {0x51, 0x800, "Qualcomm Falkor"}, {0x4e, 0x004, "NVIDIA Carmel"}, {0x61, 0x000, "Apple"}, {0, 0, 0}};
	static const struct
	{
		unsigned impl;
		const char *vendor;
	} V[] = {
			{0x41, "ARM"}, {0x42, "Broadcom"}, {0x43, "Cavium"}, {0x48, "HiSilicon"}, {0x4e, "NVIDIA"}, {0x50, "APM"}, {0x51, "Qualcomm"}, {0x53, "Samsung"}, {0x61, "Apple"}, {0xc0, "Ampere"}, {0, 0}};
	char v[64];
	unsigned impl, part;
	int k;
	if (!proc_field(ci, "CPU implementer", v, sizeof v))
		return 0;
	impl = (unsigned)strtoul(v, NULL, 0);
	if (!proc_field(ci, "CPU part", v, sizeof v))
		return 0;
	part = (unsigned)strtoul(v, NULL, 0);
	for (k = 0; P[k].name; k++)
		if (P[k].impl == impl && P[k].part == part) {
			snprintf(out, n, "%s", P[k].name);
			return 1;
		}
	for (k = 0; V[k].vendor; k++)
		if (V[k].impl == impl) {
			snprintf(out, n, "%s part 0x%x", V[k].vendor, part);
			return 1;
		}
	snprintf(out, n, "aarch64 impl 0x%x part 0x%x", impl, part);
	return 1;
}
#endif

static void fill_hostinfo(struct hostinfo *h) {
	int hv;
	memset(h, 0, sizeof *h);
	snprintf(h->cpu_model, sizeof h->cpu_model, "%s", "?");
	h->log_cores = host_nproc();
	snprintf(h->virt, sizeof h->virt, "%s", "none detected");
	hv = hypervisor_present();
	if (hv == 1)
		snprintf(h->virt, sizeof h->virt, "%s", "virtualized (hypervisor present)");

#if MCC_HOST_LINUX
	{
		char v[256];
		char *ci = read_all("/proc/cpuinfo");
		char *mi = read_all("/proc/meminfo");
		if (ci) {
			if (proc_field(ci, "model name", v, sizeof v) ||
					proc_field(ci, "Model", v, sizeof v) ||
					proc_field(ci, "Hardware", v, sizeof v) ||
					decode_arm_midr(ci, v, sizeof v))
				snprintf(h->cpu_model, sizeof h->cpu_model, "%s", v);
			if (proc_field(ci, "cpu MHz", v, sizeof v))
				h->cpu_mhz = atof(v);
			if (proc_field(ci, "cpu cores", v, sizeof v))
				h->phys_cores = atoi(v);
		}
		if (mi && proc_field(mi, "MemTotal", v, sizeof v))
			h->mem_kb = atoll(v);
		free(ci);
		free(mi);
	}
	{
		static const char *VM[] = {
				"QEMU", "KVM", "VMware", "VirtualBox", "Xen",
				"Bochs", "Parallels", "Hyper-V", "Virtual Machine",
				"Google Compute", "OpenStack", "Amazon EC2", 0};
		char prod[128] = "";
		int is_vm = 0, k;
		file_has("/sys/class/dmi/id/product_name", NULL, prod, sizeof prod);
		for (k = 0; VM[k]; k++)
			if (strstr(prod, VM[k]))
				is_vm = 1;
		if (file_has("/.dockerenv", NULL, NULL, 0) ||
				file_has("/run/.containerenv", NULL, NULL, 0))
			snprintf(h->virt, sizeof h->virt, "%s", "container");
		else if (hv == 1 || is_vm)
			snprintf(h->virt, sizeof h->virt, "VM%s%s",
							 prod[0] ? ": " : " (hypervisor present)", prod[0] ? prod : "");
		else
			snprintf(h->virt, sizeof h->virt, "%s", "none detected (bare metal)");
	}
#elif MCC_HOST_DARWIN
	{
		size_t sz;
		long long ll;
		int ci;
		sz = sizeof h->cpu_model;
		sysctlbyname("machdep.cpu.brand_string", h->cpu_model, &sz, NULL, 0);
		sz = sizeof ll;
		if (!sysctlbyname("hw.memsize", &ll, &sz, NULL, 0))
			h->mem_kb = ll / 1024;
		sz = sizeof ci;
		if (!sysctlbyname("hw.physicalcpu", &ci, &sz, NULL, 0))
			h->phys_cores = ci;
		sz = sizeof ci;
		if (!sysctlbyname("hw.logicalcpu", &ci, &sz, NULL, 0))
			h->log_cores = ci;
		sz = sizeof ll;
		if (!sysctlbyname("hw.cpufrequency", &ll, &sz, NULL, 0))
			h->cpu_mhz = (double)ll / 1e6;
	}
#elif MCC_HOST_WIN32
	{
		MEMORYSTATUSEX ms;
		SYSTEM_INFO si;
		HKEY k;
		ms.dwLength = sizeof ms;
		if (GlobalMemoryStatusEx(&ms))
			h->mem_kb = (long long)(ms.ullTotalPhys / 1024);
		GetSystemInfo(&si);
		h->log_cores = (int)si.dwNumberOfProcessors;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
											"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0,
											KEY_READ, &k) == 0) {
			DWORD sz = sizeof h->cpu_model, mhz = 0, msz = sizeof mhz;
			RegQueryValueExA(k, "ProcessorNameString", NULL, NULL,
											 (LPBYTE)h->cpu_model, &sz);
			if (RegQueryValueExA(k, "~MHz", NULL, NULL, (LPBYTE)&mhz, &msz) == 0)
				h->cpu_mhz = (double)mhz;
			RegCloseKey(k);
		}
	}
#endif
}

static void write_sysinfo(FILE *f, const char *plat, const struct compiler *ccs,
													int nccs) {
	char sysname[128] = "?", release[128] = "?", machine[64] = "?";
	struct hostinfo h;
	int i;
	host_sys_info(sysname, sizeof sysname, release, sizeof release, machine,
								sizeof machine);
	fill_hostinfo(&h);
	fprintf(f, "mcc benchmark report");
	if (plat)
		fprintf(f, " — %s", plat);
	fprintf(f, "\n====================================================\n");
	fprintf(f, "System\n");
	fprintf(f, "  os      : %s %s\n", sysname, release);
	fprintf(f, "  arch    : %s\n", machine);
	fprintf(f, "  cpu     : %s\n", h.cpu_model[0] ? h.cpu_model : "?");
	if (h.phys_cores)
		fprintf(f, "  cores   : %d physical, %d logical\n", h.phys_cores,
						h.log_cores);
	else
		fprintf(f, "  cores   : %d logical\n", h.log_cores);
	if (h.cpu_mhz > 0)
		fprintf(f, "  clock   : %.0f MHz\n", h.cpu_mhz);
	if (h.mem_kb > 0)
		fprintf(f, "  memory  : %.1f GiB (%lld kB)\n",
						(double)h.mem_kb / (1024.0 * 1024.0), h.mem_kb);
	fprintf(f, "  virt    : %s\n", h.virt);
	fprintf(f, "  compilers:\n");
	for (i = 0; i < nccs; i++) {
		if (ccs[i].opt)
			continue;
		fprintf(f, "    %-8s %s  (%s)\n", ccs[i].key,
						ccs[i].version[0] ? ccs[i].version : "?", ccs[i].path);
	}
	fprintf(f, "  mcc rows are self-hosted stage2 binaries, rebuilt from"
						 " src/mcc.c at each group's -O level\n");
}

static void probe_cl_version(const char *cc, char *version, int vsz) {
	const char *argv[] = {cc, NULL};
	char *err = NULL;
	HostSpawnOpts o;
	memset(&o, 0, sizeof o);
	o.stderr_buf = &err;
	version[0] = 0;
	if (host_spawn_ex(argv, &o) == 0 && err) {
		char *nl = strchr(err, '\n');
		if (nl)
			*nl = 0;
		snprintf(version, vsz, "%s", err);
	}
	free(err);
	version[vsz - 1] = 0;
}

static int detect(struct compiler *cc, const char *key, const char *const *names,
									int style, const char *ccmacro) {
	char m[128];
	cc->path[0] = 0;
	if (!host_find_tool_any(names, MCC_HOST_WIN32 ? ".exe" : NULL, cc->path,
													sizeof cc->path))
		return 0;
	cc->key = key;
	cc->style = style;
	cc->ccmacro = ccmacro;
	cc->version[0] = 0;
	cc->opt = NULL;
	if (style == STYLE_CL)
		probe_cl_version(cc->path, cc->version, sizeof cc->version);
	else
		ts_cc_probe(cc->path, m, sizeof m, cc->version, sizeof cc->version);
	return 1;
}

static void host_target_defs(const char **defs, int *n) {
#if defined(__x86_64__) || defined(_M_X64)
	defs[(*n)++] = "MCC_TARGET_X86_64=1";
#elif defined(__i386__) || defined(_M_IX86)
	defs[(*n)++] = "MCC_TARGET_I386=1";
#elif defined(__aarch64__) || defined(_M_ARM64)
	defs[(*n)++] = "MCC_TARGET_ARM64=1";
#elif defined(__riscv)
	defs[(*n)++] = "MCC_TARGET_RISCV64=1";
#elif defined(__arm__)
	defs[(*n)++] = "MCC_TARGET_ARM=1";
#else
	defs[(*n)++] = "MCC_TARGET_X86_64=1";
#endif
#if MCC_HOST_WIN32
	defs[(*n)++] = "MCC_TARGET_PE=1";
#endif
#if MCC_HOST_DARWIN
	defs[(*n)++] = "MCC_TARGET_MACHO=1";
#endif
}

static int build_self_mcc(const char *stage1, const struct workload *wl,
													const char *builddir, const char *opt,
													char *out, int outsz) {
	Argv v = {{0}, 0};
	char buf[4160];
	int i;
	ts_path(out, outsz, builddir, "mccbench-self%s%s", opt ? opt : "",
					MCC_HOST_WIN32 ? ".exe" : "");
	ts_arg(&v, stage1);
	snprintf(buf, sizeof buf, "-B%s", builddir);
	ts_arg(&v, strdup(buf));
	ts_arg(&v, wl->src);
	ts_arg(&v, "-o");
	ts_arg(&v, out);
	for (i = 0; wl->incs[i]; i++) {
		snprintf(buf, sizeof buf, "-I%s", wl->incs[i]);
		ts_arg(&v, strdup(buf));
	}
	for (i = 0; wl->defs[i]; i++) {
		snprintf(buf, sizeof buf, "-D%s", wl->defs[i]);
		ts_arg(&v, strdup(buf));
	}
	ts_arg(&v, "-DCC_NAME=CC_mcc");
	ts_arg(&v, "-DMCC_CONFIG_OPTIMIZER=1");
	if (opt)
		ts_arg(&v, opt);
	return host_spawn_wait(ts_argz(&v)) == 0;
}

static int detect_gnu_gcc(struct compiler *cc) {
	char m[128];
	cc->path[0] = 0;
	if (!ts_resolve_reference_cc(cc->path, sizeof cc->path))
		return 0;
	cc->key = "gcc";
	cc->style = STYLE_GCC;
	cc->ccmacro = "CC_gcc";
	cc->version[0] = 0;
	cc->opt = NULL;
	ts_cc_probe(cc->path, m, sizeof m, cc->version, sizeof cc->version);
	return 1;
}

int main(int argc, char **argv) {
	const char *mccpath = NULL, *srcroot = ".", *builddir = ".";
	const char *plat = NULL, *out = NULL, *junit = NULL;
	int repeats = REPEATS_DEFAULT, i, nccs = 0, nwl = 0;
	struct compiler ccs[MAXCC];
	struct workload wls[MAXWL];
	struct workload *self_wl = NULL;
	FILE *f;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--mcc") && i + 1 < argc)
			mccpath = argv[++i];
		else if (!strcmp(argv[i], "--srcroot") && i + 1 < argc)
			srcroot = argv[++i];
		else if (!strcmp(argv[i], "--builddir") && i + 1 < argc)
			builddir = argv[++i];
		else if (!strcmp(argv[i], "--plat") && i + 1 < argc)
			plat = argv[++i];
		else if (!strcmp(argv[i], "--out") && i + 1 < argc)
			out = argv[++i];
		else if (!strcmp(argv[i], "--junit") && i + 1 < argc)
			junit = argv[++i];
		else if ((!strcmp(argv[i], "--repeats") || !strcmp(argv[i], "--reps")) &&
						 i + 1 < argc)
			repeats = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--stats"))
			g_stats = 1;
	}
	if (!mccpath || !out) {
		fprintf(stderr, "usage: mccbench --mcc <exe> --out <file> [--srcroot D] "
										"[--builddir D] [--plat P] [--junit XML] [--repeats N] "
										"[--stats]\n");
		return 2;
	}
	if (repeats < 1)
		repeats = 1;
	if (repeats > MAXREP)
		repeats = MAXREP;

	{
		struct workload *w;

		w = &wls[nwl++];
		memset(w, 0, sizeof *w);
		w->key = "portable-corpus";
		ts_path(w->src, sizeof w->src, srcroot, "tests/bench/corpus.c");

		w = &wls[nwl++];
		memset(w, 0, sizeof *w);
		w->key = "full-language";
		w->gnu_c = 1;
		ts_path(w->src, sizeof w->src, srcroot, "tests/diff/full_language.c");
		w->incs[0] = srcroot;
		{
			static char inc1[4096];
			ts_path(inc1, sizeof inc1, srcroot, "runtime/include");
			w->incs[1] = inc1;
		}
		w->needs_ccmacro = 1;

		w = &wls[nwl++];
		memset(w, 0, sizeof *w);
		w->key = "mcc-self";
		self_wl = w;
		ts_path(w->src, sizeof w->src, srcroot, "src/mcc.c");
		{
			static char id[11][4096];
			static const char *rel[] = {
					"src", "src/arch/i386", "src/arch/x86_64",
					"src/arch/arm", "src/arch/arm64",
					"src/arch/riscv64", "src/objfmt",
					"src/formats", "include"};
			int k;
			for (k = 0; k < 9; k++) {
				ts_path(id[k], sizeof id[k], srcroot, "%s", rel[k]);
				w->incs[k] = id[k];
			}
			snprintf(id[9], sizeof id[9], "%s", builddir);
			w->incs[9] = id[9];
			w->incs[10] = srcroot;
		}
		{
			int nd = 0;
			w->defs[nd++] = "MCC_AMALGAMATED=1";
			w->defs[nd++] = "MCC_CONFIG_PREDEFS=1";
			w->defs[nd++] = "MCC_CONFIG_AUTO_MCCDIR=1";
			w->defs[nd++] = "MCC_VERSION=20260706135200";
			host_target_defs(w->defs, &nd);
		}
		w->needs_ccmacro = 1;
	}

	ccs[nccs].key = "mcc";
	snprintf(ccs[nccs].path, sizeof ccs[nccs].path, "%s", mccpath);
	ccs[nccs].style = STYLE_GCC;
	ccs[nccs].ccmacro = "CC_mcc";
	ccs[nccs].version[0] = 0;
	ccs[nccs].opt = NULL;
	{
		char mm[128];
		ts_cc_probe(mccpath, mm, sizeof mm, ccs[nccs].version, sizeof ccs[nccs].version);
	}
	nccs++;
	{
		const char *clang[] = {"clang", 0};
		const char *mingw[] = {"x86_64-w64-mingw32-gcc", "i686-w64-mingw32-gcc", 0};
		const char *cl[] = {"cl", 0};
		if (nccs < MAXCC && detect_gnu_gcc(&ccs[nccs]))
			nccs++;
		if (nccs < MAXCC && detect(&ccs[nccs], "clang", clang, STYLE_GCC, "CC_clang"))
			nccs++;
		if (nccs < MAXCC && detect(&ccs[nccs], "mingw", mingw, STYLE_GCC, "CC_gcc"))
			nccs++;
		if (nccs < MAXCC && detect(&ccs[nccs], "msvc", cl, STYLE_CL, "CC_msvc"))
			nccs++;
	}

	{
		struct compiler base[MAXCC];
		static char selfpath[4][4096];
		static const char *gccopts[] = {NULL, "-O1", "-O2", "-O3"};
		static const char *clopts[] = {NULL, "/O1", "/O2", NULL};
		int nbase = nccs, k, o;
		memcpy(base, ccs, sizeof(struct compiler) * nbase);
		nccs = 0;
		for (o = 0; o < (int)(sizeof gccopts / sizeof *gccopts); o++) {
			struct compiler *mc;
			if (nccs >= MAXCC)
				break;
			mc = &ccs[nccs++];
			*mc = base[0];
			mc->opt = gccopts[o];
			printf("==> self-hosting mcc at %s\n",
						 gccopts[o] ? gccopts[o] : "default");
			if (build_self_mcc(mccpath, self_wl, builddir, gccopts[o],
												 selfpath[o], sizeof selfpath[o])) {
				char mm[128];
				size_t vl;
				snprintf(mc->path, sizeof mc->path, "%s", selfpath[o]);
				ts_cc_probe(mc->path, mm, sizeof mm, mc->version,
										sizeof mc->version);
				vl = strlen(mc->version);
				snprintf(mc->version + vl, sizeof mc->version - vl, "%s",
								 " [self-hosted]");
			} else {
				fprintf(stderr,
								"mccbench: self-hosting mcc at %s failed; "
								"benchmarking %s instead\n",
								gccopts[o] ? gccopts[o] : "default", mccpath);
			}
			for (k = 1; k < nbase && nccs < MAXCC; k++) {
				if (base[k].style == STYLE_CL && o && !clopts[o])
					continue;
				ccs[nccs] = base[k];
				ccs[nccs].opt = base[k].style == STYLE_CL ? clopts[o] : gccopts[o];
				nccs++;
			}
		}
	}

	{
		char dir[4096];
		const char *slash = strrchr(out, '/');
#if MCC_HOST_WIN32
		const char *bslash = strrchr(out, '\\');
		if (bslash && (!slash || bslash > slash))
			slash = bslash;
#endif
		if (slash && slash > out) {
			snprintf(dir, sizeof dir, "%.*s", (int)(slash - out), out);
			host_mkdirs(dir);
		}
	}
	if (!(f = fopen(out, "wb"))) {
		fprintf(stderr, "mccbench: cannot write %s\n", out);
		return 1;
	}
	printf("==> benchmarking %d compilers x %d workloads (n=%d runs) -> %s\n",
				 nccs, nwl, repeats, out);
	write_sysinfo(f, plat, ccs, nccs);
	for (i = 0; i < nwl; i++) {
		count_with_mcc(&ccs[0], &wls[i]);
		write_table(f, ccs, nccs, &wls[i], repeats);
	}
	fprintf(f, "\nStatistics: cpu(s)/wall(s) are sample means over n=%d runs per"
						 " cell; sd is the sample standard deviation\n"
						 "  95%%CI(ms): percentile bootstrap 95%% confidence interval of the"
						 " mean wall time (B=%d resamples with replacement, deterministic"
						 " xorshift RNG, distribution-free)\n"
						 "  vs-ref: wall-time delta of each mcc row against the first"
						 " reference compiler row in the same -O group and workload\n"
						 "  * = significant, ns = not significant: Welch's t-test,"
						 " two-tailed alpha=0.05, Welch-Satterthwaite df floored into"
						 " a critical-value table\n"
						 "  d = Cohen's d effect size vs the same reference (pooled SD),"
						 " labelled negligible/small/medium/large at |d|<0.2/0.5/0.8\n",
					repeats, BOOT_B);
	if (junit)
		write_tests(f, junit);
	fclose(f);
	printf("==> wrote %s\n", out);
	return 0;
}
