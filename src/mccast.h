#ifndef MCCAST_H
#define MCCAST_H

#include <stddef.h>
#include <stdint.h>

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

#ifdef __cplusplus
}
#endif

#if MCC_CONFIG_OPTIMIZER && defined(MCC_INTERNAL)

/* Compiler integration (mcc translation unit only): the capture hooks
   mccgen.c drives while parsing a function body, and the replay,
   promotion and inline drivers that re-emit the body from the recorded
   intention tree.  Implemented in the MCC_INTERNAL section of mccast.c. */

struct MCCState;
struct Sym;
struct CType;

extern int ast_active;    /* capturing the current function body */
extern int ast_replaying; /* re-emitting code from the captured tree */
extern int ast_in_op;     /* >0: helper-emitted values, not source ops */
extern int ast_bail;      /* body not representable; drop the capture */
extern int ast_func_has_asm;
extern unsigned ast_pinned_regs; /* regs promotion pinned; allocator must skip */

void ast_configure(struct MCCState *s1);
uint64_t ast_intention_value(void);

void ast_func_begin(struct Sym *sym);
void ast_func_end(struct Sym *sym);
void ast_func_epilog(void);
void ast_reemit_forward_inlines(void);

int ast_sym_defer(struct Sym *sym);
int ast_alloc_loc(int size, int align);
int ast_fconst_reuse(void);
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

#endif

#endif
