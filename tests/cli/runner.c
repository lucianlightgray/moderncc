














#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#ifdef _MSC_VER

#define popen  _popen
#define pclose _pclose
#endif
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0777)
#endif

#include "cases.h"

#define MCC_SKIP_RC 77

static const char *envv(const char *k, const char *d){
    const char *v = getenv(k); return (v && *v) ? v : d;
}

#ifdef _WIN32






static const char *g_work = ".";
static FILE *shell_popen(const char *cmd){
    char path[1024];
    snprintf(path, sizeof path, "%s/_clicmd.sh", g_work);
    FILE *sf = fopen(path, "wb");
    if (sf){ fputs(cmd, sf); fputc('\n', sf); fclose(sf); }
    const char *sh = envv("MCC_TEST_SH", "sh");



    char line[1200];
    snprintf(line, sizeof line, "\"\"%s\" \"%s\"\"", sh, path);
    return popen(line, "r");
}
#endif






static const char *timeout_prefix(void){
#ifdef _WIN32

    FILE *f = shell_popen("command -v timeout >/dev/null 2>&1 && echo y");
    int ok = 0;
    if (f){ int c = fgetc(f); ok = (c == 'y'); pclose(f); }
    return ok ? "timeout 10 " : "";
#else
    if (system("command -v timeout  >/dev/null 2>&1") == 0) return "timeout 10 ";
    if (system("command -v gtimeout >/dev/null 2>&1") == 0) return "gtimeout 10 ";
    return "";
#endif
}

/* case-insensitive: cases say os=linux, cmake exports MCC_TEST_OS=Linux */
static int os_eq(const char *a, const char *b){
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

static int req_met(const char *req, char *reason, size_t rn){
    if (!req || !*req) return 1;
    char buf[256]; snprintf(buf, sizeof buf, "%s", req);
    const char *cpu = envv("MCC_TEST_CPU", "unknown");
    const char *os  = envv("MCC_TEST_OS",  "unknown");
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")){
        while (*tok == ' ') tok++;
        if (!strncmp(tok, "note:", 5)){ snprintf(reason, rn, "%s", tok + 5); return 0; }
        else if (!strncmp(tok, "cpu=", 4)){
            const char *want = tok + 4; int ok;
            if (!strcmp(want, "x86")) ok = !strcmp(cpu,"i386") || !strcmp(cpu,"x86_64");
            else ok = !strcmp(cpu, want);
            if (!ok){ snprintf(reason, rn, "requires %s target (host: %s)", want, cpu); return 0; }
        } else if (!strncmp(tok, "os=", 3)){
            if (!os_eq(os, tok + 3)){ snprintf(reason, rn, "requires %s OS (host: %s)", tok+3, os); return 0; }
        } else if (!strncmp(tok, "os!=", 4)){


            const char *want = tok + 4;
            const char *colon = strchr(want, ':');
            char wbuf[64];
            size_t wl = colon ? (size_t)(colon - want) : strlen(want);
            if (wl >= sizeof wbuf) wl = sizeof wbuf - 1;
            memcpy(wbuf, want, wl); wbuf[wl] = 0;
            if (os_eq(os, wbuf)){
                if (colon && colon[1]) snprintf(reason, rn, "%s", colon + 1);
                else snprintf(reason, rn, "not applicable to the %s target", wbuf);
                return 0;
            }
        } else if (!strcmp(tok, "elf")){


            if (os_eq(os, "Darwin") || os_eq(os, "WIN32")){
                snprintf(reason, rn, "requires an ELF target (host: %s)", os); return 0; }
        } else if (!strcmp(tok, "asm")){
            if (strcmp(envv("MCC_TEST_ASM", "1"), "1")){
                snprintf(reason, rn, "requires integrated assembler (MCC_CONFIG_ASM)"); return 0; }
        } else if (!strcmp(tok, "stabs")){
            /* nonempty MCC_TEST_DWARF = dwarf is the configured -g default */
            if (envv("MCC_TEST_DWARF", "")[0]){
                snprintf(reason, rn, "requires stabs as default -g format (MCC_CONFIG_DWARF set)"); return 0; }
        }
    }
    return 1;
}

static int has_dot_run(const char *s){
    int n = 0;
    for (; *s; s++){ if (*s == '.'){ if (++n >= 3) return 1; } else n = 0; }
    return 0;
}

static int glob_eq(const char *pat, const char *str){
    const char *s = str, *p = pat, *star_p = NULL, *star_s = NULL;
    while (*s){
        if (*p == '.'){ while (*p == '.') p++; star_p = p; star_s = s; }
        else if (*p && *p == *s){ p++; s++; }
        else if (star_p){ s = ++star_s; p = star_p; }
        else return 0;
    }
    while (*p == '.') p++;
    return *p == '\0';
}

static char *slurp(FILE *f){
    size_t cap = 4096, len = 0; char *buf = malloc(cap); size_t n;
    while ((n = fread(buf+len, 1, cap-len, f)) > 0){ len += n; if (len==cap){ cap*=2; buf=realloc(buf,cap);} }
    buf[len] = 0; return buf;
}

static char *run_capture(const char *cmd){
#ifdef _WIN32
    FILE *f = shell_popen(cmd);
#else
    FILE *f = popen(cmd, "r");
#endif
    if (!f) return strdup("");
    char *out = slurp(f); pclose(f); return out;
}

static char *canon_line(const char *line, size_t len){
    char *out = malloc(len+1); size_t o = 0; int ws = 0, started = 0;
    for (size_t i = 0; i < len; i++){
        char c = line[i];
        if (c == ' ' || c == '\t' || c == '\r'){ ws = 1; continue; }
        if (ws && started) out[o++] = ' ';
        ws = 0; started = 1; out[o++] = c;
    }
    out[o] = 0; return out;
}

static int texts_equal(const char *a, const char *b){
    const char *pa = a, *pb = b;
    for (;;){
        const char *ea = strchr(pa, '\n'), *eb = strchr(pb, '\n');
        size_t la = ea ? (size_t)(ea-pa) : strlen(pa);
        size_t lb = eb ? (size_t)(eb-pb) : strlen(pb);
        char *ca = canon_line(pa, la), *cb = canon_line(pb, lb);
        int eq = !strcmp(ca, cb) || (has_dot_run(ca) && glob_eq(ca, cb));
        free(ca); free(cb);
        if (!eq) return 0;
        if (!ea && !eb) return 1;
        pa = ea ? ea+1 : pa+la; pb = eb ? eb+1 : pb+lb;
        if (!ea && *pa == 0){
            while (eb){ const char *n=strchr(pb,'\n'); size_t l=n?(size_t)(n-pb):strlen(pb);
                char *c=canon_line(pb,l); int blank=(*c==0); free(c); if(!blank) return 0; if(!n)break; pb=n+1; if(*pb==0)break; }
            return 1;
        }
        if (!eb && *pb == 0){
            while (ea){ const char *n=strchr(pa,'\n'); size_t l=n?(size_t)(n-pa):strlen(pa);
                char *c=canon_line(pa,l); int blank=(*c==0); free(c); if(!blank) return 0; if(!n)break; pa=n+1; if(*pa==0)break; }
            return 1;
        }
    }
}


static char *subst(const char *cmd, const char *mcc, const char *b,
                   const char *i, const char *w, const char *d, const char *t){
    size_t cap = strlen(cmd) * 4 + 256, o = 0;
    char *out = malloc(cap);
    for (const char *p = cmd; *p; ){
        const char *rep = NULL;
        if      (!strncmp(p, "{MCC}", 5)){ rep = mcc; p += 5; }
        else if (!strncmp(p, "{B}", 3)){ rep = b; p += 3; }
        else if (!strncmp(p, "{I}", 3)){ rep = i; p += 3; }
        else if (!strncmp(p, "{W}", 3)){ rep = w; p += 3; }
        else if (!strncmp(p, "{D}", 3)){ rep = d; p += 3; }
        else if (!strncmp(p, "{TIMEOUT}", 9)){ rep = t; p += 9; }
        if (rep){
            size_t rl = strlen(rep);
            while (o + rl + 1 >= cap){ cap *= 2; out = realloc(out, cap); }
            memcpy(out + o, rep, rl); o += rl;
        } else {
            if (o + 2 >= cap){ cap *= 2; out = realloc(out, cap); }
            out[o++] = *p++;
        }
    }
    out[o] = 0; return out;
}

int main(int argc, char **argv){
    if (argc < 6){ fprintf(stderr, "usage: %s <mcc> <bdir> <idir> <workdir> <clidir>\n", argv[0]); return 2; }
     



#ifdef _WIN32
    _putenv("LC_ALL=C");
#else
    setenv("LC_ALL", "C", 1);
#endif
    const char *mcc = argv[1], *bdir = argv[2], *idir = argv[3], *work = argv[4], *cdir = argv[5];
#ifdef _WIN32
    g_work = work;
#endif
    const char *tmo = timeout_prefix();
    if (MKDIR(work) != 0 && errno != EEXIST) {
        fprintf(stderr, "cannot create workdir %s\n", work); return 2; }

    int pass = 0, fail = 0, skipped = 0;
    for (int i = 0; i < cli_cases_count; i++){
        const cli_case_t *c = &cli_cases[i];
        char reason[256];
        if (!req_met(c->req, reason, sizeof reason)){
            printf("SKIP  %-28s -- %s\n", c->name, reason); skipped++; continue;
        }
        char *full = subst(c->cmd, mcc, bdir, idir, work, cdir, tmo);
        char *out = run_capture(full);
        if (texts_equal(c->expect, out)){
            printf("ok    %s\n", c->name); pass++;
        } else {
            fail++;
            printf("FAIL  %-28s\n  cmd: %s\n  --- expected ---\n%s\n  --- got ---\n%s\n",
                   c->name, full, c->expect, out);
        }
        free(full); free(out);
    }
    printf("cli runner: %d passed, %d failed, %d skipped (of %d)\n",
           pass, fail, skipped, cli_cases_count);
    if (fail) return 1;
    if (pass == 0 && skipped > 0) return MCC_SKIP_RC;
    return 0;
}
