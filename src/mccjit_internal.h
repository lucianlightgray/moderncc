#ifndef MCC_JIT_INTERNAL_H
#define MCC_JIT_INTERNAL_H
#ifdef MCC_EMBED_JIT

#if defined(__GNUC__) || defined(__clang__)
#define MCCJIT_LOCAL __attribute__((visibility("hidden")))
#else
#define MCCJIT_LOCAL
#endif

#define MCCJIT_KGC_MAXARG 6

#define MCCJIT_INTENT_MAGIC 0x314a434dul
#define MCCJIT_INTENT_FORMAT                                                   \
	14u

#define MCCJIT_BIND_MAX 64u

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
	int bind_allow;
	int bind_n;
	int bind_elfsym[MCCJIT_BIND_MAX];
	int bind_tokv[MCCJIT_BIND_MAX];
	size_t bind_off[MCCJIT_BIND_MAX];
} MccjitBuf;

typedef struct MccjitTypeRec {
	uint8_t role;
	uint8_t building;
	uint8_t done;
	uint32_t a;
	uint32_t abf;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t nparam;
	uint32_t *pt;
	uint32_t *ptbf;
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
	uint64_t warm_gates;
	uint8_t unit_kind;
	uint32_t nbind;
	char **bind_name;
	uint64_t *bind_addr;
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

#endif
#endif
