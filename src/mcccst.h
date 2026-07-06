/*
 * mcccst.h — Concrete Syntax Tree (CST) database public seam.
 *
 * See docs/NOTES.md "CST database — design record" (design + as-shipped seam).
 *
 * The CST is a side-recorded, self-contained reflection of the *written* source.
 * It shares no memory with the compiler (invariant NOTES CST §0.3): its own arena, its
 * own copy of each file's bytes, and all cross-references expressed as CST node
 * ids — never Sym*, never pointers into compiler buffers.
 *
 * This header is the ONLY surface the compiler (mccpp.c / mccgen.c) references,
 * and then only through the CST_* hook macros at the bottom. All structure,
 * hashing, geometry and IO live behind the functions declared here, inside
 * mcccst*.c. When CONFIG_MCC_CST is unset every hook compiles to nothing, so the
 * feature is provably zero-cost-off (NOTES CST §0.2).
 */
#ifndef MCCCST_H
#define MCCCST_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ *
 * Node kinds (NOTES CST §2). Flat enum, grouped. Reserve kinds now even if
 * unused until a later milestone so adding them never reshapes the node
 * format.
 * ------------------------------------------------------------------ */
typedef enum CstKind {
    /* Structural */
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

    /* Preprocessor (concrete) */
    CST_MacroInvocation,
    CST_IncludeDirective,
    CST_PPDirective,
    CST_PPConditional,

    /* Leaves */
    CST_Token,

    /* Reserved for later milestones (LSP resilience / 9B trivia-as-nodes) */
    CST_Comment,
    CST_Error,
    CST_Missing,

    CST_KIND_COUNT
} CstKind;

/* ------------------------------------------------------------------ *
 * Ids (NOTES CST §1 "Node identity"). SoA arrays are indexed by a bare u32
 * *local* id; anything that crosses a file boundary (sym refs, include
 * targets, future 4C dedup entries) carries the full 64-bit (file,local).
 * High bits reserved now so multi-file + hash-consing never force a
 * migration.
 * ------------------------------------------------------------------ */
typedef uint32_t CstLocal; /* per-file node index into the SoA columns   */
typedef uint64_t CstId;    /* (file << 32) | local, for cross-file refs   */

#define CST_NONE ((CstLocal)0xffffffffu)

static inline CstId    cst_id(uint32_t file, CstLocal local) {
    return ((CstId)file << 32) | (CstId)local;
}
static inline uint32_t cst_id_file(CstId id) { return (uint32_t)(id >> 32); }
static inline CstLocal cst_id_local(CstId id) { return (CstLocal)id; }

/* 128-bit non-crypto hash value (NOTES CST §3). Two 64-bit lanes. */
typedef struct CstHash {
    uint64_t lo, hi;
} CstHash;

/* Trivia piece attached to a leaf token (NOTES CST §1 "Trivia"): a classified
 * (kind, relative span) excluded from the structural hash. */
typedef enum CstTriviaKind {
    CST_TRIV_WS = 0,        /* horizontal/vertical whitespace   */
    CST_TRIV_LINE_COMMENT,  /* line comment                     */
    CST_TRIV_BLOCK_COMMENT, /* block comment                    */
    CST_TRIV_NEWLINE
} CstTriviaKind;

typedef struct CstTrivia {
    uint32_t kind;   /* CstTriviaKind                                   */
    uint32_t offset; /* relative to the owning leaf's start             */
    uint32_t length;
} CstTrivia;

/* Opaque arena / node store (one per file; NOTES CST §1 Includes stitches them). */
typedef struct CstArena CstArena;

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================== *
 * Slice B — node store core
 * ==================================================================== */
CstArena *cst_arena_new(uint32_t file_id);
void      cst_arena_free(CstArena *a);
void      cst_arena_reset(CstArena *a);

/* Build-stack API: each grammar function brackets its work. cst_node_open
 * pushes a node as a child of the current top and makes it the new top;
 * cst_node_close pops it and finalizes its relative width. */
CstLocal  cst_node_open(CstArena *a, uint16_t kind);
void      cst_node_close(CstArena *a, CstLocal node);

/* Push a leaf token as a child of the current top. off/len index into the
 * owning file's owned source buffer (slice G); trivia may be NULL. */
CstLocal  cst_leaf(CstArena *a, uint16_t tok_kind, uint32_t off, uint32_t len,
                   const CstTrivia *trivia, uint32_t ntrivia);

/* Column accessors (read-only). */
uint16_t  cst_kind(const CstArena *a, CstLocal n);
CstLocal  cst_parent(const CstArena *a, CstLocal n);
CstLocal  cst_first_child(const CstArena *a, CstLocal n);
CstLocal  cst_next_sib(const CstArena *a, CstLocal n);
uint32_t  cst_width(const CstArena *a, CstLocal n); /* relative bytes      */
CstLocal  cst_node_count(const CstArena *a);
CstLocal  cst_root(const CstArena *a);

/* ==================================================================== *
 * Slice C — hashing (NOTES CST §3). Pure; usable without a tree.
 * ==================================================================== */
CstHash   cst_hash_leaf(uint16_t tok_kind, const uint8_t *bytes, uint32_t len);
CstHash   cst_hash_internal(uint16_t kind, const CstHash *child, uint32_t n);
int       cst_hash_eq(CstHash x, CstHash y);
CstHash   cst_struct_hash(const CstArena *a, CstLocal n);
CstHash   cst_trivia_hash(const CstArena *a, CstLocal n);
/* Recompute H_s over only the touched frontier + its root paths (§3.1). */
void      cst_rehash_frontier(CstArena *a, const CstLocal *touched, uint32_t n);
/* Recompute all hashes bottom-up (batch, v1). */
void      cst_rehash_all(CstArena *a);

/* ==================================================================== *
 * Slice D — geometry & offset->node index (NOTES CST §1, §2, §5)
 * ==================================================================== */
uint32_t  cst_abs_offset(const CstArena *a, CstLocal n);
void      cst_index_build(CstArena *a);
CstLocal  cst_node_at(const CstArena *a, uint32_t abs_off);

/* ==================================================================== *
 * Slice E — serialization (NOTES CST §1 Persistence, §8.1, §8.6)
 * ==================================================================== */
int       cst_snapshot_save(const CstArena *a, const char *path);
CstArena *cst_snapshot_load(const char *path); /* NULL on version/endian skew */
/* Emit the source this (sub)tree reflects; returns bytes written (or the
 * required size if out==NULL). The round-trip oracle (NOTES CST §8.1). */
size_t    cst_reflect(const CstArena *a, CstLocal root, uint8_t *out, size_t cap);
/* Validate round-trip + tiling + offset lookup (NOTES CST §8.1/§8.2/§8.3). 0 = ok. */
int       cst_validate(const CstArena *a, char *msg, size_t msgcap);

/* ==================================================================== *
 * Slice G — owned source & trivia (NOTES CST §4)
 * ==================================================================== */
uint32_t  cst_own_file(CstArena *a, const char *name,
                       const uint8_t *bytes, size_t n);
const uint8_t *cst_source(const CstArena *a, uint32_t *len_out);

/* ==================================================================== *
 * Slice I — symbol refs (NOTES CST §1 Symbols)
 * ==================================================================== */
void      cst_set_sym_ref(CstArena *a, CstLocal use, CstId def);
CstId     cst_sym_ref(const CstArena *a, CstLocal use);

/* ==================================================================== *
 * D3/D5 — SourceFile template + content-addressed store + renderer
 * (NOTES CST gap-closure D3/D5). A file's *bytes* are fixed, so its *structure* is
 * fixed: each file is a static full-concrete template (all #if/#else branches
 * captured), hash-consed by pure H_s(body) so identical headers — regardless
 * of include context — share one physical subtree. Per-include variation lives
 * in a small binding; the live token stream is render(template, binding).
 * ==================================================================== */

/* A PPConditional's branch-body children are tagged with a 1-based ordinal so
 * a binding can pick the live one (0 = not a branch body, e.g. a directive
 * line). Uses the reserved slot_key column (NOTES CST §3.1). */
void      cst_mark_branch(CstArena *a, CstLocal node, uint32_t branch_ord);
uint32_t  cst_branch_ord(const CstArena *a, CstLocal node);

/* An IncludeDirective's cross-file target: the template id of the included
 * SourceFile (stored in the sym_ref column as (template_id, 0)). */
void      cst_set_include_target(CstArena *a, CstLocal node, uint32_t template_id);
uint32_t  cst_include_target(const CstArena *a, CstLocal node);

/* Content-addressed template store (NOTES CST §10 4C hash-consing / 6B store). */
typedef struct CstStore CstStore;
CstStore *cst_store_new(void);
void      cst_store_free(CstStore *s); /* also frees every interned template */
/* Intern `tmpl` (whose tree must be hashed) keyed by H_s(root). If an entry
 * with that key already exists it is returned (dedup) and `tmpl` is freed; a
 * debug build first verifies the two reflect byte-identically (hash-collision
 * tripwire — see docs/TODO.md). Otherwise `tmpl` is adopted and a fresh id
 * returned. Returns the canonical template id. */
uint32_t  cst_store_intern(CstStore *s, CstArena *tmpl);
uint32_t  cst_store_count(const CstStore *s);
CstArena *cst_store_get(const CstStore *s, uint32_t template_id);

/* Per-include binding: the "holes" in a template for one include instance —
 * which branch of each captured PPConditional is live, plus (in encounter
 * order) a child binding for each IncludeDirective. Recursive: the include
 * tree's threaded PP environment is modelled by the binding tree. */
typedef struct CstBinding CstBinding;
CstBinding *cst_binding_new(uint32_t template_id);
void        cst_binding_free(CstBinding *b); /* frees child bindings too */
uint32_t    cst_binding_template(const CstBinding *b);
void        cst_binding_select(CstBinding *b, CstLocal cond, uint32_t which);
uint32_t    cst_binding_selected(const CstBinding *b, CstLocal cond); /* def 0 */
/* Append a child binding for the next IncludeDirective (encounter order). */
void        cst_binding_add_include(CstBinding *b, CstBinding *child);

/* Render the live token stream of a binding's template (branch selection +
 * include expansion), threading the environment through nested includes.
 * Returns bytes written, or the required size when out==NULL. */
size_t      cst_render(const CstStore *s, const CstBinding *b, uint8_t *out,
                       size_t cap);
/* Round-trip oracle: render with the identity binding — every branch, no
 * include expansion — i.e. the written source of one template. Shares the
 * leaf-emission path with cst_render (NOTES CST §8.1). */
size_t      cst_render_identity(const CstArena *tmpl, uint8_t *out, size_t cap);

#ifdef __cplusplus
}
#endif

/* ==================================================================== *
 * Slice H — recording hooks. The ONLY surface the compiler references.
 * When CONFIG_MCC_CST is unset these expand to nothing (NOTES CST §0.2).
 * The hook_* functions are thin wrappers over a per-parse current arena +
 * build stack, defined in mcccst.c; the compiler never sees the arena.
 * ==================================================================== */
#if defined(CONFIG_MCC_CST) && CONFIG_MCC_CST

void      cst_hook_begin(const char *filename);
CstArena *cst_hook_end(void);
void      cst_hook_token(uint32_t start, uint32_t end);
uint32_t  cst_cur_tok_off(void);
void      cst_hook_def(int tok_value, uint32_t off);
void      cst_hook_use(int tok_value, uint32_t off);
void      cst_hook_open(uint16_t kind);
uint32_t  cst_mark(void);
uint32_t  cst_leafcount(void);
void      cst_hook_open_at(uint16_t kind, uint32_t first_leaf);
void      cst_hook_wrap(uint16_t kind, uint32_t first_leaf, uint32_t last_leaf);
void      cst_hook_close(void);
/* D3 live capture: intern a real included file as a hash-consed SourceFile
 * template in the current capture's session store (dedup by pure H_s). When
 * from_main_file, the template is bound to the next main-file IncludeDirective
 * node (in source order) at cst_hook_end. */
void      cst_hook_include(const char *path, int from_main_file);
CstStore *cst_hook_store(void); /* the session store, valid until cst_hook_end */
void      cst_hook_leaf(uint16_t tok_kind, uint32_t byte_off, uint32_t len);

#define CST_OPEN(k)          cst_hook_open((uint16_t)(k))
#define CST_OPEN_AT(k, m)    cst_hook_open_at((uint16_t)(k), (m))
#define CST_MARK()           cst_mark()
#define CST_CLOSE()          cst_hook_close()
#define CST_LEAF(tk, off, n) cst_hook_leaf((uint16_t)(tk), (uint32_t)(off), (uint32_t)(n))

#else

#define CST_OPEN(k)          ((void)0)
#define CST_OPEN_AT(k, m)    ((void)0)
#define CST_MARK()           0u
#define CST_CLOSE()          ((void)0)
#define CST_LEAF(tk, off, n) ((void)0)

#endif /* CONFIG_MCC_CST */

#endif /* MCCCST_H */
