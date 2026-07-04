#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../support/hostcompat.h"
#ifdef _WIN32
#include <process.h>
#else
#include <sys/wait.h>
#endif
#include "libmcc.h"

static int g_argc;
static char **g_argv;

static const char *g_B, *g_I;
static int failures;

static void on_err(void *opaque, const char *msg) {
    fprintf(stderr, "mcc: %s\n", msg);
}

static MCCState *fresh(int output_type) {
    MCCState *s = mcc_new();
    mcc_set_error_func(s, NULL, on_err);
    if (g_B)
        mcc_set_lib_path(s, g_B);
    if (g_I)
        mcc_add_include_path(s, g_I);
    mcc_set_output_type(s, output_type);
    return s;
}

static void check(const char *name, int ok) {
    printf("%-16s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok)
        failures++;
}

struct collected {
    int alpha, beta, gamma;
    const void *alpha_val;
};

static void collect_cb(void *ctx, const char *name, const void *val) {
    struct collected *c = ctx;
    if (!strcmp(name, "alpha")) {
        c->alpha = 1;
        c->alpha_val = val;
    } else if (!strcmp(name, "beta"))
        c->beta = 1;
    else if (!strcmp(name, "gamma"))
        c->gamma = 1;
}

static void test_list_symbols(void) {
    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_compile_string(s,
                       "int alpha(void){return 1;}\n"
                       "int beta(void){return 2;}\n"
                       "int gamma = 3;\n");
    if (mcc_relocate(s) < 0) {
        check("list_symbols", 0);
        mcc_delete(s);
        return;
    }

    struct collected c = {0};
    mcc_list_symbols(s, &c, collect_cb);
    void *direct = mcc_get_symbol(s, "alpha");
    check("list_symbols",
          c.alpha && c.beta && c.gamma && c.alpha_val == direct && direct != NULL);
    mcc_delete(s);
}

static void test_undefine_symbol(void) {
    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_define_symbol(s, "FEATURE_X", "1");
    mcc_undefine_symbol(s, "FEATURE_X");

    int rc = mcc_compile_string(s,
                                "#ifdef FEATURE_X\n#error still defined\n#endif\nint ok = 1;\n");
    check("undefine_symbol", rc == 0);
    mcc_delete(s);
}

static void test_add_library(void) {
    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_compile_string(s,
                       "extern double sqrt(double);\n"
                       "double mysqrt(double x){ return sqrt(x); }\n");
    mcc_add_library(s, "m");
    if (mcc_relocate(s) < 0) {
        check("add_library", 0);
        mcc_delete(s);
        return;
    }
    double (*f)(double) = mcc_get_symbol(s, "mysqrt");
    check("add_library", f != NULL && f(16.0) == 4.0);
    mcc_delete(s);
}

static void test_run_argv(void) {
    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_compile_string(s,
                       "#include <stdlib.h>\n"
                       "int main(int argc, char **argv){ return argc >= 2 ? atoi(argv[1]) : -1; }\n");
    char *av[] = {"prog", "42", NULL};
    int rc = mcc_run(s, 2, av);
    check("run_argv", rc == 42);
    mcc_delete(s);
}

static void test_output_obj(void) {
    MCCState *s = fresh(MCC_OUTPUT_OBJ);
    mcc_compile_string(s, "int produced_symbol(int x){ return x + 1; }\n");
    const char *path = "api_extra_out.o";
    int wrote = mcc_output_file(s, path);
    mcc_delete(s);

    int ok = (wrote == 0);
    if (ok) {
        FILE *f = fopen(path, "rb");
        unsigned char m[4] = {0};

        ok = f && fread(m, 1, 4, f) == 4 &&
             m[0] == 0x7f && m[1] == 'E' && m[2] == 'L' && m[3] == 'F';
        if (f)
            fclose(f);
        remove(path);
    }
    check("output_obj", ok);
}

static long realloc_calls;
static void *counting_realloc(void *p, unsigned long n) {
    realloc_calls++;
    return realloc(p, n);
}

static void test_set_realloc(void) {
    realloc_calls = 0;
    mcc_set_realloc(counting_realloc);
    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_compile_string(s, "int q(void){ return 9; }");
    int ok = mcc_relocate(s) >= 0;
    int (*f)(void) = mcc_get_symbol(s, "q");
    ok = ok && f && f() == 9;
    mcc_delete(s);
    mcc_set_realloc((MCCReallocFunc *)realloc);
    check("set_realloc", ok && realloc_calls > 0);
}

static void test_add_sysinclude(void) {
    const char *dir = "api_extra_sysinc";
    const char *hdr = "api_extra_sysinc/sysapi.h";
    HC_MKDIR(dir);
    FILE *hf = fopen(hdr, "wb");
    if (!hf) {
        check("add_sysinclude", 0);
        return;
    }
    fputs("#define SYSAPI 77\n", hf);
    fclose(hf);

    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_add_sysinclude_path(s, dir);
    int rc = mcc_compile_string(s,
                                "#include <sysapi.h>\nint getv(void){ return SYSAPI; }");
    int ok = rc == 0 && mcc_relocate(s) >= 0;
    int (*f)(void) = ok ? mcc_get_symbol(s, "getv") : NULL;
    ok = ok && f && f() == 77;
    mcc_delete(s);
    remove(hdr);
    HC_RMDIR(dir);
    check("add_sysinclude", ok);
}

#define GUARD_FLAG "--relocate-guard-child"

static void run_guard_body(void) {
    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_compile_string(s, "int z(void){ return 0; }");
    mcc_relocate(s);
    mcc_relocate(s);
    _Exit(0);
}

static void test_relocate_double_guard(void) {

    char *av[5];
    int n = 0;
    av[n++] = g_argv[0];
    av[n++] = (char *)GUARD_FLAG;
    for (int i = 1; i < g_argc && n < 4; i++)
        if (!strncmp(g_argv[i], "-B", 2) || !strncmp(g_argv[i], "-I", 2))
            av[n++] = g_argv[i];
    av[n] = NULL;

    fflush(stdout);
#ifdef _WIN32
    intptr_t rc = _spawnv(_P_WAIT, av[0], (const char *const *)av);
    check("relocate_double_guard", rc != 0);
#else
    pid_t pid = fork();
    if (pid == 0) {
        execv(av[0], av);
        _Exit(127);
    }
    int st;
    waitpid(pid, &st, 0);
    check("relocate_double_guard", WIFEXITED(st) && WEXITSTATUS(st) != 0);
#endif
}

int main(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;
    int guard_child = 0;
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-B", 2))
            g_B = argv[i] + 2;
        else if (!strncmp(argv[i], "-I", 2))
            g_I = argv[i] + 2;
        else if (!strcmp(argv[i], GUARD_FLAG))
            guard_child = 1;
    }
    if (guard_child) {
        run_guard_body();
        return 0;
    }
    test_list_symbols();
    test_undefine_symbol();
    test_add_library();
    test_run_argv();
    test_output_obj();
    test_set_realloc();
    test_add_sysinclude();
    test_relocate_double_guard();
    printf("api_extra: %s\n", failures ? "FAILURES" : "all passed");
    return failures ? 1 : 0;
}
