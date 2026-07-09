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

#ifdef __cplusplus
}
#endif

#if defined(CONFIG_AST) && CONFIG_AST

extern int ast_active;

#endif

#endif
