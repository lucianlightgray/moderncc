#ifndef MCCCST_H
#define MCCCST_H

#include <stddef.h>
#include <stdint.h>

typedef enum CstKind {
	CST_TranslationUnit = 0,
	CST_Declaration,
	CST_FunctionDef,
	CST_Declarator,
	CST_ParamList,
	CST_StructOrUnion,
	CST_Enum,
	CST_TypeName,
	CST_Initializer,
	CST_CompoundStmt,
	CST_If,
	CST_While,
	CST_For,
	CST_Do,
	CST_Switch,
	CST_Return,
	CST_Goto,
	CST_Label,
	CST_ExprStmt,
	CST_Binary,
	CST_Unary,
	CST_Call,
	CST_Member,
	CST_Index,
	CST_Cast,
	CST_Cond,
	CST_Comma,
	CST_Paren,
	CST_Primary,

	CST_MacroInvocation,
	CST_IncludeDirective,
	CST_PPDirective,
	CST_PPConditional,

	CST_Token,

	CST_Comment,
	CST_Error,
	CST_Missing,

	CST_KIND_COUNT
} CstKind;

typedef uint32_t CstLocal;
typedef uint64_t CstId;

#define CST_NONE ((CstLocal)0xffffffffu)

static inline CstId cst_id(uint32_t file, CstLocal local) {
	return ((CstId)file << 32) | (CstId)local;
}

static inline uint32_t cst_id_file(CstId id) {
	return (uint32_t)(id >> 32);
}
static inline CstLocal cst_id_local(CstId id) {
	return (CstLocal)id;
}

typedef struct CstHash {
	uint64_t lo, hi;
} CstHash;

typedef enum CstTriviaKind {
	CST_TRIV_WS = 0,
	CST_TRIV_LINE_COMMENT,
	CST_TRIV_BLOCK_COMMENT,
	CST_TRIV_NEWLINE
} CstTriviaKind;

typedef struct CstTrivia {
	uint32_t kind;
	uint32_t offset;
	uint32_t length;
} CstTrivia;

typedef struct CstArena CstArena;

#ifdef __cplusplus
extern "C" {

#endif

CstArena *cst_arena_new(uint32_t file_id);
void cst_arena_free(CstArena *a);
void cst_arena_reset(CstArena *a);

CstLocal cst_node_open(CstArena *a, uint16_t kind);
void cst_node_close(CstArena *a, CstLocal node);

CstLocal cst_leaf(CstArena *a, uint16_t tok_kind, uint32_t off, uint32_t len,
									const CstTrivia *trivia, uint32_t ntrivia);

uint16_t cst_kind(const CstArena *a, CstLocal n);
CstLocal cst_parent(const CstArena *a, CstLocal n);
CstLocal cst_first_child(const CstArena *a, CstLocal n);
CstLocal cst_next_sib(const CstArena *a, CstLocal n);
uint32_t cst_width(const CstArena *a, CstLocal n);
CstLocal cst_node_count(const CstArena *a);
CstLocal cst_root(const CstArena *a);

CstHash cst_hash_leaf(uint16_t tok_kind, const uint8_t *bytes, uint32_t len);
CstHash cst_hash_internal(uint16_t kind, const CstHash *child, uint32_t n);
int cst_hash_eq(CstHash x, CstHash y);
CstHash cst_struct_hash(const CstArena *a, CstLocal n);
CstHash cst_trivia_hash(const CstArena *a, CstLocal n);
void cst_rehash_frontier(CstArena *a, const CstLocal *touched, uint32_t n);
void cst_rehash_all(CstArena *a);

uint32_t cst_abs_offset(const CstArena *a, CstLocal n);
void cst_index_build(CstArena *a);
CstLocal cst_node_at(const CstArena *a, uint32_t abs_off);

int cst_snapshot_save(const CstArena *a, const char *path);
CstArena *cst_snapshot_load(const char *path);
size_t cst_reflect(const CstArena *a, CstLocal root, uint8_t *out, size_t cap);
int cst_validate(const CstArena *a, char *msg, size_t msgcap);

uint32_t cst_own_file(CstArena *a, const char *name,
											const uint8_t *bytes, size_t n);
const uint8_t *cst_source(const CstArena *a, uint32_t *len_out);

void cst_set_sym_ref(CstArena *a, CstLocal use, CstId def);
CstId cst_sym_ref(const CstArena *a, CstLocal use);

void cst_mark_branch(CstArena *a, CstLocal node, uint32_t branch_ord);
uint32_t cst_branch_ord(const CstArena *a, CstLocal node);

void cst_set_include_target(CstArena *a, CstLocal node, uint32_t template_id);
uint32_t cst_include_target(const CstArena *a, CstLocal node);

typedef struct CstStore CstStore;
CstStore *cst_store_new(void);
void cst_store_free(CstStore *s);
uint32_t cst_store_intern(CstStore *s, CstArena *tmpl);
uint32_t cst_store_count(const CstStore *s);
CstArena *cst_store_get(const CstStore *s, uint32_t template_id);

typedef struct CstBinding CstBinding;
CstBinding *cst_binding_new(uint32_t template_id);
void cst_binding_free(CstBinding *b);
uint32_t cst_binding_template(const CstBinding *b);
void cst_binding_select(CstBinding *b, CstLocal cond, uint32_t which);
uint32_t cst_binding_selected(const CstBinding *b, CstLocal cond);
void cst_binding_add_include(CstBinding *b, CstBinding *child);

size_t cst_render(const CstStore *s, const CstBinding *b, uint8_t *out,
									size_t cap);
size_t cst_render_identity(const CstArena *tmpl, uint8_t *out, size_t cap);

#ifdef __cplusplus
}
#endif

#if defined(CONFIG_MCC_CST) && CONFIG_MCC_CST

void cst_hook_begin(const char *filename);
CstArena *cst_hook_end(void);
void cst_hook_token(uint32_t start, uint32_t end);
uint32_t cst_cur_tok_off(void);
void cst_hook_def(int tok_value, uint32_t off);
void cst_hook_use(int tok_value, uint32_t off);
void cst_hook_open(uint16_t kind);
uint32_t cst_mark(void);
uint32_t cst_leafcount(void);
void cst_hook_open_at(uint16_t kind, uint32_t first_leaf);
void cst_hook_wrap(uint16_t kind, uint32_t first_leaf, uint32_t last_leaf);
void cst_hook_close(void);
void cst_hook_include(const char *path, int from_main_file);
CstStore *cst_hook_store(void);
void cst_hook_leaf(uint16_t tok_kind, uint32_t byte_off, uint32_t len);

#define CST_OPEN(k) cst_hook_open((uint16_t)(k))
#define CST_OPEN_AT(k, m) cst_hook_open_at((uint16_t)(k), (m))
#define CST_MARK() cst_mark()
#define CST_CLOSE() cst_hook_close()
#define CST_LEAF(tk, off, n) cst_hook_leaf((uint16_t)(tk), (uint32_t)(off), (uint32_t)(n))

#else

#define CST_OPEN(k) ((void)0)
#define CST_OPEN_AT(k, m) ((void)0)
#define CST_MARK() 0u
#define CST_CLOSE() ((void)0)
#define CST_LEAF(tk, off, n) ((void)0)

#endif

#endif
