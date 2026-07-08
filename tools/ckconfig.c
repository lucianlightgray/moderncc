/* ckconfig — config-drift checker for the CONFIG_MCC_* preprocessor surface.
 *
 * mcc's build-time configuration flows CMake option -> emitted `-DCONFIG_MCC_X`
 * -> `#if CONFIG_MCC_X` in the code, with an in-code `#ifndef`-guarded default in
 * a header. This tool cross-checks the two ends so the mapping cannot rot:
 *
 *   (a) a CONFIG_MCC_X the code *reads* but that neither CMakeLists.txt mentions
 *       nor a header `#define`s (an implicit/undefined config), and
 *   (b) a CONFIG_MCC_X CMakeLists.txt *emits* as a -D but the code never reads
 *       (a dead emission).
 *
 * Known-intentional exceptions are listed in ALLOW_* below with a rationale.
 * Names a header `#define`s are code-internal (an unconditional constant or an
 * overridable default) and are never "undefined". Run: `ckconfig <src> <cmake>`.
 * `--list` prints the full inventory (used to keep docs/CONFIG.md honest).
 *
 * Companion to docs/CONFIG.md (the human-readable surface) and BUILD.md (the
 * CMake node table). Mirrors tools/hostgate.c's file-walking style. */
#include "toolsupport.h"

/* CONFIG_MCC_* the code reads with no CMake node — intentional, external. */
static const char *ALLOW_EXTERN[] = {
		/* opt-in "backtrace-only" runtime build variant; #ifndef-graceful, set
		   outside the CMake config surface by that specialized build. */
		"CONFIG_MCC_BACKTRACE_ONLY",
		0};
/* CONFIG_MCC_* CMake emits but the code no longer reads — intentional, benign. */
static const char *ALLOW_DEAD[] = {
		/* legacy uClibc marker; emitted for provenance, "no path effect on modern
		   hosts" (CMakeLists.txt), so nothing reads it. */
		"CONFIG_MCC_UCLIBC",
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

/* Sets collected from the two ends. */
static Set s_read;    /* CONFIG_MCC_* the compiler (src/) references            */
static Set s_selfdef; /* CONFIG_MCC_* a header #defines (default/constant)      */
static Set s_emit;    /* CONFIG_MCC_* CMakeLists.txt emits as a -D              */
static Set s_any;     /* CONFIG_MCC_* any provider mentions (CMake or build.c)  */
static int g_selfdef_only; /* when scanning tools/: collect #defines, not reads */

/* Return the #define'd name if `line` (trimmed) is `#define CONFIG_MCC_X ...`. */
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
	if (strncmp(p, "CONFIG_MCC_", 11))
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
	if (!strcmp(base, "ckconfig.c")) /* our own placeholder literals */
		return 0;
	/* build.c is an *emitter* (like CMakeLists.txt), not a reader — its EMIT()
	   string literals are scanned as provider mentions, not code reads. */
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

		/* Every CONFIG_MCC_* occurrence that is not the #define'd name is a read. */
		for (char *q = p; (q = strstr(q, "CONFIG_MCC_")) != NULL;) {
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

/* Scan a provider (CMakeLists.txt or tools/build.c) for CONFIG_MCC_* mentions.
   emit_ok records the -D emissions too (used only for CMakeLists.txt's dead
   check); build.c is scanned for mentions only. */
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
		for (char *q = p; (q = strstr(q, "CONFIG_MCC_")) != NULL;) {
			char name[128];
			int i = 0;
			char *r = q;
			while (is_id(*r) && i < (int)sizeof name - 1)
				name[i++] = *r++;
			name[i] = 0;
			if (i > 11) {
				set_add(&s_any, name);
				/* Emitted as a -D when it is `CONFIG_MCC_X=...` (value) or appears on
				   an mcc_def_str(...) line (string-valued define). */
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
	/* tools/ headers may #define code-internal CONFIG_MCC_* (e.g. TOOLHOST);
	   collect those #defines only, not reads (build.c is an emitter). */
	if (!host_stat("tools", &isd, NULL, NULL) && isd) {
		g_selfdef_only = 1;
		host_dir_walk("tools", 1, scan_code, NULL);
		g_selfdef_only = 0;
	}
	scan_cmake(cmake, 1);
	/* tools/build.c is the second (mccbuild) emitter — count its mentions as a
	   provider so a config it (but not CMake) supplies isn't flagged implicit. */
	if (!host_stat("tools/build.c", &isd, NULL, NULL))
		scan_cmake("tools/build.c", 0);

	int errs = 0, i;

	/* (a) code reads a config nobody defines. */
	for (i = 0; i < s_read.n; i++) {
		const char *k = s_read.v[i];
		if (set_has(&s_selfdef, k) || set_has(&s_any, k) || in_list(ALLOW_EXTERN, k))
			continue;
		printf("  DRIFT(a): code reads %s but no CMakeLists.txt mention and no "
					 "header #define\n",
					 k);
		errs++;
	}
	/* (b) CMake emits a -D the code never reads. */
	for (i = 0; i < s_emit.n; i++) {
		const char *k = s_emit.v[i];
		if (set_has(&s_read, k) || in_list(ALLOW_DEAD, k))
			continue;
		printf("  DRIFT(b): CMake emits -D%s but the code never reads it (dead)\n",
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
						"code, docs/CONFIG.md, and the ALLOW_* lists in tools/ckconfig.c\n",
						errs);
		return 1;
	}
	printf("ckconfig OK: CONFIG_MCC_* surface consistent (%d emitted, %d read, "
				 "%d header-defined)\n",
				 s_emit.n, s_read.n, s_selfdef.n);
	return 0;
}
