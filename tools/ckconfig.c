#include "toolsupport.h"

static const char *ALLOW_EXTERN[] = {
		"MCC_CONFIG_BACKTRACE_ONLY",
		"MCC_CONFIG_MCCBOOT",
		0};
static const char *ALLOW_DEAD[] = {
		"MCC_CONFIG_UCLIBC",
		"MCC_CONFIG_AUTOCORRECT",
		"MCC_CONFIG_LIBC",
		0};

/* DRIFT(c): every knob that reaches the compiler as -DMCC_CONFIG_X should be
   declared by an mcc_config_node(), so the documented config surface and the
   real one cannot drift apart. Two forms count as declared: an exact
   mcc_config_node(MCC_CONFIG_X), and the DERIVED form where the node is named
   MCC_X and the emitted define picks up the MCC_CONFIG_ prefix (MCC_CPUVER ->
   -DMCC_CONFIG_CPUVER). Everything else has to be listed here with a reason, so
   an undeclared knob is a deliberate, reviewed entry rather than an oversight.
   A name-equality check alone was rejected: it false-positives on every derived
   define, and a gate that cries wolf is worse than no gate. */
static const char *ALLOW_UNDECLARED[] = {
		/* fanned out from the MCC_CONFIG_LIBC node, one define per libc */
		"MCC_CONFIG_MUSL",
		"MCC_CONFIG_UCLIBC",
		/* derived from the install prefix/libdir, not a knob of their own */
		"MCC_CONFIG_MCCDIR",
		"MCC_CONFIG_CROSSPREFIX",
		/* derived from the MCC_CONFIG_DWARF node's value */
		"MCC_CONFIG_DWARF_VERSION",
		/* target-shape constants, pinned by the platform rather than configured */
		"MCC_CONFIG_MACHO_CHAINED_FIXUPS",
		"MCC_CONFIG_RUN_DUALMAP",
		0};

typedef struct {
	char **v;
	int n, cap;
} Set;

static int set_has(const Set *s, const char *k) {
	int i;
	for (i = 0; i < s->n; i++)
		if (!strcmp(s->v[i], k))
			return 1;
	return 0;
}
static void set_add(Set *s, const char *k) {
	if (set_has(s, k))
		return;
	if (s->n == s->cap) {
		s->cap = s->cap ? s->cap * 2 : 32;
		s->v = realloc(s->v, s->cap * sizeof *s->v);
	}
	s->v[s->n++] = strdup(k);
}
static int in_list(const char *const *list, const char *k) {
	int i;
	for (i = 0; list[i]; i++)
		if (!strcmp(list[i], k))
			return 1;
	return 0;
}

static int is_id(int c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				 (c >= '0' && c <= '9') || c == '_';
}

static Set s_read;
static Set s_selfdef;
static Set s_emit;
static Set s_any;
static Set s_node; /* names declared via mcc_config_node() */
static int g_selfdef_only;

static int line_defines(const char *line, char *out, int osz) {
	const char *p = line;
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p != '#')
		return 0;
	p++;
	while (*p == ' ' || *p == '\t')
		p++;
	if (strncmp(p, "define", 6))
		return 0;
	p += 6;
	if (is_id(*p))
		return 0;
	while (*p == ' ' || *p == '\t')
		p++;
	if (strncmp(p, "MCC_CONFIG_", 11))
		return 0;
	int i = 0;
	while (is_id(*p) && i < osz - 1)
		out[i++] = *p++;
	out[i] = 0;
	return 1;
}

static int scan_code(const char *path, int is_dir, void *ud) {
	(void)ud;
	if (is_dir)
		return 0;
	int el = (int)strlen(path);
	if (!(el >= 2 && path[el - 2] == '.' &&
				(path[el - 1] == 'c' || path[el - 1] == 'h')))
		return 0;
	const char *base = strrchr(path, '/');
	base = base ? base + 1 : path;
	if (!strcmp(base, "ckconfig.c"))
		return 0;
	if (g_selfdef_only && !strcmp(base, "build.c"))
		return 0;
	char *text = ts_read_file(path, NULL);
	if (!text)
		return 0;
	char *p = text;
	while (*p) {
		char *e = strchr(p, '\n');
		int len = e ? (int)(e - p) : (int)strlen(p);
		if (len && p[len - 1] == '\r')
			len--;
		p[len] = 0;

		char defname[128];
		int has_def = line_defines(p, defname, sizeof defname);
		if (has_def)
			set_add(&s_selfdef, defname);

		for (char *q = p; (q = strstr(q, "MCC_CONFIG_")) != NULL;) {
			char name[128];
			int i = 0;
			char *r = q;
			while (is_id(*r) && i < (int)sizeof name - 1)
				name[i++] = *r++;
			name[i] = 0;
			if (i > 11 && !g_selfdef_only && !(has_def && !strcmp(name, defname)))
				set_add(&s_read, name);
			q = r;
		}
		if (!e)
			break;
		p = e + 1;
	}
	free(text);
	return 0;
}

static void scan_cmake(const char *path, int emit_ok) {
	char *text = ts_read_file(path, NULL);
	if (!text) {
		fprintf(stderr, "ckconfig: cannot read %s\n", path);
		exit(2);
	}
	char *p = text;
	while (*p) {
		char *e = strchr(p, '\n');
		int len = e ? (int)(e - p) : (int)strlen(p);
		if (len && p[len - 1] == '\r')
			len--;
		p[len] = 0;
		int line_has_defstr = strstr(p, "mcc_def_str") != NULL;
		if (emit_ok) {
			/* mcc_config_node(NAME ...) -- the declared config surface */
			for (char *q = p; (q = strstr(q, "mcc_config_node")) != NULL;) {
				char nm[128];
				int i = 0;
				char *r = q + 15;
				while (*r == ' ' || *r == '\t' || *r == '(')
					r++;
				while (is_id(*r) && i < (int)sizeof nm - 1)
					nm[i++] = *r++;
				nm[i] = 0;
				if (i)
					set_add(&s_node, nm);
				q += 15;
			}
		}
		for (char *q = p; (q = strstr(q, "MCC_CONFIG_")) != NULL;) {
			char name[128];
			int i = 0;
			char *r = q;
			while (is_id(*r) && i < (int)sizeof name - 1)
				name[i++] = *r++;
			name[i] = 0;
			if (i > 11) {
				set_add(&s_any, name);
				if (emit_ok && (*r == '=' || line_has_defstr))
					set_add(&s_emit, name);
			}
			q = r;
		}
		if (!e)
			break;
		p = e + 1;
	}
	free(text);
}

int main(int argc, char **argv) {
	const char *srcdir = "src", *cmake = "CMakeLists.txt";
	int list = 0, ai = 1;
	if (ai < argc && !strcmp(argv[ai], "--list")) {
		list = 1;
		ai++;
	}
	if (ai < argc)
		srcdir = argv[ai++];
	if (ai < argc)
		cmake = argv[ai++];

	int isd;
	if (host_stat(srcdir, &isd, NULL, NULL) || !isd) {
		fprintf(stderr, "ckconfig: not a directory: %s\n", srcdir);
		return 2;
	}
	host_dir_walk(srcdir, 1, scan_code, NULL);
	if (!host_stat("tools", &isd, NULL, NULL) && isd) {
		g_selfdef_only = 1;
		host_dir_walk("tools", 1, scan_code, NULL);
		g_selfdef_only = 0;
	}
	scan_cmake(cmake, 1);
	if (!host_stat("tools/build.c", &isd, NULL, NULL))
		scan_cmake("tools/build.c", 0);

	int errs = 0, i;

	for (i = 0; i < s_read.n; i++) {
		const char *k = s_read.v[i];
		if (set_has(&s_selfdef, k) || set_has(&s_any, k) || in_list(ALLOW_EXTERN, k))
			continue;
		printf("  DRIFT(a): code reads %s but no CMakeLists.txt mention and no "
					 "header #define\n",
					 k);
		errs++;
	}
	for (i = 0; i < s_emit.n; i++) {
		const char *k = s_emit.v[i];
		if (set_has(&s_read, k) || in_list(ALLOW_DEAD, k))
			continue;
		printf("  DRIFT(b): CMake emits -D%s but the code never reads it (dead)\n",
					 k);
		errs++;
	}
	for (i = 0; i < s_emit.n; i++) {
		const char *k = s_emit.v[i];
		char derived[256];
		if (set_has(&s_node, k) || in_list(ALLOW_UNDECLARED, k))
			continue;
		/* derived form: node MCC_X emits -DMCC_CONFIG_X */
		if (!strncmp(k, "MCC_CONFIG_", 11)) {
			snprintf(derived, sizeof derived, "MCC_%s", k + 11);
			if (set_has(&s_node, derived))
				continue;
		}
		printf("  DRIFT(c): CMake emits -D%s but no mcc_config_node() declares it "
					 "(add the node, or list it in ALLOW_UNDECLARED with a reason)\n",
					 k);
		errs++;
	}

	if (list) {
		printf("reads=%d selfdef=%d emit=%d mentioned=%d\n", s_read.n, s_selfdef.n,
					 s_emit.n, s_any.n);
		for (i = 0; i < s_emit.n; i++)
			printf("  emit  %s%s\n", s_emit.v[i],
						 set_has(&s_read, s_emit.v[i]) ? " (read)" : " (DEAD)");
		for (i = 0; i < s_read.n; i++)
			if (!set_has(&s_any, s_read.v[i]))
				printf("  read-only  %s%s\n", s_read.v[i],
							 set_has(&s_selfdef, s_read.v[i]) ? " (#define)" : " (implicit)");
	}

	if (errs) {
		fprintf(stderr,
						"ckconfig: %d config-drift issue(s) — reconcile CMakeLists.txt, the "
						"code, EXCESS.md (config surface), and the ALLOW_* lists in tools/ckconfig.c\n",
						errs);
		return 1;
	}
	printf("ckconfig OK: MCC_CONFIG_* surface consistent (%d emitted, %d read, "
				 "%d header-defined)\n",
				 s_emit.n, s_read.n, s_selfdef.n);
	return 0;
}
