#include "toolsupport.h"

static const char *ALLOWED[] = {
		"src/mcc.h", "src/mccdefaults.h", "src/mcc.c", "src/mcctok.h", "src/mcchost.c", "src/libmcc.c",
		"src/mccpp.c", "src/mccgen.c", "src/mccasm.c", "src/mccdbg.c",
		"src/mccdis.c", "src/mccrun.c", "src/mccast.c", "src/mcctools.c",
		"src/objfmt/mccelf.c", "src/objfmt/mccpe.c", "src/objfmt/mccmacho.c", 0};

static int g_violations;

static int is_id(int c) {
	return isalnum((unsigned char)c) || c == '_';
}

static int path_allowed(const char *path) {
	int i, plen = (int)strlen(path);
	if (strstr(path, "src/arch/"))
		return 1;
	for (i = 0; ALLOWED[i]; ++i) {
		int alen = (int)strlen(ALLOWED[i]);
		if (plen >= alen && !strcmp(path + plen - alen, ALLOWED[i]) &&
				(plen == alen || path[plen - alen - 1] == '/'))
			return 1;
	}
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

static int expr_has_target(const char *s) {
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
			if (s - b > 11 && !strncmp(b, "MCC_TARGET_", 11))
				return 1;
		} else {
			++s;
		}
	}
	return 0;
}

static int scan_file(const char *path, int is_dir, void *ud) {
	char *text, *p;
	int ln = 0, elen, cont = 0;
	(void)ud;
	if (is_dir)
		return 0;
	elen = (int)strlen(path);
	if (!(elen >= 2 && path[elen - 2] == '.' &&
				(path[elen - 1] == 'c' || path[elen - 1] == 'h' ||
				 path[elen - 1] == 's' || path[elen - 1] == 'S')) &&
			!(elen >= 4 && !strcmp(path + elen - 4, ".inc")))
		return 0;
	if (path_allowed(path))
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
		if (cont || (expr = cond_expr(p)) != NULL) {
			const char *scan = cont ? p : expr;
			cont = len && p[len - 1] == '\\';
			if (expr_has_target(scan)) {
				const char *t = p;
				while (*t == ' ' || *t == '\t')
					++t;
				printf("  %s:%d: %s\n", path, ln, t);
				g_violations++;
			}
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
			fprintf(stderr, "targetgate: not a directory: %s\n", roots[i]);
			return 2;
		}
		host_dir_walk(roots[i], 1, scan_file, NULL);
	}

	if (g_violations) {
		fprintf(stderr,
						"target-gate invariant violated - MCC_TARGET_* tested in a\n"
						"preprocessor conditional outside src/arch/ and the frozen\n"
						"consumer allowlist: %d violation(s) above\n",
						g_violations);
		return 1;
	}
	printf("target-gate invariant OK: no new target conditionals outside src/arch/\n");
	return 0;
}
