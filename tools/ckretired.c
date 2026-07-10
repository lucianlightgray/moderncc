#include "toolsupport.h"

static const char *RETIRED[] = {
		"TARGET_DEFS_ONLY", "MCC_DISABLE_ASM", "ELF_OBJ_ONLY", "MCC_IS_NATIVE",
		"PROMOTE_RET", "SINGLE_SOURCE", "TAL_INFO", "NEED_RELOC_TYPE",
		"NEED_BUILD_GOT", "PE_PRINT_SECTIONS", "DEBUG_VERSION", "DEBUG_RELOC",
		"INC_DEBUG", "PP_DEBUG", "BF_DEBUG", "ASM_DEBUG",
		"MCC_CONFIG_BCHECK", "MCC_CONFIG_BACKTRACE", 0};

static int g_violations;

static int is_id(int c) {
	return isalnum((unsigned char)c) || c == '_';
}

static int tok_banned(const char *b, int len) {
	int i;
	if (len > 7 && !strncmp(b, "CONFIG_", 7) && isupper((unsigned char)b[7]))
		return 1;
	for (i = 0; RETIRED[i]; ++i)
		if ((int)strlen(RETIRED[i]) == len && !strncmp(RETIRED[i], b, len))
			return 1;
	return 0;
}

static int scan_file(const char *path, int is_dir, void *ud) {
	const char *base;
	char *text, *p;
	int ln = 1, elen;
	(void)ud;
	if (is_dir)
		return 0;
	elen = (int)strlen(path);
	if (!(elen >= 2 && path[elen - 2] == '.' &&
				(path[elen - 1] == 'c' || path[elen - 1] == 'h' ||
				 path[elen - 1] == 's' || path[elen - 1] == 'S')) &&
			!(elen >= 4 && !strcmp(path + elen - 4, ".inc")) &&
			!(elen >= 4 && !strcmp(path + elen - 4, ".txt")))
		return 0;
	base = strrchr(path, '/');
	base = base ? base + 1 : path;
	if (!strcmp(base, "ckretired.c"))
		return 0;
	if (!(text = ts_read_file(path, NULL)))
		return 0;
	for (p = text; *p;) {
		if (*p == '\n') {
			++ln, ++p;
		} else if (isalpha((unsigned char)*p) || *p == '_') {
			const char *b = p;
			while (is_id(*p))
				++p;
			if (tok_banned(b, (int)(p - b))) {
				printf("  %s:%d: %.*s\n", path, ln, (int)(p - b), b);
				g_violations++;
			}
		} else {
			++p;
		}
	}
	free(text);
	return 0;
}

int main(int argc, char **argv) {
	static const char *defaults[] = {"src", "tools", "runtime", "CMakeLists.txt", 0};
	const char *const *roots = argc > 1 ? (const char *const *)(argv + 1) : defaults;
	int i, n = argc > 1 ? argc - 1 : 4;

	for (i = 0; i < n; ++i) {
		int isd = 0;
		if (host_stat(roots[i], &isd, NULL, NULL)) {
			fprintf(stderr, "ckretired: cannot stat: %s\n", roots[i]);
			return 2;
		}
		if (isd)
			host_dir_walk(roots[i], 1, scan_file, NULL);
		else
			scan_file(roots[i], 0, NULL);
	}
	if (g_violations) {
		printf("ckretired: %d retired/unprefixed gate name(s) found\n", g_violations);
		return 1;
	}
	printf("ckretired OK: no retired or unprefixed gate names\n");
	return 0;
}
