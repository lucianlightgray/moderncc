#ifndef LIBMCC_H
#define LIBMCC_H

#ifndef LIBMCCAPI
#define LIBMCCAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *MCCReallocFunc(void *ptr, unsigned long size);
LIBMCCAPI void mcc_set_realloc(MCCReallocFunc *my_realloc);

typedef struct MCCState MCCState;

LIBMCCAPI MCCState *mcc_new(void);

LIBMCCAPI void mcc_delete(MCCState *s);

LIBMCCAPI void mcc_set_lib_path(MCCState *s, const char *path);

typedef void MCCErrorFunc(void *opaque, const char *msg);
LIBMCCAPI void mcc_set_error_func(MCCState *s, void *error_opaque, MCCErrorFunc *error_func);

LIBMCCAPI int mcc_set_options(MCCState *s, const char *str);

LIBMCCAPI int mcc_add_include_path(MCCState *s, const char *pathname);

LIBMCCAPI int mcc_add_sysinclude_path(MCCState *s, const char *pathname);

LIBMCCAPI void mcc_define_symbol(MCCState *s, const char *sym, const char *value);

LIBMCCAPI void mcc_undefine_symbol(MCCState *s, const char *sym);

LIBMCCAPI int mcc_add_file(MCCState *s, const char *filename);

LIBMCCAPI int mcc_compile_string(MCCState *s, const char *buf);

LIBMCCAPI int mcc_set_output_type(MCCState *s, int output_type);
#define MCC_OUTPUT_MEMORY 1
#define MCC_OUTPUT_EXE 2
#define MCC_OUTPUT_DLL 4
#define MCC_OUTPUT_OBJ 3
#define MCC_OUTPUT_PREPROCESS 5

#define MCC_OUTPUT_ASM 8

LIBMCCAPI int mcc_add_library_path(MCCState *s, const char *pathname);

LIBMCCAPI int mcc_add_library(MCCState *s, const char *libraryname);

LIBMCCAPI int mcc_add_symbol(MCCState *s, const char *name, const void *val);

LIBMCCAPI int mcc_output_file(MCCState *s, const char *filename);

LIBMCCAPI int mcc_run(MCCState *s, int argc, char **argv);

LIBMCCAPI int mcc_relocate(MCCState *s1);

LIBMCCAPI void *mcc_get_symbol(MCCState *s, const char *name);

LIBMCCAPI void mcc_list_symbols(MCCState *s, void *ctx,
                                void (*symbol_cb)(void *ctx, const char *name, const void *val));

LIBMCCAPI void *_mcc_setjmp(MCCState *s1, void *jmp_buf, void *top_func, void *longjmp);
#define mcc_setjmp(s1, jb, f) setjmp(_mcc_setjmp(s1, jb, f, longjmp))

typedef int MCCBtFunc(void *udata, void *pc, const char *file, int line, const char *func, const char *msg);
LIBMCCAPI void mcc_set_backtrace_func(MCCState *s1, void *userdata, MCCBtFunc *);

#ifdef __cplusplus
}
#endif

#endif
