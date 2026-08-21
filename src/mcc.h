#ifndef _MCC_H
#define _MCC_H

#define MCC_INTERNAL 1

#define _GNU_SOURCE
#define _DARWIN_C_SOURCE

#include "mccdiag.h"
#include "mccdev.h"

#ifndef MCC_VERSION
#define MCC_VERSION 20260706135200
#endif
#define MCC_STRINGIFY_(x) #x
#define MCC_STRINGIFY(x) MCC_STRINGIFY_(x)
#define MCC_VERSION_STR MCC_STRINGIFY(MCC_VERSION)
#define MCC_VERSION_MAJOR ((int)((MCC_VERSION) / 1000000))
#define MCC_VERSION_MINOR ((int)((MCC_VERSION) % 1000000))

/* GCC version mcc emulates: reported by -dumpversion (so build systems that
 * parse it get a sane, gcc-compatible version, not the date-based MCC_VERSION)
 * and by the __GNUC__/__GNUC_MINOR__/__GNUC_PATCHLEVEL__ predefines. */
#define MCC_GNUC_MAJOR 7
#define MCC_GNUC_MINOR 5
#define MCC_GNUC_PATCHLEVEL 0

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef __MCC__
#undef __attribute__
#endif
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <setjmp.h>
#include <time.h>

#include "mcchost.h"

#if !defined(MCC_TARGET_I386) && !defined(MCC_TARGET_ARM) && \
		!defined(MCC_TARGET_ARM64) &&                            \
		!defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_RISCV64)
#if defined __x86_64__
#define MCC_TARGET_X86_64
#elif defined __arm__
#define MCC_TARGET_ARM
#define MCC_ARM_EABI
#define MCC_ARM_VFP
#define MCC_ARM_HARDFLOAT
#elif defined __aarch64__
#define MCC_TARGET_ARM64
#elif defined __riscv
#define MCC_TARGET_RISCV64
#else
#define MCC_TARGET_I386
#endif
#if MCC_HOST_WIN32
#define MCC_TARGET_PE 1
#endif
#if MCC_HOST_DARWIN
#define MCC_TARGET_MACHO 1
#endif
#endif

#if (defined(MCC_TARGET_I386) + defined(MCC_TARGET_X86_64) + \
		 defined(MCC_TARGET_ARM) + defined(MCC_TARGET_ARM64) +   \
		 defined(MCC_TARGET_RISCV64)) != 1
#error "exactly one MCC_TARGET_* backend CPU (I386/X86_64/ARM/ARM64/RISCV64) must be defined"
#endif

#if MCC_HOST_WIN32 == defined MCC_TARGET_PE && MCC_HOST_DARWIN == defined MCC_TARGET_MACHO
#if defined __i386__ && defined MCC_TARGET_I386
#define MCC_TARGET_IS_HOST
#elif defined __x86_64__ && defined MCC_TARGET_X86_64
#define MCC_TARGET_IS_HOST
#elif defined __arm__ && defined MCC_TARGET_ARM
#define MCC_TARGET_IS_HOST
#elif defined __aarch64__ && defined MCC_TARGET_ARM64
#define MCC_TARGET_IS_HOST
#elif defined __riscv && defined __LP64__ && defined MCC_TARGET_RISCV64
#define MCC_TARGET_IS_HOST
#endif
#endif


enum {
	MCC_DIAG_RT_OFF,
	MCC_DIAG_RT_BACKTRACE,
	MCC_DIAG_RT_BOUNDS
};

#if defined MCC_TARGET_IS_HOST
ST_FUNC void host_fault_install(host_fault_fn fn);
ST_FUNC int host_fault_regs(void *osctx, HostFaultRegs *r);
ST_FUNC void host_fault_unblock(unsigned detail);
#endif

#ifndef MCC_CONFIG_MACHO_CHAINED_FIXUPS
#define MCC_CONFIG_MACHO_CHAINED_FIXUPS 1
#endif

#if defined MCC_TARGETOS_OpenBSD || defined MCC_TARGETOS_FreeBSD || defined MCC_TARGETOS_NetBSD || defined MCC_TARGETOS_FreeBSD_kernel
#define MCC_TARGETOS_BSD 1
#elif !(defined MCC_TARGET_PE || defined MCC_TARGET_MACHO)
#define MCC_TARGETOS_Linux 1
#endif

#if !(defined MCC_TARGET_PE || defined MCC_TARGET_MACHO)
#define MCC_TARGET_UNIX 1
#endif

#if defined MCC_TARGET_PE || (defined MCC_TARGET_MACHO && defined MCC_TARGET_ARM64)
#define MCC_USING_DOUBLE_FOR_LDOUBLE 1
#endif

#include "mccdefaults.h"

#include "libmcc.h"
#include "elf.h"
#include "stab.h"
#include "dwarf.h"
#include "mcccst.h"
#include "mccast.h"

#ifdef MCC_TARGET_I386
#include "i386-gen.h"
#include "i386-link.h"
#elif defined MCC_TARGET_X86_64
#include "x86_64-gen.h"
#include "x86_64-link.h"
#elif defined MCC_TARGET_ARM
#include "arm-gen.h"
#include "arm-link.h"
#include "arm-asm.h"
#elif defined MCC_TARGET_ARM64
#include "arm64-gen.h"
#include "arm64-link.h"
#include "arm64-asm.h"
#elif defined(MCC_TARGET_RISCV64)
#include "riscv64-gen.h"
#include "riscv64-link.h"
#include "riscv64-asm.h"
#else
#error unknown target
#endif

#ifndef MCC_MAX_VEC_ALIGN
#define MCC_MAX_VEC_ALIGN MCC_MAX_ALIGN
#endif

#if MCC_PTR_SIZE == 8
#define ELFCLASSW ELFCLASS64
#define ElfW(type) Elf##64##_##type
#define ELFW(type) ELF##64##_##type
#define ElfW_Rel ElfW(Rela)
#define SHT_RELX SHT_RELA
#define REL_SECTION_FMT ".rela%s"
#define ELFW_R_ADDEND(rel) ((rel)->r_addend)
#define ELFW_SET_R_ADDEND(rel, val) ((rel)->r_addend = (val))
#else
#define ELFCLASSW ELFCLASS32
#define ElfW(type) Elf##32##_##type
#define ELFW(type) ELF##32##_##type
#define ElfW_Rel ElfW(Rel)
#define SHT_RELX SHT_REL
#define REL_SECTION_FMT ".rel%s"
#define ELFW_R_ADDEND(rel) ((void)(rel), 0)
#define ELFW_SET_R_ADDEND(rel, val) ((void)(rel), (void)(val))
#endif
#define addr_t ElfW(Addr)
#define ElfSym ElfW(Sym)

#if MCC_PTR_SIZE == 8 && !defined MCC_TARGET_PE
#define LONG_SIZE 8
#else
#define LONG_SIZE 4
#endif

#define INCLUDE_STACK_SIZE 32
#define IFDEF_STACK_SIZE 64
#define VSTACK_SIZE 512
#define STRING_MAX_SIZE 1024
#define TOKSTR_MAX_SIZE 256
#define PACK_STACK_SIZE 8

#define MCC_WIDE256_BITS 256
#define MCC_WIDE256_LIMBS 4
#define MCC_WIDE256_SIZE 32
#define MCC_WIDE512_BITS 512
#define MCC_WIDE512_LIMBS 8
#define MCC_WIDE512_SIZE 64

#define STDC_DEFAULT 0
#define STDC_ON 1
#define STDC_OFF 2

#define TOK_HASH_SIZE 65536
#define TOK_ALLOC_INCR 512
#define TOK_MAX_SIZE 8

typedef struct TokenSym {
	struct TokenSym *hash_next;
	struct Sym *sym_define;
	struct Sym *sym_label;
	struct Sym *sym_struct;
	struct Sym *sym_identifier;
	int tok;
	int len;
	char str[1];
} TokenSym;

#ifdef MCC_TARGET_PE
typedef unsigned short nwchar_t;
#else
typedef int nwchar_t;
#endif

typedef struct CString {
	int size;
	int size_allocated;
	char *data;
} CString;

typedef struct CType {
	int t;
	unsigned char bp;
	unsigned short bs;	/* bit-field size 0..64, OR _BitInt width up to 512
						 * (bs==0 is the sentinel for _BitInt(256); widths 257..512
						 * are stored directly, which is why this is not char).
						 * For a scalar float base (VT_FLOAT/DOUBLE/LDOUBLE) that is
						 * never a bitfield/_BitInt, bs instead holds an FSPELL_*
						 * spelling code: _Float32/_Float64/_Float32x/_Float64x are
						 * distinct types for _Generic/type-compat but alias
						 * float/double/long double in arithmetic/ABI (T-lin-10463). */
	struct Sym *ref;
} CType;

/* Float-spelling distinctness codes carried in CType.bs on a float base type.
 * 0 = canonical (float/double/long double, and _Float128 which gcc treats as
 * == __float128).  Only these four spellings are distinct-but-aliasing. */
#define FSPELL_NONE 0
#define FSPELL_F32  1
#define FSPELL_F64  2
#define FSPELL_F32X 3
#define FSPELL_F64X 4

#define LDOUBLE_WORDS ((sizeof(long double) + 3) / 4)

typedef union CValue {
	long double ld;
	double d;
	float f;
	uint64_t i;

	struct
	{
		uint64_t lo, hi, w2, w3, w4, w5, w6, w7;
	} q;

	struct
	{
		char *data;
		int size;
	} str;

	int tab[LDOUBLE_WORDS];
} CValue;

typedef struct SValue {
	CType type;
	unsigned int r;
	unsigned int r2;

	union {
		struct
		{
			int jtrue, jfalse;
		};

		CValue c;
	};

	union {
		struct
		{
			unsigned short cmp_op, cmp_r;
		};

		struct Sym *sym;
	};
} SValue;

struct SymAttr {
	unsigned int
			aligned : 5,
			type_aligned : 1,
			packed : 1,
			weak : 1,
			visibility : 2,
			visibility_set : 1,
			dllexport : 1,
			nodecorate : 1,
			dllimport : 1,
			addrtaken : 1,
			nodebug : 1,
			transp_union : 1,
			is_complex : 1,
			is_vector : 1,
			is_wideint : 1,
			is_bitint : 1,
			is_wideint512 : 1,
			is_bitint512 : 1,
			tentative_array : 1,
			tentative_incomplete : 1,
			deprecated : 1,
			unavailable : 1,
			is_register : 1,
			used : 1,
			retain : 1,
			unused : 1,
			inited : 1,
			has_vla_member : 1,
			gnu_inline_body : 1,
			full_bitfield : 1,
			ms_struct : 1,
			gcc_struct : 1,
			reverse_so : 1,
			warn_unused_result : 1,
			sentinel_attr : 1,
			nonnull_all : 1,
			old_promote : 1,
			naked : 1,
			nodiscard : 1,
			nonnull_mask : 16;
};

struct FuncAttr {
	unsigned
			func_call : 3,
			func_type : 2,
			func_noreturn : 1,
			func_ctor : 1,
			func_dtor : 1,
			func_args : 8,
			func_alwinl : 1,
			func_noinl : 1,
			func_gnuinl : 1,
			func_star_param : 1,
			func_fmt_kind : 2,
			func_fmt_arg : 4,
			func_fmt_first : 5,
			func_empty_def : 1;   /* T-mac-30094: this FUNC_OLD came from an
			                       * empty-`()` DEFINITION (`int f(){}`), which
			                       * per C 6.7.6.3p14 specifies ZERO parameters
			                       * (unlike an empty-`()` DECLARATION, which is
			                       * unspecified/K&R) -- used by
			                       * is_compatible_func to conflict a 0-param def
			                       * against a prototype that has parameters. */
};

typedef struct Sym {
	int v;
	unsigned short r;
	struct SymAttr a;

	union {
		struct
		{
			int c;

			union {
				int sym_scope;
				int jnext;
				int jind;
				struct FuncAttr f;
				int auxtype;
			};
		};

		long long enum_val;
		int *d;
		struct Sym *cleanup_func;
	};

	CType type;

	union {
		struct Sym *next;
		int *e;
		int asm_label;
		struct Sym *cleanupstate;
		int *vla_array_str;
	};

	int vla_inner_id;
	int vla_min_goto_gpp;
	int vla_dyn_slot;
	int decl_line;
	struct Sym *prev;

	union {
		struct Sym *prev_tok;
		struct Sym *cleanup_sym;
		struct Sym *cleanup_label;
	};
} Sym;

typedef struct Section {
	unsigned long data_offset;
	unsigned char *data;
	unsigned long data_allocated;
	MCCState *s1;
	int sh_name;
	int sh_num;
	int sh_type;
	int sh_flags;
	int sh_info;
	int sh_addralign;
	int sh_entsize;
	unsigned long sh_size;
	addr_t sh_addr;
	unsigned long sh_offset;
	int nb_hashed_syms;
	int asm_base;
	struct Section *link;
	struct Section *reloc;
	struct Section *hash;
	struct Section *prev;
	char name[1];
} Section;

typedef struct DLLReference {
	int level;
	void *handle;
	unsigned char found, index;
	char name[1];
} DLLReference;

#define SYM_STRUCT 0x40000000
#define SYM_FIELD 0x20000000
#define SYM_FIRST_ANOM 0x10000000

#define FUNC_NEW 1
#define FUNC_OLD 2
#define FUNC_ELLIPSIS 3

#define FUNC_CDECL 0
#define FUNC_STDCALL 1
#define FUNC_FASTCALL1 2
#define FUNC_FASTCALL2 3
#define FUNC_FASTCALL3 4
#define FUNC_FASTCALLW 5
#define FUNC_THISCALL 6

#define MACRO_OBJ 0
#define MACRO_FUNC 1
#define MACRO_JOIN 2

#define LABEL_DEFINED 0
#define LABEL_FORWARD 1
#define LABEL_DECLARED 2
#define LABEL_GONE 3

#define TYPE_ABSTRACT 1
#define TYPE_DIRECT 2
#define TYPE_PARAM 4
#define TYPE_NEST 8

#define IO_BUF_SIZE 8192

typedef struct BufferedFile {
	uint8_t *buf_ptr;
	uint8_t *buf_end;
	int fd;
	struct BufferedFile *prev;
	int64_t line_num;
	int64_t line_ref;
	unsigned long cst_base;
	int ifndef_macro;
	int ifndef_macro_saved;
	int *ifdef_stack_ptr;
	int include_next_index;
	int system_header;
	int prev_tok_flags;
	char filename[1024];
	char *true_filename;
	unsigned char unget[4];
	unsigned char buffer[1];
} BufferedFile;

#define CH_EOB '\\'
#define CH_EOF (-1)

typedef struct TokenString {
	int *str;
	int len;
	int need_spc;
	int allocated_len;
	int64_t last_line_num;
	int64_t save_line_num;
	struct TokenString *prev;
	const int *prev_ptr;
	char alloc;
} TokenString;

typedef struct AttributeDef {
	struct SymAttr a;
	struct FuncAttr f;
	struct Section *section;
	Sym *cleanup_func;
	int alias_target;
	int asm_label;
	char attr_mode;
	int attr_vector_size;
	char storage_class;
	char implicit_int;
	char had_attr;
	char attr_scoped;
	int attr_tok;
	char auto_type;
	char auto_seen;
	char ext_seen;
	int ctor_prio;
	int poison_msg;
	char poison_is_error;
} AttributeDef;

typedef struct InlineFunc {
	TokenString *func_str;
	Sym *sym;
	char filename[1];
} InlineFunc;

typedef struct AliasFixup {
	int alias_v;
	int target_v;
} AliasFixup;

typedef struct CachedInclude {
	int ifndef_macro;
	int once;
	int hash_next;
	unsigned long long dev, ino;
	char filename[1];
} CachedInclude;

#define CACHED_INCLUDES_HASH_SIZE 32

typedef struct MCCAssertion {
	int pred_tok;
	char **answers;
	int nb_answers;
} MCCAssertion;

typedef struct ExprValue {
	uint64_t v;
	Sym *sym;
	int pcrel;
} ExprValue;

#define MAX_ASM_OPERANDS 30

typedef struct ASMOperand {
	int id;
	char constraint[16];
	char asm_str[16];
	SValue *vt;
	int ref_index;
	int input_index;
	int priority;
	int reg;
	int is_llong;
	int is_memory;
	int is_rw;
	int is_label;
} ASMOperand;

struct sym_attr {
	unsigned got_offset;
	unsigned plt_offset;
	int plt_sym;
	int dyn_index;
	unsigned char linker_sym : 1;
#ifdef MCC_TARGET_ARM
	unsigned char plt_thumb_stub : 1;
#endif
};

#ifdef MCC_TARGET_PE
#define PE_IMAGE_FILE_RELOCS_STRIPPED 0x0001
#define PE_IMAGE_FILE_LARGE_ADDRESS_AWARE 0x0020
#define PE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA 0x0020
#define PE_DLLCHARACTERISTICS_DYNAMIC_BASE 0x0040
#define PE_DLLCHARACTERISTICS_NX_COMPAT 0x0100
#define PE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE 0x8000
#endif

struct TinyAlloc;

#define VLA_TRACK_MAX 512
#define MAX_TEMP_LOCAL_VARIABLE_NUMBER 8
#define SEQP_MAX 64
#define MCC_ASAN_REDZONE 16

struct scope {
	struct scope *prev;

	struct
	{
		int loc, locorig, num;
	} vla;

	int vla_diag;

	struct
	{
		Sym *s;
		int n;
	} cl;

	int *bsym, *csym;
	Sym *lstk, *llstk;
	unsigned char stdc_fp_contract, stdc_fenv_access, stdc_cx_limited;
};

struct switch_t {
	struct case_t {
		int64_t v1, v2;
		uint64_t v1hi, v2hi;
		int ind, line;
	} **p;

	int n;
	int def_sym;
	int nocode_wanted;
	int *bsym;
	struct scope *scope;
	struct switch_t *prev;
	SValue sv;
	int vla_gpp;
	int ft_saw;
	int ft_last_ind;
};

struct temp_local_variable {
	int location;
	short size;
	short align;
};

struct seqp_event {
	Sym *obj;
	unsigned long long off;
	unsigned char kind;
};

struct asm_cfi_state {
	int active;
	Section *sec;
	unsigned long start;
	unsigned long last;
	int nfde;
	int have_factors;
	unsigned long code_align;
	long long data_align;
	int len;
	int cap;
	unsigned char *buf;
};

#define MCC_SANR_OVERFLOW 0x1u
#define MCC_SANR_DIVREM   0x2u
#define MCC_SANR_SHIFT    0x4u
#define MCC_SANR_NULLPTR  0x8u
#define MCC_SANR_ALL \
	(MCC_SANR_OVERFLOW | MCC_SANR_DIVREM | MCC_SANR_SHIFT | MCC_SANR_NULLPTR)

#include "mccopt.h"

#define MCC_OPT_UNSET 0xff

struct MCCState {
	unsigned char verbose;
	unsigned char nostdinc;
	unsigned char nostdlib;
	unsigned char nostdlib_paths;
	unsigned char nocommon;
	unsigned char static_link;
	unsigned char rdynamic;
	unsigned char symbolic;
	unsigned char znodelete;
	unsigned char filetype;
	unsigned char optimize;
	unsigned char optimize_level;
	unsigned char optimize_size;
	unsigned char optflag[MCC_OPT_COUNT];
	unsigned char embed_jit;
	signed char jit;
	unsigned char clear_cache;
	unsigned jit_max_duration;
	unsigned jit_threads;
	char *jit_functions;
	unsigned char jit_always_gpu;
	unsigned char jit_conservative;
	signed char jit_cpu_budget;
	signed char jit_gpu_budget;
	signed char jit_gpu_devices;
	unsigned optimize_search_ticks;
	unsigned char optimize_search_ticks_set;
	unsigned char optimize_search_all;
	unsigned stats;
	signed char pie;
	unsigned char pic;
	unsigned char option_pthread;
	unsigned char enable_new_dtags;
	unsigned int cversion;

	unsigned char char_is_unsigned;
	unsigned char leading_underscore;
	unsigned char ms_extensions;
	unsigned char dollars_in_identifiers;
	unsigned char ms_bitfields;
	unsigned char reverse_funcargs;
	unsigned char macro_eval;
	unsigned char gnu89_inline;
	unsigned char c99_inline_body;
	unsigned char std_strict_ansi;
	unsigned char permissive;
	unsigned char unwind_tables;
	unsigned char short_enums;
	unsigned char nobuiltin;
	unsigned char noasm;
	unsigned char omit_frame_pointer;
	unsigned char function_sections;
	unsigned char data_sections;
	unsigned char print_isa;
	uint32_t isa_mask;
	const char *isa_level;
	unsigned char wrapv;
	unsigned char trigraphs;
	unsigned char freestanding;
	unsigned char syntax_only;
	unsigned char diag_no_caret;
	unsigned char diag_color;
	unsigned char visibility;
	unsigned char stack_protector;
	unsigned char do_sanitize_undefined;
	unsigned do_sanitize_recover;
	unsigned char do_sanitize_address;
	unsigned char do_asan_shadow;
	unsigned char do_strip;
	unsigned char lsp;

	unsigned char warn_none;
	unsigned char warn_all;
	unsigned char warn_error;
	unsigned char warn_main;
	unsigned char warn_write_strings;
	unsigned char warn_unsupported;
	unsigned char warn_unsupported_option;
	unsigned char warn_attributes;
	unsigned char warn_implicit_function_declaration;
	unsigned char warn_discarded_qualifiers;
	unsigned char warn_sequence_point;
	unsigned char warn_format;
	unsigned char warn_vla;
	unsigned char warn_undef;
	unsigned char warn_unknown_pragmas;
	unsigned char warn_implicit_int;
	unsigned char warn_incompatible_pointer_types;
	unsigned char warn_int_conversion;
	unsigned char warn_excess_initializers;
	unsigned char warn_sign_compare;
	unsigned char warn_type_limits;
	unsigned char warn_bool_compare;
	unsigned char warn_char_subscripts;
	unsigned char warn_enum_compare;
	unsigned char warn_address;
	unsigned char warn_pointer_sign;
	unsigned char warn_parentheses;
	unsigned char warn_switch;
	unsigned char warn_implicit_fallthrough;
	unsigned char warn_unused_variable;
	unsigned char warn_unused_parameter;
	unsigned char warn_unused_function;
	unsigned char warn_unused_label;
	unsigned char warn_cpp;
	unsigned char warn_fatal_errors;
	unsigned char warn_shadow;
	unsigned char warn_unused_value;
	unsigned char warn_uninitialized;
	unsigned char warn_varargs;
	unsigned char warn_strict_prototypes;
	unsigned char warn_return_type;
	unsigned char warn_shift_count_negative;
	unsigned char warn_shift_count_overflow;
	unsigned char warn_overflow;
	unsigned char warn_div_by_zero;
	unsigned char warn_old_style_definition;
	unsigned char warn_undefined_internal;
	unsigned char warn_return_local_addr;
	unsigned char warn_deprecated_declarations;
	unsigned char warn_unused_result;
	unsigned char warn_nonnull;
	unsigned char warn_sentinel;
	unsigned char warn_address_of_packed_member;
	unsigned char warn_extra_ptr_zero_cmp;
	int max_errors;
	unsigned char warn_pedantic;
	unsigned char pedantic_errors;
#define WARN_ON 1
	unsigned char warn_num;

	unsigned char option_r;
	unsigned char do_bench;
	unsigned char just_deps;
	unsigned char gen_deps;
	unsigned char include_sys_deps;
	unsigned char gen_phony_deps;
	unsigned char gen_deps_missing_ok;

	unsigned char do_debug;
	unsigned char dwarf;
	unsigned char cv_debug;
	unsigned char do_backtrace;
	unsigned char do_bounds_check;
	unsigned char test_coverage;
	unsigned char loop_census;
	unsigned char depth_census;

	unsigned char gnu_ext;
	unsigned char mcc_ext;

	unsigned char dflag;
	unsigned char Pflag;
	unsigned char keep_comments;

#ifdef MCC_TARGET_X86_64
	unsigned char nosse;
#endif
#ifdef MCC_TARGET_ARM
	unsigned char float_abi;
#endif

	unsigned char has_text_addr;
	addr_t text_addr;
	unsigned section_align;
#ifdef MCC_TARGET_I386
	int seg_size;
#endif

	char *mcc_lib_path;
	char *soname;
	char *sysroot;
	char *rpath;
	char *elfint;
	char *elf_entryname;
	char *init_symbol;
	char *fini_symbol;
	char *mapfile;
	char *version_script;
	char *print_query;

	int output_type;
	int output_format;
	int output_format_explicit;
	int run_test;

	DLLReference **loaded_dlls;
	int nb_loaded_dlls;

	char **include_paths;
	int nb_include_paths;

	char **sysinclude_paths;
	int nb_sysinclude_paths;

	char **iquote_paths;
	int nb_iquote_paths;

	char **afterinc_paths;
	int nb_afterinc_paths;

	char **library_paths;
	int nb_library_paths;

	char **framework_paths;
	int nb_framework_paths;

	char **crt_paths;
	int nb_crt_paths;

	CString cmdline_defs;
	CString cmdline_incl;
	CString cmdline_imacros;

	void *error_opaque;
	void (*error_func)(void *opaque, const char *msg);
	int error_set_jmp_enabled;
	jmp_buf error_jmp_buf;
	int nb_errors;

	FILE *ppfp;

	char **target_deps;
	int nb_target_deps;

	BufferedFile *include_stack[INCLUDE_STACK_SIZE];
	BufferedFile **include_stack_ptr;

	int ifdef_stack[IFDEF_STACK_SIZE];
	int *ifdef_stack_ptr;

	int cached_includes_hash[CACHED_INCLUDES_HASH_SIZE];
	CachedInclude **cached_includes;
	int nb_cached_includes;

	char **pragma_weak_syms;
	int nb_pragma_weak_syms;

	char **embed_paths;
	int nb_embed_paths;

	MCCAssertion **assertions;
	int nb_assertions;

	int pack_stack[PACK_STACK_SIZE];
	int *pack_stack_ptr;
	char **pragma_libs;
	int nb_pragma_libs;

	unsigned char stdc_fp_contract;
	unsigned char stdc_fenv_access;
	unsigned char stdc_cx_limited;

	unsigned char cx_limited_range;
	unsigned char fold_math;
	unsigned char no_math_errno;
	unsigned char fast_math;

	struct InlineFunc **inline_fns;
	int nb_inline_fns;
	struct AliasFixup **alias_fixups;
	int nb_alias_fixups;

	void **ctor_init_prio;
	int nb_ctor_init_prio;
	void **ctor_fini_prio;
	int nb_ctor_fini_prio;

	Section **sections;
	int nb_sections;

	Section **priv_sections;
	int nb_priv_sections;

	Section *text_section, *data_section, *rodata_section, *bss_section;
	Section *tdata_section, *tbss_section;
	Section *common_section;
	Section *asan_lstack_section;
	Section *cur_text_section;
	Section *bounds_section;
	Section *lbounds_section;
	union {
		Section *symtab_section, *symtab;
	};

	Section *dynsymtab_section;
	Section *dynsym;
	Section *got, *plt;
	Section *ast_scratch_sec;
	Section *eh_frame_section;
	Section *eh_frame_hdr_section;
	unsigned long eh_start;
	Section *stab_section;
	Section *dwarf_info_section;
	Section *dwarf_abbrev_section;
	Section *dwarf_line_section;
	Section *dwarf_aranges_section;
	Section *dwarf_str_section;
	Section *dwarf_line_str_section;
	int dwlo, dwhi;
	Section *tcov_section;
	struct _mccdbg *dState;

	struct sym_attr *sym_attrs;
	int nb_sym_attrs;
	ElfW_Rel *qrel;
#define qrel s1->qrel

#ifdef MCC_TARGET_RISCV64
	struct pcrel_hi {
		addr_t addr, val;
	} **pcrel_hi_entries;
	int nb_pcrel_hi_entries;
#endif

#ifdef MCC_TARGET_PE
	char **pe_imp_alias;
	int nb_pe_imp_alias;
	int pe_subsystem;
	unsigned pe_characteristics;
	unsigned pe_dll_characteristics;
	unsigned pe_dll_characteristics_clear;
	unsigned pe_file_align;
	unsigned pe_stack_size;
	addr_t pe_imagebase;
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
	Section *uw_pdata;
	int uw_sym;
	int uw_xsym;
	unsigned uw_offs;
#endif
#endif

	char macho_bundle;

#if defined MCC_TARGET_MACHO
	char *install_name;
	uint32_t compatibility_version;
	uint32_t current_version;
	uint32_t macos_version_min;
#endif

#if MCC_TARGET_UNIX
	int nb_sym_versions;
	struct sym_version *sym_versions;
	int nb_sym_to_version;
	int *sym_to_version;
	int dt_verneednum;
	Section *versym_section;
	Section *verneed_section;
#endif

#ifdef MCC_TARGET_IS_HOST
	const char *run_main;
	void *run_ptr;
	unsigned run_size;
	const char *run_stdin;
	void *run_function_table;
	struct MCCState *next;
	struct rt_context *rc;
	void *run_lj, *run_jb;
	MCCBtFunc *bt_func;
	void *bt_data;
	Section *run_tls_desc;
	void *run_tls_recs;
	int run_tls_nrecs;
#endif

	addr_t run_tls_slab_tpoff;
	int run_tls_active;

	int rt_num_callers;

	int total_idents;
	int total_lines;
	int total_toks;
	int total_funcs;
	unsigned int total_bytes;
	unsigned int total_output[4];

	unsigned char *ld_p;

	const char *current_filename;

	struct filespec **files;
	int nb_files;
	int nb_libraries;
	int nb_alacarte_pulls;
	char link_in_group;
	char *outfile;
	char *deps_outfile;
	char *dep_target;
	int argc;
	char **argv;
	char **link_argv;
	int link_argc, link_optind;

	int gen_sizeof_parsed_type;
	int gen_sizeof_parsed_align;
	int gen_member_align;
	int gen_complex_re_tok, gen_complex_im_tok;
	CType gen_complex_type_cache[32];
	int gen_complex_type_cache_n;
	CType gen_vector_type_cache[64];
	int gen_vector_type_cache_n;
	CType gen_wideint_type_cache[4];
	int gen_wideint_type_cache_n;
	int gen_wideint_limb_tok[MCC_WIDE512_LIMBS];
	CType gen_bitint128_type_cache[6];
	int gen_bitint128_limb_tok[MCC_WIDE512_LIMBS];
	Sym *gen_complex_call_ftype[4];
	Sym *gen_complex_idiv_ftype[2];
	unsigned char gen_prec[256];

	Sym *sym_free_first;
	void **sym_pools;
	int nb_sym_pools;
	void **vla_array_strs;
	int nb_vla_array_strs;
	Sym *all_cleanups, *pending_gotos;
	int local_scope;
	SValue gen_vstack[1 + VSTACK_SIZE];
	int func_old;
	int cur_func_noreturn;
	int cur_func_last_param;
	int expr_was_assign;
	int expr_has_effect;
	int cur_func_inline_extern;
	int ice_float_op;
	int ice_nonconst;
	CString initstr;
	struct switch_t *cur_switch;
	int atomic_lowering;
	int in_for_init;
	int vla_seq;
	int vla_open_birth[VLA_TRACK_MAX];
	int nb_vla_open;
	int vla_track_ovf;
	struct temp_local_variable arr_temp_local_vars[MAX_TEMP_LOCAL_VARIABLE_NUMBER];
	int nb_temp_local_vars;
	struct scope *cur_scope, *loop_scope, *root_scope;
	struct seqp_event seqp_ev[SEQP_MAX];
	int nb_seqp;
	int seqp_overflow;

	TokenSym *tok_ts;
	TokenSym *hash_ident[TOK_HASH_SIZE];
	char token_buf[STRING_MAX_SIZE + 1];
	CString cstr_buf;
	TokenString tokstr_buf;
	TokenString unget_buf;
	unsigned char isidnum_table[256 - CH_EOF];
	int pp_debug_tok, pp_debug_symv;
	int pp_counter;
	int pp_debug_indent;
	struct TinyAlloc *toksym_alloc;
	struct TinyAlloc *tokstr_alloc;
	TokenString *macro_stack;

	char **disasm_uniq;

	Section *last_text_section;
	int asmgoto_n;
	struct asm_cfi_state asm_cfi_st;

	unsigned long cg_func_sub_sp_offset;
	int cg_func_ret_sub;
	int cg_func_stack_chk_loc;
	addr_t cg_func_bound_offset;
	unsigned long cg_func_bound_ind;
	addr_t cg_func_asan_offset;
	unsigned long cg_func_asan_ind;
	int cg_func_scratch, cg_func_alloca;
	int cg_arm_float_abi;
	int cg_last_itod_magic;
	int cg_leaffunc;
	CType cg_float_type, cg_double_type, cg_func_float_type, cg_func_double_type;
	unsigned long cg_arm64_func_va_list_stack;
	int cg_arm64_func_va_list_gr_offs, cg_arm64_func_va_list_vr_offs;
	int cg_arm64_func_sub_sp_offset;
	unsigned cg_arm64_func_start_offset;
	int cg_num_va_regs, cg_func_va_list_ofs;
};

static inline int stdc_cx_limited(MCCState *s1) {
	return s1->stdc_cx_limited == STDC_ON;
}

static inline int stdc_fenv_access(MCCState *s1) {
	return s1->stdc_fenv_access == STDC_ON;
}

static inline int stdc_fp_contract(MCCState *s1) {
	return s1->stdc_fp_contract == STDC_ON;
}

static inline int gnu89_inline_semantics(MCCState *s1) {
	return s1->gnu89_inline || s1->cversion < 199901;
}

static inline int c99_stmt_scopes(MCCState *s1) {
	return s1->cversion >= 199901;
}

/* Fleet C23-strictness policy (2026-08-20, human-decided via QUESTIONS.md): the
 * DISRUPTIVE C23 breaking changes -- empty `()` == `(void)` (T-mac-30142),
 * and any future accept->reject C23 change -- are honored ONLY under an
 * explicit strict-ISO C23 std (`-std=c23`/`iso9899:2024`), NOT the default
 * gnu23 mode. This keeps existing K&R/C17-valid code and mcc's own runtime
 * building by DEFAULT (so the o0-baseline stays byte-neutral), while `-std=c23`
 * gets the conforming behavior. NOTE: C23 FEATURES (typeof_unqual, etc.) stay
 * on by default -- they gate on cversion alone; only breaking changes use this. */
static inline int c23_strict_breaking(MCCState *s1) {
	return s1->std_strict_ansi && s1->cversion >= 202311;
}

struct filespec {
	char type;
	char group;
	char name[1];
};

#define VT_VALMASK 0x007f
#define VT_CONST 0x0030
#define VT_LLOCAL 0x0031
#define VT_LOCAL 0x0032
#define VT_CMP 0x0033
#define VT_JMP 0x0034
#define VT_JMPI 0x0035
#define VT_REGDISP 0x0080
#define VT_LVAL 0x0100
#define VT_SYM 0x0200
#define VT_MUSTCAST 0x0C00
#define VT_NONCONST 0x1000
#define VT_NONLVAL 0x2000
#define VT_MUSTBOUND 0x4000
#define VT_BOUNDED 0x8000
/* SValue .r flag (not a type bit): this lvalue is a scalar member of a reverse
 * scalar_storage_order struct, so gv()/vstore() byte-swap it (T-lin-10010).
 * Lives above the 16-bit range, which is why SValue.r was widened to 32-bit. */
#define VT_REVSO 0x10000
#define VT_BTYPE 0x001f
#define VT_VOID 0
#define VT_BYTE 1
#define VT_SHORT 2
#define VT_INT 3
#define VT_LLONG 4
#define VT_PTR 5
#define VT_FUNC 6
#define VT_STRUCT 7
#define VT_FLOAT 8
#define VT_DOUBLE 9
#define VT_LDOUBLE 10
#define VT_BOOL 11
#define VT_INT128 12
#define VT_QLONG 13
#define VT_QFLOAT 14
#define VT_FLOAT16 15
#define VT_BF16 16
#define VT_FLOAT128 17
#define VT_DEC32 18		/* _Decimal32 (IEEE 754 decimal32, BID) — T-lin-10460 */
#define VT_DEC64 19		/* _Decimal64 */
#define VT_DEC128 20	/* _Decimal128 */

#define IS_HALF_BT(bt) ((bt) == VT_FLOAT16 || (bt) == VT_BF16)
#define IS_DECIMAL_BT(bt) ((bt) == VT_DEC32 || (bt) == VT_DEC64 || (bt) == VT_DEC128)

/* binary128 is wired where its soft-quad runtime and __*tf* helper tokens exist
   (T-lin-10007 slice 1). Other targets keep the honest __float128 refusal until
   their ABI is wired (slice 2). */
#if defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64 || \
	(defined MCC_TARGET_X86_64 && !defined MCC_TARGET_PE)
#define MCC_HAVE_FLOAT128 1
#endif

#if defined MCC_TARGET_X86_64 && !defined MCC_TARGET_PE
#define MCC_HAVE_INT128 1
#else
#define MCC_HAVE_INT128 0
#endif


#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
#define MCC_IR_HAVE_REGADDI 1
#endif


#define VT_UNSIGNED 0x0020
#define VT_DEFSIGN 0x0040
#define VT_ARRAY 0x0080
#define VT_BITFIELD 0x0100
#define VT_CONSTANT 0x0200
#define VT_VOLATILE 0x0400
#define VT_VLA 0x0800
#define VT_LONG 0x1000

#if MCC_PTR_SIZE == 4
#define VT_SIZE_T (VT_INT | VT_UNSIGNED)
#define VT_PTRDIFF_T VT_INT
#elif LONG_SIZE == 4
#define VT_SIZE_T (VT_LLONG | VT_UNSIGNED)
#define VT_PTRDIFF_T VT_LLONG
#else
#define VT_SIZE_T (VT_LONG | VT_LLONG | VT_UNSIGNED)
#define VT_PTRDIFF_T (VT_LONG | VT_LLONG)
#endif

#define VT_EXTERN 0x00002000
#define VT_STATIC 0x00004000
#define VT_TYPEDEF 0x00008000
#define VT_INLINE 0x00010000
#define VT_TLS 0x00020000

#define VT_STRUCT_SHIFT 21
#define VT_STRUCT_MASK (((1U << 11) - 1) << VT_STRUCT_SHIFT | VT_BITFIELD)

#define VT_UNION (1 << VT_STRUCT_SHIFT | VT_STRUCT)
#define VT_ENUM (2 << VT_STRUCT_SHIFT)
#define VT_ENUM_VAL (3 << VT_STRUCT_SHIFT)

#define IS_ENUM(t) ((t & VT_STRUCT_MASK) == VT_ENUM)
#define IS_ENUM_VAL(t) ((t & VT_STRUCT_MASK) == VT_ENUM_VAL)
#define IS_UNION(t) ((t & (VT_STRUCT_MASK | VT_BTYPE)) == VT_UNION)

#define VT_ATOMIC_BIT 0x00040000
#define VT_ATOMIC (VT_ATOMIC_BIT | VT_VOLATILE)
#define VT_QUALIFY (VT_CONSTANT | VT_VOLATILE | VT_ATOMIC_BIT)

#define VT_NULLPTR 0x00080000
#define VT_CONSTEXPR 0x00100000

#define IS_NULLPTR(t) (((t) & (VT_BTYPE | VT_NULLPTR)) == (VT_PTR | VT_NULLPTR))

#define VT_STORAGE (VT_EXTERN | VT_STATIC | VT_TYPEDEF | VT_INLINE | VT_TLS | VT_CONSTEXPR)
#define VT_TYPE (~(VT_STORAGE | VT_STRUCT_MASK))

#define VT_ASM (VT_VOID | 4 << VT_STRUCT_SHIFT)
#define VT_ASM_FUNC (VT_VOID | 5 << VT_STRUCT_SHIFT)
#define IS_ASM_SYM(sym) (((sym)->type.t & ((VT_BTYPE | VT_STRUCT_MASK) & ~(1 << VT_STRUCT_SHIFT))) == VT_ASM)
#define IS_ASM_FUNC(t) ((t & (VT_BTYPE | VT_STRUCT_MASK)) == VT_ASM_FUNC)

#define VT_BT_ARRAY (6 << VT_STRUCT_SHIFT)
#define IS_BT_ARRAY(t) ((t & VT_STRUCT_MASK) == VT_BT_ARRAY)

/* C23 _BitInt(N), slice 1 (N<=64).  A _BitInt is a storage integer (VT_BYTE/
 * SHORT/INT/LLONG, sized to hold N bits) whose value is reduced to N bits.  We
 * reuse the VT_BITFIELD precision-N machinery -- bp=0, bs=N drives the masked
 * load/store (gv/vstore) and the over-wide operand reduction in gen_op -- and
 * tag it with a free struct-tag value so a standalone _BitInt stays
 * distinguishable from a struct bit-field member (tag 0), which differs for
 * &-of, sizeof/_Alignof and type compatibility.  No SymAttr/ref growth. */
#define VT_BITINT (7 << VT_STRUCT_SHIFT)
#define IS_BITINT(t) ((t & VT_STRUCT_MASK) == (VT_BITINT | VT_BITFIELD))

#define BFVAL(M, N) ((unsigned)((M) & ~((M) << 1)) * (N))
#define BFGET(X, M) (((X) & (M)) / BFVAL(M, 1))
#define BFSET(X, M, N) ((X) = ((X) & ~(M)) | BFVAL(M, N))

#define TOK_LAND 0x90
#define TOK_LOR 0x91
#define TOK_ULT 0x92
#define TOK_UGE 0x93
#define TOK_EQ 0x94
#define TOK_NE 0x95
#define TOK_ULE 0x96
#define TOK_UGT 0x97
#define TOK_Nset 0x98
#define TOK_Nclear 0x99
#define TOK_LT 0x9c
#define TOK_GE 0x9d
#define TOK_LE 0x9e
#define TOK_GT 0x9f

#define TOK_ISCOND(t) (t >= TOK_LAND && t <= TOK_GT)

#define TOK_DEC 0x80
#define TOK_MID 0x81
#define TOK_INC 0x82
#define TOK_UDIV 0x83
#define TOK_UMOD 0x84
#define TOK_PDIV 0x85
#define TOK_UMULL 0x86
#define TOK_ADDC1 0x87
#define TOK_ADDC2 0x88
#define TOK_SUBC1 0x89
#define TOK_SUBC2 0x8a
#define TOK_SHL '<'
#define TOK_SAR '>'
#define TOK_SHR 0x8b
#define TOK_NEG TOK_MID

#define TOK_ARROW 0xa0
#define TOK_DOTS 0xa1
#define TOK_TWODOTS 0xa2
#define TOK_TWOSHARPS 0xa3
#define TOK_PLCHLDR 0xa4
#define TOK_PPJOIN (TOK_TWOSHARPS | SYM_FIELD)
#define TOK_SOTYPE 0xa7
#define TOK_DIG_LBRACK 0xa8
#define TOK_DIG_RBRACK 0xa9
#define TOK_DIG_LBRACE 0xaa
#define TOK_DIG_RBRACE 0xab
#define TOK_DIG_HASH 0xac
#define TOK_DIG_TWOSHARPS 0xad
#define TOK_IS_DIGRAPH(t) ((unsigned)((t) - TOK_DIG_LBRACK) <= (unsigned)(TOK_DIG_TWOSHARPS - TOK_DIG_LBRACK))

#define TOK_A_ADD 0xb0
#define TOK_A_SUB 0xb1
#define TOK_A_MUL 0xb2
#define TOK_A_DIV 0xb3
#define TOK_A_MOD 0xb4
#define TOK_A_AND 0xb5
#define TOK_A_OR 0xb6
#define TOK_A_XOR 0xb7
#define TOK_A_SHL 0xb8
#define TOK_A_SAR 0xb9

#define TOK_ASSIGN(t) (t >= TOK_A_ADD && t <= TOK_A_SAR)
#define TOK_ASSIGN_OP(t) ("+-*/%&|^<>"[t - TOK_A_ADD])

#define TOK_CCHAR 0xc0
#define TOK_LCHAR 0xc1
#define TOK_CINT 0xc2
#define TOK_CUINT 0xc3
#define TOK_CLLONG 0xc4
#define TOK_CULLONG 0xc5
#define TOK_CLONG 0xc6
#define TOK_CULONG 0xc7
#define TOK_STR 0xc8
#define TOK_LSTR 0xc9
#define TOK_CFLOAT 0xca
#define TOK_CDOUBLE 0xcb
#define TOK_CLDOUBLE 0xcc
#define TOK_PPNUM 0xcd
#define TOK_PPSTR 0xce
#define TOK_LINENUM 0xcf
#define TOK_U16CHAR 0xd0
#define TOK_U32CHAR 0xd1
#define TOK_U16STR 0xd2
#define TOK_U32STR 0xd3
#define TOK_U8STR 0xd4
#define TOK_CFLOAT16 0xd5
#define TOK_U8CHAR 0xd6
#define TOK_CINT256 0xd7
#define TOK_CUINT256 0xd8
#define TOK_CBITINT 0xd9
#define TOK_CUBITINT 0xda
#define TOK_CFLOAT128 0xdb

#define TOK_HAS_VALUE(t) (t >= TOK_CCHAR && t <= TOK_CFLOAT128)

#define TOK_EOF (-1)
#define TOK_LINEFEED 10

#define TOK_IDENT 256

enum mcc_token {
	TOK_LAST = TOK_IDENT - 1
#define DEF(id, str) , id
#include "mcctok.h"

#undef DEF
	, TOK_PREDEF_END
};

#define TOK_UIDENT TOK_DEFINE

ST_DATA struct MCCState *mcc_state;
ST_DATA int mccjit_error_quiet;
ST_DATA int mcc_leakcheck_quiet;

#include "mcclog.h"

ST_DATA void **stk_data;
ST_DATA int nb_stk_data;
ST_DATA int stk_data_floor;
ST_DATA int g_debug;

#define MCC_DBG_RELOC 0x001
#define MCC_DBG_INC 0x002
#define MCC_DBG_PP 0x004
#define MCC_DBG_STRUCT 0x008
#define MCC_DBG_TOK 0x010
#define MCC_DBG_PE 0x020
#define MCC_DBG_VER 0x040
#define MCC_DBG_ASM 0x080
#define MCC_DBG_SYM 0x100

ST_FUNC char *pstrcpy(char *buf, size_t buf_size, const char *s);
ST_FUNC char *pstrcat(char *buf, size_t buf_size, const char *s);
ST_FUNC char *pstrncpy(char *out, size_t buf_size, const char *s, size_t num);
PUB_FUNC char *mcc_basename(const char *name);
PUB_FUNC char *mcc_fileextension(const char *name);

PUB_FUNC void mcc_free(void *ptr);
PUB_FUNC void *mcc_malloc(unsigned long size);
PUB_FUNC void *mcc_mallocz(unsigned long size);
PUB_FUNC void *mcc_realloc(void *ptr, unsigned long size);
ST_FUNC unsigned long mcc_grow_capacity(unsigned long cur, unsigned long need,
																				unsigned long min_cap);
ST_FUNC int mcc_uleb128_size(unsigned long long value);
ST_FUNC int mcc_sleb128_size(long long value);
ST_FUNC void mcc_write_uleb128(Section *sec, unsigned long long value);
ST_FUNC void mcc_write_sleb128(Section *sec, long long value);
PUB_FUNC char *mcc_strdup(const char *str);

#if MCC_DIAG
#define mcc_free(ptr) mcc_free_debug(ptr)
#define mcc_malloc(size) mcc_malloc_debug(size, __FILE__, __LINE__)
#define mcc_mallocz(size) mcc_mallocz_debug(size, __FILE__, __LINE__)
#define mcc_realloc(ptr, size) mcc_realloc_debug(ptr, size, __FILE__, __LINE__)
#define mcc_strdup(str) mcc_strdup_debug(str, __FILE__, __LINE__)
PUB_FUNC void mcc_free_debug(void *ptr);
PUB_FUNC void *mcc_malloc_debug(unsigned long size, const char *file, int line);
PUB_FUNC void *mcc_mallocz_debug(unsigned long size, const char *file, int line);
PUB_FUNC void *mcc_realloc_debug(void *ptr, unsigned long size, const char *file, int line);
PUB_FUNC char *mcc_strdup_debug(const char *str, const char *file, int line);
#endif

ST_FUNC void libc_free(void *ptr);
#define free(p) use_mcc_free(p)
#define malloc(s) use_mcc_malloc(s)
#define realloc(p, s) use_mcc_realloc(p, s)
#undef strdup
#define strdup(s) use_mcc_strdup(s)
PUB_FUNC int _mcc_error_noabort(const char *fmt, ...) PRINTF_LIKE(1, 2);
PUB_FUNC NORETURN void _mcc_error(const char *fmt, ...) PRINTF_LIKE(1, 2);
PUB_FUNC void _mcc_warning(const char *fmt, ...) PRINTF_LIKE(1, 2);
#define mcc_internal_error(msg) \
	mcc_error("internal compiler error in %s:%d: %s", __FUNCTION__, __LINE__, msg)

ST_FUNC void dynarray_add(void *ptab, int *nb_ptr, void *data);
ST_FUNC void dynarray_reset(void *pp, int *n);
ST_INLN void cstr_ccat(CString *cstr, int ch);
ST_FUNC void cstr_cat(CString *cstr, const char *str, int len);
ST_FUNC void cstr_wccat(CString *cstr, int ch);
ST_FUNC void cstr_new(CString *cstr);
ST_FUNC void cstr_free(CString *cstr);
ST_FUNC int cstr_printf(CString *cs, const char *fmt, ...) PRINTF_LIKE(2, 3);
ST_FUNC int cstr_vprintf(CString *cstr, const char *fmt, va_list ap);
ST_FUNC void cstr_reset(CString *cstr);
ST_FUNC void mcc_open_bf(MCCState *s1, const char *filename, int initlen);
ST_FUNC int mcc_open(MCCState *s1, const char *filename);
ST_FUNC void mcc_close(void);

#define stk_push(p) dynarray_add(&stk_data, &nb_stk_data, p)
#define stk_pop() (--nb_stk_data)
#define cstr_new_s(cstr) (cstr_new(cstr), stk_push(&(cstr)->data))
#define cstr_free_s(cstr) (cstr_free(cstr), stk_pop())

#define MCC_ISA_SSE2    0x0001
#define MCC_ISA_SSE3    0x0002
#define MCC_ISA_SSSE3   0x0004
#define MCC_ISA_SSE41   0x0008
#define MCC_ISA_SSE42   0x0010
#define MCC_ISA_POPCNT  0x0020
#define MCC_ISA_AVX     0x0040
#define MCC_ISA_AVX2    0x0080
#define MCC_ISA_FMA     0x0100
#define MCC_ISA_BMI     0x0200
#define MCC_ISA_BMI2    0x0400
#define MCC_ISA_LZCNT   0x0800
#define MCC_ISA_F16C    0x1000
#define MCC_ISA_AVX512F 0x2000
#define MCC_ISA_ARM_BLX 0x4000
#define MCC_ISA_ARM_MOVT 0x8000

ST_FUNC int mcc_isa_has(MCCState *s1, uint32_t feat);
ST_FUNC int mcc_isa_set_arch(MCCState *s1, const char *name);
ST_FUNC void mcc_isa_init(MCCState *s1);
ST_FUNC void mcc_isa_print(MCCState *s1);

ST_FUNC int mcc_add_file_internal(MCCState *s1, const char *filename, int flags);
#define AFF_PRINT_ERROR 0x10
#define AFF_REFERENCED_DLL 0x20
#define AFF_TYPE_BIN 0x40
#define AFF_WHOLE_ARCHIVE 0x80
#define AFF_TYPE_NONE 0
#define AFF_TYPE_C 1
#define AFF_TYPE_ASM 2
#define AFF_TYPE_ASMPP 4
#define AFF_TYPE_LIB 8
#define AFF_TYPE_FRAMEWORK 0x10
#define AFF_TYPE_MASK (7 | AFF_TYPE_BIN)
#define AFF_BINTYPE_REL 1
#define AFF_BINTYPE_DYN 2
#define AFF_BINTYPE_AR 3

#define FILE_NOT_FOUND -2
#define FILE_NOT_RECOGNIZED -3

#if MCC_TARGET_UNIX
ST_FUNC int mcc_add_crt(MCCState *s, const char *filename);
#endif
ST_FUNC int mcc_add_dll(MCCState *s, const char *filename, int flags);
ST_FUNC int mcc_support_arch_match(MCCState *s1, const char *filename);
ST_FUNC int mcc_add_support(MCCState *s1, const char *filename);
ST_FUNC int mcc_add_support_opt(MCCState *s1, const char *filename);
ST_FUNC void mcc_add_bcheck(MCCState *s1);
ST_FUNC void mcc_add_btstub(MCCState *s1);
ST_FUNC void mcc_add_pragma_libs(MCCState *s1);
PUB_FUNC int mcc_add_library_err(MCCState *s, const char *f);
PUB_FUNC void mcc_print_stats(MCCState *s, unsigned total_time);
PUB_FUNC int mcc_parse_args(MCCState *s, int *argc, char ***argv);
ST_FUNC DLLReference *mcc_add_dllref(MCCState *s1, const char *dllname, int level);
ST_FUNC char *mcc_load_text(int fd);
ST_FUNC int normalized_PATHCMP(const char *f1, const char *f2);

#define OPT_HELP 1
#define OPT_HELP2 2
#define OPT_V 3
#define OPT_PRINT_DIRS 4
#define OPT_AR 5
#define OPT_IMPDEF 6
#define OPT_PRINT_PROG 7
#define OPT_PRINT_FILE 8
#define OPT_M32 32
#define OPT_M64 64

ST_DATA struct BufferedFile *file;
ST_DATA int tok;
ST_DATA CValue tokc;
ST_DATA int tok_imaginary;
ST_DATA int tok_bitint_width;
ST_DATA const int *macro_ptr;
ST_DATA int parse_flags;
ST_DATA int tok_flags;
ST_DATA CString tokcstr;

ST_DATA int tok_ident;
ST_DATA TokenSym **table_ident;
ST_DATA int pp_expr;

#define TOK_FLAG_BOL 0x0001
#define TOK_FLAG_BOF 0x0002
#define TOK_FLAG_ENDIF 0x0004

#define PARSE_FLAG_PREPROCESS 0x0001
#define PARSE_FLAG_TOK_NUM 0x0002
#define PARSE_FLAG_LINEFEED 0x0004
#define PARSE_FLAG_ASM_FILE 0x0008
#define PARSE_FLAG_SPACES 0x0010
#define PARSE_FLAG_ACCEPT_STRAYS 0x0020
#define PARSE_FLAG_TOK_STR 0x0040

#define IS_SPC 1
#define IS_ID 2
#define IS_NUM 4

enum line_macro_output_format {
	LINE_MACRO_OUTPUT_FORMAT_GCC,
	LINE_MACRO_OUTPUT_FORMAT_NONE,
	LINE_MACRO_OUTPUT_FORMAT_STD,
	LINE_MACRO_OUTPUT_FORMAT_P10 = 11
};

ST_FUNC TokenSym *tok_alloc(const char *str, int len);
ST_FUNC int tok_alloc_const(const char *str);
ST_FUNC const char *get_tok_str(int v, CValue *cv);
ST_FUNC void begin_macro(TokenString *str, int alloc);
ST_FUNC void end_macro(void);
ST_FUNC int set_idnum(int c, int val);
ST_INLN void tok_str_new(TokenString *s);
ST_FUNC TokenString *tok_str_alloc(void);
ST_FUNC void tok_str_free(TokenString *s);
ST_FUNC void tok_str_free_str(int *str);
ST_FUNC void tok_str_add(TokenString *s, int t);
ST_FUNC void tok_str_add_tok(TokenString *s);
ST_INLN void define_push(int v, int macro_type, int *str, Sym *first_arg);
ST_FUNC void define_undef(Sym *s);
ST_INLN Sym *define_find(int v);
ST_FUNC int pp_macro_is_func(int v);
ST_FUNC int pp_macro_eval(int v, const int64_t *args, int nargs, int64_t *res);
ST_FUNC int pp_in_system_header(void);
ST_FUNC void free_defines(Sym *b);
ST_FUNC void parse_define(void);
ST_FUNC void skip_to_eol(int warn);
ST_FUNC void preprocess(int is_bof);
ST_FUNC void next(void);
ST_INLN void unget_tok(int last_tok);
ST_FUNC void preprocess_start(MCCState *s1, int filetype);
ST_FUNC void mccpp_run_imacros(MCCState *s1);
ST_FUNC void preprocess_end(MCCState *s1);
ST_FUNC void mccpp_new(MCCState *s);
ST_FUNC void mccpp_delete(MCCState *s);
ST_FUNC void mccpp_putfile(const char *filename);
ST_FUNC int mcc_preprocess(MCCState *s1);
ST_FUNC void skip(int c);
ST_FUNC NORETURN void expect(const char *msg);
ST_FUNC void pp_error(CString *cs);

static inline int is_space(int ch) {
	return ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r';
}

static inline int isid(int c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline int isnum(int c) {
	return c >= '0' && c <= '9';
}

static inline int isoct(int c) {
	return c >= '0' && c <= '7';
}

static inline int toup(int c) {
	return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

#define SYM_POOL_NB (8192 / sizeof(Sym))

ST_DATA Sym *global_stack;
ST_DATA Sym *local_stack;
ST_DATA Sym *local_label_stack;
ST_DATA Sym *global_label_stack;
ST_DATA Sym *define_stack;
ST_DATA CType int_type, func_old_type, char_pointer_type;
ST_DATA SValue *vtop;
ST_DATA int rsym, anon_sym, ind, loc;
ST_DATA long mcc_stackref_count;
ST_FUNC void mcc_stackref_note(int r);
ST_FUNC void mcc_stackref_commit(void);
ST_DATA char debug_modes;

ST_DATA int nocode_wanted;
ST_DATA int asm_lvalue_cast;
ST_DATA int global_expr;
ST_DATA CType func_vt;
ST_DATA int func_var;
ST_DATA int func_naked;
ST_DATA int func_vc;
ST_DATA int func_ind;
ST_DATA const char *funcname;

ST_FUNC void mccgen_init(MCCState *s1);
ST_FUNC int mccgen_compile(MCCState *s1);
ST_FUNC void mccgen_finish(MCCState *s1);
ST_FUNC void check_vstack(void);

ST_INLN int is_float(int t);
ST_INLN int is_float16(int t);
ST_INLN int is_float_abi(int t);
ST_FUNC uint16_t f32_to_f16_bits(float x);
ST_FUNC float f16_bits_to_f32(uint16_t h);
ST_FUNC float f16_round(long double v);
ST_FUNC uint16_t f32_to_bf16_bits(float x);
ST_FUNC float bf16_bits_to_f32(uint16_t h);
ST_FUNC float bf16_round(long double v);
ST_FUNC int ieee_finite(double d);
ST_FUNC int exact_log2p1(int i);
ST_FUNC void test_lvalue(void);
ST_FUNC void gen_cast_s(int t);
ST_INLN CType *pointed_type(CType *type);

ST_FUNC ElfSym *elfsym(Sym *);
ST_FUNC void update_storage(Sym *sym);
ST_FUNC void put_extern_sym2(Sym *sym, int sh_num, addr_t value, unsigned long size, int can_add_underscore);
ST_FUNC void put_extern_sym(Sym *sym, Section *section, addr_t value, unsigned long size);
#if MCC_PTR_SIZE == 4
ST_FUNC void greloc(Section *s, Sym *sym, unsigned long offset, int type);
#endif
ST_FUNC void greloca(Section *s, Sym *sym, unsigned long offset, int type, addr_t addend);

ST_INLN void sym_free(Sym *sym);
ST_FUNC Sym *sym_push(int v, CType *type, int r, int c);
ST_FUNC void sym_pop(Sym **ptop, Sym *b, int keep);
ST_FUNC Sym *sym_push2(Sym **ps, int v, int t, int c);
ST_FUNC Sym *sym_find2(Sym *s, int v);
ST_INLN Sym *sym_find(int v);
ST_FUNC Sym *label_find(int v);
ST_FUNC Sym *label_push(Sym **ptop, int v, int flags);
ST_FUNC void label_pop(Sym **ptop, Sym *slast, int keep);
ST_INLN Sym *struct_find(int v);

ST_FUNC Sym *global_identifier_push(int v, int t, int c);
ST_FUNC Sym *external_global_sym(int v, CType *type);
ST_FUNC Sym *external_helper_sym(int v);
ST_FUNC void vpush_helper_func(int v);
ST_FUNC void vset(CType *type, int r, int v);
ST_FUNC void vset_VT_CMP(int op);
ST_FUNC void vpushi(int v);
ST_FUNC void vpushv(SValue *v);
ST_FUNC void vpushsym(CType *type, Sym *sym);
ST_FUNC void vswap(void);
ST_FUNC void vrott(int n);
ST_FUNC void vrotb(int n);
ST_FUNC void vrev(int n);
ST_FUNC void vpop(void);
#if MCC_PTR_SIZE == 4
ST_FUNC void lexpand(void);
#endif
#ifdef MCC_TARGET_ARM
ST_FUNC int get_reg_ex(int rc, int rc2);
#endif
ST_FUNC void save_reg(int r);
ST_FUNC void save_reg_upstack(int r, int n);
ST_FUNC int get_reg(int rc);
ST_FUNC void save_regs(int n);
ST_FUNC void gaddrof(void);
ST_FUNC int gv(int rc);
ST_FUNC int gv_cast_rvalue(void);
ST_FUNC void gv2(int rc1, int rc2);
ST_FUNC void gen_op(int op);
ST_FUNC int type_size(CType *type, int *a);
ST_FUNC int builtin_libm_is(const char *base);
ST_FUNC void mk_pointer(CType *type);
ST_FUNC void vstore(void);
ST_FUNC void inc(int post, int c);
ST_FUNC CString *parse_mult_str(const char *msg);
ST_FUNC CString *parse_asm_str(void);
ST_FUNC void indir(void);
ST_FUNC void unary(void);
ST_FUNC void gexpr(void);
#define MCC_MAX_PARSE_DEPTH 384
ST_DATA int mcc_parse_depth;
ST_FUNC void mcc_parse_depth_enter(void);
ST_FUNC void mcc_parse_depth_leave(void);
ST_FUNC int expr_const(void);
ST_FUNC int64_t expr_const64_pub(void);
ST_FUNC void cst_capture_begin(const char *filename);
ST_FUNC CstArena *cst_capture_end(void);
ST_FUNC Sym *get_sym_ref(CType *type, Section *sec, unsigned long offset, unsigned long size);
#if defined MCC_TARGET_X86_64 && !defined MCC_TARGET_PE
ST_FUNC int classify_x86_64_va_arg(CType *ty);
#endif
ST_FUNC void gbound_args(int nb_args);
ST_DATA int func_bound_add_epilog;
ST_FUNC int gen_bounds_epilog_head(addr_t func_bound_offset,
																	 Sym **psym_data, int *poffset_modified);
ST_FUNC int gen_asan_stack_epilog_head(addr_t off, Sym **psym);
ST_FUNC Sym *gfunc_set_param(Sym *s, int c, int byref);

#define MCC_OUTPUT_FORMAT_ELF 0
#define MCC_OUTPUT_FORMAT_BINARY 1
#define MCC_OUTPUT_FORMAT_COFF 2
#define MCC_OUTPUT_DYN MCC_OUTPUT_DLL

#define ARMAG "!<arch>\n"

typedef struct
{
	unsigned int n_strx;
	unsigned char n_type;
	unsigned char n_other;
	unsigned short n_desc;
	unsigned int n_value;
} Stab_Sym;

ST_FUNC void mccelf_new(MCCState *s);
ST_FUNC void mccelf_delete(MCCState *s);
ST_FUNC void mccelf_begin_file(MCCState *s1);
ST_FUNC void mccelf_end_file(MCCState *s1);
ST_FUNC Section *new_section(MCCState *s1, const char *name, int sh_type, int sh_flags);
ST_FUNC void section_realloc(Section *sec, unsigned long new_size);
ST_FUNC size_t section_add(Section *sec, addr_t size, int align);
ST_FUNC void *section_ptr_add(Section *sec, addr_t size);
ST_FUNC Section *find_section(MCCState *s1, const char *name);
ST_FUNC void free_section(Section *s);
ST_FUNC Section *new_symtab(MCCState *s1, const char *symtab_name, int sh_type, int sh_flags, const char *strtab_name,
														const char *hash_name, int hash_sh_flags);
ST_FUNC void init_symtab(Section *s);

ST_FUNC int put_elf_str(Section *s, const char *sym);
ST_FUNC int put_elf_sym(Section *s, addr_t value, unsigned long size, int info, int other, int shndx, const char *name);
ST_FUNC int set_elf_sym(Section *s, addr_t value, unsigned long size, int info, int other, int shndx, const char *name);
ST_FUNC int find_elf_sym(Section *s, const char *name);
ST_FUNC void put_elf_reloc(Section *symtab, Section *s, unsigned long offset, int type, int symbol);
ST_FUNC void put_elf_reloca(Section *symtab, Section *s, unsigned long offset, int type, int symbol, addr_t addend);

ST_FUNC void resolve_common_syms(MCCState *s1);
ST_FUNC void relocate_syms(MCCState *s1, Section *symtab, int do_resolve);
ST_FUNC void relocate_sections(MCCState *s1);

ST_FUNC int mcc_winfd_open_ro(const char *path);
ST_FUNC ssize_t mcc_fd_read(int fd, void *buf, size_t count);
ST_FUNC long mcc_fd_lseek(int fd, long offset, int whence);
ST_FUNC int mcc_fd_close(int fd);
ST_FUNC ssize_t full_read(int fd, void *buf, size_t count);
ST_FUNC void *load_data(int fd, unsigned long file_offset, unsigned long size);
ST_FUNC int mcc_object_type(int fd, ElfW(Ehdr) * h);
ST_FUNC int mcc_load_object_file(MCCState *s1, int fd, unsigned long file_offset);
ST_FUNC int mcc_load_archive(MCCState *s1, int fd, int alacarte);
ST_FUNC void add_array(MCCState *s1, const char *sec, int c);
ST_FUNC void reorder_ctor_array(MCCState *s1, const char *secname, void **prios, int nb_prio);

ST_FUNC struct sym_attr *get_sym_attr(MCCState *s1, int index, int alloc);
ST_FUNC addr_t get_sym_addr(MCCState *s, const char *name, int err, int forc);
ST_FUNC void list_elf_symbols(MCCState *s, void *ctx,
															void (*symbol_cb)(void *ctx, const char *name, const void *val));
ST_FUNC int set_global_sym(MCCState *s1, const char *name, Section *sec, addr_t offs);

#define for_each_elem(sec, startoff, elem, type) \
	for (elem = (type *)sec->data + startoff;      \
			 elem < (type *)(sec->data + sec->data_offset); elem++)

#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_I386) || defined(MCC_TARGET_ARM) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64)
#define MCC_HAVE_DISASM 1
#endif

typedef struct disasm_ctx {
	MCCState *s1;
	Section *sec;
	Section *symtab;
	unsigned char *data;
	unsigned long size;
	addr_t pc;
	FILE *out;
	int collect;
	addr_t *labels;
	int nlabels, labels_cap;
	char relocbuf[256];
	char labelbuf[32];
	char dis_namepool[8][24];
	int dis_namepool_i;
} disasm_ctx;

ST_FUNC int asm_output_file(MCCState *s1, const char *filename);

ST_FUNC const char *disasm_reloc(disasm_ctx *dc, addr_t off, int size, int *ptype);

#ifdef MCC_HAVE_DISASM

ST_FUNC const char *disasm_label(disasm_ctx *dc, addr_t target);

ST_FUNC int mcc_disasm_insn(disasm_ctx *dc);

ST_FUNC int mcc_disasm_reloc_size(int type);

ST_FUNC int mcc_disasm_reloc_addend_bias(int type, int size);
#endif

#if MCC_TARGET_UNIX
ST_FUNC int mcc_load_dll(MCCState *s1, int fd, const char *filename, int level);
ST_FUNC int mcc_load_ldscript(MCCState *s1, int fd);
ST_FUNC void mccelf_add_crtbegin(MCCState *s1);
ST_FUNC void mccelf_add_crtend(MCCState *s1);
#endif
#ifndef MCC_TARGET_PE
ST_FUNC void mcc_add_runtime(MCCState *s1);
#endif
#ifdef MCC_EMBED_MCCRT
ST_FUNC int mcc_add_mccrt_embedded(MCCState *s1);
#endif
#if MCC_HOST_LINUX
ST_FUNC int mcc_run_pthread_create(void *th, const void *attr,
																	 void *(*fn)(void *), void *arg);
#endif
#if defined(MCC_TARGET_PE) && MCC_HOST_WIN32
ST_FUNC uintptr_t mcc_run_beginthreadex(void *security, unsigned stack_size,
																				unsigned(__stdcall *start)(void *), void *arg,
																				unsigned flags, unsigned *thrdaddr);
#endif

ST_FUNC int code_reloc(int reloc_type);
ST_FUNC int gotplt_entry_type(int reloc_type);

enum gotplt_entry {
	NO_GOTPLT_ENTRY,
	BUILD_GOT_ONLY,
	AUTO_GOTPLT_ENTRY,
	ALWAYS_GOTPLT_ENTRY
};

ST_FUNC unsigned create_plt_entry(MCCState *s1, unsigned got_offset, struct sym_attr *attr);
ST_FUNC void relocate_plt(MCCState *s1);
#if defined(MCC_TARGET_ARM64)
ST_FUNC void arm64_veneer_memory_calls(MCCState *s1);
#endif
#if defined(MCC_TARGET_ARM)
ST_FUNC void arm_veneer_memory_calls(MCCState *s1);
#endif
#if defined(MCC_TARGET_RISCV64)
ST_FUNC void riscv64_veneer_memory_calls(MCCState *s1);
#endif
ST_FUNC void build_got_entries(MCCState *s1, int got_sym);

ST_FUNC void relocate(MCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val);

ST_DATA const char *const target_machine_defs;
ST_DATA int reg_classes[MCC_NB_REGS];

ST_FUNC void gsym_addr(int t, int a);
ST_FUNC void gsym(int t);
ST_FUNC void load(int r, SValue *sv);
ST_FUNC void store(int r, SValue *v);
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *align, int *regsize);
ST_FUNC void gfunc_call(int nb_args);
ST_FUNC void gfunc_prolog(Sym *func_sym);
ST_FUNC void gfunc_epilog(void);
ST_FUNC void gen_fill_nops(int);
ST_FUNC int gjmp(int t);
ST_FUNC void gjmp_addr(int a);
ST_FUNC int gjmp_cond(int op, int t);
ST_FUNC int gjmp_append(int n, int t);
ST_FUNC void gen_opi(int op);
ST_FUNC void gen_opf(int op);
void gen_trap(void);
#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64 || defined MCC_TARGET_I386 || defined MCC_TARGET_ARM
void gen_ubsan_nullptr(void);
#endif
#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64 ||                    \
		defined MCC_TARGET_RISCV64 || defined MCC_TARGET_I386 ||                    \
		defined MCC_TARGET_ARM
ST_FUNC int gen_cmov(int rt, int rf, int rb, int ll);
ST_FUNC void gen_select(CType *type);
#endif
#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64 || defined MCC_TARGET_I386 || defined MCC_TARGET_ARM
void gen_asan_shadow_check(int sz);
void gen_asan_mark_write(void);
#endif
ST_FUNC void gen_cvt_ftoi(int t);
ST_FUNC void gen_cvt_itof(int t);
ST_FUNC void gen_cvt_ftof(int t);
ST_FUNC void ggoto(void);
ST_FUNC void o(unsigned int c);
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
ST_FUNC void gen_x87_pop(void);
#endif
ST_FUNC void gen_vla_sp_save(int addr);
ST_FUNC void gen_vla_sp_restore(int addr);
ST_FUNC void gen_vla_alloc(CType *type, int align);

static inline uint16_t read16le(unsigned char *p) {
	return p[0] | (uint16_t)p[1] << 8;
}

static inline void write16le(unsigned char *p, uint16_t x) {
	p[0] = x & 255;
	p[1] = x >> 8 & 255;
}

static inline uint32_t read32le(unsigned char *p) {
	return read16le(p) | (uint32_t)read16le(p + 2) << 16;
}

static inline void write32le(unsigned char *p, uint32_t x) {
	write16le(p, x);
	write16le(p + 2, x >> 16);
}

static inline void add32le(unsigned char *p, int32_t x) {
	write32le(p, read32le(p) + x);
}

static inline uint64_t read64le(unsigned char *p) {
	return read32le(p) | (uint64_t)read32le(p + 4) << 32;
}

static inline void write64le(unsigned char *p, uint64_t x) {
	write32le(p, x);
	write32le(p + 4, x >> 32);
}

static inline void add64le(unsigned char *p, int64_t x) {
	write64le(p, read64le(p) + x);
}

ST_FUNC void g(int c);
ST_FUNC void gen_le16(int c);
#if defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM
ST_FUNC void gen_le32(int c);
#endif
#if defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64
ST_FUNC int oad(int c, int s);
ST_FUNC void gen_addr32(int r, Sym *sym, int c);
ST_FUNC void gen_addrpc32(int r, Sym *sym, int c);
ST_FUNC void gen_cvt_csti(int t);
ST_FUNC void gen_increment_tcov(SValue *sv);
#endif

#ifdef MCC_TARGET_I386
ST_FUNC void gen_mulh(int sign);
#endif

#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
ST_FUNC void gen_reg_addi(int r, int64_t d);
#endif
#ifdef MCC_TARGET_ARM64
ST_FUNC int arm64_fmov_imm(int r, int is_dbl, uint64_t bits);
#endif

ST_FUNC void gen_fabs(void);
ST_FUNC void gen_sqrt(void);
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
ST_FUNC void gen_bswap(int size);
#endif
#if defined(MCC_TARGET_X86_64)
ST_FUNC void gen_bitscan(int ctz, int size);
#if defined(MCC_TARGET_X86_64)
ST_FUNC int signbit_inline_on(void);
ST_FUNC void gen_signbit(int isfloat);
#endif
ST_FUNC void gen_ffs(int size);
ST_FUNC void gen_atomic_xadd(int size);
ST_FUNC void gen_atomic_xchg(int size);
ST_FUNC void gen_atomic_cmpxchg(int size);
#endif
ST_FUNC void gen_round(int mode);
ST_FUNC void gen_copysign(void);
ST_FUNC void gen_fminmax(int is_max);
ST_FUNC void gen_fma(void);
#ifdef MCC_TARGET_X86_64
ST_FUNC void gen_opl(int op);
ST_FUNC void gen_mulh(int sign);
#ifdef MCC_TARGET_PE
ST_FUNC void gen_vla_result(int addr);
#endif
ST_FUNC void gen_cvt_sxtw(void);
ST_FUNC void gen_cvt_csti(int t);
#ifdef MCC_TARGET_X86_64
ST_FUNC void gen_cvt_trunc32(void);
#endif
#endif

#ifdef MCC_TARGET_ARM
#if defined(MCC_ARM_EABI) && !defined(MCC_CONFIG_ELFINTERP)
PUB_FUNC const char *default_elfinterp(struct MCCState *s);
#endif
ST_FUNC void arm_init(struct MCCState *s);
ST_FUNC void gen_increment_tcov(SValue *sv);
#endif

#ifdef MCC_TARGET_ARM64
ST_FUNC void gen_opl(int op);
ST_FUNC void gen_mulh(int sign);
ST_FUNC void gfunc_return(CType *func_type);
ST_FUNC void gen_va_start(void);
ST_FUNC void gen_va_arg(CType *t);
ST_FUNC void gen_clear_cache(void);
ST_FUNC void gen_cvt_sxtw(void);
ST_FUNC void gen_cvt_csti(int t);
ST_FUNC void gen_increment_tcov(SValue *sv);
#endif

#ifdef MCC_TARGET_RISCV64
ST_FUNC void gen_opl(int op);
ST_FUNC void gen_mulh(int sign);
ST_FUNC void gen_va_start(void);
ST_FUNC void arch_transfer_ret_regs(int);
ST_FUNC void gen_cvt_sxtw(void);
ST_FUNC void gen_cvt_csti(int t);
ST_FUNC void gen_increment_tcov(SValue *sv);
ST_FUNC void gen_clear_cache(void);
#endif

#if defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_PE)
ST_FUNC void arch_transfer_ret_regs(int);
#endif

ST_FUNC void asm_instr(void);
ST_FUNC void asm_global_instr(void);
ST_FUNC int mcc_assemble(MCCState *s1, int do_preprocess);
ST_FUNC void mcc_assemble_inline(MCCState *s1, const char *str, int len, int global);
ST_FUNC void mcc_asm_inline_unwind(void);
void ir_cap_teardown(void);
void rir_teardown(void);
void ir_cap_asm(const char *str, int len, int global);
#define MCC_ASM_EFF_VOLATILE 0x01
#define MCC_ASM_EFF_GOTO 0x02
#define MCC_ASM_EFF_MEM 0x04
#define MCC_ASM_EFF_CC 0x08
#define MCC_ASM_EFF_X87 0x10
#define MCC_ASM_EFF_CCOUT 0x20

void ir_cap_asm_gen_code(ASMOperand *operands, int nb_operands, int nb_outputs,
												 int nb_labels, int eff, int is_output,
												 uint8_t *clobber_regs, int out_reg);
ST_FUNC int find_constraint(ASMOperand *operands, int nb_operands, const char *name, const char **pp);
ST_FUNC const char *skip_constraint_modifiers(const char *p);
ST_FUNC Sym *get_asm_sym(int name, Sym *csym);
ST_FUNC void asm_expr(MCCState *s1, ExprValue *pe);
ST_FUNC int asm_int_expr(MCCState *s1);
#if MCC_PTR_SIZE == 8
ST_FUNC void gen_expr64(ExprValue *pe);
#endif
ST_FUNC void gen_expr32(ExprValue *pe);
ST_FUNC void asm_opcode(MCCState *s1, int opcode);
ST_FUNC int asm_parse_regvar(int t);
ST_FUNC void asm_compute_constraints(ASMOperand *operands, int nb_operands, int nb_outputs, const uint8_t *clobber_regs,
																		 int *pout_reg);
ST_FUNC void subst_asm_operand(CString *add_str, SValue *sv, int modifier);
ST_FUNC void asm_gen_code(ASMOperand *operands, int nb_operands, int nb_outputs, int is_output, uint8_t *clobber_regs,
													int out_reg);
ST_FUNC void asm_clobber(uint8_t *clobber_regs, const char *str);

#ifdef MCC_TARGET_PE
ST_FUNC int pe_load_file(struct MCCState *s1, int fd, const char *filename);
ST_FUNC int coff_load_object_file(MCCState *s1, int fd, unsigned long file_offset);
ST_FUNC int coff_object_type(int fd, unsigned long file_offset);
ST_FUNC int coff_output_obj(MCCState *s1, const char *filename);
ST_FUNC int coff_import_func_info(int fd, unsigned long off, char *impname, size_t impsz, char *expname, size_t expsz, char *headsym, size_t headsz);
ST_FUNC int coff_import_dllname(int fd, unsigned long off, char *dll, size_t dsz);
ST_FUNC int coff_short_import_info(int fd, unsigned long off, char *impname, size_t impsz, char *expname, size_t expsz, char *dll, size_t dsz, int *ordinal);
ST_FUNC void pe_import_set_alias(MCCState *s1, const char *sym, const char *bind);
ST_FUNC const char *pe_import_bindname(MCCState *s1, const char *sym);
ST_FUNC int pe_output_file(MCCState *s1, const char *filename);
ST_FUNC int pe_putimport(MCCState *s1, int dllindex, const char *name, addr_t value);
ST_FUNC int pe_setsubsy(MCCState *s1, const char *arg);
ST_FUNC Sym *pe_tls_index_sym(void);
#if defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64
#endif
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
ST_FUNC void pe_add_unwind_data(unsigned start, unsigned end, unsigned stack);
#endif
#ifdef MCC_TARGET_X86_64
ST_FUNC void pe_seh_reset(void);
ST_FUNC void pe_seh_scope(unsigned begin, unsigned end, unsigned filter, unsigned handler);
ST_FUNC void pe_seh_scope_funclet(unsigned begin, unsigned end, unsigned filt_start, unsigned filt_end, unsigned handler);
ST_FUNC void pe_seh_scope_finally(unsigned begin, unsigned end, unsigned fin_start, unsigned fin_end);
#endif
PUB_FUNC int mcc_get_dllexports(const char *filename, char **pp);
#define ST_PE_EXPORT 0x10
#define ST_PE_IMPORT 0x20
#define ST_PE_STDCALL 0x40
#endif
#define ST_ASM_SET 0x04

#ifdef MCC_TARGET_MACHO
ST_FUNC int macho_output_file(MCCState *s1, const char *filename);
ST_FUNC int macho_load_dll(MCCState *s1, int fd, const char *filename, int lev);
ST_FUNC int macho_load_tbd(MCCState *s1, int fd, const char *filename, int lev);
ST_FUNC int macho_object_type(int fd, unsigned long file_offset);
ST_FUNC int macho_load_object_file(MCCState *s1, int fd, unsigned long file_offset);
#if defined MCC_TARGET_IS_HOST || MCC_HOST_DARWIN
ST_FUNC void mcc_add_macos_sdkincludepath(MCCState *s);
ST_FUNC void mcc_add_macos_frameworkpath(MCCState *s);
#endif
#ifdef MCC_TARGET_IS_HOST
ST_FUNC void mcc_add_macos_sdkpath(MCCState *s);
ST_FUNC char *macho_tbd_soname(int fd);
#endif
#endif
#ifdef MCC_TARGET_IS_HOST
ST_FUNC void mcc_run_free(MCCState *s1);
#endif

ST_FUNC void mcc_debug_new(MCCState *s);

ST_FUNC void mcc_debug_start(MCCState *s1);
ST_FUNC void mcc_debug_end(MCCState *s1);
ST_FUNC void mcc_debug_bincl(MCCState *s1);
ST_FUNC void mcc_debug_eincl(MCCState *s1);
ST_FUNC void mcc_debug_newfile(MCCState *s1);

ST_FUNC void mcc_debug_line(MCCState *s1);
ST_FUNC void mcc_add_debug_info(MCCState *s1, Sym *s, Sym *e);
ST_FUNC void mcc_debug_funcstart(MCCState *s1, Sym *sym);
ST_FUNC void mcc_debug_prolog_epilog(MCCState *s1, int value);
ST_FUNC void mcc_debug_funcend(MCCState *s1, int size);
#ifdef MCC_TARGET_PE
ST_FUNC void mcc_cv_funcstart(MCCState *s1, Sym *sym);
ST_FUNC void mcc_cv_line(MCCState *s1, int line);
ST_FUNC void mcc_cv_funcend(MCCState *s1, int size);
ST_FUNC void mcc_cv_emit(MCCState *s1);
#endif
ST_FUNC void mcc_debug_extern_sym(MCCState *s1, Sym *sym, int sh_num, int sym_bind, int sym_type);
ST_FUNC void mcc_debug_typedef(MCCState *s1, Sym *sym);
ST_FUNC void mcc_debug_stabn(MCCState *s1, int type, int value);
ST_FUNC void mcc_debug_fix_forw(MCCState *s1, CType *t);

#if MCC_TARGET_UNIX && !defined MCC_TARGET_ARM && !defined MCC_TARGETOS_BSD
ST_FUNC void mcc_eh_frame_start(MCCState *s1);
ST_FUNC void mcc_eh_frame_end(MCCState *s1);
ST_FUNC void mcc_debug_frame_end(MCCState *s1, int size);
ST_FUNC void mcc_eh_frame_hdr(MCCState *s1, int final);
#define MCC_EH_FRAME 1
#endif

ST_FUNC void mcc_tcov_start(MCCState *s1);
ST_FUNC void mcc_tcov_end(MCCState *s1);
ST_FUNC void mcc_tcov_check_line(MCCState *s1, int start);
ST_FUNC void mcc_tcov_block_end(MCCState *s1, int line);
ST_FUNC void mcc_tcov_block_begin(MCCState *s1);
ST_FUNC void mcc_tcov_reset_ind(MCCState *s1);

#define stab_section s1->stab_section
#define stabstr_section stab_section->link
#define tcov_section s1->tcov_section
#define eh_frame_section s1->eh_frame_section
#define eh_frame_hdr_section s1->eh_frame_hdr_section
#define dwarf_info_section s1->dwarf_info_section
#define dwarf_abbrev_section s1->dwarf_abbrev_section
#define dwarf_line_section s1->dwarf_line_section
#define dwarf_aranges_section s1->dwarf_aranges_section
#define dwarf_str_section s1->dwarf_str_section
#define dwarf_line_str_section s1->dwarf_line_str_section

#define DWARF_MAX_128 ((8 * sizeof(int64_t) + 6) / 7)
#define dwarf_read_1(ln, end) \
	((ln) < (end) ? *(ln)++ : 0)
#define dwarf_read_2(ln, end) \
	((ln) + 1 < (end) ? read16le(((ln) += 2) - 2) : 0)
#define dwarf_read_4(ln, end) \
	((ln) + 3 < (end) ? read32le(((ln) += 4) - 4) : 0)
#define dwarf_read_8(ln, end) \
	((ln) + 7 < (end) ? read64le(((ln) += 8) - 8) : 0)

static inline uint64_t
dwarf_read_uleb128(unsigned char **ln, unsigned char *end) {
	unsigned char *cp = *ln;
	uint64_t retval = 0;
	int i;
	for (i = 0; i < DWARF_MAX_128; i++) {
		uint64_t byte = dwarf_read_1(cp, end);
		retval |= (byte & 0x7f) << (i * 7);
		if ((byte & 0x80) == 0)
			break;
	}
	*ln = cp;
	return retval;
}

static inline int64_t
dwarf_read_sleb128(unsigned char **ln, unsigned char *end) {
	unsigned char *cp = *ln;
	int64_t retval = 0;
	int i;
	for (i = 0; i < DWARF_MAX_128; i++) {
		uint64_t byte = dwarf_read_1(cp, end);
		retval |= (byte & 0x7f) << (i * 7);
		if ((byte & 0x80) == 0) {
			if ((byte & 0x40) && (i + 1) * 7 < 64)
				retval |= (uint64_t)-1LL << ((i + 1) * 7);
			break;
		}
	}
	*ln = cp;
	return retval;
}

#ifdef MCC_TARGET_MACHO
#define DEFAULT_DWARF_VERSION 2
#else
#define DEFAULT_DWARF_VERSION 5
#endif

#ifndef MCC_CONFIG_DWARF_VERSION
#define MCC_CONFIG_DWARF_VERSION 0
#endif

#ifndef MCC_CONFIG_NEW_DTAGS
#define MCC_CONFIG_NEW_DTAGS 0
#endif

#if defined MCC_TARGET_X86_64
#define R_DATA_32DW R_X86_64_32
#else
#define R_DATA_32DW R_DATA_32
#endif

#undef ST_DATA
#if MCC_AMALGAMATED
#define ST_DATA static
#else
#define ST_DATA
#endif

#define text_section MCC_STATE_VAR(text_section)
#define data_section MCC_STATE_VAR(data_section)
#define rodata_section MCC_STATE_VAR(rodata_section)
#define bss_section MCC_STATE_VAR(bss_section)
#define tdata_section MCC_STATE_VAR(tdata_section)
#define tbss_section MCC_STATE_VAR(tbss_section)
#define common_section MCC_STATE_VAR(common_section)
#define cur_text_section MCC_STATE_VAR(cur_text_section)
#define bounds_section MCC_STATE_VAR(bounds_section)
#define lbounds_section MCC_STATE_VAR(lbounds_section)
#define asan_lstack_section MCC_STATE_VAR(asan_lstack_section)
#define symtab_section MCC_STATE_VAR(symtab_section)
#define gnu_ext MCC_STATE_VAR(gnu_ext)
#define mcc_error_noabort MCC_SET_STATE(_mcc_error_noabort)
#define mcc_error MCC_SET_STATE(_mcc_error)
#define mcc_warning MCC_SET_STATE(_mcc_warning)

#define total_idents MCC_STATE_VAR(total_idents)
#define total_lines MCC_STATE_VAR(total_lines)
#define total_toks MCC_STATE_VAR(total_toks)
#define total_funcs MCC_STATE_VAR(total_funcs)
#define total_bytes MCC_STATE_VAR(total_bytes)

PUB_FUNC void mcc_enter_state(MCCState *s1);
PUB_FUNC void mcc_exit_state(MCCState *s1);

#define mcc_warning_c(sw) MCC_SET_STATE(( \
		mcc_state->warn_num = offsetof(MCCState, sw) - offsetof(MCCState, warn_none), _mcc_warning))

#endif

#undef MCC_STATE_VAR
#undef MCC_SET_STATE

#ifdef USING_GLOBALS
#define MCC_STATE_VAR(sym) mcc_state->sym
#define MCC_SET_STATE(fn) fn
#undef USING_GLOBALS
#undef _mcc_error
#else
#define MCC_STATE_VAR(sym) s1->sym
#define MCC_SET_STATE(fn) (mcc_enter_state(s1), fn)
#define _mcc_error use_mcc_error_noabort
#endif
