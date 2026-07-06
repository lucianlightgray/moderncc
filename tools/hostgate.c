#include "toolsupport.h"

static const char *BANNED[] = {
	"_WIN32", "_WIN64", "_MSC_VER", "__MINGW32__", "__MINGW64__", "__CYGWIN__",
	"__APPLE__", "__linux__", "__FreeBSD__", "__FreeBSD_kernel__", "__NetBSD__",
	"__OpenBSD__", "__DragonFly__", "__ANDROID__", "__dietlibc__", 0};

static int g_violations;

static int is_id(int c) {
	return isalnum((unsigned char)c) || c == '_';
}

static int is_banned(const char *tok, int len) {
	int i;
	for (i = 0; BANNED[i]; ++i)
		if ((int)strlen(BANNED[i]) == len && !strncmp(BANNED[i], tok, len))
			return 1;
	return 0;
}

static const char *cond_expr(const char *line) {
	const char *s = line;
	while (*s == ' ' || *s == '\t')
		++s;
	if (*s != '#')
		return NULL;
	++s;
	while (*s == ' ' || *s == '\t')
		++s;
	if (!strncmp(s, "ifdef", 5) && !is_id(s[5]))
		return s + 5;
	if (!strncmp(s, "ifndef", 6) && !is_id(s[6]))
		return s + 6;
	if (!strncmp(s, "elif", 4) && !is_id(s[4]))
		return s + 4;
	if (!strncmp(s, "if", 2) && !is_id(s[2]))
		return s + 2;
	return NULL;
}

static int expr_has_banned(const char *s) {
	while (*s) {
		if (*s == '"') {
			for (++s; *s && *s != '"'; ++s)
				if (*s == '\\' && s[1])
					++s;
			if (*s)
				++s;
		} else if (*s == '\'') {
			for (++s; *s && *s != '\''; ++s)
				if (*s == '\\' && s[1])
					++s;
			if (*s)
				++s;
		} else if (*s == '/' && s[1] == '/') {
			break;
		} else if (*s == '/' && s[1] == '*') {
			for (s += 2; *s && !(*s == '*' && s[1] == '/'); ++s)
				;
			if (*s)
				s += 2;
		} else if (isalpha((unsigned char)*s) || *s == '_') {
			const char *b = s;
			while (is_id(*s))
				++s;
			if (is_banned(b, (int)(s - b)))
				return 1;
		} else {
			++s;
		}
	}
	return 0;
}

static int scan_file(const char *path, int is_dir, void *ud) {
	const char *base;
	char *text, *p;
	int ln = 0, elen;
	(void)ud;
	if (is_dir)
		return 0;
	elen = (int)strlen(path);
	/* Scan C sources/headers and assembly (.s/.S) and .inc includes: any of
	 * these can carry a raw host-macro #if. Assembly/.inc appears only under
	 * tests/ today (deliberate preprocessor fixtures, not a scanned root), so
	 * this is future-proofing for src/tools should an .S/.inc ever land there. */
	if (!(elen >= 2 && path[elen - 2] == '.' &&
		  (path[elen - 1] == 'c' || path[elen - 1] == 'h' ||
		   path[elen - 1] == 's' || path[elen - 1] == 'S')) &&
		!(elen >= 4 && !strcmp(path + elen - 4, ".inc")))
		return 0;
	base = strrchr(path, '/');
	base = base ? base + 1 : path;
	if (!strcmp(base, "mcchost.h") || !strcmp(base, "mcchost.c"))
		return 0;
	if (!(text = ts_read_file(path, NULL)))
		return 0;
	for (p = text; *p;) {
		char *e = strchr(p, '\n');
		int len = e ? (int)(e - p) : (int)strlen(p);
		const char *expr;
		++ln;
		if (len && p[len - 1] == '\r')
			--len;
		p[len] = 0;
		if ((expr = cond_expr(p)) && expr_has_banned(expr)) {
			const char *t = p;
			while (*t == ' ' || *t == '\t')
				++t;
			printf("  %s:%d: %s\n", path, ln, t);
			g_violations++;
		}
		if (!e)
			break;
		p = e + 1;
	}
	free(text);
	return 0;
}

int main(int argc, char **argv) {
	static const char *defaults[] = {"src", "tools", 0};
	const char *const *roots = argc > 1 ? (const char *const *)(argv + 1) : defaults;
	int i, n = argc > 1 ? argc - 1 : 2;

	for (i = 0; i < n; ++i) {
		int isd;
		if (host_stat(roots[i], &isd, NULL, NULL) || !isd) {
			fprintf(stderr, "hostgate: not a directory: %s\n", roots[i]);
			return 2;
		}
		host_dir_walk(roots[i], 1, scan_file, NULL);
	}

	if (g_violations) {
		fprintf(stderr,
				"host-gate invariant violated - raw host macros tested outside\n"
				"src/mcchost.{h,c} (use MCC_HOST_* or a host_* function from\n"
				"mcchost.h instead): %d violation(s) above\n",
				g_violations);
		return 1;
	}
	printf("host-gate invariant OK: no raw host-macro tests outside src/mcchost.{h,c}\n");
	return 0;
}
