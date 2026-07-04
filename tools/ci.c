#include "toolsupport.h"

static const char *EXCL_PREFIX[] = {"cmake-build", "cmake-windows-", "cmake-mingw-", "build-", 0};
static const char *EXCL_EXACT[] = {"cmake-clang", ".git", 0};

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
		ts_arg(&v, "ctest");
		ts_arg(&v, "--preset");
		ts_arg(&v, preset);
		ts_arg(&v, jflag);
		if (config) {
			ts_arg(&v, "--build-config");
			ts_arg(&v, config);
		}
		for (i = extra_start; i < argc; i++)
			ts_arg(&v, argv[i]);
		printf("==> testing (preset=%s)\n", preset);
		if (ts_run(ts_argz(&v)))
			return 1;
	}

	if (out || do_install) {
		Argv v = {{0}, 0};
		snprintf(instdir, sizeof instdir, "cmake-build-%s", preset);
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
		ts_arg(&v, "czf");
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
	char pkgbuf[4096], outbuf[4096];
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
		fprintf(stderr, "usage: ci pkg --ver V --plat P [--stage D] [--out D] [--format tgz|zip]\n");
		return 2;
	}

	{
		const char *v = ver;
		if (*v == 'v')
			v++;
		snprintf(ver_s, sizeof ver_s, "%s", v);
	}
	if (!fmt)
		fmt = iswin ? "zip" : "tgz";
	ext = !strcmp(fmt, "zip") ? "zip" : "tar.gz";
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
	{
		const char *rm[] = {"cmake", "-E", "rm", "-rf", pkg, out, 0};
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
		pkg_copy_glob(rtdir, "*-libmcc1.a", dstlib);
		if (found) {
			if (pkg_archive(pkg, out, d, ext, &names))
				return 1;
		} else
			printf("ci pkg: no cross compilers in stage/bin; skipping cross bundle\n");
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

	printf("== packaged (%s) ==\n", ext);
	for (i = 0; i < names.n; i++) {
		printf("  %s\n", names.a[i]);
		free((void *)names.a[i]);
	}
	return 0;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: ci <stage|run-preset|matrix|pkg|sha256sums> ...\n");
		return 2;
	}
	if (!strcmp(argv[1], "stage"))
		return do_stage(argc - 2, argv + 2);
	if (!strcmp(argv[1], "run-preset"))
		return do_run_preset(argc - 2, argv + 2);
	if (!strcmp(argv[1], "matrix"))
		return do_matrix(argc - 2, argv + 2);
	if (!strcmp(argv[1], "pkg"))
		return do_pkg(argc - 2, argv + 2);
	if (!strcmp(argv[1], "sha256sums"))
		return do_sha256sums(argc - 2, argv + 2);
	fprintf(stderr, "ci: unknown verb '%s'\n", argv[1]);
	return 2;
}
