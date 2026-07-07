#ifndef MCCAST_H
#define MCCAST_H

/* The AST — an intention IR alongside the CST (docs/AST.md).
 *
 * Where the CST is byte-faithful concrete syntax, the AST is intention:
 * desugared, type-resolved, post-preprocessor. It is a pure side-channel like
 * the CST: -O0 never builds or reads it; -O1 builds it (lowered from the typed
 * CST / parser) and replays it through the existing vstack API to emit code.
 *
 * This header declares only the structure-agnostic arena/builder/dump API,
 * which is self-contained (no dependency on the compiler's CType/Sym). Types
 * and symbols ride as opaque handles: `type_t` carries the CType.t bit-field
 * and `type_ref`/`sym` carry opaque Sym* casts that the full build fills in and
 * the replay driver (in mccgen.c) reconstructs. The pure-library harness
 * (tools/asttool.c) treats them as tags. */

#include <stddef.h>
#include <stdint.h>

typedef enum AstKind {
	/* Structural */
	AST_TranslationUnit = 0,
	AST_BasicBlock,
	/* Terminators */
	AST_If,
	AST_Jump,
	AST_Return,
	/* Values */
	AST_Ref,      /* address of a named object (Sym)             */
	AST_Literal,  /* integer/float constant                      */
	AST_Load,     /* explicit read of an address                 */
	AST_Store,    /* explicit write: Store(addr, value)          */
	AST_Unary,    /* neg / bitnot (single-operand machine ops)   */
	AST_Binary,   /* control-free binary op (op = token value)   */
	AST_Convert,  /* genuine value conversion (resize/int<->fp)  */
	AST_Invoke,   /* abstract call: callee + arg values          */
	AST_InitList, /* aggregate initializer (blob/memset group)   */
	/* Recovery */
	AST_Poison,

	AST_KIND_COUNT
} AstKind;

typedef uint32_t AstLocal;

#define AST_NONE ((AstLocal)0xffffffffu)

typedef struct AstArena AstArena;

#ifdef __cplusplus
extern "C" {
#endif

/* --- arena lifecycle --- */
AstArena *ast_arena_new(void);
void ast_arena_free(AstArena *a);
void ast_arena_reset(AstArena *a);

/* --- construction --- */
AstLocal ast_node(AstArena *a, uint16_t kind);
void ast_add_child(AstArena *a, AstLocal parent, AstLocal child);

void ast_set_op(AstArena *a, AstLocal n, int op);
void ast_set_type(AstArena *a, AstLocal n, int type_t, uint64_t type_ref);
void ast_set_ival(AstArena *a, AstLocal n, uint64_t v);
void ast_set_fbits(AstArena *a, AstLocal n, uint64_t bits);
void ast_set_sym(AstArena *a, AstLocal n, uint64_t sym);
void ast_set_cst(AstArena *a, AstLocal n, uint64_t cst_id);

/* --- accessors --- */
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

/* --- reflection / diagnostics --- */
const char *ast_kind_name(uint16_t kind);
size_t ast_dump(const AstArena *a, AstLocal root, char *out, size_t cap);
int ast_validate(const AstArena *a, char *msg, size_t msgcap);

#ifdef __cplusplus
}
#endif

/* --- parser-side build hooks (full build only) ------------------------------
 * These fire from the same parse positions as the CST hooks and capture typed
 * vstack values into the per-function AST. They live in mccgen.c (where vtop is
 * visible). When CONFIG_AST is off, or -O1/ast_replay is not requested, they are
 * no-ops so the -O0 path is untouched. */
#if defined(CONFIG_AST) && CONFIG_AST

/* Runtime gate: only build/replay when the driver asked for it. Defined in
 * mccgen.c. */
extern int ast_active;

#endif /* CONFIG_AST */

#endif /* MCCAST_H */
