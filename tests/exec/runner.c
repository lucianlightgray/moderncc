













#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goldens.h"

static char *xstrdup(const char *s){ char *p = malloc(strlen(s)+1); strcpy(p,s); return p; }


static char *slurp(FILE *f, size_t *outlen){
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    size_t n;
    while ((n = fread(buf+len, 1, cap-len, f)) > 0){
        len += n;
        if (len == cap){ cap *= 2; buf = realloc(buf, cap); }
    }
    buf[len] = 0;
    if (outlen) *outlen = len;
    return buf;
}

static char *run_capture(const char *cmd, int *status){
    FILE *f = popen(cmd, "r");
    if (!f){ if (status) *status = -1; return xstrdup(""); }
    char *out = slurp(f, NULL);
    int rc = pclose(f);
    if (status) *status = rc;
    return out;
}


static void strip_all(char *s, const char *needle){
    size_t nl = strlen(needle);
    if (!nl) return;
    char *r = s, *w = s;
    while (*r){
        if (!strncmp(r, needle, nl)){ r += nl; continue; }
        *w++ = *r++;
    }
    *w = 0;
}



static char *canon_line(const char *line, size_t len){
    char *out = malloc(len+1);
    size_t o = 0; int ws = 0, started = 0;
    for (size_t i = 0; i < len; i++){
        char c = line[i];
        if (c == ' ' || c == '\t'){ ws = 1; continue; }
        if (ws && started) out[o++] = ' ';
        ws = 0; started = 1;
        out[o++] = c;
    }
    out[o] = 0;
    return out;
}


static int texts_equal(const char *a, const char *b){
    const char *pa = a, *pb = b;
    for (;;){

        const char *ea = strchr(pa, '\n');
        const char *eb = strchr(pb, '\n');
        size_t la = ea ? (size_t)(ea-pa) : strlen(pa);
        size_t lb = eb ? (size_t)(eb-pb) : strlen(pb);
        int a_end = (la == 0 && !ea && *pa == 0);
        int b_end = (lb == 0 && !eb && *pb == 0);

        char *ca = canon_line(pa, la);
        char *cb = canon_line(pb, lb);
        int eq = !strcmp(ca, cb);
        free(ca); free(cb);
        if (!eq) return 0;
        if (!ea && !eb) return 1;
        pa = ea ? ea+1 : pa+la;
        pb = eb ? eb+1 : pb+lb;

        if (!ea && *pa == 0){
            while (eb){ const char *n = strchr(pb,'\n'); size_t l = n?(size_t)(n-pb):strlen(pb);
                char *c = canon_line(pb,l); int blank = (*c==0); free(c);
                if (!blank) return 0; if(!n) break; pb=n+1; if(*pb==0)break; }
            return 1;
        }
        if (!eb && *pb == 0){
            while (ea){ const char *n = strchr(pa,'\n'); size_t l = n?(size_t)(n-pa):strlen(pa);
                char *c = canon_line(pa,l); int blank = (*c==0); free(c);
                if (!blank) return 0; if(!n) break; pa=n+1; if(*pa==0)break; }
            return 1;
        }
        (void)a_end; (void)b_end;
    }
}

int main(int argc, char **argv){
    if (argc < 6){
        fprintf(stderr, "usage: %s <mcc> <bdir> <idir> <testroot> <workdir>\n", argv[0]);
        return 2;
    }
    const char *mcc = argv[1], *bdir = argv[2], *idir = argv[3];
    const char *root = argv[4], *work = argv[5];

    const char *emu = getenv("MCC_TEST_EMU"); if (!emu) emu = "";

    char **skip = argv + 6; int nskip = argc - 6;
    int pass = 0, fail = 0, skipped = 0, ref = 0;
    char cmd[8192], path[4096], srcdir[4096];

    snprintf(cmd, sizeof cmd, "mkdir -p \"%s\"", work);
    if (system(cmd)) { fprintf(stderr, "cannot create workdir %s\n", work); return 2; }

    for (int i = 0; i < mcc_goldens_count; i++){
        const mcc_golden_t *g = &mcc_goldens[i];
        int do_skip = 0;
        for (int s = 0; s < nskip; s++) if (!strcmp(skip[s], g->name)){ do_skip = 1; break; }
        if (do_skip){ skipped++; continue; }
        snprintf(path, sizeof path, "%s/%s", root, g->src);

        strcpy(srcdir, path);
        char *slash = strrchr(srcdir, '/'); if (slash) *slash = 0;
        char *out; int rc;



        char xargs[8192]; { const char *a = g->args; char *w = xargs;
            while (*a){ if (!strncmp(a, "{SELF}", 6)){ w += sprintf(w, "\"%s\"", path); a += 6; }
                        else *w++ = *a++; } *w = 0; }

        if (!strcmp(g->mode, "ref")){



            ref++;
            continue;
        } else if (!strcmp(g->mode, "pp")){
            snprintf(cmd, sizeof cmd,
                "%s \"%s\" \"-B%s\" \"-I%s\" -E -P \"%s\" 2>&1",
                emu, mcc, bdir, idir, path);
            out = run_capture(cmd, &rc);
        } else if (!strcmp(g->mode, "dt")){


            snprintf(cmd, sizeof cmd,
                "cd \"%s\" && %s \"%s\" \"-B%s\" \"-I%s\" -dt -run \"%s\" %s 2>&1",
                work, emu, mcc, bdir, idir, path, g->flags);
            out = run_capture(cmd, &rc);
        } else if (!strcmp(g->mode, "run2")){

            snprintf(cmd, sizeof cmd,
                "cd \"%s\" && ( %s \"%s\" \"-B%s\" \"-I%s\" -run \"%s\" && "
                "%s \"%s\" \"-B%s\" \"-I%s\" -b -run \"%s\" ) 2>&1",
                work, emu, mcc, bdir, idir, path, emu, mcc, bdir, idir, path);
            out = run_capture(cmd, &rc);
        } else {
            char exe[4096];
            snprintf(exe, sizeof exe, "%s/t2_%s.exe", work, g->name);
            snprintf(cmd, sizeof cmd,
                "%s \"%s\" \"-B%s\" \"-I%s\" \"%s\" %s -o \"%s\" 2>&1",
                emu, mcc, bdir, idir, path, g->flags, exe);
            char *cerr = run_capture(cmd, &rc);
            if (rc != 0){
                printf("FAIL  %-32s (compile)\n%s", g->name, cerr);
                free(cerr); fail++; continue;
            }


            snprintf(cmd, sizeof cmd, "cd \"%s\" && %s \"%s\" %s", work, emu, exe, xargs);
            char *prog = run_capture(cmd, &rc);



            out = malloc(strlen(cerr) + strlen(prog) + 1);
            strcpy(out, cerr); strcat(out, prog);
            free(cerr); free(prog);
        }

        char prefix[4096];
        snprintf(prefix, sizeof prefix, "%s/", srcdir);
        strip_all(out, prefix);

        if (texts_equal(g->expect, out)){
            pass++;
        } else {
            fail++;
            printf("FAIL  %-32s (mismatch)\n", g->name);
            printf("  --- expected ---\n%s\n  --- got ---\n%s\n", g->expect, out);
        }
        free(out);
    }
    printf("tests2_runner: %d passed, %d failed, %d skipped, %d ref (of %d)\n",
           pass, fail, skipped, ref, mcc_goldens_count);
    return fail ? 1 : 0;
}
