#ifndef MCCAST_H
#define MCCAST_H

#include <stddef.h>
#include <stdint.h>

#include "mccname.h"

typedef enum AstKind {
	AST_BasicBlock = 0,
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
	AST_Poison,
	AST_StoreVal,
	AST_Bailout,

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
AstLocal ast_dup_sub(AstArena *a, AstLocal n);
void ast_arena_free(AstArena *a);
void ast_teardown(void);
void ast_arena_reset(AstArena *a);

AstLocal ast_node(AstArena *a, uint16_t kind);
void ast_add_child(AstArena *a, AstLocal parent, AstLocal child);

void ast_set_kind(AstArena *a, AstLocal n, uint16_t kind);
void ast_clear_children(AstArena *a, AstLocal n);
int ast_detach_last_child(AstArena *a, AstLocal parent, AstLocal child);

void ast_set_op(AstArena *a, AstLocal n, int op);
void ast_set_type(AstArena *a, AstLocal n, int type_t, uint64_t type_ref);
void ast_set_type_bf(AstArena *a, AstLocal n, int type_t, uint64_t type_ref,
										 unsigned bp, unsigned bs);
void ast_copy_type(AstArena *a, AstLocal n, const AstArena *src, AstLocal m);
void ast_set_ival(AstArena *a, AstLocal n, uint64_t v);
void ast_set_fbits(AstArena *a, AstLocal n, uint64_t bits);
void ast_set_sym(AstArena *a, AstLocal n, uint64_t sym);

#define AST_R2_NONE 0x30u
void ast_set_wide(AstArena *a, AstLocal n, uint64_t hi, unsigned r2);
uint64_t ast_wide_hi(const AstArena *a, AstLocal n);
unsigned ast_wide_r2(const AstArena *a, AstLocal n);

uint16_t ast_kind(const AstArena *a, AstLocal n);
int ast_op(const AstArena *a, AstLocal n);
int ast_type_t(const AstArena *a, AstLocal n);
int ast_stype_known(const AstArena *a, AstLocal n);
int ast_stype_t(const AstArena *a, AstLocal n);
uint64_t ast_stype_ref(const AstArena *a, AstLocal n);
unsigned ast_stype_bp(const AstArena *a, AstLocal n);
unsigned ast_stype_bs(const AstArena *a, AstLocal n);
void ast_set_stype(AstArena *a, AstLocal n, int t, uint64_t ref, unsigned bp,
									 unsigned bs);
uint64_t ast_type_ref(const AstArena *a, AstLocal n);
unsigned ast_type_bp(const AstArena *a, AstLocal n);
unsigned ast_type_bs(const AstArena *a, AstLocal n);
uint64_t ast_ival(const AstArena *a, AstLocal n);
uint64_t ast_fbits(const AstArena *a, AstLocal n);
uint64_t ast_sym(const AstArena *a, AstLocal n);

AstLocal ast_parent(const AstArena *a, AstLocal n);
AstLocal ast_first_child(const AstArena *a, AstLocal n);
AstLocal ast_last_child(const AstArena *a, AstLocal n);
AstLocal ast_next_sib(const AstArena *a, AstLocal n);
uint32_t ast_nchild(const AstArena *a, AstLocal n);
AstLocal ast_child(const AstArena *a, AstLocal n, uint32_t i);
AstLocal ast_count(const AstArena *a);
AstLocal ast_root(const AstArena *a);

int ast_arena_has_asm(const AstArena *a);

const char *ast_kind_name(uint16_t kind);
const long *ast_low_kind_n(void);
const long *ast_low_kind_untyped(void);
const long *ast_low_kind_void(void);
size_t ast_dump(const AstArena *a, AstLocal root, char *out, size_t cap);
int ast_validate(const AstArena *a, char *msg, size_t msgcap);
uint64_t ast_intention_hash(const AstArena *a, AstLocal root);
void ast_hash_out_emit(const char *tag, const char *fn, uint64_t h);
uint64_t ast_slice_ident_hash(const AstArena *a, AstLocal root);

int ast_slice_splice(AstArena *a, AstLocal site_root, const AstArena *kernel_src,
										 AstLocal kernel_root);
int ast_slice_locate(const AstArena *a, uint64_t ident, AstLocal *sites, int max);
int ast_slice_promote_static(int64_t baseline_cost, int64_t candidate_cost);
long ast_slice_breakeven_lanes(long nodes);
int64_t ast_slice_width_cost(int64_t nodes, int64_t lanes);

int ast_color_graph(int n, const uint64_t *adj, const int *cost, int k,
										int *color);

void ast_ladder_gpu_setup(void);
void ast_ladder_gpu_force(void);
void ast_ladder_gpu_report(void);

int ast_region_disjoint(AstArena *a, AstLocal r1, AstLocal r2);
const char *ast_region_disjoint_why(void);

#if MCC_EMBED_JIT
typedef enum AstPurity {
	AST_PURITY_IMPURE = 0,
	AST_PURITY_TIER1 = 1,
	AST_PURITY_TIER0 = 2
} AstPurity;

int ast_fn_purity(const AstArena *a);
int ast_fn_purity_noescape(const AstArena *a);

typedef struct AstSliceProfile {
	int impure_ops;
	int loads;
	int pure_compute;
	int nodes;
} AstSliceProfile;

void ast_fn_slice_profile(const AstArena *a, AstSliceProfile *out);
AstArena *ast_slice_extract(const AstArena *src, AstLocal root);
int ast_slice_certifiable(AstArena *a, AstLocal root);
int ast_slice_equiv(AstArena *a, AstLocal aroot, AstArena *b, AstLocal broot);
int ast_slice_live_ins(AstArena *a, AstLocal root, int32_t *offs, int max);
AstArena *ast_slice_wrap_kernel(const AstArena *a, AstLocal root);
int ast_slice_search(AstArena *a, AstLocal root, int budget, AstLocal *out, int max);

void ast_slice_ladder_set(int on);
int ast_slice_ladder_on(void);
void ast_slice_ladder_observed_source(int (*fn)(const int32_t *, int, int64_t *,
																								int, void *),
																			void *user);
int ast_slice_ladder_explain(AstArena *a, AstLocal aroot, AstArena *b,
														 AstLocal broot, char *buf, size_t cap);
void ast_slice_ladder_stats_dump(void);

int ast_jit_eval_refused_count(void);
int ast_jit_const_fn(AstArena *a, int64_t *out);
int ast_jit_fold_consts(AstArena *a);
int ast_jit_search_vocab(uint64_t *out, int max);
#endif

#ifdef __cplusplus
}
#endif

#if defined(MCC_INTERNAL)

struct MCCState;
struct Sym;
struct CType;

extern int ast_active;
extern int ast_replaying;
extern int ast_func_has_asm;
extern int ast_func_has_labeladdr;
extern uint64_t ast_pinned_regs;
extern int ast_reemit_guard_op;
extern int ast_regdisp_env;
extern int ast_fmov_imm_env;
extern int ast_trunc32_env;

void ast_configure(struct MCCState *s1);
ST_FUNC int ast_math_errno_folds(struct MCCState *s1);
uint64_t ast_intention_value(void);

void ast_func_begin(struct Sym *sym);
void ast_func_end(struct Sym *sym);
void ast_func_epilog(void);
void ast_reemit_finalize_span(struct Sym *sym);
void ast_reemit_forward_inlines(void);
void ast_reemit_with_gates(struct Sym *sym, AstArena *ast, uint64_t gate_mask);

int ast_env_int(const char *name, int dflt);
long ast_cost_score(AstArena *a);

int ast_sym_defer(struct Sym *sym);
int ast_alloc_loc(int size, int align);
int ast_alloc_temp_loc(int size, int align);
void ast_locrec_snapshot(int out[4]);
void ast_locrec_restore(const int in[4]);
int ast_ircap_suspend(void);
void ast_ircap_resume(int prev);
int ast_ltemp_overlaps(int lo, int sz);
#define AST_FCONST_KEY 36
int ast_fconst_reuse(int cplx, const unsigned char *key);
void ast_fconst_reuse_disable(int off);
void ast_fconst_record(int c, int cplx, const unsigned char *key);
void ast_fconst_push_ref(struct CType *type, int fc);

int ast_label_id(void *s);
void ast_label_forget(void *s);


extern int ast_zero_bss_env;
int ast_data_all_zero(void *sec, long off, long size);

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
int ast_loop_parallel_legal(AstArena *a, AstLocal loop);
const char *ast_loop_parallel_why(void);
void ast_loopdep_dump(AstArena *a, const char *fname, const char *when);

#endif

#endif
