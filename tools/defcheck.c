/*
 *  defcheck.c - maintain the win32 import .def files (PLAN 0.6 #5)
 *
 *  The checked-in runtime/win32/lib/{kernel32,msvcrt,user32,gdi32,ws2_32}.def
 *  files carry historical (Win9x-era) exports that a modern DLL no longer
 *  advertises.  Regenerating with `mcc -impdef` must therefore UNION the fresh
 *  exports onto the existing set - never drop a historical symbol - appending
 *  only genuinely new names and preserving the existing order and format
 *  (e.g. the deliberate out-of-order SetWindowLongPtrA/W in user32.def).
 *
 *  Modes:
 *    defcheck verify <def>...            CI guard: each .def has the required
 *                                        shape (LIBRARY / blank / EXPORTS /
 *                                        bare names, no ordinals/DATA) and no
 *                                        duplicate symbols.
 *    defcheck merge  <base.def> <new.def>   print base ∪ new (base order kept,
 *                                           new-only symbols appended).
 *    defcheck regen  <mcc> <base.def> <dll> [-o out]
 *                                        run `mcc -impdef dll` and merge onto
 *                                        base (maintainer mode; needs the DLL).
 *
 *  exit: 0 ok, 1 malformed / would-drop, 2 usage/IO.
 */
#include "toolsupport.h"

typedef struct { char **v; int n, cap; } Syms;

static void syms_add(Syms *s, const char *name)
{
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 64;
        s->v = realloc(s->v, s->cap * sizeof *s->v);
    }
    s->v[s->n++] = strdup(name);
}
static int syms_has(const Syms *s, const char *name)
{
    int i;
    for (i = 0; i < s->n; i++)
        if (!strcmp(s->v[i], name))
            return 1;
    return 0;
}

/* parse a .def into *lib (LIBRARY value) and ordered EXPORTS in *out.
   Returns 0 ok, -1 malformed; on malformed prints why with `who` prefix. */
static int def_parse(const char *path, char *lib, int libsz, Syms *out, const char *who)
{
    char *text = ts_read_file(path, NULL), *p;
    int seen_lib = 0, seen_exports = 0, rc = 0;
    if (!text) { fprintf(stderr, "defcheck: cannot read %s\n", path); return -1; }
    for (p = text; *p; ) {
        char *e = strchr(p, '\n');
        int len = e ? (int)(e - p) : (int)strlen(p);
        char *line;
        while (len && (p[len-1]=='\r' || p[len-1]==' ' || p[len-1]=='\t')) --len;
        line = p;
        line[len] = 0;
        while (*line == ' ' || *line == '\t') ++line;
        if (!*line || *line == ';') {           /* blank or comment */
        } else if (!seen_lib) {
            if (strncmp(line, "LIBRARY", 7)) {
                fprintf(stderr, "%s: %s: first entry is not LIBRARY\n", who, path);
                rc = -1; break;
            }
            snprintf(lib, libsz, "%s", line + 7 + strspn(line + 7, " \t"));
            seen_lib = 1;
        } else if (!seen_exports) {
            if (strcmp(line, "EXPORTS")) {
                fprintf(stderr, "%s: %s: expected EXPORTS, got '%s'\n", who, path, line);
                rc = -1; break;
            }
            seen_exports = 1;
        } else {
            /* a bare export name: a single whitespace-free token with no alias
               '='.  An ordinal ('name @N') or 'DATA'/'NONAME' attribute is
               whitespace-separated, so the space check rejects it; '@' may
               appear inside a C++ mangled name (msvcrt) and is allowed. */
            if (strpbrk(line, " \t=")) {
                fprintf(stderr, "%s: %s: symbol '%s' has ordinal/alias/DATA syntax\n", who, path, line);
                rc = -1; break;
            }
            if (syms_has(out, line)) {
                fprintf(stderr, "%s: %s: duplicate symbol '%s'\n", who, path, line);
                rc = -1; break;
            }
            syms_add(out, line);
        }
        if (!e) break;
        p = e + 1;
    }
    if (rc == 0 && !seen_exports) {
        fprintf(stderr, "%s: %s: no EXPORTS section\n", who, path);
        rc = -1;
    }
    free(text);
    return rc;
}

static void def_emit(FILE *f, const char *lib, const Syms *s)
{
    int i;
    fprintf(f, "LIBRARY %s\n\nEXPORTS\n", lib);
    for (i = 0; i < s->n; i++)
        fprintf(f, "%s\n", s->v[i]);
}

static int do_verify(int argc, char **argv)
{
    int i, bad = 0;
    for (i = 0; i < argc; i++) {
        char lib[256];
        Syms s = {0};
        if (def_parse(argv[i], lib, sizeof lib, &s, "defcheck verify") != 0)
            bad = 1;
        else
            printf("OK %s: LIBRARY %s, %d exports\n", argv[i], lib, s.n);
    }
    return bad;
}

/* merge new onto base (base order preserved, new-only appended) into *out;
   *lib is base's LIBRARY.  Returns count of appended symbols, or -1. */
static int do_merge_into(const char *basep, const char *newp, char *lib, int libsz, Syms *out)
{
    Syms nw = {0};
    char nlib[256];
    int i, appended = 0;
    if (def_parse(basep, lib, libsz, out, "defcheck merge") != 0)
        return -1;
    if (def_parse(newp, nlib, sizeof nlib, &nw, "defcheck merge") != 0)
        return -1;
    for (i = 0; i < nw.n; i++)
        if (!syms_has(out, nw.v[i])) {
            syms_add(out, nw.v[i]);
            appended++;
        }
    return appended;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: defcheck <verify|merge|regen> ...\n");
        return 2;
    }
    if (!strcmp(argv[1], "verify"))
        return do_verify(argc - 2, argv + 2);

    if (!strcmp(argv[1], "merge")) {
        char lib[256];
        Syms out = {0};
        int n;
        if (argc != 4) { fprintf(stderr, "usage: defcheck merge <base.def> <new.def>\n"); return 2; }
        n = do_merge_into(argv[2], argv[3], lib, sizeof lib, &out);
        if (n < 0) return 1;
        def_emit(stdout, lib, &out);
        fprintf(stderr, "defcheck: merged, %d new symbol(s) appended\n", n);
        return 0;
    }

    if (!strcmp(argv[1], "regen")) {
        const char *mcc, *base, *dll, *outpath = NULL;
        char tmpdef[4096], lib[256];
        Syms out = {0};
        int i, n;
        FILE *f;
        if (argc < 5) { fprintf(stderr, "usage: defcheck regen <mcc> <base.def> <dll> [-o out]\n"); return 2; }
        mcc = argv[2]; base = argv[3]; dll = argv[4];
        for (i = 5; i < argc; i++)
            if (!strcmp(argv[i], "-o") && i + 1 < argc) outpath = argv[++i];
        snprintf(tmpdef, sizeof tmpdef, "%s.impdef.tmp", base);
        {
            const char *av[] = { mcc, "-impdef", dll, "-o", tmpdef, 0 };
            if (ts_run(av)) { fprintf(stderr, "defcheck: mcc -impdef failed\n"); return 1; }
        }
        n = do_merge_into(base, tmpdef, lib, sizeof lib, &out);
        remove(tmpdef);
        if (n < 0) return 1;
        f = outpath ? fopen(outpath, "w") : stdout;
        if (!f) { fprintf(stderr, "defcheck: cannot write %s\n", outpath); return 2; }
        def_emit(f, lib, &out);
        if (outpath) fclose(f);
        fprintf(stderr, "defcheck: regen %s, %d new symbol(s) appended\n", base, n);
        return 0;
    }

    fprintf(stderr, "defcheck: unknown mode '%s'\n", argv[1]);
    return 2;
}
