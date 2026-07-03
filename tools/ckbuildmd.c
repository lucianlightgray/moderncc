/*
 *  ckbuildmd.c - docs/BUILD.md node-table drift check (PLAN 0.6 #4)
 *
 *  The mcc_config_node() declarations in CMakeLists.txt are the single source
 *  of truth for the build knobs; docs/BUILD.md documents them by hand.  This
 *  tool fails the build when the two drift: every configured node must appear
 *  as a table row in BUILD.md with a matching Type.  (The Default/Gate columns
 *  are human-prettified - **ON**, **ELF**, "always (advanced)" - and are not
 *  machine-compared; node existence and type catch the drift that matters:
 *  a node added, renamed, or retyped without updating the docs.)
 *
 *  Input: config-nodes.tsv (emitted by CMake: "<name> <type>" per line).
 *  usage: ckbuildmd <config-nodes.tsv> <docs/BUILD.md>
 *  exit:  0 in sync, 1 drift, 2 usage/IO.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXN 256

static char *trim(char *s)
{
    char *e;
    while (*s && isspace((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

/* extract the nth (0-based) '|'-delimited cell of a markdown row into out */
static int cell(const char *row, int n, char *out, int osz)
{
    const char *p = row, *start;
    int i = 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '|') return 0;
    ++p;
    for (;;) {
        start = p;
        while (*p && *p != '|') p++;
        if (i == n) {
            int len = (int)(p - start);
            if (len >= osz) len = osz - 1;
            memcpy(out, start, len);
            out[len] = 0;
            return 1;
        }
        if (!*p) return 0;
        ++p;
        ++i;
    }
}

int main(int argc, char **argv)
{
    static const char *TYPEKW[] = { "BOOL", "STRING", "FILEPATH", "PATH", "INT", 0 };
    char names[MAXN][64], types[MAXN][32];
    int found[MAXN], typed[MAXN], typeok[MAXN], nn = 0, i, drift = 0;
    FILE *tsv, *md;
    char line[8192];

    if (argc != 3) {
        fprintf(stderr, "usage: ckbuildmd <config-nodes.tsv> <docs/BUILD.md>\n");
        return 2;
    }
    if (!(tsv = fopen(argv[1], "r"))) { fprintf(stderr, "ckbuildmd: cannot open %s\n", argv[1]); return 2; }
    while (fgets(line, sizeof line, tsv) && nn < MAXN) {
        char *s = trim(line), *sp;
        if (!*s) continue;
        sp = strchr(s, ' ');
        if (!sp) continue;
        *sp = 0;
        snprintf(names[nn], sizeof names[nn], "%s", s);
        snprintf(types[nn], sizeof types[nn], "%s", trim(sp + 1));
        found[nn] = typed[nn] = typeok[nn] = 0;
        nn++;
    }
    fclose(tsv);

    if (!(md = fopen(argv[2], "r"))) { fprintf(stderr, "ckbuildmd: cannot open %s\n", argv[2]); return 2; }
    while (fgets(line, sizeof line, md)) {
        char c0[128], c1[64], *nm;
        if (!cell(line, 0, c0, sizeof c0))
            continue;
        nm = trim(c0);
        /* first cell is `NAME` in backticks */
        if (*nm == '`') { nm++; { char *bt = strchr(nm, '`'); if (bt) *bt = 0; } }
        for (i = 0; i < nn; i++) {
            if (strcmp(nm, names[i]))
                continue;
            found[i] = 1;
            /* only some tables carry a Type column; compact 2-column tables
               (e.g. Runtime-path / Build-flag knobs) legitimately omit it.
               Type-check a row only when its 2nd cell is a CMake type keyword. */
            if (cell(line, 1, c1, sizeof c1)) {
                char *ty = trim(c1);
                int k, is_type = 0;
                for (k = 0; TYPEKW[k]; k++)
                    if (!strncmp(ty, TYPEKW[k], strlen(TYPEKW[k]))) { is_type = 1; break; }
                if (is_type) {
                    typed[i] = 1;               /* a full node table row */
                    if (!strncmp(ty, types[i], strlen(types[i])))
                        typeok[i] = 1;
                }
            }
            /* no break: a node may appear in a compact row and a typed row */
        }
    }
    fclose(md);

    for (i = 0; i < nn; i++) {
        if (!found[i]) {
            printf("DRIFT: node %s is missing from BUILD.md\n", names[i]);
            drift = 1;
        } else if (typed[i] && !typeok[i]) {
            printf("DRIFT: node %s type does not match BUILD.md (expected %s)\n",
                   names[i], types[i]);
            drift = 1;
        }
    }
    if (drift) {
        printf("BUILD.md node tables have drifted from the mcc_config_node() "
               "source of truth; update docs/BUILD.md.\n");
        return 1;
    }
    printf("BUILD.md node tables in sync (%d nodes).\n", nn);
    return 0;
}
