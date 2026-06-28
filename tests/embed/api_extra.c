/* Exercises libmcc API surface not touched by api_basic.c / api_threaded.c:
 *   - mcc_list_symbols      (enumeration callback; previously zero callers)
 *   - mcc_undefine_symbol   (cancels a prior define)
 *   - mcc_add_library       (resolve -l<name> from the embedding API)
 *   - mcc_run               (argv passthrough + return-code propagation)
 *   - mcc_output_file       (MCC_OUTPUT_OBJ writes a real ELF object)
 * Self-checking: prints one line per check and exits non-zero on any failure.
 * Invoked like libmcc_test, with -B<builddir> and -I<runtime/include>. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "libmcc.h"

static const char *g_B, *g_I;
static int failures;

static void on_err(void *opaque, const char *msg) { fprintf(stderr, "mcc: %s\n", msg); }

static MCCState *fresh(int output_type)
{
    MCCState *s = mcc_new();
    mcc_set_error_func(s, NULL, on_err);
    if (g_B) mcc_set_lib_path(s, g_B);
    if (g_I) mcc_add_include_path(s, g_I);
    mcc_set_output_type(s, output_type);
    return s;
}

static void check(const char *name, int ok)
{
    printf("%-16s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) failures++;
}

/* ---- mcc_list_symbols ------------------------------------------------- */
struct collected { int alpha, beta, gamma; const void *alpha_val; };

static void collect_cb(void *ctx, const char *name, const void *val)
{
    struct collected *c = ctx;
    if (!strcmp(name, "alpha")) { c->alpha = 1; c->alpha_val = val; }
    else if (!strcmp(name, "beta"))  c->beta = 1;
    else if (!strcmp(name, "gamma")) c->gamma = 1;
}

static void test_list_symbols(void)
{
    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_compile_string(s,
        "int alpha(void){return 1;}\n"
        "int beta(void){return 2;}\n"
        "int gamma = 3;\n");
    if (mcc_relocate(s) < 0) { check("list_symbols", 0); mcc_delete(s); return; }

    struct collected c = {0};
    mcc_list_symbols(s, &c, collect_cb);
    void *direct = mcc_get_symbol(s, "alpha");
    check("list_symbols",
          c.alpha && c.beta && c.gamma && c.alpha_val == direct && direct != NULL);
    mcc_delete(s);
}

/* ---- mcc_undefine_symbol --------------------------------------------- */
static void test_undefine_symbol(void)
{
    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_define_symbol(s, "FEATURE_X", "1");
    mcc_undefine_symbol(s, "FEATURE_X");
    /* If FEATURE_X is still defined this #error aborts the compile. */
    int rc = mcc_compile_string(s,
        "#ifdef FEATURE_X\n#error still defined\n#endif\nint ok = 1;\n");
    check("undefine_symbol", rc == 0);
    mcc_delete(s);
}

/* ---- mcc_add_library -------------------------------------------------- */
static void test_add_library(void)
{
    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_compile_string(s,
        "extern double sqrt(double);\n"
        "double mysqrt(double x){ return sqrt(x); }\n");
    mcc_add_library(s, "m");
    if (mcc_relocate(s) < 0) { check("add_library", 0); mcc_delete(s); return; }
    double (*f)(double) = mcc_get_symbol(s, "mysqrt");
    check("add_library", f != NULL && f(16.0) == 4.0);
    mcc_delete(s);
}

/* ---- mcc_run: argv passthrough + return code ------------------------- */
static void test_run_argv(void)
{
    MCCState *s = fresh(MCC_OUTPUT_MEMORY);
    mcc_compile_string(s,
        "#include <stdlib.h>\n"
        "int main(int argc, char **argv){ return argc >= 2 ? atoi(argv[1]) : -1; }\n");
    char *av[] = { "prog", "42", NULL };
    int rc = mcc_run(s, 2, av);
    check("run_argv", rc == 42);
    mcc_delete(s);
}

/* ---- mcc_output_file: MCC_OUTPUT_OBJ --------------------------------- */
static void test_output_obj(void)
{
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
        if (f) fclose(f);
        remove(path);
    }
    check("output_obj", ok);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-B", 2)) g_B = argv[i] + 2;
        else if (!strncmp(argv[i], "-I", 2)) g_I = argv[i] + 2;
    }
    test_list_symbols();
    test_undefine_symbol();
    test_add_library();
    test_run_argv();
    test_output_obj();
    printf("api_extra: %s\n", failures ? "FAILURES" : "all passed");
    return failures ? 1 : 0;
}
