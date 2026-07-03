/*
 *  ci.c - CI orchestration primitives in C (PLAN 0.9)
 *
 *  Collapses the hand-repeated CI shell into one tool:
 *    ci stage <src> <dst>          tree copy with the single shared exclusion
 *                                  list (catalog #9) + CRLF->LF normalization
 *                                  over .c/.h/.cmake/.txt/.S/.def sources
 *                                  (replaces tests/ci/docker/run-ci.sh:29-48).
 *    ci run-preset <name> [--out D] [-- <ctest args>]
 *                                  configure -> build -> test -> (install),
 *                                  with one normalized parallelism probe
 *                                  (replaces the 7 duplicated configure/build/
 *                                  test/install blocks, catalog #11).
 *    ci matrix [<presets.json>]    enumerate the non-hidden configure presets,
 *                                  one per line (single-sources the CI matrix,
 *                                  catalog #12; default CMakePresets.json).
 *    ci sha256sums <dir> [--out F] merge checksums-*.txt into SHA256SUMS.txt.
 *
 *  Every child (cmake/ctest) is spawned via the host layer - no shell.
 */
#include "toolsupport.h"

/* single source-tree exclusion list (catalog #9): prefixes match a top-level
   directory name, exact entries match it whole. */
static const char *EXCL_PREFIX[] = { "cmake-build", "cmake-windows-", "cmake-mingw-", "build-", 0 };
static const char *EXCL_EXACT[]  = { "cmake-clang", ".git", 0 };

static int excluded(const char *base)
{
    int i;
    for (i = 0; EXCL_PREFIX[i]; i++)
        if (!strncmp(base, EXCL_PREFIX[i], strlen(EXCL_PREFIX[i])))
            return 1;
    for (i = 0; EXCL_EXACT[i]; i++)
        if (!strcmp(base, EXCL_EXACT[i]))
            return 1;
    return 0;
}

/* ---- stage ------------------------------------------------------------ */

struct stagectx { const char *dst; };

static int stage_cb(const char *path, int is_dir, void *ud)
{
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

static int has_suffix(const char *s, const char *suf)
{
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && !strcmp(s + ls - lf, suf);
}

/* CRLF -> LF (drop CR when it ends a line, matching sed 's/\r$//') */
static int normalize_cb(const char *path, int is_dir, void *ud)
{
    static const char *EXT[] = { ".c", ".h", ".cmake", ".txt", ".S", ".def", 0 };
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
        if (buf[i] == '\r' && (i + 1 == n || buf[i + 1] == '\n')) { touched = 1; continue; }
        buf[o++] = buf[i];
    }
    if (touched) {
        FILE *f = fopen(path, "wb");
        if (f) { fwrite(buf, 1, o, f); fclose(f); }
    }
    free(buf);
    return 0;
}

static int do_stage(int argc, char **argv)
{
    struct stagectx c;
    if (argc != 2) { fprintf(stderr, "usage: ci stage <src> <dst>\n"); return 2; }
    printf("==> staging %s -> %s\n", argv[0], argv[1]);
    if (host_mkdirs(argv[1])) { fprintf(stderr, "ci: cannot create %s\n", argv[1]); return 1; }
    c.dst = argv[1];
    host_dir_walk(argv[0], 0, stage_cb, &c);
    printf("==> normalizing line endings (CRLF -> LF) in staged sources\n");
    host_dir_walk(argv[1], 1, normalize_cb, NULL);
    return 0;
}

/* ---- run-preset ------------------------------------------------------- */

static int do_run_preset(int argc, char **argv)
{
    const char *preset = NULL, *out = NULL;
    char jflag[32], instdir[4096], prefix[4096];
    int i, extra_start = argc;
    int jobs = host_nproc();

    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--")) { extra_start = i + 1; break; }
        else if (!preset && argv[i][0] != '-') preset = argv[i];
    }
    if (!preset) { fprintf(stderr, "usage: ci run-preset <name> [--out DIR] [-- <ctest args>]\n"); return 2; }
    snprintf(jflag, sizeof jflag, "-j%d", jobs > 0 ? jobs : 1);

    /* configure */
    { Argv v = {{0}, 0};
      ts_arg(&v, "cmake"); ts_arg(&v, "--preset"); ts_arg(&v, preset);
      if (out) { snprintf(prefix, sizeof prefix, "-DCMAKE_INSTALL_PREFIX=%s", out); ts_arg(&v, prefix); }
      printf("==> configuring (preset=%s)\n", preset);
      if (ts_run(ts_argz(&v))) return 1;
    }
    /* build */
    { const char *a[] = { "cmake", "--build", "--preset", preset, jflag, 0 };
      printf("==> building (%s)\n", jflag);
      if (ts_run(a)) return 1; }
    /* test (+ any pass-through ctest args after --) */
    { Argv v = {{0}, 0};
      ts_arg(&v, "ctest"); ts_arg(&v, "--preset"); ts_arg(&v, preset); ts_arg(&v, jflag);
      for (i = extra_start; i < argc; i++) ts_arg(&v, argv[i]);
      printf("==> testing (preset=%s)\n", preset);
      if (ts_run(ts_argz(&v))) return 1; }
    /* install */
    if (out) {
        snprintf(instdir, sizeof instdir, "cmake-build-%s", preset);
        const char *a[] = { "cmake", "--install", instdir, 0 };
        printf("==> exporting build targets -> %s\n", out);
        if (ts_run(a)) return 1;
    }
    return 0;
}

/* ---- matrix (parse CMakePresets.json) --------------------------------- */

/* print the first "name" string of a JSON object [b,e) unless it has
   "hidden": true; when `filter` is set, only names starting with it */
static const char *g_filter;
static int g_json, g_json_first = 1;
static void emit_preset(const char *b, const char *e)
{
    const char *nm = NULL, *p;
    int hidden = 0;
    for (p = b; p + 6 <= e; p++) {
        if (!nm && !strncmp(p, "\"name\"", 6)) {
            const char *q = p + 6;
            while (q < e && *q != '"') q++;         /* to opening quote of value */
            if (q < e) { const char *s = ++q; while (q < e && *q != '"') q++; nm = s; b = q; }
        }
        if (!strncmp(p, "\"hidden\"", 8)) {
            const char *q = p + 8;
            while (q < e && *q != ':') q++;
            while (q < e && (*q == ':' || *q == ' ' || *q == '\t')) q++;
            if (q < e && !strncmp(q, "true", 4)) hidden = 1;
        }
    }
    if (nm && !hidden) {
        const char *q = nm; int len;
        while (*q != '"') q++;
        len = (int)(q - nm);
        if (g_filter && strncmp(nm, g_filter, strlen(g_filter))) return;
        if (g_json) { printf("%s\"%.*s\"", g_json_first ? "" : ",", len, nm); g_json_first = 0; }
        else printf("%.*s\n", len, nm);
    }
}

static int do_matrix(int argc, char **argv)
{
    const char *file = NULL;
    char *text, *p, *arr;
    int instr = 0, esc = 0, depth = 0, i;
    const char *obj_start = NULL;

    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--filter") && i + 1 < argc) g_filter = argv[++i];
        else if (!strcmp(argv[i], "--json")) g_json = 1;
        else if (!file) file = argv[i];
    }
    if (!file) file = "CMakePresets.json";
    text = ts_read_file(file, NULL);
    if (!text) { fprintf(stderr, "ci: cannot read %s\n", file); return 2; }
    if (!(arr = strstr(text, "\"configurePresets\""))) { free(text); return 0; }
    p = strchr(arr, '[');
    if (!p) { free(text); return 0; }
    if (g_json) printf("[");
    for (++p; *p; p++) {
        char c = *p;
        if (instr) { if (esc) esc = 0; else if (c == '\\') esc = 1; else if (c == '"') instr = 0; continue; }
        if (c == '"') { instr = 1; continue; }
        if (c == '{') { if (depth == 0) obj_start = p; depth++; }
        else if (c == '}') { if (--depth == 0 && obj_start) { emit_preset(obj_start, p + 1); obj_start = NULL; } }
        else if (c == ']' && depth == 0) break;     /* end of configurePresets */
    }
    if (g_json) printf("]\n");
    free(text);
    return 0;
}

/* ---- sha256sums merge ------------------------------------------------- */

struct mergectx { FILE *out; };

static int merge_cb(const char *path, int is_dir, void *ud)
{
    struct mergectx *m = ud;
    const char *base = strrchr(path, '/');
    char *text; long n;
    base = base ? base + 1 : path;
    if (is_dir) return 0;
    if (strncmp(base, "checksums-", 10) || !has_suffix(base, ".txt"))
        return 0;
    if ((text = ts_read_file(path, &n))) {
        fwrite(text, 1, n, m->out);
        free(text);
    }
    return 0;
}

static int do_sha256sums(int argc, char **argv)
{
    const char *dir = NULL, *out = "SHA256SUMS.txt";
    struct mergectx m;
    int i;
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!dir) dir = argv[i];
    }
    if (!dir) { fprintf(stderr, "usage: ci sha256sums <dir> [--out FILE]\n"); return 2; }
    if (!(m.out = fopen(out, "wb"))) { fprintf(stderr, "ci: cannot write %s\n", out); return 2; }
    host_dir_walk(dir, 1, merge_cb, &m);   /* recursive: artifacts nest per-name */
    fclose(m.out);
    printf("ci: merged checksums-*.txt -> %s\n", out);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: ci <stage|run-preset|matrix|sha256sums> ...\n");
        return 2;
    }
    if (!strcmp(argv[1], "stage"))      return do_stage(argc - 2, argv + 2);
    if (!strcmp(argv[1], "run-preset")) return do_run_preset(argc - 2, argv + 2);
    if (!strcmp(argv[1], "matrix"))     return do_matrix(argc - 2, argv + 2);
    if (!strcmp(argv[1], "sha256sums")) return do_sha256sums(argc - 2, argv + 2);
    fprintf(stderr, "ci: unknown verb '%s'\n", argv[1]);
    return 2;
}
