#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/utsname.h>

static const char *CC  = "cc";
static const char *OUT = "build-c";

static int run(const char *fmt, ...){
    char cmd[8192];
    va_list ap; va_start(ap, fmt); vsnprintf(cmd, sizeof cmd, fmt, ap); va_end(ap);
    printf("  %s\n", cmd);
    int rc = system(cmd);
    if (rc != 0) fprintf(stderr, "build: command failed (%d): %s\n", rc, cmd);
    return rc;
}

static const char *RT_OBJS[] = {
    "libmcc1", "alloca", "alloca-bt", "atomic", "stdatomic", "builtin",
    "va_list", "dsohandle", "tcov", 0
};

int main(int argc, char **argv){
    int do_run = 0;
    const char *envcc = getenv("CC");
    if (envcc && *envcc) CC = envcc;
    for (int i = 1; i < argc; i++){
        if (!strcmp(argv[i], "--cc") && i+1 < argc) CC = argv[++i];
        else if (!strcmp(argv[i], "--out") && i+1 < argc) OUT = argv[++i];
        else if (!strcmp(argv[i], "--run")) do_run = 1;
        else { fprintf(stderr, "usage: build [--cc <cc>] [--out <dir>] [--run]\n"); return 2; }
    }

    struct utsname u; uname(&u);

    printf("build: cc=%s out=%s target=x86_64 (%s)\n", CC, OUT, u.release);
    if (run("mkdir -p \"%s/include\"", OUT)) return 1;

    printf("[1/3] cc mcc.c -> mcc\n");
    char defs[2048];
    snprintf(defs, sizeof defs,
        "-DMCC_TARGET_X86_64=1 -DCONFIG_MCC_PREDEFS=0 "
        "-DCONFIG_MCC_BACKTRACE=1 -DCONFIG_MCC_BCHECK=1 "
        "-DMCC_VERSION='\"1.0.0\"' "
        "-DCONFIG_MCCDIR='\"%s\"' "
        "-DCONFIG_MCC_LIBPATHS='\"{B}:/usr/lib64:/usr/lib:/lib\"' "
        "-DCONFIG_MCC_CRTPREFIX='\"/usr/lib64:/usr/lib:/lib\"' "
        "-DCONFIG_OS_RELEASE='\"%s\"'",
        OUT, u.release);
    if (run("\"%s\" -O2 -DONE_SOURCE=1 %s -Isrc -Iinclude "
            "-Isrc/formats -Isrc/objfmt -Isrc/arch/i386 -Isrc/arch/x86_64 "
            "-Isrc/arch/arm -Isrc/arch/arm64 -Isrc/arch/riscv64 "
            "-o \"%s/mcc\" src/mcc.c -lm -ldl -lpthread", CC, defs, OUT)) return 1;

    printf("[2/3] install runtime headers\n");
    if (run("cp runtime/include/*.h \"%s/include/\"", OUT)) return 1;

    printf("[3/3] mcc -> libmcc1.a\n");
    char objs[4096] = "";
    for (int i = 0; RT_OBJS[i]; i++){
        if (run("\"%s/mcc\" -B\"%s\" -c runtime/lib/%s.c -o \"%s/%s.o\"",
                OUT, OUT, RT_OBJS[i], OUT, RT_OBJS[i])) return 1;
        char one[256]; snprintf(one, sizeof one, " \"%s/%s.o\"", OUT, RT_OBJS[i]);
        strncat(objs, one, sizeof objs - strlen(objs) - 1);
    }
    if (run("\"%s/mcc\" -ar \"%s/libmcc1.a\"%s", OUT, OUT, objs)) return 1;

    printf("      + bcheck/backtrace runtime objects\n");
    const char *AINC = "-Isrc -Iinclude -Isrc/formats -Isrc/objfmt "
                       "-Isrc/arch/i386 -Isrc/arch/x86_64 -Isrc/arch/arm "
                       "-Isrc/arch/arm64 -Isrc/arch/riscv64";
    if (run("\"%s/mcc\" -B\"%s\" -c runtime/lib/bcheck.c -o \"%s/bcheck.o\" -bt "
            "-Iruntime/include %s", OUT, OUT, OUT, AINC)) return 1;
    const char *bt[] = { "bt-exe", "bt-log", "runmain", 0 };
    for (int i = 0; bt[i]; i++)
        if (run("\"%s/mcc\" -B\"%s\" -c runtime/lib/%s.c -o \"%s/%s.o\" -Iruntime/include %s",
                OUT, OUT, bt[i], OUT, bt[i], AINC)) return 1;

    printf("\nbuild: done -- %s/mcc (use: %s/mcc -B%s <file.c>)\n", OUT, OUT, OUT);

    if (do_run){
        printf("\n[smoke] compile + run a hello program\n");
        FILE *h = fopen("/tmp/_bld_hello.c", "w");
        fputs("#include <stdio.h>\nint main(){printf(\"build.c ok: %d\\n\",6*7);return 0;}\n", h);
        fclose(h);
        if (run("\"%s/mcc\" -B\"%s\" /tmp/_bld_hello.c -o /tmp/_bld_hello", OUT, OUT)) return 1;
        if (run("/tmp/_bld_hello")) return 1;
    }
    return 0;
}
