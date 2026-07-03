/*
 *  dashsbytes.c - byte-exact `-S` roundtrip driver (PLAN 0.6 #3)
 *
 *  Ports tests/asm/run_dash_s_bytes.cmake: on the fixed-width targets
 *  (arm64, riscv64) `mcc -c prog.c` and `mcc -c (mcc -S prog.c)` must produce
 *  identical section contents.  Compiles the three artifacts shell-free via
 *  the host layer and defers the section-aware comparison to seccmp.
 *
 *  usage: dashsbytes <mcc> <seccmp> <src.c> <workdir>
 *  exit:  0 identical, 1 roundtrip differs / compile failed, 2 usage.
 */
#include "toolsupport.h"

int main(int argc, char **argv)
{
    const char *mcc, *seccmp, *src, *work;
    char ref[4096], asmf[4096], rt[4096];

    if (argc != 5) {
        fprintf(stderr, "usage: dashsbytes <mcc> <seccmp> <src.c> <workdir>\n");
        return 2;
    }
    mcc = argv[1]; seccmp = argv[2]; src = argv[3]; work = argv[4];

    if (host_mkdirs(work)) {
        fprintf(stderr, "dashsbytes: cannot create %s\n", work);
        return 2;
    }
    snprintf(ref,  sizeof ref,  "%s/ref.o",  work);
    snprintf(asmf, sizeof asmf, "%s/prog.s", work);
    snprintf(rt,   sizeof rt,   "%s/rt.o",   work);

    {
        const char *c_ref[] = { mcc, "-c", src,  "-o", ref,  0 };
        const char *c_asm[] = { mcc, "-S", src,  "-o", asmf, 0 };
        const char *c_rt[]  = { mcc, "-c", asmf, "-o", rt,   0 };
        if (ts_run(c_ref) || ts_run(c_asm) || ts_run(c_rt)) {
            fprintf(stderr, "dashsbytes: a compile step failed\n");
            return 1;
        }
    }
    {
        const char *cmp[] = { seccmp, ref, rt, 0 };
        if (ts_run(cmp)) {
            fprintf(stderr, "-S roundtrip is not byte-identical\n");
            return 1;
        }
    }
    return 0;
}
