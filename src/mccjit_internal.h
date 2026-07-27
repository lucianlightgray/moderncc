#ifndef MCC_JIT_INTERNAL_H
#define MCC_JIT_INTERNAL_H
#ifdef MCC_EMBED_JIT

/*
 * Shared internal surface for the runtime-JIT engine, split across
 * mccjit_intent.c (serialize/deserialize) and mccjit_embed.c (recompile,
 * stubs, pool, boot, selftests). Types and helpers used across those TUs
 * live here so the concern files compile independently in the multisource
 * build (and via #include in the amalgamation). Requires mcc.h + mccast.h
 * (Sym, AstArena, VT_*) to be included first.
 */

#if defined(__GNUC__) || defined(__clang__)
#define MCCJIT_LOCAL __attribute__((visibility("hidden")))
#else
#define MCCJIT_LOCAL
#endif

#define MCCJIT_KGC_MAXARG 6

#define MCCJIT_INTENT_MAGIC 0x314a434dul
#define MCCJIT_INTENT_FORMAT                                                   \
	9u /* 9: header carries unit_kind (whole-function vs promoted sub-slice) —   \
			 the marker that used to be implied by the now-removed                  \
			 AST_TranslationUnit kind. Header-only metadata: it is NEVER folded    \
			 into the slice-identity hash, so a sub-slice identical to a whole     \
			 function still hashes equal. 8: header carried the warm-start gate    \
			 mask. */

/* Whole-unit vs sub-slice, carried in the intent header (not an AST kind, not
   hashed). WHOLE = a complete function (has the fn_name/nparam signature
   trailer); KERNEL = a promoted AST sub-slice with no owning function. */
#define MCCJIT_UNIT_WHOLE 0u
#define MCCJIT_UNIT_KERNEL 1u

#define MCCJIT_ROLE_PLAIN 0u
#define MCCJIT_ROLE_NAMED 1u
#define MCCJIT_ROLE_PTR 2u
#define MCCJIT_ROLE_FUNC 3u
#define MCCJIT_ROLE_STRUCT 4u
#define MCCJIT_ROLE_DATA 5u

#define MCCJIT_DATA_MAX 65536u

typedef struct MccjitBuf {
	unsigned char *data;
	size_t len;
	size_t cap;
	int oom;
} MccjitBuf;

typedef struct MccjitTypeRec {
	uint8_t role;
	uint8_t building;
	uint8_t done;
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t nparam;
	uint32_t *pt;
	uint32_t *pr;
	uint32_t *foff;
	char **fnm;
	uint32_t datalen;
	unsigned char *data;
	struct Sym *built;
} MccjitTypeRec;

typedef struct MccjitIntent {
	AstArena *arena;
	uint64_t salt;
	int64_t anchor_sym_v;
	uint32_t handle_count;
	uint64_t *handle_raw;
	int64_t *handle_token_v;
	char **handle_name;
	MccjitTypeRec *recs;
	int has_external;
	char *fn_name;
	uint32_t ret_type_t;
	uint32_t ret_type_ref;
	uint32_t func_type;
	uint32_t nparam;
	uint32_t *param_type_t;
	int64_t *param_off;
	char **param_name;
	uint64_t warm_gates; /* AOT-selected gate mask to warm-start the JIT search (0 = none) */
	uint8_t unit_kind; /* MCCJIT_UNIT_WHOLE | MCCJIT_UNIT_KERNEL — header metadata only, never hashed */
} MccjitIntent;

MCCJIT_LOCAL unsigned mccjit_role_for_base(int t);
MCCJIT_LOCAL uint64_t mccjit_salt_witness(void);

MCCJIT_LOCAL void mccjit_buf_init(MccjitBuf *b);
MCCJIT_LOCAL void mccjit_buf_free(MccjitBuf *b);
MCCJIT_LOCAL int mccjit_intent_serialize(const AstArena *a, Sym *sym, MccjitBuf *buf,
																				 uint64_t warm_gates);
MCCJIT_LOCAL int mccjit_intent_deserialize(const void *buf, size_t len, MccjitIntent *it);
MCCJIT_LOCAL uint64_t mccjit_intent_peek_warm_gates(const void *buf, size_t len);
MCCJIT_LOCAL void mccjit_intent_release(MccjitIntent *it);
MCCJIT_LOCAL Sym *mccjit_rebuild_sym(const MccjitIntent *it);
MCCJIT_LOCAL void mccjit_note_export_name(const char *name);

#endif /* MCC_EMBED_JIT */
#endif /* MCC_JIT_INTERNAL_H */
