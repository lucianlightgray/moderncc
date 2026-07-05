/* mccbench — compile-speed / footprint benchmark and report generator.
 *
 * Races the self-hosted mcc against whatever host compilers are present
 * (gcc, clang, mingw, msvc) over a set of workloads, measuring wall time,
 * CPU time, peak memory, emitted-object size, and functions/second, then
 * writes a plain-text report: system info + a per-workload compiler table +
 * a table of test results parsed from a ctest JUnit XML file.
 *
 * Default-off; CI turns it on (see MCC_BENCH). Runs on the host only. */
#include "toolsupport.h"

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

/* Test the MSVC-only target macros first: mcchost.h aliases __x86_64__/__i386__
 * on cl (for mcc's own sources), which would otherwise pull in GCC's <cpuid.h>. */
#if defined(_M_X64) || defined(_M_IX86)
#include <intrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

#define REPS_DEFAULT 3
#define MAXCC 8
#define MAXWL 8

/* ----- compilers -------------------------------------------------------- */

enum { STYLE_GCC, STYLE_CL };

struct compiler {
	const char *key;     /* short name shown in the table */
	char path[4096];     /* resolved executable */
	int style;           /* command-line dialect */
	const char *ccmacro; /* value for full_language's CC_NAME (NULL => n/a) */
	char version[128];
};

/* ----- workloads -------------------------------------------------------- */

struct workload {
	const char *key;
	char src[4096];             /* absolute source path */
	const char *incs[16];       /* include dirs (absolute), NULL-terminated */
	const char *defs[16];       /* extra -D defines, NULL-terminated */
	int needs_ccmacro;          /* append -DCC_NAME=<compiler->ccmacro> */
	int funcs;                  /* function count (from mcc -bench), 0 if unknown */
	int lines;                  /* preprocessed line count (from mcc -bench) */
	int have_counts;
};

/* ----- one measurement -------------------------------------------------- */

struct meas {
	int ok;
	unsigned wall_ms;   /* best (min) wall time */
	long cpu_ms;        /* user+sys CPU of the compiler process (-1 = n/a) */
	long peak_kb;       /* peak RSS in KiB (-1 = n/a) */
	long long objsize;  /* emitted object size in bytes (-1 = n/a) */
};

/* Run argv once, redirecting its output to the void, and measure it. */
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
		/* ru_maxrss: KiB on Linux, bytes on Darwin. */
		m.peak_kb = MCC_HOST_DARWIN ? ru.ru_maxrss / 1024 : ru.ru_maxrss;
		m.ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
	}
#else
	/* Windows: spawn via CreateProcess so we can read the child's CPU time
	 * (GetProcessTimes) and peak working set (GetProcessMemoryInfo). */
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
				m.cpu_ms = (long)((k + u) / 10000); /* 100ns units -> ms */
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

/* Render the compile command for (cc, wl) into v, output object at obj. */
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
}

/* Best-of-reps measurement of (cc, wl). */
static struct meas bench_one(const struct compiler *cc,
							 const struct workload *wl, int reps) {
	struct meas best;
	char obj[4096];
	int r;
	best.ok = 0;
	best.wall_ms = 0;
	best.cpu_ms = -1;
	best.peak_kb = -1;
	best.objsize = -1;
	ts_path(obj, sizeof obj, ".", "mccbench-%s-%s.o", cc->key, wl->key);
	if (wl->needs_ccmacro && !cc->ccmacro)
		return best; /* workload not applicable to this compiler */
	for (r = 0; r < reps; r++) {
		Argv v = {{0}, 0};
		struct meas m;
		build_cmd(&v, cc, wl, obj);
		m = measure_once(ts_argz(&v));
		if (!m.ok)
			return m; /* a failing compile => not applicable; report n/a */
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
	return best;
}

/* Ask mcc how many functions / lines a workload has (for funcs/sec). */
static void count_with_mcc(const struct compiler *mcc, struct workload *wl) {
	Argv v = {{0}, 0};
	char obj[4096], *err = NULL;
	HostSpawnOpts o;
	const char *p;
	wl->have_counts = 0;
	ts_path(obj, sizeof obj, ".", "mccbench-count-%s.o", wl->key);
	build_cmd(&v, mcc, wl, obj);
	/* insert -bench right after the program name */
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
		/* line: "# <idents> idents, <lines> lines, <funcs> functions, ..." */
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

/* ----- report ----------------------------------------------------------- */

static void fmt_secs(char *b, int n, long ms) {
	if (ms < 0)
		snprintf(b, n, "%s", "   n/a");
	else
		snprintf(b, n, "%6.3f", (double)ms / 1000.0);
}

static void write_table(FILE *f, const struct compiler *ccs, int nccs,
						struct workload *wl, int reps) {
	int i;
	char cpu[16], wall[16];
	if (wl->have_counts)
		fprintf(f, "\nWorkload: %s  (%d lines, %d functions, best of %d)\n",
				wl->key, wl->lines, wl->funcs, reps);
	else
		fprintf(f, "\nWorkload: %s  (best of %d)\n", wl->key, reps);
	fprintf(f, "  %-8s %8s %10s %8s %9s %8s\n",
			"compiler", "cpu(s)", "funcs/s", "obj(KB)", "peak(MB)", "wall(s)");
	for (i = 0; i < nccs; i++) {
		struct meas m = bench_one(&ccs[i], wl, reps);
		fmt_secs(cpu, sizeof cpu, m.cpu_ms);
		fmt_secs(wall, sizeof wall, (long)m.wall_ms);
		if (!m.ok) {
			fprintf(f, "  %-8s %8s %10s %8s %9s %8s\n",
					ccs[i].key, "n/a", "n/a", "n/a", "n/a", "n/a");
			continue;
		}
		fprintf(f, "  %-8s %8s ", ccs[i].key, cpu);
		if (wl->have_counts && wl->funcs && m.wall_ms)
			fprintf(f, "%10.0f ", (double)wl->funcs * 1000.0 / m.wall_ms);
		else
			fprintf(f, "%10s ", "n/a");
		if (m.objsize >= 0)
			fprintf(f, "%8.1f ", (double)m.objsize / 1024.0);
		else
			fprintf(f, "%8s ", "n/a");
		if (m.peak_kb >= 0)
			fprintf(f, "%9.1f ", (double)m.peak_kb / 1024.0);
		else
			fprintf(f, "%9s ", "n/a");
		fprintf(f, "%8s\n", wall);
	}
}

/* attribute/substring search bounded to [p, lim) */
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

/* Minimal JUnit XML summary: count testcases, failures, skips, list them.
 * Each <testcase> is either self-closing (<testcase .../>) or wraps
 * <failure>/<skipped> children; child/attribute lookups are bounded to the
 * single testcase's extent so a later case's <skipped> can't leak in. */
static void write_tests(FILE *f, const char *junit) {
	char *x = ts_read_file(junit, NULL), *p;
	int total = 0, fail = 0, skip = 0;
	if (!x) {
		fprintf(f, "\nTest results: (no JUnit file at %s)\n", junit);
		return;
	}
	fprintf(f, "\nTest results (%s)\n", junit);
	fprintf(f, "  %-7s %-48s %8s\n", "status", "name", "time(s)");
	for (p = x; (p = strstr(p, "<testcase")); ) {
		const char *gt = strchr(p, '>');
		const char *tagend, *extent;
		char name[256] = "?", tm[32] = "?", st[32] = "";
		int failed, skipped;
		if (!gt)
			break;
		tagend = gt + 1;                       /* end of the opening tag */
		if (gt > p && gt[-1] == '/') {
			extent = tagend;                   /* self-closing: no children */
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
				skipped ? "SKIP" : failed ? "FAIL" : "PASS", name, tm);
		p = (char *)extent;
	}
	fprintf(f, "\n  totals: %d tests, %d passed, %d failed, %d skipped\n",
			total, total - fail - skip, fail, skip);
	free(x);
}

/* ----- host machine details --------------------------------------------- */

struct hostinfo {
	char cpu_model[256];
	int log_cores;      /* logical CPUs / hardware threads */
	int phys_cores;     /* physical cores (0 = unknown) */
	double cpu_mhz;     /* nominal/current clock (0 = unknown) */
	long long mem_kb;   /* total physical RAM (0 = unknown) */
	char virt[96];      /* virtualization / container guess */
};

/* CPUID leaf-1 ECX bit 31 => running under a hypervisor. */
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
/* Read a whole file via streaming I/O. Needed for /proc and /sys, whose
 * virtual files report st_size == 0 and so read empty via stat-sized helpers. */
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

/* value of the first "key ... : <value>" line in text (proc-style) */
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
				proc_field(ci, "Hardware", v, sizeof v))
				snprintf(h->cpu_model, sizeof h->cpu_model, "%s", v);
			if (proc_field(ci, "cpu MHz", v, sizeof v))
				h->cpu_mhz = atof(v);
			if (proc_field(ci, "cpu cores", v, sizeof v))
				h->phys_cores = atoi(v);
		}
		if (mi && proc_field(mi, "MemTotal", v, sizeof v))
			h->mem_kb = atoll(v); /* "12345 kB" -> 12345 */
		free(ci);
		free(mi);
	}
	{ /* container / VM vendor hints refine the guess */
		static const char *VM[] = {"QEMU", "KVM", "VMware", "VirtualBox", "Xen",
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
	for (i = 0; i < nccs; i++)
		fprintf(f, "    %-8s %s  (%s)\n", ccs[i].key,
				ccs[i].version[0] ? ccs[i].version : "?", ccs[i].path);
}

/* ----- main ------------------------------------------------------------- */

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
	ts_cc_probe(cc->path, m, sizeof m, cc->version, sizeof cc->version);
	return 1;
}

int main(int argc, char **argv) {
	const char *mccpath = NULL, *srcroot = ".", *builddir = ".";
	const char *plat = NULL, *out = NULL, *junit = NULL;
	int reps = REPS_DEFAULT, i, nccs = 0, nwl = 0;
	struct compiler ccs[MAXCC];
	struct workload wls[MAXWL];
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
		else if (!strcmp(argv[i], "--reps") && i + 1 < argc)
			reps = atoi(argv[++i]);
	}
	if (!mccpath || !out) {
		fprintf(stderr, "usage: mccbench --mcc <exe> --out <file> [--srcroot D] "
						"[--builddir D] [--plat P] [--junit XML] [--reps N]\n");
		return 2;
	}
	if (reps < 1)
		reps = 1;

	/* the self-hosted compiler first, then whatever else is on the host */
	ccs[nccs].key = "mcc";
	snprintf(ccs[nccs].path, sizeof ccs[nccs].path, "%s", mccpath);
	ccs[nccs].style = STYLE_GCC;
	ccs[nccs].ccmacro = "CC_mcc";
	ccs[nccs].version[0] = 0;
	{
		char mm[128];
		ts_cc_probe(mccpath, mm, sizeof mm, ccs[nccs].version, sizeof ccs[nccs].version);
	}
	nccs++;
	{
		const char *gcc[] = {"gcc", 0}, *clang[] = {"clang", 0};
		const char *mingw[] = {"x86_64-w64-mingw32-gcc", "i686-w64-mingw32-gcc", 0};
		const char *cl[] = {"cl", 0};
		if (nccs < MAXCC && detect(&ccs[nccs], "gcc", gcc, STYLE_GCC, "CC_gcc"))
			nccs++;
		if (nccs < MAXCC && detect(&ccs[nccs], "clang", clang, STYLE_GCC, "CC_clang"))
			nccs++;
		if (nccs < MAXCC && detect(&ccs[nccs], "mingw", mingw, STYLE_GCC, "CC_gcc"))
			nccs++;
		if (nccs < MAXCC && detect(&ccs[nccs], "msvc", cl, STYLE_CL, NULL))
			nccs++;
	}

	/* workloads (paths built against srcroot / builddir) */
	{
		struct workload *w;

		/* 1. portable corpus — self-contained, every compiler */
		w = &wls[nwl++];
		memset(w, 0, sizeof *w);
		w->key = "portable-corpus";
		ts_path(w->src, sizeof w->src, srcroot, "tests/bench/corpus.c");

		/* 2. full_language differential test (needs CC_NAME; msvc n/a) */
		w = &wls[nwl++];
		memset(w, 0, sizeof *w);
		w->key = "full-language";
		ts_path(w->src, sizeof w->src, srcroot, "tests/diff/full_language.c");
		w->incs[0] = srcroot;
		{
			static char inc1[4096];
			ts_path(inc1, sizeof inc1, srcroot, "runtime/include");
			w->incs[1] = inc1;
		}
		w->needs_ccmacro = 1;

		/* 3. mcc's own whole-compiler TU (single source; msvc n/a) */
		w = &wls[nwl++];
		memset(w, 0, sizeof *w);
		w->key = "mcc-self";
		ts_path(w->src, sizeof w->src, srcroot, "src/mcc.c");
		{
			static char id[11][4096];
			static const char *rel[] = {"src", "src/arch/i386", "src/arch/x86_64",
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
		w->defs[0] = "SINGLE_SOURCE=1";
		w->defs[1] = "MCC_TARGET_X86_64=1";
		w->defs[2] = "MCC_EMBED_MCCRT=0";
		w->defs[3] = "CONFIG_MCC_PREDEFS=1";
		w->defs[4] = "MCC_VERSION=\"bench\"";
		w->needs_ccmacro = 1;
	}

	{ /* ensure the output directory exists (dist/ may be freshly created) */
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
	printf("==> benchmarking %d compilers x %d workloads (best of %d) -> %s\n",
		   nccs, nwl, reps, out);
	write_sysinfo(f, plat, ccs, nccs);
	for (i = 0; i < nwl; i++) {
		count_with_mcc(&ccs[0], &wls[i]);
		write_table(f, ccs, nccs, &wls[i], reps);
	}
	if (junit)
		write_tests(f, junit);
	fclose(f);
	printf("==> wrote %s\n", out);
	return 0;
}
