#define CC_unknown 0
#define CC_gcc 1
#define CC_clang 2
#define CC_mcc 3
#define CC_msvc 4


#define ALL_ISOC99


#define BOOL_ISOC99


#define C99_MACROS

#if defined(_WIN32) \
	|| (defined(__arm__) \
		&& (defined(__FreeBSD__) \
		 || defined(__OpenBSD__) \
		 || defined(__NetBSD__) \
		 || defined __ANDROID__))
#define LONG_LONG_FORMAT "%lld"
#define ULONG_LONG_FORMAT "%llu"
#define XLONG_LONG_FORMAT "%llx"
#else
#define LONG_LONG_FORMAT "%Ld"
#define ULONG_LONG_FORMAT "%Lu"
#define XLONG_LONG_FORMAT "%Lx"
#endif



#if defined(_WIN32)
#define LONG_DOUBLE double
#define LONG_DOUBLE_LITERAL(x) x
#else
#define LONG_DOUBLE long double
#define LONG_DOUBLE_LITERAL(x) x ## L
#endif

typedef __SIZE_TYPE__ uintptr_t;



#define MCCLIB_INC <mcclib.h>
#define MCCLIB_INC1 <mcclib
#define MCCLIB_INC2 h>
#define MCCLIB_INC3 "mcclib.h"

#include MCCLIB_INC

#include MCCLIB_INC1.MCCLIB_INC2

#include MCCLIB_INC1.h>

#include MCCLIB_INC3

#include <mcclib.h>

#include "mcclib.h"

#include "full_language.h"


#define INC(name) <tests/diff/name.h>
#define funnyname 42test.h
#define incdir tests/diff/
#ifdef __clang__


#define incname <tests/diff/42test.h>
#else
#define incname < incdir funnyname >
#endif
#define __stringify(x) #x
#define stringify(x) __stringify(x)
#include INC(42test)
#include incname
#include stringify(funnyname)

int fib(int n);
void num(int n);
void forward_ref(void);
int isid(int c);



void funny_line_continuation (int, ..\
. );

#define A 2
#define N 1234 + A
#define pf printf
#define M1(a, b)  (a) + (b)

#define str\
(s) # s
#define glue(a, b) a ## b
#define xglue(a, b) glue(a, b)
#define HIGHLOW "hello"
#define LOW LOW ", world"

static int onetwothree = 123;
#define onetwothree4 onetwothree
#define onetwothree xglue(onetwothree,4)

#define min(a, b) ((a) < (b) ? (a) : (b))

#ifdef C99_MACROS
#define dprintf(level,...) printf(__VA_ARGS__)
#endif


#define dprintf1(level, fmt, args...) printf(fmt, ## args)

#define MACRO_NOARGS()

#define TEST_CALL(f, ...) f(__VA_ARGS__)
#define TEST_CONST()      123

#define AAA 3
#undef AAA
#define AAA 4

#if 1
#define B3 1
#elif 1
#define B3 2
#elif 0
#define B3 3
#else
#define B3 4
#endif

#define __INT64_C(c)	c ## LL
#define INT64_MIN	(-__INT64_C(9223372036854775807)-1)

int qq(int x)
{
	return x + 40;
}
#define qq(x) x

#define spin_lock(lock) do { } while (0)
#define wq_spin_lock spin_lock
#define TEST2() wq_spin_lock(a)


#include "parts/legacy_preproc.h"
#include "parts/legacy_expr.h"
#include "parts/legacy_aggregates.h"
#include "parts/legacy_numeric.h"
#include "parts/legacy_args.h"
#include "parts/legacy_meta.h"
#include "parts/legacy_builtins.h"
#define RUN(test) puts("---- " #test " ----"), test(), puts("")







#include <complex.h>
#include <ctype.h>
#include <errno.h>
#include <fenv.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdalign.h>
#include <time.h>
#include <locale.h>
#include <wctype.h>
#include <inttypes.h>
#include <stdnoreturn.h>
#include <limits.h>
#include <float.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <tgmath.h>
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;
void *bsearch(const void *, const void *, size_t, size_t, int (*)(const void *, const void *));
int memcmp(const void *, const void *, size_t);
void *memchr(const void *, int, size_t);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
char *strncpy(char *, const char *, size_t);
char *strncat(char *, const char *, size_t);
char *strstr(const char *, const char *);
char *strpbrk(const char *, const char *);
size_t strspn(const char *, const char *);
size_t strcspn(const char *, const char *);
char *strtok(char *, const char *);
size_t strxfrm(char *, const char *, size_t);
int strcoll(const char *, const char *);
int sscanf(const char *, const char *, ...);
int vsscanf(const char *, const char *, va_list);
long long strtoll(const char *, char **, int);
unsigned long long strtoull(const char *, char **, int);
long atol(const char *);
long long atoll(const char *);
int abs(int);
long labs(long);
long long llabs(long long);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
div_t div(int, int);
ldiv_t ldiv(long, long);
lldiv_t lldiv(long long, long long);

#undef A
#undef B
#undef N
#undef str






#if defined(_WIN32)
# define MCC_HAS_C99_LIBM 0
#else
# define MCC_HAS_C99_LIBM 1
#endif

#include "parts/s04.h"
#include "parts/s6_2_1.h"
#include "parts/s6_2_5.h"
#include "parts/s6_3.h"
#include "parts/s6_4.h"
#include "parts/s6_5_1.h"
#include "parts/s6_5_5.h"
#include "parts/s6_5_15.h"
#include "parts/s6_7_1.h"
#include "parts/s6_7_2.h"
#include "parts/s6_7_4.h"
#include "parts/s6_7_6.h"
#include "parts/s6_7_7.h"
#include "parts/s6_8.h"
#include "parts/s6_9.h"
#include "parts/s6_10_1.h"
#include "parts/s6_10_4.h"
#if MCC_HAS_C99_LIBM
#include "parts/s7_1.h"
#include "parts/s7_6.h"
#endif
#include "parts/s7_9.h"
#if MCC_HAS_C99_LIBM
#include "parts/s7_12.h"
#endif
#include "parts/s7_13.h"
#include "parts/s7_16.h"
#include "parts/s_stddef.h"
#if MCC_HAS_C99_LIBM
#include "parts/s7_21.h"
#include "parts/s7_22.h"
#include "parts/s7_23.h"
#endif
#include "parts/s7_25.h"
#include "parts/s_annFGK.h"
#include "parts/s_annCDE.h"
#if MCC_HAS_C99_LIBM
#include "parts/s7_libm.h"
#endif
#include "parts/coherency.h"

int main(int argc, char **argv)
{
	RUN(whitespace_test);
	RUN(macro_test);
	RUN(recursive_macro_test);
	RUN(string_test);
	RUN(expr_test);
	RUN(scope_test);
	RUN(scope2_test);
	RUN(forward_test);
	RUN(funcptr_test);
	RUN(if_test);
	RUN(loop_test);
	RUN(switch_test);
	RUN(goto_test);
	RUN(enum_test);
	RUN(typedef_test);
	RUN(struct_test);
	RUN(array_test);
	RUN(expr_ptr_test);
	RUN(bool_test);
	RUN(optimize_out_test);
	RUN(expr2_test);
	RUN(constant_expr_test);
	RUN(expr_cmp_test);
	RUN(char_short_test);
	RUN(init_test);
	RUN(compound_literal_test);
	RUN(kr_test);
	RUN(struct_assign_test);
	RUN(cast_test);
	RUN(bitfield_test);
	RUN(c99_bool_test);
	RUN(float_test);
	RUN(longlong_test);
	RUN(manyarg_test);
	RUN(stdarg_test);
	RUN(relocation_test);
	RUN(old_style_function_test);
	RUN(alloca_test);
	RUN(c99_vla_test);
	RUN(sizeof_test);
	RUN(typeof_test);
	RUN(statement_expr_test);
	RUN(local_label_test);
	RUN(asm_test);
	RUN(builtin_test);
	RUN(weak_test);
	RUN(global_data_test);
	RUN(cmp_comparison_test);
	RUN(math_cmp_test);
	RUN(callsave_test);
	RUN(builtin_frame_address_test);
	RUN(volatile_test);
	RUN(attrib_test);
	RUN(bounds_check1_test);
	RUN(func_arg_test);



	RUN(s04_charset_test);
	RUN(s04_ident_significance_test);
	RUN(s04_limits_test);
	RUN(s04_float_limits_test);
	RUN(s6_2_1_scopes_test);
	RUN(s6_2_5_types);
	RUN(s6_3_conversions);
	RUN(s6_4_lexical_test);
	RUN(s6_5_1_expr);
	RUN(s6_5_5_binary_ops);
	RUN(s6_5_15_cond_test);
	RUN(s6_5_16_assign_test);
	RUN(s6_5_17_comma_test);
	RUN(s6_6_const_test);
	RUN(s6_7_1_storage_qual_test);
	RUN(s6_7_2_specifiers);
	RUN(s6_7_4_specifiers);
	RUN(s6_7_6_declarators);
	RUN(s6_7_7_init_both_ends);
	RUN(s6_7_7_init_inconsistent);
	RUN(s6_7_7_init_mixed);
	RUN(s6_7_7_init_elision);
	RUN(s6_7_7_init_union_next);
	RUN(s6_7_7_init_enum_desig);
	RUN(s6_7_7_init_sparse);
	RUN(s6_7_7_init_string);
	RUN(s6_7_7_typedef_vla);
	RUN(s6_7_7_typedef_incomplete);
	RUN(s6_7_7_init_flat);
	RUN(s6_8_statements);
	RUN(s6_9_extdef);
	RUN(s6_10_1_preproc_test);
	RUN(s6_10_4_preproc_test);
#if MCC_HAS_C99_LIBM
	RUN(s7_1_ctype);
	RUN(s7_1_complex);
	RUN(s7_1_errno);
	RUN(s7_6_float_test);
	RUN(s7_6_inttypes_test);
#endif
	RUN(s7_9_iso646_test);
	RUN(s7_9_limits_test);
	RUN(s7_9_locale_test);
#if MCC_HAS_C99_LIBM
	RUN(s7_12_classify);
	RUN(s7_12_trig_hyp);
	RUN(s7_12_explog);
	RUN(s7_12_powabs);
	RUN(s7_12_nearest_rem);
	RUN(s7_12_manip_cmp);
#endif
	RUN(s7_13_setjmp_signal_align);
	RUN(s7_16_stdbool);
	RUN(s_stddef_offsetof);
	RUN(s_stddef_stdint);
#if MCC_HAS_C99_LIBM
	RUN(s7_21_printf_flags);
	RUN(s7_21_length_mods);
	RUN(s7_21_floatconv);
	RUN(s7_21_snprintf_ret);
	RUN(s7_21_sscanf_read);
	RUN(s7_21_vfamily);
	RUN(s7_22_strtol_test);
	RUN(s7_22_intarith_test);
	RUN(s7_22_sortsearch_test);
	RUN(s7_22_mem_test);
	RUN(s7_23_string_test);
	RUN(s7_23_tgmath_test);
#endif
	RUN(s7_25_asctime);
	RUN(s7_25_strftime);
	RUN(s7_25_mktime_norm);
	RUN(s7_25_difftime);
	RUN(s_annFGK_annex_test);
	RUN(s_annCDE_seqpoint_test);
#if MCC_HAS_C99_LIBM
	RUN(s7_6_fenv_test);
	RUN(s7_1_complex_libm_test);
	RUN(s7_23_tgmath_eval_test);
#endif
	RUN(coherency_test);

	return 0;
}
