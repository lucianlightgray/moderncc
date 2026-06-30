/* diff3 -- three-way differential test runner: gcc vs clang vs mcc.
 *
 * For every portable "run" golden (the test list + per-test flags come straight
 * from tests/exec/goldens.h), this builds and runs the program with gcc, clang
 * and mcc, then compares PROGRAM STDOUT.  The reference compilers are the
 * "source of truth": when gcc and clang agree but mcc differs, that is reported
 * as an mcc divergence (non-zero exit).  Tests gcc/clang cannot build (they use
 * mcc-only features) are skipped, as are sources with compiler-specific
 * `#ifdef __MCC__` sections and bcheck/backtrace/arch-gated rows.
 *
 * Verbose logging is gated on a DEFINE: build with -DDIFF3_VERBOSE (or set the
 * env var MCC_DIFF3_VERBOSE=1) to print each compiler's build command and
 * captured output per test.  A unified-style mismatch dump is always printed
 * for real divergences.
 *
 * argv: <mcc> <bdir> <idir> <testroot> <workdir> <gcc> <clang>
 * env:  MCC_TEST_CPU, MCC_TEST_OS (gate cpu=/os= reqs, like the other runners)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goldens.h"

#define MCC_SKIP_RC 77

static int verbose;

static const char *envv(const char *k, const char *d){
    const char *v = getenv(k); return (v && *v) ? v : d;
}

/* req gate: skip arch/config-specific and intentionally-skipped rows. */
static int portable_req(const char *req){
    if (!req || !*req) return 1;
    if (strstr(req, "note:") || strstr(req, "bcheck") || strstr(req, "backtrace"))
        return 0;
    char buf[256]; snprintf(buf, sizeof buf, "%s", req);
    const char *cpu = envv("MCC_TEST_CPU", "unknown");
    const char *os  = envv("MCC_TEST_OS",  "unknown");
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")){
        while (*tok == ' ') tok++;
        if (!strncmp(tok, "cpu=", 4)){
            const char *want = tok + 4; int ok;
            if (!strcmp(want, "x86")) ok = !strcmp(cpu,"i386")||!strcmp(cpu,"x86_64");
            else ok = !strcmp(cpu, want);
            if (!ok) return 0;
        } else if (!strncmp(tok, "os=", 3)){
            if (strcmp(os, tok + 3)) return 0;
        } else if (!strncmp(tok, "os!=", 4)){
            /* os!=NAME[:reason] — skip when the target OS *is* NAME. */
            const char *want = tok + 4;
            const char *colon = strchr(want, ':');
            size_t wl = colon ? (size_t)(colon - want) : strlen(want);
            if (!strncmp(os, want, wl) && os[wl] == '\0') return 0;
        } else if (!strcmp(tok, "elf")){
            /* ELF symbol/section conventions; Mach-O and PE differ. */
            if (!strcmp(os, "Darwin") || !strcmp(os, "WIN32")) return 0;
        }
        /* "asm" and other tokens: gcc/clang have an assembler too -> keep. */
    }
    return 1;
}

static char *slurp(FILE *f){
    size_t cap = 4096, len = 0; char *b = malloc(cap); size_t n;
    while ((n = fread(b+len,1,cap-len,f)) > 0){ len+=n; if(len==cap){cap*=2;b=realloc(b,cap);} }
    b[len]=0; return b;
}

/* run a shell command, capture stdout; *status gets the exit code. */
static char *cap(const char *cmd, int *status){
    FILE *f = popen(cmd, "r");
    if (!f){ if(status)*status=-1; return strdup(""); }
    char *o = slurp(f);
    int rc = pclose(f);
    if (status) *status = rc;
    return o;
}

/* Wrap a program invocation with a wall-clock hang guard so a runaway golden
   (e.g. one that loops forever or blocks on stdin) can't stall the whole suite.
   GNU coreutils `timeout` is present on Linux CI but absent on macOS/BSD, so
   when neither it nor Homebrew's `gtimeout` exists, fall back to a dependency-
   free POSIX-shell watchdog: run the program in the background, arm a killer
   that SIGKILLs it after the deadline, and report the program's own status.
   The watchdog's stdout is redirected to /dev/null so it never holds the
   capture pipe open past the program's exit. `out` must hold >= strlen(cmd)+256. */
#define DIFF3_RUN_TIMEOUT 20
static void timeout_wrap(const char *cmd, char *out, size_t n){
    static int probed, have_timeout, have_gtimeout;
    if (!probed){
        have_timeout  = system("command -v timeout  >/dev/null 2>&1") == 0;
        have_gtimeout = system("command -v gtimeout >/dev/null 2>&1") == 0;
        probed = 1;
    }
    if (have_timeout)
        snprintf(out, n, "timeout %d %s", DIFF3_RUN_TIMEOUT, cmd);
    else if (have_gtimeout)
        snprintf(out, n, "gtimeout %d %s", DIFF3_RUN_TIMEOUT, cmd);
    else
        /* POSIX fallback: background the program, kill it after the deadline,
           and exit with the program's own status (137 if the watchdog fired). */
        snprintf(out, n,
            "{ %s & __p=$!; ( sleep %d; kill -9 $__p ) >/dev/null 2>&1 & __w=$!; "
            "wait $__p; __r=$?; kill $__w 2>/dev/null; exit $__r; }",
            cmd, DIFF3_RUN_TIMEOUT);
}

static int file_has(const char *path, const char *needle){
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char *s = slurp(f); fclose(f);
    int hit = strstr(s, needle) != NULL;
    free(s); return hit;
}

/* The three-way differential treats "gcc and clang agree but mcc differs" as an
   mcc divergence. That inference only holds when gcc and clang are *distinct*
   implementations: implementation-defined goldens are absorbed by the
   gcc!=clang branch (counted as impl-defined, not a failure). On hosts where
   `gcc` and `clang` resolve to the SAME compiler -- e.g. macOS, where
   /usr/bin/gcc and /usr/bin/clang are both Apple clang -- the two references
   trivially agree on everything, so every intentional/impl-defined divergence
   (cleanup, bitfields_ms, predefined_macros, ...) is misreported as a false mcc
   failure, and the suite can no longer distinguish a real bug from a sanctioned
   choice. Detect that by comparing `--version` output and skip with a reason,
   matching the suite's "needs BOTH reference compilers" precondition. */
static int same_compiler(const char *a, const char *b){
    char cmd[4096];
    snprintf(cmd, sizeof cmd, "\"%s\" --version 2>/dev/null", a);
    char *va = cap(cmd, NULL);
    snprintf(cmd, sizeof cmd, "\"%s\" --version 2>/dev/null", b);
    char *vb = cap(cmd, NULL);
    int same = va[0] && !strcmp(va, vb);
    free(va); free(vb);
    return same;
}

/* substitute {SELF} -> src in a flags/args string */
static void sub_self(const char *in, const char *src, char *out, size_t n){
    char *w = out; const char *a = in;
    while (*a && (size_t)(w-out) < n-1){
        if (!strncmp(a, "{SELF}", 6)){ w += snprintf(w, n-(w-out), "%s", src); a += 6; }
        else *w++ = *a++;
    }
    *w = 0;
}

/* Build with `cc` (NULL => mcc) and run; returns 1 if built, fills *out (stdout). */
static int build_run(const char *label, const char *cc, const char *mcc,
                     const char *bdir, const char *idir, const char *sup,
                     const char *work, const char *src, const char *flags,
                     const char *args, char **out){
    char exe[2048], cmd[8192];
    snprintf(exe, sizeof exe, "%s/%s.out", work, label);
    remove(exe);
    if (cc) /* reference compiler */
        snprintf(cmd, sizeof cmd,
            "%s -w -O0 \"-I%s\" %s \"%s\" -o \"%s\" >/dev/null 2>&1",
            cc, sup, flags, src, exe);
    else    /* mcc */
        snprintf(cmd, sizeof cmd,
            "\"%s\" \"-B%s\" \"-I%s\" \"-I%s\" %s \"%s\" -o \"%s\" >/dev/null 2>&1",
            mcc, bdir, idir, sup, flags, src, exe);
    if (verbose) fprintf(stderr, "  [%s build] %s\n", label, cmd);
    char gbuild[8448];
    timeout_wrap(cmd, gbuild, sizeof gbuild);   /* a pathological source must
        not be able to hang a compiler forever (cleanup.c can spin Apple clang's
        codegen); the hang guard caps every build, not just the run. */
    int brc = system(gbuild);
    if (brc != 0){ *out = strdup(""); return 0; }

    char run[8192], guarded[8448]; int rrc;
    snprintf(run, sizeof run, "cd \"%s\" && \"%s\" %s 2>/dev/null", work, exe, args);
    timeout_wrap(run, guarded, sizeof guarded);
    *out = cap(guarded, &rrc);
    if (verbose) fprintf(stderr, "  [%s out] %s\n", label, *out);
    return 1;
}

int main(int argc, char **argv){
    if (argc < 8){
        fprintf(stderr, "usage: %s <mcc> <bdir> <idir> <root> <work> <gcc> <clang>\n", argv[0]);
        return 2;
    }
    const char *mcc=argv[1], *bdir=argv[2], *idir=argv[3], *root=argv[4],
               *work=argv[5], *gcc=argv[6], *clang=argv[7];
    char sup[2048]; snprintf(sup, sizeof sup, "%s/support", root);
    if (envv("MCC_DIFF3_VERBOSE", "")[0]) verbose = 1;
#ifdef DIFF3_VERBOSE
    verbose = 1;
#endif

    if (same_compiler(gcc, clang)){
        printf("diff3: SKIP -- '%s' and '%s' are the same compiler; a three-way "
               "differential needs two distinct references (the exec golden suite "
               "still covers these programs)\n", gcc, clang);
        return MCC_SKIP_RC;
    }

    char cmd[4096];
    snprintf(cmd, sizeof cmd, "mkdir -p \"%s\"", work);
    if (system(cmd)){ fprintf(stderr, "cannot mkdir %s\n", work); return 2; }

    int pass=0, mcc_diff=0, impl=0, skip=0, mcc_build_fail=0, mcc_only=0;
    for (int i=0;i<mcc_goldens_count;i++){
        const mcc_golden_t *g = &mcc_goldens[i];
        if (strcmp(g->mode,"run") && strcmp(g->mode,"run2")) continue;
        if (!portable_req(g->req)) { continue; }

        char src[2048]; snprintf(src,sizeof src,"%s/%s",root,g->src);
        /* compiler-specific sections we can't replicate under gcc/clang */
        if (file_has(src, "__MCC__")){
            printf("MCC-ONLY %-28s -- has #ifdef __MCC__ section\n", g->name);
            mcc_only++; continue;
        }
        char flags[4096], args[4096];
        sub_self(g->flags, src, flags, sizeof flags);
        sub_self(g->args,  src, args,  sizeof args);

        if (verbose){ fprintf(stderr, "RUN   %s\n", g->name); fflush(stderr); }
        char *gout,*cout,*mout;
        int gok = build_run("gcc",   gcc,  NULL, bdir,idir,sup,work,src,flags,args,&gout);
        int cok = build_run("clang", clang,NULL, bdir,idir,sup,work,src,flags,args,&cout);
        int mok = build_run("mcc",   NULL, mcc,  bdir,idir,sup,work,src,flags,args,&mout);

        if (!mok){
            printf("FAIL  %-28s -- mcc failed to build\n", g->name);
            mcc_build_fail++;
        } else if (!gok || !cok){
            if (verbose) printf("SKIP  %-28s -- reference compiler can't build (mcc-only)\n", g->name);
            skip++;
        } else if (!strcmp(gout,cout) && !strcmp(mout,gout)){
            if (verbose) printf("ok    %s\n", g->name);
            pass++;
        } else if (!strcmp(gout,cout) && strcmp(mout,gout)){
            printf("FAIL  %-28s -- mcc differs from gcc==clang consensus\n", g->name);
            printf("  gcc==clang: %s\n  mcc       : %s\n", gout, mout);
            mcc_diff++;
        } else {
            printf("INFO  %-28s -- gcc != clang (implementation-defined)\n", g->name);
            impl++; pass++;
        }
        free(gout); free(cout); free(mout);
    }
    printf("diff3: %d agree, %d mcc-divergence, %d impl-defined, "
           "%d ref-cant-build, %d mcc-only, %d mcc-build-fail\n",
           pass, mcc_diff, impl, skip, mcc_only, mcc_build_fail);
    if (mcc_diff || mcc_build_fail) return 1;
    if (pass == 0) return MCC_SKIP_RC;
    return 0;
}
