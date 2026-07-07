#include "toolhost.h"
#include "toolsupport.h"

int ts_run(const char *const *argv) {
	int i;
	for (i = 0; argv[i]; ++i)
		printf("%s%s", i ? " " : "  ", argv[i]);
	printf("\n");
	fflush(stdout);
	return host_spawn_wait(argv);
}

void ts_arg(Argv *v, const char *s) {
	if (v->n >= TS_ARGV_MAX - 1) {
		fprintf(stderr, "ts_arg: argv overflow (>%d)\n", TS_ARGV_MAX - 1);
		exit(2);
	}
	v->a[v->n++] = s;
}

void ts_args(Argv *v, const char *const *set) {
	int i;
	for (i = 0; set[i]; ++i)
		ts_arg(v, set[i]);
}

const char *const *ts_argz(Argv *v) {
	v->a[v->n] = 0;
	return v->a;
}

int ts_path(char *dst, size_t n, const char *dir, const char *fmt, ...) {
	va_list ap;
	int k;

	k = snprintf(dst, n, "%s/", dir);
	if (k < 0 || (size_t)k >= n)
		return k;
	va_start(ap, fmt);
	vsnprintf(dst + k, n - (size_t)k, fmt, ap);
	va_end(ap);
	return k;
}

int ts_fnmatch(const char *p, const char *s) {
	if (!*p)
		return !*s;
	if (*p == '*') {
		while (*p == '*')
			++p;
		if (!*p)
			return 1;
		for (; *s; ++s)
			if (ts_fnmatch(p, s))
				return 1;
		return 0;
	}
	if (!*s)
		return 0;
	if (*p == '?')
		return ts_fnmatch(p + 1, s + 1);
	if (*p == '[') {
		const char *q = p + 1;
		int neg = 0, matched = 0;
		if (*q == '!' || *q == '^') {
			neg = 1;
			++q;
		}
		for (; *q && *q != ']'; ++q) {
			if (q[1] == '-' && q[2] && q[2] != ']') {
				if ((unsigned char)*s >= (unsigned char)q[0] &&
						(unsigned char)*s <= (unsigned char)q[2])
					matched = 1;
				q += 2;
			} else if (*q == *s) {
				matched = 1;
			}
		}
		if (matched == neg)
			return 0;
		return ts_fnmatch(*q == ']' ? q + 1 : q, s + 1);
	}
	return *p == *s ? ts_fnmatch(p + 1, s + 1) : 0;
}

struct ts_globctx {
	const char *pat;
	char **out;
	int max, count;
};

static int ts_glob_cb(const char *path, int is_dir, void *ud) {
	struct ts_globctx *g = ud;
	const char *b;
	if (is_dir)
		return 0;
	b = strrchr(path, '/');
	b = b ? b + 1 : path;
	if (ts_fnmatch(g->pat, b)) {
		if (g->count < g->max)
			g->out[g->count] = strdup(path);
		g->count++;
	}
	return 0;
}

int ts_glob(const char *dir, const char *pat, int recursive, char **out, int max) {
	struct ts_globctx g;
	g.pat = pat;
	g.out = out;
	g.max = max;
	g.count = 0;
	if (host_dir_walk(dir, recursive, ts_glob_cb, &g) < 0)
		return -1;
	return g.count;
}

int ts_file_equal(const char *a, const char *b) {
	FILE *fa = fopen(a, "rb"), *fb;
	char ba[65536], bb[65536];
	int eq = 1;
	if (!fa)
		return -1;
	if (!(fb = fopen(b, "rb"))) {
		fclose(fa);
		return -1;
	}
	for (;;) {
		size_t ra = fread(ba, 1, sizeof ba, fa);
		size_t rb = fread(bb, 1, sizeof bb, fb);
		if (ra != rb || memcmp(ba, bb, ra)) {
			eq = 0;
			break;
		}
		if (ra == 0)
			break;
	}
	fclose(fa);
	fclose(fb);
	return eq;
}

char *ts_read_file(const char *path, long *len) {
	FILE *f = fopen(path, "rb");
	long n;
	char *buf;
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 0 || !(buf = malloc(n + 1))) {
		fclose(f);
		return NULL;
	}
	if (fread(buf, 1, n, f) != (size_t)n) {
		free(buf);
		fclose(f);
		return NULL;
	}
	buf[n] = 0;
	fclose(f);
	if (len)
		*len = n;
	return buf;
}

static int ts_contains_any(const char *line, const char *const *set) {
	int i;
	if (!set)
		return 0;
	for (i = 0; set[i]; ++i)
		if (strstr(line, set[i]))
			return 1;
	return 0;
}

char *ts_first_error_line(const char *text,
													const char *const *needles,
													const char *const *skips) {
	const char *p = text;
	char *fallback = NULL;
	if (!text)
		return NULL;
	while (*p) {
		const char *e = strchr(p, '\n');
		int len = e ? (int)(e - p) : (int)strlen(p);
		int trim = len;
		const char *q = p;
		char *line;
		while (trim > 0 && (p[trim - 1] == '\r' || p[trim - 1] == ' ' || p[trim - 1] == '\t'))
			--trim;
		while (q < p + trim && (*q == ' ' || *q == '\t'))
			++q;
		if (q < p + trim) {
			line = malloc(p + trim - q + 1);
			memcpy(line, q, p + trim - q);
			line[p + trim - q] = 0;
			if (!ts_contains_any(line, skips)) {
				if (ts_contains_any(line, needles)) {
					free(fallback);
					return line;
				}
				if (!fallback)
					fallback = line;
				else
					free(line);
			} else {
				free(line);
			}
		}
		if (!e)
			break;
		p = e + 1;
	}
	return fallback;
}

int ts_cc_probe(const char *cc, char *machine, int msz, char *version, int vsz) {
	int ok = 0;
	if (machine && msz) {
		const char *argv[] = {cc, "-dumpmachine", NULL};
		char *out = NULL;
		HostSpawnOpts o;
		memset(&o, 0, sizeof o);
		o.stdout_buf = &out;
		if (host_spawn_ex(argv, &o) == 0 && out) {
			char *nl = strchr(out, '\n');
			if (nl)
				*nl = 0;
			snprintf(machine, msz, "%s", out);
			ok = 1;
		}
		free(out);
		machine[msz - 1] = 0;
		if (!ok)
			return -1;
	}
	if (version && vsz) {
		const char *argv[] = {cc, "--version", NULL};
		char *out = NULL;
		HostSpawnOpts o;
		memset(&o, 0, sizeof o);
		o.stdout_buf = &out;
		version[0] = 0;
		if (host_spawn_ex(argv, &o) == 0 && out) {
			char *nl = strchr(out, '\n');
			if (nl)
				*nl = 0;
			snprintf(version, vsz, "%s", out);
		}
		free(out);
		version[vsz - 1] = 0;
	}
	return 0;
}

int ts_resolve_reference_cc(char *buf, int size) {
	static const char *cands[] = {"gcc", "cc", "gcc-14", "gcc-13", "gcc-12", NULL};
	int i;
	for (i = 0; cands[i]; ++i) {
		char path[4096], ver[256];
		if (!host_find_tool(cands[i], NULL, path, sizeof path))
			continue;
		if (ts_cc_probe(cands[i], NULL, 0, ver, sizeof ver))
			continue;

		if (strstr(ver, "clang") || strstr(ver, "Apple LLVM"))
			continue;
		if (strstr(ver, "gcc") || strstr(ver, "GCC") || strstr(ver, "Free Software")) {
			snprintf(buf, size, "%s", path);
			return 1;
		}
	}
	return 0;
}

void ts_skip(const char *fmt, ...) {
	va_list ap;
	fputs("SKIP: ", stdout);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	fputc('\n', stdout);
	exit(TS_SKIP_CODE);
}

int ts_git_stamp(char *buf, int size) {
	char *branch = NULL, *stamp = NULL, *err = NULL, fmt[256], *nl;
	HostSpawnOpts o;
	int dirty;

	{
		const char *a[] = {"git", "rev-parse", "--abbrev-ref", "HEAD", 0};
		memset(&o, 0, sizeof o);
		o.stdout_buf = &branch;
		o.stderr_buf = &err;
		if (host_spawn_ex(a, &o) != 0 || !branch) {
			free(branch);
			free(err);
			return -1;
		}
		free(err);
		err = NULL;
	}
	if ((nl = strchr(branch, '\n')))
		*nl = 0;
	if (!*branch) {
		free(branch);
		return -1;
	}

	snprintf(fmt, sizeof fmt, "--pretty=format:%%cd %s@%%h", branch);
	{
		const char *a[] = {"git", "log", "-1", "--date=short", fmt, 0};
		memset(&o, 0, sizeof o);
		o.stdout_buf = &stamp;
		o.stderr_buf = &err;
		if (host_spawn_ex(a, &o) != 0 || !stamp) {
			free(branch);
			free(stamp);
			free(err);
			return -1;
		}
		free(err);
	}
	free(branch);
	if ((nl = strchr(stamp, '\n')))
		*nl = 0;

	{
		const char *a[] = {"git", "diff", "--quiet", 0};
		dirty = host_spawn_ex(a, NULL);
	}
	snprintf(buf, size, "%s%s", stamp, dirty ? "*" : "");
	free(stamp);
	return 0;
}
