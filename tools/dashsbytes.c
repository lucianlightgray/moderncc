#include "toolsupport.h"

int main(int argc, char **argv) {
    const char *mcc, *seccmp, *src, *work;
    char ref[4096], asmf[4096], rt[4096];

    if (argc != 5) {
        fprintf(stderr, "usage: dashsbytes <mcc> <seccmp> <src.c> <workdir>\n");
        return 2;
    }
    mcc = argv[1];
    seccmp = argv[2];
    src = argv[3];
    work = argv[4];

    if (host_mkdirs(work)) {
        fprintf(stderr, "dashsbytes: cannot create %s\n", work);
        return 2;
    }
    ts_path(ref, sizeof ref, work, "ref.o");
    ts_path(asmf, sizeof asmf, work, "prog.s");
    ts_path(rt, sizeof rt, work, "rt.o");

    {
        const char *c_ref[] = {mcc, "-c", src, "-o", ref, 0};
        const char *c_asm[] = {mcc, "-S", src, "-o", asmf, 0};
        const char *c_rt[] = {mcc, "-c", asmf, "-o", rt, 0};
        if (ts_run(c_ref) || ts_run(c_asm) || ts_run(c_rt)) {
            fprintf(stderr, "dashsbytes: a compile step failed\n");
            return 1;
        }
    }
    {
        const char *cmp[] = {seccmp, ref, rt, 0};
        if (ts_run(cmp)) {
            fprintf(stderr, "-S roundtrip is not byte-identical\n");
            return 1;
        }
    }
    return 0;
}
