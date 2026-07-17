#ifndef MCCAST_H
#define MCCAST_H

#include <stddef.h>
#include <stdint.h>

#include "mccname.h"

typedef enum AstKind {
	AST_TranslationUnit = 0,
	AST_BasicBlock,
	AST_If,
	AST_Jump,
	AST_Return,
	AST_Ref,
	AST_Literal,
	AST_Load,
	AST_Store,
	AST_Unary,
	AST_Binary,
	AST_Convert,
	AST_Invoke,
	AST_InitList,
	AST_Poison,
	AST_Data,

	AST_KIND_COUNT
} AstKind;

typedef uint32_t AstLocal;

#define AST_NONE ((AstLocal)0xffffffffu)

typedef struct AstArena AstArena;

#ifdef __cplusplus
extern "C" {
#endif

AstArena *ast_arena_new(void);
AstArena *ast_arena_clone(const AstArena *src);
void ast_arena_free(AstArena *a);
void ast_arena_reset(AstArena *a);

AstLocal ast_node(AstArena *a, uint16_t kind);
void ast_add_child(AstArena *a, AstLocal parent, AstLocal child);

void ast_set_kind(AstArena *a, AstLocal n, uint16_t kind);
void ast_clear_children(AstArena *a, AstLocal n);

void ast_set_op(AstArena *a, AstLocal n, int op);
void ast_set_type(AstArena *a, AstLocal n, int type_t, uint64_t type_ref);
void ast_set_ival(AstArena *a, AstLocal n, uint64_t v);
void ast_set_fbits(AstArena *a, AstLocal n, uint64_t bits);
void ast_set_sym(AstArena *a, AstLocal n, uint64_t sym);
void ast_set_cst(AstArena *a, AstLocal n, uint64_t cst_id);

uint16_t ast_kind(const AstArena *a, AstLocal n);
int ast_op(const AstArena *a, AstLocal n);
int ast_type_t(const AstArena *a, AstLocal n);
uint64_t ast_type_ref(const AstArena *a, AstLocal n);
uint64_t ast_ival(const AstArena *a, AstLocal n);
uint64_t ast_fbits(const AstArena *a, AstLocal n);
uint64_t ast_sym(const AstArena *a, AstLocal n);
uint64_t ast_cst(const AstArena *a, AstLocal n);

AstLocal ast_parent(const AstArena *a, AstLocal n);
AstLocal ast_first_child(const AstArena *a, AstLocal n);
AstLocal ast_last_child(const AstArena *a, AstLocal n);
AstLocal ast_next_sib(const AstArena *a, AstLocal n);
uint32_t ast_nchild(const AstArena *a, AstLocal n);
AstLocal ast_child(const AstArena *a, AstLocal n, uint32_t i);
AstLocal ast_count(const AstArena *a);
AstLocal ast_root(const AstArena *a);

const char *ast_kind_name(uint16_t kind);
size_t ast_dump(const AstArena *a, AstLocal root, char *out, size_t cap);
int ast_validate(const AstArena *a, char *msg, size_t msgcap);
uint64_t ast_intention_hash(const AstArena *a, AstLocal root);
int ast_color_graph(int n, const uint64_t *adj, const int *cost, int k,
										int *color);

#if MCC_EMBED_JIT
typedef enum AstPurity {
	AST_PURITY_IMPURE = 0,
	AST_PURITY_TIER1 = 1,
	AST_PURITY_TIER0 = 2
} AstPurity;

int ast_fn_purity(const AstArena *a);

/* M5c slicing foundation: partition a function into a pure register/value
   computation kernel + the impure "bound" ops (Store / Invoke / volatile)
   that must stay on the C ABI. This is the analysis the non-ABI kernel
   codegen (K7) and inline-vs-shim (K8) will consume — a function with many
   pure_compute nodes and few impure_ops is the strong slicing candidate. */
typedef struct AstSliceProfile {
	int impure_ops;	 /* C-ABI boundary ops: Store, Invoke, volatile access */
	int loads;			 /* memory-value reads (TIER1) */
	int pure_compute; /* register-value-only compute nodes (the kernel body) */
	int nodes;			 /* total nodes */
} AstSliceProfile;

void ast_fn_slice_profile(const AstArena *a, AstSliceProfile *out);
AstArena *ast_slice_extract(const AstArena *src, AstLocal root);
int ast_slice_certifiable(AstArena *a, AstLocal root);
int ast_slice_equiv(AstArena *a, AstLocal aroot, AstArena *b, AstLocal broot);
int ast_slice_live_ins(AstArena *a, AstLocal root, int32_t *offs, int max);
AstArena *ast_slice_wrap_kernel(const AstArena *a, AstLocal root);
int ast_slice_search(AstArena *a, AstLocal root, int budget, AstLocal *out, int max);

/* 7A eval-slice hard gate: cumulative count of speculative spec-slices refused
   because the UB-soundness oracle flagged them (opt-in `MCC_AST_JIT_EVAL_GATE`). */
int ast_jit_eval_refused_count(void);
#endif

#ifdef __cplusplus
}
#endif

#if MCC_CONFIG_OPTIMIZER && defined(MCC_INTERNAL)

struct MCCState;
struct Sym;
struct CType;

extern int ast_active;
extern int ast_replaying;
extern int ast_in_op;
extern int ast_bail;
extern int ast_func_has_asm;
extern uint64_t ast_pinned_regs;

void ast_configure(struct MCCState *s1);
uint64_t ast_intention_value(void);

void ast_func_begin(struct Sym *sym);
void ast_func_end(struct Sym *sym);
void ast_func_epilog(void);
void ast_reemit_forward_inlines(void);

int ast_sym_defer(struct Sym *sym);
int ast_alloc_loc(int size, int align);
int ast_alloc_temp_loc(int size, int align);
int ast_fconst_reuse(void);
void ast_fconst_reuse_disable(int off);
void ast_fconst_record(int c);
void ast_fconst_push_ref(struct CType *type, int fc);

void ast_hook_stmt(int t);
void ast_hook_return(int has_val);
void ast_hook_return_jmp(int jumps);
void ast_hook_implicit_return(void);
void ast_hook_ret_expr_done(void);
void ast_hook_vpush(void);
void ast_hook_genop(int op);
void ast_hook_genop_end(void);
void ast_hook_vstore(void);
void ast_hook_vstore_end(void);
void ast_hook_vpop(void);
void ast_hook_vswap(void);
void ast_hook_convert(struct CType *type);
void ast_hook_call_begin(int nb_args, int is_struct_ret, int ret_nregs,
												 int variadic);
void ast_hook_call_end(void);
void ast_hook_call_effect_end(void);
void ast_hook_if_begin(void);
void ast_hook_if_gvtst_done(void);
void ast_hook_if_else(void);
void ast_hook_if_end(void);
void ast_hook_while_begin(void);
void ast_hook_while_end(void);
void ast_hook_do_begin(void);
void ast_hook_do_body_end(void);
void ast_hook_do_cond(void);
void ast_hook_do_end(void);
void ast_hook_break_continue(int is_continue);
void ast_hook_for_begin(int has_cond);
void ast_hook_for_cond(void);
void ast_hook_for_incr_begin(void);
void ast_hook_for_incr_end(void);
void ast_hook_for_no_incr(void);
void ast_hook_for_body_begin(void);
void ast_hook_for_end(void);
void ast_hook_switch_begin(void);
void ast_hook_case(int64_t v1, int64_t v2, int type);
void ast_hook_default(void);
void ast_hook_switch_body_end(void);
void ast_hook_switch_end(void);
void ast_hook_label(int v);
void ast_hook_goto(int v);
void ast_hook_inc(int post, int c);
void ast_hook_inc_end(void);
void ast_hook_indir(void);
void ast_hook_gaddrof(void);
void ast_hook_member_begin(int is_arrow);
void ast_hook_member_end(int cumofs, struct CType *mtype, int nonlval,
												 int qual, int bcheck);
void ast_hook_imag_begin(void);
void ast_hook_imag_end(int t);
void ast_hook_builtin_complex_begin(void);
void ast_hook_builtin_complex_end(void);
void ast_hook_vla_alloc_begin(void);
void ast_hook_vla_alloc_end(struct CType *type, int addr, int new_save,
														int locorig);
void ast_hook_vla_restore(int loc);
void ast_hook_ternary_begin(int c, int g);
void ast_hook_ternary_branch(int which);
void ast_hook_ternary_branch_done(int which);
void ast_hook_ternary_end(void);
void ast_hook_landor_operand(int op, int c, int first);
void ast_hook_landor_next(void);
void ast_hook_landor_end(int materialized);

/* M5 const-data visibility (roadmap step M5, first bounded/byte-neutral step). Records
 * one entry per initialized static/global object emitted at parse time — the const data
 * that lives OUTSIDE the per-function AST capture window. Read-only: it observes
 * (section, offset, size) and never changes emitted bytes. The later, non-neutral step
 * (an AstKind for data + a pass that re-emits, enabling M6 datacomp) builds on this
 * visibility. `is_ro` = the object went to .rodata (const). */
void ast_hook_data(void *sec, long off, long size, int is_ro);

/* M6z zero-init .bss placement. `ast_zero_bss_env` (MCC_ZERO_BSS, default on at -O2+) enables
 * moving an all-zero writable static from .data to .bss (NOBITS) — see decl_initializer_alloc.
 * `ast_data_all_zero` is the byte-scan predicate shared by the mover and the analysis. */
extern int ast_zero_bss_env;
int ast_data_all_zero(void *sec, long off, long size);

/* -fmerge-constants-style rodata string-literal pooling (roadmap M6-adjacent). C11 6.4.5p7
 * leaves identical string literals' distinctness unspecified, so sharing storage is sound.
 * `ast_merge_strings_env` (MCC_MERGE_STRINGS, default on at -O2+) enables it in decl_initializer_alloc.
 * `ast_strpool_find_or_add` returns the offset of a byte-identical prior rodata literal (same
 * size+align), or -1 after recording this one. Content-keyed; reset per translation unit. */
extern int ast_merge_strings_env;
long ast_strpool_find_or_add(void *sec, long addr, long size, int align);

int ast_loopnest_build(AstArena *a);
int ast_loop_depth(AstArena *a, AstLocal loop);
AstLocal ast_loop_parent(AstArena *a, AstLocal loop);
int ast_loop_iv(AstArena *a, AstLocal loop, int *off, int *tt, int64_t *stride);
int ast_loop_bounds(AstArena *a, AstLocal loop, int64_t *bound, int *is_lower);
int ast_loop_analyzable(AstArena *a, AstLocal loop);
void ast_loopnest_dump(AstArena *a, const char *fname);

int ast_loop_interchange_legal(AstArena *a, AstLocal outer, AstLocal inner);
int ast_loop_fusion_legal(AstArena *a, AstLocal loop1, AstLocal loop2);
void ast_loopdep_dump(AstArena *a, const char *fname);

#endif

#endif
