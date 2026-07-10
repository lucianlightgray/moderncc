#include "toolsupport.h"

static int g_violations;

static int is_id(int c) {
	return isalnum((unsigned char)c) || c == '_';
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
	if (!strncmp(s, "elif", 4) && !is_id(s[4]))
		return s + 4;
	if (!strncmp(s, "if", 2) && !is_id(s[2]))
		return s + 2;
	return NULL;
}

static int expr_is_bare01(const char *s) {
	char kept = 0;
	int nkept = 0;
	while (*s) {
		if (*s == ' ' || *s == '\t') {
			++s;
		} else if (*s == '/' && s[1] == '/') {
			break;
		} else if (*s == '/' && s[1] == '*') {
			for (s += 2; *s && !(*s == '*' && s[1] == '/'); ++s)
				;
			if (*s)
				s += 2;
		} else {
			kept = *s++;
			if (++nkept > 1)
				return 0;
		}
	}
	return nkept == 1 && (kept == '0' || kept == '1');
}

static int scan_file(const char *path, int is_dir, void *ud) {
	char *text, *p;
	int ln = 0, elen;
	(void)ud;
	if (is_dir)
		return 0;
	elen = (int)strlen(path);
	if (!(elen >= 2 && path[elen - 2] == '.' &&
				(path[elen - 1] == 'c' || path[elen - 1] == 'h' ||
				 path[elen - 1] == 's' || path[elen - 1] == 'S')) &&
			!(elen >= 4 && !strcmp(path + elen - 4, ".inc")))
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
		if ((expr = cond_expr(p)) && expr_is_bare01(expr)) {
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
			fprintf(stderr, "deadgate: not a directory: %s\n", roots[i]);
			return 2;
		}
		host_dir_walk(roots[i], 1, scan_file, NULL);
	}

	if (g_violations) {
		fprintf(stderr,
						"dead-gate invariant violated - '#if 0' or bare '#if 1' found\n"
						"(delete the dead branch or gate it on a named macro):\n"
						"%d violation(s) above\n",
						g_violations);
		return 1;
	}
	printf("dead-gate invariant OK: no '#if 0' or bare '#if 1' in the tree\n");
	return 0;
}
