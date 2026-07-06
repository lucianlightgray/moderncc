#include "toolsupport.h"

static const char *EXCL_PREFIX[] = {"cmake-", "build-", 0};
static const char *EXCL_EXACT[] = {"vendor", "dist", ".git", 0};

static int excluded(const char *base) {
	int i;
	for (i = 0; EXCL_PREFIX[i]; i++)
		if (!strncmp(base, EXCL_PREFIX[i], strlen(EXCL_PREFIX[i])))
			return 1;
	for (i = 0; EXCL_EXACT[i]; i++)
		if (!strcmp(base, EXCL_EXACT[i]))
			return 1;
	return 0;
}

struct stagectx {
	const char *dst;
};

static int stage_cb(const char *path, int is_dir, void *ud) {
	struct stagectx *c = ud;
	const char *base = strrchr(path, '/');
	char dstpath[8192];
	base = base ? base + 1 : path;
	ts_path(dstpath, sizeof dstpath, c->dst, "%s", base);
	if (is_dir) {
		struct stagectx nc;
		if (excluded(base))
			return 0;
		host_mkdirs(dstpath);
		nc.dst = dstpath;
		host_dir_walk(path, 0, stage_cb, &nc);
	} else {
		if (host_copy_file(path, dstpath, 1))
			fprintf(stderr, "ci: copy failed: %s\n", path);
	}
	return 0;
}

static int has_suffix(const char *s, const char *suf) {
	size_t ls = strlen(s), lf = strlen(suf);
	return ls >= lf && !strcmp(s + ls - lf, suf);
}

static int normalize_cb(const char *path, int is_dir, void *ud) {
	static const char *EXT[] = {".c", ".h", ".cmake", ".txt", ".S", ".def", 0};
	long n, i, o = 0;
	char *buf;
	int touched = 0, k;
	(void)ud;
	if (is_dir)
		return 0;
	for (k = 0; EXT[k]; k++)
		if (has_suffix(path, EXT[k]))
			break;
	if (!EXT[k])
		return 0;
	if (!(buf = ts_read_file(path, &n)))
		return 0;
	for (i = 0; i < n; i++) {
		if (buf[i] == '\r' && (i + 1 == n || buf[i + 1] == '\n')) {
			touched = 1;
			continue;
		}
		buf[o++] = buf[i];
	}
	if (touched) {
		FILE *f = fopen(path, "wb");
		if (f) {
			fwrite(buf, 1, o, f);
			fclose(f);
		}
	}
	free(buf);
	return 0;
}

static int do_stage(int argc, char **argv) {
	struct stagectx c;
	if (argc != 2) {
		fprintf(stderr, "usage: ci stage <src> <dst>\n");
		return 2;
	}
	printf("==> staging %s -> %s\n", argv[0], argv[1]);
	if (host_mkdirs(argv[1])) {
		fprintf(stderr, "ci: cannot create %s\n", argv[1]);
		return 1;
	}
	c.dst = argv[1];
	host_dir_walk(argv[0], 0, stage_cb, &c);
	printf("==> normalizing line endings (CRLF -> LF) in staged sources\n");
	host_dir_walk(argv[1], 1, normalize_cb, NULL);
	return 0;
}

static int do_run_preset(int argc, char **argv) {
	const char *preset = NULL, *out = NULL, *config = NULL;
	char jflag[32], instdir[4096], prefix[4096];
	int i, extra_start = argc, no_test = 0, do_install = 0;
	int jobs = host_nproc();

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--out") && i + 1 < argc)
			out = argv[++i];
		else if (!strcmp(argv[i], "--config") && i + 1 < argc)
			config = argv[++i];
		else if (!strcmp(argv[i], "--no-test"))
			no_test = 1;
		else if (!strcmp(argv[i], "--install"))
			do_install = 1;
		else if (!strcmp(argv[i], "--")) {
			extra_start = i + 1;
			break;
		} else if (!preset && argv[i][0] != '-')
			preset = argv[i];
	}
	if (!preset) {
		fprintf(stderr, "usage: ci run-preset <name> [--out DIR] [--install] [--no-test] [--config C] [-- <ctest args>]\n");
		return 2;
	}
	snprintf(jflag, sizeof jflag, "-j%d", jobs > 0 ? jobs : 1);

	{
		Argv v = {{0}, 0};
		ts_arg(&v, "cmake");
		ts_arg(&v, "--preset");
		ts_arg(&v, preset);
		if (out) {
			snprintf(prefix, sizeof prefix, "-DCMAKE_INSTALL_PREFIX=%s", out);
			ts_arg(&v, prefix);
		}
		printf("==> configuring (preset=%s)\n", preset);
		if (ts_run(ts_argz(&v)))
			return 1;
	}

	{
		Argv v = {{0}, 0};
		ts_arg(&v, "cmake");
		ts_arg(&v, "--build");
		ts_arg(&v, "--preset");
		ts_arg(&v, preset);
		ts_arg(&v, jflag);
		if (config) {
			ts_arg(&v, "--config");
			ts_arg(&v, config);
		}
		printf("==> building (%s)\n", jflag);
		if (ts_run(ts_argz(&v)))
			return 1;
	}

	if (!no_test) {
		Argv v = {{0}, 0};
		char junit[4200];
		ts_arg(&v, "ctest");
		ts_arg(&v, "--preset");
		ts_arg(&v, preset);
		ts_arg(&v, jflag);
		if (config) {
			ts_arg(&v, "--build-config");
			ts_arg(&v, config);
		}
		snprintf(junit, sizeof junit, "cmake-%s/ctest-junit.xml", preset);
		ts_arg(&v, "--output-junit");
		ts_arg(&v, junit);
		for (i = extra_start; i < argc; i++)
			ts_arg(&v, argv[i]);
		printf("==> testing (preset=%s)\n", preset);
		if (ts_run(ts_argz(&v)))
			return 1;
	}

	if (out || do_install) {
		Argv v = {{0}, 0};
		snprintf(instdir, sizeof instdir, "cmake-%s", preset);
		ts_arg(&v, "cmake");
		ts_arg(&v, "--install");
		ts_arg(&v, instdir);
		if (config) {
			ts_arg(&v, "--config");
			ts_arg(&v, config);
		}
		printf("==> installing (preset=%s)%s%s\n", preset, out ? " -> " : "", out ? out : "");
		if (ts_run(ts_argz(&v)))
			return 1;
	}
	return 0;
}

static void qemu_fixup_multilib(const char *dldir) {
	static const char *libcs[] = {"glibc", "musl", 0};
	static const char *objs[] = {"crt1.o",  "crti.o",  "crtn.o",
	                             "Scrt1.o", "gcrt1.o", "Mcrt1.o", 0};
	char root[4096], src[4096], dst[4096];
	int i, j, isd;

	for (i = 0; libcs[i]; i++) {
		ts_path(root, sizeof root, dldir, "gentoo-stage3-x86_64-%s", libcs[i]);
		if (host_stat(root, &isd, NULL, NULL) || !isd)
			continue;
		ts_path(src, sizeof src, root, "usr/lib64/crt1.o");
		if (host_stat(src, NULL, NULL, NULL))
			continue;
		for (j = 0; objs[j]; j++) {
			ts_path(src, sizeof src, root, "usr/lib64/%s", objs[j]);
			ts_path(dst, sizeof dst, root, "usr/lib/%s", objs[j]);
			if (host_stat(src, NULL, NULL, NULL) == 0)
				host_copy_file(src, dst, 0);
		}
		printf("==> fixed multilib crt in gentoo-stage3-x86_64-%s\n", libcs[i]);
	}
}

static int do_qemu(int argc, char **argv) {
	const char *preset = getenv("PRESET");
	const char *archs = getenv("ARCHS");
	const char *libcs = getenv("LIBCS");
	const char *dldir = getenv("MCC_QEMU_DLDIR");
	char build[4096], dover[4096], aover[4096], lover[4096], jflag[32];
	int jobs = host_nproc(), i;

	if (!preset || !*preset)
		preset = "qemu";
	snprintf(jflag, sizeof jflag, "-j%d", jobs > 0 ? jobs : 1);
	snprintf(build, sizeof build, "cmake-%s", preset);

	{
		Argv v = {{0}, 0};
		ts_arg(&v, "cmake");
		ts_arg(&v, "--preset");
		ts_arg(&v, preset);
		if (dldir && *dldir) {
			snprintf(dover, sizeof dover, "-DMCC_QEMU_DLDIR=%s", dldir);
			ts_arg(&v, dover);
		}
		if (archs && *archs) {
			snprintf(aover, sizeof aover, "-DMCC_QEMU_ARCHS=%s", archs);
			ts_arg(&v, aover);
		}
		if (libcs && *libcs) {
			snprintf(lover, sizeof lover, "-DMCC_QEMU_LIBCS=%s", libcs);
			ts_arg(&v, lover);
		}
		printf("==> configuring (preset=%s)\n", preset);
		if (ts_run(ts_argz(&v)))
			return 1;
	}
	{
		Argv v = {{0}, 0};
		ts_arg(&v, "cmake");
		ts_arg(&v, "--build");
		ts_arg(&v, "--preset");
		ts_arg(&v, preset);
		ts_arg(&v, jflag);
		printf("==> building (%s)\n", jflag);
		if (ts_run(ts_argz(&v)))
			return 1;
	}
	{
		Argv v = {{0}, 0};
		ts_arg(&v, "ctest");
		ts_arg(&v, "--test-dir");
		ts_arg(&v, build);
		ts_arg(&v, "-R");
		ts_arg(&v, "qemu-x86_64-.*-fetch");
		ts_arg(&v, "--output-on-failure");
		printf("==> pre-fetching x86_64 sysroots for multilib fixup\n");
		ts_run(ts_argz(&v));
	}
	qemu_fixup_multilib((dldir && *dldir) ? dldir : "vendor");
	{
		Argv v = {{0}, 0};
		char junit[4200];
		ts_arg(&v, "ctest");
		ts_arg(&v, "--preset");
		ts_arg(&v, preset);
		snprintf(junit, sizeof junit, "%s/ctest-junit.xml", build);
		ts_arg(&v, "--output-junit");
		ts_arg(&v, junit);
		for (i = 0; i < argc; i++)
			ts_arg(&v, argv[i]);
		printf("==> running qemu matrix (preset=%s)\n", preset);
		if (ts_run(ts_argz(&v)))
			return 1;
	}
	return 0;
}


#define LOC_MAX 64
typedef struct {
	char preset[64], cc[16], plat[64];
} LocJob;

static int loc_have(const char *name) {
	char buf[4096];
	return host_find_tool(name, ".exe", buf, sizeof buf);
}

static void loc_setcc(const char *cc) {
#if MCC_HOST_WIN32
	char buf[80];
	snprintf(buf, sizeof buf, "CC=%s", (cc && *cc) ? cc : "");
	_putenv(buf);
#else
	if (cc && *cc)
		setenv("CC", cc, 1);
	else
		unsetenv("CC");
#endif
}

static int loc_env_on(const char *var) {
	const char *v = getenv(var);
	return v && *v && strcmp(v, "0");
}

static int run_dist(const char *preset, const char *plat, const char *ver,
                    char **extra, int n_extra) {
	char pdv[256], pdp[256], bdir[128];
	int msvc = strstr(preset, "msvc") != NULL, i;

	snprintf(pdv, sizeof pdv, "-DMCC_DIST_VERSION=%.200s", ver);
	snprintf(pdp, sizeof pdp, "-DMCC_DIST_PLAT=%.63s", plat);
	snprintf(bdir, sizeof bdir, "cmake-%.63s", preset);
	{
		Argv v = {{0}, 0};
		ts_arg(&v, "cmake");
		ts_arg(&v, "--preset");
		ts_arg(&v, preset);
		ts_arg(&v, pdv);
		ts_arg(&v, pdp);
		ts_arg(&v, "-DMCC_BENCH=ON");
		for (i = 0; i < n_extra; i++)
			ts_arg(&v, extra[i]);
		if (ts_run(ts_argz(&v)))
			return 1;
	}
	{
		const char *a[] = {"cmake", "--build", "--preset", preset, "-j", 0};
		if (ts_run(a))
			return 1;
	}
	{
		const char *a[] = {"cmake",  "--install", bdir,
		                   "--config", "Release",  0};
		if (!msvc)
			a[3] = 0;
		if (ts_run(a))
			return 1;
	}
#if MCC_HOST_DARWIN
	{
		char *g[64];
		int ng = ts_glob("dist/bin", "mcc*", 0, g, 64), k;
		for (k = 0; k < ng && k < 64; k++) {
			const char *a[] = {"strip", "-x", g[k], 0};
			ts_run(a);
			free(g[k]);
		}
	}
#endif
	{
		const char *a[] = {"cmake", "--build",       "--preset",
		                   preset, "--target", "bench", 0};
		if (ts_run(a))
			return 1;
	}
	{
		const char *a[] = {"cmake", "--build",         "--preset",
		                   preset, "--target", "package-dist", 0};
		if (ts_run(a))
			return 1;
	}
	return 0;
}

static int do_local(int argc, char **argv) {
	const char *ver = "v0.0.0-local", *host_cpu = "unknown";
	const char *only = getenv("LOCAL_CI_ONLY");
	LocJob test[LOC_MAX], dist[LOC_MAX];
	char skip[LOC_MAX][160], results[LOC_MAX * 2][192];
	int n_test = 0, n_dist = 0, n_skip = 0, n_res = 0, n_fail = 0;
	int keep_going = 1, i, stop = 0;
	static const char *QARCH[] = {"x86_64", "i386", "arm", "arm64", "riscv64", 0};
	static const char *QBIN[] = {"qemu-x86_64", "qemu-i386",  "qemu-arm",
	                             "qemu-aarch64", "qemu-riscv64", 0};
	int have_gcc, have_clang, have_cl, have_wine, have_mingw, have_docker;
	int qfound[8], n_qemu = 0;
#if MCC_HOST_WIN32
	const char *os = "Windows";
#elif MCC_HOST_DARWIN
	const char *os = "Darwin";
#else
	const char *os = "Linux";
#endif

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--version") && i + 1 < argc)
			ver = argv[++i];
		else if (!strcmp(argv[i], "--host-cpu") && i + 1 < argc)
			host_cpu = argv[++i];
	}
	{
		const char *k = getenv("LOCAL_CI_KEEP_GOING");
		keep_going = (k && !strcmp(k, "0")) ? 0 : 1;
	}

	have_gcc = loc_have("gcc");
	have_clang = loc_have("clang");
	have_cl = loc_have("cl");
	have_wine = loc_have("wine") || loc_have("wine64");
	have_mingw = loc_have("x86_64-w64-mingw32-gcc");
	have_docker = loc_have("docker");
	for (i = 0; QARCH[i]; i++) {
		qfound[i] = loc_have(QBIN[i]);
		if (qfound[i])
			n_qemu++;
	}

#define LOC_TEST(P, CC)                                                        \
	do {                                                                   \
		if (n_test < LOC_MAX) {                                        \
			snprintf(test[n_test].preset, 64, "%s", (P));         \
			snprintf(test[n_test].cc, 16, "%s", (CC));            \
			n_test++;                                             \
		}                                                             \
	} while (0)
#define LOC_SKIP(FMT, ...)                                                     \
	do {                                                                   \
		if (n_skip < LOC_MAX)                                          \
			snprintf(skip[n_skip++], 160, FMT, __VA_ARGS__);      \
	} while (0)
#define LOC_DIST(P, CC, PLAT)                                                  \
	do {                                                                   \
		if (n_dist < LOC_MAX) {                                        \
			snprintf(dist[n_dist].preset, 64, "%s", (P));         \
			snprintf(dist[n_dist].cc, 16, "%s", (CC));            \
			snprintf(dist[n_dist].plat, 64, "%s", (PLAT));        \
			n_dist++;                                             \
		}                                                             \
	} while (0)

	if (!strcmp(os, "Linux")) {
		static const char *G[] = {
		    "linux-gcc",           "linux-gcc-cross",
		    "linux-gcc-release",   "linux-gcc-musl",
		    "linux-gcc-static",    "linux-gcc-multisource",
		    "linux-gcc-asm-off",   "linux-gcc-predefs-off",
		    "linux-gcc-pie",       "linux-gcc-dwarf",
		    "linux-gcc-diagnostics", "linux-gcc-sanitize", 0};
		static const char *C[] = {"linux-clang", "linux-clang-cross",
		                          "linux-clang-release", 0};
		for (i = 0; G[i]; i++) {
			if (have_gcc)
				LOC_TEST(G[i], "");
			else
				LOC_SKIP("%s - gcc not found", G[i]);
		}
		for (i = 0; C[i]; i++) {
			if (have_clang)
				LOC_TEST(C[i], "");
			else
				LOC_SKIP("%s - clang not found", C[i]);
		}
	} else if (!strcmp(os, "Darwin")) {
		static const char *M[] = {"macos", "macos-cross", 0};
		for (i = 0; M[i]; i++) {
			if (have_clang)
				LOC_TEST(M[i], "clang");
			if (have_gcc)
				LOC_TEST(M[i], "gcc");
			if (!have_clang && !have_gcc)
				LOC_SKIP("%s - no C compiler (need clang or gcc)", M[i]);
		}
	} else if (!strcmp(os, "Windows")) {
		if (have_cl)
			LOC_TEST("msvc", "");
		else
			LOC_SKIP("%s - cl (MSVC) not found (run from a VS dev shell)", "msvc");
		LOC_TEST("mingw", "");
	}

	if (!loc_env_on("LOCAL_CI_SKIP_QEMU")) {
		for (i = 0; QARCH[i]; i++) {
			char p[64];
			if (qfound[i]) {
				snprintf(p, sizeof p, "qemu-%s", QARCH[i]);
				LOC_TEST(p, "");
			} else {
				LOC_SKIP("qemu-%s - %s not found (install qemu-user)",
				         QARCH[i], QBIN[i]);
			}
		}
	} else {
		LOC_SKIP("%s", "qemu-* - skipped (LOCAL_CI_SKIP_QEMU)");
	}

	if (!loc_env_on("LOCAL_CI_SKIP_RELEASE")) {
		char plat[64];
		if (!strcmp(os, "Linux")) {
			if (have_gcc) {
				snprintf(plat, sizeof plat, "linux-%s-gcc", host_cpu);
				LOC_DIST("dist-linux-gcc", "", plat);
			}
			if (have_clang) {
				snprintf(plat, sizeof plat, "linux-%s-clang", host_cpu);
				LOC_DIST("dist-linux-clang", "", plat);
			}
		} else if (!strcmp(os, "Darwin")) {
			snprintf(plat, sizeof plat, "macos-%s-clang", host_cpu);
			LOC_DIST("dist-macos", "clang", plat);
		} else if (!strcmp(os, "Windows")) {
			if (have_cl) {
				snprintf(plat, sizeof plat, "windows-%s-msvc", host_cpu);
				LOC_DIST("dist-msvc", "", plat);
			}
			snprintf(plat, sizeof plat, "windows-%s-mingw", host_cpu);
			LOC_DIST("dist-mingw", "", plat);
		}
	} else {
		LOC_SKIP("%s", "dist-* - skipped (LOCAL_CI_SKIP_RELEASE)");
	}

	if (only && *only) {
		int k = 0;
		for (i = 0; i < n_test; i++)
			if (strstr(test[i].preset, only))
				test[k++] = test[i];
		n_test = k;
		k = 0;
		for (i = 0; i < n_dist; i++)
			if (strstr(dist[i].preset, only))
				dist[k++] = dist[i];
		n_dist = k;
	}

	printf("\n==================== local CI: host probe ====================\n");
	printf("  host            : %s / %s\n", os, host_cpu);
	printf("  gcc             : %s\n", have_gcc ? "yes" : "no");
	printf("  clang           : %s\n", have_clang ? "yes" : "no");
	printf("  msvc (cl)       : %s\n", have_cl ? "yes" : "no");
	printf("  wine            : %s   (drives pe-wine-conformance in *-cross)\n",
	       have_wine ? "yes" : "no");
	printf("  mingw cross gcc : %s\n", have_mingw ? "yes" : "no");
	printf("  docker          : %s\n", have_docker ? "yes" : "no");
	printf("  qemu-user       : %d arch(es)\n", n_qemu);
	printf("  --------------------------------------------------------\n");
	printf("  test presets    : %d\n", n_test);
	for (i = 0; i < n_test; i++)
		printf("      run   %s%s%s\n", test[i].preset,
		       *test[i].cc ? " CC=" : "", test[i].cc);
	printf("  release bundles : %d\n", n_dist);
	for (i = 0; i < n_dist; i++)
		printf("      dist  %s -> %s\n", dist[i].preset, dist[i].plat);
	printf("  skipped         : %d\n", n_skip);
	for (i = 0; i < n_skip; i++)
		printf("      skip  %s\n", skip[i]);
	printf("==============================================================\n\n");

	if (loc_env_on("LOCAL_CI_LIST")) {
		printf("LOCAL_CI_LIST set - plan only, nothing run.\n");
		return 0;
	}

	for (i = 0; i < n_test && !stop; i++) {
		char *a[1];
		int rc;
		loc_setcc(test[i].cc);
		printf("\n>>>> [test] %s%s%s\n", test[i].preset,
		       *test[i].cc ? " CC=" : "", test[i].cc);
		a[0] = test[i].preset;
		rc = do_run_preset(1, a);
		if (rc == 0) {
			snprintf(results[n_res++], 192, "PASS  test %.63s", test[i].preset);
		} else {
			snprintf(results[n_res++], 192, "FAIL  test %.63s (exit %d)",
			         test[i].preset, rc);
			n_fail++;
			if (!keep_going)
				stop = 1;
		}
	}
	for (i = 0; i < n_dist && !stop; i++) {
		int rc;
		printf("\n>>>> [dist] %s -> %s\n", dist[i].preset, dist[i].plat);
		rc = run_dist(dist[i].preset, dist[i].plat, ver, NULL, 0);
		if (rc == 0) {
			snprintf(results[n_res++], 192, "PASS  dist %.63s -> %.63s",
			         dist[i].preset, dist[i].plat);
		} else {
			snprintf(results[n_res++], 192, "FAIL  dist %.63s -> %.63s (exit %d)",
			         dist[i].preset, dist[i].plat, rc);
			n_fail++;
			if (!keep_going)
				stop = 1;
		}
	}

	printf("\n==================== local CI: summary =======================\n");
	for (i = 0; i < n_res; i++)
		printf("  %s\n", results[i]);
	for (i = 0; i < n_skip; i++)
		printf("  SKIP  %s\n", skip[i]);
	printf("==============================================================\n");
	if (stop)
		printf("  (stopped early: LOCAL_CI_KEEP_GOING=0)\n");
	if (n_fail > 0) {
		fprintf(stderr, "local CI: %d/%d job(s) FAILED\n", n_fail, n_res);
		return 1;
	}
	printf("local CI: all %d job(s) passed.\n", n_res);
	return 0;
}

#undef LOC_TEST
#undef LOC_SKIP
#undef LOC_DIST

static int do_dist(int argc, char **argv) {
	const char *preset = NULL, *plat = NULL, *ver = "v0.0.0-local";
	char **extra = NULL;
	int n_extra = 0, i;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--preset") && i + 1 < argc)
			preset = argv[++i];
		else if (!strcmp(argv[i], "--plat") && i + 1 < argc)
			plat = argv[++i];
		else if (!strcmp(argv[i], "--version") && i + 1 < argc)
			ver = argv[++i];
		else if (!strcmp(argv[i], "--")) {
			extra = &argv[i + 1];
			n_extra = argc - (i + 1);
			break;
		}
	}
	if (!preset || !plat) {
		fprintf(stderr, "usage: ci dist --preset P --plat PLAT [--version V] "
		                "[-- <extra -D configure args>]\n");
		return 2;
	}
	return run_dist(preset, plat, ver, extra, n_extra);
}

static const char *g_filter;
static int g_json, g_json_first = 1;
static void emit_preset(const char *b, const char *e) {
	const char *nm = NULL, *p;
	int hidden = 0;
	for (p = b; p + 6 <= e; p++) {
		if (!nm && !strncmp(p, "\"name\"", 6)) {
			const char *q = p + 6;
			while (q < e && *q != '"')
				q++;
			if (q < e) {
				const char *s = ++q;
				while (q < e && *q != '"')
					q++;
				nm = s;
				b = q;
			}
		}
		if (!strncmp(p, "\"hidden\"", 8)) {
			const char *q = p + 8;
			while (q < e && *q != ':')
				q++;
			while (q < e && (*q == ':' || *q == ' ' || *q == '\t'))
				q++;
			if (q < e && !strncmp(q, "true", 4))
				hidden = 1;
		}
	}
	if (nm && !hidden) {
		const char *q = nm;
		int len;
		while (*q != '"')
			q++;
		len = (int)(q - nm);
		if (g_filter && strncmp(nm, g_filter, strlen(g_filter)))
			return;
		if (g_json) {
			printf("%s\"%.*s\"", g_json_first ? "" : ",", len, nm);
			g_json_first = 0;
		} else
			printf("%.*s\n", len, nm);
	}
}

static int do_matrix(int argc, char **argv) {
	const char *file = NULL;
	char *text, *p, *arr;
	int instr = 0, esc = 0, depth = 0, i;
	const char *obj_start = NULL;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--filter") && i + 1 < argc)
			g_filter = argv[++i];
		else if (!strcmp(argv[i], "--json"))
			g_json = 1;
		else if (!file)
			file = argv[i];
	}
	if (!file)
		file = "CMakePresets.json";
	text = ts_read_file(file, NULL);
	if (!text) {
		fprintf(stderr, "ci: cannot read %s\n", file);
		return 2;
	}
	if (!(arr = strstr(text, "\"configurePresets\""))) {
		free(text);
		return 0;
	}
	p = strchr(arr, '[');
	if (!p) {
		free(text);
		return 0;
	}
	if (g_json)
		printf("[");
	for (++p; *p; p++) {
		char c = *p;
		if (instr) {
			if (esc)
				esc = 0;
			else if (c == '\\')
				esc = 1;
			else if (c == '"')
				instr = 0;
			continue;
		}
		if (c == '"') {
			instr = 1;
			continue;
		}
		if (c == '{') {
			if (depth == 0)
				obj_start = p;
			depth++;
		} else if (c == '}') {
			if (--depth == 0 && obj_start) {
				emit_preset(obj_start, p + 1);
				obj_start = NULL;
			}
		} else if (c == ']' && depth == 0)
			break;
	}
	if (g_json)
		printf("]\n");
	free(text);
	return 0;
}

struct mergectx {
	FILE *out;
};

static int merge_cb(const char *path, int is_dir, void *ud) {
	struct mergectx *m = ud;
	const char *base = strrchr(path, '/');
	char *text;
	long n;
	base = base ? base + 1 : path;
	if (is_dir)
		return 0;
	if (strncmp(base, "checksums-", 10) || !has_suffix(base, ".txt"))
		return 0;
	if ((text = ts_read_file(path, &n))) {
		fwrite(text, 1, n, m->out);
		free(text);
	}
	return 0;
}

static int do_sha256sums(int argc, char **argv) {
	const char *dir = NULL, *out = "SHA256SUMS.txt";
	struct mergectx m;
	int i;
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--out") && i + 1 < argc)
			out = argv[++i];
		else if (!dir)
			dir = argv[i];
	}
	if (!dir) {
		fprintf(stderr, "usage: ci sha256sums <dir> [--out FILE]\n");
		return 2;
	}
	if (!(m.out = fopen(out, "wb"))) {
		fprintf(stderr, "ci: cannot write %s\n", out);
		return 2;
	}
	host_dir_walk(dir, 1, merge_cb, &m);
	fclose(m.out);
	printf("ci: merged checksums-*.txt -> %s\n", out);
	return 0;
}

static const char *HOST_EXES[] = {
	"mcc", "mcc-static", "mcc-dynamic", "mcc-musl", "mcc-static-musl", "mcc-dynamic-musl", 0};

static int is_host_exe(const char *base) {
	char nm[256];
	size_t l = strlen(base);
	int i;
	if (l > 4 && !strcmp(base + l - 4, ".exe")) {
		snprintf(nm, sizeof nm, "%.*s", (int)(l - 4), base);
		base = nm;
	}
	for (i = 0; HOST_EXES[i]; i++)
		if (!strcmp(base, HOST_EXES[i]))
			return 1;
	return 0;
}

struct pkgcopy {
	const char *dst;
};
static int pkg_copy_cb(const char *path, int is_dir, void *ud) {
	struct pkgcopy *c = ud;
	const char *base = strrchr(path, '/');
	char dp[8192];
	base = base ? base + 1 : path;
	ts_path(dp, sizeof dp, c->dst, "%s", base);
	if (is_dir) {
		struct pkgcopy nc;
		host_mkdirs(dp);
		nc.dst = dp;
		host_dir_walk(path, 0, pkg_copy_cb, &nc);
	} else if (host_copy_file(path, dp, 1)) {
		fprintf(stderr, "ci pkg: copy failed: %s\n", path);
	}
	return 0;
}

static void pkg_copy_into(const char *src, const char *destdir) {
	int isd;
	const char *base;
	char dp[8192];
	if (host_stat(src, &isd, NULL, NULL))
		return;
	base = strrchr(src, '/');
	base = base ? base + 1 : src;
	ts_path(dp, sizeof dp, destdir, "%s", base);
	if (isd) {
		struct pkgcopy c;
		host_mkdirs(dp);
		c.dst = dp;
		host_dir_walk(src, 0, pkg_copy_cb, &c);
	} else if (host_copy_file(src, dp, 1)) {
		fprintf(stderr, "ci pkg: copy failed: %s\n", src);
	}
}

static void pkg_copy_glob(const char *dir, const char *pat, const char *destdir) {
	char *g[256];
	int n, k;
	n = ts_glob(dir, pat, 0, g, 256);
	for (k = 0; k < n && k < 256; k++) {
		pkg_copy_into(g[k], destdir);
		free(g[k]);
	}
}

static int pkg_archive(const char *pkg, const char *out, const char *d,
					   const char *ext, Argv *names) {
	Argv v = {{0}, 0};
	char target[8192], *nm;
	HostSpawnOpts o;
	ts_path(target, sizeof target, out, "%s.%s", d, ext);
	ts_arg(&v, "cmake");
	ts_arg(&v, "-E");
	ts_arg(&v, "tar");
	if (!strcmp(ext, "zip")) {
		ts_arg(&v, "cf");
		ts_arg(&v, target);
		ts_arg(&v, "--format=zip");
		ts_arg(&v, d);
	} else {
		ts_arg(&v, !strcmp(ext, "tar.gz") ? "czf" : "cJf");
		ts_arg(&v, target);
		ts_arg(&v, d);
	}
	memset(&o, 0, sizeof o);
	o.cwd = pkg;
	printf("==> archiving %s.%s\n", d, ext);
	if (host_spawn_ex(ts_argz(&v), &o)) {
		fprintf(stderr, "ci pkg: archiving %s failed\n", d);
		return 1;
	}
	nm = malloc(strlen(d) + strlen(ext) + 2);
	sprintf(nm, "%s.%s", d, ext);
	ts_arg(names, nm);
	return 0;
}

static int do_pkg(int argc, char **argv) {
	const char *ver = NULL, *plat = NULL, *stage = "stage", *out = "out", *fmt = NULL;
	const char *ext, *xsuf, *libdir, *pkg = "pkg";
	char ver_s[128], probe[8192], src[8192], d[512], dd[8192], dstbin[8192], dstlib[8192];
	char pkgbuf[4096], outbuf[4096], pkgscratch[8192];
	int isd, i, iswin = MCC_HOST_WIN32;
	Argv names = {{0}, 0};

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--ver") && i + 1 < argc)
			ver = argv[++i];
		else if (!strcmp(argv[i], "--plat") && i + 1 < argc)
			plat = argv[++i];
		else if (!strcmp(argv[i], "--stage") && i + 1 < argc)
			stage = argv[++i];
		else if (!strcmp(argv[i], "--out") && i + 1 < argc)
			out = argv[++i];
		else if (!strcmp(argv[i], "--format") && i + 1 < argc)
			fmt = argv[++i];
	}
	if (!ver || !plat) {
		fprintf(stderr, "usage: ci pkg --ver V --plat P [--stage D] [--out D] [--format txz|tgz|zip]\n");
		return 2;
	}

	{
		const char *v = ver;
		if (*v == 'v')
			v++;
		snprintf(ver_s, sizeof ver_s, "%s", v);
	}
	if (!fmt)
		fmt = iswin ? "zip" : "txz";
	if (!strcmp(fmt, "zip"))
		ext = "zip";
	else if (!strcmp(fmt, "tgz"))
		ext = "tar.gz";
	else
		ext = "tar.xz";
	xsuf = iswin ? ".exe" : "";
	{
		char lib64[8192];
		ts_path(lib64, sizeof lib64, stage, "lib64");
		libdir = (host_stat(lib64, &isd, NULL, NULL) == 0 && isd) ? "lib64" : "lib";
	}

	ts_path(probe, sizeof probe, stage, "bin/mcc%s", xsuf);
	if (host_stat(probe, &isd, NULL, NULL)) {
		fprintf(stderr, "ci pkg: no staged install at '%s' (run cmake --install first)\n", stage);
		return 1;
	}
	ts_path(pkgscratch, sizeof pkgscratch, out, ".pkg");
	pkg = pkgscratch;
	{
		const char *rm[] = {"cmake", "-E", "rm", "-rf", pkg, 0};
		ts_run(rm);
	}
	host_mkdirs(pkg);
	host_mkdirs(out);

	{
		char *ap = host_path_canonical(pkg), *ao = host_path_canonical(out);
		if (ap) {
			snprintf(pkgbuf, sizeof pkgbuf, "%s", ap);
			free(ap);
			pkg = pkgbuf;
		}
		if (ao) {
			snprintf(outbuf, sizeof outbuf, "%s", ao);
			free(ao);
			out = outbuf;
		}
	}

	snprintf(d, sizeof d, "mcc-%s-%s", ver_s, plat);
	ts_path(dd, sizeof dd, pkg, "%s", d);
	ts_path(dstbin, sizeof dstbin, dd, "bin");
	host_mkdirs(dstbin);
	ts_path(dstlib, sizeof dstlib, dd, "lib");
	host_mkdirs(dstlib);
	for (i = 0; HOST_EXES[i]; i++) {
		ts_path(src, sizeof src, stage, "bin/%s%s", HOST_EXES[i], xsuf);
		pkg_copy_into(src, dstbin);
	}
	if (iswin) {
		char binstage[8192];
		ts_path(binstage, sizeof binstage, stage, "bin");
		pkg_copy_glob(binstage, "libmcc*.dll", dstbin);
	}
	ts_path(src, sizeof src, stage, "%s/mcc", libdir);
	pkg_copy_into(src, dstlib);
	if (pkg_archive(pkg, out, d, ext, &names))
		return 1;

	snprintf(d, sizeof d, "libmcc-%s-%s", ver_s, plat);
	ts_path(dd, sizeof dd, pkg, "%s", d);
	ts_path(dstlib, sizeof dstlib, dd, "lib");
	host_mkdirs(dstlib);
	ts_path(src, sizeof src, stage, "include");
	pkg_copy_into(src, dd);
	{
		char libstage[8192], binstage[8192];
		static const char *PATS[] = {"libmcc*.a", "libmcc*.so", "libmcc*.dylib", "libmcc*.lib", 0};
		int pi;
		ts_path(libstage, sizeof libstage, stage, "%s", libdir);
		for (pi = 0; PATS[pi]; pi++)
			pkg_copy_glob(libstage, PATS[pi], dstlib);
		ts_path(binstage, sizeof binstage, stage, "bin");
		pkg_copy_glob(binstage, "libmcc*.dll", dstlib);
	}
	ts_path(src, sizeof src, stage, "%s/cmake", libdir);
	pkg_copy_into(src, dstlib);
	ts_path(src, sizeof src, stage, "%s/mcc", libdir);
	pkg_copy_into(src, dstlib);
	if (pkg_archive(pkg, out, d, ext, &names))
		return 1;

	snprintf(d, sizeof d, "mcc-cross-%s-%s", ver_s, plat);
	ts_path(dd, sizeof dd, pkg, "%s", d);
	ts_path(dstbin, sizeof dstbin, dd, "bin");
	host_mkdirs(dstbin);
	ts_path(dstlib, sizeof dstlib, dd, "lib/mcc");
	host_mkdirs(dstlib);
	{
		char binstage[8192], rtdir[8192];
		char *g[256];
		int n, k, found = 0;
		ts_path(binstage, sizeof binstage, stage, "bin");
		n = ts_glob(binstage, "mcc-*", 0, g, 256);
		for (k = 0; k < n && k < 256; k++) {
			const char *base = strrchr(g[k], '/');
			base = base ? base + 1 : g[k];
			if (!is_host_exe(base)) {
				pkg_copy_into(g[k], dstbin);
				found = 1;
			}
			free(g[k]);
		}
		ts_path(rtdir, sizeof rtdir, stage, "%s/mcc", libdir);
		pkg_copy_glob(rtdir, "*-libmccrt.a", dstlib);
		if (found) {
			if (pkg_archive(pkg, out, d, ext, &names))
				return 1;
		} else
			printf("ci pkg: no cross compilers in stage/bin; skipping cross bundle\n");
	}

	if (names.n > 1) {
		int ncomp = names.n, j, isf;
		char bench[8192];
		snprintf(d, sizeof d, "bundle-%s-%s", ver_s, plat);
		ts_path(dd, sizeof dd, pkg, "%s", d);
		host_mkdirs(dd);
		for (j = 0; j < ncomp; j++) {
			ts_path(src, sizeof src, out, "%s", names.a[j]);
			pkg_copy_into(src, dd);
		}
		ts_path(bench, sizeof bench, out, "bench-%s.txt", plat);
		if (host_stat(bench, &isf, NULL, NULL) == 0 && !isf)
			pkg_copy_into(bench, dd);
		if (pkg_archive(pkg, out, d, ext, &names))
			return 1;
	}

	{
		Argv v = {{0}, 0};
		HostSpawnOpts o;
		char *sout = NULL, csum[8192];
		ts_arg(&v, "cmake");
		ts_arg(&v, "-E");
		ts_arg(&v, "sha256sum");
		for (i = 0; i < names.n; i++)
			ts_arg(&v, names.a[i]);
		memset(&o, 0, sizeof o);
		o.cwd = out;
		o.stdout_buf = &sout;
		if (host_spawn_ex(ts_argz(&v), &o) == 0 && sout) {
			FILE *f;
			ts_path(csum, sizeof csum, out, "checksums-%s.txt", plat);
			if ((f = fopen(csum, "wb"))) {
				fputs(sout, f);
				fclose(f);
			}
		}
		free(sout);
	}

	{
		const char *rm[] = {"cmake", "-E", "rm", "-rf", pkg, 0};
		ts_run(rm);
	}

	printf("== packaged (%s) ==\n", ext);
	for (i = 0; i < names.n; i++) {
		printf("  %s\n", names.a[i]);
		free((void *)names.a[i]);
	}
	return 0;
}

static void js_attr(char *out, int n, const char *p, const char *end,
                    const char *key) {
	const char *a = p, *kl = 0;
	size_t klen = strlen(key);
	out[0] = 0;
	while (a && a < end && (a = strstr(a, key)) && a < end) {
		kl = a + klen;
		break;
	}
	if (!kl || kl >= end)
		return;
	{
		const char *q = strchr(kl, '"');
		if (q && q < end)
			snprintf(out, n, "%.*s", (int)(q - kl), kl);
	}
}

static int do_junit_summary(int argc, char **argv) {
	const char *xml = argc > 0 ? argv[0] : NULL;
	const char *title = argc > 1 ? argv[1] : "tests";
	char *x, *p;
	int total = 0, fail = 0, skip = 0;
	char fails[8192] = "", skips[16384] = "";
	if (!xml) {
		fprintf(stderr, "usage: ci junit-summary <xml> [title]\n");
		return 2;
	}
	x = ts_read_file(xml, NULL);
	if (!x) {
		return 0;
	}
	for (p = x; (p = strstr(p, "<testcase")); ) {
		const char *gt = strchr(p, '>');
		const char *tagend, *extent;
		char name[256] = "?", st[32] = "";
		int skipped, failed;
		if (!gt)
			break;
		tagend = gt + 1;
		if (gt > p && gt[-1] == '/')
			extent = tagend;
		else {
			const char *c = strstr(tagend, "</testcase>");
			extent = c ? c + 11 : x + strlen(x);
		}
		js_attr(name, sizeof name, p, gt, "name=\"");
		js_attr(st, sizeof st, p, gt, "status=\"");
		skipped = (strstr(tagend, "<skipped") &&
		           strstr(tagend, "<skipped") < extent) || !strcmp(st, "notrun");
		failed = (strstr(tagend, "<failure") &&
		          strstr(tagend, "<failure") < extent) || !strcmp(st, "fail");
		total++;
		if (skipped) {
			skip++;
			if (strlen(skips) + strlen(name) + 8 < sizeof skips) {
				strncat(skips, "`", sizeof skips - strlen(skips) - 1);
				strncat(skips, name, sizeof skips - strlen(skips) - 1);
				strncat(skips, "` ", sizeof skips - strlen(skips) - 1);
			}
		} else if (failed) {
			fail++;
			if (strlen(fails) + strlen(name) + 8 < sizeof fails) {
				strncat(fails, "`", sizeof fails - strlen(fails) - 1);
				strncat(fails, name, sizeof fails - strlen(fails) - 1);
				strncat(fails, "` ", sizeof fails - strlen(fails) - 1);
			}
		}
		p = (char *)extent;
	}
	printf("### tests — %s\n\n", title);
	printf("**%d tests · %d passed · %d failed · %d skipped**\n\n",
	       total, total - fail - skip, fail, skip);
	if (fail)
		printf("Failed: %s\n\n", fails);
	if (skip)
		printf("<details><summary>%d skipped</summary>\n\n%s\n\n</details>\n\n",
		       skip, skips);
	free(x);
	return 0;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: ci <stage|run-preset|qemu|local|dist|matrix|pkg|sha256sums> ...\n");
		return 2;
	}
	if (!strcmp(argv[1], "stage"))
		return do_stage(argc - 2, argv + 2);
	if (!strcmp(argv[1], "run-preset"))
		return do_run_preset(argc - 2, argv + 2);
	if (!strcmp(argv[1], "qemu"))
		return do_qemu(argc - 2, argv + 2);
	if (!strcmp(argv[1], "local"))
		return do_local(argc - 2, argv + 2);
	if (!strcmp(argv[1], "dist"))
		return do_dist(argc - 2, argv + 2);
	if (!strcmp(argv[1], "matrix"))
		return do_matrix(argc - 2, argv + 2);
	if (!strcmp(argv[1], "pkg"))
		return do_pkg(argc - 2, argv + 2);
	if (!strcmp(argv[1], "sha256sums"))
		return do_sha256sums(argc - 2, argv + 2);
	if (!strcmp(argv[1], "junit-summary"))
		return do_junit_summary(argc - 2, argv + 2);
	fprintf(stderr, "ci: unknown verb '%s'\n", argv[1]);
	return 2;
}
